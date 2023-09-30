// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#pragma once


// Insert your headers here
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define NOMINMAX

// truncation of name in debug info (long template names)
#pragma warning(disable:4786)

// ignore deprecated warnings in standard headers
#pragma warning(push)
#pragma warning(disable: 4995)

#include <streams.h>

#include <comdef.h>
_COM_SMARTPTR_TYPEDEF(IMediaSample, IID_IMediaSample);
_COM_SMARTPTR_TYPEDEF(IMediaSeeking, IID_IMediaSeeking);
_COM_SMARTPTR_TYPEDEF(IMemAllocator, IID_IMemAllocator);

#include <assert.h>

#include <memory>
#include <algorithm>
#include <iterator>
#include <vector>
#include <list>
#include <utility>
#include <string>

#include <mfapi.h> // MFllMulDiv

using namespace std;

#pragma warning(pop)

#include <wil\resource.h>
#include <wil\com.h>

#if defined(WITH_DIRECTSHOWSPY)
	// NOTE: This enables DirectShow Spy integration to review filter graphs being created;
	//       DirectShowSpy has to be installed to compile (and run) this, see http://alax.info/blog/777;
	//       This section can be safely commented out otherwise
	#pragma message("Using Alax.info DirectShowSpy integration")
	//#import "libid:B9EC374B-834B-4DA9-BFB5-C1872CE736FF" raw_interfaces_only // AlaxInfoDirectShowSpy
	namespace AlaxInfoDirectShowSpy
	{
		#include "Spy\Spy_h.h"
		#include "Spy\Spy_i.c"
	}
#endif

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

