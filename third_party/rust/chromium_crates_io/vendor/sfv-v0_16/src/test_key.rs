use crate::KeyRef;

#[test]
#[should_panic = "cannot be empty"]
fn test_constant_empty() {
    let _ = KeyRef::constant("");
}

#[test]
#[should_panic = "invalid character"]
fn test_constant_invalid_start_char() {
    let _ = KeyRef::constant("_key");
}

#[test]
#[should_panic = "invalid character"]
fn test_constant_invalid_inner_char() {
    let _ = KeyRef::constant("aND");
}

#[test]
fn test_conversions() {
    assert!(KeyRef::from_str("").is_err());
    assert!(KeyRef::from_str("aND").is_err());
    assert!(KeyRef::from_str("_key").is_err());
    assert!(KeyRef::from_str("7key").is_err());
}
