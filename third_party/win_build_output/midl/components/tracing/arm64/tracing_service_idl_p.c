

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../components/tracing/common/tracing_service_idl.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=ARM64 8.01.0628 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#if defined(_M_ARM64)


#pragma warning( disable: 4049 )  /* more than 64k source lines */
#if _MSC_VER >= 1200
#pragma warning(push)
#endif

#pragma warning( disable: 4211 )  /* redefine extern to static */
#pragma warning( disable: 4232 )  /* dllimport identity*/
#pragma warning( disable: 4024 )  /* array to pointer mapping*/
#pragma warning( disable: 4152 )  /* function/data pointer conversion in expression */

#define USE_STUBLESS_PROXY


/* verify that the <rpcproxy.h> version is high enough to compile this file*/
#ifndef __REDQ_RPCPROXY_H_VERSION__
#define __REQUIRED_RPCPROXY_H_VERSION__ 475
#endif


#include "rpcproxy.h"
#ifndef __RPCPROXY_H_VERSION__
#error this stub requires an updated version of <rpcproxy.h>
#endif /* __RPCPROXY_H_VERSION__ */


#include "tracing_service_idl.h"

#define TYPE_FORMAT_STRING_SIZE   11                                
#define PROC_FORMAT_STRING_SIZE   49                                
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   0            

typedef struct _tracing_service_idl_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } tracing_service_idl_MIDL_TYPE_FORMAT_STRING;

typedef struct _tracing_service_idl_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } tracing_service_idl_MIDL_PROC_FORMAT_STRING;

typedef struct _tracing_service_idl_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } tracing_service_idl_MIDL_EXPR_FORMAT_STRING;


static const RPC_SYNTAX_IDENTIFIER  _RpcTransferSyntax_2_0 = 
{{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}};

#if defined(_CONTROL_FLOW_GUARD_XFG)
#define XFG_TRAMPOLINES(ObjectType)\
NDR_SHAREABLE unsigned long ObjectType ## _UserSize_XFG(unsigned long * pFlags, unsigned long Offset, void * pObject)\
{\
return  ObjectType ## _UserSize(pFlags, Offset, (ObjectType *)pObject);\
}\
NDR_SHAREABLE unsigned char * ObjectType ## _UserMarshal_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserMarshal(pFlags, pBuffer, (ObjectType *)pObject);\
}\
NDR_SHAREABLE unsigned char * ObjectType ## _UserUnmarshal_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserUnmarshal(pFlags, pBuffer, (ObjectType *)pObject);\
}\
NDR_SHAREABLE void ObjectType ## _UserFree_XFG(unsigned long * pFlags, void * pObject)\
{\
ObjectType ## _UserFree(pFlags, (ObjectType *)pObject);\
}
#define XFG_TRAMPOLINES64(ObjectType)\
NDR_SHAREABLE unsigned long ObjectType ## _UserSize64_XFG(unsigned long * pFlags, unsigned long Offset, void * pObject)\
{\
return  ObjectType ## _UserSize64(pFlags, Offset, (ObjectType *)pObject);\
}\
NDR_SHAREABLE unsigned char * ObjectType ## _UserMarshal64_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserMarshal64(pFlags, pBuffer, (ObjectType *)pObject);\
}\
NDR_SHAREABLE unsigned char * ObjectType ## _UserUnmarshal64_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserUnmarshal64(pFlags, pBuffer, (ObjectType *)pObject);\
}\
NDR_SHAREABLE void ObjectType ## _UserFree64_XFG(unsigned long * pFlags, void * pObject)\
{\
ObjectType ## _UserFree64(pFlags, (ObjectType *)pObject);\
}
#define XFG_BIND_TRAMPOLINES(HandleType, ObjectType)\
static void* ObjectType ## _bind_XFG(HandleType pObject)\
{\
return ObjectType ## _bind((ObjectType) pObject);\
}\
static void ObjectType ## _unbind_XFG(HandleType pObject, handle_t ServerHandle)\
{\
ObjectType ## _unbind((ObjectType) pObject, ServerHandle);\
}
#define XFG_TRAMPOLINE_FPTR(Function) Function ## _XFG
#define XFG_TRAMPOLINE_FPTR_DEPENDENT_SYMBOL(Symbol) Symbol ## _XFG
#else
#define XFG_TRAMPOLINES(ObjectType)
#define XFG_TRAMPOLINES64(ObjectType)
#define XFG_BIND_TRAMPOLINES(HandleType, ObjectType)
#define XFG_TRAMPOLINE_FPTR(Function) Function
#define XFG_TRAMPOLINE_FPTR_DEPENDENT_SYMBOL(Symbol) Symbol
#endif


extern const tracing_service_idl_MIDL_TYPE_FORMAT_STRING tracing_service_idl__MIDL_TypeFormatString;
extern const tracing_service_idl_MIDL_PROC_FORMAT_STRING tracing_service_idl__MIDL_ProcFormatString;
extern const tracing_service_idl_MIDL_EXPR_FORMAT_STRING tracing_service_idl__MIDL_ExprFormatString;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO ISystemTraceSession_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ISystemTraceSession_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO ISystemTraceSessionChromium_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ISystemTraceSessionChromium_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO ISystemTraceSessionChrome_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ISystemTraceSessionChrome_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO ISystemTraceSessionChromeBeta_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ISystemTraceSessionChromeBeta_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO ISystemTraceSessionChromeDev_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ISystemTraceSessionChromeDev_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO ISystemTraceSessionChromeCanary_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ISystemTraceSessionChromeCanary_ProxyInfo;



#if !defined(__RPC_ARM64__)
#error  Invalid build platform for this stub.
#endif

static const tracing_service_idl_MIDL_PROC_FORMAT_STRING tracing_service_idl__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure AcceptInvitation */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x3 ),	/* 3 */
/*  8 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 10 */	NdrFcShort( 0x0 ),	/* 0 */
/* 12 */	NdrFcShort( 0x24 ),	/* 36 */
/* 14 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 16 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 18 */	NdrFcShort( 0x0 ),	/* 0 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x3 ),	/* 3 */
/* 26 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 28 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter server_name */

/* 30 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 32 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 34 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter pid */

/* 36 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 38 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 40 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 42 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 44 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 46 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const tracing_service_idl_MIDL_TYPE_FORMAT_STRING tracing_service_idl__MIDL_TypeFormatString =
    {
        0,
        {
			NdrFcShort( 0x0 ),	/* 0 */
/*  2 */	
			0x11, 0x8,	/* FC_RP [simple_pointer] */
/*  4 */	
			0x25,		/* FC_C_WSTRING */
			0x5c,		/* FC_PAD */
/*  6 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/*  8 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */

			0x0
        }
    };


/* Object interface: IUnknown, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: ISystemTraceSession, ver. 0.0,
   GUID={0xDB01E5CE,0x10CE,0x4A84,{0x8F,0xAE,0xDA,0x5E,0x46,0xEE,0xF1,0xCF}} */

#pragma code_seg(".orpc")
static const unsigned short ISystemTraceSession_FormatStringOffsetTable[] =
    {
    0
    };

static const MIDL_STUBLESS_PROXY_INFO ISystemTraceSession_ProxyInfo =
    {
    &Object_StubDesc,
    tracing_service_idl__MIDL_ProcFormatString.Format,
    &ISystemTraceSession_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ISystemTraceSession_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    tracing_service_idl__MIDL_ProcFormatString.Format,
    &ISystemTraceSession_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _ISystemTraceSessionProxyVtbl = 
{
    &ISystemTraceSession_ProxyInfo,
    &IID_ISystemTraceSession,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* ISystemTraceSession::AcceptInvitation */
};

const CInterfaceStubVtbl _ISystemTraceSessionStubVtbl =
{
    &IID_ISystemTraceSession,
    &ISystemTraceSession_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Standard interface: __MIDL_itf_tracing_service_idl_0000_0001, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: ISystemTraceSessionChromium, ver. 0.0,
   GUID={0xA3FD580A,0xFFD4,0x4075,{0x91,0x74,0x75,0xD0,0xB1,0x99,0xD3,0xCB}} */

#pragma code_seg(".orpc")
static const unsigned short ISystemTraceSessionChromium_FormatStringOffsetTable[] =
    {
    0,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO ISystemTraceSessionChromium_ProxyInfo =
    {
    &Object_StubDesc,
    tracing_service_idl__MIDL_ProcFormatString.Format,
    &ISystemTraceSessionChromium_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ISystemTraceSessionChromium_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    tracing_service_idl__MIDL_ProcFormatString.Format,
    &ISystemTraceSessionChromium_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _ISystemTraceSessionChromiumProxyVtbl = 
{
    0,
    &IID_ISystemTraceSessionChromium,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation ISystemTraceSession::AcceptInvitation */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION ISystemTraceSessionChromium_table[] =
{
    NdrStubCall2
};

CInterfaceStubVtbl _ISystemTraceSessionChromiumStubVtbl =
{
    &IID_ISystemTraceSessionChromium,
    &ISystemTraceSessionChromium_ServerInfo,
    4,
    &ISystemTraceSessionChromium_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: ISystemTraceSessionChrome, ver. 0.0,
   GUID={0x056B3371,0x1C09,0x475B,{0xA8,0xD7,0x9E,0x58,0xBF,0x45,0x53,0x3E}} */

#pragma code_seg(".orpc")
static const unsigned short ISystemTraceSessionChrome_FormatStringOffsetTable[] =
    {
    0,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO ISystemTraceSessionChrome_ProxyInfo =
    {
    &Object_StubDesc,
    tracing_service_idl__MIDL_ProcFormatString.Format,
    &ISystemTraceSessionChrome_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ISystemTraceSessionChrome_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    tracing_service_idl__MIDL_ProcFormatString.Format,
    &ISystemTraceSessionChrome_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _ISystemTraceSessionChromeProxyVtbl = 
{
    0,
    &IID_ISystemTraceSessionChrome,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation ISystemTraceSession::AcceptInvitation */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION ISystemTraceSessionChrome_table[] =
{
    NdrStubCall2
};

CInterfaceStubVtbl _ISystemTraceSessionChromeStubVtbl =
{
    &IID_ISystemTraceSessionChrome,
    &ISystemTraceSessionChrome_ServerInfo,
    4,
    &ISystemTraceSessionChrome_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: ISystemTraceSessionChromeBeta, ver. 0.0,
   GUID={0xA69D7D7D,0x9A08,0x422A,{0xB6,0xC6,0xB7,0xB8,0xD3,0x76,0xA1,0x2C}} */

#pragma code_seg(".orpc")
static const unsigned short ISystemTraceSessionChromeBeta_FormatStringOffsetTable[] =
    {
    0,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO ISystemTraceSessionChromeBeta_ProxyInfo =
    {
    &Object_StubDesc,
    tracing_service_idl__MIDL_ProcFormatString.Format,
    &ISystemTraceSessionChromeBeta_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ISystemTraceSessionChromeBeta_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    tracing_service_idl__MIDL_ProcFormatString.Format,
    &ISystemTraceSessionChromeBeta_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _ISystemTraceSessionChromeBetaProxyVtbl = 
{
    0,
    &IID_ISystemTraceSessionChromeBeta,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation ISystemTraceSession::AcceptInvitation */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION ISystemTraceSessionChromeBeta_table[] =
{
    NdrStubCall2
};

CInterfaceStubVtbl _ISystemTraceSessionChromeBetaStubVtbl =
{
    &IID_ISystemTraceSessionChromeBeta,
    &ISystemTraceSessionChromeBeta_ServerInfo,
    4,
    &ISystemTraceSessionChromeBeta_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: ISystemTraceSessionChromeDev, ver. 0.0,
   GUID={0xE08ADAE8,0x9334,0x46ED,{0xB0,0xCF,0xDD,0x17,0x80,0x15,0x8D,0x55}} */

#pragma code_seg(".orpc")
static const unsigned short ISystemTraceSessionChromeDev_FormatStringOffsetTable[] =
    {
    0,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO ISystemTraceSessionChromeDev_ProxyInfo =
    {
    &Object_StubDesc,
    tracing_service_idl__MIDL_ProcFormatString.Format,
    &ISystemTraceSessionChromeDev_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ISystemTraceSessionChromeDev_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    tracing_service_idl__MIDL_ProcFormatString.Format,
    &ISystemTraceSessionChromeDev_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _ISystemTraceSessionChromeDevProxyVtbl = 
{
    0,
    &IID_ISystemTraceSessionChromeDev,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation ISystemTraceSession::AcceptInvitation */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION ISystemTraceSessionChromeDev_table[] =
{
    NdrStubCall2
};

CInterfaceStubVtbl _ISystemTraceSessionChromeDevStubVtbl =
{
    &IID_ISystemTraceSessionChromeDev,
    &ISystemTraceSessionChromeDev_ServerInfo,
    4,
    &ISystemTraceSessionChromeDev_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: ISystemTraceSessionChromeCanary, ver. 0.0,
   GUID={0x6EFB8558,0x68D1,0x4826,{0xA6,0x12,0xA1,0x80,0xB3,0x57,0x03,0x75}} */

#pragma code_seg(".orpc")
static const unsigned short ISystemTraceSessionChromeCanary_FormatStringOffsetTable[] =
    {
    0,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO ISystemTraceSessionChromeCanary_ProxyInfo =
    {
    &Object_StubDesc,
    tracing_service_idl__MIDL_ProcFormatString.Format,
    &ISystemTraceSessionChromeCanary_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ISystemTraceSessionChromeCanary_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    tracing_service_idl__MIDL_ProcFormatString.Format,
    &ISystemTraceSessionChromeCanary_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _ISystemTraceSessionChromeCanaryProxyVtbl = 
{
    0,
    &IID_ISystemTraceSessionChromeCanary,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation ISystemTraceSession::AcceptInvitation */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION ISystemTraceSessionChromeCanary_table[] =
{
    NdrStubCall2
};

CInterfaceStubVtbl _ISystemTraceSessionChromeCanaryStubVtbl =
{
    &IID_ISystemTraceSessionChromeCanary,
    &ISystemTraceSessionChromeCanary_ServerInfo,
    4,
    &ISystemTraceSessionChromeCanary_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};

#ifdef __cplusplus
namespace {
#endif
static const MIDL_STUB_DESC Object_StubDesc = 
    {
    0,
    NdrOleAllocate,
    NdrOleFree,
    0,
    0,
    0,
    0,
    0,
    tracing_service_idl__MIDL_TypeFormatString.Format,
    1, /* -error bounds_check flag */
    0x50002, /* Ndr library version */
    0,
    0x8010274, /* MIDL Version 8.1.628 */
    0,
    0,
    0,  /* notify & notify_flag routine table */
    0x1, /* MIDL flag */
    0, /* cs routines */
    0,   /* proxy/server info */
    0
    };
#ifdef __cplusplus
}
#endif

const CInterfaceProxyVtbl * const _tracing_service_idl_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_ISystemTraceSessionChromiumProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ISystemTraceSessionChromeCanaryProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ISystemTraceSessionChromeProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ISystemTraceSessionChromeBetaProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ISystemTraceSessionProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ISystemTraceSessionChromeDevProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _tracing_service_idl_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_ISystemTraceSessionChromiumStubVtbl,
    ( CInterfaceStubVtbl *) &_ISystemTraceSessionChromeCanaryStubVtbl,
    ( CInterfaceStubVtbl *) &_ISystemTraceSessionChromeStubVtbl,
    ( CInterfaceStubVtbl *) &_ISystemTraceSessionChromeBetaStubVtbl,
    ( CInterfaceStubVtbl *) &_ISystemTraceSessionStubVtbl,
    ( CInterfaceStubVtbl *) &_ISystemTraceSessionChromeDevStubVtbl,
    0
};

PCInterfaceName const _tracing_service_idl_InterfaceNamesList[] = 
{
    "ISystemTraceSessionChromium",
    "ISystemTraceSessionChromeCanary",
    "ISystemTraceSessionChrome",
    "ISystemTraceSessionChromeBeta",
    "ISystemTraceSession",
    "ISystemTraceSessionChromeDev",
    0
};

const IID *  const _tracing_service_idl_BaseIIDList[] = 
{
    &IID_ISystemTraceSession,   /* forced */
    &IID_ISystemTraceSession,   /* forced */
    &IID_ISystemTraceSession,   /* forced */
    &IID_ISystemTraceSession,   /* forced */
    0,
    &IID_ISystemTraceSession,   /* forced */
    0
};


#define _tracing_service_idl_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _tracing_service_idl, pIID, n)

int __stdcall _tracing_service_idl_IID_Lookup( const IID * pIID, int * pIndex )
{
    IID_BS_LOOKUP_SETUP

    IID_BS_LOOKUP_INITIAL_TEST( _tracing_service_idl, 6, 4 )
    IID_BS_LOOKUP_NEXT_TEST( _tracing_service_idl, 2 )
    IID_BS_LOOKUP_NEXT_TEST( _tracing_service_idl, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _tracing_service_idl, 6, *pIndex )
    
}

EXTERN_C const ExtendedProxyFileInfo tracing_service_idl_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _tracing_service_idl_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _tracing_service_idl_StubVtblList,
    (const PCInterfaceName * ) & _tracing_service_idl_InterfaceNamesList,
    (const IID ** ) & _tracing_service_idl_BaseIIDList,
    & _tracing_service_idl_IID_Lookup, 
    6,
    2,
    0, /* table of [async_uuid] interfaces */
    0, /* Filler1 */
    0, /* Filler2 */
    0  /* Filler3 */
};
#if _MSC_VER >= 1200
#pragma warning(pop)
#endif


#endif /* defined(_M_ARM64) */

