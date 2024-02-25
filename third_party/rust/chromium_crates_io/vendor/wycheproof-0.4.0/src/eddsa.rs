//! EdDSA verification tests

use super::*;

define_test_set!("EdDSA verify", "eddsa_verify_schema.json");

define_test_set_names!(
    Ed25519 => "eddsa",
    Ed448 => "ed448"
);

define_algorithm_map!("EDDSA" => EdDsa);

define_test_flags!(SignatureMalleability);

define_typeid!(TestKeyTypeId => "EDDSAKeyPair");

#[derive(Debug, Clone, Hash, Eq, PartialEq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct TestKey {
    pub curve: EdwardsCurve,
    #[serde(rename = "keySize")]
    pub key_size: usize,
    #[serde(deserialize_with = "vec_from_hex")]
    pub pk: Vec<u8>,
    #[serde(deserialize_with = "vec_from_hex")]
    pub sk: Vec<u8>,
    #[serde(rename = "type")]
    typ: TestKeyTypeId,
}

define_typeid!(TestGroupTypeId => "EddsaVerify");

define_test_group!(
    jwk: Option<EddsaJwk>,
    key: TestKey,
    "keyDer" => der: Vec<u8> | "vec_from_hex",
    "keyPem" => pem: String,
);

define_test!(msg: Vec<u8>, sig: Vec<u8>);
