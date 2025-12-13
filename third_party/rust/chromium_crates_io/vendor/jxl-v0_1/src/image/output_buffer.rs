// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{fmt::Debug, marker::PhantomData, mem::MaybeUninit};

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
    /// It is guaranteed that `buf` will never be used to write uninitialized data.
    ///
    /// # Safety
    /// - `buf` must be valid for writes for all bytes in the range
    ///   `buf[i*bytes_between_rows..i*bytes_between_rows+bytes_per_row]` for all values of `i`
    ///   from `0` to `num_rows-1`.
    /// - The bytes in these ranges must not be accessed as long as the returned `Self` is in scope.
    /// - All the bytes in those ranges (and in between) must be part of the same allocated object.
    pub unsafe fn new_from_ptr(
        buf: *mut MaybeUninit<u8>,
        num_rows: usize,
        bytes_per_row: usize,
        bytes_between_rows: usize,
    ) -> Self {
        JxlOutputBuffer {
            // SAFETY: the safety conditions on RawImageBuffer::new_from_ptr are strictly weaker.
            // We are promised write access to the underlying data, so our own safety invariant is
            // respected.
            inner: unsafe {
                RawImageBuffer::new_from_ptr(buf, num_rows, bytes_per_row, bytes_between_rows)
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

    /// Creates a new JxlOutputBuffer from a slice of uninit data.
    /// It is guaranteed that `buf` will never be used to write uninitalized data.
    pub fn new_uninit(
        buf: &'a mut [MaybeUninit<u8>],
        num_rows: usize,
        bytes_per_row: usize,
    ) -> Self {
        assert!(buf.len() >= bytes_per_row * num_rows);
        // SAFETY: The assert above guarantees that `buf` has enough space to satisfy the first
        // safety requirement, and the rest follow from borrowing from a &mut [].
        unsafe { Self::new_from_ptr(buf.as_mut_ptr(), num_rows, bytes_per_row, bytes_per_row) }
    }

    pub fn new(buf: &'a mut [u8], num_rows: usize, bytes_per_row: usize) -> Self {
        Self::new_uninit(
            // SAFETY: `new_uninit` guarantees that no uninit data is ever written to the passed-in
            // slice. Moreover, `T` and `MaybeUninit<T>` have the same memory layout.
            unsafe { std::slice::from_raw_parts_mut(buf.as_mut_ptr().cast(), buf.len()) },
            num_rows,
            bytes_per_row,
        )
    }

    pub(crate) fn reborrow(lender: &'a mut JxlOutputBuffer<'_>) -> JxlOutputBuffer<'a> {
        // Safety note: this is effectively equivalent to a reborrow.
        Self {
            _ph: PhantomData,
            ..*lender
        }
    }

    /// # Safety
    /// The caller must guarantee that the returned slice is not used for writing uninit data.
    pub(crate) unsafe fn row_mut(&mut self, row: usize) -> &mut [MaybeUninit<u8>] {
        // SAFETY: caller guarantees no uninit data is written, and we have write access to the
        // data due to safety invariant.
        unsafe { self.inner.row_mut(row) }
    }

    #[inline]
    pub fn write_bytes(&mut self, row: usize, col: usize, bytes: &[u8]) {
        // SAFETY: We never use the returned slice to write uninit data, and we have write access
        // to the data.
        let slice = unsafe { self.inner.row_mut(row) };
        for (w, s) in slice.iter_mut().skip(col).zip(bytes.iter().copied()) {
            w.write(s);
        }
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
