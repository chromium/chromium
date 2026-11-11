use crate::StringRef;

#[test]
#[should_panic = "invalid character"]
fn test_constant_invalid_char() {
    let _ = StringRef::constant("text \x00");
}

#[test]
fn test_conversions() {
    assert!(StringRef::from_str("text \x00").is_err());
    assert!(StringRef::from_str("text \x1f").is_err());
    assert!(StringRef::from_str("text \x7f").is_err());
    assert!(StringRef::from_str("рядок").is_err());
    assert!(StringRef::from_str("non-ascii text 🐹").is_err());
}
