

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for gen/chrome/updater/app/server/win/updater_legacy_idl.idl:
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


#include "updater_legacy_idl.h"

#define TYPE_FORMAT_STRING_SIZE   1177                              
#define PROC_FORMAT_STRING_SIZE   4639                              
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   2            

typedef struct _updater_legacy_idl_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } updater_legacy_idl_MIDL_TYPE_FORMAT_STRING;

typedef struct _updater_legacy_idl_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } updater_legacy_idl_MIDL_PROC_FORMAT_STRING;

typedef struct _updater_legacy_idl_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } updater_legacy_idl_MIDL_EXPR_FORMAT_STRING;


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


extern const updater_legacy_idl_MIDL_TYPE_FORMAT_STRING updater_legacy_idl__MIDL_TypeFormatString;
extern const updater_legacy_idl_MIDL_PROC_FORMAT_STRING updater_legacy_idl__MIDL_ProcFormatString;
extern const updater_legacy_idl_MIDL_EXPR_FORMAT_STRING updater_legacy_idl__MIDL_ExprFormatString;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAppVersionWeb_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppVersionWeb_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAppVersionWebUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppVersionWebUser_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAppVersionWebSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppVersionWebSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO ICurrentState_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ICurrentState_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO ICurrentStateUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ICurrentStateUser_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO ICurrentStateSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ICurrentStateSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IGoogleUpdate3Web_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IGoogleUpdate3Web_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IGoogleUpdate3WebUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IGoogleUpdate3WebUser_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IGoogleUpdate3WebSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IGoogleUpdate3WebSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAppBundleWeb_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppBundleWeb_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAppBundleWebUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppBundleWebUser_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAppBundleWebSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppBundleWebSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAppWeb_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppWeb_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAppWebUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppWebUser_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAppWebSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppWebSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAppCommandWeb_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppCommandWeb_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAppCommandWebUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppCommandWebUser_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAppCommandWebSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppCommandWebSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IPolicyStatus_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatus_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IPolicyStatusUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatusUser_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IPolicyStatusSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatusSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IPolicyStatusValue_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatusValue_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IPolicyStatusValueUser_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatusValueUser_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IPolicyStatusValueSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatusValueSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IPolicyStatus2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatus2_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IPolicyStatus2User_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatus2User_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IPolicyStatus2System_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatus2System_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IPolicyStatus3_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatus3_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IPolicyStatus3User_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatus3User_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IPolicyStatus3System_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatus3System_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IPolicyStatus4_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatus4_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IPolicyStatus4User_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatus4User_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IPolicyStatus4System_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatus4System_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IProcessLauncher_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IProcessLauncher_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IProcessLauncherSystem_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IProcessLauncherSystem_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IProcessLauncher2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IProcessLauncher2_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IProcessLauncher2System_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IProcessLauncher2System_ProxyInfo;


extern const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ];

#if !defined(__RPC_ARM64__)
#error  Invalid build platform for this stub.
#endif

static const updater_legacy_idl_MIDL_PROC_FORMAT_STRING updater_legacy_idl__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure get_updaterVersion */


	/* Procedure get_updaterVersion */


	/* Procedure get_updaterVersion */


	/* Procedure get_source */


	/* Procedure get_source */


	/* Procedure get_source */


	/* Procedure get_appId */


	/* Procedure get_appId */


	/* Procedure get_appId */


	/* Procedure get_version */


	/* Procedure get_version */


	/* Procedure get_version */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x7 ),	/* 7 */
/*  8 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 10 */	NdrFcShort( 0x0 ),	/* 0 */
/* 12 */	NdrFcShort( 0x8 ),	/* 8 */
/* 14 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 16 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 18 */	NdrFcShort( 0x1 ),	/* 1 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x2 ),	/* 2 */
/* 26 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 28 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter version */


	/* Parameter version */


	/* Parameter version */


	/* Parameter __MIDL__IPolicyStatusValueSystem0000 */


	/* Parameter __MIDL__IPolicyStatusValueUser0000 */


	/* Parameter __MIDL__IPolicyStatusValue0000 */


	/* Parameter __MIDL__IAppWebSystem0000 */


	/* Parameter __MIDL__IAppWebUser0000 */


	/* Parameter __MIDL__IAppWeb0000 */


	/* Parameter __MIDL__IAppVersionWebSystem0000 */


	/* Parameter __MIDL__IAppVersionWebUser0000 */


	/* Parameter __MIDL__IAppVersionWeb0000 */

/* 30 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 32 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 34 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 36 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 38 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 40 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_exitCode */


	/* Procedure get_exitCode */


	/* Procedure get_exitCode */


	/* Procedure get_packageCount */


	/* Procedure get_packageCount */


	/* Procedure get_packageCount */

/* 42 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 44 */	NdrFcLong( 0x0 ),	/* 0 */
/* 48 */	NdrFcShort( 0x8 ),	/* 8 */
/* 50 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 52 */	NdrFcShort( 0x0 ),	/* 0 */
/* 54 */	NdrFcShort( 0x24 ),	/* 36 */
/* 56 */	0x44,		/* Oi2 Flags:  has return, has ext, */
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

	/* Parameter __MIDL__IAppCommandWebSystem0001 */


	/* Parameter __MIDL__IAppCommandWebUser0001 */


	/* Parameter __MIDL__IAppCommandWeb0001 */


	/* Parameter count */


	/* Parameter count */


	/* Parameter count */

/* 72 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 74 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 76 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 78 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 80 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 82 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_packageWeb */


	/* Procedure get_packageWeb */


	/* Procedure get_packageWeb */

/* 84 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 86 */	NdrFcLong( 0x0 ),	/* 0 */
/* 90 */	NdrFcShort( 0x9 ),	/* 9 */
/* 92 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 94 */	NdrFcShort( 0x8 ),	/* 8 */
/* 96 */	NdrFcShort( 0x8 ),	/* 8 */
/* 98 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 100 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 102 */	NdrFcShort( 0x0 ),	/* 0 */
/* 104 */	NdrFcShort( 0x0 ),	/* 0 */
/* 106 */	NdrFcShort( 0x0 ),	/* 0 */
/* 108 */	NdrFcShort( 0x3 ),	/* 3 */
/* 110 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 112 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter index */


	/* Parameter index */


	/* Parameter index */

/* 114 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 116 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 118 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter package */


	/* Parameter package */


	/* Parameter package */

/* 120 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 122 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 124 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 126 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 128 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 130 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_lastCheckPeriodMinutes */


	/* Procedure get_lastCheckPeriodMinutes */


	/* Procedure get_lastCheckPeriodMinutes */


	/* Procedure get_status */


	/* Procedure get_status */


	/* Procedure get_status */


	/* Procedure get_stateValue */


	/* Procedure get_stateValue */


	/* Procedure get_stateValue */

/* 132 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 134 */	NdrFcLong( 0x0 ),	/* 0 */
/* 138 */	NdrFcShort( 0x7 ),	/* 7 */
/* 140 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 142 */	NdrFcShort( 0x0 ),	/* 0 */
/* 144 */	NdrFcShort( 0x24 ),	/* 36 */
/* 146 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 148 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 150 */	NdrFcShort( 0x0 ),	/* 0 */
/* 152 */	NdrFcShort( 0x0 ),	/* 0 */
/* 154 */	NdrFcShort( 0x0 ),	/* 0 */
/* 156 */	NdrFcShort( 0x2 ),	/* 2 */
/* 158 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 160 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter minutes */


	/* Parameter minutes */


	/* Parameter minutes */


	/* Parameter __MIDL__IAppCommandWebSystem0000 */


	/* Parameter __MIDL__IAppCommandWebUser0000 */


	/* Parameter __MIDL__IAppCommandWeb0000 */


	/* Parameter __MIDL__ICurrentStateSystem0000 */


	/* Parameter __MIDL__ICurrentStateUser0000 */


	/* Parameter __MIDL__ICurrentState0000 */

/* 162 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 164 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 166 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 168 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 170 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 172 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_value */


	/* Procedure get_value */


	/* Procedure get_value */


	/* Procedure get_availableVersion */


	/* Procedure get_availableVersion */


	/* Procedure get_availableVersion */

/* 174 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 176 */	NdrFcLong( 0x0 ),	/* 0 */
/* 180 */	NdrFcShort( 0x8 ),	/* 8 */
/* 182 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 184 */	NdrFcShort( 0x0 ),	/* 0 */
/* 186 */	NdrFcShort( 0x8 ),	/* 8 */
/* 188 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 190 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 192 */	NdrFcShort( 0x1 ),	/* 1 */
/* 194 */	NdrFcShort( 0x0 ),	/* 0 */
/* 196 */	NdrFcShort( 0x0 ),	/* 0 */
/* 198 */	NdrFcShort( 0x2 ),	/* 2 */
/* 200 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 202 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IPolicyStatusValueSystem0001 */


	/* Parameter __MIDL__IPolicyStatusValueUser0001 */


	/* Parameter __MIDL__IPolicyStatusValue0001 */


	/* Parameter __MIDL__ICurrentStateSystem0001 */


	/* Parameter __MIDL__ICurrentStateUser0001 */


	/* Parameter __MIDL__ICurrentState0001 */

/* 204 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 206 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 208 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 210 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 212 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 214 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_bytesDownloaded */


	/* Procedure get_bytesDownloaded */


	/* Procedure get_bytesDownloaded */

/* 216 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 218 */	NdrFcLong( 0x0 ),	/* 0 */
/* 222 */	NdrFcShort( 0x9 ),	/* 9 */
/* 224 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 226 */	NdrFcShort( 0x0 ),	/* 0 */
/* 228 */	NdrFcShort( 0x24 ),	/* 36 */
/* 230 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 232 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 234 */	NdrFcShort( 0x0 ),	/* 0 */
/* 236 */	NdrFcShort( 0x0 ),	/* 0 */
/* 238 */	NdrFcShort( 0x0 ),	/* 0 */
/* 240 */	NdrFcShort( 0x2 ),	/* 2 */
/* 242 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 244 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentStateSystem0002 */


	/* Parameter __MIDL__ICurrentStateUser0002 */


	/* Parameter __MIDL__ICurrentState0002 */

/* 246 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 248 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 250 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 252 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 254 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 256 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_packageCacheSizeLimitMBytes */


	/* Procedure get_packageCacheSizeLimitMBytes */


	/* Procedure get_packageCacheSizeLimitMBytes */


	/* Procedure get_totalBytesToDownload */


	/* Procedure get_totalBytesToDownload */


	/* Procedure get_totalBytesToDownload */

/* 258 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 260 */	NdrFcLong( 0x0 ),	/* 0 */
/* 264 */	NdrFcShort( 0xa ),	/* 10 */
/* 266 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 268 */	NdrFcShort( 0x0 ),	/* 0 */
/* 270 */	NdrFcShort( 0x24 ),	/* 36 */
/* 272 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 274 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 276 */	NdrFcShort( 0x0 ),	/* 0 */
/* 278 */	NdrFcShort( 0x0 ),	/* 0 */
/* 280 */	NdrFcShort( 0x0 ),	/* 0 */
/* 282 */	NdrFcShort( 0x2 ),	/* 2 */
/* 284 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 286 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter limit */


	/* Parameter limit */


	/* Parameter limit */


	/* Parameter __MIDL__ICurrentStateSystem0003 */


	/* Parameter __MIDL__ICurrentStateUser0003 */


	/* Parameter __MIDL__ICurrentState0003 */

/* 288 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 290 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 292 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 294 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 296 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 298 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_packageCacheExpirationTimeDays */


	/* Procedure get_packageCacheExpirationTimeDays */


	/* Procedure get_packageCacheExpirationTimeDays */


	/* Procedure get_downloadTimeRemainingMs */


	/* Procedure get_downloadTimeRemainingMs */


	/* Procedure get_downloadTimeRemainingMs */

/* 300 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 302 */	NdrFcLong( 0x0 ),	/* 0 */
/* 306 */	NdrFcShort( 0xb ),	/* 11 */
/* 308 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 310 */	NdrFcShort( 0x0 ),	/* 0 */
/* 312 */	NdrFcShort( 0x24 ),	/* 36 */
/* 314 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 316 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 318 */	NdrFcShort( 0x0 ),	/* 0 */
/* 320 */	NdrFcShort( 0x0 ),	/* 0 */
/* 322 */	NdrFcShort( 0x0 ),	/* 0 */
/* 324 */	NdrFcShort( 0x2 ),	/* 2 */
/* 326 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 328 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter days */


	/* Parameter days */


	/* Parameter days */


	/* Parameter __MIDL__ICurrentStateSystem0004 */


	/* Parameter __MIDL__ICurrentStateUser0004 */


	/* Parameter __MIDL__ICurrentState0004 */

/* 330 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 332 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 334 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 336 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 338 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 340 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nextRetryTime */


	/* Procedure get_nextRetryTime */


	/* Procedure get_nextRetryTime */

/* 342 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 344 */	NdrFcLong( 0x0 ),	/* 0 */
/* 348 */	NdrFcShort( 0xc ),	/* 12 */
/* 350 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 352 */	NdrFcShort( 0x0 ),	/* 0 */
/* 354 */	NdrFcShort( 0x2c ),	/* 44 */
/* 356 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 358 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 360 */	NdrFcShort( 0x0 ),	/* 0 */
/* 362 */	NdrFcShort( 0x0 ),	/* 0 */
/* 364 */	NdrFcShort( 0x0 ),	/* 0 */
/* 366 */	NdrFcShort( 0x2 ),	/* 2 */
/* 368 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 370 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentStateSystem0005 */


	/* Parameter __MIDL__ICurrentStateUser0005 */


	/* Parameter __MIDL__ICurrentState0005 */

/* 372 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 374 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 376 */	0xb,		/* FC_HYPER */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 378 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 380 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 382 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_length */


	/* Procedure get_length */


	/* Procedure get_length */


	/* Procedure get_installProgress */


	/* Procedure get_installProgress */


	/* Procedure get_installProgress */

/* 384 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 386 */	NdrFcLong( 0x0 ),	/* 0 */
/* 390 */	NdrFcShort( 0xd ),	/* 13 */
/* 392 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 394 */	NdrFcShort( 0x0 ),	/* 0 */
/* 396 */	NdrFcShort( 0x24 ),	/* 36 */
/* 398 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 400 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 402 */	NdrFcShort( 0x0 ),	/* 0 */
/* 404 */	NdrFcShort( 0x0 ),	/* 0 */
/* 406 */	NdrFcShort( 0x0 ),	/* 0 */
/* 408 */	NdrFcShort( 0x2 ),	/* 2 */
/* 410 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 412 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter index */


	/* Parameter index */


	/* Parameter index */


	/* Parameter __MIDL__ICurrentStateSystem0006 */


	/* Parameter __MIDL__ICurrentStateUser0006 */


	/* Parameter __MIDL__ICurrentState0006 */

/* 414 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 416 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 418 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 420 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 422 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 424 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installTimeRemainingMs */


	/* Procedure get_installTimeRemainingMs */


	/* Procedure get_installTimeRemainingMs */

/* 426 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 428 */	NdrFcLong( 0x0 ),	/* 0 */
/* 432 */	NdrFcShort( 0xe ),	/* 14 */
/* 434 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 436 */	NdrFcShort( 0x0 ),	/* 0 */
/* 438 */	NdrFcShort( 0x24 ),	/* 36 */
/* 440 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 442 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 444 */	NdrFcShort( 0x0 ),	/* 0 */
/* 446 */	NdrFcShort( 0x0 ),	/* 0 */
/* 448 */	NdrFcShort( 0x0 ),	/* 0 */
/* 450 */	NdrFcShort( 0x2 ),	/* 2 */
/* 452 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 454 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentStateSystem0007 */


	/* Parameter __MIDL__ICurrentStateUser0007 */


	/* Parameter __MIDL__ICurrentState0007 */

/* 456 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 458 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 460 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 462 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 464 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 466 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isCanceled */


	/* Procedure get_isCanceled */


	/* Procedure get_isCanceled */

/* 468 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 470 */	NdrFcLong( 0x0 ),	/* 0 */
/* 474 */	NdrFcShort( 0xf ),	/* 15 */
/* 476 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 478 */	NdrFcShort( 0x0 ),	/* 0 */
/* 480 */	NdrFcShort( 0x22 ),	/* 34 */
/* 482 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 484 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 486 */	NdrFcShort( 0x0 ),	/* 0 */
/* 488 */	NdrFcShort( 0x0 ),	/* 0 */
/* 490 */	NdrFcShort( 0x0 ),	/* 0 */
/* 492 */	NdrFcShort( 0x2 ),	/* 2 */
/* 494 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 496 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter is_canceled */


	/* Parameter is_canceled */


	/* Parameter is_canceled */

/* 498 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 500 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 502 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 504 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 506 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 508 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_errorCode */


	/* Procedure get_errorCode */


	/* Procedure get_errorCode */

/* 510 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 512 */	NdrFcLong( 0x0 ),	/* 0 */
/* 516 */	NdrFcShort( 0x10 ),	/* 16 */
/* 518 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 520 */	NdrFcShort( 0x0 ),	/* 0 */
/* 522 */	NdrFcShort( 0x24 ),	/* 36 */
/* 524 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 526 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 528 */	NdrFcShort( 0x0 ),	/* 0 */
/* 530 */	NdrFcShort( 0x0 ),	/* 0 */
/* 532 */	NdrFcShort( 0x0 ),	/* 0 */
/* 534 */	NdrFcShort( 0x2 ),	/* 2 */
/* 536 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 538 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentStateSystem0008 */


	/* Parameter __MIDL__ICurrentStateUser0008 */


	/* Parameter __MIDL__ICurrentState0008 */

/* 540 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 542 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 544 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 546 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 548 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 550 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_extraCode1 */


	/* Procedure get_extraCode1 */


	/* Procedure get_extraCode1 */

/* 552 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 554 */	NdrFcLong( 0x0 ),	/* 0 */
/* 558 */	NdrFcShort( 0x11 ),	/* 17 */
/* 560 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 562 */	NdrFcShort( 0x0 ),	/* 0 */
/* 564 */	NdrFcShort( 0x24 ),	/* 36 */
/* 566 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 568 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 570 */	NdrFcShort( 0x0 ),	/* 0 */
/* 572 */	NdrFcShort( 0x0 ),	/* 0 */
/* 574 */	NdrFcShort( 0x0 ),	/* 0 */
/* 576 */	NdrFcShort( 0x2 ),	/* 2 */
/* 578 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 580 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentStateSystem0009 */


	/* Parameter __MIDL__ICurrentStateUser0009 */


	/* Parameter __MIDL__ICurrentState0009 */

/* 582 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 584 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 586 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 588 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 590 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 592 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_completionMessage */


	/* Procedure get_completionMessage */


	/* Procedure get_completionMessage */

/* 594 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 596 */	NdrFcLong( 0x0 ),	/* 0 */
/* 600 */	NdrFcShort( 0x12 ),	/* 18 */
/* 602 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 604 */	NdrFcShort( 0x0 ),	/* 0 */
/* 606 */	NdrFcShort( 0x8 ),	/* 8 */
/* 608 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 610 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 612 */	NdrFcShort( 0x1 ),	/* 1 */
/* 614 */	NdrFcShort( 0x0 ),	/* 0 */
/* 616 */	NdrFcShort( 0x0 ),	/* 0 */
/* 618 */	NdrFcShort( 0x2 ),	/* 2 */
/* 620 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 622 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentStateSystem0010 */


	/* Parameter __MIDL__ICurrentStateUser0010 */


	/* Parameter __MIDL__ICurrentState0010 */

/* 624 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 626 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 628 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 630 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 632 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 634 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installerResultCode */


	/* Procedure get_installerResultCode */


	/* Procedure get_installerResultCode */

/* 636 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 638 */	NdrFcLong( 0x0 ),	/* 0 */
/* 642 */	NdrFcShort( 0x13 ),	/* 19 */
/* 644 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 646 */	NdrFcShort( 0x0 ),	/* 0 */
/* 648 */	NdrFcShort( 0x24 ),	/* 36 */
/* 650 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 652 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 654 */	NdrFcShort( 0x0 ),	/* 0 */
/* 656 */	NdrFcShort( 0x0 ),	/* 0 */
/* 658 */	NdrFcShort( 0x0 ),	/* 0 */
/* 660 */	NdrFcShort( 0x2 ),	/* 2 */
/* 662 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 664 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentStateSystem0011 */


	/* Parameter __MIDL__ICurrentStateUser0011 */


	/* Parameter __MIDL__ICurrentState0011 */

/* 666 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 668 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 670 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 672 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 674 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 676 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installerResultExtraCode1 */


	/* Procedure get_installerResultExtraCode1 */


	/* Procedure get_installerResultExtraCode1 */

/* 678 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 680 */	NdrFcLong( 0x0 ),	/* 0 */
/* 684 */	NdrFcShort( 0x14 ),	/* 20 */
/* 686 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 688 */	NdrFcShort( 0x0 ),	/* 0 */
/* 690 */	NdrFcShort( 0x24 ),	/* 36 */
/* 692 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 694 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 696 */	NdrFcShort( 0x0 ),	/* 0 */
/* 698 */	NdrFcShort( 0x0 ),	/* 0 */
/* 700 */	NdrFcShort( 0x0 ),	/* 0 */
/* 702 */	NdrFcShort( 0x2 ),	/* 2 */
/* 704 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 706 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentStateSystem0012 */


	/* Parameter __MIDL__ICurrentStateUser0012 */


	/* Parameter __MIDL__ICurrentState0012 */

/* 708 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 710 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 712 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 714 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 716 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 718 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_postInstallLaunchCommandLine */


	/* Procedure get_postInstallLaunchCommandLine */


	/* Procedure get_postInstallLaunchCommandLine */

/* 720 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 722 */	NdrFcLong( 0x0 ),	/* 0 */
/* 726 */	NdrFcShort( 0x15 ),	/* 21 */
/* 728 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 730 */	NdrFcShort( 0x0 ),	/* 0 */
/* 732 */	NdrFcShort( 0x8 ),	/* 8 */
/* 734 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 736 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 738 */	NdrFcShort( 0x1 ),	/* 1 */
/* 740 */	NdrFcShort( 0x0 ),	/* 0 */
/* 742 */	NdrFcShort( 0x0 ),	/* 0 */
/* 744 */	NdrFcShort( 0x2 ),	/* 2 */
/* 746 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 748 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentStateSystem0013 */


	/* Parameter __MIDL__ICurrentStateUser0013 */


	/* Parameter __MIDL__ICurrentState0013 */

/* 750 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 752 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 754 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 756 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 758 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 760 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_postInstallUrl */


	/* Procedure get_postInstallUrl */


	/* Procedure get_postInstallUrl */

/* 762 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 764 */	NdrFcLong( 0x0 ),	/* 0 */
/* 768 */	NdrFcShort( 0x16 ),	/* 22 */
/* 770 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 772 */	NdrFcShort( 0x0 ),	/* 0 */
/* 774 */	NdrFcShort( 0x8 ),	/* 8 */
/* 776 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 778 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 780 */	NdrFcShort( 0x1 ),	/* 1 */
/* 782 */	NdrFcShort( 0x0 ),	/* 0 */
/* 784 */	NdrFcShort( 0x0 ),	/* 0 */
/* 786 */	NdrFcShort( 0x2 ),	/* 2 */
/* 788 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 790 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentStateSystem0014 */


	/* Parameter __MIDL__ICurrentStateUser0014 */


	/* Parameter __MIDL__ICurrentState0014 */

/* 792 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 794 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 796 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 798 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 800 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 802 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_postInstallAction */


	/* Procedure get_postInstallAction */


	/* Procedure get_postInstallAction */

/* 804 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 806 */	NdrFcLong( 0x0 ),	/* 0 */
/* 810 */	NdrFcShort( 0x17 ),	/* 23 */
/* 812 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 814 */	NdrFcShort( 0x0 ),	/* 0 */
/* 816 */	NdrFcShort( 0x24 ),	/* 36 */
/* 818 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 820 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 822 */	NdrFcShort( 0x0 ),	/* 0 */
/* 824 */	NdrFcShort( 0x0 ),	/* 0 */
/* 826 */	NdrFcShort( 0x0 ),	/* 0 */
/* 828 */	NdrFcShort( 0x2 ),	/* 2 */
/* 830 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 832 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentStateSystem0015 */


	/* Parameter __MIDL__ICurrentStateUser0015 */


	/* Parameter __MIDL__ICurrentState0015 */

/* 834 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 836 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 838 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 840 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 842 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 844 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure createAppBundleWeb */


	/* Procedure createAppBundleWeb */


	/* Procedure createAppBundleWeb */

/* 846 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 848 */	NdrFcLong( 0x0 ),	/* 0 */
/* 852 */	NdrFcShort( 0x7 ),	/* 7 */
/* 854 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 856 */	NdrFcShort( 0x0 ),	/* 0 */
/* 858 */	NdrFcShort( 0x8 ),	/* 8 */
/* 860 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 862 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 864 */	NdrFcShort( 0x0 ),	/* 0 */
/* 866 */	NdrFcShort( 0x0 ),	/* 0 */
/* 868 */	NdrFcShort( 0x0 ),	/* 0 */
/* 870 */	NdrFcShort( 0x2 ),	/* 2 */
/* 872 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 874 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter app_bundle_web */


	/* Parameter app_bundle_web */


	/* Parameter app_bundle_web */

/* 876 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 878 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 880 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 882 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 884 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 886 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure createApp */


	/* Procedure createApp */


	/* Procedure createApp */

/* 888 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 890 */	NdrFcLong( 0x0 ),	/* 0 */
/* 894 */	NdrFcShort( 0x7 ),	/* 7 */
/* 896 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 898 */	NdrFcShort( 0x0 ),	/* 0 */
/* 900 */	NdrFcShort( 0x8 ),	/* 8 */
/* 902 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 904 */	0x10,		/* 16 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 906 */	NdrFcShort( 0x0 ),	/* 0 */
/* 908 */	NdrFcShort( 0x1 ),	/* 1 */
/* 910 */	NdrFcShort( 0x0 ),	/* 0 */
/* 912 */	NdrFcShort( 0x5 ),	/* 5 */
/* 914 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 916 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 918 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter app_guid */


	/* Parameter app_guid */


	/* Parameter app_guid */

/* 920 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 922 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 924 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter brand_code */


	/* Parameter brand_code */


	/* Parameter brand_code */

/* 926 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 928 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 930 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter language */


	/* Parameter language */


	/* Parameter language */

/* 932 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 934 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 936 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter ap */


	/* Parameter ap */


	/* Parameter ap */

/* 938 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 940 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 942 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 944 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 946 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 948 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure createInstalledApp */


	/* Procedure createInstalledApp */


	/* Procedure createInstalledApp */

/* 950 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 952 */	NdrFcLong( 0x0 ),	/* 0 */
/* 956 */	NdrFcShort( 0x8 ),	/* 8 */
/* 958 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 960 */	NdrFcShort( 0x0 ),	/* 0 */
/* 962 */	NdrFcShort( 0x8 ),	/* 8 */
/* 964 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 966 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 968 */	NdrFcShort( 0x0 ),	/* 0 */
/* 970 */	NdrFcShort( 0x1 ),	/* 1 */
/* 972 */	NdrFcShort( 0x0 ),	/* 0 */
/* 974 */	NdrFcShort( 0x2 ),	/* 2 */
/* 976 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 978 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter app_id */


	/* Parameter app_id */


	/* Parameter app_id */

/* 980 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 982 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 984 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 986 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 988 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 990 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure refreshPolicies */


	/* Procedure refreshPolicies */


	/* Procedure refreshPolicies */


	/* Procedure createAllInstalledApps */


	/* Procedure createAllInstalledApps */


	/* Procedure createAllInstalledApps */

/* 992 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 994 */	NdrFcLong( 0x0 ),	/* 0 */
/* 998 */	NdrFcShort( 0x9 ),	/* 9 */
/* 1000 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1002 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1004 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1006 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1008 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1010 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1012 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1014 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1016 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1018 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 1020 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1022 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1024 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_conflictSource */


	/* Procedure get_conflictSource */


	/* Procedure get_conflictSource */


	/* Procedure get_displayLanguage */


	/* Procedure get_displayLanguage */


	/* Procedure get_displayLanguage */

/* 1026 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1028 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1032 */	NdrFcShort( 0xa ),	/* 10 */
/* 1034 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1036 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1038 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1040 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1042 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1044 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1046 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1048 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1050 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1052 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1054 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IPolicyStatusValueSystem0002 */


	/* Parameter __MIDL__IPolicyStatusValueUser0002 */


	/* Parameter __MIDL__IPolicyStatusValue0002 */


	/* Parameter __MIDL__IAppBundleWebSystem0000 */


	/* Parameter __MIDL__IAppBundleWebUser0000 */


	/* Parameter __MIDL__IAppBundleWeb0000 */

/* 1056 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1058 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1060 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 1062 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1064 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1066 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_displayLanguage */


	/* Procedure put_displayLanguage */


	/* Procedure put_displayLanguage */

/* 1068 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1070 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1074 */	NdrFcShort( 0xb ),	/* 11 */
/* 1076 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1078 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1080 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1082 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1084 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1086 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1088 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1090 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1092 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1094 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1096 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IAppBundleWebSystem0001 */


	/* Parameter __MIDL__IAppBundleWebUser0001 */


	/* Parameter __MIDL__IAppBundleWeb0001 */

/* 1098 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1100 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1102 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1104 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1106 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1108 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_parentHWND */


	/* Procedure put_parentHWND */


	/* Procedure put_parentHWND */

/* 1110 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1112 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1116 */	NdrFcShort( 0xc ),	/* 12 */
/* 1118 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1120 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1122 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1124 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 1126 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1128 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1130 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1132 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1134 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1136 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1138 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter hwnd */


	/* Parameter hwnd */


	/* Parameter hwnd */

/* 1140 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1142 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1144 */	0xb9,		/* FC_UINT3264 */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1146 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1148 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1150 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_appWeb */


	/* Procedure get_appWeb */


	/* Procedure get_appWeb */

/* 1152 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1154 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1158 */	NdrFcShort( 0xe ),	/* 14 */
/* 1160 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1162 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1164 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1166 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 1168 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1170 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1172 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1174 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1176 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1178 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1180 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter index */


	/* Parameter index */


	/* Parameter index */

/* 1182 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1184 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1186 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter app_web */


	/* Parameter app_web */


	/* Parameter app_web */

/* 1188 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1190 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1192 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1194 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1196 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1198 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure initialize */


	/* Procedure initialize */


	/* Procedure initialize */

/* 1200 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1202 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1206 */	NdrFcShort( 0xf ),	/* 15 */
/* 1208 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1210 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1212 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1214 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1216 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1218 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1220 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1222 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1224 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1226 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1228 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1230 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1232 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure checkForUpdate */


	/* Procedure checkForUpdate */


	/* Procedure checkForUpdate */

/* 1234 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1236 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1240 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1242 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1244 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1246 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1248 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1250 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1252 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1254 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1256 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1258 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1260 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1262 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1264 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1266 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure download */


	/* Procedure download */


	/* Procedure download */

/* 1268 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1270 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1274 */	NdrFcShort( 0x11 ),	/* 17 */
/* 1276 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1278 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1280 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1282 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1284 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1286 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1288 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1290 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1292 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1294 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1296 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1298 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1300 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure install */


	/* Procedure install */


	/* Procedure install */

/* 1302 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1304 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1308 */	NdrFcShort( 0x12 ),	/* 18 */
/* 1310 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1312 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1314 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1316 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1318 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1320 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1322 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1324 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1326 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1328 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1330 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1332 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1334 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure pause */


	/* Procedure pause */


	/* Procedure pause */

/* 1336 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1338 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1342 */	NdrFcShort( 0x13 ),	/* 19 */
/* 1344 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1346 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1348 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1350 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1352 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1354 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1356 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1358 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1360 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1362 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1364 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1366 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1368 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure resume */


	/* Procedure resume */


	/* Procedure resume */

/* 1370 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1372 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1376 */	NdrFcShort( 0x14 ),	/* 20 */
/* 1378 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1380 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1382 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1384 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1386 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1388 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1390 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1392 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1394 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1396 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1398 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1400 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1402 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure cancel */


	/* Procedure cancel */


	/* Procedure cancel */

/* 1404 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1406 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1410 */	NdrFcShort( 0x15 ),	/* 21 */
/* 1412 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1414 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1416 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1418 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1420 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1422 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1424 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1426 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1428 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1430 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1432 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1434 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1436 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure downloadPackage */


	/* Procedure downloadPackage */


	/* Procedure downloadPackage */

/* 1438 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1440 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1444 */	NdrFcShort( 0x16 ),	/* 22 */
/* 1446 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1448 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1450 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1452 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 1454 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1456 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1458 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1460 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1462 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1464 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1466 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */


	/* Parameter app_id */


	/* Parameter app_id */

/* 1468 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1470 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1472 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter package_name */


	/* Parameter package_name */


	/* Parameter package_name */

/* 1474 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1476 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1478 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1480 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1482 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1484 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_currentState */


	/* Procedure get_currentState */


	/* Procedure get_currentState */

/* 1486 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1488 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1492 */	NdrFcShort( 0x17 ),	/* 23 */
/* 1494 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1496 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1498 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1500 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1502 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1504 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1506 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1508 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1510 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1512 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1514 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter current_state */


	/* Parameter current_state */


	/* Parameter current_state */

/* 1516 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1518 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1520 */	NdrFcShort( 0x3fe ),	/* Type Offset=1022 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1522 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1524 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1526 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_currentVersionWeb */


	/* Procedure get_currentVersionWeb */


	/* Procedure get_currentVersionWeb */

/* 1528 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1530 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1534 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1536 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1538 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1540 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1542 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1544 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1546 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1548 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1550 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1552 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1554 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1556 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter current */


	/* Parameter current */


	/* Parameter current */

/* 1558 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1560 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1562 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1564 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1566 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1568 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nextVersionWeb */


	/* Procedure get_nextVersionWeb */


	/* Procedure get_nextVersionWeb */

/* 1570 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1572 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1576 */	NdrFcShort( 0x9 ),	/* 9 */
/* 1578 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1580 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1582 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1584 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1586 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1588 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1590 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1592 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1594 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1596 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1598 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter next */


	/* Parameter next */


	/* Parameter next */

/* 1600 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1602 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1604 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1606 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1608 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1610 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_command */


	/* Procedure get_command */


	/* Procedure get_command */

/* 1612 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1614 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1618 */	NdrFcShort( 0xa ),	/* 10 */
/* 1620 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1622 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1624 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1626 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 1628 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1630 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1632 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1634 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1636 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1638 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1640 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter command_id */


	/* Parameter command_id */


	/* Parameter command_id */

/* 1642 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1644 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1646 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter command */


	/* Parameter command */


	/* Parameter command */

/* 1648 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1650 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1652 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1654 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1656 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1658 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure cancel */


	/* Procedure cancel */


	/* Procedure cancel */

/* 1660 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1662 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1666 */	NdrFcShort( 0xb ),	/* 11 */
/* 1668 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1670 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1672 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1674 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1676 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1678 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1680 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1682 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1684 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1686 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1688 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1690 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1692 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_currentState */


	/* Procedure get_currentState */


	/* Procedure get_currentState */

/* 1694 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1696 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1700 */	NdrFcShort( 0xc ),	/* 12 */
/* 1702 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1704 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1706 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1708 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1710 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1712 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1714 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1716 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1718 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1720 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1722 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter current_state */


	/* Parameter current_state */


	/* Parameter current_state */

/* 1724 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1726 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1728 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1730 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1732 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1734 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure launch */


	/* Procedure launch */


	/* Procedure launch */

/* 1736 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1738 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1742 */	NdrFcShort( 0xd ),	/* 13 */
/* 1744 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1746 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1748 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1750 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1752 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1754 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1756 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1758 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1760 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1762 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1764 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1766 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1768 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure uninstall */


	/* Procedure uninstall */


	/* Procedure uninstall */

/* 1770 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1772 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1776 */	NdrFcShort( 0xe ),	/* 14 */
/* 1778 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1780 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1782 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1784 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1786 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1788 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1790 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1792 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1794 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1796 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1798 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1800 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1802 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_serverInstallDataIndex */


	/* Procedure get_serverInstallDataIndex */


	/* Procedure get_serverInstallDataIndex */

/* 1804 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1806 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1810 */	NdrFcShort( 0xf ),	/* 15 */
/* 1812 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1814 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1816 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1818 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1820 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1822 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1824 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1826 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1828 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1830 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1832 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IAppWebSystem0001 */


	/* Parameter __MIDL__IAppWebUser0001 */


	/* Parameter __MIDL__IAppWeb0001 */

/* 1834 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1836 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1838 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1840 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1842 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1844 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_serverInstallDataIndex */


	/* Procedure put_serverInstallDataIndex */


	/* Procedure put_serverInstallDataIndex */

/* 1846 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1848 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1852 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1854 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1856 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1858 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1860 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1862 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1864 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1866 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1868 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1870 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1872 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1874 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IAppWebSystem0002 */


	/* Parameter __MIDL__IAppWebUser0002 */


	/* Parameter __MIDL__IAppWeb0002 */

/* 1876 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1878 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1880 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 1882 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1884 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1886 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_downloadPreferenceGroupPolicy */


	/* Procedure get_downloadPreferenceGroupPolicy */


	/* Procedure get_downloadPreferenceGroupPolicy */


	/* Procedure get_output */


	/* Procedure get_output */


	/* Procedure get_output */

/* 1888 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1890 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1894 */	NdrFcShort( 0x9 ),	/* 9 */
/* 1896 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1898 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1900 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1902 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1904 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1906 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1908 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1910 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1912 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1914 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1916 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter pref */


	/* Parameter pref */


	/* Parameter pref */


	/* Parameter __MIDL__IAppCommandWebSystem0002 */


	/* Parameter __MIDL__IAppCommandWebUser0002 */


	/* Parameter __MIDL__IAppCommandWeb0002 */

/* 1918 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1920 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1922 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 1924 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1926 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1928 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure execute */


	/* Procedure execute */


	/* Procedure execute */

/* 1930 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1932 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1936 */	NdrFcShort( 0xa ),	/* 10 */
/* 1938 */	NdrFcShort( 0x58 ),	/* ARM64 Stack size/offset = 88 */
/* 1940 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1942 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1944 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0xa,		/* 10 */
/* 1946 */	0x16,		/* 22 */
			0x85,		/* Ext Flags:  new corr desc, srv corr check, has big byval param */
/* 1948 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1950 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1952 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1954 */	NdrFcShort( 0xa ),	/* 10 */
/* 1956 */	0xa,		/* 10 */
			0x80,		/* 128 */
/* 1958 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1960 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1962 */	0x85,		/* 133 */
			0x86,		/* 134 */
/* 1964 */	0x87,		/* 135 */
			0xf8,		/* 248 */
/* 1966 */	0xf8,		/* 248 */
			0x0,		/* 0 */

	/* Parameter substitution1 */


	/* Parameter substitution1 */


	/* Parameter substitution1 */

/* 1968 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1970 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1972 */	NdrFcShort( 0x410 ),	/* Type Offset=1040 */

	/* Parameter substitution2 */


	/* Parameter substitution2 */


	/* Parameter substitution2 */

/* 1974 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1976 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1978 */	NdrFcShort( 0x410 ),	/* Type Offset=1040 */

	/* Parameter substitution3 */


	/* Parameter substitution3 */


	/* Parameter substitution3 */

/* 1980 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1982 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1984 */	NdrFcShort( 0x410 ),	/* Type Offset=1040 */

	/* Parameter substitution4 */


	/* Parameter substitution4 */


	/* Parameter substitution4 */

/* 1986 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1988 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1990 */	NdrFcShort( 0x410 ),	/* Type Offset=1040 */

	/* Parameter substitution5 */


	/* Parameter substitution5 */


	/* Parameter substitution5 */

/* 1992 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1994 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1996 */	NdrFcShort( 0x410 ),	/* Type Offset=1040 */

	/* Parameter substitution6 */


	/* Parameter substitution6 */


	/* Parameter substitution6 */

/* 1998 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2000 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2002 */	NdrFcShort( 0x410 ),	/* Type Offset=1040 */

	/* Parameter substitution7 */


	/* Parameter substitution7 */


	/* Parameter substitution7 */

/* 2004 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2006 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2008 */	NdrFcShort( 0x410 ),	/* Type Offset=1040 */

	/* Parameter substitution8 */


	/* Parameter substitution8 */


	/* Parameter substitution8 */

/* 2010 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2012 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 2014 */	NdrFcShort( 0x410 ),	/* Type Offset=1040 */

	/* Parameter substitution9 */


	/* Parameter substitution9 */


	/* Parameter substitution9 */

/* 2016 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2018 */	NdrFcShort( 0x48 ),	/* ARM64 Stack size/offset = 72 */
/* 2020 */	NdrFcShort( 0x410 ),	/* Type Offset=1040 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 2022 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2024 */	NdrFcShort( 0x50 ),	/* ARM64 Stack size/offset = 80 */
/* 2026 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_updatesSuppressedTimes */


	/* Procedure get_updatesSuppressedTimes */


	/* Procedure get_updatesSuppressedTimes */

/* 2028 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2030 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2034 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2036 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2038 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2040 */	NdrFcShort( 0x76 ),	/* 118 */
/* 2042 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x5,		/* 5 */
/* 2044 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2046 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2048 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2050 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2052 */	NdrFcShort( 0x5 ),	/* 5 */
/* 2054 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 2056 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2058 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter start_hour */


	/* Parameter start_hour */


	/* Parameter start_hour */

/* 2060 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2062 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2064 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter start_min */


	/* Parameter start_min */


	/* Parameter start_min */

/* 2066 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2068 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2070 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter duration_min */


	/* Parameter duration_min */


	/* Parameter duration_min */

/* 2072 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2074 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2076 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter are_updates_suppressed */


	/* Parameter are_updates_suppressed */


	/* Parameter are_updates_suppressed */

/* 2078 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2080 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2082 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 2084 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2086 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2088 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_effectivePolicyForAppInstalls */


	/* Procedure get_effectivePolicyForAppInstalls */


	/* Procedure get_effectivePolicyForAppInstalls */

/* 2090 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2092 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2096 */	NdrFcShort( 0xc ),	/* 12 */
/* 2098 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2100 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2102 */	NdrFcShort( 0x24 ),	/* 36 */
/* 2104 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 2106 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2108 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2110 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2112 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2114 */	NdrFcShort( 0x3 ),	/* 3 */
/* 2116 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 2118 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */


	/* Parameter app_id */


	/* Parameter app_id */

/* 2120 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2122 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2124 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter policy */


	/* Parameter policy */


	/* Parameter policy */

/* 2126 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2128 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2130 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 2132 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2134 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2136 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_effectivePolicyForAppUpdates */


	/* Procedure get_effectivePolicyForAppUpdates */


	/* Procedure get_effectivePolicyForAppUpdates */

/* 2138 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2140 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2144 */	NdrFcShort( 0xd ),	/* 13 */
/* 2146 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2148 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2150 */	NdrFcShort( 0x24 ),	/* 36 */
/* 2152 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 2154 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2156 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2158 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2160 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2162 */	NdrFcShort( 0x3 ),	/* 3 */
/* 2164 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 2166 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */


	/* Parameter app_id */


	/* Parameter app_id */

/* 2168 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2170 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2172 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter policy */


	/* Parameter policy */


	/* Parameter policy */

/* 2174 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2176 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2178 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 2180 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2182 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2184 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_targetVersionPrefix */


	/* Procedure get_targetVersionPrefix */


	/* Procedure get_targetVersionPrefix */

/* 2186 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2188 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2192 */	NdrFcShort( 0xe ),	/* 14 */
/* 2194 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2196 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2198 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2200 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 2202 */	0xe,		/* 14 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 2204 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2206 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2208 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2210 */	NdrFcShort( 0x3 ),	/* 3 */
/* 2212 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 2214 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */


	/* Parameter app_id */


	/* Parameter app_id */

/* 2216 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2218 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2220 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter prefix */


	/* Parameter prefix */


	/* Parameter prefix */

/* 2222 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2224 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2226 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 2228 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2230 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2232 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isRollbackToTargetVersionAllowed */


	/* Procedure get_isRollbackToTargetVersionAllowed */


	/* Procedure get_isRollbackToTargetVersionAllowed */

/* 2234 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2236 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2240 */	NdrFcShort( 0xf ),	/* 15 */
/* 2242 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2244 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2246 */	NdrFcShort( 0x22 ),	/* 34 */
/* 2248 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 2250 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2252 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2254 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2256 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2258 */	NdrFcShort( 0x3 ),	/* 3 */
/* 2260 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 2262 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */


	/* Parameter app_id */


	/* Parameter app_id */

/* 2264 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2266 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2268 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter rollback_allowed */


	/* Parameter rollback_allowed */


	/* Parameter rollback_allowed */

/* 2270 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2272 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2274 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 2276 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2278 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2280 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_hasConflict */


	/* Procedure get_hasConflict */


	/* Procedure get_hasConflict */

/* 2282 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2284 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2288 */	NdrFcShort( 0x9 ),	/* 9 */
/* 2290 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2292 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2294 */	NdrFcShort( 0x22 ),	/* 34 */
/* 2296 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2298 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2300 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2302 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2304 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2306 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2308 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2310 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter has_conflict */


	/* Parameter has_conflict */


	/* Parameter has_conflict */

/* 2312 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2314 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2316 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 2318 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2320 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2322 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_conflictValue */


	/* Procedure get_conflictValue */


	/* Procedure get_conflictValue */

/* 2324 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2326 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2330 */	NdrFcShort( 0xb ),	/* 11 */
/* 2332 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2334 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2336 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2338 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2340 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2342 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2344 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2346 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2348 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2350 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2352 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IPolicyStatusValueSystem0003 */


	/* Parameter __MIDL__IPolicyStatusValueUser0003 */


	/* Parameter __MIDL__IPolicyStatusValue0003 */

/* 2354 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2356 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2358 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 2360 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2362 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2364 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_lastCheckedTime */


	/* Procedure get_lastCheckedTime */


	/* Procedure get_lastCheckedTime */

/* 2366 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2368 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2372 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2374 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2376 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2378 */	NdrFcShort( 0x2c ),	/* 44 */
/* 2380 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2382 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2384 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2386 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2388 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2390 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2392 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2394 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter last_checked */


	/* Parameter last_checked */


	/* Parameter last_checked */

/* 2396 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2398 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2400 */	0xc,		/* FC_DOUBLE */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 2402 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2404 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2406 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_lastCheckPeriodMinutes */

/* 2408 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2410 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2414 */	NdrFcShort( 0xa ),	/* 10 */
/* 2416 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2418 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2420 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2422 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2424 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2426 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2428 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2430 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2432 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2434 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2436 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 2438 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 2440 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2442 */	NdrFcShort( 0x41e ),	/* Type Offset=1054 */

	/* Return value */

/* 2444 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2446 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2448 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_updatesSuppressedTimes */

/* 2450 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2452 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2456 */	NdrFcShort( 0xb ),	/* 11 */
/* 2458 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2460 */	NdrFcShort( 0x1a ),	/* 26 */
/* 2462 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2464 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 2466 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2468 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2470 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2472 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2474 */	NdrFcShort( 0x3 ),	/* 3 */
/* 2476 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 2478 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter value */

/* 2480 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 2482 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2484 */	NdrFcShort( 0x41e ),	/* Type Offset=1054 */

	/* Parameter are_updates_suppressed */

/* 2486 */	NdrFcShort( 0x148 ),	/* Flags:  in, base type, simple ref, */
/* 2488 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2490 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 2492 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2494 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2496 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_downloadPreferenceGroupPolicy */

/* 2498 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2500 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2504 */	NdrFcShort( 0xc ),	/* 12 */
/* 2506 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2508 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2510 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2512 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2514 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2516 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2518 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2520 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2522 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2524 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2526 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 2528 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 2530 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2532 */	NdrFcShort( 0x41e ),	/* Type Offset=1054 */

	/* Return value */

/* 2534 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2536 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2538 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_packageCacheSizeLimitMBytes */

/* 2540 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2542 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2546 */	NdrFcShort( 0xd ),	/* 13 */
/* 2548 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2550 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2552 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2554 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2556 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2558 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2560 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2562 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2564 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2566 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2568 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 2570 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 2572 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2574 */	NdrFcShort( 0x41e ),	/* Type Offset=1054 */

	/* Return value */

/* 2576 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2578 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2580 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_packageCacheExpirationTimeDays */

/* 2582 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2584 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2588 */	NdrFcShort( 0xe ),	/* 14 */
/* 2590 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2592 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2594 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2596 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2598 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2600 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2602 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2604 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2606 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2608 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2610 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 2612 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 2614 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2616 */	NdrFcShort( 0x41e ),	/* Type Offset=1054 */

	/* Return value */

/* 2618 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2620 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2622 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_proxyMode */

/* 2624 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2626 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2630 */	NdrFcShort( 0xf ),	/* 15 */
/* 2632 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2634 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2636 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2638 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2640 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2642 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2644 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2646 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2648 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2650 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2652 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 2654 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 2656 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2658 */	NdrFcShort( 0x41e ),	/* Type Offset=1054 */

	/* Return value */

/* 2660 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2662 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2664 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_proxyPacUrl */

/* 2666 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2668 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2672 */	NdrFcShort( 0x10 ),	/* 16 */
/* 2674 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2676 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2678 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2680 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2682 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2684 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2686 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2688 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2690 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2692 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2694 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 2696 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 2698 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2700 */	NdrFcShort( 0x41e ),	/* Type Offset=1054 */

	/* Return value */

/* 2702 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2704 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2706 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_proxyServer */

/* 2708 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2710 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2714 */	NdrFcShort( 0x11 ),	/* 17 */
/* 2716 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2718 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2720 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2722 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2724 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2726 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2728 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2730 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2732 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2734 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2736 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 2738 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 2740 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2742 */	NdrFcShort( 0x41e ),	/* Type Offset=1054 */

	/* Return value */

/* 2744 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2746 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2748 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_effectivePolicyForAppInstalls */

/* 2750 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2752 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2756 */	NdrFcShort( 0x12 ),	/* 18 */
/* 2758 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2760 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2762 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2764 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 2766 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2768 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2770 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2772 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2774 */	NdrFcShort( 0x3 ),	/* 3 */
/* 2776 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 2778 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 2780 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2782 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2784 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter value */

/* 2786 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 2788 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2790 */	NdrFcShort( 0x41e ),	/* Type Offset=1054 */

	/* Return value */

/* 2792 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2794 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2796 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_effectivePolicyForAppUpdates */

/* 2798 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2800 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2804 */	NdrFcShort( 0x13 ),	/* 19 */
/* 2806 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2808 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2810 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2812 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 2814 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2816 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2818 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2820 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2822 */	NdrFcShort( 0x3 ),	/* 3 */
/* 2824 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 2826 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 2828 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2830 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2832 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter value */

/* 2834 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 2836 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2838 */	NdrFcShort( 0x41e ),	/* Type Offset=1054 */

	/* Return value */

/* 2840 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2842 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2844 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_targetVersionPrefix */

/* 2846 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2848 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2852 */	NdrFcShort( 0x14 ),	/* 20 */
/* 2854 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2856 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2858 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2860 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 2862 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2864 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2866 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2868 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2870 */	NdrFcShort( 0x3 ),	/* 3 */
/* 2872 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 2874 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 2876 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2878 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2880 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter value */

/* 2882 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 2884 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2886 */	NdrFcShort( 0x41e ),	/* Type Offset=1054 */

	/* Return value */

/* 2888 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2890 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2892 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isRollbackToTargetVersionAllowed */

/* 2894 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2896 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2900 */	NdrFcShort( 0x15 ),	/* 21 */
/* 2902 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2904 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2906 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2908 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 2910 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2912 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2914 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2916 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2918 */	NdrFcShort( 0x3 ),	/* 3 */
/* 2920 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 2922 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 2924 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2926 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2928 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter value */

/* 2930 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 2932 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2934 */	NdrFcShort( 0x41e ),	/* Type Offset=1054 */

	/* Return value */

/* 2936 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2938 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2940 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_targetChannel */

/* 2942 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2944 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2948 */	NdrFcShort( 0x16 ),	/* 22 */
/* 2950 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2952 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2954 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2956 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 2958 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2960 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2962 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2964 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2966 */	NdrFcShort( 0x3 ),	/* 3 */
/* 2968 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 2970 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 2972 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2974 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2976 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter value */

/* 2978 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 2980 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2982 */	NdrFcShort( 0x41e ),	/* Type Offset=1054 */

	/* Return value */

/* 2984 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2986 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2988 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_lastCheckPeriodMinutes */

/* 2990 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2992 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2996 */	NdrFcShort( 0xa ),	/* 10 */
/* 2998 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3000 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3002 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3004 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3006 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3008 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3010 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3012 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3014 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3016 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3018 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 3020 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3022 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3024 */	NdrFcShort( 0x438 ),	/* Type Offset=1080 */

	/* Return value */

/* 3026 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3028 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3030 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_updatesSuppressedTimes */

/* 3032 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3034 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3038 */	NdrFcShort( 0xb ),	/* 11 */
/* 3040 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3042 */	NdrFcShort( 0x1a ),	/* 26 */
/* 3044 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3046 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3048 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3050 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3052 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3054 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3056 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3058 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3060 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter value */

/* 3062 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3064 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3066 */	NdrFcShort( 0x438 ),	/* Type Offset=1080 */

	/* Parameter are_updates_suppressed */

/* 3068 */	NdrFcShort( 0x148 ),	/* Flags:  in, base type, simple ref, */
/* 3070 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3072 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 3074 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3076 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3078 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_downloadPreferenceGroupPolicy */

/* 3080 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3082 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3086 */	NdrFcShort( 0xc ),	/* 12 */
/* 3088 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3090 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3092 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3094 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3096 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3098 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3100 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3102 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3104 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3106 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3108 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 3110 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3112 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3114 */	NdrFcShort( 0x438 ),	/* Type Offset=1080 */

	/* Return value */

/* 3116 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3118 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3120 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_packageCacheSizeLimitMBytes */

/* 3122 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3124 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3128 */	NdrFcShort( 0xd ),	/* 13 */
/* 3130 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3132 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3134 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3136 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3138 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3140 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3142 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3144 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3146 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3148 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3150 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 3152 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3154 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3156 */	NdrFcShort( 0x438 ),	/* Type Offset=1080 */

	/* Return value */

/* 3158 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3160 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3162 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_packageCacheExpirationTimeDays */

/* 3164 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3166 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3170 */	NdrFcShort( 0xe ),	/* 14 */
/* 3172 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3174 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3176 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3178 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3180 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3182 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3184 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3186 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3188 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3190 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3192 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 3194 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3196 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3198 */	NdrFcShort( 0x438 ),	/* Type Offset=1080 */

	/* Return value */

/* 3200 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3202 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3204 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_proxyMode */

/* 3206 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3208 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3212 */	NdrFcShort( 0xf ),	/* 15 */
/* 3214 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3216 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3218 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3220 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3222 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3224 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3226 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3228 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3230 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3232 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3234 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 3236 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3238 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3240 */	NdrFcShort( 0x438 ),	/* Type Offset=1080 */

	/* Return value */

/* 3242 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3244 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3246 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_proxyPacUrl */

/* 3248 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3250 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3254 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3256 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3258 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3260 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3262 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3264 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3266 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3268 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3270 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3272 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3274 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3276 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 3278 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3280 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3282 */	NdrFcShort( 0x438 ),	/* Type Offset=1080 */

	/* Return value */

/* 3284 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3286 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3288 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_proxyServer */

/* 3290 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3292 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3296 */	NdrFcShort( 0x11 ),	/* 17 */
/* 3298 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3300 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3302 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3304 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3306 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3308 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3310 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3312 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3314 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3316 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3318 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 3320 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3322 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3324 */	NdrFcShort( 0x438 ),	/* Type Offset=1080 */

	/* Return value */

/* 3326 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3328 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3330 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_effectivePolicyForAppInstalls */

/* 3332 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3334 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3338 */	NdrFcShort( 0x12 ),	/* 18 */
/* 3340 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3342 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3344 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3346 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 3348 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 3350 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3352 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3354 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3356 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3358 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3360 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 3362 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 3364 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3366 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter value */

/* 3368 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3370 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3372 */	NdrFcShort( 0x438 ),	/* Type Offset=1080 */

	/* Return value */

/* 3374 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3376 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3378 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_effectivePolicyForAppUpdates */

/* 3380 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3382 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3386 */	NdrFcShort( 0x13 ),	/* 19 */
/* 3388 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3390 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3392 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3394 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 3396 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 3398 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3400 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3402 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3404 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3406 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3408 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 3410 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 3412 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3414 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter value */

/* 3416 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3418 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3420 */	NdrFcShort( 0x438 ),	/* Type Offset=1080 */

	/* Return value */

/* 3422 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3424 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3426 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_targetVersionPrefix */

/* 3428 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3430 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3434 */	NdrFcShort( 0x14 ),	/* 20 */
/* 3436 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3438 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3440 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3442 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 3444 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 3446 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3448 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3450 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3452 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3454 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3456 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 3458 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 3460 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3462 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter value */

/* 3464 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3466 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3468 */	NdrFcShort( 0x438 ),	/* Type Offset=1080 */

	/* Return value */

/* 3470 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3472 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3474 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isRollbackToTargetVersionAllowed */

/* 3476 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3478 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3482 */	NdrFcShort( 0x15 ),	/* 21 */
/* 3484 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3486 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3488 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3490 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 3492 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 3494 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3496 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3498 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3500 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3502 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3504 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 3506 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 3508 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3510 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter value */

/* 3512 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3514 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3516 */	NdrFcShort( 0x438 ),	/* Type Offset=1080 */

	/* Return value */

/* 3518 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3520 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3522 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_targetChannel */

/* 3524 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3526 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3530 */	NdrFcShort( 0x16 ),	/* 22 */
/* 3532 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3534 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3536 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3538 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 3540 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 3542 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3544 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3546 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3548 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3550 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3552 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 3554 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 3556 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3558 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter value */

/* 3560 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3562 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3564 */	NdrFcShort( 0x438 ),	/* Type Offset=1080 */

	/* Return value */

/* 3566 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3568 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3570 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_lastCheckPeriodMinutes */

/* 3572 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3574 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3578 */	NdrFcShort( 0xa ),	/* 10 */
/* 3580 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3582 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3584 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3586 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3588 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3590 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3592 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3594 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3596 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3598 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3600 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 3602 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3604 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3606 */	NdrFcShort( 0x44e ),	/* Type Offset=1102 */

	/* Return value */

/* 3608 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3610 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3612 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_updatesSuppressedTimes */

/* 3614 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3616 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3620 */	NdrFcShort( 0xb ),	/* 11 */
/* 3622 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3624 */	NdrFcShort( 0x1a ),	/* 26 */
/* 3626 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3628 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3630 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3632 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3634 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3636 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3638 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3640 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3642 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter value */

/* 3644 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3646 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3648 */	NdrFcShort( 0x44e ),	/* Type Offset=1102 */

	/* Parameter are_updates_suppressed */

/* 3650 */	NdrFcShort( 0x148 ),	/* Flags:  in, base type, simple ref, */
/* 3652 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3654 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 3656 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3658 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3660 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_downloadPreferenceGroupPolicy */

/* 3662 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3664 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3668 */	NdrFcShort( 0xc ),	/* 12 */
/* 3670 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3672 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3674 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3676 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3678 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3680 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3682 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3684 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3686 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3688 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3690 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 3692 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3694 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3696 */	NdrFcShort( 0x44e ),	/* Type Offset=1102 */

	/* Return value */

/* 3698 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3700 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3702 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_packageCacheSizeLimitMBytes */

/* 3704 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3706 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3710 */	NdrFcShort( 0xd ),	/* 13 */
/* 3712 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3714 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3716 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3718 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3720 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3722 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3724 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3726 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3728 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3730 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3732 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 3734 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3736 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3738 */	NdrFcShort( 0x44e ),	/* Type Offset=1102 */

	/* Return value */

/* 3740 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3742 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3744 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_packageCacheExpirationTimeDays */

/* 3746 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3748 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3752 */	NdrFcShort( 0xe ),	/* 14 */
/* 3754 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3756 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3758 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3760 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3762 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3764 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3766 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3768 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3770 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3772 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3774 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 3776 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3778 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3780 */	NdrFcShort( 0x44e ),	/* Type Offset=1102 */

	/* Return value */

/* 3782 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3784 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3786 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_proxyMode */

/* 3788 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3790 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3794 */	NdrFcShort( 0xf ),	/* 15 */
/* 3796 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3798 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3800 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3802 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3804 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3806 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3808 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3810 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3812 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3814 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3816 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 3818 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3820 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3822 */	NdrFcShort( 0x44e ),	/* Type Offset=1102 */

	/* Return value */

/* 3824 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3826 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3828 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_proxyPacUrl */

/* 3830 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3832 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3836 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3838 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3840 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3842 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3844 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3846 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3848 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3850 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3852 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3854 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3856 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3858 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 3860 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3862 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3864 */	NdrFcShort( 0x44e ),	/* Type Offset=1102 */

	/* Return value */

/* 3866 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3868 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3870 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_proxyServer */

/* 3872 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3874 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3878 */	NdrFcShort( 0x11 ),	/* 17 */
/* 3880 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3882 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3884 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3886 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3888 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3890 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3892 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3894 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3896 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3898 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3900 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 3902 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3904 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3906 */	NdrFcShort( 0x44e ),	/* Type Offset=1102 */

	/* Return value */

/* 3908 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3910 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3912 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_effectivePolicyForAppInstalls */

/* 3914 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3916 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3920 */	NdrFcShort( 0x12 ),	/* 18 */
/* 3922 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3924 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3926 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3928 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 3930 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 3932 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3934 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3936 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3938 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3940 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3942 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 3944 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 3946 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3948 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter value */

/* 3950 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3952 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3954 */	NdrFcShort( 0x44e ),	/* Type Offset=1102 */

	/* Return value */

/* 3956 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3958 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3960 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_effectivePolicyForAppUpdates */

/* 3962 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3964 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3968 */	NdrFcShort( 0x13 ),	/* 19 */
/* 3970 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3972 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3974 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3976 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 3978 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 3980 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3982 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3984 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3986 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3988 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3990 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 3992 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 3994 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3996 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter value */

/* 3998 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4000 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4002 */	NdrFcShort( 0x44e ),	/* Type Offset=1102 */

	/* Return value */

/* 4004 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4006 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4008 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_targetVersionPrefix */

/* 4010 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4012 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4016 */	NdrFcShort( 0x14 ),	/* 20 */
/* 4018 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4020 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4022 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4024 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 4026 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 4028 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4030 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4032 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4034 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4036 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4038 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 4040 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4042 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4044 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter value */

/* 4046 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4048 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4050 */	NdrFcShort( 0x44e ),	/* Type Offset=1102 */

	/* Return value */

/* 4052 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4054 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4056 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isRollbackToTargetVersionAllowed */

/* 4058 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4060 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4064 */	NdrFcShort( 0x15 ),	/* 21 */
/* 4066 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4068 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4070 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4072 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 4074 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 4076 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4078 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4080 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4082 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4084 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4086 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 4088 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4090 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4092 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter value */

/* 4094 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4096 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4098 */	NdrFcShort( 0x44e ),	/* Type Offset=1102 */

	/* Return value */

/* 4100 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4102 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4104 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_targetChannel */

/* 4106 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4108 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4112 */	NdrFcShort( 0x16 ),	/* 22 */
/* 4114 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4116 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4118 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4120 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 4122 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 4124 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4126 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4128 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4130 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4132 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4134 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 4136 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4138 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4140 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter value */

/* 4142 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4144 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4146 */	NdrFcShort( 0x44e ),	/* Type Offset=1102 */

	/* Return value */

/* 4148 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4150 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4152 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_forceInstallApps */

/* 4154 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4156 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4160 */	NdrFcShort( 0x17 ),	/* 23 */
/* 4162 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4164 */	NdrFcShort( 0x6 ),	/* 6 */
/* 4166 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4168 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 4170 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4172 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4174 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4176 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4178 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4180 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4182 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter is_machine */

/* 4184 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4186 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4188 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Parameter value */

/* 4190 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4192 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4194 */	NdrFcShort( 0x464 ),	/* Type Offset=1124 */

	/* Return value */

/* 4196 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4198 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4200 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_forceInstallApps */

/* 4202 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4204 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4208 */	NdrFcShort( 0x17 ),	/* 23 */
/* 4210 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4212 */	NdrFcShort( 0x6 ),	/* 6 */
/* 4214 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4216 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 4218 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4220 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4222 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4224 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4226 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4228 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4230 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter is_machine */

/* 4232 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4234 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4236 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Parameter value */

/* 4238 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4240 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4242 */	NdrFcShort( 0x438 ),	/* Type Offset=1080 */

	/* Return value */

/* 4244 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4246 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4248 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_forceInstallApps */

/* 4250 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4252 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4256 */	NdrFcShort( 0x17 ),	/* 23 */
/* 4258 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4260 */	NdrFcShort( 0x6 ),	/* 6 */
/* 4262 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4264 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 4266 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4268 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4270 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4272 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4274 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4276 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4278 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter is_machine */

/* 4280 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4282 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4284 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Parameter value */

/* 4286 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4288 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4290 */	NdrFcShort( 0x44e ),	/* Type Offset=1102 */

	/* Return value */

/* 4292 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4294 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4296 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_cloudPolicyOverridesPlatformPolicy */

/* 4298 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4300 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4304 */	NdrFcShort( 0x18 ),	/* 24 */
/* 4306 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4308 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4310 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4312 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 4314 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4316 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4318 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4320 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4322 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4324 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4326 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 4328 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4330 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4332 */	NdrFcShort( 0x464 ),	/* Type Offset=1124 */

	/* Return value */

/* 4334 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4336 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4338 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_cloudPolicyOverridesPlatformPolicy */

/* 4340 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4342 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4346 */	NdrFcShort( 0x18 ),	/* 24 */
/* 4348 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4350 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4352 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4354 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 4356 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4358 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4360 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4362 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4364 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4366 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4368 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 4370 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4372 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4374 */	NdrFcShort( 0x47a ),	/* Type Offset=1146 */

	/* Return value */

/* 4376 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4378 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4380 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_cloudPolicyOverridesPlatformPolicy */

/* 4382 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4384 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4388 */	NdrFcShort( 0x18 ),	/* 24 */
/* 4390 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4392 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4394 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4396 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 4398 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4400 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4402 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4404 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4406 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4408 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4410 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 4412 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4414 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4416 */	NdrFcShort( 0x44e ),	/* Type Offset=1102 */

	/* Return value */

/* 4418 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4420 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4422 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure LaunchCmdLine */


	/* Procedure LaunchCmdLine */

/* 4424 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4426 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4430 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4432 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4434 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4436 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4438 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 4440 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4442 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4444 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4446 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4448 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4450 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4452 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter cmd_line */


	/* Parameter cmd_line */

/* 4454 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 4456 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4458 */	NdrFcShort( 0x492 ),	/* Type Offset=1170 */

	/* Return value */


	/* Return value */

/* 4460 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4462 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4464 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure LaunchBrowser */


	/* Procedure LaunchBrowser */

/* 4466 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4468 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4472 */	NdrFcShort( 0x4 ),	/* 4 */
/* 4474 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4476 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4478 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4480 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 4482 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4484 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4486 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4488 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4490 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4492 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4494 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter browser_type */


	/* Parameter browser_type */

/* 4496 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4498 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4500 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter url */


	/* Parameter url */

/* 4502 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 4504 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4506 */	NdrFcShort( 0x492 ),	/* Type Offset=1170 */

	/* Return value */


	/* Return value */

/* 4508 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4510 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4512 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure LaunchCmdElevated */


	/* Procedure LaunchCmdElevated */

/* 4514 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4516 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4520 */	NdrFcShort( 0x5 ),	/* 5 */
/* 4522 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 4524 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4526 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4528 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 4530 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4532 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4534 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4536 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4538 */	NdrFcShort( 0x5 ),	/* 5 */
/* 4540 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 4542 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4544 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter app_guid */


	/* Parameter app_guid */

/* 4546 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 4548 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4550 */	NdrFcShort( 0x492 ),	/* Type Offset=1170 */

	/* Parameter cmd_id */


	/* Parameter cmd_id */

/* 4552 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 4554 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4556 */	NdrFcShort( 0x492 ),	/* Type Offset=1170 */

	/* Parameter caller_proc_id */


	/* Parameter caller_proc_id */

/* 4558 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4560 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4562 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter proc_handle */


	/* Parameter proc_handle */

/* 4564 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4566 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4568 */	0xb9,		/* FC_UINT3264 */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 4570 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4572 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 4574 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure LaunchCmdLineEx */


	/* Procedure LaunchCmdLineEx */

/* 4576 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4578 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4582 */	NdrFcShort( 0x6 ),	/* 6 */
/* 4584 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 4586 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4588 */	NdrFcShort( 0x5c ),	/* 92 */
/* 4590 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 4592 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4594 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4596 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4598 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4600 */	NdrFcShort( 0x5 ),	/* 5 */
/* 4602 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 4604 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4606 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter cmd_line */


	/* Parameter cmd_line */

/* 4608 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 4610 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4612 */	NdrFcShort( 0x492 ),	/* Type Offset=1170 */

	/* Parameter server_proc_id */


	/* Parameter server_proc_id */

/* 4614 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4616 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4618 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter proc_handle */


	/* Parameter proc_handle */

/* 4620 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4622 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4624 */	0xb9,		/* FC_UINT3264 */
			0x0,		/* 0 */

	/* Parameter stdout_handle */


	/* Parameter stdout_handle */

/* 4626 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4628 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4630 */	0xb9,		/* FC_UINT3264 */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 4632 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4634 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 4636 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const updater_legacy_idl_MIDL_TYPE_FORMAT_STRING updater_legacy_idl__MIDL_TypeFormatString =
    {
        0,
        {
			NdrFcShort( 0x0 ),	/* 0 */
/*  2 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/*  4 */	NdrFcShort( 0x1c ),	/* Offset= 28 (32) */
/*  6 */	
			0x13, 0x0,	/* FC_OP */
/*  8 */	NdrFcShort( 0xe ),	/* Offset= 14 (22) */
/* 10 */	
			0x1b,		/* FC_CARRAY */
			0x1,		/* 1 */
/* 12 */	NdrFcShort( 0x2 ),	/* 2 */
/* 14 */	0x9,		/* Corr desc: FC_ULONG */
			0x0,		/*  */
/* 16 */	NdrFcShort( 0xfffc ),	/* -4 */
/* 18 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 20 */	0x6,		/* FC_SHORT */
			0x5b,		/* FC_END */
/* 22 */	
			0x17,		/* FC_CSTRUCT */
			0x3,		/* 3 */
/* 24 */	NdrFcShort( 0x8 ),	/* 8 */
/* 26 */	NdrFcShort( 0xfff0 ),	/* Offset= -16 (10) */
/* 28 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 30 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 32 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 34 */	NdrFcShort( 0x0 ),	/* 0 */
/* 36 */	NdrFcShort( 0x8 ),	/* 8 */
/* 38 */	NdrFcShort( 0x0 ),	/* 0 */
/* 40 */	NdrFcShort( 0xffde ),	/* Offset= -34 (6) */
/* 42 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 44 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */
/* 46 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 48 */	NdrFcShort( 0x2 ),	/* Offset= 2 (50) */
/* 50 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 52 */	NdrFcLong( 0x20400 ),	/* 132096 */
/* 56 */	NdrFcShort( 0x0 ),	/* 0 */
/* 58 */	NdrFcShort( 0x0 ),	/* 0 */
/* 60 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 62 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 64 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 66 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 68 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 70 */	0xb,		/* FC_HYPER */
			0x5c,		/* FC_PAD */
/* 72 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 74 */	0x6,		/* FC_SHORT */
			0x5c,		/* FC_PAD */
/* 76 */	
			0x12, 0x0,	/* FC_UP */
/* 78 */	NdrFcShort( 0xffc8 ),	/* Offset= -56 (22) */
/* 80 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 82 */	NdrFcShort( 0x0 ),	/* 0 */
/* 84 */	NdrFcShort( 0x8 ),	/* 8 */
/* 86 */	NdrFcShort( 0x0 ),	/* 0 */
/* 88 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (76) */
/* 90 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 92 */	NdrFcShort( 0x3a2 ),	/* Offset= 930 (1022) */
/* 94 */	
			0x13, 0x0,	/* FC_OP */
/* 96 */	NdrFcShort( 0x38a ),	/* Offset= 906 (1002) */
/* 98 */	
			0x2b,		/* FC_NON_ENCAPSULATED_UNION */
			0x9,		/* FC_ULONG */
/* 100 */	0x7,		/* Corr desc: FC_USHORT */
			0x0,		/*  */
/* 102 */	NdrFcShort( 0xfff8 ),	/* -8 */
/* 104 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 106 */	NdrFcShort( 0x2 ),	/* Offset= 2 (108) */
/* 108 */	NdrFcShort( 0x10 ),	/* 16 */
/* 110 */	NdrFcShort( 0x2f ),	/* 47 */
/* 112 */	NdrFcLong( 0x14 ),	/* 20 */
/* 116 */	NdrFcShort( 0x800b ),	/* Simple arm type: FC_HYPER */
/* 118 */	NdrFcLong( 0x3 ),	/* 3 */
/* 122 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 124 */	NdrFcLong( 0x11 ),	/* 17 */
/* 128 */	NdrFcShort( 0x8001 ),	/* Simple arm type: FC_BYTE */
/* 130 */	NdrFcLong( 0x2 ),	/* 2 */
/* 134 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 136 */	NdrFcLong( 0x4 ),	/* 4 */
/* 140 */	NdrFcShort( 0x800a ),	/* Simple arm type: FC_FLOAT */
/* 142 */	NdrFcLong( 0x5 ),	/* 5 */
/* 146 */	NdrFcShort( 0x800c ),	/* Simple arm type: FC_DOUBLE */
/* 148 */	NdrFcLong( 0xb ),	/* 11 */
/* 152 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 154 */	NdrFcLong( 0xa ),	/* 10 */
/* 158 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 160 */	NdrFcLong( 0x6 ),	/* 6 */
/* 164 */	NdrFcShort( 0xe8 ),	/* Offset= 232 (396) */
/* 166 */	NdrFcLong( 0x7 ),	/* 7 */
/* 170 */	NdrFcShort( 0x800c ),	/* Simple arm type: FC_DOUBLE */
/* 172 */	NdrFcLong( 0x8 ),	/* 8 */
/* 176 */	NdrFcShort( 0xff56 ),	/* Offset= -170 (6) */
/* 178 */	NdrFcLong( 0xd ),	/* 13 */
/* 182 */	NdrFcShort( 0xdc ),	/* Offset= 220 (402) */
/* 184 */	NdrFcLong( 0x9 ),	/* 9 */
/* 188 */	NdrFcShort( 0xff76 ),	/* Offset= -138 (50) */
/* 190 */	NdrFcLong( 0x2000 ),	/* 8192 */
/* 194 */	NdrFcShort( 0xe2 ),	/* Offset= 226 (420) */
/* 196 */	NdrFcLong( 0x24 ),	/* 36 */
/* 200 */	NdrFcShort( 0x2d8 ),	/* Offset= 728 (928) */
/* 202 */	NdrFcLong( 0x4024 ),	/* 16420 */
/* 206 */	NdrFcShort( 0x2d2 ),	/* Offset= 722 (928) */
/* 208 */	NdrFcLong( 0x4011 ),	/* 16401 */
/* 212 */	NdrFcShort( 0x2d0 ),	/* Offset= 720 (932) */
/* 214 */	NdrFcLong( 0x4002 ),	/* 16386 */
/* 218 */	NdrFcShort( 0x2ce ),	/* Offset= 718 (936) */
/* 220 */	NdrFcLong( 0x4003 ),	/* 16387 */
/* 224 */	NdrFcShort( 0x2cc ),	/* Offset= 716 (940) */
/* 226 */	NdrFcLong( 0x4014 ),	/* 16404 */
/* 230 */	NdrFcShort( 0x2ca ),	/* Offset= 714 (944) */
/* 232 */	NdrFcLong( 0x4004 ),	/* 16388 */
/* 236 */	NdrFcShort( 0x2c8 ),	/* Offset= 712 (948) */
/* 238 */	NdrFcLong( 0x4005 ),	/* 16389 */
/* 242 */	NdrFcShort( 0x2c6 ),	/* Offset= 710 (952) */
/* 244 */	NdrFcLong( 0x400b ),	/* 16395 */
/* 248 */	NdrFcShort( 0x2b0 ),	/* Offset= 688 (936) */
/* 250 */	NdrFcLong( 0x400a ),	/* 16394 */
/* 254 */	NdrFcShort( 0x2ae ),	/* Offset= 686 (940) */
/* 256 */	NdrFcLong( 0x4006 ),	/* 16390 */
/* 260 */	NdrFcShort( 0x2b8 ),	/* Offset= 696 (956) */
/* 262 */	NdrFcLong( 0x4007 ),	/* 16391 */
/* 266 */	NdrFcShort( 0x2ae ),	/* Offset= 686 (952) */
/* 268 */	NdrFcLong( 0x4008 ),	/* 16392 */
/* 272 */	NdrFcShort( 0x2b0 ),	/* Offset= 688 (960) */
/* 274 */	NdrFcLong( 0x400d ),	/* 16397 */
/* 278 */	NdrFcShort( 0x2ae ),	/* Offset= 686 (964) */
/* 280 */	NdrFcLong( 0x4009 ),	/* 16393 */
/* 284 */	NdrFcShort( 0x2ac ),	/* Offset= 684 (968) */
/* 286 */	NdrFcLong( 0x6000 ),	/* 24576 */
/* 290 */	NdrFcShort( 0x2aa ),	/* Offset= 682 (972) */
/* 292 */	NdrFcLong( 0x400c ),	/* 16396 */
/* 296 */	NdrFcShort( 0x2a8 ),	/* Offset= 680 (976) */
/* 298 */	NdrFcLong( 0x10 ),	/* 16 */
/* 302 */	NdrFcShort( 0x8002 ),	/* Simple arm type: FC_CHAR */
/* 304 */	NdrFcLong( 0x12 ),	/* 18 */
/* 308 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 310 */	NdrFcLong( 0x13 ),	/* 19 */
/* 314 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 316 */	NdrFcLong( 0x15 ),	/* 21 */
/* 320 */	NdrFcShort( 0x800b ),	/* Simple arm type: FC_HYPER */
/* 322 */	NdrFcLong( 0x16 ),	/* 22 */
/* 326 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 328 */	NdrFcLong( 0x17 ),	/* 23 */
/* 332 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 334 */	NdrFcLong( 0xe ),	/* 14 */
/* 338 */	NdrFcShort( 0x286 ),	/* Offset= 646 (984) */
/* 340 */	NdrFcLong( 0x400e ),	/* 16398 */
/* 344 */	NdrFcShort( 0x28a ),	/* Offset= 650 (994) */
/* 346 */	NdrFcLong( 0x4010 ),	/* 16400 */
/* 350 */	NdrFcShort( 0x288 ),	/* Offset= 648 (998) */
/* 352 */	NdrFcLong( 0x4012 ),	/* 16402 */
/* 356 */	NdrFcShort( 0x244 ),	/* Offset= 580 (936) */
/* 358 */	NdrFcLong( 0x4013 ),	/* 16403 */
/* 362 */	NdrFcShort( 0x242 ),	/* Offset= 578 (940) */
/* 364 */	NdrFcLong( 0x4015 ),	/* 16405 */
/* 368 */	NdrFcShort( 0x240 ),	/* Offset= 576 (944) */
/* 370 */	NdrFcLong( 0x4016 ),	/* 16406 */
/* 374 */	NdrFcShort( 0x236 ),	/* Offset= 566 (940) */
/* 376 */	NdrFcLong( 0x4017 ),	/* 16407 */
/* 380 */	NdrFcShort( 0x230 ),	/* Offset= 560 (940) */
/* 382 */	NdrFcLong( 0x0 ),	/* 0 */
/* 386 */	NdrFcShort( 0x0 ),	/* Offset= 0 (386) */
/* 388 */	NdrFcLong( 0x1 ),	/* 1 */
/* 392 */	NdrFcShort( 0x0 ),	/* Offset= 0 (392) */
/* 394 */	NdrFcShort( 0xffff ),	/* Offset= -1 (393) */
/* 396 */	
			0x15,		/* FC_STRUCT */
			0x7,		/* 7 */
/* 398 */	NdrFcShort( 0x8 ),	/* 8 */
/* 400 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 402 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 404 */	NdrFcLong( 0x0 ),	/* 0 */
/* 408 */	NdrFcShort( 0x0 ),	/* 0 */
/* 410 */	NdrFcShort( 0x0 ),	/* 0 */
/* 412 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 414 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 416 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 418 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 420 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 422 */	NdrFcShort( 0x2 ),	/* Offset= 2 (424) */
/* 424 */	
			0x13, 0x0,	/* FC_OP */
/* 426 */	NdrFcShort( 0x1e4 ),	/* Offset= 484 (910) */
/* 428 */	
			0x2a,		/* FC_ENCAPSULATED_UNION */
			0x89,		/* 137 */
/* 430 */	NdrFcShort( 0x20 ),	/* 32 */
/* 432 */	NdrFcShort( 0xa ),	/* 10 */
/* 434 */	NdrFcLong( 0x8 ),	/* 8 */
/* 438 */	NdrFcShort( 0x50 ),	/* Offset= 80 (518) */
/* 440 */	NdrFcLong( 0xd ),	/* 13 */
/* 444 */	NdrFcShort( 0x70 ),	/* Offset= 112 (556) */
/* 446 */	NdrFcLong( 0x9 ),	/* 9 */
/* 450 */	NdrFcShort( 0x90 ),	/* Offset= 144 (594) */
/* 452 */	NdrFcLong( 0xc ),	/* 12 */
/* 456 */	NdrFcShort( 0xb0 ),	/* Offset= 176 (632) */
/* 458 */	NdrFcLong( 0x24 ),	/* 36 */
/* 462 */	NdrFcShort( 0x102 ),	/* Offset= 258 (720) */
/* 464 */	NdrFcLong( 0x800d ),	/* 32781 */
/* 468 */	NdrFcShort( 0x11e ),	/* Offset= 286 (754) */
/* 470 */	NdrFcLong( 0x10 ),	/* 16 */
/* 474 */	NdrFcShort( 0x138 ),	/* Offset= 312 (786) */
/* 476 */	NdrFcLong( 0x2 ),	/* 2 */
/* 480 */	NdrFcShort( 0x14e ),	/* Offset= 334 (814) */
/* 482 */	NdrFcLong( 0x3 ),	/* 3 */
/* 486 */	NdrFcShort( 0x164 ),	/* Offset= 356 (842) */
/* 488 */	NdrFcLong( 0x14 ),	/* 20 */
/* 492 */	NdrFcShort( 0x17a ),	/* Offset= 378 (870) */
/* 494 */	NdrFcShort( 0xffff ),	/* Offset= -1 (493) */
/* 496 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 498 */	NdrFcShort( 0x0 ),	/* 0 */
/* 500 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 502 */	NdrFcShort( 0x0 ),	/* 0 */
/* 504 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 506 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 510 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 512 */	
			0x13, 0x0,	/* FC_OP */
/* 514 */	NdrFcShort( 0xfe14 ),	/* Offset= -492 (22) */
/* 516 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 518 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 520 */	NdrFcShort( 0x10 ),	/* 16 */
/* 522 */	NdrFcShort( 0x0 ),	/* 0 */
/* 524 */	NdrFcShort( 0x6 ),	/* Offset= 6 (530) */
/* 526 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 528 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 530 */	
			0x11, 0x0,	/* FC_RP */
/* 532 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (496) */
/* 534 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 536 */	NdrFcShort( 0x0 ),	/* 0 */
/* 538 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 540 */	NdrFcShort( 0x0 ),	/* 0 */
/* 542 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 544 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 548 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 550 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 552 */	NdrFcShort( 0xff6a ),	/* Offset= -150 (402) */
/* 554 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 556 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 558 */	NdrFcShort( 0x10 ),	/* 16 */
/* 560 */	NdrFcShort( 0x0 ),	/* 0 */
/* 562 */	NdrFcShort( 0x6 ),	/* Offset= 6 (568) */
/* 564 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 566 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 568 */	
			0x11, 0x0,	/* FC_RP */
/* 570 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (534) */
/* 572 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 574 */	NdrFcShort( 0x0 ),	/* 0 */
/* 576 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 578 */	NdrFcShort( 0x0 ),	/* 0 */
/* 580 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 582 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 586 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 588 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 590 */	NdrFcShort( 0xfde4 ),	/* Offset= -540 (50) */
/* 592 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 594 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 596 */	NdrFcShort( 0x10 ),	/* 16 */
/* 598 */	NdrFcShort( 0x0 ),	/* 0 */
/* 600 */	NdrFcShort( 0x6 ),	/* Offset= 6 (606) */
/* 602 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 604 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 606 */	
			0x11, 0x0,	/* FC_RP */
/* 608 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (572) */
/* 610 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 612 */	NdrFcShort( 0x0 ),	/* 0 */
/* 614 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 616 */	NdrFcShort( 0x0 ),	/* 0 */
/* 618 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 620 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 624 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 626 */	
			0x13, 0x0,	/* FC_OP */
/* 628 */	NdrFcShort( 0x176 ),	/* Offset= 374 (1002) */
/* 630 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 632 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 634 */	NdrFcShort( 0x10 ),	/* 16 */
/* 636 */	NdrFcShort( 0x0 ),	/* 0 */
/* 638 */	NdrFcShort( 0x6 ),	/* Offset= 6 (644) */
/* 640 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 642 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 644 */	
			0x11, 0x0,	/* FC_RP */
/* 646 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (610) */
/* 648 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 650 */	NdrFcLong( 0x2f ),	/* 47 */
/* 654 */	NdrFcShort( 0x0 ),	/* 0 */
/* 656 */	NdrFcShort( 0x0 ),	/* 0 */
/* 658 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 660 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 662 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 664 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 666 */	
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 668 */	NdrFcShort( 0x1 ),	/* 1 */
/* 670 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 672 */	NdrFcShort( 0x4 ),	/* 4 */
/* 674 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 676 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 678 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 680 */	NdrFcShort( 0x18 ),	/* 24 */
/* 682 */	NdrFcShort( 0x0 ),	/* 0 */
/* 684 */	NdrFcShort( 0xa ),	/* Offset= 10 (694) */
/* 686 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 688 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 690 */	NdrFcShort( 0xffd6 ),	/* Offset= -42 (648) */
/* 692 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 694 */	
			0x13, 0x0,	/* FC_OP */
/* 696 */	NdrFcShort( 0xffe2 ),	/* Offset= -30 (666) */
/* 698 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 700 */	NdrFcShort( 0x0 ),	/* 0 */
/* 702 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 704 */	NdrFcShort( 0x0 ),	/* 0 */
/* 706 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 708 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 712 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 714 */	
			0x13, 0x0,	/* FC_OP */
/* 716 */	NdrFcShort( 0xffda ),	/* Offset= -38 (678) */
/* 718 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 720 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 722 */	NdrFcShort( 0x10 ),	/* 16 */
/* 724 */	NdrFcShort( 0x0 ),	/* 0 */
/* 726 */	NdrFcShort( 0x6 ),	/* Offset= 6 (732) */
/* 728 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 730 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 732 */	
			0x11, 0x0,	/* FC_RP */
/* 734 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (698) */
/* 736 */	
			0x1d,		/* FC_SMFARRAY */
			0x0,		/* 0 */
/* 738 */	NdrFcShort( 0x8 ),	/* 8 */
/* 740 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 742 */	
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 744 */	NdrFcShort( 0x10 ),	/* 16 */
/* 746 */	0x8,		/* FC_LONG */
			0x6,		/* FC_SHORT */
/* 748 */	0x6,		/* FC_SHORT */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 750 */	0x0,		/* 0 */
			NdrFcShort( 0xfff1 ),	/* Offset= -15 (736) */
			0x5b,		/* FC_END */
/* 754 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 756 */	NdrFcShort( 0x20 ),	/* 32 */
/* 758 */	NdrFcShort( 0x0 ),	/* 0 */
/* 760 */	NdrFcShort( 0xa ),	/* Offset= 10 (770) */
/* 762 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 764 */	0x36,		/* FC_POINTER */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 766 */	0x0,		/* 0 */
			NdrFcShort( 0xffe7 ),	/* Offset= -25 (742) */
			0x5b,		/* FC_END */
/* 770 */	
			0x11, 0x0,	/* FC_RP */
/* 772 */	NdrFcShort( 0xff12 ),	/* Offset= -238 (534) */
/* 774 */	
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 776 */	NdrFcShort( 0x1 ),	/* 1 */
/* 778 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 780 */	NdrFcShort( 0x0 ),	/* 0 */
/* 782 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 784 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 786 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 788 */	NdrFcShort( 0x10 ),	/* 16 */
/* 790 */	NdrFcShort( 0x0 ),	/* 0 */
/* 792 */	NdrFcShort( 0x6 ),	/* Offset= 6 (798) */
/* 794 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 796 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 798 */	
			0x13, 0x0,	/* FC_OP */
/* 800 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (774) */
/* 802 */	
			0x1b,		/* FC_CARRAY */
			0x1,		/* 1 */
/* 804 */	NdrFcShort( 0x2 ),	/* 2 */
/* 806 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 808 */	NdrFcShort( 0x0 ),	/* 0 */
/* 810 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 812 */	0x6,		/* FC_SHORT */
			0x5b,		/* FC_END */
/* 814 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 816 */	NdrFcShort( 0x10 ),	/* 16 */
/* 818 */	NdrFcShort( 0x0 ),	/* 0 */
/* 820 */	NdrFcShort( 0x6 ),	/* Offset= 6 (826) */
/* 822 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 824 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 826 */	
			0x13, 0x0,	/* FC_OP */
/* 828 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (802) */
/* 830 */	
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 832 */	NdrFcShort( 0x4 ),	/* 4 */
/* 834 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 836 */	NdrFcShort( 0x0 ),	/* 0 */
/* 838 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 840 */	0x8,		/* FC_LONG */
			0x5b,		/* FC_END */
/* 842 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 844 */	NdrFcShort( 0x10 ),	/* 16 */
/* 846 */	NdrFcShort( 0x0 ),	/* 0 */
/* 848 */	NdrFcShort( 0x6 ),	/* Offset= 6 (854) */
/* 850 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 852 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 854 */	
			0x13, 0x0,	/* FC_OP */
/* 856 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (830) */
/* 858 */	
			0x1b,		/* FC_CARRAY */
			0x7,		/* 7 */
/* 860 */	NdrFcShort( 0x8 ),	/* 8 */
/* 862 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 864 */	NdrFcShort( 0x0 ),	/* 0 */
/* 866 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 868 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 870 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 872 */	NdrFcShort( 0x10 ),	/* 16 */
/* 874 */	NdrFcShort( 0x0 ),	/* 0 */
/* 876 */	NdrFcShort( 0x6 ),	/* Offset= 6 (882) */
/* 878 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 880 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 882 */	
			0x13, 0x0,	/* FC_OP */
/* 884 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (858) */
/* 886 */	
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 888 */	NdrFcShort( 0x8 ),	/* 8 */
/* 890 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 892 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 894 */	
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 896 */	NdrFcShort( 0x8 ),	/* 8 */
/* 898 */	0x7,		/* Corr desc: FC_USHORT */
			0x0,		/*  */
/* 900 */	NdrFcShort( 0xffc8 ),	/* -56 */
/* 902 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 904 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 906 */	NdrFcShort( 0xffec ),	/* Offset= -20 (886) */
/* 908 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 910 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 912 */	NdrFcShort( 0x38 ),	/* 56 */
/* 914 */	NdrFcShort( 0xffec ),	/* Offset= -20 (894) */
/* 916 */	NdrFcShort( 0x0 ),	/* Offset= 0 (916) */
/* 918 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 920 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 922 */	0x40,		/* FC_STRUCTPAD4 */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 924 */	0x0,		/* 0 */
			NdrFcShort( 0xfe0f ),	/* Offset= -497 (428) */
			0x5b,		/* FC_END */
/* 928 */	
			0x13, 0x0,	/* FC_OP */
/* 930 */	NdrFcShort( 0xff04 ),	/* Offset= -252 (678) */
/* 932 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 934 */	0x1,		/* FC_BYTE */
			0x5c,		/* FC_PAD */
/* 936 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 938 */	0x6,		/* FC_SHORT */
			0x5c,		/* FC_PAD */
/* 940 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 942 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */
/* 944 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 946 */	0xb,		/* FC_HYPER */
			0x5c,		/* FC_PAD */
/* 948 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 950 */	0xa,		/* FC_FLOAT */
			0x5c,		/* FC_PAD */
/* 952 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 954 */	0xc,		/* FC_DOUBLE */
			0x5c,		/* FC_PAD */
/* 956 */	
			0x13, 0x0,	/* FC_OP */
/* 958 */	NdrFcShort( 0xfdce ),	/* Offset= -562 (396) */
/* 960 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 962 */	NdrFcShort( 0xfc44 ),	/* Offset= -956 (6) */
/* 964 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 966 */	NdrFcShort( 0xfdcc ),	/* Offset= -564 (402) */
/* 968 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 970 */	NdrFcShort( 0xfc68 ),	/* Offset= -920 (50) */
/* 972 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 974 */	NdrFcShort( 0xfdd6 ),	/* Offset= -554 (420) */
/* 976 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 978 */	NdrFcShort( 0x2 ),	/* Offset= 2 (980) */
/* 980 */	
			0x13, 0x0,	/* FC_OP */
/* 982 */	NdrFcShort( 0x14 ),	/* Offset= 20 (1002) */
/* 984 */	
			0x15,		/* FC_STRUCT */
			0x7,		/* 7 */
/* 986 */	NdrFcShort( 0x10 ),	/* 16 */
/* 988 */	0x6,		/* FC_SHORT */
			0x1,		/* FC_BYTE */
/* 990 */	0x1,		/* FC_BYTE */
			0x8,		/* FC_LONG */
/* 992 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 994 */	
			0x13, 0x0,	/* FC_OP */
/* 996 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (984) */
/* 998 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 1000 */	0x2,		/* FC_CHAR */
			0x5c,		/* FC_PAD */
/* 1002 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x7,		/* 7 */
/* 1004 */	NdrFcShort( 0x20 ),	/* 32 */
/* 1006 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1008 */	NdrFcShort( 0x0 ),	/* Offset= 0 (1008) */
/* 1010 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 1012 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 1014 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 1016 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1018 */	NdrFcShort( 0xfc68 ),	/* Offset= -920 (98) */
/* 1020 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1022 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 1024 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1026 */	NdrFcShort( 0x18 ),	/* 24 */
/* 1028 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1030 */	NdrFcShort( 0xfc58 ),	/* Offset= -936 (94) */
/* 1032 */	
			0x11, 0x0,	/* FC_RP */
/* 1034 */	NdrFcShort( 0x6 ),	/* Offset= 6 (1040) */
/* 1036 */	
			0x12, 0x0,	/* FC_UP */
/* 1038 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (1002) */
/* 1040 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 1042 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1044 */	NdrFcShort( 0x18 ),	/* 24 */
/* 1046 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1048 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (1036) */
/* 1050 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 1052 */	0xc,		/* FC_DOUBLE */
			0x5c,		/* FC_PAD */
/* 1054 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 1056 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1058) */
/* 1058 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1060 */	NdrFcLong( 0x2a7d2ae7 ),	/* 712846055 */
/* 1064 */	NdrFcShort( 0x8eee ),	/* -28946 */
/* 1066 */	NdrFcShort( 0x45b4 ),	/* 17844 */
/* 1068 */	0xb1,		/* 177 */
			0x7f,		/* 127 */
/* 1070 */	0x31,		/* 49 */
			0xda,		/* 218 */
/* 1072 */	0xac,		/* 172 */
			0x82,		/* 130 */
/* 1074 */	0xcc,		/* 204 */
			0xbb,		/* 187 */
/* 1076 */	
			0x11, 0x8,	/* FC_RP [simple_pointer] */
/* 1078 */	0x6,		/* FC_SHORT */
			0x5c,		/* FC_PAD */
/* 1080 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 1082 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1084) */
/* 1084 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1086 */	NdrFcLong( 0x7e0a6b39 ),	/* 2114612025 */
/* 1090 */	NdrFcShort( 0x7ceb ),	/* 31979 */
/* 1092 */	NdrFcShort( 0x4944 ),	/* 18756 */
/* 1094 */	0xab,		/* 171 */
			0xfa,		/* 250 */
/* 1096 */	0xf4,		/* 244 */
			0x19,		/* 25 */
/* 1098 */	0xd2,		/* 210 */
			0x1,		/* 1 */
/* 1100 */	0xd6,		/* 214 */
			0xa0,		/* 160 */
/* 1102 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 1104 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1106) */
/* 1106 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1108 */	NdrFcLong( 0xcc2ccd05 ),	/* -869479163 */
/* 1112 */	NdrFcShort( 0x119c ),	/* 4508 */
/* 1114 */	NdrFcShort( 0x44e1 ),	/* 17633 */
/* 1116 */	0x85,		/* 133 */
			0x2d,		/* 45 */
/* 1118 */	0x6d,		/* 109 */
			0xcc,		/* 204 */
/* 1120 */	0x2d,		/* 45 */
			0xfb,		/* 251 */
/* 1122 */	0x72,		/* 114 */
			0xec,		/* 236 */
/* 1124 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 1126 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1128) */
/* 1128 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1130 */	NdrFcLong( 0x2a7d2ae7 ),	/* 712846055 */
/* 1134 */	NdrFcShort( 0x8eee ),	/* -28946 */
/* 1136 */	NdrFcShort( 0x45b4 ),	/* 17844 */
/* 1138 */	0xb1,		/* 177 */
			0x7f,		/* 127 */
/* 1140 */	0x31,		/* 49 */
			0xda,		/* 218 */
/* 1142 */	0xac,		/* 172 */
			0x82,		/* 130 */
/* 1144 */	0xcc,		/* 204 */
			0xbb,		/* 187 */
/* 1146 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 1148 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1150) */
/* 1150 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1152 */	NdrFcLong( 0x7e0a6b39 ),	/* 2114612025 */
/* 1156 */	NdrFcShort( 0x7ceb ),	/* 31979 */
/* 1158 */	NdrFcShort( 0x4944 ),	/* 18756 */
/* 1160 */	0xab,		/* 171 */
			0xfa,		/* 250 */
/* 1162 */	0xf4,		/* 244 */
			0x19,		/* 25 */
/* 1164 */	0xd2,		/* 210 */
			0x1,		/* 1 */
/* 1166 */	0xd6,		/* 214 */
			0xa0,		/* 160 */
/* 1168 */	
			0x11, 0x8,	/* FC_RP [simple_pointer] */
/* 1170 */	
			0x25,		/* FC_C_WSTRING */
			0x5c,		/* FC_PAD */
/* 1172 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 1174 */	0xb9,		/* FC_UINT3264 */
			0x5c,		/* FC_PAD */

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



/* Standard interface: __MIDL_itf_updater_legacy_idl_0000_0000, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IUnknown, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IDispatch, ver. 0.0,
   GUID={0x00020400,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IAppVersionWeb, ver. 0.0,
   GUID={0xAA10D17D,0x7A09,0x48AC,{0xB1,0xE4,0xF1,0x24,0x93,0x7E,0x3D,0x26}} */

#pragma code_seg(".orpc")
static const unsigned short IAppVersionWeb_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    42,
    84
    };

static const MIDL_STUBLESS_PROXY_INFO IAppVersionWeb_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppVersionWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppVersionWeb_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppVersionWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(10) _IAppVersionWebProxyVtbl = 
{
    &IAppVersionWeb_ProxyInfo,
    &IID_IAppVersionWeb,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppVersionWeb::get_version */ ,
    (void *) (INT_PTR) -1 /* IAppVersionWeb::get_packageCount */ ,
    (void *) (INT_PTR) -1 /* IAppVersionWeb::get_packageWeb */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAppVersionWeb_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppVersionWebStubVtbl =
{
    &IID_IAppVersionWeb,
    &IAppVersionWeb_ServerInfo,
    10,
    &IAppVersionWeb_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppVersionWebUser, ver. 0.0,
   GUID={0xAC817E10,0x993C,0x470F,{0x8D,0xCA,0x25,0xF5,0x3D,0x70,0xEA,0x8D}} */

#pragma code_seg(".orpc")
static const unsigned short IAppVersionWebUser_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    42,
    84
    };

static const MIDL_STUBLESS_PROXY_INFO IAppVersionWebUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppVersionWebUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppVersionWebUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppVersionWebUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(10) _IAppVersionWebUserProxyVtbl = 
{
    &IAppVersionWebUser_ProxyInfo,
    &IID_IAppVersionWebUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppVersionWebUser::get_version */ ,
    (void *) (INT_PTR) -1 /* IAppVersionWebUser::get_packageCount */ ,
    (void *) (INT_PTR) -1 /* IAppVersionWebUser::get_packageWeb */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAppVersionWebUser_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppVersionWebUserStubVtbl =
{
    &IID_IAppVersionWebUser,
    &IAppVersionWebUser_ServerInfo,
    10,
    &IAppVersionWebUser_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppVersionWebSystem, ver. 0.0,
   GUID={0x9367601E,0xC100,0x4702,{0x87,0x55,0x80,0x8D,0x6B,0xB3,0x85,0xD8}} */

#pragma code_seg(".orpc")
static const unsigned short IAppVersionWebSystem_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    42,
    84
    };

static const MIDL_STUBLESS_PROXY_INFO IAppVersionWebSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppVersionWebSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppVersionWebSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppVersionWebSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(10) _IAppVersionWebSystemProxyVtbl = 
{
    &IAppVersionWebSystem_ProxyInfo,
    &IID_IAppVersionWebSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppVersionWebSystem::get_version */ ,
    (void *) (INT_PTR) -1 /* IAppVersionWebSystem::get_packageCount */ ,
    (void *) (INT_PTR) -1 /* IAppVersionWebSystem::get_packageWeb */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAppVersionWebSystem_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppVersionWebSystemStubVtbl =
{
    &IID_IAppVersionWebSystem,
    &IAppVersionWebSystem_ServerInfo,
    10,
    &IAppVersionWebSystem_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: ICurrentState, ver. 0.0,
   GUID={0xA643508B,0xB1E3,0x4457,{0x97,0x69,0x32,0xC9,0x53,0xBD,0x1D,0x57}} */

#pragma code_seg(".orpc")
static const unsigned short ICurrentState_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    132,
    174,
    216,
    258,
    300,
    342,
    384,
    426,
    468,
    510,
    552,
    594,
    636,
    678,
    720,
    762,
    804
    };

static const MIDL_STUBLESS_PROXY_INFO ICurrentState_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &ICurrentState_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ICurrentState_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &ICurrentState_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(24) _ICurrentStateProxyVtbl = 
{
    &ICurrentState_ProxyInfo,
    &IID_ICurrentState,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_stateValue */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_availableVersion */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_bytesDownloaded */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_totalBytesToDownload */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_downloadTimeRemainingMs */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_nextRetryTime */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_installProgress */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_installTimeRemainingMs */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_isCanceled */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_errorCode */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_extraCode1 */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_completionMessage */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_installerResultCode */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_installerResultExtraCode1 */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_postInstallLaunchCommandLine */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_postInstallUrl */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_postInstallAction */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION ICurrentState_table[] =
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
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _ICurrentStateStubVtbl =
{
    &IID_ICurrentState,
    &ICurrentState_ServerInfo,
    24,
    &ICurrentState_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: ICurrentStateUser, ver. 0.0,
   GUID={0x31479718,0xD170,0x467B,{0x92,0x74,0x27,0xFC,0x3E,0x88,0xCB,0x76}} */

#pragma code_seg(".orpc")
static const unsigned short ICurrentStateUser_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    132,
    174,
    216,
    258,
    300,
    342,
    384,
    426,
    468,
    510,
    552,
    594,
    636,
    678,
    720,
    762,
    804
    };

static const MIDL_STUBLESS_PROXY_INFO ICurrentStateUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &ICurrentStateUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ICurrentStateUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &ICurrentStateUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(24) _ICurrentStateUserProxyVtbl = 
{
    &ICurrentStateUser_ProxyInfo,
    &IID_ICurrentStateUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_stateValue */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_availableVersion */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_bytesDownloaded */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_totalBytesToDownload */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_downloadTimeRemainingMs */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_nextRetryTime */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_installProgress */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_installTimeRemainingMs */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_isCanceled */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_errorCode */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_extraCode1 */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_completionMessage */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_installerResultCode */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_installerResultExtraCode1 */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_postInstallLaunchCommandLine */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_postInstallUrl */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateUser::get_postInstallAction */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION ICurrentStateUser_table[] =
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
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _ICurrentStateUserStubVtbl =
{
    &IID_ICurrentStateUser,
    &ICurrentStateUser_ServerInfo,
    24,
    &ICurrentStateUser_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: ICurrentStateSystem, ver. 0.0,
   GUID={0x71CBC6BB,0xCA4B,0x4B5A,{0x83,0xC0,0xFC,0x95,0xF9,0xCA,0x6A,0x30}} */

#pragma code_seg(".orpc")
static const unsigned short ICurrentStateSystem_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    132,
    174,
    216,
    258,
    300,
    342,
    384,
    426,
    468,
    510,
    552,
    594,
    636,
    678,
    720,
    762,
    804
    };

static const MIDL_STUBLESS_PROXY_INFO ICurrentStateSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &ICurrentStateSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ICurrentStateSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &ICurrentStateSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(24) _ICurrentStateSystemProxyVtbl = 
{
    &ICurrentStateSystem_ProxyInfo,
    &IID_ICurrentStateSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_stateValue */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_availableVersion */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_bytesDownloaded */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_totalBytesToDownload */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_downloadTimeRemainingMs */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_nextRetryTime */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_installProgress */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_installTimeRemainingMs */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_isCanceled */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_errorCode */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_extraCode1 */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_completionMessage */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_installerResultCode */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_installerResultExtraCode1 */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_postInstallLaunchCommandLine */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_postInstallUrl */ ,
    (void *) (INT_PTR) -1 /* ICurrentStateSystem::get_postInstallAction */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION ICurrentStateSystem_table[] =
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
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _ICurrentStateSystemStubVtbl =
{
    &IID_ICurrentStateSystem,
    &ICurrentStateSystem_ServerInfo,
    24,
    &ICurrentStateSystem_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IGoogleUpdate3Web, ver. 0.0,
   GUID={0xA35E1C5E,0x0A18,0x4FF1,{0x8C,0x4D,0xDD,0x8E,0xD0,0x7B,0x0B,0xD0}} */

#pragma code_seg(".orpc")
static const unsigned short IGoogleUpdate3Web_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    846
    };

static const MIDL_STUBLESS_PROXY_INFO IGoogleUpdate3Web_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdate3Web_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IGoogleUpdate3Web_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdate3Web_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(8) _IGoogleUpdate3WebProxyVtbl = 
{
    &IGoogleUpdate3Web_ProxyInfo,
    &IID_IGoogleUpdate3Web,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IGoogleUpdate3Web::createAppBundleWeb */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IGoogleUpdate3Web_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2
};

CInterfaceStubVtbl _IGoogleUpdate3WebStubVtbl =
{
    &IID_IGoogleUpdate3Web,
    &IGoogleUpdate3Web_ServerInfo,
    8,
    &IGoogleUpdate3Web_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IGoogleUpdate3WebUser, ver. 0.0,
   GUID={0xEE8EE731,0xC592,0x4A4F,{0x97,0x74,0xBB,0x04,0x33,0x7B,0x8F,0x46}} */

#pragma code_seg(".orpc")
static const unsigned short IGoogleUpdate3WebUser_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    846
    };

static const MIDL_STUBLESS_PROXY_INFO IGoogleUpdate3WebUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdate3WebUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IGoogleUpdate3WebUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdate3WebUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(8) _IGoogleUpdate3WebUserProxyVtbl = 
{
    &IGoogleUpdate3WebUser_ProxyInfo,
    &IID_IGoogleUpdate3WebUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IGoogleUpdate3WebUser::createAppBundleWeb */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IGoogleUpdate3WebUser_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2
};

CInterfaceStubVtbl _IGoogleUpdate3WebUserStubVtbl =
{
    &IID_IGoogleUpdate3WebUser,
    &IGoogleUpdate3WebUser_ServerInfo,
    8,
    &IGoogleUpdate3WebUser_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IGoogleUpdate3WebSystem, ver. 0.0,
   GUID={0xAE5F8C9D,0xB94D,0x4367,{0xA4,0x22,0xD1,0xDC,0x4E,0x91,0x3A,0x52}} */

#pragma code_seg(".orpc")
static const unsigned short IGoogleUpdate3WebSystem_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    846
    };

static const MIDL_STUBLESS_PROXY_INFO IGoogleUpdate3WebSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdate3WebSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IGoogleUpdate3WebSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdate3WebSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(8) _IGoogleUpdate3WebSystemProxyVtbl = 
{
    &IGoogleUpdate3WebSystem_ProxyInfo,
    &IID_IGoogleUpdate3WebSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IGoogleUpdate3WebSystem::createAppBundleWeb */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IGoogleUpdate3WebSystem_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2
};

CInterfaceStubVtbl _IGoogleUpdate3WebSystemStubVtbl =
{
    &IID_IGoogleUpdate3WebSystem,
    &IGoogleUpdate3WebSystem_ServerInfo,
    8,
    &IGoogleUpdate3WebSystem_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppBundleWeb, ver. 0.0,
   GUID={0x0569DBB9,0xBAA0,0x48D5,{0x85,0x43,0x0F,0x3B,0xE3,0x0A,0x16,0x48}} */

#pragma code_seg(".orpc")
static const unsigned short IAppBundleWeb_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    888,
    950,
    992,
    1026,
    1068,
    1110,
    384,
    1152,
    1200,
    1234,
    1268,
    1302,
    1336,
    1370,
    1404,
    1438,
    1486
    };

static const MIDL_STUBLESS_PROXY_INFO IAppBundleWeb_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppBundleWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppBundleWeb_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppBundleWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(24) _IAppBundleWebProxyVtbl = 
{
    &IAppBundleWeb_ProxyInfo,
    &IID_IAppBundleWeb,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::createApp */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::createInstalledApp */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::createAllInstalledApps */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::get_displayLanguage */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::put_displayLanguage */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::put_parentHWND */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::get_length */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::get_appWeb */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::initialize */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::checkForUpdate */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::download */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::install */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::pause */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::resume */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::cancel */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::downloadPackage */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::get_currentState */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAppBundleWeb_table[] =
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
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppBundleWebStubVtbl =
{
    &IID_IAppBundleWeb,
    &IAppBundleWeb_ServerInfo,
    24,
    &IAppBundleWeb_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppBundleWebUser, ver. 0.0,
   GUID={0xCE7A37FD,0xA255,0x460C,{0xBA,0xF1,0x70,0x87,0x65,0xEB,0x76,0xEC}} */

#pragma code_seg(".orpc")
static const unsigned short IAppBundleWebUser_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    888,
    950,
    992,
    1026,
    1068,
    1110,
    384,
    1152,
    1200,
    1234,
    1268,
    1302,
    1336,
    1370,
    1404,
    1438,
    1486
    };

static const MIDL_STUBLESS_PROXY_INFO IAppBundleWebUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppBundleWebUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppBundleWebUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppBundleWebUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(24) _IAppBundleWebUserProxyVtbl = 
{
    &IAppBundleWebUser_ProxyInfo,
    &IID_IAppBundleWebUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::createApp */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::createInstalledApp */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::createAllInstalledApps */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::get_displayLanguage */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::put_displayLanguage */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::put_parentHWND */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::get_length */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::get_appWeb */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::initialize */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::checkForUpdate */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::download */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::install */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::pause */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::resume */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::cancel */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::downloadPackage */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebUser::get_currentState */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAppBundleWebUser_table[] =
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
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppBundleWebUserStubVtbl =
{
    &IID_IAppBundleWebUser,
    &IAppBundleWebUser_ServerInfo,
    24,
    &IAppBundleWebUser_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppBundleWebSystem, ver. 0.0,
   GUID={0xBFFD766D,0xA2DD,0x436E,{0x89,0xFA,0xBF,0x05,0xBC,0x5B,0x59,0x58}} */

#pragma code_seg(".orpc")
static const unsigned short IAppBundleWebSystem_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    888,
    950,
    992,
    1026,
    1068,
    1110,
    384,
    1152,
    1200,
    1234,
    1268,
    1302,
    1336,
    1370,
    1404,
    1438,
    1486
    };

static const MIDL_STUBLESS_PROXY_INFO IAppBundleWebSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppBundleWebSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppBundleWebSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppBundleWebSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(24) _IAppBundleWebSystemProxyVtbl = 
{
    &IAppBundleWebSystem_ProxyInfo,
    &IID_IAppBundleWebSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::createApp */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::createInstalledApp */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::createAllInstalledApps */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::get_displayLanguage */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::put_displayLanguage */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::put_parentHWND */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::get_length */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::get_appWeb */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::initialize */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::checkForUpdate */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::download */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::install */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::pause */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::resume */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::cancel */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::downloadPackage */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWebSystem::get_currentState */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAppBundleWebSystem_table[] =
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
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppBundleWebSystemStubVtbl =
{
    &IID_IAppBundleWebSystem,
    &IAppBundleWebSystem_ServerInfo,
    24,
    &IAppBundleWebSystem_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppWeb, ver. 0.0,
   GUID={0x63D941DE,0xF67B,0x4E15,{0x8A,0x90,0x27,0x88,0x1D,0xA9,0xEF,0x4A}} */

#pragma code_seg(".orpc")
static const unsigned short IAppWeb_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    1528,
    1570,
    1612,
    1660,
    1694,
    1736,
    1770,
    1804,
    1846
    };

static const MIDL_STUBLESS_PROXY_INFO IAppWeb_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppWeb_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(17) _IAppWebProxyVtbl = 
{
    &IAppWeb_ProxyInfo,
    &IID_IAppWeb,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::get_appId */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::get_currentVersionWeb */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::get_nextVersionWeb */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::get_command */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::cancel */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::get_currentState */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::launch */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::uninstall */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::get_serverInstallDataIndex */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::put_serverInstallDataIndex */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAppWeb_table[] =
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
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppWebStubVtbl =
{
    &IID_IAppWeb,
    &IAppWeb_ServerInfo,
    17,
    &IAppWeb_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppWebUser, ver. 0.0,
   GUID={0x47B9D508,0xCB72,0x4F8B,{0xAF,0x00,0x7D,0x01,0x43,0x60,0x3B,0x25}} */

#pragma code_seg(".orpc")
static const unsigned short IAppWebUser_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    1528,
    1570,
    1612,
    1660,
    1694,
    1736,
    1770,
    1804,
    1846
    };

static const MIDL_STUBLESS_PROXY_INFO IAppWebUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppWebUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppWebUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppWebUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(17) _IAppWebUserProxyVtbl = 
{
    &IAppWebUser_ProxyInfo,
    &IID_IAppWebUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppWebUser::get_appId */ ,
    (void *) (INT_PTR) -1 /* IAppWebUser::get_currentVersionWeb */ ,
    (void *) (INT_PTR) -1 /* IAppWebUser::get_nextVersionWeb */ ,
    (void *) (INT_PTR) -1 /* IAppWebUser::get_command */ ,
    (void *) (INT_PTR) -1 /* IAppWebUser::cancel */ ,
    (void *) (INT_PTR) -1 /* IAppWebUser::get_currentState */ ,
    (void *) (INT_PTR) -1 /* IAppWebUser::launch */ ,
    (void *) (INT_PTR) -1 /* IAppWebUser::uninstall */ ,
    (void *) (INT_PTR) -1 /* IAppWebUser::get_serverInstallDataIndex */ ,
    (void *) (INT_PTR) -1 /* IAppWebUser::put_serverInstallDataIndex */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAppWebUser_table[] =
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
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppWebUserStubVtbl =
{
    &IID_IAppWebUser,
    &IAppWebUser_ServerInfo,
    17,
    &IAppWebUser_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppWebSystem, ver. 0.0,
   GUID={0x540B227A,0xF442,0x45D5,{0xBA,0x52,0x29,0x8A,0x05,0xBA,0xF1,0xA8}} */

#pragma code_seg(".orpc")
static const unsigned short IAppWebSystem_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    1528,
    1570,
    1612,
    1660,
    1694,
    1736,
    1770,
    1804,
    1846
    };

static const MIDL_STUBLESS_PROXY_INFO IAppWebSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppWebSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppWebSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppWebSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(17) _IAppWebSystemProxyVtbl = 
{
    &IAppWebSystem_ProxyInfo,
    &IID_IAppWebSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppWebSystem::get_appId */ ,
    (void *) (INT_PTR) -1 /* IAppWebSystem::get_currentVersionWeb */ ,
    (void *) (INT_PTR) -1 /* IAppWebSystem::get_nextVersionWeb */ ,
    (void *) (INT_PTR) -1 /* IAppWebSystem::get_command */ ,
    (void *) (INT_PTR) -1 /* IAppWebSystem::cancel */ ,
    (void *) (INT_PTR) -1 /* IAppWebSystem::get_currentState */ ,
    (void *) (INT_PTR) -1 /* IAppWebSystem::launch */ ,
    (void *) (INT_PTR) -1 /* IAppWebSystem::uninstall */ ,
    (void *) (INT_PTR) -1 /* IAppWebSystem::get_serverInstallDataIndex */ ,
    (void *) (INT_PTR) -1 /* IAppWebSystem::put_serverInstallDataIndex */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAppWebSystem_table[] =
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
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppWebSystemStubVtbl =
{
    &IID_IAppWebSystem,
    &IAppWebSystem_ServerInfo,
    17,
    &IAppWebSystem_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppCommandWeb, ver. 0.0,
   GUID={0x10A2D03F,0x8BC7,0x49DB,{0xA2,0x1E,0xA7,0xD4,0x42,0x9D,0x27,0x59}} */

#pragma code_seg(".orpc")
static const unsigned short IAppCommandWeb_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    132,
    42,
    1888,
    1930
    };

static const MIDL_STUBLESS_PROXY_INFO IAppCommandWeb_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppCommandWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppCommandWeb_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppCommandWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(11) _IAppCommandWebProxyVtbl = 
{
    &IAppCommandWeb_ProxyInfo,
    &IID_IAppCommandWeb,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWeb::get_status */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWeb::get_exitCode */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWeb::get_output */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWeb::execute */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAppCommandWeb_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppCommandWebStubVtbl =
{
    &IID_IAppCommandWeb,
    &IAppCommandWeb_ServerInfo,
    11,
    &IAppCommandWeb_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppCommandWebUser, ver. 0.0,
   GUID={0x5515E66F,0xFA6F,0x4D74,{0xB5,0xEA,0x4F,0xCF,0xDA,0x16,0xFE,0x12}} */

#pragma code_seg(".orpc")
static const unsigned short IAppCommandWebUser_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    132,
    42,
    1888,
    1930
    };

static const MIDL_STUBLESS_PROXY_INFO IAppCommandWebUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppCommandWebUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppCommandWebUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppCommandWebUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(11) _IAppCommandWebUserProxyVtbl = 
{
    &IAppCommandWebUser_ProxyInfo,
    &IID_IAppCommandWebUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWebUser::get_status */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWebUser::get_exitCode */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWebUser::get_output */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWebUser::execute */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAppCommandWebUser_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppCommandWebUserStubVtbl =
{
    &IID_IAppCommandWebUser,
    &IAppCommandWebUser_ServerInfo,
    11,
    &IAppCommandWebUser_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppCommandWebSystem, ver. 0.0,
   GUID={0xC6E2C5D5,0x86FA,0x4A64,{0x9D,0x08,0x8C,0x9B,0x64,0x4F,0x0E,0x49}} */

#pragma code_seg(".orpc")
static const unsigned short IAppCommandWebSystem_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    132,
    42,
    1888,
    1930
    };

static const MIDL_STUBLESS_PROXY_INFO IAppCommandWebSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppCommandWebSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppCommandWebSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IAppCommandWebSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(11) _IAppCommandWebSystemProxyVtbl = 
{
    &IAppCommandWebSystem_ProxyInfo,
    &IID_IAppCommandWebSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWebSystem::get_status */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWebSystem::get_exitCode */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWebSystem::get_output */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWebSystem::execute */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAppCommandWebSystem_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppCommandWebSystemStubVtbl =
{
    &IID_IAppCommandWebSystem,
    &IAppCommandWebSystem_ServerInfo,
    11,
    &IAppCommandWebSystem_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPolicyStatus, ver. 0.0,
   GUID={0x6A54FE75,0xEDC8,0x404E,{0xA4,0x1B,0x42,0x78,0xC0,0x55,0x71,0x51}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatus_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    132,
    2028,
    1888,
    258,
    300,
    2090,
    2138,
    2186,
    2234
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatus_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatus_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(16) _IPolicyStatusProxyVtbl = 
{
    &IPolicyStatus_ProxyInfo,
    &IID_IPolicyStatus,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_lastCheckPeriodMinutes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_updatesSuppressedTimes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_downloadPreferenceGroupPolicy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_packageCacheSizeLimitMBytes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_packageCacheExpirationTimeDays */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_effectivePolicyForAppInstalls */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_effectivePolicyForAppUpdates */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_targetVersionPrefix */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_isRollbackToTargetVersionAllowed */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IPolicyStatus_table[] =
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
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IPolicyStatusStubVtbl =
{
    &IID_IPolicyStatus,
    &IPolicyStatus_ServerInfo,
    16,
    &IPolicyStatus_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPolicyStatusUser, ver. 0.0,
   GUID={0xEF739C0C,0x40B0,0x478D,{0xB7,0x6B,0x36,0x59,0xB8,0xF2,0xB0,0xEB}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatusUser_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    132,
    2028,
    1888,
    258,
    300,
    2090,
    2138,
    2186,
    2234
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatusUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatusUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatusUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatusUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(16) _IPolicyStatusUserProxyVtbl = 
{
    &IPolicyStatusUser_ProxyInfo,
    &IID_IPolicyStatusUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusUser::get_lastCheckPeriodMinutes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusUser::get_updatesSuppressedTimes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusUser::get_downloadPreferenceGroupPolicy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusUser::get_packageCacheSizeLimitMBytes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusUser::get_packageCacheExpirationTimeDays */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusUser::get_effectivePolicyForAppInstalls */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusUser::get_effectivePolicyForAppUpdates */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusUser::get_targetVersionPrefix */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusUser::get_isRollbackToTargetVersionAllowed */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IPolicyStatusUser_table[] =
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
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IPolicyStatusUserStubVtbl =
{
    &IID_IPolicyStatusUser,
    &IPolicyStatusUser_ServerInfo,
    16,
    &IPolicyStatusUser_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPolicyStatusSystem, ver. 0.0,
   GUID={0xF3964464,0xA939,0x44D3,{0x92,0x44,0x36,0xBD,0x2E,0x36,0x30,0xB8}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatusSystem_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    132,
    2028,
    1888,
    258,
    300,
    2090,
    2138,
    2186,
    2234
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatusSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatusSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatusSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatusSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(16) _IPolicyStatusSystemProxyVtbl = 
{
    &IPolicyStatusSystem_ProxyInfo,
    &IID_IPolicyStatusSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusSystem::get_lastCheckPeriodMinutes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusSystem::get_updatesSuppressedTimes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusSystem::get_downloadPreferenceGroupPolicy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusSystem::get_packageCacheSizeLimitMBytes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusSystem::get_packageCacheExpirationTimeDays */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusSystem::get_effectivePolicyForAppInstalls */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusSystem::get_effectivePolicyForAppUpdates */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusSystem::get_targetVersionPrefix */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusSystem::get_isRollbackToTargetVersionAllowed */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IPolicyStatusSystem_table[] =
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
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IPolicyStatusSystemStubVtbl =
{
    &IID_IPolicyStatusSystem,
    &IPolicyStatusSystem_ServerInfo,
    16,
    &IPolicyStatusSystem_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPolicyStatusValue, ver. 0.0,
   GUID={0x2A7D2AE7,0x8EEE,0x45B4,{0xB1,0x7F,0x31,0xDA,0xAC,0x82,0xCC,0xBB}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatusValue_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    174,
    2282,
    1026,
    2324
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatusValue_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatusValue_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatusValue_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatusValue_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(12) _IPolicyStatusValueProxyVtbl = 
{
    &IPolicyStatusValue_ProxyInfo,
    &IID_IPolicyStatusValue,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusValue::get_source */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusValue::get_value */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusValue::get_hasConflict */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusValue::get_conflictSource */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusValue::get_conflictValue */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IPolicyStatusValue_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IPolicyStatusValueStubVtbl =
{
    &IID_IPolicyStatusValue,
    &IPolicyStatusValue_ServerInfo,
    12,
    &IPolicyStatusValue_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPolicyStatusValueUser, ver. 0.0,
   GUID={0x7E0A6B39,0x7CEB,0x4944,{0xAB,0xFA,0xF4,0x19,0xD2,0x01,0xD6,0xA0}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatusValueUser_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    174,
    2282,
    1026,
    2324
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatusValueUser_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatusValueUser_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatusValueUser_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatusValueUser_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(12) _IPolicyStatusValueUserProxyVtbl = 
{
    &IPolicyStatusValueUser_ProxyInfo,
    &IID_IPolicyStatusValueUser,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusValueUser::get_source */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusValueUser::get_value */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusValueUser::get_hasConflict */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusValueUser::get_conflictSource */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusValueUser::get_conflictValue */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IPolicyStatusValueUser_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IPolicyStatusValueUserStubVtbl =
{
    &IID_IPolicyStatusValueUser,
    &IPolicyStatusValueUser_ServerInfo,
    12,
    &IPolicyStatusValueUser_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPolicyStatusValueSystem, ver. 0.0,
   GUID={0xCC2CCD05,0x119C,0x44E1,{0x85,0x2D,0x6D,0xCC,0x2D,0xFB,0x72,0xEC}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatusValueSystem_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    174,
    2282,
    1026,
    2324
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatusValueSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatusValueSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatusValueSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatusValueSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(12) _IPolicyStatusValueSystemProxyVtbl = 
{
    &IPolicyStatusValueSystem_ProxyInfo,
    &IID_IPolicyStatusValueSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusValueSystem::get_source */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusValueSystem::get_value */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusValueSystem::get_hasConflict */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusValueSystem::get_conflictSource */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatusValueSystem::get_conflictValue */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IPolicyStatusValueSystem_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IPolicyStatusValueSystemStubVtbl =
{
    &IID_IPolicyStatusValueSystem,
    &IPolicyStatusValueSystem_ServerInfo,
    12,
    &IPolicyStatusValueSystem_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPolicyStatus2, ver. 0.0,
   GUID={0x06A6AA1E,0x2680,0x4076,{0xA7,0xCD,0x60,0x53,0x72,0x2C,0xF4,0x54}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatus2_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    2366,
    992,
    2408,
    2450,
    2498,
    2540,
    2582,
    2624,
    2666,
    2708,
    2750,
    2798,
    2846,
    2894,
    2942
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatus2_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatus2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(23) _IPolicyStatus2ProxyVtbl = 
{
    &IPolicyStatus2_ProxyInfo,
    &IID_IPolicyStatus2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_updaterVersion */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_lastCheckedTime */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::refreshPolicies */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_lastCheckPeriodMinutes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_updatesSuppressedTimes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_downloadPreferenceGroupPolicy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_packageCacheSizeLimitMBytes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_packageCacheExpirationTimeDays */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_proxyMode */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_proxyPacUrl */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_proxyServer */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_effectivePolicyForAppInstalls */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_effectivePolicyForAppUpdates */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_targetVersionPrefix */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_isRollbackToTargetVersionAllowed */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_targetChannel */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IPolicyStatus2_table[] =
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

CInterfaceStubVtbl _IPolicyStatus2StubVtbl =
{
    &IID_IPolicyStatus2,
    &IPolicyStatus2_ServerInfo,
    23,
    &IPolicyStatus2_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPolicyStatus2User, ver. 0.0,
   GUID={0xAD91C851,0x86AC,0x499F,{0x9B,0xA9,0x9A,0x56,0x17,0x44,0xAA,0x4D}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatus2User_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    2366,
    992,
    2990,
    3032,
    3080,
    3122,
    3164,
    3206,
    3248,
    3290,
    3332,
    3380,
    3428,
    3476,
    3524
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatus2User_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus2User_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatus2User_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus2User_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(23) _IPolicyStatus2UserProxyVtbl = 
{
    &IPolicyStatus2User_ProxyInfo,
    &IID_IPolicyStatus2User,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_updaterVersion */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_lastCheckedTime */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::refreshPolicies */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_lastCheckPeriodMinutes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_updatesSuppressedTimes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_downloadPreferenceGroupPolicy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_packageCacheSizeLimitMBytes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_packageCacheExpirationTimeDays */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_proxyMode */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_proxyPacUrl */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_proxyServer */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_effectivePolicyForAppInstalls */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_effectivePolicyForAppUpdates */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_targetVersionPrefix */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_isRollbackToTargetVersionAllowed */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_targetChannel */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IPolicyStatus2User_table[] =
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

CInterfaceStubVtbl _IPolicyStatus2UserStubVtbl =
{
    &IID_IPolicyStatus2User,
    &IPolicyStatus2User_ServerInfo,
    23,
    &IPolicyStatus2User_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPolicyStatus2System, ver. 0.0,
   GUID={0xF4A0362A,0x3702,0x48B8,{0x98,0x96,0x7D,0x80,0x13,0xD0,0x3A,0xB2}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatus2System_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    2366,
    992,
    3572,
    3614,
    3662,
    3704,
    3746,
    3788,
    3830,
    3872,
    3914,
    3962,
    4010,
    4058,
    4106
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatus2System_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus2System_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatus2System_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus2System_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(23) _IPolicyStatus2SystemProxyVtbl = 
{
    &IPolicyStatus2System_ProxyInfo,
    &IID_IPolicyStatus2System,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_updaterVersion */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_lastCheckedTime */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::refreshPolicies */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_lastCheckPeriodMinutes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_updatesSuppressedTimes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_downloadPreferenceGroupPolicy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_packageCacheSizeLimitMBytes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_packageCacheExpirationTimeDays */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_proxyMode */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_proxyPacUrl */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_proxyServer */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_effectivePolicyForAppInstalls */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_effectivePolicyForAppUpdates */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_targetVersionPrefix */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_isRollbackToTargetVersionAllowed */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_targetChannel */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IPolicyStatus2System_table[] =
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

CInterfaceStubVtbl _IPolicyStatus2SystemStubVtbl =
{
    &IID_IPolicyStatus2System,
    &IPolicyStatus2System_ServerInfo,
    23,
    &IPolicyStatus2System_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPolicyStatus3, ver. 0.0,
   GUID={0x029BD175,0x5035,0x4E2A,{0x87,0x24,0xC9,0xD4,0x7F,0x4F,0xAE,0xA3}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatus3_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    2366,
    992,
    2408,
    2450,
    2498,
    2540,
    2582,
    2624,
    2666,
    2708,
    2750,
    2798,
    2846,
    2894,
    2942,
    4154
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatus3_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus3_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatus3_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus3_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(24) _IPolicyStatus3ProxyVtbl = 
{
    &IPolicyStatus3_ProxyInfo,
    &IID_IPolicyStatus3,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_updaterVersion */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_lastCheckedTime */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::refreshPolicies */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_lastCheckPeriodMinutes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_updatesSuppressedTimes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_downloadPreferenceGroupPolicy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_packageCacheSizeLimitMBytes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_packageCacheExpirationTimeDays */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_proxyMode */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_proxyPacUrl */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_proxyServer */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_effectivePolicyForAppInstalls */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_effectivePolicyForAppUpdates */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_targetVersionPrefix */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_isRollbackToTargetVersionAllowed */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_targetChannel */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus3::get_forceInstallApps */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IPolicyStatus3_table[] =
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
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IPolicyStatus3StubVtbl =
{
    &IID_IPolicyStatus3,
    &IPolicyStatus3_ServerInfo,
    24,
    &IPolicyStatus3_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPolicyStatus3User, ver. 0.0,
   GUID={0xBC39E1E1,0xE8FA,0x4E72,{0x90,0x3F,0x3B,0xF3,0x46,0xE7,0xE1,0x65}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatus3User_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    2366,
    992,
    2990,
    3032,
    3080,
    3122,
    3164,
    3206,
    3248,
    3290,
    3332,
    3380,
    3428,
    3476,
    3524,
    4202
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatus3User_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus3User_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatus3User_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus3User_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(24) _IPolicyStatus3UserProxyVtbl = 
{
    &IPolicyStatus3User_ProxyInfo,
    &IID_IPolicyStatus3User,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_updaterVersion */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_lastCheckedTime */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::refreshPolicies */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_lastCheckPeriodMinutes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_updatesSuppressedTimes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_downloadPreferenceGroupPolicy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_packageCacheSizeLimitMBytes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_packageCacheExpirationTimeDays */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_proxyMode */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_proxyPacUrl */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_proxyServer */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_effectivePolicyForAppInstalls */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_effectivePolicyForAppUpdates */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_targetVersionPrefix */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_isRollbackToTargetVersionAllowed */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_targetChannel */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus3User::get_forceInstallApps */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IPolicyStatus3User_table[] =
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
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IPolicyStatus3UserStubVtbl =
{
    &IID_IPolicyStatus3User,
    &IPolicyStatus3User_ServerInfo,
    24,
    &IPolicyStatus3User_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPolicyStatus3System, ver. 0.0,
   GUID={0x7B26CC23,0xB2B8,0x441B,{0xAA,0x9C,0x8B,0x55,0x1A,0xBB,0x61,0x1B}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatus3System_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    2366,
    992,
    3572,
    3614,
    3662,
    3704,
    3746,
    3788,
    3830,
    3872,
    3914,
    3962,
    4010,
    4058,
    4106,
    4250
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatus3System_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus3System_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatus3System_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus3System_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(24) _IPolicyStatus3SystemProxyVtbl = 
{
    &IPolicyStatus3System_ProxyInfo,
    &IID_IPolicyStatus3System,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_updaterVersion */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_lastCheckedTime */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::refreshPolicies */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_lastCheckPeriodMinutes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_updatesSuppressedTimes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_downloadPreferenceGroupPolicy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_packageCacheSizeLimitMBytes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_packageCacheExpirationTimeDays */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_proxyMode */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_proxyPacUrl */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_proxyServer */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_effectivePolicyForAppInstalls */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_effectivePolicyForAppUpdates */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_targetVersionPrefix */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_isRollbackToTargetVersionAllowed */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_targetChannel */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus3System::get_forceInstallApps */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IPolicyStatus3System_table[] =
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
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IPolicyStatus3SystemStubVtbl =
{
    &IID_IPolicyStatus3System,
    &IPolicyStatus3System_ServerInfo,
    24,
    &IPolicyStatus3System_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPolicyStatus4, ver. 0.0,
   GUID={0xC07BC046,0x32E0,0x4184,{0xBC,0x9F,0x13,0xC4,0x53,0x3C,0x24,0xAC}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatus4_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    2366,
    992,
    2408,
    2450,
    2498,
    2540,
    2582,
    2624,
    2666,
    2708,
    2750,
    2798,
    2846,
    2894,
    2942,
    4154,
    4298
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatus4_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus4_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatus4_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus4_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(25) _IPolicyStatus4ProxyVtbl = 
{
    &IPolicyStatus4_ProxyInfo,
    &IID_IPolicyStatus4,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_updaterVersion */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_lastCheckedTime */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::refreshPolicies */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_lastCheckPeriodMinutes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_updatesSuppressedTimes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_downloadPreferenceGroupPolicy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_packageCacheSizeLimitMBytes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_packageCacheExpirationTimeDays */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_proxyMode */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_proxyPacUrl */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_proxyServer */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_effectivePolicyForAppInstalls */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_effectivePolicyForAppUpdates */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_targetVersionPrefix */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_isRollbackToTargetVersionAllowed */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2::get_targetChannel */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus3::get_forceInstallApps */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus4::get_cloudPolicyOverridesPlatformPolicy */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IPolicyStatus4_table[] =
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
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IPolicyStatus4StubVtbl =
{
    &IID_IPolicyStatus4,
    &IPolicyStatus4_ServerInfo,
    25,
    &IPolicyStatus4_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPolicyStatus4User, ver. 0.0,
   GUID={0x0F6696F3,0x7F48,0x446B,{0x97,0xFA,0x6B,0x34,0xEC,0x2A,0xDB,0x32}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatus4User_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    2366,
    992,
    2990,
    3032,
    3080,
    3122,
    3164,
    3206,
    3248,
    3290,
    3332,
    3380,
    3428,
    3476,
    3524,
    4202,
    4340
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatus4User_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus4User_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatus4User_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus4User_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(25) _IPolicyStatus4UserProxyVtbl = 
{
    &IPolicyStatus4User_ProxyInfo,
    &IID_IPolicyStatus4User,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_updaterVersion */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_lastCheckedTime */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::refreshPolicies */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_lastCheckPeriodMinutes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_updatesSuppressedTimes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_downloadPreferenceGroupPolicy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_packageCacheSizeLimitMBytes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_packageCacheExpirationTimeDays */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_proxyMode */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_proxyPacUrl */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_proxyServer */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_effectivePolicyForAppInstalls */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_effectivePolicyForAppUpdates */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_targetVersionPrefix */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_isRollbackToTargetVersionAllowed */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2User::get_targetChannel */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus3User::get_forceInstallApps */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus4User::get_cloudPolicyOverridesPlatformPolicy */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IPolicyStatus4User_table[] =
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
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IPolicyStatus4UserStubVtbl =
{
    &IID_IPolicyStatus4User,
    &IPolicyStatus4User_ServerInfo,
    25,
    &IPolicyStatus4User_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPolicyStatus4System, ver. 0.0,
   GUID={0x423FDEC3,0x0DBC,0x441E,{0xB5,0x1D,0xFD,0x8B,0x82,0xB9,0xDC,0xF2}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatus4System_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    2366,
    992,
    3572,
    3614,
    3662,
    3704,
    3746,
    3788,
    3830,
    3872,
    3914,
    3962,
    4010,
    4058,
    4106,
    4250,
    4382
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatus4System_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus4System_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatus4System_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus4System_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(25) _IPolicyStatus4SystemProxyVtbl = 
{
    &IPolicyStatus4System_ProxyInfo,
    &IID_IPolicyStatus4System,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_updaterVersion */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_lastCheckedTime */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::refreshPolicies */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_lastCheckPeriodMinutes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_updatesSuppressedTimes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_downloadPreferenceGroupPolicy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_packageCacheSizeLimitMBytes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_packageCacheExpirationTimeDays */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_proxyMode */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_proxyPacUrl */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_proxyServer */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_effectivePolicyForAppInstalls */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_effectivePolicyForAppUpdates */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_targetVersionPrefix */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_isRollbackToTargetVersionAllowed */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus2System::get_targetChannel */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus3System::get_forceInstallApps */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus4System::get_cloudPolicyOverridesPlatformPolicy */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IPolicyStatus4System_table[] =
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
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IPolicyStatus4SystemStubVtbl =
{
    &IID_IPolicyStatus4System,
    &IPolicyStatus4System_ServerInfo,
    25,
    &IPolicyStatus4System_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IProcessLauncher, ver. 0.0,
   GUID={0x4779D540,0xF6A3,0x455F,{0xA9,0x29,0x7A,0xDF,0xE8,0x5B,0x6F,0x09}} */

#pragma code_seg(".orpc")
static const unsigned short IProcessLauncher_FormatStringOffsetTable[] =
    {
    4424,
    4466,
    4514
    };

static const MIDL_STUBLESS_PROXY_INFO IProcessLauncher_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IProcessLauncher_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IProcessLauncher_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IProcessLauncher_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(6) _IProcessLauncherProxyVtbl = 
{
    &IProcessLauncher_ProxyInfo,
    &IID_IProcessLauncher,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IProcessLauncher::LaunchCmdLine */ ,
    (void *) (INT_PTR) -1 /* IProcessLauncher::LaunchBrowser */ ,
    (void *) (INT_PTR) -1 /* IProcessLauncher::LaunchCmdElevated */
};

const CInterfaceStubVtbl _IProcessLauncherStubVtbl =
{
    &IID_IProcessLauncher,
    &IProcessLauncher_ServerInfo,
    6,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IProcessLauncherSystem, ver. 0.0,
   GUID={0xFFBAEC45,0xC5EC,0x4287,{0x85,0xCD,0xA8,0x31,0x79,0x6B,0xE9,0x52}} */

#pragma code_seg(".orpc")
static const unsigned short IProcessLauncherSystem_FormatStringOffsetTable[] =
    {
    4424,
    4466,
    4514
    };

static const MIDL_STUBLESS_PROXY_INFO IProcessLauncherSystem_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IProcessLauncherSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IProcessLauncherSystem_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IProcessLauncherSystem_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(6) _IProcessLauncherSystemProxyVtbl = 
{
    &IProcessLauncherSystem_ProxyInfo,
    &IID_IProcessLauncherSystem,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IProcessLauncherSystem::LaunchCmdLine */ ,
    (void *) (INT_PTR) -1 /* IProcessLauncherSystem::LaunchBrowser */ ,
    (void *) (INT_PTR) -1 /* IProcessLauncherSystem::LaunchCmdElevated */
};

const CInterfaceStubVtbl _IProcessLauncherSystemStubVtbl =
{
    &IID_IProcessLauncherSystem,
    &IProcessLauncherSystem_ServerInfo,
    6,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IProcessLauncher2, ver. 0.0,
   GUID={0x74F243B8,0x75D1,0x4E2D,{0xBC,0x89,0x56,0x89,0x79,0x8E,0xEF,0x3E}} */

#pragma code_seg(".orpc")
static const unsigned short IProcessLauncher2_FormatStringOffsetTable[] =
    {
    4424,
    4466,
    4514,
    4576
    };

static const MIDL_STUBLESS_PROXY_INFO IProcessLauncher2_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IProcessLauncher2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IProcessLauncher2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IProcessLauncher2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(7) _IProcessLauncher2ProxyVtbl = 
{
    &IProcessLauncher2_ProxyInfo,
    &IID_IProcessLauncher2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IProcessLauncher::LaunchCmdLine */ ,
    (void *) (INT_PTR) -1 /* IProcessLauncher::LaunchBrowser */ ,
    (void *) (INT_PTR) -1 /* IProcessLauncher::LaunchCmdElevated */ ,
    (void *) (INT_PTR) -1 /* IProcessLauncher2::LaunchCmdLineEx */
};

const CInterfaceStubVtbl _IProcessLauncher2StubVtbl =
{
    &IID_IProcessLauncher2,
    &IProcessLauncher2_ServerInfo,
    7,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IProcessLauncher2System, ver. 0.0,
   GUID={0x5F41DC50,0x029C,0x4F5A,{0x98,0x60,0xEF,0x35,0x2A,0x0B,0x66,0xD2}} */

#pragma code_seg(".orpc")
static const unsigned short IProcessLauncher2System_FormatStringOffsetTable[] =
    {
    4424,
    4466,
    4514,
    4576
    };

static const MIDL_STUBLESS_PROXY_INFO IProcessLauncher2System_ProxyInfo =
    {
    &Object_StubDesc,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IProcessLauncher2System_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IProcessLauncher2System_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_legacy_idl__MIDL_ProcFormatString.Format,
    &IProcessLauncher2System_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(7) _IProcessLauncher2SystemProxyVtbl = 
{
    &IProcessLauncher2System_ProxyInfo,
    &IID_IProcessLauncher2System,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IProcessLauncherSystem::LaunchCmdLine */ ,
    (void *) (INT_PTR) -1 /* IProcessLauncherSystem::LaunchBrowser */ ,
    (void *) (INT_PTR) -1 /* IProcessLauncherSystem::LaunchCmdElevated */ ,
    (void *) (INT_PTR) -1 /* IProcessLauncher2System::LaunchCmdLineEx */
};

const CInterfaceStubVtbl _IProcessLauncher2SystemStubVtbl =
{
    &IID_IProcessLauncher2System,
    &IProcessLauncher2System_ServerInfo,
    7,
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
    updater_legacy_idl__MIDL_TypeFormatString.Format,
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

const CInterfaceProxyVtbl * const _updater_legacy_idl_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_IPolicyStatusValueSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppWebUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPolicyStatusUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppVersionWebUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ICurrentStateUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppVersionWebSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPolicyStatus2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPolicyStatus3SystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPolicyStatus2SystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IGoogleUpdate3WebUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPolicyStatusValueUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppCommandWebProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IProcessLauncherProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IProcessLauncherSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPolicyStatus4ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IProcessLauncher2SystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPolicyStatus2UserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IGoogleUpdate3WebProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPolicyStatusSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppBundleWebSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppCommandWebUserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPolicyStatus3ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPolicyStatusProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppWebSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppVersionWebProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ICurrentStateProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IGoogleUpdate3WebSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IProcessLauncher2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppBundleWebProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ICurrentStateSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPolicyStatus4SystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppCommandWebSystemProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppWebProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPolicyStatus3UserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPolicyStatusValueProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPolicyStatus4UserProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppBundleWebUserProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _updater_legacy_idl_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_IPolicyStatusValueSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppWebUserStubVtbl,
    ( CInterfaceStubVtbl *) &_IPolicyStatusUserStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppVersionWebUserStubVtbl,
    ( CInterfaceStubVtbl *) &_ICurrentStateUserStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppVersionWebSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IPolicyStatus2StubVtbl,
    ( CInterfaceStubVtbl *) &_IPolicyStatus3SystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IPolicyStatus2SystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IGoogleUpdate3WebUserStubVtbl,
    ( CInterfaceStubVtbl *) &_IPolicyStatusValueUserStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppCommandWebStubVtbl,
    ( CInterfaceStubVtbl *) &_IProcessLauncherStubVtbl,
    ( CInterfaceStubVtbl *) &_IProcessLauncherSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IPolicyStatus4StubVtbl,
    ( CInterfaceStubVtbl *) &_IProcessLauncher2SystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IPolicyStatus2UserStubVtbl,
    ( CInterfaceStubVtbl *) &_IGoogleUpdate3WebStubVtbl,
    ( CInterfaceStubVtbl *) &_IPolicyStatusSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppBundleWebSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppCommandWebUserStubVtbl,
    ( CInterfaceStubVtbl *) &_IPolicyStatus3StubVtbl,
    ( CInterfaceStubVtbl *) &_IPolicyStatusStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppWebSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppVersionWebStubVtbl,
    ( CInterfaceStubVtbl *) &_ICurrentStateStubVtbl,
    ( CInterfaceStubVtbl *) &_IGoogleUpdate3WebSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IProcessLauncher2StubVtbl,
    ( CInterfaceStubVtbl *) &_IAppBundleWebStubVtbl,
    ( CInterfaceStubVtbl *) &_ICurrentStateSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IPolicyStatus4SystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppCommandWebSystemStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppWebStubVtbl,
    ( CInterfaceStubVtbl *) &_IPolicyStatus3UserStubVtbl,
    ( CInterfaceStubVtbl *) &_IPolicyStatusValueStubVtbl,
    ( CInterfaceStubVtbl *) &_IPolicyStatus4UserStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppBundleWebUserStubVtbl,
    0
};

PCInterfaceName const _updater_legacy_idl_InterfaceNamesList[] = 
{
    "IPolicyStatusValueSystem",
    "IAppWebUser",
    "IPolicyStatusUser",
    "IAppVersionWebUser",
    "ICurrentStateUser",
    "IAppVersionWebSystem",
    "IPolicyStatus2",
    "IPolicyStatus3System",
    "IPolicyStatus2System",
    "IGoogleUpdate3WebUser",
    "IPolicyStatusValueUser",
    "IAppCommandWeb",
    "IProcessLauncher",
    "IProcessLauncherSystem",
    "IPolicyStatus4",
    "IProcessLauncher2System",
    "IPolicyStatus2User",
    "IGoogleUpdate3Web",
    "IPolicyStatusSystem",
    "IAppBundleWebSystem",
    "IAppCommandWebUser",
    "IPolicyStatus3",
    "IPolicyStatus",
    "IAppWebSystem",
    "IAppVersionWeb",
    "ICurrentState",
    "IGoogleUpdate3WebSystem",
    "IProcessLauncher2",
    "IAppBundleWeb",
    "ICurrentStateSystem",
    "IPolicyStatus4System",
    "IAppCommandWebSystem",
    "IAppWeb",
    "IPolicyStatus3User",
    "IPolicyStatusValue",
    "IPolicyStatus4User",
    "IAppBundleWebUser",
    0
};

const IID *  const _updater_legacy_idl_BaseIIDList[] = 
{
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    0,
    0,
    &IID_IDispatch,
    0,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    0,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    0
};


#define _updater_legacy_idl_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _updater_legacy_idl, pIID, n)

int __stdcall _updater_legacy_idl_IID_Lookup( const IID * pIID, int * pIndex )
{
    IID_BS_LOOKUP_SETUP

    IID_BS_LOOKUP_INITIAL_TEST( _updater_legacy_idl, 37, 32 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_legacy_idl, 16 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_legacy_idl, 8 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_legacy_idl, 4 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_legacy_idl, 2 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_legacy_idl, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _updater_legacy_idl, 37, *pIndex )
    
}

EXTERN_C const ExtendedProxyFileInfo updater_legacy_idl_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _updater_legacy_idl_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _updater_legacy_idl_StubVtblList,
    (const PCInterfaceName * ) & _updater_legacy_idl_InterfaceNamesList,
    (const IID ** ) & _updater_legacy_idl_BaseIIDList,
    & _updater_legacy_idl_IID_Lookup, 
    37,
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

