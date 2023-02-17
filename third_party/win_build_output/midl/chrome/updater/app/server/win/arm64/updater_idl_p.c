

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for gen/chrome/updater/app/server/win/updater_idl.idl:
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


#include "updater_idl.h"

#define TYPE_FORMAT_STRING_SIZE   271                               
#define PROC_FORMAT_STRING_SIZE   2281                              
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

#if !defined(__RPC_ARM64__)
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

	/* Parameter __MIDL__ICompleteStatus0000 */


	/* Parameter __MIDL__IUpdateState0000 */

/* 30 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 32 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 34 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 36 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 38 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 40 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_statusMessage */


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

	/* Parameter __MIDL__ICompleteStatus0001 */


	/* Parameter __MIDL__IUpdateState0001 */

/* 72 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 74 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 76 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */

/* 78 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 80 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 82 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

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

	/* Parameter __MIDL__IUpdateState0002 */

/* 114 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 116 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 118 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */

/* 120 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 122 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 124 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

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

	/* Parameter __MIDL__IUpdateState0003 */

/* 156 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 158 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 160 */	0xb,		/* FC_HYPER */
			0x0,		/* 0 */

	/* Return value */

/* 162 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 164 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 166 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

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

	/* Parameter __MIDL__IUpdateState0004 */

/* 198 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 200 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 202 */	0xb,		/* FC_HYPER */
			0x0,		/* 0 */

	/* Return value */

/* 204 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 206 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 208 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

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

	/* Parameter __MIDL__IUpdateState0005 */

/* 240 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 242 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 244 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 246 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 248 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 250 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

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

	/* Parameter __MIDL__IUpdateState0006 */

/* 282 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 284 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 286 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 288 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 290 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 292 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

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

	/* Parameter __MIDL__IUpdateState0007 */

/* 324 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 326 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 328 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 330 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 332 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 334 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

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

	/* Parameter __MIDL__IUpdateState0008 */

/* 366 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 368 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 370 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 372 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 374 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 376 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

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

	/* Parameter __MIDL__IUpdateState0009 */

/* 408 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 410 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 412 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */

/* 414 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 416 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 418 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

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

	/* Parameter __MIDL__IUpdateState0010 */

/* 450 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 452 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 454 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

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

	/* Procedure OnStateChange */

/* 630 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 632 */	NdrFcLong( 0x0 ),	/* 0 */
/* 636 */	NdrFcShort( 0x3 ),	/* 3 */
/* 638 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 640 */	NdrFcShort( 0x0 ),	/* 0 */
/* 642 */	NdrFcShort( 0x8 ),	/* 8 */
/* 644 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
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

	/* Parameter update_state */

/* 660 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 662 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 664 */	NdrFcShort( 0x7a ),	/* Type Offset=122 */

	/* Return value */

/* 666 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 668 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 670 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnComplete */

/* 672 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 674 */	NdrFcLong( 0x0 ),	/* 0 */
/* 678 */	NdrFcShort( 0x4 ),	/* 4 */
/* 680 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 682 */	NdrFcShort( 0x0 ),	/* 0 */
/* 684 */	NdrFcShort( 0x8 ),	/* 8 */
/* 686 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 688 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 690 */	NdrFcShort( 0x0 ),	/* 0 */
/* 692 */	NdrFcShort( 0x0 ),	/* 0 */
/* 694 */	NdrFcShort( 0x0 ),	/* 0 */
/* 696 */	NdrFcShort( 0x2 ),	/* 2 */
/* 698 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 700 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter status */

/* 702 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 704 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 706 */	NdrFcShort( 0x8c ),	/* Type Offset=140 */

	/* Return value */

/* 708 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 710 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 712 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Run */

/* 714 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 716 */	NdrFcLong( 0x0 ),	/* 0 */
/* 720 */	NdrFcShort( 0x3 ),	/* 3 */
/* 722 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 724 */	NdrFcShort( 0x8 ),	/* 8 */
/* 726 */	NdrFcShort( 0x8 ),	/* 8 */
/* 728 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 730 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 732 */	NdrFcShort( 0x0 ),	/* 0 */
/* 734 */	NdrFcShort( 0x0 ),	/* 0 */
/* 736 */	NdrFcShort( 0x0 ),	/* 0 */
/* 738 */	NdrFcShort( 0x2 ),	/* 2 */
/* 740 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 742 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter result */

/* 744 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 746 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 748 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 750 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 752 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 754 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure GetVersion */


	/* Procedure GetVersion */


	/* Procedure GetVersion */

/* 756 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 758 */	NdrFcLong( 0x0 ),	/* 0 */
/* 762 */	NdrFcShort( 0x3 ),	/* 3 */
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

	/* Parameter version */


	/* Parameter version */


	/* Parameter version */

/* 786 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 788 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 790 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 792 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 794 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 796 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure FetchPolicies */

/* 798 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 800 */	NdrFcLong( 0x0 ),	/* 0 */
/* 804 */	NdrFcShort( 0x4 ),	/* 4 */
/* 806 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 808 */	NdrFcShort( 0x0 ),	/* 0 */
/* 810 */	NdrFcShort( 0x8 ),	/* 8 */
/* 812 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 814 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 816 */	NdrFcShort( 0x0 ),	/* 0 */
/* 818 */	NdrFcShort( 0x0 ),	/* 0 */
/* 820 */	NdrFcShort( 0x0 ),	/* 0 */
/* 822 */	NdrFcShort( 0x2 ),	/* 2 */
/* 824 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 826 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter callback */

/* 828 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 830 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 832 */	NdrFcShort( 0x9e ),	/* Type Offset=158 */

	/* Return value */

/* 834 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 836 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 838 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure CheckForUpdate */


	/* Procedure CheckForUpdate */


	/* Procedure CheckForUpdate */

/* 840 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 842 */	NdrFcLong( 0x0 ),	/* 0 */
/* 846 */	NdrFcShort( 0x5 ),	/* 5 */
/* 848 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 850 */	NdrFcShort( 0x0 ),	/* 0 */
/* 852 */	NdrFcShort( 0x8 ),	/* 8 */
/* 854 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 856 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 858 */	NdrFcShort( 0x0 ),	/* 0 */
/* 860 */	NdrFcShort( 0x0 ),	/* 0 */
/* 862 */	NdrFcShort( 0x0 ),	/* 0 */
/* 864 */	NdrFcShort( 0x2 ),	/* 2 */
/* 866 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 868 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter app_id */


	/* Parameter app_id */


	/* Parameter app_id */

/* 870 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 872 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 874 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 876 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 878 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 880 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RegisterApp */

/* 882 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 884 */	NdrFcLong( 0x0 ),	/* 0 */
/* 888 */	NdrFcShort( 0x6 ),	/* 6 */
/* 890 */	NdrFcShort( 0x48 ),	/* ARM64 Stack size/offset = 72 */
/* 892 */	NdrFcShort( 0x0 ),	/* 0 */
/* 894 */	NdrFcShort( 0x8 ),	/* 8 */
/* 896 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x8,		/* 8 */
/* 898 */	0x14,		/* 20 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 900 */	NdrFcShort( 0x0 ),	/* 0 */
/* 902 */	NdrFcShort( 0x0 ),	/* 0 */
/* 904 */	NdrFcShort( 0x0 ),	/* 0 */
/* 906 */	NdrFcShort( 0x8 ),	/* 8 */
/* 908 */	0x8,		/* 8 */
			0x80,		/* 128 */
/* 910 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 912 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 914 */	0x85,		/* 133 */
			0x86,		/* 134 */
/* 916 */	0x87,		/* 135 */
			0x0,		/* 0 */

	/* Parameter app_id */

/* 918 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 920 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 922 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_code */

/* 924 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 926 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 928 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_path */

/* 930 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 932 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 934 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter tag */

/* 936 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 938 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 940 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter version */

/* 942 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 944 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 946 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter existence_checker_path */

/* 948 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 950 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 952 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter callback */

/* 954 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 956 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 958 */	NdrFcShort( 0x9e ),	/* Type Offset=158 */

	/* Return value */

/* 960 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 962 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 964 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunPeriodicTasks */

/* 966 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 968 */	NdrFcLong( 0x0 ),	/* 0 */
/* 972 */	NdrFcShort( 0x7 ),	/* 7 */
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
/* 1000 */	NdrFcShort( 0x9e ),	/* Type Offset=158 */

	/* Return value */

/* 1002 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1004 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1006 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Update */

/* 1008 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1010 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1014 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1016 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1018 */	NdrFcShort( 0x18 ),	/* 24 */
/* 1020 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1022 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 1024 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1026 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1028 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1030 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1032 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1034 */	0x7,		/* 7 */
			0x80,		/* 128 */
/* 1036 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1038 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1040 */	0x85,		/* 133 */
			0x86,		/* 134 */

	/* Parameter app_id */

/* 1042 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1044 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1046 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data_index */

/* 1048 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1050 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1052 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter priority */

/* 1054 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1056 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1058 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 1060 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1062 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1064 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter do_update_check_only */

/* 1066 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1068 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1070 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1072 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1074 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1076 */	NdrFcShort( 0xb4 ),	/* Type Offset=180 */

	/* Return value */

/* 1078 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1080 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 1082 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure UpdateAll */

/* 1084 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1086 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1090 */	NdrFcShort( 0x9 ),	/* 9 */
/* 1092 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1094 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1096 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1098 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1100 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1102 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1104 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1106 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1108 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1110 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1112 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1114 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1116 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1118 */	NdrFcShort( 0xb4 ),	/* Type Offset=180 */

	/* Return value */

/* 1120 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1122 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1124 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Install */

/* 1126 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1128 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1132 */	NdrFcShort( 0xa ),	/* 10 */
/* 1134 */	NdrFcShort( 0x60 ),	/* ARM64 Stack size/offset = 96 */
/* 1136 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1138 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1140 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0xb,		/* 11 */
/* 1142 */	0x16,		/* 22 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1144 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1146 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1148 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1150 */	NdrFcShort( 0xb ),	/* 11 */
/* 1152 */	0xb,		/* 11 */
			0x80,		/* 128 */
/* 1154 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1156 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1158 */	0x85,		/* 133 */
			0x86,		/* 134 */
/* 1160 */	0x87,		/* 135 */
			0xf8,		/* 248 */
/* 1162 */	0xf8,		/* 248 */
			0xf8,		/* 248 */

	/* Parameter app_id */

/* 1164 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1166 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1168 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_code */

/* 1170 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1172 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1174 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_path */

/* 1176 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1178 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1180 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter tag */

/* 1182 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1184 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1186 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter version */

/* 1188 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1190 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1192 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter existence_checker_path */

/* 1194 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1196 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1198 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter client_install_data */

/* 1200 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1202 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 1204 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data_index */

/* 1206 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1208 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1210 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter priority */

/* 1212 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1214 */	NdrFcShort( 0x48 ),	/* ARM64 Stack size/offset = 72 */
/* 1216 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1218 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1220 */	NdrFcShort( 0x50 ),	/* ARM64 Stack size/offset = 80 */
/* 1222 */	NdrFcShort( 0xb4 ),	/* Type Offset=180 */

	/* Return value */

/* 1224 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1226 */	NdrFcShort( 0x58 ),	/* ARM64 Stack size/offset = 88 */
/* 1228 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure CancelInstalls */


	/* Procedure CancelInstalls */


	/* Procedure CancelInstalls */

/* 1230 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1232 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1236 */	NdrFcShort( 0xb ),	/* 11 */
/* 1238 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1240 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1242 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1244 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1246 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1248 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1250 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1252 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1254 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1256 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1258 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter app_id */


	/* Parameter app_id */


	/* Parameter app_id */

/* 1260 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1262 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1264 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1266 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1268 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1270 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunInstaller */

/* 1272 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1274 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1278 */	NdrFcShort( 0xc ),	/* 12 */
/* 1280 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1282 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1284 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1286 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 1288 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1290 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1292 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1294 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1296 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1298 */	0x7,		/* 7 */
			0x80,		/* 128 */
/* 1300 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1302 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1304 */	0x85,		/* 133 */
			0x86,		/* 134 */

	/* Parameter app_id */

/* 1306 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1308 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1310 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter installer_path */

/* 1312 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1314 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1316 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_args */

/* 1318 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1320 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1322 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data */

/* 1324 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1326 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1328 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_settings */

/* 1330 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1332 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1334 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter observer */

/* 1336 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1338 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1340 */	NdrFcShort( 0xb4 ),	/* Type Offset=180 */

	/* Return value */

/* 1342 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1344 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 1346 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure FetchPolicies */

/* 1348 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1350 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1354 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1356 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1358 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1360 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1362 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1364 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1366 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1368 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1370 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1372 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1374 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1376 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter callback */

/* 1378 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1380 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1382 */	NdrFcShort( 0xc6 ),	/* Type Offset=198 */

	/* Return value */

/* 1384 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1386 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1388 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RegisterApp */

/* 1390 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1392 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1396 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1398 */	NdrFcShort( 0x48 ),	/* ARM64 Stack size/offset = 72 */
/* 1400 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1402 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1404 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x8,		/* 8 */
/* 1406 */	0x14,		/* 20 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1408 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1410 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1412 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1414 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1416 */	0x8,		/* 8 */
			0x80,		/* 128 */
/* 1418 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1420 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1422 */	0x85,		/* 133 */
			0x86,		/* 134 */
/* 1424 */	0x87,		/* 135 */
			0x0,		/* 0 */

	/* Parameter app_id */

/* 1426 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1428 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1430 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_code */

/* 1432 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1434 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1436 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_path */

/* 1438 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1440 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1442 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter tag */

/* 1444 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1446 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1448 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter version */

/* 1450 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1452 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1454 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter existence_checker_path */

/* 1456 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1458 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1460 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter callback */

/* 1462 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1464 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 1466 */	NdrFcShort( 0xc6 ),	/* Type Offset=198 */

	/* Return value */

/* 1468 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1470 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1472 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunPeriodicTasks */

/* 1474 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1476 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1480 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1482 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1484 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1486 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1488 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1490 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1492 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1494 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1496 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1498 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1500 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1502 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter callback */

/* 1504 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1506 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1508 */	NdrFcShort( 0xc6 ),	/* Type Offset=198 */

	/* Return value */

/* 1510 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1512 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1514 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Update */

/* 1516 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1518 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1522 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1524 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1526 */	NdrFcShort( 0x18 ),	/* 24 */
/* 1528 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1530 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 1532 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1534 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1536 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1538 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1540 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1542 */	0x7,		/* 7 */
			0x80,		/* 128 */
/* 1544 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1546 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1548 */	0x85,		/* 133 */
			0x86,		/* 134 */

	/* Parameter app_id */

/* 1550 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1552 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1554 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data_index */

/* 1556 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1558 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1560 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter priority */

/* 1562 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1564 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1566 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 1568 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1570 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1572 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter do_update_check_only */

/* 1574 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1576 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1578 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1580 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1582 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1584 */	NdrFcShort( 0xd8 ),	/* Type Offset=216 */

	/* Return value */

/* 1586 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1588 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 1590 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure UpdateAll */

/* 1592 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1594 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1598 */	NdrFcShort( 0x9 ),	/* 9 */
/* 1600 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1602 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1604 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1606 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1608 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1610 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1612 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1614 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1616 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1618 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1620 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1622 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1624 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1626 */	NdrFcShort( 0xd8 ),	/* Type Offset=216 */

	/* Return value */

/* 1628 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1630 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1632 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Install */

/* 1634 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1636 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1640 */	NdrFcShort( 0xa ),	/* 10 */
/* 1642 */	NdrFcShort( 0x60 ),	/* ARM64 Stack size/offset = 96 */
/* 1644 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1646 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1648 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0xb,		/* 11 */
/* 1650 */	0x16,		/* 22 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1652 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1654 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1656 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1658 */	NdrFcShort( 0xb ),	/* 11 */
/* 1660 */	0xb,		/* 11 */
			0x80,		/* 128 */
/* 1662 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1664 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1666 */	0x85,		/* 133 */
			0x86,		/* 134 */
/* 1668 */	0x87,		/* 135 */
			0xf8,		/* 248 */
/* 1670 */	0xf8,		/* 248 */
			0xf8,		/* 248 */

	/* Parameter app_id */

/* 1672 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1674 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1676 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_code */

/* 1678 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1680 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1682 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_path */

/* 1684 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1686 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1688 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter tag */

/* 1690 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1692 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1694 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter version */

/* 1696 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1698 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1700 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter existence_checker_path */

/* 1702 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1704 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1706 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter client_install_data */

/* 1708 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1710 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 1712 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data_index */

/* 1714 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1716 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1718 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter priority */

/* 1720 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1722 */	NdrFcShort( 0x48 ),	/* ARM64 Stack size/offset = 72 */
/* 1724 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 1726 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1728 */	NdrFcShort( 0x50 ),	/* ARM64 Stack size/offset = 80 */
/* 1730 */	NdrFcShort( 0xd8 ),	/* Type Offset=216 */

	/* Return value */

/* 1732 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1734 */	NdrFcShort( 0x58 ),	/* ARM64 Stack size/offset = 88 */
/* 1736 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunInstaller */

/* 1738 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1740 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1744 */	NdrFcShort( 0xc ),	/* 12 */
/* 1746 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1748 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1750 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1752 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 1754 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1756 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1758 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1760 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1762 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1764 */	0x7,		/* 7 */
			0x80,		/* 128 */
/* 1766 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1768 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1770 */	0x85,		/* 133 */
			0x86,		/* 134 */

	/* Parameter app_id */

/* 1772 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1774 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1776 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter installer_path */

/* 1778 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1780 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1782 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_args */

/* 1784 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1786 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1788 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data */

/* 1790 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1792 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1794 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_settings */

/* 1796 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1798 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1800 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter observer */

/* 1802 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1804 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1806 */	NdrFcShort( 0xd8 ),	/* Type Offset=216 */

	/* Return value */

/* 1808 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1810 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 1812 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure FetchPolicies */

/* 1814 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1816 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1820 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1822 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1824 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1826 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1828 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1830 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1832 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1834 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1836 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1838 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1840 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1842 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter callback */

/* 1844 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1846 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1848 */	NdrFcShort( 0xea ),	/* Type Offset=234 */

	/* Return value */

/* 1850 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1852 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1854 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RegisterApp */

/* 1856 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1858 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1862 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1864 */	NdrFcShort( 0x48 ),	/* ARM64 Stack size/offset = 72 */
/* 1866 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1868 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1870 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x8,		/* 8 */
/* 1872 */	0x14,		/* 20 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1874 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1876 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1878 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1880 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1882 */	0x8,		/* 8 */
			0x80,		/* 128 */
/* 1884 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1886 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1888 */	0x85,		/* 133 */
			0x86,		/* 134 */
/* 1890 */	0x87,		/* 135 */
			0x0,		/* 0 */

	/* Parameter app_id */

/* 1892 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1894 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1896 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_code */

/* 1898 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1900 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1902 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_path */

/* 1904 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1906 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1908 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter tag */

/* 1910 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1912 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1914 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter version */

/* 1916 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1918 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1920 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter existence_checker_path */

/* 1922 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1924 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1926 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter callback */

/* 1928 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1930 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 1932 */	NdrFcShort( 0xea ),	/* Type Offset=234 */

	/* Return value */

/* 1934 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1936 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1938 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunPeriodicTasks */

/* 1940 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1942 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1946 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1948 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1950 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1952 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1954 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1956 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1958 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1960 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1962 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1964 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1966 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1968 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter callback */

/* 1970 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 1972 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1974 */	NdrFcShort( 0xea ),	/* Type Offset=234 */

	/* Return value */

/* 1976 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1978 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1980 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Update */

/* 1982 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1984 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1988 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1990 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1992 */	NdrFcShort( 0x18 ),	/* 24 */
/* 1994 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1996 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 1998 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2000 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2002 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2004 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2006 */	NdrFcShort( 0x7 ),	/* 7 */
/* 2008 */	0x7,		/* 7 */
			0x80,		/* 128 */
/* 2010 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2012 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 2014 */	0x85,		/* 133 */
			0x86,		/* 134 */

	/* Parameter app_id */

/* 2016 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2018 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2020 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data_index */

/* 2022 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2024 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2026 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter priority */

/* 2028 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2030 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2032 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter same_version_update_allowed */

/* 2034 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2036 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2038 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter do_update_check_only */

/* 2040 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2042 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2044 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 2046 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2048 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2050 */	NdrFcShort( 0xfc ),	/* Type Offset=252 */

	/* Return value */

/* 2052 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2054 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2056 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure UpdateAll */

/* 2058 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2060 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2064 */	NdrFcShort( 0x9 ),	/* 9 */
/* 2066 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2068 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2070 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2072 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 2074 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2076 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2078 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2080 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2082 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2084 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2086 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter observer */

/* 2088 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2090 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2092 */	NdrFcShort( 0xfc ),	/* Type Offset=252 */

	/* Return value */

/* 2094 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2096 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2098 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Install */

/* 2100 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2102 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2106 */	NdrFcShort( 0xa ),	/* 10 */
/* 2108 */	NdrFcShort( 0x60 ),	/* ARM64 Stack size/offset = 96 */
/* 2110 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2112 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2114 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0xb,		/* 11 */
/* 2116 */	0x16,		/* 22 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2118 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2120 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2122 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2124 */	NdrFcShort( 0xb ),	/* 11 */
/* 2126 */	0xb,		/* 11 */
			0x80,		/* 128 */
/* 2128 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2130 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 2132 */	0x85,		/* 133 */
			0x86,		/* 134 */
/* 2134 */	0x87,		/* 135 */
			0xf8,		/* 248 */
/* 2136 */	0xf8,		/* 248 */
			0xf8,		/* 248 */

	/* Parameter app_id */

/* 2138 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2140 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2142 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_code */

/* 2144 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2146 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2148 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter brand_path */

/* 2150 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2152 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2154 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter tag */

/* 2156 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2158 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2160 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter version */

/* 2162 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2164 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2166 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter existence_checker_path */

/* 2168 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2170 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2172 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter client_install_data */

/* 2174 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2176 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2178 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data_index */

/* 2180 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2182 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 2184 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter priority */

/* 2186 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2188 */	NdrFcShort( 0x48 ),	/* ARM64 Stack size/offset = 72 */
/* 2190 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter observer */

/* 2192 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2194 */	NdrFcShort( 0x50 ),	/* ARM64 Stack size/offset = 80 */
/* 2196 */	NdrFcShort( 0xfc ),	/* Type Offset=252 */

	/* Return value */

/* 2198 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2200 */	NdrFcShort( 0x58 ),	/* ARM64 Stack size/offset = 88 */
/* 2202 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunInstaller */

/* 2204 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2206 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2210 */	NdrFcShort( 0xc ),	/* 12 */
/* 2212 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 2214 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2216 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2218 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 2220 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2222 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2224 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2226 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2228 */	NdrFcShort( 0x7 ),	/* 7 */
/* 2230 */	0x7,		/* 7 */
			0x80,		/* 128 */
/* 2232 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2234 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 2236 */	0x85,		/* 133 */
			0x86,		/* 134 */

	/* Parameter app_id */

/* 2238 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2240 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2242 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter installer_path */

/* 2244 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2246 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2248 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_args */

/* 2250 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2252 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2254 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_data */

/* 2256 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2258 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2260 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter install_settings */

/* 2262 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2264 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2266 */	NdrFcShort( 0xb2 ),	/* Type Offset=178 */

	/* Parameter observer */

/* 2268 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 2270 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2272 */	NdrFcShort( 0xfc ),	/* Type Offset=252 */

	/* Return value */

/* 2274 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2276 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2278 */	0x8,		/* FC_LONG */
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
    42,
    84,
    126,
    168,
    210,
    252,
    294,
    336,
    378,
    420,
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
    42,
    84,
    126,
    168,
    210,
    252,
    294,
    336,
    378,
    420,
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
    42
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
    42,
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
    42,
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
    462,
    504
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
    546,
    588
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
    630,
    672
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
    714
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
    714,
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
    714,
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
    756,
    798,
    840,
    882,
    966,
    1008,
    1084,
    1126,
    1230,
    1272
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
    756,
    1348,
    840,
    1390,
    1474,
    1516,
    1592,
    1634,
    1230,
    1738
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
    756,
    1814,
    840,
    1856,
    1940,
    1982,
    2058,
    2100,
    1230,
    2204
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


#endif /* defined(_M_ARM64) */

