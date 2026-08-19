//! raw font bytes

#![deny(clippy::arithmetic_side_effects)]
use std::ops::{Range, RangeBounds};

use bytemuck::AnyBitPattern;
use types::{BigEndian, FixedSize, Scalar};

use crate::array::ComputedArray;
use crate::read::{ComputeSize, FontRead, ReadArgs, ReadError};

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

// we reuse a single buffer for all tables, but it gets padded with a u16
// to accurately represent format-1 tables
const ARR_LEN: usize = FontData::NULL_POOL_SIZE + u16::RAW_BYTE_LEN;

/// This is [0, 1] ('1' in u16be) followed by NULL_POOL_SIZE zeros.
///
/// - this same array is reused both for format-1 tables (which need a leading 1)
///   as well as all other tables, which don't.
static EMPTY_TABLE_BYTES: [u8; ARR_LEN] = {
    let mut arr = [0u8; ARR_LEN];
    arr[1] = 1;
    arr
};

impl FontData<'static> {
    // https://github.com/harfbuzz/harfbuzz/blob/aba63bb5/src/hb-null.hh#L40
    /// The number of bytes required to represent the largest table we have.
    ///
    /// This is checked by an assert at compile time, and can be increased as needed.
    const NULL_POOL_SIZE: usize = 262;

    // this is only used in const eval contexts, which are not visible to the
    // dead_code lint https://github.com/rust-lang/rust/issues/101532
    #[allow(dead_code)]
    /// Return `true` if our default data can represent a table `n_bytes` long
    pub(crate) const fn default_data_long_enough(n_bytes: usize) -> bool {
        n_bytes <= Self::NULL_POOL_SIZE
    }

    /// Return all zeroes suitable for the default impl of a table.
    pub(crate) fn default_table_data() -> Self {
        FontData::new(&EMPTY_TABLE_BYTES[2..])
    }

    /// Return a [0x0, 0x01] byte pair (u16be) and then all zeros, to represent
    /// the default impl of a format 1 table with u16 format.
    pub(crate) fn default_format_1_u16_table_data() -> Self {
        FontData::new(&EMPTY_TABLE_BYTES)
    }

    /// Return a single 0x01 and then all zeros, to represent the default impl
    /// of a format 1 table with u8 format.
    pub(crate) fn default_format_1_u8_table_data() -> Self {
        FontData::new(&EMPTY_TABLE_BYTES[1..])
    }
}

impl<'a> FontData<'a> {
    /// Empty data, useful for some tests and examples
    pub const EMPTY: FontData<'static> = FontData { bytes: &[] };

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
        let end = offset
            .checked_add(T::RAW_BYTE_LEN)
            .ok_or(ReadError::OutOfBounds)?;
        self.bytes
            .get(offset..end)
            .and_then(T::read)
            .ok_or(ReadError::OutOfBounds)
    }

    /// Read a big-endian value at the provided location in the data.
    pub fn read_be_at<T: Scalar>(&self, offset: usize) -> Result<BigEndian<T>, ReadError> {
        let end = offset
            .checked_add(T::RAW_BYTE_LEN)
            .ok_or(ReadError::OutOfBounds)?;
        self.bytes
            .get(offset..end)
            .and_then(BigEndian::from_slice)
            .ok_or(ReadError::OutOfBounds)
    }

    pub fn read_with_args<T>(&self, range: Range<usize>, args: T::Args) -> Result<T, ReadError>
    where
        T: FontRead<'a>,
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
    pub fn read_ref_at<T: AnyBitPattern + FixedSize>(
        &self,
        offset: usize,
    ) -> Result<&'a T, ReadError> {
        let end = offset
            .checked_add(T::RAW_BYTE_LEN)
            .ok_or(ReadError::OutOfBounds)?;
        self.bytes
            .get(offset..end)
            .ok_or(ReadError::OutOfBounds)
            .map(bytemuck::from_bytes)
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
    pub fn read_array<T: AnyBitPattern + FixedSize>(
        &self,
        range: Range<usize>,
    ) -> Result<&'a [T], ReadError> {
        let bytes = self
            .bytes
            .get(range.clone())
            .ok_or(ReadError::OutOfBounds)?;
        if bytes
            .len()
            .checked_rem(std::mem::size_of::<T>())
            .unwrap_or(1) // definitely != 0
            != 0
        {
            return Err(ReadError::InvalidArrayLen);
        };
        Ok(bytemuck::cast_slice(bytes))
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
        self.pos = self.pos.saturating_add(T::RAW_BYTE_LEN);
    }

    pub(crate) fn advance_by(&mut self, n_bytes: usize) {
        self.pos = self.pos.saturating_add(n_bytes);
    }

    /// Read a variable length u32 and advance the cursor
    pub(crate) fn read_u32_var(&mut self) -> Result<u32, ReadError> {
        let mut next = || self.read::<u8>().map(|v| v as u32);
        let b0 = next()?;
        // TODO this feels possible to simplify, e.g. compute length, loop taking one and shifting and or'ing
        #[allow(clippy::arithmetic_side_effects)] // these are all checked
        let result = match b0 {
            _ if b0 < 0x80 => b0,
            _ if b0 < 0xC0 => ((b0 - 0x80) << 8) | next()?,
            _ if b0 < 0xE0 => ((b0 - 0xC0) << 16) | (next()? << 8) | next()?,
            _ if b0 < 0xF0 => ((b0 - 0xE0) << 24) | (next()? << 16) | (next()? << 8) | next()?,
            _ => {
                // 0xF0 is a dedicated 5-byte prefix; high bits are carried entirely
                // by the following 4 bytes.
                (next()? << 24) | (next()? << 16) | (next()? << 8) | next()?
            }
        };

        Ok(result)
    }

    /// Read a scalar and advance the cursor.
    pub(crate) fn read<T: Scalar>(&mut self) -> Result<T, ReadError> {
        let temp = self.data.read_at(self.pos);
        self.advance::<T>();
        temp
    }

    /// Read a big-endian value and advance the cursor.
    pub(crate) fn read_be<T: Scalar>(&mut self) -> Result<BigEndian<T>, ReadError> {
        let temp = self.data.read_be_at(self.pos);
        self.advance::<T>();
        temp
    }

    pub(crate) fn read_with_args<T>(&mut self, args: T::Args) -> Result<T, ReadError>
    where
        T: FontRead<'a> + ComputeSize,
    {
        let len = T::compute_size(args)?;
        let range_end = self.pos.checked_add(len).ok_or(ReadError::OutOfBounds)?;
        let temp = self.data.read_with_args(self.pos..range_end, args);
        self.advance_by(len);
        temp
    }

    // only used in records that contain arrays :/
    pub(crate) fn read_computed_array<T>(
        &mut self,
        len: usize,
        args: T::Args,
    ) -> Result<ComputedArray<'a, T>, ReadError>
    where
        T: FontRead<'a> + ComputeSize,
    {
        let len = len
            .checked_mul(T::compute_size(args)?)
            .ok_or(ReadError::OutOfBounds)?;
        let range_end = self.pos.checked_add(len).ok_or(ReadError::OutOfBounds)?;
        let temp = self.data.read_with_args(self.pos..range_end, args);
        self.advance_by(len);
        temp
    }

    pub(crate) fn read_array<T: AnyBitPattern + FixedSize>(
        &mut self,
        n_elem: usize,
    ) -> Result<&'a [T], ReadError> {
        let len = n_elem
            .checked_mul(T::RAW_BYTE_LEN)
            .ok_or(ReadError::OutOfBounds)?;
        let end = self.pos.checked_add(len).ok_or(ReadError::OutOfBounds)?;
        let temp = self.data.read_array(self.pos..end);
        self.advance_by(len);
        temp
    }

    /// return the current position, or an error if we are out of bounds
    pub(crate) fn position(&self) -> Result<usize, ReadError> {
        self.data.check_in_bounds(self.pos).map(|_| self.pos)
    }

    // used when handling fields with an implicit length, which must be at the
    // end of a table.
    pub(crate) fn remaining_bytes(&self) -> usize {
        self.data.len().saturating_sub(self.pos)
    }

    pub(crate) fn remaining(self) -> Option<FontData<'a>> {
        self.data.split_off(self.pos)
    }

    pub fn is_empty(&self) -> bool {
        self.pos >= self.data.len()
    }
}

// useful so we can have offsets that are just to data
impl ReadArgs for FontData<'_> {
    type Args = ();
}

impl<'a> FontRead<'a> for FontData<'a> {
    fn read_with_args(data: FontData<'a>, _: ()) -> Result<Self, ReadError> {
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

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn how_does_big_endian_work_again() {
        let data = FontData::default_format_1_u16_table_data();
        assert_eq!(data.read_at(0), Ok(1u16));

        assert_eq!(
            FontData::default_format_1_u8_table_data().read_at(0),
            Ok(1u8)
        );
    }
}
