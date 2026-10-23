// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::error::Result;
use crate::image::{DataTypeTag, ImageDataType};
use crate::render::MAX_BORDER;
use crate::util::{
    CACHE_LINE_BYTE_SIZE, CacheLine, num_per_cache_line, slice_from_cachelines,
    slice_from_cachelines_mut,
};

/// Temporary storage for data rows. Note that the first pixel of the group is expected to be
/// located *one cacheline worth of data* inside the row.
pub struct RowBuffer {
    buffer: Box<[CacheLine]>,
    // Distance (in number of *cache lines*) between the start of two rows.
    row_stride: usize,
    // Number of rows that are actually stored (always a power of two).
    num_rows: usize,
}

impl RowBuffer {
    pub fn new(
        data_type: DataTypeTag,
        next_y_border: usize,
        y_shift: usize,
        x_shift: usize,
        row_len: usize,
    ) -> Result<Self> {
        let num_rows = (1 << y_shift) + 2 * next_y_border;
        let num_rows = num_rows.next_power_of_two();
        // Input offset is at *one* cacheline, and we need up to *two* cachelines on the other
        // side as the data might exceed xsize slightly.
        let row_stride =
            (row_len * data_type.size()).div_ceil(CACHE_LINE_BYTE_SIZE) + (3 << x_shift);
        let mut buffer = Vec::<CacheLine>::new();
        buffer.try_reserve_exact(row_stride * num_rows)?;
        buffer.resize(row_stride * num_rows, CacheLine::default());
        let buffer = buffer.into_boxed_slice();
        Ok(Self {
            buffer,
            row_stride,
            num_rows,
        })
    }

    #[inline]
    pub fn get_row<T: ImageDataType>(&self, row: usize) -> &[T] {
        let row_idx = row & (self.num_rows - 1);
        let start = row_idx * self.row_stride;
        slice_from_cachelines(&self.buffer[start..start + self.row_stride])
    }

    #[inline]
    pub fn get_row_mut<T: ImageDataType>(&mut self, row: usize) -> &mut [T] {
        let row_idx = row & (self.num_rows - 1);
        let stride = self.row_stride;
        let start = row_idx * stride;
        slice_from_cachelines_mut(&mut self.buffer[start..start + stride])
    }

    #[inline]
    pub fn as_slice<T: ImageDataType>(&self) -> &[T] {
        slice_from_cachelines(&self.buffer)
    }

    #[inline]
    pub fn as_mut_slice<T: ImageDataType>(&mut self) -> &mut [T] {
        slice_from_cachelines_mut(&mut self.buffer)
    }

    #[inline]
    pub fn stride_elements<T: ImageDataType>(&self) -> usize {
        self.row_stride * num_per_cache_line::<T>()
    }

    pub const fn x0_offset<T: ImageDataType>() -> usize {
        assert!(num_per_cache_line::<T>() >= MAX_BORDER);
        num_per_cache_line::<T>()
    }

    pub const fn x0_byte_offset() -> usize {
        CACHE_LINE_BYTE_SIZE
    }

    #[inline]
    pub fn num_rows(&self) -> usize {
        self.num_rows
    }
}
