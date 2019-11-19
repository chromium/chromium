// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef __WEBAUTHN_H_
#define __WEBAUTHN_H_

#pragma once

#include <winapifamily.h>

#pragma region Desktop Family or OneCore Family
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP | WINAPI_PARTITION_SYSTEM)

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WINAPI
#define WINAPI __stdcall
#endif

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

//+------------------------------------------------------------------------------------------
// API Version Information.
// Caller should check for WebAuthNGetApiVersionNumber to check the presence of relevant APIs
// and features for their usage.
//-------------------------------------------------------------------------------------------

#define WEBAUTHN_API_VERSION_1          1
// WEBAUTHN_API_VERSION_1 : Baseline Version
//      Data Structures and their sub versions:
//          - WEBAUTHN_RP_ENTITY_INFORMATION                    :   1
//          - WEBAUTHN_USER_ENTITY_INFORMATION                  :   1
//          - WEBAUTHN_CLIENT_DATA                              :   1
//          - WEBAUTHN_COSE_CREDENTIAL_PARAMETER                :   1
//          - WEBAUTHN_COSE_CREDENTIAL_PARAMETERS               :   Not Applicable
//          - WEBAUTHN_CREDENTIAL                               :   1
//          - WEBAUTHN_CREDENTIALS                              :   Not Applicable
//          - WEBAUTHN_CREDENTIAL_EX                            :   1
//          - WEBAUTHN_CREDENTIAL_LIST                          :   Not Applicable
//          - WEBAUTHN_EXTENSION                                :   Not Applicable
//          - WEBAUTHN_EXTENSIONS                               :   Not Applicable
//          - WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS    :   3
//          - WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS      :   4
//          - WEBAUTHN_COMMON_ATTESTATION                       :   1
//          - WEBAUTHN_CREDENTIAL_ATTESTATION                   :   3
//          - WEBAUTHN_ASSERTION                                :   1
//      Extensions:
//          - WEBAUTHN_EXTENSIONS_IDENTIFIER_HMAC_SECRET
//      APIs:
//          - WebAuthNGetApiVersionNumber
//          - WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable
//          - WebAuthNAuthenticatorMakeCredential
//          - WebAuthNAuthenticatorGetAssertion
//          - WebAuthNFreeCredentialAttestation
//          - WebAuthNFreeAssertion
//          - WebAuthNGetCancellationId
//          - WebAuthNCancelCurrentOperation
//          - WebAuthNGetErrorName
//          - WebAuthNGetW3CExceptionDOMError

#define WEBAUTHN_API_VERSION_2 2
// WEBAUTHN_API_VERSION_2 : Delta From WEBAUTHN_API_VERSION_1
//      Added Extensions:
//          - WEBAUTHN_EXTENSIONS_IDENTIFIER_CRED_PROTECT
//

#define WEBAUTHN_API_CURRENT_VERSION WEBAUTHN_API_VERSION_2

//+------------------------------------------------------------------------------------------
// Information about an RP Entity
//-------------------------------------------------------------------------------------------

#define WEBAUTHN_RP_ENTITY_INFORMATION_CURRENT_VERSION          1

typedef struct _WEBAUTHN_RP_ENTITY_INFORMATION {
    // Version of this structure, to allow for modifications in the future.
    // This field is required and should be set to CURRENT_VERSION above.
    DWORD dwVersion;

    // Identifier for the RP. This field is required.
    PCWSTR pwszId;

    // Contains the friendly name of the Relying Party, such as "Acme Corporation", "Widgets Inc" or "Awesome Site".
    // This field is required.
    PCWSTR pwszName;

    // Optional URL pointing to RP's logo. 
    PCWSTR pwszIcon;
} WEBAUTHN_RP_ENTITY_INFORMATION, *PWEBAUTHN_RP_ENTITY_INFORMATION;
typedef const WEBAUTHN_RP_ENTITY_INFORMATION *PCWEBAUTHN_RP_ENTITY_INFORMATION;

//+------------------------------------------------------------------------------------------
// Information about an User Entity
//-------------------------------------------------------------------------------------------
#define WEBAUTHN_MAX_USER_ID_LENGTH                             64

#define WEBAUTHN_USER_ENTITY_INFORMATION_CURRENT_VERSION        1

typedef struct _WEBAUTHN_USER_ENTITY_INFORMATION {
    // Version of this structure, to allow for modifications in the future.
    // This field is required and should be set to CURRENT_VERSION above.
    DWORD dwVersion;

    // Identifier for the User. This field is required.
    DWORD cbId;
    _Field_size_bytes_(cbId)
    PBYTE pbId;

    // Contains a detailed name for this account, such as "john.p.smith@example.com".
    PCWSTR pwszName;

    // Optional URL that can be used to retrieve an image containing the user's current avatar,
    // or a data URI that contains the image data.
    PCWSTR pwszIcon;

    // For User: Contains the friendly name associated with the user account by the Relying Party, such as "John P. Smith".
    PCWSTR pwszDisplayName;
} WEBAUTHN_USER_ENTITY_INFORMATION, *PWEBAUTHN_USER_ENTITY_INFORMATION;
typedef const WEBAUTHN_USER_ENTITY_INFORMATION *PCWEBAUTHN_USER_ENTITY_INFORMATION;

//+------------------------------------------------------------------------------------------
// Information about client data.
//-------------------------------------------------------------------------------------------

#define WEBAUTHN_HASH_ALGORITHM_SHA_256                         L"SHA-256"
#define WEBAUTHN_HASH_ALGORITHM_SHA_384                         L"SHA-384"
#define WEBAUTHN_HASH_ALGORITHM_SHA_512                         L"SHA-512"

#define WEBAUTHN_CLIENT_DATA_CURRENT_VERSION                    1

typedef struct _WEBAUTHN_CLIENT_DATA {
    // Version of this structure, to allow for modifications in the future.
    // This field is required and should be set to CURRENT_VERSION above.
    DWORD dwVersion;

    // Size of the pbClientDataJSON field.
    DWORD cbClientDataJSON;
    // UTF-8 encoded JSON serialization of the client data.
    _Field_size_bytes_(cbClientDataJSON)
    PBYTE pbClientDataJSON;

    // Hash algorithm ID used to hash the pbClientDataJSON field.
    LPCWSTR pwszHashAlgId;
} WEBAUTHN_CLIENT_DATA, *PWEBAUTHN_CLIENT_DATA;
typedef const WEBAUTHN_CLIENT_DATA *PCWEBAUTHN_CLIENT_DATA;

//+------------------------------------------------------------------------------------------
// Information about credential parameters.
//-------------------------------------------------------------------------------------------

#define WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY                         L"public-key"

#define WEBAUTHN_COSE_ALGORITHM_ECDSA_P256_WITH_SHA256             -7
#define WEBAUTHN_COSE_ALGORITHM_ECDSA_P384_WITH_SHA384             -35
#define WEBAUTHN_COSE_ALGORITHM_ECDSA_P521_WITH_SHA512             -36

#define WEBAUTHN_COSE_ALGORITHM_RSASSA_PKCS1_V1_5_WITH_SHA256      -257
#define WEBAUTHN_COSE_ALGORITHM_RSASSA_PKCS1_V1_5_WITH_SHA384      -258
#define WEBAUTHN_COSE_ALGORITHM_RSASSA_PKCS1_V1_5_WITH_SHA512      -259

#define WEBAUTHN_COSE_ALGORITHM_RSA_PSS_WITH_SHA256                -37
#define WEBAUTHN_COSE_ALGORITHM_RSA_PSS_WITH_SHA384                -38
#define WEBAUTHN_COSE_ALGORITHM_RSA_PSS_WITH_SHA512                -39

#define WEBAUTHN_COSE_CREDENTIAL_PARAMETER_CURRENT_VERSION          1

typedef struct _WEBAUTHN_COSE_CREDENTIAL_PARAMETER {
    // Version of this structure, to allow for modifications in the future.
    DWORD dwVersion;

    // Well-known credential type specifying a credential to create.
    LPCWSTR pwszCredentialType;

    // Well-known COSE algorithm specifying the algorithm to use for the credential.
    LONG lAlg;
} WEBAUTHN_COSE_CREDENTIAL_PARAMETER, *PWEBAUTHN_COSE_CREDENTIAL_PARAMETER;
typedef const WEBAUTHN_COSE_CREDENTIAL_PARAMETER *PCWEBAUTHN_COSE_CREDENTIAL_PARAMETER;

typedef struct _WEBAUTHN_COSE_CREDENTIAL_PARAMETERS {
    DWORD cCredentialParameters;
    _Field_size_(cCredentialParameters)
    PWEBAUTHN_COSE_CREDENTIAL_PARAMETER pCredentialParameters;
} WEBAUTHN_COSE_CREDENTIAL_PARAMETERS, *PWEBAUTHN_COSE_CREDENTIAL_PARAMETERS;
typedef const WEBAUTHN_COSE_CREDENTIAL_PARAMETERS *PCWEBAUTHN_COSE_CREDENTIAL_PARAMETERS;

//+------------------------------------------------------------------------------------------
// Information about credential.
//-------------------------------------------------------------------------------------------
#define WEBAUTHN_CREDENTIAL_CURRENT_VERSION                         1

typedef struct _WEBAUTHN_CREDENTIAL {
    // Version of this structure, to allow for modifications in the future.
    DWORD dwVersion;

    // Size of pbID.
    DWORD cbId;
    // Unique ID for this particular credential.
    _Field_size_bytes_(cbId)
    PBYTE pbId;

    // Well-known credential type specifying what this particular credential is.
    LPCWSTR pwszCredentialType;
} WEBAUTHN_CREDENTIAL, *PWEBAUTHN_CREDENTIAL;
typedef const WEBAUTHN_CREDENTIAL *PCWEBAUTHN_CREDENTIAL;

typedef struct _WEBAUTHN_CREDENTIALS {
    DWORD cCredentials;
    _Field_size_(cCredentials)
    PWEBAUTHN_CREDENTIAL pCredentials;
} WEBAUTHN_CREDENTIALS, *PWEBAUTHN_CREDENTIALS;
typedef const WEBAUTHN_CREDENTIALS *PCWEBAUTHN_CREDENTIALS;

//+------------------------------------------------------------------------------------------
// Information about credential with extra information, such as, dwTransports
//-------------------------------------------------------------------------------------------

#define WEBAUTHN_CTAP_TRANSPORT_USB         0x00000001
#define WEBAUTHN_CTAP_TRANSPORT_NFC         0x00000002
#define WEBAUTHN_CTAP_TRANSPORT_BLE         0x00000004
#define WEBAUTHN_CTAP_TRANSPORT_TEST        0x00000008
#define WEBAUTHN_CTAP_TRANSPORT_INTERNAL    0x00000010
#define WEBAUTHN_CTAP_TRANSPORT_FLAGS_MASK  0x0000001F

#define WEBAUTHN_CREDENTIAL_EX_CURRENT_VERSION                         1

typedef struct _WEBAUTHN_CREDENTIAL_EX {
    // Version of this structure, to allow for modifications in the future.
    DWORD dwVersion;

    // Size of pbID.
    DWORD cbId;
    // Unique ID for this particular credential.
    _Field_size_bytes_(cbId)
    PBYTE pbId;

    // Well-known credential type specifying what this particular credential is.
    LPCWSTR pwszCredentialType;

    // Transports. 0 implies no transport restrictions.
    DWORD dwTransports;
} WEBAUTHN_CREDENTIAL_EX, *PWEBAUTHN_CREDENTIAL_EX;
typedef const WEBAUTHN_CREDENTIAL_EX *PCWEBAUTHN_CREDENTIAL_EX;

//+------------------------------------------------------------------------------------------
// Information about credential list with extra information
//-------------------------------------------------------------------------------------------

typedef struct _WEBAUTHN_CREDENTIAL_LIST {
    DWORD cCredentials;
    _Field_size_(cCredentials)
    PWEBAUTHN_CREDENTIAL_EX *ppCredentials;
} WEBAUTHN_CREDENTIAL_LIST, *PWEBAUTHN_CREDENTIAL_LIST;
typedef const WEBAUTHN_CREDENTIAL_LIST *PCWEBAUTHN_CREDENTIAL_LIST;

//+------------------------------------------------------------------------------------------
// Hmac-Secret extension
//-------------------------------------------------------------------------------------------

#define WEBAUTHN_EXTENSIONS_IDENTIFIER_HMAC_SECRET                  L"hmac-secret"
// Below type definitions is for WEBAUTHN_EXTENSIONS_IDENTIFIER_HMAC_SECRET
// MakeCredential Input Type:   BOOL.
//      - pvExtension must point to a BOOL with the value TRUE.
//      - cbExtension must contain the sizeof(BOOL).
// MakeCredential Output Type:  BOOL.
//      - pvExtension will point to a BOOL with the value TRUE if credential
//        was successfully created with HMAC_SECRET.
//      - cbExtension will contain the sizeof(BOOL).
// GetAssertion Input Type:     Not Supported
// GetAssertion Output Type:    Not Supported

//+------------------------------------------------------------------------------------------
//  credProtect  extension
//-------------------------------------------------------------------------------------------

#define WEBAUTHN_USER_VERIFICATION_ANY 0
#define WEBAUTHN_USER_VERIFICATION_OPTIONAL 1
#define WEBAUTHN_USER_VERIFICATION_OPTIONAL_WITH_CREDENTIAL_ID_LIST 2
#define WEBAUTHN_USER_VERIFICATION_REQUIRED 3

typedef struct _WEBAUTHN_CRED_PROTECT_EXTENSION_IN {
  // One of the above WEBAUTHN_USER_VERIFICATION_* values
  DWORD dwCredProtect;
  // Set the following to TRUE to require authenticator support for the
  // credProtect extension
  BOOL bRequireCredProtect;
} WEBAUTHN_CRED_PROTECT_EXTENSION_IN, *PWEBAUTHN_CRED_PROTECT_EXTENSION_IN;
typedef const WEBAUTHN_CRED_PROTECT_EXTENSION_IN*
    PCWEBAUTHN_CRED_PROTECT_EXTENSION_IN;

#define WEBAUTHN_EXTENSIONS_IDENTIFIER_CRED_PROTECT L"credProtect"
// Below type definitions is for WEBAUTHN_EXTENSIONS_IDENTIFIER_CRED_PROTECT
// MakeCredential Input Type:   WEBAUTHN_CRED_PROTECT_EXTENSION_IN.
//      - pvExtension must point to a WEBAUTHN_CRED_PROTECT_EXTENSION_IN struct
//      - cbExtension will contain the
//      sizeof(WEBAUTHN_CRED_PROTECT_EXTENSION_IN).
// MakeCredential Output Type:  DWORD.
//      - pvExtension will point to a DWORD with one of the above
//      WEBAUTHN_USER_VERIFICATION_* values
//        if credential was successfully created with CRED_PROTECT.
//      - cbExtension will contain the sizeof(DWORD).
// GetAssertion Input Type:     Not Supported
// GetAssertion Output Type:    Not Supported

//+------------------------------------------------------------------------------------------
// Information about Extensions.
//-------------------------------------------------------------------------------------------
typedef struct _WEBAUTHN_EXTENSION {
    LPCWSTR pwszExtensionIdentifier;
    DWORD cbExtension;
    PVOID pvExtension;
} WEBAUTHN_EXTENSION, *PWEBAUTHN_EXTENSION;
typedef const WEBAUTHN_EXTENSION *PCWEBAUTHN_EXTENSION;

typedef struct _WEBAUTHN_EXTENSIONS {
    DWORD cExtensions;
    _Field_size_(cExtensions)
    PWEBAUTHN_EXTENSION pExtensions;
} WEBAUTHN_EXTENSIONS, *PWEBAUTHN_EXTENSIONS;
typedef const WEBAUTHN_EXTENSIONS *PCWEBAUTHN_EXTENSIONS;

//+------------------------------------------------------------------------------------------
// Options.
//-------------------------------------------------------------------------------------------

#define WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY                               0
#define WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM                          1
#define WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM                    2
#define WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM_U2F_V2             3

#define WEBAUTHN_USER_VERIFICATION_REQUIREMENT_ANY                          0
#define WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED                     1
#define WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED                    2
#define WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED                  3

#define WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_ANY                      0
#define WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE                     1
#define WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_INDIRECT                 2
#define WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT                   3

#define WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_1            1
#define WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_2            2
#define WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_3            3
#define WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_CURRENT_VERSION      WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_3

typedef struct _WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS {
    // Version of this structure, to allow for modifications in the future.
    DWORD dwVersion;

    // Time that the operation is expected to complete within.
    // This is used as guidance, and can be overridden by the platform.
    DWORD dwTimeoutMilliseconds;

    // Credentials used for exclusion.
    WEBAUTHN_CREDENTIALS CredentialList;

    // Optional extensions to parse when performing the operation.
    WEBAUTHN_EXTENSIONS Extensions;

    // Optional. Platform vs Cross-Platform Authenticators.
    DWORD dwAuthenticatorAttachment;

    // Optional. Require key to be resident or not. Defaulting to FALSE;
    BOOL bRequireResidentKey;

    // User Verification Requirement.
    DWORD dwUserVerificationRequirement;

    // Attestation Conveyance Preference.
    DWORD dwAttestationConveyancePreference;

    // Reserved for future Use
    DWORD dwFlags;

    //
    // The following fields have been added in WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_2
    //

    // Cancellation Id - Optional - See WebAuthNGetCancellationId
    GUID *pCancellationId;

    //
    // The following fields have been added in WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_3
    //

    // Exclude Credential List. If present, "CredentialList" will be ignored.
    PWEBAUTHN_CREDENTIAL_LIST pExcludeCredentialList;

} WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS, *PWEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS;
typedef const WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS *PCWEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS;


#define WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_1          1
#define WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_2          2
#define WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_3          3
#define WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_4          4
#define WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_CURRENT_VERSION    WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_4

typedef struct _WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS {
    // Version of this structure, to allow for modifications in the future.
    DWORD dwVersion;

    // Time that the operation is expected to complete within.
    // This is used as guidance, and can be overridden by the platform.
    DWORD dwTimeoutMilliseconds;

    // Allowed Credentials List.
    WEBAUTHN_CREDENTIALS CredentialList;

    // Optional extensions to parse when performing the operation.
    WEBAUTHN_EXTENSIONS Extensions;

    // Optional. Platform vs Cross-Platform Authenticators.
    DWORD dwAuthenticatorAttachment;

    // User Verification Requirement.
    DWORD dwUserVerificationRequirement;

    // Reserved for future Use
    DWORD dwFlags;

    //
    // The following fields have been added in WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_2
    //

    // Optional identifier for the U2F AppId. Converted to UTF8 before being hashed. Not lower cased.
    PCWSTR pwszU2fAppId;

    // If the following is non-NULL, then, set to TRUE if the above pwszU2fAppid was used instead of
    // PCWSTR pwszRpId;
    BOOL *pbU2fAppId;

    //
    // The following fields have been added in WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_3
    //

    // Cancellation Id - Optional - See WebAuthNGetCancellationId
    GUID *pCancellationId;

    //
    // The following fields have been added in WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_4
    //

    // Allow Credential List. If present, "CredentialList" will be ignored.
    PWEBAUTHN_CREDENTIAL_LIST pAllowCredentialList;

} WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS,  *PWEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS;
typedef const WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS  *PCWEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS;


//+------------------------------------------------------------------------------------------
// Attestation Info.
//
//-------------------------------------------------------------------------------------------
#define WEBAUTHN_ATTESTATION_DECODE_NONE                                0
#define WEBAUTHN_ATTESTATION_DECODE_COMMON                              1
// WEBAUTHN_ATTESTATION_DECODE_COMMON supports format types
//  L"packed"
//  L"fido-u2f"

#define WEBAUTHN_ATTESTATION_VER_TPM_2_0   L"2.0"

typedef struct _WEBAUTHN_X5C {
    // Length of X.509 encoded certificate
    DWORD cbData;
    // X.509 encoded certificate bytes
    _Field_size_bytes_(cbData)
    PBYTE pbData;
} WEBAUTHN_X5C, *PWEBAUTHN_X5C;

// Supports either Self or Full Basic Attestation

// Note, new fields will be added to the following data structure to
// support additional attestation format types, such as, TPM.
// When fields are added, the dwVersion will be incremented.
//
// Therefore, your code must make the following check:
//  "if (dwVersion >= WEBAUTHN_COMMON_ATTESTATION_CURRENT_VERSION)"

#define WEBAUTHN_COMMON_ATTESTATION_CURRENT_VERSION                     1

typedef struct _WEBAUTHN_COMMON_ATTESTATION {
    // Version of this structure, to allow for modifications in the future.
    DWORD dwVersion;

    // Hash and Padding Algorithm
    //
    // The following won't be set for "fido-u2f" which assumes "ES256".
    PCWSTR pwszAlg;
    LONG lAlg;      // COSE algorithm

    // Signature that was generated for this attestation.
    DWORD cbSignature;
    _Field_size_bytes_(cbSignature)
    PBYTE pbSignature;

    // Following is set for Full Basic Attestation. If not, set then, this is Self Attestation.
    // Array of X.509 DER encoded certificates. The first certificate is the signer, leaf certificate.
    DWORD cX5c;
    _Field_size_(cX5c)
    PWEBAUTHN_X5C pX5c;

    // Following are also set for tpm
    PCWSTR pwszVer; // L"2.0"
    DWORD cbCertInfo;
    _Field_size_bytes_(cbCertInfo)
    PBYTE pbCertInfo;
    DWORD cbPubArea;
    _Field_size_bytes_(cbPubArea)
    PBYTE pbPubArea;
} WEBAUTHN_COMMON_ATTESTATION, *PWEBAUTHN_COMMON_ATTESTATION;
typedef const WEBAUTHN_COMMON_ATTESTATION *PCWEBAUTHN_COMMON_ATTESTATION;

#define WEBAUTHN_ATTESTATION_TYPE_PACKED                                L"packed"
#define WEBAUTHN_ATTESTATION_TYPE_U2F                                   L"fido-u2f"
#define WEBAUTHN_ATTESTATION_TYPE_TPM                                   L"tpm"
#define WEBAUTHN_ATTESTATION_TYPE_NONE                                  L"none"

#define WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_1               1
#define WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_2               2
#define WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_3               3
#define WEBAUTHN_CREDENTIAL_ATTESTATION_CURRENT_VERSION         WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_3

typedef struct _WEBAUTHN_CREDENTIAL_ATTESTATION {
    // Version of this structure, to allow for modifications in the future.
    DWORD dwVersion;

    // Attestation format type
    PCWSTR pwszFormatType;

    // Size of cbAuthenticatorData.
    DWORD cbAuthenticatorData;
    // Authenticator data that was created for this credential.
    _Field_size_bytes_(cbAuthenticatorData)
    PBYTE pbAuthenticatorData;

    // Size of CBOR encoded attestation information
    //0 => encoded as CBOR null value.
    DWORD cbAttestation;
    //Encoded CBOR attestation information
    _Field_size_bytes_(cbAttestation)
    PBYTE pbAttestation;

    DWORD dwAttestationDecodeType;
    // Following depends on the dwAttestationDecodeType
    //  WEBAUTHN_ATTESTATION_DECODE_NONE
    //      NULL - not able to decode the CBOR attestation information
    //  WEBAUTHN_ATTESTATION_DECODE_COMMON
    //      PWEBAUTHN_COMMON_ATTESTATION;
    PVOID pvAttestationDecode;

    // The CBOR encoded Attestation Object to be returned to the RP.
    DWORD cbAttestationObject;
    _Field_size_bytes_(cbAttestationObject)
    PBYTE pbAttestationObject;

    // The CredentialId bytes extracted from the Authenticator Data.
    // Used by Edge to return to the RP.
    DWORD cbCredentialId;
    _Field_size_bytes_(cbCredentialId)
    PBYTE pbCredentialId;

    //
    // Following fields have been added in WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_2
    //

    WEBAUTHN_EXTENSIONS Extensions;

    //
    // Following fields have been added in WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_3
    //

    // One of the WEBAUTHN_CTAP_TRANSPORT_* bits will be set corresponding to
    // the transport that was used.
    DWORD dwUsedTransport;

} WEBAUTHN_CREDENTIAL_ATTESTATION, *PWEBAUTHN_CREDENTIAL_ATTESTATION;
typedef const WEBAUTHN_CREDENTIAL_ATTESTATION *PCWEBAUTHN_CREDENTIAL_ATTESTATION;


//+------------------------------------------------------------------------------------------
// authenticatorGetAssertion output.
//-------------------------------------------------------------------------------------------

#define WEBAUTHN_ASSERTION_CURRENT_VERSION                              1

typedef struct _WEBAUTHN_ASSERTION {
    // Version of this structure, to allow for modifications in the future.
    DWORD dwVersion;

    // Size of cbAuthenticatorData.
    DWORD cbAuthenticatorData;
    // Authenticator data that was created for this assertion.
    _Field_size_bytes_(cbAuthenticatorData)
    PBYTE pbAuthenticatorData;

    // Size of pbSignature.
    DWORD cbSignature;
    // Signature that was generated for this assertion.
    _Field_size_bytes_(cbSignature)
    PBYTE pbSignature;

    // Credential that was used for this assertion.
    WEBAUTHN_CREDENTIAL Credential;

    // Size of User Id
    DWORD cbUserId;
    // UserId
    _Field_size_bytes_(cbUserId)
    PBYTE pbUserId;
} WEBAUTHN_ASSERTION, *PWEBAUTHN_ASSERTION;
typedef const WEBAUTHN_ASSERTION *PCWEBAUTHN_ASSERTION;

//+------------------------------------------------------------------------------------------
// APIs.
//-------------------------------------------------------------------------------------------

DWORD
WINAPI
WebAuthNGetApiVersionNumber();

HRESULT
WINAPI
WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable(
    _Out_ BOOL *pbIsUserVerifyingPlatformAuthenticatorAvailable);


HRESULT
WINAPI
WebAuthNAuthenticatorMakeCredential(
    _In_        HWND                                                hWnd,
    _In_        PCWEBAUTHN_RP_ENTITY_INFORMATION                    pRpInformation,
    _In_        PCWEBAUTHN_USER_ENTITY_INFORMATION                  pUserInformation,
    _In_        PCWEBAUTHN_COSE_CREDENTIAL_PARAMETERS               pPubKeyCredParams,
    _In_        PCWEBAUTHN_CLIENT_DATA                              pWebAuthNClientData,
    _In_opt_    PCWEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS    pWebAuthNMakeCredentialOptions,
    _Outptr_result_maybenull_ PWEBAUTHN_CREDENTIAL_ATTESTATION      *ppWebAuthNCredentialAttestation);


HRESULT
WINAPI
WebAuthNAuthenticatorGetAssertion(
    _In_        HWND                                                hWnd,
    _In_        LPCWSTR                                             pwszRpId,
    _In_        PCWEBAUTHN_CLIENT_DATA                              pWebAuthNClientData,
    _In_opt_    PCWEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS      pWebAuthNGetAssertionOptions,
    _Outptr_result_maybenull_ PWEBAUTHN_ASSERTION                   *ppWebAuthNAssertion);

void
WINAPI
WebAuthNFreeCredentialAttestation(
    _In_opt_ PWEBAUTHN_CREDENTIAL_ATTESTATION pWebAuthNCredentialAttestation);

void
WINAPI
WebAuthNFreeAssertion(
    _In_ PWEBAUTHN_ASSERTION pWebAuthNAssertion);

HRESULT
WINAPI
WebAuthNGetCancellationId(
    _Out_ GUID* pCancellationId);

HRESULT
WINAPI
WebAuthNCancelCurrentOperation(
    _In_ const GUID* pCancellationId);

//
// Returns the following Error Names:
//  L"Success"              - S_OK
//  L"InvalidStateError"    - NTE_EXISTS
//  L"ConstraintError"      - HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED),
//                            NTE_NOT_SUPPORTED,
//                            NTE_TOKEN_KEYSET_STORAGE_FULL
//  L"NotSupportedError"    - NTE_INVALID_PARAMETER
//  L"NotAllowedError"      - NTE_DEVICE_NOT_FOUND,
//                            NTE_NOT_FOUND,
//                            HRESULT_FROM_WIN32(ERROR_CANCELLED),
//                            NTE_USER_CANCELLED,
//                            HRESULT_FROM_WIN32(ERROR_TIMEOUT)
//  L"UnknownError"         - All other hr values
//
PCWSTR
WINAPI
WebAuthNGetErrorName(
    _In_ HRESULT hr);

HRESULT
WINAPI
WebAuthNGetW3CExceptionDOMError(
    _In_ HRESULT hr);


#ifdef __cplusplus
}       // Balance extern "C" above
#endif

#endif // WINAPI_FAMILY_PARTITION
#pragma endregion

#endif  // __WEBAUTHN_H_
