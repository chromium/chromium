// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

#![allow(warnings)]

mod util;

use {static_assertions::assert_impl_all, zerocopy::FromZeroes};

#[derive(FromZeroes)]
enum Foo {
    A,
}

assert_impl_all!(Foo: FromZeroes);

#[derive(FromZeroes)]
enum Bar {
    A = 0,
}

assert_impl_all!(Bar: FromZeroes);

#[derive(FromZeroes)]
enum Baz {
    A = 1,
    B = 0,
}

assert_impl_all!(Baz: FromZeroes);
