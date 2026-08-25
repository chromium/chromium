use super::buffer::GlyphPropsFlags;
use super::ot_layout::TableIndex;
use super::{common::TagExt, set_digest::hb_set_digest_t};
use crate::hb::hb_tag_t;
use crate::hb::ot_layout_gsubgpos::{BinaryCache, MappingCache};
use crate::hb::tables::TableRanges;
use alloc::vec::Vec;
use lookup::{LookupCache, LookupInfo};
use read_fonts::tables::layout::{ClassRangeRecord, RangeRecord};
use read_fonts::types::GlyphId16;
use read_fonts::{
    tables::{
        gdef::Gdef,
        gpos::{AnchorTable, DeviceOrVariationIndex, Gpos},
        gsub::{ClassDef, FeatureList, FeatureVariations, Gsub, ScriptList},
        layout::{Feature, LangSys, Script},
        varc::{Condition, CoverageTable},
        variations::{DeltaSetIndex, ItemVariationStore},
    },
    types::{BigEndian, F2Dot14, GlyphId},
    FontData, FontRead, FontRef, ReadError, TableProvider,
};

pub mod contextual;
pub mod gpos;
pub mod gsub;
pub mod lookup;

pub struct OtCache {
    pub gsub: LookupCache,
    pub gpos: LookupCache,
    pub gdef_glyph_props_cache: MappingCache,
    gdef: GdefCache,
    has_gsub: bool,
    has_gpos: bool,
    pub(crate) has_gdef: bool,
    pub(crate) gsub_len: u32,
    pub(crate) gdef_len: u32,
}

const MARK_GLYPH_SET_PAGE_BITS: u32 = 512;

#[derive(Clone, Debug)]
pub enum MarkGlyphSetBitmap {
    Page { bias: u32, bits: [u64; 8] },
    Digest(hb_set_digest_t),
}

impl MarkGlyphSetBitmap {
    fn page_from_coverage(coverage: &CoverageTable, min_gid: u32) -> Option<[u64; 8]> {
        let mut bits = [0; 8];
        let mut add_glyph = |glyph: u32| {
            let biased = glyph.wrapping_sub(min_gid);
            *bits.get_mut((biased / 64) as usize)? |= 1 << (biased & 63);
            Some(())
        };

        match coverage {
            CoverageTable::Format1(table) => {
                for glyph in table.glyph_array() {
                    add_glyph(u32::from(glyph.get()))?;
                }
            }
            CoverageTable::Format2(table) => {
                for range in table.range_records() {
                    for glyph in u32::from(range.start_glyph_id())..=u32::from(range.end_glyph_id())
                    {
                        add_glyph(glyph)?;
                    }
                }
            }
        }
        Some(bits)
    }

    fn from_coverage(coverage: &CoverageTable) -> Self {
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
        };

        if let Some((min_gid, max_gid)) = bounds {
            if max_gid.wrapping_sub(min_gid) < MARK_GLYPH_SET_PAGE_BITS {
                if let Some(bits) = Self::page_from_coverage(coverage, min_gid) {
                    return Self::Page {
                        bias: min_gid,
                        bits,
                    };
                }
            }
        }

        Self::Digest(hb_set_digest_t::from_coverage(coverage))
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
    pub fn new<'a>(font: &impl TableProvider<'a>) -> Self {
        let gsub_table = font.gsub().ok();
        let gpos_table = font.gpos().ok();
        let gdef_table = font.gdef().ok();
        let gsub_len = gsub_table
            .as_ref()
            .map_or(0, |table| table.offset_data().len() as u32);
        let gpos_len = gpos_table
            .as_ref()
            .map_or(0, |table| table.offset_data().len() as u32);
        let gdef_len = gdef_table
            .as_ref()
            .map_or(0, |table| table.offset_data().len() as u32);
        let has_gdef = gdef_table.is_some() && !is_gdef_blocklisted(gdef_len, gsub_len, gpos_len);
        let gsub = gsub_table
            .as_ref()
            .map(LookupCache::new)
            .unwrap_or_default();
        let gpos = gpos_table
            .as_ref()
            .map(LookupCache::new)
            .unwrap_or_default();
        let gdef = gdef_table
            .as_ref()
            .filter(|_| has_gdef)
            .map(GdefCache::new)
            .unwrap_or_default();
        Self {
            gsub,
            gpos,
            gdef_glyph_props_cache: MappingCache::new(),
            gdef,
            has_gsub: gsub_table.is_some(),
            has_gpos: gpos_table.is_some(),
            has_gdef,
            gsub_len,
            gdef_len,
        }
    }
}

#[derive(Default)]
struct GdefCache {
    classes: Option<ClassDefInfo>,
    mark_classes: Option<ClassDefInfo>,
    mark_set_offsets: Vec<u32>,
    mark_set_bitmaps: Vec<MarkGlyphSetBitmap>,
    item_var_store_offset: u32,
}

impl GdefCache {
    fn new(gdef: &Gdef) -> Self {
        let data = gdef.offset_data();
        let classes = ClassDefInfo::new(
            &data,
            gdef.glyph_class_def_offset().offset().to_u32() as u16,
        );
        let mark_classes = ClassDefInfo::new(
            &data,
            gdef.mark_attach_class_def_offset().offset().to_u32() as u16,
        );
        let mut mark_set_offsets = Vec::new();
        let mut mark_set_bitmaps = Vec::new();
        if let Some((base, Ok(mark_sets))) = gdef
            .mark_glyph_sets_def_offset()
            .map(|offset| offset.offset().to_u32())
            .zip(gdef.mark_glyph_sets_def())
        {
            mark_set_offsets.extend(
                mark_sets
                    .coverage_offsets()
                    .iter()
                    .map(|offset| base.saturating_add(offset.get().to_u32())),
            );
            mark_set_bitmaps.extend(mark_sets.coverages().iter().map(|set| {
                set.ok().map_or_else(
                    || MarkGlyphSetBitmap::Digest(hb_set_digest_t::default()),
                    |coverage| MarkGlyphSetBitmap::from_coverage(&coverage),
                )
            }));
        }
        let item_var_store_offset = gdef
            .item_var_store_offset()
            .map(|offset| offset.offset().to_u32())
            .unwrap_or_default();
        Self {
            classes,
            mark_classes,
            mark_set_offsets,
            mark_set_bitmaps,
            item_var_store_offset,
        }
    }

    fn item_var_store<'a>(&self, gdef: &Gdef<'a>) -> Option<ItemVariationStore<'a>> {
        if self.item_var_store_offset == 0 {
            return None;
        }
        let data = gdef
            .offset_data()
            .split_off(self.item_var_store_offset as usize)?;
        ItemVariationStore::read(data).ok()
    }
}

#[derive(Clone)]
pub struct GsubTable<'a> {
    pub table: Gsub<'a>,
    pub lookups: &'a LookupCache,
}

impl crate::hb::ot_layout::LayoutTable for GsubTable<'_> {
    const INDEX: TableIndex = TableIndex::GSUB;
    const IN_PLACE: bool = false;

    fn get_lookup(&self, index: u16) -> Option<&LookupInfo> {
        self.lookups.get(&self.table, index)
    }
}

#[derive(Clone)]
pub struct GposTable<'a> {
    pub table: Gpos<'a>,
    pub lookups: &'a LookupCache,
}

impl crate::hb::ot_layout::LayoutTable for GposTable<'_> {
    const INDEX: TableIndex = TableIndex::GPOS;
    const IN_PLACE: bool = true;

    fn get_lookup(&self, index: u16) -> Option<&LookupInfo> {
        self.lookups.get(&self.table, index)
    }
}

#[derive(Clone, Default)]
pub struct GdefTable<'a> {
    pub(crate) table: Option<Gdef<'a>>,
}

impl<'a> GdefTable<'a> {
    fn new(gdef: Gdef<'a>) -> Self {
        Self { table: Some(gdef) }
    }
}

#[derive(Clone)]
pub struct OtTables<'a> {
    pub gsub: Option<GsubTable<'a>>,
    pub gpos: Option<GposTable<'a>>,
    pub gdef: GdefTable<'a>,
    pub gdef_glyph_props_cache: &'a MappingCache,
    pub gdef_mark_set_bitmaps: &'a [MarkGlyphSetBitmap],
    gdef_cache: &'a GdefCache,
    pub coords: &'a [F2Dot14],
    pub var_store: Option<ItemVariationStore<'a>>,
    pub feature_variations: [Option<u32>; 2],
}

impl<'a> OtTables<'a> {
    pub fn new(
        font: &FontRef<'a>,
        cache: &'a OtCache,
        table_offsets: &TableRanges,
        coords: &'a [F2Dot14],
        feature_variations: [Option<u32>; 2],
    ) -> Self {
        let gsub = table_offsets
            .gsub
            .resolve_table(font)
            .map(|table| GsubTable {
                table,
                lookups: &cache.gsub,
            });
        let gpos = table_offsets
            .gpos
            .resolve_table(font)
            .map(|table| GposTable {
                table,
                lookups: &cache.gpos,
            });
        let gdef = if cache.has_gdef {
            table_offsets
                .gdef
                .resolve_table(font)
                .map(GdefTable::new)
                .unwrap_or_default()
        } else {
            GdefTable::default()
        };
        let var_store = if !coords.is_empty() {
            gdef.table
                .as_ref()
                .and_then(|gdef| cache.gdef.item_var_store(gdef))
        } else {
            None
        };
        Self {
            gsub,
            gpos,
            gdef,
            gdef_glyph_props_cache: &cache.gdef_glyph_props_cache,
            gdef_mark_set_bitmaps: &cache.gdef.mark_set_bitmaps,
            gdef_cache: &cache.gdef,
            var_store,
            coords,
            feature_variations,
        }
    }

    pub fn from_tables(
        font: &impl TableProvider<'a>,
        cache: &'a OtCache,
        coords: &'a [F2Dot14],
        feature_variations: [Option<u32>; 2],
    ) -> Self {
        let gsub = cache
            .has_gsub
            .then(|| font.gsub().ok())
            .flatten()
            .map(|table| GsubTable {
                table,
                lookups: &cache.gsub,
            });
        let gpos = cache
            .has_gpos
            .then(|| font.gpos().ok())
            .flatten()
            .map(|table| GposTable {
                table,
                lookups: &cache.gpos,
            });
        let gdef = cache
            .has_gdef
            .then(|| font.gdef().ok())
            .flatten()
            .map(GdefTable::new)
            .unwrap_or_default();
        let var_store = if !coords.is_empty() {
            gdef.table
                .as_ref()
                .and_then(|gdef| cache.gdef.item_var_store(gdef))
        } else {
            None
        };
        Self {
            gsub,
            gpos,
            gdef,
            gdef_glyph_props_cache: &cache.gdef_glyph_props_cache,
            gdef_mark_set_bitmaps: &cache.gdef.mark_set_bitmaps,
            gdef_cache: &cache.gdef,
            var_store,
            coords,
            feature_variations,
        }
    }

    pub fn has_glyph_classes(&self) -> bool {
        self.gdef_cache.classes.is_some()
    }

    pub fn glyph_class(&self, glyph_id: u32) -> u16 {
        self.gdef_cache
            .classes
            .as_ref()
            .zip(self.gdef.table.as_ref())
            .map_or(0, |(class_def, gdef)| {
                class_def.class(&gdef.offset_data(), GlyphId::new(glyph_id))
            })
    }

    pub fn glyph_mark_attachment_class(&self, glyph_id: u32) -> u16 {
        self.gdef_cache
            .mark_classes
            .as_ref()
            .zip(self.gdef.table.as_ref())
            .map_or(0, |(class_def, gdef)| {
                class_def.class(&gdef.offset_data(), GlyphId::new(glyph_id))
            })
    }

    pub(crate) fn glyph_props(&self, glyph: GlyphId) -> u16 {
        let glyph = glyph.to_u32();

        if let Some(props) = self.gdef_glyph_props_cache.get(glyph) {
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

        self.gdef_glyph_props_cache.set(glyph, props as u32);

        props
    }

    #[inline(never)]
    pub fn is_mark_glyph_gdef(&self, glyph_id: u32, set_index: u16) -> bool {
        self.gdef
            .table
            .as_ref()
            .and_then(|gdef| {
                let offset = *self.gdef_cache.mark_set_offsets.get(set_index as usize)? as usize;
                CoverageTable::read(gdef.offset_data().split_off(offset)?).ok()
            })
            .is_some_and(|coverage| coverage.get(glyph_id).is_some())
    }

    #[inline(always)]
    pub fn is_mark_glyph(&self, glyph_id: u32, set_index: u16) -> bool {
        self.gdef_mark_set_bitmaps
            .get(set_index as usize)
            .and_then(|bitmap| bitmap.covers(glyph_id))
            .unwrap_or_else(|| self.is_mark_glyph_gdef(glyph_id, set_index))
    }

    pub fn table_data(&self, table_index: TableIndex) -> Option<&'a [u8]> {
        if table_index == TableIndex::GSUB {
            self.gsub.as_ref().map(|t| t.table.offset_data().as_bytes())
        } else {
            self.gpos.as_ref().map(|t| t.table.offset_data().as_bytes())
        }
    }

    pub fn table_data_and_lookup(
        &self,
        table_index: TableIndex,
        lookup_index: u16,
    ) -> Option<(&'a [u8], &'a LookupInfo)> {
        if table_index == TableIndex::GSUB {
            let table = self.gsub.as_ref()?;
            Some((
                table.table.offset_data().as_bytes(),
                table.lookups.get(&table.table, lookup_index)?,
            ))
        } else {
            let table = self.gpos.as_ref()?;
            Some((
                table.table.offset_data().as_bytes(),
                table.lookups.get(&table.table, lookup_index)?,
            ))
        }
    }

    pub(super) fn resolve_anchor(&self, anchor: &AnchorTable) -> (f32, f32) {
        let mut x = anchor.x_coordinate() as f32;
        let mut y = anchor.y_coordinate() as f32;
        if let Some(vs) = self.var_store.as_ref() {
            // Keep the delta fractional; the caller rounds once when scaling,
            // matching HarfBuzz's Anchor::get_anchor.
            let delta = |val: Option<Result<DeviceOrVariationIndex<'_>, ReadError>>| match val {
                Some(Ok(DeviceOrVariationIndex::VariationIndex(varix))) => {
                    vs.compute_float_delta(
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

    fn feature_variations(&self) -> Option<FeatureVariations<'a>> {
        match self {
            Self::Gsub(gsub) => gsub.feature_variations(),
            Self::Gpos(gpos) => gpos.feature_variations(),
        }
        .transpose()
        .ok()
        .flatten()
    }

    fn script(&self, index: u16) -> Option<Script<'a>> {
        self.script_list()?
            .get(index)
            .ok()
            .map(|script| script.element)
    }

    fn langsys_index(&self, script_index: u16, tag: hb_tag_t) -> Option<u16> {
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

    pub(crate) fn feature(&self, index: u16) -> Option<Feature<'a>> {
        self.feature_list()?
            .get(index)
            .ok()
            .map(|feature| feature.element)
    }

    fn feature_tag(&self, index: u16) -> Option<hb_tag_t> {
        self.feature_list()?
            .get(index)
            .ok()
            .map(|feature| feature.tag)
    }

    pub(crate) fn feature_variation_index(&self, coords: &[F2Dot14]) -> Option<u32> {
        let feature_variations = self.feature_variations()?;
        for (index, rec) in feature_variations
            .feature_variation_records()
            .iter()
            .enumerate()
        {
            // If the ConditionSet offset is 0, this is treated as the
            // universal condition: all contexts are matched.
            if rec.condition_set_offset().is_null() {
                return Some(index as u32);
            }
            let Some(Ok(condition_set)) = rec.condition_set(feature_variations.offset_data())
            else {
                continue;
            };
            // Otherwise, all conditions must be satisfied.
            if condition_set
                .conditions()
                .iter()
                // .. except we ignore errors
                .filter_map(Result::ok)
                .all(|cond| match cond {
                    Condition::Format1AxisRange(format1) => {
                        let coord = coords
                            .get(format1.axis_index() as usize)
                            .copied()
                            .unwrap_or_default();
                        coord >= format1.filter_range_min_value()
                            && coord <= format1.filter_range_max_value()
                    }
                    _ => false,
                })
            {
                return Some(index as u32);
            }
        }
        None
    }

    pub(crate) fn feature_substitution(
        &self,
        variation_index: u32,
        feature_index: u16,
    ) -> Option<Feature<'a>> {
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

    pub(crate) fn feature_index(&self, tag: hb_tag_t) -> Option<u16> {
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
    pub(crate) fn select_script(&self, script_tags: &[hb_tag_t]) -> Option<(bool, u16, hb_tag_t)> {
        let selected = self.script_list()?.select(script_tags)?;
        Some((!selected.is_fallback, selected.index, selected.tag))
    }

    // hb_ot_layout_script_select_language
    /// Returns the index of the first found language tag in the given GSUB or GPOS table,
    /// underneath the specified script index.
    pub(crate) fn select_script_language(
        &self,
        script_index: u16,
        lang_tags: &[hb_tag_t],
    ) -> Option<u16> {
        for &tag in lang_tags {
            if let Some(index) = self.langsys_index(script_index, tag) {
                return Some(index);
            }
        }

        // try finding 'dflt'
        if let Some(index) = self.langsys_index(script_index, hb_tag_t::default_language()) {
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
    ) -> Option<(u16, hb_tag_t)> {
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
        feature_tag: hb_tag_t,
    ) -> Option<u16> {
        self.langsys(script_index, lang_index)?
            .feature_index_for_tag(&self.feature_list()?, feature_tag)
    }
}

fn coverage_index(coverage: Result<CoverageTable, ReadError>, gid: GlyphId) -> Option<u16> {
    coverage.ok().and_then(|coverage| coverage.get(gid))
}

fn coverage_index_cached(
    coverage: impl Fn(GlyphId) -> Option<u16>,
    gid: GlyphId,
    cache: &MappingCache,
) -> Option<u16> {
    if let Some(index) = cache.get(gid.into()) {
        if index == MappingCache::MAX_VALUE {
            None
        } else {
            Some(index as u16)
        }
    } else {
        let index = coverage(gid);
        if let Some(index) = index {
            if (index as u32) < MappingCache::MAX_VALUE {
                cache.set(gid.into(), index as u32);
            }
            Some(index)
        } else {
            cache.set(gid.into(), MappingCache::MAX_VALUE);
            None
        }
    }
}

fn coverage_binary_cached(
    coverage: impl Fn(GlyphId) -> Option<u16>,
    gid: GlyphId,
    cache: &BinaryCache,
) -> Option<bool> {
    if let Some(index) = cache.get(gid.into()) {
        if index == BinaryCache::MAX_VALUE {
            None
        } else {
            Some(true)
        }
    } else {
        let index = coverage(gid);
        if index.is_some() {
            cache.set(gid.into(), 0);
            Some(true)
        } else {
            cache.set(gid.into(), BinaryCache::MAX_VALUE);
            None
        }
    }
}

fn covered(coverage: Result<CoverageTable, ReadError>, gid: GlyphId) -> bool {
    coverage_index(coverage, gid).is_some()
}

fn glyph_class(class_def: Result<ClassDef, ReadError>, gid: GlyphId) -> u16 {
    class_def
        .map(|class_def| class_def.get(gid))
        .unwrap_or_default()
}

fn glyph_class_cached(
    class_def: impl Fn(GlyphId) -> u16,
    gid: GlyphId,
    cache: &MappingCache,
) -> u16 {
    if let Some(index) = cache.get(gid.into()) {
        index as u16
    } else {
        let index = class_def(gid);
        cache.set(gid.into(), index as u32);
        index
    }
}

#[derive(Copy, Clone, Default, Debug)]
pub(crate) struct CoverageInfo {
    pub offset: u16,
    pub format: u16,
    pub count: u16,
}

impl CoverageInfo {
    pub fn new(parent_data: &FontData, offset: u16) -> Option<Self> {
        if offset == 0 {
            return None;
        }
        let format = parent_data.read_at::<u16>(offset as usize).ok()?;
        if format != 1 && format != 2 {
            return None;
        }
        let count = parent_data.read_at::<u16>(offset as usize + 2).ok()?;
        Some(Self {
            offset,
            format,
            count,
        })
    }

    pub fn index(&self, parent_data: &FontData, gid: GlyphId) -> Option<u16> {
        if self.offset == 0 {
            return None;
        }
        let gid = gid.to_u32();
        let data_offset = self.offset as usize + 4;
        let len = self.count as usize;
        if self.format == 1 {
            let glyphs = parent_data
                .read_array::<BigEndian<GlyphId16>>(data_offset..data_offset + len * 2)
                .ok()?;
            glyphs
                .binary_search_by_key(&gid, |g| g.get().to_u32())
                .ok()
                .map(|idx| idx as _)
        } else {
            use core::cmp::Ordering;
            let records = parent_data
                .read_array::<RangeRecord>(
                    data_offset..data_offset + len * size_of::<RangeRecord>(),
                )
                .ok()?;
            records
                .binary_search_by(|rec| {
                    if rec.end_glyph_id().to_u32() < gid {
                        Ordering::Less
                    } else if rec.start_glyph_id().to_u32() > gid {
                        Ordering::Greater
                    } else {
                        Ordering::Equal
                    }
                })
                .ok()
                .map(|idx| {
                    let rec = &records[idx];
                    (rec.start_coverage_index() as u32 + gid - rec.start_glyph_id().to_u32()) as u16
                })
        }
    }
}

#[derive(Copy, Clone, Default, Debug)]
pub(crate) struct ClassDefInfo {
    pub offset: u16,
    pub format: u16,
    // For format 1 only
    pub start_glyph_id: u16,
    pub count: u16,
}

impl ClassDefInfo {
    pub fn new(parent_data: &FontData, offset: u16) -> Option<Self> {
        if offset == 0 {
            return None;
        }
        let format = parent_data.read_at::<u16>(offset as usize).ok()?;
        if format != 1 && format != 2 {
            return None;
        }
        let (start_glyph_id, count) = if format == 1 {
            let start_glyph_id = parent_data.read_at::<u16>(offset as usize + 2).ok()?;
            let count = parent_data.read_at::<u16>(offset as usize + 4).ok()?;
            (start_glyph_id, count)
        } else if format == 2 {
            let count = parent_data.read_at::<u16>(offset as usize + 2).ok()?;
            (0, count)
        } else {
            return None;
        };
        Some(Self {
            offset,
            format,
            start_glyph_id,
            count,
        })
    }

    pub fn class(&self, parent_data: &FontData, gid: GlyphId) -> u16 {
        let offset = self.offset as usize;
        if offset == 0 {
            return 0;
        }
        let gid = gid.to_u32();
        if self.format == 1 {
            let Some(idx) = gid.checked_sub(self.start_glyph_id as u32) else {
                return 0;
            };
            if idx >= self.count as u32 {
                return 0;
            }
            parent_data
                .read_at::<u16>(offset + 6 + idx as usize * 2)
                .unwrap_or(0)
        } else {
            use core::cmp::Ordering;
            let start = offset + 4;
            let end = start + self.count as usize * size_of::<ClassRangeRecord>();
            let Ok(records) = parent_data.read_array::<ClassRangeRecord>(start..end) else {
                return 0;
            };
            records
                .binary_search_by(|rec| {
                    if rec.end_glyph_id().to_u32() < gid {
                        Ordering::Less
                    } else if rec.start_glyph_id().to_u32() > gid {
                        Ordering::Greater
                    } else {
                        Ordering::Equal
                    }
                })
                .ok()
                .map_or(0, |idx| records[idx].class())
        }
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
