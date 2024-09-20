// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

#![allow(warnings)]

#[macro_use]
mod util;

use std::{marker::PhantomData, option::IntoIter};

use {static_assertions::assert_impl_all, zerocopy::FromZeroes};

use crate::util::AU16;

// A struct is `FromZeroes` if:
// - all fields are `FromZeroes`

#[derive(FromZeroes)]
struct Zst;

assert_impl_all!(Zst: FromZeroes);

#[derive(FromZeroes)]
struct One {
    a: bool,
}

assert_impl_all!(One: FromZeroes);

#[derive(FromZeroes)]
struct Two {
    a: bool,
    b: Zst,
}

assert_impl_all!(Two: FromZeroes);

#[derive(FromZeroes)]
struct Unsized {
    a: [u8],
}

assert_impl_all!(Unsized: FromZeroes);

#[derive(FromZeroes)]
struct TypeParams<'a, T: ?Sized, I: Iterator> {
    a: I::Item,
    b: u8,
    c: PhantomData<&'a [u8]>,
    d: PhantomData<&'static str>,
    e: PhantomData<String>,
    f: T,
}

assert_impl_all!(TypeParams<'static, (), IntoIter<()>>: FromZeroes);
assert_impl_all!(TypeParams<'static, AU16, IntoIter<()>>: FromZeroes);
assert_impl_all!(TypeParams<'static, [AU16], IntoIter<()>>: FromZeroes);

// Deriving `FromZeroes` should work if the struct has bounded parameters.

#[derive(FromZeroes)]
#[repr(transparent)]
struct WithParams<'a: 'b, 'b: 'a, const N: usize, T: 'a + 'b + FromZeroes>(
    [T; N],
    PhantomData<&'a &'b ()>,
)
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + FromZeroes;

assert_impl_all!(WithParams<'static, 'static, 42, u8>: FromZeroes);
