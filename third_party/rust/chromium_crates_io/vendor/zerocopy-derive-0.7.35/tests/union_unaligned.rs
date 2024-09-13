// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

#![allow(warnings)]

use std::{marker::PhantomData, option::IntoIter};

use {static_assertions::assert_impl_all, zerocopy::Unaligned};

// A union is `Unaligned` if:
// - `repr(align)` is no more than 1 and either
//   - `repr(C)` or `repr(transparent)` and
//     - all fields `Unaligned`
//   - `repr(packed)`

#[derive(Unaligned)]
#[repr(C)]
union Foo {
    a: u8,
}

assert_impl_all!(Foo: Unaligned);

// Transparent unions are unstable; see issue #60405
// <https://github.com/rust-lang/rust/issues/60405> for more information.

// #[derive(Unaligned)]
// #[repr(transparent)]
// union Bar {
//     a: u8,
// }

// is_unaligned!(Bar);

#[derive(Unaligned)]
#[repr(packed)]
union Baz {
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
union FooAlign {
    a: u8,
}

assert_impl_all!(FooAlign: Unaligned);

#[derive(Unaligned)]
#[repr(C)]
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

assert_impl_all!(TypeParams<'static, (), IntoIter<()>>: Unaligned);
