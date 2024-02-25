//! ECDSA tests

use super::*;

define_test_set!(
    "ECDSA verify",
    "ecdsa_verify_schema.json",
    "ecdsa_p1363_verify_schema.json"
);

define_algorithm_map!("ECDSA" => Ecdsa);

define_test_set_names!(
    EcdsaBrainpool224Sha224 => "ecdsa_brainpoolP224r1_sha224",
    EcdsaBrainpool256r1Sha256 => "ecdsa_brainpoolP256r1_sha256",
    EcdsaBrainpool320r1Sha384 => "ecdsa_brainpoolP320r1_sha384",
    EcdsaBrainpool384r1Sha384 => "ecdsa_brainpoolP384r1_sha384",
    EcdsaBrainpool512r1Sha512 => "ecdsa_brainpoolP512r1_sha512",
    EcdsaSecp224r1Sha224 => "ecdsa_secp224r1_sha224",
    EcdsaSecp224r1Sha256 => "ecdsa_secp224r1_sha256",
    EcdsaSecp224r1Sha3_224 => "ecdsa_secp224r1_sha3_224",
    EcdsaSecp224r1Sha3_256 => "ecdsa_secp224r1_sha3_256",
    EcdsaSecp224r1Sha3_512 => "ecdsa_secp224r1_sha3_512",
    EcdsaSecp224r1Sha512 => "ecdsa_secp224r1_sha512",
    EcdsaSecp256k1Sha256 => "ecdsa_secp256k1_sha256",
    EcdsaSecp256k1Sha3_256 => "ecdsa_secp256k1_sha3_256",
    EcdsaSecp256k1Sha3_512 => "ecdsa_secp256k1_sha3_512",
    EcdsaSecp256k1Sha512 => "ecdsa_secp256k1_sha512",
    EcdsaSecp256r1Sha256 => "ecdsa_secp256r1_sha256",
    EcdsaSecp256r1Sha3_256 => "ecdsa_secp256r1_sha3_256",
    EcdsaSecp256r1Sha3_512 => "ecdsa_secp256r1_sha3_512",
    EcdsaSecp256r1Sha512 => "ecdsa_secp256r1_sha512",
    EcdsaSecp384r1Sha3_384 => "ecdsa_secp384r1_sha3_384",
    EcdsaSecp384r1Sha3_512 => "ecdsa_secp384r1_sha3_512",
    EcdsaSecp384r1Sha384 => "ecdsa_secp384r1_sha384",
    EcdsaSecp384r1Sha512 => "ecdsa_secp384r1_sha512",
    EcdsaSecp521r1Sha3_512 => "ecdsa_secp521r1_sha3_512",
    EcdsaSecp521r1Sha512 => "ecdsa_secp521r1_sha512",
    EcdsaBrainpool224r1Sha224P1363 => "ecdsa_brainpoolP224r1_sha224_p1363",
    EcdsaBrainpool256r1Sha256P1363 => "ecdsa_brainpoolP256r1_sha256_p1363",
    EcdsaBrainpool320r1Sha384P1363 => "ecdsa_brainpoolP320r1_sha384_p1363",
    EcdsaBrainpool384r1Sha384P1363 => "ecdsa_brainpoolP384r1_sha384_p1363",
    EcdsaBrainpool512r1Sha512P1363 => "ecdsa_brainpoolP512r1_sha512_p1363",
    EcdsaSecp224r1Sha224P1363 => "ecdsa_secp224r1_sha224_p1363",
    EcdsaSecp224r1Sha256P1363 => "ecdsa_secp224r1_sha256_p1363",
    EcdsaSecp224r1Sha512P1363 => "ecdsa_secp224r1_sha512_p1363",
    EcdsaSecp256k1Sha256P1363 => "ecdsa_secp256k1_sha256_p1363",
    EcdsaSecp256k1Sha512P1363 => "ecdsa_secp256k1_sha512_p1363",
    EcdsaSecp256r1Sha256P1363 => "ecdsa_secp256r1_sha256_p1363",
    EcdsaSecp256r1Sha512P1363 => "ecdsa_secp256r1_sha512_p1363",
    EcdsaSecp384r1Sha384P1363 => "ecdsa_secp384r1_sha384_p1363",
    EcdsaSecp384r1Sha512P1363 => "ecdsa_secp384r1_sha512_p1363",
    EcdsaSecp521r1Sha512P1363 => "ecdsa_secp521r1_sha512_p1363",
    EcdsaMisc => "ecdsa",
    EcdsaWebcrypto => "ecdsa_webcrypto"
);

#[derive(Debug, Copy, Clone, Hash, Eq, PartialEq, Deserialize)]
pub enum TestFlag {
    #[serde(rename = "BER")]
    Ber,
    EdgeCase,
    GroupIsomorphism,
    MissingZero,
    PointDuplication,
    SigSize,
    WeakHash,
}

define_typeid!(TestKeyTypeId => "EcPublicKey");

#[derive(Debug, Clone, Hash, Eq, PartialEq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct TestKey {
    pub curve: EllipticCurve,
    #[serde(rename = "keySize")]
    pub key_size: usize,
    #[serde(rename = "type")]
    typ: TestKeyTypeId,
    #[serde(deserialize_with = "vec_from_hex", rename = "uncompressed")]
    pub key: Vec<u8>,
    #[serde(deserialize_with = "vec_from_hex", rename = "wx")]
    pub affine_x: Vec<u8>,
    #[serde(deserialize_with = "vec_from_hex", rename = "wy")]
    pub affine_y: Vec<u8>,
}

define_typeid!(TestGroupTypeId => "EcdsaVerify", "EcdsaP1363Verify");

define_test_group!(
    jwk: Option<EcdsaPublicJwk>,
    key: TestKey,
    "keyDer" => der: Vec<u8> | "vec_from_hex",
    "keyPem" => pem: String,
    "sha" => hash: HashFunction,
);

define_test!(msg: Vec<u8>, sig: Vec<u8>);
