//! Font representation.

mod blob;
mod cache;
mod format;
mod source;
mod tables;

pub use blob::Blob;
pub use format::Format;
pub use source::Source;
pub use tables::{TableFunction, Tables};

// Do our best to not expose this to users through docs or rust-analyzer.
#[doc(hidden)]
#[rust_analyzer::completions(hidden_from_completion)]
pub mod interop;

use super::charmap::{Charmap, EncodingTables, UnicodeCharmap};
use super::metrics::{empty_glyph_metrics, GlyphMetrics, Metrics, RawGlyphMetrics, StyleMetrics};
use super::name::{self, GlyphName};
use super::once::Once;
use crate::tables::loca::LocaGlyph;
use crate::tables::{
    avar::Avar,
    fvar::Fvar,
    glyf::{outline::OutlineContext, Glyf, PHANTOM_POINT_COUNT},
    gvar::{GlyphVariationData, Gvar},
    hvar::Hvar,
    layout::SelectedFeatureVariations,
    loca::Loca,
    vorg::Vorg,
    vvar::Vvar,
};
use crate::{
    ps::{cff::CffFontRef, type1::Type1Font},
    TableProvider,
};
use alloc::{boxed::Box, sync::Arc, vec::Vec};
use cache::{CffFont, GlyfLoca, GvarTable, HvarTable, TableCache, VorgTable, VvarTable};
use core::{
    any::Any,
    str::FromStr,
    sync::atomic::{self, AtomicU32},
};
use types::{F2Dot14, Fixed, GlyphId, Point, Tag};

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

/// What a location away from the default carries.
struct VariedInstance {
    font: SharedFont,
    /// Never empty: an all-default location is [`Repr::Default`].
    coords: CoordStorage,
    feature_vars: FeatureVarsStorage,
    metrics: Once<Metrics>,
    style_metrics: Once<Box<StyleMetrics>>,
}

impl Font {
    /// Creates a font from the given source and font index, at its default
    /// location.
    ///
    /// The index parameter specifies the desired font in a font collection
    /// (ttc or otc) file. It is ignored if the data source is not a blob.
    ///
    /// Returns `None` if the source holds no font in a supported format.
    pub fn new(source: impl Into<Source>, index: u32) -> Option<Self> {
        Some(Font(Repr::Default(SharedFont::new(source, index)?)))
    }

    /// Returns this font at its default location.
    #[inline]
    pub fn default_instance(&self) -> Font {
        Font(Repr::Default(self.shared().clone()))
    }

    /// Returns a builder for another instance of the same font.
    ///
    /// The two share everything the location does not change, so deriving one
    /// instance from another costs no more than reading the tables once.
    pub fn instance_builder(&self) -> InstanceBuilder {
        InstanceBuilder {
            instance: VariedInstance {
                font: self.shared().clone(),
                coords: CoordStorage::default(),
                feature_vars: FeatureVarsStorage::new(),
                metrics: Once::new(),
                style_metrics: Once::new(),
            },
        }
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

    /// Returns the number of glyphs in the font.
    ///
    /// Fixed for the font: no location varies it.
    #[inline]
    pub fn num_glyphs(&self) -> u32 {
        self.shared().num_glyphs()
    }

    /// Returns the size of the em square, in design units.
    ///
    /// Fixed for the font: no location varies it.
    #[inline]
    pub fn units_per_em(&self) -> u16 {
        self.shared().units_per_em()
    }

    /// Returns an object that provides access to individual font tables.
    ///
    /// For non-SFNT fonts, this will return an empty set of tables.
    #[inline]
    pub fn tables(&self) -> &Tables {
        self.shared().tables()
    }

    /// Returns the font's character mappings.
    pub fn charmap(&self) -> Charmap<'_> {
        Charmap::new(self)
    }

    /// Returns the normalized variation coordinates for this font instance.
    pub fn normalized_coords(&self) -> &[NormalizedCoord] {
        match &self.0 {
            Repr::Default(_) => &[],
            Repr::Varied(varied) => varied.coords.as_slice(),
        }
    }

    /// Returns the metrics describing the font as a whole, at this
    /// instance's location.
    pub fn metrics(&self) -> &Metrics {
        match &self.0 {
            // Nothing varies there, so the font already holds the answer.
            Repr::Default(font) => font.metrics(),
            Repr::Varied(varied) => varied.metrics.get_or_init(|| {
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
            Repr::Varied(varied) => varied
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

    /// Returns the layout feature variations this instance selects.
    pub fn feature_variations(&self) -> SelectedFeatureVariations {
        match &self.0 {
            Repr::Default(_) => SelectedFeatureVariations::default(),
            Repr::Varied(varied) => varied
                .feature_vars
                .load(&varied.font, varied.coords.as_slice()),
        }
    }
}

impl Font {
    pub(crate) fn unicode_charmap(&self) -> Option<&UnicodeCharmap<'_>> {
        self.shared().unicode_charmap()
    }

    pub(crate) fn encoding_tables(&self) -> Option<&EncodingTables<'_>> {
        self.shared().encodings()
    }

    /// The state every instance of this font shares.
    fn shared(&self) -> &SharedFont {
        match &self.0 {
            Repr::Default(font) => font,
            Repr::Varied(varied) => &varied.font,
        }
    }

    /// Returns what `hmtx` states.
    #[inline]
    pub(crate) fn h_metrics(&self) -> &RawGlyphMetrics<'_> {
        self.shared().h_metrics()
    }

    /// Returns what `vmtx` states.
    #[inline]
    pub(crate) fn v_metrics(&self) -> &RawGlyphMetrics<'_> {
        self.shared().v_metrics()
    }

    /// Returns the outline tables.
    #[inline]
    pub(crate) fn glyf_loca(&self) -> Option<&(Glyf<'_>, Loca<'_>)> {
        self.shared().glyf_loca()
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

    /// Returns `gvar`.
    #[inline]
    pub(crate) fn gvar(&self) -> Option<&Gvar<'_>> {
        self.shared().gvar()
    }

    /// Returns the charstring outlines.
    #[inline]
    pub(crate) fn cff(&self) -> Option<&CffFontRef<'_>> {
        self.shared().cff()
    }

    /// Returns `VORG`.
    #[inline]
    pub(crate) fn vorg(&self) -> Option<&Vorg<'_>> {
        self.shared().vorg()
    }
}

impl<'a> OutlineContext<'a> for &'a Font {
    fn glyph(&self, glyph: GlyphId) -> Option<LocaGlyph<'a>> {
        let (glyf, loca) = self.glyf_loca()?;
        loca.get(glyph, glyf)
    }

    fn glyph_variation_data(&self, glyph: GlyphId) -> Option<GlyphVariationData<'a>> {
        self.gvar()?.glyph_variation_data(glyph).ok()?
    }

    fn has_gvar(&self) -> bool {
        self.gvar().is_some()
    }

    fn phantom_point_deltas(&self, glyph: GlyphId) -> Option<[Point<Fixed>; PHANTOM_POINT_COUNT]> {
        let (glyf, loca) = self.glyf_loca()?;
        self.gvar()?
            .phantom_point_deltas(glyf, loca, self.normalized_coords(), glyph)
    }

    fn coords(&self) -> &[F2Dot14] {
        self.normalized_coords()
    }

    fn units_per_em(&self) -> u16 {
        Font::units_per_em(self)
    }

    fn h_metrics(&self, glyph: GlyphId) -> (i32, i32) {
        let metrics = Font::h_metrics(self);
        (
            metrics
                .stated_side_bearing(glyph)
                .unwrap_or_default()
                .to_i32(),
            metrics.stored_advance(self.num_glyphs(), glyph).to_i32(),
        )
    }

    fn v_metrics(&self, glyph: GlyphId) -> Option<(i32, i32)> {
        let metrics = Font::v_metrics(self);
        if metrics.is_empty() {
            return None;
        }
        Some((
            metrics
                .stated_side_bearing(glyph)
                .unwrap_or_default()
                .to_i32(),
            metrics.stored_advance(self.num_glyphs(), glyph).to_i32(),
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

#[cfg(test)]
impl Font {
    /// The storage behind a varied instance, for the cache tests.
    fn feature_vars(&self) -> &FeatureVarsStorage {
        match &self.0 {
            Repr::Varied(varied) => &varied.feature_vars,
            Repr::Default(_) => panic!("the default instance keeps no storage of its own"),
        }
    }
}

/// Builder for configuring a font instance.
pub struct InstanceBuilder {
    instance: VariedInstance,
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
    pub fn named_instance(mut self, index: usize) -> Self {
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
    pub fn named_instance_with_overrides<V>(mut self, index: usize, overrides: V) -> Self
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
        if self.instance.coords.as_slice().is_empty() {
            Font(Repr::Default(self.instance.font))
        } else {
            Font(Repr::Varied(Arc::new(self.instance)))
        }
    }
}

impl InstanceBuilder {
    fn set_variations<V>(&mut self, variations: V)
    where
        V: IntoIterator,
        V::Item: Into<Variation>,
    {
        let tables = self.instance.font.tables();
        if let Ok(fvar) = tables.fvar() {
            set_variations(
                &fvar,
                tables.avar().ok(),
                &mut self.instance.coords,
                variations,
            );
        } else {
            self.instance.coords.resize(0);
        }
    }

    fn set_coords(&mut self, coords: impl IntoIterator<Item = NormalizedCoord>) {
        if let Ok(fvar) = self.instance.font.tables().fvar() {
            let count = fvar.axis_count() as usize;
            self.instance.coords.resize(count);
            for (dst, src) in self.instance.coords.as_mut_slice().iter_mut().zip(
                coords
                    .into_iter()
                    .chain(core::iter::repeat(NormalizedCoord::ZERO)),
            ) {
                *dst = src;
            }
            self.instance.coords.clear_if_all_zeroes();
        } else {
            self.instance.coords.resize(0);
        }
    }

    fn set_named_instance(&mut self, index: usize) {
        let tables = self.instance.font.tables();
        if let Ok(fvar) = tables.fvar() {
            set_variations(
                &fvar,
                tables.avar().ok(),
                &mut self.instance.coords,
                named_instance_variations(&fvar, index),
            );
        } else {
            self.instance.coords.resize(0);
        }
    }

    fn set_named_instance_with_overrides<V>(&mut self, index: usize, overrides: V)
    where
        V: IntoIterator,
        V::Item: Into<Variation>,
    {
        let tables = self.instance.font.tables();
        if let Ok(fvar) = tables.fvar() {
            set_variations(
                &fvar,
                tables.avar().ok(),
                &mut self.instance.coords,
                named_instance_variations(&fvar, index)
                    .chain(overrides.into_iter().map(Into::into)),
            );
        } else {
            self.instance.coords.resize(0);
        }
    }
}

// Helper to extract an iterator of Variation from a named instance index.
fn named_instance_variations<'a>(
    fvar: &'a Fvar,
    index: usize,
) -> impl Iterator<Item = Variation> + 'a {
    fvar.axis_instance_arrays()
        .ok()
        .and_then(|arrays| {
            let axes = arrays.axes();
            arrays.instances().get(index).ok().map(|instance| {
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
    coords.resize(fvar.axis_count() as usize);
    fvar.user_to_normalized(
        avar.as_ref(),
        variations
            .into_iter()
            .map(Into::into)
            .map(|var| (var.tag, Fixed::from_f64(var.value as _))),
        coords.as_mut_slice(),
    );
    coords.clear_if_all_zeroes();
}

/// A normalized variation coordinate in 2.14 fixed point in the range
/// [-1.0, 1.0].
pub type NormalizedCoord = types::F2Dot14;

/// A variation setting for a font instance.
///
/// The tag identifies the axis, and the value is the desired value for that
/// axis in user space.
#[derive(Copy, Clone, PartialEq, Debug)]
pub struct Variation {
    /// The tag that identifies the axis.
    pub tag: Tag,
    /// The value for the axis in user space.
    pub value: f32,
}

impl Variation {
    /// Creates a new font variation with the given tag and value.
    pub fn new(tag: Tag, value: f32) -> Self {
        Self { tag, value }
    }
}

// Various conversions for Variation that have proven to be ergonomically
// useful in practice. These allow, for example, passing &[("wght", 700.0)]
// directly to the variations() method of InstanceBuilder without needing
//to manually construct Variation objects or tags.

impl From<&'_ Variation> for Variation {
    fn from(value: &'_ Variation) -> Self {
        *value
    }
}

impl From<(Tag, f32)> for Variation {
    fn from(value: (Tag, f32)) -> Self {
        Self::new(value.0, value.1)
    }
}

impl From<&(Tag, f32)> for Variation {
    fn from(value: &(Tag, f32)) -> Self {
        Self::new(value.0, value.1)
    }
}

impl From<(&str, f32)> for Variation {
    fn from(value: (&str, f32)) -> Self {
        Self::new(Tag::from_str(value.0).unwrap_or_default(), value.1)
    }
}

impl From<&(&str, f32)> for Variation {
    fn from(value: &(&str, f32)) -> Self {
        Self::new(Tag::from_str(value.0).unwrap_or_default(), value.1)
    }
}

/// Maximum number of coordinates we store inline. Chosen to maximize
/// number of coords while minimizing space overhead.
const MAX_INLINE_COORDS: usize = 15;

enum CoordStorage {
    None,
    Inline([NormalizedCoord; MAX_INLINE_COORDS], u8),
    Heap(Vec<NormalizedCoord>),
}

impl Default for CoordStorage {
    fn default() -> Self {
        Self::None
    }
}

impl CoordStorage {
    /// Empty storage if all the coordinates are zeros. This allows us to
    /// bypass variation processing for the default instance with a simple
    /// is_empty() check.
    fn clear_if_all_zeroes(&mut self) {
        match self {
            Self::None => {}
            Self::Inline(coords, len) => {
                if coords[..*len as usize]
                    .iter()
                    .all(|&c| c == NormalizedCoord::ZERO)
                {
                    *len = 0;
                }
            }
            Self::Heap(heap) => {
                if heap.iter().all(|&c| c == NormalizedCoord::ZERO) {
                    heap.clear();
                }
            }
        }
    }

    fn resize(&mut self, new_len: usize) {
        match self {
            Self::None => {
                if new_len > MAX_INLINE_COORDS {
                    let mut heap = Vec::with_capacity(new_len);
                    heap.resize(new_len, NormalizedCoord::ZERO);
                    *self = Self::Heap(heap);
                } else {
                    *self = Self::Inline([NormalizedCoord::ZERO; MAX_INLINE_COORDS], new_len as u8);
                }
            }
            Self::Inline(_, len) => {
                if new_len > MAX_INLINE_COORDS {
                    let mut heap = Vec::with_capacity(new_len);
                    heap.resize(new_len, NormalizedCoord::ZERO);
                    *self = Self::Heap(heap);
                } else {
                    *len = new_len as u8;
                }
            }
            Self::Heap(heap) => {
                heap.resize(new_len, NormalizedCoord::ZERO);
            }
        }
    }

    fn as_slice(&self) -> &[NormalizedCoord] {
        match self {
            Self::None => &[],
            Self::Inline(coords, len) => &coords[..*len as usize],
            Self::Heap(heap) => heap.as_slice(),
        }
    }

    fn as_mut_slice(&mut self) -> &mut [NormalizedCoord] {
        match self {
            Self::None => &mut [],
            Self::Inline(coords, len) => &mut coords[..*len as usize],
            Self::Heap(heap) => heap.as_mut_slice(),
        }
    }
}

/// Lazy atomic storage for feature variation selections.
///
/// We don't want to load the GSUB and GPOS tables unless explicitly requested.
struct FeatureVarsStorage {
    gsub: AtomicU32,
    gpos: AtomicU32,
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
            };
        }
        // Two threads that race here select the same indices, so the second
        // store writes what the first did.
        let selected = SelectedFeatureVariations::new(&font.tables(), coords);
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

/// The state every instance of a font shares.
///
/// Reference counted internally: cloning it costs no more than the count,
/// and it is thread safe.
#[derive(Clone)]
struct SharedFont(Arc<SharedFontRepr>);

impl SharedFont {
    /// Creates a new font from the given source and font index.
    ///
    /// The index parameter specifies the desired font in a font collection
    /// (ttc or otc) file. It is ignored if the data source is not a blob.
    ///
    /// Returns `None` if the source holds no font in a supported format.
    fn new(source: impl Into<Source>, index: u32) -> Option<Self> {
        let source = source.into();
        let kind = if let Ok(tables) = Tables::new(source.clone(), index) {
            Some(KindRepr::Sfnt(Arc::new(tables), index))
        } else if let Source::Blob(blob) = &source {
            match Format::new(blob) {
                Some(Format::Type1) => Type1Font::new(blob)
                    .ok()
                    .map(|font| KindRepr::Type1(Box::new(font))),
                // TODO: pure CFF fonts
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
            metrics: Once::new(),
            style_metrics: Once::new(),
            h_metrics: Once::new(),
            v_metrics: Once::new(),
            glyf_loca: Once::new(),
            hvar: Once::new(),
            vvar: Once::new(),
            gvar: Once::new(),
            cff: Once::new(),
            vorg: Once::new(),
            charmap: Once::new(),
            encodings: Once::new(),
        };
        Some(Self(Arc::new(repr)))
    }

    /// Returns the underlying source of font data.
    fn source(&self) -> &Source {
        &self.0.source
    }

    /// Returns the underlying kind of the font.
    fn kind(&self) -> Kind<'_> {
        match &self.0.kind {
            KindRepr::Sfnt(tables, index) => Kind::Sfnt(tables, *index),
            KindRepr::Type1(font) => Kind::Type1(font),
        }
    }

    /// Returns the metrics describing the font as a whole, at its default
    /// location.
    #[inline]
    fn metrics(&self) -> &Metrics {
        self.0.metrics.get_or_init(|| match self.kind() {
            Kind::Type1(font) => Metrics::from_type1(font),
            _ => Metrics::from_sfnt(&self.tables(), &[]),
        })
    }

    /// Returns the style measurements at the font's default location.
    fn style_metrics(&self) -> &StyleMetrics {
        self.0
            .style_metrics
            .get_or_init(|| {
                Box::new(match self.kind() {
                    Kind::Type1(font) => StyleMetrics::from_type1(font),
                    _ => StyleMetrics::from_sfnt(&self.tables(), &[]),
                })
            })
            .as_ref()
    }

    /// Returns the number of glyphs in the font.
    ///
    /// Fixed for the font: no location varies it.
    #[inline]
    fn num_glyphs(&self) -> u32 {
        self.metrics().num_glyphs
    }

    /// Returns the size of the em square, in design units.
    ///
    /// Fixed for the font: no location varies it.
    #[inline]
    fn units_per_em(&self) -> u16 {
        self.metrics().units_per_em
    }

    /// Returns the tables behind this font, for a cache that holds them.
    fn tables_arc(&self) -> Option<&Arc<Tables>> {
        match &self.0.kind {
            KindRepr::Sfnt(tables, _) => Some(tables),
            _ => None,
        }
    }

    /// Returns the selected character maps, parsed once for the font.
    fn unicode_charmap(&self) -> Option<&UnicodeCharmap<'_>> {
        let tables = self.tables_arc()?;
        Some(
            self.0
                .charmap
                .get_or_init(|| {
                    TableCache::read(tables.clone(), |tables| UnicodeCharmap::read(&tables))
                })
                .get(),
        )
    }

    /// Returns all selectable cmap subtables, parsed on first use.
    fn encodings(&self) -> Option<&EncodingTables<'_>> {
        let tables = self.tables_arc()?;
        Some(
            self.0
                .encodings
                .get_or_init(|| {
                    Box::new(TableCache::read(tables.clone(), |tables| {
                        EncodingTables::read(&tables)
                    }))
                })
                .get(),
        )
    }

    /// Returns what `hmtx` states, parsed once for the font.
    #[inline]
    fn h_metrics(&self) -> &RawGlyphMetrics<'_> {
        let Some(tables) = self.tables_arc() else {
            return empty_glyph_metrics();
        };
        self.0
            .h_metrics
            .get_or_init(|| {
                TableCache::read(tables.clone(), |tables| RawGlyphMetrics::from_hmtx(&tables))
            })
            .get()
    }

    /// Returns what `vmtx` states, parsed once for the font.
    ///
    /// Read on demand, unlike `hmtx`: most text is horizontal and never
    /// asks.
    #[inline]
    fn v_metrics(&self) -> &RawGlyphMetrics<'_> {
        let Some(tables) = self.tables_arc() else {
            return empty_glyph_metrics();
        };
        self.0
            .v_metrics
            .get_or_init(|| {
                TableCache::read(tables.clone(), |tables| RawGlyphMetrics::from_vmtx(&tables))
            })
            .get()
    }

    /// Returns the outline tables, parsed once for the font.
    ///
    /// Outlines and metrics in both directions read these, so they are
    /// shared rather than parsed by each.
    #[inline]
    fn glyf_loca(&self) -> Option<&(Glyf<'_>, Loca<'_>)> {
        let tables = self.tables_arc()?;
        self.0
            .glyf_loca
            .get_or_init(|| TableCache::read(tables.clone(), |tables| GlyfLoca::read(&tables)))
            .get()
            .0
            .as_ref()
    }

    /// Returns `HVAR`, parsed once for the font.
    #[inline]
    fn hvar(&self) -> Option<&Hvar<'_>> {
        let tables = self.tables_arc()?;
        self.0
            .hvar
            .get_or_init(|| TableCache::read(tables.clone(), |tables| HvarTable::read(&tables)))
            .get()
            .0
            .as_ref()
    }

    /// Returns `VVAR`, parsed once for the font.
    #[inline]
    fn vvar(&self) -> Option<&Vvar<'_>> {
        let tables = self.tables_arc()?;
        self.0
            .vvar
            .get_or_init(|| TableCache::read(tables.clone(), |tables| VvarTable::read(&tables)))
            .get()
            .0
            .as_ref()
    }

    /// Returns `gvar`, parsed once for the font.
    ///
    /// Typically about half a variable font, so nothing should ask for this
    /// that another table can answer.
    #[inline]
    fn gvar(&self) -> Option<&Gvar<'_>> {
        let tables = self.tables_arc()?;
        self.0
            .gvar
            .get_or_init(|| TableCache::read(tables.clone(), |tables| GvarTable::read(&tables)))
            .get()
            .0
            .as_ref()
    }

    /// Returns `VORG`, parsed once for the font.
    #[inline]
    fn vorg(&self) -> Option<&Vorg<'_>> {
        let tables = self.tables_arc()?;
        self.0
            .vorg
            .get_or_init(|| TableCache::read(tables.clone(), |tables| VorgTable::read(&tables)))
            .get()
            .0
            .as_ref()
    }

    /// Returns the charstring outlines, parsed once for the font.
    ///
    /// Reading one means running it, so nothing here is parsed per glyph
    /// beyond what a charstring itself needs.
    #[inline]
    fn cff(&self) -> Option<&CffFontRef<'_>> {
        let tables = self.tables_arc()?;
        self.0
            .cff
            .get_or_init(|| TableCache::read(tables.clone(), |tables| CffFont::read(&tables)))
            .get()
            .0
            .as_ref()
    }

    /// Returns an object that provides access to individual font tables.
    ///
    /// For non-SFNT fonts, this will return an empty set of tables.
    fn tables(&self) -> &Tables {
        if let KindRepr::Sfnt(tables, _) = &self.0.kind {
            tables
        } else {
            &tables::EMPTY_FONT_TABLES
        }
    }
}

struct SharedFontRepr {
    source: Source,
    kind: KindRepr,
    // Storage cell for lazily loaded HarfRust shaping data.
    shaping_data: Once<Box<dyn Any + Send + Sync>>,
    // Metrics that describe the font as a whole, at the default location,
    // read once rather than per query. Kept apart from `shaping_data`, which
    // holds one thing for one owner.
    metrics: Once<Metrics>,
    style_metrics: Once<Box<StyleMetrics>>,
    // What `hmtx` states, parsed once for the font. Held beside the tables
    // it borrows, which is what lets it live here at all.
    h_metrics: Once<TableCache<RawGlyphMetrics<'static>>>,
    /// `vmtx`, read only when something measures vertically.
    v_metrics: Once<TableCache<RawGlyphMetrics<'static>>>,
    // `HVAR` states the deltas a location makes to a metric outright, and
    // `gvar` states them as phantom points on an outline, which `glyf` and
    // `loca` are read to reach and which outlines will read for their own
    // sake. None of the three depends on a location to parse, so every
    // instance shares them, and only an instance asks for them: a font read
    // at its default location touches none of them.
    glyf_loca: Once<TableCache<GlyfLoca<'static>>>,
    hvar: Once<TableCache<HvarTable<'static>>>,
    vvar: Once<TableCache<VvarTable<'static>>>,
    gvar: Once<TableCache<GvarTable<'static>>>,
    // The charstrings of a `CFF` or `CFF2` font, which neither metrics nor
    // outlines read unless the font states them this way.
    cff: Once<TableCache<CffFont<'static>>>,
    // Where a glyph's vertical origin sits, read only by vertical text.
    vorg: Once<TableCache<VorgTable<'static>>>,
    // Selected Unicode cmap subtables and their encoding metadata.
    charmap: Once<TableCache<UnicodeCharmap<'static>>>,
    // Only clients selecting a charmap pay for the dense record array.
    encodings: Once<Box<TableCache<EncodingTables<'static>>>>,
}

/// The underlying type of a font.
#[derive(Clone)]
pub enum Kind<'a> {
    /// An SFNT-based font represented by a set of tables and an index.
    Sfnt(&'a Tables, u32),
    /// An Adobe Type1 font.
    Type1(&'a Type1Font),
    /// A CFF font with an associated index.
    Cff(CffFontRef<'a>, u32),
}

/// The underlying type of a font.
enum KindRepr {
    Sfnt(Arc<Tables>, u32),
    // Boxed: a `Type1Font` is an order of magnitude larger than the sfnt
    // variant, and inline it would be paid by every font that is not one.
    Type1(Box<Type1Font>),
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::tables::glyf::outline::{Outline, Unscaled};
    use core::sync::atomic::Ordering;

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
                                gpos: None
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
            far.style_metrics().strikethrough,
            font.style_metrics().strikethrough
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
