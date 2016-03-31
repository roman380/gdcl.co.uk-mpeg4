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
        ChunkStart = (DWORD)SwapLong(m_pSTCO + 8 + (nChunkThis * 4));
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

SampleTimes::SampleTimes()
: m_patmCTTS(NULL),
  m_patmSTTS(NULL)
{
}
bool 
SampleTimes::Parse(long scale, LONGLONG CTOffset, Atom* patmSTBL)
{
    m_scale = scale;            // track timescale units/sec
    m_CTOffset = CTOffset;      // offset to start of first sample in 100ns

    // basic duration table
    m_patmSTTS = patmSTBL->FindChild(FOURCC("stts"));
    if (!m_patmSTTS)
    {
        return false;
    }
    m_pSTTS = m_patmSTTS;
    m_nSTTS = SwapLong(m_pSTTS + 4);

	m_total = 0;
	for (int i = 0; i < m_nSTTS; i++)
	{
        long nEntries = SwapLong(m_pSTTS + 8 + (i * 8));
        long nDuration = SwapLong(m_pSTTS + 8 + 4 + (i * 8));
		m_total += (nEntries * nDuration);
	}
	m_total = TrackToReftime(m_total);

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
	SIZE_T nEntryCount = 0; 
    for(SIZE_T nIndex = 0; nIndex < (SIZE_T) m_nSTTS; nIndex++)
	{
        SIZE_T nCurrentEntryCount = (SIZE_T) SwapLong(m_pSTTS + 8 + (nIndex * 8));
		nEntryCount += nCurrentEntryCount;
	}
	if(nEntryCount)
	{
		pnTimes = (REFERENCE_TIME*) CoTaskMemAlloc(nEntryCount * sizeof *pnTimes);
		ASSERT(pnTimes);
		SIZE_T nEntryIndex = 0;
		REFERENCE_TIME nBaseTime = 0;
		for(SIZE_T nIndex = 0; nIndex < (SIZE_T) m_nSTTS; nIndex++)
		{
			const BYTE* pSttsEntry = m_pSTTS + 8 + (nIndex * 8);
			const SIZE_T nCurrentEntryCount = (SIZE_T) SwapLong(pSttsEntry + 0);
			const SIZE_T nCurrentDuration = (SIZE_T) SwapLong(pSttsEntry + 4);
			for(SIZE_T nCurrentEntryIndex = 0; nCurrentEntryIndex < nCurrentEntryCount; nCurrentEntryIndex++)
			{
				const SIZE_T nSampleIndex = nEntryIndex + nCurrentEntryIndex;
				const REFERENCE_TIME nSampleTime = nBaseTime + TrackToReftime(nCurrentEntryIndex * nCurrentDuration) + CTSOffset((long) nSampleIndex);
				pnTimes[nSampleIndex] = nSampleTime;
			}
			nEntryIndex += nCurrentEntryCount;
			nBaseTime += TrackToReftime(nCurrentEntryCount * nCurrentDuration);
		}
		ASSERT(nEntryIndex == nEntryCount);
	} else
		pnTimes = 0;
	return nEntryCount;
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
        long nEntries = SwapLong(m_pSTTS + 8 + (m_idx * 8));
        long nDuration = SwapLong(m_pSTTS + 8 + 4 + (m_idx * 8));
        if (nSample < (m_nBaseSample + nEntries))
        {
            LONGLONG tThis = m_tAtBase + TrackToReftime((nSample - m_nBaseSample) * nDuration);

            // allow for CTS Offset
            tThis += CTSOffset(nSample);

            return tThis;
        }
        m_tAtBase += TrackToReftime(nEntries * nDuration);
        m_nBaseSample += nEntries;
    }

    // should not get here
    return 0;
}

// offset from decode to composition time for this sample
LONGLONG 
SampleTimes::CTSOffset(long nSample) const
{
    if (!m_nCTTS)
    {
        return 0;
    }
    long nBase = 0;
    for (long i = 0; i < m_nCTTS; i++)
    {
        long nEntries = SwapLong(m_pCTTS + 8 + (i * 8));
        if (nSample < (nBase + nEntries))
        {
			LONGLONG tOffset = TrackToReftime(SwapLong(m_pCTTS + 8 + 4 + (i * 8)));
            return tOffset;
        }
        nBase += nEntries;
    }
    // should not get here
    return 0;
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

