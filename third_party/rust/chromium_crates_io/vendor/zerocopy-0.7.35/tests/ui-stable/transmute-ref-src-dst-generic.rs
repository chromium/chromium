// Copyright 2023 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

extern crate zerocopy;

use zerocopy::{transmute_ref, AsBytes, FromBytes};

fn main() {}

fn transmute_ref<T: AsBytes, U: FromBytes>(t: &T) -> &U {
    // `transmute_ref!` requires the source and destination types to be
    // concrete.
    transmute_ref!(t)
}
