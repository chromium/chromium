//! raw font bytes

use std::ops::{Range, RangeBounds};

use types::{FixedSize, ReadScalar};

use crate::array::ComputedArray;
use crate::read::{ComputeSize, FontReadWithArgs, ReadError};
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

    pub fn read_at<T: ReadScalar>(&self, offset: usize) -> Result<T, ReadError> {
        self.bytes
            .get(offset..offset + T::RAW_BYTE_LEN)
            .and_then(T::read)
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

    /// Read a scalar value out of the buffer at `offset`, elliding bounds checks
    ///
    /// # Safety
    ///
    /// The range `offset..offset + T::RAW_BYTE_LEN` must be in bounds.
    pub unsafe fn read_at_unchecked<T: ReadScalar>(&self, offset: usize) -> T {
        T::read(self.bytes.get_unchecked(offset..offset + T::RAW_BYTE_LEN)).unwrap_unchecked()
    }

    fn check_in_bounds(&self, offset: usize) -> Result<(), ReadError> {
        self.bytes
            .get(..offset)
            .ok_or(ReadError::OutOfBounds)
            .map(|_| ())
    }

    //NOTE: this is definitely unsound, since FixedSized isn't private,
    // and we don't enforce all soundness requirements: for instance, you could
    // use this to create an enum with an invalid discriminant, which is UB.
    //
    // In practice I believe my *current* use is correct, as it is all in auto-generated
    // code, and I know the invariants, but this should be revisited.
    pub fn read_ref_at<T: FixedSize>(&self, offset: usize) -> Result<&'a T, ReadError> {
        assert_ne!(std::mem::size_of::<T>(), 0);
        assert_eq!(std::mem::align_of::<T>(), 1);
        self.bytes
            .get(offset..offset + T::RAW_BYTE_LEN)
            .ok_or(ReadError::OutOfBounds)?;

        unsafe { Ok(self.read_ref_unchecked(offset)) }
    }

    /// Interpret the bytes at `offset` as a reference to some type `T`.
    ///
    /// # Safety
    ///
    /// `T` must be a struct or scalar that has alignment of 1, a non-zero size,
    /// and no internal padding, and offset must point to a slice of bytes that
    /// has length >= `size_of::<T>()`.
    unsafe fn read_ref_unchecked<T: FixedSize>(&self, offset: usize) -> &'a T {
        let bytes = self.bytes.get_unchecked(offset..offset + T::RAW_BYTE_LEN);
        &*(bytes.as_ptr() as *const T)
    }

    //NOTE: unsound, see the note on read_ref_at
    pub fn read_array<T: FixedSize>(&self, range: Range<usize>) -> Result<&'a [T], ReadError> {
        assert_ne!(std::mem::size_of::<T>(), 0);
        assert_eq!(std::mem::align_of::<T>(), 1);
        let bytes = self
            .bytes
            .get(range.clone())
            .ok_or(ReadError::OutOfBounds)?;
        if bytes.len() % std::mem::size_of::<T>() != 0 {
            return Err(ReadError::InvalidArrayLen);
        };
        unsafe { Ok(self.read_array_unchecked(range)) }
    }

    /// Interpret the bytes at `offset` as a reference to some type `T`.
    ///
    /// # Safety
    ///
    /// `T` must be a struct or scalar that has alignment of 1, a non-zero size,
    /// and no internal padding, and `range` must have a length that is non-zero
    /// and is a multiple of `size_of::<T>()`.
    pub unsafe fn read_array_unchecked<T: FixedSize>(&self, range: Range<usize>) -> &'a [T] {
        let bytes = self.bytes.get_unchecked(range);
        let elems = bytes.len() / std::mem::size_of::<T>();
        std::slice::from_raw_parts(bytes.as_ptr() as *const _, elems)
    }

    //pub fn resolve_offset<T: FontRead<'a>, O: Offset>(&self, off: O) -> Result<T, ReadError> {
    //let off = off.non_null().ok_or(ReadError::NullOffset)?;
    //self.split_off(off)
    //.ok_or(ReadError::OutOfBounds)
    //.and_then(|data| T::read(data))
    //}

    pub(crate) fn cursor(&self) -> Cursor<'a> {
        Cursor {
            pos: 0,
            data: *self,
        }
    }

    pub(crate) fn as_bytes(&self) -> &'a [u8] {
        self.bytes
    }
}

impl<'a> Cursor<'a> {
    pub(crate) fn advance<T: ReadScalar>(&mut self) {
        self.pos += T::RAW_BYTE_LEN
    }

    pub(crate) fn advance_by(&mut self, n_bytes: usize) {
        self.pos += n_bytes;
    }

    pub(crate) fn read<T: ReadScalar>(&mut self) -> Result<T, ReadError> {
        let temp = self.data.read_at(self.pos);
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

    pub(crate) fn read_array<T: FixedSize>(&mut self, n_elem: usize) -> Result<&'a [T], ReadError> {
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
