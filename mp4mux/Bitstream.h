#pragma once

class Bitstream
{
public:
    Bitstream(size_t BitCount, bool BigEndian) :
        m_BigEndian(BigEndian),
        m_BitCount(BitCount)
    {
    }

    void Position(size_t Position)
    {
        m_Position = Position;
        WI_ASSERT(m_Position <= m_BitCount);
    }
    void Skip(unsigned int BitCount) 
    { 
        m_Position += BitCount;
        WI_ASSERT(m_Position <= m_BitCount);
    }
    size_t BitLeft() const
    {
        return m_BitCount - m_Position;
    }

    bool IsByteAligned() const
    {
        return (m_Position & 7) == 0;
    }
    size_t ByteLeft() const
    {
        return (m_Position + 7) >> 3;
    }

protected:
    bool m_BigEndian;
    size_t m_BitCount;
    size_t m_Position = 0;
};

class BitstreamWriter : public Bitstream
{
public:
    BitstreamWriter(uint8_t* pBuffer, size_t cBytes, bool bBigEndian = true) :
        Bitstream(cBytes << 3, bBigEndian),
        m_Data(pBuffer)
    {
    }

    void Write(uintptr_t Value, unsigned int BitCount)
    {
        WI_ASSERT(m_Data);
        WI_ASSERT(BitCount > 0 && BitCount <= 32);
        WI_ASSERT(BitLeft() >= BitCount);
        if(BitLeft() < BitCount)
            return;
        for(unsigned int BitIndex = BitCount; BitIndex > 0; BitIndex--)
        {
            uint8_t const Bit = 0x80 >> (m_Position & 7);
            auto& Data = m_Data[m_Position >> 3];
            if(Value & (static_cast<uintptr_t>(1u) << (BitIndex - 1)))
                Data |= Bit;
            else
                Data &= ~Bit;
            m_Position++;
        }
    }
    void Reserve(unsigned int BitCount)
    {
        Write(0, BitCount);
    }

    void Write8(uint8_t Value)
    {
        WI_ASSERT(IsByteAligned());
        WI_ASSERT(BitLeft() >= 8);
        if(BitLeft() < 8)
            return;
        m_Data[m_Position >> 3] = Value;
        m_Position += 8;
    }
    void Write16(uint16_t Value)
    {
        WI_ASSERT(IsByteAligned());
        WI_ASSERT(BitLeft() >= 16);
        if(BitLeft() < 16)
            return;
        *reinterpret_cast<uint16_t*>(m_Data + (m_Position >> 3)) = m_BigEndian ? _byteswap_ushort(Value) : Value;
        m_Position += 16;
    }
    void Write24(uint32_t Value)
    {
        WI_ASSERT(IsByteAligned());
        WI_ASSERT(BitLeft() >= 32); // WARN: Writing beyond bits we have to write
        if(BitLeft() < 32)
            return;
        *reinterpret_cast<uint32_t*>(m_Data + (m_Position >> 3)) = m_BigEndian ? _byteswap_ulong(Value) >> 8 : Value;
        m_Position += 24;
    }
    void Write32(uint32_t Value)
    {
        WI_ASSERT(IsByteAligned());
        WI_ASSERT(BitLeft() >= 32);
        if(BitLeft() < 32)
            return;
        *reinterpret_cast<uint32_t*>(m_Data + (m_Position >> 3)) = m_BigEndian ? _byteswap_ulong(Value) : Value;
        m_Position += 32;
    }
    //void Write64(uint64_t Value)
    //{
    //    WI_ASSERT(IsByteAligned());
    //    WI_ASSERT(BitLeft() >= 64);
    //    if(BitLeft() < 64)
    //        return;
    //    *reinterpret_cast<uint64_t*>(m_Data + (m_Position >> 3)) = m_BigEndian ? _byteswap_uint64(Value) : Value;
    //    m_Position += 64;
    //}

    void WriteBytes(uint8_t const* Data, size_t ByteCount)
    {
        WI_ASSERT(IsByteAligned());
        WI_ASSERT(m_Data);
        auto const cBits = ByteCount << 3;
        WI_ASSERT(BitLeft() >= cBits);
        if(BitLeft() < cBits)
            return;
        std::memcpy(m_Data + (m_Position >> 3), Data, ByteCount);
        m_Position += cBits;
    }
    void ReserveBytes(size_t ByteCount)
    {
        WI_ASSERT(IsByteAligned());
        WI_ASSERT(m_Data);
        auto const BitCount = ByteCount << 3;
        WI_ASSERT(BitLeft() >= BitCount);
        if(BitLeft() < BitCount)
            return;
        std::memset(m_Data + (m_Position >> 3), 0, ByteCount);
        m_Position += BitCount;
    }

protected:
    uint8_t* m_Data;
};

class BitstreamReader : public Bitstream
{
public:
    BitstreamReader(uint8_t const* Data, size_t DataSize, bool BigEndian = true) :
        Bitstream(DataSize << 3, BigEndian),
        m_Data(Data)
    {
    }

    uintptr_t Read(unsigned int BitCount)
    {
        WI_ASSERT(m_Data);
        WI_ASSERT(BitCount > 0 && BitCount <= sizeof(uintptr_t) * 8);
        WI_ASSERT(BitLeft() >= BitCount);
        if(BitLeft() < BitCount)
            return 0;
        uintptr_t Value = 0;
        for(unsigned int BitIndex = 0; BitIndex < BitCount; BitIndex++)
        {
            Value <<= 1;
            if(m_Data[m_Position >> 3] & (0x80 >> (m_Position & 7)))
                Value |= 1;
            m_Position++;
        }
        return Value;
    }
    void SkipIfBitSet(unsigned int BitCount)
    {
        if(Read(1))
            Skip(BitCount);
    }

    //uint8_t Read8()
    //{
    //	WI_ASSERT(IsByteAligned());
    //	WI_ASSERT(BitLeft() >= 8);
    //	if(BitLeft() < 8)
    //		return 0;
    //	auto const Value = m_Data[m_Position >> 3];
    //	m_Position += 8;
    //	return Value;
    //}
    uint16_t Read16()
    {
        WI_ASSERT(IsByteAligned());
        WI_ASSERT(BitLeft() >= 16);
        if(BitLeft() < 16)
            return 0;
        auto const Value = *reinterpret_cast<uint16_t const*>(m_Data + (m_Position >> 3));
        m_Position += 16;
        return m_BigEndian ? _byteswap_ushort(Value) : Value;
    }
    //uint32_t Read24()
    //{
    //	WI_ASSERT(IsByteAligned());
    //	WI_ASSERT(BitLeft() >= 24);
    //	if(BitLeft() < 24)
    //		return 0;
    //	// WARN: Unsafely reading extra byte
    //	auto const Value = *reinterpret_cast<uint32_t const*>(m_Data + (m_Position >> 3));
    //	m_Position += 24;
    //	return m_BigEndian ? _byteswap_ulong(Value) >> 8 : Value & 0x00FFFFFF;
    //}
    //uint32_t Read32()
    //{
    //	WI_ASSERT(IsByteAligned());
    //	WI_ASSERT(BitLeft() >= 32);
    //	if(BitLeft() < 32)
    //		return 0;
    //	auto const Value = *reinterpret_cast<uint32_t const*>(m_Data + (m_Position >> 3));
    //	m_Position += 32;
    //	return m_BigEndian ? _byteswap_ulong(Value) : Value;
    //}
    //uint64_t Read64()
    //{
    //	WI_ASSERT(IsByteAligned());
    //	WI_ASSERT(BitLeft() >= 64);
    //	if(BitLeft() < 64)
    //		return 0;
    //	auto const Value = *reinterpret_cast<uint64_t const*>(m_Data + (m_Position >> 3));
    //	m_Position += 64;
    //	return m_BigEndian ? _byteswap_uint64(Value) : Value;
    //}

protected:
    uint8_t const* m_Data;
};
