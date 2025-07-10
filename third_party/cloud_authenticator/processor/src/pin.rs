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

use super::{open_aes_256_gcm, RequestError};
use crate::recovery_key_store;
use alloc::{collections::btree_map::BTreeMap, vec::Vec};
use cbor::{cbor, MapKey, Value};

// The HKDF "info" parameter used when deriving a PIN data key from a security
// domain secret.
pub(crate) const KEY_PURPOSE_PIN_DATA_KEY: &[u8] =
    b"KeychainApplicationKey:chrome:GPM PIN data wrapping key";

#[derive(PartialEq, Debug)]
pub(crate) struct VaultCohortDetails {
    /// The serial number for the cert.xml file last used to select a cohort.
    pub cert_xml_serial_number: i64,
    // The cohort public key selected for the Vault entry for the PIN.
    pub cohort_public_key: Vec<u8>,
}

impl TryFrom<&alloc::collections::BTreeMap<cbor::MapKey, cbor::Value>> for VaultCohortDetails {
    type Error = ();

    fn try_from(
        map: &alloc::collections::BTreeMap<cbor::MapKey, cbor::Value>,
    ) -> Result<VaultCohortDetails, Self::Error> {
        let Some(Value::Int(cert_xml_serial_number)) = map.get(&MapKey::Int(6)) else {
            return Err(());
        };
        let Some(Value::Bytestring(cohort_public_key)) = map.get(&MapKey::Int(7)) else {
            return Err(());
        };
        Ok(VaultCohortDetails {
            cert_xml_serial_number: *cert_xml_serial_number,
            cohort_public_key: cohort_public_key.to_vec(),
        })
    }
}

/// A representation of the PIN data after unwrapping with the
/// security domain secret.
#[derive(PartialEq, Debug)]
pub(crate) struct Data {
    /// The hash of the PIN. Usually hashed with scrypt, but this code only
    /// deals with hashes and so isn't affected by the choice of algorithm so
    /// long as the output is 256 bits long.
    pub pin_hash: [u8; 32],
    /// An AES-256-GCM key used to encrypt claimed PIN hashes.
    pub claim_key: [u8; 32],
    /// The recovery key store counter ID. Used when refreshing the recovery
    /// key store.
    pub counter_id: [u8; recovery_key_store::COUNTER_ID_LEN],
    /// The recovery key store vault handle. Doesn't include the type byte. Used
    /// when refreshing the recovery key store.
    pub vault_handle_without_type: [u8; recovery_key_store::VAULT_HANDLE_LEN - 1],
    // Details of the Vault cohort that corresponds to this PIN. Some clients
    // may not set this value.
    pub vault_cohort_details: Option<VaultCohortDetails>,
}

impl TryFrom<Vec<u8>> for Data {
    type Error = ();

    /// Parse a `Data` from CBOR bytes.
    fn try_from(pin_data: Vec<u8>) -> Result<Data, Self::Error> {
        let parsed = cbor::parse(pin_data).map_err(|_| ())?;
        let Value::Map(map) = parsed else {
            return Err(());
        };
        let Some(Value::Bytestring(pin_hash)) = map.get(&MapKey::Int(1)) else {
            return Err(());
        };
        // 2 corresponded to the generation, which was removed. Do not reuse.
        let Some(Value::Bytestring(claim_key)) = map.get(&MapKey::Int(3)) else {
            return Err(());
        };
        let Some(Value::Bytestring(counter_id)) = map.get(&MapKey::Int(4)) else {
            return Err(());
        };
        let Some(Value::Bytestring(vault_handle)) = map.get(&MapKey::Int(5)) else {
            return Err(());
        };
        let vault_cohort_details = match VaultCohortDetails::try_from(&map) {
            Ok(result) => Some(result),
            Err(_) => None,
        };
        Ok(Data {
            pin_hash: pin_hash.as_ref().try_into().map_err(|_| ())?,
            claim_key: claim_key.as_ref().try_into().map_err(|_| ())?,
            counter_id: counter_id.as_ref().try_into().map_err(|_| ())?,
            vault_handle_without_type: vault_handle.as_ref().try_into().map_err(|_| ())?,
            vault_cohort_details,
        })
    }
}

impl Data {
    /// Serialize as CBOR.
    #[allow(unused_parens)]
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut map: BTreeMap<cbor::MapKey, cbor::Value> = BTreeMap::default();
        map.insert(cbor::MapKey::Int(1), cbor!((&self.pin_hash)));
        // 2 corresponded to the generation, which was removed. Do not reuse.
        map.insert(cbor::MapKey::Int(3), cbor!((&self.claim_key)));
        map.insert(cbor::MapKey::Int(4), cbor!((&self.counter_id)));
        map.insert(
            cbor::MapKey::Int(5),
            cbor!((&self.vault_handle_without_type)),
        );
        if let Some(vault_cohort_details) = &self.vault_cohort_details {
            map.insert(
                cbor::MapKey::Int(6),
                cbor!((vault_cohort_details.cert_xml_serial_number)),
            );
            map.insert(
                cbor::MapKey::Int(7),
                cbor!((vault_cohort_details.cohort_public_key.clone())),
            );
        }
        cbor::Value::from(map).to_bytes()
    }

    /// Decrypt a wrapped PIN.
    pub fn from_wrapped(
        wrapped_pin_data: &[u8],
        security_domain_secret: &[u8],
    ) -> Result<Data, RequestError> {
        let mut pin_data_key = [0u8; 32];
        // unwrap: this only fails if the output is too long, but the output length
        // is fixed at 32.
        crypto::hkdf_sha256(
            security_domain_secret,
            &[],
            KEY_PURPOSE_PIN_DATA_KEY,
            &mut pin_data_key,
        )
        .unwrap();
        open_aes_256_gcm(&pin_data_key, wrapped_pin_data, &[])
            .ok_or(RequestError::Debug("PIN data decryption failed"))?
            .try_into()
            .map_err(|_| RequestError::Debug("invalid PIN data"))
    }

    /// Wrap PIN `Data`.
    pub fn encrypt(&self, security_domain_secret: &[u8]) -> Vec<u8> {
        let mut pin_data_key = [0u8; 32];
        // unwrap: this only fails if the output is too long, but the output length
        // is fixed at 32.
        crypto::hkdf_sha256(
            security_domain_secret,
            &[],
            KEY_PURPOSE_PIN_DATA_KEY,
            &mut pin_data_key,
        )
        .unwrap();

        let mut nonce = [0u8; crypto::NONCE_LEN];
        crypto::rand_bytes(&mut nonce);
        let mut serialized = self.to_bytes();
        crypto::aes_256_gcm_seal_in_place(&pin_data_key, &nonce, &[], &mut serialized);
        [nonce.as_ref(), &serialized].concat()
    }
}
