//! Reads and applies font patches to font binaries.
//!
//! Font patch formats are defined as part of the incremental font transfer
//! specification: <https://w3c.github.io/IFT/Overview.html#font-patch-formats>
//!
//! Two main types of font patches are implemented:
//! 1. Table Keyed Patch - these patches contain a per table brotli binary patch
//!    to be applied to the input font.
//! 2. Glyph Keyed - these patches contain blobs of data associated with
//!    combinations of glyph id + table. The patch inserts these blobs into the
//!    table at the location for the corresponding glyph id.

use std::collections::HashMap;

use crate::patch_group::PatchInfo;

use crate::glyph_keyed::apply_glyph_keyed_patches;

use crate::table_keyed::apply_table_keyed_patch;
use font_types::Tag;
use read_fonts::tables::ift::{CompatibilityId, GlyphKeyedPatch, TableKeyedPatch};
use skera::serialize::SerializeErrorFlags;

use read_fonts::{FontData, FontRead, FontRef, ReadError};

use shared_brotli_patch_decoder::decode_error::DecodeError;
use shared_brotli_patch_decoder::SharedBrotliDecoder;

/// A trait for types to which an incremental font transfer patch can be
/// applied.
///
/// See: <https://w3c.github.io/IFT/Overview.html#font-patch-formats> for details on the format of patches.
pub trait IncrementalFontPatchBase {
    /// Apply a table keyed incremental font patches (<https://w3c.github.io/IFT/Overview.html#font-patch-formats>)
    ///
    /// Applies the patches to this base.
    ///
    /// Returns the byte data for the new font produced as a result of the patch
    /// applications.
    fn apply_table_keyed_patch<D: SharedBrotliDecoder>(
        &self,
        patch: &PatchInfo,
        patch_data: &[u8],
        brotli_decoder: &D,
    ) -> Result<Vec<u8>, PatchingError>;

    /// Apply a set of glyph keyed incremental font patches (<https://w3c.github.io/IFT/Overview.html#font-patch-formats>)
    ///
    /// Applies the patches to this base.
    ///
    /// Returns the byte data for the new font produced as a result of the patch
    /// applications.
    fn apply_glyph_keyed_patches<'a, D: SharedBrotliDecoder>(
        &self,
        patches: impl Iterator<Item = (&'a PatchInfo, &'a [u8])>,
        brotli_decoder: &D,
    ) -> Result<Vec<u8>, PatchingError>;
}

/// An error that occurs while trying to apply an IFT patch to a font file.
#[derive(Debug, Clone, PartialEq)]
pub enum PatchingError {
    PatchParsingFailed(ReadError),
    FontParsingFailed(ReadError),
    SerializationError(SerializeErrorFlags),
    IncompatiblePatch,
    NonIncrementalFont,
    InvalidPatch(&'static str),
    EmptyPatchList,
    InternalError,
    MissingPatches,
}

impl From<SerializeErrorFlags> for PatchingError {
    fn from(err: SerializeErrorFlags) -> Self {
        PatchingError::SerializationError(err)
    }
}

impl From<ReadError> for PatchingError {
    fn from(err: ReadError) -> Self {
        PatchingError::FontParsingFailed(err)
    }
}

impl From<DecodeError> for PatchingError {
    fn from(decoding_error: DecodeError) -> Self {
        match decoding_error {
            DecodeError::InitFailure => {
                PatchingError::InvalidPatch("Failure to init brotli encoder.")
            }
            DecodeError::InvalidStream => PatchingError::InvalidPatch("Malformed brotli stream."),
            DecodeError::InvalidDictionary => PatchingError::InvalidPatch("Malformed dictionary."),
            DecodeError::MaxSizeExceeded => PatchingError::InvalidPatch("Max size exceeded."),
            DecodeError::ExcessInputData => {
                PatchingError::InvalidPatch("Input brotli stream has excess bytes.")
            }
            DecodeError::IoError(_) => {
                PatchingError::InvalidPatch("IO error decoding input brotli stream.")
            }
        }
    }
}

impl std::fmt::Display for PatchingError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            PatchingError::PatchParsingFailed(err) => {
                write!(f, "Failed to parse patch file: {}", err)
            }
            PatchingError::FontParsingFailed(err) => {
                write!(f, "Failed to parse font file: {}", err)
            }
            PatchingError::SerializationError(err) => {
                write!(f, "serialization failure constructing patched table: {err}")
            }
            PatchingError::IncompatiblePatch => {
                write!(f, "Compatibility ID of the patch does not match the font.")
            }

            PatchingError::NonIncrementalFont => {
                write!(f, "Can't patch font as it's not an incremental transfer font.")
            }
            PatchingError::InvalidPatch(msg) => write!(f, "Invalid patch file: '{msg}'"),
            PatchingError::EmptyPatchList => write!(f, "At least one patch file must be provided."),
            PatchingError::InternalError => {
                write!(f, "Internal constraint violated, typically should not happen.")
            }
            PatchingError::MissingPatches => write!(f, "Not all patch data has been supplied."),
        }
    }
}

impl std::error::Error for PatchingError {}

impl IncrementalFontPatchBase for FontRef<'_> {
    fn apply_table_keyed_patch<D: SharedBrotliDecoder>(
        &self,
        patch: &PatchInfo,
        patch_data: &[u8],
        brotli_decoder: &D,
    ) -> Result<Vec<u8>, PatchingError> {
        let font_compat_id =
            patch.tag().font_compat_id(self).map_err(PatchingError::FontParsingFailed)?;
        if font_compat_id != *patch.tag().expected_compat_id() {
            return Err(PatchingError::IncompatiblePatch);
        }

        let patch = TableKeyedPatch::read(FontData::new(patch_data))
            .map_err(PatchingError::PatchParsingFailed)?;

        if patch.compatibility_id() != font_compat_id {
            return Err(PatchingError::IncompatiblePatch);
        }

        apply_table_keyed_patch(&patch, self, brotli_decoder)
    }

    fn apply_glyph_keyed_patches<'a, D: SharedBrotliDecoder>(
        &self,
        patches: impl Iterator<Item = (&'a PatchInfo, &'a [u8])>,
        brotli_decoder: &D,
    ) -> Result<Vec<u8>, PatchingError> {
        let mut cached_compat_ids: HashMap<Tag, Result<CompatibilityId, PatchingError>> =
            Default::default();

        let mut raw_patches: Vec<(&PatchInfo, GlyphKeyedPatch<'_>)> = vec![];
        for (patch_info, patch_data) in patches {
            let tag = patch_info.tag();
            let font_compat_id = cached_compat_ids
                .entry(tag.tag())
                .or_insert_with(|| {
                    tag.font_compat_id(self).map_err(PatchingError::FontParsingFailed)
                })
                .as_ref()
                .map_err(Clone::clone)?;
            if font_compat_id != tag.expected_compat_id() {
                return Err(PatchingError::IncompatiblePatch);
            }

            let patch = GlyphKeyedPatch::read(FontData::new(patch_data))
                .map_err(PatchingError::PatchParsingFailed)?;

            if *font_compat_id != patch.compatibility_id() {
                return Err(PatchingError::IncompatiblePatch);
            }

            raw_patches.push((patch_info, patch));
        }

        apply_glyph_keyed_patches(&raw_patches, self, brotli_decoder)
    }
}

impl IncrementalFontPatchBase for &[u8] {
    fn apply_table_keyed_patch<D: SharedBrotliDecoder>(
        &self,
        patch: &PatchInfo,
        patch_data: &[u8],
        brotli_decoder: &D,
    ) -> Result<Vec<u8>, PatchingError> {
        FontRef::new(self).map_err(PatchingError::FontParsingFailed)?.apply_table_keyed_patch(
            patch,
            patch_data,
            brotli_decoder,
        )
    }

    fn apply_glyph_keyed_patches<'a, D: SharedBrotliDecoder>(
        &self,
        patches: impl Iterator<Item = (&'a PatchInfo, &'a [u8])>,
        brotli_decoder: &D,
    ) -> Result<Vec<u8>, PatchingError> {
        FontRef::new(self)
            .map_err(PatchingError::FontParsingFailed)?
            .apply_glyph_keyed_patches(patches, brotli_decoder)
    }
}

#[cfg(test)]
mod tests {

    use std::collections::HashMap;

    use font_test_data::ift::{
        codepoints_only_format2, glyf_u16_glyph_patches, glyph_keyed_patch_header,
        table_keyed_patch,
    };
    use read_fonts::{
        collections::IntSet,
        tables::ift::{CompatibilityId, IFTX_TAG, IFT_TAG},
    };
    use shared_brotli_patch_decoder::BuiltInBrotliDecoder;

    use crate::{
        font_patch::PatchingError,
        glyph_keyed::tests::assemble_glyph_keyed_patch,
        patchmap::{IftTableTag, PatchId, PatchUrl},
        testdata::test_font_for_patching_with_loca_mod,
    };

    use super::{IncrementalFontPatchBase, PatchInfo};

    // Testing only exceptional situations here, actual applications are tested by
    // "patch_group.rs".

    #[test]
    fn table_keyed_patch_and_font_compat_id_mismatch() {
        let info = PatchInfo {
            url: PatchUrl::expand_template(
                &[8, b'f', b'o', b'o', b'.', b'b', b'a', b'r', b'/', 128],
                &PatchId::Numeric(0),
            )
            .unwrap(),
            source_table: IftTableTag::Ift(CompatibilityId::from_u32s([1, 2, 3, 4])),
            application_flag_bit_indices: IntSet::<u32>::empty(),
        };

        let ift_table = codepoints_only_format2();
        let mut iftx_table = codepoints_only_format2();
        iftx_table.write_at("compat_id[0]", 2u32);

        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([(IFT_TAG, ift_table.as_slice()), (IFTX_TAG, iftx_table.as_slice())]),
        );

        let mut patch = table_keyed_patch();
        patch.write_at("compat_id", 2);
        assert_eq!(
            font.as_slice().apply_table_keyed_patch(&info, &patch, &BuiltInBrotliDecoder),
            Err(PatchingError::IncompatiblePatch)
        );
    }

    #[test]
    fn table_keyed_patch_info_and_font_compat_id_mismatch() {
        let info = PatchInfo {
            url: PatchUrl::expand_template(
                &[8, b'f', b'o', b'o', b'.', b'b', b'a', b'r', b'/', 128],
                &PatchId::Numeric(0),
            )
            .unwrap(),
            source_table: IftTableTag::Ift(CompatibilityId::from_u32s([2, 2, 3, 4])),
            application_flag_bit_indices: IntSet::<u32>::empty(),
        };

        let ift_table = codepoints_only_format2();
        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([(IFT_TAG, ift_table.as_slice())]),
        );

        let patch = table_keyed_patch();
        assert_eq!(
            font.as_slice().apply_table_keyed_patch(&info, &patch, &BuiltInBrotliDecoder),
            Err(PatchingError::IncompatiblePatch)
        );
    }

    #[test]
    fn glyph_keyed_patch_and_font_compat_id_mismatch() {
        let info = PatchInfo {
            url: PatchUrl::expand_template(
                &[8, b'f', b'o', b'o', b'.', b'b', b'a', b'r', b'/', 128],
                &PatchId::Numeric(0),
            )
            .unwrap(),
            source_table: IftTableTag::Ift(CompatibilityId::from_u32s([1, 2, 3, 4])),
            application_flag_bit_indices: IntSet::<u32>::empty(),
        };

        let ift_table = codepoints_only_format2();
        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([(IFT_TAG, ift_table.as_slice())]),
        );

        let patch =
            assemble_glyph_keyed_patch(glyph_keyed_patch_header(), glyf_u16_glyph_patches());

        let input = vec![(&info, patch.as_slice())];
        assert_eq!(
            font.as_slice().apply_glyph_keyed_patches(input.into_iter(), &BuiltInBrotliDecoder),
            Err(PatchingError::IncompatiblePatch)
        );
    }

    #[test]
    fn glyph_keyed_patch_info_and_font_compat_id_mismatch() {
        let info = PatchInfo {
            url: PatchUrl::expand_template(
                &[8, b'f', b'o', b'o', b'.', b'b', b'a', b'r', b'/', 128],
                &PatchId::Numeric(0),
            )
            .unwrap(),
            source_table: IftTableTag::Ift(CompatibilityId::from_u32s([6, 7, 9, 9])),
            application_flag_bit_indices: IntSet::<u32>::empty(),
        };

        let mut ift_table = codepoints_only_format2();
        ift_table.write_at("compat_id[0]", 6u32);
        ift_table.write_at("compat_id[1]", 7u32);
        ift_table.write_at("compat_id[2]", 8u32);
        ift_table.write_at("compat_id[3]", 9u32);

        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([(IFT_TAG, ift_table.as_slice())]),
        );

        let patch =
            assemble_glyph_keyed_patch(glyph_keyed_patch_header(), glyf_u16_glyph_patches());

        let input = vec![(&info, patch.as_slice())];
        assert_eq!(
            font.as_slice().apply_glyph_keyed_patches(input.into_iter(), &BuiltInBrotliDecoder),
            Err(PatchingError::IncompatiblePatch)
        );
    }
}
