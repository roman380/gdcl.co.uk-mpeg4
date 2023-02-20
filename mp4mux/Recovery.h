#pragma once

#include <filesystem>
#include <fstream>

#include "mp4mux_h.h"
#include "DebugTrace.h"
#include "..\test\SampleGrabberEx.h"

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
			Properties->cBuffers = 1;
			ALLOCATOR_PROPERTIES ActualProperties;
			WI_VERIFY_SUCCEEDED(MemAllocator->SetProperties(Properties, &ActualProperties));
			return S_OK;
		}
		HRESULT FillBuffer(IMediaSample* MediaSample) override
		{
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
						auto&& MediaSampleLock = m_MediaSampleMutex.lock_exclusive();
						if(m_MediaSampleList.empty())
						{
							std::this_thread::yield();
							continue;
						}
						MediaSample = m_MediaSampleList.front();
						m_MediaSampleList.pop_front();
					}

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
					Reply((DWORD) E_UNEXPECTED);
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
			auto&& MediaSampleLock = m_MediaSampleMutex.lock_exclusive();
			m_MediaSampleList.emplace_back(MediaSample);
		}
		void AddEndOfStream()
		{
			auto&& MediaSampleLock = m_MediaSampleMutex.lock_exclusive();
			m_MediaSampleList.emplace_back(nullptr);
		}

	private:
		CMediaType m_MediaType;
		mutable wil::srwlock m_MediaSampleMutex;
		std::list<wil::com_ptr<IMediaSample>> m_MediaSampleList;
	};

	SampleSource(IUnknown* Unknown, HRESULT* Result) :
		CSource("Source", Unknown, __uuidof(SampleSource))
	{
		m_Pin = new Stream(Result, this);
		WI_ASSERT(m_Pin);
	}
	~SampleSource()
	{
		delete m_Pin;
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
		WI_ASSERT(Index == 0);
		m_Pin->SetMediaType(std::move(MediaType));
	}
	void AddMediaSample(uint16_t Index, wil::com_ptr<IMediaSample> const& MediaSample)
	{
		WI_ASSERT(Index == 0 && MediaSample);
		m_Pin->AddMediaSample(MediaSample);
	}
	void AddEndOfStream()
	{
		m_Pin->AddEndOfStream();
	}

	Stream* m_Pin = nullptr;
};


class __declspec(uuid("{73D9D53D-30A3-451E-976A-2B4186FE27EC}")) MuxFilterRecovery : 
	public winrt::implements<MuxFilterRecovery, IMuxFilterRecovery>
{
public:

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
		bool Needed()
		{
			// NOTE: ISO/IEC 14496-12:2005; 4.2 Object Structure
			Stream.open(Path, std::ios_base::in | std::ios_base::binary); // https://learn.microsoft.com/en-us/cpp/standard-library/basic-istream-class
			Stream.seekg(0, std::ios_base::end);
			auto const FileSize = Stream.tellg();
			Stream.seekg(0, std::ios_base::beg);
			if(Stream.fail())
				return false;
			std::vector<std::pair<uint64_t, uint32_t>> BoxVector;
			for(; ; )
			{
				auto const BoxPosition = Stream.tellg();
				if(BoxPosition == FileSize)
					break;
				uint32_t const size = ReadNetwork<uint32_t>();
				if(Stream.fail())
					return false;
				uint32_t const type = ReadNetwork<uint32_t>();
				if(Stream.fail())
					return false;
				if(size == 1)
					return false; // Unsupported/unexpected 64-bit sizes
				if(size < 8)
					return false; // Unexpected open boxes and boxes smaller than minimally possible
				uint8_t usertype[16];
				Stream.read(reinterpret_cast<char*>(&usertype), sizeof usertype);
				if(Stream.fail())
					return false;
				TRACE(L"BoxPosition %llu, type %hs\n", static_cast<uint64_t>(BoxPosition), FormatFourCharacterCode(type).c_str());
				BoxVector.emplace_back(std::make_pair(BoxPosition, type));
				if(static_cast<uint64_t>(BoxPosition) + size > static_cast<uint64_t>(FileSize))
					return false; // Incomplete box
				Stream.seekg(static_cast<uint64_t>(BoxPosition) + size, std::ios_base::beg);
				if(Stream.fail())
					return false;
			}
			return std::find_if(BoxVector.cbegin(), BoxVector.cend(), [] (auto&& Pair) { return Pair.second == 'moov'; }) == BoxVector.cend();
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
		IFACEMETHOD(SetTime)(REFERENCE_TIME* pTimeStart, REFERENCE_TIME* pTimeEnd) override
		{
			return E_NOTIMPL;
		}
		IFACEMETHOD(IsSyncPoint)() override
		{
			return (m_Properties.dwSampleFlags & AM_SAMPLE_SPLICEPOINT) ? S_OK : S_FALSE;
		}
		IFACEMETHOD(SetSyncPoint)(BOOL bIsSyncPoint) override
		{
			return E_NOTIMPL;
		}
		IFACEMETHOD(IsPreroll)() override
		{
			return (m_Properties.dwSampleFlags & AM_SAMPLE_PREROLL) ? S_OK : S_FALSE;
		}
		IFACEMETHOD(SetPreroll)(BOOL bIsPreroll) override
		{
			return E_NOTIMPL;
		}
		IFACEMETHOD_(long, GetActualDataLength)() override
		{
			return m_Properties.lActual;
		}
		IFACEMETHOD(SetActualDataLength)(long ActualDataLength) override
		{
			return E_NOTIMPL;
		}
		IFACEMETHOD(GetMediaType)(AM_MEDIA_TYPE** MediaType) override
		{
			return E_NOTIMPL;
		}
		IFACEMETHOD(SetMediaType)(AM_MEDIA_TYPE* MediaType) override
		{
			return E_NOTIMPL;
		}
		IFACEMETHOD(IsDiscontinuity)() override
		{
			return (m_Properties.dwSampleFlags & AM_SAMPLE_DATADISCONTINUITY) ? S_OK : S_FALSE;
		}
		IFACEMETHOD(SetDiscontinuity)(BOOL bDiscontinuity) override
		{
			return E_NOTIMPL;
		}
		IFACEMETHOD(GetMediaTime)(LONGLONG* StartTime, LONGLONG* StopTime) override
		{
			WI_ASSERT(StartTime && StopTime);
			return VFW_E_MEDIA_TIME_NOT_SET;
		}
		IFACEMETHOD(SetMediaTime)(LONGLONG *pTimeStart, LONGLONG *pTimeEnd) override
		{
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
			return E_NOTIMPL;
		}

		AM_SAMPLE2_PROPERTIES m_Properties;
		std::vector<uint8_t> m_Data;
	};

	void Run()
	{
		winrt::init_apartment(winrt::apartment_type::single_threaded);
		if(m_Site)
			WI_VERIFY_SUCCEEDED(m_Site->AfterStart());
		try
		{
			wil::com_ptr<IFilterGraph2> FilterGraph2;
			HRESULT Result = S_OK;
			SampleSource* Source = static_cast<SampleSource*>(SampleSource::CreateInstance(nullptr, &Result));
			THROW_IF_FAILED(Result);
			wil::unique_event RunEvent { wil::EventOptions::ManualReset };
			auto const CreateFilters = [&]
			{
				FilterGraph2 = wil::CoCreateInstance<IFilterGraph2>(CLSID_FilterGraph, CLSCTX_INPROC_SERVER);
				wil::com_ptr<IPin> CurrectOutputPin;
				#pragma region Source
				{
					wil::com_ptr<IBaseFilter> const BaseFilter = Source;
					THROW_IF_FAILED(FilterGraph2->AddFilter(BaseFilter.get(), L"Source"));
					CurrectOutputPin = Pin(BaseFilter);
				}
				#pragma endregion
				#pragma region Multiplexer
				{
					wil::com_ptr<IBaseFilter> BaseFilter;
					{
						wil::com_ptr<IClassFactory> ClassFactory;
						THROW_IF_FAILED(DllGetClassObject(__uuidof(MuxFilter), IID_PPV_ARGS(ClassFactory.put())));
						ClassFactory->CreateInstance(nullptr, IID_PPV_ARGS(BaseFilter.put()));
					}
					THROW_IF_FAILED(FilterGraph2->AddFilter(BaseFilter.get(), L"Multiplexer"));
					THROW_IF_FAILED(FilterGraph2->Connect(CurrectOutputPin.get(), Pin(BaseFilter, PINDIR_INPUT).get()));
					CurrectOutputPin = Pin(BaseFilter, PINDIR_OUTPUT);
				}
				#pragma endregion
				#pragma region File Writer
				std::wstring const Path = L"C:\\Project\\github.com\\gdcl.co.uk-mpeg4\\bin\\x64\\Debug\\Recovery.Temporary-Output.mp4";
				{
					auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(CLSID_FileWriter, CLSCTX_INPROC_SERVER);
					auto const FileSinkFilter2 = BaseFilter.query<IFileSinkFilter2>();
					THROW_IF_FAILED(FileSinkFilter2->SetFileName(Path.c_str(), nullptr));
					THROW_IF_FAILED(FileSinkFilter2->SetMode(AM_FILE_OVERWRITE));
					THROW_IF_FAILED(FilterGraph2->AddFilter(BaseFilter.get(), L"Renderer"));
					THROW_IF_FAILED(FilterGraph2->Connect(CurrectOutputPin.get(), Pin(BaseFilter).get()));
					CurrectOutputPin.reset();
				}
				#pragma endregion
				THROW_IF_FAILED(FilterGraph2.query<IMediaFilter>()->SetSyncSource(nullptr));
			};
			std::thread Thread([&]
			{
				std::ifstream Stream;
				Stream.open(m_Path, std::ios_base::in | std::ios_base::binary);
				THROW_HR_IF(E_FAIL, Stream.fail());
				std::ifstream IndexStream;
				IndexStream.open(L"C:\\Project\\github.com\\gdcl.co.uk-mpeg4\\bin\\x64\\Debug\\TemporaryIndex\\Recovery.Temporary.mp4-Index.tmp", std::ios_base::in | std::ios_base::binary);
				THROW_HR_IF(E_FAIL, IndexStream.fail());
				try
				{
					IndexStream.seekg(0, std::ios_base::end);
					auto const IndexFileSize = IndexStream.tellg();
					IndexStream.seekg(0, std::ios_base::beg);
					THROW_HR_IF(E_FAIL, IndexStream.fail());
					bool Running = false;
					for(; ; )
					{
						auto const Position = IndexStream.tellg();
						if(Position == IndexFileSize)
							break;
						auto const Signature = Read<uint32_t>(IndexStream);
						TRACE(L"Position %llu, Signature %hs\n", static_cast<uint64_t>(Position), FormatFourCharacterCode(Signature).c_str());
						switch(Signature)
						{
						case MAKEFOURCC('M', 'P', '4', 'I'):
							{
								auto const Version = Read<uint32_t>(IndexStream);
								THROW_HR_IF(E_FAIL, Version != 1);
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
								WI_ASSERT(Index == 0);
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
								WI_ASSERT(MediaSample.Index == 0);
								Stream.seekg(MediaSample.Position, std::ios_base::beg);
								auto FilterSample = winrt::make_self<MuxFilterRecovery::MediaSample>();
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
								Source->AddMediaSample(MediaSample.Index, FilterSample.as<IMediaSample>().get());
								if(!std::exchange(Running, true))
								{
									CreateFilters();
									THROW_IF_FAILED(FilterGraph2.query<IMediaControl>()->Run());
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
				RunEvent.SetEvent();
			});
			WI_VERIFY(RunEvent.wait(INFINITE));
			THROW_HR_IF(E_FAIL, !FilterGraph2);
			LONG EventCode;
			THROW_IF_FAILED(FilterGraph2.query<IMediaEventEx>()->WaitForCompletion(INFINITE, &EventCode));
			WI_ASSERT(EventCode == EC_COMPLETE);
			Thread.join();
			m_Result = S_OK;
		}
		catch(...)
		{
			LOG_CAUGHT_EXCEPTION();
			m_Result = wil::ResultFromCaughtException();
		}
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
				m_Needed = Probe.Needed();
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
			[[maybe_unused]] auto&& DataLock = m_DataMutex.lock_exclusive();
			if(m_Active)
				return S_FALSE;
			WI_ASSERT(!m_Thread.joinable());
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
			[[maybe_unused]] auto&& DataLock = m_DataMutex.lock_exclusive();
			if(!m_Active)
				return S_FALSE;
			m_ThreadTermination.store(true);
			if(m_Thread.joinable())
				m_Thread.join();
			m_Active = false;
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
	bool m_Active = false;
	std::atomic_bool m_ThreadTermination;
	std::thread m_Thread;
	std::optional<HRESULT> m_Result;
};
