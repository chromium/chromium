//! The [loca (Index to Location)][loca] table
//!
//! [loca]: https://docs.microsoft.com/en-us/typography/opentype/spec/loca

use crate::{
    read::{FontRead, ReadArgs},
    table_provider::TopLevelTable,
    FontData, ReadError,
};
use types::{BigEndian, GlyphId, Tag};

/// The [loca] table.
///
/// [loca]: https://docs.microsoft.com/en-us/typography/opentype/spec/loca
#[derive(Clone)]
pub enum Loca<'a> {
    Short(&'a [BigEndian<u16>]),
    Long(&'a [BigEndian<u32>]),
}

impl TopLevelTable for Loca<'_> {
    const TAG: Tag = Tag::new(b"loca");
}

impl<'a> Loca<'a> {
    pub fn read(data: FontData<'a>, is_long: bool) -> Result<Self, ReadError> {
        Self::read_with_args(data, is_long)
    }

    pub fn len(&self) -> usize {
        match self {
            Loca::Short(data) => data.len().saturating_sub(1),
            Loca::Long(data) => data.len().saturating_sub(1),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    pub fn all_offsets_are_ascending(&self) -> bool {
        match self {
            Loca::Short(data) => !data
                .iter()
                .zip(data.iter().skip(1))
                .any(|(start, end)| start > end),
            Loca::Long(data) => !data
                .iter()
                .zip(data.iter().skip(1))
                .any(|(start, end)| start > end),
        }
    }

    /// Attempt to return the offset for a given glyph id.
    pub fn get_raw(&self, idx: usize) -> Option<u32> {
        match self {
            Loca::Short(data) => data.get(idx).map(|x| x.get() as u32 * 2),
            Loca::Long(data) => data.get(idx).map(|x| x.get()),
        }
    }

    /// What this table says about a glyph, or `None` if it says nothing
    /// readable.
    pub fn get(&self, gid: GlyphId, glyf: &super::glyf::Glyf<'a>) -> Option<LocaGlyph<'a>> {
        let idx = gid.to_u32() as usize;
        let start = self.get_raw(idx)?;
        let end = self.get_raw(idx + 1)?;
        if start == end {
            return Some(LocaGlyph::Empty);
        }
        let data = glyf.offset_data().slice(start as usize..end as usize)?;
        super::glyf::Glyph::read(data).ok().map(LocaGlyph::Glyph)
    }

    /// The outline for a glyph, `Ok(None)` where the glyph is empty, and an
    /// error where this table says nothing readable about it.
    ///
    /// Prefer [`get`][Self::get], which names the empty glyph instead of
    /// spelling it as an inner `None`. This reports every unreadable glyph as
    /// [`ReadError::OutOfBounds`], whatever the reason.
    #[doc(hidden)]
    pub fn get_glyf(
        &self,
        gid: GlyphId,
        glyf: &super::glyf::Glyf<'a>,
    ) -> Result<Option<super::glyf::Glyph<'a>>, ReadError> {
        self.get(gid, glyf)
            .map(LocaGlyph::into_glyph)
            .ok_or(ReadError::OutOfBounds)
    }
}

/// What a glyph's entry in the `loca` table leads to.
///
/// A glyph with no contours — a space, say — is described by an empty range,
/// and finding one is a success. It is separate from the glyph being
/// unreadable, which [`Loca::get`] reports by answering `None`.
#[derive(Clone)]
pub enum LocaGlyph<'a> {
    /// The glyph has no outline.
    Empty,
    /// The glyph's outline.
    Glyph(super::glyf::Glyph<'a>),
}

impl<'a> LocaGlyph<'a> {
    /// The outline, or `None` where the glyph is empty.
    pub fn glyph(&self) -> Option<&super::glyf::Glyph<'a>> {
        match self {
            Self::Empty => None,
            Self::Glyph(glyph) => Some(glyph),
        }
    }

    /// The outline by value, or `None` where the glyph is empty.
    pub fn into_glyph(self) -> Option<super::glyf::Glyph<'a>> {
        match self {
            Self::Empty => None,
            Self::Glyph(glyph) => Some(glyph),
        }
    }
}

impl ReadArgs for Loca<'_> {
    type Args = bool;
}

impl<'a> FontRead<'a> for Loca<'a> {
    fn read_with_args(data: FontData<'a>, args: Self::Args) -> Result<Self, ReadError> {
        let is_long = args;
        if is_long {
            data.read_array(0..data.len()).map(Loca::Long)
        } else {
            data.read_array(0..data.len()).map(Loca::Short)
        }
    }
}

#[cfg(test)]
mod tests {
    use font_test_data::bebuffer::BeBuffer;
    use types::Scalar;

    use super::Loca;

    fn to_loca_bytes<T: Scalar + Copy>(values: &[T]) -> (BeBuffer, bool) {
        let value_num_bytes = std::mem::size_of::<T>();
        let is_long = if value_num_bytes == 2 {
            false
        } else if value_num_bytes == 4 {
            true
        } else {
            panic!("invalid integer type must be u32 or u16")
        };
        let mut buffer = BeBuffer::default();

        for v in values {
            buffer = buffer.push(*v);
        }

        (buffer, is_long)
    }

    fn check_loca_sorting(values: &[u16], is_sorted: bool) {
        let (bytes, is_long) = to_loca_bytes(values);
        let loca = Loca::read(bytes.data().into(), is_long).unwrap();
        assert_eq!(loca.all_offsets_are_ascending(), is_sorted);

        let u32_values: Vec<u32> = values.iter().map(|v| *v as u32).collect();
        let (bytes, is_long) = to_loca_bytes(&u32_values);
        let loca = Loca::read(bytes.data().into(), is_long).unwrap();
        assert_eq!(loca.all_offsets_are_ascending(), is_sorted);
    }

    #[test]
    fn all_offsets_are_ascending() {
        // Sorted
        let empty: &[u16] = &[];
        check_loca_sorting(empty, true);
        check_loca_sorting(&[0], true);
        check_loca_sorting(&[0, 0], true);
        check_loca_sorting(&[0, 1], true);
        check_loca_sorting(&[1, 2, 2, 3, 7], true);

        // Unsorted
        check_loca_sorting(&[1, 0], false);
        check_loca_sorting(&[1, 3, 2], false);
        check_loca_sorting(&[2, 1, 3], false);
        check_loca_sorting(&[1, 2, 3, 2, 7], false);
    }
}
