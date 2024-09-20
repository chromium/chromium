// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

#[macro_use]
extern crate zerocopy;

#[path = "../util.rs"]
mod util;

use self::util::AU16;
use std::mem::ManuallyDrop;

fn main() {}

//
// AsBytes errors
//

#[derive(AsBytes)]
#[repr(C)]
union AsBytes1<T> {
    foo: ManuallyDrop<T>,
}

#[derive(AsBytes)]
#[repr(C)]
union AsBytes2 {
    foo: u8,
    bar: [u8; 2],
}

//
// Unaligned errors
//

#[derive(Unaligned)]
#[repr(C, align(2))]
union Unaligned1 {
    foo: i16,
    bar: AU16,
}

// Transparent unions are unstable; see issue #60405
// <https://github.com/rust-lang/rust/issues/60405> for more information.

// #[derive(Unaligned)]
// #[repr(transparent, align(2))]
// union Unaligned2 {
//     foo: u8,
// }

#[derive(Unaligned)]
#[repr(packed, align(2))]
union Unaligned3 {
    foo: u8,
}

#[derive(Unaligned)]
#[repr(align(1), align(2))]
struct Unaligned4 {
    foo: u8,
}

#[derive(Unaligned)]
#[repr(align(2), align(4))]
struct Unaligned5 {
    foo: u8,
}
