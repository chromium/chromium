

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for gen/chrome/updater/app/server/win/updater_idl.idl:
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

#ifndef __updater_idl_h__
#define __updater_idl_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef DECLSPEC_XFGVIRT
#if _CONTROL_FLOW_GUARD_XFG
#define DECLSPEC_XFGVIRT(base, func) __declspec(xfg_virtual(base, func))
#else
#define DECLSPEC_XFGVIRT(base, func)
#endif
#endif

/* Forward Declarations */ 

#ifndef __IUpdateState_FWD_DEFINED__
#define __IUpdateState_FWD_DEFINED__
typedef interface IUpdateState IUpdateState;

#endif 	/* __IUpdateState_FWD_DEFINED__ */


#ifndef __IUpdaterRegisterAppCallback_FWD_DEFINED__
#define __IUpdaterRegisterAppCallback_FWD_DEFINED__
typedef interface IUpdaterRegisterAppCallback IUpdaterRegisterAppCallback;

#endif 	/* __IUpdaterRegisterAppCallback_FWD_DEFINED__ */


#ifndef __ICompleteStatus_FWD_DEFINED__
#define __ICompleteStatus_FWD_DEFINED__
typedef interface ICompleteStatus ICompleteStatus;

#endif 	/* __ICompleteStatus_FWD_DEFINED__ */


#ifndef __IUpdaterObserver_FWD_DEFINED__
#define __IUpdaterObserver_FWD_DEFINED__
typedef interface IUpdaterObserver IUpdaterObserver;

#endif 	/* __IUpdaterObserver_FWD_DEFINED__ */


#ifndef __IUpdaterCallback_FWD_DEFINED__
#define __IUpdaterCallback_FWD_DEFINED__
typedef interface IUpdaterCallback IUpdaterCallback;

#endif 	/* __IUpdaterCallback_FWD_DEFINED__ */


#ifndef __IUpdater_FWD_DEFINED__
#define __IUpdater_FWD_DEFINED__
typedef interface IUpdater IUpdater;

#endif 	/* __IUpdater_FWD_DEFINED__ */


#ifndef __UpdaterUserClass_FWD_DEFINED__
#define __UpdaterUserClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class UpdaterUserClass UpdaterUserClass;
#else
typedef struct UpdaterUserClass UpdaterUserClass;
#endif /* __cplusplus */

#endif 	/* __UpdaterUserClass_FWD_DEFINED__ */


#ifndef __UpdaterSystemClass_FWD_DEFINED__
#define __UpdaterSystemClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class UpdaterSystemClass UpdaterSystemClass;
#else
typedef struct UpdaterSystemClass UpdaterSystemClass;
#endif /* __cplusplus */

#endif 	/* __UpdaterSystemClass_FWD_DEFINED__ */


#ifndef __ICompleteStatus_FWD_DEFINED__
#define __ICompleteStatus_FWD_DEFINED__
typedef interface ICompleteStatus ICompleteStatus;

#endif 	/* __ICompleteStatus_FWD_DEFINED__ */


#ifndef __IUpdater_FWD_DEFINED__
#define __IUpdater_FWD_DEFINED__
typedef interface IUpdater IUpdater;

#endif 	/* __IUpdater_FWD_DEFINED__ */


#ifndef __IUpdaterObserver_FWD_DEFINED__
#define __IUpdaterObserver_FWD_DEFINED__
typedef interface IUpdaterObserver IUpdaterObserver;

#endif 	/* __IUpdaterObserver_FWD_DEFINED__ */


#ifndef __IUpdateState_FWD_DEFINED__
#define __IUpdateState_FWD_DEFINED__
typedef interface IUpdateState IUpdateState;

#endif 	/* __IUpdateState_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __IUpdateState_INTERFACE_DEFINED__
#define __IUpdateState_INTERFACE_DEFINED__

/* interface IUpdateState */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IUpdateState;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("46ACF70B-AC13-406D-B53B-B2C4BF091FF6")
    IUpdateState : public IUnknown
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_state( 
            /* [retval][out] */ LONG *__MIDL__IUpdateState0000) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_appId( 
            /* [retval][out] */ BSTR *__MIDL__IUpdateState0001) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nextVersion( 
            /* [retval][out] */ BSTR *__MIDL__IUpdateState0002) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_downloadedBytes( 
            /* [retval][out] */ LONGLONG *__MIDL__IUpdateState0003) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_totalBytes( 
            /* [retval][out] */ LONGLONG *__MIDL__IUpdateState0004) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_installProgress( 
            /* [retval][out] */ LONG *__MIDL__IUpdateState0005) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_errorCategory( 
            /* [retval][out] */ LONG *__MIDL__IUpdateState0006) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_errorCode( 
            /* [retval][out] */ LONG *__MIDL__IUpdateState0007) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_extraCode1( 
            /* [retval][out] */ LONG *__MIDL__IUpdateState0008) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_installerText( 
            /* [retval][out] */ BSTR *__MIDL__IUpdateState0009) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_installerCommandLine( 
            /* [retval][out] */ BSTR *__MIDL__IUpdateState0010) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IUpdateStateVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IUpdateState * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IUpdateState * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IUpdateState * This);
        
        DECLSPEC_XFGVIRT(IUpdateState, get_state)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_state )( 
            IUpdateState * This,
            /* [retval][out] */ LONG *__MIDL__IUpdateState0000);
        
        DECLSPEC_XFGVIRT(IUpdateState, get_appId)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_appId )( 
            IUpdateState * This,
            /* [retval][out] */ BSTR *__MIDL__IUpdateState0001);
        
        DECLSPEC_XFGVIRT(IUpdateState, get_nextVersion)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextVersion )( 
            IUpdateState * This,
            /* [retval][out] */ BSTR *__MIDL__IUpdateState0002);
        
        DECLSPEC_XFGVIRT(IUpdateState, get_downloadedBytes)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_downloadedBytes )( 
            IUpdateState * This,
            /* [retval][out] */ LONGLONG *__MIDL__IUpdateState0003);
        
        DECLSPEC_XFGVIRT(IUpdateState, get_totalBytes)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_totalBytes )( 
            IUpdateState * This,
            /* [retval][out] */ LONGLONG *__MIDL__IUpdateState0004);
        
        DECLSPEC_XFGVIRT(IUpdateState, get_installProgress)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_installProgress )( 
            IUpdateState * This,
            /* [retval][out] */ LONG *__MIDL__IUpdateState0005);
        
        DECLSPEC_XFGVIRT(IUpdateState, get_errorCategory)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_errorCategory )( 
            IUpdateState * This,
            /* [retval][out] */ LONG *__MIDL__IUpdateState0006);
        
        DECLSPEC_XFGVIRT(IUpdateState, get_errorCode)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_errorCode )( 
            IUpdateState * This,
            /* [retval][out] */ LONG *__MIDL__IUpdateState0007);
        
        DECLSPEC_XFGVIRT(IUpdateState, get_extraCode1)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_extraCode1 )( 
            IUpdateState * This,
            /* [retval][out] */ LONG *__MIDL__IUpdateState0008);
        
        DECLSPEC_XFGVIRT(IUpdateState, get_installerText)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_installerText )( 
            IUpdateState * This,
            /* [retval][out] */ BSTR *__MIDL__IUpdateState0009);
        
        DECLSPEC_XFGVIRT(IUpdateState, get_installerCommandLine)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_installerCommandLine )( 
            IUpdateState * This,
            /* [retval][out] */ BSTR *__MIDL__IUpdateState0010);
        
        END_INTERFACE
    } IUpdateStateVtbl;

    interface IUpdateState
    {
        CONST_VTBL struct IUpdateStateVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IUpdateState_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IUpdateState_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IUpdateState_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IUpdateState_get_state(This,__MIDL__IUpdateState0000)	\
    ( (This)->lpVtbl -> get_state(This,__MIDL__IUpdateState0000) ) 

#define IUpdateState_get_appId(This,__MIDL__IUpdateState0001)	\
    ( (This)->lpVtbl -> get_appId(This,__MIDL__IUpdateState0001) ) 

#define IUpdateState_get_nextVersion(This,__MIDL__IUpdateState0002)	\
    ( (This)->lpVtbl -> get_nextVersion(This,__MIDL__IUpdateState0002) ) 

#define IUpdateState_get_downloadedBytes(This,__MIDL__IUpdateState0003)	\
    ( (This)->lpVtbl -> get_downloadedBytes(This,__MIDL__IUpdateState0003) ) 

#define IUpdateState_get_totalBytes(This,__MIDL__IUpdateState0004)	\
    ( (This)->lpVtbl -> get_totalBytes(This,__MIDL__IUpdateState0004) ) 

#define IUpdateState_get_installProgress(This,__MIDL__IUpdateState0005)	\
    ( (This)->lpVtbl -> get_installProgress(This,__MIDL__IUpdateState0005) ) 

#define IUpdateState_get_errorCategory(This,__MIDL__IUpdateState0006)	\
    ( (This)->lpVtbl -> get_errorCategory(This,__MIDL__IUpdateState0006) ) 

#define IUpdateState_get_errorCode(This,__MIDL__IUpdateState0007)	\
    ( (This)->lpVtbl -> get_errorCode(This,__MIDL__IUpdateState0007) ) 

#define IUpdateState_get_extraCode1(This,__MIDL__IUpdateState0008)	\
    ( (This)->lpVtbl -> get_extraCode1(This,__MIDL__IUpdateState0008) ) 

#define IUpdateState_get_installerText(This,__MIDL__IUpdateState0009)	\
    ( (This)->lpVtbl -> get_installerText(This,__MIDL__IUpdateState0009) ) 

#define IUpdateState_get_installerCommandLine(This,__MIDL__IUpdateState0010)	\
    ( (This)->lpVtbl -> get_installerCommandLine(This,__MIDL__IUpdateState0010) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IUpdateState_INTERFACE_DEFINED__ */


#ifndef __IUpdaterRegisterAppCallback_INTERFACE_DEFINED__
#define __IUpdaterRegisterAppCallback_INTERFACE_DEFINED__

/* interface IUpdaterRegisterAppCallback */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IUpdaterRegisterAppCallback;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("3FDEC4CB-8501-4ECD-A4CF-BF70326218D0")
    IUpdaterRegisterAppCallback : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Run( 
            /* [in] */ LONG status_code) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IUpdaterRegisterAppCallbackVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IUpdaterRegisterAppCallback * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IUpdaterRegisterAppCallback * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IUpdaterRegisterAppCallback * This);
        
        DECLSPEC_XFGVIRT(IUpdaterRegisterAppCallback, Run)
        HRESULT ( STDMETHODCALLTYPE *Run )( 
            IUpdaterRegisterAppCallback * This,
            /* [in] */ LONG status_code);
        
        END_INTERFACE
    } IUpdaterRegisterAppCallbackVtbl;

    interface IUpdaterRegisterAppCallback
    {
        CONST_VTBL struct IUpdaterRegisterAppCallbackVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IUpdaterRegisterAppCallback_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IUpdaterRegisterAppCallback_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IUpdaterRegisterAppCallback_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IUpdaterRegisterAppCallback_Run(This,status_code)	\
    ( (This)->lpVtbl -> Run(This,status_code) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IUpdaterRegisterAppCallback_INTERFACE_DEFINED__ */


#ifndef __ICompleteStatus_INTERFACE_DEFINED__
#define __ICompleteStatus_INTERFACE_DEFINED__

/* interface ICompleteStatus */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_ICompleteStatus;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2FCD14AF-B645-4351-8359-E80A0E202A0B")
    ICompleteStatus : public IUnknown
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_statusCode( 
            /* [retval][out] */ LONG *__MIDL__ICompleteStatus0000) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_statusMessage( 
            /* [retval][out] */ BSTR *__MIDL__ICompleteStatus0001) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ICompleteStatusVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ICompleteStatus * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ICompleteStatus * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ICompleteStatus * This);
        
        DECLSPEC_XFGVIRT(ICompleteStatus, get_statusCode)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_statusCode )( 
            ICompleteStatus * This,
            /* [retval][out] */ LONG *__MIDL__ICompleteStatus0000);
        
        DECLSPEC_XFGVIRT(ICompleteStatus, get_statusMessage)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_statusMessage )( 
            ICompleteStatus * This,
            /* [retval][out] */ BSTR *__MIDL__ICompleteStatus0001);
        
        END_INTERFACE
    } ICompleteStatusVtbl;

    interface ICompleteStatus
    {
        CONST_VTBL struct ICompleteStatusVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ICompleteStatus_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ICompleteStatus_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ICompleteStatus_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ICompleteStatus_get_statusCode(This,__MIDL__ICompleteStatus0000)	\
    ( (This)->lpVtbl -> get_statusCode(This,__MIDL__ICompleteStatus0000) ) 

#define ICompleteStatus_get_statusMessage(This,__MIDL__ICompleteStatus0001)	\
    ( (This)->lpVtbl -> get_statusMessage(This,__MIDL__ICompleteStatus0001) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ICompleteStatus_INTERFACE_DEFINED__ */


#ifndef __IUpdaterObserver_INTERFACE_DEFINED__
#define __IUpdaterObserver_INTERFACE_DEFINED__

/* interface IUpdaterObserver */
/* [unique][helpstring][uuid][oleautomation][object] */ 


EXTERN_C const IID IID_IUpdaterObserver;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("7B416CFD-4216-4FD6-BD83-7C586054676E")
    IUpdaterObserver : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE OnStateChange( 
            /* [in] */ IUpdateState *update_state) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnComplete( 
            /* [in] */ ICompleteStatus *status) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IUpdaterObserverVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IUpdaterObserver * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IUpdaterObserver * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IUpdaterObserver * This);
        
        DECLSPEC_XFGVIRT(IUpdaterObserver, OnStateChange)
        HRESULT ( STDMETHODCALLTYPE *OnStateChange )( 
            IUpdaterObserver * This,
            /* [in] */ IUpdateState *update_state);
        
        DECLSPEC_XFGVIRT(IUpdaterObserver, OnComplete)
        HRESULT ( STDMETHODCALLTYPE *OnComplete )( 
            IUpdaterObserver * This,
            /* [in] */ ICompleteStatus *status);
        
        END_INTERFACE
    } IUpdaterObserverVtbl;

    interface IUpdaterObserver
    {
        CONST_VTBL struct IUpdaterObserverVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IUpdaterObserver_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IUpdaterObserver_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IUpdaterObserver_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IUpdaterObserver_OnStateChange(This,update_state)	\
    ( (This)->lpVtbl -> OnStateChange(This,update_state) ) 

#define IUpdaterObserver_OnComplete(This,status)	\
    ( (This)->lpVtbl -> OnComplete(This,status) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IUpdaterObserver_INTERFACE_DEFINED__ */


#ifndef __IUpdaterCallback_INTERFACE_DEFINED__
#define __IUpdaterCallback_INTERFACE_DEFINED__

/* interface IUpdaterCallback */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IUpdaterCallback;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8BAB6F84-AD67-4819-B846-CC890880FD3B")
    IUpdaterCallback : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Run( 
            /* [in] */ LONG result) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IUpdaterCallbackVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IUpdaterCallback * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IUpdaterCallback * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IUpdaterCallback * This);
        
        DECLSPEC_XFGVIRT(IUpdaterCallback, Run)
        HRESULT ( STDMETHODCALLTYPE *Run )( 
            IUpdaterCallback * This,
            /* [in] */ LONG result);
        
        END_INTERFACE
    } IUpdaterCallbackVtbl;

    interface IUpdaterCallback
    {
        CONST_VTBL struct IUpdaterCallbackVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IUpdaterCallback_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IUpdaterCallback_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IUpdaterCallback_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IUpdaterCallback_Run(This,result)	\
    ( (This)->lpVtbl -> Run(This,result) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IUpdaterCallback_INTERFACE_DEFINED__ */


#ifndef __IUpdater_INTERFACE_DEFINED__
#define __IUpdater_INTERFACE_DEFINED__

/* interface IUpdater */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IUpdater;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("63B8FFB1-5314-48C9-9C57-93EC8BC6184B")
    IUpdater : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetVersion( 
            /* [retval][out] */ BSTR *version) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE FetchPolicies( 
            /* [in] */ IUpdaterCallback *callback) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CheckForUpdate( 
            /* [string][in] */ const WCHAR *app_id) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE RegisterApp( 
            /* [string][in] */ const WCHAR *app_id,
            /* [string][in] */ const WCHAR *brand_code,
            /* [string][in] */ const WCHAR *brand_path,
            /* [string][in] */ const WCHAR *tag,
            /* [string][in] */ const WCHAR *version,
            /* [string][in] */ const WCHAR *existence_checker_path,
            /* [in] */ IUpdaterRegisterAppCallback *callback) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE RunPeriodicTasks( 
            /* [in] */ IUpdaterCallback *callback) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Update( 
            /* [string][in] */ const WCHAR *app_id,
            /* [string][in] */ const WCHAR *install_data_index,
            /* [in] */ LONG priority,
            /* [in] */ BOOL same_version_update_allowed,
            /* [in] */ IUpdaterObserver *observer) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE UpdateAll( 
            /* [in] */ IUpdaterObserver *observer) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Install( 
            /* [string][in] */ const WCHAR *app_id,
            /* [string][in] */ const WCHAR *brand_code,
            /* [string][in] */ const WCHAR *brand_path,
            /* [string][in] */ const WCHAR *tag,
            /* [string][in] */ const WCHAR *version,
            /* [string][in] */ const WCHAR *existence_checker_path,
            /* [string][in] */ const WCHAR *install_data_index,
            /* [in] */ LONG priority,
            /* [in] */ IUpdaterObserver *observer) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CancelInstalls( 
            /* [string][in] */ const WCHAR *app_id) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE RunInstaller( 
            /* [string][in] */ const WCHAR *app_id,
            /* [string][in] */ const WCHAR *installer_path,
            /* [string][in] */ const WCHAR *install_args,
            /* [string][in] */ const WCHAR *install_data,
            /* [string][in] */ const WCHAR *install_settings,
            /* [in] */ IUpdaterObserver *observer) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IUpdaterVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IUpdater * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IUpdater * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IUpdater * This);
        
        DECLSPEC_XFGVIRT(IUpdater, GetVersion)
        HRESULT ( STDMETHODCALLTYPE *GetVersion )( 
            IUpdater * This,
            /* [retval][out] */ BSTR *version);
        
        DECLSPEC_XFGVIRT(IUpdater, FetchPolicies)
        HRESULT ( STDMETHODCALLTYPE *FetchPolicies )( 
            IUpdater * This,
            /* [in] */ IUpdaterCallback *callback);
        
        DECLSPEC_XFGVIRT(IUpdater, CheckForUpdate)
        HRESULT ( STDMETHODCALLTYPE *CheckForUpdate )( 
            IUpdater * This,
            /* [string][in] */ const WCHAR *app_id);
        
        DECLSPEC_XFGVIRT(IUpdater, RegisterApp)
        HRESULT ( STDMETHODCALLTYPE *RegisterApp )( 
            IUpdater * This,
            /* [string][in] */ const WCHAR *app_id,
            /* [string][in] */ const WCHAR *brand_code,
            /* [string][in] */ const WCHAR *brand_path,
            /* [string][in] */ const WCHAR *tag,
            /* [string][in] */ const WCHAR *version,
            /* [string][in] */ const WCHAR *existence_checker_path,
            /* [in] */ IUpdaterRegisterAppCallback *callback);
        
        DECLSPEC_XFGVIRT(IUpdater, RunPeriodicTasks)
        HRESULT ( STDMETHODCALLTYPE *RunPeriodicTasks )( 
            IUpdater * This,
            /* [in] */ IUpdaterCallback *callback);
        
        DECLSPEC_XFGVIRT(IUpdater, Update)
        HRESULT ( STDMETHODCALLTYPE *Update )( 
            IUpdater * This,
            /* [string][in] */ const WCHAR *app_id,
            /* [string][in] */ const WCHAR *install_data_index,
            /* [in] */ LONG priority,
            /* [in] */ BOOL same_version_update_allowed,
            /* [in] */ IUpdaterObserver *observer);
        
        DECLSPEC_XFGVIRT(IUpdater, UpdateAll)
        HRESULT ( STDMETHODCALLTYPE *UpdateAll )( 
            IUpdater * This,
            /* [in] */ IUpdaterObserver *observer);
        
        DECLSPEC_XFGVIRT(IUpdater, Install)
        HRESULT ( STDMETHODCALLTYPE *Install )( 
            IUpdater * This,
            /* [string][in] */ const WCHAR *app_id,
            /* [string][in] */ const WCHAR *brand_code,
            /* [string][in] */ const WCHAR *brand_path,
            /* [string][in] */ const WCHAR *tag,
            /* [string][in] */ const WCHAR *version,
            /* [string][in] */ const WCHAR *existence_checker_path,
            /* [string][in] */ const WCHAR *install_data_index,
            /* [in] */ LONG priority,
            /* [in] */ IUpdaterObserver *observer);
        
        DECLSPEC_XFGVIRT(IUpdater, CancelInstalls)
        HRESULT ( STDMETHODCALLTYPE *CancelInstalls )( 
            IUpdater * This,
            /* [string][in] */ const WCHAR *app_id);
        
        DECLSPEC_XFGVIRT(IUpdater, RunInstaller)
        HRESULT ( STDMETHODCALLTYPE *RunInstaller )( 
            IUpdater * This,
            /* [string][in] */ const WCHAR *app_id,
            /* [string][in] */ const WCHAR *installer_path,
            /* [string][in] */ const WCHAR *install_args,
            /* [string][in] */ const WCHAR *install_data,
            /* [string][in] */ const WCHAR *install_settings,
            /* [in] */ IUpdaterObserver *observer);
        
        END_INTERFACE
    } IUpdaterVtbl;

    interface IUpdater
    {
        CONST_VTBL struct IUpdaterVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IUpdater_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IUpdater_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IUpdater_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IUpdater_GetVersion(This,version)	\
    ( (This)->lpVtbl -> GetVersion(This,version) ) 

#define IUpdater_FetchPolicies(This,callback)	\
    ( (This)->lpVtbl -> FetchPolicies(This,callback) ) 

#define IUpdater_CheckForUpdate(This,app_id)	\
    ( (This)->lpVtbl -> CheckForUpdate(This,app_id) ) 

#define IUpdater_RegisterApp(This,app_id,brand_code,brand_path,tag,version,existence_checker_path,callback)	\
    ( (This)->lpVtbl -> RegisterApp(This,app_id,brand_code,brand_path,tag,version,existence_checker_path,callback) ) 

#define IUpdater_RunPeriodicTasks(This,callback)	\
    ( (This)->lpVtbl -> RunPeriodicTasks(This,callback) ) 

#define IUpdater_Update(This,app_id,install_data_index,priority,same_version_update_allowed,observer)	\
    ( (This)->lpVtbl -> Update(This,app_id,install_data_index,priority,same_version_update_allowed,observer) ) 

#define IUpdater_UpdateAll(This,observer)	\
    ( (This)->lpVtbl -> UpdateAll(This,observer) ) 

#define IUpdater_Install(This,app_id,brand_code,brand_path,tag,version,existence_checker_path,install_data_index,priority,observer)	\
    ( (This)->lpVtbl -> Install(This,app_id,brand_code,brand_path,tag,version,existence_checker_path,install_data_index,priority,observer) ) 

#define IUpdater_CancelInstalls(This,app_id)	\
    ( (This)->lpVtbl -> CancelInstalls(This,app_id) ) 

#define IUpdater_RunInstaller(This,app_id,installer_path,install_args,install_data,install_settings,observer)	\
    ( (This)->lpVtbl -> RunInstaller(This,app_id,installer_path,install_args,install_data,install_settings,observer) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IUpdater_INTERFACE_DEFINED__ */



#ifndef __UpdaterLib_LIBRARY_DEFINED__
#define __UpdaterLib_LIBRARY_DEFINED__

/* library UpdaterLib */
/* [helpstring][version][uuid] */ 






EXTERN_C const IID LIBID_UpdaterLib;

EXTERN_C const CLSID CLSID_UpdaterUserClass;

#ifdef __cplusplus

class DECLSPEC_UUID("158428a4-6014-4978-83ba-9fad0dabe791")
UpdaterUserClass;
#endif

EXTERN_C const CLSID CLSID_UpdaterSystemClass;

#ifdef __cplusplus

class DECLSPEC_UUID("415FD747-D79E-42D7-93AC-1BA6E5FD4E93")
UpdaterSystemClass;
#endif
#endif /* __UpdaterLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

unsigned long             __RPC_USER  BSTR_UserSize(     unsigned long *, unsigned long            , BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserMarshal(  unsigned long *, unsigned char *, BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserUnmarshal(unsigned long *, unsigned char *, BSTR * ); 
void                      __RPC_USER  BSTR_UserFree(     unsigned long *, BSTR * ); 

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


