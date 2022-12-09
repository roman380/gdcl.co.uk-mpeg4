#include "pch.h"
#include "Common.h"

#include "..\mp4mux\mp4mux_h.h"
#include "..\mp4mux\mp4mux_i.c"

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

		class RecoverySite : 
			public winrt::implements<RecoverySite, IMuxFilterRecoverySite>
		{
		public:

		// IMuxFilterRecoverySite
			IFACEMETHOD(AfterStart)() override
			{
				//TRACE(L"this 0x%p\n", this);
				return S_OK;
			}
			IFACEMETHOD(BeforeStop)() override
			{
				//TRACE(L"this 0x%p\n", this);
				return S_OK;
			}
			IFACEMETHOD(Progress)([[maybe_unused]] DOUBLE Progress) override
			{
				//TRACE(L"this 0x%p, Progress %.03f\n", this, Progress);
				return S_OK;
			}
		};

		#if defined(WITH_DIRECTSHOWREFERENCESOURCE)

		BEGIN_TEST_METHOD_ATTRIBUTE(Temporary)
		END_TEST_METHOD_ATTRIBUTE()
		TEST_METHOD(Temporary)
		{
			static REFERENCE_TIME constexpr const g_StopTime = duration_cast<nanoseconds>(100s).count() / 100;
			auto const Path = OutputPath(L"Recovery.Temporary.mp4");
			if(PathFileExistsW(Path.c_str()))
				WI_VERIFY(DeleteFileW(Path.c_str()));
			auto const TemporaryIndexFileDirectory = OutputPath(L"TemporaryIndex");
			CreateDirectoryW(TemporaryIndexFileDirectory.c_str(), nullptr);
			Library Library(L"mp4mux.dll");
			{
				auto const FilterGraph2 = wil::CoCreateInstance<IFilterGraph2>(CLSID_FilterGraph, CLSCTX_INPROC_SERVER);
				wil::com_ptr<IPin> CurrectOutputPin;
				#pragma region Source
				{
					auto const Filter = wil::CoCreateInstance<IVideoSourceFilter>(__uuidof(VideoSourceFilter), CLSCTX_INPROC_SERVER);
					wil::unique_variant MediaType;
					MediaType.vt = VT_BSTR;
					MediaType.bstrVal = wil::make_bstr(FormatIdentifier(MEDIASUBTYPE_RGB32).c_str()).release();
					THROW_IF_FAILED(Filter->SetMediaType(360, 240, MediaType));
					THROW_IF_FAILED(Filter->SetMediaTypeRate(50, 1));
					THROW_IF_FAILED(Filter->put_Live(VARIANT_TRUE));
					auto const SourceBaseFilter = Filter.query<IBaseFilter>();
					AddFilter(FilterGraph2, SourceBaseFilter, L"Source");
					CurrectOutputPin = Pin(SourceBaseFilter);
					THROW_IF_FAILED(CurrectOutputPin.query<IAMStreamControl>()->StopAt(&g_StopTime, FALSE, 1));
				}
				#pragma endregion
				#pragma region Multiplexer
				{
					auto const Filter = Library.CreateInstance<MuxFilter, IMuxFilter>();
					THROW_IF_FAILED(Filter->SetTemporaryIndexFileEnabled(TRUE));
					THROW_IF_FAILED(Filter->SetTemporaryIndexFileDirectory(const_cast<BSTR>(TemporaryIndexFileDirectory.c_str())));
					THROW_IF_FAILED(Filter->SetSkipClose(TRUE)); // Stop will skip write of moof atom and leave the produced MP4 unplayable
					auto const BaseFilter = Filter.query<IBaseFilter>();
					AddFilter(FilterGraph2, BaseFilter, L"Multiplexer");
					THROW_IF_FAILED(FilterGraph2->Connect(CurrectOutputPin.get(), Pin(BaseFilter, PINDIR_INPUT).get()));
					CurrectOutputPin = Pin(BaseFilter, PINDIR_OUTPUT);
				}
				#pragma endregion
				#pragma region File Writer
				{
					auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(CLSID_FileWriter, CLSCTX_INPROC_SERVER);
					auto const FileSinkFilter2 = BaseFilter.query<IFileSinkFilter2>();
					THROW_IF_FAILED(FileSinkFilter2->SetFileName(Path.c_str(), nullptr));
					THROW_IF_FAILED(FileSinkFilter2->SetMode(AM_FILE_OVERWRITE));
					AddFilter(FilterGraph2, BaseFilter, L"Renderer");
					THROW_IF_FAILED(FilterGraph2->Connect(CurrectOutputPin.get(), Pin(BaseFilter).get()));
					CurrectOutputPin.reset();
				}
				#pragma endregion
				RunFilterGraph(FilterGraph2, 3s);
			}
			Assert::IsTrue(PathFileExistsW(Path.c_str()));
			// NOTE: File is unusable at this point as SetSkipClose above instructed to skip finalization
			{
				auto const Recovery = Library.CreateInstance<MuxFilterRecovery, IMuxFilterRecovery>();
				auto const Site = winrt::make_self<RecoverySite>();
				THROW_IF_FAILED(Recovery->Initialize(Site.get(), const_cast<BSTR>(Path.c_str())));
				BOOL Needed;
				THROW_IF_FAILED(Recovery->Needed(&Needed));
				Assert::IsTrue(Needed);
				THROW_IF_FAILED(Recovery->Start());
				THROW_IF_FAILED(Recovery->Stop());
			}
			// TODO: Ensure playability by opening and sample counting, just a quick test for now with IMuxFilterRecovery::Needed
			auto const Recovery = Library.CreateInstance<MuxFilterRecovery, IMuxFilterRecovery>();
			THROW_IF_FAILED(Recovery->Initialize(nullptr, const_cast<BSTR>(Path.c_str())));
			BOOL Needed;
			THROW_IF_FAILED(Recovery->Needed(&Needed));
			Assert::IsFalse(Needed);
		}

		#endif
	};
}
