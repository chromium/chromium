

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


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

MIDL_DEFINE_GUID(IID, IID_ICurrentState,0x247954F9,0x9EDC,0x4E68,0x8C,0xC3,0x15,0x0C,0x2B,0x89,0xEA,0xDF);


MIDL_DEFINE_GUID(IID, IID_IGoogleUpdate3Web,0x494B20CF,0x282E,0x4BDD,0x9F,0x5D,0xB7,0x0C,0xB0,0x9D,0x35,0x1E);


MIDL_DEFINE_GUID(IID, IID_IAppBundleWeb,0xDD42475D,0x6D46,0x496a,0x92,0x4E,0xBD,0x56,0x30,0xB4,0xCB,0xBA);


MIDL_DEFINE_GUID(IID, IID_IAppWeb,0x18D0F672,0x18B4,0x48e6,0xAD,0x36,0x6E,0x6B,0xF0,0x1D,0xBB,0xC4);


MIDL_DEFINE_GUID(IID, LIBID_UpdaterLegacyLib,0x69464FF0,0xD9EC,0x4037,0xA3,0x5F,0x8A,0xE4,0x35,0x81,0x06,0xCC);


MIDL_DEFINE_GUID(CLSID, CLSID_GoogleUpdate3WebUserClass,0x22181302,0xA8A6,0x4f84,0xA5,0x41,0xE5,0xCB,0xFC,0x70,0xCC,0x43);


MIDL_DEFINE_GUID(CLSID, CLSID_GoogleUpdate3WebSystemClass,0x8A1D4361,0x2C08,0x4700,0xA3,0x51,0x3E,0xAA,0x9C,0xBF,0xF5,0xE4);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



