// MovieWriter.cpp: implementation of basic file structure classes.
//
//
// Geraint Davies, May 2004
//
// Copyright (c) GDCL 2004-6. All Rights Reserved. 
// You are free to re-use this as the basis for your own filter development,
// provided you retain this copyright notice in the source.
// http://www.gdcl.co.uk
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MovieWriter.h"
#include "TypeHandler.h"

MovieWriter::MovieWriter(AtomWriter* pContainer)
: m_Container(pContainer),
  m_bAlignTrackStartTimeDisabled(FALSE),
  m_nMinimalMovieDuration(0),
  m_bStopped(false),
  m_bFTYPInserted(false),
  m_tInterleave(UNITS)
{
}

std::shared_ptr<TrackWriter> 
MovieWriter::MakeTrack(const CMediaType* pmt, bool NotifyMediaSampleWrite)
{
	auto Handler = TypeHandler::Make(pmt);
	if (!Handler)
		return nullptr;
    auto const Track = std::make_shared<TrackWriter>(this, static_cast<int>(m_Tracks.size()), std::move(Handler), NotifyMediaSampleWrite);
    m_Tracks.push_back(Track);
    return Track;
}

HRESULT 
MovieWriter::Close(REFERENCE_TIME* pDuration)
{
    // get longest duration of all tracks
    // also get earliest sample
    REFERENCE_TIME tEarliest = -1;
	REFERENCE_TIME tThis;
    for(auto&& pTrack: m_Tracks)
    {
        tThis = pTrack->Earliest();
        if (tThis != -1)
        {
          if ((tEarliest == -1) || (tThis < tEarliest))
              tEarliest = tThis;
        }
    }

    // adjust track start times so that the earliest track starts at 0
    REFERENCE_TIME tDur = 0;
    REFERENCE_TIME tAdj = -tEarliest;
    for(auto&& pTrack: m_Tracks)
    {
		if(!m_bAlignTrackStartTimeDisabled)
			pTrack->AdjustStart(tAdj);
        tThis = pTrack->Duration();
        if (tThis > tDur)
            tDur = tThis;
    }
	if(m_nMinimalMovieDuration && tDur < m_nMinimalMovieDuration)
		tDur = m_nMinimalMovieDuration;
    *pDuration = tDur;
    LONGLONG tScaledDur = tDur * MovieScale() / UNITS;

    // finish writing mdat
    if (m_patmMDAT)
    {
        m_patmMDAT->Close();
        m_patmMDAT = NULL;
    }

    // create moov atom
    HRESULT hr = S_OK;
    auto const pmoov = std::make_shared<Atom>(m_Container, m_Container->Length(), DWORD('moov'));

    // movie header
    // we are using 90khz as the movie timescale, so
    // we may need 64-bits.
    auto const pmvhd = pmoov->CreateAtom('mvhd');
    BYTE b[28*4] { };
    int cHdr;
    if (tScaledDur > 0x7fffffff)
    {
        b[0] = 1;               // version 1 
        // create time 64bit
        // modify time 64bit
        // timescale 32-bit
        // duration 64-bit
        Write32(MovieScale(), b + (5*4));
        Write64(tScaledDur, b + (6 * 4));
        cHdr = 8 * 4;
    }
    else
    {
        long lDur = long(tScaledDur);
        Write32(MovieScale(), b + (3 * 4));
        Write32(lDur, b + (4 * 4));
        cHdr = 5 * 4;
    }
    b[cHdr + 1] = 0x01;
    b[cHdr + 4] = 0x01;
    b[cHdr + 17] = 0x01;
    b[cHdr + 33] = 0x01;
    b[cHdr + 48] = 0x40;
    Write32((long)m_Tracks.size() + 1, b + cHdr + 76); // one-based next-track-id
    pmvhd->Append(b, cHdr + 80);
    pmvhd->Close();

    MakeIODS(pmoov);

    for(auto&& pTrack: m_Tracks)
    {
        hr = pTrack->Close(pmoov);
        if (FAILED(hr))
            break;
    }

    if(!m_Comment.empty())
    {
        auto const udta = pmoov->CreateAtom('udta'); // ISO/IEC 14496-12:2012 8.10.1 User Data Box
        auto const meta = udta->CreateAtom('meta'); // https://developer.apple.com/library/archive/documentation/QuickTime/QTFF/Metadata/Metadata.html
        {
            uint8_t MetaData[44];
            uint8_t* MetaDataPointer = MetaData;
            Write32(0, MetaDataPointer); MetaDataPointer += 4;
            Write32(32, MetaDataPointer); MetaDataPointer += 4;
            Write32('hdlr', MetaDataPointer); MetaDataPointer += 4;
            Write32(0, MetaDataPointer); MetaDataPointer += 4; // Version, Flags
            Write32(0, MetaDataPointer); MetaDataPointer += 4; // Predefined
            Write32('mdir', MetaDataPointer); MetaDataPointer += 4;
            Write32(0, MetaDataPointer); MetaDataPointer += 4;
            Write32(0, MetaDataPointer); MetaDataPointer += 4;
            Write32(0, MetaDataPointer); MetaDataPointer += 4;
            ASSERT(static_cast<size_t>(MetaDataPointer - MetaData) <= std::size(MetaData));
            meta->Append(MetaData, static_cast<long>(MetaDataPointer - MetaData));
            auto const ilst = meta->CreateAtom('ilst');
            auto const cmt = ilst->CreateAtom(0xA9000000 | 'cmt');
            {
                auto const data = cmt->CreateAtom('data');
                uint8_t Data[8];
                uint8_t* DataPointer = Data;
                Write32(0x00000001, DataPointer); DataPointer += 4; // Version, Flags
                Write32(0, DataPointer); DataPointer += 4;
                ASSERT(static_cast<size_t>(DataPointer - Data) <= std::size(Data));
                data->Append(Data, static_cast<long>(DataPointer - Data));
                data->Append(reinterpret_cast<uint8_t const*>(m_Comment.data()), static_cast<long>(m_Comment.size()));
                data->Close();
            }
            cmt->Close();
            ilst->Close();
        }
        meta->Close();
        // SUGG: Use Xtra atom with Windows Media specific properties
        udta->Close();
    }
	
	pmoov->Close();

    return hr;
}

void 
MovieWriter::Stop()
{
    CAutoLock lock(&m_csWrite);
    m_bStopped = true;
}
    
void 
MovieWriter::InsertFTYP(AtomWriter* pFile)
{
    if (!m_bFTYPInserted)
    {
		bool bHasOld = false;
        for(auto&& pTrack: m_Tracks)
			if (pTrack->IsNonMP4())
			{
				bHasOld = true;
				break;
			}
        auto const pFTYP = std::make_shared<Atom>(pFile, pFile->Length(), DWORD('ftyp'));
        // file type
        BYTE b[8];
		if (bHasOld)
		{
			Write32(DWORD('qt  '), b);
			// minor version
			b[4] = 0x20;
			b[5] = 0x04;
			b[6] = 0x06;
			b[7] = 0x00;
		}
		else
		{
			Write32(DWORD('mp42'), b);
			// minor version
			Write32(0, b+4);
		}
		pFTYP->Append(b, 8);
        // additional compatible specs
        Write32(DWORD('mp42'), b);
        pFTYP->Append(b, 4);
        Write32(DWORD('isom'), b);
        pFTYP->Append(b, 4);
        pFTYP->Close();
        m_bFTYPInserted = true;
    }
}

bool 
MovieWriter::CheckQueues()
{
    CAutoLock lock(&m_csWrite);
    if (m_bStopped)
    {
        return false;
    }

    // threading notes: we don't lock the
    // individual track queues except during the
    // actual access functions. The tracks are free to
    // add data to the end of the queue. The head of the queue 
    // will not be removed except during Stop and by us. The
    // m_bStopped flag ensures that we are not running when the
    // tracks enter Stop.

    // we need to return true if the whole set is at EOS
    // and all queues emptied
    bool bAllFinished;
    for(;;)
    {
        bAllFinished = true; // ... until proven otherwise

        // scan tracks to find which if any should write a chunk
        bool bSomeNotReady = false;
        bool bSomeAtEOS = false;
        LONGLONG tEarliestNotReady = -1;
        LONGLONG tEarliestReady = -1;
        int indexReady = -1;
        for (UINT i = 0; i < m_Tracks.size(); i++)
        {
            LONGLONG tHead;
            if (!m_Tracks[i]->GetHeadTime(&tHead))
            {
                // no chunk ready -- ok if finished
                if (!m_Tracks[i]->IsAtEOS())
                {
                    bAllFinished = false;

                    // note last write time
                    bSomeNotReady = true;
                    LONGLONG tWritten = m_Tracks[i]->LastWrite();
                    if ((tEarliestNotReady == -1) || (tWritten < tEarliestNotReady))
                    {
                        // remember the time of the track that is furthest
                        // behind
                        tEarliestNotReady = tWritten;
                    }
                } else {
                    bSomeAtEOS = true;
                }
            } else {

                bAllFinished = false;  // queue not empty -> not finished

                // remember the earliest of the ready blocks
                if ((tEarliestReady == -1) || (tHead < tEarliestReady))
                {
                    tEarliestReady = tHead;
                    indexReady = i;
                }
            }
        }

        // is there anything to write
        if (indexReady < 0)
        {
            break;
        }
        
        // mustn't get too far ahead of any blocked tracks (unless we have reached EOS)
		if (!bSomeAtEOS && bSomeNotReady)
        {
            // wait for more data on earliest-not-ready track
            break;
        }

		WriteTrack(indexReady);
    }

    return bAllFinished;
}

void
MovieWriter::WriteTrack(int indexReady)
{
    // make sure we have space in an mdat atom
    // -- make a new atom every 1Gb
    if ((m_patmMDAT) && (m_patmMDAT->Length() >= 1024*1024*1024))
    {
        m_patmMDAT->Close();
        m_patmMDAT = NULL;
    }
    if (m_patmMDAT == NULL)
    {
        if (!m_bFTYPInserted)
        {
            InsertFTYP(m_Container);
        }
        m_patmMDAT = std::make_shared<Atom>(m_Container, m_Container->Length(), DWORD('mdat'));
    }

	// write earliest block
    m_Tracks[indexReady]->WriteHead(m_patmMDAT);
}

void
MovieWriter::WriteOnStop()
{
    CAutoLock lock(&m_csWrite);
	ASSERT(m_bStopped);

	// loop writing as long as there are blocks queued at the pins
	for (;;)
	{
		LONGLONG tReady = 0;
		int idxReady = -1;
		// find the earliest
        for (UINT i = 0; i < m_Tracks.size(); i++)
        {
            LONGLONG tHead;
            if (m_Tracks[i]->GetHeadTime(&tHead))
            {
				if ((idxReady == -1) ||
					(tHead < tReady))
				{
					idxReady = i;
					tReady = tHead;
				}
			}
		}
	
		if (idxReady == -1)
		{
			// all done
			return;
		}

		WriteTrack(idxReady);
	}
}

REFERENCE_TIME 
MovieWriter::CurrentPosition()
{
    CAutoLock lock(&m_csWrite);
    LONGLONG tEarliest = -1;
    for (UINT i = 0; i < m_Tracks.size(); i++)
    {
        LONGLONG tWritten = m_Tracks[i]->LastWrite();
        if ((tEarliest < 0) || (tWritten < tEarliest))
        {
            tEarliest = tWritten;
        }
    }
    return tEarliest;
}

void 
MovieWriter::MakeIODS(std::shared_ptr<Atom> const& pmoov)
{
    auto const piods = pmoov->CreateAtom('iods');

    Descriptor iod(Descriptor::MP4_IOD);
    BYTE b[16];
    Write16(0x004f, b);      // object id 1, no url, no inline profile + reserved bits
    b[2] = 0xff;        // no od capability required
    b[3] = 0xff;        // no scene graph capability required
    b[4] = 0x0f;        // audio profile
    b[5] = 0x03;        // video profile
    b[6] = 0xff;        // no graphics capability required
    iod.Append(b, 7);

    // append the id of each media track
    for(auto&& Track: m_Tracks)
    {
        if (Track->IsVideo() || Track->IsAudio())
        {
            // use 32-bit track id in IODS
            Descriptor es(Descriptor::ES_ID_Inc);
            Write32(Track->ID(), b);
            es.Append(b, 4);
            iod.Append(&es);
        }
    }
    Write32(0, b);
    piods->Append(b, 4);       // ver/flags
    iod.Write(piods);
    piods->Close();
}

void MovieWriter::RecordBitrate(size_t index, long bitrate)
{
	CAutoLock lock(&m_csBitrate);

	if (m_Bitrates.size() <= index)
	{
		m_Bitrates.resize(index+1, 0);
	}
	if (m_Bitrates[index] < bitrate)
	{
		DbgLog((LOG_TRACE, 0, TEXT("Bitrate %d : %d kb/s"), index, bitrate/1024));

		m_Bitrates[index] = bitrate;
		long totalbits = 0;
		for (size_t i = 0; i < m_Bitrates.size(); i++)
		{
			totalbits += m_Bitrates[i];
		}
		REFERENCE_TIME tNew  = (UNITS * 8 * MAX_INTERLEAVE_SIZE) / totalbits;
		if (tNew < m_tInterleave)
		{
			m_tInterleave = tNew;
			DbgLog((LOG_TRACE, 0, TEXT("Interleave: %d ms"), long(m_tInterleave / 10000)));
		}
	}
}

void MovieWriter::NotifyMediaSampleWrite(INT TrackIndex, wil::com_ptr<IMediaSample> const& MediaSample, size_t DataSize)
{
	if(m_Container)
		m_Container->NotifyMediaSampleWrite(TrackIndex, MediaSample, DataSize);
}

// -------- Track -------------------------------------------------------

TrackWriter::TrackWriter(MovieWriter* pMovie, int index, std::unique_ptr<TypeHandler>&& TypeHandler, bool NotifyMediaSampleWrite)
: m_pMovie(pMovie),
  m_index(index),
  m_pType(std::move(TypeHandler)),
  m_NotifyMediaSampleWrite(NotifyMediaSampleWrite),
  m_Durations(DEFAULT_TIMESCALE),
  m_SC(1)                // dataref 1
{
    // adjust scale to media type (mostly because audio scales must be 16 bits);
    m_Durations.SetScale(m_pType->Scale());
	m_Durations.SetFrameDuration(m_pType->FrameDuration());
}

HRESULT 
TrackWriter::Add(IMediaSample* pSample)
{
    HRESULT hr = S_OK;
    { 
        // restrict scope of cs so we don't hold it
        // during the CheckQueues call
        CAutoLock lock(&m_csQueue);

        if (m_bEOS || m_bStopped)
        {
            hr = VFW_E_WRONG_STATE;
        } else 
        {
            if(!m_pCurrent)
            {
                m_pCurrent = std::make_shared<MediaChunk>(this);
				if(m_pType->IsOldIndexFormat())
					m_pCurrent->SetOldIndexFormat();
            }
            m_pCurrent->AddSample(pSample);
			// write out this block if larger than permitted interleave size
			if (m_pCurrent->IsFull(m_pMovie->MaxInterleaveDuration()))
			{
				auto const Duration = m_pCurrent->GetDuration();
				if(Duration > 0)
					m_pMovie->RecordBitrate(m_index, static_cast<long>((m_pCurrent->Length() * UNITS / Duration) * 8));
                m_Queue.push_back(std::exchange(m_pCurrent, nullptr));
            }
        }
    }

    m_pMovie->CheckQueues();

    return hr;
}

// returns true if all tracks now at end
bool 
TrackWriter::OnEOS()
{
    {
        CAutoLock lock(&m_csQueue);
        m_bEOS = true;
        // queue final partial chunk
        if (m_pCurrent && (m_pCurrent->MediaSampleCount() > 0))
        {
            m_Queue.push_back(m_pCurrent);
            m_pCurrent = NULL;
        }
    }
    return m_pMovie->CheckQueues();
}

// no more writes accepted -- partial/queued writes abandoned
void 
TrackWriter::Stop(bool bFlush)
{
    CAutoLock lock(&m_csQueue);

    // prevent further writes
    m_bStopped = true;

	if (bFlush)
	{
		// discard queued but unwritten samples
		m_pCurrent = NULL;
		m_Queue.clear();
	}
	else
	{
		// queue current partial block 
        if (m_pCurrent && (m_pCurrent->MediaSampleCount() > 0))
        {
            m_Queue.push_back(m_pCurrent);
            m_pCurrent = NULL;
        }

	}
}

bool 
TrackWriter::GetHeadTime(LONGLONG* ptHead) const
{
    CAutoLock lock(&m_csQueue);
    if(m_Queue.empty())
        return false;
    REFERENCE_TIME tLast;
    m_Queue.front()->GetTime(ptHead, &tLast);
    return true;
}

HRESULT 
TrackWriter::WriteHead(std::shared_ptr<Atom> const& Atom)
{
    CAutoLock lock(&m_csQueue);
    if(m_Queue.empty())
        return E_FAIL;
    auto const Chunk = m_Queue.front();
    m_Queue.pop_front();

    REFERENCE_TIME tStart, tEnd;
    Chunk->GetTime(&tStart, &tEnd);

    // the chunk will call back to us to index
    // the samples during this call, once
    // the media data is successfully written
    auto const hr = Chunk->Write(Atom);
    if (FAILED(hr))
        return hr;
    m_tLast = tEnd;
    return hr;
}

void 
TrackWriter::IndexChunk(LONGLONG posChunk, size_t nSamples)
{
    m_SC.Add(static_cast<long>(nSamples));
    m_CO.Add(posChunk);
}

void 
TrackWriter::IndexSample(bool bSync, REFERENCE_TIME tStart, REFERENCE_TIME tStop, size_t cBytes)
{
    // CTS offset means ES type-specific content parser?
	// -- this is done now by calculation from the frames start time (heuristically!)

    m_Sizes.Add(static_cast<long>(cBytes));
    m_Durations.Add(tStart, tStop);
    m_Syncs.Add(bSync);
}

void 
TrackWriter::OldIndex(LONGLONG posChunk, size_t cBytes)
{
	// check if it works before optimising it!
	size_t nSamples = cBytes / m_pType->BlockAlign();
	m_Sizes.AddMultiple(1, static_cast<long>(nSamples));
	m_Durations.AddOldFormat(static_cast<long>(nSamples));

	m_SC.Add(static_cast<long>(nSamples));
	m_CO.Add(posChunk);
}

HRESULT 
TrackWriter::Close(std::shared_ptr<Atom> const& Atom)
{
    auto const ptrak = Atom->CreateAtom('trak');

    // track header tkhd
    auto const ptkhd = ptrak->CreateAtom('tkhd');
    BYTE b[24*4] { };

    // duration in movie timescale
    LONGLONG scaledur = long(Duration() * m_pMovie->MovieScale() / UNITS);
    int cHdr = 6 * 4;
    if (scaledur > 0x7fffffff)
    {
        // use 64-bit version (64-bit create/modify and duration
        cHdr = 9*4;
        b[0] = 1;
        Write32(ID(), b+(5*4));
        Write64(scaledur, b+(7*4));
    }
    else
    {
        Write32(ID(), b+(3*4));     // 1-base track id
        Write32(long(scaledur), b+(5*4));
        cHdr = 6*4;
    }
    b[3] = 7;   // enabled, in movie and in preview

    if (IsAudio())
    {
        b[cHdr + 12] = 0x01;
    }
    b[cHdr + 17] = 1;
    b[cHdr + 33] = 1;
    b[cHdr + 48] = 0x40;
    if (IsVideo())
    {
		Write16(static_cast<uint16_t>(m_pType->Width()), &b[cHdr + 52]);
		Write16(static_cast<uint16_t>(m_pType->Height()), &b[cHdr + 56]);
    }

    ptkhd->Append(b, cHdr + 60);
    ptkhd->Close();

    // track ref tref
    Handler()->WriteTREF(ptrak);

    // edts -- used for first-sample offet
    // -- note, this is in movie timescale, not track
    m_Durations.WriteEDTS(ptrak, m_pMovie->MovieScale());

    auto const pmdia = ptrak->CreateAtom('mdia');

    // Media Header mdhd
    auto const pmdhd = pmdia->CreateAtom('mdhd');
    ZeroMemory(b, 9*4);
    
    // duration now in track timescale
    scaledur = m_Durations.Duration() * m_Durations.Scale() / UNITS;
    if (scaledur > 0x7fffffff)
    {
        b[0] = 1;       // 64-bit
        Write32(m_Durations.Scale(), b+20);
        Write64(scaledur, b+24);         
        cHdr = 8*4;
    }
    else
    {
        Write32(m_Durations.Scale(), b+12);
        Write32(long(scaledur), b+16);         
        cHdr = 5*4;
    }
    // 'eng' as offset from 0x60 in 0 pad bit plus 3x5-bit (05 0xe 07)
    b[cHdr] = 0x15;
    b[cHdr+1] = 0xc7;
    pmdhd->Append(b, cHdr + 4);
    pmdhd->Close();

    // handler id hdlr
    auto const phdlr = pmdia->CreateAtom('hdlr');
    ZeroMemory(b, 25);
	Write32(Handler()->DataType(), b+4);
    Write32(Handler()->Handler(), b+8);
    phdlr->Append(b, 25);
    phdlr->Close();
    
    auto const pminf = pmdia->CreateAtom('minf');

    // media information header vmhd/smhd
    ZeroMemory(b, sizeof(b));
    if (IsVideo())
    {
        auto const pvmhd = pminf->CreateAtom('vmhd');
        b[3] = 1;
        pvmhd->Append(b, 12);
        pvmhd->Close();
    } else if (IsAudio())
    {
        auto const psmhd = pminf->CreateAtom('smhd');
        psmhd->Append(b, 8);
        psmhd->Close();
    } else {
        auto const pnmhd = pminf->CreateAtom('nmhd');
        pnmhd->Append(b, 4);
        pnmhd->Close();
    }

    // dinf/dref -- data reference
    auto const pdinf = pminf->CreateAtom('dinf');
    auto const pdref = pdinf->CreateAtom('dref');
    Write32(0, b);        // ver/flags
    Write32(1, b+4);      // entries
    pdref->Append(b, 8);
    auto const purl = pdref->CreateAtom('url ');
    // self-contained flag set, and no string required
    // -- all data is in this file
    b[3] = 1;
    purl->Append(b, 4);
    purl->Close();
    pdref->Close();
    pdinf->Close();

    auto const pstbl = pminf->CreateAtom('stbl');

    // Sample description
    // -- contains one descriptor atom mp4v/mp4a/... for each data reference.
    auto const pstsd = pstbl->CreateAtom('stsd');
    Write32(0, b);    // ver/flags
    Write32(1, b+4);    // count of entries
    pstsd->Append(b, 8);
    Handler()->WriteDescriptor(pstsd, ID(), 1, m_Durations.Scale());   // dataref = 1
    pstsd->Close();

    HRESULT hr = m_Durations.WriteTable(pstbl);
    if (SUCCEEDED(hr))
    {
        hr = m_Syncs.Write(pstbl);
    }
    if (SUCCEEDED(hr))
    {
        hr = m_SC.Write(pstbl);
    }
    if (SUCCEEDED(hr))
    {
        hr = m_Sizes.Write(pstbl);
    }
    if (SUCCEEDED(hr))
    {
        hr = m_CO.Write(pstbl);
    }
    pstbl->Close();
    pminf->Close();
    pmdia->Close();
    ptrak->Close();

    return hr;
}

void TrackWriter::NotifyMediaSampleWrite(wil::com_ptr<IMediaSample> const& MediaSample, size_t DataSize)
{
	if(m_NotifyMediaSampleWrite && m_pMovie)
		m_pMovie->NotifyMediaSampleWrite(m_index, MediaSample, DataSize);
}

// -- Media Chunk ----------------------

HRESULT 
MediaChunk::AddSample(IMediaSample* MediaSample)
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

HRESULT
MediaChunk::Write(std::shared_ptr<Atom> const& Atom)
{
	// record chunk start position
	LONGLONG posChunk = Atom->Position() + Atom->Length();

	if (m_bOldIndexFormat)
	{
		size_t cBytes = 0;

		// ensure that we don't break in the middle of a sample (Maxim Kartavenkov)
		const size_t MAX_PCM_SIZE = 22050;
		size_t max_bytes = MAX_PCM_SIZE - (MAX_PCM_SIZE % m_pTrack->Handler()->BlockAlign());

		size_t cAvail = 0;
		BYTE* pBuffer = NULL;

		for (auto it = m_MediaSampleList.begin(); ; )
		{
			if (!cAvail)
			{
				if (it == m_MediaSampleList.end())
				{
					break;
				}
				auto const& pSample = *it++;
				pSample->GetPointer(&pBuffer);
				cAvail = pSample->GetActualDataLength();
				REFERENCE_TIME tStart, tStop;
				if (SUCCEEDED(pSample->GetTime(&tStart, &tStop)))
				{
					m_pTrack->SetOldIndexStart(tStart);
				}
			}
			auto const cThis = std::min<size_t>(max_bytes - cBytes, cAvail);

			size_t cActual = 0;
			m_pTrack->Handler()->WriteData(Atom, pBuffer, cThis, &cActual);
			cBytes += cActual;
			cAvail -= cActual;
			pBuffer += cActual;

			if (cBytes >= max_bytes)
			{
				m_pTrack->OldIndex(posChunk, cBytes);
				posChunk = Atom->Position() + Atom->Length();
				cBytes = 0;
			}
		}
		if (cBytes)
		{
			m_pTrack->OldIndex(posChunk, cBytes);
		}
		return S_OK;
	}

	// Remember that large H264 samples may be broken 
	// across several buffers, with Sync flag at start and
	// time on last buffer.
	bool bSync = false;
	size_t cBytes = 0;
	size_t nSamples = 0;

	// loop once through the samples writing the data
	for (auto it = m_MediaSampleList.begin(); it != m_MediaSampleList.end(); it++)
	{
		auto const& pSample = *it;

		// record positive sync flag, but for
		// multiple-buffer samples, only one sync flag will be present
		// so don't overwrite with later negatives.
		if (pSample->IsSyncPoint() == S_OK)
		{
			bSync = true;
		}

		// write payload, including any transformation (eg BSF to length-prepended)
		BYTE* pBuffer;
		pSample->GetPointer(&pBuffer);
		size_t cActual = 0;
		m_pTrack->Handler()->WriteData(Atom, pBuffer, pSample->GetActualDataLength(), &cActual);
		cBytes += cActual;
		REFERENCE_TIME tStart, tEnd;
		HRESULT hr = pSample->GetTime(&tStart, &tEnd);
		if (hr == VFW_S_NO_STOP_TIME)
			tEnd = tStart + 1;
		if (SUCCEEDED(hr))
		{
			// this is the last buffer in the sample
			m_pTrack->IndexSample(bSync, tStart, tEnd, cBytes);
			// reset for new sample
			bSync = false;
			cBytes = 0;
			nSamples++;
		}

		m_pTrack->NotifyMediaSampleWrite(pSample, cActual);
	}

	// add chunk position to index
	m_pTrack->IndexChunk(posChunk, nSamples);

	DbgLog((LOG_TRACE, 4, TEXT("Writing %ld samples to track"), nSamples));

	return S_OK;
}

bool 
MediaChunk::IsFull(REFERENCE_TIME tMaxDur) const
{
	if(Length() > MAX_INTERLEAVE_SIZE)
		return true;
	if(GetDuration() > tMaxDur)
		return true;
	return false;
}

REFERENCE_TIME 
MediaChunk::GetDuration() const
{
    if(m_pTrack->IsAudio())
        return m_tEnd - m_tStart;
    return m_MediaSampleList.size() * m_pTrack->SampleDuration();
}

// ---- index classes --------------------

void 
ListOfPairs::Append(long val)
{
	if (m_lCount == 0)
	{
		m_lCount = 1;
		m_lValue = val;
	}
	else if (val == m_lValue)
    {
        m_lCount++;
    } else {
        m_Table.Append(m_lCount);
        m_Table.Append(m_lValue);

		m_lCount = 1;
        m_lValue = val;
    }
	m_cEntries++;
}

HRESULT 
ListOfPairs::Write(std::shared_ptr<Atom> const& Atom)
{
	if (m_lCount > 0)
	{
		m_Table.Append(m_lCount);
		m_Table.Append(m_lValue);
	}
    // ver/flags == 0
    // nEntries
    // pairs of <count, value>

    BYTE b[8] { };
    // entry count is count of pairs
    Write32(static_cast<uint32_t>(m_Table.Entries() / 2), b+4);
    RETURN_IF_FAILED(Atom->Append(b, 8));
	return m_Table.Write(Atom);
}

// -----

void 
SizeIndex::AddMultiple(long cBytes, long count)
{
	if (m_nSamples == 0)
	{
		// first entry
		m_cBytesCurrent = cBytes;
		m_nCurrent = count;
		m_nSamples = count;
	}
	else if ((m_nCurrent > 0) && (cBytes == m_cBytesCurrent))
	{
		// normal case
		m_nCurrent += count;
		m_nSamples += count;
	}
	else
	{
		// not worth trying to optimise this, but make sure
		// it works if we ever get here.
		for (int i = 0; i < count; i++)
		{
			Add(cBytes);
		}
	}
}
void 
SizeIndex::Add(long cBytes)
{
    // if all sizes are the same, we only need a single count/size entry.
    // Otherwise we need one size for each entry

    if (m_nSamples == 0)
    {
        // first sample
        m_cBytesCurrent = cBytes;
        m_nCurrent = 1;
    } else if (m_nCurrent > 0)
    {
        // still accumulating identical sizes
        if (cBytes == m_cBytesCurrent)
        {
            // another of the same size
            m_nCurrent++;
        } else {
            // different -- need to create an entry for every one so far
            for (long n = 0; n < m_nCurrent; n++)
            {
                m_Table.Append(m_cBytesCurrent);
            }
            m_nCurrent = 0;

            // add this sample
            m_Table.Append(cBytes);
        }
    } else 
    {
        // we are creating a separate entry for each sample
        m_Table.Append(cBytes);
    }
    m_nSamples++;
}

HRESULT 
SizeIndex::Write(std::shared_ptr<Atom> const& Atom)
{
    auto const psz = Atom->CreateAtom('stsz');
    BYTE b[12] { };

    // ver/flags = 0
    // size
    // count
    // if size == 0, list of <count> sizes

    if (m_Table.Entries() == 0)
    {
        // this size field is left 0 if we are
        // creating a size entry for each sample
        Write32(m_cBytesCurrent, b+4);
    }
    Write32(m_nSamples, b+8);
    psz->Append(b, 12);
    if (m_Table.Entries() > 0)
        m_Table.Write(psz);
    psz->Close();
    return S_OK;
}

DurationIndex::DurationIndex(long scale)
: m_scale(scale),
  m_TotalDuration(0),
  m_refDuration(0),
  m_tStartFirst(-1),
  m_tStartLast(0),
  m_tStopLast(0),
  m_nSamples(0),
  m_bCTTS(false),
  m_tFrame(0)
{
}

void 
DurationIndex::Add(REFERENCE_TIME tStart, REFERENCE_TIME tEnd)
{
	// In general it is safer to just use the start time of each sample
	// since the stop time will be either wrong, or will just be deduced from
	// the next sample start time.
	// However, when frame re-ordering is happening, the composition time (== PTS) will 
	// not be the same as the decode time (== DTS) and we need to use both start and
	// stop time to build the CTTS table. 
	// We save the first few timestamps and then decide which mode to be in.
	if (m_nSamples < mode_decide_count)
	{
		if (m_nSamples == 0)
		{
			m_SumDurations = 0;
		}
		m_SumDurations += (tEnd - tStart);
		
		m_SampleStarts[m_nSamples] = tStart;
		m_SampleStops[m_nSamples] = tEnd;
		m_nSamples++;
		return;
	}
	else if (m_nSamples == mode_decide_count)
	{
		// this decides on a mode and then processes 
		// all the samples in the table
		ModeDecide();
	}

	if (m_bCTTS)
	{
		AppendCTTSMode(tStart, tEnd);
	}
	else
	{
        AddDuration(long(ToScale(tStart) - m_TotalDuration));
    }
	m_nSamples++;
    m_tStartLast = tStart;
    m_tStopLast = tEnd;
    return;
}

void 
DurationIndex::AddOldFormat(int count)
{
	for (int i = 0; i < count; i++)
	{
		AddDuration(1);
	}
	m_nSamples += count;
	m_tStopLast = ToReftime(m_nSamples);
}

void 
DurationIndex::SetOldIndexStart(REFERENCE_TIME tStart)
{
	if (m_nSamples == 0)
	{
		m_tStartFirst = tStart;
	}
}

void
DurationIndex::AddDuration(long cThis)
{
	if (cThis < 0)
	{
		// sometimes AdjustTimes causes the final duration to be negative.
		// I think I've fixed that, but just in case:
		cThis = 1;
	}

	m_STTS.Append(cThis);

    m_TotalDuration += cThis;
}

void 
DurationIndex::AppendCTTSMode(REFERENCE_TIME tStart, REFERENCE_TIME tEnd)
{
	// if the frames are out of order and the end time is invalid,
	// we must use the frame rate to work out the difference
	REFERENCE_TIME dur;
	if (m_bUseFrameRate)
	{
		dur = m_tFrame;
	}
	else
	{
		 dur = (tEnd - tStart);
	}
	// ToScale will round down, and this truncation causes the 
	// diff between decode and presentation time to get larger and larger
	// so we sum both reference time and scaled totals and use the difference. 
	// That way the rounding error does not build up.
	// the simpler version of these two lines is: cThis = long(ToScale(tEnd - tStart));
	m_refDuration += dur;
	long cThis = long(ToScale(m_refDuration) - m_TotalDuration);

	// difference between sum of durations to here and actual CTS
	// -- note: do this before adding current sample to total duration
	long cDiff =  long(ToScale(tStart) - m_TotalDuration);

	AddDuration(cThis);

	m_CTTS.Append(cDiff);
}

void
DurationIndex::ModeDecide()
{
	if (m_nSamples > 0)
	{
		bool bReverse = false;
		bool bDurOk = true;
		LONGLONG ave = m_SumDurations / m_nSamples;

		// 70fps is the maximum reasonable frame rate, so anything less than this is
		// likely to be an error
		const REFERENCE_TIME min_frame_dur = (UNITS / 70);
		if (ave < min_frame_dur)
		{
			bDurOk = false;
		}

		// in the most common case, when converting from TS output, we don't
		// get either accurate end times or an accuration rate in the media type.
		// The smallest positive interval between frames should be the frame duration.
		REFERENCE_TIME tInterval = 0;

		for (int i = 0; i < m_nSamples; i++)
		{
			if (i > 0)
			{
				if (m_SampleStarts[i] < m_SampleStarts[i-1])
				{
					bReverse = true;
				}
				else 
				{
					REFERENCE_TIME tThis = m_SampleStarts[i] - m_SampleStarts[i-1];
					if (tThis > min_frame_dur)
					{
						if ((tInterval == 0) || (tThis < tInterval))
						{
							tInterval = tThis;
						}
					}
				}
			}
		}
		m_tStartFirst = m_SampleStarts[0];

		// this interval is a better guess than the media type frame rate
		if (tInterval > min_frame_dur)
		{
			m_tFrame = tInterval;
		}

		if (bReverse)
		{
			m_bCTTS = true;

			if (!bDurOk)
			{
				m_bUseFrameRate = true;
			}
			else
			{
				m_bUseFrameRate = false;
			}

			// remember that the first frame might not be zero
			m_TotalDuration = ToScale(m_tStartFirst);
			m_refDuration = m_tStartFirst;
			for (int i = 0; i < m_nSamples; i++)
			{
				AppendCTTSMode(m_SampleStarts[i], m_SampleStops[i]);
			}
		}
		else
		{
			m_bCTTS = false;
			m_TotalDuration = ToScale(m_tStartFirst);
			for (int i = 1; i < m_nSamples; i++)
			{
				AddDuration(long(ToScale(m_SampleStarts[i]) - m_TotalDuration));
			}
		}
	}
}

HRESULT 
DurationIndex::WriteEDTS(std::shared_ptr<Atom> const& Atom, long scale)
{
    if (m_tStartFirst > 0)
    {
        // structure is 8 x 32-bit values
        //  flags/ver
        //  nr of entries
        //     duration : offset of first sample
        //     -1 : media time -- no media
        //     media rate: 1 (16 bit + 16-bit 0)
        //     duration : duration of whole track
        //     0 : start of media
        //     media rate 1

        auto const pedts = Atom->CreateAtom('edts');
        auto const pelst = pedts->CreateAtom('elst');

        BYTE b[48] { };

        // values are in movie scale
        LONGLONG offset = long(m_tStartFirst * scale / UNITS);
        LONGLONG dur  = long((m_tStopLast - m_tStartFirst) * scale / UNITS);

        int cSz;
        if ((offset > 0x7fffffff) || (dur > 0x7fffffff))
        {
            b[0] = 1;   // version 1 = 64-bit entries

            // create an offset for the first sample
            // using an "empty" edit
            Write32(2, b+4);
            Write64(offset, b+8);
            Write64(0xFFFF, b+16);        // no media used
            b[25] = 1;

            // whole track as next edit
            Write64(dur, b+28);
            Write64(0, b+36);
            b[45] = 1;
            cSz = 48;
        }
        else
        {
            // create an offset for the first sample
            // using an "empty" edit
            Write32(2, b+4);
            Write32(long(offset), b+8);
            Write32(0xFFFF, b+12);        // no media used
            b[17] = 1;

            // whole track as next edit
            Write32(long(dur), b+20);
            Write32(0, b+24);
            b[29] = 1;
            cSz = 32;
        }
        pelst->Append(b, cSz);

        pelst->Close();
        pedts->Close();
    }
    return S_OK;
}

HRESULT 
DurationIndex::WriteTable(std::shared_ptr<Atom> const& Atom)
{
    // do nothing if no samples at all
    HRESULT hr = S_OK;
	if (m_nSamples <= mode_decide_count)
	{
		ModeDecide();
	}
	if (m_nSamples > 0)
    {
		if (!m_bCTTS)
		{
			// the final sample duration has not been recorded -- use the
			// stop time
			if (ToScale(m_tStopLast) > m_TotalDuration)
			{
				AddDuration(long(ToScale(m_tStopLast) - m_TotalDuration));
			} else
				// NOTE: We still need some duration recorded, to avoid stts/stsz discrepancy at the very least
				AddDuration(1);
		}

        // create atom and write table
        auto const pstts = Atom->CreateAtom('stts');
		m_STTS.Write(pstts);
        pstts->Close();

		if (m_bCTTS)
		{
			// write CTTS table
			auto const pctts = Atom->CreateAtom('ctts');
			m_CTTS.Write(pctts);
			pctts->Close();
		}
    }
    return hr;
}

void 
SamplesPerChunkIndex::Add(long nSamples)
{
    // make a new entry if the old one does not match
    if (m_nSamples != nSamples)
    {
        // the entry is <chunk nr, samples per chunk, data ref>
        // The Chunk Nr is one-based
        m_Table.Append(m_nTotalChunks+1);
        m_Table.Append(nSamples);
        m_Table.Append(m_dataref);

        m_nSamples = nSamples;
    }
    m_nTotalChunks++;
}

HRESULT 
SamplesPerChunkIndex::Write(std::shared_ptr<Atom> const& Atom)
{
    auto const pSC = Atom->CreateAtom('stsc');
    //
    // ver/flags = 0
    // count of entries
    //    triple <first chunk, samples per chunk, dataref>

    BYTE b[8];
    Write32(0, b);
    Write32(static_cast<uint32_t>(m_Table.Entries() / 3), b+4);
    HRESULT hr = pSC->Append(b, 8);
    if (SUCCEEDED(hr))
        hr = m_Table.Write(pSC);
    return hr;
}

void 
ChunkOffsetIndex::Add(LONGLONG posChunk)
{
    // use the 32-bit table until we see a
    // value > 2Gb, then always use the 
    // 64-bit table for the remainder.
    if ((posChunk >= 0x80000000) || (m_Table64.Entries() > 0))
    {
        m_Table64.Append(posChunk);
    } else {
        m_Table32.Append(long(posChunk & 0xffffffff));
    }
}

HRESULT 
ChunkOffsetIndex::Write(std::shared_ptr<Atom> const& Atom)
{
    HRESULT hr = S_OK;
    // did we need 64-bit offsets?
    if (m_Table64.Entries() > 0)
    {
        // convert 32-bit entries to 64-bit
        ListOfI64 converted;
        for (size_t idx = 0; idx < m_Table32.Entries(); idx++)
            converted.Append(m_Table32.Entry(idx));

        // create 64-bit atom co64
        auto const pCO = Atom->CreateAtom('co64');
        BYTE b[8];
        Write32(0, b);        // ver/flags
        Write32(static_cast<uint32_t>(converted.Entries() + m_Table64.Entries()), b+4);
        hr = pCO->Append(b, 8);
        if (SUCCEEDED(hr))
            hr = converted.Write(pCO);
        if (SUCCEEDED(hr))
            hr = m_Table64.Write(pCO);

        pCO->Close();
    } else if (m_Table32.Entries() > 0) {
        // 32-bit atom
        auto const pCO = Atom->CreateAtom('stco');
        BYTE b[8];
        Write32(0, b);        // ver/flags
        Write32(static_cast<uint32_t>(m_Table32.Entries()), b+4);
        hr = pCO->Append(b, 8);
        if (SUCCEEDED(hr))
        {
            hr = m_Table32.Write(pCO);
        }
        pCO->Close();
    }
    return hr;
}

void 
SyncIndex::Add(bool bSync)
{
    if (m_bAllSync)
    {
        if (!bSync)
        {
            // no longer all syncs - 
            m_bAllSync = false;

            // must create table entries for all syncs so far
            for (long i = 0; i < m_nSamples; i++)
            {
                // 1-based sample index
                m_Syncs.Append(i+1);
            }
            // but we don't need to record this one as it is not sync
        }
    } else {
        if (bSync)
        {
            m_Syncs.Append(m_nSamples+1);
        }
    }
    m_nSamples++;
}

HRESULT 
SyncIndex::Write(std::shared_ptr<Atom> const& Atom)
{
    HRESULT hr = S_OK;

    // if all syncs, create no table
    if (!m_bAllSync)
    {
        auto const pss = Atom->CreateAtom('stss');

        BYTE b[8];
        Write32(0, b);    // ver/flags
        Write32(static_cast<uint32_t>(m_Syncs.Entries()), b+4);
        hr = pss->Append(b, 8);
        if (SUCCEEDED(hr))
        {
            hr = m_Syncs.Write(pss);
        }
        pss->Close();
    }
    return hr;
}



