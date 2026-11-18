//! Glyph representations: outlines, bitmaps and color.
//!
//! A font can contain a glyph in several forms. [`GlyphSource`] identifies
//! the tables that provide them, and [`GlyphSourceOrder`] sets which ones to
//! prefer. Collections and glyphs report a narrower type,
//! [`outline::OutlineSource`] or [`bitmap::BitmapSource`], and each converts
//! into a [`GlyphSource`].
//!
//! The collections [`outline::OutlineGlyphs`] and [`bitmap::BitmapStrikes`]
//! resolve an order against a font once, and [`color::ColorGlyphs`] reads
//! `COLR`. Glyph metrics use an order to measure ink.

pub mod bitmap;
pub mod color;
pub mod outline;

/// A table that can provide a glyph.
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[repr(u8)]
pub enum GlyphSource {
    /// PNG bitmaps in `sbix`.
    Sbix = 1,
    /// Color bitmaps in `CBDT`.
    Cbdt,
    /// Monochrome and grayscale bitmaps in `EBDT`.
    Ebdt,
    /// Layered color glyphs in `COLR`, version 0 or 1.
    Colr,
    /// Variable composites in `VARC`.
    ///
    /// The components are drawn from one of the outline sources below.
    Varc,
    /// TrueType outlines in `glyf`.
    Glyf,
    /// Variable PostScript outlines in `CFF2`.
    Cff2,
    /// PostScript outlines in `CFF`.
    Cff,
    /// Charstrings of a Type 1 font.
    Type1,
}

impl GlyphSource {
    const ALL: [Self; 9] = [
        Self::Sbix,
        Self::Cbdt,
        Self::Ebdt,
        Self::Colr,
        Self::Varc,
        Self::Glyf,
        Self::Cff2,
        Self::Cff,
        Self::Type1,
    ];

    /// Returns the source for a packed nibble.
    const fn from_nibble(nibble: u8) -> Option<Self> {
        let index = nibble as usize;
        if index == 0 || index > Self::ALL.len() {
            None
        } else {
            Some(Self::ALL[index - 1])
        }
    }
}

/// An ordered list of glyph sources, most preferred first.
///
/// Holds every source, four bits each, in eight bytes. It can be used as a
/// constant and passed by value. A source listed more than once keeps only
/// its first position.
///
/// How an order is used depends on the consumer:
///
/// - Outlines use one base source per font. This is the first of `Glyf`,
///   `Cff2`, `Cff` and `Type1` that the font contains. If `Varc` comes before
///   the base source, it applies to the glyphs it covers, and its components
///   are drawn from the base source.
/// - Bitmaps use the first of `Sbix`, `Cbdt` and `Ebdt` that the font
///   contains.
/// - Glyph extents try each source in the font, in order, until one provides
///   extents for the glyph.
#[derive(Copy, Clone, PartialEq, Eq, Hash)]
pub struct GlyphSourceOrder(u64);

impl GlyphSourceOrder {
    /// The default order for measuring glyphs: `sbix`, `CBDT`, `COLR`,
    /// `VARC`, `glyf`, `CFF2`, `CFF`, then Type 1 charstrings.
    ///
    /// Color bitmaps come first, then color glyphs, then outlines with
    /// `VARC` before its base source. `EBDT` is not included, so monochrome
    /// and grayscale bitmaps are used only when a caller asks for them.
    pub const DEFAULT: Self = Self::new(&[
        GlyphSource::Sbix,
        GlyphSource::Cbdt,
        GlyphSource::Colr,
        GlyphSource::Varc,
        GlyphSource::Glyf,
        GlyphSource::Cff2,
        GlyphSource::Cff,
        GlyphSource::Type1,
    ]);

    /// Every source: `sbix`, `CBDT`, `EBDT`, `COLR`, `VARC`, `glyf`, `CFF2`,
    /// `CFF`, then Type 1 charstrings.
    ///
    /// The order of [`DEFAULT`](Self::DEFAULT), with `EBDT` after the color
    /// bitmaps.
    pub const ALL: Self = Self::new(&[
        GlyphSource::Sbix,
        GlyphSource::Cbdt,
        GlyphSource::Ebdt,
        GlyphSource::Colr,
        GlyphSource::Varc,
        GlyphSource::Glyf,
        GlyphSource::Cff2,
        GlyphSource::Cff,
        GlyphSource::Type1,
    ]);

    /// The outline sources: `VARC`, `glyf`, `CFF2`, `CFF`, then Type 1
    /// charstrings.
    pub const OUTLINES: Self = Self::new(&[
        GlyphSource::Varc,
        GlyphSource::Glyf,
        GlyphSource::Cff2,
        GlyphSource::Cff,
        GlyphSource::Type1,
    ]);

    /// The outline sources without `VARC`: `glyf`, `CFF2`, `CFF`, then Type
    /// 1 charstrings.
    ///
    /// Every glyph is drawn from the base source.
    pub const BASE_OUTLINES: Self = Self::new(&[
        GlyphSource::Glyf,
        GlyphSource::Cff2,
        GlyphSource::Cff,
        GlyphSource::Type1,
    ]);

    /// The bitmap sources: `sbix`, `CBDT`, then `EBDT`.
    pub const BITMAPS: Self = Self::new(&[GlyphSource::Sbix, GlyphSource::Cbdt, GlyphSource::Ebdt]);

    /// The color bitmap sources: `sbix`, then `CBDT`.
    pub const COLOR_BITMAPS: Self = Self::new(&[GlyphSource::Sbix, GlyphSource::Cbdt]);

    /// The monochrome and grayscale bitmap source: `EBDT`.
    pub const MONO_BITMAPS: Self = Self::new(&[GlyphSource::Ebdt]);

    /// The color glyph source: `COLR`.
    pub const COLOR: Self = Self::new(&[GlyphSource::Colr]);

    /// No sources.
    pub const EMPTY: Self = Self(0);

    /// The most sources an order holds: each one at most once.
    pub(crate) const MAX_LEN: usize = GlyphSource::ALL.len();

    /// Creates an order from `sources`.
    ///
    /// A source listed more than once keeps only its first position.
    pub const fn new(sources: &[GlyphSource]) -> Self {
        let mut packed = 0u64;
        let mut len = 0;
        let mut i = 0;
        while i < sources.len() && len < Self::MAX_LEN {
            let source = sources[i];
            i += 1;
            if Self(packed).contains(source) {
                continue;
            }
            packed |= (source as u64) << (len * 4);
            len += 1;
        }
        Self(packed)
    }

    /// Returns `true` if the order contains `source`.
    pub const fn contains(self, source: GlyphSource) -> bool {
        let mut packed = self.0;
        while packed != 0 {
            if (packed & 0xF) as u8 == source as u8 {
                return true;
            }
            packed >>= 4;
        }
        false
    }

    /// Returns `true` if the order contains no sources.
    pub const fn is_empty(self) -> bool {
        self.0 == 0
    }

    /// Returns an iterator over the sources, most preferred first.
    pub fn iter(self) -> impl Iterator<Item = GlyphSource> {
        let mut packed = self.0;
        core::iter::from_fn(move || {
            let source = GlyphSource::from_nibble((packed & 0xF) as u8)?;
            packed >>= 4;
            Some(source)
        })
    }
}

impl Default for GlyphSourceOrder {
    fn default() -> Self {
        Self::DEFAULT
    }
}

impl core::fmt::Debug for GlyphSourceOrder {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_list().entries(self.iter()).finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use alloc::vec::Vec;
    use bitmap::BitmapSource;
    use outline::OutlineSource;

    // Each source converts to the narrow type of its kind and back, and to
    // no other.
    #[test]
    fn narrow_sources_round_trip() {
        for source in GlyphSource::ALL {
            let outline = OutlineSource::try_from(source);
            let bitmap = BitmapSource::try_from(source);
            assert_eq!(outline.map(GlyphSource::from).unwrap_or(source), source);
            assert_eq!(bitmap.map(GlyphSource::from).unwrap_or(source), source);
            let kinds = usize::from(outline.is_ok()) + usize::from(bitmap.is_ok());
            assert_eq!(
                kinds,
                usize::from(source != GlyphSource::Colr),
                "{source:?}"
            );
        }
    }

    // The named orders hold the sources their documentation lists.
    #[test]
    fn named_orders_hold_their_sources() {
        use GlyphSource::*;
        let sources = |order: GlyphSourceOrder| order.iter().collect::<Vec<_>>();
        assert_eq!(sources(GlyphSourceOrder::ALL), GlyphSource::ALL);
        assert_eq!(
            sources(GlyphSourceOrder::DEFAULT),
            [Sbix, Cbdt, Colr, Varc, Glyf, Cff2, Cff, Type1]
        );
        assert_eq!(sources(GlyphSourceOrder::BITMAPS), [Sbix, Cbdt, Ebdt]);
        assert_eq!(sources(GlyphSourceOrder::COLOR_BITMAPS), [Sbix, Cbdt]);
        assert_eq!(sources(GlyphSourceOrder::MONO_BITMAPS), [Ebdt]);
        assert_eq!(sources(GlyphSourceOrder::COLOR), [Colr]);
        assert_eq!(
            sources(GlyphSourceOrder::OUTLINES),
            [Varc, Glyf, Cff2, Cff, Type1]
        );
        assert_eq!(
            sources(GlyphSourceOrder::BASE_OUTLINES),
            [Glyf, Cff2, Cff, Type1]
        );
        assert!(GlyphSourceOrder::EMPTY.is_empty());
        assert_eq!(
            alloc::format!("{:?}", GlyphSourceOrder::COLOR_BITMAPS),
            "[Sbix, Cbdt]"
        );
    }

    #[test]
    fn order_keeps_first_position_of_each_source() {
        let order = GlyphSourceOrder::new(&[
            GlyphSource::Glyf,
            GlyphSource::Varc,
            GlyphSource::Glyf,
            GlyphSource::Cff,
        ]);
        let sources: Vec<_> = order.iter().collect();
        assert_eq!(
            sources,
            [GlyphSource::Glyf, GlyphSource::Varc, GlyphSource::Cff]
        );
        assert!(order.contains(GlyphSource::Cff));
        assert!(!order.contains(GlyphSource::Cff2));
    }

    #[test]
    fn default_order_is_the_harfbuzz_extents_chain() {
        let sources: Vec<_> = GlyphSourceOrder::DEFAULT.iter().collect();
        assert_eq!(
            sources,
            [
                GlyphSource::Sbix,
                GlyphSource::Cbdt,
                GlyphSource::Colr,
                GlyphSource::Varc,
                GlyphSource::Glyf,
                GlyphSource::Cff2,
                GlyphSource::Cff,
                GlyphSource::Type1,
            ]
        );
        assert!(!GlyphSourceOrder::DEFAULT.contains(GlyphSource::Ebdt));
    }

    #[test]
    fn empty_order_has_no_sources() {
        assert!(GlyphSourceOrder::EMPTY.is_empty());
        assert_eq!(GlyphSourceOrder::EMPTY.iter().count(), 0);
        assert!(!GlyphSourceOrder::new(&[]).contains(GlyphSource::Glyf));
    }

    #[test]
    fn every_source_round_trips_through_an_order() {
        let order = GlyphSourceOrder::new(&GlyphSource::ALL);
        let sources: Vec<_> = order.iter().collect();
        assert_eq!(sources, GlyphSource::ALL);
    }
}
