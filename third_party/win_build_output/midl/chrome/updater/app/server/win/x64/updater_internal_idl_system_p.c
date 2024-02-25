

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for gen/chrome/updater/app/server/win/updater_internal_idl_system.idl:
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


#include "updater_internal_idl_system.h"

#define TYPE_FORMAT_STRING_SIZE   39                                
#define PROC_FORMAT_STRING_SIZE   191                               
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   0            

typedef struct _updater_internal_idl_system_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } updater_internal_idl_system_MIDL_TYPE_FORMAT_STRING;

typedef struct _updater_internal_idl_system_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } updater_internal_idl_system_MIDL_PROC_FORMAT_STRING;

typedef struct _updater_internal_idl_system_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } updater_internal_idl_system_MIDL_EXPR_FORMAT_STRING;


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


extern const updater_internal_idl_system_MIDL_TYPE_FORMAT_STRING updater_internal_idl_system__MIDL_TypeFormatString;
extern const updater_internal_idl_system_MIDL_PROC_FORMAT_STRING updater_internal_idl_system__MIDL_ProcFormatString;
extern const updater_internal_idl_system_MIDL_EXPR_FORMAT_STRING updater_internal_idl_system__MIDL_ExprFormatString;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterInternalCallback_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterInternalCallback_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterInternalCallbackSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterInternalCallbackSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterInternal_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterInternal_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterInternalSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterInternalSystem_ProxyInfo;



#if !defined(__RPC_WIN64__)
#error  Invalid build platform for this stub.
#endif

static const updater_internal_idl_system_MIDL_PROC_FORMAT_STRING updater_internal_idl_system__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure Run */


	/* Procedure Run */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x3 ),	/* 3 */
/*  8 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 10 */	NdrFcShort( 0x8 ),	/* 8 */
/* 12 */	NdrFcShort( 0x8 ),	/* 8 */
/* 14 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 16 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 18 */	NdrFcShort( 0x0 ),	/* 0 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter result */


	/* Parameter result */

/* 26 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 28 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 30 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 32 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 34 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 36 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Run */

/* 38 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 40 */	NdrFcLong( 0x0 ),	/* 0 */
/* 44 */	NdrFcShort( 0x3 ),	/* 3 */
/* 46 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 48 */	NdrFcShort( 0x0 ),	/* 0 */
/* 50 */	NdrFcShort( 0x8 ),	/* 8 */
/* 52 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 54 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 56 */	NdrFcShort( 0x0 ),	/* 0 */
/* 58 */	NdrFcShort( 0x0 ),	/* 0 */
/* 60 */	NdrFcShort( 0x0 ),	/* 0 */
/* 62 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 64 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 66 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 68 */	NdrFcShort( 0x2 ),	/* Type Offset=2 */

	/* Return value */

/* 70 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 72 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 74 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Hello */

/* 76 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 78 */	NdrFcLong( 0x0 ),	/* 0 */
/* 82 */	NdrFcShort( 0x4 ),	/* 4 */
/* 84 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 86 */	NdrFcShort( 0x0 ),	/* 0 */
/* 88 */	NdrFcShort( 0x8 ),	/* 8 */
/* 90 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 92 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 94 */	NdrFcShort( 0x0 ),	/* 0 */
/* 96 */	NdrFcShort( 0x0 ),	/* 0 */
/* 98 */	NdrFcShort( 0x0 ),	/* 0 */
/* 100 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 102 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 104 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 106 */	NdrFcShort( 0x2 ),	/* Type Offset=2 */

	/* Return value */

/* 108 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 110 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 112 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Run */

/* 114 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 116 */	NdrFcLong( 0x0 ),	/* 0 */
/* 120 */	NdrFcShort( 0x3 ),	/* 3 */
/* 122 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 124 */	NdrFcShort( 0x0 ),	/* 0 */
/* 126 */	NdrFcShort( 0x8 ),	/* 8 */
/* 128 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 130 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 132 */	NdrFcShort( 0x0 ),	/* 0 */
/* 134 */	NdrFcShort( 0x0 ),	/* 0 */
/* 136 */	NdrFcShort( 0x0 ),	/* 0 */
/* 138 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 140 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 142 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 144 */	NdrFcShort( 0x14 ),	/* Type Offset=20 */

	/* Return value */

/* 146 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 148 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 150 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Hello */

/* 152 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 154 */	NdrFcLong( 0x0 ),	/* 0 */
/* 158 */	NdrFcShort( 0x4 ),	/* 4 */
/* 160 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 162 */	NdrFcShort( 0x0 ),	/* 0 */
/* 164 */	NdrFcShort( 0x8 ),	/* 8 */
/* 166 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 168 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 170 */	NdrFcShort( 0x0 ),	/* 0 */
/* 172 */	NdrFcShort( 0x0 ),	/* 0 */
/* 174 */	NdrFcShort( 0x0 ),	/* 0 */
/* 176 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 178 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 180 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 182 */	NdrFcShort( 0x14 ),	/* Type Offset=20 */

	/* Return value */

/* 184 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 186 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 188 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const updater_internal_idl_system_MIDL_TYPE_FORMAT_STRING updater_internal_idl_system__MIDL_TypeFormatString =
    {
        0,
        {
			NdrFcShort( 0x0 ),	/* 0 */
/*  2 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/*  4 */	NdrFcLong( 0xd272c794 ),	/* -764229740 */
/*  8 */	NdrFcShort( 0x2ace ),	/* 10958 */
/* 10 */	NdrFcShort( 0x4584 ),	/* 17796 */
/* 12 */	0xb9,		/* 185 */
			0x93,		/* 147 */
/* 14 */	0x3b,		/* 59 */
			0x90,		/* 144 */
/* 16 */	0xc6,		/* 198 */
			0x22,		/* 34 */
/* 18 */	0xbe,		/* 190 */
			0x65,		/* 101 */
/* 20 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 22 */	NdrFcLong( 0x7e806c73 ),	/* 2122345587 */
/* 26 */	NdrFcShort( 0xb2a4 ),	/* -19804 */
/* 28 */	NdrFcShort( 0x4bc5 ),	/* 19397 */
/* 30 */	0xbd,		/* 189 */
			0xad,		/* 173 */
/* 32 */	0x22,		/* 34 */
			0x49,		/* 73 */
/* 34 */	0xd8,		/* 216 */
			0x7f,		/* 127 */
/* 36 */	0x67,		/* 103 */
			0xfc,		/* 252 */

			0x0
        }
    };


/* Object interface: IUnknown, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IUpdaterInternalCallback, ver. 0.0,
   GUID={0xD272C794,0x2ACE,0x4584,{0xB9,0x93,0x3B,0x90,0xC6,0x22,0xBE,0x65}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterInternalCallback_FormatStringOffsetTable[] =
    {
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterInternalCallback_ProxyInfo =
    {
    &Object_StubDesc,
    updater_internal_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterInternalCallback_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterInternalCallback_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_internal_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterInternalCallback_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IUpdaterInternalCallbackProxyVtbl = 
{
    &IUpdaterInternalCallback_ProxyInfo,
    &IID_IUpdaterInternalCallback,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterInternalCallback::Run */
};

const CInterfaceStubVtbl _IUpdaterInternalCallbackStubVtbl =
{
    &IID_IUpdaterInternalCallback,
    &IUpdaterInternalCallback_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterInternalCallbackSystem, ver. 0.0,
   GUID={0x7E806C73,0xB2A4,0x4BC5,{0xBD,0xAD,0x22,0x49,0xD8,0x7F,0x67,0xFC}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterInternalCallbackSystem_FormatStringOffsetTable[] =
    {
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterInternalCallbackSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_internal_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterInternalCallbackSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterInternalCallbackSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_internal_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterInternalCallbackSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IUpdaterInternalCallbackSystemProxyVtbl = 
{
    &IUpdaterInternalCallbackSystem_ProxyInfo,
    &IID_IUpdaterInternalCallbackSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterInternalCallbackSystem::Run */
};

const CInterfaceStubVtbl _IUpdaterInternalCallbackSystemStubVtbl =
{
    &IID_IUpdaterInternalCallbackSystem,
    &IUpdaterInternalCallbackSystem_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterInternal, ver. 0.0,
   GUID={0x526DA036,0x9BD3,0x4697,{0x86,0x5A,0xDA,0x12,0xD3,0x7D,0xFF,0xCA}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterInternal_FormatStringOffsetTable[] =
    {
    38,
    76
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterInternal_ProxyInfo =
    {
    &Object_StubDesc,
    updater_internal_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterInternal_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterInternal_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_internal_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterInternal_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(5) _IUpdaterInternalProxyVtbl = 
{
    &IUpdaterInternal_ProxyInfo,
    &IID_IUpdaterInternal,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterInternal::Run */ ,
    (void *) (INT_PTR) -1 /* IUpdaterInternal::Hello */
};

const CInterfaceStubVtbl _IUpdaterInternalStubVtbl =
{
    &IID_IUpdaterInternal,
    &IUpdaterInternal_ServerInfo,
    5,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterInternalSystem, ver. 0.0,
   GUID={0xE690EB97,0x6E46,0x4361,{0xAF,0x8F,0x90,0xA4,0xF5,0x49,0x64,0x75}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterInternalSystem_FormatStringOffsetTable[] =
    {
    114,
    152
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterInternalSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_internal_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterInternalSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterInternalSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_internal_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterInternalSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(5) _IUpdaterInternalSystemProxyVtbl = 
{
    &IUpdaterInternalSystem_ProxyInfo,
    &IID_IUpdaterInternalSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterInternalSystem::Run */ ,
    (void *) (INT_PTR) -1 /* IUpdaterInternalSystem::Hello */
};

const CInterfaceStubVtbl _IUpdaterInternalSystemStubVtbl =
{
    &IID_IUpdaterInternalSystem,
    &IUpdaterInternalSystem_ServerInfo,
    5,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
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
    updater_internal_idl_system__MIDL_TypeFormatString.Format,
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

const CInterfaceProxyVtbl * const _updater_internal_idl_system_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_IUpdaterInternalProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterInternalCallbackSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterInternalCallbackProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterInternalSystemProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _updater_internal_idl_system_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_IUpdaterInternalStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterInternalCallbackSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterInternalCallbackStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterInternalSystemStubVtbl,
    0
};

PCInterfaceName const _updater_internal_idl_system_InterfaceNamesList[] = 
{
    "IUpdaterInternal",
    "IUpdaterInternalCallbackSystem",
    "IUpdaterInternalCallback",
    "IUpdaterInternalSystem",
    0
};


#define _updater_internal_idl_system_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _updater_internal_idl_system, pIID, n)

int __stdcall _updater_internal_idl_system_IID_Lookup( const IID * pIID, int * pIndex )
{
    IID_BS_LOOKUP_SETUP

    IID_BS_LOOKUP_INITIAL_TEST( _updater_internal_idl_system, 4, 2 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_internal_idl_system, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _updater_internal_idl_system, 4, *pIndex )
    
}

EXTERN_C const ExtendedProxyFileInfo updater_internal_idl_system_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _updater_internal_idl_system_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _updater_internal_idl_system_StubVtblList,
    (const PCInterfaceName * ) & _updater_internal_idl_system_InterfaceNamesList,
    0, /* no delegation */
    & _updater_internal_idl_system_IID_Lookup, 
    4,
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

