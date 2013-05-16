//
// ElemType.h: implementations of elementary stream type classes.
//
// Media type and other format-specific data that depends
// on the format of the contained elementary streams.
//
// Geraint Davies, April 2004
//
// Copyright (c) GDCL 2004-6. All Rights Reserved. 
// You are free to re-use this as the basis for your own filter development,
// provided you retain this copyright notice in the source.
// http://www.gdcl.co.uk
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "mpeg4.h"
#include "ElemType.h"
#include <dvdmedia.h>

inline WORD Swap2Bytes(USHORT x)
{
	return ((x & 0xff) << 8) | ((x >> 8) & 0xff);
}
inline DWORD Swap4Bytes(DWORD x)
{
	return ((x & 0xff) << 24) |
		   ((x & 0xff00) << 8) |
		   ((x & 0xff0000) >> 8) |
		   ((x >> 24) & 0xff);
}

// ----- format-specific handlers -------------------

// default no-translation

class NoChangeHandler : public FormatHandler
{
public:
    long BufferSize(long MaxSize);
    void StartStream();
    long PrepareOutput(IMediaSample* pSample, Movie* pMovie, LONGLONG llPos, long cBytes);
};

long 
NoChangeHandler::BufferSize(long MaxSize)
{
    return MaxSize;
}

void 
NoChangeHandler::StartStream()
{
}

long 
NoChangeHandler::PrepareOutput(IMediaSample* pSample, Movie* pMovie, LONGLONG llPos, long cBytes)
{
    BYTE* pBuffer;
    pSample->GetPointer(&pBuffer);

    if (pMovie->ReadAbsolute(llPos, pBuffer, cBytes) == S_OK)
    {
        return cBytes;
    }
    return 0;
}

class BigEndianAudioHandler : public NoChangeHandler
{
public:
    long PrepareOutput(IMediaSample* pSample, Movie* pMovie, LONGLONG llPos, long cBytes);
};

long 
BigEndianAudioHandler::PrepareOutput(IMediaSample* pSample, Movie* pMovie, LONGLONG llPos, long cBytes)
{
	cBytes = __super::PrepareOutput(pSample, pMovie, llPos, cBytes);
	if (cBytes > 0)
	{
		BYTE* pBuffer;
		pSample->GetPointer(&pBuffer);
		if (cBytes%2) 
		{
			cBytes--;
		}
		BYTE* pEnd = pBuffer + cBytes;
		while (pBuffer < pEnd)
		{
			WORD w = *(WORD*)pBuffer;
			w = Swap2Bytes(w);
			*(WORD*)pBuffer = w;
			pBuffer += 2;
		}
	}
	return cBytes;
}

// for CoreAAC, minimum buffer size is 8192 bytes.
class CoreAACHandler : public NoChangeHandler
{
public:
    long BufferSize(long MaxSize)
    {
        if (MaxSize < 8192)
        {
            MaxSize = 8192;
        }
        return MaxSize;
    }
};

// for DivX, need to prepend data to the first buffer
// from the media type
class DivxHandler : public NoChangeHandler
{
public:
    DivxHandler(const BYTE* pDSI, long cDSI);
    long BufferSize(long MaxSize);
    void StartStream();
    long PrepareOutput(IMediaSample* pSample, Movie* pMovie, LONGLONG llPos, long cBytes);
private:
    smart_array<BYTE> m_pPrepend;
    long m_cBytes;
    bool m_bFirst;
};

DivxHandler::DivxHandler(const BYTE* pDSI, long cDSI)
: m_cBytes(0)
{
    // The divx codec requires the stream to start with a VOL
    // header from the DecSpecificInfo. We search for
    // a VOL start code in the form 0x0000012x.
    while (cDSI > 4)
    {
        if ((pDSI[0] == 0) &&
            (pDSI[1] == 0) &&
            (pDSI[2] == 1) &&
            ((pDSI[3] & 0xF0) == 0x20))
        {
            m_cBytes = cDSI;
            m_pPrepend = new BYTE[m_cBytes];
            CopyMemory(m_pPrepend, pDSI,  m_cBytes);
            break;
        }
        pDSI++;
        cDSI--;
    }
}

long 
DivxHandler::BufferSize(long MaxSize)
{
    // we need to prepend the media type data
    // to the first sample, and with seeking, we don't know which
    // that will be.
    return MaxSize + m_cBytes; 
}

void 
DivxHandler::StartStream()
{
    m_bFirst = true;
}

long 
DivxHandler::PrepareOutput(IMediaSample* pSample, Movie* pMovie, LONGLONG llPos, long cBytes)
{
    if (m_bFirst)
    {
        m_bFirst = false;

        if (pSample->GetSize() < (cBytes + m_cBytes))
        {
            return 0;
        }

        BYTE* pBuffer;
        pSample->GetPointer(&pBuffer);

        // stuff the VOL header at the start of the stream
        CopyMemory(pBuffer,  m_pPrepend,  m_cBytes);
        pBuffer += m_cBytes;
        if (pMovie->ReadAbsolute(llPos, pBuffer, cBytes) != S_OK)
        {
            return 0;
        }
        return m_cBytes + cBytes;
            
    } else {
        return NoChangeHandler::PrepareOutput(pSample, pMovie, llPos, cBytes);
    }
}

// for H624 byte stream, need to re-insert the 00 00 01 start codes

class H264ByteStreamHandler : public NoChangeHandler
{
public:
	H264ByteStreamHandler(const BYTE* pDSI, long cDSI);
	void StartStream()
	{
		m_bFirst = true;
	}
    long BufferSize(long MaxSize)
	{
		// we need to add 00 00 00 01 for each NALU. There
		// could potentially be several NALUs for each frame. Assume a max of 12.
		if (m_cLength < 4)
		{		
			MaxSize += (12 * (4 - m_cLength));
		}
		return MaxSize + m_cPrepend;
	}
    long PrepareOutput(IMediaSample* pSample, Movie* pMovie, LONGLONG llPos, long cBytes);

private:
	long m_cLength;
	smart_array<BYTE> m_pPrepend;
	int m_cPrepend;
	bool m_bFirst;
};


H264ByteStreamHandler::H264ByteStreamHandler(const BYTE* pDSI, long cDSI)
: m_cLength(0),
  m_cPrepend(cDSI)
{
	m_pPrepend = new BYTE[m_cPrepend];
	CopyMemory(m_pPrepend, pDSI, m_cPrepend);
	if (cDSI > 4)
	{
		m_cLength = (pDSI[4] & 3) + 1;
	}
}

int ReadMSW(const BYTE* p)
{
	return (p[0] << 8) + (p[1]);
}

// convert length-preceded format to start code format
long
H264ByteStreamHandler::PrepareOutput(IMediaSample* pSample, Movie* pMovie, LONGLONG llPos, long cBytes)
{
	// check that length field is in valid range
	if ((m_cLength == 0) || (m_cLength > 5))
	{
		return 0;
	}

	BYTE* pDest;
	pSample->GetPointer(&pDest);
	long cRemain = pSample->GetSize();

	if (m_bFirst)
	{
		m_bFirst = false;

		const BYTE* pSrc = m_pPrepend + 6;
		int cSPS = m_pPrepend[5] & 0x1f;
		while (cSPS--)
		{
			int c = ReadMSW(pSrc);
			pSrc += 2;
			pDest[0] = 0;
			pDest[1] = 0;
			pDest[2] = 0;
			pDest[3] = 1;
			pDest += 4;
			cRemain -= 4;
			
			CopyMemory(pDest, pSrc, c);
			pDest += c;
			cRemain -= c;
			pSrc += c;
		}
		int cPPS = *pSrc++;
		while (cPPS--)
		{
			int c = ReadMSW(pSrc);
			pSrc += 2;
			pDest[0] = 0;
			pDest[1] = 0;
			pDest[2] = 0;
			pDest[3] = 1;
			pDest += 4;
			cRemain -= 4;
			
			CopyMemory(pDest, pSrc, c);
			pDest += c;
			cRemain -= c;
			pSrc += c;
		}
	}

	while (cBytes > m_cLength)
	{
		// read length field
		BYTE abLength[5];
		if (pMovie->ReadAbsolute(llPos, abLength, m_cLength) != S_OK)
		{
			return 0;
		}
		llPos += m_cLength;
		cBytes -= m_cLength;
		long cThis = 0;
		for (int i = 0; i < m_cLength; i++)
		{
			cThis <<= 8;
			cThis += abLength[i];
		}
		if ((cThis > cBytes) || ((cThis + 4) > cRemain))
		{
			return 0;
		}
		// output start code and then cThis bytes
		pDest[0] = 0;
		pDest[1] = 0;
		pDest[2] = 0;
		pDest[3] = 1;
		pDest += 4;
		cRemain -= 4;
		if (pMovie->ReadAbsolute(llPos, pDest, cThis) != S_OK)
		{
			return 0;
		}
		pDest += cThis;
		cRemain -= cThis;
		llPos += cThis;
		cBytes -= cThis;
	}
	BYTE* pStart;
	pSample->GetPointer(&pStart);
	return long(pDest - pStart);		// 32-bit consumption per packet is safe
}

// -----------------------------------------------------

ElementaryType::ElementaryType()
: m_cDecoderSpecific(0),
  m_tFrame(0),
  m_pHandler(NULL),
  m_depth(0)
{
}

ElementaryType::~ElementaryType()
{
    delete m_pHandler;
}


bool 
ElementaryType::ParseDescriptor(Atom* patmESD)
{
    AtomCache pESD(patmESD);
    if (pESD[0] != 0)
    {
        return false;   // only version 0 is speced
    }

	long cPayload = long(patmESD->Length() - patmESD->HeaderSize());

    // parse the ES_Descriptor to get the decoder info
    Descriptor ESDescr;
    if (!ESDescr.Parse(pESD+4, cPayload-4) || (ESDescr.Type() != Descriptor::ES_DescrTag))
    {
        return false;
    }
	long cOffset = 3;
    BYTE flags = ESDescr.Start()[2];
    if (flags & 0x80)
    {
        cOffset += 2;   // depends-on stream
    }
    if (flags & 0x40)
    {
        // URL string -- count of chars precedes string
        cOffset += ESDescr.Start()[cOffset] + 1;
    }
    if (flags & 0x20)
    {
        cOffset += 2;   // OCR id
    }
    Descriptor dscDecoderConfig;
    if (!ESDescr.DescriptorAt(cOffset, dscDecoderConfig))
    {
        return false;
    }
    Descriptor dscSpecific;
    if (!dscDecoderConfig.DescriptorAt(13, dscSpecific))
    {
		// could be mpeg-1/2 audio
		if (*dscDecoderConfig.Start() == 0x6b)
		{
			m_type = Audio_Mpeg2;
			return true;
		}
        return false;
    }

    // store decoder-specific info
    m_cDecoderSpecific = dscSpecific.Length();
    m_pDecoderSpecific = new BYTE[dscSpecific.Length()];
    CopyMemory(m_pDecoderSpecific,  dscSpecific.Start(),  m_cDecoderSpecific);

    return true;
}

struct QTVideo 
{
	BYTE	reserved1[6];		// 0
	USHORT	dataref;
	
	USHORT	version;			// 0
	USHORT	revision;			// 0
	ULONG	vendor;

	ULONG	temporal_compression;
	ULONG	spatial_compression;

	USHORT	width;
	USHORT	height;
	
	ULONG	horz_resolution;	// 00 48 00 00
	ULONG	vert_resolution;	// 00 48 00 00
	ULONG	reserved2;			// 0
	USHORT	frames_per_sample;	// 1
	BYTE	codec_name[32];		// pascal string - ascii, first byte is char count
	USHORT	bit_depth;
	USHORT	colour_table_id;		// ff ff
};


bool 
ElementaryType::Parse(REFERENCE_TIME tFrame, Atom* patm)
{
	m_shortname = "Unknown";
    m_tFrame = tFrame;

    // the ESD atom is at the end of a type-specific
    // structure.
    AtomCache pSD(patm);
    long cOffset;
	bool bDescriptor = true;
    if (patm->Type() == DWORD('mp4v'))
    {
        m_type = Video_Mpeg4;
		m_shortname = "MPEG4 Video";
        m_cx = (pSD[24] << 8) + pSD[25];
        m_cy = (pSD[26] << 8) + pSD[27];
        cOffset = 78;
    } else if (patm->Type() == DWORD('jvt1')) 
    {
		// this is an older format that I think is not in use
        m_type = Video_H264;
		m_shortname = "H264 Video";
        m_cx = (pSD[24] << 8) + pSD[25];
        m_cy = (pSD[26] << 8) + pSD[27];
        cOffset = 78;
	} else if ((patm->Type() == DWORD('s263')) || (patm->Type() == DWORD('M263')))
	{
		m_type = Video_H263;
		m_shortname = "H263 Video";
        m_cx = (pSD[24] << 8) + pSD[25];
        m_cy = (pSD[26] << 8) + pSD[27];
        cOffset = 78;
    } else if (patm->Type() == DWORD('avc1'))
	{
		// this is 14496-15: there is no descriptor in this case
        m_type = Video_H264;
		m_shortname = "H264 Video";
        m_cx = (pSD[24] << 8) + pSD[25];
        m_cy = (pSD[26] << 8) + pSD[27];
		cOffset = 78;
		bDescriptor = false;
	} else if ((patm->Type() == DWORD('rle ')) ||
				(patm->Type() == DWORD('I420')) ||
				(patm->Type() == DWORD('YUY2')) ||
				(patm->Type() == DWORD('dvc ')) ||
				(patm->Type() == DWORD('dvcp')) ||
				(patm->Type() == DWORD('dvsd')) ||
				(patm->Type() == DWORD('dvh5')) ||
				(patm->Type() == DWORD('dvhq')) ||
				(patm->Type() == DWORD('dvhd')) ||
				(patm->Type() == DWORD('jpeg')) ||
				(patm->Type() == DWORD('mjpg')) ||
				(patm->Type() == DWORD('MJPG')) ||
				(patm->Type() == DWORD('mjpa'))
				)
	{
		m_type = Video_FOURCC;
		m_shortname = "Video";

		// some decoders (eg mainconcept) only accept basic dv types, so map to these types:
		DWORD fcc = patm->Type();
		if ((fcc == DWORD('dvc ')) ||
			(fcc == DWORD('dvcp'))
			)
		{
			fcc = DWORD('dvsd');
			m_shortname = "DV Video";
		}
		else if ((fcc == DWORD('dvhq')) ||
				(fcc == DWORD('dvh5'))
				)
		{
			fcc = DWORD('dvhd');
			m_shortname = "DV Video";
		}
		else if ((fcc == DWORD('jpeg')) ||
				 (fcc == DWORD('mjpa')) ||
				 (fcc == DWORD('mjpg')))
		{
			fcc = DWORD('MJPG');
			m_shortname = "Motion JPEG Video";
		}
		m_fourcc = Swap4Bytes(fcc);


		// casting this to the structure makes it clearer what
		// info is there, but remember all shorts and longs are byte swapped
		const QTVideo* pformat = (const QTVideo*)(const BYTE*)pSD;
		m_cx = Swap2Bytes(pformat->width);
		m_cy = Swap2Bytes(pformat->height);
		m_depth = Swap2Bytes(pformat->bit_depth);

		// no further info needed
		return true;
	} else if ((patm->Type() == DWORD('mpg2')) ||
			   (patm->Type() == DWORD('xdvc')) ||
			   (patm->Type() == DWORD('xdv3'))
			   )
	{
        m_type = Video_Mpeg2;
		m_shortname = "MPEG-2 Video";
        m_cx = (pSD[24] << 8) + pSD[25];
        m_cy = (pSD[26] << 8) + pSD[27];
        cOffset = 78;
		return true;
	}else if (patm->Type() == DWORD('mp4a'))
    {
        m_type = Audio_AAC;
		m_shortname = "AAC Audio";
        cOffset = 28;

		// parse audio sample entry and store in case we don't find a descriptor
		m_cDecoderSpecific = sizeof(WAVEFORMATEX);
		m_pDecoderSpecific = new BYTE[m_cDecoderSpecific];
		WAVEFORMATEX* pwfx = (WAVEFORMATEX*)(BYTE*)m_pDecoderSpecific;
		pwfx->cbSize = 0;
		WORD w = *(USHORT*)(pSD + 16);
		pwfx->nChannels = Swap2Bytes(w);
		w = *(USHORT*)(pSD + 18);
		pwfx->wBitsPerSample = Swap2Bytes(w);

		// rate is fixed point 16.16
		pwfx->nSamplesPerSec = (SwapLong(pSD + 24) >> 16) & 0xffff;
		pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
		pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
		pwfx->wFormatTag = WAVE_FORMAT_PCM;


		// add support for some non-ISO files
		int version = (pSD[8] << 8) + pSD[9];
		if (version == 1)
		{
			cOffset += 16;
		}
		else if (version == 2)
		{
			cOffset += 16 + 20;
		}
		if (SwapLong(pSD+cOffset + 4) != DWORD('wave'))
		{
			if (version > 0)
			{
				long cSearch = 28;
				while (cSearch < (patm->Length()-8))
				{
					if (SwapLong(pSD + cSearch + 4) == DWORD('wave'))
					{
						cOffset = cSearch;
						break;
					}
					cSearch++;
				}
			}
		}
    } else if ((patm->Type() == DWORD('lpcm')) ||
               (patm->Type() == DWORD('alaw')) ||
               (patm->Type() == DWORD('ulaw')) ||
			   (patm->Type() == DWORD('samr')) ||
			   (patm->Type() == DWORD('samw')))
    {
        m_type = Audio_WAVEFORMATEX;
		m_shortname = "Audio";
        cOffset = 28;
	}
	else if ((patm->Type() == DWORD('sowt')) ||
			(patm->Type() == DWORD('twos')) ||
			(patm->Type() == DWORD('in24')) ||
			(patm->Type() == DWORD('in32')) ||
			(patm->Type() == DWORD('raw '))
			)
	{
		m_type = Audio_WAVEFORMATEX;
		m_shortname = "Audio";

		m_fourcc = patm->Type();

		// basic pcm audio type - parse this audio atom as a QT sample description
		m_cDecoderSpecific = sizeof(WAVEFORMATEX);
		m_pDecoderSpecific = new BYTE[m_cDecoderSpecific];
		WAVEFORMATEX* pwfx = (WAVEFORMATEX*)(BYTE*)m_pDecoderSpecific;

		pwfx->cbSize = 0;
		WORD w = *(USHORT*)(pSD + 16);
		pwfx->nChannels = Swap2Bytes(w);
		if (patm->Type() == DWORD('in24'))
		{
			pwfx->wBitsPerSample = 24;
		}
		else if (patm->Type() == DWORD('in32'))
		{
			pwfx->wBitsPerSample = 32;
		}
		else
		{
			w = *(USHORT*)(pSD + 18);
			pwfx->wBitsPerSample = Swap2Bytes(w);
		}

		// rate is fixed point 16.16
		pwfx->nSamplesPerSec = (SwapLong(pSD + 24) >> 16) & 0xffff;
		pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
		pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
		pwfx->wFormatTag = WAVE_FORMAT_PCM;
		
		return true;
	}
	else if (patm->Type() == DWORD('c608'))
	{
		m_type = Text_CC608;
		m_shortname = "CC 608 Data";
		return true;
    } else {
        return false;
    }

    patm->ScanChildrenAt(cOffset);
	for (int i = 0; i < patm->ChildCount(); i++)
	{
	    Atom* patmESD = patm->Child(i);
		if (!patmESD)
		{
			return false;
		}
    
		AtomCache pESD(patmESD);
		long cPayload = long(patmESD->Length() - patmESD->HeaderSize());
		if ((m_type == Video_H264) && !bDescriptor)
		{
			// iso 14496-15
			if (patmESD->Type() == DWORD('avcC'))
			{
				// store the whole payload as decoder specific
				m_cDecoderSpecific = cPayload;
				m_pDecoderSpecific = new BYTE[cPayload];
				CopyMemory(m_pDecoderSpecific,  pESD,  m_cDecoderSpecific);
				break;
			}

		} else if (patmESD->Type() == DWORD('d263'))
		{
			// 4 byte vendor code
			// 1 byte version
			// 1 byte level
			// 1 byte profile
			break;
		}
		else if (patmESD->Type() == DWORD('damr'))
		{
			// 4 byte vendor code
			// 1 byte version
			// 2-byte mode index
			// 1-byte number of packet mode changes
			// 1-byte number of samples per packet
			break;
		}
		else if (patmESD->Type() == DWORD('esds'))
		{
			if (ParseDescriptor(patmESD))
			{
				break;
			}
			else
			{
				return false;
			}
		}
		else if (patmESD->Type() == DWORD('wave'))
		{
			// this appears to be a non-ISO file (prob quicktime)
			// search for esds in children of this atom
			for (int j = 0; j < patmESD->ChildCount(); j++)
			{
				Atom* pwav = patmESD->Child(j);
				if (pwav->Type() == DWORD('esds'))
				{
					if (ParseDescriptor(pwav))
					{
						break;
					}
				}
			}
			if (m_cDecoderSpecific > 0)
			{
				break;
			}
		}
	}
    // check that we have picked up the seq header or other data
    // except for the few formats where it is not needed.
    if ((m_type != Audio_AAC) && (m_type != Audio_WAVEFORMATEX))
    {
        if (m_cDecoderSpecific <= 0)
        {
            return false;
        }
    }
    return true;
}

bool 
ElementaryType::IsVideo()
{
    return (m_type > First_Video) && (m_type < First_Audio);
}
    
bool 
ElementaryType::SetType(const CMediaType* pmt)
{
    if (m_mtChosen != *pmt)
    {
        delete m_pHandler;
        m_pHandler = NULL;
        m_mtChosen = *pmt;
    
        int idx = 0;
        CMediaType mtCompare;
        while (GetType(&mtCompare, idx))
        {
            if (mtCompare == *pmt)
            {
                // handler based on m_type and idx
                if (m_type == Audio_AAC)
                {
                    m_pHandler = new CoreAACHandler();
                } else 
					// bugfix pointed out by David Hunter --
					// Use the divxhandler to prepend VOL header for divx and xvid types.
					// index must match index values in GetType_Mpeg4V
					// (should really compare subtypes here I think)
					if ((m_type == Video_Mpeg4) && (idx > 0))
                {
                    m_pHandler = new DivxHandler(m_pDecoderSpecific, m_cDecoderSpecific);
				} 
				else if ((m_type == Video_H264) && (*pmt->FormatType() != FORMAT_MPEG2Video))
				{
					m_pHandler = new H264ByteStreamHandler(m_pDecoderSpecific, m_cDecoderSpecific);
				}
				else if ((m_type == Audio_WAVEFORMATEX) &&
						(m_fourcc == DWORD('twos'))
						)
				{
					m_pHandler = new BigEndianAudioHandler();
                } else {
                    m_pHandler = new NoChangeHandler();
                }
                return true;
            }
    
            idx++;
        }
        return false;
    }

    return true;
}

bool 
ElementaryType::GetType(CMediaType* pmt, int nType)
{
    // !! enable more type choices (eg dicas instead of divx for mpeg4)

    switch (m_type)
    {
    case Video_H264:
        if (nType == 1)
        {
			return GetType_H264ByteStream(pmt);
        }
		else if (nType == 0)
		{
			return GetType_H264(pmt);
		}
        break;

    case Video_Mpeg4:
	case Video_H263:
        return GetType_Mpeg4V(pmt, nType);
        // !! dicas 
        break;

	case Video_FOURCC:
		if (nType == 0)
		{
			return GetType_FOURCC(pmt);
		}
		break;

	case Video_Mpeg2:
		if (nType == 0)
		{
			// for standard mpeg-4 video, we set the media type
			// up for the DivX decoder
			pmt->InitMediaType();
			pmt->SetType(&MEDIATYPE_Video);
			pmt->SetSubtype(&MEDIASUBTYPE_MPEG2_VIDEO);
			pmt->SetFormatType(&FORMAT_MPEG2_VIDEO);
			MPEG2VIDEOINFO* pVI = (MPEG2VIDEOINFO*)pmt->AllocFormatBuffer(sizeof(MPEG2VIDEOINFO) + m_cDecoderSpecific);
			ZeroMemory(pVI, sizeof(MPEG2VIDEOINFO));
			pVI->hdr.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			pVI->hdr.bmiHeader.biBitCount = 24;
			pVI->hdr.bmiHeader.biWidth = m_cx;
			pVI->hdr.bmiHeader.biHeight = m_cy;
			pVI->hdr.bmiHeader.biSizeImage = DIBSIZE(pVI->hdr.bmiHeader);
			pVI->hdr.bmiHeader.biCompression = Swap4Bytes(DWORD('mpg2'));
			pVI->hdr.AvgTimePerFrame = m_tFrame;

			if (m_cDecoderSpecific)
			{
				BYTE* pDecSpecific = (BYTE*)(pVI+1);
				CopyMemory(pDecSpecific, m_pDecoderSpecific,  m_cDecoderSpecific);
			}
			return true;
		}
		break;

    case Audio_AAC:
        if (nType == 0)
        {
            return GetType_AAC(pmt);
        }
        break;
    case Audio_WAVEFORMATEX:
 	case Audio_Mpeg2:
       if (nType == 0)
        {
            return GetType_WAVEFORMATEX(pmt);
        }
        break;
	case Text_CC608:
		if (nType == 0)
		{
			pmt->InitMediaType();
			pmt->SetType(&MEDIATYPE_AUXLine21Data);
			pmt->SetSubtype(&MEDIASUBTYPE_Line21_BytePair);
			return true;
		}
    }

    return false;
}

bool
ElementaryType::GetType_H264(CMediaType* pmt)
{
    pmt->InitMediaType();
    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetSubtype(&__uuidof(MEDIASUBTYPE_H264_MP4_Stream));
    pmt->SetFormatType(&FORMAT_MPEG2Video);

	// following the Nero/Elecard standard, we use an mpeg2videoinfo block
	// with dwFlags for nr of bytes in length field, and param sets in 
	// sequence header (allows 1 DWORD already -- extend this).

	const BYTE* pconfig = m_pDecoderSpecific;
	// count param set bytes (including 2-byte length)
	int cParams = 0;
	int cSeq = pconfig[5] & 0x1f;
	const BYTE* p = &pconfig[6];
	for (int i = 0; i < cSeq; i++)
	{
		int c = 2 + (p[0] << 8) + p[1];
		cParams += c;
		p += c;
	}
	int cPPS = *p++;
	for (int i = 0; i < cPPS; i++)
	{
		int c = 2 + (p[0] << 8) + p[1];
		cParams += c;
		p += c;
	}


	int cFormat = sizeof(MPEG2VIDEOINFO) + cParams - sizeof(DWORD);
    MPEG2VIDEOINFO* pVI = (MPEG2VIDEOINFO*)pmt->AllocFormatBuffer(cFormat);
    ZeroMemory(pVI, cFormat);
	pVI->hdr.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pVI->hdr.bmiHeader.biWidth = m_cx;
    pVI->hdr.bmiHeader.biHeight = m_cy;
    pVI->hdr.AvgTimePerFrame = m_tFrame;
	pVI->hdr.bmiHeader.biCompression = DWORD('1cva');

	pVI->dwProfile = pconfig[1];
	pVI->dwLevel = pconfig[3];

	// nr of bytes used in length field, for length-preceded NALU format.
	pVI->dwFlags = (pconfig[4] & 3) + 1;

	pVI->cbSequenceHeader = cParams;
	// copy all param sets
	cSeq = pconfig[5] & 0x1f;
	p = &pconfig[6];
	BYTE* pDest = (BYTE*) &pVI->dwSequenceHeader;
	for (int i = 0; i < cSeq; i++)
	{
		int c = 2 + (p[0] << 8) + p[1];
		CopyMemory(pDest, p, c);
		pDest += c;
		p += c;
	}
	cPPS = *p++;
	for (int i = 0; i < cPPS; i++)
	{
		int c = 2 + (p[0] << 8) + p[1];
		CopyMemory(pDest, p, c);
		pDest += c;
		p += c;
	}

    return true;
}
	
FOURCCMap H264(DWORD('462H'));
bool 
ElementaryType::GetType_H264ByteStream(CMediaType* pmt)
{
    pmt->InitMediaType();
    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetSubtype(&H264);  //__uuidof(CLSID_H264));

	pmt->SetFormatType(&FORMAT_VideoInfo2);
	VIDEOINFOHEADER2* pvi2 = (VIDEOINFOHEADER2*)pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER2));
	ZeroMemory(pvi2, sizeof(VIDEOINFOHEADER2));
	pvi2->bmiHeader.biWidth = m_cx;
	pvi2->bmiHeader.biHeight = m_cy;
    pvi2->bmiHeader.biSizeImage = DIBSIZE(pvi2->bmiHeader);
    pvi2->bmiHeader.biCompression = DWORD('1cva');
	pvi2->AvgTimePerFrame = m_tFrame;
	SetRect(&pvi2->rcSource, 0, 0, m_cx, m_cy);
	pvi2->rcTarget = pvi2->rcSource;

	pvi2->dwPictAspectRatioX = m_cx;
	pvi2->dwPictAspectRatioY = m_cy;
		
	// interlace?
	// aspect ratio?
	return true;
}

bool
ElementaryType::GetType_Mpeg4V(CMediaType* pmt, int n)
{
	DWORD fourcc;
	if (n == 0)
	{
		fourcc = DWORD('V4PM');
	}
	else if (n == 1)
	{
		fourcc = DWORD('XVID');
	}
	else if (n == 2)
	{
		fourcc = DWORD('DIVX');
	}
	else
	{
		return false;
	}


    // for standard mpeg-4 video, we set the media type
    // up for the DivX decoder
    pmt->InitMediaType();
    pmt->SetType(&MEDIATYPE_Video);
    FOURCCMap divx(fourcc);
    pmt->SetSubtype(&divx);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    VIDEOINFOHEADER* pVI = (VIDEOINFOHEADER*)pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER) + m_cDecoderSpecific);
    ZeroMemory(pVI, sizeof(VIDEOINFOHEADER));
    pVI->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pVI->bmiHeader.biPlanes = 1;
    pVI->bmiHeader.biBitCount = 24;
    pVI->bmiHeader.biWidth = m_cx;
    pVI->bmiHeader.biHeight = m_cy;
    pVI->bmiHeader.biSizeImage = DIBSIZE(pVI->bmiHeader);
    pVI->bmiHeader.biCompression = fourcc;
    pVI->AvgTimePerFrame = m_tFrame;

	BYTE* pDecSpecific = (BYTE*)(pVI+1);
	CopyMemory(pDecSpecific, m_pDecoderSpecific,  m_cDecoderSpecific);

    return true;
}

bool 
ElementaryType::GetType_FOURCC(CMediaType* pmt)
{
	pmt->InitMediaType();
	pmt->SetType(&MEDIATYPE_Video);
	pmt->SetFormatType(&FORMAT_VideoInfo);

	VIDEOINFOHEADER* pVI = (VIDEOINFOHEADER*)pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER));
    ZeroMemory(pVI, sizeof(VIDEOINFOHEADER));
    pVI->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pVI->bmiHeader.biPlanes = 1;
    pVI->bmiHeader.biBitCount = (WORD)m_depth;
    pVI->bmiHeader.biWidth = m_cx;
    pVI->bmiHeader.biHeight = m_cy;
    pVI->bmiHeader.biSizeImage = DIBSIZE(pVI->bmiHeader);
    pVI->bmiHeader.biCompression = BI_RGB;
    pVI->AvgTimePerFrame = m_tFrame;

	pmt->SetSampleSize(pVI->bmiHeader.biSizeImage);

	if (m_fourcc == DWORD('rle '))
	{
		pmt->SetSubtype(&MEDIASUBTYPE_QTRle);
	}
	else 
	{
		FOURCCMap fcc(m_fourcc);
		pmt->SetSubtype(&fcc);
		pVI->bmiHeader.biCompression = m_fourcc;
	}

	return true;
}

// static
const int ElementaryType::SamplingFrequencies[] = 
{
    96000,
    88200,
    64000,
    48000,
    44100,
    32000,
    24000,
    22050,
    16000,
    12000,
    11025,
    8000,
    7350,
    0,
    0,
    0,
};

bool
ElementaryType::GetType_AAC(CMediaType* pmt)
{
    // set for Free AAC Decoder faad

    const int WAVE_FORMAT_AAC = 0x00ff;

    pmt->InitMediaType();
    pmt->SetType(&MEDIATYPE_Audio);
    FOURCCMap faad(WAVE_FORMAT_AAC);
    pmt->SetSubtype(&faad);
    pmt->SetFormatType(&FORMAT_WaveFormatEx);
    WAVEFORMATEX* pwfx = (WAVEFORMATEX*)pmt->AllocFormatBuffer(sizeof(WAVEFORMATEX) + m_cDecoderSpecific);
    ZeroMemory(pwfx,  sizeof(WAVEFORMATEX));
    pwfx->cbSize = WORD(m_cDecoderSpecific);
    CopyMemory((pwfx+1),  m_pDecoderSpecific,  m_cDecoderSpecific);

    // parse decoder-specific info to get rate/channels
    long samplerate = ((m_pDecoderSpecific[0] & 0x7) << 1) + ((m_pDecoderSpecific[1] & 0x80) >> 7);
    pwfx->nSamplesPerSec = SamplingFrequencies[samplerate];
    pwfx->nBlockAlign = 1;
    pwfx->wBitsPerSample = 16;
    pwfx->wFormatTag = WAVE_FORMAT_AAC;
    pwfx->nChannels = (m_pDecoderSpecific[1] & 0x78) >> 3;
    
    return true;
}

bool
ElementaryType::GetType_WAVEFORMATEX(CMediaType* pmt)
{
    // common to standard audio types that have known atoms
    // in the mpeg-4 file format

    // the dec-specific info is a waveformatex
    WAVEFORMATEX* pwfx = (WAVEFORMATEX*)(BYTE*)m_pDecoderSpecific;
    if ((m_cDecoderSpecific < sizeof(WAVEFORMATEX)) ||
        (m_cDecoderSpecific < int(sizeof(WAVEFORMATEX) + pwfx->cbSize)))
    {
        return false;
    }

    pmt->InitMediaType();
    pmt->SetType(&MEDIATYPE_Audio);
	if (m_type == Audio_Mpeg2)
	{
		pwfx->wFormatTag = 0x55;// WAVE_FORMAT_MPEG;
		pmt->SetSubtype(&MEDIASUBTYPE_MPEG2_AUDIO);
	}
	else
	{
		FOURCCMap subtype(pwfx->wFormatTag);
		pmt->SetSubtype(&subtype);
	}
	pmt->SetFormatType(&FORMAT_WaveFormatEx);

    int cLen = pwfx->cbSize + sizeof(WAVEFORMATEX);
    WAVEFORMATEX* pwfxMT = (WAVEFORMATEX*)pmt->AllocFormatBuffer(cLen);
    CopyMemory(pwfxMT, pwfx, cLen);

    return true;
}

// --- descriptor parsing ----------------------
bool 
Descriptor::Parse(const BYTE* pBuffer, long cBytes)
{
    m_pBuffer = pBuffer;
    m_type = (eType)pBuffer[0];
    long idx = 1;
    m_cBytes = 0;
    do {
        m_cBytes = (m_cBytes << 7) + (pBuffer[idx] & 0x7f);
    } while ((idx < cBytes) && (pBuffer[idx++] & 0x80));

    m_cHdr = idx;
    if ((m_cHdr + m_cBytes) > cBytes)
    {
        m_cHdr = m_cBytes = 0;
        return false;
    }
    return true;
}

    
bool 
Descriptor::DescriptorAt(long cOffset, Descriptor& desc)
{
    return desc.Parse(Start() + cOffset, Length() - cOffset); 
}



