#include "stdafx.h"

#include <unknwn.h>
#include <winrt\base.h>
#include <winrt\Windows.Foundation.h>

#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "runtimeobject.lib")

#include <wil\resource.h>
#include <wil\com.h>
#include <wil\winrt.h>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

inline std::wstring FormatIdentifier(GUID const& Value)
{
    wchar_t Text[40] { };
    WINRT_VERIFY(StringFromGUID2(Value, Text, static_cast<int>(std::size(Text))));
    return Text;
}

#include "Recovery.h"

template <typename Class>
class ClassFactory :
    public winrt::implements<ClassFactory<Class>, IClassFactory>
{
public:

// IClassFactory
    IFACEMETHOD(CreateInstance)(IUnknown* OuterUnknown, REFIID InterfaceIdentifier, VOID** Object) override
    {
        TRACE(L"this 0x%p, OuterUnknown 0x%p, InterfaceIdentifier %ls\n", this, OuterUnknown, FormatIdentifier(InterfaceIdentifier).c_str());
        try
        {
            THROW_HR_IF(CLASS_E_NOAGGREGATION, OuterUnknown != nullptr);
            THROW_HR_IF_NULL(E_POINTER, Object);
            *Object = nullptr;
            auto const Instance = winrt::make_self<Class>();
            THROW_IF_FAILED(Instance->QueryInterface(InterfaceIdentifier, Object));
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(LockServer)(BOOL Lock) override
    {
        //TRACE(L"this 0x%p, Lock %d\n", this, Lock);
        auto& ModuleLock = winrt::get_module_lock();
        if(Lock) ++ModuleLock; else --ModuleLock;
        return S_OK;
    }
};

// NOTE: CLSID Key https://docs.microsoft.com/en-us/windows/win32/com/clsid-key-hklm

void GetRegisterUnregisterClsidKey(CLSID const& ClassIdentifier, wil::unique_hkey& ClsidKey, OLECHAR (&ClassIdentifierString)[64])
{
    THROW_IF_FAILED(HRESULT_FROM_WIN32(RegOpenKeyExW(HKEY_CLASSES_ROOT, L"CLSID", 0, KEY_READ | KEY_WRITE, ClsidKey.put())));
    WI_VERIFY(StringFromGUID2(ClassIdentifier, ClassIdentifierString, static_cast<INT>(std::size(ClassIdentifierString))));
}
void RegisterServer(CLSID const& ClassIdentifier, wchar_t const* Description, wchar_t const* ThreadingModel = nullptr)
{
    wil::unique_hkey ClsidKey;
    OLECHAR ClassIdentifierString[64];
    GetRegisterUnregisterClsidKey(ClassIdentifier, ClsidKey, ClassIdentifierString);
    wil::unique_hkey ClassKey;
    THROW_IF_FAILED(HRESULT_FROM_WIN32(RegCreateKeyW(ClsidKey.get(), ClassIdentifierString, ClassKey.put())));
    if(Description)
        THROW_IF_FAILED(HRESULT_FROM_WIN32(RegSetValueExW(ClassKey.get(), nullptr, 0, REG_SZ, reinterpret_cast<BYTE const*>(Description), static_cast<DWORD>(wcslen(Description) * sizeof *Description))));
    // NOTE: InprocServer32 https://docs.microsoft.com/en-us/windows/win32/com/inprocserver32
    wil::unique_hkey ServerKey;
    THROW_IF_FAILED(HRESULT_FROM_WIN32(RegCreateKeyW(ClassKey.get(), L"InprocServer32", ServerKey.put())));
    wchar_t Path[MAX_PATH];
    WI_VERIFY(GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), Path, static_cast<DWORD>(std::size(Path))));
    THROW_IF_FAILED(HRESULT_FROM_WIN32(RegSetValueExW(ServerKey.get(), nullptr, 0, REG_SZ, reinterpret_cast<BYTE const*>(Path), static_cast<DWORD>(wcslen(Path) * sizeof *Path))));
    if(ThreadingModel)
        THROW_IF_FAILED(HRESULT_FROM_WIN32(RegSetValueExW(ServerKey.get(), L"ThreadingModel", 0, REG_SZ, reinterpret_cast<BYTE const*>(ThreadingModel), static_cast<DWORD>(wcslen(ThreadingModel) * sizeof *ThreadingModel))));
}
void UnregisterServer(CLSID const& ClassIdentifier)
{
    wil::unique_hkey ClsidKey;
    OLECHAR ClassIdentifierString[64];
    GetRegisterUnregisterClsidKey(ClassIdentifier, ClsidKey, ClassIdentifierString);
    auto const Result = HRESULT_FROM_WIN32(RegDeleteTreeW(ClsidKey.get(), ClassIdentifierString));
    THROW_HR_IF(Result, FAILED(Result) && Result != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
}

STDAPI WinrtDllGetClassObject(REFCLSID ClassIdentifier, REFIID InterfaceIdentifier, void** Object)
{
    //TRACE(L"ClassIdentifier %ls, InterfaceIdentifier %ls\n", FormatIdentifier(ClassIdentifier).c_str(), FormatIdentifier(InterfaceIdentifier).c_str());
    try
    {
        THROW_HR_IF_NULL(E_POINTER, Object);
        if(ClassIdentifier == __uuidof(MuxFilterRecovery))
        {
            auto const Instance = winrt::make_self<ClassFactory<MuxFilterRecovery>>();
            wil::com_query_to(Instance.get(), InterfaceIdentifier, Object);
        } else
            return CLASS_E_CLASSNOTAVAILABLE;
    }
    CATCH_RETURN();
    return S_OK;
}

STDAPI WinrtDllRegisterServer()
{
    TRACE(L"...\n");
    try
    {
        //try
        //{
            RegisterServer(__uuidof(MuxFilterRecovery), L"MuxFilterRecovery Class", L"Both");
        //}
        //catch(...)
        //{
        //	LOG_CAUGHT_EXCEPTION();
        //	UnregisterServer(__uuidof(MuxFilterRecovery));
        //	throw;
        //}
    }
    CATCH_RETURN();
    return S_OK;
}

STDAPI WinrtDllUnregisterServer()
{
    TRACE(L"...\n");
    try
    {
        UnregisterServer(__uuidof(MuxFilterRecovery));
    }
    CATCH_RETURN();
    return S_OK;
}

STDAPI WinrtDllCanUnloadNow()
{
    //TRACE(L"...\n");
    if(winrt::get_module_lock())
        return S_FALSE;
    return S_OK;
}
