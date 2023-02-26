// mp4mux.cpp : Defines the entry point for the DLL 
//
// Copyright (c) GDCL 2004-6. All Rights Reserved. 
// You are free to re-use this as the basis for your own filter development,
// provided you retain this copyright notice in the source.
// http://www.gdcl.co.uk

#include "stdafx.h"
#include "mp4mux_h.h"
#include "mp4mux_i.c"
#include "muxfilter.h"

// --- COM factory table and registration code --------------

// DirectShow base class COM factory requires this table, 
// declaring all the COM objects in this DLL
CFactoryTemplate g_Templates[] = {
    // one entry for each CoCreate-able object
    {
        Mpeg4Mux::m_sudFilter.strName,
        Mpeg4Mux::m_sudFilter.clsID,
        Mpeg4Mux::CreateInstance,
        NULL,
        &Mpeg4Mux::m_sudFilter
    },
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

STDAPI WinrtDllGetClassObject(REFCLSID ClassIdentifier, REFIID InterfaceIdentifier, void** Object);
STDAPI WinrtDllRegisterServer();
STDAPI WinrtDllUnregisterServer();
STDAPI WinrtDllCanUnloadNow();

// self-registration entrypoint
STDAPI DllRegisterServer()
{
    // base classes will handle registration using the factory template table
    HRESULT nResult;
    nResult = AMovieDllRegisterServer2(true);
    if(FAILED(nResult))
        return nResult;
    WCHAR pszPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(g_hInst, pszPath, _countof(pszPath));
    QzCComPtr<ITypeLib> pTypeLib;
    nResult = LoadTypeLib(pszPath, &pTypeLib);
    if(FAILED(nResult))
        return nResult;
    nResult = RegisterTypeLib(pTypeLib, pszPath, NULL);
    if(FAILED(nResult))
        return nResult;
    nResult = WinrtDllRegisterServer();
    if(FAILED(nResult))
        return nResult;
    return S_OK;
}

STDAPI DllUnregisterServer()
{
    // base classes will handle de-registration using the factory template table
    HRESULT nResult;
    // NOTE: We could obtain exact values using ITypeLib::GetLibAttr
    #if defined(_WIN64)
        static const SYSKIND g_SysKind = SYS_WIN64;
    #else
        static const SYSKIND g_SysKind = SYS_WIN32;
    #endif
    nResult = UnRegisterTypeLib(LIBID_GdclMp4Mux, 1, 0, 0, g_SysKind);
    if(nResult == TYPE_E_REGISTRYACCESS)
        nResult = S_OK;
    if(FAILED(nResult))
        return nResult;
    nResult = AMovieDllRegisterServer2(false);
    if(FAILED(nResult))
        return nResult;
    nResult = WinrtDllUnregisterServer();
    if(FAILED(nResult))
        return nResult;
    return S_OK;
}

STDAPI DllGetClassObjectEx(REFCLSID ClassIdentifier, REFIID InterfaceIdentifier, void** Object)
{
    auto const Result = WinrtDllGetClassObject(ClassIdentifier, InterfaceIdentifier, Object);
    if(SUCCEEDED(Result) || Result != CLASS_E_CLASSNOTAVAILABLE)
        return Result;
    return DllGetClassObject(ClassIdentifier, InterfaceIdentifier, Object);
}

STDAPI DllCanUnloadNowEx()
{
    auto const Result = WinrtDllCanUnloadNow();
    if(Result != S_OK)
        return S_FALSE;
    return DllCanUnloadNow();
}

// if we declare the correct C runtime entrypoint and then forward it to the DShow base
// classes we will be sure that both the C/C++ runtimes and the base classes are initialized
// correctly
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);
BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpReserved)
{
    return DllEntryPoint(reinterpret_cast<HINSTANCE>(hDllHandle), dwReason, lpReserved);
}


