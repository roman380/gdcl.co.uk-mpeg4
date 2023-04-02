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
			TEST_IGNORE()
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
				wil::com_ptr<IPin> CurrentOutputPin;
				#pragma region Source
				{
					auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(CLSID_AsyncReader, CLSCTX_INPROC_SERVER);
					auto const FileSourceFilter = BaseFilter.query<IFileSourceFilter>();
					THROW_IF_FAILED(FileSourceFilter->Load(Path.c_str(), nullptr));
					AddFilter(FilterGraph2, BaseFilter, L"Source");
					CurrentOutputPin = Pin(BaseFilter);
				}
				#pragma endregion
				#pragma region Demultiplexer
				{
					auto const Filter = DemultiplexerLibrary.CreateInstance<DemuxFilter, IDemuxFilter>();
					auto const BaseFilter = Filter.query<IBaseFilter>();
					AddFilter(FilterGraph2, BaseFilter, L"Demultiplexer");
					THROW_IF_FAILED(FilterGraph2->Connect(CurrentOutputPin.get(), Pin(BaseFilter).get()));
					CurrentOutputPin.reset();
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
								CurrentOutputPin = Pin(BaseFilter, PINDIR_OUTPUT);
							}
							#pragma endregion
							#pragma region NullRenderer
							auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(__uuidof(NullRenderer), CLSCTX_INPROC_SERVER);
							AddFilter(FilterGraph2, BaseFilter, Format(L"NullRenderer-%02u", OutputPinIndex).c_str());
							THROW_IF_FAILED(FilterGraph2->Connect(CurrentOutputPin.get(), Pin(BaseFilter, PINDIR_INPUT).get()));
							CurrentOutputPin.reset();
							#pragma endregion
							OutputPinIndex++;
						}
						return false;
					});
					WINRT_ASSERT(OutputPinIndex > 0);
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

		BEGIN_TEST_METHOD_ATTRIBUTE(Copy)
			//TEST_IGNORE()
		END_TEST_METHOD_ATTRIBUTE()
		TEST_METHOD(Copy)
		{
			auto const SourcePath = OutputPath(L"..\\..\\..\\input\\v1280x720@60-a-5min.mp4");
			auto const DestinationPath = OutputPath(L"Performance.Copy.mp4");
			Library DemultiplexerLibrary(L"mp4demux.dll"), MultiplexerLibrary(L"mp4mux.dll");
			struct Stream
			{
				std::wstring Name;
				unsigned long SampleCount = 0;
			};
			std::list<Stream> StreamList;
			{
				auto const FilterGraph2 = wil::CoCreateInstance<IFilterGraph2>(CLSID_FilterGraph, CLSCTX_INPROC_SERVER);
				wil::com_ptr<IPin> CurrentOutputPin;
				#pragma region Source
				{
					auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(CLSID_AsyncReader, CLSCTX_INPROC_SERVER);
					auto const FileSourceFilter = BaseFilter.query<IFileSourceFilter>();
					THROW_IF_FAILED(FileSourceFilter->Load(SourcePath.c_str(), nullptr));
					AddFilter(FilterGraph2, BaseFilter, L"Source");
					CurrentOutputPin = Pin(BaseFilter);
				}
				#pragma endregion
				#pragma region Demultiplexer
				{
					auto const DemultiplexerFilter = DemultiplexerLibrary.CreateInstance<DemuxFilter, IDemuxFilter>();
					auto const DemultiplexerBaseFilter = DemultiplexerFilter.query<IBaseFilter>();
					AddFilter(FilterGraph2, DemultiplexerBaseFilter, L"Demultiplexer");
					THROW_IF_FAILED(FilterGraph2->Connect(CurrentOutputPin.get(), Pin(DemultiplexerBaseFilter).get()));
					CurrentOutputPin.reset();
					#pragma region Multiplexer
					{
						auto const MultiplexerFilter = MultiplexerLibrary.CreateInstance<MuxFilter, IMuxFilter>();
						auto const MultiplexerBaseFilter = MultiplexerFilter.query<IBaseFilter>();
						AddFilter(FilterGraph2, MultiplexerBaseFilter, L"Multiplexer");
						unsigned int MultiplexerPinIndex = 0;
						unsigned int DemultiplexerPinIndex = 0;
						EnumeratePins(DemultiplexerBaseFilter, [&](wil::com_ptr<IPin> const& OutputPin) 
						{
							PIN_DIRECTION PinDirection;
							THROW_IF_FAILED(OutputPin->QueryDirection(&PinDirection));
							if(PinDirection == PIN_DIRECTION::PINDIR_OUTPUT)
							{
								if(PinName(OutputPin).compare(L"AAC Audio 2") == 0)
									return false;
								auto& Stream = StreamList.emplace_back();
								Stream.Name = PinName(OutputPin);
								auto const Site = winrt::make_self<SampleGrabberSite>([&] (IMediaSample*)
								{
									Stream.SampleCount++;
								});
								#pragma region SampleGrabber
								{
									auto const BaseFilter = AddSampleGrabberFilter(Site.get());
									AddFilter(FilterGraph2, BaseFilter, Format(L"SampleGrabber-%02u", DemultiplexerPinIndex).c_str());
									THROW_IF_FAILED(FilterGraph2->Connect(OutputPin.get(), Pin(BaseFilter, PINDIR_INPUT).get()));
									CurrentOutputPin = Pin(BaseFilter, PINDIR_OUTPUT);
								}
								#pragma endregion
								THROW_IF_FAILED(FilterGraph2->Connect(CurrentOutputPin.get(), Pin(MultiplexerBaseFilter, PINDIR_INPUT, MultiplexerPinIndex++).get()));
								CurrentOutputPin.reset();
								DemultiplexerPinIndex++;
							}
							return false;
						});
						WINRT_ASSERT(DemultiplexerPinIndex > 0);
						CurrentOutputPin = Pin(MultiplexerBaseFilter, PINDIR_OUTPUT);
					}
					#pragma endregion
					#pragma region File Writer
					{
						auto const BaseFilter = wil::CoCreateInstance<IBaseFilter>(CLSID_FileWriter, CLSCTX_INPROC_SERVER);
						auto const FileSinkFilter2 = BaseFilter.query<IFileSinkFilter2>();
						THROW_IF_FAILED(FileSinkFilter2->SetFileName(DestinationPath.c_str(), nullptr));
						THROW_IF_FAILED(FileSinkFilter2->SetMode(AM_FILE_OVERWRITE));
						AddFilter(FilterGraph2, BaseFilter, L"Renderer");
						THROW_IF_FAILED(FilterGraph2->Connect(CurrentOutputPin.get(), Pin(BaseFilter).get()));
						CurrentOutputPin.reset();
					}
					#pragma endregion
				}
				#pragma endregion
				THROW_IF_FAILED(FilterGraph2.query<IMediaFilter>()->SetSyncSource(nullptr));
				auto const TimeA = system_clock::now();
				RunFilterGraph(FilterGraph2, 300s);
				auto const TimeB = system_clock::now();
				Logger::WriteMessage(Format(L"Elapsed Time: %.3f seconds\n", duration_cast<milliseconds>(TimeB - TimeA).count() / 1E3).c_str());
				for(auto&& Stream: StreamList)
					Logger::WriteMessage(Format(L"Stream %ls: %u samples\n", Stream.Name.c_str(), Stream.SampleCount).c_str());
			}
		}
	};
}

/*

..\\..\\..\\input\\v1280x720@60-a-30min.mp4

Read

Elapsed Time: 6.355 seconds
Stream H264 Video 1: 108000 samples
Stream AAC Audio 2: 84375 samples

Copy

Elapsed Time: 49.673 seconds
Stream H264 Video 1: 108000 samples
Stream AAC Audio 2: 84375 samples

Copy Debug

Elapsed Time: 17.992 seconds
Stream AAC Audio 2: 84375 samples

Copy Debug

Elapsed Time: 41.067 seconds
Stream H264 Video 1: 108000 samples

Copy Debug, 5min

Elapsed Time: 6.594 seconds
Stream H264 Video 1: 18000 samples

Copy Release, 5min

Elapsed Time: 8.534 seconds
Stream H264 Video 1: 18000 samples
Elapsed Time: 3.889 seconds
Stream H264 Video 1: 18000 samples
Elapsed Time: 4.952 seconds
Stream H264 Video 1: 18000 samples

*/
