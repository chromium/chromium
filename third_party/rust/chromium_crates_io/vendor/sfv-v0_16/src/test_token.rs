use crate::TokenRef;

#[test]
#[should_panic = "cannot be empty"]
fn test_constant_empty() {
    let _ = TokenRef::constant("");
}

#[test]
#[should_panic = "invalid character"]
fn test_constant_invalid_start_char() {
    let _ = TokenRef::constant("#some");
}

#[test]
#[should_panic = "invalid character"]
fn test_constant_invalid_inner_char() {
    let _ = TokenRef::constant("s ");
}

#[test]
fn test_conversions() {
    assert!(TokenRef::from_str("").is_err());
    assert!(TokenRef::from_str("#some").is_err());
    assert!(TokenRef::from_str("s ").is_err());
    assert!(TokenRef::from_str("abc:de\t").is_err());
}
