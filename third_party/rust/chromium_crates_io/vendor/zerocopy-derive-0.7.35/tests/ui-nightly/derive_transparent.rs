// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

extern crate zerocopy;

#[path = "../util.rs"]
mod util;

use core::marker::PhantomData;

use {
    static_assertions::assert_impl_all,
    zerocopy::{AsBytes, FromBytes, FromZeroes, Unaligned},
};

use self::util::NotZerocopy;

fn main() {}

// Test generic transparent structs

#[derive(AsBytes, FromZeroes, FromBytes, Unaligned)]
#[repr(transparent)]
struct TransparentStruct<T> {
    inner: T,
    _phantom: PhantomData<()>,
}

// It should be legal to derive these traits on a transparent struct, but it
// must also ensure the traits are only implemented when the inner type
// implements them.
assert_impl_all!(TransparentStruct<NotZerocopy>: FromZeroes);
assert_impl_all!(TransparentStruct<NotZerocopy>: FromBytes);
assert_impl_all!(TransparentStruct<NotZerocopy>: AsBytes);
assert_impl_all!(TransparentStruct<NotZerocopy>: Unaligned);
