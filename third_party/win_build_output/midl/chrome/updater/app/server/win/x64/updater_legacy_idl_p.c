

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../chrome/updater/app/server/win/updater_legacy_idl.template:
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


#include "updater_legacy_idl.h"

#define TYPE_FORMAT_STRING_SIZE   1033                              
#define PROC_FORMAT_STRING_SIZE   1643                              
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


static const RPC_SYNTAX_IDENTIFIER  _RpcTransferSyntax = 
{{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}};


extern const updater_legacy_idl_MIDL_TYPE_FORMAT_STRING updater_legacy_idl__MIDL_TypeFormatString;
extern const updater_legacy_idl_MIDL_PROC_FORMAT_STRING updater_legacy_idl__MIDL_ProcFormatString;
extern const updater_legacy_idl_MIDL_EXPR_FORMAT_STRING updater_legacy_idl__MIDL_ExprFormatString;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO ICurrentState_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ICurrentState_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IGoogleUpdate3Web_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IGoogleUpdate3Web_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAppBundleWeb_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppBundleWeb_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAppWeb_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppWeb_ProxyInfo;


extern const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ];

#if !defined(__RPC_WIN64__)
#error  Invalid build platform for this stub.
#endif

static const updater_legacy_idl_MIDL_PROC_FORMAT_STRING updater_legacy_idl__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure get_stateValue */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x7 ),	/* 7 */
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

	/* Parameter __MIDL__ICurrentState0000 */

/* 26 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 28 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 30 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 32 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 34 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 36 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_availableVersion */

/* 38 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 40 */	NdrFcLong( 0x0 ),	/* 0 */
/* 44 */	NdrFcShort( 0x8 ),	/* 8 */
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

	/* Parameter __MIDL__ICurrentState0001 */

/* 64 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 66 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 68 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */

/* 70 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 72 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 74 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_bytesDownloaded */

/* 76 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 78 */	NdrFcLong( 0x0 ),	/* 0 */
/* 82 */	NdrFcShort( 0x9 ),	/* 9 */
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

	/* Parameter __MIDL__ICurrentState0002 */

/* 102 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 104 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 106 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 108 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 110 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 112 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_totalBytesToDownload */

/* 114 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 116 */	NdrFcLong( 0x0 ),	/* 0 */
/* 120 */	NdrFcShort( 0xa ),	/* 10 */
/* 122 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 124 */	NdrFcShort( 0x0 ),	/* 0 */
/* 126 */	NdrFcShort( 0x24 ),	/* 36 */
/* 128 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 130 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 132 */	NdrFcShort( 0x0 ),	/* 0 */
/* 134 */	NdrFcShort( 0x0 ),	/* 0 */
/* 136 */	NdrFcShort( 0x0 ),	/* 0 */
/* 138 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__ICurrentState0003 */

/* 140 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 142 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 144 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 146 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 148 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 150 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_downloadTimeRemainingMs */

/* 152 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 154 */	NdrFcLong( 0x0 ),	/* 0 */
/* 158 */	NdrFcShort( 0xb ),	/* 11 */
/* 160 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 162 */	NdrFcShort( 0x0 ),	/* 0 */
/* 164 */	NdrFcShort( 0x24 ),	/* 36 */
/* 166 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 168 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 170 */	NdrFcShort( 0x0 ),	/* 0 */
/* 172 */	NdrFcShort( 0x0 ),	/* 0 */
/* 174 */	NdrFcShort( 0x0 ),	/* 0 */
/* 176 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__ICurrentState0004 */

/* 178 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 180 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 182 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 184 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 186 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 188 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nextRetryTime */

/* 190 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 192 */	NdrFcLong( 0x0 ),	/* 0 */
/* 196 */	NdrFcShort( 0xc ),	/* 12 */
/* 198 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 200 */	NdrFcShort( 0x0 ),	/* 0 */
/* 202 */	NdrFcShort( 0x2c ),	/* 44 */
/* 204 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 206 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 208 */	NdrFcShort( 0x0 ),	/* 0 */
/* 210 */	NdrFcShort( 0x0 ),	/* 0 */
/* 212 */	NdrFcShort( 0x0 ),	/* 0 */
/* 214 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__ICurrentState0005 */

/* 216 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 218 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 220 */	0xb,		/* FC_HYPER */
			0x0,		/* 0 */

	/* Return value */

/* 222 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 224 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 226 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_length */


	/* Procedure get_installProgress */

/* 228 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 230 */	NdrFcLong( 0x0 ),	/* 0 */
/* 234 */	NdrFcShort( 0xd ),	/* 13 */
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

	/* Parameter index */


	/* Parameter __MIDL__ICurrentState0006 */

/* 254 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 256 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 258 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 260 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 262 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 264 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installTimeRemainingMs */

/* 266 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 268 */	NdrFcLong( 0x0 ),	/* 0 */
/* 272 */	NdrFcShort( 0xe ),	/* 14 */
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

	/* Parameter __MIDL__ICurrentState0007 */

/* 292 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 294 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 296 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 298 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 300 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 302 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isCanceled */

/* 304 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 306 */	NdrFcLong( 0x0 ),	/* 0 */
/* 310 */	NdrFcShort( 0xf ),	/* 15 */
/* 312 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 314 */	NdrFcShort( 0x0 ),	/* 0 */
/* 316 */	NdrFcShort( 0x22 ),	/* 34 */
/* 318 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 320 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 322 */	NdrFcShort( 0x0 ),	/* 0 */
/* 324 */	NdrFcShort( 0x0 ),	/* 0 */
/* 326 */	NdrFcShort( 0x0 ),	/* 0 */
/* 328 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter is_canceled */

/* 330 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 332 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 334 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 336 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 338 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 340 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_errorCode */

/* 342 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 344 */	NdrFcLong( 0x0 ),	/* 0 */
/* 348 */	NdrFcShort( 0x10 ),	/* 16 */
/* 350 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 352 */	NdrFcShort( 0x0 ),	/* 0 */
/* 354 */	NdrFcShort( 0x24 ),	/* 36 */
/* 356 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 358 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 360 */	NdrFcShort( 0x0 ),	/* 0 */
/* 362 */	NdrFcShort( 0x0 ),	/* 0 */
/* 364 */	NdrFcShort( 0x0 ),	/* 0 */
/* 366 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__ICurrentState0008 */

/* 368 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 370 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 372 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 374 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 376 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 378 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_extraCode1 */

/* 380 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 382 */	NdrFcLong( 0x0 ),	/* 0 */
/* 386 */	NdrFcShort( 0x11 ),	/* 17 */
/* 388 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 390 */	NdrFcShort( 0x0 ),	/* 0 */
/* 392 */	NdrFcShort( 0x24 ),	/* 36 */
/* 394 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 396 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 398 */	NdrFcShort( 0x0 ),	/* 0 */
/* 400 */	NdrFcShort( 0x0 ),	/* 0 */
/* 402 */	NdrFcShort( 0x0 ),	/* 0 */
/* 404 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__ICurrentState0009 */

/* 406 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 408 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 410 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 412 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 414 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 416 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_completionMessage */

/* 418 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 420 */	NdrFcLong( 0x0 ),	/* 0 */
/* 424 */	NdrFcShort( 0x12 ),	/* 18 */
/* 426 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 428 */	NdrFcShort( 0x0 ),	/* 0 */
/* 430 */	NdrFcShort( 0x8 ),	/* 8 */
/* 432 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 434 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 436 */	NdrFcShort( 0x1 ),	/* 1 */
/* 438 */	NdrFcShort( 0x0 ),	/* 0 */
/* 440 */	NdrFcShort( 0x0 ),	/* 0 */
/* 442 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__ICurrentState0010 */

/* 444 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 446 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 448 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */

/* 450 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 452 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 454 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installerResultCode */

/* 456 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 458 */	NdrFcLong( 0x0 ),	/* 0 */
/* 462 */	NdrFcShort( 0x13 ),	/* 19 */
/* 464 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 466 */	NdrFcShort( 0x0 ),	/* 0 */
/* 468 */	NdrFcShort( 0x24 ),	/* 36 */
/* 470 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 472 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 474 */	NdrFcShort( 0x0 ),	/* 0 */
/* 476 */	NdrFcShort( 0x0 ),	/* 0 */
/* 478 */	NdrFcShort( 0x0 ),	/* 0 */
/* 480 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__ICurrentState0011 */

/* 482 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 484 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 486 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 488 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 490 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 492 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installerResultExtraCode1 */

/* 494 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 496 */	NdrFcLong( 0x0 ),	/* 0 */
/* 500 */	NdrFcShort( 0x14 ),	/* 20 */
/* 502 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 504 */	NdrFcShort( 0x0 ),	/* 0 */
/* 506 */	NdrFcShort( 0x24 ),	/* 36 */
/* 508 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 510 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 512 */	NdrFcShort( 0x0 ),	/* 0 */
/* 514 */	NdrFcShort( 0x0 ),	/* 0 */
/* 516 */	NdrFcShort( 0x0 ),	/* 0 */
/* 518 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__ICurrentState0012 */

/* 520 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 522 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 524 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 526 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 528 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 530 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_postInstallLaunchCommandLine */

/* 532 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 534 */	NdrFcLong( 0x0 ),	/* 0 */
/* 538 */	NdrFcShort( 0x15 ),	/* 21 */
/* 540 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 542 */	NdrFcShort( 0x0 ),	/* 0 */
/* 544 */	NdrFcShort( 0x8 ),	/* 8 */
/* 546 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 548 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 550 */	NdrFcShort( 0x1 ),	/* 1 */
/* 552 */	NdrFcShort( 0x0 ),	/* 0 */
/* 554 */	NdrFcShort( 0x0 ),	/* 0 */
/* 556 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__ICurrentState0013 */

/* 558 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 560 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 562 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */

/* 564 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 566 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 568 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_postInstallUrl */

/* 570 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 572 */	NdrFcLong( 0x0 ),	/* 0 */
/* 576 */	NdrFcShort( 0x16 ),	/* 22 */
/* 578 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 580 */	NdrFcShort( 0x0 ),	/* 0 */
/* 582 */	NdrFcShort( 0x8 ),	/* 8 */
/* 584 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 586 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 588 */	NdrFcShort( 0x1 ),	/* 1 */
/* 590 */	NdrFcShort( 0x0 ),	/* 0 */
/* 592 */	NdrFcShort( 0x0 ),	/* 0 */
/* 594 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__ICurrentState0014 */

/* 596 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 598 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 600 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */

/* 602 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 604 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 606 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_postInstallAction */

/* 608 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 610 */	NdrFcLong( 0x0 ),	/* 0 */
/* 614 */	NdrFcShort( 0x17 ),	/* 23 */
/* 616 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 618 */	NdrFcShort( 0x0 ),	/* 0 */
/* 620 */	NdrFcShort( 0x24 ),	/* 36 */
/* 622 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 624 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 626 */	NdrFcShort( 0x0 ),	/* 0 */
/* 628 */	NdrFcShort( 0x0 ),	/* 0 */
/* 630 */	NdrFcShort( 0x0 ),	/* 0 */
/* 632 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__ICurrentState0015 */

/* 634 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 636 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 638 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 640 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 642 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 644 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure createAppBundleWeb */

/* 646 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 648 */	NdrFcLong( 0x0 ),	/* 0 */
/* 652 */	NdrFcShort( 0x7 ),	/* 7 */
/* 654 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 656 */	NdrFcShort( 0x0 ),	/* 0 */
/* 658 */	NdrFcShort( 0x8 ),	/* 8 */
/* 660 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 662 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 664 */	NdrFcShort( 0x0 ),	/* 0 */
/* 666 */	NdrFcShort( 0x0 ),	/* 0 */
/* 668 */	NdrFcShort( 0x0 ),	/* 0 */
/* 670 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_bundle_web */

/* 672 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 674 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 676 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Return value */

/* 678 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 680 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 682 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure createApp */

/* 684 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 686 */	NdrFcLong( 0x0 ),	/* 0 */
/* 690 */	NdrFcShort( 0x7 ),	/* 7 */
/* 692 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 694 */	NdrFcShort( 0x0 ),	/* 0 */
/* 696 */	NdrFcShort( 0x8 ),	/* 8 */
/* 698 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 700 */	0xa,		/* 10 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 702 */	NdrFcShort( 0x0 ),	/* 0 */
/* 704 */	NdrFcShort( 0x1 ),	/* 1 */
/* 706 */	NdrFcShort( 0x0 ),	/* 0 */
/* 708 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_guid */

/* 710 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 712 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 714 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter brand_code */

/* 716 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 718 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 720 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter language */

/* 722 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 724 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 726 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter ap */

/* 728 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 730 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 732 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Return value */

/* 734 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 736 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 738 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure createInstalledApp */

/* 740 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 742 */	NdrFcLong( 0x0 ),	/* 0 */
/* 746 */	NdrFcShort( 0x8 ),	/* 8 */
/* 748 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 750 */	NdrFcShort( 0x0 ),	/* 0 */
/* 752 */	NdrFcShort( 0x8 ),	/* 8 */
/* 754 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 756 */	0xa,		/* 10 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 758 */	NdrFcShort( 0x0 ),	/* 0 */
/* 760 */	NdrFcShort( 0x1 ),	/* 1 */
/* 762 */	NdrFcShort( 0x0 ),	/* 0 */
/* 764 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 766 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 768 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 770 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Return value */

/* 772 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 774 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 776 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure createAllInstalledApps */

/* 778 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 780 */	NdrFcLong( 0x0 ),	/* 0 */
/* 784 */	NdrFcShort( 0x9 ),	/* 9 */
/* 786 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 788 */	NdrFcShort( 0x0 ),	/* 0 */
/* 790 */	NdrFcShort( 0x8 ),	/* 8 */
/* 792 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 794 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 796 */	NdrFcShort( 0x0 ),	/* 0 */
/* 798 */	NdrFcShort( 0x0 ),	/* 0 */
/* 800 */	NdrFcShort( 0x0 ),	/* 0 */
/* 802 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Return value */

/* 804 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 806 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 808 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_displayLanguage */

/* 810 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 812 */	NdrFcLong( 0x0 ),	/* 0 */
/* 816 */	NdrFcShort( 0xa ),	/* 10 */
/* 818 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 820 */	NdrFcShort( 0x0 ),	/* 0 */
/* 822 */	NdrFcShort( 0x8 ),	/* 8 */
/* 824 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 826 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 828 */	NdrFcShort( 0x1 ),	/* 1 */
/* 830 */	NdrFcShort( 0x0 ),	/* 0 */
/* 832 */	NdrFcShort( 0x0 ),	/* 0 */
/* 834 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IAppBundleWeb0000 */

/* 836 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 838 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 840 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */

/* 842 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 844 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 846 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_displayLanguage */

/* 848 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 850 */	NdrFcLong( 0x0 ),	/* 0 */
/* 854 */	NdrFcShort( 0xb ),	/* 11 */
/* 856 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 858 */	NdrFcShort( 0x0 ),	/* 0 */
/* 860 */	NdrFcShort( 0x8 ),	/* 8 */
/* 862 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 864 */	0xa,		/* 10 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 866 */	NdrFcShort( 0x0 ),	/* 0 */
/* 868 */	NdrFcShort( 0x1 ),	/* 1 */
/* 870 */	NdrFcShort( 0x0 ),	/* 0 */
/* 872 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IAppBundleWeb0001 */

/* 874 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 876 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 878 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Return value */

/* 880 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 882 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 884 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_parentHWND */

/* 886 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 888 */	NdrFcLong( 0x0 ),	/* 0 */
/* 892 */	NdrFcShort( 0xc ),	/* 12 */
/* 894 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 896 */	NdrFcShort( 0x8 ),	/* 8 */
/* 898 */	NdrFcShort( 0x8 ),	/* 8 */
/* 900 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 902 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 904 */	NdrFcShort( 0x0 ),	/* 0 */
/* 906 */	NdrFcShort( 0x0 ),	/* 0 */
/* 908 */	NdrFcShort( 0x0 ),	/* 0 */
/* 910 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter hwnd */

/* 912 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 914 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 916 */	0xb9,		/* FC_UINT3264 */
			0x0,		/* 0 */

	/* Return value */

/* 918 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 920 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 922 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_appWeb */

/* 924 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 926 */	NdrFcLong( 0x0 ),	/* 0 */
/* 930 */	NdrFcShort( 0xe ),	/* 14 */
/* 932 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 934 */	NdrFcShort( 0x8 ),	/* 8 */
/* 936 */	NdrFcShort( 0x8 ),	/* 8 */
/* 938 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 940 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 942 */	NdrFcShort( 0x0 ),	/* 0 */
/* 944 */	NdrFcShort( 0x0 ),	/* 0 */
/* 946 */	NdrFcShort( 0x0 ),	/* 0 */
/* 948 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter index */

/* 950 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 952 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 954 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter app_web */

/* 956 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 958 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 960 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Return value */

/* 962 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 964 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 966 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure initialize */

/* 968 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 970 */	NdrFcLong( 0x0 ),	/* 0 */
/* 974 */	NdrFcShort( 0xf ),	/* 15 */
/* 976 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 978 */	NdrFcShort( 0x0 ),	/* 0 */
/* 980 */	NdrFcShort( 0x8 ),	/* 8 */
/* 982 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 984 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 986 */	NdrFcShort( 0x0 ),	/* 0 */
/* 988 */	NdrFcShort( 0x0 ),	/* 0 */
/* 990 */	NdrFcShort( 0x0 ),	/* 0 */
/* 992 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Return value */

/* 994 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 996 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 998 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure checkForUpdate */

/* 1000 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1002 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1006 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1008 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1010 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1012 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1014 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1016 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1018 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1020 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1022 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1024 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Return value */

/* 1026 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1028 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1030 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure download */

/* 1032 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1034 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1038 */	NdrFcShort( 0x11 ),	/* 17 */
/* 1040 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1042 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1044 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1046 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1048 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1050 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1052 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1054 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1056 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Return value */

/* 1058 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1060 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1062 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure install */

/* 1064 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1066 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1070 */	NdrFcShort( 0x12 ),	/* 18 */
/* 1072 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1074 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1076 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1078 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1080 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1082 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1084 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1086 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1088 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Return value */

/* 1090 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1092 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1094 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure pause */

/* 1096 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1098 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1102 */	NdrFcShort( 0x13 ),	/* 19 */
/* 1104 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1106 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1108 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1110 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1112 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1114 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1116 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1118 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1120 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Return value */

/* 1122 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1124 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1126 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure resume */

/* 1128 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1130 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1134 */	NdrFcShort( 0x14 ),	/* 20 */
/* 1136 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1138 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1140 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1142 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1144 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1146 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1148 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1150 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1152 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Return value */

/* 1154 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1156 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1158 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure cancel */

/* 1160 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1162 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1166 */	NdrFcShort( 0x15 ),	/* 21 */
/* 1168 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1170 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1172 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1174 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1176 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1178 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1180 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1182 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1184 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Return value */

/* 1186 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1188 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1190 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure downloadPackage */

/* 1192 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1194 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1198 */	NdrFcShort( 0x16 ),	/* 22 */
/* 1200 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1202 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1204 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1206 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 1208 */	0xa,		/* 10 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1210 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1212 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1214 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1216 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 1218 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1220 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1222 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter package_name */

/* 1224 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1226 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1228 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Return value */

/* 1230 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1232 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1234 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_currentState */

/* 1236 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1238 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1242 */	NdrFcShort( 0x17 ),	/* 23 */
/* 1244 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1246 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1248 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1250 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1252 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1254 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1256 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1258 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1260 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter current_state */

/* 1262 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1264 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1266 */	NdrFcShort( 0x3fe ),	/* Type Offset=1022 */

	/* Return value */

/* 1268 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1270 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1272 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_appId */

/* 1274 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1276 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1280 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1282 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1284 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1286 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1288 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1290 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1292 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1294 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1296 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1298 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IAppWeb0000 */

/* 1300 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1302 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1304 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */

/* 1306 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1308 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1310 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_currentVersionWeb */

/* 1312 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1314 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1318 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1320 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1322 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1324 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1326 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1328 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1330 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1332 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1334 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1336 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter current */

/* 1338 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1340 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1342 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Return value */

/* 1344 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1346 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1348 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nextVersionWeb */

/* 1350 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1352 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1356 */	NdrFcShort( 0x9 ),	/* 9 */
/* 1358 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1360 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1362 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1364 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1366 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1368 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1370 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1372 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1374 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter next */

/* 1376 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1378 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1380 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Return value */

/* 1382 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1384 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1386 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_command */

/* 1388 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1390 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1394 */	NdrFcShort( 0xa ),	/* 10 */
/* 1396 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 1398 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1400 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1402 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 1404 */	0xa,		/* 10 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1406 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1408 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1410 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1412 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter command_id */

/* 1414 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1416 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1418 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Parameter command */

/* 1420 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1422 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1424 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Return value */

/* 1426 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1428 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1430 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure cancel */

/* 1432 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1434 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1438 */	NdrFcShort( 0xb ),	/* 11 */
/* 1440 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1442 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1444 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1446 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1448 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1450 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1452 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1454 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1456 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Return value */

/* 1458 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1460 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1462 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_currentState */

/* 1464 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1466 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1470 */	NdrFcShort( 0xc ),	/* 12 */
/* 1472 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1474 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1476 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1478 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1480 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1482 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1484 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1486 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1488 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter current_state */

/* 1490 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1492 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1494 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Return value */

/* 1496 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1498 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1500 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure launch */

/* 1502 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1504 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1508 */	NdrFcShort( 0xd ),	/* 13 */
/* 1510 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1512 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1514 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1516 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1518 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1520 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1522 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1524 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1526 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Return value */

/* 1528 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1530 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1532 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure uninstall */

/* 1534 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1536 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1540 */	NdrFcShort( 0xe ),	/* 14 */
/* 1542 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1544 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1546 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1548 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1550 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1552 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1554 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1556 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1558 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Return value */

/* 1560 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1562 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1564 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_serverInstallDataIndex */

/* 1566 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1568 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1572 */	NdrFcShort( 0xf ),	/* 15 */
/* 1574 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1576 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1578 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1580 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1582 */	0xa,		/* 10 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1584 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1586 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1588 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1590 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IAppWeb0001 */

/* 1592 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1594 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1596 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Return value */

/* 1598 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1600 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1602 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_serverInstallDataIndex */

/* 1604 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1606 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1610 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1612 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 1614 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1616 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1618 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1620 */	0xa,		/* 10 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1622 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1624 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1626 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1628 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter __MIDL__IAppWeb0002 */

/* 1630 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1632 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 1634 */	NdrFcShort( 0x50 ),	/* Type Offset=80 */

	/* Return value */

/* 1636 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1638 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 1640 */	0x8,		/* FC_LONG */
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
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 52 */	0x6,		/* FC_SHORT */
			0x5c,		/* FC_PAD */
/* 54 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 56 */	NdrFcShort( 0x2 ),	/* Offset= 2 (58) */
/* 58 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 60 */	NdrFcLong( 0x20400 ),	/* 132096 */
/* 64 */	NdrFcShort( 0x0 ),	/* 0 */
/* 66 */	NdrFcShort( 0x0 ),	/* 0 */
/* 68 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 70 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 72 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 74 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 76 */	
			0x12, 0x0,	/* FC_UP */
/* 78 */	NdrFcShort( 0xffcc ),	/* Offset= -52 (26) */
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
/* 176 */	NdrFcShort( 0xff5a ),	/* Offset= -166 (10) */
/* 178 */	NdrFcLong( 0xd ),	/* 13 */
/* 182 */	NdrFcShort( 0xdc ),	/* Offset= 220 (402) */
/* 184 */	NdrFcLong( 0x9 ),	/* 9 */
/* 188 */	NdrFcShort( 0xff7e ),	/* Offset= -130 (58) */
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
/* 514 */	NdrFcShort( 0xfe18 ),	/* Offset= -488 (26) */
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
/* 590 */	NdrFcShort( 0xfdec ),	/* Offset= -532 (58) */
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
/* 962 */	NdrFcShort( 0xfc48 ),	/* Offset= -952 (10) */
/* 964 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 966 */	NdrFcShort( 0xfdcc ),	/* Offset= -564 (402) */
/* 968 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 970 */	NdrFcShort( 0xfc70 ),	/* Offset= -912 (58) */
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

			0x0
        }
    };

static const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ] = 
        {
            
            {
            BSTR_UserSize
            ,BSTR_UserMarshal
            ,BSTR_UserUnmarshal
            ,BSTR_UserFree
            },
            {
            VARIANT_UserSize
            ,VARIANT_UserMarshal
            ,VARIANT_UserUnmarshal
            ,VARIANT_UserFree
            }

        };



/* Standard interface: __MIDL_itf_updater_legacy_idl_0000_0000, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IUnknown, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IDispatch, ver. 0.0,
   GUID={0x00020400,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: ICurrentState, ver. 0.0,
   GUID={0x247954F9,0x9EDC,0x4E68,{0x8C,0xC3,0x15,0x0C,0x2B,0x89,0xEA,0xDF}} */

#pragma code_seg(".orpc")
static const unsigned short ICurrentState_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
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
    418,
    456,
    494,
    532,
    570,
    608
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


static const PRPC_STUB_FUNCTION ICurrentState_table[] =
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


/* Object interface: IGoogleUpdate3Web, ver. 0.0,
   GUID={0x494B20CF,0x282E,0x4BDD,{0x9F,0x5D,0xB7,0x0C,0xB0,0x9D,0x35,0x1E}} */

#pragma code_seg(".orpc")
static const unsigned short IGoogleUpdate3Web_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    646
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


static const PRPC_STUB_FUNCTION IGoogleUpdate3Web_table[] =
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


/* Object interface: IAppBundleWeb, ver. 0.0,
   GUID={0xDD42475D,0x6D46,0x496a,{0x92,0x4E,0xBD,0x56,0x30,0xB4,0xCB,0xBA}} */

#pragma code_seg(".orpc")
static const unsigned short IAppBundleWeb_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    684,
    740,
    778,
    810,
    848,
    886,
    228,
    924,
    968,
    1000,
    1032,
    1064,
    1096,
    1128,
    1160,
    1192,
    1236
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


static const PRPC_STUB_FUNCTION IAppBundleWeb_table[] =
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


/* Object interface: IAppWeb, ver. 0.0,
   GUID={0x18D0F672,0x18B4,0x48e6,{0xAD,0x36,0x6E,0x6B,0xF0,0x1D,0xBB,0xC4}} */

#pragma code_seg(".orpc")
static const unsigned short IAppWeb_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    1274,
    1312,
    1350,
    1388,
    1432,
    1464,
    1502,
    1534,
    1566,
    1604
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


static const PRPC_STUB_FUNCTION IAppWeb_table[] =
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
    0x801026e, /* MIDL Version 8.1.622 */
    0,
    UserMarshalRoutines,
    0,  /* notify & notify_flag routine table */
    0x1, /* MIDL flag */
    0, /* cs routines */
    0,   /* proxy/server info */
    0
    };

const CInterfaceProxyVtbl * const _updater_legacy_idl_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_IAppBundleWebProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppWebProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IGoogleUpdate3WebProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ICurrentStateProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _updater_legacy_idl_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_IAppBundleWebStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppWebStubVtbl,
    ( CInterfaceStubVtbl *) &_IGoogleUpdate3WebStubVtbl,
    ( CInterfaceStubVtbl *) &_ICurrentStateStubVtbl,
    0
};

PCInterfaceName const _updater_legacy_idl_InterfaceNamesList[] = 
{
    "IAppBundleWeb",
    "IAppWeb",
    "IGoogleUpdate3Web",
    "ICurrentState",
    0
};

const IID *  const _updater_legacy_idl_BaseIIDList[] = 
{
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

    IID_BS_LOOKUP_INITIAL_TEST( _updater_legacy_idl, 4, 2 )
    IID_BS_LOOKUP_NEXT_TEST( _updater_legacy_idl, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _updater_legacy_idl, 4, *pIndex )
    
}

const ExtendedProxyFileInfo updater_legacy_idl_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _updater_legacy_idl_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _updater_legacy_idl_StubVtblList,
    (const PCInterfaceName * ) & _updater_legacy_idl_InterfaceNamesList,
    (const IID ** ) & _updater_legacy_idl_BaseIIDList,
    & _updater_legacy_idl_IID_Lookup, 
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


#endif /* defined(_M_AMD64)*/

