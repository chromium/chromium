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

use zerocopy::KnownLayout;

use self::util::AU16;

fn main() {}

//
// KnownLayout errors
//

struct NotKnownLayout;

struct NotKnownLayoutDst([u8]);

// | `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
// |          N |        N |              N |        N |      KL00 |
#[derive(KnownLayout)]
struct KL00(u8, NotKnownLayoutDst);

// | `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
// |          N |        N |              Y |        N |      KL02 |
#[derive(KnownLayout)]
struct KL02(u8, [u8]);

// | `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
// |          Y |        N |              N |        N |      KL08 |
#[derive(KnownLayout)]
#[repr(C)]
struct KL08(u8, NotKnownLayoutDst);

// | `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
// |          Y |        N |              N |        Y |      KL09 |
#[derive(KnownLayout)]
#[repr(C)]
struct KL09(NotKnownLayout, NotKnownLayout);

//
// AsBytes errors
//

#[derive(AsBytes)]
#[repr(C)]
struct AsBytes1<T>(T);

#[derive(AsBytes)]
#[repr(C)]
struct AsBytes2 {
    foo: u8,
    bar: AU16,
}

#[derive(AsBytes)]
#[repr(C, packed(2))]
struct AsBytes3 {
    foo: u8,
    // We'd prefer to use AU64 here, but you can't use aligned types in
    // packed structs.
    bar: u64,
}

//
// Unaligned errors
//

#[derive(Unaligned)]
#[repr(C, align(2))]
struct Unaligned1;

#[derive(Unaligned)]
#[repr(transparent, align(2))]
struct Unaligned2 {
    foo: u8,
}

#[derive(Unaligned)]
#[repr(packed, align(2))]
struct Unaligned3;

#[derive(Unaligned)]
#[repr(align(1), align(2))]
struct Unaligned4;

#[derive(Unaligned)]
#[repr(align(2), align(4))]
struct Unaligned5;
