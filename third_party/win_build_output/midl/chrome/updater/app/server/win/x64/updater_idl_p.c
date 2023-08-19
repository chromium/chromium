

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

#define TYPE_FORMAT_STRING_SIZE   1289                              
#define PROC_FORMAT_STRING_SIZE   2501                              
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   2            

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


extern const MIDL_SERVER_INFO IUpdaterAppState_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterAppState_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IUpdaterAppStateUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterAppStateUser_ProxyInfo;

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


extern const MIDL_SERVER_INFO IUpdaterAppStatesCallbackUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterAppStatesCallbackUser_ProxyInfo;

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


	/* Procedure get_statusCode */


	/* Procedure get_statusCode */


	/* Procedure get_state */


	/* Procedure get_state */


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

	/* Parameter __MIDL__ICompleteStatusSystem0000 */


	/* Parameter __MIDL__ICompleteStatusUser0000 */


	/* Parameter __MIDL__ICompleteStatus0000 */


	/* Parameter __MIDL__IUpdateStateSystem0000 */


	/* Parameter __MIDL__IUpdateStateUser0000 */


	/* Parameter __MIDL__IUpdateState0000 */

/* 26 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 28 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 30 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 32 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 34 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 36 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_statusMessage */


	/* Procedure get_statusMessage */


	/* Procedure get_statusMessage */


	/* Procedure get_appId */


	/* Procedure get_appId */


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

	/* Parameter __MIDL__ICompleteStatusSystem0001 */


	/* Parameter __MIDL__ICompleteStatusUser0001 */


	/* Parameter __MIDL__ICompleteStatus0001 */


	/* Parameter __MIDL__IUpdateStateSystem0001 */


	/* Parameter __MIDL__IUpdateStateUser0001 */


	/* Parameter __MIDL__IUpdateState0001 */

/* 64 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 66 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 68 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 70 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 72 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 74 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nextVersion */


	/* Procedure get_nextVersion */


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

	/* Parameter __MIDL__IUpdateStateSystem0002 */


	/* Parameter __MIDL__IUpdateStateUser0002 */


	/* Parameter __MIDL__IUpdateState0002 */

/* 102 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 104 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 106 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 108 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 110 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 112 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_downloadedBytes */


	/* Procedure get_downloadedBytes */


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

	/* Parameter __MIDL__IUpdateStateSystem0003 */


	/* Parameter __MIDL__IUpdateStateUser0003 */


	/* Parameter __MIDL__IUpdateState0003 */

/* 140 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 142 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 144 */	0xb,		/* FC_HYPER */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 146 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 148 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 150 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_totalBytes */


	/* Procedure get_totalBytes */


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

	/* Parameter __MIDL__IUpdateStateSystem0004 */


	/* Parameter __MIDL__IUpdateStateUser0004 */


	/* Parameter __MIDL__IUpdateState0004 */

/* 178 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 180 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 182 */	0xb,		/* FC_HYPER */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 184 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 186 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 188 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installProgress */


	/* Procedure get_installProgress */


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

	/* Parameter __MIDL__IUpdateStateSystem0005 */


	/* Parameter __MIDL__IUpdateStateUser0005 */


	/* Parameter __MIDL__IUpdateState0005 */

/* 216 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 218 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 220 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 222 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 224 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 226 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_errorCategory */


	/* Procedure get_errorCategory */


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

	/* Parameter __MIDL__IUpdateStateSystem0006 */


	/* Parameter __MIDL__IUpdateStateUser0006 */


	/* Parameter __MIDL__IUpdateState0006 */

/* 254 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 256 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 258 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 260 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 262 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 264 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_errorCode */


	/* Procedure get_errorCode */


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

	/* Parameter __MIDL__IUpdateStateSystem0007 */


	/* Parameter __MIDL__IUpdateStateUser0007 */


	/* Parameter __MIDL__IUpdateState0007 */

/* 292 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 294 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 296 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 298 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 300 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 302 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_extraCode1 */


	/* Procedure get_extraCode1 */


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

	/* Parameter __MIDL__IUpdateStateSystem0008 */


	/* Parameter __MIDL__IUpdateStateUser0008 */


	/* Parameter __MIDL__IUpdateState0008 */

/* 330 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 332 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 334 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 336 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 338 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 340 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_ecp */


	/* Procedure get_ecp */


	/* Procedure get_ecp */


	/* Procedure get_installerText */


	/* Procedure get_installerText */


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

	/* Parameter __MIDL__IUpdaterAppStateSystem0005 */


	/* Parameter __MIDL__IUpdaterAppStateUser0005 */


	/* Parameter __MIDL__IUpdaterAppState0005 */


	/* Parameter __MIDL__IUpdateStateSystem0009 */


	/* Parameter __MIDL__IUpdateStateUser0009 */


	/* Parameter __MIDL__IUpdateState0009 */

/* 368 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 370 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 372 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 374 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 376 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 378 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installerCommandLine */


	/* Procedure get_installerCommandLine */


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

	/* Parameter __MIDL__IUpdateStateSystem0010 */


	/* Parameter __MIDL__IUpdateStateUser0010 */


	/* Parameter __MIDL__IUpdateState0010 */

/* 406 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 408 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 410 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */


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


	/* Procedure Run */


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


	/* Parameter result */


	/* Parameter result */

/* 672 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 674 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 676 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 678 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 680 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 682 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_appId */


	/* Procedure get_appId */


	/* Procedure get_appId */

/* 684 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 686 */	NdrFcLong( 0x0 ),	/* 0 */
/* 690 */	NdrFcShort( 0x7 ),	/* 7 */
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

	/* Parameter __MIDL__IUpdaterAppStateSystem0000 */


	/* Parameter __MIDL__IUpdaterAppStateUser0000 */


	/* Parameter __MIDL__IUpdaterAppState0000 */

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

	/* Procedure get_version */


	/* Procedure get_version */


	/* Procedure get_version */

/* 722 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 724 */	NdrFcLong( 0x0 ),	/* 0 */
/* 728 */	NdrFcShort( 0x8 ),	/* 8 */
/* 730 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 732 */	NdrFcShort( 0x0 ),	/* 0 */
/* 734 */	NdrFcShort( 0x8 ),	/* 8 */
/* 736 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 738 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 740 */	NdrFcShort( 0x1 ),	/* 1 */
/* 742 */	NdrFcShort( 0x0 ),	/* 0 */
/* 744 */	NdrFcShort( 0x0 ),	/* 0 */
/* 746 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IUpdaterAppStateSystem0001 */


	/* Parameter __MIDL__IUpdaterAppStateUser0001 */


	/* Parameter __MIDL__IUpdaterAppState0001 */

/* 748 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 750 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 752 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 754 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 756 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 758 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_ap */


	/* Procedure get_ap */


	/* Procedure get_ap */

/* 760 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 762 */	NdrFcLong( 0x0 ),	/* 0 */
/* 766 */	NdrFcShort( 0x9 ),	/* 9 */
/* 768 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 770 */	NdrFcShort( 0x0 ),	/* 0 */
/* 772 */	NdrFcShort( 0x8 ),	/* 8 */
/* 774 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 776 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 778 */	NdrFcShort( 0x1 ),	/* 1 */
/* 780 */	NdrFcShort( 0x0 ),	/* 0 */
/* 782 */	NdrFcShort( 0x0 ),	/* 0 */
/* 784 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IUpdaterAppStateSystem0002 */


	/* Parameter __MIDL__IUpdaterAppStateUser0002 */


	/* Parameter __MIDL__IUpdaterAppState0002 */

/* 786 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 788 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 790 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 792 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 794 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 796 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_brandCode */


	/* Procedure get_brandCode */


	/* Procedure get_brandCode */

/* 798 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 800 */	NdrFcLong( 0x0 ),	/* 0 */
/* 804 */	NdrFcShort( 0xa ),	/* 10 */
/* 806 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 808 */	NdrFcShort( 0x0 ),	/* 0 */
/* 810 */	NdrFcShort( 0x8 ),	/* 8 */
/* 812 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 814 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 816 */	NdrFcShort( 0x1 ),	/* 1 */
/* 818 */	NdrFcShort( 0x0 ),	/* 0 */
/* 820 */	NdrFcShort( 0x0 ),	/* 0 */
/* 822 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IUpdaterAppStateSystem0003 */


	/* Parameter __MIDL__IUpdaterAppStateUser0003 */


	/* Parameter __MIDL__IUpdaterAppState0003 */

/* 824 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 826 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 828 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 830 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 832 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 834 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_brandPath */


	/* Procedure get_brandPath */


	/* Procedure get_brandPath */

/* 836 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 838 */	NdrFcLong( 0x0 ),	/* 0 */
/* 842 */	NdrFcShort( 0xb ),	/* 11 */
/* 844 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 846 */	NdrFcShort( 0x0 ),	/* 0 */
/* 848 */	NdrFcShort( 0x8 ),	/* 8 */
/* 850 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 852 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 854 */	NdrFcShort( 0x1 ),	/* 1 */
/* 856 */	NdrFcShort( 0x0 ),	/* 0 */
/* 858 */	NdrFcShort( 0x0 ),	/* 0 */
/* 860 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IUpdaterAppStateSystem0004 */


	/* Parameter __MIDL__IUpdaterAppStateUser0004 */


	/* Parameter __MIDL__IUpdaterAppState0004 */

/* 862 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 864 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 866 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 868 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 870 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 872 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Run */


	/* Procedure Run */


	/* Procedure Run */

/* 874 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 876 */	NdrFcLong( 0x0 ),	/* 0 */
/* 880 */	NdrFcShort( 0x3 ),	/* 3 */
/* 882 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 884 */	NdrFcShort( 0x0 ),	/* 0 */
/* 886 */	NdrFcShort( 0x8 ),	/* 8 */
/* 888 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 890 */	0xa,		/* 10 */
			0x85,		/* Ext Flags:  new corr desc, srv corr check, has big byval param */
/* 892 */	NdrFcShort( 0x0 ),	/* 0 */
/* 894 */	NdrFcShort( 0x1 ),	/* 1 */
/* 896 */	NdrFcShort( 0x0 ),	/* 0 */
/* 898 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_states */


	/* Parameter app_states */


	/* Parameter app_states */

/* 900 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 902 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 904 */	NdrFcShort( 0x458 ),	/* Type Offset=1112 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 906 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 908 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 910 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure GetVersion */


	/* Procedure GetVersion */


	/* Procedure GetVersion */

/* 912 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 914 */	NdrFcLong( 0x0 ),	/* 0 */
/* 918 */	NdrFcShort( 0x3 ),	/* 3 */
/* 920 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 922 */	NdrFcShort( 0x0 ),	/* 0 */
/* 924 */	NdrFcShort( 0x8 ),	/* 8 */
/* 926 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 928 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 930 */	NdrFcShort( 0x1 ),	/* 1 */
/* 932 */	NdrFcShort( 0x0 ),	/* 0 */
/* 934 */	NdrFcShort( 0x0 ),	/* 0 */
/* 936 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter version */


	/* Parameter version */


	/* Parameter version */

/* 938 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 940 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 942 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 944 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 946 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 948 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure FetchPolicies */

/* 950 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 952 */	NdrFcLong( 0x0 ),	/* 0 */
/* 956 */	NdrFcShort( 0x4 ),	/* 4 */
/* 958 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 960 */	NdrFcShort( 0x0 ),	/* 0 */
/* 962 */	NdrFcShort( 0x8 ),	/* 8 */
/* 964 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 966 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 968 */	NdrFcShort( 0x0 ),	/* 0 */
/* 970 */	NdrFcShort( 0x0 ),	/* 0 */
/* 972 */	NdrFcShort( 0x0 ),	/* 0 */
/* 974 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 976 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 978 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 980 */	NdrFcShort( 0x462 ),	/* Type Offset=1122 */

	/* Return value */

/* 982 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 984 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 986 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RegisterApp */

/* 988 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 990 */	NdrFcLong( 0x0 ),	/* 0 */
/* 994 */	NdrFcShort( 0x5 ),	/* 5 */
/* 996 */	NdrFcShort( 0x48 ),	/* X64 Stack size/offset = 72 */
/* 998 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1000 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1002 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x8,		/* 8 */
/* 1004 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1006 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1008 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1010 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1012 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1014 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1016 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1018 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter brand_code */

/* 1020 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1022 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1024 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter brand_path */

/* 1026 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1028 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1030 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter tag */

/* 1032 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1034 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1036 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter version */

/* 1038 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1040 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1042 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter existence_checker_path */

/* 1044 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1046 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1048 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter callback */

/* 1050 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1052 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1054 */	NdrFcShort( 0x462 ),	/* Type Offset=1122 */

	/* Return value */

/* 1056 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1058 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1060 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunPeriodicTasks */

/* 1062 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1064 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1068 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1070 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1072 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1074 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1076 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1078 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1080 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1082 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1084 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1086 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 1088 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1090 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1092 */	NdrFcShort( 0x462 ),	/* Type Offset=1122 */

	/* Return value */

/* 1094 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1096 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1098 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure CheckForUpdate */

/* 1100 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1102 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1106 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1108 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1110 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1112 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1114 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 1116 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1118 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1120 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1122 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1124 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1126 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1128 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1130 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter priority */

/* 1132 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1134 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1136 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 1138 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1140 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1142 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1144 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1146 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1148 */	NdrFcShort( 0x478 ),	/* Type Offset=1144 */

	/* Return value */

/* 1150 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1152 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1154 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Update */

/* 1156 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1158 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1162 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1164 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1166 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1168 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1170 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x6,		/* 6 */
/* 1172 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1174 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1176 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1178 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1180 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1182 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1184 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1186 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter install_data_index */

/* 1188 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1190 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1192 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter priority */

/* 1194 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1196 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1198 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 1200 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1202 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1204 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1206 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1208 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1210 */	NdrFcShort( 0x478 ),	/* Type Offset=1144 */

	/* Return value */

/* 1212 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1214 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1216 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure UpdateAll */

/* 1218 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1220 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1224 */	NdrFcShort( 0x9 ),	/* 9 */
/* 1226 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1228 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1230 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1232 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1234 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1236 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1238 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1240 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1242 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter observer */

/* 1244 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1246 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1248 */	NdrFcShort( 0x478 ),	/* Type Offset=1144 */

	/* Return value */

/* 1250 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1252 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1254 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Install */

/* 1256 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1258 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1262 */	NdrFcShort( 0xa ),	/* 10 */
/* 1264 */	NdrFcShort( 0x60 ),	/* X64 Stack size/offset = 96 */
/* 1266 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1268 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1270 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0xb,		/* 11 */
/* 1272 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1274 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1276 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1278 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1280 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1282 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1284 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1286 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter brand_code */

/* 1288 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1290 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1292 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter brand_path */

/* 1294 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1296 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1298 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter tag */

/* 1300 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1302 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1304 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter version */

/* 1306 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1308 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1310 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter existence_checker_path */

/* 1312 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1314 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1316 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter client_install_data */

/* 1318 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1320 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1322 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter install_data_index */

/* 1324 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1326 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1328 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter priority */

/* 1330 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1332 */	NdrFcShort( 0x48 ),	/* X64 Stack size/offset = 72 */
/* 1334 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1336 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1338 */	NdrFcShort( 0x50 ),	/* X64 Stack size/offset = 80 */
/* 1340 */	NdrFcShort( 0x478 ),	/* Type Offset=1144 */

	/* Return value */

/* 1342 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1344 */	NdrFcShort( 0x58 ),	/* X64 Stack size/offset = 88 */
/* 1346 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure CancelInstalls */


	/* Procedure CancelInstalls */


	/* Procedure CancelInstalls */

/* 1348 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1350 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1354 */	NdrFcShort( 0xb ),	/* 11 */
/* 1356 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1358 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1360 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1362 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1364 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1366 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1368 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1370 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1372 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */


	/* Parameter app_id */


	/* Parameter app_id */

/* 1374 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1376 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1378 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1380 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1382 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1384 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunInstaller */

/* 1386 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1388 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1392 */	NdrFcShort( 0xc ),	/* 12 */
/* 1394 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1396 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1398 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1400 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 1402 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1404 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1406 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1408 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1410 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1412 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1414 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1416 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter installer_path */

/* 1418 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1420 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1422 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter install_args */

/* 1424 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1426 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1428 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter install_data */

/* 1430 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1432 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1434 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter install_settings */

/* 1436 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1438 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1440 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter observer */

/* 1442 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1444 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1446 */	NdrFcShort( 0x478 ),	/* Type Offset=1144 */

	/* Return value */

/* 1448 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1450 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1452 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure GetAppStates */

/* 1454 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1456 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1460 */	NdrFcShort( 0xd ),	/* 13 */
/* 1462 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1464 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1466 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1468 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1470 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1472 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1474 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1476 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1478 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 1480 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1482 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1484 */	NdrFcShort( 0x48a ),	/* Type Offset=1162 */

	/* Return value */

/* 1486 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1488 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1490 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure FetchPolicies */

/* 1492 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1494 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1498 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1500 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1502 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1504 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1506 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1508 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1510 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1512 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1514 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1516 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 1518 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1520 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1522 */	NdrFcShort( 0x49c ),	/* Type Offset=1180 */

	/* Return value */

/* 1524 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1526 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1528 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RegisterApp */

/* 1530 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1532 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1536 */	NdrFcShort( 0x5 ),	/* 5 */
/* 1538 */	NdrFcShort( 0x48 ),	/* X64 Stack size/offset = 72 */
/* 1540 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1542 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1544 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x8,		/* 8 */
/* 1546 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1548 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1550 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1552 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1554 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1556 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1558 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1560 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter brand_code */

/* 1562 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1564 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1566 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter brand_path */

/* 1568 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1570 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1572 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter tag */

/* 1574 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1576 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1578 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter version */

/* 1580 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1582 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1584 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter existence_checker_path */

/* 1586 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1588 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1590 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter callback */

/* 1592 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1594 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1596 */	NdrFcShort( 0x49c ),	/* Type Offset=1180 */

	/* Return value */

/* 1598 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1600 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1602 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunPeriodicTasks */

/* 1604 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1606 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1610 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1612 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1614 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1616 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1618 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1620 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1622 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1624 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1626 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1628 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 1630 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1632 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1634 */	NdrFcShort( 0x49c ),	/* Type Offset=1180 */

	/* Return value */

/* 1636 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1638 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1640 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure CheckForUpdate */

/* 1642 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1644 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1648 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1650 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1652 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1654 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1656 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 1658 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1660 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1662 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1664 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1666 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1668 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1670 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1672 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter priority */

/* 1674 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1676 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1678 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 1680 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1682 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1684 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1686 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1688 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1690 */	NdrFcShort( 0x4ae ),	/* Type Offset=1198 */

	/* Return value */

/* 1692 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1694 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1696 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Update */

/* 1698 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1700 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1704 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1706 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1708 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1710 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1712 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x6,		/* 6 */
/* 1714 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1716 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1718 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1720 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1722 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1724 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1726 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1728 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter install_data_index */

/* 1730 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1732 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1734 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter priority */

/* 1736 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1738 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1740 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 1742 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1744 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1746 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1748 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1750 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1752 */	NdrFcShort( 0x4ae ),	/* Type Offset=1198 */

	/* Return value */

/* 1754 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1756 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1758 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure UpdateAll */

/* 1760 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1762 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1766 */	NdrFcShort( 0x9 ),	/* 9 */
/* 1768 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1770 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1772 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1774 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1776 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1778 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1780 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1782 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1784 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter observer */

/* 1786 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1788 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1790 */	NdrFcShort( 0x4ae ),	/* Type Offset=1198 */

	/* Return value */

/* 1792 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1794 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1796 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Install */

/* 1798 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1800 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1804 */	NdrFcShort( 0xa ),	/* 10 */
/* 1806 */	NdrFcShort( 0x60 ),	/* X64 Stack size/offset = 96 */
/* 1808 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1810 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1812 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0xb,		/* 11 */
/* 1814 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1816 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1818 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1820 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1822 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1824 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1826 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1828 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter brand_code */

/* 1830 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1832 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1834 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter brand_path */

/* 1836 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1838 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1840 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter tag */

/* 1842 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1844 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1846 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter version */

/* 1848 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1850 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1852 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter existence_checker_path */

/* 1854 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1856 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1858 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter client_install_data */

/* 1860 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1862 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1864 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter install_data_index */

/* 1866 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1868 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1870 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter priority */

/* 1872 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1874 */	NdrFcShort( 0x48 ),	/* X64 Stack size/offset = 72 */
/* 1876 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1878 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1880 */	NdrFcShort( 0x50 ),	/* X64 Stack size/offset = 80 */
/* 1882 */	NdrFcShort( 0x4ae ),	/* Type Offset=1198 */

	/* Return value */

/* 1884 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1886 */	NdrFcShort( 0x58 ),	/* X64 Stack size/offset = 88 */
/* 1888 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunInstaller */

/* 1890 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1892 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1896 */	NdrFcShort( 0xc ),	/* 12 */
/* 1898 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1900 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1902 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1904 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 1906 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1908 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1910 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1912 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1914 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1916 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1918 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1920 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter installer_path */

/* 1922 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1924 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1926 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter install_args */

/* 1928 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1930 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1932 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter install_data */

/* 1934 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1936 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1938 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter install_settings */

/* 1940 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1942 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1944 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter observer */

/* 1946 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1948 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1950 */	NdrFcShort( 0x4ae ),	/* Type Offset=1198 */

	/* Return value */

/* 1952 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1954 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1956 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure GetAppStates */

/* 1958 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1960 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1964 */	NdrFcShort( 0xd ),	/* 13 */
/* 1966 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1968 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1970 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1972 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1974 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1976 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1978 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1980 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1982 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 1984 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1986 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1988 */	NdrFcShort( 0x4c0 ),	/* Type Offset=1216 */

	/* Return value */

/* 1990 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1992 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1994 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure FetchPolicies */

/* 1996 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1998 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2002 */	NdrFcShort( 0x4 ),	/* 4 */
/* 2004 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2006 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2008 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2010 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 2012 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2014 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2016 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2018 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2020 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 2022 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2024 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2026 */	NdrFcShort( 0x4d2 ),	/* Type Offset=1234 */

	/* Return value */

/* 2028 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2030 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2032 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RegisterApp */

/* 2034 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2036 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2040 */	NdrFcShort( 0x5 ),	/* 5 */
/* 2042 */	NdrFcShort( 0x48 ),	/* X64 Stack size/offset = 72 */
/* 2044 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2046 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2048 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x8,		/* 8 */
/* 2050 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2052 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2054 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2056 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2058 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 2060 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2062 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2064 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter brand_code */

/* 2066 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2068 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2070 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter brand_path */

/* 2072 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2074 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2076 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter tag */

/* 2078 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2080 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2082 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter version */

/* 2084 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2086 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2088 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter existence_checker_path */

/* 2090 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2092 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 2094 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter callback */

/* 2096 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2098 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 2100 */	NdrFcShort( 0x4d2 ),	/* Type Offset=1234 */

	/* Return value */

/* 2102 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2104 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 2106 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunPeriodicTasks */

/* 2108 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2110 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2114 */	NdrFcShort( 0x6 ),	/* 6 */
/* 2116 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2118 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2120 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2122 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 2124 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2126 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2128 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2130 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2132 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 2134 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2136 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2138 */	NdrFcShort( 0x4d2 ),	/* Type Offset=1234 */

	/* Return value */

/* 2140 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2142 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2144 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure CheckForUpdate */

/* 2146 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2148 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2152 */	NdrFcShort( 0x7 ),	/* 7 */
/* 2154 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 2156 */	NdrFcShort( 0x10 ),	/* 16 */
/* 2158 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2160 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 2162 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2164 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2166 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2168 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2170 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 2172 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2174 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2176 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter priority */

/* 2178 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2180 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2182 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 2184 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2186 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2188 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 2190 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2192 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2194 */	NdrFcShort( 0x4e4 ),	/* Type Offset=1252 */

	/* Return value */

/* 2196 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2198 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2200 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Update */

/* 2202 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2204 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2208 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2210 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 2212 */	NdrFcShort( 0x10 ),	/* 16 */
/* 2214 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2216 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x6,		/* 6 */
/* 2218 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2220 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2222 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2224 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2226 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 2228 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2230 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2232 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter install_data_index */

/* 2234 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2236 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2238 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter priority */

/* 2240 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2242 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2244 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 2246 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2248 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2250 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 2252 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2254 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2256 */	NdrFcShort( 0x4e4 ),	/* Type Offset=1252 */

	/* Return value */

/* 2258 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2260 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 2262 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure UpdateAll */

/* 2264 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2266 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2270 */	NdrFcShort( 0x9 ),	/* 9 */
/* 2272 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2274 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2276 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2278 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 2280 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2282 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2284 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2286 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2288 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter observer */

/* 2290 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2292 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2294 */	NdrFcShort( 0x4e4 ),	/* Type Offset=1252 */

	/* Return value */

/* 2296 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2298 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2300 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Install */

/* 2302 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2304 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2308 */	NdrFcShort( 0xa ),	/* 10 */
/* 2310 */	NdrFcShort( 0x60 ),	/* X64 Stack size/offset = 96 */
/* 2312 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2314 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2316 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0xb,		/* 11 */
/* 2318 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2320 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2322 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2324 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2326 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 2328 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2330 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2332 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter brand_code */

/* 2334 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2336 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2338 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter brand_path */

/* 2340 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2342 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2344 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter tag */

/* 2346 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2348 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2350 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter version */

/* 2352 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2354 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2356 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter existence_checker_path */

/* 2358 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2360 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 2362 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter client_install_data */

/* 2364 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2366 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 2368 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter install_data_index */

/* 2370 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2372 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 2374 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter priority */

/* 2376 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2378 */	NdrFcShort( 0x48 ),	/* X64 Stack size/offset = 72 */
/* 2380 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 2382 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2384 */	NdrFcShort( 0x50 ),	/* X64 Stack size/offset = 80 */
/* 2386 */	NdrFcShort( 0x4e4 ),	/* Type Offset=1252 */

	/* Return value */

/* 2388 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2390 */	NdrFcShort( 0x58 ),	/* X64 Stack size/offset = 88 */
/* 2392 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunInstaller */

/* 2394 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2396 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2400 */	NdrFcShort( 0xc ),	/* 12 */
/* 2402 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 2404 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2406 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2408 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 2410 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2412 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2414 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2416 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2418 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 2420 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2422 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2424 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter installer_path */

/* 2426 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2428 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2430 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter install_args */

/* 2432 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2434 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2436 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter install_data */

/* 2438 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2440 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2442 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter install_settings */

/* 2444 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2446 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2448 */	NdrFcShort( 0x476 ),	/* Type Offset=1142 */

	/* Parameter observer */

/* 2450 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2452 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 2454 */	NdrFcShort( 0x4e4 ),	/* Type Offset=1252 */

	/* Return value */

/* 2456 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2458 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 2460 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure GetAppStates */

/* 2462 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2464 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2468 */	NdrFcShort( 0xd ),	/* 13 */
/* 2470 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2472 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2474 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2476 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 2478 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2480 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2482 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2484 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2486 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter callback */

/* 2488 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2490 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2492 */	NdrFcShort( 0x4f6 ),	/* Type Offset=1270 */

	/* Return value */

/* 2494 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2496 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2498 */	0x8,		/* FC_LONG */
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
			0x11, 0x0,	/* FC_RP */
/* 160 */	NdrFcShort( 0x3b8 ),	/* Offset= 952 (1112) */
/* 162 */	
			0x12, 0x0,	/* FC_UP */
/* 164 */	NdrFcShort( 0x3a0 ),	/* Offset= 928 (1092) */
/* 166 */	
			0x2b,		/* FC_NON_ENCAPSULATED_UNION */
			0x9,		/* FC_ULONG */
/* 168 */	0x7,		/* Corr desc: FC_USHORT */
			0x0,		/*  */
/* 170 */	NdrFcShort( 0xfff8 ),	/* -8 */
/* 172 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 174 */	NdrFcShort( 0x2 ),	/* Offset= 2 (176) */
/* 176 */	NdrFcShort( 0x10 ),	/* 16 */
/* 178 */	NdrFcShort( 0x2f ),	/* 47 */
/* 180 */	NdrFcLong( 0x14 ),	/* 20 */
/* 184 */	NdrFcShort( 0x800b ),	/* Simple arm type: FC_HYPER */
/* 186 */	NdrFcLong( 0x3 ),	/* 3 */
/* 190 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 192 */	NdrFcLong( 0x11 ),	/* 17 */
/* 196 */	NdrFcShort( 0x8001 ),	/* Simple arm type: FC_BYTE */
/* 198 */	NdrFcLong( 0x2 ),	/* 2 */
/* 202 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 204 */	NdrFcLong( 0x4 ),	/* 4 */
/* 208 */	NdrFcShort( 0x800a ),	/* Simple arm type: FC_FLOAT */
/* 210 */	NdrFcLong( 0x5 ),	/* 5 */
/* 214 */	NdrFcShort( 0x800c ),	/* Simple arm type: FC_DOUBLE */
/* 216 */	NdrFcLong( 0xb ),	/* 11 */
/* 220 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 222 */	NdrFcLong( 0xa ),	/* 10 */
/* 226 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 228 */	NdrFcLong( 0x6 ),	/* 6 */
/* 232 */	NdrFcShort( 0xe8 ),	/* Offset= 232 (464) */
/* 234 */	NdrFcLong( 0x7 ),	/* 7 */
/* 238 */	NdrFcShort( 0x800c ),	/* Simple arm type: FC_DOUBLE */
/* 240 */	NdrFcLong( 0x8 ),	/* 8 */
/* 244 */	NdrFcShort( 0xe2 ),	/* Offset= 226 (470) */
/* 246 */	NdrFcLong( 0xd ),	/* 13 */
/* 250 */	NdrFcShort( 0xe0 ),	/* Offset= 224 (474) */
/* 252 */	NdrFcLong( 0x9 ),	/* 9 */
/* 256 */	NdrFcShort( 0xec ),	/* Offset= 236 (492) */
/* 258 */	NdrFcLong( 0x2000 ),	/* 8192 */
/* 262 */	NdrFcShort( 0xf8 ),	/* Offset= 248 (510) */
/* 264 */	NdrFcLong( 0x24 ),	/* 36 */
/* 268 */	NdrFcShort( 0x2ee ),	/* Offset= 750 (1018) */
/* 270 */	NdrFcLong( 0x4024 ),	/* 16420 */
/* 274 */	NdrFcShort( 0x2e8 ),	/* Offset= 744 (1018) */
/* 276 */	NdrFcLong( 0x4011 ),	/* 16401 */
/* 280 */	NdrFcShort( 0x2e6 ),	/* Offset= 742 (1022) */
/* 282 */	NdrFcLong( 0x4002 ),	/* 16386 */
/* 286 */	NdrFcShort( 0x2e4 ),	/* Offset= 740 (1026) */
/* 288 */	NdrFcLong( 0x4003 ),	/* 16387 */
/* 292 */	NdrFcShort( 0x2e2 ),	/* Offset= 738 (1030) */
/* 294 */	NdrFcLong( 0x4014 ),	/* 16404 */
/* 298 */	NdrFcShort( 0x2e0 ),	/* Offset= 736 (1034) */
/* 300 */	NdrFcLong( 0x4004 ),	/* 16388 */
/* 304 */	NdrFcShort( 0x2de ),	/* Offset= 734 (1038) */
/* 306 */	NdrFcLong( 0x4005 ),	/* 16389 */
/* 310 */	NdrFcShort( 0x2dc ),	/* Offset= 732 (1042) */
/* 312 */	NdrFcLong( 0x400b ),	/* 16395 */
/* 316 */	NdrFcShort( 0x2c6 ),	/* Offset= 710 (1026) */
/* 318 */	NdrFcLong( 0x400a ),	/* 16394 */
/* 322 */	NdrFcShort( 0x2c4 ),	/* Offset= 708 (1030) */
/* 324 */	NdrFcLong( 0x4006 ),	/* 16390 */
/* 328 */	NdrFcShort( 0x2ce ),	/* Offset= 718 (1046) */
/* 330 */	NdrFcLong( 0x4007 ),	/* 16391 */
/* 334 */	NdrFcShort( 0x2c4 ),	/* Offset= 708 (1042) */
/* 336 */	NdrFcLong( 0x4008 ),	/* 16392 */
/* 340 */	NdrFcShort( 0x2c6 ),	/* Offset= 710 (1050) */
/* 342 */	NdrFcLong( 0x400d ),	/* 16397 */
/* 346 */	NdrFcShort( 0x2c4 ),	/* Offset= 708 (1054) */
/* 348 */	NdrFcLong( 0x4009 ),	/* 16393 */
/* 352 */	NdrFcShort( 0x2c2 ),	/* Offset= 706 (1058) */
/* 354 */	NdrFcLong( 0x6000 ),	/* 24576 */
/* 358 */	NdrFcShort( 0x2c0 ),	/* Offset= 704 (1062) */
/* 360 */	NdrFcLong( 0x400c ),	/* 16396 */
/* 364 */	NdrFcShort( 0x2be ),	/* Offset= 702 (1066) */
/* 366 */	NdrFcLong( 0x10 ),	/* 16 */
/* 370 */	NdrFcShort( 0x8002 ),	/* Simple arm type: FC_CHAR */
/* 372 */	NdrFcLong( 0x12 ),	/* 18 */
/* 376 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 378 */	NdrFcLong( 0x13 ),	/* 19 */
/* 382 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 384 */	NdrFcLong( 0x15 ),	/* 21 */
/* 388 */	NdrFcShort( 0x800b ),	/* Simple arm type: FC_HYPER */
/* 390 */	NdrFcLong( 0x16 ),	/* 22 */
/* 394 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 396 */	NdrFcLong( 0x17 ),	/* 23 */
/* 400 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 402 */	NdrFcLong( 0xe ),	/* 14 */
/* 406 */	NdrFcShort( 0x29c ),	/* Offset= 668 (1074) */
/* 408 */	NdrFcLong( 0x400e ),	/* 16398 */
/* 412 */	NdrFcShort( 0x2a0 ),	/* Offset= 672 (1084) */
/* 414 */	NdrFcLong( 0x4010 ),	/* 16400 */
/* 418 */	NdrFcShort( 0x29e ),	/* Offset= 670 (1088) */
/* 420 */	NdrFcLong( 0x4012 ),	/* 16402 */
/* 424 */	NdrFcShort( 0x25a ),	/* Offset= 602 (1026) */
/* 426 */	NdrFcLong( 0x4013 ),	/* 16403 */
/* 430 */	NdrFcShort( 0x258 ),	/* Offset= 600 (1030) */
/* 432 */	NdrFcLong( 0x4015 ),	/* 16405 */
/* 436 */	NdrFcShort( 0x256 ),	/* Offset= 598 (1034) */
/* 438 */	NdrFcLong( 0x4016 ),	/* 16406 */
/* 442 */	NdrFcShort( 0x24c ),	/* Offset= 588 (1030) */
/* 444 */	NdrFcLong( 0x4017 ),	/* 16407 */
/* 448 */	NdrFcShort( 0x246 ),	/* Offset= 582 (1030) */
/* 450 */	NdrFcLong( 0x0 ),	/* 0 */
/* 454 */	NdrFcShort( 0x0 ),	/* Offset= 0 (454) */
/* 456 */	NdrFcLong( 0x1 ),	/* 1 */
/* 460 */	NdrFcShort( 0x0 ),	/* Offset= 0 (460) */
/* 462 */	NdrFcShort( 0xffff ),	/* Offset= -1 (461) */
/* 464 */	
			0x15,		/* FC_STRUCT */
			0x7,		/* 7 */
/* 466 */	NdrFcShort( 0x8 ),	/* 8 */
/* 468 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 470 */	
			0x12, 0x0,	/* FC_UP */
/* 472 */	NdrFcShort( 0xfe42 ),	/* Offset= -446 (26) */
/* 474 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 476 */	NdrFcLong( 0x0 ),	/* 0 */
/* 480 */	NdrFcShort( 0x0 ),	/* 0 */
/* 482 */	NdrFcShort( 0x0 ),	/* 0 */
/* 484 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 486 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 488 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 490 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 492 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 494 */	NdrFcLong( 0x20400 ),	/* 132096 */
/* 498 */	NdrFcShort( 0x0 ),	/* 0 */
/* 500 */	NdrFcShort( 0x0 ),	/* 0 */
/* 502 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 504 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 506 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 508 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 510 */	
			0x12, 0x10,	/* FC_UP [pointer_deref] */
/* 512 */	NdrFcShort( 0x2 ),	/* Offset= 2 (514) */
/* 514 */	
			0x12, 0x0,	/* FC_UP */
/* 516 */	NdrFcShort( 0x1e4 ),	/* Offset= 484 (1000) */
/* 518 */	
			0x2a,		/* FC_ENCAPSULATED_UNION */
			0x89,		/* 137 */
/* 520 */	NdrFcShort( 0x20 ),	/* 32 */
/* 522 */	NdrFcShort( 0xa ),	/* 10 */
/* 524 */	NdrFcLong( 0x8 ),	/* 8 */
/* 528 */	NdrFcShort( 0x50 ),	/* Offset= 80 (608) */
/* 530 */	NdrFcLong( 0xd ),	/* 13 */
/* 534 */	NdrFcShort( 0x70 ),	/* Offset= 112 (646) */
/* 536 */	NdrFcLong( 0x9 ),	/* 9 */
/* 540 */	NdrFcShort( 0x90 ),	/* Offset= 144 (684) */
/* 542 */	NdrFcLong( 0xc ),	/* 12 */
/* 546 */	NdrFcShort( 0xb0 ),	/* Offset= 176 (722) */
/* 548 */	NdrFcLong( 0x24 ),	/* 36 */
/* 552 */	NdrFcShort( 0x102 ),	/* Offset= 258 (810) */
/* 554 */	NdrFcLong( 0x800d ),	/* 32781 */
/* 558 */	NdrFcShort( 0x11e ),	/* Offset= 286 (844) */
/* 560 */	NdrFcLong( 0x10 ),	/* 16 */
/* 564 */	NdrFcShort( 0x138 ),	/* Offset= 312 (876) */
/* 566 */	NdrFcLong( 0x2 ),	/* 2 */
/* 570 */	NdrFcShort( 0x14e ),	/* Offset= 334 (904) */
/* 572 */	NdrFcLong( 0x3 ),	/* 3 */
/* 576 */	NdrFcShort( 0x164 ),	/* Offset= 356 (932) */
/* 578 */	NdrFcLong( 0x14 ),	/* 20 */
/* 582 */	NdrFcShort( 0x17a ),	/* Offset= 378 (960) */
/* 584 */	NdrFcShort( 0xffff ),	/* Offset= -1 (583) */
/* 586 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 588 */	NdrFcShort( 0x0 ),	/* 0 */
/* 590 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 592 */	NdrFcShort( 0x0 ),	/* 0 */
/* 594 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 596 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 600 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 602 */	
			0x12, 0x0,	/* FC_UP */
/* 604 */	NdrFcShort( 0xfdbe ),	/* Offset= -578 (26) */
/* 606 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 608 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 610 */	NdrFcShort( 0x10 ),	/* 16 */
/* 612 */	NdrFcShort( 0x0 ),	/* 0 */
/* 614 */	NdrFcShort( 0x6 ),	/* Offset= 6 (620) */
/* 616 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 618 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 620 */	
			0x11, 0x0,	/* FC_RP */
/* 622 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (586) */
/* 624 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 626 */	NdrFcShort( 0x0 ),	/* 0 */
/* 628 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 630 */	NdrFcShort( 0x0 ),	/* 0 */
/* 632 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 634 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 638 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 640 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 642 */	NdrFcShort( 0xff58 ),	/* Offset= -168 (474) */
/* 644 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 646 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 648 */	NdrFcShort( 0x10 ),	/* 16 */
/* 650 */	NdrFcShort( 0x0 ),	/* 0 */
/* 652 */	NdrFcShort( 0x6 ),	/* Offset= 6 (658) */
/* 654 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 656 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 658 */	
			0x11, 0x0,	/* FC_RP */
/* 660 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (624) */
/* 662 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 664 */	NdrFcShort( 0x0 ),	/* 0 */
/* 666 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 668 */	NdrFcShort( 0x0 ),	/* 0 */
/* 670 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 672 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 676 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 678 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 680 */	NdrFcShort( 0xff44 ),	/* Offset= -188 (492) */
/* 682 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 684 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 686 */	NdrFcShort( 0x10 ),	/* 16 */
/* 688 */	NdrFcShort( 0x0 ),	/* 0 */
/* 690 */	NdrFcShort( 0x6 ),	/* Offset= 6 (696) */
/* 692 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 694 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 696 */	
			0x11, 0x0,	/* FC_RP */
/* 698 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (662) */
/* 700 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 702 */	NdrFcShort( 0x0 ),	/* 0 */
/* 704 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 706 */	NdrFcShort( 0x0 ),	/* 0 */
/* 708 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 710 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 714 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 716 */	
			0x12, 0x0,	/* FC_UP */
/* 718 */	NdrFcShort( 0x176 ),	/* Offset= 374 (1092) */
/* 720 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 722 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 724 */	NdrFcShort( 0x10 ),	/* 16 */
/* 726 */	NdrFcShort( 0x0 ),	/* 0 */
/* 728 */	NdrFcShort( 0x6 ),	/* Offset= 6 (734) */
/* 730 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 732 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 734 */	
			0x11, 0x0,	/* FC_RP */
/* 736 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (700) */
/* 738 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 740 */	NdrFcLong( 0x2f ),	/* 47 */
/* 744 */	NdrFcShort( 0x0 ),	/* 0 */
/* 746 */	NdrFcShort( 0x0 ),	/* 0 */
/* 748 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 750 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 752 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 754 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 756 */	
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 758 */	NdrFcShort( 0x1 ),	/* 1 */
/* 760 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 762 */	NdrFcShort( 0x4 ),	/* 4 */
/* 764 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 766 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 768 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 770 */	NdrFcShort( 0x18 ),	/* 24 */
/* 772 */	NdrFcShort( 0x0 ),	/* 0 */
/* 774 */	NdrFcShort( 0xa ),	/* Offset= 10 (784) */
/* 776 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 778 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 780 */	NdrFcShort( 0xffd6 ),	/* Offset= -42 (738) */
/* 782 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 784 */	
			0x12, 0x0,	/* FC_UP */
/* 786 */	NdrFcShort( 0xffe2 ),	/* Offset= -30 (756) */
/* 788 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 790 */	NdrFcShort( 0x0 ),	/* 0 */
/* 792 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 794 */	NdrFcShort( 0x0 ),	/* 0 */
/* 796 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 798 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 802 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 804 */	
			0x12, 0x0,	/* FC_UP */
/* 806 */	NdrFcShort( 0xffda ),	/* Offset= -38 (768) */
/* 808 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 810 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 812 */	NdrFcShort( 0x10 ),	/* 16 */
/* 814 */	NdrFcShort( 0x0 ),	/* 0 */
/* 816 */	NdrFcShort( 0x6 ),	/* Offset= 6 (822) */
/* 818 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 820 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 822 */	
			0x11, 0x0,	/* FC_RP */
/* 824 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (788) */
/* 826 */	
			0x1d,		/* FC_SMFARRAY */
			0x0,		/* 0 */
/* 828 */	NdrFcShort( 0x8 ),	/* 8 */
/* 830 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 832 */	
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 834 */	NdrFcShort( 0x10 ),	/* 16 */
/* 836 */	0x8,		/* FC_LONG */
			0x6,		/* FC_SHORT */
/* 838 */	0x6,		/* FC_SHORT */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 840 */	0x0,		/* 0 */
			NdrFcShort( 0xfff1 ),	/* Offset= -15 (826) */
			0x5b,		/* FC_END */
/* 844 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 846 */	NdrFcShort( 0x20 ),	/* 32 */
/* 848 */	NdrFcShort( 0x0 ),	/* 0 */
/* 850 */	NdrFcShort( 0xa ),	/* Offset= 10 (860) */
/* 852 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 854 */	0x36,		/* FC_POINTER */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 856 */	0x0,		/* 0 */
			NdrFcShort( 0xffe7 ),	/* Offset= -25 (832) */
			0x5b,		/* FC_END */
/* 860 */	
			0x11, 0x0,	/* FC_RP */
/* 862 */	NdrFcShort( 0xff12 ),	/* Offset= -238 (624) */
/* 864 */	
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 866 */	NdrFcShort( 0x1 ),	/* 1 */
/* 868 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 870 */	NdrFcShort( 0x0 ),	/* 0 */
/* 872 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 874 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 876 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 878 */	NdrFcShort( 0x10 ),	/* 16 */
/* 880 */	NdrFcShort( 0x0 ),	/* 0 */
/* 882 */	NdrFcShort( 0x6 ),	/* Offset= 6 (888) */
/* 884 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 886 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 888 */	
			0x12, 0x0,	/* FC_UP */
/* 890 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (864) */
/* 892 */	
			0x1b,		/* FC_CARRAY */
			0x1,		/* 1 */
/* 894 */	NdrFcShort( 0x2 ),	/* 2 */
/* 896 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 898 */	NdrFcShort( 0x0 ),	/* 0 */
/* 900 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 902 */	0x6,		/* FC_SHORT */
			0x5b,		/* FC_END */
/* 904 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 906 */	NdrFcShort( 0x10 ),	/* 16 */
/* 908 */	NdrFcShort( 0x0 ),	/* 0 */
/* 910 */	NdrFcShort( 0x6 ),	/* Offset= 6 (916) */
/* 912 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 914 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 916 */	
			0x12, 0x0,	/* FC_UP */
/* 918 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (892) */
/* 920 */	
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 922 */	NdrFcShort( 0x4 ),	/* 4 */
/* 924 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 926 */	NdrFcShort( 0x0 ),	/* 0 */
/* 928 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 930 */	0x8,		/* FC_LONG */
			0x5b,		/* FC_END */
/* 932 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 934 */	NdrFcShort( 0x10 ),	/* 16 */
/* 936 */	NdrFcShort( 0x0 ),	/* 0 */
/* 938 */	NdrFcShort( 0x6 ),	/* Offset= 6 (944) */
/* 940 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 942 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 944 */	
			0x12, 0x0,	/* FC_UP */
/* 946 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (920) */
/* 948 */	
			0x1b,		/* FC_CARRAY */
			0x7,		/* 7 */
/* 950 */	NdrFcShort( 0x8 ),	/* 8 */
/* 952 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 954 */	NdrFcShort( 0x0 ),	/* 0 */
/* 956 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 958 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 960 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 962 */	NdrFcShort( 0x10 ),	/* 16 */
/* 964 */	NdrFcShort( 0x0 ),	/* 0 */
/* 966 */	NdrFcShort( 0x6 ),	/* Offset= 6 (972) */
/* 968 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 970 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 972 */	
			0x12, 0x0,	/* FC_UP */
/* 974 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (948) */
/* 976 */	
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 978 */	NdrFcShort( 0x8 ),	/* 8 */
/* 980 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 982 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 984 */	
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 986 */	NdrFcShort( 0x8 ),	/* 8 */
/* 988 */	0x7,		/* Corr desc: FC_USHORT */
			0x0,		/*  */
/* 990 */	NdrFcShort( 0xffc8 ),	/* -56 */
/* 992 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 994 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 996 */	NdrFcShort( 0xffec ),	/* Offset= -20 (976) */
/* 998 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1000 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1002 */	NdrFcShort( 0x38 ),	/* 56 */
/* 1004 */	NdrFcShort( 0xffec ),	/* Offset= -20 (984) */
/* 1006 */	NdrFcShort( 0x0 ),	/* Offset= 0 (1006) */
/* 1008 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 1010 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 1012 */	0x40,		/* FC_STRUCTPAD4 */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 1014 */	0x0,		/* 0 */
			NdrFcShort( 0xfe0f ),	/* Offset= -497 (518) */
			0x5b,		/* FC_END */
/* 1018 */	
			0x12, 0x0,	/* FC_UP */
/* 1020 */	NdrFcShort( 0xff04 ),	/* Offset= -252 (768) */
/* 1022 */	
			0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 1024 */	0x1,		/* FC_BYTE */
			0x5c,		/* FC_PAD */
/* 1026 */	
			0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 1028 */	0x6,		/* FC_SHORT */
			0x5c,		/* FC_PAD */
/* 1030 */	
			0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 1032 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */
/* 1034 */	
			0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 1036 */	0xb,		/* FC_HYPER */
			0x5c,		/* FC_PAD */
/* 1038 */	
			0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 1040 */	0xa,		/* FC_FLOAT */
			0x5c,		/* FC_PAD */
/* 1042 */	
			0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 1044 */	0xc,		/* FC_DOUBLE */
			0x5c,		/* FC_PAD */
/* 1046 */	
			0x12, 0x0,	/* FC_UP */
/* 1048 */	NdrFcShort( 0xfdb8 ),	/* Offset= -584 (464) */
/* 1050 */	
			0x12, 0x10,	/* FC_UP [pointer_deref] */
/* 1052 */	NdrFcShort( 0xfdba ),	/* Offset= -582 (470) */
/* 1054 */	
			0x12, 0x10,	/* FC_UP [pointer_deref] */
/* 1056 */	NdrFcShort( 0xfdba ),	/* Offset= -582 (474) */
/* 1058 */	
			0x12, 0x10,	/* FC_UP [pointer_deref] */
/* 1060 */	NdrFcShort( 0xfdc8 ),	/* Offset= -568 (492) */
/* 1062 */	
			0x12, 0x10,	/* FC_UP [pointer_deref] */
/* 1064 */	NdrFcShort( 0xfdd6 ),	/* Offset= -554 (510) */
/* 1066 */	
			0x12, 0x10,	/* FC_UP [pointer_deref] */
/* 1068 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1070) */
/* 1070 */	
			0x12, 0x0,	/* FC_UP */
/* 1072 */	NdrFcShort( 0x14 ),	/* Offset= 20 (1092) */
/* 1074 */	
			0x15,		/* FC_STRUCT */
			0x7,		/* 7 */
/* 1076 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1078 */	0x6,		/* FC_SHORT */
			0x1,		/* FC_BYTE */
/* 1080 */	0x1,		/* FC_BYTE */
			0x8,		/* FC_LONG */
/* 1082 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 1084 */	
			0x12, 0x0,	/* FC_UP */
/* 1086 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (1074) */
/* 1088 */	
			0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 1090 */	0x2,		/* FC_CHAR */
			0x5c,		/* FC_PAD */
/* 1092 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x7,		/* 7 */
/* 1094 */	NdrFcShort( 0x20 ),	/* 32 */
/* 1096 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1098 */	NdrFcShort( 0x0 ),	/* Offset= 0 (1098) */
/* 1100 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 1102 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 1104 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 1106 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1108 */	NdrFcShort( 0xfc52 ),	/* Offset= -942 (166) */
/* 1110 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1112 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 1114 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1116 */	NdrFcShort( 0x18 ),	/* 24 */
/* 1118 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1120 */	NdrFcShort( 0xfc42 ),	/* Offset= -958 (162) */
/* 1122 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1124 */	NdrFcLong( 0x8bab6f84 ),	/* -1951699068 */
/* 1128 */	NdrFcShort( 0xad67 ),	/* -21145 */
/* 1130 */	NdrFcShort( 0x4819 ),	/* 18457 */
/* 1132 */	0xb8,		/* 184 */
			0x46,		/* 70 */
/* 1134 */	0xcc,		/* 204 */
			0x89,		/* 137 */
/* 1136 */	0x8,		/* 8 */
			0x80,		/* 128 */
/* 1138 */	0xfd,		/* 253 */
			0x3b,		/* 59 */
/* 1140 */	
			0x11, 0x8,	/* FC_RP [simple_pointer] */
/* 1142 */	
			0x25,		/* FC_C_WSTRING */
			0x5c,		/* FC_PAD */
/* 1144 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1146 */	NdrFcLong( 0x7b416cfd ),	/* 2067885309 */
/* 1150 */	NdrFcShort( 0x4216 ),	/* 16918 */
/* 1152 */	NdrFcShort( 0x4fd6 ),	/* 20438 */
/* 1154 */	0xbd,		/* 189 */
			0x83,		/* 131 */
/* 1156 */	0x7c,		/* 124 */
			0x58,		/* 88 */
/* 1158 */	0x60,		/* 96 */
			0x54,		/* 84 */
/* 1160 */	0x67,		/* 103 */
			0x6e,		/* 110 */
/* 1162 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1164 */	NdrFcLong( 0xefe903c0 ),	/* -269941824 */
/* 1168 */	NdrFcShort( 0xe820 ),	/* -6112 */
/* 1170 */	NdrFcShort( 0x4136 ),	/* 16694 */
/* 1172 */	0x9f,		/* 159 */
			0xae,		/* 174 */
/* 1174 */	0xfd,		/* 253 */
			0xcd,		/* 205 */
/* 1176 */	0x7f,		/* 127 */
			0x25,		/* 37 */
/* 1178 */	0x63,		/* 99 */
			0x2,		/* 2 */
/* 1180 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1182 */	NdrFcLong( 0x34adc89d ),	/* 883804317 */
/* 1186 */	NdrFcShort( 0x552b ),	/* 21803 */
/* 1188 */	NdrFcShort( 0x4102 ),	/* 16642 */
/* 1190 */	0x8a,		/* 138 */
			0xe5,		/* 229 */
/* 1192 */	0xd6,		/* 214 */
			0x13,		/* 19 */
/* 1194 */	0xa6,		/* 166 */
			0x91,		/* 145 */
/* 1196 */	0x33,		/* 51 */
			0x5b,		/* 91 */
/* 1198 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1200 */	NdrFcLong( 0xb54493a0 ),	/* -1253796960 */
/* 1204 */	NdrFcShort( 0x65b7 ),	/* 26039 */
/* 1206 */	NdrFcShort( 0x408c ),	/* 16524 */
/* 1208 */	0xb6,		/* 182 */
			0x50,		/* 80 */
/* 1210 */	0x6,		/* 6 */
			0x26,		/* 38 */
/* 1212 */	0x5d,		/* 93 */
			0x21,		/* 33 */
/* 1214 */	0x82,		/* 130 */
			0xac,		/* 172 */
/* 1216 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1218 */	NdrFcLong( 0xbcfcf95c ),	/* -1124271780 */
/* 1222 */	NdrFcShort( 0xde48 ),	/* -8632 */
/* 1224 */	NdrFcShort( 0x4f42 ),	/* 20290 */
/* 1226 */	0xb0,		/* 176 */
			0xe9,		/* 233 */
/* 1228 */	0xd5,		/* 213 */
			0xd,		/* 13 */
/* 1230 */	0xb4,		/* 180 */
			0x7,		/* 7 */
/* 1232 */	0xdb,		/* 219 */
			0x53,		/* 83 */
/* 1234 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1236 */	NdrFcLong( 0xf0d6763a ),	/* -254380486 */
/* 1240 */	NdrFcShort( 0x182 ),	/* 386 */
/* 1242 */	NdrFcShort( 0x4136 ),	/* 16694 */
/* 1244 */	0xb1,		/* 177 */
			0xfa,		/* 250 */
/* 1246 */	0x50,		/* 80 */
			0x8e,		/* 142 */
/* 1248 */	0x33,		/* 51 */
			0x4c,		/* 76 */
/* 1250 */	0xff,		/* 255 */
			0xc1,		/* 193 */
/* 1252 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1254 */	NdrFcLong( 0x57b500a ),	/* 91967498 */
/* 1258 */	NdrFcShort( 0x4ba2 ),	/* 19362 */
/* 1260 */	NdrFcShort( 0x496a ),	/* 18794 */
/* 1262 */	0xb1,		/* 177 */
			0xcd,		/* 205 */
/* 1264 */	0xc5,		/* 197 */
			0xde,		/* 222 */
/* 1266 */	0xd3,		/* 211 */
			0xcc,		/* 204 */
/* 1268 */	0xc6,		/* 198 */
			0x1b,		/* 27 */
/* 1270 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1272 */	NdrFcLong( 0x2cb8867e ),	/* 750290558 */
/* 1276 */	NdrFcShort( 0x495e ),	/* 18782 */
/* 1278 */	NdrFcShort( 0x459f ),	/* 17823 */
/* 1280 */	0xb1,		/* 177 */
			0xb6,		/* 182 */
/* 1282 */	0x2d,		/* 45 */
			0xd7,		/* 215 */
/* 1284 */	0xff,		/* 255 */
			0xdb,		/* 219 */
/* 1286 */	0xd4,		/* 212 */
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
    380
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
    &IUpdateStateUser_ProxyInfo,
    &IID_IUpdateStateUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdateStateUser::get_state */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateUser::get_appId */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateUser::get_nextVersion */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateUser::get_downloadedBytes */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateUser::get_totalBytes */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateUser::get_installProgress */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateUser::get_errorCategory */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateUser::get_errorCode */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateUser::get_extraCode1 */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateUser::get_installerText */ ,
    (void *) (INT_PTR) -1 /* IUpdateStateUser::get_installerCommandLine */
};

const CInterfaceStubVtbl _IUpdateStateUserStubVtbl =
{
    &IID_IUpdateStateUser,
    &IUpdateStateUser_ServerInfo,
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
    38
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
    &ICompleteStatusUser_ProxyInfo,
    &IID_ICompleteStatusUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* ICompleteStatusUser::get_statusCode */ ,
    (void *) (INT_PTR) -1 /* ICompleteStatusUser::get_statusMessage */
};

const CInterfaceStubVtbl _ICompleteStatusUserStubVtbl =
{
    &IID_ICompleteStatusUser,
    &ICompleteStatusUser_ServerInfo,
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
    38
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
    646
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
    &IUpdaterCallbackUser_ProxyInfo,
    &IID_IUpdaterCallbackUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterCallbackUser::Run */
};

const CInterfaceStubVtbl _IUpdaterCallbackUserStubVtbl =
{
    &IID_IUpdaterCallbackUser,
    &IUpdaterCallbackUser_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterCallbackSystem, ver. 0.0,
   GUID={0xF0D6763A,0x0182,0x4136,{0xB1,0xFA,0x50,0x8E,0x33,0x4C,0xFF,0xC1}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterCallbackSystem_FormatStringOffsetTable[] =
    {
    646
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
    684,
    722,
    760,
    798,
    836,
    342
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterAppState_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterAppState_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterAppState_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
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


/* Object interface: IUpdaterAppStateUser, ver. 0.0,
   GUID={0x028FEB84,0x44BC,0x4A73,{0xA0,0xCD,0x60,0x36,0x78,0x15,0x5C,0xC3}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterAppStateUser_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    684,
    722,
    760,
    798,
    836,
    342
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterAppStateUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterAppStateUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterAppStateUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterAppStateUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(13) _IUpdaterAppStateUserProxyVtbl = 
{
    &IUpdaterAppStateUser_ProxyInfo,
    &IID_IUpdaterAppStateUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppStateUser::get_appId */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppStateUser::get_version */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppStateUser::get_ap */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppStateUser::get_brandCode */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppStateUser::get_brandPath */ ,
    (void *) (INT_PTR) -1 /* IUpdaterAppStateUser::get_ecp */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IUpdaterAppStateUser_table[] =
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

CInterfaceStubVtbl _IUpdaterAppStateUserStubVtbl =
{
    &IID_IUpdaterAppStateUser,
    &IUpdaterAppStateUser_ServerInfo,
    13,
    &IUpdaterAppStateUser_table[-3],
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
    684,
    722,
    760,
    798,
    836,
    342
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterAppStateSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterAppStateSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterAppStateSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
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
    874
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterAppStatesCallback_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterAppStatesCallback_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterAppStatesCallback_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
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


/* Object interface: IUpdaterAppStatesCallbackUser, ver. 0.0,
   GUID={0xBCFCF95C,0xDE48,0x4F42,{0xB0,0xE9,0xD5,0x0D,0xB4,0x07,0xDB,0x53}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterAppStatesCallbackUser_FormatStringOffsetTable[] =
    {
    874
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterAppStatesCallbackUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterAppStatesCallbackUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterAppStatesCallbackUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterAppStatesCallbackUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IUpdaterAppStatesCallbackUserProxyVtbl = 
{
    &IUpdaterAppStatesCallbackUser_ProxyInfo,
    &IID_IUpdaterAppStatesCallbackUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterAppStatesCallbackUser::Run */
};

const CInterfaceStubVtbl _IUpdaterAppStatesCallbackUserStubVtbl =
{
    &IID_IUpdaterAppStatesCallbackUser,
    &IUpdaterAppStatesCallbackUser_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterAppStatesCallbackSystem, ver. 0.0,
   GUID={0x2CB8867E,0x495E,0x459F,{0xB1,0xB6,0x2D,0xD7,0xFF,0xDB,0xD4,0x62}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterAppStatesCallbackSystem_FormatStringOffsetTable[] =
    {
    874
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterAppStatesCallbackSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterAppStatesCallbackSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterAppStatesCallbackSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
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
    912,
    950,
    988,
    1062,
    1100,
    1156,
    1218,
    1256,
    1348,
    1386,
    1454
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


/* Object interface: IUpdaterUser, ver. 0.0,
   GUID={0x02AFCB67,0x0899,0x4676,{0x91,0xA9,0x67,0xD9,0x2B,0x3B,0x79,0x18}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterUser_FormatStringOffsetTable[] =
    {
    912,
    1492,
    1530,
    1604,
    1642,
    1698,
    1760,
    1798,
    1348,
    1890,
    1958
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
CINTERFACE_PROXY_VTABLE(14) _IUpdaterUserProxyVtbl = 
{
    &IUpdaterUser_ProxyInfo,
    &IID_IUpdaterUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::GetVersion */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::FetchPolicies */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::RegisterApp */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::RunPeriodicTasks */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::CheckForUpdate */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::Update */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::UpdateAll */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::Install */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::CancelInstalls */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::RunInstaller */ ,
    (void *) (INT_PTR) -1 /* IUpdaterUser::GetAppStates */
};

const CInterfaceStubVtbl _IUpdaterUserStubVtbl =
{
    &IID_IUpdaterUser,
    &IUpdaterUser_ServerInfo,
    14,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdaterSystem, ver. 0.0,
   GUID={0xFCE335F3,0xA55C,0x496E,{0x81,0x4F,0x85,0x97,0x1C,0x9F,0xA6,0xF1}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterSystem_FormatStringOffsetTable[] =
    {
    912,
    1996,
    2034,
    2108,
    2146,
    2202,
    2264,
    2302,
    1348,
    2394,
    2462
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
    ( CInterfaceProxyVtbl *) &_IUpdaterAppStateSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterCallbackSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ICompleteStatusUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterAppStateProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterAppStatesCallbackUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ICompleteStatusSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterAppStatesCallbackSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterCallbackProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterAppStateUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterCallbackUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdateStateUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterObserverUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ICompleteStatusProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterAppStatesCallbackProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterObserverProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _updater_idl_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_IUpdateStateSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterObserverSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdateStateStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterAppStateSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterCallbackSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_ICompleteStatusUserStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterAppStateStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterAppStatesCallbackUserStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterUserStubVtbl,
    ( CInterfaceStubVtbl *) &_ICompleteStatusSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterAppStatesCallbackSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterCallbackStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterAppStateUserStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterCallbackUserStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdateStateUserStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterObserverUserStubVtbl,
    ( CInterfaceStubVtbl *) &_ICompleteStatusStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterAppStatesCallbackStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterObserverStubVtbl,
    0
};

PCInterfaceName const _updater_idl_InterfaceNamesList[] = 
{
    "IUpdateStateSystem",
    "IUpdaterObserverSystem",
    "IUpdateState",
    "IUpdaterAppStateSystem",
    "IUpdaterCallbackSystem",
    "ICompleteStatusUser",
    "IUpdaterAppState",
    "IUpdaterAppStatesCallbackUser",
    "IUpdaterUser",
    "ICompleteStatusSystem",
    "IUpdaterAppStatesCallbackSystem",
    "IUpdaterCallback",
    "IUpdaterAppStateUser",
    "IUpdaterCallbackUser",
    "IUpdateStateUser",
    "IUpdaterObserverUser",
    "ICompleteStatus",
    "IUpdater",
    "IUpdaterAppStatesCallback",
    "IUpdaterSystem",
    "IUpdaterObserver",
    0
};

const IID *  const _updater_idl_BaseIIDList[] = 
{
    0,
    0,
    0,
    &IID_IDispatch,
    0,
    0,
    &IID_IDispatch,
    0,
    0,
    0,
    0,
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


#define _updater_idl_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _updater_idl, pIID, n)

int __stdcall _updater_idl_IID_Lookup( const IID * pIID, int * pIndex )
{
    IID_BS_LOOKUP_SETUP

    IID_BS_LOOKUP_INITIAL_TEST( _updater_idl, 21, 16 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_idl, 8 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_idl, 4 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_idl, 2 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_idl, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _updater_idl, 21, *pIndex )
    
}

EXTERN_C const ExtendedProxyFileInfo updater_idl_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _updater_idl_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _updater_idl_StubVtblList,
    (const PCInterfaceName * ) & _updater_idl_InterfaceNamesList,
    (const IID ** ) & _updater_idl_BaseIIDList,
    & _updater_idl_IID_Lookup, 
    21,
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

