

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 7.00.0555 */
/* at Sat May 09 02:24:58 2015
 */
/* Compiler settings for Module.idl:
    Oicf, W1, Zp8, env=Win32 (32b run), target_arch=X86 7.00.0555 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__


#ifndef __Module_i_h__
#define __Module_i_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
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


DEFINE_GUID(LIBID_GdclMp4Mux,0x01C87765,0xFA54,0x45C5,0xAB,0xB4,0x3C,0x74,0x82,0x46,0xAE,0x77);

#ifndef __IMuxMemAllocator_INTERFACE_DEFINED__
#define __IMuxMemAllocator_INTERFACE_DEFINED__

/* interface IMuxMemAllocator */
/* [unique][nonextensible][uuid][object] */ 


DEFINE_GUID(IID_IMuxMemAllocator,0xB583CDE8,0x2E32,0x468A,0xAA,0x60,0x36,0xD2,0x0C,0x86,0x28,0xA1);

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("B583CDE8-2E32-468A-AA60-36D20C8628A1")
    IMuxMemAllocator : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetMinimalBufferCount( 
            LONG *pnMinimalBufferCount) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetMinimalBufferCount( 
            LONG nMinimalBufferCount) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IMuxMemAllocatorVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMuxMemAllocator * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMuxMemAllocator * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMuxMemAllocator * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetMinimalBufferCount )( 
            IMuxMemAllocator * This,
            LONG *pnMinimalBufferCount);
        
        HRESULT ( STDMETHODCALLTYPE *SetMinimalBufferCount )( 
            IMuxMemAllocator * This,
            LONG nMinimalBufferCount);
        
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


DEFINE_GUID(IID_IMuxInputPin,0x51613985,0xD540,0x435B,0xBE,0xBC,0x68,0xD2,0x52,0x43,0xE2,0x39);

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("51613985-D540-435B-BEBC-68D25243E239")
    IMuxInputPin : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetMemAllocators( 
            IUnknown **ppMemAllocatorUnknown,
            IUnknown **ppCopyMemAllocatorUnknown) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IMuxInputPinVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMuxInputPin * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMuxInputPin * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMuxInputPin * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetMemAllocators )( 
            IMuxInputPin * This,
            IUnknown **ppMemAllocatorUnknown,
            IUnknown **ppCopyMemAllocatorUnknown);
        
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

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMuxInputPin_INTERFACE_DEFINED__ */


#ifndef __IMuxFilter_INTERFACE_DEFINED__
#define __IMuxFilter_INTERFACE_DEFINED__

/* interface IMuxFilter */
/* [unique][nonextensible][uuid][object] */ 


DEFINE_GUID(IID_IMuxFilter,0x6CE45967,0xF228,0x4F7B,0x8B,0x93,0x83,0xDC,0x59,0x96,0x18,0xCA);

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("6CE45967-F228-4F7B-8B93-83DC599618CA")
    IMuxFilter : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE IsTemporaryIndexFileEnabled( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetTemporaryIndexFileEnabled( 
            BOOL bEnabled) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IMuxFilterVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMuxFilter * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMuxFilter * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMuxFilter * This);
        
        HRESULT ( STDMETHODCALLTYPE *IsTemporaryIndexFileEnabled )( 
            IMuxFilter * This);
        
        HRESULT ( STDMETHODCALLTYPE *SetTemporaryIndexFileEnabled )( 
            IMuxFilter * This,
            BOOL bEnabled);
        
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

#define IMuxFilter_SetTemporaryIndexFileEnabled(This,bEnabled)	\
    ( (This)->lpVtbl -> SetTemporaryIndexFileEnabled(This,bEnabled) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMuxFilter_INTERFACE_DEFINED__ */


DEFINE_GUID(CLSID_MuxFilter,0x5FD85181,0xE542,0x4e52,0x8D,0x9D,0x5D,0x61,0x3C,0x30,0x13,0x1B);

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


