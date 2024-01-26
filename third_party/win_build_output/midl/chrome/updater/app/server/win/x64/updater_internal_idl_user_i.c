

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for gen/chrome/updater/app/server/win/updater_internal_idl_user.idl:
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

MIDL_DEFINE_GUID(IID, IID_IUpdaterInternalCallback,0xD272C794,0x2ACE,0x4584,0xB9,0x93,0x3B,0x90,0xC6,0x22,0xBE,0x65);


MIDL_DEFINE_GUID(IID, IID_IUpdaterInternalCallbackUser,0x618D9B82,0x9F51,0x4490,0xAF,0x24,0xBB,0x80,0x48,0x9E,0x15,0x37);


MIDL_DEFINE_GUID(IID, IID_IUpdaterInternal,0x526DA036,0x9BD3,0x4697,0x86,0x5A,0xDA,0x12,0xD3,0x7D,0xFF,0xCA);


MIDL_DEFINE_GUID(IID, IID_IUpdaterInternalUser,0xC82AFDA3,0xCA76,0x46EE,0x96,0xE9,0x47,0x47,0x17,0xBF,0xA7,0xBA);


MIDL_DEFINE_GUID(IID, LIBID_UpdaterInternalLib,0xC6CE92DB,0x72CA,0x42EF,0x8C,0x98,0x6E,0xE9,0x24,0x81,0xB3,0xC9);


MIDL_DEFINE_GUID(CLSID, CLSID_UpdaterInternalUserClass,0x1F87FE2F,0xD6A9,0x4711,0x9D,0x11,0x81,0x87,0x70,0x5F,0x84,0x57);


MIDL_DEFINE_GUID(CLSID, CLSID_UpdaterInternalSystemClass,0x4556BA55,0x517E,0x4F03,0x80,0x16,0x33,0x1A,0x43,0xC2,0x69,0xC9);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



