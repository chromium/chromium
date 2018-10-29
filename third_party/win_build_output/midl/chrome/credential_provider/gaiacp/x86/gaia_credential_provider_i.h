

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for gen/chrome/credential_provider/gaiacp/gaia_credential_provider.idl:
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

#ifndef __gaia_credential_provider_i_h__
#define __gaia_credential_provider_i_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IGaiaCredentialProvider_FWD_DEFINED__
#define __IGaiaCredentialProvider_FWD_DEFINED__
typedef interface IGaiaCredentialProvider IGaiaCredentialProvider;

#endif 	/* __IGaiaCredentialProvider_FWD_DEFINED__ */


#ifndef __IGaiaCredentialProviderForTesting_FWD_DEFINED__
#define __IGaiaCredentialProviderForTesting_FWD_DEFINED__
typedef interface IGaiaCredentialProviderForTesting IGaiaCredentialProviderForTesting;

#endif 	/* __IGaiaCredentialProviderForTesting_FWD_DEFINED__ */


#ifndef __IGaiaCredential_FWD_DEFINED__
#define __IGaiaCredential_FWD_DEFINED__
typedef interface IGaiaCredential IGaiaCredential;

#endif 	/* __IGaiaCredential_FWD_DEFINED__ */


#ifndef __IReauthCredential_FWD_DEFINED__
#define __IReauthCredential_FWD_DEFINED__
typedef interface IReauthCredential IReauthCredential;

#endif 	/* __IReauthCredential_FWD_DEFINED__ */


#ifndef __GaiaCredentialProvider_FWD_DEFINED__
#define __GaiaCredentialProvider_FWD_DEFINED__

#ifdef __cplusplus
typedef class GaiaCredentialProvider GaiaCredentialProvider;
#else
typedef struct GaiaCredentialProvider GaiaCredentialProvider;
#endif /* __cplusplus */

#endif 	/* __GaiaCredentialProvider_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __IGaiaCredentialProvider_INTERFACE_DEFINED__
#define __IGaiaCredentialProvider_INTERFACE_DEFINED__

/* interface IGaiaCredentialProvider */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IGaiaCredentialProvider;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("CEC9EF6C-B2E6-4BB6-8F1E-1747BA4F7138")
    IGaiaCredentialProvider : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE OnUserAuthenticated( 
            /* [in] */ IUnknown *credential,
            /* [in] */ BSTR username,
            /* [in] */ BSTR password,
            /* [in] */ BSTR sid) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IGaiaCredentialProviderVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IGaiaCredentialProvider * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IGaiaCredentialProvider * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IGaiaCredentialProvider * This);
        
        HRESULT ( STDMETHODCALLTYPE *OnUserAuthenticated )( 
            IGaiaCredentialProvider * This,
            /* [in] */ IUnknown *credential,
            /* [in] */ BSTR username,
            /* [in] */ BSTR password,
            /* [in] */ BSTR sid);
        
        END_INTERFACE
    } IGaiaCredentialProviderVtbl;

    interface IGaiaCredentialProvider
    {
        CONST_VTBL struct IGaiaCredentialProviderVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IGaiaCredentialProvider_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IGaiaCredentialProvider_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IGaiaCredentialProvider_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IGaiaCredentialProvider_OnUserAuthenticated(This,credential,username,password,sid)	\
    ( (This)->lpVtbl -> OnUserAuthenticated(This,credential,username,password,sid) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IGaiaCredentialProvider_INTERFACE_DEFINED__ */


#ifndef __IGaiaCredentialProviderForTesting_INTERFACE_DEFINED__
#define __IGaiaCredentialProviderForTesting_INTERFACE_DEFINED__

/* interface IGaiaCredentialProviderForTesting */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IGaiaCredentialProviderForTesting;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("224CE2FB-2977-4585-BD46-1BAE8D7964DE")
    IGaiaCredentialProviderForTesting : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE SetReauthCheckDoneEvent( 
            /* [in] */ INT_PTR event) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IGaiaCredentialProviderForTestingVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IGaiaCredentialProviderForTesting * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IGaiaCredentialProviderForTesting * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IGaiaCredentialProviderForTesting * This);
        
        HRESULT ( STDMETHODCALLTYPE *SetReauthCheckDoneEvent )( 
            IGaiaCredentialProviderForTesting * This,
            /* [in] */ INT_PTR event);
        
        END_INTERFACE
    } IGaiaCredentialProviderForTestingVtbl;

    interface IGaiaCredentialProviderForTesting
    {
        CONST_VTBL struct IGaiaCredentialProviderForTestingVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IGaiaCredentialProviderForTesting_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IGaiaCredentialProviderForTesting_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IGaiaCredentialProviderForTesting_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IGaiaCredentialProviderForTesting_SetReauthCheckDoneEvent(This,event)	\
    ( (This)->lpVtbl -> SetReauthCheckDoneEvent(This,event) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IGaiaCredentialProviderForTesting_INTERFACE_DEFINED__ */


#ifndef __IGaiaCredential_INTERFACE_DEFINED__
#define __IGaiaCredential_INTERFACE_DEFINED__

/* interface IGaiaCredential */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IGaiaCredential;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("E5BF88DF-9966-465B-B233-C1CAC7510A59")
    IGaiaCredential : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( 
            /* [in] */ IGaiaCredentialProvider *provider) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Terminate( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE FinishAuthentication( 
            /* [in] */ BSTR username,
            /* [in] */ BSTR password,
            /* [in] */ BSTR fullname,
            /* [out] */ BSTR *sid,
            /* [out] */ BSTR *error_text) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnUserAuthenticated( 
            /* [in] */ BSTR username,
            /* [in] */ BSTR password,
            /* [in] */ BSTR sid) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ReportError( 
            /* [in] */ LONG status,
            /* [in] */ LONG substatus,
            /* [in] */ BSTR status_text) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IGaiaCredentialVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IGaiaCredential * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IGaiaCredential * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IGaiaCredential * This);
        
        HRESULT ( STDMETHODCALLTYPE *Initialize )( 
            IGaiaCredential * This,
            /* [in] */ IGaiaCredentialProvider *provider);
        
        HRESULT ( STDMETHODCALLTYPE *Terminate )( 
            IGaiaCredential * This);
        
        HRESULT ( STDMETHODCALLTYPE *FinishAuthentication )( 
            IGaiaCredential * This,
            /* [in] */ BSTR username,
            /* [in] */ BSTR password,
            /* [in] */ BSTR fullname,
            /* [out] */ BSTR *sid,
            /* [out] */ BSTR *error_text);
        
        HRESULT ( STDMETHODCALLTYPE *OnUserAuthenticated )( 
            IGaiaCredential * This,
            /* [in] */ BSTR username,
            /* [in] */ BSTR password,
            /* [in] */ BSTR sid);
        
        HRESULT ( STDMETHODCALLTYPE *ReportError )( 
            IGaiaCredential * This,
            /* [in] */ LONG status,
            /* [in] */ LONG substatus,
            /* [in] */ BSTR status_text);
        
        END_INTERFACE
    } IGaiaCredentialVtbl;

    interface IGaiaCredential
    {
        CONST_VTBL struct IGaiaCredentialVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IGaiaCredential_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IGaiaCredential_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IGaiaCredential_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IGaiaCredential_Initialize(This,provider)	\
    ( (This)->lpVtbl -> Initialize(This,provider) ) 

#define IGaiaCredential_Terminate(This)	\
    ( (This)->lpVtbl -> Terminate(This) ) 

#define IGaiaCredential_FinishAuthentication(This,username,password,fullname,sid,error_text)	\
    ( (This)->lpVtbl -> FinishAuthentication(This,username,password,fullname,sid,error_text) ) 

#define IGaiaCredential_OnUserAuthenticated(This,username,password,sid)	\
    ( (This)->lpVtbl -> OnUserAuthenticated(This,username,password,sid) ) 

#define IGaiaCredential_ReportError(This,status,substatus,status_text)	\
    ( (This)->lpVtbl -> ReportError(This,status,substatus,status_text) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IGaiaCredential_INTERFACE_DEFINED__ */


#ifndef __IReauthCredential_INTERFACE_DEFINED__
#define __IReauthCredential_INTERFACE_DEFINED__

/* interface IReauthCredential */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IReauthCredential;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("CC75BCEA-A636-4798-BF8E-0FF64D743451")
    IReauthCredential : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE SetUserInfo( 
            /* [in] */ BSTR sid,
            /* [in] */ BSTR email) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IReauthCredentialVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IReauthCredential * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IReauthCredential * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IReauthCredential * This);
        
        HRESULT ( STDMETHODCALLTYPE *SetUserInfo )( 
            IReauthCredential * This,
            /* [in] */ BSTR sid,
            /* [in] */ BSTR email);
        
        END_INTERFACE
    } IReauthCredentialVtbl;

    interface IReauthCredential
    {
        CONST_VTBL struct IReauthCredentialVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IReauthCredential_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IReauthCredential_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IReauthCredential_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IReauthCredential_SetUserInfo(This,sid,email)	\
    ( (This)->lpVtbl -> SetUserInfo(This,sid,email) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IReauthCredential_INTERFACE_DEFINED__ */



#ifndef __GaiaCredentialProviderLib_LIBRARY_DEFINED__
#define __GaiaCredentialProviderLib_LIBRARY_DEFINED__

/* library GaiaCredentialProviderLib */
/* [version][uuid] */ 


EXTERN_C const IID LIBID_GaiaCredentialProviderLib;

EXTERN_C const CLSID CLSID_GaiaCredentialProvider;

#ifdef __cplusplus

class DECLSPEC_UUID("89adae71-aee5-4ee2-bffb-e8424e06f519")
GaiaCredentialProvider;
#endif
#endif /* __GaiaCredentialProviderLib_LIBRARY_DEFINED__ */

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


