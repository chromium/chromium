//! Message Authentication Code tests

use super::*;

define_test_set!("MAC", "mac_test_schema.json");

define_test_set_names!(
    AesCmac => "aes_cmac",
    HmacSha1 => "hmac_sha1",
    HmacSha224 => "hmac_sha224",
    HmacSha256 => "hmac_sha256",
    HmacSha384 => "hmac_sha384",
    HmacSha512 => "hmac_sha512",
    HmacSha3_224 => "hmac_sha3_224",
    HmacSha3_256 => "hmac_sha3_256",
    HmacSha3_384 => "hmac_sha3_384",
    HmacSha3_512 => "hmac_sha3_512",
);

define_algorithm_map!(
    "AES-CMAC" => AesCmac,
    "HMACSHA1" => HmacSha1,
    "HMACSHA224" => HmacSha224,
    "HMACSHA256" => HmacSha256,
    "HMACSHA384" => HmacSha384,
    "HMACSHA512" => HmacSha512,
    "HMACSHA3-224" => HmacSha3_224,
    "HMACSHA3-256" => HmacSha3_256,
    "HMACSHA3-384" => HmacSha3_384,
    "HMACSHA3-512" => HmacSha3_512,
);

define_typeid!(TestGroupTypeId => "MacTest");

define_test_flags!();

define_test_group!(
    "keySize" => key_size: usize,
    "tagSize" => tag_size: usize,
);

define_test!(key: Vec<u8>, msg: Vec<u8>, tag: Vec<u8>,);
