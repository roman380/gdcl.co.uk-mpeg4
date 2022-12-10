#pragma once

#include "mp4mux_h.h"

class __declspec(uuid("{73D9D53D-30A3-451E-976A-2B4186FE27EC}")) MuxFilterRecovery : 
	public winrt::implements<MuxFilterRecovery, IMuxFilterRecovery>
{
public:
	
// IMuxFilterRecovery
	IFACEMETHOD(Initialize)(IMuxFilterRecoverySite* Site, BSTR Path, BSTR TemporaryIndexFileDirectory) override
	{
		//TRACE(L"this 0x%p\n", this);
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
		//TRACE(L"this 0x%p\n", this);
		try
		{
			THROW_HR_IF_NULL(E_POINTER, Needed);
		}
		CATCH_RETURN();
		return S_OK;
	}
	IFACEMETHOD(Active)(BOOL* Active) override
	{
		//TRACE(L"this 0x%p\n", this);
		try
		{
			THROW_HR_IF_NULL(E_POINTER, Active);
		}
		CATCH_RETURN();
		return S_OK;
	}
	IFACEMETHOD(Start)() override
	{
		//TRACE(L"this 0x%p\n", this);
		try
		{
		}
		CATCH_RETURN();
		return S_OK;
	}
	IFACEMETHOD(Stop)() override
	{
		//TRACE(L"this 0x%p\n", this);
		try
		{
		}
		CATCH_RETURN();
		return S_OK;
	}

private:
	mutable wil::srwlock m_DataMutex;
	wil::com_ptr<IMuxFilterRecoverySite> m_Site;
	std::wstring m_Path;
	std::wstring m_TemporaryIndexFileDirectory;
};
