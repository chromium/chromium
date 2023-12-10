//! Message Authentication Code tests

use super::*;

define_test_set!("MAC with IV", "mac_with_iv_test_schema.json");

define_test_set_names!(
    Gmac => "gmac",
    Vmac64 => "vmac_64",
    Vmac128 => "vmac_128",
);

define_algorithm_map!(
    "AES-GMAC" => AesGmac,
    "VMAC-AES" => VmacAes,
);

define_typeid!(TestGroupTypeId => "MacWithIvTest");

define_test_flags!(InvalidNonce);

define_test_group!(
    "keySize" => key_size: usize,
    "tagSize" => tag_size: usize,
    "ivSize" => nonce_size: usize,
);

define_test!(
    key: Vec<u8>,
    "iv" => nonce: Vec<u8>,
    msg: Vec<u8>,
    tag: Vec<u8>,
);
