// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

#![allow(warnings)]

mod util;

use std::{marker::PhantomData, option::IntoIter};

use {
    static_assertions::assert_impl_all,
    zerocopy::{FromBytes, FromZeroes},
};

use crate::util::AU16;

// A struct is `FromBytes` if:
// - all fields are `FromBytes`

#[derive(FromZeroes, FromBytes)]
struct Zst;

assert_impl_all!(Zst: FromBytes);

#[derive(FromZeroes, FromBytes)]
struct One {
    a: u8,
}

assert_impl_all!(One: FromBytes);

#[derive(FromZeroes, FromBytes)]
struct Two {
    a: u8,
    b: Zst,
}

assert_impl_all!(Two: FromBytes);

#[derive(FromZeroes, FromBytes)]
struct Unsized {
    a: [u8],
}

assert_impl_all!(Unsized: FromBytes);

#[derive(FromZeroes, FromBytes)]
struct TypeParams<'a, T: ?Sized, I: Iterator> {
    a: I::Item,
    b: u8,
    c: PhantomData<&'a [u8]>,
    d: PhantomData<&'static str>,
    e: PhantomData<String>,
    f: T,
}

assert_impl_all!(TypeParams<'static, (), IntoIter<()>>: FromBytes);
assert_impl_all!(TypeParams<'static, AU16, IntoIter<()>>: FromBytes);
assert_impl_all!(TypeParams<'static, [AU16], IntoIter<()>>: FromBytes);

// Deriving `FromBytes` should work if the struct has bounded parameters.

#[derive(FromZeroes, FromBytes)]
#[repr(transparent)]
struct WithParams<'a: 'b, 'b: 'a, const N: usize, T: 'a + 'b + FromBytes>(
    [T; N],
    PhantomData<&'a &'b ()>,
)
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + FromBytes;

assert_impl_all!(WithParams<'static, 'static, 42, u8>: FromBytes);
