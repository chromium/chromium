#![cfg(all(feature = "std", feature = "derive"))]

extern crate std;

use bincode::{Decode, Encode};
use std::borrow::Cow;
use std::string::String;

#[derive(Decode, Encode, PartialEq, Debug)]
#[bincode(
    decode_context = "()",
    borrow_decode_bounds = "&'__de U<'a, A>: ::bincode::de::BorrowDecode<'__de, ()> + '__de, '__de: 'a"
)]
struct T<'a, A: Clone + Encode + Decode<()>> {
    t: Cow<'a, U<'a, A>>,
}

#[derive(Clone, Decode, Encode, PartialEq, Debug)]
#[bincode(
    decode_context = "()",
    borrow_decode_bounds = "&'__de A: ::bincode::de::BorrowDecode<'__de, ()> + '__de, '__de: 'a"
)]
struct U<'a, A: Clone + Encode + Decode<()>> {
    u: Cow<'a, A>,
}

#[test]
fn test() {
    let u = U {
        u: Cow::Owned(String::from("Hello world")),
    };
    let t = T {
        t: Cow::Borrowed(&u),
    };
    let vec = bincode::encode_to_vec(&t, bincode::config::standard()).unwrap();

    let (decoded, len): (T<String>, usize) =
        bincode::decode_from_slice(&vec, bincode::config::standard()).unwrap();

    assert_eq!(t, decoded);
    assert_eq!(len, 12);
}
