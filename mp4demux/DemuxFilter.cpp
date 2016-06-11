//
// DemuxFilter.cpp
// 
// Implementation of classes for DirectShow Mpeg-4 Demultiplexor filter
//
// Geraint Davies, April 2004
//
// Copyright (c) GDCL 2004-6. All Rights Reserved. 
// You are free to re-use this as the basis for your own filter development,
// provided you retain this copyright notice in the source.
// http://www.gdcl.co.uk
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "DemuxFilter.h"
#include "index.h"

#include <math.h>

#if defined(_DEBUG) && FALSE
	#define TRACE_SEEK
#endif // defined(_DEBUG)
#if defined(_DEBUG) || defined(TRACE_SEEK) //|| TRUE
	#include <stdio.h>
	#include <tchar.h>
#endif // defined(_DEBUG) || defined(TRACE_SEEK)

// --- registration tables ----------------

// filter registration -- these are the types that our
// pins accept and produce
const AMOVIESETUP_MEDIATYPE 
Mpeg4Demultiplexor::m_sudType[] = 
{
    {
        &MEDIATYPE_Stream,
        &MEDIASUBTYPE_NULL,
    },
    {
        &MEDIATYPE_Video,
        &MEDIASUBTYPE_NULL      // wild card
    },
    {
        &MEDIATYPE_Audio,
        &MEDIASUBTYPE_NULL
    },
};

// registration of our pins for auto connect and render operations
const AMOVIESETUP_PIN 
Mpeg4Demultiplexor::m_sudPin[] = 
{
    {
        L"Input",           // pin name
        FALSE,              // is rendered?    
        FALSE,              // is output?
        FALSE,              // zero instances allowed?
        FALSE,              // many instances allowed?
        &CLSID_NULL,        // connects to filter (for bridge pins)
        NULL,               // connects to pin (for bridge pins)
        1,                  // count of registered media types
        &m_sudType[0]       // list of registered media types    
    },
    {
        L"Video",          // pin name
        FALSE,              // is rendered?    
        TRUE,               // is output?
        FALSE,              // zero instances allowed?
        FALSE,              // many instances allowed?
        &CLSID_NULL,        // connects to filter (for bridge pins)
        NULL,               // connects to pin (for bridge pins)
        1,                  // count of registered media types
        &m_sudType[1]       // list of registered media types    
    },
    {
        L"Audio",          // pin name
        FALSE,              // is rendered?    
        TRUE,               // is output?
        FALSE,              // zero instances allowed?
        FALSE,              // many instances allowed?
        &CLSID_NULL,        // connects to filter (for bridge pins)
        NULL,               // connects to pin (for bridge pins)
        1,                  // count of registered media types
        &m_sudType[2]       // list of registered media types    
    },
};

// filter registration information. 
const AMOVIESETUP_FILTER 
Mpeg4Demultiplexor::m_sudFilter = 
{
    &__uuidof(Mpeg4Demultiplexor),  // filter clsid
    L"GDCL Mpeg-4 Demultiplexor",   // filter name
    MERIT_NORMAL,                   // ie default for auto graph building
    3,                              // count of registered pins
    m_sudPin                        // list of pins to register
};

// ---- construction/destruction and COM support -------------

// the class factory calls this to create the filter
//static 
CUnknown* WINAPI 
Mpeg4Demultiplexor::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr)
{
    return new Mpeg4Demultiplexor(pUnk, phr);
}


Mpeg4Demultiplexor::Mpeg4Demultiplexor(LPUNKNOWN pUnk, HRESULT* phr)
: m_pInput(NULL),
  m_pSeekingPin(NULL),
  m_tStart(0),
  m_tStop(0x7ffffffffffffff),       // less than MAX_TIME so we can add one second to it
  m_dRate(1.0),
  CBaseFilter(NAME("Mpeg4Demultiplexor"), pUnk, &m_csFilter, *m_sudFilter.clsID)
{
    m_pInput = new DemuxInputPin(this, &m_csFilter, phr);
}

Mpeg4Demultiplexor::~Mpeg4Demultiplexor()
{
    delete m_pInput;
}


int 
Mpeg4Demultiplexor::GetPinCount()
{
    return 1 + (int)m_Outputs.size();
}

CBasePin *
Mpeg4Demultiplexor::GetPin(int n)
{
    if (n == 0)
    {
        return m_pInput;
    } else if (n <= (int)m_Outputs.size())
    {
        return Output(n-1);
    } else {
        return NULL;
    }
}


bool 
Mpeg4Demultiplexor::SelectSeekingPin(DemuxOutputPin* pPin)
{
    CAutoLock lock(&m_csSeeking);
    if (m_pSeekingPin == NULL)
    {
        m_pSeekingPin = pPin;
    }
    return(m_pSeekingPin == pPin);
}

void 
Mpeg4Demultiplexor::DeselectSeekingPin(DemuxOutputPin* pPin)
{
    CAutoLock lock(&m_csSeeking);
    if(pPin == m_pSeekingPin)
        m_pSeekingPin = NULL;
}

void 
Mpeg4Demultiplexor::GetSeekingParams(REFERENCE_TIME* ptStart, REFERENCE_TIME* ptStop, double* pdRate)
{
    if (ptStart != NULL)
    {
        *ptStart = m_tStart;
    }
    if (ptStop != NULL)
    {
        *ptStop = m_tStop;
    }
    if (pdRate != NULL)
    {
        *pdRate = m_dRate;
    }
}
HRESULT 
Mpeg4Demultiplexor::SetRate(double dRate)
{
    CAutoLock lock(&m_csSeeking);
    m_dRate = dRate;
    return S_OK;
}

HRESULT 
Mpeg4Demultiplexor::SetStopTime(REFERENCE_TIME tStop)
{
    CAutoLock lock(&m_csSeeking);
    // this does not guarantee that a stop change only, while running,
    // will stop at the right point -- but most filters only
    // implement stop/rate changes when the current position changes
    m_tStop = tStop;
    return S_OK;
}

REFERENCE_TIME 
Mpeg4Demultiplexor::GetDuration()
{
    return m_pMovie->Duration();
}

HRESULT 
Mpeg4Demultiplexor::Seek(REFERENCE_TIME& tStart, BOOL bSeekToKeyFrame, REFERENCE_TIME tStop, double dRate)
{
	#pragma region Flush, Stop Thread
    if(IsActive())
    {
        for(SIZE_T nIndex = 0; nIndex < m_Outputs.size(); nIndex++)
        {
            DemuxOutputPin* pPin = Output((INT) nIndex);
            if(!pPin->IsConnected())
				continue;
			pPin->DeliverBeginFlush();
        }
        for(SIZE_T nIndex = 0; nIndex < m_Outputs.size(); nIndex++)
        {
            DemuxOutputPin* pPin = Output((INT) nIndex);
            if(!pPin->IsConnected())
				continue;
			#if TRUE
				pPin->BeginRestartThread();
			#else
				pPin->StopThread();
			#endif
        }
        for(SIZE_T nIndex = 0; nIndex < m_Outputs.size(); nIndex++)
        {
            DemuxOutputPin* pPin = Output((INT) nIndex);
            if(!pPin->IsConnected())
				continue;
            pPin->DeliverEndFlush();
        }
    }
	#pragma endregion
	#pragma region Start Time Adjustment
	if(bSeekToKeyFrame)
	{
		DemuxOutputPin* pSeekingPin = m_pSeekingPin;
		{
			CAutoLock lock(&m_csSeeking);
			if(!m_pSeekingPin)
			{
				if(m_Outputs.size())
				{
					DemuxOutputPin* pConnectedPin = NULL;
					// Take connected video pin
					for(vector<DemuxOutputPinPtr>::iterator it = m_Outputs.begin(); it < m_Outputs.end(); it++)
					{
						DemuxOutputPin* pPin = static_cast<DemuxOutputPin*>((IPin*) *it);
						if(!pPin->IsConnected())
							continue;
						if(!pConnectedPin)
							pConnectedPin = pPin;
						GUID MajorType;
						if(pPin->GetMajorMediaType(MajorType) && MajorType == MEDIATYPE_Video)
						{
							pSeekingPin = pPin;
							break;
						}
					}
					// Or, take just connected pin
					if(!pSeekingPin)
						pSeekingPin = pConnectedPin;
				}
			} else
				pSeekingPin = m_pSeekingPin;
		}
		if(pSeekingPin)
		{
			#if defined(_DEBUG)
				TCHAR pszText[1024] = { 0 };
				_stprintf_s(pszText, _T("%hs: tStart %I64d"), __FUNCTION__, tStart);
				GUID MajorType;
				if(pSeekingPin->GetMajorMediaType(MajorType))
					if(MajorType == MEDIATYPE_Video)
						_tcscat_s(pszText, _T(", using video pin"));
					else if(MajorType == MEDIATYPE_Audio)
						_tcscat_s(pszText, _T(", using audio pin"));
				pSeekingPin->SeekBackToKeyFrame(tStart);
				_stprintf_s(pszText + _tcslen(pszText), _countof(pszText) - _tcslen(pszText), _T(", updated m_tStart %I64d"), tStart);
				_tcscat_s(pszText, _T("\n"));
				OutputDebugString(pszText);
			#else
				pSeekingPin->SeekBackToKeyFrame(tStart);
			#endif
		}
	}
	#pragma endregion 
	#pragma region Update
    m_tStart = tStart;
    m_tStop = tStop;
    m_dRate = dRate;
	#pragma endregion
	#pragma region Start Thread
    if(IsActive())
    {
        for(SIZE_T nIndex = 0; nIndex < m_Outputs.size(); nIndex++)
        {
            DemuxOutputPin* pPin = Output((INT) nIndex);
            if(!pPin->IsConnected())
				continue;
			#if TRUE
				pPin->EndRestartThread();
			#else
	            pPin->StartThread();
			#endif
        }
    }
	#pragma endregion
    return S_OK;
}

HRESULT 
Mpeg4Demultiplexor::BeginFlush()
{
    for (UINT i =0; i < m_Outputs.size(); i++)
    {
        DemuxOutputPin* p = Output(i);
        p->DeliverBeginFlush();
    }
    return S_OK;
}

HRESULT 
Mpeg4Demultiplexor::EndFlush()
{
    for (UINT i =0; i < m_Outputs.size(); i++)
    {
        DemuxOutputPin* p = Output(i);
        p->DeliverEndFlush();
    }
    return S_OK;
}

HRESULT
Mpeg4Demultiplexor::BreakConnect()
{
    for (UINT i =0; i < m_Outputs.size(); i++)
    {
        IPinPtr pPeer;
        m_Outputs[i]->ConnectedTo(&pPeer);
        if (pPeer != NULL)
        {
            m_Outputs[i]->Disconnect();
            pPeer->Disconnect();
        }
    }
    m_Outputs.clear();
    return S_OK;
}

HRESULT 
Mpeg4Demultiplexor::CompleteConnect(IPin* pPeer)
{
    IAsyncReaderPtr pRdr = pPeer;
    if (pRdr == NULL)
    {
        return E_NOINTERFACE;
    }
    LONGLONG llTotal, llAvail;
    pRdr->Length(&llTotal, &llAvail);
    Atom* pfile = new Atom(m_pInput, 0, llTotal, 0, 0);
    m_pMovie = new Movie(pfile);
    // pfile now owned and deleted by Movie object

    if (m_pMovie->Tracks() <= 0)
    {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }


    // construct output pin for each valid track
    HRESULT hr = S_OK;
    for (long  nTrack = 0; nTrack < m_pMovie->Tracks(); nTrack++)
    {
        MovieTrack* pTrack = m_pMovie->Track(nTrack);
        _bstr_t strName = pTrack->Name();
        DemuxOutputPinPtr pPin = new DemuxOutputPin(pTrack, this, &m_csFilter, &hr, strName);
        m_Outputs.push_back(pPin);
    }
    return hr;
}


AsyncRequestor::AsyncRequestor()
: m_bBusy(false),
  m_ev(true)		// manual reset
{}

void AsyncRequestor::Active(IAsyncReader* pRdr)
{
	CAutoLock lock(&m_csRequests);
	if (!m_rdr)
	{
		m_rdr = pRdr;
		ZeroMemory(&m_props, sizeof(m_props));
		m_props.cBuffers = 16;
		m_props.cbBuffer = max_buffer_size;
		m_rdr->RequestAllocator(NULL, &m_props, &m_pAlloc);
		m_pAlloc->Commit();
	}
}

void AsyncRequestor::Inactive()
{
	CAutoLock lock(&m_csRequests);
	if (m_rdr)
	{
		m_rdr->BeginFlush();
		m_rdr->EndFlush();
	}
	m_rdr = NULL;
	m_ev.Set();
	if (m_pAlloc)
	{
		m_pAlloc->Decommit();
	}
}

HRESULT AsyncRequestor::Read(LONGLONG llOffset, long cBytes, BYTE* pBuffer)
{
	// first get a buffer and make a request
	HRESULT hr = S_OK;
	IMediaSamplePtr pSample;
	int startOffset = 0;

	{
		CAutoLock lock(&m_csRequests);
		if (!m_rdr || !m_pAlloc)
		{
			return VFW_E_WRONG_STATE;
		}
		hr = m_pAlloc->GetBuffer(&pSample, NULL, NULL, 0);
		if (hr != S_OK)
		{
			return hr;
		}
		
		// align start, pointer and length. Assume pointer is valid (their allocator).
		// align start down
		startOffset = int(llOffset % m_props.cbAlign);
		LONGLONG pos = llOffset - startOffset;

		// align length up
		LONGLONG len = (cBytes + startOffset);
		if (len % m_props.cbAlign)
		{
			len += m_props.cbAlign - (len % m_props.cbAlign);
		}

		if (len > pSample->GetSize())
		{
			return VFW_E_BUFFER_OVERFLOW;
		}

		// byte offset set as start/end time (1 byte = 1 second)
		REFERENCE_TIME tStart = pos * UNITS;
		REFERENCE_TIME tEnd = (pos + len) * UNITS;
		pSample->SetTime(&tStart, &tEnd);
		hr = m_rdr->Request(pSample, 0);
		if (FAILED(hr))
		{
			return hr;
		}
	}

	// now wait for completion. The next completion may be for a different thread, so
	// we need to queue it.
	bool bUsWaiting = false;
	for (;;)
	{
		if (!bUsWaiting)
		{
			CAutoLock lock(&m_csRequests);
			// is anyone waiting?
			if (!m_bBusy)
			{
				// no, we're first. Tell next caller that we are waiting
				m_ev.Reset();
				m_bBusy = true;
				bUsWaiting = true;
			}
		}
		if (bUsWaiting)
		{
			IMediaSamplePtr pDone;
			DWORD dw;
			HRESULT hr = m_rdr->WaitForNext(INFINITE, &pDone, (DWORD_PTR*)&dw);
			if (hr == S_OK)
			{
				CAutoLock lock(&m_csRequests);

				// signal either that we are leaving control
				// or that other users should look at the list
				m_ev.Set();

				if (pDone == pSample)
				{
					m_bBusy = false;
					break;
				}
				else
				{
					m_requests.push_back(pDone);
				}
			}
		}
		else
		{
			m_ev.Wait(250);
			CAutoLock lock(&m_csRequests);
			bool bFound = false;
			for (list<IMediaSamplePtr>::iterator it = m_requests.begin(); it != m_requests.end(); it++)
			{
				if (*it == pSample)
				{
					m_requests.erase(it);
					bFound = true;
					break;
				}
			}
		}
	}
	if (pSample->GetActualDataLength() < (startOffset + cBytes))
	{
		return E_FAIL;
	}

	BYTE* pSrc;
	pSample->GetPointer(&pSrc);
	pSrc += startOffset;
	CopyMemory(pBuffer, pSrc, cBytes);
	return S_OK;
}



// -------- input pin -----------------------------------------

DemuxInputPin::DemuxInputPin(Mpeg4Demultiplexor* pFilter, CCritSec* pLock, HRESULT* phr)
: m_pParser(pFilter), 
  CBasePin(NAME("DemuxInputPin"), pFilter, pLock, phr, L"Input", PINDIR_INPUT)
{
}

// base pin overrides
HRESULT 
DemuxInputPin::CheckMediaType(const CMediaType* pmt)
{
    // we accept any stream type and validate it ourselves during Connection
	UNREFERENCED_PARAMETER(pmt);
	return S_OK;
}

HRESULT 
DemuxInputPin::GetMediaType(int iPosition, CMediaType* pmt)
{
    if (iPosition != 0)
    {
        return VFW_S_NO_MORE_ITEMS;
    }
    pmt->InitMediaType();
    pmt->SetType(&MEDIATYPE_Stream);
    pmt->SetSubtype(&MEDIASUBTYPE_NULL);
    return S_OK;
}

STDMETHODIMP 
DemuxInputPin::BeginFlush()
{
    return m_pParser->BeginFlush();
}

STDMETHODIMP 
DemuxInputPin::EndFlush()
{
    return m_pParser->EndFlush();
}

HRESULT 
DemuxInputPin::CompleteConnect(IPin* pPeer)
{
    HRESULT hr = CBasePin::CompleteConnect(pPeer);

    if (SUCCEEDED(hr))
    {
        // validate input with parser
        hr = m_pParser->CompleteConnect(pPeer);
    }
    return hr;
}

HRESULT 
DemuxInputPin::BreakConnect()
{
    HRESULT hr = CBasePin::BreakConnect();
    if (SUCCEEDED(hr))
    {
        hr = m_pParser->BreakConnect();
    }
    return hr;
}
HRESULT 
DemuxInputPin::Active()
{
    IAsyncReaderPtr pRdr = GetConnected();
    if (pRdr != NULL)
    {
		m_requestor.Active(pRdr);
	}
	return __super::Active();
}

HRESULT 
DemuxInputPin::Inactive()
{
	m_requestor.Inactive();
	return __super::Inactive();
}

HRESULT 
DemuxInputPin::Read(LONGLONG llOffset, long cBytes, BYTE* pBuffer)
{
	HRESULT hr = E_FAIL;

#if 0
	while (cBytes > 32*1024)
	{
		int cThis = min(AsyncRequestor::max_buffer_size, cBytes);
		hr = m_requestor.Read(llOffset, cThis, pBuffer);
		if (hr != S_OK)
		{
			break;
		}
		llOffset += cThis;
		pBuffer += cThis;
		cBytes -= cThis;
	}
#endif
	if (hr == S_OK)
	{
		return S_OK;
	}
    IAsyncReaderPtr pRdr = GetConnected();
    if (pRdr != NULL)
    {
        hr = pRdr->SyncRead(llOffset, cBytes, pBuffer);
    }
    return hr;
}

LONGLONG 
DemuxInputPin::Length()
{
    LONGLONG llTotal = 0, llAvail;
    IAsyncReaderPtr pRdr = GetConnected();
    if (pRdr != NULL)
    {
        pRdr->Length(&llTotal, &llAvail);
    }
    return llTotal;
}


// -------- output pin ----------------------------------------

DemuxOutputPin::DemuxOutputPin(MovieTrack* pTrack, Mpeg4Demultiplexor* pDemux, CCritSec* pLock, HRESULT* phr, LPCWSTR pName)
: m_pParser(pDemux),
  m_pTrack(pTrack),
  m_tLate(0),
  CBaseOutputPin(NAME("DemuxOutputPin"), pDemux, pLock, phr, pName)
{
}
	
STDMETHODIMP 
DemuxOutputPin::NonDelegatingQueryInterface(REFIID iid, void** ppv)
{
    if (iid == IID_IMediaSeeking)
    {
        return GetInterface((IMediaSeeking*)this, ppv);
    } else
    if (iid == __uuidof(IDemuxOutputPin))
    {
        return GetInterface((IDemuxOutputPin*) this, ppv);
    } else
    {
        return CBaseOutputPin::NonDelegatingQueryInterface(iid, ppv);
    }
}

HRESULT 
DemuxOutputPin::CheckMediaType(const CMediaType *pmt)
{
    CMediaType mtTrack;
    int idx = 0;
    while(m_pTrack->GetType(&mtTrack, idx++))
    {
        if (*pmt == mtTrack)
        {
            // precise match to the type in the file
            return S_OK;
        }
    }
    // does not match any alternative
    return S_FALSE;
}

HRESULT 
DemuxOutputPin::GetMediaType(int iPosition, CMediaType *pmt)
{
    if (m_pTrack->GetType(pmt, iPosition))
    {
        return S_OK;
    } 
    return VFW_S_NO_MORE_ITEMS;
}
    
HRESULT 
DemuxOutputPin::SetMediaType(const CMediaType* pmt)
{
    HRESULT hr = CBaseOutputPin::SetMediaType(pmt);
    if (SUCCEEDED(hr))
    {
        if (!m_pTrack->SetType(pmt))
        {
            hr = VFW_E_TYPE_NOT_ACCEPTED;
        }
    }
    return hr;
}

HRESULT 
DemuxOutputPin::DecideBufferSize(IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES * pprop)
{
    long cMax = m_pTrack->SizeIndex()->MaxSize();
    if (m_pTrack->Handler() == NULL)
    {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }
    pprop->cbBuffer = m_pTrack->Handler()->BufferSize(cMax);

    pprop->cBuffers = 30;
    ALLOCATOR_PROPERTIES propActual;

    return pAlloc->SetProperties(pprop, &propActual);
}

// this group of methods deal with the COutputQueue
HRESULT 
DemuxOutputPin::Active()
{
    HRESULT hr = CBaseOutputPin::Active();
    if (SUCCEEDED(hr) && IsConnected())
    {
		#if defined(ALAXINFODIRECTSHOWSPY_AVAILABLE)
			if(!m_pMediaSampleTrace)
			{
				ASSERT(m_pFilter && m_pFilter->GetFilterGraph());
				QzCComPtr<AlaxInfoDirectShowSpy::IModuleVersionInformation> pModuleVersionInformation;
				QzCComPtr<AlaxInfoDirectShowSpy::ISpy> pSpy;
				m_pFilter->GetFilterGraph()->QueryInterface(__uuidof(AlaxInfoDirectShowSpy::IModuleVersionInformation), (VOID**) &pModuleVersionInformation);
				m_pFilter->GetFilterGraph()->QueryInterface(__uuidof(AlaxInfoDirectShowSpy::ISpy), (VOID**) &pSpy);
				if(pModuleVersionInformation && pSpy)
				{
					LONGLONG nFileVersion = 0;
					pModuleVersionInformation->get_FileVersion(&nFileVersion);
					if(nFileVersion >= ((1i64 << 48) + 1875)) // 1.0.0.1875+
						pSpy->CreateMediaSampleTrace(&m_pMediaSampleTrace);
				}
			}
		#endif // defined(ALAXINFODIRECTSHOWSPY_AVAILABLE)

        StartThread();
    }
    return hr;
}

HRESULT 
DemuxOutputPin::Inactive()
{
    HRESULT hr = CBaseOutputPin::Inactive();
    StopThread();
    return hr;
}

DWORD
DemuxOutputPin::ThreadProc()
{
    if (!IsConnected() || (m_pTrack == NULL))
    {
        return 0;
    }
    FormatHandler* pHandler = m_pTrack->Handler();
    if (pHandler == NULL)
    {
        m_pParser->NotifyEvent(EC_STREAM_ERROR_STOPPED, VFW_E_TYPE_NOT_ACCEPTED, 0);
        return 0;
    }

	for(; ; )
	{
		if(!InternalRestartThread())
			break;

		REFERENCE_TIME tStart, tStop;
		// HOTFIX: Volatile specifier is not really necessary here but it fixes a nasty problem with MainConcept AVC SDK violating x64 calling convention;
		//         MS compiler might choose to keep dRate in XMM6 register and the value would be destroyed by the violating call leading to incorrect 
		//         further streaming (wrong time stamps)
		volatile DOUBLE dRate;
		m_pParser->GetSeekingParams(&tStart, &tStop, (DOUBLE*) &dRate);

		#if defined(ALAXINFODIRECTSHOWSPY_AVAILABLE)
			if(m_pMediaSampleTrace)
				m_pMediaSampleTrace->RegisterNewSegment((IBaseFilter*) m_pFilter, (USHORT*) Name(), tStart, tStop, dRate, NULL, 0);
		#endif // defined(ALAXINFODIRECTSHOWSPY_AVAILABLE)

		DeliverNewSegment(tStart, tStop, dRate);

		m_tLate = 0;

		// wind back to key frame before and check against duration
		long nSample;
		size_t segment;
		if (!m_pTrack->CheckInSegment(tStart, true, &segment, &nSample))
		{
			DeliverEndOfStream();
			return 0;
		}

		if (tStop > m_pTrack->GetMovie()->Duration())
		{
			tStop = m_pTrack->GetMovie()->Duration();
		}
		// used only for quality management. No segment support yet
		long nStop = m_pTrack->TimesIndex()->DTSToSample(tStop);

		bool bFirst = true;
		pHandler->StartStream();

		bool bHandleQuality = false;
		// for some formats it might make sense to handle quality here, since we
		// can skip the read as well as the decode.
		// For now this is only enabled for the (rare and old) Quicktime RLE codec.
		if (*m_mt.Subtype() == MEDIASUBTYPE_QTRle)
		{
			bHandleQuality = true;
		}

		////////////////////////////////////////////////
		// HOTFIX: For zero length samples
		const GUID& Subtype = *m_mt.Subtype();
		const BOOL bIsFourCharacterCodeSubtype = memcmp(&Subtype.Data2, &MEDIASUBTYPE_YV12.Data2, sizeof (GUID) - offsetof(GUID, Data2)) == 0;
		const BOOL bIsAvc1Subtype = bIsFourCharacterCodeSubtype && (Subtype.Data1 == MAKEFOURCC('A', 'V', 'C', '1') || Subtype.Data1 == MAKEFOURCC('a', 'v', 'c', '1'));
		////////////////////////////////////////////////

		const HANDLE phObjects[] = { ExitEvent(), RestartRequestEvent() };
		BOOL bRestart = FALSE;
		for(; ; )
		{
			const DWORD nWaitResult = WaitForMultipleObjects(_countof(phObjects), phObjects, FALSE, 0);
			ASSERT(nWaitResult - WAIT_OBJECT_0 < _countof(phObjects) || nWaitResult == WAIT_TIMEOUT);
			if(nWaitResult != WAIT_TIMEOUT)
			{
				bRestart = nWaitResult == WAIT_OBJECT_0 + 1; // m_evRestartRequest
				break;
			}

			REFERENCE_TIME tNext, tDur;
			m_pTrack->GetTimeBySegment(nSample, segment, &tNext, &tDur);

			if (tNext >= tStop)
			{
				DeliverEndOfStream();
				break;
			}

			#pragma region Quality
			if (bHandleQuality)
			{
				// only for uncompressed YUV format -- no segment support yet
				REFERENCE_TIME late;
				{
					CAutoLock lock(&m_csLate);
					late = m_tLate;
				}
				REFERENCE_TIME perFrame = REFERENCE_TIME(tDur / dRate);
				// if we are more than two frames late, aim to be a frame early
				if (late > (perFrame * 2))
				{
					late += perFrame;
				}

				while (late > (perFrame / 2))
				{
					// we are more than 3/4 frame late. Should we skip?
					int next = m_pTrack->GetKeyMap()->Next(nSample);
					if (next && (next <= nStop))
					{
						REFERENCE_TIME tDiff = m_pTrack->TimesIndex()->SampleToCTS(next) - m_pTrack->TimesIndex()->SampleToCTS(nSample);
						tDiff = REFERENCE_TIME(tDiff / dRate);
						if ((next == (nSample+1)) || ((tDiff/2) < late))
						{
							// yes -- we are late by at least 1/2 of the distance to the next key frame
							late -= tDiff;
							nSample = next;
							bFirst = true;
						}
					}

					if (nSample != next)
					{
						break;
					}
				}
			}
			#pragma endregion 
			#pragma region Sample
			IMediaSamplePtr pSample;
			HRESULT hr = GetDeliveryBuffer(&pSample, NULL, NULL, 0);
			if (hr != S_OK)
			{
				break;
			}
			#pragma endregion 

			LONGLONG llPos = m_pTrack->SizeIndex()->Offset(nSample);
			long cSample = m_pTrack->SizeIndex()->Size(nSample);
			long lastSample = nSample;

			#pragma region Processing
			if ((cSample < 16) && (m_pTrack->IsOldAudioFormat()))
			{
				// this is the older MOV format: uncompressed audio is indexed by individual samples
				// fill the buffer with contiguous samples
				long nThis = lastSample;
				size_t segThis = segment;
				while (m_pTrack->NextBySegment(&nThis, &segThis))
				{
					REFERENCE_TIME tAdd, tDurAdd;
					m_pTrack->GetTimeBySegment(nThis, segThis, &tAdd, &tDurAdd);
					if (tAdd >= tStop)
					{
						break;
					}
					LONGLONG llPosNext = m_pTrack->SizeIndex()->Offset(nThis);
					if (llPosNext != (llPos + cSample))
					{
						break;
					}
					long cNext = m_pTrack->SizeIndex()->Size(nThis);
					if ((cSample + cNext) > pSample->GetSize())
					{
						break;
					}
					tDur += tDurAdd;
					cSample += cNext;
					lastSample = nThis;
					segment = segThis;
				}
			}
			else if ((cSample < 4) || (tDur < 1))
			{
				// some formats have empty frames with non-zero entries of less than 4 bytes
				////////////////////////////////////////////////
				// HOTFIX: H.264 video streams might have vital zero length NALs we cannot skip
				if(!bIsAvc1Subtype)
				////////////////////////////////////////////////
					cSample = 0;
			}
			#pragma endregion
			#pragma region Delivery
			if (cSample > 0)
			{
				if (cSample > pSample->GetSize())
				{
					// internal error since we checked the sizes
					// before setting the allocator
					m_pParser->NotifyEvent(EC_STREAM_ERROR_STOPPED, VFW_E_BUFFER_OVERFLOW, 0);
					break;
				}

				// format-specific handler does read from file and any conversion/wrapping needed
				cSample = pHandler->PrepareOutput(pSample, m_pTrack->GetMovie(), llPos, cSample);

				if (cSample > 0)
				{
					pSample->SetActualDataLength(cSample);
					if (m_pTrack->GetKeyMap()->SyncFor(nSample) == nSample)
					{
						pSample->SetSyncPoint(true);
					}

					{
						REFERENCE_TIME& nMediaStartTime = tNext;
						REFERENCE_TIME nMediaStopTime = nMediaStartTime + tDur;
						pSample->SetMediaTime(&nMediaStartTime, &nMediaStopTime);
					}

					REFERENCE_TIME tSampleStart = tNext - tStart;
					if (tSampleStart < 0)
					{
						pSample->SetPreroll(true);
					}
					REFERENCE_TIME tSampleEnd = tSampleStart + tDur;

					// oops. clearly you need to divide by dRate. At double the rate, each frame lasts half as long.
					tSampleStart = REFERENCE_TIME(tSampleStart / dRate);
					tSampleEnd = REFERENCE_TIME(tSampleEnd / dRate);

					pSample->SetTime(&tSampleStart, &tSampleEnd);
					if (bFirst)
					{
						pSample->SetDiscontinuity(true);
						bFirst = false;
					}
					#if defined(TRACE_SEEK)
						TCHAR pszText[1024] = { 0 };
						_stprintf(pszText, _T("%hs: tSampleStart %d ms, IsSyncPoint() %d, IsPreroll() %d\n"), __FUNCTION__, (LONG) (tSampleStart / 10000i64), pSample->IsSyncPoint() == S_OK, pSample->IsPreroll() == S_OK);
						OutputDebugString(pszText);
					#endif // defined(TRACE_SEEK)

					#if defined(ALAXINFODIRECTSHOWSPY_AVAILABLE)
						if(m_pMediaSampleTrace)
						{
							QzCComPtr<IMediaSample2> pMediaSample2;
							pSample->QueryInterface(__uuidof(IMediaSample2), (VOID**) &pMediaSample2);
							if(pMediaSample2)
							{
								AM_SAMPLE2_PROPERTIES Properties = { sizeof Properties };
								pMediaSample2->GetProperties(sizeof Properties, (BYTE*) &Properties);
								m_pMediaSampleTrace->RegisterMediaSample((IBaseFilter*) m_pFilter, (USHORT*) Name(), (BYTE*) &Properties, NULL, 0);
							}
						}
					#endif // defined(ALAXINFODIRECTSHOWSPY_AVAILABLE)

					Deliver(pSample);
				}
			}
			#pragma endregion

			nSample = lastSample;
			if (!m_pTrack->NextBySegment(&nSample, &segment))
			{
				DeliverEndOfStream();
				break;
			}
		}
		if(bRestart)
			continue;
		break;
	}
    return 0;
}

BOOL DemuxOutputPin::GetMajorMediaType(GUID& MajorType) const
{
	CAutoLock Lock(m_pLock);
	if(!m_mt.IsValid())
		return FALSE;
	MajorType = *m_mt.Type();
	return TRUE;
}

HRESULT DemuxOutputPin::SeekBackToKeyFrame(REFERENCE_TIME& tStart) const
{
	// NOTE: This basically duplicates initial seek logic in ThreadProc above
	if(!m_pTrack)
		return E_NOINTERFACE;
	size_t segment;
	long nSample;
	if (!m_pTrack->CheckInSegment(tStart, true, &segment, &nSample))
		return S_FALSE;
	REFERENCE_TIME tNext, tDur;
	m_pTrack->GetTimeBySegment(nSample, segment, &tNext, &tDur);
	if(tNext == tStart)
		return S_FALSE;
	tStart = tNext;
	return S_OK;
}

// output pin seeking implementation -----------------------

STDMETHODIMP 
DemuxOutputPin::GetCapabilities(DWORD * pCapabilities)
{

    // Some versions of DShow have an aggregation bug that
    // affects playback with Media Player. To work around this,
    // we need to report the capabilities and time format the
    // same on all pins, even though only one
    // can seek at once.
    *pCapabilities =        AM_SEEKING_CanSeekAbsolute |
                            AM_SEEKING_CanSeekForwards |
                            AM_SEEKING_CanSeekBackwards |
                            AM_SEEKING_CanGetDuration |
                            AM_SEEKING_CanGetCurrentPos |
                            AM_SEEKING_CanGetStopPos;
    return S_OK;
}

STDMETHODIMP 
DemuxOutputPin::CheckCapabilities(DWORD * pCapabilities)
{
    DWORD dwActual;
    GetCapabilities(&dwActual);
    if (*pCapabilities & (~dwActual))
    {
        return S_FALSE;
    }
    return S_OK;
}

STDMETHODIMP 
DemuxOutputPin::IsFormatSupported(const GUID * pFormat)
{
    // Some versions of DShow have an aggregation bug that
    // affects playback with Media Player. To work around this,
    // we need to report the capabilities and time format the
    // same on all pins, even though only one
    // can seek at once.
    if (*pFormat == TIME_FORMAT_MEDIA_TIME)
    {
        return S_OK;
    }
    return S_FALSE;

}
STDMETHODIMP 
DemuxOutputPin::QueryPreferredFormat(GUID * pFormat)
{
    // Some versions of DShow have an aggregation bug that
    // affects playback with Media Player. To work around this,
    // we need to report the capabilities and time format the
    // same on all pins, even though only one
    // can seek at once.
    *pFormat = TIME_FORMAT_MEDIA_TIME;
    return S_OK;
}

STDMETHODIMP 
DemuxOutputPin::GetTimeFormat(GUID *pFormat)
{
    return QueryPreferredFormat(pFormat);
}

STDMETHODIMP 
DemuxOutputPin::IsUsingTimeFormat(const GUID * pFormat)
{
    GUID guidActual;
    HRESULT hr = GetTimeFormat(&guidActual);

    if (SUCCEEDED(hr) && (guidActual == *pFormat))
    {
        return S_OK;
    } else
    {
        return S_FALSE;
    }
}

STDMETHODIMP 
DemuxOutputPin::ConvertTimeFormat(
                                      LONGLONG* pTarget, 
                                      const GUID* pTargetFormat,
                                      LONGLONG Source, 
                                      const GUID* pSourceFormat)
{
    // format guids can be null to indicate current format

    // since we only support TIME_FORMAT_MEDIA_TIME, we don't really
    // offer any conversions.
    if (pTargetFormat == 0 || *pTargetFormat == TIME_FORMAT_MEDIA_TIME)
    {
        if (pSourceFormat == 0 || *pSourceFormat == TIME_FORMAT_MEDIA_TIME)
        {
            *pTarget = Source;
            return S_OK;
        }
    }

    return E_INVALIDARG;
}

STDMETHODIMP 
DemuxOutputPin::SetTimeFormat(const GUID * pFormat)
{
    // only one pin can control seeking for the whole filter.
    // This method is used to select the seeker.
    if (*pFormat == TIME_FORMAT_MEDIA_TIME)
    {
        // try to select this pin as seeker (if the first to do so)
        if (m_pParser->SelectSeekingPin(this))
        {
            return S_OK;
        } else
        {
            return E_NOTIMPL;
        }
    } else if (*pFormat == TIME_FORMAT_NONE)
    {
        // deselect ourself, if we were the controlling pin
        m_pParser->DeselectSeekingPin(this);
        return S_OK;
    } else
    {
        // no other formats supported
        return E_NOTIMPL;
    }
}

STDMETHODIMP 
DemuxOutputPin::GetDuration(LONGLONG *pDuration)
{
    *pDuration = m_pParser->GetDuration();
    return S_OK;
}

STDMETHODIMP 
DemuxOutputPin::GetStopPosition(LONGLONG *pStop)
{
    REFERENCE_TIME tStart, tStop;
    double dRate;
    m_pParser->GetSeekingParams(&tStart, &tStop, &dRate);
    *pStop = tStop;
    return S_OK;
}

STDMETHODIMP 
DemuxOutputPin::GetCurrentPosition(LONGLONG *pCurrent)
{
    // this method is not supposed to report the previous start
    // position, but rather where we are now. This is normally
    // implemented by renderers, not parsers
    UNREFERENCED_PARAMETER(pCurrent);
    return E_NOTIMPL;
}

STDMETHODIMP 
DemuxOutputPin::SetPositions(
                                 LONGLONG * pCurrent, 
                                 DWORD dwCurrentFlags, 
                                 LONGLONG * pStop, 
                                 DWORD dwStopFlags)
{
	#if defined(TRACE_SEEK)
		const BOOL bIsSeekingPin = m_pParser->SelectSeekingPin(this);
		TCHAR pszText[1024] = { 0 };
		_stprintf(pszText, _T("%hs: this 0x%p, bIsSeekingPin %d, "), __FUNCTION__, this, bIsSeekingPin);
		if(pCurrent)
			_stprintf(pszText + _tcslen(pszText), _T("*pCurrent %I64d, "), *pCurrent);
		_stprintf(pszText + _tcslen(pszText), _T("dwCurrentFlags 0x%x, "), dwCurrentFlags);
		if(pCurrent)
			_stprintf(pszText + _tcslen(pszText), _T("*pStop %I64d, "), *pStop);
		_stprintf(pszText + _tcslen(pszText), _T("dwStopFlags 0x%x, "), dwStopFlags);
		_tcscat(pszText, _T("\n"));
		OutputDebugString(pszText);
	#endif // defined(TRACE_SEEK)

	ASSERT(!(dwCurrentFlags & (AM_SEEKING_Segment | AM_SEEKING_NoFlush)));
	ASSERT(!(dwStopFlags & (AM_SEEKING_SeekToKeyFrame | AM_SEEKING_Segment | AM_SEEKING_NoFlush)));

    // for media player, with the aggregation bug in DShow, it
    // is better to return success and ignore the call if we are
    // not the controlling pin
    if (!m_pParser->SelectSeekingPin(this))
    {
        return S_OK;
    }

	#if defined(TRACE_SEEK)
		const ULONG nTimeA = GetTickCount();
	#endif // defined(TRACE_SEEK)

    // fetch current properties
    REFERENCE_TIME tStart, tStop;
    double dRate;
    m_pParser->GetSeekingParams(&tStart, &tStop, &dRate);
    if (dwCurrentFlags & AM_SEEKING_AbsolutePositioning)
    {
        tStart = *pCurrent;
    } else if (dwCurrentFlags & AM_SEEKING_RelativePositioning)
    {
        tStart += *pCurrent;
    }

    if (dwStopFlags & AM_SEEKING_AbsolutePositioning)
    {
        tStop = *pStop;
    } else if (dwStopFlags & AM_SEEKING_IncrementalPositioning)
    {
        tStop = *pStop + tStart;
    } else
    {
        if (dwStopFlags & AM_SEEKING_RelativePositioning)
        {
            tStop += *pStop;
        }
    }

	HRESULT nResult;
    if(dwCurrentFlags & AM_SEEKING_PositioningBitsMask)
    {
        nResult = m_pParser->Seek(tStart, dwCurrentFlags & AM_SEEKING_SeekToKeyFrame, tStop, dRate);
    } else 
	if(dwStopFlags & AM_SEEKING_PositioningBitsMask)
    {
        nResult = m_pParser->SetStopTime(tStop); // stop change only
    } else
        return S_FALSE; // no operation required
	if(SUCCEEDED(nResult))
	{
		if(pCurrent && (dwCurrentFlags & AM_SEEKING_ReturnTime))
			*pCurrent = tStart;
		if(pStop && (dwStopFlags & AM_SEEKING_ReturnTime))
			*pStop = tStop;
	}

	#if defined(TRACE_SEEK)
		const ULONG nTimeB = GetTickCount();
		TCHAR pszText[1024] = { 0 };
		_stprintf(pszText, _T("%hs: this 0x%p, this, Time %d - %d (%d), dwCurrentFlags 0x%X") _T("\n"), __FUNCTION__, this, nTimeA, nTimeB, nTimeB - nTimeA, dwCurrentFlags);
		OutputDebugString(pszText);
	#endif // defined(TRACE_SEEK)

	return nResult;
}

STDMETHODIMP 
DemuxOutputPin::GetPositions(LONGLONG * pCurrent, LONGLONG * pStop)
{
    REFERENCE_TIME tStart, tStop;
    double dRate;
    m_pParser->GetSeekingParams(&tStart, &tStop, &dRate);
    *pCurrent = tStart;
    *pStop = tStop;
    return S_OK;
}

STDMETHODIMP 
DemuxOutputPin::GetAvailable(LONGLONG * pEarliest, LONGLONG * pLatest)
{
    if (pEarliest != NULL)
    {
        *pEarliest = 0;
    }
    if (pLatest != NULL)
    {
        *pLatest = m_pParser->GetDuration();
    }
    return S_OK;
}

STDMETHODIMP 
DemuxOutputPin::SetRate(double dRate)
{
    HRESULT hr = S_OK;
    if (m_pParser->SelectSeekingPin(this))
    {
        hr = m_pParser->SetRate(dRate);
    }
    return hr;
}



STDMETHODIMP 
DemuxOutputPin::GetRate(double * pdRate)
{
    REFERENCE_TIME tStart, tStop;
    double dRate;
    m_pParser->GetSeekingParams(&tStart, &tStop, &dRate);
    *pdRate = dRate;
    return S_OK;
}

STDMETHODIMP 
DemuxOutputPin::GetPreroll(LONGLONG * pllPreroll)
{
    // don't need to allow any preroll time for us
    *pllPreroll = 0;
    return S_OK;
}

// IDemuxOutputPin

STDMETHODIMP DemuxOutputPin::GetMediaSampleTimes(ULONG* pnCount, LONGLONG** ppnStartTimes, LONGLONG** ppnStopTimes, ULONG** ppnFlags, ULONG** ppnDataSizes)
{
	if(!pnCount || !ppnStartTimes)
		return E_POINTER;
	if(!ppnStopTimes && !ppnFlags && !ppnDataSizes)
		return E_INVALIDARG; // Nothing to Do
	if(ppnStopTimes || ppnDataSizes)
		return E_NOTIMPL;
	//CAutoLock Lock(m_pLock);
	if(!m_pTrack)
		return E_NOINTERFACE;
	*pnCount = (ULONG) m_pTrack->GetTimes(ppnStartTimes, ppnStopTimes, ppnFlags, ppnDataSizes);
	return S_OK;
}

