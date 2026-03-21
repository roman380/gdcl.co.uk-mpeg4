//
// Index.h: declarations of classes for index management of mpeg-4 files.
//
// Geraint Davies, April 2004
//
// Copyright (c) GDCL 2004-6. All Rights Reserved. 
// You are free to re-use this as the basis for your own filter development,
// provided you retain this copyright notice in the source.
// http://www.gdcl.co.uk
//////////////////////////////////////////////////////////////////////

#pragma once

// currently all index tables are kept in memory. This is
// typically a few hundred kilobytes. For very large files a
// more sophisticated scheme might be worth considering?

// index giving count of samples and size of each sample
// and file location of sample
class SampleSizes
{
public:
    bool Parse(Atom* patmSTBL);
    long Size(long nSample) const;
    long SampleCount() const
    {
        return m_nSamples;
    }
    long MaxSize() const
    {
        return m_nMaxSize;
    }
    LONGLONG Offset(long nSample) const;

    // support for old-style uncompressed audio, where fixedsize =1 means 1 sample
    void AdjustFixedSize(long nBytes);

private:
    Atom* m_patmSTSZ = nullptr;
    AtomCache m_pBuffer;
    long m_nSamples = 0;
    long m_nMaxSize = 0;
    long m_nFixedSize = 0;

    long m_nEntriesSTSC = 0;
    long m_nChunks = 0;
    bool m_bCO64 = false;
    Atom* m_patmSTSC = nullptr;
    AtomCache m_pSTSC;
    Atom* m_patmSTCO = nullptr;
    AtomCache m_pSTCO;
};

// map of key samples
class KeyMap
{
public:
    ~KeyMap();

    bool Parse(Atom* patmSTBL);
    long SyncFor(long nSample) const;
    long Next(long nSample);
    size_t Get(size_t*& pnIndexes) const;

private:
    Atom* m_patmSTSS = nullptr;
    const BYTE* m_pSTSS = nullptr;
    long m_nEntries = 0;
};

// time and duration of samples
// -- all times in 100ns units
class SampleTimes
{
public:

    struct CompositionTimeOffset
    {
        static uint32_t LookupValue(const std::vector<CompositionTimeOffset>& Vector, uint32_t SampleIndex)
        {
            ASSERT(!Vector.empty());
            ASSERT(SampleIndex <= Vector.back().m_BaseSampleIndex + Vector.back().m_SampleCount);
            size_t L = 0, R = Vector.size();
            for (; ; )
            {
                auto const& E = Vector[L];
                if (SampleIndex - E.m_BaseSampleIndex < E.m_SampleCount)
                    return E.m_Value;
                ASSERT(R - L >= 2);
                size_t const C = (L + R) / 2;
                if (SampleIndex < Vector[C].m_BaseSampleIndex)
                    R = C;
                else
                    L = C;
            }
            ASSERT(FALSE);
            return 0;
        }
        static uint32_t IncrementalLookupValue(const std::vector<CompositionTimeOffset>& Vector, size_t& Index, uint32_t SampleIndex)
        {
            if (Vector.empty())
                return 0;
            ASSERT(Index < Vector.size());
            auto const& E0 = Vector[Index];
            ASSERT(SampleIndex >= E0.m_BaseSampleIndex);
            if (SampleIndex - E0.m_BaseSampleIndex < E0.m_SampleCount)
                return E0.m_Value;
            auto const& E1 = Vector[++Index];
            ASSERT(SampleIndex >= E1.m_BaseSampleIndex);
            ASSERT(SampleIndex - E1.m_BaseSampleIndex < E1.m_SampleCount);
            return E1.m_Value;
        }

        CompositionTimeOffset(uint32_t BaseSampleIndex, uint32_t SampleCount, uint32_t Value) :
            m_BaseSampleIndex(BaseSampleIndex),
            m_SampleCount(SampleCount),
            m_Value(Value)
        {
        }

        uint32_t m_BaseSampleIndex;
        uint32_t m_SampleCount;
        uint32_t m_Value;
    };

    bool Parse(long scale, LONGLONG CTOffset, Atom* patmSTBL);

    long DTSToSample(LONGLONG tStart);
    size_t Get(REFERENCE_TIME*& pnTimes) const;
    LONGLONG SampleToCTS(long nSample);
    LONGLONG Duration(long nSample);
    LONGLONG CTSOffset(long nSample) const;
    void GetCompositionTimeOffsetVector(std::vector<CompositionTimeOffset>& CompositionTimeOffsetVector) const;
    long CTSToSample(LONGLONG tStart);
    LONGLONG TotalDuration() const
    {
        return m_total;
    }

    LONGLONG TrackToReftime(LONGLONG nTrack) const;
    bool HasCTSTable() const
    {
        return m_nCTTS > 0;
    }
    LONGLONG ReftimeToTrack(LONGLONG reftime);

private:
    long m_scale;               // track scale units
    LONGLONG m_CTOffset;        // CT offset of first sample

    Atom* m_patmSTTS = nullptr;
    Atom* m_patmCTTS = nullptr;
    AtomCache m_pSTTS;
    AtomCache m_pCTTS;

    size_t m_nSTTS;
    size_t m_nCTTS;

    // Duration, DTSToSample and SampleToCTS need to
    // add up durations from the start of the table. We
    // cache the current position to reduce effort
    long m_nBaseSample;     // sample number at m_idx
    size_t m_idx;             // table index corresponding to m_nBaseSample

    LONGLONG m_total;       // sum of durations, in reftime
    LONGLONG m_tAtBase;     // total of durations at m_nBaseSample
};


