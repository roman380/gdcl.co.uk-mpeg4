// TypeHandler.cpp: implementation of type-specific handler classes.
//
// Copyright (c) GDCL 2004-6. All Rights Reserved. 
// You are free to re-use this as the basis for your own filter development,
// provided you retain this copyright notice in the source.
// http://www.gdcl.co.uk
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <dvdmedia.h>
#include <mmreg.h>  // for a-law and u-law G.711 audio types
#include <wmcodecdsp.h> // MEDIASUBTYPE_DOLBY_DDPLUS
#include <ks.h>
#include <ksmedia.h> // FORMAT_UVCH264Video
#include "MovieWriter.h"
#include "nalunit.h"
#include "ParseBuffer.h"
#include "Bitstream.h"
#include "logger.h"

#pragma comment(lib, "wmcodecdspuuid.lib")
#pragma comment(lib, "strmiids.lib")

void WriteVariable(ULONG val, BYTE* pDest, int cBytes)
{
	for (int i = 0; i < cBytes; i++)
	{
		pDest[i] = BYTE((val >> (8 * (cBytes - (i+1)))) & 0xff);
	}
}

class DivxHandler : public TypeHandler
{
public:
    DivxHandler(const CMediaType* pmt);

    DWORD Handler() 
    {
        return 'vide';
    }
    void WriteTREF(Atom* patm) { UNREFERENCED_PARAMETER(patm); }
    bool IsVideo() 
    {
        return true;
    }
    bool IsAudio()
    { 
        return false;
    }
    long SampleRate()
    {
        // an approximation is sufficient
        return 30;
    }
    long Scale()
    {
        return DEFAULT_TIMESCALE;
    }
	long Width();
	long Height();

    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);
	HRESULT WriteData(Atom* patm, const BYTE* pData, int cBytes, int* pcActual);

private:
    CMediaType m_mt;
	smart_array<BYTE> m_pConfig;
	long m_cConfig;
};

class H264Handler : public TypeHandler
{
public:
    H264Handler(const CMediaType* pmt)
    : m_mt(*pmt)
    {}

    DWORD Handler() 
    {
        return 'vide';
    }
    void WriteTREF(Atom* patm) {UNREFERENCED_PARAMETER(patm);}
    bool IsVideo() 
    {
        return true;
    }
    bool IsAudio()
    { 
        return false;
    }
    long SampleRate()
    {
        // an approximation is sufficient
        return 30;
    }
    long Scale()
    {
        return DEFAULT_TIMESCALE;
    }
	long Width();
	long Height();

    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);
	LONGLONG FrameDuration();

protected:
    CMediaType m_mt;
};

class H264ByteStreamHandler : public H264Handler
{
public:
	H264ByteStreamHandler(const CMediaType* pmt);

    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);
	LONGLONG FrameDuration();
	HRESULT WriteData(Atom* patm, const BYTE* pData, int cBytes, int* pcActual);

	long Width()	{ return m_cx; }
	long Height()	{ return m_cy; }

	enum { nalunit_length_field = 4 };
private:
	REFERENCE_TIME m_tFrame;
	long m_cx;
	long m_cy;

	ParseBuffer m_ParamSets;		// stores param sets for WriteDescriptor
	bool m_bSPS;
	bool m_bPPS;
};

class FOURCCVideoHandler : public TypeHandler
{
public:
    FOURCCVideoHandler(const CMediaType* pmt)
    : m_mt(*pmt),
	  m_bMJPG(false),
	  m_bProcessMJPG(false)
	{
		FOURCCMap MJPG(DWORD('GPJM'));
		FOURCCMap mjpg(DWORD('gpjm'));
		if ((*pmt->Subtype() == MJPG) ||
			(*pmt->Subtype() == mjpg))
		{
			m_bMJPG = true;
			m_bProcessMJPG = true;
		}
	}

    DWORD Handler() 
    {
        return 'vide';
    }
    void WriteTREF(Atom* patm) { UNREFERENCED_PARAMETER(patm); }
    bool IsVideo() 
    {
        return true;
    }
    bool IsAudio()
    { 
        return false;
    }
    long SampleRate()
    {
        // an approximation is sufficient
        return 30;
    }
    long Scale()
    {
        return DEFAULT_TIMESCALE;
    }
	long Width();
	long Height();
    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);
	HRESULT WriteData(Atom* patm, const BYTE* pData, int cBytes, int* pcActual);

	bool IsNonMP4()			{ return m_bMJPG; }

private:
	bool m_bMJPG;
	bool m_bProcessMJPG;	// false if already has APP1
    CMediaType m_mt;
};

class AACHandler : public TypeHandler
{
public:
    AACHandler(const CMediaType* pmt);

    DWORD Handler() 
    {
        return 'soun';
    }
    void WriteTREF(Atom* patm) { UNREFERENCED_PARAMETER(patm); }
    bool IsVideo() 
    {
        return false;
    }
    bool IsAudio()
    { 
        return true;
    }
    long SampleRate()
    {
        return 50;
    }
    long Scale();
	long Width()	{ return 0; }
	long Height()	{ return 0; }
    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);
	HRESULT WriteData(Atom* patm, const BYTE* pData, int cBytes, int* pcActual);

private:
    CMediaType m_mt;
};


// handles some standard WAVEFORMATEX wave formats
class WaveHandler : public TypeHandler
{
public:
    WaveHandler(const CMediaType* pmt)
    : m_mt(*pmt)
    {}

    DWORD Handler() 
    {
        return 'soun';
    }
    void WriteTREF(Atom* patm) {UNREFERENCED_PARAMETER(patm);}
    bool IsVideo() 
    {
        return false;
    }
    bool IsAudio()
    { 
        return true;
    }
    long SampleRate()
    {
        return 50;
    }
	bool CanTruncate();
	bool Truncate(IMediaSample* pSample, REFERENCE_TIME tNewStart);

    long Scale();
	long Width()	{ return 0; }
	long Height()	{ return 0; }
    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);
	bool IsOldIndexFormat();
	long BlockAlign();
private:
    CMediaType m_mt;
};

// CC 608 closed-caption data as a private stream in byte-pair format
class CC608Handler : public TypeHandler
{
public:
    CC608Handler(const CMediaType* pmt)
    : m_mt(*pmt)
    {}

	DWORD DataType()
	{
		return 'dhlr';
	}

    DWORD Handler() 
    {
        return 'text';
    }
    void WriteTREF(Atom* patm) {UNREFERENCED_PARAMETER(patm);}
    bool IsVideo() 
    {
        return false;
    }
    bool IsAudio()
    { 
        return false;
    }
    long SampleRate()
    {
        return 30;
    }
	long Scale()
	{
		return 90000;
	}
	long Width()	{ return 0; }
	long Height()	{ return 0; }
    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);
private:
    CMediaType m_mt;
};

///////////////////////////////////////////////////////////////////////////////
// MPEG-2 Video and Audio support

class MPEG2VideoHandler : public TypeHandler
{
public:
    MPEG2VideoHandler(const CMediaType* pmt) : m_mt(*pmt) {}

    DWORD Handler()				{ return 'vide'; }
    void WriteTREF(Atom* patm)	{ UNREFERENCED_PARAMETER(patm); }
    bool IsVideo()				{ return true;	 }
    bool IsAudio()				{ return false;  }
	long SampleRate()			{ return 30;	 } // TODO: value???
    long Scale()				{ return 90000;  }
	long Width();
	long Height();

    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);
	LONGLONG FrameDuration();

protected:
    CMediaType m_mt;
};

class MPEG2AudioHandler : public TypeHandler
{
public:
	MPEG2AudioHandler(const CMediaType* pmt) : m_mt(*pmt) {}

    DWORD Handler()				{ return 'soun'; }
    void WriteTREF(Atom* patm)	{ UNREFERENCED_PARAMETER(patm); }
    bool IsVideo()				{ return false;  }
    bool IsAudio()				{ return true;   }
	long SampleRate()			{ return 50;	 } // TODO: value???
	long Width()				{ return 0;		 }
	long Height()				{ return 0;		 }

	long Scale();
    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);

private:
    CMediaType m_mt;
};

///////////////////////////////////////////////////////////////////////////////
// Dolby Digital (AC-3) support

struct AC3StreamInfo
{
	int fscod;
	int frmsizcod;
	int bsid;
	int bsmod;
	int acmod;
	int lfeon;
	
	AC3StreamInfo(void);
	BOOL Parse(const BYTE* pData, int cbData);
};

class DolbyDigitalHandler : public TypeHandler
{
public:
    DolbyDigitalHandler(const CMediaType* pmt) : 
		m_mt(*pmt), m_bParsed(FALSE) {}

    DWORD Handler()				{ return 'soun'; }
    void WriteTREF(Atom* patm)	{ UNREFERENCED_PARAMETER(patm); }
    bool IsVideo()				{ return false;  }
    bool IsAudio()				{ return true;	 }
    long SampleRate()			{ return 50;	 }  // TODO: value???
	long Width()				{ return 0;		 }
	long Height()				{ return 0;		 }

	long Scale();
	HRESULT WriteData(Atom* patm, const BYTE* pData, int cbData, int* pcActual);
    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);

private:
    CMediaType m_mt;
	AC3StreamInfo m_info;
	BOOL m_bParsed;
};

///////////////////////////////////////////////////////////////////////////////
// Dolby Digital Plus (Enhanced AC-3) support

// Table E.1: Stream type
enum EAC3StreamTypes
{
	EAC3IndependentStream,
	EAC3DependentSubtream,
	EAC3IndependentAC3Stream,
	EAC3ReservedStreamType
};

struct EAC3StreamInfo
{
	int frameSize;
	int bitrate;
	int strmtyp;
	int substreamid;
	int fscod;
    int bsid;
    int bsmod;
    int acmod;
    int lfeon;
	int chanmap;

	EAC3StreamInfo(void);
	bool Parse(const BYTE* pData, int cbData);
};

typedef std::vector<EAC3StreamInfo> EAC3StreamInfoArray;

class DolbyDigitalPlusHandler : public TypeHandler
{
public:
    DolbyDigitalPlusHandler(const CMediaType* pmt) : m_mt(*pmt)	{}

    DWORD Handler()				{ return 'soun'; }
    void WriteTREF(Atom* patm)	{ UNREFERENCED_PARAMETER(patm); }
    bool IsVideo()				{ return false;  }
    bool IsAudio()				{ return true;	 }
    long SampleRate()			{ return 50;	 }  // TODO: value???
	long Width()				{ return 0;		 }
	long Height()				{ return 0;		 }

	long Scale();
	HRESULT WriteData(Atom* patm, const BYTE* pData, int cbData, int* pcActual);
    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);


private:
	bool StreamInfoExists(const EAC3StreamInfo& info);
	int GetDependentSubstreams(int substreamid, int& chan_loc);


private:
    CMediaType m_mt;
	EAC3StreamInfoArray m_streams;
};

// Broadcom/Cyberlink Byte-Stream H264 subtype
// CLSID_H264
class DECLSPEC_UUID("8D2D71CB-243F-45E3-B2D8-5FD7967EC09B") CLSID_H264_BSF; // AKA MEDIASUBTYPE_H264_bis (GraphStudioNext)

const int WAVE_FORMAT_AAC = 0x00ff;
const int WAVE_FORMAT_AACEncoder = 0x1234;

//static 
bool
TypeHandler::CanSupport(const CMediaType* pmt)
{
	#pragma region Video
    if (*pmt->Type() == MEDIATYPE_Video)
    {
        // divx
        FOURCCMap divx(DWORD('xvid'));
        FOURCCMap xvidCaps(DWORD('XVID'));
        FOURCCMap divxCaps(DWORD('DIVX'));
        FOURCCMap dx50(DWORD('05XD'));
        if (((*pmt->Subtype() == divx) || 
			(*pmt->Subtype() == divxCaps) ||
			(*pmt->Subtype() == xvidCaps) ||
			(*pmt->Subtype() == dx50)) 
			    &&
				(*pmt->FormatType() == FORMAT_VideoInfo))
        {
            return true;
        }

		FOURCCMap x264(DWORD('462x'));
		FOURCCMap H264(DWORD('462H'));
		FOURCCMap h264(DWORD('462h'));
		FOURCCMap avc1(DWORD('1CVA'));

		// H264 BSF
		if ((*pmt->Subtype() == x264) || 
			(*pmt->Subtype() == H264) ||
			(*pmt->Subtype() == h264) ||
			(*pmt->Subtype() == avc1) ||
			(*pmt->Subtype() == __uuidof(CLSID_H264_BSF)))
		{
			// BSF
			if ((*pmt->FormatType() == FORMAT_VideoInfo) || (*pmt->FormatType() == FORMAT_VideoInfo2))
			{
				return true;
			}
			// length-prepended
			if (*pmt->FormatType() == FORMAT_MPEG2Video)
			{
				return true;
			}	
			if (*pmt->FormatType() == FORMAT_UVCH264Video)
			{
				return true;
			}
		}
		if (*pmt->Subtype() == MEDIASUBTYPE_MPEG2_VIDEO && *pmt->FormatType() == FORMAT_MPEG2Video)
		{
			return true;
		}

		// uncompressed
		// it would be nice to select any uncompressed type eg by checking that
		// the bitcount and biSize match up with the dimensions, but that
		// also works for ffdshow encoder outputs, so I'm returning to an 
		// explicit list. 

		const GUID& Subtype = *pmt->Subtype();
		if(*pmt->FormatType() == FORMAT_VideoInfo)
		{
			const VIDEOINFOHEADER* pvi = (const VIDEOINFOHEADER*) pmt->Format();
			#pragma region 24/32-bit RGB
			if(pvi->bmiHeader.biCompression == BI_RGB && DIBSIZE(pvi->bmiHeader) == pmt->GetSampleSize())
			{
				const WORD nBitCount = pvi->bmiHeader.biBitCount;
				if((Subtype == MEDIASUBTYPE_ARGB32 || Subtype == MEDIASUBTYPE_RGB32) && nBitCount == 32 || Subtype == MEDIASUBTYPE_RGB24 && nBitCount == 24)
					return true;
			}
			#pragma endregion
			FOURCCMap fcc(Subtype.Data1);
			if(fcc == Subtype)
			{
				if((pvi->bmiHeader.biBitCount > 0) && (DIBSIZE(pvi->bmiHeader) == pmt->GetSampleSize()))
				{
					FOURCCMap yuy2(DWORD('2YUY')); // YUY2
					FOURCCMap uyvy(DWORD('YVYU')); // UYVY
					FOURCCMap hdyc(DWORD('CYDH')); // HDYC
					FOURCCMap yv12(DWORD('21VY')); // YV12
					FOURCCMap nv12(DWORD('21VN')); // NV12
					FOURCCMap i420(DWORD('024I')); // I420
					if ((*pmt->Subtype() == yuy2) ||
						(*pmt->Subtype() == uyvy) ||
						(*pmt->Subtype() == hdyc) ||
						(*pmt->Subtype() == yv12) ||
						(*pmt->Subtype() == nv12) ||
						//(*pmt->Subtype() == MEDIASUBTYPE_RGB32) ||
						//(*pmt->Subtype() == MEDIASUBTYPE_RGB24) ||
						(*pmt->Subtype() == i420)
						)
					{
						return true;
					}
				}
				FOURCCMap MJPG(DWORD('GPJM'));
				FOURCCMap jpeg(DWORD('gepj'));
				FOURCCMap mjpg(DWORD('gpjm'));
				if ((*pmt->Subtype() == MJPG) ||
					(*pmt->Subtype() == jpeg) ||
					(*pmt->Subtype() == mjpg))
				{
					return true;
				}
			}
		}
    } else 
	#pragma endregion
	#pragma region Audio
	if (*pmt->Type() == MEDIATYPE_Audio)
    {
        // rely on format tag to identify formats -- for 
        // this, subtype adds nothing

        if (*pmt->FormatType() == FORMAT_WaveFormatEx)
        {
            // CoreAAC decoder
            WAVEFORMATEX* pwfx = (WAVEFORMATEX*)pmt->Format();
            if ((pwfx->wFormatTag == WAVE_FORMAT_AAC) || 
                (pwfx->wFormatTag == WAVE_FORMAT_AACEncoder))
            {
                return true;
            }
            if ((pwfx->wFormatTag == WAVE_FORMAT_PCM) ||
                (pwfx->wFormatTag == WAVE_FORMAT_ALAW) ||
                (pwfx->wFormatTag == WAVE_FORMAT_MULAW))
            {
                return true;
            }
			if (pwfx->wFormatTag == WAVE_FORMAT_MPEG ||				// MPEG-2 Audio
				pwfx->wFormatTag == WAVE_FORMAT_DOLBY_AC3_SPDIF)	// Dolby AC-3 SPDIF
            {
                return true;
            }
			// Dolby Digital Plus (or E-AC3) (wFormatTag = 0)
			if (*pmt->Subtype() == MEDIASUBTYPE_DOLBY_DDPLUS)
			{
				return true;
			}
			// Intel Media SDK uses the 0xFF- aac subtype guid, but
			// the wFormatTag does not match
			FOURCCMap aac(WAVE_FORMAT_AAC);
			if (*pmt->Subtype() == aac)
			{
				return true;
			}
        }
    } else 
	#pragma endregion
	if(*pmt->Type() == MEDIATYPE_AUXLine21Data && *pmt->Subtype() == MEDIASUBTYPE_Line21_BytePair)
	{
		return true;
	}
    return false;
}

//static 
TypeHandler* 
TypeHandler::Make(const CMediaType* pmt)
{
    if (!CanSupport(pmt))
    {
        return NULL;
    }
	#pragma region Video
    if (*pmt->Type() == MEDIATYPE_Video)
    {
		#pragma region DivX
        FOURCCMap divx(DWORD('xvid'));
        FOURCCMap xvidCaps(DWORD('XVID'));
        FOURCCMap divxCaps(DWORD('DIVX'));
        FOURCCMap dx50(DWORD('05XD'));
        if ((*pmt->Subtype() == divx) || 
			(*pmt->Subtype() == divxCaps) ||
			(*pmt->Subtype() == xvidCaps) ||
			(*pmt->Subtype() == dx50)) 
        {
            return new DivxHandler(pmt);
        }	
		#pragma	endregion 
		#pragma region MPEG-4 Part 10 H.264
		FOURCCMap x264(DWORD('462x'));
		FOURCCMap H264(DWORD('462H'));
		FOURCCMap h264(DWORD('462h'));
		FOURCCMap avc1(DWORD('1CVA'));
		if ((*pmt->Subtype() == x264) || 
			(*pmt->Subtype() == H264) ||
			(*pmt->Subtype() == h264) ||
			(*pmt->Subtype() == avc1) ||
			(*pmt->Subtype() == __uuidof(CLSID_H264_BSF)))
		{
			// BSF
			if ((*pmt->FormatType() == FORMAT_VideoInfo) || (*pmt->FormatType() == FORMAT_VideoInfo2))
			{
				return new H264ByteStreamHandler(pmt);
			}
			// length-prepended
			if (*pmt->FormatType() == FORMAT_MPEG2Video)
			{
				// check that the length field is 1-4. This is stored in dwFlags
				MPEG2VIDEOINFO* pvi = (MPEG2VIDEOINFO*)pmt->Format();
				if ((pvi->dwFlags < 1) || (pvi->dwFlags > 4))
				{
					// this is not MP4 format. 
					return new H264ByteStreamHandler(pmt);
				}
	            return new H264Handler(pmt);
			}

			if (*pmt->FormatType() == FORMAT_UVCH264Video)
			{
				return new H264ByteStreamHandler(pmt);
			}
		}
		#pragma	endregion 
		#pragma region MPEG-2
		if ((*pmt->Subtype() == MEDIASUBTYPE_MPEG2_VIDEO) &&
			(*pmt->FormatType()	== FORMAT_MPEG2Video))
		{
			return new MPEG2VideoHandler(pmt);
		}
		#pragma	endregion 
		#pragma region Other (Raw, FourCC)
		// NOTE: As checked in CanSupport
		if(*pmt->FormatType() == FORMAT_VideoInfo)
		{
			const VIDEOINFOHEADER* pvi = (const VIDEOINFOHEADER*) pmt->Format();
			#pragma region 24/32-bit RGB
			if(pmt->subtype == MEDIASUBTYPE_ARGB32 || pmt->subtype == MEDIASUBTYPE_RGB32 || pmt->subtype == MEDIASUBTYPE_RGB24)
				if(pvi->bmiHeader.biCompression == BI_RGB && DIBSIZE(pvi->bmiHeader) == pmt->GetSampleSize())
					return new FOURCCVideoHandler(pmt);
			#pragma	endregion 
			FOURCCMap fcc(pmt->subtype.Data1);
			if(fcc == *pmt->Subtype())
			{
				if((pvi->bmiHeader.biBitCount > 0) && (DIBSIZE(pvi->bmiHeader) == pmt->GetSampleSize()))
					return new FOURCCVideoHandler(pmt);
				#pragma region M-JPEG
				FOURCCMap MJPG(DWORD('GPJM'));
				FOURCCMap jpeg(DWORD('gepj'));
				FOURCCMap mjpg(DWORD('gpjm'));
				if ((*pmt->Subtype() == MJPG) ||
					(*pmt->Subtype() == jpeg) ||
					(*pmt->Subtype() == mjpg))
				{
					return new FOURCCVideoHandler(pmt);
				}
				#pragma endregion 
			}
		}
		#pragma	endregion 
    } else 
	#pragma endregion 
	#pragma region Audio
	if (*pmt->Type() == MEDIATYPE_Audio)
    {
        // rely on format tag to identify formats -- for 
        // this, subtype adds nothing

        if (*pmt->FormatType() == FORMAT_WaveFormatEx)
        {
            // CoreAAC decoder
            WAVEFORMATEX* pwfx = (WAVEFORMATEX*)pmt->Format();
            if ((pwfx->wFormatTag == WAVE_FORMAT_AAC) || 
                (pwfx->wFormatTag == WAVE_FORMAT_AACEncoder))
            {
                return new AACHandler(pmt);
            }
            if ((pwfx->wFormatTag == WAVE_FORMAT_PCM) ||
                (pwfx->wFormatTag == WAVE_FORMAT_ALAW) ||
                (pwfx->wFormatTag == WAVE_FORMAT_MULAW))
            {
                return new WaveHandler(pmt);
            }
			if (pwfx->wFormatTag == WAVE_FORMAT_MPEG)
			{
				return new MPEG2AudioHandler(pmt);
			}
			if (pwfx->wFormatTag == WAVE_FORMAT_DOLBY_AC3_SPDIF)	// Dolby AC-3 SPDIF
            {
				return new DolbyDigitalHandler(pmt);
            }
			// (wFormatTag = 0)
			if (*pmt->Subtype() == MEDIASUBTYPE_DOLBY_DDPLUS)
			{
				return new DolbyDigitalPlusHandler(pmt);
			}
			// Intel Media SDK uses the 0xFF- aac subtype guid, but
			// the wFormatTag does not match
			FOURCCMap aac(WAVE_FORMAT_AAC);
			if (*pmt->Subtype() == aac)
			{
				return new AACHandler(pmt);
			}
        }
    } else 
	#pragma endregion 
	if(*pmt->Type() == MEDIATYPE_AUXLine21Data && *pmt->Subtype() == MEDIASUBTYPE_Line21_BytePair)
	{
		return new CC608Handler(pmt);
	}
    return NULL;
}

HRESULT 
TypeHandler::WriteData(Atom* patm, const BYTE* pData, int cBytes, int* pcActual)
{
	*pcActual = cBytes;
	return patm->Append(pData, cBytes);
}


// -------------------------------------------
DivxHandler::DivxHandler(const CMediaType* pmt)
: m_mt(*pmt),
  m_cConfig(0)
{
	if ((*m_mt.FormatType() == FORMAT_VideoInfo) && 
		(m_mt.FormatLength() > sizeof(VIDEOINFOHEADER)))
	{
		m_cConfig = m_mt.FormatLength() - sizeof(VIDEOINFOHEADER);
		m_pConfig = new BYTE[m_cConfig];
		const BYTE* pExtra = m_mt.Format() + sizeof(VIDEOINFOHEADER);
		CopyMemory(m_pConfig, pExtra, m_cConfig);
	}
}

long 
DivxHandler::Width()
{
    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
	return pvi->bmiHeader.biWidth;
}

long 
DivxHandler::Height()
{
    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
	return abs(pvi->bmiHeader.biHeight);
}

void 
DivxHandler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
    UNREFERENCED_PARAMETER(scale);
    smart_ptr<Atom> psd = patm->CreateAtom('mp4v');

    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
    int width = pvi->bmiHeader.biWidth;
    int height = abs(pvi->bmiHeader.biHeight);

    BYTE b[78];
    ZeroMemory(b, 78);
    WriteShort(dataref, b+6);
    WriteShort(width, b+24);
    WriteShort(height, b+26);
    b[29] = 0x48;
    b[33] = 0x48;
    b[41] = 1;
    b[75] = 24;
    WriteShort(-1, b+76);
    psd->Append(b, 78);

    smart_ptr<Atom> pesd = psd->CreateAtom('esds');
    WriteLong(0, b);        // ver/flags
    pesd->Append(b, 4);

    // es descr
    //      decoder config
    //          <objtype/stream type/bitrates>
    //          decoder specific info desc
    //      sl descriptor
    Descriptor es(Descriptor::ES_Desc);
    WriteShort(id, b);
    b[2] = 0;
    es.Append(b, 3);
    Descriptor dcfg(Descriptor::Decoder_Config);
    b[0] = 0x20;    //mpeg-4 video
    b[1] = (4 << 2) | 1;    // video stream

    // buffer size 15000
    b[2] = 0;
    b[3] = 0x3a;
    b[4] = 0x98;
    WriteLong(1500000, b+5);    // max bitrate
    WriteLong(0, b+9);          // avg bitrate 0 = variable
    dcfg.Append(b, 13);
    Descriptor dsi(Descriptor::Decoder_Specific_Info);

	dsi.Append(m_pConfig, m_cConfig);
    dcfg.Append(&dsi);
    es.Append(&dcfg);
	Descriptor sl(Descriptor::SL_Config); // ISO 14496-1 8.3.6, 10.2.3
	b[0] = 2; // Reserved for ISO use???
	b[1] = 0x7F; // OCRStreamFlag 0, Reserved 1111111
    sl.Append(b, 2);
    es.Append(&sl);
    es.Write(pesd);
    pesd->Close();

    psd->Close();
}

inline bool NextStartCode(const BYTE*&pBuffer, long& cBytes)
{
    while ((cBytes >= 4) &&
           (*(UNALIGNED DWORD *)pBuffer & 0x00FFFFFF) != 0x00010000) {
        cBytes--;
        pBuffer++;
    }
    return cBytes >= 4;
}

HRESULT 
DivxHandler::WriteData(Atom* patm, const BYTE* pData, int cBytes, int* pcActual)
{
	if (m_cConfig == 0)
	{
		const BYTE* p = pData;
		long c = cBytes;
		const BYTE* pVOL = NULL;
		while (NextStartCode(p, c))
		{
			if (pVOL == NULL)
			{
				if (p[3] == 0x20)
				{
					pVOL = p;
				}
			}
			else
			{
				m_cConfig = long(p - pVOL);
				m_pConfig = new BYTE[m_cConfig];
				CopyMemory(m_pConfig, pVOL, m_cConfig);
				break;
			}
			p += 4;
			c -= 4;
		}
	}
	return __super::WriteData(patm, pData, cBytes, pcActual);
}

long 
AACHandler::Scale()
{
    // for audio, the scale should be the sampling rate but
    // must not exceed 65535
    WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_mt.Format();
    if (pwfx->nSamplesPerSec > 65535)
    {
        return 45000;
    }
    else
    {
        return pwfx->nSamplesPerSec;
    }
}

AACHandler::AACHandler(const CMediaType* pmt)
: m_mt(*pmt)
{
	// the Intel encoder uses a tag that doesn't match the subtype
	FOURCCMap aac(WAVE_FORMAT_AAC);
	if ((*m_mt.Subtype() == aac) &&
		(*m_mt.FormatType() == FORMAT_WaveFormatEx))
	{
		WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_mt.Format();
		pwfx->wFormatTag = WAVE_FORMAT_AAC;
	}
}

HRESULT 
AACHandler::WriteData(Atom* patm, const BYTE* pData, int cBytes, int* pcActual)
{
	if ((cBytes > 7) && (pData[0] == 0xff) && ((pData[1] & 0xF0) == 0xF0))
	{
		if (m_mt.FormatLength() == sizeof(WAVEFORMATEX))
		{
			int len = ((pData[3] & 0x3) << 11) + (pData[4] << 3) + ((pData[5] >> 5) & 0x7);
			if (len == cBytes)
			{
				int header = 7;
				if ((pData[1] & 1) == 0) 
				{
					header = 9;
				}
				pData += header;
				cBytes -= header;
			}
		}
	}
	return __super::WriteData(patm, pData, cBytes, pcActual);
}

const DWORD AACSamplingFrequencies[] = 
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
#ifndef SIZEOF_ARRAY
#define SIZEOF_ARRAY(a) (sizeof(a) / sizeof(a[0]))
#endif

void 
AACHandler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
    smart_ptr<Atom> psd = patm->CreateAtom('mp4a');

    BYTE b[28];
    ZeroMemory(b, 28);
    WriteShort(dataref, b+6);
    WriteShort(2, b+16);
    WriteShort(16, b+18);
    WriteShort(unsigned short(scale), b+24);    // this is what forces us to use short audio scales
    psd->Append(b, 28);

    smart_ptr<Atom> pesd = psd->CreateAtom('esds');
    WriteLong(0, b);        // ver/flags
    pesd->Append(b, 4);
    // es descr
    //      decoder config
    //          <objtype/stream type/bitrates>
    //          decoder specific info desc
    //      sl descriptor
    Descriptor es(Descriptor::ES_Desc);
    WriteShort(id, b);
    b[2] = 0;
    es.Append(b, 3);
    Descriptor dcfg(Descriptor::Decoder_Config);
    b[0] = 0x40;    // AAC audio
    b[1] = (5 << 2) | 1;    // audio stream

    // buffer size 15000
    b[2] = 0;
    b[3] = 0x3a;
    b[4] = 0x98;
    WriteLong(1500000, b+5);    // max bitrate
    WriteLong(0, b+9);          // avg bitrate 0 = variable
    dcfg.Append(b, 13);
    Descriptor dsi(Descriptor::Decoder_Specific_Info);
    BYTE* pExtra = m_mt.Format() + sizeof(WAVEFORMATEX);
    long cExtra = m_mt.FormatLength() - sizeof(WAVEFORMATEX);
	if (cExtra == 0)
	{
		long ObjectType = 2;
		WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_mt.Format();
		long ChannelIndex = pwfx->nChannels;
		if (ChannelIndex == 8)
		{
			ChannelIndex = 7;
		}
		long RateIndex = 0;
		for (long i =0; i < SIZEOF_ARRAY(AACSamplingFrequencies); i++)
		{
			if (AACSamplingFrequencies[i] == pwfx->nSamplesPerSec)
			{
				RateIndex = i;
				break;
			}
		}
		BYTE b[2];
		b[0] = (BYTE) ((ObjectType << 3) | ((RateIndex >> 1) & 7));
		b[1] = (BYTE) (((RateIndex & 1) << 7) | (ChannelIndex << 3));
		dsi.Append(b, 2);
	}
    if (cExtra > 0)
    {
		// HOTFIX: LEAD Tools AAC Encoder provides incorrect MPEG-4 Audio Object Type which causes compatibility issues, 
		//         To work this around we override 0 (Null) and 1 values with 2 (AAC-LC)
		if(cExtra == 2)
		{
			BYTE pnNewExtraData[2] = { pExtra[0], pExtra[1] };
			if(((pnNewExtraData[0] & 0xF8) >> 3) != 2) // AAC-LC
				pnNewExtraData[0] = ((2) << 3) | (pnNewExtraData[0] & 0x07);
	        dsi.Append(pnNewExtraData, 2);
		} else
	        dsi.Append(pExtra, cExtra);
    }
    dcfg.Append(&dsi);
    es.Append(&dcfg);
	Descriptor sl(Descriptor::SL_Config); // ISO 14496-1 8.3.6, 10.2.3
	b[0] = 2; // Reserved for ISO use???
	b[1] = 0x7F; // OCRStreamFlag 0, Reserved 1111111
    sl.Append(b, 2);
    es.Append(&sl);
    es.Write(pesd);
    pesd->Close();
    psd->Close();
}
	
LONGLONG 
H264Handler::FrameDuration()
{
	MPEG2VIDEOINFO* pvi = (MPEG2VIDEOINFO*)m_mt.Format();
	return pvi->hdr.AvgTimePerFrame;
}

long 
H264Handler::Width()
{
	MPEG2VIDEOINFO* pvi = (MPEG2VIDEOINFO*)m_mt.Format();
	return pvi->hdr.bmiHeader.biWidth;
}

long 
H264Handler::Height()
{
	MPEG2VIDEOINFO* pvi = (MPEG2VIDEOINFO*)m_mt.Format();
	return abs(pvi->hdr.bmiHeader.biHeight);
}

void 
H264Handler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
    UNREFERENCED_PARAMETER(scale);
    UNREFERENCED_PARAMETER(id);
    smart_ptr<Atom> psd = patm->CreateAtom('avc1');

	MPEG2VIDEOINFO* pvi = (MPEG2VIDEOINFO*)m_mt.Format();
    int width = pvi->hdr.bmiHeader.biWidth;
    int height = abs(pvi->hdr.bmiHeader.biHeight);


    BYTE b[78];
    ZeroMemory(b, 78);
    WriteShort(dataref, b+6);
    WriteShort(width, b+24);
    WriteShort(height, b+26);
    b[29] = 0x48;
    b[33] = 0x48;
    b[41] = 1;
    b[75] = 24;
    WriteShort(-1, b+76);
    psd->Append(b, 78);

    smart_ptr<Atom> pesd = psd->CreateAtom('avcC');
    b[0] = 1;           // version 1
    b[1] = (BYTE)pvi->dwProfile;
    b[2] = 0;
    b[3] = (BYTE)pvi->dwLevel;
    // length of length-preceded nalus
    b[4] = BYTE(0xfC | (pvi->dwFlags - 1));
    b[5] = 0xe1;        // 1 SPS

    // SPS
    const BYTE* p = (const BYTE*)&pvi->dwSequenceHeader;
    const BYTE* pEnd = p + pvi->cbSequenceHeader;
    int c = (p[0] << 8) | p[1];
    // extract profile/level compat from SPS
    b[2] = p[4];
    pesd->Append(b, 6);
    pesd->Append(p, 2+c);
    int type = p[2] & 0x1f;
    while ((p < pEnd) && (type != 8))
    {
        p += 2+c;
        c = (p[0] << 8) | p[1];
        type = p[2] & 0x1f;
    }
    if ((type == 8) && ((p+2+c) <= pEnd))
    {
        b[0] = 1;   // 1 PPS
        pesd->Append(b, 1);
        pesd->Append(p, 2+c);
    }
    pesd->Close();

    psd->Close();
}

#pragma pack(push, 1)
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

inline USHORT Swap2Bytes(int x)
{
	return (USHORT) (((x & 0xff) << 8) | ((x >> 8) & 0xff));
}
inline DWORD Swap4Bytes(DWORD x)
{
	return ((x & 0xff) << 24) |
		   ((x & 0xff00) << 8) |
		   ((x & 0xff0000) >> 8) |
		   ((x >> 24) & 0xff);
}

long 
FOURCCVideoHandler::Width()
{
	if (*m_mt.FormatType() == FORMAT_VideoInfo)
	{
		VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
		return pvi->bmiHeader.biWidth;
	}
	else if (*m_mt.FormatType() == FORMAT_VideoInfo2)
	{
		VIDEOINFOHEADER2* pvi = (VIDEOINFOHEADER2*)m_mt.Format();
		return pvi->bmiHeader.biWidth;
	}
	else
	{
		return 0;
	}
}

long 
FOURCCVideoHandler::Height()
{
	if (*m_mt.FormatType() == FORMAT_VideoInfo)
	{
		VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
		return abs(pvi->bmiHeader.biHeight);
	}
	else if (*m_mt.FormatType() == FORMAT_VideoInfo2)
	{
		VIDEOINFOHEADER2* pvi = (VIDEOINFOHEADER2*)m_mt.Format();
		return abs(pvi->bmiHeader.biHeight);
	}
	else
	{
		return 0;
	}
}

BYTE DefaultHuffTable[] = 
{
    0xff, 0xc4, 0x01, 0xa2,

    0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,

    0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,

    0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7d,
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa,

    0x11, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77,
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
    0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};
const int DefaultHuffSize = sizeof(DefaultHuffTable);

void
FOURCCVideoHandler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
	UNREFERENCED_PARAMETER(scale);
	UNREFERENCED_PARAMETER(dataref);
	UNREFERENCED_PARAMETER(id);

	FOURCCMap fcc = m_mt.Subtype();

	DWORD codec;
	if(*m_mt.Subtype() == MEDIASUBTYPE_RGB24 || *m_mt.Subtype() == MEDIASUBTYPE_RGB32)
	{
		codec = BI_RGB;
	} else
	{
		codec = fcc.GetFOURCC();
		if (m_bMJPG)
		{
			codec = MAKEFOURCC('m', 'j', 'p', 'a'); // mjpa
		}
		else if (codec == MAKEFOURCC('M', 'J', 'P', 'G')) // MJPG
		{
			// we didn't need the APP1 insertion,
			// so call it Photo JPEG.
			codec = MAKEFOURCC('j', 'p', 'e', 'g'); // jpeg
		}
	}
	smart_ptr<Atom> psd = patm->CreateAtom(Swap4Bytes(codec));

	int cx, cy, depth;
	if (*m_mt.FormatType() == FORMAT_VideoInfo)
	{
		VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
		cx = pvi->bmiHeader.biWidth;
		cy = abs(pvi->bmiHeader.biHeight);
		depth = pvi->bmiHeader.biBitCount;
	}
	else if (*m_mt.FormatType() == FORMAT_VideoInfo2)
	{
		VIDEOINFOHEADER2* pvi = (VIDEOINFOHEADER2*)m_mt.Format();
		cx = pvi->bmiHeader.biWidth;
		cy = abs(pvi->bmiHeader.biHeight);
		depth = pvi->bmiHeader.biBitCount;
	}
	else
	{
		return;
	}

	QTVideo fmt;
	ZeroMemory(&fmt, sizeof(fmt));
	// remember we must byte-swap all data
	fmt.width = Swap2Bytes(cx);
	fmt.height = Swap2Bytes(cy);
	fmt.bit_depth = Swap2Bytes(depth);

	fmt.dataref = Swap2Bytes(1);
	fmt.vert_resolution = fmt.horz_resolution = Swap4Bytes(0x00480000);
	fmt.frames_per_sample  = Swap2Bytes(1);
	fmt.colour_table_id = 0xffff;

	// pascal string codec name
	const char* pName;
	if (m_bMJPG)
	{
		pName = "Motion JPEG";
		fmt.spatial_compression = Swap4Bytes(512);
	}
	else if (codec == MAKEFOURCC('j', 'p', 'e', 'g')) // jpeg
	{
		pName = "Photo JPEG";
	}
	else if (codec == BI_RGB)
	{
		pName = "RGB Video";
	}
	else
	{
		pName = "YUV Video";
	}
	int cch = lstrlenA(pName);
	CopyMemory(&fmt.codec_name[1], pName, cch);
	fmt.codec_name[0] = (BYTE)cch;

	psd->Append((const BYTE*)&fmt, sizeof(fmt));
	if (m_bMJPG)
	{
		smart_ptr<Atom> pfiel = psd->CreateAtom(DWORD('fiel'));
		BYTE b[] = {2, 1};
		pfiel->Append(b, sizeof(b));
		pfiel->Close();
	}

	psd->Close();
}

// Quicktime and FCP require an APP1 marker for dual-field.
// Blackmagic and Microsoft decoders require APP0. 
// The Blackmagic decoder creates APP0.
// We will insert both and fix up one if present.
// If neither are present, finding the end of the image is slow.

// big-endian values.
struct APP1
{
	WORD	marker;
	WORD	length;
	DWORD	reserved;
	DWORD	tag;
	DWORD	fieldsize;
	DWORD	paddedsize;
	DWORD	nextfield;
	DWORD	quantoffset;
	DWORD	huffoffset;
	DWORD	sofoffset;
	DWORD	sosoffset;
	DWORD	dataoffset;
};

struct APP0
{
	WORD marker;
	WORD length;
	DWORD tag;
	BYTE polarity;
	BYTE reserved;
	DWORD paddedsize;
	DWORD fieldsize;
};

HRESULT FOURCCVideoHandler::WriteData(Atom* patm, const BYTE* pData, int cBytes, int* pcActual)
{
	if (!m_bProcessMJPG)
	{
		return __super::WriteData(patm, pData, cBytes, pcActual);
	}
	if ((cBytes < 2) || (pData[0] != 0xff) || (pData[1] != 0xD8))
	{
		return VFW_E_INVALIDMEDIATYPE;
	}
	
	APP1 header;
	ZeroMemory(&header, sizeof(header));
	header.marker = Swap2Bytes(0xffe1);
	header.length = Swap2Bytes(sizeof(APP1) - 2);
	header.tag = Swap4Bytes(DWORD('mjpg'));

	APP0 app0;
	ZeroMemory(&app0, sizeof(app0));
	app0.marker = Swap2Bytes(0xffe0);
	app0.length = Swap2Bytes(sizeof(app0) - 2);
	app0.tag = Swap4Bytes(DWORD('AVI1'));
	app0.polarity = 1;
	
	int total = 0;
	const BYTE* fieldstart = pData;
	int cBytesTotal = cBytes;

	// point at which we insert APP0 and/or APP1.
	const BYTE* pInsertBefore = 0;

	// we can use the alternate variant to find the EOI, 
	// and in any case we need to adjust for insertion
	APP0* pAPP0 = NULL;
	APP1* pAPP1 = NULL;

	while (cBytes > 0)
	{
		int markerlen = 1;
		if (*pData == 0xFF)
		{
			if (cBytes < 2)
			{
				// HOTFIX: Microsoft stock MJPEG Compressor Filter https://msdn.microsoft.com/en-us/library/windows/desktop/dd390690
				//         adds extra 0xFF padding which causes underflow here and results in bad unplayable output; the hotfix is mostly
				//         fine and files are accepted by most players, even though Windows 10 Movies and TV won't like it anyway.
				//         Note this is a problem of Motion JPEG compressor, since alternate compresors are free from this issue and the MP4 is OK
				if(cBytes == 1)
					break;
				return VFW_E_BUFFER_UNDERFLOW;
			} 
			if (pData[1] == 0xff)
			{
				markerlen = 1;
			}
			else if (pData[1] == 0)
			{
				markerlen = 2;
			}
			else
			{
				BYTE m = pData[1];
				markerlen = 2;
				if ((m < 0xD0) || (m > 0xD9))
				{
					if (cBytes < 4)
					{
						return VFW_E_BUFFER_UNDERFLOW;
					}
					markerlen += (pData[2] << 8) | pData[3];
				}

				// if there's an existing APP0 or APP1,
				// we may need to fix it up.
				if (m == 0xE0)
				{
					pInsertBefore = pData + markerlen;
					if (memcmp(pData+4, "AVI1", 4) == 0)
					{
						pAPP0 = (APP0*) pData;
					}
				}
				if (m == 0xE1)
				{
					pInsertBefore = pData;
					if (memcmp(pData + 8, "mjpg", 4) == 0)
					{
						pAPP1 = (APP1*) pData;
					}
				}
				
				int offset = int(pData - fieldstart);
				if (m == 0xC0)
				{
					header.sofoffset = Swap4Bytes(offset);
				}
				else if (m == 0xC4)
				{
					header.huffoffset = Swap4Bytes(offset);
				}
				else if (m == 0xDB)
				{
					header.quantoffset = Swap4Bytes(offset);
				}
				else if (m == 0xDA)
				{
					header.sosoffset = Swap4Bytes(offset);
					header.dataoffset = Swap4Bytes(offset + markerlen);
					// if either APPx header is present, we don't need to
					// slowly scan the entire frame
					if (pAPP0 != NULL)
					{
						DWORD oldlen = Swap4Bytes(pAPP0->fieldsize);
						const BYTE* EOI = fieldstart + oldlen - 2;
						markerlen = int(EOI - pData);
					}
					else if (pAPP1 != NULL)
					{
						DWORD oldlen = Swap4Bytes(pAPP1->fieldsize);
						const BYTE* EOI = fieldstart + oldlen - 2;
						markerlen = int(EOI - pData);
					}
				}
				else if (m == 0xD9)
				{
					header.fieldsize = Swap4Bytes(offset + markerlen);
				}
				if ((m == 0xD8) || (cBytes == markerlen))
				{
					if (pData != fieldstart)
					{
						bool bInsertHuff = false;
						int bytesInserted = 0;
						if (header.huffoffset == 0)
						{
							// will need to insert a default
							bInsertHuff = true;
							bytesInserted = DefaultHuffSize;
						}
						if (!pAPP0)
						{
							bytesInserted += sizeof(APP0);
						}
						if (!pAPP1)
						{
							bytesInserted += sizeof(APP1);
						}

						// any insertions needed?
						if (bytesInserted)
						{
							offset += bytesInserted;
							// move existing pointers down
							header.fieldsize = Swap4Bytes(Swap4Bytes(header.fieldsize) + bytesInserted);
							header.sofoffset = Swap4Bytes(Swap4Bytes(header.sofoffset) + bytesInserted);
							header.quantoffset = Swap4Bytes(Swap4Bytes(header.quantoffset) + bytesInserted);
							header.sosoffset = Swap4Bytes(Swap4Bytes(header.sosoffset) + bytesInserted);
							header.dataoffset = Swap4Bytes(Swap4Bytes(header.dataoffset) + bytesInserted);
						}
						else
						{
							// no modification necessary (still MJPEG but no processing needed)
							m_bProcessMJPG = false;
							return __super::WriteData(patm, fieldstart, cBytesTotal, pcActual);
						}
						const BYTE* pNextFrame = pData;
						if (m == 0xd8)
						{
							// marker is start of next field
							header.paddedsize = Swap4Bytes(offset);
							header.nextfield = header.paddedsize;
						}
						else
						{
							// single field?
							if (total == 0)
							{
								// no modification necessary (treat as Photo JPEG not MJPEG)
								m_bMJPG = false;
								m_bProcessMJPG = false;
								return __super::WriteData(patm, fieldstart, cBytesTotal, pcActual);
							}
							// marker is last in present field
							header.paddedsize = Swap4Bytes(offset + markerlen);
							pNextFrame += markerlen;
						}

						// APP0 is before the insertion point, if already present, so fix up before
						// writing it out
						if (pAPP0)
						{
							// need to update field size in alternate header
							pAPP0->fieldsize = Swap4Bytes(Swap4Bytes(pAPP0->fieldsize) + bytesInserted);
							pAPP0->paddedsize = Swap4Bytes(Swap4Bytes(pAPP0->paddedsize) + bytesInserted);
						}

						// find insertion point and write out data that comes before that
						// Then we can fix up the huffman location if needed
						if (pInsertBefore == NULL)
						{
							// insert immediately after SOI
							pInsertBefore = fieldstart + 2;
						}
						int len = int(pInsertBefore - fieldstart);
						HRESULT hr = patm->Append(fieldstart, len);
						if (FAILED(hr))
						{
							return hr;
						}
						total += len;
						if (bInsertHuff)
						{
							int hufloc = len;
							if (!pAPP0)
							{
								hufloc += sizeof(APP0);
							}
							if (!pAPP1)
							{
								hufloc += sizeof(APP1);
							}
							header.huffoffset = Swap4Bytes(hufloc);
						}

						if (pAPP1)
						{
							pAPP1->fieldsize = Swap4Bytes(Swap4Bytes(pAPP1->fieldsize) + bytesInserted);
							pAPP1->paddedsize = Swap4Bytes(Swap4Bytes(pAPP1->paddedsize) + bytesInserted);
							pAPP1->quantoffset = Swap4Bytes(Swap4Bytes(pAPP1->quantoffset) + bytesInserted);
							if (pAPP1->huffoffset)
							{
								pAPP1->huffoffset = Swap4Bytes(Swap4Bytes(pAPP1->huffoffset) + bytesInserted);
							}
							else if (bInsertHuff)
							{
								pAPP1->huffoffset = header.huffoffset;
							}

							pAPP1->sofoffset = Swap4Bytes(Swap4Bytes(pAPP1->sofoffset) + bytesInserted);
							pAPP1->sosoffset = Swap4Bytes(Swap4Bytes(pAPP1->sosoffset) + bytesInserted);
							pAPP1->dataoffset = Swap4Bytes(Swap4Bytes(pAPP1->dataoffset) + bytesInserted);
							if (pAPP1->nextfield)
							{
								pAPP1->nextfield = Swap4Bytes(Swap4Bytes(pAPP1->nextfield) + bytesInserted);
							}
						}

						// APP0
						if (!pAPP0)
						{
							app0.fieldsize = header.fieldsize;
							app0.paddedsize = header.paddedsize;

							hr = patm->Append((const BYTE*)&app0, sizeof(APP0));
							if (FAILED(hr))
							{
								return hr;
							}
							total += sizeof(APP0);
						}


						// APP1
						if (!pAPP1)
						{
							hr = patm->Append((const BYTE*)&header, sizeof(header));
							if (FAILED(hr))
							{
								return hr;
							}
							total += sizeof(header);
						}

						if (bInsertHuff)
						{
							hr = patm->Append(DefaultHuffTable, DefaultHuffSize);
							if (FAILED(hr))
							{
								return hr;
							}
							total += DefaultHuffSize;
						}

						// rest of field
						len = int(pNextFrame - pInsertBefore);
						hr = patm->Append(pInsertBefore, len);
						total += len;
						if (FAILED(hr))
						{
							return hr;
						}

						// set up for next field

						fieldstart = pNextFrame;
						pInsertBefore = NULL;
						pAPP0 = NULL;
						pAPP1 = NULL;
						ZeroMemory(&header, sizeof(header));
						header.marker = Swap2Bytes(0xffe1);
						header.length = Swap2Bytes(sizeof(APP1) - 2);
						header.tag = Swap4Bytes(DWORD('mjpg'));
					}
				}
			}
		}
		pData += markerlen;
		cBytes -= markerlen;
	}
	*pcActual = total;
	return S_OK;
}

#pragma pack(pop)

long 
WaveHandler::Scale()
{
    // for audio, the scale should be the sampling rate but
    // must not exceed 65535
    WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_mt.Format();
    if (pwfx->nSamplesPerSec > 65535)
    {
        return 45000;
    }
    else
    {
        return pwfx->nSamplesPerSec;
    }
}

long WaveHandler::BlockAlign()
{
	WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_mt.Format();
	return pwfx->nBlockAlign;
}

bool WaveHandler::IsOldIndexFormat() 
{
	WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_mt.Format();

	if ((pwfx->wFormatTag == WAVE_FORMAT_PCM) && (pwfx->nSamplesPerSec < 65536) && (pwfx->nChannels <= 2))
	{
		return true;
	}
	return false;
}

void 
WaveHandler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
	WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_mt.Format();

	if (IsOldIndexFormat())
	{
	    smart_ptr<Atom> psd = patm->CreateAtom(DWORD('sowt'));
		BYTE b[44];
		ZeroMemory(b, 44);
		WriteShort(dataref, b+6);
		WriteShort(1, b+8);		// ver 1 of sound sample desc
		WriteShort(pwfx->nChannels, b+16);
		short bits = (pwfx->wBitsPerSample == 8) ? 8 : 16;
		WriteShort(bits, b+18);
		WriteShort(0xffff, b+20);
		WriteShort(pwfx->nSamplesPerSec, b+24);    // this is what forces us to use short audio scales

		short bytesperchan = pwfx->wBitsPerSample / 8;
		WriteLong(1, b+28);
		WriteLong(bytesperchan, b+32);
		WriteLong(bytesperchan * pwfx->nChannels, b+36);
		WriteLong(2, b+40);

		psd->Append(b, 44);
	    psd->Close();
	}
	else
	{
		DWORD dwAtom = 0;
		if (pwfx->wFormatTag == WAVE_FORMAT_PCM)
		{
			dwAtom = 'lpcm';
		} else if (pwfx->wFormatTag == WAVE_FORMAT_MULAW)
		{
			dwAtom = 'ulaw';
		} else if (pwfx->wFormatTag == WAVE_FORMAT_ALAW)
		{
			dwAtom = 'alaw';
		}
	    smart_ptr<Atom> psd = patm->CreateAtom(dwAtom);
		BYTE b[28];
		ZeroMemory(b, 28);
		WriteShort(dataref, b+6);
		WriteShort(2, b+16);
		WriteShort(16, b+18);
		WriteShort(unsigned short(scale), b+24);    // this is what forces us to use short audio scales
		psd->Append(b, 28);

		smart_ptr<Atom> pesd = psd->CreateAtom('esds');
		WriteLong(0, b);        // ver/flags
		pesd->Append(b, 4);
		// es descr
		//      decoder config
		//          <objtype/stream type/bitrates>
		//          decoder specific info desc
		//      sl descriptor
		Descriptor es(Descriptor::ES_Desc);
		WriteShort(id, b);
		b[2] = 0;
		es.Append(b, 3);
		Descriptor dcfg(Descriptor::Decoder_Config); // ISO 14496-1 8.3.4
		b[0] = 0xC0;    // custom object type
		b[1] = (5 << 2) | 1;    // audio stream; streamType, ISO 14496-1 8.3.4.1

		// buffer size 15000
		b[2] = 0;
		b[3] = 0x3a;
		b[4] = 0x98;
		WriteLong(1500000, b+5);    // max bitrate
		WriteLong(0, b+9);          // avg bitrate 0 = variable
		dcfg.Append(b, 13);
		Descriptor dsi(Descriptor::Decoder_Specific_Info); // ISO 14496-1 8.3.5

		// write whole WAVEFORMATEX as decoder specific info
		int cLen = pwfx->cbSize + sizeof(WAVEFORMATEX);
		dsi.Append((const BYTE*)pwfx, cLen);
		dcfg.Append(&dsi);
		es.Append(&dcfg);
		Descriptor sl(Descriptor::SL_Config); // ISO 14496-1 8.3.6, 10.2.3
		b[0] = 2; // Reserved for ISO use???
		b[1] = 0x7F; // OCRStreamFlag 0, Reserved 1111111
		sl.Append(b, 2);
		es.Append(&sl);
		es.Write(pesd);
		pesd->Close();
	    psd->Close();
	}
}

bool 
WaveHandler::CanTruncate()
{
    WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_mt.Format();
    if (pwfx->wFormatTag == WAVE_FORMAT_PCM)
    {
		return true;
	}
	return false;
}

bool 
WaveHandler::Truncate(IMediaSample* pSample, REFERENCE_TIME tNewStart)
{
	if (!CanTruncate())
	{
		return false;
	}
	REFERENCE_TIME tStart, tEnd;
	if (pSample->GetTime(&tStart, &tEnd) != S_OK)
	{
		// VFW_S_NO_STOP_TIME: if the demux is not able to work out the stop time, then we will not truncate.
		return false;
	}
    WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_mt.Format();
	LONGLONG tDiff = tNewStart - tStart;
	long cBytesExcess = long (tDiff * pwfx->nSamplesPerSec / UNITS) * pwfx->nBlockAlign;
	long cData = pSample->GetActualDataLength();
	BYTE* pBuffer;
	pSample->GetPointer(&pBuffer);
	MoveMemory(pBuffer, pBuffer+cBytesExcess, cData - cBytesExcess);
	pSample->SetActualDataLength(cData - cBytesExcess);
	pSample->SetTime(&tNewStart, &tEnd);
	return true;

}

///////////////////////////////////////////////////////////////////////////////
// MP4 bitstream helper class

class MP4BitstreamWriter : public BitstreamWriter
{
public:
	MP4BitstreamWriter(void) : 
		BitstreamWriter(m_bits, sizeof(m_bits)) {}
	
	MP4BitstreamWriter(BYTE *pData, int cbData) : 
		BitstreamWriter(pData, cbData) {}

	//void WriteLanguage(LPCSTR pszLanguage);
	
	HRESULT AppendTo(Atom* pAtom, long cbLength = -1);
	void AppendTo(Descriptor* pDesc, long cbLength = -1);


protected:
	BYTE m_bits[256];
};
	
/*
void MP4BitstreamWriter::WriteLanguage(LPCSTR pszLanguage)
{
	// Write packed ISO-639-2/T language code.
	// http://en.wikipedia.org/wiki/List_of_ISO_639-2_codes ISO 639-2/T

	// 8.4.2.2 - Media Header Box - Syntax
	char ch;
	int lang = 0;

	for (int i = 0; i < 3; i++)
	{
		ch = *pszLanguage++;
		
		ASSERT(ch >= 'a' && ch <= 'z');

		lang = (lang << 5) | (ch - 0x60);
	}

	Write16(lang);	// pad:				= 0
					// language:5 [3]	= ISO-639-2/T language code
}
*/

HRESULT MP4BitstreamWriter::AppendTo(Atom* pAtom, long cbLength)
{
	ASSERT(pAtom);
	ASSERT(m_pBits);

	if (cbLength < 0)
		cbLength = GetByteCount();

	ASSERT(cbLength <= static_cast<long>(m_cMaxBits >> 3));
	return pAtom->Append(m_pBits, cbLength);
}

void MP4BitstreamWriter::AppendTo(Descriptor* pDesc, long cbLength)
{
	ASSERT(pDesc);
	ASSERT(m_pBits);

	if (cbLength < 0)
		cbLength = GetByteCount();

	ASSERT(cbLength <= static_cast<long>(m_cMaxBits >> 3));
	pDesc->Append(m_pBits, cbLength);
}

///////////////////////////////////////////////////////////////////////////////
// MPEG-2 Video support

LONGLONG MPEG2VideoHandler::FrameDuration()
{
	MPEG2VIDEOINFO* pvi = reinterpret_cast<MPEG2VIDEOINFO*>(m_mt.Format());
	return pvi->hdr.AvgTimePerFrame;
}

long MPEG2VideoHandler::Width()
{
	MPEG2VIDEOINFO* pvi = reinterpret_cast<MPEG2VIDEOINFO*>(m_mt.Format());
	return pvi->hdr.bmiHeader.biWidth;
}

long MPEG2VideoHandler::Height()
{
	MPEG2VIDEOINFO* pvi = reinterpret_cast<MPEG2VIDEOINFO*>(m_mt.Format());
	return abs(pvi->hdr.bmiHeader.biHeight);
}

// ISO/IEC 14496-1 - Table 8-5: objectProfileIndication Values
// See also: http://www.mp4ra.org/object.html

int GetMPEG2VideoObjectTypeId(MPEG2VIDEOINFO* pvi)
{
	static const struct tagMPEG2ProfileToOID
	{
		int objectTypeIdentifier;
		DWORD dwProfile;
	}
	aProfileToOidMap[] =
	{
		0x60, AM_MPEG2Profile_Simple,				// Visual ISO/IEC 13818-2 Simple Profile
		0x61, AM_MPEG2Profile_Main,					// Visual ISO/IEC 13818-2 Main Profile
		0x62, AM_MPEG2Profile_SNRScalable,			// Visual ISO/IEC 13818-2 SNR Profile
		0x63, AM_MPEG2Profile_SpatiallyScalable,	// Visual ISO/IEC 13818-2 Spatial Profile
		0x64, AM_MPEG2Profile_High,					// Visual ISO/IEC 13818-2 High Profile
		//0x65, n/a,								// Visual ISO/IEC 13818-2 422 Profile 
	};

	if (pvi)
	{
		for (size_t i = 0; i < _countof(aProfileToOidMap); i++)
		{
			if (pvi->dwProfile == aProfileToOidMap[i].dwProfile)
			{
				return aProfileToOidMap[i].objectTypeIdentifier;
			}
		}
	}

	return 0;
}

void MPEG2VideoHandler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
    UNREFERENCED_PARAMETER(scale);

	MPEG2VIDEOINFO* pvi = reinterpret_cast<MPEG2VIDEOINFO*>(m_mt.Format());
    int width = pvi->hdr.bmiHeader.biWidth;
    int height = abs(pvi->hdr.bmiHeader.biHeight);

	// SampleEntry
	// ISO/IEC 14496-12 - 8.5.2.2 - Syntax
	MP4BitstreamWriter bsw;
	bsw.ReserveBytes(6);						//  0- 5: reserved:8 [6]		= 0
	bsw.Write16(static_cast<short>(dataref));	//  6- 7: data-reference-index:16

	// VisualSampleEntry
	// ISO/IEC 14496-12 - 8.5.2.2 - Syntax
	bsw.Write16(0);								//  8- 9: pre_defined:16		= 0
	bsw.Write16(0);								// 10-11: reserved:16			= 0
	bsw.ReserveBytes(12);						// 12-23: pre_defined:32 [3]	= 0
    bsw.Write16(static_cast<short>(width));		// 24-25: width:16
    bsw.Write16(static_cast<short>(height));	// 26-27: height:16
    bsw.Write32(0x00480000);					// 28-31: horizresolution:32	= 0x00480000 (72dpi)
    bsw.Write32(0x00480000);					// 32-35: vertresolution:32		= 0x00480000 (72dpi)
	bsw.Write32(0);								// 36-39: reserved:32			= 0
    bsw.Write16(1);								// 40-41: Frames count:16		= 1
	bsw.ReserveBytes(32);						// 42-73: compressorname:8 [32] = 0
    bsw.Write16(0x0018);						// 74-75: depth:16				= 0x0018 (24bpp)
    bsw.Write16(-1);							// 76-77: pre_defined:16		= -1

	// MP4VisualSampleEntry
	// ISO/IEC 14496-14 - 5.6.1 - Syntax
    smart_ptr<Atom> psd = patm->CreateAtom('mp4v');
	bsw.AppendTo(psd);

	// ESDescriptor Box
	smart_ptr<Atom> pesd = psd->CreateAtom('esds');
	bsw.Rewind();
	bsw.Write32(0);								//  0- 1: version:8				= 0
												//  1- 3: flags:24				= 0
	bsw.AppendTo(pesd);

	// ISO/IEC 14496-1 - 8.3.3 - ES_Descriptor
	// TODO: MPEG-4 ESDescriptor
    Descriptor es(Descriptor::ES_Desc);
	bsw.Rewind();
	bsw.Write16(static_cast<short>(id));		// ES_ID: 16					= Track Id
    bsw.Write8(0);								// flags:8
												//		streamDependenceFlag:1	= 0
												//		URL_Flag:1				= 0
												//		OCRstreamFlag:1			= 0
												//		streamPriority:5		= 0
    bsw.AppendTo(&es);

	// DecoderConfigDescriptor
	// ISO/IEC 14496-1 - 8.3.4 - DecoderConfigDescriptor 
    Descriptor dcfg(Descriptor::Decoder_Config);
	BYTE oid = static_cast<BYTE>(GetMPEG2VideoObjectTypeId(pvi));
	bsw.Rewind();
	bsw.Write8(oid);							// objectProfileIndication:8
	bsw.Write(0x04, 6);							// streamType:6		= 0x04 (VisualStream)
	bsw.Write(1, 2);							// upStream:1		= 0
												// reserved:1		= 1
    bsw.Write24(0x100000);						// bufferSizeDB:24	= 0x100000 (1024 * 1024)???
    bsw.Write32(0x7fffffff);					// maxBitRate:32
    bsw.Write32(0);								// avgBitRate:32	= 0 (variable)
    bsw.AppendTo(&dcfg);

	// TODO: remove sequence headers from media samples see ISO/IEC 14496-14 (just before 5.6.1 Syntax)
	// ISO/IEC 14496-1 - 8.3.5 - DecoderSpecificInfo
	if (pvi->cbSequenceHeader > 0)
	{
		Descriptor dsi(Descriptor::Decoder_Specific_Info);
		BYTE *pExtra = reinterpret_cast<BYTE*>(&pvi->dwSequenceHeader);
		dsi.Append(pExtra, pvi->cbSequenceHeader);
		dcfg.Append(&dsi);
	}

    es.Append(&dcfg);
    
	// SLConfigDescriptor (mandatory)
	// ISO/IEC 14496-1 - 8.3.6 - SLConfigDescriptor
	// ISO/IEC 14496-1 - 10.2.3 SL Packet Header Configuration
	Descriptor sl(Descriptor::SL_Config);
	bsw.Rewind();
	bsw.Write8(2);								// predefined:8		= 2
	bsw.Write8(0x7F);							// OCRstreamFlag:1	= 0
												// reserved:7		= 0b1111.111 (0x7F) TODO: ????
	bsw.AppendTo(&sl);
    
	es.Append(&sl);
	es.Write(pesd);
	
	pesd->Close();
    psd->Close();
}

///////////////////////////////////////////////////////////////////////////////
// MPEG-2 Audio support

long MPEG2AudioHandler::Scale()
{
    // for audio, the scale should be the sampling rate but
    // must not exceed 65535
	WAVEFORMATEX* pwfx = reinterpret_cast<WAVEFORMATEX*>(m_mt.Format());
    if (pwfx->nSamplesPerSec > 65535)
    {
        return 45000;
    }
    else
    {
        return pwfx->nSamplesPerSec;
    }
}


// ISO/IEC 14496-1 - Table 8-5: objectProfileIndication Values
// See also: http://www.mp4ra.org/object.html

int GetMPEG2AudioObjectTypeId(WAVEFORMATEX* pwfx)
{
	ASSERT(pwfx);

	if (pwfx != NULL && pwfx->wFormatTag == WAVE_FORMAT_MPEG)
	{
		MPEG1WAVEFORMAT *pmwf = reinterpret_cast<MPEG1WAVEFORMAT*>(pwfx);

		// TODO: is this reliable enough?
		if (pmwf->fwHeadFlags & ACM_MPEG_ID_MPEG1)
		{
			return 0x6B; // Audio ISO/IEC 11172-3
		}
		else
		{
			return 0x69; // Audio ISO/IEC 13818-3
		}
	}

	return 0;
}

void MPEG2AudioHandler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
	WAVEFORMATEX* pwfx = reinterpret_cast<WAVEFORMATEX*>(m_mt.Format());

	// SampleEntry
	// ISO/IEC 14496-12 - 8.5.2.2 - Syntax
	MP4BitstreamWriter bsw;
	bsw.ReserveBytes(6);						//  0- 5: reserved:8 [6]		= 0
	bsw.Write16(static_cast<short>(dataref));	//  6- 7: data-reference-index:16

	// AudioSampleEntry
	// ISO/IEC 14496-12 - 8.5.2.2 - Syntax
	bsw.ReserveBytes(8);						//  8-15: reserved:32 [2]		= 0;
	bsw.Write16(pwfx->nChannels);				// 16-17: channelcount:16		= 2
												// 18-19: samplesize:16			= 16
	bsw.Write16(pwfx->wBitsPerSample ? pwfx->wBitsPerSample : 16);
	bsw.Write32(0);								// 20-21: pre_defined:16		= 0
												// 22-23: reserved:16			= 0
	bsw.Write32(scale << 16);					// 24-27: samplerate:32			= { default samplerate of media} << 16
	//bsw.Write32(pwfx->nSamplesPerSec << 16);
	
	
	// MP4AudioSampleEntry
	// ISO/IEC 14496-14 - 5.6.1 - Syntax
    smart_ptr<Atom> psd = patm->CreateAtom('mp4a');
	bsw.AppendTo(psd);

	// ES_Descr box
	smart_ptr<Atom> pesd = psd->CreateAtom('esds');
	bsw.Rewind();
	bsw.Write32(0);								//  0   : version:8				= 0
												//  1- 3: flags:24				= 0
	bsw.AppendTo(pesd);

	// ES_Descr
    Descriptor es(Descriptor::ES_Desc);
	bsw.Rewind();
	bsw.Write16(static_cast<short>(id));		// ES_ID: 16					= Track Id
    bsw.Write8(0);								// flags:8
												//		streamDependenceFlag:1	= 0
												//		URL_Flag:1				= 0
												//		reserved:1				= 1 or OCRstreamFlag=0??? TODO: ???
												//		streamPriority:5		= 0
    bsw.AppendTo(&es);

	// DecoderConfigDescriptor
	// ISO/IEC 14496-1 - 8.3.4 - DecoderConfigDescriptor 
    Descriptor dcfg(Descriptor::Decoder_Config);
	BYTE oid = static_cast<BYTE>(GetMPEG2AudioObjectTypeId(pwfx));
	bsw.Rewind();
	bsw.Write8(oid);							// objectProfileIndication:8
	bsw.Write(0x05, 6);							// streamType:6		= 0x05 (AudioStream)
	bsw.Write(1, 2);							// upStream:1		= 0
												// reserved:1		= 1
    bsw.Write24(0x003A98);						// bufferSizeDB:24	= 0x003A98 (15000), TODO: ???
    bsw.Write32(1500000);						// maxBitRate:32
    bsw.Write32(0);								// avgBitRate:32	= 0 (variable)
    bsw.AppendTo(&dcfg);

	// TODO: ???? ISO/IEC 14496-1 - 8.3.5 - DecoderSpecificInfo
////////////
// 
/*
	if (pwfx != NULL && pwfx->wFormatTag == WAVE_FORMAT_MPEG)
	{
		MPEG1WAVEFORMAT *pmwf = reinterpret_cast<MPEG1WAVEFORMAT*>(pwfx);
		Descriptor dsi(Descriptor::Decoder_Specific_Info);
		bsw.Rewind();
		bsw.Write16(pmwf->fwHeadLayer);
		bsw.Write32(pmwf->dwHeadBitRate);
		bsw.Write16(pmwf->fwHeadMode);
		bsw.Write16(pmwf->fwHeadModeExt);
		bsw.Write16(pmwf->wHeadEmphasis);
		bsw.Write16(pmwf->fwHeadFlags);
		bsw.Write32(pmwf->dwPTSLow);
		bsw.Write32(pmwf->dwPTSHigh);
		bsw.AppendTo(&dsi);
		dcfg.Append(&dsi);
	}
*/
/*
    long cExtra = m_mt.FormatLength() - sizeof(WAVEFORMATEX);

	if (cExtra > 0)
    {
		Descriptor dsi(Descriptor::Decoder_Specific_Info);
		BYTE* pExtra = m_mt.Format() + sizeof(WAVEFORMATEX);
        dsi.Append(pExtra, cExtra);
		dcfg.Append(&dsi);
	}
*/
/*
	Descriptor dsi(Descriptor::Decoder_Specific_Info);
	int cbSize = pwfx->cbSize + sizeof(WAVEFORMATEX);
	dsi.Append(reinterpret_cast<const BYTE*>(pwfx), cbSize);
	dcfg.Append(&dsi);
*/
	es.Append(&dcfg);
    
	// ISO/IEC 14496-1 - 8.3.6 - SLConfigDescriptor
	// ISO/IEC 14496-1 - 10.2.3 SL Packet Header Configuration
	Descriptor sl(Descriptor::SL_Config);
	bsw.Rewind();
	bsw.Write8(2);								// predefined:8		= 2
	bsw.Write8(0x7F);							// OCRstreamFlag:1	= 0 TODO: ???
												// reserved:7		= 0b1111.111 (0x7F) TODO: ????
	bsw.AppendTo(&sl);

    es.Append(&sl);
    es.Write(pesd);
    
	pesd->Close();
    psd->Close();
}
	
///////////////////////////////////////////////////////////////////////////////
// Dolby Digital (AC-3) Audio support
// ETSI TS 102 366: Digital Audio Compression (AC-3, Enhanced AC-3) Standard
// http://www.etsi.org/deliver/etsi_ts/102300_102399/102366/01.02.01_60/ts_102366v010201p.pdf

class DolbyBitstreamReader : public BitstreamReader
{
public:
	DolbyBitstreamReader(const BYTE* pData, int cbData) :
		BitstreamReader(pData, cbData) {}

	BOOL IsSyncword(void);
};

BOOL DolbyBitstreamReader::IsSyncword(void)
{
	m_bBigEndian = TRUE; // Force BE by default.

	// 4.3.1 - syncinfo - Synchronization information
	int syncword = Read16();

	if (syncword == 0x0B77)
	{
		// Big Endian stream.
		return TRUE;
	}

	if (syncword == 0x770B)
	{
		// Little Endian stream.
		m_bBigEndian = FALSE;
		return TRUE;
	}

	return FALSE;
}

AC3StreamInfo::AC3StreamInfo(void) :
	fscod(0), 
	frmsizcod(0),
	bsid(0),
	bsmod(0),
	acmod(0),
	lfeon(0)
{
}

BOOL AC3StreamInfo::Parse(const BYTE* pData, int cbData)
{
	// Retrieve some required information from the stream.
	DolbyBitstreamReader bsr(pData, cbData);

	// 4.3.1 - syncinfo - Synchronization information
	if (!bsr.IsSyncword())			// syncword:16 = 0x0B77
		return FALSE;

	bsr.Skip(16);					// crc1:16

	fscod = bsr.Read(2);			// fscod:2 - Sampling rate: 00=48K, 01=44.1K, 10=32K, 11=reserved

	if (fscod == 0x3)
		return FALSE;

	frmsizcod = bsr.Read(6);		// frmsizecod:6 - # of words before next syncword (see Table 4.13).

	
	// 4.3.2 - bsi - Bit stream information
	bsid  = bsr.Read(5);			// bsid:5  - Bit stream identification
	bsmod = bsr.Read(3);			// bsmod:3 - Bit stream mode
	acmod = bsr.Read(3);			// acmod:3 - Audio coding mode

	// if 3 front channels, read {cmixlev}
	if ((acmod & 0x1) && (acmod != 0x1)) 
	{
		bsr.Skip(2);
	}

	// if a surround channel exists, read {surmixlev}
	if (acmod & 0x4)
	{
		bsr.Skip(2);
	}

	// if in 2/0 mode, read {dsurmod}
	if (acmod == 0x2) 
	{
		bsr.Skip(2);
	}

	lfeon = bsr.Read(1);

	return TRUE;
}

long DolbyDigitalHandler::Scale()
{
    // for audio, the scale should be the sampling rate but
    // must not exceed 65535
	WAVEFORMATEX* pwfx = reinterpret_cast<WAVEFORMATEX*>(m_mt.Format());
    if (pwfx->nSamplesPerSec > 65535)
    {
        return 45000;
    }
    else
    {
        return pwfx->nSamplesPerSec;
    }
}

HRESULT DolbyDigitalHandler::WriteData(Atom* patm, const BYTE* pData, int cbData, int* pcActual)
{
	if (!m_bParsed)
	{
		if (!m_info.Parse(pData, cbData))
		{
			return E_INVALIDARG;
		}

		m_bParsed = TRUE;
	}

	return __super::WriteData(patm, pData, cbData, pcActual);
}

void DolbyDigitalHandler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
	UNREFERENCED_PARAMETER(id); // TODO: not used???

	// ETSI TS 102 366 - Annex F
	// AC-3 and Enhanced AC-3 Bitstream Storage in the ISO Base Media File Format

	// F.3 - AC3SampleEntry Box
	MP4BitstreamWriter bsw;
	bsw.ReserveBytes(6);						//  0- 5: reserved:8 [6]		= 0
	bsw.Write16(static_cast<short>(dataref));	//  6- 7: data-reference-index:16

	bsw.ReserveBytes(8);						//  8-15: reserved				= 0	   
	bsw.Write16(2);								// 16-17: channel count			= 2  (ignored)
	bsw.Write16(16);							// 18-19: bits per sample		= 16 (ignored)
    bsw.Write32(0);								// 20-23: reserved				= 0
    bsw.Write16(static_cast<short>(scale));		// 24-25: sample rate
	bsw.Write16(0);								// 26-27: reserved				= 0	   
	
    smart_ptr<Atom> psd = patm->CreateAtom('ac-3');
	bsw.AppendTo(psd);

	
	// F.4 - AC3SpecificBox
	bsw.Rewind();
	bsw.Write(m_info.fscod, 2);
	bsw.Write(m_info.bsid,  5);
	bsw.Write(m_info.bsmod, 3);
	bsw.Write(m_info.acmod, 3);
	bsw.Write(m_info.lfeon, 1);
	bsw.Write(m_info.frmsizcod / 2, 5);			// see Table F.1: bit_rate_code
	bsw.Reserve(5);

	smart_ptr<Atom> pdac3 = psd->CreateAtom('dac3');
	bsw.AppendTo(pdac3);

	pdac3->Close();
    psd->Close();
}
	

///////////////////////////////////////////////////////////////////////////////
// Dolby Dolby Digital Plus Audio support
// ETSI TS 102 366: Digital Audio Compression (AC-3, Enhanced AC-3) Standard
// http://www.etsi.org/deliver/etsi_ts/102300_102399/102366/01.02.01_60/ts_102366v010201p.pdf

EAC3StreamInfo::EAC3StreamInfo(void) :
	frameSize(0),
	bitrate(0),
	strmtyp(0),
	substreamid(0),
	fscod(0),
    bsid(0),
    bsmod(0),
    acmod(0),
    lfeon(0),
	chanmap(0)
{
}

bool EAC3StreamInfo::Parse(const BYTE* pData, int cbData)
{
	// Table E.3: Number of audio blocks per syncframe
	static const int aAudBlkPerSyncFrame[]	 = { 1, 2, 3, 6 };
	
	// Table E.2: Sample rate codes
	static const int aSamplingRates[]		 = { 48000, 44100, 32000, 0 };

	// Table E.4: Reduced sampling rates
	static const int aReducedSamplingRates[] = { 24000, 22050, 16000, 0 };


	// Retrieve some required information from the stream.
	DolbyBitstreamReader bsr(pData, cbData);

	// 4.3.1 - syncinfo - Synchronization information
	if (!bsr.IsSyncword())						// syncword:16 = 0x0B77
		return false;

	
	// E.1.2.2 - bsi - Bit stream information
	int frmsiz;
	int fscod2;
    int numblkscod;
	int samplerate;
	int numberOfBlocksPerSyncFrame;

	strmtyp		= bsr.Read(2);
    substreamid	= bsr.Read(3);
    frmsiz		= bsr.Read(11);					// Frame size one less of 16-bit words
    fscod		= bsr.Read(2);
    
	if (fscod == 0x3) 
	{
        fscod2	   = bsr.Read(2);
        numblkscod = 0x3;						// six blocks per frame
		samplerate = aReducedSamplingRates[fscod2];
    } 
	else 
	{
        numblkscod = bsr.Read(2);
		samplerate = aSamplingRates[fscod];
    }

    if (samplerate == 0)
        return false;

	// words_per_frame = frmsiz + 1
	numberOfBlocksPerSyncFrame = aAudBlkPerSyncFrame[numblkscod];
	frameSize = 2 * (frmsiz + 1);				// frame size in bytes = 2 * words_per_frame

    bitrate = samplerate * frameSize * 8 / (numberOfBlocksPerSyncFrame * 256);
	
	acmod = bsr.Read(3);
    lfeon = bsr.Read(1);
    bsid  = bsr.Read(5);
    bsr.Skip(5);								// dialnorm
 
    bsr.SkipIfBitSet(8);						// compre; if (compre) {compr}
 
    if (acmod == 0x0)							// if 1+1 mode (dual mono, so some items need a second value)
	{
        bsr.Skip(5);							// dialnorm2
		bsr.SkipIfBitSet(8);					// compr2e; if (compr2e) {compr2}
    }
 
    if (strmtyp == EAC3DependentSubtream)		// if dependent stream
	{
        if (bsr.Read(1))						// if (chanmape) 
		{
            chanmap = bsr.Read(16);				// {chanmap}
        }
    }
 
    if (bsr.Read(1))							// mixing metadata
	{     
        if (acmod > 0x2)						// if more than 2 channels
		{
            bsr.Skip(2);						// {dmixmod}
        }
 
        if ((acmod & 0x1) && (acmod > 0x2))		// if three front channels exist
		{
            bsr.Skip(6);						// ltrtcmixlev; lorocmixlev
        }
 
        if (acmod & 0x4)						// if a surround channel exists
		{
            bsr.Skip(6);						// ltrtsurmixlev; lorosurmixlev
        }
 
        if (lfeon)								// if the LFE channel exists
		{
            bsr.SkipIfBitSet(5);				// lfemixlevcode; if (lfemixlevcode) {lfemixlevcod}
        }
 
        if (strmtyp == EAC3IndependentStream)	// if independent stream
		{
            bsr.SkipIfBitSet(6);				// pgmscle; if (pgmscle) {pgmscl}
 
            if (acmod == 0x0)					// if 1+1 mode (dual mono, so some items need a second value)
			{
                bsr.SkipIfBitSet(6);			// pgmscl2e; if (pgmscl2e) {pgmscl2}
            }
 
            bsr.SkipIfBitSet(6);				// extpgmscle; if (extpgmscle) {extpgmscl}
 
            int mixdef = bsr.Read(2);
 
            if (mixdef == 0x1)					// mixing option 2
			{
                bsr.Skip(5);					// premixcmpsel:1, drcsrc:1, premixcmpscl:3
            } 
			else if (mixdef == 0x2)				// mixing option 3
			{
                bsr.Skip(12);					// {mixdata}
            } 
			else if (mixdef == 0x3)				// mixing option 4
			{
                int mixdeflen = bsr.Read(5);
 
                if (bsr.Read(1))				// mixdata2e
				{
                    bsr.Skip(5);				// premixcmpsel:1, drcsrc:1, premixcmpscl:3

					bsr.SkipIfBitSet(4);		// extpgmlscle;   if (extpgmlscle)   extpgmlscl
					bsr.SkipIfBitSet(4);		// extpgmcscle;   if (extpgmcscle)   extpgmcscl
					bsr.SkipIfBitSet(4);		// extpgmrscle;   if (extpgmrscle)   extpgmrscl
					bsr.SkipIfBitSet(4);		// extpgmlsscle;  if (extpgmlsscle)  extpgmlsscl
					bsr.SkipIfBitSet(4);		// extpgmrsscle;  if (extpgmrsscle)  extpgmrsscl
					bsr.SkipIfBitSet(4);		// extpgmlfescle; if (extpgmlfescle) extpgmlfescl
					bsr.SkipIfBitSet(4);		// dmixscle;	  if (dmixscle)      dmixscl
 
                    if (bsr.Read(1))			// addche; if (addche)
					{
						bsr.SkipIfBitSet(4);	// extpgmaux1scle; if (extpgmaux1scle) extpgmaux1scl
						bsr.SkipIfBitSet(4);	// extpgmaux2scle; if (extpgmaux2scle) extpgmaux2scl
                    }
                }
 
                if (bsr.Read(1))				// mixdata3e; if (mixdata3e)
				{
                    bsr.Skip(5);				// spchdat
 
                    if (bsr.Read(1))			// addspchdate
					{
                        bsr.Skip(7);			// spchdat1:5, spchan1att:2
						bsr.SkipIfBitSet(8);	// addspchdat1e; if (addspdat1e) { spchdat2:5, spchan2att:3 }
                    }
                }
 
				// mixdata = (8*(mixdeflen+2)) - no. mixdata bits
                for (int i = 0; i < (mixdeflen + 2); i++) 
				{
                    bsr.Skip(8);				// mixdatafill (0 - 7)
                }
            }
 
            if (acmod < 0x2)					// if mono or dual mono source
			{
				bsr.SkipIfBitSet(14);			// paninfoe; if (paninfoe) { panmean:8, paninfo:6 }
 
                if (acmod == 0x0)				// if 1+1 mode (dual mono, so some items need a second value)
				{
					bsr.SkipIfBitSet(14);		// paninfo2e; if (paninfo2e) { panmean2:8, paninfo2:6 }
                }
 
                if (bsr.Read(1))				// frmmixcfginfoe; if (frmmixcfginfoe)
				{
					// mixing configuration information
                    if (numblkscod == 0) 
					{
                        bsr.Skip(5);
                    } 
					else 
					{
                        for (int i = 0; i < numberOfBlocksPerSyncFrame; i++) 
						{
							bsr.SkipIfBitSet(5);	// blkmixcfginfoe; if (blkmixcfginfoe) {blkmixcfginfo[blk]}
                        }
                    }
                }
            }
        }
    }
 
    if (bsr.Read(1))							// infomdate - informational metadata
	{ 
        bsmod = bsr.Read(3);

		// Skip everything else, not used.
    }

	// Skip everything else, not used.

	return true;
}


long DolbyDigitalPlusHandler::Scale()
{
    // for audio, the scale should be the sampling rate but
    // must not exceed 65535
	WAVEFORMATEX* pwfx = reinterpret_cast<WAVEFORMATEX*>(m_mt.Format());
    if (pwfx->nSamplesPerSec > 65535)
    {
        return 45000;
    }
    else
    {
        return pwfx->nSamplesPerSec;
    }
}

bool DolbyDigitalPlusHandler::StreamInfoExists(const EAC3StreamInfo& info)
{
	for (EAC3StreamInfoArray::iterator it = m_streams.begin(); it != m_streams.end(); it++)
	{
		if (it->strmtyp != EAC3DependentSubtream && it->substreamid == info.substreamid)
		{
			return true;
		}
	}

	return false;
}

int DolbyDigitalPlusHandler::GetDependentSubstreams(int substreamid, int& chan_loc)
{
	int num_dep_sub = 0;
	
	chan_loc = 0;

	for (EAC3StreamInfoArray::iterator it = m_streams.begin(); it != m_streams.end(); it++)
	{
		if (it->strmtyp == EAC3DependentSubtream && it->substreamid == substreamid)
		{
			num_dep_sub++;

			// Convert Custom channel mapping (Table E.5) to Chan_loc field bit assignments (Table F.2)
            chan_loc |= ((it->chanmap >> 6) & 0x100) | ((it->chanmap >> 5) & 0xff);
		}
	}

	return num_dep_sub;
}

HRESULT DolbyDigitalPlusHandler::WriteData(Atom* patm, const BYTE* pData, int cbData, int* pcActual)
{
	if (m_streams.size() == 0)
	{
		const BYTE* pFrame = pData;
		int cbFrame = cbData;

		while (cbFrame > 0)
		{
			EAC3StreamInfo info;

			if (!info.Parse(pFrame, cbFrame))
				return E_INVALIDARG;

			if (StreamInfoExists(info))
				break;

			m_streams.push_back(info);

			//LOG((TEXT("DD+ cbData=%d, cbFrame=%d, frameSize=%d"), cbData, cbFrame, info.frameSize));

			pFrame += info.frameSize;
			cbFrame -= info.frameSize;
		}
	}

	//LOG((TEXT("Writing DD+ data")));

	return __super::WriteData(patm, pData, cbData, pcActual);
}

void DolbyDigitalPlusHandler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
	UNREFERENCED_PARAMETER(id); // TODO: not used???

	// ETSI TS 102 366 - Annex F
	// AC-3 and Enhanced AC-3 Bitstream Storage in the ISO Base Media File Format

	// F.5 - EAC3SampleEntry Box
	MP4BitstreamWriter bsw;
	bsw.ReserveBytes(6);							//  0- 5: reserved:8 [6]		= 0
	bsw.Write16(static_cast<short>(dataref));		//  6- 7: data-reference-index:16

	bsw.ReserveBytes(8);							//  8-15: reserved				= 0	   
	bsw.Write16(2);									// 16-17: channel count			= 2  (ignored)
	bsw.Write16(16);								// 18-19: bits per sample		= 16 (ignored)
    bsw.Write32(0);									// 20-23: reserved				= 0
    bsw.Write16(static_cast<short>(scale));			// 24-25: sample rate
	bsw.Write16(0);									// 26-27: reserved				= 0	   
	
	smart_ptr<Atom> psd = patm->CreateAtom('ec-3');
	bsw.AppendTo(psd);

	// F.6 - EC3SpecificBox
	int num_ind_sub = 0;
	int bitrate = 0;
	int cBytes = 0;
	int num_dep_sub;
	int chan_loc;

	bsw.Rewind();
	bsw.Write16(0);									// data_rate:13 and num_ind_sub:3 will be set later

	for (EAC3StreamInfoArray::iterator it = m_streams.begin(); it != m_streams.end(); it++)
	{
		if (it->strmtyp != EAC3DependentSubtream)	// Any other type is an independent stream
		{
			num_ind_sub++;

			bsw.Write(it->fscod, 2);
			bsw.Write(it->bsid,  5);
			bsw.Write(it->bsmod, 5);
			bsw.Write(it->acmod, 3);
			bsw.Write(it->lfeon, 1);
			bsw.Reserve(3);

			num_dep_sub = GetDependentSubstreams(it->substreamid, chan_loc);
		
			bsw.Write(num_dep_sub, 4);

			if (num_dep_sub > 0)
			{
				bsw.Write(chan_loc, 9);
			}
			else
			{
				bsw.Reserve(1);
			}
		}

		bitrate += it->bitrate;
	}

	cBytes = bsw.GetByteCount();

	bsw.Rewind();
	bsw.Write(bitrate / 1000, 13);	// data_rate:13 in Kb/s
	bsw.Write(num_ind_sub - 1, 3);	// num_ind_sub:3, one less than the number of independent substreams present

	smart_ptr<Atom> pdec3 = psd->CreateAtom('dec3');
	bsw.AppendTo(pdec3, cBytes);
	
	pdec3->Close();
    psd->Close();
}
	

///////////////////////////////////////////////////////////////////////////////


// ---- descriptor ------------------------

Descriptor::Descriptor(TagType type)
: m_type(type),
  m_cBytes(0),
  m_cValid(0)
{
}

void
Descriptor::Append(const BYTE* pBuffer, long cBytes)
{
    Reserve(cBytes);
    CopyMemory(m_pBuffer+m_cValid, pBuffer, cBytes);
    m_cValid += cBytes;
}

void
Descriptor::Reserve(long cBytes)
{
    if ((m_cValid + cBytes) > m_cBytes)
    {
        // increment memory in 128 byte chunks
        long inc = ((cBytes+127)/128) * 128;
        smart_array<BYTE> pNew = new BYTE[m_cBytes + inc];
        if (m_cValid > 0)
        {
            CopyMemory(pNew, m_pBuffer, m_cValid);
        }
        m_pBuffer = pNew;
        m_cBytes += inc;
    }
}

void
Descriptor::Append(Descriptor* pdesc)
{
    long cBytes = pdesc->Length();
    Reserve(cBytes);
    pdesc->Write(m_pBuffer + m_cValid);
    m_cValid += cBytes;
}

long 
Descriptor::Length()
{
    long cHdr = 2;
    long cBody = m_cValid;
    while (cBody > 0x7f)
    {
        cHdr++;
        cBody >>= 7;
    }
    return cHdr + m_cValid;

}

void 
Descriptor::Write(BYTE* pBuffer)
{
    int idx = 0;
    pBuffer[idx++] = (BYTE) m_type;
	if (m_cValid == 0)
	{
		pBuffer[idx++] = 0;
	}
	else
	{
		long cBody = m_cValid;
		while (cBody)
		{
			BYTE b = BYTE(cBody & 0x7f);
			if (cBody > 0x7f)
			{
				b |= 0x80;
			}
			pBuffer[idx++] = b;
			cBody >>= 7;
		}
	}
	CopyMemory(pBuffer + idx, m_pBuffer, m_cValid);
}

HRESULT 
Descriptor::Write(Atom* patm)
{
    long cBytes = Length();
    smart_array<BYTE> ptemp = new BYTE[cBytes];
    Write(ptemp);
    return patm->Append(ptemp, cBytes);
}

// --- H264 BSF support --------------
H264ByteStreamHandler::H264ByteStreamHandler(const CMediaType* pmt)
: H264Handler(pmt),
  m_bSPS(false),
  m_bPPS(false)
{
	if(*m_mt.FormatType() == FORMAT_MPEG2Video)
	{
		MPEG2VIDEOINFO* pvi = (MPEG2VIDEOINFO*)m_mt.Format();
		m_tFrame = pvi->hdr.AvgTimePerFrame;
		m_cx = pvi->hdr.bmiHeader.biWidth;
		m_cy = pvi->hdr.bmiHeader.biHeight;
	} else 
	if(*m_mt.FormatType() == FORMAT_VideoInfo)
	{
		VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
		m_tFrame = pvi->AvgTimePerFrame;
		m_cx = pvi->bmiHeader.biWidth;
		m_cy = pvi->bmiHeader.biHeight;
	} else 
	if(*m_mt.FormatType() == FORMAT_VideoInfo2)
	{
		VIDEOINFOHEADER2* pvi = (VIDEOINFOHEADER2*)m_mt.Format();
		m_tFrame = pvi->AvgTimePerFrame;
		m_cx = pvi->bmiHeader.biWidth;
		m_cy = pvi->bmiHeader.biHeight;
	} else 
	// TODO: or H264Handler
	if(*m_mt.FormatType() == FORMAT_UVCH264Video)
	{
		KS_H264VIDEOINFO* pvi = (KS_H264VIDEOINFO*)m_mt.Format();
		m_tFrame = pvi->dwFrameInterval;
		m_cx = pvi->wWidth;
		m_cy = pvi->wHeight;
	} else
		m_tFrame = m_cx = m_cy = 0;
}

void 
H264ByteStreamHandler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
    UNREFERENCED_PARAMETER(scale);
    UNREFERENCED_PARAMETER(id);
    smart_ptr<Atom> psd = patm->CreateAtom('avc1');

	// locate param sets in parse buffer
	NALUnit sps, pps;
	NALUnit nal;
	const BYTE* pBuffer = m_ParamSets.Data();
	long cBytes = m_ParamSets.Size();
	while (nal.Parse(pBuffer, cBytes, nalunit_length_field, true))
	{
		if (nal.Type() == NALUnit::NAL_Sequence_Params)
		{
			sps = nal;
		}
		else if (nal.Type() == NALUnit::NAL_Picture_Params)
		{
			pps = nal;
		}
		const BYTE* pNext = nal.Start() + nal.Length();
		cBytes-= long(pNext - pBuffer);
		pBuffer = pNext;
	}

	SeqParamSet seq;
	seq.Parse(&sps);

    BYTE b[78];
    ZeroMemory(b, 78);
    WriteShort(dataref, b+6);
    WriteShort(m_cx, b+24);
    WriteShort(m_cy, b+26);
    b[29] = 0x48;
    b[33] = 0x48;
    b[41] = 1;
    b[75] = 24;
    WriteShort(-1, b+76);
    psd->Append(b, 78);

    smart_ptr<Atom> pesd = psd->CreateAtom('avcC');
    b[0] = 1;           // version 1
    b[1] = (BYTE)seq.Profile();
    b[2] = seq.Compat();
    b[3] = (BYTE)seq.Level();
    // length of length-preceded nalus
    b[4] = BYTE(0xfC | (nalunit_length_field - 1));

    b[5] = 0xe1;        // 1 SPS

	// in the descriptor, the length field for param set nalus is always 2
	pesd->Append(b, 6);
	WriteVariable(sps.Length(), b, 2);
	pesd->Append(b, 2);
	pesd->Append(sps.Start(), sps.Length());

    b[0] = 1;   // 1 PPS
	WriteVariable(pps.Length(), b+1, 2);
	pesd->Append(b, 3);
	pesd->Append(pps.Start(), pps.Length());

    pesd->Close();
    psd->Close();
}

LONGLONG 
H264ByteStreamHandler::FrameDuration()
{
	return m_tFrame;
}

HRESULT 
H264ByteStreamHandler::WriteData(Atom* patm, const BYTE* pData, int cBytes, int* pcActual)
{
	int cActual = 0;

	NALUnit nal;
	while(nal.Parse(pData, cBytes, 0, true))
	{
		const BYTE* pNext = nal.Start() + nal.Length();
		cBytes-= long(pNext - pData);
		pData = pNext;

		// convert length to correct byte order
		BYTE length[nalunit_length_field];
		WriteVariable(nal.Length(), length, nalunit_length_field);

		if (!m_bSPS && (nal.Type() == NALUnit::NAL_Sequence_Params))
		{
			// store in length-preceded format for use in WriteDescriptor
			m_bSPS = true;
			m_ParamSets.Append(length, nalunit_length_field);
			m_ParamSets.Append(nal.Start(), nal.Length());
		}
		else if (!m_bPPS && (nal.Type() == NALUnit::NAL_Picture_Params))
		{
			// store in length-preceded format for use in WriteDescriptor
			m_bPPS = true;
			m_ParamSets.Append(length, nalunit_length_field);
			m_ParamSets.Append(nal.Start(), nal.Length());
		}

		// write length and data to file
		patm->Append(length, nalunit_length_field);
		patm->Append(nal.Start(), nal.Length());
		cActual += nalunit_length_field + nal.Length();
	}

	*pcActual = cActual;
	return S_OK;
}

void 
CC608Handler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
    UNREFERENCED_PARAMETER(scale);
    UNREFERENCED_PARAMETER(dataref);
    UNREFERENCED_PARAMETER(id);

    smart_ptr<Atom> psd = patm->CreateAtom('c608');
	psd->Close();
}
