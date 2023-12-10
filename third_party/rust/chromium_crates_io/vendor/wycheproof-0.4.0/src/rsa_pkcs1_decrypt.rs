//! RSA PKCS1v1.5 decryption tests

use super::*;

define_test_set!("RSA PKCS1 decrypt", "rsaes_pkcs1_decrypt_schema.json");

define_algorithm_map!("RSAES-PKCS1-v1_5" => RsaPkcs1v15Encryption);

define_test_set_names!(
    Rsa2048 => "rsa_pkcs1_2048",
    Rsa3072 => "rsa_pkcs1_3072",
    Rsa4096 => "rsa_pkcs1_4096"
);

define_test_flags!(InvalidPkcs1Padding);

define_typeid!(TestGroupTypeId => "RsaesPkcs1Decrypt");

define_test_group!(
    d: Vec<u8> | "vec_from_hex",
    e: Vec<u8> | "vec_from_hex",
    "keysize" => key_size: usize,
    n: Vec<u8> | "vec_from_hex",
    "privateKeyJwk" => jwk: Option<RsaPrivateJwk>,
    "privateKeyPkcs8" => pkcs8: Vec<u8> | "vec_from_hex",
    "privateKeyPem" => pem: String,
);

define_test!(msg: Vec<u8>, ct: Vec<u8>);
