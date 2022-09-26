#include "pch.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework; // https://learn.microsoft.com/en-us/visualstudio/test/how-to-use-microsoft-test-framework-for-cpp?view=vs-2022

#include <propkey.h>
#include <propvarutil.h>
#include <propsys.h>

#pragma comment(lib, "propsys.lib")

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
	};
}
