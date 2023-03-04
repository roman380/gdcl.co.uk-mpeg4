#pragma once

#include <filesystem>
#include <fstream>

#include <shlwapi.h>

#include "mp4mux_h.h"
#include "DebugTrace.h"
#include "..\test\SampleGrabberEx.h"

class MediaSample : public winrt::implements<MediaSample, IMediaSample, IMediaSample2>
{
public:
    MediaSample() = default;

// IMediaSample
    IFACEMETHOD(GetPointer)(BYTE** ppBuffer) override
    {
        WI_ASSERT(ppBuffer);
        *ppBuffer = m_Properties.pbBuffer;
        return S_OK;
    }
    IFACEMETHOD_(long, GetSize)() override
    {
        return m_Properties.cbBuffer;
    }
    IFACEMETHOD(GetTime)(REFERENCE_TIME* StartTime, REFERENCE_TIME* StopTime) override
    {
        WI_ASSERT(StartTime && StopTime);
        if(!(m_Properties.dwSampleFlags & AM_SAMPLE_TIMEVALID))
            return VFW_E_SAMPLE_TIME_NOT_SET;
        *StartTime = m_Properties.tStart;
        if(!(m_Properties.dwSampleFlags & AM_SAMPLE_STOPVALID))
            return VFW_S_NO_STOP_TIME;
        *StopTime = m_Properties.tStop;
        return S_OK;
    }
    IFACEMETHOD(SetTime)(REFERENCE_TIME* StartTime, REFERENCE_TIME* StopTime) override
    {
        StartTime; StopTime;
        return E_NOTIMPL;
    }
    IFACEMETHOD(IsSyncPoint)() override
    {
        return (m_Properties.dwSampleFlags & AM_SAMPLE_SPLICEPOINT) ? S_OK : S_FALSE;
    }
    IFACEMETHOD(SetSyncPoint)(BOOL SyncPoint) override
    {
        SyncPoint;
        return E_NOTIMPL;
    }
    IFACEMETHOD(IsPreroll)() override
    {
        return (m_Properties.dwSampleFlags & AM_SAMPLE_PREROLL) ? S_OK : S_FALSE;
    }
    IFACEMETHOD(SetPreroll)(BOOL Preroll) override
    {
        Preroll;
        return E_NOTIMPL;
    }
    IFACEMETHOD_(long, GetActualDataLength)() override
    {
        return m_Properties.lActual;
    }
    IFACEMETHOD(SetActualDataLength)(long ActualDataLength) override
    {
        ActualDataLength;
        return E_NOTIMPL;
    }
    IFACEMETHOD(GetMediaType)(AM_MEDIA_TYPE** MediaType) override
    {
        MediaType;
        return E_NOTIMPL;
    }
    IFACEMETHOD(SetMediaType)(AM_MEDIA_TYPE* MediaType) override
    {
        MediaType;
        return E_NOTIMPL;
    }
    IFACEMETHOD(IsDiscontinuity)() override
    {
        return (m_Properties.dwSampleFlags & AM_SAMPLE_DATADISCONTINUITY) ? S_OK : S_FALSE;
    }
    IFACEMETHOD(SetDiscontinuity)(BOOL Discontinuity) override
    {
        Discontinuity;
        return E_NOTIMPL;
    }
    IFACEMETHOD(GetMediaTime)(LONGLONG* StartTime, LONGLONG* StopTime) override
    {
        WI_ASSERT(StartTime && StopTime); StartTime; StopTime;
        return VFW_E_MEDIA_TIME_NOT_SET;
    }
    IFACEMETHOD(SetMediaTime)(LONGLONG* StartTime, LONGLONG* StopTime) override
    {
        StartTime; StopTime;
        return E_NOTIMPL;
    }

// IMediaSample2
    IFACEMETHOD(GetProperties)(DWORD PropertiesSize, BYTE* Properties) override
    {
        WI_ASSERT(PropertiesSize == sizeof m_Properties && Properties);
        *reinterpret_cast<AM_SAMPLE2_PROPERTIES*>(Properties) = m_Properties;
        return S_OK;
    }
    IFACEMETHOD(SetProperties)(DWORD PropertiesSize, BYTE const* Properties) override
    {
        PropertiesSize; Properties;
        return E_NOTIMPL;
    }

    AM_SAMPLE2_PROPERTIES m_Properties;
    std::vector<uint8_t> m_Data;
};

class __declspec(uuid("{9869C322-03DB-4A9B-A683-F6F6D732824C}")) SampleSource : public CSource
{
public:

    class Stream : public CSourceStream
    {
    public:
        Stream(HRESULT* Result, CSource* Filter) :
            CSourceStream(L"", Result, Filter, L"Out")
        {
        }

        HRESULT GetMediaType(CMediaType* MediaType) override
        {
            WI_ASSERT(MediaType);
            *MediaType = m_MediaType;
            return S_OK;
        }
        HRESULT DecideBufferSize(IMemAllocator* MemAllocator, ALLOCATOR_PROPERTIES* Properties) override
        {
            WI_ASSERT(MemAllocator && Properties);
            Properties->cbBuffer = 1;
            Properties->cBuffers = 64; // Needs to be over 20, see MuxInput::NotifyAllocator
            ALLOCATOR_PROPERTIES ActualProperties;
            WI_VERIFY_SUCCEEDED(MemAllocator->SetProperties(Properties, &ActualProperties));
            return S_OK;
        }
        HRESULT FillBuffer(IMediaSample* MediaSample) override
        {
            MediaSample;
            return E_NOTIMPL;
        }
        HRESULT DoBufferProcessingLoop() override
        {
            OnThreadStartPlay();
            for(; ; )
            {
                Command com;
                while(!CheckRequest(&com)) 
                {
                    wil::com_ptr<IMediaSample> MediaSample;
                    {
                        [[maybe_unused]] auto&& MediaSampleLock = m_MediaSampleMutex.lock_exclusive();
                        if(m_MediaSampleList.empty())
                        {
                            std::this_thread::yield();
                            continue;
                        }
                        MediaSample = m_MediaSampleList.front();
                        m_MediaSampleList.pop_front();
                    }

                    //TRACE(L"MediaSample 0x%p\n", MediaSample.get());
                    if(!MediaSample) 
                    {
                        DeliverEndOfStream();
                        return S_OK;
                    }
                    auto const Result = Deliver(MediaSample.get());
                    if(Result != S_OK)
                    {
                        // NotifyErrorAbort?
                        return S_OK;
                    }
                }
                if(com == CMD_RUN || com == CMD_PAUSE)
                {
                    Reply(NOERROR);
                } else
                if(com != CMD_STOP) 
                {
                    Reply(static_cast<DWORD>(E_UNEXPECTED));
                    DbgLog((LOG_ERROR, 1, TEXT("Unexpected command!!!")));
                }
                if(com == CMD_STOP)
                    break;
            }
            return S_FALSE;
        }

        void SetMediaType(CMediaType&& MediaType)
        {
            m_MediaType = std::move(MediaType);
        }
        void AddMediaSample(wil::com_ptr<IMediaSample> const& MediaSample)
        {
            WI_ASSERT(MediaSample);
            [[maybe_unused]] auto&& MediaSampleLock = m_MediaSampleMutex.lock_exclusive();
            WI_ASSERT(m_MediaType.IsValid());
            if(m_MediaType.subtype == MEDIASUBTYPE_H264)
            {
                // NOTE: H264ByteStreamHandler converts byte stream format to length prefixed NAL units written into resulting file, hence
                //       byte stream media type corresponds to length prefixed content in the incomplete file.
                auto const MediaSampleA = static_cast<::MediaSample*>(MediaSample.query<IMediaSample2>().get());
                auto const MediaSampleB = winrt::make_self<::MediaSample>();
                MediaSampleB->m_Properties = MediaSampleA->m_Properties;
                MediaSampleB->m_Data.reserve(MediaSampleA->m_Data.size());
                uint8_t const* I = MediaSampleA->m_Data.data();
                uint8_t const* IB = I + MediaSampleA->m_Properties.lActual;
                for(; ; )
                {
                    if(IB - I < 4)
                        break;
                    auto const S = _byteswap_ulong(*reinterpret_cast<unsigned long const*>(I));
                    I += 4;
                    if(IB - I < static_cast<ptrdiff_t>(S))
                        break;
                    static uint8_t constexpr const g_LongStartCodePrefix[] { 0x00, 0x00, 0x00, 0x01 };
                    static uint8_t constexpr const g_ShortStartCodePrefix[] { 0x00, 0x00, 0x01 };
                    auto const T = *I & 0x1F;
                    if(T == 7 || T == 8)
                        std::copy(std::cbegin(g_LongStartCodePrefix), std::cend(g_LongStartCodePrefix), std::back_inserter(MediaSampleB->m_Data));
                    else
                        std::copy(std::cbegin(g_ShortStartCodePrefix), std::cend(g_ShortStartCodePrefix), std::back_inserter(MediaSampleB->m_Data));
                    std::copy(I, I + S, std::back_inserter(MediaSampleB->m_Data));
                    I += S;
                }
                WI_ASSERT(I == IB);
                MediaSampleB->m_Properties.pbBuffer = MediaSampleB->m_Data.data();
                MediaSampleB->m_Properties.cbBuffer = static_cast<long>(MediaSampleB->m_Data.size());
                MediaSampleB->m_Properties.lActual = MediaSampleB->m_Properties.cbBuffer;
                m_MediaSampleList.emplace_back(static_cast<IMediaSample2*>(MediaSampleB.get()));
                return;
            }
            m_MediaSampleList.emplace_back(MediaSample);
        }
        void AddEndOfStream()
        {
            [[maybe_unused]] auto&& MediaSampleLock = m_MediaSampleMutex.lock_exclusive();
            m_MediaSampleList.emplace_back(nullptr);
        }

    private:
        CMediaType m_MediaType;
        mutable wil::srwlock m_MediaSampleMutex;
        std::list<wil::com_ptr<IMediaSample>> m_MediaSampleList;
    };

    SampleSource(IUnknown* Unknown, HRESULT*) :
        CSource("Source", Unknown, __uuidof(SampleSource))
    {
    }
    ~SampleSource()
    {
        for(auto&& Pin: m_PinVector)
            delete Pin;
    }

    DECLARE_IUNKNOWN;

    static CUnknown* WINAPI CreateInstance(IUnknown* Unknown, HRESULT* Result)
    {
        WI_ASSERT(Result);
        auto Instance = new SampleSource(Unknown, Result);
        WI_ASSERT(Instance);
        WI_ASSERT(SUCCEEDED(*Result));
        return Instance;
    }

    void CreatePin(uint16_t Index, CMediaType&& MediaType)
    {
        m_PinVector.resize(std::max<size_t>(m_PinVector.size(), Index + 1));
        auto*& Pin = m_PinVector[Index];
        WI_ASSERT(!Pin);
        HRESULT Result = S_OK;
        Pin = new Stream(&Result, this);
        THROW_IF_FAILED(Result);
        Pin->SetMediaType(std::move(MediaType));
    }
    void AddMediaSample(uint16_t Index, wil::com_ptr<IMediaSample> const& MediaSample)
    {
        WI_ASSERT(MediaSample);
        WI_ASSERT(Index < m_PinVector.size());
        WI_ASSERT(m_PinVector[Index]);
        m_PinVector[Index]->AddMediaSample(MediaSample);
    }
    void AddEndOfStream()
    {
        for(auto&& Pin: m_PinVector)
            Pin->AddEndOfStream();
    }

    std::vector<Stream*> m_PinVector;
};

class __declspec(uuid("{73D9D53D-30A3-451E-976A-2B4186FE27EC}")) MuxFilterRecovery : 
    public winrt::implements<MuxFilterRecovery, IMuxFilterRecovery>
{
public:
    MuxFilterRecovery() = default;
    ~MuxFilterRecovery()
    {
        WI_ASSERT(!m_Thread.joinable());
    }

    static std::string FormatFourCharacterCode(uint32_t Value)
    {
        char Text[5] 
        { 
            static_cast<char>(Value >> 24),
            static_cast<char>(Value >> 16),
            static_cast<char>(Value >>  8),
            static_cast<char>(Value      ),
            0
        };
        return Text;
    }

    struct Probe
    {
        Probe(std::wstring const& Path) :
            Path(Path)
        {
        }

        static uint16_t FromNetwork(uint16_t Value)
        {
            return (Value >> 8) | (Value << 8);
        }
        static uint32_t FromNetwork(uint32_t Value)
        {
            return (static_cast<uint32_t>(FromNetwork(static_cast<uint16_t>(Value))) << 16) | FromNetwork(static_cast<uint16_t>(Value >> 16));
        }
        static uint64_t FromNetwork(uint64_t Value)
        {
            return (static_cast<uint64_t>(FromNetwork(static_cast<uint32_t>(Value))) << 32) | FromNetwork(static_cast<uint32_t>(Value >> 32));
        }
        template <typename ValueType>
        ValueType ReadNetwork()
        {
            ValueType Value;
            Stream.read(reinterpret_cast<char*>(&Value), sizeof Value);
            return FromNetwork(Value);
        }
        unsigned int Needed()
        {
            // NOTE: ISO/IEC 14496-12:2005; 4.2 Object Structure
            Stream.open(Path, std::ios_base::in | std::ios_base::binary); // https://learn.microsoft.com/en-us/cpp/standard-library/basic-istream-class
            Stream.seekg(0, std::ios_base::end);
            auto const FileSize = Stream.tellg();
            Stream.seekg(0, std::ios_base::beg);
            if(Stream.fail())
                return 2;
            std::vector<std::pair<uint64_t, uint32_t>> BoxVector;
            for(; ; )
            {
                auto const BoxPosition = Stream.tellg();
                if(BoxPosition + static_cast<std::streampos>(sizeof (uint32_t) * 2) >= FileSize)
                    break;
                uint32_t const size = ReadNetwork<uint32_t>();
                if(Stream.fail())
                    return 2;
                uint32_t const type = ReadNetwork<uint32_t>();
                if(Stream.fail())
                    return 2;
                if(size == 1)
                    return 2; // Unsupported/unexpected 64-bit sizes
                if(size < 8)
                    return 2; // Unexpected open boxes and boxes smaller than minimally possible
                if(size == 8 && type == 'mdat')
                    return 1; // mdat is intentionally open because it is incomplete
                uint8_t usertype[16];
                Stream.read(reinterpret_cast<char*>(&usertype), sizeof usertype);
                if(Stream.fail())
                    return 2;
                TRACE(L"BoxPosition %llu, type %hs\n", static_cast<uint64_t>(BoxPosition), FormatFourCharacterCode(type).c_str());
                BoxVector.emplace_back(std::make_pair(BoxPosition, type));
                if(static_cast<uint64_t>(BoxPosition) + size > static_cast<uint64_t>(FileSize))
                    return 2; // Incomplete box
                Stream.seekg(static_cast<uint64_t>(BoxPosition) + size, std::ios_base::beg);
                if(Stream.fail())
                    return 2;
            }
            return std::find_if(BoxVector.cbegin(), BoxVector.cend(), [] (auto&& Pair) { return Pair.second == 'moov'; }) == BoxVector.cend() ? 1 : 0;
        }

        std::wstring const Path;
        std::ifstream Stream;
    };

    template <typename ValueType>
    static ValueType Read(std::ifstream& Stream)
    {
        ValueType Value;
        Stream.read(reinterpret_cast<char*>(&Value), sizeof Value);
        THROW_HR_IF(E_FAIL, Stream.fail());
        return Value;
    }

    void Run()
    {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        if(m_Site)
            WI_VERIFY_SUCCEEDED(m_Site->AfterStart());
        wchar_t IndexPath[MAX_PATH] { };
        wchar_t TemporaryPath[MAX_PATH] { };
        bool ReplaceComplete = false;
        try
        {
            WI_VERIFY(PathCombineW(IndexPath, m_TemporaryIndexFileDirectory.c_str(), PathFindFileNameW(m_Path.c_str())));
            wcscat_s(IndexPath, L"-Index.tmp");
            wchar_t Directory[MAX_PATH];
            wcscpy_s(Directory, m_Path.c_str());
            std::wstring const Extension = PathFindExtensionW(Directory);
            PathRemoveExtensionW(Directory);
            std::wstring const Name = PathFindFileNameW(Directory);
            WI_VERIFY(PathRemoveFileSpecW(Directory));
            WI_VERIFY(PathCombineW(TemporaryPath, Directory, Name.c_str()));
            wcscat_s(TemporaryPath, L"-Temporary");
            wcscat_s(TemporaryPath, Extension.c_str());

            THROW_HR_IF_MSG(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), !PathFileExistsW(IndexPath), "Index file required for recovery was not found, %ls", IndexPath);
            wil::com_ptr<IFilterGraph2> FilterGraph2;
            HRESULT Result = S_OK;
            SampleSource* Source = static_cast<SampleSource*>(SampleSource::CreateInstance(nullptr, &Result));
            THROW_IF_FAILED(Result);
            wil::unique_event RunEvent { wil::EventOptions::ManualReset };
            auto const CreateFilters = [&]
            {
                FilterGraph2 = wil::CoCreateInstance<IFilterGraph2>(CLSID_FilterGraph, CLSCTX_INPROC_SERVER);
                #pragma region Source
                wil::com_ptr<IBaseFilter> const SourceBaseFilter = Source;
                THROW_IF_FAILED(FilterGraph2->AddFilter(SourceBaseFilter.get(), L"Source"));
                #pragma endregion
                #pragma region Multiplexer
                wil::com_ptr<IPin> CurrectOutputPin;
                {
                    wil::com_ptr<IBaseFilter> BaseFilter;
                    {
                        wil::com_ptr<IClassFactory> ClassFactory;
                        THROW_IF_FAILED(DllGetClassObject(__uuidof(MuxFilter), IID_PPV_ARGS(ClassFactory.put())));
                        ClassFactory->CreateInstance(nullptr, IID_PPV_ARGS(BaseFilter.put()));
                    }
                    THROW_IF_FAILED(FilterGraph2->AddFilter(BaseFilter.get(), L"Multiplexer"));
                    unsigned int Index = 0;
                    EnumeratePins(SourceBaseFilter, [&] (auto const& OutputPin) 
                    { 
                        THROW_IF_FAILED(FilterGraph2->Connect(OutputPin.get(), Pin(BaseFilter, PINDIR_INPUT, Index++).get())); 
                        return false; 
                    });
                    WI_ASSERT(Index > 0);
                    CurrectOutputPin = Pin(BaseFilter, PINDIR_OUTPUT);
                }
                #pragma endregion
                #pragma region File Writer
                {
                    auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(CLSID_FileWriter, CLSCTX_INPROC_SERVER);
                    auto const FileSinkFilter2 = BaseFilter.query<IFileSinkFilter2>();
                    THROW_IF_FAILED(FileSinkFilter2->SetFileName(TemporaryPath, nullptr));
                    THROW_IF_FAILED(FileSinkFilter2->SetMode(AM_FILE_OVERWRITE));
                    THROW_IF_FAILED(FilterGraph2->AddFilter(BaseFilter.get(), L"Renderer"));
                    THROW_IF_FAILED(FilterGraph2->Connect(CurrectOutputPin.get(), Pin(BaseFilter).get()));
                    CurrectOutputPin.reset();
                }
                #pragma endregion
                THROW_IF_FAILED(FilterGraph2.query<IMediaFilter>()->SetSyncSource(nullptr));
            };
            bool ThreadAbort = false;
            std::thread Thread([&]
            {
                std::ifstream Stream;
                Stream.open(m_Path, std::ios_base::in | std::ios_base::binary);
                THROW_HR_IF_MSG(E_FAIL, Stream.fail(), "Failed to open input file, %ls", m_Path.c_str());
                std::ifstream IndexStream;
                IndexStream.open(IndexPath, std::ios_base::in | std::ios_base::binary);
                THROW_HR_IF_MSG(E_FAIL, IndexStream.fail(), "Failed to open input index file, %ls", IndexPath);
                try
                {
                    IndexStream.seekg(0, std::ios_base::end);
                    auto const IndexFileSize = IndexStream.tellg();
                    IndexStream.seekg(0, std::ios_base::beg);
                    THROW_HR_IF(E_FAIL, IndexStream.fail());
                    auto ReportTime = std::chrono::system_clock::now();
                    static auto constexpr const g_ReportPeriodTime = 1s;
                    bool Running = false;
                    for(unsigned long RecordIndex = 0; ; RecordIndex++)
                    {
                        auto const Position = IndexStream.tellg();
                        if(Position == IndexFileSize)
                            break;
                        if(m_ThreadTermination)
                            break;
                        if(m_Site && RecordIndex % 64)
                        {
                            auto const Time = std::chrono::system_clock::now();
                            if(Time - ReportTime >= g_ReportPeriodTime)
                            {
                                auto const Progress = static_cast<double>(Position) / IndexFileSize;
                                {
                                    [[maybe_unused]] auto&& DataLock = m_DataMutex.lock_exclusive();
                                    m_Progress = Progress;
                                }
                                WI_VERIFY_SUCCEEDED(m_Site->Progress(Progress));
                                ReportTime = Time;
                            }
                        }
                        auto const Signature = Read<uint32_t>(IndexStream);
                        //TRACE(L"Position %llu, Signature %hs\n", static_cast<uint64_t>(Position), FormatFourCharacterCode(Signature).c_str());
                        switch(Signature)
                        {
                        case MAKEFOURCC('M', 'P', '4', 'I'):
                            {
                                auto const Version = Read<uint32_t>(IndexStream);
                                THROW_HR_IF_MSG(E_FAIL, Version != 1, "Unexpected file version, %u", Version);
                                TRACE(L"Position %llu, Signature %hs, Version %u\n", static_cast<uint64_t>(Position), FormatFourCharacterCode(Signature).c_str(), Version);
                            }
                            break;
                        case MAKEFOURCC('I', 'P', 'I', 'N'):
                            {
                                auto const Index = Read<uint16_t>(IndexStream);
                                auto const MediaType = Read<AM_MEDIA_TYPE>(IndexStream);
                                CMediaType MediaTypeEx;
                                MediaTypeEx.majortype = MediaType.majortype;
                                MediaTypeEx.subtype = MediaType.subtype;
                                MediaTypeEx.bFixedSizeSamples = MediaType.bFixedSizeSamples;
                                MediaTypeEx.bTemporalCompression = MediaType.bTemporalCompression;
                                MediaTypeEx.lSampleSize = MediaType.lSampleSize;
                                MediaTypeEx.formattype = MediaType.formattype;
                                WI_ASSERT(!MediaType.pUnk);
                                if(MediaType.cbFormat)
                                {
                                    IndexStream.read(reinterpret_cast<char*>(MediaTypeEx.AllocFormatBuffer(MediaType.cbFormat)), MediaType.cbFormat);
                                    THROW_HR_IF(E_FAIL, IndexStream.fail());
                                }
                                TRACE(L"Position %llu, Signature %hs, Index %u\n", static_cast<uint64_t>(Position), FormatFourCharacterCode(Signature).c_str(), Index);
                                Source->CreatePin(Index, std::move(MediaTypeEx));
                            }
                            break;
                        case MAKEFOURCC('S', 'A', 'M', 'P'):
                            {
                                #pragma region Structure
                                #pragma pack(push, 1)
                                struct MEDIASAMPLE
                                {
                                    //uint32_t Signature;
                                    uint16_t Index;
                                    uint64_t Position;
                                    uint32_t SampleFlags;
                                    uint32_t Size;
                                    uint64_t StartTime;
                                    uint32_t LengthTime;
                                };
                                #pragma pack(pop)
                                #pragma endregion
                                MEDIASAMPLE MediaSample;
                                IndexStream.read(reinterpret_cast<char*>(&MediaSample), sizeof MediaSample);
                                THROW_HR_IF(E_FAIL, IndexStream.fail());
                                Stream.seekg(MediaSample.Position, std::ios_base::beg);
                                auto FilterSample = winrt::make_self<::MediaSample>();
                                auto& Properties = FilterSample->m_Properties;
                                FilterSample->m_Data.resize(MediaSample.Size);
                                Stream.read(reinterpret_cast<char*>(FilterSample->m_Data.data()), MediaSample.Size);
                                THROW_HR_IF(E_FAIL, Stream.fail());
                                Properties.cbData = sizeof Properties;
                                Properties.dwTypeSpecificFlags = 0;
                                Properties.dwSampleFlags = MediaSample.SampleFlags;
                                Properties.lActual = static_cast<LONG>(MediaSample.Size);
                                Properties.tStart = MediaSample.StartTime;
                                Properties.tStop = MediaSample.StartTime + MediaSample.LengthTime;
                                Properties.dwStreamId = 0;
                                Properties.pMediaType = nullptr;
                                Properties.pbBuffer = FilterSample->m_Data.data();
                                Properties.cbBuffer = static_cast<LONG>(FilterSample->m_Data.size());
                                TRACE(L"Position %llu, Signature %hs, MediaSample.Index %u, .Position %llu, .Size %u\n", static_cast<uint64_t>(Position), FormatFourCharacterCode(Signature).c_str(), MediaSample.Index, MediaSample.Position, MediaSample.Size);
                                Source->AddMediaSample(MediaSample.Index, FilterSample.as<IMediaSample>().get());
                                if(!std::exchange(Running, true))
                                {
                                    CreateFilters();
                                    THROW_IF_FAILED_MSG(FilterGraph2.query<IMediaControl>()->Run(), "Failed to run recovery pipeline");
                                    RunEvent.SetEvent();
                                }
                            }
                            break;
                        default:
                            THROW_HR_MSG(E_FAIL, "Unexpected signature %hs at position %llu while reading index file", FormatFourCharacterCode(Signature).c_str(), static_cast<uint64_t>(Position));
                        }
                    }
                }
                CATCH_LOG_MSG("Failure while reading from broken and/or index file");
                Source->AddEndOfStream();
                ThreadAbort = m_ThreadTermination;
                RunEvent.SetEvent();
            });
            WI_VERIFY(RunEvent.wait(INFINITE));
            THROW_HR_IF(E_FAIL, !FilterGraph2);
            LONG EventCode;
            THROW_IF_FAILED(FilterGraph2.query<IMediaEventEx>()->WaitForCompletion(INFINITE, &EventCode));
            WI_ASSERT(EventCode == EC_COMPLETE);
            Thread.join();
            FilterGraph2.reset();
            if(!ThreadAbort)
            {
                THROW_IF_WIN32_BOOL_FALSE_MSG(MoveFileExW(TemporaryPath, m_Path.c_str(), MOVEFILE_REPLACE_EXISTING), "Failed to move the recovered file in place of original file");
                ReplaceComplete = true;
                LOG_IF_WIN32_BOOL_FALSE_MSG(DeleteFileW(IndexPath), "Failed to delete index file, %ls", IndexPath);
                m_Result = S_OK;
            } else
            {
                if(PathFileExistsW(TemporaryPath))
                    LOG_IF_WIN32_BOOL_FALSE_MSG(DeleteFileW(TemporaryPath), "Failed to delete incomplete recovery file, %ls", TemporaryPath);
                m_Result = S_FALSE;
            }
        }
        catch(...)
        {
            LOG_CAUGHT_EXCEPTION();
            m_Result = wil::ResultFromCaughtException();
            if(!ReplaceComplete && PathFileExistsW(TemporaryPath))
                LOG_IF_WIN32_BOOL_FALSE_MSG(DeleteFileW(TemporaryPath), "Failed to delete incomplete recovery file, %ls", TemporaryPath);
        }
        m_Active = false;
        if(m_Site)
            WI_VERIFY_SUCCEEDED(m_Site->BeforeStop());
        winrt::uninit_apartment();
    }
    
// IMuxFilterRecovery
    IFACEMETHOD(Initialize)(IMuxFilterRecoverySite* Site, BSTR Path, BSTR TemporaryIndexFileDirectory) override
    {
        TRACE(L"this 0x%p\n", this);
        try
        {
            THROW_HR_IF_NULL(E_INVALIDARG, Path);
            [[maybe_unused]] auto&& DataLock = m_DataMutex.lock_exclusive();
            m_Site = Site;
            m_Path = Path;
            m_TemporaryIndexFileDirectory = TemporaryIndexFileDirectory;
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(Needed)(BOOL* Needed) override
    {
        TRACE(L"this 0x%p\n", this);
        try
        {
            THROW_HR_IF_NULL(E_POINTER, Needed);
            [[maybe_unused]] auto&& DataLock = m_DataMutex.lock_exclusive();
            if(!m_Needed.has_value())
            {
                Probe Probe(m_Path);
                m_Needed = Probe.Needed() != 0;
            }
            *Needed = m_Needed.value() ? 1 : 0;
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(Active)(BOOL* Active) override
    {
        TRACE(L"this 0x%p\n", this);
        try
        {
            THROW_HR_IF_NULL(E_POINTER, Active);
            [[maybe_unused]] auto&& DataLock = m_DataMutex.lock_shared();
            *Active = m_Active ? 1 : 0;
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(Start)() override
    {
        TRACE(L"this 0x%p\n", this);
        try
        {
            [[maybe_unused]] auto&& ThreadLock = m_ThreadMutex.lock_exclusive();
            if(m_Active)
                return S_FALSE;
            WI_ASSERT(!m_Thread.joinable());
            {
                [[maybe_unused]] auto&& DataLock = m_DataMutex.lock_exclusive();
                m_Progress = 0;
            }
            m_ThreadTermination.store(false);
            m_Thread = std::move(std::thread([&] { Run(); }));
            m_Active = true;
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(Stop)() override
    {
        TRACE(L"this 0x%p\n", this);
        try
        {
            [[maybe_unused]] auto&& ThreadLock = m_ThreadMutex.lock_exclusive();
            m_ThreadTermination.store(true);
            if(m_Thread.joinable())
                m_Thread.join();
            m_Active = false;
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(Progress)(DOUBLE* Progress) override
    {
        //TRACE(L"this 0x%p\n", this);
        try
        {
            THROW_HR_IF_NULL(E_POINTER, Progress);
            [[maybe_unused]] auto&& DataLock = m_DataMutex.lock_shared();
            *Progress = m_Progress;
        }
        CATCH_RETURN();
        return S_OK;
    }

private:
    mutable wil::srwlock m_DataMutex;
    wil::com_ptr<IMuxFilterRecoverySite> m_Site;
    std::wstring m_Path;
    std::wstring m_TemporaryIndexFileDirectory;
    std::optional<bool> m_Needed;
    std::atomic_bool m_Active = false;
    double m_Progress;
    mutable wil::srwlock m_ThreadMutex;
    std::atomic_bool m_ThreadTermination;
    std::thread m_Thread;
    std::optional<HRESULT> m_Result;
};
