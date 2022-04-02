

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../chrome/elevation_service/elevation_service_idl.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.xx.xxxx 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#if defined(_M_AMD64)


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


#include "elevation_service_idl.h"

#define TYPE_FORMAT_STRING_SIZE   11                                
#define PROC_FORMAT_STRING_SIZE   69                                
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   0            

typedef struct _elevation_service_idl_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } elevation_service_idl_MIDL_TYPE_FORMAT_STRING;

typedef struct _elevation_service_idl_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } elevation_service_idl_MIDL_PROC_FORMAT_STRING;

typedef struct _elevation_service_idl_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } elevation_service_idl_MIDL_EXPR_FORMAT_STRING;


static const RPC_SYNTAX_IDENTIFIER  _RpcTransferSyntax = 
{{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}};

#if defined(_CONTROL_FLOW_GUARD_XFG)
#define XFG_TRAMPOLINES(ObjectType)\
static unsigned long ObjectType ## _UserSize_XFG(unsigned long * pFlags, unsigned long Offset, void * pObject)\
{\
return  ObjectType ## _UserSize(pFlags, Offset, pObject);\
}\
static unsigned char * ObjectType ## _UserMarshal_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserMarshal(pFlags, pBuffer, pObject);\
}\
static unsigned char * ObjectType ## _UserUnmarshal_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserUnmarshal(pFlags, pBuffer, pObject);\
}\
static void ObjectType ## _UserFree_XFG(unsigned long * pFlags, void * pObject)\
{\
ObjectType ## _UserFree(pFlags, pObject);\
}
#define XFG_TRAMPOLINES64(ObjectType)\
static unsigned long ObjectType ## _UserSize64_XFG(unsigned long * pFlags, unsigned long Offset, void * pObject)\
{\
return  ObjectType ## _UserSize64(pFlags, Offset, pObject);\
}\
static unsigned char * ObjectType ## _UserMarshal64_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserMarshal64(pFlags, pBuffer, pObject);\
}\
static unsigned char * ObjectType ## _UserUnmarshal64_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserUnmarshal64(pFlags, pBuffer, pObject);\
}\
static void ObjectType ## _UserFree64_XFG(unsigned long * pFlags, void * pObject)\
{\
ObjectType ## _UserFree64(pFlags, pObject);\
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
#else
#define XFG_TRAMPOLINES(ObjectType)
#define XFG_TRAMPOLINES64(ObjectType)
#define XFG_BIND_TRAMPOLINES(HandleType, ObjectType)
#define XFG_TRAMPOLINE_FPTR(Function) Function
#endif


extern const elevation_service_idl_MIDL_TYPE_FORMAT_STRING elevation_service_idl__MIDL_TypeFormatString;
extern const elevation_service_idl_MIDL_PROC_FORMAT_STRING elevation_service_idl__MIDL_ProcFormatString;
extern const elevation_service_idl_MIDL_EXPR_FORMAT_STRING elevation_service_idl__MIDL_ExprFormatString;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IElevator_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevator_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IElevatorChromium_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevatorChromium_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IElevatorChrome_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevatorChrome_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IElevatorChromeBeta_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevatorChromeBeta_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IElevatorChromeDev_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevatorChromeDev_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IElevatorChromeCanary_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevatorChromeCanary_ProxyInfo;



#if !defined(__RPC_WIN64__)
#error  Invalid build platform for this stub.
#endif

static const elevation_service_idl_MIDL_PROC_FORMAT_STRING elevation_service_idl__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure RunRecoveryCRXElevated */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x3 ),	/* 3 */
/*  8 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 10 */	NdrFcShort( 0x8 ),	/* 8 */
/* 12 */	NdrFcShort( 0x24 ),	/* 36 */
/* 14 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 16 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 18 */	NdrFcShort( 0x0 ),	/* 0 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter crx_path */

/* 26 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 28 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 30 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter browser_appid */

/* 32 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 34 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 36 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter browser_version */

/* 38 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 40 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 42 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter session_id */

/* 44 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 46 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 48 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter caller_proc_id */

/* 50 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 52 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 54 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter proc_handle */

/* 56 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 58 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 60 */	0xb9,		/* FC_UINT3264 */
			0x0,		/* 0 */

	/* Return value */

/* 62 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 64 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 66 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const elevation_service_idl_MIDL_TYPE_FORMAT_STRING elevation_service_idl__MIDL_TypeFormatString =
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
/*  8 */	0xb9,		/* FC_UINT3264 */
			0x5c,		/* FC_PAD */

			0x0
        }
    };


/* Object interface: IUnknown, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IElevator, ver. 0.0,
   GUID={0xA949CB4E,0xC4F9,0x44C4,{0xB2,0x13,0x6B,0xF8,0xAA,0x9A,0xC6,0x9C}} */

#pragma code_seg(".orpc")
static const unsigned short IElevator_FormatStringOffsetTable[] =
    {
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevator_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevator_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IElevatorProxyVtbl = 
{
    &IElevator_ProxyInfo,
    &IID_IElevator,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IElevator::RunRecoveryCRXElevated */
};

const CInterfaceStubVtbl _IElevatorStubVtbl =
{
    &IID_IElevator,
    &IElevator_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IElevatorChromium, ver. 0.0,
   GUID={0xB88C45B9,0x8825,0x4629,{0xB8,0x3E,0x77,0xCC,0x67,0xD9,0xCE,0xED}} */

#pragma code_seg(".orpc")
static const unsigned short IElevatorChromium_FormatStringOffsetTable[] =
    {
    0,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevatorChromium_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromium_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevatorChromium_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromium_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IElevatorChromiumProxyVtbl = 
{
    0,
    &IID_IElevatorChromium,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IElevator::RunRecoveryCRXElevated */
};


static const PRPC_STUB_FUNCTION IElevatorChromium_table[] =
{
    NdrStubCall2
};

CInterfaceStubVtbl _IElevatorChromiumStubVtbl =
{
    &IID_IElevatorChromium,
    &IElevatorChromium_ServerInfo,
    4,
    &IElevatorChromium_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IElevatorChrome, ver. 0.0,
   GUID={0x463ABECF,0x410D,0x407F,{0x8A,0xF5,0x0D,0xF3,0x5A,0x00,0x5C,0xC8}} */

#pragma code_seg(".orpc")
static const unsigned short IElevatorChrome_FormatStringOffsetTable[] =
    {
    0,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevatorChrome_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChrome_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevatorChrome_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChrome_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IElevatorChromeProxyVtbl = 
{
    0,
    &IID_IElevatorChrome,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IElevator::RunRecoveryCRXElevated */
};


static const PRPC_STUB_FUNCTION IElevatorChrome_table[] =
{
    NdrStubCall2
};

CInterfaceStubVtbl _IElevatorChromeStubVtbl =
{
    &IID_IElevatorChrome,
    &IElevatorChrome_ServerInfo,
    4,
    &IElevatorChrome_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IElevatorChromeBeta, ver. 0.0,
   GUID={0xA2721D66,0x376E,0x4D2F,{0x9F,0x0F,0x90,0x70,0xE9,0xA4,0x2B,0x5F}} */

#pragma code_seg(".orpc")
static const unsigned short IElevatorChromeBeta_FormatStringOffsetTable[] =
    {
    0,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevatorChromeBeta_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromeBeta_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevatorChromeBeta_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromeBeta_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IElevatorChromeBetaProxyVtbl = 
{
    0,
    &IID_IElevatorChromeBeta,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IElevator::RunRecoveryCRXElevated */
};


static const PRPC_STUB_FUNCTION IElevatorChromeBeta_table[] =
{
    NdrStubCall2
};

CInterfaceStubVtbl _IElevatorChromeBetaStubVtbl =
{
    &IID_IElevatorChromeBeta,
    &IElevatorChromeBeta_ServerInfo,
    4,
    &IElevatorChromeBeta_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IElevatorChromeDev, ver. 0.0,
   GUID={0xBB2AA26B,0x343A,0x4072,{0x8B,0x6F,0x80,0x55,0x7B,0x8C,0xE5,0x71}} */

#pragma code_seg(".orpc")
static const unsigned short IElevatorChromeDev_FormatStringOffsetTable[] =
    {
    0,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevatorChromeDev_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromeDev_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevatorChromeDev_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromeDev_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IElevatorChromeDevProxyVtbl = 
{
    0,
    &IID_IElevatorChromeDev,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IElevator::RunRecoveryCRXElevated */
};


static const PRPC_STUB_FUNCTION IElevatorChromeDev_table[] =
{
    NdrStubCall2
};

CInterfaceStubVtbl _IElevatorChromeDevStubVtbl =
{
    &IID_IElevatorChromeDev,
    &IElevatorChromeDev_ServerInfo,
    4,
    &IElevatorChromeDev_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IElevatorChromeCanary, ver. 0.0,
   GUID={0x4F7CE041,0x28E9,0x484F,{0x9D,0xD0,0x61,0xA8,0xCA,0xCE,0xFE,0xE4}} */

#pragma code_seg(".orpc")
static const unsigned short IElevatorChromeCanary_FormatStringOffsetTable[] =
    {
    0,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevatorChromeCanary_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromeCanary_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevatorChromeCanary_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromeCanary_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IElevatorChromeCanaryProxyVtbl = 
{
    0,
    &IID_IElevatorChromeCanary,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IElevator::RunRecoveryCRXElevated */
};


static const PRPC_STUB_FUNCTION IElevatorChromeCanary_table[] =
{
    NdrStubCall2
};

CInterfaceStubVtbl _IElevatorChromeCanaryStubVtbl =
{
    &IID_IElevatorChromeCanary,
    &IElevatorChromeCanary_ServerInfo,
    4,
    &IElevatorChromeCanary_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};

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
    elevation_service_idl__MIDL_TypeFormatString.Format,
    1, /* -error bounds_check flag */
    0x50002, /* Ndr library version */
    0,
    0x8010272, /* MIDL Version 8.1.626 */
    0,
    0,
    0,  /* notify & notify_flag routine table */
    0x1, /* MIDL flag */
    0, /* cs routines */
    0,   /* proxy/server info */
    0
    };

const CInterfaceProxyVtbl * const _elevation_service_idl_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_IElevatorChromeCanaryProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevatorProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevatorChromeBetaProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevatorChromeDevProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevatorChromiumProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevatorChromeProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _elevation_service_idl_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_IElevatorChromeCanaryStubVtbl,
    ( CInterfaceStubVtbl *) &_IElevatorStubVtbl,
    ( CInterfaceStubVtbl *) &_IElevatorChromeBetaStubVtbl,
    ( CInterfaceStubVtbl *) &_IElevatorChromeDevStubVtbl,
    ( CInterfaceStubVtbl *) &_IElevatorChromiumStubVtbl,
    ( CInterfaceStubVtbl *) &_IElevatorChromeStubVtbl,
    0
};

PCInterfaceName const _elevation_service_idl_InterfaceNamesList[] = 
{
    "IElevatorChromeCanary",
    "IElevator",
    "IElevatorChromeBeta",
    "IElevatorChromeDev",
    "IElevatorChromium",
    "IElevatorChrome",
    0
};

const IID *  const _elevation_service_idl_BaseIIDList[] = 
{
    &IID_IElevator,   /* forced */
    0,
    &IID_IElevator,   /* forced */
    &IID_IElevator,   /* forced */
    &IID_IElevator,   /* forced */
    &IID_IElevator,   /* forced */
    0
};


#define _elevation_service_idl_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _elevation_service_idl, pIID, n)

int __stdcall _elevation_service_idl_IID_Lookup( const IID * pIID, int * pIndex )
{
    IID_BS_LOOKUP_SETUP

    IID_BS_LOOKUP_INITIAL_TEST( _elevation_service_idl, 6, 4 )
    IID_BS_LOOKUP_NEXT_TEST( _elevation_service_idl, 2 )
    IID_BS_LOOKUP_NEXT_TEST( _elevation_service_idl, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _elevation_service_idl, 6, *pIndex )
    
}

const ExtendedProxyFileInfo elevation_service_idl_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _elevation_service_idl_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _elevation_service_idl_StubVtblList,
    (const PCInterfaceName * ) & _elevation_service_idl_InterfaceNamesList,
    (const IID ** ) & _elevation_service_idl_BaseIIDList,
    & _elevation_service_idl_IID_Lookup, 
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


#endif /* defined(_M_AMD64)*/

