

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../google_update/google_update_idl.idl:
    Oicf, W1, Zp8, env=Win32 (32b run), target_arch=X86 8.xx.xxxx 
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

#ifndef __google_update_idl_h__
#define __google_update_idl_h__

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

#ifndef __IGoogleUpdate3_FWD_DEFINED__
#define __IGoogleUpdate3_FWD_DEFINED__
typedef interface IGoogleUpdate3 IGoogleUpdate3;

#endif 	/* __IGoogleUpdate3_FWD_DEFINED__ */


#ifndef __IAppBundle_FWD_DEFINED__
#define __IAppBundle_FWD_DEFINED__
typedef interface IAppBundle IAppBundle;

#endif 	/* __IAppBundle_FWD_DEFINED__ */


#ifndef __IApp_FWD_DEFINED__
#define __IApp_FWD_DEFINED__
typedef interface IApp IApp;

#endif 	/* __IApp_FWD_DEFINED__ */


#ifndef __IApp2_FWD_DEFINED__
#define __IApp2_FWD_DEFINED__
typedef interface IApp2 IApp2;

#endif 	/* __IApp2_FWD_DEFINED__ */


#ifndef __IAppCommand_FWD_DEFINED__
#define __IAppCommand_FWD_DEFINED__
typedef interface IAppCommand IAppCommand;

#endif 	/* __IAppCommand_FWD_DEFINED__ */


#ifndef __IAppCommand2_FWD_DEFINED__
#define __IAppCommand2_FWD_DEFINED__
typedef interface IAppCommand2 IAppCommand2;

#endif 	/* __IAppCommand2_FWD_DEFINED__ */


#ifndef __IAppVersion_FWD_DEFINED__
#define __IAppVersion_FWD_DEFINED__
typedef interface IAppVersion IAppVersion;

#endif 	/* __IAppVersion_FWD_DEFINED__ */


#ifndef __IPackage_FWD_DEFINED__
#define __IPackage_FWD_DEFINED__
typedef interface IPackage IPackage;

#endif 	/* __IPackage_FWD_DEFINED__ */


#ifndef __ICurrentState_FWD_DEFINED__
#define __ICurrentState_FWD_DEFINED__
typedef interface ICurrentState ICurrentState;

#endif 	/* __ICurrentState_FWD_DEFINED__ */


#ifndef __IRegistrationUpdateHook_FWD_DEFINED__
#define __IRegistrationUpdateHook_FWD_DEFINED__
typedef interface IRegistrationUpdateHook IRegistrationUpdateHook;

#endif 	/* __IRegistrationUpdateHook_FWD_DEFINED__ */


#ifndef __ICredentialDialog_FWD_DEFINED__
#define __ICredentialDialog_FWD_DEFINED__
typedef interface ICredentialDialog ICredentialDialog;

#endif 	/* __ICredentialDialog_FWD_DEFINED__ */


#ifndef __IPolicyStatus_FWD_DEFINED__
#define __IPolicyStatus_FWD_DEFINED__
typedef interface IPolicyStatus IPolicyStatus;

#endif 	/* __IPolicyStatus_FWD_DEFINED__ */


#ifndef __IPolicyStatusValue_FWD_DEFINED__
#define __IPolicyStatusValue_FWD_DEFINED__
typedef interface IPolicyStatusValue IPolicyStatusValue;

#endif 	/* __IPolicyStatusValue_FWD_DEFINED__ */


#ifndef __IPolicyStatus2_FWD_DEFINED__
#define __IPolicyStatus2_FWD_DEFINED__
typedef interface IPolicyStatus2 IPolicyStatus2;

#endif 	/* __IPolicyStatus2_FWD_DEFINED__ */


#ifndef __IGoogleUpdate3Web_FWD_DEFINED__
#define __IGoogleUpdate3Web_FWD_DEFINED__
typedef interface IGoogleUpdate3Web IGoogleUpdate3Web;

#endif 	/* __IGoogleUpdate3Web_FWD_DEFINED__ */


#ifndef __IGoogleUpdate3WebSecurity_FWD_DEFINED__
#define __IGoogleUpdate3WebSecurity_FWD_DEFINED__
typedef interface IGoogleUpdate3WebSecurity IGoogleUpdate3WebSecurity;

#endif 	/* __IGoogleUpdate3WebSecurity_FWD_DEFINED__ */


#ifndef __IAppBundleWeb_FWD_DEFINED__
#define __IAppBundleWeb_FWD_DEFINED__
typedef interface IAppBundleWeb IAppBundleWeb;

#endif 	/* __IAppBundleWeb_FWD_DEFINED__ */


#ifndef __IAppWeb_FWD_DEFINED__
#define __IAppWeb_FWD_DEFINED__
typedef interface IAppWeb IAppWeb;

#endif 	/* __IAppWeb_FWD_DEFINED__ */


#ifndef __IAppCommandWeb_FWD_DEFINED__
#define __IAppCommandWeb_FWD_DEFINED__
typedef interface IAppCommandWeb IAppCommandWeb;

#endif 	/* __IAppCommandWeb_FWD_DEFINED__ */


#ifndef __IAppVersionWeb_FWD_DEFINED__
#define __IAppVersionWeb_FWD_DEFINED__
typedef interface IAppVersionWeb IAppVersionWeb;

#endif 	/* __IAppVersionWeb_FWD_DEFINED__ */


#ifndef __ICoCreateAsyncStatus_FWD_DEFINED__
#define __ICoCreateAsyncStatus_FWD_DEFINED__
typedef interface ICoCreateAsyncStatus ICoCreateAsyncStatus;

#endif 	/* __ICoCreateAsyncStatus_FWD_DEFINED__ */


#ifndef __ICoCreateAsync_FWD_DEFINED__
#define __ICoCreateAsync_FWD_DEFINED__
typedef interface ICoCreateAsync ICoCreateAsync;

#endif 	/* __ICoCreateAsync_FWD_DEFINED__ */


#ifndef __IBrowserHttpRequest2_FWD_DEFINED__
#define __IBrowserHttpRequest2_FWD_DEFINED__
typedef interface IBrowserHttpRequest2 IBrowserHttpRequest2;

#endif 	/* __IBrowserHttpRequest2_FWD_DEFINED__ */


#ifndef __IProcessLauncher_FWD_DEFINED__
#define __IProcessLauncher_FWD_DEFINED__
typedef interface IProcessLauncher IProcessLauncher;

#endif 	/* __IProcessLauncher_FWD_DEFINED__ */


#ifndef __IProcessLauncher2_FWD_DEFINED__
#define __IProcessLauncher2_FWD_DEFINED__
typedef interface IProcessLauncher2 IProcessLauncher2;

#endif 	/* __IProcessLauncher2_FWD_DEFINED__ */


#ifndef __IProgressWndEvents_FWD_DEFINED__
#define __IProgressWndEvents_FWD_DEFINED__
typedef interface IProgressWndEvents IProgressWndEvents;

#endif 	/* __IProgressWndEvents_FWD_DEFINED__ */


#ifndef __IJobObserver_FWD_DEFINED__
#define __IJobObserver_FWD_DEFINED__
typedef interface IJobObserver IJobObserver;

#endif 	/* __IJobObserver_FWD_DEFINED__ */


#ifndef __IJobObserver2_FWD_DEFINED__
#define __IJobObserver2_FWD_DEFINED__
typedef interface IJobObserver2 IJobObserver2;

#endif 	/* __IJobObserver2_FWD_DEFINED__ */


#ifndef __IGoogleUpdate_FWD_DEFINED__
#define __IGoogleUpdate_FWD_DEFINED__
typedef interface IGoogleUpdate IGoogleUpdate;

#endif 	/* __IGoogleUpdate_FWD_DEFINED__ */


#ifndef __IGoogleUpdateCore_FWD_DEFINED__
#define __IGoogleUpdateCore_FWD_DEFINED__
typedef interface IGoogleUpdateCore IGoogleUpdateCore;

#endif 	/* __IGoogleUpdateCore_FWD_DEFINED__ */


#ifndef __IGoogleUpdate3_FWD_DEFINED__
#define __IGoogleUpdate3_FWD_DEFINED__
typedef interface IGoogleUpdate3 IGoogleUpdate3;

#endif 	/* __IGoogleUpdate3_FWD_DEFINED__ */


#ifndef __IAppBundle_FWD_DEFINED__
#define __IAppBundle_FWD_DEFINED__
typedef interface IAppBundle IAppBundle;

#endif 	/* __IAppBundle_FWD_DEFINED__ */


#ifndef __IApp_FWD_DEFINED__
#define __IApp_FWD_DEFINED__
typedef interface IApp IApp;

#endif 	/* __IApp_FWD_DEFINED__ */


#ifndef __IApp2_FWD_DEFINED__
#define __IApp2_FWD_DEFINED__
typedef interface IApp2 IApp2;

#endif 	/* __IApp2_FWD_DEFINED__ */


#ifndef __IAppCommand_FWD_DEFINED__
#define __IAppCommand_FWD_DEFINED__
typedef interface IAppCommand IAppCommand;

#endif 	/* __IAppCommand_FWD_DEFINED__ */


#ifndef __IAppCommand2_FWD_DEFINED__
#define __IAppCommand2_FWD_DEFINED__
typedef interface IAppCommand2 IAppCommand2;

#endif 	/* __IAppCommand2_FWD_DEFINED__ */


#ifndef __IAppVersion_FWD_DEFINED__
#define __IAppVersion_FWD_DEFINED__
typedef interface IAppVersion IAppVersion;

#endif 	/* __IAppVersion_FWD_DEFINED__ */


#ifndef __IPackage_FWD_DEFINED__
#define __IPackage_FWD_DEFINED__
typedef interface IPackage IPackage;

#endif 	/* __IPackage_FWD_DEFINED__ */


#ifndef __ICurrentState_FWD_DEFINED__
#define __ICurrentState_FWD_DEFINED__
typedef interface ICurrentState ICurrentState;

#endif 	/* __ICurrentState_FWD_DEFINED__ */


#ifndef __IPolicyStatus_FWD_DEFINED__
#define __IPolicyStatus_FWD_DEFINED__
typedef interface IPolicyStatus IPolicyStatus;

#endif 	/* __IPolicyStatus_FWD_DEFINED__ */


#ifndef __IPolicyStatus2_FWD_DEFINED__
#define __IPolicyStatus2_FWD_DEFINED__
typedef interface IPolicyStatus2 IPolicyStatus2;

#endif 	/* __IPolicyStatus2_FWD_DEFINED__ */


#ifndef __IPolicyStatusValue_FWD_DEFINED__
#define __IPolicyStatusValue_FWD_DEFINED__
typedef interface IPolicyStatusValue IPolicyStatusValue;

#endif 	/* __IPolicyStatusValue_FWD_DEFINED__ */


#ifndef __IGoogleUpdate3Web_FWD_DEFINED__
#define __IGoogleUpdate3Web_FWD_DEFINED__
typedef interface IGoogleUpdate3Web IGoogleUpdate3Web;

#endif 	/* __IGoogleUpdate3Web_FWD_DEFINED__ */


#ifndef __IAppBundleWeb_FWD_DEFINED__
#define __IAppBundleWeb_FWD_DEFINED__
typedef interface IAppBundleWeb IAppBundleWeb;

#endif 	/* __IAppBundleWeb_FWD_DEFINED__ */


#ifndef __IAppWeb_FWD_DEFINED__
#define __IAppWeb_FWD_DEFINED__
typedef interface IAppWeb IAppWeb;

#endif 	/* __IAppWeb_FWD_DEFINED__ */


#ifndef __IAppCommandWeb_FWD_DEFINED__
#define __IAppCommandWeb_FWD_DEFINED__
typedef interface IAppCommandWeb IAppCommandWeb;

#endif 	/* __IAppCommandWeb_FWD_DEFINED__ */


#ifndef __IAppVersionWeb_FWD_DEFINED__
#define __IAppVersionWeb_FWD_DEFINED__
typedef interface IAppVersionWeb IAppVersionWeb;

#endif 	/* __IAppVersionWeb_FWD_DEFINED__ */


#ifndef __ICoCreateAsyncStatus_FWD_DEFINED__
#define __ICoCreateAsyncStatus_FWD_DEFINED__
typedef interface ICoCreateAsyncStatus ICoCreateAsyncStatus;

#endif 	/* __ICoCreateAsyncStatus_FWD_DEFINED__ */


#ifndef __GoogleUpdate3UserClass_FWD_DEFINED__
#define __GoogleUpdate3UserClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class GoogleUpdate3UserClass GoogleUpdate3UserClass;
#else
typedef struct GoogleUpdate3UserClass GoogleUpdate3UserClass;
#endif /* __cplusplus */

#endif 	/* __GoogleUpdate3UserClass_FWD_DEFINED__ */


#ifndef __GoogleUpdate3ServiceClass_FWD_DEFINED__
#define __GoogleUpdate3ServiceClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class GoogleUpdate3ServiceClass GoogleUpdate3ServiceClass;
#else
typedef struct GoogleUpdate3ServiceClass GoogleUpdate3ServiceClass;
#endif /* __cplusplus */

#endif 	/* __GoogleUpdate3ServiceClass_FWD_DEFINED__ */


#ifndef __GoogleUpdate3WebUserClass_FWD_DEFINED__
#define __GoogleUpdate3WebUserClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class GoogleUpdate3WebUserClass GoogleUpdate3WebUserClass;
#else
typedef struct GoogleUpdate3WebUserClass GoogleUpdate3WebUserClass;
#endif /* __cplusplus */

#endif 	/* __GoogleUpdate3WebUserClass_FWD_DEFINED__ */


#ifndef __GoogleUpdate3WebMachineClass_FWD_DEFINED__
#define __GoogleUpdate3WebMachineClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class GoogleUpdate3WebMachineClass GoogleUpdate3WebMachineClass;
#else
typedef struct GoogleUpdate3WebMachineClass GoogleUpdate3WebMachineClass;
#endif /* __cplusplus */

#endif 	/* __GoogleUpdate3WebMachineClass_FWD_DEFINED__ */


#ifndef __GoogleUpdate3WebServiceClass_FWD_DEFINED__
#define __GoogleUpdate3WebServiceClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class GoogleUpdate3WebServiceClass GoogleUpdate3WebServiceClass;
#else
typedef struct GoogleUpdate3WebServiceClass GoogleUpdate3WebServiceClass;
#endif /* __cplusplus */

#endif 	/* __GoogleUpdate3WebServiceClass_FWD_DEFINED__ */


#ifndef __GoogleUpdate3WebMachineFallbackClass_FWD_DEFINED__
#define __GoogleUpdate3WebMachineFallbackClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class GoogleUpdate3WebMachineFallbackClass GoogleUpdate3WebMachineFallbackClass;
#else
typedef struct GoogleUpdate3WebMachineFallbackClass GoogleUpdate3WebMachineFallbackClass;
#endif /* __cplusplus */

#endif 	/* __GoogleUpdate3WebMachineFallbackClass_FWD_DEFINED__ */


#ifndef __CurrentStateUserClass_FWD_DEFINED__
#define __CurrentStateUserClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class CurrentStateUserClass CurrentStateUserClass;
#else
typedef struct CurrentStateUserClass CurrentStateUserClass;
#endif /* __cplusplus */

#endif 	/* __CurrentStateUserClass_FWD_DEFINED__ */


#ifndef __CurrentStateMachineClass_FWD_DEFINED__
#define __CurrentStateMachineClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class CurrentStateMachineClass CurrentStateMachineClass;
#else
typedef struct CurrentStateMachineClass CurrentStateMachineClass;
#endif /* __cplusplus */

#endif 	/* __CurrentStateMachineClass_FWD_DEFINED__ */


#ifndef __CoCreateAsyncClass_FWD_DEFINED__
#define __CoCreateAsyncClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class CoCreateAsyncClass CoCreateAsyncClass;
#else
typedef struct CoCreateAsyncClass CoCreateAsyncClass;
#endif /* __cplusplus */

#endif 	/* __CoCreateAsyncClass_FWD_DEFINED__ */


#ifndef __CredentialDialogUserClass_FWD_DEFINED__
#define __CredentialDialogUserClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class CredentialDialogUserClass CredentialDialogUserClass;
#else
typedef struct CredentialDialogUserClass CredentialDialogUserClass;
#endif /* __cplusplus */

#endif 	/* __CredentialDialogUserClass_FWD_DEFINED__ */


#ifndef __CredentialDialogMachineClass_FWD_DEFINED__
#define __CredentialDialogMachineClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class CredentialDialogMachineClass CredentialDialogMachineClass;
#else
typedef struct CredentialDialogMachineClass CredentialDialogMachineClass;
#endif /* __cplusplus */

#endif 	/* __CredentialDialogMachineClass_FWD_DEFINED__ */


#ifndef __PolicyStatusValueUserClass_FWD_DEFINED__
#define __PolicyStatusValueUserClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class PolicyStatusValueUserClass PolicyStatusValueUserClass;
#else
typedef struct PolicyStatusValueUserClass PolicyStatusValueUserClass;
#endif /* __cplusplus */

#endif 	/* __PolicyStatusValueUserClass_FWD_DEFINED__ */


#ifndef __PolicyStatusValueMachineClass_FWD_DEFINED__
#define __PolicyStatusValueMachineClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class PolicyStatusValueMachineClass PolicyStatusValueMachineClass;
#else
typedef struct PolicyStatusValueMachineClass PolicyStatusValueMachineClass;
#endif /* __cplusplus */

#endif 	/* __PolicyStatusValueMachineClass_FWD_DEFINED__ */


#ifndef __PolicyStatusUserClass_FWD_DEFINED__
#define __PolicyStatusUserClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class PolicyStatusUserClass PolicyStatusUserClass;
#else
typedef struct PolicyStatusUserClass PolicyStatusUserClass;
#endif /* __cplusplus */

#endif 	/* __PolicyStatusUserClass_FWD_DEFINED__ */


#ifndef __PolicyStatusMachineClass_FWD_DEFINED__
#define __PolicyStatusMachineClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class PolicyStatusMachineClass PolicyStatusMachineClass;
#else
typedef struct PolicyStatusMachineClass PolicyStatusMachineClass;
#endif /* __cplusplus */

#endif 	/* __PolicyStatusMachineClass_FWD_DEFINED__ */


#ifndef __PolicyStatusMachineServiceClass_FWD_DEFINED__
#define __PolicyStatusMachineServiceClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class PolicyStatusMachineServiceClass PolicyStatusMachineServiceClass;
#else
typedef struct PolicyStatusMachineServiceClass PolicyStatusMachineServiceClass;
#endif /* __cplusplus */

#endif 	/* __PolicyStatusMachineServiceClass_FWD_DEFINED__ */


#ifndef __PolicyStatusMachineFallbackClass_FWD_DEFINED__
#define __PolicyStatusMachineFallbackClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class PolicyStatusMachineFallbackClass PolicyStatusMachineFallbackClass;
#else
typedef struct PolicyStatusMachineFallbackClass PolicyStatusMachineFallbackClass;
#endif /* __cplusplus */

#endif 	/* __PolicyStatusMachineFallbackClass_FWD_DEFINED__ */


#ifndef __GoogleComProxyMachineClass_FWD_DEFINED__
#define __GoogleComProxyMachineClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class GoogleComProxyMachineClass GoogleComProxyMachineClass;
#else
typedef struct GoogleComProxyMachineClass GoogleComProxyMachineClass;
#endif /* __cplusplus */

#endif 	/* __GoogleComProxyMachineClass_FWD_DEFINED__ */


#ifndef __GoogleComProxyUserClass_FWD_DEFINED__
#define __GoogleComProxyUserClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class GoogleComProxyUserClass GoogleComProxyUserClass;
#else
typedef struct GoogleComProxyUserClass GoogleComProxyUserClass;
#endif /* __cplusplus */

#endif 	/* __GoogleComProxyUserClass_FWD_DEFINED__ */


#ifndef __ProcessLauncherClass_FWD_DEFINED__
#define __ProcessLauncherClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class ProcessLauncherClass ProcessLauncherClass;
#else
typedef struct ProcessLauncherClass ProcessLauncherClass;
#endif /* __cplusplus */

#endif 	/* __ProcessLauncherClass_FWD_DEFINED__ */


#ifndef __OnDemandUserAppsClass_FWD_DEFINED__
#define __OnDemandUserAppsClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class OnDemandUserAppsClass OnDemandUserAppsClass;
#else
typedef struct OnDemandUserAppsClass OnDemandUserAppsClass;
#endif /* __cplusplus */

#endif 	/* __OnDemandUserAppsClass_FWD_DEFINED__ */


#ifndef __OnDemandMachineAppsClass_FWD_DEFINED__
#define __OnDemandMachineAppsClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class OnDemandMachineAppsClass OnDemandMachineAppsClass;
#else
typedef struct OnDemandMachineAppsClass OnDemandMachineAppsClass;
#endif /* __cplusplus */

#endif 	/* __OnDemandMachineAppsClass_FWD_DEFINED__ */


#ifndef __OnDemandMachineAppsServiceClass_FWD_DEFINED__
#define __OnDemandMachineAppsServiceClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class OnDemandMachineAppsServiceClass OnDemandMachineAppsServiceClass;
#else
typedef struct OnDemandMachineAppsServiceClass OnDemandMachineAppsServiceClass;
#endif /* __cplusplus */

#endif 	/* __OnDemandMachineAppsServiceClass_FWD_DEFINED__ */


#ifndef __OnDemandMachineAppsFallbackClass_FWD_DEFINED__
#define __OnDemandMachineAppsFallbackClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class OnDemandMachineAppsFallbackClass OnDemandMachineAppsFallbackClass;
#else
typedef struct OnDemandMachineAppsFallbackClass OnDemandMachineAppsFallbackClass;
#endif /* __cplusplus */

#endif 	/* __OnDemandMachineAppsFallbackClass_FWD_DEFINED__ */


#ifndef __GoogleUpdateCoreClass_FWD_DEFINED__
#define __GoogleUpdateCoreClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class GoogleUpdateCoreClass GoogleUpdateCoreClass;
#else
typedef struct GoogleUpdateCoreClass GoogleUpdateCoreClass;
#endif /* __cplusplus */

#endif 	/* __GoogleUpdateCoreClass_FWD_DEFINED__ */


#ifndef __GoogleUpdateCoreMachineClass_FWD_DEFINED__
#define __GoogleUpdateCoreMachineClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class GoogleUpdateCoreMachineClass GoogleUpdateCoreMachineClass;
#else
typedef struct GoogleUpdateCoreMachineClass GoogleUpdateCoreMachineClass;
#endif /* __cplusplus */

#endif 	/* __GoogleUpdateCoreMachineClass_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_google_update_idl_0000_0000 */
/* [local] */ 

typedef 
enum BrowserType
    {
        BROWSER_UNKNOWN	= 0,
        BROWSER_DEFAULT	= 1,
        BROWSER_INTERNET_EXPLORER	= 2,
        BROWSER_FIREFOX	= 3,
        BROWSER_CHROME	= 4
    } 	BrowserType;

typedef 
enum CurrentState
    {
        STATE_INIT	= 1,
        STATE_WAITING_TO_CHECK_FOR_UPDATE	= 2,
        STATE_CHECKING_FOR_UPDATE	= 3,
        STATE_UPDATE_AVAILABLE	= 4,
        STATE_WAITING_TO_DOWNLOAD	= 5,
        STATE_RETRYING_DOWNLOAD	= 6,
        STATE_DOWNLOADING	= 7,
        STATE_DOWNLOAD_COMPLETE	= 8,
        STATE_EXTRACTING	= 9,
        STATE_APPLYING_DIFFERENTIAL_PATCH	= 10,
        STATE_READY_TO_INSTALL	= 11,
        STATE_WAITING_TO_INSTALL	= 12,
        STATE_INSTALLING	= 13,
        STATE_INSTALL_COMPLETE	= 14,
        STATE_PAUSED	= 15,
        STATE_NO_UPDATE	= 16,
        STATE_ERROR	= 17
    } 	CurrentState;

typedef 
enum InstallPriority
    {
        INSTALL_PRIORITY_LOW	= 0,
        INSTALL_PRIORITY_HIGH	= 10
    } 	InstallPriority;

typedef 
enum PostInstallAction
    {
        POST_INSTALL_ACTION_DEFAULT	= 0,
        POST_INSTALL_ACTION_EXIT_SILENTLY	= 1,
        POST_INSTALL_ACTION_LAUNCH_COMMAND	= 2,
        POST_INSTALL_ACTION_EXIT_SILENTLY_ON_LAUNCH_COMMAND	= 3,
        POST_INSTALL_ACTION_RESTART_BROWSER	= 4,
        POST_INSTALL_ACTION_RESTART_ALL_BROWSERS	= 5,
        POST_INSTALL_ACTION_REBOOT	= 6
    } 	PostInstallAction;


enum AppCommandStatus
    {
        COMMAND_STATUS_INIT	= 1,
        COMMAND_STATUS_RUNNING	= 2,
        COMMAND_STATUS_ERROR	= 3,
        COMMAND_STATUS_COMPLETE	= 4
    } ;


extern RPC_IF_HANDLE __MIDL_itf_google_update_idl_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_google_update_idl_0000_0000_v0_0_s_ifspec;

#ifndef __IGoogleUpdate3_INTERFACE_DEFINED__
#define __IGoogleUpdate3_INTERFACE_DEFINED__

/* interface IGoogleUpdate3 */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IGoogleUpdate3;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("6DB17455-4E85-46e7-9D23-E555E4B005AF")
    IGoogleUpdate3 : public IDispatch
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Count( 
            /* [retval][out] */ long *count) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Item( 
            /* [in] */ long index,
            /* [retval][out] */ IDispatch **bundle) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE createAppBundle( 
            /* [retval][out] */ IDispatch **app_bundle) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IGoogleUpdate3Vtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IGoogleUpdate3 * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IGoogleUpdate3 * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IGoogleUpdate3 * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IGoogleUpdate3 * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IGoogleUpdate3 * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IGoogleUpdate3 * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IGoogleUpdate3 * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IGoogleUpdate3, get_Count)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Count )( 
            IGoogleUpdate3 * This,
            /* [retval][out] */ long *count);
        
        DECLSPEC_XFGVIRT(IGoogleUpdate3, get_Item)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Item )( 
            IGoogleUpdate3 * This,
            /* [in] */ long index,
            /* [retval][out] */ IDispatch **bundle);
        
        DECLSPEC_XFGVIRT(IGoogleUpdate3, createAppBundle)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *createAppBundle )( 
            IGoogleUpdate3 * This,
            /* [retval][out] */ IDispatch **app_bundle);
        
        END_INTERFACE
    } IGoogleUpdate3Vtbl;

    interface IGoogleUpdate3
    {
        CONST_VTBL struct IGoogleUpdate3Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IGoogleUpdate3_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IGoogleUpdate3_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IGoogleUpdate3_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IGoogleUpdate3_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IGoogleUpdate3_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IGoogleUpdate3_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IGoogleUpdate3_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IGoogleUpdate3_get_Count(This,count)	\
    ( (This)->lpVtbl -> get_Count(This,count) ) 

#define IGoogleUpdate3_get_Item(This,index,bundle)	\
    ( (This)->lpVtbl -> get_Item(This,index,bundle) ) 

#define IGoogleUpdate3_createAppBundle(This,app_bundle)	\
    ( (This)->lpVtbl -> createAppBundle(This,app_bundle) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IGoogleUpdate3_INTERFACE_DEFINED__ */


#ifndef __IAppBundle_INTERFACE_DEFINED__
#define __IAppBundle_INTERFACE_DEFINED__

/* interface IAppBundle */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IAppBundle;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("fe908cdd-22bb-472a-9870-1a0390e42f36")
    IAppBundle : public IDispatch
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_displayName( 
            /* [retval][out] */ BSTR *__MIDL__IAppBundle0000) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_displayName( 
            /* [in] */ BSTR __MIDL__IAppBundle0001) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_displayLanguage( 
            /* [retval][out] */ BSTR *__MIDL__IAppBundle0002) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_displayLanguage( 
            /* [in] */ BSTR __MIDL__IAppBundle0003) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_installSource( 
            /* [retval][out] */ BSTR *__MIDL__IAppBundle0004) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_installSource( 
            /* [in] */ BSTR __MIDL__IAppBundle0005) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_originURL( 
            /* [retval][out] */ BSTR *__MIDL__IAppBundle0006) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_originURL( 
            /* [in] */ BSTR __MIDL__IAppBundle0007) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_offlineDirectory( 
            /* [retval][out] */ BSTR *offline_dir) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_offlineDirectory( 
            /* [in] */ BSTR offline_dir) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_sessionId( 
            /* [retval][out] */ BSTR *session_id) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_sessionId( 
            /* [in] */ BSTR session_id) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_sendPings( 
            /* [retval][out] */ VARIANT_BOOL *send_pings) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_sendPings( 
            /* [in] */ VARIANT_BOOL send_pings) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_priority( 
            /* [retval][out] */ long *priority) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_priority( 
            /* [in] */ long priority) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Count( 
            /* [retval][out] */ long *count) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Item( 
            /* [in] */ long index,
            /* [retval][out] */ IDispatch **app) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_altTokens( 
            /* [in] */ ULONG_PTR impersonation_token,
            /* [in] */ ULONG_PTR primary_token,
            /* [in] */ DWORD caller_proc_id) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_parentHWND( 
            /* [in] */ ULONG_PTR hwnd) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE initialize( void) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE createApp( 
            /* [in] */ BSTR app_id,
            /* [retval][out] */ IDispatch **app) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE createInstalledApp( 
            /* [in] */ BSTR app_id,
            /* [retval][out] */ IDispatch **app) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE createAllInstalledApps( void) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE checkForUpdate( void) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE download( void) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE install( void) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE updateAllApps( void) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE stop( void) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE pause( void) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE resume( void) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE isBusy( 
            /* [retval][out] */ VARIANT_BOOL *is_busy) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE downloadPackage( 
            /* [in] */ BSTR app_id,
            /* [in] */ BSTR package_name) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_currentState( 
            /* [retval][out] */ VARIANT *current_state) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IAppBundleVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IAppBundle * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IAppBundle * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IAppBundle * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IAppBundle * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IAppBundle * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IAppBundle * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IAppBundle * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IAppBundle, get_displayName)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_displayName )( 
            IAppBundle * This,
            /* [retval][out] */ BSTR *__MIDL__IAppBundle0000);
        
        DECLSPEC_XFGVIRT(IAppBundle, put_displayName)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_displayName )( 
            IAppBundle * This,
            /* [in] */ BSTR __MIDL__IAppBundle0001);
        
        DECLSPEC_XFGVIRT(IAppBundle, get_displayLanguage)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_displayLanguage )( 
            IAppBundle * This,
            /* [retval][out] */ BSTR *__MIDL__IAppBundle0002);
        
        DECLSPEC_XFGVIRT(IAppBundle, put_displayLanguage)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_displayLanguage )( 
            IAppBundle * This,
            /* [in] */ BSTR __MIDL__IAppBundle0003);
        
        DECLSPEC_XFGVIRT(IAppBundle, get_installSource)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_installSource )( 
            IAppBundle * This,
            /* [retval][out] */ BSTR *__MIDL__IAppBundle0004);
        
        DECLSPEC_XFGVIRT(IAppBundle, put_installSource)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_installSource )( 
            IAppBundle * This,
            /* [in] */ BSTR __MIDL__IAppBundle0005);
        
        DECLSPEC_XFGVIRT(IAppBundle, get_originURL)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_originURL )( 
            IAppBundle * This,
            /* [retval][out] */ BSTR *__MIDL__IAppBundle0006);
        
        DECLSPEC_XFGVIRT(IAppBundle, put_originURL)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_originURL )( 
            IAppBundle * This,
            /* [in] */ BSTR __MIDL__IAppBundle0007);
        
        DECLSPEC_XFGVIRT(IAppBundle, get_offlineDirectory)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_offlineDirectory )( 
            IAppBundle * This,
            /* [retval][out] */ BSTR *offline_dir);
        
        DECLSPEC_XFGVIRT(IAppBundle, put_offlineDirectory)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_offlineDirectory )( 
            IAppBundle * This,
            /* [in] */ BSTR offline_dir);
        
        DECLSPEC_XFGVIRT(IAppBundle, get_sessionId)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_sessionId )( 
            IAppBundle * This,
            /* [retval][out] */ BSTR *session_id);
        
        DECLSPEC_XFGVIRT(IAppBundle, put_sessionId)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_sessionId )( 
            IAppBundle * This,
            /* [in] */ BSTR session_id);
        
        DECLSPEC_XFGVIRT(IAppBundle, get_sendPings)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_sendPings )( 
            IAppBundle * This,
            /* [retval][out] */ VARIANT_BOOL *send_pings);
        
        DECLSPEC_XFGVIRT(IAppBundle, put_sendPings)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_sendPings )( 
            IAppBundle * This,
            /* [in] */ VARIANT_BOOL send_pings);
        
        DECLSPEC_XFGVIRT(IAppBundle, get_priority)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_priority )( 
            IAppBundle * This,
            /* [retval][out] */ long *priority);
        
        DECLSPEC_XFGVIRT(IAppBundle, put_priority)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_priority )( 
            IAppBundle * This,
            /* [in] */ long priority);
        
        DECLSPEC_XFGVIRT(IAppBundle, get_Count)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Count )( 
            IAppBundle * This,
            /* [retval][out] */ long *count);
        
        DECLSPEC_XFGVIRT(IAppBundle, get_Item)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Item )( 
            IAppBundle * This,
            /* [in] */ long index,
            /* [retval][out] */ IDispatch **app);
        
        DECLSPEC_XFGVIRT(IAppBundle, put_altTokens)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_altTokens )( 
            IAppBundle * This,
            /* [in] */ ULONG_PTR impersonation_token,
            /* [in] */ ULONG_PTR primary_token,
            /* [in] */ DWORD caller_proc_id);
        
        DECLSPEC_XFGVIRT(IAppBundle, put_parentHWND)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_parentHWND )( 
            IAppBundle * This,
            /* [in] */ ULONG_PTR hwnd);
        
        DECLSPEC_XFGVIRT(IAppBundle, initialize)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *initialize )( 
            IAppBundle * This);
        
        DECLSPEC_XFGVIRT(IAppBundle, createApp)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *createApp )( 
            IAppBundle * This,
            /* [in] */ BSTR app_id,
            /* [retval][out] */ IDispatch **app);
        
        DECLSPEC_XFGVIRT(IAppBundle, createInstalledApp)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *createInstalledApp )( 
            IAppBundle * This,
            /* [in] */ BSTR app_id,
            /* [retval][out] */ IDispatch **app);
        
        DECLSPEC_XFGVIRT(IAppBundle, createAllInstalledApps)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *createAllInstalledApps )( 
            IAppBundle * This);
        
        DECLSPEC_XFGVIRT(IAppBundle, checkForUpdate)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *checkForUpdate )( 
            IAppBundle * This);
        
        DECLSPEC_XFGVIRT(IAppBundle, download)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *download )( 
            IAppBundle * This);
        
        DECLSPEC_XFGVIRT(IAppBundle, install)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *install )( 
            IAppBundle * This);
        
        DECLSPEC_XFGVIRT(IAppBundle, updateAllApps)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *updateAllApps )( 
            IAppBundle * This);
        
        DECLSPEC_XFGVIRT(IAppBundle, stop)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *stop )( 
            IAppBundle * This);
        
        DECLSPEC_XFGVIRT(IAppBundle, pause)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *pause )( 
            IAppBundle * This);
        
        DECLSPEC_XFGVIRT(IAppBundle, resume)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *resume )( 
            IAppBundle * This);
        
        DECLSPEC_XFGVIRT(IAppBundle, isBusy)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *isBusy )( 
            IAppBundle * This,
            /* [retval][out] */ VARIANT_BOOL *is_busy);
        
        DECLSPEC_XFGVIRT(IAppBundle, downloadPackage)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *downloadPackage )( 
            IAppBundle * This,
            /* [in] */ BSTR app_id,
            /* [in] */ BSTR package_name);
        
        DECLSPEC_XFGVIRT(IAppBundle, get_currentState)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_currentState )( 
            IAppBundle * This,
            /* [retval][out] */ VARIANT *current_state);
        
        END_INTERFACE
    } IAppBundleVtbl;

    interface IAppBundle
    {
        CONST_VTBL struct IAppBundleVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IAppBundle_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IAppBundle_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IAppBundle_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IAppBundle_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IAppBundle_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IAppBundle_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IAppBundle_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IAppBundle_get_displayName(This,__MIDL__IAppBundle0000)	\
    ( (This)->lpVtbl -> get_displayName(This,__MIDL__IAppBundle0000) ) 

#define IAppBundle_put_displayName(This,__MIDL__IAppBundle0001)	\
    ( (This)->lpVtbl -> put_displayName(This,__MIDL__IAppBundle0001) ) 

#define IAppBundle_get_displayLanguage(This,__MIDL__IAppBundle0002)	\
    ( (This)->lpVtbl -> get_displayLanguage(This,__MIDL__IAppBundle0002) ) 

#define IAppBundle_put_displayLanguage(This,__MIDL__IAppBundle0003)	\
    ( (This)->lpVtbl -> put_displayLanguage(This,__MIDL__IAppBundle0003) ) 

#define IAppBundle_get_installSource(This,__MIDL__IAppBundle0004)	\
    ( (This)->lpVtbl -> get_installSource(This,__MIDL__IAppBundle0004) ) 

#define IAppBundle_put_installSource(This,__MIDL__IAppBundle0005)	\
    ( (This)->lpVtbl -> put_installSource(This,__MIDL__IAppBundle0005) ) 

#define IAppBundle_get_originURL(This,__MIDL__IAppBundle0006)	\
    ( (This)->lpVtbl -> get_originURL(This,__MIDL__IAppBundle0006) ) 

#define IAppBundle_put_originURL(This,__MIDL__IAppBundle0007)	\
    ( (This)->lpVtbl -> put_originURL(This,__MIDL__IAppBundle0007) ) 

#define IAppBundle_get_offlineDirectory(This,offline_dir)	\
    ( (This)->lpVtbl -> get_offlineDirectory(This,offline_dir) ) 

#define IAppBundle_put_offlineDirectory(This,offline_dir)	\
    ( (This)->lpVtbl -> put_offlineDirectory(This,offline_dir) ) 

#define IAppBundle_get_sessionId(This,session_id)	\
    ( (This)->lpVtbl -> get_sessionId(This,session_id) ) 

#define IAppBundle_put_sessionId(This,session_id)	\
    ( (This)->lpVtbl -> put_sessionId(This,session_id) ) 

#define IAppBundle_get_sendPings(This,send_pings)	\
    ( (This)->lpVtbl -> get_sendPings(This,send_pings) ) 

#define IAppBundle_put_sendPings(This,send_pings)	\
    ( (This)->lpVtbl -> put_sendPings(This,send_pings) ) 

#define IAppBundle_get_priority(This,priority)	\
    ( (This)->lpVtbl -> get_priority(This,priority) ) 

#define IAppBundle_put_priority(This,priority)	\
    ( (This)->lpVtbl -> put_priority(This,priority) ) 

#define IAppBundle_get_Count(This,count)	\
    ( (This)->lpVtbl -> get_Count(This,count) ) 

#define IAppBundle_get_Item(This,index,app)	\
    ( (This)->lpVtbl -> get_Item(This,index,app) ) 

#define IAppBundle_put_altTokens(This,impersonation_token,primary_token,caller_proc_id)	\
    ( (This)->lpVtbl -> put_altTokens(This,impersonation_token,primary_token,caller_proc_id) ) 

#define IAppBundle_put_parentHWND(This,hwnd)	\
    ( (This)->lpVtbl -> put_parentHWND(This,hwnd) ) 

#define IAppBundle_initialize(This)	\
    ( (This)->lpVtbl -> initialize(This) ) 

#define IAppBundle_createApp(This,app_id,app)	\
    ( (This)->lpVtbl -> createApp(This,app_id,app) ) 

#define IAppBundle_createInstalledApp(This,app_id,app)	\
    ( (This)->lpVtbl -> createInstalledApp(This,app_id,app) ) 

#define IAppBundle_createAllInstalledApps(This)	\
    ( (This)->lpVtbl -> createAllInstalledApps(This) ) 

#define IAppBundle_checkForUpdate(This)	\
    ( (This)->lpVtbl -> checkForUpdate(This) ) 

#define IAppBundle_download(This)	\
    ( (This)->lpVtbl -> download(This) ) 

#define IAppBundle_install(This)	\
    ( (This)->lpVtbl -> install(This) ) 

#define IAppBundle_updateAllApps(This)	\
    ( (This)->lpVtbl -> updateAllApps(This) ) 

#define IAppBundle_stop(This)	\
    ( (This)->lpVtbl -> stop(This) ) 

#define IAppBundle_pause(This)	\
    ( (This)->lpVtbl -> pause(This) ) 

#define IAppBundle_resume(This)	\
    ( (This)->lpVtbl -> resume(This) ) 

#define IAppBundle_isBusy(This,is_busy)	\
    ( (This)->lpVtbl -> isBusy(This,is_busy) ) 

#define IAppBundle_downloadPackage(This,app_id,package_name)	\
    ( (This)->lpVtbl -> downloadPackage(This,app_id,package_name) ) 

#define IAppBundle_get_currentState(This,current_state)	\
    ( (This)->lpVtbl -> get_currentState(This,current_state) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IAppBundle_INTERFACE_DEFINED__ */


#ifndef __IApp_INTERFACE_DEFINED__
#define __IApp_INTERFACE_DEFINED__

/* interface IApp */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IApp;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("76F7B787-A67C-4c73-82C7-31F5E3AABC5C")
    IApp : public IDispatch
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_currentVersion( 
            /* [retval][out] */ IDispatch **current) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_nextVersion( 
            /* [retval][out] */ IDispatch **next) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_appId( 
            /* [retval][out] */ BSTR *__MIDL__IApp0000) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_displayName( 
            /* [retval][out] */ BSTR *__MIDL__IApp0001) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_displayName( 
            /* [in] */ BSTR __MIDL__IApp0002) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_language( 
            /* [retval][out] */ BSTR *__MIDL__IApp0003) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_language( 
            /* [in] */ BSTR __MIDL__IApp0004) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_ap( 
            /* [retval][out] */ BSTR *__MIDL__IApp0005) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_ap( 
            /* [in] */ BSTR __MIDL__IApp0006) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_ttToken( 
            /* [retval][out] */ BSTR *__MIDL__IApp0007) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_ttToken( 
            /* [in] */ BSTR __MIDL__IApp0008) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_iid( 
            /* [retval][out] */ BSTR *__MIDL__IApp0009) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_iid( 
            /* [in] */ BSTR __MIDL__IApp0010) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_brandCode( 
            /* [retval][out] */ BSTR *__MIDL__IApp0011) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_brandCode( 
            /* [in] */ BSTR __MIDL__IApp0012) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_clientId( 
            /* [retval][out] */ BSTR *__MIDL__IApp0013) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_clientId( 
            /* [in] */ BSTR __MIDL__IApp0014) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_labels( 
            /* [retval][out] */ BSTR *__MIDL__IApp0015) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_labels( 
            /* [in] */ BSTR __MIDL__IApp0016) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_referralId( 
            /* [retval][out] */ BSTR *__MIDL__IApp0017) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_referralId( 
            /* [in] */ BSTR __MIDL__IApp0018) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_command( 
            /* [in] */ BSTR command_id,
            /* [retval][out] */ IDispatch **command) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_browserType( 
            /* [retval][out] */ UINT *__MIDL__IApp0019) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_browserType( 
            /* [in] */ UINT __MIDL__IApp0020) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_clientInstallData( 
            /* [retval][out] */ BSTR *__MIDL__IApp0021) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_clientInstallData( 
            /* [in] */ BSTR __MIDL__IApp0022) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_serverInstallDataIndex( 
            /* [retval][out] */ BSTR *__MIDL__IApp0023) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_serverInstallDataIndex( 
            /* [in] */ BSTR __MIDL__IApp0024) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_isEulaAccepted( 
            /* [retval][out] */ VARIANT_BOOL *__MIDL__IApp0025) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_isEulaAccepted( 
            /* [in] */ VARIANT_BOOL __MIDL__IApp0026) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_usageStatsEnable( 
            /* [retval][out] */ UINT *__MIDL__IApp0027) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_usageStatsEnable( 
            /* [in] */ UINT __MIDL__IApp0028) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_installTimeDiffSec( 
            /* [retval][out] */ UINT *__MIDL__IApp0029) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_currentState( 
            /* [retval][out] */ IDispatch **__MIDL__IApp0030) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IAppVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IApp * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IApp * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IApp * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IApp * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IApp * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IApp * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IApp * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IApp, get_currentVersion)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_currentVersion )( 
            IApp * This,
            /* [retval][out] */ IDispatch **current);
        
        DECLSPEC_XFGVIRT(IApp, get_nextVersion)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_nextVersion )( 
            IApp * This,
            /* [retval][out] */ IDispatch **next);
        
        DECLSPEC_XFGVIRT(IApp, get_appId)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_appId )( 
            IApp * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0000);
        
        DECLSPEC_XFGVIRT(IApp, get_displayName)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_displayName )( 
            IApp * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0001);
        
        DECLSPEC_XFGVIRT(IApp, put_displayName)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_displayName )( 
            IApp * This,
            /* [in] */ BSTR __MIDL__IApp0002);
        
        DECLSPEC_XFGVIRT(IApp, get_language)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_language )( 
            IApp * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0003);
        
        DECLSPEC_XFGVIRT(IApp, put_language)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_language )( 
            IApp * This,
            /* [in] */ BSTR __MIDL__IApp0004);
        
        DECLSPEC_XFGVIRT(IApp, get_ap)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_ap )( 
            IApp * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0005);
        
        DECLSPEC_XFGVIRT(IApp, put_ap)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_ap )( 
            IApp * This,
            /* [in] */ BSTR __MIDL__IApp0006);
        
        DECLSPEC_XFGVIRT(IApp, get_ttToken)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_ttToken )( 
            IApp * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0007);
        
        DECLSPEC_XFGVIRT(IApp, put_ttToken)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_ttToken )( 
            IApp * This,
            /* [in] */ BSTR __MIDL__IApp0008);
        
        DECLSPEC_XFGVIRT(IApp, get_iid)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_iid )( 
            IApp * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0009);
        
        DECLSPEC_XFGVIRT(IApp, put_iid)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_iid )( 
            IApp * This,
            /* [in] */ BSTR __MIDL__IApp0010);
        
        DECLSPEC_XFGVIRT(IApp, get_brandCode)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_brandCode )( 
            IApp * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0011);
        
        DECLSPEC_XFGVIRT(IApp, put_brandCode)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_brandCode )( 
            IApp * This,
            /* [in] */ BSTR __MIDL__IApp0012);
        
        DECLSPEC_XFGVIRT(IApp, get_clientId)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_clientId )( 
            IApp * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0013);
        
        DECLSPEC_XFGVIRT(IApp, put_clientId)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_clientId )( 
            IApp * This,
            /* [in] */ BSTR __MIDL__IApp0014);
        
        DECLSPEC_XFGVIRT(IApp, get_labels)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_labels )( 
            IApp * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0015);
        
        DECLSPEC_XFGVIRT(IApp, put_labels)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_labels )( 
            IApp * This,
            /* [in] */ BSTR __MIDL__IApp0016);
        
        DECLSPEC_XFGVIRT(IApp, get_referralId)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_referralId )( 
            IApp * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0017);
        
        DECLSPEC_XFGVIRT(IApp, put_referralId)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_referralId )( 
            IApp * This,
            /* [in] */ BSTR __MIDL__IApp0018);
        
        DECLSPEC_XFGVIRT(IApp, get_command)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_command )( 
            IApp * This,
            /* [in] */ BSTR command_id,
            /* [retval][out] */ IDispatch **command);
        
        DECLSPEC_XFGVIRT(IApp, get_browserType)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_browserType )( 
            IApp * This,
            /* [retval][out] */ UINT *__MIDL__IApp0019);
        
        DECLSPEC_XFGVIRT(IApp, put_browserType)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_browserType )( 
            IApp * This,
            /* [in] */ UINT __MIDL__IApp0020);
        
        DECLSPEC_XFGVIRT(IApp, get_clientInstallData)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_clientInstallData )( 
            IApp * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0021);
        
        DECLSPEC_XFGVIRT(IApp, put_clientInstallData)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_clientInstallData )( 
            IApp * This,
            /* [in] */ BSTR __MIDL__IApp0022);
        
        DECLSPEC_XFGVIRT(IApp, get_serverInstallDataIndex)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_serverInstallDataIndex )( 
            IApp * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0023);
        
        DECLSPEC_XFGVIRT(IApp, put_serverInstallDataIndex)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_serverInstallDataIndex )( 
            IApp * This,
            /* [in] */ BSTR __MIDL__IApp0024);
        
        DECLSPEC_XFGVIRT(IApp, get_isEulaAccepted)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_isEulaAccepted )( 
            IApp * This,
            /* [retval][out] */ VARIANT_BOOL *__MIDL__IApp0025);
        
        DECLSPEC_XFGVIRT(IApp, put_isEulaAccepted)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_isEulaAccepted )( 
            IApp * This,
            /* [in] */ VARIANT_BOOL __MIDL__IApp0026);
        
        DECLSPEC_XFGVIRT(IApp, get_usageStatsEnable)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_usageStatsEnable )( 
            IApp * This,
            /* [retval][out] */ UINT *__MIDL__IApp0027);
        
        DECLSPEC_XFGVIRT(IApp, put_usageStatsEnable)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_usageStatsEnable )( 
            IApp * This,
            /* [in] */ UINT __MIDL__IApp0028);
        
        DECLSPEC_XFGVIRT(IApp, get_installTimeDiffSec)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_installTimeDiffSec )( 
            IApp * This,
            /* [retval][out] */ UINT *__MIDL__IApp0029);
        
        DECLSPEC_XFGVIRT(IApp, get_currentState)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_currentState )( 
            IApp * This,
            /* [retval][out] */ IDispatch **__MIDL__IApp0030);
        
        END_INTERFACE
    } IAppVtbl;

    interface IApp
    {
        CONST_VTBL struct IAppVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IApp_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IApp_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IApp_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IApp_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IApp_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IApp_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IApp_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IApp_get_currentVersion(This,current)	\
    ( (This)->lpVtbl -> get_currentVersion(This,current) ) 

#define IApp_get_nextVersion(This,next)	\
    ( (This)->lpVtbl -> get_nextVersion(This,next) ) 

#define IApp_get_appId(This,__MIDL__IApp0000)	\
    ( (This)->lpVtbl -> get_appId(This,__MIDL__IApp0000) ) 

#define IApp_get_displayName(This,__MIDL__IApp0001)	\
    ( (This)->lpVtbl -> get_displayName(This,__MIDL__IApp0001) ) 

#define IApp_put_displayName(This,__MIDL__IApp0002)	\
    ( (This)->lpVtbl -> put_displayName(This,__MIDL__IApp0002) ) 

#define IApp_get_language(This,__MIDL__IApp0003)	\
    ( (This)->lpVtbl -> get_language(This,__MIDL__IApp0003) ) 

#define IApp_put_language(This,__MIDL__IApp0004)	\
    ( (This)->lpVtbl -> put_language(This,__MIDL__IApp0004) ) 

#define IApp_get_ap(This,__MIDL__IApp0005)	\
    ( (This)->lpVtbl -> get_ap(This,__MIDL__IApp0005) ) 

#define IApp_put_ap(This,__MIDL__IApp0006)	\
    ( (This)->lpVtbl -> put_ap(This,__MIDL__IApp0006) ) 

#define IApp_get_ttToken(This,__MIDL__IApp0007)	\
    ( (This)->lpVtbl -> get_ttToken(This,__MIDL__IApp0007) ) 

#define IApp_put_ttToken(This,__MIDL__IApp0008)	\
    ( (This)->lpVtbl -> put_ttToken(This,__MIDL__IApp0008) ) 

#define IApp_get_iid(This,__MIDL__IApp0009)	\
    ( (This)->lpVtbl -> get_iid(This,__MIDL__IApp0009) ) 

#define IApp_put_iid(This,__MIDL__IApp0010)	\
    ( (This)->lpVtbl -> put_iid(This,__MIDL__IApp0010) ) 

#define IApp_get_brandCode(This,__MIDL__IApp0011)	\
    ( (This)->lpVtbl -> get_brandCode(This,__MIDL__IApp0011) ) 

#define IApp_put_brandCode(This,__MIDL__IApp0012)	\
    ( (This)->lpVtbl -> put_brandCode(This,__MIDL__IApp0012) ) 

#define IApp_get_clientId(This,__MIDL__IApp0013)	\
    ( (This)->lpVtbl -> get_clientId(This,__MIDL__IApp0013) ) 

#define IApp_put_clientId(This,__MIDL__IApp0014)	\
    ( (This)->lpVtbl -> put_clientId(This,__MIDL__IApp0014) ) 

#define IApp_get_labels(This,__MIDL__IApp0015)	\
    ( (This)->lpVtbl -> get_labels(This,__MIDL__IApp0015) ) 

#define IApp_put_labels(This,__MIDL__IApp0016)	\
    ( (This)->lpVtbl -> put_labels(This,__MIDL__IApp0016) ) 

#define IApp_get_referralId(This,__MIDL__IApp0017)	\
    ( (This)->lpVtbl -> get_referralId(This,__MIDL__IApp0017) ) 

#define IApp_put_referralId(This,__MIDL__IApp0018)	\
    ( (This)->lpVtbl -> put_referralId(This,__MIDL__IApp0018) ) 

#define IApp_get_command(This,command_id,command)	\
    ( (This)->lpVtbl -> get_command(This,command_id,command) ) 

#define IApp_get_browserType(This,__MIDL__IApp0019)	\
    ( (This)->lpVtbl -> get_browserType(This,__MIDL__IApp0019) ) 

#define IApp_put_browserType(This,__MIDL__IApp0020)	\
    ( (This)->lpVtbl -> put_browserType(This,__MIDL__IApp0020) ) 

#define IApp_get_clientInstallData(This,__MIDL__IApp0021)	\
    ( (This)->lpVtbl -> get_clientInstallData(This,__MIDL__IApp0021) ) 

#define IApp_put_clientInstallData(This,__MIDL__IApp0022)	\
    ( (This)->lpVtbl -> put_clientInstallData(This,__MIDL__IApp0022) ) 

#define IApp_get_serverInstallDataIndex(This,__MIDL__IApp0023)	\
    ( (This)->lpVtbl -> get_serverInstallDataIndex(This,__MIDL__IApp0023) ) 

#define IApp_put_serverInstallDataIndex(This,__MIDL__IApp0024)	\
    ( (This)->lpVtbl -> put_serverInstallDataIndex(This,__MIDL__IApp0024) ) 

#define IApp_get_isEulaAccepted(This,__MIDL__IApp0025)	\
    ( (This)->lpVtbl -> get_isEulaAccepted(This,__MIDL__IApp0025) ) 

#define IApp_put_isEulaAccepted(This,__MIDL__IApp0026)	\
    ( (This)->lpVtbl -> put_isEulaAccepted(This,__MIDL__IApp0026) ) 

#define IApp_get_usageStatsEnable(This,__MIDL__IApp0027)	\
    ( (This)->lpVtbl -> get_usageStatsEnable(This,__MIDL__IApp0027) ) 

#define IApp_put_usageStatsEnable(This,__MIDL__IApp0028)	\
    ( (This)->lpVtbl -> put_usageStatsEnable(This,__MIDL__IApp0028) ) 

#define IApp_get_installTimeDiffSec(This,__MIDL__IApp0029)	\
    ( (This)->lpVtbl -> get_installTimeDiffSec(This,__MIDL__IApp0029) ) 

#define IApp_get_currentState(This,__MIDL__IApp0030)	\
    ( (This)->lpVtbl -> get_currentState(This,__MIDL__IApp0030) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IApp_INTERFACE_DEFINED__ */


#ifndef __IApp2_INTERFACE_DEFINED__
#define __IApp2_INTERFACE_DEFINED__

/* interface IApp2 */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IApp2;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("084D78A8-B084-4E14-A629-A2C419B0E3D9")
    IApp2 : public IApp
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_untrustedData( 
            /* [retval][out] */ BSTR *__MIDL__IApp20000) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_untrustedData( 
            /* [in] */ BSTR __MIDL__IApp20001) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IApp2Vtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IApp2 * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IApp2 * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IApp2 * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IApp2 * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IApp2 * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IApp2 * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IApp2 * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IApp, get_currentVersion)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_currentVersion )( 
            IApp2 * This,
            /* [retval][out] */ IDispatch **current);
        
        DECLSPEC_XFGVIRT(IApp, get_nextVersion)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_nextVersion )( 
            IApp2 * This,
            /* [retval][out] */ IDispatch **next);
        
        DECLSPEC_XFGVIRT(IApp, get_appId)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_appId )( 
            IApp2 * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0000);
        
        DECLSPEC_XFGVIRT(IApp, get_displayName)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_displayName )( 
            IApp2 * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0001);
        
        DECLSPEC_XFGVIRT(IApp, put_displayName)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_displayName )( 
            IApp2 * This,
            /* [in] */ BSTR __MIDL__IApp0002);
        
        DECLSPEC_XFGVIRT(IApp, get_language)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_language )( 
            IApp2 * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0003);
        
        DECLSPEC_XFGVIRT(IApp, put_language)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_language )( 
            IApp2 * This,
            /* [in] */ BSTR __MIDL__IApp0004);
        
        DECLSPEC_XFGVIRT(IApp, get_ap)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_ap )( 
            IApp2 * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0005);
        
        DECLSPEC_XFGVIRT(IApp, put_ap)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_ap )( 
            IApp2 * This,
            /* [in] */ BSTR __MIDL__IApp0006);
        
        DECLSPEC_XFGVIRT(IApp, get_ttToken)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_ttToken )( 
            IApp2 * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0007);
        
        DECLSPEC_XFGVIRT(IApp, put_ttToken)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_ttToken )( 
            IApp2 * This,
            /* [in] */ BSTR __MIDL__IApp0008);
        
        DECLSPEC_XFGVIRT(IApp, get_iid)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_iid )( 
            IApp2 * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0009);
        
        DECLSPEC_XFGVIRT(IApp, put_iid)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_iid )( 
            IApp2 * This,
            /* [in] */ BSTR __MIDL__IApp0010);
        
        DECLSPEC_XFGVIRT(IApp, get_brandCode)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_brandCode )( 
            IApp2 * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0011);
        
        DECLSPEC_XFGVIRT(IApp, put_brandCode)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_brandCode )( 
            IApp2 * This,
            /* [in] */ BSTR __MIDL__IApp0012);
        
        DECLSPEC_XFGVIRT(IApp, get_clientId)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_clientId )( 
            IApp2 * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0013);
        
        DECLSPEC_XFGVIRT(IApp, put_clientId)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_clientId )( 
            IApp2 * This,
            /* [in] */ BSTR __MIDL__IApp0014);
        
        DECLSPEC_XFGVIRT(IApp, get_labels)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_labels )( 
            IApp2 * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0015);
        
        DECLSPEC_XFGVIRT(IApp, put_labels)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_labels )( 
            IApp2 * This,
            /* [in] */ BSTR __MIDL__IApp0016);
        
        DECLSPEC_XFGVIRT(IApp, get_referralId)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_referralId )( 
            IApp2 * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0017);
        
        DECLSPEC_XFGVIRT(IApp, put_referralId)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_referralId )( 
            IApp2 * This,
            /* [in] */ BSTR __MIDL__IApp0018);
        
        DECLSPEC_XFGVIRT(IApp, get_command)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_command )( 
            IApp2 * This,
            /* [in] */ BSTR command_id,
            /* [retval][out] */ IDispatch **command);
        
        DECLSPEC_XFGVIRT(IApp, get_browserType)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_browserType )( 
            IApp2 * This,
            /* [retval][out] */ UINT *__MIDL__IApp0019);
        
        DECLSPEC_XFGVIRT(IApp, put_browserType)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_browserType )( 
            IApp2 * This,
            /* [in] */ UINT __MIDL__IApp0020);
        
        DECLSPEC_XFGVIRT(IApp, get_clientInstallData)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_clientInstallData )( 
            IApp2 * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0021);
        
        DECLSPEC_XFGVIRT(IApp, put_clientInstallData)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_clientInstallData )( 
            IApp2 * This,
            /* [in] */ BSTR __MIDL__IApp0022);
        
        DECLSPEC_XFGVIRT(IApp, get_serverInstallDataIndex)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_serverInstallDataIndex )( 
            IApp2 * This,
            /* [retval][out] */ BSTR *__MIDL__IApp0023);
        
        DECLSPEC_XFGVIRT(IApp, put_serverInstallDataIndex)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_serverInstallDataIndex )( 
            IApp2 * This,
            /* [in] */ BSTR __MIDL__IApp0024);
        
        DECLSPEC_XFGVIRT(IApp, get_isEulaAccepted)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_isEulaAccepted )( 
            IApp2 * This,
            /* [retval][out] */ VARIANT_BOOL *__MIDL__IApp0025);
        
        DECLSPEC_XFGVIRT(IApp, put_isEulaAccepted)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_isEulaAccepted )( 
            IApp2 * This,
            /* [in] */ VARIANT_BOOL __MIDL__IApp0026);
        
        DECLSPEC_XFGVIRT(IApp, get_usageStatsEnable)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_usageStatsEnable )( 
            IApp2 * This,
            /* [retval][out] */ UINT *__MIDL__IApp0027);
        
        DECLSPEC_XFGVIRT(IApp, put_usageStatsEnable)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_usageStatsEnable )( 
            IApp2 * This,
            /* [in] */ UINT __MIDL__IApp0028);
        
        DECLSPEC_XFGVIRT(IApp, get_installTimeDiffSec)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_installTimeDiffSec )( 
            IApp2 * This,
            /* [retval][out] */ UINT *__MIDL__IApp0029);
        
        DECLSPEC_XFGVIRT(IApp, get_currentState)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_currentState )( 
            IApp2 * This,
            /* [retval][out] */ IDispatch **__MIDL__IApp0030);
        
        DECLSPEC_XFGVIRT(IApp2, get_untrustedData)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_untrustedData )( 
            IApp2 * This,
            /* [retval][out] */ BSTR *__MIDL__IApp20000);
        
        DECLSPEC_XFGVIRT(IApp2, put_untrustedData)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_untrustedData )( 
            IApp2 * This,
            /* [in] */ BSTR __MIDL__IApp20001);
        
        END_INTERFACE
    } IApp2Vtbl;

    interface IApp2
    {
        CONST_VTBL struct IApp2Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IApp2_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IApp2_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IApp2_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IApp2_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IApp2_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IApp2_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IApp2_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IApp2_get_currentVersion(This,current)	\
    ( (This)->lpVtbl -> get_currentVersion(This,current) ) 

#define IApp2_get_nextVersion(This,next)	\
    ( (This)->lpVtbl -> get_nextVersion(This,next) ) 

#define IApp2_get_appId(This,__MIDL__IApp0000)	\
    ( (This)->lpVtbl -> get_appId(This,__MIDL__IApp0000) ) 

#define IApp2_get_displayName(This,__MIDL__IApp0001)	\
    ( (This)->lpVtbl -> get_displayName(This,__MIDL__IApp0001) ) 

#define IApp2_put_displayName(This,__MIDL__IApp0002)	\
    ( (This)->lpVtbl -> put_displayName(This,__MIDL__IApp0002) ) 

#define IApp2_get_language(This,__MIDL__IApp0003)	\
    ( (This)->lpVtbl -> get_language(This,__MIDL__IApp0003) ) 

#define IApp2_put_language(This,__MIDL__IApp0004)	\
    ( (This)->lpVtbl -> put_language(This,__MIDL__IApp0004) ) 

#define IApp2_get_ap(This,__MIDL__IApp0005)	\
    ( (This)->lpVtbl -> get_ap(This,__MIDL__IApp0005) ) 

#define IApp2_put_ap(This,__MIDL__IApp0006)	\
    ( (This)->lpVtbl -> put_ap(This,__MIDL__IApp0006) ) 

#define IApp2_get_ttToken(This,__MIDL__IApp0007)	\
    ( (This)->lpVtbl -> get_ttToken(This,__MIDL__IApp0007) ) 

#define IApp2_put_ttToken(This,__MIDL__IApp0008)	\
    ( (This)->lpVtbl -> put_ttToken(This,__MIDL__IApp0008) ) 

#define IApp2_get_iid(This,__MIDL__IApp0009)	\
    ( (This)->lpVtbl -> get_iid(This,__MIDL__IApp0009) ) 

#define IApp2_put_iid(This,__MIDL__IApp0010)	\
    ( (This)->lpVtbl -> put_iid(This,__MIDL__IApp0010) ) 

#define IApp2_get_brandCode(This,__MIDL__IApp0011)	\
    ( (This)->lpVtbl -> get_brandCode(This,__MIDL__IApp0011) ) 

#define IApp2_put_brandCode(This,__MIDL__IApp0012)	\
    ( (This)->lpVtbl -> put_brandCode(This,__MIDL__IApp0012) ) 

#define IApp2_get_clientId(This,__MIDL__IApp0013)	\
    ( (This)->lpVtbl -> get_clientId(This,__MIDL__IApp0013) ) 

#define IApp2_put_clientId(This,__MIDL__IApp0014)	\
    ( (This)->lpVtbl -> put_clientId(This,__MIDL__IApp0014) ) 

#define IApp2_get_labels(This,__MIDL__IApp0015)	\
    ( (This)->lpVtbl -> get_labels(This,__MIDL__IApp0015) ) 

#define IApp2_put_labels(This,__MIDL__IApp0016)	\
    ( (This)->lpVtbl -> put_labels(This,__MIDL__IApp0016) ) 

#define IApp2_get_referralId(This,__MIDL__IApp0017)	\
    ( (This)->lpVtbl -> get_referralId(This,__MIDL__IApp0017) ) 

#define IApp2_put_referralId(This,__MIDL__IApp0018)	\
    ( (This)->lpVtbl -> put_referralId(This,__MIDL__IApp0018) ) 

#define IApp2_get_command(This,command_id,command)	\
    ( (This)->lpVtbl -> get_command(This,command_id,command) ) 

#define IApp2_get_browserType(This,__MIDL__IApp0019)	\
    ( (This)->lpVtbl -> get_browserType(This,__MIDL__IApp0019) ) 

#define IApp2_put_browserType(This,__MIDL__IApp0020)	\
    ( (This)->lpVtbl -> put_browserType(This,__MIDL__IApp0020) ) 

#define IApp2_get_clientInstallData(This,__MIDL__IApp0021)	\
    ( (This)->lpVtbl -> get_clientInstallData(This,__MIDL__IApp0021) ) 

#define IApp2_put_clientInstallData(This,__MIDL__IApp0022)	\
    ( (This)->lpVtbl -> put_clientInstallData(This,__MIDL__IApp0022) ) 

#define IApp2_get_serverInstallDataIndex(This,__MIDL__IApp0023)	\
    ( (This)->lpVtbl -> get_serverInstallDataIndex(This,__MIDL__IApp0023) ) 

#define IApp2_put_serverInstallDataIndex(This,__MIDL__IApp0024)	\
    ( (This)->lpVtbl -> put_serverInstallDataIndex(This,__MIDL__IApp0024) ) 

#define IApp2_get_isEulaAccepted(This,__MIDL__IApp0025)	\
    ( (This)->lpVtbl -> get_isEulaAccepted(This,__MIDL__IApp0025) ) 

#define IApp2_put_isEulaAccepted(This,__MIDL__IApp0026)	\
    ( (This)->lpVtbl -> put_isEulaAccepted(This,__MIDL__IApp0026) ) 

#define IApp2_get_usageStatsEnable(This,__MIDL__IApp0027)	\
    ( (This)->lpVtbl -> get_usageStatsEnable(This,__MIDL__IApp0027) ) 

#define IApp2_put_usageStatsEnable(This,__MIDL__IApp0028)	\
    ( (This)->lpVtbl -> put_usageStatsEnable(This,__MIDL__IApp0028) ) 

#define IApp2_get_installTimeDiffSec(This,__MIDL__IApp0029)	\
    ( (This)->lpVtbl -> get_installTimeDiffSec(This,__MIDL__IApp0029) ) 

#define IApp2_get_currentState(This,__MIDL__IApp0030)	\
    ( (This)->lpVtbl -> get_currentState(This,__MIDL__IApp0030) ) 


#define IApp2_get_untrustedData(This,__MIDL__IApp20000)	\
    ( (This)->lpVtbl -> get_untrustedData(This,__MIDL__IApp20000) ) 

#define IApp2_put_untrustedData(This,__MIDL__IApp20001)	\
    ( (This)->lpVtbl -> put_untrustedData(This,__MIDL__IApp20001) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IApp2_INTERFACE_DEFINED__ */


#ifndef __IAppCommand_INTERFACE_DEFINED__
#define __IAppCommand_INTERFACE_DEFINED__

/* interface IAppCommand */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IAppCommand;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("4DE778FE-F195-4ee3-9DAB-FE446C239221")
    IAppCommand : public IDispatch
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_isWebAccessible( 
            /* [retval][out] */ VARIANT_BOOL *__MIDL__IAppCommand0000) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_status( 
            /* [retval][out] */ UINT *__MIDL__IAppCommand0001) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_exitCode( 
            /* [retval][out] */ DWORD *__MIDL__IAppCommand0002) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE execute( 
            /* [optional][in] */ VARIANT arg1,
            /* [optional][in] */ VARIANT arg2,
            /* [optional][in] */ VARIANT arg3,
            /* [optional][in] */ VARIANT arg4,
            /* [optional][in] */ VARIANT arg5,
            /* [optional][in] */ VARIANT arg6,
            /* [optional][in] */ VARIANT arg7,
            /* [optional][in] */ VARIANT arg8,
            /* [optional][in] */ VARIANT arg9) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IAppCommandVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IAppCommand * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IAppCommand * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IAppCommand * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IAppCommand * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IAppCommand * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IAppCommand * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IAppCommand * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IAppCommand, get_isWebAccessible)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_isWebAccessible )( 
            IAppCommand * This,
            /* [retval][out] */ VARIANT_BOOL *__MIDL__IAppCommand0000);
        
        DECLSPEC_XFGVIRT(IAppCommand, get_status)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_status )( 
            IAppCommand * This,
            /* [retval][out] */ UINT *__MIDL__IAppCommand0001);
        
        DECLSPEC_XFGVIRT(IAppCommand, get_exitCode)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_exitCode )( 
            IAppCommand * This,
            /* [retval][out] */ DWORD *__MIDL__IAppCommand0002);
        
        DECLSPEC_XFGVIRT(IAppCommand, execute)
        HRESULT ( STDMETHODCALLTYPE *execute )( 
            IAppCommand * This,
            /* [optional][in] */ VARIANT arg1,
            /* [optional][in] */ VARIANT arg2,
            /* [optional][in] */ VARIANT arg3,
            /* [optional][in] */ VARIANT arg4,
            /* [optional][in] */ VARIANT arg5,
            /* [optional][in] */ VARIANT arg6,
            /* [optional][in] */ VARIANT arg7,
            /* [optional][in] */ VARIANT arg8,
            /* [optional][in] */ VARIANT arg9);
        
        END_INTERFACE
    } IAppCommandVtbl;

    interface IAppCommand
    {
        CONST_VTBL struct IAppCommandVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IAppCommand_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IAppCommand_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IAppCommand_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IAppCommand_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IAppCommand_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IAppCommand_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IAppCommand_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IAppCommand_get_isWebAccessible(This,__MIDL__IAppCommand0000)	\
    ( (This)->lpVtbl -> get_isWebAccessible(This,__MIDL__IAppCommand0000) ) 

#define IAppCommand_get_status(This,__MIDL__IAppCommand0001)	\
    ( (This)->lpVtbl -> get_status(This,__MIDL__IAppCommand0001) ) 

#define IAppCommand_get_exitCode(This,__MIDL__IAppCommand0002)	\
    ( (This)->lpVtbl -> get_exitCode(This,__MIDL__IAppCommand0002) ) 

#define IAppCommand_execute(This,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)	\
    ( (This)->lpVtbl -> execute(This,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IAppCommand_INTERFACE_DEFINED__ */


#ifndef __IAppCommand2_INTERFACE_DEFINED__
#define __IAppCommand2_INTERFACE_DEFINED__

/* interface IAppCommand2 */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IAppCommand2;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("3D05F64F-71E3-48A5-BF6B-83315BC8AE1F")
    IAppCommand2 : public IAppCommand
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_output( 
            /* [retval][out] */ BSTR *__MIDL__IAppCommand20000) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IAppCommand2Vtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IAppCommand2 * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IAppCommand2 * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IAppCommand2 * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IAppCommand2 * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IAppCommand2 * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IAppCommand2 * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IAppCommand2 * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IAppCommand, get_isWebAccessible)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_isWebAccessible )( 
            IAppCommand2 * This,
            /* [retval][out] */ VARIANT_BOOL *__MIDL__IAppCommand0000);
        
        DECLSPEC_XFGVIRT(IAppCommand, get_status)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_status )( 
            IAppCommand2 * This,
            /* [retval][out] */ UINT *__MIDL__IAppCommand0001);
        
        DECLSPEC_XFGVIRT(IAppCommand, get_exitCode)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_exitCode )( 
            IAppCommand2 * This,
            /* [retval][out] */ DWORD *__MIDL__IAppCommand0002);
        
        DECLSPEC_XFGVIRT(IAppCommand, execute)
        HRESULT ( STDMETHODCALLTYPE *execute )( 
            IAppCommand2 * This,
            /* [optional][in] */ VARIANT arg1,
            /* [optional][in] */ VARIANT arg2,
            /* [optional][in] */ VARIANT arg3,
            /* [optional][in] */ VARIANT arg4,
            /* [optional][in] */ VARIANT arg5,
            /* [optional][in] */ VARIANT arg6,
            /* [optional][in] */ VARIANT arg7,
            /* [optional][in] */ VARIANT arg8,
            /* [optional][in] */ VARIANT arg9);
        
        DECLSPEC_XFGVIRT(IAppCommand2, get_output)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_output )( 
            IAppCommand2 * This,
            /* [retval][out] */ BSTR *__MIDL__IAppCommand20000);
        
        END_INTERFACE
    } IAppCommand2Vtbl;

    interface IAppCommand2
    {
        CONST_VTBL struct IAppCommand2Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IAppCommand2_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IAppCommand2_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IAppCommand2_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IAppCommand2_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IAppCommand2_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IAppCommand2_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IAppCommand2_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IAppCommand2_get_isWebAccessible(This,__MIDL__IAppCommand0000)	\
    ( (This)->lpVtbl -> get_isWebAccessible(This,__MIDL__IAppCommand0000) ) 

#define IAppCommand2_get_status(This,__MIDL__IAppCommand0001)	\
    ( (This)->lpVtbl -> get_status(This,__MIDL__IAppCommand0001) ) 

#define IAppCommand2_get_exitCode(This,__MIDL__IAppCommand0002)	\
    ( (This)->lpVtbl -> get_exitCode(This,__MIDL__IAppCommand0002) ) 

#define IAppCommand2_execute(This,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)	\
    ( (This)->lpVtbl -> execute(This,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9) ) 


#define IAppCommand2_get_output(This,__MIDL__IAppCommand20000)	\
    ( (This)->lpVtbl -> get_output(This,__MIDL__IAppCommand20000) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IAppCommand2_INTERFACE_DEFINED__ */


#ifndef __IAppVersion_INTERFACE_DEFINED__
#define __IAppVersion_INTERFACE_DEFINED__

/* interface IAppVersion */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IAppVersion;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("BCDCB538-01C0-46d1-A6A7-52F4D021C272")
    IAppVersion : public IDispatch
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_version( 
            /* [retval][out] */ BSTR *__MIDL__IAppVersion0000) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_packageCount( 
            /* [retval][out] */ long *count) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_package( 
            /* [in] */ long index,
            /* [retval][out] */ IDispatch **package) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IAppVersionVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IAppVersion * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IAppVersion * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IAppVersion * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IAppVersion * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IAppVersion * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IAppVersion * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IAppVersion * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IAppVersion, get_version)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_version )( 
            IAppVersion * This,
            /* [retval][out] */ BSTR *__MIDL__IAppVersion0000);
        
        DECLSPEC_XFGVIRT(IAppVersion, get_packageCount)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_packageCount )( 
            IAppVersion * This,
            /* [retval][out] */ long *count);
        
        DECLSPEC_XFGVIRT(IAppVersion, get_package)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_package )( 
            IAppVersion * This,
            /* [in] */ long index,
            /* [retval][out] */ IDispatch **package);
        
        END_INTERFACE
    } IAppVersionVtbl;

    interface IAppVersion
    {
        CONST_VTBL struct IAppVersionVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IAppVersion_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IAppVersion_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IAppVersion_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IAppVersion_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IAppVersion_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IAppVersion_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IAppVersion_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IAppVersion_get_version(This,__MIDL__IAppVersion0000)	\
    ( (This)->lpVtbl -> get_version(This,__MIDL__IAppVersion0000) ) 

#define IAppVersion_get_packageCount(This,count)	\
    ( (This)->lpVtbl -> get_packageCount(This,count) ) 

#define IAppVersion_get_package(This,index,package)	\
    ( (This)->lpVtbl -> get_package(This,index,package) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IAppVersion_INTERFACE_DEFINED__ */


#ifndef __IPackage_INTERFACE_DEFINED__
#define __IPackage_INTERFACE_DEFINED__

/* interface IPackage */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IPackage;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("DCAB8386-4F03-4dbd-A366-D90BC9F68DE6")
    IPackage : public IDispatch
    {
    public:
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE get( 
            /* [in] */ BSTR dir) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_isAvailable( 
            /* [retval][out] */ VARIANT_BOOL *__MIDL__IPackage0000) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_filename( 
            /* [retval][out] */ BSTR *__MIDL__IPackage0001) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IPackageVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IPackage * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IPackage * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IPackage * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IPackage * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IPackage * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IPackage * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IPackage * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IPackage, get)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *get )( 
            IPackage * This,
            /* [in] */ BSTR dir);
        
        DECLSPEC_XFGVIRT(IPackage, get_isAvailable)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_isAvailable )( 
            IPackage * This,
            /* [retval][out] */ VARIANT_BOOL *__MIDL__IPackage0000);
        
        DECLSPEC_XFGVIRT(IPackage, get_filename)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_filename )( 
            IPackage * This,
            /* [retval][out] */ BSTR *__MIDL__IPackage0001);
        
        END_INTERFACE
    } IPackageVtbl;

    interface IPackage
    {
        CONST_VTBL struct IPackageVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IPackage_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IPackage_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IPackage_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IPackage_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IPackage_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IPackage_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IPackage_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IPackage_get(This,dir)	\
    ( (This)->lpVtbl -> get(This,dir) ) 

#define IPackage_get_isAvailable(This,__MIDL__IPackage0000)	\
    ( (This)->lpVtbl -> get_isAvailable(This,__MIDL__IPackage0000) ) 

#define IPackage_get_filename(This,__MIDL__IPackage0001)	\
    ( (This)->lpVtbl -> get_filename(This,__MIDL__IPackage0001) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IPackage_INTERFACE_DEFINED__ */


#ifndef __ICurrentState_INTERFACE_DEFINED__
#define __ICurrentState_INTERFACE_DEFINED__

/* interface ICurrentState */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_ICurrentState;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("247954F9-9EDC-4E68-8CC3-150C2B89EADF")
    ICurrentState : public IDispatch
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_stateValue( 
            /* [retval][out] */ LONG *__MIDL__ICurrentState0000) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_availableVersion( 
            /* [retval][out] */ BSTR *__MIDL__ICurrentState0001) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_bytesDownloaded( 
            /* [retval][out] */ ULONG *__MIDL__ICurrentState0002) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_totalBytesToDownload( 
            /* [retval][out] */ ULONG *__MIDL__ICurrentState0003) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_downloadTimeRemainingMs( 
            /* [retval][out] */ LONG *__MIDL__ICurrentState0004) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nextRetryTime( 
            /* [retval][out] */ ULONGLONG *__MIDL__ICurrentState0005) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_installProgress( 
            /* [retval][out] */ LONG *__MIDL__ICurrentState0006) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_installTimeRemainingMs( 
            /* [retval][out] */ LONG *__MIDL__ICurrentState0007) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_isCanceled( 
            /* [retval][out] */ VARIANT_BOOL *is_canceled) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_errorCode( 
            /* [retval][out] */ LONG *__MIDL__ICurrentState0008) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_extraCode1( 
            /* [retval][out] */ LONG *__MIDL__ICurrentState0009) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_completionMessage( 
            /* [retval][out] */ BSTR *__MIDL__ICurrentState0010) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_installerResultCode( 
            /* [retval][out] */ LONG *__MIDL__ICurrentState0011) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_installerResultExtraCode1( 
            /* [retval][out] */ LONG *__MIDL__ICurrentState0012) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_postInstallLaunchCommandLine( 
            /* [retval][out] */ BSTR *__MIDL__ICurrentState0013) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_postInstallUrl( 
            /* [retval][out] */ BSTR *__MIDL__ICurrentState0014) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_postInstallAction( 
            /* [retval][out] */ LONG *__MIDL__ICurrentState0015) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ICurrentStateVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ICurrentState * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ICurrentState * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ICurrentState * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ICurrentState * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ICurrentState * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ICurrentState * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ICurrentState * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_stateValue)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_stateValue )( 
            ICurrentState * This,
            /* [retval][out] */ LONG *__MIDL__ICurrentState0000);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_availableVersion)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_availableVersion )( 
            ICurrentState * This,
            /* [retval][out] */ BSTR *__MIDL__ICurrentState0001);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_bytesDownloaded)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_bytesDownloaded )( 
            ICurrentState * This,
            /* [retval][out] */ ULONG *__MIDL__ICurrentState0002);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_totalBytesToDownload)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_totalBytesToDownload )( 
            ICurrentState * This,
            /* [retval][out] */ ULONG *__MIDL__ICurrentState0003);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_downloadTimeRemainingMs)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_downloadTimeRemainingMs )( 
            ICurrentState * This,
            /* [retval][out] */ LONG *__MIDL__ICurrentState0004);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_nextRetryTime)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextRetryTime )( 
            ICurrentState * This,
            /* [retval][out] */ ULONGLONG *__MIDL__ICurrentState0005);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_installProgress)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_installProgress )( 
            ICurrentState * This,
            /* [retval][out] */ LONG *__MIDL__ICurrentState0006);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_installTimeRemainingMs)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_installTimeRemainingMs )( 
            ICurrentState * This,
            /* [retval][out] */ LONG *__MIDL__ICurrentState0007);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_isCanceled)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_isCanceled )( 
            ICurrentState * This,
            /* [retval][out] */ VARIANT_BOOL *is_canceled);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_errorCode)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_errorCode )( 
            ICurrentState * This,
            /* [retval][out] */ LONG *__MIDL__ICurrentState0008);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_extraCode1)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_extraCode1 )( 
            ICurrentState * This,
            /* [retval][out] */ LONG *__MIDL__ICurrentState0009);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_completionMessage)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_completionMessage )( 
            ICurrentState * This,
            /* [retval][out] */ BSTR *__MIDL__ICurrentState0010);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_installerResultCode)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_installerResultCode )( 
            ICurrentState * This,
            /* [retval][out] */ LONG *__MIDL__ICurrentState0011);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_installerResultExtraCode1)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_installerResultExtraCode1 )( 
            ICurrentState * This,
            /* [retval][out] */ LONG *__MIDL__ICurrentState0012);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_postInstallLaunchCommandLine)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_postInstallLaunchCommandLine )( 
            ICurrentState * This,
            /* [retval][out] */ BSTR *__MIDL__ICurrentState0013);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_postInstallUrl)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_postInstallUrl )( 
            ICurrentState * This,
            /* [retval][out] */ BSTR *__MIDL__ICurrentState0014);
        
        DECLSPEC_XFGVIRT(ICurrentState, get_postInstallAction)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_postInstallAction )( 
            ICurrentState * This,
            /* [retval][out] */ LONG *__MIDL__ICurrentState0015);
        
        END_INTERFACE
    } ICurrentStateVtbl;

    interface ICurrentState
    {
        CONST_VTBL struct ICurrentStateVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ICurrentState_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ICurrentState_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ICurrentState_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ICurrentState_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define ICurrentState_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define ICurrentState_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define ICurrentState_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define ICurrentState_get_stateValue(This,__MIDL__ICurrentState0000)	\
    ( (This)->lpVtbl -> get_stateValue(This,__MIDL__ICurrentState0000) ) 

#define ICurrentState_get_availableVersion(This,__MIDL__ICurrentState0001)	\
    ( (This)->lpVtbl -> get_availableVersion(This,__MIDL__ICurrentState0001) ) 

#define ICurrentState_get_bytesDownloaded(This,__MIDL__ICurrentState0002)	\
    ( (This)->lpVtbl -> get_bytesDownloaded(This,__MIDL__ICurrentState0002) ) 

#define ICurrentState_get_totalBytesToDownload(This,__MIDL__ICurrentState0003)	\
    ( (This)->lpVtbl -> get_totalBytesToDownload(This,__MIDL__ICurrentState0003) ) 

#define ICurrentState_get_downloadTimeRemainingMs(This,__MIDL__ICurrentState0004)	\
    ( (This)->lpVtbl -> get_downloadTimeRemainingMs(This,__MIDL__ICurrentState0004) ) 

#define ICurrentState_get_nextRetryTime(This,__MIDL__ICurrentState0005)	\
    ( (This)->lpVtbl -> get_nextRetryTime(This,__MIDL__ICurrentState0005) ) 

#define ICurrentState_get_installProgress(This,__MIDL__ICurrentState0006)	\
    ( (This)->lpVtbl -> get_installProgress(This,__MIDL__ICurrentState0006) ) 

#define ICurrentState_get_installTimeRemainingMs(This,__MIDL__ICurrentState0007)	\
    ( (This)->lpVtbl -> get_installTimeRemainingMs(This,__MIDL__ICurrentState0007) ) 

#define ICurrentState_get_isCanceled(This,is_canceled)	\
    ( (This)->lpVtbl -> get_isCanceled(This,is_canceled) ) 

#define ICurrentState_get_errorCode(This,__MIDL__ICurrentState0008)	\
    ( (This)->lpVtbl -> get_errorCode(This,__MIDL__ICurrentState0008) ) 

#define ICurrentState_get_extraCode1(This,__MIDL__ICurrentState0009)	\
    ( (This)->lpVtbl -> get_extraCode1(This,__MIDL__ICurrentState0009) ) 

#define ICurrentState_get_completionMessage(This,__MIDL__ICurrentState0010)	\
    ( (This)->lpVtbl -> get_completionMessage(This,__MIDL__ICurrentState0010) ) 

#define ICurrentState_get_installerResultCode(This,__MIDL__ICurrentState0011)	\
    ( (This)->lpVtbl -> get_installerResultCode(This,__MIDL__ICurrentState0011) ) 

#define ICurrentState_get_installerResultExtraCode1(This,__MIDL__ICurrentState0012)	\
    ( (This)->lpVtbl -> get_installerResultExtraCode1(This,__MIDL__ICurrentState0012) ) 

#define ICurrentState_get_postInstallLaunchCommandLine(This,__MIDL__ICurrentState0013)	\
    ( (This)->lpVtbl -> get_postInstallLaunchCommandLine(This,__MIDL__ICurrentState0013) ) 

#define ICurrentState_get_postInstallUrl(This,__MIDL__ICurrentState0014)	\
    ( (This)->lpVtbl -> get_postInstallUrl(This,__MIDL__ICurrentState0014) ) 

#define ICurrentState_get_postInstallAction(This,__MIDL__ICurrentState0015)	\
    ( (This)->lpVtbl -> get_postInstallAction(This,__MIDL__ICurrentState0015) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ICurrentState_INTERFACE_DEFINED__ */


#ifndef __IRegistrationUpdateHook_INTERFACE_DEFINED__
#define __IRegistrationUpdateHook_INTERFACE_DEFINED__

/* interface IRegistrationUpdateHook */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IRegistrationUpdateHook;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("4E223325-C16B-4eeb-AEDC-19AA99A237FA")
    IRegistrationUpdateHook : public IDispatch
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE UpdateRegistry( 
            /* [in] */ BSTR app_id,
            /* [in] */ VARIANT_BOOL is_machine) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IRegistrationUpdateHookVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IRegistrationUpdateHook * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IRegistrationUpdateHook * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IRegistrationUpdateHook * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IRegistrationUpdateHook * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IRegistrationUpdateHook * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IRegistrationUpdateHook * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IRegistrationUpdateHook * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IRegistrationUpdateHook, UpdateRegistry)
        HRESULT ( STDMETHODCALLTYPE *UpdateRegistry )( 
            IRegistrationUpdateHook * This,
            /* [in] */ BSTR app_id,
            /* [in] */ VARIANT_BOOL is_machine);
        
        END_INTERFACE
    } IRegistrationUpdateHookVtbl;

    interface IRegistrationUpdateHook
    {
        CONST_VTBL struct IRegistrationUpdateHookVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IRegistrationUpdateHook_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IRegistrationUpdateHook_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IRegistrationUpdateHook_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IRegistrationUpdateHook_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IRegistrationUpdateHook_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IRegistrationUpdateHook_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IRegistrationUpdateHook_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IRegistrationUpdateHook_UpdateRegistry(This,app_id,is_machine)	\
    ( (This)->lpVtbl -> UpdateRegistry(This,app_id,is_machine) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IRegistrationUpdateHook_INTERFACE_DEFINED__ */


#ifndef __ICredentialDialog_INTERFACE_DEFINED__
#define __ICredentialDialog_INTERFACE_DEFINED__

/* interface ICredentialDialog */
/* [unique][helpstring][uuid][object] */ 


EXTERN_C const IID IID_ICredentialDialog;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("b3a47570-0a85-4aea-8270-529d47899603")
    ICredentialDialog : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE QueryUserForCredentials( 
            /* [in] */ ULONG_PTR owner_hwnd,
            /* [in] */ BSTR server,
            /* [in] */ BSTR message,
            /* [out] */ BSTR *username,
            /* [out] */ BSTR *password) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ICredentialDialogVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ICredentialDialog * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ICredentialDialog * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ICredentialDialog * This);
        
        DECLSPEC_XFGVIRT(ICredentialDialog, QueryUserForCredentials)
        HRESULT ( STDMETHODCALLTYPE *QueryUserForCredentials )( 
            ICredentialDialog * This,
            /* [in] */ ULONG_PTR owner_hwnd,
            /* [in] */ BSTR server,
            /* [in] */ BSTR message,
            /* [out] */ BSTR *username,
            /* [out] */ BSTR *password);
        
        END_INTERFACE
    } ICredentialDialogVtbl;

    interface ICredentialDialog
    {
        CONST_VTBL struct ICredentialDialogVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ICredentialDialog_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ICredentialDialog_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ICredentialDialog_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ICredentialDialog_QueryUserForCredentials(This,owner_hwnd,server,message,username,password)	\
    ( (This)->lpVtbl -> QueryUserForCredentials(This,owner_hwnd,server,message,username,password) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ICredentialDialog_INTERFACE_DEFINED__ */


#ifndef __IPolicyStatus_INTERFACE_DEFINED__
#define __IPolicyStatus_INTERFACE_DEFINED__

/* interface IPolicyStatus */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IPolicyStatus;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("F63F6F8B-ACD5-413C-A44B-0409136D26CB")
    IPolicyStatus : public IDispatch
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_lastCheckPeriodMinutes( 
            /* [retval][out] */ DWORD *minutes) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_updatesSuppressedTimes( 
            /* [out] */ DWORD *start_hour,
            /* [out] */ DWORD *start_min,
            /* [out] */ DWORD *duration_min,
            /* [out] */ VARIANT_BOOL *are_updates_suppressed) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_downloadPreferenceGroupPolicy( 
            /* [retval][out] */ BSTR *pref) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_packageCacheSizeLimitMBytes( 
            /* [retval][out] */ DWORD *limit) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_packageCacheExpirationTimeDays( 
            /* [retval][out] */ DWORD *days) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_effectivePolicyForAppInstalls( 
            /* [in] */ BSTR app_id,
            /* [retval][out] */ DWORD *policy) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_effectivePolicyForAppUpdates( 
            /* [in] */ BSTR app_id,
            /* [retval][out] */ DWORD *policy) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_targetVersionPrefix( 
            /* [in] */ BSTR app_id,
            /* [retval][out] */ BSTR *prefix) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_isRollbackToTargetVersionAllowed( 
            /* [in] */ BSTR app_id,
            /* [retval][out] */ VARIANT_BOOL *rollback_allowed) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IPolicyStatusVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IPolicyStatus * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IPolicyStatus * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IPolicyStatus * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IPolicyStatus * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IPolicyStatus * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IPolicyStatus * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IPolicyStatus * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IPolicyStatus, get_lastCheckPeriodMinutes)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastCheckPeriodMinutes )( 
            IPolicyStatus * This,
            /* [retval][out] */ DWORD *minutes);
        
        DECLSPEC_XFGVIRT(IPolicyStatus, get_updatesSuppressedTimes)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_updatesSuppressedTimes )( 
            IPolicyStatus * This,
            /* [out] */ DWORD *start_hour,
            /* [out] */ DWORD *start_min,
            /* [out] */ DWORD *duration_min,
            /* [out] */ VARIANT_BOOL *are_updates_suppressed);
        
        DECLSPEC_XFGVIRT(IPolicyStatus, get_downloadPreferenceGroupPolicy)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_downloadPreferenceGroupPolicy )( 
            IPolicyStatus * This,
            /* [retval][out] */ BSTR *pref);
        
        DECLSPEC_XFGVIRT(IPolicyStatus, get_packageCacheSizeLimitMBytes)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_packageCacheSizeLimitMBytes )( 
            IPolicyStatus * This,
            /* [retval][out] */ DWORD *limit);
        
        DECLSPEC_XFGVIRT(IPolicyStatus, get_packageCacheExpirationTimeDays)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_packageCacheExpirationTimeDays )( 
            IPolicyStatus * This,
            /* [retval][out] */ DWORD *days);
        
        DECLSPEC_XFGVIRT(IPolicyStatus, get_effectivePolicyForAppInstalls)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_effectivePolicyForAppInstalls )( 
            IPolicyStatus * This,
            /* [in] */ BSTR app_id,
            /* [retval][out] */ DWORD *policy);
        
        DECLSPEC_XFGVIRT(IPolicyStatus, get_effectivePolicyForAppUpdates)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_effectivePolicyForAppUpdates )( 
            IPolicyStatus * This,
            /* [in] */ BSTR app_id,
            /* [retval][out] */ DWORD *policy);
        
        DECLSPEC_XFGVIRT(IPolicyStatus, get_targetVersionPrefix)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_targetVersionPrefix )( 
            IPolicyStatus * This,
            /* [in] */ BSTR app_id,
            /* [retval][out] */ BSTR *prefix);
        
        DECLSPEC_XFGVIRT(IPolicyStatus, get_isRollbackToTargetVersionAllowed)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_isRollbackToTargetVersionAllowed )( 
            IPolicyStatus * This,
            /* [in] */ BSTR app_id,
            /* [retval][out] */ VARIANT_BOOL *rollback_allowed);
        
        END_INTERFACE
    } IPolicyStatusVtbl;

    interface IPolicyStatus
    {
        CONST_VTBL struct IPolicyStatusVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IPolicyStatus_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IPolicyStatus_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IPolicyStatus_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IPolicyStatus_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IPolicyStatus_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IPolicyStatus_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IPolicyStatus_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IPolicyStatus_get_lastCheckPeriodMinutes(This,minutes)	\
    ( (This)->lpVtbl -> get_lastCheckPeriodMinutes(This,minutes) ) 

#define IPolicyStatus_get_updatesSuppressedTimes(This,start_hour,start_min,duration_min,are_updates_suppressed)	\
    ( (This)->lpVtbl -> get_updatesSuppressedTimes(This,start_hour,start_min,duration_min,are_updates_suppressed) ) 

#define IPolicyStatus_get_downloadPreferenceGroupPolicy(This,pref)	\
    ( (This)->lpVtbl -> get_downloadPreferenceGroupPolicy(This,pref) ) 

#define IPolicyStatus_get_packageCacheSizeLimitMBytes(This,limit)	\
    ( (This)->lpVtbl -> get_packageCacheSizeLimitMBytes(This,limit) ) 

#define IPolicyStatus_get_packageCacheExpirationTimeDays(This,days)	\
    ( (This)->lpVtbl -> get_packageCacheExpirationTimeDays(This,days) ) 

#define IPolicyStatus_get_effectivePolicyForAppInstalls(This,app_id,policy)	\
    ( (This)->lpVtbl -> get_effectivePolicyForAppInstalls(This,app_id,policy) ) 

#define IPolicyStatus_get_effectivePolicyForAppUpdates(This,app_id,policy)	\
    ( (This)->lpVtbl -> get_effectivePolicyForAppUpdates(This,app_id,policy) ) 

#define IPolicyStatus_get_targetVersionPrefix(This,app_id,prefix)	\
    ( (This)->lpVtbl -> get_targetVersionPrefix(This,app_id,prefix) ) 

#define IPolicyStatus_get_isRollbackToTargetVersionAllowed(This,app_id,rollback_allowed)	\
    ( (This)->lpVtbl -> get_isRollbackToTargetVersionAllowed(This,app_id,rollback_allowed) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IPolicyStatus_INTERFACE_DEFINED__ */


#ifndef __IPolicyStatusValue_INTERFACE_DEFINED__
#define __IPolicyStatusValue_INTERFACE_DEFINED__

/* interface IPolicyStatusValue */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IPolicyStatusValue;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("27634814-8E41-4C35-8577-980134A96544")
    IPolicyStatusValue : public IDispatch
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_source( 
            /* [retval][out] */ BSTR *__MIDL__IPolicyStatusValue0000) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_value( 
            /* [retval][out] */ BSTR *__MIDL__IPolicyStatusValue0001) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_hasConflict( 
            /* [retval][out] */ VARIANT_BOOL *has_conflict) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_conflictSource( 
            /* [retval][out] */ BSTR *__MIDL__IPolicyStatusValue0002) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_conflictValue( 
            /* [retval][out] */ BSTR *__MIDL__IPolicyStatusValue0003) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IPolicyStatusValueVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IPolicyStatusValue * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IPolicyStatusValue * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IPolicyStatusValue * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IPolicyStatusValue * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IPolicyStatusValue * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IPolicyStatusValue * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IPolicyStatusValue * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IPolicyStatusValue, get_source)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_source )( 
            IPolicyStatusValue * This,
            /* [retval][out] */ BSTR *__MIDL__IPolicyStatusValue0000);
        
        DECLSPEC_XFGVIRT(IPolicyStatusValue, get_value)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_value )( 
            IPolicyStatusValue * This,
            /* [retval][out] */ BSTR *__MIDL__IPolicyStatusValue0001);
        
        DECLSPEC_XFGVIRT(IPolicyStatusValue, get_hasConflict)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_hasConflict )( 
            IPolicyStatusValue * This,
            /* [retval][out] */ VARIANT_BOOL *has_conflict);
        
        DECLSPEC_XFGVIRT(IPolicyStatusValue, get_conflictSource)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_conflictSource )( 
            IPolicyStatusValue * This,
            /* [retval][out] */ BSTR *__MIDL__IPolicyStatusValue0002);
        
        DECLSPEC_XFGVIRT(IPolicyStatusValue, get_conflictValue)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_conflictValue )( 
            IPolicyStatusValue * This,
            /* [retval][out] */ BSTR *__MIDL__IPolicyStatusValue0003);
        
        END_INTERFACE
    } IPolicyStatusValueVtbl;

    interface IPolicyStatusValue
    {
        CONST_VTBL struct IPolicyStatusValueVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IPolicyStatusValue_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IPolicyStatusValue_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IPolicyStatusValue_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IPolicyStatusValue_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IPolicyStatusValue_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IPolicyStatusValue_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IPolicyStatusValue_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IPolicyStatusValue_get_source(This,__MIDL__IPolicyStatusValue0000)	\
    ( (This)->lpVtbl -> get_source(This,__MIDL__IPolicyStatusValue0000) ) 

#define IPolicyStatusValue_get_value(This,__MIDL__IPolicyStatusValue0001)	\
    ( (This)->lpVtbl -> get_value(This,__MIDL__IPolicyStatusValue0001) ) 

#define IPolicyStatusValue_get_hasConflict(This,has_conflict)	\
    ( (This)->lpVtbl -> get_hasConflict(This,has_conflict) ) 

#define IPolicyStatusValue_get_conflictSource(This,__MIDL__IPolicyStatusValue0002)	\
    ( (This)->lpVtbl -> get_conflictSource(This,__MIDL__IPolicyStatusValue0002) ) 

#define IPolicyStatusValue_get_conflictValue(This,__MIDL__IPolicyStatusValue0003)	\
    ( (This)->lpVtbl -> get_conflictValue(This,__MIDL__IPolicyStatusValue0003) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IPolicyStatusValue_INTERFACE_DEFINED__ */


#ifndef __IPolicyStatus2_INTERFACE_DEFINED__
#define __IPolicyStatus2_INTERFACE_DEFINED__

/* interface IPolicyStatus2 */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IPolicyStatus2;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("34527502-D3DB-4205-A69B-789B27EE0414")
    IPolicyStatus2 : public IDispatch
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_updaterVersion( 
            /* [retval][out] */ BSTR *version) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_lastCheckedTime( 
            /* [retval][out] */ DATE *last_checked) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE refreshPolicies( void) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_lastCheckPeriodMinutes( 
            /* [retval][out] */ IPolicyStatusValue **value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_updatesSuppressedTimes( 
            /* [out] */ IPolicyStatusValue **value,
            VARIANT_BOOL *are_updates_suppressed) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_downloadPreferenceGroupPolicy( 
            /* [retval][out] */ IPolicyStatusValue **value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_packageCacheSizeLimitMBytes( 
            /* [retval][out] */ IPolicyStatusValue **value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_packageCacheExpirationTimeDays( 
            /* [retval][out] */ IPolicyStatusValue **value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_proxyMode( 
            /* [retval][out] */ IPolicyStatusValue **value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_proxyPacUrl( 
            /* [retval][out] */ IPolicyStatusValue **value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_proxyServer( 
            /* [retval][out] */ IPolicyStatusValue **value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_effectivePolicyForAppInstalls( 
            /* [in] */ BSTR app_id,
            /* [retval][out] */ IPolicyStatusValue **value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_effectivePolicyForAppUpdates( 
            /* [in] */ BSTR app_id,
            /* [retval][out] */ IPolicyStatusValue **value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_targetVersionPrefix( 
            /* [in] */ BSTR app_id,
            /* [retval][out] */ IPolicyStatusValue **value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_isRollbackToTargetVersionAllowed( 
            /* [in] */ BSTR app_id,
            /* [retval][out] */ IPolicyStatusValue **value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_targetChannel( 
            /* [in] */ BSTR app_id,
            /* [retval][out] */ IPolicyStatusValue **value) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IPolicyStatus2Vtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IPolicyStatus2 * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IPolicyStatus2 * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IPolicyStatus2 * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IPolicyStatus2 * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IPolicyStatus2 * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IPolicyStatus2 * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IPolicyStatus2 * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, get_updaterVersion)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_updaterVersion )( 
            IPolicyStatus2 * This,
            /* [retval][out] */ BSTR *version);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, get_lastCheckedTime)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastCheckedTime )( 
            IPolicyStatus2 * This,
            /* [retval][out] */ DATE *last_checked);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, refreshPolicies)
        HRESULT ( STDMETHODCALLTYPE *refreshPolicies )( 
            IPolicyStatus2 * This);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, get_lastCheckPeriodMinutes)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastCheckPeriodMinutes )( 
            IPolicyStatus2 * This,
            /* [retval][out] */ IPolicyStatusValue **value);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, get_updatesSuppressedTimes)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_updatesSuppressedTimes )( 
            IPolicyStatus2 * This,
            /* [out] */ IPolicyStatusValue **value,
            VARIANT_BOOL *are_updates_suppressed);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, get_downloadPreferenceGroupPolicy)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_downloadPreferenceGroupPolicy )( 
            IPolicyStatus2 * This,
            /* [retval][out] */ IPolicyStatusValue **value);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, get_packageCacheSizeLimitMBytes)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_packageCacheSizeLimitMBytes )( 
            IPolicyStatus2 * This,
            /* [retval][out] */ IPolicyStatusValue **value);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, get_packageCacheExpirationTimeDays)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_packageCacheExpirationTimeDays )( 
            IPolicyStatus2 * This,
            /* [retval][out] */ IPolicyStatusValue **value);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, get_proxyMode)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_proxyMode )( 
            IPolicyStatus2 * This,
            /* [retval][out] */ IPolicyStatusValue **value);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, get_proxyPacUrl)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_proxyPacUrl )( 
            IPolicyStatus2 * This,
            /* [retval][out] */ IPolicyStatusValue **value);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, get_proxyServer)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_proxyServer )( 
            IPolicyStatus2 * This,
            /* [retval][out] */ IPolicyStatusValue **value);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, get_effectivePolicyForAppInstalls)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_effectivePolicyForAppInstalls )( 
            IPolicyStatus2 * This,
            /* [in] */ BSTR app_id,
            /* [retval][out] */ IPolicyStatusValue **value);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, get_effectivePolicyForAppUpdates)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_effectivePolicyForAppUpdates )( 
            IPolicyStatus2 * This,
            /* [in] */ BSTR app_id,
            /* [retval][out] */ IPolicyStatusValue **value);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, get_targetVersionPrefix)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_targetVersionPrefix )( 
            IPolicyStatus2 * This,
            /* [in] */ BSTR app_id,
            /* [retval][out] */ IPolicyStatusValue **value);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, get_isRollbackToTargetVersionAllowed)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_isRollbackToTargetVersionAllowed )( 
            IPolicyStatus2 * This,
            /* [in] */ BSTR app_id,
            /* [retval][out] */ IPolicyStatusValue **value);
        
        DECLSPEC_XFGVIRT(IPolicyStatus2, get_targetChannel)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_targetChannel )( 
            IPolicyStatus2 * This,
            /* [in] */ BSTR app_id,
            /* [retval][out] */ IPolicyStatusValue **value);
        
        END_INTERFACE
    } IPolicyStatus2Vtbl;

    interface IPolicyStatus2
    {
        CONST_VTBL struct IPolicyStatus2Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IPolicyStatus2_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IPolicyStatus2_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IPolicyStatus2_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IPolicyStatus2_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IPolicyStatus2_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IPolicyStatus2_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IPolicyStatus2_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IPolicyStatus2_get_updaterVersion(This,version)	\
    ( (This)->lpVtbl -> get_updaterVersion(This,version) ) 

#define IPolicyStatus2_get_lastCheckedTime(This,last_checked)	\
    ( (This)->lpVtbl -> get_lastCheckedTime(This,last_checked) ) 

#define IPolicyStatus2_refreshPolicies(This)	\
    ( (This)->lpVtbl -> refreshPolicies(This) ) 

#define IPolicyStatus2_get_lastCheckPeriodMinutes(This,value)	\
    ( (This)->lpVtbl -> get_lastCheckPeriodMinutes(This,value) ) 

#define IPolicyStatus2_get_updatesSuppressedTimes(This,value,are_updates_suppressed)	\
    ( (This)->lpVtbl -> get_updatesSuppressedTimes(This,value,are_updates_suppressed) ) 

#define IPolicyStatus2_get_downloadPreferenceGroupPolicy(This,value)	\
    ( (This)->lpVtbl -> get_downloadPreferenceGroupPolicy(This,value) ) 

#define IPolicyStatus2_get_packageCacheSizeLimitMBytes(This,value)	\
    ( (This)->lpVtbl -> get_packageCacheSizeLimitMBytes(This,value) ) 

#define IPolicyStatus2_get_packageCacheExpirationTimeDays(This,value)	\
    ( (This)->lpVtbl -> get_packageCacheExpirationTimeDays(This,value) ) 

#define IPolicyStatus2_get_proxyMode(This,value)	\
    ( (This)->lpVtbl -> get_proxyMode(This,value) ) 

#define IPolicyStatus2_get_proxyPacUrl(This,value)	\
    ( (This)->lpVtbl -> get_proxyPacUrl(This,value) ) 

#define IPolicyStatus2_get_proxyServer(This,value)	\
    ( (This)->lpVtbl -> get_proxyServer(This,value) ) 

#define IPolicyStatus2_get_effectivePolicyForAppInstalls(This,app_id,value)	\
    ( (This)->lpVtbl -> get_effectivePolicyForAppInstalls(This,app_id,value) ) 

#define IPolicyStatus2_get_effectivePolicyForAppUpdates(This,app_id,value)	\
    ( (This)->lpVtbl -> get_effectivePolicyForAppUpdates(This,app_id,value) ) 

#define IPolicyStatus2_get_targetVersionPrefix(This,app_id,value)	\
    ( (This)->lpVtbl -> get_targetVersionPrefix(This,app_id,value) ) 

#define IPolicyStatus2_get_isRollbackToTargetVersionAllowed(This,app_id,value)	\
    ( (This)->lpVtbl -> get_isRollbackToTargetVersionAllowed(This,app_id,value) ) 

#define IPolicyStatus2_get_targetChannel(This,app_id,value)	\
    ( (This)->lpVtbl -> get_targetChannel(This,app_id,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IPolicyStatus2_INTERFACE_DEFINED__ */


#ifndef __IGoogleUpdate3Web_INTERFACE_DEFINED__
#define __IGoogleUpdate3Web_INTERFACE_DEFINED__

/* interface IGoogleUpdate3Web */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IGoogleUpdate3Web;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("494B20CF-282E-4BDD-9F5D-B70CB09D351E")
    IGoogleUpdate3Web : public IDispatch
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE createAppBundleWeb( 
            /* [retval][out] */ IDispatch **app_bundle_web) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IGoogleUpdate3WebVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IGoogleUpdate3Web * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IGoogleUpdate3Web * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IGoogleUpdate3Web * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IGoogleUpdate3Web * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IGoogleUpdate3Web * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IGoogleUpdate3Web * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IGoogleUpdate3Web * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IGoogleUpdate3Web, createAppBundleWeb)
        HRESULT ( STDMETHODCALLTYPE *createAppBundleWeb )( 
            IGoogleUpdate3Web * This,
            /* [retval][out] */ IDispatch **app_bundle_web);
        
        END_INTERFACE
    } IGoogleUpdate3WebVtbl;

    interface IGoogleUpdate3Web
    {
        CONST_VTBL struct IGoogleUpdate3WebVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IGoogleUpdate3Web_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IGoogleUpdate3Web_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IGoogleUpdate3Web_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IGoogleUpdate3Web_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IGoogleUpdate3Web_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IGoogleUpdate3Web_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IGoogleUpdate3Web_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IGoogleUpdate3Web_createAppBundleWeb(This,app_bundle_web)	\
    ( (This)->lpVtbl -> createAppBundleWeb(This,app_bundle_web) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IGoogleUpdate3Web_INTERFACE_DEFINED__ */


#ifndef __IGoogleUpdate3WebSecurity_INTERFACE_DEFINED__
#define __IGoogleUpdate3WebSecurity_INTERFACE_DEFINED__

/* interface IGoogleUpdate3WebSecurity */
/* [unique][helpstring][uuid][object] */ 


EXTERN_C const IID IID_IGoogleUpdate3WebSecurity;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2D363682-561D-4c3a-81C6-F2F82107562A")
    IGoogleUpdate3WebSecurity : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE setOriginURL( 
            /* [in] */ BSTR origin_url) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IGoogleUpdate3WebSecurityVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IGoogleUpdate3WebSecurity * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IGoogleUpdate3WebSecurity * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IGoogleUpdate3WebSecurity * This);
        
        DECLSPEC_XFGVIRT(IGoogleUpdate3WebSecurity, setOriginURL)
        HRESULT ( STDMETHODCALLTYPE *setOriginURL )( 
            IGoogleUpdate3WebSecurity * This,
            /* [in] */ BSTR origin_url);
        
        END_INTERFACE
    } IGoogleUpdate3WebSecurityVtbl;

    interface IGoogleUpdate3WebSecurity
    {
        CONST_VTBL struct IGoogleUpdate3WebSecurityVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IGoogleUpdate3WebSecurity_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IGoogleUpdate3WebSecurity_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IGoogleUpdate3WebSecurity_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IGoogleUpdate3WebSecurity_setOriginURL(This,origin_url)	\
    ( (This)->lpVtbl -> setOriginURL(This,origin_url) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IGoogleUpdate3WebSecurity_INTERFACE_DEFINED__ */


#ifndef __IAppBundleWeb_INTERFACE_DEFINED__
#define __IAppBundleWeb_INTERFACE_DEFINED__

/* interface IAppBundleWeb */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IAppBundleWeb;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("DD42475D-6D46-496a-924E-BD5630B4CBBA")
    IAppBundleWeb : public IDispatch
    {
    public:
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE createApp( 
            /* [in] */ BSTR app_guid,
            /* [in] */ BSTR brand_code,
            /* [in] */ BSTR language,
            /* [in] */ BSTR ap) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE createInstalledApp( 
            /* [in] */ BSTR app_id) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE createAllInstalledApps( void) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_displayLanguage( 
            /* [retval][out] */ BSTR *__MIDL__IAppBundleWeb0000) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_displayLanguage( 
            /* [in] */ BSTR __MIDL__IAppBundleWeb0001) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_parentHWND( 
            /* [in] */ ULONG_PTR hwnd) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_length( 
            /* [retval][out] */ int *index) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_appWeb( 
            /* [in] */ int index,
            /* [retval][out] */ IDispatch **app_web) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE initialize( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE checkForUpdate( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE download( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE install( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE pause( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE resume( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE cancel( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE downloadPackage( 
            /* [in] */ BSTR app_id,
            /* [in] */ BSTR package_name) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_currentState( 
            /* [retval][out] */ VARIANT *current_state) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IAppBundleWebVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IAppBundleWeb * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IAppBundleWeb * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IAppBundleWeb * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IAppBundleWeb * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IAppBundleWeb * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IAppBundleWeb * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IAppBundleWeb * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, createApp)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *createApp )( 
            IAppBundleWeb * This,
            /* [in] */ BSTR app_guid,
            /* [in] */ BSTR brand_code,
            /* [in] */ BSTR language,
            /* [in] */ BSTR ap);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, createInstalledApp)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *createInstalledApp )( 
            IAppBundleWeb * This,
            /* [in] */ BSTR app_id);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, createAllInstalledApps)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *createAllInstalledApps )( 
            IAppBundleWeb * This);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, get_displayLanguage)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_displayLanguage )( 
            IAppBundleWeb * This,
            /* [retval][out] */ BSTR *__MIDL__IAppBundleWeb0000);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, put_displayLanguage)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_displayLanguage )( 
            IAppBundleWeb * This,
            /* [in] */ BSTR __MIDL__IAppBundleWeb0001);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, put_parentHWND)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_parentHWND )( 
            IAppBundleWeb * This,
            /* [in] */ ULONG_PTR hwnd);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, get_length)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            IAppBundleWeb * This,
            /* [retval][out] */ int *index);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, get_appWeb)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_appWeb )( 
            IAppBundleWeb * This,
            /* [in] */ int index,
            /* [retval][out] */ IDispatch **app_web);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, initialize)
        HRESULT ( STDMETHODCALLTYPE *initialize )( 
            IAppBundleWeb * This);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, checkForUpdate)
        HRESULT ( STDMETHODCALLTYPE *checkForUpdate )( 
            IAppBundleWeb * This);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, download)
        HRESULT ( STDMETHODCALLTYPE *download )( 
            IAppBundleWeb * This);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, install)
        HRESULT ( STDMETHODCALLTYPE *install )( 
            IAppBundleWeb * This);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, pause)
        HRESULT ( STDMETHODCALLTYPE *pause )( 
            IAppBundleWeb * This);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, resume)
        HRESULT ( STDMETHODCALLTYPE *resume )( 
            IAppBundleWeb * This);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, cancel)
        HRESULT ( STDMETHODCALLTYPE *cancel )( 
            IAppBundleWeb * This);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, downloadPackage)
        HRESULT ( STDMETHODCALLTYPE *downloadPackage )( 
            IAppBundleWeb * This,
            /* [in] */ BSTR app_id,
            /* [in] */ BSTR package_name);
        
        DECLSPEC_XFGVIRT(IAppBundleWeb, get_currentState)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_currentState )( 
            IAppBundleWeb * This,
            /* [retval][out] */ VARIANT *current_state);
        
        END_INTERFACE
    } IAppBundleWebVtbl;

    interface IAppBundleWeb
    {
        CONST_VTBL struct IAppBundleWebVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IAppBundleWeb_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IAppBundleWeb_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IAppBundleWeb_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IAppBundleWeb_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IAppBundleWeb_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IAppBundleWeb_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IAppBundleWeb_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IAppBundleWeb_createApp(This,app_guid,brand_code,language,ap)	\
    ( (This)->lpVtbl -> createApp(This,app_guid,brand_code,language,ap) ) 

#define IAppBundleWeb_createInstalledApp(This,app_id)	\
    ( (This)->lpVtbl -> createInstalledApp(This,app_id) ) 

#define IAppBundleWeb_createAllInstalledApps(This)	\
    ( (This)->lpVtbl -> createAllInstalledApps(This) ) 

#define IAppBundleWeb_get_displayLanguage(This,__MIDL__IAppBundleWeb0000)	\
    ( (This)->lpVtbl -> get_displayLanguage(This,__MIDL__IAppBundleWeb0000) ) 

#define IAppBundleWeb_put_displayLanguage(This,__MIDL__IAppBundleWeb0001)	\
    ( (This)->lpVtbl -> put_displayLanguage(This,__MIDL__IAppBundleWeb0001) ) 

#define IAppBundleWeb_put_parentHWND(This,hwnd)	\
    ( (This)->lpVtbl -> put_parentHWND(This,hwnd) ) 

#define IAppBundleWeb_get_length(This,index)	\
    ( (This)->lpVtbl -> get_length(This,index) ) 

#define IAppBundleWeb_get_appWeb(This,index,app_web)	\
    ( (This)->lpVtbl -> get_appWeb(This,index,app_web) ) 

#define IAppBundleWeb_initialize(This)	\
    ( (This)->lpVtbl -> initialize(This) ) 

#define IAppBundleWeb_checkForUpdate(This)	\
    ( (This)->lpVtbl -> checkForUpdate(This) ) 

#define IAppBundleWeb_download(This)	\
    ( (This)->lpVtbl -> download(This) ) 

#define IAppBundleWeb_install(This)	\
    ( (This)->lpVtbl -> install(This) ) 

#define IAppBundleWeb_pause(This)	\
    ( (This)->lpVtbl -> pause(This) ) 

#define IAppBundleWeb_resume(This)	\
    ( (This)->lpVtbl -> resume(This) ) 

#define IAppBundleWeb_cancel(This)	\
    ( (This)->lpVtbl -> cancel(This) ) 

#define IAppBundleWeb_downloadPackage(This,app_id,package_name)	\
    ( (This)->lpVtbl -> downloadPackage(This,app_id,package_name) ) 

#define IAppBundleWeb_get_currentState(This,current_state)	\
    ( (This)->lpVtbl -> get_currentState(This,current_state) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IAppBundleWeb_INTERFACE_DEFINED__ */


#ifndef __IAppWeb_INTERFACE_DEFINED__
#define __IAppWeb_INTERFACE_DEFINED__

/* interface IAppWeb */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IAppWeb;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("18D0F672-18B4-48e6-AD36-6E6BF01DBBC4")
    IAppWeb : public IDispatch
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_appId( 
            /* [retval][out] */ BSTR *__MIDL__IAppWeb0000) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_currentVersionWeb( 
            /* [retval][out] */ IDispatch **current) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nextVersionWeb( 
            /* [retval][out] */ IDispatch **next) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_command( 
            /* [in] */ BSTR command_id,
            /* [retval][out] */ IDispatch **command) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE cancel( void) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_currentState( 
            /* [retval][out] */ IDispatch **current_state) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE launch( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE uninstall( void) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_serverInstallDataIndex( 
            /* [retval][out] */ BSTR *__MIDL__IAppWeb0001) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_serverInstallDataIndex( 
            /* [in] */ BSTR __MIDL__IAppWeb0002) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IAppWebVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IAppWeb * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IAppWeb * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IAppWeb * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IAppWeb * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IAppWeb * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IAppWeb * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IAppWeb * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IAppWeb, get_appId)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_appId )( 
            IAppWeb * This,
            /* [retval][out] */ BSTR *__MIDL__IAppWeb0000);
        
        DECLSPEC_XFGVIRT(IAppWeb, get_currentVersionWeb)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_currentVersionWeb )( 
            IAppWeb * This,
            /* [retval][out] */ IDispatch **current);
        
        DECLSPEC_XFGVIRT(IAppWeb, get_nextVersionWeb)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextVersionWeb )( 
            IAppWeb * This,
            /* [retval][out] */ IDispatch **next);
        
        DECLSPEC_XFGVIRT(IAppWeb, get_command)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_command )( 
            IAppWeb * This,
            /* [in] */ BSTR command_id,
            /* [retval][out] */ IDispatch **command);
        
        DECLSPEC_XFGVIRT(IAppWeb, cancel)
        HRESULT ( STDMETHODCALLTYPE *cancel )( 
            IAppWeb * This);
        
        DECLSPEC_XFGVIRT(IAppWeb, get_currentState)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_currentState )( 
            IAppWeb * This,
            /* [retval][out] */ IDispatch **current_state);
        
        DECLSPEC_XFGVIRT(IAppWeb, launch)
        HRESULT ( STDMETHODCALLTYPE *launch )( 
            IAppWeb * This);
        
        DECLSPEC_XFGVIRT(IAppWeb, uninstall)
        HRESULT ( STDMETHODCALLTYPE *uninstall )( 
            IAppWeb * This);
        
        DECLSPEC_XFGVIRT(IAppWeb, get_serverInstallDataIndex)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_serverInstallDataIndex )( 
            IAppWeb * This,
            /* [retval][out] */ BSTR *__MIDL__IAppWeb0001);
        
        DECLSPEC_XFGVIRT(IAppWeb, put_serverInstallDataIndex)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_serverInstallDataIndex )( 
            IAppWeb * This,
            /* [in] */ BSTR __MIDL__IAppWeb0002);
        
        END_INTERFACE
    } IAppWebVtbl;

    interface IAppWeb
    {
        CONST_VTBL struct IAppWebVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IAppWeb_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IAppWeb_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IAppWeb_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IAppWeb_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IAppWeb_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IAppWeb_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IAppWeb_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IAppWeb_get_appId(This,__MIDL__IAppWeb0000)	\
    ( (This)->lpVtbl -> get_appId(This,__MIDL__IAppWeb0000) ) 

#define IAppWeb_get_currentVersionWeb(This,current)	\
    ( (This)->lpVtbl -> get_currentVersionWeb(This,current) ) 

#define IAppWeb_get_nextVersionWeb(This,next)	\
    ( (This)->lpVtbl -> get_nextVersionWeb(This,next) ) 

#define IAppWeb_get_command(This,command_id,command)	\
    ( (This)->lpVtbl -> get_command(This,command_id,command) ) 

#define IAppWeb_cancel(This)	\
    ( (This)->lpVtbl -> cancel(This) ) 

#define IAppWeb_get_currentState(This,current_state)	\
    ( (This)->lpVtbl -> get_currentState(This,current_state) ) 

#define IAppWeb_launch(This)	\
    ( (This)->lpVtbl -> launch(This) ) 

#define IAppWeb_uninstall(This)	\
    ( (This)->lpVtbl -> uninstall(This) ) 

#define IAppWeb_get_serverInstallDataIndex(This,__MIDL__IAppWeb0001)	\
    ( (This)->lpVtbl -> get_serverInstallDataIndex(This,__MIDL__IAppWeb0001) ) 

#define IAppWeb_put_serverInstallDataIndex(This,__MIDL__IAppWeb0002)	\
    ( (This)->lpVtbl -> put_serverInstallDataIndex(This,__MIDL__IAppWeb0002) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IAppWeb_INTERFACE_DEFINED__ */


#ifndef __IAppCommandWeb_INTERFACE_DEFINED__
#define __IAppCommandWeb_INTERFACE_DEFINED__

/* interface IAppCommandWeb */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IAppCommandWeb;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8476CE12-AE1F-4198-805C-BA0F9B783F57")
    IAppCommandWeb : public IDispatch
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_status( 
            /* [retval][out] */ UINT *__MIDL__IAppCommandWeb0000) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_exitCode( 
            /* [retval][out] */ DWORD *__MIDL__IAppCommandWeb0001) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_output( 
            /* [retval][out] */ BSTR *__MIDL__IAppCommandWeb0002) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE execute( 
            /* [optional][in] */ VARIANT arg1,
            /* [optional][in] */ VARIANT arg2,
            /* [optional][in] */ VARIANT arg3,
            /* [optional][in] */ VARIANT arg4,
            /* [optional][in] */ VARIANT arg5,
            /* [optional][in] */ VARIANT arg6,
            /* [optional][in] */ VARIANT arg7,
            /* [optional][in] */ VARIANT arg8,
            /* [optional][in] */ VARIANT arg9) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IAppCommandWebVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IAppCommandWeb * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IAppCommandWeb * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IAppCommandWeb * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IAppCommandWeb * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IAppCommandWeb * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IAppCommandWeb * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IAppCommandWeb * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IAppCommandWeb, get_status)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_status )( 
            IAppCommandWeb * This,
            /* [retval][out] */ UINT *__MIDL__IAppCommandWeb0000);
        
        DECLSPEC_XFGVIRT(IAppCommandWeb, get_exitCode)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_exitCode )( 
            IAppCommandWeb * This,
            /* [retval][out] */ DWORD *__MIDL__IAppCommandWeb0001);
        
        DECLSPEC_XFGVIRT(IAppCommandWeb, get_output)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_output )( 
            IAppCommandWeb * This,
            /* [retval][out] */ BSTR *__MIDL__IAppCommandWeb0002);
        
        DECLSPEC_XFGVIRT(IAppCommandWeb, execute)
        HRESULT ( STDMETHODCALLTYPE *execute )( 
            IAppCommandWeb * This,
            /* [optional][in] */ VARIANT arg1,
            /* [optional][in] */ VARIANT arg2,
            /* [optional][in] */ VARIANT arg3,
            /* [optional][in] */ VARIANT arg4,
            /* [optional][in] */ VARIANT arg5,
            /* [optional][in] */ VARIANT arg6,
            /* [optional][in] */ VARIANT arg7,
            /* [optional][in] */ VARIANT arg8,
            /* [optional][in] */ VARIANT arg9);
        
        END_INTERFACE
    } IAppCommandWebVtbl;

    interface IAppCommandWeb
    {
        CONST_VTBL struct IAppCommandWebVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IAppCommandWeb_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IAppCommandWeb_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IAppCommandWeb_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IAppCommandWeb_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IAppCommandWeb_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IAppCommandWeb_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IAppCommandWeb_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IAppCommandWeb_get_status(This,__MIDL__IAppCommandWeb0000)	\
    ( (This)->lpVtbl -> get_status(This,__MIDL__IAppCommandWeb0000) ) 

#define IAppCommandWeb_get_exitCode(This,__MIDL__IAppCommandWeb0001)	\
    ( (This)->lpVtbl -> get_exitCode(This,__MIDL__IAppCommandWeb0001) ) 

#define IAppCommandWeb_get_output(This,__MIDL__IAppCommandWeb0002)	\
    ( (This)->lpVtbl -> get_output(This,__MIDL__IAppCommandWeb0002) ) 

#define IAppCommandWeb_execute(This,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)	\
    ( (This)->lpVtbl -> execute(This,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IAppCommandWeb_INTERFACE_DEFINED__ */


#ifndef __IAppVersionWeb_INTERFACE_DEFINED__
#define __IAppVersionWeb_INTERFACE_DEFINED__

/* interface IAppVersionWeb */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IAppVersionWeb;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("0CD01D1E-4A1C-489d-93B9-9B6672877C57")
    IAppVersionWeb : public IDispatch
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_version( 
            /* [retval][out] */ BSTR *__MIDL__IAppVersionWeb0000) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_packageCount( 
            /* [retval][out] */ long *count) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_packageWeb( 
            /* [in] */ long index,
            /* [retval][out] */ IDispatch **package) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IAppVersionWebVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IAppVersionWeb * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IAppVersionWeb * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IAppVersionWeb * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IAppVersionWeb * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IAppVersionWeb * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IAppVersionWeb * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IAppVersionWeb * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IAppVersionWeb, get_version)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_version )( 
            IAppVersionWeb * This,
            /* [retval][out] */ BSTR *__MIDL__IAppVersionWeb0000);
        
        DECLSPEC_XFGVIRT(IAppVersionWeb, get_packageCount)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_packageCount )( 
            IAppVersionWeb * This,
            /* [retval][out] */ long *count);
        
        DECLSPEC_XFGVIRT(IAppVersionWeb, get_packageWeb)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_packageWeb )( 
            IAppVersionWeb * This,
            /* [in] */ long index,
            /* [retval][out] */ IDispatch **package);
        
        END_INTERFACE
    } IAppVersionWebVtbl;

    interface IAppVersionWeb
    {
        CONST_VTBL struct IAppVersionWebVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IAppVersionWeb_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IAppVersionWeb_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IAppVersionWeb_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IAppVersionWeb_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IAppVersionWeb_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IAppVersionWeb_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IAppVersionWeb_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IAppVersionWeb_get_version(This,__MIDL__IAppVersionWeb0000)	\
    ( (This)->lpVtbl -> get_version(This,__MIDL__IAppVersionWeb0000) ) 

#define IAppVersionWeb_get_packageCount(This,count)	\
    ( (This)->lpVtbl -> get_packageCount(This,count) ) 

#define IAppVersionWeb_get_packageWeb(This,index,package)	\
    ( (This)->lpVtbl -> get_packageWeb(This,index,package) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IAppVersionWeb_INTERFACE_DEFINED__ */


#ifndef __ICoCreateAsyncStatus_INTERFACE_DEFINED__
#define __ICoCreateAsyncStatus_INTERFACE_DEFINED__

/* interface ICoCreateAsyncStatus */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_ICoCreateAsyncStatus;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2E629606-312A-482f-9B12-2C4ABF6F0B6D")
    ICoCreateAsyncStatus : public IDispatch
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_isDone( 
            /* [retval][out] */ VARIANT_BOOL *is_done) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_completionHResult( 
            /* [retval][out] */ LONG *hr) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_createdInstance( 
            /* [retval][out] */ IDispatch **instance) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ICoCreateAsyncStatusVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ICoCreateAsyncStatus * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ICoCreateAsyncStatus * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ICoCreateAsyncStatus * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ICoCreateAsyncStatus * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ICoCreateAsyncStatus * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ICoCreateAsyncStatus * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ICoCreateAsyncStatus * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(ICoCreateAsyncStatus, get_isDone)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_isDone )( 
            ICoCreateAsyncStatus * This,
            /* [retval][out] */ VARIANT_BOOL *is_done);
        
        DECLSPEC_XFGVIRT(ICoCreateAsyncStatus, get_completionHResult)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_completionHResult )( 
            ICoCreateAsyncStatus * This,
            /* [retval][out] */ LONG *hr);
        
        DECLSPEC_XFGVIRT(ICoCreateAsyncStatus, get_createdInstance)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_createdInstance )( 
            ICoCreateAsyncStatus * This,
            /* [retval][out] */ IDispatch **instance);
        
        END_INTERFACE
    } ICoCreateAsyncStatusVtbl;

    interface ICoCreateAsyncStatus
    {
        CONST_VTBL struct ICoCreateAsyncStatusVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ICoCreateAsyncStatus_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ICoCreateAsyncStatus_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ICoCreateAsyncStatus_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ICoCreateAsyncStatus_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define ICoCreateAsyncStatus_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define ICoCreateAsyncStatus_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define ICoCreateAsyncStatus_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define ICoCreateAsyncStatus_get_isDone(This,is_done)	\
    ( (This)->lpVtbl -> get_isDone(This,is_done) ) 

#define ICoCreateAsyncStatus_get_completionHResult(This,hr)	\
    ( (This)->lpVtbl -> get_completionHResult(This,hr) ) 

#define ICoCreateAsyncStatus_get_createdInstance(This,instance)	\
    ( (This)->lpVtbl -> get_createdInstance(This,instance) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ICoCreateAsyncStatus_INTERFACE_DEFINED__ */


#ifndef __ICoCreateAsync_INTERFACE_DEFINED__
#define __ICoCreateAsync_INTERFACE_DEFINED__

/* interface ICoCreateAsync */
/* [unique][helpstring][uuid][object] */ 


EXTERN_C const IID IID_ICoCreateAsync;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("DAB1D343-1B2A-47f9-B445-93DC50704BFE")
    ICoCreateAsync : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE createOmahaMachineServerAsync( 
            /* [in] */ BSTR origin_url,
            /* [in] */ BOOL create_elevated,
            /* [retval][out] */ ICoCreateAsyncStatus **status) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ICoCreateAsyncVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ICoCreateAsync * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ICoCreateAsync * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ICoCreateAsync * This);
        
        DECLSPEC_XFGVIRT(ICoCreateAsync, createOmahaMachineServerAsync)
        HRESULT ( STDMETHODCALLTYPE *createOmahaMachineServerAsync )( 
            ICoCreateAsync * This,
            /* [in] */ BSTR origin_url,
            /* [in] */ BOOL create_elevated,
            /* [retval][out] */ ICoCreateAsyncStatus **status);
        
        END_INTERFACE
    } ICoCreateAsyncVtbl;

    interface ICoCreateAsync
    {
        CONST_VTBL struct ICoCreateAsyncVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ICoCreateAsync_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ICoCreateAsync_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ICoCreateAsync_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ICoCreateAsync_createOmahaMachineServerAsync(This,origin_url,create_elevated,status)	\
    ( (This)->lpVtbl -> createOmahaMachineServerAsync(This,origin_url,create_elevated,status) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ICoCreateAsync_INTERFACE_DEFINED__ */


#ifndef __IBrowserHttpRequest2_INTERFACE_DEFINED__
#define __IBrowserHttpRequest2_INTERFACE_DEFINED__

/* interface IBrowserHttpRequest2 */
/* [unique][nonextensible][oleautomation][uuid][object] */ 


EXTERN_C const IID IID_IBrowserHttpRequest2;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("5B25A8DC-1780-4178-A629-6BE8B8DEFAA2")
    IBrowserHttpRequest2 : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Send( 
            /* [in] */ BSTR url,
            /* [in] */ BSTR post_data,
            /* [in] */ BSTR request_headers,
            /* [in] */ VARIANT response_headers_needed,
            /* [out] */ VARIANT *response_headers,
            /* [out] */ DWORD *response_code,
            /* [out] */ BSTR *cache_filename) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IBrowserHttpRequest2Vtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IBrowserHttpRequest2 * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IBrowserHttpRequest2 * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IBrowserHttpRequest2 * This);
        
        DECLSPEC_XFGVIRT(IBrowserHttpRequest2, Send)
        HRESULT ( STDMETHODCALLTYPE *Send )( 
            IBrowserHttpRequest2 * This,
            /* [in] */ BSTR url,
            /* [in] */ BSTR post_data,
            /* [in] */ BSTR request_headers,
            /* [in] */ VARIANT response_headers_needed,
            /* [out] */ VARIANT *response_headers,
            /* [out] */ DWORD *response_code,
            /* [out] */ BSTR *cache_filename);
        
        END_INTERFACE
    } IBrowserHttpRequest2Vtbl;

    interface IBrowserHttpRequest2
    {
        CONST_VTBL struct IBrowserHttpRequest2Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBrowserHttpRequest2_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IBrowserHttpRequest2_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IBrowserHttpRequest2_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IBrowserHttpRequest2_Send(This,url,post_data,request_headers,response_headers_needed,response_headers,response_code,cache_filename)	\
    ( (This)->lpVtbl -> Send(This,url,post_data,request_headers,response_headers_needed,response_headers,response_code,cache_filename) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IBrowserHttpRequest2_INTERFACE_DEFINED__ */


#ifndef __IProcessLauncher_INTERFACE_DEFINED__
#define __IProcessLauncher_INTERFACE_DEFINED__

/* interface IProcessLauncher */
/* [unique][helpstring][uuid][oleautomation][object] */ 


EXTERN_C const IID IID_IProcessLauncher;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("128C2DA6-2BC0-44c0-B3F6-4EC22E647964")
    IProcessLauncher : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE LaunchCmdLine( 
            /* [string][in] */ const WCHAR *cmd_line) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE LaunchBrowser( 
            /* [in] */ DWORD browser_type,
            /* [string][in] */ const WCHAR *url) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE LaunchCmdElevated( 
            /* [string][in] */ const WCHAR *app_guid,
            /* [string][in] */ const WCHAR *cmd_id,
            /* [in] */ DWORD caller_proc_id,
            /* [out] */ ULONG_PTR *proc_handle) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IProcessLauncherVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IProcessLauncher * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IProcessLauncher * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IProcessLauncher * This);
        
        DECLSPEC_XFGVIRT(IProcessLauncher, LaunchCmdLine)
        HRESULT ( STDMETHODCALLTYPE *LaunchCmdLine )( 
            IProcessLauncher * This,
            /* [string][in] */ const WCHAR *cmd_line);
        
        DECLSPEC_XFGVIRT(IProcessLauncher, LaunchBrowser)
        HRESULT ( STDMETHODCALLTYPE *LaunchBrowser )( 
            IProcessLauncher * This,
            /* [in] */ DWORD browser_type,
            /* [string][in] */ const WCHAR *url);
        
        DECLSPEC_XFGVIRT(IProcessLauncher, LaunchCmdElevated)
        HRESULT ( STDMETHODCALLTYPE *LaunchCmdElevated )( 
            IProcessLauncher * This,
            /* [string][in] */ const WCHAR *app_guid,
            /* [string][in] */ const WCHAR *cmd_id,
            /* [in] */ DWORD caller_proc_id,
            /* [out] */ ULONG_PTR *proc_handle);
        
        END_INTERFACE
    } IProcessLauncherVtbl;

    interface IProcessLauncher
    {
        CONST_VTBL struct IProcessLauncherVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IProcessLauncher_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IProcessLauncher_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IProcessLauncher_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IProcessLauncher_LaunchCmdLine(This,cmd_line)	\
    ( (This)->lpVtbl -> LaunchCmdLine(This,cmd_line) ) 

#define IProcessLauncher_LaunchBrowser(This,browser_type,url)	\
    ( (This)->lpVtbl -> LaunchBrowser(This,browser_type,url) ) 

#define IProcessLauncher_LaunchCmdElevated(This,app_guid,cmd_id,caller_proc_id,proc_handle)	\
    ( (This)->lpVtbl -> LaunchCmdElevated(This,app_guid,cmd_id,caller_proc_id,proc_handle) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IProcessLauncher_INTERFACE_DEFINED__ */


#ifndef __IProcessLauncher2_INTERFACE_DEFINED__
#define __IProcessLauncher2_INTERFACE_DEFINED__

/* interface IProcessLauncher2 */
/* [unique][helpstring][uuid][oleautomation][object] */ 


EXTERN_C const IID IID_IProcessLauncher2;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("D106AB5F-A70E-400E-A21B-96208C1D8DBB")
    IProcessLauncher2 : public IProcessLauncher
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE LaunchCmdLineEx( 
            /* [string][in] */ const WCHAR *cmd_line,
            /* [out] */ DWORD *server_proc_id,
            /* [out] */ ULONG_PTR *proc_handle,
            /* [out] */ ULONG_PTR *stdout_handle) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IProcessLauncher2Vtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IProcessLauncher2 * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IProcessLauncher2 * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IProcessLauncher2 * This);
        
        DECLSPEC_XFGVIRT(IProcessLauncher, LaunchCmdLine)
        HRESULT ( STDMETHODCALLTYPE *LaunchCmdLine )( 
            IProcessLauncher2 * This,
            /* [string][in] */ const WCHAR *cmd_line);
        
        DECLSPEC_XFGVIRT(IProcessLauncher, LaunchBrowser)
        HRESULT ( STDMETHODCALLTYPE *LaunchBrowser )( 
            IProcessLauncher2 * This,
            /* [in] */ DWORD browser_type,
            /* [string][in] */ const WCHAR *url);
        
        DECLSPEC_XFGVIRT(IProcessLauncher, LaunchCmdElevated)
        HRESULT ( STDMETHODCALLTYPE *LaunchCmdElevated )( 
            IProcessLauncher2 * This,
            /* [string][in] */ const WCHAR *app_guid,
            /* [string][in] */ const WCHAR *cmd_id,
            /* [in] */ DWORD caller_proc_id,
            /* [out] */ ULONG_PTR *proc_handle);
        
        DECLSPEC_XFGVIRT(IProcessLauncher2, LaunchCmdLineEx)
        HRESULT ( STDMETHODCALLTYPE *LaunchCmdLineEx )( 
            IProcessLauncher2 * This,
            /* [string][in] */ const WCHAR *cmd_line,
            /* [out] */ DWORD *server_proc_id,
            /* [out] */ ULONG_PTR *proc_handle,
            /* [out] */ ULONG_PTR *stdout_handle);
        
        END_INTERFACE
    } IProcessLauncher2Vtbl;

    interface IProcessLauncher2
    {
        CONST_VTBL struct IProcessLauncher2Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IProcessLauncher2_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IProcessLauncher2_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IProcessLauncher2_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IProcessLauncher2_LaunchCmdLine(This,cmd_line)	\
    ( (This)->lpVtbl -> LaunchCmdLine(This,cmd_line) ) 

#define IProcessLauncher2_LaunchBrowser(This,browser_type,url)	\
    ( (This)->lpVtbl -> LaunchBrowser(This,browser_type,url) ) 

#define IProcessLauncher2_LaunchCmdElevated(This,app_guid,cmd_id,caller_proc_id,proc_handle)	\
    ( (This)->lpVtbl -> LaunchCmdElevated(This,app_guid,cmd_id,caller_proc_id,proc_handle) ) 


#define IProcessLauncher2_LaunchCmdLineEx(This,cmd_line,server_proc_id,proc_handle,stdout_handle)	\
    ( (This)->lpVtbl -> LaunchCmdLineEx(This,cmd_line,server_proc_id,proc_handle,stdout_handle) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IProcessLauncher2_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_google_update_idl_0000_0025 */
/* [local] */ 

typedef /* [public][public] */ 
enum __MIDL___MIDL_itf_google_update_idl_0000_0025_0001
    {
        COMPLETION_CODE_SUCCESS	= 1,
        COMPLETION_CODE_SUCCESS_CLOSE_UI	= ( COMPLETION_CODE_SUCCESS + 1 ) ,
        COMPLETION_CODE_ERROR	= ( COMPLETION_CODE_SUCCESS_CLOSE_UI + 1 ) ,
        COMPLETION_CODE_RESTART_ALL_BROWSERS	= ( COMPLETION_CODE_ERROR + 1 ) ,
        COMPLETION_CODE_REBOOT	= ( COMPLETION_CODE_RESTART_ALL_BROWSERS + 1 ) ,
        COMPLETION_CODE_RESTART_BROWSER	= ( COMPLETION_CODE_REBOOT + 1 ) ,
        COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY	= ( COMPLETION_CODE_RESTART_BROWSER + 1 ) ,
        COMPLETION_CODE_REBOOT_NOTICE_ONLY	= ( COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY + 1 ) ,
        COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY	= ( COMPLETION_CODE_REBOOT_NOTICE_ONLY + 1 ) ,
        COMPLETION_CODE_RUN_COMMAND	= ( COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY + 1 ) 
    } 	LegacyCompletionCodes;



extern RPC_IF_HANDLE __MIDL_itf_google_update_idl_0000_0025_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_google_update_idl_0000_0025_v0_0_s_ifspec;

#ifndef __IProgressWndEvents_INTERFACE_DEFINED__
#define __IProgressWndEvents_INTERFACE_DEFINED__

/* interface IProgressWndEvents */
/* [unique][helpstring][uuid][oleautomation][object] */ 


EXTERN_C const IID IID_IProgressWndEvents;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("1C642CED-CA3B-4013-A9DF-CA6CE5FF6503")
    IProgressWndEvents : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE DoClose( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE DoPause( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE DoResume( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE DoRestartBrowsers( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE DoReboot( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE DoLaunchBrowser( 
            /* [string][in] */ const WCHAR *url) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IProgressWndEventsVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IProgressWndEvents * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IProgressWndEvents * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IProgressWndEvents * This);
        
        DECLSPEC_XFGVIRT(IProgressWndEvents, DoClose)
        HRESULT ( STDMETHODCALLTYPE *DoClose )( 
            IProgressWndEvents * This);
        
        DECLSPEC_XFGVIRT(IProgressWndEvents, DoPause)
        HRESULT ( STDMETHODCALLTYPE *DoPause )( 
            IProgressWndEvents * This);
        
        DECLSPEC_XFGVIRT(IProgressWndEvents, DoResume)
        HRESULT ( STDMETHODCALLTYPE *DoResume )( 
            IProgressWndEvents * This);
        
        DECLSPEC_XFGVIRT(IProgressWndEvents, DoRestartBrowsers)
        HRESULT ( STDMETHODCALLTYPE *DoRestartBrowsers )( 
            IProgressWndEvents * This);
        
        DECLSPEC_XFGVIRT(IProgressWndEvents, DoReboot)
        HRESULT ( STDMETHODCALLTYPE *DoReboot )( 
            IProgressWndEvents * This);
        
        DECLSPEC_XFGVIRT(IProgressWndEvents, DoLaunchBrowser)
        HRESULT ( STDMETHODCALLTYPE *DoLaunchBrowser )( 
            IProgressWndEvents * This,
            /* [string][in] */ const WCHAR *url);
        
        END_INTERFACE
    } IProgressWndEventsVtbl;

    interface IProgressWndEvents
    {
        CONST_VTBL struct IProgressWndEventsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IProgressWndEvents_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IProgressWndEvents_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IProgressWndEvents_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IProgressWndEvents_DoClose(This)	\
    ( (This)->lpVtbl -> DoClose(This) ) 

#define IProgressWndEvents_DoPause(This)	\
    ( (This)->lpVtbl -> DoPause(This) ) 

#define IProgressWndEvents_DoResume(This)	\
    ( (This)->lpVtbl -> DoResume(This) ) 

#define IProgressWndEvents_DoRestartBrowsers(This)	\
    ( (This)->lpVtbl -> DoRestartBrowsers(This) ) 

#define IProgressWndEvents_DoReboot(This)	\
    ( (This)->lpVtbl -> DoReboot(This) ) 

#define IProgressWndEvents_DoLaunchBrowser(This,url)	\
    ( (This)->lpVtbl -> DoLaunchBrowser(This,url) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IProgressWndEvents_INTERFACE_DEFINED__ */


#ifndef __IJobObserver_INTERFACE_DEFINED__
#define __IJobObserver_INTERFACE_DEFINED__

/* interface IJobObserver */
/* [unique][helpstring][uuid][oleautomation][object] */ 


EXTERN_C const IID IID_IJobObserver;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("49D7563B-2DDB-4831-88C8-768A53833837")
    IJobObserver : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE OnShow( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnCheckingForUpdate( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnUpdateAvailable( 
            /* [string][in] */ const WCHAR *version_string) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnWaitingToDownload( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnDownloading( 
            /* [in] */ int time_remaining_ms,
            /* [in] */ int pos) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnWaitingToInstall( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnInstalling( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnPause( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnComplete( 
            /* [in] */ LegacyCompletionCodes code,
            /* [string][in] */ const WCHAR *completion_text) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetEventSink( 
            /* [in] */ IProgressWndEvents *ui_sink) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IJobObserverVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IJobObserver * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IJobObserver * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IJobObserver * This);
        
        DECLSPEC_XFGVIRT(IJobObserver, OnShow)
        HRESULT ( STDMETHODCALLTYPE *OnShow )( 
            IJobObserver * This);
        
        DECLSPEC_XFGVIRT(IJobObserver, OnCheckingForUpdate)
        HRESULT ( STDMETHODCALLTYPE *OnCheckingForUpdate )( 
            IJobObserver * This);
        
        DECLSPEC_XFGVIRT(IJobObserver, OnUpdateAvailable)
        HRESULT ( STDMETHODCALLTYPE *OnUpdateAvailable )( 
            IJobObserver * This,
            /* [string][in] */ const WCHAR *version_string);
        
        DECLSPEC_XFGVIRT(IJobObserver, OnWaitingToDownload)
        HRESULT ( STDMETHODCALLTYPE *OnWaitingToDownload )( 
            IJobObserver * This);
        
        DECLSPEC_XFGVIRT(IJobObserver, OnDownloading)
        HRESULT ( STDMETHODCALLTYPE *OnDownloading )( 
            IJobObserver * This,
            /* [in] */ int time_remaining_ms,
            /* [in] */ int pos);
        
        DECLSPEC_XFGVIRT(IJobObserver, OnWaitingToInstall)
        HRESULT ( STDMETHODCALLTYPE *OnWaitingToInstall )( 
            IJobObserver * This);
        
        DECLSPEC_XFGVIRT(IJobObserver, OnInstalling)
        HRESULT ( STDMETHODCALLTYPE *OnInstalling )( 
            IJobObserver * This);
        
        DECLSPEC_XFGVIRT(IJobObserver, OnPause)
        HRESULT ( STDMETHODCALLTYPE *OnPause )( 
            IJobObserver * This);
        
        DECLSPEC_XFGVIRT(IJobObserver, OnComplete)
        HRESULT ( STDMETHODCALLTYPE *OnComplete )( 
            IJobObserver * This,
            /* [in] */ LegacyCompletionCodes code,
            /* [string][in] */ const WCHAR *completion_text);
        
        DECLSPEC_XFGVIRT(IJobObserver, SetEventSink)
        HRESULT ( STDMETHODCALLTYPE *SetEventSink )( 
            IJobObserver * This,
            /* [in] */ IProgressWndEvents *ui_sink);
        
        END_INTERFACE
    } IJobObserverVtbl;

    interface IJobObserver
    {
        CONST_VTBL struct IJobObserverVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IJobObserver_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IJobObserver_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IJobObserver_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IJobObserver_OnShow(This)	\
    ( (This)->lpVtbl -> OnShow(This) ) 

#define IJobObserver_OnCheckingForUpdate(This)	\
    ( (This)->lpVtbl -> OnCheckingForUpdate(This) ) 

#define IJobObserver_OnUpdateAvailable(This,version_string)	\
    ( (This)->lpVtbl -> OnUpdateAvailable(This,version_string) ) 

#define IJobObserver_OnWaitingToDownload(This)	\
    ( (This)->lpVtbl -> OnWaitingToDownload(This) ) 

#define IJobObserver_OnDownloading(This,time_remaining_ms,pos)	\
    ( (This)->lpVtbl -> OnDownloading(This,time_remaining_ms,pos) ) 

#define IJobObserver_OnWaitingToInstall(This)	\
    ( (This)->lpVtbl -> OnWaitingToInstall(This) ) 

#define IJobObserver_OnInstalling(This)	\
    ( (This)->lpVtbl -> OnInstalling(This) ) 

#define IJobObserver_OnPause(This)	\
    ( (This)->lpVtbl -> OnPause(This) ) 

#define IJobObserver_OnComplete(This,code,completion_text)	\
    ( (This)->lpVtbl -> OnComplete(This,code,completion_text) ) 

#define IJobObserver_SetEventSink(This,ui_sink)	\
    ( (This)->lpVtbl -> SetEventSink(This,ui_sink) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IJobObserver_INTERFACE_DEFINED__ */


#ifndef __IJobObserver2_INTERFACE_DEFINED__
#define __IJobObserver2_INTERFACE_DEFINED__

/* interface IJobObserver2 */
/* [unique][helpstring][uuid][oleautomation][object] */ 


EXTERN_C const IID IID_IJobObserver2;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("19692F10-ADD2-4EFF-BE54-E61C62E40D13")
    IJobObserver2 : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE OnInstalling2( 
            /* [in] */ int time_remaining_ms,
            /* [in] */ int pos) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IJobObserver2Vtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IJobObserver2 * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IJobObserver2 * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IJobObserver2 * This);
        
        DECLSPEC_XFGVIRT(IJobObserver2, OnInstalling2)
        HRESULT ( STDMETHODCALLTYPE *OnInstalling2 )( 
            IJobObserver2 * This,
            /* [in] */ int time_remaining_ms,
            /* [in] */ int pos);
        
        END_INTERFACE
    } IJobObserver2Vtbl;

    interface IJobObserver2
    {
        CONST_VTBL struct IJobObserver2Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IJobObserver2_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IJobObserver2_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IJobObserver2_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IJobObserver2_OnInstalling2(This,time_remaining_ms,pos)	\
    ( (This)->lpVtbl -> OnInstalling2(This,time_remaining_ms,pos) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IJobObserver2_INTERFACE_DEFINED__ */


#ifndef __IGoogleUpdate_INTERFACE_DEFINED__
#define __IGoogleUpdate_INTERFACE_DEFINED__

/* interface IGoogleUpdate */
/* [unique][helpstring][uuid][oleautomation][object] */ 


EXTERN_C const IID IID_IGoogleUpdate;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("31AC3F11-E5EA-4a85-8A3D-8E095A39C27B")
    IGoogleUpdate : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE CheckForUpdate( 
            /* [string][in] */ const WCHAR *guid,
            /* [in] */ IJobObserver *observer) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Update( 
            /* [string][in] */ const WCHAR *guid,
            /* [in] */ IJobObserver *observer) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IGoogleUpdateVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IGoogleUpdate * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IGoogleUpdate * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IGoogleUpdate * This);
        
        DECLSPEC_XFGVIRT(IGoogleUpdate, CheckForUpdate)
        HRESULT ( STDMETHODCALLTYPE *CheckForUpdate )( 
            IGoogleUpdate * This,
            /* [string][in] */ const WCHAR *guid,
            /* [in] */ IJobObserver *observer);
        
        DECLSPEC_XFGVIRT(IGoogleUpdate, Update)
        HRESULT ( STDMETHODCALLTYPE *Update )( 
            IGoogleUpdate * This,
            /* [string][in] */ const WCHAR *guid,
            /* [in] */ IJobObserver *observer);
        
        END_INTERFACE
    } IGoogleUpdateVtbl;

    interface IGoogleUpdate
    {
        CONST_VTBL struct IGoogleUpdateVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IGoogleUpdate_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IGoogleUpdate_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IGoogleUpdate_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IGoogleUpdate_CheckForUpdate(This,guid,observer)	\
    ( (This)->lpVtbl -> CheckForUpdate(This,guid,observer) ) 

#define IGoogleUpdate_Update(This,guid,observer)	\
    ( (This)->lpVtbl -> Update(This,guid,observer) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IGoogleUpdate_INTERFACE_DEFINED__ */


#ifndef __IGoogleUpdateCore_INTERFACE_DEFINED__
#define __IGoogleUpdateCore_INTERFACE_DEFINED__

/* interface IGoogleUpdateCore */
/* [unique][helpstring][uuid][oleautomation][object] */ 


EXTERN_C const IID IID_IGoogleUpdateCore;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("909489C2-85A6-4322-AA56-D25278649D67")
    IGoogleUpdateCore : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE LaunchCmdElevated( 
            /* [string][in] */ const WCHAR *app_guid,
            /* [string][in] */ const WCHAR *cmd_id,
            /* [in] */ DWORD caller_proc_id,
            /* [out] */ ULONG_PTR *proc_handle) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IGoogleUpdateCoreVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IGoogleUpdateCore * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IGoogleUpdateCore * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IGoogleUpdateCore * This);
        
        DECLSPEC_XFGVIRT(IGoogleUpdateCore, LaunchCmdElevated)
        HRESULT ( STDMETHODCALLTYPE *LaunchCmdElevated )( 
            IGoogleUpdateCore * This,
            /* [string][in] */ const WCHAR *app_guid,
            /* [string][in] */ const WCHAR *cmd_id,
            /* [in] */ DWORD caller_proc_id,
            /* [out] */ ULONG_PTR *proc_handle);
        
        END_INTERFACE
    } IGoogleUpdateCoreVtbl;

    interface IGoogleUpdateCore
    {
        CONST_VTBL struct IGoogleUpdateCoreVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IGoogleUpdateCore_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IGoogleUpdateCore_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IGoogleUpdateCore_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IGoogleUpdateCore_LaunchCmdElevated(This,app_guid,cmd_id,caller_proc_id,proc_handle)	\
    ( (This)->lpVtbl -> LaunchCmdElevated(This,app_guid,cmd_id,caller_proc_id,proc_handle) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IGoogleUpdateCore_INTERFACE_DEFINED__ */



#ifndef __GoogleUpdate3Lib_LIBRARY_DEFINED__
#define __GoogleUpdate3Lib_LIBRARY_DEFINED__

/* library GoogleUpdate3Lib */
/* [helpstring][version][uuid] */ 




















EXTERN_C const IID LIBID_GoogleUpdate3Lib;

EXTERN_C const CLSID CLSID_GoogleUpdate3UserClass;

#ifdef __cplusplus

class DECLSPEC_UUID("022105BD-948A-40c9-AB42-A3300DDF097F")
GoogleUpdate3UserClass;
#endif

EXTERN_C const CLSID CLSID_GoogleUpdate3ServiceClass;

#ifdef __cplusplus

class DECLSPEC_UUID("4EB61BAC-A3B6-4760-9581-655041EF4D69")
GoogleUpdate3ServiceClass;
#endif

EXTERN_C const CLSID CLSID_GoogleUpdate3WebUserClass;

#ifdef __cplusplus

class DECLSPEC_UUID("22181302-A8A6-4f84-A541-E5CBFC70CC43")
GoogleUpdate3WebUserClass;
#endif

EXTERN_C const CLSID CLSID_GoogleUpdate3WebMachineClass;

#ifdef __cplusplus

class DECLSPEC_UUID("8A1D4361-2C08-4700-A351-3EAA9CBFF5E4")
GoogleUpdate3WebMachineClass;
#endif

EXTERN_C const CLSID CLSID_GoogleUpdate3WebServiceClass;

#ifdef __cplusplus

class DECLSPEC_UUID("534F5323-3569-4f42-919D-1E1CF93E5BF6")
GoogleUpdate3WebServiceClass;
#endif

EXTERN_C const CLSID CLSID_GoogleUpdate3WebMachineFallbackClass;

#ifdef __cplusplus

class DECLSPEC_UUID("598FE0E5-E02D-465d-9A9D-37974A28FD42")
GoogleUpdate3WebMachineFallbackClass;
#endif

EXTERN_C const CLSID CLSID_CurrentStateUserClass;

#ifdef __cplusplus

class DECLSPEC_UUID("E8CF3E55-F919-49d9-ABC0-948E6CB34B9F")
CurrentStateUserClass;
#endif

EXTERN_C const CLSID CLSID_CurrentStateMachineClass;

#ifdef __cplusplus

class DECLSPEC_UUID("9D6AA569-9F30-41ad-885A-346685C74928")
CurrentStateMachineClass;
#endif

EXTERN_C const CLSID CLSID_CoCreateAsyncClass;

#ifdef __cplusplus

class DECLSPEC_UUID("7DE94008-8AFD-4c70-9728-C6FBFFF6A73E")
CoCreateAsyncClass;
#endif

EXTERN_C const CLSID CLSID_CredentialDialogUserClass;

#ifdef __cplusplus

class DECLSPEC_UUID("e67be843-bbbe-4484-95fb-05271ae86750")
CredentialDialogUserClass;
#endif

EXTERN_C const CLSID CLSID_CredentialDialogMachineClass;

#ifdef __cplusplus

class DECLSPEC_UUID("25461599-633d-42b1-84fb-7cd68d026e53")
CredentialDialogMachineClass;
#endif

EXTERN_C const CLSID CLSID_PolicyStatusValueUserClass;

#ifdef __cplusplus

class DECLSPEC_UUID("85D8EE2F-794F-41F0-BB03-49D56A23BEF4")
PolicyStatusValueUserClass;
#endif

EXTERN_C const CLSID CLSID_PolicyStatusValueMachineClass;

#ifdef __cplusplus

class DECLSPEC_UUID("C6271107-A214-4F11-98C0-3F16BC670D28")
PolicyStatusValueMachineClass;
#endif

EXTERN_C const CLSID CLSID_PolicyStatusUserClass;

#ifdef __cplusplus

class DECLSPEC_UUID("6DDCE70D-A4AE-4E97-908C-BE7B2DB750AD")
PolicyStatusUserClass;
#endif

EXTERN_C const CLSID CLSID_PolicyStatusMachineClass;

#ifdef __cplusplus

class DECLSPEC_UUID("521FDB42-7130-4806-822A-FC5163FAD983")
PolicyStatusMachineClass;
#endif

EXTERN_C const CLSID CLSID_PolicyStatusMachineServiceClass;

#ifdef __cplusplus

class DECLSPEC_UUID("1C4CDEFF-756A-4804-9E77-3E8EB9361016")
PolicyStatusMachineServiceClass;
#endif

EXTERN_C const CLSID CLSID_PolicyStatusMachineFallbackClass;

#ifdef __cplusplus

class DECLSPEC_UUID("ADDF22CF-3E9B-4CD7-9139-8169EA6636E4")
PolicyStatusMachineFallbackClass;
#endif

EXTERN_C const CLSID CLSID_GoogleComProxyMachineClass;

#ifdef __cplusplus

class DECLSPEC_UUID("02B24573-5230-485A-8787-AD56B20E8ADB")
GoogleComProxyMachineClass;
#endif

EXTERN_C const CLSID CLSID_GoogleComProxyUserClass;

#ifdef __cplusplus

class DECLSPEC_UUID("D89179AA-B869-4491-AC5F-615D2B10696E")
GoogleComProxyUserClass;
#endif

EXTERN_C const CLSID CLSID_ProcessLauncherClass;

#ifdef __cplusplus

class DECLSPEC_UUID("ABC01078-F197-4b0b-ADBC-CFE684B39C82")
ProcessLauncherClass;
#endif

EXTERN_C const CLSID CLSID_OnDemandUserAppsClass;

#ifdef __cplusplus

class DECLSPEC_UUID("2F0E2680-9FF5-43c0-B76E-114A56E93598")
OnDemandUserAppsClass;
#endif

EXTERN_C const CLSID CLSID_OnDemandMachineAppsClass;

#ifdef __cplusplus

class DECLSPEC_UUID("6F8BD55B-E83D-4a47-85BE-81FFA8057A69")
OnDemandMachineAppsClass;
#endif

EXTERN_C const CLSID CLSID_OnDemandMachineAppsServiceClass;

#ifdef __cplusplus

class DECLSPEC_UUID("9465B4B4-5216-4042-9A2C-754D3BCDC410")
OnDemandMachineAppsServiceClass;
#endif

EXTERN_C const CLSID CLSID_OnDemandMachineAppsFallbackClass;

#ifdef __cplusplus

class DECLSPEC_UUID("B3D28DBD-0DFA-40e4-8071-520767BADC7E")
OnDemandMachineAppsFallbackClass;
#endif

EXTERN_C const CLSID CLSID_GoogleUpdateCoreClass;

#ifdef __cplusplus

class DECLSPEC_UUID("E225E692-4B47-4777-9BED-4FD7FE257F0E")
GoogleUpdateCoreClass;
#endif

EXTERN_C const CLSID CLSID_GoogleUpdateCoreMachineClass;

#ifdef __cplusplus

class DECLSPEC_UUID("9B2340A0-4068-43d6-B404-32E27217859D")
GoogleUpdateCoreMachineClass;
#endif
#endif /* __GoogleUpdate3Lib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

unsigned long             __RPC_USER  BSTR_UserSize(     unsigned long *, unsigned long            , BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserMarshal(  unsigned long *, unsigned char *, BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserUnmarshal(unsigned long *, unsigned char *, BSTR * ); 
void                      __RPC_USER  BSTR_UserFree(     unsigned long *, BSTR * ); 

unsigned long             __RPC_USER  VARIANT_UserSize(     unsigned long *, unsigned long            , VARIANT * ); 
unsigned char * __RPC_USER  VARIANT_UserMarshal(  unsigned long *, unsigned char *, VARIANT * ); 
unsigned char * __RPC_USER  VARIANT_UserUnmarshal(unsigned long *, unsigned char *, VARIANT * ); 
void                      __RPC_USER  VARIANT_UserFree(     unsigned long *, VARIANT * ); 

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


