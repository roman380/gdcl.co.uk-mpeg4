// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#pragma once


// Insert your headers here
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

// ignore deprecated warnings for standard headers
#pragma warning(push)
#pragma warning(disable:4995)

#include <streams.h>

#include <comdef.h>
_COM_SMARTPTR_TYPEDEF(IMediaSample, IID_IMediaSample);
_COM_SMARTPTR_TYPEDEF(IAsyncReader, IID_IAsyncReader);
_COM_SMARTPTR_TYPEDEF(IPin, IID_IPin);

#include "smartptr.h"
#include <list>
#include <vector>

#include <string>
using namespace std;

#pragma warning(pop)

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.


