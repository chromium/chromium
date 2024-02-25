//! AEAD tests

use super::*;

define_test_set!("AEAD", "aead_test_schema.json");

define_test_set_names!(
    Aegis128 => "aegis128",
    Aegis128L => "aegis128L",
    Aegis256 => "aegis256",
    AesCcm => "aes_ccm",
    AesEax => "aes_eax",
    AesGcm => "aes_gcm",
    AesGcmSiv => "aes_gcm_siv",
    AesSivCmac => "aead_aes_siv_cmac",
    ChaCha20Poly1305 => "chacha20_poly1305",
    XChaCha20Poly1305 => "xchacha20_poly1305",
);

define_algorithm_map!(
    "AEGIS128L" => Aegis128L,
    "AEGIS128" => Aegis128,
    "AEGIS256" => Aegis256,
    "AES-CCM" => AesCcm,
    "AES-EAX" => AesEax,
    "AES-GCM" => AesGcm,
    "AES-GCM-SIV" => AesGcmSiv,
    "AEAD-AES-SIV-CMAC" => AesSivCmac,
    "CHACHA20-POLY1305" => ChaCha20Poly1305,
    "XCHACHA20-POLY1305" => XChaCha20Poly1305,
);

define_test_flags!(
    BadPadding,
    ConstructedIv,
    CounterWrap,
    EdgeCaseSiv,
    InvalidNonceSize,
    InvalidTagSize,
    LongIv,
    OldVersion,
    SmallIv,
    ZeroLengthIv,
);

define_typeid!(TestGroupTypeId => "AeadTest");

define_test_group!(
    "ivSize" => nonce_size: usize,
    "keySize" => key_size: usize,
    "tagSize" => tag_size: usize,
);

define_test!(
    key: Vec<u8>,
    "iv" => nonce: Vec<u8>,
    aad: Vec<u8>,
    "msg" => pt: Vec<u8>,
    ct: Vec<u8>,
    tag: Vec<u8>,
);
