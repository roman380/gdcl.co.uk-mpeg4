#pragma once

#define NOMINMAX

#include <winsdkver.h>
#include <sdkddkver.h>

#include <windows.h>
#include "oaidl.h"
#include "ocidl.h"
#include <shlwapi.h>

#include <restrictederrorinfo.h>
#include <wil\resource.h>
#include <wil\com.h>
#include <wil\winrt.h>

#include <unknwn.h>
#include <winrt\base.h>
#include <winrt\Windows.Foundation.h>

#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "runtimeobject.lib")

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxva2.lib")

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

#if defined(WITH_DIRECTSHOWREFERENCESOURCE)
	#pragma message("Using Alax.info DirectShowReferenceSource integration")
	namespace AlaxInfoDirectShowReferenceSource
	{
		#include "ReferenceSource\ReferenceSource_h.h"
		#include "ReferenceSource\ReferenceSource_i.c"
	}
#endif
