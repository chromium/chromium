//! The [mort (Glyph Metamorphosis)](https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6mort.html) table.

use super::aat::{safe_read_array_to_end, LegacyStateTableParts, LookupU16, NoPayload, StateTable};

include!("../../generated/generated_mort.rs");

impl VarSize for Chain<'_> {
    type Size = u32;

    fn read_len_at(data: FontData, pos: usize) -> Option<usize> {
        data.read_at::<u32>(pos.checked_add(u32::RAW_BYTE_LEN)?)
            .ok()
            .map(|size| size as usize)
    }
}

impl VarSize for Subtable<'_> {
    type Size = u16;

    fn read_len_at(data: FontData, pos: usize) -> Option<usize> {
        data.read_at::<u16>(pos).ok().map(usize::from)
    }
}

impl<'a> Subtable<'a> {
    /// If true, this subtable will process glyphs in logical order.
    #[inline]
    pub fn is_logical(&self) -> bool {
        self.coverage() & 0x1000 != 0
    }

    /// If true, this subtable applies to horizontal and vertical text.
    #[inline]
    pub fn is_all_directions(&self) -> bool {
        self.coverage() & 0x2000 != 0
    }

    /// If true, this subtable processes glyphs in descending order.
    #[inline]
    pub fn is_backwards(&self) -> bool {
        self.coverage() & 0x4000 != 0
    }

    /// If true, this subtable applies only to vertical text.
    #[inline]
    pub fn is_vertical(&self) -> bool {
        self.coverage() & 0x8000 != 0
    }

    /// Returns the format-specific subtable data.
    pub fn kind(&self) -> Result<SubtableKind<'a>, ReadError> {
        SubtableKind::read_with_args(FontData::new(self.data()), self.coverage())
    }
}

/// The various `mort` subtable formats.
#[derive(Clone)]
pub enum SubtableKind<'a> {
    Rearrangement(StateTable<'a>),
    Contextual(ContextualSubtable<'a>),
    Ligature(LigatureSubtable<'a>),
    NonContextual(LookupU16<'a>),
    Insertion(InsertionSubtable<'a>),
}

impl ReadArgs for SubtableKind<'_> {
    type Args = u16;
}

impl<'a> FontRead<'a> for SubtableKind<'a> {
    fn read_with_args(data: FontData<'a>, coverage: Self::Args) -> Result<Self, ReadError> {
        match coverage & 0xFF {
            0 => Ok(Self::Rearrangement(StateTable::read(data)?)),
            1 => Ok(Self::Contextual(ContextualSubtable::read(data)?)),
            2 => Ok(Self::Ligature(LigatureSubtable::read(data)?)),
            4 => Ok(Self::NonContextual(LookupU16::read(data)?)),
            5 => Ok(Self::Insertion(InsertionSubtable::read(data)?)),
            format => Err(ReadError::InvalidFormat(format as _)),
        }
    }
}

/// Pre-resolved, lifetime-free description of a `mort` subtable's layout.
#[derive(Clone, Copy, Debug, Default)]
pub struct SubtableParts {
    /// Low byte of coverage: the subtable format (0/1/2/4/5).
    pub format: u8,
    pub state: LegacyStateTableParts,
    /// Format-dependent offsets following the state-table header.
    pub extra: [u16; 3],
}

impl SubtableKind<'_> {
    /// Captures the offsets needed to rebuild this subtable kind from the same data.
    pub fn parts(data: FontData, coverage: u16) -> Result<SubtableParts, ReadError> {
        let format = (coverage & 0xFF) as u8;
        let mut parts = SubtableParts {
            format,
            ..Default::default()
        };
        if format == 4 {
            return Ok(parts);
        }
        parts.state = LegacyStateTableParts::read(data)?;
        let mut cursor = data.cursor();
        cursor.advance_by(StateTable::<NoPayload>::HEADER_LEN);
        match format {
            1 | 5 => parts.extra[0] = cursor.read::<u16>()?,
            2 => {
                parts.extra[0] = cursor.read::<u16>()?;
                parts.extra[1] = cursor.read::<u16>()?;
                parts.extra[2] = cursor.read::<u16>()?;
            }
            _ => {}
        }
        Ok(parts)
    }

    /// Rebuilds a subtable kind from data and previously captured offsets.
    #[inline]
    pub fn from_parts<'a>(
        data: FontData<'a>,
        parts: &SubtableParts,
    ) -> Result<SubtableKind<'a>, ReadError> {
        match parts.format {
            0 => Ok(SubtableKind::Rearrangement(StateTable::from_parts(
                data,
                &parts.state,
            )?)),
            1 => Ok(SubtableKind::Contextual(ContextualSubtable {
                state_table: StateTable::from_parts(data, &parts.state)?,
                data,
            })),
            2 => Ok(SubtableKind::Ligature(LigatureSubtable {
                state_table: StateTable::from_parts(data, &parts.state)?,
                data,
            })),
            4 => Ok(SubtableKind::NonContextual(LookupU16::read(data)?)),
            5 => Ok(SubtableKind::Insertion(InsertionSubtable {
                state_table: StateTable::from_parts(data, &parts.state)?,
                glyphs: safe_read_array_to_end(&data, parts.extra[0] as usize)?,
            })),
            format => Err(ReadError::InvalidFormat(format as _)),
        }
    }
}

/// Contextual glyph substitution subtable.
#[derive(Clone)]
pub struct ContextualSubtable<'a> {
    pub state_table: StateTable<'a, ContextualEntryData>,
    data: FontData<'a>,
}

impl ContextualSubtable<'_> {
    /// Resolves a legacy signed word offset for the specified glyph.
    pub fn substitution(&self, offset: i16, glyph: GlyphId16) -> Result<GlyphId16, ReadError> {
        let word = i32::from(offset)
            .checked_add(i32::from(glyph.to_u16()))
            .ok_or(ReadError::OutOfBounds)?;
        let byte = usize::try_from(word)
            .ok()
            .and_then(|word| word.checked_mul(u16::RAW_BYTE_LEN))
            .ok_or(ReadError::OutOfBounds)?;
        self.data.read_at(byte)
    }
}

impl ReadArgs for ContextualSubtable<'_> {
    type Args = ();
}

impl<'a> FontRead<'a> for ContextualSubtable<'a> {
    fn read_with_args(data: FontData<'a>, _: ()) -> Result<Self, ReadError> {
        let state_table = StateTable::<ContextualEntryData>::read_with_args(data, ())?;
        let mut cursor = data.cursor();
        cursor.advance_by(StateTable::<NoPayload>::HEADER_LEN);
        cursor.read::<u16>()?;
        Ok(Self { state_table, data })
    }
}

/// Ligature glyph substitution subtable.
#[derive(Clone)]
pub struct LigatureSubtable<'a> {
    pub state_table: StateTable<'a>,
    data: FontData<'a>,
}

impl LigatureSubtable<'_> {
    /// Reads an action at an absolute byte offset from the subtable start.
    pub fn ligature_action(&self, offset: usize) -> Result<u32, ReadError> {
        self.data.read_at(offset)
    }

    /// Reads a component at an absolute word offset from the subtable start.
    pub fn component(&self, offset: i32) -> Result<u16, ReadError> {
        let byte = usize::try_from(offset)
            .ok()
            .and_then(|offset| offset.checked_mul(u16::RAW_BYTE_LEN))
            .ok_or(ReadError::OutOfBounds)?;
        self.data.read_at(byte)
    }

    /// Reads a ligature glyph at an absolute byte offset from the subtable start.
    pub fn ligature(&self, offset: usize) -> Result<GlyphId16, ReadError> {
        self.data.read_at(offset)
    }
}

impl ReadArgs for LigatureSubtable<'_> {
    type Args = ();
}

impl<'a> FontRead<'a> for LigatureSubtable<'a> {
    fn read_with_args(data: FontData<'a>, _: ()) -> Result<Self, ReadError> {
        let state_table = StateTable::read(data)?;
        let mut cursor = data.cursor();
        cursor.advance_by(StateTable::<NoPayload>::HEADER_LEN);
        cursor.read::<u16>()?;
        cursor.read::<u16>()?;
        cursor.read::<u16>()?;
        Ok(Self { state_table, data })
    }
}

/// Insertion glyph substitution subtable.
#[derive(Clone)]
pub struct InsertionSubtable<'a> {
    pub state_table: StateTable<'a, InsertionEntryData>,
    pub glyphs: &'a [BigEndian<GlyphId16>],
}

impl ReadArgs for InsertionSubtable<'_> {
    type Args = ();
}

impl<'a> FontRead<'a> for InsertionSubtable<'a> {
    fn read_with_args(data: FontData<'a>, _: ()) -> Result<Self, ReadError> {
        let state_table = StateTable::<InsertionEntryData>::read_with_args(data, ())?;
        let mut cursor = data.cursor();
        cursor.advance_by(StateTable::<NoPayload>::HEADER_LEN);
        let glyphs_offset = cursor.read::<u16>()? as usize;
        let glyphs = safe_read_array_to_end(&data, glyphs_offset)?;
        Ok(Self {
            state_table,
            glyphs,
        })
    }
}

#[cfg(feature = "experimental_traverse")]
impl<'a> SomeRecord<'a> for Chain<'a> {
    fn traverse(self, data: FontData<'a>) -> RecordResolver<'a> {
        RecordResolver {
            name: "Chain",
            get_field: Box::new(move |idx, _data| match idx {
                0usize => Some(Field::new("default_flags", self.default_flags())),
                _ => None,
            }),
            data,
        }
    }
}

#[cfg(feature = "experimental_traverse")]
impl<'a> SomeRecord<'a> for Subtable<'a> {
    fn traverse(self, data: FontData<'a>) -> RecordResolver<'a> {
        RecordResolver {
            name: "Subtable",
            get_field: Box::new(move |idx, _data| match idx {
                0usize => Some(Field::new("coverage", self.coverage())),
                1usize => Some(Field::new("sub_feature_flags", self.sub_feature_flags())),
                _ => None,
            }),
            data,
        }
    }
}
