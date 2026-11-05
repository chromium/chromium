//! Font representation.

mod blob;
mod cache;
mod format;
mod source;
mod tables;

#[doc(inline)]
pub use blob::Blob;
#[doc(inline)]
pub use format::Format;
#[doc(inline)]
pub use source::Source;
#[doc(inline)]
pub use tables::{TableFunction, Tables};

// Do our best to not expose this to users through docs or rust-analyzer.
#[doc(hidden)]
#[rust_analyzer::completions(hidden_from_completion)]
pub mod interop;

use super::attribute::Attributes;
use super::charmap::{Charmap, EncodingTables, SfntUnicodeCharmap};
use super::glyph::{
    bitmap::BitmapStrikes,
    color::{ColorGlyphs, Palettes},
    outline::{varc, OutlineGlyphs, OutlineSource},
    GlyphSource, GlyphSourceOrder,
};
use super::metrics::{empty_glyph_metrics, GlyphMetrics, Metrics, StyleMetrics};
use super::name::{self, GlyphName, Names};
use super::once::Once;
use super::variation::{Axes, NamedInstances, NormalizedCoord, Variation};
use crate::tables::glyf::outline::OutlineContext;
use crate::tables::loca::LocaGlyph;
use crate::tables::{
    avar::Avar,
    fvar::Fvar,
    glyf::{Glyf, PHANTOM_POINT_COUNT},
    gvar::{GlyphVariationData, SelectedGvar},
    hmtx::GlyphMetricRecords,
    hvar::Hvar,
    layout::SelectedFeatureVariations,
    loca::Loca,
    vorg::Vorg,
    vvar::Vvar,
};
use crate::{
    ps::{
        cff::{CffFontRef, Subfont},
        type1::{MultipleMaster, Type1Font, Type1Instance},
    },
    tables::{
        cbdt::Cbdt, cblc::Cblc, cpal::Cpal, ebdt::Ebdt, eblc::Eblc, fvar::VariationAxisRecord,
        sbix::Sbix,
    },
    TableProvider,
};
use alloc::{boxed::Box, sync::Arc};
pub(crate) use cache::ColrTables;
use cache::{
    CbdtTables, Cff2Font, CffFont, CpalTable, EbdtTables, GlyfLoca, GvarTable, HvarTable,
    MetricsTable, SbixTable, TableCache, VorgTable, VvarTable,
};
use core::{
    any::Any,
    sync::atomic::{self, AtomicBool, AtomicU32},
};
use smallvec::SmallVec;
use types::{F2Dot14, Fixed, GlyphId, Point};
use yoke::Yoke;

/// A font at one location in its design space.
///
/// Cloning is cheap: clones share one instance rather than copying it.
#[derive(Clone)]
pub struct Font(Repr);

#[derive(Clone)]
enum Repr {
    Default(SharedFont),
    Varied(Arc<VariedInstance>),
}

impl Font {
    /// Creates a font from `source`, at its default location.
    ///
    /// `index` selects a font within the data:
    ///
    /// - SFNT: the font in a collection (`ttc` or `otc`). Data holding a
    ///   single font accepts only zero.
    /// - Standalone CFF: the font in the CFF font set.
    /// - Type 1: ignored, since the data holds one font.
    /// - [`TableFunction`]: ignored, since the function supplies the tables
    ///   of one font.
    ///
    /// Returns `None` if the source holds no font in a supported format, or
    /// none at `index`.
    pub fn new(source: impl Into<Source>, index: u32) -> Option<Self> {
        Some(Font(Repr::Default(SharedFont::new(source, index)?)))
    }

    /// Returns the font's variation axes, which are empty if the font is not
    /// variable.
    pub fn axes(&self) -> Axes<'_> {
        Axes::new(self)
    }

    /// Returns the named instances defined by the font.
    pub fn named_instances(&self) -> NamedInstances<'_> {
        NamedInstances::new(self)
    }

    /// Returns a builder for another instance of the same font.
    ///
    /// The two share everything the location does not change, so deriving one
    /// instance from another costs no more than reading the tables once.
    pub fn instance_builder(&self) -> InstanceBuilder {
        InstanceBuilder {
            font: self.shared().clone(),
            coords: CoordStorage::default(),
            type1_blend: SmallVec::new(),
        }
    }

    /// Returns this font at its default location.
    #[inline]
    pub fn default_instance(&self) -> Font {
        Font(Repr::Default(self.shared().clone()))
    }

    /// Returns the normalized variation coordinates for this font instance.
    pub fn normalized_coords(&self) -> &[NormalizedCoord] {
        match &self.0 {
            Repr::Default(_) => &[],
            Repr::Varied(varied) => varied.coords.as_slice(),
        }
    }

    /// Returns the layout feature variations this instance selects.
    pub fn feature_variations(&self) -> SelectedFeatureVariations {
        match self.sfnt_location() {
            Some(location) => location
                .feature_vars
                .load(self.shared(), self.normalized_coords()),
            // Only an SFNT font has layout tables to vary.
            None => SelectedFeatureVariations::default(),
        }
    }

    /// Returns the number of glyphs in the font.
    ///
    /// Fixed for the font: no location varies it.
    #[inline]
    pub fn num_glyphs(&self) -> u32 {
        self.shared().num_glyphs()
    }

    /// Returns the size of the em square, in font units.
    ///
    /// Fixed for the font: no location varies it.
    #[inline]
    pub fn units_per_em(&self) -> u16 {
        self.shared().units_per_em()
    }

    /// Returns the width, style and weight of the default instance.
    ///
    /// For an SFNT font, these come from the OS/2 table, with the `post`
    /// italic angle for an oblique style. If there is no OS/2 table, they
    /// come from `head`. These tables do not vary, so every instance of a
    /// variable font returns the same values. To get the attributes at a
    /// location, use [`Attributes::from_variations`].
    ///
    /// For a Type 1 or standalone CFF font, these come from the `Weight` name
    /// and `ItalicAngle`.
    pub fn default_attributes(&self) -> Attributes {
        Attributes::from_font(self)
    }

    /// Returns `true` if the font needs its TrueType instructions to draw
    /// correct contours.
    ///
    /// Such fonts are often called "tricky". They are recognized by family name
    /// or by the checksums of the `cvt`, `fpgm` and `prep` tables, and only
    /// fonts with `glyf` outlines qualify. The result is computed once and
    /// cached. See [`is_hint_reliant`](crate::tables::glyf::is_hint_reliant).
    pub fn is_hint_reliant(&self) -> bool {
        use core::sync::atomic::Ordering;
        const UNKNOWN: u8 = 0;
        const KNOWN: u8 = 1;
        // Only an SFNT font can have TrueType outlines.
        let Some(sfnt) = self.shared().sfnt() else {
            return false;
        };
        let cell = &sfnt.hint_reliant;
        match cell.load(Ordering::Relaxed) {
            UNKNOWN => {
                let answer = crate::tables::glyf::is_hint_reliant(&self.tables());
                cell.store(KNOWN + answer as u8, Ordering::Relaxed);
                answer
            }
            state => state > KNOWN,
        }
    }

    /// Returns the font's names and other localized strings.
    pub fn names(&self) -> Names<'_> {
        match self.kind() {
            Kind::Type1(font) => Names::from_type1(font),
            Kind::Cff(ref font, _) => Names::from_cff(font),
            _ => Names::from_sfnt(&self.tables()),
        }
    }

    /// Returns the name of a glyph, synthesizing `gidNNN` if none is stored.
    ///
    /// Returns `None` for a glyph outside the font.
    pub fn glyph_name(&self, glyph: GlyphId) -> Option<GlyphName> {
        name::glyph_name(self, glyph)
    }

    /// Returns the identifier and name of every glyph in the font.
    ///
    /// Names come from `post`, then `CFF`, or from Type 1 charstrings. A
    /// missing name is synthesized as `gidNNN`.
    pub fn glyph_names(&self) -> impl Iterator<Item = (GlyphId, GlyphName)> + '_ {
        name::glyph_names(self)
    }

    /// Returns the glyph named `name`.
    ///
    /// Names come from the same sources as [`glyph_names`](Self::glyph_names),
    /// but a synthesized `gidNNN` name matches nothing. Where several glyphs
    /// share a name, the first is returned.
    ///
    /// The first call sorts the font's names, and later calls search them.
    pub fn glyph_by_name(&self, name: &str) -> Option<GlyphId> {
        self.shared()
            .0
            .glyph_name_index
            .get_or_init(|| name::GlyphNameIndex::new(self))
            .get(name)
    }

    /// Returns `true` if the font is a standalone CID-keyed CFF font, whose
    /// glyphs are identified by CID.
    ///
    /// A `CFF` table in an SFNT font may be CID-keyed too, but its glyphs
    /// are identified by glyph index, as in any SFNT font.
    pub fn is_cid_keyed(&self) -> bool {
        self.shared()
            .standalone_cff()
            .is_some_and(|cff| cff.is_cid())
    }

    /// Returns the largest CID that a CID-keyed font maps to a glyph.
    ///
    /// Glyphs in a CID-keyed font can be addressed by CID, so one more than
    /// this value is the number of addressable glyph IDs. Returns `None` if the
    /// font is not CID-keyed.
    pub fn max_cid(&self) -> Option<u32> {
        Some(self.shared().cid_glyphs()?.len() as u32 - 1)
    }

    /// Returns the glyph for `cid` in a CID-keyed font.
    ///
    /// Where several glyphs have the same CID, the first is returned. Returns
    /// `None` if the font is not CID-keyed or no glyph has `cid`.
    ///
    /// The first call builds a table from each CID to its glyph.
    pub fn glyph_by_cid(&self, cid: u32) -> Option<GlyphId> {
        let glyph = *self.shared().cid_glyphs()?.get(cid as usize)?;
        (cid == 0 || glyph != 0).then(|| GlyphId::new(glyph as u32))
    }

    /// Returns the font's character mappings.
    pub fn charmap(&self) -> Charmap<'_> {
        Charmap::new(self)
    }

    /// Returns the metrics describing the font as a whole, at this
    /// instance's location.
    pub fn metrics(&self) -> &Metrics {
        match &self.0 {
            // Nothing varies there, so the font already holds the answer.
            Repr::Default(font) => font.metrics(),
            // A Type 1 font's metrics come from dictionaries that no
            // location changes.
            Repr::Varied(varied) if matches!(varied.kind, VariedKind::Type1(_)) => {
                varied.font.metrics()
            }
            Repr::Varied(varied) => varied.location.metrics.get_or_init(|| {
                debug_assert!(!varied.coords.as_slice().is_empty());
                Metrics::from_sfnt(&varied.font.tables(), varied.coords.as_slice())
            }),
        }
    }

    /// Returns the font's style measurements at this instance's location.
    ///
    /// This reads `post` when requested; [`metrics`](Self::metrics) does not.
    pub fn style_metrics(&self) -> &StyleMetrics {
        match &self.0 {
            Repr::Default(font) => font.style_metrics(),
            Repr::Varied(varied) if matches!(varied.kind, VariedKind::Type1(_)) => {
                varied.font.style_metrics()
            }
            Repr::Varied(varied) => varied
                .location
                .style_metrics
                .get_or_init(|| {
                    Box::new(StyleMetrics::from_sfnt(
                        &varied.font.tables(),
                        varied.coords.as_slice(),
                    ))
                })
                .as_ref(),
        }
    }

    /// Returns measurements of individual glyphs, at this instance's
    /// location.
    #[inline]
    pub fn glyph_metrics(&self) -> GlyphMetrics<'_> {
        GlyphMetrics::new(self, self.metrics(), self.normalized_coords())
    }

    /// Returns the font's outlines, using the source order in
    /// [`GlyphSourceOrder::DEFAULT`].
    pub fn outline_glyphs(&self) -> OutlineGlyphs<'_> {
        OutlineGlyphs::new(self, GlyphSourceOrder::DEFAULT)
    }

    /// Returns the font's outlines, using the source order in `order`.
    ///
    /// The first of `Glyf`, `Cff2`, `Cff` and `Type1` that the font has
    /// becomes the base source for every glyph. If `Varc` comes before the
    /// base source, it applies to the glyphs it covers, and their components
    /// are drawn from the base source. Sources that are not outlines are
    /// ignored.
    pub fn outline_glyphs_with(&self, order: GlyphSourceOrder) -> OutlineGlyphs<'_> {
        OutlineGlyphs::new(self, order)
    }

    /// Returns the font's bitmaps, from the first of `sbix`, `CBDT` and
    /// `EBDT` that the font has.
    pub fn bitmap_strikes(&self) -> BitmapStrikes<'_> {
        BitmapStrikes::new(self, GlyphSourceOrder::BITMAPS)
    }

    /// Returns the font's bitmaps, using the source order in `order`.
    ///
    /// The bitmaps come from the first of `Sbix`, `Cbdt` and `Ebdt` in
    /// `order` that the font has. Sources that are not bitmaps are ignored.
    pub fn bitmap_strikes_with(&self, order: GlyphSourceOrder) -> BitmapStrikes<'_> {
        BitmapStrikes::new(self, order)
    }

    /// Returns the font's color glyphs.
    pub fn color_glyphs(&self) -> ColorGlyphs<'_> {
        ColorGlyphs::new(self)
    }

    /// Returns the font's color palettes.
    pub fn palettes(&self) -> Palettes<'_> {
        Palettes::new(self)
    }

    /// Returns the underlying source of font data.
    #[inline]
    pub fn source(&self) -> &Source {
        self.shared().source()
    }

    /// Returns the underlying kind of the font.
    #[inline]
    pub fn kind(&self) -> Kind<'_> {
        self.shared().kind()
    }

    /// Returns an object that provides access to individual font tables.
    ///
    /// For non-SFNT fonts, this will return an empty set of tables.
    #[inline]
    pub fn tables(&self) -> &Tables {
        self.shared().tables()
    }
}

impl Font {
    /// Returns the slot for data that Skrifa derives from the font at this
    /// location.
    ///
    /// At the default location, the slot belongs to the shared font.
    /// Elsewhere, it belongs to the instance.
    pub(super) fn scaling_data(&self) -> &Once<Box<dyn Any + Send + Sync>> {
        &self.location_caches().scaling_data
    }

    /// Returns the caches for this instance's location.
    ///
    /// At the default location, the caches belong to the shared font.
    /// Elsewhere, they belong to the instance.
    fn location_caches(&self) -> &LocationCaches {
        match &self.0 {
            Repr::Default(font) => &font.0.default_location,
            Repr::Varied(varied) => &varied.location,
        }
    }

    /// Returns the SFNT caches for this instance's location, or `None` if
    /// the font is not an SFNT font.
    fn sfnt_location(&self) -> Option<&SfntLocation> {
        match &self.0 {
            Repr::Default(font) => font.sfnt().map(|sfnt| &sfnt.default_location),
            Repr::Varied(varied) => match &varied.kind {
                VariedKind::Sfnt(sfnt) => Some(&sfnt.location),
                VariedKind::Type1(_) => None,
            },
        }
    }

    /// The state every instance of this font shares.
    pub(super) fn shared(&self) -> &SharedFont {
        match &self.0 {
            Repr::Default(font) => font,
            Repr::Varied(varied) => &varied.font,
        }
    }

    /// Returns the axes of a Type 1 multiple master font, or `None` for any
    /// other font.
    pub(crate) fn type1_axes(&self) -> Option<&[VariationAxisRecord]> {
        self.shared().multiple_master()?;
        Some(self.shared().type1_axes())
    }

    /// Returns a Type 1 font drawn at this instance's location.
    pub(crate) fn type1_instance(&self) -> Option<Type1Instance<'_>> {
        let Kind::Type1(font) = self.shared().kind() else {
            return None;
        };
        Some(match &self.0 {
            Repr::Varied(varied) => match &varied.kind {
                VariedKind::Type1(type1) => font.instance(&type1.weights),
                VariedKind::Sfnt(_) => font.default_instance(),
            },
            Repr::Default(_) => font.default_instance(),
        })
    }

    /// Returns the horizontal glyph metrics, from `HMTX` or `hmtx`.
    #[inline]
    pub(crate) fn h_metric_records(&self) -> &GlyphMetricRecords<'_> {
        self.shared().h_metric_records()
    }

    /// Returns the vertical glyph metrics, from `VMTX` or `vmtx`.
    #[inline]
    pub(crate) fn v_metric_records(&self) -> &GlyphMetricRecords<'_> {
        self.shared().v_metric_records()
    }

    /// Returns the outline tables.
    #[inline]
    pub(crate) fn selected_glyf_loca(&self) -> Option<&(Glyf<'_>, Loca<'_>)> {
        self.shared().selected_glyf_loca()
    }

    /// Returns `HVAR`.
    #[inline]
    pub(crate) fn hvar(&self) -> Option<&Hvar<'_>> {
        self.shared().hvar()
    }

    /// Returns `VVAR`.
    #[inline]
    pub(crate) fn vvar(&self) -> Option<&Vvar<'_>> {
        self.shared().vvar()
    }

    /// Returns `GVAR`, or `gvar` without it.
    #[inline]
    pub(crate) fn selected_gvar(&self) -> Option<&SelectedGvar<'_>> {
        self.shared().selected_gvar()
    }

    /// Returns the `gvar` shared tuple scalars at this location, computed
    /// once.
    pub(crate) fn gvar_scalars(&self) -> &[Fixed] {
        self.location_scalars(
            |sfnt| &sfnt.gvar_scalars,
            |varied| match varied.font.selected_gvar() {
                Some(gvar) => scalars_with(gvar.shared_tuple_count() as usize, |out| {
                    gvar.compute_scalars(varied.coords.as_slice(), out)
                }),
                None => Box::new([]),
            },
        )
    }

    /// Returns the `HVAR` region scalars at this location, computed once.
    pub(crate) fn hvar_scalars(&self) -> &[Fixed] {
        self.location_scalars(
            |sfnt| &sfnt.hvar_scalars,
            |varied| {
                let store = varied
                    .font
                    .hvar()
                    .and_then(|hvar| hvar.item_variation_store().ok());
                store_scalars(store.as_ref(), varied.coords.as_slice())
            },
        )
    }

    /// Returns the `VVAR` region scalars at this location, computed once.
    pub(crate) fn vvar_scalars(&self) -> &[Fixed] {
        self.location_scalars(
            |sfnt| &sfnt.vvar_scalars,
            |varied| {
                let store = varied
                    .font
                    .vvar()
                    .and_then(|vvar| vvar.item_variation_store().ok());
                store_scalars(store.as_ref(), varied.coords.as_slice())
            },
        )
    }

    /// Returns the `COLR` region scalars at this location, computed once.
    pub(crate) fn colr_scalars(&self) -> &[Fixed] {
        self.location_scalars(
            |sfnt| &sfnt.colr_scalars,
            |varied| {
                let store = varied
                    .font
                    .colr_tables()
                    .and_then(|tables| tables.var_store.as_ref());
                store_scalars(store, varied.coords.as_slice())
            },
        )
    }

    /// Returns the scalars `cache` selects for this location, filling it
    /// with `compute` the first time. The default location needs none.
    fn location_scalars(
        &self,
        cache: fn(&VariedSfnt) -> &Once<Box<[Fixed]>>,
        compute: impl FnOnce(&VariedInstance) -> Box<[Fixed]>,
    ) -> &[Fixed] {
        match &self.0 {
            Repr::Default(_) => &[],
            Repr::Varied(varied) => match &varied.kind {
                VariedKind::Sfnt(sfnt) => cache(sfnt).get_or_init(|| compute(varied)),
                VariedKind::Type1(_) => &[],
            },
        }
    }

    /// Returns the `CFF` charstring outlines from SFNT tables.
    #[inline]
    pub(crate) fn cff(&self) -> Option<&CffFontRef<'_>> {
        self.shared().cff()
    }

    /// Returns the charstrings for `source`.
    ///
    /// For an SFNT font, these come from the `CFF2` or `CFF` table. A
    /// standalone CFF font provides its own charstrings as `CFF`.
    pub(crate) fn charstrings(&self, source: OutlineSource) -> Option<&CffFontRef<'_>> {
        let shared = self.shared();
        match source {
            OutlineSource::Cff2 => shared.cff2(),
            OutlineSource::Cff => shared.cff().or_else(|| shared.standalone_cff()),
            _ => None,
        }
    }

    /// Returns subfont `index` of the charstrings for `source`, parsed at
    /// this instance's location.
    ///
    /// The first [`CACHED_SUBFONTS`] subfonts are parsed once and kept,
    /// since nearly every glyph in a font shares one of them. A standalone
    /// CFF font parses them when the font is read. An SFNT font parses each
    /// once per location, when a glyph first needs it. A subfont past them
    /// is parsed each time it is asked for, so a font with thousands of
    /// subfonts costs no more memory than one with eight.
    pub(crate) fn cff_subfont(&self, source: OutlineSource, index: u16) -> Option<Subfont> {
        let cff = self.charstrings(source)?;
        if let Some(subfonts) = self.shared().standalone_cff_subfonts() {
            return match subfonts.get(usize::from(index)) {
                Some(subfont) => *subfont,
                None => cff.subfont(index, &[]).ok(),
            };
        }
        let location = self.sfnt_location()?;
        let cache = match source {
            OutlineSource::Cff2 => &location.cff2_subfonts,
            _ => &location.cff_subfonts,
        };
        let coords = self.normalized_coords();
        let subfonts = cache.get_or_init(|| {
            let count = (cff.num_subfonts() as usize).min(CACHED_SUBFONTS);
            (0..count).map(|_| Once::new()).collect()
        });
        match subfonts.get(usize::from(index)) {
            Some(subfont) => *subfont.get_or_init(|| cff.subfont(index, coords).ok()),
            None => cff.subfont(index, coords).ok(),
        }
    }

    /// Returns `true` if the font has glyphs from `source`, one of the
    /// bitmap tables, `COLR` or `VARC`.
    ///
    /// Each source is probed the first time it is asked about and the answer
    /// kept, so asking loads no table that measuring with it would not.
    pub(crate) fn has_layered_source(&self, source: GlyphSource) -> bool {
        use core::sync::atomic::Ordering;
        let Some(sfnt) = self.shared().sfnt() else {
            return false;
        };
        let shift = 2 * (source as u32 - 1);
        match (sfnt.glyph_sources.load(Ordering::Relaxed) >> shift) & 3 {
            SOURCE_ABSENT => return false,
            SOURCE_PRESENT => return true,
            _ => {}
        }
        let present = match source {
            GlyphSource::Sbix | GlyphSource::Cbdt | GlyphSource::Ebdt => !self
                .bitmap_strikes_with(GlyphSourceOrder::new(&[source]))
                .is_empty(),
            GlyphSource::Colr => !self.color_glyphs().is_empty(),
            GlyphSource::Varc => self.varc_outlines().is_some(),
            _ => return false,
        };
        let state = if present {
            SOURCE_PRESENT
        } else {
            SOURCE_ABSENT
        };
        sfnt.glyph_sources
            .fetch_or(state << shift, Ordering::Relaxed);
        present
    }

    /// Returns `true` if the font has glyphs from any bitmap table, `COLR`
    /// or `VARC` that `order` lists.
    ///
    /// Reads the cached results once and probes only the sources not yet
    /// checked.
    pub(crate) fn has_layered_source_in(&self, order: GlyphSourceOrder) -> bool {
        let Some(sfnt) = self.shared().sfnt() else {
            return false;
        };
        let known = sfnt
            .glyph_sources
            .load(core::sync::atomic::Ordering::Relaxed);
        order.iter().any(|source| {
            if !matches!(
                source,
                GlyphSource::Sbix
                    | GlyphSource::Cbdt
                    | GlyphSource::Ebdt
                    | GlyphSource::Colr
                    | GlyphSource::Varc
            ) {
                return false;
            }
            match (known >> (2 * (source as u32 - 1))) & 3 {
                SOURCE_ABSENT => false,
                SOURCE_PRESENT => true,
                _ => self.has_layered_source(source),
            }
        })
    }

    /// Returns the probed state of the bitmap, color and composite sources.
    #[cfg(test)]
    pub(crate) fn probed_glyph_sources(&self) -> u32 {
        self.shared().sfnt().map_or(0, |sfnt| {
            sfnt.glyph_sources
                .load(core::sync::atomic::Ordering::Relaxed)
        })
    }

    /// Returns the variable composites, read once for the font.
    #[inline]
    pub(crate) fn varc_outlines(&self) -> Option<&varc::Outlines<'_>> {
        self.shared().varc_outlines()
    }

    /// Returns `COLR` with its variation tables, read once for the font.
    #[inline]
    pub(crate) fn colr_tables(&self) -> Option<&ColrTables<'_>> {
        self.shared().colr_tables()
    }

    /// Returns `CPAL`, read once for the font.
    #[inline]
    pub(crate) fn cpal(&self) -> Option<&Cpal<'_>> {
        self.shared().cpal()
    }

    /// Returns `sbix`, read once for the font.
    #[inline]
    pub(crate) fn sbix(&self) -> Option<&Sbix<'_>> {
        self.shared().sbix()
    }

    /// Returns `CBLC` and `CBDT`, read once for the font.
    #[inline]
    pub(crate) fn cbdt(&self) -> Option<&(Cblc<'_>, Cbdt<'_>)> {
        self.shared().cbdt()
    }

    /// Returns `EBLC` and `EBDT`, read once for the font.
    #[inline]
    pub(crate) fn ebdt(&self) -> Option<&(Eblc<'_>, Ebdt<'_>)> {
        self.shared().ebdt()
    }

    /// Returns `VORG`.
    #[inline]
    pub(crate) fn vorg(&self) -> Option<&Vorg<'_>> {
        self.shared().vorg()
    }
}

impl<'a> OutlineContext<'a> for &'a Font {
    fn glyph(&self, glyph: GlyphId) -> Option<LocaGlyph<'a>> {
        let (glyf, loca) = self.selected_glyf_loca()?;
        loca.get(glyph, glyf)
    }

    fn glyph_variation_data(&self, glyph: GlyphId) -> Option<GlyphVariationData<'a>> {
        self.selected_gvar()?.glyph_variation_data(glyph).ok()?
    }

    fn has_gvar(&self) -> bool {
        self.selected_gvar().is_some()
    }

    fn phantom_point_deltas(&self, glyph: GlyphId) -> Option<[Point<Fixed>; PHANTOM_POINT_COUNT]> {
        let (glyf, loca) = self.selected_glyf_loca()?;
        self.selected_gvar()?.phantom_point_deltas_with_scalars(
            glyf,
            loca,
            self.normalized_coords(),
            self.gvar_scalars(),
            glyph,
        )
    }

    fn gvar_scalars(&self) -> &[Fixed] {
        Font::gvar_scalars(self)
    }

    fn coords(&self) -> &[F2Dot14] {
        self.normalized_coords()
    }

    fn units_per_em(&self) -> u16 {
        Font::units_per_em(self)
    }

    fn h_metrics(&self, glyph: GlyphId) -> (i32, i32) {
        let metrics = self.h_metric_records();
        (
            metrics
                .stated_side_bearing(glyph)
                .unwrap_or_default()
                .to_i32(),
            metrics.stated_advance(self.num_glyphs(), glyph).to_i32(),
        )
    }

    fn v_metrics(&self, glyph: GlyphId) -> Option<(i32, i32)> {
        let metrics = self.v_metric_records();
        if metrics.is_empty() {
            return None;
        }
        Some((
            metrics
                .stated_side_bearing(glyph)
                .unwrap_or_default()
                .to_i32(),
            metrics.stated_advance(self.num_glyphs(), glyph).to_i32(),
        ))
    }

    fn h_line_metrics(&self) -> (i32, i32) {
        self.shared().metrics().h_line().map_or((0, 0), |line| {
            (line.ascender.to_i32(), line.descender.to_i32())
        })
    }

    fn has_hvar(&self) -> bool {
        self.hvar().is_some()
    }
}

/// The underlying type of a font.
#[derive(Clone)]
pub enum Kind<'a> {
    /// An SFNT-based font represented by a set of tables and an index.
    Sfnt(&'a Tables, u32),
    /// An Adobe Type 1 font.
    Type1(&'a Type1Font),
    /// A standalone CFF font with an associated index.
    Cff(CffFontRef<'a>, u32),
}

/// Builder for configuring a font instance.
pub struct InstanceBuilder {
    font: SharedFont,
    coords: CoordStorage,
    /// The blend coordinates of a Type 1 multiple master location, which
    /// `coords` holds rounded to 2.14. The weights are computed from these.
    type1_blend: SmallVec<[Fixed; 4]>,
}

impl InstanceBuilder {
    /// Sets the variations for the font instance from an unordered sequence of
    /// variations in user space.
    ///
    /// Omitted axes will be set to their default values. Unsupported axes are
    /// ignored. If an axis is specified multiple times, the last value is used.
    ///
    /// This will overwrite any previous variation settings.
    pub fn variations<V>(mut self, variations: V) -> Self
    where
        V: IntoIterator,
        V::Item: Into<Variation>,
    {
        self.set_variations(variations);
        self
    }

    /// Sets the variations for the font instance from an ordered sequence
    /// of normalized coordinates.
    ///
    /// If the number of provided coordinates is less than the number of axes,
    /// the remaining axes will be set to their default values. If the number
    /// of provided coordinates is greater than the number of axes, the extra
    /// coordinates will be ignored.
    ///
    /// This will overwrite any previous variation settings.
    pub fn normalized_coords(mut self, coords: impl IntoIterator<Item = NormalizedCoord>) -> Self {
        self.set_coords(coords);
        self
    }

    /// Sets the variations for the font instance from a named instance.
    ///
    /// If the given named instance index is invalid, then variation settings
    /// will be reset to default.
    ///
    /// This will overwrite any previous variation settings.
    pub fn named_instance(mut self, index: u32) -> Self {
        self.set_named_instance(index);
        self
    }

    /// Sets the variations for the font instance from a named instance, with
    /// additional overrides.
    ///
    /// If the given named instance index is invalid, then it is ignored and
    /// only overrides are applied.
    ///
    /// This will overwrite any previous variation settings.
    pub fn named_instance_with_overrides<V>(mut self, index: u32, overrides: V) -> Self
    where
        V: IntoIterator,
        V::Item: Into<Variation>,
    {
        self.set_named_instance_with_overrides(index, overrides);
        self
    }

    /// Builds the font instance.
    pub fn build(self) -> Font {
        // An all-default location is just the font, so nothing needs owning.
        // This is what keeps `VariedInstance::coords` non-empty.
        if self.coords.as_slice().is_empty() {
            return Font(Repr::Default(self.font));
        }
        let kind = match &self.font.0.kind {
            KindRepr::Sfnt(_) => VariedKind::Sfnt(VariedSfnt {
                location: SfntLocation::new(),
                gvar_scalars: Once::new(),
                hvar_scalars: Once::new(),
                colr_scalars: Once::new(),
                vvar_scalars: Once::new(),
            }),
            KindRepr::Type1(type1) => {
                let Some(mm) = type1.font.multiple_master() else {
                    return Font(Repr::Default(self.font));
                };
                let mut weights = SmallVec::from_elem(Fixed::ZERO, mm.num_masters());
                mm.blend_to_weights(&self.type1_blend, &mut weights);
                VariedKind::Type1(VariedType1 { weights })
            }
            // A standalone CFF font has no axes, so no coordinates reach
            // here; it is at its default location.
            KindRepr::StandaloneCff(_) => return Font(Repr::Default(self.font)),
        };
        Font(Repr::Varied(Arc::new(VariedInstance {
            font: self.font,
            coords: self.coords,
            location: LocationCaches::new(),
            kind,
        })))
    }
}

impl InstanceBuilder {
    fn set_variations<V>(&mut self, variations: V)
    where
        V: IntoIterator,
        V::Item: Into<Variation>,
    {
        if let Some(mm) = self.font.multiple_master() {
            // Design coordinates, starting from each axis's default.
            let axes = self.font.type1_axes();
            let mut design: SmallVec<[Fixed; 4]> =
                axes.iter().map(|axis| axis.default_value()).collect();
            let mut named: SmallVec<[bool; 4]> = SmallVec::from_elem(false, design.len());
            for variation in variations {
                let variation = variation.into();
                for ((axis, value), named) in axes.iter().zip(design.iter_mut()).zip(&mut named) {
                    if axis.axis_tag() == variation.tag {
                        *value = Fixed::from_f64(variation.value as f64);
                        *named = true;
                    }
                }
            }
            let mut blend = SmallVec::from_elem(Fixed::ZERO, design.len());
            mm.design_to_blend(&design, &mut blend);
            // An axis no variation names keeps the default blend. Its
            // default design coordinate doesn't always map back to it.
            let mut default = SmallVec::<[Fixed; 4]>::from_elem(Fixed::ZERO, design.len());
            mm.default_blend(&mut default);
            for ((value, default), named) in blend.iter_mut().zip(default).zip(named) {
                if !named {
                    *value = default;
                }
            }
            self.set_type1_blend(blend);
            return;
        }
        let tables = self.font.tables();
        if let Ok(fvar) = tables.fvar() {
            set_variations(&fvar, tables.avar().ok(), &mut self.coords, variations);
        } else {
            self.coords.clear();
        }
    }

    fn set_coords(&mut self, coords: impl IntoIterator<Item = NormalizedCoord>) {
        if let Some(mm) = self.font.multiple_master() {
            // Blend coordinates, with the default blend for any not given.
            let mut blend: SmallVec<[Fixed; 4]> = SmallVec::from_elem(Fixed::ZERO, mm.axes().len());
            mm.default_blend(&mut blend);
            for (dst, src) in blend.iter_mut().zip(coords) {
                *dst = src.to_fixed();
            }
            self.set_type1_blend(blend);
            return;
        }
        if let Ok(fvar) = self.font.tables().fvar() {
            let count = fvar.axis_count() as usize;
            self.coords.resize(count, NormalizedCoord::ZERO);
            for (dst, src) in self.coords.as_mut_slice().iter_mut().zip(
                coords
                    .into_iter()
                    .chain(core::iter::repeat(NormalizedCoord::ZERO)),
            ) {
                *dst = src;
            }
            clear_if_all_zeroes(&mut self.coords);
        } else {
            self.coords.clear();
        }
    }

    /// Sets the location of a Type 1 multiple master font.
    ///
    /// Every blend is kept, even all zeros, which is a master rather than
    /// the font's default.
    ///
    /// The weights are computed from `blend` itself. The coordinates are
    /// those the weights give back, which rounding in the weights can move
    /// from `blend` by the smallest step, as FreeType reports them.
    fn set_type1_blend(&mut self, blend: SmallVec<[Fixed; 4]>) {
        let font = self.font.clone();
        let Some(mm) = font.multiple_master() else {
            return;
        };
        let mut weights: SmallVec<[Fixed; 16]> = SmallVec::from_elem(Fixed::ZERO, mm.num_masters());
        mm.blend_to_weights(&blend, &mut weights);
        let mut reported: SmallVec<[Fixed; 4]> = SmallVec::from_elem(Fixed::ZERO, blend.len());
        mm.weights_to_blend(&weights, &mut reported);
        self.coords = reported.iter().map(|value| value.to_f2dot14()).collect();
        self.type1_blend = blend;
    }

    fn set_named_instance(&mut self, index: u32) {
        let tables = self.font.tables();
        if let Ok(fvar) = tables.fvar() {
            set_variations(
                &fvar,
                tables.avar().ok(),
                &mut self.coords,
                named_instance_variations(&fvar, index),
            );
        } else {
            self.coords.clear();
        }
    }

    fn set_named_instance_with_overrides<V>(&mut self, index: u32, overrides: V)
    where
        V: IntoIterator,
        V::Item: Into<Variation>,
    {
        // A Type 1 multiple master font has no named instances, so only the
        // overrides apply.
        if self.font.multiple_master().is_some() {
            self.set_variations(overrides);
            return;
        }
        let tables = self.font.tables();
        if let Ok(fvar) = tables.fvar() {
            set_variations(
                &fvar,
                tables.avar().ok(),
                &mut self.coords,
                named_instance_variations(&fvar, index)
                    .chain(overrides.into_iter().map(Into::into)),
            );
        } else {
            self.coords.clear();
        }
    }
}

// Helper to extract an iterator of Variation from a named instance index.
fn named_instance_variations<'a>(
    fvar: &'a Fvar,
    index: u32,
) -> impl Iterator<Item = Variation> + 'a {
    fvar.axis_instance_arrays()
        .ok()
        .and_then(|arrays| {
            let axes = arrays.axes();
            arrays.instances().get(index as usize).ok().map(|instance| {
                axes.iter()
                    .zip(instance.coordinates)
                    .map(|(axis, coord)| Variation::new(axis.axis_tag(), coord.get().to_f32()))
            })
        })
        .into_iter()
        .flatten()
}

/// Helper for setting variations.
///
/// Pulled out into a separate function to avoid borrow checker issues.
fn set_variations<V>(fvar: &Fvar, avar: Option<Avar>, coords: &mut CoordStorage, variations: V)
where
    V: IntoIterator,
    V::Item: Into<Variation>,
{
    coords.resize(fvar.axis_count() as usize, NormalizedCoord::ZERO);
    fvar.user_to_normalized(
        avar.as_ref(),
        variations
            .into_iter()
            .map(Into::into)
            .map(|var| (var.tag, Fixed::from_f64(var.value as _))),
        coords.as_mut_slice(),
    );
    clear_if_all_zeroes(coords);
}

/// The state every instance of a font shares.
///
/// Reference counted internally: cloning it costs no more than the count,
/// and it is thread safe.
#[derive(Clone)]
pub(super) struct SharedFont(Arc<SharedFontRepr>);

impl SharedFont {
    /// Creates a new font from the given source and font index.
    ///
    /// See [`Font::new`] for how `index` is used.
    fn new(source: impl Into<Source>, index: u32) -> Option<Self> {
        let source = source.into();
        let kind = if let Some(tables) = Tables::new(source.clone(), index) {
            Some(KindRepr::Sfnt(SfntCaches::new(Arc::new(tables), index)))
        } else if let Source::Blob(blob) = &source {
            match Format::new(blob) {
                Some(Format::Type1) => Type1Font::new(blob).ok().map(|font| {
                    let axes = font
                        .multiple_master()
                        .map(super::variation::type1_axis_records)
                        .unwrap_or_default();
                    KindRepr::Type1(Type1Data { font, axes })
                }),
                Some(Format::Cff(count)) if index < count => {
                    let font = Yoke::<CffFontRef<'static>, Arc<Blob>>::try_attach_to_cart(
                        Arc::new(blob.clone()),
                        |blob| CffFontRef::new_cff(blob, index, None),
                    )
                    .ok()?;
                    // A subfont that cannot be read fails only its own
                    // glyphs, not the font.
                    let count = font.get().num_subfonts().min(CACHED_SUBFONTS as _);
                    let subfonts = (0..count)
                        .map(|index| font.get().subfont(index, &[]).ok())
                        .collect();
                    Some(KindRepr::StandaloneCff(StandaloneCffData {
                        font,
                        subfonts,
                        index,
                        unicode_charmap: Once::new(),
                        cid_glyphs: Once::new(),
                    }))
                }
                _ => None,
            }
        } else {
            None
        };
        let kind = kind?;
        let repr = SharedFontRepr {
            source,
            kind,
            shaping_data: Once::new(),
            glyph_name_index: Once::new(),
            default_location: LocationCaches::new(),
        };
        Some(Self(Arc::new(repr)))
    }

    /// Returns the underlying source of font data.
    fn source(&self) -> &Source {
        &self.0.source
    }

    /// Returns the underlying kind of the font.
    pub(super) fn kind(&self) -> Kind<'_> {
        match &self.0.kind {
            KindRepr::Sfnt(sfnt) => Kind::Sfnt(&sfnt.tables, sfnt.index),
            KindRepr::Type1(type1) => Kind::Type1(&type1.font),
            KindRepr::StandaloneCff(cff) => Kind::Cff(cff.font.get().clone(), cff.index),
        }
    }

    /// Returns the design space of a Type 1 multiple master font.
    fn multiple_master(&self) -> Option<&MultipleMaster> {
        match &self.0.kind {
            KindRepr::Type1(type1) => type1.font.multiple_master(),
            _ => None,
        }
    }

    /// Returns the axes of a Type 1 multiple master font, which is empty
    /// for any other font.
    fn type1_axes(&self) -> &[VariationAxisRecord] {
        match &self.0.kind {
            KindRepr::Type1(type1) => &type1.axes,
            _ => &[],
        }
    }

    /// Returns the metrics describing the font as a whole, at its default
    /// location.
    #[inline]
    fn metrics(&self) -> &Metrics {
        self.0
            .default_location
            .metrics
            .get_or_init(|| match self.kind() {
                Kind::Type1(font) => Metrics::from_type1(font),
                Kind::Cff(font, _) => Metrics::from_cff(&font),
                _ => Metrics::from_sfnt(&self.tables(), &[]),
            })
    }

    /// Returns the style measurements at the font's default location.
    fn style_metrics(&self) -> &StyleMetrics {
        self.0
            .default_location
            .style_metrics
            .get_or_init(|| {
                Box::new(match self.kind() {
                    Kind::Type1(font) => StyleMetrics::from_type1(font),
                    Kind::Cff(font, _) => StyleMetrics::from_cff(&font),
                    _ => StyleMetrics::from_sfnt(&self.tables(), &[]),
                })
            })
            .as_ref()
    }

    /// Returns the number of glyphs in the font.
    ///
    /// Fixed for the font: no location varies it.
    #[inline]
    pub(super) fn num_glyphs(&self) -> u32 {
        self.metrics().num_glyphs
    }

    /// Returns the size of the em square, in font units.
    ///
    /// Fixed for the font: no location varies it.
    #[inline]
    fn units_per_em(&self) -> u16 {
        self.metrics().units_per_em
    }

    /// Returns the caches of an SFNT font, or `None` for a font of another
    /// kind.
    #[inline]
    pub(super) fn sfnt(&self) -> Option<&SfntCaches> {
        match &self.0.kind {
            KindRepr::Sfnt(sfnt) => Some(sfnt),
            _ => None,
        }
    }

    /// Returns the tables behind this font, for a cache that holds them.
    fn tables_arc(&self) -> Option<&Arc<Tables>> {
        self.sfnt().map(|sfnt| &sfnt.tables)
    }

    /// Returns the selected character maps, parsed once for the font.
    pub(super) fn unicode_charmap(&self) -> Option<&SfntUnicodeCharmap<'_>> {
        let sfnt = self.sfnt()?;
        Some(
            sfnt.charmap
                .get_or_init(|| {
                    TableCache::read(sfnt.tables.clone(), |tables| {
                        SfntUnicodeCharmap::read(&tables)
                    })
                })
                .get(),
        )
    }

    /// Returns all selectable cmap subtables, parsed on first use.
    pub(super) fn encodings(&self) -> Option<&EncodingTables<'_>> {
        let sfnt = self.sfnt()?;
        Some(
            sfnt.encodings
                .get_or_init(|| {
                    Box::new(TableCache::read(sfnt.tables.clone(), |tables| {
                        EncodingTables::read(&tables)
                    }))
                })
                .get(),
        )
    }

    pub(super) fn standalone_cff_unicode_charmap(&self) -> Option<&crate::ps::UnicodeCharmap> {
        let KindRepr::StandaloneCff(cff) = &self.0.kind else {
            return None;
        };
        Some(cff.unicode_charmap.get_or_init(|| {
            #[cfg(feature = "agl")]
            {
                let cff = cff.font.get();
                if cff.is_cid() {
                    return crate::ps::UnicodeCharmap::default();
                }
                crate::ps::UnicodeCharmap::from_glyph_names(
                    cff.charset()
                        .into_iter()
                        .flat_map(|charset| charset.iter())
                        .filter_map(|(glyph, sid)| {
                            let name = core::str::from_utf8(cff.string(sid)?).ok()?;
                            Some((glyph, name))
                        }),
                )
            }
            #[cfg(not(feature = "agl"))]
            {
                let _ = cff;
                crate::ps::UnicodeCharmap::default()
            }
        }))
    }

    /// Returns the horizontal glyph metrics, parsed once for the font.
    #[inline]
    fn h_metric_records(&self) -> &GlyphMetricRecords<'_> {
        let Some(sfnt) = self.sfnt() else {
            return empty_glyph_metrics();
        };
        &sfnt
            .h_metrics
            .get_or_init(|| {
                TableCache::read(sfnt.tables.clone(), |tables| {
                    MetricsTable(tables.h_glyph_metric_records().unwrap_or_default())
                })
            })
            .get()
            .0
    }

    /// Returns the vertical glyph metrics, parsed once for the font.
    ///
    /// Read on demand, unlike the horizontal metrics: most text is
    /// horizontal and never asks.
    #[inline]
    fn v_metric_records(&self) -> &GlyphMetricRecords<'_> {
        let Some(sfnt) = self.sfnt() else {
            return empty_glyph_metrics();
        };
        &sfnt
            .v_metrics
            .get_or_init(|| {
                TableCache::read(sfnt.tables.clone(), |tables| {
                    MetricsTable(tables.v_glyph_metric_records().unwrap_or_default())
                })
            })
            .get()
            .0
    }

    /// Returns the outline tables, parsed once for the font.
    ///
    /// Outlines and metrics in both directions read these, so they are
    /// shared rather than parsed by each.
    #[inline]
    fn selected_glyf_loca(&self) -> Option<&(Glyf<'_>, Loca<'_>)> {
        let sfnt = self.sfnt()?;
        sfnt.glyf_loca
            .get_or_init(|| TableCache::read(sfnt.tables.clone(), |tables| GlyfLoca::read(&tables)))
            .get()
            .0
            .as_ref()
    }

    /// Returns `HVAR`, parsed once for the font.
    #[inline]
    fn hvar(&self) -> Option<&Hvar<'_>> {
        let sfnt = self.sfnt()?;
        sfnt.hvar
            .get_or_init(|| {
                TableCache::read(sfnt.tables.clone(), |tables| HvarTable::read(&tables))
            })
            .get()
            .0
            .as_ref()
    }

    /// Returns `VVAR`, parsed once for the font.
    #[inline]
    fn vvar(&self) -> Option<&Vvar<'_>> {
        let sfnt = self.sfnt()?;
        sfnt.vvar
            .get_or_init(|| {
                TableCache::read(sfnt.tables.clone(), |tables| VvarTable::read(&tables))
            })
            .get()
            .0
            .as_ref()
    }

    /// Returns `GVAR`, or `gvar` without it, parsed once for the font.
    ///
    /// Typically about half a variable font, so nothing should ask for this
    /// that another table can answer.
    #[inline]
    fn selected_gvar(&self) -> Option<&SelectedGvar<'_>> {
        let sfnt = self.sfnt()?;
        sfnt.gvar
            .get_or_init(|| {
                TableCache::read(sfnt.tables.clone(), |tables| GvarTable::read(&tables))
            })
            .get()
            .0
            .as_ref()
    }

    /// Returns `VORG`, parsed once for the font.
    #[inline]
    fn vorg(&self) -> Option<&Vorg<'_>> {
        let sfnt = self.sfnt()?;
        sfnt.vorg
            .get_or_init(|| {
                TableCache::read(sfnt.tables.clone(), |tables| VorgTable::read(&tables))
            })
            .get()
            .0
            .as_ref()
    }

    /// Returns the `CFF2` charstring outlines, parsed once for the font.
    ///
    /// Reading one means running it, so nothing here is parsed per glyph
    /// beyond what a charstring itself needs.
    #[inline]
    fn cff2(&self) -> Option<&CffFontRef<'_>> {
        let sfnt = self.sfnt()?;
        sfnt.cff2
            .get_or_init(|| TableCache::read(sfnt.tables.clone(), |tables| Cff2Font::read(&tables)))
            .get()
            .0
            .as_ref()
    }

    /// Returns the `CFF` charstring outlines, parsed once for the font.
    #[inline]
    fn cff(&self) -> Option<&CffFontRef<'_>> {
        let sfnt = self.sfnt()?;
        sfnt.cff
            .get_or_init(|| TableCache::read(sfnt.tables.clone(), |tables| CffFont::read(&tables)))
            .get()
            .0
            .as_ref()
    }

    fn standalone_cff(&self) -> Option<&CffFontRef<'_>> {
        match &self.0.kind {
            KindRepr::StandaloneCff(cff) => Some(cff.font.get()),
            _ => None,
        }
    }

    /// Returns the glyph for each CID of a standalone CID-keyed font, built
    /// once.
    ///
    /// As in FreeType, the table ends at the largest CID and the first glyph
    /// with a CID is kept.
    fn cid_glyphs(&self) -> Option<&[u16]> {
        let KindRepr::StandaloneCff(data) = &self.0.kind else {
            return None;
        };
        let cff = data.font.get();
        if !cff.is_cid() {
            return None;
        }
        let glyphs = data.cid_glyphs.get_or_init(|| {
            let num_glyphs = cff.num_glyphs();
            let Some(charset) = cff.charset() else {
                return Box::new([0]);
            };
            cid_table(|| {
                charset
                    .iter()
                    .filter(move |(glyph, _)| glyph.to_u32() < num_glyphs)
                    .map(|(glyph, cid)| (glyph.to_u32() as u16, cid.to_u16() as usize))
            })
        });
        Some(glyphs)
    }

    fn standalone_cff_subfonts(&self) -> Option<&[Option<Subfont>]> {
        match &self.0.kind {
            KindRepr::StandaloneCff(cff) => Some(&cff.subfonts),
            _ => None,
        }
    }

    /// Returns the variable composites, read once for the font.
    fn varc_outlines(&self) -> Option<&varc::Outlines<'_>> {
        let sfnt = self.sfnt()?;
        sfnt.varc
            .get_or_init(|| {
                Box::new(TableCache::read(sfnt.tables.clone(), |tables| {
                    varc::Outlines::read(&tables)
                }))
            })
            .get()
            .as_ref()
    }

    /// Returns `COLR` with its variation tables, read once for the font.
    fn colr_tables(&self) -> Option<&ColrTables<'_>> {
        let sfnt = self.sfnt()?;
        sfnt.colr
            .get_or_init(|| {
                Box::new(TableCache::read(sfnt.tables.clone(), |tables| {
                    ColrTables::read(&tables)
                }))
            })
            .get()
            .as_ref()
    }

    /// Returns `CPAL`, read once for the font.
    fn cpal(&self) -> Option<&Cpal<'_>> {
        let sfnt = self.sfnt()?;
        sfnt.cpal
            .get_or_init(|| {
                Box::new(TableCache::read(sfnt.tables.clone(), |tables| {
                    CpalTable::read(&tables)
                }))
            })
            .get()
            .0
            .as_ref()
    }

    /// Returns `sbix`, read once for the font.
    fn sbix(&self) -> Option<&Sbix<'_>> {
        let sfnt = self.sfnt()?;
        sfnt.sbix
            .get_or_init(|| {
                Box::new(TableCache::read(sfnt.tables.clone(), |tables| {
                    SbixTable::read(&tables)
                }))
            })
            .get()
            .0
            .as_ref()
    }

    /// Returns `CBLC` and `CBDT`, read once for the font.
    fn cbdt(&self) -> Option<&(Cblc<'_>, Cbdt<'_>)> {
        let sfnt = self.sfnt()?;
        sfnt.cbdt
            .get_or_init(|| {
                Box::new(TableCache::read(sfnt.tables.clone(), |tables| {
                    CbdtTables::read(&tables)
                }))
            })
            .get()
            .0
            .as_ref()
    }

    /// Returns `EBLC` and `EBDT`, read once for the font.
    fn ebdt(&self) -> Option<&(Eblc<'_>, Ebdt<'_>)> {
        let sfnt = self.sfnt()?;
        sfnt.ebdt
            .get_or_init(|| {
                Box::new(TableCache::read(sfnt.tables.clone(), |tables| {
                    EbdtTables::read(&tables)
                }))
            })
            .get()
            .0
            .as_ref()
    }

    /// Returns an object that provides access to individual font tables.
    ///
    /// For non-SFNT fonts, this will return an empty set of tables.
    fn tables(&self) -> &Tables {
        if let KindRepr::Sfnt(sfnt) = &self.0.kind {
            &sfnt.tables
        } else {
            &tables::EMPTY_FONT_TABLES
        }
    }
}

/// Builds the table from each CID to its glyph, keeping the first glyph for
/// a CID.
///
/// `cids` yields each glyph and its CID, glyph zero first with CID zero.
fn cid_table<I: Iterator<Item = (u16, usize)>>(cids: impl Fn() -> I) -> Box<[u16]> {
    let max_cid = cids().map(|(_, cid)| cid).max().unwrap_or(0);
    let mut glyphs = alloc::vec![0u16; max_cid + 1];
    // Zero marks an empty slot. Glyph zero has CID zero, so that slot is
    // already right and no later glyph takes it.
    for (glyph, cid) in cids() {
        if cid != 0 && glyphs[cid] == 0 {
            glyphs[cid] = glyph;
        }
    }
    glyphs.into_boxed_slice()
}

struct SharedFontRepr {
    source: Source,
    kind: KindRepr,
    // Storage cell for lazily loaded HarfRust shaping data.
    shaping_data: Once<Box<dyn Any + Send + Sync>>,
    // The glyph names sorted for lookup by name.
    glyph_name_index: Once<name::GlyphNameIndex>,
    // The caches of the default location, shared by every default instance.
    // A varied instance keeps its own.
    default_location: LocationCaches,
}

/// The underlying type of a font.
///
/// Every variant is stored inline. The SFNT caches are the largest variant,
/// so the other variants add no size. Inlining also avoids a pointer
/// indirection in the metric lookups for every advance.
#[allow(
    clippy::large_enum_variant,
    reason = "boxing the SFNT caches would put a pointer chase on every advance, to save space only in the rare fonts of the other kinds"
)]
enum KindRepr {
    Sfnt(SfntCaches),
    Type1(Type1Data),
    StandaloneCff(StandaloneCffData),
}

/// A Type 1 font and the axes of its design space, if it has one.
struct Type1Data {
    font: Type1Font,
    axes: Box<[VariationAxisRecord]>,
}

/// Caches for an SFNT font, with each table parsed once per font.
pub(super) struct SfntCaches {
    tables: Arc<Tables>,
    index: u32,
    // Whether the font needs its instructions for correct contours: zero
    // until asked, then one more than the answer.
    hint_reliant: core::sync::atomic::AtomicU8,
    // Which of the bitmap, color and composite sources the font has: two
    // bits per source, zero until asked, then absent or present. They are
    // probed one at a time, as something asks for each.
    glyph_sources: core::sync::atomic::AtomicU32,
    // The default location's own SFNT caches. A varied instance keeps its
    // own.
    default_location: SfntLocation,
    // The horizontal glyph metrics, parsed once for the font. Held beside
    // the tables they borrow, which is what lets them live here at all.
    h_metrics: Once<TableCache<MetricsTable<'static>>>,
    // The vertical glyph metrics, read only when something measures
    // vertically.
    v_metrics: Once<TableCache<MetricsTable<'static>>>,
    // The outlines, and the tables that vary metrics: `HVAR` and `VVAR` hold
    // metric deltas, and `gvar` holds outline deltas, with metric deltas in
    // its phantom points. None depends on a location to parse, so every
    // instance shares them. Each is read when something first needs it.
    glyf_loca: Once<TableCache<GlyfLoca<'static>>>,
    hvar: Once<TableCache<HvarTable<'static>>>,
    vvar: Once<TableCache<VvarTable<'static>>>,
    gvar: Once<TableCache<GvarTable<'static>>>,
    // The charstrings of a `CFF2` or `CFF` font, which neither metrics nor
    // outlines read unless the font states them this way. Each table has a
    // cell of its own, so an order can prefer either of a font with both.
    cff2: Once<TableCache<Cff2Font<'static>>>,
    cff: Once<TableCache<CffFont<'static>>>,
    // Tables that draw glyphs in other ways, read only when something asks
    // for that way. Boxed, so a font without them pays a pointer each.
    varc: Once<Box<TableCache<Option<varc::Outlines<'static>>>>>,
    colr: Once<Box<TableCache<Option<ColrTables<'static>>>>>,
    cpal: Once<Box<TableCache<CpalTable<'static>>>>,
    sbix: Once<Box<TableCache<SbixTable<'static>>>>,
    cbdt: Once<Box<TableCache<CbdtTables<'static>>>>,
    ebdt: Once<Box<TableCache<EbdtTables<'static>>>>,
    // Where a glyph's vertical origin sits, read only by vertical text.
    vorg: Once<TableCache<VorgTable<'static>>>,
    // Selected Unicode cmap subtables and their encoding metadata.
    charmap: Once<TableCache<SfntUnicodeCharmap<'static>>>,
    // Only clients selecting a charmap pay for the dense record array.
    encodings: Once<Box<TableCache<EncodingTables<'static>>>>,
}

impl SfntCaches {
    fn new(tables: Arc<Tables>, index: u32) -> Self {
        Self {
            tables,
            index,
            hint_reliant: core::sync::atomic::AtomicU8::new(0),
            glyph_sources: core::sync::atomic::AtomicU32::new(0),
            default_location: SfntLocation::new(),
            h_metrics: Once::new(),
            v_metrics: Once::new(),
            glyf_loca: Once::new(),
            hvar: Once::new(),
            vvar: Once::new(),
            gvar: Once::new(),
            cff2: Once::new(),
            cff: Once::new(),
            varc: Once::new(),
            colr: Once::new(),
            cpal: Once::new(),
            sbix: Once::new(),
            cbdt: Once::new(),
            ebdt: Once::new(),
            vorg: Once::new(),
            charmap: Once::new(),
            encodings: Once::new(),
        }
    }
}

struct StandaloneCffData {
    font: Yoke<CffFontRef<'static>, Arc<Blob>>,
    /// The first [`CACHED_SUBFONTS`] subfonts, parsed when the font is
    /// read; `None` where a private dict could not be.
    subfonts: Box<[Option<Subfont>]>,
    index: u32,
    unicode_charmap: Once<crate::ps::UnicodeCharmap>,
    /// The glyph for each CID of a CID-keyed font, zero where none has it.
    cid_glyphs: Once<Box<[u16]>>,
}

/// Caches for one location of a font of any kind.
struct LocationCaches {
    /// Data that Skrifa derives from the font at this location.
    scaling_data: Once<Box<dyn Any + Send + Sync>>,
    // Metrics that describe the font as a whole, read once rather than per
    // query. Kept apart from the shaping and scaling slots, which each hold
    // one thing for one owner.
    metrics: Once<Metrics>,
    style_metrics: Once<Box<StyleMetrics>>,
}

impl LocationCaches {
    const fn new() -> Self {
        Self {
            scaling_data: Once::new(),
            metrics: Once::new(),
            style_metrics: Once::new(),
        }
    }
}

/// Caches for one location of an SFNT font.
struct SfntLocation {
    // Feature variation selection. Zero coordinates can still satisfy
    // conditions, so the default location keeps one too.
    feature_vars: FeatureVarsStorage,
    // CFF private dicts blend at a location, so each location parses its
    // own, as glyphs ask.
    cff2_subfonts: SubfontCache,
    cff_subfonts: SubfontCache,
}

impl SfntLocation {
    fn new() -> Self {
        Self {
            feature_vars: FeatureVarsStorage::new(),
            cff2_subfonts: Once::new(),
            cff_subfonts: Once::new(),
        }
    }
}

/// What a location away from the default carries.
struct VariedInstance {
    font: SharedFont,
    /// Never empty: an all-default location is [`Repr::Default`].
    coords: CoordStorage,
    location: LocationCaches,
    kind: VariedKind,
}

/// Caches for a varied instance, by font kind.
///
/// SFNT fonts and Type 1 multiple master fonts have variation axes.
enum VariedKind {
    Sfnt(VariedSfnt),
    Type1(VariedType1),
}

/// A Type 1 multiple master font at a location.
struct VariedType1 {
    /// The weight of each master.
    weights: SmallVec<[Fixed; 16]>,
}

/// Caches for a varied SFNT instance.
struct VariedSfnt {
    location: SfntLocation,
    // The region scalars at the location, for the variation tables that
    // state deltas by region. The default location needs none.
    gvar_scalars: Once<Box<[Fixed]>>,
    hvar_scalars: Once<Box<[Fixed]>>,
    vvar_scalars: Once<Box<[Fixed]>>,
    colr_scalars: Once<Box<[Fixed]>>,
}

const MAX_PRECOMPUTED_SCALARS: usize = 128;

/// The kept answer for a glyph source the font does not have.
const SOURCE_ABSENT: u32 = 1;
/// The kept answer for a glyph source the font has.
const SOURCE_PRESENT: u32 = 2;

/// Returns up to [`MAX_PRECOMPUTED_SCALARS`] scalars from `count`, filled by
/// `compute`, which returns how many it wrote.
fn scalars_with(count: usize, compute: impl FnOnce(&mut [Fixed]) -> usize) -> Box<[Fixed]> {
    let mut scalars = alloc::vec![Fixed::ZERO; count.min(MAX_PRECOMPUTED_SCALARS)];
    let written = compute(&mut scalars);
    scalars.truncate(written);
    scalars.into_boxed_slice()
}

/// Returns the region scalars of `store` at `coords`, or none without a
/// store.
fn store_scalars(
    store: Option<&crate::tables::variations::ItemVariationStore<'_>>,
    coords: &[NormalizedCoord],
) -> Box<[Fixed]> {
    let Some(store) = store else {
        return Box::new([]);
    };
    let count = store
        .variation_region_list()
        .ok()
        .map_or(0, |regions| regions.variation_regions().len());
    scalars_with(count, |out| store.compute_scalars(coords, out))
}

/// Lazy atomic storage for feature variation selections.
///
/// We don't want to load the GSUB and GPOS tables unless explicitly requested.
struct FeatureVarsStorage {
    gsub: AtomicU32,
    gpos: AtomicU32,
    /// Written before the indices, so it is current once they are.
    has_lookup_variations: AtomicBool,
}

impl Default for FeatureVarsStorage {
    fn default() -> Self {
        Self::new()
    }
}

impl FeatureVarsStorage {
    // Both sentinels sit where no record index reaches: a feature variation
    // record is eight bytes, so indexing this high would take a font of tens
    // of gigabytes.
    /// Nothing has read the tables yet.
    const UNCHECKED: u32 = u32::MAX;
    /// The tables were read and selected nothing.
    const ABSENT: u32 = u32::MAX - 1;

    fn new() -> Self {
        Self {
            gsub: AtomicU32::new(Self::UNCHECKED),
            gpos: AtomicU32::new(Self::UNCHECKED),
            has_lookup_variations: AtomicBool::new(false),
        }
    }

    fn load(&self, font: &SharedFont, coords: &[NormalizedCoord]) -> SelectedFeatureVariations {
        let (gsub, gpos) = (
            self.gsub.load(atomic::Ordering::Acquire),
            self.gpos.load(atomic::Ordering::Acquire),
        );
        if gsub != Self::UNCHECKED && gpos != Self::UNCHECKED {
            return SelectedFeatureVariations {
                gsub: Self::decode(gsub),
                gpos: Self::decode(gpos),
                has_lookup_variations: self.has_lookup_variations.load(atomic::Ordering::Relaxed),
            };
        }
        // Two threads that race here select the same indices, so the second
        // store writes what the first did.
        let selected = SelectedFeatureVariations::new(&font.tables(), coords);
        self.has_lookup_variations
            .store(selected.has_lookup_variations, atomic::Ordering::Relaxed);
        self.gsub
            .store(Self::encode(selected.gsub), atomic::Ordering::Release);
        self.gpos
            .store(Self::encode(selected.gpos), atomic::Ordering::Release);
        selected
    }

    /// Returns the index a stored word holds, if it holds one.
    fn decode(value: u32) -> Option<u32> {
        (value < Self::ABSENT).then_some(value)
    }

    /// Returns the word that stores a selection.
    fn encode(index: Option<u32>) -> u32 {
        index.unwrap_or(Self::ABSENT)
    }
}

/// A cache of CFF subfonts at one location, each parsed when a glyph first
/// needs it.
///
/// A `None` cell means the subfont could not be read.
type SubfontCache = Once<Box<[Once<Option<Subfont>>]>>;

/// The number of CFF subfonts a font keeps once parsed. A subfont past them
/// is parsed again whenever a glyph needs it.
const CACHED_SUBFONTS: usize = 8;

/// Maximum number of coordinates we store inline. Chosen to maximize the
/// number of coords without growing the storage: a `SmallVec` is a capacity
/// alongside a union of the inline array and the heap pointer, so twelve
/// two byte coords are the most that fit in 32 bytes.
const MAX_INLINE_COORDS: usize = 12;

type CoordStorage = SmallVec<[NormalizedCoord; MAX_INLINE_COORDS]>;

/// Empties `coords` if every coordinate is zero. This lets the default
/// instance skip variation processing behind a plain `is_empty` check.
fn clear_if_all_zeroes(coords: &mut CoordStorage) {
    if coords.iter().all(|&c| c == NormalizedCoord::ZERO) {
        coords.clear();
    }
}

#[cfg(test)]
impl Font {
    /// The storage behind a varied instance, for the cache tests.
    fn feature_vars(&self) -> &FeatureVarsStorage {
        match &self.0 {
            Repr::Varied(varied) => &varied.sfnt().location.feature_vars,
            Repr::Default(_) => panic!("the default instance keeps no storage of its own"),
        }
    }
}

#[cfg(test)]
impl VariedInstance {
    /// Returns the SFNT caches of the instance, for the cache tests.
    fn sfnt(&self) -> &VariedSfnt {
        match &self.kind {
            VariedKind::Sfnt(sfnt) => sfnt,
            VariedKind::Type1(_) => panic!("not an SFNT instance"),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ps::Sid;
    use crate::tables::glyf::outline::{Outline, Unscaled};
    use core::sync::atomic::Ordering;
    use types::Tag;

    /// Returns where glyph 1 of the synthetic multiple master font starts
    /// drawing, which moves with the weight of its second master.
    fn type1_mm_start(font: &Font) -> f32 {
        use crate::model::glyph::outline::{DrawMemory, DrawOptions, PathElement};
        let mut path = alloc::vec::Vec::<PathElement>::new();
        font.outline_glyphs()
            .get(GlyphId::new(1))
            .unwrap()
            .draw(&DrawOptions::default(), DrawMemory::Scratch, &mut path)
            .unwrap();
        match path[0] {
            PathElement::MoveTo { x, .. } => x,
            _ => panic!("expected a move"),
        }
    }

    // A Type 1 multiple master font has an axis per blend axis, and draws
    // and measures at the location an instance is built for.
    #[test]
    fn type1_multiple_master_instances() {
        use crate::ps::type1::multiple_master_tests::{pfa_with_blend, ONE_AXIS};
        let font = Font::new(pfa_with_blend(ONE_AXIS), 0).unwrap();
        let axes = font.axes();
        assert_eq!(axes.len(), 1);
        let axis = axes.get(0).unwrap();
        assert_eq!(axis.tag(), Tag::new(b"wght"));
        // The default is the font's own blend: a weight of 0.25 on the
        // second master, at design 300.
        assert_eq!(
            (axis.min_value(), axis.default_value(), axis.max_value()),
            (100.0, 300.0, 900.0)
        );
        let name = font.names().get(axis.name()).unwrap();
        assert_eq!(name.english_or_first().unwrap().name(), "Weight");
        assert!(font.named_instances().is_empty());
        assert!(font.normalized_coords().is_empty());
        assert_eq!(type1_mm_start(&font), 150.0);
        // The normalized coordinates are the blend, from 0 to 1. The first
        // master is all zeros, and stays a location of its own.
        for (design, blend, start) in [
            (900.0, 1.0, 300.0),
            (500.0, 0.5, 200.0),
            (100.0, 0.0, 100.0),
        ] {
            let instance = font
                .instance_builder()
                .variations([("wght", design)])
                .build();
            assert_eq!(
                instance.normalized_coords(),
                [NormalizedCoord::from_f32(blend)],
                "{design}"
            );
            assert_eq!(type1_mm_start(&instance), start, "{design}");
            let extents = instance.glyph_metrics().extents(GlyphId::new(1)).unwrap();
            assert_eq!(extents.x_bearing, start, "{design}");
            let by_blend = font
                .instance_builder()
                .normalized_coords([NormalizedCoord::from_f32(blend)])
                .build();
            assert_eq!(type1_mm_start(&by_blend), start, "{design}");
        }
    }

    // A variation naming no axis leaves the default instance alone, even
    // where the default design coordinate doesn't map back to the default
    // blend.
    #[test]
    fn type1_multiple_master_ignores_unknown_axes() {
        use crate::ps::type1::multiple_master_tests::pfa_with_blend;
        let font = Font::new(
            pfa_with_blend(
                "/BlendAxisTypes [/Weight] def /BlendDesignPositions [[0] [1]] def                  /BlendDesignMap [[[0 0] [500 0.5] [1000 1]]] def                  /WeightVector [0.25 0.75] def",
            ),
            0,
        )
        .unwrap();
        assert_eq!(type1_mm_start(&font), 250.0);
        let instance = font.instance_builder().variations([("XXXX", 42.0)]).build();
        assert_eq!(type1_mm_start(&instance), 250.0);
    }

    // Design maps spanning every design coordinate give axes whose values
    // saturate.
    #[test]
    fn type1_multiple_master_widest_axes() {
        use crate::ps::type1::multiple_master_tests::pfa_with_blend;
        let font = Font::new(
            pfa_with_blend(
                "/BlendAxisTypes [/Weight] def /BlendDesignPositions [[0] [1]] def                  /BlendDesignMap [[[-2147483648 0] [2147483647 1]]] def                  /WeightVector [0.5 0.5] def",
            ),
            0,
        )
        .unwrap();
        let axis = font.axes().get(0).unwrap();
        assert_eq!(
            (axis.min_value(), axis.default_value(), axis.max_value()),
            (-32768.0, 0.0, 32767.0)
        );
    }

    // A Type 1 font's metrics don't vary, so an instance reports the
    // font's own.
    #[test]
    fn type1_multiple_master_instances_keep_metrics() {
        use crate::ps::type1::multiple_master_tests::{pfa_with_blend, ONE_AXIS};
        let font = Font::new(
            pfa_with_blend(&format!(
                "{ONE_AXIS} /FontBBox [-10 -200 600 800] def \
                 /ItalicAngle -12 def /UnderlinePosition -100 def \
                 /UnderlineThickness 50 def"
            )),
            0,
        )
        .unwrap();
        let instance = font
            .instance_builder()
            .variations([("wght", 900.0)])
            .build();
        assert!(!instance.normalized_coords().is_empty());
        assert_eq!(instance.metrics().num_glyphs, 2);
        assert_eq!(instance.metrics(), font.metrics());
        assert!(instance.style_metrics().underline.is_some());
        assert_eq!(instance.style_metrics(), font.style_metrics());
    }

    // Type 1 has no named instances, but the overrides still apply.
    #[test]
    fn type1_multiple_master_named_instance_overrides() {
        use crate::ps::type1::multiple_master_tests::{pfa_with_blend, ONE_AXIS};
        let font = Font::new(pfa_with_blend(ONE_AXIS), 0).unwrap();
        let instance = font
            .instance_builder()
            .named_instance_with_overrides(0, [("wght", 900.0)])
            .build();
        assert_eq!(type1_mm_start(&instance), 300.0);
    }

    // The index selects a font where the data can hold several, and is
    // ignored where it cannot.
    #[test]
    fn new_uses_the_index_as_documented() {
        assert!(Font::new(font_test_data::TINOS_SUBSET, 0).is_some());
        assert!(Font::new(font_test_data::TINOS_SUBSET, 1).is_none());
        let count = crate::FileRef::new(font_test_data::ttc::TTC)
            .ok()
            .and_then(|file| match file {
                crate::FileRef::Collection(collection) => Some(collection.len()),
                _ => None,
            })
            .unwrap();
        assert!(count > 1);
        assert!(Font::new(font_test_data::ttc::TTC, count - 1).is_some());
        assert!(Font::new(font_test_data::ttc::TTC, count).is_none());
        let type1 = font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA;
        assert!(Font::new(type1, 7).is_some());
        let sfnt = crate::FontRef::new(font_test_data::NOTO_SANS_JP_CFF).unwrap();
        let cff = sfnt.cff().unwrap().offset_data().as_bytes().to_vec();
        assert!(Font::new(cff.clone(), 0).is_some());
        assert!(Font::new(cff, 1).is_none());
    }

    // A standalone CID-keyed font finds each glyph by the CID its charset
    // gives it.
    #[test]
    fn cid_keyed_fonts_map_cids_to_glyphs() {
        let sfnt = crate::FontRef::new(font_test_data::NOTO_SANS_JP_CFF).unwrap();
        let font = Font::new(sfnt.cff().unwrap().offset_data().as_bytes(), 0).unwrap();
        assert!(font.is_cid_keyed());
        let charset = font.shared().standalone_cff().unwrap().charset().unwrap();
        let max = charset
            .iter()
            .map(|(_, cid)| cid.to_u16() as u32)
            .max()
            .unwrap();
        assert_eq!(font.max_cid(), Some(max));
        for cid in 0..=max + 1 {
            let expected = charset
                .glyph_id(Sid::new(cid as u16))
                .filter(|glyph| glyph.to_u32() < font.num_glyphs());
            assert_eq!(font.glyph_by_cid(cid), expected, "cid {cid}");
        }
        assert_eq!(font.glyph_by_cid(0x10000), None);
    }

    // A later glyph that claims a CID already taken, CID zero included,
    // leaves the first glyph in place.
    #[test]
    fn the_first_glyph_keeps_its_cid() {
        let cids = [(0u16, 0usize), (1, 2), (2, 0), (3, 2), (4, 1)];
        assert_eq!(&*cid_table(|| cids.into_iter()), &[0, 4, 1]);
    }

    #[test]
    fn other_fonts_have_no_cids() {
        for font in [
            // A CID-keyed `CFF` table in an SFNT font.
            Font::new(font_test_data::NOTO_SANS_JP_CFF, 0).unwrap(),
            Font::new(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED, 0).unwrap(),
            Font::new(font_test_data::VAZIRMATN_VAR, 0).unwrap(),
            Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA, 0).unwrap(),
        ] {
            assert!(!font.is_cid_keyed());
            assert_eq!(font.glyph_by_cid(1), None);
            assert_eq!(font.max_cid(), None);
        }
    }

    #[test]
    fn standalone_cff_with_a_bad_private_dict_still_opens() {
        let sfnt = crate::FontRef::new(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED).unwrap();
        let mut raw = sfnt.cff().unwrap().offset_data().as_bytes().to_vec();
        let private = CffFontRef::new_cff(&raw, 0, None)
            .unwrap()
            .sid_private_dict_range()
            .unwrap();
        // A Subrs offset of -1, which no private dict can resolve.
        let damage = [29u8, 0xff, 0xff, 0xff, 0xff, 19];
        assert!(private.len() >= damage.len());
        raw[private.start..private.start + damage.len()].copy_from_slice(&damage);
        let font = Font::new(raw, 0).unwrap();
        let subfonts = font.shared().standalone_cff_subfonts().unwrap();
        assert!(subfonts.len() == 1 && subfonts[0].is_none());
        assert!(font.num_glyphs() > 1);
        // Every glyph of the font is in that one subfont, so none draws or
        // measures.
        let outlines = font.outline_glyphs();
        assert_eq!(outlines.source(), Some(OutlineSource::Cff));
        for glyph in 0..font.num_glyphs() {
            let glyph = GlyphId::new(glyph);
            assert!(outlines.get(glyph).is_none());
            assert_eq!(font.glyph_metrics().h_advance(glyph), 0.0);
            assert!(font.glyph_metrics().extents(glyph).is_none());
        }
    }

    #[test]
    fn standalone_cff_uses_selected_font_data() {
        let sfnt = crate::FontRef::new(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED).unwrap();
        let raw = sfnt.cff().unwrap().offset_data().as_bytes();
        let direct = CffFontRef::new_cff(raw, 0, None).unwrap();
        let font = Font::new(raw.to_vec(), 0).unwrap();
        assert!(matches!(font.kind(), Kind::Cff(_, 0)));
        assert!(Font::new(raw.to_vec(), 1).is_none());
        assert_eq!(font.num_glyphs(), direct.num_glyphs());
        assert_eq!(font.units_per_em(), direct.upem() as u16);
        let bbox = direct.metadata().unwrap().bbox();
        assert_eq!(font.metrics().bounds.x_min, bbox.x_min.to_f48dot16());
        assert_eq!(font.metrics().bounds.y_max, bbox.y_max.to_f48dot16());
        assert_eq!(
            font.style_metrics().italic_angle,
            Some(direct.metadata().unwrap().italic_angle())
        );
        assert_eq!(
            font.shared().standalone_cff_subfonts().unwrap().len(),
            direct.num_subfonts() as usize
        );
        assert!(font.tables().cff().is_err());
        assert!(font.cff().is_none());
        let metadata = direct.metadata().unwrap();
        assert_eq!(metadata.version(), Some("2.9"));
        assert_eq!(
            metadata.notice(),
            Some("Noto is a trademark of Google LLC.")
        );
        assert!(metadata.copyright().unwrap().starts_with("Copyright 2022"));
        for (id, expected) in [
            (types::NameId::COPYRIGHT_NOTICE, metadata.copyright()),
            (types::NameId::FAMILY_NAME, metadata.family_name()),
            (types::NameId::SUBFAMILY_NAME, metadata.weight()),
            (types::NameId::FULL_NAME, metadata.full_name()),
            (types::NameId::VERSION_STRING, metadata.version()),
            (types::NameId::POSTSCRIPT_NAME, metadata.name()),
            (types::NameId::TRADEMARK, metadata.notice()),
        ] {
            let entries: Vec<_> = font
                .names()
                .get(id)
                .map(|name| name.iter().collect())
                .unwrap_or_default();
            assert_eq!(entries.len(), usize::from(expected.is_some()));
            if let Some(expected) = expected {
                assert_eq!(entries[0].name(), expected);
                assert!(entries[0].language().is_none());
                assert!(entries[0].sfnt_record().is_none());
            }
        }
        assert!(font
            .glyph_metrics()
            .extents_exact(GlyphId::new(1))
            .is_some());
        assert_eq!(font.glyph_name(GlyphId::new(1)).unwrap().as_str(), "i");
        #[cfg(feature = "agl")]
        assert_eq!(font.charmap().map_unicode('i'), Some(GlyphId::new(1)));
        let native = font
            .charmap()
            .encodings()
            .find(|encoding| encoding.kind() != super::super::charmap::EncodingKind::Unicode)
            .unwrap();
        // A standalone CFF encoding names no record in either mapping table.
        for encoding in font.charmap().encodings() {
            assert_eq!(encoding.sfnt_index(), None);
            assert_eq!(encoding.sfnt_ids(), None);
        }
        assert_eq!(
            font.charmap().map_code(native, b'i' as u32),
            direct.encoding().unwrap().map(b'i')
        );
        assert!(font
            .charmap()
            .codes(native)
            .any(|(code, glyph)| code == b'i' as u32 && glyph == GlyphId::new(1)));

        let names: Vec<_> = font.glyph_names().collect();
        assert_eq!(names.len(), font.num_glyphs() as usize);
        for (glyph, name) in names.iter().take(8) {
            assert_eq!(font.glyph_name(*glyph).unwrap().as_str(), name.as_str());
            let subfont = direct
                .subfont(direct.subfont_index(*glyph).unwrap(), &[])
                .unwrap();
            let width = direct.evaluate_width(&subfont, *glyph, &[]).unwrap();
            assert_eq!(
                font.glyph_metrics().h_advance_exact(*glyph),
                width.map(|width| width.to_f48dot16()).unwrap_or_default(),
            );
        }
        assert_eq!(
            font.glyph_metrics()
                .h_advance_exact(GlyphId::new(font.num_glyphs())),
            types::F48Dot16::ZERO
        );
        assert_eq!(
            font.default_instance()
                .glyph_name(GlyphId::new(1))
                .unwrap()
                .as_str(),
            font.glyph_name(GlyphId::new(1)).unwrap().as_str()
        );
    }

    #[cfg(feature = "std")]
    #[test]
    fn variation_scalars_are_lazy_bounded_and_shared() {
        let font = Font::new(font_test_data::VAZIRMATN_VAR, 0).unwrap();
        let instance = font
            .instance_builder()
            .normalized_coords([NormalizedCoord::from_f32(-0.75)])
            .build();
        let Repr::Varied(varied) = &instance.0 else {
            panic!("expected a varied instance");
        };
        assert!(varied.sfnt().gvar_scalars.get().is_none());
        assert!(varied.sfnt().hvar_scalars.get().is_none());

        let gvar_scalars = instance.gvar_scalars();
        assert!(!gvar_scalars.is_empty());
        let gvar = instance.selected_gvar().unwrap();
        assert_eq!(
            gvar_scalars.len(),
            (gvar.shared_tuple_count() as usize).min(MAX_PRECOMPUTED_SCALARS)
        );
        assert!(varied.sfnt().hvar_scalars.get().is_none());
        let mut expected = alloc::vec![Fixed::ZERO; gvar_scalars.len()];
        let count = gvar.compute_scalars(instance.normalized_coords(), &mut expected);
        assert_eq!(gvar_scalars, &expected[..count]);

        let hvar_scalars = instance.hvar_scalars();
        assert!(!hvar_scalars.is_empty());
        let hvar = instance.hvar().unwrap();
        let regions = hvar
            .item_variation_store()
            .unwrap()
            .variation_region_list()
            .unwrap();
        assert_eq!(
            hvar_scalars.len(),
            regions
                .variation_regions()
                .len()
                .min(MAX_PRECOMPUTED_SCALARS)
        );
        expected.resize(hvar_scalars.len(), Fixed::ZERO);
        let count = hvar.compute_scalars(instance.normalized_coords(), &mut expected);
        assert_eq!(hvar_scalars, &expected[..count]);
        let clone = instance.clone();
        assert!(core::ptr::eq(gvar_scalars, clone.gvar_scalars()));
        assert!(core::ptr::eq(hvar_scalars, clone.hvar_scalars()));
        assert!(font.gvar_scalars().is_empty());
        assert!(font.hvar_scalars().is_empty());
    }

    #[cfg(feature = "std")]
    #[test]
    fn vvar_scalars_are_lazy_bounded_and_shared() {
        let font = Font::new(font_test_data::MPLUS1CODE_VERTICAL_SUBSET, 0).unwrap();
        let instance = font
            .instance_builder()
            .normalized_coords([NormalizedCoord::from_f32(-0.75)])
            .build();
        let Repr::Varied(varied) = &instance.0 else {
            panic!("expected a varied instance");
        };
        assert!(varied.sfnt().vvar_scalars.get().is_none());

        let scalars = instance.vvar_scalars();
        assert!(!scalars.is_empty());
        let vvar = instance.vvar().unwrap();
        let regions = vvar
            .item_variation_store()
            .unwrap()
            .variation_region_list()
            .unwrap();
        assert_eq!(
            scalars.len(),
            regions
                .variation_regions()
                .len()
                .min(MAX_PRECOMPUTED_SCALARS)
        );
        let mut expected = alloc::vec![Fixed::ZERO; scalars.len()];
        let count = vvar.compute_scalars(instance.normalized_coords(), &mut expected);
        assert_eq!(scalars, &expected[..count]);
        assert!(core::ptr::eq(scalars, instance.clone().vvar_scalars()));
        assert!(font.vvar_scalars().is_empty());
    }

    #[test]
    fn font_outline_context_uses_stated_vertical_metrics() {
        let data = font_test_data::MPLUS1CODE_VERTICAL_SUBSET;
        let font = Font::new(data, 0).unwrap();
        let direct = crate::FontRef::new(data).unwrap();
        let gid = GlyphId::new(1);
        let glyph = direct
            .loca(None)
            .unwrap()
            .get_glyf(gid, &direct.glyf().unwrap())
            .unwrap()
            .unwrap();
        let tsb = direct.vmtx().unwrap().side_bearing(gid).unwrap();
        let context = &font;
        let mut outline = Outline::<Unscaled>::new();
        let loaded = outline.load_with(&context, gid, &Unscaled, None).unwrap();
        assert_eq!(
            loaded.phantom_points()[2].y,
            glyph.y_max() as i32 + tsb as i32
        );
    }

    #[test]
    fn outline_context_uses_unvaried_h_line_metrics() {
        let font = Font::new(MVAR_FONT, 0).unwrap();
        let far = font
            .instance_builder()
            .normalized_coords([NormalizedCoord::from_f32(1.0); 12])
            .build();
        let line = font.shared().metrics().h_line().unwrap();
        assert_ne!(Some(line), font.metrics().typo_line);
        let expected = (line.ascender.to_i32(), line.descender.to_i32());
        assert_ne!(far.metrics().h_line(), font.metrics().h_line());
        assert_eq!((&font).h_line_metrics(), expected);
        assert_eq!((&far).h_line_metrics(), expected);
    }

    #[test]
    fn named_instances() {
        let font = Font::new(font_test_data::CANTARELL_VF_TRIMMED, 0).unwrap();
        let cases = [
            // (named instance index, expected weight value)
            (0, 100.0),
            (1, 300.0),
            (2, 400.0),
            (3, 700.0),
            (4, 800.0),
        ];
        for (index, weight) in cases {
            let named_instance = font.instance_builder().named_instance(index).build();
            let var_instance = font
                .instance_builder()
                .variations([("wght", weight)])
                .build();
            assert_eq!(
                named_instance.normalized_coords(),
                var_instance.normalized_coords(),
                "index={index}"
            );
        }
        // Out of bounds index should give us the default instance.
        let invalid_instance = font.instance_builder().named_instance(5).build();
        assert!(
            invalid_instance.normalized_coords().is_empty(),
            "out of bounds index should give default instance"
        );
    }

    #[test]
    fn named_instance_with_overrides_override_named_value() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let actual = font
            .instance_builder()
            .named_instance_with_overrides(3, [("FILL", 1.0)])
            .build();
        let expected = font
            .instance_builder()
            .variations([
                ("FILL", 1.0),
                ("GRAD", 0.0),
                ("opsz", 24.0),
                ("wght", 400.0),
            ])
            .build();
        assert_eq!(actual.normalized_coords(), expected.normalized_coords());
    }

    #[test]
    fn named_instance_with_overrides_invalid_index_uses_overrides_only() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let actual = font
            .instance_builder()
            .named_instance_with_overrides(999, [("FILL", 1.0), ("ZZZZ", 123.0)])
            .build();
        let expected = font
            .instance_builder()
            .variations([("FILL", 1.0), ("ZZZZ", 123.0)])
            .build();
        assert_eq!(actual.normalized_coords(), expected.normalized_coords());
        assert_eq!(actual.normalized_coords().len(), 4);
    }

    #[test]
    fn named_instance_with_overrides_overwrites_previous_settings() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let actual = font
            .instance_builder()
            .variations([("FILL", 0.0), ("wght", 100.0)])
            .named_instance_with_overrides(5, [("GRAD", -25.0)])
            .build();
        let expected = font
            .instance_builder()
            .variations([
                ("FILL", 0.0),
                ("GRAD", -25.0),
                ("opsz", 24.0),
                ("wght", 600.0),
            ])
            .build();
        assert_eq!(actual.normalized_coords(), expected.normalized_coords());
    }

    #[test]
    fn feature_variations() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let cases = [
            // (fill, [GSUB feature variation index, GPOS feature variation index])
            (0.0, [None, None]),
            (0.5, [None, None]),
            (0.98, [None, None]),
            (0.99, [Some(0), None]),
            (1.0, [Some(0), None]),
        ];
        for (fill, [gsub, gpos]) in cases {
            let instance = font.instance_builder().variations([("FILL", fill)]).build();
            let feature_vars = instance.feature_variations();
            let actual = [feature_vars.gsub, feature_vars.gpos];
            assert_eq!(actual, [gsub, gpos], "fill={fill}");
        }
    }

    #[test]
    fn default_instance_evaluates_and_caches_feature_variations() {
        let source = crate::FontRef::new(font_test_data::MATERIAL_SYMBOLS_SUBSET).unwrap();
        let tables = super::super::TableFunction::new(Arc::new(move |tag| {
            if tag == Tag::new(b"GSUB") {
                // GSUB 1.1 followed by one feature-variation record whose
                // axis-zero range matches exactly the default location.
                return Some(super::super::Blob::from(vec![
                    0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 16,
                    0, 0, 0, 0, 0, 1, 0, 0, 0, 6, 0, 1, 0, 0, 0, 0, 0, 0,
                ]));
            }
            if tag == Tag::new(b"GPOS") {
                return None;
            }
            source
                .data_for_tag(tag)
                .map(|data| super::super::Blob::from(data.as_bytes().to_vec()))
        }));
        let font = Font::new(tables, 0).unwrap();
        assert_eq!(
            font.shared()
                .sfnt()
                .unwrap()
                .default_location
                .feature_vars
                .gsub
                .load(Ordering::Acquire),
            FeatureVarsStorage::UNCHECKED
        );
        let expected = SelectedFeatureVariations {
            gsub: Some(0),
            gpos: None,
            has_lookup_variations: false,
        };
        assert_eq!(font.feature_variations(), expected);
        assert_eq!(
            font.shared()
                .sfnt()
                .unwrap()
                .default_location
                .feature_vars
                .gsub
                .load(Ordering::Acquire),
            0
        );
        assert_eq!(font.default_instance().feature_variations(), expected);
        let default = font
            .instance_builder()
            .normalized_coords([NormalizedCoord::ZERO; 4])
            .build();
        assert!(default.normalized_coords().is_empty());
        assert_eq!(default.feature_variations(), expected);
        let varied = font
            .instance_builder()
            .normalized_coords([NormalizedCoord::from_f32(0.5); 4])
            .build();
        assert_eq!(
            varied.feature_variations(),
            SelectedFeatureVariations::default()
        );
        assert_eq!(font.feature_variations(), expected);
    }

    #[test]
    fn feature_variation_cache_marks_both_absent() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let instance = font.instance_builder().variations([("FILL", 0.5)]).build();
        let storage = instance.feature_vars();
        assert_eq!(
            storage.gsub.load(Ordering::Acquire),
            FeatureVarsStorage::UNCHECKED
        );
        assert_eq!(
            storage.gpos.load(Ordering::Acquire),
            FeatureVarsStorage::UNCHECKED
        );
        assert_eq!(
            instance.feature_variations(),
            SelectedFeatureVariations::default()
        );
        assert_eq!(
            storage.gsub.load(Ordering::Acquire),
            FeatureVarsStorage::ABSENT
        );
        assert_eq!(
            storage.gpos.load(Ordering::Acquire),
            FeatureVarsStorage::ABSENT
        );
    }

    #[test]
    fn feature_variation_cache_is_thread_safe_and_stable() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let instance = font.instance_builder().variations([("FILL", 1.0)]).build();
        std::thread::scope(|scope| {
            for _ in 0..8 {
                scope.spawn(|| {
                    for _ in 0..64 {
                        let vars = instance.feature_variations();
                        assert_eq!(
                            vars,
                            SelectedFeatureVariations {
                                gsub: Some(0),
                                gpos: None,
                                has_lookup_variations: false,
                            }
                        );
                    }
                });
            }
        });
        let storage = instance.feature_vars();
        assert_eq!(storage.gsub.load(Ordering::Acquire), 0);
        assert_eq!(
            storage.gpos.load(Ordering::Acquire),
            FeatureVarsStorage::ABSENT
        );
    }

    #[test]
    fn variations_last_value_wins_and_unknown_axis_ignored() {
        let font = Font::new(font_test_data::CANTARELL_VF_TRIMMED, 0).unwrap();
        let expected = font
            .instance_builder()
            .variations([("wght", 700.0)])
            .build();
        let repeated_axis = font
            .instance_builder()
            .variations([("wght", 100.0), ("wght", 700.0)])
            .build();
        assert_eq!(
            repeated_axis.normalized_coords(),
            expected.normalized_coords()
        );
        let unknown_axis = font
            .instance_builder()
            .variations([("wght", 700.0), ("ZZZZ", 123.0)])
            .build();
        assert_eq!(
            unknown_axis.normalized_coords(),
            expected.normalized_coords()
        );
    }

    #[test]
    fn later_variation_call_overwrites_previous() {
        let font = Font::new(font_test_data::CANTARELL_VF_TRIMMED, 0).unwrap();
        let overwritten = font
            .instance_builder()
            .variations([("wght", 700.0)])
            .variations([("wght", 100.0)])
            .build();
        let expected = font
            .instance_builder()
            .variations([("wght", 100.0)])
            .build();
        assert_eq!(
            overwritten.normalized_coords(),
            expected.normalized_coords()
        );
    }

    #[test]
    fn normalized_coords_empty_resets_to_default_instance() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let instance = font.instance_builder().normalized_coords([]).build();
        assert!(instance.normalized_coords().is_empty());
    }

    #[test]
    fn normalized_coords_truncates_and_pads() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let axis_count = font.tables().fvar().unwrap().axis_count() as usize;
        let values = [0.25, -0.5, 1.0, 0.75, -0.25].map(NormalizedCoord::from_f32);
        let instance = font.instance_builder().normalized_coords(values).build();
        let coords = instance.normalized_coords();
        assert_eq!(coords.len(), axis_count);
        let copied = values.len().min(axis_count);
        assert_eq!(&coords[..copied], &values[..copied]);
        assert!(coords[copied..]
            .iter()
            .all(|&coord| coord == NormalizedCoord::ZERO));
    }

    /// Twelve axes, and an `MVAR` that varies its heights.
    const MVAR_FONT: &[u8] = font_test_data::AMSTELVAR_AVAR2_A;

    #[test]
    fn ordinary_metrics_do_not_read_post() {
        let asked = Arc::new(std::sync::Mutex::new(Vec::new()));
        let logged = asked.clone();
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag| {
            logged.lock().unwrap().push(tag);
            let font = crate::FontRef::new(font_test_data::TINOS_SUBSET).ok()?;
            font.table_data(tag)
                .map(|data| Blob::from(data.as_bytes().to_vec()))
        });
        let font = Font::new(source, 0).unwrap();
        asked.lock().unwrap().clear();
        let _ = font.metrics();
        assert!(!asked.lock().unwrap().contains(&Tag::new(b"post")));
        let _ = font.style_metrics();
        assert!(asked.lock().unwrap().contains(&Tag::new(b"post")));
        assert!(core::ptr::eq(font.style_metrics(), font.style_metrics()));
    }

    #[test]
    fn an_instance_varies_its_metrics() {
        let font = Font::new(MVAR_FONT, 0).unwrap();
        let far = font
            .instance_builder()
            .normalized_coords([NormalizedCoord::from_f32(1.0); 12])
            .build();
        assert!(!far.normalized_coords().is_empty());
        assert_ne!(far.metrics(), font.metrics());
    }

    #[test]
    fn an_instance_varies_and_shares_its_style_metrics() {
        let font = Font::new(MVAR_FONT, 0).unwrap();
        let far = font
            .instance_builder()
            .normalized_coords([NormalizedCoord::from_f32(1.0); 12])
            .build();
        assert_ne!(
            far.style_metrics().strike_through,
            font.style_metrics().strike_through
        );
        assert!(core::ptr::eq(
            far.style_metrics(),
            far.clone().style_metrics()
        ));
    }

    #[test]
    fn an_instance_at_the_default_location_shares_the_font_metrics() {
        // Nothing varies there, so there is no second copy to compute.
        let font = Font::new(MVAR_FONT, 0).unwrap();
        let default = font.instance_builder().build();
        assert!(core::ptr::eq(default.metrics(), font.metrics()));
    }

    #[test]
    fn clones_of_an_instance_share_one_set_of_metrics() {
        let font = Font::new(MVAR_FONT, 0).unwrap();
        let far = font
            .instance_builder()
            .normalized_coords([NormalizedCoord::from_f32(1.0); 12])
            .build();
        let shared = far.clone();
        // The location is resolved once, not once per holder.
        assert!(core::ptr::eq(far.metrics(), shared.metrics()));
    }

    #[test]
    fn the_fixed_numbers_do_not_resolve_a_location() {
        // Both are properties of the font, so reaching them through an
        // instance must not read `MVAR` the way its whole-font metrics do.
        let asked = Arc::new(std::sync::Mutex::new(Vec::new()));
        let logged = asked.clone();
        let source: Arc<dyn Fn(Tag) -> Option<crate::model::Blob> + Send + Sync> =
            Arc::new(move |tag: Tag| {
                logged.lock().unwrap().push(tag);
                let font = crate::FontRef::new(MVAR_FONT).ok()?;
                Some(crate::model::Blob::from(
                    font.table_data(tag)?.as_bytes().to_vec(),
                ))
            });
        let font = Font::new(source, 0).unwrap();
        let far = font
            .instance_builder()
            .normalized_coords([NormalizedCoord::from_f32(1.0); 12])
            .build();
        asked.lock().unwrap().clear();

        assert_eq!(far.num_glyphs(), font.num_glyphs());
        assert_eq!(far.units_per_em(), font.units_per_em());
        assert!(
            !asked.lock().unwrap().contains(&Tag::new(b"MVAR")),
            "reading a fixed number resolved the location"
        );

        // And the varied metrics still do.
        let _ = far.metrics();
        assert!(asked.lock().unwrap().contains(&Tag::new(b"MVAR")));
    }

    #[test]
    fn a_default_instance_is_only_its_font() {
        // Nothing about the default location needs owning, so building one
        // shares the font rather than allocating beside it.
        let font = Font::new(MVAR_FONT, 0).unwrap();
        for instance in [
            font.default_instance(),
            font.instance_builder().build(),
            // An all-default location collapses to no coordinates, so this
            // reaches the same place.
            font.instance_builder()
                .normalized_coords([NormalizedCoord::from_f32(0.0); 12])
                .build(),
        ] {
            assert!(matches!(instance.0, Repr::Default(_)));
            assert!(instance.normalized_coords().is_empty());
            assert!(core::ptr::eq(instance.metrics(), font.metrics()));
        }
    }

    #[test]
    fn a_location_away_from_the_default_is_owned() {
        let font = Font::new(MVAR_FONT, 0).unwrap();
        let far = font
            .instance_builder()
            .normalized_coords([NormalizedCoord::from_f32(1.0); 12])
            .build();
        assert!(matches!(far.0, Repr::Varied(_)));
        assert_eq!(far.normalized_coords().len(), 12);
    }
}
