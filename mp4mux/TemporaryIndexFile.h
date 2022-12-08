#pragma once

#include <string>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

#include <wil\resource.h>
#include <wil\com.h>
#include <wil\winrt.h>

class CTemporaryIndexFileSite :
	public IUnknown
{
};

class CTemporaryIndexFile
{
public:
	static std::wstring DefaultDirectory()
	{
		wchar_t Directory[MAX_PATH] { };
		WI_VERIFY(GetTempPathW(_countof(Directory), Directory));
		wchar_t ModulePath[MAX_PATH] { };
		WI_VERIFY(GetModuleFileNameW(g_hInst, ModulePath, _countof(ModulePath)));
		WI_VERIFY(PathCombineW(Directory, Directory, PathFindFileNameW(ModulePath)));
		CreateDirectory(Directory, nullptr);
		return Directory;
	}
	static std::wstring TemporaryFileName(wchar_t const* FileName)
	{
		wchar_t Result[MAX_PATH] { };
		swprintf_s(Result, L"%s-Index.tmp", FileName);
		return Result;
	}
	bool Initialize(wchar_t const* FileName)
	{
		if(!FileName)
			return false; // No File Name
		wchar_t Path[MAX_PATH] { };
		WI_VERIFY(PathCombineW(Path, DefaultDirectory().c_str(), TemporaryFileName(FileName).c_str()));
		m_Path = Path;
		m_File.reset(CreateFileW(m_Path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
		return m_File.is_valid();
	}
	void Terminate()
	{
		if(!IsActive())
			return;
		m_File.reset();
		WI_VERIFY(DeleteFileW(m_Path.c_str()));
	}
	bool IsActive() const
	{
		return m_File.is_valid();
	}
	void WriteHeader()
	{
		DWORD WriteDataSize;
		static uint32_t constexpr const g_Signature = MAKEFOURCC('M', 'P', '4', 'I');
		static uint32_t constexpr const g_Version = 1;
		LOG_IF_WIN32_BOOL_FALSE(WriteFile(m_File.get(), &g_Signature, sizeof g_Signature, &WriteDataSize, nullptr));
		LOG_IF_WIN32_BOOL_FALSE(WriteFile(m_File.get(), &g_Version, sizeof g_Version, &WriteDataSize, nullptr));
		m_MediaSampleIndex = 0;
	}
	void WriteInputPin(uint16_t Index, CMediaType const& MediaType)
	{
		DWORD WriteDataSize;
		static uint32_t constexpr const g_Signature = MAKEFOURCC('I', 'P', 'I', 'N');
		LOG_IF_WIN32_BOOL_FALSE(WriteFile(m_File.get(), &g_Signature, sizeof g_Signature, &WriteDataSize, nullptr));
		LOG_IF_WIN32_BOOL_FALSE(WriteFile(m_File.get(), &Index, sizeof Index, &WriteDataSize, nullptr));
		LOG_IF_WIN32_BOOL_FALSE(WriteFile(m_File.get(), static_cast<AM_MEDIA_TYPE const*>(&MediaType), sizeof (AM_MEDIA_TYPE), &WriteDataSize, nullptr));
		if(MediaType.FormatLength())
			LOG_IF_WIN32_BOOL_FALSE(WriteFile(m_File.get(), MediaType.Format(), static_cast<DWORD>(MediaType.FormatLength()), &WriteDataSize, nullptr));
	}
	void WriteMediaSample(uint16_t Index, uint64_t DataPosition, uint32_t DataSize, AM_SAMPLE2_PROPERTIES const& Properties)
	{
		DWORD WriteDataSize;
		static uint32_t constexpr const g_Signature = MAKEFOURCC('S', 'A', 'M', 'P');
		#pragma region Structure
		#pragma pack(push, 1)
		struct MEDIASAMPLE
		{
			uint32_t Signature;
			uint16_t Index;
			uint64_t Position;
			uint32_t SampleFlags;
			uint32_t Size;
			uint64_t StartTime;
			uint32_t LengthTime;
		};
		#pragma pack(pop)
		#pragma endregion
		MEDIASAMPLE MediaSample;
		MediaSample.Signature = g_Signature;
		MediaSample.Index = Index;
		MediaSample.Position = DataPosition;
		MediaSample.SampleFlags = static_cast<uint32_t>(Properties.dwSampleFlags);
		MediaSample.Size = DataSize; //static_cast<uint32_t>(Properties.lActual);
		MediaSample.StartTime = static_cast<uint64_t>(Properties.tStart);
		MediaSample.LengthTime = static_cast<uint32_t>(Properties.tStop - Properties.tStart);
		LOG_IF_WIN32_BOOL_FALSE(WriteFile(m_File.get(), &MediaSample, sizeof MediaSample, &WriteDataSize, nullptr));
		if(!(++m_MediaSampleIndex % 1024))
			LOG_IF_WIN32_BOOL_FALSE(FlushFileBuffers(m_File.get()));
	}

private:
	std::wstring m_Path;
	wil::unique_hfile m_File;
	size_t m_MediaSampleIndex;
};
