//! Shaping support for autohinting.

use super::style::{GlyphStyle, StyleClass};
use crate::{charmap::Charmap, collections::SmallVec, FontRef, GlyphId, MetadataProvider};
use core::ops::Range;
use raw::{
    tables::{
        gsub::{
            ChainedSequenceContext, Gsub, SequenceContext, SingleSubst, SubstitutionLookupList,
            SubstitutionSubtables,
        },
        layout::ScriptTags,
        varc::CoverageTable,
    },
    types::Tag,
    ReadError, TableProvider,
};

/// Determines the fidelity with which we apply shaping in the
/// autohinter.
///
/// Shaping only affects glyph style classification and the glyphs that
/// are chosen for metrics computations. We keep the `Nominal` mode around
/// to enable validation of internal algorithms against a configuration that
/// is known to match FreeType. The `BestEffort` mode should always be
/// used for actual rendering.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub(crate) enum ShaperMode {
    /// Characters are mapped to nominal glyph identifiers and layout tables
    /// are not used for style coverage.
    ///
    /// This matches FreeType when HarfBuzz support is not enabled.
    Nominal,
    /// Simple substitutions are applied according to script rules and layout
    /// tables are used to extend style coverage beyond the character map.
    #[allow(unused)]
    BestEffort,
}

#[derive(Copy, Clone, Default, Debug)]
pub(crate) struct ShapedGlyph {
    pub id: GlyphId,
    /// This may be used for computing vertical alignment zones, particularly
    /// for glyphs like super/subscripts which might have adjustments in GPOS.
    ///
    /// Note that we don't do the same in the horizontal direction which
    /// means that we don't care about the x-offset.
    pub y_offset: i32,
}

/// Arbitrarily chosen to cover our max input size plus some extra to account
/// for expansion from multiple substitution tables.
const SHAPED_CLUSTER_INLINE_SIZE: usize = 16;

/// Container for storing the result of shaping a cluster.
///
/// Some of our input "characters" for metrics computations are actually
/// multi-character [grapheme clusters](https://www.unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries)
/// that may expand to multiple glyphs.
pub(crate) type ShapedCluster = SmallVec<ShapedGlyph, SHAPED_CLUSTER_INLINE_SIZE>;

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub(crate) enum ShaperCoverageKind {
    /// Shaper coverage that traverses a specific script.
    Script,
    /// Shaper coverage that also includes the `Dflt` script.
    ///
    /// This is used as a catch all after all styles are processed.
    Default,
}

/// Maps characters to glyphs and handles extended style coverage beyond
/// glyphs that are available in the character map.
///
/// Roughly covers the functionality in <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afshaper.c>.
pub(crate) struct Shaper<'a> {
    font: FontRef<'a>,
    #[allow(unused)]
    mode: ShaperMode,
    charmap: Charmap<'a>,
    gsub: Option<Gsub<'a>>,
}

impl<'a> Shaper<'a> {
    pub fn new(font: &FontRef<'a>, mode: ShaperMode) -> Self {
        let charmap = font.charmap();
        let gsub = (mode != ShaperMode::Nominal)
            .then(|| font.gsub().ok())
            .flatten();
        Self {
            font: font.clone(),
            mode,
            charmap,
            gsub,
        }
    }

    pub fn font(&self) -> &FontRef<'a> {
        &self.font
    }

    pub fn charmap(&self) -> &Charmap<'a> {
        &self.charmap
    }

    /// Shapes the given input text with the current mode and stores the
    /// resulting glyphs in the output cluster.
    pub fn shape_cluster(&self, input: &str, output: &mut ShapedCluster) {
        output.clear();
        for (i, ch) in input.chars().enumerate() {
            if i > 0 {
                // In nominal mode, we reject input clusters with multiple
                // characters
                // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afshaper.c#L639>
                output.clear();
                return;
            }
            output.push(ShapedGlyph {
                id: self.charmap.map(ch).unwrap_or_default(),
                y_offset: 0,
            });
        }
    }

    /// Uses layout tables to compute coverage for the given style.
    ///
    /// Returns `true` if any glyph styles were updated for this style.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afshaper.c#L99>
    pub(crate) fn compute_coverage(
        &self,
        style: &StyleClass,
        coverage_kind: ShaperCoverageKind,
        glyph_styles: &mut [GlyphStyle],
    ) -> bool {
        let Some(gsub) = self.gsub.as_ref() else {
            return false;
        };
        let (Ok(script_list), Ok(feature_list), Ok(lookup_list)) =
            (gsub.script_list(), gsub.feature_list(), gsub.lookup_list())
        else {
            return false;
        };
        let mut script_tags: [Option<Tag>; 3] = [None; 3];
        for (a, b) in script_tags
            .iter_mut()
            .zip(ScriptTags::from_unicode(style.script.tag).iter())
        {
            *a = Some(*b);
        }
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afshaper.c#L153>
        const DEFAULT_SCRIPT: Tag = Tag::new(b"Dflt");
        if coverage_kind == ShaperCoverageKind::Default {
            if script_tags[0].is_none() {
                script_tags[0] = Some(DEFAULT_SCRIPT);
            } else if script_tags[1].is_none() {
                script_tags[1] = Some(DEFAULT_SCRIPT);
            } else if script_tags[1] != Some(DEFAULT_SCRIPT) {
                script_tags[2] = Some(DEFAULT_SCRIPT);
            }
        } else {
            // Script classes contain some non-standard tags used for special
            // purposes. We ignore these
            // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afshaper.c#L167>
            const NON_STANDARD_TAGS: &[Option<Tag>] = &[
                // Khmer symbols
                Some(Tag::new(b"Khms")),
                // Latin subscript fallbacks
                Some(Tag::new(b"Latb")),
                // Latin superscript fallbacks
                Some(Tag::new(b"Latp")),
            ];
            if NON_STANDARD_TAGS.contains(&script_tags[0]) {
                return false;
            }
        }
        // Check each requested script that is available in GSUB
        let mut gsub_handler = GsubHandler::new(&self.charmap, &lookup_list, style, glyph_styles);
        for script in script_tags.iter().filter_map(|tag| {
            tag.and_then(|tag| script_list.index_for_tag(tag))
                .and_then(|ix| script_list.script_records().get(ix as usize))
                .and_then(|rec| rec.script(script_list.offset_data()).ok())
        }) {
            // And all language systems for each script
            for langsys in script
                .lang_sys_records()
                .iter()
                .filter_map(|rec| rec.lang_sys(script.offset_data()).ok())
                .chain(script.default_lang_sys().transpose().ok().flatten())
            {
                for feature_ix in langsys.feature_indices() {
                    let Some(feature) = feature_list
                        .feature_records()
                        .get(feature_ix.get() as usize)
                        .and_then(|rec| {
                            // If our style has a feature tag, we only look at that specific
                            // feature; otherwise, handle all of them
                            if style.feature == Some(rec.feature_tag()) || style.feature.is_none() {
                                rec.feature(feature_list.offset_data()).ok()
                            } else {
                                None
                            }
                        })
                    else {
                        continue;
                    };
                    // And now process associated lookups
                    for index in feature.lookup_list_indices().iter() {
                        gsub_handler.process_lookup(index.get(), 0);
                    }
                }
            }
        }
        if let Some(range) = gsub_handler.finish() {
            // If we get a range then we captured at least some glyphs so
            // let's try to assign our current style
            let mut result = false;
            for glyph_style in &mut glyph_styles[range] {
                // We only want to return true here if we actually assign the
                // style to avoid computing unnecessary metrics
                result |= glyph_style.maybe_assign_gsub_output_style(style);
            }
            result
        } else {
            false
        }
    }
}

/// Captures glyphs from the GSUB table that aren't present in cmap.
///
/// FreeType does this in a few phases:
/// 1. Collect all lookups for a given set of scripts and features.
///    <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afshaper.c#L174>
/// 2. For each lookup, collect all _output_ glyphs.
///    <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afshaper.c#L201>
/// 3. If the style represents a specific feature, make sure at least one of
///    the characters in the associated blue string would be substituted by
///    those lookups. If none would be substituted, then we don't assign the
///    style to any glyphs because we don't have any modified alignment
///    zones.
///    <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afshaper.c#L264>
///
/// We roll these into one pass over the lookups below so that we don't have
/// to allocate a lookup set or iterate them twice. Note that since
/// substitutions are checked for individual characters, we ignore ligatures
/// and contextual lookups (and alternates since they aren't applicable).
struct GsubHandler<'a> {
    charmap: &'a Charmap<'a>,
    lookup_list: &'a SubstitutionLookupList<'a>,
    style: &'a StyleClass,
    glyph_styles: &'a mut [GlyphStyle],
    // Set to true when we need to check if any substitutions are available
    // for our blue strings. This is the case when style.feature != None
    need_blue_substs: bool,
    // Keep track of our range of touched gids in the style list
    min_gid: usize,
    max_gid: usize,
}

impl<'a> GsubHandler<'a> {
    fn new(
        charmap: &'a Charmap<'a>,
        lookup_list: &'a SubstitutionLookupList,
        style: &'a StyleClass,
        glyph_styles: &'a mut [GlyphStyle],
    ) -> Self {
        let min_gid = glyph_styles.len();
        // If we have a feature, then we need to check the blue string to see
        // if any substitutions are available. If not, we don't enable this
        // style because it won't have any affect on alignment zones
        let need_blue_substs = style.feature.is_some();
        Self {
            charmap,
            lookup_list,
            style,
            glyph_styles,
            need_blue_substs,
            min_gid,
            max_gid: 0,
        }
    }

    fn process_lookup(&mut self, lookup_index: u16, nesting_depth: u32) {
        // To prevent infinite recursion in contextual lookups. Matches HB
        // <https://github.com/harfbuzz/harfbuzz/blob/c7ef6a2ed58ae8ec108ee0962bef46f42c73a60c/src/hb-limits.hh#L53>
        const MAX_NESTING_DEPTH: u32 = 64;
        if nesting_depth > MAX_NESTING_DEPTH {
            return;
        }
        let Ok(subtables) = self
            .lookup_list
            .lookups()
            .get(lookup_index as usize)
            .and_then(|lookup| lookup.subtables())
        else {
            return;
        };
        match subtables {
            SubstitutionSubtables::Single(tables) => {
                for table in tables.iter().filter_map(|table| table.ok()) {
                    match table {
                        SingleSubst::Format1(table) => {
                            let Ok(coverage) = table.coverage() else {
                                continue;
                            };
                            let delta = table.delta_glyph_id() as i32;
                            for gid in coverage.iter() {
                                self.capture_glyph((gid.to_u32() as i32 + delta) as u16 as u32);
                            }
                            // Check input coverage for blue strings if
                            // required and if we're not under a contextual
                            // lookup
                            if self.need_blue_substs && nesting_depth == 0 {
                                self.check_blue_coverage(Ok(coverage));
                            }
                        }
                        SingleSubst::Format2(table) => {
                            for gid in table.substitute_glyph_ids() {
                                self.capture_glyph(gid.get().to_u32());
                            }
                            // See above
                            if self.need_blue_substs && nesting_depth == 0 {
                                self.check_blue_coverage(table.coverage());
                            }
                        }
                    }
                }
            }
            SubstitutionSubtables::Multiple(tables) => {
                for table in tables.iter().filter_map(|table| table.ok()) {
                    for seq in table.sequences().iter().filter_map(|seq| seq.ok()) {
                        for gid in seq.substitute_glyph_ids() {
                            self.capture_glyph(gid.get().to_u32());
                        }
                    }
                    // See above
                    if self.need_blue_substs && nesting_depth == 0 {
                        self.check_blue_coverage(table.coverage());
                    }
                }
            }
            SubstitutionSubtables::Ligature(tables) => {
                for table in tables.iter().filter_map(|table| table.ok()) {
                    for set in table.ligature_sets().iter().filter_map(|set| set.ok()) {
                        for lig in set.ligatures().iter().filter_map(|lig| lig.ok()) {
                            self.capture_glyph(lig.ligature_glyph().to_u32());
                        }
                    }
                }
            }
            SubstitutionSubtables::Alternate(tables) => {
                for table in tables.iter().filter_map(|table| table.ok()) {
                    for set in table.alternate_sets().iter().filter_map(|set| set.ok()) {
                        for gid in set.alternate_glyph_ids() {
                            self.capture_glyph(gid.get().to_u32());
                        }
                    }
                }
            }
            SubstitutionSubtables::Contextual(tables) => {
                for table in tables.iter().filter_map(|table| table.ok()) {
                    match table {
                        SequenceContext::Format1(table) => {
                            for set in table
                                .seq_rule_sets()
                                .iter()
                                .filter_map(|set| set.transpose().ok().flatten())
                            {
                                for rule in set.seq_rules().iter().filter_map(|rule| rule.ok()) {
                                    for rec in rule.seq_lookup_records() {
                                        self.process_lookup(
                                            rec.lookup_list_index(),
                                            nesting_depth + 1,
                                        );
                                    }
                                }
                            }
                        }
                        SequenceContext::Format2(table) => {
                            for set in table
                                .class_seq_rule_sets()
                                .iter()
                                .filter_map(|set| set.transpose().ok().flatten())
                            {
                                for rule in
                                    set.class_seq_rules().iter().filter_map(|rule| rule.ok())
                                {
                                    for rec in rule.seq_lookup_records() {
                                        self.process_lookup(
                                            rec.lookup_list_index(),
                                            nesting_depth + 1,
                                        );
                                    }
                                }
                            }
                        }
                        SequenceContext::Format3(table) => {
                            for rec in table.seq_lookup_records() {
                                self.process_lookup(rec.lookup_list_index(), nesting_depth + 1);
                            }
                        }
                    }
                }
            }
            SubstitutionSubtables::ChainContextual(tables) => {
                for table in tables.iter().filter_map(|table| table.ok()) {
                    match table {
                        ChainedSequenceContext::Format1(table) => {
                            for set in table
                                .chained_seq_rule_sets()
                                .iter()
                                .filter_map(|set| set.transpose().ok().flatten())
                            {
                                for rule in
                                    set.chained_seq_rules().iter().filter_map(|rule| rule.ok())
                                {
                                    for rec in rule.seq_lookup_records() {
                                        self.process_lookup(
                                            rec.lookup_list_index(),
                                            nesting_depth + 1,
                                        );
                                    }
                                }
                            }
                        }
                        ChainedSequenceContext::Format2(table) => {
                            for set in table
                                .chained_class_seq_rule_sets()
                                .iter()
                                .filter_map(|set| set.transpose().ok().flatten())
                            {
                                for rule in set
                                    .chained_class_seq_rules()
                                    .iter()
                                    .filter_map(|rule| rule.ok())
                                {
                                    for rec in rule.seq_lookup_records() {
                                        self.process_lookup(
                                            rec.lookup_list_index(),
                                            nesting_depth + 1,
                                        );
                                    }
                                }
                            }
                        }
                        ChainedSequenceContext::Format3(table) => {
                            for rec in table.seq_lookup_records() {
                                self.process_lookup(rec.lookup_list_index(), nesting_depth + 1);
                            }
                        }
                    }
                }
            }
            SubstitutionSubtables::Reverse(tables) => {
                for table in tables.iter().filter_map(|table| table.ok()) {
                    for gid in table.substitute_glyph_ids() {
                        self.capture_glyph(gid.get().to_u32());
                    }
                }
            }
        }
    }

    /// Finishes processing for this set of GSUB lookups and
    /// returns the range of touched glyphs.
    fn finish(self) -> Option<Range<usize>> {
        if self.min_gid > self.max_gid {
            // We didn't touch any glyphs
            return None;
        }
        let range = self.min_gid..self.max_gid + 1;
        if self.need_blue_substs {
            // We didn't find any substitutions for our blue strings so
            // we ignore the style. Clear the GSUB marker for any touched
            // glyphs
            for glyph in &mut self.glyph_styles[range] {
                glyph.clear_from_gsub();
            }
            None
        } else {
            Some(range)
        }
    }

    /// Checks the given coverage table for any characters in the blue
    /// strings associated with our current style.
    fn check_blue_coverage(&mut self, coverage: Result<CoverageTable<'a>, ReadError>) {
        let Ok(coverage) = coverage else {
            return;
        };
        for (blue_str, _) in self.style.script.blues {
            if blue_str
                .chars()
                .filter_map(|ch| self.charmap.map(ch))
                .filter_map(|gid| coverage.get(gid))
                .next()
                .is_some()
            {
                // Condition satisfied, so don't check any further subtables
                self.need_blue_substs = false;
                return;
            }
        }
    }

    fn capture_glyph(&mut self, gid: u32) {
        let gid = gid as usize;
        if let Some(style) = self.glyph_styles.get_mut(gid) {
            style.set_from_gsub_output();
            self.min_gid = gid.min(self.min_gid);
            self.max_gid = gid.max(self.max_gid);
        }
    }
}
