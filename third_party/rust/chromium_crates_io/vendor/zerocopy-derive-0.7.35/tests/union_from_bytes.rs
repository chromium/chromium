// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

#![allow(warnings)]

use std::{marker::PhantomData, option::IntoIter};

use {
    static_assertions::assert_impl_all,
    zerocopy::{FromBytes, FromZeroes},
};

// A union is `FromBytes` if:
// - all fields are `FromBytes`

#[derive(Clone, Copy, FromZeroes, FromBytes)]
union Zst {
    a: (),
}

assert_impl_all!(Zst: FromBytes);

#[derive(FromZeroes, FromBytes)]
union One {
    a: u8,
}

assert_impl_all!(One: FromBytes);

#[derive(FromZeroes, FromBytes)]
union Two {
    a: u8,
    b: Zst,
}

assert_impl_all!(Two: FromBytes);

#[derive(FromZeroes, FromBytes)]
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

assert_impl_all!(TypeParams<'static, (), IntoIter<()>>: FromBytes);

// Deriving `FromBytes` should work if the union has bounded parameters.

#[derive(FromZeroes, FromBytes)]
#[repr(C)]
union WithParams<'a: 'b, 'b: 'a, const N: usize, T: 'a + 'b + FromBytes>
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + Copy + FromBytes,
{
    a: [T; N],
    b: PhantomData<&'a &'b ()>,
}

assert_impl_all!(WithParams<'static, 'static, 42, u8>: FromBytes);
