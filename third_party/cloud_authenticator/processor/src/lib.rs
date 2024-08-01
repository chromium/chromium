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

//! `processor` updates the account's state in response to a request.
//!
//! The account's state is passed as a pair of bytestrings (in [`StateData`]).
//! The `transparent` data must be at least integrity protected. The
//! `confidential` data must have confidentiality and integrity.
//!
//! The caller must ensure that the account's state is updated atomically. So
//! if the account state was changed by something else while this code was also
//! processing it then the output of this code must be discarded. This code
//! doesn't have side effects so it's reasonable to try apply the commands again
//! to resolve this.
//!
//! The client's request is an array of one of more commands. Commands are
//! applied in sequence until either all have been applied, or else a command
//! fails. The output of a request is an array of results, one from each
//! applied command plus, optionally, one error result from the failed command.
//!
//! Partially successful requests are valid and the account's state should still
//! be updated.
//!
//! Not all commands must be authenticated. It's assumed that some level of
//! authentication (i.e. access to the account) has already been confirmed.

#![no_std]
#![forbid(unsafe_code)]

#[cfg(test)]
#[macro_use]
extern crate lazy_static;

extern crate alloc;
extern crate base64;
extern crate cbor;
extern crate crypto;

mod der;
#[macro_use]
mod macros;
mod passkeys;
mod pin;
mod recovery_key_store;
mod spki;

// When building for fuzzing, these functions are re-exported so the fuzzer can
// call them.
#[cfg(fuzzing)]
pub use recovery_key_store::fuzzing::{x509_parse, xml_parse};
#[cfg(fuzzing)]
pub use spki::parse as spki_parse;

use alloc::collections::{btree_map, BTreeMap};
use alloc::string::String;
use alloc::vec::Vec;
use bytes::Bytes;
use cbor::{cbor, MapKey, MapKeyRef, MapLookupKey, Value};

/// Holds the account state to which the commands are applied.
///
/// If an account has never been used before, its state is [`Initial`].
/// Otherwise there is state stored for the account.
#[derive(Clone)]
pub enum ClientState {
    Initial,
    Explicit(StateData),
}

/// A `StateUpdate` is produced by successfully processing a client message.
pub enum StateUpdate {
    /// This update contains important changes. The response should not be sent
    /// to the client until it has been accepted by the storage system.
    Major(StateData),
    /// This update contains minor changes. The response can be sent to the
    /// client immediately and it's ok if this update is lost.
    Minor(StateData),
    /// There is no change to the state.
    None,
}

/// Holds stored account state.
///
/// The `transparent` data must be at least integrity protected. The
/// `confidential` data must have confidentiality and integrity. These
/// properties must be provided within the enclave, but outside of this module.
#[derive(Clone)]
pub struct StateData {
    pub transparent: Vec<u8>,
    pub confidential: Vec<u8>,
}

/// Represents fatal processing errors.
///
/// Individual commands within an request can fail without being fatal of the
/// whole request. But some errors doom the whole request and are represented
/// here.
#[derive(Debug)]
pub enum Error {
    // These errors indicate an internal error with the enclave system because
    // account state should never be corrupt.
    TransparentDataCBORError(cbor::Error),
    ConfidentialDataCBORError(cbor::Error),

    // This error is worth distinguishing to the client because it indicates
    // that it is not recognised and thus no authenticated request will ever
    // be accepted from it.
    UnknownClient,

    // A large number of errors are not distinguished and are just strings.
    // The only exception is an error while parsing the client's request, since
    // we can include the detail of the CBOR parse error, which may be useful
    // for debugging.
    Str(&'static str),
    CBORError(cbor::Error),
}

/// ExternalContext contains context about a client request that comes from
/// server-side components outside of this enclave.
#[derive(Clone)]
pub struct ExternalContext {
    /// The current time, in milliseconds since the UNIX epoch.
    pub current_time_epoch_millis: i64,
    /// An opaque identifier for the device that the client's request came from.
    /// This will be recorded in the enclave's "transparent" state for this
    /// device.
    pub client_device_identifier: Vec<u8>,
    /// A signal that this client performed reauthentication very recently. This
    /// can authorize some actions.
    pub is_reauthenticated: bool,
}

// These constants are map keys used within the CBOR. For each map key constant
// there is also a `*_KEY` constant that can be used to lookup that key in a
// `BTreeMap<MapKey, Value>`. (Looking up enum keys in a map without allocating
// is a little awkward in Rust.)

const OK: &str = "ok";
const ERR: &str = "err";

// The "purpose" value of a security domain secret. Used when the client
// presents a wrapped secret that will be used as such.
pub(crate) const KEY_PURPOSE_SECURITY_DOMAIN_SECRET: &str = "security domain secret";

map_keys! {
    AUTH_LEVEL, AUTH_LEVEL_KEY = "auth_level",
    CMD, CMD_KEY = "cmd",
    COUNTER_ID, COUNTER_ID_KEY = "counter_id",
    DEVICE_ID, DEVICE_ID_KEY = "device_id",
    DEVICES, DEVICES_KEY = "devices",
    ENCODED_REQUESTS, ENCODED_REQUESTS_KEY = "encoded_requests",
    EXTERNAL_DEVICE_IDENTIFIER, EXTERNAL_DEVICE_IDENTIFIER_KEY = "ext_device_id",
    KEY, KEY_KEY = "key",
    LAST_USED, LAST_USED_KEY = "last_used",
    PIN_ATTEMPTS, PIN_ATTEMPTS_KEY = "pin_attempts",
    PRIV_KEY, PRIV_KEY_KEY = "priv_key",
    PUB_KEY, PUB_KEY_KEY = "pub_key",
    PUB_KEYS, PUB_KEYS_KEY = "pub_keys",
    PURPOSE, PURPOSE_KEY = "purpose",
    REGISTER_TIME, REGISTER_TIME_KEY = "register_time",
    SECRET, SECRET_KEY = "secret",
    SIG, SIG_KEY = "sig",
    TO, TO_KEY = "to",
    UV_KEY_PENDING, UV_KEY_PENDING_KEY = "uv_key_pending",
    VAULT_HANDLE_WITHOUT_TYPE, VAULT_HANDLE_WITHOUT_TYPE_KEY = "vault_handle_without_type",
    WRAPPED_PIN_DATA, WRAPPED_PIN_DATA_KEY = "wrapped_pin_data",
    WRAPPED_SECRET, WRAPPED_SECRET_KEY = "wrapped_secret",
    WRAPPING_KEYS, WRAPPING_KEYS_KEY = "wrapping_keys",
}

// Since AES-GCM can only handle 2^32 encryptions per key, the per-registration
// keys use a two-step construction where the nonce is a pair of 96-bit values.
// The first of the pair is used with HKDF to derive an AES-GCM key, and the
// second of the pair is the standard AES-GCM nonce.

const LARGE_NONCE_LEN: usize = 24;
const AES256_KEY_LEN: usize = 32;
const GCM_OVERHEAD: usize = 16;

/// Return an AES-256-GCM key for encrypting account data, plus the GCM
/// nonce to use.
fn get_key_and_nonce(
    wrapping_key: &[u8],
    nonce: &[u8; LARGE_NONCE_LEN],
) -> ([u8; 32], [u8; crypto::NONCE_LEN]) {
    static_assertions::const_assert!(LARGE_NONCE_LEN == 2 * crypto::NONCE_LEN);
    let (key_nonce, gcm_nonce) = nonce.split_at(LARGE_NONCE_LEN - crypto::NONCE_LEN);
    let mut gcm_key = [0u8; AES256_KEY_LEN];
    // unwrap: only fails if output is too long, but output here is only 32 bytes.
    crypto::hkdf_sha256(wrapping_key, key_nonce, b"derive wrapping key", &mut gcm_key).unwrap();
    // unwrap: `gcm_nonce` is the correct length, as checked above.
    (gcm_key, gcm_nonce.try_into().unwrap())
}

// Encrypt `data` with `wrapping_key`. The same `purpose` value must be
// presented to `unwrap` for `unwrap` to be successful.
fn wrap(wrapping_key: &[u8], data: &[u8], purpose: &str) -> Vec<u8> {
    let mut nonce = [0u8; LARGE_NONCE_LEN];
    crypto::rand_bytes(&mut nonce);
    let (gcm_key, gcm_nonce) = get_key_and_nonce(wrapping_key, &nonce);
    let mut ciphertext = Vec::with_capacity(data.len() + GCM_OVERHEAD + LARGE_NONCE_LEN);
    ciphertext.extend_from_slice(data);
    crypto::aes_256_gcm_seal_in_place(&gcm_key, &gcm_nonce, purpose.as_bytes(), &mut ciphertext);
    let mut nonce = nonce.to_vec();
    nonce.extend_from_slice(&ciphertext);
    nonce
}

// Decrypt `data` that was encrypted by calling `wrap` with the same
// `wrapping_key` and `purpose`.
fn unwrap(wrapping_key: &[u8], data: &[u8], purpose: &str) -> Result<Vec<u8>, RequestError> {
    if data.len() < LARGE_NONCE_LEN {
        return debug("wrapped data too small");
    }
    let (nonce_slice, ciphertext) = data.split_at(LARGE_NONCE_LEN);
    // unwrap: we know that the length is correct because it came from `split_at`.
    let (gcm_key, gcm_nonce) = get_key_and_nonce(wrapping_key, nonce_slice.try_into().unwrap());
    crypto::aes_256_gcm_open_in_place(
        &gcm_key,
        &gcm_nonce,
        purpose.as_bytes(),
        Vec::from(ciphertext),
    )
    .map_err(|_| RequestError::Debug("decryption failed"))
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

enum SourceOfSecret {
    Wrapped,
    Direct,
}

/// Get the security domain secret for a client's request, either because it's
/// wrapped, or because the client provided it directly.
fn get_secret_from_request(
    state: &DirtyFlag<ParsedState>,
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

pub struct PINState {
    /// The number of incorrect attempts made.
    attempts: i64,
}

// The parsed form of the account's state.
//
// This code only does a shallow parse of the account's state and manipulates
// the CBOR structures directly. This results in a few cases of "impossible"
// errors, where the account state is invalid, but saves having another
// representation and its accompanying parse and serialise logic. This might
// be worth revisiting in the future if we later feel the tradeoff wasn't
// worthwhile.
pub struct ParsedState {
    transparent: BTreeMap<MapKey, Value>,
    confidential: BTreeMap<MapKey, Value>,
}

impl ParsedState {
    fn serialize(self: ParsedState) -> StateData {
        StateData {
            transparent: Value::Map(self.transparent).to_bytes(),
            confidential: Value::Map(self.confidential).to_bytes(),
        }
    }

    /// Gets a trusted device, by id.
    fn get_device(&self, device_id: &[u8]) -> Option<&BTreeMap<MapKey, Value>> {
        let Some(Value::Map(devices)) = self.transparent.get(DEVICES_KEY) else {
            return None;
        };
        let Some(Value::Map(client)) =
            devices.get(&MapKeyRef::Slice(device_id) as &dyn MapLookupKey)
        else {
            return None;
        };
        Some(client)
    }

    fn get_mut_device(&mut self, device_id: &[u8]) -> Option<&mut BTreeMap<MapKey, Value>> {
        let Some(Value::Map(devices)) = self.transparent.get_mut(DEVICES_KEY) else {
            return None;
        };
        let Some(Value::Map(client)) =
            devices.get_mut(&MapKeyRef::Slice(device_id) as &dyn MapLookupKey)
        else {
            return None;
        };
        Some(client)
    }

    /// Gets a [`btree_map::Entry`] for a trusted device, which can be used
    /// to insert or delete it.
    fn get_device_entry(
        &mut self,
        device_id: Vec<u8>,
    ) -> Result<btree_map::Entry<'_, MapKey, Value>, RequestError> {
        let Some(Value::Map(devices)) = self.transparent.get_mut(DEVICES_KEY) else {
            return debug("malformed transparent data");
        };
        Ok(devices.entry(MapKey::Bytestring(device_id)))
    }

    /// Get a per-device wrapping key.
    fn wrapping_key(&self, device_id: &[u8]) -> Result<&[u8], RequestError> {
        let Some(Value::Map(wrapping_keys)) = self.confidential.get(WRAPPING_KEYS_KEY) else {
            return debug("malformed confidential data");
        };
        let Some(Value::Bytestring(wrapping_key)) =
            wrapping_keys.get(&MapKeyRef::Slice(device_id) as &dyn MapLookupKey)
        else {
            return debug("missing wrapping key");
        };
        Ok(wrapping_key)
    }

    /// Encrypt `data` for a given device to store. See the top-level `wrap`
    /// function.
    fn wrap(&self, device_id: &[u8], data: &[u8], purpose: &str) -> Result<Vec<u8>, RequestError> {
        let wrapping_key = self.wrapping_key(device_id)?;
        Ok(wrap(wrapping_key, data, purpose))
    }

    /// Decrypt data previously encrypted with `wrap`. See the top-level
    /// `unwrap` function.
    fn unwrap(
        &self,
        device_id: &[u8],
        data: &[u8],
        purpose: &str,
    ) -> Result<Vec<u8>, RequestError> {
        let wrapping_key = self.wrapping_key(device_id)?;
        unwrap(wrapping_key, data, purpose)
    }

    fn get_pin_state(&self, device_id: &[u8]) -> Result<PINState, RequestError> {
        let device = self.get_device(device_id).ok_or(RequestError::Debug("unknown device"))?;
        let attempts = match device.get(PIN_ATTEMPTS_KEY) {
            Some(Value::Int(attempts)) => *attempts,
            _ => 0,
        };
        Ok(PINState { attempts })
    }

    fn set_pin_state(&mut self, device_id: &[u8], pin_state: PINState) -> Result<(), RequestError> {
        let device = self.get_mut_device(device_id).ok_or(RequestError::Debug("unknown device"))?;
        if pin_state.attempts == 0 {
            device.remove(PIN_ATTEMPTS_KEY);
        } else {
            device.insert(PIN_ATTEMPTS.into(), Value::Int(pin_state.attempts));
        }
        Ok(())
    }
}

impl Default for ParsedState {
    fn default() -> ParsedState {
        let confidential = BTreeMap::from([(
            MapKey::String(String::from(WRAPPING_KEYS)),
            Value::Map(BTreeMap::new()),
        )]);

        ParsedState {
            transparent: BTreeMap::from([(
                MapKey::String(String::from(DEVICES)),
                Value::Map(BTreeMap::new()),
            )]),
            confidential,
        }
    }
}

/// Represents the type of public key that authenticates a request.
#[derive(PartialEq, Eq, PartialOrd, Ord, Copy, Clone)]
enum AuthLevel {
    /// The key is kept in software.
    Software,
    /// The key is hardware bound to the device.
    Hardware,
    /// The key is bound to the device and requires user verification before
    /// it can be used for signing.
    UserVerification,
    // The key is kept in software, but user verification is performed before it is used for
    // signing.
    SoftwareUserVerification,
}

impl AuthLevel {
    fn as_str(&self) -> &'static str {
        match self {
            AuthLevel::Software => "sw",
            AuthLevel::Hardware => "hw",
            AuthLevel::UserVerification => "uv",
            AuthLevel::SoftwareUserVerification => "swuv",
        }
    }
}

impl core::str::FromStr for AuthLevel {
    type Err = ();
    fn from_str(s: &str) -> Result<AuthLevel, ()> {
        match s {
            "sw" => Ok(AuthLevel::Software),
            "hw" => Ok(AuthLevel::Hardware),
            "uv" => Ok(AuthLevel::UserVerification),
            "swuv" => Ok(AuthLevel::SoftwareUserVerification),
            _ => Err(()),
        }
    }
}

/// Represents whether a client has very recently reauthenticated. This is a
/// feature of Google Accounts and so the host simply tells this enclave whether
/// it's true or not for a given client.
#[derive(Copy, Clone)]
enum Reauth {
    None,
    Done,
}

/// Represents whether the client is using its declared option to register a UV
/// key after registration. In this case, it is able to make UV assertions if
/// the assertion command is in the same batch.
#[derive(Copy, Clone)]
enum OneTimeUV {
    Consumed,
    None,
}

/// Represents which device a request is coming from.
enum Authentication {
    None,
    // Contains the device ID, authentication level, whether the client is using
    // a one-time UV assertion, and whether the client reauthenticated very
    // recently.
    Device(Vec<u8>, AuthLevel, OneTimeUV, Reauth),
    // Requests processed after a registration will observe this special
    // authentication level. Duplicate registrations are silently accepted so
    // one must be very careful with this authentication level since it can be
    // asserted by anyone with knowledge of the (semi-public) device ID and
    // public keys of an existing device.
    NewlyRegistered(Vec<u8>),
}

impl ClientState {
    fn parse(self: ClientState) -> Result<ParsedState, Error> {
        match self {
            ClientState::Initial => Ok(ParsedState::default()),
            ClientState::Explicit(data) => {
                let Value::Map(transparent) =
                    cbor::parse(data.transparent).map_err(Error::TransparentDataCBORError)?
                else {
                    return Err(Error::Str("transparent data isn't a map"));
                };
                let Value::Map(confidential) =
                    cbor::parse(data.confidential).map_err(Error::ConfidentialDataCBORError)?
                else {
                    return Err(Error::Str("confidential data isn't a map"));
                };

                Ok(ParsedState { transparent, confidential })
            }
        }
    }
}

struct DirtyFlag<'a, T> {
    _contents: &'a mut T,
    changed: bool,
    minor_change: bool,
}

impl<'a, T> core::ops::Deref for DirtyFlag<'a, T> {
    type Target = T;
    fn deref(&self) -> &T {
        self._contents
    }
}

impl<'a, T> DirtyFlag<'a, T> {
    fn new(r: &'a mut T) -> Self {
        DirtyFlag { _contents: r, changed: false, minor_change: false }
    }

    fn get_mut(&mut self) -> &mut T where {
        self.changed = true;
        self._contents
    }

    /// Declare that a mutation is minor and thus shouldn't set the dirty flag.
    fn get_mut_for_minor_change(&mut self) -> &mut T where {
        self.minor_change = true;
        self._contents
    }
}

pub fn process_client_msg(
    state: ClientState,
    mut ext_ctx: ExternalContext,
    handshake_hash: &[u8],
    client_msg: Vec<u8>,
) -> Result<(Value, StateUpdate), Error> {
    let mut state = state.parse()?;

    let Value::Map(client_msg) = cbor::parse(client_msg).map_err(Error::CBORError)? else {
        return Err(Error::Str("request structure was not a map"));
    };
    let Some(Value::Bytestring(encoded_requests)) = client_msg.get(ENCODED_REQUESTS_KEY) else {
        return Err(Error::Str("encoded_requests must be given"));
    };
    let Value::Array(requests) =
        cbor::parse_bytes(encoded_requests.clone()).map_err(Error::CBORError)?
    else {
        return Err(Error::Str("encoded_requests must be an array"));
    };

    let mut auth = Authentication::None;
    if let Some(device_id) = client_msg.get(DEVICE_ID_KEY) {
        let Value::Bytestring(device_id) = device_id else {
            return Err(Error::Str("device_id must be a bytestring"));
        };
        let device_id = device_id.to_vec();
        let Some(Value::String(auth_level)) = client_msg.get(AUTH_LEVEL_KEY) else {
            return Err(Error::Str("auth_level must be given"));
        };
        let auth_level: AuthLevel =
            auth_level.parse().map_err(|_| Error::Str("unrecognised authentication level"))?;
        let Some(Value::Bytestring(sig)) = client_msg.get(SIG_KEY) else {
            return Err(Error::Str("signature must be given"));
        };
        let Some(client) = state.get_device(&device_id) else {
            return Err(Error::UnknownClient);
        };
        let Some(Value::Map(pub_keys)) = client.get(PUB_KEYS_KEY) else {
            return Err(Error::Str("client is missing pub_keys"));
        };
        let Some(Value::Bytestring(pub_key)) =
            pub_keys.get(&MapKeyRef::Str(auth_level.as_str()) as &dyn MapLookupKey)
        else {
            return Err(Error::Str("no such public key at that auth level"));
        };
        let Some((pub_key_type, pub_key)) = spki::parse(pub_key) else {
            return Err(Error::Str("cannot parse registered public key"));
        };
        let encoded_requests_hash = crypto::sha256(encoded_requests);
        let signed_message = [handshake_hash, encoded_requests_hash.as_ref()].concat();
        if !match pub_key_type {
            spki::PublicKeyType::P256 => crypto::ecdsa_verify(pub_key, &signed_message, sig),
            spki::PublicKeyType::RSA => crypto::rsa_verify(pub_key, &signed_message, sig),
        } {
            return Err(Error::Str("signature validation failed"));
        }
        auth = Authentication::Device(
            device_id,
            auth_level,
            OneTimeUV::None,
            if ext_ctx.is_reauthenticated { Reauth::Done } else { Reauth::None },
        );
    }

    // The state is passed to `do_request` wrapped in a `DirtyFlag`, which tracks
    // whether any mutable references to the state were requested.
    let mut state_with_dirty_flag = DirtyFlag::new(&mut state);
    let mut results = Vec::<Value>::with_capacity(requests.len());
    for request in requests {
        let Value::Map(request) = request else {
            return Err(Error::Str("each request must be a map"));
        };
        match do_request(&ext_ctx, &mut auth, &mut state_with_dirty_flag, request) {
            Ok(result) => results
                .push(Value::Map(BTreeMap::from([(MapKey::String(String::from(OK)), result)]))),
            Err(error) => {
                results.push(Value::Map(BTreeMap::from([(
                    MapKey::String(String::from(ERR)),
                    error.to_cbor(),
                )])));
                break;
            }
        }
    }
    // If any mutable references to the state were requested then the state change
    // is "major" and must be saved to the datastore in order for the request to be
    // successful.
    let has_major_update = state_with_dirty_flag.changed;

    // If a device was recognised, the `last_used` value for it will be updated.
    // This is a "minor" state update and may be discarded.
    let has_minor_update = state_with_dirty_flag.minor_change
        || match auth {
            Authentication::Device(device_id, _, _, _) => {
                if let Some(device) = state.get_mut_device(&device_id) {
                    device.insert(
                        MapKey::String(String::from(LAST_USED)),
                        Value::Int(ext_ctx.current_time_epoch_millis),
                    );
                    device.insert(
                        MapKey::String(String::from(EXTERNAL_DEVICE_IDENTIFIER)),
                        Value::Bytestring(Bytes::from(core::mem::take(
                            &mut ext_ctx.client_device_identifier,
                        ))),
                    );
                    true
                } else {
                    false
                }
            }
            _ => false,
        };

    let update = if has_major_update {
        StateUpdate::Major(state.serialize())
    } else if has_minor_update {
        StateUpdate::Minor(state.serialize())
    } else {
        StateUpdate::None
    };

    Ok((Value::Array(results), update))
}

/// Enumerates the possible errors from a single request.
///
/// A message from a client contains an array of requests which are performed
/// until one fails. Thus a message can partially succeed and so errors from
/// processing requests are separate from the top-level errors described by
/// `Error`.
#[derive(Debug, PartialEq)]
enum RequestError {
    /// A passkey creation request could not be satisfied because the
    /// enclave doesn't support any of the requested algorithms.
    NoSupportedAlgorithm,

    /// A resource with the same identifier already exists.
    Duplicate,

    /// The claimed PIN was incorrect.
    IncorrectPIN,

    /// The device has made too many incorrect PIN attempts and cannot make
    /// any more.
    PINLocked,

    /// Client provided recovery key store keys that had a lower version than
    /// those previously used.
    RecoveryKeyStoreDowngrade,

    /// An error that should never happen and thus is only reported for
    /// debugging purposes. Clients are not expected to handle these errors
    /// other than to log them.
    Debug(&'static str),
}

impl RequestError {
    fn to_cbor(&self) -> Value {
        match self {
            RequestError::NoSupportedAlgorithm => Value::Int(1),
            RequestError::Duplicate => Value::Int(2),
            RequestError::IncorrectPIN => Value::Int(3),
            RequestError::PINLocked => Value::Int(4),
            RequestError::RecoveryKeyStoreDowngrade => Value::Int(6),
            RequestError::Debug(s) => Value::String(String::from(*s)),
        }
    }
}

/// A trivial function to return a `Debug` error.
fn debug<T>(msg: &'static str) -> Result<T, RequestError> {
    Err(RequestError::Debug(msg))
}

fn do_request(
    ext_ctx: &ExternalContext,
    auth: &mut Authentication,
    state: &mut DirtyFlag<ParsedState>,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    let Some(Value::String(cmd)) = request.get(CMD_KEY) else {
        return debug("request is missing cmd");
    };
    match cmd.as_str() {
        "device/register" => do_device_register(ext_ctx, auth, state, request),
        "device/add_uv_key" => do_device_add_uv_key(auth, state, request),
        "device/forget" => do_device_forget(auth, state, request),
        "debug/success" => Ok(Value::Boolean(true)),
        "debug/dump" => do_debug_dump(ext_ctx, state, request),
        "keys/genpair" => do_keys_genpair(auth, state, request),
        "keys/wrap" => do_keys_wrap(auth, state, request),
        "passkeys/assert" => passkeys::do_assert(auth, state, request),
        "passkeys/create" => passkeys::do_create(auth, state, request),
        "passkeys/wrap_pin" => passkeys::do_wrap_pin(auth, state, request),
        "recovery_key_store/wrap" => {
            recovery_key_store::do_wrap(ext_ctx.current_time_epoch_millis, request)
        }
        "recovery_key_store/wrap_as_member" => recovery_key_store::do_wrap_as_member(
            auth,
            state,
            ext_ctx.current_time_epoch_millis,
            request,
        ),
        "recovery_key_store/rewrap" => {
            recovery_key_store::do_rewrap(auth, state, ext_ctx.current_time_epoch_millis, request)
        }
        _ => debug("unknown command"),
    }
}

fn do_device_register(
    ext_ctx: &ExternalContext,
    auth: &mut Authentication,
    state: &mut DirtyFlag<ParsedState>,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    let Some(Value::Bytestring(device_id)) = request.get(DEVICE_ID_KEY) else {
        return debug("missing device_id");
    };
    if device_id.len() > 128 {
        return debug("device_id too long");
    }
    let device_id = device_id.clone();

    let mut device: BTreeMap<MapKey, Value> = BTreeMap::new();
    device.insert(
        MapKey::String(String::from(REGISTER_TIME)),
        Value::Int(ext_ctx.current_time_epoch_millis),
    );
    device.insert(
        MapKey::String(String::from(EXTERNAL_DEVICE_IDENTIFIER)),
        Value::Bytestring(Bytes::from(ext_ctx.client_device_identifier.clone())),
    );

    let mut has_uv_key = false;
    let mut has_uv_key_pending = false;

    for (key, value) in request {
        let MapKey::String(key) = key else {
            continue;
        };
        match key.as_str() {
            PUB_KEYS => {
                let Value::Map(pub_keys) = value else {
                    return debug("pub_keys must be a map");
                };
                if pub_keys.is_empty() {
                    return debug("pub_keys cannot be empty");
                }
                for (k, v) in &pub_keys {
                    let MapKey::String(k) = k else {
                        return debug("pub_keys contains non-string key");
                    };
                    let Value::Bytestring(spki) = v else {
                        return debug("pub_keys contains non-bytestring value");
                    };
                    if spki::parse(spki).is_none() {
                        return debug("cannot parse SPKI from pub_key entry");
                    };
                    if k == AuthLevel::UserVerification.as_str()
                        || k == AuthLevel::SoftwareUserVerification.as_str()
                    {
                        if has_uv_key {
                            return debug("can't register both uv and swuv key");
                        }
                        has_uv_key = true;
                    }
                }
                device.insert(MapKey::String(key), Value::Map(pub_keys));
            }
            UV_KEY_PENDING => {
                device.insert(MapKey::String(String::from(UV_KEY_PENDING)), Value::Boolean(true));
                has_uv_key_pending = true;
            }
            _ => continue,
        }
    }

    if !device.contains_key(PUB_KEYS_KEY) {
        return debug("missing pub_keys");
    }
    if has_uv_key && has_uv_key_pending {
        return debug("can't defer UV key creation when also setting one");
    }

    /// Check if an existing device (given as a `Value`) matches the proposed
    /// new device record.
    fn entry_matches(existing: &Value, new: &BTreeMap<MapKey, Value>) -> bool {
        let Value::Map(existing) = existing else {
            return false;
        };
        let Some(Value::Map(existing_pub_keys)) = existing.get(PUB_KEYS_KEY) else {
            return false;
        };
        let Some(Value::Map(new_pub_keys)) = new.get(PUB_KEYS_KEY) else {
            return false;
        };
        let Value::Boolean(existing_uv_key_pending) =
            existing.get(UV_KEY_PENDING_KEY).unwrap_or(&Value::Boolean(false))
        else {
            return false;
        };
        let Value::Boolean(new_uv_key_pending) =
            new.get(UV_KEY_PENDING_KEY).unwrap_or(&Value::Boolean(false))
        else {
            return false;
        };
        existing_pub_keys == new_pub_keys && existing_uv_key_pending == new_uv_key_pending
    }

    let did_insert = match state.get_mut().get_device_entry(device_id.to_vec())? {
        btree_map::Entry::Vacant(entry) => {
            entry.insert(Value::Map(device));
            true
        }
        btree_map::Entry::Occupied(entry) => {
            // Entry already exists. The registration will be a no-op success if
            // the device matches, otherwise a failure.
            if !entry_matches(entry.get(), &device) {
                return Err(RequestError::Duplicate);
            }
            false
        }
    };

    if did_insert {
        let Some(Value::Map(wrapping_keys)) =
            state.get_mut().confidential.get_mut(WRAPPING_KEYS_KEY)
        else {
            return debug("malformed confidential data");
        };
        let mut random_key = [0u8; 32];
        crypto::rand_bytes(&mut random_key);
        wrapping_keys.insert(MapKey::Bytestring(device_id.to_vec()), random_key.to_vec().into());
    }

    if let Authentication::None = *auth {
        *auth = Authentication::NewlyRegistered(device_id.to_vec());
    }
    Ok(Value::Boolean(true))
}

fn do_device_add_uv_key(
    auth: &mut Authentication,
    state: &mut DirtyFlag<ParsedState>,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    let (device_id, auth_level, reauth) = match auth {
        Authentication::Device(device_id, auth_level, _, reauth) => (device_id, auth_level, reauth),
        _ => {
            return debug("device identity required");
        }
    };
    let Some(Value::Bytestring(spki)) = request.get(PUB_KEY_KEY) else {
        return debug("need pub_key");
    };
    if spki::parse(spki).is_none() {
        return debug("invalid SPKI");
    }

    // Check whether the device already has a UV key.
    let Some(device) = state.get_device(device_id) else {
        return debug("no device record");
    };
    let Some(Value::Map(pub_keys)) = device.get(PUB_KEYS_KEY) else {
        return debug("device missing pub_keys");
    };
    let swuv = MapKey::String(String::from(AuthLevel::SoftwareUserVerification.as_str()));
    if pub_keys.contains_key(&swuv) {
        return debug("software UV key already registered");
    }
    let uv = MapKey::String(String::from(AuthLevel::UserVerification.as_str()));
    match pub_keys.get(&uv) {
        Some(Value::Bytestring(existing_uv_key)) => {
            if existing_uv_key == spki {
                return Ok(Value::Boolean(true));
            } else {
                return debug("different UV key already registered");
            }
        }
        Some(_) => {
            return debug("UV key is wrong type");
        }
        None => (),
    }
    // Check that  `uv_key_pending` is set.
    match device.get(UV_KEY_PENDING_KEY) {
        Some(Value::Boolean(uv_key_pending)) if *uv_key_pending => (),
        _ => return debug("uv_key_pending is missing"),
    }

    // Requirements have been checked. Now get a mutable reference and update
    // the device record.
    let Some(device) = state.get_mut().get_mut_device(device_id) else {
        return debug("no device record");
    };
    device.remove(UV_KEY_PENDING_KEY);
    let Some(Value::Map(pub_keys)) = device.get_mut(PUB_KEYS_KEY) else {
        // Impossible since the structure of "pub_keys" was checked above.
        return debug("internal error");
    };
    pub_keys.insert(uv, Value::Bytestring(spki.clone()));

    // Allow some subsequent commands in this request to act as if UV was asserted.
    *auth = Authentication::Device(device_id.clone(), *auth_level, OneTimeUV::Consumed, *reauth);

    Ok(Value::Boolean(true))
}

fn do_device_forget(
    _auth: &Authentication,
    state: &mut DirtyFlag<ParsedState>,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    let Some(Value::Bytestring(device_id)) = request.get(DEVICE_ID_KEY) else {
        return debug("missing device_id");
    };
    let btree_map::Entry::Occupied(entry) = state.get_mut().get_device_entry(device_id.to_vec())?
    else {
        return Ok(Value::Boolean(false));
    };
    entry.remove_entry();
    Ok(Value::Boolean(true))
}

fn do_keys_genpair(
    auth: &Authentication,
    state: &mut DirtyFlag<ParsedState>,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    let device_id: &Vec<u8> = match auth {
        Authentication::Device(device_id, _, _, _) => device_id,
        Authentication::NewlyRegistered(device_id) => device_id,
        Authentication::None => {
            return debug("device identity required");
        }
    };
    let Some(Value::String(purpose)) = request.get(PURPOSE_KEY) else {
        return debug("purpose required");
    };

    let key = crypto::P256Scalar::generate();

    Ok(cbor!({
        PUB_KEY: (key.compute_public_key().to_vec()),
        PRIV_KEY: (state.wrap(device_id, &key.bytes(), purpose)?),
    }))
}

fn do_keys_wrap(
    auth: &Authentication,
    state: &mut DirtyFlag<ParsedState>,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    let device_id: &Vec<u8> = match auth {
        Authentication::Device(device_id, _, _, _) => device_id,
        Authentication::NewlyRegistered(device_id) => device_id,
        Authentication::None => {
            return debug("device identity required");
        }
    };
    let Some(Value::Bytestring(key)) = request.get(KEY_KEY) else {
        return debug("key required");
    };
    let Some(Value::String(purpose)) = request.get(PURPOSE_KEY) else {
        return debug("purpose required");
    };
    Ok(state.wrap(device_id, key, purpose)?.into())
}

fn do_debug_dump(
    ext_ctx: &ExternalContext,
    state: &mut DirtyFlag<ParsedState>,
    _request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    Ok(cbor!({
        "transparent": (Value::Map(state.transparent.clone())),
        "current_time": (ext_ctx.current_time_epoch_millis),
        "reauth": (ext_ctx.is_reauthenticated),
    }))
}

#[cfg(test)]
mod tests {
    extern crate bytes;
    extern crate hex;
    extern crate std;

    use super::*;
    use alloc::boxed::Box;
    use alloc::{format, vec};
    use cbor::cbor;
    use crypto::EcdsaKeyPair;
    use passkeys::{
        CLAIMED_PIN, CLIENT_DATA_JSON, COSE_ALGORITHM, PIN_CLAIM_KEY, PIN_GENERATION, PIN_HASH,
        PROTOBUF, PUB_KEY_CRED_PARAMS, RP_ID, WEBAUTHN_REQUEST,
    };
    use prost::Message;
    use recovery_key_store::{CERT_XML, SIG_XML};

    const ERR_KEY: &dyn MapLookupKey = &MapKeyRef::Str(ERR) as &dyn MapLookupKey;
    pub const SAMPLE_SECURITY_DOMAIN_SECRET : &[u8] = b"\xc4\xdf\xa4\xed\xfc\xf9\x7c\xc0\x3a\xb1\xcb\x3c\x03\x02\x9b\x5a\x05\xec\x88\x48\x54\x42\xf1\x20\xb4\x75\x01\xde\x61\xf1\x39\x5d";
    pub const WEBAUTHN_SECRETS_ENCRYPTION_KEY : &[u8] = b"\x55\x9d\xec\xf5\xc3\x42\xbd\xd1\x74\xd3\x3a\x9f\x8f\x8a\x4a\xe0\xf6\x60\x3b\xf8\xe2\xda\x2c\x59\x58\x90\xae\xd9\x3b\xcf\xa8\x18";
    // PROTOBUF_BYTES is a serialized WebauthnCredentialSpecifics that contains an
    // encrypted private key.
    pub const PROTOBUF_BYTES : &[u8] = b"\x0a\x10\x78\x0e\x1d\x97\x71\xc7\xc4\x21\x1a\xdf\xf5\x6f\x88\xe8\xf8\x0b\x12\x10\x2e\x32\x3a\x5b\x2a\x6b\xb8\x8f\x8b\x86\x98\x01\xc8\xfd\x55\xff\x1a\x0b\x77\x65\x62\x61\x75\x74\x68\x6e\x2e\x69\x6f\x22\x0f\x52\x57\x35\x6a\x62\x47\x46\x32\x5a\x56\x52\x6c\x63\x33\x51\x30\x9e\x90\xde\xc9\xa5\x31\x3a\x0b\x45\x6e\x63\x6c\x61\x76\x65\x54\x65\x73\x74\x42\x0b\x45\x6e\x63\x6c\x61\x76\x65\x54\x65\x73\x74\x4a\xa6\x01\x7a\x8c\xb5\xf4\x9b\x0a\xeb\xc3\xd7\x7f\xbf\xe5\x25\xcf\x81\x5f\x7e\x2a\xd2\x6b\xe4\xfb\xd7\x71\x14\x2a\x7f\xc7\xe4\xad\xb1\xa2\x9b\xe9\x7a\xac\x56\x9f\x21\xe3\xc3\xa6\x91\x5a\x0a\xd1\x41\x59\xff\xb7\xad\x5a\x3a\x20\x3d\x35\xac\x5c\x8d\xc8\xfe\x2c\x59\x69\x23\x3f\xda\x6c\x3b\xc9\x30\x45\x8b\xc2\x87\x64\x33\xb0\x87\x6d\x55\x48\x96\x36\x39\x03\xc2\x18\x43\xa0\xde\x9c\x47\x37\x58\xb9\x1e\x29\xdf\x14\xcd\x3b\xb8\x19\x02\x7e\xc6\x44\x57\xf0\xce\x1b\x77\xa3\xb5\x63\x08\x81\x1a\x1b\x28\x98\xc3\x6c\xc0\x8e\xd6\x45\xe0\x5d\x14\x98\x3d\x1f\xe6\xba\x9f\xe1\xe5\xe9\x09\xbd\xbf\x85\xe9\xef\xe0\x5c\x9a\xea\x62\xfa\xa5\xe3\xfc\x05\x42\x62\xa7\xeb\x26\xb4\x77\xe0\xe0\x39\x58\x00";
    // PROTOBUF2_BYTES is a serialized WebauthnCredentialSpecifics that contains an
    // encrypted protobuf within it.
    pub const PROTOBUF2_BYTES : &[u8] = b"\x0a\x10\x1d\x3e\xb1\xeb\xd4\x37\x0c\xc1\xfe\xaa\xdc\x49\x7b\x5c\x24\xa1\x12\x10\x8f\xb8\xa3\x31\xd7\xdf\x84\x47\xdb\x3a\x64\x49\xc9\x70\x3f\xfa\x1a\x0b\x77\x65\x62\x61\x75\x74\x68\x6e\x2e\x69\x6f\x22\x10\x52\x57\x35\x6a\x62\x47\x46\x32\x5a\x56\x52\x6c\x63\x33\x51\x79\x30\xe4\xe1\xc2\x82\xa6\x31\x3a\x0c\x45\x6e\x63\x6c\x61\x76\x65\x54\x65\x73\x74\x32\x42\x0c\x45\x6e\x63\x6c\x61\x76\x65\x54\x65\x73\x74\x32\x58\x00\x62\xcb\x01\x3f\x25\xa1\x79\x8b\xc5\x55\x01\x15\xc8\xe5\xb4\xf4\x00\xc6\x03\x70\xc1\x61\xaf\x4a\x02\xeb\xa6\xea\x9b\xd4\x2c\x88\x7b\x80\x59\xfd\xf5\xe9\xef\xf6\xa2\x8a\xbb\xa1\xe8\x44\x91\x8e\x83\x05\x28\x5c\x98\x9a\xd9\xa5\x9a\x99\x74\x05\x47\x67\xc3\x65\xff\xcf\x98\x2f\xfd\xcb\xd4\x6c\x1a\xeb\x8d\xcf\xee\x24\x42\x5b\x14\xfe\x77\x4a\x2d\x4e\x6c\x87\x56\xdb\xf3\x36\x42\x12\xb7\x49\x11\xee\xb6\x97\xa3\x78\xca\xbf\x75\xeb\xe8\x6f\xf5\xa0\xf3\x04\x48\xf5\x99\x44\x4b\x1c\x80\x08\x6a\x37\xe4\x8e\xf9\xbb\xa7\xd2\xa1\xc8\xa1\x89\xf0\x60\x6d\x69\xf8\x3f\x03\x53\x3f\xbd\x9b\x8c\xfd\x82\xf7\x13\xc0\xd3\xae\xf5\x73\x3c\x31\xad\x95\xb4\x4b\xc3\x94\xbc\xd6\x0b\x84\x9b\xe2\x0f\xed\x8f\x25\x1a\x9b\xda\xad\xff\x2f\xe2\xd0\x07\xfc\x6e\xb0\x2a\x78\x0d\xd6\xf5\x83\x42\x66\x10\x4b\xc7\x51\xd5\x01\xb5\x54\xf5\x4a\xcd\x5e\x8c\xdd\xa3";
    // RSA_PKCS8 was generated with:
    //   openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -pkeyopt
    // rsa_keygen_pubexp:65537 | openssl pkcs8 -topk8 -nocrypt -outform der >
    // /tmp/priv
    pub const RSA_PKCS8 : &[u8] = b"\x30\x82\x04\xbd\x02\x01\x00\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x04\x82\x04\xa7\x30\x82\x04\xa3\x02\x01\x00\x02\x82\x01\x01\x00\xaa\x67\xa4\x73\xd7\xa3\xf1\x2e\xb5\x54\x03\xc7\x4f\x69\x02\x7e\x64\x74\x3b\x7d\xd2\xe5\xc6\x07\x94\xb3\x38\xf4\xc3\xb6\x2b\xe1\x27\xe0\x95\x90\xdd\x5e\x00\xb9\x64\x5a\x35\xa1\x03\x5b\xf3\x3f\x13\xfe\x74\xb6\x2b\x73\xe9\x0f\xd9\x32\xc6\xf6\x83\x5e\xe4\xbb\xd3\x2a\x77\xb3\xb5\x91\xd5\xa7\x69\x6b\x81\x55\xd8\x13\xb7\x48\xf6\xa6\xa7\x5d\x7c\xcf\x03\x50\x5d\xd6\xc3\x05\xed\x55\x69\xe7\x1c\x59\xef\x2a\x87\xbc\x1a\xfe\x30\xc4\xe8\x29\x54\x13\x61\xdd\x3a\x9d\x1e\x20\xf5\x03\x00\x53\xb1\x98\x05\x88\xc9\xba\xe8\x41\x09\x32\x91\x57\x42\xa9\xf7\x93\xb6\xfb\x16\x0e\x6b\x05\x49\xc4\x19\xe9\x2a\x5b\x37\x19\x0a\xd4\x2c\x1b\x84\x77\x46\x6e\xd8\xbe\x32\x32\xc2\x44\x3a\xaf\xc1\xf5\xf0\xdc\x56\x75\x24\xd6\xe0\xc4\x1c\xae\x63\xe5\xca\x97\x9f\x73\x8c\x70\xf7\xe4\x8f\xf8\x42\xd8\x0c\x14\xa6\xde\x25\xa7\xb8\xd1\xb9\x8b\xd0\x92\x4b\xff\x6e\xee\xe3\x88\x77\xe0\xc4\xe1\xc7\x4a\x2a\x75\x70\xde\x9a\xda\xf3\x27\x2c\x42\xaf\x9c\x00\x4a\x4f\x01\x1d\xa8\x9e\xfc\x86\x05\xbb\x51\x65\x29\x64\x8f\xb1\x5e\x66\xfb\xbc\xdc\x33\x21\x82\x76\x8c\xc3\x02\x03\x01\x00\x01\x02\x82\x01\x00\x44\xe3\x4d\x4a\x3f\x7c\xd9\x3d\xa6\xb4\x66\x2a\xa6\xe1\xae\xce\x65\xd1\xcf\x53\x18\x75\x27\x4f\x5d\x3f\xee\xe0\x94\x56\x0a\xfb\x24\xe1\xd7\xd5\x0e\x88\xb8\x06\x3a\x99\x75\x60\xb8\x38\xed\xe7\x2c\x30\x0c\x02\xb1\x22\x54\xaf\xc1\x80\x93\x8a\x88\xa5\x4e\x16\xd8\x51\x2c\xbf\x0b\xc1\xfe\xfb\x84\xd4\x9f\x1e\x93\x11\xb5\x60\xdb\xc5\x97\x97\x65\xa3\x52\x95\xa4\xb9\xf3\x71\x6b\xf6\xc1\xaf\x5a\x78\xc9\x05\x0a\x86\x72\xeb\x1b\xd0\x1e\x82\xc6\xa8\x67\x41\xc6\x36\x4a\x3d\xcc\x8f\x00\x0c\xd5\x98\xbd\x74\x05\x09\x78\x66\x59\x65\xdf\x37\xf6\x6f\x8b\xb6\xa9\x33\x0c\xd1\xa7\x47\xe8\x57\x4d\x8f\xb8\xd5\x33\xd3\xda\xad\xd9\xab\x3c\xfd\xb7\xec\xfa\x6a\x97\x06\xdd\xb5\x6a\x19\xb5\x5d\x82\xe4\x5d\x0e\xe3\x60\x83\x6f\x72\xe3\x8a\x59\x9f\x5e\x79\xed\x45\x15\x87\xc1\x9a\xa6\x14\xac\x33\x77\xe6\x67\xb2\x2b\xdc\x27\xb3\xa0\x64\xc7\xfc\x08\x30\xff\x0f\x02\x6f\xf1\x54\x6a\x18\xe1\x52\x47\x0a\x4b\x2d\xa7\x94\x79\xa2\xa5\xf4\x30\x14\x08\xf3\xf1\x4a\x02\x64\x69\xdc\x87\x54\x7b\x89\x01\xe1\x77\xa8\x74\x94\xaa\xd5\xc5\x11\x89\x2d\xe6\x3a\xd1\x02\x81\x81\x00\xd5\x7a\x7e\x60\x62\x9a\x39\xcd\x70\xc5\x5f\xd2\x34\x69\x53\xc5\xdc\xc4\x8f\x0e\xea\xd6\xd9\xfa\xe6\x8c\x37\x5f\x7a\xa7\xab\x0a\x98\xa0\x09\x3f\xfe\x7c\xef\x01\x9c\x5d\xc3\x9d\x58\xca\xfa\xb3\xcd\x01\x80\xe3\xd9\xb3\x89\x13\x86\xb7\xbe\x5d\x20\x06\x77\x84\xa1\x60\x0d\x17\x77\xc4\x04\xca\x3a\x5f\x23\x80\x65\x15\x01\x93\xcd\x8a\xd8\x3a\xc7\xa9\xdb\x41\x33\xb1\x49\xb1\xa9\x61\x93\x6e\x08\x0a\x18\xfc\xa7\xd1\xcc\xcc\x88\x35\x23\x5f\x4c\x22\x12\xa4\x52\x80\x53\x57\xfb\x4b\x7d\x65\x23\x1e\xfc\xf5\x13\x0e\x4e\x05\x02\x81\x81\x00\xcc\x58\xc6\xa1\xb6\x75\x90\x60\xb6\x3d\x89\xd1\xbb\x1b\x47\x4d\x33\xc7\x9c\x3c\x6c\xf2\x4b\xbb\x9a\xb2\x1e\x5f\xf7\x6d\x41\x60\xf3\xa2\x2c\xfb\xe3\x77\x4c\x52\xe2\xab\xad\xcf\x09\xdf\x94\x0c\x58\xb0\xcc\x3b\x39\x2f\x71\x61\x2c\x0e\x8e\x6e\xc6\x45\xdd\x78\x2b\xfe\x94\x19\x31\x26\x69\x12\x43\x52\xdb\xcb\x60\x73\x24\x7c\xec\x94\xf3\x13\xc5\x91\x4e\xbb\xec\x3b\x04\x31\xe9\x0a\x81\x1f\xe6\xd4\x3e\x84\xd4\x50\xc6\xbf\xd2\x62\xe5\xd7\x8a\x4f\x18\xca\xc7\xd1\xe0\x99\x9c\xf2\xeb\x23\xd3\x09\xff\x3f\xc8\xfc\x22\x27\x02\x81\x81\x00\x84\x7b\xe0\xb2\x30\x7f\x46\x20\x19\x3c\x64\x9b\x2f\xab\xae\x31\xbd\x30\xbf\x17\xa2\xe6\x73\xa1\x22\x33\x22\xaa\x3e\x94\x8f\xb1\xa3\xc6\xad\xf6\xe9\x18\xdf\xbb\x40\x2f\x70\x96\xd5\xe4\x22\x72\x33\x68\x1b\x75\x4c\x45\xff\x6b\xfe\xcf\x49\x74\xc1\xcb\x41\xa1\x2e\x05\x4e\x1a\xa2\x59\x24\x1f\xdc\xd9\xee\x4e\x60\x6d\x08\xed\x91\x41\xf9\xaf\x80\xfa\x08\xf8\x0d\xfc\x98\x9f\x89\x5e\xe5\x00\x04\x3d\x40\x04\x8c\xa1\xc7\x57\xa7\xb0\x52\xa3\x71\xbc\x33\x95\x87\x1d\xdc\x9b\x5d\x79\x1b\xf9\x08\x32\xd3\x09\xc5\x29\xbb\x81\x02\x81\x80\x2f\xe6\x37\x59\x3c\xad\xbe\x14\x0d\x63\xcb\x64\x70\x19\x6a\xd3\x3b\xe9\xf4\x43\x6d\xbe\x35\xe6\x59\xd2\x9a\xb0\x20\x0d\x6a\x1f\xd1\xbc\x18\x13\x4b\x34\x71\x9d\x94\x28\x6d\xeb\x74\x03\x06\x6f\x06\x73\x1a\xcc\x5f\x11\x31\xe0\x77\x35\x4a\x49\xc9\x0c\x23\x67\xc1\xd8\x40\xda\xce\xdc\x94\x10\x85\xdb\x6c\x4d\xf5\xe3\xc7\x8f\xc8\xdc\xf9\x45\x8f\x30\x0a\x66\x9e\x6f\x0f\x02\xab\xff\x9c\x58\xe0\x00\xac\x4e\xf2\x7d\xa4\xb8\xde\x15\xf4\x8e\x5b\x8b\x42\xe2\x75\x88\x4a\xbf\x77\x3c\xb1\xc5\x89\xf8\x73\xee\x7d\xac\x2c\x4d\x02\x81\x80\x44\x70\x7e\x1d\x0f\x2a\xce\x43\xf5\x0c\x09\x8a\xb7\x81\x4a\x40\xf1\xf3\x09\xa7\x72\xdc\x0a\x7e\x8b\x39\x11\x24\x00\x49\x00\x0e\xab\x74\xf4\xf0\xef\x5e\x1f\xac\x4b\x89\x30\xe8\x95\x45\xcd\x5b\x6a\xa6\x73\xe8\x33\x1e\xb4\x5a\x4c\xe9\x96\xf3\x36\xd9\xe8\xd5\x33\xe4\x8c\x89\xd2\xcb\x0a\xa1\x43\x13\xe5\x67\xe7\x8a\x23\x5d\xd9\xf4\xd7\xff\xce\x4f\x4b\x81\x48\xcd\x54\x9d\xf9\x21\x5d\x5a\x36\x6b\x25\xbb\x9f\xe0\x44\x8c\x1a\x5c\x67\x17\x80\x59\x20\xc4\xf6\x55\x70\xee\x7f\x66\x75\x6d\x20\x2a\xb0\xc3\xd4\xce\xe5\x1a";
    // RSA_SPKI is the public-key from `RSA_PKCS8`. It was generated by hand with
    // der2ascii.
    pub const RSA_SPKI : &[u8] = b"\x30\x82\x01\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x03\x82\x01\x0f\x00\x30\x82\x01\x0a\x02\x82\x01\x01\x00\xaa\x67\xa4\x73\xd7\xa3\xf1\x2e\xb5\x54\x03\xc7\x4f\x69\x02\x7e\x64\x74\x3b\x7d\xd2\xe5\xc6\x07\x94\xb3\x38\xf4\xc3\xb6\x2b\xe1\x27\xe0\x95\x90\xdd\x5e\x00\xb9\x64\x5a\x35\xa1\x03\x5b\xf3\x3f\x13\xfe\x74\xb6\x2b\x73\xe9\x0f\xd9\x32\xc6\xf6\x83\x5e\xe4\xbb\xd3\x2a\x77\xb3\xb5\x91\xd5\xa7\x69\x6b\x81\x55\xd8\x13\xb7\x48\xf6\xa6\xa7\x5d\x7c\xcf\x03\x50\x5d\xd6\xc3\x05\xed\x55\x69\xe7\x1c\x59\xef\x2a\x87\xbc\x1a\xfe\x30\xc4\xe8\x29\x54\x13\x61\xdd\x3a\x9d\x1e\x20\xf5\x03\x00\x53\xb1\x98\x05\x88\xc9\xba\xe8\x41\x09\x32\x91\x57\x42\xa9\xf7\x93\xb6\xfb\x16\x0e\x6b\x05\x49\xc4\x19\xe9\x2a\x5b\x37\x19\x0a\xd4\x2c\x1b\x84\x77\x46\x6e\xd8\xbe\x32\x32\xc2\x44\x3a\xaf\xc1\xf5\xf0\xdc\x56\x75\x24\xd6\xe0\xc4\x1c\xae\x63\xe5\xca\x97\x9f\x73\x8c\x70\xf7\xe4\x8f\xf8\x42\xd8\x0c\x14\xa6\xde\x25\xa7\xb8\xd1\xb9\x8b\xd0\x92\x4b\xff\x6e\xee\xe3\x88\x77\xe0\xc4\xe1\xc7\x4a\x2a\x75\x70\xde\x9a\xda\xf3\x27\x2c\x42\xaf\x9c\x00\x4a\x4f\x01\x1d\xa8\x9e\xfc\x86\x05\xbb\x51\x65\x29\x64\x8f\xb1\x5e\x66\xfb\xbc\xdc\x33\x21\x82\x76\x8c\xc3\x02\x03\x01\x00\x01";
    pub const TIMESTAMP: i64 = recovery_key_store::SAMPLE_VALIDATION_EPOCH_MILLIS;
    pub const EXTERNAL_CONTEXT: ExternalContext = ExternalContext {
        current_time_epoch_millis: TIMESTAMP,
        client_device_identifier: Vec::new(),
        is_reauthenticated: false,
    };

    fn bytes(b: Vec<u8>) -> Value {
        Value::Bytestring(Bytes::from(b))
    }

    fn x962_to_spki(x962: &[u8]) -> Vec<u8> {
        const PREFIX : &[u8] = b"\x30\x59\x30\x13\x06\x07\x2a\x86\x48\xce\x3d\x02\x01\x06\x08\x2a\x86\x48\xce\x3d\x03\x01\x07\x03\x42\x00";
        [PREFIX, x962].concat()
    }

    lazy_static! {
        static ref TEST_DEVICE_ID: Vec<u8> = hex::decode("01020304").unwrap();
        static ref TEST_DEVICE_ID2: Vec<u8> = hex::decode("01020305").unwrap();
        static ref TEST_HANDSHAKE_HASH: [u8; 32] = [42u8; 32];
        static ref KEYPAIR: EcdsaKeyPair = {
            let pkcs8_bytes = EcdsaKeyPair::generate_pkcs8();
            EcdsaKeyPair::from_pkcs8(pkcs8_bytes.as_ref()).unwrap()
        };
        static ref SPKI: Vec<u8> = x962_to_spki(KEYPAIR.public_key().as_ref());
        static ref REGISTERED_STATE: ClientState = {
            let encoded_register = cbor!([{
                CMD: "device/register",
                DEVICE_ID: (TEST_DEVICE_ID.clone()),
                PUB_KEYS: {"hw": (SPKI.as_slice())},
            }])
            .to_bytes();
            let msg = cbor!({ENCODED_REQUESTS: encoded_register}).to_bytes();
            let (output, StateUpdate::Major(state)) = process_client_msg(
                ClientState::Initial,
                EXTERNAL_CONTEXT.clone(),
                TEST_HANDSHAKE_HASH.as_slice(),
                msg,
            )
            .unwrap() else {
                panic!("");
            };
            assert_eq!(output, cbor!([{"ok": true}]));
            ClientState::Explicit(state)
        };
        static ref REGISTERED_STATE_WRAPPED_SECRET: Vec<u8> = {
            let msg = sign_request(cbor!({
                CMD: "keys/wrap",
                KEY: SAMPLE_SECURITY_DOMAIN_SECRET,
                PURPOSE: KEY_PURPOSE_SECURITY_DOMAIN_SECRET,
            }));
            let (output, _state) = process_client_msg(
                REGISTERED_STATE.clone(),
                EXTERNAL_CONTEXT.clone(),
                TEST_HANDSHAKE_HASH.as_slice(),
                msg,
            )
            .unwrap();
            let Value::Bytestring(wrapped) = ok_value(&output).unwrap() else {
                panic!("unexpected result")
            };
            wrapped.to_vec()
        };
        static ref REGISTERED_STATE_UV_PENDING: ClientState = {
            let encoded_register = cbor!([{
                CMD: "device/register",
                DEVICE_ID: (TEST_DEVICE_ID.clone()),
                PUB_KEYS: {"hw": (SPKI.as_slice())},
                UV_KEY_PENDING: true,
            }])
            .to_bytes();
            let msg = cbor!({ENCODED_REQUESTS: encoded_register}).to_bytes();
            let (output, StateUpdate::Major(state)) = process_client_msg(
                ClientState::Initial,
                EXTERNAL_CONTEXT.clone(),
                TEST_HANDSHAKE_HASH.as_slice(),
                msg,
            )
            .unwrap() else {
                panic!("");
            };
            assert_eq!(output, cbor!([{"ok": true}]));
            ClientState::Explicit(state)
        };
        static ref ENTITY_PROTOBUF_BYTES: Vec<u8> = {
            let msg = sign_request(cbor!({
                CMD: "passkeys/create",
                WRAPPED_SECRET: (REGISTERED_STATE_WRAPPED_SECRET.clone()),
                WEBAUTHN_REQUEST: {
                    PUB_KEY_CRED_PARAMS: [{
                        COSE_ALGORITHM: (-7),
                    }],
                },
            }));

            let (output, _state) = process_client_msg(
                REGISTERED_STATE.clone(),
                EXTERNAL_CONTEXT.clone(),
                TEST_HANDSHAKE_HASH.as_slice(),
                msg.clone(),
            )
            .unwrap();

            let Value::Map(result) = ok_value(&output).unwrap() else {
                panic!("wrong type: {:?}", output)
            };
            let Some(Value::Bytestring(encrypted)) = result.get(passkeys::ENCRYPTED_KEY) else {
                panic!("missing encrypted data: {:?}", result)
            };
            let Some(Value::Bytestring(_)) = result.get(PUB_KEY_KEY) else {
                panic!("missing public key: {:?}", result)
            };

            chromesync::pb::WebauthnCredentialSpecifics {
                sync_id: None,
                credential_id: Some(vec![4, 3, 2, 1]),
                rp_id: None,
                user_id: Some(vec![1, 2, 3, 4]),
                newly_shadowed_credential_ids: vec![],
                creation_time: None,
                user_name: None,
                user_display_name: None,
                third_party_payments_support: None,
                last_used_time_windows_epoch_micros: None,
                key_version: Some(1),
                encrypted_data: Some(
                    chromesync::pb::webauthn_credential_specifics::EncryptedData::Encrypted(
                        encrypted.clone(),
                    ),
                ),
            }
            .encode_to_vec()
        };
        static ref RSA_REGISTERED_STATE: ClientState = {
            let encoded_register = cbor!([{
                CMD: "device/register",
                DEVICE_ID: (TEST_DEVICE_ID.clone()),
                PUB_KEYS: {"hw": RSA_SPKI},
            }])
            .to_bytes();
            let msg = cbor!({ENCODED_REQUESTS: encoded_register}).to_bytes();
            let (output, StateUpdate::Major(state)) = process_client_msg(
                ClientState::Initial,
                EXTERNAL_CONTEXT.clone(),
                TEST_HANDSHAKE_HASH.as_slice(),
                msg,
            )
            .unwrap() else {
                panic!("");
            };
            assert_eq!(output, cbor!([{"ok": true}]));
            ClientState::Explicit(state)
        };
        static ref RSA_KEYPAIR: crypto::RsaKeyPair =
            crypto::RsaKeyPair::from_pkcs8(RSA_PKCS8).unwrap();
    }

    fn unauthenticated_request(cmd: BTreeMap<MapKey, Value>) -> Vec<u8> {
        let encoded_requests = cbor!([(Value::Map(cmd))]).to_bytes();
        cbor!({ENCODED_REQUESTS: encoded_requests}).to_bytes()
    }

    fn sign_authenticated_request<F>(
        cmd: BTreeMap<MapKey, Value>,
        auth_level: &str,
        sign: F,
    ) -> Vec<u8>
    where
        F: FnOnce(&[u8]) -> Vec<u8>,
    {
        let encoded_requests = cbor!([(Value::Map(cmd))]).to_bytes();
        let encoded_requests_hash = crypto::sha256(&encoded_requests);
        let signed_message =
            vec![TEST_HANDSHAKE_HASH.as_slice(), encoded_requests_hash.as_ref()].concat();
        cbor!({
            DEVICE_ID: (TEST_DEVICE_ID.clone()),
            AUTH_LEVEL: auth_level,
            SIG: (sign(&signed_message).as_slice()),
            ENCODED_REQUESTS: encoded_requests,
        })
        .to_bytes()
    }

    fn authenticated_request(cmd: BTreeMap<MapKey, Value>) -> Vec<u8> {
        sign_authenticated_request(cmd, "hw", |to_be_signed| {
            KEYPAIR.sign(to_be_signed).unwrap().as_ref().to_vec()
        })
    }

    fn sign_request(request: Value) -> Vec<u8> {
        let Value::Map(map) = request else {
            panic!("requests must be maps");
        };
        authenticated_request(map)
    }

    fn get_device_entry(state: ClientState) -> BTreeMap<MapKey, Value> {
        let ClientState::Explicit(state) = state else {
            panic!("");
        };
        let Ok(Value::Map(mut transparent)) = cbor::parse(state.transparent) else {
            panic!("");
        };
        let Some(Value::Map(devices)) = transparent.get_mut(DEVICES_KEY) else {
            panic!("");
        };
        let Some(Value::Map(device)) = devices.remove(&MapKey::Bytestring(TEST_DEVICE_ID.clone()))
        else {
            panic!("");
        };
        device
    }

    #[test]
    fn test_registration_timestamp() {
        let device = get_device_entry(REGISTERED_STATE.clone());
        let Some(Value::Int(timestamp)) = device.get(REGISTER_TIME_KEY) else {
            panic!("");
        };
        assert_eq!(*timestamp, TIMESTAMP);

        if let Some(Value::Int(_timestamp)) = device.get(LAST_USED_KEY) {
            panic!("last_used should not be set");
        }
    }

    #[test]
    fn test_registration() {
        let msg = sign_request(cbor!({CMD: "debug/success"}));
        let device_id = vec![1, 2, 3];
        let (output, state) = process_client_msg(
            REGISTERED_STATE.clone(),
            ExternalContext {
                client_device_identifier: device_id.clone(),
                ..EXTERNAL_CONTEXT.clone()
            },
            TEST_HANDSHAKE_HASH.as_slice(),
            msg,
        )
        .unwrap();
        assert!(is_ok(&output), "{:?}", output);

        let StateUpdate::Minor(new_state) = state else {
            panic!("update from debug request was not minor");
        };
        let device = get_device_entry(ClientState::Explicit(new_state));
        let Some(Value::Int(timestamp)) = device.get(LAST_USED_KEY) else {
            panic!("");
        };
        assert_eq!(*timestamp, TIMESTAMP);
        let Some(Value::Bytestring(client_device_identifier)) =
            device.get(EXTERNAL_DEVICE_IDENTIFIER_KEY)
        else {
            panic!("");
        };
        assert_eq!(*client_device_identifier, device_id)
    }

    #[test]
    fn test_rsa_registration() {
        let Value::Map(cmd) = cbor!({CMD: "debug/success"}) else {
            panic!("!");
        };
        let msg = sign_authenticated_request(cmd, "hw", |to_be_signed| {
            RSA_KEYPAIR.sign(to_be_signed).unwrap().as_ref().to_vec()
        });
        let (output, _state) = process_client_msg(
            RSA_REGISTERED_STATE.clone(),
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg,
        )
        .unwrap();
        assert!(is_ok(&output), "{:?}", output);
    }

    #[test]
    fn test_device_register_twice_matching_keys() {
        // Registering the same device (defined as having the same ID and public keys)
        // is a no-op.
        let encoded_register = cbor!([{
            CMD: "device/register",
            DEVICE_ID: (TEST_DEVICE_ID.clone()),
            PUB_KEYS: {"hw": (SPKI.as_slice())},
        }])
        .to_bytes();
        let msg = cbor!({ENCODED_REQUESTS: encoded_register}).to_bytes();
        let (output, _) = process_client_msg(
            REGISTERED_STATE.clone(),
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg,
        )
        .unwrap();
        assert_eq!(output, cbor!([{"ok": true}]));
    }

    #[test]
    fn test_device_register_twice_mismatching_keys() {
        // Registering different devices (defined by their public keys) with the same ID
        // is an error.
        let encoded_register = cbor!([{
            CMD: "device/register",
            DEVICE_ID: (TEST_DEVICE_ID.clone()),
            PUB_KEYS: {"nothw": (SPKI.as_slice())},
        }])
        .to_bytes();
        let msg = cbor!({ENCODED_REQUESTS: encoded_register}).to_bytes();
        let (output, _) = process_client_msg(
            REGISTERED_STATE.clone(),
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg,
        )
        .unwrap();
        assert_eq!(output, cbor!([{"err": 2}]));
    }

    #[test]
    fn test_device_register_uv_and_uv_pending() {
        // Can't register both a UV key and a UV-pending signal.
        let encoded_register = cbor!([{
            CMD: "device/register",
            DEVICE_ID: (TEST_DEVICE_ID.clone()),
            PUB_KEYS: {"uv": (SPKI.as_slice())},
            UV_KEY_PENDING: true,
        }])
        .to_bytes();
        let msg = cbor!({ENCODED_REQUESTS: encoded_register}).to_bytes();
        let (output, _) = process_client_msg(
            ClientState::Initial,
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg,
        )
        .unwrap();
        assert!(!is_ok(&output));
    }

    #[test]
    fn test_device_register_uv_and_swuv() {
        // Can't register both a UV and SWUV key.
        let encoded_register = cbor!([{
            CMD: "device/register",
            DEVICE_ID: (TEST_DEVICE_ID.clone()),
            PUB_KEYS: {"uv": (SPKI.as_slice()), "swuv": (SPKI.as_slice())},
        }])
        .to_bytes();
        let msg = cbor!({ENCODED_REQUESTS: encoded_register}).to_bytes();
        let (output, _) = process_client_msg(
            ClientState::Initial,
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg,
        )
        .unwrap();
        assert!(!is_ok(&output));
    }

    #[test]
    fn test_device_register_software_uv_and_uv_pending() {
        // Can't register both a software UV key and a UV-pending signal.
        let encoded_register = cbor!([{
            CMD: "device/register",
            DEVICE_ID: (TEST_DEVICE_ID.clone()),
            PUB_KEYS: {"swuv": (SPKI.as_slice())},
            UV_KEY_PENDING: true,
        }])
        .to_bytes();
        let msg = cbor!({ENCODED_REQUESTS: encoded_register}).to_bytes();
        let (output, _) = process_client_msg(
            ClientState::Initial,
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg,
        )
        .unwrap();
        assert!(!is_ok(&output));
    }

    #[test]
    fn test_device_add_uv_key_without_uv_pending() {
        // add_uv_key should fail if the device didn't opt to later add a UV
        // key at registration time.
        let encoded_register = cbor!([{
            CMD: "device/add_uv_key",
            PUB_KEY: (SPKI.as_slice()),
        }])
        .to_bytes();
        let msg = cbor!({ENCODED_REQUESTS: encoded_register}).to_bytes();
        let (output, _) = process_client_msg(
            REGISTERED_STATE.clone(),
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg,
        )
        .unwrap();

        assert!(!is_ok(&output));
    }

    #[test]
    fn test_subsequent_uv() {
        let msg = sign_request(cbor!({
            CMD: "device/add_uv_key",
            PUB_KEY: (SPKI.as_slice()),
        }));
        let (output, StateUpdate::Major(state)) = process_client_msg(
            REGISTERED_STATE_UV_PENDING.clone(),
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg,
        )
        .unwrap() else {
            panic!("")
        };
        assert!(is_ok(&output));

        // Doing the same command a second time is fine if the public key
        // matches.
        let msg = sign_request(cbor!({
            CMD: "device/add_uv_key",
            PUB_KEY: (SPKI.as_slice()),
        }));
        let (output, _update) = process_client_msg(
            ClientState::Explicit(state.clone()),
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg,
        )
        .unwrap();
        assert!(is_ok(&output));

        // ... but it fails if the public key is different.
        let msg = sign_request(cbor!({
            CMD: "device/add_uv_key",
            PUB_KEY: RSA_SPKI,
        }));
        let (output, _update) = process_client_msg(
            ClientState::Explicit(state.clone()),
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg,
        )
        .unwrap();
        assert!(!is_ok(&output));

        // The UV key should work now.
        let Value::Map(cmd) = cbor!({CMD: "debug/success"}) else {
            panic!("!");
        };
        let msg = sign_authenticated_request(cmd, "uv", |to_be_signed| {
            KEYPAIR.sign(to_be_signed).unwrap().as_ref().to_vec()
        });
        let (output, _update) = process_client_msg(
            ClientState::Explicit(state),
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg,
        )
        .unwrap();
        assert!(is_ok(&output), "{:?}", output);
    }

    #[test]
    fn test_device_forget() {
        let msg = sign_request(cbor!({
            CMD: "device/forget",
            DEVICE_ID: (TEST_DEVICE_ID.clone()),
        }));
        let (output, _state) = process_client_msg(
            REGISTERED_STATE.clone(),
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg,
        )
        .unwrap();
        assert!(is_ok(&output), "{:?}", output);
    }

    #[test]
    fn test_keys_genpair() {
        let msg = sign_request(cbor!({
            CMD: "keys/genpair",
            PURPOSE: "not yet used",
        }));
        let (output, _state) = process_client_msg(
            REGISTERED_STATE.clone(),
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg.clone(),
        )
        .unwrap();

        let Value::Map(response) = ok_value(&output).unwrap() else {
            panic!("{:?}", output);
        };

        assert!(matches!(response.get(PUB_KEY_KEY), Some(Value::Bytestring(_))));
        assert!(matches!(response.get(PRIV_KEY_KEY), Some(Value::Bytestring(_))));
        // No way to use the generated key pair yet.
    }

    #[test]
    fn test_passkeys_assert() {
        let msg = sign_request(cbor!({
            CMD: "passkeys/assert",
            WRAPPED_SECRET: (REGISTERED_STATE_WRAPPED_SECRET.as_slice()),
            PROTOBUF: (PROTOBUF_BYTES.to_vec()),
            CLIENT_DATA_JSON: r#"{"type": "webauthn.get", challenge: "1234", "origin": "example.com"}"#,
            WEBAUTHN_REQUEST: {
                RP_ID: "example.com",
            },
        }));
        let (output, _state) = process_client_msg(
            REGISTERED_STATE.clone(),
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg.clone(),
        )
        .unwrap();
        assert!(is_ok(&output), "{:?}", output);
    }

    #[test]
    fn test_passkeys_create() {
        // Test that we can successfully assert the credential that was
        // created with "passkeys/create".

        let msg = sign_request(cbor!({
            CMD: "passkeys/assert",
            WRAPPED_SECRET: (REGISTERED_STATE_WRAPPED_SECRET.as_slice()),
            PROTOBUF: (ENTITY_PROTOBUF_BYTES.clone()),
            CLIENT_DATA_JSON: r#"{"type": "webauthn.get", challenge: "1234", "origin": "example.com"}"#,
            WEBAUTHN_REQUEST: {
                RP_ID: "example.com",
            },
        }));
        let (output, _state) = process_client_msg(
            REGISTERED_STATE.clone(),
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg.clone(),
        )
        .unwrap();
        assert!(is_ok(&output), "{:?}", output);
    }

    #[test]
    fn test_both_wrapped_and_unwrapped() {
        let msg = sign_request(cbor!({
            CMD: "passkeys/assert",
            // Providing _both_ a wrapped and unwrapped secret should fail.
            WRAPPED_SECRET: (REGISTERED_STATE_WRAPPED_SECRET.as_slice()),
            SECRET: SAMPLE_SECURITY_DOMAIN_SECRET,
            PROTOBUF: (ENTITY_PROTOBUF_BYTES.clone()),
            CLIENT_DATA_JSON: r#"{"type": "webauthn.get", challenge: "1234", "origin": "example.com"}"#,
            WEBAUTHN_REQUEST: {
                RP_ID: "example.com",
            },
        }));
        let (output, _state) = process_client_msg(
            REGISTERED_STATE.clone(),
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg.clone(),
        )
        .unwrap();
        assert!(!is_ok(&output));
        let error = single_error_string(&output).unwrap();
        assert!(error.contains("both wrapped and unwrapped"), "{:?}", output);
    }

    fn seal_aes_256_gcm(key: &[u8; 32], plaintext: &[u8], aad: &[u8]) -> Vec<u8> {
        let mut plaintext = plaintext.to_vec();
        let mut nonce = [0u8; 12];
        crypto::rand_bytes(&mut nonce);
        crypto::aes_256_gcm_seal_in_place(&key, &nonce, aad, &mut plaintext);

        [nonce.as_slice(), &plaintext].concat()
    }

    /// Make an assertion with the given claimed PIN. Returns any error that
    /// resulted, the resulting PIN state, and the updated account state.
    fn attempt_pin(
        state: ClientState,
        wrapped_pin_data: &[u8],
        pin_claim: &[u8],
    ) -> (Option<cbor::Value>, PINState, ClientState) {
        let msg = sign_request(cbor!({
            CMD: "passkeys/assert",
            WRAPPED_SECRET: (REGISTERED_STATE_WRAPPED_SECRET.as_slice()),
            PROTOBUF: (ENTITY_PROTOBUF_BYTES.clone()),
            WRAPPED_PIN_DATA: wrapped_pin_data,
            CLAIMED_PIN: pin_claim,
            CLIENT_DATA_JSON: r#"{"type": "webauthn.get", challenge: "1234", "origin": "example.com"}"#,
            WEBAUTHN_REQUEST: {
                RP_ID: "example.com",
            },
        }));
        let (output, state_update) = process_client_msg(
            state.clone(),
            EXTERNAL_CONTEXT.clone(),
            TEST_HANDSHAKE_HASH.as_slice(),
            msg.clone(),
        )
        .unwrap();

        // Get the state after processing the command. That's either a new
        // state, or the original state because no update was made.
        let state_data = match state_update {
            StateUpdate::Minor(state_data) => state_data,
            StateUpdate::Major(state_data) => state_data,
            StateUpdate::None => match state {
                ClientState::Explicit(state_data) => state_data,
                ClientState::Initial => panic!(""),
            },
        };
        let parsed_state = ClientState::Explicit(state_data.clone()).parse().unwrap();
        (
            single_response(&output)
                .unwrap()
                .get(&MapKeyRef::Str("err") as &dyn MapLookupKey)
                .cloned(),
            parsed_state.get_pin_state(&TEST_DEVICE_ID).unwrap(),
            ClientState::Explicit(state_data),
        )
    }

    #[test]
    fn test_use_pin() {
        let pin_data = pin::Data {
            pin_hash: [1u8; 32],
            generation: 1,
            claim_key: [2u8; 32],
            counter_id: [3u8; recovery_key_store::COUNTER_ID_LEN],
            vault_handle_without_type: [4u8; recovery_key_store::VAULT_HANDLE_LEN - 1],
        };
        let wrapped_pin_data = pin_data.encrypt(SAMPLE_SECURITY_DOMAIN_SECRET);
        let pin_claim =
            seal_aes_256_gcm(&pin_data.claim_key, &pin_data.pin_hash, passkeys::PIN_CLAIM_AAD);

        let (error, pin_state, state) =
            attempt_pin(REGISTERED_STATE.clone(), &wrapped_pin_data, &pin_claim);
        assert!(error.is_none());
        assert_eq!(pin_state.attempts, 0);

        // Using the same PIN again shouldn't change anything.
        let (error, pin_state, state) = attempt_pin(state, &wrapped_pin_data, &pin_claim);
        assert!(error.is_none());
        assert_eq!(pin_state.attempts, 0);

        // Trying the wrong PIN should fail and increment the attempts counter.
        let wrong_pin_hash = [20u8; 32];
        let wrong_pin_claim =
            seal_aes_256_gcm(&pin_data.claim_key, &wrong_pin_hash, passkeys::PIN_CLAIM_AAD);
        let (error, pin_state, state) = attempt_pin(state, &wrapped_pin_data, &wrong_pin_claim);
        assert_eq!(error, Some(Value::Int(3)));
        assert_eq!(pin_state.attempts, 1);

        // The correct PIN should reset it again.
        let (error, pin_state, state) = attempt_pin(state, &wrapped_pin_data, &pin_claim);
        assert!(error.is_none());
        assert_eq!(pin_state.attempts, 0);

        // The wrong PIN five times in a row should lock the device.
        let (_error, _pin_state, state) = attempt_pin(state, &wrapped_pin_data, &wrong_pin_claim);
        let (_error, _pin_state, state) = attempt_pin(state, &wrapped_pin_data, &wrong_pin_claim);
        let (_error, _pin_state, state) = attempt_pin(state, &wrapped_pin_data, &wrong_pin_claim);
        let (_error, _pin_state, state) = attempt_pin(state, &wrapped_pin_data, &wrong_pin_claim);
        let (error, pin_state, state) = attempt_pin(state, &wrapped_pin_data, &wrong_pin_claim);
        assert_eq!(error, Some(Value::Int(3)));
        assert_eq!(pin_state.attempts, 5);

        // Now the wrong PIN will generate a different error and not increment the
        // counter.
        let (error, pin_state, state) = attempt_pin(state, &wrapped_pin_data, &wrong_pin_claim);
        assert_eq!(error, Some(Value::Int(4)));
        assert_eq!(pin_state.attempts, 5);

        // And so will the correct PIN.
        let (error, pin_state, _state) = attempt_pin(state, &wrapped_pin_data, &pin_claim);
        assert_eq!(error, Some(Value::Int(4)));
        assert_eq!(pin_state.attempts, 5);
    }

    #[test]
    fn test_wrap_pin() {
        // Wrap a PIN and then attempt to use it.
        let pin_hash = [1u8; 32];
        let claim_key = [2u8; 32];
        let counter_id = [3u8; recovery_key_store::COUNTER_ID_LEN];
        let vault_handle_without_type = [4u8; recovery_key_store::VAULT_HANDLE_LEN - 1];
        let msg = sign_request(cbor!({
            CMD: "passkeys/wrap_pin",
            PIN_HASH: (&pin_hash),
            PIN_GENERATION: 1,
            PIN_CLAIM_KEY: (&claim_key),
            COUNTER_ID: (&counter_id),
            VAULT_HANDLE_WITHOUT_TYPE: (&vault_handle_without_type),
            WRAPPED_SECRET: (REGISTERED_STATE_WRAPPED_SECRET.clone()),
        }));
        let (output, _) = process_client_msg(
            REGISTERED_STATE.clone(),
            ExternalContext { is_reauthenticated: true, ..EXTERNAL_CONTEXT.clone() },
            TEST_HANDSHAKE_HASH.as_slice(),
            msg.clone(),
        )
        .unwrap();
        let Value::Bytestring(wrapped_pin_data) = ok_value(&output).unwrap() else {
            panic!("{:?}", output);
        };

        let pin_claim = seal_aes_256_gcm(&claim_key, &pin_hash, passkeys::PIN_CLAIM_AAD);
        let (error, pin_state, _) =
            attempt_pin(REGISTERED_STATE.clone(), &wrapped_pin_data, &pin_claim);
        assert!(error.is_none());
        assert_eq!(pin_state.attempts, 0);
    }

    fn is_single_error_response(value: &Value) -> bool {
        let Value::Array(array) = value else {
            return false;
        };
        matches!(&array[..], [Value::Map(map)] if map.contains_key(ERR_KEY))
    }

    fn single_response(value: &Value) -> Option<&BTreeMap<cbor::MapKey, cbor::Value>> {
        let Value::Array(array) = value else {
            return None;
        };
        let [first] = &array[..] else {
            return None;
        };
        let Value::Map(map) = first else {
            return None;
        };
        Some(map)
    }

    fn ok_value(value: &Value) -> Option<&cbor::Value> {
        single_response(value)?.get(&MapKeyRef::Str("ok") as &dyn MapLookupKey)
    }

    fn is_ok(value: &Value) -> bool {
        ok_value(value).is_some()
    }

    /// Return the error string from the single response in `value`.
    fn single_error_string(value: &Value) -> Option<&str> {
        let error = single_response(value)?.get(&MapKeyRef::Str("err") as &dyn MapLookupKey)?;
        let Value::String(error) = error else {
            return None;
        };
        Some(error)
    }

    // Automated mutation of requests:
    //
    // In order to test invalid versions of requests, the following test
    // infrastructure can mutate maps: removing entries, replacing them with
    // values of a different type, and replacing them with invalid values.
    //
    // First some utilities for mutating generic maps are provided, then those
    // are used for producing mutated requests.

    /// The result of applying a single mutation to a map.
    struct MutatedMap {
        /// The mutated map.
        map: BTreeMap<MapKey, Value>,
        /// Whether making a request with this map should result in an error.
        should_fail: bool,
        /// A string describing the mutation, for debugging.
        debug: String,
    }

    /// Instruction for mutating a specific value in a map.
    #[derive(Default)]
    struct MutationConfig {
        /// Whether the value is optional. If so, removing it shouldn't cause
        /// a request to fail.
        is_optional: bool,
        /// An optional list of invalid values that should cause failures.
        invalid_values: Option<Vec<Value>>,
        /// If the value itself is a map, instructions for recursively mutating
        /// it.
        subconfig: Option<Box<BTreeMap<String, MutationConfig>>>,
    }

    /// Applies mutations to each key of the given string-keyed map and returns
    /// a vector of mutated maps.
    fn mutate_map(
        map: &BTreeMap<MapKey, Value>,
        configs: &BTreeMap<String, MutationConfig>,
    ) -> Vec<MutatedMap> {
        let default_config: MutationConfig = Default::default();
        let mut ret: Vec<MutatedMap> = Vec::new();

        for key in map.keys() {
            let MapKey::String(key_str) = key else {
                panic!("only string-keyed maps expected");
            };

            let config = configs.get(key_str).unwrap_or(&default_config);

            // First, trying removing the value.
            let mut mutated = map.clone();
            mutated.remove(key);
            ret.push(MutatedMap {
                map: mutated.clone(),
                should_fail: !config.is_optional,
                debug: format!("removed {key_str}"),
            });

            // Next, try making it a different type.
            let mut mutated = map.clone();
            let Some(value) = mutated.remove(key as &dyn MapLookupKey) else {
                panic!("impossible");
            };
            mutated.insert(
                key.clone(),
                match value {
                    Value::String(_) => Value::Boolean(true),
                    Value::Bytestring(_) => Value::Boolean(true),
                    Value::Array(_) => Value::Boolean(true),
                    Value::Map(_) => Value::Boolean(true),
                    Value::Int(_) => Value::Boolean(true),
                    Value::Boolean(_) => Value::Int(42),
                },
            );
            ret.push(MutatedMap {
                map: mutated,
                should_fail: !config.is_optional,
                debug: format!("mutated {key_str}"),
            });

            // If any specific, invalid values were provided, try those.
            if let Some(invalid_values) = &config.invalid_values {
                for value in invalid_values {
                    let mut mutated = map.clone();
                    mutated.insert(key.clone(), value.clone());
                    ret.push(MutatedMap {
                        map: mutated,
                        should_fail: true,
                        debug: format!("invalid for {key_str}"),
                    });
                }
            }

            // If a configuration was provided for mutating the value itself,
            // try all those variants.
            if let Some(subconfig) = &config.subconfig {
                let mut mutated = map.clone();
                let Some(Value::Map(map)) = mutated.remove(key) else {
                    panic!("subconfig provided for non-map {key_str}");
                };
                for mutation in mutate_map(&map, subconfig) {
                    mutated.insert(key.clone(), Value::Map(mutation.map));
                    ret.push(MutatedMap {
                        map: mutated.clone(),
                        should_fail: mutation.should_fail,
                        debug: format!("mutating {key_str}: {}", mutation.debug),
                    });
                }
            }
        }

        ret
    }

    /// The result of mutating a request.
    ///
    /// This mirrors `MutatedMap`, but the values are serialized requests.
    struct MutatedRequest {
        request: Vec<u8>,
        should_fail: bool,
        debug: String,
    }

    /// An enum that describes whether a specific request need be authenticated.
    enum RequestAuthentication {
        Required,
        Never,
    }

    /// Mutate a given request and return a vector of mutated requests.
    fn mutate_request(
        request: &BTreeMap<MapKey, Value>,
        authentication: RequestAuthentication,
        configs: &BTreeMap<String, MutationConfig>,
    ) -> Vec<MutatedRequest> {
        let serialize = if matches!(authentication, RequestAuthentication::Never) {
            unauthenticated_request
        } else {
            authenticated_request
        };
        let mut ret: Vec<MutatedRequest> = Vec::new();

        // First, check that the unmodified request is successful.

        ret.push(MutatedRequest {
            request: serialize(request.clone()),
            should_fail: false,
            debug: String::from("unmodified"),
        });

        // Next, if the request requires authentication, check that an
        // unauthenticated request fails.

        if !matches!(authentication, RequestAuthentication::Never) {
            ret.push(MutatedRequest {
                request: unauthenticated_request(request.clone()),
                should_fail: true,
                debug: String::from("unauthenticated"),
            });
        }

        // Finally, mutate the request map itself.

        ret.extend(mutate_map(request, configs).into_iter().map(|mutated_map| MutatedRequest {
            request: serialize(mutated_map.map),
            should_fail: mutated_map.should_fail,
            debug: mutated_map.debug,
        }));
        ret
    }

    fn test_invalid_requests(
        request: &Value,
        initial_state: ClientState,
        authentication: RequestAuthentication,
        configs: &BTreeMap<String, MutationConfig>,
    ) {
        let Value::Map(request) = request else {
            panic!("requests must be maps");
        };
        for mutated_request in mutate_request(request, authentication, configs) {
            let (output, _state) = process_client_msg(
                initial_state.clone(),
                ExternalContext { is_reauthenticated: true, ..EXTERNAL_CONTEXT.clone() },
                TEST_HANDSHAKE_HASH.as_slice(),
                mutated_request.request,
            )
            .unwrap();

            if mutated_request.should_fail {
                assert!(
                    is_single_error_response(&output),
                    "{}: {:?}",
                    mutated_request.debug,
                    output
                );
            } else {
                assert!(is_ok(&output), "{}: {:?}", mutated_request.debug, output);
            }
        }
    }

    #[test]
    fn test_invalid_device_register() {
        let request = cbor!({
            CMD: "device/register",
            DEVICE_ID: (TEST_DEVICE_ID.clone()),
            PUB_KEYS: {"hw": (SPKI.as_slice())},
        });
        let configs = BTreeMap::from([
            (
                String::from(DEVICE_ID),
                MutationConfig {
                    invalid_values: Some(vec![bytes((0..=255).collect())]),
                    ..Default::default()
                },
            ),
            (
                String::from(PUB_KEYS),
                MutationConfig {
                    subconfig: Some(Box::new(Default::default())),
                    ..Default::default()
                },
            ),
        ]);

        test_invalid_requests(
            &request,
            ClientState::Initial,
            RequestAuthentication::Never,
            &configs,
        );
    }

    #[test]
    fn test_invalid_device_add_uv_key() {
        let request = cbor!({
            CMD: "device/add_uv_key",
            PUB_KEY: (SPKI.as_slice()),
        });
        let configs = BTreeMap::from([]);

        test_invalid_requests(
            &request,
            REGISTERED_STATE_UV_PENDING.clone(),
            RequestAuthentication::Required,
            &configs,
        );
    }

    #[test]
    fn test_invalid_passkeys_assert() {
        let request = cbor!({
            CMD: "passkeys/assert",
            WRAPPED_SECRET: (REGISTERED_STATE_WRAPPED_SECRET.as_slice()),
            PROTOBUF: PROTOBUF_BYTES,
            CLIENT_DATA_JSON: r#"{"type": "webauthn.get", challenge: "1234", "origin": "example.com"}"#,
            WEBAUTHN_REQUEST: {
                RP_ID: "example.com",
            },
        });
        let configs = BTreeMap::from([
            (
                String::from(passkeys::PROTOBUF),
                MutationConfig {
                    invalid_values: Some(vec![bytes((0..128).collect())]),
                    ..Default::default()
                },
            ),
            (
                String::from(passkeys::WEBAUTHN_REQUEST),
                MutationConfig { subconfig: Some(Box::new(BTreeMap::new())), ..Default::default() },
            ),
        ]);

        test_invalid_requests(
            &request,
            REGISTERED_STATE.clone(),
            RequestAuthentication::Required,
            &configs,
        );
    }

    #[test]
    fn test_invalid_passkeys_create() {
        let request = cbor!({
            CMD: "passkeys/create",
            WRAPPED_SECRET: (REGISTERED_STATE_WRAPPED_SECRET.clone()),
            WEBAUTHN_REQUEST: {
                PUB_KEY_CRED_PARAMS: [{
                    COSE_ALGORITHM: (-7),
                }],
            },
        });
        let configs = BTreeMap::from([(
            String::from(passkeys::COSE_ALGORITHM),
            MutationConfig {
                invalid_values: Some(vec![Value::Array(vec![Value::Int(-1)])]),
                ..Default::default()
            },
        )]);

        test_invalid_requests(
            &request,
            REGISTERED_STATE.clone(),
            RequestAuthentication::Required,
            &configs,
        );
    }

    #[test]
    fn test_invalid_passkeys_wrap_pin() {
        let pin_hash = [1u8; 32];
        let claim_key = [2u8; 32];
        let counter_id = [3u8; recovery_key_store::COUNTER_ID_LEN];
        let vault_handle_without_type = [4u8; recovery_key_store::VAULT_HANDLE_LEN - 1];
        let request = cbor!({
            CMD: "passkeys/wrap_pin",
            PIN_HASH: (&pin_hash),
            PIN_GENERATION: 1,
            PIN_CLAIM_KEY: (&claim_key),
            COUNTER_ID: (&counter_id),
            VAULT_HANDLE_WITHOUT_TYPE: (&vault_handle_without_type),
            WRAPPED_SECRET: (REGISTERED_STATE_WRAPPED_SECRET.clone()),
        });
        let configs = BTreeMap::from([]);

        test_invalid_requests(
            &request,
            REGISTERED_STATE.clone(),
            RequestAuthentication::Required,
            &configs,
        );
    }

    #[test]
    fn test_invalid_recovery_key_store_wrap() {
        let pin_hash = [1u8; 32];
        let request = cbor!({
            CMD: "recovery_key_store/wrap",
            PIN_HASH: (&pin_hash),
            CERT_XML: (recovery_key_store::SAMPLE_CERTS_XML),
            SIG_XML: (recovery_key_store::SAMPLE_SIG_XML),
        });
        let configs = BTreeMap::from([]);

        test_invalid_requests(
            &request,
            REGISTERED_STATE.clone(),
            RequestAuthentication::Never,
            &configs,
        );
    }

    #[test]
    fn test_invalid_recovery_key_store_wrap_as_member() {
        let pin_hash = [1u8; 32];
        let request = cbor!({
            CMD: "recovery_key_store/wrap_as_member",
            PIN_HASH: (&pin_hash),
            CERT_XML: (recovery_key_store::SAMPLE_CERTS_XML),
            SIG_XML: (recovery_key_store::SAMPLE_SIG_XML),
            WRAPPED_SECRET: (REGISTERED_STATE_WRAPPED_SECRET.clone()),
            COUNTER_ID: (&[3u8; recovery_key_store::COUNTER_ID_LEN]),
            VAULT_HANDLE_WITHOUT_TYPE: (&[4u8; recovery_key_store::VAULT_HANDLE_LEN - 1]),
        });
        let configs = BTreeMap::from([]);

        test_invalid_requests(
            &request,
            REGISTERED_STATE.clone(),
            RequestAuthentication::Required,
            &configs,
        );
    }

    #[test]
    fn test_invalid_recovery_key_store_rewrap() {
        let pin_data = pin::Data {
            pin_hash: [1u8; 32],
            generation: 1,
            claim_key: [2u8; 32],
            counter_id: [3u8; recovery_key_store::COUNTER_ID_LEN],
            vault_handle_without_type: [4u8; recovery_key_store::VAULT_HANDLE_LEN - 1],
        };
        let wrapped_pin_data = pin_data.encrypt(SAMPLE_SECURITY_DOMAIN_SECRET);

        let request = cbor!({
            CMD: "recovery_key_store/rewrap",
            CERT_XML: (recovery_key_store::SAMPLE_CERTS_XML),
            SIG_XML: (recovery_key_store::SAMPLE_SIG_XML),
            WRAPPED_SECRET: (REGISTERED_STATE_WRAPPED_SECRET.clone()),
            WRAPPED_PIN_DATA: wrapped_pin_data,
        });
        let configs = BTreeMap::from([]);

        test_invalid_requests(
            &request,
            REGISTERED_STATE.clone(),
            RequestAuthentication::Required,
            &configs,
        );
    }
}
