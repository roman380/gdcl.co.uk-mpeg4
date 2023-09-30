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
    SampleSizes();

    bool Parse(Atom* patmSTBL);
    long Size(long nSample);
    long SampleCount()
    {
        return m_nSamples;
    }
    long MaxSize()
    {
        return m_nMaxSize;
    }
    LONGLONG Offset(long nSample);

	// support for old-style uncompressed audio, where fixedsize =1 means 1 sample
	void AdjustFixedSize(long nBytes);
private:
    Atom* m_patmSTSZ;
	AtomCache m_pBuffer;
    long m_nSamples;
    long m_nMaxSize;
    long m_nFixedSize;
    
    long m_nEntriesSTSC;
    long m_nChunks;
    bool m_bCO64;
    Atom* m_patmSTSC;
	AtomCache m_pSTSC;
    Atom* m_patmSTCO;
	AtomCache m_pSTCO;
};

// map of key samples
class KeyMap
{
public:
    KeyMap();
    ~KeyMap();

    bool Parse(Atom* patmSTBL);
    long SyncFor(long nSample);
	long Next(long nSample);
	SIZE_T Get(SIZE_T*& pnIndexes) const;

private:
    Atom* m_patmSTSS;
    const BYTE* m_pSTSS;
    long m_nEntries;
};

// time and duration of samples
// -- all times in 100ns units
class SampleTimes
{
public:

	class CompositionTimeOffset
	{
	public:
		UINT32 m_BaseSampleIndex;
		UINT32 m_SampleCount;
		UINT32 m_Value;

	public:
	// CompositionTimeOffset
		CompositionTimeOffset()
		{
		}
		CompositionTimeOffset(UINT32 BaseSampleIndex, UINT32 SampleCount, UINT32 Value) :
			m_BaseSampleIndex(BaseSampleIndex),
			m_SampleCount(SampleCount),
			m_Value(Value)
		{
		}
		static UINT32 LookupValue(const std::vector<CompositionTimeOffset>& Vector, UINT32 SampleIndex)
		{
			ASSERT(!Vector.empty());
			ASSERT(SampleIndex <= Vector.back().m_BaseSampleIndex + Vector.back().m_SampleCount);
			SIZE_T L = 0, R = Vector.size();
			for(; ; )
			{
				const CompositionTimeOffset& E = Vector[L];
				if(SampleIndex - E.m_BaseSampleIndex < E.m_SampleCount)
					return E.m_Value;
				ASSERT(R - L >= 2);
				const SIZE_T C = (L + R) / 2;
				if(SampleIndex < Vector[C].m_BaseSampleIndex)
					R = C;
				else
					L = C;
			}
			ASSERT(FALSE);
			return 0;
		}
		static UINT32 IncrementalLookupValue(const std::vector<CompositionTimeOffset>& Vector, SIZE_T& Index, UINT32 SampleIndex)
		{
			if(Vector.empty())
				return 0;
			ASSERT(Index < Vector.size());
			const CompositionTimeOffset& E0 = Vector[Index];
			ASSERT(SampleIndex >= E0.m_BaseSampleIndex);
			if(SampleIndex - E0.m_BaseSampleIndex < E0.m_SampleCount)
				return E0.m_Value;
			const CompositionTimeOffset& E1 = Vector[++Index];
			ASSERT(SampleIndex >= E1.m_BaseSampleIndex);
			ASSERT(SampleIndex - E1.m_BaseSampleIndex < E1.m_SampleCount);
			return E1.m_Value;
		}
	};

public:
	bool Parse(long scale, LONGLONG CTOffset, Atom* patmSTBL);

    long DTSToSample(LONGLONG tStart);
	SIZE_T Get(REFERENCE_TIME*& pnTimes) const;
    LONGLONG SampleToCTS(long nSample);
    LONGLONG Duration(long nSample);
    LONGLONG CTSOffset(long nSample) const;
	void GetCompositionTimeOffsetVector(std::vector<CompositionTimeOffset>& CompositionTimeOffsetVector) const;
    long CTSToSample(LONGLONG tStart);
    LONGLONG TotalDuration()					{ return m_total; }

    LONGLONG TrackToReftime(LONGLONG nTrack) const;
    bool HasCTSTable()  { return m_nCTTS > 0; }
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


