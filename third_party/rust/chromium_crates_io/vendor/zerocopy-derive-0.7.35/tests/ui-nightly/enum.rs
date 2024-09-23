// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

#[macro_use]
extern crate zerocopy;

fn main() {}

//
// Generic errors
//

#[derive(FromZeroes, FromBytes)]
#[repr("foo")]
enum Generic1 {
    A,
}

#[derive(FromZeroes, FromBytes)]
#[repr(foo)]
enum Generic2 {
    A,
}

#[derive(FromZeroes, FromBytes)]
#[repr(transparent)]
enum Generic3 {
    A,
}

#[derive(FromZeroes, FromBytes)]
#[repr(u8, u16)]
enum Generic4 {
    A,
}

#[derive(FromZeroes, FromBytes)]
enum Generic5 {
    A,
}

//
// FromZeroes errors
//

#[derive(FromZeroes)]
enum FromZeroes1 {
    A(u8),
}

#[derive(FromZeroes)]
enum FromZeroes2 {
    A,
    B(u8),
}

#[derive(FromZeroes)]
enum FromZeroes3 {
    A = 1,
    B,
}

//
// FromBytes errors
//

#[derive(FromZeroes, FromBytes)]
#[repr(C)]
enum FromBytes1 {
    A,
}

#[derive(FromZeroes, FromBytes)]
#[repr(usize)]
enum FromBytes2 {
    A,
}

#[derive(FromZeroes, FromBytes)]
#[repr(isize)]
enum FromBytes3 {
    A,
}

#[derive(FromZeroes, FromBytes)]
#[repr(u32)]
enum FromBytes4 {
    A,
}

#[derive(FromZeroes, FromBytes)]
#[repr(i32)]
enum FromBytes5 {
    A,
}

#[derive(FromZeroes, FromBytes)]
#[repr(u64)]
enum FromBytes6 {
    A,
}

#[derive(FromZeroes, FromBytes)]
#[repr(i64)]
enum FromBytes7 {
    A,
}

//
// Unaligned errors
//

#[derive(Unaligned)]
#[repr(C)]
enum Unaligned1 {
    A,
}

#[derive(Unaligned)]
#[repr(u16)]
enum Unaligned2 {
    A,
}

#[derive(Unaligned)]
#[repr(i16)]
enum Unaligned3 {
    A,
}

#[derive(Unaligned)]
#[repr(u32)]
enum Unaligned4 {
    A,
}

#[derive(Unaligned)]
#[repr(i32)]
enum Unaligned5 {
    A,
}

#[derive(Unaligned)]
#[repr(u64)]
enum Unaligned6 {
    A,
}

#[derive(Unaligned)]
#[repr(i64)]
enum Unaligned7 {
    A,
}

#[derive(Unaligned)]
#[repr(usize)]
enum Unaligned8 {
    A,
}

#[derive(Unaligned)]
#[repr(isize)]
enum Unaligned9 {
    A,
}

#[derive(Unaligned)]
#[repr(u8, align(2))]
enum Unaligned10 {
    A,
}

#[derive(Unaligned)]
#[repr(i8, align(2))]
enum Unaligned11 {
    A,
}

#[derive(Unaligned)]
#[repr(align(1), align(2))]
enum Unaligned12 {
    A,
}

#[derive(Unaligned)]
#[repr(align(2), align(4))]
enum Unaligned13 {
    A,
}
