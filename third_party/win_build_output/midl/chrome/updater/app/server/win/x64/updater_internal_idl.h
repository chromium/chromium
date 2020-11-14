

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for gen/chrome/updater/app/server/win/updater_internal_idl.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.xx.xxxx 
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
#endif /* __RPCNDR_H_VERSION__ */

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __updater_internal_idl_h__
#define __updater_internal_idl_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IUpdaterControlCallback_FWD_DEFINED__
#define __IUpdaterControlCallback_FWD_DEFINED__
typedef interface IUpdaterControlCallback IUpdaterControlCallback;

#endif 	/* __IUpdaterControlCallback_FWD_DEFINED__ */


#ifndef __IUpdaterControl_FWD_DEFINED__
#define __IUpdaterControl_FWD_DEFINED__
typedef interface IUpdaterControl IUpdaterControl;

#endif 	/* __IUpdaterControl_FWD_DEFINED__ */


#ifndef __UpdaterControlClass_FWD_DEFINED__
#define __UpdaterControlClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class UpdaterControlClass UpdaterControlClass;
#else
typedef struct UpdaterControlClass UpdaterControlClass;
#endif /* __cplusplus */

#endif 	/* __UpdaterControlClass_FWD_DEFINED__ */


#ifndef __IUpdaterControl_FWD_DEFINED__
#define __IUpdaterControl_FWD_DEFINED__
typedef interface IUpdaterControl IUpdaterControl;

#endif 	/* __IUpdaterControl_FWD_DEFINED__ */


#ifndef __IUpdaterControlCallback_FWD_DEFINED__
#define __IUpdaterControlCallback_FWD_DEFINED__
typedef interface IUpdaterControlCallback IUpdaterControlCallback;

#endif 	/* __IUpdaterControlCallback_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __IUpdaterControlCallback_INTERFACE_DEFINED__
#define __IUpdaterControlCallback_INTERFACE_DEFINED__

/* interface IUpdaterControlCallback */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IUpdaterControlCallback;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("D272C794-2ACE-4584-B993-3B90C622BE65")
    IUpdaterControlCallback : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Run( 
            /* [in] */ LONG result) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IUpdaterControlCallbackVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IUpdaterControlCallback * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IUpdaterControlCallback * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IUpdaterControlCallback * This);
        
        HRESULT ( STDMETHODCALLTYPE *Run )( 
            IUpdaterControlCallback * This,
            /* [in] */ LONG result);
        
        END_INTERFACE
    } IUpdaterControlCallbackVtbl;

    interface IUpdaterControlCallback
    {
        CONST_VTBL struct IUpdaterControlCallbackVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IUpdaterControlCallback_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IUpdaterControlCallback_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IUpdaterControlCallback_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IUpdaterControlCallback_Run(This,result)	\
    ( (This)->lpVtbl -> Run(This,result) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IUpdaterControlCallback_INTERFACE_DEFINED__ */


#ifndef __IUpdaterControl_INTERFACE_DEFINED__
#define __IUpdaterControl_INTERFACE_DEFINED__

/* interface IUpdaterControl */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IUpdaterControl;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("526DA036-9BD3-4697-865A-DA12D37DFFCA")
    IUpdaterControl : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Run( 
            /* [in] */ IUpdaterControlCallback *callback) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE InitializeUpdateService( 
            /* [in] */ IUpdaterControlCallback *callback) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IUpdaterControlVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IUpdaterControl * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IUpdaterControl * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IUpdaterControl * This);
        
        HRESULT ( STDMETHODCALLTYPE *Run )( 
            IUpdaterControl * This,
            /* [in] */ IUpdaterControlCallback *callback);
        
        HRESULT ( STDMETHODCALLTYPE *InitializeUpdateService )( 
            IUpdaterControl * This,
            /* [in] */ IUpdaterControlCallback *callback);
        
        END_INTERFACE
    } IUpdaterControlVtbl;

    interface IUpdaterControl
    {
        CONST_VTBL struct IUpdaterControlVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IUpdaterControl_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IUpdaterControl_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IUpdaterControl_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IUpdaterControl_Run(This,callback)	\
    ( (This)->lpVtbl -> Run(This,callback) ) 

#define IUpdaterControl_InitializeUpdateService(This,callback)	\
    ( (This)->lpVtbl -> InitializeUpdateService(This,callback) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IUpdaterControl_INTERFACE_DEFINED__ */



#ifndef __UpdaterInternalLib_LIBRARY_DEFINED__
#define __UpdaterInternalLib_LIBRARY_DEFINED__

/* library UpdaterInternalLib */
/* [helpstring][version][uuid] */ 




EXTERN_C const IID LIBID_UpdaterInternalLib;

EXTERN_C const CLSID CLSID_UpdaterControlClass;

#ifdef __cplusplus

class DECLSPEC_UUID("1F87FE2F-D6A9-4711-9D11-8187705F8457")
UpdaterControlClass;
#endif
#endif /* __UpdaterInternalLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


