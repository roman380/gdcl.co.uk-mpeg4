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

    virtual uint64_t Length() const = 0;
    virtual int64_t Position() const = 0;
    virtual HRESULT Replace(int64_t Position, uint8_t const* Data, size_t DataSize) = 0;
    virtual HRESULT Append(uint8_t const* Data, size_t DataSize) = 0;

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
    Atom(AtomWriter* Container, int64_t Offset, uint32_t Type) : 
        m_Container(Container),
        m_Offset(Offset)
    {
        // write the initial length and type dwords
        BYTE b[8];
        Write32(8, b);
        Write32(Type, b + 4);
        Append(b, 8);
    }
    ~Atom()
    {
        if(!m_Closed)
            Close();
    }

    std::shared_ptr<Atom> CreateAtom(DWORD type)
    {
        // SUGG: Rather unique_ptr?
        return std::make_shared<Atom>(this, Length(), type);
    }
    HRESULT Close()
    {
        m_Closed = true;
        // we only support 32-bit lengths for atoms
        // (otherwise you would have to either decide in the constructor
        // or shift the whole atom down).
        RETURN_HR_IF(E_INVALIDARG, m_DataSize > std::numeric_limits<uint32_t>::max());
        BYTE b[4];
        Write32(static_cast<uint32_t>(m_DataSize), b);
        return Replace(0, b, 4);
    }
    HRESULT Append(std::vector<uint8_t> const& Data)
    {
        m_DataSize += Data.size();
        return m_Container->Append(Data.data(), Data.size());
    }

// Atom
    uint64_t Length() const override
    {
        return m_DataSize;
    }
    int64_t Position() const override
    {
        return m_Container->Position() + m_Offset;
    }
    HRESULT Replace(int64_t Position, uint8_t const* Data, size_t DataSize) override
    {
        return m_Container->Replace(m_Offset + Position, Data, DataSize);
    }
    HRESULT Append(uint8_t const* Data, size_t DataSize) override
    {
        m_DataSize += DataSize;
        return m_Container->Append(Data, DataSize);
    }

private:
    AtomWriter* m_Container;
    bool m_Closed = false;
    int64_t m_Offset;
    uint64_t m_DataSize = 0;
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
    size_t Length() const
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

template <typename T, typename ValueType> 
class ListOf
{
public:
    static size_t constexpr const EntriesPerBlock = 4096;

    ListOf()
    {
        std::vector<ValueType> Vector;
        Vector.reserve(EntriesPerBlock);
        m_Blocks.emplace_back(std::move(Vector));
    }

    void Append(ValueType Value)
    {
        WI_ASSERT(!m_Blocks.empty());
        if (m_Blocks.back().size() >= EntriesPerBlock)
        {
            std::vector<ValueType> Vector;
            Vector.reserve(EntriesPerBlock);
            m_Blocks.emplace_back(std::move(Vector));
        }
        auto& Block = m_Blocks.back();
        Block.emplace_back(static_cast<T*>(this)->Transform(Value));
    }
    HRESULT Write(std::shared_ptr<Atom> const& Atom)
    {
        for(auto&& Block: m_Blocks)
            RETURN_IF_FAILED(Atom->Append(reinterpret_cast<uint8_t const*>(Block.data()), Block.size() * sizeof (ValueType)));
        return S_OK;
    }
    size_t Entries() const
    {
        return ((m_Blocks.size() - 1) * EntriesPerBlock) + m_Blocks.back().size();
    }
    ValueType Entry(size_t Index) const
    {
        if(Index >= Entries())
            return 0;
        auto Iterator = m_Blocks.cbegin();
        std::advance(Iterator, Index / EntriesPerBlock);
        auto const& Block = *Iterator;
        return static_cast<T const*>(this)->Transform(Block[Index % EntriesPerBlock]);
    }

private:
    std::list<std::vector<ValueType>> m_Blocks;
};

class ListOfLongs : public ListOf<ListOfLongs, uint32_t>
{
public:
    static uint32_t Transform(uint32_t Value)
    {
        return _byteswap_ulong(Value);
    }
};

class ListOfI64 : public ListOf<ListOfI64, uint64_t>
{
public:
    static uint64_t Transform(uint64_t Value)
    {
        return _byteswap_uint64(Value);
    }
};

// pairs of <count, value> longs -- this is essentially an RLE compression
// scheme for some index tables; instead of a list of values, consecutive
// identical values are grouped, so you get a list of <count, value> pairs.
// Used for CTTS and STTS
class ListOfPairs
{
public:
    void Append(long l);
    HRESULT Write(std::shared_ptr<Atom> const& Atom);
    long Entries() const { return m_cEntries; }
private:
    ListOfLongs m_Table;

    // total entries
    long m_cEntries;

    // current pair not in table
    long m_lValue = 0;
    long m_lCount = 0;
};

// sample size index -- table of <count, size> pairs
// possibly reduced to a single header
class SizeIndex
{
public:
    void Add(long cBytes);
    void AddMultiple(long cBytes, long count);
    HRESULT Write(std::shared_ptr<Atom> const& Atom);
private:
    ListOfLongs m_Table;

    // current pair not in table
    long m_cBytesCurrent = 0;
    long m_nCurrent = 0;

    // total samples
    long m_nSamples = 0;
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
    REFERENCE_TIME Duration() const
    {
        return m_tStopLast;
    }
    long Scale() const
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
    int SampleCount() const { return m_nSamples; }
    REFERENCE_TIME AverageDuration() const { return m_refDuration / m_nSamples; }

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
    SamplesPerChunkIndex(long dataref) : 
        m_dataref(dataref)
    {
    }

    void Add(long nSamples);
    HRESULT Write(std::shared_ptr<Atom> const& Atom);
private:
    long m_dataref;
    ListOfLongs m_Table;
    long m_nTotalChunks = 0;
    long m_nSamples = 0;    //last entry
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
    void Add(bool bSync);
    HRESULT Write(std::shared_ptr<Atom> const& Atom);

private:
    long m_nSamples = 0;
    bool m_bAllSync = true;
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

    bool IsAtEOS() const
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

    REFERENCE_TIME SampleDuration() const
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
    REFERENCE_TIME Duration() const
    {
        return m_Durations.Duration();
    }
    bool IsVideo() const
    {
        return m_pType->IsVideo();
    }
    bool IsAudio() const
    {
        return m_pType->IsAudio();
    }
    std::unique_ptr<TypeHandler> const& Handler() const
    {
        return m_pType;
    }
    long ID() const
    {
        return m_index + 1;
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
    bool IsNonMP4() const
    {
        return m_pType && m_pType->IsNonMP4();
    }

    void NotifyMediaSampleWrite(wil::com_ptr<IMediaSample> const& MediaSample, size_t DataSize);

private:
    MovieWriter* m_pMovie;
    int m_index;
    std::unique_ptr<TypeHandler> m_pType;
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

    size_t TrackCount() const
    {
        return m_Tracks.size();
    }
    std::shared_ptr<TrackWriter> const& Track(size_t TrackIndex) const
    {
        return m_Tracks[TrackIndex];
    }
    REFERENCE_TIME CurrentPosition();

    REFERENCE_TIME MaxInterleaveDuration() const
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
    AtomWriter* m_Container;

    BOOL m_bAlignTrackStartTimeDisabled;
    REFERENCE_TIME m_nMinimalMovieDuration;

    mutable CCritSec m_csWrite;
    bool m_bStopped;
    bool m_bFTYPInserted;
    std::shared_ptr<Atom> m_patmMDAT;
    std::vector<std::shared_ptr<TrackWriter>> m_Tracks;

    mutable CCritSec m_csBitrate;
    vector<int> m_Bitrates;
    REFERENCE_TIME m_tInterleave;

    std::string m_Comment;
};

