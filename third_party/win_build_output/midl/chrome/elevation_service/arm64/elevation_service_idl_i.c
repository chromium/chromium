

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../chrome/elevation_service/elevation_service_idl.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=ARM64 8.01.0628 
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

MIDL_DEFINE_GUID(IID, IID_IElevator,0xA949CB4E,0xC4F9,0x44C4,0xB2,0x13,0x6B,0xF8,0xAA,0x9A,0xC6,0x9C);


MIDL_DEFINE_GUID(IID, IID_IElevatorChromium,0xB88C45B9,0x8825,0x4629,0xB8,0x3E,0x77,0xCC,0x67,0xD9,0xCE,0xED);


MIDL_DEFINE_GUID(IID, IID_IElevatorChrome,0x463ABECF,0x410D,0x407F,0x8A,0xF5,0x0D,0xF3,0x5A,0x00,0x5C,0xC8);


MIDL_DEFINE_GUID(IID, IID_IElevatorChromeBeta,0xA2721D66,0x376E,0x4D2F,0x9F,0x0F,0x90,0x70,0xE9,0xA4,0x2B,0x5F);


MIDL_DEFINE_GUID(IID, IID_IElevatorChromeDev,0xBB2AA26B,0x343A,0x4072,0x8B,0x6F,0x80,0x55,0x7B,0x8C,0xE5,0x71);


MIDL_DEFINE_GUID(IID, IID_IElevatorChromeCanary,0x4F7CE041,0x28E9,0x484F,0x9D,0xD0,0x61,0xA8,0xCA,0xCE,0xFE,0xE4);


MIDL_DEFINE_GUID(IID, LIBID_ElevatorLib,0x0014D784,0x7012,0x4A79,0x8A,0xB6,0xAD,0xDB,0x81,0x93,0xA0,0x6E);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



