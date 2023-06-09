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

#include <memory>

#include "mp4mux_h.h"
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
    MuxInput(Mpeg4Mux* pFilter, CCritSec* pLock, HRESULT* phr, LPCWSTR pName, uint32_t index);
    ~MuxInput();

    uint32_t GetIndex() const
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
        #if defined(WITH_DIRECTSHOWSPY)
            if(m_pMediaSampleTrace)
                m_pMediaSampleTrace->RegisterNewSegment((IBaseFilter*) m_pFilter, Name(), nStartTime, nStopTime, fRate, nullptr, 0);
        #endif // defined(WITH_DIRECTSHOWSPY)
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
    uint32_t m_index;
    std::shared_ptr<TrackWriter> m_pTrack;

    CCritSec m_csStreamControl;
    AM_STREAM_INFO m_StreamInfo { };

    size_t m_nMaximalCopyBufferCapacity;
    ContigBuffer m_CopyBuffer;
    Suballocator* m_pCopyAlloc = nullptr;

    #if defined(WITH_DIRECTSHOWSPY)
        QzCComPtr<AlaxInfoDirectShowSpy::IMediaSampleTrace> m_pMediaSampleTrace;
    #endif // defined(WITH_DIRECTSHOWSPY)
};

CBaseFilter* ToBaseFilter(Mpeg4Mux* Value);

// output pin, writes multiplexed data downstream
// using IMemOutputPin while running, and then writes 
// metadata using IStream::Write when stopping.
class MuxOutput 
: public CBaseOutputPin,
  public AtomWriter
{
public:
    MuxOutput(Mpeg4Mux* Filter, CCritSec* Lock, HRESULT* Result) : 
        m_pMux(Filter),
        CBaseOutputPin(NAME("MuxOutput"), ToBaseFilter(Filter), Lock, Result, L"Output")
    {
    }

    void CombineDataCapacity(size_t CombineDataCapacity)
    {
        CAutoLock lock(&m_csWrite);
        WI_ASSERT(m_CombineData.empty());
        m_CombineDataCapacity = CombineDataCapacity;
    }

    // CBaseOutputPin overrides
    HRESULT CheckMediaType(const CMediaType* pmt);
    HRESULT GetMediaType(int iPosition, CMediaType* pmt);
    HRESULT DecideBufferSize(IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES * pprop);
    HRESULT CompleteConnect(IPin* ReceivePin)
    {
        // NOTE: make sure that this is the file writer, supporting IStream, or we will not be able to write out the metadata at stop time
        auto Stream = wil::try_com_query<IStream>(ReceivePin);
        RETURN_HR_IF_NULL(E_NOINTERFACE, Stream);
        RETURN_IF_FAILED(CBaseOutputPin::CompleteConnect(ReceivePin));
        m_Stream = std::move(Stream);
        return S_OK;
    }
    HRESULT BreakConnect()
    {
        m_Stream.reset();
        return __super::BreakConnect();
    }

    // called from filter
    void Reset()
    {
        CAutoLock lock(&m_csWrite);
        m_bUseIStream = true;		// always use IStream, so we don't fail when downstream filter is stopped first
        m_DataSize = 0;
    }
    void UseIStream()
    {
        CAutoLock lock(&m_csWrite);
        m_bUseIStream = true;
    }
    void CompleteOutput()
    {
        IStreamPtr Stream = GetConnected();
        if(!Stream)
            return;
        LOG_IF_FAILED(WriteCombineData());
        LARGE_INTEGER SeekPosition;
        SeekPosition.QuadPart = 0;
        ULARGE_INTEGER EndPosition;
        auto const SeekResult = Stream->Seek(SeekPosition, STREAM_SEEK_END, &EndPosition);
        LOG_IF_FAILED(SeekResult);
        if(FAILED(SeekResult))
            return;
        if(EndPosition.QuadPart <= m_DataSize)
            return;
        uint64_t const FreeDataSize = EndPosition.QuadPart - m_DataSize;
        if(FreeDataSize < std::numeric_limits<int32_t>::max() && FreeDataSize >= 8)
        {
            // create a free chunk
            BYTE b[8];
            Write32(static_cast<uint32_t>(FreeDataSize), b);
            Write32('free', b + 4);
            Append(b, 8);
        }
    }

// AtomWriter
    uint64_t Length() const override
    {
        // length of this atom container (ie location of next atom)
        return m_DataSize;
    }
    int64_t Position() const override
    {
        // start of this container in absolute byte position
        return 0;
    }
    HRESULT Replace(int64_t Position, uint8_t const* Data, size_t DataSize) override;
    HRESULT Append(uint8_t const* Data, size_t DataSize) override
    {
        RETURN_IF_FAILED(Replace(m_DataSize, Data, DataSize));
        m_DataSize += DataSize;
        return S_OK;
    }
    void NotifyMediaSampleWrite(uint32_t TrackIndex, wil::com_ptr<IMediaSample> const& MediaSample, size_t DataSize) override;

private:
    HRESULT WriteCombineData()
    {
        CAutoLock lock(&m_csWrite);
        WI_ASSERT(m_bUseIStream); // Assumed code path in Replace
        if(m_CombineData.empty())
            return S_OK;
        DbgLog((LOG_TRACE, 4, TEXT("m_DataSize %llu, m_CombineData.size() %zu"), m_DataSize, m_CombineData.size()));
        IStreamPtr Stream = GetConnected();
        RETURN_HR_IF_NULL(E_NOINTERFACE, Stream);
        LARGE_INTEGER StreamPosition;
        StreamPosition.QuadPart = m_DataSize - m_CombineData.size();
        ULARGE_INTEGER ResultStreamPosition;
        RETURN_IF_FAILED(Stream->Seek(StreamPosition, STREAM_SEEK_SET, &ResultStreamPosition));
        ULONG WriteDataSize;
        RETURN_IF_FAILED(Stream->Write(m_CombineData.data(), static_cast<ULONG>(m_CombineData.size()), &WriteDataSize));
        RETURN_HR_IF(E_FAIL, WriteDataSize != m_CombineData.size());
        m_CombineData.clear();
        return S_OK;
    }

    Mpeg4Mux* m_pMux;
    wil::com_ptr<IStream> m_Stream;
    mutable CCritSec m_csWrite;
    bool m_bUseIStream = true; // use IStream always
    uint64_t m_DataSize = 0;
    size_t m_CombineDataCapacity = 0;
    std::vector<uint8_t> m_CombineData;
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
    std::shared_ptr<TrackWriter> MakeTrack(int index, const CMediaType* pmt);
    void OnEOS();
    REFERENCE_TIME Start() { return m_tStart;}

    void NotifyMediaSampleWrite(uint32_t TrackIndex, uint64_t DataPosition, size_t DataSize, wil::com_ptr<IMediaSample> const& MediaSample);

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
    IFACEMETHOD(IsTemporaryIndexFileEnabled)() override
    {
        //TRACE(L"this 0x%p\n", this);
        try
        {
            CAutoLock lock(&m_csFilter);
            if(!m_TemporaryIndexFileEnabled)
                return S_FALSE;
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(SetTemporaryIndexFileEnabled)(BOOL TemporaryIndexFileEnabled) override
    {
        //TRACE(L"this 0x%p, TemporaryIndexFileEnabled %d\n", this, TemporaryIndexFileEnabled);
        try
        {
            CAutoLock lock(&m_csFilter);
            if(m_TemporaryIndexFileEnabled == static_cast<bool>(TemporaryIndexFileEnabled))
                return S_FALSE;
            //THROW_HR_IF(VFW_E_WRONG_STATE, IsActive());
            m_TemporaryIndexFileEnabled = static_cast<bool>(TemporaryIndexFileEnabled);
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(GetAlignTrackStartTimeDisabled)() override
    {
        //TRACE(atlTraceCOM, 4, _T("this 0x%p\n"), this);
        try
        {
            CAutoLock lock(&m_csFilter);
            if(!m_bAlignTrackStartTimeDisabled)
                return S_FALSE;
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(SetAlignTrackStartTimeDisabled)(BOOL bAlignTrackStartTimeDisabled) override
    {
        //TRACE(atlTraceCOM, 4, _T("this 0x%p, bAlignTrackStartTimeDisabled %d\n"), this, bAlignTrackStartTimeDisabled);
        try
        {
            CAutoLock lock(&m_csFilter);
            if(m_bAlignTrackStartTimeDisabled == bAlignTrackStartTimeDisabled)
                return S_FALSE;
            //THROW_HR_IF(VFW_E_WRONG_STATE, IsActive());
            m_bAlignTrackStartTimeDisabled = bAlignTrackStartTimeDisabled;
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(GetMinimalMovieDuration)(LONGLONG* pnMinimalMovieDuration) override
    {
        //TRACE(atlTraceCOM, 4, _T("this 0x%p\n"), this);
        try
        {
        	THROW_HR_IF_NULL(E_POINTER, pnMinimalMovieDuration);
            CAutoLock lock(&m_csFilter);
            *pnMinimalMovieDuration = static_cast<LONGLONG>(m_nMinimalMovieDuration);
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(SetMinimalMovieDuration)(LONGLONG nMinimalMovieDuration) override
    {
        //TRACE(atlTraceCOM, 4, _T("this 0x%p, nMinimalMovieDuration %I64d\n"), this, nMinimalMovieDuration);
        try
        {
            CAutoLock lock(&m_csFilter);
            if(m_nMinimalMovieDuration == static_cast<REFERENCE_TIME>(nMinimalMovieDuration))
                return S_FALSE;
            //THROW_HR_IF(VFW_E_WRONG_STATE, IsActive());
            m_nMinimalMovieDuration = static_cast<REFERENCE_TIME>(nMinimalMovieDuration);
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(SetComment)(BSTR Comment) override
    {
        //TRACE(atlTraceCOM, 4, _T("this 0x%p, Comment 0x%p \"%ls\"\n"), this, Comment, Comment ? Comment : L"");
        try
        {
            CAutoLock lock(&m_csFilter);
            //THROW_HR_IF(VFW_E_WRONG_STATE, IsActive());
            if(Comment)
            {
                auto const CommentLength = wcslen(Comment);
                m_Comment.resize(WideCharToMultiByte(CP_UTF8, 0, Comment, static_cast<int>(CommentLength), nullptr, 0, nullptr, nullptr) + 1);
                m_Comment.resize(WideCharToMultiByte(CP_UTF8, 0, Comment, static_cast<int>(CommentLength), const_cast<char*>(m_Comment.data()), static_cast<int>(m_Comment.size()), nullptr, nullptr));
            } else
                m_Comment.clear();
            if(m_pMovie)
                m_pMovie->SetComment(m_Comment);
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(SetTemporaryIndexFileDirectory)(BSTR TemporaryIndexFileDirectory) override
    {
        //TRACE(L"this 0x%p, TemporaryIndexFileDirectory %ls\n", this, TemporaryIndexFileDirectory ? TemporaryIndexFileDirectory : L"(null)");
        try
        {
            CAutoLock lock(&m_csFilter);
            //THROW_HR_IF(VFW_E_WRONG_STATE, IsActive());
            m_TemporaryIndexFileDirectory = TemporaryIndexFileDirectory;
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(SetCombineOutputCapacity)(ULONG CombineOutputCapacity) override
    {
        //TRACE(L"this 0x%p, CombineOutputCapacity %u\n", this, CombineOutputCapacity);
        try
        {
            CAutoLock lock(&m_csFilter);
            THROW_HR_IF(VFW_E_WRONG_STATE, IsActive());
            m_pOutput->CombineDataCapacity(static_cast<size_t>(CombineOutputCapacity));
        }
        CATCH_RETURN();
        return S_OK;
    }
    #if !defined(NDEBUG) || defined(DEVELOPMENT)
        STDMETHOD(SetSkipClose)(BOOL SkipClose) override
        {
            //TRACE(L"this 0x%p, SkipClose %d\n", this, SkipClose);
            try
            {
                CAutoLock lock(&m_csFilter);
                //THROW_HR_IF(VFW_E_WRONG_STATE, IsActive());
                m_SkipClose = static_cast<bool>(SkipClose);
            }
            CATCH_RETURN();
            return S_OK;
        }
    #endif
    
private:
    // construct only via class factory
    Mpeg4Mux(LPUNKNOWN pUnk, HRESULT* phr);
    ~Mpeg4Mux();

private:
    mutable CCritSec m_csFilter;
    std::string m_Comment;
    mutable CCritSec m_csTracks;
    MuxOutput* m_pOutput;
    vector<MuxInput*> m_pInputs;
    std::shared_ptr<MovieWriter> m_pMovie;

    // for reporting (via GetCurrentPosition) after completion
    REFERENCE_TIME m_tWritten;

    BOOL m_bAlignTrackStartTimeDisabled;
    REFERENCE_TIME m_nMinimalMovieDuration;

    bool m_TemporaryIndexFileEnabled = false;
    std::wstring m_TemporaryIndexFileDirectory;
    mutable CCritSec m_TemporaryIndexFileCriticalSection;
    CTemporaryIndexFile m_TemporaryIndexFile;

    #if !defined(NDEBUG) || defined(DEVELOPMENT)
        bool m_SkipClose = false;
    #endif
};

inline CBaseFilter* ToBaseFilter(Mpeg4Mux* Value)
{
    return static_cast<CBaseFilter*>(Value);
}
