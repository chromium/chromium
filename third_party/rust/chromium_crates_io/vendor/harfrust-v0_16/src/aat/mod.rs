pub mod common;
pub mod kern;
pub mod kerx;
pub mod layout;
pub mod map;
pub mod morx;
pub mod trak;

use crate::aat::common::SafeToBreakAccel;
use crate::aat::kern::KernSubtableCache;
use crate::aat::kerx::KerxSubtableCache;
use crate::aat::morx::{MorphSubtableCache, MorphSubtableDescriptor};
use crate::budget::Budget;
use crate::once::Once;
use crate::ot::OtCache;
use alloc::boxed::Box;
use alloc::sync::Arc;
use alloc::vec::Vec;
use read_fonts::{
    model::Tables,
    tables::{
        ankr::Ankr, feat::Feat, kern::Kern, kerx::Kerx, ltag::Ltag, mort::Mort, morx::Morx,
        trak::Trak,
    },
    TableProvider,
};
use yoke::{Yoke, Yokeable};

/// Font-wide AAT tables and accelerators initialized on first use.
pub struct AatCache {
    // Keep uninitialized caches small, including in fonts without AAT tables.
    morx: Once<Box<Yoke<MorphTableCache<'static>, Arc<Tables>>>>,
    mort: Once<Box<Yoke<MorphTableCache<'static>, Arc<Tables>>>>,
    kern: Once<Box<Yoke<KernTableCache<'static>, Arc<Tables>>>>,
    kerx: Once<Box<Yoke<KerxTableCache<'static>, Arc<Tables>>>>,
    tables: Option<Yoke<CachedAatTables<'static>, Arc<Tables>>>,
    /// The allowance shared by the accelerators above, as the layout
    /// lookups share theirs.
    budget: Budget,
}

impl AatCache {
    pub fn new(tables: Option<Arc<Tables>>, ot_cache: &OtCache) -> Self {
        let active_gdef_len = if ot_cache.has_gdef {
            ot_cache.gdef_len
        } else {
            0
        };
        let gsub_len = ot_cache.gsub_len;
        let bytes = tables.as_deref().map_or(0, |tables| {
            [
                tables.morx_data(),
                tables.mort_data(),
                tables.kern_data(),
                tables.kerx_data(),
            ]
            .into_iter()
            .flatten()
            .fold(0usize, |bytes, data| bytes.saturating_add(data.len()))
        });
        Self {
            morx: Once::new(),
            mort: Once::new(),
            kern: Once::new(),
            kerx: Once::new(),
            tables: tables.map(|tables| {
                Yoke::attach_to_cart(tables, |tables| {
                    CachedAatTables::new(tables, gsub_len, active_gdef_len)
                })
            }),
            budget: Budget::for_table_bytes(bytes),
        }
    }
}

/// The AAT tables, parsed once and kept with the font tables.
#[derive(Yokeable)]
pub struct CachedAatTables<'a> {
    pub morx: Option<Morx<'a>>,
    pub mort: Option<Mort<'a>>,
    pub ankr: Option<Ankr<'a>>,
    pub kern: Option<Kern<'a>>,
    pub kerx: Option<Kerx<'a>>,
    pub trak: Option<Trak<'a>>,
    pub feat: Option<Feat<'a>>,
    pub ltag: Option<Ltag<'a>>,
    num_glyphs: u32,
}

impl<'a> CachedAatTables<'a> {
    const EMPTY: Self = Self {
        morx: None,
        mort: None,
        ankr: None,
        kern: None,
        kerx: None,
        trak: None,
        feat: None,
        ltag: None,
        num_glyphs: 0,
    };

    fn new(tables: &'a Tables, gsub_len: u32, gdef_len: u32) -> Self {
        let morx = tables.morx().ok().filter(|morx| {
            let morx_len = morx.offset_data().len() as u32;
            !is_morx_blocklisted(morx_len, gsub_len, gdef_len)
                && is_within_subtable_limit(
                    morx.chains()
                        .iter()
                        .flatten()
                        .map(|chain| chain.n_subtables()),
                )
        });
        let mort = tables.mort().ok().filter(|mort| {
            is_within_subtable_limit(
                mort.chains()
                    .iter()
                    .flatten()
                    .map(|chain| u32::from(chain.n_subtables())),
            )
        });
        let kern = tables.kern().ok().filter(|kern| {
            is_within_subtable_limit(core::iter::once(match kern {
                Kern::Ot(kern) => u32::from(kern.n_tables()),
                Kern::Aat(kern) => kern.n_tables(),
            }))
        });
        let kerx = tables
            .kerx()
            .ok()
            .filter(|kerx| is_within_subtable_limit(core::iter::once(kerx.n_tables())));
        Self {
            morx,
            mort,
            ankr: tables.ankr().ok(),
            kern,
            kerx,
            trak: tables.trak().ok(),
            feat: tables.feat().ok(),
            ltag: tables.ltag().ok(),
            num_glyphs: tables
                .selected_maxp()
                .map(|maxp| maxp.num_glyphs())
                .unwrap_or_default(),
        }
    }
}

/// The subtables of a `morx` or `mort` table, parsed, with their
/// accelerators.
#[derive(Default, Yokeable)]
pub(crate) struct MorphTableCache<'a> {
    safe_to_break: SafeToBreakAccel,
    subtables: Vec<MorphSubtableCache<'a>>,
    descriptors: Vec<MorphSubtableDescriptor>,
}

impl<'a> MorphTableCache<'a> {
    fn new_morx(morx: &Morx<'a>, num_glyphs: u32, budget: &Budget) -> Self {
        let mut cache = Self::default();
        for (chain_index, chain) in morx.chains().iter().enumerate() {
            let Ok(chain) = chain else {
                continue;
            };
            for subtable in chain.subtables().iter() {
                let Ok(subtable) = subtable else {
                    continue;
                };
                let entry =
                    MorphSubtableCache::new_morx(&subtable, num_glyphs, &mut cache.safe_to_break);
                // Past the allowance, the remaining subtables stay skipped,
                // as exhausted layout lookups do.
                if budget.alloc::<u8>(entry.retained_bytes()).is_err() {
                    return cache;
                }
                cache
                    .descriptors
                    .push(MorphSubtableCache::descriptor_morx(chain_index, &subtable));
                cache.subtables.push(entry);
            }
        }
        cache
    }

    fn new_mort(mort: &Mort<'a>, num_glyphs: u32, budget: &Budget) -> Self {
        let mut cache = Self::default();
        for (chain_index, chain) in mort.chains().iter().enumerate() {
            let Ok(chain) = chain else {
                continue;
            };
            for subtable in chain.subtables().iter() {
                let Ok(subtable) = subtable else {
                    continue;
                };
                let entry =
                    MorphSubtableCache::new_mort(&subtable, num_glyphs, &mut cache.safe_to_break);
                if budget.alloc::<u8>(entry.retained_bytes()).is_err() {
                    return cache;
                }
                cache
                    .descriptors
                    .push(MorphSubtableCache::descriptor_mort(chain_index, &subtable));
                cache.subtables.push(entry);
            }
        }
        cache
    }
}

/// The subtables of a `kern` table, parsed, with their accelerators.
#[derive(Default, Yokeable)]
pub(crate) struct KernTableCache<'a> {
    safe_to_break: SafeToBreakAccel,
    subtables: Vec<KernSubtableCache<'a>>,
}

impl<'a> KernTableCache<'a> {
    fn new(kern: &Kern<'a>, num_glyphs: u32, budget: &Budget) -> Self {
        let mut cache = Self::default();
        for subtable in kern.subtables() {
            let Ok(subtable) = subtable else {
                continue;
            };
            let entry = KernSubtableCache::new(&subtable, num_glyphs, &mut cache.safe_to_break);
            if budget.alloc::<u8>(entry.retained_bytes()).is_err() {
                break;
            }
            cache.subtables.push(entry);
        }
        cache
    }
}

/// The subtables of a `kerx` table, parsed, with their accelerators.
#[derive(Default, Yokeable)]
pub(crate) struct KerxTableCache<'a> {
    safe_to_break: SafeToBreakAccel,
    subtables: Vec<KerxSubtableCache<'a>>,
}

impl<'a> KerxTableCache<'a> {
    fn new(kerx: &Kerx<'a>, num_glyphs: u32, budget: &Budget) -> Self {
        let mut cache = Self::default();
        for subtable in kerx.subtables().iter() {
            let Ok(subtable) = subtable else {
                continue;
            };
            let entry = KerxSubtableCache::new(&subtable, num_glyphs, &mut cache.safe_to_break);
            if budget.alloc::<u8>(entry.retained_bytes()).is_err() {
                break;
            }
            cache.subtables.push(entry);
        }
        cache
    }
}

/// The AAT tables of a font, and its accelerators.
#[derive(Clone)]
pub struct AatData<'a> {
    cache: Option<&'a AatCache>,
    tables: &'a CachedAatTables<'a>,
}

impl<'a> core::ops::Deref for AatData<'a> {
    type Target = CachedAatTables<'a>;

    fn deref(&self) -> &Self::Target {
        self.tables
    }
}

/// The most subtables a `kern`, `kerx`, `mort` or `morx` table may have, as
/// in HarfBuzz (`HB_AAT_MAX_SUBTABLES`). A table that declares more is
/// ignored. Each applied subtable walks the whole buffer and its accelerator
/// keeps glyph sets, so this bounds both.
const MAX_SUBTABLES: u32 = 1024;

/// Whether the subtable counts of a table's chains (or the table's single
/// subtable count) add up to at most `MAX_SUBTABLES`.
fn is_within_subtable_limit(counts: impl Iterator<Item = u32>) -> bool {
    let mut total = 0u32;
    for count in counts {
        total = total.saturating_add(count);
        if total > MAX_SUBTABLES {
            return false;
        }
    }
    true
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

static EMPTY_AAT_TABLES: CachedAatTables<'static> = CachedAatTables::EMPTY;

pub(crate) static EMPTY_AAT_DATA: AatData<'static> = AatData {
    cache: None,
    tables: &EMPTY_AAT_TABLES,
};

impl<'a> AatData<'a> {
    pub fn new(cache: &'a AatCache) -> Self {
        Self {
            cache: Some(cache),
            tables: cache.tables.as_ref().map_or(&EMPTY_AAT_TABLES, Yoke::get),
        }
    }

    // Access accelerators only when applying the corresponding table, so
    // planning can select GPOS or a preferred AAT table without building them.
    // The accelerators borrow the parsed tables, so each is projected from
    // the yoke that holds them.
    pub(crate) fn morx_cache(&self) -> Option<&'a MorphTableCache<'a>> {
        self.morx.as_ref()?;
        let cache = self.cache?;
        let tables = cache.tables.as_ref()?;
        let yoke = cache.morx.get_or_init(|| {
            Box::new(tables.map_project_cloned(|tables, _| {
                tables
                    .morx
                    .as_ref()
                    .map(|morx| MorphTableCache::new_morx(morx, tables.num_glyphs, &cache.budget))
                    .unwrap_or_default()
            }))
        });
        Some(yoke.get())
    }

    pub(crate) fn mort_cache(&self) -> Option<&'a MorphTableCache<'a>> {
        self.mort.as_ref()?;
        let cache = self.cache?;
        let tables = cache.tables.as_ref()?;
        let yoke = cache.mort.get_or_init(|| {
            Box::new(tables.map_project_cloned(|tables, _| {
                tables
                    .mort
                    .as_ref()
                    .map(|mort| MorphTableCache::new_mort(mort, tables.num_glyphs, &cache.budget))
                    .unwrap_or_default()
            }))
        });
        Some(yoke.get())
    }

    pub(crate) fn kern_cache(&self) -> Option<&'a KernTableCache<'a>> {
        self.kern.as_ref()?;
        let cache = self.cache?;
        let tables = cache.tables.as_ref()?;
        let yoke = cache.kern.get_or_init(|| {
            Box::new(tables.map_project_cloned(|tables, _| {
                tables
                    .kern
                    .as_ref()
                    .map(|kern| KernTableCache::new(kern, tables.num_glyphs, &cache.budget))
                    .unwrap_or_default()
            }))
        });
        Some(yoke.get())
    }

    pub(crate) fn kerx_cache(&self) -> Option<&'a KerxTableCache<'a>> {
        self.kerx.as_ref()?;
        let cache = self.cache?;
        let tables = cache.tables.as_ref()?;
        let yoke = cache.kerx.get_or_init(|| {
            Box::new(tables.map_project_cloned(|tables, _| {
                tables
                    .kerx
                    .as_ref()
                    .map(|kerx| KerxTableCache::new(kerx, tables.num_glyphs, &cache.budget))
                    .unwrap_or_default()
            }))
        });
        Some(yoke.get())
    }
}

#[cfg(all(test, feature = "std"))]
mod tests {
    use super::*;
    use crate::{
        font::Font, shape, Buffer, SerializeFlags, ShapeOptions, ShapePlan, ShaperFont, Tag,
    };
    use alloc::sync::Arc;
    use read_fonts::{
        model::{Blob, TableFunction},
        FontData, FontRead, FontRef,
    };

    fn initialized(cache: &AatCache) -> [bool; 4] {
        [
            cache.morx.get().is_some(),
            cache.mort.get().is_some(),
            cache.kern.get().is_some(),
            cache.kerx.get().is_some(),
        ]
    }

    fn shape_text(shaper: &ShaperFont<'_, '_>, text: &str, plan: &ShapePlan) -> Buffer {
        let mut buffer = Buffer::new();
        buffer.push_str(text);
        buffer.guess_segment_properties();
        shape(shaper, &mut buffer, ShapeOptions::new().plan(Some(plan))).unwrap();
        buffer
    }

    fn plan_for_text(font: &Font, text: &str) -> ShapePlan {
        let mut buffer = Buffer::new();
        buffer.push_str(text);
        buffer.guess_segment_properties();
        ShapePlan::new(font, buffer.direction(), buffer.script(), None, &[])
    }

    #[test]
    fn gpos_kerning_leaves_legacy_kern_cache_uninitialized() {
        let font = Font::new(
            include_bytes!("../../tests/fonts/rb_custom/PT_Sans-Caption-Web-Regular.ttf").to_vec(),
            0,
        )
        .unwrap();
        let shaper = ShaperFont::new(&font);
        let aat = shaper.layout().aat;
        let cache = aat.cache.unwrap();
        assert!(aat.kern.is_some());
        assert_eq!(initialized(cache), [false; 4]);

        let plan = plan_for_text(&font, "AV");
        assert!(plan.apply_gpos);
        assert!(!plan.apply_kern);
        assert_eq!(initialized(cache), [false; 4]);
        let _ = shape_text(&shaper, "AV", &plan);
        assert_eq!(initialized(cache), [false; 4]);
    }

    #[test]
    fn aat_caches_initialize_on_first_shape_and_are_shared() {
        let cases: &[(&[u8], &str, usize)] = &[
            (
                include_bytes!("../../tests/fonts/in-house/MORXTwentyeight.ttf"),
                "AxEyDyy",
                0,
            ),
            (
                include_bytes!("../../tests/fonts/text-rendering-tests/TestAATMort.ttf"),
                "ABCEFGX",
                1,
            ),
            (
                include_bytes!("../../tests/fonts/text-rendering-tests/TestKERNOne.otf"),
                "uTuT",
                2,
            ),
        ];
        for &(bytes, text, cache_index) in cases {
            let font = Font::new(bytes.to_vec(), 0).unwrap();
            let shaper = ShaperFont::new(&font);
            let aat = shaper.layout().aat;
            let cache = aat.cache.unwrap();
            assert_eq!(initialized(cache), [false; 4]);
            let plan = plan_for_text(&font, text);
            assert_eq!(initialized(cache), [false; 4]);

            let first = shape_text(&shaper, text, &plan);
            let mut expected = [false; 4];
            expected[cache_index] = true;
            assert_eq!(initialized(cache), expected);

            let instance = font.instance_builder().build();
            let second_shaper = ShaperFont::new(&instance);
            assert!(core::ptr::eq(
                cache,
                second_shaper.layout().aat.cache.unwrap()
            ));
            let second = shape_text(&second_shaper, text, &plan);
            assert_eq!(
                first.serialize(Some(&shaper), SerializeFlags::GLYPH_FLAGS),
                second.serialize(Some(&second_shaper), SerializeFlags::GLYPH_FLAGS)
            );
            assert_eq!(initialized(cache), expected);
        }
    }

    #[test]
    fn kerx_cache_initializes_without_legacy_kern_cache() {
        let bytes = include_bytes!("../../tests/fonts/text-rendering-tests/TestKERNOne.otf");
        let source = FontRef::new(bytes).unwrap();
        let source_font = Font::new(bytes.to_vec(), 0).unwrap();
        let charmap = source_font.charmap();
        let left = charmap.map_unicode('u' as u32).unwrap().to_u32() as u16;
        let right = charmap.map_unicode('T' as u32).unwrap().to_u32() as u16;
        // A format-0 kerx subtable with one horizontal pair adjustment of -100.
        #[rustfmt::skip]
        let mut kerx = vec![
            0, 2, 0, 0, // version 2, padding
            0, 0, 0, 1, // nTables
            0, 0, 0, 34, // subtable length
            0, 0, 0, 0, // coverage: horizontal, format 0
            0, 0, 0, 0, // tupleCount
            0, 0, 0, 1, // nPairs
            0, 0, 0, 6, // searchRange
            0, 0, 0, 0, // entrySelector
            0, 0, 0, 0, // rangeShift
        ];
        kerx.extend_from_slice(&left.to_be_bytes());
        kerx.extend_from_slice(&right.to_be_bytes());
        kerx.extend_from_slice(&(-100i16).to_be_bytes());
        let tables = TableFunction::new(Arc::new(move |tag| {
            if tag == Tag::new(b"kerx") {
                Some(Blob::from(kerx.clone()))
            } else {
                source
                    .data_for_tag(tag)
                    .map(|data| Blob::from(data.as_bytes().to_vec()))
            }
        }));
        let font = Font::new(tables, 0).unwrap();
        let shaper = ShaperFont::new(&font);
        let aat = shaper.layout().aat;
        let cache = aat.cache.unwrap();
        assert!(aat.kern.is_some());
        assert!(aat.kerx.is_some());
        let plan = plan_for_text(&font, "uT");
        assert!(plan.apply_kerx);
        assert!(!plan.apply_kern);
        assert_eq!(initialized(cache), [false; 4]);

        let buffer = shape_text(&shaper, "uT", &plan);
        assert_eq!(initialized(cache), [false, false, false, true]);
        let positions = buffer.glyph_positions();
        assert_eq!(
            positions[0].x_advance,
            shaper.default_glyph_h_advance(left.into()) - 50
        );
        assert_eq!(
            positions[1].x_advance,
            shaper.default_glyph_h_advance(right.into()) - 50
        );
        assert_eq!(positions[1].x_offset, -50);
    }

    /// A morx table with one insertion subtable that inserts `count` glyphs
    /// of `list`, from `start`, after each of `glyphs`.
    fn insertion_morx(glyphs: [u16; 2], list: &[u16], start: u16, count: u16) -> Vec<u8> {
        let first = glyphs[0].min(glyphs[1]);
        let last = glyphs[0].max(glyphs[1]);
        // A format 8 lookup giving `glyphs` class 4, padded to four bytes.
        let mut classes = vec![8, first, last - first + 1];
        classes.extend((first..=last).map(|gid| if glyphs.contains(&gid) { 4 } else { 1 }));
        classes.resize(classes.len().next_multiple_of(2), 0);
        let states = [0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
        let entries = [0, 0, 0xFFFF, 0xFFFF, 0, count << 5, start, 0xFFFF];
        let class_table = 20;
        let state_array = class_table + 2 * classes.len() as u32;
        let entry_table = state_array + 2 * states.len() as u32;
        let insertion_action = entry_table + 2 * entries.len() as u32;
        let mut body = vec![];
        for value in [5, class_table, state_array, entry_table, insertion_action] {
            body.extend(value.to_be_bytes());
        }
        for value in classes.iter().chain(&states).chain(&entries).chain(list) {
            body.extend(value.to_be_bytes());
        }
        let subtable_len = 12 + body.len() as u32;
        let mut morx = vec![];
        // version 2, one chain of one subtable, an insertion in every direction
        for value in [
            0x0002_0000,
            1,
            1,
            16 + subtable_len,
            0,
            1,
            subtable_len,
            0x2000_0005,
            1,
        ] {
            morx.extend(u32::to_be_bytes(value));
        }
        morx.extend(body);
        morx
    }

    #[test]
    fn morx_insertion_index_past_u16() {
        let bytes = include_bytes!("../../tests/fonts/text-rendering-tests/TestKERNOne.otf");
        let source = FontRef::new(bytes).unwrap();
        let source_font = Font::new(bytes.to_vec(), 0).unwrap();
        let charmap = source_font.charmap();
        let u = charmap.map_unicode('u' as u32).unwrap().to_u32() as u16;
        let t = charmap.map_unicode('T' as u32).unwrap().to_u32() as u16;
        let shape_with = |list: &[u16], start: u16, count: u16| -> Vec<u32> {
            let morx = insertion_morx([u, t], list, start, count);
            let source = source.clone();
            let tables = TableFunction::new(Arc::new(move |tag| {
                if tag == Tag::new(b"morx") {
                    Some(Blob::from(morx.clone()))
                } else {
                    source
                        .data_for_tag(tag)
                        .map(|data| Blob::from(data.as_bytes().to_vec()))
                }
            }));
            let font = Font::new(tables, 0).unwrap();
            let shaper = ShaperFont::new(&font);
            let plan = plan_for_text(&font, "uT");
            let buffer = shape_text(&shaper, "uT", &plan);
            buffer
                .glyph_infos()
                .iter()
                .map(|info| info.glyph_id)
                .collect()
        };
        // 65,536 glyphs, the first different from the rest.
        let mut long = vec![u; 0x1_0000];
        long[0] = t;
        let short = [t, u, u, u];
        assert_eq!(shape_with(&short, 2, 2), [u, u, u, t, u, u].map(u32::from));
        assert_eq!(shape_with(&long, 0xFFFE, 2), shape_with(&short, 2, 2));
        // The third glyph's index is past the end of either list.
        assert_eq!(shape_with(&long, 0xFFFE, 3), shape_with(&short, 2, 3));
    }

    #[test]
    fn kerx_format4_leaves_distant_glyphs_unattached() {
        let bytes = include_bytes!("../../tests/fonts/text-rendering-tests/TestKERNOne.otf");
        let source = FontRef::new(bytes).unwrap();
        let source_font = Font::new(bytes.to_vec(), 0).unwrap();
        let charmap = source_font.charmap();
        let base = charmap.map_unicode('u' as u32).unwrap().to_u32() as u16;
        let attached = charmap.map_unicode('T' as u32).unwrap().to_u32() as u16;
        let mut classes = [(base, 4u16), (attached, 5u16)];
        classes.sort_unstable();
        // A format-4 kerx subtable that marks `base` and attaches every
        // following `attached` glyph to it.
        #[rustfmt::skip]
        let mut kerx = vec![
            0, 2, 0, 0, // version 2, padding
            0, 0, 0, 1, // nTables
            0, 0, 0, 102, // subtable length
            0, 0, 0, 4, // coverage: horizontal, format 4
            0, 0, 0, 0, // tupleCount
            0, 0, 0, 6, // nClasses
            0, 0, 0, 20, // classTableOffset
            0, 0, 0, 40, // stateArrayOffset
            0, 0, 0, 64, // entryTableOffset
            0x80, 0, 0, 82, // flags: control point coordinates, action offset
            0, 6, 0, 4, 0, 2, 0, 0, 0, 0, 0, 0, // lookup format 6, two units
        ];
        for (glyph, class) in classes {
            kerx.extend_from_slice(&glyph.to_be_bytes());
            kerx.extend_from_slice(&class.to_be_bytes());
        }
        #[rustfmt::skip]
        kerx.extend_from_slice(&[
            0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, // state 0: class 4 takes entry 1
            0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2, // state 1: class 5 takes entry 2
            0, 0, 0, 0, 0xFF, 0xFF, // entry 0: no action
            0, 1, 0x80, 0, 0xFF, 0xFF, // entry 1: mark, go to state 1
            0, 1, 0, 0, 0, 0, // entry 2: action 0, stay in state 1
            0, 0, 0, 0, 0, 0, 0, 0, // action 0: both points at the origin
        ]);
        let tables = TableFunction::new(Arc::new(move |tag| {
            if tag == Tag::new(b"kerx") {
                Some(Blob::from(kerx.clone()))
            } else {
                source
                    .data_for_tag(tag)
                    .map(|data| Blob::from(data.as_bytes().to_vec()))
            }
        }));
        let font = Font::new(tables, 0).unwrap();
        let shaper = ShaperFont::new(&font);
        let text = format!("u{}", "T".repeat(40000));
        let plan = plan_for_text(&font, &text);
        assert!(plan.apply_kerx);

        let buffer = shape_text(&shaper, &text, &plan);
        let positions = buffer.glyph_positions();
        let base_advance = shaper.default_glyph_h_advance(base.into());
        assert_eq!(positions[1].x_offset, -base_advance);
        assert_ne!(positions[32768].x_offset, 0);
        // 32769 glyphs do not fit the 16-bit attachment chain.
        assert_eq!(positions[32769].x_offset, 0);
    }

    #[test]
    fn concurrent_first_shapes_share_morx_cache() {
        let font = Font::new(
            include_bytes!("../../tests/fonts/in-house/MORXTwentyeight.ttf").to_vec(),
            0,
        )
        .unwrap();
        let shaper = ShaperFont::new(&font);
        let cache = shaper.layout().aat.cache.unwrap();
        let plan = plan_for_text(&font, "AxEyDyy");
        let barrier = std::sync::Barrier::new(4);
        assert_eq!(initialized(cache), [false; 4]);
        std::thread::scope(|scope| {
            let handles: Vec<_> = (0..4)
                .map(|_| {
                    scope.spawn(|| {
                        let shaper = ShaperFont::new(&font);
                        barrier.wait();
                        let buffer = shape_text(&shaper, "AxEyDyy", &plan);
                        (
                            buffer.serialize(Some(&shaper), SerializeFlags::GLYPH_FLAGS),
                            core::ptr::from_ref(shaper.layout().aat.morx_cache().unwrap()) as usize,
                        )
                    })
                })
                .collect();
            let results: Vec<_> = handles
                .into_iter()
                .map(|handle| handle.join().unwrap())
                .collect();
            for result in &results[1..] {
                assert_eq!(results[0].0, result.0);
                assert_eq!(results[0].1, result.1);
            }
        });
        assert_eq!(initialized(cache), [true, false, false, false]);
    }

    #[test]
    fn morx_cache_stops_at_the_preprocessing_allowance() {
        const N_SUBTABLES: usize = 8;
        // A non-contextual subtable whose single format 2 segment maps the
        // whole glyph space to glyph 1: 30 bytes of font, 128 set pages.
        #[rustfmt::skip]
        let subtable = [
            0, 0, 0, 30, // length
            0, 0, 0, 4, // coverage: non-contextual
            0, 0, 0, 1, // subFeatureFlags
            0, 2, 0, 6, 0, 1, // lookup format 2, unitSize, nUnits
            0, 6, 0, 0, 0, 0, // searchRange, entrySelector, rangeShift
            0xFF, 0xFE, 0, 0, 0, 1, // lastGlyph, firstGlyph, value
        ];
        #[rustfmt::skip]
        let mut morx = vec![
            0, 2, 0, 0, // version 2, unused
            0, 0, 0, 1, // nChains
            0, 0, 0, 1, // defaultFlags
        ];
        morx.extend_from_slice(&(16 + 30 * N_SUBTABLES as u32).to_be_bytes()); // chainLength
        morx.extend_from_slice(&[0, 0, 0, 0]); // nFeatureEntries
        morx.extend_from_slice(&(N_SUBTABLES as u32).to_be_bytes()); // nSubtables
        for _ in 0..N_SUBTABLES {
            morx.extend_from_slice(&subtable);
        }
        let morx = Morx::read(FontData::new(&morx)).unwrap();

        let budget = Budget::for_table_bytes(morx.offset_data().len());
        let cache = MorphTableCache::new_morx(&morx, 4, &budget);
        assert_eq!(cache.subtables.len(), N_SUBTABLES);
        let entry_bytes = cache.subtables[0].retained_bytes();
        assert!(entry_bytes > 128 * 80);
        assert!(budget.remaining().1 <= (4 << 20) - N_SUBTABLES * entry_bytes);

        let budget = Budget::new(usize::MAX, 3 * entry_bytes);
        let cache = MorphTableCache::new_morx(&morx, 4, &budget);
        assert_eq!(cache.subtables.len(), 3);
        assert_eq!(cache.descriptors.len(), 3);
        assert_eq!(budget.remaining().1, 0);
    }
}
