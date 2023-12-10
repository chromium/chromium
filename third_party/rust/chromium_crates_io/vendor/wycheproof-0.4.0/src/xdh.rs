//! Montgomery curve ECDH tests

use super::*;

define_test_set!("xDH", "xdh_comp_schema.json");

define_test_set_names!(
    X25519 => "x25519",
    X448 => "x448",
);

define_algorithm_map!("XDH" => Xdh);

define_test_flags!(
    LowOrderPublic,
    NonCanonicalPublic,
    SmallPublicKey,
    Twist,
    ZeroSharedSecret,
);

define_typeid!(TestGroupTypeId => "XdhComp");

define_test_group!(curve: MontgomeryCurve);

define_test!(
    "public" => public_key: Vec<u8>,
    "private" => private_key: Vec<u8>,
    "shared" => shared_secret: Vec<u8>,
);
