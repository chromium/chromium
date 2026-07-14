//! API for selecting and applying a group of IFT patches.
//!
//! This provides methods for selecting a maximal group of patches that are
//! compatible with each other and additionally methods for applying that group
//! of patches.

use read_fonts::{
    collections::IntSet, tables::ift::CompatibilityId, FontRef, ReadError, TableProvider,
};
use shared_brotli_patch_decoder::{BuiltInBrotliDecoder, SharedBrotliDecoder};
use std::{
    cmp::Ordering,
    collections::{BTreeMap, BTreeSet, HashMap},
};

use crate::{
    font_patch::{IncrementalFontPatchBase, PatchingError},
    patchmap::{
        intersecting_patches, IftTableTag, IntersectionInfo, PatchFormat, PatchMapEntry, PatchUrl,
        SubsetDefinition,
    },
};

/// A group of patches derived from a single IFT font.
///
/// This is a group which can be applied simultaneously to that font. Patches
/// are initially missing data which must be fetched and supplied to patch
/// application method.
///
/// Also optionally includes a list of patches which are not compatible but have
/// been requested to be preloaded.
#[derive(Clone)]
pub struct PatchGroup<'a> {
    font: FontRef<'a>,
    patches: Option<CompatibleGroup>,

    // These patches aren't compatible, but have been requested as preloads by the
    // patch mapping.
    preload_urls: BTreeSet<PatchUrl>,
}

// We implement Debug manually because FontRef does not implement Debug
// unconditionally. Additionally, FontRef may be too verbose to be helpful.
impl std::fmt::Debug for PatchGroup<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("PatchGroup")
            .field("patches", &self.patches)
            .field("preload_urls", &self.preload_urls)
            .finish_non_exhaustive()
    }
}

enum Selection {
    IftPartial,
    IftxPartial,
    IftNo,
    IftxNo,
    Done,
}

impl Selection {
    fn tag_index(&self) -> usize {
        match self {
            Self::IftPartial | Self::IftNo => 0,
            _ => 1,
        }
    }

    fn next(self) -> Selection {
        match self {
            Self::IftPartial => Self::IftxPartial,
            Self::IftxPartial => Self::IftNo,
            Self::IftNo => Self::IftxNo,
            Self::IftxNo => Self::Done,
            Self::Done => Self::Done,
        }
    }
}

impl PatchGroup<'_> {
    /// Intersect the available and unapplied patches in ift_font against
    /// subset_definition
    ///
    /// patch_data provides any patch data that has been previously loaded,
    /// keyed by patch url. May be empty if no patch data is loaded yet.
    ///
    /// Returns a group of patches which would be applied next.
    pub fn select_next_patches<'b>(
        ift_font: FontRef<'b>,
        patch_data: &HashMap<PatchUrl, UrlStatus>,
        subset_definition: &SubsetDefinition,
    ) -> Result<PatchGroup<'b>, ReadError> {
        let candidates = intersecting_patches(&ift_font, subset_definition)?;
        if candidates.is_empty() {
            return Ok(PatchGroup {
                font: ift_font,
                patches: None,
                preload_urls: Default::default(),
            });
        }

        let ift_compat_id = ift_font.ift().ok().map(|t| t.compatibility_id());
        let iftx_compat_id = ift_font.iftx().ok().map(|t| t.compatibility_id());
        if ift_compat_id == iftx_compat_id {
            // The spec disallows two tables with same compat ids.
            // See: https://w3c.github.io/IFT/Overview.html#extend-font-subset
            return Err(ReadError::ValidationError);
        }

        let (compat_group, preload_urls) = Self::select_next_patches_from_candidates(
            candidates,
            patch_data,
            ift_compat_id,
            iftx_compat_id,
        )?;

        Ok(PatchGroup { font: ift_font, patches: Some(compat_group), preload_urls })
    }

    /// Returns an iterator over URLs in this group.
    pub fn urls(&self) -> impl Iterator<Item = &PatchUrl> {
        self.invalidating_patch_iter()
            .chain(self.non_invalidating_patch_iter())
            .map(|info| &info.url)
            .chain(self.preload_urls.iter())
    }

    /// Returns true if there is at least one url associated with this group.
    pub fn has_urls(&self) -> bool {
        let Some(patches) = &self.patches else {
            return !self.preload_urls.is_empty();
        };
        match patches {
            CompatibleGroup::Full(FullInvalidationPatch(_)) => true,
            CompatibleGroup::Mixed { ift, iftx } => {
                ift.has_urls() || iftx.has_urls() || !self.preload_urls.is_empty()
            }
        }
    }

    pub fn next_invalidating_patch(&self) -> Option<&PatchInfo> {
        self.invalidating_patch_iter().next()
    }

    fn invalidating_patch_iter(&self) -> impl Iterator<Item = &PatchInfo> {
        let full = match &self.patches {
            Some(CompatibleGroup::Full(info)) => Some(&info.0),
            _ => None,
        };

        let partial_1 = match &self.patches {
            Some(CompatibleGroup::Mixed { ift: ScopedGroup::PartialInvalidation(v), iftx: _ }) => {
                Some(&v.0)
            }
            _ => None,
        };

        let partial_2 = match &self.patches {
            Some(CompatibleGroup::Mixed { ift: _, iftx: ScopedGroup::PartialInvalidation(v) }) => {
                Some(&v.0)
            }
            _ => None,
        };

        full.into_iter().chain(partial_1).chain(partial_2)
    }

    fn non_invalidating_patch_iter(&self) -> impl Iterator<Item = &PatchInfo> {
        let ift = match &self.patches {
            Some(CompatibleGroup::Mixed { ift, iftx: _ }) => Some(ift),
            _ => None,
        };
        let iftx = match &self.patches {
            Some(CompatibleGroup::Mixed { ift: _, iftx }) => Some(iftx),
            _ => None,
        };

        let it1 = ift.into_iter().flat_map(|scope| scope.no_invalidation_iter());
        let it2 = iftx.into_iter().flat_map(|scope| scope.no_invalidation_iter());

        it1.chain(it2)
    }

    fn select_next_patches_from_candidates(
        candidates: Vec<PatchMapEntry>,
        patch_data: &HashMap<PatchUrl, UrlStatus>,
        ift_compat_id: Option<CompatibilityId>,
        iftx_compat_id: Option<CompatibilityId>,
    ) -> Result<(CompatibleGroup, BTreeSet<PatchUrl>), ReadError> {
        // Some notes about this implementation:
        // - From candidates we need to form the largest possible group of patches which
        //   follow the selection criteria from: https://w3c.github.io/IFT/Overview.html#extend-font-subset
        //   and won't invalidate each other.
        //
        // - Validation constraints are encoded into the structure of CompatibleGroup so
        //   the task here is to fill up a compatible group appropriately.
        //
        // - When multiple valid choices exist the specification provides a procedure for picking amongst the options:
        //   https://w3c.github.io/IFT/Overview.html#invalidating-patch-selection
        //
        // - During selection we need to ensure that there are no PatchInfo's with
        //   duplicate URLs. The spec doesn't require erroring on this case, and it's
        //   resolved by:
        //   - In the spec algo patches are selected and applied one at a time.
        //   - Further it specifically disallows re-applying the same URL later.
        //   - So therefore we de-dup by retaining the particular instance which has the
        //     highest selection priority.

        // Step 1: sort the candidates into separate lists based on invalidation
        // characteristics.
        let GroupingByInvalidation {
            full_invalidation,
            partial_invalidation_ift,
            partial_invalidation_iftx,
            no_invalidation_ift,
            no_invalidation_iftx,
        } = GroupingByInvalidation::group_patches(
            candidates,
            patch_data,
            ift_compat_id,
            iftx_compat_id,
        );

        let mut combined_preload_urls: BTreeSet<PatchUrl> = Default::default();

        // First check for a full invalidation patch, if one exists it's the only
        // selection possible
        if let Some(patch) = Self::select_invalidating_candidate(full_invalidation) {
            // TODO(garretrieger): use a heuristic to select the best patch
            combined_preload_urls = patch.preload_urls.iter().cloned().collect();
            combined_preload_urls.remove(&patch.patch_info.url);
            return Ok((CompatibleGroup::Full(patch.into()), combined_preload_urls));
        }

        // Otherwise fill in the two possible selections in priority order (as defined
        // by `Selection`):
        // 1. Partial Invalidating IFT
        // 2. Partial Invalidating IFTX
        // 3. Non Invalidating IFT
        // 4. Non Invalidating IFT
        // The selections are stored in scoped_groups
        let mut selection_mode = Selection::IftPartial;
        let mut scoped_groups: [Option<ScopedGroup>; 2] = [None, None];

        let mut partial_invalidation_candidates =
            [partial_invalidation_ift, partial_invalidation_iftx];
        let mut no_invalidation_candidates = [no_invalidation_ift, no_invalidation_iftx];
        let mut selected_urls: BTreeSet<PatchUrl> = Default::default();

        loop {
            let tag_index = selection_mode.tag_index();
            match selection_mode {
                Selection::IftPartial | Selection::IftxPartial => {
                    // Select a partial invalidating candidate if possible
                    let mut candidates: Vec<CandidatePatch> = vec![];
                    std::mem::swap(
                        &mut candidates,
                        &mut partial_invalidation_candidates[tag_index],
                    );

                    scoped_groups[tag_index] = Self::select_invalidating_candidate(
                        candidates
                            .into_iter()
                            .filter(|c| !selected_urls.contains(&c.patch_info.url)),
                    )
                    .map(|patch| {
                        combined_preload_urls.extend(patch.preload_urls.iter().cloned());
                        selected_urls.insert(patch.patch_info.url.clone());
                        ScopedGroup::PartialInvalidation(patch.into())
                    })
                }
                Selection::IftNo | Selection::IftxNo => {
                    if scoped_groups[tag_index].is_none() {
                        let mut candidates: BTreeMap<PatchUrl, CandidateNoInvalidationPatch> =
                            Default::default();
                        std::mem::swap(&mut candidates, &mut no_invalidation_candidates[tag_index]);

                        scoped_groups[tag_index] =
                            Some(ScopedGroup::NoInvalidation(Self::filter_and_extract_preloads(
                                candidates,
                                &mut selected_urls,
                                &mut combined_preload_urls,
                            )))
                    }
                }
                Selection::Done => break,
            };

            selection_mode = selection_mode.next();
        }

        // Remove any url's selected above from the preloads
        combined_preload_urls.retain(|url| !selected_urls.contains(url));

        let [Some(ift), Some(iftx)] = scoped_groups else {
            return Err(ReadError::MalformedData(
                "Failed invariant. Both arms of the mixed compat group should always be filled.",
            ));
        };

        Ok((CompatibleGroup::Mixed { ift, iftx }, combined_preload_urls))
    }

    fn filter_and_extract_preloads(
        candidates: BTreeMap<PatchUrl, CandidateNoInvalidationPatch>,
        previously_selected_urls: &mut BTreeSet<PatchUrl>,
        preloads: &mut BTreeSet<PatchUrl>,
    ) -> BTreeMap<OrderedPatchUrl, NoInvalidationPatch> {
        preloads.extend(
            candidates.values().flat_map(|candidate| candidate.preload_urls.iter().cloned()),
        );
        let filtered: BTreeMap<OrderedPatchUrl, NoInvalidationPatch> = candidates
            .into_iter()
            .filter(|(k, _)| !previously_selected_urls.contains(k))
            .map(|(k, v)| (OrderedPatchUrl(v.entry_order, k), NoInvalidationPatch(v.patch_info)))
            .collect();

        for (url, _) in filtered.iter() {
            previously_selected_urls.insert(url.1.clone());
        }

        filtered
    }

    /// Select an entry from a list of candidate invalidating entries according
    /// to the specs selection criteria.
    ///
    /// Context: <https://w3c.github.io/IFT/Overview.html#invalidating-patch-selection>
    fn select_invalidating_candidate<T>(candidates: T) -> Option<CandidatePatch>
    where
        T: IntoIterator<Item = CandidatePatch>,
    {
        // Note:
        // - As mentioned in the spec we can find at least one entry matching that
        //   criteria by finding an entry with the largest intersection (since that
        //   can't be a strict subset of others).
        // - Intersection size is tracked in intersection info.
        // - Ties are broken by entry order, which is also tracked in intersection info.
        // - So it's sufficient to just find a candidate patch with the largest
        //   intersection info, relying on it's Ord implementation.
        candidates.into_iter().max()
    }

    /// Attempt to apply the next patch (or patches if non-invalidating) listed
    /// in this group.
    ///
    /// Returns the bytes of the updated font.
    pub fn apply_next_patches(
        self,
        patch_data: &mut HashMap<PatchUrl, UrlStatus>,
    ) -> Result<Vec<u8>, PatchingError> {
        self.apply_next_patches_with_decoder(patch_data, &BuiltInBrotliDecoder)
    }

    /// Attempt to apply the next patch (or patches if non-invalidating) listed
    /// in this group.
    ///
    /// Returns the bytes of the updated font.
    pub fn apply_next_patches_with_decoder<D: SharedBrotliDecoder>(
        self,
        patch_data: &mut HashMap<PatchUrl, UrlStatus>,
        brotli_decoder: &D,
    ) -> Result<Vec<u8>, PatchingError> {
        if let Some(patch) = self.next_invalidating_patch() {
            let entry = patch_data.get_mut(&patch.url).ok_or(PatchingError::MissingPatches)?;

            match entry {
                UrlStatus::Pending(patch_data) => {
                    let r = self.font.apply_table_keyed_patch(patch, patch_data, brotli_decoder)?;
                    *entry = UrlStatus::Applied;
                    return Ok(r);
                }
                UrlStatus::Applied => {} /* previously applied urls are ignored according to the
                                          * spec. */
            }
        }

        // No invalidating patches left, so apply any non invalidating ones in one pass.
        // First check if we have all of the needed data.
        let new_font = {
            let mut accumulated_info: Vec<(&PatchInfo, &[u8])> = vec![];
            for info in self.non_invalidating_patch_iter() {
                let data = patch_data.get(&info.url).ok_or(PatchingError::MissingPatches)?;

                match data {
                    UrlStatus::Pending(data) => accumulated_info.push((info, data)),
                    UrlStatus::Applied => {} /* previously applied urls are ignored according to
                                              * the spec. */
                }
            }

            if accumulated_info.is_empty() {
                return Err(PatchingError::EmptyPatchList);
            }

            self.font.apply_glyph_keyed_patches(accumulated_info.into_iter(), brotli_decoder)?
        };

        for info in self.non_invalidating_patch_iter() {
            if let Some(status) = patch_data.get_mut(&info.url) {
                *status = UrlStatus::Applied;
            };
        }

        Ok(new_font)
    }
}

#[derive(Default)]
struct GroupingByInvalidation {
    full_invalidation: Vec<CandidatePatch>,
    partial_invalidation_ift: Vec<CandidatePatch>,
    partial_invalidation_iftx: Vec<CandidatePatch>,
    // TODO(garretrieger): do we need sorted order, use HashMap instead?
    no_invalidation_ift: BTreeMap<PatchUrl, CandidateNoInvalidationPatch>,
    no_invalidation_iftx: BTreeMap<PatchUrl, CandidateNoInvalidationPatch>,
}

impl GroupingByInvalidation {
    fn group_patches(
        candidates: Vec<PatchMapEntry>,
        patch_data: &HashMap<PatchUrl, UrlStatus>,
        ift_compat_id: Option<CompatibilityId>,
        iftx_compat_id: Option<CompatibilityId>,
    ) -> GroupingByInvalidation {
        let mut result: GroupingByInvalidation = Default::default();

        for entry in candidates.into_iter() {
            // TODO(garretrieger): for efficiency can we delay url template resolution until
            // we have actually selected patches? TODO(garretrieger): for btree
            // construction don't recompute the resolved url, cache inside the patch url
            // object?
            match entry.format {
                PatchFormat::TableKeyed { fully_invalidating: true } => {
                    result.full_invalidation.push(CandidatePatch::from_entry(entry, patch_data))
                }
                PatchFormat::TableKeyed { fully_invalidating: false } => {
                    if Some(entry.expected_compat_id()) == ift_compat_id.as_ref() {
                        result
                            .partial_invalidation_ift
                            .push(CandidatePatch::from_entry(entry, patch_data))
                    } else if Some(entry.expected_compat_id()) == iftx_compat_id.as_ref() {
                        result
                            .partial_invalidation_iftx
                            .push(CandidatePatch::from_entry(entry, patch_data))
                    }
                }
                PatchFormat::GlyphKeyed => {
                    let mapping = if Some(entry.expected_compat_id()) == ift_compat_id.as_ref() {
                        Some(&mut result.no_invalidation_ift)
                    } else if Some(entry.expected_compat_id()) == iftx_compat_id.as_ref() {
                        Some(&mut result.no_invalidation_iftx)
                    } else {
                        None
                    };
                    if let Some(mapping) = mapping {
                        mapping
                            .entry(entry.url().clone())
                            .and_modify(|existing| {
                                // When duplicate URLs are present we want to always keep the one
                                // with the lowest entry order.
                                if entry.intersection_info.entry_order() < existing.entry_order {
                                    *existing = entry.clone().into()
                                }
                            })
                            .or_insert_with(|| entry.into());
                    }
                }
            }
        }

        result
    }
}

/// Tracks whether a URL has already been applied to a font or not.
#[derive(PartialEq, Eq, Debug, Clone)]
pub enum UrlStatus {
    Applied,
    Pending(Vec<u8>),
}

/// Tracks information related to a patch necessary to apply that patch.
#[derive(PartialEq, Eq, Debug, Clone)]
pub struct PatchInfo {
    pub(crate) url: PatchUrl,
    pub(crate) source_table: IftTableTag,
    pub(crate) application_flag_bit_indices: IntSet<u32>,
}

impl From<PatchMapEntry> for PatchInfo {
    fn from(value: PatchMapEntry) -> Self {
        PatchInfo {
            url: value.url,
            source_table: value.source_table,
            application_flag_bit_indices: value.application_bit_indices,
        }
    }
}

impl PatchInfo {
    pub(crate) fn tag(&self) -> &IftTableTag {
        &self.source_table
    }

    pub(crate) fn application_flag_bit_indices(&self) -> impl Iterator<Item = u32> + '_ {
        self.application_flag_bit_indices.iter()
    }

    pub fn url(&self) -> &str {
        self.url.as_ref()
    }
}

/// Type to track a patch being considered for selection.
#[derive(PartialEq, Eq)]
struct CandidatePatch {
    intersection_info: IntersectionInfo,
    patch_info: PatchInfo,
    preload_urls: Vec<PatchUrl>,
    already_loaded: bool,
}

struct CandidateNoInvalidationPatch {
    patch_info: PatchInfo,
    entry_order: usize,
    preload_urls: Vec<PatchUrl>,
}

impl CandidatePatch {
    fn from_entry(
        value: PatchMapEntry,
        patch_data: &HashMap<PatchUrl, UrlStatus>,
    ) -> CandidatePatch {
        let patch_info = PatchInfo {
            url: value.url,
            source_table: value.source_table,
            application_flag_bit_indices: value.application_bit_indices,
        };
        let already_loaded = patch_data.contains_key(&patch_info.url);
        Self {
            intersection_info: value.intersection_info,
            patch_info,
            preload_urls: value.preload_urls,
            already_loaded,
        }
    }
}

impl PartialOrd for CandidatePatch {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for CandidatePatch {
    fn cmp(&self, other: &Self) -> Ordering {
        // Ordering is primarily derived from intersection info, but first we need to
        // check if either patch is already loaded. If so the loaded one is
        // prioritized above any that are not already loaded.
        //
        // See: https://w3c.github.io/IFT/Overview.html#invalidating-patch-selection
        match (self.already_loaded, other.already_loaded) {
            (true, false) => Ordering::Greater,
            (false, true) => Ordering::Less,
            _ => self.intersection_info.cmp(&other.intersection_info),
        }
    }
}

impl From<PatchMapEntry> for CandidateNoInvalidationPatch {
    fn from(mut value: PatchMapEntry) -> Self {
        let mut preload_urls: Vec<PatchUrl> = vec![];
        std::mem::swap(&mut preload_urls, &mut value.preload_urls);
        let entry_order = value.intersection_info.entry_order();
        Self { patch_info: value.into(), entry_order, preload_urls }
    }
}

/// Type for a single non invalidating patch.
#[derive(PartialEq, Eq, Debug, Clone)]
struct NoInvalidationPatch(PatchInfo);

/// Type for a single partially invalidating patch.
#[derive(PartialEq, Eq, Debug, Clone)]
struct PartialInvalidationPatch(PatchInfo);

impl From<CandidatePatch> for PartialInvalidationPatch {
    fn from(value: CandidatePatch) -> Self {
        Self(value.patch_info)
    }
}

/// Type for a single fully invalidating patch.
#[derive(PartialEq, Eq, Debug, Clone)]
struct FullInvalidationPatch(PatchInfo);

impl From<CandidatePatch> for FullInvalidationPatch {
    fn from(value: CandidatePatch) -> Self {
        Self(value.patch_info)
    }
}

/// Represents a group of patches which are valid (compatible) to be applied
/// together to an IFT font.
#[derive(PartialEq, Eq, Debug, Clone)]
enum CompatibleGroup {
    Full(FullInvalidationPatch),
    Mixed { ift: ScopedGroup, iftx: ScopedGroup },
}

/// A set of zero or more compatible patches that are derived from the same
/// scope ("IFT " vs "IFTX")
#[derive(PartialEq, Eq, Debug, Clone)]
enum ScopedGroup {
    PartialInvalidation(PartialInvalidationPatch),
    NoInvalidation(BTreeMap<OrderedPatchUrl, NoInvalidationPatch>),
}

impl ScopedGroup {
    fn has_urls(&self) -> bool {
        match self {
            ScopedGroup::PartialInvalidation(PartialInvalidationPatch(_)) => true,
            ScopedGroup::NoInvalidation(url_map) => !url_map.is_empty(),
        }
    }

    fn no_invalidation_iter(&self) -> impl Iterator<Item = &PatchInfo> {
        match self {
            ScopedGroup::PartialInvalidation(_) => NoInvalidationPatchesIter { it: None },
            ScopedGroup::NoInvalidation(map) => {
                NoInvalidationPatchesIter { it: Some(map.values()) }
            }
        }
    }
}

/// For non invalidating patches the specification requires they be ordered by
/// entry order.
///
/// That is the order that the physical entries in the patch map are in. This
/// struct adds that ordering onto PatchUrl's for when they are stored in a
/// btree set/map. Context: <https://github.com/w3c/IFT/pull/279>
#[derive(PartialEq, Eq, PartialOrd, Ord, Debug, Clone)]
pub struct OrderedPatchUrl(usize, PatchUrl);

struct NoInvalidationPatchesIter<'a, T>
where
    T: Iterator<Item = &'a NoInvalidationPatch>,
{
    it: Option<T>,
}

impl<'a, T> Iterator for NoInvalidationPatchesIter<'a, T>
where
    T: Iterator<Item = &'a NoInvalidationPatch>,
{
    type Item = &'a PatchInfo;

    fn next(&mut self) -> Option<Self::Item> {
        let it = self.it.as_mut()?;
        Some(&it.next()?.0)
    }
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use super::*;
    use crate::{
        glyph_keyed::tests::assemble_glyph_keyed_patch, patchmap::PatchId,
        testdata::test_font_for_patching_with_loca_mod,
    };
    use font_test_data::{
        bebuffer::BeBuffer,
        ift::{
            custom_ids_format2, glyf_u16_glyph_patches, glyph_keyed_patch_header,
            table_keyed_format2, table_keyed_patch, ABSOLUTE_URL_TEMPLATE, RELATIVE_URL_TEMPLATE,
        },
    };

    use font_types::{Int24, Tag, Uint24};

    use read_fonts::{
        tables::ift::{IFTX_TAG, IFT_TAG},
        FontRef,
    };

    use write_fonts::FontBuilder;

    const TABLE_1_FINAL_STATE: &[u8] = "hijkabcdeflmnohijkabcdeflmno\n".as_bytes();
    const TABLE_2_FINAL_STATE: &[u8] = "foobarbaz foobarbaz foobarbaz\n".as_bytes();

    impl PatchUrl {
        fn new(url: &str) -> Self {
            Self(url.to_string())
        }
    }

    impl OrderedPatchUrl {
        fn url(order: usize, url: &str) -> Self {
            OrderedPatchUrl(order, PatchUrl(url.to_string()))
        }
    }

    fn base_font(ift: Option<BeBuffer>, iftx: Option<BeBuffer>) -> Vec<u8> {
        let mut font_builder = FontBuilder::new();

        if let Some(buffer) = &ift {
            font_builder.add_raw(IFT_TAG, buffer.as_slice());
        }
        if let Some(buffer) = &iftx {
            font_builder.add_raw(IFTX_TAG, buffer.as_slice());
        }

        font_builder.add_raw(Tag::new(b"tab1"), "abcdef\n".as_bytes());
        font_builder.add_raw(Tag::new(b"tab2"), "foobar\n".as_bytes());
        font_builder.add_raw(Tag::new(b"tab4"), "abcdef\n".as_bytes());
        font_builder.add_raw(Tag::new(b"tab5"), "foobar\n".as_bytes());
        font_builder.build()
    }

    fn cid_1() -> CompatibilityId {
        CompatibilityId::from_u32s([0, 0, 0, 1])
    }

    fn cid_2() -> CompatibilityId {
        CompatibilityId::from_u32s([0, 0, 0, 2])
    }

    fn p(index: u32, table: IftTableTag, format: PatchFormat) -> PatchMapEntry {
        let url =
            PatchUrl::expand_template(ABSOLUTE_URL_TEMPLATE, &PatchId::Numeric(index)).unwrap();
        let mut e = url.into_format_1_entry(table, format, Default::default());
        e.application_bit_indices.insert(42);
        e
    }

    fn p1_full() -> PatchMapEntry {
        p(1, IftTableTag::Ift(cid_1()), PatchFormat::TableKeyed { fully_invalidating: true })
    }

    fn p2_partial_c1() -> PatchMapEntry {
        p(2, IftTableTag::Ift(cid_1()), PatchFormat::TableKeyed { fully_invalidating: false })
    }

    fn p2_partial_c2() -> PatchMapEntry {
        p(2, IftTableTag::Iftx(cid_2()), PatchFormat::TableKeyed { fully_invalidating: false })
    }

    fn p2_no_c2() -> PatchMapEntry {
        p(2, IftTableTag::Iftx(cid_2()), PatchFormat::GlyphKeyed)
    }

    fn p3_partial_c2() -> PatchMapEntry {
        p(3, IftTableTag::Iftx(cid_2()), PatchFormat::TableKeyed { fully_invalidating: false })
    }

    fn p3_no_c1() -> PatchMapEntry {
        p(3, IftTableTag::Ift(cid_1()), PatchFormat::GlyphKeyed)
    }

    fn p4_no_c1() -> PatchMapEntry {
        p(4, IftTableTag::Ift(cid_1()), PatchFormat::GlyphKeyed)
    }

    fn p4_no_c2() -> PatchMapEntry {
        p(4, IftTableTag::Iftx(cid_2()), PatchFormat::GlyphKeyed)
    }

    fn p5_no_c2() -> PatchMapEntry {
        p(5, IftTableTag::Iftx(cid_2()), PatchFormat::GlyphKeyed)
    }

    fn full(index: u32, codepoints: u64) -> PatchMapEntry {
        let url =
            PatchUrl::expand_template(ABSOLUTE_URL_TEMPLATE, &PatchId::Numeric(index)).unwrap();
        let mut e = url.into_format_1_entry(
            IftTableTag::Ift(cid_1()),
            PatchFormat::TableKeyed { fully_invalidating: true },
            IntersectionInfo::new(codepoints, 0, 0),
        );
        e.application_bit_indices.insert(42);
        e
    }

    fn partial(index: u32, compat_id: CompatibilityId, codepoints: u64) -> PatchMapEntry {
        let tag = if compat_id == cid_1() {
            IftTableTag::Ift(compat_id)
        } else {
            IftTableTag::Iftx(compat_id)
        };
        let url =
            PatchUrl::expand_template(ABSOLUTE_URL_TEMPLATE, &PatchId::Numeric(index)).unwrap();
        let mut e = url.into_format_1_entry(
            tag,
            PatchFormat::TableKeyed { fully_invalidating: false },
            IntersectionInfo::new(codepoints, 0, 0),
        );
        e.application_bit_indices.insert(42);
        e
    }

    fn patch_info_ift(url: &str) -> PatchInfo {
        let mut application_flag_bit_indices = IntSet::<u32>::empty();
        application_flag_bit_indices.insert(42);
        PatchInfo {
            url: PatchUrl::new(url),
            application_flag_bit_indices,
            source_table: IftTableTag::Ift(cid_1()),
        }
    }

    fn patch_info_iftx(url: &str) -> PatchInfo {
        let mut application_flag_bit_indices = IntSet::<u32>::empty();
        application_flag_bit_indices.insert(42);
        PatchInfo {
            url: PatchUrl::new(url),
            application_flag_bit_indices,
            source_table: IftTableTag::Iftx(cid_2()),
        }
    }

    #[test]
    fn full_invalidation() {
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p1_full()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Full(FullInvalidationPatch(patch_info_ift("//foo.bar/04")))
        );
        assert!(preloads.is_empty());

        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p1_full(), p2_partial_c1(), p3_partial_c2(), p4_no_c1(), p5_no_c2()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Full(FullInvalidationPatch(patch_info_ift("//foo.bar/04"),))
        );
        assert!(preloads.is_empty());
    }

    fn preload_list(urls: &[String]) -> Vec<PatchUrl> {
        urls.iter().map(|url| PatchUrl::new(url)).collect()
    }

    #[test]
    fn full_invalidation_with_preloads() {
        let expected_preloads = [PatchUrl::new("abc"), PatchUrl::new("def")];
        let mut p1_full = p1_full();
        p1_full.preload_urls.extend(expected_preloads.clone());

        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p1_full.clone()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Full(FullInvalidationPatch(patch_info_ift("//foo.bar/04")))
        );
        assert_eq!(preloads, BTreeSet::from(expected_preloads.clone()));

        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p1_full, p2_partial_c1(), p3_partial_c2(), p4_no_c1(), p5_no_c2()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Full(FullInvalidationPatch(patch_info_ift("//foo.bar/04"),))
        );
        assert_eq!(preloads, BTreeSet::from(expected_preloads));
    }

    #[test]
    fn full_invalidation_with_preloads_removes_duplicate_urls() {
        let expected_preloads = [PatchUrl::new("abc"), PatchUrl::new("def")];
        let mut p1_full = p1_full();
        p1_full.preload_urls.extend(expected_preloads.clone());
        p1_full.preload_urls.extend(preload_list(&["//foo.bar/04".to_string()]));

        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p1_full.clone()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Full(FullInvalidationPatch(patch_info_ift("//foo.bar/04")))
        );
        assert_eq!(preloads, BTreeSet::from(expected_preloads.clone()));
    }

    #[test]
    fn full_invalidation_selection_order() {
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![full(3, 9), full(1, 7), full(2, 24)],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Full(FullInvalidationPatch(patch_info_ift("//foo.bar/08")))
        );
        assert!(preloads.is_empty());
    }

    #[test]
    fn partial_invalidation_selection_order() {
        // Only IFT
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![partial(3, cid_1(), 9), partial(1, cid_1(), 23), partial(2, cid_1(), 24)],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_ift(
                    "//foo.bar/08"
                ),)),
                iftx: ScopedGroup::NoInvalidation(BTreeMap::default()),
            }
        );
        assert!(preloads.is_empty());

        // Only IFTX
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![partial(4, cid_2(), 1), partial(5, cid_2(), 22), partial(6, cid_2(), 2)],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::NoInvalidation(BTreeMap::default()),

                iftx: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_iftx(
                    "//foo.bar/0K"
                ),)),
            }
        );
        assert!(preloads.is_empty());

        // Both
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![
                partial(3, cid_1(), 9),
                partial(1, cid_1(), 23),
                partial(2, cid_1(), 24),
                partial(4, cid_2(), 1),
                partial(5, cid_2(), 22),
                partial(6, cid_2(), 2),
            ],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_ift(
                    "//foo.bar/08"
                ),)),
                iftx: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_iftx(
                    "//foo.bar/0K"
                ),)),
            }
        );
        assert!(preloads.is_empty());
    }

    #[test]
    fn partial_invalidation_with_preloaded() {
        // 1 -> 04
        // 2 -> 08
        // 3 -> 0C
        // //foo.bar/{id}

        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![partial(3, cid_1(), 9), partial(1, cid_1(), 23), partial(2, cid_1(), 24)],
            // Entry 3 is marked as already loaded which gives it priority
            &HashMap::from([(PatchUrl::new("//foo.bar/0C"), UrlStatus::Pending(vec![]))]),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_ift(
                    "//foo.bar/0C"
                ),)),
                iftx: ScopedGroup::NoInvalidation(BTreeMap::default()),
            }
        );
        assert!(preloads.is_empty());

        // With multiple preloaded
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![partial(3, cid_1(), 9), partial(1, cid_1(), 23), partial(2, cid_1(), 24)],
            // Entry 1 and 3 are marked as already loaded which gives it priority
            &HashMap::from([
                (PatchUrl::new("//foo.bar/04"), UrlStatus::Pending(vec![])),
                (PatchUrl::new("//foo.bar/0C"), UrlStatus::Pending(vec![])),
            ]),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_ift(
                    "//foo.bar/04"
                ),)),
                iftx: ScopedGroup::NoInvalidation(BTreeMap::default()),
            }
        );
        assert!(preloads.is_empty());
    }

    #[test]
    fn partial_invalidation_with_preloads() {
        let mut partial_c1 = partial(3, cid_1(), 9);
        let mut partial_c2 = partial(4, cid_2(), 9);

        let expected_preloads_c1 = [PatchUrl::new("abc"), PatchUrl::new("def")];
        partial_c1.preload_urls.extend(expected_preloads_c1.clone());

        let expected_preloads_c2 = [PatchUrl::new("hij"), PatchUrl::new("def")];
        partial_c2.preload_urls.extend(expected_preloads_c2.clone());

        // Only IFT
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![partial_c1.clone()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_ift(
                    "//foo.bar/0C"
                ),)),
                iftx: ScopedGroup::NoInvalidation(BTreeMap::default()),
            }
        );
        assert_eq!(preloads, BTreeSet::from(expected_preloads_c1.clone()));

        // Only IFTX
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![partial_c2.clone()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::NoInvalidation(BTreeMap::default()),

                iftx: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_iftx(
                    "//foo.bar/0G"
                ),)),
            }
        );
        assert_eq!(preloads, BTreeSet::from(expected_preloads_c2.clone()));

        // Both
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![partial_c1, partial_c2],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_ift(
                    "//foo.bar/0C"
                ),)),
                iftx: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_iftx(
                    "//foo.bar/0G"
                ),)),
            }
        );
        assert_eq!(
            preloads,
            BTreeSet::from([PatchUrl::new("abc"), PatchUrl::new("def"), PatchUrl::new("hij")])
        );
    }

    #[test]
    fn partial_invalidation_with_preloads_removes_duplicates() {
        let mut partial_c1 = partial(3, cid_1(), 9);
        let mut partial_c2 = partial(4, cid_2(), 9);

        let expected_preloads_c1 = ["abc".to_string(), "def".to_string()];
        partial_c1.preload_urls.extend(preload_list(&expected_preloads_c1));
        partial_c1
            .preload_urls
            .extend(preload_list(&["//foo.bar/0C".to_string(), "//foo.bar/0G".to_string()]));

        let expected_preloads_c2 = ["hij".to_string(), "def".to_string()];
        partial_c2.preload_urls.extend(preload_list(&expected_preloads_c2));

        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![partial_c1, partial_c2],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_ift(
                    "//foo.bar/0C"
                ),)),
                iftx: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_iftx(
                    "//foo.bar/0G"
                ),)),
            }
        );
        assert_eq!(
            preloads,
            BTreeSet::from([PatchUrl::new("abc"), PatchUrl::new("def"), PatchUrl::new("hij"),])
        );
    }

    #[test]
    fn no_invalidation_with_preloads() {
        let expected_preloads_c1 = ["abc".to_string(), "def".to_string()];
        let expected_preloads_c2 = ["hij".to_string(), "def".to_string()];
        let mut p4_no_c1 = p4_no_c1();
        p4_no_c1.preload_urls.extend(preload_list(&expected_preloads_c1));

        let mut p4_no_c2 = p4_no_c2();
        p4_no_c2.preload_urls.extend(preload_list(&expected_preloads_c1));
        let mut p5_no_c2 = p5_no_c2();
        p5_no_c2.preload_urls.extend(preload_list(&expected_preloads_c2));

        // (no inval, no inval)
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p4_no_c1, p5_no_c2.clone()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::NoInvalidation(BTreeMap::from([(
                    OrderedPatchUrl::url(0, "//foo.bar/0G"),
                    NoInvalidationPatch(patch_info_ift("//foo.bar/0G"))
                )])),
                iftx: ScopedGroup::NoInvalidation(BTreeMap::from([(
                    OrderedPatchUrl::url(0, "//foo.bar/0K"),
                    NoInvalidationPatch(patch_info_iftx("//foo.bar/0K"))
                )]))
            }
        );
        assert_eq!(preloads, BTreeSet::from(["abc", "def", "hij",].map(PatchUrl::new)));

        // (None, no inval)
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p4_no_c2, p5_no_c2],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::NoInvalidation(Default::default()),
                iftx: ScopedGroup::NoInvalidation(BTreeMap::from([
                    (
                        OrderedPatchUrl::url(0, "//foo.bar/0K"),
                        NoInvalidationPatch(patch_info_iftx("//foo.bar/0K"))
                    ),
                    (
                        OrderedPatchUrl::url(0, "//foo.bar/0G"),
                        NoInvalidationPatch(patch_info_iftx("//foo.bar/0G"))
                    )
                ]))
            }
        );
        assert_eq!(preloads, BTreeSet::from(["abc", "def", "hij",].map(PatchUrl::new)));
    }

    #[test]
    fn no_invalidation_with_preloads_removes_duplicates() {
        let expected_preloads_c1 = ["abc".to_string(), "def".to_string()];
        let expected_preloads_c2 = ["hij".to_string(), "def".to_string()];
        let mut p4_no_c1 = p4_no_c1();
        p4_no_c1.preload_urls.extend(preload_list(&expected_preloads_c1));

        let mut p5_no_c2 = p5_no_c2();
        p5_no_c2.preload_urls.extend(preload_list(&expected_preloads_c2));
        p5_no_c2
            .preload_urls
            .extend(preload_list(&["//foo.bar/0G".to_string(), "//foo.bar/0K".to_string()]));

        // (no inval, no inval)
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p4_no_c1, p5_no_c2.clone()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::NoInvalidation(BTreeMap::from([(
                    OrderedPatchUrl::url(0, "//foo.bar/0G"),
                    NoInvalidationPatch(patch_info_ift("//foo.bar/0G"))
                )])),
                iftx: ScopedGroup::NoInvalidation(BTreeMap::from([(
                    OrderedPatchUrl::url(0, "//foo.bar/0K"),
                    NoInvalidationPatch(patch_info_iftx("//foo.bar/0K"))
                )]))
            }
        );
        assert_eq!(preloads, BTreeSet::from(["abc", "def", "hij",].map(PatchUrl::new)));
    }

    #[test]
    fn mixed() {
        // (partial, no inval)
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p2_partial_c1(), p4_no_c1(), p5_no_c2()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_ift(
                    "//foo.bar/08"
                ),)),
                iftx: ScopedGroup::NoInvalidation(BTreeMap::from([(
                    OrderedPatchUrl::url(0, "//foo.bar/0K"),
                    NoInvalidationPatch(patch_info_iftx("//foo.bar/0K"))
                )]))
            }
        );
        assert!(preloads.is_empty());

        // (no inval, partial)
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p3_partial_c2(), p4_no_c1(), p5_no_c2()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::NoInvalidation(BTreeMap::from([(
                    OrderedPatchUrl::url(0, "//foo.bar/0G"),
                    NoInvalidationPatch(patch_info_ift("//foo.bar/0G"))
                )])),
                iftx: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_iftx(
                    "//foo.bar/0C"
                ),))
            }
        );
        assert!(preloads.is_empty());

        // (partial, empty)
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p2_partial_c1(), p4_no_c1()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_ift(
                    "//foo.bar/08"
                ),)),
                iftx: ScopedGroup::NoInvalidation(BTreeMap::default()),
            }
        );
        assert!(preloads.is_empty());

        // (empty, partial)
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p3_partial_c2(), p5_no_c2()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::NoInvalidation(BTreeMap::default()),
                iftx: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_iftx(
                    "//foo.bar/0C"
                ),)),
            }
        );
        assert!(preloads.is_empty());
    }

    #[test]
    fn mixed_with_preloads() {
        let mut p2_partial_c1 = p2_partial_c1();
        p2_partial_c1.preload_urls.extend(preload_list(&["abc".to_string(), "def".to_string()]));

        let mut p3_partial_c2 = p3_partial_c2();
        p3_partial_c2.preload_urls.extend(preload_list(&["klm".to_string(), "nop".to_string()]));

        let mut p4_no_c1 = p4_no_c1();
        p4_no_c1.preload_urls.extend(preload_list(&["foo".to_string(), "bar".to_string()]));

        let mut p5_no_c2 = p5_no_c2();
        p5_no_c2.preload_urls.extend(preload_list(&["hij".to_string(), "def".to_string()]));

        // (partial, no inval)
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p2_partial_c1, p4_no_c1.clone(), p5_no_c2.clone()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_ift(
                    "//foo.bar/08"
                ),)),
                iftx: ScopedGroup::NoInvalidation(BTreeMap::from([(
                    OrderedPatchUrl::url(0, "//foo.bar/0K"),
                    NoInvalidationPatch(patch_info_iftx("//foo.bar/0K"))
                )]))
            }
        );
        assert_eq!(preloads, BTreeSet::from(["abc", "def", "hij",].map(PatchUrl::new)));

        // (no inval, partial)
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p3_partial_c2, p4_no_c1, p5_no_c2],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::NoInvalidation(BTreeMap::from([(
                    OrderedPatchUrl::url(0, "//foo.bar/0G"),
                    NoInvalidationPatch(patch_info_ift("//foo.bar/0G"))
                )])),
                iftx: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_iftx(
                    "//foo.bar/0C"
                ),))
            }
        );
        assert_eq!(preloads, BTreeSet::from(["klm", "nop", "foo", "bar",].map(PatchUrl::new)));
    }

    #[test]
    fn missing_compat_ids() {
        // (None, None)
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p2_partial_c1(), p4_no_c1(), p5_no_c2()],
            &Default::default(),
            None,
            None,
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::NoInvalidation(Default::default()),
                iftx: ScopedGroup::NoInvalidation(Default::default()),
            }
        );
        assert!(preloads.is_empty());

        // (Some, None)
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p2_partial_c1(), p4_no_c1(), p5_no_c2()],
            &Default::default(),
            Some(cid_1()),
            None,
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_ift(
                    "//foo.bar/08"
                ),)),
                iftx: ScopedGroup::NoInvalidation(Default::default()),
            }
        );
        assert!(preloads.is_empty());

        // (None, Some)
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p2_partial_c1(), p4_no_c1(), p5_no_c2()],
            &Default::default(),
            None,
            Some(cid_1()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::NoInvalidation(Default::default()),
                iftx: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_ift(
                    "//foo.bar/08"
                ),)),
            }
        );
        assert!(preloads.is_empty());
    }

    #[test]
    fn dedups_urls() {
        // Duplicates inside a scope
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p4_no_c1(), p4_no_c1()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::NoInvalidation(BTreeMap::from([(
                    OrderedPatchUrl::url(0, "//foo.bar/0G"),
                    NoInvalidationPatch(patch_info_ift("//foo.bar/0G"))
                )])),
                iftx: ScopedGroup::NoInvalidation(BTreeMap::new()),
            }
        );
        assert!(preloads.is_empty());

        // Duplicates across scopes (no invalidation + no invalidation)
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p4_no_c1(), p4_no_c2(), p5_no_c2()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::NoInvalidation(BTreeMap::from([(
                    OrderedPatchUrl::url(0, "//foo.bar/0G"),
                    NoInvalidationPatch(patch_info_ift("//foo.bar/0G"))
                )])),
                iftx: ScopedGroup::NoInvalidation(BTreeMap::from([(
                    OrderedPatchUrl::url(0, "//foo.bar/0K"),
                    NoInvalidationPatch(patch_info_iftx("//foo.bar/0K"))
                )])),
            }
        );
        assert!(preloads.is_empty());

        // Duplicates across scopes (partial + partial)
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p2_partial_c1(), p2_partial_c2(), p3_partial_c2()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_ift(
                    "//foo.bar/08"
                ))),
                iftx: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_iftx(
                    "//foo.bar/0C"
                ))),
            }
        );
        assert!(preloads.is_empty());

        // Duplicates across scopes (partial + no invalidation)
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p2_partial_c1(), p2_no_c2(), p5_no_c2()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_ift(
                    "//foo.bar/08"
                ))),
                iftx: ScopedGroup::NoInvalidation(BTreeMap::from([(
                    OrderedPatchUrl::url(0, "//foo.bar/0K"),
                    NoInvalidationPatch(patch_info_iftx("//foo.bar/0K"))
                )])),
            }
        );
        assert!(preloads.is_empty());

        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            vec![p3_partial_c2(), p3_no_c1(), p4_no_c1()],
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        assert_eq!(
            group,
            CompatibleGroup::Mixed {
                ift: ScopedGroup::NoInvalidation(BTreeMap::from([(
                    OrderedPatchUrl::url(0, "//foo.bar/0G"),
                    NoInvalidationPatch(patch_info_ift("//foo.bar/0G"))
                )])),
                iftx: ScopedGroup::PartialInvalidation(PartialInvalidationPatch(patch_info_iftx(
                    "//foo.bar/0C"
                ))),
            }
        );
        assert!(preloads.is_empty());
    }

    fn create_group_for(urls: Vec<PatchMapEntry>) -> PatchGroup<'static> {
        let data = FontRef::new(font_test_data::CMAP12_FONT1).unwrap();
        let (group, preloads) = PatchGroup::select_next_patches_from_candidates(
            urls,
            &Default::default(),
            Some(cid_1()),
            Some(cid_2()),
        )
        .unwrap();

        PatchGroup { font: data, patches: Some(group), preload_urls: preloads }
    }

    fn empty_group() -> PatchGroup<'static> {
        let data = FontRef::new(font_test_data::CMAP12_FONT1).unwrap();
        PatchGroup { font: data, patches: None, preload_urls: Default::default() }
    }

    #[test]
    fn urls() {
        let g = create_group_for(vec![]);
        assert_eq!(g.urls().map(|url| url.as_ref()).collect::<Vec<&str>>(), Vec::<&str>::default());
        assert!(!g.has_urls());

        let g = empty_group();
        assert_eq!(g.urls().map(|url| url.as_ref()).collect::<Vec<&str>>(), Vec::<&str>::default());
        assert!(!g.has_urls());

        let g = create_group_for(vec![p1_full()]);
        assert_eq!(g.urls().map(|url| url.as_ref()).collect::<Vec<&str>>(), vec!["//foo.bar/04"],);
        assert!(g.has_urls());

        let g = create_group_for(vec![p2_partial_c1(), p3_partial_c2()]);
        assert_eq!(
            g.urls().map(|url| url.as_ref()).collect::<Vec<&str>>(),
            vec!["//foo.bar/08", "//foo.bar/0C"]
        );
        assert!(g.has_urls());

        let g = create_group_for(vec![p2_partial_c1()]);
        assert_eq!(g.urls().map(|url| url.as_ref()).collect::<Vec<&str>>(), vec!["//foo.bar/08",],);
        assert!(g.has_urls());

        let g = create_group_for(vec![p3_partial_c2()]);
        assert_eq!(g.urls().map(|url| url.as_ref()).collect::<Vec<&str>>(), vec!["//foo.bar/0C"],);
        assert!(g.has_urls());

        let g = create_group_for(vec![p2_partial_c1(), p4_no_c2(), p5_no_c2()]);
        assert_eq!(
            g.urls().map(|url| url.as_ref()).collect::<Vec<&str>>(),
            vec!["//foo.bar/08", "//foo.bar/0G", "//foo.bar/0K"],
        );
        assert!(g.has_urls());

        let g = create_group_for(vec![p3_partial_c2(), p4_no_c1()]);
        assert_eq!(
            g.urls().map(|url| url.as_ref()).collect::<Vec<&str>>(),
            vec!["//foo.bar/0C", "//foo.bar/0G"],
        );

        let g = create_group_for(vec![p4_no_c1(), p5_no_c2()]);
        assert_eq!(
            g.urls().map(|url| url.as_ref()).collect::<Vec<&str>>(),
            vec!["//foo.bar/0G", "//foo.bar/0K"],
        );
        assert!(g.has_urls());
    }

    #[test]
    fn urls_with_preloads() {
        let mut p2_partial_c1 = p2_partial_c1();
        p2_partial_c1.preload_urls.extend(preload_list(&["abc".to_string(), "def".to_string()]));

        let mut p3_partial_c2 = p3_partial_c2();
        p3_partial_c2.preload_urls.extend(preload_list(&["foo".to_string(), "bar".to_string()]));

        let g = create_group_for(vec![p2_partial_c1, p3_partial_c2]);
        assert_eq!(
            g.urls().map(|url| url.as_ref()).collect::<Vec<&str>>(),
            vec!["//foo.bar/08", "//foo.bar/0C", "abc", "bar", "def", "foo"]
        );
        assert!(g.has_urls());
    }

    #[test]
    fn select_next_patches_no_intersection() {
        let font = base_font(Some(table_keyed_format2()), None);
        let font = FontRef::new(&font).unwrap();

        let s = SubsetDefinition::codepoints([55].into_iter().collect());
        let g = PatchGroup::select_next_patches(font, &Default::default(), &s).unwrap();

        assert!(!g.has_urls());
        assert_eq!(g.urls().map(|url| url.as_ref()).collect::<Vec<&str>>(), Vec::<&str>::default());

        assert_eq!(
            g.apply_next_patches(&mut Default::default()),
            Err(PatchingError::EmptyPatchList)
        );
    }

    #[test]
    fn apply_patches_full_invalidation() {
        let font = base_font(Some(table_keyed_format2()), None);
        let font = FontRef::new(&font).unwrap();

        let s = SubsetDefinition::codepoints([5].into_iter().collect());
        let g = PatchGroup::select_next_patches(font, &Default::default(), &s).unwrap();

        assert!(g.has_urls());
        let mut patch_data = HashMap::from([
            (PatchUrl::new("foo/04"), UrlStatus::Pending(table_keyed_patch().as_slice().to_vec())),
            (PatchUrl::new("foo/bar"), UrlStatus::Pending(table_keyed_patch().as_slice().to_vec())),
        ]);

        let new_font = g.apply_next_patches(&mut patch_data).unwrap();
        let new_font = FontRef::new(&new_font).unwrap();

        assert_eq!(new_font.table_data(Tag::new(b"tab1")).unwrap().as_bytes(), TABLE_1_FINAL_STATE,);
        assert_eq!(new_font.table_data(Tag::new(b"tab2")).unwrap().as_bytes(), TABLE_2_FINAL_STATE,);

        assert_eq!(
            patch_data,
            HashMap::from([
                (PatchUrl::new("foo/04"), UrlStatus::Applied,),
                (
                    PatchUrl::new("foo/bar"),
                    UrlStatus::Pending(table_keyed_patch().as_slice().to_vec()),
                ),
            ])
        )
    }

    struct CustomBrotliDecoder;

    impl SharedBrotliDecoder for CustomBrotliDecoder {
        fn decode(
            &self,
            _encoded: &[u8],
            _shared_dictionary: Option<&[u8]>,
            _max_uncompressed_length: usize,
        ) -> Result<Vec<u8>, shared_brotli_patch_decoder::decode_error::DecodeError> {
            Ok(vec![1, 2, 3, 4, 5])
        }
    }

    #[test]
    fn apply_patches_full_invalidation_with_custom_brotli() {
        let font = base_font(Some(table_keyed_format2()), None);
        let font = FontRef::new(&font).unwrap();

        let s = SubsetDefinition::codepoints([5].into_iter().collect());
        let g = PatchGroup::select_next_patches(font, &Default::default(), &s).unwrap();

        assert!(g.has_urls());
        let mut patch_data = HashMap::from([
            (PatchUrl::new("foo/04"), UrlStatus::Pending(table_keyed_patch().as_slice().to_vec())),
            (PatchUrl::new("foo/bar"), UrlStatus::Pending(table_keyed_patch().as_slice().to_vec())),
        ]);

        let new_font =
            g.apply_next_patches_with_decoder(&mut patch_data, &CustomBrotliDecoder).unwrap();
        let new_font = FontRef::new(&new_font).unwrap();

        assert_eq!(new_font.table_data(Tag::new(b"tab1")).unwrap().as_bytes(), vec![1, 2, 3, 4, 5]);
        assert_eq!(new_font.table_data(Tag::new(b"tab2")).unwrap().as_bytes(), vec![1, 2, 3, 4, 5]);

        assert_eq!(
            patch_data,
            HashMap::from([
                (PatchUrl::new("foo/04"), UrlStatus::Applied,),
                (
                    PatchUrl::new("foo/bar"),
                    UrlStatus::Pending(table_keyed_patch().as_slice().to_vec()),
                ),
            ])
        )
    }

    #[test]
    fn apply_patches_one_partial_invalidation() {
        let mut buffer = table_keyed_format2();
        buffer.write_at("encoding", 2u8);

        // IFT
        let font = base_font(Some(buffer.clone()), None);
        let font = FontRef::new(&font).unwrap();

        let s = SubsetDefinition::codepoints([5].into_iter().collect());
        let g = PatchGroup::select_next_patches(font, &Default::default(), &s).unwrap();

        let mut patch_data = HashMap::from([(
            PatchUrl::new("foo/04"),
            UrlStatus::Pending(table_keyed_patch().as_slice().to_vec()),
        )]);

        let new_font = g.apply_next_patches(&mut patch_data).unwrap();
        let new_font = FontRef::new(&new_font).unwrap();

        assert_eq!(new_font.table_data(Tag::new(b"tab1")).unwrap().as_bytes(), TABLE_1_FINAL_STATE,);
        assert_eq!(new_font.table_data(Tag::new(b"tab2")).unwrap().as_bytes(), TABLE_2_FINAL_STATE,);

        assert_eq!(patch_data, HashMap::from([(PatchUrl::new("foo/04"), UrlStatus::Applied,),]));

        // IFTX
        let font = base_font(None, Some(buffer.clone()));
        let font = FontRef::new(&font).unwrap();

        let s = SubsetDefinition::codepoints([5].into_iter().collect());
        let g = PatchGroup::select_next_patches(font, &Default::default(), &s).unwrap();

        let mut patch_data = HashMap::from([(
            PatchUrl::new("foo/04"),
            UrlStatus::Pending(table_keyed_patch().as_slice().to_vec()),
        )]);

        let new_font = g.apply_next_patches(&mut patch_data).unwrap();
        let new_font = FontRef::new(&new_font).unwrap();

        assert_eq!(new_font.table_data(Tag::new(b"tab1")).unwrap().as_bytes(), TABLE_1_FINAL_STATE,);
        assert_eq!(new_font.table_data(Tag::new(b"tab2")).unwrap().as_bytes(), TABLE_2_FINAL_STATE,);

        assert_eq!(patch_data, HashMap::from([(PatchUrl::new("foo/04"), UrlStatus::Applied,),]));
    }

    #[test]
    fn apply_patches_two_partial_invalidation() {
        let mut ift_buffer = table_keyed_format2();
        ift_buffer.write_at("encoding", 2u8);

        let mut iftx_buffer = table_keyed_format2();
        iftx_buffer.write_at("compat_id[0]", 2u32);
        iftx_buffer.write_at("encoding", 2u8);
        iftx_buffer.write_at("id_delta", Int24::new(2)); // delta = +1

        let font = base_font(Some(ift_buffer), Some(iftx_buffer));
        let font = FontRef::new(&font).unwrap();

        let s = SubsetDefinition::codepoints([5].into_iter().collect());
        let g = PatchGroup::select_next_patches(font.clone(), &Default::default(), &s).unwrap();

        let mut patch_2 = table_keyed_patch();
        patch_2.write_at("compat_id", 2u32);
        patch_2.write_at("patch[0]", Tag::new(b"tab4"));
        patch_2.write_at("patch[1]", Tag::new(b"tab5"));

        let mut patch_data = HashMap::from([
            (PatchUrl::new("foo/04"), UrlStatus::Pending(table_keyed_patch().as_slice().to_vec())),
            (PatchUrl::new("foo/08"), UrlStatus::Pending(patch_2.as_slice().to_vec())),
        ]);

        let new_font = g.apply_next_patches(&mut patch_data).unwrap();
        let new_font = FontRef::new(&new_font).unwrap();

        assert_eq!(new_font.table_data(Tag::new(b"tab1")).unwrap().as_bytes(), TABLE_1_FINAL_STATE,);
        assert_eq!(new_font.table_data(Tag::new(b"tab2")).unwrap().as_bytes(), TABLE_2_FINAL_STATE,);

        // only the first patch gets applied so tab4/tab5 are unchanged.
        assert_eq!(
            new_font.table_data(Tag::new(b"tab4")).unwrap().as_bytes(),
            font.table_data(Tag::new(b"tab4")).unwrap().as_bytes(),
        );
        assert_eq!(
            new_font.table_data(Tag::new(b"tab5")).unwrap().as_bytes(),
            font.table_data(Tag::new(b"tab5")).unwrap().as_bytes(),
        );
    }

    #[test]
    fn apply_patches_mixed() {
        let mut ift_builder = table_keyed_format2();
        ift_builder.write_at("encoding", 2u8);

        let mut iftx_builder = table_keyed_format2();
        iftx_builder.write_at("encoding", 3u8);
        iftx_builder.write_at("compat_id[0]", 6u32);
        iftx_builder.write_at("compat_id[1]", 7u32);
        iftx_builder.write_at("compat_id[2]", 8u32);
        iftx_builder.write_at("compat_id[3]", 9u32);
        iftx_builder.write_at("id_delta", Int24::new(2)); // delta = +1

        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([
                (IFT_TAG, ift_builder.as_slice()),
                (IFTX_TAG, iftx_builder.as_slice()),
                (Tag::new(b"tab1"), "abcdef\n".as_bytes()),
            ]),
        );
        let font = FontRef::new(font.as_slice()).unwrap();

        let s = SubsetDefinition::codepoints([5].into_iter().collect());
        let g = PatchGroup::select_next_patches(font.clone(), &Default::default(), &s).unwrap();

        let patch_ift = table_keyed_patch();
        let patch_iftx =
            assemble_glyph_keyed_patch(glyph_keyed_patch_header(), glyf_u16_glyph_patches());

        let mut patch_data = HashMap::from([
            (PatchUrl::new("foo/04"), UrlStatus::Pending(patch_ift.as_slice().to_vec())),
            (PatchUrl::new("foo/08"), UrlStatus::Pending(patch_iftx.as_slice().to_vec())),
        ]);

        let new_font = g.apply_next_patches(&mut patch_data).unwrap();
        let new_font = FontRef::new(&new_font).unwrap();

        assert_eq!(new_font.table_data(Tag::new(b"tab1")).unwrap().as_bytes(), TABLE_1_FINAL_STATE,);

        // only the partial invalidation patch gets applied, so glyf is unchanged.
        assert_eq!(
            new_font.table_data(Tag::new(b"glyf")).unwrap().as_bytes(),
            font.table_data(Tag::new(b"glyf")).unwrap().as_bytes(),
        );
    }

    #[test]
    fn apply_patches_all_no_invalidation() {
        let mut ift_builder = table_keyed_format2();
        ift_builder.write_at("encoding", 3u8);
        ift_builder.write_at("compat_id[0]", 6u32);
        ift_builder.write_at("compat_id[1]", 7u32);
        ift_builder.write_at("compat_id[2]", 8u32);
        ift_builder.write_at("compat_id[3]", 9u32);

        let mut iftx_builder = table_keyed_format2();
        iftx_builder.write_at("encoding", 3u8);
        iftx_builder.write_at("compat_id[0]", 7u32);
        iftx_builder.write_at("compat_id[1]", 7u32);
        iftx_builder.write_at("compat_id[2]", 8u32);
        iftx_builder.write_at("compat_id[3]", 9u32);
        iftx_builder.write_at("id_delta", Int24::new(2)); // delta = 1

        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([(IFT_TAG, ift_builder.as_slice()), (IFTX_TAG, iftx_builder.as_slice())]),
        );

        let font = FontRef::new(font.as_slice()).unwrap();

        let s = SubsetDefinition::codepoints([5].into_iter().collect());
        let g = PatchGroup::select_next_patches(font, &Default::default(), &s).unwrap();

        let patch1 =
            assemble_glyph_keyed_patch(glyph_keyed_patch_header(), glyf_u16_glyph_patches());

        let mut patch2 = glyf_u16_glyph_patches();
        patch2.write_at("gid_13", 14u16);
        let mut header = glyph_keyed_patch_header();
        header.write_at("compatibility_id", 7u32);
        let patch2 = assemble_glyph_keyed_patch(header, patch2);

        let mut patch_data = HashMap::from([
            (PatchUrl::new("foo/04"), UrlStatus::Pending(patch1.as_slice().to_vec())),
            (PatchUrl::new("foo/08"), UrlStatus::Pending(patch2.as_slice().to_vec())),
        ]);

        let new_font = g.apply_next_patches(&mut patch_data).unwrap();
        let new_font = FontRef::new(&new_font).unwrap();

        let new_glyf: &[u8] = new_font.table_data(Tag::new(b"glyf")).unwrap().as_bytes();
        assert_eq!(
            &[
                1, 2, 3, 4, 5, 0, // gid 0
                6, 7, 8, 0, // gid 1
                b'a', b'b', b'c', 0, // gid2
                b'd', b'e', b'f', b'g', // gid 7
                b'h', b'i', b'j', b'k', b'l', 0, // gid 8 + 9
                b'm', b'n', // gid 13
                b'm', b'n', // gid 14
            ],
            new_glyf
        );

        assert_eq!(
            patch_data,
            HashMap::from([
                (PatchUrl::new("foo/04"), UrlStatus::Applied,),
                (PatchUrl::new("foo/08"), UrlStatus::Applied,),
            ])
        );

        // there should be no more applicable patches left now.
        let g = PatchGroup::select_next_patches(new_font, &Default::default(), &s).unwrap();
        assert!(!g.has_urls());
    }

    #[test]
    fn apply_patches_no_invalidation_duplicate_urls() {
        // Two types of duplicate url situations
        // 1. Same mapping table has duplicate urls. All should be marked applied.
        // 2. Different mapping table has duplicate urls. These will not be marked as
        //    applied.
        let mut ift_builder = table_keyed_format2();
        ift_builder.write_at("encoding", 3u8);
        ift_builder.write_at("compat_id[0]", 6u32);
        ift_builder.write_at("compat_id[1]", 7u32);
        ift_builder.write_at("compat_id[2]", 8u32);
        ift_builder.write_at("compat_id[3]", 9u32);
        ift_builder.write_at("entry_count", Uint24::new(2));

        let ift_builder = ift_builder
            .push(0b00100100u8) // format
            .push(Int24::new(-2)) // id delta
            .push(100u16) // bias
            // codpeoints {100..117}
            .extend([0b00001101, 0b00000011, 0b00110001u8]);

        let mut iftx_builder = table_keyed_format2();
        iftx_builder.write_at("encoding", 3u8);
        iftx_builder.write_at("compat_id[0]", 0u32);
        iftx_builder.write_at("compat_id[1]", 0u32);
        iftx_builder.write_at("compat_id[2]", 0u32);
        iftx_builder.write_at("compat_id[3]", 2u32);
        iftx_builder.write_at("bias", 100u16);

        // Total mapping is:
        // IFT:
        // {0..17} -> foo/04
        // {100..117} -> foo/04
        // IFTX:
        // {100..117} -> foo/04

        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([(IFT_TAG, ift_builder.as_slice()), (IFTX_TAG, iftx_builder.as_slice())]),
        );

        let font = FontRef::new(font.as_slice()).unwrap();

        let s = SubsetDefinition::codepoints([5].into_iter().collect());
        let g = PatchGroup::select_next_patches(font, &Default::default(), &s).unwrap();

        let patch =
            assemble_glyph_keyed_patch(glyph_keyed_patch_header(), glyf_u16_glyph_patches());

        let mut patch_data = HashMap::from([(
            PatchUrl::new("foo/04"),
            UrlStatus::Pending(patch.as_slice().to_vec()),
        )]);

        let new_font = g.apply_next_patches(&mut patch_data).unwrap();
        let new_font = FontRef::new(&new_font).unwrap();

        let new_glyf: &[u8] = new_font.table_data(Tag::new(b"glyf")).unwrap().as_bytes();
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

        assert_eq!(patch_data, HashMap::from([(PatchUrl::new("foo/04"), UrlStatus::Applied,),]));

        // there should be one IFTX patch for foo/04 left now.
        let all = SubsetDefinition::all();
        let group = PatchGroup::select_next_patches(new_font, &Default::default(), &all).unwrap();
        let mut info = patch_info_iftx("foo/04");
        info.application_flag_bit_indices.clear();
        info.application_flag_bit_indices.insert(334);
        assert_eq!(
            group.patches.unwrap(),
            CompatibleGroup::Mixed {
                ift: ScopedGroup::NoInvalidation(Default::default()),
                iftx: ScopedGroup::NoInvalidation(BTreeMap::from([(
                    OrderedPatchUrl::url(0, "foo/04"),
                    NoInvalidationPatch(info)
                )])),
            }
        );
    }

    #[test]
    fn tables_have_same_compat_id() {
        let ift_buffer = table_keyed_format2();
        let iftx_buffer = table_keyed_format2();

        let font = base_font(Some(ift_buffer), Some(iftx_buffer));
        let font = FontRef::new(&font).unwrap();

        let s = SubsetDefinition::codepoints([5].into_iter().collect());
        let g = PatchGroup::select_next_patches(font.clone(), &Default::default(), &s);

        assert!(g.is_err(), "did not fail as expected.");
        if let Err(err) = g {
            assert_eq!(ReadError::ValidationError, err);
        }
    }

    #[test]
    fn invalid_url_templates() {
        let mut buffer = table_keyed_format2();
        buffer.write_at("url_template_var_end", b'~');

        let font = base_font(Some(buffer), None);
        let font = FontRef::new(&font).unwrap();

        let s = SubsetDefinition::codepoints([5].into_iter().collect());

        let Err(err) = PatchGroup::select_next_patches(font, &Default::default(), &s) else {
            panic!("Should have failed")
        };
        assert_eq!(
            err,
            ReadError::MalformedData("Failed to expand url template in format 2 table.")
        );
    }

    #[test]
    fn select_next_patches_ordering_for_non_invalidating() {
        let mut ift_builder = custom_ids_format2();
        ift_builder.write_at("entries[0].id_delta", Int24::new(30)); // delta = +15
        ift_builder.write_at("entries[1].id_delta", Int24::new(-10)); // delta = -5

        let mut iftx_builder = custom_ids_format2();
        iftx_builder.write_at("compat_id[0]", 5u32);
        iftx_builder.write_at("compat_id[1]", 6u32);
        iftx_builder.write_at("compat_id[2]", 7u32);
        iftx_builder.write_at("compat_id[3]", 8u32);

        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([(IFT_TAG, ift_builder.as_slice()), (IFTX_TAG, iftx_builder.as_slice())]),
        );

        let font = FontRef::new(font.as_slice()).unwrap();

        let s = SubsetDefinition::codepoints([10].into_iter().collect());
        let g = PatchGroup::select_next_patches(font, &Default::default(), &s).unwrap();

        // Expected ID ordering
        // IFT
        // 16 (+15)
        // 12 (-5)
        // 21 (+7, +0)
        // IFTX
        // 0
        // 6
        // 15
        // Note: these are in entry order (physical ordering in the map encoding) and
        // not in entry id order
        let urls: Vec<String> = g.urls().map(|p| p.as_ref().to_string()).collect();
        let expected_urls: Vec<String> = [16, 12, 21, 0, 6, 15]
            .into_iter()
            .map(|index| PatchUrl::expand_template(RELATIVE_URL_TEMPLATE, &PatchId::Numeric(index)))
            .map(|url| url.unwrap())
            .map(|url| url.as_ref().to_string())
            .collect();

        assert_eq!(urls, expected_urls);
    }

    #[test]
    fn select_next_patches_ordering_for_non_invalidating_with_duplicates() {
        let mut ift_builder = custom_ids_format2();
        ift_builder.write_at("entries[0].id_delta", Int24::new(30)); // delta = +15
        ift_builder.write_at("entries[1].id_delta", Int24::new(-20)); // delta = -10

        let mut iftx_builder = custom_ids_format2();
        iftx_builder.write_at("compat_id[0]", 5u32);
        iftx_builder.write_at("compat_id[1]", 6u32);
        iftx_builder.write_at("compat_id[2]", 7u32);
        iftx_builder.write_at("compat_id[3]", 8u32);

        let font = test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([(IFT_TAG, ift_builder.as_slice()), (IFTX_TAG, iftx_builder.as_slice())]),
        );

        let font = FontRef::new(font.as_slice()).unwrap();

        let s = SubsetDefinition::codepoints([10].into_iter().collect());
        let g = PatchGroup::select_next_patches(font, &Default::default(), &s).unwrap();

        // Expected ID ordering
        // IFT
        // 16 (+15)
        // 7 (-10)
        // 16 (+7, +0)
        // IFTX
        // 0
        // 6
        // 15
        // Note: these are in entry order (physical ordering in the map encoding) and
        // not in entry id order
        let urls: Vec<String> = g.urls().map(|p| p.as_ref().to_string()).collect();
        let expected_urls: Vec<String> = [16, 7, 0, 6, 15]
            .into_iter()
            .map(|index| PatchUrl::expand_template(RELATIVE_URL_TEMPLATE, &PatchId::Numeric(index)))
            .map(|url| url.unwrap())
            .map(|url| url.as_ref().to_string())
            .collect();

        assert_eq!(urls, expected_urls);
    }
}
