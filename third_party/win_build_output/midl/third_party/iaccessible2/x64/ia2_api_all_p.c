

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../third_party/iaccessible2/ia2_api_all.idl:
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


#include "ia2_api_all.h"

#define TYPE_FORMAT_STRING_SIZE   1641                              
#define PROC_FORMAT_STRING_SIZE   5621                              
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   3            

typedef struct _ia2_api_all_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } ia2_api_all_MIDL_TYPE_FORMAT_STRING;

typedef struct _ia2_api_all_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } ia2_api_all_MIDL_PROC_FORMAT_STRING;

typedef struct _ia2_api_all_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } ia2_api_all_MIDL_EXPR_FORMAT_STRING;


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


extern const ia2_api_all_MIDL_TYPE_FORMAT_STRING ia2_api_all__MIDL_TypeFormatString;
extern const ia2_api_all_MIDL_PROC_FORMAT_STRING ia2_api_all__MIDL_ProcFormatString;
extern const ia2_api_all_MIDL_EXPR_FORMAT_STRING ia2_api_all__MIDL_ExprFormatString;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleRelation_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleRelation_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleAction_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleAction_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessible2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessible2_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessible2_2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessible2_2_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessible2_3_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessible2_3_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessible2_4_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessible2_4_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleComponent_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleComponent_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleValue_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleValue_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleText_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleText_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleText2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleText2_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleTextSelectionContainer_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleTextSelectionContainer_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleEditableText_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleEditableText_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleHyperlink_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleHyperlink_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleHypertext_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleHypertext_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleHypertext2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleHypertext2_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleTable_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleTable_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleTable2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleTable2_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleTableCell_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleTableCell_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleImage_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleImage_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleApplication_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleApplication_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IAccessibleDocument_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleDocument_ProxyInfo;


extern const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ];

#if !defined(__RPC_WIN64__)
#error  Invalid build platform for this stub.
#endif

static const ia2_api_all_MIDL_PROC_FORMAT_STRING ia2_api_all__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure get_appName */


	/* Procedure get_description */


	/* Procedure get_relationType */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x3 ),	/* 3 */
/*  8 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 10 */	NdrFcShort( 0x0 ),	/* 0 */
/* 12 */	NdrFcShort( 0x8 ),	/* 8 */
/* 14 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 16 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 18 */	NdrFcShort( 0x1 ),	/* 1 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter name */


	/* Parameter description */


	/* Parameter relationType */

/* 26 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 28 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 30 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 32 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 34 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 36 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_appVersion */


	/* Procedure get_localizedRelationType */

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

	/* Parameter version */


	/* Parameter localizedRelationType */

/* 64 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 66 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 68 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */

/* 70 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 72 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 74 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnIndex */


	/* Procedure get_caretOffset */


	/* Procedure get_background */


	/* Procedure get_nTargets */

/* 76 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 78 */	NdrFcLong( 0x0 ),	/* 0 */
/* 82 */	NdrFcShort( 0x5 ),	/* 5 */
/* 84 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 86 */	NdrFcShort( 0x0 ),	/* 0 */
/* 88 */	NdrFcShort( 0x24 ),	/* 36 */
/* 90 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 92 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 94 */	NdrFcShort( 0x0 ),	/* 0 */
/* 96 */	NdrFcShort( 0x0 ),	/* 0 */
/* 98 */	NdrFcShort( 0x0 ),	/* 0 */
/* 100 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter columnIndex */


	/* Parameter offset */


	/* Parameter background */


	/* Parameter nTargets */

/* 102 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 104 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 106 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 108 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 110 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 112 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_target */

/* 114 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 116 */	NdrFcLong( 0x0 ),	/* 0 */
/* 120 */	NdrFcShort( 0x6 ),	/* 6 */
/* 122 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 124 */	NdrFcShort( 0x8 ),	/* 8 */
/* 126 */	NdrFcShort( 0x8 ),	/* 8 */
/* 128 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 130 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 132 */	NdrFcShort( 0x0 ),	/* 0 */
/* 134 */	NdrFcShort( 0x0 ),	/* 0 */
/* 136 */	NdrFcShort( 0x0 ),	/* 0 */
/* 138 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter targetIndex */

/* 140 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 142 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 144 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter target */

/* 146 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 148 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 150 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 152 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 154 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 156 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_targets */

/* 158 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 160 */	NdrFcLong( 0x0 ),	/* 0 */
/* 164 */	NdrFcShort( 0x7 ),	/* 7 */
/* 166 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 168 */	NdrFcShort( 0x8 ),	/* 8 */
/* 170 */	NdrFcShort( 0x24 ),	/* 36 */
/* 172 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 174 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 176 */	NdrFcShort( 0x1 ),	/* 1 */
/* 178 */	NdrFcShort( 0x0 ),	/* 0 */
/* 180 */	NdrFcShort( 0x0 ),	/* 0 */
/* 182 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter maxTargets */

/* 184 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 186 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 188 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter targets */

/* 190 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 192 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 194 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter nTargets */

/* 196 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 198 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 200 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 202 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 204 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 206 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnExtent */


	/* Procedure nActions */

/* 208 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 210 */	NdrFcLong( 0x0 ),	/* 0 */
/* 214 */	NdrFcShort( 0x3 ),	/* 3 */
/* 216 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 218 */	NdrFcShort( 0x0 ),	/* 0 */
/* 220 */	NdrFcShort( 0x24 ),	/* 36 */
/* 222 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 224 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 226 */	NdrFcShort( 0x0 ),	/* 0 */
/* 228 */	NdrFcShort( 0x0 ),	/* 0 */
/* 230 */	NdrFcShort( 0x0 ),	/* 0 */
/* 232 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter nColumnsSpanned */


	/* Parameter nActions */

/* 234 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 236 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 238 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 240 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 242 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 244 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure doAction */

/* 246 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 248 */	NdrFcLong( 0x0 ),	/* 0 */
/* 252 */	NdrFcShort( 0x4 ),	/* 4 */
/* 254 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 256 */	NdrFcShort( 0x8 ),	/* 8 */
/* 258 */	NdrFcShort( 0x8 ),	/* 8 */
/* 260 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 262 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 264 */	NdrFcShort( 0x0 ),	/* 0 */
/* 266 */	NdrFcShort( 0x0 ),	/* 0 */
/* 268 */	NdrFcShort( 0x0 ),	/* 0 */
/* 270 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter actionIndex */

/* 272 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 274 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 276 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 278 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 280 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 282 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnDescription */


	/* Procedure get_description */

/* 284 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 286 */	NdrFcLong( 0x0 ),	/* 0 */
/* 290 */	NdrFcShort( 0x5 ),	/* 5 */
/* 292 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 294 */	NdrFcShort( 0x8 ),	/* 8 */
/* 296 */	NdrFcShort( 0x8 ),	/* 8 */
/* 298 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 300 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 302 */	NdrFcShort( 0x1 ),	/* 1 */
/* 304 */	NdrFcShort( 0x0 ),	/* 0 */
/* 306 */	NdrFcShort( 0x0 ),	/* 0 */
/* 308 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter column */


	/* Parameter actionIndex */

/* 310 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 312 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 314 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter description */


	/* Parameter description */

/* 316 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 318 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 320 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */

/* 322 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 324 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 326 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_keyBinding */

/* 328 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 330 */	NdrFcLong( 0x0 ),	/* 0 */
/* 334 */	NdrFcShort( 0x6 ),	/* 6 */
/* 336 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 338 */	NdrFcShort( 0x10 ),	/* 16 */
/* 340 */	NdrFcShort( 0x24 ),	/* 36 */
/* 342 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x5,		/* 5 */
/* 344 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 346 */	NdrFcShort( 0x1 ),	/* 1 */
/* 348 */	NdrFcShort( 0x0 ),	/* 0 */
/* 350 */	NdrFcShort( 0x0 ),	/* 0 */
/* 352 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter actionIndex */

/* 354 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 356 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 358 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter nMaxBindings */

/* 360 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 362 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 364 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter keyBindings */

/* 366 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 368 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 370 */	NdrFcShort( 0x5e ),	/* Type Offset=94 */

	/* Parameter nBindings */

/* 372 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 374 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 376 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 378 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 380 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 382 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_name */

/* 384 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 386 */	NdrFcLong( 0x0 ),	/* 0 */
/* 390 */	NdrFcShort( 0x7 ),	/* 7 */
/* 392 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 394 */	NdrFcShort( 0x8 ),	/* 8 */
/* 396 */	NdrFcShort( 0x8 ),	/* 8 */
/* 398 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 400 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 402 */	NdrFcShort( 0x1 ),	/* 1 */
/* 404 */	NdrFcShort( 0x0 ),	/* 0 */
/* 406 */	NdrFcShort( 0x0 ),	/* 0 */
/* 408 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter actionIndex */

/* 410 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 412 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 414 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter name */

/* 416 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 418 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 420 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 422 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 424 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 426 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_localizedName */

/* 428 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 430 */	NdrFcLong( 0x0 ),	/* 0 */
/* 434 */	NdrFcShort( 0x8 ),	/* 8 */
/* 436 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 438 */	NdrFcShort( 0x8 ),	/* 8 */
/* 440 */	NdrFcShort( 0x8 ),	/* 8 */
/* 442 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 444 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 446 */	NdrFcShort( 0x1 ),	/* 1 */
/* 448 */	NdrFcShort( 0x0 ),	/* 0 */
/* 450 */	NdrFcShort( 0x0 ),	/* 0 */
/* 452 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter actionIndex */

/* 454 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 456 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 458 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter localizedName */

/* 460 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 462 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 464 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 466 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 468 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 470 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nRelations */

/* 472 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 474 */	NdrFcLong( 0x0 ),	/* 0 */
/* 478 */	NdrFcShort( 0x1c ),	/* 28 */
/* 480 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 482 */	NdrFcShort( 0x0 ),	/* 0 */
/* 484 */	NdrFcShort( 0x24 ),	/* 36 */
/* 486 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 488 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 490 */	NdrFcShort( 0x0 ),	/* 0 */
/* 492 */	NdrFcShort( 0x0 ),	/* 0 */
/* 494 */	NdrFcShort( 0x0 ),	/* 0 */
/* 496 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter nRelations */

/* 498 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 500 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 502 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 504 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 506 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 508 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_relation */

/* 510 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 512 */	NdrFcLong( 0x0 ),	/* 0 */
/* 516 */	NdrFcShort( 0x1d ),	/* 29 */
/* 518 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 520 */	NdrFcShort( 0x8 ),	/* 8 */
/* 522 */	NdrFcShort( 0x8 ),	/* 8 */
/* 524 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 526 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 528 */	NdrFcShort( 0x0 ),	/* 0 */
/* 530 */	NdrFcShort( 0x0 ),	/* 0 */
/* 532 */	NdrFcShort( 0x0 ),	/* 0 */
/* 534 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter relationIndex */

/* 536 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 538 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 540 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter relation */

/* 542 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 544 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 546 */	NdrFcShort( 0x7c ),	/* Type Offset=124 */

	/* Return value */

/* 548 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 550 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 552 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_relations */

/* 554 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 556 */	NdrFcLong( 0x0 ),	/* 0 */
/* 560 */	NdrFcShort( 0x1e ),	/* 30 */
/* 562 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 564 */	NdrFcShort( 0x8 ),	/* 8 */
/* 566 */	NdrFcShort( 0x24 ),	/* 36 */
/* 568 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 570 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 572 */	NdrFcShort( 0x1 ),	/* 1 */
/* 574 */	NdrFcShort( 0x0 ),	/* 0 */
/* 576 */	NdrFcShort( 0x0 ),	/* 0 */
/* 578 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter maxRelations */

/* 580 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 582 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 584 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter relations */

/* 586 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 588 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 590 */	NdrFcShort( 0x96 ),	/* Type Offset=150 */

	/* Parameter nRelations */

/* 592 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 594 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 596 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 598 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 600 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 602 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure role */

/* 604 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 606 */	NdrFcLong( 0x0 ),	/* 0 */
/* 610 */	NdrFcShort( 0x1f ),	/* 31 */
/* 612 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 614 */	NdrFcShort( 0x0 ),	/* 0 */
/* 616 */	NdrFcShort( 0x24 ),	/* 36 */
/* 618 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 620 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 622 */	NdrFcShort( 0x0 ),	/* 0 */
/* 624 */	NdrFcShort( 0x0 ),	/* 0 */
/* 626 */	NdrFcShort( 0x0 ),	/* 0 */
/* 628 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter role */

/* 630 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 632 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 634 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 636 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 638 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 640 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure scrollTo */

/* 642 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 644 */	NdrFcLong( 0x0 ),	/* 0 */
/* 648 */	NdrFcShort( 0x20 ),	/* 32 */
/* 650 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 652 */	NdrFcShort( 0x6 ),	/* 6 */
/* 654 */	NdrFcShort( 0x8 ),	/* 8 */
/* 656 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 658 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 660 */	NdrFcShort( 0x0 ),	/* 0 */
/* 662 */	NdrFcShort( 0x0 ),	/* 0 */
/* 664 */	NdrFcShort( 0x0 ),	/* 0 */
/* 666 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter scrollType */

/* 668 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 670 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 672 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Return value */

/* 674 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 676 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 678 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure scrollToPoint */

/* 680 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 682 */	NdrFcLong( 0x0 ),	/* 0 */
/* 686 */	NdrFcShort( 0x21 ),	/* 33 */
/* 688 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 690 */	NdrFcShort( 0x16 ),	/* 22 */
/* 692 */	NdrFcShort( 0x8 ),	/* 8 */
/* 694 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 696 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 698 */	NdrFcShort( 0x0 ),	/* 0 */
/* 700 */	NdrFcShort( 0x0 ),	/* 0 */
/* 702 */	NdrFcShort( 0x0 ),	/* 0 */
/* 704 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter coordinateType */

/* 706 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 708 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 710 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter x */

/* 712 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 714 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 716 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 718 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 720 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 722 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 724 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 726 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 728 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_groupPosition */

/* 730 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 732 */	NdrFcLong( 0x0 ),	/* 0 */
/* 736 */	NdrFcShort( 0x22 ),	/* 34 */
/* 738 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 740 */	NdrFcShort( 0x0 ),	/* 0 */
/* 742 */	NdrFcShort( 0x5c ),	/* 92 */
/* 744 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 746 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 748 */	NdrFcShort( 0x0 ),	/* 0 */
/* 750 */	NdrFcShort( 0x0 ),	/* 0 */
/* 752 */	NdrFcShort( 0x0 ),	/* 0 */
/* 754 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter groupLevel */

/* 756 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 758 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 760 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter similarItemsInGroup */

/* 762 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 764 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 766 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter positionInGroup */

/* 768 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 770 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 772 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 774 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 776 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 778 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_states */

/* 780 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 782 */	NdrFcLong( 0x0 ),	/* 0 */
/* 786 */	NdrFcShort( 0x23 ),	/* 35 */
/* 788 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 790 */	NdrFcShort( 0x0 ),	/* 0 */
/* 792 */	NdrFcShort( 0x24 ),	/* 36 */
/* 794 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 796 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 798 */	NdrFcShort( 0x0 ),	/* 0 */
/* 800 */	NdrFcShort( 0x0 ),	/* 0 */
/* 802 */	NdrFcShort( 0x0 ),	/* 0 */
/* 804 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter states */

/* 806 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 808 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 810 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 812 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 814 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 816 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_extendedRole */

/* 818 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 820 */	NdrFcLong( 0x0 ),	/* 0 */
/* 824 */	NdrFcShort( 0x24 ),	/* 36 */
/* 826 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 828 */	NdrFcShort( 0x0 ),	/* 0 */
/* 830 */	NdrFcShort( 0x8 ),	/* 8 */
/* 832 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 834 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 836 */	NdrFcShort( 0x1 ),	/* 1 */
/* 838 */	NdrFcShort( 0x0 ),	/* 0 */
/* 840 */	NdrFcShort( 0x0 ),	/* 0 */
/* 842 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter extendedRole */

/* 844 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 846 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 848 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 850 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 852 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 854 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_localizedExtendedRole */

/* 856 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 858 */	NdrFcLong( 0x0 ),	/* 0 */
/* 862 */	NdrFcShort( 0x25 ),	/* 37 */
/* 864 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 866 */	NdrFcShort( 0x0 ),	/* 0 */
/* 868 */	NdrFcShort( 0x8 ),	/* 8 */
/* 870 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 872 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 874 */	NdrFcShort( 0x1 ),	/* 1 */
/* 876 */	NdrFcShort( 0x0 ),	/* 0 */
/* 878 */	NdrFcShort( 0x0 ),	/* 0 */
/* 880 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter localizedExtendedRole */

/* 882 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 884 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 886 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 888 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 890 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 892 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nExtendedStates */

/* 894 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 896 */	NdrFcLong( 0x0 ),	/* 0 */
/* 900 */	NdrFcShort( 0x26 ),	/* 38 */
/* 902 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 904 */	NdrFcShort( 0x0 ),	/* 0 */
/* 906 */	NdrFcShort( 0x24 ),	/* 36 */
/* 908 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 910 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 912 */	NdrFcShort( 0x0 ),	/* 0 */
/* 914 */	NdrFcShort( 0x0 ),	/* 0 */
/* 916 */	NdrFcShort( 0x0 ),	/* 0 */
/* 918 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter nExtendedStates */

/* 920 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 922 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 924 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 926 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 928 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 930 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_extendedStates */

/* 932 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 934 */	NdrFcLong( 0x0 ),	/* 0 */
/* 938 */	NdrFcShort( 0x27 ),	/* 39 */
/* 940 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 942 */	NdrFcShort( 0x8 ),	/* 8 */
/* 944 */	NdrFcShort( 0x24 ),	/* 36 */
/* 946 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 948 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 950 */	NdrFcShort( 0x1 ),	/* 1 */
/* 952 */	NdrFcShort( 0x0 ),	/* 0 */
/* 954 */	NdrFcShort( 0x0 ),	/* 0 */
/* 956 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter maxExtendedStates */

/* 958 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 960 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 962 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter extendedStates */

/* 964 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 966 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 968 */	NdrFcShort( 0xac ),	/* Type Offset=172 */

	/* Parameter nExtendedStates */

/* 970 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 972 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 974 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 976 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 978 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 980 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_localizedExtendedStates */

/* 982 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 984 */	NdrFcLong( 0x0 ),	/* 0 */
/* 988 */	NdrFcShort( 0x28 ),	/* 40 */
/* 990 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 992 */	NdrFcShort( 0x8 ),	/* 8 */
/* 994 */	NdrFcShort( 0x24 ),	/* 36 */
/* 996 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 998 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1000 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1002 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1004 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1006 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter maxLocalizedExtendedStates */

/* 1008 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1010 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1012 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter localizedExtendedStates */

/* 1014 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 1016 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1018 */	NdrFcShort( 0xac ),	/* Type Offset=172 */

	/* Parameter nLocalizedExtendedStates */

/* 1020 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1022 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1024 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1026 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1028 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1030 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_uniqueID */

/* 1032 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1034 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1038 */	NdrFcShort( 0x29 ),	/* 41 */
/* 1040 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1042 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1044 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1046 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 1048 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1050 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1052 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1054 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1056 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter uniqueID */

/* 1058 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1060 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1062 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1064 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1066 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1068 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_windowHandle */

/* 1070 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1072 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1076 */	NdrFcShort( 0x2a ),	/* 42 */
/* 1078 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1080 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1082 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1084 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1086 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1088 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1090 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1092 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1094 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter windowHandle */

/* 1096 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1098 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1100 */	NdrFcShort( 0xe6 ),	/* Type Offset=230 */

	/* Return value */

/* 1102 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1104 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1106 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_indexInParent */

/* 1108 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1110 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1114 */	NdrFcShort( 0x2b ),	/* 43 */
/* 1116 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1118 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1120 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1122 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 1124 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1126 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1128 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1130 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1132 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter indexInParent */

/* 1134 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1136 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1138 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1140 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1142 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1144 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_locale */

/* 1146 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1148 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1152 */	NdrFcShort( 0x2c ),	/* 44 */
/* 1154 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1156 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1158 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1160 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1162 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1164 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1166 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1168 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1170 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter locale */

/* 1172 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1174 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1176 */	NdrFcShort( 0xf4 ),	/* Type Offset=244 */

	/* Return value */

/* 1178 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1180 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1182 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attributes */

/* 1184 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1186 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1190 */	NdrFcShort( 0x2d ),	/* 45 */
/* 1192 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1194 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1196 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1198 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1200 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1202 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1204 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1206 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1208 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter attributes */

/* 1210 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1212 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1214 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 1216 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1218 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1220 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attribute */

/* 1222 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1224 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1228 */	NdrFcShort( 0x2e ),	/* 46 */
/* 1230 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1232 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1234 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1236 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 1238 */	0xa,		/* 10 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 1240 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1242 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1244 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1246 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter name */

/* 1248 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1250 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1252 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Parameter attribute */

/* 1254 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1256 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1258 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 1260 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1262 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1264 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_accessibleWithCaret */

/* 1266 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1268 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1272 */	NdrFcShort( 0x2f ),	/* 47 */
/* 1274 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1276 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1278 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1280 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 1282 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1284 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1286 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1288 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1290 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter accessible */

/* 1292 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1294 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1296 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Parameter caretOffset */

/* 1298 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1300 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1302 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1304 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1306 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1308 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_relationTargetsOfType */

/* 1310 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1312 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1316 */	NdrFcShort( 0x30 ),	/* 48 */
/* 1318 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1320 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1322 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1324 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 1326 */	0xa,		/* 10 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 1328 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1330 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1332 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1334 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter type */

/* 1336 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1338 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1340 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Parameter maxTargets */

/* 1342 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1344 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1346 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter targets */

/* 1348 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 1350 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1352 */	NdrFcShort( 0x4c6 ),	/* Type Offset=1222 */

	/* Parameter nTargets */

/* 1354 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1356 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1358 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1360 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1362 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1364 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectionRanges */

/* 1366 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1368 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1372 */	NdrFcShort( 0x31 ),	/* 49 */
/* 1374 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1376 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1378 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1380 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 1382 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1384 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1386 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1388 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1390 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter ranges */

/* 1392 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 1394 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1396 */	NdrFcShort( 0x4e4 ),	/* Type Offset=1252 */

	/* Parameter nRanges */

/* 1398 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1400 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1402 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1404 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1406 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1408 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setSelectionRanges */

/* 1410 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1412 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1416 */	NdrFcShort( 0x32 ),	/* 50 */
/* 1418 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1420 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1422 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1424 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 1426 */	0xa,		/* 10 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1428 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1430 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1432 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1434 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter nRanges */

/* 1436 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1438 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1440 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter ranges */

/* 1442 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1444 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1446 */	NdrFcShort( 0x51c ),	/* Type Offset=1308 */

	/* Return value */

/* 1448 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1450 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1452 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_locationInParent */

/* 1454 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1456 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1460 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1462 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1464 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1466 */	NdrFcShort( 0x40 ),	/* 64 */
/* 1468 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 1470 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1472 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1474 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1476 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1478 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter x */

/* 1480 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1482 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1484 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 1486 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1488 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1490 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1492 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1494 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1496 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_foreground */

/* 1498 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1500 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1504 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1506 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1508 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1510 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1512 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 1514 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1516 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1518 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1520 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1522 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter foreground */

/* 1524 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1526 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1528 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1530 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1532 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1534 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_currentValue */

/* 1536 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1538 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1542 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1544 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1546 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1548 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1550 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1552 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1554 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1556 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1558 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1560 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter currentValue */

/* 1562 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1564 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1566 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 1568 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1570 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1572 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setCurrentValue */

/* 1574 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1576 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1580 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1582 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1584 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1586 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1588 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1590 */	0xa,		/* 10 */
			0x85,		/* Ext Flags:  new corr desc, srv corr check, has big byval param */
/* 1592 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1594 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1596 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1598 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter value */

/* 1600 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1602 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1604 */	NdrFcShort( 0x53a ),	/* Type Offset=1338 */

	/* Return value */

/* 1606 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1608 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1610 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_maximumValue */

/* 1612 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1614 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1618 */	NdrFcShort( 0x5 ),	/* 5 */
/* 1620 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1622 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1624 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1626 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1628 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1630 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1632 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1634 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1636 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter maximumValue */

/* 1638 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1640 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1642 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 1644 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1646 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1648 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_minimumValue */

/* 1650 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1652 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1656 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1658 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1660 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1662 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1664 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1666 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1668 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1670 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1672 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1674 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter minimumValue */

/* 1676 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1678 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1680 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 1682 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1684 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1686 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure copyText */


	/* Procedure addSelection */

/* 1688 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1690 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1694 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1696 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1698 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1700 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1702 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 1704 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1706 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1708 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1710 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1712 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter startOffset */


	/* Parameter startOffset */

/* 1714 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1716 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1718 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */


	/* Parameter endOffset */

/* 1720 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1722 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1724 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 1726 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1728 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1730 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attributes */

/* 1732 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1734 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1738 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1740 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1742 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1744 */	NdrFcShort( 0x40 ),	/* 64 */
/* 1746 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x5,		/* 5 */
/* 1748 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1750 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1752 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1754 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1756 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter offset */

/* 1758 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1760 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1762 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 1764 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1766 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1768 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 1770 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1772 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1774 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter textAttributes */

/* 1776 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1778 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1780 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 1782 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1784 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1786 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_characterExtents */

/* 1788 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1790 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1794 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1796 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 1798 */	NdrFcShort( 0xe ),	/* 14 */
/* 1800 */	NdrFcShort( 0x78 ),	/* 120 */
/* 1802 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x7,		/* 7 */
/* 1804 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1806 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1808 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1810 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1812 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter offset */

/* 1814 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1816 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1818 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter coordType */

/* 1820 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1822 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1824 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter x */

/* 1826 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1828 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1830 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 1832 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1834 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1836 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter width */

/* 1838 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1840 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1842 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter height */

/* 1844 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1846 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1848 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1850 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1852 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 1854 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nRows */


	/* Procedure get_nSelections */

/* 1856 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1858 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1862 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1864 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1866 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1868 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1870 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 1872 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1874 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1876 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1878 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1880 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter rowCount */


	/* Parameter nSelections */

/* 1882 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1884 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1886 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 1888 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1890 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1892 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_offsetAtPoint */

/* 1894 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1896 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1900 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1902 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 1904 */	NdrFcShort( 0x16 ),	/* 22 */
/* 1906 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1908 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x5,		/* 5 */
/* 1910 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1912 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1914 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1916 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1918 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter x */

/* 1920 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1922 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1924 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 1926 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1928 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1930 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter coordType */

/* 1932 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1934 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1936 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter offset */

/* 1938 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1940 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1942 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1944 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1946 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1948 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selection */

/* 1950 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1952 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1956 */	NdrFcShort( 0x9 ),	/* 9 */
/* 1958 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 1960 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1962 */	NdrFcShort( 0x40 ),	/* 64 */
/* 1964 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 1966 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1968 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1970 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1972 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1974 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter selectionIndex */

/* 1976 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1978 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1980 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 1982 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1984 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1986 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 1988 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1990 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1992 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1994 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1996 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1998 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_text */

/* 2000 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2002 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2006 */	NdrFcShort( 0xa ),	/* 10 */
/* 2008 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2010 */	NdrFcShort( 0x10 ),	/* 16 */
/* 2012 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2014 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 2016 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2018 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2020 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2022 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2024 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter startOffset */

/* 2026 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2028 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2030 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2032 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2034 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2036 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 2038 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2040 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2042 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 2044 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2046 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2048 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_textBeforeOffset */

/* 2050 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2052 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2056 */	NdrFcShort( 0xb ),	/* 11 */
/* 2058 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 2060 */	NdrFcShort( 0xe ),	/* 14 */
/* 2062 */	NdrFcShort( 0x40 ),	/* 64 */
/* 2064 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x6,		/* 6 */
/* 2066 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2068 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2070 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2072 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2074 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter offset */

/* 2076 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2078 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2080 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter boundaryType */

/* 2082 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2084 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2086 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2088 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2090 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2092 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2094 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2096 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2098 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 2100 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2102 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2104 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 2106 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2108 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 2110 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_textAfterOffset */

/* 2112 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2114 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2118 */	NdrFcShort( 0xc ),	/* 12 */
/* 2120 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 2122 */	NdrFcShort( 0xe ),	/* 14 */
/* 2124 */	NdrFcShort( 0x40 ),	/* 64 */
/* 2126 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x6,		/* 6 */
/* 2128 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2130 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2132 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2134 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2136 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter offset */

/* 2138 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2140 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2142 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter boundaryType */

/* 2144 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2146 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2148 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2150 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2152 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2154 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2156 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2158 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2160 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 2162 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2164 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2166 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 2168 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2170 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 2172 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_textAtOffset */

/* 2174 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2176 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2180 */	NdrFcShort( 0xd ),	/* 13 */
/* 2182 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 2184 */	NdrFcShort( 0xe ),	/* 14 */
/* 2186 */	NdrFcShort( 0x40 ),	/* 64 */
/* 2188 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x6,		/* 6 */
/* 2190 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2192 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2194 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2196 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2198 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter offset */

/* 2200 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2202 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2204 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter boundaryType */

/* 2206 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2208 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2210 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2212 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2214 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2216 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2218 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2220 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2222 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 2224 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2226 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2228 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 2230 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2232 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 2234 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure removeSelection */

/* 2236 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2238 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2242 */	NdrFcShort( 0xe ),	/* 14 */
/* 2244 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2246 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2248 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2250 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2252 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2254 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2256 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2258 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2260 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter selectionIndex */

/* 2262 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2264 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2266 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2268 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2270 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2272 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setCaretOffset */

/* 2274 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2276 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2280 */	NdrFcShort( 0xf ),	/* 15 */
/* 2282 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2284 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2286 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2288 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2290 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2292 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2294 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2296 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2298 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter offset */

/* 2300 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2302 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2304 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2306 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2308 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2310 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setSelection */

/* 2312 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2314 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2318 */	NdrFcShort( 0x10 ),	/* 16 */
/* 2320 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2322 */	NdrFcShort( 0x18 ),	/* 24 */
/* 2324 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2326 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 2328 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2330 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2332 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2334 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2336 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter selectionIndex */

/* 2338 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2340 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2342 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2344 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2346 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2348 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2350 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2352 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2354 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2356 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2358 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2360 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nCharacters */

/* 2362 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2364 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2368 */	NdrFcShort( 0x11 ),	/* 17 */
/* 2370 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2372 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2374 */	NdrFcShort( 0x24 ),	/* 36 */
/* 2376 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2378 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2380 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2382 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2384 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2386 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter nCharacters */

/* 2388 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2390 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2392 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2394 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2396 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2398 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure scrollSubstringTo */

/* 2400 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2402 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2406 */	NdrFcShort( 0x12 ),	/* 18 */
/* 2408 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2410 */	NdrFcShort( 0x16 ),	/* 22 */
/* 2412 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2414 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 2416 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2418 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2420 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2422 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2424 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter startIndex */

/* 2426 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2428 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2430 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endIndex */

/* 2432 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2434 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2436 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter scrollType */

/* 2438 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2440 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2442 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Return value */

/* 2444 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2446 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2448 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure scrollSubstringToPoint */

/* 2450 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2452 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2456 */	NdrFcShort( 0x13 ),	/* 19 */
/* 2458 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 2460 */	NdrFcShort( 0x26 ),	/* 38 */
/* 2462 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2464 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x6,		/* 6 */
/* 2466 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2468 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2470 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2472 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2474 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter startIndex */

/* 2476 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2478 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2480 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endIndex */

/* 2482 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2484 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2486 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter coordinateType */

/* 2488 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2490 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2492 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter x */

/* 2494 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2496 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2498 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 2500 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2502 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2504 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2506 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2508 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 2510 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_newText */

/* 2512 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2514 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2518 */	NdrFcShort( 0x14 ),	/* 20 */
/* 2520 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2522 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2524 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2526 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2528 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2530 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2532 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2534 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2536 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter newText */

/* 2538 */	NdrFcShort( 0x4113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=16 */
/* 2540 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2542 */	NdrFcShort( 0x548 ),	/* Type Offset=1352 */

	/* Return value */

/* 2544 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2546 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2548 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_oldText */

/* 2550 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2552 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2556 */	NdrFcShort( 0x15 ),	/* 21 */
/* 2558 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2560 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2562 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2564 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2566 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2568 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2570 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2572 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2574 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter oldText */

/* 2576 */	NdrFcShort( 0x4113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=16 */
/* 2578 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2580 */	NdrFcShort( 0x548 ),	/* Type Offset=1352 */

	/* Return value */

/* 2582 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2584 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2586 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attributeRange */

/* 2588 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2590 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2594 */	NdrFcShort( 0x16 ),	/* 22 */
/* 2596 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 2598 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2600 */	NdrFcShort( 0x40 ),	/* 64 */
/* 2602 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x6,		/* 6 */
/* 2604 */	0xa,		/* 10 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 2606 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2608 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2610 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2612 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter offset */

/* 2614 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2616 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2618 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter filter */

/* 2620 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2622 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2624 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Parameter startOffset */

/* 2626 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2628 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2630 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2632 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2634 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2636 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter attributeValues */

/* 2638 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2640 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2642 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 2644 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2646 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 2648 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selections */

/* 2650 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2652 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2656 */	NdrFcShort( 0x3 ),	/* 3 */
/* 2658 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2660 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2662 */	NdrFcShort( 0x24 ),	/* 36 */
/* 2664 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 2666 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2668 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2670 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2672 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2674 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter selections */

/* 2676 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 2678 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2680 */	NdrFcShort( 0x558 ),	/* Type Offset=1368 */

	/* Parameter nSelections */

/* 2682 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2684 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2686 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2688 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2690 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2692 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setSelections */

/* 2694 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2696 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2700 */	NdrFcShort( 0x4 ),	/* 4 */
/* 2702 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2704 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2706 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2708 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 2710 */	0xa,		/* 10 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2712 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2714 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2716 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2718 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter nSelections */

/* 2720 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2722 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2724 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter selections */

/* 2726 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2728 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2730 */	NdrFcShort( 0x5a2 ),	/* Type Offset=1442 */

	/* Return value */

/* 2732 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2734 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2736 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure deleteText */

/* 2738 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2740 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2744 */	NdrFcShort( 0x4 ),	/* 4 */
/* 2746 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2748 */	NdrFcShort( 0x10 ),	/* 16 */
/* 2750 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2752 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 2754 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2756 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2758 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2760 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2762 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter startOffset */

/* 2764 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2766 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2768 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2770 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2772 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2774 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2776 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2778 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2780 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure insertText */

/* 2782 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2784 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2788 */	NdrFcShort( 0x5 ),	/* 5 */
/* 2790 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2792 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2794 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2796 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 2798 */	0xa,		/* 10 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2800 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2802 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2804 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2806 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter offset */

/* 2808 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2810 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2812 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 2814 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2816 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2818 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Return value */

/* 2820 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2822 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2824 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure cutText */

/* 2826 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2828 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2832 */	NdrFcShort( 0x6 ),	/* 6 */
/* 2834 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2836 */	NdrFcShort( 0x10 ),	/* 16 */
/* 2838 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2840 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 2842 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2844 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2846 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2848 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2850 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter startOffset */

/* 2852 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2854 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2856 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2858 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2860 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2862 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2864 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2866 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2868 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure pasteText */

/* 2870 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2872 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2876 */	NdrFcShort( 0x7 ),	/* 7 */
/* 2878 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2880 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2882 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2884 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2886 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2888 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2890 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2892 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2894 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter offset */

/* 2896 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2898 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2900 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2902 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2904 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2906 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure replaceText */

/* 2908 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2910 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2914 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2916 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2918 */	NdrFcShort( 0x10 ),	/* 16 */
/* 2920 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2922 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x4,		/* 4 */
/* 2924 */	0xa,		/* 10 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2926 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2928 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2930 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2932 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter startOffset */

/* 2934 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2936 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2938 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2940 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2942 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2944 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 2946 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2948 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 2950 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Return value */

/* 2952 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2954 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 2956 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setAttributes */

/* 2958 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2960 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2964 */	NdrFcShort( 0x9 ),	/* 9 */
/* 2966 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 2968 */	NdrFcShort( 0x10 ),	/* 16 */
/* 2970 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2972 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x4,		/* 4 */
/* 2974 */	0xa,		/* 10 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2976 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2978 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2980 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2982 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter startOffset */

/* 2984 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2986 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 2988 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2990 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2992 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 2994 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter attributes */

/* 2996 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2998 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3000 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Return value */

/* 3002 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3004 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3006 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_anchor */

/* 3008 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3010 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3014 */	NdrFcShort( 0x9 ),	/* 9 */
/* 3016 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3018 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3020 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3022 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3024 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 3026 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3028 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3030 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3032 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter index */

/* 3034 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3036 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3038 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter anchor */

/* 3040 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 3042 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3044 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 3046 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3048 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3050 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_anchorTarget */

/* 3052 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3054 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3058 */	NdrFcShort( 0xa ),	/* 10 */
/* 3060 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3062 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3064 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3066 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3068 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 3070 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3072 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3074 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3076 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter index */

/* 3078 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3080 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3082 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter anchorTarget */

/* 3084 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 3086 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3088 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 3090 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3092 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3094 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nRows */


	/* Procedure get_startIndex */

/* 3096 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3098 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3102 */	NdrFcShort( 0xb ),	/* 11 */
/* 3104 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3106 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3108 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3110 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3112 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3114 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3116 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3118 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3120 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter rowCount */


	/* Parameter index */

/* 3122 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3124 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3126 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 3128 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3130 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3132 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nSelectedChildren */


	/* Procedure get_endIndex */

/* 3134 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3136 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3140 */	NdrFcShort( 0xc ),	/* 12 */
/* 3142 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3144 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3146 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3148 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3150 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3152 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3154 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3156 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3158 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter cellCount */


	/* Parameter index */

/* 3160 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3162 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3164 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 3166 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3168 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3170 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_valid */

/* 3172 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3174 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3178 */	NdrFcShort( 0xd ),	/* 13 */
/* 3180 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3182 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3184 */	NdrFcShort( 0x21 ),	/* 33 */
/* 3186 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3188 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3190 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3192 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3194 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3196 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter valid */

/* 3198 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3200 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3202 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 3204 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3206 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3208 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nHyperlinks */

/* 3210 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3212 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3216 */	NdrFcShort( 0x16 ),	/* 22 */
/* 3218 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3220 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3222 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3224 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3226 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3228 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3230 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3232 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3234 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter hyperlinkCount */

/* 3236 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3238 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3240 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3242 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3244 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3246 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_hyperlink */

/* 3248 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3250 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3254 */	NdrFcShort( 0x17 ),	/* 23 */
/* 3256 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3258 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3260 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3262 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3264 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3266 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3268 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3270 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3272 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter index */

/* 3274 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3276 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3278 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter hyperlink */

/* 3280 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3282 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3284 */	NdrFcShort( 0x5c0 ),	/* Type Offset=1472 */

	/* Return value */

/* 3286 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3288 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3290 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_hyperlinkIndex */

/* 3292 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3294 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3298 */	NdrFcShort( 0x18 ),	/* 24 */
/* 3300 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3302 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3304 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3306 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 3308 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3310 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3312 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3314 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3316 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter charIndex */

/* 3318 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3320 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3322 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter hyperlinkIndex */

/* 3324 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3326 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3328 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3330 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3332 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3334 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_hyperlinks */

/* 3336 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3338 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3342 */	NdrFcShort( 0x19 ),	/* 25 */
/* 3344 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3346 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3348 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3350 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3352 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 3354 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3356 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3358 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3360 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter hyperlinks */

/* 3362 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 3364 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3366 */	NdrFcShort( 0x5d6 ),	/* Type Offset=1494 */

	/* Parameter nHyperlinks */

/* 3368 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3370 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3372 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3374 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3376 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3378 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_cellAt */


	/* Procedure get_accessibleAt */

/* 3380 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3382 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3386 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3388 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 3390 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3392 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3394 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 3396 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3398 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3400 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3402 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3404 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter row */


	/* Parameter row */

/* 3406 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3408 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3410 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */


	/* Parameter column */

/* 3412 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3414 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3416 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter cell */


	/* Parameter accessible */

/* 3418 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3420 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3422 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */


	/* Return value */

/* 3424 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3426 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3428 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_caption */


	/* Procedure get_caption */

/* 3430 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3432 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3436 */	NdrFcShort( 0x4 ),	/* 4 */
/* 3438 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3440 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3442 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3444 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3446 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3448 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3450 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3452 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3454 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter accessible */


	/* Parameter accessible */

/* 3456 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3458 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3460 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */


	/* Return value */

/* 3462 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3464 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3466 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_childIndex */

/* 3468 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3470 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3474 */	NdrFcShort( 0x5 ),	/* 5 */
/* 3476 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 3478 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3480 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3482 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 3484 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3486 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3488 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3490 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3492 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter rowIndex */

/* 3494 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3496 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3498 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter columnIndex */

/* 3500 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3502 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3504 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter cellIndex */

/* 3506 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3508 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3510 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3512 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3514 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3516 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnDescription */

/* 3518 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3520 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3524 */	NdrFcShort( 0x6 ),	/* 6 */
/* 3526 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3528 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3530 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3532 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3534 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 3536 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3538 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3540 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3542 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter column */

/* 3544 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3546 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3548 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter description */

/* 3550 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 3552 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3554 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 3556 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3558 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3560 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnExtentAt */

/* 3562 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3564 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3568 */	NdrFcShort( 0x7 ),	/* 7 */
/* 3570 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 3572 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3574 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3576 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 3578 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3580 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3582 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3584 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3586 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter row */

/* 3588 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3590 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3592 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */

/* 3594 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3596 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3598 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter nColumnsSpanned */

/* 3600 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3602 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3604 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3606 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3608 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3610 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnHeader */

/* 3612 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3614 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3618 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3620 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3622 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3624 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3626 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3628 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3630 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3632 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3634 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3636 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter accessibleTable */

/* 3638 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3640 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3642 */	NdrFcShort( 0x5f4 ),	/* Type Offset=1524 */

	/* Parameter startingRowIndex */

/* 3644 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3646 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3648 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3650 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3652 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3654 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnIndex */

/* 3656 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3658 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3662 */	NdrFcShort( 0x9 ),	/* 9 */
/* 3664 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3666 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3668 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3670 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 3672 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3674 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3676 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3678 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3680 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter cellIndex */

/* 3682 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3684 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3686 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter columnIndex */

/* 3688 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3690 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3692 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3694 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3696 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3698 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nSelectedRows */


	/* Procedure get_nColumns */

/* 3700 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3702 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3706 */	NdrFcShort( 0xa ),	/* 10 */
/* 3708 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3710 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3712 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3714 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3716 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3718 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3720 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3722 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3724 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter rowCount */


	/* Parameter columnCount */

/* 3726 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3728 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3730 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 3732 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3734 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3736 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nSelectedColumns */

/* 3738 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3740 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3744 */	NdrFcShort( 0xd ),	/* 13 */
/* 3746 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3748 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3750 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3752 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3754 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3756 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3758 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3760 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3762 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter columnCount */

/* 3764 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3766 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3768 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3770 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3772 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3774 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nSelectedRows */

/* 3776 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3778 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3782 */	NdrFcShort( 0xe ),	/* 14 */
/* 3784 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3786 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3788 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3790 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3792 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3794 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3796 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3798 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3800 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter rowCount */

/* 3802 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3804 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3806 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3808 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3810 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3812 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowDescription */

/* 3814 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3816 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3820 */	NdrFcShort( 0xf ),	/* 15 */
/* 3822 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3824 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3826 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3828 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3830 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 3832 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3834 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3836 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3838 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter row */

/* 3840 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3842 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3844 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter description */

/* 3846 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 3848 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3850 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 3852 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3854 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3856 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowExtentAt */

/* 3858 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3860 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3864 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3866 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 3868 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3870 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3872 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 3874 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3876 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3878 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3880 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3882 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter row */

/* 3884 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3886 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3888 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */

/* 3890 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3892 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3894 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter nRowsSpanned */

/* 3896 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3898 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3900 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3902 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3904 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3906 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowHeader */

/* 3908 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3910 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3914 */	NdrFcShort( 0x11 ),	/* 17 */
/* 3916 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3918 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3920 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3922 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3924 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3926 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3928 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3930 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3932 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter accessibleTable */

/* 3934 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3936 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3938 */	NdrFcShort( 0x5f4 ),	/* Type Offset=1524 */

	/* Parameter startingColumnIndex */

/* 3940 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3942 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3944 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3946 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3948 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3950 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowIndex */

/* 3952 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3954 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3958 */	NdrFcShort( 0x12 ),	/* 18 */
/* 3960 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 3962 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3964 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3966 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 3968 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3970 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3972 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3974 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3976 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter cellIndex */

/* 3978 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3980 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 3982 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter rowIndex */

/* 3984 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3986 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 3988 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3990 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3992 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 3994 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedChildren */

/* 3996 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3998 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4002 */	NdrFcShort( 0x13 ),	/* 19 */
/* 4004 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 4006 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4008 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4010 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 4012 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4014 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4016 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4018 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4020 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter maxChildren */

/* 4022 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4024 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4026 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter children */

/* 4028 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 4030 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4032 */	NdrFcShort( 0x60a ),	/* Type Offset=1546 */

	/* Parameter nChildren */

/* 4034 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4036 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4038 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4040 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4042 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 4044 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedColumns */

/* 4046 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4048 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4052 */	NdrFcShort( 0x14 ),	/* 20 */
/* 4054 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 4056 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4058 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4060 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 4062 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4064 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4066 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4068 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4070 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter maxColumns */

/* 4072 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4074 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4076 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter columns */

/* 4078 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 4080 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4082 */	NdrFcShort( 0x60a ),	/* Type Offset=1546 */

	/* Parameter nColumns */

/* 4084 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4086 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4088 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4090 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4092 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 4094 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedRows */

/* 4096 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4098 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4102 */	NdrFcShort( 0x15 ),	/* 21 */
/* 4104 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 4106 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4108 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4110 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 4112 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4114 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4116 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4118 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4120 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter maxRows */

/* 4122 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4124 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4126 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter rows */

/* 4128 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 4130 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4132 */	NdrFcShort( 0x60a ),	/* Type Offset=1546 */

	/* Parameter nRows */

/* 4134 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4136 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4138 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4140 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4142 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 4144 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_summary */

/* 4146 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4148 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4152 */	NdrFcShort( 0x16 ),	/* 22 */
/* 4154 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4156 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4158 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4160 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 4162 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4164 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4166 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4168 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4170 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter accessible */

/* 4172 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4174 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4176 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 4178 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4180 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4182 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isColumnSelected */

/* 4184 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4186 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4190 */	NdrFcShort( 0x17 ),	/* 23 */
/* 4192 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 4194 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4196 */	NdrFcShort( 0x21 ),	/* 33 */
/* 4198 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 4200 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4202 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4204 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4206 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4208 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter column */

/* 4210 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4212 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4214 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 4216 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4218 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4220 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 4222 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4224 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4226 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isRowSelected */

/* 4228 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4230 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4234 */	NdrFcShort( 0x18 ),	/* 24 */
/* 4236 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 4238 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4240 */	NdrFcShort( 0x21 ),	/* 33 */
/* 4242 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 4244 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4246 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4248 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4250 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4252 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter row */

/* 4254 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4256 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4258 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 4260 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4262 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4264 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 4266 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4268 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4270 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isSelected */

/* 4272 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4274 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4278 */	NdrFcShort( 0x19 ),	/* 25 */
/* 4280 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 4282 */	NdrFcShort( 0x10 ),	/* 16 */
/* 4284 */	NdrFcShort( 0x21 ),	/* 33 */
/* 4286 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 4288 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4290 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4292 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4294 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4296 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter row */

/* 4298 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4300 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4302 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */

/* 4304 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4306 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4308 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 4310 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4312 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4314 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 4316 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4318 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 4320 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure selectRow */

/* 4322 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4324 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4328 */	NdrFcShort( 0x1a ),	/* 26 */
/* 4330 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4332 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4334 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4336 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4338 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4340 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4342 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4344 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4346 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter row */

/* 4348 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4350 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4352 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4354 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4356 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4358 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure selectColumn */

/* 4360 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4362 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4366 */	NdrFcShort( 0x1b ),	/* 27 */
/* 4368 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4370 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4372 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4374 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4376 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4378 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4380 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4382 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4384 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter column */

/* 4386 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4388 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4390 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4392 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4394 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4396 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure unselectRow */

/* 4398 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4400 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4404 */	NdrFcShort( 0x1c ),	/* 28 */
/* 4406 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4408 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4410 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4412 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4414 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4416 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4418 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4420 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4422 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter row */

/* 4424 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4426 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4428 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4430 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4432 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4434 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure unselectColumn */

/* 4436 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4438 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4442 */	NdrFcShort( 0x1d ),	/* 29 */
/* 4444 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4446 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4448 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4450 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4452 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4454 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4456 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4458 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4460 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter column */

/* 4462 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4464 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4466 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4468 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4470 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4472 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowColumnExtentsAtIndex */

/* 4474 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4476 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4480 */	NdrFcShort( 0x1e ),	/* 30 */
/* 4482 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 4484 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4486 */	NdrFcShort( 0x91 ),	/* 145 */
/* 4488 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x7,		/* 7 */
/* 4490 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4492 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4494 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4496 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4498 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter index */

/* 4500 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4502 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4504 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter row */

/* 4506 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4508 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4510 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */

/* 4512 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4514 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4516 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter rowExtents */

/* 4518 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4520 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 4522 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter columnExtents */

/* 4524 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4526 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 4528 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 4530 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4532 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 4534 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 4536 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4538 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 4540 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_modelChange */

/* 4542 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4544 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4548 */	NdrFcShort( 0x1f ),	/* 31 */
/* 4550 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4552 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4554 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4556 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 4558 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4560 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4562 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4564 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4566 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter modelChange */

/* 4568 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 4570 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4572 */	NdrFcShort( 0x628 ),	/* Type Offset=1576 */

	/* Return value */

/* 4574 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4576 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4578 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowExtent */


	/* Procedure get_nColumns */

/* 4580 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4582 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4586 */	NdrFcShort( 0x6 ),	/* 6 */
/* 4588 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4590 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4592 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4594 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4596 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4598 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4600 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4602 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4604 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter nRowsSpanned */


	/* Parameter columnCount */

/* 4606 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4608 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4610 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 4612 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4614 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4616 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowIndex */


	/* Procedure get_nSelectedCells */

/* 4618 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4620 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4624 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4626 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4628 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4630 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4632 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4634 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4636 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4638 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4640 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4642 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter rowIndex */


	/* Parameter cellCount */

/* 4644 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4646 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4648 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 4650 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4652 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4654 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nSelectedColumns */

/* 4656 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4658 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4662 */	NdrFcShort( 0x9 ),	/* 9 */
/* 4664 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4666 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4668 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4670 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4672 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4674 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4676 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4678 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4680 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter columnCount */

/* 4682 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4684 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4686 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4688 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4690 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4692 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowDescription */

/* 4694 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4696 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4700 */	NdrFcShort( 0xb ),	/* 11 */
/* 4702 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 4704 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4706 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4708 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 4710 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4712 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4714 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4716 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4718 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter row */

/* 4720 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4722 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4724 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter description */

/* 4726 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 4728 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4730 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 4732 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4734 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4736 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedCells */

/* 4738 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4740 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4744 */	NdrFcShort( 0xc ),	/* 12 */
/* 4746 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 4748 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4750 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4752 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 4754 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4756 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4758 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4760 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4762 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter cells */

/* 4764 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 4766 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4768 */	NdrFcShort( 0x636 ),	/* Type Offset=1590 */

	/* Parameter nSelectedCells */

/* 4770 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4772 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4774 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4776 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4778 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4780 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedColumns */

/* 4782 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4784 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4788 */	NdrFcShort( 0xd ),	/* 13 */
/* 4790 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 4792 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4794 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4796 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 4798 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4800 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4802 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4804 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4806 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter selectedColumns */

/* 4808 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 4810 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4812 */	NdrFcShort( 0x654 ),	/* Type Offset=1620 */

	/* Parameter nColumns */

/* 4814 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4816 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4818 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4820 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4822 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4824 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedRows */

/* 4826 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4828 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4832 */	NdrFcShort( 0xe ),	/* 14 */
/* 4834 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 4836 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4838 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4840 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 4842 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4844 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4846 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4848 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4850 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter selectedRows */

/* 4852 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 4854 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4856 */	NdrFcShort( 0x654 ),	/* Type Offset=1620 */

	/* Parameter nRows */

/* 4858 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4860 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4862 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4864 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4866 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4868 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_summary */

/* 4870 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4872 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4876 */	NdrFcShort( 0xf ),	/* 15 */
/* 4878 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4880 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4882 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4884 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 4886 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4888 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4890 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4892 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4894 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter accessible */

/* 4896 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4898 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4900 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 4902 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4904 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4906 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isColumnSelected */

/* 4908 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4910 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4914 */	NdrFcShort( 0x10 ),	/* 16 */
/* 4916 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 4918 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4920 */	NdrFcShort( 0x21 ),	/* 33 */
/* 4922 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 4924 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4926 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4928 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4930 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4932 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter column */

/* 4934 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4936 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4938 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 4940 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4942 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4944 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 4946 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4948 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4950 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isRowSelected */

/* 4952 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4954 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4958 */	NdrFcShort( 0x11 ),	/* 17 */
/* 4960 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 4962 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4964 */	NdrFcShort( 0x21 ),	/* 33 */
/* 4966 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 4968 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4970 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4972 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4974 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4976 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter row */

/* 4978 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4980 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 4982 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 4984 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4986 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 4988 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 4990 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4992 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 4994 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure selectRow */

/* 4996 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4998 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5002 */	NdrFcShort( 0x12 ),	/* 18 */
/* 5004 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 5006 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5008 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5010 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5012 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5014 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5016 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5018 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5020 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter row */

/* 5022 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5024 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 5026 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5028 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5030 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 5032 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure selectColumn */

/* 5034 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5036 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5040 */	NdrFcShort( 0x13 ),	/* 19 */
/* 5042 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 5044 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5046 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5048 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5050 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5052 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5054 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5056 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5058 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter column */

/* 5060 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5062 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 5064 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5066 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5068 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 5070 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure unselectRow */

/* 5072 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5074 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5078 */	NdrFcShort( 0x14 ),	/* 20 */
/* 5080 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 5082 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5084 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5086 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5088 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5090 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5092 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5094 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5096 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter row */

/* 5098 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5100 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 5102 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5104 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5106 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 5108 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure unselectColumn */

/* 5110 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5112 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5116 */	NdrFcShort( 0x15 ),	/* 21 */
/* 5118 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 5120 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5122 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5124 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5126 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5128 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5130 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5132 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5134 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter column */

/* 5136 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5138 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 5140 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5142 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5144 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 5146 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_modelChange */

/* 5148 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5150 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5154 */	NdrFcShort( 0x16 ),	/* 22 */
/* 5156 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 5158 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5160 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5162 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 5164 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5166 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5168 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5170 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5172 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter modelChange */

/* 5174 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 5176 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 5178 */	NdrFcShort( 0x628 ),	/* Type Offset=1576 */

	/* Return value */

/* 5180 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5182 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 5184 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnHeaderCells */

/* 5186 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5188 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5192 */	NdrFcShort( 0x4 ),	/* 4 */
/* 5194 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 5196 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5198 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5200 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 5202 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5204 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5206 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5208 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5210 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter cellAccessibles */

/* 5212 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 5214 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 5216 */	NdrFcShort( 0x636 ),	/* Type Offset=1590 */

	/* Parameter nColumnHeaderCells */

/* 5218 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5220 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 5222 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5224 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5226 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 5228 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowHeaderCells */

/* 5230 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5232 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5236 */	NdrFcShort( 0x7 ),	/* 7 */
/* 5238 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 5240 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5242 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5244 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 5246 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5248 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5250 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5252 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5254 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter cellAccessibles */

/* 5256 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 5258 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 5260 */	NdrFcShort( 0x636 ),	/* Type Offset=1590 */

	/* Parameter nRowHeaderCells */

/* 5262 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5264 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 5266 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5268 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5270 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 5272 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isSelected */

/* 5274 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5276 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5280 */	NdrFcShort( 0x9 ),	/* 9 */
/* 5282 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 5284 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5286 */	NdrFcShort( 0x21 ),	/* 33 */
/* 5288 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5290 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5292 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5294 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5296 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5298 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter isSelected */

/* 5300 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5302 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 5304 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 5306 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5308 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 5310 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowColumnExtents */

/* 5312 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5314 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5318 */	NdrFcShort( 0xa ),	/* 10 */
/* 5320 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 5322 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5324 */	NdrFcShort( 0x91 ),	/* 145 */
/* 5326 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x6,		/* 6 */
/* 5328 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5330 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5332 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5334 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5336 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter row */

/* 5338 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5340 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 5342 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */

/* 5344 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5346 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 5348 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter rowExtents */

/* 5350 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5352 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 5354 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter columnExtents */

/* 5356 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5358 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 5360 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 5362 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5364 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 5366 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 5368 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5370 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 5372 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_table */

/* 5374 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5376 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5380 */	NdrFcShort( 0xb ),	/* 11 */
/* 5382 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 5384 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5386 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5388 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 5390 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5392 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5394 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5396 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5398 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter table */

/* 5400 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 5402 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 5404 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 5406 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5408 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 5410 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_imagePosition */

/* 5412 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5414 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5418 */	NdrFcShort( 0x4 ),	/* 4 */
/* 5420 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 5422 */	NdrFcShort( 0x6 ),	/* 6 */
/* 5424 */	NdrFcShort( 0x40 ),	/* 64 */
/* 5426 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 5428 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5430 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5432 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5434 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5436 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter coordinateType */

/* 5438 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5440 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 5442 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter x */

/* 5444 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5446 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 5448 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 5450 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5452 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 5454 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5456 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5458 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 5460 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_imageSize */

/* 5462 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5464 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5468 */	NdrFcShort( 0x5 ),	/* 5 */
/* 5470 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 5472 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5474 */	NdrFcShort( 0x40 ),	/* 64 */
/* 5476 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 5478 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5480 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5482 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5484 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5486 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter height */

/* 5488 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5490 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 5492 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter width */

/* 5494 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5496 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 5498 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5500 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5502 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 5504 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_toolkitName */

/* 5506 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5508 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5512 */	NdrFcShort( 0x5 ),	/* 5 */
/* 5514 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 5516 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5518 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5520 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 5522 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5524 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5526 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5528 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5530 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter name */

/* 5532 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 5534 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 5536 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 5538 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5540 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 5542 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_toolkitVersion */

/* 5544 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5546 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5550 */	NdrFcShort( 0x6 ),	/* 6 */
/* 5552 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 5554 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5556 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5558 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 5560 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5562 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5564 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5566 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5568 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter version */

/* 5570 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 5572 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 5574 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 5576 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5578 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 5580 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_anchorTarget */

/* 5582 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5584 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5588 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5590 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 5592 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5594 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5596 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 5598 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5600 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5602 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5604 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5606 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter accessible */

/* 5608 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 5610 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 5612 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 5614 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5616 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 5618 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const ia2_api_all_MIDL_TYPE_FORMAT_STRING ia2_api_all__MIDL_TypeFormatString =
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
/* 52 */	NdrFcLong( 0x0 ),	/* 0 */
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
			0x11, 0x0,	/* FC_RP */
/* 70 */	NdrFcShort( 0x2 ),	/* Offset= 2 (72) */
/* 72 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 74 */	NdrFcShort( 0x0 ),	/* 0 */
/* 76 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x0,		/*  */
/* 78 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 80 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 82 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 84 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 86 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 88 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 90 */	NdrFcShort( 0xffd8 ),	/* Offset= -40 (50) */
/* 92 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 94 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 96 */	NdrFcShort( 0x2 ),	/* Offset= 2 (98) */
/* 98 */	
			0x13, 0x0,	/* FC_OP */
/* 100 */	NdrFcShort( 0x2 ),	/* Offset= 2 (102) */
/* 102 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 104 */	NdrFcShort( 0x0 ),	/* 0 */
/* 106 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x0,		/*  */
/* 108 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 110 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 112 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 114 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 116 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 118 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 120 */	NdrFcShort( 0xffa8 ),	/* Offset= -88 (32) */
/* 122 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 124 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 126 */	NdrFcShort( 0x2 ),	/* Offset= 2 (128) */
/* 128 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 130 */	NdrFcLong( 0x7cdf86ee ),	/* 2095023854 */
/* 134 */	NdrFcShort( 0xc3da ),	/* -15398 */
/* 136 */	NdrFcShort( 0x496a ),	/* 18794 */
/* 138 */	0xbd,		/* 189 */
			0xa4,		/* 164 */
/* 140 */	0x28,		/* 40 */
			0x1b,		/* 27 */
/* 142 */	0x33,		/* 51 */
			0x6e,		/* 110 */
/* 144 */	0x1f,		/* 31 */
			0xdc,		/* 220 */
/* 146 */	
			0x11, 0x0,	/* FC_RP */
/* 148 */	NdrFcShort( 0x2 ),	/* Offset= 2 (150) */
/* 150 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 152 */	NdrFcShort( 0x0 ),	/* 0 */
/* 154 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x0,		/*  */
/* 156 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 158 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 160 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 162 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 164 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 166 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 168 */	NdrFcShort( 0xffd8 ),	/* Offset= -40 (128) */
/* 170 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 172 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 174 */	NdrFcShort( 0x2 ),	/* Offset= 2 (176) */
/* 176 */	
			0x13, 0x0,	/* FC_OP */
/* 178 */	NdrFcShort( 0x2 ),	/* Offset= 2 (180) */
/* 180 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 182 */	NdrFcShort( 0x0 ),	/* 0 */
/* 184 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x0,		/*  */
/* 186 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 188 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 190 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 192 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 194 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 196 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 198 */	NdrFcShort( 0xff5a ),	/* Offset= -166 (32) */
/* 200 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 202 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 204 */	NdrFcShort( 0x1a ),	/* Offset= 26 (230) */
/* 206 */	
			0x13, 0x0,	/* FC_OP */
/* 208 */	NdrFcShort( 0x2 ),	/* Offset= 2 (210) */
/* 210 */	
			0x2a,		/* FC_ENCAPSULATED_UNION */
			0x48,		/* 72 */
/* 212 */	NdrFcShort( 0x4 ),	/* 4 */
/* 214 */	NdrFcShort( 0x2 ),	/* 2 */
/* 216 */	NdrFcLong( 0x48746457 ),	/* 1215587415 */
/* 220 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 222 */	NdrFcLong( 0x52746457 ),	/* 1383359575 */
/* 226 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 228 */	NdrFcShort( 0xffff ),	/* Offset= -1 (227) */
/* 230 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 232 */	NdrFcShort( 0x1 ),	/* 1 */
/* 234 */	NdrFcShort( 0x8 ),	/* 8 */
/* 236 */	NdrFcShort( 0x0 ),	/* 0 */
/* 238 */	NdrFcShort( 0xffe0 ),	/* Offset= -32 (206) */
/* 240 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 242 */	NdrFcShort( 0x2 ),	/* Offset= 2 (244) */
/* 244 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 246 */	NdrFcShort( 0x18 ),	/* 24 */
/* 248 */	NdrFcShort( 0x0 ),	/* 0 */
/* 250 */	NdrFcShort( 0x0 ),	/* Offset= 0 (250) */
/* 252 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 254 */	NdrFcShort( 0xff22 ),	/* Offset= -222 (32) */
/* 256 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 258 */	NdrFcShort( 0xff1e ),	/* Offset= -226 (32) */
/* 260 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 262 */	NdrFcShort( 0xff1a ),	/* Offset= -230 (32) */
/* 264 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 266 */	
			0x12, 0x0,	/* FC_UP */
/* 268 */	NdrFcShort( 0xff0a ),	/* Offset= -246 (22) */
/* 270 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 272 */	NdrFcShort( 0x0 ),	/* 0 */
/* 274 */	NdrFcShort( 0x8 ),	/* 8 */
/* 276 */	NdrFcShort( 0x0 ),	/* 0 */
/* 278 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (266) */
/* 280 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 282 */	NdrFcShort( 0x3a2 ),	/* Offset= 930 (1212) */
/* 284 */	
			0x13, 0x0,	/* FC_OP */
/* 286 */	NdrFcShort( 0x38a ),	/* Offset= 906 (1192) */
/* 288 */	
			0x2b,		/* FC_NON_ENCAPSULATED_UNION */
			0x9,		/* FC_ULONG */
/* 290 */	0x7,		/* Corr desc: FC_USHORT */
			0x0,		/*  */
/* 292 */	NdrFcShort( 0xfff8 ),	/* -8 */
/* 294 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 296 */	NdrFcShort( 0x2 ),	/* Offset= 2 (298) */
/* 298 */	NdrFcShort( 0x10 ),	/* 16 */
/* 300 */	NdrFcShort( 0x2f ),	/* 47 */
/* 302 */	NdrFcLong( 0x14 ),	/* 20 */
/* 306 */	NdrFcShort( 0x800b ),	/* Simple arm type: FC_HYPER */
/* 308 */	NdrFcLong( 0x3 ),	/* 3 */
/* 312 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 314 */	NdrFcLong( 0x11 ),	/* 17 */
/* 318 */	NdrFcShort( 0x8001 ),	/* Simple arm type: FC_BYTE */
/* 320 */	NdrFcLong( 0x2 ),	/* 2 */
/* 324 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 326 */	NdrFcLong( 0x4 ),	/* 4 */
/* 330 */	NdrFcShort( 0x800a ),	/* Simple arm type: FC_FLOAT */
/* 332 */	NdrFcLong( 0x5 ),	/* 5 */
/* 336 */	NdrFcShort( 0x800c ),	/* Simple arm type: FC_DOUBLE */
/* 338 */	NdrFcLong( 0xb ),	/* 11 */
/* 342 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 344 */	NdrFcLong( 0xa ),	/* 10 */
/* 348 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 350 */	NdrFcLong( 0x6 ),	/* 6 */
/* 354 */	NdrFcShort( 0xe8 ),	/* Offset= 232 (586) */
/* 356 */	NdrFcLong( 0x7 ),	/* 7 */
/* 360 */	NdrFcShort( 0x800c ),	/* Simple arm type: FC_DOUBLE */
/* 362 */	NdrFcLong( 0x8 ),	/* 8 */
/* 366 */	NdrFcShort( 0xfe98 ),	/* Offset= -360 (6) */
/* 368 */	NdrFcLong( 0xd ),	/* 13 */
/* 372 */	NdrFcShort( 0xfebe ),	/* Offset= -322 (50) */
/* 374 */	NdrFcLong( 0x9 ),	/* 9 */
/* 378 */	NdrFcShort( 0xd6 ),	/* Offset= 214 (592) */
/* 380 */	NdrFcLong( 0x2000 ),	/* 8192 */
/* 384 */	NdrFcShort( 0xe2 ),	/* Offset= 226 (610) */
/* 386 */	NdrFcLong( 0x24 ),	/* 36 */
/* 390 */	NdrFcShort( 0x2d8 ),	/* Offset= 728 (1118) */
/* 392 */	NdrFcLong( 0x4024 ),	/* 16420 */
/* 396 */	NdrFcShort( 0x2d2 ),	/* Offset= 722 (1118) */
/* 398 */	NdrFcLong( 0x4011 ),	/* 16401 */
/* 402 */	NdrFcShort( 0x2d0 ),	/* Offset= 720 (1122) */
/* 404 */	NdrFcLong( 0x4002 ),	/* 16386 */
/* 408 */	NdrFcShort( 0x2ce ),	/* Offset= 718 (1126) */
/* 410 */	NdrFcLong( 0x4003 ),	/* 16387 */
/* 414 */	NdrFcShort( 0x2cc ),	/* Offset= 716 (1130) */
/* 416 */	NdrFcLong( 0x4014 ),	/* 16404 */
/* 420 */	NdrFcShort( 0x2ca ),	/* Offset= 714 (1134) */
/* 422 */	NdrFcLong( 0x4004 ),	/* 16388 */
/* 426 */	NdrFcShort( 0x2c8 ),	/* Offset= 712 (1138) */
/* 428 */	NdrFcLong( 0x4005 ),	/* 16389 */
/* 432 */	NdrFcShort( 0x2c6 ),	/* Offset= 710 (1142) */
/* 434 */	NdrFcLong( 0x400b ),	/* 16395 */
/* 438 */	NdrFcShort( 0x2b0 ),	/* Offset= 688 (1126) */
/* 440 */	NdrFcLong( 0x400a ),	/* 16394 */
/* 444 */	NdrFcShort( 0x2ae ),	/* Offset= 686 (1130) */
/* 446 */	NdrFcLong( 0x4006 ),	/* 16390 */
/* 450 */	NdrFcShort( 0x2b8 ),	/* Offset= 696 (1146) */
/* 452 */	NdrFcLong( 0x4007 ),	/* 16391 */
/* 456 */	NdrFcShort( 0x2ae ),	/* Offset= 686 (1142) */
/* 458 */	NdrFcLong( 0x4008 ),	/* 16392 */
/* 462 */	NdrFcShort( 0x2b0 ),	/* Offset= 688 (1150) */
/* 464 */	NdrFcLong( 0x400d ),	/* 16397 */
/* 468 */	NdrFcShort( 0x2ae ),	/* Offset= 686 (1154) */
/* 470 */	NdrFcLong( 0x4009 ),	/* 16393 */
/* 474 */	NdrFcShort( 0x2ac ),	/* Offset= 684 (1158) */
/* 476 */	NdrFcLong( 0x6000 ),	/* 24576 */
/* 480 */	NdrFcShort( 0x2aa ),	/* Offset= 682 (1162) */
/* 482 */	NdrFcLong( 0x400c ),	/* 16396 */
/* 486 */	NdrFcShort( 0x2a8 ),	/* Offset= 680 (1166) */
/* 488 */	NdrFcLong( 0x10 ),	/* 16 */
/* 492 */	NdrFcShort( 0x8002 ),	/* Simple arm type: FC_CHAR */
/* 494 */	NdrFcLong( 0x12 ),	/* 18 */
/* 498 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 500 */	NdrFcLong( 0x13 ),	/* 19 */
/* 504 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 506 */	NdrFcLong( 0x15 ),	/* 21 */
/* 510 */	NdrFcShort( 0x800b ),	/* Simple arm type: FC_HYPER */
/* 512 */	NdrFcLong( 0x16 ),	/* 22 */
/* 516 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 518 */	NdrFcLong( 0x17 ),	/* 23 */
/* 522 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 524 */	NdrFcLong( 0xe ),	/* 14 */
/* 528 */	NdrFcShort( 0x286 ),	/* Offset= 646 (1174) */
/* 530 */	NdrFcLong( 0x400e ),	/* 16398 */
/* 534 */	NdrFcShort( 0x28a ),	/* Offset= 650 (1184) */
/* 536 */	NdrFcLong( 0x4010 ),	/* 16400 */
/* 540 */	NdrFcShort( 0x288 ),	/* Offset= 648 (1188) */
/* 542 */	NdrFcLong( 0x4012 ),	/* 16402 */
/* 546 */	NdrFcShort( 0x244 ),	/* Offset= 580 (1126) */
/* 548 */	NdrFcLong( 0x4013 ),	/* 16403 */
/* 552 */	NdrFcShort( 0x242 ),	/* Offset= 578 (1130) */
/* 554 */	NdrFcLong( 0x4015 ),	/* 16405 */
/* 558 */	NdrFcShort( 0x240 ),	/* Offset= 576 (1134) */
/* 560 */	NdrFcLong( 0x4016 ),	/* 16406 */
/* 564 */	NdrFcShort( 0x236 ),	/* Offset= 566 (1130) */
/* 566 */	NdrFcLong( 0x4017 ),	/* 16407 */
/* 570 */	NdrFcShort( 0x230 ),	/* Offset= 560 (1130) */
/* 572 */	NdrFcLong( 0x0 ),	/* 0 */
/* 576 */	NdrFcShort( 0x0 ),	/* Offset= 0 (576) */
/* 578 */	NdrFcLong( 0x1 ),	/* 1 */
/* 582 */	NdrFcShort( 0x0 ),	/* Offset= 0 (582) */
/* 584 */	NdrFcShort( 0xffff ),	/* Offset= -1 (583) */
/* 586 */	
			0x15,		/* FC_STRUCT */
			0x7,		/* 7 */
/* 588 */	NdrFcShort( 0x8 ),	/* 8 */
/* 590 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 592 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 594 */	NdrFcLong( 0x20400 ),	/* 132096 */
/* 598 */	NdrFcShort( 0x0 ),	/* 0 */
/* 600 */	NdrFcShort( 0x0 ),	/* 0 */
/* 602 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 604 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 606 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 608 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 610 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 612 */	NdrFcShort( 0x2 ),	/* Offset= 2 (614) */
/* 614 */	
			0x13, 0x0,	/* FC_OP */
/* 616 */	NdrFcShort( 0x1e4 ),	/* Offset= 484 (1100) */
/* 618 */	
			0x2a,		/* FC_ENCAPSULATED_UNION */
			0x89,		/* 137 */
/* 620 */	NdrFcShort( 0x20 ),	/* 32 */
/* 622 */	NdrFcShort( 0xa ),	/* 10 */
/* 624 */	NdrFcLong( 0x8 ),	/* 8 */
/* 628 */	NdrFcShort( 0x50 ),	/* Offset= 80 (708) */
/* 630 */	NdrFcLong( 0xd ),	/* 13 */
/* 634 */	NdrFcShort( 0x70 ),	/* Offset= 112 (746) */
/* 636 */	NdrFcLong( 0x9 ),	/* 9 */
/* 640 */	NdrFcShort( 0x90 ),	/* Offset= 144 (784) */
/* 642 */	NdrFcLong( 0xc ),	/* 12 */
/* 646 */	NdrFcShort( 0xb0 ),	/* Offset= 176 (822) */
/* 648 */	NdrFcLong( 0x24 ),	/* 36 */
/* 652 */	NdrFcShort( 0x102 ),	/* Offset= 258 (910) */
/* 654 */	NdrFcLong( 0x800d ),	/* 32781 */
/* 658 */	NdrFcShort( 0x11e ),	/* Offset= 286 (944) */
/* 660 */	NdrFcLong( 0x10 ),	/* 16 */
/* 664 */	NdrFcShort( 0x138 ),	/* Offset= 312 (976) */
/* 666 */	NdrFcLong( 0x2 ),	/* 2 */
/* 670 */	NdrFcShort( 0x14e ),	/* Offset= 334 (1004) */
/* 672 */	NdrFcLong( 0x3 ),	/* 3 */
/* 676 */	NdrFcShort( 0x164 ),	/* Offset= 356 (1032) */
/* 678 */	NdrFcLong( 0x14 ),	/* 20 */
/* 682 */	NdrFcShort( 0x17a ),	/* Offset= 378 (1060) */
/* 684 */	NdrFcShort( 0xffff ),	/* Offset= -1 (683) */
/* 686 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 688 */	NdrFcShort( 0x0 ),	/* 0 */
/* 690 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 692 */	NdrFcShort( 0x0 ),	/* 0 */
/* 694 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 696 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 700 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 702 */	
			0x13, 0x0,	/* FC_OP */
/* 704 */	NdrFcShort( 0xfd56 ),	/* Offset= -682 (22) */
/* 706 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 708 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 710 */	NdrFcShort( 0x10 ),	/* 16 */
/* 712 */	NdrFcShort( 0x0 ),	/* 0 */
/* 714 */	NdrFcShort( 0x6 ),	/* Offset= 6 (720) */
/* 716 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 718 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 720 */	
			0x11, 0x0,	/* FC_RP */
/* 722 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (686) */
/* 724 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 726 */	NdrFcShort( 0x0 ),	/* 0 */
/* 728 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 730 */	NdrFcShort( 0x0 ),	/* 0 */
/* 732 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 734 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 738 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 740 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 742 */	NdrFcShort( 0xfd4c ),	/* Offset= -692 (50) */
/* 744 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 746 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 748 */	NdrFcShort( 0x10 ),	/* 16 */
/* 750 */	NdrFcShort( 0x0 ),	/* 0 */
/* 752 */	NdrFcShort( 0x6 ),	/* Offset= 6 (758) */
/* 754 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 756 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 758 */	
			0x11, 0x0,	/* FC_RP */
/* 760 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (724) */
/* 762 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 764 */	NdrFcShort( 0x0 ),	/* 0 */
/* 766 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 768 */	NdrFcShort( 0x0 ),	/* 0 */
/* 770 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 772 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 776 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 778 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 780 */	NdrFcShort( 0xff44 ),	/* Offset= -188 (592) */
/* 782 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 784 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 786 */	NdrFcShort( 0x10 ),	/* 16 */
/* 788 */	NdrFcShort( 0x0 ),	/* 0 */
/* 790 */	NdrFcShort( 0x6 ),	/* Offset= 6 (796) */
/* 792 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 794 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 796 */	
			0x11, 0x0,	/* FC_RP */
/* 798 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (762) */
/* 800 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 802 */	NdrFcShort( 0x0 ),	/* 0 */
/* 804 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 806 */	NdrFcShort( 0x0 ),	/* 0 */
/* 808 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 810 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 814 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 816 */	
			0x13, 0x0,	/* FC_OP */
/* 818 */	NdrFcShort( 0x176 ),	/* Offset= 374 (1192) */
/* 820 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 822 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 824 */	NdrFcShort( 0x10 ),	/* 16 */
/* 826 */	NdrFcShort( 0x0 ),	/* 0 */
/* 828 */	NdrFcShort( 0x6 ),	/* Offset= 6 (834) */
/* 830 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 832 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 834 */	
			0x11, 0x0,	/* FC_RP */
/* 836 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (800) */
/* 838 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 840 */	NdrFcLong( 0x2f ),	/* 47 */
/* 844 */	NdrFcShort( 0x0 ),	/* 0 */
/* 846 */	NdrFcShort( 0x0 ),	/* 0 */
/* 848 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 850 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 852 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 854 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 856 */	
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 858 */	NdrFcShort( 0x1 ),	/* 1 */
/* 860 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 862 */	NdrFcShort( 0x4 ),	/* 4 */
/* 864 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 866 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 868 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 870 */	NdrFcShort( 0x18 ),	/* 24 */
/* 872 */	NdrFcShort( 0x0 ),	/* 0 */
/* 874 */	NdrFcShort( 0xa ),	/* Offset= 10 (884) */
/* 876 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 878 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 880 */	NdrFcShort( 0xffd6 ),	/* Offset= -42 (838) */
/* 882 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 884 */	
			0x13, 0x0,	/* FC_OP */
/* 886 */	NdrFcShort( 0xffe2 ),	/* Offset= -30 (856) */
/* 888 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 890 */	NdrFcShort( 0x0 ),	/* 0 */
/* 892 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 894 */	NdrFcShort( 0x0 ),	/* 0 */
/* 896 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 898 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 902 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 904 */	
			0x13, 0x0,	/* FC_OP */
/* 906 */	NdrFcShort( 0xffda ),	/* Offset= -38 (868) */
/* 908 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 910 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 912 */	NdrFcShort( 0x10 ),	/* 16 */
/* 914 */	NdrFcShort( 0x0 ),	/* 0 */
/* 916 */	NdrFcShort( 0x6 ),	/* Offset= 6 (922) */
/* 918 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 920 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 922 */	
			0x11, 0x0,	/* FC_RP */
/* 924 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (888) */
/* 926 */	
			0x1d,		/* FC_SMFARRAY */
			0x0,		/* 0 */
/* 928 */	NdrFcShort( 0x8 ),	/* 8 */
/* 930 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 932 */	
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 934 */	NdrFcShort( 0x10 ),	/* 16 */
/* 936 */	0x8,		/* FC_LONG */
			0x6,		/* FC_SHORT */
/* 938 */	0x6,		/* FC_SHORT */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 940 */	0x0,		/* 0 */
			NdrFcShort( 0xfff1 ),	/* Offset= -15 (926) */
			0x5b,		/* FC_END */
/* 944 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 946 */	NdrFcShort( 0x20 ),	/* 32 */
/* 948 */	NdrFcShort( 0x0 ),	/* 0 */
/* 950 */	NdrFcShort( 0xa ),	/* Offset= 10 (960) */
/* 952 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 954 */	0x36,		/* FC_POINTER */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 956 */	0x0,		/* 0 */
			NdrFcShort( 0xffe7 ),	/* Offset= -25 (932) */
			0x5b,		/* FC_END */
/* 960 */	
			0x11, 0x0,	/* FC_RP */
/* 962 */	NdrFcShort( 0xff12 ),	/* Offset= -238 (724) */
/* 964 */	
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 966 */	NdrFcShort( 0x1 ),	/* 1 */
/* 968 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 970 */	NdrFcShort( 0x0 ),	/* 0 */
/* 972 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 974 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 976 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 978 */	NdrFcShort( 0x10 ),	/* 16 */
/* 980 */	NdrFcShort( 0x0 ),	/* 0 */
/* 982 */	NdrFcShort( 0x6 ),	/* Offset= 6 (988) */
/* 984 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 986 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 988 */	
			0x13, 0x0,	/* FC_OP */
/* 990 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (964) */
/* 992 */	
			0x1b,		/* FC_CARRAY */
			0x1,		/* 1 */
/* 994 */	NdrFcShort( 0x2 ),	/* 2 */
/* 996 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 998 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1000 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 1002 */	0x6,		/* FC_SHORT */
			0x5b,		/* FC_END */
/* 1004 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1006 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1008 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1010 */	NdrFcShort( 0x6 ),	/* Offset= 6 (1016) */
/* 1012 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 1014 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 1016 */	
			0x13, 0x0,	/* FC_OP */
/* 1018 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (992) */
/* 1020 */	
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 1022 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1024 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 1026 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1028 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 1030 */	0x8,		/* FC_LONG */
			0x5b,		/* FC_END */
/* 1032 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1034 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1036 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1038 */	NdrFcShort( 0x6 ),	/* Offset= 6 (1044) */
/* 1040 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 1042 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 1044 */	
			0x13, 0x0,	/* FC_OP */
/* 1046 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (1020) */
/* 1048 */	
			0x1b,		/* FC_CARRAY */
			0x7,		/* 7 */
/* 1050 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1052 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 1054 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1056 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 1058 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 1060 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1062 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1064 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1066 */	NdrFcShort( 0x6 ),	/* Offset= 6 (1072) */
/* 1068 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 1070 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 1072 */	
			0x13, 0x0,	/* FC_OP */
/* 1074 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (1048) */
/* 1076 */	
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 1078 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1080 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 1082 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1084 */	
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 1086 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1088 */	0x7,		/* Corr desc: FC_USHORT */
			0x0,		/*  */
/* 1090 */	NdrFcShort( 0xffc8 ),	/* -56 */
/* 1092 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 1094 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1096 */	NdrFcShort( 0xffec ),	/* Offset= -20 (1076) */
/* 1098 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1100 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1102 */	NdrFcShort( 0x38 ),	/* 56 */
/* 1104 */	NdrFcShort( 0xffec ),	/* Offset= -20 (1084) */
/* 1106 */	NdrFcShort( 0x0 ),	/* Offset= 0 (1106) */
/* 1108 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 1110 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 1112 */	0x40,		/* FC_STRUCTPAD4 */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 1114 */	0x0,		/* 0 */
			NdrFcShort( 0xfe0f ),	/* Offset= -497 (618) */
			0x5b,		/* FC_END */
/* 1118 */	
			0x13, 0x0,	/* FC_OP */
/* 1120 */	NdrFcShort( 0xff04 ),	/* Offset= -252 (868) */
/* 1122 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 1124 */	0x1,		/* FC_BYTE */
			0x5c,		/* FC_PAD */
/* 1126 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 1128 */	0x6,		/* FC_SHORT */
			0x5c,		/* FC_PAD */
/* 1130 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 1132 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */
/* 1134 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 1136 */	0xb,		/* FC_HYPER */
			0x5c,		/* FC_PAD */
/* 1138 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 1140 */	0xa,		/* FC_FLOAT */
			0x5c,		/* FC_PAD */
/* 1142 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 1144 */	0xc,		/* FC_DOUBLE */
			0x5c,		/* FC_PAD */
/* 1146 */	
			0x13, 0x0,	/* FC_OP */
/* 1148 */	NdrFcShort( 0xfdce ),	/* Offset= -562 (586) */
/* 1150 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 1152 */	NdrFcShort( 0xfb86 ),	/* Offset= -1146 (6) */
/* 1154 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 1156 */	NdrFcShort( 0xfbae ),	/* Offset= -1106 (50) */
/* 1158 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 1160 */	NdrFcShort( 0xfdc8 ),	/* Offset= -568 (592) */
/* 1162 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 1164 */	NdrFcShort( 0xfdd6 ),	/* Offset= -554 (610) */
/* 1166 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 1168 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1170) */
/* 1170 */	
			0x13, 0x0,	/* FC_OP */
/* 1172 */	NdrFcShort( 0x14 ),	/* Offset= 20 (1192) */
/* 1174 */	
			0x15,		/* FC_STRUCT */
			0x7,		/* 7 */
/* 1176 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1178 */	0x6,		/* FC_SHORT */
			0x1,		/* FC_BYTE */
/* 1180 */	0x1,		/* FC_BYTE */
			0x8,		/* FC_LONG */
/* 1182 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 1184 */	
			0x13, 0x0,	/* FC_OP */
/* 1186 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (1174) */
/* 1188 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 1190 */	0x2,		/* FC_CHAR */
			0x5c,		/* FC_PAD */
/* 1192 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x7,		/* 7 */
/* 1194 */	NdrFcShort( 0x20 ),	/* 32 */
/* 1196 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1198 */	NdrFcShort( 0x0 ),	/* Offset= 0 (1198) */
/* 1200 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 1202 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 1204 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 1206 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1208 */	NdrFcShort( 0xfc68 ),	/* Offset= -920 (288) */
/* 1210 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1212 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 1214 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1216 */	NdrFcShort( 0x18 ),	/* 24 */
/* 1218 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1220 */	NdrFcShort( 0xfc58 ),	/* Offset= -936 (284) */
/* 1222 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 1224 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1226) */
/* 1226 */	
			0x13, 0x0,	/* FC_OP */
/* 1228 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1230) */
/* 1230 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 1232 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1234 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 1236 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1238 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1240 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 1244 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1246 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1248 */	NdrFcShort( 0xfb52 ),	/* Offset= -1198 (50) */
/* 1250 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1252 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 1254 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1256) */
/* 1256 */	
			0x13, 0x0,	/* FC_OP */
/* 1258 */	NdrFcShort( 0x18 ),	/* Offset= 24 (1282) */
/* 1260 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1262 */	NdrFcShort( 0x20 ),	/* 32 */
/* 1264 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1266 */	NdrFcShort( 0x10 ),	/* Offset= 16 (1282) */
/* 1268 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1270 */	NdrFcShort( 0xfb3c ),	/* Offset= -1220 (50) */
/* 1272 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 1274 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1276 */	NdrFcShort( 0xfb36 ),	/* Offset= -1226 (50) */
/* 1278 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 1280 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1282 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 1284 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1286 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 1288 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1290 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1292 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 1296 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1298 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1300 */	NdrFcShort( 0xffd8 ),	/* Offset= -40 (1260) */
/* 1302 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1304 */	
			0x11, 0x0,	/* FC_RP */
/* 1306 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1308) */
/* 1308 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 1310 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1312 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x0,		/*  */
/* 1314 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1316 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 1318 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 1322 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1324 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1326 */	NdrFcShort( 0xffbe ),	/* Offset= -66 (1260) */
/* 1328 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1330 */	
			0x11, 0x0,	/* FC_RP */
/* 1332 */	NdrFcShort( 0x6 ),	/* Offset= 6 (1338) */
/* 1334 */	
			0x12, 0x0,	/* FC_UP */
/* 1336 */	NdrFcShort( 0xff70 ),	/* Offset= -144 (1192) */
/* 1338 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 1340 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1342 */	NdrFcShort( 0x18 ),	/* 24 */
/* 1344 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1346 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (1334) */
/* 1348 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 1350 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1352) */
/* 1352 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1354 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1356 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1358 */	NdrFcShort( 0x0 ),	/* Offset= 0 (1358) */
/* 1360 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1362 */	NdrFcShort( 0xface ),	/* Offset= -1330 (32) */
/* 1364 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 1366 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1368 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 1370 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1372) */
/* 1372 */	
			0x13, 0x0,	/* FC_OP */
/* 1374 */	NdrFcShort( 0x2a ),	/* Offset= 42 (1416) */
/* 1376 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1378 */	NdrFcLong( 0x24fd2ffb ),	/* 620572667 */
/* 1382 */	NdrFcShort( 0x3aad ),	/* 15021 */
/* 1384 */	NdrFcShort( 0x4a08 ),	/* 18952 */
/* 1386 */	0x83,		/* 131 */
			0x35,		/* 53 */
/* 1388 */	0xa3,		/* 163 */
			0xad,		/* 173 */
/* 1390 */	0x89,		/* 137 */
			0xc0,		/* 192 */
/* 1392 */	0xfb,		/* 251 */
			0x4b,		/* 75 */
/* 1394 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1396 */	NdrFcShort( 0x20 ),	/* 32 */
/* 1398 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1400 */	NdrFcShort( 0x10 ),	/* Offset= 16 (1416) */
/* 1402 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1404 */	NdrFcShort( 0xffe4 ),	/* Offset= -28 (1376) */
/* 1406 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 1408 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1410 */	NdrFcShort( 0xffde ),	/* Offset= -34 (1376) */
/* 1412 */	0x8,		/* FC_LONG */
			0x3,		/* FC_SMALL */
/* 1414 */	0x3f,		/* FC_STRUCTPAD3 */
			0x5b,		/* FC_END */
/* 1416 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 1418 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1420 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 1422 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1424 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1426 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 1430 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1432 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1434 */	NdrFcShort( 0xffd8 ),	/* Offset= -40 (1394) */
/* 1436 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1438 */	
			0x11, 0x0,	/* FC_RP */
/* 1440 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1442) */
/* 1442 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 1444 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1446 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x0,		/*  */
/* 1448 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1450 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 1452 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 1456 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1458 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1460 */	NdrFcShort( 0xffbe ),	/* Offset= -66 (1394) */
/* 1462 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1464 */	
			0x11, 0x0,	/* FC_RP */
/* 1466 */	NdrFcShort( 0xfb54 ),	/* Offset= -1196 (270) */
/* 1468 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 1470 */	0x3,		/* FC_SMALL */
			0x5c,		/* FC_PAD */
/* 1472 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 1474 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1476) */
/* 1476 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1478 */	NdrFcLong( 0x1c20f2b ),	/* 29495083 */
/* 1482 */	NdrFcShort( 0x3dd2 ),	/* 15826 */
/* 1484 */	NdrFcShort( 0x400f ),	/* 16399 */
/* 1486 */	0x94,		/* 148 */
			0x9f,		/* 159 */
/* 1488 */	0xad,		/* 173 */
			0x0,		/* 0 */
/* 1490 */	0xbd,		/* 189 */
			0xab,		/* 171 */
/* 1492 */	0x1d,		/* 29 */
			0x41,		/* 65 */
/* 1494 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 1496 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1498) */
/* 1498 */	
			0x13, 0x0,	/* FC_OP */
/* 1500 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1502) */
/* 1502 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 1504 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1506 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 1508 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1510 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1512 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 1516 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1518 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1520 */	NdrFcShort( 0xffd4 ),	/* Offset= -44 (1476) */
/* 1522 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1524 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 1526 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1528) */
/* 1528 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1530 */	NdrFcLong( 0x35ad8070 ),	/* 900563056 */
/* 1534 */	NdrFcShort( 0xc20c ),	/* -15860 */
/* 1536 */	NdrFcShort( 0x4fb4 ),	/* 20404 */
/* 1538 */	0xb0,		/* 176 */
			0x94,		/* 148 */
/* 1540 */	0xf4,		/* 244 */
			0xf7,		/* 247 */
/* 1542 */	0x27,		/* 39 */
			0x5d,		/* 93 */
/* 1544 */	0xd4,		/* 212 */
			0x69,		/* 105 */
/* 1546 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 1548 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1550) */
/* 1550 */	
			0x13, 0x0,	/* FC_OP */
/* 1552 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1554) */
/* 1554 */	
			0x1c,		/* FC_CVARRAY */
			0x3,		/* 3 */
/* 1556 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1558 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x0,		/*  */
/* 1560 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1562 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 1564 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 1566 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1568 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1570 */	0x8,		/* FC_LONG */
			0x5b,		/* FC_END */
/* 1572 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 1574 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1576) */
/* 1576 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1578 */	NdrFcShort( 0x14 ),	/* 20 */
/* 1580 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1582 */	NdrFcShort( 0x0 ),	/* Offset= 0 (1582) */
/* 1584 */	0xd,		/* FC_ENUM16 */
			0x8,		/* FC_LONG */
/* 1586 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 1588 */	0x8,		/* FC_LONG */
			0x5b,		/* FC_END */
/* 1590 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 1592 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1594) */
/* 1594 */	
			0x13, 0x0,	/* FC_OP */
/* 1596 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1598) */
/* 1598 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 1600 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1602 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 1604 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1606 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1608 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 1612 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1614 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1616 */	NdrFcShort( 0xf9e2 ),	/* Offset= -1566 (50) */
/* 1618 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1620 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 1622 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1624) */
/* 1624 */	
			0x13, 0x0,	/* FC_OP */
/* 1626 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1628) */
/* 1628 */	
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 1630 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1632 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 1634 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1636 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1638 */	0x8,		/* FC_LONG */
			0x5b,		/* FC_END */

			0x0
        }
    };

XFG_TRAMPOLINES(BSTR)
XFG_TRAMPOLINES(HWND)
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
            (USER_MARSHAL_SIZING_ROUTINE)XFG_TRAMPOLINE_FPTR(HWND_UserSize)
            ,(USER_MARSHAL_MARSHALLING_ROUTINE)XFG_TRAMPOLINE_FPTR(HWND_UserMarshal)
            ,(USER_MARSHAL_UNMARSHALLING_ROUTINE)XFG_TRAMPOLINE_FPTR(HWND_UserUnmarshal)
            ,(USER_MARSHAL_FREEING_ROUTINE)XFG_TRAMPOLINE_FPTR(HWND_UserFree)
            
            }
            ,
            {
            (USER_MARSHAL_SIZING_ROUTINE)XFG_TRAMPOLINE_FPTR(VARIANT_UserSize)
            ,(USER_MARSHAL_MARSHALLING_ROUTINE)XFG_TRAMPOLINE_FPTR(VARIANT_UserMarshal)
            ,(USER_MARSHAL_UNMARSHALLING_ROUTINE)XFG_TRAMPOLINE_FPTR(VARIANT_UserUnmarshal)
            ,(USER_MARSHAL_FREEING_ROUTINE)XFG_TRAMPOLINE_FPTR(VARIANT_UserFree)
            
            }
            

        };



/* Standard interface: __MIDL_itf_ia2_api_all_0000_0000, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IUnknown, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IAccessibleRelation, ver. 0.0,
   GUID={0x7CDF86EE,0xC3DA,0x496a,{0xBD,0xA4,0x28,0x1B,0x33,0x6E,0x1F,0xDC}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleRelation_FormatStringOffsetTable[] =
    {
    0,
    38,
    76,
    114,
    158
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleRelation_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleRelation_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleRelation_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleRelation_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(8) _IAccessibleRelationProxyVtbl = 
{
    &IAccessibleRelation_ProxyInfo,
    &IID_IAccessibleRelation,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleRelation::get_relationType */ ,
    (void *) (INT_PTR) -1 /* IAccessibleRelation::get_localizedRelationType */ ,
    (void *) (INT_PTR) -1 /* IAccessibleRelation::get_nTargets */ ,
    (void *) (INT_PTR) -1 /* IAccessibleRelation::get_target */ ,
    (void *) (INT_PTR) -1 /* IAccessibleRelation::get_targets */
};

const CInterfaceStubVtbl _IAccessibleRelationStubVtbl =
{
    &IID_IAccessibleRelation,
    &IAccessibleRelation_ServerInfo,
    8,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Standard interface: __MIDL_itf_ia2_api_all_0000_0001, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IAccessibleAction, ver. 0.0,
   GUID={0xB70D9F59,0x3B5A,0x4dba,{0xAB,0x9E,0x22,0x01,0x2F,0x60,0x7D,0xF5}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleAction_FormatStringOffsetTable[] =
    {
    208,
    246,
    284,
    328,
    384,
    428
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleAction_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleAction_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleAction_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleAction_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(9) _IAccessibleActionProxyVtbl = 
{
    &IAccessibleAction_ProxyInfo,
    &IID_IAccessibleAction,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::nActions */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::doAction */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_description */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_keyBinding */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_name */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_localizedName */
};

const CInterfaceStubVtbl _IAccessibleActionStubVtbl =
{
    &IID_IAccessibleAction,
    &IAccessibleAction_ServerInfo,
    9,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Standard interface: __MIDL_itf_ia2_api_all_0000_0002, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IDispatch, ver. 0.0,
   GUID={0x00020400,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IAccessible, ver. 0.0,
   GUID={0x618736e0,0x3c3d,0x11cf,{0x81,0x0c,0x00,0xaa,0x00,0x38,0x9b,0x71}} */


/* Object interface: IAccessible2, ver. 0.0,
   GUID={0xE89F726E,0xC4F4,0x4c19,{0xBB,0x19,0xB6,0x47,0xD7,0xFA,0x84,0x78}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessible2_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    472,
    510,
    554,
    604,
    642,
    680,
    730,
    780,
    818,
    856,
    894,
    932,
    982,
    1032,
    1070,
    1108,
    1146,
    1184
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessible2_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessible2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessible2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessible2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(46) _IAccessible2ProxyVtbl = 
{
    &IAccessible2_ProxyInfo,
    &IID_IAccessible2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    0 /* IAccessible::get_accParent */ ,
    0 /* IAccessible::get_accChildCount */ ,
    0 /* IAccessible::get_accChild */ ,
    0 /* IAccessible::get_accName */ ,
    0 /* IAccessible::get_accValue */ ,
    0 /* IAccessible::get_accDescription */ ,
    0 /* IAccessible::get_accRole */ ,
    0 /* IAccessible::get_accState */ ,
    0 /* IAccessible::get_accHelp */ ,
    0 /* IAccessible::get_accHelpTopic */ ,
    0 /* IAccessible::get_accKeyboardShortcut */ ,
    0 /* IAccessible::get_accFocus */ ,
    0 /* IAccessible::get_accSelection */ ,
    0 /* IAccessible::get_accDefaultAction */ ,
    0 /* IAccessible::accSelect */ ,
    0 /* IAccessible::accLocation */ ,
    0 /* IAccessible::accNavigate */ ,
    0 /* IAccessible::accHitTest */ ,
    0 /* IAccessible::accDoDefaultAction */ ,
    0 /* IAccessible::put_accName */ ,
    0 /* IAccessible::put_accValue */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_nRelations */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_relation */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_relations */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::role */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::scrollTo */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::scrollToPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_groupPosition */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_states */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_extendedRole */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_localizedExtendedRole */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_nExtendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_extendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_localizedExtendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_uniqueID */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_windowHandle */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_indexInParent */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_locale */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_attributes */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAccessible2_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
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

CInterfaceStubVtbl _IAccessible2StubVtbl =
{
    &IID_IAccessible2,
    &IAccessible2_ServerInfo,
    46,
    &IAccessible2_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAccessible2_2, ver. 0.0,
   GUID={0x6C9430E9,0x299D,0x4E6F,{0xBD,0x01,0xA8,0x2A,0x1E,0x88,0xD3,0xFF}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessible2_2_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    472,
    510,
    554,
    604,
    642,
    680,
    730,
    780,
    818,
    856,
    894,
    932,
    982,
    1032,
    1070,
    1108,
    1146,
    1184,
    1222,
    1266,
    1310
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessible2_2_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessible2_2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessible2_2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessible2_2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(49) _IAccessible2_2ProxyVtbl = 
{
    &IAccessible2_2_ProxyInfo,
    &IID_IAccessible2_2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    0 /* IAccessible::get_accParent */ ,
    0 /* IAccessible::get_accChildCount */ ,
    0 /* IAccessible::get_accChild */ ,
    0 /* IAccessible::get_accName */ ,
    0 /* IAccessible::get_accValue */ ,
    0 /* IAccessible::get_accDescription */ ,
    0 /* IAccessible::get_accRole */ ,
    0 /* IAccessible::get_accState */ ,
    0 /* IAccessible::get_accHelp */ ,
    0 /* IAccessible::get_accHelpTopic */ ,
    0 /* IAccessible::get_accKeyboardShortcut */ ,
    0 /* IAccessible::get_accFocus */ ,
    0 /* IAccessible::get_accSelection */ ,
    0 /* IAccessible::get_accDefaultAction */ ,
    0 /* IAccessible::accSelect */ ,
    0 /* IAccessible::accLocation */ ,
    0 /* IAccessible::accNavigate */ ,
    0 /* IAccessible::accHitTest */ ,
    0 /* IAccessible::accDoDefaultAction */ ,
    0 /* IAccessible::put_accName */ ,
    0 /* IAccessible::put_accValue */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_nRelations */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_relation */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_relations */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::role */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::scrollTo */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::scrollToPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_groupPosition */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_states */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_extendedRole */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_localizedExtendedRole */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_nExtendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_extendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_localizedExtendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_uniqueID */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_windowHandle */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_indexInParent */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_locale */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_attributes */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_2::get_attribute */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_2::get_accessibleWithCaret */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_2::get_relationTargetsOfType */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAccessible2_2_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
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
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAccessible2_2StubVtbl =
{
    &IID_IAccessible2_2,
    &IAccessible2_2_ServerInfo,
    49,
    &IAccessible2_2_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Standard interface: __MIDL_itf_ia2_api_all_0000_0004, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IAccessible2_3, ver. 0.0,
   GUID={0x5BE18059,0x762E,0x4E73,{0x94,0x76,0xAB,0xA2,0x94,0xFE,0xD4,0x11}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessible2_3_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    472,
    510,
    554,
    604,
    642,
    680,
    730,
    780,
    818,
    856,
    894,
    932,
    982,
    1032,
    1070,
    1108,
    1146,
    1184,
    1222,
    1266,
    1310,
    1366
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessible2_3_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessible2_3_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessible2_3_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessible2_3_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(50) _IAccessible2_3ProxyVtbl = 
{
    &IAccessible2_3_ProxyInfo,
    &IID_IAccessible2_3,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    0 /* IAccessible::get_accParent */ ,
    0 /* IAccessible::get_accChildCount */ ,
    0 /* IAccessible::get_accChild */ ,
    0 /* IAccessible::get_accName */ ,
    0 /* IAccessible::get_accValue */ ,
    0 /* IAccessible::get_accDescription */ ,
    0 /* IAccessible::get_accRole */ ,
    0 /* IAccessible::get_accState */ ,
    0 /* IAccessible::get_accHelp */ ,
    0 /* IAccessible::get_accHelpTopic */ ,
    0 /* IAccessible::get_accKeyboardShortcut */ ,
    0 /* IAccessible::get_accFocus */ ,
    0 /* IAccessible::get_accSelection */ ,
    0 /* IAccessible::get_accDefaultAction */ ,
    0 /* IAccessible::accSelect */ ,
    0 /* IAccessible::accLocation */ ,
    0 /* IAccessible::accNavigate */ ,
    0 /* IAccessible::accHitTest */ ,
    0 /* IAccessible::accDoDefaultAction */ ,
    0 /* IAccessible::put_accName */ ,
    0 /* IAccessible::put_accValue */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_nRelations */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_relation */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_relations */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::role */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::scrollTo */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::scrollToPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_groupPosition */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_states */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_extendedRole */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_localizedExtendedRole */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_nExtendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_extendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_localizedExtendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_uniqueID */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_windowHandle */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_indexInParent */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_locale */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_attributes */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_2::get_attribute */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_2::get_accessibleWithCaret */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_2::get_relationTargetsOfType */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_3::get_selectionRanges */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAccessible2_3_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
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
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAccessible2_3StubVtbl =
{
    &IID_IAccessible2_3,
    &IAccessible2_3_ServerInfo,
    50,
    &IAccessible2_3_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAccessible2_4, ver. 0.0,
   GUID={0x610a7bec,0x91bb,0x444d,{0xa3,0x36,0xa0,0xda,0xf1,0x3c,0x4c,0x29}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessible2_4_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    472,
    510,
    554,
    604,
    642,
    680,
    730,
    780,
    818,
    856,
    894,
    932,
    982,
    1032,
    1070,
    1108,
    1146,
    1184,
    1222,
    1266,
    1310,
    1366,
    1410
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessible2_4_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessible2_4_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessible2_4_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessible2_4_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(51) _IAccessible2_4ProxyVtbl = 
{
    &IAccessible2_4_ProxyInfo,
    &IID_IAccessible2_4,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    0 /* IAccessible::get_accParent */ ,
    0 /* IAccessible::get_accChildCount */ ,
    0 /* IAccessible::get_accChild */ ,
    0 /* IAccessible::get_accName */ ,
    0 /* IAccessible::get_accValue */ ,
    0 /* IAccessible::get_accDescription */ ,
    0 /* IAccessible::get_accRole */ ,
    0 /* IAccessible::get_accState */ ,
    0 /* IAccessible::get_accHelp */ ,
    0 /* IAccessible::get_accHelpTopic */ ,
    0 /* IAccessible::get_accKeyboardShortcut */ ,
    0 /* IAccessible::get_accFocus */ ,
    0 /* IAccessible::get_accSelection */ ,
    0 /* IAccessible::get_accDefaultAction */ ,
    0 /* IAccessible::accSelect */ ,
    0 /* IAccessible::accLocation */ ,
    0 /* IAccessible::accNavigate */ ,
    0 /* IAccessible::accHitTest */ ,
    0 /* IAccessible::accDoDefaultAction */ ,
    0 /* IAccessible::put_accName */ ,
    0 /* IAccessible::put_accValue */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_nRelations */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_relation */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_relations */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::role */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::scrollTo */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::scrollToPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_groupPosition */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_states */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_extendedRole */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_localizedExtendedRole */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_nExtendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_extendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_localizedExtendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_uniqueID */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_windowHandle */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_indexInParent */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_locale */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_attributes */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_2::get_attribute */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_2::get_accessibleWithCaret */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_2::get_relationTargetsOfType */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_3::get_selectionRanges */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_4::setSelectionRanges */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IAccessible2_4_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
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
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAccessible2_4StubVtbl =
{
    &IID_IAccessible2_4,
    &IAccessible2_4_ServerInfo,
    51,
    &IAccessible2_4_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Standard interface: __MIDL_itf_ia2_api_all_0000_0006, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IAccessibleComponent, ver. 0.0,
   GUID={0x1546D4B0,0x4C98,0x4bda,{0x89,0xAE,0x9A,0x64,0x74,0x8B,0xDD,0xE4}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleComponent_FormatStringOffsetTable[] =
    {
    1454,
    1498,
    76
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleComponent_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleComponent_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleComponent_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleComponent_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(6) _IAccessibleComponentProxyVtbl = 
{
    &IAccessibleComponent_ProxyInfo,
    &IID_IAccessibleComponent,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleComponent::get_locationInParent */ ,
    (void *) (INT_PTR) -1 /* IAccessibleComponent::get_foreground */ ,
    (void *) (INT_PTR) -1 /* IAccessibleComponent::get_background */
};

const CInterfaceStubVtbl _IAccessibleComponentStubVtbl =
{
    &IID_IAccessibleComponent,
    &IAccessibleComponent_ServerInfo,
    6,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleValue, ver. 0.0,
   GUID={0x35855B5B,0xC566,0x4fd0,{0xA7,0xB1,0xE6,0x54,0x65,0x60,0x03,0x94}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleValue_FormatStringOffsetTable[] =
    {
    1536,
    1574,
    1612,
    1650
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleValue_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleValue_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleValue_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleValue_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(7) _IAccessibleValueProxyVtbl = 
{
    &IAccessibleValue_ProxyInfo,
    &IID_IAccessibleValue,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleValue::get_currentValue */ ,
    (void *) (INT_PTR) -1 /* IAccessibleValue::setCurrentValue */ ,
    (void *) (INT_PTR) -1 /* IAccessibleValue::get_maximumValue */ ,
    (void *) (INT_PTR) -1 /* IAccessibleValue::get_minimumValue */
};

const CInterfaceStubVtbl _IAccessibleValueStubVtbl =
{
    &IID_IAccessibleValue,
    &IAccessibleValue_ServerInfo,
    7,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Standard interface: __MIDL_itf_ia2_api_all_0000_0008, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IAccessibleText, ver. 0.0,
   GUID={0x24FD2FFB,0x3AAD,0x4a08,{0x83,0x35,0xA3,0xAD,0x89,0xC0,0xFB,0x4B}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleText_FormatStringOffsetTable[] =
    {
    1688,
    1732,
    76,
    1788,
    1856,
    1894,
    1950,
    2000,
    2050,
    2112,
    2174,
    2236,
    2274,
    2312,
    2362,
    2400,
    2450,
    2512,
    2550
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleText_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleText_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleText_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleText_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(22) _IAccessibleTextProxyVtbl = 
{
    &IAccessibleText_ProxyInfo,
    &IID_IAccessibleText,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleText::addSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_attributes */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_caretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_characterExtents */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nSelections */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_offsetAtPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_selection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_text */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textBeforeOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAfterOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAtOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::removeSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setCaretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nCharacters */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringTo */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringToPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_newText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_oldText */
};

const CInterfaceStubVtbl _IAccessibleTextStubVtbl =
{
    &IID_IAccessibleText,
    &IAccessibleText_ServerInfo,
    22,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleText2, ver. 0.0,
   GUID={0x9690A9CC,0x5C80,0x4DF5,{0x85,0x2E,0x2D,0x5A,0xE4,0x18,0x9A,0x54}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleText2_FormatStringOffsetTable[] =
    {
    1688,
    1732,
    76,
    1788,
    1856,
    1894,
    1950,
    2000,
    2050,
    2112,
    2174,
    2236,
    2274,
    2312,
    2362,
    2400,
    2450,
    2512,
    2550,
    2588
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleText2_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleText2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleText2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleText2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(23) _IAccessibleText2ProxyVtbl = 
{
    &IAccessibleText2_ProxyInfo,
    &IID_IAccessibleText2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleText::addSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_attributes */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_caretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_characterExtents */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nSelections */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_offsetAtPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_selection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_text */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textBeforeOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAfterOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAtOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::removeSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setCaretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nCharacters */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringTo */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringToPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_newText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_oldText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText2::get_attributeRange */
};

const CInterfaceStubVtbl _IAccessibleText2StubVtbl =
{
    &IID_IAccessibleText2,
    &IAccessibleText2_ServerInfo,
    23,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Standard interface: __MIDL_itf_ia2_api_all_0000_0010, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IAccessibleTextSelectionContainer, ver. 0.0,
   GUID={0x2118B599,0x733F,0x43D0,{0xA5,0x69,0x0B,0x31,0xD1,0x25,0xED,0x9A}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleTextSelectionContainer_FormatStringOffsetTable[] =
    {
    2650,
    2694
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleTextSelectionContainer_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleTextSelectionContainer_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleTextSelectionContainer_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleTextSelectionContainer_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(5) _IAccessibleTextSelectionContainerProxyVtbl = 
{
    &IAccessibleTextSelectionContainer_ProxyInfo,
    &IID_IAccessibleTextSelectionContainer,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleTextSelectionContainer::get_selections */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTextSelectionContainer::setSelections */
};

const CInterfaceStubVtbl _IAccessibleTextSelectionContainerStubVtbl =
{
    &IID_IAccessibleTextSelectionContainer,
    &IAccessibleTextSelectionContainer_ServerInfo,
    5,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleEditableText, ver. 0.0,
   GUID={0xA59AA09A,0x7011,0x4b65,{0x93,0x9D,0x32,0xB1,0xFB,0x55,0x47,0xE3}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleEditableText_FormatStringOffsetTable[] =
    {
    1688,
    2738,
    2782,
    2826,
    2870,
    2908,
    2958
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleEditableText_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleEditableText_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleEditableText_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleEditableText_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(10) _IAccessibleEditableTextProxyVtbl = 
{
    &IAccessibleEditableText_ProxyInfo,
    &IID_IAccessibleEditableText,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleEditableText::copyText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleEditableText::deleteText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleEditableText::insertText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleEditableText::cutText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleEditableText::pasteText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleEditableText::replaceText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleEditableText::setAttributes */
};

const CInterfaceStubVtbl _IAccessibleEditableTextStubVtbl =
{
    &IID_IAccessibleEditableText,
    &IAccessibleEditableText_ServerInfo,
    10,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleHyperlink, ver. 0.0,
   GUID={0x01C20F2B,0x3DD2,0x400f,{0x94,0x9F,0xAD,0x00,0xBD,0xAB,0x1D,0x41}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleHyperlink_FormatStringOffsetTable[] =
    {
    208,
    246,
    284,
    328,
    384,
    428,
    3008,
    3052,
    3096,
    3134,
    3172
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleHyperlink_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleHyperlink_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleHyperlink_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleHyperlink_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(14) _IAccessibleHyperlinkProxyVtbl = 
{
    &IAccessibleHyperlink_ProxyInfo,
    &IID_IAccessibleHyperlink,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::nActions */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::doAction */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_description */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_keyBinding */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_name */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_localizedName */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHyperlink::get_anchor */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHyperlink::get_anchorTarget */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHyperlink::get_startIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHyperlink::get_endIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHyperlink::get_valid */
};

const CInterfaceStubVtbl _IAccessibleHyperlinkStubVtbl =
{
    &IID_IAccessibleHyperlink,
    &IAccessibleHyperlink_ServerInfo,
    14,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleHypertext, ver. 0.0,
   GUID={0x6B4F8BBF,0xF1F2,0x418a,{0xB3,0x5E,0xA1,0x95,0xBC,0x41,0x03,0xB9}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleHypertext_FormatStringOffsetTable[] =
    {
    1688,
    1732,
    76,
    1788,
    1856,
    1894,
    1950,
    2000,
    2050,
    2112,
    2174,
    2236,
    2274,
    2312,
    2362,
    2400,
    2450,
    2512,
    2550,
    3210,
    3248,
    3292
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleHypertext_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleHypertext_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleHypertext_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleHypertext_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(25) _IAccessibleHypertextProxyVtbl = 
{
    &IAccessibleHypertext_ProxyInfo,
    &IID_IAccessibleHypertext,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleText::addSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_attributes */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_caretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_characterExtents */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nSelections */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_offsetAtPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_selection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_text */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textBeforeOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAfterOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAtOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::removeSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setCaretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nCharacters */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringTo */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringToPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_newText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_oldText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHypertext::get_nHyperlinks */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHypertext::get_hyperlink */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHypertext::get_hyperlinkIndex */
};

const CInterfaceStubVtbl _IAccessibleHypertextStubVtbl =
{
    &IID_IAccessibleHypertext,
    &IAccessibleHypertext_ServerInfo,
    25,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleHypertext2, ver. 0.0,
   GUID={0xCF64D89F,0x8287,0x4B44,{0x85,0x01,0xA8,0x27,0x45,0x3A,0x60,0x77}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleHypertext2_FormatStringOffsetTable[] =
    {
    1688,
    1732,
    76,
    1788,
    1856,
    1894,
    1950,
    2000,
    2050,
    2112,
    2174,
    2236,
    2274,
    2312,
    2362,
    2400,
    2450,
    2512,
    2550,
    3210,
    3248,
    3292,
    3336
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleHypertext2_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleHypertext2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleHypertext2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleHypertext2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(26) _IAccessibleHypertext2ProxyVtbl = 
{
    &IAccessibleHypertext2_ProxyInfo,
    &IID_IAccessibleHypertext2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleText::addSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_attributes */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_caretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_characterExtents */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nSelections */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_offsetAtPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_selection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_text */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textBeforeOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAfterOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAtOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::removeSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setCaretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nCharacters */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringTo */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringToPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_newText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_oldText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHypertext::get_nHyperlinks */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHypertext::get_hyperlink */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHypertext::get_hyperlinkIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHypertext2::get_hyperlinks */
};

const CInterfaceStubVtbl _IAccessibleHypertext2StubVtbl =
{
    &IID_IAccessibleHypertext2,
    &IAccessibleHypertext2_ServerInfo,
    26,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleTable, ver. 0.0,
   GUID={0x35AD8070,0xC20C,0x4fb4,{0xB0,0x94,0xF4,0xF7,0x27,0x5D,0xD4,0x69}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleTable_FormatStringOffsetTable[] =
    {
    3380,
    3430,
    3468,
    3518,
    3562,
    3612,
    3656,
    3700,
    3096,
    3134,
    3738,
    3776,
    3814,
    3858,
    3908,
    3952,
    3996,
    4046,
    4096,
    4146,
    4184,
    4228,
    4272,
    4322,
    4360,
    4398,
    4436,
    4474,
    4542
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleTable_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleTable_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleTable_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleTable_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(32) _IAccessibleTableProxyVtbl = 
{
    &IAccessibleTable_ProxyInfo,
    &IID_IAccessibleTable,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_accessibleAt */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_caption */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_childIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_columnDescription */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_columnExtentAt */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_columnHeader */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_columnIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_nColumns */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_nRows */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_nSelectedChildren */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_nSelectedColumns */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_nSelectedRows */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_rowDescription */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_rowExtentAt */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_rowHeader */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_rowIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_selectedChildren */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_selectedColumns */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_selectedRows */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_summary */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_isColumnSelected */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_isRowSelected */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_isSelected */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::selectRow */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::selectColumn */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::unselectRow */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::unselectColumn */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_rowColumnExtentsAtIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_modelChange */
};

const CInterfaceStubVtbl _IAccessibleTableStubVtbl =
{
    &IID_IAccessibleTable,
    &IAccessibleTable_ServerInfo,
    32,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleTable2, ver. 0.0,
   GUID={0x6167f295,0x06f0,0x4cdd,{0xa1,0xfa,0x02,0xe2,0x51,0x53,0xd8,0x69}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleTable2_FormatStringOffsetTable[] =
    {
    3380,
    3430,
    284,
    4580,
    1856,
    4618,
    4656,
    3700,
    4694,
    4738,
    4782,
    4826,
    4870,
    4908,
    4952,
    4996,
    5034,
    5072,
    5110,
    5148
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleTable2_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleTable2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleTable2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleTable2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(23) _IAccessibleTable2ProxyVtbl = 
{
    &IAccessibleTable2_ProxyInfo,
    &IID_IAccessibleTable2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_cellAt */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_caption */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_columnDescription */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_nColumns */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_nRows */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_nSelectedCells */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_nSelectedColumns */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_nSelectedRows */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_rowDescription */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_selectedCells */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_selectedColumns */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_selectedRows */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_summary */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_isColumnSelected */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_isRowSelected */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::selectRow */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::selectColumn */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::unselectRow */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::unselectColumn */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_modelChange */
};

const CInterfaceStubVtbl _IAccessibleTable2StubVtbl =
{
    &IID_IAccessibleTable2,
    &IAccessibleTable2_ServerInfo,
    23,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleTableCell, ver. 0.0,
   GUID={0x594116B1,0xC99F,0x4847,{0xAD,0x06,0x0A,0x7A,0x86,0xEC,0xE6,0x45}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleTableCell_FormatStringOffsetTable[] =
    {
    208,
    5186,
    76,
    4580,
    5230,
    4618,
    5274,
    5312,
    5374
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleTableCell_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleTableCell_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleTableCell_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleTableCell_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(12) _IAccessibleTableCellProxyVtbl = 
{
    &IAccessibleTableCell_ProxyInfo,
    &IID_IAccessibleTableCell,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_columnExtent */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_columnHeaderCells */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_columnIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_rowExtent */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_rowHeaderCells */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_rowIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_isSelected */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_rowColumnExtents */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_table */
};

const CInterfaceStubVtbl _IAccessibleTableCellStubVtbl =
{
    &IID_IAccessibleTableCell,
    &IAccessibleTableCell_ServerInfo,
    12,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleImage, ver. 0.0,
   GUID={0xFE5ABB3D,0x615E,0x4f7b,{0x90,0x9F,0x5F,0x0E,0xDA,0x9E,0x8D,0xDE}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleImage_FormatStringOffsetTable[] =
    {
    0,
    5412,
    5462
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleImage_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleImage_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleImage_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleImage_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(6) _IAccessibleImageProxyVtbl = 
{
    &IAccessibleImage_ProxyInfo,
    &IID_IAccessibleImage,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleImage::get_description */ ,
    (void *) (INT_PTR) -1 /* IAccessibleImage::get_imagePosition */ ,
    (void *) (INT_PTR) -1 /* IAccessibleImage::get_imageSize */
};

const CInterfaceStubVtbl _IAccessibleImageStubVtbl =
{
    &IID_IAccessibleImage,
    &IAccessibleImage_ServerInfo,
    6,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Standard interface: __MIDL_itf_ia2_api_all_0000_0019, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IAccessibleApplication, ver. 0.0,
   GUID={0xD49DED83,0x5B25,0x43F4,{0x9B,0x95,0x93,0xB4,0x45,0x95,0x97,0x9E}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleApplication_FormatStringOffsetTable[] =
    {
    0,
    38,
    5506,
    5544
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleApplication_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleApplication_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleApplication_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleApplication_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(7) _IAccessibleApplicationProxyVtbl = 
{
    &IAccessibleApplication_ProxyInfo,
    &IID_IAccessibleApplication,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleApplication::get_appName */ ,
    (void *) (INT_PTR) -1 /* IAccessibleApplication::get_appVersion */ ,
    (void *) (INT_PTR) -1 /* IAccessibleApplication::get_toolkitName */ ,
    (void *) (INT_PTR) -1 /* IAccessibleApplication::get_toolkitVersion */
};

const CInterfaceStubVtbl _IAccessibleApplicationStubVtbl =
{
    &IID_IAccessibleApplication,
    &IAccessibleApplication_ServerInfo,
    7,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleDocument, ver. 0.0,
   GUID={0xC48C7FCF,0x4AB5,0x4056,{0xAF,0xA6,0x90,0x2D,0x6E,0x1D,0x11,0x49}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleDocument_FormatStringOffsetTable[] =
    {
    5582
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleDocument_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleDocument_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleDocument_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleDocument_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IAccessibleDocumentProxyVtbl = 
{
    &IAccessibleDocument_ProxyInfo,
    &IID_IAccessibleDocument,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleDocument::get_anchorTarget */
};

const CInterfaceStubVtbl _IAccessibleDocumentStubVtbl =
{
    &IID_IAccessibleDocument,
    &IAccessibleDocument_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Standard interface: __MIDL_itf_ia2_api_all_0000_0021, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */

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
    ia2_api_all__MIDL_TypeFormatString.Format,
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

const CInterfaceProxyVtbl * const _ia2_api_all_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_IAccessibleHyperlinkProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleImageProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessible2_3ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleActionProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleValueProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessible2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleTableProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleApplicationProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleTable2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleTextSelectionContainerProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleEditableTextProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleHypertext2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleComponentProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleTableCellProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleHypertextProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleText2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleDocumentProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessible2_2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessible2_4ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleRelationProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleTextProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _ia2_api_all_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_IAccessibleHyperlinkStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleImageStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessible2_3StubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleActionStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleValueStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessible2StubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleTableStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleApplicationStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleTable2StubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleTextSelectionContainerStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleEditableTextStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleHypertext2StubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleComponentStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleTableCellStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleHypertextStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleText2StubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleDocumentStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessible2_2StubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessible2_4StubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleRelationStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleTextStubVtbl,
    0
};

PCInterfaceName const _ia2_api_all_InterfaceNamesList[] = 
{
    "IAccessibleHyperlink",
    "IAccessibleImage",
    "IAccessible2_3",
    "IAccessibleAction",
    "IAccessibleValue",
    "IAccessible2",
    "IAccessibleTable",
    "IAccessibleApplication",
    "IAccessibleTable2",
    "IAccessibleTextSelectionContainer",
    "IAccessibleEditableText",
    "IAccessibleHypertext2",
    "IAccessibleComponent",
    "IAccessibleTableCell",
    "IAccessibleHypertext",
    "IAccessibleText2",
    "IAccessibleDocument",
    "IAccessible2_2",
    "IAccessible2_4",
    "IAccessibleRelation",
    "IAccessibleText",
    0
};

const IID *  const _ia2_api_all_BaseIIDList[] = 
{
    0,
    0,
    &IID_IAccessible,
    0,
    0,
    &IID_IAccessible,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    &IID_IAccessible,
    &IID_IAccessible,
    0,
    0,
    0
};


#define _ia2_api_all_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _ia2_api_all, pIID, n)

int __stdcall _ia2_api_all_IID_Lookup( const IID * pIID, int * pIndex )
{
    IID_BS_LOOKUP_SETUP

    IID_BS_LOOKUP_INITIAL_TEST( _ia2_api_all, 21, 16 )
    IID_BS_LOOKUP_NEXT_TEST( _ia2_api_all, 8 )
    IID_BS_LOOKUP_NEXT_TEST( _ia2_api_all, 4 )
    IID_BS_LOOKUP_NEXT_TEST( _ia2_api_all, 2 )
    IID_BS_LOOKUP_NEXT_TEST( _ia2_api_all, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _ia2_api_all, 21, *pIndex )
    
}

EXTERN_C const ExtendedProxyFileInfo ia2_api_all_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _ia2_api_all_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _ia2_api_all_StubVtblList,
    (const PCInterfaceName * ) & _ia2_api_all_InterfaceNamesList,
    (const IID ** ) & _ia2_api_all_BaseIIDList,
    & _ia2_api_all_IID_Lookup, 
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

