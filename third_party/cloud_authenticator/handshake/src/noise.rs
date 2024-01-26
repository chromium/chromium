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

extern crate crypto;

use crate::error::Error;
use alloc::vec::Vec;

const SYMMETRIC_KEY_SIZE: usize = 32;
const NONCE_LEN: usize = 12;

#[derive(Debug, PartialEq)]
pub enum HandshakeType {
    Nk, // https://noiseexplorer.com/patterns/NK/
}

// Helper to generate 2 keys.
fn hkdf2(
    ck: &[u8; SYMMETRIC_KEY_SIZE],
    ikm: &[u8],
) -> ([u8; SYMMETRIC_KEY_SIZE], [u8; SYMMETRIC_KEY_SIZE]) {
    let mut output = [0; crypto::SHA256_OUTPUT_LEN * 2];
    // unwrap: only fails if the output size is too large, but the output
    // size is small and fixed here.
    crypto::hkdf_sha256(ikm, ck, &[], &mut output).unwrap();
    let key1: [u8; SYMMETRIC_KEY_SIZE] = output[..SYMMETRIC_KEY_SIZE].try_into().unwrap();
    let key2: [u8; SYMMETRIC_KEY_SIZE] = output[SYMMETRIC_KEY_SIZE..].try_into().unwrap();
    (key1, key2)
}

#[derive(Debug, PartialEq)]
pub struct Noise {
    chaining_key: [u8; SYMMETRIC_KEY_SIZE],
    h: [u8; SYMMETRIC_KEY_SIZE],
    symmetric_key: [u8; SYMMETRIC_KEY_SIZE],
    symmetric_nonce: u32,
}

impl Noise {
    pub fn new(handshake_type: HandshakeType) -> Self {
        assert_eq!(handshake_type, HandshakeType::Nk);
        let mut chaining_key_in = [0; SYMMETRIC_KEY_SIZE];
        let nk_protocol_name = b"Noise_NK_P256_AESGCM_SHA256";
        chaining_key_in[..nk_protocol_name.len()].copy_from_slice(nk_protocol_name);
        Noise {
            chaining_key: chaining_key_in,
            h: chaining_key_in,
            symmetric_key: [0; SYMMETRIC_KEY_SIZE],
            symmetric_nonce: 0,
        }
    }

    pub fn mix_hash(&mut self, in_data: &[u8]) {
        // See https://www.noiseprotocol.org/noise.html#the-symmetricstate-object
        self.h = crypto::sha256_two_part(&self.h, in_data);
    }

    pub fn mix_key(&mut self, ikm: &[u8]) {
        // See https://www.noiseprotocol.org/noise.html#the-symmetricstate-object
        let derived_keys = hkdf2(&self.chaining_key, ikm);
        self.chaining_key.copy_from_slice(&derived_keys.0);
        self.initialize_key(&derived_keys.1);
    }

    #[cfg(test)]
    pub fn mix_key_and_hash(&mut self, ikm: &[u8]) {
        // See https://www.noiseprotocol.org/noise.html#the-symmetricstate-object
        let mut output = [0; SYMMETRIC_KEY_SIZE * 3];
        // unwrap: only fails if the output size is too large, but the output
        // size is small and fixed here.
        crypto::hkdf_sha256(&ikm, &self.chaining_key, &[], &mut output).unwrap();
        self.chaining_key.copy_from_slice(&output[..SYMMETRIC_KEY_SIZE]);
        self.mix_hash(&output[SYMMETRIC_KEY_SIZE..SYMMETRIC_KEY_SIZE * 2]);
        self.initialize_key(&output[SYMMETRIC_KEY_SIZE * 2..].try_into().unwrap());
    }

    fn next_nonce(&mut self) -> [u8; NONCE_LEN] {
        let mut nonce_bytes = [0; NONCE_LEN];
        nonce_bytes[0] = (self.symmetric_nonce >> 24) as u8;
        nonce_bytes[1] = (self.symmetric_nonce >> 16) as u8;
        nonce_bytes[2] = (self.symmetric_nonce >> 8) as u8;
        nonce_bytes[3] = self.symmetric_nonce as u8;
        self.symmetric_nonce += 1;
        nonce_bytes
    }

    pub fn encrypt_and_hash(&mut self, plaintext: &[u8]) -> Vec<u8> {
        let mut encrypted_data = Vec::from(plaintext);
        let nonce = self.next_nonce();
        crypto::aes_256_gcm_seal_in_place(
            &self.symmetric_key,
            &nonce,
            &self.h,
            &mut encrypted_data,
        );
        self.mix_hash(&encrypted_data);
        encrypted_data
    }

    pub fn decrypt_and_hash(&mut self, ciphertext: &[u8]) -> Result<Vec<u8>, Error> {
        let h = self.h;
        self.mix_hash(ciphertext);

        let ciphertext = Vec::from(ciphertext);
        let nonce = self.next_nonce();
        let plaintext =
            crypto::aes_256_gcm_open_in_place(&self.symmetric_key, &nonce, &h, ciphertext)
                .map_err(|_| Error::DecryptFailed)?;
        Ok(plaintext)
    }

    pub fn handshake_hash(&self) -> [u8; SYMMETRIC_KEY_SIZE] {
        self.h
    }

    // |point| is the contents from either agreement::PublicKey::as_ref() or
    // agreement::UnparsedPublicKey::bytes().
    pub fn mix_hash_point(&mut self, point: &[u8]) {
        self.mix_hash(point);
    }

    pub fn traffic_keys(&self) -> ([u8; SYMMETRIC_KEY_SIZE], [u8; SYMMETRIC_KEY_SIZE]) {
        hkdf2(&self.chaining_key, &[])
    }

    fn initialize_key(&mut self, key: &[u8; SYMMETRIC_KEY_SIZE]) {
        // See https://www.noiseprotocol.org/noise.html#the-cipherstate-object
        self.symmetric_key.copy_from_slice(key);
        self.symmetric_nonce = 0;
    }
}

#[cfg(test)]
mod tests {
    extern crate hex;

    use super::*;

    // The golden values embedded in these tests were generated by using the
    // Noise code from the reference implementation:
    // http://google3/experimental/users/agl/cablespec/noise.go;l=1;rcl=549756285
    #[test]
    fn mix_hash() {
        let mut noise = Noise::new(HandshakeType::Nk);
        noise.mix_hash("mixHash".as_bytes());
        assert_eq!(
            hex::encode(noise.handshake_hash()),
            "04d999779401b40a318f8729c99bec79cc15ec375f4a0bb3de1a00965b61d666"
        );
    }

    #[test]
    fn mix_hash_point() {
        let mut noise = Noise::new(HandshakeType::Nk);
        let x962_point: [u8; 65] = [
            0x04, 0x6f, 0x40, 0x4f, 0xbd, 0xa2, 0x1f, 0x6f, 0x26, 0x26, 0x11, 0xe2, 0x00, 0x5c,
            0x57, 0x14, 0x21, 0x72, 0x5c, 0xcb, 0xe8, 0xdd, 0x88, 0xfd, 0xd3, 0x63, 0xb8, 0x20,
            0xde, 0x29, 0x51, 0x67, 0xd0, 0x8d, 0x49, 0x88, 0x07, 0x7e, 0xc5, 0x21, 0x36, 0xd7,
            0x2f, 0x6c, 0xc0, 0x58, 0xee, 0x9a, 0x78, 0x5c, 0xf6, 0xb1, 0x91, 0xc3, 0xd2, 0xaa,
            0x1e, 0x3f, 0x5f, 0x20, 0xb0, 0xea, 0x9b, 0x2b, 0xa0,
        ];
        noise.mix_hash_point(&x962_point);
        assert_eq!(
            hex::encode(noise.handshake_hash()),
            "53741f8d5a69c11d8f6f0865193f15f6d756b0d209fd7116b400a4a8a39439b8"
        );
    }

    #[test]
    fn mix_key_then_encrypt_and_hash() {
        let mut noise = Noise::new(HandshakeType::Nk);
        noise.mix_key("encryptAndHash".as_bytes());
        let ciphertext = noise.encrypt_and_hash("plaintext".as_bytes());
        assert_eq!(hex::encode(ciphertext), "fdf1564256dcee03b5babe81df599ec4273c95e4269d747764");
        assert_eq!(
            hex::encode(noise.handshake_hash()),
            "1cf5b0af5f3365d715e9137382a5fd139a56e25bea8ecdbf3018f42af7432a75"
        );
    }

    #[test]
    fn mix_key_and_hash_then_encrypt_and_hash() {
        let mut noise = Noise::new(HandshakeType::Nk);
        noise.mix_key_and_hash("encryptAndHash".as_bytes());
        let ciphertext = noise.encrypt_and_hash("plaintext".as_bytes());
        assert_eq!(hex::encode(ciphertext), "2693c80637c7fd9e686949c46a4d189d41ba7cf74a53a85752");
        assert_eq!(
            hex::encode(noise.handshake_hash()),
            "b883ed0aa2303502e497c8609a979970bc6292cc316eafa7fc0c434f818b9e01"
        );
    }

    #[test]
    fn decrypt_and_hash() {
        let mut noise = Noise::new(HandshakeType::Nk);
        noise.mix_key("encryptAndHash".as_bytes());
        // This test uses the values from MixKeyThenEncryptAndHash, but in the
        // other direction.
        let plaintext = noise
            .decrypt_and_hash(
                &hex::decode("fdf1564256dcee03b5babe81df599ec4273c95e4269d747764").unwrap(),
            )
            .unwrap();
        assert_eq!(hex::encode(&plaintext), "706c61696e74657874");
        assert_eq!(
            hex::encode(noise.handshake_hash()),
            "1cf5b0af5f3365d715e9137382a5fd139a56e25bea8ecdbf3018f42af7432a75"
        );
    }

    #[test]
    fn split() {
        let mut noise = Noise::new(HandshakeType::Nk);
        noise.mix_key_and_hash("split".as_bytes());
        let keys = noise.traffic_keys();
        assert_eq!(
            hex::encode(keys.0),
            "1f73215029f964ce0a65ef92eaf97bd67c45feff8e49cca94fdf5050aaf25a58"
        );
        assert_eq!(
            hex::encode(keys.1),
            "0c7f90f88233858ecbd6492c273ed328acaef75ebcb31fd2f94033c5d296a623"
        );
    }

    #[test]
    fn combined() {
        let mut noise = Noise::new(HandshakeType::Nk);
        noise.mix_hash("mixHash".as_bytes());
        noise.mix_key("mixKey".as_bytes());
        noise.mix_key_and_hash("mixKeyAndHash".as_bytes());
        let ciphertext = noise.encrypt_and_hash("plaintext".as_bytes());
        assert_eq!(hex::encode(ciphertext), "b7235b3d77c9cf5ebb087793b399de2c2276edae52bba5199e");
        assert_eq!(
            hex::encode(noise.handshake_hash()),
            "db420fd8f9b072eacca4599019303f02b1938fb9096e3b4b063afebc687c9987"
        );
    }
}
