//! Loads incremental font transfer <https://w3c.github.io/IFT/Overview.html> patch mappings.
//!
//! The IFT and IFTX tables encode mappings from subset definitions to URL's which host patches
//! that can be applied to the font to add support for the corresponding subset definition.

use std::collections::BTreeSet;
use std::collections::HashMap;
use std::io::Cursor;
use std::io::Read;
use std::ops::RangeInclusive;

use crate::GlyphId;
use crate::Tag;
use raw::tables::ift::EntryFormatFlags;
use raw::types::Offset32;
use raw::types::Uint24;
use raw::{FontData, FontRef};
use read_fonts::{
    tables::ift::{EntryData, EntryMapRecord, Ift, PatchMapFormat1, PatchMapFormat2},
    ReadError, TableProvider,
};

use read_fonts::collections::IntSet;

use crate::charmap::Charmap;

// TODO(garretrieger): implement support for building and compiling mapping tables.

/// Find the set of patches which intersect the specified subset definition.
pub fn intersecting_patches(
    font: &FontRef,
    subset_definition: &SubsetDefinition,
) -> Result<Vec<PatchUri>, ReadError> {
    // TODO(garretrieger): move this function to a struct so we can optionally store
    //  indexes or other data to accelerate intersection.
    let mut result: Vec<PatchUri> = vec![];
    if let Ok(ift) = font.ift() {
        add_intersecting_patches(font, &ift, subset_definition, &mut result)?;
    };
    if let Ok(iftx) = font.iftx() {
        add_intersecting_patches(font, &iftx, subset_definition, &mut result)?;
    };

    Ok(result)
}

fn add_intersecting_patches(
    font: &FontRef,
    ift: &Ift,
    subset_definition: &SubsetDefinition,
    patches: &mut Vec<PatchUri>,
) -> Result<(), ReadError> {
    match ift {
        Ift::Format1(format_1) => add_intersecting_format1_patches(
            font,
            format_1,
            &subset_definition.codepoints,
            &subset_definition.feature_tags,
            patches,
        ),
        Ift::Format2(format_2) => {
            add_intersecting_format2_patches(format_2, subset_definition, patches)
        }
    }
}

fn add_intersecting_format1_patches(
    font: &FontRef,
    map: &PatchMapFormat1,
    codepoints: &IntSet<u32>,
    features: &BTreeSet<Tag>,
    patches: &mut Vec<PatchUri>, // TODO(garretrieger): btree set to allow for de-duping?
) -> Result<(), ReadError> {
    // Step 0: Top Level Field Validation
    let maxp = font.maxp()?;
    if map.glyph_count() != Uint24::new(maxp.num_glyphs() as u32) {
        return Err(ReadError::MalformedData(
            "IFT glyph count must match maxp glyph count.",
        ));
    }

    let max_entry_index = map.max_entry_index();
    let max_glyph_map_entry_index = map.max_glyph_map_entry_index();
    if max_glyph_map_entry_index > max_entry_index {
        return Err(ReadError::MalformedData(
            "max_glyph_map_entry_index() must be >= max_entry_index().",
        ));
    }

    let Ok(uri_template) = map.uri_template_as_string() else {
        return Err(ReadError::MalformedData(
            "Invalid unicode string for the uri_template.",
        ));
    };

    let encoding = PatchEncoding::from_format_number(map.patch_encoding())?;

    // Step 1: Collect the glyph map entries.
    let mut entries = IntSet::<u16>::empty();
    intersect_format1_glyph_map(font, map, codepoints, &mut entries)?;

    // Step 2: Collect feature mappings.
    intersect_format1_feature_map(map, features, &mut entries)?;

    // Step 3: produce final output.
    patches.extend(
        entries
            .iter()
            // Entry 0 is the entry for codepoints already in the font, so it's always considered applied and skipped.
            .filter(|index| *index > 0)
            .filter(|index| !map.is_entry_applied(*index))
            .map(|index| PatchUri::from_index(uri_template, index as u32, encoding)),
    );
    Ok(())
}

fn intersect_format1_glyph_map(
    font: &FontRef,
    map: &PatchMapFormat1,
    codepoints: &IntSet<u32>,
    entries: &mut IntSet<u16>,
) -> Result<(), ReadError> {
    let charmap = Charmap::new(font);
    if codepoints.is_inverted() {
        // TODO(garretrieger): consider invoking this path if codepoints set is above a size threshold
        //                     relative to the fonts cmap.
        let gids = charmap
            .mappings()
            .filter(|(cp, _)| codepoints.contains(*cp))
            .map(|(_, gid)| gid);

        return intersect_format1_glyph_map_inner(map, gids, entries);
    }

    // TODO(garretrieger): since codepoints are looked up in sorted order we may be able to speed up the charmap lookup
    // (eg. walking the charmap in parallel with the codepoints, or caching the last binary search index)
    let gids = codepoints.iter().flat_map(|cp| charmap.map(cp));
    intersect_format1_glyph_map_inner(map, gids, entries)
}

fn intersect_format1_glyph_map_inner(
    map: &PatchMapFormat1,
    gids: impl Iterator<Item = GlyphId>,
    entries: &mut IntSet<u16>,
) -> Result<(), ReadError> {
    let glyph_map = map.glyph_map()?;
    let first_gid = glyph_map.first_mapped_glyph() as u32;
    let max_glyph_map_entry_index = map.max_glyph_map_entry_index();

    for gid in gids {
        let entry_index = if gid.to_u32() < first_gid {
            0
        } else {
            glyph_map
                .entry_index()
                .get((gid.to_u32() - first_gid) as usize)?
                .get()
        };

        if entry_index > max_glyph_map_entry_index {
            continue;
        }

        entries.insert(entry_index);
    }

    Ok(())
}

fn intersect_format1_feature_map(
    map: &PatchMapFormat1,
    features: &BTreeSet<Tag>,
    entries: &mut IntSet<u16>,
) -> Result<(), ReadError> {
    // TODO(garretrieger): special case features = * (inverted set), will need to change to use a IntSet.
    let Some(feature_map) = map.feature_map() else {
        return Ok(());
    };
    let feature_map = feature_map?;

    let max_entry_index = map.max_entry_index();
    let max_glyph_map_entry_index = map.max_glyph_map_entry_index();
    let field_width = if max_entry_index < 256 { 1u16 } else { 2u16 };

    // We need to check up front there is enough data for all of the listed entry records, this
    // isn't checked by the read_fonts generated code. Specification requires the operation to fail
    // up front if the data is too short.
    if feature_map.entry_records_size(max_entry_index)? > feature_map.entry_map_data().len() {
        return Err(ReadError::OutOfBounds);
    }

    let mut tag_it = features.iter();
    let mut record_it = feature_map.feature_records().iter();

    let mut next_tag = tag_it.next();
    let mut next_record = record_it.next();
    let mut cumulative_entry_map_count = 0;
    let mut largest_tag: Option<Tag> = None;
    loop {
        let Some((tag, record)) = next_tag.zip(next_record.clone()) else {
            break;
        };
        let record = record?;

        if *tag > record.feature_tag() {
            cumulative_entry_map_count += record.entry_map_count().get();
            next_record = record_it.next();
            continue;
        }

        if let Some(largest_tag) = largest_tag {
            if *tag <= largest_tag {
                // Out of order or duplicate tag, skip this record.
                next_tag = tag_it.next();
                continue;
            }
        }

        largest_tag = Some(*tag);

        let entry_count = record.entry_map_count().get();
        if *tag < record.feature_tag() {
            next_tag = tag_it.next();
            continue;
        }

        for i in 0..entry_count {
            let index = i + cumulative_entry_map_count;
            let byte_index = (index * field_width * 2) as usize;
            let data = FontData::new(&feature_map.entry_map_data()[byte_index..]);
            let mapped_entry_index = record.first_new_entry_index().get() + i;
            let record = EntryMapRecord::read(data, max_entry_index)?;
            let first = record.first_entry_index().get();
            let last = record.last_entry_index().get();
            if first > last
                || first > max_glyph_map_entry_index
                || last > max_glyph_map_entry_index
                || mapped_entry_index <= max_glyph_map_entry_index
                || mapped_entry_index > max_entry_index
            {
                // Invalid, continue on
                continue;
            }

            if entries.intersects_range(first..=last) {
                entries.insert(mapped_entry_index);
            }
        }
        next_tag = tag_it.next();
    }

    Ok(())
}

fn add_intersecting_format2_patches(
    map: &PatchMapFormat2,
    subset_definition: &SubsetDefinition,
    patches: &mut Vec<PatchUri>, // TODO(garretrieger): btree set to allow for de-duping?
) -> Result<(), ReadError> {
    let entries = decode_format2_entries(map)?;

    for e in entries {
        if e.ignored {
            continue;
        }

        if !e.intersects(subset_definition) {
            continue;
        }

        patches.push(e.uri)
    }

    Ok(())
}

fn decode_format2_entries(map: &PatchMapFormat2) -> Result<Vec<Entry>, ReadError> {
    let compat_id = map.get_compatibility_id();
    let uri_template = map.uri_template_as_string()?;
    let entries_data = map.entries()?.entry_data();
    let default_encoding = PatchEncoding::from_format_number(map.default_patch_encoding())?;

    let mut entry_count = map.entry_count().to_u32();
    let mut entries_data = FontData::new(entries_data);
    let mut entries: Vec<Entry> = vec![];

    let mut id_string_data = map
        .entry_id_string_data()
        .transpose()?
        .map(|table| table.id_data())
        .map(Cursor::new);
    while entry_count > 0 {
        entries_data = decode_format2_entry(
            entries_data,
            &compat_id,
            uri_template,
            &default_encoding,
            &mut id_string_data,
            &mut entries,
        )?;
        entry_count -= 1;
    }

    Ok(entries)
}

fn decode_format2_entry<'a>(
    data: FontData<'a>,
    compat_id: &[u32; 4],
    uri_template: &str,
    default_encoding: &PatchEncoding,
    id_string_data: &mut Option<Cursor<&[u8]>>,
    entries: &mut Vec<Entry>,
) -> Result<FontData<'a>, ReadError> {
    let entry_data = EntryData::read(
        data,
        Offset32::new(if id_string_data.is_none() { 0 } else { 1 }),
    )?;
    let mut entry = Entry::new(uri_template, compat_id, default_encoding);

    // Features
    if let Some(features) = entry_data.feature_tags() {
        entry
            .subset_definition
            .feature_tags
            .extend(features.iter().map(|t| t.get()));
    }

    // Design space
    if let Some(design_space_segments) = entry_data.design_space_segments() {
        for dss in design_space_segments {
            if dss.start() > dss.end() {
                return Err(ReadError::MalformedData(
                    "Design space segment start > end.",
                ));
            }
            entry
                .subset_definition
                .design_space
                .entry(dss.axis_tag())
                .or_default()
                .push(dss.start().to_f64()..=dss.end().to_f64());
        }
    }

    // Copy Indices
    if let Some(copy_indices) = entry_data.copy_indices() {
        for index in copy_indices {
            let entry_to_copy =
                entries
                    .get(index.get().to_u32() as usize)
                    .ok_or(ReadError::MalformedData(
                        "copy index can only refer to a previous entry.",
                    ))?;
            entry.union(entry_to_copy);
        }
    }

    // Entry ID
    entry.uri.id = format2_new_entry_id(&entry_data, entries.last(), id_string_data)?;

    // Encoding
    if let Some(patch_encoding) = entry_data.patch_encoding() {
        entry.uri.encoding = PatchEncoding::from_format_number(patch_encoding)?;
    }

    // Codepoints
    let (codepoints, remaining_data) = decode_format2_codepoints(&entry_data)?;
    if entry.subset_definition.codepoints.is_empty() {
        // as an optimization move the existing set instead of copying it in if possible.
        entry.subset_definition.codepoints = codepoints;
    } else {
        entry.subset_definition.codepoints.union(&codepoints);
    }

    // Ignored
    entry.ignored = entry_data
        .format_flags()
        .contains(EntryFormatFlags::IGNORED);

    entries.push(entry);
    Ok(FontData::new(remaining_data))
}

fn format2_new_entry_id(
    entry_data: &EntryData,
    last_entry: Option<&Entry>,
    id_string_data: &mut Option<Cursor<&[u8]>>,
) -> Result<PatchId, ReadError> {
    let Some(id_string_data) = id_string_data else {
        let last_entry_index = last_entry
            .and_then(|e| match e.uri.id {
                PatchId::Numeric(index) => Some(index),
                _ => None,
            })
            .unwrap_or(0);
        return Ok(PatchId::Numeric(compute_format2_new_entry_index(
            entry_data,
            last_entry_index,
        )?));
    };

    let Some(id_string_length) = entry_data.entry_id_delta().map(|v| v.into_inner()) else {
        let last_id_string = last_entry
            .and_then(|e| match &e.uri.id {
                PatchId::String(id_string) => Some(id_string.clone()),
                _ => None,
            })
            .unwrap_or_default();
        return Ok(PatchId::String(last_id_string));
    };

    let mut id_string: Vec<u8> = vec![0; id_string_length as usize];
    id_string_data
        .read_exact(id_string.as_mut_slice())
        .map_err(|_| ReadError::MalformedData("ID string is out of bounds."))?;
    Ok(PatchId::String(id_string))
}

fn compute_format2_new_entry_index(
    entry_data: &EntryData,
    last_entry_index: u32,
) -> Result<u32, ReadError> {
    let new_index = (last_entry_index as i64)
        + 1
        + entry_data
            .entry_id_delta()
            .map(|v| v.into_inner() as i64)
            .unwrap_or(0);

    if new_index.is_negative() {
        return Err(ReadError::MalformedData("Negative entry id encountered."));
    }

    u32::try_from(new_index).map_err(|_| {
        ReadError::MalformedData("Entry index exceeded maximum size (unsigned 32 bit).")
    })
}

fn decode_format2_codepoints<'a>(
    entry_data: &EntryData<'a>,
) -> Result<(IntSet<u32>, &'a [u8]), ReadError> {
    let format = entry_data
        .format_flags()
        .intersection(EntryFormatFlags::CODEPOINTS_BIT_1 | EntryFormatFlags::CODEPOINTS_BIT_2);

    let codepoint_data = entry_data.codepoint_data();

    if format.bits() == 0 {
        return Ok((IntSet::<u32>::empty(), codepoint_data));
    }

    // See: https://w3c.github.io/IFT/Overview.html#abstract-opdef-interpret-format-2-patch-map-entry
    // for interpretation of codepoint bit balues.
    let codepoint_data = FontData::new(codepoint_data);
    let (bias, skipped) = if format == EntryFormatFlags::CODEPOINTS_BIT_2 {
        (codepoint_data.read_at::<u16>(0)? as u32, 2)
    } else if format == (EntryFormatFlags::CODEPOINTS_BIT_1 | EntryFormatFlags::CODEPOINTS_BIT_2) {
        (codepoint_data.read_at::<Uint24>(0)?.to_u32(), 3)
    } else {
        (0, 0)
    };

    let Some(codepoint_data) = codepoint_data.split_off(skipped) else {
        return Err(ReadError::MalformedData("Codepoints data is too short."));
    };

    let (set, remaining_data) =
        IntSet::<u32>::from_sparse_bit_set_bounded(codepoint_data.as_bytes(), bias, 0x10FFFF)
            .map_err(|_| {
                ReadError::MalformedData("Failed to decode sparse bit set data stream.")
            })?;

    Ok((set, remaining_data))
}

/// Models the encoding type for a incremental font transfer patch.
/// See: <https://w3c.github.io/IFT/Overview.html#font-patch-formats-summary>
#[derive(Clone, Eq, PartialEq, Debug, Hash, Copy)]
pub enum PatchEncoding {
    Brotli,
    PerTableBrotli { fully_invalidating: bool },
    GlyphKeyed,
}

impl PatchEncoding {
    fn from_format_number(format: u8) -> Result<Self, ReadError> {
        // Based on https://w3c.github.io/IFT/Overview.html#font-patch-formats-summary
        match format {
            1 => Ok(Self::Brotli),
            2 => Ok(Self::PerTableBrotli {
                fully_invalidating: true,
            }),
            3 => Ok(Self::PerTableBrotli {
                fully_invalidating: false,
            }),
            4 => Ok(Self::GlyphKeyed),
            _ => Err(ReadError::MalformedData("Invalid encoding format number.")),
        }
    }
}

#[derive(Debug, Clone, Eq, PartialEq, Hash)]
enum PatchId {
    Numeric(u32),
    String(Vec<u8>), // TODO(garretrieger): Make this a reference?
}

/// Stores the information needed to create the URI which points to and incremental font transfer patch.
///
/// Stores a template and the arguments used to instantiate it. See:
/// <https://w3c.github.io/IFT/Overview.html#uri-templates> for details on the template format.
///
/// The input to the template expansion can be either a numeric index or a string id. Currently only
/// the numeric index is supported.
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct PatchUri {
    template: String, // TODO(garretrieger): Make this a reference?
    id: PatchId,
    encoding: PatchEncoding,
}

impl PatchUri {
    fn from_index(uri_template: &str, entry_index: u32, encoding: PatchEncoding) -> PatchUri {
        PatchUri {
            template: uri_template.to_string(),
            id: PatchId::Numeric(entry_index),
            encoding,
        }
    }
}

/// Stores a description of a font subset over codepoints, feature tags, and design space.
#[derive(Debug, Clone, PartialEq)]
pub struct SubsetDefinition {
    codepoints: IntSet<u32>,
    feature_tags: BTreeSet<Tag>,
    design_space: HashMap<Tag, Vec<RangeInclusive<f64>>>,
}

impl SubsetDefinition {
    pub fn new(
        codepoints: IntSet<u32>,
        feature_tags: BTreeSet<Tag>,
        design_space: HashMap<Tag, Vec<RangeInclusive<f64>>>,
    ) -> SubsetDefinition {
        SubsetDefinition {
            codepoints,
            feature_tags,
            design_space,
        }
    }

    fn union(&mut self, other: &SubsetDefinition) {
        self.codepoints.union(&other.codepoints);
        other.feature_tags.iter().for_each(|t| {
            self.feature_tags.insert(*t);
        });
        for (tag, segments) in other.design_space.iter() {
            self.design_space
                .entry(*tag)
                .or_default()
                .extend(segments.clone());
        }
    }
}

/// Stores a materialized version of an IFT patchmap entry.
///
/// See: <https://w3c.github.io/IFT/Overview.html#patch-map-dfn>
#[derive(Debug, Clone, PartialEq)]
struct Entry {
    // Key
    subset_definition: SubsetDefinition,
    ignored: bool,

    // Value
    uri: PatchUri,
    compatibility_id: [u32; 4], // TODO(garretrieger): Make this a reference?
}

impl Entry {
    fn new(template: &str, compat_id: &[u32; 4], default_encoding: &PatchEncoding) -> Entry {
        Entry {
            subset_definition: SubsetDefinition {
                codepoints: IntSet::empty(),
                feature_tags: BTreeSet::new(),
                design_space: HashMap::new(),
            },
            ignored: false,

            uri: PatchUri::from_index(template, 0, *default_encoding),
            compatibility_id: *compat_id,
        }
    }

    fn intersects(&self, subset_definition: &SubsetDefinition) -> bool {
        // Intersection defined here: https://w3c.github.io/IFT/Overview.html#abstract-opdef-check-entry-intersection
        let codepoints_intersects = self.subset_definition.codepoints.is_empty()
            || self
                .subset_definition
                .codepoints
                .intersects_set(&subset_definition.codepoints);
        let features_intersects = self.subset_definition.feature_tags.is_empty()
            || self
                .subset_definition
                .feature_tags
                .intersection(&subset_definition.feature_tags)
                .next()
                .is_some();

        let design_space_intersects = self.subset_definition.design_space.is_empty()
            || self.design_space_intersects(&subset_definition.design_space);

        codepoints_intersects && features_intersects && design_space_intersects
    }

    fn design_space_intersects(
        &self,
        design_space: &HashMap<Tag, Vec<RangeInclusive<f64>>>,
    ) -> bool {
        for (tag, input_segments) in design_space {
            let Some(entry_segments) = self.subset_definition.design_space.get(tag) else {
                continue;
            };

            // TODO(garretrieger): this is inefficient (O(n^2)). If we keep the ranges sorted by start then
            //                     this can be implemented much more efficiently.
            for a in input_segments {
                for b in entry_segments {
                    if a.start() <= b.end() && b.start() <= a.end() {
                        return true;
                    }
                }
            }
        }

        false
    }

    /// Union in the subset definition (codepoints, features, and design space segments)
    /// from other.
    fn union(&mut self, other: &Entry) {
        self.subset_definition.union(&other.subset_definition);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use font_test_data as test_data;
    use font_test_data::ift::{
        codepoints_only_format2, copy_indices_format2, custom_ids_format2, feature_map_format1,
        features_and_design_space_format2, simple_format1, string_ids_format2, u16_entries_format1,
    };
    use raw::types::Int24;
    use read_fonts::tables::ift::{IFTX_TAG, IFT_TAG};
    use read_fonts::FontRef;
    use write_fonts::FontBuilder;

    fn create_ift_font(font: FontRef, ift: Option<&[u8]>, iftx: Option<&[u8]>) -> Vec<u8> {
        let mut builder = FontBuilder::default();

        if let Some(bytes) = ift {
            builder.add_raw(IFT_TAG, bytes);
        }

        if let Some(bytes) = iftx {
            builder.add_raw(IFTX_TAG, bytes);
        }

        builder.copy_missing_tables(font);
        builder.build()
    }

    // Format 1 tests:
    // TODO(garretrieger): test w/ multi codepoints mapping to the same glyph.
    // TODO(garretrieger): test w/ IFT + IFTX both populated tables.
    // TODO(garretrieger): test which has entry that has empty codepoint array.
    // TODO(garretrieger): test with format 1 that has max entry = 0.
    // TODO(garretrieger): font with no maxp.
    // TODO(garretrieger): font with MAXP and maxp.

    // TODO(garretrieger): fuzzer to check consistency vs intersecting "*" subset def.

    fn test_intersection<const M: usize, const N: usize, const O: usize>(
        font: &FontRef,
        codepoints: [u32; M],
        tags: [Tag; N],
        expected_entries: [u32; O],
    ) {
        test_design_space_intersection(font, codepoints, tags, [], expected_entries)
    }

    fn test_design_space_intersection<
        const M: usize,
        const N: usize,
        const O: usize,
        const P: usize,
    >(
        font: &FontRef,
        codepoints: [u32; M],
        tags: [Tag; N],
        design_space: [(Tag, Vec<RangeInclusive<f64>>); O],
        expected_entries: [u32; P],
    ) {
        let patches = intersecting_patches(
            font,
            &SubsetDefinition::new(
                IntSet::from(codepoints),
                BTreeSet::<Tag>::from(tags),
                design_space.into_iter().collect(),
            ),
        )
        .unwrap();

        let expected: Vec<PatchUri> = expected_entries
            .iter()
            .map(|index| PatchUri::from_index("ABCDEFɤ", *index, PatchEncoding::GlyphKeyed))
            .collect();

        assert_eq!(patches, expected);
    }

    fn test_intersection_with_all<const M: usize, const N: usize>(
        font: &FontRef,
        tags: [Tag; M],
        expected_entries: [u32; N],
    ) {
        let patches = intersecting_patches(
            font,
            &SubsetDefinition::new(
                IntSet::<u32>::all(),
                BTreeSet::<Tag>::from(tags),
                HashMap::new(),
            ),
        )
        .unwrap();

        let expected: Vec<PatchUri> = expected_entries
            .iter()
            .map(|index| PatchUri::from_index("ABCDEFɤ", *index, PatchEncoding::GlyphKeyed))
            .collect();

        assert_eq!(expected, patches);
    }

    #[test]
    fn format_1_patch_map_u8_entries() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&simple_format1()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        test_intersection(&font, [], [], []);
        test_intersection(&font, [0x123], [], []); // 0x123 is not in the mapping
        test_intersection(&font, [0x13], [], []); // 0x13 maps to entry 0
        test_intersection(&font, [0x12], [], []); // 0x12 maps to entry 1 which is applied
        test_intersection(&font, [0x11], [], [2]); // 0x11 maps to entry 2
        test_intersection(&font, [0x11, 0x12, 0x123], [], [2]);

        test_intersection_with_all(&font, [], [2]);
    }

    #[test]
    fn format_1_patch_map_bad_entry_index() {
        let mut data = simple_format1();
        data.write_at("entry_index[1]", 3u8);

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        test_intersection(&font, [0x11], [], []);
    }

    #[test]
    fn format_1_patch_map_glyph_map_too_short() {
        let data: &[u8] = &simple_format1();
        let data = &data[..data.len() - 1];

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(
                IntSet::from([0x123]),
                BTreeSet::<Tag>::from([]),
                HashMap::new(),
            ),
        )
        .is_err());
    }

    #[test]
    fn format_1_patch_map_bad_glyph_count() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::CMAP12_FONT1).unwrap(),
            Some(&simple_format1()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(
                IntSet::from([0x123]),
                BTreeSet::<Tag>::from([]),
                HashMap::new(),
            ),
        )
        .is_err());
    }

    #[test]
    fn format_1_patch_map_bad_max_entry() {
        let mut data = simple_format1();
        data.write_at("max_glyph_map_entry_id", 3u16);

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(
                IntSet::from([0x123]),
                BTreeSet::<Tag>::from([]),
                HashMap::new(),
            ),
        )
        .is_err());
    }

    #[test]
    fn format_1_patch_map_bad_uri_template() {
        let mut data = simple_format1();
        data.write_at("uri_template[0]", 0x80u8);
        data.write_at("uri_template[1]", 0x81u8);

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(
                IntSet::from([0x123]),
                BTreeSet::<Tag>::from([]),
                HashMap::new(),
            )
        )
        .is_err());
    }

    #[test]
    fn format_1_patch_map_bad_encoding_number() {
        let mut data = simple_format1();
        data.write_at("patch_encoding", 0x12u8);

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(
                IntSet::from([0x123]),
                BTreeSet::<Tag>::from([]),
                HashMap::new()
            )
        )
        .is_err());
    }

    #[test]
    fn format_1_patch_map_u16_entries() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&u16_entries_format1()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        test_intersection(&font, [], [], []);
        test_intersection(&font, [0x11], [], []);
        test_intersection(&font, [0x12], [], [0x50]);
        test_intersection(&font, [0x13, 0x15], [], [0x51, 0x12c]);

        test_intersection_with_all(&font, [], [0x50, 0x51, 0x12c]);
    }

    #[test]
    fn format_1_patch_map_u16_entries_with_feature_mapping() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&feature_map_format1()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        test_intersection(&font, [], [], []);
        test_intersection(
            &font,
            [],
            [Tag::new(b"liga"), Tag::new(b"dlig"), Tag::new(b"null")],
            [],
        );
        test_intersection(&font, [0x12], [], [0x50]);
        test_intersection(&font, [0x12], [Tag::new(b"liga")], [0x50, 0x180]);
        test_intersection(
            &font,
            [0x13, 0x14],
            [Tag::new(b"liga")],
            [0x51, 0x12c, 0x180, 0x181],
        );
        test_intersection(
            &font,
            [0x13, 0x14],
            [Tag::new(b"dlig")],
            [0x51, 0x12c, 0x190],
        );
        test_intersection(
            &font,
            [0x13, 0x14],
            [Tag::new(b"dlig"), Tag::new(b"liga")],
            [0x51, 0x12c, 0x180, 0x181, 0x190],
        );
        test_intersection(&font, [0x11], [Tag::new(b"null")], [0x12D]);
        test_intersection(&font, [0x15], [Tag::new(b"liga")], [0x181]);

        test_intersection_with_all(&font, [], [0x50, 0x51, 0x12c]);
        test_intersection_with_all(
            &font,
            [Tag::new(b"liga")],
            [0x50, 0x51, 0x12c, 0x180, 0x181],
        );
        test_intersection_with_all(&font, [Tag::new(b"dlig")], [0x50, 0x51, 0x12c, 0x190]);
    }

    #[test]
    fn format_1_patch_map_u16_entries_with_out_of_order_feature_mapping() {
        let mut data = feature_map_format1();
        data.write_at("FeatureRecord[0]", Tag::new(b"liga"));
        data.write_at("FeatureRecord[1]", Tag::new(b"dlig"));

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        test_intersection(
            &font,
            [0x13, 0x14],
            [Tag::new(b"liga")],
            [0x51, 0x12c, 0x190],
        );
        test_intersection(
            &font,
            [0x13, 0x14],
            [Tag::new(b"dlig")],
            [0x51, 0x12c], // dlig is ignored since it's out of order.
        );
        test_intersection(&font, [0x11], [Tag::new(b"null")], [0x12D]);
    }

    #[test]
    fn format_1_patch_map_u16_entries_with_duplicate_feature_mapping() {
        let mut data = feature_map_format1();
        data.write_at("FeatureRecord[0]", Tag::new(b"liga"));
        data.write_at("FeatureRecord[1]", Tag::new(b"liga"));

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        test_intersection(
            &font,
            [0x13, 0x14],
            [Tag::new(b"liga")],
            [0x51, 0x12c, 0x190],
        );
        test_intersection(&font, [0x11], [Tag::new(b"null")], [0x12D]);
    }

    #[test]
    fn format_1_patch_map_feature_map_entry_record_too_short() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&feature_map_format1()[..feature_map_format1().len() - 1]),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(
                IntSet::from([0x12]),
                BTreeSet::<Tag>::from([]),
                HashMap::new(),
            ),
        )
        .is_err());
        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(
                IntSet::from([0x12]),
                BTreeSet::<Tag>::from([Tag::new(b"liga")]),
                HashMap::new(),
            )
        )
        .is_err());
        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(
                IntSet::from([0x12]),
                BTreeSet::<Tag>::from([]),
                HashMap::new(),
            )
        )
        .is_err());
    }

    #[test]
    fn format_1_patch_map_feature_record_too_short() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&feature_map_format1()[..123]),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(
                IntSet::from([0x12]),
                BTreeSet::<Tag>::from([]),
                HashMap::new(),
            ),
        )
        .is_err());
        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(
                IntSet::from([0x12]),
                BTreeSet::<Tag>::from([Tag::new(b"liga")]),
                HashMap::new(),
            )
        )
        .is_err());
        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(
                IntSet::from([0x12]),
                BTreeSet::<Tag>::from([]),
                HashMap::new(),
            )
        )
        .is_err());
    }

    #[test]
    fn format_2_patch_map_codepoints_only() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&codepoints_only_format2()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        test_intersection(&font, [], [], []);
        test_intersection(&font, [0x02], [], [1]);
        test_intersection(&font, [0x15], [], [3]);
        test_intersection(&font, [0x07], [], [1, 3]);
        test_intersection(&font, [80_007], [], [4]);

        test_intersection_with_all(&font, [], [1, 3, 4]);
    }

    #[test]
    fn format_2_patch_map_features_and_design_space() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&features_and_design_space_format2()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        test_intersection(&font, [], [], []);
        test_intersection(&font, [0x02], [], []);
        test_intersection(&font, [0x50], [Tag::new(b"rlig")], []);
        test_intersection(&font, [0x02], [Tag::new(b"rlig")], [2]);

        test_design_space_intersection(
            &font,
            [0x02],
            [Tag::new(b"rlig")],
            [(Tag::new(b"wdth"), vec![0.7..=0.8])],
            [2],
        );

        test_design_space_intersection(
            &font,
            [0x05],
            [Tag::new(b"smcp")],
            [(Tag::new(b"wdth"), vec![0.7..=0.8])],
            [1],
        );
        test_design_space_intersection(
            &font,
            [0x05],
            [Tag::new(b"smcp")],
            [(Tag::new(b"wdth"), vec![0.2..=0.3])],
            [3],
        );
        test_design_space_intersection(
            &font,
            [0x05],
            [Tag::new(b"smcp")],
            [(Tag::new(b"wdth"), vec![1.2..=1.3])],
            [],
        );

        test_design_space_intersection(
            &font,
            [0x05],
            [Tag::new(b"smcp")],
            [(Tag::new(b"wdth"), vec![0.2..=0.3, 0.7..=0.8])],
            [1, 3],
        );
        test_design_space_intersection(
            &font,
            [0x05],
            [Tag::new(b"smcp")],
            [(Tag::new(b"wdth"), vec![2.2..=2.3])],
            [3],
        );
        test_design_space_intersection(
            &font,
            [0x05],
            [Tag::new(b"smcp")],
            [(Tag::new(b"wdth"), vec![2.2..=2.3, 1.2..=1.3])],
            [3],
        );
    }

    #[test]
    fn format_2_patch_map_copy_indices() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&copy_indices_format2()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        test_intersection(&font, [], [], []);
        test_intersection(&font, [0x05], [], [5, 9]);
        test_intersection(&font, [0x65], [], [9]);

        test_design_space_intersection(
            &font,
            [],
            [Tag::new(b"rlig")],
            [(Tag::new(b"wght"), vec![500.0..=500.0])],
            [3, 6],
        );

        test_design_space_intersection(
            &font,
            [0x05],
            [Tag::new(b"rlig")],
            [(Tag::new(b"wght"), vec![500.0..=500.0])],
            [3, 5, 6, 7, 8, 9],
        );
    }

    #[test]
    fn format_2_patch_map_custom_ids() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&custom_ids_format2()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        test_intersection_with_all(&font, [], [0, 6, 15]);
    }

    #[test]
    fn format_2_patch_map_custom_encoding() {
        let mut data = custom_ids_format2();
        data.write_at("entry[4] encoding", 1u8); // Brotli Full Invalidation.

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        let patches = intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), BTreeSet::new(), HashMap::new()),
        )
        .unwrap();

        let encodings: Vec<PatchEncoding> = patches.into_iter().map(|uri| uri.encoding).collect();
        assert_eq!(
            encodings,
            vec![
                PatchEncoding::GlyphKeyed,
                PatchEncoding::GlyphKeyed,
                PatchEncoding::Brotli,
            ]
        );
    }

    #[test]
    fn format_2_patch_map_id_strings() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&string_ids_format2()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        let patches = intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), BTreeSet::new(), HashMap::new()),
        )
        .unwrap();

        let ids: Vec<PatchId> = patches.into_iter().map(|uri| uri.id).collect();
        let expected_ids = vec!["", "abc", "defg", "defg", "hij", ""];
        assert_eq!(
            ids,
            expected_ids
                .into_iter()
                .map(|s| PatchId::String(Vec::from(s)))
                .collect::<Vec<PatchId>>()
        );
    }

    #[test]
    fn format_2_patch_map_id_strings_too_short() {
        let mut data = string_ids_format2();
        data.write_at("entry[4] id length", 4u16);

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), BTreeSet::new(), HashMap::new()),
        )
        .is_err());
    }

    #[test]
    fn format_2_patch_map_invalid_design_space() {
        let mut data = features_and_design_space_format2();
        data.write_at("wdth start", 0x20000u32);

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), BTreeSet::new(), HashMap::new()),
        )
        .is_err());
    }

    #[test]
    fn format_2_patch_map_invalid_sparse_bit_set() {
        let data = codepoints_only_format2();
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data[..(data.len() - 1)]),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), BTreeSet::new(), HashMap::new()),
        )
        .is_err());
    }

    #[test]
    fn format_2_patch_map_negative_entry_id() {
        let mut data = custom_ids_format2();
        data.write_at("id delta", Int24::new(-2));

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), BTreeSet::new(), HashMap::new()),
        )
        .is_err());
    }

    #[test]
    fn format_2_patch_map_negative_entry_id_on_ignored() {
        let mut data = custom_ids_format2();
        data.write_at("id delta - ignored entry", Int24::new(-20));

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), BTreeSet::new(), HashMap::new()),
        )
        .is_err());
    }

    #[test]
    fn format_2_patch_map_entry_id_overflow() {
        let count = 511;
        let mut data = custom_ids_format2();
        data.write_at("entry_count", Uint24::new(count + 5));

        for _ in 0..count {
            data = data
                .push(0b01000100u8) // format = ID_DELTA | IGNORED
                .push(Int24::new(0x7FFFFF)); // delta = max(i24)
        }

        // at this point the second last entry id is:
        // 15 +                   # last entry id from the first 4 entries
        // count * (0x7FFFFF + 1) # sum of added deltas
        //
        // So the max delta without overflow on the last entry is:
        //
        // u32::MAX - second last entry id - 1
        //
        // The -1 is needed because the last entry implicitly includes a + 1
        let max_delta_without_overflow = (u32::MAX - ((15 + count * (0x7FFFFF + 1)) + 1)) as i32;
        data = data
            .push(0b01000100u8) // format = ID_DELTA | IGNORED
            .push_with_tag(Int24::new(max_delta_without_overflow), "last delta"); // delta

        // Check one less than max doesn't overflow
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), BTreeSet::new(), HashMap::new()),
        )
        .is_ok());

        // Check one more does overflow
        data.write_at("last delta", Int24::new(max_delta_without_overflow + 1));

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), BTreeSet::new(), HashMap::new()),
        )
        .is_err());
    }
}
