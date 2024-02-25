//! RSA OAEP decryption tests

use super::*;

define_test_set!("RSA OAEP decrypt", "rsaes_oaep_decrypt_schema.json");

define_test_set_names!(
    Rsa2048Sha1Mgf1Sha1 => "rsa_oaep_2048_sha1_mgf1sha1",
    Rsa2048Sha224Mgf1Sha1 => "rsa_oaep_2048_sha224_mgf1sha1",
    Rsa2048Sha224Mgf1Sha224 => "rsa_oaep_2048_sha224_mgf1sha224",
    Rsa2048Sha256Mgf1Sha1 => "rsa_oaep_2048_sha256_mgf1sha1",
    Rsa2048Sha256Mgf1Sha256 => "rsa_oaep_2048_sha256_mgf1sha256",
    Rsa2048Sha384Mgf1Sha1 => "rsa_oaep_2048_sha384_mgf1sha1",
    Rsa2048Sha384Mgf1Sha384 => "rsa_oaep_2048_sha384_mgf1sha384",
    Rsa2048Sha512Mgf1Sha1 => "rsa_oaep_2048_sha512_mgf1sha1",
    Rsa2048Sha512Mgf1Sha512 => "rsa_oaep_2048_sha512_mgf1sha512",
    Rsa3072Sha256Mgf1Sha1 => "rsa_oaep_3072_sha256_mgf1sha1",
    Rsa3072Sha256Mgf1Sha256 => "rsa_oaep_3072_sha256_mgf1sha256",
    Rsa3072Sha512Mgf1Sha1 => "rsa_oaep_3072_sha512_mgf1sha1",
    Rsa3072Sha512Mgf1Sha512 => "rsa_oaep_3072_sha512_mgf1sha512",
    Rsa4096Sha256Mgf1Sha1 => "rsa_oaep_4096_sha256_mgf1sha1",
    Rsa4096Sha256Mgf1Sha256 => "rsa_oaep_4096_sha256_mgf1sha256",
    Rsa4096Sha512Mgf1Sha1 => "rsa_oaep_4096_sha512_mgf1sha1",
    Rsa4096Sha512Mgf1Sha512 => "rsa_oaep_4096_sha512_mgf1sha512",
    RsaMisc => "rsa_oaep_misc"
);

define_algorithm_map!("RSAES-OAEP" => RsaOaep);

define_test_flags!(Constructed, InvalidOaepPadding, SmallModulus);

define_typeid!(TestGroupTypeId => "RsaesOaepDecrypt");

define_test_group!(
    d: Vec<u8> | "vec_from_hex",
    e: Vec<u8> | "vec_from_hex",
    "keysize" => key_size: usize,
    mgf: Mgf,
    "mgfSha" => mgf_hash: HashFunction,
    n: Vec<u8> | "vec_from_hex",
    "privateKeyJwk" => jwk: Option<RsaPrivateJwk>,
    "privateKeyPkcs8" => pkcs8: Vec<u8> | "vec_from_hex",
    "privateKeyPem" => pem: String,
    "sha" => hash: HashFunction,
);

define_test!(msg: Vec<u8>, ct: Vec<u8>, label: Vec<u8>);
