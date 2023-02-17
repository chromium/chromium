

/* this ALWAYS GENERATED file contains the proxy stub code */


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


#include "updater_idl.h"

#define TYPE_FORMAT_STRING_SIZE   271                               
#define PROC_FORMAT_STRING_SIZE   2047                              
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   1            

typedef struct _updater_idl_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } updater_idl_MIDL_TYPE_FORMAT_STRING;

typedef struct _updater_idl_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } updater_idl_MIDL_PROC_FORMAT_STRING;

typedef struct _updater_idl_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } updater_idl_MIDL_EXPR_FORMAT_STRING;


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


extern const updater_idl_MIDL_TYPE_FORMAT_STRING updater_idl__MIDL_TypeFormatString;
extern const updater_idl_MIDL_PROC_FORMAT_STRING updater_idl__MIDL_ProcFormatString;
extern const updater_idl_MIDL_EXPR_FORMAT_STRING updater_idl__MIDL_ExprFormatString;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdateState_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdateState_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdateStateUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdateStateUser_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdateStateSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdateStateSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO ICompleteStatus_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ICompleteStatus_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO ICompleteStatusUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ICompleteStatusUser_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO ICompleteStatusSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ICompleteStatusSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterObserver_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterObserver_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterObserverUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterObserverUser_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterObserverSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterObserverSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterCallback_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterCallback_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterCallbackUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterCallbackUser_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterCallbackSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterCallbackSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdater_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdater_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterUser_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterSystem_ProxyInfo;


extern const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ];

#if !defined(__RPC_WIN64__)
#error  Invalid build platform for this stub.
#endif

static const updater_idl_MIDL_PROC_FORMAT_STRING updater_idl__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure get_statusCode */


	/* Procedure get_state */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x3 ),	/* 3 */
/*  8 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 10 */	NdrFcShort( 0x0 ),	/* 0 */
/* 12 */	NdrFcShort( 0x24 ),	/* 36 */
/* 14 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 16 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 18 */	NdrFcShort( 0x0 ),	/* 0 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__ICompleteStatus0000 */


	/* Parameter __MIDL__IUpdateState0000 */

/* 26 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 28 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 30 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 32 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 34 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 36 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_statusMessage */


	/* Procedure get_appId */

/* 38 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 40 */	NdrFcLong( 0x0 ),	/* 0 */
/* 44 */	NdrFcShort( 0x4 ),	/* 4 */
/* 46 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 48 */	NdrFcShort( 0x0 ),	/* 0 */
/* 50 */	NdrFcShort( 0x8 ),	/* 8 */
/* 52 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 54 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 56 */	NdrFcShort( 0x1 ),	/* 1 */
/* 58 */	NdrFcShort( 0x0 ),	/* 0 */
/* 60 */	NdrFcShort( 0x0 ),	/* 0 */
/* 62 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__ICompleteStatus0001 */


	/* Parameter __MIDL__IUpdateState0001 */

/* 64 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 66 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 68 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */

/* 70 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 72 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 74 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nextVersion */

/* 76 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 78 */	NdrFcLong( 0x0 ),	/* 0 */
/* 82 */	NdrFcShort( 0x5 ),	/* 5 */
/* 84 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 86 */	NdrFcShort( 0x0 ),	/* 0 */
/* 88 */	NdrFcShort( 0x8 ),	/* 8 */
/* 90 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 92 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 94 */	NdrFcShort( 0x1 ),	/* 1 */
/* 96 */	NdrFcShort( 0x0 ),	/* 0 */
/* 98 */	NdrFcShort( 0x0 ),	/* 0 */
/* 100 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IUpdateState0002 */

/* 102 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 104 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 106 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */

/* 108 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 110 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 112 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_downloadedBytes */

/* 114 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 116 */	NdrFcLong( 0x0 ),	/* 0 */
/* 120 */	NdrFcShort( 0x6 ),	/* 6 */
/* 122 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 124 */	NdrFcShort( 0x0 ),	/* 0 */
/* 126 */	NdrFcShort( 0x2c ),	/* 44 */
/* 128 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 130 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 132 */	NdrFcShort( 0x0 ),	/* 0 */
/* 134 */	NdrFcShort( 0x0 ),	/* 0 */
/* 136 */	NdrFcShort( 0x0 ),	/* 0 */
/* 138 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IUpdateState0003 */

/* 140 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 142 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 144 */	0xb,		/* FC_HYPER */
			0x0,		/* 0 */

	/* Return value */

/* 146 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 148 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 150 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_totalBytes */

/* 152 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 154 */	NdrFcLong( 0x0 ),	/* 0 */
/* 158 */	NdrFcShort( 0x7 ),	/* 7 */
/* 160 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 162 */	NdrFcShort( 0x0 ),	/* 0 */
/* 164 */	NdrFcShort( 0x2c ),	/* 44 */
/* 166 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 168 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 170 */	NdrFcShort( 0x0 ),	/* 0 */
/* 172 */	NdrFcShort( 0x0 ),	/* 0 */
/* 174 */	NdrFcShort( 0x0 ),	/* 0 */
/* 176 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IUpdateState0004 */

/* 178 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 180 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 182 */	0xb,		/* FC_HYPER */
			0x0,		/* 0 */

	/* Return value */

/* 184 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 186 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 188 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installProgress */

/* 190 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 192 */	NdrFcLong( 0x0 ),	/* 0 */
/* 196 */	NdrFcShort( 0x8 ),	/* 8 */
/* 198 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 200 */	NdrFcShort( 0x0 ),	/* 0 */
/* 202 */	NdrFcShort( 0x24 ),	/* 36 */
/* 204 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 206 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 208 */	NdrFcShort( 0x0 ),	/* 0 */
/* 210 */	NdrFcShort( 0x0 ),	/* 0 */
/* 212 */	NdrFcShort( 0x0 ),	/* 0 */
/* 214 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IUpdateState0005 */

/* 216 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 218 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 220 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 222 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 224 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 226 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_errorCategory */

/* 228 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 230 */	NdrFcLong( 0x0 ),	/* 0 */
/* 234 */	NdrFcShort( 0x9 ),	/* 9 */
/* 236 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 238 */	NdrFcShort( 0x0 ),	/* 0 */
/* 240 */	NdrFcShort( 0x24 ),	/* 36 */
/* 242 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 244 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 246 */	NdrFcShort( 0x0 ),	/* 0 */
/* 248 */	NdrFcShort( 0x0 ),	/* 0 */
/* 250 */	NdrFcShort( 0x0 ),	/* 0 */
/* 252 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IUpdateState0006 */

/* 254 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 256 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 258 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 260 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 262 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 264 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_errorCode */

/* 266 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 268 */	NdrFcLong( 0x0 ),	/* 0 */
/* 272 */	NdrFcShort( 0xa ),	/* 10 */
/* 274 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 276 */	NdrFcShort( 0x0 ),	/* 0 */
/* 278 */	NdrFcShort( 0x24 ),	/* 36 */
/* 280 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 282 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 284 */	NdrFcShort( 0x0 ),	/* 0 */
/* 286 */	NdrFcShort( 0x0 ),	/* 0 */
/* 288 */	NdrFcShort( 0x0 ),	/* 0 */
/* 290 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IUpdateState0007 */

/* 292 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 294 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 296 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 298 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 300 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 302 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_extraCode1 */

/* 304 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 306 */	NdrFcLong( 0x0 ),	/* 0 */
/* 310 */	NdrFcShort( 0xb ),	/* 11 */
/* 312 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 314 */	NdrFcShort( 0x0 ),	/* 0 */
/* 316 */	NdrFcShort( 0x24 ),	/* 36 */
/* 318 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 320 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 322 */	NdrFcShort( 0x0 ),	/* 0 */
/* 324 */	NdrFcShort( 0x0 ),	/* 0 */
/* 326 */	NdrFcShort( 0x0 ),	/* 0 */
/* 328 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IUpdateState0008 */

/* 330 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 332 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 334 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 336 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 338 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 340 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installerText */

/* 342 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 344 */	NdrFcLong( 0x0 ),	/* 0 */
/* 348 */	NdrFcShort( 0xc ),	/* 12 */
/* 350 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 352 */	NdrFcShort( 0x0 ),	/* 0 */
/* 354 */	NdrFcShort( 0x8 ),	/* 8 */
/* 356 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 358 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 360 */	NdrFcShort( 0x1 ),	/* 1 */
/* 362 */	NdrFcShort( 0x0 ),	/* 0 */
/* 364 */	NdrFcShort( 0x0 ),	/* 0 */
/* 366 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IUpdateState0009 */

/* 368 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 370 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 372 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */

/* 374 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 376 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 378 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installerCommandLine */

/* 380 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 382 */	NdrFcLong( 0x0 ),	/* 0 */
/* 386 */	NdrFcShort( 0xd ),	/* 13 */
/* 388 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 390 */	NdrFcShort( 0x0 ),	/* 0 */
/* 392 */	NdrFcShort( 0x8 ),	/* 8 */
/* 394 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 396 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 398 */	NdrFcShort( 0x1 ),	/* 1 */
/* 400 */	NdrFcShort( 0x0 ),	/* 0 */
/* 402 */	NdrFcShort( 0x0 ),	/* 0 */
/* 404 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IUpdateState0010 */

/* 406 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 408 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 410 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */

/* 412 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 414 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 416 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnStateChange */

/* 418 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 420 */	NdrFcLong( 0x0 ),	/* 0 */
/* 424 */	NdrFcShort( 0x3 ),	/* 3 */
/* 426 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 428 */	NdrFcShort( 0x0 ),	/* 0 */
/* 430 */	NdrFcShort( 0x8 ),	/* 8 */
/* 432 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 434 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 436 */	NdrFcShort( 0x0 ),	/* 0 */
/* 438 */	NdrFcShort( 0x0 ),	/* 0 */
/* 440 */	NdrFcShort( 0x0 ),	/* 0 */
/* 442 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter update_state */

/* 444 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 446 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 448 */	NdrFcShort( 0x32 ),	/* Type Offset=50 */

	/* Return value */

/* 450 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 452 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 454 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnComplete */

/* 456 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 458 */	NdrFcLong( 0x0 ),	/* 0 */
/* 462 */	NdrFcShort( 0x4 ),	/* 4 */
/* 464 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 466 */	NdrFcShort( 0x0 ),	/* 0 */
/* 468 */	NdrFcShort( 0x8 ),	/* 8 */
/* 470 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 472 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 474 */	NdrFcShort( 0x0 ),	/* 0 */
/* 476 */	NdrFcShort( 0x0 ),	/* 0 */
/* 478 */	NdrFcShort( 0x0 ),	/* 0 */
/* 480 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter status */

/* 482 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 484 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 486 */	NdrFcShort( 0x44 ),	/* Type Offset=68 */

	/* Return value */

/* 488 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 490 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 492 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnStateChange */

/* 494 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 496 */	NdrFcLong( 0x0 ),	/* 0 */
/* 500 */	NdrFcShort( 0x3 ),	/* 3 */
/* 502 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 504 */	NdrFcShort( 0x0 ),	/* 0 */
/* 506 */	NdrFcShort( 0x8 ),	/* 8 */
/* 508 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 510 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 512 */	NdrFcShort( 0x0 ),	/* 0 */
/* 514 */	NdrFcShort( 0x0 ),	/* 0 */
/* 516 */	NdrFcShort( 0x0 ),	/* 0 */
/* 518 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter update_state */

/* 520 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 522 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 524 */	NdrFcShort( 0x56 ),	/* Type Offset=86 */

	/* Return value */

/* 526 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 528 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 530 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnComplete */

/* 532 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 534 */	NdrFcLong( 0x0 ),	/* 0 */
/* 538 */	NdrFcShort( 0x4 ),	/* 4 */
/* 540 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 542 */	NdrFcShort( 0x0 ),	/* 0 */
/* 544 */	NdrFcShort( 0x8 ),	/* 8 */
/* 546 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 548 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 550 */	NdrFcShort( 0x0 ),	/* 0 */
/* 552 */	NdrFcShort( 0x0 ),	/* 0 */
/* 554 */	NdrFcShort( 0x0 ),	/* 0 */
/* 556 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter status */

/* 558 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 560 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 562 */	NdrFcShort( 0x68 ),	/* Type Offset=104 */

	/* Return value */

/* 564 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 566 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 568 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnStateChange */

/* 570 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 572 */	NdrFcLong( 0x0 ),	/* 0 */
/* 576 */	NdrFcShort( 0x3 ),	/* 3 */
/* 578 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 580 */	NdrFcShort( 0x0 ),	/* 0 */
/* 582 */	NdrFcShort( 0x8 ),	/* 8 */
/* 584 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 586 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 588 */	NdrFcShort( 0x0 ),	/* 0 */
/* 590 */	NdrFcShort( 0x0 ),	/* 0 */
/* 592 */	NdrFcShort( 0x0 ),	/* 0 */
/* 594 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter update_state */

/* 596 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 598 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 600 */	NdrFcShort( 0x7a ),	/* Type Offset=122 */

	/* Return value */

/* 602 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 604 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 606 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnComplete */

/* 608 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 610 */	NdrFcLong( 0x0 ),	/* 0 */
/* 614 */	NdrFcShort( 0x4 ),	/* 4 */
/* 616 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 618 */	NdrFcShort( 0x0 ),	/* 0 */
/* 620 */	NdrFcShort( 0x8 ),	/* 8 */
/* 622 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 624 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 626 */	NdrFcShort( 0x0 ),	/* 0 */
/* 628 */	NdrFcShort( 0x0 ),	/* 0 */
/* 630 */	NdrFcShort( 0x0 ),	/* 0 */
/* 632 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter status */

/* 634 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 636 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 638 */	NdrFcShort( 0x8c ),	/* Type Offset=140 */

	/* Return value */

/* 640 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 642 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 644 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Run */

/* 646 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 648 */	NdrFcLong( 0x0 ),	/* 0 */
/* 652 */	NdrFcShort( 0x3 ),	/* 3 */
/* 654 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 656 */	NdrFcShort( 0x8 ),	/* 8 */
/* 658 */	NdrFcShort( 0x8 ),	/* 8 */
/* 660 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 662 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 664 */	NdrFcShort( 0x0 ),	/* 0 */
/* 666 */	NdrFcShort( 0x0 ),	/* 0 */
/* 668 */	NdrFcShort( 0x0 ),	/* 0 */
/* 670 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter result */

/* 672 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 674 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 676 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 678 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 680 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 682 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure GetVersion */


	/* Procedure GetVersion */


	/* Procedure GetVersion */

/* 684 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 686 */	NdrFcLong( 0x0 ),	/* 0 */
/* 690 */	NdrFcShort( 0x3 ),	/* 3 */
/* 692 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 694 */	NdrFcShort( 0x0 ),	/* 0 */
/* 696 */	NdrFcShort( 0x8 ),	/* 8 */
/* 698 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 700 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 702 */	NdrFcShort( 0x1 ),	/* 1 */
/* 704 */	NdrFcShort( 0x0 ),	/* 0 */
/* 706 */	NdrFcShort( 0x0 ),	/* 0 */
/* 708 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter version */


	/* Parameter version */


	/* Parameter version */

/* 710 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 712 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 714 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 716 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 718 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 720 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure FetchPolicies */

/* 722 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 724 */	NdrFcLong( 0x0 ),	/* 0 */
/* 728 */	NdrFcShort( 0x4 ),	/* 4 */
/* 730 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 732 */	NdrFcShort( 0x0 ),	/* 0 */
/* 734 */	NdrFcShort( 0x8 ),	/* 8 */
/* 736 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 738 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 740 */	NdrFcShort( 0x0 ),	/* 0 */
/* 742 */	NdrFcShort( 0x0 ),	/* 0 */
/* 744 */	NdrFcShort( 0x0 ),	/* 0 */
/* 746 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 748 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 750 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 752 */	NdrFcShort( 0x9e ),	/* Type Offset=158 */

	/* Return value */

/* 754 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 756 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 758 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure CheckForUpdate */


	/* Procedure CheckForUpdate */


	/* Procedure CheckForUpdate */

/* 760 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 762 */	NdrFcLong( 0x0 ),	/* 0 */
/* 766 */	NdrFcShort( 0x5 ),	/* 5 */
/* 768 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 770 */	NdrFcShort( 0x0 ),	/* 0 */
/* 772 */	NdrFcShort( 0x8 ),	/* 8 */
/* 774 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 776 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 778 */	NdrFcShort( 0x0 ),	/* 0 */
/* 780 */	NdrFcShort( 0x0 ),	/* 0 */
/* 782 */	NdrFcShort( 0x0 ),	/* 0 */
/* 784 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */


	/* Parameter app_id */


	/* Parameter app_id */

/* 786 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 788 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 790 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 792 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 794 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 796 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RegisterApp */

/* 798 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 800 */	NdrFcLong( 0x0 ),	/* 0 */
/* 804 */	NdrFcShort( 0x6 ),	/* 6 */
/* 806 */	NdrFcShort( 0x48 ),	/* X64 Stack size/offset = 72 */
/* 808 */	NdrFcShort( 0x0 ),	/* 0 */
/* 810 */	NdrFcShort( 0x8 ),	/* 8 */
/* 812 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x8,		/* 8 */
/* 814 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 816 */	NdrFcShort( 0x0 ),	/* 0 */
/* 818 */	NdrFcShort( 0x0 ),	/* 0 */
/* 820 */	NdrFcShort( 0x0 ),	/* 0 */
/* 822 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 824 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 826 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 828 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_code */

/* 830 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 832 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 834 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_path */

/* 836 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 838 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 840 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter tag */

/* 842 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 844 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 846 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter version */

/* 848 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 850 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 852 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter existence_checker_path */

/* 854 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 856 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 858 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter callback */

/* 860 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 862 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 864 */	NdrFcShort( 0x9e ),	/* Type Offset=158 */

	/* Return value */

/* 866 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 868 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 870 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunPeriodicTasks */

/* 872 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 874 */	NdrFcLong( 0x0 ),	/* 0 */
/* 878 */	NdrFcShort( 0x7 ),	/* 7 */
/* 880 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 882 */	NdrFcShort( 0x0 ),	/* 0 */
/* 884 */	NdrFcShort( 0x8 ),	/* 8 */
/* 886 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 888 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 890 */	NdrFcShort( 0x0 ),	/* 0 */
/* 892 */	NdrFcShort( 0x0 ),	/* 0 */
/* 894 */	NdrFcShort( 0x0 ),	/* 0 */
/* 896 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 898 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 900 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 902 */	NdrFcShort( 0x9e ),	/* Type Offset=158 */

	/* Return value */

/* 904 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 906 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 908 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Update */

/* 910 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 912 */	NdrFcLong( 0x0 ),	/* 0 */
/* 916 */	NdrFcShort( 0x8 ),	/* 8 */
/* 918 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 920 */	NdrFcShort( 0x18 ),	/* 24 */
/* 922 */	NdrFcShort( 0x8 ),	/* 8 */
/* 924 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 926 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 928 */	NdrFcShort( 0x0 ),	/* 0 */
/* 930 */	NdrFcShort( 0x0 ),	/* 0 */
/* 932 */	NdrFcShort( 0x0 ),	/* 0 */
/* 934 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 936 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 938 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 940 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data_index */

/* 942 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 944 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 946 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter priority */

/* 948 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 950 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 952 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 954 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 956 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 958 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter do_update_check_only */

/* 960 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 962 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 964 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 966 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 968 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 970 */	NdrFcShort( 0xb4 ),	/* Type Offset=180 */

	/* Return value */

/* 972 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 974 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 976 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure UpdateAll */

/* 978 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 980 */	NdrFcLong( 0x0 ),	/* 0 */
/* 984 */	NdrFcShort( 0x9 ),	/* 9 */
/* 986 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 988 */	NdrFcShort( 0x0 ),	/* 0 */
/* 990 */	NdrFcShort( 0x8 ),	/* 8 */
/* 992 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 994 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 996 */	NdrFcShort( 0x0 ),	/* 0 */
/* 998 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1000 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1002 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter observer */

/* 1004 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1006 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1008 */	NdrFcShort( 0xb4 ),	/* Type Offset=180 */

	/* Return value */

/* 1010 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1012 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1014 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Install */

/* 1016 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1018 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1022 */	NdrFcShort( 0xa ),	/* 10 */
/* 1024 */	NdrFcShort( 0x60 ),	/* X64 Stack size/offset = 96 */
/* 1026 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1028 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1030 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0xb,		/* 11 */
/* 1032 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1034 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1036 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1038 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1040 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1042 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1044 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1046 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_code */

/* 1048 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1050 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1052 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_path */

/* 1054 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1056 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1058 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter tag */

/* 1060 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1062 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1064 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter version */

/* 1066 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1068 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1070 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter existence_checker_path */

/* 1072 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1074 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1076 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter client_install_data */

/* 1078 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1080 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1082 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data_index */

/* 1084 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1086 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1088 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter priority */

/* 1090 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1092 */	NdrFcShort( 0x48 ),	/* X64 Stack size/offset = 72 */
/* 1094 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1096 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1098 */	NdrFcShort( 0x50 ),	/* X64 Stack size/offset = 80 */
/* 1100 */	NdrFcShort( 0xb4 ),	/* Type Offset=180 */

	/* Return value */

/* 1102 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1104 */	NdrFcShort( 0x58 ),	/* X64 Stack size/offset = 88 */
/* 1106 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure CancelInstalls */


	/* Procedure CancelInstalls */


	/* Procedure CancelInstalls */

/* 1108 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1110 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1114 */	NdrFcShort( 0xb ),	/* 11 */
/* 1116 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1118 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1120 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1122 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1124 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1126 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1128 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1130 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1132 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */


	/* Parameter app_id */


	/* Parameter app_id */

/* 1134 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1136 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1138 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1140 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1142 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1144 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunInstaller */

/* 1146 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1148 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1152 */	NdrFcShort( 0xc ),	/* 12 */
/* 1154 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1156 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1158 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1160 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 1162 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1164 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1166 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1168 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1170 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1172 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1174 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1176 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter installer_path */

/* 1178 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1180 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1182 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_args */

/* 1184 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1186 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1188 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data */

/* 1190 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1192 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1194 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_settings */

/* 1196 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1198 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1200 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter observer */

/* 1202 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1204 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1206 */	NdrFcShort( 0xb4 ),	/* Type Offset=180 */

	/* Return value */

/* 1208 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1210 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1212 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure FetchPolicies */

/* 1214 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1216 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1220 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1222 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1224 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1226 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1228 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1230 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1232 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1234 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1236 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1238 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 1240 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1242 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1244 */	NdrFcShort( 0xc6 ),	/* Type Offset=198 */

	/* Return value */

/* 1246 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1248 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1250 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RegisterApp */

/* 1252 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1254 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1258 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1260 */	NdrFcShort( 0x48 ),	/* X64 Stack size/offset = 72 */
/* 1262 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1264 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1266 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x8,		/* 8 */
/* 1268 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1270 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1272 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1274 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1276 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1278 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1280 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1282 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_code */

/* 1284 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1286 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1288 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_path */

/* 1290 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1292 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1294 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter tag */

/* 1296 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1298 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1300 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter version */

/* 1302 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1304 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1306 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter existence_checker_path */

/* 1308 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1310 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1312 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter callback */

/* 1314 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1316 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1318 */	NdrFcShort( 0xc6 ),	/* Type Offset=198 */

	/* Return value */

/* 1320 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1322 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1324 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunPeriodicTasks */

/* 1326 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1328 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1332 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1334 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1336 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1338 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1340 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1342 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1344 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1346 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1348 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1350 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 1352 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1354 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1356 */	NdrFcShort( 0xc6 ),	/* Type Offset=198 */

	/* Return value */

/* 1358 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1360 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1362 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Update */

/* 1364 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1366 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1370 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1372 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1374 */	NdrFcShort( 0x18 ),	/* 24 */
/* 1376 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1378 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 1380 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1382 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1384 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1386 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1388 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1390 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1392 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1394 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data_index */

/* 1396 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1398 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1400 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter priority */

/* 1402 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1404 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1406 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 1408 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1410 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1412 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter do_update_check_only */

/* 1414 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1416 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1418 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1420 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1422 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1424 */	NdrFcShort( 0xd8 ),	/* Type Offset=216 */

	/* Return value */

/* 1426 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1428 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1430 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure UpdateAll */

/* 1432 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1434 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1438 */	NdrFcShort( 0x9 ),	/* 9 */
/* 1440 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1442 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1444 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1446 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1448 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1450 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1452 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1454 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1456 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter observer */

/* 1458 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1460 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1462 */	NdrFcShort( 0xd8 ),	/* Type Offset=216 */

	/* Return value */

/* 1464 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1466 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1468 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Install */

/* 1470 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1472 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1476 */	NdrFcShort( 0xa ),	/* 10 */
/* 1478 */	NdrFcShort( 0x60 ),	/* X64 Stack size/offset = 96 */
/* 1480 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1482 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1484 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0xb,		/* 11 */
/* 1486 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1488 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1490 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1492 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1494 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1496 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1498 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1500 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_code */

/* 1502 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1504 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1506 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_path */

/* 1508 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1510 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1512 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter tag */

/* 1514 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1516 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1518 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter version */

/* 1520 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1522 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1524 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter existence_checker_path */

/* 1526 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1528 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1530 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter client_install_data */

/* 1532 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1534 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1536 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data_index */

/* 1538 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1540 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1542 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter priority */

/* 1544 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1546 */	NdrFcShort( 0x48 ),	/* X64 Stack size/offset = 72 */
/* 1548 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1550 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1552 */	NdrFcShort( 0x50 ),	/* X64 Stack size/offset = 80 */
/* 1554 */	NdrFcShort( 0xd8 ),	/* Type Offset=216 */

	/* Return value */

/* 1556 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1558 */	NdrFcShort( 0x58 ),	/* X64 Stack size/offset = 88 */
/* 1560 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunInstaller */

/* 1562 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1564 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1568 */	NdrFcShort( 0xc ),	/* 12 */
/* 1570 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1572 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1574 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1576 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 1578 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1580 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1582 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1584 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1586 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1588 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1590 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1592 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter installer_path */

/* 1594 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1596 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1598 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_args */

/* 1600 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1602 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1604 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data */

/* 1606 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1608 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1610 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_settings */

/* 1612 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1614 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1616 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter observer */

/* 1618 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1620 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1622 */	NdrFcShort( 0xd8 ),	/* Type Offset=216 */

	/* Return value */

/* 1624 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1626 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1628 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure FetchPolicies */

/* 1630 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1632 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1636 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1638 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1640 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1642 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1644 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1646 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1648 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1650 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1652 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1654 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 1656 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1658 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1660 */	NdrFcShort( 0xea ),	/* Type Offset=234 */

	/* Return value */

/* 1662 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1664 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1666 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RegisterApp */

/* 1668 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1670 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1674 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1676 */	NdrFcShort( 0x48 ),	/* X64 Stack size/offset = 72 */
/* 1678 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1680 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1682 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x8,		/* 8 */
/* 1684 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1686 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1688 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1690 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1692 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1694 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1696 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1698 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_code */

/* 1700 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1702 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1704 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_path */

/* 1706 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1708 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1710 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter tag */

/* 1712 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1714 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1716 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter version */

/* 1718 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1720 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1722 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter existence_checker_path */

/* 1724 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1726 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1728 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter callback */

/* 1730 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1732 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1734 */	NdrFcShort( 0xea ),	/* Type Offset=234 */

	/* Return value */

/* 1736 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1738 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1740 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunPeriodicTasks */

/* 1742 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1744 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1748 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1750 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1752 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1754 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1756 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1758 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1760 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1762 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1764 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1766 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 1768 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1770 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1772 */	NdrFcShort( 0xea ),	/* Type Offset=234 */

	/* Return value */

/* 1774 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1776 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1778 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Update */

/* 1780 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1782 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1786 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1788 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1790 */	NdrFcShort( 0x18 ),	/* 24 */
/* 1792 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1794 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 1796 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1798 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1800 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1802 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1804 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1806 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1808 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1810 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data_index */

/* 1812 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1814 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1816 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter priority */

/* 1818 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1820 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1822 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 1824 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1826 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1828 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter do_update_check_only */

/* 1830 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1832 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1834 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1836 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1838 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1840 */	NdrFcShort( 0xfc ),	/* Type Offset=252 */

	/* Return value */

/* 1842 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1844 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1846 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure UpdateAll */

/* 1848 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1850 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1854 */	NdrFcShort( 0x9 ),	/* 9 */
/* 1856 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1858 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1860 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1862 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1864 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1866 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1868 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1870 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1872 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter observer */

/* 1874 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1876 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1878 */	NdrFcShort( 0xfc ),	/* Type Offset=252 */

	/* Return value */

/* 1880 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1882 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1884 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Install */

/* 1886 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1888 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1892 */	NdrFcShort( 0xa ),	/* 10 */
/* 1894 */	NdrFcShort( 0x60 ),	/* X64 Stack size/offset = 96 */
/* 1896 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1898 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1900 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0xb,		/* 11 */
/* 1902 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1904 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1906 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1908 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1910 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1912 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1914 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1916 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_code */

/* 1918 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1920 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1922 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_path */

/* 1924 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1926 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1928 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter tag */

/* 1930 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1932 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1934 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter version */

/* 1936 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1938 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1940 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter existence_checker_path */

/* 1942 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1944 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1946 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter client_install_data */

/* 1948 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1950 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1952 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data_index */

/* 1954 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1956 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1958 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter priority */

/* 1960 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1962 */	NdrFcShort( 0x48 ),	/* X64 Stack size/offset = 72 */
/* 1964 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1966 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1968 */	NdrFcShort( 0x50 ),	/* X64 Stack size/offset = 80 */
/* 1970 */	NdrFcShort( 0xfc ),	/* Type Offset=252 */

	/* Return value */

/* 1972 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1974 */	NdrFcShort( 0x58 ),	/* X64 Stack size/offset = 88 */
/* 1976 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunInstaller */

/* 1978 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1980 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1984 */	NdrFcShort( 0xc ),	/* 12 */
/* 1986 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1988 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1990 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1992 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 1994 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1996 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1998 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2000 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2002 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 2004 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2006 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2008 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter installer_path */

/* 2010 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2012 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2014 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_args */

/* 2016 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2018 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2020 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data */

/* 2022 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2024 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2026 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_settings */

/* 2028 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2030 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2032 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter observer */

/* 2034 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2036 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 2038 */	NdrFcShort( 0xfc ),	/* Type Offset=252 */

	/* Return value */

/* 2040 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2042 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 2044 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const updater_idl_MIDL_TYPE_FORMAT_STRING updater_idl__MIDL_TypeFormatString =
    {
        0,
        {
			NdrFcShort( 0x0 ),	/* 0 */
/*  2 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/*  4 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */
/*  6 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/*  8 */	NdrFcShort( 0x1c ),	/* Offset= 28 (36) */
/* 10 */	
			0x13, 0x0,	/* FC_OP */
/* 12 */	NdrFcShort( 0xe ),	/* Offset= 14 (26) */
/* 14 */	
			0x1b,		/* FC_CARRAY */
			0x1,		/* 1 */
/* 16 */	NdrFcShort( 0x2 ),	/* 2 */
/* 18 */	0x9,		/* Corr desc: FC_ULONG */
			0x0,		/*  */
/* 20 */	NdrFcShort( 0xfffc ),	/* -4 */
/* 22 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 24 */	0x6,		/* FC_SHORT */
			0x5b,		/* FC_END */
/* 26 */	
			0x17,		/* FC_CSTRUCT */
			0x3,		/* 3 */
/* 28 */	NdrFcShort( 0x8 ),	/* 8 */
/* 30 */	NdrFcShort( 0xfff0 ),	/* Offset= -16 (14) */
/* 32 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 34 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 36 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 38 */	NdrFcShort( 0x0 ),	/* 0 */
/* 40 */	NdrFcShort( 0x8 ),	/* 8 */
/* 42 */	NdrFcShort( 0x0 ),	/* 0 */
/* 44 */	NdrFcShort( 0xffde ),	/* Offset= -34 (10) */
/* 46 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 48 */	0xb,		/* FC_HYPER */
			0x5c,		/* FC_PAD */
/* 50 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 52 */	NdrFcLong( 0x46acf70b ),	/* 1185740555 */
/* 56 */	NdrFcShort( 0xac13 ),	/* -21485 */
/* 58 */	NdrFcShort( 0x406d ),	/* 16493 */
/* 60 */	0xb5,		/* 181 */
			0x3b,		/* 59 */
/* 62 */	0xb2,		/* 178 */
			0xc4,		/* 196 */
/* 64 */	0xbf,		/* 191 */
			0x9,		/* 9 */
/* 66 */	0x1f,		/* 31 */
			0xf6,		/* 246 */
/* 68 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 70 */	NdrFcLong( 0x2fcd14af ),	/* 801969327 */
/* 74 */	NdrFcShort( 0xb645 ),	/* -18875 */
/* 76 */	NdrFcShort( 0x4351 ),	/* 17233 */
/* 78 */	0x83,		/* 131 */
			0x59,		/* 89 */
/* 80 */	0xe8,		/* 232 */
			0xa,		/* 10 */
/* 82 */	0xe,		/* 14 */
			0x20,		/* 32 */
/* 84 */	0x2a,		/* 42 */
			0xb,		/* 11 */
/* 86 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 88 */	NdrFcLong( 0xc3485d9f ),	/* -1018667617 */
/* 92 */	NdrFcShort( 0xc684 ),	/* -14716 */
/* 94 */	NdrFcShort( 0x4c43 ),	/* 19523 */
/* 96 */	0xb8,		/* 184 */
			0x5b,		/* 91 */
/* 98 */	0xe3,		/* 227 */
			0x39,		/* 57 */
/* 100 */	0xea,		/* 234 */
			0x39,		/* 57 */
/* 102 */	0x5c,		/* 92 */
			0x29,		/* 41 */
/* 104 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 106 */	NdrFcLong( 0x9ad1a645 ),	/* -1697536443 */
/* 110 */	NdrFcShort( 0x5a4b ),	/* 23115 */
/* 112 */	NdrFcShort( 0x4d36 ),	/* 19766 */
/* 114 */	0xbc,		/* 188 */
			0x21,		/* 33 */
/* 116 */	0xf0,		/* 240 */
			0x5,		/* 5 */
/* 118 */	0x94,		/* 148 */
			0x82,		/* 130 */
/* 120 */	0xe6,		/* 230 */
			0xea,		/* 234 */
/* 122 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 124 */	NdrFcLong( 0xea6fdc05 ),	/* -361767931 */
/* 128 */	NdrFcShort( 0xcdc5 ),	/* -12859 */
/* 130 */	NdrFcShort( 0x4ea4 ),	/* 20132 */
/* 132 */	0xab,		/* 171 */
			0x41,		/* 65 */
/* 134 */	0xcc,		/* 204 */
			0xbd,		/* 189 */
/* 136 */	0x10,		/* 16 */
			0x40,		/* 64 */
/* 138 */	0xa2,		/* 162 */
			0xb5,		/* 181 */
/* 140 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 142 */	NdrFcLong( 0xe2bd9a6b ),	/* -490890645 */
/* 146 */	NdrFcShort( 0xa19 ),	/* 2585 */
/* 148 */	NdrFcShort( 0x4c89 ),	/* 19593 */
/* 150 */	0xae,		/* 174 */
			0x8b,		/* 139 */
/* 152 */	0xb7,		/* 183 */
			0xe9,		/* 233 */
/* 154 */	0xe5,		/* 229 */
			0x1d,		/* 29 */
/* 156 */	0x9a,		/* 154 */
			0x7,		/* 7 */
/* 158 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 160 */	NdrFcLong( 0x8bab6f84 ),	/* -1951699068 */
/* 164 */	NdrFcShort( 0xad67 ),	/* -21145 */
/* 166 */	NdrFcShort( 0x4819 ),	/* 18457 */
/* 168 */	0xb8,		/* 184 */
			0x46,		/* 70 */
/* 170 */	0xcc,		/* 204 */
			0x89,		/* 137 */
/* 172 */	0x8,		/* 8 */
			0x80,		/* 128 */
/* 174 */	0xfd,		/* 253 */
			0x3b,		/* 59 */
/* 176 */	
			0x11, 0x8,	/* FC_RP [simple_pointer] */
/* 178 */	
			0x25,		/* FC_C_WSTRING */
			0x5c,		/* FC_PAD */
/* 180 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 182 */	NdrFcLong( 0x7b416cfd ),	/* 2067885309 */
/* 186 */	NdrFcShort( 0x4216 ),	/* 16918 */
/* 188 */	NdrFcShort( 0x4fd6 ),	/* 20438 */
/* 190 */	0xbd,		/* 189 */
			0x83,		/* 131 */
/* 192 */	0x7c,		/* 124 */
			0x58,		/* 88 */
/* 194 */	0x60,		/* 96 */
			0x54,		/* 84 */
/* 196 */	0x67,		/* 103 */
			0x6e,		/* 110 */
/* 198 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 200 */	NdrFcLong( 0x34adc89d ),	/* 883804317 */
/* 204 */	NdrFcShort( 0x552b ),	/* 21803 */
/* 206 */	NdrFcShort( 0x4102 ),	/* 16642 */
/* 208 */	0x8a,		/* 138 */
			0xe5,		/* 229 */
/* 210 */	0xd6,		/* 214 */
			0x13,		/* 19 */
/* 212 */	0xa6,		/* 166 */
			0x91,		/* 145 */
/* 214 */	0x33,		/* 51 */
			0x5b,		/* 91 */
/* 216 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 218 */	NdrFcLong( 0xb54493a0 ),	/* -1253796960 */
/* 222 */	NdrFcShort( 0x65b7 ),	/* 26039 */
/* 224 */	NdrFcShort( 0x408c ),	/* 16524 */
/* 226 */	0xb6,		/* 182 */
			0x50,		/* 80 */
/* 228 */	0x6,		/* 6 */
			0x26,		/* 38 */
/* 230 */	0x5d,		/* 93 */
			0x21,		/* 33 */
/* 232 */	0x82,		/* 130 */
			0xac,		/* 172 */
/* 234 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 236 */	NdrFcLong( 0xf0d6763a ),	/* -254380486 */
/* 240 */	NdrFcShort( 0x182 ),	/* 386 */
/* 242 */	NdrFcShort( 0x4136 ),	/* 16694 */
/* 244 */	0xb1,		/* 177 */
			0xfa,		/* 250 */
/* 246 */	0x50,		/* 80 */
			0x8e,		/* 142 */
/* 248 */	0x33,		/* 51 */
			0x4c,		/* 76 */
/* 250 */	0xff,		/* 255 */
			0xc1,		/* 193 */
/* 252 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 254 */	NdrFcLong( 0x57b500a ),	/* 91967498 */
/* 258 */	NdrFcShort( 0x4ba2 ),	/* 19362 */
/* 260 */	NdrFcShort( 0x496a ),	/* 18794 */
/* 262 */	0xb1,		/* 177 */
			0xcd,		/* 205 */
/* 264 */	0xc5,		/* 197 */
			0xde,		/* 222 */
/* 266 */	0xd3,		/* 211 */
			0xcc,		/* 204 */
/* 268 */	0xc6,		/* 198 */
			0x1b,		/* 27 */

			0x0
        }
    };

XFG_TRAMPOLINES(BSTR)

static const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ] = 
        {
            
            {
            (USER_MARSHAL_SIZING_ROUTINE)XFG_TRAMPOLINE_FPTR(BSTR_UserSize)
            ,(USER_MARSHAL_MARSHALLING_ROUTINE)XFG_TRAMPOLINE_FPTR(BSTR_UserMarshal)
            ,(USER_MARSHAL_UNMARSHALLING_ROUTINE)XFG_TRAMPOLINE_FPTR(BSTR_UserUnmarshal)
            ,(USER_MARSHAL_FREEING_ROUTINE)XFG_TRAMPOLINE_FPTR(BSTR_UserFree)
            
            }
            

        };



/* Object interface: IUnknown, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IUpdateState, ver. 0.0,
   GUID={0x46ACF70B,0xAC13,0x406D,{0xB5,0x3B,0xB2,0xC4,0xBF,0x09,0x1F,0xF6}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdateState_FormatStringOffsetTable[] =
    {
    0,
    38,
    76,
    114,
    152,
    190,
    228,
    266,
    304,
    342,
    380
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdateState_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdateState_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdateState_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdateState_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(14) _IUpdateStateProxyVtbl = 
{
    &IUpdateState_ProxyInfo,
    &IID_IUpdateState,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdateState::get_state */ ,
    (void *) (INT_PTR) -1 /* IUpdateState::get_appId */ ,
    (void *) (INT_PTR) -1 /* IUpdateState::get_nextVersion */ ,
    (void *) (INT_PTR) -1 /* IUpdateState::get_downloadedBytes */ ,
    (void *) (INT_PTR) -1 /* IUpdateState::get_totalBytes */ ,
    (void *) (INT_PTR) -1 /* IUpdateState::get_installProgress */ ,
    (void *) (INT_PTR) -1 /* IUpdateState::get_errorCategory */ ,
    (void *) (INT_PTR) -1 /* IUpdateState::get_errorCode */ ,
    (void *) (INT_PTR) -1 /* IUpdateState::get_extraCode1 */ ,
    (void *) (INT_PTR) -1 /* IUpdateState::get_installerText */ ,
    (void *) (INT_PTR) -1 /* IUpdateState::get_installerCommandLine */
};

const CInterfaceStubVtbl _IUpdateStateStubVtbl =
{
    &IID_IUpdateState,
    &IUpdateState_ServerInfo,
    14,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdateStateUser, ver. 0.0,
   GUID={0xC3485D9F,0xC684,0x4C43,{0xB8,0x5B,0xE3,0x39,0xEA,0x39,0x5C,0x29}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdateStateUser_FormatStringOffsetTable[] =
    {
    0,
    38,
    76,
    114,
    152,
    190,
    228,
    266,
    304,
    342,
    380,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdateStateUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdateStateUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdateStateUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdateStateUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(14) _IUpdateStateUserProxyVtbl = 
{
    0,
    &IID_IUpdateStateUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IUpdateState::get_state */ ,
    0 /* forced delegation IUpdateState::get_appId */ ,
    0 /* forced delegation IUpdateState::get_nextVersion */ ,
    0 /* forced delegation IUpdateState::get_downloadedBytes */ ,
    0 /* forced delegation IUpdateState::get_totalBytes */ ,
    0 /* forced delegation IUpdateState::get_installProgress */ ,
    0 /* forced delegation IUpdateState::get_errorCategory */ ,
    0 /* forced delegation IUpdateState::get_errorCode */ ,
    0 /* forced delegation IUpdateState::get_extraCode1 */ ,
    0 /* forced delegation IUpdateState::get_installerText */ ,
    0 /* forced delegation IUpdateState::get_installerCommandLine */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IUpdateStateUser_table[] =
{
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IUpdateStateUserStubVtbl =
{
    &IID_IUpdateStateUser,
    &IUpdateStateUser_ServerInfo,
    14,
    &IUpdateStateUser_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IUpdateStateSystem, ver. 0.0,
   GUID={0xEA6FDC05,0xCDC5,0x4EA4,{0xAB,0x41,0xCC,0xBD,0x10,0x40,0xA2,0xB5}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdateStateSystem_FormatStringOffsetTable[] =
    {
    0,
    38,
    76,
    114,
    152,
    190,
    228,
    266,
    304,
    342,
    380,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdateStateSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdateStateSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdateStateSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdateStateSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(14) _IUpdateStateSystemProxyVtbl = 
{
    0,
    &IID_IUpdateStateSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IUpdateState::get_state */ ,
    0 /* forced delegation IUpdateState::get_appId */ ,
    0 /* forced delegation IUpdateState::get_nextVersion */ ,
    0 /* forced delegation IUpdateState::get_downloadedBytes */ ,
    0 /* forced delegation IUpdateState::get_totalBytes */ ,
    0 /* forced delegation IUpdateState::get_installProgress */ ,
    0 /* forced delegation IUpdateState::get_errorCategory */ ,
    0 /* forced delegation IUpdateState::get_errorCode */ ,
    0 /* forced delegation IUpdateState::get_extraCode1 */ ,
    0 /* forced delegation IUpdateState::get_installerText */ ,
    0 /* forced delegation IUpdateState::get_installerCommandLine */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IUpdateStateSystem_table[] =
{
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IUpdateStateSystemStubVtbl =
{
    &IID_IUpdateStateSystem,
    &IUpdateStateSystem_ServerInfo,
    14,
    &IUpdateStateSystem_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: ICompleteStatus, ver. 0.0,
   GUID={0x2FCD14AF,0xB645,0x4351,{0x83,0x59,0xE8,0x0A,0x0E,0x20,0x2A,0x0B}} */

#pragma code_seg(".orpc")
static const unsigned short ICompleteStatus_FormatStringOffsetTable[] =
    {
    0,
    38
    };

static const MIDL_STUBLESS_PROXY_INFO ICompleteStatus_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &ICompleteStatus_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ICompleteStatus_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &ICompleteStatus_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(5) _ICompleteStatusProxyVtbl = 
{
    &ICompleteStatus_ProxyInfo,
    &IID_ICompleteStatus,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* ICompleteStatus::get_statusCode */ ,
    (void *) (INT_PTR) -1 /* ICompleteStatus::get_statusMessage */
};

const CInterfaceStubVtbl _ICompleteStatusStubVtbl =
{
    &IID_ICompleteStatus,
    &ICompleteStatus_ServerInfo,
    5,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: ICompleteStatusUser, ver. 0.0,
   GUID={0x9AD1A645,0x5A4B,0x4D36,{0xBC,0x21,0xF0,0x05,0x94,0x82,0xE6,0xEA}} */

#pragma code_seg(".orpc")
static const unsigned short ICompleteStatusUser_FormatStringOffsetTable[] =
    {
    0,
    38,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO ICompleteStatusUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &ICompleteStatusUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ICompleteStatusUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &ICompleteStatusUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(5) _ICompleteStatusUserProxyVtbl = 
{
    0,
    &IID_ICompleteStatusUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation ICompleteStatus::get_statusCode */ ,
    0 /* forced delegation ICompleteStatus::get_statusMessage */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION ICompleteStatusUser_table[] =
{
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _ICompleteStatusUserStubVtbl =
{
    &IID_ICompleteStatusUser,
    &ICompleteStatusUser_ServerInfo,
    5,
    &ICompleteStatusUser_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: ICompleteStatusSystem, ver. 0.0,
   GUID={0xE2BD9A6B,0x0A19,0x4C89,{0xAE,0x8B,0xB7,0xE9,0xE5,0x1D,0x9A,0x07}} */

#pragma code_seg(".orpc")
static const unsigned short ICompleteStatusSystem_FormatStringOffsetTable[] =
    {
    0,
    38,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO ICompleteStatusSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &ICompleteStatusSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ICompleteStatusSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &ICompleteStatusSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(5) _ICompleteStatusSystemProxyVtbl = 
{
    0,
    &IID_ICompleteStatusSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation ICompleteStatus::get_statusCode */ ,
    0 /* forced delegation ICompleteStatus::get_statusMessage */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION ICompleteStatusSystem_table[] =
{
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _ICompleteStatusSystemStubVtbl =
{
    &IID_ICompleteStatusSystem,
    &ICompleteStatusSystem_ServerInfo,
    5,
    &ICompleteStatusSystem_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IUpdaterObserver, ver. 0.0,
   GUID={0x7B416CFD,0x4216,0x4FD6,{0xBD,0x83,0x7C,0x58,0x60,0x54,0x67,0x6E}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterObserver_FormatStringOffsetTable[] =
    {
    418,
    456
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterObserver_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterObserver_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterObserver_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterObserver_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(5) _IUpdaterObserverProxyVtbl = 
{
    &IUpdaterObserver_ProxyInfo,
    &IID_IUpdaterObserver,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterObserver::OnStateChange */ ,
    (void *) (INT_PTR) -1 /* IUpdaterObserver::OnComplete */
};

const CInterfaceStubVtbl _IUpdaterObserverStubVtbl =
{
    &IID_IUpdaterObserver,
    &IUpdaterObserver_ServerInfo,
    5,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterObserverUser, ver. 0.0,
   GUID={0xB54493A0,0x65B7,0x408C,{0xB6,0x50,0x06,0x26,0x5D,0x21,0x82,0xAC}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterObserverUser_FormatStringOffsetTable[] =
    {
    494,
    532
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterObserverUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterObserverUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterObserverUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterObserverUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(5) _IUpdaterObserverUserProxyVtbl = 
{
    &IUpdaterObserverUser_ProxyInfo,
    &IID_IUpdaterObserverUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterObserverUser::OnStateChange */ ,
    (void *) (INT_PTR) -1 /* IUpdaterObserverUser::OnComplete */
};

const CInterfaceStubVtbl _IUpdaterObserverUserStubVtbl =
{
    &IID_IUpdaterObserverUser,
    &IUpdaterObserverUser_ServerInfo,
    5,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterObserverSystem, ver. 0.0,
   GUID={0x057B500A,0x4BA2,0x496A,{0xB1,0xCD,0xC5,0xDE,0xD3,0xCC,0xC6,0x1B}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterObserverSystem_FormatStringOffsetTable[] =
    {
    570,
    608
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterObserverSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterObserverSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterObserverSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterObserverSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(5) _IUpdaterObserverSystemProxyVtbl = 
{
    &IUpdaterObserverSystem_ProxyInfo,
    &IID_IUpdaterObserverSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterObserverSystem::OnStateChange */ ,
    (void *) (INT_PTR) -1 /* IUpdaterObserverSystem::OnComplete */
};

const CInterfaceStubVtbl _IUpdaterObserverSystemStubVtbl =
{
    &IID_IUpdaterObserverSystem,
    &IUpdaterObserverSystem_ServerInfo,
    5,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterCallback, ver. 0.0,
   GUID={0x8BAB6F84,0xAD67,0x4819,{0xB8,0x46,0xCC,0x89,0x08,0x80,0xFD,0x3B}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterCallback_FormatStringOffsetTable[] =
    {
    646
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterCallback_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterCallback_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterCallback_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterCallback_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IUpdaterCallbackProxyVtbl = 
{
    &IUpdaterCallback_ProxyInfo,
    &IID_IUpdaterCallback,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterCallback::Run */
};

const CInterfaceStubVtbl _IUpdaterCallbackStubVtbl =
{
    &IID_IUpdaterCallback,
    &IUpdaterCallback_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterCallbackUser, ver. 0.0,
   GUID={0x34ADC89D,0x552B,0x4102,{0x8A,0xE5,0xD6,0x13,0xA6,0x91,0x33,0x5B}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterCallbackUser_FormatStringOffsetTable[] =
    {
    646,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterCallbackUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterCallbackUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterCallbackUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterCallbackUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IUpdaterCallbackUserProxyVtbl = 
{
    0,
    &IID_IUpdaterCallbackUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IUpdaterCallback::Run */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IUpdaterCallbackUser_table[] =
{
    NdrStubCall2
};

CInterfaceStubVtbl _IUpdaterCallbackUserStubVtbl =
{
    &IID_IUpdaterCallbackUser,
    &IUpdaterCallbackUser_ServerInfo,
    4,
    &IUpdaterCallbackUser_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IUpdaterCallbackSystem, ver. 0.0,
   GUID={0xF0D6763A,0x0182,0x4136,{0xB1,0xFA,0x50,0x8E,0x33,0x4C,0xFF,0xC1}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterCallbackSystem_FormatStringOffsetTable[] =
    {
    646,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterCallbackSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterCallbackSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterCallbackSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterCallbackSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IUpdaterCallbackSystemProxyVtbl = 
{
    0,
    &IID_IUpdaterCallbackSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IUpdaterCallback::Run */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IUpdaterCallbackSystem_table[] =
{
    NdrStubCall2
};

CInterfaceStubVtbl _IUpdaterCallbackSystemStubVtbl =
{
    &IID_IUpdaterCallbackSystem,
    &IUpdaterCallbackSystem_ServerInfo,
    4,
    &IUpdaterCallbackSystem_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IUpdater, ver. 0.0,
   GUID={0x63B8FFB1,0x5314,0x48C9,{0x9C,0x57,0x93,0xEC,0x8B,0xC6,0x18,0x4B}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdater_FormatStringOffsetTable[] =
    {
    684,
    722,
    760,
    798,
    872,
    910,
    978,
    1016,
    1108,
    1146
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdater_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdater_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdater_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdater_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(13) _IUpdaterProxyVtbl = 
{
    &IUpdater_ProxyInfo,
    &IID_IUpdater,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdater::GetVersion */ ,
    (void *) (INT_PTR) -1 /* IUpdater::FetchPolicies */ ,
    (void *) (INT_PTR) -1 /* IUpdater::CheckForUpdate */ ,
    (void *) (INT_PTR) -1 /* IUpdater::RegisterApp */ ,
    (void *) (INT_PTR) -1 /* IUpdater::RunPeriodicTasks */ ,
    (void *) (INT_PTR) -1 /* IUpdater::Update */ ,
    (void *) (INT_PTR) -1 /* IUpdater::UpdateAll */ ,
    (void *) (INT_PTR) -1 /* IUpdater::Install */ ,
    (void *) (INT_PTR) -1 /* IUpdater::CancelInstalls */ ,
    (void *) (INT_PTR) -1 /* IUpdater::RunInstaller */
};

const CInterfaceStubVtbl _IUpdaterStubVtbl =
{
    &IID_IUpdater,
    &IUpdater_ServerInfo,
    13,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterUser, ver. 0.0,
   GUID={0x02AFCB67,0x0899,0x4676,{0x91,0xA9,0x67,0xD9,0x2B,0x3B,0x79,0x18}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterUser_FormatStringOffsetTable[] =
    {
    684,
    1214,
    760,
    1252,
    1326,
    1364,
    1432,
    1470,
    1108,
    1562
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(13) _IUpdaterUserProxyVtbl = 
{
    &IUpdaterUser_ProxyInfo,
    &IID_IUpdaterUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::GetVersion */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::FetchPolicies */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::CheckForUpdate */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::RegisterApp */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::RunPeriodicTasks */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::Update */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::UpdateAll */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::Install */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::CancelInstalls */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::RunInstaller */
};

const CInterfaceStubVtbl _IUpdaterUserStubVtbl =
{
    &IID_IUpdaterUser,
    &IUpdaterUser_ServerInfo,
    13,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterSystem, ver. 0.0,
   GUID={0xFCE335F3,0xA55C,0x496E,{0x81,0x4F,0x85,0x97,0x1C,0x9F,0xA6,0xF1}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterSystem_FormatStringOffsetTable[] =
    {
    684,
    1630,
    760,
    1668,
    1742,
    1780,
    1848,
    1886,
    1108,
    1978
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(13) _IUpdaterSystemProxyVtbl = 
{
    &IUpdaterSystem_ProxyInfo,
    &IID_IUpdaterSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::GetVersion */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::FetchPolicies */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::CheckForUpdate */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::RegisterApp */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::RunPeriodicTasks */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::Update */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::UpdateAll */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::Install */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::CancelInstalls */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::RunInstaller */
};

const CInterfaceStubVtbl _IUpdaterSystemStubVtbl =
{
    &IID_IUpdaterSystem,
    &IUpdaterSystem_ServerInfo,
    13,
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
    updater_idl__MIDL_TypeFormatString.Format,
    1, /* -error bounds_check flag */
    0x50002, /* Ndr library version */
    0,
    0x8010274, /* MIDL Version 8.1.628 */
    0,
    UserMarshalRoutines,
    0,  /* notify & notify_flag routine table */
    0x1, /* MIDL flag */
    0, /* cs routines */
    0,   /* proxy/server info */
    0
    };
#ifdef __cplusplus
}
#endif

const CInterfaceProxyVtbl * const _updater_idl_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_IUpdateStateSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterObserverSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdateStateProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterCallbackSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ICompleteStatusUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ICompleteStatusSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterCallbackProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterCallbackUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdateStateUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterObserverUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ICompleteStatusProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterObserverProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _updater_idl_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_IUpdateStateSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterObserverSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdateStateStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterCallbackSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_ICompleteStatusUserStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterUserStubVtbl,
    ( CInterfaceStubVtbl *) &_ICompleteStatusSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterCallbackStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterCallbackUserStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdateStateUserStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterObserverUserStubVtbl,
    ( CInterfaceStubVtbl *) &_ICompleteStatusStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterObserverStubVtbl,
    0
};

PCInterfaceName const _updater_idl_InterfaceNamesList[] = 
{
    "IUpdateStateSystem",
    "IUpdaterObserverSystem",
    "IUpdateState",
    "IUpdaterCallbackSystem",
    "ICompleteStatusUser",
    "IUpdaterUser",
    "ICompleteStatusSystem",
    "IUpdaterCallback",
    "IUpdaterCallbackUser",
    "IUpdateStateUser",
    "IUpdaterObserverUser",
    "ICompleteStatus",
    "IUpdater",
    "IUpdaterSystem",
    "IUpdaterObserver",
    0
};

const IID *  const _updater_idl_BaseIIDList[] = 
{
    &IID_IUpdateState,   /* forced */
    0,
    0,
    &IID_IUpdaterCallback,   /* forced */
    &IID_ICompleteStatus,   /* forced */
    0,
    &IID_ICompleteStatus,   /* forced */
    0,
    &IID_IUpdaterCallback,   /* forced */
    &IID_IUpdateState,   /* forced */
    0,
    0,
    0,
    0,
    0,
    0
};


#define _updater_idl_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _updater_idl, pIID, n)

int __stdcall _updater_idl_IID_Lookup( const IID * pIID, int * pIndex )
{
    IID_BS_LOOKUP_SETUP

    IID_BS_LOOKUP_INITIAL_TEST( _updater_idl, 15, 8 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_idl, 4 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_idl, 2 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_idl, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _updater_idl, 15, *pIndex )
    
}

EXTERN_C const ExtendedProxyFileInfo updater_idl_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _updater_idl_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _updater_idl_StubVtblList,
    (const PCInterfaceName * ) & _updater_idl_InterfaceNamesList,
    (const IID ** ) & _updater_idl_BaseIIDList,
    & _updater_idl_IID_Lookup, 
    15,
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

