//
// DemuxFilter.h
//
// Declaration of classes for DirectShow MPEG-4 Demultiplexor filter
//
// Geraint Davies, April 2004
//
// Copyright (c) GDCL 2004-6. All Rights Reserved. 
// You are free to re-use this as the basis for your own filter development,
// provided you retain this copyright notice in the source.
// http://www.gdcl.co.uk
//////////////////////////////////////////////////////////////////////

#pragma once

#include "thread.h"
#include "mpeg4.h"
#include "ElemType.h"
#include "mp4demux_h.h"

//
// Partially derived from GDCL sample MPEG-1 parser filter
// available in source form at http://www.gdcl.co.uk/articles
//

// Declaration of Mpeg-4 Demultiplexor Filter
//
// The filter has an IAsyncReader input pin and creates an output pin
// for each valid stream in the input pin.

class Mpeg4Demultiplexor;
class DemuxInputPin;
class DemuxOutputPin;

// pool multiple async requests from different threads
// ending at the same pin.
class AsyncRequestor
{
public:
	AsyncRequestor();

	void Active(IAsyncReader* pRdr);
	void Inactive();
	HRESULT Read(LONGLONG llOffset, long cBytes, BYTE* pBuffer);

	enum
	{
		max_buffer_size = 200 * 1024,
	};

private:
	CCritSec m_csRequests;
	IAsyncReaderPtr m_rdr;
	IMemAllocatorPtr m_pAlloc;
	list<IMediaSamplePtr> m_free;
	list<IMediaSamplePtr> m_requests;
	bool m_bBusy;
	CAMEvent m_ev;
	ALLOCATOR_PROPERTIES m_props;
};

// Since we are not using IMemInputPin, it does not make sense to derive from 
// CBaseInputPin. We need random access to the data, so we do not use CPullPin.
// A worker thread on the filter calls a Read method to synchronously fetch data
// from the source.
//
// Since we provide the data source for the file, we implement
// AtomReader to provide access to the data.
class DemuxInputPin 
: public CBasePin,
  public AtomReader
{
public:
    DemuxInputPin(Mpeg4Demultiplexor* pFilter, CCritSec* pLock, HRESULT* phr);

    // base pin overrides
    HRESULT CheckMediaType(const CMediaType* pmt);
    HRESULT GetMediaType(int iPosition, CMediaType* pmt);
    STDMETHODIMP BeginFlush();
    STDMETHODIMP EndFlush();
    HRESULT CompleteConnect(IPin* pPeer);
    HRESULT BreakConnect();

    // AtomReader abstraction:
    // access to the file from the Mpeg-4 parsing classes
    HRESULT Read(LONGLONG llOffset, long cBytes, BYTE* pBuffer);
    LONGLONG Length();

    // ...but we don't support the caching interface for the whole file
    // -- only individual atoms should be cached in memory
    bool IsBuffered()
    {
        return false;
    }

    // calls to Buffer and BufferRelease are refcounted and should correspond.
    const BYTE* Buffer() 
    {
        return NULL;
    }
    void BufferRelease() 
    {
    }

    HRESULT Active();
    HRESULT Inactive();

private:
    Mpeg4Demultiplexor* m_pParser;

	AsyncRequestor m_requestor;
};


class DemuxOutputPin
: public CBaseOutputPin,
  public IMediaSeeking,
  public thread,
  public IDemuxOutputPin
{
public:
    DemuxOutputPin(MovieTrack* pTrack, Mpeg4Demultiplexor* pDemux, CCritSec* pLock, HRESULT* phr, LPCWSTR pName);
	DECLARE_IUNKNOWN
	STDMETHODIMP NonDelegatingQueryInterface(REFIID iid, void** ppv);

    // normally, pins share the filter's lifetime, but we will release the pins
    // when the input is disconnected, so they need to have a normal COM lifetime
    STDMETHODIMP_(ULONG) NonDelegatingAddRef()
    {
        return CUnknown::NonDelegatingAddRef();
    }
    STDMETHODIMP_(ULONG) NonDelegatingRelease()
    {
        return CUnknown::NonDelegatingRelease();
    }

    HRESULT CheckMediaType(const CMediaType *pmt);
    HRESULT GetMediaType(int iPosition, CMediaType *pmt);
    HRESULT SetMediaType(const CMediaType* pmt);
    HRESULT DecideBufferSize(IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES * pprop);
    
	// normally this is handled in the decoder, but there are some
	// rare cases (low complexity, high bitrate with temporal compression) where
	// it makes sense to do it here. We record the info
	// and only use it if it makes sense in ThreadProc
    STDMETHODIMP Notify(IBaseFilter * pSender, Quality q)
    {
		UNREFERENCED_PARAMETER(pSender);
		CAutoLock lock(&m_csLate);
		m_tLate = q.Late;
		return S_OK;
    }
    
    HRESULT Active();
    HRESULT Inactive();
    DWORD ThreadProc();

	HRESULT DeliverEndOfStream()
	{
		#if defined(WITH_DIRECTSHOWSPY)
			if(m_pMediaSampleTrace)
				m_pMediaSampleTrace->RegisterEndOfStream((IBaseFilter*) m_pFilter, Name(), nullptr, 0);
		#endif // defined(WITH_DIRECTSHOWSPY)

		return __super::DeliverEndOfStream();
	}
    HRESULT DeliverBeginFlush()
	{
		#if defined(WITH_DIRECTSHOWSPY)
			if(m_pMediaSampleTrace)
				m_pMediaSampleTrace->RegisterComment((IBaseFilter*) m_pFilter, Name(), L"Begin Flush", 0);
		#endif // defined(WITH_DIRECTSHOWSPY)

		return __super::DeliverBeginFlush();
	}
    HRESULT DeliverEndFlush()
	{
		#if defined(WITH_DIRECTSHOWSPY)
			if(m_pMediaSampleTrace)
				m_pMediaSampleTrace->RegisterComment((IBaseFilter*) m_pFilter, Name(), L"End Flush", 0);
		#endif // defined(WITH_DIRECTSHOWSPY)

		return __super::DeliverEndFlush();
	}

	BOOL GetMajorMediaType(GUID& MajorType) const;
	HRESULT SeekBackToKeyFrame(REFERENCE_TIME& tStart) const;

// IMediaSeeking
public:
    STDMETHODIMP GetCapabilities(DWORD * pCapabilities );
    STDMETHODIMP CheckCapabilities(DWORD * pCapabilities );
    STDMETHODIMP IsFormatSupported(const GUID * pFormat);
    STDMETHODIMP QueryPreferredFormat(GUID * pFormat);
    STDMETHODIMP GetTimeFormat(GUID *pFormat);
    STDMETHODIMP IsUsingTimeFormat(const GUID * pFormat);
    STDMETHODIMP SetTimeFormat(const GUID * pFormat);
    STDMETHODIMP GetDuration(LONGLONG *pDuration);
    STDMETHODIMP GetStopPosition(LONGLONG *pStop);
    STDMETHODIMP GetCurrentPosition(LONGLONG *pCurrent);
    STDMETHODIMP ConvertTimeFormat(LONGLONG * pTarget, const GUID * pTargetFormat,
                              LONGLONG    Source, const GUID * pSourceFormat );
    STDMETHODIMP SetPositions(LONGLONG * pCurrent, DWORD dwCurrentFlags
			, LONGLONG * pStop, DWORD dwStopFlags );
    STDMETHODIMP GetPositions(LONGLONG * pCurrent,
                              LONGLONG * pStop );
    STDMETHODIMP GetAvailable(LONGLONG * pEarliest, LONGLONG * pLatest );
    STDMETHODIMP SetRate(double dRate);
    STDMETHODIMP GetRate(double * pdRate);
    STDMETHODIMP GetPreroll(LONGLONG * pllPreroll);

// IDemuxOutputPin
    STDMETHODIMP GetMediaSampleTimes(ULONG* pnCount, LONGLONG** ppnStartTimes, LONGLONG** ppnStopTimes, ULONG** ppnFlags, ULONG** ppnDataSizes);

private:
    MovieTrack* m_pTrack;
    Mpeg4Demultiplexor* m_pParser;

	// quality record
	CCritSec m_csLate;
	REFERENCE_TIME m_tLate;

	#if defined(WITH_DIRECTSHOWSPY)
		QzCComPtr<AlaxInfoDirectShowSpy::IMediaSampleTrace> m_pMediaSampleTrace;
	#endif // defined(WITH_DIRECTSHOWSPY)
};
typedef IPinPtr DemuxOutputPinPtr;

extern bool g_ElstMediaTimeTruncation;
__declspec(selectany) bool g_ElstMediaTimeTruncation = false;

class DECLSPEC_UUID("025BE2E4-1787-4da4-A585-C5B2B9EEB57C")
Mpeg4Demultiplexor
: public CBaseFilter, public IDemuxFilter
{
public:
    // constructor method used by class factory
    static CUnknown* WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT* phr);

	DECLARE_IUNKNOWN
	STDMETHODIMP NonDelegatingQueryInterface(REFIID iid, void** ppv)
	{
		if(iid == __uuidof(IDemuxFilter))
			return GetInterface((IDemuxFilter*) this, ppv);
		return __super::NonDelegatingQueryInterface(iid, ppv);
	}

    // filter registration tables
    static const AMOVIESETUP_MEDIATYPE m_sudType[];
    static const AMOVIESETUP_PIN m_sudPin[];
    static const AMOVIESETUP_FILTER m_sudFilter;

    // CBaseFilter methods
    int GetPinCount();
    CBasePin *GetPin(int n);

    // called from input pin
    HRESULT BeginFlush();
    HRESULT EndFlush();
    HRESULT CompleteConnect(IPin* pPeer);
    HRESULT BreakConnect();

    // called from output pins for seeking support
    bool SelectSeekingPin(DemuxOutputPin* pPin);
    void DeselectSeekingPin(DemuxOutputPin* pPin);
    REFERENCE_TIME GetDuration();
    void GetSeekingParams(REFERENCE_TIME* ptStart, REFERENCE_TIME* ptStop, double* pdRate);
    HRESULT Seek(REFERENCE_TIME& tStart, BOOL bSeekToKeyFrame, REFERENCE_TIME tStop, double dRate);
    HRESULT SetRate(double dRate);
    HRESULT SetStopTime(REFERENCE_TIME tStop);

// IDemuxFilter
    IFACEMETHOD(GetInvalidTrackCount)(ULONG* InvalidTrackCount) override
    {
        try
        {
            THROW_HR_IF_NULL(E_POINTER, InvalidTrackCount);
            // WARN: Thread unsafe
            *InvalidTrackCount = m_pMovie ? static_cast<ULONG>(m_pMovie->InvalidTrackCount()) : 0;
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(GetComment)(BSTR* Comment) override
    {
        try
        {
            THROW_HR_IF_NULL(E_POINTER, Comment);
            *Comment = nullptr;
            RETURN_HR_IF(S_FALSE, !m_pMovie);
            // WARN: Thread unsafe?
            auto const& CommentA = m_pMovie->Comment();
		    auto const Capacity = MultiByteToWideChar(CP_UTF8, 0, CommentA.c_str(), static_cast<INT>(CommentA.size()), nullptr, 0);
        	std::wstring CommentW;
            CommentW.resize(Capacity);
		    auto const Size = MultiByteToWideChar(CP_UTF8, 0, CommentA.c_str(), static_cast<INT>(CommentA.size()), const_cast<wchar_t*>(CommentW.data()), Capacity);
    		CommentW.resize(Size);
            *Comment = wil::make_bstr(CommentW.c_str()).release();
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(GetAttributes)(VARIANT* Values) override
    {
        try
        {
            THROW_HR_IF_NULL(E_POINTER, Values);
            VariantInit(Values);
            RETURN_HR_IF(S_FALSE, !m_pMovie);
            // WARN: Thread unsafe?
            auto const& AttributeVector = m_pMovie->AttributeVector();
            if(!AttributeVector.empty())
            {
                SAFEARRAYBOUND Bounds[] { { static_cast<ULONG>(AttributeVector.size()) } };
                Values->parray = SafeArrayCreate(VT_VARIANT, static_cast<UINT>(std::size(Bounds)), Bounds);
                Values->vt = VT_ARRAY | VT_VARIANT;
                size_t Index = 0;
                for(auto&& Attribute: AttributeVector)
                {
                    LONG ArrayIndices[] { static_cast<LONG>(Index++) };
                    wil::unique_variant VariantAttribute;
                    {
                        SAFEARRAYBOUND AttributeBounds[] { { 2u } };
                        VariantAttribute.parray = SafeArrayCreate(VT_BSTR, static_cast<UINT>(std::size(AttributeBounds)), AttributeBounds);
                        VariantAttribute.vt = VT_ARRAY | VT_BSTR;
                        wchar_t Name[128];
                        swprintf_s(Name, L"%hs", Attribute.first.c_str());
                        LONG Indices[] { 0 };
                        THROW_IF_FAILED(SafeArrayPutElement(VariantAttribute.parray, Indices, wil::make_bstr(Name).get())); 
                        Indices[0]++;
                        THROW_IF_FAILED(SafeArrayPutElement(VariantAttribute.parray, Indices, wil::make_bstr(Attribute.second.c_str()).get()));
                    }
                    THROW_IF_FAILED(SafeArrayPutElement(Values->parray, ArrayIndices, &VariantAttribute));
                }
            }
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(Compatibility)(BSTR Name, [[maybe_unused]] VARIANT* Value) override
    {
        try
        {
            THROW_HR_IF_NULL(E_INVALIDARG, Name);
            if(wcscmp(Name, L"elst media_time truncation") == 0)
            {
                // WARN: Thread unsafe and also global (we need to pass this to Movie consutructor before it is created)
                g_ElstMediaTimeTruncation = true;
            }
        }
        CATCH_RETURN();
        return S_OK;
    }

private:
    // construct only via class factory
    Mpeg4Demultiplexor(LPUNKNOWN pUnk, HRESULT* phr);
    ~Mpeg4Demultiplexor();

    DemuxOutputPin* Output(int n)
    {
        return static_cast<DemuxOutputPin*>((IPin*)m_Outputs[n]);
    }
private:
    CCritSec m_csFilter;
    DemuxInputPin* m_pInput;

    // one output pin for each enabled track
    vector<DemuxOutputPinPtr> m_Outputs;

    // for seeking
    CCritSec m_csSeeking;
    REFERENCE_TIME m_tStart;
    REFERENCE_TIME m_tStop;
    double m_dRate;
    DemuxOutputPin* m_pSeekingPin;

    // file headers
    smart_ptr<Movie> m_pMovie;
};



