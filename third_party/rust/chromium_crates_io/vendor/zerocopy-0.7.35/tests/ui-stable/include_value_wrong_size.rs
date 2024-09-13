// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[macro_use]
extern crate zerocopy;

fn main() {}

// Should fail because the file is 4 bytes long, not 8.
const WRONG_SIZE: u64 = include_value!("../../testdata/include_value/data");
