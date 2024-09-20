// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

use zerocopy::{AsBytes, FromBytes, FromZeroes, KnownLayout};

/// A type that doesn't implement any zerocopy traits.
pub struct NotZerocopy<T = ()>(T);

/// A `u16` with alignment 2.
///
/// Though `u16` has alignment 2 on some platforms, it's not guaranteed. By
/// contrast, `AU16` is guaranteed to have alignment 2.
#[derive(KnownLayout, FromZeroes, FromBytes, AsBytes, Copy, Clone)]
#[repr(C, align(2))]
pub struct AU16(u16);
