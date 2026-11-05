//! the [GDEF] table
//!
//! [GDEF]: https://docs.microsoft.com/en-us/typography/opentype/spec/gdef

pub use super::layout::{ClassDef, CoverageTable, DeviceOrVariationIndex};

use super::layout::header::HeaderOffset;
use super::variations::ItemVariationStore;

#[cfg(test)]
#[path = "../tests/test_gdef.rs"]
mod tests;

use crate::types::Compatible;

include!("../../generated/generated_gdef.rs");

impl<'a> Gdef<'a> {
    fn select_offset(
        &self,
        original: Nullable<Offset16>,
        extended: Option<Nullable<Offset32>>,
    ) -> Result<HeaderOffset, ReadError> {
        HeaderOffset::select(original, extended, self.version().compatible((1, 4)))
    }

    fn resolve_selected<T: FontRead<'a, Args = ()>>(
        &self,
        original: Nullable<Offset16>,
        extended: Option<Nullable<Offset32>>,
    ) -> Option<Result<T, ReadError>> {
        match self.select_offset(original, extended) {
            Ok(offset) => offset.resolve(self.offset_data()),
            Err(error) => Some(Err(error)),
        }
    }

    /// Resolves the table, preferring its 32-bit offset when nonzero.
    pub fn glyph_class_def(&self) -> Option<Result<ClassDef<'a>, ReadError>> {
        self.resolve_selected(
            self.glyph_class_def_offset(),
            self.glyph_class_def2_offset(),
        )
    }

    /// Resolves only the original 16-bit offset.
    pub fn original_glyph_class_def(&self) -> Option<Result<ClassDef<'a>, ReadError>> {
        self.glyph_class_def_offset().resolve(self.offset_data())
    }

    /// Resolves the table, preferring its 32-bit offset when nonzero.
    pub fn attach_list(&self) -> Option<Result<AttachList<'a>, ReadError>> {
        self.resolve_selected(self.attach_list_offset(), self.attach_list2_offset())
    }

    /// Resolves only the original 16-bit offset.
    pub fn original_attach_list(&self) -> Option<Result<AttachList<'a>, ReadError>> {
        self.attach_list_offset().resolve(self.offset_data())
    }

    /// Resolves the table, preferring its 32-bit offset when nonzero.
    pub fn mark_attach_class_def(&self) -> Option<Result<ClassDef<'a>, ReadError>> {
        self.resolve_selected(
            self.mark_attach_class_def_offset(),
            self.mark_attach_class_def2_offset(),
        )
    }

    /// Resolves only the original 16-bit offset.
    pub fn original_mark_attach_class_def(&self) -> Option<Result<ClassDef<'a>, ReadError>> {
        self.mark_attach_class_def_offset()
            .resolve(self.offset_data())
    }

    /// Resolves the table, preferring its 32-bit offset when nonzero.
    pub fn mark_glyph_sets_def(&self) -> Option<Result<MarkGlyphSets<'a>, ReadError>> {
        self.resolve_selected(
            self.mark_glyph_sets_def_offset().unwrap_or_default(),
            self.mark_glyph_sets_def2_offset(),
        )
    }

    /// Resolves only the original 16-bit offset.
    pub fn original_mark_glyph_sets_def(&self) -> Option<Result<MarkGlyphSets<'a>, ReadError>> {
        self.mark_glyph_sets_def_offset()
            .unwrap_or_default()
            .resolve(self.offset_data())
    }
}

/// LigCaretList or LigCaretList2, selected by the GDEF header.
#[derive(Clone)]
pub enum SelectedLigCaretList<'a> {
    /// A LigCaretList, with 16-bit offsets.
    Original(LigCaretList<'a>),
    /// A LigCaretList2, with 24-bit offsets.
    Extended(LigCaretList2<'a>),
}

impl<'a> SelectedLigCaretList<'a> {
    /// Returns the coverage of the ligature glyphs.
    pub fn coverage(&self) -> Result<CoverageTable<'a>, ReadError> {
        match self {
            Self::Original(t) => t.coverage(),
            Self::Extended(t) => t.coverage(),
        }
    }

    /// Returns the number of ligature glyphs.
    pub fn lig_glyph_count(&self) -> u32 {
        match self {
            Self::Original(t) => u32::from(t.lig_glyph_count()),
            Self::Extended(t) => t.lig_glyph_count().to_u32(),
        }
    }

    /// Returns the caret tables of the ligature glyphs, in coverage order.
    pub fn lig_glyphs(&self) -> super::layout::LayoutOffsetArray<'a, LigGlyph<'a>> {
        match self {
            Self::Original(t) => super::layout::LayoutOffsetArray::Offset16(t.lig_glyphs()),
            Self::Extended(t) => super::layout::LayoutOffsetArray::Offset24(t.lig_glyphs()),
        }
    }
}

impl<'a> Gdef<'a> {
    /// Resolves the ligature caret list, preferring LigCaretList2 when its
    /// offset is nonzero.
    pub fn lig_caret_list(&self) -> Option<Result<SelectedLigCaretList<'a>, ReadError>> {
        let offset =
            match self.select_offset(self.lig_caret_list_offset(), self.lig_caret_list2_offset()) {
                Ok(offset) => offset,
                Err(error) => return Some(Err(error)),
            };
        if offset.is_null() {
            return None;
        }
        let data = self.offset_data();
        Some(match offset {
            HeaderOffset::Original(offset) => {
                offset.resolve(data).map(SelectedLigCaretList::Original)
            }
            HeaderOffset::Extended(offset) => {
                offset.resolve(data).map(SelectedLigCaretList::Extended)
            }
        })
    }

    /// Resolves the original 16-bit offset without applying precedence.
    pub fn original_lig_caret_list(&self) -> Option<Result<LigCaretList<'a>, ReadError>> {
        self.lig_caret_list_offset().resolve(self.offset_data())
    }
}
