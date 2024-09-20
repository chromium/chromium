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

// A union is `FromZeroes` if:
// - all fields are `FromZeroes`

#[derive(Clone, Copy, FromZeroes)]
union Zst {
    a: (),
}

assert_impl_all!(Zst: FromZeroes);

#[derive(FromZeroes)]
union One {
    a: bool,
}

assert_impl_all!(One: FromZeroes);

#[derive(FromZeroes)]
union Two {
    a: bool,
    b: Zst,
}

assert_impl_all!(Two: FromZeroes);

#[derive(FromZeroes)]
union TypeParams<'a, T: Copy, I: Iterator>
where
    I::Item: Copy,
{
    a: T,
    c: I::Item,
    d: u8,
    e: PhantomData<&'a [u8]>,
    f: PhantomData<&'static str>,
    g: PhantomData<String>,
}

assert_impl_all!(TypeParams<'static, (), IntoIter<()>>: FromZeroes);

// Deriving `FromZeroes` should work if the union has bounded parameters.

#[derive(FromZeroes)]
#[repr(C)]
union WithParams<'a: 'b, 'b: 'a, const N: usize, T: 'a + 'b + FromZeroes>
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + Copy + FromZeroes,
{
    a: [T; N],
    b: PhantomData<&'a &'b ()>,
}

assert_impl_all!(WithParams<'static, 'static, 42, u8>: FromZeroes);
