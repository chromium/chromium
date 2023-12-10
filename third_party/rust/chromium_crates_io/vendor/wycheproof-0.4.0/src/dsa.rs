//! DSA verification tests

use super::*;

define_test_set!(
    "DSA verify",
    "dsa_verify_schema.json",
    "dsa_p1363_verify_schema.json"
);

define_algorithm_map!("DSA" => Dsa);

define_test_set_names!(
    Dsa2048_224Sha224 => "dsa_2048_224_sha224",
    Dsa2048_224Sha256 => "dsa_2048_224_sha256",
    Dsa2048_256Sha256 => "dsa_2048_256_sha256",
    Dsa3072_256Sha256 => "dsa_3072_256_sha256",
    Dsa2048_224Sha224P1363 => "dsa_2048_224_sha224_p1363",
    Dsa2048_224Sha256P1363 => "dsa_2048_224_sha256_p1363",
    Dsa2048_256Sha256P1363 => "dsa_2048_256_sha256_p1363",
    Dsa3072_256Sha256P1363 => "dsa_3072_256_sha256_p1363",
    DsaMisc => "dsa"
);

define_test_flags!(EdgeCase, NoLeadingZero);

define_typeid!(TestKeyTypeId => "DsaPublicKey");

#[derive(Debug, Clone, Hash, Eq, PartialEq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct TestKey {
    #[serde(deserialize_with = "vec_from_hex")]
    pub g: Vec<u8>,
    #[serde(rename = "keySize")]
    pub key_size: usize,
    #[serde(deserialize_with = "vec_from_hex")]
    pub p: Vec<u8>,
    #[serde(deserialize_with = "vec_from_hex")]
    pub q: Vec<u8>,
    #[serde(rename = "type")]
    typ: TestKeyTypeId,
    #[serde(deserialize_with = "vec_from_hex")]
    pub y: Vec<u8>,
}

define_typeid!(TestGroupTypeId => "DsaVerify", "DsaP1363Verify");

define_test_group!(
    key: TestKey,
    "keyDer" => der: Vec<u8> | "vec_from_hex",
    "keyPem" => pem: String,
    "sha" => hash: HashFunction,
);

define_test!(msg: Vec<u8>, sig: Vec<u8>);
