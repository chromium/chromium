//! Embedded bitmap glyphs.
//!
//! A font stores its bitmaps in strikes, one per size. [`BitmapStrikes`]
//! reads them from the first of `sbix`, `CBDT` and `EBDT` that a
//! [`GlyphSourceOrder`] prefers. It selects a [`Strike`] for a size, and a
//! strike returns a [`BitmapGlyph`]. The pixels of a
//! [`BitmapGlyph`] can be read without knowing how the table stores them.

use crate::{
    model::{
        glyph::{GlyphSource, GlyphSourceOrder},
        metrics::GlyphExtents,
        Font,
    },
    tables::{
        bitmap::{self as bdt, BitmapSize},
        cbdt::Cbdt,
        cblc::Cblc,
        ebdt::Ebdt,
        eblc::Eblc,
        sbix::{self, Sbix},
    },
    types::{GlyphId, Tag},
    FontData,
};

/// A table that provides bitmaps.
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub enum BitmapSource {
    /// PNG bitmaps in `sbix`.
    Sbix,
    /// Color bitmaps in `CBDT`.
    Cbdt,
    /// Monochrome and grayscale bitmaps in `EBDT`.
    Ebdt,
}

impl From<BitmapSource> for GlyphSource {
    fn from(source: BitmapSource) -> Self {
        match source {
            BitmapSource::Sbix => Self::Sbix,
            BitmapSource::Cbdt => Self::Cbdt,
            BitmapSource::Ebdt => Self::Ebdt,
        }
    }
}

impl TryFrom<GlyphSource> for BitmapSource {
    /// The original source, which contains no bitmaps.
    type Error = GlyphSource;

    fn try_from(source: GlyphSource) -> Result<Self, GlyphSource> {
        Ok(match source {
            GlyphSource::Sbix => Self::Sbix,
            GlyphSource::Cbdt => Self::Cbdt,
            GlyphSource::Ebdt => Self::Ebdt,
            _ => return Err(source),
        })
    }
}

#[cfg(feature = "libm")]
#[allow(unused_imports)]
use core_maths::CoreFloat;

/// The bitmap strikes of a font, read from a single table.
///
/// Cheap to create and copy. It borrows tables that the font has already
/// parsed.
#[derive(Clone, Copy)]
pub struct BitmapStrikes<'a> {
    font: &'a Font,
    kind: Kind<'a>,
}

#[derive(Clone, Copy)]
enum Kind<'a> {
    None,
    Sbix(&'a Sbix<'a>),
    Cbdt(&'a Cblc<'a>, &'a Cbdt<'a>),
    Ebdt(&'a Eblc<'a>, &'a Ebdt<'a>),
}

impl<'a> BitmapStrikes<'a> {
    pub(crate) fn new(font: &'a Font, order: GlyphSourceOrder) -> Self {
        for source in order
            .iter()
            .filter_map(|source| BitmapSource::try_from(source).ok())
        {
            let kind = match source {
                BitmapSource::Sbix => font.sbix().map(Kind::Sbix),
                BitmapSource::Cbdt => font.cbdt().map(|(cblc, cbdt)| Kind::Cbdt(cblc, cbdt)),
                BitmapSource::Ebdt => font.ebdt().map(|(eblc, ebdt)| Kind::Ebdt(eblc, ebdt)),
            };
            if let Some(kind) = kind {
                return Self { font, kind };
            }
        }
        Self {
            font,
            kind: Kind::None,
        }
    }

    /// Returns the table that provides the bitmaps, or `None` if the font has
    /// none of the requested sources.
    pub fn source(&self) -> Option<BitmapSource> {
        match self.kind {
            Kind::None => None,
            Kind::Sbix(..) => Some(BitmapSource::Sbix),
            Kind::Cbdt(..) => Some(BitmapSource::Cbdt),
            Kind::Ebdt(..) => Some(BitmapSource::Ebdt),
        }
    }

    /// Returns the number of strikes, or zero if there is no bitmap table.
    pub fn len(&self) -> u32 {
        let count = match self.kind {
            Kind::None => 0,
            Kind::Sbix(sbix) => sbix.strikes().len(),
            Kind::Cbdt(cblc, _) => cblc.bitmap_sizes().len(),
            Kind::Ebdt(eblc, _) => eblc.bitmap_sizes().len(),
        };
        count as u32
    }

    /// Returns `true` if there are no strikes.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Returns the strike at `index`.
    pub fn get(&self, index: u32) -> Option<Strike<'a>> {
        let index = index as usize;
        let kind = match self.kind {
            Kind::None => return None,
            Kind::Sbix(sbix) => {
                let ppem = sbix.strikes().get(index).ok()?.ppem();
                StrikeKind::Sbix(sbix, index as u32, ppem)
            }
            Kind::Cbdt(cblc, cbdt) => StrikeKind::Cbdt(cblc.bitmap_sizes().get(index)?, cblc, cbdt),
            Kind::Ebdt(eblc, ebdt) => StrikeKind::Ebdt(eblc.bitmap_sizes().get(index)?, eblc, ebdt),
        };
        Some(Strike {
            font: self.font,
            kind,
        })
    }

    /// Returns an iterator over the strikes, in index order.
    pub fn iter(&self) -> impl Iterator<Item = Strike<'a>> + 'a {
        let this = *self;
        (0..this.len()).filter_map(move |index| this.get(index))
    }

    /// Returns the strike for `ppem`.
    ///
    /// This is the smallest strike that is at least `ppem`, or the largest
    /// strike if none is that large. `None` or a size that is not positive
    /// selects the largest strike.
    pub fn select(&self, ppem: Option<f32>) -> Option<Strike<'a>> {
        let mut best: Option<Strike<'a>> = None;
        for strike in self.iter() {
            if best
                .as_ref()
                .is_none_or(|best| closer_to(ppem, strike.ppem(), best.ppem()))
            {
                best = Some(strike);
            }
        }
        best
    }

    /// Returns the bitmap for `glyph` from the strike selected for `ppem`.
    ///
    /// Strikes are ranked the same way as in [`select`](Self::select), but
    /// only strikes that contain the glyph are considered. A strike may not
    /// cover every glyph, so the strike [`select`](Self::select) returns is
    /// not always the one used.
    pub fn select_bitmap(&self, glyph: GlyphId, ppem: Option<f32>) -> Option<BitmapGlyph<'a>> {
        let mut best: Option<BitmapGlyph<'a>> = None;
        for strike in self.iter() {
            if best
                .as_ref()
                .is_none_or(|best| closer_to(ppem, strike.ppem(), best.ppem()))
            {
                if let Some(bitmap) = strike.get(glyph) {
                    best = Some(bitmap);
                }
            }
        }
        best
    }
}

/// Returns `true` if a strike of `candidate` pixels per em is a better
/// match for `ppem` than a strike of `best`.
///
/// The smallest strike that is at least
/// `ppem` is preferred, or the largest strike if none is that large. `None`
/// or a size that is not positive selects the largest strike.
fn closer_to(ppem: Option<f32>, candidate: u16, best: u16) -> bool {
    let requested = ppem.filter(|ppem| *ppem > 0.0).unwrap_or(f32::INFINITY);
    let (candidate, best) = (candidate as f32, best as f32);
    (requested <= candidate && candidate < best) || (requested > best && candidate > best)
}

/// A set of bitmaps at one size.
#[derive(Clone, Copy)]
pub struct Strike<'a> {
    /// The font is kept for its units per em, and because an `sbix` bitmap
    /// is positioned using the bearings of the outline it replaces.
    font: &'a Font,
    kind: StrikeKind<'a>,
}

#[derive(Clone, Copy)]
enum StrikeKind<'a> {
    /// The table, the strike's index in it, and its size.
    Sbix(&'a Sbix<'a>, u32, u16),
    Cbdt(&'a BitmapSize, &'a Cblc<'a>, &'a Cbdt<'a>),
    Ebdt(&'a BitmapSize, &'a Eblc<'a>, &'a Ebdt<'a>),
}

impl<'a> Strike<'a> {
    /// Returns the horizontal pixels per em.
    pub fn ppem_x(&self) -> u16 {
        match &self.kind {
            StrikeKind::Sbix(_, _, ppem) => *ppem,
            StrikeKind::Cbdt(size, ..) | StrikeKind::Ebdt(size, ..) => size.ppem_x() as u16,
        }
    }

    /// Returns the vertical pixels per em.
    pub fn ppem_y(&self) -> u16 {
        match &self.kind {
            StrikeKind::Sbix(_, _, ppem) => *ppem,
            StrikeKind::Cbdt(size, ..) | StrikeKind::Ebdt(size, ..) => size.ppem_y() as u16,
        }
    }

    /// Returns the size of the strike.
    ///
    /// This is the larger of the horizontal and vertical pixels per em.
    pub fn ppem(&self) -> u16 {
        self.ppem_x().max(self.ppem_y())
    }

    /// Returns the table that contains the strike.
    pub fn source(&self) -> BitmapSource {
        match &self.kind {
            StrikeKind::Sbix(..) => BitmapSource::Sbix,
            StrikeKind::Cbdt(..) => BitmapSource::Cbdt,
            StrikeKind::Ebdt(..) => BitmapSource::Ebdt,
        }
    }

    /// Returns the bitmap for `glyph`, or `None` if the strike does not
    /// contain it.
    ///
    /// A strike with no size has no glyphs, because its bitmaps cannot be
    /// placed in font units.
    pub fn get(&self, glyph: GlyphId) -> Option<BitmapGlyph<'a>> {
        if self.ppem_x() == 0 || self.ppem_y() == 0 {
            return None;
        }
        let units_per_em = self.font.units_per_em();
        match &self.kind {
            StrikeKind::Sbix(sbix, index, _) => {
                let strike = sbix.strikes().get(*index as usize).ok()?;
                sbix_glyph(&strike, self.font, glyph)
            }
            StrikeKind::Cbdt(size, cblc, cbdt) => {
                let location = size.location(cblc.offset_data(), glyph).ok()?;
                let data = cbdt.data(&location).ok()?;
                BitmapGlyph::from_bdt(glyph, BitmapSource::Cbdt, size, &data, units_per_em)
            }
            StrikeKind::Ebdt(size, eblc, ebdt) => {
                let location = size.location(eblc.offset_data(), glyph).ok()?;
                let data = ebdt.data(&location).ok()?;
                BitmapGlyph::from_bdt(glyph, BitmapSource::Ebdt, size, &data, units_per_em)
            }
        }
    }
}

/// The maximum number of `dupe` entries to follow.
///
/// This limit stops a cycle of entries from looping forever.
const MAX_SBIX_DUPES: usize = 8;

/// The eight-byte signature at the start of every PNG file.
const PNG_SIGNATURE: [u8; 8] = [0x89, b'P', b'N', b'G', 0x0D, 0x0A, 0x1A, 0x0A];

/// Reads a PNG glyph from an `sbix` strike.
///
/// A `dupe` entry resolves to the image of the glyph it names.
fn sbix_glyph<'a>(
    strike: &sbix::Strike<'a>,
    font: &Font,
    mut glyph: GlyphId,
) -> Option<BitmapGlyph<'a>> {
    // The bearings come from the glyph asked for, not the one a `dupe`
    // entry points at, and are only read once a PNG is found.
    let requested = glyph;
    for _ in 0..=MAX_SBIX_DUPES {
        let entry = strike.glyph_data(glyph).ok()??;
        match entry.graphic_type() {
            tag if tag == Tag::new(b"dupe") => {
                glyph = GlyphId::from(FontData::new(entry.data()).read_at::<u16>(0).ok()?);
            }
            tag if tag == Tag::new(b"png ") => {
                let data = entry.data();
                if data.get(..8)? != PNG_SIGNATURE {
                    return None;
                }
                // The IHDR chunk follows the signature: four bytes of length,
                // four of type, then the width and height.
                let reader = FontData::new(data);
                let width = reader.read_at::<u32>(16).ok()?;
                let height = reader.read_at::<u32>(20).ok()?;
                // HarfBuzz refuses a dimension that an i32 of pixels would
                // not hold once scaled.
                if width >= 1 << 16 || height >= 1 << 16 {
                    return None;
                }
                let ppem = strike.ppem();
                let (x_bearing, y_bearing) = sbix_bearings(font, requested);
                return Some(BitmapGlyph {
                    glyph: requested,
                    source: BitmapSource::Sbix,
                    pixels: Pixels::Png(data),
                    width,
                    height,
                    x_bearing,
                    y_bearing,
                    inner_x_bearing: entry.origin_offset_x() as i32,
                    inner_y_bearing: entry.origin_offset_y() as i32,
                    h_advance: None,
                    ppem_x: ppem,
                    ppem_y: ppem,
                    origin: Origin::BottomLeft,
                    units_per_em: font.units_per_em(),
                });
            }
            _ => return None,
        }
    }
    None
}

/// Returns the bearings used to position an `sbix` bitmap, in font units.
///
/// The specification places the image at the glyph origin. Skia and
/// CoreText offset it by the `hmtx` bearing and the bottom of the outline's
/// bounding box. This places the image where the outline would be. A glyph
/// without an outline is placed at the origin.
fn sbix_bearings(font: &Font, glyph: GlyphId) -> (i32, i32) {
    let Some((glyf, loca)) = font.selected_glyf_loca() else {
        return (0, 0);
    };
    let Ok(Some(outline)) = loca.get_glyf(glyph, glyf) else {
        return (0, 0);
    };
    if outline.number_of_contours() == 0 {
        return (0, 0);
    }
    let lsb = font
        .h_metric_records()
        .stated_side_bearing(glyph)
        .map(|lsb| lsb.to_i32())
        .unwrap_or(0);
    (lsb, outline.y_min() as i32)
}

/// The corner of a bitmap that the inner bearings position.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum Origin {
    /// The bearings position the top left corner, as in `CBDT` and `EBDT`.
    TopLeft,
    /// The bearings position the bottom left corner, as in `sbix`.
    BottomLeft,
}

/// An embedded bitmap glyph.
///
/// A bitmap is positioned in two steps. The outer bearings offset the glyph
/// origin in font units, so they scale with the font size. The inner
/// bearings then position the image's corner in pixels of the strike, so
/// they scale by the ratio of the font size to the strike size.
#[derive(Clone, Copy)]
pub struct BitmapGlyph<'a> {
    glyph: GlyphId,
    source: BitmapSource,
    pixels: Pixels<'a>,
    width: u32,
    height: u32,
    x_bearing: i32,
    y_bearing: i32,
    inner_x_bearing: i32,
    inner_y_bearing: i32,
    h_advance: Option<u32>,
    ppem_x: u16,
    ppem_y: u16,
    origin: Origin,
    /// The font's units per em, to convert pixels to font units.
    units_per_em: u16,
}

impl<'a> BitmapGlyph<'a> {
    fn from_bdt(
        glyph: GlyphId,
        source: BitmapSource,
        size: &BitmapSize,
        data: &bdt::BitmapData<'a>,
        units_per_em: u16,
    ) -> Option<Self> {
        let (inner_x_bearing, inner_y_bearing, advance, width, height) = match data.metrics {
            bdt::BitmapMetrics::Small(metrics) => (
                metrics.bearing_x() as i32,
                metrics.bearing_y() as i32,
                metrics.advance() as u32,
                metrics.width() as u32,
                metrics.height() as u32,
            ),
            bdt::BitmapMetrics::Big(metrics) => (
                metrics.hori_bearing_x() as i32,
                metrics.hori_bearing_y() as i32,
                metrics.hori_advance() as u32,
                metrics.width() as u32,
                metrics.height() as u32,
            ),
        };
        let bdt::BitmapContent::Data(format, bytes) = &data.content else {
            // Composite bitmaps are assembled from others, which nothing
            // here does yet.
            return None;
        };
        let bits_per_pixel = size.bit_depth();
        let pixels = match (bits_per_pixel, format) {
            (32, bdt::BitmapDataFormat::Png) => Pixels::Png(bytes),
            // Color bitmaps are a byte per channel, so they are never packed.
            (32, bdt::BitmapDataFormat::ByteAligned) => {
                Pixels::Bgra(Bgra::new(bytes, width, height)?)
            }
            (1 | 2 | 4 | 8, bdt::BitmapDataFormat::ByteAligned) => {
                Pixels::Mask(Mask::new(bytes, width, height, bits_per_pixel, false)?)
            }
            (1 | 2 | 4 | 8, bdt::BitmapDataFormat::BitAligned) => {
                Pixels::Mask(Mask::new(bytes, width, height, bits_per_pixel, true)?)
            }
            _ => return None,
        };
        Some(Self {
            glyph,
            source,
            pixels,
            width,
            height,
            x_bearing: 0,
            y_bearing: 0,
            inner_x_bearing,
            inner_y_bearing,
            h_advance: Some(advance),
            ppem_x: size.ppem_x() as u16,
            ppem_y: size.ppem_y() as u16,
            origin: Origin::TopLeft,
            units_per_em,
        })
    }

    /// Returns the glyph ID of this bitmap.
    ///
    /// For an `sbix` entry that reuses another glyph's image, this is the
    /// requested glyph.
    pub fn glyph(&self) -> GlyphId {
        self.glyph
    }

    /// Returns the table that contains the bitmap.
    pub fn source(&self) -> BitmapSource {
        self.source
    }

    /// Returns the pixel data.
    pub fn pixels(&self) -> Pixels<'a> {
        self.pixels
    }

    /// Returns the width in pixels.
    pub fn width(&self) -> u32 {
        self.width
    }

    /// Returns the height in pixels.
    pub fn height(&self) -> u32 {
        self.height
    }

    /// Returns the horizontal offset of the image's origin from the glyph
    /// origin, in font units.
    ///
    /// This is the `hmtx` bearing for an `sbix` glyph with an outline, and
    /// zero otherwise.
    pub fn x_bearing(&self) -> i32 {
        self.x_bearing
    }

    /// Returns the vertical offset of the image's origin above the glyph
    /// origin, in font units.
    ///
    /// This is the bottom of the outline's bounding box for an `sbix` glyph
    /// with an outline, and zero otherwise.
    pub fn y_bearing(&self) -> i32 {
        self.y_bearing
    }

    /// Returns the horizontal position of the [`origin`](Self::origin)
    /// corner, in pixels to the right of the image's origin.
    pub fn inner_x_bearing(&self) -> i32 {
        self.inner_x_bearing
    }

    /// Returns the vertical position of the [`origin`](Self::origin) corner,
    /// in pixels above the image's origin.
    pub fn inner_y_bearing(&self) -> i32 {
        self.inner_y_bearing
    }

    /// Returns the advance width in pixels, if the table provides one.
    ///
    /// This is always `None` for `sbix`.
    pub fn h_advance(&self) -> Option<u32> {
        self.h_advance
    }

    /// Returns the horizontal pixels per em of the strike that contains the
    /// glyph.
    pub fn ppem_x(&self) -> u16 {
        self.ppem_x
    }

    /// Returns the vertical pixels per em of the strike that contains the
    /// glyph.
    pub fn ppem_y(&self) -> u16 {
        self.ppem_y
    }

    /// Returns the corner that the inner bearings position.
    pub fn origin(&self) -> Origin {
        self.origin
    }

    /// Returns the size of the strike.
    ///
    /// This is the larger of the horizontal and vertical pixels per em.
    pub fn ppem(&self) -> u16 {
        self.ppem_x.max(self.ppem_y)
    }

    /// Returns the ink extents of the bitmap, in font units.
    ///
    /// Each value is scaled from pixels by the strike size and rounded to a
    /// whole unit. The bearing is to the top left corner, whatever corner the
    /// table uses. The table's outer bearings are not included.
    pub fn extents(&self) -> GlyphExtents<f32> {
        let pixels = self.pixel_extents();
        let units_per_em = self.units_per_em as f32;
        // A strike with no size has no glyphs, so neither size is zero.
        let scale = |value: i32, ppem: u16| (value as f32 * units_per_em / ppem as f32).round();
        GlyphExtents {
            x_bearing: scale(pixels.x_bearing, self.ppem_x),
            y_bearing: scale(pixels.y_bearing, self.ppem_y),
            width: scale(pixels.width, self.ppem_x),
            height: scale(pixels.height, self.ppem_y),
        }
    }

    /// The ink extents in pixels from the image's origin, with the bearing
    /// to the top left corner.
    fn pixel_extents(&self) -> GlyphExtents<i32> {
        let y_bearing = match self.origin {
            Origin::TopLeft => self.inner_y_bearing,
            Origin::BottomLeft => self.inner_y_bearing + self.height as i32,
        };
        GlyphExtents {
            x_bearing: self.inner_x_bearing,
            y_bearing,
            width: self.width as i32,
            height: self.height as i32,
        }
    }
}

/// Pixel data for a bitmap glyph.
#[derive(Clone, Copy)]
pub enum Pixels<'a> {
    /// A PNG image.
    Png(&'a [u8]),
    /// Uncompressed 32-bit color.
    Bgra(Bgra<'a>),
    /// A single channel coverage mask.
    Mask(Mask<'a>),
}

/// Uncompressed 32-bit color pixels.
///
/// Pixels are premultiplied, in BGRA order and in the sRGB color space.
#[derive(Clone, Copy)]
pub struct Bgra<'a> {
    data: &'a [u8],
    width: u32,
    height: u32,
}

impl<'a> Bgra<'a> {
    /// Creates a view of `data` as `width` by `height` pixels of four bytes
    /// each.
    ///
    /// Returns `None` if `data` is too short.
    pub fn new(data: &'a [u8], width: u32, height: u32) -> Option<Self> {
        let len = (width as usize)
            .checked_mul(height as usize)?
            .checked_mul(4)?;
        (data.len() >= len).then(|| Self {
            data: &data[..len],
            width,
            height,
        })
    }

    /// Returns the width in pixels.
    pub fn width(&self) -> u32 {
        self.width
    }

    /// Returns the height in pixels.
    pub fn height(&self) -> u32 {
        self.height
    }

    /// Returns the pixel data, row by row, four bytes per pixel.
    pub fn data(&self) -> &'a [u8] {
        self.data
    }

    /// Returns an iterator over the rows.
    ///
    /// Each row contains `width` pixels of four bytes.
    pub fn rows(&self) -> impl Iterator<Item = &'a [u8]> + 'a {
        let row_len = (self.width as usize * 4).max(1);
        self.data.chunks_exact(row_len)
    }

    /// Returns the pixel at column `x` of row `y`, as blue, green, red and
    /// alpha.
    pub fn pixel(&self, x: u32, y: u32) -> Option<[u8; 4]> {
        if x >= self.width || y >= self.height {
            return None;
        }
        let start = (y as usize * self.width as usize + x as usize) * 4;
        self.data.get(start..start + 4)?.try_into().ok()
    }
}

/// A single channel coverage mask of 1, 2, 4 or 8 bits per pixel.
///
/// The data is kept in the table's packing. The accessors decode it, so a
/// caller does not need to know the bit depth or whether rows are padded.
#[derive(Clone, Copy)]
pub struct Mask<'a> {
    data: &'a [u8],
    width: u32,
    height: u32,
    bits_per_pixel: u8,
    packed: bool,
}

impl<'a> Mask<'a> {
    /// Creates a view of `data` as `width` by `height` pixels of
    /// `bits_per_pixel` each.
    ///
    /// In a packed mask, each row begins at the bit that follows the previous
    /// row. In an unpacked mask, each row is padded to a whole byte.
    ///
    /// Returns `None` if the bit depth is not 1, 2, 4 or 8, or if `data` is
    /// too short.
    pub fn new(
        data: &'a [u8],
        width: u32,
        height: u32,
        bits_per_pixel: u8,
        packed: bool,
    ) -> Option<Self> {
        if !matches!(bits_per_pixel, 1 | 2 | 4 | 8) {
            return None;
        }
        let bits = bits_per_pixel as usize;
        let len = if packed {
            (width as usize)
                .checked_mul(height as usize)?
                .checked_mul(bits)?
                .div_ceil(8)
        } else {
            (width as usize)
                .checked_mul(bits)?
                .div_ceil(8)
                .checked_mul(height as usize)?
        };
        (data.len() >= len).then(|| Self {
            data: &data[..len],
            width,
            height,
            bits_per_pixel,
            packed,
        })
    }

    /// Returns the width in pixels.
    pub fn width(&self) -> u32 {
        self.width
    }

    /// Returns the height in pixels.
    pub fn height(&self) -> u32 {
        self.height
    }

    /// Returns the number of bits per pixel: 1, 2, 4 or 8.
    pub fn bits_per_pixel(&self) -> u8 {
        self.bits_per_pixel
    }

    /// Returns `true` if rows are not padded to a whole byte.
    pub fn is_packed(&self) -> bool {
        self.packed
    }

    /// Returns the raw pixel data from the table.
    pub fn data(&self) -> &'a [u8] {
        self.data
    }

    /// Returns the size in bytes of the decoded mask, one byte per pixel.
    ///
    /// This is the length [`decode_into`](Self::decode_into) writes.
    pub fn decoded_len(&self) -> usize {
        self.width as usize * self.height as usize
    }

    /// Returns `true` if there are no pixels.
    pub fn is_empty(&self) -> bool {
        self.decoded_len() == 0
    }

    /// Returns the coverage of the pixel at column `x` of row `y`.
    ///
    /// The value ranges from 0 to 255 for any bit depth.
    pub fn pixel(&self, x: u32, y: u32) -> Option<u8> {
        if x >= self.width || y >= self.height {
            return None;
        }
        Some(self.pixel_at(self.bit_offset(x, y)))
    }

    /// Returns an iterator over the coverage of each pixel, row by row.
    pub fn pixels(&self) -> impl Iterator<Item = u8> + 'a {
        let mask = *self;
        (0..mask.height).flat_map(move |y| mask.row(y).pixels())
    }

    /// Returns an iterator over the rows, from top to bottom.
    pub fn rows(&self) -> impl Iterator<Item = MaskRow<'a>> + 'a {
        let mask = *self;
        (0..mask.height).map(move |y| mask.row(y))
    }

    /// Writes the coverage of each pixel to `dst`, row by row, one byte per
    /// pixel.
    ///
    /// `dst` must hold at least [`decoded_len`](Self::decoded_len) bytes, and
    /// any bytes past that are left as they were. Returns `false`, writing
    /// nothing, if `dst` is shorter.
    pub fn decode_into(&self, dst: &mut [u8]) -> bool {
        let len = self.decoded_len();
        let Some(dst) = dst.get_mut(..len) else {
            return false;
        };
        if self.bits_per_pixel == 8 {
            // A byte per pixel is already the output, packed or not.
            dst.copy_from_slice(&self.data[..len]);
            return true;
        }
        for (out, pixel) in dst.iter_mut().zip(self.pixels()) {
            *out = pixel;
        }
        true
    }

    fn row(&self, y: u32) -> MaskRow<'a> {
        MaskRow { mask: *self, y }
    }

    /// Returns the bit offset of the pixel at column `x` of row `y`.
    fn bit_offset(&self, x: u32, y: u32) -> usize {
        let bits = self.bits_per_pixel as usize;
        if self.packed {
            (y as usize * self.width as usize + x as usize) * bits
        } else {
            let row_bits = (self.width as usize * bits).div_ceil(8) * 8;
            y as usize * row_bits + x as usize * bits
        }
    }

    /// Returns the coverage of the pixel at `bit`, scaled to 0 to 255.
    fn pixel_at(&self, bit: usize) -> u8 {
        let byte = self.data.get(bit >> 3).copied().unwrap_or(0);
        match self.bits_per_pixel {
            1 => ((byte >> (7 - (bit & 7))) & 1) * 255,
            2 => ((byte >> (6 - (bit & 7))) & 3) * 85,
            4 => ((byte >> (4 - (bit & 7))) & 15) * 17,
            _ => byte,
        }
    }
}

/// One row of a [`Mask`].
#[derive(Clone, Copy)]
pub struct MaskRow<'a> {
    mask: Mask<'a>,
    y: u32,
}

impl<'a> MaskRow<'a> {
    /// Returns the index of the row, counting from the top.
    pub fn y(&self) -> u32 {
        self.y
    }

    /// Returns the coverage of the pixel at column `x`, from 0 to 255.
    pub fn pixel(&self, x: u32) -> Option<u8> {
        self.mask.pixel(x, self.y)
    }

    /// Returns an iterator over the coverage of each pixel, left to right.
    pub fn pixels(&self) -> impl Iterator<Item = u8> + 'a {
        let mask = self.mask;
        let start = mask.bit_offset(0, self.y);
        let bits = mask.bits_per_pixel as usize;
        (0..mask.width as usize).map(move |x| mask.pixel_at(start + x * bits))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::model::glyph::GlyphSource;
    use alloc::vec::Vec;

    fn font(data: &[u8]) -> Font {
        Font::new(data.to_vec(), 0).unwrap()
    }

    #[test]
    fn cbdt_strikes() {
        let font = font(font_test_data::CBDT);
        let bitmaps = font.bitmap_strikes();
        assert_eq!(bitmaps.source(), Some(BitmapSource::Cbdt));
        assert_eq!(bitmaps.len(), 3);
        let ppems: Vec<_> = bitmaps.iter().map(|strike| strike.ppem()).collect();
        assert_eq!(ppems, [16, 64, 128]);
    }

    #[test]
    fn cbdt_glyph_metrics() {
        let font = font(font_test_data::CBDT);
        let bitmaps = font.bitmap_strikes();
        let zero = bitmaps.get(0).unwrap().get(GlyphId::new(0)).unwrap();
        assert_eq!(zero.glyph(), GlyphId::new(0));
        assert_eq!(zero.source(), BitmapSource::Cbdt);
        assert_eq!((zero.width(), zero.height()), (11, 13));
        assert_eq!((zero.x_bearing(), zero.y_bearing()), (0, 0));
        assert_eq!((zero.inner_x_bearing(), zero.inner_y_bearing()), (1, 13));
        assert_eq!(zero.h_advance(), Some(12));
        assert_eq!(zero.origin(), Origin::TopLeft);
        assert_eq!(
            zero.pixel_extents(),
            GlyphExtents {
                x_bearing: 1,
                y_bearing: 13,
                width: 11,
                height: 13,
            }
        );
        let two = bitmaps.get(1).unwrap().get(GlyphId::new(2)).unwrap();
        assert_eq!((two.width(), two.height()), (39, 52));
        assert_eq!((two.x_bearing(), two.y_bearing()), (0, 0));
        assert_eq!((two.inner_x_bearing(), two.inner_y_bearing()), (6, 52));
        assert_eq!(two.h_advance(), Some(51));
        assert!(matches!(two.pixels(), Pixels::Png(_)));
    }

    #[test]
    fn strike_selection_follows_harfbuzz() {
        let font = font(font_test_data::CBDT);
        let bitmaps = font.bitmap_strikes();
        let strike_for = |ppem| bitmaps.select(ppem).unwrap().ppem();
        // The smallest strike at least as large as the size.
        assert_eq!(strike_for(Some(12.0)), 16);
        assert_eq!(strike_for(Some(16.0)), 16);
        assert_eq!(strike_for(Some(17.0)), 64);
        assert_eq!(strike_for(Some(60.0)), 64);
        // Past every strike, the largest.
        assert_eq!(strike_for(Some(200.0)), 128);
        // A fractional size takes the strike at least as large.
        assert_eq!(strike_for(Some(16.5)), 64);
        assert_eq!(strike_for(Some(15.5)), 16);
        // No size, or one HarfBuzz would read as zero, picks the largest.
        assert_eq!(strike_for(None), 128);
        for ppem in [0.0, -0.0, -12.0, f32::NAN] {
            assert_eq!(strike_for(Some(ppem)), 128, "{ppem}");
        }
        assert_eq!(
            bitmaps
                .select_bitmap(GlyphId::new(2), Some(12.0))
                .unwrap()
                .ppem_x(),
            16
        );
        assert_eq!(
            bitmaps
                .select_bitmap(GlyphId::new(2), Some(0.0))
                .unwrap()
                .ppem_x(),
            128
        );
    }

    #[test]
    fn a_glyph_comes_from_the_best_strike_that_has_it() {
        let font = font(font_test_data::CBDT);
        let bitmaps = font.bitmap_strikes();
        let gid = GlyphId::new(2);
        for (ppem, expected) in [
            (Some(12.0), 16),
            (Some(17.0), 64),
            (Some(60.0), 64),
            (None, 128),
        ] {
            assert_eq!(
                bitmaps.select_bitmap(gid, ppem).unwrap().ppem(),
                expected,
                "{ppem:?}"
            );
        }
        // The ranking alone, as a strike missing a glyph would apply it.
        assert!(closer_to(Some(12.0), 16, 64));
        assert!(!closer_to(Some(12.0), 64, 16));
        assert!(closer_to(Some(200.0), 128, 64));
        assert!(closer_to(None, 128, 16));
        assert!(!closer_to(None, 16, 128));
    }

    #[test]
    fn extents_scale_to_font_units_as_harfbuzz_does() {
        let font = font(font_test_data::CBDT);
        let glyph = font
            .bitmap_strikes()
            .select_bitmap(GlyphId::new(0), Some(16.0))
            .unwrap();
        let extents = glyph.extents();
        let scale = font.units_per_em() as f32 / 16.0;
        assert_eq!(extents.x_bearing, (1.0 * scale).round());
        assert_eq!(extents.y_bearing, (13.0 * scale).round());
        assert_eq!(extents.width, (11.0 * scale).round());
        assert_eq!(extents.height, (13.0 * scale).round());
    }

    // A strike with no size has no glyphs, as in HarfBuzz, so the search
    // moves on to the next strike.
    #[test]
    fn a_strike_with_no_size_has_no_glyphs() {
        use crate::{model::Blob, types::Tag, FontRef};
        use alloc::sync::Arc;
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(|tag| {
            let font = FontRef::new(font_test_data::CBDT).ok()?;
            let mut data = font.table_data(tag)?.as_bytes().to_vec();
            if tag == Tag::new(b"CBLC") {
                // The first size record's horizontal ppem: an eight byte
                // header, then 44 bytes into the record.
                data[52] = 0;
            }
            Some(Blob::from(data))
        });
        let font = Font::new(source, 0).unwrap();
        let bitmaps = font.bitmap_strikes();
        let strike = bitmaps.get(0).unwrap();
        assert_eq!(strike.ppem_x(), 0);
        assert!(strike.get(GlyphId::new(0)).is_none());
        let glyph = bitmaps.select_bitmap(GlyphId::new(0), Some(1.0)).unwrap();
        assert!(glyph.ppem_x() > 0);
        let extents = glyph.extents();
        assert!(extents.width.is_finite() && extents.width > 0.0);
    }

    #[test]
    fn sbix_glyphs() {
        let font = font(font_test_data::NOTO_HANDWRITING_SBIX);
        let bitmaps = font.bitmap_strikes();
        assert_eq!(bitmaps.source(), Some(BitmapSource::Sbix));
        assert_eq!(bitmaps.len(), 1);
        let strike = bitmaps.get(0).unwrap();
        assert_eq!(strike.ppem(), 109);
        let glyph = strike.get(GlyphId::new(7)).unwrap();
        assert_eq!(glyph.glyph(), GlyphId::new(7));
        assert_eq!(glyph.source(), BitmapSource::Sbix);
        // The glyph has no outline, so nothing moves its origin.
        assert_eq!((glyph.x_bearing(), glyph.y_bearing()), (0, 0));
        assert_eq!((glyph.inner_x_bearing(), glyph.inner_y_bearing()), (4, -27));
        assert_eq!(glyph.origin(), Origin::BottomLeft);
        assert!(matches!(glyph.pixels(), Pixels::Png(_)));
        assert!(glyph.width() > 0 && glyph.height() > 0);
        // The bearing to the top of the image is the height above the offset.
        assert_eq!(glyph.pixel_extents().y_bearing, glyph.height() as i32 - 27);
        assert_eq!(glyph.h_advance(), None);
    }

    #[test]
    fn sbix_bearings_come_from_the_outline_the_image_stands_in_for() {
        let font = font(font_test_data::NOTO_HANDWRITING_SBIX);
        // Glyph 0 is the only glyph with contours: its `hmtx` bearing is 51
        // and its stated box reaches down to -250.
        assert_eq!(sbix_bearings(&font, GlyphId::new(0)), (51, -250));
        // Glyph 7 has an image but no contours, so its origin stays put.
        assert_eq!(sbix_bearings(&font, GlyphId::new(7)), (0, 0));
    }

    #[test]
    fn ebdt_masks() {
        let font = font(font_test_data::EMBEDDED_BITMAPS);
        let bitmaps = font.bitmap_strikes_with(GlyphSourceOrder::new(&[GlyphSource::Ebdt]));
        assert_eq!(bitmaps.source(), Some(BitmapSource::Ebdt));
        let strike = bitmaps.get(0).unwrap();
        assert_eq!((strike.ppem_x(), strike.ppem_y()), (7, 7));
        let glyph = strike.get(GlyphId::new(1)).unwrap();
        assert_eq!(glyph.glyph(), GlyphId::new(1));
        assert_eq!(glyph.source(), BitmapSource::Ebdt);
        assert_eq!((glyph.width(), glyph.height()), (3, 8));
        let Pixels::Mask(mask) = &glyph.pixels() else {
            panic!("expected a mask");
        };
        assert_eq!(mask.bits_per_pixel(), 1);
        assert!(mask.is_packed());
        assert_eq!(mask.decoded_len(), 24);
        // The data is 0xee, 0xae, 0xea: three columns of eight rows.
        let pixels: Vec<_> = mask.pixels().map(|p| (p == 255) as u8).collect();
        assert_eq!(
            pixels,
            [1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0]
        );
        assert_eq!(mask.rows().count(), 8);
        assert_eq!(
            mask.rows().nth(1).unwrap().pixels().collect::<Vec<_>>(),
            [0, 255, 255]
        );
        let mut decoded = [0u8; 24];
        assert!(mask.decode_into(&mut decoded));
        assert_eq!(decoded[..3], [255, 255, 255]);
        assert!(!mask.decode_into(&mut [0u8; 23]));
    }

    #[test]
    fn default_order_prefers_cbdt_over_ebdt() {
        let font = font(font_test_data::EMBEDDED_BITMAPS);
        assert_eq!(font.bitmap_strikes().source(), Some(BitmapSource::Cbdt));
        assert_eq!(
            font.bitmap_strikes_with(GlyphSourceOrder::OUTLINES)
                .source(),
            None
        );
    }

    // A font with only EBDT bitmaps still has bitmaps, though the extents
    // order leaves EBDT out.
    #[test]
    fn a_font_with_only_ebdt_has_bitmaps() {
        use crate::{model::Blob, types::Tag, FontRef};
        use alloc::sync::Arc;
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(|tag| {
            if tag == Tag::new(b"CBDT") || tag == Tag::new(b"CBLC") {
                return None;
            }
            let font = FontRef::new(font_test_data::EMBEDDED_BITMAPS).ok()?;
            Some(Blob::from(font.table_data(tag)?.as_bytes().to_vec()))
        });
        let font = Font::new(source, 0).unwrap();
        let bitmaps = font.bitmap_strikes();
        assert_eq!(bitmaps.source(), Some(BitmapSource::Ebdt));
        assert!(!bitmaps.is_empty());
        assert!(!GlyphSourceOrder::DEFAULT.contains(GlyphSource::Ebdt));
    }

    #[test]
    fn unpacked_mask_pixels() {
        // 4 by 2, 1 bit per pixel, each row a byte: 1010 then 0101.
        let mask = Mask::new(&[0xA0, 0x50], 4, 2, 1, false).unwrap();
        let pixels: Vec<_> = mask.pixels().collect();
        assert_eq!(pixels, [255, 0, 255, 0, 0, 255, 0, 255]);
        assert_eq!(mask.pixel(3, 1), Some(255));
        assert_eq!(mask.pixel(4, 1), None);
        // 3 by 2, 4 bits per pixel, rows padded to two bytes.
        let mask = Mask::new(&[0xF8, 0x40, 0x05, 0xA0], 3, 2, 4, false).unwrap();
        let pixels: Vec<_> = mask.pixels().collect();
        assert_eq!(pixels, [255, 136, 68, 0, 85, 170]);
        // 4 by 1, 2 bits per pixel.
        let mask = Mask::new(&[0xE4], 4, 1, 2, false).unwrap();
        assert_eq!(mask.pixels().collect::<Vec<_>>(), [255, 170, 85, 0]);
    }

    #[test]
    fn packed_mask_pixels() {
        // 3 by 3, 1 bit per pixel, nine bits run together.
        let mask = Mask::new(&[0xAB, 0x00], 3, 3, 1, true).unwrap();
        let pixels: Vec<_> = mask.pixels().collect();
        assert_eq!(pixels, [255, 0, 255, 0, 255, 0, 255, 255, 0]);
        // 5 by 2, 2 bits per pixel.
        let mask = Mask::new(&[0xE4, 0xC6, 0xC0], 5, 2, 2, true).unwrap();
        assert_eq!(
            mask.pixels().collect::<Vec<_>>(),
            [255, 170, 85, 0, 255, 0, 85, 170, 255, 0]
        );
        // 3 by 2, 4 bits per pixel.
        let mask = Mask::new(&[0xF0, 0x84, 0xA5], 3, 2, 4, true).unwrap();
        assert_eq!(
            mask.pixels().collect::<Vec<_>>(),
            [255, 0, 136, 68, 170, 85]
        );
        // A byte per pixel copies through.
        let mask = Mask::new(&[100, 200, 50, 0, 128, 255], 3, 2, 8, true).unwrap();
        let mut out = [0u8; 6];
        assert!(mask.decode_into(&mut out));
        assert_eq!(out, [100, 200, 50, 0, 128, 255]);
    }

    #[test]
    fn short_or_odd_masks_are_refused() {
        assert!(Mask::new(&[0xFF], 3, 3, 1, true).is_none());
        assert!(Mask::new(&[1, 2, 3], 4, 2, 8, false).is_none());
        assert!(Mask::new(&[0; 16], 2, 2, 3, false).is_none());
        let empty = Mask::new(&[], 0, 5, 8, false).unwrap();
        assert!(empty.is_empty());
        assert_eq!(empty.pixels().count(), 0);
    }

    #[test]
    fn bgra_rows_and_pixels() {
        let data: Vec<u8> = (0..16).collect();
        let bgra = Bgra::new(&data, 2, 2).unwrap();
        assert_eq!(bgra.rows().count(), 2);
        assert_eq!(bgra.pixel(1, 1), Some([12, 13, 14, 15]));
        assert_eq!(bgra.pixel(2, 0), None);
        assert!(Bgra::new(&data, 3, 2).is_none());
    }
}
