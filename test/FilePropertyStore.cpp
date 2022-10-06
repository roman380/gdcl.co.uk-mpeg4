#include "pch.h"
#include "Common.h"

#include <propkey.h>
#include <propvarutil.h>
#include <propsys.h>

#pragma comment(lib, "propsys.lib")

#include "..\mp4mux\mp4mux_h.h"
#include "..\mp4mux\mp4mux_i.c"

#if defined(WITH_DIRECTSHOWREFERENCESOURCE)
	using namespace AlaxInfoDirectShowReferenceSource;
#endif

struct FilePropertyStore
{
	typedef HRESULT (WINAPI* SHGETPROPERTYSTOREFROMPARSINGNAME)(PCWSTR, IBindCtx *, GETPROPERTYSTOREFLAGS, REFIID, VOID**);

	FilePropertyStore(wchar_t const* Path, GETPROPERTYSTOREFLAGS Flags = GPS_READWRITE)
	{
		WI_ASSERT(!m_ShellLibrary && !m_SHGetPropertyStoreFromParsingName && !m_PropertyStore);
		m_ShellLibrary.reset(LoadLibrary(L"shell32.dll"));
		FAIL_FAST_LAST_ERROR_IF(!m_ShellLibrary.is_valid());
		m_SHGetPropertyStoreFromParsingName = reinterpret_cast<SHGETPROPERTYSTOREFROMPARSINGNAME>(GetProcAddress(m_ShellLibrary.get(), "SHGetPropertyStoreFromParsingName"));
		FAIL_FAST_LAST_ERROR_IF(!m_SHGetPropertyStoreFromParsingName);
		THROW_IF_FAILED(m_SHGetPropertyStoreFromParsingName(Path, nullptr, Flags, __uuidof(IPropertyStore), m_PropertyStore.put_void()));
	}
	void Get(PROPERTYKEY const& Key, PROPVARIANT& Value) const
	{
		WI_ASSERT(m_PropertyStore);
		THROW_IF_FAILED(m_PropertyStore->GetValue(Key, &Value));
	}
	void Set(PROPERTYKEY const& Key, PROPVARIANT& Value)
	{
		WI_ASSERT(m_PropertyStore);
		THROW_IF_FAILED(m_PropertyStore->SetValue(Key, Value));
	}
	void Set(PROPERTYKEY const& Key, wchar_t const* Value)
	{
		wil::unique_prop_variant VariantValue;
		THROW_IF_FAILED(InitPropVariantFromString(Value, &VariantValue));
		Set(Key, VariantValue);
	}
	void Commit()
	{
		WI_ASSERT(m_PropertyStore);
		THROW_IF_FAILED(m_PropertyStore->Commit());
	}

	wil::unique_hmodule m_ShellLibrary;
	SHGETPROPERTYSTOREFROMPARSINGNAME m_SHGetPropertyStoreFromParsingName = nullptr;
	wil::com_ptr<IPropertyStore> m_PropertyStore;
};

namespace Test
{
	TEST_CLASS(Mp4File)
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
		
		BEGIN_TEST_METHOD_ATTRIBUTE(SetComment)
			TEST_IGNORE()
		END_TEST_METHOD_ATTRIBUTE()
		TEST_METHOD(SetComment)
		{
			static wchar_t constexpr const* g_Path = L"C:\\....mp4";
			FilePropertyStore PropertyStore { g_Path };
			// NOTE: Metadata Properties for Media Files https://msdn.microsoft.com/en-us/library/windows/desktop/ff384862
			// NOTE: Attempt to set PKEY_Media_DateEncoded results in MF_E_PROPERTY_READ_ONLY in Commit below since certain Windows update...
			PropertyStore.Set(PKEY_Comment, L"Test Comment 1");
			PropertyStore.Commit();
			PropertyStore.Set(PKEY_Comment, L"Test Comment 2");
			PropertyStore.Commit();
		}

		#if defined(WITH_DIRECTSHOWREFERENCESOURCE)

		BEGIN_TEST_METHOD_ATTRIBUTE(CheckComment)
		END_TEST_METHOD_ATTRIBUTE()
		TEST_METHOD(CheckComment)
		{
			static REFERENCE_TIME constexpr const g_StopTime = duration_cast<nanoseconds>(2s).count() / 100;
			auto const Path = OutputPath(L"Mp4File.CheckComment.mp4");
			if(!DeleteFileW(Path.c_str()))
				THROW_LAST_ERROR_IF(GetLastError() != ERROR_FILE_NOT_FOUND);
			static char constexpr const g_Comment[] = "Test Comment";
			{
				Library Library(L"mp4mux.dll");
				auto const FilterGraph2 = wil::CoCreateInstance<IFilterGraph2>(CLSID_FilterGraph, CLSCTX_INPROC_SERVER);
				wil::com_ptr<IPin> CurrectOutputPin;
				#pragma region Source
				{
					auto const Filter = wil::CoCreateInstance<IVideoSourceFilter>(__uuidof(VideoSourceFilter), CLSCTX_INPROC_SERVER);
					wil::unique_variant MediaType;
					MediaType.vt = VT_BSTR;
					MediaType.bstrVal = wil::make_bstr(FormatIdentifier(MEDIASUBTYPE_RGB32).c_str()).release();
					THROW_IF_FAILED(Filter->SetMediaType(720, 480, MediaType));
					THROW_IF_FAILED(Filter->SetMediaTypeRate(25, 1));
					auto const SourceBaseFilter = Filter.query<IBaseFilter>();
					AddFilter(FilterGraph2, SourceBaseFilter, L"Source");
					CurrectOutputPin = Pin(SourceBaseFilter);
					THROW_IF_FAILED(CurrectOutputPin.query<IAMStreamControl>()->StopAt(&g_StopTime, FALSE, 1));
				}
				#pragma endregion
				#pragma region Multiplexer
				{
					auto const Filter = Library.CreateInstance<MuxFilter, IMuxFilter>();
					THROW_IF_FAILED(Filter->SetComment(const_cast<BSTR>(FromMultiByte(g_Comment).c_str())));
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
				THROW_IF_FAILED(FilterGraph2.query<IMediaFilter>()->SetSyncSource(nullptr)); // ASAP
				RunFilterGraph(FilterGraph2, 20s);
				// SUGG: Also try to set comment before closing on already running filter graph
			}
			FilePropertyStore PropertyStore { Path.c_str() };
			wil::unique_prop_variant Comment;
			PropertyStore.Get(PKEY_Comment, Comment);
			Assert::AreEqual<VARTYPE>(VT_LPWSTR, Comment.vt);
			Assert::IsNotNull(Comment.pwszVal);
			Assert::IsTrue(strcmp(g_Comment, ToMultiByte(Comment.pwszVal).c_str()) == 0);
		}

		#endif
	};
}
