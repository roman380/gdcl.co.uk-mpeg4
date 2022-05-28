

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


 /* File created by MIDL compiler version 8.01.0626 */
/* at Tue Jan 19 05:14:07 2038
 */
/* Compiler settings for mp4mux.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.01.0626 
    protocol : all , ms_ext, c_ext, robust
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

MIDL_DEFINE_GUID(IID, LIBID_GdclMp4Mux,0x01C87765,0xFA54,0x45C5,0xAB,0xB4,0x3C,0x74,0x82,0x46,0xAE,0x77);


MIDL_DEFINE_GUID(IID, IID_IMuxMemAllocator,0xB583CDE8,0x2E32,0x468A,0xAA,0x60,0x36,0xD2,0x0C,0x86,0x28,0xA1);


MIDL_DEFINE_GUID(IID, IID_IMuxInputPin,0x51613985,0xD540,0x435B,0xBE,0xBC,0x68,0xD2,0x52,0x43,0xE2,0x39);


MIDL_DEFINE_GUID(IID, IID_IMuxFilter,0x6CE45967,0xF228,0x4F7B,0x8B,0x93,0x83,0xDC,0x59,0x96,0x18,0xCA);


MIDL_DEFINE_GUID(CLSID, CLSID_MuxFilter,0x5FD85181,0xE542,0x4e52,0x8D,0x9D,0x5D,0x61,0x3C,0x30,0x13,0x1B);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



