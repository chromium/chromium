

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../chrome/windows_services/elevated_tracing_service/tracing_service_idl.idl:
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

MIDL_DEFINE_GUID(IID, IID_ISystemTraceSession,0xCF38F35B,0x1913,0x4214,0xAC,0x70,0x6E,0x39,0xC9,0x72,0x9D,0xA3);


MIDL_DEFINE_GUID(IID, IID_ISystemTraceSessionChromium,0xE0B03E2D,0x7682,0x4D83,0xB9,0xFF,0x45,0x74,0xAF,0x72,0x05,0x00);


MIDL_DEFINE_GUID(IID, IID_ISystemTraceSessionChrome,0xA780C41E,0x1D88,0x4E7C,0x98,0xF9,0xB0,0x68,0x96,0x68,0x05,0x5C);


MIDL_DEFINE_GUID(IID, IID_ISystemTraceSessionChromeBeta,0x14F7041D,0x19E4,0x4F7F,0xAB,0x6C,0x85,0x8E,0x09,0xDE,0x9F,0x97);


MIDL_DEFINE_GUID(IID, IID_ISystemTraceSessionChromeDev,0xAEFB2E52,0xD121,0x4617,0xA3,0x66,0xDD,0x78,0x22,0x46,0xFB,0x4B);


MIDL_DEFINE_GUID(IID, IID_ISystemTraceSessionChromeCanary,0x4A5732F2,0xDC92,0x4EE4,0xB8,0xF4,0xA3,0x21,0x69,0x67,0x31,0x2A);


MIDL_DEFINE_GUID(IID, LIBID_SystemTraceSessionLib,0xC9368104,0x11AE,0x4784,0x8C,0x2D,0x11,0x5C,0x5D,0x42,0x19,0x37);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



