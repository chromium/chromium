// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![allow(warnings)]

mod util;

use {core::marker::PhantomData, static_assertions::assert_impl_all, zerocopy::KnownLayout};

#[derive(KnownLayout)]
enum Foo {
    A,
}

assert_impl_all!(Foo: KnownLayout);

#[derive(KnownLayout)]
enum Bar {
    A = 0,
}

assert_impl_all!(Bar: KnownLayout);

#[derive(KnownLayout)]
enum Baz {
    A = 1,
    B = 0,
}

assert_impl_all!(Baz: KnownLayout);

// Deriving `KnownLayout` should work if the enum has bounded parameters.

#[derive(KnownLayout)]
#[repr(C)]
enum WithParams<'a: 'b, 'b: 'a, const N: usize, T: 'a + 'b + KnownLayout>
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + KnownLayout,
{
    Variant([T; N], PhantomData<&'a &'b ()>),
}

assert_impl_all!(WithParams<'static, 'static, 42, u8>: KnownLayout);
