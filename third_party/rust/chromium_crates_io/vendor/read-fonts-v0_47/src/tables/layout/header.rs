//! Header-selected lookup lists and offset arrays of more than one width.

use super::*;

/// An offset array whose serialized offsets have one of the layout widths.
#[derive(Clone)]
pub enum LayoutOffsetArray<'a, T: FontRead<'a, Args = ()>> {
    /// 16-bit offsets.
    Offset16(ArrayOfOffsets<'a, T, Offset16>),
    /// 24-bit offsets.
    Offset24(ArrayOfOffsets<'a, T, Offset24>),
    /// 32-bit offsets.
    Offset32(ArrayOfOffsets<'a, T, Offset32>),
}

impl<'a, T: FontRead<'a, Args = ()> + 'a> LayoutOffsetArray<'a, T> {
    /// Returns the number of offsets.
    pub fn len(&self) -> usize {
        match self {
            Self::Offset16(array) => array.len(),
            Self::Offset24(array) => array.len(),
            Self::Offset32(array) => array.len(),
        }
    }

    /// Returns `true` if there are no offsets.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Resolves the offset at `index`.
    pub fn get(&self, index: usize) -> Result<T, ReadError> {
        match self {
            Self::Offset16(array) => array.get(index),
            Self::Offset24(array) => array.get(index),
            Self::Offset32(array) => array.get(index),
        }
    }

    /// Iterates over the resolved offsets.
    pub fn iter(&self) -> impl Iterator<Item = Result<T, ReadError>> + 'a {
        let (one, two, three) = match self {
            Self::Offset16(array) => (Some(array.iter()), None, None),
            Self::Offset24(array) => (None, Some(array.iter()), None),
            Self::Offset32(array) => (None, None, Some(array.iter())),
        };
        one.into_iter()
            .flatten()
            .chain(two.into_iter().flatten())
            .chain(three.into_iter().flatten())
    }

    /// Iterates over the resolved offsets, with null offsets as `None`.
    pub fn iter_as_nullable(&self) -> impl Iterator<Item = Option<Result<T, ReadError>>> + 'a {
        self.iter().map(|result| match result {
            Err(ReadError::NullOffset) => None,
            other => Some(other),
        })
    }
}

/// The lookup list of a GSUB or GPOS table: the original LookupList, or
/// the LookupList2 of a version 1.2 header.
#[derive(Clone)]
pub enum SelectedLookupList<'a, T: FontRead<'a, Args = ()>> {
    /// A LookupList, with 16-bit offsets.
    Original(LookupList<'a, T>),
    /// A LookupList2, with 32-bit offsets.
    Extended(LookupList2<'a, T>),
}

impl<'a, T: FontRead<'a, Args = ()>> From<LookupList<'a, T>> for SelectedLookupList<'a, T> {
    fn from(list: LookupList<'a, T>) -> Self {
        Self::Original(list)
    }
}

impl<'a, T: FontRead<'a, Args = ()>> From<LookupList2<'a, T>> for SelectedLookupList<'a, T> {
    fn from(list: LookupList2<'a, T>) -> Self {
        Self::Extended(list)
    }
}

impl<'a, T: FontRead<'a, Args = ()> + 'a> SelectedLookupList<'a, T> {
    /// Returns the number of lookups.
    pub fn lookup_count(&self) -> u16 {
        match self {
            Self::Original(table) => table.lookup_count(),
            Self::Extended(table) => table.lookup_count(),
        }
    }

    /// Returns the offsets to the lookups.
    pub fn lookups(&self) -> LayoutOffsetArray<'a, T> {
        match self {
            Self::Original(table) => LayoutOffsetArray::Offset16(table.lookups()),
            Self::Extended(table) => LayoutOffsetArray::Offset32(table.lookups()),
        }
    }

    /// Returns the data the lookup offsets are relative to.
    pub fn offset_data(&self) -> FontData<'a> {
        match self {
            Self::Original(table) => table.offset_data(),
            Self::Extended(table) => table.offset_data(),
        }
    }
}

/// The offset a header field resolves through, for headers that carry both
/// an original 16-bit offset and an extended 32-bit one.
///
/// The extended offset applies when the header version has it and it is
/// nonzero; otherwise the original one does.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum HeaderOffset {
    Original(Offset32),
    Extended(Offset32),
}

impl HeaderOffset {
    /// Chooses between the two offsets of a header field. An extended header
    /// too short to hold its extended offset is an error.
    pub(crate) fn select(
        original: Nullable<Offset16>,
        extended: Option<Nullable<Offset32>>,
        has_extended: bool,
    ) -> Result<Self, ReadError> {
        if has_extended {
            let extended = *extended.ok_or(ReadError::OutOfBounds)?.offset();
            if !extended.is_null() {
                return Ok(Self::Extended(extended));
            }
        }
        Ok(Self::Original(Offset32::new(original.offset().to_u32())))
    }

    pub(crate) fn offset(self) -> Offset32 {
        match self {
            Self::Original(offset) | Self::Extended(offset) => offset,
        }
    }

    pub(crate) fn is_null(self) -> bool {
        self.offset().is_null()
    }

    /// Resolves the offset, or returns `None` if it is null.
    pub(crate) fn resolve<'a, T: FontRead<'a, Args = ()>>(
        self,
        data: FontData<'a>,
    ) -> Option<Result<T, ReadError>> {
        (!self.is_null()).then(|| self.offset().resolve(data))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn original(offset: u16) -> Nullable<Offset16> {
        Scalar::from_raw(offset.to_be_bytes())
    }

    fn extended(offset: u32) -> Option<Nullable<Offset32>> {
        Some(Scalar::from_raw(offset.to_be_bytes()))
    }

    #[test]
    fn header_offset_prefers_nonzero_extended_offset() {
        assert_eq!(
            HeaderOffset::select(original(10), extended(70000), true),
            Ok(HeaderOffset::Extended(Offset32::new(70000)))
        );
    }

    #[test]
    fn header_offset_falls_back_to_original_offset() {
        // A zero extended offset defers to the original one.
        assert_eq!(
            HeaderOffset::select(original(10), extended(0), true),
            Ok(HeaderOffset::Original(Offset32::new(10)))
        );
        // A header version without the extended field ignores it.
        assert_eq!(
            HeaderOffset::select(original(10), extended(70000), false),
            Ok(HeaderOffset::Original(Offset32::new(10)))
        );
        assert!(HeaderOffset::select(original(0), extended(0), true)
            .unwrap()
            .is_null());
    }

    #[test]
    fn header_offset_requires_extended_field_in_extended_header() {
        assert_eq!(
            HeaderOffset::select(original(10), None, true),
            Err(ReadError::OutOfBounds)
        );
        assert_eq!(
            HeaderOffset::select(original(10), None, false),
            Ok(HeaderOffset::Original(Offset32::new(10)))
        );
    }
}
