

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../third_party/iaccessible2/ia2_api_all.idl:
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


#include "ia2_api_all.h"

#define TYPE_FORMAT_STRING_SIZE   1641                              
#define PROC_FORMAT_STRING_SIZE   6215                              
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

#if !defined(__RPC_ARM64__)
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

	/* Parameter name */


	/* Parameter description */


	/* Parameter relationType */

/* 30 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 32 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 34 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 36 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 38 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 40 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_appVersion */


	/* Procedure get_localizedRelationType */

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

	/* Parameter version */


	/* Parameter localizedRelationType */

/* 72 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 74 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 76 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */

/* 78 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 80 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 82 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnIndex */


	/* Procedure get_caretOffset */


	/* Procedure get_background */


	/* Procedure get_nTargets */

/* 84 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 86 */	NdrFcLong( 0x0 ),	/* 0 */
/* 90 */	NdrFcShort( 0x5 ),	/* 5 */
/* 92 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 94 */	NdrFcShort( 0x0 ),	/* 0 */
/* 96 */	NdrFcShort( 0x24 ),	/* 36 */
/* 98 */	0x44,		/* Oi2 Flags:  has return, has ext, */
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

	/* Parameter columnIndex */


	/* Parameter offset */


	/* Parameter background */


	/* Parameter nTargets */

/* 114 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 116 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 118 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 120 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 122 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 124 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_target */

/* 126 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 128 */	NdrFcLong( 0x0 ),	/* 0 */
/* 132 */	NdrFcShort( 0x6 ),	/* 6 */
/* 134 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 136 */	NdrFcShort( 0x8 ),	/* 8 */
/* 138 */	NdrFcShort( 0x8 ),	/* 8 */
/* 140 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 142 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 144 */	NdrFcShort( 0x0 ),	/* 0 */
/* 146 */	NdrFcShort( 0x0 ),	/* 0 */
/* 148 */	NdrFcShort( 0x0 ),	/* 0 */
/* 150 */	NdrFcShort( 0x3 ),	/* 3 */
/* 152 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 154 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter targetIndex */

/* 156 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 158 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 160 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter target */

/* 162 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 164 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 166 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 168 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 170 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 172 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_targets */

/* 174 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 176 */	NdrFcLong( 0x0 ),	/* 0 */
/* 180 */	NdrFcShort( 0x7 ),	/* 7 */
/* 182 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 184 */	NdrFcShort( 0x8 ),	/* 8 */
/* 186 */	NdrFcShort( 0x24 ),	/* 36 */
/* 188 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 190 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 192 */	NdrFcShort( 0x1 ),	/* 1 */
/* 194 */	NdrFcShort( 0x0 ),	/* 0 */
/* 196 */	NdrFcShort( 0x0 ),	/* 0 */
/* 198 */	NdrFcShort( 0x4 ),	/* 4 */
/* 200 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 202 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 204 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter maxTargets */

/* 206 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 208 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 210 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter targets */

/* 212 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 214 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 216 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter nTargets */

/* 218 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 220 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 222 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 224 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 226 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 228 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnExtent */


	/* Procedure nActions */

/* 230 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 232 */	NdrFcLong( 0x0 ),	/* 0 */
/* 236 */	NdrFcShort( 0x3 ),	/* 3 */
/* 238 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 240 */	NdrFcShort( 0x0 ),	/* 0 */
/* 242 */	NdrFcShort( 0x24 ),	/* 36 */
/* 244 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 246 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 248 */	NdrFcShort( 0x0 ),	/* 0 */
/* 250 */	NdrFcShort( 0x0 ),	/* 0 */
/* 252 */	NdrFcShort( 0x0 ),	/* 0 */
/* 254 */	NdrFcShort( 0x2 ),	/* 2 */
/* 256 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 258 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter nColumnsSpanned */


	/* Parameter nActions */

/* 260 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 262 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 264 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 266 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 268 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 270 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure doAction */

/* 272 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 274 */	NdrFcLong( 0x0 ),	/* 0 */
/* 278 */	NdrFcShort( 0x4 ),	/* 4 */
/* 280 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 282 */	NdrFcShort( 0x8 ),	/* 8 */
/* 284 */	NdrFcShort( 0x8 ),	/* 8 */
/* 286 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 288 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 290 */	NdrFcShort( 0x0 ),	/* 0 */
/* 292 */	NdrFcShort( 0x0 ),	/* 0 */
/* 294 */	NdrFcShort( 0x0 ),	/* 0 */
/* 296 */	NdrFcShort( 0x2 ),	/* 2 */
/* 298 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 300 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter actionIndex */

/* 302 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 304 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 306 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 308 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 310 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 312 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnDescription */


	/* Procedure get_description */

/* 314 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 316 */	NdrFcLong( 0x0 ),	/* 0 */
/* 320 */	NdrFcShort( 0x5 ),	/* 5 */
/* 322 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 324 */	NdrFcShort( 0x8 ),	/* 8 */
/* 326 */	NdrFcShort( 0x8 ),	/* 8 */
/* 328 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 330 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 332 */	NdrFcShort( 0x1 ),	/* 1 */
/* 334 */	NdrFcShort( 0x0 ),	/* 0 */
/* 336 */	NdrFcShort( 0x0 ),	/* 0 */
/* 338 */	NdrFcShort( 0x3 ),	/* 3 */
/* 340 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 342 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter column */


	/* Parameter actionIndex */

/* 344 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 346 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 348 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter description */


	/* Parameter description */

/* 350 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 352 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 354 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */

/* 356 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 358 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 360 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_keyBinding */

/* 362 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 364 */	NdrFcLong( 0x0 ),	/* 0 */
/* 368 */	NdrFcShort( 0x6 ),	/* 6 */
/* 370 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 372 */	NdrFcShort( 0x10 ),	/* 16 */
/* 374 */	NdrFcShort( 0x24 ),	/* 36 */
/* 376 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x5,		/* 5 */
/* 378 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 380 */	NdrFcShort( 0x1 ),	/* 1 */
/* 382 */	NdrFcShort( 0x0 ),	/* 0 */
/* 384 */	NdrFcShort( 0x0 ),	/* 0 */
/* 386 */	NdrFcShort( 0x5 ),	/* 5 */
/* 388 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 390 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 392 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter actionIndex */

/* 394 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 396 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 398 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter nMaxBindings */

/* 400 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 402 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 404 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter keyBindings */

/* 406 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 408 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 410 */	NdrFcShort( 0x5e ),	/* Type Offset=94 */

	/* Parameter nBindings */

/* 412 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 414 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 416 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 418 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 420 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 422 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_name */

/* 424 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 426 */	NdrFcLong( 0x0 ),	/* 0 */
/* 430 */	NdrFcShort( 0x7 ),	/* 7 */
/* 432 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 434 */	NdrFcShort( 0x8 ),	/* 8 */
/* 436 */	NdrFcShort( 0x8 ),	/* 8 */
/* 438 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 440 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 442 */	NdrFcShort( 0x1 ),	/* 1 */
/* 444 */	NdrFcShort( 0x0 ),	/* 0 */
/* 446 */	NdrFcShort( 0x0 ),	/* 0 */
/* 448 */	NdrFcShort( 0x3 ),	/* 3 */
/* 450 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 452 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter actionIndex */

/* 454 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 456 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 458 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter name */

/* 460 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 462 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 464 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 466 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 468 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 470 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_localizedName */

/* 472 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 474 */	NdrFcLong( 0x0 ),	/* 0 */
/* 478 */	NdrFcShort( 0x8 ),	/* 8 */
/* 480 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 482 */	NdrFcShort( 0x8 ),	/* 8 */
/* 484 */	NdrFcShort( 0x8 ),	/* 8 */
/* 486 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 488 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 490 */	NdrFcShort( 0x1 ),	/* 1 */
/* 492 */	NdrFcShort( 0x0 ),	/* 0 */
/* 494 */	NdrFcShort( 0x0 ),	/* 0 */
/* 496 */	NdrFcShort( 0x3 ),	/* 3 */
/* 498 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 500 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter actionIndex */

/* 502 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 504 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 506 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter localizedName */

/* 508 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 510 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 512 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 514 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 516 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 518 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nRelations */

/* 520 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 522 */	NdrFcLong( 0x0 ),	/* 0 */
/* 526 */	NdrFcShort( 0x1c ),	/* 28 */
/* 528 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 530 */	NdrFcShort( 0x0 ),	/* 0 */
/* 532 */	NdrFcShort( 0x24 ),	/* 36 */
/* 534 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 536 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 538 */	NdrFcShort( 0x0 ),	/* 0 */
/* 540 */	NdrFcShort( 0x0 ),	/* 0 */
/* 542 */	NdrFcShort( 0x0 ),	/* 0 */
/* 544 */	NdrFcShort( 0x2 ),	/* 2 */
/* 546 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 548 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter nRelations */

/* 550 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 552 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 554 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 556 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 558 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 560 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_relation */

/* 562 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 564 */	NdrFcLong( 0x0 ),	/* 0 */
/* 568 */	NdrFcShort( 0x1d ),	/* 29 */
/* 570 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 572 */	NdrFcShort( 0x8 ),	/* 8 */
/* 574 */	NdrFcShort( 0x8 ),	/* 8 */
/* 576 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 578 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 580 */	NdrFcShort( 0x0 ),	/* 0 */
/* 582 */	NdrFcShort( 0x0 ),	/* 0 */
/* 584 */	NdrFcShort( 0x0 ),	/* 0 */
/* 586 */	NdrFcShort( 0x3 ),	/* 3 */
/* 588 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 590 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter relationIndex */

/* 592 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 594 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 596 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter relation */

/* 598 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 600 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 602 */	NdrFcShort( 0x7c ),	/* Type Offset=124 */

	/* Return value */

/* 604 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 606 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 608 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_relations */

/* 610 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 612 */	NdrFcLong( 0x0 ),	/* 0 */
/* 616 */	NdrFcShort( 0x1e ),	/* 30 */
/* 618 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 620 */	NdrFcShort( 0x8 ),	/* 8 */
/* 622 */	NdrFcShort( 0x24 ),	/* 36 */
/* 624 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 626 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 628 */	NdrFcShort( 0x1 ),	/* 1 */
/* 630 */	NdrFcShort( 0x0 ),	/* 0 */
/* 632 */	NdrFcShort( 0x0 ),	/* 0 */
/* 634 */	NdrFcShort( 0x4 ),	/* 4 */
/* 636 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 638 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 640 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter maxRelations */

/* 642 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 644 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 646 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter relations */

/* 648 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 650 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 652 */	NdrFcShort( 0x96 ),	/* Type Offset=150 */

	/* Parameter nRelations */

/* 654 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 656 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 658 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 660 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 662 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 664 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure role */

/* 666 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 668 */	NdrFcLong( 0x0 ),	/* 0 */
/* 672 */	NdrFcShort( 0x1f ),	/* 31 */
/* 674 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 676 */	NdrFcShort( 0x0 ),	/* 0 */
/* 678 */	NdrFcShort( 0x24 ),	/* 36 */
/* 680 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 682 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 684 */	NdrFcShort( 0x0 ),	/* 0 */
/* 686 */	NdrFcShort( 0x0 ),	/* 0 */
/* 688 */	NdrFcShort( 0x0 ),	/* 0 */
/* 690 */	NdrFcShort( 0x2 ),	/* 2 */
/* 692 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 694 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter role */

/* 696 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 698 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 700 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 702 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 704 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 706 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure scrollTo */

/* 708 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 710 */	NdrFcLong( 0x0 ),	/* 0 */
/* 714 */	NdrFcShort( 0x20 ),	/* 32 */
/* 716 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 718 */	NdrFcShort( 0x6 ),	/* 6 */
/* 720 */	NdrFcShort( 0x8 ),	/* 8 */
/* 722 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 724 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 726 */	NdrFcShort( 0x0 ),	/* 0 */
/* 728 */	NdrFcShort( 0x0 ),	/* 0 */
/* 730 */	NdrFcShort( 0x0 ),	/* 0 */
/* 732 */	NdrFcShort( 0x2 ),	/* 2 */
/* 734 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 736 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter scrollType */

/* 738 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 740 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 742 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Return value */

/* 744 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 746 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 748 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure scrollToPoint */

/* 750 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 752 */	NdrFcLong( 0x0 ),	/* 0 */
/* 756 */	NdrFcShort( 0x21 ),	/* 33 */
/* 758 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 760 */	NdrFcShort( 0x16 ),	/* 22 */
/* 762 */	NdrFcShort( 0x8 ),	/* 8 */
/* 764 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 766 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 768 */	NdrFcShort( 0x0 ),	/* 0 */
/* 770 */	NdrFcShort( 0x0 ),	/* 0 */
/* 772 */	NdrFcShort( 0x0 ),	/* 0 */
/* 774 */	NdrFcShort( 0x4 ),	/* 4 */
/* 776 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 778 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 780 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter coordinateType */

/* 782 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 784 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 786 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter x */

/* 788 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 790 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 792 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 794 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 796 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 798 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 800 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 802 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 804 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_groupPosition */

/* 806 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 808 */	NdrFcLong( 0x0 ),	/* 0 */
/* 812 */	NdrFcShort( 0x22 ),	/* 34 */
/* 814 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 816 */	NdrFcShort( 0x0 ),	/* 0 */
/* 818 */	NdrFcShort( 0x5c ),	/* 92 */
/* 820 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 822 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 824 */	NdrFcShort( 0x0 ),	/* 0 */
/* 826 */	NdrFcShort( 0x0 ),	/* 0 */
/* 828 */	NdrFcShort( 0x0 ),	/* 0 */
/* 830 */	NdrFcShort( 0x4 ),	/* 4 */
/* 832 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 834 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 836 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter groupLevel */

/* 838 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 840 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 842 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter similarItemsInGroup */

/* 844 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 846 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 848 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter positionInGroup */

/* 850 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 852 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 854 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 856 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 858 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 860 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_states */

/* 862 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 864 */	NdrFcLong( 0x0 ),	/* 0 */
/* 868 */	NdrFcShort( 0x23 ),	/* 35 */
/* 870 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 872 */	NdrFcShort( 0x0 ),	/* 0 */
/* 874 */	NdrFcShort( 0x24 ),	/* 36 */
/* 876 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 878 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 880 */	NdrFcShort( 0x0 ),	/* 0 */
/* 882 */	NdrFcShort( 0x0 ),	/* 0 */
/* 884 */	NdrFcShort( 0x0 ),	/* 0 */
/* 886 */	NdrFcShort( 0x2 ),	/* 2 */
/* 888 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 890 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter states */

/* 892 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 894 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 896 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 898 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 900 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 902 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_extendedRole */

/* 904 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 906 */	NdrFcLong( 0x0 ),	/* 0 */
/* 910 */	NdrFcShort( 0x24 ),	/* 36 */
/* 912 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 914 */	NdrFcShort( 0x0 ),	/* 0 */
/* 916 */	NdrFcShort( 0x8 ),	/* 8 */
/* 918 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 920 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 922 */	NdrFcShort( 0x1 ),	/* 1 */
/* 924 */	NdrFcShort( 0x0 ),	/* 0 */
/* 926 */	NdrFcShort( 0x0 ),	/* 0 */
/* 928 */	NdrFcShort( 0x2 ),	/* 2 */
/* 930 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 932 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter extendedRole */

/* 934 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 936 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 938 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 940 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 942 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 944 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_localizedExtendedRole */

/* 946 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 948 */	NdrFcLong( 0x0 ),	/* 0 */
/* 952 */	NdrFcShort( 0x25 ),	/* 37 */
/* 954 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 956 */	NdrFcShort( 0x0 ),	/* 0 */
/* 958 */	NdrFcShort( 0x8 ),	/* 8 */
/* 960 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 962 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 964 */	NdrFcShort( 0x1 ),	/* 1 */
/* 966 */	NdrFcShort( 0x0 ),	/* 0 */
/* 968 */	NdrFcShort( 0x0 ),	/* 0 */
/* 970 */	NdrFcShort( 0x2 ),	/* 2 */
/* 972 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 974 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter localizedExtendedRole */

/* 976 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 978 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 980 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 982 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 984 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 986 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nExtendedStates */

/* 988 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 990 */	NdrFcLong( 0x0 ),	/* 0 */
/* 994 */	NdrFcShort( 0x26 ),	/* 38 */
/* 996 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 998 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1000 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1002 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 1004 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1006 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1008 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1010 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1012 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1014 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1016 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter nExtendedStates */

/* 1018 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1020 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1022 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1024 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1026 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1028 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_extendedStates */

/* 1030 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1032 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1036 */	NdrFcShort( 0x27 ),	/* 39 */
/* 1038 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1040 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1042 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1044 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 1046 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1048 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1050 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1052 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1054 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1056 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 1058 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1060 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter maxExtendedStates */

/* 1062 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1064 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1066 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter extendedStates */

/* 1068 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 1070 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1072 */	NdrFcShort( 0xac ),	/* Type Offset=172 */

	/* Parameter nExtendedStates */

/* 1074 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1076 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1078 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1080 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1082 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1084 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_localizedExtendedStates */

/* 1086 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1088 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1092 */	NdrFcShort( 0x28 ),	/* 40 */
/* 1094 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1096 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1098 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1100 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 1102 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1104 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1106 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1108 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1110 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1112 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 1114 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1116 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter maxLocalizedExtendedStates */

/* 1118 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1120 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1122 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter localizedExtendedStates */

/* 1124 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 1126 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1128 */	NdrFcShort( 0xac ),	/* Type Offset=172 */

	/* Parameter nLocalizedExtendedStates */

/* 1130 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1132 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1134 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1136 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1138 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1140 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_uniqueID */

/* 1142 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1144 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1148 */	NdrFcShort( 0x29 ),	/* 41 */
/* 1150 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1152 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1154 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1156 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 1158 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1160 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1162 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1164 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1166 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1168 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1170 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter uniqueID */

/* 1172 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1174 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1176 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1178 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1180 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1182 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_windowHandle */

/* 1184 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1186 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1190 */	NdrFcShort( 0x2a ),	/* 42 */
/* 1192 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1194 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1196 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1198 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1200 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1202 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1204 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1206 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1208 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1210 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1212 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter windowHandle */

/* 1214 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1216 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1218 */	NdrFcShort( 0xe6 ),	/* Type Offset=230 */

	/* Return value */

/* 1220 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1222 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1224 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_indexInParent */

/* 1226 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1228 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1232 */	NdrFcShort( 0x2b ),	/* 43 */
/* 1234 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1236 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1238 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1240 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 1242 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1244 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1246 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1248 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1250 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1252 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1254 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter indexInParent */

/* 1256 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1258 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1260 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1262 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1264 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1266 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_locale */

/* 1268 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1270 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1274 */	NdrFcShort( 0x2c ),	/* 44 */
/* 1276 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1278 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1280 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1282 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1284 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1286 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1288 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1290 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1292 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1294 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1296 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter locale */

/* 1298 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1300 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1302 */	NdrFcShort( 0xf4 ),	/* Type Offset=244 */

	/* Return value */

/* 1304 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1306 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1308 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attributes */

/* 1310 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1312 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1316 */	NdrFcShort( 0x2d ),	/* 45 */
/* 1318 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1320 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1322 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1324 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1326 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1328 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1330 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1332 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1334 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1336 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1338 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter attributes */

/* 1340 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1342 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1344 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 1346 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1348 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1350 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attribute */

/* 1352 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1354 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1358 */	NdrFcShort( 0x2e ),	/* 46 */
/* 1360 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1362 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1364 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1366 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 1368 */	0xe,		/* 14 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 1370 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1372 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1374 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1376 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1378 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1380 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter name */

/* 1382 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1384 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1386 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Parameter attribute */

/* 1388 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1390 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1392 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 1394 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1396 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1398 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_accessibleWithCaret */

/* 1400 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1402 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1406 */	NdrFcShort( 0x2f ),	/* 47 */
/* 1408 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1410 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1412 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1414 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 1416 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1418 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1420 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1422 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1424 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1426 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1428 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter accessible */

/* 1430 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1432 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1434 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Parameter caretOffset */

/* 1436 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1438 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1440 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1442 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1444 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1446 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_relationTargetsOfType */

/* 1448 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1450 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1454 */	NdrFcShort( 0x30 ),	/* 48 */
/* 1456 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1458 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1460 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1462 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 1464 */	0x10,		/* 16 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 1466 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1468 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1470 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1472 */	NdrFcShort( 0x5 ),	/* 5 */
/* 1474 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 1476 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1478 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter type */

/* 1480 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1482 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1484 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Parameter maxTargets */

/* 1486 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1488 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1490 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter targets */

/* 1492 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 1494 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1496 */	NdrFcShort( 0x4c6 ),	/* Type Offset=1222 */

	/* Parameter nTargets */

/* 1498 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1500 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1502 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1504 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1506 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1508 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectionRanges */

/* 1510 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1512 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1516 */	NdrFcShort( 0x31 ),	/* 49 */
/* 1518 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1520 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1522 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1524 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 1526 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1528 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1530 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1532 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1534 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1536 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1538 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter ranges */

/* 1540 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 1542 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1544 */	NdrFcShort( 0x4e4 ),	/* Type Offset=1252 */

	/* Parameter nRanges */

/* 1546 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1548 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1550 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1552 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1554 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1556 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setSelectionRanges */

/* 1558 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1560 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1564 */	NdrFcShort( 0x32 ),	/* 50 */
/* 1566 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1568 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1570 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1572 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 1574 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1576 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1578 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1580 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1582 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1584 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1586 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter nRanges */

/* 1588 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1590 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1592 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter ranges */

/* 1594 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1596 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1598 */	NdrFcShort( 0x51c ),	/* Type Offset=1308 */

	/* Return value */

/* 1600 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1602 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1604 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_locationInParent */

/* 1606 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1608 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1612 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1614 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1616 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1618 */	NdrFcShort( 0x40 ),	/* 64 */
/* 1620 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 1622 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1624 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1626 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1628 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1630 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1632 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1634 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter x */

/* 1636 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1638 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1640 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 1642 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1644 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1646 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1648 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1650 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1652 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_foreground */

/* 1654 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1656 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1660 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1662 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1664 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1666 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1668 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 1670 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1672 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1674 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1676 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1678 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1680 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1682 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter foreground */

/* 1684 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1686 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1688 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1690 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1692 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1694 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_currentValue */

/* 1696 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1698 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1702 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1704 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1706 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1708 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1710 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1712 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1714 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1716 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1718 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1720 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1722 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1724 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter currentValue */

/* 1726 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1728 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1730 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 1732 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1734 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1736 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setCurrentValue */

/* 1738 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1740 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1744 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1746 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1748 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1750 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1752 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1754 */	0xe,		/* 14 */
			0x85,		/* Ext Flags:  new corr desc, srv corr check, has big byval param */
/* 1756 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1758 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1760 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1762 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1764 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1766 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 1768 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1770 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1772 */	NdrFcShort( 0x53a ),	/* Type Offset=1338 */

	/* Return value */

/* 1774 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1776 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1778 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_maximumValue */

/* 1780 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1782 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1786 */	NdrFcShort( 0x5 ),	/* 5 */
/* 1788 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1790 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1792 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1794 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1796 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1798 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1800 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1802 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1804 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1806 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1808 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter maximumValue */

/* 1810 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1812 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1814 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 1816 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1818 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1820 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_minimumValue */

/* 1822 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1824 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1828 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1830 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1832 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1834 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1836 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1838 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1840 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1842 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1844 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1846 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1848 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1850 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter minimumValue */

/* 1852 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1854 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1856 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 1858 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1860 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1862 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure copyText */


	/* Procedure addSelection */

/* 1864 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1866 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1870 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1872 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1874 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1876 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1878 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 1880 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1882 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1884 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1886 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1888 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1890 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1892 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter startOffset */


	/* Parameter startOffset */

/* 1894 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1896 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1898 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */


	/* Parameter endOffset */

/* 1900 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1902 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1904 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 1906 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1908 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1910 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attributes */

/* 1912 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1914 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1918 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1920 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1922 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1924 */	NdrFcShort( 0x40 ),	/* 64 */
/* 1926 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x5,		/* 5 */
/* 1928 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1930 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1932 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1934 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1936 */	NdrFcShort( 0x5 ),	/* 5 */
/* 1938 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 1940 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1942 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter offset */

/* 1944 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1946 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1948 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 1950 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1952 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1954 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 1956 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1958 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1960 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter textAttributes */

/* 1962 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1964 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1966 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 1968 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1970 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1972 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_characterExtents */

/* 1974 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1976 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1980 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1982 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1984 */	NdrFcShort( 0xe ),	/* 14 */
/* 1986 */	NdrFcShort( 0x78 ),	/* 120 */
/* 1988 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x7,		/* 7 */
/* 1990 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1992 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1994 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1996 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1998 */	NdrFcShort( 0x7 ),	/* 7 */
/* 2000 */	0x7,		/* 7 */
			0x80,		/* 128 */
/* 2002 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2004 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 2006 */	0x85,		/* 133 */
			0x86,		/* 134 */

	/* Parameter offset */

/* 2008 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2010 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2012 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter coordType */

/* 2014 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2016 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2018 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter x */

/* 2020 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2022 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2024 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 2026 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2028 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2030 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter width */

/* 2032 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2034 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2036 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter height */

/* 2038 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2040 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2042 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2044 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2046 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2048 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nRows */


	/* Procedure get_nSelections */

/* 2050 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2052 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2056 */	NdrFcShort( 0x7 ),	/* 7 */
/* 2058 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2060 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2062 */	NdrFcShort( 0x24 ),	/* 36 */
/* 2064 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2066 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2068 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2070 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2072 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2074 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2076 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2078 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter rowCount */


	/* Parameter nSelections */

/* 2080 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2082 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2084 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 2086 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2088 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2090 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_offsetAtPoint */

/* 2092 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2094 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2098 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2100 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2102 */	NdrFcShort( 0x16 ),	/* 22 */
/* 2104 */	NdrFcShort( 0x24 ),	/* 36 */
/* 2106 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x5,		/* 5 */
/* 2108 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2110 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2112 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2114 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2116 */	NdrFcShort( 0x5 ),	/* 5 */
/* 2118 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 2120 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2122 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter x */

/* 2124 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2126 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2128 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 2130 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2132 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2134 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter coordType */

/* 2136 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2138 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2140 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter offset */

/* 2142 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2144 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2146 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2148 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2150 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2152 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selection */

/* 2154 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2156 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2160 */	NdrFcShort( 0x9 ),	/* 9 */
/* 2162 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2164 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2166 */	NdrFcShort( 0x40 ),	/* 64 */
/* 2168 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 2170 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2172 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2174 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2176 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2178 */	NdrFcShort( 0x4 ),	/* 4 */
/* 2180 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 2182 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2184 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter selectionIndex */

/* 2186 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2188 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2190 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2192 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2194 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2196 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2198 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2200 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2202 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2204 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2206 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2208 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_text */

/* 2210 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2212 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2216 */	NdrFcShort( 0xa ),	/* 10 */
/* 2218 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2220 */	NdrFcShort( 0x10 ),	/* 16 */
/* 2222 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2224 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 2226 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2228 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2230 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2232 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2234 */	NdrFcShort( 0x4 ),	/* 4 */
/* 2236 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 2238 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2240 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2242 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2244 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2246 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2248 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2250 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2252 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 2254 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2256 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2258 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 2260 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2262 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2264 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_textBeforeOffset */

/* 2266 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2268 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2272 */	NdrFcShort( 0xb ),	/* 11 */
/* 2274 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2276 */	NdrFcShort( 0xe ),	/* 14 */
/* 2278 */	NdrFcShort( 0x40 ),	/* 64 */
/* 2280 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x6,		/* 6 */
/* 2282 */	0x12,		/* 18 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2284 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2286 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2288 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2290 */	NdrFcShort( 0x6 ),	/* 6 */
/* 2292 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 2294 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2296 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 2298 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter offset */

/* 2300 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2302 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2304 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter boundaryType */

/* 2306 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2308 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2310 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2312 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2314 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2316 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2318 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2320 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2322 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 2324 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2326 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2328 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 2330 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2332 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2334 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_textAfterOffset */

/* 2336 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2338 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2342 */	NdrFcShort( 0xc ),	/* 12 */
/* 2344 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2346 */	NdrFcShort( 0xe ),	/* 14 */
/* 2348 */	NdrFcShort( 0x40 ),	/* 64 */
/* 2350 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x6,		/* 6 */
/* 2352 */	0x12,		/* 18 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2354 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2356 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2358 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2360 */	NdrFcShort( 0x6 ),	/* 6 */
/* 2362 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 2364 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2366 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 2368 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter offset */

/* 2370 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2372 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2374 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter boundaryType */

/* 2376 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2378 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2380 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2382 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2384 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2386 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2388 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2390 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2392 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 2394 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2396 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2398 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 2400 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2402 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2404 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_textAtOffset */

/* 2406 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2408 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2412 */	NdrFcShort( 0xd ),	/* 13 */
/* 2414 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2416 */	NdrFcShort( 0xe ),	/* 14 */
/* 2418 */	NdrFcShort( 0x40 ),	/* 64 */
/* 2420 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x6,		/* 6 */
/* 2422 */	0x12,		/* 18 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2424 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2426 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2428 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2430 */	NdrFcShort( 0x6 ),	/* 6 */
/* 2432 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 2434 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2436 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 2438 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter offset */

/* 2440 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2442 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2444 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter boundaryType */

/* 2446 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2448 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2450 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2452 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2454 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2456 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2458 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2460 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2462 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 2464 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2466 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2468 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 2470 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2472 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2474 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure removeSelection */

/* 2476 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2478 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2482 */	NdrFcShort( 0xe ),	/* 14 */
/* 2484 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2486 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2488 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2490 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2492 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2494 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2496 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2498 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2500 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2502 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2504 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter selectionIndex */

/* 2506 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2508 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2510 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2512 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2514 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2516 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setCaretOffset */

/* 2518 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2520 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2524 */	NdrFcShort( 0xf ),	/* 15 */
/* 2526 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2528 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2530 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2532 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2534 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2536 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2538 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2540 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2542 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2544 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2546 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter offset */

/* 2548 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2550 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2552 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2554 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2556 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2558 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setSelection */

/* 2560 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2562 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2566 */	NdrFcShort( 0x10 ),	/* 16 */
/* 2568 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2570 */	NdrFcShort( 0x18 ),	/* 24 */
/* 2572 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2574 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 2576 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2578 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2580 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2582 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2584 */	NdrFcShort( 0x4 ),	/* 4 */
/* 2586 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 2588 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2590 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter selectionIndex */

/* 2592 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2594 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2596 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2598 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2600 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2602 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2604 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2606 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2608 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2610 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2612 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2614 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nCharacters */

/* 2616 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2618 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2622 */	NdrFcShort( 0x11 ),	/* 17 */
/* 2624 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2626 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2628 */	NdrFcShort( 0x24 ),	/* 36 */
/* 2630 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2632 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2634 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2636 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2638 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2640 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2642 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2644 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter nCharacters */

/* 2646 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2648 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2650 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2652 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2654 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2656 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure scrollSubstringTo */

/* 2658 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2660 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2664 */	NdrFcShort( 0x12 ),	/* 18 */
/* 2666 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2668 */	NdrFcShort( 0x16 ),	/* 22 */
/* 2670 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2672 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 2674 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2676 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2678 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2680 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2682 */	NdrFcShort( 0x4 ),	/* 4 */
/* 2684 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 2686 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2688 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter startIndex */

/* 2690 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2692 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2694 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endIndex */

/* 2696 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2698 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2700 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter scrollType */

/* 2702 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2704 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2706 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Return value */

/* 2708 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2710 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2712 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure scrollSubstringToPoint */

/* 2714 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2716 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2720 */	NdrFcShort( 0x13 ),	/* 19 */
/* 2722 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2724 */	NdrFcShort( 0x26 ),	/* 38 */
/* 2726 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2728 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x6,		/* 6 */
/* 2730 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2732 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2734 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2736 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2738 */	NdrFcShort( 0x6 ),	/* 6 */
/* 2740 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 2742 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2744 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 2746 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter startIndex */

/* 2748 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2750 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2752 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endIndex */

/* 2754 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2756 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2758 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter coordinateType */

/* 2760 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2762 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2764 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter x */

/* 2766 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2768 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2770 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 2772 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2774 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2776 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2778 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2780 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2782 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_newText */

/* 2784 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2786 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2790 */	NdrFcShort( 0x14 ),	/* 20 */
/* 2792 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2794 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2796 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2798 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2800 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2802 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2804 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2806 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2808 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2810 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2812 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter newText */

/* 2814 */	NdrFcShort( 0x4113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=16 */
/* 2816 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2818 */	NdrFcShort( 0x548 ),	/* Type Offset=1352 */

	/* Return value */

/* 2820 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2822 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2824 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_oldText */

/* 2826 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2828 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2832 */	NdrFcShort( 0x15 ),	/* 21 */
/* 2834 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2836 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2838 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2840 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2842 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2844 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2846 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2848 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2850 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2852 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2854 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter oldText */

/* 2856 */	NdrFcShort( 0x4113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=16 */
/* 2858 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2860 */	NdrFcShort( 0x548 ),	/* Type Offset=1352 */

	/* Return value */

/* 2862 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2864 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2866 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attributeRange */

/* 2868 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2870 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2874 */	NdrFcShort( 0x16 ),	/* 22 */
/* 2876 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2878 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2880 */	NdrFcShort( 0x40 ),	/* 64 */
/* 2882 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x6,		/* 6 */
/* 2884 */	0x12,		/* 18 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 2886 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2888 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2890 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2892 */	NdrFcShort( 0x6 ),	/* 6 */
/* 2894 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 2896 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2898 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 2900 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter offset */

/* 2902 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2904 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2906 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter filter */

/* 2908 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2910 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2912 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Parameter startOffset */

/* 2914 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2916 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2918 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2920 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2922 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2924 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter attributeValues */

/* 2926 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2928 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2930 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 2932 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2934 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2936 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selections */

/* 2938 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2940 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2944 */	NdrFcShort( 0x3 ),	/* 3 */
/* 2946 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2948 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2950 */	NdrFcShort( 0x24 ),	/* 36 */
/* 2952 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 2954 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2956 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2958 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2960 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2962 */	NdrFcShort( 0x3 ),	/* 3 */
/* 2964 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 2966 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter selections */

/* 2968 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 2970 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2972 */	NdrFcShort( 0x558 ),	/* Type Offset=1368 */

	/* Parameter nSelections */

/* 2974 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2976 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2978 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2980 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2982 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2984 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setSelections */

/* 2986 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2988 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2992 */	NdrFcShort( 0x4 ),	/* 4 */
/* 2994 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2996 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2998 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3000 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 3002 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 3004 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3006 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3008 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3010 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3012 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3014 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter nSelections */

/* 3016 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3018 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3020 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter selections */

/* 3022 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 3024 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3026 */	NdrFcShort( 0x5a2 ),	/* Type Offset=1442 */

	/* Return value */

/* 3028 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3030 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3032 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure deleteText */

/* 3034 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3036 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3040 */	NdrFcShort( 0x4 ),	/* 4 */
/* 3042 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3044 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3046 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3048 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 3050 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3052 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3054 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3056 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3058 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3060 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3062 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter startOffset */

/* 3064 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3066 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3068 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 3070 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3072 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3074 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3076 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3078 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3080 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure insertText */

/* 3082 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3084 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3088 */	NdrFcShort( 0x5 ),	/* 5 */
/* 3090 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3092 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3094 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3096 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 3098 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 3100 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3102 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3104 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3106 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3108 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3110 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter offset */

/* 3112 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3114 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3116 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 3118 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 3120 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3122 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Return value */

/* 3124 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3126 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3128 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure cutText */

/* 3130 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3132 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3136 */	NdrFcShort( 0x6 ),	/* 6 */
/* 3138 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3140 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3142 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3144 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 3146 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3148 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3150 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3152 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3154 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3156 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3158 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter startOffset */

/* 3160 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3162 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3164 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 3166 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3168 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3170 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3172 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3174 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3176 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure pasteText */

/* 3178 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3180 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3184 */	NdrFcShort( 0x7 ),	/* 7 */
/* 3186 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3188 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3190 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3192 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3194 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3196 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3198 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3200 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3202 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3204 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3206 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter offset */

/* 3208 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3210 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3212 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3214 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3216 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3218 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure replaceText */

/* 3220 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3222 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3226 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3228 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 3230 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3232 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3234 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x4,		/* 4 */
/* 3236 */	0x10,		/* 16 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 3238 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3240 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3242 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3244 */	NdrFcShort( 0x4 ),	/* 4 */
/* 3246 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 3248 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 3250 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 3252 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3254 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3256 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 3258 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3260 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3262 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 3264 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 3266 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3268 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Return value */

/* 3270 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3272 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3274 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setAttributes */

/* 3276 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3278 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3282 */	NdrFcShort( 0x9 ),	/* 9 */
/* 3284 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 3286 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3288 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3290 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x4,		/* 4 */
/* 3292 */	0x10,		/* 16 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 3294 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3296 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3298 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3300 */	NdrFcShort( 0x4 ),	/* 4 */
/* 3302 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 3304 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 3306 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 3308 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3310 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3312 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 3314 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3316 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3318 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter attributes */

/* 3320 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 3322 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3324 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Return value */

/* 3326 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3328 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3330 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_anchor */

/* 3332 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3334 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3338 */	NdrFcShort( 0x9 ),	/* 9 */
/* 3340 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3342 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3344 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3346 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3348 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 3350 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3352 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3354 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3356 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3358 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3360 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter index */

/* 3362 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3364 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3366 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter anchor */

/* 3368 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 3370 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3372 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 3374 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3376 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3378 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_anchorTarget */

/* 3380 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3382 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3386 */	NdrFcShort( 0xa ),	/* 10 */
/* 3388 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3390 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3392 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3394 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3396 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 3398 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3400 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3402 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3404 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3406 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3408 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter index */

/* 3410 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3412 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3414 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter anchorTarget */

/* 3416 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 3418 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3420 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 3422 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3424 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3426 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nRows */


	/* Procedure get_startIndex */

/* 3428 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3430 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3434 */	NdrFcShort( 0xb ),	/* 11 */
/* 3436 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3438 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3440 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3442 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3444 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3446 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3448 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3450 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3452 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3454 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3456 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter rowCount */


	/* Parameter index */

/* 3458 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3460 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3462 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 3464 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3466 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3468 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nSelectedChildren */


	/* Procedure get_endIndex */

/* 3470 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3472 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3476 */	NdrFcShort( 0xc ),	/* 12 */
/* 3478 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3480 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3482 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3484 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3486 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3488 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3490 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3492 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3494 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3496 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3498 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter cellCount */


	/* Parameter index */

/* 3500 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3502 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3504 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 3506 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3508 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3510 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_valid */

/* 3512 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3514 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3518 */	NdrFcShort( 0xd ),	/* 13 */
/* 3520 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3522 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3524 */	NdrFcShort( 0x21 ),	/* 33 */
/* 3526 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3528 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3530 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3532 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3534 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3536 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3538 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3540 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter valid */

/* 3542 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3544 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3546 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 3548 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3550 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3552 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nHyperlinks */

/* 3554 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3556 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3560 */	NdrFcShort( 0x16 ),	/* 22 */
/* 3562 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3564 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3566 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3568 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3570 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3572 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3574 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3576 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3578 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3580 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3582 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter hyperlinkCount */

/* 3584 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3586 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3588 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3590 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3592 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3594 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_hyperlink */

/* 3596 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3598 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3602 */	NdrFcShort( 0x17 ),	/* 23 */
/* 3604 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3606 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3608 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3610 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3612 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3614 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3616 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3618 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3620 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3622 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3624 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter index */

/* 3626 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3628 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3630 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter hyperlink */

/* 3632 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3634 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3636 */	NdrFcShort( 0x5c0 ),	/* Type Offset=1472 */

	/* Return value */

/* 3638 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3640 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3642 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_hyperlinkIndex */

/* 3644 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3646 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3650 */	NdrFcShort( 0x18 ),	/* 24 */
/* 3652 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3654 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3656 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3658 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 3660 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3662 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3664 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3666 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3668 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3670 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3672 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter charIndex */

/* 3674 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3676 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3678 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter hyperlinkIndex */

/* 3680 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3682 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3684 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3686 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3688 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3690 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_hyperlinks */

/* 3692 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3694 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3698 */	NdrFcShort( 0x19 ),	/* 25 */
/* 3700 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3702 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3704 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3706 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3708 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 3710 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3712 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3714 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3716 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3718 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3720 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter hyperlinks */

/* 3722 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 3724 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3726 */	NdrFcShort( 0x5d6 ),	/* Type Offset=1494 */

	/* Parameter nHyperlinks */

/* 3728 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3730 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3732 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3734 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3736 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3738 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_cellAt */


	/* Procedure get_accessibleAt */

/* 3740 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3742 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3746 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3748 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 3750 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3752 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3754 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 3756 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3758 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3760 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3762 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3764 */	NdrFcShort( 0x4 ),	/* 4 */
/* 3766 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 3768 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 3770 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter row */


	/* Parameter row */

/* 3772 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3774 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3776 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */


	/* Parameter column */

/* 3778 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3780 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3782 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter cell */


	/* Parameter accessible */

/* 3784 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3786 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3788 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */


	/* Return value */

/* 3790 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3792 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3794 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_caption */


	/* Procedure get_caption */

/* 3796 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3798 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3802 */	NdrFcShort( 0x4 ),	/* 4 */
/* 3804 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3806 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3808 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3810 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3812 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3814 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3816 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3818 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3820 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3822 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3824 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter accessible */


	/* Parameter accessible */

/* 3826 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3828 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3830 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */


	/* Return value */

/* 3832 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3834 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3836 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_childIndex */

/* 3838 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3840 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3844 */	NdrFcShort( 0x5 ),	/* 5 */
/* 3846 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 3848 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3850 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3852 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 3854 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3856 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3858 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3860 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3862 */	NdrFcShort( 0x4 ),	/* 4 */
/* 3864 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 3866 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 3868 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter rowIndex */

/* 3870 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3872 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3874 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter columnIndex */

/* 3876 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3878 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3880 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter cellIndex */

/* 3882 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3884 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3886 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3888 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3890 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3892 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnDescription */

/* 3894 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3896 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3900 */	NdrFcShort( 0x6 ),	/* 6 */
/* 3902 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3904 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3906 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3908 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3910 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 3912 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3914 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3916 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3918 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3920 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3922 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter column */

/* 3924 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3926 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3928 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter description */

/* 3930 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 3932 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3934 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 3936 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3938 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3940 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnExtentAt */

/* 3942 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3944 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3948 */	NdrFcShort( 0x7 ),	/* 7 */
/* 3950 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 3952 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3954 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3956 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 3958 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3960 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3962 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3964 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3966 */	NdrFcShort( 0x4 ),	/* 4 */
/* 3968 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 3970 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 3972 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter row */

/* 3974 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3976 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3978 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */

/* 3980 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3982 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3984 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter nColumnsSpanned */

/* 3986 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3988 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3990 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3992 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3994 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3996 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnHeader */

/* 3998 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4000 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4004 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4006 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4008 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4010 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4012 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 4014 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4016 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4018 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4020 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4022 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4024 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4026 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter accessibleTable */

/* 4028 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4030 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4032 */	NdrFcShort( 0x5f4 ),	/* Type Offset=1524 */

	/* Parameter startingRowIndex */

/* 4034 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4036 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4038 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4040 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4042 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4044 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnIndex */

/* 4046 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4048 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4052 */	NdrFcShort( 0x9 ),	/* 9 */
/* 4054 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4056 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4058 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4060 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 4062 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4064 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4066 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4068 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4070 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4072 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4074 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter cellIndex */

/* 4076 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4078 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4080 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter columnIndex */

/* 4082 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4084 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4086 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4088 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4090 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4092 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nSelectedRows */


	/* Procedure get_nColumns */

/* 4094 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4096 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4100 */	NdrFcShort( 0xa ),	/* 10 */
/* 4102 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4104 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4106 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4108 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4110 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4112 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4114 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4116 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4118 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4120 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4122 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter rowCount */


	/* Parameter columnCount */

/* 4124 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4126 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4128 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 4130 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4132 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4134 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nSelectedColumns */

/* 4136 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4138 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4142 */	NdrFcShort( 0xd ),	/* 13 */
/* 4144 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4146 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4148 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4150 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4152 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4154 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4156 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4158 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4160 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4162 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4164 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter columnCount */

/* 4166 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4168 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4170 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4172 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4174 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4176 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nSelectedRows */

/* 4178 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4180 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4184 */	NdrFcShort( 0xe ),	/* 14 */
/* 4186 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4188 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4190 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4192 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4194 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4196 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4198 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4200 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4202 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4204 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4206 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter rowCount */

/* 4208 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4210 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4212 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4214 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4216 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4218 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowDescription */

/* 4220 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4222 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4226 */	NdrFcShort( 0xf ),	/* 15 */
/* 4228 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4230 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4232 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4234 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 4236 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4238 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4240 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4242 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4244 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4246 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4248 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter row */

/* 4250 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4252 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4254 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter description */

/* 4256 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 4258 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4260 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 4262 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4264 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4266 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowExtentAt */

/* 4268 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4270 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4274 */	NdrFcShort( 0x10 ),	/* 16 */
/* 4276 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 4278 */	NdrFcShort( 0x10 ),	/* 16 */
/* 4280 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4282 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 4284 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4286 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4288 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4290 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4292 */	NdrFcShort( 0x4 ),	/* 4 */
/* 4294 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 4296 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4298 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter row */

/* 4300 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4302 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4304 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */

/* 4306 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4308 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4310 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter nRowsSpanned */

/* 4312 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4314 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4316 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4318 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4320 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4322 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowHeader */

/* 4324 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4326 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4330 */	NdrFcShort( 0x11 ),	/* 17 */
/* 4332 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4334 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4336 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4338 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 4340 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4342 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4344 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4346 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4348 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4350 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4352 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter accessibleTable */

/* 4354 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4356 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4358 */	NdrFcShort( 0x5f4 ),	/* Type Offset=1524 */

	/* Parameter startingColumnIndex */

/* 4360 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4362 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4364 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4366 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4368 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4370 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowIndex */

/* 4372 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4374 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4378 */	NdrFcShort( 0x12 ),	/* 18 */
/* 4380 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4382 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4384 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4386 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 4388 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4390 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4392 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4394 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4396 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4398 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4400 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter cellIndex */

/* 4402 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4404 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4406 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter rowIndex */

/* 4408 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4410 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4412 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4414 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4416 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4418 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedChildren */

/* 4420 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4422 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4426 */	NdrFcShort( 0x13 ),	/* 19 */
/* 4428 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 4430 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4432 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4434 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 4436 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4438 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4440 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4442 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4444 */	NdrFcShort( 0x4 ),	/* 4 */
/* 4446 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 4448 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4450 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter maxChildren */

/* 4452 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4454 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4456 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter children */

/* 4458 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 4460 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4462 */	NdrFcShort( 0x60a ),	/* Type Offset=1546 */

	/* Parameter nChildren */

/* 4464 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4466 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4468 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4470 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4472 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4474 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedColumns */

/* 4476 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4478 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4482 */	NdrFcShort( 0x14 ),	/* 20 */
/* 4484 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 4486 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4488 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4490 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 4492 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4494 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4496 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4498 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4500 */	NdrFcShort( 0x4 ),	/* 4 */
/* 4502 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 4504 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4506 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter maxColumns */

/* 4508 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4510 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4512 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter columns */

/* 4514 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 4516 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4518 */	NdrFcShort( 0x60a ),	/* Type Offset=1546 */

	/* Parameter nColumns */

/* 4520 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4522 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4524 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4526 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4528 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4530 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedRows */

/* 4532 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4534 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4538 */	NdrFcShort( 0x15 ),	/* 21 */
/* 4540 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 4542 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4544 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4546 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 4548 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4550 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4552 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4554 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4556 */	NdrFcShort( 0x4 ),	/* 4 */
/* 4558 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 4560 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4562 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter maxRows */

/* 4564 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4566 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4568 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter rows */

/* 4570 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 4572 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4574 */	NdrFcShort( 0x60a ),	/* Type Offset=1546 */

	/* Parameter nRows */

/* 4576 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4578 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4580 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4582 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4584 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4586 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_summary */

/* 4588 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4590 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4594 */	NdrFcShort( 0x16 ),	/* 22 */
/* 4596 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4598 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4600 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4602 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 4604 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4606 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4608 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4610 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4612 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4614 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4616 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter accessible */

/* 4618 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4620 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4622 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 4624 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4626 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4628 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isColumnSelected */

/* 4630 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4632 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4636 */	NdrFcShort( 0x17 ),	/* 23 */
/* 4638 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4640 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4642 */	NdrFcShort( 0x21 ),	/* 33 */
/* 4644 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 4646 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4648 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4650 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4652 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4654 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4656 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4658 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter column */

/* 4660 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4662 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4664 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 4666 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4668 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4670 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 4672 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4674 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4676 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isRowSelected */

/* 4678 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4680 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4684 */	NdrFcShort( 0x18 ),	/* 24 */
/* 4686 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4688 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4690 */	NdrFcShort( 0x21 ),	/* 33 */
/* 4692 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 4694 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4696 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4698 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4700 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4702 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4704 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4706 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter row */

/* 4708 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4710 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4712 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 4714 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4716 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4718 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 4720 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4722 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4724 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isSelected */

/* 4726 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4728 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4732 */	NdrFcShort( 0x19 ),	/* 25 */
/* 4734 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 4736 */	NdrFcShort( 0x10 ),	/* 16 */
/* 4738 */	NdrFcShort( 0x21 ),	/* 33 */
/* 4740 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 4742 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4744 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4746 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4748 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4750 */	NdrFcShort( 0x4 ),	/* 4 */
/* 4752 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 4754 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4756 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter row */

/* 4758 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4760 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4762 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */

/* 4764 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4766 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4768 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 4770 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4772 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4774 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 4776 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4778 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4780 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure selectRow */

/* 4782 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4784 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4788 */	NdrFcShort( 0x1a ),	/* 26 */
/* 4790 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4792 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4794 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4796 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4798 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4800 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4802 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4804 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4806 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4808 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4810 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter row */

/* 4812 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4814 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4816 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4818 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4820 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4822 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure selectColumn */

/* 4824 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4826 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4830 */	NdrFcShort( 0x1b ),	/* 27 */
/* 4832 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4834 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4836 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4838 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4840 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4842 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4844 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4846 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4848 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4850 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4852 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter column */

/* 4854 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4856 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4858 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4860 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4862 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4864 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure unselectRow */

/* 4866 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4868 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4872 */	NdrFcShort( 0x1c ),	/* 28 */
/* 4874 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4876 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4878 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4880 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4882 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4884 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4886 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4888 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4890 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4892 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4894 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter row */

/* 4896 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4898 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4900 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4902 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4904 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4906 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure unselectColumn */

/* 4908 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4910 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4914 */	NdrFcShort( 0x1d ),	/* 29 */
/* 4916 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4918 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4920 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4922 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4924 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4926 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4928 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4930 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4932 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4934 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4936 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter column */

/* 4938 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4940 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4942 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4944 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4946 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4948 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowColumnExtentsAtIndex */

/* 4950 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4952 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4956 */	NdrFcShort( 0x1e ),	/* 30 */
/* 4958 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 4960 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4962 */	NdrFcShort( 0x91 ),	/* 145 */
/* 4964 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x7,		/* 7 */
/* 4966 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4968 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4970 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4972 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4974 */	NdrFcShort( 0x7 ),	/* 7 */
/* 4976 */	0x7,		/* 7 */
			0x80,		/* 128 */
/* 4978 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4980 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 4982 */	0x85,		/* 133 */
			0x86,		/* 134 */

	/* Parameter index */

/* 4984 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4986 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4988 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter row */

/* 4990 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4992 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4994 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */

/* 4996 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4998 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5000 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter rowExtents */

/* 5002 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5004 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5006 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter columnExtents */

/* 5008 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5010 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 5012 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 5014 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5016 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 5018 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 5020 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5022 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 5024 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_modelChange */

/* 5026 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5028 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5032 */	NdrFcShort( 0x1f ),	/* 31 */
/* 5034 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5036 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5038 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5040 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 5042 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5044 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5046 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5048 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5050 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5052 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5054 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter modelChange */

/* 5056 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 5058 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5060 */	NdrFcShort( 0x628 ),	/* Type Offset=1576 */

	/* Return value */

/* 5062 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5064 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5066 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowExtent */


	/* Procedure get_nColumns */

/* 5068 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5070 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5074 */	NdrFcShort( 0x6 ),	/* 6 */
/* 5076 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5078 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5080 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5082 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5084 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5086 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5088 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5090 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5092 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5094 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5096 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter nRowsSpanned */


	/* Parameter columnCount */

/* 5098 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5100 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5102 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 5104 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5106 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5108 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowIndex */


	/* Procedure get_nSelectedCells */

/* 5110 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5112 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5116 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5118 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5120 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5122 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5124 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5126 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5128 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5130 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5132 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5134 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5136 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5138 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter rowIndex */


	/* Parameter cellCount */

/* 5140 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5142 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5144 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 5146 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5148 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5150 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nSelectedColumns */

/* 5152 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5154 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5158 */	NdrFcShort( 0x9 ),	/* 9 */
/* 5160 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5162 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5164 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5166 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5168 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5170 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5172 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5174 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5176 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5178 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5180 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter columnCount */

/* 5182 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5184 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5186 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5188 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5190 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5192 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowDescription */

/* 5194 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5196 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5200 */	NdrFcShort( 0xb ),	/* 11 */
/* 5202 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5204 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5206 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5208 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 5210 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5212 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5214 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5216 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5218 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5220 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5222 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter row */

/* 5224 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5226 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5228 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter description */

/* 5230 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 5232 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5234 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 5236 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5238 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5240 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedCells */

/* 5242 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5244 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5248 */	NdrFcShort( 0xc ),	/* 12 */
/* 5250 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5252 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5254 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5256 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 5258 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5260 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5262 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5264 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5266 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5268 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5270 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter cells */

/* 5272 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 5274 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5276 */	NdrFcShort( 0x636 ),	/* Type Offset=1590 */

	/* Parameter nSelectedCells */

/* 5278 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5280 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5282 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5284 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5286 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5288 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedColumns */

/* 5290 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5292 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5296 */	NdrFcShort( 0xd ),	/* 13 */
/* 5298 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5300 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5302 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5304 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 5306 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5308 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5310 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5312 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5314 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5316 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5318 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter selectedColumns */

/* 5320 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 5322 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5324 */	NdrFcShort( 0x654 ),	/* Type Offset=1620 */

	/* Parameter nColumns */

/* 5326 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5328 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5330 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5332 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5334 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5336 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedRows */

/* 5338 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5340 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5344 */	NdrFcShort( 0xe ),	/* 14 */
/* 5346 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5348 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5350 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5352 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 5354 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5356 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5358 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5360 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5362 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5364 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5366 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter selectedRows */

/* 5368 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 5370 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5372 */	NdrFcShort( 0x654 ),	/* Type Offset=1620 */

	/* Parameter nRows */

/* 5374 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5376 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5378 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5380 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5382 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5384 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_summary */

/* 5386 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5388 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5392 */	NdrFcShort( 0xf ),	/* 15 */
/* 5394 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5396 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5398 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5400 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 5402 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5404 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5406 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5408 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5410 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5412 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5414 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter accessible */

/* 5416 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 5418 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5420 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 5422 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5424 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5426 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isColumnSelected */

/* 5428 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5430 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5434 */	NdrFcShort( 0x10 ),	/* 16 */
/* 5436 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5438 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5440 */	NdrFcShort( 0x21 ),	/* 33 */
/* 5442 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 5444 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5446 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5448 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5450 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5452 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5454 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5456 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter column */

/* 5458 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5460 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5462 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 5464 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5466 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5468 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 5470 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5472 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5474 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isRowSelected */

/* 5476 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5478 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5482 */	NdrFcShort( 0x11 ),	/* 17 */
/* 5484 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5486 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5488 */	NdrFcShort( 0x21 ),	/* 33 */
/* 5490 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 5492 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5494 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5496 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5498 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5500 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5502 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5504 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter row */

/* 5506 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5508 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5510 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 5512 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5514 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5516 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 5518 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5520 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5522 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure selectRow */

/* 5524 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5526 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5530 */	NdrFcShort( 0x12 ),	/* 18 */
/* 5532 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5534 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5536 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5538 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5540 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5542 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5544 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5546 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5548 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5550 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5552 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter row */

/* 5554 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5556 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5558 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5560 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5562 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5564 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure selectColumn */

/* 5566 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5568 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5572 */	NdrFcShort( 0x13 ),	/* 19 */
/* 5574 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5576 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5578 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5580 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5582 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5584 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5586 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5588 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5590 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5592 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5594 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter column */

/* 5596 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5598 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5600 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5602 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5604 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5606 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure unselectRow */

/* 5608 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5610 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5614 */	NdrFcShort( 0x14 ),	/* 20 */
/* 5616 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5618 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5620 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5622 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5624 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5626 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5628 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5630 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5632 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5634 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5636 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter row */

/* 5638 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5640 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5642 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5644 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5646 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5648 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure unselectColumn */

/* 5650 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5652 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5656 */	NdrFcShort( 0x15 ),	/* 21 */
/* 5658 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5660 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5662 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5664 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5666 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5668 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5670 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5672 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5674 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5676 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5678 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter column */

/* 5680 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5682 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5684 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5686 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5688 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5690 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_modelChange */

/* 5692 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5694 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5698 */	NdrFcShort( 0x16 ),	/* 22 */
/* 5700 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5702 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5704 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5706 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 5708 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5710 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5712 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5714 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5716 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5718 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5720 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter modelChange */

/* 5722 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 5724 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5726 */	NdrFcShort( 0x628 ),	/* Type Offset=1576 */

	/* Return value */

/* 5728 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5730 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5732 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnHeaderCells */

/* 5734 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5736 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5740 */	NdrFcShort( 0x4 ),	/* 4 */
/* 5742 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5744 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5746 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5748 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 5750 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5752 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5754 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5756 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5758 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5760 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5762 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter cellAccessibles */

/* 5764 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 5766 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5768 */	NdrFcShort( 0x636 ),	/* Type Offset=1590 */

	/* Parameter nColumnHeaderCells */

/* 5770 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5772 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5774 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5776 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5778 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5780 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowHeaderCells */

/* 5782 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5784 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5788 */	NdrFcShort( 0x7 ),	/* 7 */
/* 5790 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5792 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5794 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5796 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 5798 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5800 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5802 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5804 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5806 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5808 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5810 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter cellAccessibles */

/* 5812 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 5814 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5816 */	NdrFcShort( 0x636 ),	/* Type Offset=1590 */

	/* Parameter nRowHeaderCells */

/* 5818 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5820 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5822 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5824 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5826 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5828 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isSelected */

/* 5830 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5832 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5836 */	NdrFcShort( 0x9 ),	/* 9 */
/* 5838 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5840 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5842 */	NdrFcShort( 0x21 ),	/* 33 */
/* 5844 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5846 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5848 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5850 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5852 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5854 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5856 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5858 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 5860 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5862 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5864 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 5866 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5868 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5870 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowColumnExtents */

/* 5872 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5874 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5878 */	NdrFcShort( 0xa ),	/* 10 */
/* 5880 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 5882 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5884 */	NdrFcShort( 0x91 ),	/* 145 */
/* 5886 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x6,		/* 6 */
/* 5888 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5890 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5892 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5894 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5896 */	NdrFcShort( 0x6 ),	/* 6 */
/* 5898 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 5900 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 5902 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 5904 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter row */

/* 5906 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5908 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5910 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */

/* 5912 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5914 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5916 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter rowExtents */

/* 5918 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5920 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5922 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter columnExtents */

/* 5924 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5926 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5928 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 5930 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5932 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 5934 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 5936 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5938 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 5940 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_table */

/* 5942 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5944 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5948 */	NdrFcShort( 0xb ),	/* 11 */
/* 5950 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5952 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5954 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5956 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 5958 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5960 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5962 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5964 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5966 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5968 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5970 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter table */

/* 5972 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 5974 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5976 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 5978 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5980 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5982 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_imagePosition */

/* 5984 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5986 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5990 */	NdrFcShort( 0x4 ),	/* 4 */
/* 5992 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 5994 */	NdrFcShort( 0x6 ),	/* 6 */
/* 5996 */	NdrFcShort( 0x40 ),	/* 64 */
/* 5998 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 6000 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 6002 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6004 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6006 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6008 */	NdrFcShort( 0x4 ),	/* 4 */
/* 6010 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 6012 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 6014 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter coordinateType */

/* 6016 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 6018 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 6020 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter x */

/* 6022 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 6024 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 6026 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 6028 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 6030 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 6032 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 6034 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 6036 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 6038 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_imageSize */

/* 6040 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 6042 */	NdrFcLong( 0x0 ),	/* 0 */
/* 6046 */	NdrFcShort( 0x5 ),	/* 5 */
/* 6048 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 6050 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6052 */	NdrFcShort( 0x40 ),	/* 64 */
/* 6054 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 6056 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 6058 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6060 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6062 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6064 */	NdrFcShort( 0x3 ),	/* 3 */
/* 6066 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 6068 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter height */

/* 6070 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 6072 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 6074 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter width */

/* 6076 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 6078 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 6080 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 6082 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 6084 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 6086 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_toolkitName */

/* 6088 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 6090 */	NdrFcLong( 0x0 ),	/* 0 */
/* 6094 */	NdrFcShort( 0x5 ),	/* 5 */
/* 6096 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 6098 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6100 */	NdrFcShort( 0x8 ),	/* 8 */
/* 6102 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 6104 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 6106 */	NdrFcShort( 0x1 ),	/* 1 */
/* 6108 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6110 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6112 */	NdrFcShort( 0x2 ),	/* 2 */
/* 6114 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 6116 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter name */

/* 6118 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 6120 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 6122 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 6124 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 6126 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 6128 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_toolkitVersion */

/* 6130 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 6132 */	NdrFcLong( 0x0 ),	/* 0 */
/* 6136 */	NdrFcShort( 0x6 ),	/* 6 */
/* 6138 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 6140 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6142 */	NdrFcShort( 0x8 ),	/* 8 */
/* 6144 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 6146 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 6148 */	NdrFcShort( 0x1 ),	/* 1 */
/* 6150 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6152 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6154 */	NdrFcShort( 0x2 ),	/* 2 */
/* 6156 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 6158 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter version */

/* 6160 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 6162 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 6164 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 6166 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 6168 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 6170 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_anchorTarget */

/* 6172 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 6174 */	NdrFcLong( 0x0 ),	/* 0 */
/* 6178 */	NdrFcShort( 0x3 ),	/* 3 */
/* 6180 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 6182 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6184 */	NdrFcShort( 0x8 ),	/* 8 */
/* 6186 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 6188 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 6190 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6192 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6194 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6196 */	NdrFcShort( 0x2 ),	/* 2 */
/* 6198 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 6200 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter accessible */

/* 6202 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 6204 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 6206 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 6208 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 6210 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 6212 */	0x8,		/* FC_LONG */
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
/* 78 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 80 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 82 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 84 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
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
/* 108 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 110 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 112 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 114 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
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
/* 156 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 158 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 160 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 162 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
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
/* 186 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 188 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 190 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 192 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
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
/* 1236 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
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
/* 1288 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
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
/* 1314 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
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
/* 1422 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
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
/* 1448 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
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
/* 1508 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
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
/* 1560 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1562 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 1564 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 1566 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
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
/* 1604 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
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
/* 1634 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
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
    42,
    84,
    126,
    174
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
    230,
    272,
    314,
    362,
    424,
    472
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
    520,
    562,
    610,
    666,
    708,
    750,
    806,
    862,
    904,
    946,
    988,
    1030,
    1086,
    1142,
    1184,
    1226,
    1268,
    1310
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
    520,
    562,
    610,
    666,
    708,
    750,
    806,
    862,
    904,
    946,
    988,
    1030,
    1086,
    1142,
    1184,
    1226,
    1268,
    1310,
    1352,
    1400,
    1448
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
    520,
    562,
    610,
    666,
    708,
    750,
    806,
    862,
    904,
    946,
    988,
    1030,
    1086,
    1142,
    1184,
    1226,
    1268,
    1310,
    1352,
    1400,
    1448,
    1510
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
    520,
    562,
    610,
    666,
    708,
    750,
    806,
    862,
    904,
    946,
    988,
    1030,
    1086,
    1142,
    1184,
    1226,
    1268,
    1310,
    1352,
    1400,
    1448,
    1510,
    1558
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
    1606,
    1654,
    84
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
    1696,
    1738,
    1780,
    1822
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
    1864,
    1912,
    84,
    1974,
    2050,
    2092,
    2154,
    2210,
    2266,
    2336,
    2406,
    2476,
    2518,
    2560,
    2616,
    2658,
    2714,
    2784,
    2826
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
    1864,
    1912,
    84,
    1974,
    2050,
    2092,
    2154,
    2210,
    2266,
    2336,
    2406,
    2476,
    2518,
    2560,
    2616,
    2658,
    2714,
    2784,
    2826,
    2868
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
    2938,
    2986
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
    1864,
    3034,
    3082,
    3130,
    3178,
    3220,
    3276
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
    230,
    272,
    314,
    362,
    424,
    472,
    3332,
    3380,
    3428,
    3470,
    3512
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
    1864,
    1912,
    84,
    1974,
    2050,
    2092,
    2154,
    2210,
    2266,
    2336,
    2406,
    2476,
    2518,
    2560,
    2616,
    2658,
    2714,
    2784,
    2826,
    3554,
    3596,
    3644
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
    1864,
    1912,
    84,
    1974,
    2050,
    2092,
    2154,
    2210,
    2266,
    2336,
    2406,
    2476,
    2518,
    2560,
    2616,
    2658,
    2714,
    2784,
    2826,
    3554,
    3596,
    3644,
    3692
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
    3740,
    3796,
    3838,
    3894,
    3942,
    3998,
    4046,
    4094,
    3428,
    3470,
    4136,
    4178,
    4220,
    4268,
    4324,
    4372,
    4420,
    4476,
    4532,
    4588,
    4630,
    4678,
    4726,
    4782,
    4824,
    4866,
    4908,
    4950,
    5026
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
    3740,
    3796,
    314,
    5068,
    2050,
    5110,
    5152,
    4094,
    5194,
    5242,
    5290,
    5338,
    5386,
    5428,
    5476,
    5524,
    5566,
    5608,
    5650,
    5692
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
    230,
    5734,
    84,
    5068,
    5782,
    5110,
    5830,
    5872,
    5942
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
    5984,
    6040
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
    42,
    6088,
    6130
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
    6172
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


#endif /* defined(_M_ARM64) */

