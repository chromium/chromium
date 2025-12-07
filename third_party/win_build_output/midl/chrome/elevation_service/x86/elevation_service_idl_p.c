

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../chrome/elevation_service/elevation_service_idl.idl:
    Oicf, W1, Zp8, env=Win32 (32b run), target_arch=X86 8.xx.xxxx 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#if !defined(_M_IA64) && !defined(_M_AMD64) && !defined(_ARM_)


#pragma warning( disable: 4049 )  /* more than 64k source lines */
#if _MSC_VER >= 1200
#pragma warning(push)
#endif

#pragma warning( disable: 4211 )  /* redefine extern to static */
#pragma warning( disable: 4232 )  /* dllimport identity*/
#pragma warning( disable: 4024 )  /* array to pointer mapping*/
#pragma warning( disable: 4152 )  /* function/data pointer conversion in expression */
#pragma warning( disable: 4100 ) /* unreferenced arguments in x86 call */

#pragma optimize("", off ) 

#define USE_STUBLESS_PROXY


/* verify that the <rpcproxy.h> version is high enough to compile this file*/
#ifndef __REDQ_RPCPROXY_H_VERSION__
#define __REQUIRED_RPCPROXY_H_VERSION__ 475
#endif


#include "rpcproxy.h"
#ifndef __RPCPROXY_H_VERSION__
#error this stub requires an updated version of <rpcproxy.h>
#endif /* __RPCPROXY_H_VERSION__ */


#include "elevation_service_idl.h"

#define TYPE_FORMAT_STRING_SIZE   65                                
#define PROC_FORMAT_STRING_SIZE   265                               
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   1            

typedef struct _elevation_service_idl_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } elevation_service_idl_MIDL_TYPE_FORMAT_STRING;

typedef struct _elevation_service_idl_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } elevation_service_idl_MIDL_PROC_FORMAT_STRING;

typedef struct _elevation_service_idl_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } elevation_service_idl_MIDL_EXPR_FORMAT_STRING;


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


extern const elevation_service_idl_MIDL_TYPE_FORMAT_STRING elevation_service_idl__MIDL_TypeFormatString;
extern const elevation_service_idl_MIDL_PROC_FORMAT_STRING elevation_service_idl__MIDL_ProcFormatString;
extern const elevation_service_idl_MIDL_EXPR_FORMAT_STRING elevation_service_idl__MIDL_ExprFormatString;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IElevator_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevator_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IElevator2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevator2_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IElevatorChromium_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevatorChromium_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IElevatorChrome_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevatorChrome_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IElevatorChromeBeta_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevatorChromeBeta_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IElevatorChromeDev_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevatorChromeDev_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IElevatorChromeCanary_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevatorChromeCanary_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IElevator2Chromium_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevator2Chromium_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IElevator2Chrome_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevator2Chrome_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IElevator2ChromeBeta_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevator2ChromeBeta_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IElevator2ChromeDev_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevator2ChromeDev_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IElevator2ChromeCanary_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IElevator2ChromeCanary_ProxyInfo;


extern const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ];

#if !defined(__RPC_WIN32__)
#error  Invalid build platform for this stub.
#endif

#if !(TARGET_IS_NT50_OR_LATER)
#error You need Windows 2000 or later to run this stub because it uses these features:
#error   /robust command line switch.
#error However, your C/C++ compilation flags indicate you intend to run this app on earlier systems.
#error This app will fail with the RPC_X_WRONG_STUB_VERSION error.
#endif


static const elevation_service_idl_MIDL_PROC_FORMAT_STRING elevation_service_idl__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure RunRecoveryCRXElevated */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x3 ),	/* 3 */
/*  8 */	NdrFcShort( 0x20 ),	/* x86 Stack size/offset = 32 */
/* 10 */	NdrFcShort( 0x8 ),	/* 8 */
/* 12 */	NdrFcShort( 0x24 ),	/* 36 */
/* 14 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x7,		/* 7 */
/* 16 */	0x8,		/* 8 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 18 */	NdrFcShort( 0x0 ),	/* 0 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter crx_path */

/* 24 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 26 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 28 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter browser_appid */

/* 30 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 32 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 34 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter browser_version */

/* 36 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 38 */	NdrFcShort( 0xc ),	/* x86 Stack size/offset = 12 */
/* 40 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter session_id */

/* 42 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 44 */	NdrFcShort( 0x10 ),	/* x86 Stack size/offset = 16 */
/* 46 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter caller_proc_id */

/* 48 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 50 */	NdrFcShort( 0x14 ),	/* x86 Stack size/offset = 20 */
/* 52 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter proc_handle */

/* 54 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 56 */	NdrFcShort( 0x18 ),	/* x86 Stack size/offset = 24 */
/* 58 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 60 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 62 */	NdrFcShort( 0x1c ),	/* x86 Stack size/offset = 28 */
/* 64 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure EncryptData */

/* 66 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 68 */	NdrFcLong( 0x0 ),	/* 0 */
/* 72 */	NdrFcShort( 0x4 ),	/* 4 */
/* 74 */	NdrFcShort( 0x18 ),	/* x86 Stack size/offset = 24 */
/* 76 */	NdrFcShort( 0x6 ),	/* 6 */
/* 78 */	NdrFcShort( 0x24 ),	/* 36 */
/* 80 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 82 */	0x8,		/* 8 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 84 */	NdrFcShort( 0x1 ),	/* 1 */
/* 86 */	NdrFcShort( 0x1 ),	/* 1 */
/* 88 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter protection_level */

/* 90 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 92 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 94 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter plaintext */

/* 96 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 98 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 100 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Parameter ciphertext */

/* 102 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 104 */	NdrFcShort( 0xc ),	/* x86 Stack size/offset = 12 */
/* 106 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Parameter last_error */

/* 108 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 110 */	NdrFcShort( 0x10 ),	/* x86 Stack size/offset = 16 */
/* 112 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 114 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 116 */	NdrFcShort( 0x14 ),	/* x86 Stack size/offset = 20 */
/* 118 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure DecryptData */

/* 120 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 122 */	NdrFcLong( 0x0 ),	/* 0 */
/* 126 */	NdrFcShort( 0x5 ),	/* 5 */
/* 128 */	NdrFcShort( 0x14 ),	/* x86 Stack size/offset = 20 */
/* 130 */	NdrFcShort( 0x0 ),	/* 0 */
/* 132 */	NdrFcShort( 0x24 ),	/* 36 */
/* 134 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x4,		/* 4 */
/* 136 */	0x8,		/* 8 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 138 */	NdrFcShort( 0x1 ),	/* 1 */
/* 140 */	NdrFcShort( 0x1 ),	/* 1 */
/* 142 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter ciphertext */

/* 144 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 146 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 148 */	NdrFcShort( 0x24 ),	/* Type Offset=36 */

	/* Parameter plaintext */

/* 150 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 152 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 154 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Parameter last_error */

/* 156 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 158 */	NdrFcShort( 0xc ),	/* x86 Stack size/offset = 12 */
/* 160 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 162 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 164 */	NdrFcShort( 0x10 ),	/* x86 Stack size/offset = 16 */
/* 166 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RunIsolatedChrome */

/* 168 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 170 */	NdrFcLong( 0x0 ),	/* 0 */
/* 174 */	NdrFcShort( 0x6 ),	/* 6 */
/* 176 */	NdrFcShort( 0x1c ),	/* x86 Stack size/offset = 28 */
/* 178 */	NdrFcShort( 0x8 ),	/* 8 */
/* 180 */	NdrFcShort( 0x40 ),	/* 64 */
/* 182 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x6,		/* 6 */
/* 184 */	0x8,		/* 8 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 186 */	NdrFcShort( 0x1 ),	/* 1 */
/* 188 */	NdrFcShort( 0x0 ),	/* 0 */
/* 190 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter flags */

/* 192 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 194 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 196 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter command_line */

/* 198 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 200 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 202 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter log */

/* 204 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 206 */	NdrFcShort( 0xc ),	/* x86 Stack size/offset = 12 */
/* 208 */	NdrFcShort( 0x36 ),	/* Type Offset=54 */

	/* Parameter proc_handle */

/* 210 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 212 */	NdrFcShort( 0x10 ),	/* x86 Stack size/offset = 16 */
/* 214 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter last_error */

/* 216 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 218 */	NdrFcShort( 0x14 ),	/* x86 Stack size/offset = 20 */
/* 220 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 222 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 224 */	NdrFcShort( 0x18 ),	/* x86 Stack size/offset = 24 */
/* 226 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure AcceptInvitation */

/* 228 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 230 */	NdrFcLong( 0x0 ),	/* 0 */
/* 234 */	NdrFcShort( 0x7 ),	/* 7 */
/* 236 */	NdrFcShort( 0xc ),	/* x86 Stack size/offset = 12 */
/* 238 */	NdrFcShort( 0x0 ),	/* 0 */
/* 240 */	NdrFcShort( 0x8 ),	/* 8 */
/* 242 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 244 */	0x8,		/* 8 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 246 */	NdrFcShort( 0x0 ),	/* 0 */
/* 248 */	NdrFcShort( 0x0 ),	/* 0 */
/* 250 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter server_name */

/* 252 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 254 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 256 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Return value */

/* 258 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 260 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 262 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const elevation_service_idl_MIDL_TYPE_FORMAT_STRING elevation_service_idl__MIDL_TypeFormatString =
    {
        0,
        {
			NdrFcShort( 0x0 ),	/* 0 */
/*  2 */	
			0x11, 0x8,	/* FC_RP [simple_pointer] */
/*  4 */	
			0x25,		/* FC_C_WSTRING */
			0x5c,		/* FC_PAD */
/*  6 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/*  8 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */
/* 10 */	
			0x12, 0x0,	/* FC_UP */
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
/* 40 */	NdrFcShort( 0x4 ),	/* 4 */
/* 42 */	NdrFcShort( 0x0 ),	/* 0 */
/* 44 */	NdrFcShort( 0xffde ),	/* Offset= -34 (10) */
/* 46 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 48 */	NdrFcShort( 0x6 ),	/* Offset= 6 (54) */
/* 50 */	
			0x13, 0x0,	/* FC_OP */
/* 52 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (26) */
/* 54 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 56 */	NdrFcShort( 0x0 ),	/* 0 */
/* 58 */	NdrFcShort( 0x4 ),	/* 4 */
/* 60 */	NdrFcShort( 0x0 ),	/* 0 */
/* 62 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (50) */

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



/* Standard interface: __MIDL_itf_elevation_service_idl_0000_0000, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IUnknown, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IElevator, ver. 0.0,
   GUID={0xA949CB4E,0xC4F9,0x44C4,{0xB2,0x13,0x6B,0xF8,0xAA,0x9A,0xC6,0x9C}} */

#pragma code_seg(".orpc")
static const unsigned short IElevator_FormatStringOffsetTable[] =
    {
    0,
    66,
    120
    };

static const MIDL_STUBLESS_PROXY_INFO IElevator_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevator_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(6) _IElevatorProxyVtbl = 
{
    &IElevator_ProxyInfo,
    &IID_IElevator,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IElevator::RunRecoveryCRXElevated */ ,
    (void *) (INT_PTR) -1 /* IElevator::EncryptData */ ,
    (void *) (INT_PTR) -1 /* IElevator::DecryptData */
};

const CInterfaceStubVtbl _IElevatorStubVtbl =
{
    &IID_IElevator,
    &IElevator_ServerInfo,
    6,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IElevator2, ver. 0.0,
   GUID={0x8F7B6792,0x784D,0x4047,{0x84,0x5D,0x17,0x82,0xEF,0xBE,0xF2,0x05}} */

#pragma code_seg(".orpc")
static const unsigned short IElevator2_FormatStringOffsetTable[] =
    {
    0,
    66,
    120,
    168,
    228
    };

static const MIDL_STUBLESS_PROXY_INFO IElevator2_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevator2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(8) _IElevator2ProxyVtbl = 
{
    &IElevator2_ProxyInfo,
    &IID_IElevator2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IElevator::RunRecoveryCRXElevated */ ,
    (void *) (INT_PTR) -1 /* IElevator::EncryptData */ ,
    (void *) (INT_PTR) -1 /* IElevator::DecryptData */ ,
    (void *) (INT_PTR) -1 /* IElevator2::RunIsolatedChrome */ ,
    (void *) (INT_PTR) -1 /* IElevator2::AcceptInvitation */
};

const CInterfaceStubVtbl _IElevator2StubVtbl =
{
    &IID_IElevator2,
    &IElevator2_ServerInfo,
    8,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IElevatorChromium, ver. 0.0,
   GUID={0xB88C45B9,0x8825,0x4629,{0xB8,0x3E,0x77,0xCC,0x67,0xD9,0xCE,0xED}} */

#pragma code_seg(".orpc")
static const unsigned short IElevatorChromium_FormatStringOffsetTable[] =
    {
    0,
    66,
    120,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevatorChromium_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromium_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevatorChromium_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromium_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(6) _IElevatorChromiumProxyVtbl = 
{
    0,
    &IID_IElevatorChromium,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IElevator::RunRecoveryCRXElevated */ ,
    0 /* forced delegation IElevator::EncryptData */ ,
    0 /* forced delegation IElevator::DecryptData */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IElevatorChromium_table[] =
{
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IElevatorChromiumStubVtbl =
{
    &IID_IElevatorChromium,
    &IElevatorChromium_ServerInfo,
    6,
    &IElevatorChromium_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IElevatorChrome, ver. 0.0,
   GUID={0x463ABECF,0x410D,0x407F,{0x8A,0xF5,0x0D,0xF3,0x5A,0x00,0x5C,0xC8}} */

#pragma code_seg(".orpc")
static const unsigned short IElevatorChrome_FormatStringOffsetTable[] =
    {
    0,
    66,
    120,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevatorChrome_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChrome_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevatorChrome_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChrome_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(6) _IElevatorChromeProxyVtbl = 
{
    0,
    &IID_IElevatorChrome,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IElevator::RunRecoveryCRXElevated */ ,
    0 /* forced delegation IElevator::EncryptData */ ,
    0 /* forced delegation IElevator::DecryptData */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IElevatorChrome_table[] =
{
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IElevatorChromeStubVtbl =
{
    &IID_IElevatorChrome,
    &IElevatorChrome_ServerInfo,
    6,
    &IElevatorChrome_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IElevatorChromeBeta, ver. 0.0,
   GUID={0xA2721D66,0x376E,0x4D2F,{0x9F,0x0F,0x90,0x70,0xE9,0xA4,0x2B,0x5F}} */

#pragma code_seg(".orpc")
static const unsigned short IElevatorChromeBeta_FormatStringOffsetTable[] =
    {
    0,
    66,
    120,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevatorChromeBeta_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromeBeta_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevatorChromeBeta_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromeBeta_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(6) _IElevatorChromeBetaProxyVtbl = 
{
    0,
    &IID_IElevatorChromeBeta,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IElevator::RunRecoveryCRXElevated */ ,
    0 /* forced delegation IElevator::EncryptData */ ,
    0 /* forced delegation IElevator::DecryptData */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IElevatorChromeBeta_table[] =
{
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IElevatorChromeBetaStubVtbl =
{
    &IID_IElevatorChromeBeta,
    &IElevatorChromeBeta_ServerInfo,
    6,
    &IElevatorChromeBeta_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IElevatorChromeDev, ver. 0.0,
   GUID={0xBB2AA26B,0x343A,0x4072,{0x8B,0x6F,0x80,0x55,0x7B,0x8C,0xE5,0x71}} */

#pragma code_seg(".orpc")
static const unsigned short IElevatorChromeDev_FormatStringOffsetTable[] =
    {
    0,
    66,
    120,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevatorChromeDev_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromeDev_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevatorChromeDev_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromeDev_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(6) _IElevatorChromeDevProxyVtbl = 
{
    0,
    &IID_IElevatorChromeDev,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IElevator::RunRecoveryCRXElevated */ ,
    0 /* forced delegation IElevator::EncryptData */ ,
    0 /* forced delegation IElevator::DecryptData */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IElevatorChromeDev_table[] =
{
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IElevatorChromeDevStubVtbl =
{
    &IID_IElevatorChromeDev,
    &IElevatorChromeDev_ServerInfo,
    6,
    &IElevatorChromeDev_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IElevatorChromeCanary, ver. 0.0,
   GUID={0x4F7CE041,0x28E9,0x484F,{0x9D,0xD0,0x61,0xA8,0xCA,0xCE,0xFE,0xE4}} */

#pragma code_seg(".orpc")
static const unsigned short IElevatorChromeCanary_FormatStringOffsetTable[] =
    {
    0,
    66,
    120,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevatorChromeCanary_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromeCanary_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevatorChromeCanary_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevatorChromeCanary_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(6) _IElevatorChromeCanaryProxyVtbl = 
{
    0,
    &IID_IElevatorChromeCanary,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IElevator::RunRecoveryCRXElevated */ ,
    0 /* forced delegation IElevator::EncryptData */ ,
    0 /* forced delegation IElevator::DecryptData */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IElevatorChromeCanary_table[] =
{
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IElevatorChromeCanaryStubVtbl =
{
    &IID_IElevatorChromeCanary,
    &IElevatorChromeCanary_ServerInfo,
    6,
    &IElevatorChromeCanary_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IElevator2Chromium, ver. 0.0,
   GUID={0xBB19A0E5,0x00C6,0x4966,{0x94,0xB2,0x5A,0xFE,0xC6,0xFE,0xD9,0x3A}} */

#pragma code_seg(".orpc")
static const unsigned short IElevator2Chromium_FormatStringOffsetTable[] =
    {
    0,
    66,
    120,
    168,
    228,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevator2Chromium_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator2Chromium_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevator2Chromium_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator2Chromium_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(8) _IElevator2ChromiumProxyVtbl = 
{
    0,
    &IID_IElevator2Chromium,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IElevator::RunRecoveryCRXElevated */ ,
    0 /* forced delegation IElevator::EncryptData */ ,
    0 /* forced delegation IElevator::DecryptData */ ,
    0 /* forced delegation IElevator2::RunIsolatedChrome */ ,
    0 /* forced delegation IElevator2::AcceptInvitation */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IElevator2Chromium_table[] =
{
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IElevator2ChromiumStubVtbl =
{
    &IID_IElevator2Chromium,
    &IElevator2Chromium_ServerInfo,
    8,
    &IElevator2Chromium_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IElevator2Chrome, ver. 0.0,
   GUID={0x1BF5208B,0x295F,0x4992,{0xB5,0xF4,0x3A,0x9B,0xB6,0x49,0x48,0x38}} */

#pragma code_seg(".orpc")
static const unsigned short IElevator2Chrome_FormatStringOffsetTable[] =
    {
    0,
    66,
    120,
    168,
    228,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevator2Chrome_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator2Chrome_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevator2Chrome_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator2Chrome_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(8) _IElevator2ChromeProxyVtbl = 
{
    0,
    &IID_IElevator2Chrome,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IElevator::RunRecoveryCRXElevated */ ,
    0 /* forced delegation IElevator::EncryptData */ ,
    0 /* forced delegation IElevator::DecryptData */ ,
    0 /* forced delegation IElevator2::RunIsolatedChrome */ ,
    0 /* forced delegation IElevator2::AcceptInvitation */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IElevator2Chrome_table[] =
{
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IElevator2ChromeStubVtbl =
{
    &IID_IElevator2Chrome,
    &IElevator2Chrome_ServerInfo,
    8,
    &IElevator2Chrome_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IElevator2ChromeBeta, ver. 0.0,
   GUID={0xB96A14B8,0xD0B0,0x44D8,{0xBA,0x68,0x23,0x85,0xB2,0xA0,0x32,0x54}} */

#pragma code_seg(".orpc")
static const unsigned short IElevator2ChromeBeta_FormatStringOffsetTable[] =
    {
    0,
    66,
    120,
    168,
    228,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevator2ChromeBeta_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator2ChromeBeta_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevator2ChromeBeta_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator2ChromeBeta_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(8) _IElevator2ChromeBetaProxyVtbl = 
{
    0,
    &IID_IElevator2ChromeBeta,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IElevator::RunRecoveryCRXElevated */ ,
    0 /* forced delegation IElevator::EncryptData */ ,
    0 /* forced delegation IElevator::DecryptData */ ,
    0 /* forced delegation IElevator2::RunIsolatedChrome */ ,
    0 /* forced delegation IElevator2::AcceptInvitation */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IElevator2ChromeBeta_table[] =
{
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IElevator2ChromeBetaStubVtbl =
{
    &IID_IElevator2ChromeBeta,
    &IElevator2ChromeBeta_ServerInfo,
    8,
    &IElevator2ChromeBeta_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IElevator2ChromeDev, ver. 0.0,
   GUID={0x3FEFA48E,0xC8BF,0x461F,{0xAE,0xD6,0x63,0xF6,0x58,0xCC,0x85,0x0A}} */

#pragma code_seg(".orpc")
static const unsigned short IElevator2ChromeDev_FormatStringOffsetTable[] =
    {
    0,
    66,
    120,
    168,
    228,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevator2ChromeDev_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator2ChromeDev_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevator2ChromeDev_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator2ChromeDev_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(8) _IElevator2ChromeDevProxyVtbl = 
{
    0,
    &IID_IElevator2ChromeDev,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IElevator::RunRecoveryCRXElevated */ ,
    0 /* forced delegation IElevator::EncryptData */ ,
    0 /* forced delegation IElevator::DecryptData */ ,
    0 /* forced delegation IElevator2::RunIsolatedChrome */ ,
    0 /* forced delegation IElevator2::AcceptInvitation */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IElevator2ChromeDev_table[] =
{
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IElevator2ChromeDevStubVtbl =
{
    &IID_IElevator2ChromeDev,
    &IElevator2ChromeDev_ServerInfo,
    8,
    &IElevator2ChromeDev_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IElevator2ChromeCanary, ver. 0.0,
   GUID={0xFF672E9F,0x0994,0x4322,{0x81,0xE5,0x3A,0x5A,0x97,0x46,0x14,0x0A}} */

#pragma code_seg(".orpc")
static const unsigned short IElevator2ChromeCanary_FormatStringOffsetTable[] =
    {
    0,
    66,
    120,
    168,
    228,
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IElevator2ChromeCanary_ProxyInfo =
    {
    &Object_StubDesc,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator2ChromeCanary_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IElevator2ChromeCanary_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    elevation_service_idl__MIDL_ProcFormatString.Format,
    &IElevator2ChromeCanary_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(8) _IElevator2ChromeCanaryProxyVtbl = 
{
    0,
    &IID_IElevator2ChromeCanary,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* forced delegation IElevator::RunRecoveryCRXElevated */ ,
    0 /* forced delegation IElevator::EncryptData */ ,
    0 /* forced delegation IElevator::DecryptData */ ,
    0 /* forced delegation IElevator2::RunIsolatedChrome */ ,
    0 /* forced delegation IElevator2::AcceptInvitation */
};


EXTERN_C DECLSPEC_SELECTANY const PRPC_STUB_FUNCTION IElevator2ChromeCanary_table[] =
{
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IElevator2ChromeCanaryStubVtbl =
{
    &IID_IElevator2ChromeCanary,
    &IElevator2ChromeCanary_ServerInfo,
    8,
    &IElevator2ChromeCanary_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
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
    elevation_service_idl__MIDL_TypeFormatString.Format,
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

const CInterfaceProxyVtbl * const _elevation_service_idl_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_IElevatorChromeCanaryProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevatorProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevatorChromeBetaProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevatorChromeDevProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevator2ChromeProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevator2ChromeDevProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevator2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevator2ChromeCanaryProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevator2ChromeBetaProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevatorChromiumProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevatorChromeProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IElevator2ChromiumProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _elevation_service_idl_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_IElevatorChromeCanaryStubVtbl,
    ( CInterfaceStubVtbl *) &_IElevatorStubVtbl,
    ( CInterfaceStubVtbl *) &_IElevatorChromeBetaStubVtbl,
    ( CInterfaceStubVtbl *) &_IElevatorChromeDevStubVtbl,
    ( CInterfaceStubVtbl *) &_IElevator2ChromeStubVtbl,
    ( CInterfaceStubVtbl *) &_IElevator2ChromeDevStubVtbl,
    ( CInterfaceStubVtbl *) &_IElevator2StubVtbl,
    ( CInterfaceStubVtbl *) &_IElevator2ChromeCanaryStubVtbl,
    ( CInterfaceStubVtbl *) &_IElevator2ChromeBetaStubVtbl,
    ( CInterfaceStubVtbl *) &_IElevatorChromiumStubVtbl,
    ( CInterfaceStubVtbl *) &_IElevatorChromeStubVtbl,
    ( CInterfaceStubVtbl *) &_IElevator2ChromiumStubVtbl,
    0
};

PCInterfaceName const _elevation_service_idl_InterfaceNamesList[] = 
{
    "IElevatorChromeCanary",
    "IElevator",
    "IElevatorChromeBeta",
    "IElevatorChromeDev",
    "IElevator2Chrome",
    "IElevator2ChromeDev",
    "IElevator2",
    "IElevator2ChromeCanary",
    "IElevator2ChromeBeta",
    "IElevatorChromium",
    "IElevatorChrome",
    "IElevator2Chromium",
    0
};

const IID *  const _elevation_service_idl_BaseIIDList[] = 
{
    &IID_IElevator,   /* forced */
    0,
    &IID_IElevator,   /* forced */
    &IID_IElevator,   /* forced */
    &IID_IElevator2,   /* forced */
    &IID_IElevator2,   /* forced */
    0,
    &IID_IElevator2,   /* forced */
    &IID_IElevator2,   /* forced */
    &IID_IElevator,   /* forced */
    &IID_IElevator,   /* forced */
    &IID_IElevator2,   /* forced */
    0
};


#define _elevation_service_idl_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _elevation_service_idl, pIID, n)

int __stdcall _elevation_service_idl_IID_Lookup( const IID * pIID, int * pIndex )
{
    IID_BS_LOOKUP_SETUP

    IID_BS_LOOKUP_INITIAL_TEST( _elevation_service_idl, 12, 8 )
    IID_BS_LOOKUP_NEXT_TEST( _elevation_service_idl, 4 )
    IID_BS_LOOKUP_NEXT_TEST( _elevation_service_idl, 2 )
    IID_BS_LOOKUP_NEXT_TEST( _elevation_service_idl, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _elevation_service_idl, 12, *pIndex )
    
}

EXTERN_C const ExtendedProxyFileInfo elevation_service_idl_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _elevation_service_idl_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _elevation_service_idl_StubVtblList,
    (const PCInterfaceName * ) & _elevation_service_idl_InterfaceNamesList,
    (const IID ** ) & _elevation_service_idl_BaseIIDList,
    & _elevation_service_idl_IID_Lookup, 
    12,
    2,
    0, /* table of [async_uuid] interfaces */
    0, /* Filler1 */
    0, /* Filler2 */
    0  /* Filler3 */
};
#if _MSC_VER >= 1200
#pragma warning(pop)
#endif


#endif /* !defined(_M_IA64) && !defined(_M_AMD64) && !defined(_ARM_) */

