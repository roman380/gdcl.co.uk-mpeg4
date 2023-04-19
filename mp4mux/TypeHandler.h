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

    Descriptor(TagType Type) :
        m_Type(Type)
    {
    }

    void Append(uint8_t const* Data, size_t DataSize)
    {
	    std::copy(Data, Data + DataSize, std::back_inserter(m_Data));
    }
    void Append(Descriptor* Descriptor)
    {
        auto Data = Descriptor->Compose();
	    std::copy(Data.cbegin(), Data.cend(), std::back_inserter(m_Data));
    }
    std::vector<uint8_t> Compose() const
    {
        size_t HeaderSize = 2;
        for(size_t Size = m_Data.size(); Size > 0x7F; )
        {
            HeaderSize++;
            Size >>= 7;
        }
        HeaderSize += m_Data.size();
        std::vector<uint8_t> Data;
        Data.reserve(16 + m_Data.size());
        Data.emplace_back(static_cast<uint8_t>(m_Type));
        if(!m_Data.empty())
        {
		    for(size_t Size = m_Data.size(); Size; )
		    {
			    uint8_t Value = static_cast<uint8_t>(Size & 0x7F);
			    Size >>= 7;
			    if(Size)
				    Value |= 0x80;
                Data.emplace_back(Value);
		    }
	        std::copy(m_Data.cbegin(), m_Data.cend(), std::back_inserter(Data));
        } else
            Data.emplace_back(static_cast<uint8_t>(0u));
        return Data;
    }
    HRESULT Write(std::shared_ptr<Atom> const& Atom) const;

private:
    TagType const m_Type;
    std::vector<uint8_t> m_Data;
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

