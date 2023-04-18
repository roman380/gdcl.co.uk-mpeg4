// MovieWriter.h: interface for basic file structure classes.
//
// Geraint Davies, May 2004
//
// Copyright (c) GDCL 2004-6. All Rights Reserved. 
// You are free to re-use this as the basis for your own filter development,
// provided you retain this copyright notice in the source.
// http://www.gdcl.co.uk
//////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <algorithm>
#include <utility>
#include <string>
#include <vector>
#include <list>

// tracks are normally interleaved by the second. Define this
// to set the maximum size of a single interleave piece.
// Android sets a max 500KB limit on the offset between video
// and audio, and this can be used to control that.
#define MAX_INTERLEAVE_SIZE	(400 * 1024)

#include "TypeHandler.h"

// byte ordering to buffer
inline void Write16(uint16_t Value, uint8_t* Data)
{
    *reinterpret_cast<uint16_t*>(Data) = _byteswap_ushort(Value);
}
inline void Write32(uint32_t Value, uint8_t* Data)
{
    *reinterpret_cast<uint32_t*>(Data) = _byteswap_ulong(Value);
}
inline void Write64(uint64_t Value, uint8_t* Data)
{
    *reinterpret_cast<uint64_t*>(Data) = _byteswap_uint64(Value);
}
inline uint32_t Read32(uint8_t const* Data)
{
    return _byteswap_ulong(*reinterpret_cast<uint32_t const*>(Data));
}
inline uint64_t Read64(uint8_t const* Data)
{
    return _byteswap_uint64(*reinterpret_cast<uint64_t const*>(Data));
}

// forward references
class Atom;
class AtomWriter;
class MovieWriter;
class TrackWriter;
// do you feel at this point there should be a class ScriptWriter?

// abstract interface to atom, supported by parent
// atom or by external container (eg output pin)
class AtomWriter
{
public:
    virtual ~AtomWriter() = default;

    virtual LONGLONG Length() = 0;
    virtual LONGLONG Position() = 0;
    virtual HRESULT Replace(LONGLONG pos, uint8_t const* pBuffer, size_t cBytes) = 0;
    virtual HRESULT Append(uint8_t const* pBuffer, size_t cBytes) = 0;

    virtual void NotifyMediaSampleWrite(int TrackIndex, wil::com_ptr<IMediaSample> const& MediaSample, size_t DataSize)
    { 
        TrackIndex; MediaSample; DataSize;
    }
};

// basic container structure for MPEG-4 file format.
// Starts with length and FOURCC four byte type.
// Can contain other atoms and/or payload data
class Atom : public AtomWriter
{
public:
    Atom(AtomWriter* pContainer, LONGLONG llOffset, DWORD type);
    ~Atom()
    {
        if (!m_bClosed)
        {
            Close();
        }
    }

    LONGLONG Position()
    {
        return m_pContainer->Position() + m_llOffset;
    }
    HRESULT Replace(LONGLONG pos, const BYTE* pBuffer, size_t cBytes) 
    {
        return m_pContainer->Replace(m_llOffset + pos, pBuffer, cBytes);
    }
    HRESULT Append(const BYTE* pBuffer, size_t cBytes)
    {
        m_cBytes += cBytes;
        return m_pContainer->Append(pBuffer, cBytes);
    }
    LONGLONG Length()
    {
        return m_cBytes;
    }

    HRESULT Close();
    std::shared_ptr<Atom> CreateAtom(DWORD type); // TODO: Should be rather unique_ptr?

private:
    AtomWriter* m_pContainer;
    bool m_bClosed = false;
    LONGLONG m_llOffset;
    LONGLONG m_cBytes = 0;
};

// a collection of samples, to be written as one contiguous 
// chunk in the mdat atom. The properties will
// be indexed once the data is written.
// The IMediaSample object is kept here until written and indexed.
class MediaChunk
{
public:
    MediaChunk(TrackWriter* Track) :
        m_pTrack(Track)
    {
    }

    HRESULT AddSample(IMediaSample* pSample);
    HRESULT Write(std::shared_ptr<Atom> const& Atom);
    long Length() const
    {
        return m_cBytes;
    }
    void GetTime(REFERENCE_TIME* ptStart, REFERENCE_TIME* ptEnd)
    {
        *ptStart = m_tStart;
        *ptEnd = m_tEnd;
    }
    size_t MediaSampleCount() const
    {
        return m_MediaSampleList.size();
    }
    bool IsFull(REFERENCE_TIME tMaxDur) const;
    REFERENCE_TIME GetDuration() const;
    void SetOldIndexFormat()
    {
        m_bOldIndexFormat = true;
    }

private:
    TrackWriter* m_pTrack;
    REFERENCE_TIME m_tStart = 0;
    REFERENCE_TIME m_tEnd = 0;
    bool m_bOldIndexFormat = false;
    size_t m_cBytes = 0;
    std::list<wil::com_ptr<IMediaSample>> m_MediaSampleList;
};

// --- indexing ---

// a growable list of 32-bit values maintained in
// file byte order for writing directly to one of the
// index atoms
class ListOfLongs
{
public:
    ListOfLongs();

    void Append(long l);
    HRESULT Write(std::shared_ptr<Atom> const& Atom);
    enum {
        EntriesPerBlock = 4096/4,
    };
    long Entries() {
        return (long)(((m_Blocks.size() - 1) * EntriesPerBlock) + m_nEntriesInLast);
    }
    long Entry(long nEntry);

private:
    std::vector<std::vector<uint8_t>> m_Blocks;
    long m_nEntriesInLast = 0;
};

// growable list of 64-bit values
class ListOfI64
{
public:
    ListOfI64();

    void Append(LONGLONG ll);
    HRESULT Write(std::shared_ptr<Atom> const& Atom);
    enum {
        EntriesPerBlock = 4096/8,
    };
    long Entries() {
        return (long) (((m_Blocks.size() - 1) * EntriesPerBlock) + m_nEntriesInLast);
    }
private:
    std::vector<std::vector<uint8_t>> m_Blocks;
    long m_nEntriesInLast = 0;
};

// pairs of <count, value> longs -- this is essentially an RLE compression
// scheme for some index tables; instead of a list of values, consecutive
// identical values are grouped, so you get a list of <count, value> pairs.
// Used for CTTS and STTS
class ListOfPairs
{
public:
    ListOfPairs();
    void Append(long l);
    HRESULT Write(std::shared_ptr<Atom> const& Atom);
    long Entries() { return m_cEntries; }
private:
    ListOfLongs m_Table;

    // total entries
    long m_cEntries;

    // current pair not in table
    long m_lValue;
    long m_lCount;
};

// sample size index -- table of <count, size> pairs
// possibly reduced to a single header
class SizeIndex
{
public:
    SizeIndex();

    void Add(long cBytes);
    void AddMultiple(long cBytes, long count);
    HRESULT Write(std::shared_ptr<Atom> const& Atom);
private:
    ListOfLongs m_Table;

    // current pair not in table
    long m_cBytesCurrent;
    long m_nCurrent;

    // total samples
    long m_nSamples;
};

// sample duration table -- table of <count, duration> pairs
// Duration of each sample is calculated from
// start of next sample, except final entry (which is from stop time).
// Also records offset to first sample for edts atom
//
// Added CTTS table support for re-ordered frames.
// This table contains offsets from decode time to composition time for
// out-of-order frames. Since we do not receive decode time in DirectShow, 
// this is calculated from the difference between sample start and stop times,
// if the sample duration seems reasonably constant.
class DurationIndex
{
public:
    DurationIndex(long scale);

    void Add(REFERENCE_TIME tStart, REFERENCE_TIME tEnd);
    void AddOldFormat(int count);
    void SetOldIndexStart(REFERENCE_TIME tStart);
    HRESULT WriteEDTS(std::shared_ptr<Atom> const& Atom, long scale);
    HRESULT WriteTable(std::shared_ptr<Atom> const& Atom);
    REFERENCE_TIME Duration()
    {
        return m_tStopLast;
    }
    long Scale()
    {
        return m_scale;
    }
    HRESULT SetScale(long scale)
    {
        if (m_tStartFirst != -1)
        {
            return E_FAIL;
        }
        m_scale = scale;
        return S_OK;
    }
    
    void SetFrameDuration(LONGLONG tFrame)
    {
        m_tFrame = tFrame;
    }

    // for track start adjustment
    REFERENCE_TIME Earliest()
    {
        return m_tStartFirst;
    }
    void OffsetTimes(LONGLONG tAdjust)
    {
        m_tStartFirst += tAdjust;
        m_tStartLast += tAdjust;
        m_tStopLast += tAdjust;
        m_TotalDuration += ToScale(tAdjust);
        m_refDuration += tAdjust;
    }
    int SampleCount()						{ return m_nSamples; }
    REFERENCE_TIME AverageDuration()		{ return m_refDuration / m_nSamples; }

private:
    void AddDuration(long cThis);
    LONGLONG ToScale(REFERENCE_TIME t)
    {
        return t * m_scale / UNITS;
    }
    REFERENCE_TIME ToReftime(LONGLONG scale)
    {
        return scale * UNITS / m_scale;
    }
    void ModeDecide();
    void AppendCTTSMode(REFERENCE_TIME tStart, REFERENCE_TIME tEnd);


private:
    long m_scale;
    ListOfPairs m_STTS;
    REFERENCE_TIME m_tStartFirst;
    REFERENCE_TIME m_tStartLast;
    REFERENCE_TIME m_tStopLast;

    // check for rounding errors
    LONGLONG m_TotalDuration;
    REFERENCE_TIME m_refDuration;

    // total samples recorded
    int m_nSamples;

    // for CTTS calculation
    ListOfPairs m_CTTS;
    bool m_bCTTS;

    // look at the first few samples to decide whether to use
    // start-time-only mode (duration is just this start - last start)
    // or CTTS mode.
    enum { mode_decide_count = 10, };
    REFERENCE_TIME m_SampleStarts[mode_decide_count];
    REFERENCE_TIME m_SampleStops[mode_decide_count];
    REFERENCE_TIME m_SumDurations;
    REFERENCE_TIME m_tFrame;
    bool m_bUseFrameRate;
};

// index of samples per chunk.
// table of triplets <first chunk number, samples per chunk, data reference>
class SamplesPerChunkIndex
{
public:
    SamplesPerChunkIndex(long dataref);

    void Add(long nSamples);
    HRESULT Write(std::shared_ptr<Atom> const& Atom);
private:
    long m_dataref;
    ListOfLongs m_Table;
    long m_nTotalChunks;
    long m_nSamples;    //last entry
};

// index of chunk offsets
// 
// We need to use 64-bit offsets for the whole
// table if any are > 32bit. However it would be 
// wasteful to always use 64-bit offsets.
// We use 32-bit offsets until we see a 64-bit offset.
// The 32-bit offset table will be converted on Write
// if needed.

class ChunkOffsetIndex
{
public:
    void Add(LONGLONG posChunk);
    HRESULT Write(std::shared_ptr<Atom> const& Atom);
private:
    ListOfLongs m_Table32;
    ListOfI64 m_Table64;
};

// map of key (sync-point) samples
class SyncIndex
{
public:
    SyncIndex();

    void Add(bool bSync);
    HRESULT Write(std::shared_ptr<Atom> const& Atom);
private:
    long m_nSamples;
    bool m_bAllSync;
    ListOfLongs m_Syncs;
};

// one media track within a file.
class TrackWriter
{
public:
    TrackWriter(MovieWriter* pMovie, int index, std::unique_ptr<TypeHandler>&& TypeHandler, bool NotifyMediaSampleWrite = false);

    HRESULT Add(IMediaSample* pSample);

    // returns true if all tracks now at end
    bool OnEOS();

    bool IsAtEOS()
    {
        CAutoLock lock(&m_csQueue);
        return m_bEOS;
    }

    // no more writes accepted -- partial/queued writes abandoned (optionally)
    void Stop(bool bFlush);

    bool GetHeadTime(LONGLONG* ptHead) const;
    HRESULT WriteHead(std::shared_ptr<Atom> const& Atom);
    REFERENCE_TIME LastWrite() const
    {
        CAutoLock lock(&m_csQueue);
        return m_tLast;
    }

    void IndexChunk(LONGLONG posChunk, size_t nSamples);
    void IndexSample(bool bSync, REFERENCE_TIME tStart, REFERENCE_TIME tStop, size_t cBytes);
    void OldIndex(LONGLONG posChunk, size_t cBytes);
    void SetOldIndexStart(REFERENCE_TIME tStart)
    {
        m_Durations.SetOldIndexStart(tStart);
    }
    HRESULT Close(std::shared_ptr<Atom> const& Atom);

    REFERENCE_TIME SampleDuration()
    {
        if (m_Durations.SampleCount() > 3)
        {
            REFERENCE_TIME tDur = m_Durations.AverageDuration();
            if (tDur > 0)
            {
                return m_Durations.AverageDuration();
            }
        }
        return UNITS / m_pType->SampleRate();
    }
    REFERENCE_TIME Duration()
    {
        REFERENCE_TIME tDur = m_Durations.Duration();
        
        return tDur;
    }
    bool IsVideo()
    {
        return m_pType->IsVideo();
    }
    bool IsAudio()
    {
        return m_pType->IsAudio();
    }
    std::shared_ptr<TypeHandler> const& Handler() const
    {
        return m_pType;
    }
    long ID() 
    {
        return m_index+1;
    }
    REFERENCE_TIME Earliest()
    {
        if (m_StartAt != 0)
        {
            return m_StartAt;
        }
        return m_Durations.Earliest();
    }
    void AdjustStart(REFERENCE_TIME tAdj)
    {
        m_Durations.OffsetTimes(tAdj);
    }
    void SetStartAt(REFERENCE_TIME tStart)
    {
        if ((m_StartAt == 0) || (tStart < m_StartAt))
        {
            m_StartAt = tStart;
        }
    }
    bool IsNonMP4()
    {
        if (m_pType)
        {
            return m_pType->IsNonMP4();
        }
        return false;
    }

    void NotifyMediaSampleWrite(wil::com_ptr<IMediaSample> const& MediaSample, size_t DataSize);

private:
    MovieWriter* m_pMovie;
    int m_index;
    std::shared_ptr<TypeHandler> m_pType;
    bool m_NotifyMediaSampleWrite;

    mutable CCritSec m_csQueue;
    bool m_bEOS = false;
    bool m_bStopped = false;
    REFERENCE_TIME m_tLast = 0;
    std::shared_ptr<MediaChunk> m_pCurrent;
    std::list<std::shared_ptr<MediaChunk>> m_Queue;

    SizeIndex m_Sizes;
    DurationIndex m_Durations;
    SamplesPerChunkIndex m_SC;
    ChunkOffsetIndex m_CO;
    SyncIndex m_Syncs;

    // IAMStreamControl start offset
    // -- set to first StartAt time, if explicit,
    // which is used instead of Earliest to zero-base the
    // timestamps
    REFERENCE_TIME m_StartAt = 0;
};

class MovieWriter
{
public:
    MovieWriter(AtomWriter* pContainer);

    void Initialize(BOOL bAlignTrackStartTimeDisabled, REFERENCE_TIME nMinimalMovieDuration)
    {
        m_bAlignTrackStartTimeDisabled = bAlignTrackStartTimeDisabled;
        m_nMinimalMovieDuration = nMinimalMovieDuration;
    }
    void SetComment(std::string const& Comment)
    {
        m_Comment = Comment;
    }

    std::shared_ptr<TrackWriter> MakeTrack(const CMediaType* pmt, bool NotifyMediaSampleWrite = false);
    HRESULT Close(REFERENCE_TIME* pDuration);

    // ensures that CheckQueues is not active when
    // tracks are in their Stop method discarding queues
    void Stop();

    // mux output from pin queues -- returns true if all tracks at EOS
    bool CheckQueues();

    // empty queues  - similar to CheckQueues, but called when 
    // all pins are stopped, to flush queued data to the file
    void WriteOnStop();

    // use 90khz for movie and track
    // -- this avoids the problem with audio headers where
    // the timescale must fit in 16 bits
    long MovieScale() 
    {
        return DEFAULT_TIMESCALE;
    }

    long TrackCount()
    {
        return (long)m_Tracks.size();
    }
    std::shared_ptr<TrackWriter> const& Track(long nTrack) const
    {
        return m_Tracks[nTrack];
    }
    REFERENCE_TIME CurrentPosition();

    REFERENCE_TIME MaxInterleaveDuration()
    {
        CAutoLock lock(&m_csBitrate);
        return m_tInterleave;
    }
    void RecordBitrate(size_t index, long bitrate);

    void NotifyMediaSampleWrite(INT TrackIndex, wil::com_ptr<IMediaSample> const& MediaSample, size_t DataSize);

private:
    void MakeIODS(std::shared_ptr<Atom> const& pmoov);
    void InsertFTYP(AtomWriter* pFile);
    void WriteTrack(int indexReady);

private:
    AtomWriter* m_pContainer;

    BOOL m_bAlignTrackStartTimeDisabled;
    REFERENCE_TIME m_nMinimalMovieDuration;

    CCritSec m_csWrite;
    bool m_bStopped;
    bool m_bFTYPInserted;
    std::shared_ptr<Atom> m_patmMDAT;
    std::vector<std::shared_ptr<TrackWriter>> m_Tracks;

    CCritSec m_csBitrate;
    vector<int> m_Bitrates;
    REFERENCE_TIME m_tInterleave;

    std::string m_Comment;
};

