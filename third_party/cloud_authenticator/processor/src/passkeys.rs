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

use super::{
    debug, get_secret_from_request, open_aes_256_gcm, AuthLevel, Authentication, DirtyFlag,
    OneTimeUV, PINState, ParsedState, Reauth, RequestError, SourceOfSecret, COUNTER_ID_KEY,
    KEY_PURPOSE_SECURITY_DOMAIN_SECRET, PUB_KEY, VAULT_HANDLE_WITHOUT_TYPE_KEY,
    WRAPPED_PIN_DATA_KEY, WRAPPED_SECRET_KEY,
};
use crate::pin::VaultCohortDetails;
use crate::{pin, MetricsUpdate, CERT_XML_SERIAL_NUMBER_KEY, COHORT_PUBLIC_KEY_KEY};
use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use base64::Engine;
use cbor::{MapKey, MapKeyRef, MapLookupKey, Value};
use chromesync::pb::webauthn_credential_specifics::EncryptedData;
use chromesync::pb::WebauthnCredentialSpecifics;
use core::ops::Deref;
use crypto::EcdsaKeyPair;
use prost::Message;

map_keys! {
    CLAIMED_PIN, CLAIMED_PIN_KEY = "claimed_pin",
    CLIENT_DATA_JSON, CLIENT_DATA_JSON_KEY = "client_data_json",
    CLIENT_DATA_JSON_HASH, CLIENT_DATA_JSON_HASH_KEY = "client_data_json_hash",
    COSE_ALGORITHM, COSE_ALGORITHM_KEY = "alg",
    ENCRYPTED, ENCRYPTED_KEY = "encrypted",
    EVAL, EVAL_KEY = "eval",
    EVAL_BY_CREDENTIAL, EVAL_BY_CREDENTIAL_KEY = "evalByCredential",
    EXTENSIONS, EXTENSIONS_KEY = "extensions",
    FIRST, FIRST_KEY = "first",
    LARGE_BLOB, LARGE_BLOB_KEY = "largeBlob",
    LARGE_BLOB_READ, LARGE_BLOB_READ_KEY = "read",
    LARGE_BLOB_WRITE, LARGE_BLOB_WRITE_KEY = "write",
    LARGE_BLOB_DATA, LARGE_BLOB_DATA_KEY = "largeBlobData",
    LARGE_BLOB_SIZE,   LARGE_BLOB_SIZE_KEY   = "largeBlobSize",
    LARGE_BLOB_SUPPORTED, LARGE_BLOB_SUPPORTED_KEY = "largeBlobSupported",
    LARGE_BLOB_WRITTEN, LARGE_BLOB_WRITTEN_KEY   = "largeBlobWritten",
    PIN_CLAIM_KEY, PIN_CLAIM_KEY_KEY = "pin_claim_key",
    PIN_HASH, PIN_HASH_KEY = "pin_hash",
    PRF, PRF_KEY = "prf",
    PROTOBUF, PROTOBUF_KEY = "protobuf",
    PUB_KEY_CRED_PARAMS, PUB_KEY_CRED_PARAMS_KEY = "pubKeyCredParams",
    RP_ID, RP_ID_KEY = "rpId",
    SECOND, SECOND_KEY = "second",
    VERSION, VERSION_KEY = "version",
    WEBAUTHN_REQUEST, WEBAUTHN_REQUEST_KEY = "request",
}

// The encrypted part of a WebauthnCredentialSpecifics sync entity is encrypted
// with AES-GCM and can come in two forms. These are the AAD inputs to AES-GCM
// that ensure domain separation.
const PRIVATE_KEY_FIELD_AAD: &[u8] = b"";
const ENCRYPTED_FIELD_AAD: &[u8] = b"WebauthnCredentialSpecifics.Encrypted";
pub(crate) const PIN_CLAIM_AAD: &[u8] = b"PIN claim";

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

// The number of incorrect PIN attempts before further PIN attempts will be
// denied.
const MAX_PIN_ATTEMPTS: i64 = 5;

fn key(k: &str) -> MapKey {
    MapKey::String(String::from(k))
}

/// Returns a passkey assertion.
/// This function can take either a `client_data_json` or a
/// `client_data_json_hash`. If both are passed, `client_data_json_hash` is
/// ignored. Callers should prefer passing `client_data_json_hash`.
///
/// If a client passes `client_data_json`, the enclave will return it as part of
/// the assertion response. Otherwise, clients are expected to fill it in.
pub(crate) fn do_assert(
    metrics: &mut MetricsUpdate,
    auth: &Authentication,
    state: &mut DirtyFlag<ParsedState>,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    let Authentication::Device(device_id, auth_level, one_time_uv, _) = auth else {
        return debug("device identity required");
    };
    let Some(Value::Bytestring(proto_bytes)) = request.get(PROTOBUF_KEY) else {
        return debug("protobuf required");
    };
    let client_data_json = match request.get(CLIENT_DATA_JSON_KEY) {
        Some(Value::String(client_data_json)) => Some(client_data_json),
        Some(_) => return debug("clientDataJson is not a string"),
        None => None,
    };
    let client_data_json_hash = if let Some(client_data_json) = client_data_json {
        crypto::sha256(client_data_json.as_bytes())
    } else {
        match request.get(CLIENT_DATA_JSON_HASH_KEY) {
            Some(Value::Bytestring(client_data_json_hash)) => {
                client_data_json_hash.to_vec().try_into().map_err(|_| {
                    RequestError::Debug("clientDataJsonHash does not match expected length")
                })?
            }
            Some(_) => return debug("clientDataJsonHash is not a bytestring"),
            None => return debug("either clientDataJson or clientDataJsonHash are required"),
        }
    };
    let Some(Value::Map(webauthn_request)) = request.get(WEBAUTHN_REQUEST_KEY) else {
        return debug("WebAuthn request required");
    };
    let Some(Value::String(rp_id)) = webauthn_request.get(RP_ID_KEY) else {
        return debug("rpId required");
    };
    let rp_id_hash = crypto::sha256(rp_id.as_bytes());
    let (security_domain_secret, secret_source) =
        get_secret_from_request(state, &request, device_id)?;
    let proto = WebauthnCredentialSpecifics::decode(proto_bytes.deref())
        .map_err(|_| RequestError::Debug("failed to decode protobuf"))?;
    let Some(ref credential_id) = proto.credential_id else {
        return debug("protobuf is missing credential ID");
    };
    let Some(ref user_id) = proto.user_id else {
        return debug("protobuf is missing user ID");
    };

    let mut entity_secrets = entity_secrets_from_proto(&security_domain_secret, &proto)?;

    let pin_verified =
        maybe_validate_pin_from_request(&request, state, device_id, &security_domain_secret)?;
    let user_verification = matches!(auth_level, AuthLevel::UserVerification)
        || matches!(auth_level, AuthLevel::SoftwareUserVerification)
        || pin_verified
        // If the client provided the security domain secret itself, then it could have
        // done the signing itself too. Thus this is sufficient to claim UV.
        || matches!(secret_source, SourceOfSecret::Direct)
        // A client can also nominate to get one free UV when registering their
        // UV key, which is also sufficient for an assertion.
        || matches!(one_time_uv, OneTimeUV::Consumed);

    let flags = [FLAG_BACKUP_ELIGIBLE
        | FLAG_BACKED_UP
        | FLAG_USER_PRESENT
        | if user_verification {
            FLAG_USER_VERIFIED
        } else {
            0
        }];
    let authenticator_data = [rp_id_hash.as_slice(), &flags, ZERO_SIGNATURE_COUNTER].concat();
    let signed_data = [&authenticator_data, client_data_json_hash.as_slice()].concat();
    let signature = entity_secrets
        .primary_key
        .sign(&signed_data)
        .map_err(|_| RequestError::Debug("signing failed"))?;

    // https://w3c.github.io/webauthn/#dictdef-authenticatorassertionresponsejson
    let mut assertion_response_json = BTreeMap::<MapKey, Value>::from([
        (key("authenticatorData"), Value::from(authenticator_data)),
        (key("signature"), Value::from(signature.as_ref())),
        (key("userHandle"), Value::from(user_id.to_vec())),
    ]);
    if let Some(client_data_json) = client_data_json {
        assertion_response_json.insert(
            key("clientDataJSON"),
            Value::String(client_data_json.clone()),
        );
    }
    let mut response = BTreeMap::from([(key("response"), Value::Map(assertion_response_json))]);

    if let Some(prf_result) = handle_prf(
        webauthn_request,
        &entity_secrets.hmac_secret,
        Some(credential_id.as_ref()),
    )? {
        response.insert(key(PRF), prf_result);
    }
    if let Some(large_blob_out) = handle_large_blob(
        webauthn_request,
        &mut entity_secrets,
        &security_domain_secret,
    )? {
        response.insert(key(LARGE_BLOB), large_blob_out);
    }
    metrics.passkeys_assert += 1;
    Ok(Value::Map(response))
}

pub(crate) fn do_create(
    metrics: &mut MetricsUpdate,
    auth: &Authentication,
    state: &mut DirtyFlag<ParsedState>,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    let Authentication::Device(device_id, _, _, _) = auth else {
        return debug("device identity required");
    };
    let (security_domain_secret, _) = get_secret_from_request(state, &request, device_id)?;
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
    // Creating a credential doesn't sign anything, so the return value here
    // isn't used. But an incorrect PIN will still cause the request to fail
    // so that the client can check whether it was correct.
    maybe_validate_pin_from_request(&request, state, device_id, &security_domain_secret)?;

    let pkcs8 = EcdsaKeyPair::generate_pkcs8();
    let key = EcdsaKeyPair::from_pkcs8(pkcs8.as_ref())
        .map_err(|_| RequestError::Debug("failed to parse private key"))?;
    let pub_key = key.public_key();

    let mut hmac_secret = [0u8; 32];
    crypto::rand_bytes(&mut hmac_secret);
    let pb = chromesync::pb::webauthn_credential_specifics::Encrypted {
        private_key: Some(pkcs8.as_ref().to_vec()),
        hmac_secret: Some(hmac_secret.to_vec()),
        cred_blob: None,
        large_blob: None,
        large_blob_uncompressed_size: None,
    };
    let ciphertext = encrypt(
        &security_domain_secret,
        pb.encode_to_vec(),
        ENCRYPTED_FIELD_AAD,
    )?;

    let mut result = BTreeMap::from([
        (
            MapKey::String(String::from(ENCRYPTED)),
            Value::from(ciphertext),
        ),
        (
            MapKey::String(String::from(PUB_KEY)),
            Value::from(pub_key.as_ref().to_vec()),
        ),
    ]);
    if let Some(prf_result) = handle_prf(webauthn_request, &hmac_secret, None)? {
        result.insert(MapKey::String(String::from(PRF)), prf_result);
    }
    result.insert(
        MapKey::String(String::from(LARGE_BLOB_SUPPORTED)),
        Value::Boolean(true),
    );
    metrics.passkeys_create += 1;
    Ok(Value::Map(result))
}

/// Attempt to verify a claimed PIN from `request`. Returns true if the PIN
/// verified correctly, false if there was no PIN claim, and an error if
/// the PIN claim was invalid for any reason.
fn maybe_validate_pin_from_request(
    request: &BTreeMap<MapKey, Value>,
    state: &mut DirtyFlag<'_, ParsedState>,
    device_id: &[u8],
    security_domain_secret: &[u8; 32],
) -> Result<bool, RequestError> {
    if let Some(Value::Bytestring(wrapped_pin_data)) = request.get(WRAPPED_PIN_DATA_KEY) {
        let Some(Value::Bytestring(claimed_pin)) = request.get(CLAIMED_PIN_KEY) else {
            return debug("claimed PIN required");
        };
        validate_pin(
            state,
            device_id,
            security_domain_secret.as_ref(),
            claimed_pin,
            wrapped_pin_data,
        )?;
        Ok(true)
    } else {
        Ok(false)
    }
}

/// Contains the secrets from a specific passkey Sync entity.
struct EntitySecrets {
    primary_key: EcdsaKeyPair,
    hmac_secret: [u8; 32],
    // The encrypted part of the WebauthnCredentialSpecifics protobuf.
    // This is needed to update the large blob data.
    encrypted: Option<chromesync::pb::webauthn_credential_specifics::Encrypted>,
    // This field is not yet implemented but is contained in the protobuf
    // definition.
    // cred_blob: Option<Vec<u8>>,
}

/// Given a list of wrapped secrets and the wrapping key for this device,
/// returns the entity secrets from a protobuf and the security domain secret
/// used.
fn entity_secrets_from_proto(
    security_domain_secret: &[u8; 32],
    proto: &WebauthnCredentialSpecifics,
) -> Result<EntitySecrets, RequestError> {
    let Some(encrypted_data) = &proto.encrypted_data else {
        return debug("sync entity missing encrypted data");
    };
    match encrypted_data {
        EncryptedData::PrivateKey(ciphertext) => {
            let plaintext = decrypt(ciphertext, security_domain_secret, PRIVATE_KEY_FIELD_AAD)?;
            let primary_key = EcdsaKeyPair::from_pkcs8(&plaintext)
                .map_err(|_| RequestError::Debug("PKCS#8 parse failed"))?;
            let hmac_secret = derive_hmac_secret_from_private_key(&plaintext);
            Ok(EntitySecrets {
                primary_key,
                hmac_secret,
                encrypted: None,
            })
        }
        EncryptedData::Encrypted(ciphertext) => {
            let plaintext = decrypt(ciphertext, security_domain_secret, ENCRYPTED_FIELD_AAD)?;
            let encrypted = chromesync::pb::webauthn_credential_specifics::Encrypted::decode(
                plaintext.as_slice(),
            )
            .map_err(|_| RequestError::Debug("failed to decode encrypted data"))?;
            let Some(ref private_key_bytes) = encrypted.private_key else {
                return debug("missing private key");
            };
            let primary_key = EcdsaKeyPair::from_pkcs8(private_key_bytes.as_slice())
                .map_err(|_| RequestError::Debug("PKCS#8 parse failed"))?;
            let hmac_secret = encrypted
                .hmac_secret
                .as_ref()
                .and_then(|vec| <[u8; 32]>::try_from(vec.as_slice()).ok())
                .unwrap_or_else(|| {
                    derive_hmac_secret_from_private_key(private_key_bytes.as_slice())
                });
            Ok(EntitySecrets {
                primary_key,
                hmac_secret,
                encrypted: Some(encrypted),
            })
        }
    }
}

/// Calculate an HMAC secret from a private key.
///
/// We want to support the PRF extension for credentials that were generated
/// without PRF support being requested at creation time. To do so we derive an
/// HMAC secret from the encoded private key.
fn derive_hmac_secret_from_private_key(pkcs8_bytes: &[u8]) -> [u8; 32] {
    let mut ret = [0u8; 32];
    // unwrap: only fails if the output length is too long, but we know that
    // `ret` is 32 bytes.
    crypto::hkdf_sha256(pkcs8_bytes, &[], b"derived PRF HMAC secret", &mut ret).unwrap();
    ret
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

/// Decrypt an entity secret using a security domain secret.
///
/// Different entity secrets will have different "additional authenticated
/// data" values to ensure that ciphertexts are interpreted in their correct
/// context.
fn decrypt(
    ciphertext: &[u8],
    security_domain_secret: &[u8; 32],
    aad: &[u8],
) -> Result<Vec<u8>, RequestError> {
    let mut encryption_key = [0u8; 32];
    security_domain_secret_to_encryption_key(security_domain_secret, &mut encryption_key);
    open_aes_256_gcm(&encryption_key, ciphertext, aad)
        .ok_or(RequestError::Debug("decryption failed"))
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

/// Compares two secrets for equality.
fn constant_time_compare(a: &[u8], b: &[u8]) -> bool {
    if a.len() != b.len() {
        return false;
    }

    // There is a crate called `constant_time_eq` that uses lots of magic to
    // try and stop Rust from optimising away the constant-time compare. I
    // can't judge whether that magic is sufficient nor, if it is today,
    // whether it'll continue to be in the future. Since this operation isn't
    // performance-sensitive in the context that it's used here, this code
    // does a randomised hash of the secret values and compares the digests.
    // That's very slow, but it's safe.
    let mut rand_bytes = [0; 32];
    crypto::rand_bytes(&mut rand_bytes);

    crypto::sha256_two_part(&rand_bytes, a) == crypto::sha256_two_part(&rand_bytes, b)
}

/// Validate a claimed PIN, returning an error if it's incorrect.
fn validate_pin(
    state: &mut DirtyFlag<ParsedState>,
    device_id: &[u8],
    security_domain_secret: &[u8],
    claim: &[u8],
    wrapped_pin_data: &[u8],
) -> Result<(), RequestError> {
    let PINState { attempts } = state.get_pin_state(device_id)?;
    if attempts >= MAX_PIN_ATTEMPTS {
        return Err(RequestError::PINLocked);
    }

    let pin_data = pin::Data::from_wrapped(wrapped_pin_data, security_domain_secret)?;
    let claimed_pin_hash = open_aes_256_gcm(&pin_data.claim_key, claim, PIN_CLAIM_AAD)
        .ok_or(RequestError::Debug("failed to decrypt PIN claim"))?;
    if !constant_time_compare(&claimed_pin_hash, &pin_data.pin_hash) {
        state.get_mut().set_pin_state(
            device_id,
            PINState {
                attempts: attempts + 1,
            },
        )?;
        return Err(RequestError::IncorrectPIN);
    }

    // These is an availability / security tradeoff here. In the case of a correct
    // PIN guess, we don't serialise the updated state. Thus, if a user's machine
    // has malware that can grab the needed secrets it can try to rush more than
    // `MAX_PIN_ATTEMPTS` guesses at the enclave service. If the storage system is
    // having trouble it's possible that more than `MAX_PIN_ATTEMPTS` will be
    // considered. However, if we serialize these state updates then every time the
    // storage system has a glitch, nobody will be able to validate assertions with
    // a PIN. Since the attack requires malware on the client machine, where the
    // user could probably be phished for their PIN much more effectively than
    // trying to exploit a concurrency issue, we err on the side of availability.
    if attempts > 0 {
        state
            .get_mut_for_minor_change()
            .set_pin_state(device_id, PINState { attempts: 0 })?;
    }

    Ok(())
}

/// This is intended to be removed in favour of recovery_key_store.rs'
/// do_wrap_pin_and_secret.
pub(crate) fn do_wrap_pin(
    metrics: &mut MetricsUpdate,
    auth: &Authentication,
    state: &mut DirtyFlag<ParsedState>,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    // Reauth is required to perform this command.
    let device_id = match auth {
        Authentication::Device(device_id, _, _, Reauth::Done) => device_id,
        _ => return debug("PIN change needs reauth via RAPT token"),
    };
    let Some(Value::Bytestring(pin_hash)) = request.get(PIN_HASH_KEY) else {
        return debug("pin_hash required");
    };
    let Some(Value::Bytestring(claim_key)) = request.get(PIN_CLAIM_KEY_KEY) else {
        return debug("pin_claim_key required");
    };
    let Some(Value::Bytestring(wrapped_secret)) = request.get(WRAPPED_SECRET_KEY) else {
        return debug("wrapped secret required");
    };
    let Some(Value::Bytestring(counter_id)) = request.get(COUNTER_ID_KEY) else {
        return debug("counter ID required");
    };
    let Some(Value::Bytestring(vault_handle_without_type)) =
        request.get(VAULT_HANDLE_WITHOUT_TYPE_KEY)
    else {
        return debug("vault handle required");
    };
    let cert_xml_serial_number = request.get(CERT_XML_SERIAL_NUMBER_KEY);
    let cohort_public_key = request.get(COHORT_PUBLIC_KEY_KEY);
    // If either the serial number or the cohort public is present, the other is
    // required.
    if cert_xml_serial_number.is_none() && cohort_public_key.is_some() {
        return debug("cert xml serial number required");
    }
    if cert_xml_serial_number.is_some() && cohort_public_key.is_none() {
        return debug("cohort public key required");
    }
    let cert_xml_serial_number = match cert_xml_serial_number {
        None => None,
        Some(Value::Int(cert_xml_serial_number)) => Some(cert_xml_serial_number),
        _ => return debug("cert xml serial number has wrong format"),
    };
    let cohort_public_key = match cohort_public_key {
        None => None,
        Some(Value::Bytestring(cohort_public_key)) => Some(cohort_public_key),
        _ => return debug("cohort public key has wrong format"),
    };
    let vault_cohort_details = match (cert_xml_serial_number, cohort_public_key) {
        (Some(cert_xml_serial_number), Some(cohort_public_key)) => Some(VaultCohortDetails {
            cert_xml_serial_number: *cert_xml_serial_number,
            cohort_public_key: cohort_public_key.to_vec(),
        }),
        _ => None,
    };
    let security_domain_secret = state.unwrap(
        device_id,
        wrapped_secret,
        KEY_PURPOSE_SECURITY_DOMAIN_SECRET,
    )?;

    let pin_data = pin::Data {
        pin_hash: pin_hash
            .as_ref()
            .try_into()
            .map_err(|_| RequestError::Debug("incorrect length PIN hash"))?,
        claim_key: claim_key
            .as_ref()
            .try_into()
            .map_err(|_| RequestError::Debug("incorrect length claim key"))?,
        counter_id: counter_id
            .as_ref()
            .try_into()
            .map_err(|_| RequestError::Debug("incorrect length counter id"))?,
        vault_handle_without_type: vault_handle_without_type
            .as_ref()
            .try_into()
            .map_err(|_| RequestError::Debug("incorrect length vault handle"))?,
        vault_cohort_details,
    };
    metrics.passkeys_wrap_pin += 1;
    Ok(Value::from(pin_data.encrypt(&security_domain_secret)))
}

/// PRFValues mirrors `AuthenticationExtensionsPRFValues` from the WebAuthn
/// spec, although it contains either post-hashed values or HMAC outputs.
struct PRFValues {
    first: [u8; 32],
    second: Option<[u8; 32]>,
}

impl PRFValues {
    /// Treat a `PRFValues` as evaluation points and evaluate them for a given
    /// HMAC key.
    fn hmac(self, hmac_key: &[u8; 32]) -> Self {
        PRFValues {
            first: crypto::hmac_sha256(hmac_key, &self.first),
            second: self
                .second
                .map(|second| crypto::hmac_sha256(hmac_key, &second)),
        }
    }

    /// Convert to a CBOR structure.
    fn into_cbor(self) -> Value {
        let mut ret = BTreeMap::from([(key(FIRST), Value::from(&self.first))]);
        if let Some(second) = self.second {
            ret.insert(key(SECOND), Value::from(&second));
        }
        Value::Map(ret)
    }
}

impl TryFrom<&Value> for PRFValues {
    type Error = RequestError;

    /// Attempt to parse a PRFValues from a CBOR input that reflects a
    /// `AuthenticationExtensionsPRFValues` WebAuthn structure.
    fn try_from(v: &Value) -> Result<Self, Self::Error> {
        let Value::Map(prf) = v else {
            return debug("PRF value is not a map");
        };
        fn get_value(value: &Value) -> Result<[u8; 32], RequestError> {
            let Value::String(value) = value else {
                return debug("invalid PRF value");
            };
            let value = base64::engine::general_purpose::URL_SAFE_NO_PAD
                .decode(value)
                .map_err(|_| RequestError::Debug("invalid PRF base64url"))?;
            Ok(hash_prf_value(&value))
        }

        let first = get_value(
            prf.get(FIRST_KEY)
                .ok_or(RequestError::Debug("missing PRF first value"))?,
        )?;
        let second = prf.get(SECOND_KEY).map(get_value).transpose()?;

        Ok(PRFValues { first, second })
    }
}

/// Map WebAuthn-scoped PRF inputs to raw values.
///
/// The PRF inputs that a website is allowed to evaluate are limited to the
/// images of a hash function so that different parties can be given different
/// evaluation powers. See https://w3c.github.io/webauthn/#prf-extension
fn hash_prf_value(input: &[u8]) -> [u8; 32] {
    const PREFIX: &[u8] = b"WebAuthn PRF\x00";
    crypto::sha256_two_part(PREFIX, input)
}

/// Optionally handle the PRF extension from a request given an HMAC key. If the
/// request is an assertion then `credential_id` specifies the credential being
/// evaluated.
fn handle_prf(
    webauthn_request: &BTreeMap<MapKey, Value>,
    hmac_secret: &[u8; 32],
    credential_id: Option<&[u8]>,
) -> Result<Option<Value>, RequestError> {
    let Some(Value::Map(extensions)) = webauthn_request.get(EXTENSIONS_KEY) else {
        return Ok(None);
    };
    let Some(Value::Map(prf)) = extensions.get(PRF_KEY) else {
        return Ok(None);
    };
    if credential_id.is_none() && prf.is_empty() {
        return Ok(Some(Value::Boolean(true)));
    }
    Ok(prf_values_by_id(prf, credential_id)?
        .or(prf_default_values(prf)?)
        .map(|values| values.hmac(hmac_secret))
        .map(PRFValues::into_cbor))
}

/// Get the PRF values for a given credential ID, if any.
fn prf_values_by_id(
    prf: &BTreeMap<MapKey, Value>,
    credential_id: Option<&[u8]>,
) -> Result<Option<PRFValues>, RequestError> {
    let Some(credential_id) = credential_id else {
        return Ok(None);
    };
    let Some(Value::Map(by_credential)) = prf.get(EVAL_BY_CREDENTIAL_KEY) else {
        return Ok(None);
    };
    let base64url_credential_id =
        base64::engine::general_purpose::URL_SAFE_NO_PAD.encode(credential_id);
    let Some(values) = by_credential.get(&MapKey::String(base64url_credential_id)) else {
        return Ok(None);
    };
    Ok(Some(values.try_into()?))
}

/// Get the default PRF values from the extension, if any.
fn prf_default_values(prf: &BTreeMap<MapKey, Value>) -> Result<Option<PRFValues>, RequestError> {
    let Some(eval) = prf.get(EVAL_KEY) else {
        return Ok(None);
    };
    Ok(Some(eval.try_into()?))
}

/// Handle the large blob extension request.
/// The large blob extension allows a Relying Party to associate opaque binary data
/// with a credential. See https://w3c.github.io/webauthn/#sctn-large-blob-extension
///
/// Returns:
/// - `Ok(None)` if the request doesn't contain a large blob extension.
/// - `Ok(Some(Value))` if the request contains a large blob extension.
///   - The returned `Value` is a map containing the large blob response fields.
/// - `Err(RequestError)` if the request is invalid.
fn handle_large_blob(
    webauthn_req: &BTreeMap<MapKey, Value>,
    entity_secrets: &mut EntitySecrets,
    security_domain_secret: &[u8; 32],
) -> Result<Option<Value>, RequestError> {
    let Some(Value::Map(exts)) = webauthn_req.get(EXTENSIONS_KEY) else {
        return Ok(None);
    };
    let Some(Value::Map(large_blob_req)) = exts.get(LARGE_BLOB_KEY) else {
        return Ok(None);
    };

    // Both read and write is disallowed.
    if large_blob_req.contains_key(LARGE_BLOB_READ_KEY)
        && large_blob_req.contains_key(LARGE_BLOB_WRITE_KEY)
    {
        return debug("invalid largeBlob request: read + write");
    }

    // Read path.
    if let Some(read_resp) = handle_read_large_blob(large_blob_req, entity_secrets) {
        return Ok(Some(read_resp));
    }

    // Write path.
    if let Some(write_resp) =
        handle_write_large_blob(large_blob_req, entity_secrets, security_domain_secret)?
    {
        return Ok(Some(write_resp));
    }

    // Any other shape is invalid.
    debug("invalid large blob request")
}

/// Helper function that handles large blob read.
/// It returns a `Value` containing the large blob data and its size if a read
/// was requested. Otherwise, it returns `None`.
fn handle_read_large_blob(
    large_blob_req: &BTreeMap<MapKey, Value>,
    entity_secrets: &EntitySecrets,
) -> Option<Value> {
    if !matches!(
        large_blob_req.get(LARGE_BLOB_READ_KEY),
        Some(Value::Boolean(true))
    ) {
        return None;
    }

    let (data, size_val) = match entity_secrets.encrypted.as_ref() {
        Some(enc) if enc.large_blob.is_some() && enc.large_blob_uncompressed_size.is_some() => {
            let blob = enc.large_blob.as_ref().unwrap();
            let size = enc.large_blob_uncompressed_size.unwrap();
            (Value::from(blob.as_slice()), Value::Int(size as i64))
        }
        _ => (Value::from(&[] as &[u8]), Value::Int(0_i64)),
    };

    Some(Value::Map(BTreeMap::from([
        (key(LARGE_BLOB_DATA), data),
        (key(LARGE_BLOB_SIZE), size_val),
    ])))
}

/// Helper function to write large blob data.
///
/// This function takes a large blob request, the entity secrets, and the
/// security domain secret. It updates the `entity_secrets` with the new
/// large blob data and returns a tuple containing:
/// - A `Value` representing the response to the large blob write request.
///
/// It returns `None` if the request does not contain a large blob write.
/// It returns an error if the new blob is larger than 8KiB.
fn handle_write_large_blob(
    large_blob_req: &BTreeMap<MapKey, Value>,
    entity_secrets: &mut EntitySecrets,
    security_domain_secret: &[u8; 32],
) -> Result<Option<Value>, RequestError> {
    let new_blob: Vec<u8> = match large_blob_req.get(LARGE_BLOB_WRITE_KEY) {
        Some(Value::String(s)) => base64::engine::general_purpose::URL_SAFE_NO_PAD
            .decode(s)
            .map_err(|_| RequestError::Debug("invalid base64 in largeBlob.write"))?,
        _ => return Ok(None),
    };

    const MAX_LARGE_BLOB_LEN: usize = 8 * 1024;
    if new_blob.len() > MAX_LARGE_BLOB_LEN {
        return Err(RequestError::Debug("large blob write > 8KiB"));
    }

    // The caller must tell what the uncompressed length of its data is.
    let Some(Value::Int(uncompressed_len)) = large_blob_req.get(LARGE_BLOB_SIZE_KEY) else {
        return debug("largeBlobSize required");
    };
    if *uncompressed_len < 0 {
        return debug("largeBlobSize must be non-negative");
    }

    let Some(enc) = entity_secrets.encrypted.as_mut() else {
        // Old style passkeys without an `encrypted` field do not support large
        // blobs.
        return Ok(Some(Value::Map(BTreeMap::from([(
            key(LARGE_BLOB_WRITTEN),
            Value::Boolean(false),
        )]))));
    };
    enc.large_blob = Some(new_blob.to_vec());
    enc.large_blob_uncompressed_size = Some(*uncompressed_len as u64);

    let ciphertext = encrypt(
        security_domain_secret,
        enc.encode_to_vec(),
        ENCRYPTED_FIELD_AAD,
    )?;

    let resp = Value::Map(BTreeMap::from([
        (key(LARGE_BLOB_WRITTEN), Value::Boolean(true)),
        (key(ENCRYPTED), Value::from(ciphertext)),
    ]));

    Ok(Some(resp))
}

#[cfg(test)]
pub mod tests {
    use super::*;
    use crate::recovery_key_store;
    use crate::tests::{
        PROTOBUF2_BYTES, PROTOBUF_BYTES, SAMPLE_SECURITY_DOMAIN_SECRET,
        WEBAUTHN_SECRETS_ENCRYPTION_KEY,
    };
    use alloc::vec;
    use cbor::cbor;

    lazy_static! {
        static ref PROTOBUF: WebauthnCredentialSpecifics =
            WebauthnCredentialSpecifics::decode(PROTOBUF_BYTES).unwrap();
        static ref PROTOBUF2: WebauthnCredentialSpecifics =
            WebauthnCredentialSpecifics::decode(PROTOBUF2_BYTES).unwrap();
    }

    #[test]
    fn test_decrypt() {
        let Some(EncryptedData::PrivateKey(ciphertext)) = &PROTOBUF.encrypted_data else {
            panic!("bad protobuf");
        };
        let pkcs8 = decrypt(
            &ciphertext,
            SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
            &[],
        )
        .unwrap();

        assert!(EcdsaKeyPair::from_pkcs8(&pkcs8).is_ok());

        assert!(decrypt(
            &[0u8; 8],
            SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
            &[]
        )
        .is_err());
    }

    #[test]
    fn test_entity_secrets_from_proto() {
        let protobuf1: &WebauthnCredentialSpecifics = &PROTOBUF;
        let protobuf2: &WebauthnCredentialSpecifics = &PROTOBUF2;

        for proto in [protobuf1, protobuf2] {
            let result =
                entity_secrets_from_proto(SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(), proto);
            assert!(result.is_ok(), "{:?}", proto);
        }
    }

    #[test]
    fn test_derived_hmac_secret() {
        // This HMAC secret is derived.
        let secrets =
            entity_secrets_from_proto(SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(), &PROTOBUF)
                .unwrap();
        assert_eq!(&secrets.hmac_secret, b"\x78\xbd\x3f\x1a\xbb\x66\x52\xe3\x2d\xc1\x50\x7d\x75\x83\x73\xdc\xeb\xa5\x8a\x17\x02\x9c\xe5\x12\x73\xee\x3f\x85\xd6\xc9\x2e\x21");

        // This protobuf has an explicit HMAC secret and so one must not be derived from
        // the private key.
        let secrets = entity_secrets_from_proto(
            SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
            &PROTOBUF2,
        )
        .unwrap();
        assert_eq!(&secrets.hmac_secret, b"\x08\xa2\xe8\x8e\xd3\x78\xbf\xcd\x82\x5f\x0b\x06\xde\xd5\x6d\x2d\x03\xa2\x47\xff\x34\xd0\x81\x40\x52\xec\x6d\xe5\x1a\x98\x22\x91");
    }

    #[test]
    fn test_encrypt() {
        let plaintext = b"hello";
        let ciphertext = encrypt(
            SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
            plaintext.to_vec(),
            &[],
        )
        .unwrap();

        let plaintext2 = decrypt(
            &ciphertext,
            SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
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

    #[test]
    fn test_constant_time_compare() {
        struct Test {
            a: &'static [u8],
            b: &'static [u8],
            expected: bool,
        }
        let tests: &[Test] = &[
            Test {
                a: b"",
                b: b"",
                expected: true,
            },
            Test {
                a: b"a",
                b: b"",
                expected: false,
            },
            Test {
                a: b"",
                b: b"b",
                expected: false,
            },
            Test {
                a: b"a",
                b: b"b",
                expected: false,
            },
            Test {
                a: b"a",
                b: b"a",
                expected: true,
            },
            Test {
                a: b"abcde",
                b: b"abcde",
                expected: true,
            },
            Test {
                a: b"abcdf",
                b: b"abcde",
                expected: false,
            },
        ];

        for (i, test) in tests.iter().enumerate() {
            if constant_time_compare(test.a, test.b) != test.expected {
                panic!("failed at #{}", i)
            }
        }
    }

    #[test]
    fn test_pin_data() {
        let pin_data = pin::Data {
            pin_hash: [1u8; 32],
            claim_key: [2u8; 32],
            counter_id: [3u8; recovery_key_store::COUNTER_ID_LEN],
            vault_handle_without_type: [4u8; recovery_key_store::VAULT_HANDLE_LEN - 1],
            vault_cohort_details: None,
        };
        let pin_data2: pin::Data = pin_data.to_bytes().try_into().unwrap();
        assert_eq!(pin_data, pin_data2);

        let security_domain_secret = [3u8; 32];
        let encrypted = pin_data.encrypt(&security_domain_secret);
        let decrypted = pin::Data::from_wrapped(&encrypted, &security_domain_secret).unwrap();
        assert_eq!(pin_data, decrypted);
    }

    #[test]
    fn test_pin_data_with_cohort_details() {
        let pin_data = pin::Data {
            pin_hash: [1u8; 32],
            claim_key: [2u8; 32],
            counter_id: [3u8; recovery_key_store::COUNTER_ID_LEN],
            vault_handle_without_type: [4u8; recovery_key_store::VAULT_HANDLE_LEN - 1],
            vault_cohort_details: Some(VaultCohortDetails {
                cert_xml_serial_number: 42,
                cohort_public_key: vec![1, 2, 3, 4],
            }),
        };
        let pin_data2: pin::Data = pin_data.to_bytes().try_into().unwrap();
        assert_eq!(pin_data, pin_data2);

        let security_domain_secret = [3u8; 32];
        let encrypted = pin_data.encrypt(&security_domain_secret);
        let decrypted = pin::Data::from_wrapped(&encrypted, &security_domain_secret).unwrap();
        assert_eq!(pin_data, decrypted);
    }

    // Helper to generate large blob requests for testing.
    fn make_req(large_blob_val: Value) -> BTreeMap<MapKey, Value> {
        let Value::Map(map) = cbor!({
            "extensions": {
            "largeBlob": large_blob_val
        }}) else {
            panic!("expected map");
        };
        map
    }

    // Helper to return a fresh EntitySecrets with empty large blob fields.
    // Used to test large blob handling without pre-existing blob data.
    fn empty_secrets() -> EntitySecrets {
        entity_secrets_from_proto(
            &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
            &PROTOBUF2,
        )
        .unwrap()
    }

    // Test writing a new large blob, decrypting and reading it back.
    #[test]
    fn test_large_blob_write_then_read() {
        let new_blob_content = b"new blob data".to_vec();
        let b64_blob = base64::engine::general_purpose::URL_SAFE_NO_PAD.encode(&new_blob_content);
        let uncompressed_size = new_blob_content.len() + 10;
        let webauthn_req_write = make_req(cbor!({
           "write": (b64_blob.as_str()),
           "largeBlobSize": (uncompressed_size as i64),
        }));

        let mut secrets_for_write = entity_secrets_from_proto(
            &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
            &PROTOBUF2,
        )
        .unwrap();

        let enc = secrets_for_write
            .encrypted
            .as_mut()
            .expect("encrypted should be present");
        enc.large_blob = None;
        enc.large_blob_uncompressed_size = None;

        let result = handle_large_blob(
            &webauthn_req_write,
            &mut secrets_for_write,
            &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
        )
        .unwrap();

        let response_val = result.expect("expected a response");
        let Value::Map(response_map) = response_val else {
            panic!("expected a map response");
        };
        assert_eq!(
            response_map.get(LARGE_BLOB_WRITTEN_KEY),
            Some(&Value::Boolean(true))
        );

        let Value::Bytestring(new_ciphertext_bs) =
            response_map.get(ENCRYPTED_KEY).expect("ciphertext present")
        else {
            panic!("encrypted not bytestring");
        };

        let plaintext = decrypt(
            &new_ciphertext_bs,
            &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
            ENCRYPTED_FIELD_AAD,
        )
        .unwrap();
        let encrypted_proto =
            chromesync::pb::webauthn_credential_specifics::Encrypted::decode(plaintext.as_slice())
                .unwrap();
        assert_eq!(encrypted_proto.large_blob.unwrap(), new_blob_content);
        assert_eq!(
            secrets_for_write
                .encrypted
                .as_ref()
                .and_then(|e| e.large_blob.as_ref()),
            Some(&new_blob_content)
        );
        assert_eq!(
            encrypted_proto.large_blob_uncompressed_size,
            Some(uncompressed_size as u64)
        );

        use chromesync::pb::webauthn_credential_specifics::EncryptedData;

        // Re-create a protobuf whose encrypted_data field is the new ciphertext.
        let mut proto_after_write = (*PROTOBUF2).clone();
        proto_after_write.encrypted_data =
            Some(EncryptedData::Encrypted(new_ciphertext_bs.to_vec().into()));

        let mut secrets_for_read = entity_secrets_from_proto(
            &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
            &proto_after_write,
        )
        .unwrap();

        let webauthn_req_read = make_req(cbor!({
            "read": true
        }));

        let read_resp = handle_large_blob(
            &webauthn_req_read,
            &mut secrets_for_read,
            &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
        )
        .unwrap()
        .expect("read response");

        let Value::Map(ref _read_map) = read_resp else {
            panic!("map");
        };
        assert_eq!(
            read_resp,
            cbor!({
                "largeBlobData": (new_blob_content.clone()),
                "largeBlobSize": (uncompressed_size as i64)
            })
        );
    }

    // Test write > 8 KiB.
    #[test]
    fn test_large_blob_write_too_large() {
        let huge_blob = vec![0u8; 8 * 1024 + 1];
        let huge_blob_b64 = base64::engine::general_purpose::URL_SAFE_NO_PAD.encode(&huge_blob);
        let req = make_req(cbor!({
            "write": (huge_blob_b64.as_str()),
            "largeBlobSize":  (huge_blob.len() as i64)
        }));
        assert!(matches!(
            handle_large_blob(&req,
                              &mut empty_secrets(),
                              &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap()),
            Err(RequestError::Debug(m)) if m.contains("8KiB")
        ));
    }

    // Test simultaneous read + write.
    #[test]
    fn test_large_blob_read_and_write() {
        let b64_blob = base64::engine::general_purpose::URL_SAFE_NO_PAD.encode(b"abc");
        let req = make_req(cbor!({
            "read":  (true),
            "write": (b64_blob.as_str()),
            "largeBlobSize":  (3)
        }));
        assert!(matches!(
            handle_large_blob(&req,
                              &mut empty_secrets(),
                              &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap()),
            Err(RequestError::Debug(m)) if m.contains("read + write")
        ));
    }

    // Test missing `largeBlobSize` on write.
    #[test]
    fn test_large_blob_write_missing_size() {
        let b64_blob = base64::engine::general_purpose::URL_SAFE_NO_PAD.encode(b"abc");
        let req = make_req(cbor!({
            "write": (b64_blob.as_str()),
        }));
        assert!(matches!(
            handle_large_blob(&req,
                              &mut empty_secrets(),
                              &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap()),
            Err(RequestError::Debug(m)) if m.contains("largeBlobSize required")
        ));
    }

    // Test negative `largeBlobSize`.
    #[test]
    fn test_large_blob_negative_size() {
        let b64_blob = base64::engine::general_purpose::URL_SAFE_NO_PAD.encode(b"abc");
        let req = make_req(cbor!({
            "write": (b64_blob.as_str()),
            "largeBlobSize":  (-1),
        }));
        assert!(matches!(
            handle_large_blob(&req,
                              &mut empty_secrets(),
                              &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap()),
            Err(RequestError::Debug(m)) if m.contains("non-negative")
        ));
    }

    // Test missing `largeBlob` field entirely.
    #[test]
    fn test_large_blob_extension_absent() {
        let req_no_large_blob: BTreeMap<MapKey, Value> = {
            let Value::Map(map) = cbor!({
                "extensions": {
                    // no 'largeBlob' field.
                }
            }) else {
                unreachable!();
            };
            map
        };

        let no_blob_result = handle_large_blob(
            &req_no_large_blob,
            &mut empty_secrets(),
            &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
        )
        .unwrap();

        assert!(
            no_blob_result.is_none(),
            "unexpected large blob handling when field is absent"
        );
    }

    // Test invalid base64 string.
    #[test]
    fn test_large_blob_invalid_base64() {
        let bad_b64 = "***not-b64***";
        let req = make_req(cbor!({
            "write": (bad_b64),
            "largeBlobSize": 0_i64
        }));

        assert!(matches!(
            handle_large_blob(
                &req,
                &mut empty_secrets(),
                &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap()
            ),
            Err(RequestError::Debug(m)) if m.contains("invalid base64 in largeBlob.write")
        ));
    }

    // Test attempting to write a large blob on an old style `private_key` proto
    // field.
    // Regression test for https://crbug.com/441240975.
    #[test]
    fn test_write_large_blob_on_old_credential_type() {
        let new_blob_content = b"new blob data".to_vec();
        let b64_blob = base64::engine::general_purpose::URL_SAFE_NO_PAD.encode(&new_blob_content);
        let uncompressed_size = new_blob_content.len() + 10;
        let webauthn_req_write = make_req(cbor!({
           "write": (b64_blob.as_str()),
           "largeBlobSize": (uncompressed_size as i64),
        }));

        let mut secrets_for_write = entity_secrets_from_proto(
            &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
            &PROTOBUF,
        )
        .unwrap();
        assert!(secrets_for_write.encrypted.is_none());

        let result = handle_large_blob(
            &webauthn_req_write,
            &mut secrets_for_write,
            &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
        )
        .unwrap();
        let response_val = result.expect("expected a response");
        let Value::Map(response_map) = response_val else {
            panic!("expected a map response");
        };
        assert_eq!(
            response_map.get(LARGE_BLOB_WRITTEN_KEY),
            Some(&Value::Boolean(false))
        );
    }

    // Test attempting to read a large blob on an old style `private_key` proto
    // field.
    // Regression test for https://crbug.com/441240975.
    #[test]
    fn test_read_large_blob_on_old_credential_type() {
        let webauthn_req_read = make_req(cbor!({
           "read": true
        }));
        let mut secrets_for_read = entity_secrets_from_proto(
            &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
            &PROTOBUF,
        )
        .unwrap();
        assert!(secrets_for_read.encrypted.is_none());

        let result = handle_large_blob(
            &webauthn_req_read,
            &mut secrets_for_read,
            &SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(),
        )
        .unwrap();

        let response_val = result.expect("expected a response");
        assert_eq!(
            response_val,
            cbor!({
                "largeBlobData": (&[] as &[u8]),
                "largeBlobSize": 0,
            })
        );
    }

    // Integration tests of this code are done in Chromium, which builds this
    // code and runs it against the client code to ensure that, e.g.,
    // assertions work end-to-end.
}
