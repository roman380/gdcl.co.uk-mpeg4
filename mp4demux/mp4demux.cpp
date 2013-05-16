// mp4demux.cpp : Defines the entry point for the DLL.
//
// Copyright (c) GDCL 2004-6. All Rights Reserved. 
// You are free to re-use this as the basis for your own filter development,
// provided you retain this copyright notice in the source.
// http://www.gdcl.co.uk

#include "stdafx.h"
#include "demuxfilter.h"

// --- COM factory table and registration code --------------

// DirectShow base class COM factory requires this table, 
// declaring all the COM objects in this DLL
CFactoryTemplate g_Templates[] = {
    // one entry for each CoCreate-able object
    {
        Mpeg4Demultiplexor::m_sudFilter.strName,
        Mpeg4Demultiplexor::m_sudFilter.clsID,
        Mpeg4Demultiplexor::CreateInstance,
        NULL,
        &Mpeg4Demultiplexor::m_sudFilter
    },
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

// self-registration entrypoint
STDAPI DllRegisterServer()
{
    // base classes will handle registration using the factory template table
    HRESULT hr = AMovieDllRegisterServer2(true);
    return hr;
}

STDAPI DllUnregisterServer()
{
    // base classes will handle de-registration using the factory template table
    HRESULT hr = AMovieDllRegisterServer2(false);
    return hr;
}

// if we declare the correct C runtime entrypoint and then forward it to the DShow base
// classes we will be sure that both the C/C++ runtimes and the base classes are initialized
// correctly
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);
BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpReserved)
{
    return DllEntryPoint(reinterpret_cast<HINSTANCE>(hDllHandle), dwReason, lpReserved);
}

