pub mod layout;
pub mod layout_common;
pub mod layout_kerx_table;
pub mod layout_morx_table;
pub mod layout_trak_table;
pub mod map;

use crate::hb::aat::layout_common::SafeToBreakAccel;
use crate::hb::aat::layout_kerx_table::KerxSubtableCache;
use crate::hb::aat::layout_morx_table::{MorxSubtableCache, MorxSubtableDescriptor};
use crate::hb::kerning::KernSubtableCache;
use crate::hb::ot::OtCache;
use crate::hb::tables::TableRanges;
use alloc::vec::Vec;
use read_fonts::{
    tables::{ankr::Ankr, feat::Feat, kern::Kern, kerx::Kerx, morx::Morx, trak::Trak},
    FontRef, TableProvider,
};

#[derive(Default)]
pub struct AatCache {
    safe_to_break: SafeToBreakAccel,
    pub morx: Vec<MorxSubtableCache>,
    pub morx_descriptors: Vec<MorxSubtableDescriptor>,
    pub kern: Vec<KernSubtableCache>,
    pub kerx: Vec<KerxSubtableCache>,
    has_morx: bool,
    has_morx_from_tables: bool,
    has_ankr: bool,
    has_kern: bool,
    has_kerx: bool,
    pub(crate) has_trak: bool,
    has_feat: bool,
}

impl AatCache {
    #[allow(unused)]
    pub fn new<'a>(font: &impl TableProvider<'a>, ot_cache: &OtCache) -> Self {
        let mut cache = Self::default();
        let num_glyphs = font
            .maxp()
            .map(|maxp| maxp.num_glyphs() as u32)
            .unwrap_or_default();
        let morx = font.morx().ok();
        let kern = font.kern().ok();
        let kerx = font.kerx().ok();
        let morx_len = morx
            .as_ref()
            .map_or(0, |table| table.offset_data().len() as u32);
        cache.has_morx =
            morx.is_some() && !is_morx_blocklisted(morx_len, ot_cache.gsub_len, ot_cache.gdef_len);
        let active_gdef_len = if ot_cache.has_gdef {
            ot_cache.gdef_len
        } else {
            0
        };
        cache.has_morx_from_tables =
            morx.is_some() && !is_morx_blocklisted(morx_len, ot_cache.gsub_len, active_gdef_len);
        cache.has_ankr = font.ankr().is_ok();
        cache.has_kern = kern.is_some();
        cache.has_kerx = kerx.is_some();
        cache.has_trak = font.trak().is_ok();
        cache.has_feat = font.feat().is_ok();

        if let Some(morx) = morx.filter(|_| cache.has_morx || cache.has_morx_from_tables) {
            let morx_base = morx.offset_data().as_bytes().as_ptr() as usize;
            for (chain_index, chain) in morx.chains().iter().enumerate() {
                let Ok(chain) = chain else {
                    continue;
                };
                for subtable in chain.subtables().iter() {
                    let Ok(subtable) = subtable else {
                        continue;
                    };
                    let entry =
                        MorxSubtableCache::new(&subtable, num_glyphs, &mut cache.safe_to_break);
                    cache.morx_descriptors.push(MorxSubtableCache::descriptor(
                        chain_index,
                        &subtable,
                        morx_base,
                    ));
                    cache.morx.push(entry);
                }
            }
        }
        if let Some(kern) = kern {
            for subtable in kern.subtables() {
                let Ok(subtable) = subtable else {
                    continue;
                };
                let entry = KernSubtableCache::new(&subtable, num_glyphs, &mut cache.safe_to_break);
                cache.kern.push(entry);
            }
        }
        if let Some(kerx) = kerx {
            for subtable in kerx.subtables().iter() {
                let Ok(subtable) = subtable else {
                    continue;
                };
                let entry = KerxSubtableCache::new(&subtable, num_glyphs, &mut cache.safe_to_break);
                cache.kerx.push(entry);
            }
        }
        cache
    }
}

#[derive(Clone, Default)]
pub struct AatTables<'a> {
    pub(crate) safe_to_break: Option<&'a SafeToBreakAccel>,
    pub morx: Option<(
        Morx<'a>,
        &'a [MorxSubtableCache],
        &'a [MorxSubtableDescriptor],
    )>,
    pub ankr: Option<Ankr<'a>>,
    pub kern: Option<(Kern<'a>, &'a [KernSubtableCache])>,
    pub kerx: Option<(Kerx<'a>, &'a [KerxSubtableCache])>,
    pub trak: Option<Trak<'a>>,
    pub feat: Option<Feat<'a>>,
}

use crate::algs::HB_CODEPOINT_ENCODE3 as encode3;

/// Blocklist specific broken morx tables identified by the combination of
/// morx, GSUB, and GDEF table lengths.
fn is_morx_blocklisted(morx_len: u32, gsub_len: u32, gdef_len: u32) -> bool {
    const BLOCKLIST: &[u64] = &[
        // AALMAGHRIBI.ttf — https://github.com/harfbuzz/harfbuzz/issues/4108
        encode3(19892, 2794, 340),
    ];
    let key = encode3(morx_len, gsub_len, gdef_len);
    BLOCKLIST.contains(&key)
}

impl<'a> AatTables<'a> {
    pub fn new(font: &FontRef<'a>, cache: &'a AatCache, table_ranges: &TableRanges) -> Self {
        let morx = if cache.has_morx {
            table_ranges.morx.resolve_table(font).map(|table| {
                (
                    table,
                    cache.morx.as_slice(),
                    cache.morx_descriptors.as_slice(),
                )
            })
        } else {
            None
        };
        let ankr = cache
            .has_ankr
            .then(|| table_ranges.ankr.resolve_table(font))
            .flatten();
        let kern = cache
            .has_kern
            .then(|| table_ranges.kern.resolve_table(font))
            .flatten()
            .map(|table| (table, cache.kern.as_slice()));
        let kerx = cache
            .has_kerx
            .then(|| table_ranges.kerx.resolve_table(font))
            .flatten()
            .map(|table| (table, cache.kerx.as_slice()));
        let trak = cache
            .has_trak
            .then(|| table_ranges.trak.resolve_table(font))
            .flatten();
        let feat = cache
            .has_feat
            .then(|| table_ranges.feat.resolve_table(font))
            .flatten();
        Self {
            safe_to_break: Some(&cache.safe_to_break),
            morx,
            ankr,
            kern,
            kerx,
            trak,
            feat,
        }
    }

    pub fn from_tables(font: &impl TableProvider<'a>, cache: &'a AatCache) -> Self {
        let morx = cache
            .has_morx_from_tables
            .then(|| font.morx().ok())
            .flatten()
            .map(|morx| {
                (
                    morx,
                    cache.morx.as_slice(),
                    cache.morx_descriptors.as_slice(),
                )
            });
        let ankr = cache.has_ankr.then(|| font.ankr().ok()).flatten();
        let kern = cache
            .has_kern
            .then(|| font.kern().ok())
            .flatten()
            .map(|table| (table, cache.kern.as_slice()));
        let kerx = cache
            .has_kerx
            .then(|| font.kerx().ok())
            .flatten()
            .map(|table| (table, cache.kerx.as_slice()));
        let trak = cache.has_trak.then(|| font.trak().ok()).flatten();
        let feat = cache.has_feat.then(|| font.feat().ok()).flatten();
        Self {
            safe_to_break: Some(&cache.safe_to_break),
            morx,
            ankr,
            kern,
            kerx,
            trak,
            feat,
        }
    }
}
