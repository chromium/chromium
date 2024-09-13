// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![allow(warnings)]

#[macro_use]
mod util;

use std::{marker::PhantomData, option::IntoIter};

use {
    static_assertions::assert_impl_all,
    zerocopy::{DstLayout, KnownLayout},
};

use crate::util::AU16;

#[derive(KnownLayout)]
struct Zst;

assert_impl_all!(Zst: KnownLayout);

#[derive(KnownLayout)]
struct One {
    a: bool,
}

assert_impl_all!(One: KnownLayout);

#[derive(KnownLayout)]
struct Two {
    a: bool,
    b: Zst,
}

assert_impl_all!(Two: KnownLayout);

#[derive(KnownLayout)]
struct TypeParams<'a, T, I: Iterator> {
    a: I::Item,
    b: u8,
    c: PhantomData<&'a [u8]>,
    d: PhantomData<&'static str>,
    e: PhantomData<String>,
    f: T,
}

assert_impl_all!(TypeParams<'static, (), IntoIter<()>>: KnownLayout);
assert_impl_all!(TypeParams<'static, AU16, IntoIter<()>>: KnownLayout);

// Deriving `KnownLayout` should work if the struct has bounded parameters.

#[derive(KnownLayout)]
#[repr(C)]
struct WithParams<'a: 'b, 'b: 'a, const N: usize, T: 'a + 'b + KnownLayout>(
    [T; N],
    PhantomData<&'a &'b ()>,
)
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + KnownLayout;

assert_impl_all!(WithParams<'static, 'static, 42, u8>: KnownLayout);
