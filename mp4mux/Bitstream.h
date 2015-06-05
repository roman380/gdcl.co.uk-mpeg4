#pragma once

class Bitstream
{
public:
	Bitstream(UINT nMaxBits, bool bBigEndian);

	inline void SetBigEndian(bool bBigEndian)	{ m_bBigEndian = bBigEndian;		}
	inline void Rewind(void)					{ m_nCurBit = 0;					}
	inline void SkipBytes(UINT cBytes)			{ Skip(cBytes << 3);				}
	inline void Skip(UINT cBits)				{ SetPosition(m_nCurBit + cBits);	}
	inline bool IsByteAligned(void) const		{ return ((m_nCurBit & 7) == 0);	}
	inline UINT GetPosition(void) const			{ return m_nCurBit;					}
	inline UINT GetByteCount() const			{ return (m_nCurBit + 7) >> 3;		}
	inline UINT GetBitLeft() const				{ return m_cMaxBits - m_nCurBit;	}

	void SetPosition(UINT nPos);


protected:
	bool m_bBigEndian;
	UINT m_cMaxBits;
	UINT m_nCurBit;
};

class BitstreamWriter : public Bitstream
{
public:
	BitstreamWriter(BYTE* pBuffer, UINT cBytes, bool bBigEndian = true);

	inline void Reserve(UINT cBits)	{ Write(0, cBits); }

	void Clear(void);
	void ReserveBytes(UINT cBytes);
	void WriteBytes(const BYTE* pBytes, UINT cBytes);
	void Write(int nValue, UINT cBits);
	void Write8(BYTE bValue);
	void Write16(short nValue);
	void Write24(long lValue);
	void Write32(long lValue);
	void Write64(LONGLONG llValue);


protected:
	BYTE* m_pBits;
};

class BitstreamReader : public Bitstream
{
public:
	BitstreamReader(const BYTE* pBuffer, UINT cBytes, bool bBigEndian = true);

	inline void SkipIfBitSet(UINT cBits)	{ if (Read(1)) Skip(cBits);	}

	int Read(UINT cBits);
	char Read8(void);
	short Read16(void);
	long Read24(void);
	long Read32(void);
	LONGLONG Read64(void);


protected:
	const BYTE* m_pBits;
};

