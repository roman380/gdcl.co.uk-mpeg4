#pragma once

#include <dshow.h>
#include <dvdmedia.h>
#include <dmodshow.h>

#pragma comment(lib, "strmiids.lib")

class __declspec(uuid("{C1F400A4-3F08-11D3-9F0B-006008039E37}")) NullRenderer;

struct __declspec(uuid("0579154a-2b53-4994-b0d0-e773148eff85"))
ISampleGrabberCB : IUnknown
{
	virtual HRESULT __stdcall SampleCB (double SampleTime, struct IMediaSample * pSample ) = 0;
	virtual HRESULT __stdcall BufferCB (double SampleTime, unsigned char * pBuffer, long BufferLen ) = 0;
};

struct __declspec(uuid("6b652fff-11fe-4fce-92ad-0266b5d7c78f"))
ISampleGrabber : IUnknown
{
	virtual HRESULT __stdcall SetOneShot (long OneShot ) = 0; 
	virtual HRESULT __stdcall SetMediaType (struct _AMMediaType * pType ) = 0;
	virtual HRESULT __stdcall GetConnectedMediaType (struct _AMMediaType * pType ) = 0;
	virtual HRESULT __stdcall SetBufferSamples (long BufferThem ) = 0;
	virtual HRESULT __stdcall GetCurrentBuffer (/*[in,out]*/ long * pBufferSize, /*[out]*/ long * pBuffer ) = 0;
	virtual HRESULT __stdcall GetCurrentSample (/*[out,retval]*/ struct IMediaSample * * ppSample ) = 0;
	virtual HRESULT __stdcall SetCallback (struct ISampleGrabberCB * pCallback, long WhichMethodToCallback ) = 0;
};

struct __declspec(uuid("c1f400a0-3f08-11d3-9f0b-006008039e37")) SampleGrabber; // [ default ] interface ISampleGrabber

inline bool EnumeratePins(wil::com_ptr<IBaseFilter> const& BaseFilter, std::function<bool(wil::com_ptr<IPin> const&)> Handle)
{
	WI_ASSERT(BaseFilter && Handle);
	wil::com_ptr<IEnumPins> EnumPins;
	THROW_IF_FAILED(BaseFilter->EnumPins(EnumPins.put()));
	for(; ; )
	{
		wil::com_ptr<IPin> Pin;
		ULONG Count;
		if(EnumPins->Next(1, Pin.put(), &Count) != S_OK)
			break;
		WI_ASSERT(Pin);
		if(Handle(Pin))
			return true;
	}
	return false;
}
inline wil::com_ptr<IPin> Pin(wil::com_ptr<IBaseFilter> const& BaseFilter, unsigned int PinIndex = 0)
{
	WI_ASSERT(BaseFilter);
	wil::com_ptr<IEnumPins> EnumPins;
	THROW_IF_FAILED(BaseFilter->EnumPins(EnumPins.put()));
	for(; ; )
	{
		wil::com_ptr<IPin> Pin;
		ULONG Count;
		THROW_HR_IF(E_FAIL, EnumPins->Next(1, Pin.put(), &Count) != S_OK);
		if(PinIndex-- == 0)
			return Pin;
	}
	WI_ASSERT(false);
}
inline wil::com_ptr<IPin> Pin(wil::com_ptr<IBaseFilter> const& BaseFilter, PIN_DIRECTION Direction, unsigned int PinIndex = 0)
{
	WI_ASSERT(BaseFilter);
	wil::com_ptr<IEnumPins> EnumPins;
	THROW_IF_FAILED(BaseFilter->EnumPins(EnumPins.put()));
	for(; ; )
	{
		wil::com_ptr<IPin> Pin;
		ULONG Count;
		THROW_HR_IF(E_FAIL, EnumPins->Next(1, Pin.put(), &Count) != S_OK);
		PIN_DIRECTION PinDirection;
		THROW_IF_FAILED(Pin->QueryDirection(&PinDirection));
		if(PinDirection != Direction)
			continue;
		if(PinIndex-- == 0)
			return Pin;
	}
	WI_ASSERT(false);
}

inline std::wstring PinName(wil::com_ptr<IPin> const& Pin)
{
	std::wstring Name;
	if(Pin)
	{
		PIN_INFO Info { };
		THROW_IF_FAILED(Pin->QueryPinInfo(&Info));
		if(Info.pFilter)
			Info.pFilter->Release();
		Name = Info.achName;
	}
	return Name;
}

class SampleGrabberSite : public winrt::implements<SampleGrabberSite, ISampleGrabberCB>
{
public:
	using HandleSample = std::function<void(IMediaSample*)>;

	SampleGrabberSite(HandleSample HandleSample) :
		m_HandleSample(HandleSample)
	{
		WI_ASSERT(HandleSample);
	}

// ISampleGrabberCB
	IFACEMETHOD(SampleCB)([[maybe_unused]] double SampleTime, struct IMediaSample* MediaSample) override
	{
		try
		{
			//Logger::WriteMessage(Format("SampleTime %.3f\n", SampleTime).c_str());
			WI_ASSERT(m_HandleSample);
			m_HandleSample(MediaSample);
		}
		CATCH_RETURN();
		return S_OK;
	}
	IFACEMETHOD(BufferCB)([[maybe_unused]] double SampleTime, [[maybe_unused]] unsigned char* Data, [[maybe_unused]] long DataSize) override
	{
		WI_ASSERT(false);
		return E_NOTIMPL;
	}

private:
	HandleSample m_HandleSample;
};

inline void AddFilter(wil::com_ptr<IFilterGraph2> const& FilterGraph2, wil::com_ptr<IBaseFilter> const& BaseFilter, wchar_t const* Name = nullptr)
{
	WI_ASSERT(FilterGraph2 && BaseFilter);
	THROW_IF_FAILED(FilterGraph2->AddFilter(BaseFilter.get(), Name));
}
inline wil::com_ptr<IBaseFilter> AddFilter(wil::com_ptr<IFilterGraph2> const& FilterGraph2, CLSID const& ClassIdentifier, wchar_t const* Name = nullptr)
{
	WI_ASSERT(FilterGraph2);
	auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(ClassIdentifier, CLSCTX_INPROC_SERVER);
	THROW_IF_FAILED(FilterGraph2->AddFilter(BaseFilter.get(), Name));
	return BaseFilter;
}

inline wil::com_ptr<IBaseFilter> AddSampleGrabberFilter(ISampleGrabberCB* Site)
{
	WI_ASSERT(Site);
	auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(__uuidof(SampleGrabber), CLSCTX_INPROC_SERVER);
	auto const SampleGrabber = BaseFilter.query<ISampleGrabber>();
	THROW_IF_FAILED(SampleGrabber->SetOneShot(FALSE));
	THROW_IF_FAILED(SampleGrabber->SetBufferSamples(FALSE));
	THROW_IF_FAILED(SampleGrabber->SetCallback(Site, 0)); // SampleCB
	return BaseFilter;
}
