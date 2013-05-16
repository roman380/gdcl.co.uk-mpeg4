//
// ElemType.h: declarations of elementary stream type classes.
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

#pragma once

// descriptors have a tag byte, then a length
// encoded with a "next byte" bit and 7 bit length
//
// This class refers to a buffer held elsewhere
class Descriptor
{
public:
    Descriptor()
    : m_pBuffer(NULL),
      m_cBytes(0),
      m_cHdr(0),
      m_type(InvalidTag)
    {
    }

    bool Parse(const BYTE* pBuffer, long cBytes);
    
    enum eType
    {
        InvalidTag = 0,
        ES_DescrTag = 3,
        DecoderConfigDescrTag = 4,
        DecSpecificInfoTag = 5,
    };
    eType Type() {
        return m_type;
    }
    const BYTE* Start() {
        return m_pBuffer + Header();
    }
    long Header() {
        return m_cHdr;
    }
    long Length() {
        return m_cBytes;
    }
    bool DescriptorAt(long cOffset, Descriptor& desc);

private:
    eType m_type;
    long m_cHdr;
    long m_cBytes;
    const BYTE* m_pBuffer;
};

// these are the types we currently understand
enum eESType {
	First_Video = 0,
    Video_Mpeg4,
    Video_H264,
	Video_H263,
	Video_FOURCC,
	Video_Mpeg2,
	First_Audio,
    Audio_AAC = First_Audio,
    Audio_WAVEFORMATEX,
	Audio_Mpeg2,
	First_Data,
	Text_CC608,
};



// abstract interface for format handler, responsible
// for creating a specific output stream 
class FormatHandler
{
public:
    virtual ~FormatHandler() {}

    virtual long BufferSize(long MaxSize) = 0;
    virtual void StartStream() = 0;
    virtual long PrepareOutput(IMediaSample* pSample, Movie* pMovie, LONGLONG llPos, long cBytes) = 0;
};



// conversion from elementary stream descriptors to
// DirectShow media type
//
// We can offer multiple type definitions for a given stream,
// and support a format handler object specific to the type chosen
// to create the output stream.
class ElementaryType  
{
public:
    ElementaryType();
    ~ElementaryType();

    bool Parse(REFERENCE_TIME tFrame, Atom* patm); // atom should be stsd descriptor mp4v, jvt1, mp4a
	bool IsVideo();
	string ShortName()
	{
		return m_shortname;
	}
    bool GetType(CMediaType* pmt, int nType);
    bool SetType(const CMediaType* pmt);
    FormatHandler* Handler() 
    {
        return m_pHandler;
    }

	eESType StreamType()
	{
		return m_type;
	}
private:
    bool GetType_H264(CMediaType* pmt);
	bool GetType_H264ByteStream(CMediaType* pmt);
    bool GetType_Mpeg4V(CMediaType* pmt, int n);
    bool GetType_AAC(CMediaType* pmt);
    bool GetType_WAVEFORMATEX(CMediaType* pmt);
    bool ParseDescriptor(Atom* patmESD);
	bool GetType_FOURCC(CMediaType* pmt);
	bool GetType_JPEG(CMediaType* pmt);

private:
    eESType m_type;
    smart_array<BYTE> m_pDecoderSpecific;
    long m_cDecoderSpecific;

    long m_cx;
    long m_cy;
    static const int SamplingFrequencies[];
    REFERENCE_TIME m_tFrame;

	// fourcc and bitdepth -- for uncompressed or RLE format
	DWORD m_fourcc;
	int m_depth;

    CMediaType m_mtChosen;
    FormatHandler* m_pHandler;
	string m_shortname;
};

// --- directshow type info

// de-facto standard for H.264 elementary stream : FOURCC('AVC1')
class DECLSPEC_UUID("31435641-0000-0010-8000-00AA00389B71") MEDIASUBTYPE_H264_MP4_Stream;

// Broadcom/Cyberlink Byte-Stream H264 subtype
// CLSID_H264
class DECLSPEC_UUID("8D2D71CB-243F-45E3-B2D8-5FD7967EC09B") CLSID_H264;



