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
    aes_256_gcm_open_in_place, aes_256_gcm_seal_in_place, ecdsa_verify, hkdf_sha256,
    p256_scalar_mult, rand_bytes, rsa_verify, sha1_two_part, sha256, sha256_two_part, EcdsaKeyPair,
    P256Scalar, RsaKeyPair,
};

#[cfg(feature = "bssl")]
pub use crate::bsslimpl::{
    aes_256_gcm_open_in_place, aes_256_gcm_seal_in_place, ecdsa_verify, hkdf_sha256,
    p256_scalar_mult, rand_bytes, rsa_verify, sha1_two_part, sha256, sha256_two_part, EcdsaKeyPair,
    P256Scalar, RsaKeyPair,
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

#[cfg(feature = "bssl")]
mod bsslimpl {
    #![allow(clippy::upper_case_acronyms)]

    use crate::{
        NONCE_LEN, P256_SCALAR_LENGTH, P256_X962_LENGTH, SHA1_OUTPUT_LEN, SHA256_OUTPUT_LEN,
    };
    use alloc::vec::Vec;
    use libc::{c_int, size_t};

    use bssl_sys::{
        i2d_ECPrivateKey, point_conversion_form_t::POINT_CONVERSION_UNCOMPRESSED, CBS_init,
        ECDH_compute_key, EC_KEY_free, EC_KEY_generate_key, EC_KEY_get0_group,
        EC_KEY_get0_private_key, EC_KEY_get0_public_key, EC_KEY_new, EC_KEY_oct2priv,
        EC_KEY_priv2oct, EC_KEY_set_enc_flags, EC_KEY_set_group, EC_KEY_set_public_key,
        EC_POINT_free, EC_POINT_mul, EC_POINT_new, EC_POINT_oct2point, EC_POINT_point2oct,
        EC_group_p256, EVP_AEAD_CTX_free, EVP_AEAD_CTX_new, EVP_AEAD_CTX_open, EVP_AEAD_CTX_seal,
        EVP_DigestSign, EVP_DigestSignInit, EVP_DigestVerify, EVP_DigestVerifyInit,
        EVP_MD_CTX_free, EVP_MD_CTX_new, EVP_PKEY_assign_EC_KEY, EVP_PKEY_assign_RSA,
        EVP_PKEY_free, EVP_PKEY_get0_EC_KEY, EVP_PKEY_get0_RSA, EVP_PKEY_new, EVP_aead_aes_256_gcm,
        EVP_parse_private_key, EVP_sha256, RAND_bytes, RSA_parse_public_key, CBS, EC_KEY, EVP_PKEY,
        HKDF, SHA1, SHA256,
    };

    const GCM_TAG_LEN: usize = 16;

    pub fn rand_bytes(output: &mut [u8]) {
        unsafe {
            RAND_bytes(output.as_mut_ptr(), output.len());
        }
    }

    pub fn aes_256_gcm_open_in_place(
        key: &[u8; 32],
        nonce: &[u8; NONCE_LEN],
        aad: &[u8],
        mut ciphertext: Vec<u8>,
    ) -> Result<Vec<u8>, ()> {
        unsafe {
            let ctx =
                EVP_AEAD_CTX_new(EVP_aead_aes_256_gcm(), key.as_ptr(), key.len(), GCM_TAG_LEN);
            let mut out_len: size_t = 0;
            let ret = if EVP_AEAD_CTX_open(
                ctx,
                ciphertext.as_mut_ptr(),
                &mut out_len,
                ciphertext.len(),
                nonce.as_ptr(),
                NONCE_LEN,
                ciphertext.as_ptr(),
                ciphertext.len(),
                aad.as_ptr(),
                aad.len(),
            ) != 1
            {
                Err(())
            } else {
                ciphertext.resize(out_len, 0u8);
                Ok(ciphertext)
            };
            EVP_AEAD_CTX_free(ctx);
            ret
        }
    }

    pub fn aes_256_gcm_seal_in_place(
        key: &[u8; 32],
        nonce: &[u8; NONCE_LEN],
        aad: &[u8],
        plaintext: &mut Vec<u8>,
    ) {
        unsafe {
            plaintext.resize(plaintext.len() + GCM_TAG_LEN, 0u8);
            let ctx =
                EVP_AEAD_CTX_new(EVP_aead_aes_256_gcm(), key.as_ptr(), key.len(), GCM_TAG_LEN);
            let mut out_len: size_t = 0;
            if EVP_AEAD_CTX_seal(
                ctx,
                plaintext.as_mut_ptr(),
                &mut out_len,
                plaintext.len(),
                nonce.as_ptr(),
                NONCE_LEN,
                plaintext.as_ptr(),
                plaintext.len() - GCM_TAG_LEN,
                aad.as_ptr(),
                aad.len(),
            ) != 1
            {
                panic!("EVP_AEAD_CTX_seal failed");
            };
            EVP_AEAD_CTX_free(ctx);
        }
    }

    pub fn hkdf_sha256(ikm: &[u8], salt: &[u8], info: &[u8], output: &mut [u8]) -> Result<(), ()> {
        unsafe {
            if HKDF(
                output.as_mut_ptr(),
                output.len(),
                EVP_sha256(),
                ikm.as_ptr(),
                ikm.len(),
                salt.as_ptr(),
                salt.len(),
                info.as_ptr(),
                info.len(),
            ) == 1
            {
                Ok(())
            } else {
                Err(())
            }
        }
    }

    pub fn sha1_two_part(input1: &[u8], input2: &[u8]) -> [u8; SHA1_OUTPUT_LEN] {
        let mut ret = [0u8; SHA1_OUTPUT_LEN];
        let input = [input1, input2].concat();
        unsafe {
            SHA1(input.as_ptr(), input.len(), ret.as_mut_ptr());
        }
        ret
    }

    pub fn sha256(input: &[u8]) -> [u8; SHA256_OUTPUT_LEN] {
        let mut ret = [0u8; SHA256_OUTPUT_LEN];
        unsafe {
            SHA256(input.as_ptr(), input.len(), ret.as_mut_ptr());
        }
        ret
    }

    pub fn sha256_two_part(input1: &[u8], input2: &[u8]) -> [u8; SHA256_OUTPUT_LEN] {
        sha256(&[input1, input2].concat())
    }

    pub struct P256Scalar {
        ec_key: *mut EC_KEY,
    }

    impl P256Scalar {
        pub fn generate() -> P256Scalar {
            unsafe {
                let key = EC_KEY_new();
                assert!(EC_KEY_set_group(key, EC_group_p256()) == 1);
                assert!(EC_KEY_generate_key(key) == 1);
                P256Scalar { ec_key: key }
            }
        }

        pub fn compute_public_key(&self) -> [u8; P256_X962_LENGTH] {
            let mut ret = [0u8; P256_X962_LENGTH];
            unsafe {
                let point = EC_KEY_get0_public_key(self.ec_key);
                assert!(
                    EC_POINT_point2oct(
                        EC_group_p256(),
                        point,
                        POINT_CONVERSION_UNCOMPRESSED,
                        ret.as_mut_ptr(),
                        ret.len(),
                        core::ptr::null_mut()
                    ) == P256_X962_LENGTH
                );
            }
            ret
        }

        pub fn bytes(&self) -> [u8; P256_SCALAR_LENGTH] {
            let mut ret = [0u8; P256_SCALAR_LENGTH];
            unsafe {
                assert!(EC_KEY_priv2oct(self.ec_key, ret.as_mut_ptr(), ret.len()) == ret.len());
            }
            ret
        }
    }

    impl Drop for P256Scalar {
        fn drop(&mut self) {
            unsafe { EC_KEY_free(self.ec_key) }
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
            unsafe {
                let group = EC_group_p256();
                let key = EC_KEY_new();
                assert!(EC_KEY_set_group(key, EC_group_p256()) == 1);
                assert!(EC_KEY_oct2priv(key, bytes.as_ptr(), bytes.len()) == 1);
                let pub_key = EC_POINT_new(group);
                if EC_POINT_mul(
                    group,
                    pub_key,
                    EC_KEY_get0_private_key(key),
                    core::ptr::null(),
                    core::ptr::null(),
                    core::ptr::null_mut(),
                ) != 1
                {
                    EC_KEY_free(key);
                    EC_POINT_free(pub_key);
                    return Err(());
                }
                assert!(EC_KEY_set_public_key(key, pub_key) == 1);
                EC_POINT_free(pub_key);

                Ok(P256Scalar { ec_key: key })
            }
        }
    }

    pub fn p256_scalar_mult(
        scalar: &P256Scalar,
        encoded_point: &[u8; P256_X962_LENGTH],
    ) -> Result<[u8; 32], ()> {
        let mut buf = [0u8; 32];
        unsafe {
            let group = EC_group_p256();
            let point = EC_POINT_new(group);
            let ret = if EC_POINT_oct2point(
                group,
                point,
                encoded_point.as_ptr(),
                encoded_point.len(),
                core::ptr::null_mut(),
            ) != 1
                || ECDH_compute_key(
                    buf.as_mut_ptr() as *mut libc::c_void,
                    buf.len(),
                    point,
                    scalar.ec_key,
                    Option::None,
                ) != buf.len() as c_int
            {
                Err(())
            } else {
                Ok(buf)
            };
            EC_POINT_free(point);
            ret
        }
    }

    fn i2d<T, F>(ctx: *const T, i2d_func: F) -> Vec<u8>
    where
        F: Fn(*const T, *mut *mut u8) -> c_int,
    {
        unsafe {
            let size = i2d_func(ctx, core::ptr::null_mut());
            assert!(size > 0);
            let size = size as usize;
            let mut ret = Vec::with_capacity(size);
            let mut ptr = ret.as_mut_ptr();
            i2d_func(ctx, &mut ptr);
            ret.set_len(size);
            ret
        }
    }

    struct KeyPair {
        pkey: *mut EVP_PKEY,
    }

    unsafe impl Sync for KeyPair {}
    unsafe impl Send for KeyPair {}

    impl KeyPair {
        fn from_pkcs8(pkcs8: &[u8]) -> Result<KeyPair, ()> {
            unsafe {
                let mut cbs = CBS { data: core::ptr::null(), len: 0 };
                CBS_init(&mut cbs, pkcs8.as_ptr(), pkcs8.len());
                let pkey = EVP_parse_private_key(&mut cbs);
                if pkey.is_null() {
                    return Err(());
                }
                Ok(KeyPair { pkey })
            }
        }

        fn is_ecdsa_p256(&self) -> bool {
            unsafe {
                let ec_key = EVP_PKEY_get0_EC_KEY(self.pkey);
                if ec_key.is_null() {
                    return false;
                }
                EC_KEY_get0_group(ec_key) == EC_group_p256()
            }
        }

        fn is_rsa(&self) -> bool {
            unsafe { !EVP_PKEY_get0_RSA(self.pkey).is_null() }
        }

        fn sign(&self, signed_data: &[u8]) -> Result<impl AsRef<[u8]>, ()> {
            unsafe {
                let ctx = EVP_MD_CTX_new();
                let mut sig_len: size_t = 0;
                assert!(
                    EVP_DigestSignInit(
                        ctx,
                        core::ptr::null_mut(),
                        EVP_sha256(),
                        core::ptr::null_mut(),
                        self.pkey
                    ) == 1
                );
                assert!(
                    EVP_DigestSign(
                        ctx,
                        core::ptr::null_mut(),
                        &mut sig_len,
                        signed_data.as_ptr(),
                        signed_data.len()
                    ) == 1
                );
                let mut ret = Vec::with_capacity(sig_len);
                assert!(
                    EVP_DigestSign(
                        ctx,
                        ret.as_mut_ptr(),
                        &mut sig_len,
                        signed_data.as_ptr(),
                        signed_data.len()
                    ) == 1
                );
                ret.set_len(sig_len);
                EVP_MD_CTX_free(ctx);
                Ok(ret)
            }
        }
    }

    impl Drop for KeyPair {
        fn drop(&mut self) {
            unsafe { EVP_PKEY_free(self.pkey) }
        }
    }

    pub struct EcdsaKeyPair {
        keypair: KeyPair,
    }

    impl EcdsaKeyPair {
        pub fn from_pkcs8(pkcs8: &[u8]) -> Result<EcdsaKeyPair, ()> {
            let keypair = KeyPair::from_pkcs8(pkcs8)?;
            if !keypair.is_ecdsa_p256() {
                return Err(());
            }
            Ok(EcdsaKeyPair { keypair })
        }

        pub fn generate_pkcs8() -> impl AsRef<[u8]> {
            unsafe {
                let ec_key = EC_KEY_new();
                assert!(EC_KEY_set_group(ec_key, EC_group_p256()) == 1);
                assert!(EC_KEY_generate_key(ec_key) == 1);
                // Set EC_PKEY_NO_PARAMETERS, since we'll be wrapping in PKCS#8.
                EC_KEY_set_enc_flags(ec_key, 1);

                let serialized = i2d(ec_key, |ctx, ptr| i2d_ECPrivateKey(ctx, ptr));
                const PKCS8_PREFIX : &[u8] = b"\x30\x81\x87\x02\x01\x00\x30\x13\x06\x07\x2a\x86\x48\xce\x3d\x02\x01\x06\x08\x2a\x86\x48\xce\x3d\x03\x01\x07\x04\x6d";
                [PKCS8_PREFIX, &serialized].concat()
            }
        }

        pub fn public_key(&self) -> impl AsRef<[u8]> + '_ {
            unsafe {
                let ec_key = EVP_PKEY_get0_EC_KEY(self.keypair.pkey);
                let point = EC_KEY_get0_public_key(ec_key);
                let mut ret = [0u8; P256_X962_LENGTH];
                assert!(
                    EC_POINT_point2oct(
                        EC_group_p256(),
                        point,
                        POINT_CONVERSION_UNCOMPRESSED,
                        ret.as_mut_ptr(),
                        ret.len(),
                        core::ptr::null_mut()
                    ) == P256_X962_LENGTH
                );
                ret
            }
        }

        pub fn sign(&self, signed_data: &[u8]) -> Result<impl AsRef<[u8]>, ()> {
            self.keypair.sign(signed_data)
        }
    }

    pub fn ecdsa_verify(pub_key: &[u8], signed_data: &[u8], signature: &[u8]) -> bool {
        unsafe {
            let group = EC_group_p256();
            let point = EC_POINT_new(group);
            if EC_POINT_oct2point(
                group,
                point,
                pub_key.as_ptr(),
                pub_key.len(),
                core::ptr::null_mut(),
            ) != 1
            {
                EC_POINT_free(point);
                return false;
            }
            let ec_key = EC_KEY_new();
            EC_KEY_set_group(ec_key, group);
            EC_KEY_set_public_key(ec_key, point);
            EC_POINT_free(point);

            let pkey = EVP_PKEY_new();
            EVP_PKEY_assign_EC_KEY(pkey, ec_key);

            let ctx = EVP_MD_CTX_new();
            assert!(
                EVP_DigestVerifyInit(
                    ctx,
                    core::ptr::null_mut(),
                    EVP_sha256(),
                    core::ptr::null_mut(),
                    pkey
                ) == 1
            );
            let ok = EVP_DigestVerify(
                ctx,
                signature.as_ptr(),
                signature.len(),
                signed_data.as_ptr(),
                signed_data.len(),
            ) == 1;
            EVP_MD_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            ok
        }
    }

    pub struct RsaKeyPair {
        keypair: KeyPair,
    }

    impl RsaKeyPair {
        pub fn from_pkcs8(pkcs8: &[u8]) -> Result<RsaKeyPair, ()> {
            let keypair = KeyPair::from_pkcs8(pkcs8)?;
            if !keypair.is_rsa() {
                return Err(());
            }
            Ok(RsaKeyPair { keypair })
        }

        pub fn sign(&self, signed_data: &[u8]) -> Result<impl AsRef<[u8]>, ()> {
            self.keypair.sign(signed_data)
        }
    }

    pub fn rsa_verify(pub_key: &[u8], signed_data: &[u8], signature: &[u8]) -> bool {
        unsafe {
            let mut cbs = CBS { data: core::ptr::null(), len: 0 };
            CBS_init(&mut cbs, pub_key.as_ptr(), pub_key.len());
            let rsa = RSA_parse_public_key(&mut cbs);
            if rsa.is_null() {
                return false;
            }

            let pkey = EVP_PKEY_new();
            EVP_PKEY_assign_RSA(pkey, rsa);

            let ctx = EVP_MD_CTX_new();
            assert!(
                EVP_DigestVerifyInit(
                    ctx,
                    core::ptr::null_mut(),
                    EVP_sha256(),
                    core::ptr::null_mut(),
                    pkey
                ) == 1
            );
            let ok = EVP_DigestVerify(
                ctx,
                signature.as_ptr(),
                signature.len(),
                signed_data.as_ptr(),
                signed_data.len(),
            ) == 1;
            EVP_MD_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            ok
        }
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
