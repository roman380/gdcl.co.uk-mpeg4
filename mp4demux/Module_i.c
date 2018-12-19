

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


 /* File created by MIDL compiler version 8.01.0622 */
/* at Tue Jan 19 05:14:07 2038
 */
/* Compiler settings for Module.idl:
    Oicf, W1, Zp8, env=Win32 (32b run), target_arch=X86 8.01.0622 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */



#ifdef __cplusplus
extern "C"{
#endif 


#include <rpc.h>
#include <rpcndr.h>

#ifdef _MIDL_USE_GUIDDEF_

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)

#else // !_MIDL_USE_GUIDDEF_

#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        EXTERN_C __declspec(selectany) const type name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#endif // !_MIDL_USE_GUIDDEF_

MIDL_DEFINE_GUID(IID, LIBID_GdclMp4Demux,0x2B5B3422,0xE4C3,0x4401,0xA3,0x8F,0x32,0x73,0x7F,0xFB,0x04,0x50);


MIDL_DEFINE_GUID(IID, IID_IDemuxOutputPin,0x1B2E20A1,0x8C41,0x4313,0xBA,0x18,0x0A,0x3D,0xA4,0x84,0xF2,0xEC);


MIDL_DEFINE_GUID(IID, IID_IDemuxFilter,0xF05F893D,0x02D7,0x4256,0x8E,0xFA,0x2A,0xCC,0x26,0x35,0x25,0x66);


MIDL_DEFINE_GUID(CLSID, CLSID_DemuxFilter,0x025BE2E4,0x1787,0x4da4,0xA5,0x85,0xC5,0xB2,0xB9,0xEE,0xB5,0x7C);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



