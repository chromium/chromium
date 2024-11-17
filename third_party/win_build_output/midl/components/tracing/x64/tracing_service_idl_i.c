

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../components/tracing/common/tracing_service_idl.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.xx.xxxx 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#pragma warning( disable: 4049 )  /* more than 64k source lines */


#ifdef __cplusplus
extern "C"{
#endif 


#include <rpc.h>
#include <rpcndr.h>

#ifdef _MIDL_USE_GUIDDEF_

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)

#else // !_MIDL_USE_GUIDDEF_

#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        EXTERN_C __declspec(selectany) const type name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#endif // !_MIDL_USE_GUIDDEF_

MIDL_DEFINE_GUID(IID, IID_ISystemTraceSession,0xDB01E5CE,0x10CE,0x4A84,0x8F,0xAE,0xDA,0x5E,0x46,0xEE,0xF1,0xCF);


MIDL_DEFINE_GUID(IID, IID_ISystemTraceSessionChromium,0xA3FD580A,0xFFD4,0x4075,0x91,0x74,0x75,0xD0,0xB1,0x99,0xD3,0xCB);


MIDL_DEFINE_GUID(IID, IID_ISystemTraceSessionChrome,0x056B3371,0x1C09,0x475B,0xA8,0xD7,0x9E,0x58,0xBF,0x45,0x53,0x3E);


MIDL_DEFINE_GUID(IID, IID_ISystemTraceSessionChromeBeta,0xA69D7D7D,0x9A08,0x422A,0xB6,0xC6,0xB7,0xB8,0xD3,0x76,0xA1,0x2C);


MIDL_DEFINE_GUID(IID, IID_ISystemTraceSessionChromeDev,0xE08ADAE8,0x9334,0x46ED,0xB0,0xCF,0xDD,0x17,0x80,0x15,0x8D,0x55);


MIDL_DEFINE_GUID(IID, IID_ISystemTraceSessionChromeCanary,0x6EFB8558,0x68D1,0x4826,0xA6,0x12,0xA1,0x80,0xB3,0x57,0x03,0x75);


MIDL_DEFINE_GUID(IID, LIBID_SystemTraceSessionLib,0xC9368104,0x11AE,0x4784,0x8C,0x2D,0x11,0x5C,0x5D,0x42,0x19,0x37);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



