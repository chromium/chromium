

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for gen/chrome/credential_provider/gaiacp/gaia_credential_provider.idl:
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

MIDL_DEFINE_GUID(IID, IID_IGaiaCredentialProvider,0xCEC9EF6C,0xB2E6,0x4BB6,0x8F,0x1E,0x17,0x47,0xBA,0x4F,0x71,0x38);


MIDL_DEFINE_GUID(IID, IID_IGaiaCredential,0xE5BF88DF,0x9966,0x465B,0xB2,0x33,0xC1,0xCA,0xC7,0x51,0x0A,0x59);


MIDL_DEFINE_GUID(IID, IID_IReauthCredential,0xCC75BCEA,0xA636,0x4798,0xBF,0x8E,0x0F,0xF6,0x4D,0x74,0x34,0x51);


MIDL_DEFINE_GUID(IID, LIBID_GaiaCredentialProviderLib,0x4ADC3A52,0x8673,0x4CE3,0x81,0xF6,0x83,0x3D,0x18,0xBE,0xEB,0xA2);


MIDL_DEFINE_GUID(CLSID, CLSID_GaiaCredentialProvider,0x89adae71,0xaee5,0x4ee2,0xbf,0xfb,0xe8,0x42,0x4e,0x06,0xf5,0x19);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



