//
// MuxFilter.h
//
// Declaration of classes for DirectShow MPEG-4 Multiplexor filter
//
// Geraint Davies, May 2004
//
// Copyright (c) GDCL 2004-6. All Rights Reserved. 
// You are free to re-use this as the basis for your own filter development,
// provided you retain this copyright notice in the source.
// http://www.gdcl.co.uk
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Module_i.h"
#include "MovieWriter.h"
#include "TemporaryIndexFile.h"
#include "alloc.h"

// forward declarations
class Mpeg4Mux;
class MuxInput;
class MuxOutput;
class MuxAllocator;


// override the standard allocator to 
// force use of a larger number of buffers. 
// We use the input buffers to queue the chunks for
// interleaving, so the input connection must allow
// us to hold at least 2 seconds of data.
class MuxAllocator : public CMemAllocator, public IMuxMemAllocator
{
public:
// MuxAllocator
    MuxAllocator(LPUNKNOWN pUnk, HRESULT* phr, long cMaxBuffer, const CMediaType* pmt);
	~MuxAllocator();
	static LONG GetSuggestBufferCount()
	{
		return 100;
	}

// IUnknown
    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID InterfaceIdentifier, VOID** ppvObject)
	{
		#define A(x) if(InterfaceIdentifier == __uuidof(x)) return GetInterface((x*) this, ppvObject);
		A(IMuxMemAllocator)
		#undef A
		return __super::NonDelegatingQueryInterface(InterfaceIdentifier, ppvObject);
	}

// IMemAllocator
    // we override this just to increase the requested buffer count
    STDMETHODIMP SetProperties(
            ALLOCATOR_PROPERTIES* pRequest,
            ALLOCATOR_PROPERTIES* pActual);

// IMuxMemAllocator
	STDMETHOD(GetMinimalBufferCount)(LONG* pnMinimalBufferCount)
	{
		if(!pnMinimalBufferCount)
			return E_POINTER;
		*pnMinimalBufferCount = m_nMinimalBufferCount;
		return S_OK;
	}
	STDMETHOD(SetMinimalBufferCount)(LONG nMinimalBufferCount)
	{
		m_nMinimalBufferCount = nMinimalBufferCount;
		return S_OK;
	}

private:
	long m_cMaxBuffer;
    CMediaType m_mt;
	LONG m_nMinimalBufferCount;
};

// input pin, receives data corresponding to one
// media track in the file.
// Pins are created and deleted dynamically to 
// ensure that there is always one unconnected pin.
class MuxInput 
: public CBaseInputPin,
  public IAMStreamControl,
  public IMuxInputPin
{
public:
    MuxInput(Mpeg4Mux* pFilter, CCritSec* pLock, HRESULT* phr, LPCWSTR pName, int index);
	~MuxInput();

    INT GetIndex() const
	{
		return m_index;
	}

    // lifetime management for pins is normally delegated to the filter, but
    // we need to be able to create and delete them independently, so keep 
    // a separate refcount.
    STDMETHODIMP_(ULONG) NonDelegatingRelease()
    {
        return CUnknown::NonDelegatingRelease();
    }
    STDMETHODIMP_(ULONG) NonDelegatingAddRef()
    {
        return CUnknown::NonDelegatingAddRef();
    }

    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID InterfaceIdentifier, VOID** ppvObject)
	{
		#define A(x) if(InterfaceIdentifier == __uuidof(x)) return GetInterface((x*) this, ppvObject);
		A(IAMStreamControl)
		A(IMuxInputPin)
		#undef A
		return __super::NonDelegatingQueryInterface(InterfaceIdentifier, ppvObject);
	}

    // CBasePin overrides
    HRESULT CheckMediaType(const CMediaType* pmt);
    HRESULT GetMediaType(int iPosition, CMediaType* pmt);
    
    // input
	STDMETHODIMP NewSegment(REFERENCE_TIME nStartTime, REFERENCE_TIME nStopTime, DOUBLE fRate)
	{
		#if defined(ALAXINFODIRECTSHOWSPY_AVAILABLE)
			if(m_pMediaSampleTrace)
				m_pMediaSampleTrace->RegisterNewSegment((IBaseFilter*) m_pFilter, (USHORT*) Name(), nStartTime, nStopTime, fRate, NULL, 0);
		#endif // defined(ALAXINFODIRECTSHOWSPY_AVAILABLE)
		return __super::NewSegment(nStartTime, nStopTime, fRate);
	}
    STDMETHODIMP Receive(IMediaSample* pSample);
    STDMETHODIMP EndOfStream();
    STDMETHODIMP BeginFlush();
    STDMETHODIMP EndFlush();

    // state change
    HRESULT Active();
    HRESULT Inactive();

    // connection management -- used to maintain one free pin
    HRESULT BreakConnect();
    HRESULT CompleteConnect(IPin *pReceivePin);

    // support custom allocator
    STDMETHOD(GetAllocator)(IMemAllocator** ppAllocator) override;
	STDMETHOD(NotifyAllocator)(IMemAllocator* pAlloc, BOOL bReadOnly) override;

	// IAMStreamControl methods
	STDMETHOD(StartAt)(const REFERENCE_TIME* ptStart, DWORD dwCookie) override;
	STDMETHOD(StopAt)(const REFERENCE_TIME* ptStop, BOOL bSendExtra, DWORD dwCookie) override;
	STDMETHOD(GetInfo)(AM_STREAM_INFO* pInfo) override;

	// IMuxInputPin
	STDMETHOD(GetMemAllocators)(IUnknown** ppMemAllocatorUnknown, IUnknown** ppCopyMemAllocatorUnknown) override
	{
		if(!ppMemAllocatorUnknown || !ppCopyMemAllocatorUnknown)
			return E_POINTER;
		QzCComPtr<IMemAllocator>& pMemAllocator = reinterpret_cast<QzCComPtr<IMemAllocator>&>(*ppMemAllocatorUnknown);
		QzCComPtr<IMemAllocator>& pCopyMemAllocator = reinterpret_cast<QzCComPtr<IMemAllocator>&>(*ppCopyMemAllocatorUnknown);
		// WARN: Thread unsafe
		pMemAllocator = m_pAllocator;
		pCopyMemAllocator = m_pCopyAlloc;
		return S_OK;
	}
	STDMETHOD(SetMaximalCopyMemAllocatorCapacity)(ULONG nCapacity) override
	{
		m_nMaximalCopyBufferCapacity = (SIZE_T) nCapacity;
		return S_OK;
	}

private:
	bool ShouldDiscard(IMediaSample* pSample);
	HRESULT CopySampleProps(IMediaSample* pIn, IMediaSample* pOut);

private:
    Mpeg4Mux* m_pMux;
    int m_index;
    TrackWriter* m_pTrack;

	CCritSec m_csStreamControl;
	AM_STREAM_INFO m_StreamInfo;

	SIZE_T m_nMaximalCopyBufferCapacity;
	ContigBuffer m_CopyBuffer;
	Suballocator* m_pCopyAlloc;

	#if defined(ALAXINFODIRECTSHOWSPY_AVAILABLE)
		QzCComPtr<AlaxInfoDirectShowSpy::IMediaSampleTrace> m_pMediaSampleTrace;
	#endif // defined(ALAXINFODIRECTSHOWSPY_AVAILABLE)
};


// output pin, writes multiplexed data downstream
// using IMemOutputPin while running, and then writes 
// metadata using IStream::Write when stopping.
class MuxOutput 
: public CBaseOutputPin,
  public AtomWriter
{
public:
    MuxOutput(Mpeg4Mux* pFilter, CCritSec* pLock, HRESULT* phr);

    // CBaseOutputPin overrides
    HRESULT CheckMediaType(const CMediaType* pmt);
    HRESULT GetMediaType(int iPosition, CMediaType* pmt);
    HRESULT DecideBufferSize(IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES * pprop);
    HRESULT CompleteConnect(IPin *pReceivePin);
	HRESULT BreakConnect();

    // called from filter
    void Reset();
    void UseIStream();
    void FillSpace();

    // AtomWriter methods
    LONGLONG Length();
    LONGLONG Position();
    HRESULT Replace(LONGLONG pos, const BYTE* pBuffer, long cBytes);
    HRESULT Append(const BYTE* pBuffer, long cBytes);
	VOID NotifyMediaSampleWrite(INT nTrackIndex, IMediaSample* pMediaSample, SIZE_T nDataSize) override;

private:
    Mpeg4Mux* m_pMux;
    CCritSec m_csWrite;
    bool m_bUseIStream;
    LONGLONG m_llBytes;
};

// To pass seeking calls upstream we must try all connected input pins.
// Where two input pins lead to the same splitter, only one will be
// allowed to SetTimeFormat at once, so we must call this on all
// pins and store them, then call SetTimeFormat(NULL) once the
// operation is complete.
// 
// This class manages that list of seekable pins. 
// It is also used for seeking calls that do not need to set 
// the time format.
class SeekingAggregator
{
public:
    SeekingAggregator(CBaseFilter* pFilter, bool bSetTimeFormat = false);
    ~SeekingAggregator();

    typedef list<IMediaSeeking*>::iterator iterator;
    iterator Begin()
    {
        return m_Pins.begin();
    }
    iterator End()
    {
        return m_Pins.end();
    }

private:
    bool m_bSetTimeFormat;
    list<IMediaSeeking*> m_Pins;

};



class DECLSPEC_UUID("5FD85181-E542-4e52-8D9D-5D613C30131B")
Mpeg4Mux 
: public CBaseFilter,
  public IMediaSeeking,
  public IMuxFilter
{
public:
    // constructor method used by class factory
    static CUnknown* WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT* phr);

    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID InterfaceIdentifier, VOID** ppvObject)
	{
		#define A(x) if(InterfaceIdentifier == __uuidof(x)) return GetInterface((x*) this, ppvObject);
		A(IMediaSeeking)
		A(IMuxFilter)
		#undef A
		return __super::NonDelegatingQueryInterface(InterfaceIdentifier, ppvObject);
	}

    // filter registration tables
    static const AMOVIESETUP_MEDIATYPE m_sudType[];
    static const AMOVIESETUP_PIN m_sudPin[];
    static const AMOVIESETUP_FILTER m_sudFilter;

    // CBaseFilter methods
    int GetPinCount();
    CBasePin *GetPin(int n);
    STDMETHODIMP Stop();
    STDMETHODIMP Pause();

    void CreateInput();

    // called from input pin
    void OnDisconnect(int index);
    void OnConnect(int index);
    bool CanReceive(const CMediaType* pmt);
    TrackWriter* MakeTrack(int index, const CMediaType* pmt);
    void OnEOS();
	REFERENCE_TIME Start() { return m_tStart;}

	VOID NotifyMediaSampleWrite(INT nTrackIndex, LONGLONG nDataPosition, SIZE_T nDataSize, IMediaSample* pMediaSample);

    // we implement IMediaSeeking to allow encoding
    // of specific portions of an input clip, and
    // to report progress via the current position.
    // Calls (apart from current position) are
    // passed upstream to any pins that support seeking
public:
// IMediaSeeking
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
    STDMETHODIMP ConvertTimeFormat(LONGLONG * pTarget, const GUID * pTargetFormat, LONGLONG Source, const GUID * pSourceFormat );
    STDMETHODIMP SetPositions(LONGLONG * pCurrent, DWORD dwCurrentFlags, LONGLONG * pStop, DWORD dwStopFlags );
    STDMETHODIMP GetPositions(LONGLONG * pCurrent, LONGLONG * pStop );
    STDMETHODIMP GetAvailable(LONGLONG * pEarliest, LONGLONG * pLatest );
    STDMETHODIMP SetRate(double dRate);
    STDMETHODIMP GetRate(double * pdRate);
    STDMETHODIMP GetPreroll(LONGLONG * pllPreroll);

// IMuxFilter
    STDMETHOD(IsTemporaryIndexFileEnabled)() override
	{
		//_Z4(atlTraceCOM, 4, _T("this 0x%p\n"), this);
		//_ATLTRY
		//{
		    CAutoLock lock(&m_csFilter);
			if(!m_bTemporaryIndexFileEnabled)
				return S_FALSE;
		//}
		//_ATLCATCH(Exception)
		//{
		//	_C(Exception);
		//}
		return S_OK;
	}
    STDMETHOD(SetTemporaryIndexFileEnabled)(BOOL bTemporaryIndexFileEnabled) override
	{
		//_Z4(atlTraceCOM, 4, _T("this 0x%p, bEnabled %d\n"), this, bEnabled);
		//_ATLTRY
		//{
		    CAutoLock lock(&m_csFilter);
			if(m_bTemporaryIndexFileEnabled == bTemporaryIndexFileEnabled)
				return S_FALSE;
			//__D(IsActive(), VFW_E_WRONG_STATE);
			m_bTemporaryIndexFileEnabled = bTemporaryIndexFileEnabled;
		//}
		//_ATLCATCH(Exception)
		//{
		//	_C(Exception);
		//}
		return S_OK;
	}
	STDMETHOD(GetAlignTrackStartTimeDisabled)() override
	{
		//_Z4(atlTraceCOM, 4, _T("this 0x%p\n"), this);
		//_ATLTRY
		//{
		    CAutoLock lock(&m_csFilter);
			if(!m_bAlignTrackStartTimeDisabled)
				return S_FALSE;
		//}
		//_ATLCATCH(Exception)
		//{
		//	_C(Exception);
		//}
		return S_OK;
	}
	STDMETHOD(SetAlignTrackStartTimeDisabled)(BOOL bAlignTrackStartTimeDisabled) override
	{
		//_Z4(atlTraceCOM, 4, _T("this 0x%p, bAlignTrackStartTimeDisabled %d\n"), this, bAlignTrackStartTimeDisabled);
		//_ATLTRY
		//{
		    CAutoLock lock(&m_csFilter);
			if(m_bAlignTrackStartTimeDisabled == bAlignTrackStartTimeDisabled)
				return S_FALSE;
			//__D(IsActive(), VFW_E_WRONG_STATE);
			m_bAlignTrackStartTimeDisabled = bAlignTrackStartTimeDisabled;
		//}
		//_ATLCATCH(Exception)
		//{
		//	_C(Exception);
		//}
		return S_OK;
	}
	STDMETHOD(GetMinimalMovieDuration)(LONGLONG* pnMinimalMovieDuration) override
	{
		//_Z4(atlTraceCOM, 4, _T("this 0x%p\n"), this);
		//_ATLTRY
		//{
		//	__D(pnMinimalMovieDuration, E_POINTER);
			CAutoLock lock(&m_csFilter);
			*pnMinimalMovieDuration = (LONGLONG) m_nMinimalMovieDuration;
		//}
		//_ATLCATCH(Exception)
		//{
		//	_C(Exception);
		//}
		return S_OK;
	}
	STDMETHOD(SetMinimalMovieDuration)(LONGLONG nMinimalMovieDuration) override
	{
		//_Z4(atlTraceCOM, 4, _T("this 0x%p, nMinimalMovieDuration %I64d\n"), this, nMinimalMovieDuration);
		//_ATLTRY
		//{
			CAutoLock lock(&m_csFilter);
			if(m_nMinimalMovieDuration == (REFERENCE_TIME) nMinimalMovieDuration)
				return S_FALSE;
			//__D(IsActive(), VFW_E_WRONG_STATE);
			m_nMinimalMovieDuration = (REFERENCE_TIME) nMinimalMovieDuration;
		//}
		//_ATLCATCH(Exception)
		//{
		//	_C(Exception);
		//}
		return S_OK;
	}
	
private:
    // construct only via class factory
    Mpeg4Mux(LPUNKNOWN pUnk, HRESULT* phr);
    ~Mpeg4Mux();

private:
    CCritSec m_csFilter;
    CCritSec m_csTracks;
    MuxOutput* m_pOutput;
    vector<MuxInput*> m_pInputs;
    smart_ptr<MovieWriter> m_pMovie;

    // for reporting (via GetCurrentPosition) after completion
    REFERENCE_TIME m_tWritten;

	BOOL m_bAlignTrackStartTimeDisabled;
	REFERENCE_TIME m_nMinimalMovieDuration;

	BOOL m_bTemporaryIndexFileEnabled;
    CCritSec m_TemporaryIndexFileCriticalSection;
	CTemporaryIndexFile m_TemporaryIndexFile;
};

