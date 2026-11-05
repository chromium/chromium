//! Width-independent access to individual-glyph pair positioning.

use super::*;

#[derive(Clone)]
pub(super) enum PairSetArray<'a> {
    Narrow(ArrayOfOffsets<'a, PairSet<'a>>),
    Wide(ArrayOfOffsets<'a, PairSet2<'a>, Offset24>),
}

impl<'a> PairSetArray<'a> {
    pub(super) fn get(&self, index: usize) -> Result<PairSetData<'a>, ReadError> {
        match self {
            Self::Narrow(sets) => sets.get(index).map(|set| PairSetData::from(&set)),
            Self::Wide(sets) => sets.get(index).map(|set| PairSetData::from(&set)),
        }
    }
}

pub(super) struct PairSetData<'a> {
    data: FontData<'a>,
    count: u32,
    wide: bool,
    complete: bool,
}

macro_rules! pair_set_data {
    ($table:ident, $wide:literal) => {
        impl<'a> From<&$table<'a>> for PairSetData<'a> {
            fn from(table: &$table<'a>) -> Self {
                Self {
                    data: table.offset_data(),
                    count: u32::from(table.pair_value_count()),
                    wide: $wide,
                    complete: !table.min_table_bytes().is_empty(),
                }
            }
        }
    };
}
pair_set_data!(PairSet, false);
pair_set_data!(PairSet2, true);

impl<'a> PairSetData<'a> {
    pub(super) fn pair_value_count(&self) -> u32 {
        self.count
    }

    pub(super) fn offset_data(&self) -> FontData<'a> {
        self.data
    }

    pub(super) fn width(&self) -> usize {
        if self.wide {
            3
        } else {
            2
        }
    }

    pub(super) fn second_glyph(&self, offset: usize) -> Result<GlyphId, ReadError> {
        if self.wide {
            self.data.read_at::<GlyphId24>(offset).map(GlyphId::from)
        } else {
            self.data.read_at::<u16>(offset).map(GlyphId::from)
        }
    }

    pub(super) fn complete(&self) -> bool {
        self.complete
    }

    pub(super) fn embed_glyph(
        &self,
        s: &mut Serializer,
        glyph: GlyphId,
    ) -> Result<(), SerializeErrorFlags> {
        if self.wide {
            let glyph = GlyphId24::checked_new(glyph.to_u32())
                .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
            s.embed(glyph).map(|_| ())
        } else {
            let glyph = u16::try_from(glyph.to_u32())
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
            s.embed(glyph).map(|_| ())
        }
    }

    pub(super) fn assign_count(
        &self,
        s: &mut Serializer,
        pos: usize,
        count: usize,
    ) -> Result<(), SerializeErrorFlags> {
        if self.wide {
            let count = Uint24::try_from(count)
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
            s.copy_assign(pos, count);
        } else {
            let count = u16::try_from(count)
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
            s.copy_assign(pos, count);
        }
        Ok(())
    }
}
