// Copyright 2022 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

// Since some macros from `macros.rs` are unused.
#![allow(unused)]

extern crate zerocopy;
extern crate zerocopy_derive;

include!("../../../src/macros.rs");

use zerocopy::*;
use zerocopy_derive::*;

fn main() {}

#[derive(FromZeroes, FromBytes, AsBytes, Unaligned)]
#[repr(transparent)]
struct Foo<T>(T);

impl_or_verify!(T => FromZeroes for Foo<T>);
impl_or_verify!(T => FromBytes for Foo<T>);
impl_or_verify!(T => AsBytes for Foo<T>);
impl_or_verify!(T => Unaligned for Foo<T>);
