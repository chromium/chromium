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
    debug, AuthLevel, Authentication, DirtyFlag, PINState, ParsedState, Reauth, RequestError,
    PUB_KEY,
};
use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec;
use alloc::vec::Vec;
use cbor::{cbor, MapKey, MapKeyRef, MapLookupKey, Value};
use chromesync::pb::webauthn_credential_specifics::EncryptedData;
use chromesync::pb::WebauthnCredentialSpecifics;
use core::ops::Deref;
use crypto::EcdsaKeyPair;
use prost::Message;

map_keys! {
    CLAIMED_PIN, CLAIMED_PIN_KEY = "claimed_pin",
    CLIENT_DATA_JSON, CLIENT_DATA_JSON_KEY = "client_data_json",
    COSE_ALGORITHM, COSE_ALGORITHM_KEY = "alg",
    ENCRYPTED, ENCRYPTED_KEY = "encrypted",
    PIN_CLAIM_KEY, PIN_CLAIM_KEY_KEY = "pin_claim_key",
    PIN_GENERATION, PIN_GENERATION_KEY = "pin_generation",
    PIN_HASH, PIN_HASH_KEY = "pin_hash",
    PROTOBUF, PROTOBUF_KEY = "protobuf",
    PUB_KEY_CRED_PARAMS, PUB_KEY_CRED_PARAMS_KEY = "pubKeyCredParams",
    RP_ID, RP_ID_KEY = "rpId",
    SECRET, SECRET_KEY = "secret",
    VERSION, VERSION_KEY = "version",
    WEBAUTHN_REQUEST, WEBAUTHN_REQUEST_KEY = "request",
    WRAPPED_PIN_DATA, WRAPPED_PIN_DATA_KEY = "wrapped_pin_data",
    WRAPPED_SECRET, WRAPPED_SECRET_KEY = "wrapped_secret",
}

// The encrypted part of a WebauthnCredentialSpecifics sync entity is encrypted
// with AES-GCM and can come in two forms. These are the AAD inputs to AES-GCM
// that ensure domain separation.
const PRIVATE_KEY_FIELD_AAD: &[u8] = b"";
const ENCRYPTED_FIELD_AAD: &[u8] = b"WebauthnCredentialSpecifics.Encrypted";
pub(crate) const PIN_CLAIM_AAD: &[u8] = b"PIN claim";

// The "purpose" value of a security domain secret. Used when the client
// presents a wrapped secret that will be used as such.
pub(crate) const KEY_PURPOSE_SECURITY_DOMAIN_SECRET: &str = "security domain secret";

// The HKDF "info" parameter used when deriving a PIN data key from a security
// domain secret.
pub(crate) const KEY_PURPOSE_PIN_DATA_KEY: &[u8] =
    b"KeychainApplicationKey:chrome:GPM PIN data wrapping key";

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
const MAX_PIN_ATTEMPTS: i64 = 3;

fn key(k: &str) -> MapKey {
    MapKey::String(String::from(k))
}

enum SourceOfSecret {
    Wrapped,
    Direct,
}

/// Get the security domain secret for a client's request, either because it's
/// wrapped, or because the client provided it directly.
fn get_secret_from_request(
    state: &mut DirtyFlag<ParsedState>,
    request: &BTreeMap<MapKey, Value>,
    device_id: &[u8],
) -> Result<([u8; 32], SourceOfSecret), RequestError> {
    let (secret, source) =
        if let Some(Value::Bytestring(wrapped_secret)) = request.get(WRAPPED_SECRET_KEY) {
            if request.get(SECRET_KEY).is_some() {
                return debug("both wrapped and unwrapped secret provided");
            } else {
                (
                    state.unwrap(device_id, wrapped_secret, KEY_PURPOSE_SECURITY_DOMAIN_SECRET)?,
                    SourceOfSecret::Wrapped,
                )
            }
        } else if let Some(Value::Bytestring(secret)) = request.get(SECRET_KEY) {
            (secret.to_vec(), SourceOfSecret::Direct)
        } else {
            return debug("must provide secret or wrapped secret");
        };
    let secret =
        secret.as_slice().try_into().map_err(|_| RequestError::Debug("wrong length secret"))?;
    Ok((secret, source))
}

pub(crate) fn do_assert(
    auth: &Authentication,
    state: &mut DirtyFlag<ParsedState>,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    let Authentication::Device(device_id, auth_level, _) = auth else {
        return debug("device identity required");
    };
    let Some(Value::Bytestring(proto_bytes)) = request.get(PROTOBUF_KEY) else {
        return debug("protobuf required");
    };
    let Some(Value::String(client_data_json)) = request.get(CLIENT_DATA_JSON_KEY) else {
        return debug("clientDataJSON required");
    };
    let client_data_json_hash = crypto::sha256(client_data_json.as_bytes());
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
    let Some(ref user_id) = proto.user_id else {
        return debug("protobuf is missing user ID");
    };

    let entity_secrets = entity_secrets_from_proto(&security_domain_secret, &proto)?;

    let pin_verified =
        maybe_validate_pin_from_request(&request, state, device_id, &security_domain_secret)?;
    let user_verification = matches!(auth_level, AuthLevel::UserVerification)
        || pin_verified
        // If the client provided the security domain secret itself, then it could have
        // done the signing itself too. Thus this is sufficient to claim UV.
        || matches!(secret_source, SourceOfSecret::Direct);

    let flags = [FLAG_BACKUP_ELIGIBLE
        | FLAG_BACKED_UP
        | FLAG_USER_PRESENT
        | if user_verification { FLAG_USER_VERIFIED } else { 0 }];
    let authenticator_data = [rp_id_hash.as_slice(), &flags, ZERO_SIGNATURE_COUNTER].concat();
    let signed_data = [&authenticator_data, client_data_json_hash.as_slice()].concat();
    let signature = entity_secrets
        .primary_key
        .sign(&signed_data)
        .map_err(|_| RequestError::Debug("signing failed"))?;

    // https://w3c.github.io/webauthn/#dictdef-authenticatorassertionresponsejson
    let assertion_response_json = BTreeMap::<MapKey, Value>::from([
        (key("clientDataJSON"), Value::String(client_data_json.clone())),
        (key("authenticatorData"), Value::from(authenticator_data)),
        (key("signature"), Value::from(signature.as_ref())),
        (key("userHandle"), Value::from(user_id.to_vec())),
    ]);
    let response = BTreeMap::from([(key("response"), Value::Map(assertion_response_json))]);

    Ok(Value::Map(response))
}

pub(crate) fn do_create(
    auth: &Authentication,
    state: &mut DirtyFlag<ParsedState>,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    let Authentication::Device(device_id, _, _) = auth else {
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

    let mut hmac_secret = vec![0u8; 32];
    crypto::rand_bytes(hmac_secret.as_mut_slice());
    let pb = chromesync::pb::webauthn_credential_specifics::Encrypted {
        private_key: Some(pkcs8.as_ref().to_vec()),
        hmac_secret: Some(hmac_secret),
        cred_blob: None,
        large_blob: None,
        large_blob_uncompressed_size: None,
    };
    let ciphertext = encrypt(&security_domain_secret, pb.encode_to_vec(), ENCRYPTED_FIELD_AAD)?;

    Ok(Value::Map(BTreeMap::from([
        (MapKey::String(String::from(ENCRYPTED)), Value::from(ciphertext)),
        (MapKey::String(String::from(PUB_KEY)), Value::from(pub_key.as_ref().to_vec())),
    ])))
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
    // hmac_secret is generated, but never used because PRF extension
    // support hasn't been added yet.
    #[allow(dead_code)]
    hmac_secret: Option<Vec<u8>>,
    // These fields are not yet implemented but are contained in the protobuf
    // definition.
    // cred_blob: Option<Vec<u8>>,
    // large_blob: Option<(Vec<u8>, u64)>,
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
            Ok(EntitySecrets { primary_key, hmac_secret: None })
        }
        EncryptedData::Encrypted(ciphertext) => {
            let plaintext = decrypt(ciphertext, security_domain_secret, ENCRYPTED_FIELD_AAD)?;
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

/// A representation of the PIN data after unwrapping with the
/// security domain secret.
#[derive(PartialEq, Debug)]
pub(crate) struct PinData {
    /// The hash of the PIN. Usually hashed with scrypt, but this code only
    /// deals with hashes and so isn't affected by the choice of algorithm so
    /// long as the output is 256 bits long.
    pub pin_hash: [u8; 32],
    /// The generation number. Starts at zero and is incremented for each
    /// PIN change.
    pub generation: i64,
    /// An AES-256-GCM key used to encrypt claimed PIN hashes.
    pub claim_key: [u8; 32],
}

impl TryFrom<Vec<u8>> for PinData {
    type Error = ();

    /// Parse a `PinData` from CBOR bytes.
    fn try_from(pin_data: Vec<u8>) -> Result<PinData, Self::Error> {
        let parsed = cbor::parse(pin_data).map_err(|_| ())?;
        let Value::Map(map) = parsed else {
            return Err(());
        };
        let Some(Value::Bytestring(pin_hash)) = map.get(&MapKey::Int(1)) else {
            return Err(());
        };
        let Some(Value::Int(generation)) = map.get(&MapKey::Int(2)) else {
            return Err(());
        };
        let Some(Value::Bytestring(claim_key)) = map.get(&MapKey::Int(3)) else {
            return Err(());
        };
        Ok(PinData {
            pin_hash: pin_hash.as_ref().try_into().map_err(|_| ())?,
            generation: *generation,
            claim_key: claim_key.as_ref().try_into().map_err(|_| ())?,
        })
    }
}

impl PinData {
    pub fn to_bytes(&self) -> Vec<u8> {
        cbor!({
            1: (&self.pin_hash),
            2: (self.generation),
            3: (&self.claim_key),
        })
        .to_bytes()
    }
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
    let PINState { attempts, generation_high_water } = state.get_pin_state(device_id)?;
    if attempts >= MAX_PIN_ATTEMPTS {
        return Err(RequestError::PINLocked);
    }

    let pin_data = decrypt_pin_data(wrapped_pin_data, security_domain_secret)?;
    let Ok(pin_data): Result<PinData, ()> = pin_data.try_into() else {
        return debug("invalid PIN data");
    };

    if pin_data.generation < generation_high_water {
        return Err(RequestError::PINOutdated);
    }

    let claimed_pin_hash = open_aes_256_gcm(&pin_data.claim_key, claim, PIN_CLAIM_AAD)
        .ok_or(RequestError::Debug("failed to decrypt PIN claim"))?;
    if !constant_time_compare(&claimed_pin_hash, &pin_data.pin_hash) {
        state
            .get_mut()
            .set_pin_state(device_id, PINState { attempts: attempts + 1, generation_high_water })?;
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
    if attempts > 0 || pin_data.generation > generation_high_water {
        state.get_mut_for_minor_change().set_pin_state(
            device_id,
            PINState {
                attempts: 0,
                generation_high_water: core::cmp::max(pin_data.generation, generation_high_water),
            },
        )?;
    }

    Ok(())
}

fn open_aes_256_gcm(key: &[u8; 32], nonce_and_ciphertext: &[u8], aad: &[u8]) -> Option<Vec<u8>> {
    if nonce_and_ciphertext.len() < crypto::NONCE_LEN {
        return None;
    }
    let (nonce, ciphertext) = nonce_and_ciphertext.split_at(crypto::NONCE_LEN);
    // unwrap: the length is correct because it just came from `split_at`.
    let nonce: [u8; crypto::NONCE_LEN] = nonce.try_into().unwrap();
    crypto::aes_256_gcm_open_in_place(key, &nonce, aad, ciphertext.to_vec()).ok()
}

/// Decrypt the PIN data, i.e. the data that is stored as metadata in the PIN
/// virtual member of the security domain.
fn decrypt_pin_data(
    wrapped_pin_data: &[u8],
    security_domain_secret: &[u8],
) -> Result<Vec<u8>, RequestError> {
    let mut pin_data_key = [0u8; 32];
    // unwrap: this only fails if the output is too long, but the output length
    // is fixed at 32.
    crypto::hkdf_sha256(security_domain_secret, &[], KEY_PURPOSE_PIN_DATA_KEY, &mut pin_data_key)
        .unwrap();
    open_aes_256_gcm(&pin_data_key, wrapped_pin_data, &[])
        .ok_or(RequestError::Debug("PIN data decryption failed"))
}

pub(crate) fn encrypt_pin_data(
    mut serialized_pin_data: Vec<u8>,
    security_domain_secret: &[u8],
) -> Vec<u8> {
    let mut pin_data_key = [0u8; 32];
    // unwrap: this only fails if the output is too long, but the output length
    // is fixed at 32.
    crypto::hkdf_sha256(security_domain_secret, &[], KEY_PURPOSE_PIN_DATA_KEY, &mut pin_data_key)
        .unwrap();

    let mut nonce = [0u8; crypto::NONCE_LEN];
    crypto::rand_bytes(&mut nonce);
    crypto::aes_256_gcm_seal_in_place(&pin_data_key, &nonce, &[], &mut serialized_pin_data);
    [nonce.as_ref(), &serialized_pin_data].concat()
}

pub(crate) fn do_wrap_pin(
    auth: &Authentication,
    state: &mut DirtyFlag<ParsedState>,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    // Either UV or reauth is required to perform this command.
    let device_id = match auth {
        Authentication::Device(device_id, AuthLevel::UserVerification, _) => device_id,
        Authentication::Device(device_id, _, Reauth::Done) => device_id,
        _ => return debug("not authenticated"),
    };
    let Some(Value::Bytestring(pin_hash)) = request.get(PIN_HASH_KEY) else {
        return debug("pin_hash required");
    };
    let Some(Value::Int(generation)) = request.get(PIN_GENERATION_KEY) else {
        return debug("pin_generation required");
    };
    let Some(Value::Bytestring(claim_key)) = request.get(PIN_CLAIM_KEY_KEY) else {
        return debug("pin_claim_key required");
    };
    let Some(Value::Bytestring(wrapped_secret)) = request.get(WRAPPED_SECRET_KEY) else {
        return debug("wrapped secret required");
    };
    let security_domain_secret =
        state.unwrap(device_id, wrapped_secret, KEY_PURPOSE_SECURITY_DOMAIN_SECRET)?;

    let pin_data = PinData {
        pin_hash: pin_hash
            .as_ref()
            .try_into()
            .map_err(|_| RequestError::Debug("incorrect length PIN hash"))?,
        generation: *generation,
        claim_key: claim_key
            .as_ref()
            .try_into()
            .map_err(|_| RequestError::Debug("incorrect length claim key"))?,
    };
    Ok(Value::from(encrypt_pin_data(pin_data.to_bytes(), &security_domain_secret)))
}

#[cfg(test)]
pub mod tests {
    use super::*;
    use crate::tests::{
        PROTOBUF2_BYTES, PROTOBUF_BYTES, SAMPLE_SECURITY_DOMAIN_SECRET,
        WEBAUTHN_SECRETS_ENCRYPTION_KEY,
    };

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
        let pkcs8 =
            decrypt(&ciphertext, SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(), &[]).unwrap();

        assert!(EcdsaKeyPair::from_pkcs8(&pkcs8).is_ok());

        assert!(
            decrypt(&[0u8; 8], SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(), &[]).is_err()
        );
    }

    #[test]
    fn test_entity_secrets_from_proto() {
        let protobuf1: &WebauthnCredentialSpecifics = &PROTOBUF;
        let protobuf2: &WebauthnCredentialSpecifics = &PROTOBUF2;

        for (n, proto) in [protobuf1, protobuf2].iter().enumerate() {
            let result =
                entity_secrets_from_proto(SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(), proto);
            assert!(result.is_ok(), "{:?}", proto);
            let result = result.unwrap();

            let should_have_hmac_secret = n == 1;
            assert_eq!(matches!(result.hmac_secret, Some(_)), should_have_hmac_secret);
        }
    }

    #[test]
    fn test_encrypt() {
        let plaintext = b"hello";
        let ciphertext =
            encrypt(SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(), plaintext.to_vec(), &[])
                .unwrap();

        let plaintext2 =
            decrypt(&ciphertext, SAMPLE_SECURITY_DOMAIN_SECRET.try_into().unwrap(), &[]).unwrap();
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
            Test { a: b"", b: b"", expected: true },
            Test { a: b"a", b: b"", expected: false },
            Test { a: b"", b: b"b", expected: false },
            Test { a: b"a", b: b"b", expected: false },
            Test { a: b"a", b: b"a", expected: true },
            Test { a: b"abcde", b: b"abcde", expected: true },
            Test { a: b"abcdf", b: b"abcde", expected: false },
        ];

        for (i, test) in tests.iter().enumerate() {
            if constant_time_compare(test.a, test.b) != test.expected {
                panic!("failed at #{}", i)
            }
        }
    }

    #[test]
    fn test_pin_data() {
        let pin_data = PinData { pin_hash: [1u8; 32], generation: 42, claim_key: [2u8; 32] };
        let pin_data2: PinData = pin_data.to_bytes().try_into().unwrap();
        assert_eq!(pin_data, pin_data2);

        let security_domain_secret = [3u8; 32];
        let encrypted = encrypt_pin_data(pin_data.to_bytes(), &security_domain_secret);
        let decrypted =
            decrypt_pin_data(&encrypted, &security_domain_secret).unwrap().try_into().unwrap();
        assert_eq!(pin_data, decrypted);
    }

    // Integration tests of this code are done in Chromium, which builds this
    // code and runs it against the client code to ensure that, e.g.,
    // assertions work end-to-end.
}
