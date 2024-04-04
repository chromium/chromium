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

#![no_std]

#[allow(unused_imports)] // Macros only used in tests.
#[macro_use]
extern crate alloc;
extern crate crypto;
extern crate static_assertions;

mod error;
mod noise;

use crate::error::Error;
use crate::noise::{HandshakeType, Noise};
use alloc::vec::Vec;
use crypto::{P256Scalar, NONCE_LEN, P256_X962_LENGTH};

// This is assumed to be vastly larger than any connection will ever reach.
const MAX_SEQUENCE: u32 = 1u32 << 24;

#[derive(Debug)]
pub struct Crypter {
    read_key: [u8; 32],
    write_key: [u8; 32],
    read_nonce: u32,
    write_nonce: u32,
}

/// Utility for encrypting and decrypting traffic between the Noise endpoints.
/// It is created by |respond| and configured with a key for each traffic
/// direction.
impl Crypter {
    fn new(read_key: &[u8; 32], write_key: &[u8; 32]) -> Self {
        Self { read_key: *read_key, write_key: *write_key, read_nonce: 0, write_nonce: 0 }
    }

    fn next_nonce(nonce: &mut u32) -> Result<[u8; NONCE_LEN], Error> {
        if *nonce > MAX_SEQUENCE {
            return Err(Error::DecryptFailed);
        }
        let mut ret = [0u8; NONCE_LEN];
        ret[NONCE_LEN - 4..].copy_from_slice(nonce.to_be_bytes().as_slice());
        *nonce += 1;
        Ok(ret)
    }

    pub fn encrypt(&mut self, plaintext: &[u8]) -> Result<Vec<u8>, Error> {
        const PADDING_GRANULARITY: usize = 32;
        static_assertions::const_assert!(PADDING_GRANULARITY < 256);
        static_assertions::const_assert!((PADDING_GRANULARITY & (PADDING_GRANULARITY - 1)) == 0);

        let mut padded_size: usize = plaintext.len();
        if padded_size > (1usize << 28) {
            return Err(Error::DataTooLarge(padded_size));
        }
        padded_size += 1; // padding-length byte
        padded_size = (padded_size + PADDING_GRANULARITY - 1) & !(PADDING_GRANULARITY - 1);

        let mut padded_encrypt_data = Vec::with_capacity(padded_size);
        padded_encrypt_data.extend_from_slice(plaintext);
        padded_encrypt_data.resize(padded_size, 0u8);
        let num_zeros = padded_size - plaintext.len() - 1;
        padded_encrypt_data[padded_size - 1] = num_zeros as u8;

        crypto::aes_256_gcm_seal_in_place(
            &self.write_key,
            &Self::next_nonce(&mut self.write_nonce)?,
            &[],
            &mut padded_encrypt_data,
        );
        Ok(padded_encrypt_data)
    }

    pub fn decrypt(&mut self, ciphertext: &[u8]) -> Result<Vec<u8>, Error> {
        let plaintext = crypto::aes_256_gcm_open_in_place(
            &self.read_key,
            &Self::next_nonce(&mut self.read_nonce)?,
            &[],
            Vec::from(ciphertext),
        )
        .map_err(|_| Error::DecryptFailed)?;

        // Plaintext must have a padding byte, and the unpadded length must be
        // at least one.
        if plaintext.is_empty() || (plaintext[plaintext.len() - 1] as usize) >= plaintext.len() {
            return Err(Error::DecryptionPaddingError);
        }
        let unpadded_length = plaintext.len() - (plaintext[plaintext.len() - 1] as usize);
        Ok(Vec::from(&plaintext[0..unpadded_length - 1]))
    }
}

pub struct Response {
    pub crypter: Crypter,
    pub handshake_hash: [u8; 32],
    pub response: Vec<u8>,
}

/// Performs the Responder side of the Noise protocol with the NK pattern.
/// |identity_private_key_bytes| contains the private key scalar for the
/// service's provisioned identity. |in_data| is provided by the Initiator and
/// contains its ephemeral public key and encrypted payload.
///
/// The identity public key is computed from the private key, but could
/// alternatively be stored separately to reduce computation if needed to
/// reduce per-transaction computation.
/// See https://noiseexplorer.com/patterns/NK/
pub fn respond(identity_private_key_bytes: &[u8], in_data: &[u8]) -> Result<Response, Error> {
    if in_data.len() < P256_X962_LENGTH {
        return Err(Error::InvalidHandshake);
    }

    let mut noise = Noise::new(HandshakeType::Nk);
    noise.mix_hash(&[0; 1]); // Prologue

    let identity_scalar: P256Scalar =
        identity_private_key_bytes.try_into().map_err(|_| Error::InvalidPrivateKey)?;
    let identity_pub = identity_scalar.compute_public_key();

    noise.mix_hash_point(identity_pub.as_slice());

    // unwrap: we know that `in_data` is `P256_X962_LENGTH` bytes long.
    let peer_pub: [u8; P256_X962_LENGTH] = (&in_data[..P256_X962_LENGTH]).try_into().unwrap();
    noise.mix_hash(peer_pub.as_slice());
    noise.mix_key(peer_pub.as_slice());

    let es_ecdh_bytes = crypto::p256_scalar_mult(&identity_scalar, &peer_pub)
        .map_err(|_| Error::InvalidHandshake)?;
    noise.mix_key(es_ecdh_bytes.as_slice());

    let plaintext = noise.decrypt_and_hash(&in_data[P256_X962_LENGTH..])?;
    if !plaintext.is_empty() {
        return Err(Error::InvalidHandshake);
    }

    // Generate ephemeral key pair
    let ephemeral_priv = P256Scalar::generate();
    let ephemeral_pub_key_bytes = ephemeral_priv.compute_public_key();
    noise.mix_hash(ephemeral_pub_key_bytes.as_slice());
    noise.mix_key(ephemeral_pub_key_bytes.as_slice());
    let ee_ecdh_bytes = crypto::p256_scalar_mult(&ephemeral_priv, &peer_pub)
        .map_err(|_| Error::InvalidHandshake)?;
    noise.mix_key(ee_ecdh_bytes.as_slice());

    let response_ciphertext = noise.encrypt_and_hash(&[]);

    let keys = noise.traffic_keys();
    Ok(Response {
        crypter: Crypter::new(&keys.0, &keys.1),
        handshake_hash: noise.handshake_hash(),
        response: [ephemeral_pub_key_bytes.as_slice(), &response_ciphertext].concat(),
    })
}

pub mod test_client {
    use super::*;

    pub struct HandshakeInitiator {
        noise: Noise,
        identity_pub_key: [u8; P256_X962_LENGTH],
        ephemeral_priv_key: P256Scalar,
    }

    impl HandshakeInitiator {
        pub fn new(peer_public_key: &[u8; P256_X962_LENGTH]) -> Self {
            Self {
                noise: Noise::new(HandshakeType::Nk),
                identity_pub_key: *peer_public_key,
                ephemeral_priv_key: P256Scalar::generate(),
            }
        }

        pub fn build_initial_message(&mut self) -> Vec<u8> {
            self.noise.mix_hash(&[0; 1]);
            self.noise.mix_hash_point(self.identity_pub_key.as_slice());
            let ephemeral_pub_key = self.ephemeral_priv_key.compute_public_key();
            let ephemeral_pub_key_bytes = ephemeral_pub_key.as_ref();

            self.noise.mix_hash(ephemeral_pub_key_bytes);
            self.noise.mix_key(ephemeral_pub_key_bytes);
            let es_ecdh_bytes =
                crypto::p256_scalar_mult(&self.ephemeral_priv_key, &self.identity_pub_key).unwrap();
            self.noise.mix_key(&es_ecdh_bytes);

            let ciphertext = self.noise.encrypt_and_hash(&[]);
            [ephemeral_pub_key_bytes, &ciphertext].concat()
        }

        pub fn process_response(&mut self, handshake_response: &[u8]) -> ([u8; 32], Crypter) {
            let peer_public_key_bytes = &handshake_response[..P256_X962_LENGTH];
            let ciphertext = &handshake_response[P256_X962_LENGTH..];

            let ee_ecdh_bytes = crypto::p256_scalar_mult(
                &self.ephemeral_priv_key,
                peer_public_key_bytes.try_into().unwrap(),
            )
            .unwrap();
            self.noise.mix_hash(peer_public_key_bytes);
            self.noise.mix_key(peer_public_key_bytes);
            self.noise.mix_key(&ee_ecdh_bytes);

            let plaintext = self.noise.decrypt_and_hash(ciphertext).unwrap();
            assert_eq!(plaintext.len(), 0);
            let (write_key, read_key) = self.noise.traffic_keys();
            (self.noise.handshake_hash(), Crypter::new(&read_key, &write_key))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::test_client::HandshakeInitiator;
    use super::*;

    #[test]
    fn process_handshake() {
        let test_messages = vec![vec![1u8, 2u8, 3u8, 4u8], vec![4u8, 3u8, 2u8, 1u8], vec![]];
        let identity_priv = P256Scalar::generate();
        let identity_pub_bytes = identity_priv.compute_public_key();
        let mut initiator = HandshakeInitiator::new(&identity_pub_bytes);
        let message = initiator.build_initial_message();
        let handshake_response = respond(identity_priv.bytes().as_slice(), &message).unwrap();
        let mut enclave_crypter = handshake_response.crypter;

        let (client_hash, mut client_crypter) =
            initiator.process_response(&handshake_response.response);
        assert_eq!(&client_hash, &handshake_response.handshake_hash);

        // Client -> Enclave encrypt+decrypt
        for message in &test_messages {
            let ciphertext = client_crypter.encrypt(message).unwrap();
            let plaintext = enclave_crypter.decrypt(&ciphertext).unwrap();
            assert_eq!(message, &plaintext);
        }

        // Enclave -> Client encrypt+decrypt
        for message in &test_messages {
            let ciphertext = enclave_crypter.encrypt(message).unwrap();
            let plaintext = client_crypter.decrypt(&ciphertext).unwrap();
            assert_eq!(message, &plaintext);
        }
    }
}
