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

//! The `crypto` crate abstracts over various crypto implementations.
//!
//! The ring implementation is the default and is used in production. The
//! BoringSSL implementation avoids needing to build ring in Chromium when
//! the enclave code is used with unit tests. The rustcrypto implementation
//! isn't fully complete, but can be used to compile to wasm, which we might
//! use in the future.

#![no_std]
#![allow(clippy::result_unit_err)]

extern crate alloc;

pub const NONCE_LEN: usize = 12;
pub const SHA1_OUTPUT_LEN: usize = 20;
pub const SHA256_OUTPUT_LEN: usize = 32;

/// The length of an uncompressed, X9.62 encoding of a P-256 point.
pub const P256_X962_LENGTH: usize = 65;

/// The length of a P-256 scalar value.
pub const P256_SCALAR_LENGTH: usize = 32;

#[cfg(feature = "rustcrypto")]
pub use crate::rustcrypto::{
    aes_256_gcm_open_in_place, aes_256_gcm_seal_in_place, ecdsa_verify, hkdf_sha256,
    p256_scalar_mult, rand_bytes, sha256, sha256_two_part, EcdsaKeyPair, P256Scalar,
};

#[cfg(feature = "ring")]
pub use crate::ringimpl::{
    aes_128_gcm_open_in_place, aes_128_gcm_seal_in_place, aes_256_gcm_open_in_place,
    aes_256_gcm_seal_in_place, ecdsa_verify, hkdf_sha256, hmac_sha256, p256_scalar_mult,
    rand_bytes, rsa_verify, sha1_two_part, sha256, sha256_two_part, EcdsaKeyPair, P256Scalar,
    RsaKeyPair,
};

#[cfg(feature = "bssl")]
pub use crate::bsslimpl::{
    aes_128_gcm_open_in_place, aes_128_gcm_seal_in_place, aes_256_gcm_open_in_place,
    aes_256_gcm_seal_in_place, ecdsa_verify, hkdf_sha256, hmac_sha256, p256_scalar_mult,
    rand_bytes, rsa_verify, sha1_two_part, sha256, sha256_two_part, EcdsaKeyPair, P256Scalar,
    RsaKeyPair,
};

#[cfg(feature = "rustcrypto")]
mod rustcrypto {
    use crate::rustcrypto::ecdsa::signature::Verifier;
    extern crate aes_gcm;
    extern crate ecdsa;
    extern crate hkdf;
    extern crate pkcs8;
    extern crate primeorder;
    extern crate sha2;

    use aes_gcm::{AeadInPlace, KeyInit};
    use p256::ecdsa::signature::Signer;
    use pkcs8::{DecodePrivateKey, EncodePrivateKey};
    use primeorder::PrimeField;
    use sha2::Digest;

    use crate::{
        NONCE_LEN, P256_SCALAR_LENGTH, P256_X962_LENGTH, SHA1_OUTPUT_LEN, SHA256_OUTPUT_LEN,
    };
    use primeorder::elliptic_curve::ops::{Mul, MulByGenerator};
    use primeorder::elliptic_curve::sec1::{FromEncodedPoint, ToEncodedPoint};
    use primeorder::Field;

    use alloc::vec::Vec;

    pub fn rand_bytes(output: &mut [u8]) {
        panic!("unimplemented");
    }

    /// Perform the HKDF operation from https://datatracker.ietf.org/doc/html/rfc5869
    pub fn hkdf_sha256(ikm: &[u8], salt: &[u8], info: &[u8], output: &mut [u8]) -> Result<(), ()> {
        hkdf::Hkdf::<sha2::Sha256>::new(Some(salt), ikm).expand(info, output).map_err(|_| ())
    }

    pub fn aes_256_gcm_seal_in_place(
        key: &[u8; 32],
        nonce: &[u8; NONCE_LEN],
        aad: &[u8],
        plaintext: &mut Vec<u8>,
    ) {
        aes_gcm::Aes256Gcm::new_from_slice(key.as_slice())
            .unwrap()
            .encrypt_in_place(nonce.into(), aad, plaintext)
            .unwrap();
    }

    pub fn aes_256_gcm_open_in_place(
        key: &[u8; 32],
        nonce: &[u8; NONCE_LEN],
        aad: &[u8],
        mut ciphertext: Vec<u8>,
    ) -> Result<Vec<u8>, ()> {
        aes_gcm::Aes256Gcm::new_from_slice(key.as_slice())
            .unwrap()
            .decrypt_in_place(nonce.into(), aad, &mut ciphertext)
            .map_err(|_| ())?;
        Ok(ciphertext)
    }

    pub fn sha256(input: &[u8]) -> [u8; SHA256_OUTPUT_LEN] {
        let mut ctx = sha2::Sha256::new();
        ctx.update(input);
        ctx.finalize().into()
    }

    /// Compute the SHA-256 hash of the concatenation of two inputs.
    pub fn sha256_two_part(input1: &[u8], input2: &[u8]) -> [u8; SHA256_OUTPUT_LEN] {
        let mut ctx = sha2::Sha256::new();
        ctx.update(input1);
        ctx.update(input2);
        ctx.finalize().into()
    }

    pub struct P256Scalar {
        v: p256::Scalar,
    }

    impl P256Scalar {
        pub fn generate() -> P256Scalar {
            let mut ret = [0u8; P256_SCALAR_LENGTH];
            // Warning: not very random.
            ret[0] = 1;
            P256Scalar { v: p256::Scalar::from_repr(ret.into()).unwrap() }
        }

        pub fn compute_public_key(&self) -> [u8; P256_X962_LENGTH] {
            p256::ProjectivePoint::mul_by_generator(&self.v)
                .to_encoded_point(false)
                .as_bytes()
                .try_into()
                .unwrap()
        }

        pub fn bytes(&self) -> [u8; P256_SCALAR_LENGTH] {
            self.v.to_repr().as_slice().try_into().unwrap()
        }
    }

    impl TryFrom<&[u8]> for P256Scalar {
        type Error = ();

        fn try_from(bytes: &[u8]) -> Result<Self, ()> {
            let array: [u8; P256_SCALAR_LENGTH] = bytes.try_into().map_err(|_| ())?;
            (&array).try_into()
        }
    }

    impl TryFrom<&[u8; P256_SCALAR_LENGTH]> for P256Scalar {
        type Error = ();

        fn try_from(bytes: &[u8; P256_SCALAR_LENGTH]) -> Result<Self, ()> {
            let scalar = p256::Scalar::from_repr((*bytes).into());
            if !bool::from(scalar.is_some()) {
                return Err(());
            }
            let scalar = scalar.unwrap();
            if scalar.is_zero_vartime() {
                return Err(());
            }
            Ok(P256Scalar { v: scalar })
        }
    }

    pub fn p256_scalar_mult(
        scalar: &P256Scalar,
        point: &[u8; P256_X962_LENGTH],
    ) -> Result<[u8; 32], ()> {
        let point = p256::EncodedPoint::from_bytes(point).map_err(|_| ())?;
        let affine_point = p256::AffinePoint::from_encoded_point(&point);
        if !bool::from(affine_point.is_some()) {
            // The peer's point is considered public input and so we don't need to work in
            // constant time.
            return Err(());
        }
        // unwrap: `is_some` checked just above.
        let result = affine_point.unwrap().mul(scalar.v).to_encoded_point(false);
        let x = result.x().ok_or(())?;
        // unwrap: the length of a P256 field-element had better be 32 bytes.
        Ok(x.as_slice().try_into().unwrap())
    }

    pub struct EcdsaKeyPair {
        key_pair: p256::ecdsa::SigningKey,
    }

    impl EcdsaKeyPair {
        pub fn from_pkcs8(pkcs8: &[u8]) -> Result<EcdsaKeyPair, ()> {
            let key_pair: p256::ecdsa::SigningKey =
                p256::ecdsa::SigningKey::from_pkcs8_der(pkcs8).map_err(|_| ())?;
            Ok(EcdsaKeyPair { key_pair })
        }

        pub fn generate_pkcs8() -> Result<impl AsRef<[u8]>, ()> {
            // WARNING: not actually a random scalar
            let mut scalar = [0u8; P256_SCALAR_LENGTH];
            scalar[0] = 42;
            let non_zero_scalar = p256::NonZeroScalar::from_repr(scalar.into()).unwrap();
            let key = p256::ecdsa::SigningKey::from(non_zero_scalar);
            Ok(key.to_pkcs8_der().map_err(|_| ())?.to_bytes())
        }

        pub fn public_key(&self) -> impl AsRef<[u8]> + '_ {
            p256::ecdsa::VerifyingKey::from(&self.key_pair).to_sec1_bytes()
        }

        pub fn sign(&self, signed_data: &[u8]) -> Result<impl AsRef<[u8]>, ()> {
            let sig: ecdsa::Signature<p256::NistP256> = self.key_pair.sign(signed_data);
            Ok(sig.to_der())
        }
    }

    pub fn ecdsa_verify(pub_key: &[u8], signed_data: &[u8], signature: &[u8]) -> bool {
        let signature = match ecdsa::der::Signature::from_bytes(signature) {
            Ok(signature) => signature,
            Err(_) => return false,
        };
        let key = match p256::ecdsa::VerifyingKey::from_sec1_bytes(pub_key) {
            Ok(key) => key,
            Err(_) => return false,
        };
        key.verify(signed_data, &signature).is_ok()
    }
}

#[cfg(feature = "ring")]
mod ringimpl {
    use crate::ringimpl::ring::signature::KeyPair;
    extern crate prng;
    extern crate ring;
    extern crate untrusted;

    use crate::{
        NONCE_LEN, P256_SCALAR_LENGTH, P256_X962_LENGTH, SHA1_OUTPUT_LEN, SHA256_OUTPUT_LEN,
    };
    use alloc::vec;
    use alloc::vec::Vec;
    use ring::rand::SecureRandom;
    use ring::signature::VerificationAlgorithm;

    const PRNG: prng::Generator = prng::Generator(());

    pub fn rand_bytes(output: &mut [u8]) {
        // unwrap: the PRNG had better not fail otherwise we can't do much.
        PRNG.fill(output).unwrap();
    }

    pub fn hkdf_sha256(ikm: &[u8], salt: &[u8], info: &[u8], output: &mut [u8]) -> Result<(), ()> {
        ring::hkdf::hkdf(ring::hkdf::HKDF_SHA256, ikm, salt, info, output).map_err(|_| ())
    }

    pub fn aes_128_gcm_seal_in_place(
        key: &[u8; 16],
        nonce: &[u8; NONCE_LEN],
        aad: &[u8],
        plaintext: &mut Vec<u8>,
    ) {
        let key = ring::aead::UnboundKey::new(&ring::aead::AES_128_GCM, key).unwrap();
        let key = ring::aead::LessSafeKey::new(key);
        key.seal_in_place_append_tag(
            ring::aead::Nonce::assume_unique_for_key(*nonce),
            ring::aead::Aad::from(aad),
            plaintext,
        )
        .unwrap();
    }

    pub fn aes_128_gcm_open_in_place(
        key: &[u8; 16],
        nonce: &[u8; NONCE_LEN],
        aad: &[u8],
        mut ciphertext: Vec<u8>,
    ) -> Result<Vec<u8>, ()> {
        let key = ring::aead::UnboundKey::new(&ring::aead::AES_128_GCM, key).unwrap();
        let key = ring::aead::LessSafeKey::new(key);
        let plaintext_len = key
            .open_in_place(
                ring::aead::Nonce::assume_unique_for_key(*nonce),
                ring::aead::Aad::from(aad),
                &mut ciphertext,
            )
            .map_err(|_| ())?
            .len();
        ciphertext.resize(plaintext_len, 0u8);
        Ok(ciphertext)
    }

    pub fn aes_256_gcm_seal_in_place(
        key: &[u8; 32],
        nonce: &[u8; NONCE_LEN],
        aad: &[u8],
        plaintext: &mut Vec<u8>,
    ) {
        let key = ring::aead::UnboundKey::new(&ring::aead::AES_256_GCM, key).unwrap();
        let key = ring::aead::LessSafeKey::new(key);
        key.seal_in_place_append_tag(
            ring::aead::Nonce::assume_unique_for_key(*nonce),
            ring::aead::Aad::from(aad),
            plaintext,
        )
        .unwrap();
    }

    pub fn aes_256_gcm_open_in_place(
        key: &[u8; 32],
        nonce: &[u8; NONCE_LEN],
        aad: &[u8],
        mut ciphertext: Vec<u8>,
    ) -> Result<Vec<u8>, ()> {
        let key = ring::aead::UnboundKey::new(&ring::aead::AES_256_GCM, key).unwrap();
        let key = ring::aead::LessSafeKey::new(key);
        let plaintext_len = key
            .open_in_place(
                ring::aead::Nonce::assume_unique_for_key(*nonce),
                ring::aead::Aad::from(aad),
                &mut ciphertext,
            )
            .map_err(|_| ())?
            .len();
        ciphertext.resize(plaintext_len, 0u8);
        Ok(ciphertext)
    }

    /// Compute the SHA-1 hash of the concatenation of two inputs.
    pub fn sha1_two_part(input1: &[u8], input2: &[u8]) -> [u8; SHA1_OUTPUT_LEN] {
        let mut ctx = ring::digest::Context::new(&ring::digest::SHA1_FOR_LEGACY_USE_ONLY);
        ctx.update(input1);
        ctx.update(input2);
        ctx.finish().as_ref().try_into().unwrap()
    }

    pub fn sha256(input: &[u8]) -> [u8; SHA256_OUTPUT_LEN] {
        let mut ctx = ring::digest::Context::new(&ring::digest::SHA256);
        ctx.update(input);
        ctx.finish().as_ref().try_into().unwrap()
    }

    pub fn sha256_two_part(input1: &[u8], input2: &[u8]) -> [u8; SHA256_OUTPUT_LEN] {
        let mut ctx = ring::digest::Context::new(&ring::digest::SHA256);
        ctx.update(input1);
        ctx.update(input2);
        ctx.finish().as_ref().try_into().unwrap()
    }

    pub fn hmac_sha256(key: &[u8], msg: &[u8]) -> [u8; SHA256_OUTPUT_LEN] {
        let key = ring::hmac::Key::new(ring::hmac::HMAC_SHA256, key);
        // unwrap: HMAC-SHA256 has to produce output of the correct length.
        ring::hmac::sign(&key, msg).as_ref().try_into().unwrap()
    }

    pub struct P256Scalar {
        v: ring::agreement::EphemeralPrivateKey,
    }

    impl P256Scalar {
        pub fn generate() -> P256Scalar {
            P256Scalar {
                v: ring::agreement::EphemeralPrivateKey::generate(
                    &ring::agreement::ECDH_P256,
                    &PRNG,
                )
                .unwrap(),
            }
        }

        pub fn compute_public_key(&self) -> [u8; P256_X962_LENGTH] {
            // unwrap: only returns an error if the input length is incorrect, but it isn't.
            self.v.compute_public_key().unwrap().as_ref().try_into().unwrap()
        }

        pub fn bytes(&self) -> [u8; P256_SCALAR_LENGTH] {
            self.v.bytes().as_ref().try_into().unwrap()
        }

        /// This exists only because ring insists on consuming the private key
        /// during DH operations.
        fn clone_scalar(&self) -> ring::agreement::EphemeralPrivateKey {
            ring::agreement::EphemeralPrivateKey::from_bytes(
                &ring::agreement::ECDH_P256,
                &self.bytes(),
            )
            .unwrap()
        }
    }

    impl TryFrom<&[u8]> for P256Scalar {
        type Error = ();

        fn try_from(bytes: &[u8]) -> Result<Self, ()> {
            let array: [u8; P256_SCALAR_LENGTH] = bytes.try_into().map_err(|_| ())?;
            (&array).try_into()
        }
    }

    impl TryFrom<&[u8; P256_SCALAR_LENGTH]> for P256Scalar {
        type Error = ();

        fn try_from(bytes: &[u8; P256_SCALAR_LENGTH]) -> Result<Self, ()> {
            let scalar = ring::agreement::EphemeralPrivateKey::from_bytes(
                &ring::agreement::ECDH_P256,
                bytes,
            )
            .map_err(|_| ())?;
            Ok(P256Scalar { v: scalar })
        }
    }

    pub fn p256_scalar_mult(
        scalar: &P256Scalar,
        point: &[u8; P256_X962_LENGTH],
    ) -> Result<[u8; 32], ()> {
        // unwrap: only returns an error if the input length is incorrect, but it isn't.
        ring::agreement::agree_ephemeral(
            scalar.clone_scalar(),
            &ring::agreement::UnparsedPublicKey::new(&ring::agreement::ECDH_P256, point),
            (),
            |key| Ok(key.try_into().unwrap()),
        )
        .map_err(|_| ())
    }

    pub struct EcdsaKeyPair {
        key_pair: ring::signature::EcdsaKeyPair,
    }

    impl EcdsaKeyPair {
        pub fn from_pkcs8(pkcs8: &[u8]) -> Result<EcdsaKeyPair, ()> {
            let key_pair = ring::signature::EcdsaKeyPair::from_pkcs8(
                &ring::signature::ECDSA_P256_SHA256_ASN1_SIGNING,
                pkcs8,
                &PRNG,
            )
            .map_err(|_| ())?;
            Ok(EcdsaKeyPair { key_pair })
        }

        pub fn generate_pkcs8() -> impl AsRef<[u8]> {
            ring::signature::EcdsaKeyPair::generate_pkcs8(
                &ring::signature::ECDSA_P256_SHA256_ASN1_SIGNING,
                &PRNG,
            )
            .unwrap()
        }

        pub fn public_key(&self) -> impl AsRef<[u8]> + '_ {
            self.key_pair.public_key()
        }

        pub fn sign(&self, signed_data: &[u8]) -> Result<impl AsRef<[u8]>, ()> {
            self.key_pair.sign(&PRNG, signed_data).map_err(|_| ())
        }
    }

    pub fn ecdsa_verify(pub_key: &[u8], signed_data: &[u8], signature: &[u8]) -> bool {
        ring::signature::ECDSA_P256_SHA256_ASN1
            .verify(
                untrusted::Input::from(pub_key),
                untrusted::Input::from(signed_data),
                untrusted::Input::from(signature),
            )
            .is_ok()
    }

    pub struct RsaKeyPair {
        key_pair: ring::signature::RsaKeyPair,
    }

    impl RsaKeyPair {
        pub fn from_pkcs8(pkcs8: &[u8]) -> Result<RsaKeyPair, ()> {
            let key_pair = ring::signature::RsaKeyPair::from_pkcs8(pkcs8).map_err(|_| ())?;
            Ok(RsaKeyPair { key_pair })
        }

        pub fn sign(&self, to_be_signed: &[u8]) -> Result<impl AsRef<[u8]>, ()> {
            let mut signature = vec![0; self.key_pair.public_modulus_len()];
            self.key_pair
                .sign(&ring::signature::RSA_PKCS1_SHA256, &PRNG, to_be_signed, &mut signature)
                .unwrap();
            Ok(signature)
        }
    }

    pub fn rsa_verify(pub_key: &[u8], signed_data: &[u8], signature: &[u8]) -> bool {
        ring::signature::RSA_PKCS1_2048_8192_SHA256
            .verify(
                untrusted::Input::from(pub_key),
                untrusted::Input::from(signed_data),
                untrusted::Input::from(signature),
            )
            .is_ok()
    }
}

// This implementation uses the bssl-crypto crate from the BoringSSL
// distribution. This is used for testing within Chromium.
#[cfg(feature = "bssl")]
mod bsslimpl {
    #![allow(clippy::upper_case_acronyms)]

    use crate::{
        NONCE_LEN, P256_SCALAR_LENGTH, P256_X962_LENGTH, SHA1_OUTPUT_LEN, SHA256_OUTPUT_LEN,
    };
    use alloc::vec::Vec;
    use bssl_crypto::aead::{Aead, Aes128Gcm, Aes256Gcm};
    use bssl_crypto::ec::P256;
    use bssl_crypto::hkdf::HkdfSha256;
    use bssl_crypto::hmac::HmacSha256;
    use bssl_crypto::{digest, ecdh, ecdsa, hkdf, rsa};

    const GCM_TAG_LEN: usize = 16;

    pub fn rand_bytes(output: &mut [u8]) {
        bssl_crypto::rand_bytes(output)
    }

    pub fn aes_128_gcm_open_in_place(
        key: &[u8; 16],
        nonce: &[u8; NONCE_LEN],
        aad: &[u8],
        mut ciphertext: Vec<u8>,
    ) -> Result<Vec<u8>, ()> {
        if ciphertext.len() < GCM_TAG_LEN {
            return Err(());
        }
        let ciphertext_len = ciphertext.len() - GCM_TAG_LEN;
        let (ciphertext_slice, tag) = ciphertext.split_at_mut(ciphertext_len);

        let tag: &[u8; GCM_TAG_LEN] = &tag.try_into().unwrap();
        let aead = Aes128Gcm::new(key);
        aead.open_in_place(nonce, ciphertext_slice, tag, aad).map_err(|_| ())?;
        ciphertext.resize(ciphertext_len, 0u8);
        Ok(ciphertext)
    }

    pub fn aes_128_gcm_seal_in_place(
        key: &[u8; 16],
        nonce: &[u8; NONCE_LEN],
        aad: &[u8],
        plaintext: &mut Vec<u8>,
    ) {
        let tag = Aes128Gcm::new(key).seal_in_place(nonce, plaintext, aad);
        plaintext.extend_from_slice(&tag);
    }

    pub fn aes_256_gcm_open_in_place(
        key: &[u8; 32],
        nonce: &[u8; NONCE_LEN],
        aad: &[u8],
        mut ciphertext: Vec<u8>,
    ) -> Result<Vec<u8>, ()> {
        if ciphertext.len() < GCM_TAG_LEN {
            return Err(());
        }
        let ciphertext_len = ciphertext.len() - GCM_TAG_LEN;
        let (ciphertext_slice, tag) = ciphertext.split_at_mut(ciphertext_len);

        let tag: &[u8; GCM_TAG_LEN] = &tag.try_into().unwrap();
        let aead = Aes256Gcm::new(key);
        aead.open_in_place(nonce, ciphertext_slice, tag, aad).map_err(|_| ())?;
        ciphertext.resize(ciphertext_len, 0u8);
        Ok(ciphertext)
    }

    pub fn aes_256_gcm_seal_in_place(
        key: &[u8; 32],
        nonce: &[u8; NONCE_LEN],
        aad: &[u8],
        plaintext: &mut Vec<u8>,
    ) {
        let tag = Aes256Gcm::new(key).seal_in_place(nonce, plaintext, aad);
        plaintext.extend_from_slice(&tag);
    }

    pub fn hkdf_sha256(ikm: &[u8], salt: &[u8], info: &[u8], output: &mut [u8]) -> Result<(), ()> {
        HkdfSha256::derive_into(ikm, hkdf::Salt::NonEmpty(salt), info, output).map_err(|_| ())
    }

    pub fn hmac_sha256(key: &[u8], msg: &[u8]) -> [u8; SHA256_OUTPUT_LEN] {
        HmacSha256::mac(key, msg)
    }

    pub fn sha1_two_part(input1: &[u8], input2: &[u8]) -> [u8; SHA1_OUTPUT_LEN] {
        let mut ctx = digest::InsecureSha1::new();
        ctx.update(input1);
        ctx.update(input2);
        ctx.digest()
    }

    pub fn sha256(input: &[u8]) -> [u8; SHA256_OUTPUT_LEN] {
        digest::Sha256::hash(input)
    }

    pub fn sha256_two_part(input1: &[u8], input2: &[u8]) -> [u8; SHA256_OUTPUT_LEN] {
        let mut ctx = digest::Sha256::new();
        ctx.update(input1);
        ctx.update(input2);
        ctx.digest()
    }

    pub struct P256Scalar(ecdh::PrivateKey<P256>);

    impl P256Scalar {
        pub fn generate() -> Self {
            Self(ecdh::PrivateKey::generate())
        }

        pub fn compute_public_key(&self) -> [u8; P256_X962_LENGTH] {
            self.0.to_x962_uncompressed().as_ref().try_into().unwrap()
        }

        pub fn bytes(&self) -> [u8; P256_SCALAR_LENGTH] {
            self.0.to_big_endian().as_ref().try_into().unwrap()
        }
    }

    impl TryFrom<&[u8]> for P256Scalar {
        type Error = ();

        fn try_from(bytes: &[u8]) -> Result<Self, ()> {
            Ok(Self(ecdh::PrivateKey::from_big_endian(bytes).ok_or(())?))
        }
    }

    impl TryFrom<&[u8; P256_SCALAR_LENGTH]> for P256Scalar {
        type Error = ();

        fn try_from(bytes: &[u8; P256_SCALAR_LENGTH]) -> Result<Self, ()> {
            Ok(Self(ecdh::PrivateKey::from_big_endian(bytes).ok_or(())?))
        }
    }

    pub fn p256_scalar_mult(
        scalar: &P256Scalar,
        encoded_point: &[u8; P256_X962_LENGTH],
    ) -> Result<[u8; 32], ()> {
        let pub_key = ecdh::PublicKey::from_x962_uncompressed(encoded_point).ok_or(())?;
        Ok(scalar.0.compute_shared_key(&pub_key).try_into().unwrap())
    }

    pub struct EcdsaKeyPair(ecdsa::PrivateKey<P256>);

    impl EcdsaKeyPair {
        pub fn from_pkcs8(pkcs8: &[u8]) -> Result<EcdsaKeyPair, ()> {
            Ok(Self(ecdsa::PrivateKey::from_der_private_key_info(pkcs8).ok_or(())?))
        }

        pub fn generate_pkcs8() -> impl AsRef<[u8]> {
            ecdsa::PrivateKey::<P256>::generate().to_der_private_key_info()
        }

        pub fn public_key(&self) -> impl AsRef<[u8]> + '_ {
            self.0.to_x962_uncompressed()
        }

        pub fn sign(&self, signed_data: &[u8]) -> Result<impl AsRef<[u8]>, ()> {
            Ok(self.0.sign(signed_data))
        }
    }

    pub fn ecdsa_verify(pub_key: &[u8], signed_data: &[u8], signature: &[u8]) -> bool {
        let Some(pub_key) = ecdsa::PublicKey::<P256>::from_x962_uncompressed(pub_key) else {
            return false;
        };
        pub_key.verify(signed_data, signature).is_ok()
    }

    pub struct RsaKeyPair(rsa::PrivateKey);

    impl RsaKeyPair {
        pub fn from_pkcs8(pkcs8: &[u8]) -> Result<RsaKeyPair, ()> {
            Ok(Self(rsa::PrivateKey::from_der_private_key_info(pkcs8).ok_or(())?))
        }

        pub fn sign(&self, signed_data: &[u8]) -> Result<impl AsRef<[u8]>, ()> {
            Ok(self.0.sign_pkcs1::<digest::Sha256>(signed_data))
        }
    }

    pub fn rsa_verify(pub_key: &[u8], signed_data: &[u8], signature: &[u8]) -> bool {
        let Some(pub_key) = rsa::PublicKey::from_der_rsa_public_key(pub_key) else {
            return false;
        };
        pub_key.verify_pkcs1::<digest::Sha256>(signed_data, signature).is_ok()
    }

    #[cfg(test)]
    mod test {
        use super::*;

        #[test]
        fn test_rand_bytes() {
            let mut buf = [0u8; 16];
            rand_bytes(&mut buf);
        }

        #[test]
        fn test_aes_256_gcm() {
            let key = [1u8; 32];
            let nonce = [2u8; 12];
            let aad = [3u8; 16];
            let mut plaintext = Vec::new();
            plaintext.resize(50, 4u8);
            let mut ciphertext = plaintext.clone();
            aes_256_gcm_seal_in_place(&key, &nonce, &aad, &mut ciphertext);
            let plaintext2 = aes_256_gcm_open_in_place(&key, &nonce, &aad, ciphertext).unwrap();
            assert_eq!(plaintext, plaintext2);
        }

        #[test]
        fn test_ecdh() {
            let priv1 = P256Scalar::generate();
            let pub1 = priv1.compute_public_key();
            let priv2 = P256Scalar::generate();
            let pub2 = priv2.compute_public_key();
            let shared1 = p256_scalar_mult(&priv1, &pub2).unwrap();
            let shared2 = p256_scalar_mult(&priv2, &pub1).unwrap();
            assert_eq!(shared1, shared2);

            let priv3: P256Scalar = (&priv1.bytes()).try_into().unwrap();
            let pub3 = priv3.compute_public_key();
            let shared3 = p256_scalar_mult(&priv2, &pub3).unwrap();
            assert_eq!(shared1, shared3);
        }

        #[test]
        fn test_ecdsa() {
            let pkcs8 = EcdsaKeyPair::generate_pkcs8();
            let priv_key = EcdsaKeyPair::from_pkcs8(pkcs8.as_ref()).unwrap();
            let pub_key = priv_key.public_key();
            let signed_message = [42u8; 20];
            let signature = priv_key.sign(&signed_message).unwrap();
            assert!(ecdsa_verify(pub_key.as_ref(), &signed_message, signature.as_ref()));
        }
    }
}
