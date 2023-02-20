#pragma once

#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

struct SourceCodeReference
{
	SourceCodeReference(char const* File, int Line, char const* Function) :
		File(File),
		Line(Line),
		Function(Function)
	{
	}
	char const* FileName() const
	{
		if(NotShorten)
			return File;
		char const* Separator = strrchr(File, '\\');
		if(!Separator)
			return File;
		return Separator + 1;
	}

	char const* File;
	bool NotShorten = false;
	int Line;
	char const* Function;
};

struct TraceContext
{
	TraceContext(char const* File, int Line, char const* Function) :
		SourceCodeReference(File, Line, Function)
	{
	}
	explicit TraceContext(SourceCodeReference& SourceCodeReference) :
		SourceCodeReference(SourceCodeReference)
	{
	}
	void operator () (wchar_t const* Format, ...) const
	{
		va_list Arguments;
		va_start(Arguments, Format);
		wchar_t TextA[8 << 10] { };
		vswprintf_s(TextA, Format, Arguments);
		va_end(Arguments);
		wchar_t TextB[8 << 10] { };
		swprintf_s(TextB, L"%hs(%d): %hs: ", SourceCodeReference.FileName(), SourceCodeReference.Line, SourceCodeReference.Function);
		wcscat_s(TextB, TextA);
		OutputDebugStringW(TextB);
	}

	SourceCodeReference SourceCodeReference;
};

#if !defined(NDEBUG)
	#define TRACE TraceContext(__FILE__, __LINE__, __FUNCTION__)
#else
	#define TRACE 1 ? 0 :
#endif
