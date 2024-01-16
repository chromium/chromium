// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Passkey-related functionality in the enclave.

extern crate bytes;
extern crate chromesync;
extern crate crypto;
extern crate prost;

use super::{debug, unwrap, Authentication, DirtyFlag, ParsedState, RequestError, PUB_KEY};
use alloc::vec;

use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use cbor::{MapKey, MapKeyRef, MapLookupKey, Value};
use chromesync::pb::webauthn_credential_specifics::EncryptedData;
use chromesync::pb::WebauthnCredentialSpecifics;
use core::ops::Deref;
use crypto::EcdsaKeyPair;
use prost::Message;

map_keys! {
    CLIENT_DATA_JSON, CLIENT_DATA_JSON_KEY = "client_data_json",
    COSE_ALGORITHM, COSE_ALGORITHM_KEY = "alg",
    ENCRYPTED, ENCRYPTED_KEY = "encrypted",
    PROTOBUF, PROTOBUF_KEY = "protobuf",
    PUB_KEY_CRED_PARAMS, PUB_KEY_CRED_PARAMS_KEY = "pubKeyCredParams",
    RP_ID, RP_ID_KEY = "rpId",
    USER_VERIFICATION, USER_VERIFICATION_KEY = "uv",
    VERSION, VERSION_KEY = "version",
    WEBAUTHN_REQUEST, WEBAUTHN_REQUEST_KEY = "request",
    WRAPPED_SECRET, WRAPPED_SECRET_KEY = "wrapped_secret",
    WRAPPED_SECRETS, WRAPPED_SECRETS_KEY = "wrapped_secrets",
}

// The encrypted part of a WebauthnCredentialSpecifics sync entity is encrypted
// with AES-GCM and can come in two forms. These are the AAD inputs to AES-GCM
// that ensure domain separation.
const PRIVATE_KEY_FIELD_AAD: &[u8] = b"";
const ENCRYPTED_FIELD_AAD: &[u8] = b"WebauthnCredentialSpecifics.Encrypted";

// The "purpose" value of a security domain secret. Used when the client
// presents a wrapped secret that will be used as such.
pub(crate) const KEY_PURPOSE_SECURITY_DOMAIN_SECRET: &str = "security domain secret";

// These constants are CTAP flags.
// See https://w3c.github.io/webauthn/#authdata-flags
const FLAG_USER_PRESENT: u8 = 1 << 0;
const FLAG_USER_VERIFIED: u8 = 1 << 2;
const FLAG_BACKUP_ELIGIBLE: u8 = 1 << 3;
const FLAG_BACKED_UP: u8 = 1 << 4;

// The signed authenticator data contains a four-byte signature counter. This
// is the special zero value that indicates that a counter is not supported.
// See https://w3c.github.io/webauthn/#authdata-signcount
const ZERO_SIGNATURE_COUNTER: &[u8; 4] = &[0u8; 4];

// https://www.w3.org/TR/webauthn-2/#sctn-alg-identifier
const COSE_ALGORITHM_ECDSA_P256_SHA256: i64 = -7;

fn key(k: &str) -> MapKey {
    MapKey::String(String::from(k))
}

fn b64url(bytes: &[u8]) -> Value {
    Value::String(base64url::base64url_encode(bytes))
}

pub(crate) fn do_assert(
    auth: &Authentication,
    state: &mut DirtyFlag<ParsedState>,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    let Authentication::Device(device_id, _) = auth else {
        return debug("device identity required");
    };
    let Some(Value::Bytestring(proto_bytes)) = request.get(PROTOBUF_KEY) else {
        return debug("protobuf required");
    };
    let Some(Value::String(client_data_json)) = request.get(CLIENT_DATA_JSON_KEY) else {
        return debug("clientDataJSON required");
    };
    let client_data_json_hash = crypto::sha256(client_data_json.as_bytes());
    let Some(Value::Boolean(user_verification)) = request.get(USER_VERIFICATION_KEY) else {
        return debug("uv flag required");
    };
    let Some(Value::Map(webauthn_request)) = request.get(WEBAUTHN_REQUEST_KEY) else {
        return debug("WebAuthn request required");
    };
    let Some(Value::Array(wrapped_secrets)) = request.get(WRAPPED_SECRETS_KEY) else {
        return debug("wrapped secrets required");
    };
    let wrapped_secrets: Option<Vec<Vec<u8>>> = wrapped_secrets
        .iter()
        .map(|value| match value {
            Value::Bytestring(wrapped) => Some(wrapped.to_vec()),
            _ => None,
        })
        .collect();
    let wrapped_secrets =
        wrapped_secrets.ok_or(RequestError::Debug("wrapped secrets contained non-bytestring"))?;
    let Some(Value::String(rp_id)) = webauthn_request.get(RP_ID_KEY) else {
        return debug("rpId required");
    };
    let rp_id_hash = crypto::sha256(rp_id.as_bytes());

    let proto = WebauthnCredentialSpecifics::decode(proto_bytes.deref())
        .map_err(|_| RequestError::Debug("failed to decode protobuf"))?;
    let Some(ref user_id) = proto.user_id else {
        return debug("protobuf is missing user ID");
    };

    let entity_secrets =
        entity_secrets_from_proto(state.wrapping_key(device_id)?, wrapped_secrets, &proto)?;

    // We may, in the future, want to limit UV requests to the UV key, if
    // a device has one registered.
    let flags = [FLAG_BACKUP_ELIGIBLE
        | FLAG_BACKED_UP
        | FLAG_USER_PRESENT
        | if *user_verification { FLAG_USER_VERIFIED } else { 0 }];
    let authenticator_data = [rp_id_hash.as_slice(), &flags, ZERO_SIGNATURE_COUNTER].concat();
    let signed_data = [&authenticator_data, client_data_json_hash.as_slice()].concat();
    let signature = entity_secrets
        .primary_key
        .sign(&signed_data)
        .map_err(|_| RequestError::Debug("signing failed"))?;

    // https://w3c.github.io/webauthn/#dictdef-authenticatorassertionresponsejson
    let assertion_response_json = BTreeMap::<MapKey, Value>::from([
        (key("clientDataJSON"), Value::String(client_data_json.clone())),
        (key("authenticatorData"), b64url(&authenticator_data)),
        (key("signature"), b64url(signature.as_ref())),
        (key("userHandle"), b64url(user_id)),
    ]);
    let response = BTreeMap::from([(key("response"), Value::Map(assertion_response_json))]);

    Ok(Value::Map(response))
}

pub(crate) fn do_create(
    auth: &Authentication,
    state: &mut DirtyFlag<ParsedState>,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    let Authentication::Device(device_id, _) = auth else {
        return debug("device identity required");
    };
    let Some(Value::Bytestring(wrapped_secret)) = request.get(WRAPPED_SECRET_KEY) else {
        return debug("wrapped secret required");
    };
    let secret = state.unwrap(device_id, wrapped_secret, KEY_PURPOSE_SECURITY_DOMAIN_SECRET)?;
    let security_domain_secret: &[u8; 32] =
        secret.as_slice().try_into().map_err(|_| RequestError::Debug("wrong length secret"))?;
    let Some(Value::Map(webauthn_request)) = request.get(WEBAUTHN_REQUEST_KEY) else {
        return debug("WebAuthn request required");
    };
    let Some(Value::Array(pub_key_cred_params)) = webauthn_request.get(PUB_KEY_CRED_PARAMS_KEY)
    else {
        return debug("missing pubKeyCredParams array");
    };
    let cose_algorithms: Result<Vec<i64>, RequestError> = pub_key_cred_params
        .iter()
        .map(|cred_param| match cred_param {
            Value::Map(i) => {
                let Some(Value::Int(alg)) = i.get(COSE_ALGORITHM_KEY) else {
                    return debug("missing algorithm");
                };
                Ok(*alg)
            }
            _ => debug("invalid algorithm type"),
        })
        .collect();
    // I can't get Rust to accept merging this "?" into the previous line.
    let cose_algorithms = cose_algorithms?;

    if !cose_algorithms.contains(&COSE_ALGORITHM_ECDSA_P256_SHA256) {
        return Err(RequestError::NoSupportedAlgorithm);
    }

    let pkcs8 = EcdsaKeyPair::generate_pkcs8();
    let key = EcdsaKeyPair::from_pkcs8(pkcs8.as_ref())
        .map_err(|_| RequestError::Debug("failed to parse private key"))?;
    let pub_key = key.public_key();

    let mut hmac_secret = vec![0u8; 32];
    crypto::rand_bytes(hmac_secret.as_mut_slice());
    let pb = chromesync::pb::webauthn_credential_specifics::Encrypted {
        private_key: Some(pkcs8.as_ref().to_vec()),
        hmac_secret: Some(hmac_secret),
        cred_blob: None,
        large_blob: None,
        large_blob_uncompressed_size: None,
    };
    let ciphertext = encrypt(security_domain_secret, pb.encode_to_vec(), ENCRYPTED_FIELD_AAD)?;

    Ok(Value::Map(BTreeMap::from([
        (MapKey::String(String::from(ENCRYPTED)), Value::from(ciphertext)),
        (MapKey::String(String::from(PUB_KEY)), Value::from(pub_key.as_ref().to_vec())),
    ])))
}

/// Contains the secrets from a specific passkey Sync entity.
struct EntitySecrets {
    primary_key: EcdsaKeyPair,
    // hmac_secret is generated, but never used because PRF extension
    // support hasn't been added yet.
    #[allow(dead_code)]
    hmac_secret: Option<Vec<u8>>,
    // These fields are not yet implemented but are contained in the protobuf
    // definition.
    // cred_blob: Option<Vec<u8>>,
    // large_blob: Option<(Vec<u8>, u64)>,
}

/// Get the entity secrets from a protobuf, given a list of wrapped secrets and
/// the wrapping key for this device.
fn entity_secrets_from_proto(
    wrapping_key: &[u8],
    wrapped_secrets: Vec<Vec<u8>>,
    proto: &WebauthnCredentialSpecifics,
) -> Result<EntitySecrets, RequestError> {
    let Some(encrypted_data) = &proto.encrypted_data else {
        return debug("sync entity missing encrypted data");
    };
    match encrypted_data {
        EncryptedData::PrivateKey(ciphertext) => {
            let plaintext = decrypt_entity_secret(
                wrapping_key,
                wrapped_secrets,
                ciphertext,
                PRIVATE_KEY_FIELD_AAD,
            )?;
            let primary_key = EcdsaKeyPair::from_pkcs8(&plaintext)
                .map_err(|_| RequestError::Debug("PKCS#8 parse failed"))?;
            Ok(EntitySecrets { primary_key, hmac_secret: None })
        }
        EncryptedData::Encrypted(ciphertext) => {
            let plaintext = decrypt_entity_secret(
                wrapping_key,
                wrapped_secrets,
                ciphertext,
                ENCRYPTED_FIELD_AAD,
            )?;
            let encrypted = chromesync::pb::webauthn_credential_specifics::Encrypted::decode(
                plaintext.as_slice(),
            )
            .map_err(|_| RequestError::Debug("failed to decode encrypted data"))?;
            let Some(private_key_bytes) = encrypted.private_key else {
                return debug("missing private key");
            };
            let primary_key = EcdsaKeyPair::from_pkcs8(&private_key_bytes)
                .map_err(|_| RequestError::Debug("PKCS#8 parse failed"))?;

            Ok(EntitySecrets { primary_key, hmac_secret: encrypted.hmac_secret })
        }
    }
}

/// Encrypt an entity secret using a security domain secret.
fn encrypt(
    security_domain_secret: &[u8; 32],
    mut plaintext: Vec<u8>,
    aad: &[u8],
) -> Result<Vec<u8>, RequestError> {
    let mut encryption_key = [0u8; 32];
    security_domain_secret_to_encryption_key(security_domain_secret, &mut encryption_key);

    let mut nonce_bytes = [0u8; 12];
    crypto::rand_bytes(&mut nonce_bytes);
    crypto::aes_256_gcm_seal_in_place(&encryption_key, &nonce_bytes, aad, &mut plaintext);

    Ok([nonce_bytes.as_slice(), &plaintext].concat())
}

/// Trial decrypt an entity secret given a list of possible wrapped secrets.
fn decrypt_entity_secret(
    wrapping_key: &[u8],
    wrapped_secrets: Vec<Vec<u8>>,
    ciphertext: &[u8],
    aad: &[u8],
) -> Result<Vec<u8>, RequestError> {
    for wrapped_secret in wrapped_secrets {
        let secret = unwrap(wrapping_key, &wrapped_secret, KEY_PURPOSE_SECURITY_DOMAIN_SECRET)?;
        let security_domain_secret: &[u8; 32] = secret
            .as_slice()
            .try_into()
            .map_err(|_| RequestError::Debug("wrapped secret is wrong length"))?;
        let result = decrypt_entity_secret_with_security_domain_secret(
            ciphertext,
            security_domain_secret,
            aad,
        );
        if result.is_ok() {
            return result;
        }
    }
    Err(RequestError::MissingSecrets)
}

/// Decrypt an entity secret using a security domain secret.
///
/// Different entity secrets will have different "additional authenticated
/// data" values to ensure that ciphertexts are interpreted in their correct
/// context.
fn decrypt_entity_secret_with_security_domain_secret(
    ciphertext: &[u8],
    security_domain_secret: &[u8; 32],
    aad: &[u8],
) -> Result<Vec<u8>, RequestError> {
    if ciphertext.len() < crypto::NONCE_LEN {
        return debug("ciphertext too short");
    }
    let (nonce, ciphertext) = ciphertext.split_at(crypto::NONCE_LEN);
    // unwrap: only fails if `nonce` is the wrong length, which is a constant.
    let nonce: [u8; crypto::NONCE_LEN] = nonce.try_into().unwrap();

    let mut encryption_key = [0u8; 32];
    security_domain_secret_to_encryption_key(security_domain_secret, &mut encryption_key);

    crypto::aes_256_gcm_open_in_place(&encryption_key, &nonce, aad, ciphertext.to_vec())
        .map_err(|_| RequestError::Debug("decryption failed"))
}

/// Derive the key used for encrypting entity secrets from the security domain
/// secret.
fn security_domain_secret_to_encryption_key(
    security_domain_secret: &[u8; 32],
    out_passkey_key: &mut [u8; 32],
) {
    // unwrap: only fails if the output length is too long, but we know that
    // `out_passkey_key` is 32 bytes.
    crypto::hkdf_sha256(
        security_domain_secret,
        &[],
        b"KeychainApplicationKey:gmscore_module:com.google.android.gms.fido",
        out_passkey_key,
    )
    .unwrap();
}

#[cfg(test)]
mod tests {
    extern crate bytes;
    extern crate hex;

    use super::*;
    use crate::tests::{
        PROTOBUF2_BYTES, PROTOBUF_BYTES, SAMPLE_SECURITY_DOMAIN_SECRET, SAMPLE_WRAPPED_SECRET,
        SAMPLE_WRAPPING_KEY, WEBAUTHN_SECRETS_ENCRYPTION_KEY,
    };
    use crate::wrap;

    lazy_static! {
        static ref PROTOBUF: WebauthnCredentialSpecifics =
            WebauthnCredentialSpecifics::decode(PROTOBUF_BYTES).unwrap();
        static ref PROTOBUF2: WebauthnCredentialSpecifics =
            WebauthnCredentialSpecifics::decode(PROTOBUF2_BYTES).unwrap();
    }

    #[test]
    fn test_decrypt_passkey_ciphertext_with_security_domain_secret() {
        let Some(EncryptedData::PrivateKey(ciphertext)) = &PROTOBUF.encrypted_data else {
            panic!("bad protobuf");
        };
        let pkcs8 = decrypt_entity_secret_with_security_domain_secret(
            &ciphertext,
            SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
            &[],
        )
        .unwrap();

        assert!(EcdsaKeyPair::from_pkcs8(&pkcs8).is_ok());

        assert!(
            decrypt_entity_secret_with_security_domain_secret(
                &[0u8; 8],
                SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
                &[],
            )
            .is_err()
        );
    }

    #[test]
    fn test_entity_secrets_from_proto() {
        let protobuf1: &WebauthnCredentialSpecifics = &PROTOBUF;
        let protobuf2: &WebauthnCredentialSpecifics = &PROTOBUF2;

        for (n, proto) in [protobuf1, protobuf2].iter().enumerate() {
            let result = entity_secrets_from_proto(
                SAMPLE_WRAPPING_KEY,
                vec![SAMPLE_WRAPPED_SECRET.to_vec()],
                proto,
            );
            assert!(result.is_ok(), "{:?}", proto);
            let result = result.unwrap();

            let should_have_hmac_secret = n == 1;
            assert_eq!(matches!(result.hmac_secret, Some(_)), should_have_hmac_secret);
        }
    }

    #[test]
    fn test_decrypt_entity_secret() {
        let Some(EncryptedData::PrivateKey(ciphertext)) = &PROTOBUF.encrypted_data else {
            panic!("bad protobuf");
        };

        // An empty list of secrets should fail to decrypt.
        assert_eq!(
            decrypt_entity_secret(SAMPLE_WRAPPING_KEY, Vec::new(), ciphertext, &[]),
            Err(RequestError::MissingSecrets)
        );
        // The correct wrapping secret should work.
        assert!(
            decrypt_entity_secret(
                SAMPLE_WRAPPING_KEY,
                vec![SAMPLE_WRAPPED_SECRET.to_vec()],
                ciphertext,
                &[]
            )
            .is_ok()
        );
        // ... but not with the wrong ciphertext
        assert!(
            decrypt_entity_secret(
                SAMPLE_WRAPPING_KEY,
                vec![SAMPLE_WRAPPED_SECRET.to_vec()],
                &[42u8; 50],
                &[]
            )
            .is_err()
        );
        // Putting incorrect wrapped keys around the correct key should still work.
        let incorrect1 = wrap(SAMPLE_WRAPPING_KEY, &[1u8; 32], KEY_PURPOSE_SECURITY_DOMAIN_SECRET);
        let incorrect2 = wrap(SAMPLE_WRAPPING_KEY, &[2u8; 32], KEY_PURPOSE_SECURITY_DOMAIN_SECRET);
        assert!(
            decrypt_entity_secret(
                SAMPLE_WRAPPING_KEY,
                vec![incorrect1, SAMPLE_WRAPPED_SECRET.to_vec(), incorrect2],
                ciphertext,
                &[]
            )
            .is_ok()
        );
        // But an wrapped secret that doesn't decrypt will stop the process.
        let incorrect3 = wrap(SAMPLE_WRAPPING_KEY, &[1u8; 32], "wrong purpose");
        assert!(matches!(
            decrypt_entity_secret(
                SAMPLE_WRAPPING_KEY,
                vec![incorrect3, SAMPLE_WRAPPED_SECRET.to_vec()],
                ciphertext,
                &[]
            ),
            Err(RequestError::Debug(_))
        ));
        // ... and so will one that's the wrong length.
        let incorrect4 = wrap(SAMPLE_WRAPPING_KEY, &[1u8; 1], KEY_PURPOSE_SECURITY_DOMAIN_SECRET);
        assert!(matches!(
            decrypt_entity_secret(
                SAMPLE_WRAPPING_KEY,
                vec![incorrect4, SAMPLE_WRAPPED_SECRET.to_vec()],
                ciphertext,
                &[]
            ),
            Err(RequestError::Debug(_))
        ));
    }

    #[test]
    fn test_encrypt() {
        let plaintext = b"hello";
        let ciphertext =
            encrypt(SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(), plaintext.to_vec(), &[])
                .unwrap();

        let plaintext2 = decrypt_entity_secret(
            SAMPLE_WRAPPING_KEY,
            vec![SAMPLE_WRAPPED_SECRET.to_vec()],
            &ciphertext,
            &[],
        )
        .unwrap();
        assert_eq!(plaintext, plaintext2.as_slice());
    }

    #[test]
    fn test_security_domain_secret_to_encryption_key() {
        let mut calculated = [0u8; 32];
        security_domain_secret_to_encryption_key(
            SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
            &mut calculated,
        );
        assert_eq!(WEBAUTHN_SECRETS_ENCRYPTION_KEY, &calculated);
    }
}
