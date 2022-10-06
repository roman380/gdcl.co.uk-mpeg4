

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Tue Jan 19 04:14:07 2038
 */
/* Compiler settings for mp4mux.idl:
    Oicf, W1, Zp8, env=Win32 (32b run), target_arch=X86 8.01.0628 
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


#ifndef __mp4mux_h_h__
#define __mp4mux_h_h__

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

#ifndef __IMuxMemAllocator_FWD_DEFINED__
#define __IMuxMemAllocator_FWD_DEFINED__
typedef interface IMuxMemAllocator IMuxMemAllocator;

#endif 	/* __IMuxMemAllocator_FWD_DEFINED__ */


#ifndef __IMuxInputPin_FWD_DEFINED__
#define __IMuxInputPin_FWD_DEFINED__
typedef interface IMuxInputPin IMuxInputPin;

#endif 	/* __IMuxInputPin_FWD_DEFINED__ */


#ifndef __IMuxFilter_FWD_DEFINED__
#define __IMuxFilter_FWD_DEFINED__
typedef interface IMuxFilter IMuxFilter;

#endif 	/* __IMuxFilter_FWD_DEFINED__ */


#ifndef __MuxFilter_FWD_DEFINED__
#define __MuxFilter_FWD_DEFINED__

#ifdef __cplusplus
typedef class MuxFilter MuxFilter;
#else
typedef struct MuxFilter MuxFilter;
#endif /* __cplusplus */

#endif 	/* __MuxFilter_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 



#ifndef __GdclMp4Mux_LIBRARY_DEFINED__
#define __GdclMp4Mux_LIBRARY_DEFINED__

/* library GdclMp4Mux */
/* [version][uuid] */ 


EXTERN_C const IID LIBID_GdclMp4Mux;

#ifndef __IMuxMemAllocator_INTERFACE_DEFINED__
#define __IMuxMemAllocator_INTERFACE_DEFINED__

/* interface IMuxMemAllocator */
/* [unique][nonextensible][uuid][object] */ 


EXTERN_C const IID IID_IMuxMemAllocator;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("B583CDE8-2E32-468A-AA60-36D20C8628A1")
    IMuxMemAllocator : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetMinimalBufferCount( 
            /* [out] */ LONG *pnMinimalBufferCount) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetMinimalBufferCount( 
            /* [in] */ LONG nMinimalBufferCount) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMuxMemAllocatorVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMuxMemAllocator * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMuxMemAllocator * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMuxMemAllocator * This);
        
        DECLSPEC_XFGVIRT(IMuxMemAllocator, GetMinimalBufferCount)
        HRESULT ( STDMETHODCALLTYPE *GetMinimalBufferCount )( 
            IMuxMemAllocator * This,
            /* [out] */ LONG *pnMinimalBufferCount);
        
        DECLSPEC_XFGVIRT(IMuxMemAllocator, SetMinimalBufferCount)
        HRESULT ( STDMETHODCALLTYPE *SetMinimalBufferCount )( 
            IMuxMemAllocator * This,
            /* [in] */ LONG nMinimalBufferCount);
        
        END_INTERFACE
    } IMuxMemAllocatorVtbl;

    interface IMuxMemAllocator
    {
        CONST_VTBL struct IMuxMemAllocatorVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMuxMemAllocator_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMuxMemAllocator_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMuxMemAllocator_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMuxMemAllocator_GetMinimalBufferCount(This,pnMinimalBufferCount)	\
    ( (This)->lpVtbl -> GetMinimalBufferCount(This,pnMinimalBufferCount) ) 

#define IMuxMemAllocator_SetMinimalBufferCount(This,nMinimalBufferCount)	\
    ( (This)->lpVtbl -> SetMinimalBufferCount(This,nMinimalBufferCount) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMuxMemAllocator_INTERFACE_DEFINED__ */


#ifndef __IMuxInputPin_INTERFACE_DEFINED__
#define __IMuxInputPin_INTERFACE_DEFINED__

/* interface IMuxInputPin */
/* [unique][nonextensible][uuid][object] */ 


EXTERN_C const IID IID_IMuxInputPin;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("51613985-D540-435B-BEBC-68D25243E239")
    IMuxInputPin : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetMemAllocators( 
            /* [out] */ IUnknown **ppMemAllocatorUnknown,
            /* [out] */ IUnknown **ppCopyMemAllocatorUnknown) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetMaximalCopyMemAllocatorCapacity( 
            /* [in] */ ULONG nCapacity) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMuxInputPinVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMuxInputPin * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMuxInputPin * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMuxInputPin * This);
        
        DECLSPEC_XFGVIRT(IMuxInputPin, GetMemAllocators)
        HRESULT ( STDMETHODCALLTYPE *GetMemAllocators )( 
            IMuxInputPin * This,
            /* [out] */ IUnknown **ppMemAllocatorUnknown,
            /* [out] */ IUnknown **ppCopyMemAllocatorUnknown);
        
        DECLSPEC_XFGVIRT(IMuxInputPin, SetMaximalCopyMemAllocatorCapacity)
        HRESULT ( STDMETHODCALLTYPE *SetMaximalCopyMemAllocatorCapacity )( 
            IMuxInputPin * This,
            /* [in] */ ULONG nCapacity);
        
        END_INTERFACE
    } IMuxInputPinVtbl;

    interface IMuxInputPin
    {
        CONST_VTBL struct IMuxInputPinVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMuxInputPin_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMuxInputPin_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMuxInputPin_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMuxInputPin_GetMemAllocators(This,ppMemAllocatorUnknown,ppCopyMemAllocatorUnknown)	\
    ( (This)->lpVtbl -> GetMemAllocators(This,ppMemAllocatorUnknown,ppCopyMemAllocatorUnknown) ) 

#define IMuxInputPin_SetMaximalCopyMemAllocatorCapacity(This,nCapacity)	\
    ( (This)->lpVtbl -> SetMaximalCopyMemAllocatorCapacity(This,nCapacity) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMuxInputPin_INTERFACE_DEFINED__ */


#ifndef __IMuxFilter_INTERFACE_DEFINED__
#define __IMuxFilter_INTERFACE_DEFINED__

/* interface IMuxFilter */
/* [unique][nonextensible][uuid][object] */ 


EXTERN_C const IID IID_IMuxFilter;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("6CE45967-F228-4F7B-8B93-83DC599618CA")
    IMuxFilter : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE IsTemporaryIndexFileEnabled( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetTemporaryIndexFileEnabled( 
            /* [in] */ BOOL bTemporaryIndexFileEnabled) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetAlignTrackStartTimeDisabled( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetAlignTrackStartTimeDisabled( 
            /* [in] */ BOOL bAlignTrackStartTimeDisabled) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMinimalMovieDuration( 
            /* [out] */ LONGLONG *pnMinimalMovieDuration) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetMinimalMovieDuration( 
            /* [in] */ LONGLONG nMinimalMovieDuration) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetComment( 
            /* [in] */ BSTR Comment) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMuxFilterVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMuxFilter * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMuxFilter * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMuxFilter * This);
        
        DECLSPEC_XFGVIRT(IMuxFilter, IsTemporaryIndexFileEnabled)
        HRESULT ( STDMETHODCALLTYPE *IsTemporaryIndexFileEnabled )( 
            IMuxFilter * This);
        
        DECLSPEC_XFGVIRT(IMuxFilter, SetTemporaryIndexFileEnabled)
        HRESULT ( STDMETHODCALLTYPE *SetTemporaryIndexFileEnabled )( 
            IMuxFilter * This,
            /* [in] */ BOOL bTemporaryIndexFileEnabled);
        
        DECLSPEC_XFGVIRT(IMuxFilter, GetAlignTrackStartTimeDisabled)
        HRESULT ( STDMETHODCALLTYPE *GetAlignTrackStartTimeDisabled )( 
            IMuxFilter * This);
        
        DECLSPEC_XFGVIRT(IMuxFilter, SetAlignTrackStartTimeDisabled)
        HRESULT ( STDMETHODCALLTYPE *SetAlignTrackStartTimeDisabled )( 
            IMuxFilter * This,
            /* [in] */ BOOL bAlignTrackStartTimeDisabled);
        
        DECLSPEC_XFGVIRT(IMuxFilter, GetMinimalMovieDuration)
        HRESULT ( STDMETHODCALLTYPE *GetMinimalMovieDuration )( 
            IMuxFilter * This,
            /* [out] */ LONGLONG *pnMinimalMovieDuration);
        
        DECLSPEC_XFGVIRT(IMuxFilter, SetMinimalMovieDuration)
        HRESULT ( STDMETHODCALLTYPE *SetMinimalMovieDuration )( 
            IMuxFilter * This,
            /* [in] */ LONGLONG nMinimalMovieDuration);
        
        DECLSPEC_XFGVIRT(IMuxFilter, SetComment)
        HRESULT ( STDMETHODCALLTYPE *SetComment )( 
            IMuxFilter * This,
            /* [in] */ BSTR Comment);
        
        END_INTERFACE
    } IMuxFilterVtbl;

    interface IMuxFilter
    {
        CONST_VTBL struct IMuxFilterVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMuxFilter_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMuxFilter_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMuxFilter_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMuxFilter_IsTemporaryIndexFileEnabled(This)	\
    ( (This)->lpVtbl -> IsTemporaryIndexFileEnabled(This) ) 

#define IMuxFilter_SetTemporaryIndexFileEnabled(This,bTemporaryIndexFileEnabled)	\
    ( (This)->lpVtbl -> SetTemporaryIndexFileEnabled(This,bTemporaryIndexFileEnabled) ) 

#define IMuxFilter_GetAlignTrackStartTimeDisabled(This)	\
    ( (This)->lpVtbl -> GetAlignTrackStartTimeDisabled(This) ) 

#define IMuxFilter_SetAlignTrackStartTimeDisabled(This,bAlignTrackStartTimeDisabled)	\
    ( (This)->lpVtbl -> SetAlignTrackStartTimeDisabled(This,bAlignTrackStartTimeDisabled) ) 

#define IMuxFilter_GetMinimalMovieDuration(This,pnMinimalMovieDuration)	\
    ( (This)->lpVtbl -> GetMinimalMovieDuration(This,pnMinimalMovieDuration) ) 

#define IMuxFilter_SetMinimalMovieDuration(This,nMinimalMovieDuration)	\
    ( (This)->lpVtbl -> SetMinimalMovieDuration(This,nMinimalMovieDuration) ) 

#define IMuxFilter_SetComment(This,Comment)	\
    ( (This)->lpVtbl -> SetComment(This,Comment) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMuxFilter_INTERFACE_DEFINED__ */


EXTERN_C const CLSID CLSID_MuxFilter;

#ifdef __cplusplus

class DECLSPEC_UUID("5FD85181-E542-4e52-8D9D-5D613C30131B")
MuxFilter;
#endif
#endif /* __GdclMp4Mux_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


