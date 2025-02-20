// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;
use std::simd::prelude::*;

chromium::import! {
    "//ui/android:texture_compressor";
}

use texture_compressor::interleave_etc1;

#[gtest(TextureCompressorTest, InterleaveEtc1)]
fn test_interleave_etc1() {
    let input =
        [Simd::splat(0x1234), Simd::splat(0x5678), Simd::splat(0x9ABC), Simd::splat(0xDEF0)];
    let expected = [Simd::splat(0x3412_7856_BC9A_F0DE); 4];
    let result = interleave_etc1(input);
    expect_eq!(result, expected);
}
