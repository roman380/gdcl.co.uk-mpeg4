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
    if (pPin == m_pSeekingPin)
    {
        m_pSeekingPin = pPin;
    }
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
Mpeg4Demultiplexor::Seek(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    // stop all output pin threads using BeginFlush/StopThread/EndFlush
    UINT i;
    if (IsActive())
    {
        for (i =0; i < m_Outputs.size(); i++)
        {
            DemuxOutputPin* p = Output(i);
            if (p->IsConnected())
            {
                p->DeliverBeginFlush();
            }
        }
        for (i =0; i < m_Outputs.size(); i++)
        {
            DemuxOutputPin* p = Output(i);
            if (p->IsConnected())
            {
                p->StopThread();
            }
        }
        for (i =0; i < m_Outputs.size(); i++)
        {
            DemuxOutputPin* p = Output(i);
            if (p->IsConnected())
            {
                p->DeliverEndFlush();
            }
        }
    }
    
    // new seeking params
    m_tStart = tStart;
    m_tStop = tStop;
    m_dRate = dRate;

    // restart all threads for connected pins if we are active
    if (IsActive())
    {
    
        for (i =0; i < m_Outputs.size(); i++)
        {
            DemuxOutputPin* p = Output(i);
            if (p->IsConnected())
            {
                p->StartThread();
            }
        }
    }

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
DemuxInputPin::Read(LONGLONG llOffset, long cBytes, BYTE* pBuffer)
{
    HRESULT hr = E_NOINTERFACE;
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

    REFERENCE_TIME tStart, tStop;
    double dRate;
    m_pParser->GetSeekingParams(&tStart, &tStop, &dRate);
    DeliverNewSegment(tStart, tStop, dRate);

	m_tLate = 0;

    if (tStart >= m_pTrack->GetMovie()->Duration())
    {   DeliverEndOfStream();
        return 0;
    }

    // find start sample.
    // Note that this finds the sample to be decoded at tStart,
    // not presented at tStart -- so we may start a few samples early
    // This is ok since the timestamping will
    // be ok (we subtract tStart from all times) and we wind back to the key frame anyway.
    long nStart = m_pTrack->TimesIndex()->DTSToSample(tStart);

    // start at preceding key frame
    nStart = m_pTrack->GetKeyMap()->SyncFor(nStart);

    // stop time -- may be "max"
    long nStop = 0;
    if (tStop < m_pTrack->GetMovie()->Duration())
    {
        nStop = m_pTrack->TimesIndex()->DTSToSample(tStop);
    }
    if ((nStop == 0) || (nStop >= m_pTrack->SizeIndex()->SampleCount()))
    {
        nStop = m_pTrack->SizeIndex()->SampleCount()-1;
    }


    long nSample = nStart;
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

    while (!ShouldExit())
    {
		if (bHandleQuality)
		{
			REFERENCE_TIME late;
			{
				CAutoLock lock(&m_csLate);
				late = m_tLate;
			}
			REFERENCE_TIME perFrame = REFERENCE_TIME(m_pTrack->TimesIndex()->Duration(nSample) / dRate);
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

        IMediaSamplePtr pSample;
        HRESULT hr = GetDeliveryBuffer(&pSample, NULL, NULL, 0);
        if (hr != S_OK)
        {
            break;
        }
		REFERENCE_TIME tDur = m_pTrack->TimesIndex()->Duration(nSample);
        LONGLONG llPos = m_pTrack->SizeIndex()->Offset(nSample);

        long cSample = m_pTrack->SizeIndex()->Size(nSample);
		long SampleCount = 1;
		if ((cSample < 16) && (m_pTrack->IsOldAudioFormat()))
		{
			// this is the older MOV format: uncompressed audio is indexed by individual samples
			// fill the buffer with contiguous samples
			while ((nSample + SampleCount) <= nStop)
			{
				long cNext = m_pTrack->SizeIndex()->Size(nSample);
				LONGLONG llPosNext = m_pTrack->SizeIndex()->Offset(nSample + SampleCount);
				if ((cSample + cNext) > pSample->GetSize())
				{
					break;
				}
				if (llPosNext != (llPos + cSample))
				{
					break;
				}
				tDur += m_pTrack->TimesIndex()->Duration(nSample + SampleCount);
				SampleCount++;
				cSample += cNext;
			}
		}

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
                REFERENCE_TIME tSampleStart = m_pTrack->TimesIndex()->SampleToCTS(nSample) - tStart;
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
                Deliver(pSample);
            }
        }

        nSample += SampleCount;
        if (nSample > nStop)
        {
            DeliverEndOfStream();
            break;
        }
    }
    return 0;
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
    // for media player, with the aggregation bug in DShow, it
    // is better to return success and ignore the call if we are
    // not the controlling pin
    if (!m_pParser->SelectSeekingPin(this))
    {
        return S_OK;
    }

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

    if (dwCurrentFlags & AM_SEEKING_PositioningBitsMask)
    {
        return m_pParser->Seek(tStart, tStop, dRate);
    } else if (dwStopFlags & AM_SEEKING_PositioningBitsMask)
    {
        // stop change only
        return m_pParser->SetStopTime(tStop);
    } else
    {
        // no operation required
        return S_FALSE;
    }

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


