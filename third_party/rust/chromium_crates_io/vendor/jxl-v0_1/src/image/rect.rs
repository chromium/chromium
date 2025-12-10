// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use super::DataTypeTag;

#[derive(Clone, Copy, Debug)]
pub struct Rect {
    pub origin: (usize, usize),
    // width, height
    pub size: (usize, usize),
}

impl Rect {
    pub fn check_within(&self, size: (usize, usize)) {
        if self.origin.0.checked_add(self.size.0).unwrap() > size.0
            || self.origin.1.checked_add(self.size.1).unwrap() > size.1
        {
            panic!(
                "Rect out of bounds: {}x{}+{}+{} rect in {}x{} view",
                self.size.0, self.size.1, self.origin.0, self.origin.1, size.0, size.1
            );
        }
    }

    pub fn to_byte_rect(&self, data_type: DataTypeTag) -> Rect {
        self.to_byte_rect_sz(data_type.size())
    }

    pub fn to_byte_rect_sz(&self, sz: usize) -> Rect {
        Rect {
            origin: (self.origin.0 * sz, self.origin.1),
            size: (self.size.0 * sz, self.size.1),
        }
    }

    pub fn downsample(&self, downsample: (u8, u8)) -> Rect {
        Rect {
            origin: (self.origin.0 >> downsample.0, self.origin.1 >> downsample.1),
            size: (self.size.0 >> downsample.0, self.size.1 >> downsample.1),
        }
    }

    pub fn end(&self) -> (usize, usize) {
        (self.origin.0 + self.size.0, self.origin.1 + self.size.1)
    }

    pub fn clip(&self, size: (usize, usize)) -> Rect {
        let end = self.end();
        Rect {
            origin: self.origin,
            size: (
                end.0.min(size.0).saturating_sub(self.origin.0),
                end.1.min(size.1).saturating_sub(self.origin.1),
            ),
        }
    }
}
