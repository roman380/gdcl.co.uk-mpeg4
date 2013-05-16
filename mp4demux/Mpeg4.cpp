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
#include "index.h"
#include <sstream>

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
        LONGLONG llLength = (DWORD)SwapLong(hdr);
        DWORD type = (DWORD)SwapLong(hdr + 4);
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
        if (type == DWORD('uuid'))
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

Movie::Movie(Atom* pRoot)
: m_pRoot(pRoot)
{
    Atom* pMovie = m_pRoot->FindChild(DWORD('moov'));
    if (pMovie != NULL)
    {
        Atom* patmHdr = pMovie->FindChild(DWORD('mvhd'));
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
            m_tDuration = REFERENCE_TIME(m_duration) * UNITS / LONGLONG(m_scale);
        }

        // list tracks
        long idxTrack = 0;
        for (int i = 0; i < pMovie->ChildCount(); i++)
        {
            Atom* patm = pMovie->Child(i);
            if (patm->Type() == DWORD('trak'))
            {
                MovieTrackPtr pTrack = new MovieTrack(patm, this, idxTrack++);
                if (pTrack->Valid())
                {
                    m_Tracks.push_back(pTrack);
                }
            }
        }
    }
}
    
HRESULT 
Movie::ReadAbsolute(LONGLONG llPos, BYTE* pBuffer, long cBytes)
{
    return m_pRoot->Read(llPos, cBytes, pBuffer);
}


// ------------------------------------------------------------------


MovieTrack::MovieTrack(Atom* pAtom, Movie* pMovie, long idx)
: m_pRoot(NULL),
  m_pMovie(pMovie),
  m_patmSTBL(NULL),
  m_idx(idx),
  m_bOldFixedAudio(false)
{
    // check version/flags entry for track header
    Atom* pHdr = pAtom->FindChild(DWORD('tkhd'));
    if (pHdr != NULL)
    {
        BYTE verflags[4];
        pHdr->Read(pHdr->HeaderSize(), 4, verflags);
        if ((verflags[3] & 1) == 1)      // enabled?
        {
            // edit list may contain offset for first sample
            REFERENCE_TIME tFirst = 0;
            Atom* patmEDTS = pAtom->FindChild(DWORD('edts'));
            if (patmEDTS != NULL)
            {
                LONGLONG first = ParseEDTS(patmEDTS);
                // convert from movie scale to 100ns
                tFirst = first * UNITS / pMovie->Scale();
            }

            Atom* patmMDIA = pAtom->FindChild(DWORD('mdia'));
            if (patmMDIA && ParseMDIA(patmMDIA, tFirst))
            {
                // valid track -- make a name for the pin to use
				ostringstream strm;
				strm << m_pType->ShortName() << " ";
                strm << m_idx+1;
                m_strName = strm.str();

                m_pRoot = pAtom;
            }
        }
    }
}

LONGLONG 
MovieTrack::ParseEDTS(Atom* patm)
{
    LONGLONG tFirst = 0;
    Atom* patmELST = patm->FindChild(DWORD('elst'));
    if (patmELST != NULL)
    {
        AtomCache pELST(patmELST);

        // we only support the basic start-offset edit, which has two entries:
        //      0: media time -1, + duration --> offset
        //      1: media time 0, + duration of track -> whole of track
        if (SwapLong(pELST + 4) == 2)   // 2 entries
        {
            LONGLONG ll[4];     // duration, media time, duration, media time
            if (pELST[0] == 0)
            {
                // 32-bit values
                ll[0] = SwapLong(pELST + 8);
                ll[1] = SwapLong(pELST + 12);
                ll[2] = SwapLong(pELST + 20);
                ll[3] = SwapLong(pELST + 24);
            } else if (pELST[0] == 1)
            {
                ll[0] = SwapI64(pELST + 8);
                ll[1] = SwapI64(pELST + 16);
                ll[2] = SwapI64(pELST + 28);
                ll[3] = SwapI64(pELST + 36);
            } else {
                // unrecognised version
                return 0;
            }

            // media time should be -1 for the first clip
            // -- ie empty offset, then 0 for the second
            // (ie whole of track from start).
            if ((ll[1] == -1) && (ll[3] == 0))
            {
                tFirst = ll[0];
            }
        }
    }
    return tFirst;
}

// parse the track type information
bool
MovieTrack::ParseMDIA(Atom* patm, REFERENCE_TIME tFirst)
{
    // get track timescale from mdhd
    Atom* patmMDHD = patm->FindChild(DWORD('mdhd'));
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

    Atom* patmMINF = patm->FindChild(DWORD('minf'));
    if (!patmMINF)
    {
        return false;
    }
    m_patmSTBL = patmMINF->FindChild(DWORD('stbl'));
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

    Atom* pSTSD = m_patmSTBL->FindChild(DWORD('stsd'));
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

