//! Loads incremental font transfer <https://w3c.github.io/IFT/Overview.html> patch mappings.
//!
//! The IFT and IFTX tables encode mappings from subset definitions to URL's which host patches
//! that can be applied to the font to add support for the corresponding subset definition.

use std::cmp::Ordering;
use std::collections::BTreeMap;
use std::collections::BTreeSet;
use std::collections::HashMap;
use std::io::Cursor;
use std::io::Read;

use font_types::Fixed;
use font_types::Int24;
use font_types::Tag;

use read_fonts::{
    collections::{IntSet, RangeSet},
    tables::ift::{CompatibilityId, EntryData, EntryFormatFlags, IftPatchMap, IFTX_TAG, IFT_TAG},
    types::Uint24,
    FontData, FontRead, FontRef, ReadError, TableProvider,
};

use crate::short_string::ShortString;
use crate::url_templates;
use crate::url_templates::UrlTemplateError;

// TODO(garretrieger): implement support for building and compiling mapping tables.

/// Find the set of patches which intersect the specified subset definition.
pub fn intersecting_patches(
    font: &FontRef,
    subset_definition: &SubsetDefinition,
) -> Result<Vec<PatchMapEntry>, ReadError> {
    // TODO(garretrieger): move this function to a struct so we can optionally store
    //  indexes or other data to accelerate intersection.
    let mut result: Vec<PatchMapEntry> = vec![];

    for (tag, table) in IftTableTag::tables_in(font)? {
        add_intersecting_patches(&tag, &table, subset_definition, &mut result)?;
    }

    Ok(result)
}

#[derive(Clone, Default)]
struct EntryCaches {
    // If the entry intersects or `None` if it has not been computing.
    intersects: Option<bool>,
    // The coverage for the intersection or `None` if there is no subset.
    coverage: Option<SubsetDefinition>,
}

struct EntryIntersectionCache<'a> {
    target_subset_definition: &'a SubsetDefinition,
    // The entries that we are inspecting.
    entries: &'a [Entry],
    // Storage for cached values for entries. Initialized to have the same length as `entries`.
    cache: Vec<EntryCaches>,
}

impl<'a> EntryIntersectionCache<'a> {
    fn new(
        entries: &'a [Entry],
        target_subset_definition: &'a SubsetDefinition,
    ) -> EntryIntersectionCache<'a> {
        EntryIntersectionCache {
            target_subset_definition,
            entries,
            cache: vec![EntryCaches::default(); entries.len()],
        }
    }

    /// Returns true if the target_subset_definition intersects the entry at index.
    ///
    /// # TODO
    ///
    /// Determine what to do when `index` is invalid. This should not happen in well-formed fonts,
    /// but `compute_intersection` may discover a child index that is out of bounds.
    fn intersects(&mut self, index: usize) -> bool {
        if let Some(EntryCaches {
            intersects: Some(result),
            ..
        }) = self.cache.get(index)
        {
            return *result;
        }

        let Some(entry) = self.entries.get(index) else {
            return false;
        };

        let result = self.compute_intersection(entry);
        if let Some(cache) = self.cache.get_mut(index) {
            cache.intersects = Some(result);
        }
        result
    }

    fn compute_intersection(&mut self, entry: &Entry) -> bool {
        // See: https://w3c.github.io/IFT/Overview.html#abstract-opdef-check-entry-intersection
        if !entry.intersects(self.target_subset_definition) {
            return false;
        }

        if entry.child_indices.is_empty() {
            return true;
        }

        if entry.conjunctive_child_match {
            self.all_children_intersect(entry)
        } else {
            self.some_children_intersect(entry)
        }
    }

    fn all_children_intersect(&mut self, entry: &Entry) -> bool {
        for child_index in entry.child_indices.iter() {
            if !self.intersects(*child_index) {
                return false;
            }
        }
        true
    }

    fn some_children_intersect(&mut self, entry: &Entry) -> bool {
        for child_index in entry.child_indices.iter() {
            if self.intersects(*child_index) {
                return true;
            }
        }
        false
    }

    /// Returns the intersection of target_subset_definition and the union of entry and all of its descendants.
    fn coverage_intersection(&mut self, index: usize) -> Result<&SubsetDefinition, ReadError> {
        Self::coverage_intersection_impl(
            self.entries,
            self.target_subset_definition,
            index,
            &mut self.cache,
        )
        .ok_or(ReadError::OutOfBounds)
    }

    /// Returns None if `index` is out of range.
    fn coverage_intersection_impl<'b>(
        entries: &[Entry],
        target_subset_definition: &SubsetDefinition,
        index: usize,
        cache: &'b mut [EntryCaches],
    ) -> Option<&'b SubsetDefinition> {
        if cache.get(index)?.coverage.is_some() {
            return cache[index].coverage.as_ref();
        }

        let entry = entries.get(index)?;

        let mut self_intersection = entry
            .subset_definition
            .intersection(target_subset_definition);
        for child_index in entry.child_indices.iter() {
            let subset = Self::coverage_intersection_impl(
                entries,
                target_subset_definition,
                *child_index,
                cache,
            )?;
            self_intersection.union(subset);
        }

        Some(cache.get_mut(index)?.coverage.insert(self_intersection))
    }
}

fn add_intersecting_patches(
    source_table: &IftTableTag,
    map: &IftPatchMap,
    subset_definition: &SubsetDefinition,
    patches: &mut Vec<PatchMapEntry>,
) -> Result<(), ReadError> {
    let entries = decode_entries(map)?;

    // Caches the result of intersection check for an entry index.
    let mut entry_intersection_cache = EntryIntersectionCache::new(&entries, subset_definition);

    let mut application_bit_indices: HashMap<PatchUrl, IntSet<u32>> = Default::default();
    let new_patches_first_index = patches.len();

    for (order, e) in entries.iter().enumerate() {
        if e.ignored {
            continue;
        }

        if !entry_intersection_cache.intersects(order) {
            continue;
        }

        let mut it = e.urls.iter();
        let Some(first_url) = it.next() else {
            continue;
        };

        // for invalidating keyed patches we need to record information about
        // intersection size to use later for patch selection. Only the first
        // url in an entry needs to be updated because only the first url is
        // used for selection.
        let intersection_info = if e.format.is_invalidating() {
            let subset = entry_intersection_cache.coverage_intersection(order)?;
            IntersectionInfo::from_subset(subset, order)
        } else {
            // non-invalidating entries still require information on entry order so just record that.
            IntersectionInfo::from_order(order)
        };
        let preload_urls: Vec<PatchUrl> = it.cloned().collect();
        patches.push(first_url.clone().into_entry(
            preload_urls,
            source_table.clone(),
            e.format,
            intersection_info,
        ));

        application_bit_indices
            .entry(first_url.clone())
            .or_default()
            .insert(e.application_flag_bit_index);
    }

    // In format 2 there may be non intersected entries that have urls
    // that are the same as other intersected entries. We need to record
    // the application bit indices for these
    //
    // So reloop through all decoded entries and collect the indices for
    // any which match an intersected entry.
    for e in entries.iter().filter(|e| !e.ignored) {
        let Some(first_url) = e.urls.first() else {
            continue;
        };

        if let Some(indices) = application_bit_indices.get_mut(first_url) {
            indices.insert(e.application_flag_bit_index);
        }
    }

    // Lastly copy the aggregated application bit indices back into
    // the individual patch map entries.
    if patches.len() > new_patches_first_index {
        for p in patches[new_patches_first_index..].iter_mut() {
            p.application_bit_indices = application_bit_indices
                .get(&p.url)
                .cloned()
                .unwrap_or_default();
        }
    }

    Ok(())
}

fn decode_entries(map: &IftPatchMap) -> Result<Vec<Entry>, ReadError> {
    let url_template = map.url_template();
    let entries_data = map.entries()?.entry_data();
    let default_encoding = PatchFormat::from_format_number(map.default_patch_format())?;

    let mut entry_count = map.entry_count().to_u32();
    let mut entries_data = FontData::new(entries_data);
    let mut entries: Vec<Entry> = vec![];

    let mut entry_start_byte = map.entries_offset().to_u32() as usize;

    let mut id_string_data = map
        .entry_id_string_data()
        .transpose()?
        .map(|table| table.id_data())
        .map(Cursor::new);

    let mut last_entry_id = if id_string_data.is_none() {
        PatchId::Numeric(0)
    } else {
        PatchId::String(vec![])
    };

    while entry_count > 0 {
        let consumed_bytes;
        // TODO(garretrieger): processing context type ovject to reduce argument passing to decode_entry(...)
        (entries_data, consumed_bytes) = decode_entry(
            entries_data,
            entry_start_byte,
            url_template,
            &default_encoding,
            &mut id_string_data,
            &mut entries,
            &mut last_entry_id,
        )?;
        entry_start_byte += consumed_bytes;
        entry_count -= 1;
    }

    Ok(entries)
}

fn decode_entry<'a>(
    data: FontData<'a>,
    data_start_index: usize,
    url_template: &[u8],
    default_format: &PatchFormat,
    id_string_data: &mut Option<Cursor<&[u8]>>,
    entries: &mut Vec<Entry>,
    last_entry_id: &mut PatchId,
) -> Result<(FontData<'a>, usize), ReadError> {
    let entry_data = EntryData::read(data)?;

    // Record the index of the bit which when set causes this entry to be ignored.
    // See: https://w3c.github.io/IFT/Overview.html#mapping-entry-formatflags
    let mut entry: Entry = Entry::base_entry(*default_format, (data_start_index as u32 * 8) + 6);

    // Features
    if let Some(features) = entry_data.feature_tags() {
        entry
            .subset_definition
            .feature_tags
            .extend(features.iter().map(|t| t.get()));
    }

    // Copy indices
    if let (Some(child_indices), Some(match_mode)) = (
        entry_data.child_indices(),
        entry_data.match_mode_and_count(),
    ) {
        let max_index = entries.len();
        let it = child_indices.iter().map(|v| Into::<usize>::into(v.get()));
        for i in it.clone() {
            if i >= max_index {
                return Err(ReadError::MalformedData(
                    "Child index must refer to only prior entries.",
                ));
            }
        }
        entry.child_indices = it.collect();
        entry.conjunctive_child_match = match_mode.conjunctive_match();
    }

    // Design space
    if let Some(design_space_segments) = entry_data.design_space_segments() {
        let mut ranges = HashMap::<Tag, RangeSet<Fixed>>::new();

        for dss in design_space_segments {
            if dss.start() > dss.end() {
                return Err(ReadError::MalformedData(
                    "Design space segment start > end.",
                ));
            }
            ranges
                .entry(dss.axis_tag())
                .or_default()
                .insert(dss.start()..=dss.end());
        }

        entry.subset_definition.design_space = DesignSpace::Ranges(ranges);
    }

    // Entry ID
    let (entry_deltas, trailing_data) = if id_string_data.is_some() {
        decode_entry_deltas::<true>(entry_data.format_flags(), entry_data.trailing_data())?
    } else {
        decode_entry_deltas::<false>(entry_data.format_flags(), entry_data.trailing_data())?
    };

    // Encoding
    let (patch_format, trailing_data) =
        decode_patch_format(entry_data.format_flags(), trailing_data)?;
    entry.format = patch_format.unwrap_or(*default_format);

    // We now have info information to generate the associated urls.
    entry.populate_urls(url_template, entry_deltas, last_entry_id, id_string_data)?;

    // Codepoints
    let (codepoints, trailing_data) = decode_codepoints(entry_data.format_flags(), trailing_data)?;
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

    let consumed_bytes = entry_data.trailing_data_byte_range().end - trailing_data.len();
    Ok((FontData::new(trailing_data), consumed_bytes))
}

fn new_entry_id(
    delta_or_length: Option<i32>,
    last_id: &PatchId,
    id_string_data: &mut Option<Cursor<&[u8]>>,
) -> Result<PatchId, ReadError> {
    let Some(id_string_data) = id_string_data else {
        let last_entry_index = match last_id {
            PatchId::Numeric(index) => *index,
            PatchId::String(_) => return Err(ReadError::MalformedData("Unexpected string id.")),
        };

        return Ok(PatchId::Numeric(compute_new_entry_index(
            delta_or_length.unwrap_or_default(),
            last_entry_index,
        )?));
    };

    let Some(length) = delta_or_length else {
        // If no length was provided the spec says to copy the previous entries
        // id string.
        let last_id_string = match last_id {
            PatchId::String(id_string) => id_string.clone(),
            PatchId::Numeric(_) => return Err(ReadError::MalformedData("Unexpected numeric id.")),
        };
        return Ok(PatchId::String(last_id_string));
    };

    match length.cmp(&0) {
        Ordering::Equal => return Ok(PatchId::String(Default::default())),
        Ordering::Less => return Err(ReadError::MalformedData("Negative string length.")),
        Ordering::Greater => {}
    };

    let mut id_string: Vec<u8> = vec![0; length as usize];
    id_string_data
        .read_exact(id_string.as_mut_slice())
        .map_err(|_| ReadError::MalformedData("ID string is out of bounds."))?;
    Ok(PatchId::String(id_string))
}

fn compute_new_entry_index(delta: i32, last_entry_index: u32) -> Result<u32, ReadError> {
    let new_index = (last_entry_index as i64) + 1 + (delta as i64);

    if new_index.is_negative() {
        return Err(ReadError::MalformedData("Negative entry id encountered."));
    }

    u32::try_from(new_index).map_err(|_| {
        ReadError::MalformedData("Entry index exceeded maximum size (unsigned 32 bit).")
    })
}

fn decode_patch_format(
    flags: EntryFormatFlags,
    format_data: &[u8],
) -> Result<(Option<PatchFormat>, &[u8]), ReadError> {
    if !flags.contains(EntryFormatFlags::PATCH_FORMAT) {
        return Ok((None, format_data));
    }

    let format_byte = format_data.first().ok_or(ReadError::OutOfBounds)?;

    let patch_format = PatchFormat::from_format_number(*format_byte)?;

    Ok((Some(patch_format), &format_data[1..]))
}

fn decode_entry_deltas<const HAS_STRING_DATA: bool>(
    flags: EntryFormatFlags,
    delta_data: &[u8],
) -> Result<(Vec<i32>, &[u8]), ReadError> {
    if !flags.contains(EntryFormatFlags::ENTRY_ID_DELTA) {
        return Ok((vec![], delta_data));
    }

    let mut result: Vec<i32> = vec![];
    const WIDTH: usize = 3;
    let mut index = 0usize;
    loop {
        let (value, has_more) =
            decode_entry_delta::<HAS_STRING_DATA>(&delta_data[index * WIDTH..])?;
        result.push(value);
        index += 1;

        if !has_more {
            break;
        }
    }

    Ok((result, &delta_data[index * WIDTH..]))
}

fn decode_entry_delta<const HAS_STRING_DATA: bool>(
    delta_data: &[u8],
) -> Result<(i32, bool), ReadError> {
    if HAS_STRING_DATA {
        // For length values the most significant bit signals the presence of
        // another value. The remaining bits are the length value (unsigned).
        let val: Uint24 = FontData::new(delta_data).read_at(0)?;
        let val = val.to_u32();
        let has_more = (val & (1 << 23)) > 0;
        let val = val & !(1 << 23);
        Ok((val as i32, has_more))
    } else {
        // For delta values the least significant bit signals the presence of
        // another value. The delta is computed by dividing the entire value (signed)
        // by 2
        let val: Int24 = FontData::new(delta_data).read_at(0)?;
        let val: i32 = val.to_i32();
        let has_more = (val & 1) > 0;
        let val = val / 2;
        Ok((val, has_more))
    }
}

fn decode_codepoints(
    flags: EntryFormatFlags,
    codepoint_data: &[u8],
) -> Result<(IntSet<u32>, &[u8]), ReadError> {
    let format =
        flags.intersection(EntryFormatFlags::CODEPOINTS_BIT_1 | EntryFormatFlags::CODEPOINTS_BIT_2);

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
pub enum PatchFormat {
    TableKeyed { fully_invalidating: bool },
    GlyphKeyed,
}

impl PatchFormat {
    fn is_invalidating(&self) -> bool {
        matches!(self, PatchFormat::TableKeyed { .. })
    }

    fn from_format_number(format: u8) -> Result<Self, ReadError> {
        // Based on https://w3c.github.io/IFT/Overview.html#font-patch-formats-summary
        match format {
            1 => Ok(Self::TableKeyed {
                fully_invalidating: true,
            }),
            2 => Ok(Self::TableKeyed {
                fully_invalidating: false,
            }),
            3 => Ok(Self::GlyphKeyed),
            _ => Err(ReadError::MalformedData("Invalid format number.")),
        }
    }
}

/// Id for a patch which will be subbed into a URL template. The spec allows integer or string IDs.
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub enum PatchId {
    Numeric(u32),
    String(Vec<u8>), // TODO(garretrieger): Make this a reference?
}

/// List of possible IFT mapping table tags.
#[derive(PartialEq, Eq, Debug, Clone, Hash)]
pub(crate) enum IftTableTag {
    Ift(CompatibilityId),
    Iftx(CompatibilityId),
}

impl IftTableTag {
    pub(crate) fn tables_in<'a>(
        font: &'a FontRef,
    ) -> Result<impl Iterator<Item = (IftTableTag, IftPatchMap<'a>)>, ReadError> {
        let ift = font
            .data_for_tag(IFT_TAG)
            .map(IftPatchMap::read)
            .transpose()
            .and_then(Self::check_format)?
            .map(|t| (IftTableTag::Ift(t.compatibility_id()), t))
            .into_iter();
        let iftx = font
            .data_for_tag(IFTX_TAG)
            .map(IftPatchMap::read)
            .transpose()
            .and_then(Self::check_format)?
            .map(|t| (IftTableTag::Iftx(t.compatibility_id()), t))
            .into_iter();

        Ok(ift.chain(iftx))
    }

    fn check_format(table: Option<IftPatchMap>) -> Result<Option<IftPatchMap>, ReadError> {
        match table {
            Some(table) => {
                if table.format() == 2 {
                    Ok(Some(table))
                } else {
                    Err(ReadError::InvalidFormat(table.format().into()))
                }
            }
            None => Ok(table),
        }
    }

    pub(crate) fn font_compat_id(&self, font: &FontRef) -> Result<CompatibilityId, ReadError> {
        Ok(self.mapping_table(font)?.compatibility_id())
    }

    pub(crate) fn tag(&self) -> Tag {
        match self {
            Self::Ift(_) => IFT_TAG,
            Self::Iftx(_) => IFTX_TAG,
        }
    }

    pub(crate) fn mapping_table<'a>(
        &self,
        font: &'a FontRef,
    ) -> Result<IftPatchMap<'a>, ReadError> {
        font.expect_data_for_tag(self.tag())
            .and_then(FontRead::read)
    }

    pub(crate) fn expected_compat_id(&self) -> &CompatibilityId {
        match self {
            Self::Ift(cid) | Self::Iftx(cid) => cid,
        }
    }
}

/// Stores a collection of URLs associated with each patch mapping entry.
///
/// Each entry has a primary URL which is what is loaded and applied when the entry is selected.
/// Additionally each entry has an optional set of preload URL's which should be preloaded if the
/// entry is selected
#[derive(PartialEq, Eq, Debug, Clone)]
pub struct PatchMapEntry {
    pub(crate) url: PatchUrl,
    pub(crate) preload_urls: Vec<PatchUrl>,
    pub(crate) format: PatchFormat,
    pub(crate) source_table: IftTableTag,
    pub(crate) application_bit_indices: IntSet<u32>,
    pub(crate) intersection_info: IntersectionInfo,
}

impl PatchMapEntry {
    pub fn url(&self) -> &PatchUrl {
        &self.url
    }

    pub fn format(&self) -> PatchFormat {
        self.format
    }

    pub(crate) fn expected_compat_id(&self) -> &CompatibilityId {
        self.source_table.expected_compat_id()
    }
}

/// An expanded PatchUrl string which identifies where a patch is located.
#[derive(Debug, Clone, Eq, PartialEq, Hash, PartialOrd, Ord)]
pub struct PatchUrl(ShortString);

impl PatchUrl {
    /// Creates a new `PatchUrl` from a URL string.
    pub fn new(url: &str) -> PatchUrl {
        PatchUrl(url.into())
    }

    /// Expands a URL template using the given patch ID.
    pub(crate) fn expand_template(
        template_string: &[u8],
        patch_id: &PatchId,
    ) -> Result<Self, UrlTemplateError> {
        let buffer = url_templates::expand_template(template_string, patch_id)?;
        Ok(Self(buffer))
    }

    /// Returns the URL as a string slice.
    pub fn as_str(&self) -> &str {
        self.0.as_str()
    }

    pub(crate) fn into_entry(
        self,
        preload_urls: Vec<PatchUrl>,
        source_table: IftTableTag,
        format: PatchFormat,
        intersection_info: IntersectionInfo,
    ) -> PatchMapEntry {
        PatchMapEntry {
            url: self,
            preload_urls,
            format,
            source_table,
            application_bit_indices: IntSet::<u32>::empty(), // these are populated later on
            intersection_info,
        }
    }
}

impl AsRef<str> for PatchUrl {
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

/// Stores information on the intersection which lead to the selection of this patch.
///
/// Intersection details are used later on to choose a specific patch to apply next.
/// See: <https://w3c.github.io/IFT/Overview.html#invalidating-patch-selection>
#[derive(Debug, Clone, Eq, PartialEq, Hash, Default)]
pub(crate) struct IntersectionInfo {
    intersecting_codepoints: u64,
    intersecting_layout_tags: usize,
    intersecting_design_space: BTreeMap<Tag, Fixed>,
    entry_order: usize,
}

impl PartialOrd for IntersectionInfo {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for IntersectionInfo {
    fn cmp(&self, other: &Self) -> Ordering {
        // See: https://w3c.github.io/IFT/Overview.html#invalidating-patch-selection
        // for information on how these are ordered.
        match self
            .intersecting_codepoints
            .cmp(&other.intersecting_codepoints)
        {
            Ordering::Equal => {}
            ord => return ord,
        }
        match self
            .intersecting_layout_tags
            .cmp(&other.intersecting_layout_tags)
        {
            Ordering::Equal => {}
            ord => return ord,
        }
        match self
            .intersecting_design_space
            .cmp(&other.intersecting_design_space)
        {
            Ordering::Equal => {}
            ord => return ord,
        }

        // We select the largest intersection info, and the spec requires in ties that the lowest entry order
        // is selected. So reverse the ordering of comparing entry_order.
        self.entry_order.cmp(&other.entry_order).reverse()
    }
}

impl IntersectionInfo {
    fn from_subset(value: &SubsetDefinition, order: usize) -> Self {
        Self {
            intersecting_codepoints: value.codepoints.len(),
            intersecting_layout_tags: value.feature_tags.len(),
            intersecting_design_space: Self::design_space_size(&value.design_space),
            entry_order: order,
        }
    }

    fn from_order(entry_order: usize) -> Self {
        Self {
            entry_order,
            ..Default::default()
        }
    }

    fn design_space_size(value: &DesignSpace) -> BTreeMap<Tag, Fixed> {
        match value {
            DesignSpace::All => Default::default(),
            DesignSpace::Ranges(value) => value
                .iter()
                .map(|(tag, ranges)| {
                    let total = ranges
                        .iter()
                        .map(|range| *range.end() - *range.start())
                        .fold(Fixed::ZERO, |acc, x| acc + x);

                    (*tag, total)
                })
                .collect(),
        }
    }

    pub(crate) fn entry_order(&self) -> usize {
        self.entry_order
    }
}

/// Stores a set of features tags, can additionally represent all features.
#[derive(Debug, Clone, PartialEq)]
pub enum FeatureSet {
    Set(BTreeSet<Tag>),
    All,
}

impl Default for FeatureSet {
    fn default() -> Self {
        Self::Set(Default::default())
    }
}

impl FeatureSet {
    fn len(&self) -> usize {
        match self {
            Self::All => usize::MAX,
            Self::Set(set) => set.len(),
        }
    }

    /// Add tag to this feature set.
    ///
    /// Returns true if the tag was newly inserted.
    pub fn insert(&mut self, tag: Tag) -> bool {
        match self {
            FeatureSet::All => false,
            FeatureSet::Set(feature_set) => feature_set.insert(tag),
        }
    }

    pub fn extend<It>(&mut self, tags: It)
    where
        It: Iterator<Item = Tag>,
    {
        match self {
            FeatureSet::All => {}
            FeatureSet::Set(feature_set) => {
                feature_set.extend(tags);
            }
        }
    }
}

/// Stores a collection of ranges across zero or more axes.
#[derive(Debug, Clone, PartialEq)]
pub enum DesignSpace {
    Ranges(HashMap<Tag, RangeSet<Fixed>>),
    All,
}

impl Default for DesignSpace {
    fn default() -> Self {
        Self::Ranges(Default::default())
    }
}

impl DesignSpace {
    fn is_empty(&self) -> bool {
        match self {
            Self::All => false,
            Self::Ranges(ranges) => ranges.is_empty(),
        }
    }
}

/// Stores a description of a font subset over codepoints, feature tags, and design space.
#[derive(Debug, Clone, PartialEq, Default)]
pub struct SubsetDefinition {
    pub codepoints: IntSet<u32>,
    pub feature_tags: FeatureSet,
    pub design_space: DesignSpace,
}

impl SubsetDefinition {
    pub fn new(
        codepoints: IntSet<u32>,
        feature_tags: FeatureSet,
        design_space: DesignSpace,
    ) -> SubsetDefinition {
        SubsetDefinition {
            codepoints,
            feature_tags,
            design_space,
        }
    }

    pub fn codepoints(codepoints: IntSet<u32>) -> SubsetDefinition {
        SubsetDefinition {
            codepoints,
            feature_tags: FeatureSet::Set(Default::default()),
            design_space: Default::default(),
        }
    }

    /// Returns a SubsetDefinition which includes all things.
    pub fn all() -> SubsetDefinition {
        SubsetDefinition {
            codepoints: IntSet::all(),
            feature_tags: FeatureSet::All,
            design_space: DesignSpace::All,
        }
    }

    pub fn union(&mut self, other: &SubsetDefinition) {
        self.codepoints.union(&other.codepoints);

        match &other.feature_tags {
            FeatureSet::All => self.feature_tags = FeatureSet::All,
            FeatureSet::Set(set) => self.feature_tags.extend(set.iter().copied()),
        };

        match (&other.design_space, &mut self.design_space) {
            (_, DesignSpace::All) | (DesignSpace::All, _) => self.design_space = DesignSpace::All,
            (DesignSpace::Ranges(other_ranges), DesignSpace::Ranges(self_ranges)) => {
                for (tag, segments) in other_ranges.iter() {
                    self_ranges.entry(*tag).or_default().extend(segments.iter());
                }
            }
        };
    }

    fn intersection(&self, other: &Self) -> Self {
        let mut result: SubsetDefinition = self.clone();

        result.codepoints.intersect(&other.codepoints);

        match (&self.feature_tags, &other.feature_tags) {
            (FeatureSet::All, FeatureSet::Set(tags)) => {
                result.feature_tags = FeatureSet::Set(tags.clone());
            }
            (FeatureSet::Set(a), FeatureSet::Set(b)) => {
                result.feature_tags = FeatureSet::Set(a.intersection(b).copied().collect());
            }
            // In these cases result already has the correct intersection
            (FeatureSet::All, FeatureSet::All) => {}
            (FeatureSet::Set(_), FeatureSet::All) => {}
        };

        result.design_space = self.design_space_intersection(&other.design_space);

        result
    }

    fn design_space_intersection(&self, other_design_space: &DesignSpace) -> DesignSpace {
        match (&self.design_space, other_design_space) {
            (DesignSpace::All, DesignSpace::All) => DesignSpace::All,
            (DesignSpace::All, DesignSpace::Ranges(ranges)) => DesignSpace::Ranges(ranges.clone()),
            (DesignSpace::Ranges(ranges), DesignSpace::All) => DesignSpace::Ranges(ranges.clone()),
            (DesignSpace::Ranges(self_ranges), DesignSpace::Ranges(other_ranges)) => {
                let mut result: HashMap<Tag, RangeSet<Fixed>> = Default::default();
                for (tag, input_segments) in other_ranges {
                    let Some(entry_segments) = self_ranges.get(tag) else {
                        continue;
                    };

                    let ranges: RangeSet<Fixed> =
                        input_segments.intersection(entry_segments).collect();
                    if !ranges.is_empty() {
                        result.insert(*tag, ranges);
                    }
                }

                DesignSpace::Ranges(result)
            }
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
    child_indices: Vec<usize>,
    conjunctive_child_match: bool,
    ignored: bool,

    // Value
    urls: Vec<PatchUrl>,
    format: PatchFormat,
    application_flag_bit_index: u32,
}

impl Entry {
    fn base_entry(default_format: PatchFormat, application_flag_bit_index: u32) -> Self {
        Entry {
            subset_definition: Default::default(),
            child_indices: vec![],
            conjunctive_child_match: false,
            ignored: false,
            urls: vec![],
            format: default_format,
            application_flag_bit_index,
        }
    }

    fn intersects(&self, subset_definition: &SubsetDefinition) -> bool {
        // Intersection defined here: https://w3c.github.io/IFT/Overview.html#abstract-opdef-check-entry-intersection
        let codepoints_intersects = self.subset_definition.codepoints.is_empty()
            || self
                .subset_definition
                .codepoints
                .intersects_set(&subset_definition.codepoints);
        if !codepoints_intersects {
            return false;
        }

        let features_intersects = match &self.subset_definition.feature_tags {
            FeatureSet::All => subset_definition.feature_tags.len() > 0,
            FeatureSet::Set(set) => match &subset_definition.feature_tags {
                FeatureSet::All => true,
                FeatureSet::Set(other) => {
                    set.is_empty() || set.intersection(other).next().is_some()
                }
            },
        };

        if !features_intersects {
            return false;
        }

        match &self.subset_definition.design_space {
            DesignSpace::All => !subset_definition.design_space.is_empty(),
            DesignSpace::Ranges(entry_ranges) => match &subset_definition.design_space {
                DesignSpace::All => true,
                DesignSpace::Ranges(other_ranges) => {
                    entry_ranges.is_empty()
                        || Self::design_space_intersects(entry_ranges, other_ranges)
                }
            },
        }
    }

    fn populate_urls(
        &mut self,
        url_template: &[u8],
        deltas: Vec<i32>,
        last_id: &mut PatchId,
        id_string_data: &mut Option<Cursor<&[u8]>>,
    ) -> Result<(), ReadError> {
        if deltas.is_empty() {
            let next_id = new_entry_id(None, last_id, id_string_data)?;
            self.urls.push(
                PatchUrl::expand_template(url_template, &next_id).map_err(|_| {
                    ReadError::MalformedData("Failed to expand url template in format 2 table.")
                })?,
            );
            *last_id = next_id;
            return Ok(());
        }

        for delta in deltas {
            let next_id = new_entry_id(Some(delta), last_id, id_string_data)?;
            self.urls.push(
                PatchUrl::expand_template(url_template, &next_id).map_err(|_| {
                    ReadError::MalformedData("Failed to expand url template in format 2 table.")
                })?,
            );
            *last_id = next_id;
        }

        Ok(())
    }

    fn design_space_intersects(
        a: &HashMap<Tag, RangeSet<Fixed>>,
        b: &HashMap<Tag, RangeSet<Fixed>>,
    ) -> bool {
        for (tag, a_segments) in a {
            let Some(b_segments) = b.get(tag) else {
                continue;
            };

            if a_segments.intersection(b_segments).next().is_some() {
                return true;
            }
        }

        false
    }
}

#[cfg(test)]
mod tests {
    use std::str::FromStr;

    use super::*;
    use font_test_data as test_data;
    use font_test_data::ift::{
        child_indices, codepoints_only, custom_ids, features_and_design_space, string_ids,
        string_ids_with_preloads, table_keyed_with_preload_urls, ABSOLUTE_URL_TEMPLATE,
        RELATIVE_URL_TEMPLATE,
    };
    use read_fonts::tables::ift::{IFTX_TAG, IFT_TAG};
    use read_fonts::types::Int24;
    use read_fonts::FontRef;
    use write_fonts::FontBuilder;

    impl FeatureSet {
        fn from<const N: usize>(tags: [Tag; N]) -> FeatureSet {
            FeatureSet::Set(BTreeSet::<Tag>::from(tags))
        }
    }

    impl DesignSpace {
        fn from<const N: usize>(design_space: [(Tag, RangeSet<Fixed>); N]) -> DesignSpace {
            DesignSpace::Ranges(design_space.into_iter().collect())
        }
    }

    impl IntersectionInfo {
        pub(crate) fn new(codepoints: u64, features: usize, order: usize) -> Self {
            IntersectionInfo {
                intersecting_codepoints: codepoints,
                intersecting_layout_tags: features,
                intersecting_design_space: Default::default(),
                entry_order: order,
            }
        }

        pub(crate) fn from_design_space<const N: usize>(
            codepoints: u64,
            features: usize,
            design_space: [(Tag, Fixed); N],
            order: usize,
        ) -> Self {
            IntersectionInfo {
                intersecting_codepoints: codepoints,
                intersecting_layout_tags: features,
                intersecting_design_space: BTreeMap::from(design_space),
                entry_order: order,
            }
        }
    }

    fn compat_id() -> CompatibilityId {
        CompatibilityId::from_u32s([1, 2, 3, 4])
    }

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
    // TODO(garretrieger): test for design space union of SubsetDefinition.
    // TODO(garretrieger): fuzzer to check consistency vs intersecting "*" subset def.

    #[derive(Clone)]
    struct ExpectedEntry {
        indices: Vec<u32>,
        application_bit_index: IntSet<u32>,
        order: usize,
    }

    fn set(value: u32) -> IntSet<u32> {
        let mut set = IntSet::<u32>::empty();
        set.insert(value);
        set
    }

    fn f2(index: u32, entry_start: usize, order: usize) -> ExpectedEntry {
        ExpectedEntry {
            indices: vec![index],
            application_bit_index: set(entry_start as u32 * 8 + 6),
            order,
        }
    }

    fn f2p(indices: Vec<u32>, entry_start: usize, order: usize) -> ExpectedEntry {
        ExpectedEntry {
            indices,
            application_bit_index: set(entry_start as u32 * 8 + 6),
            order,
        }
    }

    fn test_intersection<const M: usize, const N: usize, const O: usize>(
        font: &FontRef,
        codepoints: [u32; M],
        tags: [Tag; N],
        expected_entries: [ExpectedEntry; O],
    ) {
        test_design_space_intersection(
            font,
            codepoints,
            FeatureSet::Set(BTreeSet::<Tag>::from(tags)),
            Default::default(),
            expected_entries,
        )
    }

    fn test_design_space_intersection<const M: usize, const P: usize>(
        font: &FontRef,
        codepoints: [u32; M],
        tags: FeatureSet,
        design_space: DesignSpace,
        expected_entries: [ExpectedEntry; P],
    ) {
        let patches = intersecting_patches(
            font,
            &SubsetDefinition::new(IntSet::from(codepoints), tags, design_space),
        )
        .unwrap();

        let expected: Vec<_> = expected_entries
            .iter()
            .map(
                |ExpectedEntry {
                     indices,
                     application_bit_index,
                     order,
                 }| {
                    let mut it = indices.iter().map(|i| {
                        PatchUrl::expand_template(RELATIVE_URL_TEMPLATE, &PatchId::Numeric(*i))
                            .unwrap()
                    });

                    let mut e = it.next().unwrap().into_entry(
                        it.collect(),
                        IftTableTag::Ift(compat_id()),
                        PatchFormat::GlyphKeyed,
                        IntersectionInfo::from_order(*order),
                    );
                    e.application_bit_indices.union(application_bit_index);
                    e
                },
            )
            .collect();

        assert_eq!(patches, expected);
    }

    fn test_intersection_with_all<const M: usize, const N: usize>(
        font: &FontRef,
        tags: [Tag; M],
        expected_entries: [ExpectedEntry; N],
    ) {
        test_intersection_with_all_and_template(font, tags, RELATIVE_URL_TEMPLATE, expected_entries)
    }

    fn test_intersection_with_all_and_template<const M: usize, const N: usize>(
        font: &FontRef,
        tags: [Tag; M],
        url_template: &[u8],
        expected_entries: [ExpectedEntry; N],
    ) {
        let patches = intersecting_patches(
            font,
            &SubsetDefinition::new(
                IntSet::<u32>::all(),
                FeatureSet::from(tags),
                Default::default(),
            ),
        )
        .unwrap();

        let expected: Vec<_> = expected_entries
            .iter()
            .map(
                |ExpectedEntry {
                     indices,
                     application_bit_index,
                     order,
                 }| {
                    let mut it = indices.iter().map(|i| {
                        PatchUrl::expand_template(url_template, &PatchId::Numeric(*i)).unwrap()
                    });
                    let mut e = it.next().unwrap().into_entry(
                        it.collect(),
                        IftTableTag::Ift(compat_id()),
                        PatchFormat::GlyphKeyed,
                        IntersectionInfo::from_order(*order),
                    );
                    e.application_bit_indices.union(application_bit_index);
                    e
                },
            )
            .collect();

        assert_eq!(expected, patches);
    }

    fn check_url_template_substitution(template: &[u8], value: u32, expected: &str) {
        assert_eq!(
            PatchUrl::expand_template(template, &PatchId::Numeric(value))
                .unwrap()
                .as_ref(),
            expected,
        );
    }

    fn check_string_url_template_substitution(template: &[u8], value: &str, expected: &str) {
        assert_eq!(
            PatchUrl::expand_template(template, &PatchId::String(Vec::from(value.as_bytes())))
                .unwrap()
                .as_ref(),
            expected,
        );
    }

    #[test]
    fn url_template_substitution() {
        // These test cases are used in other tests.

        let foo_bar_id = ABSOLUTE_URL_TEMPLATE;
        let foo_bar_d1_d2_id = b"\x0a//foo.bar/\x81\x01/\x82\x01/\x80";
        let foo_bar_d1_d2_d3_id = b"\x0a//foo.bar/\x81\x01/\x82\x01/\x83\x01/\x80";
        let foo_bar_id64 = b"\x0a//foo.bar/\x85";

        check_url_template_substitution(foo_bar_id, 1, "//foo.bar/04");
        check_url_template_substitution(foo_bar_id, 2, "//foo.bar/08");
        check_url_template_substitution(foo_bar_id, 3, "//foo.bar/0C");
        check_url_template_substitution(foo_bar_id, 4, "//foo.bar/0G");
        check_url_template_substitution(foo_bar_id, 5, "//foo.bar/0K");

        // These test cases are from specification:
        // https://w3c.github.io/IFT/Overview.html#url-templates
        check_url_template_substitution(foo_bar_id, 0, "//foo.bar/00");
        check_url_template_substitution(foo_bar_id, 123, "//foo.bar/FC");
        check_url_template_substitution(foo_bar_d1_d2_id, 478, "//foo.bar/0/F/07F0");
        check_url_template_substitution(foo_bar_d1_d2_d3_id, 123, "//foo.bar/C/F/_/FC");

        check_string_url_template_substitution(foo_bar_d1_d2_d3_id, "baz", "//foo.bar/K/N/G/C9GNK");
        check_string_url_template_substitution(foo_bar_d1_d2_d3_id, "z", "//foo.bar/8/F/_/F8");
        check_string_url_template_substitution(
            foo_bar_d1_d2_d3_id,
            "àbc",
            "//foo.bar/O/O/4/OEG64OO",
        );

        check_url_template_substitution(foo_bar_id64, 0, "//foo.bar/AA%3D%3D");
        check_url_template_substitution(foo_bar_id64, 14_000_000, "//foo.bar/1Z-A");
        check_url_template_substitution(foo_bar_id64, 17_000_000, "//foo.bar/AQNmQA%3D%3D");

        check_string_url_template_substitution(foo_bar_id64, "àbc", "//foo.bar/w6BiYw%3D%3D");
    }

    #[test]
    fn rejects_invalid_format() {
        let mut bad_format = codepoints_only();
        bad_format.write_at("format", 3u8);

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&bad_format),
            Some(&codepoints_only()),
        );
        let font = FontRef::new(&font_bytes).unwrap();
        assert_eq!(
            intersecting_patches(
                &font,
                &SubsetDefinition::new(IntSet::all(), FeatureSet::from([]), Default::default()),
            )
            .unwrap_err(),
            ReadError::InvalidFormat(3)
        );
    }

    fn patch_with_intersection(
        applied_entries_start: usize,
        index: u32,
        intersection_info: IntersectionInfo,
    ) -> PatchMapEntry {
        let url =
            PatchUrl::expand_template(RELATIVE_URL_TEMPLATE, &PatchId::Numeric(index)).unwrap();
        let mut e = url.into_entry(
            vec![],
            IftTableTag::Ift(compat_id()),
            PatchFormat::TableKeyed {
                fully_invalidating: true,
            },
            intersection_info,
        );
        e.application_bit_indices
            .insert(applied_entries_start as u32 + index);
        e
    }

    #[test]
    fn patch_map_codepoints_only() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&codepoints_only()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        let e1 = f2(1, codepoints_only().offset_for("entries[0]"), 0);
        let e3 = f2(3, codepoints_only().offset_for("entries[2]"), 2);
        let e4 = f2(4, codepoints_only().offset_for("entries[3]"), 3);
        test_intersection(&font, [], [], []);
        test_intersection(&font, [0x02], [], [e1.clone()]);
        test_intersection(&font, [0x15], [], [e3.clone()]);
        test_intersection(&font, [0x07], [], [e1.clone(), e3.clone()]);
        test_intersection(&font, [80_007], [], [e4.clone()]);

        test_intersection_with_all(&font, [], [e1.clone(), e3.clone(), e4.clone()]);
    }

    #[test]
    fn patch_map_features_and_design_space() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&features_and_design_space()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        let e1 = f2(1, features_and_design_space().offset_for("entries[0]"), 0);
        let e2 = f2(2, features_and_design_space().offset_for("entries[1]"), 1);
        let e3 = f2(3, features_and_design_space().offset_for("entries[2]"), 2);

        test_intersection(&font, [], [], []);
        test_intersection(&font, [0x02], [], []);
        test_intersection(&font, [0x50], [Tag::new(b"rlig")], []);
        test_intersection(&font, [0x02], [Tag::new(b"rlig")], [e2.clone()]);

        test_design_space_intersection(
            &font,
            [0x02],
            FeatureSet::from([Tag::new(b"rlig")]),
            DesignSpace::from([(
                Tag::new(b"wdth"),
                [Fixed::from_f64(0.7)..=Fixed::from_f64(0.8)]
                    .into_iter()
                    .collect(),
            )]),
            [e2],
        );

        test_design_space_intersection(
            &font,
            [0x05],
            FeatureSet::from([Tag::new(b"smcp")]),
            DesignSpace::from([(
                Tag::new(b"wdth"),
                [Fixed::from_f64(0.7)..=Fixed::from_f64(0.8)]
                    .into_iter()
                    .collect(),
            )]),
            [e1.clone()],
        );
        test_design_space_intersection(
            &font,
            [0x05],
            FeatureSet::from([Tag::new(b"smcp")]),
            DesignSpace::from([(
                Tag::new(b"wdth"),
                [Fixed::from_f64(0.2)..=Fixed::from_f64(0.3)]
                    .into_iter()
                    .collect(),
            )]),
            [e3.clone()],
        );
        test_design_space_intersection(
            &font,
            [0x55],
            FeatureSet::from([Tag::new(b"smcp")]),
            DesignSpace::from([(
                Tag::new(b"wdth"),
                [Fixed::from_f64(0.2)..=Fixed::from_f64(0.3)]
                    .into_iter()
                    .collect(),
            )]),
            [],
        );
        test_design_space_intersection(
            &font,
            [0x05],
            FeatureSet::from([Tag::new(b"smcp")]),
            DesignSpace::from([(
                Tag::new(b"wdth"),
                [Fixed::from_f64(1.2)..=Fixed::from_f64(1.3)]
                    .into_iter()
                    .collect(),
            )]),
            [],
        );

        test_design_space_intersection(
            &font,
            [0x05],
            FeatureSet::from([Tag::new(b"smcp")]),
            DesignSpace::from([(
                Tag::new(b"wdth"),
                [
                    Fixed::from_f64(0.2)..=Fixed::from_f64(0.3),
                    Fixed::from_f64(0.7)..=Fixed::from_f64(0.8),
                ]
                .into_iter()
                .collect(),
            )]),
            [e1.clone(), e3.clone()],
        );
        test_design_space_intersection(
            &font,
            [0x05],
            FeatureSet::from([Tag::new(b"smcp")]),
            DesignSpace::from([(
                Tag::new(b"wdth"),
                [Fixed::from_f64(2.2)..=Fixed::from_f64(2.3)]
                    .into_iter()
                    .collect(),
            )]),
            [e3.clone()],
        );
        test_design_space_intersection(
            &font,
            [0x05],
            FeatureSet::from([Tag::new(b"smcp")]),
            DesignSpace::from([(
                Tag::new(b"wdth"),
                [
                    Fixed::from_f64(2.2)..=Fixed::from_f64(2.3),
                    Fixed::from_f64(1.2)..=Fixed::from_f64(1.3),
                ]
                .into_iter()
                .collect(),
            )]),
            [e3],
        );
    }

    #[test]
    fn patch_map_all_features() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&features_and_design_space()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        let e1 = f2(1, features_and_design_space().offset_for("entries[0]"), 0);
        let e2 = f2(2, features_and_design_space().offset_for("entries[1]"), 1);
        let e3 = f2(3, features_and_design_space().offset_for("entries[2]"), 2);

        test_design_space_intersection(
            &font,
            [0x06],
            FeatureSet::All,
            DesignSpace::from([(
                Tag::new(b"wdth"),
                [Fixed::from_f64(0.7)..=Fixed::from_f64(2.2)]
                    .into_iter()
                    .collect(),
            )]),
            [e1, e2, e3],
        );
    }

    #[test]
    fn patch_map_all_design_space() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&features_and_design_space()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        let e1 = f2(1, features_and_design_space().offset_for("entries[0]"), 0);
        let e2 = f2(2, features_and_design_space().offset_for("entries[1]"), 1);
        let e3 = f2(3, features_and_design_space().offset_for("entries[2]"), 2);

        test_design_space_intersection(
            &font,
            [0x05],
            FeatureSet::from([Tag::new(b"smcp")]),
            DesignSpace::All,
            [e1.clone(), e3.clone()],
        );

        test_design_space_intersection(
            &font,
            [0x05],
            FeatureSet::All,
            DesignSpace::All,
            [e1, e2, e3],
        );
    }

    #[test]
    fn patch_map_with_duplicate_urls() {
        // The mapping is set up to contain multiple entries that have the same url.
        // Checks that application bit indices get correctly recorded.
        let mut buffer = codepoints_only();
        buffer.write_at("entry_count", Uint24::new(5));
        let buffer = buffer
            .push(0b00010100u8) // DELTA | CODEPOINT 1
            .push(Int24::new(-8)) // entry delta -4
            .extend([0b00001101, 0b00000011, 0b00110001u8]); // {0..17}

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&buffer),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        let mut e1 = f2(1, buffer.offset_for("entries[0]"), 0);
        let mut e5 = f2(1, buffer.offset_for("entries[3]") + 7, 4);

        e1.application_bit_index.union(&e5.application_bit_index);
        e5.application_bit_index.union(&e1.application_bit_index);

        test_intersection(&font, [], [], []);
        test_intersection(&font, [0x02], [], [e1, e5]);
    }

    #[test]
    fn patch_map_intersection_info() {
        let mut map = features_and_design_space();
        map.write_at("patch_format", 1u8);

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&map),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        // Case 1
        let patches = intersecting_patches(
            &font,
            &SubsetDefinition::new(
                IntSet::from([10, 15, 22]),
                FeatureSet::from([Tag::new(b"rlig"), Tag::new(b"liga")]),
                Default::default(),
            ),
        )
        .unwrap();
        assert_eq!(
            patches,
            vec![patch_with_intersection(
                map.offset_for("entries[1]") * 8 + 4,
                2,
                IntersectionInfo::new(2, 1, 1),
            ),]
        );

        // Case 2
        let patches = intersecting_patches(
            &font,
            &SubsetDefinition::new(
                IntSet::from([10, 15, 22]),
                FeatureSet::from([Tag::new(b"rlig"), Tag::new(b"liga"), Tag::new(b"smcp")]),
                DesignSpace::from([(
                    Tag::new(b"wght"),
                    [Fixed::from_i32(505)..=Fixed::from_i32(800)]
                        .into_iter()
                        .collect(),
                )]),
            ),
        )
        .unwrap();
        assert_eq!(
            patches,
            vec![
                patch_with_intersection(
                    map.offset_for("entries[1]") * 8 + 4,
                    2,
                    IntersectionInfo::new(2, 1, 1),
                ),
                patch_with_intersection(
                    map.offset_for("entries[2]") * 8 + 3,
                    3,
                    IntersectionInfo::from_design_space(
                        3,
                        1,
                        [(Tag::new(b"wght"), Fixed::from_i32(195))],
                        2
                    ),
                ),
            ]
        );
    }

    #[test]
    fn patch_map_invalid_child_indices() {
        let mut builder = child_indices();
        builder.write_at("entries[6]_child", Uint24::new(6));

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&builder),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert_eq!(
            intersecting_patches(
                &font,
                &SubsetDefinition::new(IntSet::all(), FeatureSet::from([]), Default::default()),
            )
            .unwrap_err(),
            ReadError::MalformedData("Child index must refer to only prior entries.")
        );
    }

    #[test]
    fn patch_map_disjunctive_child_indices() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&child_indices()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        let e3 = f2(3, child_indices().offset_for("entries[2]"), 2);
        let e5 = f2(5, child_indices().offset_for("entries[4]"), 4);
        let e6 = f2(6, child_indices().offset_for("entries[5]"), 5);
        let e7 = f2(7, child_indices().offset_for("entries[6]"), 6);
        let e8 = f2(8, child_indices().offset_for("entries[7]"), 7);
        let e9 = f2(9, child_indices().offset_for("entries[8]"), 8);
        test_intersection(&font, [], [], []);
        test_intersection(&font, [0x05], [], [e5.clone(), e7.clone(), e8.clone()]);
        test_intersection(&font, [0x65], [], []);
        test_intersection(
            &font,
            [0x05, 0x65],
            [],
            [e5.clone(), e7.clone(), e8.clone(), e9],
        );

        test_design_space_intersection(
            &font,
            [],
            FeatureSet::from([Tag::new(b"rlig")]),
            DesignSpace::from([(
                Tag::new(b"wght"),
                [Fixed::from_i32(500)..=Fixed::from_i32(500)]
                    .into_iter()
                    .collect(),
            )]),
            [e3.clone(), e6.clone(), e7.clone(), e8.clone()],
        );

        test_design_space_intersection(
            &font,
            [0x05],
            FeatureSet::from([Tag::new(b"rlig")]),
            DesignSpace::from([(
                Tag::new(b"wght"),
                [Fixed::from_i32(500)..=Fixed::from_i32(500)]
                    .into_iter()
                    .collect(),
            )]),
            [e3, e5, e6, e7, e8],
        );
    }

    #[test]
    fn patch_map_conjunctive_child_indices() {
        let mut builder = child_indices();
        builder.write_at("entries[6]_child_count", 0b10000000u8 | 4u8);

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&builder),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        let e2 = f2(2, child_indices().offset_for("entries[1]"), 1);
        let e3 = f2(3, child_indices().offset_for("entries[2]"), 2);
        let e4 = f2(4, child_indices().offset_for("entries[3]"), 3);
        let e5 = f2(5, child_indices().offset_for("entries[4]"), 4);
        let e6 = f2(6, child_indices().offset_for("entries[5]"), 5);
        let e7 = f2(7, child_indices().offset_for("entries[6]"), 6);
        let e8 = f2(8, child_indices().offset_for("entries[7]"), 7);
        test_intersection(&font, [0x05], [], [e5.clone(), e8.clone()]);
        test_design_space_intersection(
            &font,
            [0x05, 51],
            FeatureSet::from([Tag::new(b"liga"), Tag::new(b"rlig")]),
            DesignSpace::from([(
                Tag::new(b"wght"),
                [
                    Fixed::from_i32(75)..=Fixed::from_i32(75),
                    Fixed::from_i32(500)..=Fixed::from_i32(500),
                ]
                .into_iter()
                .collect(),
            )]),
            [e2, e3, e4, e5, e6, e7, e8],
        );
    }

    #[test]
    fn patch_map_conjunctive_child_indices_intersection_info() {
        let mut builder = child_indices();
        builder.write_at("entries[6]_child_count", 0b10000000u8 | 4u8);
        builder.write_at("encoding", 1u8);

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&builder),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        let patches = intersecting_patches(
            &font,
            &SubsetDefinition::new(
                IntSet::from([6, 51, 22]),
                FeatureSet::from([Tag::new(b"rlig"), Tag::new(b"liga")]),
                DesignSpace::from([(
                    Tag::new(b"wght"),
                    [Fixed::from_i32(75)..=Fixed::from_i32(300)]
                        .into_iter()
                        .collect(),
                )]),
            ),
        )
        .unwrap();

        let e = patches
            .into_iter()
            .find(|p| p.url().as_str() == "foo/0S")
            .unwrap();

        let mut expected_info = IntersectionInfo::new(3, 2, 6);
        expected_info
            .intersecting_design_space
            .insert(Tag::new(b"wght"), Fixed::from_i32(125)); // [75..100] + [200..300]

        assert_eq!(
            e,
            patch_with_intersection(
                builder.offset_for("entries[6]") * 8 + 6 - 7,
                7,
                expected_info,
            ),
        );
    }

    #[test]
    fn patch_map_custom_ids() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&custom_ids()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        let e0 = f2(0, custom_ids().offset_for("entries[0]"), 0);
        let e6 = f2(6, custom_ids().offset_for("entries[1]"), 1);
        let e15 = f2(15, custom_ids().offset_for("entries[3]"), 3);

        test_intersection_with_all(&font, [], [e0, e6, e15]);
    }

    #[test]
    fn patch_map_custom_preload_ids() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&table_keyed_with_preload_urls()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        let e0 = f2(
            1,
            table_keyed_with_preload_urls().offset_for("entries[0]"),
            0,
        );
        let e1 = f2p(
            vec![9, 10, 6],
            table_keyed_with_preload_urls().offset_for("entries[1]"),
            1,
        );
        let e2 = f2p(
            vec![2, 3],
            table_keyed_with_preload_urls().offset_for("entries[2]"),
            2,
        );
        let e3 = f2(
            4,
            table_keyed_with_preload_urls().offset_for("entries[3]"),
            3,
        );

        test_intersection_with_all(&font, [], [e0, e1, e2, e3]);
    }

    #[test]
    fn patch_map_custom_encoding() {
        let mut data = custom_ids();
        data.write_at("entry[4] encoding", 1u8); // Tabled Keyed Full Invalidation.

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        let patches = intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), FeatureSet::from([]), Default::default()),
        )
        .unwrap();

        let encodings: Vec<PatchFormat> = patches.into_iter().map(|e| e.format).collect();
        assert_eq!(
            encodings,
            vec![
                PatchFormat::GlyphKeyed,
                PatchFormat::GlyphKeyed,
                PatchFormat::TableKeyed {
                    fully_invalidating: true,
                },
            ]
        );
    }

    #[test]
    fn patch_map_id_strings() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&string_ids()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        let patches = intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), FeatureSet::from([]), Default::default()),
        )
        .unwrap();

        let urls: Vec<PatchUrl> = patches.into_iter().map(|e| e.url).collect();
        let expected_urls: Vec<_> = ["", "abc", "defg", "defg", "hij", ""]
            .iter()
            .map(|id| PatchId::String(Vec::from(id.as_bytes())))
            .map(|id| PatchUrl::expand_template(RELATIVE_URL_TEMPLATE, &id).unwrap())
            .collect();
        assert_eq!(urls, expected_urls);
    }

    #[test]
    fn patch_map_id_strings_with_preloads() {
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&string_ids_with_preloads()),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        let patches = intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), FeatureSet::from([]), Default::default()),
        )
        .unwrap();

        let urls: Vec<Vec<PatchUrl>> = patches
            .into_iter()
            .map(|e| {
                let mut ids = vec![e.url];
                ids.extend(e.preload_urls.clone());
                ids
            })
            .collect();

        let expected_urls = vec![
            vec![""],
            vec!["abc", "", "defg"],
            vec!["defg"],
            vec!["hij"],
            vec![""],
        ];
        let expected_urls = expected_urls
            .into_iter()
            .map(|group| {
                group
                    .iter()
                    .map(|id| PatchId::String(Vec::from(id.as_bytes())))
                    .map(|id| PatchUrl::expand_template(RELATIVE_URL_TEMPLATE, &id).unwrap())
                    .collect::<Vec<PatchUrl>>()
            })
            .collect::<Vec<Vec<PatchUrl>>>();

        assert_eq!(urls, expected_urls);
    }

    #[test]
    fn patch_map_id_strings_too_short() {
        let mut data = string_ids();
        data.write_at("entry[4] id length", Uint24::new(4));

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), FeatureSet::from([]), Default::default()),
        )
        .is_err());
    }

    #[test]
    fn patch_map_invalid_design_space() {
        let mut data = features_and_design_space();
        data.write_at("wdth start", 0x20000u32);

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), FeatureSet::from([]), Default::default()),
        )
        .is_err());
    }

    #[test]
    fn patch_map_invalid_sparse_bit_set() {
        let data = codepoints_only();
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data[..(data.len() - 1)]),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), FeatureSet::from([]), Default::default()),
        )
        .is_err());
    }

    #[test]
    fn patch_map_negative_entry_id() {
        let mut data = custom_ids();
        data.write_at("entries[1].id_delta", Int24::new(-4));

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), FeatureSet::from([]), Default::default()),
        )
        .is_err());
    }

    #[test]
    fn patch_map_negative_entry_id_on_ignored() {
        let mut data = custom_ids();
        data.write_at("id delta - ignored entry", Int24::new(-20));

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), FeatureSet::from([]), Default::default()),
        )
        .is_err());
    }

    #[test]
    fn patch_map_entry_id_overflow() {
        let count = 1023;
        let mut data = custom_ids();
        data.write_at("entry_count", Uint24::new(count + 5));

        for _ in 0..count {
            data = data
                .push(0b01000100u8) // format = ID_DELTA | IGNORED
                .push(Int24::new(0x7FFFFE)); // delta = max(i24) / 2
        }

        // at this point the second last entry id is:
        // 15 +                   # last entry id from the first 4 entries
        // count * (7FFFFE/2 + 1) # sum of added deltas
        //
        // So the max delta without overflow on the last entry is:
        //
        // u32::MAX - second last entry id - 1
        //
        // The -1 is needed because the last entry implicitly includes a + 1
        let max_delta_without_overflow =
            (u32::MAX - ((15 + count * ((0x7FFFFE / 2) + 1)) + 1)) as i32;
        data = data
            .push(0b01000100u8) // format = ID_DELTA | IGNORED
            .push_with_tag(Int24::new(max_delta_without_overflow * 2), "last delta"); // delta

        // Check one less than max doesn't overflow
        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), FeatureSet::from([]), Default::default()),
        )
        .is_ok());

        // Check one more does overflow
        data.write_at("last delta", Int24::new(max_delta_without_overflow + 2));

        let font_bytes = create_ift_font(
            FontRef::new(test_data::ift::IFT_BASE).unwrap(),
            Some(&data),
            None,
        );
        let font = FontRef::new(&font_bytes).unwrap();

        assert!(intersecting_patches(
            &font,
            &SubsetDefinition::new(IntSet::all(), FeatureSet::from([]), Default::default()),
        )
        .is_err());
    }

    #[test]
    fn intersection_info_ordering() {
        // these are in the correct order
        let v1 = IntersectionInfo::new(5, 9, 1);
        let v2 = IntersectionInfo::new(5, 10, 2);
        let v3 = IntersectionInfo::new(5, 10, 1);
        let v4 = IntersectionInfo::new(6, 1, 10);
        let v5 = IntersectionInfo::new(6, 1, 9);

        assert_eq!(v1.cmp(&v1), Ordering::Equal);

        assert_eq!(v1.cmp(&v2), Ordering::Less);
        assert_eq!(v2.cmp(&v1), Ordering::Greater);

        assert_eq!(v2.cmp(&v3), Ordering::Less);
        assert_eq!(v3.cmp(&v2), Ordering::Greater);

        assert_eq!(v3.cmp(&v4), Ordering::Less);
        assert_eq!(v4.cmp(&v3), Ordering::Greater);

        assert_eq!(v4.cmp(&v5), Ordering::Less);
        assert_eq!(v5.cmp(&v4), Ordering::Greater);

        assert_eq!(v3.cmp(&v5), Ordering::Less);
        assert_eq!(v5.cmp(&v3), Ordering::Greater);
    }

    #[test]
    fn intersection_info_ordering_with_design_space() {
        let aaaa = Tag::new(b"aaaa");
        let bbbb = Tag::new(b"bbbb");

        // these are in the correct order
        let v1 = IntersectionInfo::from_design_space(1, 0, [(aaaa, 4i32.into())], 0);
        let v2 = IntersectionInfo::from_design_space(
            1,
            0,
            [(aaaa, 5i32.into()), (bbbb, 2i32.into())],
            0,
        );
        let v3 = IntersectionInfo::from_design_space(1, 0, [(aaaa, 6i32.into())], 0);
        let v4 = IntersectionInfo::from_design_space(
            1,
            0,
            [(aaaa, 6i32.into()), (bbbb, 2i32.into())],
            0,
        );
        let v5 = IntersectionInfo::from_design_space(1, 0, [(bbbb, 1i32.into())], 0);
        let v6 = IntersectionInfo::from_design_space(2, 0, [(aaaa, 1i32.into())], 0);

        assert_eq!(v1.cmp(&v1), Ordering::Equal);

        assert_eq!(v1.cmp(&v2), Ordering::Less);
        assert_eq!(v2.cmp(&v1), Ordering::Greater);

        assert_eq!(v2.cmp(&v3), Ordering::Less);
        assert_eq!(v3.cmp(&v2), Ordering::Greater);

        assert_eq!(v3.cmp(&v4), Ordering::Less);
        assert_eq!(v4.cmp(&v3), Ordering::Greater);

        assert_eq!(v4.cmp(&v5), Ordering::Less);
        assert_eq!(v5.cmp(&v4), Ordering::Greater);

        assert_eq!(v5.cmp(&v6), Ordering::Less);
        assert_eq!(v6.cmp(&v5), Ordering::Greater);

        assert_eq!(v3.cmp(&v5), Ordering::Less);
        assert_eq!(v5.cmp(&v3), Ordering::Greater);
    }

    #[test]
    fn entry_design_codepoints_intersection() {
        let url = PatchUrl::expand_template(RELATIVE_URL_TEMPLATE, &PatchId::Numeric(0)).unwrap();

        let s1 = SubsetDefinition::codepoints([3, 5, 7].into_iter().collect());
        let s2 = SubsetDefinition::codepoints([13, 15, 17].into_iter().collect());
        let s3 = SubsetDefinition::codepoints([7, 13].into_iter().collect());

        let e1 = Entry {
            subset_definition: s1.clone(),
            child_indices: Default::default(),
            conjunctive_child_match: Default::default(),
            ignored: false,

            urls: vec![url.clone()],
            format: PatchFormat::GlyphKeyed,
            application_flag_bit_index: 0,
        };
        let e2 = Entry {
            subset_definition: Default::default(),
            child_indices: Default::default(),
            conjunctive_child_match: Default::default(),
            ignored: false,

            urls: vec![url.clone()],
            format: PatchFormat::GlyphKeyed,
            application_flag_bit_index: 0,
        };

        assert!(e1.intersects(&s1));
        assert_eq!(s1.intersection(&s1), s1);

        assert!(!e1.intersects(&s2));
        assert_eq!(s1.intersection(&s2), Default::default());

        assert!(e1.intersects(&s3));
        assert_eq!(
            s1.intersection(&s3),
            SubsetDefinition::codepoints([7].into_iter().collect())
        );

        assert!(e2.intersects(&s1));
        assert_eq!(
            SubsetDefinition::default().intersection(&s1),
            Default::default()
        );
    }

    #[test]
    fn entry_design_space_intersection() {
        let url = PatchUrl::expand_template(RELATIVE_URL_TEMPLATE, &PatchId::Numeric(0)).unwrap();

        let s1 = SubsetDefinition::new(
            Default::default(),
            Default::default(),
            DesignSpace::from([
                (
                    Tag::new(b"aaaa"),
                    [Fixed::from_i32(100)..=Fixed::from_i32(200)]
                        .into_iter()
                        .collect(),
                ),
                (
                    Tag::new(b"bbbb"),
                    [
                        Fixed::from_i32(300)..=Fixed::from_i32(600),
                        Fixed::from_i32(700)..=Fixed::from_i32(900),
                    ]
                    .into_iter()
                    .collect(),
                ),
            ]),
        );
        let s2 = SubsetDefinition::new(
            Default::default(),
            Default::default(),
            DesignSpace::from([
                (
                    Tag::new(b"bbbb"),
                    [Fixed::from_i32(100)..=Fixed::from_i32(200)]
                        .into_iter()
                        .collect(),
                ),
                (
                    Tag::new(b"cccc"),
                    [Fixed::from_i32(300)..=Fixed::from_i32(600)]
                        .into_iter()
                        .collect(),
                ),
            ]),
        );
        let s3 = SubsetDefinition::new(
            Default::default(),
            Default::default(),
            DesignSpace::from([
                (
                    Tag::new(b"bbbb"),
                    [Fixed::from_i32(500)..=Fixed::from_i32(800)]
                        .into_iter()
                        .collect(),
                ),
                (
                    Tag::new(b"cccc"),
                    [Fixed::from_i32(300)..=Fixed::from_i32(600)]
                        .into_iter()
                        .collect(),
                ),
            ]),
        );
        let s4 = SubsetDefinition::new(
            Default::default(),
            Default::default(),
            DesignSpace::from([(
                Tag::new(b"bbbb"),
                [
                    Fixed::from_i32(500)..=Fixed::from_i32(600),
                    Fixed::from_i32(700)..=Fixed::from_i32(800),
                ]
                .into_iter()
                .collect(),
            )]),
        );

        let e1 = Entry {
            subset_definition: s1.clone(),
            child_indices: Default::default(),
            conjunctive_child_match: Default::default(),
            ignored: false,

            urls: vec![url.clone()],
            format: PatchFormat::GlyphKeyed,
            application_flag_bit_index: 0,
        };

        let e2 = Entry {
            subset_definition: Default::default(),
            child_indices: Default::default(),
            conjunctive_child_match: Default::default(),
            ignored: false,

            urls: vec![url.clone()],
            format: PatchFormat::GlyphKeyed,
            application_flag_bit_index: 0,
        };

        assert!(e1.intersects(&s1));
        assert_eq!(s1.intersection(&s1), s1.clone());

        assert!(!e1.intersects(&s2));
        assert_eq!(s1.intersection(&s2), Default::default());

        assert!(e1.intersects(&s3));
        assert_eq!(s1.intersection(&s3), s4.clone());

        assert!(e2.intersects(&s1));
        assert_eq!(
            SubsetDefinition::default().intersection(&s1),
            SubsetDefinition::default()
        );
    }

    #[test]
    fn feature_set_extend_insert() {
        let mut features: FeatureSet = Default::default();

        let foo = Tag::from_str("fooo").unwrap();
        let bar = Tag::from_str("baar").unwrap();
        let baz = Tag::from_str("baaz").unwrap();

        features.extend([foo, bar].into_iter());
        features.insert(baz);
        features.insert(foo);

        assert_eq!(features, FeatureSet::Set(BTreeSet::from([foo, bar, baz])));

        let mut features: FeatureSet = FeatureSet::All;

        features.extend([foo, bar].into_iter());
        features.insert(baz);
        features.insert(foo);

        assert_eq!(features, FeatureSet::All);
    }
}
