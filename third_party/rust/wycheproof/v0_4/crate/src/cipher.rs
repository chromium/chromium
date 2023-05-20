//! IND-CPA cipher tests

use super::*;

define_test_set!("Cipher", "ind_cpa_test_schema.json");

define_test_set_names!(AesCbcPkcs5 => "aes_cbc_pkcs5");

define_algorithm_map!("AES-CBC-PKCS5" => AesCbcPkcs5);

define_test_flags!(BadPadding);

define_typeid!(TestGroupTypeId => "IndCpaTest");

define_test_group!(
    "ivSize" => nonce_size: usize,
    "keySize" => key_size: usize,
);

define_test!(
    iv: Vec<u8>,
    key: Vec<u8>,
    "msg" => pt: Vec<u8>,
    ct: Vec<u8>,
);
