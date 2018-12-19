

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


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



/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */


#ifndef __Module_i_h__
#define __Module_i_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IDemuxOutputPin_FWD_DEFINED__
#define __IDemuxOutputPin_FWD_DEFINED__
typedef interface IDemuxOutputPin IDemuxOutputPin;

#endif 	/* __IDemuxOutputPin_FWD_DEFINED__ */


#ifndef __IDemuxFilter_FWD_DEFINED__
#define __IDemuxFilter_FWD_DEFINED__
typedef interface IDemuxFilter IDemuxFilter;

#endif 	/* __IDemuxFilter_FWD_DEFINED__ */


#ifndef __DemuxFilter_FWD_DEFINED__
#define __DemuxFilter_FWD_DEFINED__

#ifdef __cplusplus
typedef class DemuxFilter DemuxFilter;
#else
typedef struct DemuxFilter DemuxFilter;
#endif /* __cplusplus */

#endif 	/* __DemuxFilter_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 



#ifndef __GdclMp4Demux_LIBRARY_DEFINED__
#define __GdclMp4Demux_LIBRARY_DEFINED__

/* library GdclMp4Demux */
/* [version][uuid] */ 


DEFINE_GUID(LIBID_GdclMp4Demux,0x2B5B3422,0xE4C3,0x4401,0xA3,0x8F,0x32,0x73,0x7F,0xFB,0x04,0x50);

#ifndef __IDemuxOutputPin_INTERFACE_DEFINED__
#define __IDemuxOutputPin_INTERFACE_DEFINED__

/* interface IDemuxOutputPin */
/* [unique][nonextensible][uuid][object] */ 


DEFINE_GUID(IID_IDemuxOutputPin,0x1B2E20A1,0x8C41,0x4313,0xBA,0x18,0x0A,0x3D,0xA4,0x84,0xF2,0xEC);

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("1B2E20A1-8C41-4313-BA18-0A3DA484F2EC")
    IDemuxOutputPin : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetMediaSampleTimes( 
            /* [out] */ ULONG *pnCount,
            /* [out] */ LONGLONG **ppnStartTimes,
            /* [out] */ LONGLONG **ppnStopTimes,
            /* [out] */ ULONG **ppnFlags,
            /* [out] */ ULONG **ppnDataSizes) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IDemuxOutputPinVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IDemuxOutputPin * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IDemuxOutputPin * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IDemuxOutputPin * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetMediaSampleTimes )( 
            IDemuxOutputPin * This,
            /* [out] */ ULONG *pnCount,
            /* [out] */ LONGLONG **ppnStartTimes,
            /* [out] */ LONGLONG **ppnStopTimes,
            /* [out] */ ULONG **ppnFlags,
            /* [out] */ ULONG **ppnDataSizes);
        
        END_INTERFACE
    } IDemuxOutputPinVtbl;

    interface IDemuxOutputPin
    {
        CONST_VTBL struct IDemuxOutputPinVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IDemuxOutputPin_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IDemuxOutputPin_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IDemuxOutputPin_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IDemuxOutputPin_GetMediaSampleTimes(This,pnCount,ppnStartTimes,ppnStopTimes,ppnFlags,ppnDataSizes)	\
    ( (This)->lpVtbl -> GetMediaSampleTimes(This,pnCount,ppnStartTimes,ppnStopTimes,ppnFlags,ppnDataSizes) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IDemuxOutputPin_INTERFACE_DEFINED__ */


#ifndef __IDemuxFilter_INTERFACE_DEFINED__
#define __IDemuxFilter_INTERFACE_DEFINED__

/* interface IDemuxFilter */
/* [unique][nonextensible][uuid][object] */ 


DEFINE_GUID(IID_IDemuxFilter,0xF05F893D,0x02D7,0x4256,0x8E,0xFA,0x2A,0xCC,0x26,0x35,0x25,0x66);

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("F05F893D-02D7-4256-8EFA-2ACC26352566")
    IDemuxFilter : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetInvalidTrackCount( 
            /* [out] */ ULONG *pnInvalidTrackCount) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IDemuxFilterVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IDemuxFilter * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IDemuxFilter * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IDemuxFilter * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetInvalidTrackCount )( 
            IDemuxFilter * This,
            /* [out] */ ULONG *pnInvalidTrackCount);
        
        END_INTERFACE
    } IDemuxFilterVtbl;

    interface IDemuxFilter
    {
        CONST_VTBL struct IDemuxFilterVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IDemuxFilter_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IDemuxFilter_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IDemuxFilter_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IDemuxFilter_GetInvalidTrackCount(This,pnInvalidTrackCount)	\
    ( (This)->lpVtbl -> GetInvalidTrackCount(This,pnInvalidTrackCount) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IDemuxFilter_INTERFACE_DEFINED__ */


DEFINE_GUID(CLSID_DemuxFilter,0x025BE2E4,0x1787,0x4da4,0xA5,0x85,0xC5,0xB2,0xB9,0xEE,0xB5,0x7C);

#ifdef __cplusplus

class DECLSPEC_UUID("025BE2E4-1787-4da4-A585-C5B2B9EEB57C")
DemuxFilter;
#endif
#endif /* __GdclMp4Demux_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


