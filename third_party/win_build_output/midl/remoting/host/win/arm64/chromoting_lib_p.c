

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for gen/remoting/host/win/chromoting_lib.idl:
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


#include "chromoting_lib.h"

#define TYPE_FORMAT_STRING_SIZE   57                                
#define PROC_FORMAT_STRING_SIZE   249                               
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   1            

typedef struct _chromoting_lib_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } chromoting_lib_MIDL_TYPE_FORMAT_STRING;

typedef struct _chromoting_lib_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } chromoting_lib_MIDL_PROC_FORMAT_STRING;

typedef struct _chromoting_lib_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } chromoting_lib_MIDL_EXPR_FORMAT_STRING;


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


extern const chromoting_lib_MIDL_TYPE_FORMAT_STRING chromoting_lib__MIDL_TypeFormatString;
extern const chromoting_lib_MIDL_PROC_FORMAT_STRING chromoting_lib__MIDL_ProcFormatString;
extern const chromoting_lib_MIDL_EXPR_FORMAT_STRING chromoting_lib__MIDL_ExprFormatString;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IRdpDesktopSessionEventHandler_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IRdpDesktopSessionEventHandler_ProxyInfo;

#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC Object_StubDesc;
#ifdef __cplusplus
}
#endif


extern const MIDL_SERVER_INFO IRdpDesktopSession_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IRdpDesktopSession_ProxyInfo;


extern const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ];

#if !defined(__RPC_ARM64__)
#error  Invalid build platform for this stub.
#endif

static const chromoting_lib_MIDL_PROC_FORMAT_STRING chromoting_lib__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure OnRdpConnected */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x3 ),	/* 3 */
/*  8 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 10 */	NdrFcShort( 0x0 ),	/* 0 */
/* 12 */	NdrFcShort( 0x8 ),	/* 8 */
/* 14 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 16 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 18 */	NdrFcShort( 0x0 ),	/* 0 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x1 ),	/* 1 */
/* 26 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 28 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 30 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 32 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Disconnect */


	/* Procedure OnRdpClosed */

/* 34 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 36 */	NdrFcLong( 0x0 ),	/* 0 */
/* 40 */	NdrFcShort( 0x4 ),	/* 4 */
/* 42 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 44 */	NdrFcShort( 0x0 ),	/* 0 */
/* 46 */	NdrFcShort( 0x8 ),	/* 8 */
/* 48 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 50 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 52 */	NdrFcShort( 0x0 ),	/* 0 */
/* 54 */	NdrFcShort( 0x0 ),	/* 0 */
/* 56 */	NdrFcShort( 0x0 ),	/* 0 */
/* 58 */	NdrFcShort( 0x1 ),	/* 1 */
/* 60 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */

/* 62 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 64 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 66 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Connect */

/* 68 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 70 */	NdrFcLong( 0x0 ),	/* 0 */
/* 74 */	NdrFcShort( 0x3 ),	/* 3 */
/* 76 */	NdrFcShort( 0x48 ),	/* ARM64 Stack size/offset = 72 */
/* 78 */	NdrFcShort( 0x28 ),	/* 40 */
/* 80 */	NdrFcShort( 0x8 ),	/* 8 */
/* 82 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x8,		/* 8 */
/* 84 */	0x14,		/* 20 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 86 */	NdrFcShort( 0x0 ),	/* 0 */
/* 88 */	NdrFcShort( 0x1 ),	/* 1 */
/* 90 */	NdrFcShort( 0x0 ),	/* 0 */
/* 92 */	NdrFcShort( 0x8 ),	/* 8 */
/* 94 */	0x8,		/* 8 */
			0x80,		/* 128 */
/* 96 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 98 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 100 */	0x85,		/* 133 */
			0x86,		/* 134 */
/* 102 */	0x87,		/* 135 */
			0x0,		/* 0 */

	/* Parameter width */

/* 104 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 106 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 108 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter height */

/* 110 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 112 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 114 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter dpi_x */

/* 116 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 118 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 120 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter dpi_y */

/* 122 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 124 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 126 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter terminal_id */

/* 128 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 130 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 132 */	NdrFcShort( 0x1c ),	/* Type Offset=28 */

	/* Parameter port_number */

/* 134 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 136 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 138 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter event_handler */

/* 140 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 142 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 144 */	NdrFcShort( 0x26 ),	/* Type Offset=38 */

	/* Return value */

/* 146 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 148 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 150 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure ChangeResolution */

/* 152 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 154 */	NdrFcLong( 0x0 ),	/* 0 */
/* 158 */	NdrFcShort( 0x5 ),	/* 5 */
/* 160 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 162 */	NdrFcShort( 0x20 ),	/* 32 */
/* 164 */	NdrFcShort( 0x8 ),	/* 8 */
/* 166 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x5,		/* 5 */
/* 168 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 170 */	NdrFcShort( 0x0 ),	/* 0 */
/* 172 */	NdrFcShort( 0x0 ),	/* 0 */
/* 174 */	NdrFcShort( 0x0 ),	/* 0 */
/* 176 */	NdrFcShort( 0x5 ),	/* 5 */
/* 178 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 180 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 182 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter width */

/* 184 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 186 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 188 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter height */

/* 190 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 192 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 194 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter dpi_x */

/* 196 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 198 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 200 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter dpi_y */

/* 202 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 204 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 206 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 208 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 210 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 212 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure InjectSas */

/* 214 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 216 */	NdrFcLong( 0x0 ),	/* 0 */
/* 220 */	NdrFcShort( 0x6 ),	/* 6 */
/* 222 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 224 */	NdrFcShort( 0x0 ),	/* 0 */
/* 226 */	NdrFcShort( 0x8 ),	/* 8 */
/* 228 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 230 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 232 */	NdrFcShort( 0x0 ),	/* 0 */
/* 234 */	NdrFcShort( 0x0 ),	/* 0 */
/* 236 */	NdrFcShort( 0x0 ),	/* 0 */
/* 238 */	NdrFcShort( 0x1 ),	/* 1 */
/* 240 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 242 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 244 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 246 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const chromoting_lib_MIDL_TYPE_FORMAT_STRING chromoting_lib__MIDL_TypeFormatString =
    {
        0,
        {
			NdrFcShort( 0x0 ),	/* 0 */
/*  2 */	
			0x12, 0x0,	/* FC_UP */
/*  4 */	NdrFcShort( 0xe ),	/* Offset= 14 (18) */
/*  6 */	
			0x1b,		/* FC_CARRAY */
			0x1,		/* 1 */
/*  8 */	NdrFcShort( 0x2 ),	/* 2 */
/* 10 */	0x9,		/* Corr desc: FC_ULONG */
			0x0,		/*  */
/* 12 */	NdrFcShort( 0xfffc ),	/* -4 */
/* 14 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 16 */	0x6,		/* FC_SHORT */
			0x5b,		/* FC_END */
/* 18 */	
			0x17,		/* FC_CSTRUCT */
			0x3,		/* 3 */
/* 20 */	NdrFcShort( 0x8 ),	/* 8 */
/* 22 */	NdrFcShort( 0xfff0 ),	/* Offset= -16 (6) */
/* 24 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 26 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 28 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 30 */	NdrFcShort( 0x0 ),	/* 0 */
/* 32 */	NdrFcShort( 0x8 ),	/* 8 */
/* 34 */	NdrFcShort( 0x0 ),	/* 0 */
/* 36 */	NdrFcShort( 0xffde ),	/* Offset= -34 (2) */
/* 38 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 40 */	NdrFcLong( 0xb59b96da ),	/* -1248094502 */
/* 44 */	NdrFcShort( 0x83cb ),	/* -31797 */
/* 46 */	NdrFcShort( 0x40ee ),	/* 16622 */
/* 48 */	0x9b,		/* 155 */
			0x91,		/* 145 */
/* 50 */	0xc3,		/* 195 */
			0x77,		/* 119 */
/* 52 */	0x40,		/* 64 */
			0xf,		/* 15 */
/* 54 */	0xc3,		/* 195 */
			0xe3,		/* 227 */

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


/* Object interface: IRdpDesktopSessionEventHandler, ver. 0.0,
   GUID={0xb59b96da,0x83cb,0x40ee,{0x9b,0x91,0xc3,0x77,0x40,0x0f,0xc3,0xe3}} */

#pragma code_seg(".orpc")
static const unsigned short IRdpDesktopSessionEventHandler_FormatStringOffsetTable[] =
    {
    0,
    34
    };

static const MIDL_STUBLESS_PROXY_INFO IRdpDesktopSessionEventHandler_ProxyInfo =
    {
    &Object_StubDesc,
    chromoting_lib__MIDL_ProcFormatString.Format,
    &IRdpDesktopSessionEventHandler_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IRdpDesktopSessionEventHandler_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    chromoting_lib__MIDL_ProcFormatString.Format,
    &IRdpDesktopSessionEventHandler_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(5) _IRdpDesktopSessionEventHandlerProxyVtbl = 
{
    &IRdpDesktopSessionEventHandler_ProxyInfo,
    &IID_IRdpDesktopSessionEventHandler,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IRdpDesktopSessionEventHandler::OnRdpConnected */ ,
    (void *) (INT_PTR) -1 /* IRdpDesktopSessionEventHandler::OnRdpClosed */
};

const CInterfaceStubVtbl _IRdpDesktopSessionEventHandlerStubVtbl =
{
    &IID_IRdpDesktopSessionEventHandler,
    &IRdpDesktopSessionEventHandler_ServerInfo,
    5,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IRdpDesktopSession, ver. 0.0,
   GUID={0x6a7699f0,0xee43,0x43e7,{0xaa,0x30,0xa6,0x73,0x8f,0x9b,0xd4,0x70}} */

#pragma code_seg(".orpc")
static const unsigned short IRdpDesktopSession_FormatStringOffsetTable[] =
    {
    68,
    34,
    152,
    214
    };

static const MIDL_STUBLESS_PROXY_INFO IRdpDesktopSession_ProxyInfo =
    {
    &Object_StubDesc,
    chromoting_lib__MIDL_ProcFormatString.Format,
    &IRdpDesktopSession_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IRdpDesktopSession_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    chromoting_lib__MIDL_ProcFormatString.Format,
    &IRdpDesktopSession_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(7) _IRdpDesktopSessionProxyVtbl = 
{
    &IRdpDesktopSession_ProxyInfo,
    &IID_IRdpDesktopSession,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IRdpDesktopSession::Connect */ ,
    (void *) (INT_PTR) -1 /* IRdpDesktopSession::Disconnect */ ,
    (void *) (INT_PTR) -1 /* IRdpDesktopSession::ChangeResolution */ ,
    (void *) (INT_PTR) -1 /* IRdpDesktopSession::InjectSas */
};

const CInterfaceStubVtbl _IRdpDesktopSessionStubVtbl =
{
    &IID_IRdpDesktopSession,
    &IRdpDesktopSession_ServerInfo,
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
    chromoting_lib__MIDL_TypeFormatString.Format,
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

const CInterfaceProxyVtbl * const _chromoting_lib_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_IRdpDesktopSessionEventHandlerProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IRdpDesktopSessionProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _chromoting_lib_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_IRdpDesktopSessionEventHandlerStubVtbl,
    ( CInterfaceStubVtbl *) &_IRdpDesktopSessionStubVtbl,
    0
};

PCInterfaceName const _chromoting_lib_InterfaceNamesList[] = 
{
    "IRdpDesktopSessionEventHandler",
    "IRdpDesktopSession",
    0
};


#define _chromoting_lib_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _chromoting_lib, pIID, n)

int __stdcall _chromoting_lib_IID_Lookup( const IID * pIID, int * pIndex )
{
    IID_BS_LOOKUP_SETUP

    IID_BS_LOOKUP_INITIAL_TEST( _chromoting_lib, 2, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _chromoting_lib, 2, *pIndex )
    
}

EXTERN_C const ExtendedProxyFileInfo chromoting_lib_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _chromoting_lib_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _chromoting_lib_StubVtblList,
    (const PCInterfaceName * ) & _chromoting_lib_InterfaceNamesList,
    0, /* no delegation */
    & _chromoting_lib_IID_Lookup, 
    2,
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

