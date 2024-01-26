

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for gen/chrome/updater/app/server/win/updater_idl_system.idl:
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


#include "updater_idl_system.h"

#define TYPE_FORMAT_STRING_SIZE   1199                              
#define PROC_FORMAT_STRING_SIZE   2137                              
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   2            

typedef struct _updater_idl_system_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } updater_idl_system_MIDL_TYPE_FORMAT_STRING;

typedef struct _updater_idl_system_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } updater_idl_system_MIDL_PROC_FORMAT_STRING;

typedef struct _updater_idl_system_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } updater_idl_system_MIDL_EXPR_FORMAT_STRING;


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


extern const updater_idl_system_MIDL_TYPE_FORMAT_STRING updater_idl_system__MIDL_TypeFormatString;
extern const updater_idl_system_MIDL_PROC_FORMAT_STRING updater_idl_system__MIDL_ProcFormatString;
extern const updater_idl_system_MIDL_EXPR_FORMAT_STRING updater_idl_system__MIDL_ExprFormatString;

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


extern const MIDL_SERVER_INFO IUpdaterCallbackSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterCallbackSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterAppState_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterAppState_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterAppStateSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterAppStateSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterAppStatesCallback_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterAppStatesCallback_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterAppStatesCallbackSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterAppStatesCallbackSystem_ProxyInfo;

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


extern const MIDL_SERVER_INFO IUpdaterSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterSystem_ProxyInfo;


extern const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ];

#if !defined(__RPC_ARM64__)
#error  Invalid build platform for this stub.
#endif

static const updater_idl_system_MIDL_PROC_FORMAT_STRING updater_idl_system__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure get_statusCode */


	/* Procedure get_statusCode */


	/* Procedure get_state */


	/* Procedure get_state */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x3 ),	/* 3 */
/*  8 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 10 */	NdrFcShort( 0x0 ),	/* 0 */
/* 12 */	NdrFcShort( 0x24 ),	/* 36 */
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

	/* Parameter __MIDL__ICompleteStatusSystem0000 */


	/* Parameter __MIDL__ICompleteStatus0000 */


	/* Parameter __MIDL__IUpdateStateSystem0000 */


	/* Parameter __MIDL__IUpdateState0000 */

/* 30 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 32 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 34 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 36 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 38 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 40 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_statusMessage */


	/* Procedure get_statusMessage */


	/* Procedure get_appId */


	/* Procedure get_appId */

/* 42 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 44 */	NdrFcLong( 0x0 ),	/* 0 */
/* 48 */	NdrFcShort( 0x4 ),	/* 4 */
/* 50 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 52 */	NdrFcShort( 0x0 ),	/* 0 */
/* 54 */	NdrFcShort( 0x8 ),	/* 8 */
/* 56 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 58 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 60 */	NdrFcShort( 0x1 ),	/* 1 */
/* 62 */	NdrFcShort( 0x0 ),	/* 0 */
/* 64 */	NdrFcShort( 0x0 ),	/* 0 */
/* 66 */	NdrFcShort( 0x2 ),	/* 2 */
/* 68 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 70 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICompleteStatusSystem0001 */


	/* Parameter __MIDL__ICompleteStatus0001 */


	/* Parameter __MIDL__IUpdateStateSystem0001 */


	/* Parameter __MIDL__IUpdateState0001 */

/* 72 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 74 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 76 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 78 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 80 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 82 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nextVersion */


	/* Procedure get_nextVersion */

/* 84 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 86 */	NdrFcLong( 0x0 ),	/* 0 */
/* 90 */	NdrFcShort( 0x5 ),	/* 5 */
/* 92 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 94 */	NdrFcShort( 0x0 ),	/* 0 */
/* 96 */	NdrFcShort( 0x8 ),	/* 8 */
/* 98 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 100 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 102 */	NdrFcShort( 0x1 ),	/* 1 */
/* 104 */	NdrFcShort( 0x0 ),	/* 0 */
/* 106 */	NdrFcShort( 0x0 ),	/* 0 */
/* 108 */	NdrFcShort( 0x2 ),	/* 2 */
/* 110 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 112 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IUpdateStateSystem0002 */


	/* Parameter __MIDL__IUpdateState0002 */

/* 114 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 116 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 118 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */

/* 120 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 122 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 124 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_downloadedBytes */


	/* Procedure get_downloadedBytes */

/* 126 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 128 */	NdrFcLong( 0x0 ),	/* 0 */
/* 132 */	NdrFcShort( 0x6 ),	/* 6 */
/* 134 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 136 */	NdrFcShort( 0x0 ),	/* 0 */
/* 138 */	NdrFcShort( 0x2c ),	/* 44 */
/* 140 */	0x44,		/* Oi2 Flags:  has return, has ext, */
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

	/* Parameter __MIDL__IUpdateStateSystem0003 */


	/* Parameter __MIDL__IUpdateState0003 */

/* 156 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 158 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 160 */	0xb,		/* FC_HYPER */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 162 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 164 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 166 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_totalBytes */


	/* Procedure get_totalBytes */

/* 168 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 170 */	NdrFcLong( 0x0 ),	/* 0 */
/* 174 */	NdrFcShort( 0x7 ),	/* 7 */
/* 176 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 178 */	NdrFcShort( 0x0 ),	/* 0 */
/* 180 */	NdrFcShort( 0x2c ),	/* 44 */
/* 182 */	0x44,		/* Oi2 Flags:  has return, has ext, */
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

	/* Parameter __MIDL__IUpdateStateSystem0004 */


	/* Parameter __MIDL__IUpdateState0004 */

/* 198 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 200 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 202 */	0xb,		/* FC_HYPER */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 204 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 206 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 208 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installProgress */


	/* Procedure get_installProgress */

/* 210 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 212 */	NdrFcLong( 0x0 ),	/* 0 */
/* 216 */	NdrFcShort( 0x8 ),	/* 8 */
/* 218 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 220 */	NdrFcShort( 0x0 ),	/* 0 */
/* 222 */	NdrFcShort( 0x24 ),	/* 36 */
/* 224 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 226 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 228 */	NdrFcShort( 0x0 ),	/* 0 */
/* 230 */	NdrFcShort( 0x0 ),	/* 0 */
/* 232 */	NdrFcShort( 0x0 ),	/* 0 */
/* 234 */	NdrFcShort( 0x2 ),	/* 2 */
/* 236 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 238 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IUpdateStateSystem0005 */


	/* Parameter __MIDL__IUpdateState0005 */

/* 240 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 242 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 244 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 246 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 248 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 250 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_errorCategory */


	/* Procedure get_errorCategory */

/* 252 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 254 */	NdrFcLong( 0x0 ),	/* 0 */
/* 258 */	NdrFcShort( 0x9 ),	/* 9 */
/* 260 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 262 */	NdrFcShort( 0x0 ),	/* 0 */
/* 264 */	NdrFcShort( 0x24 ),	/* 36 */
/* 266 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 268 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 270 */	NdrFcShort( 0x0 ),	/* 0 */
/* 272 */	NdrFcShort( 0x0 ),	/* 0 */
/* 274 */	NdrFcShort( 0x0 ),	/* 0 */
/* 276 */	NdrFcShort( 0x2 ),	/* 2 */
/* 278 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 280 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IUpdateStateSystem0006 */


	/* Parameter __MIDL__IUpdateState0006 */

/* 282 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 284 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 286 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 288 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 290 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 292 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_errorCode */


	/* Procedure get_errorCode */

/* 294 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 296 */	NdrFcLong( 0x0 ),	/* 0 */
/* 300 */	NdrFcShort( 0xa ),	/* 10 */
/* 302 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 304 */	NdrFcShort( 0x0 ),	/* 0 */
/* 306 */	NdrFcShort( 0x24 ),	/* 36 */
/* 308 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 310 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 312 */	NdrFcShort( 0x0 ),	/* 0 */
/* 314 */	NdrFcShort( 0x0 ),	/* 0 */
/* 316 */	NdrFcShort( 0x0 ),	/* 0 */
/* 318 */	NdrFcShort( 0x2 ),	/* 2 */
/* 320 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 322 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IUpdateStateSystem0007 */


	/* Parameter __MIDL__IUpdateState0007 */

/* 324 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 326 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 328 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 330 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 332 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 334 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_extraCode1 */


	/* Procedure get_extraCode1 */

/* 336 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 338 */	NdrFcLong( 0x0 ),	/* 0 */
/* 342 */	NdrFcShort( 0xb ),	/* 11 */
/* 344 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 346 */	NdrFcShort( 0x0 ),	/* 0 */
/* 348 */	NdrFcShort( 0x24 ),	/* 36 */
/* 350 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 352 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 354 */	NdrFcShort( 0x0 ),	/* 0 */
/* 356 */	NdrFcShort( 0x0 ),	/* 0 */
/* 358 */	NdrFcShort( 0x0 ),	/* 0 */
/* 360 */	NdrFcShort( 0x2 ),	/* 2 */
/* 362 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 364 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IUpdateStateSystem0008 */


	/* Parameter __MIDL__IUpdateState0008 */

/* 366 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 368 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 370 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 372 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 374 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 376 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_ecp */


	/* Procedure get_ecp */


	/* Procedure get_installerText */


	/* Procedure get_installerText */

/* 378 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 380 */	NdrFcLong( 0x0 ),	/* 0 */
/* 384 */	NdrFcShort( 0xc ),	/* 12 */
/* 386 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 388 */	NdrFcShort( 0x0 ),	/* 0 */
/* 390 */	NdrFcShort( 0x8 ),	/* 8 */
/* 392 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 394 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 396 */	NdrFcShort( 0x1 ),	/* 1 */
/* 398 */	NdrFcShort( 0x0 ),	/* 0 */
/* 400 */	NdrFcShort( 0x0 ),	/* 0 */
/* 402 */	NdrFcShort( 0x2 ),	/* 2 */
/* 404 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 406 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IUpdaterAppStateSystem0005 */


	/* Parameter __MIDL__IUpdaterAppState0005 */


	/* Parameter __MIDL__IUpdateStateSystem0009 */


	/* Parameter __MIDL__IUpdateState0009 */

/* 408 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 410 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 412 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 414 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 416 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 418 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installerCommandLine */


	/* Procedure get_installerCommandLine */

/* 420 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 422 */	NdrFcLong( 0x0 ),	/* 0 */
/* 426 */	NdrFcShort( 0xd ),	/* 13 */
/* 428 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 430 */	NdrFcShort( 0x0 ),	/* 0 */
/* 432 */	NdrFcShort( 0x8 ),	/* 8 */
/* 434 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 436 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 438 */	NdrFcShort( 0x1 ),	/* 1 */
/* 440 */	NdrFcShort( 0x0 ),	/* 0 */
/* 442 */	NdrFcShort( 0x0 ),	/* 0 */
/* 444 */	NdrFcShort( 0x2 ),	/* 2 */
/* 446 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 448 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IUpdateStateSystem0010 */


	/* Parameter __MIDL__IUpdateState0010 */

/* 450 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 452 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 454 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */

/* 456 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 458 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 460 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnStateChange */

/* 462 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 464 */	NdrFcLong( 0x0 ),	/* 0 */
/* 468 */	NdrFcShort( 0x3 ),	/* 3 */
/* 470 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 472 */	NdrFcShort( 0x0 ),	/* 0 */
/* 474 */	NdrFcShort( 0x8 ),	/* 8 */
/* 476 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 478 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 480 */	NdrFcShort( 0x0 ),	/* 0 */
/* 482 */	NdrFcShort( 0x0 ),	/* 0 */
/* 484 */	NdrFcShort( 0x0 ),	/* 0 */
/* 486 */	NdrFcShort( 0x2 ),	/* 2 */
/* 488 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 490 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter update_state */

/* 492 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 494 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 496 */	NdrFcShort( 0x32 ),	/* Type Offset=50 */

	/* Return value */

/* 498 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 500 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 502 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnComplete */

/* 504 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 506 */	NdrFcLong( 0x0 ),	/* 0 */
/* 510 */	NdrFcShort( 0x4 ),	/* 4 */
/* 512 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 514 */	NdrFcShort( 0x0 ),	/* 0 */
/* 516 */	NdrFcShort( 0x8 ),	/* 8 */
/* 518 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 520 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 522 */	NdrFcShort( 0x0 ),	/* 0 */
/* 524 */	NdrFcShort( 0x0 ),	/* 0 */
/* 526 */	NdrFcShort( 0x0 ),	/* 0 */
/* 528 */	NdrFcShort( 0x2 ),	/* 2 */
/* 530 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 532 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter status */

/* 534 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 536 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 538 */	NdrFcShort( 0x44 ),	/* Type Offset=68 */

	/* Return value */

/* 540 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 542 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 544 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnStateChange */

/* 546 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 548 */	NdrFcLong( 0x0 ),	/* 0 */
/* 552 */	NdrFcShort( 0x3 ),	/* 3 */
/* 554 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 556 */	NdrFcShort( 0x0 ),	/* 0 */
/* 558 */	NdrFcShort( 0x8 ),	/* 8 */
/* 560 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 562 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 564 */	NdrFcShort( 0x0 ),	/* 0 */
/* 566 */	NdrFcShort( 0x0 ),	/* 0 */
/* 568 */	NdrFcShort( 0x0 ),	/* 0 */
/* 570 */	NdrFcShort( 0x2 ),	/* 2 */
/* 572 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 574 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter update_state */

/* 576 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 578 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 580 */	NdrFcShort( 0x56 ),	/* Type Offset=86 */

	/* Return value */

/* 582 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 584 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 586 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnComplete */

/* 588 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 590 */	NdrFcLong( 0x0 ),	/* 0 */
/* 594 */	NdrFcShort( 0x4 ),	/* 4 */
/* 596 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 598 */	NdrFcShort( 0x0 ),	/* 0 */
/* 600 */	NdrFcShort( 0x8 ),	/* 8 */
/* 602 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 604 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 606 */	NdrFcShort( 0x0 ),	/* 0 */
/* 608 */	NdrFcShort( 0x0 ),	/* 0 */
/* 610 */	NdrFcShort( 0x0 ),	/* 0 */
/* 612 */	NdrFcShort( 0x2 ),	/* 2 */
/* 614 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 616 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter status */

/* 618 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 620 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 622 */	NdrFcShort( 0x68 ),	/* Type Offset=104 */

	/* Return value */

/* 624 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 626 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 628 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Run */


	/* Procedure Run */

/* 630 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 632 */	NdrFcLong( 0x0 ),	/* 0 */
/* 636 */	NdrFcShort( 0x3 ),	/* 3 */
/* 638 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 640 */	NdrFcShort( 0x8 ),	/* 8 */
/* 642 */	NdrFcShort( 0x8 ),	/* 8 */
/* 644 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 646 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 648 */	NdrFcShort( 0x0 ),	/* 0 */
/* 650 */	NdrFcShort( 0x0 ),	/* 0 */
/* 652 */	NdrFcShort( 0x0 ),	/* 0 */
/* 654 */	NdrFcShort( 0x2 ),	/* 2 */
/* 656 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 658 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter result */


	/* Parameter result */

/* 660 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 662 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 664 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 666 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 668 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 670 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_appId */


	/* Procedure get_appId */

/* 672 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 674 */	NdrFcLong( 0x0 ),	/* 0 */
/* 678 */	NdrFcShort( 0x7 ),	/* 7 */
/* 680 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 682 */	NdrFcShort( 0x0 ),	/* 0 */
/* 684 */	NdrFcShort( 0x8 ),	/* 8 */
/* 686 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 688 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 690 */	NdrFcShort( 0x1 ),	/* 1 */
/* 692 */	NdrFcShort( 0x0 ),	/* 0 */
/* 694 */	NdrFcShort( 0x0 ),	/* 0 */
/* 696 */	NdrFcShort( 0x2 ),	/* 2 */
/* 698 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 700 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IUpdaterAppStateSystem0000 */


	/* Parameter __MIDL__IUpdaterAppState0000 */

/* 702 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 704 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 706 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */

/* 708 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 710 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 712 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_version */


	/* Procedure get_version */

/* 714 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 716 */	NdrFcLong( 0x0 ),	/* 0 */
/* 720 */	NdrFcShort( 0x8 ),	/* 8 */
/* 722 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 724 */	NdrFcShort( 0x0 ),	/* 0 */
/* 726 */	NdrFcShort( 0x8 ),	/* 8 */
/* 728 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 730 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 732 */	NdrFcShort( 0x1 ),	/* 1 */
/* 734 */	NdrFcShort( 0x0 ),	/* 0 */
/* 736 */	NdrFcShort( 0x0 ),	/* 0 */
/* 738 */	NdrFcShort( 0x2 ),	/* 2 */
/* 740 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 742 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IUpdaterAppStateSystem0001 */


	/* Parameter __MIDL__IUpdaterAppState0001 */

/* 744 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 746 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 748 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */

/* 750 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 752 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 754 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_ap */


	/* Procedure get_ap */

/* 756 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 758 */	NdrFcLong( 0x0 ),	/* 0 */
/* 762 */	NdrFcShort( 0x9 ),	/* 9 */
/* 764 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 766 */	NdrFcShort( 0x0 ),	/* 0 */
/* 768 */	NdrFcShort( 0x8 ),	/* 8 */
/* 770 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 772 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 774 */	NdrFcShort( 0x1 ),	/* 1 */
/* 776 */	NdrFcShort( 0x0 ),	/* 0 */
/* 778 */	NdrFcShort( 0x0 ),	/* 0 */
/* 780 */	NdrFcShort( 0x2 ),	/* 2 */
/* 782 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 784 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IUpdaterAppStateSystem0002 */


	/* Parameter __MIDL__IUpdaterAppState0002 */

/* 786 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 788 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 790 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */

/* 792 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 794 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 796 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_brandCode */


	/* Procedure get_brandCode */

/* 798 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 800 */	NdrFcLong( 0x0 ),	/* 0 */
/* 804 */	NdrFcShort( 0xa ),	/* 10 */
/* 806 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 808 */	NdrFcShort( 0x0 ),	/* 0 */
/* 810 */	NdrFcShort( 0x8 ),	/* 8 */
/* 812 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 814 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 816 */	NdrFcShort( 0x1 ),	/* 1 */
/* 818 */	NdrFcShort( 0x0 ),	/* 0 */
/* 820 */	NdrFcShort( 0x0 ),	/* 0 */
/* 822 */	NdrFcShort( 0x2 ),	/* 2 */
/* 824 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 826 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IUpdaterAppStateSystem0003 */


	/* Parameter __MIDL__IUpdaterAppState0003 */

/* 828 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 830 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 832 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */

/* 834 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 836 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 838 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_brandPath */


	/* Procedure get_brandPath */

/* 840 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 842 */	NdrFcLong( 0x0 ),	/* 0 */
/* 846 */	NdrFcShort( 0xb ),	/* 11 */
/* 848 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 850 */	NdrFcShort( 0x0 ),	/* 0 */
/* 852 */	NdrFcShort( 0x8 ),	/* 8 */
/* 854 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 856 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 858 */	NdrFcShort( 0x1 ),	/* 1 */
/* 860 */	NdrFcShort( 0x0 ),	/* 0 */
/* 862 */	NdrFcShort( 0x0 ),	/* 0 */
/* 864 */	NdrFcShort( 0x2 ),	/* 2 */
/* 866 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 868 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IUpdaterAppStateSystem0004 */


	/* Parameter __MIDL__IUpdaterAppState0004 */

/* 870 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 872 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 874 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */

/* 876 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 878 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 880 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Run */


	/* Procedure Run */

/* 882 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 884 */	NdrFcLong( 0x0 ),	/* 0 */
/* 888 */	NdrFcShort( 0x3 ),	/* 3 */
/* 890 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 892 */	NdrFcShort( 0x0 ),	/* 0 */
/* 894 */	NdrFcShort( 0x8 ),	/* 8 */
/* 896 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 898 */	0xe,		/* 14 */
			0x85,		/* Ext Flags:  new corr desc, srv corr check, has big byval param */
/* 900 */	NdrFcShort( 0x0 ),	/* 0 */
/* 902 */	NdrFcShort( 0x1 ),	/* 1 */
/* 904 */	NdrFcShort( 0x0 ),	/* 0 */
/* 906 */	NdrFcShort( 0x2 ),	/* 2 */
/* 908 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 910 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter app_states */


	/* Parameter app_states */

/* 912 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 914 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 916 */	NdrFcShort( 0x434 ),	/* Type Offset=1076 */

	/* Return value */


	/* Return value */

/* 918 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 920 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 922 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure GetVersion */


	/* Procedure GetVersion */

/* 924 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 926 */	NdrFcLong( 0x0 ),	/* 0 */
/* 930 */	NdrFcShort( 0x3 ),	/* 3 */
/* 932 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 934 */	NdrFcShort( 0x0 ),	/* 0 */
/* 936 */	NdrFcShort( 0x8 ),	/* 8 */
/* 938 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 940 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 942 */	NdrFcShort( 0x1 ),	/* 1 */
/* 944 */	NdrFcShort( 0x0 ),	/* 0 */
/* 946 */	NdrFcShort( 0x0 ),	/* 0 */
/* 948 */	NdrFcShort( 0x2 ),	/* 2 */
/* 950 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 952 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter version */


	/* Parameter version */

/* 954 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 956 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 958 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */

/* 960 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 962 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 964 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure FetchPolicies */

/* 966 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 968 */	NdrFcLong( 0x0 ),	/* 0 */
/* 972 */	NdrFcShort( 0x4 ),	/* 4 */
/* 974 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 976 */	NdrFcShort( 0x0 ),	/* 0 */
/* 978 */	NdrFcShort( 0x8 ),	/* 8 */
/* 980 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 982 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 984 */	NdrFcShort( 0x0 ),	/* 0 */
/* 986 */	NdrFcShort( 0x0 ),	/* 0 */
/* 988 */	NdrFcShort( 0x0 ),	/* 0 */
/* 990 */	NdrFcShort( 0x2 ),	/* 2 */
/* 992 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 994 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter callback */

/* 996 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 998 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1000 */	NdrFcShort( 0x43e ),	/* Type Offset=1086 */

	/* Return value */

/* 1002 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1004 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1006 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RegisterApp */

/* 1008 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1010 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1014 */	NdrFcShort( 0x5 ),	/* 5 */
/* 1016 */	NdrFcShort( 0x48 ),	/* ARM64 Stack size/offset = 72 */
/* 1018 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1020 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1022 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x8,		/* 8 */
/* 1024 */	0x14,		/* 20 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1026 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1028 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1030 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1032 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1034 */	0x8,		/* 8 */
			0x80,		/* 128 */
/* 1036 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1038 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1040 */	0x85,		/* 133 */
			0x86,		/* 134 */
/* 1042 */	0x87,		/* 135 */
			0x0,		/* 0 */

	/* Parameter app_id */

/* 1044 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1046 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1048 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter brand_code */

/* 1050 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1052 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1054 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter brand_path */

/* 1056 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1058 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1060 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter tag */

/* 1062 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1064 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1066 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter version */

/* 1068 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1070 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1072 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter existence_checker_path */

/* 1074 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1076 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1078 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter callback */

/* 1080 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1082 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 1084 */	NdrFcShort( 0x43e ),	/* Type Offset=1086 */

	/* Return value */

/* 1086 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1088 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1090 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunPeriodicTasks */

/* 1092 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1094 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1098 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1100 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1102 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1104 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1106 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1108 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1110 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1112 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1114 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1116 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1118 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1120 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter callback */

/* 1122 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1124 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1126 */	NdrFcShort( 0x43e ),	/* Type Offset=1086 */

	/* Return value */

/* 1128 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1130 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1132 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure CheckForUpdate */

/* 1134 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1136 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1140 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1142 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1144 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1146 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1148 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 1150 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1152 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1154 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1156 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1158 */	NdrFcShort( 0x5 ),	/* 5 */
/* 1160 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 1162 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1164 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter app_id */

/* 1166 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1168 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1170 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter priority */

/* 1172 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1174 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1176 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 1178 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1180 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1182 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1184 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1186 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1188 */	NdrFcShort( 0x454 ),	/* Type Offset=1108 */

	/* Return value */

/* 1190 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1192 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1194 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Update */

/* 1196 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1198 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1202 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1204 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 1206 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1208 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1210 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x6,		/* 6 */
/* 1212 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1214 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1216 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1218 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1220 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1222 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 1224 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1226 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1228 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter app_id */

/* 1230 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1232 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1234 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter install_data_index */

/* 1236 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1238 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1240 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter priority */

/* 1242 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1244 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1246 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 1248 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1250 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1252 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1254 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1256 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1258 */	NdrFcShort( 0x454 ),	/* Type Offset=1108 */

	/* Return value */

/* 1260 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1262 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1264 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure UpdateAll */

/* 1266 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1268 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1272 */	NdrFcShort( 0x9 ),	/* 9 */
/* 1274 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1276 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1278 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1280 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1282 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1284 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1286 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1288 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1290 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1292 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1294 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1296 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1298 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1300 */	NdrFcShort( 0x454 ),	/* Type Offset=1108 */

	/* Return value */

/* 1302 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1304 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1306 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Install */

/* 1308 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1310 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1314 */	NdrFcShort( 0xa ),	/* 10 */
/* 1316 */	NdrFcShort( 0x60 ),	/* ARM64 Stack size/offset = 96 */
/* 1318 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1320 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1322 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0xb,		/* 11 */
/* 1324 */	0x16,		/* 22 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1326 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1328 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1330 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1332 */	NdrFcShort( 0xb ),	/* 11 */
/* 1334 */	0xb,		/* 11 */
			0x80,		/* 128 */
/* 1336 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1338 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1340 */	0x85,		/* 133 */
			0x86,		/* 134 */
/* 1342 */	0x87,		/* 135 */
			0xf8,		/* 248 */
/* 1344 */	0xf8,		/* 248 */
			0xf8,		/* 248 */

	/* Parameter app_id */

/* 1346 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1348 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1350 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter brand_code */

/* 1352 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1354 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1356 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter brand_path */

/* 1358 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1360 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1362 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter tag */

/* 1364 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1366 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1368 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter version */

/* 1370 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1372 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1374 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter existence_checker_path */

/* 1376 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1378 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1380 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter client_install_data */

/* 1382 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1384 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 1386 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter install_data_index */

/* 1388 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1390 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1392 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter priority */

/* 1394 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1396 */	NdrFcShort( 0x48 ),	/* ARM64 Stack size/offset = 72 */
/* 1398 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1400 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1402 */	NdrFcShort( 0x50 ),	/* ARM64 Stack size/offset = 80 */
/* 1404 */	NdrFcShort( 0x454 ),	/* Type Offset=1108 */

	/* Return value */

/* 1406 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1408 */	NdrFcShort( 0x58 ),	/* ARM64 Stack size/offset = 88 */
/* 1410 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure CancelInstalls */


	/* Procedure CancelInstalls */

/* 1412 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1414 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1418 */	NdrFcShort( 0xb ),	/* 11 */
/* 1420 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1422 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1424 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1426 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1428 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1430 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1432 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1434 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1436 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1438 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1440 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter app_id */


	/* Parameter app_id */

/* 1442 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1444 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1446 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Return value */


	/* Return value */

/* 1448 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1450 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1452 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunInstaller */

/* 1454 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1456 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1460 */	NdrFcShort( 0xc ),	/* 12 */
/* 1462 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1464 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1466 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1468 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 1470 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1472 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1474 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1476 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1478 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1480 */	0x7,		/* 7 */
			0x80,		/* 128 */
/* 1482 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1484 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1486 */	0x85,		/* 133 */
			0x86,		/* 134 */

	/* Parameter app_id */

/* 1488 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1490 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1492 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter installer_path */

/* 1494 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1496 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1498 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter install_args */

/* 1500 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1502 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1504 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter install_data */

/* 1506 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1508 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1510 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter install_settings */

/* 1512 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1514 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1516 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter observer */

/* 1518 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1520 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1522 */	NdrFcShort( 0x454 ),	/* Type Offset=1108 */

	/* Return value */

/* 1524 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1526 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 1528 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure GetAppStates */

/* 1530 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1532 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1536 */	NdrFcShort( 0xd ),	/* 13 */
/* 1538 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1540 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1542 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1544 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1546 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1548 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1550 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1552 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1554 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1556 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1558 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter callback */

/* 1560 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1562 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1564 */	NdrFcShort( 0x466 ),	/* Type Offset=1126 */

	/* Return value */

/* 1566 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1568 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1570 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure FetchPolicies */

/* 1572 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1574 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1578 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1580 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1582 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1584 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1586 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1588 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1590 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1592 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1594 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1596 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1598 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1600 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter callback */

/* 1602 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1604 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1606 */	NdrFcShort( 0x478 ),	/* Type Offset=1144 */

	/* Return value */

/* 1608 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1610 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1612 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RegisterApp */

/* 1614 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1616 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1620 */	NdrFcShort( 0x5 ),	/* 5 */
/* 1622 */	NdrFcShort( 0x48 ),	/* ARM64 Stack size/offset = 72 */
/* 1624 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1626 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1628 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x8,		/* 8 */
/* 1630 */	0x14,		/* 20 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1632 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1634 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1636 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1638 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1640 */	0x8,		/* 8 */
			0x80,		/* 128 */
/* 1642 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1644 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1646 */	0x85,		/* 133 */
			0x86,		/* 134 */
/* 1648 */	0x87,		/* 135 */
			0x0,		/* 0 */

	/* Parameter app_id */

/* 1650 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1652 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1654 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter brand_code */

/* 1656 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1658 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1660 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter brand_path */

/* 1662 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1664 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1666 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter tag */

/* 1668 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1670 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1672 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter version */

/* 1674 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1676 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1678 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter existence_checker_path */

/* 1680 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1682 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1684 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter callback */

/* 1686 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1688 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 1690 */	NdrFcShort( 0x478 ),	/* Type Offset=1144 */

	/* Return value */

/* 1692 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1694 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1696 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunPeriodicTasks */

/* 1698 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1700 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1704 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1706 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1708 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1710 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1712 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1714 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1716 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1718 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1720 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1722 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1724 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1726 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter callback */

/* 1728 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1730 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1732 */	NdrFcShort( 0x478 ),	/* Type Offset=1144 */

	/* Return value */

/* 1734 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1736 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1738 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure CheckForUpdate */

/* 1740 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1742 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1746 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1748 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1750 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1752 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1754 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 1756 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1758 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1760 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1762 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1764 */	NdrFcShort( 0x5 ),	/* 5 */
/* 1766 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 1768 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1770 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter app_id */

/* 1772 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1774 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1776 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter priority */

/* 1778 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1780 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1782 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 1784 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1786 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1788 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1790 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1792 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1794 */	NdrFcShort( 0x48a ),	/* Type Offset=1162 */

	/* Return value */

/* 1796 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1798 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1800 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Update */

/* 1802 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1804 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1808 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1810 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 1812 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1814 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1816 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x6,		/* 6 */
/* 1818 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1820 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1822 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1824 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1826 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1828 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 1830 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1832 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1834 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter app_id */

/* 1836 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1838 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1840 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter install_data_index */

/* 1842 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1844 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1846 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter priority */

/* 1848 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1850 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1852 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 1854 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1856 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1858 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1860 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1862 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1864 */	NdrFcShort( 0x48a ),	/* Type Offset=1162 */

	/* Return value */

/* 1866 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1868 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1870 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure UpdateAll */

/* 1872 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1874 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1878 */	NdrFcShort( 0x9 ),	/* 9 */
/* 1880 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1882 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1884 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1886 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1888 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1890 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1892 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1894 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1896 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1898 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1900 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1902 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1904 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1906 */	NdrFcShort( 0x48a ),	/* Type Offset=1162 */

	/* Return value */

/* 1908 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1910 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1912 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Install */

/* 1914 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1916 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1920 */	NdrFcShort( 0xa ),	/* 10 */
/* 1922 */	NdrFcShort( 0x60 ),	/* ARM64 Stack size/offset = 96 */
/* 1924 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1926 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1928 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0xb,		/* 11 */
/* 1930 */	0x16,		/* 22 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1932 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1934 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1936 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1938 */	NdrFcShort( 0xb ),	/* 11 */
/* 1940 */	0xb,		/* 11 */
			0x80,		/* 128 */
/* 1942 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1944 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1946 */	0x85,		/* 133 */
			0x86,		/* 134 */
/* 1948 */	0x87,		/* 135 */
			0xf8,		/* 248 */
/* 1950 */	0xf8,		/* 248 */
			0xf8,		/* 248 */

	/* Parameter app_id */

/* 1952 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1954 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1956 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter brand_code */

/* 1958 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1960 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1962 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter brand_path */

/* 1964 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1966 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1968 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter tag */

/* 1970 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1972 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1974 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter version */

/* 1976 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1978 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1980 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter existence_checker_path */

/* 1982 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1984 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1986 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter client_install_data */

/* 1988 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1990 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 1992 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter install_data_index */

/* 1994 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1996 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1998 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter priority */

/* 2000 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2002 */	NdrFcShort( 0x48 ),	/* ARM64 Stack size/offset = 72 */
/* 2004 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 2006 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2008 */	NdrFcShort( 0x50 ),	/* ARM64 Stack size/offset = 80 */
/* 2010 */	NdrFcShort( 0x48a ),	/* Type Offset=1162 */

	/* Return value */

/* 2012 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2014 */	NdrFcShort( 0x58 ),	/* ARM64 Stack size/offset = 88 */
/* 2016 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunInstaller */

/* 2018 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2020 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2024 */	NdrFcShort( 0xc ),	/* 12 */
/* 2026 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 2028 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2030 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2032 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 2034 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2036 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2038 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2040 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2042 */	NdrFcShort( 0x7 ),	/* 7 */
/* 2044 */	0x7,		/* 7 */
			0x80,		/* 128 */
/* 2046 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2048 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 2050 */	0x85,		/* 133 */
			0x86,		/* 134 */

	/* Parameter app_id */

/* 2052 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2054 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2056 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter installer_path */

/* 2058 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2060 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2062 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter install_args */

/* 2064 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2066 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2068 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter install_data */

/* 2070 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2072 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2074 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter install_settings */

/* 2076 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2078 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2080 */	NdrFcShort( 0x452 ),	/* Type Offset=1106 */

	/* Parameter observer */

/* 2082 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2084 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2086 */	NdrFcShort( 0x48a ),	/* Type Offset=1162 */

	/* Return value */

/* 2088 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2090 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2092 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure GetAppStates */

/* 2094 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2096 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2100 */	NdrFcShort( 0xd ),	/* 13 */
/* 2102 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2104 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2106 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2108 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 2110 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2112 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2114 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2116 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2118 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2120 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2122 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter callback */

/* 2124 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2126 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2128 */	NdrFcShort( 0x49c ),	/* Type Offset=1180 */

	/* Return value */

/* 2130 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2132 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2134 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const updater_idl_system_MIDL_TYPE_FORMAT_STRING updater_idl_system__MIDL_TypeFormatString =
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
/* 88 */	NdrFcLong( 0xea6fdc05 ),	/* -361767931 */
/* 92 */	NdrFcShort( 0xcdc5 ),	/* -12859 */
/* 94 */	NdrFcShort( 0x4ea4 ),	/* 20132 */
/* 96 */	0xab,		/* 171 */
			0x41,		/* 65 */
/* 98 */	0xcc,		/* 204 */
			0xbd,		/* 189 */
/* 100 */	0x10,		/* 16 */
			0x40,		/* 64 */
/* 102 */	0xa2,		/* 162 */
			0xb5,		/* 181 */
/* 104 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 106 */	NdrFcLong( 0xe2bd9a6b ),	/* -490890645 */
/* 110 */	NdrFcShort( 0xa19 ),	/* 2585 */
/* 112 */	NdrFcShort( 0x4c89 ),	/* 19593 */
/* 114 */	0xae,		/* 174 */
			0x8b,		/* 139 */
/* 116 */	0xb7,		/* 183 */
			0xe9,		/* 233 */
/* 118 */	0xe5,		/* 229 */
			0x1d,		/* 29 */
/* 120 */	0x9a,		/* 154 */
			0x7,		/* 7 */
/* 122 */	
			0x11, 0x0,	/* FC_RP */
/* 124 */	NdrFcShort( 0x3b8 ),	/* Offset= 952 (1076) */
/* 126 */	
			0x12, 0x0,	/* FC_UP */
/* 128 */	NdrFcShort( 0x3a0 ),	/* Offset= 928 (1056) */
/* 130 */	
			0x2b,		/* FC_NON_ENCAPSULATED_UNION */
			0x9,		/* FC_ULONG */
/* 132 */	0x7,		/* Corr desc: FC_USHORT */
			0x0,		/*  */
/* 134 */	NdrFcShort( 0xfff8 ),	/* -8 */
/* 136 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 138 */	NdrFcShort( 0x2 ),	/* Offset= 2 (140) */
/* 140 */	NdrFcShort( 0x10 ),	/* 16 */
/* 142 */	NdrFcShort( 0x2f ),	/* 47 */
/* 144 */	NdrFcLong( 0x14 ),	/* 20 */
/* 148 */	NdrFcShort( 0x800b ),	/* Simple arm type: FC_HYPER */
/* 150 */	NdrFcLong( 0x3 ),	/* 3 */
/* 154 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 156 */	NdrFcLong( 0x11 ),	/* 17 */
/* 160 */	NdrFcShort( 0x8001 ),	/* Simple arm type: FC_BYTE */
/* 162 */	NdrFcLong( 0x2 ),	/* 2 */
/* 166 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 168 */	NdrFcLong( 0x4 ),	/* 4 */
/* 172 */	NdrFcShort( 0x800a ),	/* Simple arm type: FC_FLOAT */
/* 174 */	NdrFcLong( 0x5 ),	/* 5 */
/* 178 */	NdrFcShort( 0x800c ),	/* Simple arm type: FC_DOUBLE */
/* 180 */	NdrFcLong( 0xb ),	/* 11 */
/* 184 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 186 */	NdrFcLong( 0xa ),	/* 10 */
/* 190 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 192 */	NdrFcLong( 0x6 ),	/* 6 */
/* 196 */	NdrFcShort( 0xe8 ),	/* Offset= 232 (428) */
/* 198 */	NdrFcLong( 0x7 ),	/* 7 */
/* 202 */	NdrFcShort( 0x800c ),	/* Simple arm type: FC_DOUBLE */
/* 204 */	NdrFcLong( 0x8 ),	/* 8 */
/* 208 */	NdrFcShort( 0xe2 ),	/* Offset= 226 (434) */
/* 210 */	NdrFcLong( 0xd ),	/* 13 */
/* 214 */	NdrFcShort( 0xe0 ),	/* Offset= 224 (438) */
/* 216 */	NdrFcLong( 0x9 ),	/* 9 */
/* 220 */	NdrFcShort( 0xec ),	/* Offset= 236 (456) */
/* 222 */	NdrFcLong( 0x2000 ),	/* 8192 */
/* 226 */	NdrFcShort( 0xf8 ),	/* Offset= 248 (474) */
/* 228 */	NdrFcLong( 0x24 ),	/* 36 */
/* 232 */	NdrFcShort( 0x2ee ),	/* Offset= 750 (982) */
/* 234 */	NdrFcLong( 0x4024 ),	/* 16420 */
/* 238 */	NdrFcShort( 0x2e8 ),	/* Offset= 744 (982) */
/* 240 */	NdrFcLong( 0x4011 ),	/* 16401 */
/* 244 */	NdrFcShort( 0x2e6 ),	/* Offset= 742 (986) */
/* 246 */	NdrFcLong( 0x4002 ),	/* 16386 */
/* 250 */	NdrFcShort( 0x2e4 ),	/* Offset= 740 (990) */
/* 252 */	NdrFcLong( 0x4003 ),	/* 16387 */
/* 256 */	NdrFcShort( 0x2e2 ),	/* Offset= 738 (994) */
/* 258 */	NdrFcLong( 0x4014 ),	/* 16404 */
/* 262 */	NdrFcShort( 0x2e0 ),	/* Offset= 736 (998) */
/* 264 */	NdrFcLong( 0x4004 ),	/* 16388 */
/* 268 */	NdrFcShort( 0x2de ),	/* Offset= 734 (1002) */
/* 270 */	NdrFcLong( 0x4005 ),	/* 16389 */
/* 274 */	NdrFcShort( 0x2dc ),	/* Offset= 732 (1006) */
/* 276 */	NdrFcLong( 0x400b ),	/* 16395 */
/* 280 */	NdrFcShort( 0x2c6 ),	/* Offset= 710 (990) */
/* 282 */	NdrFcLong( 0x400a ),	/* 16394 */
/* 286 */	NdrFcShort( 0x2c4 ),	/* Offset= 708 (994) */
/* 288 */	NdrFcLong( 0x4006 ),	/* 16390 */
/* 292 */	NdrFcShort( 0x2ce ),	/* Offset= 718 (1010) */
/* 294 */	NdrFcLong( 0x4007 ),	/* 16391 */
/* 298 */	NdrFcShort( 0x2c4 ),	/* Offset= 708 (1006) */
/* 300 */	NdrFcLong( 0x4008 ),	/* 16392 */
/* 304 */	NdrFcShort( 0x2c6 ),	/* Offset= 710 (1014) */
/* 306 */	NdrFcLong( 0x400d ),	/* 16397 */
/* 310 */	NdrFcShort( 0x2c4 ),	/* Offset= 708 (1018) */
/* 312 */	NdrFcLong( 0x4009 ),	/* 16393 */
/* 316 */	NdrFcShort( 0x2c2 ),	/* Offset= 706 (1022) */
/* 318 */	NdrFcLong( 0x6000 ),	/* 24576 */
/* 322 */	NdrFcShort( 0x2c0 ),	/* Offset= 704 (1026) */
/* 324 */	NdrFcLong( 0x400c ),	/* 16396 */
/* 328 */	NdrFcShort( 0x2be ),	/* Offset= 702 (1030) */
/* 330 */	NdrFcLong( 0x10 ),	/* 16 */
/* 334 */	NdrFcShort( 0x8002 ),	/* Simple arm type: FC_CHAR */
/* 336 */	NdrFcLong( 0x12 ),	/* 18 */
/* 340 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 342 */	NdrFcLong( 0x13 ),	/* 19 */
/* 346 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 348 */	NdrFcLong( 0x15 ),	/* 21 */
/* 352 */	NdrFcShort( 0x800b ),	/* Simple arm type: FC_HYPER */
/* 354 */	NdrFcLong( 0x16 ),	/* 22 */
/* 358 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 360 */	NdrFcLong( 0x17 ),	/* 23 */
/* 364 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 366 */	NdrFcLong( 0xe ),	/* 14 */
/* 370 */	NdrFcShort( 0x29c ),	/* Offset= 668 (1038) */
/* 372 */	NdrFcLong( 0x400e ),	/* 16398 */
/* 376 */	NdrFcShort( 0x2a0 ),	/* Offset= 672 (1048) */
/* 378 */	NdrFcLong( 0x4010 ),	/* 16400 */
/* 382 */	NdrFcShort( 0x29e ),	/* Offset= 670 (1052) */
/* 384 */	NdrFcLong( 0x4012 ),	/* 16402 */
/* 388 */	NdrFcShort( 0x25a ),	/* Offset= 602 (990) */
/* 390 */	NdrFcLong( 0x4013 ),	/* 16403 */
/* 394 */	NdrFcShort( 0x258 ),	/* Offset= 600 (994) */
/* 396 */	NdrFcLong( 0x4015 ),	/* 16405 */
/* 400 */	NdrFcShort( 0x256 ),	/* Offset= 598 (998) */
/* 402 */	NdrFcLong( 0x4016 ),	/* 16406 */
/* 406 */	NdrFcShort( 0x24c ),	/* Offset= 588 (994) */
/* 408 */	NdrFcLong( 0x4017 ),	/* 16407 */
/* 412 */	NdrFcShort( 0x246 ),	/* Offset= 582 (994) */
/* 414 */	NdrFcLong( 0x0 ),	/* 0 */
/* 418 */	NdrFcShort( 0x0 ),	/* Offset= 0 (418) */
/* 420 */	NdrFcLong( 0x1 ),	/* 1 */
/* 424 */	NdrFcShort( 0x0 ),	/* Offset= 0 (424) */
/* 426 */	NdrFcShort( 0xffff ),	/* Offset= -1 (425) */
/* 428 */	
			0x15,		/* FC_STRUCT */
			0x7,		/* 7 */
/* 430 */	NdrFcShort( 0x8 ),	/* 8 */
/* 432 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 434 */	
			0x12, 0x0,	/* FC_UP */
/* 436 */	NdrFcShort( 0xfe66 ),	/* Offset= -410 (26) */
/* 438 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 440 */	NdrFcLong( 0x0 ),	/* 0 */
/* 444 */	NdrFcShort( 0x0 ),	/* 0 */
/* 446 */	NdrFcShort( 0x0 ),	/* 0 */
/* 448 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 450 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 452 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 454 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 456 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 458 */	NdrFcLong( 0x20400 ),	/* 132096 */
/* 462 */	NdrFcShort( 0x0 ),	/* 0 */
/* 464 */	NdrFcShort( 0x0 ),	/* 0 */
/* 466 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 468 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 470 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 472 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 474 */	
			0x12, 0x10,	/* FC_UP [pointer_deref] */
/* 476 */	NdrFcShort( 0x2 ),	/* Offset= 2 (478) */
/* 478 */	
			0x12, 0x0,	/* FC_UP */
/* 480 */	NdrFcShort( 0x1e4 ),	/* Offset= 484 (964) */
/* 482 */	
			0x2a,		/* FC_ENCAPSULATED_UNION */
			0x89,		/* 137 */
/* 484 */	NdrFcShort( 0x20 ),	/* 32 */
/* 486 */	NdrFcShort( 0xa ),	/* 10 */
/* 488 */	NdrFcLong( 0x8 ),	/* 8 */
/* 492 */	NdrFcShort( 0x50 ),	/* Offset= 80 (572) */
/* 494 */	NdrFcLong( 0xd ),	/* 13 */
/* 498 */	NdrFcShort( 0x70 ),	/* Offset= 112 (610) */
/* 500 */	NdrFcLong( 0x9 ),	/* 9 */
/* 504 */	NdrFcShort( 0x90 ),	/* Offset= 144 (648) */
/* 506 */	NdrFcLong( 0xc ),	/* 12 */
/* 510 */	NdrFcShort( 0xb0 ),	/* Offset= 176 (686) */
/* 512 */	NdrFcLong( 0x24 ),	/* 36 */
/* 516 */	NdrFcShort( 0x102 ),	/* Offset= 258 (774) */
/* 518 */	NdrFcLong( 0x800d ),	/* 32781 */
/* 522 */	NdrFcShort( 0x11e ),	/* Offset= 286 (808) */
/* 524 */	NdrFcLong( 0x10 ),	/* 16 */
/* 528 */	NdrFcShort( 0x138 ),	/* Offset= 312 (840) */
/* 530 */	NdrFcLong( 0x2 ),	/* 2 */
/* 534 */	NdrFcShort( 0x14e ),	/* Offset= 334 (868) */
/* 536 */	NdrFcLong( 0x3 ),	/* 3 */
/* 540 */	NdrFcShort( 0x164 ),	/* Offset= 356 (896) */
/* 542 */	NdrFcLong( 0x14 ),	/* 20 */
/* 546 */	NdrFcShort( 0x17a ),	/* Offset= 378 (924) */
/* 548 */	NdrFcShort( 0xffff ),	/* Offset= -1 (547) */
/* 550 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 552 */	NdrFcShort( 0x0 ),	/* 0 */
/* 554 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 556 */	NdrFcShort( 0x0 ),	/* 0 */
/* 558 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 560 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 564 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 566 */	
			0x12, 0x0,	/* FC_UP */
/* 568 */	NdrFcShort( 0xfde2 ),	/* Offset= -542 (26) */
/* 570 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 572 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 574 */	NdrFcShort( 0x10 ),	/* 16 */
/* 576 */	NdrFcShort( 0x0 ),	/* 0 */
/* 578 */	NdrFcShort( 0x6 ),	/* Offset= 6 (584) */
/* 580 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 582 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 584 */	
			0x11, 0x0,	/* FC_RP */
/* 586 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (550) */
/* 588 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 590 */	NdrFcShort( 0x0 ),	/* 0 */
/* 592 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 594 */	NdrFcShort( 0x0 ),	/* 0 */
/* 596 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 598 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 602 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 604 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 606 */	NdrFcShort( 0xff58 ),	/* Offset= -168 (438) */
/* 608 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 610 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 612 */	NdrFcShort( 0x10 ),	/* 16 */
/* 614 */	NdrFcShort( 0x0 ),	/* 0 */
/* 616 */	NdrFcShort( 0x6 ),	/* Offset= 6 (622) */
/* 618 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 620 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 622 */	
			0x11, 0x0,	/* FC_RP */
/* 624 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (588) */
/* 626 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 628 */	NdrFcShort( 0x0 ),	/* 0 */
/* 630 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 632 */	NdrFcShort( 0x0 ),	/* 0 */
/* 634 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 636 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 640 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 642 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 644 */	NdrFcShort( 0xff44 ),	/* Offset= -188 (456) */
/* 646 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 648 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 650 */	NdrFcShort( 0x10 ),	/* 16 */
/* 652 */	NdrFcShort( 0x0 ),	/* 0 */
/* 654 */	NdrFcShort( 0x6 ),	/* Offset= 6 (660) */
/* 656 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 658 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 660 */	
			0x11, 0x0,	/* FC_RP */
/* 662 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (626) */
/* 664 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 666 */	NdrFcShort( 0x0 ),	/* 0 */
/* 668 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 670 */	NdrFcShort( 0x0 ),	/* 0 */
/* 672 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 674 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 678 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 680 */	
			0x12, 0x0,	/* FC_UP */
/* 682 */	NdrFcShort( 0x176 ),	/* Offset= 374 (1056) */
/* 684 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 686 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 688 */	NdrFcShort( 0x10 ),	/* 16 */
/* 690 */	NdrFcShort( 0x0 ),	/* 0 */
/* 692 */	NdrFcShort( 0x6 ),	/* Offset= 6 (698) */
/* 694 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 696 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 698 */	
			0x11, 0x0,	/* FC_RP */
/* 700 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (664) */
/* 702 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 704 */	NdrFcLong( 0x2f ),	/* 47 */
/* 708 */	NdrFcShort( 0x0 ),	/* 0 */
/* 710 */	NdrFcShort( 0x0 ),	/* 0 */
/* 712 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 714 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 716 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 718 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 720 */	
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 722 */	NdrFcShort( 0x1 ),	/* 1 */
/* 724 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 726 */	NdrFcShort( 0x4 ),	/* 4 */
/* 728 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 730 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 732 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 734 */	NdrFcShort( 0x18 ),	/* 24 */
/* 736 */	NdrFcShort( 0x0 ),	/* 0 */
/* 738 */	NdrFcShort( 0xa ),	/* Offset= 10 (748) */
/* 740 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 742 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 744 */	NdrFcShort( 0xffd6 ),	/* Offset= -42 (702) */
/* 746 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 748 */	
			0x12, 0x0,	/* FC_UP */
/* 750 */	NdrFcShort( 0xffe2 ),	/* Offset= -30 (720) */
/* 752 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 754 */	NdrFcShort( 0x0 ),	/* 0 */
/* 756 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 758 */	NdrFcShort( 0x0 ),	/* 0 */
/* 760 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 762 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 766 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 768 */	
			0x12, 0x0,	/* FC_UP */
/* 770 */	NdrFcShort( 0xffda ),	/* Offset= -38 (732) */
/* 772 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 774 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 776 */	NdrFcShort( 0x10 ),	/* 16 */
/* 778 */	NdrFcShort( 0x0 ),	/* 0 */
/* 780 */	NdrFcShort( 0x6 ),	/* Offset= 6 (786) */
/* 782 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 784 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 786 */	
			0x11, 0x0,	/* FC_RP */
/* 788 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (752) */
/* 790 */	
			0x1d,		/* FC_SMFARRAY */
			0x0,		/* 0 */
/* 792 */	NdrFcShort( 0x8 ),	/* 8 */
/* 794 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 796 */	
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 798 */	NdrFcShort( 0x10 ),	/* 16 */
/* 800 */	0x8,		/* FC_LONG */
			0x6,		/* FC_SHORT */
/* 802 */	0x6,		/* FC_SHORT */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 804 */	0x0,		/* 0 */
			NdrFcShort( 0xfff1 ),	/* Offset= -15 (790) */
			0x5b,		/* FC_END */
/* 808 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 810 */	NdrFcShort( 0x20 ),	/* 32 */
/* 812 */	NdrFcShort( 0x0 ),	/* 0 */
/* 814 */	NdrFcShort( 0xa ),	/* Offset= 10 (824) */
/* 816 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 818 */	0x36,		/* FC_POINTER */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 820 */	0x0,		/* 0 */
			NdrFcShort( 0xffe7 ),	/* Offset= -25 (796) */
			0x5b,		/* FC_END */
/* 824 */	
			0x11, 0x0,	/* FC_RP */
/* 826 */	NdrFcShort( 0xff12 ),	/* Offset= -238 (588) */
/* 828 */	
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 830 */	NdrFcShort( 0x1 ),	/* 1 */
/* 832 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 834 */	NdrFcShort( 0x0 ),	/* 0 */
/* 836 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 838 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 840 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 842 */	NdrFcShort( 0x10 ),	/* 16 */
/* 844 */	NdrFcShort( 0x0 ),	/* 0 */
/* 846 */	NdrFcShort( 0x6 ),	/* Offset= 6 (852) */
/* 848 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 850 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 852 */	
			0x12, 0x0,	/* FC_UP */
/* 854 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (828) */
/* 856 */	
			0x1b,		/* FC_CARRAY */
			0x1,		/* 1 */
/* 858 */	NdrFcShort( 0x2 ),	/* 2 */
/* 860 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 862 */	NdrFcShort( 0x0 ),	/* 0 */
/* 864 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 866 */	0x6,		/* FC_SHORT */
			0x5b,		/* FC_END */
/* 868 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 870 */	NdrFcShort( 0x10 ),	/* 16 */
/* 872 */	NdrFcShort( 0x0 ),	/* 0 */
/* 874 */	NdrFcShort( 0x6 ),	/* Offset= 6 (880) */
/* 876 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 878 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 880 */	
			0x12, 0x0,	/* FC_UP */
/* 882 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (856) */
/* 884 */	
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 886 */	NdrFcShort( 0x4 ),	/* 4 */
/* 888 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 890 */	NdrFcShort( 0x0 ),	/* 0 */
/* 892 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 894 */	0x8,		/* FC_LONG */
			0x5b,		/* FC_END */
/* 896 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 898 */	NdrFcShort( 0x10 ),	/* 16 */
/* 900 */	NdrFcShort( 0x0 ),	/* 0 */
/* 902 */	NdrFcShort( 0x6 ),	/* Offset= 6 (908) */
/* 904 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 906 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 908 */	
			0x12, 0x0,	/* FC_UP */
/* 910 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (884) */
/* 912 */	
			0x1b,		/* FC_CARRAY */
			0x7,		/* 7 */
/* 914 */	NdrFcShort( 0x8 ),	/* 8 */
/* 916 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 918 */	NdrFcShort( 0x0 ),	/* 0 */
/* 920 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 922 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 924 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 926 */	NdrFcShort( 0x10 ),	/* 16 */
/* 928 */	NdrFcShort( 0x0 ),	/* 0 */
/* 930 */	NdrFcShort( 0x6 ),	/* Offset= 6 (936) */
/* 932 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 934 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 936 */	
			0x12, 0x0,	/* FC_UP */
/* 938 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (912) */
/* 940 */	
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 942 */	NdrFcShort( 0x8 ),	/* 8 */
/* 944 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 946 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 948 */	
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 950 */	NdrFcShort( 0x8 ),	/* 8 */
/* 952 */	0x7,		/* Corr desc: FC_USHORT */
			0x0,		/*  */
/* 954 */	NdrFcShort( 0xffc8 ),	/* -56 */
/* 956 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 958 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 960 */	NdrFcShort( 0xffec ),	/* Offset= -20 (940) */
/* 962 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 964 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 966 */	NdrFcShort( 0x38 ),	/* 56 */
/* 968 */	NdrFcShort( 0xffec ),	/* Offset= -20 (948) */
/* 970 */	NdrFcShort( 0x0 ),	/* Offset= 0 (970) */
/* 972 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 974 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 976 */	0x40,		/* FC_STRUCTPAD4 */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 978 */	0x0,		/* 0 */
			NdrFcShort( 0xfe0f ),	/* Offset= -497 (482) */
			0x5b,		/* FC_END */
/* 982 */	
			0x12, 0x0,	/* FC_UP */
/* 984 */	NdrFcShort( 0xff04 ),	/* Offset= -252 (732) */
/* 986 */	
			0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 988 */	0x1,		/* FC_BYTE */
			0x5c,		/* FC_PAD */
/* 990 */	
			0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 992 */	0x6,		/* FC_SHORT */
			0x5c,		/* FC_PAD */
/* 994 */	
			0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 996 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */
/* 998 */	
			0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 1000 */	0xb,		/* FC_HYPER */
			0x5c,		/* FC_PAD */
/* 1002 */	
			0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 1004 */	0xa,		/* FC_FLOAT */
			0x5c,		/* FC_PAD */
/* 1006 */	
			0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 1008 */	0xc,		/* FC_DOUBLE */
			0x5c,		/* FC_PAD */
/* 1010 */	
			0x12, 0x0,	/* FC_UP */
/* 1012 */	NdrFcShort( 0xfdb8 ),	/* Offset= -584 (428) */
/* 1014 */	
			0x12, 0x10,	/* FC_UP [pointer_deref] */
/* 1016 */	NdrFcShort( 0xfdba ),	/* Offset= -582 (434) */
/* 1018 */	
			0x12, 0x10,	/* FC_UP [pointer_deref] */
/* 1020 */	NdrFcShort( 0xfdba ),	/* Offset= -582 (438) */
/* 1022 */	
			0x12, 0x10,	/* FC_UP [pointer_deref] */
/* 1024 */	NdrFcShort( 0xfdc8 ),	/* Offset= -568 (456) */
/* 1026 */	
			0x12, 0x10,	/* FC_UP [pointer_deref] */
/* 1028 */	NdrFcShort( 0xfdd6 ),	/* Offset= -554 (474) */
/* 1030 */	
			0x12, 0x10,	/* FC_UP [pointer_deref] */
/* 1032 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1034) */
/* 1034 */	
			0x12, 0x0,	/* FC_UP */
/* 1036 */	NdrFcShort( 0x14 ),	/* Offset= 20 (1056) */
/* 1038 */	
			0x15,		/* FC_STRUCT */
			0x7,		/* 7 */
/* 1040 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1042 */	0x6,		/* FC_SHORT */
			0x1,		/* FC_BYTE */
/* 1044 */	0x1,		/* FC_BYTE */
			0x8,		/* FC_LONG */
/* 1046 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 1048 */	
			0x12, 0x0,	/* FC_UP */
/* 1050 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (1038) */
/* 1052 */	
			0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 1054 */	0x2,		/* FC_CHAR */
			0x5c,		/* FC_PAD */
/* 1056 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x7,		/* 7 */
/* 1058 */	NdrFcShort( 0x20 ),	/* 32 */
/* 1060 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1062 */	NdrFcShort( 0x0 ),	/* Offset= 0 (1062) */
/* 1064 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 1066 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 1068 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 1070 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1072 */	NdrFcShort( 0xfc52 ),	/* Offset= -942 (130) */
/* 1074 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1076 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 1078 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1080 */	NdrFcShort( 0x18 ),	/* 24 */
/* 1082 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1084 */	NdrFcShort( 0xfc42 ),	/* Offset= -958 (126) */
/* 1086 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1088 */	NdrFcLong( 0x8bab6f84 ),	/* -1951699068 */
/* 1092 */	NdrFcShort( 0xad67 ),	/* -21145 */
/* 1094 */	NdrFcShort( 0x4819 ),	/* 18457 */
/* 1096 */	0xb8,		/* 184 */
			0x46,		/* 70 */
/* 1098 */	0xcc,		/* 204 */
			0x89,		/* 137 */
/* 1100 */	0x8,		/* 8 */
			0x80,		/* 128 */
/* 1102 */	0xfd,		/* 253 */
			0x3b,		/* 59 */
/* 1104 */	
			0x11, 0x8,	/* FC_RP [simple_pointer] */
/* 1106 */	
			0x25,		/* FC_C_WSTRING */
			0x5c,		/* FC_PAD */
/* 1108 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1110 */	NdrFcLong( 0x7b416cfd ),	/* 2067885309 */
/* 1114 */	NdrFcShort( 0x4216 ),	/* 16918 */
/* 1116 */	NdrFcShort( 0x4fd6 ),	/* 20438 */
/* 1118 */	0xbd,		/* 189 */
			0x83,		/* 131 */
/* 1120 */	0x7c,		/* 124 */
			0x58,		/* 88 */
/* 1122 */	0x60,		/* 96 */
			0x54,		/* 84 */
/* 1124 */	0x67,		/* 103 */
			0x6e,		/* 110 */
/* 1126 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1128 */	NdrFcLong( 0xefe903c0 ),	/* -269941824 */
/* 1132 */	NdrFcShort( 0xe820 ),	/* -6112 */
/* 1134 */	NdrFcShort( 0x4136 ),	/* 16694 */
/* 1136 */	0x9f,		/* 159 */
			0xae,		/* 174 */
/* 1138 */	0xfd,		/* 253 */
			0xcd,		/* 205 */
/* 1140 */	0x7f,		/* 127 */
			0x25,		/* 37 */
/* 1142 */	0x63,		/* 99 */
			0x2,		/* 2 */
/* 1144 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1146 */	NdrFcLong( 0xf0d6763a ),	/* -254380486 */
/* 1150 */	NdrFcShort( 0x182 ),	/* 386 */
/* 1152 */	NdrFcShort( 0x4136 ),	/* 16694 */
/* 1154 */	0xb1,		/* 177 */
			0xfa,		/* 250 */
/* 1156 */	0x50,		/* 80 */
			0x8e,		/* 142 */
/* 1158 */	0x33,		/* 51 */
			0x4c,		/* 76 */
/* 1160 */	0xff,		/* 255 */
			0xc1,		/* 193 */
/* 1162 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1164 */	NdrFcLong( 0x57b500a ),	/* 91967498 */
/* 1168 */	NdrFcShort( 0x4ba2 ),	/* 19362 */
/* 1170 */	NdrFcShort( 0x496a ),	/* 18794 */
/* 1172 */	0xb1,		/* 177 */
			0xcd,		/* 205 */
/* 1174 */	0xc5,		/* 197 */
			0xde,		/* 222 */
/* 1176 */	0xd3,		/* 211 */
			0xcc,		/* 204 */
/* 1178 */	0xc6,		/* 198 */
			0x1b,		/* 27 */
/* 1180 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1182 */	NdrFcLong( 0x2cb8867e ),	/* 750290558 */
/* 1186 */	NdrFcShort( 0x495e ),	/* 18782 */
/* 1188 */	NdrFcShort( 0x459f ),	/* 17823 */
/* 1190 */	0xb1,		/* 177 */
			0xb6,		/* 182 */
/* 1192 */	0x2d,		/* 45 */
			0xd7,		/* 215 */
/* 1194 */	0xff,		/* 255 */
			0xdb,		/* 219 */
/* 1196 */	0xd4,		/* 212 */
			0x62,		/* 98 */

			0x0
        }
    };

XFG_TRAMPOLINES(BSTR)
XFG_TRAMPOLINES(VARIANT)

static const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ] = 
        {
            
            {
            (USER_MARSHAL_SIZING_ROUTINE)XFG_TRAMPOLINE_FPTR(BSTR_UserSize)
            ,(USER_MARSHAL_MARSHALLING_ROUTINE)XFG_TRAMPOLINE_FPTR(BSTR_UserMarshal)
            ,(USER_MARSHAL_UNMARSHALLING_ROUTINE)XFG_TRAMPOLINE_FPTR(BSTR_UserUnmarshal)
            ,(USER_MARSHAL_FREEING_ROUTINE)XFG_TRAMPOLINE_FPTR(BSTR_UserFree)
            
            }
            ,
            {
            (USER_MARSHAL_SIZING_ROUTINE)XFG_TRAMPOLINE_FPTR(VARIANT_UserSize)
            ,(USER_MARSHAL_MARSHALLING_ROUTINE)XFG_TRAMPOLINE_FPTR(VARIANT_UserMarshal)
            ,(USER_MARSHAL_UNMARSHALLING_ROUTINE)XFG_TRAMPOLINE_FPTR(VARIANT_UserUnmarshal)
            ,(USER_MARSHAL_FREEING_ROUTINE)XFG_TRAMPOLINE_FPTR(VARIANT_UserFree)
            
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
    42,
    84,
    126,
    168,
    210,
    252,
    294,
    336,
    378,
    420
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdateState_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdateState_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdateState_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl_system__MIDL_ProcFormatString.Format,
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


/* Object interface: IUpdateStateSystem, ver. 0.0,
   GUID={0xEA6FDC05,0xCDC5,0x4EA4,{0xAB,0x41,0xCC,0xBD,0x10,0x40,0xA2,0xB5}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdateStateSystem_FormatStringOffsetTable[] =
    {
    0,
    42,
    84,
    126,
    168,
    210,
    252,
    294,
    336,
    378,
    420
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdateStateSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdateStateSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdateStateSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdateStateSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(14) _IUpdateStateSystemProxyVtbl = 
{
    &IUpdateStateSystem_ProxyInfo,
    &IID_IUpdateStateSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdateStateSystem::get_state */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateSystem::get_appId */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateSystem::get_nextVersion */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateSystem::get_downloadedBytes */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateSystem::get_totalBytes */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateSystem::get_installProgress */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateSystem::get_errorCategory */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateSystem::get_errorCode */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateSystem::get_extraCode1 */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateSystem::get_installerText */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateSystem::get_installerCommandLine */
};

const CInterfaceStubVtbl _IUpdateStateSystemStubVtbl =
{
    &IID_IUpdateStateSystem,
    &IUpdateStateSystem_ServerInfo,
    14,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: ICompleteStatus, ver. 0.0,
   GUID={0x2FCD14AF,0xB645,0x4351,{0x83,0x59,0xE8,0x0A,0x0E,0x20,0x2A,0x0B}} */

#pragma code_seg(".orpc")
static const unsigned short ICompleteStatus_FormatStringOffsetTable[] =
    {
    0,
    42
    };

static const MIDL_STUBLESS_PROXY_INFO ICompleteStatus_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &ICompleteStatus_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ICompleteStatus_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl_system__MIDL_ProcFormatString.Format,
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


/* Object interface: ICompleteStatusSystem, ver. 0.0,
   GUID={0xE2BD9A6B,0x0A19,0x4C89,{0xAE,0x8B,0xB7,0xE9,0xE5,0x1D,0x9A,0x07}} */

#pragma code_seg(".orpc")
static const unsigned short ICompleteStatusSystem_FormatStringOffsetTable[] =
    {
    0,
    42
    };

static const MIDL_STUBLESS_PROXY_INFO ICompleteStatusSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &ICompleteStatusSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ICompleteStatusSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &ICompleteStatusSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(5) _ICompleteStatusSystemProxyVtbl = 
{
    &ICompleteStatusSystem_ProxyInfo,
    &IID_ICompleteStatusSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* ICompleteStatusSystem::get_statusCode */ ,
    (void *) (INT_PTR) -1 /* ICompleteStatusSystem::get_statusMessage */
};

const CInterfaceStubVtbl _ICompleteStatusSystemStubVtbl =
{
    &IID_ICompleteStatusSystem,
    &ICompleteStatusSystem_ServerInfo,
    5,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterObserver, ver. 0.0,
   GUID={0x7B416CFD,0x4216,0x4FD6,{0xBD,0x83,0x7C,0x58,0x60,0x54,0x67,0x6E}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterObserver_FormatStringOffsetTable[] =
    {
    462,
    504
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterObserver_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterObserver_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterObserver_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl_system__MIDL_ProcFormatString.Format,
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


/* Object interface: IUpdaterObserverSystem, ver. 0.0,
   GUID={0x057B500A,0x4BA2,0x496A,{0xB1,0xCD,0xC5,0xDE,0xD3,0xCC,0xC6,0x1B}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterObserverSystem_FormatStringOffsetTable[] =
    {
    546,
    588
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterObserverSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterObserverSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterObserverSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl_system__MIDL_ProcFormatString.Format,
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
    630
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterCallback_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterCallback_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterCallback_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl_system__MIDL_ProcFormatString.Format,
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


/* Object interface: IUpdaterCallbackSystem, ver. 0.0,
   GUID={0xF0D6763A,0x0182,0x4136,{0xB1,0xFA,0x50,0x8E,0x33,0x4C,0xFF,0xC1}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterCallbackSystem_FormatStringOffsetTable[] =
    {
    630
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterCallbackSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterCallbackSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterCallbackSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterCallbackSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IUpdaterCallbackSystemProxyVtbl = 
{
    &IUpdaterCallbackSystem_ProxyInfo,
    &IID_IUpdaterCallbackSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterCallbackSystem::Run */
};

const CInterfaceStubVtbl _IUpdaterCallbackSystemStubVtbl =
{
    &IID_IUpdaterCallbackSystem,
    &IUpdaterCallbackSystem_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IDispatch, ver. 0.0,
   GUID={0x00020400,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IUpdaterAppState, ver. 0.0,
   GUID={0xA22AFC54,0x2DEF,0x4578,{0x91,0x87,0xDB,0x3B,0x24,0x38,0x10,0x90}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterAppState_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    672,
    714,
    756,
    798,
    840,
    378
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterAppState_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterAppState_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterAppState_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterAppState_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(13) _IUpdaterAppStateProxyVtbl = 
{
    &IUpdaterAppState_ProxyInfo,
    &IID_IUpdaterAppState,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppState::get_appId */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppState::get_version */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppState::get_ap */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppState::get_brandCode */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppState::get_brandPath */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppState::get_ecp */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IUpdaterAppState_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IUpdaterAppStateStubVtbl =
{
    &IID_IUpdaterAppState,
    &IUpdaterAppState_ServerInfo,
    13,
    &IUpdaterAppState_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IUpdaterAppStateSystem, ver. 0.0,
   GUID={0x92631531,0x8044,0x46F4,{0xB6,0x45,0xCD,0xFB,0xCC,0xC7,0xFA,0x3B}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterAppStateSystem_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    672,
    714,
    756,
    798,
    840,
    378
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterAppStateSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterAppStateSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterAppStateSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterAppStateSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(13) _IUpdaterAppStateSystemProxyVtbl = 
{
    &IUpdaterAppStateSystem_ProxyInfo,
    &IID_IUpdaterAppStateSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppStateSystem::get_appId */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppStateSystem::get_version */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppStateSystem::get_ap */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppStateSystem::get_brandCode */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppStateSystem::get_brandPath */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppStateSystem::get_ecp */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IUpdaterAppStateSystem_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IUpdaterAppStateSystemStubVtbl =
{
    &IID_IUpdaterAppStateSystem,
    &IUpdaterAppStateSystem_ServerInfo,
    13,
    &IUpdaterAppStateSystem_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IUpdaterAppStatesCallback, ver. 0.0,
   GUID={0xEFE903C0,0xE820,0x4136,{0x9F,0xAE,0xFD,0xCD,0x7F,0x25,0x63,0x02}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterAppStatesCallback_FormatStringOffsetTable[] =
    {
    882
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterAppStatesCallback_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterAppStatesCallback_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterAppStatesCallback_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterAppStatesCallback_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IUpdaterAppStatesCallbackProxyVtbl = 
{
    &IUpdaterAppStatesCallback_ProxyInfo,
    &IID_IUpdaterAppStatesCallback,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterAppStatesCallback::Run */
};

const CInterfaceStubVtbl _IUpdaterAppStatesCallbackStubVtbl =
{
    &IID_IUpdaterAppStatesCallback,
    &IUpdaterAppStatesCallback_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterAppStatesCallbackSystem, ver. 0.0,
   GUID={0x2CB8867E,0x495E,0x459F,{0xB1,0xB6,0x2D,0xD7,0xFF,0xDB,0xD4,0x62}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterAppStatesCallbackSystem_FormatStringOffsetTable[] =
    {
    882
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterAppStatesCallbackSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterAppStatesCallbackSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterAppStatesCallbackSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterAppStatesCallbackSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IUpdaterAppStatesCallbackSystemProxyVtbl = 
{
    &IUpdaterAppStatesCallbackSystem_ProxyInfo,
    &IID_IUpdaterAppStatesCallbackSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterAppStatesCallbackSystem::Run */
};

const CInterfaceStubVtbl _IUpdaterAppStatesCallbackSystemStubVtbl =
{
    &IID_IUpdaterAppStatesCallbackSystem,
    &IUpdaterAppStatesCallbackSystem_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdater, ver. 0.0,
   GUID={0x63B8FFB1,0x5314,0x48C9,{0x9C,0x57,0x93,0xEC,0x8B,0xC6,0x18,0x4B}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdater_FormatStringOffsetTable[] =
    {
    924,
    966,
    1008,
    1092,
    1134,
    1196,
    1266,
    1308,
    1412,
    1454,
    1530
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdater_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdater_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdater_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdater_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(14) _IUpdaterProxyVtbl = 
{
    &IUpdater_ProxyInfo,
    &IID_IUpdater,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdater::GetVersion */ ,
    (void *) (INT_PTR) -1 /* IUpdater::FetchPolicies */ ,
    (void *) (INT_PTR) -1 /* IUpdater::RegisterApp */ ,
    (void *) (INT_PTR) -1 /* IUpdater::RunPeriodicTasks */ ,
    (void *) (INT_PTR) -1 /* IUpdater::CheckForUpdate */ ,
    (void *) (INT_PTR) -1 /* IUpdater::Update */ ,
    (void *) (INT_PTR) -1 /* IUpdater::UpdateAll */ ,
    (void *) (INT_PTR) -1 /* IUpdater::Install */ ,
    (void *) (INT_PTR) -1 /* IUpdater::CancelInstalls */ ,
    (void *) (INT_PTR) -1 /* IUpdater::RunInstaller */ ,
    (void *) (INT_PTR) -1 /* IUpdater::GetAppStates */
};

const CInterfaceStubVtbl _IUpdaterStubVtbl =
{
    &IID_IUpdater,
    &IUpdater_ServerInfo,
    14,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterSystem, ver. 0.0,
   GUID={0xFCE335F3,0xA55C,0x496E,{0x81,0x4F,0x85,0x97,0x1C,0x9F,0xA6,0xF1}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterSystem_FormatStringOffsetTable[] =
    {
    924,
    1572,
    1614,
    1698,
    1740,
    1802,
    1872,
    1914,
    1412,
    2018,
    2094
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl_system__MIDL_ProcFormatString.Format,
    &IUpdaterSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(14) _IUpdaterSystemProxyVtbl = 
{
    &IUpdaterSystem_ProxyInfo,
    &IID_IUpdaterSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::GetVersion */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::FetchPolicies */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::RegisterApp */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::RunPeriodicTasks */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::CheckForUpdate */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::Update */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::UpdateAll */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::Install */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::CancelInstalls */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::RunInstaller */ ,
    (void *) (INT_PTR) -1 /* IUpdaterSystem::GetAppStates */
};

const CInterfaceStubVtbl _IUpdaterSystemStubVtbl =
{
    &IID_IUpdaterSystem,
    &IUpdaterSystem_ServerInfo,
    14,
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
    updater_idl_system__MIDL_TypeFormatString.Format,
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

const CInterfaceProxyVtbl * const _updater_idl_system_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_IUpdateStateSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterObserverSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdateStateProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterAppStateSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterCallbackSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterAppStateProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ICompleteStatusSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterAppStatesCallbackSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterCallbackProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ICompleteStatusProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterAppStatesCallbackProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterObserverProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _updater_idl_system_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_IUpdateStateSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterObserverSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdateStateStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterAppStateSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterCallbackSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterAppStateStubVtbl,
    ( CInterfaceStubVtbl *) &_ICompleteStatusSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterAppStatesCallbackSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterCallbackStubVtbl,
    ( CInterfaceStubVtbl *) &_ICompleteStatusStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterAppStatesCallbackStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterObserverStubVtbl,
    0
};

PCInterfaceName const _updater_idl_system_InterfaceNamesList[] = 
{
    "IUpdateStateSystem",
    "IUpdaterObserverSystem",
    "IUpdateState",
    "IUpdaterAppStateSystem",
    "IUpdaterCallbackSystem",
    "IUpdaterAppState",
    "ICompleteStatusSystem",
    "IUpdaterAppStatesCallbackSystem",
    "IUpdaterCallback",
    "ICompleteStatus",
    "IUpdater",
    "IUpdaterAppStatesCallback",
    "IUpdaterSystem",
    "IUpdaterObserver",
    0
};

const IID *  const _updater_idl_system_BaseIIDList[] = 
{
    0,
    0,
    0,
    &IID_IDispatch,
    0,
    &IID_IDispatch,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};


#define _updater_idl_system_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _updater_idl_system, pIID, n)

int __stdcall _updater_idl_system_IID_Lookup( const IID * pIID, int * pIndex )
{
    IID_BS_LOOKUP_SETUP

    IID_BS_LOOKUP_INITIAL_TEST( _updater_idl_system, 14, 8 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_idl_system, 4 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_idl_system, 2 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_idl_system, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _updater_idl_system, 14, *pIndex )
    
}

EXTERN_C const ExtendedProxyFileInfo updater_idl_system_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _updater_idl_system_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _updater_idl_system_StubVtblList,
    (const PCInterfaceName * ) & _updater_idl_system_InterfaceNamesList,
    (const IID ** ) & _updater_idl_system_BaseIIDList,
    & _updater_idl_system_IID_Lookup, 
    14,
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

