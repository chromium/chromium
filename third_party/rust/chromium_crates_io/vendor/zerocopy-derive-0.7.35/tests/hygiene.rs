// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

// Make sure that macro hygiene will ensure that when we reference "zerocopy",
// that will work properly even if they've renamed the crate and have not
// imported its traits.

#![allow(warnings)]

extern crate zerocopy as _zerocopy;

#[macro_use]
mod util;

use std::{marker::PhantomData, option::IntoIter};

use static_assertions::assert_impl_all;

#[derive(
    _zerocopy::KnownLayout, _zerocopy::FromZeroes, _zerocopy::FromBytes, _zerocopy::Unaligned,
)]
#[repr(C)]
struct TypeParams<'a, T, I: Iterator> {
    a: T,
    c: I::Item,
    d: u8,
    e: PhantomData<&'a [u8]>,
    f: PhantomData<&'static str>,
    g: PhantomData<String>,
}

assert_impl_all!(
    TypeParams<'static, (), IntoIter<()>>:
        _zerocopy::KnownLayout,
        _zerocopy::FromZeroes,
        _zerocopy::FromBytes,
        _zerocopy::Unaligned
);
