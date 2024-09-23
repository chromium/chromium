//! Common functionality for glyf, cff and autohinting scalers.
//!
//! Currently this includes the font reference as well as horizontal glyph
//! metrics and access to the control value table.

use raw::{
    tables::{hmtx::Hmtx, hvar::Hvar},
    types::{BigEndian, F2Dot14, GlyphId, Tag},
    FontRef, TableProvider,
};

/// Common functionality for glyf, cff and autohinting scalers.
#[derive(Clone)]
pub(crate) struct OutlinesCommon<'a> {
    pub font: FontRef<'a>,
    pub hmtx: Hmtx<'a>,
    pub hvar: Option<Hvar<'a>>,
}

impl<'a> OutlinesCommon<'a> {
    pub fn new(font: &FontRef<'a>) -> Option<Self> {
        // Note: hmtx is required and HVAR is optional
        let hmtx = font.hmtx().ok()?;
        let hvar = font.hvar().ok();
        Some(Self {
            font: font.clone(),
            hmtx,
            hvar,
        })
    }

    /// Returns the advance width (in font units) for the given glyph and
    /// the location in variation space represented by the set of normalized
    /// coordinates in 2.14 fixed point.
    pub fn advance_width(&self, gid: GlyphId, coords: &'a [F2Dot14]) -> i32 {
        let mut advance = self.hmtx.advance(gid).unwrap_or_default() as i32;
        if let Some(hvar) = &self.hvar {
            advance += hvar
                .advance_width_delta(gid, coords)
                .map(|delta| delta.to_i32())
                .unwrap_or(0);
        }
        advance
    }

    /// Returns the left side bearing (in font units) for the given glyph and
    /// the location in variation space represented by the set of normalized
    /// coordinates in 2.14 fixed point.    
    pub fn lsb(&self, gid: GlyphId, coords: &'a [F2Dot14]) -> i32 {
        let mut lsb = self.hmtx.side_bearing(gid).unwrap_or_default() as i32;
        if let Some(hvar) = &self.hvar {
            lsb += hvar
                .lsb_delta(gid, coords)
                .map(|delta| delta.to_i32())
                .unwrap_or(0);
        }
        lsb
    }

    /// Returns the array of entries for the control value table which is used
    /// for TrueType hinting.
    pub fn cvt(&self) -> &[BigEndian<i16>] {
        self.font
            .data_for_tag(Tag::new(b"cvt "))
            .and_then(|d| d.read_array(0..d.len()).ok())
            .unwrap_or_default()
    }
}
