// 
// alloc.h
//
// Implementation of IMemAllocator that uses a single contiguous
// buffer with lockable regions instead of a pool of fixed size buffers.
//
// Geraint Davies, January 2013

#pragma once

#include <utility>
#include "logger.h"

typedef std::pair<SSIZE_T, SSIZE_T> lock_t;
typedef std::list<lock_t> lock_list_t;

class ContigBuffer
{
public:
	ContigBuffer(SSIZE_T cSpace = 0)	: 
		m_pBuffer(NULL), 
		m_cSpace(0),
		m_cValid(0),
		m_bAbort(false),
		m_idxRead(0),
		m_evLocks(true)	// manual reset
	{
		#if defined(_DEBUG)
			m_cLocked = 0;
			m_cbMaxData = 0;
		#endif
		Allocate(cSpace);
	}
	~ContigBuffer()
	{
		delete[] m_pBuffer;
	}
	HRESULT Allocate(SSIZE_T cSpace)
	{
		if ((m_cValid > 0) || !m_locks.empty())
		{
			LOG((TEXT("Allocate when not empty")));
			return VFW_E_WRONG_STATE;
		}
		delete[] m_pBuffer;
		m_pBuffer = NULL;
		m_idxRead = 0;
		m_cSpace = cSpace;
		if (m_cSpace > 0)
		{
			m_pBuffer = new BYTE[m_cSpace];
		}
		LOG((TEXT("Allocate %d bytes"), cSpace));
		return S_OK;
	}

	HRESULT Consume(const BYTE* p, SSIZE_T c)
	{
		CAutoLock lock(&m_csLocks);
		if ((c >= m_cValid) && m_locks.empty())
		{
			LOG((TEXT("Resetting idxRead in Consume")));
			m_idxRead = 0;
			m_cValid = 0;
		} else 
		if ((p == (m_pBuffer + m_idxRead)) && (c <= m_cValid))
		{
			m_idxRead += c;
			m_cValid -= c;
		} else
		{
			LOG((TEXT("Consume not contiguous")));
			return E_INVALIDARG;
		}
		return S_OK;
	}

	HRESULT Lock(const BYTE* p, SSIZE_T c)
	{
		if((p < m_pBuffer) || (p > (m_pBuffer + m_cSpace)) || (c > m_cSpace) || ((p+c) > (m_pBuffer + m_cSpace)))
		{
			LOG((TEXT("Invalid lock region")));
			return E_INVALIDARG;
		}
		SSIZE_T index = p - m_pBuffer;
		SSIZE_T indexEnd = index + c;
		#if defined(_DEBUG) //&& FALSE
			const SSIZE_T length = indexEnd - index;
			m_cLocked += length;
			LOG((TEXT("Locking region %d - %d (data = %d, total = %ld of %ld)"), index, indexEnd, length, m_cLocked, m_cSpace));
			if(m_cLocked > m_cbMaxData)
				m_cbMaxData = m_cLocked;
			if(m_cLocked == m_cSpace)
				LOG((TEXT("100% of buffer space in use")));
		#endif
		CAutoLock lock(&m_csLocks);
		m_locks.push_back(lock_t(index, indexEnd));
		return S_OK;
	}
	HRESULT Unlock(const BYTE* p, SSIZE_T c)
	{
		if ((p < m_pBuffer) || (p > (m_pBuffer + m_cSpace)) || (c > m_cSpace) || ((p+c) > (m_pBuffer + m_cSpace)))
		{
			LOG((TEXT("Invalid unlock region")));
			return E_INVALIDARG;
		}
		SSIZE_T index = p - m_pBuffer;
		SSIZE_T indexEnd = index + c;
		CAutoLock lock(&m_csLocks);
		for (lock_list_t::iterator it = m_locks.begin(); it != m_locks.end(); it++)
		{
			if ((it->first == index) && (it->second == indexEnd))
			{
				#if defined(_DEBUG) //&& FALSE
					const SSIZE_T length = indexEnd - index;
					m_cLocked -= length;
					LOG((TEXT("Unlocking region %d - %d (data = %d, total = %ld of %ld)"), index, indexEnd, length, m_cLocked, m_cSpace));
				#endif
				m_locks.erase(it);
				m_evLocks.Set();
				#if 0
					if (!m_cValid && m_locks.empty())
					{
						LOG((TEXT("resetting idxRead on empty in Unlock")));
						m_idxRead = 0;
					}
				#endif
				return S_OK;
			}
		}
		LOG((TEXT("Lock not found in Unlock, len %d"), c));
		return E_INVALIDARG;
	}

	BYTE* Append(const BYTE* p, SSIZE_T cActual, SSIZE_T cMaxBytes)
	{
		for (;;)
		{
			{
				CAutoLock lock(&m_csLocks);
				if (m_bAbort)
				{
					return NULL;
				}
				m_evLocks.Reset();

				if (cMaxBytes > m_cSpace)
				{
					LOG((TEXT("Whole buffer too small for packet %d"), cMaxBytes));
					return NULL;
				}
				SSIZE_T index = m_idxRead + m_cValid;
				if ((m_cSpace - index) < cMaxBytes)
				{
					LOG((TEXT("Allocator wrapping to start")));
					index = 0;
				}
				bool bLocked = true;
				if (!SearchLocks(index, index + cMaxBytes))
				{
					bLocked = false;
					if ((index == 0) && (m_idxRead > 0))
					{
						if (m_cValid && SearchLocks(m_idxRead, m_idxRead + m_cValid))
						{
							bLocked = true;
							LOG((TEXT("Waiting for locks on valid bytes %d"), m_cValid));
						}
						else
						{
							if (m_cValid)
							{
								MoveMemory(m_pBuffer, m_pBuffer + m_idxRead, m_cValid);
							}
							m_idxRead = 0;
						}
					}
				}
				if (!bLocked)
				{
					BYTE* pDest = m_pBuffer + m_idxRead + m_cValid;
					CopyMemory(pDest, p, cActual);
					m_cValid += cMaxBytes;
					return pDest;
				}
			}
			m_evLocks.Wait(2 * 1000);

			if (!m_evLocks.Check())
			{
				LOG((TEXT("Timeout in Append")));
			}
		}
	}
	BYTE* AppendAndLock(const BYTE* p, SSIZE_T cActual, SSIZE_T cMaxBytes)
	{
		BYTE* pDest = Append(p, cActual, cMaxBytes);
		if (!pDest)
		{
			return pDest;
		}
		HRESULT hr = Lock(pDest, cMaxBytes);
		if (FAILED(hr))
		{
			return NULL;
		}
		Consume(pDest, cMaxBytes);
		return pDest;
	}

	BYTE* ValidRegion()
	{
		return m_pBuffer + m_idxRead;
	}
	SSIZE_T ValidLength() const
	{
		return m_cValid;
	}

	void Abort()
	{
		LOG((TEXT("Max locked data: %d of %d"), m_cbMaxData, m_cSpace));
		CAutoLock lock(&m_csLocks);
		m_bAbort = true;
		m_evLocks.Set();
	}
	void ResetAbort()
	{
		CAutoLock lock(&m_csLocks);
		m_bAbort = false;
	}

private:
	bool SearchLocks(SSIZE_T index, SSIZE_T indexEnd)
	{
		for (lock_list_t::iterator it = m_locks.begin(); it != m_locks.end(); it++)
		{
			if ((it->first < indexEnd) && (it->second > index))
			{
				return true;
			}
		}
		return false;
	}

private:
	CCritSec m_csLocks;
	CAMEvent m_evLocks;
	lock_list_t m_locks;
	bool m_bAbort;

	SSIZE_T m_idxRead;
	SSIZE_T m_cValid;
	SSIZE_T m_cSpace;
	BYTE* m_pBuffer;

#if defined(_DEBUG)
	SSIZE_T m_cLocked;
	SSIZE_T m_cbMaxData;
#endif
};

// creates IMediaSample objects that
// are subsamples of another buffer
class Suballocator : 
	public CBaseAllocator
{
public:
// Suballocator
	Suballocator(ContigBuffer* pSource, HRESULT* phr) : 
		m_pSource(pSource),
		CBaseAllocator(NAME("Suballocator"), NULL, phr)
	{
		ASSERT(m_pSource);
	}
	HRESULT AppendAndWrap(BYTE* pnData, SSIZE_T nDataSize, SSIZE_T nDataCapacity, IMediaSample** ppSample)
	{
		BYTE* pDest = m_pSource->AppendAndLock(pnData, nDataSize, nDataCapacity);
		if(!pDest)
			return E_FAIL;
		HRESULT hr = S_OK;
		CMediaSample* pS = new CMediaSample(NAME("CMediaSample"), this, &hr);
		pS->SetPointer(pDest, (LONG) nDataCapacity);
		pS->SetActualDataLength((LONG) nDataSize);
		IMediaSamplePtr pSample = pS;
		*ppSample = pSample.Detach();
		return S_OK;
	}
	HRESULT AppendAndWrap(BYTE* pnData, SSIZE_T nDataSize, IMediaSample** ppSample)
	{
		return AppendAndWrap(pnData, nDataSize, nDataSize, ppSample);
	}

// IMemAllocator
	STDMETHOD(SetProperties)(ALLOCATOR_PROPERTIES* pRequest, ALLOCATOR_PROPERTIES* pActual)
	{
		UNREFERENCED_PARAMETER(pRequest);
		UNREFERENCED_PARAMETER(pActual);
		return S_OK;
	}
    STDMETHOD(GetProperties)(ALLOCATOR_PROPERTIES* pProperties)
	{
		// QUES: Do we ever reach here? Or do we need to care for hardcoded buffer size otherwise?
		ASSERT(pProperties);
		ZeroMemory(pProperties, sizeof *pProperties);
		pProperties->cBuffers = 100;
		pProperties->cbBuffer = 64 * 1024; // TODO: ???
		return S_OK;
	}
    STDMETHOD(Commit)()
	{
		return S_OK;
	}
    STDMETHOD(Decommit)()
	{
		return S_OK;
	}
    STDMETHOD(GetBuffer)(IMediaSample **ppBuffer, REFERENCE_TIME * pStartTime, REFERENCE_TIME * pEndTime, DWORD dwFlags)
	{
		UNREFERENCED_PARAMETER(ppBuffer);
		UNREFERENCED_PARAMETER(pStartTime);
		UNREFERENCED_PARAMETER(pEndTime);
		UNREFERENCED_PARAMETER(dwFlags);
		return E_NOTIMPL;
	}
    STDMETHOD(ReleaseBuffer)(IMediaSample *pBuffer)
	{
		BYTE* pBuf;
		pBuffer->GetPointer(&pBuf);
		m_pSource->Unlock(pBuf, pBuffer->GetSize());
		CMediaSample* pS = static_cast<CMediaSample*>(pBuffer);
		delete pS;
		return S_OK;
	}

protected:
    void Free() override
	{
	}

private:
	ContigBuffer* m_pSource;
};
