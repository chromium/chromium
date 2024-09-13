// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

#![allow(warnings)]

use {static_assertions::assert_impl_all, zerocopy::AsBytes};

// An enum is `AsBytes` if if has a defined repr.

#[derive(AsBytes)]
#[repr(C)]
enum C {
    A,
}

assert_impl_all!(C: AsBytes);

#[derive(AsBytes)]
#[repr(u8)]
enum U8 {
    A,
}

assert_impl_all!(U8: AsBytes);

#[derive(AsBytes)]
#[repr(u16)]
enum U16 {
    A,
}

assert_impl_all!(U16: AsBytes);

#[derive(AsBytes)]
#[repr(u32)]
enum U32 {
    A,
}

assert_impl_all!(U32: AsBytes);

#[derive(AsBytes)]
#[repr(u64)]
enum U64 {
    A,
}

assert_impl_all!(U64: AsBytes);

#[derive(AsBytes)]
#[repr(usize)]
enum Usize {
    A,
}

assert_impl_all!(Usize: AsBytes);

#[derive(AsBytes)]
#[repr(i8)]
enum I8 {
    A,
}

assert_impl_all!(I8: AsBytes);

#[derive(AsBytes)]
#[repr(i16)]
enum I16 {
    A,
}

assert_impl_all!(I16: AsBytes);

#[derive(AsBytes)]
#[repr(i32)]
enum I32 {
    A,
}

assert_impl_all!(I32: AsBytes);

#[derive(AsBytes)]
#[repr(i64)]
enum I64 {
    A,
}

assert_impl_all!(I64: AsBytes);

#[derive(AsBytes)]
#[repr(isize)]
enum Isize {
    A,
}

assert_impl_all!(Isize: AsBytes);
