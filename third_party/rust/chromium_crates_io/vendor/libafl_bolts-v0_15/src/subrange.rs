//! Subrange of things.
//! Convenient wrappers to handle sub-slices efficiently.

use core::{
    cmp::min,
    ops::{Bound, Range, RangeBounds},
};

use crate::{
    HasLen,
    ownedref::{OwnedMutSlice, OwnedSlice},
};

/// An immutable contiguous subslice of a byte slice.
/// It is mostly useful to cheaply wrap a subslice of a given input.
///
/// A mutable version is available: [`SubRangeMutSlice`].
#[derive(Debug)]
pub struct SubRangeSlice<'a, T> {
    /// The (complete) parent input we will work on
    parent_slice: OwnedSlice<'a, T>,
    /// The range inside the parent input we will work on
    range: Range<usize>,
}

/// A mutable contiguous subslice of a byte slice.
/// It is mostly useful to cheaply wrap a subslice of a given input.
///
/// An immutable version is available: [`SubRangeSlice`].
#[derive(Debug)]
pub struct SubRangeMutSlice<'a, T> {
    /// The (complete) parent input we will work on
    parent_slice: OwnedMutSlice<'a, T>,
    /// The range inside the parent input we will work on
    range: Range<usize>,
}

/// Slice wrapper keeping track of the current read position.
/// Convenient wrapper when the slice must be split in multiple sub-slices and read sequentially.
#[derive(Debug)]
pub struct SliceReader<'a, T> {
    parent_slice: &'a [T],
    pos: usize,
}

impl<'a, T> SliceReader<'a, T> {
    /// Create a new [`SliceReader`].
    /// The position of the reader is initialized to 0.
    #[must_use]
    pub fn new(parent_slice: &'a [T]) -> Self {
        Self {
            parent_slice,
            pos: 0,
        }
    }

    /// Read an immutable sub-slice from the parent slice, from the current cursor position up to `limit` elements.
    /// If the resulting slice would go beyond the end of the parent slice, it will be truncated to the length of the parent slice.
    /// This function does not provide any feedback on whether the slice was cropped or not.
    #[must_use]
    pub fn next_sub_slice_truncated(&mut self, limit: usize) -> SubRangeSlice<'a, T> {
        let sub_slice = SubRangeSlice::with_slice(self.parent_slice, self.pos..(self.pos + limit));

        self.pos += sub_slice.len();

        sub_slice
    }

    /// Read an immutable sub-slice from the parent slice, from the current cursor position up to `limit` bytes.
    /// If the resulting slice would go beyond the end of the parent slice, it will be limited to the length of the parent slice.
    /// The function returns
    /// - `Ok(Slice)` if the returned slice has `limit` elements.
    /// - `Err(Partial(slice))` if the returned slice has strictly less than `limit` elements and is not empty.
    /// - `Err(Empty)` if the reader was already at the end or `limit` equals zero.
    pub fn next_sub_input(
        &mut self,
        limit: usize,
    ) -> Result<SubRangeSlice<'a, T>, PartialSubRangeSlice<'a, T>> {
        let slice_to_return = self.next_sub_slice_truncated(limit);

        let real_len = slice_to_return.len();

        if real_len == 0 {
            Err(PartialSubRangeSlice::Empty)
        } else if real_len < limit {
            Err(PartialSubRangeSlice::Partial(slice_to_return))
        } else {
            Ok(slice_to_return)
        }
    }
}

impl<'a, T> From<&'a [T]> for SliceReader<'a, T> {
    fn from(input: &'a [T]) -> Self {
        Self::new(input)
    }
}

impl<T> HasLen for SubRangeSlice<'_, T> {
    #[inline]
    fn len(&self) -> usize {
        self.range.len()
    }
}

impl<T> HasLen for SubRangeMutSlice<'_, T> {
    #[inline]
    fn len(&self) -> usize {
        self.range.len()
    }
}

/// Gets the relevant concrete start index from [`RangeBounds`] (inclusive)
pub fn start_index<R>(range: &R) -> usize
where
    R: RangeBounds<usize>,
{
    match range.start_bound() {
        Bound::Unbounded => 0,
        Bound::Included(start) => *start,
        Bound::Excluded(start) => start + 1,
    }
}

/// Gets the relevant concrete end index from [`RangeBounds`] (exclusive)
pub fn end_index<R>(range: &R, max_len: usize) -> usize
where
    R: RangeBounds<usize>,
{
    let end = match range.end_bound() {
        Bound::Unbounded => max_len,
        Bound::Included(end) => end + 1,
        Bound::Excluded(end) => *end,
    };

    min(end, max_len)
}

/// Gets the relevant subrange of a [`Range<usize>`] from [`RangeBounds`].
pub fn sub_range<R>(outer_range: &Range<usize>, inner_range: R) -> (Bound<usize>, Bound<usize>)
where
    R: RangeBounds<usize>,
{
    let start =
        match (outer_range.start_bound(), inner_range.start_bound()) {
            (Bound::Unbounded, Bound::Unbounded) => Bound::Unbounded,
            (Bound::Excluded(bound), Bound::Unbounded)
            | (Bound::Unbounded, Bound::Excluded(bound)) => Bound::Excluded(*bound),
            (Bound::Included(bound), Bound::Unbounded)
            | (Bound::Unbounded, Bound::Included(bound)) => Bound::Included(*bound),
            (Bound::Included(own), Bound::Included(other)) => Bound::Included(own + other),
            (Bound::Included(own), Bound::Excluded(other))
            | (Bound::Excluded(own), Bound::Included(other)) => Bound::Excluded(own + other),
            (Bound::Excluded(own), Bound::Excluded(other)) => Bound::Excluded(own + other + 1),
        };

    let end = match (outer_range.end_bound(), inner_range.end_bound()) {
        (Bound::Unbounded, Bound::Unbounded) => Bound::Unbounded,
        (Bound::Excluded(bound), Bound::Unbounded) => Bound::Excluded(*bound),
        (Bound::Unbounded, Bound::Excluded(bound)) => Bound::Excluded(outer_range.end - *bound),
        (Bound::Included(bound), Bound::Unbounded) => Bound::Included(*bound),
        (Bound::Unbounded, Bound::Included(bound)) => Bound::Included(outer_range.end - *bound),
        (Bound::Included(own), Bound::Included(other)) => {
            Bound::Included(min(*own, outer_range.start + other))
        }
        (Bound::Included(own), Bound::Excluded(other)) => {
            Bound::Included(min(*own, outer_range.start + other - 1))
        }
        (Bound::Excluded(own), Bound::Included(other)) => {
            Bound::Included(min(*own - 1, outer_range.start + other))
        }
        (Bound::Excluded(own), Bound::Excluded(other)) => {
            Bound::Excluded(min(*own, outer_range.start + other))
        }
    };

    (start, end)
}

/// Representation of a partial slice
/// This is used when providing a slice smaller than the expected one.
/// It notably happens when trying to read the end of an input.
#[derive(Debug)]
pub enum PartialSubRangeSlice<'a, T> {
    /// The slice is empty, and thus not kept
    Empty,
    /// The slice is strictly smaller than the expected one.
    Partial(SubRangeSlice<'a, T>),
}

impl<'a, T> PartialSubRangeSlice<'a, T> {
    /// Consumes `PartialBytesSubInput` and returns true if it was empty, false otherwise.
    #[must_use]
    pub fn empty(self) -> bool {
        matches!(self, PartialSubRangeSlice::Empty)
    }

    /// Consumes `PartialBytesSubInput` and returns the partial slice if it was a partial slice, None otherwise.
    #[must_use]
    pub fn partial(self) -> Option<SubRangeSlice<'a, T>> {
        #[expect(clippy::match_wildcard_for_single_variants)]
        match self {
            PartialSubRangeSlice::Partial(partial_slice) => Some(partial_slice),
            _ => None,
        }
    }
}

impl<'a, T> SubRangeSlice<'a, T> {
    /// Creates a new [`SubRangeSlice`], a sub-slice representation of a byte array.
    pub fn new<R>(parent_slice: OwnedSlice<'a, T>, range: R) -> Self
    where
        R: RangeBounds<usize>,
    {
        let parent_len = parent_slice.len();

        SubRangeSlice {
            parent_slice,
            range: Range {
                start: start_index(&range),
                end: end_index(&range, parent_len),
            },
        }
    }

    /// Get the sub slice as bytes.
    #[must_use]
    pub fn as_slice(&self) -> &[T] {
        &self.parent_slice[self.range.clone()]
    }

    /// Creates a new [`SubRangeSlice`] that's a sliced view on a bytes slice.
    pub fn with_slice<R>(parent_slice: &'a [T], range: R) -> Self
    where
        R: RangeBounds<usize>,
    {
        Self::new(parent_slice.into(), range)
    }

    /// The parent input
    #[must_use]
    pub fn parent_slice(self) -> OwnedSlice<'a, T> {
        self.parent_slice
    }

    /// The inclusive start index in the parent buffer
    #[must_use]
    pub fn start_index(&self) -> usize {
        self.range.start
    }

    /// The exclusive end index in the parent buffer
    #[must_use]
    pub fn end_index(&self) -> usize {
        self.range.end
    }

    /// Creates a sub range in the current own range
    pub fn sub_range<R>(&self, range: R) -> (Bound<usize>, Bound<usize>)
    where
        R: RangeBounds<usize>,
    {
        sub_range(&self.range, range)
    }
}

impl<'a, T> SubRangeMutSlice<'a, T> {
    /// Creates a new [`SubRangeMutSlice`], a sub-slice representation of a byte array.
    pub fn new<R>(parent_slice: OwnedMutSlice<'a, T>, range: R) -> Self
    where
        R: RangeBounds<usize>,
    {
        let parent_len = parent_slice.len();

        SubRangeMutSlice {
            parent_slice,
            range: Range {
                start: start_index(&range),
                end: end_index(&range, parent_len),
            },
        }
    }

    /// Get the sub slice as bytes.
    #[must_use]
    pub fn as_slice(&self) -> &[T] {
        &self.parent_slice[self.range.clone()]
    }

    /// Get the sub slice as bytes.
    #[must_use]
    pub fn as_slice_mut(&mut self) -> &mut [T] {
        &mut self.parent_slice[self.range.clone()]
    }

    /// Creates a new [`SubRangeMutSlice`] that's a view on a bytes slice.
    /// The sub-slice can then be used to mutate parts of the original bytes.
    pub fn with_slice<R>(parent_slice: &'a mut [T], range: R) -> Self
    where
        R: RangeBounds<usize>,
    {
        Self::new(parent_slice.into(), range)
    }

    /// The parent input
    #[must_use]
    pub fn parent_slice(self) -> OwnedMutSlice<'a, T> {
        self.parent_slice
    }

    /// The inclusive start index in the parent buffer
    #[must_use]
    pub fn start_index(&self) -> usize {
        self.range.start
    }

    /// The exclusive end index in the parent buffer
    #[must_use]
    pub fn end_index(&self) -> usize {
        self.range.end
    }

    /// Creates a sub range in the current own range
    pub fn sub_range<R>(&self, range: R) -> (Bound<usize>, Bound<usize>)
    where
        R: RangeBounds<usize>,
    {
        sub_range(&self.range, range)
    }
}

#[cfg(test)]
mod tests {
    use super::SliceReader;

    #[test]
    fn test_bytesreader_toslice_unchecked() {
        let bytes_input = vec![1, 2, 3, 4, 5, 6, 7];
        let mut bytes_reader = SliceReader::new(&bytes_input);

        let bytes_read = bytes_reader.next_sub_slice_truncated(2);
        assert_eq!(*bytes_read.as_slice(), [1, 2]);

        let bytes_read = bytes_reader.next_sub_slice_truncated(3);
        assert_eq!(*bytes_read.as_slice(), [3, 4, 5]);

        let bytes_read = bytes_reader.next_sub_slice_truncated(8);
        assert_eq!(*bytes_read.as_slice(), [6, 7]);

        let bytes_read = bytes_reader.next_sub_slice_truncated(8);
        let bytes_read_ref: &[u8] = &[];
        assert_eq!(bytes_read.as_slice(), bytes_read_ref);
    }

    #[test]
    fn test_bytesreader_toslice() {
        let bytes_input = vec![1, 2, 3, 4, 5, 6, 7];
        let mut bytes_reader = SliceReader::new(&bytes_input);

        let bytes_read = bytes_reader.next_sub_input(2);
        assert_eq!(*bytes_read.unwrap().as_slice(), [1, 2]);

        let bytes_read = bytes_reader.next_sub_input(3);
        assert_eq!(*bytes_read.unwrap().as_slice(), [3, 4, 5]);

        let bytes_read = bytes_reader.next_sub_input(8);
        assert_eq!(
            *bytes_read.unwrap_err().partial().unwrap().as_slice(),
            [6, 7]
        );

        let bytes_read = bytes_reader.next_sub_input(8);
        assert!(bytes_read.unwrap_err().empty());
    }
}
