#pragma once

#include <chrono>
#include <functional>

#include "SampleGrabberEx.h"

#include <CppUnitTest.h> // MS_CPP_UNITTESTFRAMEWORK

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

using namespace std::chrono;
using namespace std::chrono_literals;

using namespace Microsoft::VisualStudio::CppUnitTestFramework; // https://learn.microsoft.com/en-us/visualstudio/test/how-to-use-microsoft-test-framework-for-cpp?view=vs-2022

inline std::string Format(char const* Format, ...)
{
	va_list Arguments;
	va_start(Arguments, Format);
	char Text[(8 << 10) - 32];
	Text[0] = 0;
	auto const Result = vsprintf_s(Text, Format, Arguments);
	assert(Result != -1);
	va_end(Arguments);
	return Text;
}
inline std::wstring Format(wchar_t const* Format, ...)
{
	va_list Arguments;
	va_start(Arguments, Format);
	wchar_t Text[(8 << 10) - 32];
	Text[0] = 0;
	auto const Result = vswprintf_s(Text, Format, Arguments);
	assert(Result != -1);
	va_end(Arguments);
	return Text;
}

inline std::string Join(std::vector<std::string> const& Vector, char const* Separator)
{
	std::string Text;
	if(!Vector.empty())
	{
		Text = Vector[0];
		for(size_t Index = 1; Index < Vector.size(); Index++)
		{
			Text.append(Separator);
			Text.append(Vector[Index]);
		}
	}
	return Text;
}
inline std::wstring Join(std::vector<std::wstring> const& Vector, wchar_t const* Separator)
{
	std::wstring Text;
	if(!Vector.empty())
	{
		Text = Vector[0];
		for (size_t Index = 1; Index < Vector.size(); Index++)
		{
			Text.append(Separator);
			Text.append(Vector[Index]);
		}
	}
	return Text;
}

inline std::wstring FromMultiByte(char const* Input, size_t InputLength, DWORD CodePage = CP_UTF8, DWORD Flags = 0)
{
	std::wstring Output;
	if(InputLength)
	{
		auto const OutputCapacity = MultiByteToWideChar(CodePage, Flags, Input, static_cast<INT>(InputLength), nullptr, 0);
		Output.resize(OutputCapacity);
		auto const OutputSize = MultiByteToWideChar(CodePage, Flags, Input, static_cast<INT>(InputLength), const_cast<wchar_t*>(Output.data()), OutputCapacity);
		assert(OutputSize > 0);
		Output.resize(OutputSize);
	}
	return Output;
}
inline std::wstring FromMultiByte(std::string const& Input, DWORD CodePage = CP_UTF8, DWORD Flags = 0)
{
	return FromMultiByte(Input.c_str(), Input.size(), CodePage, Flags);
}
inline std::wstring FromMultiByte(uint8_t const* Input, size_t InputLength, DWORD CodePage = CP_UTF8, DWORD Flags = 0)
{
	return FromMultiByte(reinterpret_cast<char const*>(Input), InputLength, CodePage, Flags);
}

inline std::string ToMultiByte(wchar_t const* Input, size_t InputLength, DWORD CodePage = CP_UTF8, DWORD Flags = 0)
{
	std::string Output;
	if(InputLength)
	{
		auto const OutputCapacity = WideCharToMultiByte(CodePage, Flags, Input, static_cast<INT>(InputLength), nullptr, 0, nullptr, nullptr);
		Output.resize(OutputCapacity);
		auto const OutputSize = WideCharToMultiByte(CodePage, Flags, Input, static_cast<INT>(InputLength), const_cast<char*>(Output.data()), OutputCapacity, nullptr, nullptr);
		assert(OutputSize > 0);
		Output.resize(OutputSize);
	}
	return Output;
}
inline std::string ToMultiByte(std::wstring const& Input, DWORD CodePage = CP_UTF8, DWORD Flags = 0)
{
	return ToMultiByte(Input.c_str(), Input.size(), CodePage, Flags);
}

inline std::string FormatData(uint8_t const* Data, size_t DataSize, size_t MaximalDataSize = 0)
{
	std::vector<std::string> Vector;
	for(size_t Index = 0; Index < DataSize && (!MaximalDataSize || Index < MaximalDataSize); Index++)
		Vector.push_back(Format("%02X", Data[Index]));
	std::string Text = Join(Vector, " ");
	if(MaximalDataSize && DataSize > MaximalDataSize)
		Text.append("...");
	return Text;
}
inline std::string FormatData(std::vector<uint8_t> const& Data, size_t MaximalDataSize = 0)
{
	return FormatData(Data.data(), Data.size(), MaximalDataSize);
}
inline std::wstring FormatIdentifier(GUID const& Value)
{
	OLECHAR Text[64] { };
	StringFromGUID2(Value, Text, static_cast<INT>(std::size(Text)));
	return Text;
}

namespace Test
{
	struct Library
	{
		using DllGetClassObject = HRESULT (STDMETHODCALLTYPE*)(REFCLSID, REFIID, LPVOID*);

		Library()
		{
			wchar_t Path[MAX_PATH];
			WI_VERIFY(GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), Path, static_cast<DWORD>(std::size(Path))));
			PathRemoveExtensionW(Path);
			WI_ASSERT(wcslen(Path) > 4);
			auto A = Path + wcslen(Path) - 4;
			WI_ASSERT(_wcsicmp(A, L"Test") == 0);
			wcscpy_s(A, Path + std::size(Path) - A, L".dll");
			Load(Path);
		}
		Library(wchar_t const* Name)
		{
			WI_ASSERT(Name);
			wchar_t Path[MAX_PATH];
			WI_VERIFY(GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), Path, static_cast<DWORD>(std::size(Path))));
			WI_VERIFY(PathRemoveFileSpecW(Path));
			WI_VERIFY(PathCombineW(Path, Path, Name));
			Load(Path);
		}
		~Library()
		{
			if(Instance)
				CoFreeLibrary(std::exchange(Instance, nullptr));
		}
		void Load(wchar_t const* Path)
		{
			WI_ASSERT(Path);
			WI_ASSERT(!Instance);
			Instance = CoLoadLibrary(const_cast<wchar_t*>(Path), TRUE);
			THROW_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND), Instance);
			GetClassObject = reinterpret_cast<DllGetClassObject>(GetProcAddress(Instance, "DllGetClassObject"));
			WI_ASSERT(GetClassObject);
		}
		template <typename C, typename I>
		wil::com_ptr<I> CreateInstance(IUnknown* OuterUnknown = nullptr)
		{
			WI_ASSERT(GetClassObject);
			wil::com_ptr<IClassFactory> ClassFactory;
			THROW_IF_FAILED(GetClassObject(__uuidof(C), IID_PPV_ARGS(ClassFactory.put())));
			wil::com_ptr<I> Object;
			THROW_IF_FAILED(ClassFactory->CreateInstance(OuterUnknown, IID_PPV_ARGS(Object.put())));
			return Object;
		}

		HINSTANCE Instance = nullptr;
		DllGetClassObject GetClassObject = nullptr;
	};

	struct RunningFilterGraph
	{
		RunningFilterGraph(IUnknown* FilterGraphUnknown)
		{
			WI_ASSERT(FilterGraphUnknown);
			THROW_IF_FAILED(GetRunningObjectTable(0, RunningObjectTable.put()));
			wil::com_ptr<IMoniker> Moniker;
			THROW_IF_FAILED(CreateItemMoniker(L"!", Format(L"FilterGraph %08x pid %08x\0", reinterpret_cast<DWORD_PTR>(FilterGraphUnknown), GetCurrentProcessId()).c_str(), &Moniker));
			THROW_IF_FAILED(RunningObjectTable->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, FilterGraphUnknown, Moniker.get(), &Cookie));
		}
		~RunningFilterGraph()
		{
			if(RunningObjectTable)
				THROW_IF_FAILED(RunningObjectTable->Revoke(Cookie));
		}

		wil::com_ptr<IRunningObjectTable> RunningObjectTable;
		DWORD Cookie;
	};

	struct RunFilterGraphContext
	{
		RunFilterGraphContext() = default;
		template <typename TimeoutTimeType>
		RunFilterGraphContext(TimeoutTimeType TimeoutTime) :
			TimeoutTime(duration_cast<milliseconds>(TimeoutTime))
		{
		}

		std::optional<milliseconds> TimeoutTime;
		std::function<void()> HandleAfterRun;
		std::function<bool()> HandleTimeout;
	};

	inline void RunFilterGraph(wil::com_ptr<IFilterGraph2> const& FilterGraph2, std::optional<RunFilterGraphContext> Context = std::nullopt)
	{
		WI_ASSERT(FilterGraph2);
		auto const MediaControl = FilterGraph2.query<IMediaControl>();
		Logger::WriteMessage(Format(L"Before IMediaControl::Run\n").c_str());
		THROW_IF_FAILED(MediaControl->Run());
		if(Context.has_value() && Context.value().HandleAfterRun)
			Context.value().HandleAfterRun();
		auto const MediaEventEx = FilterGraph2.query<IMediaEventEx>();
		auto const HandleEvents = [&] (std::function<void(LONG, LONG_PTR, LONG_PTR)> HandleEvent = nullptr)
		{
			for(; ; )
			{
				LONG EventCode;
				LONG_PTR Parameter1, Parameter2;
				auto const Result = MediaEventEx->GetEvent(&EventCode, &Parameter1, &Parameter2, 0);
				if(FAILED(Result))
					break;
				auto const EventParameterScope = wil::scope_exit([&] { MediaEventEx->FreeEventParams(EventCode, Parameter1, Parameter2); });
				//TRACE(L"EventCode %d, 0x%p, 0x%p\n", EventCode, Parameter1, Parameter2);
				if(HandleEvent)
					HandleEvent(EventCode, Parameter1, Parameter2);
			}
		};
		OAEVENT FilterGraphEventAvailabilityEvent;
		THROW_IF_FAILED(MediaEventEx->GetEventHandle(&FilterGraphEventAvailabilityEvent));
		// WARN: IMediaEventEx::WaitForCompletion returns E_NOTIMPL in CLSID_FilterGraphNoThread version
		#if defined(DEVELOPMENT) && 0
			if(IsDebuggerPresent())
			{
				auto const BaseTime = std::chrono::system_clock::now() + TimeoutTime;
				for(bool Complete = false; !Complete; )
				{
					auto const WaitTime = BaseTime - std::chrono::system_clock::now();
					auto const WaitTimeoutTime = static_cast<LONG>(std::chrono::duration_cast<milliseconds>(WaitTime).count());
					auto const WaitResult = WaitTimeoutTime >= 0 ? WaitForSingleObject(reinterpret_cast<HANDLE>(FilterGraphEventAvailabilityEvent), WaitTimeoutTime) : WAIT_TIMEOUT;
					if(WaitResult == WAIT_TIMEOUT && Context.has_value() && Context.value().HandleTimeout)
					{
						if(Context.has_value() && Context.value().HandleTimeout())
							break;
						THROW_HR(E_ABORT);
					}
					WI_ASSERT(WaitResult == WAIT_OBJECT_0);
					HandleEvents([&] (LONG EventCode, auto, auto)
					{
						if(EventCode == EC_COMPLETE)
							Complete = true;
						Assert::IsTrue(EventCode != EC_USERABORT && EventCode != EC_ERRORABORT);
					});
				}
			} else
		#endif
		{
			auto const TimeoutTime = Context.has_value() && Context.value().TimeoutTime.has_value() ? static_cast<DWORD>(duration_cast<milliseconds>(Context.value().TimeoutTime.value()).count()) : INFINITE;
			LONG EventCode;
			auto const Result = MediaEventEx->WaitForCompletion(static_cast<LONG>(TimeoutTime), &EventCode);
			if(Result == E_ABORT)
			{
				auto& HandleTimeout = Context.value().HandleTimeout;
				if(HandleTimeout)
					HandleTimeout();
			} else
			{
				THROW_IF_FAILED(Result);
				Assert::IsTrue(EventCode == EC_COMPLETE);
			}
		}
		Logger::WriteMessage(Format(L"Before IMediaControl::Stop\n").c_str());
		THROW_IF_FAILED(MediaControl->Stop());
	}
}

inline std::wstring OutputPath(wchar_t const* Name)
{
	wchar_t Directory[MAX_PATH];
	WI_VERIFY(GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), Directory, static_cast<DWORD>(std::size(Directory))));
	WI_VERIFY(PathRemoveFileSpecW(Directory));
	WI_ASSERT(Name);
	wchar_t Path[MAX_PATH];
	WI_VERIFY(PathCombineW(Path, Directory, Name));
	return Path;
}
inline std::wstring OutputPath(std::wstring const& Name)
{
	return OutputPath(Name.c_str());
}
