// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{fmt::Debug, marker::PhantomData};

use crate::{
    error::Result,
    image::internal::DistinctRowsIndexes,
    util::{CACHE_LINE_BYTE_SIZE, tracing_wrappers::*},
};

use super::{ImageDataType, OwnedRawImage, RawImageRect, RawImageRectMut, Rect};

#[repr(transparent)]
pub struct Image<T: ImageDataType> {
    // Safety invariant: self.raw.data.is_aligned(T::DATA_TYPE_ID.size()) is true.
    raw: OwnedRawImage,
    _ph: PhantomData<T>,
}

impl<T: ImageDataType> Image<T> {
    #[instrument(ret, err)]
    pub fn new_with_padding(
        size: (usize, usize),
        offset: (usize, usize),
        padding: (usize, usize),
    ) -> Result<Image<T>> {
        let s = T::DATA_TYPE_ID.size();
        let img = OwnedRawImage::new_zeroed_with_padding(
            (size.0 * s, size.1),
            (offset.0 * s, offset.1),
            (padding.0 * s, padding.1),
        )?;
        Ok(Self::from_raw(img))
    }

    #[instrument(ret, err)]
    pub fn new(size: (usize, usize)) -> Result<Image<T>> {
        Self::new_with_padding(size, (0, 0), (0, 0))
    }

    pub fn new_with_value(size: (usize, usize), value: T) -> Result<Image<T>> {
        // TODO(veluca): skip zero-initializing the allocation if this becomes
        // performance-sensitive.
        let mut ret = Self::new(size)?;
        ret.fill(value);
        Ok(ret)
    }

    pub fn size(&self) -> (usize, usize) {
        (
            self.raw.byte_size().0 / T::DATA_TYPE_ID.size(),
            self.raw.byte_size().1,
        )
    }

    pub fn offset(&self) -> (usize, usize) {
        (
            self.raw.byte_offset().0 / T::DATA_TYPE_ID.size(),
            self.raw.byte_offset().1,
        )
    }

    pub fn padding(&self) -> (usize, usize) {
        (
            self.raw.byte_padding().0 / T::DATA_TYPE_ID.size(),
            self.raw.byte_padding().1,
        )
    }

    pub fn fill(&mut self, v: T) {
        if self.size().0 == 0 {
            return;
        }
        for y in 0..self.size().1 {
            self.row_mut(y).fill(v);
        }
    }

    pub fn get_rect_including_padding_mut(&mut self, rect: Rect) -> ImageRectMut<'_, T> {
        ImageRectMut::from_raw(
            self.raw
                .get_rect_including_padding_mut(rect.to_byte_rect(T::DATA_TYPE_ID)),
        )
    }

    pub fn get_rect_including_padding(&mut self, rect: Rect) -> ImageRect<'_, T> {
        ImageRect::from_raw(
            self.raw
                .get_rect_including_padding(rect.to_byte_rect(T::DATA_TYPE_ID)),
        )
    }

    pub fn get_rect_mut(&mut self, rect: Rect) -> ImageRectMut<'_, T> {
        ImageRectMut::from_raw(self.raw.get_rect_mut(rect.to_byte_rect(T::DATA_TYPE_ID)))
    }

    pub fn get_rect(&self, rect: Rect) -> ImageRect<'_, T> {
        ImageRect::from_raw(self.raw.get_rect(rect.to_byte_rect(T::DATA_TYPE_ID)))
    }

    pub fn try_clone(&self) -> Result<Self> {
        Ok(Self::from_raw(self.raw.try_clone()?))
    }

    pub fn into_raw(self) -> OwnedRawImage {
        self.raw
    }

    pub fn from_raw(raw: OwnedRawImage) -> Self {
        const { assert!(CACHE_LINE_BYTE_SIZE.is_multiple_of(T::DATA_TYPE_ID.size())) };
        assert!(raw.data.is_aligned(T::DATA_TYPE_ID.size()));
        Image {
            // Safety note: we just checked alignment.
            raw,
            _ph: PhantomData,
        }
    }

    #[inline(always)]
    pub fn row(&self, row: usize) -> &[T] {
        let row = self.raw.row(row);
        // SAFETY: Since self.raw.data.is_aligned(T::DATA_TYPE_ID.size()) by the safety invariant
        // on `self`, the returned slice is aligned to T::DATA_TYPE_ID.size(), and sizeof(T) ==
        // T::DATA_TYPE_ID.size() by the requirements of ImageDataType; moreover, ImageDataType
        // requires T to be a bag-of-bits type with no padding, so the implicit transmute is not
        // an issue.
        unsafe {
            std::slice::from_raw_parts(row.as_ptr().cast::<T>(), row.len() / T::DATA_TYPE_ID.size())
        }
    }

    #[inline(always)]
    pub fn row_mut(&mut self, row: usize) -> &mut [T] {
        let row = self.raw.row_mut(row);
        // SAFETY: Since self.raw.data.is_aligned(T::DATA_TYPE_ID.size()) by the safety invariant
        // on `self`, the returned slice is aligned to T::DATA_TYPE_ID.size(), and sizeof(T) ==
        // T::DATA_TYPE_ID.size() by the requirements of ImageDataType; moreover, ImageDataType
        // requires T to be a bag-of-bits type with no padding, so the implicit transmute is not
        // an issue.
        unsafe {
            std::slice::from_raw_parts_mut(
                row.as_mut_ptr().cast::<T>(),
                row.len() / T::DATA_TYPE_ID.size(),
            )
        }
    }

    /// Note: this is quadratic in the number of rows. Indexing *ignores any padding rows*, i.e.
    /// the row at index 0 will be the first row of the *padding*, unlike with all the other row
    /// accessors.
    #[inline(always)]
    pub fn distinct_full_rows_mut<I: DistinctRowsIndexes>(&mut self, rows: I) -> I::Output<'_, T> {
        // SAFETY: we don't write uninit data to the returned `rows`, and `self.raw` has ownership
        // of the accessible bytes of `self.raw.data`.
        let rows = unsafe { self.raw.data.distinct_rows_mut(rows) };
        // SAFETY: Since self.raw.data.is_aligned(T::DATA_TYPE_ID.size()) by the safety invariant
        // on `self`, the returned slices are aligned to T::DATA_TYPE_ID.size(), and sizeof(T)
        // == T::DATA_TYPE_ID.size() by the requirements of ImageDataType; moreover, ImageDataType
        // requires T to be a bag-of-bits type with no padding and `self.raw` guarantees its
        // accessible bytes are initialized, so the transmute is not an issue.
        unsafe { I::transmute_rows(rows) }
    }
}

#[derive(Clone, Copy)]
pub struct ImageRect<'a, T: ImageDataType> {
    // Safety invariant: self.raw.data.is_aligned(T::DATA_TYPE_ID.size()) is true.
    raw: RawImageRect<'a>,
    _ph: PhantomData<T>,
}

impl<'a, T: ImageDataType> ImageRect<'a, T> {
    pub fn rect(&self, rect: Rect) -> ImageRect<'a, T> {
        Self::from_raw(self.raw.rect(rect.to_byte_rect(T::DATA_TYPE_ID)))
    }

    pub fn size(&self) -> (usize, usize) {
        (
            self.raw.byte_size().0 / T::DATA_TYPE_ID.size(),
            self.raw.byte_size().1,
        )
    }

    #[inline(always)]
    pub fn row(&self, row: usize) -> &'a [T] {
        let row = self.raw.row(row);
        // SAFETY: Since self.raw.data.is_aligned(T::DATA_TYPE_ID.size()) by the safety invariant
        // on `self`, the returned slice is aligned to T::DATA_TYPE_ID.size(), and sizeof(T) ==
        // T::DATA_TYPE_ID.size() by the requirements of ImageDataType; moreover, ImageDataType
        // requires T to be a bag-of-bits type with no padding, so the implicit transmute is not
        // an issue.
        unsafe {
            std::slice::from_raw_parts(row.as_ptr().cast::<T>(), row.len() / T::DATA_TYPE_ID.size())
        }
    }

    pub fn iter(&self) -> impl Iterator<Item = T> + '_ {
        (0..self.size().1).flat_map(|x| self.row(x).iter().cloned())
    }

    pub fn into_raw(self) -> RawImageRect<'a> {
        self.raw
    }

    pub fn from_raw(raw: RawImageRect<'a>) -> Self {
        const { assert!(CACHE_LINE_BYTE_SIZE.is_multiple_of(T::DATA_TYPE_ID.size())) };
        assert!(raw.data.is_aligned(T::DATA_TYPE_ID.size()));
        ImageRect {
            // Safety note: we just checked alignment.
            raw,
            _ph: PhantomData,
        }
    }
}

pub struct ImageRectMut<'a, T: ImageDataType> {
    // Safety invariant: self.raw.data.is_aligned(T::DATA_TYPE_ID.size()) is true.
    raw: RawImageRectMut<'a>,
    _ph: PhantomData<T>,
}

impl<'a, T: ImageDataType> ImageRectMut<'a, T> {
    pub fn rect(&'a mut self, rect: Rect) -> ImageRectMut<'a, T> {
        Self::from_raw(self.raw.rect_mut(rect.to_byte_rect(T::DATA_TYPE_ID)))
    }

    pub fn size(&self) -> (usize, usize) {
        (
            self.raw.byte_size().0 / T::DATA_TYPE_ID.size(),
            self.raw.byte_size().1,
        )
    }

    #[inline(always)]
    pub fn row(&mut self, row: usize) -> &mut [T] {
        let row = self.raw.row(row);
        // SAFETY: Since self.raw.data.is_aligned(T::DATA_TYPE_ID.size()) by the safety invariant
        // on `self`, the returned slice is aligned to T::DATA_TYPE_ID.size(), and sizeof(T) ==
        // T::DATA_TYPE_ID.size() by the requirements of ImageDataType; moreover, ImageDataType
        // requires T to be a bag-of-bits type with no padding, so the implicit transmute is not
        // an issue.
        unsafe {
            std::slice::from_raw_parts_mut(
                row.as_mut_ptr().cast::<T>(),
                row.len() / T::DATA_TYPE_ID.size(),
            )
        }
    }

    pub fn as_rect(&'a self) -> ImageRect<'a, T> {
        ImageRect::from_raw(self.raw.as_rect())
    }

    pub fn into_raw(self) -> RawImageRectMut<'a> {
        self.raw
    }

    pub fn from_raw(raw: RawImageRectMut<'a>) -> Self {
        const { assert!(CACHE_LINE_BYTE_SIZE.is_multiple_of(T::DATA_TYPE_ID.size())) };
        assert!(raw.data.is_aligned(T::DATA_TYPE_ID.size()));
        ImageRectMut {
            // Safety note: we just checked alignment.
            raw,
            _ph: PhantomData,
        }
    }
}

impl<T: ImageDataType> Debug for Image<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{:?} {}x{}",
            T::DATA_TYPE_ID,
            self.size().0,
            self.size().1
        )
    }
}

impl<T: ImageDataType> Debug for ImageRect<'_, T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{:?} rect {}x{}",
            T::DATA_TYPE_ID,
            self.size().0,
            self.size().1
        )
    }
}

impl<T: ImageDataType> Debug for ImageRectMut<'_, T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{:?} mutrect {}x{}",
            T::DATA_TYPE_ID,
            self.size().0,
            self.size().1
        )
    }
}
