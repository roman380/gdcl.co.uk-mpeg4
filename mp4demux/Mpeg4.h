//
// Mpeg4.cpp: definition of Mpeg-4 parsing classes
//
//
// Geraint Davies, April 2004
//
// Copyright (c) GDCL 2004-6. All Rights Reserved. 
// You are free to re-use this as the basis for your own filter development,
// provided you retain this copyright notice in the source.
// http://www.gdcl.co.uk
//////////////////////////////////////////////////////////////////////

#pragma once

inline UINT16 Swap2Bytes(UINT16 Value)
{
	return _byteswap_ushort(Value);
}
inline UINT32 Swap4Bytes(UINT32 Value)
{
	return _byteswap_ulong(Value);
}
inline UINT32 SwapLong(const BYTE* Value)
{
	return _byteswap_ulong(*((const UINT32*) Value));
}
inline UINT64 SwapI64(const BYTE* Value)
{
	return _byteswap_uint64(*((const UINT64*) Value));
}

class Atom;
typedef smart_ptr<Atom> AtomPtr;

// abstract interface that is implemented by the supplier of data,
// e.g. by the filter's input pin
class AtomReader
{
public:
    virtual ~AtomReader() {}

    virtual HRESULT Read(LONGLONG llOffset, long cBytes, BYTE* pBuffer) = 0;
    virtual LONGLONG Length() = 0;

    // support for caching in memory. 
    virtual bool IsBuffered() = 0;

    // calls to Buffer and BufferRelease are refcounted and should correspond.
    virtual const BYTE* Buffer() = 0;
    virtual void BufferRelease() = 0;
};


// MPEG-4 files (based on QuickTime) are comprised of file elements
// with a length and 4-byte FOURCC type code. This basic header can
// be extended with an 8-byte length and a 16-byte type GUID. 
// 
// The payload can contain other atoms recursively.
//
// Some atoms begin with a 1-byte version number and 3-byte flags field,
// but as this is type-specific, we regard it as part of the payload.
//
// This Atom implementation accesses data
// via the AtomReader abstraction. The data source may be
// a containing atom or access to the file (perhaps via an input pin).
class Atom : public AtomReader
{
public:
    // all the header params are parsed in the "Child" method and passed
    // to the constructor. This means we can use the same class for the outer file
    // container (which does not have a header).
    Atom(AtomReader* pReader, LONGLONG llOffset, LONGLONG llLength, DWORD type, long cHeader);
    virtual ~Atom() {}

    virtual HRESULT Read(LONGLONG llOffset, long cBytes, BYTE* pBuffer);
    virtual LONGLONG Length()
    {
        return m_llLength;
    }
    virtual DWORD Type()
    {
        return m_type;
    }

    // size of size and type fields (including extended size and guid type if present)
    virtual long HeaderSize()
    {
        return m_cHeader;
    }

    virtual long ChildCount();

    // these methods return a pointer to an Atom object that is contained within
    // this Atom -- so do not delete.
    virtual Atom* Child(long nChild);
    virtual Atom* FindChild(DWORD fourcc);

    virtual bool IsBuffered();
    // calls to Buffer and BufferRelease are refcounted and should correspond.
    virtual const BYTE* Buffer();
    virtual void BufferRelease();

    // call this if the child items do not start immediately after the header
    // -- llOffset is an offset from HeaderSize
    virtual void ScanChildrenAt(LONGLONG llOffset);

private:
    AtomReader* m_pSource;
    LONGLONG m_llOffset;
    LONGLONG m_llLength;
    long m_cHeader;
    DWORD m_type;

    // caching
    smart_array<BYTE> m_Buffer;
    long m_cBufferRefCount;

    // list of children
    list<AtomPtr> m_Children;
};

// this class is a simple way to manage the Buffer/BufferRelease
// caching scheme for an Atom
class AtomCache
{
public:
    AtomCache(Atom* patm = NULL)
    : m_pAtom(patm)
    {
		if (patm != NULL)
		{
			m_pBuffer = patm->Buffer() + patm->HeaderSize();
		}
    }
    ~AtomCache()
    {
		if (m_pAtom != NULL)
		{
			m_pAtom->BufferRelease();
		}
    }
    const AtomCache& operator=(Atom* patm)
    {
        if (m_pAtom != NULL)
        {
            m_pAtom->BufferRelease();
        }
        m_pAtom = patm;
        if (m_pAtom != NULL)
        {
            m_pBuffer = patm->Buffer() + patm->HeaderSize();
        }
        return *this;
    }


    const BYTE* operator->() const 
    {
        return m_pBuffer;
    }
    BYTE operator[](int idx) const
    {
        return m_pBuffer[idx];
    }
    operator const BYTE*() const
    {
        return m_pBuffer;
    }
	
private:
    Atom* m_pAtom;
    const BYTE* m_pBuffer;
};

// --- movie and track headers ---------------------------

class Movie;
class ElementaryType;
class FormatHandler;
class SampleSizes;
class KeyMap;
class SampleTimes;

struct EditEntry
{
	LONGLONG duration;
	LONGLONG offset;
	LONGLONG sumDurations;
};

class MovieTrack
{
public:
    MovieTrack(Atom* pAtom, Movie* pMovie, long idx);
    bool Valid()
    {
        return (m_pRoot != NULL);
    }
    const char* Name()
    {
        return m_strName.c_str();
    }
    bool IsVideo();
    bool GetType(CMediaType* pmt, int nType);
    bool SetType(const CMediaType* pmt);
    FormatHandler* Handler();

    SampleSizes* SizeIndex()
    {
        return m_pSizes;
    }
    KeyMap* GetKeyMap()
    {
        return m_pKeyMap;
    }
    SampleTimes* TimesIndex()
    {
        return m_pTimes;
    }
    Movie* GetMovie()
    {
        return m_pMovie;
    }
    HRESULT ReadSample(long nSample, BYTE* pBuffer, long cBytes);
	bool IsOldAudioFormat()		{ return m_bOldFixedAudio; }
	bool CheckInSegment(REFERENCE_TIME tNext, bool bSyncBefore, size_t* pnSegment, long* pnSample);
	void GetTimeBySegment(long nSample, size_t segment, REFERENCE_TIME* ptStart, REFERENCE_TIME* pDuration);
	bool NextBySegment(long* pnSample, size_t* psegment);
	SIZE_T GetTimes(REFERENCE_TIME** ppnStartTimes, REFERENCE_TIME** ppnStopTimes, ULONG** ppnFlags, ULONG** ppnDataSizes);

private:
    bool ParseMDIA(Atom* patm, REFERENCE_TIME tFirst);
    bool ParseSTSD(REFERENCE_TIME tFrame, Atom* pSTSD);
    LONGLONG ParseEDTS(Atom* patm);

private:
    Atom* m_pRoot = nullptr;
    Movie* m_pMovie = nullptr;
    string m_strName;
    long m_idx;

    long m_scale;
    Atom* m_patmSTBL = nullptr;
    smart_ptr<ElementaryType> m_pType;
    smart_ptr<SampleSizes> m_pSizes;
    smart_ptr<KeyMap> m_pKeyMap;
    smart_ptr<SampleTimes> m_pTimes;
	bool m_bOldFixedAudio;

	REFERENCE_TIME m_tNext;
	vector<EditEntry> m_Edits;
};
typedef smart_ptr<MovieTrack> MovieTrackPtr;

class Movie
{
public:
    Movie(Atom* pRoot);

    long Tracks()
    {
        return (long)m_Tracks.size();
    }
    MovieTrack* Track(long nTrack)
    {
        return m_Tracks[nTrack];
    }
	SIZE_T InvalidTrackCount() const
	{
		return m_invalidTrackCount;
	}

    REFERENCE_TIME Duration()
    {
        return m_tDuration;
    }
    LONGLONG Scale()
    {
        return m_scale;
    }
    HRESULT ReadAbsolute(LONGLONG llPos, BYTE* pBuffer, long cBytes);

    std::vector<std::pair<std::string, std::wstring>> const& AttributeVector() const
    {
        return m_AttributeVector;
    }

private:
    smart_ptr<Atom> m_pRoot;
    vector<MovieTrackPtr> m_Tracks;
	size_t m_invalidTrackCount = 0;
    long m_scale;
    LONGLONG m_duration;
    REFERENCE_TIME m_tDuration;
    std::vector<std::pair<std::string, std::wstring>> m_AttributeVector;
};
