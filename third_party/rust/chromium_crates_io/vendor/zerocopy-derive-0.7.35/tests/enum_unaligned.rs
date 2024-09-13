// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

#![allow(warnings)]

use {static_assertions::assert_impl_all, zerocopy::Unaligned};

// An enum is `Unaligned` if:
// - No `repr(align(N > 1))`
// - `repr(u8)` or `repr(i8)`

#[derive(Unaligned)]
#[repr(u8)]
enum Foo {
    A,
}

assert_impl_all!(Foo: Unaligned);

#[derive(Unaligned)]
#[repr(i8)]
enum Bar {
    A,
}

assert_impl_all!(Bar: Unaligned);

#[derive(Unaligned)]
#[repr(u8, align(1))]
enum Baz {
    A,
}

assert_impl_all!(Baz: Unaligned);

#[derive(Unaligned)]
#[repr(i8, align(1))]
enum Blah {
    B,
}

assert_impl_all!(Blah: Unaligned);
