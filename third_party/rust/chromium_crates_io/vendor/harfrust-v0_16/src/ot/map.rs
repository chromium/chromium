use alloc::boxed::Box;
use alloc::vec::Vec;
use core::cmp::Ordering;
use core::ops::Range;

use super::buffer::{Buffer, GlyphFlags};
use super::layout::LayoutTableKind;
use super::{Language, Mask, Script, Tag};
use crate::plan::ShapePlan;
use crate::tag::TagExt;
use crate::{LayoutData, ShaperFont};
use read_fonts::tables::layout::{store_deltas, LookupVariations};
use read_fonts::types::F2Dot14;

// TODO: Remove once MSRV is 1.80+
use core::mem::{size_of, size_of_val};

pub struct OtMap {
    found_script: [bool; 2],
    chosen_script: [Option<Tag>; 2],
    global_mask: Mask,
    features: Vec<FeatureMap>,
    lookups: [Vec<LookupMap>; 2],
    stages: [Vec<StageMap>; 2],
    feature_variations: [Option<u32>; 2],
    /// The location lookup variations were resolved at, if the font has any.
    lookup_coords: Option<Box<[F2Dot14]>>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct FeatureMap {
    tag: Tag,
    // GSUB/GPOS
    index: [Option<u16>; 2],
    stage: [usize; 2],
    shift: u32,
    mask: Mask,
    // mask for value=1, for quick access
    one_mask: Mask,
    needs_fallback: bool,
    auto_zwnj: bool,
    auto_zwj: bool,
    random: bool,
    per_syllable: bool,
}

impl Ord for FeatureMap {
    fn cmp(&self, other: &Self) -> Ordering {
        self.tag.cmp(&other.tag)
    }
}

impl PartialOrd for FeatureMap {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        self.tag.partial_cmp(&other.tag)
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct LookupMap {
    pub index: u16,
    // TODO: to bitflags
    pub auto_zwnj: bool,
    pub auto_zwj: bool,
    pub random: bool,
    pub mask: Mask,
    pub per_syllable: bool,
    #[cfg(feature = "tracing")]
    pub feature_tag: Tag,
}

// HarfBuzz prints tag bytes directly, whereas Tag's Display escapes them.
#[cfg(feature = "tracing")]
pub(crate) struct MessageTag(pub Option<Tag>);

#[cfg(feature = "tracing")]
impl core::fmt::Display for MessageTag {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        use core::fmt::Write;
        if let Some(tag) = self.0 {
            for &byte in tag.as_ref() {
                // Script tags use a C string; feature tags use four %c fields.
                if f.alternate() && byte == 0 {
                    break;
                }
                f.write_char(char::from(byte))?;
            }
        }
        Ok(())
    }
}

#[derive(Clone, Copy)]
pub struct StageMap {
    // Cumulative
    pub last_lookup: usize,
    pub pause_func: Option<PauseFn>,
}

// Pause functions return true if new glyph indices might have been added to the buffer.
// This is used to update buffer digest.
pub type PauseFn = fn(&ShapePlan, &ShaperFont<'_, '_>, &mut Buffer) -> Option<bool>;

impl OtMap {
    pub const MAX_BITS: u32 = 8;
    pub const MAX_VALUE: u32 = (1 << Self::MAX_BITS) - 1;

    #[inline]
    pub fn found_script(&self, table_index: LayoutTableKind) -> bool {
        self.found_script[table_index]
    }

    #[inline]
    pub fn chosen_script(&self, table_index: LayoutTableKind) -> Option<Tag> {
        self.chosen_script[table_index]
    }

    #[inline]
    pub fn get_global_mask(&self) -> Mask {
        self.global_mask
    }

    #[inline]
    pub fn get_mask(&self, feature_tag: Tag) -> (Mask, u32) {
        self.features
            .binary_search_by_key(&feature_tag, |f| f.tag)
            .map_or((0, 0), |idx| {
                (self.features[idx].mask, self.features[idx].shift)
            })
    }

    #[inline]
    pub fn get_1_mask(&self, feature_tag: Tag) -> Mask {
        self.features
            .binary_search_by_key(&feature_tag, |f| f.tag)
            .map_or(0, |idx| self.features[idx].one_mask)
    }

    #[inline]
    pub fn needs_fallback(&self, feature_tag: Tag) -> bool {
        self.features
            .binary_search_by_key(&feature_tag, |f| f.tag)
            .is_ok_and(|idx| self.features[idx].needs_fallback)
    }

    #[inline]
    pub fn get_feature_index(&self, table_index: LayoutTableKind, feature_tag: Tag) -> Option<u16> {
        self.features
            .binary_search_by_key(&feature_tag, |f| f.tag)
            .ok()
            .and_then(|idx| self.features[idx].index[table_index])
    }

    #[inline]
    pub fn get_feature_stage(
        &self,
        table_index: LayoutTableKind,
        feature_tag: Tag,
    ) -> Option<usize> {
        self.features
            .binary_search_by_key(&feature_tag, |f| f.tag)
            .map(|idx| self.features[idx].stage[table_index])
            .ok()
    }

    #[inline]
    pub fn stages(&self, table_index: LayoutTableKind) -> &[StageMap] {
        &self.stages[table_index]
    }

    #[inline]
    pub fn lookup(&self, table_index: LayoutTableKind, index: usize) -> &LookupMap {
        &self.lookups[table_index][index]
    }

    #[inline]
    pub fn stage_lookups(&self, table_index: LayoutTableKind, stage: usize) -> &[LookupMap] {
        &self.lookups[table_index][self.stage_lookup_range(table_index, stage)]
    }

    #[inline]
    pub fn stage_lookup_range(&self, table_index: LayoutTableKind, stage: usize) -> Range<usize> {
        let stages = &self.stages[table_index];
        let lookups = &self.lookups[table_index];
        let start = stage
            .checked_sub(1)
            .map_or(0, |prev| stages[prev].last_lookup);
        let end = stages
            .get(stage)
            .map_or(lookups.len(), |curr| curr.last_lookup);
        start..end
    }

    pub fn feature_variations(&self) -> &[Option<u32>; 2] {
        &self.feature_variations
    }

    pub fn lookup_coords(&self) -> Option<&[F2Dot14]> {
        self.lookup_coords.as_deref()
    }
}

pub type MapFeatureFlags = u32;
pub const F_NONE: u32 = 0x0000;
pub const F_GLOBAL: u32 = 0x0001; /* Feature applies to all characters; results in no mask allocated for it. */
pub const F_HAS_FALLBACK: u32 = 0x0002; /* Has fallback implementation, so include mask bit even if feature not found. */
pub const F_MANUAL_ZWNJ: u32 = 0x0004; /* Don't skip over ZWNJ when matching **context**. */
pub const F_MANUAL_ZWJ: u32 = 0x0008; /* Don't skip over ZWJ when matching **input**. */
pub const F_MANUAL_JOINERS: u32 = F_MANUAL_ZWNJ | F_MANUAL_ZWJ;
pub const F_GLOBAL_MANUAL_JOINERS: u32 = F_GLOBAL | F_MANUAL_JOINERS;
pub const F_GLOBAL_HAS_FALLBACK: u32 = F_GLOBAL | F_HAS_FALLBACK;
pub const F_GLOBAL_SEARCH: u32 = 0x0010; /* If feature not found in LangSys, look for it in global feature list and pick one. */
pub const F_RANDOM: u32 = 0x0020; /* Randomly select a glyph from an AlternateSubstFormat1 subtable. */
pub const F_PER_SYLLABLE: u32 = 0x0040; /* Contain lookup application to within syllable. */

pub struct OtMapBuilder<'a> {
    layout: LayoutData<'a>,
    found_script: [bool; 2],
    script_index: [Option<u16>; 2],
    chosen_script: [Option<Tag>; 2],
    lang_index: [Option<u16>; 2],
    current_stage: [usize; 2],
    feature_infos: Vec<FeatureInfo>,
    stages: [Vec<StageInfo>; 2],
    pub(crate) is_simple: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
struct FeatureInfo {
    tag: Tag,
    // sequence number, used for stable sorting only
    seq: usize,
    max_value: u32,
    flags: MapFeatureFlags,
    // for non-global features, what should the unset glyphs take
    default_value: u32,
    // GSUB/GPOS
    stage: [usize; 2],
}

#[derive(Clone, Copy)]
struct StageInfo {
    index: usize,
    pause_func: Option<PauseFn>,
}

/// The lookups of a table, collected stage by stage.
///
/// Features may share lookups and list one any number of times, so the
/// references of a stage are merged as they are added: a stage holds each
/// lookup of the table at most once, whatever the features list.
struct LookupCollector {
    lookups: Vec<LookupMap>,
    /// Where the current stage starts in `lookups`.
    stage_start: usize,
    /// Per lookup index of the table: its entry in `lookups`, and what
    /// merging its references of the current stage so far gives.
    merged: Vec<MergedLookup>,
}

#[derive(Clone, Copy)]
struct MergedLookup {
    position: usize,
    mask: Mask,
    auto_zwnj: bool,
    auto_zwj: bool,
}

impl LookupCollector {
    fn new(lookup_count: u16) -> Self {
        let unused = MergedLookup {
            position: usize::MAX,
            mask: 0,
            auto_zwnj: true,
            auto_zwj: true,
        };
        Self {
            lookups: Vec::new(),
            stage_start: 0,
            merged: alloc::vec![unused; usize::from(lookup_count)],
        }
    }

    /// Adds a reference to a lookup from the current stage; one to a lookup
    /// the table does not have is ignored.
    fn add(&mut self, lookup: LookupMap) {
        let Some(merged) = self.merged.get_mut(usize::from(lookup.index)) else {
            return;
        };
        match self.lookups.get_mut(merged.position) {
            Some(first) if merged.position >= self.stage_start => {
                // Keep the reference that sorts first, as sorting the stage
                // and merging adjacent duplicates would.
                if lookup < *first {
                    *first = lookup;
                }
                merged.mask |= lookup.mask;
                merged.auto_zwnj &= lookup.auto_zwnj;
                merged.auto_zwj &= lookup.auto_zwj;
            }
            _ => {
                *merged = MergedLookup {
                    position: self.lookups.len(),
                    mask: lookup.mask,
                    auto_zwnj: lookup.auto_zwnj,
                    auto_zwj: lookup.auto_zwj,
                };
                self.lookups.push(lookup);
            }
        }
    }

    /// Gives the lookups of the current stage their merged masks and flags,
    /// sorts them, and returns where the next stage starts.
    fn finish_stage(&mut self) -> usize {
        let stage = &mut self.lookups[self.stage_start..];
        for lookup in stage.iter_mut() {
            let merged = self.merged[usize::from(lookup.index)];
            lookup.mask = merged.mask;
            lookup.auto_zwnj = merged.auto_zwnj;
            lookup.auto_zwj = merged.auto_zwj;
        }
        crate::algs::sort_unstable_by(stage, Ord::cmp);
        self.stage_start = self.lookups.len();
        self.stage_start
    }
}

const GLOBAL_BIT_SHIFT: u32 = 8 * size_of::<u32>() as u32 - 1;
const GLOBAL_BIT_MASK: Mask = 1 << GLOBAL_BIT_SHIFT;

/// The most lookup indices the lookup variations of a feature may add:
/// one for each value a lookup index can take.
const MAX_LOOKUP_VARIATION_INDICES: usize = 1 << 16;

impl<'a> OtMapBuilder<'a> {
    pub fn new(
        layout: LayoutData<'a>,
        script: Option<Script>,
        language: Option<&Language>,
    ) -> Self {
        // Fetch script/language indices for GSUB/GPOS.  We need these later to skip
        // features not available in either table and not waste precious bits for them.
        let (script_tags, lang_tags) = Script::tags_for_language(script, language);

        let mut found_script = [false; 2];
        let mut script_index = [None; 2];
        let mut chosen_script = [None; 2];
        let mut lang_index = [None; 2];

        for (table_index, table) in layout.ot.layout_tables() {
            if let Some((found, idx, tag)) = table.select_script(script_tags.as_slice()) {
                chosen_script[table_index] = Some(tag);
                found_script[table_index] = found;
                script_index[table_index] = Some(idx);

                if let Some(idx) = table.select_script_language(idx, lang_tags.as_slice()) {
                    lang_index[table_index] = Some(idx);
                }
            }
        }

        Self {
            layout,
            found_script,
            script_index,
            chosen_script,
            lang_index,
            current_stage: [0, 0],
            feature_infos: Vec::new(),
            stages: [Vec::new(), Vec::new()],
            is_simple: false,
        }
    }

    #[inline]
    pub fn chosen_script(&self, table_index: LayoutTableKind) -> Option<Tag> {
        self.chosen_script[table_index]
    }

    #[inline]
    pub fn has_feature(&self, tag: Tag) -> bool {
        for (table_index, table) in self.layout.ot.layout_tables() {
            if let Some(script_index) = self.script_index[table_index] {
                if table
                    .find_language_feature(script_index, self.lang_index[table_index], tag)
                    .is_some()
                {
                    return true;
                }
            }
        }

        false
    }

    #[inline]
    pub fn add_feature(&mut self, tag: Tag, flags: MapFeatureFlags, value: u32) {
        if !tag.is_null() {
            let seq = self.feature_infos.len();
            self.feature_infos.push(FeatureInfo {
                tag,
                seq,
                max_value: value,
                flags,
                default_value: if flags & F_GLOBAL != 0 { value } else { 0 },
                stage: self.current_stage,
            });
        }
    }

    #[inline]
    pub fn enable_feature(&mut self, tag: Tag, flags: MapFeatureFlags, value: u32) {
        self.add_feature(tag, flags | F_GLOBAL, value);
    }

    #[inline]
    pub fn disable_feature(&mut self, tag: Tag) {
        self.add_feature(tag, F_GLOBAL, 0);
    }

    #[inline]
    pub fn add_gsub_pause(&mut self, pause: Option<PauseFn>) {
        self.add_pause(LayoutTableKind::Gsub, pause);
    }

    #[inline]
    pub fn add_gpos_pause(&mut self, pause: Option<PauseFn>) {
        self.add_pause(LayoutTableKind::Gpos, pause);
    }

    fn add_pause(&mut self, table_index: LayoutTableKind, pause: Option<PauseFn>) {
        self.stages[table_index].push(StageInfo {
            index: self.current_stage[table_index],
            pause_func: pause,
        });

        self.current_stage[table_index] += 1;
    }

    pub fn compile(&mut self) -> OtMap {
        // We default to applying required feature in stage 0.  If the required
        // feature has a tag that is known to the shaper, we apply required feature
        // in the stage for that tag.
        let mut required_index = [None; 2];
        let mut required_tag = [None; 2];

        for (table_index, table) in self.layout.ot.layout_tables() {
            if let Some(script) = self.script_index[table_index] {
                let lang = self.lang_index[table_index];
                if let Some((idx, tag)) = table.get_required_language_feature(script, lang) {
                    required_index[table_index] = Some(idx);
                    required_tag[table_index] = Some(tag);
                }
            }
        }

        let (features, required_stage, global_mask) = self.collect_feature_maps(required_tag);

        self.add_gsub_pause(None);
        self.add_gpos_pause(None);

        let (lookups, stages) =
            self.collect_lookup_stages(&features, required_index, required_stage);

        OtMap {
            found_script: self.found_script,
            chosen_script: self.chosen_script,
            global_mask,
            features,
            lookups,
            stages,
            feature_variations: self.layout.ot.feature_variations,
            lookup_coords: self
                .layout
                .ot
                .has_lookup_variations
                .then(|| self.layout.ot.coords.into()),
        }
    }

    fn collect_feature_maps(
        &mut self,
        required_tag: [Option<Tag>; 2],
    ) -> (Vec<FeatureMap>, [usize; 2], Mask) {
        let mut map_features = Vec::new();
        let mut required_stage = [0; 2];
        let mut global_mask = GLOBAL_BIT_MASK;
        let mut next_bit = GlyphFlags::DEFINED_BITS.count_ones() + 1;

        // Sort features and merge duplicates.
        self.dedup_feature_infos();

        let language_feature_indices = self.find_language_features();

        for (info, language_feature_index) in
            self.feature_infos.iter().zip(language_feature_indices)
        {
            let bits_needed = if info.flags & F_GLOBAL != 0 && info.max_value == 1 {
                // Uses the global bit.
                0
            } else {
                // Limit bits per feature.
                let v = info.max_value;
                let num_bits = 8 * size_of_val(&v) as u32 - v.leading_zeros();
                OtMap::MAX_BITS.min(num_bits)
            };

            if info.max_value == 0 || next_bit + bits_needed >= GLOBAL_BIT_SHIFT {
                // Feature disabled, or not enough bits.
                continue;
            }

            let mut found = false;
            let mut feature_index = [None; 2];

            for (table_index, _) in self.layout.ot.layout_tables() {
                if required_tag[table_index] == Some(info.tag) {
                    required_stage[table_index] = info.stage[table_index];
                }

                if let Some(idx) = language_feature_index[table_index] {
                    feature_index[table_index] = Some(idx);
                    found = true;
                }
            }

            if !found && info.flags & F_GLOBAL_SEARCH != 0 {
                // hb_ot_layout_table_find_feature
                for (table_index, table) in self.layout.ot.layout_tables() {
                    if let Some(idx) = table.feature_index(info.tag) {
                        feature_index[table_index] = Some(idx);
                        found = true;
                    }
                }
            }

            if !found && !info.flags & F_HAS_FALLBACK != 0 {
                continue;
            }

            let (shift, mask) = if info.flags & F_GLOBAL != 0 && info.max_value == 1 {
                // Uses the global bit
                (GLOBAL_BIT_SHIFT, GLOBAL_BIT_MASK)
            } else {
                let shift = next_bit;
                let mask = (1 << (next_bit + bits_needed)) - (1 << next_bit);
                next_bit += bits_needed;
                global_mask |= (info.default_value << shift) & mask;
                (shift, mask)
            };

            map_features.push(FeatureMap {
                tag: info.tag,
                index: feature_index,
                stage: info.stage,
                shift,
                mask,
                one_mask: (1 << shift) & mask,
                needs_fallback: !found,
                auto_zwnj: info.flags & F_MANUAL_ZWNJ == 0,
                auto_zwj: info.flags & F_MANUAL_ZWJ == 0,
                random: info.flags & F_RANDOM != 0,
                per_syllable: info.flags & F_PER_SYLLABLE != 0,
            });
        }

        if self.is_simple {
            crate::algs::sort_unstable_by(&mut map_features, Ord::cmp);
        }

        (map_features, required_stage, global_mask)
    }

    /// Finds every feature info in the chosen language system of each table,
    /// as [`LayoutTable::find_language_feature`] would one by one.
    ///
    /// A language system can list 65535 feature indices and the caller can
    /// request any number of features, so rather than searching the list
    /// once per feature, this walks it once per table and looks each listed
    /// tag up among the requested ones.
    ///
    /// [`LayoutTable::find_language_feature`]: super::LayoutTable::find_language_feature
    fn find_language_features(&self) -> Vec<[Option<u16>; 2]> {
        // The infos are only sorted by tag when `is_simple` is unset.
        let mut tags: Vec<(Tag, usize)> = self
            .feature_infos
            .iter()
            .enumerate()
            .map(|(i, info)| (info.tag, i))
            .collect();
        crate::algs::sort_unstable_by(&mut tags, Ord::cmp);

        let mut indices = alloc::vec![[None; 2]; self.feature_infos.len()];
        for (table_index, table) in self.layout.ot.layout_tables() {
            let Some(script) = self.script_index[table_index] else {
                continue;
            };
            let lang = self.lang_index[table_index];
            for (idx, tag) in table.language_features(script, lang) {
                let start = tags.partition_point(|&(t, _)| t < tag);
                for &(_, i) in tags[start..].iter().take_while(|&&(t, _)| t == tag) {
                    // Like the search, keep the first index listed with the tag.
                    indices[i][table_index].get_or_insert(idx);
                }
            }
        }
        indices
    }

    fn dedup_feature_infos(&mut self) {
        let feature_infos = &mut self.feature_infos;
        if feature_infos.is_empty() {
            return;
        }

        if !self.is_simple {
            crate::algs::sort_unstable_by(feature_infos, Ord::cmp);
        }

        let mut j = 0;
        for i in 1..feature_infos.len() {
            if feature_infos[i].tag != feature_infos[j].tag {
                j += 1;
                feature_infos[j] = feature_infos[i];
            } else {
                if feature_infos[i].flags & F_GLOBAL != 0 {
                    feature_infos[j].flags |= F_GLOBAL;
                    feature_infos[j].max_value = feature_infos[i].max_value;
                    feature_infos[j].default_value = feature_infos[i].default_value;
                } else {
                    if feature_infos[j].flags & F_GLOBAL != 0 {
                        feature_infos[j].flags ^= F_GLOBAL;
                    }
                    feature_infos[j].max_value =
                        feature_infos[j].max_value.max(feature_infos[i].max_value);
                    // Inherit default_value from j
                }
                let flags = feature_infos[i].flags & F_HAS_FALLBACK;
                feature_infos[j].flags |= flags;
                feature_infos[j].stage[0] =
                    feature_infos[j].stage[0].min(feature_infos[i].stage[0]);
                feature_infos[j].stage[1] =
                    feature_infos[j].stage[1].min(feature_infos[i].stage[1]);
            }
        }

        feature_infos.truncate(j + 1);
    }

    fn collect_lookup_stages(
        &self,
        map_features: &[FeatureMap],
        required_feature_index: [Option<u16>; 2],
        required_feature_stage: [usize; 2],
    ) -> ([Vec<LookupMap>; 2], [Vec<StageMap>; 2]) {
        let mut map_lookups = [Vec::new(), Vec::new()];
        let mut map_stages = [Vec::new(), Vec::new()];

        for table_index in LayoutTableKind::iter() {
            // Collect lookup indices for features.
            let mut stage_index = 0;
            let lookup_count = self
                .layout
                .ot
                .layout_table(table_index)
                .map_or(0, |table| table.lookup_count());
            let mut lookups = LookupCollector::new(lookup_count);

            let variation_index = self.layout.ot.feature_variations[table_index as usize];
            let lookup_variations = if self.layout.ot.has_lookup_variations {
                self.layout
                    .ot
                    .layout_table(table_index)
                    .and_then(|table| table.lookup_variations())
            } else {
                None
            };

            for stage in 0..self.current_stage[table_index] {
                if let Some(feature_index) = required_feature_index[table_index] {
                    if required_feature_stage[table_index] == stage {
                        self.add_lookups(
                            &mut lookups,
                            table_index,
                            feature_index,
                            variation_index,
                            lookup_variations.as_ref(),
                            GLOBAL_BIT_MASK,
                            true,
                            true,
                            false,
                            false,
                            #[cfg(feature = "tracing")]
                            Tag::new(b"    "),
                        );
                    }
                }

                for feature in map_features {
                    if let Some(feature_index) = feature.index[table_index] {
                        if feature.stage[table_index] == stage {
                            self.add_lookups(
                                &mut lookups,
                                table_index,
                                feature_index,
                                variation_index,
                                lookup_variations.as_ref(),
                                feature.mask,
                                feature.auto_zwnj,
                                feature.auto_zwj,
                                feature.random,
                                feature.per_syllable,
                                #[cfg(feature = "tracing")]
                                feature.tag,
                            );
                        }
                    }
                }

                // Sort lookups; duplicates were merged as they were added.
                let last_lookup = lookups.finish_stage();

                if let Some(info) = self.stages[table_index].get(stage_index) {
                    if info.index == stage {
                        map_stages[table_index].push(StageMap {
                            last_lookup,
                            pause_func: info.pause_func,
                        });

                        stage_index += 1;
                    }
                }
            }

            map_lookups[table_index] = lookups.lookups;
        }

        (map_lookups, map_stages)
    }

    fn add_lookups(
        &self,
        lookups: &mut LookupCollector,
        table_index: LayoutTableKind,
        feature_index: u16,
        variation_index: Option<u32>,
        lookup_variations: Option<&LookupVariations>,
        mask: Mask,
        auto_zwnj: bool,
        auto_zwj: bool,
        random: bool,
        per_syllable: bool,
        #[cfg(feature = "tracing")] feature_tag: Tag,
    ) -> Option<()> {
        let table = self.layout.ot.layout_table(table_index)?;

        let feature = match variation_index {
            Some(idx) => table
                .feature_substitution(idx, feature_index)
                .or_else(|| table.feature(feature_index))?,
            None => table.feature(feature_index)?,
        };

        let mut add_lookup = |index: u16| {
            lookups.add(LookupMap {
                index,
                auto_zwnj,
                auto_zwj,
                random,
                mask,
                per_syllable,
                #[cfg(feature = "tracing")]
                feature_tag,
            });
        };
        let indices = feature.lookup_list_indices();
        // Condition records may name overlapping lookup index lists, so what
        // they add is not bounded by the table's size. Lookup variations that
        // could add more indices than there are lookup index values are
        // ignored, as nothing a feature can use needs that many.
        let Some(lookup_variations) = lookup_variations
            .filter(|_| table.lookup_variation_len(feature_index) <= MAX_LOOKUP_VARIATION_INDICES)
        else {
            for index in indices {
                add_lookup(index.get());
            }
            return Some(());
        };
        // The feature's lookups at this location: they replace or extend
        // those of the feature, or of the alternate selected above.
        let mut resolved: Vec<u16> = indices.iter().map(|index| index.get()).collect();
        let ot = &self.layout.ot;
        lookup_variations.apply(
            feature_index,
            ot.coords,
            store_deltas(ot.var_store(), ot.coords),
            &mut resolved,
        );
        for index in resolved {
            add_lookup(index);
        }

        Some(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{Direction, Feature, Font};
    use alloc::sync::Arc;
    use read_fonts::{
        model::{Blob, TableFunction},
        FontRef, TableProvider,
    };

    fn u16s(values: &[u16]) -> Vec<u8> {
        values
            .iter()
            .flat_map(|value| value.to_be_bytes())
            .collect()
    }

    fn put16(bytes: &mut [u8], position: usize, value: usize) {
        bytes[position..position + 2].copy_from_slice(&u16::try_from(value).unwrap().to_be_bytes());
    }

    /// A GSUB with two lookups, where every one of `features` shares one
    /// Feature table listing lookup 0 `repeats` times and then lookup 1.
    fn gsub(features: &[Tag], repeats: usize) -> Vec<u8> {
        let count = u16::try_from(features.len()).unwrap();
        let mut bytes = u16s(&[1, 0, 10, 0, 0]);
        // DFLT script whose default language system uses every feature.
        bytes.extend_from_slice(&u16s(&[1, 0x4446, 0x4C54, 8, 4, 0, 0, 0xFFFF, count]));
        bytes.extend((0..count).flat_map(u16::to_be_bytes));
        let list = bytes.len();
        put16(&mut bytes, 6, list);
        bytes.extend_from_slice(&u16s(&[count]));
        let feature = 2 + 6 * features.len();
        for tag in features {
            bytes.extend_from_slice(&tag.to_be_bytes());
            bytes.extend_from_slice(&u16s(&[u16::try_from(feature).unwrap()]));
        }
        bytes.extend_from_slice(&u16s(&[0, u16::try_from(repeats + 1).unwrap()]));
        bytes.extend(
            core::iter::repeat_n(0u16, repeats)
                .chain([1])
                .flat_map(u16::to_be_bytes),
        );
        let list = bytes.len();
        put16(&mut bytes, 8, list);
        // Two single substitutions with empty coverage.
        bytes.extend_from_slice(&u16s(&[2, 6, 24]));
        for _ in 0..2 {
            bytes.extend_from_slice(&u16s(&[1, 0, 1, 8, 1, 6, 0, 1, 0]));
        }
        bytes
    }

    /// A GSUB whose DFLT script lists `indices` of `features` in its default
    /// language system, with no lookups.
    fn language_system_gsub(features: &[&[u8; 4]], indices: &[u16]) -> Vec<u8> {
        let feature_list = 10 + 8 + 4 + 6 + 2 * indices.len();
        let lookup_list = feature_list + 2 + 6 * features.len() + 4 * features.len();
        let mut bytes = u16s(&[1, 0, 10, feature_list as u16, lookup_list as u16]);
        // DFLT script, with a default language system and no others.
        bytes.extend_from_slice(&u16s(&[1, 0x4446, 0x4C54, 8, 4, 0]));
        bytes.extend_from_slice(&u16s(&[0, 0xFFFF, indices.len() as u16]));
        bytes.extend_from_slice(&u16s(indices));
        assert_eq!(bytes.len(), feature_list);
        bytes.extend_from_slice(&u16s(&[features.len() as u16]));
        for (index, tag) in features.iter().enumerate() {
            let offset = 2 + 6 * features.len() + 4 * index;
            bytes.extend_from_slice(tag.as_slice());
            bytes.extend_from_slice(&u16s(&[offset as u16]));
        }
        for _ in features {
            bytes.extend_from_slice(&u16s(&[0, 0]));
        }
        assert_eq!(bytes.len(), lookup_list);
        bytes.extend_from_slice(&u16s(&[0]));
        bytes
    }

    fn font(gsub: Vec<u8>) -> Font {
        let source =
            FontRef::new(include_bytes!("../../benches/fonts/Roboto-Regular.ttf")).unwrap();
        let tables = TableFunction::new(Arc::new(move |tag| {
            if tag == Tag::new(b"GSUB") {
                return Some(Blob::from(gsub.clone()));
            }
            source
                .data_for_tag(tag)
                .map(|data| Blob::from(data.as_bytes().to_vec()))
        }));
        Font::new(tables, 0).unwrap()
    }

    /// The GSUB feature index a plan for `font` resolves `tag` to.
    fn feature_index(font: &Font, user_features: &[Feature], tag: &[u8; 4]) -> Option<u16> {
        let plan = ShapePlan::new(font, Direction::LeftToRight, None, None, user_features);
        plan.ot_map
            .get_feature_index(LayoutTableKind::Gsub, Tag::new(tag))
    }

    #[test]
    fn language_features_resolve_like_a_search_of_the_language_system() {
        // Index 9 is beyond the feature list, `calt` is not listed, and the
        // `liga` listed first is not the lower index.
        let font = font(language_system_gsub(
            &[b"liga", b"liga", b"ss01", b"kern", b"calt"],
            &[9, 2, 1, 0, 3],
        ));
        let ss01 = [Feature::new(Tag::new(b"ss01"), 1, ..)];
        assert_eq!(feature_index(&font, &[], b"liga"), Some(1));
        assert_eq!(feature_index(&font, &[], b"kern"), Some(3));
        assert_eq!(feature_index(&font, &[], b"calt"), None);
        assert_eq!(feature_index(&font, &[], b"ss01"), None);
        assert_eq!(feature_index(&font, &ss01, b"ss01"), Some(2));
        assert_eq!(feature_index(&font, &ss01, b"liga"), Some(1));
    }

    #[test]
    fn empty_language_system_finds_no_features() {
        let font = font(language_system_gsub(&[b"liga"], &[]));
        assert_eq!(feature_index(&font, &[], b"liga"), None);
    }

    #[test]
    fn shared_feature_tables_do_not_multiply_lookups() {
        // Features the default shaper enables, and ones the user requests.
        let user: Vec<Tag> = (1..=20)
            .map(|n| Tag::from_be_bytes([b's', b's', b'0' + n / 10, b'0' + n % 10]))
            .collect();
        let mut features = alloc::vec![
            Tag::new(b"ccmp"),
            Tag::new(b"locl"),
            Tag::new(b"calt"),
            Tag::new(b"liga"),
        ];
        features.extend_from_slice(&user);
        let repeats = 4096;
        let font = font(gsub(&features, repeats));
        let user_features: Vec<Feature> =
            user.iter().map(|&tag| Feature::new(tag, 1, ..)).collect();

        let plan = ShapePlan::new(&font, Direction::LeftToRight, None, None, &user_features);
        let lookups = &plan.ot_map.lookups[LayoutTableKind::Gsub];
        // Each lookup once, merged from every feature as it was added: the
        // list never holds (features × listed lookups) entries.
        let indices: Vec<u16> = lookups.iter().map(|lookup| lookup.index).collect();
        assert_eq!(indices, [0, 1]);
        assert!(lookups
            .iter()
            .all(|lookup| lookup.mask & GLOBAL_BIT_MASK != 0));
        assert!(
            lookups.capacity() < repeats,
            "capacity {}",
            lookups.capacity()
        );
    }
}
