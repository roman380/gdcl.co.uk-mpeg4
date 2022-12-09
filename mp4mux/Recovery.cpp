#include "stdafx.h"

#include <unknwn.h>
#include <winrt\base.h>
#include <winrt\Windows.Foundation.h>

#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "runtimeobject.lib")

#include <wil\resource.h>
#include <wil\com.h>
#include <wil\winrt.h>

#include "Recovery.h"

template <typename Class>
class ClassFactory :
	public winrt::implements<ClassFactory<Class>, IClassFactory>
{
public:

// IClassFactory
	IFACEMETHOD(CreateInstance)(IUnknown* OuterUnknown, REFIID InterfaceIdentifier, VOID** Object) override
	{
		//TRACE(L"this 0x%p, OuterUnknown 0x%p, InterfaceIdentifier %ls\n", this, OuterUnknown, MF::FormatInterfaceIdentifier(InterfaceIdentifier).c_str());
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

STDAPI WinrtDllGetClassObject(REFCLSID ClassIdentifier, REFIID InterfaceIdentifier, void** Object)
{
	try
	{
		THROW_HR_IF_NULL(E_POINTER, Object);
		WINRT_ASSERT(ClassIdentifier == __uuidof(MuxFilterRecovery));
		auto const Instance = winrt::make_self<ClassFactory<MuxFilterRecovery>>();
		wil::com_query_to(Instance.get(), InterfaceIdentifier, Object);
	}
	CATCH_RETURN();
	return S_OK;
}
