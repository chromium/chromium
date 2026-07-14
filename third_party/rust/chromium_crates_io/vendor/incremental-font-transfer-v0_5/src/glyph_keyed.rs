//! Implementation of Glyph Keyed patch application.
//!
//! Glyph Keyed patches are a type of incremental font patch which stores opaque
//! data blobs keyed by glyph id. Patch application places the data blobs into
//! the appropriate place in the base font based on the associated glyph id.
//!
//! Glyph Keyed patches are specified here:
//! <https://w3c.github.io/IFT/Overview.html#glyph-keyed>
use crate::patchmap::IftTableTag;
use crate::table_keyed::copy_unprocessed_tables;
use crate::{font_patch::PatchingError, patch_group::PatchInfo};

use font_types::{Scalar, Uint24};
use read_fonts::ps::cff::{index::Index, v1::Index as Index1, v2::Index as Index2};
use read_fonts::{
    collections::IntSet,
    tables::{
        cff::Cff,
        cff2::Cff2,
        glyf::Glyf,
        gvar::{Gvar, GvarFlags},
        ift::{GlyphKeyedPatch, GlyphPatches},
        ift::{IFTX_TAG, IFT_TAG},
        loca::Loca,
    },
    types::Tag,
    FontData, FontRead, FontRef, ReadError, TableProvider, TopLevelTable,
};

use shared_brotli_patch_decoder::SharedBrotliDecoder;
use skera::serialize::{OffsetWhence, SerializeErrorFlags, Serializer};
use skrifa::GlyphId;
use std::collections::{BTreeSet, HashMap};
use std::ops::{Range, RangeInclusive};

use write_fonts::FontBuilder;

pub(crate) fn apply_glyph_keyed_patches<D: SharedBrotliDecoder>(
    patches: &[(&PatchInfo, GlyphKeyedPatch<'_>)],
    font: &FontRef,
    brotli_decoder: &D,
) -> Result<Vec<u8>, PatchingError> {
    let mut decompression_buffer: Vec<Vec<u8>> = Vec::with_capacity(patches.len());

    for (_, patch) in patches {
        if patch.format() != Tag::new(b"ifgk") {
            return Err(PatchingError::InvalidPatch("Patch file tag is not 'ifgk'"));
        }

        decompression_buffer.push(
            brotli_decoder
                .decode(patch.brotli_stream(), None, patch.max_uncompressed_length() as usize)
                .map_err(PatchingError::from)?,
        );
    }

    let mut glyph_patches: Vec<GlyphPatches<'_>> = vec![];
    for (raw_data, patch) in decompression_buffer.iter().zip(patches) {
        glyph_patches.push(
            GlyphPatches::read(FontData::new(raw_data), patch.1.flags())
                .map_err(PatchingError::PatchParsingFailed)?,
        );
    }

    let num_glyphs = font.maxp().map_err(PatchingError::FontParsingFailed)?.num_glyphs();

    let max_glyph_id =
        GlyphId::new(num_glyphs.checked_sub(1).ok_or(PatchingError::FontParsingFailed(
            ReadError::MalformedData("Font has no glyphs."),
        ))? as u32);

    // IFT and IFTX tables will be modified and then copied below.
    let mut processed_tables = BTreeSet::from([IFT_TAG, IFTX_TAG]);
    let mut font_builder = FontBuilder::new();

    for table_tag in table_tag_list(&glyph_patches)? {
        if table_tag == Glyf::TAG {
            let (Some(glyf), Ok(loca)) = (font.table_data(Glyf::TAG), font.loca(None)) else {
                return Err(PatchingError::InvalidPatch(
                    "Trying to patch glyf/loca but base font doesn't have them.",
                ));
            };
            patch_offset_array(
                Glyf::TAG,
                &glyph_patches,
                GlyfAndLoca { loca, glyf: glyf.as_bytes() },
                max_glyph_id,
                &mut font_builder,
            )?;
            // glyf patch application also generates a loca table.
            processed_tables.insert(table_tag);
            processed_tables.insert(Loca::TAG);
        } else if table_tag == Gvar::TAG {
            let Ok(gvar) = font.gvar() else {
                return Err(PatchingError::InvalidPatch(
                    "Trying to patch gvar but base font doesn't have them.",
                ));
            };
            patch_offset_array(Gvar::TAG, &glyph_patches, gvar, max_glyph_id, &mut font_builder)?;
            processed_tables.insert(Gvar::TAG);
        } else if table_tag == Cff::TAG {
            let Some(charstrings_offset) =
                font.ift().ok().as_ref().and_then(|t| t.cff_charstrings_offset())
            else {
                return Err(PatchingError::InvalidPatch(
                    "Required CFF charstrings offset is missing from IFT table.",
                ));
            };
            patch_offset_array(
                table_tag,
                &glyph_patches,
                CFFAndCharStrings::from_cff_font(font, charstrings_offset, max_glyph_id)
                    .map_err(PatchingError::from)?,
                max_glyph_id,
                &mut font_builder,
            )?;
            processed_tables.insert(table_tag);
        } else if table_tag == Cff2::TAG {
            let Some(charstrings_offset) =
                font.ift().ok().as_ref().and_then(|t| t.cff2_charstrings_offset())
            else {
                return Err(PatchingError::InvalidPatch(
                    "Required CFF2 charstrings offset is missing from IFT table.",
                ));
            };
            patch_offset_array(
                table_tag,
                &glyph_patches,
                CFFAndCharStrings::from_cff2_font(font, charstrings_offset, max_glyph_id)
                    .map_err(PatchingError::from)?,
                max_glyph_id,
                &mut font_builder,
            )?;
            processed_tables.insert(table_tag);
        } else {
            // All other table tags are ignored.
            continue;
        }
    }

    // Mark patches applied in IFT and IFTX as needed, copy the modified tables into
    // the font builder.
    let mut new_itf_data = font.table_data(IFT_TAG).map(|data| data.as_bytes().to_vec());
    let mut new_itfx_data = font.table_data(IFTX_TAG).map(|data| data.as_bytes().to_vec());
    for (info, _) in patches {
        let data = match info.tag() {
            IftTableTag::Ift(_) => new_itf_data.as_mut().ok_or(PatchingError::InternalError)?,
            IftTableTag::Iftx(_) => new_itfx_data.as_mut().ok_or(PatchingError::InternalError)?,
        };

        for bit_index in info.application_flag_bit_indices() {
            let byte_index = (bit_index as usize) / 8;
            let bit_index = (bit_index as usize % 8) as u8;
            let byte = data.get_mut(byte_index).ok_or(PatchingError::InternalError)?;
            *byte |= 1 << bit_index;
        }
    }

    if let Some(data) = new_itf_data {
        font_builder.add_raw(IFT_TAG, data);
    }
    if let Some(data) = new_itfx_data {
        font_builder.add_raw(IFTX_TAG, data);
    }

    copy_unprocessed_tables(font, processed_tables, &mut font_builder);

    Ok(font_builder.build())
}

fn table_tag_list(glyph_patches: &[GlyphPatches]) -> Result<BTreeSet<Tag>, PatchingError> {
    for patches in glyph_patches {
        if patches
            .tables()
            .iter()
            .zip(patches.tables().iter().skip(1))
            .any(|(prev_tag, next_tag)| next_tag <= prev_tag)
        {
            return Err(PatchingError::InvalidPatch("Duplicate or unsorted table tag."));
        }
    }

    Ok(glyph_patches
        .iter()
        .flat_map(|patch| patch.tables())
        .map(|tag| tag.get())
        .collect::<BTreeSet<Tag>>())
}

fn dedup_gid_replacement_data<'a>(
    glyph_patches: impl Iterator<Item = &'a GlyphPatches<'a>>,
    table_tag: Tag,
) -> Result<(IntSet<GlyphId>, Vec<&'a [u8]>), ReadError> {
    // Since the specification allows us to freely choose patch application order
    // (for groups of glyph keyed patches, see: https://w3c.github.io/IFT/Overview.html#extend-font-subset) if two patches affect the same gid we can choose
    // one arbitrarily to remain applied. In this case we choose the first applied
    // patch for each gid be the one that takes priority.
    let mut gids: IntSet<GlyphId> = IntSet::default();
    let mut data_for_gid: HashMap<GlyphId, &'a [u8]> = HashMap::default();
    for glyph_patch in glyph_patches {
        let Some((table_index, _)) =
            glyph_patch.tables().iter().enumerate().find(|(_, tag)| tag.get() == table_tag)
        else {
            continue;
        };

        glyph_patch.glyph_data_for_table(table_index).try_for_each(|result| {
            let (gid, data) = result?;
            data_for_gid.entry(gid).or_insert(data);
            gids.insert(gid);
            Ok(())
        })?;
    }

    let mut deduped: Vec<&'a [u8]> = Vec::with_capacity(data_for_gid.len());
    gids.iter().for_each(|gid| {
        // in the above loop for each gid in gids there is always an  entry in
        // data_for_gid
        deduped.push(data_for_gid.get(&gid).unwrap());
    });

    Ok((gids, deduped))
}

fn retained_glyphs_in_font(
    replace_gids: &IntSet<GlyphId>,
    max_glyph_id: GlyphId,
) -> impl Iterator<Item = RangeInclusive<GlyphId>> + '_ {
    replace_gids.iter_excluded_ranges().filter_map(move |range| {
        // Filter out values beyond max_glyph_id.
        if *range.start() > max_glyph_id {
            return None;
        }
        if *range.end() > max_glyph_id {
            return Some(*range.start()..=max_glyph_id);
        }
        Some(range)
    })
}

fn retained_glyphs_total_size<T: GlyphDataOffsetArray>(
    gids: &IntSet<GlyphId>,
    offsets: &T,
    max_glyph_id: GlyphId,
) -> Result<usize, PatchingError> {
    let mut total_size = 0usize;
    for keep_range in retained_glyphs_in_font(gids, max_glyph_id) {
        let start = *keep_range.start();
        let end: GlyphId =
            keep_range.end().to_u32().checked_add(1).ok_or(PatchingError::InternalError)?.into();

        let start_offset = offsets.offset_for(start)?;
        let end_offset = offsets.offset_for(end)?;

        total_size += end_offset
            .checked_sub(start_offset) // TODO: this can be removed if we pre-verify ascending order
            .ok_or(PatchingError::FontParsingFailed(ReadError::MalformedData(
                "offset entries are not in ascending order",
            )))? as usize;
    }

    Ok(total_size)
}

struct OffsetArrayAndData {
    data: Vec<u8>,
    offset_array: Vec<u8>,
}

struct OffsetArrayBuilder<'a, T> {
    gids: &'a IntSet<GlyphId>,
    max_glyph_id: GlyphId,
    replacement_data: &'a [&'a [u8]],
    offset_array: &'a T,
    new_data_len: usize,
    new_offsets_len: usize,
}

impl<T: GlyphDataOffsetArray> OffsetArrayBuilder<'_, T> {
    fn build<OffsetInfo: OffsetTypeInfo, OffsetType: Scalar + TryFrom<usize>>(
        self,
    ) -> Result<OffsetArrayAndData, PatchingError> {
        if !self.offset_array.all_offsets_are_ascending() {
            return Err(PatchingError::FontParsingFailed(ReadError::MalformedData(
                "offset array contains unordered offsets.",
            )));
        }

        let mut new_data = Serializer::new(self.new_data_len);
        new_data.start_serialize().map_err(PatchingError::SerializationError)?;
        let mut new_offsets = Serializer::new(self.new_offsets_len);
        new_offsets.start_serialize().map_err(PatchingError::SerializationError)?;

        let divisor = OffsetInfo::DIVISOR;
        let bias = OffsetInfo::BIAS as usize;

        let mut replace_it = self.gids.iter_ranges().peekable();
        let mut keep_it = retained_glyphs_in_font(self.gids, self.max_glyph_id).peekable();
        let mut replacement_data_it = self.replacement_data.iter();
        let mut write_index = 0;

        loop {
            let (range, replace) = match (replace_it.peek(), keep_it.peek()) {
                (Some(replace), Some(keep)) => {
                    if replace.start() <= keep.start() {
                        (replace_it.next().unwrap(), true)
                    } else {
                        (keep_it.next().unwrap(), false)
                    }
                }
                (Some(_), None) => (replace_it.next().unwrap(), true),
                (None, Some(_)) => (keep_it.next().unwrap(), false),
                (None, None) => break,
            };

            let (start, end) = (range.start().to_u32(), range.end().to_u32());

            if replace {
                for _ in start..=end {
                    let data = *replacement_data_it.next().ok_or(PatchingError::InternalError)?;

                    new_data.embed_bytes(data).map_err(PatchingError::SerializationError)?;

                    let new_off: OffsetType = ((write_index / divisor) + bias)
                        .try_into()
                        .map_err(|_| PatchingError::InternalError)?;

                    new_offsets.embed(new_off).map_err(PatchingError::SerializationError)?;

                    write_index += data.len();
                    // Add padding if the offset gets divided
                    if divisor > 1 {
                        let padding = data.len() % divisor;
                        write_index += padding;
                        new_data.pad(padding).map_err(PatchingError::SerializationError)?;
                    }
                }
            } else {
                let start_off = self.offset_array.offset_for(start.into())? as usize;
                let end_off = self
                    .offset_array
                    .offset_for(end.checked_add(1).ok_or(PatchingError::InternalError)?.into())?
                    as usize;

                let len = end_off.checked_sub(start_off).ok_or(PatchingError::InternalError)?;
                new_data
                    .embed_bytes(self.offset_array.get(start_off..end_off)?)
                    .map_err(PatchingError::SerializationError)?;

                for gid in start..=end {
                    let cur_off = self.offset_array.offset_for(gid.into())? as usize;
                    let new_off = cur_off - start_off + write_index;

                    let new_off: OffsetType = ((new_off / divisor) + bias)
                        .try_into()
                        .map_err(|_| PatchingError::InternalError)?;
                    new_offsets.embed(new_off).map_err(PatchingError::SerializationError)?;
                }

                write_index += len;
            }
        }

        // Write the last offset
        let new_off: OffsetType = ((write_index / divisor) + bias)
            .try_into()
            .map_err(|_| PatchingError::InternalError)?;
        new_offsets.embed(new_off).map_err(PatchingError::SerializationError)?;

        new_data.end_serialize();
        new_offsets.end_serialize();

        Ok(OffsetArrayAndData {
            data: new_data.copy_bytes(),
            offset_array: new_offsets.copy_bytes(),
        })
    }
}

fn patch_offset_array<'a, T: GlyphDataOffsetArray>(
    table_tag: Tag,
    glyph_patches: &'a [GlyphPatches<'a>],
    offset_array: T,
    max_glyph_id: GlyphId,
    font_builder: &mut FontBuilder,
) -> Result<(), PatchingError> {
    // Step 0: merge the individual patches into a list of replacement data for gid.
    // TODO(garretrieger): special case where gids is empty, just returned umodified
    // copy of glyf + loca?
    let offset_type = offset_array.offset_type();
    let (gids, replacement_data) = dedup_gid_replacement_data(glyph_patches.iter(), table_tag)
        .map_err(PatchingError::PatchParsingFailed)?;

    // Step 1: determine the new total size of the data portion.
    let mut total_data_size = retained_glyphs_total_size(&gids, &offset_array, max_glyph_id)?;
    for data in replacement_data.iter() {
        let len = data.len();
        // note: include padding when needed (if offsets are divided for storage)
        total_data_size += len + (len % offset_type.offset_divisor());
    }

    // TODO(garretrieger): pre-check loca has all ascending offsets.

    // Check to see if the offset size needs to be upgraded
    let new_offset_type = if total_data_size > offset_type.max_representable_size() {
        offset_array
            .available_offset_types()
            .find(|candidate_type| candidate_type.max_representable_size() >= total_data_size)
            .ok_or(PatchingError::SerializationError(
                SerializeErrorFlags::SERIALIZE_ERROR_OFFSET_OVERFLOW,
            ))?
    } else {
        offset_type
    };

    if gids.last().unwrap_or(GlyphId::new(0)) > max_glyph_id {
        return Err(PatchingError::InvalidPatch(
            "Patch would add a glyph beyond this fonts maximum.",
        ));
    }

    // Step 2: patch together the new data array (by copying in ranges of data in
    // the correct order). Note: we synthesize using whatever new_offset_type
    // was selected above. Note: max_glyph_id + 2 here because we want num
    // glyphs + 1.
    let offsets_size = (max_glyph_id.to_u32() as usize + 2) * new_offset_type.offset_width();

    let offset_array_builder = OffsetArrayBuilder {
        gids: &gids,
        max_glyph_id,
        replacement_data: &replacement_data,
        offset_array: &offset_array,
        new_data_len: total_data_size as usize,
        new_offsets_len: offsets_size,
    };

    let new_offsets = match new_offset_type {
        OffsetType::CffOne(_) => offset_array_builder.build::<CffOneInfo, u8>()?,
        OffsetType::CffTwo(_) => offset_array_builder.build::<CffTwoInfo, u16>()?,
        OffsetType::CffThree(_) => offset_array_builder.build::<CffThreeInfo, Uint24>()?,
        OffsetType::CffFour(_) => offset_array_builder.build::<CffFourInfo, u32>()?,
        OffsetType::ShortDivByTwo(_) => offset_array_builder.build::<ShortDivByTwoInfo, u16>()?,
        OffsetType::Long(_) => offset_array_builder.build::<LongInfo, u32>()?,
    };

    // Step 3: add new tables to the output builder
    offset_array.add_to_font(font_builder, new_offsets, new_offset_type)?;

    Ok(())
}

/// Classifies the different style of offsets that can be used in a data offset
/// array.
#[derive(Clone, Copy, PartialEq, Eq)]
enum OffsetType {
    // CFF needs it's own offset types since CFF offsets have a 1 byte bias.
    CffOne(CffOneInfo),
    CffTwo(CffTwoInfo),
    CffThree(CffThreeInfo),
    CffFour(CffFourInfo),

    // For gvar, loca, glyf
    ShortDivByTwo(ShortDivByTwoInfo),
    Long(LongInfo),
}

trait OffsetTypeInfo {
    const WIDTH: usize;
    const DIVISOR: usize;
    const BIAS: u32;

    fn width(&self) -> usize {
        Self::WIDTH
    }

    fn divisor(&self) -> usize {
        Self::DIVISOR
    }

    fn bias(&self) -> u32 {
        Self::BIAS
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Default)]
struct CffOneInfo;

impl OffsetTypeInfo for CffOneInfo {
    const WIDTH: usize = 1;
    const DIVISOR: usize = 1;
    const BIAS: u32 = 1;
}

#[derive(Clone, Copy, PartialEq, Eq, Default)]
struct CffTwoInfo;

impl OffsetTypeInfo for CffTwoInfo {
    const WIDTH: usize = 2;
    const DIVISOR: usize = 1;
    const BIAS: u32 = 1;
}

#[derive(Clone, Copy, PartialEq, Eq, Default)]
struct CffThreeInfo;

impl OffsetTypeInfo for CffThreeInfo {
    const WIDTH: usize = 3;
    const DIVISOR: usize = 1;
    const BIAS: u32 = 1;
}

#[derive(Clone, Copy, PartialEq, Eq, Default)]
struct CffFourInfo;

impl OffsetTypeInfo for CffFourInfo {
    const WIDTH: usize = 4;
    const DIVISOR: usize = 1;
    const BIAS: u32 = 1;
}

#[derive(Clone, Copy, PartialEq, Eq, Default)]
struct ShortDivByTwoInfo;
impl OffsetTypeInfo for ShortDivByTwoInfo {
    const WIDTH: usize = 2;
    const DIVISOR: usize = 2;
    const BIAS: u32 = 0;
}

#[derive(Clone, Copy, PartialEq, Eq, Default)]
struct LongInfo;
impl OffsetTypeInfo for LongInfo {
    const WIDTH: usize = 4;
    const DIVISOR: usize = 1;
    const BIAS: u32 = 0;
}

impl OffsetType {
    fn max_representable_size(self) -> usize {
        match self {
            Self::ShortDivByTwo(_) => ((1 << 16) - 1) * 2,
            _ => ((1 << (self.offset_width() as u64 * 8)) - 1 - self.offset_bias() as u64) as usize,
        }
    }

    fn offset_width(&self) -> usize {
        match self {
            Self::CffOne(info) => info.width(),
            Self::CffTwo(info) => info.width(),
            Self::CffThree(info) => info.width(),
            Self::CffFour(info) => info.width(),
            Self::ShortDivByTwo(info) => info.width(),
            Self::Long(info) => info.width(),
        }
    }

    fn offset_divisor(&self) -> usize {
        match self {
            Self::CffOne(info) => info.divisor(),
            Self::CffTwo(info) => info.divisor(),
            Self::CffThree(info) => info.divisor(),
            Self::CffFour(info) => info.divisor(),
            Self::ShortDivByTwo(info) => info.divisor(),
            Self::Long(info) => info.divisor(),
        }
    }

    fn offset_bias(&self) -> u32 {
        match self {
            Self::CffOne(info) => info.bias(),
            Self::CffTwo(info) => info.bias(),
            Self::CffThree(info) => info.bias(),
            Self::CffFour(info) => info.bias(),
            Self::ShortDivByTwo(info) => info.bias(),
            Self::Long(info) => info.bias(),
        }
    }
}

struct GlyfAndLoca<'a> {
    loca: Loca<'a>,
    glyf: &'a [u8],
}

struct CFFAndCharStrings<'a> {
    cff_data: &'a [u8],
    charstrings: Index<'a>,
    charstrings_object_data: &'a [u8],
    charstrings_offset: usize,
    offset_type: OffsetType,
}

impl CFFAndCharStrings<'_> {
    fn from_cff_font<'a>(
        font: &FontRef<'a>,
        charstrings_offset: u32,
        max_glyph_id: GlyphId,
    ) -> Result<CFFAndCharStrings<'a>, ReadError> {
        let cff = font.cff()?;
        let charstrings_data =
            Self::charstrings_data(cff.offset_data(), charstrings_offset as usize)?;
        let charstrings = Index1::read(charstrings_data)?;
        let offset_type = Self::offset_type(charstrings.off_size())?;

        let offset_base = charstrings.data_byte_range().start;
        let charstrings_object_data =
            charstrings_data.split_off(offset_base).ok_or(ReadError::OutOfBounds)?.as_bytes();

        Self::check_glyph_count(charstrings.count() as u32, max_glyph_id)?;

        Ok(CFFAndCharStrings {
            cff_data: cff.offset_data().as_bytes(),
            charstrings: Index::Format1(charstrings),
            charstrings_object_data,
            charstrings_offset: charstrings_offset as usize,
            offset_type,
        })
    }

    fn from_cff2_font<'a>(
        font: &FontRef<'a>,
        charstrings_offset: u32,
        max_glyph_id: GlyphId,
    ) -> Result<CFFAndCharStrings<'a>, ReadError> {
        let cff2 = font.cff2()?;
        let charstrings_data =
            Self::charstrings_data(cff2.offset_data(), charstrings_offset as usize)?;
        let charstrings = Index2::read(charstrings_data)?;
        let offset_type = Self::offset_type(charstrings.off_size())?;

        let offset_base = charstrings.data_byte_range().start;
        let charstrings_object_data =
            charstrings_data.split_off(offset_base).ok_or(ReadError::OutOfBounds)?.as_bytes();

        Self::check_glyph_count(charstrings.count(), max_glyph_id)?;

        Ok(CFFAndCharStrings {
            cff_data: cff2.offset_data().as_bytes(),
            charstrings: Index::Format2(charstrings),
            charstrings_object_data,
            charstrings_offset: charstrings_offset as usize,
            offset_type,
        })
    }

    fn check_glyph_count(count: u32, max_glyph_id: GlyphId) -> Result<(), ReadError> {
        if count != max_glyph_id.to_u32() + 1 {
            return Err(ReadError::MalformedData(
                "CFF/CFF2 charstrings glyph count does not match maxp's.",
            ));
        }
        Ok(())
    }

    fn charstrings_data(
        table_data: FontData,
        charstrings_offset: usize,
    ) -> Result<FontData, ReadError> {
        table_data.split_off(charstrings_offset).ok_or(ReadError::OutOfBounds)
    }

    fn offset_type(size: u8) -> Result<OffsetType, ReadError> {
        Ok(match size {
            1 => OffsetType::CffOne(Default::default()),
            2 => OffsetType::CffTwo(Default::default()),
            3 => OffsetType::CffThree(Default::default()),
            4 => OffsetType::CffFour(Default::default()),
            _ => {
                return Err(ReadError::MalformedData(
                    "Invalid charstrings offset size (is not 1, 2, 3, or 4).",
                ))
            }
        })
    }
}

/// Abstraction of a table which has blocks of data located by an array of
/// ascending offsets (eg. glyf + loca)
trait GlyphDataOffsetArray {
    fn offset_type(&self) -> OffsetType;

    /// Returns which offset types this array could be changed into.
    ///
    /// If no changes are possible will only include offset_type().
    /// Types are listed in ascending order of size.
    fn available_offset_types(&self) -> impl Iterator<Item = OffsetType>;

    /// Returns the offset associated with a specific gid.
    ///
    /// This is the offset at which data for that glyph starts.
    fn offset_for(&self, gid: GlyphId) -> Result<u32, PatchingError>;

    /// Checks that all offsets are in ascending order.
    fn all_offsets_are_ascending(&self) -> bool;

    fn get(&self, range: Range<usize>) -> Result<&[u8], PatchingError>;

    fn add_to_font(
        &self,
        font_builder: &mut FontBuilder,
        offsets: OffsetArrayAndData,
        offset_type: OffsetType,
    ) -> Result<(), PatchingError>;
}

impl GlyphDataOffsetArray for GlyfAndLoca<'_> {
    fn offset_type(&self) -> OffsetType {
        match self.loca {
            Loca::Short(_) => OffsetType::ShortDivByTwo(Default::default()),
            Loca::Long(_) => OffsetType::Long(Default::default()),
        }
    }

    fn available_offset_types(&self) -> impl Iterator<Item = OffsetType> {
        std::iter::once(self.offset_type())
    }

    fn offset_for(&self, gid: GlyphId) -> Result<u32, PatchingError> {
        self.loca
            // Note: get_raw(...) applies the * 2 for short loca when needed.
            .get_raw(gid.to_u32() as usize)
            .ok_or(PatchingError::InvalidPatch("Start loca entry is missing."))
    }

    fn all_offsets_are_ascending(&self) -> bool {
        self.loca.all_offsets_are_ascending()
    }

    fn get(&self, range: Range<usize>) -> Result<&[u8], PatchingError> {
        self.glyf.get(range).ok_or(PatchingError::from(ReadError::OutOfBounds))
    }

    fn add_to_font(
        &self,
        font_builder: &mut FontBuilder,
        offsets: OffsetArrayAndData,
        new_offset_type: OffsetType,
    ) -> Result<(), PatchingError> {
        if new_offset_type != self.offset_type() {
            // glyf/loca does not support changing offset types.
            return Err(PatchingError::SerializationError(
                SerializeErrorFlags::SERIALIZE_ERROR_OFFSET_OVERFLOW,
            ));
        }

        font_builder.add_raw(Glyf::TAG, offsets.data);
        font_builder.add_raw(Loca::TAG, offsets.offset_array);
        Ok(())
    }
}

impl GlyphDataOffsetArray for Gvar<'_> {
    fn offset_type(&self) -> OffsetType {
        if self.flags().contains(GvarFlags::LONG_OFFSETS) {
            OffsetType::Long(Default::default())
        } else {
            OffsetType::ShortDivByTwo(Default::default())
        }
    }

    fn available_offset_types(&self) -> impl Iterator<Item = OffsetType> {
        [OffsetType::ShortDivByTwo(ShortDivByTwoInfo), OffsetType::Long(LongInfo)].iter().copied()
    }

    fn offset_for(&self, gid: GlyphId) -> Result<u32, PatchingError> {
        Ok(self
            .glyph_variation_data_offsets()
            .get(gid.to_u32() as usize)
            .map_err(PatchingError::FontParsingFailed)?
            .get())
    }

    fn all_offsets_are_ascending(&self) -> bool {
        let mut prev: Option<u32> = None;
        for v in self.glyph_variation_data_offsets().iter() {
            let Ok(v) = v else {
                return false;
            };
            let v = v.get();
            if let Some(prev_value) = prev {
                if prev_value > v {
                    return false;
                }
            }
            prev = Some(v);
        }
        true
    }

    fn get(&self, range: Range<usize>) -> Result<&[u8], PatchingError> {
        self.glyph_variation_data_for_range(range)
            .map_err(PatchingError::FontParsingFailed)
            .map(|fd| fd.as_bytes())
    }

    fn add_to_font(
        &self,
        font_builder: &mut FontBuilder,
        offsets: OffsetArrayAndData,
        new_offset_type: OffsetType,
    ) -> Result<(), PatchingError> {
        const GVAR_FLAGS_OFFSET: usize = 15;

        // typical gvar layout (see: https://learn.microsoft.com/en-us/typography/opentype/spec/gvar):
        //   Part 1 - Header
        //   Part 2 - glyphVariationDataOffsets[glyphCount + 1]
        //   Part 3 - Shared Tuples
        //   Part 4 - Array of per glyph variation data
        //
        // When constructing a new gvar from the newly synthesized data we'll be
        // replacing Part 2 and 4 with 'offsets' and 'data' respectively. In
        // order to be consistent with the open type spec which prescribes the
        // above ordering we always output the parts in the spec ordering
        // regardless of how they were ordered in the original table. Additionally this
        // will correctly resolve cases where the original table had overlapping
        // shared tuple and glyph variation data. However, as a result we may
        // need to change offsets in part 1 if ordering gets modified. The skera
        // serializer is used to recalculate offsets as needed.
        let orig_bytes = self.as_bytes();
        let orig_size = orig_bytes.len();

        let original_offsets_range = self.glyph_variation_data_offsets_byte_range();

        if new_offset_type == self.offset_type()
            && offsets.offset_array.len()
                != original_offsets_range.end - original_offsets_range.start
        {
            // computed offsets array length should not have changed from the original
            // offsets array length if offset type is not changing.
            return Err(PatchingError::InternalError);
        }

        let part1_header_pre_flag =
            orig_bytes.get(0..GVAR_FLAGS_OFFSET).ok_or(PatchingError::InternalError)?;

        let part1_header_post_flag = orig_bytes
            .get(GVAR_FLAGS_OFFSET + 1..original_offsets_range.start)
            .ok_or(PatchingError::InternalError)?;

        let mut flags: u8 =
            orig_bytes.get(GVAR_FLAGS_OFFSET).copied().ok_or(PatchingError::InternalError)?;
        if new_offset_type.offset_width() == 4 {
            flags |= 0b00000001;
        } else {
            flags &= 0b11111110;
        }

        let max_new_size = orig_size + offsets.data.len();

        // part 1 and 2 - write gvar header and offsets
        let mut serializer = Serializer::new(max_new_size);
        serializer
            .start_serialize()
            .and(serializer.embed_bytes(part1_header_pre_flag))
            .and(serializer.embed(flags))
            .and(serializer.embed_bytes(part1_header_post_flag))
            .and(serializer.embed_bytes(&offsets.offset_array))
            .map_err(PatchingError::from)?;

        // part 4 - write new glyph variation data
        serializer
            .push()
            .and(serializer.embed_bytes(&offsets.data))
            .map_err(PatchingError::from)?;

        let glyph_data_obj = serializer
            .pop_pack(false)
            .ok_or_else(|| PatchingError::SerializationError(serializer.error()))?;

        // part 3 - write shared tuples.
        let shared_tuples = self.shared_tuples().map_err(PatchingError::from)?;

        let shared_tuples_bytes = shared_tuples
            .offset_data()
            .as_bytes()
            .get(shared_tuples.tuples_byte_range())
            .ok_or_else(|| PatchingError::SerializationError(serializer.error()))?;

        let shared_tuples_obj = if !shared_tuples_bytes.is_empty() {
            serializer
                .push()
                .and(serializer.embed_bytes(shared_tuples_bytes))
                .map_err(PatchingError::from)?;

            // The spec says that shared tuple data should come before glyph variation data
            // so use a virtual link to ensure the correct ordering.
            serializer.add_virtual_link(glyph_data_obj)?;

            serializer
                .pop_pack(false)
                .ok_or_else(|| PatchingError::SerializationError(serializer.error()))?
        } else {
            // If there's no shared tuples just point the shared tuples offset to the start
            // of glyph_data_obj (since it can't be null).
            glyph_data_obj
        };

        // Set up offsets to shared tuples and glyph data.
        serializer
            .add_link(
                self.shared_tuples_offset_byte_range(),
                shared_tuples_obj,
                OffsetWhence::Head,
                0,
                false,
            )
            .and(serializer.add_link(
                self.glyph_variation_data_array_offset_byte_range(),
                glyph_data_obj,
                OffsetWhence::Head,
                0,
                false,
            ))
            .map_err(PatchingError::from)?;

        // Generate the final output
        serializer.end_serialize();
        let new_gvar = serializer.copy_bytes();
        font_builder.add_raw(Gvar::TAG, new_gvar);
        Ok(())
    }
}

impl GlyphDataOffsetArray for CFFAndCharStrings<'_> {
    fn offset_type(&self) -> OffsetType {
        self.offset_type
    }

    fn available_offset_types(&self) -> impl Iterator<Item = OffsetType> {
        [
            OffsetType::CffOne(CffOneInfo),
            OffsetType::CffTwo(CffTwoInfo),
            OffsetType::CffThree(CffThreeInfo),
            OffsetType::CffFour(CffFourInfo),
        ]
        .iter()
        .copied()
    }

    fn offset_for(&self, gid: GlyphId) -> Result<u32, PatchingError> {
        self.charstrings
            .get_offset(gid.to_u32() as usize)
            .map_err(|_| PatchingError::FontParsingFailed(ReadError::OutOfBounds))
            .map(|offset| offset as u32)
    }

    fn all_offsets_are_ascending(&self) -> bool {
        let it1 = (0..self.charstrings.count()).map(|index| self.offset_for(GlyphId::new(index)));
        let it2 = it1.clone().skip(1);

        !it1.zip(it2).any(|(start, end)| {
            let (Ok(start), Ok(end)) = (start, end) else {
                return true;
            };
            start > end
        })
    }

    fn get(&self, range: Range<usize>) -> Result<&[u8], PatchingError> {
        self.charstrings_object_data
            .get(range)
            .ok_or(PatchingError::FontParsingFailed(ReadError::OutOfBounds))
    }

    fn add_to_font(
        &self,
        font_builder: &mut FontBuilder,
        offsets: OffsetArrayAndData,
        offset_type: OffsetType,
    ) -> Result<(), PatchingError> {
        // The IFT specification requires that for IFT fonts CFF tables must have the
        // charstrings data at the end and not overlapping anything (see: https://w3c.github.io/IFT/Overview.html#cff).
        //
        // This allows us to significantly simplify the CFF table reconstruction in this
        // method:
        // 1. Copy everything preceding charstrings unmodified into the new table.
        // 2. Synthesize a new charstrings to the requested offset size.
        let (count_width, table_tag) = match &self.charstrings {
            Index::Format1(_) => (2, Cff::TAG),
            Index::Format2(_) => (4, Cff2::TAG),
            Index::Empty => return Err(PatchingError::InternalError),
        };

        let offset_data: &[u8] = &offsets.offset_array;
        let outline_data: &[u8] = &offsets.data;
        let max_new_size = self.charstrings_offset + // this is the size of everything other than charstrings
            outline_data.len() +
            offset_data.len() +
            1 + count_width; // header size for INDEX

        let mut serializer = Serializer::new(max_new_size);

        // Part 1 - Non charstrings data.
        let r = serializer.start_serialize().and(
            serializer.embed_bytes(
                self.cff_data
                    .get(0..self.charstrings_offset)
                    .ok_or(PatchingError::FontParsingFailed(ReadError::OutOfBounds))?,
            ),
        );

        // Part 2 - Charstrings data.
        let r = match &self.charstrings {
            // Count size differs between format 1 and 2 so pull out the inner type in
            // order to embed the count with the correct width.
            Index::Format1(charstrings) => r.and(serializer.embed(charstrings.count())),
            Index::Format2(charstrings) => r.and(serializer.embed(charstrings.count())),
            Index::Empty => return Err(PatchingError::InternalError),
        };

        r.and(serializer.embed(offset_type.offset_width() as u8))
            .and(serializer.embed_bytes(offset_data))
            .and(serializer.embed_bytes(outline_data))
            .map_err(PatchingError::SerializationError)?;

        // Generate the final output
        serializer.end_serialize();
        let new_cff = serializer.copy_bytes();

        font_builder.add_raw(table_tag, new_cff);

        Ok(())
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use std::{
        collections::{BTreeSet, HashMap},
        io::Write,
        iter,
    };

    use brotlic::CompressorWriter;
    use read_fonts::{
        collections::IntSet,
        tables::{
            cff2::Cff2,
            glyf::Glyf,
            gvar::Gvar,
            ift::{CompatibilityId, GlyphKeyedPatch, IFTX_TAG, IFT_TAG},
            loca::Loca,
        },
        FontData, FontRead, ReadError, TableProvider, TopLevelTable,
    };
    use shared_brotli_patch_decoder::BuiltInBrotliDecoder;

    use font_test_data::{
        bebuffer::BeBuffer,
        ift::{
            cff_u16_glyph_patches, format2_with_one_charstrings_offset,
            glyf_and_gvar_u16_glyph_patches, glyf_u16_glyph_patches, glyf_u16_glyph_patches_2,
            glyph_keyed_patch_header, long_gvar_with_shared_tuples, noop_glyf_glyph_patches,
            out_of_order_gvar_with_shared_tuples, short_gvar_near_maximum_offset_size,
            short_gvar_with_no_shared_tuples, short_gvar_with_shared_tuples, CFF2_FONT,
            CFF2_FONT_CHARSTRINGS_OFFSET, CFF_FONT, CFF_FONT_CHARSTRINGS_OFFSET,
        },
    };
    use skrifa::{FontRef, GlyphId, Tag};
    use write_fonts::FontBuilder;

    use crate::{
        font_patch::PatchingError,
        glyph_keyed::{apply_glyph_keyed_patches, CffFourInfo, ShortDivByTwoInfo},
        patchmap::{PatchId, PatchUrl},
        testdata::{test_font_for_patching, test_font_for_patching_with_loca_mod},
    };

    use super::{CFFAndCharStrings, IftTableTag, LongInfo, OffsetType, PatchInfo};

    pub(crate) fn assemble_glyph_keyed_patch(mut header: BeBuffer, payload: BeBuffer) -> BeBuffer {
        let payload_data: &[u8] = &payload;
        let mut compressor = CompressorWriter::new(Vec::new());
        compressor.write_all(payload_data).unwrap();
        let compressed = compressor.into_inner().unwrap();

        header.write_at("max_uncompressed_length", payload_data.len() as u32);
        header.extend(compressed)
    }

    fn check_tables_equal(a: &FontRef, b: &FontRef, excluding: BTreeSet<Tag>) {
        let it_a = a
            .table_directory()
            .table_records()
            .iter()
            .map(|r| r.tag())
            .filter(|tag| !excluding.contains(tag));
        let it_b = b
            .table_directory()
            .table_records()
            .iter()
            .map(|r| r.tag())
            .filter(|tag| !excluding.contains(tag));

        for (tag_a, tag_b) in it_a.zip(it_b) {
            assert_eq!(tag_a, tag_b);
            let data_a = a.table_data(tag_a).unwrap();
            let data_b = b.table_data(tag_b).unwrap();
            if tag_a == Tag::new(b"head") {
                // ignore the head.checksum_adjustment, which will necessarily differ
                assert_eq!(data_a.as_bytes()[..8], data_b.as_bytes()[..8]);
                assert_eq!(data_a.as_bytes()[12..], data_b.as_bytes()[12..]);
            } else {
                assert_eq!(data_a.as_bytes(), data_b.as_bytes(), "{}", tag_a);
            }
        }
    }

    fn patch_info(tag: Tag, bit_index: usize) -> PatchInfo {
        let source = match &tag.to_be_bytes() {
            b"IFT " => IftTableTag::Ift(CompatibilityId::from_u32s([0, 0, 0, 0])),
            b"IFTX" => IftTableTag::Iftx(CompatibilityId::from_u32s([0, 0, 0, 0])),
            _ => panic!("Unexpected tag value."),
        };

        let mut info = PatchInfo {
            url: PatchUrl::expand_template(&[], &PatchId::Numeric(0)).unwrap(),
            source_table: source,
            application_flag_bit_indices: IntSet::<u32>::empty(),
        };
        info.application_flag_bit_indices.insert(bit_index as u32);
        info
    }

    #[test]
    fn noop_glyph_keyed() {
        let patch =
            assemble_glyph_keyed_patch(glyph_keyed_patch_header(), noop_glyf_glyph_patches());
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();

        let font = test_font_for_patching();
        let font = FontRef::new(&font).unwrap();

        let patch_info = patch_info(IFT_TAG, 4);

        let patched =
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder)
                .unwrap();
        let patched = FontRef::new(&patched).unwrap();

        // Application bit will be set in the patched font.
        let expected_font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([(IFT_TAG, vec![0b0001_0000, 0, 0, 0].as_slice())]),
        );
        let expected_font = FontRef::new(&expected_font).unwrap();
        check_tables_equal(&expected_font, &patched, BTreeSet::default());
    }

    #[test]
    fn basic_glyph_keyed() {
        let patch =
            assemble_glyph_keyed_patch(glyph_keyed_patch_header(), glyf_u16_glyph_patches());
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();

        let font = test_font_for_patching();
        let font = FontRef::new(&font).unwrap();

        let patch_info = patch_info(IFT_TAG, 28);
        let patched =
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder)
                .unwrap();
        let patched = FontRef::new(&patched).unwrap();

        let new_ift: &[u8] = patched.table_data(IFT_TAG).unwrap().as_bytes();
        assert_eq!(&[0, 0, 0, 0b0001_0000], new_ift);

        let new_glyf: &[u8] = patched.table_data(Glyf::TAG).unwrap().as_bytes();
        assert_eq!(
            &[
                1, 2, 3, 4, 5, 0, // gid 0
                6, 7, 8, 0, // gid 1
                b'a', b'b', b'c', 0, // gid2
                b'd', b'e', b'f', b'g', // gid 7
                b'h', b'i', b'j', b'k', b'l', 0, // gid 8 + 9
                b'm', b'n', // gid 13
            ],
            new_glyf
        );

        let new_loca = patched.loca(None).unwrap();
        let indices: Vec<u32> = (0..=15).map(|gid| new_loca.get_raw(gid).unwrap()).collect();

        assert_eq!(
            vec![
                0,  // gid 0
                6,  // gid 1
                10, // gid 2
                14, // gid 3
                14, // gid 4
                14, // gid 5
                14, // gid 6
                14, // gid 7
                18, // gid 8
                18, // gid 9
                24, // gid 10
                24, // gid 11
                24, // gid 12
                24, // gid 13
                26, // gid 14
                26, // end
            ],
            indices
        );

        check_tables_equal(&font, &patched, [IFT_TAG, Glyf::TAG, Loca::TAG].into());
    }

    #[test]
    fn basic_glyph_keyed_with_long_loca() {
        let patch =
            assemble_glyph_keyed_patch(glyph_keyed_patch_header(), glyf_u16_glyph_patches());
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();

        let font = test_font_for_patching_with_loca_mod(
            false, // force long loca
            |_| {},
            HashMap::from([(IFT_TAG, vec![0, 0, 0, 0].as_slice())]),
        );
        let font = FontRef::new(&font).unwrap();

        let patch_info = patch_info(IFT_TAG, 28);
        let patched =
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder)
                .unwrap();
        let patched = FontRef::new(&patched).unwrap();

        let new_ift: &[u8] = patched.table_data(IFT_TAG).unwrap().as_bytes();
        assert_eq!(&[0, 0, 0, 0b0001_0000], new_ift);

        let new_glyf: &[u8] = patched.table_data(Glyf::TAG).unwrap().as_bytes();
        assert_eq!(
            &[
                1, 2, 3, 4, 5, 0, // gid 0
                6, 7, 8, 0, // gid 1
                b'a', b'b', b'c', // gid2
                b'd', b'e', b'f', b'g', // gid 7
                b'h', b'i', b'j', b'k', b'l', // gid 8 + 9
                b'm', b'n', // gid 13
            ],
            new_glyf
        );

        let new_loca = patched.loca(None).unwrap();
        let indices: Vec<u32> = (0..=15).map(|gid| new_loca.get_raw(gid).unwrap()).collect();

        assert_eq!(
            vec![
                0,  // gid 0
                6,  // gid 1
                10, // gid 2
                13, // gid 3
                13, // gid 4
                13, // gid 5
                13, // gid 6
                13, // gid 7
                17, // gid 8
                17, // gid 9
                22, // gid 10
                22, // gid 11
                22, // gid 12
                22, // gid 13
                24, // gid 14
                24, // end
            ],
            indices
        );

        check_tables_equal(&font, &patched, [IFT_TAG, Glyf::TAG, Loca::TAG].into());
    }

    #[test]
    fn multiple_glyph_keyed() {
        let patch =
            assemble_glyph_keyed_patch(glyph_keyed_patch_header(), glyf_u16_glyph_patches());
        let patch: &[u8] = &patch;
        let patch1 = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info_1 = patch_info(IFTX_TAG, 13);

        let patch =
            assemble_glyph_keyed_patch(glyph_keyed_patch_header(), glyf_u16_glyph_patches_2());
        let patch: &[u8] = &patch;
        let patch2 = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info_2 = patch_info(IFTX_TAG, 28);

        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([(IFTX_TAG, vec![0, 0, 0, 0].as_slice())]),
        );
        let font = FontRef::new(&font).unwrap();

        let patched = apply_glyph_keyed_patches(
            &[(&patch_info_2, patch2), (&patch_info_1, patch1)],
            &font,
            &BuiltInBrotliDecoder,
        )
        .unwrap();
        let patched = FontRef::new(&patched).unwrap();

        let new_ift: &[u8] = patched.table_data(IFTX_TAG).unwrap().as_bytes();
        assert_eq!(&[0, 0b0010_0000, 0, 0b0001_0000], new_ift);

        let new_glyf: &[u8] = patched.table_data(Glyf::TAG).unwrap().as_bytes();
        assert_eq!(
            &[
                1, 2, 3, 4, 5, 0, // gid 0
                6, 7, 8, 0, // gid 1
                b'a', b'b', b'c', 0, // gid2
                b'q', b'r', // gid 7
                b'h', b'i', b'j', b'k', b'l', 0, // gid 8 + 9
                b's', b't', b'u', 0, // gid 12
                b'm', b'n', // gid 13
                b'v', 0, // gid 14
            ],
            new_glyf
        );

        let new_loca = patched.loca(None).unwrap();
        let indices: Vec<u32> = (0..=15).map(|gid| new_loca.get_raw(gid).unwrap()).collect();

        assert_eq!(
            vec![
                0,  // gid 0
                6,  // gid 1
                10, // gid 2
                14, // gid 3
                14, // gid 4
                14, // gid 5
                14, // gid 6
                14, // gid 7
                16, // gid 8
                16, // gid 9
                22, // gid 10
                22, // gid 11
                22, // gid 12
                26, // gid 13
                28, // gid 14
                30, // end
            ],
            indices
        );

        check_tables_equal(&font, &patched, [Glyf::TAG, Loca::TAG, IFTX_TAG].into());
    }

    #[test]
    fn glyph_keyed_glyf_and_gvar() {
        let patch = assemble_glyph_keyed_patch(
            glyph_keyed_patch_header(),
            glyf_and_gvar_u16_glyph_patches(),
        );
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let gvar = short_gvar_with_shared_tuples();

        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([
                (Gvar::TAG, gvar.as_slice()),
                (Tag::new(b"IFT "), vec![0, 0, 0, 0].as_slice()),
            ]),
        );
        let font = FontRef::new(&font).unwrap();

        let patched =
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder)
                .unwrap();
        let patched = FontRef::new(&patched).unwrap();

        let new_gvar: &[u8] = patched.table_data(Gvar::TAG).unwrap().as_bytes();

        let mut expected_gvar: Vec<u8> = vec![];

        let change_start = gvar.offset_for("glyph_offset[3]");

        expected_gvar.extend_from_slice(gvar.get(0..change_start).unwrap());
        // Offsets
        expected_gvar.extend_from_slice(&[
            0x00, 0x03, // gid 3
            0x00, 0x03, // gid 4
            0x00, 0x03, // gid 5
            0x00, 0x03, // gid 6
            0x00, 0x03, // gid 7
            0x00, 0x05, // gid 8
            0x00, 0x06, // gid 9
            0x00, 0x06, // gid 10
            0x00, 0x06, // gid 11
            0x00, 0x06, // gid 12
            0x00, 0x06, // gid 13
            0x00, 0x06, // gid 14
            0x00, 0x06u8, // trailing
        ]);
        // Shared tuples
        expected_gvar.extend_from_slice(&[0, 42, 0, 13, 0, 25u8]);
        // Data
        expected_gvar.extend_from_slice(&[
            1, 2, 3, 4, // gid 0
            b'm', b'n', // gid 2
            b'o', b'p', b'q', 0, // gid 7
            b'r', 0u8, // gid 8
        ]);
        assert_eq!(&expected_gvar, new_gvar);
    }

    #[test]
    fn glyph_keyed_glyf_and_long_gvar() {
        let patch = assemble_glyph_keyed_patch(
            glyph_keyed_patch_header(),
            glyf_and_gvar_u16_glyph_patches(),
        );
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let gvar = long_gvar_with_shared_tuples();

        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([
                (Gvar::TAG, gvar.as_slice()),
                (Tag::new(b"IFT "), vec![0, 0, 0, 0].as_slice()),
            ]),
        );
        let font = FontRef::new(&font).unwrap();

        let patched =
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder)
                .unwrap();
        let patched = FontRef::new(&patched).unwrap();

        let new_gvar: &[u8] = patched.table_data(Gvar::TAG).unwrap().as_bytes();

        let mut expected_gvar: Vec<u8> = vec![];

        let change_start = gvar.offset_for("glyph_offset[3]");

        expected_gvar.extend_from_slice(gvar.get(0..change_start).unwrap());
        // Offsets
        expected_gvar.extend_from_slice(&[
            0x00, 0x00, 0x00, 0x06, // gid 3
            0x00, 0x00, 0x00, 0x06, // gid 4
            0x00, 0x00, 0x00, 0x06, // gid 5
            0x00, 0x00, 0x00, 0x06, // gid 6
            0x00, 0x00, 0x00, 0x06, // gid 7
            0x00, 0x00, 0x00, 0x09, // gid 8
            0x00, 0x00, 0x00, 0x0A, // gid 9
            0x00, 0x00, 0x00, 0x0A, // gid 10
            0x00, 0x00, 0x00, 0x0A, // gid 11
            0x00, 0x00, 0x00, 0x0A, // gid 12
            0x00, 0x00, 0x00, 0x0A, // gid 13
            0x00, 0x00, 0x00, 0x0A, // gid 14
            0x00, 0x00, 0x00, 0x0Au8, // trailing
        ]);
        // Shared tuples
        expected_gvar.extend_from_slice(&[0, 42, 0, 13, 0, 25u8]);
        // Data
        expected_gvar.extend_from_slice(&[
            1, 2, 3, 4, // gid 0
            b'm', b'n', // gid 2
            b'o', b'p', b'q', // gid 7
            b'r', // gid 8
        ]);
        assert_eq!(&expected_gvar, new_gvar);
    }

    #[test]
    fn glyph_keyed_glyf_and_gvar_no_shared_tuples() {
        let patch = assemble_glyph_keyed_patch(
            glyph_keyed_patch_header(),
            glyf_and_gvar_u16_glyph_patches(),
        );
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let gvar = short_gvar_with_no_shared_tuples();

        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([
                (Gvar::TAG, gvar.as_slice()),
                (Tag::new(b"IFT "), vec![0, 0, 0, 0].as_slice()),
            ]),
        );
        let font = FontRef::new(&font).unwrap();

        let patched =
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder)
                .unwrap();
        let patched = FontRef::new(&patched).unwrap();

        let new_gvar: &[u8] = patched.table_data(Gvar::TAG).unwrap().as_bytes();

        let mut expected_gvar: Vec<u8> = vec![];

        let change_start = gvar.offset_for("glyph_offset[3]");

        expected_gvar.extend_from_slice(gvar.get(0..change_start).unwrap());
        // Offsets
        expected_gvar.extend_from_slice(&[
            0x00, 0x03, // gid 3
            0x00, 0x03, // gid 4
            0x00, 0x03, // gid 5
            0x00, 0x03, // gid 6
            0x00, 0x03, // gid 7
            0x00, 0x05, // gid 8
            0x00, 0x06, // gid 9
            0x00, 0x06, // gid 10
            0x00, 0x06, // gid 11
            0x00, 0x06, // gid 12
            0x00, 0x06, // gid 13
            0x00, 0x06, // gid 14
            0x00, 0x06u8, // trailing
        ]);
        // Data
        expected_gvar.extend_from_slice(&[
            1, 2, 3, 4, // gid 0
            b'm', b'n', // gid 2
            b'o', b'p', b'q', 0, // gid 7
            b'r', 0u8, // gid 8
        ]);
        assert_eq!(&expected_gvar, new_gvar);
    }

    #[test]
    fn glyph_keyed_glyf_and_gvar_overlapping_shared_tuples() {
        let patch = assemble_glyph_keyed_patch(
            glyph_keyed_patch_header(),
            glyf_and_gvar_u16_glyph_patches(),
        );
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let mut gvar = short_gvar_with_no_shared_tuples();
        gvar.write_at("shared_tuple_count", 2u16);

        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([
                (Gvar::TAG, gvar.as_slice()),
                (Tag::new(b"IFT "), vec![0, 0, 0, 0].as_slice()),
            ]),
        );
        let font = FontRef::new(&font).unwrap();

        let patched =
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder)
                .unwrap();
        let patched = FontRef::new(&patched).unwrap();

        let new_gvar: &[u8] = patched.table_data(Gvar::TAG).unwrap().as_bytes();

        let mut expected_gvar: Vec<u8> = vec![];

        let change_start = gvar.offset_for("glyph_offset[3]");

        gvar.write_at("glyph_variation_data_offset", (gvar.offset_for("glyph_0") + 4) as u32); // glyph variation data gets shifted by 4 bytes due to duplication of 4 bytes of
                                                                                               // shared tuple data.
        expected_gvar.extend_from_slice(gvar.get(0..change_start).unwrap());
        // Offsets
        expected_gvar.extend_from_slice(&[
            0x00, 0x03, // gid 3
            0x00, 0x03, // gid 4
            0x00, 0x03, // gid 5
            0x00, 0x03, // gid 6
            0x00, 0x03, // gid 7
            0x00, 0x05, // gid 8
            0x00, 0x06, // gid 9
            0x00, 0x06, // gid 10
            0x00, 0x06, // gid 11
            0x00, 0x06, // gid 12
            0x00, 0x06, // gid 13
            0x00, 0x06, // gid 14
            0x00, 0x06u8, // trailing
        ]);
        // Shared tuples
        expected_gvar.extend_from_slice(&[1, 2, 3, 4u8]); // overlapping portion is duplicated into its own region.
                                                          // Data
        expected_gvar.extend_from_slice(&[
            1, 2, 3, 4, // gid 0
            b'm', b'n', // gid 2
            b'o', b'p', b'q', 0, // gid 7
            b'r', 0u8, // gid 8
        ]);
        assert_eq!(&expected_gvar, new_gvar);
    }

    #[test]
    fn glyph_keyed_out_of_order_gvar() {
        let patch = assemble_glyph_keyed_patch(
            glyph_keyed_patch_header(),
            glyf_and_gvar_u16_glyph_patches(),
        );
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let gvar = out_of_order_gvar_with_shared_tuples();

        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([
                (Gvar::TAG, gvar.as_slice()),
                (Tag::new(b"IFT "), vec![0, 0, 0, 0].as_slice()),
            ]),
        );
        let font = FontRef::new(&font).unwrap();

        let patched =
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder)
                .unwrap();
        let patched = FontRef::new(&patched).unwrap();

        let new_gvar: &[u8] = patched.table_data(Gvar::TAG).unwrap().as_bytes();

        let mut expected_gvar: Vec<u8> = vec![];

        // Patching will reorder the gvar table to the expected spec ordering, so for
        // expected compare to the correctly ordered version.
        let gvar = short_gvar_with_shared_tuples();
        let change_start = gvar.offset_for("glyph_offset[3]");

        expected_gvar.extend_from_slice(gvar.get(0..change_start).unwrap());
        // Offsets
        expected_gvar.extend_from_slice(&[
            0x00, 0x03, // gid 3
            0x00, 0x03, // gid 4
            0x00, 0x03, // gid 5
            0x00, 0x03, // gid 6
            0x00, 0x03, // gid 7
            0x00, 0x05, // gid 8
            0x00, 0x06, // gid 9
            0x00, 0x06, // gid 10
            0x00, 0x06, // gid 11
            0x00, 0x06, // gid 12
            0x00, 0x06, // gid 13
            0x00, 0x06, // gid 14
            0x00, 0x06u8, // trailing
        ]);
        // Shared tuples
        expected_gvar.extend_from_slice(&[0, 42, 0, 13, 0, 25u8]);
        // Data
        expected_gvar.extend_from_slice(&[
            1, 2, 3, 4, // gid 0
            b'm', b'n', // gid 2
            b'o', b'p', b'q', 0, // gid 7
            b'r', 0u8, // gid 8
        ]);
        assert_eq!(&expected_gvar, new_gvar);
    }

    #[test]
    fn glyph_keyed_gvar_requires_offset_type_switch() {
        let patch = assemble_glyph_keyed_patch(
            glyph_keyed_patch_header(),
            glyf_and_gvar_u16_glyph_patches(),
        );
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let gvar = short_gvar_near_maximum_offset_size();

        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([
                (Gvar::TAG, gvar.as_slice()),
                (Tag::new(b"IFT "), vec![0, 0, 0, 0].as_slice()),
            ]),
        );
        let font = FontRef::new(&font).unwrap();

        let patched =
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder)
                .unwrap();
        let patched = FontRef::new(&patched).unwrap();

        let new_gvar: &[u8] = patched.table_data(Gvar::TAG).unwrap().as_bytes();
        let new_gvar = FontData::new(new_gvar);
        let new_gvar = Gvar::read(new_gvar).unwrap();

        let gid0_data = vec![1u8; 131066];
        assert_eq!(new_gvar.data_for_gid(GlyphId::new(0)).unwrap().unwrap().as_bytes(), &gid0_data);

        assert!(new_gvar.data_for_gid(GlyphId::new(1)).unwrap().is_none());

        assert_eq!(new_gvar.data_for_gid(GlyphId::new(2)).unwrap().unwrap().as_bytes(), b"mn");

        assert!(new_gvar.data_for_gid(GlyphId::new(6)).unwrap().is_none());

        assert_eq!(new_gvar.data_for_gid(GlyphId::new(7)).unwrap().unwrap().as_bytes(), b"opq",);

        assert_eq!(new_gvar.data_for_gid(GlyphId::new(8)).unwrap().unwrap().as_bytes(), b"r",);

        assert!(new_gvar.data_for_gid(GlyphId::new(9)).unwrap().is_none());
    }

    #[test]
    fn glyph_keyed_bad_format() {
        let mut header_builder = glyph_keyed_patch_header();
        header_builder.write_at("format", Tag::new(b"iftk"));
        let patch = assemble_glyph_keyed_patch(header_builder, glyf_u16_glyph_patches());
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let font = test_font_for_patching();
        let font = FontRef::new(&font).unwrap();

        assert_eq!(
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder),
            Err(PatchingError::InvalidPatch("Patch file tag is not 'ifgk'"))
        );
    }

    #[test]
    fn glyph_keyed_unknown_table() {
        let mut builder = glyf_and_gvar_u16_glyph_patches();
        builder.write_at("gvar_tag", Tag::new(b"hijk"));

        let patch = assemble_glyph_keyed_patch(glyph_keyed_patch_header(), builder);
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let font = test_font_for_patching();
        let font = FontRef::new(&font).unwrap();

        let patched =
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder)
                .unwrap();
        let patched = FontRef::new(&patched).unwrap();

        let new_glyf: &[u8] = patched.table_data(Glyf::TAG).unwrap().as_bytes();
        assert_eq!(
            &[
                1, 2, 3, 4, 5, 0, // gid 0
                6, 7, 8, 0, // gid 1
                b'a', b'b', b'c', 0, // gid2
                b'd', b'e', b'f', b'g', // gid 7
                b'h', b'i', b'j', b'k', b'l', 0, // gid 8
            ],
            new_glyf
        );

        let new_loca = patched.loca(None).unwrap();
        let indices: Vec<u32> = (0..=15).map(|gid| new_loca.get_raw(gid).unwrap()).collect();

        assert_eq!(
            vec![
                0,  // gid 0
                6,  // gid 1
                10, // gid 2
                14, // gid 3
                14, // gid 4
                14, // gid 5
                14, // gid 6
                14, // gid 7
                18, // gid 8
                24, // gid 9
                24, // gid 10
                24, // gid 11
                24, // gid 12
                24, // gid 13
                24, // gid 14
                24, // end
            ],
            indices
        );

        check_tables_equal(&font, &patched, [Glyf::TAG, Loca::TAG, IFT_TAG].into());
    }

    #[test]
    fn glyph_keyed_unsorted_tables() {
        let mut builder = glyf_and_gvar_u16_glyph_patches();
        builder.write_at("gvar_tag", Tag::new(b"glye"));
        let patch = assemble_glyph_keyed_patch(glyph_keyed_patch_header(), builder);
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let font = test_font_for_patching();
        let font = FontRef::new(&font).unwrap();

        assert_eq!(
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder),
            Err(PatchingError::InvalidPatch("Duplicate or unsorted table tag."))
        );
    }

    #[test]
    fn glyph_keyed_duplicate_tables() {
        let mut builder = glyf_and_gvar_u16_glyph_patches();
        builder.write_at("gvar_tag", Glyf::TAG);
        let patch = assemble_glyph_keyed_patch(glyph_keyed_patch_header(), builder);
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let font = test_font_for_patching();
        let font = FontRef::new(&font).unwrap();

        assert_eq!(
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder),
            Err(PatchingError::InvalidPatch("Duplicate or unsorted table tag."))
        );
    }

    #[test]
    fn glyph_keyed_unsorted_gids() {
        let mut builder = glyf_u16_glyph_patches();
        builder.write_at("gid_8", 6);
        let patch = assemble_glyph_keyed_patch(glyph_keyed_patch_header(), builder);
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let font = test_font_for_patching();
        let font = FontRef::new(&font).unwrap();

        assert_eq!(
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder),
            Err(PatchingError::PatchParsingFailed(ReadError::MalformedData(
                "Glyph IDs are unsorted or duplicated."
            ))),
        );
    }

    #[test]
    fn glyph_keyed_duplicate_gids() {
        let mut builder = glyf_u16_glyph_patches();
        builder.write_at("gid_8", 7);
        let patch = assemble_glyph_keyed_patch(glyph_keyed_patch_header(), builder);
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let font = test_font_for_patching();
        let font = FontRef::new(&font).unwrap();

        assert_eq!(
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder),
            Err(PatchingError::PatchParsingFailed(ReadError::MalformedData(
                "Glyph IDs are unsorted or duplicated."
            ))),
        );
    }

    #[test]
    fn glyph_keyed_uncompressed_length_to_small() {
        let len = glyf_u16_glyph_patches().as_slice().len();
        let mut patch =
            assemble_glyph_keyed_patch(glyph_keyed_patch_header(), glyf_u16_glyph_patches());
        patch.write_at("max_uncompressed_length", len as u32 - 1);
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let font = test_font_for_patching();
        let font = FontRef::new(&font).unwrap();

        assert_eq!(
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder),
            Err(PatchingError::InvalidPatch("Max size exceeded.")),
        );
    }

    #[test]
    fn glyph_keyed_max_glyph_exceeded() {
        let mut builder = glyf_u16_glyph_patches();
        builder.write_at("gid_13", 15u16);
        let patch = assemble_glyph_keyed_patch(glyph_keyed_patch_header(), builder);
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let font = test_font_for_patching();
        let font = FontRef::new(&font).unwrap();

        assert_eq!(
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder),
            Err(PatchingError::InvalidPatch("Patch would add a glyph beyond this fonts maximum.")),
        );
    }

    #[test]
    fn glyph_keyed_unordered_loca_offsets() {
        let patch =
            assemble_glyph_keyed_patch(glyph_keyed_patch_header(), glyf_u16_glyph_patches());
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        // unorder offsets related to a glyph not being replaced
        let font = test_font_for_patching_with_loca_mod(
            true,
            |loca| {
                let loca_gid_1 = loca[1];
                let loca_gid_2 = loca[2];
                loca[1] = loca_gid_2;
                loca[2] = loca_gid_1;
            },
            Default::default(),
        );

        let font = FontRef::new(&font).unwrap();

        assert_eq!(
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &font, &BuiltInBrotliDecoder),
            Err(PatchingError::FontParsingFailed(ReadError::MalformedData(
                "offset array contains unordered offsets."
            ))),
        );
    }

    #[test]
    fn cff_patching() {
        let patch = assemble_glyph_keyed_patch(glyph_keyed_patch_header(), cff_u16_glyph_patches());
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let cff_font = FontRef::new(CFF_FONT).unwrap();
        let mut font_builder = FontBuilder::new();
        font_builder.copy_missing_tables(cff_font);

        let mut ift_table = format2_with_one_charstrings_offset();
        ift_table.write_at("charstrings_offset", CFF_FONT_CHARSTRINGS_OFFSET);
        font_builder.add_raw(Tag::new(b"IFT "), ift_table.data());

        let cff_font_data = font_builder.build();
        let cff_font = FontRef::new(cff_font_data.as_slice()).unwrap();

        let patched =
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &cff_font, &BuiltInBrotliDecoder)
                .unwrap();
        let patched = FontRef::new(&patched).unwrap();

        let old_cff = CFFAndCharStrings::from_cff_font(
            &cff_font,
            CFF_FONT_CHARSTRINGS_OFFSET,
            GlyphId::new(59),
        )
        .unwrap();
        let new_cff = CFFAndCharStrings::from_cff_font(
            &patched,
            CFF_FONT_CHARSTRINGS_OFFSET,
            GlyphId::new(59),
        )
        .unwrap();

        assert_eq!(new_cff.charstrings.off_size(), 2);
        assert_eq!(old_cff.charstrings.count(), new_cff.charstrings.count());

        // Unmodified glyphs
        assert_eq!(old_cff.charstrings.get(0).unwrap(), new_cff.charstrings.get(0).unwrap());
        assert_eq!(old_cff.charstrings.get(2).unwrap(), new_cff.charstrings.get(2).unwrap());
        assert_eq!(old_cff.charstrings.get(34).unwrap(), new_cff.charstrings.get(34).unwrap());
        assert_eq!(old_cff.charstrings.get(37).unwrap(), new_cff.charstrings.get(37).unwrap());
        assert_eq!(old_cff.charstrings.get(39).unwrap(), new_cff.charstrings.get(39).unwrap());

        // Inserted glyphs
        assert_eq!(b"abc", new_cff.charstrings.get(1).unwrap());
        assert_eq!(b"defg", new_cff.charstrings.get(38).unwrap());
        assert_eq!(b"hijkl", new_cff.charstrings.get(47).unwrap());
        assert_eq!(b"mn", new_cff.charstrings.get(59).unwrap());
    }

    #[test]
    fn cff_patching_changes_offset_size() {
        let patch_buffer = cff_u16_glyph_patches();
        let mut patch_buffer = patch_buffer.extend(iter::repeat_n(42u8, 70_000));
        patch_buffer.write_at("end_offset", patch_buffer.len() as u32);

        let patch = assemble_glyph_keyed_patch(glyph_keyed_patch_header(), patch_buffer);
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let cff_font = FontRef::new(CFF_FONT).unwrap();
        let mut font_builder = FontBuilder::new();
        font_builder.copy_missing_tables(cff_font);

        let mut ift_table = format2_with_one_charstrings_offset();
        ift_table.write_at("charstrings_offset", CFF_FONT_CHARSTRINGS_OFFSET);
        font_builder.add_raw(Tag::new(b"IFT "), ift_table.data());

        let cff_font_data = font_builder.build();
        let cff_font = FontRef::new(cff_font_data.as_slice()).unwrap();

        let patched =
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &cff_font, &BuiltInBrotliDecoder)
                .unwrap();
        let patched = FontRef::new(&patched).unwrap();

        let old_cff = CFFAndCharStrings::from_cff_font(
            &cff_font,
            CFF_FONT_CHARSTRINGS_OFFSET,
            GlyphId::new(59),
        )
        .unwrap();
        let new_cff = CFFAndCharStrings::from_cff_font(
            &patched,
            CFF_FONT_CHARSTRINGS_OFFSET, // patching doesn't ever change the offsets location
            GlyphId::new(59),
        )
        .unwrap();

        assert_eq!(new_cff.charstrings.off_size(), 3);
        assert_eq!(old_cff.charstrings.count(), new_cff.charstrings.count());

        // Unmodified glyphs
        assert_eq!(old_cff.charstrings.get(0).unwrap(), new_cff.charstrings.get(0).unwrap());
        assert_eq!(old_cff.charstrings.get(2).unwrap(), new_cff.charstrings.get(2).unwrap());
        assert_eq!(old_cff.charstrings.get(34).unwrap(), new_cff.charstrings.get(34).unwrap());
        assert_eq!(old_cff.charstrings.get(37).unwrap(), new_cff.charstrings.get(37).unwrap());
        assert_eq!(old_cff.charstrings.get(39).unwrap(), new_cff.charstrings.get(39).unwrap());

        // Inserted glyphs
        assert_eq!(b"abc", new_cff.charstrings.get(1).unwrap());
        assert_eq!(b"defg", new_cff.charstrings.get(38).unwrap());
        assert_eq!(b"hijkl", new_cff.charstrings.get(47).unwrap());
        assert_eq!([b'm', b'n', 42, 42, 42], &new_cff.charstrings.get(59).unwrap()[0..5]);
        assert_eq!(70_002, new_cff.charstrings.get(59).unwrap().len());
    }

    #[test]
    fn cff2_patching() {
        let mut patches = cff_u16_glyph_patches();
        patches.write_at("tag", Cff2::TAG);

        let patch = assemble_glyph_keyed_patch(glyph_keyed_patch_header(), patches);
        let patch: &[u8] = &patch;
        let patch = GlyphKeyedPatch::read(FontData::new(patch)).unwrap();
        let patch_info = patch_info(IFT_TAG, 0);

        let cff2_font = FontRef::new(CFF2_FONT).unwrap();
        let mut font_builder = FontBuilder::new();
        font_builder.copy_missing_tables(cff2_font);

        let mut ift_table = format2_with_one_charstrings_offset();
        ift_table.write_at("field_flags", 0b00000010u8);
        ift_table.write_at("charstrings_offset", CFF2_FONT_CHARSTRINGS_OFFSET);
        font_builder.add_raw(Tag::new(b"IFT "), ift_table.data());

        let cff2_font_data = font_builder.build();
        let cff2_font = FontRef::new(cff2_font_data.as_slice()).unwrap();

        let patched =
            apply_glyph_keyed_patches(&[(&patch_info, patch)], &cff2_font, &BuiltInBrotliDecoder)
                .unwrap();
        let patched = FontRef::new(&patched).unwrap();

        let old_cff = CFFAndCharStrings::from_cff2_font(
            &cff2_font,
            CFF2_FONT_CHARSTRINGS_OFFSET,
            GlyphId::new(59),
        )
        .unwrap();
        let new_cff = CFFAndCharStrings::from_cff2_font(
            &patched,
            CFF2_FONT_CHARSTRINGS_OFFSET,
            GlyphId::new(59),
        )
        .unwrap();

        assert_eq!(new_cff.charstrings.off_size(), 2);
        assert_eq!(old_cff.charstrings.count(), new_cff.charstrings.count());

        // Unmodified glyphs
        assert_eq!(old_cff.charstrings.get(0).unwrap(), new_cff.charstrings.get(0).unwrap());
        assert_eq!(old_cff.charstrings.get(2).unwrap(), new_cff.charstrings.get(2).unwrap());
        assert_eq!(old_cff.charstrings.get(34).unwrap(), new_cff.charstrings.get(34).unwrap());
        assert_eq!(old_cff.charstrings.get(37).unwrap(), new_cff.charstrings.get(37).unwrap());
        assert_eq!(old_cff.charstrings.get(39).unwrap(), new_cff.charstrings.get(39).unwrap());

        // Inserted glyphs
        assert_eq!(b"abc", new_cff.charstrings.get(1).unwrap());
        assert_eq!(b"defg", new_cff.charstrings.get(38).unwrap());
        assert_eq!(b"hijkl", new_cff.charstrings.get(47).unwrap());
        assert_eq!(b"mn", new_cff.charstrings.get(59).unwrap());
    }

    #[test]
    fn max_representable_size() {
        assert_eq!(OffsetType::ShortDivByTwo(ShortDivByTwoInfo).max_representable_size(), 131070);
        assert_eq!(OffsetType::Long(LongInfo).max_representable_size(), 4_294_967_295);
        assert_eq!(OffsetType::CffFour(CffFourInfo).max_representable_size(), 4_294_967_294);
    }

    // TODO test of invalid cases:
    // - patch data offsets unordered.
    // - loca offset type switch required.
    // - glyph keyed test with large number of offsets to check type conversion
    //   on (glyphCount * tableCount)
    // - test that glyph keyed patches are idempotent.
}
