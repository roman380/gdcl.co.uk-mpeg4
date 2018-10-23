// MovieWriter.h: interface for basic file structure classes.
//
// Geraint Davies, May 2004
//
// Copyright (c) GDCL 2004-6. All Rights Reserved. 
// You are free to re-use this as the basis for your own filter development,
// provided you retain this copyright notice in the source.
// http://www.gdcl.co.uk
//////////////////////////////////////////////////////////////////////

#pragma once

// tracks are normally interleaved by the second. Define this
// to set the maximum size of a single interleave piece.
// Android sets a max 500KB limit on the offset between video
// and audio, and this can be used to control that.
#define MAX_INTERLEAVE_SIZE	(400 * 1024)

#include "TypeHandler.h"

// byte ordering to buffer
inline void WriteShort(int x, BYTE* pBuffer)
{
    pBuffer[0] = (BYTE)((x >> 8) & 0xff);
    pBuffer[1] = (BYTE)(x & 0xff);
}

inline void WriteLong(long l, BYTE* pBuffer)
{
    pBuffer[0] = BYTE(l >> 24);
    pBuffer[1] = BYTE((l >> 16) & 0xff);
    pBuffer[2] = BYTE((l >> 8) & 0xff);
    pBuffer[3] = BYTE(l & 0xff);
}
inline void WriteI64(LONGLONG l, BYTE* pBuffer)
{
    WriteLong(long(l >> 32), pBuffer);
    WriteLong(long(l & 0xffffffff), pBuffer + 4);
}
inline long ReadLong(const BYTE* pByte)
{
    return (pByte[0] << 24) |
            (pByte[1] << 16) |
            (pByte[2] << 8)  |
            pByte[3];
}
inline LONGLONG ReadI64(BYTE* pBuffer)
{
	return (LONGLONG(ReadLong(pBuffer)) << 32) + ReadLong(pBuffer + 4);
}

// forward references
class Atom;
class AtomWriter;
class MovieWriter;
class TrackWriter;
// do you feel at this point there should be a class ScriptWriter?



// abstract interface to atom, supported by parent
// atom or by external container (eg output pin)
class AtomWriter
{
public:
    virtual ~AtomWriter() {}

    virtual LONGLONG Length() = 0;
    virtual LONGLONG Position() = 0;
    virtual HRESULT Replace(LONGLONG pos, const BYTE* pBuffer, long cBytes) = 0;
    virtual HRESULT Append(const BYTE* pBuffer, long cBytes) = 0;

	virtual VOID NotifyMediaSampleWrite(INT nTrackIndex, IMediaSample* pMediaSample, SIZE_T nDataSize)
	{ 
		nTrackIndex; pMediaSample; nDataSize;
	}
};

// basic container structure for MPEG-4 file format.
// Starts with length and FOURCC four byte type.
// Can contain other atoms and/or payload data
class Atom : public AtomWriter
{
public:
    Atom(AtomWriter* pContainer, LONGLONG llOffset, DWORD type);
    ~Atom()
    {
        if (!m_bClosed)
        {
            Close();
        }
    }
    LONGLONG Position()
    {
        return m_pContainer->Position() + m_llOffset;
    }
    HRESULT Replace(LONGLONG pos, const BYTE* pBuffer, long cBytes) 
    {
        return m_pContainer->Replace(m_llOffset + pos, pBuffer, cBytes);
    }
    HRESULT Append(const BYTE* pBuffer, long cBytes)
    {
        m_cBytes += cBytes;
        return m_pContainer->Append(pBuffer, cBytes);
    }
    LONGLONG Length()
    {
        return m_cBytes;
    }

    HRESULT Close();
    Atom* CreateAtom(DWORD type);
private:
    AtomWriter* m_pContainer;
    bool m_bClosed;
    LONGLONG m_llOffset;
    LONGLONG m_cBytes;
};


// a collection of samples, to be written as one contiguous 
// chunk in the mdat atom. The properties will
// be indexed once the data is written.
// The IMediaSample object is kept here until written and indexed.
class MediaChunk
{
public:
    MediaChunk(TrackWriter* pTrack);
    ~MediaChunk();

    HRESULT AddSample(IMediaSample* pSample);
    HRESULT Write(Atom* patm);
    long Length()
    {
        return m_cBytes;
    }
    void GetTime(REFERENCE_TIME* ptStart, REFERENCE_TIME* ptEnd)
    {
        *ptStart = m_tStart;
        *ptEnd = m_tEnd;
    }
    long Samples()
    {
        return (long)m_Samples.size();
    }
    bool IsFull(REFERENCE_TIME tMaxDur);
	REFERENCE_TIME GetDuration();
	void SetOldIndexFormat()	{ m_bOldIndexFormat = true; }

private:
    TrackWriter* m_pTrack;
    REFERENCE_TIME m_tStart;
    REFERENCE_TIME m_tEnd;
	bool m_bOldIndexFormat;
    long m_cBytes;
    list<IMediaSample*> m_Samples;
};
typedef smart_ptr<MediaChunk> MediaChunkPtr;



// --- indexing ---
/*
entiendo que aunque esta utilizando un 'smart array' en realidad el uso es super simple
* tiene un append, que cuando llena un buffer, crea uno nuevo y lo añade a la lista
* al cierre del archivo, los indices mandan llamar su Write(Atom) que al final desencadena en una escritura
* al pin de salida.  Write itera todos los bloques almacenados en memoria de manera secuencial, por lo que podemos
* leer los bloques almacenados en disco.
* La única lectura que se hace de los bloques es cuando tiene que cambiar de offsets de 32 bits a offsets de 64 bits
* Es un acceso secuencial, por lo que podemos mantener adicional al bloque activo (de escritura) un bloque de
* lectura
*/

//typedef smart_array<BYTE> BytePtr;
#define BASE_EXIT_CODE 7200

/*
// a growable list of 32-bit values maintained in
// file byte order for writing directly to one of the
// index atoms
class ListOfLongs
{
public:
    ListOfLongs();

    void Append(long l);
    HRESULT Write(Atom* patm);
    enum {
        EntriesPerBlock = 4096/4,
    };
    long Entries() {
        return (long)(((m_Blocks.size() - 1) * EntriesPerBlock) + m_nEntriesInLast);
    }
    long Entry(long nEntry);

private:
    vector<BytePtr> m_Blocks;
    long m_nEntriesInLast;
};
*/



// sgor: disk-backed list intended to reduce memory consumption of large recordings
template<class T> class ListOf {
public:
   
   enum {
      EntriesPerBlock = 4096 / sizeof(T),
   };

   ListOf() : _backingFile(NULL)
            , _chunkReadIdx(0)
   {
      _readPos.HighPart = DWORD_MAX;
      TCHAR pTempPath[MAX_PATH];
      TCHAR pTempFileName[MAX_PATH];
      DWORD ret = GetTempPath(MAX_PATH, pTempPath);
      if( ret <= 0 || ret > MAX_PATH )
         exit(BASE_EXIT_CODE);

      if( GetTempFileName(pTempPath, TEXT("mp4"), 0, pTempFileName) == 0 )
         exit(BASE_EXIT_CODE + 1);
      
      _backingFile = CreateFile(
         pTempFileName,
         GENERIC_READ | GENERIC_WRITE,
         0,
         NULL,
         CREATE_ALWAYS,
         FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
         NULL
      );
      if( _backingFile == INVALID_HANDLE_VALUE )
         exit(BASE_EXIT_CODE + 2);
   }
   ~ListOf() {
      if( _backingFile != NULL )
         CloseHandle(_backingFile);
   }
   void Append(T item) {
      // Calls for append are relatively sparse, we won't optimize this and will append every received T in the file
      // individually
      DWORD _;
      LARGE_INTEGER toMove = { 0 };
      SetFilePointerEx(_backingFile, toMove, NULL, FILE_END);
      BYTE buff[8];
      if constexpr( sizeof(T) == 4 ) {
         WriteLong((long)item, buff);
      } else if constexpr( sizeof(T) == 8 ) {
         WriteI64((LONGLONG)item, buff);
      } else {
         static_assert("unsupported type");
      }
      if( !WriteFile(_backingFile, buff, sizeof(T), &_, NULL) )
         exit(BASE_EXIT_CODE + 3); 
      _readPos.HighPart = DWORD_MAX; // invalidate read-position cursor
   }
   HRESULT Write(Atom* patm) {
      LARGE_INTEGER fileSz;
      LARGE_INTEGER toMove = { 0 };
      if( !SetFilePointerEx(_backingFile, toMove, &fileSz, FILE_END) ||
          !SetFilePointerEx(_backingFile, toMove, NULL, FILE_BEGIN) ) 
      {
         return E_FAIL;
      }
      
      UINT fullBlockCount  = (UINT)(fileSz.QuadPart / (EntriesPerBlock * sizeof(T)));
      UINT partialBlockLen = (UINT)(fileSz.QuadPart % (EntriesPerBlock * sizeof(T)));
      
      DWORD _;
      BYTE block[EntriesPerBlock * sizeof(T)];
      for( UINT i = 0; i < fullBlockCount; i++ ) {
         if( !ReadFile(_backingFile, block, sizeof(block), &_, NULL) )
            return E_FAIL;
         HRESULT hr = patm->Append(block, sizeof(block));
         if( FAILED(hr) ) {
            return hr;
         }
      }
      if( !ReadFile(_backingFile, block, partialBlockLen, &_, NULL) )
         return E_FAIL;
      return patm->Append(block, partialBlockLen);
   }
   long Entries() {
      LARGE_INTEGER fileSz;
      LARGE_INTEGER curPos;
      LARGE_INTEGER toMove = { 0 };
      
      if( !SetFilePointerEx(_backingFile, toMove, &curPos, FILE_CURRENT) ||   // remember current position
          !SetFilePointerEx(_backingFile, toMove, &fileSz, FILE_END    ) ||   // move to end-of-file and extract pos
          !SetFilePointerEx(_backingFile, curPos, NULL,    FILE_BEGIN  )      // restore current position
      ) {
         return E_FAIL;
      }
      return (long)(fileSz.QuadPart / sizeof(T));
   }
   T Entry(long nEntry) {
      // Optimized for linear access (inspection of client-code shows this is the access pattern, but works with random 
      // access too (albeit very slowly), in case I missed random-access logic somewhere else in the project
      LARGE_INTEGER pos;
      pos.QuadPart = nEntry * sizeof(T);
      // Perform seek only if our read cursor position mismatches the real file cursor position
      if( pos.QuadPart != _readPos.QuadPart ) {
         if( !SetFilePointerEx(_backingFile, pos, NULL, FILE_BEGIN) )
            exit(BASE_EXIT_CODE + 4);
         _chunkReadIdx = EntriesPerBlock;  // Force reading new chunk
      }
      if( _chunkReadIdx >= EntriesPerBlock ) {
         // We have processed the whole chunk (or the read cursor has been repositioned), read next chunk
         if( !ReadFile(_backingFile, (void*)_chunk, EntriesPerBlock * sizeof(T), &_chunkActualEntries, NULL) )
            exit(BASE_EXIT_CODE + 5);
         _chunkActualEntries /= sizeof(T);
         _chunkReadIdx = 0;
      }
      if( _chunkReadIdx >= _chunkActualEntries )  // detect read past end-of-file
         exit(BASE_EXIT_CODE + 6);
      T entry;
      if constexpr( sizeof(T) == 4 ) {
         entry = (T)ReadLong((BYTE*)&_chunk[_chunkReadIdx++]);
      } else if constexpr( sizeof(T) == 8) {
         entry = (T)ReadI64((BYTE*)&_chunk[_chunkReadIdx++]);
      } else {
         static_assert("unsupported type");
      }
      _readPos.QuadPart += sizeof(T);
      return entry;
   }

private:
   HANDLE _backingFile;
   LARGE_INTEGER _readPos;
   DWORD _chunkReadIdx;
   DWORD _chunkActualEntries;
   T _chunk[EntriesPerBlock];
};

using ListOfLongs = ListOf<long>;
using ListOfI64 = ListOf<LONGLONG>;

/*
// growable list of 64-bit values
class ListOfI64
{
public:
    ListOfI64();

    void Append(LONGLONG ll);
    HRESULT Write(Atom* patm);
    enum {
        EntriesPerBlock = 4096/8,
    };
    long Entries() {
        return (long) (((m_Blocks.size() - 1) * EntriesPerBlock) + m_nEntriesInLast);
    }
private:
    vector<BytePtr> m_Blocks;
    long m_nEntriesInLast;
};
*/

// pairs of <count, value> longs -- this is essentially an RLE compression
// scheme for some index tables; instead of a list of values, consecutive
// identical values are grouped, so you get a list of <count, value> pairs.
// Used for CTTS and STTS
class ListOfPairs
{
public:
	ListOfPairs();
	void Append(long l);
    HRESULT Write(Atom* patm);
	long Entries() { return m_cEntries; }
private:
	ListOfLongs m_Table;

	// total entries
	long m_cEntries;

	// current pair not in table
	long m_lValue;
	long m_lCount;
};

// sample size index -- table of <count, size> pairs
// possibly reduced to a single header
class SizeIndex
{
public:
    SizeIndex();

    void Add(long cBytes);
	void AddMultiple(long cBytes, long count);
	HRESULT Write(Atom* patm);
private:
    ListOfLongs m_Table;

    // current pair not in table
    long m_cBytesCurrent;
    long m_nCurrent;

    // total samples
    long m_nSamples;
};

// sample duration table -- table of <count, duration> pairs
// Duration of each sample is calculated from
// start of next sample, except final entry (which is from stop time).
// Also records offset to first sample for edts atom
//
// Added CTTS table support for re-ordered frames.
// This table contains offsets from decode time to composition time for
// out-of-order frames. Since we do not receive decode time in DirectShow, 
// this is calculated from the difference between sample start and stop times,
// if the sample duration seems reasonably constant.
class DurationIndex
{
public:
    DurationIndex(long scale);

    void Add(REFERENCE_TIME tStart, REFERENCE_TIME tEnd);
	void AddOldFormat(int count);
	void SetOldIndexStart(REFERENCE_TIME tStart);
    HRESULT WriteEDTS(Atom* patm, long scale);
    HRESULT WriteTable(Atom* patm);
    REFERENCE_TIME Duration()
    {
        return m_tStopLast;
    }
    long Scale()
    {
        return m_scale;
    }
    HRESULT SetScale(long scale)
    {
        if (m_tStartFirst != -1)
        {
            return E_FAIL;
        }
        m_scale = scale;
        return S_OK;
    }
	
	void SetFrameDuration(LONGLONG tFrame)
	{
		m_tFrame = tFrame;
	}

    // for track start adjustment
    REFERENCE_TIME Earliest()
    {
        return m_tStartFirst;
    }
    void OffsetTimes(LONGLONG tAdjust)
    {
        m_tStartFirst += tAdjust;
        m_tStartLast += tAdjust;
        m_tStopLast += tAdjust;
		m_TotalDuration += ToScale(tAdjust);
		m_refDuration += tAdjust;
    }
	int SampleCount()						{ return m_nSamples; }
	REFERENCE_TIME AverageDuration()		{ return m_refDuration / m_nSamples; }

private:
    void AddDuration(long cThis);
    LONGLONG ToScale(REFERENCE_TIME t)
    {
        return t * m_scale / UNITS;
    }
	REFERENCE_TIME ToReftime(LONGLONG scale)
	{
		return scale * UNITS / m_scale;
	}
	void ModeDecide();
	void AppendCTTSMode(REFERENCE_TIME tStart, REFERENCE_TIME tEnd);


private:
    long m_scale;
    ListOfPairs m_STTS;
    REFERENCE_TIME m_tStartFirst;
    REFERENCE_TIME m_tStartLast;
    REFERENCE_TIME m_tStopLast;

    // check for rounding errors
    LONGLONG m_TotalDuration;
	REFERENCE_TIME m_refDuration;

	// total samples recorded
	int m_nSamples;

	// for CTTS calculation
	ListOfPairs m_CTTS;
	bool m_bCTTS;

	// look at the first few samples to decide whether to use
	// start-time-only mode (duration is just this start - last start)
	// or CTTS mode.
	enum { mode_decide_count = 10, };
	REFERENCE_TIME m_SampleStarts[mode_decide_count];
	REFERENCE_TIME m_SampleStops[mode_decide_count];
	REFERENCE_TIME m_SumDurations;
	REFERENCE_TIME m_tFrame;
	bool m_bUseFrameRate;
};

// index of samples per chunk.
// table of triplets <first chunk number, samples per chunk, data reference>
class SamplesPerChunkIndex
{
public:
    SamplesPerChunkIndex(long dataref);

    void Add(long nSamples);
    HRESULT Write(Atom* patm);
private:
    long m_dataref;
    ListOfLongs m_Table;
    long m_nTotalChunks;
    long m_nSamples;    //last entry
};

// index of chunk offsets
// 
// We need to use 64-bit offsets for the whole
// table if any are > 32bit. However it would be 
// wasteful to always use 64-bit offsets.
// We use 32-bit offsets until we see a 64-bit offset.
// The 32-bit offset table will be converted on Write
// if needed.

class ChunkOffsetIndex
{
public:
    void Add(LONGLONG posChunk);
    HRESULT Write(Atom* patm);
private:
    ListOfLongs m_Table32;
    ListOfI64 m_Table64;
};

// map of key (sync-point) samples
class SyncIndex
{
public:
    SyncIndex();

    void Add(bool bSync);
    HRESULT Write(Atom* patm);
private:
    long m_nSamples;
    bool m_bAllSync;
    ListOfLongs m_Syncs;
};

// one media track within a file.
class TrackWriter
{
public:
    TrackWriter(MovieWriter* pMovie, int index, TypeHandler* ptype, BOOL bNotifyMediaSampleWrite = FALSE);

    HRESULT Add(IMediaSample* pSample);

    // returns true if all tracks now at end
    bool OnEOS();

    bool IsAtEOS()
    {
        CAutoLock lock(&m_csQueue);
        return m_bEOS;
    }

    // no more writes accepted -- partial/queued writes abandoned (optionally)
    void Stop(bool bFlush);

    bool GetHeadTime(LONGLONG* ptHead);
    HRESULT WriteHead(Atom* patm);
    REFERENCE_TIME LastWrite();

    void IndexChunk(LONGLONG posChunk, long nSamples);
    void IndexSample(bool bSync, REFERENCE_TIME tStart, REFERENCE_TIME tStop, long cBytes);
	void OldIndex(LONGLONG posChunk, long cBytes);
	void SetOldIndexStart(REFERENCE_TIME tStart)
	{
		m_Durations.SetOldIndexStart(tStart);
	}
    HRESULT Close(Atom* patm);

    REFERENCE_TIME SampleDuration()
    {
		if (m_Durations.SampleCount() > 3)
		{
			REFERENCE_TIME tDur = m_Durations.AverageDuration();
			if (tDur > 0)
			{
				return m_Durations.AverageDuration();
			}
		}
        return UNITS / m_pType->SampleRate();
    }
    REFERENCE_TIME Duration()
    {
		REFERENCE_TIME tDur = m_Durations.Duration();
		
        return tDur;
    }
    bool IsVideo()
    {
        return m_pType->IsVideo();
    }
    bool IsAudio()
    {
        return m_pType->IsAudio();
    }
    TypeHandler* Handler()
    {
        return m_pType;
    }
    long ID() 
    {
        return m_index+1;
    }
    REFERENCE_TIME Earliest()
    {
		if (m_StartAt != 0)
		{
			return m_StartAt;
		}
        return m_Durations.Earliest();
    }
    void AdjustStart(REFERENCE_TIME tAdj)
    {
		m_Durations.OffsetTimes(tAdj);
    }
	void SetStartAt(REFERENCE_TIME tStart)
	{
		if ((m_StartAt == 0) || (tStart < m_StartAt))
		{
			m_StartAt = tStart;
		}
	}
	bool IsNonMP4()
	{
		if (m_pType)
		{
			return m_pType->IsNonMP4();
		}
		return false;
	}

	VOID NotifyMediaSampleWrite(IMediaSample* pMediaSample, SIZE_T nDataSize);

private:
    MovieWriter* m_pMovie;
    int m_index;
    smart_ptr<TypeHandler> m_pType;
	BOOL m_bNotifyMediaSampleWrite;

    CCritSec m_csQueue;
    bool m_bEOS;
    bool m_bStopped;
    REFERENCE_TIME m_tLast;
    MediaChunkPtr m_pCurrent;
    list<MediaChunkPtr> m_Queue;

    SizeIndex m_Sizes;
    DurationIndex m_Durations;
    SamplesPerChunkIndex m_SC;
    ChunkOffsetIndex m_CO;
    SyncIndex m_Syncs;

	// IAMStreamControl start offset
	// -- set to first StartAt time, if explicit,
	// which is used instead of Earliest to zero-base the
	// timestamps
	REFERENCE_TIME m_StartAt;
};
typedef smart_ptr<TrackWriter> TrackWriterPtr;


class MovieWriter
{
public:
    MovieWriter(AtomWriter* pContainer);

	VOID Initialize(BOOL bAlignTrackStartTimeDisabled, REFERENCE_TIME nMinimalMovieDuration)
	{
		m_bAlignTrackStartTimeDisabled = bAlignTrackStartTimeDisabled;
		m_nMinimalMovieDuration = nMinimalMovieDuration;
	}

    TrackWriter* MakeTrack(const CMediaType* pmt, BOOL bNotifyMediaSampleWrite = FALSE);
    HRESULT Close(REFERENCE_TIME* pDuration);

    // ensures that CheckQueues is not active when
    // tracks are in their Stop method discarding queues
    void Stop();

    // mux output from pin queues -- returns true if all tracks at EOS
    bool CheckQueues();

	// empty queues  - similar to CheckQueues, but called when 
	// all pins are stopped, to flush queued data to the file
	void WriteOnStop();

    // use 90khz for movie and track
    // -- this avoids the problem with audio headers where
    // the timescale must fit in 16 bits
    long MovieScale() 
    {
        return DEFAULT_TIMESCALE;
    }

    long TrackCount()
    {
        return (long)m_Tracks.size();
    }
    TrackWriter* Track(long nTrack)
    {
        return m_Tracks[nTrack];
    }
    REFERENCE_TIME CurrentPosition();

	REFERENCE_TIME MaxInterleaveDuration()
	{
		CAutoLock lock(&m_csBitrate);
		return m_tInterleave;
	}
	void RecordBitrate(size_t index, long bitrate);

	VOID NotifyMediaSampleWrite(INT nTrackIndex, IMediaSample* pMediaSample, SIZE_T nDataSize);

private:
    void MakeIODS(Atom* pmoov);
    void InsertFTYP(AtomWriter* pFile);
	void WriteTrack(int indexReady);

private:
    AtomWriter* m_pContainer;

	BOOL m_bAlignTrackStartTimeDisabled;
	REFERENCE_TIME m_nMinimalMovieDuration;

    CCritSec m_csWrite;
    bool m_bStopped;
    bool m_bFTYPInserted;
    smart_ptr<Atom> m_patmMDAT;
    vector<TrackWriterPtr> m_Tracks;

	CCritSec m_csBitrate;
	vector<int> m_Bitrates;
	REFERENCE_TIME m_tInterleave;
};

