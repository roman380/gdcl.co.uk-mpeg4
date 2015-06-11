#pragma once

#include <string.h>
#include <tchar.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

class CTemporaryIndexFileSite :
	public IUnknown
{
};

class CTemporaryIndexFile
{
private:
	TCHAR m_pszPath[MAX_PATH];
	HANDLE m_hFile;
	SIZE_T m_nMediaSampleIndex;

public:
// CTemporaryIndexFile
	CTemporaryIndexFile() :
		m_hFile(INVALID_HANDLE_VALUE)
	{
	}
	~CTemporaryIndexFile()
	{
		// NOTE: We are not deleting file here, only on explicit termination
		if(IsActive())
			CloseHandle(m_hFile);
	}
	BOOL Initialize(LPCTSTR pszFileName)
	{
		if(!pszFileName)
			return FALSE; // No File Name
		TCHAR pszDirectory[MAX_PATH] = { 0 };
		GetTempPath(_countof(pszDirectory), pszDirectory);
		TCHAR pszModulePath[MAX_PATH] = { 0 };
		GetModuleFileName(g_hInst, pszModulePath, _countof(pszModulePath));
		PathCombine(pszDirectory, pszDirectory, PathFindFileName(pszModulePath));
		CreateDirectory(pszDirectory, NULL);
		TCHAR pszTemporaryFileName[MAX_PATH] = { 0 };
		_stprintf_s(pszTemporaryFileName, _T("%s-Index.tmp"), pszFileName);
		PathCombine(m_pszPath, pszDirectory, pszTemporaryFileName);
		m_hFile = CreateFile(m_pszPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if(m_hFile == INVALID_HANDLE_VALUE)
			return FALSE; // Failed
		return TRUE;
	}
	VOID Terminate()
	{
		if(!IsActive())
			return;
		CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
		DeleteFile(m_pszPath);
	}
	BOOL IsActive() const
	{
		return m_hFile != INVALID_HANDLE_VALUE;
	}
	VOID WriteHeader()
	{
		DWORD nWriteDataSize;
		static const UINT32 g_nSignature = MAKEFOURCC('M', 'P', '4', 'I');
		static const UINT32 g_nVersion = 1;
		WriteFile(m_hFile, &g_nSignature, sizeof g_nSignature, &nWriteDataSize, NULL);
		WriteFile(m_hFile, &g_nVersion, sizeof g_nVersion, &nWriteDataSize, NULL);
		m_nMediaSampleIndex = 0;
	}
	VOID WriteInputPin(UINT16 nIndex, const CMediaType& MediaType)
	{
		DWORD nWriteDataSize;
		static const UINT32 g_nSignature = MAKEFOURCC('I', 'P', 'I', 'N');
		WriteFile(m_hFile, &g_nSignature, sizeof g_nSignature, &nWriteDataSize, NULL);
		WriteFile(m_hFile, &nIndex, sizeof nIndex, &nWriteDataSize, NULL);
		WriteFile(m_hFile, (const AM_MEDIA_TYPE*) &MediaType, sizeof (AM_MEDIA_TYPE), &nWriteDataSize, NULL);
		if(MediaType.FormatLength())
			WriteFile(m_hFile, MediaType.Format(), (DWORD) MediaType.FormatLength(), &nWriteDataSize, NULL);
	}
	VOID WriteMediaSample(UINT16 nIndex, UINT64 nDataPosition, UINT32 nDataSize, const AM_SAMPLE2_PROPERTIES& Properties)
	{
		DWORD nWriteDataSize;
		static const UINT32 g_nSignature = MAKEFOURCC('S', 'A', 'M', 'P');
		#pragma region Structure
		#pragma pack(push, 1)
		typedef struct _MEDIASAMPLE
		{
			UINT32 nSignature;
			UINT16 nIndex;
			UINT64 nPosition;
			UINT32 nSampleFlags;
			UINT32 nSize;
			UINT64 nStartTime;
			UINT32 nLengthTime;
		} MEDIASAMPLE;
		#pragma pack(pop)
		#pragma endregion
		MEDIASAMPLE MediaSample;
		MediaSample.nSignature = g_nSignature;
		MediaSample.nIndex = nIndex;
		MediaSample.nPosition = nDataPosition;
		MediaSample.nSampleFlags = (UINT32) Properties.dwSampleFlags;
		MediaSample.nSize = nDataSize; //(UINT32) Properties.lActual;
		MediaSample.nStartTime = (UINT64) Properties.tStart;
		MediaSample.nLengthTime = (UINT32) (Properties.tStop - Properties.tStart);
		WriteFile(m_hFile, &MediaSample, sizeof MediaSample, &nWriteDataSize, NULL);
		if(!(++m_nMediaSampleIndex % 1024))
			FlushFileBuffers(m_hFile);
	}
};