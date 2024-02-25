

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for gen/chrome/updater/app/server/win/updater_internal_idl_user.idl:
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


#include "updater_internal_idl_user.h"

#define TYPE_FORMAT_STRING_SIZE   39                                
#define PROC_FORMAT_STRING_SIZE   211                               
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   0            

typedef struct _updater_internal_idl_user_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } updater_internal_idl_user_MIDL_TYPE_FORMAT_STRING;

typedef struct _updater_internal_idl_user_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } updater_internal_idl_user_MIDL_PROC_FORMAT_STRING;

typedef struct _updater_internal_idl_user_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } updater_internal_idl_user_MIDL_EXPR_FORMAT_STRING;


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


extern const updater_internal_idl_user_MIDL_TYPE_FORMAT_STRING updater_internal_idl_user__MIDL_TypeFormatString;
extern const updater_internal_idl_user_MIDL_PROC_FORMAT_STRING updater_internal_idl_user__MIDL_ProcFormatString;
extern const updater_internal_idl_user_MIDL_EXPR_FORMAT_STRING updater_internal_idl_user__MIDL_ExprFormatString;

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


extern const MIDL_SERVER_INFO IUpdaterInternalCallbackUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterInternalCallbackUser_ProxyInfo;

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


extern const MIDL_SERVER_INFO IUpdaterInternalUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterInternalUser_ProxyInfo;



#if !defined(__RPC_ARM64__)
#error  Invalid build platform for this stub.
#endif

static const updater_internal_idl_user_MIDL_PROC_FORMAT_STRING updater_internal_idl_user__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure Run */


	/* Procedure Run */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x3 ),	/* 3 */
/*  8 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 10 */	NdrFcShort( 0x8 ),	/* 8 */
/* 12 */	NdrFcShort( 0x8 ),	/* 8 */
/* 14 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 16 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 18 */	NdrFcShort( 0x0 ),	/* 0 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x2 ),	/* 2 */
/* 26 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 28 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter result */


	/* Parameter result */

/* 30 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 32 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 34 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 36 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 38 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 40 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Run */

/* 42 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 44 */	NdrFcLong( 0x0 ),	/* 0 */
/* 48 */	NdrFcShort( 0x3 ),	/* 3 */
/* 50 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 52 */	NdrFcShort( 0x0 ),	/* 0 */
/* 54 */	NdrFcShort( 0x8 ),	/* 8 */
/* 56 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 58 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 60 */	NdrFcShort( 0x0 ),	/* 0 */
/* 62 */	NdrFcShort( 0x0 ),	/* 0 */
/* 64 */	NdrFcShort( 0x0 ),	/* 0 */
/* 66 */	NdrFcShort( 0x2 ),	/* 2 */
/* 68 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 70 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter callback */

/* 72 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 74 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 76 */	NdrFcShort( 0x2 ),	/* Type Offset=2 */

	/* Return value */

/* 78 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 80 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 82 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Hello */

/* 84 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 86 */	NdrFcLong( 0x0 ),	/* 0 */
/* 90 */	NdrFcShort( 0x4 ),	/* 4 */
/* 92 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 94 */	NdrFcShort( 0x0 ),	/* 0 */
/* 96 */	NdrFcShort( 0x8 ),	/* 8 */
/* 98 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 100 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 102 */	NdrFcShort( 0x0 ),	/* 0 */
/* 104 */	NdrFcShort( 0x0 ),	/* 0 */
/* 106 */	NdrFcShort( 0x0 ),	/* 0 */
/* 108 */	NdrFcShort( 0x2 ),	/* 2 */
/* 110 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 112 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter callback */

/* 114 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 116 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 118 */	NdrFcShort( 0x2 ),	/* Type Offset=2 */

	/* Return value */

/* 120 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 122 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 124 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Run */

/* 126 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 128 */	NdrFcLong( 0x0 ),	/* 0 */
/* 132 */	NdrFcShort( 0x3 ),	/* 3 */
/* 134 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 136 */	NdrFcShort( 0x0 ),	/* 0 */
/* 138 */	NdrFcShort( 0x8 ),	/* 8 */
/* 140 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 142 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 144 */	NdrFcShort( 0x0 ),	/* 0 */
/* 146 */	NdrFcShort( 0x0 ),	/* 0 */
/* 148 */	NdrFcShort( 0x0 ),	/* 0 */
/* 150 */	NdrFcShort( 0x2 ),	/* 2 */
/* 152 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 154 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter callback */

/* 156 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 158 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 160 */	NdrFcShort( 0x14 ),	/* Type Offset=20 */

	/* Return value */

/* 162 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 164 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 166 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Hello */

/* 168 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 170 */	NdrFcLong( 0x0 ),	/* 0 */
/* 174 */	NdrFcShort( 0x4 ),	/* 4 */
/* 176 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 178 */	NdrFcShort( 0x0 ),	/* 0 */
/* 180 */	NdrFcShort( 0x8 ),	/* 8 */
/* 182 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 184 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 186 */	NdrFcShort( 0x0 ),	/* 0 */
/* 188 */	NdrFcShort( 0x0 ),	/* 0 */
/* 190 */	NdrFcShort( 0x0 ),	/* 0 */
/* 192 */	NdrFcShort( 0x2 ),	/* 2 */
/* 194 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 196 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter callback */

/* 198 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 200 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 202 */	NdrFcShort( 0x14 ),	/* Type Offset=20 */

	/* Return value */

/* 204 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 206 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 208 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const updater_internal_idl_user_MIDL_TYPE_FORMAT_STRING updater_internal_idl_user__MIDL_TypeFormatString =
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
/* 22 */	NdrFcLong( 0x618d9b82 ),	/* 1636670338 */
/* 26 */	NdrFcShort( 0x9f51 ),	/* -24751 */
/* 28 */	NdrFcShort( 0x4490 ),	/* 17552 */
/* 30 */	0xaf,		/* 175 */
			0x24,		/* 36 */
/* 32 */	0xbb,		/* 187 */
			0x80,		/* 128 */
/* 34 */	0x48,		/* 72 */
			0x9e,		/* 158 */
/* 36 */	0x15,		/* 21 */
			0x37,		/* 55 */

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
    updater_internal_idl_user__MIDL_ProcFormatString.Format,
    &IUpdaterInternalCallback_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterInternalCallback_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_internal_idl_user__MIDL_ProcFormatString.Format,
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


/* Object interface: IUpdaterInternalCallbackUser, ver. 0.0,
   GUID={0x618D9B82,0x9F51,0x4490,{0xAF,0x24,0xBB,0x80,0x48,0x9E,0x15,0x37}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterInternalCallbackUser_FormatStringOffsetTable[] =
    {
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterInternalCallbackUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_internal_idl_user__MIDL_ProcFormatString.Format,
    &IUpdaterInternalCallbackUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterInternalCallbackUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_internal_idl_user__MIDL_ProcFormatString.Format,
    &IUpdaterInternalCallbackUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IUpdaterInternalCallbackUserProxyVtbl = 
{
    &IUpdaterInternalCallbackUser_ProxyInfo,
    &IID_IUpdaterInternalCallbackUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterInternalCallbackUser::Run */
};

const CInterfaceStubVtbl _IUpdaterInternalCallbackUserStubVtbl =
{
    &IID_IUpdaterInternalCallbackUser,
    &IUpdaterInternalCallbackUser_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterInternal, ver. 0.0,
   GUID={0x526DA036,0x9BD3,0x4697,{0x86,0x5A,0xDA,0x12,0xD3,0x7D,0xFF,0xCA}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterInternal_FormatStringOffsetTable[] =
    {
    42,
    84
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterInternal_ProxyInfo =
    {
    &Object_StubDesc,
    updater_internal_idl_user__MIDL_ProcFormatString.Format,
    &IUpdaterInternal_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterInternal_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_internal_idl_user__MIDL_ProcFormatString.Format,
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


/* Object interface: IUpdaterInternalUser, ver. 0.0,
   GUID={0xC82AFDA3,0xCA76,0x46EE,{0x96,0xE9,0x47,0x47,0x17,0xBF,0xA7,0xBA}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterInternalUser_FormatStringOffsetTable[] =
    {
    126,
    168
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterInternalUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_internal_idl_user__MIDL_ProcFormatString.Format,
    &IUpdaterInternalUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterInternalUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_internal_idl_user__MIDL_ProcFormatString.Format,
    &IUpdaterInternalUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(5) _IUpdaterInternalUserProxyVtbl = 
{
    &IUpdaterInternalUser_ProxyInfo,
    &IID_IUpdaterInternalUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterInternalUser::Run */ ,
    (void *) (INT_PTR) -1 /* IUpdaterInternalUser::Hello */
};

const CInterfaceStubVtbl _IUpdaterInternalUserStubVtbl =
{
    &IID_IUpdaterInternalUser,
    &IUpdaterInternalUser_ServerInfo,
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
    updater_internal_idl_user__MIDL_TypeFormatString.Format,
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

const CInterfaceProxyVtbl * const _updater_internal_idl_user_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_IUpdaterInternalProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterInternalCallbackUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterInternalCallbackProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterInternalUserProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _updater_internal_idl_user_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_IUpdaterInternalStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterInternalCallbackUserStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterInternalCallbackStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterInternalUserStubVtbl,
    0
};

PCInterfaceName const _updater_internal_idl_user_InterfaceNamesList[] = 
{
    "IUpdaterInternal",
    "IUpdaterInternalCallbackUser",
    "IUpdaterInternalCallback",
    "IUpdaterInternalUser",
    0
};


#define _updater_internal_idl_user_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _updater_internal_idl_user, pIID, n)

int __stdcall _updater_internal_idl_user_IID_Lookup( const IID * pIID, int * pIndex )
{
    IID_BS_LOOKUP_SETUP

    IID_BS_LOOKUP_INITIAL_TEST( _updater_internal_idl_user, 4, 2 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_internal_idl_user, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _updater_internal_idl_user, 4, *pIndex )
    
}

EXTERN_C const ExtendedProxyFileInfo updater_internal_idl_user_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _updater_internal_idl_user_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _updater_internal_idl_user_StubVtblList,
    (const PCInterfaceName * ) & _updater_internal_idl_user_InterfaceNamesList,
    0, /* no delegation */
    & _updater_internal_idl_user_IID_Lookup, 
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


#endif /* defined(_M_ARM64) */

