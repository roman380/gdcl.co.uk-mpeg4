#include "pch.h"
#include "Common.h"

#include "..\mp4mux\mp4mux_h.h"
#include "..\mp4mux\mp4mux_i.c"
#include "..\mp4demux\mp4demux_h.h"
#include "..\mp4demux\mp4demux_i.c"
#include "..\mp4mux\DebugTrace.h"

namespace Test
{
	TEST_CLASS(Performance)
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

		BEGIN_TEST_METHOD_ATTRIBUTE(Read)
		END_TEST_METHOD_ATTRIBUTE()
		TEST_METHOD(Read)
		{
			auto const Path = OutputPath(L"..\\..\\..\\input\\v1280x720@60-a-30min.mp4");
			Library DemultiplexerLibrary(L"mp4demux.dll");
			struct Stream
			{
				std::wstring Name;
				unsigned long SampleCount = 0;
			};
			std::list<Stream> StreamList;
			{
				auto const FilterGraph2 = wil::CoCreateInstance<IFilterGraph2>(CLSID_FilterGraph, CLSCTX_INPROC_SERVER);
				wil::com_ptr<IPin> CurrectOutputPin;
				#pragma region Multiplexer
				{
					auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(CLSID_AsyncReader, CLSCTX_INPROC_SERVER);
					auto const FileSourceFilter = BaseFilter.query<IFileSourceFilter>();
					THROW_IF_FAILED(FileSourceFilter->Load(Path.c_str(), nullptr));
					AddFilter(FilterGraph2, BaseFilter, L"Source");
					CurrectOutputPin = Pin(BaseFilter);
				}
				#pragma endregion
				#pragma region Demultiplexer
				{
					auto const Filter = DemultiplexerLibrary.CreateInstance<DemuxFilter, IDemuxFilter>();
					auto const BaseFilter = Filter.query<IBaseFilter>();
					AddFilter(FilterGraph2, BaseFilter, L"Demultiplexer");
					THROW_IF_FAILED(FilterGraph2->Connect(CurrectOutputPin.get(), Pin(BaseFilter).get()));
					CurrectOutputPin.reset();
					unsigned int OutputPinIndex = 0;
					EnumeratePins(BaseFilter, [&](wil::com_ptr<IPin> const& OutputPin) 
					{
						PIN_DIRECTION PinDirection;
						THROW_IF_FAILED(OutputPin->QueryDirection(&PinDirection));
						if(PinDirection == PIN_DIRECTION::PINDIR_OUTPUT)
						{
							auto& Stream = StreamList.emplace_back();
							Stream.Name = PinName(OutputPin);
							auto const Site = winrt::make_self<SampleGrabberSite>([&] (IMediaSample*)
							{
								Stream.SampleCount++;
							});
							#pragma region SampleGrabber
							{
								auto const BaseFilter = AddSampleGrabberFilter(Site.get());
								AddFilter(FilterGraph2, BaseFilter, Format(L"SampleGrabber-%02u", OutputPinIndex).c_str());
								THROW_IF_FAILED(FilterGraph2->Connect(OutputPin.get(), Pin(BaseFilter, PINDIR_INPUT).get()));
								CurrectOutputPin = Pin(BaseFilter, PINDIR_OUTPUT);
							}
							#pragma endregion
							#pragma region NullRenderer
							auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(__uuidof(NullRenderer), CLSCTX_INPROC_SERVER);
							AddFilter(FilterGraph2, BaseFilter, Format(L"NullRenderer-%02u", OutputPinIndex).c_str());
							THROW_IF_FAILED(FilterGraph2->Connect(CurrectOutputPin.get(), Pin(BaseFilter, PINDIR_INPUT).get()));
							CurrectOutputPin.reset();
							#pragma endregion
							OutputPinIndex++;
						}
						return false;
					});
				}
				#pragma endregion
				THROW_IF_FAILED(FilterGraph2.query<IMediaFilter>()->SetSyncSource(nullptr));
				auto const TimeA = system_clock::now();
				RunFilterGraph(FilterGraph2, 30s);
				auto const TimeB = system_clock::now();
				Logger::WriteMessage(Format(L"Elapsed Time: %.3f seconds\n", duration_cast<milliseconds>(TimeB - TimeA).count() / 1E3).c_str());
				for(auto&& Stream: StreamList)
					Logger::WriteMessage(Format(L"Stream %ls: %u samples\n", Stream.Name.c_str(), Stream.SampleCount).c_str());
			}
		}
	};
}
