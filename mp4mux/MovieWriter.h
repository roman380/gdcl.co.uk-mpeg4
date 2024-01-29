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

inline void Write8(uint8_t Value, std::vector<uint8_t>& Data)
{
    Data.emplace_back(Value);
}
inline void Write16(uint16_t Value, std::vector<uint8_t>& Data)
{
    auto const NetworkValue = _byteswap_ushort(Value);
    std::copy(reinterpret_cast<uint8_t const*>(&NetworkValue), reinterpret_cast<uint8_t const*>(&NetworkValue) + sizeof NetworkValue, std::back_inserter(Data));
}
inline void Write24(uint32_t Value, std::vector<uint8_t>& Data)
{
    auto const NetworkValue = _byteswap_ulong(Value);
    std::copy(reinterpret_cast<uint8_t const*>(&NetworkValue) + 1, reinterpret_cast<uint8_t const*>(&NetworkValue) + sizeof NetworkValue, std::back_inserter(Data));
}
inline void Write32(uint32_t Value, std::vector<uint8_t>& Data)
{
    auto const NetworkValue = _byteswap_ulong(Value);
    std::copy(reinterpret_cast<uint8_t const*>(&NetworkValue), reinterpret_cast<uint8_t const*>(&NetworkValue) + sizeof NetworkValue, std::back_inserter(Data));
}
inline void Write64(uint64_t Value, std::vector<uint8_t>& Data)
{
    auto const NetworkValue = _byteswap_uint64(Value);
    std::copy(reinterpret_cast<uint8_t const*>(&NetworkValue), reinterpret_cast<uint8_t const*>(&NetworkValue) + sizeof NetworkValue, std::back_inserter(Data));
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

    virtual void NotifyMediaSampleWrite([[maybe_unused]] uint32_t TrackIndex, [[maybe_unused]] wil::com_ptr<IMediaSample> const& MediaSample, [[maybe_unused]] size_t DataSize)
    { 
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
    #if !defined(NDEBUG)
        m_Type = Type;
    #endif
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

    #if !defined(NDEBUG)
        uint32_t m_Type;
    #endif

private:
    AtomWriter* m_Container;
    int64_t m_Offset;
    bool m_Closed = false;
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
        m_Track(Track)
    {
        WI_ASSERT(m_Track);
    }

    HRESULT AddSample(IMediaSample* MediaSample)
    {
        REFERENCE_TIME tStart, tEnd;
        auto const hr = MediaSample->GetTime(&tStart, &tEnd);
        if(hr == VFW_S_NO_STOP_TIME)
            tEnd = tStart + 1;
        if(SUCCEEDED(hr))
        {
            // NOTE: H264 samples from large frames may be broken across several buffers, with the time set on the last sample

            if (!m_cBytes)
            {
                m_tStart = tStart;
                m_tEnd = tEnd;
            } else 
            {
                if (tStart < m_tStart)
                    m_tStart = tStart;
                if (tEnd > m_tEnd)
                    m_tEnd = tEnd;
            }
        }

        m_cBytes += static_cast<size_t>(MediaSample->GetActualDataLength());
        m_MediaSampleList.push_back(MediaSample);
        return S_OK;
    }
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
    bool IsFull(REFERENCE_TIME tMaxDur) const
    {
        if(Length() > MAX_INTERLEAVE_SIZE)
            return true;
        if(GetDuration() > tMaxDur)
            return true;
        return false;
    }
    REFERENCE_TIME GetDuration() const;
    void SetOldIndexFormat()
    {
        m_bOldIndexFormat = true;
    }

private:
    TrackWriter* m_Track;
    REFERENCE_TIME m_tStart = 0;
    REFERENCE_TIME m_tEnd = 0;
    bool m_bOldIndexFormat = false;
    size_t m_cBytes = 0;
    std::list<wil::com_ptr<IMediaSample>> m_MediaSampleList;
};

// --- indexing ---

template <typename T, typename ValueType> 
class BlockList
{
public:
    static size_t constexpr const g_BlockEntryCount = 16u << 10;

    BlockList()
    {
        std::vector<ValueType> Vector;
        Vector.reserve(g_BlockEntryCount);
        m_Blocks.emplace_back(std::move(Vector));
    }

    void Append(ValueType Value)
    {
        WI_ASSERT(!m_Blocks.empty());
        if (m_Blocks.back().size() >= g_BlockEntryCount)
        {
            std::vector<ValueType> Vector;
            Vector.reserve(g_BlockEntryCount);
            m_Blocks.emplace_back(std::move(Vector));
        }
        auto& Block = m_Blocks.back();
        Block.emplace_back(static_cast<T*>(this)->Transform(Value));
    }
    void Write(std::shared_ptr<Atom> const& Atom)
    {
        for(auto&& Block: m_Blocks)
            THROW_IF_FAILED(Atom->Append(reinterpret_cast<uint8_t const*>(Block.data()), Block.size() * sizeof (ValueType)));
    }
    size_t Entries() const
    {
        return ((m_Blocks.size() - 1) * g_BlockEntryCount) + m_Blocks.back().size();
    }
    ValueType Entry(size_t Index) const
    {
        if(Index >= Entries())
            return 0;
        auto Iterator = m_Blocks.cbegin();
        std::advance(Iterator, Index / g_BlockEntryCount);
        auto const& Block = *Iterator;
        return static_cast<T const*>(this)->Transform(Block[Index % g_BlockEntryCount]);
    }

private:
    std::list<std::vector<ValueType>> m_Blocks;
};

class Uint32List : public BlockList<Uint32List, uint32_t>
{
public:
    static uint32_t Transform(uint32_t Value)
    {
        return _byteswap_ulong(Value);
    }
};

class Uint64List : public BlockList<Uint64List, uint64_t>
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
class RunLengthUint32List
{
public:
    void Append(uint32_t Value)
    {
        m_ValueCount++;
        if(m_Count)
        {
            if(Value != m_Value)
            {
                m_Table.Append(static_cast<uint32_t>(m_Count));
                m_Table.Append(m_Value);
                //m_Count = 0;
            } else
            {
                m_Count++;
                return;
            }
        }
        m_Value = Value;
        m_Count = 1;
    }
    void Write(std::shared_ptr<Atom> const& Atom)
    {
        if(m_Count > 0)
        {
            m_Table.Append(static_cast<uint32_t>(m_Count));
            m_Table.Append(m_Value);
            //m_Count = 0;
        }
        // ver/flags == 0
        // nEntries
        // pairs of <count, value>
        BYTE b[8] { };
        Write32(static_cast<uint32_t>(m_Table.Entries() / 2), b + 4); // entry count is count of pairs
        THROW_IF_FAILED(Atom->Append(b, 8));
        m_Table.Write(Atom);
    }
    size_t Entries() const 
    { 
        return m_ValueCount; 
    }

private:
    size_t m_ValueCount = 0; // Values
    uint32_t m_Value = 0; // Current Value
    size_t m_Count = 0; // Current Count
    Uint32List m_Table;
};

// sample size index -- table of <count, size> pairs
// possibly reduced to a single header
class SizeIndex
{
public:
    void Add(size_t DataSize)
    {
        // if all sizes are the same, we only need a single count/size entry.
        // Otherwise we need one size for each entry
        if(m_ValueCount == 0) // first sample
        {
            m_DataSize = DataSize;
            m_Count = 1;
        } else 
        if(m_Count > 0)
        {
            // still accumulating identical sizes
            if(DataSize != m_DataSize)
            {
                for(size_t Index = 0; Index < m_Count; Index++) // different -- need to create an entry for every one so far
                    m_Table.Append(static_cast<uint32_t>(m_DataSize));
                m_Count = 0;
                m_Table.Append(static_cast<uint32_t>(DataSize)); // add this sample
            } else 
                m_Count++;
        } else 
            m_Table.Append(static_cast<uint32_t>(DataSize)); // we are creating a separate entry for each sample
        m_ValueCount++;
    }
    void Add(size_t DataSize, size_t Count)
    {
        if(m_ValueCount == 0) // first entry
        {
            m_DataSize = DataSize;
            m_Count = Count;
            m_ValueCount = Count;
        } else 
        if(m_Count > 0 && DataSize == m_DataSize)
        {
            m_Count += Count;
            m_ValueCount += Count;
        } else
        {
            for(size_t Index = 0; Index < Count; Index++) // not worth trying to optimise this, but make sure it works if we ever get here.
                Add(DataSize);
        }
    }
    void Write(std::shared_ptr<Atom> const& Atom)
    {
        auto const StszAtom = Atom->CreateAtom('stsz');
        // ver/flags = 0
        // size
        // count
        // if size == 0, list of <count> sizes
        BYTE b[12] { };
        if(m_Table.Entries() == 0)
            Write32(static_cast<uint32_t>(m_DataSize), b + 4); // this size field is left 0 if we are creating a size entry for each sample
        Write32(static_cast<uint32_t>(m_ValueCount), b + 8);
        THROW_IF_FAILED(StszAtom->Append(b, 12));
        if(m_Table.Entries() > 0)
            m_Table.Write(StszAtom);
        THROW_IF_FAILED(StszAtom->Close());
    }

private:
    size_t m_ValueCount = 0;
    size_t m_DataSize = 0;
    size_t m_Count = 0;
    Uint32List m_Table;
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
    void WriteEDTS(std::shared_ptr<Atom> const& Atom, long scale);
    void WriteTable(std::shared_ptr<Atom> const& Atom)
    {
        if(m_nSamples <= 0)
            return;
        if(m_nSamples <= mode_decide_count) // do nothing if no samples at all
            ModeDecide();
        if(!m_bCTTS)
        {
            // the final sample duration has not been recorded -- use the stop time
            if(ToScale(m_tStopLast) > m_TotalDuration)
                AddDuration(static_cast<long>(ToScale(m_tStopLast) - m_TotalDuration));
            else
                // NOTE: We still need some duration recorded, to avoid stts/stsz discrepancy at the very least
                AddDuration(1);
        }
        auto const SttsAtom = Atom->CreateAtom('stts');
        m_STTS.Write(SttsAtom);
        THROW_IF_FAILED(SttsAtom->Close());
        if(m_bCTTS)
        {
            auto const CttsAtom = Atom->CreateAtom('ctts');
            m_CTTS.Write(CttsAtom);
            THROW_IF_FAILED(CttsAtom->Close());
        }
    }
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

    long m_scale;
    RunLengthUint32List m_STTS;
    REFERENCE_TIME m_tStartFirst;
    REFERENCE_TIME m_tStartLast;
    REFERENCE_TIME m_tStopLast;

    // check for rounding errors
    LONGLONG m_TotalDuration;
    REFERENCE_TIME m_refDuration;

    // total samples recorded
    int m_nSamples;

    // for CTTS calculation
    RunLengthUint32List m_CTTS;
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
class SampleToChunkIndex
{
public:
    SampleToChunkIndex(uint32_t sample_description_index = 1) : 
        m_sample_description_index(sample_description_index)
    {
    }

    void Add(uint32_t samples_per_chunk)
    {
        if(m_samples_per_chunk != samples_per_chunk) // make a new entry if the old one does not match
        {
            // the entry is <chunk nr, samples per chunk, data ref>
            // The Chunk Nr is one-based
            m_Table.Append(m_ChunkIndex); // first_chunk
            m_Table.Append(samples_per_chunk); // samples_per_chunk
            m_Table.Append(m_sample_description_index); // sample_description_index
            m_samples_per_chunk = samples_per_chunk;
        }
        m_ChunkIndex++;
    }
    void Write(std::shared_ptr<Atom> const& Atom)
    {
        auto const StscAtom = Atom->CreateAtom('stsc'); // 8.18 Sample To Chunk Box
        // ver/flags = 0
        // count of entries
        //    triple <first chunk, samples per chunk, sample_description_index>
        BYTE b[8] { };
        Write32(static_cast<uint32_t>(m_Table.Entries() / 3), b + 4);
        THROW_IF_FAILED(StscAtom->Append(b, 8));
        m_Table.Write(StscAtom);
    }

private:
    uint32_t const m_sample_description_index;
    uint32_t m_ChunkIndex = 1;
    uint32_t m_samples_per_chunk = 0;    //last entry
    Uint32List m_Table;
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
    void Add(uint64_t ChunkPosition)
    {
        // use the 32-bit table until we see a value > 2Gb, then always use the 64-bit table for the remainder.
        if(ChunkPosition >= 0x80000000 || m_Table64.Entries() > 0)
            m_Table64.Append(ChunkPosition);
        else
            m_Table32.Append(static_cast<uint32_t>(ChunkPosition));
    }
    void Write(std::shared_ptr<Atom> const& Atom)
    {
        // did we need 64-bit offsets?
        if(m_Table64.Entries() > 0)
        {
            Uint64List Table64; // convert 32-bit entries to 64-bit
            for(size_t Index = 0; Index < m_Table32.Entries(); Index++)
                Table64.Append(m_Table32.Entry(Index));
            
            auto const Co64Atom = Atom->CreateAtom('co64'); // 8.19 Chunk Offset Box
            BYTE b[8] { };
            //Write32(0, b); // ver/flags
            Write32(static_cast<uint32_t>(Table64.Entries() + m_Table64.Entries()), b + 4);
            THROW_IF_FAILED(Co64Atom->Append(b, 8));
            Table64.Write(Co64Atom);
            m_Table64.Write(Co64Atom);
            THROW_IF_FAILED(Co64Atom->Close());
            return;
        }
        if(m_Table32.Entries() > 0)
        {
            auto const StcoAtom = Atom->CreateAtom('stco'); // 8.19 Chunk Offset Box
            BYTE b[8] { };
            //Write32(0, b); // ver/flags
            Write32(static_cast<uint32_t>(m_Table32.Entries()), b + 4);
            THROW_IF_FAILED(StcoAtom->Append(b, 8));
            m_Table32.Write(StcoAtom);
            THROW_IF_FAILED(StcoAtom->Close());
        }
    }

private:
    Uint32List m_Table32;
    Uint64List m_Table64;
};

// map of key (sync-point) samples
class SyncSampleIndex
{
public:
    void Add(bool Sync)
    {
        if(!m_AnyNotSync)
        {
            if(!Sync)
            {
                m_AnyNotSync = true; // no longer all syncs - 
                for(uint32_t SampleIndex = 0; SampleIndex < m_SampleIndex; SampleIndex++) // must create table entries for all syncs so far
                    m_Table.Append(SampleIndex + 1); // 1-based sample index
                // but we don't need to record this one as it is not sync
            }
        } else
        if(Sync)
            m_Table.Append(m_SampleIndex + 1);
        m_SampleIndex++;
    }
    void Write(std::shared_ptr<Atom> const& Atom)
    {
        if(!m_AnyNotSync)
            return; // if all syncs, create no table
        auto const StssAtom = Atom->CreateAtom('stss'); // 8.20 Sync Sample Box
        BYTE b[8] { };
        //Write32(0, b); // ver/flags
        Write32(static_cast<uint32_t>(m_Table.Entries()), b + 4);
        THROW_IF_FAILED(StssAtom->Append(b, 8));
        m_Table.Write(StssAtom);
        THROW_IF_FAILED(StssAtom->Close());
    }

private:
    bool m_AnyNotSync = false;
    uint32_t m_SampleIndex = 0;
    Uint32List m_Table;
};

// one media track within a file.
class TrackWriter
{
public:
    TrackWriter(MovieWriter* pMovie, uint32_t index, std::unique_ptr<TypeHandler>&& TypeHandler, bool NotifyMediaSampleWrite = false);

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
    void WriteHead(std::shared_ptr<Atom> const& Atom);
    REFERENCE_TIME LastWrite() const
    {
        CAutoLock lock(&m_csQueue);
        return m_tLast;
    }

    void IndexChunk(uint64_t ChunkPosition, size_t nSamples);
    void IndexSample(bool bSync, REFERENCE_TIME tStart, REFERENCE_TIME tStop, size_t cBytes);
    void OldIndex(uint64_t ChunkPosition, size_t cBytes);
    void SetOldIndexStart(REFERENCE_TIME tStart)
    {
        m_Durations.SetOldIndexStart(tStart);
    }
    void Close(std::shared_ptr<Atom> const& Atom);

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
    uint32_t ID() const
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
    uint32_t m_index;
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
    SampleToChunkIndex m_SC;
    ChunkOffsetIndex m_CO;
    SyncSampleIndex m_Syncs;

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
    void AddAttribute(std::string Name, wil::unique_prop_variant&& Value)
    {
        m_AttributeList.emplace_back(std::make_pair(std::move(Name), std::move(Value)));
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

    void NotifyMediaSampleWrite(uint32_t TrackIndex, wil::com_ptr<IMediaSample> const& MediaSample, size_t DataSize);

private:
    void MakeIODS(std::shared_ptr<Atom> const& pmoov);
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
    std::list<std::pair<std::string, wil::unique_prop_variant>> m_AttributeList;
};

