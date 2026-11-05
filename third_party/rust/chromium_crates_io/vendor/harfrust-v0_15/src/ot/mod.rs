use self::apply::MappingCache;
use self::layout::LayoutTableKind;
use super::buffer::GlyphPropsFlags;
use super::{set_digest::SetDigest, tag::TagExt};
use crate::budget::{Budget, Exhausted};
use crate::Tag;
use alloc::{sync::Arc, vec::Vec};
use lookup::{CachedLookup, ClassDef, Coverage, LookupCache};
use read_fonts::model::Tables;
use read_fonts::{
    tables::{
        gdef::Gdef,
        gpos::{AnchorTable, DeviceOrVariationIndex, Gpos},
        gsub::{FeatureList, FeatureVariations, Gsub, ScriptList},
        layout::{
            Feature as LayoutFeature, LangSys, LookupVariations, Script as LayoutScript,
            SelectedFeatureVariations,
        },
        varc::CoverageTable,
        variations::{DeltaSetIndex, ItemVariationStore},
    },
    types::{F2Dot14, GlyphId},
    FontData, ReadError, TableProvider,
};
use yoke::{Yoke, Yokeable};

pub(crate) mod apply;
pub mod gpos;
pub mod gsub;
pub(crate) mod layout;
pub mod lookup;

#[cfg(test)]
#[path = "lookup_variations_tests.rs"]
mod lookup_variations_tests;
pub(crate) mod map;
pub(crate) mod shaper;

#[allow(unused_imports)]
use super::{aat, buffer, cache, set_digest, tag};
#[allow(unused_imports)]
use super::{clamp_i64_to_i32, Direction, Feature, GlyphInfo, Language, Mask, Script};

#[allow(dead_code)]
pub(crate) mod lookup_flags {
    pub const RIGHT_TO_LEFT: u16 = 0x0001;
    pub const IGNORE_BASE_GLYPHS: u16 = 0x0002;
    pub const IGNORE_LIGATURES: u16 = 0x0004;
    pub const IGNORE_MARKS: u16 = 0x0008;
    pub const IGNORE_FLAGS: u16 = 0x000E;
    pub const USE_MARK_FILTERING_SET: u16 = 0x0010;
    pub const MARK_ATTACHMENT_TYPE_MASK: u16 = 0xFF00;
}

pub struct OtCache {
    pub gsub: LookupCache,
    pub gpos: LookupCache,
    pub gdef_glyph_props_cache: MappingCache,
    tables: Option<Yoke<CachedTables<'static>, Arc<Tables>>>,
    pub(crate) has_gdef: bool,
    pub(crate) gsub_len: u32,
    pub(crate) gdef_len: u32,
}

const MARK_GLYPH_SET_PAGE_BITS: u32 = 512;

#[derive(Clone, Debug)]
pub enum MarkGlyphSetBitmap {
    Page { bias: u32, bits: [u64; 8] },
    Digest(SetDigest),
}

impl MarkGlyphSetBitmap {
    fn page_from_coverage(
        coverage: &CoverageTable,
        min_gid: u32,
        budget: &Budget,
    ) -> Result<Option<[u64; 8]>, Exhausted> {
        let mut bits = [0; 8];
        let mut add_glyph = |glyph: u32| {
            let biased = glyph.wrapping_sub(min_gid);
            *bits.get_mut((biased / 64) as usize)? |= 1 << (biased & 63);
            Some(())
        };

        match coverage {
            CoverageTable::Format1(table) => {
                budget.work(table.glyph_array().len())?;
                for glyph in table.glyph_array() {
                    if add_glyph(u32::from(glyph.get())).is_none() {
                        return Ok(None);
                    }
                }
            }
            CoverageTable::Format2(table) => {
                budget.work(table.range_records().len())?;
                for range in table.range_records() {
                    let start = u32::from(range.start_glyph_id());
                    let end = u32::from(range.end_glyph_id());
                    budget.work(end.saturating_sub(start) as usize + 1)?;
                    for glyph in start..=end {
                        if add_glyph(glyph).is_none() {
                            return Ok(None);
                        }
                    }
                }
            }
            CoverageTable::Format3(table) => {
                budget.work(table.glyph_array().len())?;
                for glyph in table.glyph_array() {
                    if add_glyph(glyph.get().to_u32()).is_none() {
                        return Ok(None);
                    }
                }
            }
            CoverageTable::Format4(table) => {
                budget.work(table.range_records().len())?;
                for range in table.range_records() {
                    let start = range.start_glyph_id().to_u32();
                    let end = range.end_glyph_id().to_u32();
                    budget.work(end.saturating_sub(start) as usize + 1)?;
                    for glyph in start..=end {
                        if add_glyph(glyph).is_none() {
                            return Ok(None);
                        }
                    }
                }
            }
        }
        Ok(Some(bits))
    }

    fn from_coverage(coverage: &CoverageTable, budget: &Budget) -> Result<Self, Exhausted> {
        let bounds = match coverage {
            CoverageTable::Format1(table) => table
                .glyph_array()
                .first()
                .zip(table.glyph_array().last())
                .map(|(min, max)| (u32::from(min.get()), u32::from(max.get()))),
            CoverageTable::Format2(table) => table
                .range_records()
                .first()
                .zip(table.range_records().last())
                .map(|(min, max)| {
                    (
                        u32::from(min.start_glyph_id()),
                        u32::from(max.end_glyph_id()),
                    )
                }),
            CoverageTable::Format3(table) => table
                .glyph_array()
                .first()
                .zip(table.glyph_array().last())
                .map(|(min, max)| (min.get().to_u32(), max.get().to_u32())),
            CoverageTable::Format4(table) => table
                .range_records()
                .first()
                .zip(table.range_records().last())
                .map(|(min, max)| (min.start_glyph_id().to_u32(), max.end_glyph_id().to_u32())),
        };

        if let Some((min_gid, max_gid)) = bounds {
            if max_gid.wrapping_sub(min_gid) < MARK_GLYPH_SET_PAGE_BITS {
                if let Some(bits) = Self::page_from_coverage(coverage, min_gid, budget)? {
                    return Ok(Self::Page {
                        bias: min_gid,
                        bits,
                    });
                }
            }
        }

        budget.work(match coverage {
            CoverageTable::Format1(table) => table.glyph_array().len(),
            CoverageTable::Format2(table) => table.range_records().len(),
            CoverageTable::Format3(table) => table.glyph_array().len(),
            CoverageTable::Format4(table) => table.range_records().len(),
        })?;
        Ok(Self::Digest(SetDigest::from_coverage(coverage)))
    }

    #[inline(always)]
    fn covers(&self, glyph_id: u32) -> Option<bool> {
        match self {
            Self::Page { bias, bits } => {
                let biased = glyph_id.wrapping_sub(*bias);
                Some(
                    biased < MARK_GLYPH_SET_PAGE_BITS
                        && bits[(biased / 64) as usize] & (1 << (biased & 63)) != 0,
                )
            }
            Self::Digest(digest) => (!digest.may_have(glyph_id)).then_some(false),
        }
    }
}

impl OtCache {
    pub fn new(tables: Option<Arc<Tables>>) -> Self {
        let bytes = tables.as_deref().map_or(0, |tables| {
            [tables.gsub_data(), tables.gpos_data(), tables.gdef_data()]
                .into_iter()
                .flatten()
                .fold(0usize, |bytes, data| bytes.saturating_add(data.len()))
        });
        let budget = Arc::new(Budget::for_table_bytes(bytes));
        let cached = tables.clone().map(|tables| {
            Yoke::attach_to_cart(tables, |tables| CachedTables::new(tables, &budget))
        });
        let parsed: Option<&CachedTables> = cached.as_ref().map(Yoke::get);
        Self {
            // A missing table has no lookups.
            gsub: LookupCache::new(tables.clone(), true, budget.clone()),
            gpos: LookupCache::new(tables, false, budget.clone()),
            gdef_glyph_props_cache: MappingCache::new(),
            has_gdef: parsed.is_some_and(|cached| cached.has_gdef),
            gsub_len: parsed.map_or(0, |cached| cached.gsub_len),
            gdef_len: parsed.map_or(0, |cached| cached.gdef_len),
            tables: cached,
        }
    }
}

/// The layout tables, parsed once and kept with the font tables.
#[derive(Yokeable)]
pub(crate) struct CachedTables<'a> {
    gsub: Option<Gsub<'a>>,
    gpos: Option<Gpos<'a>>,
    gdef: CachedGdef<'a>,
    /// Whether GDEF is used: present, and not one of the known broken ones.
    has_gdef: bool,
    gsub_len: u32,
    gdef_len: u32,
}

impl<'a> CachedTables<'a> {
    const EMPTY: Self = Self {
        gsub: None,
        gpos: None,
        gdef: CachedGdef::EMPTY,
        has_gdef: false,
        gsub_len: 0,
        gdef_len: 0,
    };

    fn new(tables: &'a Tables, budget: &Budget) -> Self {
        let gsub = tables.gsub().ok();
        let gpos = tables.gpos().ok();
        let gdef = tables.gdef().ok();
        let len = |data: Option<FontData>| data.map_or(0, |data| data.len() as u32);
        let gsub_len = len(gsub.as_ref().map(|table| table.offset_data()));
        let gpos_len = len(gpos.as_ref().map(|table| table.offset_data()));
        let gdef_len = len(gdef.as_ref().map(|table| table.offset_data()));
        let gdef = gdef.filter(|_| !is_gdef_blocklisted(gdef_len, gsub_len, gpos_len));
        Self {
            gsub,
            gpos,
            has_gdef: gdef.is_some(),
            gdef: gdef.map_or(CachedGdef::EMPTY, |gdef| CachedGdef::new(&gdef, budget)),
            gsub_len,
            gdef_len,
        }
    }
}

/// The parts of GDEF read while shaping.
#[derive(Yokeable)]
pub(crate) struct CachedGdef<'a> {
    classes: ClassDef<'a>,
    mark_classes: ClassDef<'a>,
    mark_sets: Vec<MarkSet<'a>>,
    var_store: Option<ItemVariationStore<'a>>,
}

impl<'a> CachedGdef<'a> {
    const EMPTY: Self = Self {
        classes: ClassDef::None,
        mark_classes: ClassDef::None,
        mark_sets: Vec::new(),
        var_store: None,
    };

    fn new(gdef: &Gdef<'a>, budget: &Budget) -> Self {
        let class_def = |table: Option<Result<_, _>>| table.map_or(ClassDef::None, ClassDef::new);
        let build_mark_sets = || -> Result<Vec<MarkSet<'a>>, Exhausted> {
            let Some(sets) = gdef.mark_glyph_sets_def().and_then(Result::ok) else {
                return Ok(Vec::new());
            };
            let coverages = sets.coverages();
            budget.work(coverages.len())?;
            budget.alloc::<MarkSet>(coverages.len())?;
            let mut mark_sets = Vec::with_capacity(coverages.len());
            for coverage in coverages.iter() {
                mark_sets.push(MarkSet::new(coverage, budget)?);
            }
            Ok(mark_sets)
        };
        // An incomplete mark-set cache must not approve a filtering lookup.
        let mark_sets = build_mark_sets().unwrap_or_default();
        Self {
            classes: class_def(gdef.glyph_class_def()),
            mark_classes: class_def(gdef.mark_attach_class_def()),
            mark_sets,
            var_store: gdef.item_var_store().and_then(Result::ok),
        }
    }
}

/// A mark glyph set, with a bitmap or digest that settles most queries.
struct MarkSet<'a> {
    bitmap: MarkGlyphSetBitmap,
    coverage: Coverage<'a>,
}

impl<'a> MarkSet<'a> {
    fn new(
        coverage: Result<CoverageTable<'a>, ReadError>,
        budget: &Budget,
    ) -> Result<Self, Exhausted> {
        let bitmap = match &coverage {
            Ok(coverage) => MarkGlyphSetBitmap::from_coverage(coverage, budget)?,
            Err(_) => MarkGlyphSetBitmap::Digest(SetDigest::default()),
        };
        Ok(Self {
            bitmap,
            coverage: Coverage::new(coverage),
        })
    }

    #[inline(never)]
    fn covers(&self, glyph_id: u32) -> bool {
        self.coverage.covers(GlyphId::new(glyph_id))
    }
}

#[derive(Clone)]
pub struct GsubTable<'a> {
    pub lookups: &'a LookupCache,
}

impl layout::LayoutTable for GsubTable<'_> {
    const KIND: LayoutTableKind = LayoutTableKind::Gsub;
    const IN_PLACE: bool = false;

    fn get_lookup(&self, index: u16) -> Option<&CachedLookup<'_>> {
        self.lookups.get(index)
    }
}

#[derive(Clone)]
pub struct GposTable<'a> {
    pub lookups: &'a LookupCache,
}

impl layout::LayoutTable for GposTable<'_> {
    const KIND: LayoutTableKind = LayoutTableKind::Gpos;
    const IN_PLACE: bool = true;

    fn get_lookup(&self, index: u16) -> Option<&CachedLookup<'_>> {
        self.lookups.get(index)
    }
}

/// The OpenType layout data of a font, borrowed from its layout cache.
#[derive(Clone)]
pub struct OtData<'a> {
    cache: &'a OtCache,
    tables: &'a CachedTables<'a>,
    /// The variation store, if the font is variable and has an instance.
    var_store: Option<&'a ItemVariationStore<'a>>,
    pub coords: &'a [F2Dot14],
    pub feature_variations: [Option<u32>; 2],
    /// Whether a layout table has lookup variations, which plans then
    /// resolve per feature at `coords`.
    pub has_lookup_variations: bool,
}

static EMPTY_OT_CACHE: OtCache = OtCache {
    gsub: LookupCache::EMPTY,
    gpos: LookupCache::EMPTY,
    gdef_glyph_props_cache: MappingCache::empty(),
    tables: None,
    has_gdef: false,
    gsub_len: 0,
    gdef_len: 0,
};
static EMPTY_TABLES: CachedTables<'static> = CachedTables::EMPTY;

pub(crate) static EMPTY_OT_DATA: OtData<'static> = OtData {
    cache: &EMPTY_OT_CACHE,
    tables: &EMPTY_TABLES,
    var_store: None,
    coords: &[],
    feature_variations: [None; 2],
    has_lookup_variations: false,
};

impl<'a> OtData<'a> {
    pub fn new(
        cache: &'a OtCache,
        coords: &'a [F2Dot14],
        variations: SelectedFeatureVariations,
    ) -> Self {
        let tables = cache.tables.as_ref().map_or(&EMPTY_TABLES, Yoke::get);
        Self {
            cache,
            tables,
            var_store: tables
                .gdef
                .var_store
                .as_ref()
                .filter(|_| !coords.is_empty()),
            coords,
            feature_variations: [variations.gsub, variations.gpos],
            has_lookup_variations: variations.has_lookup_variations,
        }
    }

    pub fn gsub(&self) -> Option<GsubTable<'a>> {
        self.tables.gsub.is_some().then_some(GsubTable {
            lookups: &self.cache.gsub,
        })
    }

    pub fn gpos(&self) -> Option<GposTable<'a>> {
        self.tables.gpos.is_some().then_some(GposTable {
            lookups: &self.cache.gpos,
        })
    }

    /// The variation store, if the font is variable and has an instance.
    pub fn var_store(&self) -> Option<&'a ItemVariationStore<'a>> {
        self.var_store
    }

    pub(crate) fn layout_table(&self, table_index: LayoutTableKind) -> Option<LayoutTable<'a>> {
        match table_index {
            LayoutTableKind::Gsub => self.tables.gsub.clone().map(LayoutTable::Gsub),
            LayoutTableKind::Gpos => self.tables.gpos.clone().map(LayoutTable::Gpos),
        }
    }

    pub(crate) fn layout_tables(
        &self,
    ) -> impl Iterator<Item = (LayoutTableKind, LayoutTable<'a>)> + '_ {
        LayoutTableKind::iter()
            .filter_map(move |idx| self.layout_table(idx).map(|table| (idx, table)))
    }

    pub fn has_glyph_classes(&self) -> bool {
        !self.tables.gdef.classes.is_none()
    }

    pub fn glyph_class(&self, glyph_id: u32) -> u16 {
        self.tables.gdef.classes.get(GlyphId::new(glyph_id)) as u16
    }

    pub fn glyph_mark_attachment_class(&self, glyph_id: u32) -> u16 {
        // Glyph props keep only the low bits, as in HarfBuzz.
        self.tables.gdef.mark_classes.get(GlyphId::new(glyph_id)) as u16
    }

    pub(crate) fn glyph_props(&self, glyph: GlyphId) -> u16 {
        let glyph = glyph.to_u32();

        if let Some(props) = self.cache.gdef_glyph_props_cache.get(glyph) {
            return props as u16;
        }

        let props = match self.glyph_class(glyph) {
            1 => GlyphPropsFlags::BASE_GLYPH.bits(),
            2 => GlyphPropsFlags::LIGATURE.bits(),
            3 => {
                let class = self.glyph_mark_attachment_class(glyph);
                (class << 8) | GlyphPropsFlags::MARK.bits()
            }
            _ => 0,
        };

        self.cache.gdef_glyph_props_cache.set(glyph, props as u32);

        props
    }

    #[inline(always)]
    pub fn is_mark_glyph(&self, glyph_id: u32, set_index: u16) -> bool {
        self.tables
            .gdef
            .mark_sets
            .get(set_index as usize)
            .is_some_and(|set| {
                set.bitmap
                    .covers(glyph_id)
                    .unwrap_or_else(|| set.covers(glyph_id))
            })
    }

    pub fn lookup(
        &self,
        table_index: LayoutTableKind,
        lookup_index: u16,
    ) -> Option<&'a CachedLookup<'a>> {
        // A missing table has no lookups.
        if table_index == LayoutTableKind::Gsub {
            self.cache.gsub.get(lookup_index)
        } else {
            self.cache.gpos.get(lookup_index)
        }
    }

    pub(super) fn resolve_anchor(&self, anchor: &AnchorTable) -> (f32, f32) {
        let mut x = anchor.x_coordinate() as f32;
        let mut y = anchor.y_coordinate() as f32;
        if let Some(vs) = self.var_store() {
            // Keep the delta fractional; the caller rounds once when scaling,
            // matching HarfBuzz's Anchor::get_anchor.
            let delta = |val: Option<Result<DeviceOrVariationIndex<'_>, ReadError>>| match val {
                Some(Ok(DeviceOrVariationIndex::VariationIndex(varix))) => {
                    vs.compute_delta(
                        DeltaSetIndex {
                            outer: varix.delta_set_outer_index(),
                            inner: varix.delta_set_inner_index(),
                        },
                        self.coords,
                    )
                    .unwrap_or_default()
                    .to_f64() as f32
                }
                _ => 0.0,
            };
            x += delta(anchor.x_device());
            y += delta(anchor.y_device());
        }
        (x, y)
    }
}

pub enum LayoutTable<'a> {
    Gsub(Gsub<'a>),
    Gpos(Gpos<'a>),
}

impl<'a> LayoutTable<'a> {
    fn script_list(&self) -> Option<ScriptList<'a>> {
        match self {
            Self::Gsub(gsub) => gsub.script_list().ok(),
            Self::Gpos(gpos) => gpos.script_list().ok(),
        }
    }

    fn feature_list(&self) -> Option<FeatureList<'a>> {
        match self {
            Self::Gsub(gsub) => gsub.feature_list().ok(),
            Self::Gpos(gpos) => gpos.feature_list().ok(),
        }
    }

    /// The table's lookup variations, if it has any.
    pub(crate) fn lookup_variations(&self) -> Option<LookupVariations<'a>> {
        self.feature_variations()?.lookup_variations()
    }

    fn feature_variations(&self) -> Option<FeatureVariations<'a>> {
        match self {
            Self::Gsub(gsub) => gsub.feature_variations(),
            Self::Gpos(gpos) => gpos.feature_variations(),
        }
        .transpose()
        .ok()
        .flatten()
    }

    fn script(&self, index: u16) -> Option<LayoutScript<'a>> {
        self.script_list()?
            .get(index)
            .ok()
            .map(|script| script.element)
    }

    fn langsys_index(&self, script_index: u16, tag: Tag) -> Option<u16> {
        let script = self.script(script_index)?;
        script.lang_sys_index_for_tag(tag)
    }

    fn langsys(&self, script_index: u16, langsys_index: Option<u16>) -> Option<LangSys<'a>> {
        let script = self.script(script_index)?;
        if let Some(index) = langsys_index {
            let record = script.lang_sys_records().get(index as usize)?;
            record.lang_sys(script.offset_data()).ok()
        } else {
            script.default_lang_sys().transpose().ok().flatten()
        }
    }

    pub(crate) fn feature(&self, index: u16) -> Option<LayoutFeature<'a>> {
        self.feature_list()?
            .get(index)
            .ok()
            .map(|feature| feature.element)
    }

    fn feature_tag(&self, index: u16) -> Option<Tag> {
        self.feature_list()?
            .get(index)
            .ok()
            .map(|feature| feature.tag)
    }

    pub(crate) fn feature_substitution(
        &self,
        variation_index: u32,
        feature_index: u16,
    ) -> Option<LayoutFeature<'a>> {
        let feature_variations = self.feature_variations()?;
        let record = feature_variations
            .feature_variation_records()
            .get(variation_index as usize)?;
        let subst_table = record
            .feature_table_substitution(feature_variations.offset_data())?
            .ok()?;
        let subst_records = subst_table.substitutions();
        match subst_records.binary_search_by_key(&feature_index, |subst| subst.feature_index()) {
            Ok(ix) => Some(
                subst_records
                    .get(ix)?
                    .alternate_feature(subst_table.offset_data())
                    .ok()?,
            ),
            _ => None,
        }
    }

    pub(crate) fn feature_index(&self, tag: Tag) -> Option<u16> {
        let list = self.feature_list()?;
        for (index, feature) in list.feature_records().iter().enumerate() {
            if feature.feature_tag() == tag {
                return Some(index as u16);
            }
        }
        None
    }

    pub(crate) fn lookup_count(&self) -> u16 {
        match self {
            Self::Gsub(gsub) => gsub
                .lookup_list()
                .map(|list| list.lookup_count())
                .unwrap_or_default(),
            Self::Gpos(gpos) => gpos
                .lookup_list()
                .map(|list| list.lookup_count())
                .unwrap_or_default(),
        }
    }

    // hb_ot_layout_table_select_script
    /// Returns true + index and tag of the first found script tag in the given GSUB or GPOS table
    /// or false + index and tag if falling back to a default script.
    pub(crate) fn select_script(&self, script_tags: &[Tag]) -> Option<(bool, u16, Tag)> {
        let selected = self.script_list()?.select(script_tags)?;
        Some((!selected.is_fallback, selected.index, selected.tag))
    }

    // hb_ot_layout_script_select_language
    /// Returns the index of the first found language tag in the given GSUB or GPOS table,
    /// underneath the specified script index.
    pub(crate) fn select_script_language(
        &self,
        script_index: u16,
        lang_tags: &[Tag],
    ) -> Option<u16> {
        for &tag in lang_tags {
            if let Some(index) = self.langsys_index(script_index, tag) {
                return Some(index);
            }
        }

        // try finding 'dflt'
        if let Some(index) = self.langsys_index(script_index, Tag::default_language()) {
            return Some(index);
        }

        None
    }

    // hb_ot_layout_language_get_required_feature
    /// Returns the index and tag of a required feature in the given GSUB or GPOS table,
    /// underneath the specified script and language.
    pub(crate) fn get_required_language_feature(
        &self,
        script_index: u16,
        lang_index: Option<u16>,
    ) -> Option<(u16, Tag)> {
        let sys = self.langsys(script_index, lang_index)?;
        let idx = sys.required_feature_index();
        if idx == 0xFFFF {
            return None;
        }
        let tag = self.feature_tag(idx)?;
        Some((idx, tag))
    }

    // hb_ot_layout_language_find_feature
    /// Returns the index of a given feature tag in the given GSUB or GPOS table,
    /// underneath the specified script and language.
    pub(crate) fn find_language_feature(
        &self,
        script_index: u16,
        lang_index: Option<u16>,
        feature_tag: Tag,
    ) -> Option<u16> {
        self.langsys(script_index, lang_index)?
            .feature_index_for_tag(&self.feature_list()?, feature_tag)
    }
}

use crate::algs::HB_CODEPOINT_ENCODE3 as encode3;

/// Blocklist specific broken GDEF tables identified by the combination of
/// GDEF, GSUB, and GPOS table lengths. Nuke the GDEF tables to avoid
/// unwanted width-zeroing.
///
/// In certain versions of Times New Roman Italic and Bold Italic,
/// ASCII double quotation mark U+0022 has wrong glyph class 3 (mark)
/// in GDEF. Many versions of Tahoma have bad GDEF tables that
/// incorrectly classify some spacing marks such as certain IPA
/// symbols as glyph class 3. So do older versions of Microsoft
/// Himalaya, and the version of Cantarell shipped by Ubuntu 16.04.
///
/// See https://lists.freedesktop.org/archives/harfbuzz/2016-February/005489.html
///     https://bugzilla.mozilla.org/show_bug.cgi?id=1279925
///     https://bugzilla.mozilla.org/show_bug.cgi?id=1279693
///     https://bugzilla.mozilla.org/show_bug.cgi?id=1279875
///
/// Upstream: hb-ot-layout.cc OT::GDEF::is_blocklisted
fn is_gdef_blocklisted(gdef_len: u32, gsub_len: u32, gpos_len: u32) -> bool {
    const BLOCKLIST: &[u64] = &[
        // Windows 7? timesi.ttf
        encode3(442, 2874, 42038),
        // Windows 7? timesbi.ttf
        encode3(430, 2874, 40662),
        // Windows 7 timesi.ttf
        encode3(442, 2874, 39116),
        // Windows 7 timesbi.ttf
        encode3(430, 2874, 39374),
        // OS X 10.11.3 Times New Roman Italic.ttf
        encode3(490, 3046, 41638),
        // OS X 10.11.3 Times New Roman Bold Italic.ttf
        encode3(478, 3046, 41902),
        // tahoma.ttf from Windows 8
        encode3(898, 12554, 46470),
        // tahomabd.ttf from Windows 8
        encode3(910, 12566, 47732),
        // tahoma.ttf from Windows 8.1
        encode3(928, 23298, 59332),
        // tahomabd.ttf from Windows 8.1
        encode3(940, 23310, 60732),
        // tahoma.ttf v6.04 from Windows 8.1 x64
        encode3(964, 23836, 60072),
        // tahomabd.ttf v6.04 from Windows 8.1 x64
        encode3(976, 23832, 61456),
        // tahoma.ttf from Windows 10
        encode3(994, 24474, 60336),
        // tahomabd.ttf from Windows 10
        encode3(1006, 24470, 61740),
        // tahoma.ttf v6.91 from Windows 10 x64
        encode3(1006, 24576, 61346),
        // tahomabd.ttf v6.91 from Windows 10 x64
        encode3(1018, 24572, 62828),
        // tahoma.ttf from Windows 10 AU
        encode3(1006, 24576, 61352),
        // tahomabd.ttf from Windows 10 AU
        encode3(1018, 24572, 62834),
        // Tahoma.ttf from Mac OS X 10.9
        encode3(832, 7324, 47162),
        // Tahoma Bold.ttf from Mac OS X 10.9
        encode3(844, 7302, 45474),
        // himalaya.ttf from Windows 7
        encode3(180, 13054, 7254),
        // himalaya.ttf from Windows 8
        encode3(192, 12638, 7254),
        // himalaya.ttf from Windows 8.1
        encode3(192, 12690, 7254),
        // cantarell-fonts-0.0.21 Cantarell-Regular.otf / Cantarell-Oblique.otf
        encode3(188, 248, 3852),
        // cantarell-fonts-0.0.21 Cantarell-Bold.otf / Cantarell-Bold-Oblique.otf
        encode3(188, 264, 3426),
        // padauk-2.80/Padauk.ttf RHEL 7.2
        encode3(1058, 47032, 11818),
        // padauk-2.80/Padauk-Bold.ttf RHEL 7.2
        encode3(1046, 47030, 12600),
        // padauk-2.80/Padauk.ttf Ubuntu 16.04
        encode3(1058, 71796, 16770),
        // padauk-2.80/Padauk-Bold.ttf Ubuntu 16.04
        encode3(1046, 71790, 17862),
        // padauk-2.80/Padauk-book.ttf
        encode3(1046, 71788, 17112),
        // padauk-2.80/Padauk-bookbold.ttf
        encode3(1058, 71794, 17514),
        // padauk-3.0/Padauk-book.ttf
        encode3(1330, 109_904, 57938),
        // padauk-3.0/Padauk-bookbold.ttf
        encode3(1330, 109_904, 58972),
        // Padauk.ttf "Version 2.5", https://crbug.com/681813
        encode3(1004, 59092, 14836),
        // Courier New.ttf from macOS 15
        encode3(588, 5078, 14418),
        // Courier New Bold.ttf from macOS 15
        encode3(588, 5078, 14238),
        // cour.ttf from Windows 10
        encode3(894, 17162, 33960),
        // courbd.ttf from Windows 10
        encode3(894, 17154, 34472),
        // cour.ttf from Windows 8.1
        encode3(816, 7868, 17052),
        // courbd.ttf from Windows 8.1
        encode3(816, 7868, 17138),
    ];
    let key = encode3(gdef_len, gsub_len, gpos_len);
    BLOCKLIST.contains(&key)
}

#[cfg(test)]
mod tests {
    use super::*;
    use read_fonts::FontRead;

    #[test]
    fn exhausted_mark_sets_are_discarded_together() {
        // GDEF 1.2, with two mark-set offsets aliasing one coverage.
        let words: [u16; 16] = [1, 2, 0, 0, 0, 0, 14, 1, 2, 0, 12, 0, 12, 1, 1, 42];
        let data: Vec<_> = words.into_iter().flat_map(u16::to_be_bytes).collect();
        let gdef = Gdef::read(FontData::new(&data)).unwrap();
        let cache = CachedGdef::new(&gdef, &Budget::new(4, 1024));
        assert_eq!(cache.mark_sets.len(), 2);
        assert!(cache.mark_sets.iter().all(|set| set.covers(42)));

        // Exhaustion while building the second set must discard the first.
        let cache = CachedGdef::new(&gdef, &Budget::new(3, 1024));
        assert!(cache.mark_sets.is_empty());
        let cache = CachedGdef::new(&gdef, &Budget::new(4, 0));
        assert!(cache.mark_sets.is_empty());
    }
}
