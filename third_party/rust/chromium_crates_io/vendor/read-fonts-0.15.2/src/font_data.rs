//! raw font bytes

use std::ops::{Range, RangeBounds};

use types::{BigEndian, FixedSize, Scalar};

use crate::array::ComputedArray;
use crate::read::{ComputeSize, FontReadWithArgs, FromBytes, ReadError};
use crate::table_ref::TableRef;
use crate::FontRead;

/// A reference to raw binary font data.
///
/// This is a wrapper around a byte slice, that provides convenience methods
/// for parsing and validating that data.
#[derive(Debug, Default, Clone, Copy)]
pub struct FontData<'a> {
    bytes: &'a [u8],
}

/// A cursor for validating bytes during parsing.
///
/// This type improves the ergonomics of validation blah blah
///
/// # Note
///
/// call `finish` when you're done to ensure you're in bounds
#[derive(Debug, Default, Clone, Copy)]
pub struct Cursor<'a> {
    pos: usize,
    data: FontData<'a>,
}

impl<'a> FontData<'a> {
    /// Create a new `FontData` with these bytes.
    ///
    /// You generally don't need to do this? It is handled for you when loading
    /// data from disk, but may be useful in tests.
    pub const fn new(bytes: &'a [u8]) -> Self {
        FontData { bytes }
    }

    /// The length of the data, in bytes
    pub fn len(&self) -> usize {
        self.bytes.len()
    }

    /// `true` if the data has a length of zero bytes.
    pub fn is_empty(&self) -> bool {
        self.bytes.is_empty()
    }

    /// Returns self[pos..]
    pub fn split_off(&self, pos: usize) -> Option<FontData<'a>> {
        self.bytes.get(pos..).map(|bytes| FontData { bytes })
    }

    /// returns self[..pos], and updates self to = self[pos..];
    pub fn take_up_to(&mut self, pos: usize) -> Option<FontData<'a>> {
        if pos > self.len() {
            return None;
        }
        let (head, tail) = self.bytes.split_at(pos);
        self.bytes = tail;
        Some(FontData { bytes: head })
    }

    pub fn slice(&self, range: impl RangeBounds<usize>) -> Option<FontData<'a>> {
        let bounds = (range.start_bound().cloned(), range.end_bound().cloned());
        self.bytes.get(bounds).map(|bytes| FontData { bytes })
    }

    /// Read a scalar at the provided location in the data.
    pub fn read_at<T: Scalar>(&self, offset: usize) -> Result<T, ReadError> {
        self.bytes
            .get(offset..offset + T::RAW_BYTE_LEN)
            .and_then(T::read)
            .ok_or(ReadError::OutOfBounds)
    }

    /// Read a big-endian value at the provided location in the data.
    pub fn read_be_at<T: Scalar>(&self, offset: usize) -> Result<BigEndian<T>, ReadError> {
        self.bytes
            .get(offset..offset + T::RAW_BYTE_LEN)
            .and_then(BigEndian::from_slice)
            .ok_or(ReadError::OutOfBounds)
    }

    pub fn read_with_args<T>(&self, range: Range<usize>, args: &T::Args) -> Result<T, ReadError>
    where
        T: FontReadWithArgs<'a>,
    {
        self.slice(range)
            .ok_or(ReadError::OutOfBounds)
            .and_then(|data| T::read_with_args(data, args))
    }

    fn check_in_bounds(&self, offset: usize) -> Result<(), ReadError> {
        self.bytes
            .get(..offset)
            .ok_or(ReadError::OutOfBounds)
            .map(|_| ())
    }

    /// Interpret the bytes at the provided offset as a reference to `T`.
    ///
    /// Returns an error if the slice `offset..` is shorter than `T::RAW_BYTE_LEN`.
    ///
    /// This is a wrapper around [`read_ref_unchecked`][], which panics if
    /// the type does not uphold the required invariants.
    ///
    /// # Panics
    ///
    /// This function will panic if `T` is zero-sized, has an alignment
    /// other than one, or has any internal padding.
    ///
    /// [`read_ref_unchecked`]: [Self::read_ref_unchecked]
    pub fn read_ref_at<T: FromBytes>(&self, offset: usize) -> Result<&'a T, ReadError> {
        assert_ne!(std::mem::size_of::<T>(), 0);
        // assert we have no padding.
        // `T::RAW_BYTE_LEN` is computed by recursively taking the raw length of
        // any fields of `T`; if `size_of::<T>() == T::RAW_BYTE_LEN`` we know that
        // `T` contains no padding.
        assert_eq!(std::mem::size_of::<T>(), T::RAW_BYTE_LEN);
        assert_eq!(std::mem::align_of::<T>(), 1);
        self.bytes
            .get(offset..offset + T::RAW_BYTE_LEN)
            .ok_or(ReadError::OutOfBounds)?;

        // SAFETY: if we have reached this point without hitting an assert or
        // returning an error, our invariants are met.
        unsafe { Ok(self.read_ref_unchecked(offset)) }
    }

    /// Interpret the bytes at `offset` as a reference to some type `T`.
    ///
    /// # Safety
    ///
    /// `T` must be a struct or scalar that has alignment of 1, a non-zero size,
    /// and no internal padding, and offset must point to a slice of bytes that
    /// has length >= `size_of::<T>()`.
    unsafe fn read_ref_unchecked<T: FromBytes>(&self, offset: usize) -> &'a T {
        let bytes = self.bytes.get_unchecked(offset..offset + T::RAW_BYTE_LEN);
        &*(bytes.as_ptr() as *const T)
    }

    /// Interpret the bytes at the provided offset as a slice of `T`.
    ///
    /// Returns an error if `range` is out of bounds for the underlying data,
    /// or if the length of the range is not a multiple of `T::RAW_BYTE_LEN`.
    ///
    /// This is a wrapper around [`read_array_unchecked`][], which panics if
    /// the type does not uphold the required invariants.
    ///
    /// # Panics
    ///
    /// This function will panic if `T` is zero-sized, has an alignment
    /// other than one, or has any internal padding.
    ///
    /// [`read_array_unchecked`]: [Self::read_array_unchecked]
    pub fn read_array<T: FromBytes>(&self, range: Range<usize>) -> Result<&'a [T], ReadError> {
        assert_ne!(std::mem::size_of::<T>(), 0);
        // assert we have no padding.
        // `T::RAW_BYTE_LEN` is computed by recursively taking the raw length of
        // any fields of `T`; if `size_of::<T>() == T::RAW_BYTE_LEN`` we know that
        // `T` contains no padding.
        assert_eq!(std::mem::size_of::<T>(), T::RAW_BYTE_LEN);
        assert_eq!(std::mem::align_of::<T>(), 1);
        let bytes = self
            .bytes
            .get(range.clone())
            .ok_or(ReadError::OutOfBounds)?;
        if bytes.len() % std::mem::size_of::<T>() != 0 {
            return Err(ReadError::InvalidArrayLen);
        };
        // SAFETY: if we have reached this point without hitting an assert or
        // returning an error, our invariants are met.
        unsafe { Ok(self.read_array_unchecked(range)) }
    }

    /// Interpret the bytes at `offset` as a reference to some type `T`.
    ///
    /// # Safety
    ///
    /// `T` must be a struct or scalar that has alignment of 1, a non-zero size,
    /// and no internal padding, and `range` must have a length that is non-zero
    /// and is a multiple of `size_of::<T>()`.
    unsafe fn read_array_unchecked<T: FromBytes>(&self, range: Range<usize>) -> &'a [T] {
        let bytes = self.bytes.get_unchecked(range);
        let elems = bytes.len() / std::mem::size_of::<T>();
        std::slice::from_raw_parts(bytes.as_ptr() as *const _, elems)
    }

    pub(crate) fn cursor(&self) -> Cursor<'a> {
        Cursor {
            pos: 0,
            data: *self,
        }
    }

    /// Return the data as a byte slice
    pub fn as_bytes(&self) -> &'a [u8] {
        self.bytes
    }
}

impl<'a> Cursor<'a> {
    pub(crate) fn advance<T: Scalar>(&mut self) {
        self.pos += T::RAW_BYTE_LEN
    }

    pub(crate) fn advance_by(&mut self, n_bytes: usize) {
        self.pos += n_bytes;
    }

    /// Read a scalar and advance the cursor.
    pub(crate) fn read<T: Scalar>(&mut self) -> Result<T, ReadError> {
        let temp = self.data.read_at(self.pos);
        self.pos += T::RAW_BYTE_LEN;
        temp
    }

    /// Read a big-endian value and advance the cursor.
    pub(crate) fn read_be<T: Scalar>(&mut self) -> Result<BigEndian<T>, ReadError> {
        let temp = self.data.read_be_at(self.pos);
        self.pos += T::RAW_BYTE_LEN;
        temp
    }

    pub(crate) fn read_with_args<T>(&mut self, args: &T::Args) -> Result<T, ReadError>
    where
        T: FontReadWithArgs<'a> + ComputeSize,
    {
        let len = T::compute_size(args);
        let temp = self.data.read_with_args(self.pos..self.pos + len, args);
        self.pos += len;
        temp
    }

    // only used in records that contain arrays :/
    pub(crate) fn read_computed_array<T>(
        &mut self,
        len: usize,
        args: &T::Args,
    ) -> Result<ComputedArray<'a, T>, ReadError>
    where
        T: FontReadWithArgs<'a> + ComputeSize,
    {
        let len = len * T::compute_size(args);
        let temp = self.data.read_with_args(self.pos..self.pos + len, args);
        self.pos += len;
        temp
    }

    pub(crate) fn read_array<T: FromBytes>(&mut self, n_elem: usize) -> Result<&'a [T], ReadError> {
        let len = n_elem * T::RAW_BYTE_LEN;
        let temp = self.data.read_array(self.pos..self.pos + len);
        self.pos += len;
        temp
    }

    /// read a value, validating it with the provided function if successful.
    //pub(crate) fn read_validate<T, F>(&mut self, f: F) -> Result<T, ReadError>
    //where
    //T: ReadScalar,
    //F: FnOnce(&T) -> bool,
    //{
    //let temp = self.read()?;
    //if f(&temp) {
    //Ok(temp)
    //} else {
    //Err(ReadError::ValidationError)
    //}
    //}

    //pub(crate) fn check_array<T: Scalar>(&mut self, len_bytes: usize) -> Result<(), ReadError> {
    //assert_ne!(std::mem::size_of::<BigEndian<T>>(), 0);
    //assert_eq!(std::mem::align_of::<BigEndian<T>>(), 1);
    //if len_bytes % T::SIZE != 0 {
    //return Err(ReadError::InvalidArrayLen);
    //}
    //self.data.check_in_bounds(self.pos + len_bytes)
    //todo!()
    //}

    /// return the current position, or an error if we are out of bounds
    pub(crate) fn position(&self) -> Result<usize, ReadError> {
        self.data.check_in_bounds(self.pos).map(|_| self.pos)
    }

    // used when handling fields with an implicit length, which must be at the
    // end of a table.
    pub(crate) fn remaining_bytes(&self) -> usize {
        self.data.len().saturating_sub(self.pos)
    }

    pub(crate) fn finish<T>(self, shape: T) -> Result<TableRef<'a, T>, ReadError> {
        let data = self.data;
        data.check_in_bounds(self.pos)?;
        Ok(TableRef { data, shape })
    }
}

// useful so we can have offsets that are just to data
impl<'a> FontRead<'a> for FontData<'a> {
    fn read(data: FontData<'a>) -> Result<Self, ReadError> {
        Ok(data)
    }
}

impl AsRef<[u8]> for FontData<'_> {
    fn as_ref(&self) -> &[u8] {
        self.bytes
    }
}

impl<'a> From<&'a [u8]> for FontData<'a> {
    fn from(src: &'a [u8]) -> FontData<'a> {
        FontData::new(src)
    }
}

//kind of ugly, but makes FontData work with FontBuilder. If FontBuilder stops using
//Cow in its API, we can probably get rid of this?
#[cfg(feature = "std")]
impl<'a> From<FontData<'a>> for std::borrow::Cow<'a, [u8]> {
    fn from(src: FontData<'a>) -> Self {
        src.bytes.into()
    }
}
