// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{fmt::Debug, marker::PhantomData};

use super::{RawImageRectMut, Rect, internal::RawImageBuffer};

#[derive(Debug)]
#[repr(transparent)]
pub struct JxlOutputBuffer<'a> {
    // Safety invariant: `self` has exclusive (write) access to the accessible bytes of `inner`.
    inner: RawImageBuffer,
    _ph: PhantomData<&'a mut u8>,
}

impl<'a> JxlOutputBuffer<'a> {
    /// Creates a new JxlOutputBuffer from raw pointers.
    ///
    /// # Safety
    /// - `buf` must be valid for writes for all bytes in the range
    ///   `buf[i*bytes_between_rows..i*bytes_between_rows+bytes_per_row]` for all values of `i`
    ///   from `0` to `num_rows-1`.
    /// - `buf` must be initialized for all the bytes in these ranges.
    /// - The bytes in these ranges must not be accessed as long as the returned `Self` is in scope.
    /// - All the bytes in those ranges (and in between) must be part of the same allocated object.
    pub unsafe fn new_from_ptr(
        buf: *mut u8,
        num_rows: usize,
        bytes_per_row: usize,
        bytes_between_rows: usize,
    ) -> Self {
        JxlOutputBuffer {
            // SAFETY: the safety conditions on RawImageBuffer::new_from_ptr are strictly weaker.
            // We are promised write access to the underlying data, so our own safety invariant is
            // respected.
            inner: unsafe {
                RawImageBuffer::new_from_ptr(
                    buf.cast(),
                    num_rows,
                    bytes_per_row,
                    bytes_between_rows,
                )
            },
            _ph: PhantomData,
        }
    }

    pub fn from_image_rect_mut(raw: RawImageRectMut<'a>) -> Self {
        Self {
            // Safety note: since `raw` has exclusive access to the data, we are just transferring
            // this access.
            inner: raw.data,
            _ph: PhantomData,
        }
    }

    pub fn new_with_stride(
        buf: &'a mut [u8],
        num_rows: usize,
        bytes_per_row: usize,
        byte_stride: usize,
    ) -> Self {
        assert_ne!(num_rows, 0);
        assert!(
            buf.len()
                >= byte_stride
                    .checked_mul(num_rows - 1)
                    .unwrap()
                    .checked_add(bytes_per_row)
                    .unwrap()
        );
        // SAFETY: The assert above guarantees that `buf` has enough space to satisfy the first
        // safety requirement, and the rest follow from borrowing from a &mut [].
        unsafe { Self::new_from_ptr(buf.as_mut_ptr(), num_rows, bytes_per_row, byte_stride) }
    }

    pub fn new(buf: &'a mut [u8], num_rows: usize, bytes_per_row: usize) -> Self {
        Self::new_with_stride(buf, num_rows, bytes_per_row, bytes_per_row)
    }

    pub(crate) fn reborrow(lender: &'a mut JxlOutputBuffer<'_>) -> JxlOutputBuffer<'a> {
        // Safety note: this is effectively equivalent to a reborrow.
        Self {
            _ph: PhantomData,
            ..*lender
        }
    }

    pub(crate) fn row_mut(&mut self, row: usize) -> &mut [u8] {
        // SAFETY: we have write access to the data due to safety invariant.
        unsafe { self.inner.row_mut(row) }
    }

    pub fn byte_size(&self) -> (usize, usize) {
        self.inner.byte_size()
    }

    pub fn rect(&mut self, rect: Rect) -> JxlOutputBuffer<'_> {
        // Safety note: the return value borrows from `self`, so we are lending our memory to the
        // returned JxlOutputBuffer.
        Self {
            inner: self.inner.rect(rect),
            _ph: PhantomData,
        }
    }
}
