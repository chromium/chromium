//! NIST keywrapping tests

use super::*;

define_test_set!("Keywrap", "keywrap_test_schema.json");

define_test_set_names!(
    KeyWrap => "kw",
    KeyWrapWithPadding => "kwp",
);

define_algorithm_map!(
    "KW" => KeyWrap,
    "KWP" => KeyWrapWithPadding,
);

define_test_flags!(SmallKey, WeakWrapping);

define_typeid!(TestGroupTypeId => "KeywrapTest");

define_test_group!(
    "keySize" => key_size: usize,
);

define_test!(key: Vec<u8>, msg: Vec<u8>, ct: Vec<u8>);
