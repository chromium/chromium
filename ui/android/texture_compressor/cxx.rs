// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge]
mod ffi {
    extern "Rust" {
        fn compress_etc1(
            src: &[u32],
            dst: &mut [u8],
            width: u32,
            height: u32,
            src_row_width: u32,
            dst_row_width: u32,
        );
    }
}

use crate::compress_etc1;
