//! HKDF tests

use super::*;

define_test_set!("HKDF", "hkdf_test_schema.json");

define_test_set_names!(
    HkdfSha1 => "hkdf_sha1",
    HkdfSha256 => "hkdf_sha256",
    HkdfSha384 => "hkdf_sha384",
    HkdfSha512 => "hkdf_sha512",
);

define_algorithm_map!(
    "HKDF-SHA-1" => HkdfSha1,
    "HKDF-SHA-256" => HkdfSha256,
    "HKDF-SHA-384" => HkdfSha384,
    "HKDF-SHA-512" => HkdfSha512,
);

define_test_flags!(EmptySalt, SizeTooLarge);

define_typeid!(TestGroupTypeId => "HkdfTest");

define_test_group!(
    "keySize" => key_size: usize,
);

define_test_ex!(
    ikm: Vec<u8> | "vec_from_hex",
    salt: Vec<u8> | "vec_from_hex",
    info: Vec<u8> | "vec_from_hex",
    size: usize,
    okm: Vec<u8> | "vec_from_hex",
);
