use super::{u8_slice_eq, str_slice_eq};

#[test]
fn u8_slice_test() {
    assert!(u8_slice_eq(b"", b""));
    assert!(!u8_slice_eq(b"", b"0"));
    assert!(!u8_slice_eq(b"0", b""));
    assert!(u8_slice_eq(b"0", b"0"));
    assert!(!u8_slice_eq(b"0", b"1"));
    assert!(!u8_slice_eq(b"1", b"0"));
    assert!(!u8_slice_eq(b"0", b"0, 1"));
    assert!(!u8_slice_eq(b"0, 1", b"0"));
    assert!(!u8_slice_eq(b"0, 1", b"1"));
    assert!(u8_slice_eq(b"0, 1", b"0, 1"));
    assert!(!u8_slice_eq(b"0, 1", b"0, 2"));
}

#[test]
fn str_slice_eq_test() {
    // different lengths
    assert!(str_slice_eq(&[], &[]));
    assert!(str_slice_eq(&[""], &[""]));
    assert!(!str_slice_eq(&[], &[""]));
    assert!(!str_slice_eq(&[""], &[]));

    // length 1
    assert!(str_slice_eq(&["foo"], &["foo"]));
    assert!(!str_slice_eq(&["foo"], &["bar"]));

    // length 2
    assert!(str_slice_eq(&["foo", "bar"], &["foo", "bar"]));
    assert!(!str_slice_eq(&["foo", "bar"], &["foo", "baz"]));
    assert!(!str_slice_eq(&["foo", "bar"], &["foo", "bbr"]));
    assert!(!str_slice_eq(&["foo", "bar"], &["foo", "car"]));

    // length 3
    assert!(str_slice_eq(&["foo", "foo2", "bar"], &["foo", "foo2", "bar"]));
    assert!(!str_slice_eq(&["foo", "foo2", "bar"], &["foo", "foo2", "baz"]));
    assert!(!str_slice_eq(&["foo", "foo2", "bar"], &["foo", "foo2", "bbr"]));
    assert!(!str_slice_eq(&["foo", "foo2", "bar"], &["foo", "foo2", "car"]));

}