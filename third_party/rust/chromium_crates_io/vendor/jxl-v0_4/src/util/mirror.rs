// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

/// Mirror-reflects a value v to fit in a [0; s) range.
pub fn mirror(mut v: isize, s: usize) -> usize {
    // TODO(veluca): consider speeding this up if needed.
    loop {
        if v < 0 {
            v = -v - 1;
        } else if v >= s as isize {
            v = s as isize * 2 - v - 1;
        } else {
            return v as usize;
        }
    }
}
