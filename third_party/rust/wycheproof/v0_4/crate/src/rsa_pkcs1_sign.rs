//! RSA PKCS1v1.5 signature generation tests

use super::*;

define_test_set!("RSA PKCS1 sign", "rsassa_pkcs1_generate_schema.json");

define_test_set_names!(RsaMisc => "rsa_sig_gen_misc");

define_algorithm_map!("RSASSA-PKCS1-v1_5" => RsaPkcs1v15);

define_test_flags!(SmallPublicKey, SmallModulus, WeakHash);

define_typeid!(TestGroupTypeId => "RsassaPkcs1Generate");

define_test_group!(
    d: Vec<u8> | "vec_from_hex",
    e: Vec<u8> | "vec_from_hex",
    "keyAsn" => asn_key: Vec<u8> | "vec_from_hex",
    "keyDer" => der: Vec<u8> | "vec_from_hex",
    "keyJwk" => public_jwk: Option<RsaPublicJwk>,
    "privateKeyJwk" => private_jwk: Option<RsaPrivateJwk>,
    "keyPem" => public_pem: String,
    "privateKeyPem" => private_pem: String,
    "privateKeyPkcs8" => private_pkcs8: String,
    "keysize" => key_size: usize,
    n: Vec<u8> | "vec_from_hex",
    "sha" => hash: HashFunction,
);

define_test!(msg: Vec<u8>, sig: Vec<u8>);
