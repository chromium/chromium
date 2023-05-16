//! Deterministic AEAD tests
//!
//! These do not use a per-message nonce

use super::*;

define_test_set!("DAEAD", "daead_test_schema.json");

define_test_set_names!(AesSivCmac => "aes_siv_cmac");

define_algorithm_map!("AES-SIV-CMAC" => AesSivCmac);

define_test_flags!(EdgeCaseSiv);

define_typeid!(TestGroupTypeId => "DaeadTest");

define_test_group!(
    "keySize" => key_size: usize,
);

define_test!(
    key: Vec<u8>,
    aad: Vec<u8>,
    "msg" => pt: Vec<u8>,
    ct: Vec<u8>,
);
