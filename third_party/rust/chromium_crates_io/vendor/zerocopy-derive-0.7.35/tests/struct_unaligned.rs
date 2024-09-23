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

use {static_assertions::assert_impl_all, zerocopy::Unaligned};

use crate::util::AU16;

// A struct is `Unaligned` if:
// - `repr(align)` is no more than 1 and either
//   - `repr(C)` or `repr(transparent)` and
//     - all fields Unaligned
//   - `repr(packed)`

#[derive(Unaligned)]
#[repr(C)]
struct Foo {
    a: u8,
}

assert_impl_all!(Foo: Unaligned);

#[derive(Unaligned)]
#[repr(transparent)]
struct Bar {
    a: u8,
}

assert_impl_all!(Bar: Unaligned);

#[derive(Unaligned)]
#[repr(packed)]
struct Baz {
    // NOTE: The `u16` type is not guaranteed to have alignment 2, although it
    // does on many platforms. However, to fix this would require a custom type
    // with a `#[repr(align(2))]` attribute, and `#[repr(packed)]` types are not
    // allowed to transitively contain `#[repr(align(...))]` types. Thus, we
    // have no choice but to use `u16` here. Luckily, these tests run in CI on
    // platforms on which `u16` has alignment 2, so this isn't that big of a
    // deal.
    a: u16,
}

assert_impl_all!(Baz: Unaligned);

#[derive(Unaligned)]
#[repr(C, align(1))]
struct FooAlign {
    a: u8,
}

assert_impl_all!(FooAlign: Unaligned);

#[derive(Unaligned)]
#[repr(transparent)]
struct Unsized {
    a: [u8],
}

assert_impl_all!(Unsized: Unaligned);

#[derive(Unaligned)]
#[repr(C)]
struct TypeParams<'a, T: ?Sized, I: Iterator> {
    a: I::Item,
    b: u8,
    c: PhantomData<&'a [u8]>,
    d: PhantomData<&'static str>,
    e: PhantomData<String>,
    f: T,
}

assert_impl_all!(TypeParams<'static, (), IntoIter<()>>: Unaligned);
assert_impl_all!(TypeParams<'static, u8, IntoIter<()>>: Unaligned);
assert_impl_all!(TypeParams<'static, [u8], IntoIter<()>>: Unaligned);

// Deriving `Unaligned` should work if the struct has bounded parameters.

#[derive(Unaligned)]
#[repr(transparent)]
struct WithParams<'a: 'b, 'b: 'a, const N: usize, T: 'a + 'b + Unaligned>(
    [T; N],
    PhantomData<&'a &'b ()>,
)
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + Unaligned;

assert_impl_all!(WithParams<'static, 'static, 42, u8>: Unaligned);
