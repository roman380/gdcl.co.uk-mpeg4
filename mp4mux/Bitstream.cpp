// Bitstream.cpp

#include "StdAfx.h"
#include "Bitstream.h"

///////////////////////////////////////////////////////////////////////////////
// Bitstream Helper implementation.

Bitstream::Bitstream(UINT nMaxBits, bool bBigEndian) :
	m_bBigEndian(bBigEndian),
	m_cMaxBits(nMaxBits),
	m_nCurBit(0)
{
}

void Bitstream::SetPosition(UINT nPos) 
{
	ASSERT(nPos < m_cMaxBits);
	m_nCurBit = nPos;
}


///////////////////////////////////////////////////////////////////////////////
// BitstreamWriter.

BitstreamWriter::BitstreamWriter(BYTE* pBuffer, UINT cBytes, bool bBigEndian) :
	Bitstream(cBytes << 3, bBigEndian),
	m_pBits(pBuffer)
{
}

void BitstreamWriter::Clear(void)
{
	ASSERT(m_pBits);
	ZeroMemory(m_pBits, m_cMaxBits >> 3);
}

void BitstreamWriter::ReserveBytes(UINT cBytes)
{ 
	ASSERT(IsByteAligned());
	ASSERT(m_pBits);

	UINT cBits = cBytes << 3;

	ASSERT(GetBitLeft() >= cBits);
	
	if (GetBitLeft() >= cBits)
	{
		ZeroMemory(&m_pBits[m_nCurBit >> 3], cBytes);
		m_nCurBit += cBits;
	}
}

void BitstreamWriter::WriteBytes(const BYTE* pBytes, UINT cBytes)
{
	ASSERT(IsByteAligned());
	ASSERT(m_pBits);

	UINT cBits = cBytes << 3;

	ASSERT(GetBitLeft() >= cBits);

	if (GetBitLeft() >= cBits)
	{
		CopyMemory(&m_pBits[m_nCurBit >> 3], pBytes, cBytes);
		m_nCurBit += cBits;
	}
}

void BitstreamWriter::Write(int nValue, UINT cBits)
{
	ASSERT(m_pBits);
	ASSERT(cBits > 0 && cBits <= 32);
	ASSERT(GetBitLeft() >= cBits);

	if (GetBitLeft() >= cBits)
	{
		BYTE bBit;

		for (int i = cBits - 1; i >= 0; i--)
		{
			bBit = 1 << (7 - (m_nCurBit & 7));
		
			if (nValue & (1 << i))
			{
				m_pBits[m_nCurBit >> 3] |= bBit;
			}
			else
			{
				m_pBits[m_nCurBit >> 3] &= ~bBit;
			}

			m_nCurBit++;
		}
	}
}

void BitstreamWriter::Write8(BYTE bValue)
{
	ASSERT(IsByteAligned());
	ASSERT(GetBitLeft() >= 8);
		
	if (GetBitLeft() >= 8)
	{
		m_pBits[m_nCurBit >> 3] = bValue;
		m_nCurBit += 8;
	}
}

void BitstreamWriter::Write16(short nValue)
{
	ASSERT(IsByteAligned());
	ASSERT(GetBitLeft() >= 16);

	if (GetBitLeft() >= 16)
	{
		BYTE* pb = &m_pBits[m_nCurBit >> 3];

		m_nCurBit += 16;

		if (m_bBigEndian)
		{
			pb[0] = static_cast<BYTE>(nValue >> 8);
			pb[1] = static_cast<BYTE>(nValue & 0xFF);
		}
		else
		{
			*(reinterpret_cast<short*>(pb)) = nValue;
		}
	}
}

void BitstreamWriter::Write24(long lValue)
{
	ASSERT(IsByteAligned());
	ASSERT(GetBitLeft() >= 24);

	if (GetBitLeft() >= 24)
	{
		BYTE* pb = &m_pBits[m_nCurBit >> 3];

		m_nCurBit += 24;

		if (m_bBigEndian)
		{
			pb[0] = static_cast<BYTE>(lValue >> 16);
			pb[1] = static_cast<BYTE>((lValue >> 8) & 0xFF);
			pb[2] = static_cast<BYTE>(lValue & 0xFF);
		}
		else
		{
			pb[0] = static_cast<BYTE>(lValue & 0xFF);
			pb[1] = static_cast<BYTE>((lValue >> 8) & 0xFF);
			pb[2] = static_cast<BYTE>(lValue >> 16);
		}
	}
}

void BitstreamWriter::Write32(long lValue)
{
	ASSERT(IsByteAligned());
	ASSERT(GetBitLeft() >= 32);
		
	if (GetBitLeft() >= 32)
	{
		BYTE* pb = &m_pBits[m_nCurBit >> 3];

		m_nCurBit += 32;

		if (m_bBigEndian)
		{
			pb[0] = static_cast<BYTE>(lValue >> 24);
			pb[1] = static_cast<BYTE>((lValue >> 16) & 0xFF);
			pb[2] = static_cast<BYTE>((lValue >> 8)  & 0xFF);
			pb[3] = static_cast<BYTE>(lValue & 0xFF);
		}
		else
		{
			*(reinterpret_cast<long*>(pb)) = lValue;
		}
	}
}

void BitstreamWriter::Write64(LONGLONG llValue)
{
	ASSERT(IsByteAligned());
	ASSERT(GetBitLeft() >= 64);
		
	if (GetBitLeft() >= 64)
	{
		BYTE* pb = &m_pBits[m_nCurBit >> 3];

		m_nCurBit += 64;

		if (m_bBigEndian)
		{
			pb[0] = static_cast<BYTE>(llValue >> 56);
			pb[1] = static_cast<BYTE>((llValue >> 48) & 0xFF);
			pb[2] = static_cast<BYTE>((llValue >> 40) & 0xFF);
			pb[3] = static_cast<BYTE>((llValue >> 32) & 0xFF);		
			pb[4] = static_cast<BYTE>((llValue >> 24) & 0xFF);
			pb[5] = static_cast<BYTE>((llValue >> 16) & 0xFF);
			pb[6] = static_cast<BYTE>((llValue >> 8)  & 0xFF);
			pb[7] = static_cast<BYTE>(llValue & 0xFF);
		}
		else
		{
			*(reinterpret_cast<LONGLONG*>(pb)) = llValue;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// BitstreamReader.

BitstreamReader::BitstreamReader(const BYTE* pBuffer, UINT cBytes, bool bBigEndian) :
	Bitstream(cBytes << 3, bBigEndian),
	m_pBits(pBuffer)
{
}

int BitstreamReader::Read(UINT cBits)
{
	ASSERT(m_pBits);
	ASSERT(cBits > 0 && cBits <= 32);
	ASSERT(GetBitLeft() >= cBits);

	int nValue = 0;
	
	if (GetBitLeft() >= cBits)
	{
		for (UINT i = 0; i < cBits; i++)
		{
			nValue <<= 1;

			if (m_pBits[m_nCurBit >> 3] & (1 << (7 - (m_nCurBit & 7))))
			{
				nValue |= 1;
			}

			m_nCurBit++;
		}
	}

	return nValue;
}

char BitstreamReader::Read8(void)
{
	ASSERT(IsByteAligned());
	ASSERT(GetBitLeft() >= 8);
	
	char bValue = 0;
		
	if (GetBitLeft() >= 8)
	{
		bValue = m_pBits[m_nCurBit >> 3];
		m_nCurBit += 8;
	}

	return bValue;
}

short BitstreamReader::Read16(void)
{
	ASSERT(IsByteAligned());
	ASSERT(GetBitLeft() >= 16);

	short nValue = 0;
	
	if (GetBitLeft() >= 16)
	{
		const BYTE* pb = &m_pBits[m_nCurBit >> 3];
		
		m_nCurBit += 16;

		if (m_bBigEndian)
		{
			nValue = (pb[0] << 8) | pb[1];
		}
		else
		{
			nValue = *reinterpret_cast<const short*>(pb);
		}
	}

	return nValue;
}

long BitstreamReader::Read24(void)
{
	ASSERT(IsByteAligned());
	ASSERT(GetBitLeft() >= 24);

	long lValue = 0;
	
	if (GetBitLeft() >= 24)
	{
		const BYTE* pb = &m_pBits[m_nCurBit >> 3];
		
		m_nCurBit += 24;

		if (m_bBigEndian)
		{
			lValue = (pb[0] << 16) | (pb[1] << 8) | pb[2];
		}
		else
		{
			lValue = (*reinterpret_cast<const long *>(pb)) & 0x00FFFFFF;
		}
	}

	return lValue;
}

long BitstreamReader::Read32(void)
{
	ASSERT(IsByteAligned());
	ASSERT(GetBitLeft() >= 32);

	long lValue = 0;
	
	if (GetBitLeft() >= 32)
	{
		const BYTE* pb = &m_pBits[m_nCurBit >> 3];
		
		m_nCurBit += 32;
	
		if (m_bBigEndian)
		{
			lValue = (pb[0] << 24) | (pb[1] << 16) | (pb[2] << 8) | pb[3];
		}
		else
		{
			lValue = *reinterpret_cast<const long *>(pb);
		}
	}

	return lValue;
}

LONGLONG BitstreamReader::Read64(void)
{
	ASSERT(IsByteAligned());
	ASSERT(GetBitLeft() >= 64);

	LONGLONG llValue = 0LL;
	
	if (GetBitLeft() >= 64)
	{
		const BYTE* pb = &m_pBits[m_nCurBit >> 3];
		
		m_nCurBit += 64;
	
		if (m_bBigEndian)
		{
			llValue = 
				(static_cast<LONGLONG>(
					(pb[0] << 24) | (pb[1] << 16) | (pb[2] << 8) | pb[3]) << 32) |
					(pb[4] << 24) | (pb[5] << 16) | (pb[6] << 8) | pb[7];
		}
		else
		{
			llValue = *reinterpret_cast<const LONGLONG *>(pb);
		}
	}

	return llValue;
}
