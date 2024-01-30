//
// Mpeg4.cpp: implementation of Mpeg-4 parsing classes
//
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
#include "ElemType.h"
#include "Index.h"

Atom::Atom(AtomReader* pReader, LONGLONG llOffset, LONGLONG llLength, DWORD type, long cHeader)
: m_pSource(pReader),
  m_llOffset(llOffset),
  m_cBufferRefCount(0),
  m_cHeader(cHeader),
  m_llLength(llLength),
  m_type(type)
{
}

HRESULT 
Atom::Read(LONGLONG llOffset, long cBytes, BYTE* pBuffer)
{
    HRESULT hr = S_OK;
    if (IsBuffered())
    {
        const BYTE* pSrc = Buffer() + long(llOffset);
        CopyMemory(pBuffer,  pSrc,  cBytes);
        BufferRelease();
    } else {
        hr = m_pSource->Read(llOffset + m_llOffset, cBytes, pBuffer);
    }
    return hr;
}
    
void 
Atom::ScanChildrenAt(LONGLONG llOffset)
{
    llOffset += m_cHeader;

    while (llOffset < m_llLength)
    {
        BYTE hdr[8];
        long cHeader = 8;
        Read(llOffset, 8, hdr);
        LONGLONG llLength = SwapLong(hdr);
        DWORD type = SwapLong(hdr + 4);
        if (llLength == 1)
        {
            Read(llOffset + 8, 8, hdr);
            llLength = SwapI64(hdr);
            cHeader += 8;
        }
        else if (llLength == 0)
        {
            // whole remainder
            llLength = m_llLength - llOffset;
        }
        if (type == FOURCC("uuid"))
        {
            cHeader += 16;
        }
        
        if ((llLength < 0) || ((llOffset + llLength) > (m_llLength)))
        {
            break;
        }

        AtomPtr pChild = new Atom(this, llOffset, llLength, type, cHeader);
        m_Children.push_back(pChild);

        llOffset += llLength;
    }
}

long 
Atom::ChildCount()
{
    if (m_Children.size() == 0)
    {
        ScanChildrenAt(0);
    }
    return (long)m_Children.size();
}

Atom* 
Atom::Child(long nChild)
{
    if (nChild >= ChildCount())
    {
        return NULL;
    }
    list<AtomPtr>::iterator it = m_Children.begin();
    while ((nChild-- > 0) && (it != m_Children.end()))
    {
        it++;
    }
    if (it == m_Children.end())
    {
        return NULL;
    }
    return *it;
}

Atom* 
Atom::FindChild(DWORD fourcc)
{
    if (ChildCount() == 0) // force enum of children
    {
        return NULL;
    }

    list<AtomPtr>::iterator it = m_Children.begin();
    while (it != m_Children.end())
    {
        Atom* pChild = *it;
        if (pChild->Type() == fourcc)
        {
            return pChild;
        }
        it++;
    }
    return NULL;
}

bool 
Atom::IsBuffered()
{
    if (m_cBufferRefCount <= 0)
    {
        return m_pSource->IsBuffered();
    } 
    return true;
}

const BYTE* 
Atom::Buffer()
{
    if (m_llLength > 0x7fffffff)
    {
        return NULL;
    }

    if (!m_Buffer)
    {
        if (m_pSource->IsBuffered() && (m_llOffset < 0x7fffffff))
        {
            return m_pSource->Buffer() + long(m_llOffset);
        }
        m_Buffer = new BYTE[long(m_llLength)];
        Read(0, long(m_llLength), m_Buffer);
    }
    m_cBufferRefCount++;
    return m_Buffer;
}

void 
Atom::BufferRelease()
{
    if (--m_cBufferRefCount == 0)
    {
        m_Buffer = NULL;
    }
}

// -- main movie header, contains list of tracks ---------------

extern bool g_ElstMediaTimeTruncation;

Movie::Movie(Atom* pRoot) : 
    m_pRoot(pRoot)
{
    Atom* pMovie = m_pRoot->FindChild(FOURCC("moov"));
    if(!pMovie)
        return;
    Atom* patmHdr = pMovie->FindChild(FOURCC("mvhd"));
    if (patmHdr)
    {
        AtomCache phdr(patmHdr);
        if (phdr[0] == 1)   // version 0 or 1
        {
            m_scale = SwapLong(phdr + 20);
            m_duration = SwapI64(phdr + 24);
        } else {
            m_scale = SwapLong(phdr + 12);
            m_duration = SwapLong(phdr + 16);
        }
        m_tDuration = MFllMulDiv(m_duration, 1'000'0000ll, m_scale, 0);
    }
    long TrackIndexEx = 0;
    for(long TrackIndex = 0; TrackIndex < pMovie->ChildCount(); TrackIndex++)
    {
        Atom* patm = pMovie->Child(TrackIndex);
        if(patm->Type() != FOURCC("trak") && patm->Type() != FOURCC("cctk"))
            continue;
        MovieTrackPtr pTrack = new MovieTrack(patm, this, TrackIndexEx++);
        if(pTrack->Valid())
            m_Tracks.push_back(pTrack);
        else
			m_invalidTrackCount++;
    }
    Atom* UdtaHeader = pMovie->FindChild(FOURCC("udta"));
    if(UdtaHeader)
    {
        try
        {
            AtomCache Udta(UdtaHeader);
            // TODO: Meta atom and comment; see mp4mux\MovieWriter.cpp#L130
            Atom* XtraHeader = UdtaHeader->FindChild(FOURCC("Xtra"));
            if(XtraHeader)
            {
                std::vector<uint8_t> Data;
                Data.resize(static_cast<size_t>(XtraHeader->Length() - XtraHeader->HeaderSize()));
                THROW_IF_FAILED(XtraHeader->Read(XtraHeader->HeaderSize(), static_cast<long>(Data.size()), Data.data()));
                for(size_t Position = 0; Position < Data.size(); )
                {
                    auto const AttributePosition = Position;
                    uint32_t AttributeSize;
                    THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_CORRUPT), Position + sizeof AttributeSize > Data.size());
                    AttributeSize = SwapLong(Data.data() + Position);
                    Position += sizeof AttributeSize;
                    THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_CORRUPT), Position + AttributeSize - sizeof AttributeSize > Data.size());
                    THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_CORRUPT), AttributeSize < 4 + 10);
                    uint32_t NameLength;
                    THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_CORRUPT), Position + sizeof NameLength > Data.size());
                    NameLength = SwapLong(Data.data() + Position);
                    Position += sizeof NameLength;
                    THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_CORRUPT), Position + NameLength * sizeof (char) > Data.size());
                    std::string Name;
                    Name.resize(NameLength);
                    std::memcpy(Name.data(), Data.data() + Position, NameLength * sizeof (char));
                    Position += NameLength * sizeof (char);
                    uint32_t One;
                    uint32_t ValueSize;
                    uint16_t ValueType;
                    THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_CORRUPT), Position + sizeof One + sizeof ValueSize + sizeof ValueType > Data.size());
                    One = SwapLong(Data.data() + Position);
                    Position += sizeof One;
                    if(One == 1u)
                    {
                        ValueSize = SwapLong(Data.data() + Position);
                        Position += sizeof ValueSize;
                        ValueType = Swap2Bytes(*reinterpret_cast<uint16_t const*>(Data.data() + Position));
                        Position += sizeof ValueType;
                        if(ValueType == VT_BSTR)
                        {
                            THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_CORRUPT), Position + ValueSize - (sizeof ValueSize + sizeof ValueType) > Data.size());
                            std::wstring Value;
                            Value.resize((ValueSize - (sizeof ValueSize + sizeof ValueType)) / sizeof (wchar_t));
                            std::memcpy(Value.data(), Data.data() + Position, Value.length() * sizeof (wchar_t));
                            Position += ValueSize - (sizeof ValueSize + sizeof ValueType);
                            if(!Value.empty() && Value.back() == 0)
                                Value.erase(Value.length() - 1);
                            m_AttributeVector.emplace_back(std::make_pair(Name, Value));
                            WI_ASSERT(Position == AttributePosition + AttributeSize);
                        }
                    }
                    Position = AttributePosition + AttributeSize;
                }
            }
        }
        CATCH_LOG();
    }
}
    
HRESULT 
Movie::ReadAbsolute(LONGLONG llPos, BYTE* pBuffer, long cBytes)
{
    return m_pRoot->Read(llPos, cBytes, pBuffer);
}


// ------------------------------------------------------------------


MovieTrack::MovieTrack(Atom* pAtom, Movie* pMovie, long idx) : 
    m_pMovie(pMovie),
    m_idx(idx)
{
    // check version/flags entry for track header
    Atom* pHdr = pAtom->FindChild(FOURCC("tkhd"));
    if (pHdr != NULL)
    {
        BYTE verflags[4];
        pHdr->Read(pHdr->HeaderSize(), 4, verflags);
        if ((verflags[3] & 1) == 1)      // enabled?
        {
            // edit list may contain offset for first sample
            REFERENCE_TIME tFirst = 0;
            Atom* patmEDTS = pAtom->FindChild(FOURCC("edts"));
            if (patmEDTS != NULL)
            {
                LONGLONG first = ParseEDTS(patmEDTS);
                // convert from movie scale to 100ns
                tFirst = first * UNITS / pMovie->Scale();
            }

            Atom* patmMDIA = pAtom->FindChild(FOURCC("mdia"));
            if (patmMDIA && ParseMDIA(patmMDIA, tFirst))
            {
                // valid track -- make a name for the pin to use
				ostringstream strm;
				strm << m_pType->ShortName() << " ";
                strm << m_idx+1;
                m_strName = strm.str();

                m_pRoot = pAtom;

				// sum up and convert ELST for efficiency later
				if (m_Edits.size() > 0)
				{
					LONGLONG sumDurations = 0;
					for (vector<EditEntry>::iterator it = m_Edits.begin(); it != m_Edits.end(); it++)
					{
						// duration is in movie scale; offset is in track scale. 
						it->duration = it->duration * UNITS / m_pMovie->Scale();
						if (it->offset > 0)
						{
							it->offset = TimesIndex()->TrackToReftime(it->offset);
						}
						it->sumDurations = sumDurations;
						sumDurations += it->duration;
					}
				}
				else
				{
					// create dummy edit
					EditEntry e;
					e.offset = 0;
					e.sumDurations = 0;
					e.duration = TimesIndex()->TotalDuration();
					m_Edits.push_back(e);
				}
            }
        }
    }
}

LONGLONG 
MovieTrack::ParseEDTS(Atom* patm)
{
    Atom* Elst = patm->FindChild(FOURCC("elst"));
    if(Elst)
    {
		AtomCache ElstCache = Elst;
		uint32_t const entry_count = SwapLong(ElstCache + 4);
		for(uint32_t Index = 0; Index < entry_count; Index++)
        {
			EditEntry Entry;
            if(ElstCache[0] == 0)
            {
				Entry.duration = SwapLong(ElstCache + 8 + (Index * 12));
				// WARN: elst atom has an explicitly defined offset -1 for an empty edit, should we still treat the value as signed otherwise?
				Entry.offset = (INT32) SwapLong(ElstCache + 8 + 4 + (Index * 12));
                if(g_ElstMediaTimeTruncation && Entry.offset == 0xFFFF)
                {
                    // WARN: Fixing truncated at 16-bit -1 value via explicit compatibility shim; see https://github.com/roman380/gdcl.co.uk-mpeg4/pull/49/files#r1469965874
                    Entry.offset = -1;
                }
			} else 
            {
				Entry.duration = SwapI64(ElstCache + 8 + (Index * 20));
				Entry.offset = (INT64) SwapI64(ElstCache + 8 + 8 + (Index * 20));
			}
			m_Edits.push_back(Entry);
		}
    }
	// always return 0 for start offset. We now add the ELST above this layer, so the TimesIndex()
	// should return the actual start of the media, not an offset time. Thus use 0 here.
    return 0;
}

// parse the track type information
bool
MovieTrack::ParseMDIA(Atom* patm, REFERENCE_TIME tFirst)
{
    // get track timescale from mdhd
    Atom* patmMDHD = patm->FindChild(FOURCC("mdhd"));
    if (!patmMDHD)
    {
        return false;
    }
    AtomCache pMDHD(patmMDHD);
    if (pMDHD[0] == 1)  // version 0 or 1
    {
        m_scale = SwapLong(pMDHD + 20);
    } else {
        m_scale = SwapLong(pMDHD + 12);
    }


    // locate and parse the ES_Descriptor
    // that will give us the media type for this
    // track. That is in minf/stbl/stsd

    Atom* patmMINF = patm->FindChild(FOURCC("minf"));
    if (!patmMINF)
    {
        return false;
    }
    m_patmSTBL = patmMINF->FindChild(FOURCC("stbl"));
    if (!m_patmSTBL)
    {
        return false;
    }

    // initialize index tables
    m_pSizes = new SampleSizes;
    if ((!m_pSizes->Parse(m_patmSTBL) || (m_pSizes->SampleCount() <= 0)))
    {
        return false;
    }
    m_pKeyMap = new KeyMap;
    if (!m_pKeyMap->Parse(m_patmSTBL))
    {
        return false;
    }

    m_pTimes = new SampleTimes;
    if (!m_pTimes->Parse(m_scale, tFirst, m_patmSTBL))
    {
        return false;
    }

    // now index is ready, we can calculate average frame duration
    // for the media type
    REFERENCE_TIME tFrame = m_pMovie->Duration() / m_pSizes->SampleCount();

    Atom* pSTSD = m_patmSTBL->FindChild(FOURCC("stsd"));
    if (!pSTSD || !ParseSTSD(tFrame, pSTSD))
    {
        return false;
    }

	// check for old-format uncomp audio
	if ((m_pType->StreamType() == Audio_WAVEFORMATEX) &&
		(m_pSizes->Size(0) == 1))
	{
		CMediaType mt;
		m_pType->GetType(&mt, 0);
		WAVEFORMATEX* pwfx = (WAVEFORMATEX*)mt.Format();
		m_pSizes->AdjustFixedSize(pwfx->nBlockAlign);
		m_bOldFixedAudio = true;
	}

	if (IsVideo())
	{
		// attempt to normalise the frame rate

		// first average the first few, discarding outliers
		int cFrames = min(m_pSizes->SampleCount(), 60L);
		REFERENCE_TIME total = 0;
		int cCounted = 0;
		// outliers are beyond +/- 20%
		REFERENCE_TIME tMin = tFrame * 80 / 100;
		REFERENCE_TIME tMax = tFrame * 120 / 100;
		for (int i = 0; i < cFrames; i++)
		{
			REFERENCE_TIME tDur = m_pTimes->Duration(i);
			if ((tDur > tMin) && (tDur < tMax))
			{
				total += tDur;
				cCounted++;
			}
		}
		if (cCounted > 2)
		{
			tFrame = (total / cCounted);

			LONGLONG fpsk = (UNITS * 1000 / tFrame);
			if (fpsk > 23950)
			{
				if (fpsk < 23988)
				{
					fpsk = 23976;
				}
				else if (fpsk < 24100)
				{
					fpsk = 24000;
				}
				else if (fpsk > 24800)
				{
					if (fpsk < 25200)
					{
						fpsk = 25000;
					} 
					else if (fpsk > 29800)
					{
						if (fpsk < 29985)
						{
							fpsk = 29970;
						}
						else if (fpsk < 30200)
						{
							fpsk = 30000;
						}
					}
				}
			}
			tFrame = (UNITS * 1000 / fpsk);
			m_pType->SetRate(tFrame);
		}

	}

    return true;
}

// locate and parse the ES_Descriptor    
bool 
MovieTrack::ParseSTSD(REFERENCE_TIME tFrame, Atom* pSTSD)
{
    // We don't accept files with format changes mid-track,
    // so there must be only one descriptor entry 
    // -- and the spec only defines version 0, so validate both these.
    BYTE ab[8];
    pSTSD->Read(pSTSD->HeaderSize(), 8, ab);
    if ((ab[0] !=0) || (SwapLong(ab+4) != 1))
    {
        return false;
    }
    pSTSD->ScanChildrenAt(8);
    Atom* patm = pSTSD->Child(0);
    if (!patm)
    {
        return false;
    }

    m_pType = new ElementaryType();
    if (!m_pType->Parse(tFrame, patm))
    {
        return false;
    }
    return true;
}

bool 
MovieTrack::IsVideo()
{
    return m_pType->IsVideo();
}

bool 
MovieTrack::GetType(CMediaType* pmt, int nType)
{
    return m_pType->GetType(pmt, nType);
}
    
bool 
MovieTrack::SetType(const CMediaType* pmt)
{
    return m_pType->SetType(pmt);
}

FormatHandler* 
MovieTrack::Handler()
{
    return m_pType->Handler();
}

HRESULT 
MovieTrack::ReadSample(long nSample, BYTE* pBuffer, long cBytes)
{
    LONGLONG llPos = m_pSizes->Offset(nSample);

    // llPos is absolute within referenced file
    return GetMovie()->ReadAbsolute(llPos, pBuffer, cBytes);
}

bool MovieTrack::CheckInSegment(REFERENCE_TIME tNext, bool bSyncBefore, size_t* pnSegment, long* pnSample)
{
	for (size_t idx = 0; idx < m_Edits.size(); idx++)
	{
		EditEntry* it = &m_Edits[idx];
        LONGLONG endDuration = it->sumDurations + it->duration;
        if (tNext < endDuration)
		{
			if (it->offset == -1)
			{
				// empty edit; skip to next segment start
				tNext = it->sumDurations + it->duration;
			}
			else
			{
				// map to sample number
				LONGLONG rCTS = tNext - it->sumDurations;
				LONGLONG trackCTS = rCTS + it->offset;
				long n = TimesIndex()->CTSToSample(trackCTS);
				if (n < 0)
				{
					return false;
				}
				if (bSyncBefore)
				{
					n = GetKeyMap()->SyncFor(n);
				}

				*pnSample = n;
				*pnSegment = idx;
				return true;
			}
		}
	}

	return false;
}

void MovieTrack::GetTimeBySegment(
	long nSample, 
	size_t segment, 
	REFERENCE_TIME* ptStart, 
	REFERENCE_TIME* pDuration)
{
	EditEntry* it = &m_Edits[segment];
	REFERENCE_TIME cts = TimesIndex()->SampleToCTS(nSample);
	REFERENCE_TIME relCTS = cts - it->offset;

	REFERENCE_TIME duration = TimesIndex()->Duration(nSample);
	if ((relCTS + duration) > it->duration)
	{
		duration = it->duration - relCTS;
	}
	*ptStart = relCTS + it->sumDurations;
	*pDuration = duration;
}

bool MovieTrack::NextBySegment(long* pnSample, size_t* psegment)
{
	int n = *pnSample + 1;
	EditEntry* it = &m_Edits[*psegment];


	if (n < SizeIndex()->SampleCount())
	{
		REFERENCE_TIME cts = TimesIndex()->SampleToCTS(n);
		REFERENCE_TIME relCTS = cts - it->offset;
		if (relCTS < it->duration)
		{
			*pnSample = n;
			return true;
		}
	}
	REFERENCE_TIME tEdit = it->duration + it->sumDurations;
	return CheckInSegment(tEdit, false, psegment, pnSample);
}

SIZE_T MovieTrack::GetTimes(REFERENCE_TIME** ppnStartTimes, REFERENCE_TIME** ppnStopTimes, ULONG** ppnFlags, ULONG** ppnDataSizes)
{
	ASSERT(ppnStartTimes);
	if(!TimesIndex())
		return 0;
	ppnStopTimes; ppnDataSizes;
	ASSERT(!ppnStopTimes && !ppnDataSizes); // Not Implemented
	const SIZE_T nSampleCount = TimesIndex()->Get(*ppnStartTimes);
	if(nSampleCount)
	{
		if(ppnFlags)
		{
			ULONG* pnFlags = (ULONG*) CoTaskMemAlloc(nSampleCount * sizeof *pnFlags);
			ASSERT(pnFlags);
			for(SIZE_T nSampleIndex = 0; nSampleIndex < nSampleCount; nSampleIndex++)
				pnFlags[nSampleIndex] = AM_SAMPLE_TIMEVALID;
			if(GetKeyMap())
			{
				SIZE_T* pnIndexes = NULL;
				const SIZE_T nIndexCount = GetKeyMap()->Get(pnIndexes);
				if(nIndexCount)
				{
					for(SIZE_T nIndexIndex = 0; nIndexIndex < nIndexCount; nIndexIndex++)
					{
						const SIZE_T nSampleIndex = pnIndexes[nIndexIndex];
						ASSERT(nSampleIndex < nSampleCount);
						if(nSampleIndex < nSampleCount)
							pnFlags[nSampleIndex] |= AM_SAMPLE_SPLICEPOINT;
					}
				} else
				{
					// NOTE: Missing key map means all samples are splice points (all frames are key frames)
					for(SIZE_T nSampleIndex = 0; nSampleIndex < nSampleCount; nSampleIndex++)
						pnFlags[nSampleIndex] |= AM_SAMPLE_SPLICEPOINT;
				}
				CoTaskMemFree(pnIndexes);
			}
			*ppnFlags = pnFlags;
		}
	}
	return nSampleCount;
}
