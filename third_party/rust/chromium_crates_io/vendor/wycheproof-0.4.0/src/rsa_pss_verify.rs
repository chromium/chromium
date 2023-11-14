//! RSA PSS verification tests

use super::*;

define_test_set!("RSA PKCS1 verify", "rsassa_pss_verify_schema.json");

define_test_set_names!(
    RsaPss2048Sha1Mgf1_20 => "rsa_pss_2048_sha1_mgf1_20",
    RsaPss2048Sha256Mgf1_0 => "rsa_pss_2048_sha256_mgf1_0",
    RsaPss2048Sha256Mgf1_32 => "rsa_pss_2048_sha256_mgf1_32",
    RsaPss2048Sha512_256Mgf1_28 => "rsa_pss_2048_sha512_256_mgf1_28",
    RsaPss2048Sha512_256Mgf1_32 => "rsa_pss_2048_sha512_256_mgf1_32",
    RsaPss3072Sha256Mgf1_32 => "rsa_pss_3072_sha256_mgf1_32",
    RsaPss4096Sha256Mgf1_32 => "rsa_pss_4096_sha256_mgf1_32",
    RsaPss4096Sha512Mgf1_32 => "rsa_pss_4096_sha512_mgf1_32",
    RsaPssmisc => "rsa_pss_misc"
);

define_algorithm_map!("RSASSA-PSS" => RsaPss);

define_test_flags!(WeakHash);

define_typeid!(TestGroupTypeId => "RsassaPssVerify");

define_test_group!(
    e: Vec<u8> | "vec_from_hex",
    "keyAsn" => asn_key: Vec<u8> | "vec_from_hex",
    "keyDer" => der: Vec<u8> | "vec_from_hex",
    "keyPem" => pem: String,
    "keysize" => key_size: usize,
    mgf: Mgf,
    "mgfSha" => mgf_hash: HashFunction,
    n: Vec<u8> | "vec_from_hex",
    "sLen" => salt_length: usize,
    "sha" => hash: HashFunction,
);

define_test!(msg: Vec<u8>, sig: Vec<u8>);
