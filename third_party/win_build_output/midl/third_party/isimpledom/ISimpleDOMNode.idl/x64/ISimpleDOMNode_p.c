

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../third_party/isimpledom/ISimpleDOMNode.idl:
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


#include "ISimpleDOMNode.h"

#define TYPE_FORMAT_STRING_SIZE   209                               
#define PROC_FORMAT_STRING_SIZE   653                               
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   1            

typedef struct _ISimpleDOMNode_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } ISimpleDOMNode_MIDL_TYPE_FORMAT_STRING;

typedef struct _ISimpleDOMNode_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } ISimpleDOMNode_MIDL_PROC_FORMAT_STRING;

typedef struct _ISimpleDOMNode_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } ISimpleDOMNode_MIDL_EXPR_FORMAT_STRING;


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


extern const ISimpleDOMNode_MIDL_TYPE_FORMAT_STRING ISimpleDOMNode__MIDL_TypeFormatString;
extern const ISimpleDOMNode_MIDL_PROC_FORMAT_STRING ISimpleDOMNode__MIDL_ProcFormatString;
extern const ISimpleDOMNode_MIDL_EXPR_FORMAT_STRING ISimpleDOMNode__MIDL_ExprFormatString;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO ISimpleDOMNode_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ISimpleDOMNode_ProxyInfo;


extern const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ];

#if !defined(__RPC_WIN64__)
#error  Invalid build platform for this stub.
#endif

static const ISimpleDOMNode_MIDL_PROC_FORMAT_STRING ISimpleDOMNode__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure get_nodeInfo */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x3 ),	/* 3 */
/*  8 */	NdrFcShort( 0x40 ),	/* X64 Stack size/offset = 64 */
/* 10 */	NdrFcShort( 0x0 ),	/* 0 */
/* 12 */	NdrFcShort( 0x74 ),	/* 116 */
/* 14 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x7,		/* 7 */
/* 16 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 18 */	NdrFcShort( 0x1 ),	/* 1 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter nodeName */

/* 26 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 28 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 30 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Parameter nameSpaceID */

/* 32 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 34 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 36 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Parameter nodeValue */

/* 38 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 40 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 42 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Parameter numChildren */

/* 44 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 46 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 48 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter uniqueID */

/* 50 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 52 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 54 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter nodeType */

/* 56 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 58 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 60 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 62 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 64 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 66 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attributes */

/* 68 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 70 */	NdrFcLong( 0x0 ),	/* 0 */
/* 74 */	NdrFcShort( 0x4 ),	/* 4 */
/* 76 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 78 */	NdrFcShort( 0x6 ),	/* 6 */
/* 80 */	NdrFcShort( 0x22 ),	/* 34 */
/* 82 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x6,		/* 6 */
/* 84 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 86 */	NdrFcShort( 0x1 ),	/* 1 */
/* 88 */	NdrFcShort( 0x0 ),	/* 0 */
/* 90 */	NdrFcShort( 0x0 ),	/* 0 */
/* 92 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter maxAttribs */

/* 94 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 96 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 98 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Parameter attribNames */

/* 100 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 102 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 104 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Parameter nameSpaceID */

/* 106 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 108 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 110 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter attribValues */

/* 112 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 114 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 116 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Parameter numAttribs */

/* 118 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 120 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 122 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 124 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 126 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 128 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attributesForNames */

/* 130 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 132 */	NdrFcLong( 0x0 ),	/* 0 */
/* 136 */	NdrFcShort( 0x5 ),	/* 5 */
/* 138 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 140 */	NdrFcShort( 0x6 ),	/* 6 */
/* 142 */	NdrFcShort( 0x8 ),	/* 8 */
/* 144 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 146 */	0xa,		/* 10 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 148 */	NdrFcShort( 0x1 ),	/* 1 */
/* 150 */	NdrFcShort( 0x1 ),	/* 1 */
/* 152 */	NdrFcShort( 0x0 ),	/* 0 */
/* 154 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter numAttribs */

/* 156 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 158 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 160 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Parameter attribNames */

/* 162 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 164 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 166 */	NdrFcShort( 0x74 ),	/* Type Offset=116 */

	/* Parameter nameSpaceID */

/* 168 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 170 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 172 */	NdrFcShort( 0x8e ),	/* Type Offset=142 */

	/* Parameter attribValues */

/* 174 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 176 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 178 */	NdrFcShort( 0xa4 ),	/* Type Offset=164 */

	/* Return value */

/* 180 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 182 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 184 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_computedStyle */

/* 186 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 188 */	NdrFcLong( 0x0 ),	/* 0 */
/* 192 */	NdrFcShort( 0x6 ),	/* 6 */
/* 194 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 196 */	NdrFcShort( 0xb ),	/* 11 */
/* 198 */	NdrFcShort( 0x22 ),	/* 34 */
/* 200 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x6,		/* 6 */
/* 202 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 204 */	NdrFcShort( 0x1 ),	/* 1 */
/* 206 */	NdrFcShort( 0x0 ),	/* 0 */
/* 208 */	NdrFcShort( 0x0 ),	/* 0 */
/* 210 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter maxStyleProperties */

/* 212 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 214 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 216 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Parameter useAlternateView */

/* 218 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 220 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 222 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Parameter styleProperties */

/* 224 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 226 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 228 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Parameter styleValues */

/* 230 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 232 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 234 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Parameter numStyleProperties */

/* 236 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 238 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 240 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 242 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 244 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 246 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_computedStyleForProperties */

/* 248 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 250 */	NdrFcLong( 0x0 ),	/* 0 */
/* 254 */	NdrFcShort( 0x7 ),	/* 7 */
/* 256 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 258 */	NdrFcShort( 0xb ),	/* 11 */
/* 260 */	NdrFcShort( 0x8 ),	/* 8 */
/* 262 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 264 */	0xa,		/* 10 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 266 */	NdrFcShort( 0x1 ),	/* 1 */
/* 268 */	NdrFcShort( 0x1 ),	/* 1 */
/* 270 */	NdrFcShort( 0x0 ),	/* 0 */
/* 272 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter numStyleProperties */

/* 274 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 276 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 278 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Parameter useAlternateView */

/* 280 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 282 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 284 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Parameter styleProperties */

/* 286 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 288 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 290 */	NdrFcShort( 0x74 ),	/* Type Offset=116 */

	/* Parameter styleValues */

/* 292 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 294 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 296 */	NdrFcShort( 0xa4 ),	/* Type Offset=164 */

	/* Return value */

/* 298 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 300 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 302 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure scrollTo */

/* 304 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 306 */	NdrFcLong( 0x0 ),	/* 0 */
/* 310 */	NdrFcShort( 0x8 ),	/* 8 */
/* 312 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 314 */	NdrFcShort( 0x5 ),	/* 5 */
/* 316 */	NdrFcShort( 0x8 ),	/* 8 */
/* 318 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 320 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 322 */	NdrFcShort( 0x0 ),	/* 0 */
/* 324 */	NdrFcShort( 0x0 ),	/* 0 */
/* 326 */	NdrFcShort( 0x0 ),	/* 0 */
/* 328 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter placeTopLeft */

/* 330 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 332 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 334 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 336 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 338 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 340 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_parentNode */

/* 342 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 344 */	NdrFcLong( 0x0 ),	/* 0 */
/* 348 */	NdrFcShort( 0x9 ),	/* 9 */
/* 350 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 352 */	NdrFcShort( 0x0 ),	/* 0 */
/* 354 */	NdrFcShort( 0x8 ),	/* 8 */
/* 356 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 358 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 360 */	NdrFcShort( 0x0 ),	/* 0 */
/* 362 */	NdrFcShort( 0x0 ),	/* 0 */
/* 364 */	NdrFcShort( 0x0 ),	/* 0 */
/* 366 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter node */

/* 368 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 370 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 372 */	NdrFcShort( 0xba ),	/* Type Offset=186 */

	/* Return value */

/* 374 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 376 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 378 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_firstChild */

/* 380 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 382 */	NdrFcLong( 0x0 ),	/* 0 */
/* 386 */	NdrFcShort( 0xa ),	/* 10 */
/* 388 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 390 */	NdrFcShort( 0x0 ),	/* 0 */
/* 392 */	NdrFcShort( 0x8 ),	/* 8 */
/* 394 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 396 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 398 */	NdrFcShort( 0x0 ),	/* 0 */
/* 400 */	NdrFcShort( 0x0 ),	/* 0 */
/* 402 */	NdrFcShort( 0x0 ),	/* 0 */
/* 404 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter node */

/* 406 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 408 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 410 */	NdrFcShort( 0xba ),	/* Type Offset=186 */

	/* Return value */

/* 412 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 414 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 416 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_lastChild */

/* 418 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 420 */	NdrFcLong( 0x0 ),	/* 0 */
/* 424 */	NdrFcShort( 0xb ),	/* 11 */
/* 426 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 428 */	NdrFcShort( 0x0 ),	/* 0 */
/* 430 */	NdrFcShort( 0x8 ),	/* 8 */
/* 432 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 434 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 436 */	NdrFcShort( 0x0 ),	/* 0 */
/* 438 */	NdrFcShort( 0x0 ),	/* 0 */
/* 440 */	NdrFcShort( 0x0 ),	/* 0 */
/* 442 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter node */

/* 444 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 446 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 448 */	NdrFcShort( 0xba ),	/* Type Offset=186 */

	/* Return value */

/* 450 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 452 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 454 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_previousSibling */

/* 456 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 458 */	NdrFcLong( 0x0 ),	/* 0 */
/* 462 */	NdrFcShort( 0xc ),	/* 12 */
/* 464 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 466 */	NdrFcShort( 0x0 ),	/* 0 */
/* 468 */	NdrFcShort( 0x8 ),	/* 8 */
/* 470 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 472 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 474 */	NdrFcShort( 0x0 ),	/* 0 */
/* 476 */	NdrFcShort( 0x0 ),	/* 0 */
/* 478 */	NdrFcShort( 0x0 ),	/* 0 */
/* 480 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter node */

/* 482 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 484 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 486 */	NdrFcShort( 0xba ),	/* Type Offset=186 */

	/* Return value */

/* 488 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 490 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 492 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nextSibling */

/* 494 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 496 */	NdrFcLong( 0x0 ),	/* 0 */
/* 500 */	NdrFcShort( 0xd ),	/* 13 */
/* 502 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 504 */	NdrFcShort( 0x0 ),	/* 0 */
/* 506 */	NdrFcShort( 0x8 ),	/* 8 */
/* 508 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 510 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 512 */	NdrFcShort( 0x0 ),	/* 0 */
/* 514 */	NdrFcShort( 0x0 ),	/* 0 */
/* 516 */	NdrFcShort( 0x0 ),	/* 0 */
/* 518 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter node */

/* 520 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 522 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 524 */	NdrFcShort( 0xba ),	/* Type Offset=186 */

	/* Return value */

/* 526 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 528 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 530 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_childAt */

/* 532 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 534 */	NdrFcLong( 0x0 ),	/* 0 */
/* 538 */	NdrFcShort( 0xe ),	/* 14 */
/* 540 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 542 */	NdrFcShort( 0x8 ),	/* 8 */
/* 544 */	NdrFcShort( 0x8 ),	/* 8 */
/* 546 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 548 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 550 */	NdrFcShort( 0x0 ),	/* 0 */
/* 552 */	NdrFcShort( 0x0 ),	/* 0 */
/* 554 */	NdrFcShort( 0x0 ),	/* 0 */
/* 556 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter childIndex */

/* 558 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 560 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 562 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter node */

/* 564 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 566 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 568 */	NdrFcShort( 0xba ),	/* Type Offset=186 */

	/* Return value */

/* 570 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 572 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 574 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_innerHTML */

/* 576 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 578 */	NdrFcLong( 0x0 ),	/* 0 */
/* 582 */	NdrFcShort( 0xf ),	/* 15 */
/* 584 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 586 */	NdrFcShort( 0x0 ),	/* 0 */
/* 588 */	NdrFcShort( 0x8 ),	/* 8 */
/* 590 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 592 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 594 */	NdrFcShort( 0x1 ),	/* 1 */
/* 596 */	NdrFcShort( 0x0 ),	/* 0 */
/* 598 */	NdrFcShort( 0x0 ),	/* 0 */
/* 600 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter innerHTML */

/* 602 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 604 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 606 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 608 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 610 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 612 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_language */

/* 614 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 616 */	NdrFcLong( 0x0 ),	/* 0 */
/* 620 */	NdrFcShort( 0x11 ),	/* 17 */
/* 622 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 624 */	NdrFcShort( 0x0 ),	/* 0 */
/* 626 */	NdrFcShort( 0x8 ),	/* 8 */
/* 628 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 630 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 632 */	NdrFcShort( 0x1 ),	/* 1 */
/* 634 */	NdrFcShort( 0x0 ),	/* 0 */
/* 636 */	NdrFcShort( 0x0 ),	/* 0 */
/* 638 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter language */

/* 640 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 642 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 644 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 646 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 648 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 650 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const ISimpleDOMNode_MIDL_TYPE_FORMAT_STRING ISimpleDOMNode__MIDL_TypeFormatString =
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
/* 44 */	0x6,		/* FC_SHORT */
			0x5c,		/* FC_PAD */
/* 46 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 48 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */
/* 50 */	
			0x11, 0x0,	/* FC_RP */
/* 52 */	NdrFcShort( 0x2 ),	/* Offset= 2 (54) */
/* 54 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 56 */	NdrFcShort( 0x0 ),	/* 0 */
/* 58 */	0x27,		/* Corr desc:  parameter, FC_USHORT */
			0x0,		/*  */
/* 60 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 62 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 64 */	0x27,		/* Corr desc:  parameter, FC_USHORT */
			0x54,		/* FC_DEREFERENCE */
/* 66 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 68 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 70 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 72 */	NdrFcShort( 0xffd8 ),	/* Offset= -40 (32) */
/* 74 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 76 */	
			0x11, 0x0,	/* FC_RP */
/* 78 */	NdrFcShort( 0x2 ),	/* Offset= 2 (80) */
/* 80 */	
			0x1c,		/* FC_CVARRAY */
			0x1,		/* 1 */
/* 82 */	NdrFcShort( 0x2 ),	/* 2 */
/* 84 */	0x27,		/* Corr desc:  parameter, FC_USHORT */
			0x0,		/*  */
/* 86 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 88 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 90 */	0x27,		/* Corr desc:  parameter, FC_USHORT */
			0x54,		/* FC_DEREFERENCE */
/* 92 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 94 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 96 */	0x6,		/* FC_SHORT */
			0x5b,		/* FC_END */
/* 98 */	
			0x11, 0x0,	/* FC_RP */
/* 100 */	NdrFcShort( 0x10 ),	/* Offset= 16 (116) */
/* 102 */	
			0x12, 0x0,	/* FC_UP */
/* 104 */	NdrFcShort( 0xffae ),	/* Offset= -82 (22) */
/* 106 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 108 */	NdrFcShort( 0x0 ),	/* 0 */
/* 110 */	NdrFcShort( 0x8 ),	/* 8 */
/* 112 */	NdrFcShort( 0x0 ),	/* 0 */
/* 114 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (102) */
/* 116 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 118 */	NdrFcShort( 0x0 ),	/* 0 */
/* 120 */	0x27,		/* Corr desc:  parameter, FC_USHORT */
			0x0,		/*  */
/* 122 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 124 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 126 */	0x27,		/* Corr desc:  parameter, FC_USHORT */
			0x0,		/*  */
/* 128 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 130 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 132 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 134 */	NdrFcShort( 0xffe4 ),	/* Offset= -28 (106) */
/* 136 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 138 */	
			0x11, 0x0,	/* FC_RP */
/* 140 */	NdrFcShort( 0x2 ),	/* Offset= 2 (142) */
/* 142 */	
			0x1c,		/* FC_CVARRAY */
			0x1,		/* 1 */
/* 144 */	NdrFcShort( 0x2 ),	/* 2 */
/* 146 */	0x27,		/* Corr desc:  parameter, FC_USHORT */
			0x0,		/*  */
/* 148 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 150 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 152 */	0x27,		/* Corr desc:  parameter, FC_USHORT */
			0x0,		/*  */
/* 154 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 156 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 158 */	0x6,		/* FC_SHORT */
			0x5b,		/* FC_END */
/* 160 */	
			0x11, 0x0,	/* FC_RP */
/* 162 */	NdrFcShort( 0x2 ),	/* Offset= 2 (164) */
/* 164 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 166 */	NdrFcShort( 0x0 ),	/* 0 */
/* 168 */	0x27,		/* Corr desc:  parameter, FC_USHORT */
			0x0,		/*  */
/* 170 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 172 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 174 */	0x27,		/* Corr desc:  parameter, FC_USHORT */
			0x0,		/*  */
/* 176 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 178 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 180 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 182 */	NdrFcShort( 0xff6a ),	/* Offset= -150 (32) */
/* 184 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 186 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 188 */	NdrFcShort( 0x2 ),	/* Offset= 2 (190) */
/* 190 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 192 */	NdrFcLong( 0x1814ceeb ),	/* 404016875 */
/* 196 */	NdrFcShort( 0x49e2 ),	/* 18914 */
/* 198 */	NdrFcShort( 0x407f ),	/* 16511 */
/* 200 */	0xaf,		/* 175 */
			0x99,		/* 153 */
/* 202 */	0xfa,		/* 250 */
			0x75,		/* 117 */
/* 204 */	0x5a,		/* 90 */
			0x7d,		/* 125 */
/* 206 */	0x26,		/* 38 */
			0x7,		/* 7 */

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



/* Standard interface: __MIDL_itf_ISimpleDOMNode_0000_0000, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IUnknown, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: ISimpleDOMNode, ver. 0.0,
   GUID={0x1814ceeb,0x49e2,0x407f,{0xaf,0x99,0xfa,0x75,0x5a,0x7d,0x26,0x07}} */

#pragma code_seg(".orpc")
static const unsigned short ISimpleDOMNode_FormatStringOffsetTable[] =
    {
    0,
    68,
    130,
    186,
    248,
    304,
    342,
    380,
    418,
    456,
    494,
    532,
    576,
    (unsigned short) -1,
    614
    };

static const MIDL_STUBLESS_PROXY_INFO ISimpleDOMNode_ProxyInfo =
    {
    &Object_StubDesc,
    ISimpleDOMNode__MIDL_ProcFormatString.Format,
    &ISimpleDOMNode_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ISimpleDOMNode_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ISimpleDOMNode__MIDL_ProcFormatString.Format,
    &ISimpleDOMNode_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(18) _ISimpleDOMNodeProxyVtbl = 
{
    &ISimpleDOMNode_ProxyInfo,
    &IID_ISimpleDOMNode,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* ISimpleDOMNode::get_nodeInfo */ ,
    (void *) (INT_PTR) -1 /* ISimpleDOMNode::get_attributes */ ,
    (void *) (INT_PTR) -1 /* ISimpleDOMNode::get_attributesForNames */ ,
    (void *) (INT_PTR) -1 /* ISimpleDOMNode::get_computedStyle */ ,
    (void *) (INT_PTR) -1 /* ISimpleDOMNode::get_computedStyleForProperties */ ,
    (void *) (INT_PTR) -1 /* ISimpleDOMNode::scrollTo */ ,
    (void *) (INT_PTR) -1 /* ISimpleDOMNode::get_parentNode */ ,
    (void *) (INT_PTR) -1 /* ISimpleDOMNode::get_firstChild */ ,
    (void *) (INT_PTR) -1 /* ISimpleDOMNode::get_lastChild */ ,
    (void *) (INT_PTR) -1 /* ISimpleDOMNode::get_previousSibling */ ,
    (void *) (INT_PTR) -1 /* ISimpleDOMNode::get_nextSibling */ ,
    (void *) (INT_PTR) -1 /* ISimpleDOMNode::get_childAt */ ,
    (void *) (INT_PTR) -1 /* ISimpleDOMNode::get_innerHTML */ ,
    0 /* ISimpleDOMNode::get_localInterface */ ,
    (void *) (INT_PTR) -1 /* ISimpleDOMNode::get_language */
};

const CInterfaceStubVtbl _ISimpleDOMNodeStubVtbl =
{
    &IID_ISimpleDOMNode,
    &ISimpleDOMNode_ServerInfo,
    18,
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
    ISimpleDOMNode__MIDL_TypeFormatString.Format,
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

const CInterfaceProxyVtbl * const _ISimpleDOMNode_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_ISimpleDOMNodeProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _ISimpleDOMNode_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_ISimpleDOMNodeStubVtbl,
    0
};

PCInterfaceName const _ISimpleDOMNode_InterfaceNamesList[] = 
{
    "ISimpleDOMNode",
    0
};


#define _ISimpleDOMNode_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _ISimpleDOMNode, pIID, n)

int __stdcall _ISimpleDOMNode_IID_Lookup( const IID * pIID, int * pIndex )
{
    
    if(!_ISimpleDOMNode_CHECK_IID(0))
        {
        *pIndex = 0;
        return 1;
        }

    return 0;
}

EXTERN_C const ExtendedProxyFileInfo ISimpleDOMNode_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _ISimpleDOMNode_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _ISimpleDOMNode_StubVtblList,
    (const PCInterfaceName * ) & _ISimpleDOMNode_InterfaceNamesList,
    0, /* no delegation */
    & _ISimpleDOMNode_IID_Lookup, 
    1,
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

