//! try to define Subset trait so I can add methods for Hmtx
//! TODO: make it generic for all tables
mod base;
mod cblc;
mod cmap;
mod colr;
mod cpal;
mod fnv;
mod fvar;
mod gdef;
mod glyf_loca;
mod gpos;
mod graph;
mod gsub;
mod gsubgpos;
mod gvar;
mod hdmx;
mod head;
mod hmtx;
mod hvar;
mod inc_bimap;
mod layout;
mod maxp;
mod name;
mod offset;
mod offset_array;
mod os2;
mod parsing_util;
mod post;
mod priority_queue;
mod repack;
mod sbix;
pub mod serialize;
mod stat;
mod unicode_closure;
mod variations;
mod vmtx;
mod vorg;
mod vvar;
use crate::repack::resolve_overflows;
use gdef::CollectUsedMarkSets;
use inc_bimap::IncBiMap;
use layout::{
    collect_features_with_retained_subs, find_duplicate_features, prune_features,
    remap_feature_indices, PruneLangSysContext, SubsetLayoutContext,
};
pub use parsing_util::{
    parse_name_ids, parse_name_languages, parse_tag_list, parse_unicodes, populate_gids,
};
use unicode_closure::unicode_closure;

use fnv::FnvHashMap;
use serialize::{SerializeErrorFlags, Serializer};
use skrifa::MetadataProvider;
use thiserror::Error;
use write_fonts::{
    read::{
        collections::{int_set::Domain, IntSet},
        tables::{
            avar::Avar,
            base::Base,
            cbdt::Cbdt,
            cblc::Cblc,
            cff::Cff,
            cff2::Cff2,
            cmap::{Cmap, CmapSubtable, PlatformId},
            colr::Colr,
            cpal::Cpal,
            cvar::Cvar,
            fvar::Fvar,
            gasp,
            gdef::Gdef,
            glyf::{Glyf, Glyph},
            gpos::Gpos,
            gsub::Gsub,
            gvar::Gvar,
            hdmx::Hdmx,
            head::Head,
            hhea::Hhea,
            hmtx::Hmtx,
            hvar::Hvar,
            loca::Loca,
            maxp::Maxp,
            name::Name,
            os2::Os2,
            post::Post,
            sbix::Sbix,
            stat::Stat,
            variations::NO_VARIATION_INDEX,
            vhea::Vhea,
            vmtx::Vmtx,
            vorg::Vorg,
            vvar::Vvar,
        },
        types::{GlyphId, NameId, Tag},
        FontRef, TableProvider, TopLevelTable,
    },
    FontBuilder,
};

const MAX_COMPOSITE_OPERATIONS_PER_GLYPH: u8 = 64;
const MAX_NESTING_LEVEL: u8 = 64;
// Support 24-bit gids. This should probably be extended to u32::MAX but
// this causes tests to fail with 'subtract with overflow error'.
// See <https://github.com/googlefonts/fontations/issues/997>
const MAX_GID: GlyphId = GlyphId::new(0xFFFFFFFF);

// ref: <https://github.com/harfbuzz/harfbuzz/blob/689383d05b2609a67efa307a9860b85cf40bf8c3/src/hb-subset-input.cc#L82>
pub static DEFAULT_LAYOUT_FEATURES: &[Tag] = &[
    // default shaper
    // common
    Tag::new(b"rvrn"),
    Tag::new(b"ccmp"),
    Tag::new(b"liga"),
    Tag::new(b"locl"),
    Tag::new(b"mark"),
    Tag::new(b"mkmk"),
    Tag::new(b"rlig"),
    //fractions
    Tag::new(b"frac"),
    Tag::new(b"numr"),
    Tag::new(b"dnom"),
    // horizontal
    Tag::new(b"calt"),
    Tag::new(b"clig"),
    Tag::new(b"curs"),
    Tag::new(b"kern"),
    Tag::new(b"rclt"),
    //vertical
    Tag::new(b"valt"),
    Tag::new(b"vert"),
    Tag::new(b"vkrn"),
    Tag::new(b"vpal"),
    Tag::new(b"vrt2"),
    //ltr
    Tag::new(b"ltra"),
    Tag::new(b"ltrm"),
    //rtl
    Tag::new(b"rtla"),
    Tag::new(b"rtlm"),
    //random
    Tag::new(b"rand"),
    //justify
    Tag::new(b"jalt"),
    //east asian spacing
    Tag::new(b"chws"),
    Tag::new(b"vchw"),
    Tag::new(b"halt"),
    Tag::new(b"vhal"),
    Tag::new(b"palt"),
    //private
    Tag::new(b"Harf"),
    Tag::new(b"HARF"),
    Tag::new(b"Buzz"),
    Tag::new(b"BUZZ"),
    //complex shapers
    //arabic
    Tag::new(b"init"),
    Tag::new(b"medi"),
    Tag::new(b"fina"),
    Tag::new(b"isol"),
    Tag::new(b"med2"),
    Tag::new(b"fin2"),
    Tag::new(b"fin3"),
    Tag::new(b"cswh"),
    Tag::new(b"mset"),
    Tag::new(b"stch"),
    //hangul
    Tag::new(b"ljmo"),
    Tag::new(b"vjmo"),
    Tag::new(b"tjmo"),
    //tibetan
    Tag::new(b"abvs"),
    Tag::new(b"blws"),
    Tag::new(b"abvm"),
    Tag::new(b"blwm"),
    //indic
    Tag::new(b"nukt"),
    Tag::new(b"akhn"),
    Tag::new(b"rphf"),
    Tag::new(b"rkrf"),
    Tag::new(b"pref"),
    Tag::new(b"blwf"),
    Tag::new(b"half"),
    Tag::new(b"abvf"),
    Tag::new(b"pstf"),
    Tag::new(b"cfar"),
    Tag::new(b"vatu"),
    Tag::new(b"cjct"),
    Tag::new(b"init"),
    Tag::new(b"pres"),
    Tag::new(b"abvs"),
    Tag::new(b"blws"),
    Tag::new(b"psts"),
    Tag::new(b"haln"),
    Tag::new(b"dist"),
    Tag::new(b"abvm"),
    Tag::new(b"blwm"),
];

//ref: <https://github.com/harfbuzz/harfbuzz/blob/5cbce05a7c/src/hb-subset-input.cc#L46>
pub static DEFAULT_DROP_TABLES: &[Tag] = &[
    // Layout disabled by default
    Tag::new(b"morx"),
    Tag::new(b"mort"),
    Tag::new(b"kerx"),
    Tag::new(b"kern"),
    // Copied from fontTools:
    Tag::new(b"JSTF"),
    Tag::new(b"DSIG"),
    Tag::new(b"EBDT"),
    Tag::new(b"EBLC"),
    Tag::new(b"EBSC"),
    Tag::new(b"SVG "),
    Tag::new(b"PCLT"),
    Tag::new(b"LTSH"),
    // Graphite tables
    Tag::new(b"Feat"),
    Tag::new(b"Glat"),
    Tag::new(b"Gloc"),
    Tag::new(b"Silf"),
    Tag::new(b"Sill"),
];

#[derive(Clone, Copy, Debug)]
pub struct SubsetFlags(u16);

impl SubsetFlags {
    /// all flags at their default value of false.
    pub const SUBSET_FLAGS_DEFAULT: Self = Self(0x0000);

    /// If set hinting instructions will be dropped in the produced subset.
    /// Otherwise hinting instructions will be retained.
    pub const SUBSET_FLAGS_NO_HINTING: Self = Self(0x0001);

    /// If set glyph indices will not be modified in the produced subset.
    /// If glyphs are dropped their indices will be retained as an empty glyph.
    pub const SUBSET_FLAGS_RETAIN_GIDS: Self = Self(0x0002);

    /// If set and subsetting a CFF font the subsetter will attempt to remove subroutines from the CFF glyphs.
    /// This flag is UNIMPLEMENTED yet
    pub const SUBSET_FLAGS_DESUBROUTINIZE: Self = Self(0x0004);

    /// If set non-unicode name records will be retained in the subset.
    /// This flag is UNIMPLEMENTED yet
    pub const SUBSET_FLAGS_NAME_LEGACY: Self = Self(0x0008);

    /// If set the subsetter will set the OVERLAP_SIMPLE flag on each simple glyph.
    pub const SUBSET_FLAGS_SET_OVERLAPS_FLAG: Self = Self(0x0010);

    /// If set the subsetter will not drop unrecognized tables and instead pass them through untouched.
    pub const SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED: Self = Self(0x0020);

    /// If set the notdef glyph outline will be retained in the final subset.
    pub const SUBSET_FLAGS_NOTDEF_OUTLINE: Self = Self(0x0040);

    /// If set the PS glyph names will be retained in the final subset.
    pub const SUBSET_FLAGS_GLYPH_NAMES: Self = Self(0x0080);

    /// If set then the unicode ranges in OS/2 will not be recalculated.
    pub const SUBSET_FLAGS_NO_PRUNE_UNICODE_RANGES: Self = Self(0x0100);

    /// If set don't perform glyph closure on layout substitution rules (GSUB)
    pub const SUBSET_FLAGS_NO_LAYOUT_CLOSURE: Self = Self(0x0200);

    /// If set perform IUP delta optimization on the remaining gvar table's deltas.
    /// This flag is UNIMPLEMENTED yet
    pub const SUBSET_FLAGS_OPTIMIZE_IUP_DELTAS: Self = Self(0x0400);

    /// If set do not pull mirrored versions of input codepoints into the subset.
    pub const SUBSET_FLAGS_NO_BIDI_CLOSURE: Self = Self(0x0800);

    /// Returns `true` if all of the flags in `other` are contained within `self`.
    #[inline]
    pub const fn contains(&self, other: Self) -> bool {
        (self.0 & other.0) == other.0
    }
}

impl Default for SubsetFlags {
    fn default() -> Self {
        Self::SUBSET_FLAGS_DEFAULT
    }
}

impl PartialEq for SubsetFlags {
    fn eq(&self, other: &Self) -> bool {
        self.0 == other.0
    }
}

impl std::ops::BitOr for SubsetFlags {
    type Output = Self;

    /// Returns the union of the two sets of flags.
    #[inline]
    fn bitor(self, other: SubsetFlags) -> Self {
        Self(self.0 | other.0)
    }
}

impl From<u16> for SubsetFlags {
    fn from(value: u16) -> Self {
        Self(value)
    }
}

impl std::ops::BitOrAssign for SubsetFlags {
    /// Adds the set of flags.
    #[inline]
    fn bitor_assign(&mut self, other: Self) {
        self.0 |= other.0;
    }
}

#[allow(dead_code)]
#[derive(Default)]
pub struct Plan {
    unicodes: IntSet<u32>,
    glyphs_requested: IntSet<GlyphId>,
    glyphset_gsub: IntSet<GlyphId>,
    glyphset_colred: IntSet<GlyphId>,
    glyphset: IntSet<GlyphId>,
    /// Old->New glyph id mapping,
    glyph_map: FnvHashMap<GlyphId, GlyphId>,
    // Old->New glyph id (in glyph_set_gsub) mapping
    glyph_map_gsub: FnvHashMap<GlyphId, GlyphId>,
    /// New->Old glyph id mapping,
    reverse_glyph_map: FnvHashMap<GlyphId, GlyphId>,

    new_to_old_gid_list: Vec<(GlyphId, GlyphId)>,

    num_output_glyphs: usize,
    font_num_glyphs: usize,
    unicode_to_new_gid_list: Vec<(u32, GlyphId)>,
    codepoint_to_glyph: FnvHashMap<u32, GlyphId>,

    subset_flags: SubsetFlags,
    no_subset_tables: IntSet<Tag>,
    drop_tables: IntSet<Tag>,
    name_ids: IntSet<NameId>,
    name_languages: IntSet<u16>,
    layout_scripts: IntSet<Tag>,
    layout_features: IntSet<Tag>,

    //active old->new feature index map after removing redundant langsys and prune_features
    gsub_features: FnvHashMap<u16, u16>,
    gpos_features: FnvHashMap<u16, u16>,

    //active features(with duplicates) old->new feature index map, used by Script/FeatureVariations
    gsub_features_w_duplicates: FnvHashMap<u16, u16>,
    gpos_features_w_duplicates: FnvHashMap<u16, u16>,

    // active old->new lookup index map
    gsub_lookups: FnvHashMap<u16, u16>,
    gpos_lookups: FnvHashMap<u16, u16>,

    // active script-langsys
    gsub_script_langsys: FnvHashMap<u16, IntSet<u16>>,
    gpos_script_langsys: FnvHashMap<u16, IntSet<u16>>,

    // used_mark_sets mapping: old->new
    used_mark_sets_map: FnvHashMap<u16, u16>,

    //old->new colrv1 layer index map
    colrv1_layers: FnvHashMap<u32, u32>,
    //old->new CPAL palette index map
    colr_palettes: FnvHashMap<u16, u16>,
    // COLR varstore retained varidx mapping
    colr_varstore_inner_maps: Vec<IncBiMap>,
    // COLR table old variation index -> (New varidx, new delta) mapping
    colr_varidx_delta_map: FnvHashMap<u32, (u32, i32)>,
    // COLR table new delta set index -> new var index mapping
    colr_new_deltaset_idx_varidx_map: FnvHashMap<u32, u32>,

    os2_info: Os2Info,

    //BASE table old variation index -> (New varidx, new delta) mapping
    base_varidx_delta_map: FnvHashMap<u32, (u32, i32)>,
    //BASE table varstore retained varidx mapping
    base_varstore_inner_maps: Vec<IncBiMap>,

    //Old layout item variation index -> (New varidx, delta) mapping
    layout_varidx_delta_map: FnvHashMap<u32, (u32, i32)>,
    //GDEF table varstore retained varidx mapping
    gdef_varstore_inner_maps: Vec<IncBiMap>,
}

#[derive(Default)]
struct Os2Info {
    min_cmap_codepoint: u32,
    max_cmap_codepoint: u32,
}

impl Plan {
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        input_gids: &IntSet<GlyphId>,
        input_unicodes: &IntSet<u32>,
        font: &FontRef,
        flags: SubsetFlags,
        drop_tables: &IntSet<Tag>,
        layout_scripts: &IntSet<Tag>,
        layout_features: &IntSet<Tag>,
        name_ids: &IntSet<NameId>,
        name_languages: &IntSet<u16>,
    ) -> Self {
        let mut this = Plan {
            glyphs_requested: input_gids.clone(),
            font_num_glyphs: get_font_num_glyphs(font),
            subset_flags: flags,
            drop_tables: drop_tables.clone(),
            layout_scripts: layout_scripts.clone(),
            layout_features: layout_features.clone(),
            name_ids: name_ids.clone(),
            name_languages: name_languages.clone(),
            ..Default::default()
        };

        // ref: <https://github.com/harfbuzz/harfbuzz/blob/b5a65e0f20c30a7f13b2f6619479a6d666e603e0/src/hb-subset-input.cc#L71>
        let default_no_subset_tables = [gasp::Gasp::TAG, FPGM, PREP, VDMX, DSIG];
        this.no_subset_tables
            .extend(default_no_subset_tables.iter().copied());

        this.populate_unicodes_to_retain(input_gids, input_unicodes, font);
        this.populate_gids_to_retain(font);
        this.create_old_gid_to_new_gid_map();

        this.create_glyph_map_gsub();
        //update the unicode to new gid list
        let num = this.unicode_to_new_gid_list.len();
        for i in 0..num {
            let old_gid = this.unicode_to_new_gid_list[i].1;
            let new_gid = this.glyph_map.get(&old_gid).unwrap();
            this.unicode_to_new_gid_list[i].1 = *new_gid;
        }
        this.collect_base_var_indices(font);
        this
    }

    fn populate_unicodes_to_retain(
        &mut self,
        input_gids: &IntSet<GlyphId>,
        input_unicodes: &IntSet<u32>,
        font: &FontRef,
    ) {
        let unicodes = unicode_closure(
            input_unicodes,
            !self
                .subset_flags
                .contains(SubsetFlags::SUBSET_FLAGS_NO_BIDI_CLOSURE),
        );
        let charmap = font.charmap();
        if input_gids.is_empty() && unicodes.len() < (self.font_num_glyphs as u64) {
            let cap: usize = unicodes.len().try_into().unwrap_or(usize::MAX);
            self.unicode_to_new_gid_list.reserve(cap);
            self.codepoint_to_glyph.reserve(cap);
            //TODO: add support for subset accelerator?

            for cp in unicodes.iter() {
                match charmap.map(cp) {
                    Some(gid) => {
                        self.codepoint_to_glyph.insert(cp, gid);
                        self.unicode_to_new_gid_list.push((cp, gid));
                    }
                    None => {
                        continue;
                    }
                }
            }
        } else {
            //TODO: add support for subset accelerator?
            let cmap_unicodes = charmap.mappings().map(|t| t.0).collect::<IntSet<u32>>();
            let unicode_gid_map = charmap.mappings().collect::<FnvHashMap<u32, GlyphId>>();

            let vec_cap: u64 = input_gids.len() + unicodes.len();
            let vec_cap: usize = vec_cap
                .min(cmap_unicodes.len())
                .try_into()
                .unwrap_or(usize::MAX);
            self.codepoint_to_glyph.reserve(vec_cap);
            self.unicode_to_new_gid_list.reserve(vec_cap);
            for range in cmap_unicodes.iter_ranges() {
                for cp in range {
                    match unicode_gid_map.get(&cp) {
                        Some(gid) => {
                            if !input_gids.contains(*gid) && !unicodes.contains(cp) {
                                continue;
                            }
                            self.codepoint_to_glyph.insert(cp, *gid);
                            self.unicode_to_new_gid_list.push((cp, *gid));
                        }
                        None => {
                            continue;
                        }
                    }
                }
            }

            /* Add gids which where requested, but not mapped in cmap */
            for range in input_gids.iter_ranges() {
                if range.start().to_u32() as usize >= self.font_num_glyphs {
                    break;
                }
                let mut last = range.end().to_u32() as usize;
                if last >= self.font_num_glyphs {
                    last = self.font_num_glyphs - 1;
                }
                self.glyphset_gsub
                    .insert_range(*range.start()..=GlyphId::from(last as u32));
            }
        }
        self.unicode_to_new_gid_list.sort();
        self.glyphset_gsub
            .extend(self.unicode_to_new_gid_list.iter().map(|t| t.1));
        self.unicodes
            .extend(self.unicode_to_new_gid_list.iter().map(|t| t.0));

        // ref: <https://github.com/harfbuzz/harfbuzz/blob/e451e91ec3608a2ebfec34d0c4f0b3d880e00e33/src/hb-subset-plan.cc#L802>
        self.os2_info.min_cmap_codepoint = self.unicodes.first().unwrap_or(0xFFFF_u32);
        self.os2_info.max_cmap_codepoint = self.unicodes.last().unwrap_or(0xFFFF_u32);

        self.collect_variation_selectors(font, &unicodes);
    }

    fn collect_variation_selectors(&mut self, font: &FontRef, input_unicodes: &IntSet<u32>) {
        if let Ok(cmap) = font.cmap() {
            let encoding_records = cmap.encoding_records();
            if let Ok(i) = encoding_records.binary_search_by(|r| {
                if r.platform_id() != PlatformId::Unicode {
                    r.platform_id().cmp(&PlatformId::Unicode)
                } else if r.encoding_id() != 5 {
                    r.encoding_id().cmp(&5)
                } else {
                    std::cmp::Ordering::Equal
                }
            }) {
                if let Ok(CmapSubtable::Format14(cmap14)) = encoding_records
                    .get(i)
                    .unwrap()
                    .subtable(cmap.offset_data())
                {
                    self.unicodes.extend(
                        cmap14
                            .var_selector()
                            .iter()
                            .map(|s| s.var_selector().to_u32())
                            .filter(|v| input_unicodes.contains(*v)),
                    );
                }
            }
        }
    }

    fn populate_gids_to_retain(&mut self, font: &FontRef) {
        //not-def
        self.glyphset_gsub.insert(GlyphId::NOTDEF);

        //glyph closure for cmap
        if let Ok(cmap) = font.cmap() {
            cmap.closure_glyphs(&self.unicodes, &mut self.glyphset_gsub);
        }

        // layout closure
        self.layout_populate_gids_to_retain(font);
        remove_invalid_gids(&mut self.glyphset_gsub, self.font_num_glyphs);

        //skip glyph closure for MATH table, it's not supported yet

        //glyph closure for COLR
        if !self.drop_tables.contains(Tag::new(b"COLR")) {
            self.colr_closure(font);
            remove_invalid_gids(&mut self.glyphset_colred, self.font_num_glyphs);
        } else {
            self.glyphset_colred = self.glyphset_gsub.clone();
        }

        /* Populate a full set of glyphs to retain by adding all referenced composite glyphs. */
        if let Ok(loca) = font.loca(None) {
            let glyf = font.glyf().expect("Error reading glyf table");
            let operation_count =
                self.glyphset_gsub.len() * (MAX_COMPOSITE_OPERATIONS_PER_GLYPH as u64);
            for gid in self.glyphset_colred.iter() {
                glyf_closure_glyphs(
                    &loca,
                    &glyf,
                    gid,
                    &mut self.glyphset,
                    operation_count as i32,
                    0,
                );
            }
            remove_invalid_gids(&mut self.glyphset, self.font_num_glyphs);
        } else {
            self.glyphset = self.glyphset_colred.clone();
        }

        self.nameid_closure(font);
        self.collect_layout_var_indices(font);
    }

    fn layout_populate_gids_to_retain(&mut self, font: &FontRef) {
        if !self.drop_tables.contains(Tag::new(b"GSUB")) {
            if let Ok(gsub) = font.gsub() {
                gsub.closure_glyphs_lookups_features(self);
            }
        }

        if !self.drop_tables.contains(Tag::new(b"GPOS")) {
            if let Ok(gpos) = font.gpos() {
                gpos.closure_glyphs_lookups_features(self);
            }
        }
    }

    fn create_old_gid_to_new_gid_map(&mut self) {
        let pop = self.glyphset.len();
        self.glyph_map.reserve(pop as usize);
        self.reverse_glyph_map.reserve(pop as usize);
        self.new_to_old_gid_list.reserve(pop as usize);

        //TODO: Add support for requested_glyph_map, command line option --gid-map
        if !self
            .subset_flags
            .contains(SubsetFlags::SUBSET_FLAGS_RETAIN_GIDS)
        {
            self.new_to_old_gid_list.extend(
                self.glyphset
                    .iter()
                    .zip(0u16..)
                    .map(|x| (GlyphId::from(x.1), x.0)),
            );
            self.num_output_glyphs = self.new_to_old_gid_list.len();
        } else {
            self.new_to_old_gid_list
                .extend(self.glyphset.iter().map(|x| (x, x)));
            let Some(max_glyph) = self.glyphset.last() else {
                return;
            };
            self.num_output_glyphs = max_glyph.to_u32() as usize + 1;
        }
        self.glyph_map
            .extend(self.new_to_old_gid_list.iter().map(|x| (x.1, x.0)));
        self.reverse_glyph_map
            .extend(self.new_to_old_gid_list.iter().map(|x| (x.0, x.1)));
    }

    fn create_glyph_map_gsub(&mut self) {
        let map: FnvHashMap<GlyphId, GlyphId> = self
            .glyphset_gsub
            .iter()
            .filter_map(|g| self.glyph_map.get(&g).map(|new_gid| (g, *new_gid)))
            .collect();
        let _ = std::mem::replace(&mut self.glyph_map_gsub, map);
    }

    fn colr_closure(&mut self, font: &FontRef) {
        if let Ok(colr) = font.colr() {
            colr.v0_closure_glyphs(&self.glyphset_gsub, &mut self.glyphset_colred);
            let mut layer_indices = IntSet::empty();
            let mut palette_indices = IntSet::empty();
            let mut variation_indices = IntSet::empty();
            colr.v1_closure(
                &mut self.glyphset_colred,
                &mut layer_indices,
                &mut palette_indices,
                &mut variation_indices,
            );

            colr.v0_closure_palette_indices(&self.glyphset_colred, &mut palette_indices);
            let _ = std::mem::replace(&mut self.colrv1_layers, remap_indices(layer_indices));
            let _ = std::mem::replace(
                &mut self.colr_palettes,
                remap_palette_indices(palette_indices),
            );

            if variation_indices.is_empty() {
                return;
            }

            // generate 3 maps:
            // colr_varidx_delta_map
            // When delta set index map is not included, it's a mapping from varIdx-> (new varIdx,delta).
            // Otherwise, it's a mapping from old delta set idx-> (new delta set idx, delta).
            // Mapping delta set indices is the same as gid mapping.
            //
            // colr_varstore_inner_maps:
            // mapping from old varidx -> new varidx
            //
            // colr_new_deltaset_idx_varidx_map:
            // generate new delta set idx-> new var_idx map if DeltsSetIndexMap exists
            if let Some(Ok(var_store)) = colr.item_variation_store() {
                let vardata_count = var_store.item_variation_data_count() as u32;
                let Ok(var_index_map) = colr.var_index_map().transpose() else {
                    return;
                };

                let mut delta_set_indices = IntSet::empty();
                let mut deltaset_idx_var_idx_map = FnvHashMap::default();
                // when a DeltaSetIndexMap is included, collected variation indices are actually delta set indices,
                // we need to map them into variation indices
                if let Some(var_index_map) = &var_index_map {
                    delta_set_indices.extend(variation_indices.iter());
                    variation_indices.clear();
                    for idx in delta_set_indices.iter() {
                        if let Ok(var_idx) = var_index_map.get(idx) {
                            let var_idx = ((var_idx.outer as u32) << 16) + var_idx.inner as u32;
                            variation_indices.insert(var_idx);
                            deltaset_idx_var_idx_map.insert(idx, var_idx);
                        }
                    }
                }
                remap_variation_indices(
                    vardata_count,
                    &variation_indices,
                    &mut self.colr_varidx_delta_map,
                );
                generate_varstore_inner_maps(
                    &variation_indices,
                    vardata_count,
                    &mut self.colr_varstore_inner_maps,
                );

                // if DeltaSetIndexMap exists, we need to use deltaset index instead of var_idx
                if var_index_map.is_some() {
                    let (new_deltaset_idx_varidx_map, deltaset_idx_delta_map) =
                        remap_delta_set_indices(
                            &delta_set_indices,
                            &deltaset_idx_var_idx_map,
                            &self.colr_varidx_delta_map,
                        );
                    let _ = std::mem::replace(
                        &mut self.colr_new_deltaset_idx_varidx_map,
                        new_deltaset_idx_varidx_map,
                    );
                    let _ =
                        std::mem::replace(&mut self.colr_varidx_delta_map, deltaset_idx_delta_map);
                }
            }
        } else {
            self.glyphset_colred.union(&self.glyphset_gsub);
        }
    }

    fn nameid_closure(&mut self, font: &FontRef) {
        if !self.drop_tables.contains(Tag::new(b"STAT")) {
            if let Ok(stat) = font.stat() {
                stat.collect_name_ids(self);
            }
        };

        //TODO: skip fvar table when all axes are pinned
        if !self.drop_tables.contains(Tag::new(b"fvar")) {
            if let Ok(fvar) = font.fvar() {
                fvar.collect_name_ids(self);
            }
        }

        if !self.drop_tables.contains(Tag::new(b"CPAL")) {
            if let Ok(cpal) = font.cpal() {
                cpal.collect_name_ids(self);
            }
        }

        if !self.drop_tables.contains(Tag::new(b"GSUB")) {
            if let Ok(gsub) = font.gsub() {
                gsub.collect_name_ids(self);
            }
        }

        if !self.drop_tables.contains(Tag::new(b"GPOS")) {
            if let Ok(gpos) = font.gpos() {
                gpos.collect_name_ids(self);
            }
        }
    }

    fn collect_layout_var_indices(&mut self, font: &FontRef) {
        if self.drop_tables.contains(Tag::new(b"GDEF")) {
            return;
        }
        let Ok(gdef) = font.gdef() else {
            return;
        };

        let mut used_mark_sets = IntSet::empty();
        gdef.collect_used_mark_sets(self, &mut used_mark_sets);
        let _ = std::mem::replace(&mut self.used_mark_sets_map, remap_indices(used_mark_sets));

        let Some(Ok(var_store)) = gdef.item_var_store() else {
            return;
        };
        let mut varidx_set = IntSet::empty();
        gdef.collect_variation_indices(self, &mut varidx_set);

        if let Ok(gpos) = font.gpos() {
            gpos.collect_variation_indices(self, &mut varidx_set);
        }

        let vardata_count = var_store.item_variation_data_count() as u32;
        remap_variation_indices(
            vardata_count,
            &varidx_set,
            &mut self.layout_varidx_delta_map,
        );

        generate_varstore_inner_maps(
            &varidx_set,
            vardata_count,
            &mut self.gdef_varstore_inner_maps,
        );
    }

    fn collect_base_var_indices(&mut self, font: &FontRef) {
        if self.drop_tables.contains(Tag::new(b"BASE")) {
            return;
        }

        if font.fvar().is_err() {
            return;
        }
        let Ok(base) = font.base() else {
            return;
        };

        let Some(Ok(var_store)) = base.item_var_store() else {
            return;
        };

        let mut varidx_set = IntSet::empty();
        {
            base.collect_variation_indices(self, &mut varidx_set);
        }

        let vardata_count = var_store.item_variation_data_count() as u32;
        remap_variation_indices(vardata_count, &varidx_set, &mut self.base_varidx_delta_map);
        generate_varstore_inner_maps(
            &varidx_set,
            vardata_count,
            &mut self.base_varstore_inner_maps,
        );
    }
}

// TODO: when instancing, calculate delta value and set new varidx to NO_VARIATIONS_IDX if all axes are pinned
fn remap_variation_indices(
    vardata_count: u32,
    varidx_set: &IntSet<u32>,
    varidx_delta_map: &mut FnvHashMap<u32, (u32, i32)>,
) {
    if vardata_count == 0 || varidx_set.is_empty() {
        return;
    }

    let mut new_major: u32 = 0;
    let mut new_minor: u32 = 0;
    let mut last_major = varidx_set.first().unwrap() >> 16;
    for var_idx in varidx_set.iter() {
        let major = var_idx >> 16;
        if major >= vardata_count {
            break;
        }

        if major != last_major {
            new_minor = 0;
            new_major += 1;
        }

        let new_idx = (new_major << 16) + new_minor;
        varidx_delta_map.insert(var_idx, (new_idx, 0));

        new_minor += 1;
        last_major = major;
    }
}

fn generate_varstore_inner_maps(
    varidx_set: &IntSet<u32>,
    vardata_count: u32,
    inner_maps: &mut Vec<IncBiMap>,
) {
    if varidx_set.is_empty() || vardata_count == 0 {
        return;
    }

    inner_maps.resize_with(vardata_count as usize, Default::default);
    for idx in varidx_set.iter() {
        let major = idx >> 16;
        let minor = idx & 0xFFFF;
        if major >= vardata_count {
            break;
        }

        inner_maps[major as usize].add(minor);
    }
}
//
fn remap_delta_set_indices(
    delta_set_indices: &IntSet<u32>,
    deltaset_idx_var_idx_map: &FnvHashMap<u32, u32>,
    varidx_delta_map: &FnvHashMap<u32, (u32, i32)>,
) -> (FnvHashMap<u32, u32>, FnvHashMap<u32, (u32, i32)>) {
    let mut new_deltaset_idx_varidx_map = FnvHashMap::default();
    let mut deltaset_idx_delta_map = FnvHashMap::default();
    let mut new_idx = 0_u32;

    for deltaset_idx in delta_set_indices.iter() {
        let Some(&var_idx) = deltaset_idx_var_idx_map.get(&deltaset_idx) else {
            continue;
        };

        let (new_var_idx, delta) = if var_idx == NO_VARIATION_INDEX {
            (var_idx, 0)
        } else if let Some((new_idx, d)) = varidx_delta_map.get(&var_idx) {
            (*new_idx, *d)
        } else {
            continue;
        };

        new_deltaset_idx_varidx_map.insert(new_idx, new_var_idx);
        deltaset_idx_delta_map.insert(deltaset_idx, (new_idx, delta));
        new_idx += 1;
    }
    (new_deltaset_idx_varidx_map, deltaset_idx_delta_map)
}

/// glyph closure for Composite glyphs in glyf table
/// limit the number of operations through returning an operation count
fn glyf_closure_glyphs(
    loca: &Loca,
    glyf: &Glyf,
    gid: GlyphId,
    gids_to_retain: &mut IntSet<GlyphId>,
    operation_count: i32,
    depth: u8,
) -> i32 {
    if gids_to_retain.contains(gid) {
        return operation_count;
    }
    gids_to_retain.insert(gid);

    if depth > MAX_NESTING_LEVEL {
        return operation_count;
    }
    let depth = depth + 1;

    let mut operation_count = operation_count - 1;
    if operation_count < 0 {
        return operation_count;
    }

    if let Some(Glyph::Composite(glyph)) = loca.get_glyf(gid, glyf).ok().flatten() {
        for child in glyph.components() {
            operation_count = glyf_closure_glyphs(
                loca,
                glyf,
                child.glyph.into(),
                gids_to_retain,
                operation_count,
                depth,
            );
        }
    }
    operation_count
}

fn remove_invalid_gids(gids: &mut IntSet<GlyphId>, num_glyphs: usize) {
    gids.remove_range(GlyphId::new(num_glyphs as u32)..=MAX_GID);
}

fn get_font_num_glyphs(font: &FontRef) -> usize {
    let ret = font.loca(None).map(|loca| loca.len()).unwrap_or_default();
    let maxp = font.maxp().expect("Error reading maxp table");
    ret.max(maxp.num_glyphs() as usize)
}

pub(crate) fn remap_indices<T: Domain + std::cmp::Eq + std::hash::Hash + From<u16>>(
    indices: IntSet<T>,
) -> FnvHashMap<T, T> {
    indices
        .iter()
        .enumerate()
        .map(|x| (x.1, T::from(x.0 as u16)))
        .collect()
}

fn remap_palette_indices(indices: IntSet<u16>) -> FnvHashMap<u16, u16> {
    indices
        .iter()
        .enumerate()
        .map(|x| {
            if x.1 == 0xFFFF {
                (0xFFFF, 0xFFFF)
            } else {
                (x.1, x.0 as u16)
            }
        })
        .collect()
}

/// mutable struct, updated during table subsetting
/// some tables depend on other tables' subset output
#[derive(Default)]
pub struct SubsetState {
    // whether GDEF ItemVariationStore is retained after subsetting
    has_gdef_varstore: bool,
}

#[derive(Debug, Error)]
pub enum SubsetError {
    #[error("Invalid input gid {0}")]
    InvalidGid(String),

    #[error("Invalid gid range {start}-{end}")]
    InvalidGidRange { start: u32, end: u32 },

    #[error("Invalid input unicode {0}")]
    InvalidUnicode(String),

    #[error("Invalid unicode range {start}-{end}")]
    InvalidUnicodeRange { start: u32, end: u32 },

    #[error("Invalid tag {0}")]
    InvalidTag(String),

    #[error("Invalid ID {0}")]
    InvalidId(String),

    #[error("Subsetting table '{0}' failed")]
    SubsetTableError(Tag),
}

pub trait NameIdClosure {
    /// collect name_ids
    fn collect_name_ids(&self, plan: &mut Plan);
}

pub(crate) trait CollectVariationIndices {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>);
}

pub(crate) trait LayoutClosure {
    /// Remove unreferenced features
    fn prune_features(
        &self,
        lookup_indices: &IntSet<u16>,
        feature_indices: IntSet<u16>,
    ) -> IntSet<u16>;

    /// Return a duplicate feature(after subsetting) index map
    /// feature index -> the first index of all duplicates for this feature
    fn find_duplicate_features(
        &self,
        lookup_indices: &IntSet<u16>,
        feature_indices: IntSet<u16>,
    ) -> FnvHashMap<u16, u16>;

    //remove unreferenced langsys and return (script->langsys mapping, retained feature indices)
    fn prune_langsys(
        &self,
        duplicate_feature_index_map: &FnvHashMap<u16, u16>,
        layout_scripts: &IntSet<Tag>,
    ) -> (FnvHashMap<u16, IntSet<u16>>, IntSet<u16>);

    fn closure_glyphs_lookups_features(&self, plan: &mut Plan);
}

pub const CVT: Tag = Tag::new(b"cvt ");
pub const DSIG: Tag = Tag::new(b"DSIG");
pub const EBSC: Tag = Tag::new(b"EBSC");
pub const FPGM: Tag = Tag::new(b"fpgm");
pub const GLAT: Tag = Tag::new(b"Glat");
pub const GLOC: Tag = Tag::new(b"Gloc");
pub const JSTF: Tag = Tag::new(b"JSTF");
pub const LTSH: Tag = Tag::new(b"LTSH");
pub const MORX: Tag = Tag::new(b"morx");
pub const MORT: Tag = Tag::new(b"mort");
pub const KERX: Tag = Tag::new(b"kerx");
pub const KERN: Tag = Tag::new(b"kern");
pub const PCLT: Tag = Tag::new(b"PCLT");
pub const PREP: Tag = Tag::new(b"prep");
pub const SILF: Tag = Tag::new(b"Silf");
pub const SILL: Tag = Tag::new(b"Sill");
pub const VDMX: Tag = Tag::new(b"VDMX");
pub const MVAR: Tag = Tag::new(b"MVAR");
// This trait is implemented for all font top-level tables
pub trait Subset {
    /// Subset this table, if successful a subset version of this table will be added to builder
    fn subset(
        &self,
        _plan: &Plan,
        _font: &FontRef,
        _s: &mut Serializer,
        _builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        Ok(())
    }

    /// Subset this table with a mutable Subsetstate
    /// This is needed when some tables have dependencies on other table's subset output
    fn subset_with_state(
        &self,
        _plan: &Plan,
        _font: &FontRef,
        _state: &mut SubsetState,
        _s: &mut Serializer,
        _builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        Ok(())
    }
}

// A helper trait providing a 'subset' method for various subtables that have no associated tag
pub(crate) trait SubsetTable<'a> {
    type ArgsForSubset: 'a;
    type Output: 'a;
    /// Subset this table and write a subset version of this table into serializer
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags>;
}

// A helper trait providing a 'serialize' method
trait Serialize<'a> {
    type Args: 'a;
    /// Serialize this table
    fn serialize(s: &mut Serializer, args: Self::Args) -> Result<(), SerializeErrorFlags>;
}

pub fn subset_font(font: &FontRef, plan: &Plan) -> Result<Vec<u8>, SubsetError> {
    let mut builder = FontBuilder::default();

    let mut state = SubsetState::default();
    let mut tags_with_dependencies = Vec::with_capacity(5);
    for record in font.table_directory().table_records() {
        let tag = record.tag();
        if should_drop_table(tag, plan) {
            continue;
        }

        // TODO: add more tags with dependencies for instancing
        match tag {
            Gpos::TAG => tags_with_dependencies.push((tag, record.length())),
            _ => subset(tag, font, plan, &mut builder, record.length(), &mut state)?,
        }
    }

    for (tag, table_len) in tags_with_dependencies {
        subset(tag, font, plan, &mut builder, table_len, &mut state)?;
    }
    Ok(builder.build())
}

fn should_drop_table(tag: Tag, plan: &Plan) -> bool {
    if plan.drop_tables.contains(tag) {
        return true;
    }

    let no_hinting = plan
        .subset_flags
        .contains(SubsetFlags::SUBSET_FLAGS_NO_HINTING);

    match tag {
        // hint tables
        Cvar::TAG | CVT | FPGM | PREP | Hdmx::TAG | VDMX => no_hinting,
        //TODO: drop var tables during instancing when all axes are pinned
        _ => false,
    }
}

fn subset<'a>(
    table_tag: Tag,
    font: &FontRef<'a>,
    plan: &Plan,
    builder: &mut FontBuilder<'a>,
    table_len: u32,
    state: &mut SubsetState,
) -> Result<(), SubsetError> {
    let buf_size = estimate_subset_table_size(font, table_tag, plan);
    let mut s = Serializer::new(buf_size);
    let needed = try_subset(table_tag, font, plan, builder, &mut s, table_len, state);
    if s.in_error() && !s.only_offset_overflow() {
        return Err(SubsetError::SubsetTableError(table_tag));
    }

    // table subsetted to empty
    if needed.is_err() {
        return Ok(());
    }

    //TODO: complete overflow resolution
    let subsetted_data = if !s.offset_overflow() {
        s.copy_bytes()
    } else {
        resolve_overflows(&s, table_tag, 32)
            .map_err(|_| SubsetError::SubsetTableError(table_tag))?
    };

    if !subsetted_data.is_empty() {
        builder.add_raw(table_tag, subsetted_data);
    }
    Ok(())
}

fn try_subset<'a>(
    table_tag: Tag,
    font: &FontRef<'a>,
    plan: &Plan,
    builder: &mut FontBuilder<'a>,
    s: &mut Serializer,
    table_len: u32,
    state: &mut SubsetState,
) -> Result<(), SubsetError> {
    s.start_serialize()
        .map_err(|_| SubsetError::SubsetTableError(table_tag))?;

    let ret = subset_table(table_tag, font, plan, builder, s, state);
    if !s.ran_out_of_room() {
        s.end_serialize();
        return ret;
    }

    // ran out of room, reallocate more bytes
    let buf_size = s.allocated() * 2 + 16;
    if buf_size > (table_len as usize) * 256 {
        return ret;
    }
    s.reset_size(buf_size);
    try_subset(table_tag, font, plan, builder, s, table_len, state)
}

fn subset_table<'a>(
    tag: Tag,
    font: &FontRef<'a>,
    plan: &Plan,
    builder: &mut FontBuilder<'a>,
    s: &mut Serializer,
    state: &mut SubsetState,
) -> Result<(), SubsetError> {
    if plan.no_subset_tables.contains(tag) {
        return passthrough_table(tag, font, s);
    }

    match tag {
        Base::TAG => font
            .base()
            .map_err(|_| SubsetError::SubsetTableError(Base::TAG))?
            .subset(plan, font, s, builder),

        //Skip, handled by Cblc
        Cbdt::TAG => Ok(()),

        Cblc::TAG => font
            .cblc()
            .map_err(|_| SubsetError::SubsetTableError(Cblc::TAG))?
            .subset(plan, font, s, builder),

        Cmap::TAG => font
            .cmap()
            .map_err(|_| SubsetError::SubsetTableError(Cmap::TAG))?
            .subset(plan, font, s, builder),

        Colr::TAG => font
            .colr()
            .map_err(|_| SubsetError::SubsetTableError(Colr::TAG))?
            .subset(plan, font, s, builder),

        //TODO: if SVG is present and we support subsetting SVG table, pass through CPAL table
        // see fonttools: <https://github.com/fonttools/fonttools/blob/64e5277d040e1a5c84f21f8fb8a5dc7d8ad3c3fa/Lib/fontTools/subset/__init__.py#L2545>
        Cpal::TAG => font
            .cpal()
            .map_err(|_| SubsetError::SubsetTableError(Cpal::TAG))?
            .subset(plan, font, s, builder),

        Gdef::TAG => font
            .gdef()
            .map_err(|_| SubsetError::SubsetTableError(Gdef::TAG))?
            .subset_with_state(plan, font, state, s, builder),

        Glyf::TAG => font
            .glyf()
            .map_err(|_| SubsetError::SubsetTableError(Glyf::TAG))?
            .subset(plan, font, s, builder),

        Gpos::TAG => font
            .gpos()
            .map_err(|_| SubsetError::SubsetTableError(Gpos::TAG))?
            .subset_with_state(plan, font, state, s, builder),

        Gsub::TAG => font
            .gsub()
            .map_err(|_| SubsetError::SubsetTableError(Gsub::TAG))?
            .subset_with_state(plan, font, state, s, builder),

        Gvar::TAG => font
            .gvar()
            .map_err(|_| SubsetError::SubsetTableError(Gvar::TAG))?
            .subset(plan, font, s, builder),

        Hdmx::TAG => font
            .hdmx()
            .map_err(|_| SubsetError::SubsetTableError(Hdmx::TAG))?
            .subset(plan, font, s, builder),

        //handled by glyf table if exists
        Head::TAG => font.glyf().map(|_| ()).or_else(|_| {
            font.head()
                .map_err(|_| SubsetError::SubsetTableError(Head::TAG))?
                .subset(plan, font, s, builder)
        }),

        //Skip, handled by Hmtx
        Hhea::TAG => Ok(()),

        Hmtx::TAG => font
            .hmtx()
            .map_err(|_| SubsetError::SubsetTableError(Hmtx::TAG))?
            .subset(plan, font, s, builder),

        //Skip, handled by Vmtx
        Vhea::TAG => Ok(()),

        Vmtx::TAG => font
            .vmtx()
            .map_err(|_| SubsetError::SubsetTableError(Vmtx::TAG))?
            .subset(plan, font, s, builder),

        Hvar::TAG => font
            .hvar()
            .map_err(|_| SubsetError::SubsetTableError(Hvar::TAG))?
            .subset(plan, font, s, builder),

        Vvar::TAG => font
            .vvar()
            .map_err(|_| SubsetError::SubsetTableError(Vvar::TAG))?
            .subset(plan, font, s, builder),

        Fvar::TAG | Cvar::TAG | Avar::TAG => passthrough_table(tag, font, s),
        // MVAR/CVT: not supported by read-fonts, but we want to pass through it during subsetting
        MVAR | CVT => passthrough_table(tag, font, s),

        //Skip, handled by glyf
        Loca::TAG => Ok(()),

        Maxp::TAG => font
            .maxp()
            .map_err(|_| SubsetError::SubsetTableError(Maxp::TAG))?
            .subset(plan, font, s, builder),

        Name::TAG => font
            .name()
            .map_err(|_| SubsetError::SubsetTableError(Name::TAG))?
            .subset(plan, font, s, builder),

        Os2::TAG => font
            .os2()
            .map_err(|_| SubsetError::SubsetTableError(Os2::TAG))?
            .subset(plan, font, s, builder),

        Post::TAG => font
            .post()
            .map_err(|_| SubsetError::SubsetTableError(Post::TAG))?
            .subset(plan, font, s, builder),

        Sbix::TAG => font
            .sbix()
            .map_err(|_| SubsetError::SubsetTableError(Sbix::TAG))?
            .subset(plan, font, s, builder),

        Vorg::TAG => font
            .vorg()
            .map_err(|_| SubsetError::SubsetTableError(Vorg::TAG))?
            .subset(plan, font, s, builder),

        Stat::TAG => passthrough_table(tag, font, s),

        _ if plan
            .subset_flags
            .contains(SubsetFlags::SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED) =>
        {
            passthrough_table(tag, font, s)
        }
        _ => Ok(()),
    }
}

fn passthrough_table(tag: Tag, font: &FontRef<'_>, s: &mut Serializer) -> Result<(), SubsetError> {
    if let Some(data) = font.data_for_tag(tag) {
        s.embed_bytes(data.as_bytes())
            .map_err(|_| SubsetError::SubsetTableError(tag))?;
    }
    Ok(())
}

pub fn estimate_subset_table_size(font: &FontRef, table_tag: Tag, plan: &Plan) -> usize {
    let Some(table_data) = font.data_for_tag(table_tag) else {
        return 0;
    };

    let table_len = table_data.len();
    let mut bulk: usize = 8192;
    let src_glyphs = plan.font_num_glyphs;
    let dst_glyphs = plan.num_output_glyphs;

    // ported from HB: Tables that we want to allocate same space as the source table.
    // For GSUB/GPOS it's because those are expensive to subset, so giving them more room is fine.
    let same_size: bool =
        table_tag == Gsub::TAG || table_tag == Gpos::TAG || table_tag == Name::TAG;

    if plan
        .subset_flags
        .contains(SubsetFlags::SUBSET_FLAGS_RETAIN_GIDS)
    {
        if table_tag == Cff::TAG {
            //Add some extra room for the CFF charset
            bulk += src_glyphs * 16;
        } else if table_tag == Cff2::TAG {
            // Just extra CharString offsets
            bulk += src_glyphs * 4;
        }
    }

    if src_glyphs == 0 || same_size {
        return bulk + table_len;
    }

    bulk + (table_len as f32 * (dst_glyphs as f32 / src_glyphs as f32).sqrt()) as usize
}

#[cfg(test)]
mod test {
    use super::*;
    #[test]
    fn populate_unicodes_wo_input_gid() {
        let mut plan = Plan::default();
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        plan.font_num_glyphs = get_font_num_glyphs(&font);

        let input_gids = IntSet::empty();
        let mut input_unicodes = IntSet::empty();
        input_unicodes.insert(0x2c_u32);
        input_unicodes.insert(0x31_u32);

        plan.populate_unicodes_to_retain(&input_gids, &input_unicodes, &font);

        assert_eq!(plan.unicodes.len(), 2);
        assert!(plan.unicodes.contains(0x2c_u32));
        assert!(plan.unicodes.contains(0x31_u32));

        assert_eq!(plan.glyphset_gsub.len(), 2);
        assert!(plan.glyphset_gsub.contains(GlyphId::new(2)));
        assert!(plan.glyphset_gsub.contains(GlyphId::new(4)));

        assert_eq!(plan.unicode_to_new_gid_list.len(), 2);
        assert_eq!(plan.unicode_to_new_gid_list[0], (0x2c_u32, GlyphId::new(2)));
        assert_eq!(plan.unicode_to_new_gid_list[1], (0x31_u32, GlyphId::new(4)));

        assert_eq!(plan.codepoint_to_glyph.len(), 2);
        assert_eq!(
            plan.codepoint_to_glyph.get(&0x2c_u32),
            Some(GlyphId::new(2)).as_ref()
        );
        assert_eq!(
            plan.codepoint_to_glyph.get(&0x31_u32),
            Some(GlyphId::new(4)).as_ref()
        );
    }

    #[test]
    fn populate_unicodes_w_input_gid() {
        let mut plan = Plan::default();
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        plan.font_num_glyphs = get_font_num_glyphs(&font);

        let mut input_gids = IntSet::empty();
        let input_unicodes = IntSet::empty();
        input_gids.insert(GlyphId::new(2));
        input_gids.insert(GlyphId::new(4));

        plan.populate_unicodes_to_retain(&input_gids, &input_unicodes, &font);
        assert_eq!(plan.unicodes.len(), 2);
        assert!(plan.unicodes.contains(0x2c_u32));
        assert!(plan.unicodes.contains(0x31_u32));

        assert_eq!(plan.glyphset_gsub.len(), 2);
        assert!(plan.glyphset_gsub.contains(GlyphId::new(2)));
        assert!(plan.glyphset_gsub.contains(GlyphId::new(4)));

        assert_eq!(plan.unicode_to_new_gid_list.len(), 2);
        assert_eq!(plan.unicode_to_new_gid_list[0], (0x2c_u32, GlyphId::new(2)));
        assert_eq!(plan.unicode_to_new_gid_list[1], (0x31_u32, GlyphId::new(4)));

        assert_eq!(plan.codepoint_to_glyph.len(), 2);
        assert_eq!(
            plan.codepoint_to_glyph.get(&0x2c_u32),
            Some(GlyphId::new(2)).as_ref()
        );
        assert_eq!(
            plan.codepoint_to_glyph.get(&0x31_u32),
            Some(GlyphId::new(4)).as_ref()
        );
    }

    #[test]
    fn glyf_closure_composite_glyphs() {
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        let loca = font.loca(None).unwrap();
        let glyf = font.glyf().unwrap();
        let mut gids = IntSet::empty();

        glyf_closure_glyphs(&loca, &glyf, GlyphId::new(5), &mut gids, 64, 0);
        assert_eq!(gids.len(), 2);
        assert!(gids.contains(GlyphId::new(5)));
        assert!(gids.contains(GlyphId::new(1)));
    }

    #[test]
    fn populate_gids_wo_cmap_colr_layout() {
        let mut plan = Plan::default();
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        plan.font_num_glyphs = get_font_num_glyphs(&font);
        plan.unicodes.insert(0x2c_u32);
        plan.unicodes.insert(0x34_u32);

        plan.glyphset_gsub.insert(GlyphId::new(2));
        plan.glyphset_gsub.insert(GlyphId::new(7));

        plan.populate_gids_to_retain(&font);
        assert_eq!(plan.glyphset_gsub.len(), 3);
        assert!(plan.glyphset_gsub.contains(GlyphId::new(0)));
        assert!(plan.glyphset_gsub.contains(GlyphId::new(2)));
        assert!(plan.glyphset_gsub.contains(GlyphId::new(7)));

        assert_eq!(plan.glyphset.len(), 5);
        assert!(plan.glyphset.contains(GlyphId::new(0)));
        assert!(plan.glyphset.contains(GlyphId::new(1)));
        assert!(plan.glyphset.contains(GlyphId::new(2)));
        assert!(plan.glyphset.contains(GlyphId::new(4)));
        assert!(plan.glyphset.contains(GlyphId::new(7)));
    }
}
