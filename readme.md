# DirectShow MPEG-4 Part 14 (.MP4) Multiplexer and Demultiplexer Filters

These are based on [filters by Geraint Davies](http://www.gdcl.co.uk/mpeg4/) (info at gdcl dot co dot uk) enhanced by Roman Ryltsov (roman at alax dot info).

## Build Notes

In order to successfully build the solution/projects, baseclasses directory from Windows SDK needs to be copied as a subdirectory in solution file directory.

## Update History

### 05 Jun 2015

 * merged update by [Noël Danjou](http://noeld.com) (added handlers for: MPEG-2 Video and Audio, Dolby Digital (AC-3), Dolby Digital Plus (Enhanced AC-3); added support for FORMAT_UVCH264Video)
 * minor updates

### 17 May 2015

 * Fixed problem in multiplexer with stts/stsz atom discrepancy; produced files could be rejected by MainConcept demultiplexer and GDCL demultiplexer would produce final sample on the track with incorrect zero timestamp

### 08 May 2015

 * Projects converted to Visual Studio 2013, stripped older solution/project files; using Windows XP compatible toolset
 * Added type libraries with coclasses (esp. to enable `#import` against built binaries and automation of registration free COM)    
 * Demultiplexer:
  * Added ability to restart streaming without re-creation of thread, esp. to speed up seeking in scrubbing mode
  * Added support for edit entries (elst atom and related) (*)
  * Added AsyncRequestor code, which is not used/disabled (*)
  * Added attempt to normalize video frame rate (*)
  * Added (extended) ability to store raw/uncompressed video in MP4 container, such as UYVY, RGB video; this might be lacking compatibility but can be used for temporary storage 
  * Added implementation to handle `AM_SEEKING_SeekToKeyFrame` and `AM_SEEKING_ReturnTime` flags when seeking
  * Added private interface and method `IDemuxOutputPin::GetMediaSampleTimes` to quickly read track/stream media sample times without loading actual data
  * Fixed problem with stream times set off (by small amount) after seeking
 * Multiplexer:
  * Time scale changed from 90 kHz to 360 kHz to add precision to 59.94 fps timings
  * Added capability to create temporary index file as multiplexer writes, in order to be able to recover aborted/crashed session using broken .MP4 file and the index file
  * Added `IMuxMemAllocator` interface with ability to (a) identify copy allocator, (b) reduce minimal forced amount of buffers on copy allocator
  * Added `IMuxInputPin::GetMemAllocators` to access public and internal memory allocators
  * Fixed problem with input samples without stop time
  * Safer `MuxInput::GetAllocator` implementation, fixing memory leak and avoiding unnecessary allocator construction  
  * Improved support for raw video media types, added support for HDYC code

(*) Geraint's unpublished updates

---

## Free DirectShow Mpeg-4 Filters

The GDCL Mpeg-4 Demultiplexor and Multiplexor filters are now freely available for download in source form. You can re-use them in your projects, commercial or otherwise (provided that you don’t pretend that you wrote them) or use them as sample code when starting on your own project. Of course support and documentation are somewhat limited.

This initial release is being made available because of the insolvency of a customer. As I get time, I intend to enhance these filters. For now, they support ISO Mpeg-4 files containing Mpeg-4 and H.264 video and a variety of audio formats. The files created by the multiplexor will work with QuickTime and the iPod.

Note: both mux and demux filters need a small amount of code added for each media type that is to be supported. So far I have only added a limited set of types. If you need other types to be accepted, please get in touch directly. It’s likely that only a few lines of code will be needed.

Published: September 2006. Latest Update: May 2013.