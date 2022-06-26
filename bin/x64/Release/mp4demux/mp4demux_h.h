

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Tue Jan 19 05:14:07 2038
 */
/* Compiler settings for mp4demux.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.01.0628 
    protocol : all , ms_ext, c_ext, robust
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


#ifndef __mp4demux_h_h__
#define __mp4demux_h_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef DECLSPEC_XFGVIRT
#if defined(_CONTROL_FLOW_GUARD_XFG)
#define DECLSPEC_XFGVIRT(base, func) __declspec(xfg_virtual(base, func))
#else
#define DECLSPEC_XFGVIRT(base, func)
#endif
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


EXTERN_C const IID LIBID_GdclMp4Demux;

#ifndef __IDemuxOutputPin_INTERFACE_DEFINED__
#define __IDemuxOutputPin_INTERFACE_DEFINED__

/* interface IDemuxOutputPin */
/* [unique][nonextensible][uuid][object] */ 


EXTERN_C const IID IID_IDemuxOutputPin;

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
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IDemuxOutputPin * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IDemuxOutputPin * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IDemuxOutputPin * This);
        
        DECLSPEC_XFGVIRT(IDemuxOutputPin, GetMediaSampleTimes)
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


EXTERN_C const IID IID_IDemuxFilter;

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
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IDemuxFilter * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IDemuxFilter * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IDemuxFilter * This);
        
        DECLSPEC_XFGVIRT(IDemuxFilter, GetInvalidTrackCount)
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


EXTERN_C const CLSID CLSID_DemuxFilter;

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


