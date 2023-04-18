// TypeHandler.h: interface for type-specific handlers.
//
// Copyright (c) GDCL 2004-6. All Rights Reserved. 
// You are free to re-use this as the basis for your own filter development,
// provided you retain this copyright notice in the source.
// http://www.gdcl.co.uk
//////////////////////////////////////////////////////////////////////

#pragma once

// NOTE: Orignial Geraint's project uses 90 kHz clock, and 360 kHz advantage is that frame rates up to 60M and 60 are mapped 
//       accurately without integer rounding precision loss
#define DEFAULT_TIMESCALE 360000 // 90000

class Atom;
class MovieWriter;

// directshow subtypes

class Descriptor
{
public:
    enum TagType
    {
        Invalid = 0,
        ES_Desc = 3,
        Decoder_Config = 4,
        Decoder_Specific_Info = 5,
        SL_Config = 6,
        ES_ID_Inc = 0x0e,
        ES_ID_Ref = 0x0f,
        MP4_IOD = 0x10,
        MP4_OD = 0x11,

        // Command tags
        ObjDescrUpdate = 1,
    };

    Descriptor(TagType type) :
        m_type(type)
    {
    }

    void Append(uint8_t const* pBuffer, size_t cBytes)
    {
	    std::copy(pBuffer, pBuffer + cBytes, std::back_inserter(m_pBuffer));
    }
    void Append(Descriptor* pdesc)
    {
        auto const cBytes = pdesc->Length();
        Reserve(cBytes);
        m_pBuffer.resize(m_pBuffer.size() + cBytes);
        pdesc->Write(m_pBuffer.data() + m_pBuffer.size());
    }
    size_t Length() const
    {
        long cHdr = 2;
        size_t cBody = m_pBuffer.size();
        while (cBody > 0x7f)
        {
            cHdr++;
            cBody >>= 7;
        }
        return cHdr + m_pBuffer.size();

    }
    void Write(uint8_t* pBuffer) const
    {
        size_t idx = 0;
        pBuffer[idx++] = static_cast<uint8_t>(m_type);
	    if (m_pBuffer.empty())
	    {
		    pBuffer[idx++] = 0;
            return;
        }
		size_t cBody = m_pBuffer.size();
		while (cBody)
		{
			uint8_t b = static_cast<uint8_t>(cBody & 0x7f);
			cBody >>= 7;
			if (cBody)
				b |= 0x80;
			pBuffer[idx++] = b;
		}
	    std::memcpy(pBuffer + idx, m_pBuffer.data(), m_pBuffer.size());
    }
    HRESULT Write(std::shared_ptr<Atom> const& patm) const;

private:
    void Reserve(size_t cBytes)
    {
        static auto constexpr const g_Alignment = 128;
        static_assert(!(g_Alignment & (g_Alignment - 1)));
        m_pBuffer.reserve((m_pBuffer.size() + cBytes + (g_Alignment - 1)) & ~(g_Alignment - 1));
    }

    TagType const m_type;
    std::vector<uint8_t> m_pBuffer;
};

class Atom;

// abstract interface and factory
class TypeHandler  
{
public:
    virtual ~TypeHandler() {}

    virtual DWORD Handler() = 0;
	virtual DWORD DataType()
	{
		return DWORD('mhlr');
	}
    virtual void WriteTREF(std::shared_ptr<Atom> const& Atom)
    {
		UNREFERENCED_PARAMETER(Atom);
    }
    virtual bool IsVideo() = 0;
    virtual bool IsAudio() = 0;
	virtual bool IsOldIndexFormat() { return false; }
	virtual bool IsNonMP4()			{ return IsOldIndexFormat(); }
    virtual void WriteDescriptor(std::shared_ptr<Atom> const& Atom, int id, int dataref, long scale) = 0;
    virtual long SampleRate() = 0;
    virtual long Scale() = 0;
	virtual long Width() = 0;
	virtual long Height() = 0;
	virtual long BlockAlign() { return 1; }
	virtual bool CanTruncate() { return false; }
	virtual bool Truncate(IMediaSample* pSample, REFERENCE_TIME tNewStart) 
	{ 
		UNREFERENCED_PARAMETER(pSample);
		UNREFERENCED_PARAMETER(tNewStart);
		return false; 
	}
	virtual LONGLONG FrameDuration() 
	{
		// default answer
		return UNITS / SampleRate();
	}

	virtual HRESULT WriteData(std::shared_ptr<Atom> const& Atom, uint8_t const* pData, size_t cBytes, size_t* pcActual);
    static bool CanSupport(const CMediaType* pmt);
    static std::unique_ptr<TypeHandler> Make(const CMediaType* pmt);
};

