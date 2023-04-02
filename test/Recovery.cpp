#include "pch.h"
#include "Common.h"

#include "..\mp4mux\mp4mux_h.h"
#include "..\mp4mux\mp4mux_i.c"
#include "..\mp4demux\mp4demux_h.h"
#include "..\mp4demux\mp4demux_i.c"
#include "..\mp4mux\DebugTrace.h"

#if defined(WITH_DIRECTSHOWREFERENCESOURCE)
	using namespace AlaxInfoDirectShowReferenceSource;
#endif

namespace Test
{
	TEST_CLASS(Recovery)
	{
	public:
		TEST_CLASS_INITIALIZE(Initialize)
		{
			winrt::init_apartment(winrt::apartment_type::single_threaded); // Follows Unit Test Framework initialization
		}
		TEST_CLASS_CLEANUP(Cleanup)
		{
			winrt::uninit_apartment();
		}

		class RecoverySite : public winrt::implements<RecoverySite, IMuxFilterRecoverySite>
		{
		public:

		// IMuxFilterRecoverySite
			IFACEMETHOD(AfterStart)() override
			{
				TRACE(L"this 0x%p\n", this);
				return S_OK;
			}
			IFACEMETHOD(BeforeStop)() override
			{
				TRACE(L"this 0x%p\n", this);
				StoppingCondition.notify_all();
				return S_OK;
			}
			IFACEMETHOD(Progress)([[maybe_unused]] DOUBLE Progress) override
			{
				TRACE(L"this 0x%p, Progress %.03f\n", this, Progress);
				return S_OK;
			}

			wil::srwlock Activity;
			wil::condition_variable StoppingCondition;
		};

		#if defined(WITH_DIRECTSHOWREFERENCESOURCE) && !defined(NDEBUG)

		// WARN: Release builds don't offer SetSkipClose and friends

		void InternalRecovery(std::wstring const BaseName, std::function<void(wil::com_ptr<IFilterGraph2> const&, wil::com_ptr<IBaseFilter> const&)> AddSourceFilters)
		{
			auto const Path = OutputPath(Format(L"Recovery.%ls.mp4", BaseName.c_str()));
			if(PathFileExistsW(Path.c_str()))
				WI_VERIFY(DeleteFileW(Path.c_str()));
			auto const TemporaryIndexFileDirectory = OutputPath(L"TemporaryIndex");
			CreateDirectoryW(TemporaryIndexFileDirectory.c_str(), nullptr);
			Library Library(L"mp4mux.dll"), DemultiplexerLibrary(L"mp4demux.dll");
			{
				auto const FilterGraph2 = wil::CoCreateInstance<IFilterGraph2>(CLSID_FilterGraph, CLSCTX_INPROC_SERVER);
				wil::com_ptr<IPin> CurrentOutputPin;
				#pragma region Multiplexer
				{
					auto const Filter = Library.CreateInstance<MuxFilter, IMuxFilter>();
					THROW_IF_FAILED(Filter->SetTemporaryIndexFileEnabled(TRUE));
					THROW_IF_FAILED(Filter->SetTemporaryIndexFileDirectory(const_cast<BSTR>(TemporaryIndexFileDirectory.c_str())));
					THROW_IF_FAILED(Filter->SetSkipClose(TRUE)); // Stop will skip write of moof atom and leave the produced MP4 unplayable
					auto const BaseFilter = Filter.query<IBaseFilter>();
					AddFilter(FilterGraph2, BaseFilter, L"Multiplexer");
					AddSourceFilters(FilterGraph2, BaseFilter);
					CurrentOutputPin = Pin(BaseFilter, PINDIR_OUTPUT);
				}
				#pragma endregion
				#pragma region File Writer
				{
					auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(CLSID_FileWriter, CLSCTX_INPROC_SERVER);
					auto const FileSinkFilter2 = BaseFilter.query<IFileSinkFilter2>();
					THROW_IF_FAILED(FileSinkFilter2->SetFileName(Path.c_str(), nullptr));
					THROW_IF_FAILED(FileSinkFilter2->SetMode(AM_FILE_OVERWRITE));
					AddFilter(FilterGraph2, BaseFilter, L"Renderer");
					THROW_IF_FAILED(FilterGraph2->Connect(CurrentOutputPin.get(), Pin(BaseFilter).get()));
					CurrentOutputPin.reset();
				}
				#pragma endregion
				RunFilterGraph(FilterGraph2, 3s);
			}
			Assert::IsTrue(PathFileExistsW(Path.c_str()));
			Assert::IsTrue(PathFileExistsW(OutputPath(Format(L"TemporaryIndex\\Recovery.%ls.mp4-Index.tmp", BaseName.c_str())).c_str()));
			LOG_IF_WIN32_BOOL_FALSE(CopyFileW(Path.c_str(), OutputPath(Format(L"Recovery.%ls-Source.mp4", BaseName.c_str())).c_str(), FALSE));
			// NOTE: File is unusable at this point as SetSkipClose above instructed to skip finalization
			{
				auto const Recovery = Library.CreateInstance<MuxFilterRecovery, IMuxFilterRecovery>();
				auto const Site = winrt::make_self<RecoverySite>();
				THROW_IF_FAILED(Recovery->Initialize(Site.get(), const_cast<BSTR>(Path.c_str()), const_cast<BSTR>(TemporaryIndexFileDirectory.c_str())));
				BOOL Needed;
				THROW_IF_FAILED(Recovery->Needed(&Needed));
				Assert::IsTrue(Needed);
				THROW_IF_FAILED(Recovery->Start());
				{
					auto ActivityLock = Site->Activity.lock_exclusive();
					Site->StoppingCondition.wait(ActivityLock);
				}
				THROW_IF_FAILED(Recovery->Stop());
				Assert::IsFalse(PathFileExistsW(OutputPath(Format(L"Recovery.%ls-Temporary.mp4", BaseName.c_str())).c_str()));
				Assert::IsTrue(PathFileExistsW(OutputPath(Format(L"Recovery.%ls.mp4", BaseName.c_str())).c_str()));
				Assert::IsFalse(PathFileExistsW(OutputPath(Format(L"TemporaryIndex\\Recovery.%ls.mp4-Index.tmp", BaseName.c_str())).c_str()));
			}
			// TODO: Ensure playability by opening and sample counting, just a quick test for now with IMuxFilterRecovery::Needed
			auto const Recovery = Library.CreateInstance<MuxFilterRecovery, IMuxFilterRecovery>();
			THROW_IF_FAILED(Recovery->Initialize(nullptr, const_cast<BSTR>(Path.c_str()), const_cast<BSTR>(TemporaryIndexFileDirectory.c_str())));
			BOOL Needed;
			THROW_IF_FAILED(Recovery->Needed(&Needed));
			Assert::IsFalse(Needed);
			unsigned long SampleCount = 0;
			auto const Site = winrt::make_self<SampleGrabberSite>([&] (IMediaSample*) { SampleCount++; });
			{
				auto const FilterGraph2 = wil::CoCreateInstance<IFilterGraph2>(CLSID_FilterGraph, CLSCTX_INPROC_SERVER);
				wil::com_ptr<IPin> CurrentOutputPin;
				#pragma region Source
				{
					auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(CLSID_AsyncReader, CLSCTX_INPROC_SERVER);
					auto const FileSourceFilter = BaseFilter.query<IFileSourceFilter>();
					THROW_IF_FAILED(FileSourceFilter->Load(Path.c_str(), nullptr));
					AddFilter(FilterGraph2, BaseFilter, L"Source");
					CurrentOutputPin = Pin(BaseFilter, PINDIR_OUTPUT);
				}
				#pragma endregion
				#pragma region Demultiplexer
				{
					auto const Filter = DemultiplexerLibrary.CreateInstance<DemuxFilter, IDemuxFilter>();
					auto const BaseFilter = Filter.query<IBaseFilter>();
					AddFilter(FilterGraph2, BaseFilter, L"Demultiplexer");
					THROW_IF_FAILED(FilterGraph2->Connect(CurrentOutputPin.get(), Pin(BaseFilter, PINDIR_INPUT).get()));
					CurrentOutputPin = Pin(BaseFilter, PINDIR_OUTPUT);
				}
				#pragma endregion
				#pragma region SampleGrabber
				{
					auto const BaseFilter = AddSampleGrabberFilter(Site.get());
					AddFilter(FilterGraph2, BaseFilter, L"SampleGrabber");
					THROW_IF_FAILED(FilterGraph2->Connect(CurrentOutputPin.get(), Pin(BaseFilter, PINDIR_INPUT).get()));
					CurrentOutputPin = Pin(BaseFilter, PINDIR_OUTPUT);
				}
				#pragma endregion
				#pragma region NullRenderer
				{
					auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(__uuidof(NullRenderer), CLSCTX_INPROC_SERVER);
					AddFilter(FilterGraph2, BaseFilter, L"NullRenderer");
					THROW_IF_FAILED(FilterGraph2->Connect(CurrentOutputPin.get(), Pin(BaseFilter, PINDIR_INPUT).get()));
					CurrentOutputPin.reset();
				}
				#pragma endregion
				THROW_IF_FAILED(FilterGraph2.query<IMediaFilter>()->SetSyncSource(nullptr));
				THROW_IF_FAILED(FilterGraph2.query<IMediaControl>()->Run());
				LONG EventCode;
				THROW_IF_FAILED(FilterGraph2.query<IMediaEventEx>()->WaitForCompletion(INFINITE, &EventCode));
				Assert::IsTrue(EventCode == EC_COMPLETE);
			}
			Logger::WriteMessage(Format(L"SampleCount %u", SampleCount).c_str());
			Assert::IsTrue(SampleCount > 0);
		}

		BEGIN_TEST_METHOD_ATTRIBUTE(SingleTrackRecovery)
		END_TEST_METHOD_ATTRIBUTE()
		TEST_METHOD(SingleTrackRecovery)
		{
			InternalRecovery(L"SingleTrackRecovery", [] (wil::com_ptr<IFilterGraph2> const& FilterGraph2, wil::com_ptr<IBaseFilter> const& MultiplexerBaseFilter)
			{
				static REFERENCE_TIME constexpr const g_StopTime = duration_cast<nanoseconds>(100s).count() / 100;
				auto const Filter = wil::CoCreateInstance<IVideoSourceFilter>(__uuidof(VideoSourceFilter), CLSCTX_INPROC_SERVER);
				wil::unique_variant MediaType;
				MediaType.vt = VT_BSTR;
				MediaType.bstrVal = wil::make_bstr(FormatIdentifier(MEDIASUBTYPE_RGB32).c_str()).release();
				THROW_IF_FAILED(Filter->SetMediaType(360, 240, MediaType));
				THROW_IF_FAILED(Filter->SetMediaTypeRate(50, 1));
				THROW_IF_FAILED(Filter->put_Live(VARIANT_TRUE));
				auto const SourceBaseFilter = Filter.query<IBaseFilter>();
				AddFilter(FilterGraph2, SourceBaseFilter, L"Source");
				auto const CurrentOutputPin = Pin(SourceBaseFilter);
				THROW_IF_FAILED(CurrentOutputPin.query<IAMStreamControl>()->StopAt(&g_StopTime, FALSE, 1));
				THROW_IF_FAILED(FilterGraph2->Connect(CurrentOutputPin.get(), Pin(MultiplexerBaseFilter, PINDIR_INPUT).get()));
			});
		}

		BEGIN_TEST_METHOD_ATTRIBUTE(SingleAudioTrackRecovery)
			TEST_IGNORE()
		END_TEST_METHOD_ATTRIBUTE()
		TEST_METHOD(SingleAudioTrackRecovery)
		{
			InternalRecovery(L"SingleAudioTrackRecovery", [] (wil::com_ptr<IFilterGraph2> const& FilterGraph2, wil::com_ptr<IBaseFilter> const& MultiplexerBaseFilter)
			{
				static REFERENCE_TIME constexpr const g_StopTime = duration_cast<nanoseconds>(100s).count() / 100;
				auto const Filter = wil::CoCreateInstance<IAudioSourceFilter>(__uuidof(AudioSourceFilter), CLSCTX_INPROC_SERVER);
				THROW_IF_FAILED(Filter->SetMediaType(wil::make_bstr(FormatIdentifier(MEDIASUBTYPE_PCM).c_str()).get(), 48000, 2, 16));
				THROW_IF_FAILED(Filter->SetMediaSampleDuration(960));
				THROW_IF_FAILED(Filter->put_Live(!VARIANT_TRUE)); // TODO: Fix live audio?
				auto const SourceBaseFilter = Filter.query<IBaseFilter>();
				AddFilter(FilterGraph2, SourceBaseFilter, L"Source");
				auto CurrentOutputPin = Pin(SourceBaseFilter);
				THROW_IF_FAILED(CurrentOutputPin.query<IAMStreamControl>()->StopAt(&g_StopTime, FALSE, 1));
				#if 1
					{
						struct __declspec(uuid("{8946E78B-FC60-4A6C-9CCF-A7A3C9CF5E31}")) Mpeg4AacAudioEncoderFilter;
						auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(__uuidof(Mpeg4AacAudioEncoderFilter), CLSCTX_INPROC_SERVER);
						AddFilter(FilterGraph2, BaseFilter, L"Encoder");
						THROW_IF_FAILED(FilterGraph2->Connect(CurrentOutputPin.get(), Pin(BaseFilter, PINDIR_INPUT).get()));
						CurrentOutputPin = Pin(BaseFilter, PINDIR_OUTPUT);
					}
				#endif
				THROW_IF_FAILED(FilterGraph2->Connect(CurrentOutputPin.get(), Pin(MultiplexerBaseFilter, PINDIR_INPUT).get()));
			});
		}

		BEGIN_TEST_METHOD_ATTRIBUTE(DualTrackRecovery)
			#if defined(_WIN64)
				TEST_IGNORE()
			#endif
		END_TEST_METHOD_ATTRIBUTE()
		TEST_METHOD(DualTrackRecovery)
		{
			InternalRecovery(L"DualTrackRecovery", [] (wil::com_ptr<IFilterGraph2> const& FilterGraph2, wil::com_ptr<IBaseFilter> const& MultiplexerBaseFilter)
			{
				static REFERENCE_TIME constexpr const g_StopTime = duration_cast<nanoseconds>(100s).count() / 100;
				{
					auto const Filter = wil::CoCreateInstance<IVideoSourceFilter>(__uuidof(VideoSourceFilter), CLSCTX_INPROC_SERVER);
					wil::unique_variant MediaType;
					MediaType.vt = VT_BSTR;
					MediaType.bstrVal = wil::make_bstr(FormatIdentifier(MEDIASUBTYPE_RGB32).c_str()).release();
					THROW_IF_FAILED(Filter->SetMediaType(360, 240, MediaType));
					THROW_IF_FAILED(Filter->SetMediaTypeRate(50, 1));
					THROW_IF_FAILED(Filter->put_Live(VARIANT_TRUE));
					auto const SourceBaseFilter = Filter.query<IBaseFilter>();
					AddFilter(FilterGraph2, SourceBaseFilter, L"Video Source");
					auto const CurrentOutputPin = Pin(SourceBaseFilter);
					THROW_IF_FAILED(CurrentOutputPin.query<IAMStreamControl>()->StopAt(&g_StopTime, FALSE, 1));
					THROW_IF_FAILED(FilterGraph2->Connect(CurrentOutputPin.get(), Pin(MultiplexerBaseFilter, PINDIR_INPUT, 0).get()));
				}
				{
					auto const Filter = wil::CoCreateInstance<IAudioSourceFilter>(__uuidof(AudioSourceFilter), CLSCTX_INPROC_SERVER);
					THROW_IF_FAILED(Filter->SetMediaType(wil::make_bstr(FormatIdentifier(MEDIASUBTYPE_PCM).c_str()).get(), 48000, 2, 16));
					THROW_IF_FAILED(Filter->SetMediaSampleDuration(960));
					THROW_IF_FAILED(Filter->put_Live(!VARIANT_TRUE)); // TODO: Fix live audio?
					auto const SourceBaseFilter = Filter.query<IBaseFilter>();
					AddFilter(FilterGraph2, SourceBaseFilter, L"Audio Source");
					auto CurrentOutputPin = Pin(SourceBaseFilter);
					THROW_IF_FAILED(CurrentOutputPin.query<IAMStreamControl>()->StopAt(&g_StopTime, FALSE, 1));
					#if 1 // WARN: PCM audio is handled differently in MuxFilter and... is basically incompatible
						{
							struct __declspec(uuid("{8946E78B-FC60-4A6C-9CCF-A7A3C9CF5E31}")) Mpeg4AacAudioEncoderFilter;
							auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(__uuidof(Mpeg4AacAudioEncoderFilter), CLSCTX_INPROC_SERVER);
							AddFilter(FilterGraph2, BaseFilter, L"Encoder");
							THROW_IF_FAILED(FilterGraph2->Connect(CurrentOutputPin.get(), Pin(BaseFilter, PINDIR_INPUT).get()));
							CurrentOutputPin = Pin(BaseFilter, PINDIR_OUTPUT);
						}
					#endif
					THROW_IF_FAILED(FilterGraph2->Connect(CurrentOutputPin.get(), Pin(MultiplexerBaseFilter, PINDIR_INPUT, 1).get()));
				}
			});
		}

		#endif
	};
}
