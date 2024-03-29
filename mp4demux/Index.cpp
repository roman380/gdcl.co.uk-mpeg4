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


#include "stdafx.h"
#include "Mpeg4.h"
#include "Index.h"

// sample count and sizes ------------------------------------------------

SampleSizes::SampleSizes()
: m_nSamples(0),
  m_nMaxSize(0),
  m_nFixedSize(0),
  m_patmSTSZ(NULL),
  m_patmSTSC(NULL),
  m_patmSTCO(NULL)
{}
     
bool 
SampleSizes::Parse(Atom* patmSTBL)
{
    // need to locate three inter-related tables
    // stsz, stsc and stco/co64

    m_patmSTSZ = patmSTBL->FindChild(FOURCC("stsz"));
    if (!m_patmSTSZ)
    {
        return false;
    }
    m_pBuffer = m_patmSTSZ;
    if (m_pBuffer[0] != 0)  // check version 0
    {
        return false;
    }

    m_nFixedSize = SwapLong(m_pBuffer+4);
    m_nSamples = SwapLong(m_pBuffer+8);

    m_nMaxSize = m_nFixedSize;
    if (m_nFixedSize == 0)
    {
        // variable size -- need to scan whole table to find largest
        for (long  i = 0; i < m_nSamples; i++)
        {
            long cThis = SwapLong(m_pBuffer + 12 + (i * 4));
            if (cThis > m_nMaxSize)
            {
                m_nMaxSize = cThis;
            }
        }
    }

    m_patmSTSC = patmSTBL->FindChild(FOURCC("stsc"));
    if (!m_patmSTSC)
    {
        return false;
    }
    m_pSTSC = m_patmSTSC;
    m_nEntriesSTSC = SwapLong(m_pSTSC+4);

    m_patmSTCO = patmSTBL->FindChild(FOURCC("stco"));
    if (m_patmSTCO)
    {
        m_bCO64 = false;
    } else {
        m_patmSTCO = patmSTBL->FindChild(FOURCC("co64"));
        if (!m_patmSTCO)
        {
            return false;
        }
        m_bCO64 = true;
    }
    m_pSTCO = m_patmSTCO;
    m_nChunks = SwapLong(m_pSTCO + 4);
    return true;
}

long 
SampleSizes::Size(long nSample)
{
    long cThis = m_nFixedSize;
    if ((cThis == 0) && (nSample < m_nSamples))
    {
        cThis = SwapLong(m_pBuffer + 12 + (nSample * 4));
    }
    return cThis;
}
                 
LONGLONG 
SampleSizes::Offset(long nSample)
{
    // !! consider caching prev entry and length of chunk
    // and just adding on sample size until chunk count reached

    // each entry in this table describes a
    // run of chunks with the same number of samples.
    // find which entry covers this sample, and
    // the total samples covered by previous entries
    long nSampleBase = 0;
    long nChunkThis = 0, nSamplesPerChunk = 1;

    for (long i = 0; i < m_nEntriesSTSC; i++)
    {
        // remember that chunk numbers are 1-based

        nChunkThis = SwapLong(m_pSTSC + 8 + (12 * i)) - 1;
        nSamplesPerChunk = SwapLong(m_pSTSC + 8 + (12 * i) + 4);
        
        long nChunkNext = SwapLong(m_pSTSC + 8 + (12 * i) + 12) - 1;
        long nSampleBaseNext = nSampleBase + (nChunkNext - nChunkThis)*nSamplesPerChunk;
        if ((i == m_nEntriesSTSC-1) || (nSample < nSampleBaseNext))
        {
            break;
        } else {
            nSampleBase = nSampleBaseNext;
        }
    }

    // found correct entry -- work out chunk number and sample number within chunk
    long nChunkOffset = (nSample - nSampleBase) / nSamplesPerChunk; // whole chunks 
    long nSampleOffset = (nSample - nSampleBase) - (nChunkOffset * nSamplesPerChunk);
    nChunkThis += nChunkOffset;

    // add up sizes of previous samples to get offset into this chunk
    LONGLONG nByteOffset = 0;
    if (m_nFixedSize != 0)
    {
        nByteOffset = nSampleOffset * m_nFixedSize;
    } else {
        for (long i = nSample - nSampleOffset; i < nSample; i++)
        {
            nByteOffset += Size(i);
        }
    }

    // start location of this chunk
    if (nChunkThis >= m_nChunks)
    {
        return 0;
    }
    LONGLONG ChunkStart;
    if (m_bCO64)
    {
        ChunkStart = SwapI64(m_pSTCO + 8 + (nChunkThis * 8));
    } else {
        ChunkStart = SwapLong(m_pSTCO + 8 + (nChunkThis * 4));
    }
    return ChunkStart + nByteOffset;
}

void 
SampleSizes::AdjustFixedSize(long nBytes)
{
	m_nFixedSize = nBytes;

	// set max buffer size based on contig samples
	for (long i = 0; i < m_nEntriesSTSC; i++)
	{
		long nSamplesPerChunk = SwapLong(m_pSTSC + 8 + (12 * i) + 4);
		long cThis = nSamplesPerChunk * m_nFixedSize;
		if (cThis > m_nMaxSize)
		{
			m_nMaxSize = cThis;
		}
	}
}

// --- sync sample map --------------------------------

KeyMap::KeyMap()
: m_patmSTSS(NULL),
  m_pSTSS(NULL),
  m_nEntries(0)
{
}

KeyMap::~KeyMap()
{
    if (m_patmSTSS)
    {
        m_patmSTSS->BufferRelease();
    }
}

bool 
KeyMap::Parse(Atom* patmSTBL)
{
    m_patmSTSS = patmSTBL->FindChild(FOURCC("stss"));
    if (!m_patmSTSS)
    {
		// no key map -- so all samples are key
        return true;
    }
    m_pSTSS = m_patmSTSS->Buffer() + m_patmSTSS->HeaderSize();
    if (m_pSTSS[0] != 0)  // check version 0
    {
        return false;
    }

    m_nEntries = SwapLong(m_pSTSS+4);

    return true;
}

long 
KeyMap::SyncFor(long nSample)
{
    if (!m_patmSTSS || (m_nEntries == 0))
    {
        // no table -- all samples are key samples
        return nSample;
    }

    // find preceding key
    long nPrev = 0;
    for (long i = 0; i < m_nEntries; i++)
    {
        long nThis = SwapLong(m_pSTSS + 8 + (i * 4))-1;
        if (nThis > nSample)
        {
            break;
        }
        nPrev = nThis;
    }
    return nPrev;
}

long 
KeyMap::Next(long nSample)
{
    if (!m_patmSTSS || (m_nEntries == 0))
    {
        // no table -- all samples are key samples
        return nSample + 1;
    }

    // find next key after nSample
    for (long i = 0; i < m_nEntries; i++)
    {
        long nThis = SwapLong(m_pSTSS + 8 + (i * 4))-1;
        if (nThis > nSample)
        {
			return nThis;
        }
    }
    return 0;
}

SIZE_T KeyMap::Get(SIZE_T*& pnIndexes) const
{
	ASSERT(!pnIndexes);
	if(m_nEntries)
	{
		pnIndexes = (SIZE_T*) CoTaskMemAlloc(m_nEntries * sizeof *pnIndexes);
		ASSERT(pnIndexes);
	    for(SIZE_T nEntryIndex = 0; nEntryIndex < (SIZE_T) m_nEntries; nEntryIndex++)
		{
	        const SIZE_T nIndex = SwapLong(m_pSTSS + 8 + (nEntryIndex * 4))-1;
			pnIndexes[nEntryIndex] = nIndex;
		}
	}
	return (SIZE_T) m_nEntries;
}

// ----- times index ----------------------------------

bool SampleTimes::Parse(long scale, LONGLONG CTOffset, Atom* patmSTBL)
{
    m_scale = scale;            // track timescale units/sec
    m_CTOffset = CTOffset;      // offset to start of first sample in 100ns

    m_patmSTTS = patmSTBL->FindChild(FOURCC("stts")); // basic duration table
    if (!m_patmSTTS)
        return false;
    m_pSTTS = m_patmSTTS;
    m_nSTTS = SwapLong(m_pSTTS + 4);

	LONGLONG total = 0;
	for(size_t i = 0; i < m_nSTTS; i++)
	{
        auto const EntryCount = SwapLong(m_pSTTS + 8 + (i * 8));
        auto const Duration = SwapLong(m_pSTTS + 8 + 4 + (i * 8));
		total += static_cast<LONGLONG>(static_cast<uint64_t>(EntryCount) * Duration);
	}
	m_total = TrackToReftime(total);

    // optional decode-to-composition offset table
    m_patmCTTS = patmSTBL->FindChild(FOURCC("ctts"));
    if (m_patmCTTS)
    {
        m_pCTTS = m_patmCTTS;
        m_nCTTS = SwapLong(m_pCTTS + 4);
    } else {
        m_pCTTS = NULL;
        m_nCTTS = 0;
    }

    // reset cache data
    m_idx = 0;
    m_nBaseSample = 0;
    m_tAtBase = m_CTOffset;

    return true;
}

long SampleTimes::CTSToSample(LONGLONG tStart)
{
	if (!m_nCTTS)
	{
		return DTSToSample(tStart);
	}

	// we have a RLE list of durations and a RLE list of CTS offsets.
	// maybe start from a DTS time a little earlier, and step forward?
	LONGLONG pos = tStart;
	if (tStart > 0)
	{
		if (tStart < (UNITS/2))
		{
			pos = 0;
		}
		else
		{
			pos = tStart - (UNITS/2);
		}
	}
	long n = DTSToSample(pos); 
	for (;;)
	{
		LONGLONG cts = SampleToCTS(n);
		if (cts < 0)
		{
			return -1;
		}
		LONGLONG dur = Duration(n);
		if (cts > tStart)
		{
			return n;
		}
		if ((cts <= tStart) && ((cts + dur) > tStart))
		{
			return n;
		}
		if (dur == 0)
		{
			return -1;
		}
		n++;
	}
}

long 
SampleTimes::DTSToSample(LONGLONG tStart)
{
    // find the sample containing this composition time
    // by adding up the durations of individual samples
    // until we reach it

    // start at cache position unless we need a time before it
    if (tStart < m_tAtBase)
    {
        m_idx = 0;
        m_nBaseSample = 0;
        m_tAtBase = m_CTOffset;
        if (tStart < m_tAtBase)
        {
            return 0;
        }
    }
    for (; m_idx < m_nSTTS; m_idx++)
    {
        long nEntries = SwapLong(m_pSTTS + 8 + (m_idx * 8));
        long nDuration = SwapLong(m_pSTTS + 8 + 4 + (m_idx * 8));
		if (nDuration == 0)
		{
			nDuration = 1;
		}
        LONGLONG tLimit = m_tAtBase + TrackToReftime(nEntries * nDuration) + CTSOffset(m_nBaseSample + nEntries);
        if (tStart < tLimit || m_idx + 1 == m_nSTTS)
        {
			LONGLONG trackoffset = ReftimeToTrack(tStart - m_tAtBase);
            return m_nBaseSample + long(trackoffset / nDuration);
        }
        m_tAtBase += TrackToReftime(nEntries * nDuration);
        m_nBaseSample += nEntries;
    }

    // should not get here?
    return 0;
}

SIZE_T SampleTimes::Get(REFERENCE_TIME*& pnTimes) const
{
	ASSERT(!pnTimes);
	size_t EntryCount = 0; 
    for(size_t Index = 0; Index < m_nSTTS; Index++)
		EntryCount += static_cast<size_t>(SwapLong(m_pSTTS + 8 + (Index * 8)));
	if(EntryCount)
	{
		std::vector<CompositionTimeOffset> CompositionTimeOffsetVector;
		GetCompositionTimeOffsetVector(CompositionTimeOffsetVector);
		SIZE_T CompositionTimeOffsetVectorIndex = 0;
		pnTimes = (REFERENCE_TIME*) CoTaskMemAlloc(EntryCount * sizeof *pnTimes);
		ASSERT(pnTimes);
		size_t EntryIndex = 0;
		REFERENCE_TIME BaseTime = 0;
		for(size_t Index = 0; Index < m_nSTTS; Index++)
		{
			const BYTE* pSttsEntry = m_pSTTS + 8 + (Index * 8);
			auto const CurrentEntryCount = static_cast<size_t>(SwapLong(pSttsEntry + 0));
			auto const CurrentDuration = static_cast<size_t>(SwapLong(pSttsEntry + 4));
			for(size_t CurrentEntryIndex = 0; CurrentEntryIndex < CurrentEntryCount; CurrentEntryIndex++)
			{
				auto const SampleIndex = EntryIndex + CurrentEntryIndex;
				pnTimes[SampleIndex] = BaseTime + TrackToReftime(CurrentEntryIndex * CurrentDuration) + CompositionTimeOffset::IncrementalLookupValue(CompositionTimeOffsetVector, CompositionTimeOffsetVectorIndex, static_cast<UINT32>(SampleIndex));
			}
			EntryIndex += CurrentEntryCount;
			BaseTime += TrackToReftime(CurrentEntryCount * CurrentDuration);
		}
		ASSERT(EntryIndex == EntryCount);
	} else
		pnTimes = 0;
	return EntryCount;
}

LONGLONG 
SampleTimes::SampleToCTS(long nSample)
{
    // calculate CTS for this sample by adding durations

    // start at cache position unless it is too late
    if (nSample < m_nBaseSample)
    {
        m_idx = 0;
        m_nBaseSample = 0;
        m_tAtBase = m_CTOffset;
    }
    for (; m_idx < m_nSTTS; m_idx++)
    {
        auto const SampleCount = static_cast<long>(SwapLong(m_pSTTS + 8 + (m_idx * 8)));
        auto const Duration = static_cast<long>(SwapLong(m_pSTTS + 8 + 4 + (m_idx * 8)));
        if (nSample < (m_nBaseSample + SampleCount))
            return m_tAtBase + TrackToReftime(static_cast<LONGLONG>(nSample - m_nBaseSample) * Duration) + CTSOffset(nSample);
        m_tAtBase += TrackToReftime(SampleCount * Duration);
        m_nBaseSample += SampleCount;
    }

    ASSERT(false); // should not get here
    return 0;
}

// offset from decode to composition time for this sample
LONGLONG 
SampleTimes::CTSOffset(long SampleIndex) const
{
	ASSERT(SampleIndex >= 0);
    if(!m_nCTTS)
        return 0;
    UINT32 BaseEntryCount = 0;
	const UINT8* Data = (const UINT8*) (m_pCTTS + 8);
    for(size_t TableIndex = 0; TableIndex < m_nCTTS; TableIndex++)
    {
        auto const EntryCount = SwapLong(Data + (TableIndex * 8));
        if(static_cast<UINT32>(SampleIndex) < (BaseEntryCount + EntryCount))
            return TrackToReftime(SwapLong(Data + (TableIndex * 8) + 4));
        BaseEntryCount += EntryCount;
    }
	//ASSERT(FALSE); // should not get here
    return 0;
}

void SampleTimes::GetCompositionTimeOffsetVector(std::vector<CompositionTimeOffset>& CompositionTimeOffsetVector) const
{
	ASSERT(CompositionTimeOffsetVector.empty());
    if(m_nCTTS)
	{
		const UINT8* Data = (const UINT8*) (m_pCTTS + 8);
		CompositionTimeOffsetVector.reserve(m_nCTTS);
		UINT32 BaseEntryCount = 0;
		for(size_t TableIndex = 0; TableIndex < m_nCTTS; TableIndex++)
		{
			auto const EntryCount = SwapLong(Data + (TableIndex * 8));
			auto const Value = SwapLong(Data + (TableIndex * 8) + 4);
			CompositionTimeOffsetVector.emplace_back(BaseEntryCount, EntryCount, Value);
	        BaseEntryCount += EntryCount;
		}
		ASSERT(CompositionTimeOffsetVector.size() == m_nCTTS);
	}
}

LONGLONG 
SampleTimes::Duration(long nSample)
{
    // the entries in stts give the duration of each sample
    // as a series of pairs
    //      < count of samples, duration of these samples>

    // start at cache position unless too far along
    // -- nb, we need to sum the durations so that
    // the updated cache position is shared by SampleToCTS
    if (nSample < m_nBaseSample)
    {
        m_idx = 0;
        m_nBaseSample = 0;
        m_tAtBase = m_CTOffset;
    }
    for (; m_idx < m_nSTTS; m_idx++)
    {
        long nEntries = SwapLong(m_pSTTS + 8 + (m_idx * 8));
        long nDuration = SwapLong(m_pSTTS + 8 + 4 + (m_idx * 8));
        LONGLONG tDur = TrackToReftime(nDuration);
        if (nSample < (m_nBaseSample + nEntries))
        {
            // requested sample is within this range
            return TrackToReftime(nDuration);
        }
        m_tAtBase += (nEntries * tDur);
        m_nBaseSample += nEntries;
    }
    // ? should not get here, since all samples should be covered
    return 0;
}

LONGLONG 
SampleTimes::TrackToReftime(LONGLONG nTrack) const
{
    // convert times in the track timescale (m_scale units/sec) to 100ns
    return REFERENCE_TIME(nTrack) * UNITS / LONGLONG(m_scale);
}

LONGLONG SampleTimes::ReftimeToTrack(LONGLONG reftime)
{
	return ((reftime * m_scale) + (UNITS/2)) / UNITS;
}

