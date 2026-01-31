// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{fmt::Debug, marker::PhantomData};

use crate::{error::Result, util::CACHE_LINE_BYTE_SIZE};

use super::{Rect, internal::RawImageBuffer};

pub struct OwnedRawImage {
    // Safety invariant: all the accessible bytes of `self.data` are initialized, and
    // belongs to a single allocation that lives until `self` is dropped.
    // The data referenced by self.data was allocated by RawImageBuffer::try_allocate.
    // `data.is_aligned(CACHE_LINE_BYTE_SIZE)` is true.
    pub(super) data: RawImageBuffer,
    offset: (usize, usize),
    padding: (usize, usize),
}

impl OwnedRawImage {
    pub fn new_zeroed_with_padding(
        byte_size: (usize, usize),
        offset: (usize, usize),
        mut padding: (usize, usize),
    ) -> Result<Self> {
        // Since RawImageBuffer::try_allocate will round up the length of a row to a cache line,
        // might as well declare that as available padding space.
        if !(padding.0 + byte_size.0).is_multiple_of(CACHE_LINE_BYTE_SIZE) {
            padding.0 += CACHE_LINE_BYTE_SIZE - (padding.0 + byte_size.0) % CACHE_LINE_BYTE_SIZE;
        }
        Ok(Self {
            // Safety note: the returned memory is initialized and part of a single allocation of
            // the correct length.
            data: RawImageBuffer::try_allocate(
                (byte_size.0 + padding.0, byte_size.1 + padding.1),
                false,
            )?,
            offset,
            padding,
        })
    }

    pub fn get_rect_including_padding_mut(&mut self, rect: Rect) -> RawImageRectMut<'_> {
        RawImageRectMut {
            // Safety note: we are lending exclusive ownership to RawImageRectMut.
            data: self.data.rect(rect),
            _ph: PhantomData,
        }
    }

    pub fn get_rect_including_padding(&'_ self, rect: Rect) -> RawImageRect<'_> {
        RawImageRect {
            // Safety note: correctness ensured by the return value borrowing from `self`.
            data: self.data.rect(rect),
            _ph: PhantomData,
        }
    }

    fn shift_rect(&self, rect: Rect) -> Rect {
        if cfg!(debug_assertions) {
            // Check the original rect is within the content size (without padding)
            rect.check_within(self.byte_size());
        }
        Rect {
            origin: (rect.origin.0 + self.offset.0, rect.origin.1 + self.offset.1),
            size: rect.size,
        }
    }

    pub fn get_rect_mut(&mut self, rect: Rect) -> RawImageRectMut<'_> {
        self.get_rect_including_padding_mut(self.shift_rect(rect))
    }

    pub fn get_rect(&'_ self, rect: Rect) -> RawImageRect<'_> {
        self.get_rect_including_padding(self.shift_rect(rect))
    }

    #[inline(always)]
    pub fn row_mut(&mut self, row: usize) -> &mut [u8] {
        let offset = self.offset;
        let end = offset.0 + self.byte_size().0;
        // SAFETY: we don't write uninit data to `row`, and we have ownership of the accessible
        // bytes of `self.data`.
        let row = &mut unsafe { self.data.row_mut(row + offset.1) }[offset.0..end];
        // SAFETY: MaybeUninit<u8> and u8 have the same size and layout, and our safety invariant
        // guarantees the data is initialized.
        unsafe { std::slice::from_raw_parts_mut(row.as_mut_ptr().cast::<u8>(), row.len()) }
    }

    #[inline(always)]
    pub fn row(&self, row: usize) -> &[u8] {
        let offset = self.offset;
        let end = offset.0 + self.byte_size().0;
        // SAFETY: we have shared access to the accessible bytes of `self.data`.
        let row = &unsafe { self.data.row(row + offset.1) }[offset.0..end];
        // SAFETY: MaybeUninit<u8> and u8 have the same size and layout, and our safety invariant
        // guarantees the data is initialized.
        unsafe { std::slice::from_raw_parts(row.as_ptr().cast::<u8>(), row.len()) }
    }

    pub fn byte_size(&self) -> (usize, usize) {
        let size = self.data.byte_size();
        (size.0 - self.padding.0, size.1 - self.padding.1)
    }

    pub fn byte_offset(&self) -> (usize, usize) {
        self.offset
    }

    pub fn byte_padding(&self) -> (usize, usize) {
        self.padding
    }

    pub fn try_clone(&self) -> Result<OwnedRawImage> {
        Ok(Self {
            // SAFETY: we own the data that self.data references, so it is all accessible.
            // Moreover, it is initialized and try_clone creates a copy, so the resulting data is
            // owned and initialized.
            data: unsafe { self.data.try_clone()? },
            offset: self.offset,
            padding: self.padding,
        })
    }
}

impl Drop for OwnedRawImage {
    fn drop(&mut self) {
        // SAFETY: we own the data referenced by self.data, and it was allocated by
        // RawImageBuffer::try_allocate.
        unsafe {
            self.data.deallocate();
        }
    }
}

#[derive(Clone, Copy)]
pub struct RawImageRect<'a> {
    // Safety invariant: all the accessible bytes of `self.data` are initialized.
    pub(super) data: RawImageBuffer,
    _ph: PhantomData<&'a u8>,
}

impl<'a> RawImageRect<'a> {
    #[inline(always)]
    pub fn row(&self, row: usize) -> &[u8] {
        // SAFETY: we have shared access to the accessible bytes of `self.data`.
        let row = unsafe { self.data.row(row) };
        // SAFETY: MaybeUninit<u8> and u8 have the same size and layout, and our safety invariant
        // guarantees the data is initialized.
        unsafe { std::slice::from_raw_parts(row.as_ptr().cast::<u8>(), row.len()) }
    }

    pub fn rect(&self, rect: Rect) -> RawImageRect<'a> {
        Self {
            // Safety note: correctness ensured by the fact that the return value still borrows
            // from the original data source.
            data: self.data.rect(rect),
            _ph: PhantomData,
        }
    }

    pub fn byte_size(&self) -> (usize, usize) {
        self.data.byte_size()
    }
}

pub struct RawImageRectMut<'a> {
    // Safety invariant: all the accessible bytes of `self.data` are initialized and we have
    // exclusive access to them.
    pub(super) data: RawImageBuffer,
    _ph: PhantomData<&'a mut u8>,
}

impl<'a> RawImageRectMut<'a> {
    #[inline(always)]
    pub fn row(&mut self, row: usize) -> &mut [u8] {
        // SAFETY: we don't write uninit data to `row`, and we have exclusive access to the accessible
        // bytes of `self.data`.
        let row = unsafe { self.data.row_mut(row) };
        // SAFETY: MaybeUninit<u8> and u8 have the same size and layout, and our safety invariant
        // guarantees the data is initialized.
        unsafe { std::slice::from_raw_parts_mut(row.as_mut_ptr().cast::<u8>(), row.len()) }
    }

    pub fn rect_mut(&'_ mut self, rect: Rect) -> RawImageRectMut<'_> {
        Self {
            // Safety note: we are lending ownership to the returned RawImageRectMut, and Rust's
            // type system ensures correctness.
            data: self.data.rect(rect),
            _ph: PhantomData,
        }
    }

    pub fn as_rect(&'_ self) -> RawImageRect<'_> {
        RawImageRect {
            // Safety note: correctness ensured by the return value borrowing from self.
            data: self.data,
            _ph: PhantomData,
        }
    }

    pub fn byte_size(&self) -> (usize, usize) {
        self.data.byte_size()
    }
}

impl Debug for OwnedRawImage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "raw {}x{}", self.byte_size().0, self.byte_size().1)
    }
}

impl Debug for RawImageRect<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "raw rect {}x{}", self.byte_size().0, self.byte_size().1)
    }
}

impl Debug for RawImageRectMut<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "raw mutrect {}x{}",
            self.byte_size().0,
            self.byte_size().1
        )
    }
}
