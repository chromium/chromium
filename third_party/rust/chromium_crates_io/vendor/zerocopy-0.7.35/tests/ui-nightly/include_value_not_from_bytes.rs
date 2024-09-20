// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[macro_use]
extern crate zerocopy;

fn main() {}

// Should fail because `UnsafeCell<u32>: !FromBytes`.
const NOT_FROM_BYTES: core::cell::UnsafeCell<u32> =
    include_value!("../../testdata/include_value/data");
