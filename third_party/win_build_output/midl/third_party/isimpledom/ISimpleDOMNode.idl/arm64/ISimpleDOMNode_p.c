

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../third_party/isimpledom/ISimpleDOMNode.idl:
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


#include "ISimpleDOMNode.h"

#define TYPE_FORMAT_STRING_SIZE   209                               
#define PROC_FORMAT_STRING_SIZE   725                               
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

#if !defined(__RPC_ARM64__)
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
/*  8 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 10 */	NdrFcShort( 0x0 ),	/* 0 */
/* 12 */	NdrFcShort( 0x74 ),	/* 116 */
/* 14 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x7,		/* 7 */
/* 16 */	0x12,		/* 18 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 18 */	NdrFcShort( 0x1 ),	/* 1 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x7 ),	/* 7 */
/* 26 */	0x7,		/* 7 */
			0x80,		/* 128 */
/* 28 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 30 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 32 */	0x85,		/* 133 */
			0x86,		/* 134 */

	/* Parameter nodeName */

/* 34 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 36 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 38 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Parameter nameSpaceID */

/* 40 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 42 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 44 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Parameter nodeValue */

/* 46 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 48 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 50 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Parameter numChildren */

/* 52 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 54 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 56 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter uniqueID */

/* 58 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 60 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 62 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter nodeType */

/* 64 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 66 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 68 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 70 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 72 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 74 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attributes */

/* 76 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 78 */	NdrFcLong( 0x0 ),	/* 0 */
/* 82 */	NdrFcShort( 0x4 ),	/* 4 */
/* 84 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 86 */	NdrFcShort( 0x6 ),	/* 6 */
/* 88 */	NdrFcShort( 0x22 ),	/* 34 */
/* 90 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x6,		/* 6 */
/* 92 */	0x12,		/* 18 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 94 */	NdrFcShort( 0x1 ),	/* 1 */
/* 96 */	NdrFcShort( 0x0 ),	/* 0 */
/* 98 */	NdrFcShort( 0x0 ),	/* 0 */
/* 100 */	NdrFcShort( 0x6 ),	/* 6 */
/* 102 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 104 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 106 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 108 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter maxAttribs */

/* 110 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 112 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 114 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Parameter attribNames */

/* 116 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 118 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 120 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Parameter nameSpaceID */

/* 122 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 124 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 126 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter attribValues */

/* 128 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 130 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 132 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Parameter numAttribs */

/* 134 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 136 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 138 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 140 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 142 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 144 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attributesForNames */

/* 146 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 148 */	NdrFcLong( 0x0 ),	/* 0 */
/* 152 */	NdrFcShort( 0x5 ),	/* 5 */
/* 154 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 156 */	NdrFcShort( 0x6 ),	/* 6 */
/* 158 */	NdrFcShort( 0x8 ),	/* 8 */
/* 160 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 162 */	0x10,		/* 16 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 164 */	NdrFcShort( 0x1 ),	/* 1 */
/* 166 */	NdrFcShort( 0x1 ),	/* 1 */
/* 168 */	NdrFcShort( 0x0 ),	/* 0 */
/* 170 */	NdrFcShort( 0x5 ),	/* 5 */
/* 172 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 174 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 176 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter numAttribs */

/* 178 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 180 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 182 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Parameter attribNames */

/* 184 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 186 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 188 */	NdrFcShort( 0x74 ),	/* Type Offset=116 */

	/* Parameter nameSpaceID */

/* 190 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 192 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 194 */	NdrFcShort( 0x8e ),	/* Type Offset=142 */

	/* Parameter attribValues */

/* 196 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 198 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 200 */	NdrFcShort( 0xa4 ),	/* Type Offset=164 */

	/* Return value */

/* 202 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 204 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 206 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_computedStyle */

/* 208 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 210 */	NdrFcLong( 0x0 ),	/* 0 */
/* 214 */	NdrFcShort( 0x6 ),	/* 6 */
/* 216 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 218 */	NdrFcShort( 0xb ),	/* 11 */
/* 220 */	NdrFcShort( 0x22 ),	/* 34 */
/* 222 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x6,		/* 6 */
/* 224 */	0x12,		/* 18 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 226 */	NdrFcShort( 0x1 ),	/* 1 */
/* 228 */	NdrFcShort( 0x0 ),	/* 0 */
/* 230 */	NdrFcShort( 0x0 ),	/* 0 */
/* 232 */	NdrFcShort( 0x6 ),	/* 6 */
/* 234 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 236 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 238 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 240 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter maxStyleProperties */

/* 242 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 244 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 246 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Parameter useAlternateView */

/* 248 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 250 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 252 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Parameter styleProperties */

/* 254 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 256 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 258 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Parameter styleValues */

/* 260 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 262 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 264 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Parameter numStyleProperties */

/* 266 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 268 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 270 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 272 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 274 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 276 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_computedStyleForProperties */

/* 278 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 280 */	NdrFcLong( 0x0 ),	/* 0 */
/* 284 */	NdrFcShort( 0x7 ),	/* 7 */
/* 286 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 288 */	NdrFcShort( 0xb ),	/* 11 */
/* 290 */	NdrFcShort( 0x8 ),	/* 8 */
/* 292 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 294 */	0x10,		/* 16 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 296 */	NdrFcShort( 0x1 ),	/* 1 */
/* 298 */	NdrFcShort( 0x1 ),	/* 1 */
/* 300 */	NdrFcShort( 0x0 ),	/* 0 */
/* 302 */	NdrFcShort( 0x5 ),	/* 5 */
/* 304 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 306 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 308 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter numStyleProperties */

/* 310 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 312 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 314 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Parameter useAlternateView */

/* 316 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 318 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 320 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Parameter styleProperties */

/* 322 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 324 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 326 */	NdrFcShort( 0x74 ),	/* Type Offset=116 */

	/* Parameter styleValues */

/* 328 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 330 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 332 */	NdrFcShort( 0xa4 ),	/* Type Offset=164 */

	/* Return value */

/* 334 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 336 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 338 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure scrollTo */

/* 340 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 342 */	NdrFcLong( 0x0 ),	/* 0 */
/* 346 */	NdrFcShort( 0x8 ),	/* 8 */
/* 348 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 350 */	NdrFcShort( 0x5 ),	/* 5 */
/* 352 */	NdrFcShort( 0x8 ),	/* 8 */
/* 354 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 356 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 358 */	NdrFcShort( 0x0 ),	/* 0 */
/* 360 */	NdrFcShort( 0x0 ),	/* 0 */
/* 362 */	NdrFcShort( 0x0 ),	/* 0 */
/* 364 */	NdrFcShort( 0x2 ),	/* 2 */
/* 366 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 368 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter placeTopLeft */

/* 370 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 372 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 374 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 376 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 378 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 380 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_parentNode */

/* 382 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 384 */	NdrFcLong( 0x0 ),	/* 0 */
/* 388 */	NdrFcShort( 0x9 ),	/* 9 */
/* 390 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 392 */	NdrFcShort( 0x0 ),	/* 0 */
/* 394 */	NdrFcShort( 0x8 ),	/* 8 */
/* 396 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 398 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 400 */	NdrFcShort( 0x0 ),	/* 0 */
/* 402 */	NdrFcShort( 0x0 ),	/* 0 */
/* 404 */	NdrFcShort( 0x0 ),	/* 0 */
/* 406 */	NdrFcShort( 0x2 ),	/* 2 */
/* 408 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 410 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter node */

/* 412 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 414 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 416 */	NdrFcShort( 0xba ),	/* Type Offset=186 */

	/* Return value */

/* 418 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 420 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 422 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_firstChild */

/* 424 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 426 */	NdrFcLong( 0x0 ),	/* 0 */
/* 430 */	NdrFcShort( 0xa ),	/* 10 */
/* 432 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 434 */	NdrFcShort( 0x0 ),	/* 0 */
/* 436 */	NdrFcShort( 0x8 ),	/* 8 */
/* 438 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 440 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 442 */	NdrFcShort( 0x0 ),	/* 0 */
/* 444 */	NdrFcShort( 0x0 ),	/* 0 */
/* 446 */	NdrFcShort( 0x0 ),	/* 0 */
/* 448 */	NdrFcShort( 0x2 ),	/* 2 */
/* 450 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 452 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter node */

/* 454 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 456 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 458 */	NdrFcShort( 0xba ),	/* Type Offset=186 */

	/* Return value */

/* 460 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 462 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 464 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_lastChild */

/* 466 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 468 */	NdrFcLong( 0x0 ),	/* 0 */
/* 472 */	NdrFcShort( 0xb ),	/* 11 */
/* 474 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 476 */	NdrFcShort( 0x0 ),	/* 0 */
/* 478 */	NdrFcShort( 0x8 ),	/* 8 */
/* 480 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 482 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 484 */	NdrFcShort( 0x0 ),	/* 0 */
/* 486 */	NdrFcShort( 0x0 ),	/* 0 */
/* 488 */	NdrFcShort( 0x0 ),	/* 0 */
/* 490 */	NdrFcShort( 0x2 ),	/* 2 */
/* 492 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 494 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter node */

/* 496 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 498 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 500 */	NdrFcShort( 0xba ),	/* Type Offset=186 */

	/* Return value */

/* 502 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 504 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 506 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_previousSibling */

/* 508 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 510 */	NdrFcLong( 0x0 ),	/* 0 */
/* 514 */	NdrFcShort( 0xc ),	/* 12 */
/* 516 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 518 */	NdrFcShort( 0x0 ),	/* 0 */
/* 520 */	NdrFcShort( 0x8 ),	/* 8 */
/* 522 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 524 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 526 */	NdrFcShort( 0x0 ),	/* 0 */
/* 528 */	NdrFcShort( 0x0 ),	/* 0 */
/* 530 */	NdrFcShort( 0x0 ),	/* 0 */
/* 532 */	NdrFcShort( 0x2 ),	/* 2 */
/* 534 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 536 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter node */

/* 538 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 540 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 542 */	NdrFcShort( 0xba ),	/* Type Offset=186 */

	/* Return value */

/* 544 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 546 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 548 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nextSibling */

/* 550 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 552 */	NdrFcLong( 0x0 ),	/* 0 */
/* 556 */	NdrFcShort( 0xd ),	/* 13 */
/* 558 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 560 */	NdrFcShort( 0x0 ),	/* 0 */
/* 562 */	NdrFcShort( 0x8 ),	/* 8 */
/* 564 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 566 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 568 */	NdrFcShort( 0x0 ),	/* 0 */
/* 570 */	NdrFcShort( 0x0 ),	/* 0 */
/* 572 */	NdrFcShort( 0x0 ),	/* 0 */
/* 574 */	NdrFcShort( 0x2 ),	/* 2 */
/* 576 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 578 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter node */

/* 580 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 582 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 584 */	NdrFcShort( 0xba ),	/* Type Offset=186 */

	/* Return value */

/* 586 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 588 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 590 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_childAt */

/* 592 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 594 */	NdrFcLong( 0x0 ),	/* 0 */
/* 598 */	NdrFcShort( 0xe ),	/* 14 */
/* 600 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 602 */	NdrFcShort( 0x8 ),	/* 8 */
/* 604 */	NdrFcShort( 0x8 ),	/* 8 */
/* 606 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 608 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 610 */	NdrFcShort( 0x0 ),	/* 0 */
/* 612 */	NdrFcShort( 0x0 ),	/* 0 */
/* 614 */	NdrFcShort( 0x0 ),	/* 0 */
/* 616 */	NdrFcShort( 0x3 ),	/* 3 */
/* 618 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 620 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter childIndex */

/* 622 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 624 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 626 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter node */

/* 628 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 630 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 632 */	NdrFcShort( 0xba ),	/* Type Offset=186 */

	/* Return value */

/* 634 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 636 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 638 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_innerHTML */

/* 640 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 642 */	NdrFcLong( 0x0 ),	/* 0 */
/* 646 */	NdrFcShort( 0xf ),	/* 15 */
/* 648 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 650 */	NdrFcShort( 0x0 ),	/* 0 */
/* 652 */	NdrFcShort( 0x8 ),	/* 8 */
/* 654 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 656 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 658 */	NdrFcShort( 0x1 ),	/* 1 */
/* 660 */	NdrFcShort( 0x0 ),	/* 0 */
/* 662 */	NdrFcShort( 0x0 ),	/* 0 */
/* 664 */	NdrFcShort( 0x2 ),	/* 2 */
/* 666 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 668 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter innerHTML */

/* 670 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 672 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 674 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 676 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 678 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 680 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_language */

/* 682 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 684 */	NdrFcLong( 0x0 ),	/* 0 */
/* 688 */	NdrFcShort( 0x11 ),	/* 17 */
/* 690 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 692 */	NdrFcShort( 0x0 ),	/* 0 */
/* 694 */	NdrFcShort( 0x8 ),	/* 8 */
/* 696 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 698 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 700 */	NdrFcShort( 0x1 ),	/* 1 */
/* 702 */	NdrFcShort( 0x0 ),	/* 0 */
/* 704 */	NdrFcShort( 0x0 ),	/* 0 */
/* 706 */	NdrFcShort( 0x2 ),	/* 2 */
/* 708 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 710 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter language */

/* 712 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 714 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 716 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 718 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 720 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 722 */	0x8,		/* FC_LONG */
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
/* 60 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 62 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 64 */	0x27,		/* Corr desc:  parameter, FC_USHORT */
			0x54,		/* FC_DEREFERENCE */
/* 66 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
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
/* 86 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 88 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 90 */	0x27,		/* Corr desc:  parameter, FC_USHORT */
			0x54,		/* FC_DEREFERENCE */
/* 92 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
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
/* 122 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 124 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 126 */	0x27,		/* Corr desc:  parameter, FC_USHORT */
			0x0,		/*  */
/* 128 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
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
/* 148 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 150 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 152 */	0x27,		/* Corr desc:  parameter, FC_USHORT */
			0x0,		/*  */
/* 154 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
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
/* 170 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 172 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 174 */	0x27,		/* Corr desc:  parameter, FC_USHORT */
			0x0,		/*  */
/* 176 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
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
    76,
    146,
    208,
    278,
    340,
    382,
    424,
    466,
    508,
    550,
    592,
    640,
    (unsigned short) -1,
    682
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


#endif /* defined(_M_ARM64) */

