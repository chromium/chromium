#![cfg(target_pointer_width = "64")]

#[test]
fn decode_error_size() {
    assert_eq!(std::mem::size_of::<bincode::error::DecodeError>(), 32);
}

#[test]
fn encode_error_size() {
    #[cfg(feature = "std")]
    assert_eq!(std::mem::size_of::<bincode::error::EncodeError>(), 32);

    #[cfg(not(feature = "std"))]
    assert_eq!(std::mem::size_of::<bincode::error::EncodeError>(), 24);
}
