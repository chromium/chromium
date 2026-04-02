#![cfg(all(feature = "derive", feature = "std"))]

use bincode::{Decode, Encode};

#[derive(Encode, Decode)]
struct Foo<Bar = ()> {
    x: Bar,
}
