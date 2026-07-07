use crate::{
    hb::{
        face::{BasicFontMetrics, Scale},
        tables::TableRanges,
    },
    GlyphExtents, GlyphInfo, GlyphPosition, Tag,
};
use read_fonts::{
    tables::{
        glyf::Glyf,
        gvar::Gvar,
        hmtx::{Hmtx, LongMetric},
        hvar::Hvar,
        loca::Loca,
        mvar::Mvar,
        vmtx::Vmtx,
        vorg::Vorg,
        vvar::Vvar,
    },
    types::{BoundingBox, F2Dot14, Fixed, GlyphId, Point},
    FontRef, TableProvider,
};

#[derive(Clone)]
pub struct GlyphMetrics<'a> {
    _hmtx: Option<Hmtx<'a>>,
    h_metrics: &'a [LongMetric],
    hvar: Option<Hvar<'a>>,
    vmtx: Option<Vmtx<'a>>,
    vvar: Option<Vvar<'a>>,
    vorg: Option<Vorg<'a>>,
    glyf: Option<GlyfTables<'a>>,
    mvar: Option<Mvar<'a>>,
    num_glyphs: u32,
    upem: u16,
    ascent: i16,
    descent: i16,
}

#[derive(Clone)]
struct GlyfTables<'a> {
    loca: Loca<'a>,
    glyf: Glyf<'a>,
    gvar: Option<Gvar<'a>>,
}

impl<'a> GlyphMetrics<'a> {
    pub(crate) fn new(font: &FontRef<'a>, table_ranges: &TableRanges) -> Self {
        let num_glyphs = table_ranges.num_glyphs;
        let upem = table_ranges.units_per_em;
        let hmtx = table_ranges
            .hmtx
            .resolve_data(font)
            .and_then(|data| Hmtx::read(data, table_ranges.num_h_metrics).ok());
        let h_metrics = hmtx
            .as_ref()
            .map(|hmtx| hmtx.h_metrics())
            .unwrap_or_default();
        let hvar = table_ranges.hvar.resolve_table(font);
        let vmtx = table_ranges
            .vmtx
            .resolve_data(font)
            .and_then(|data| Vmtx::read(data, table_ranges.num_v_metrics).ok());
        let vvar = table_ranges.vvar.resolve_table(font);
        let vorg = table_ranges.vorg.resolve_table(font);
        let loca = table_ranges
            .loca
            .resolve_data(font)
            .and_then(|data| Loca::read(data, table_ranges.loca_long).ok());
        let glyf = table_ranges.glyf.resolve_table(font);
        let glyf = if let Some((loca, glyf)) = loca.zip(glyf) {
            let gvar = table_ranges.gvar.resolve_table(font);
            Some(GlyfTables { loca, glyf, gvar })
        } else {
            None
        };
        let mvar = table_ranges.mvar.resolve_table(font);
        let ascent = table_ranges.ascent;
        let descent = table_ranges.descent;
        Self {
            _hmtx: hmtx,
            h_metrics,
            hvar,
            vmtx,
            vvar,
            vorg,
            glyf,
            mvar,
            num_glyphs,
            upem,
            ascent,
            descent,
        }
    }

    pub(crate) fn from_tables(font: &impl TableProvider<'a>, metrics: &BasicFontMetrics) -> Self {
        let hmtx = font.hmtx().ok();
        let h_metrics = hmtx
            .as_ref()
            .map(|hmtx| hmtx.h_metrics())
            .unwrap_or_default();
        let hvar = font.hvar().ok();
        let vmtx = font.vmtx().ok();
        let vvar = font.vvar().ok();
        let vorg = font.vorg().ok();
        let loca = font.loca(None).ok();
        let glyf = font.glyf().ok();
        let glyf = if let Some((loca, glyf)) = loca.zip(glyf) {
            let gvar = font.gvar().ok();
            Some(GlyfTables { loca, glyf, gvar })
        } else {
            None
        };
        let mvar = font.mvar().ok();
        Self {
            _hmtx: hmtx,
            h_metrics,
            hvar,
            vmtx,
            vvar,
            vorg,
            glyf,
            mvar,
            num_glyphs: metrics.num_glyphs,
            upem: metrics.units_per_em,
            ascent: metrics.ascent,
            descent: metrics.descent,
        }
    }

    pub(crate) fn advance_width(&self, gid: impl Into<GlyphId>, coords: &[F2Dot14]) -> Option<i32> {
        let gid = gid.into();
        let Some(mut advance) = self
            .h_metrics
            .get(gid.to_u32() as usize)
            .or_else(|| self.h_metrics.last())
            .map(|metric| metric.advance() as i32)
        else {
            return (gid.to_u32() < self.num_glyphs).then_some(self.upem as i32 / 2);
        };
        if !coords.is_empty() {
            if let Some(hvar) = self.hvar.as_ref() {
                advance += hvar
                    .advance_width_delta(gid, coords)
                    .unwrap_or_default()
                    .to_i32();
            } else if let Some(deltas) = self.phantom_deltas(gid, coords) {
                advance += deltas[1].x.to_i32() - deltas[0].x.to_i32();
            }
        }
        Some(advance)
    }

    pub(crate) fn populate_advance_widths(
        &self,
        infos: &[GlyphInfo],
        pos: &mut [GlyphPosition],
        coords: &[F2Dot14],
        scale: Scale,
    ) {
        for (info, pos) in infos.iter().zip(pos.iter_mut()) {
            pos.x_advance = self
                .h_metrics
                .get(info.glyph_id as usize)
                .or_else(|| self.h_metrics.last())
                .map(|metric| metric.advance() as i32)
                .or_else(|| (info.glyph_id < self.num_glyphs).then_some(self.upem as i32 / 2))
                .unwrap_or_default();
        }
        if !coords.is_empty() {
            if let Some(hvar) = self.hvar.as_ref() {
                for (info, pos) in infos.iter().zip(pos.iter_mut()) {
                    pos.x_advance += hvar
                        .advance_width_delta(info.as_glyph(), coords)
                        .unwrap_or_default()
                        .to_i32();
                }
            } else {
                for (info, pos) in infos.iter().zip(pos.iter_mut()) {
                    if let Some(deltas) = self.phantom_deltas(info.as_glyph(), coords) {
                        pos.x_advance += deltas[1].x.to_i32() - deltas[0].x.to_i32();
                    }
                }
            }
        }
        for pos in pos.iter_mut() {
            pos.x_advance = scale.scale_x(pos.x_advance);
        }
    }

    pub(crate) fn _left_side_bearing(
        &self,
        gid: impl Into<GlyphId>,
        coords: &[F2Dot14],
    ) -> Option<i32> {
        let gid = gid.into();
        let mut bearing = if let Some(hmtx) = self._hmtx.as_ref() {
            hmtx.side_bearing(gid).unwrap_or_default() as i32
        } else if let Some(extents) = self.bounds(gid, coords) {
            return Some(extents.x_min);
        } else {
            return None;
        };
        if !coords.is_empty() {
            if let Some(hvar) = self.hvar.as_ref() {
                bearing += hvar.lsb_delta(gid, coords).unwrap_or_default().to_i32();
            } else if let Some(deltas) = self.phantom_deltas(gid, coords) {
                bearing += deltas[0].x.to_i32();
            }
        }
        Some(bearing)
    }

    pub(crate) fn advance_height(
        &self,
        gid: impl Into<GlyphId>,
        coords: &[F2Dot14],
    ) -> Option<i32> {
        let gid = gid.into();
        let Some(mut advance) = self
            .vmtx
            .as_ref()
            .and_then(|vmtx| vmtx.advance(gid))
            .map(|advance| advance as i32)
        else {
            return Some(self.ascent as i32 - self.descent as i32);
        };
        if !coords.is_empty() {
            if let Some(vvar) = self.vvar.as_ref() {
                advance += vvar
                    .advance_height_delta(gid, coords)
                    .unwrap_or_default()
                    .to_i32();
            } else if let Some(deltas) = self.phantom_deltas(gid, coords) {
                advance += deltas[3].y.to_i32() - deltas[2].y.to_i32();
            }
        }
        Some(advance)
    }

    pub(crate) fn top_side_bearing(
        &self,
        gid: impl Into<GlyphId>,
        coords: &[F2Dot14],
    ) -> Option<i32> {
        let gid = gid.into();
        let mut bearing = if let Some(vmtx) = self.vmtx.as_ref() {
            vmtx.side_bearing(gid).unwrap_or_default() as i32
        } else {
            return None;
        };
        if !coords.is_empty() {
            if let Some(vvar) = self.vvar.as_ref() {
                bearing += vvar.tsb_delta(gid, coords).unwrap_or_default().to_i32();
            } else if let Some(deltas) = self.phantom_deltas(gid, coords) {
                bearing += deltas[3].y.to_i32();
            }
        }
        Some(bearing)
    }

    pub(crate) fn v_origin(&self, gid: impl Into<GlyphId>, coords: &[F2Dot14]) -> Option<i32> {
        let gid = gid.into();
        let origin = if let Some(vorg) = self.vorg.as_ref() {
            let mut origin = vorg.vertical_origin_y(gid) as i32;
            if !coords.is_empty() {
                if let Some(vvar) = self.vvar.as_ref() {
                    origin += vvar.v_org_delta(gid, coords).unwrap_or_default().to_i32();
                }
            }
            origin
        } else if let Some(extents) = self.bounds(gid, coords) {
            let origin = if self.vmtx.is_some() {
                let mut origin = Some(extents.y_max);
                let tsb = self.top_side_bearing(gid, coords);
                if let Some(tsb) = tsb {
                    origin = Some(origin.unwrap() + tsb);
                } else {
                    origin = None;
                }
                if origin.is_some() && !coords.is_empty() {
                    if let Some(vvar) = self.vvar.as_ref() {
                        origin = Some(
                            origin.unwrap()
                                + vvar.v_org_delta(gid, coords).unwrap_or_default().to_i32(),
                        );
                    }
                }
                origin
            } else {
                None
            };

            if let Some(origin) = origin {
                origin
            } else {
                let mut advance = self.ascent as i32 - self.descent as i32;
                if let Some(mvar) = self.mvar.as_ref() {
                    advance += mvar
                        .metric_delta(Tag::new(b"hasc"), coords)
                        .unwrap_or_default()
                        .to_i32()
                        - mvar
                            .metric_delta(Tag::new(b"hdsc"), coords)
                            .unwrap_or_default()
                            .to_i32();
                }
                let diff = advance - (extents.y_max - extents.y_min);
                extents.y_max + (diff >> 1)
            }
        } else {
            let mut ascent = self.ascent as i32;
            if let Some(mvar) = self.mvar.as_ref() {
                ascent += mvar
                    .metric_delta(Tag::new(b"hasc"), coords)
                    .unwrap_or_default()
                    .to_i32();
            }
            ascent
        };
        Some(origin)
    }

    fn bounds(&self, gid: impl Into<GlyphId>, coords: &[F2Dot14]) -> Option<BoundingBox<i32>> {
        let gid = gid.into();
        let glyf = self.glyf.as_ref()?;
        let glyph = glyf.loca.get_glyf(gid, &glyf.glyf).ok()?;
        let Some(glyph) = glyph else {
            // Return empty extents for empty glyph
            return Some(BoundingBox::default());
        };
        if !coords.is_empty() {
            return None; // TODO https://github.com/harfbuzz/harfrust/pull/52#issuecomment-2878117808
        }
        Some(BoundingBox {
            x_min: glyph.x_min() as i32,
            y_min: glyph.y_min() as i32,
            x_max: glyph.x_max() as i32,
            y_max: glyph.y_max() as i32,
        })
    }

    pub(crate) fn extents(
        &self,
        gid: impl Into<GlyphId>,
        coords: &[F2Dot14],
    ) -> Option<GlyphExtents> {
        let gid = gid.into();
        let glyf = self.glyf.as_ref()?;
        let glyph = glyf.loca.get_glyf(gid, &glyf.glyf).ok()?;
        let Some(glyph) = glyph else {
            // Return empty extents for empty glyph
            return Some(GlyphExtents::default());
        };
        if !coords.is_empty() {
            return None; // TODO https://github.com/harfbuzz/harfrust/pull/52#issuecomment-2878117808
        }
        Some(GlyphExtents {
            x_bearing: glyph.x_min() as i32,
            y_bearing: glyph.y_max() as i32,
            width: glyph.x_max() as i32 - glyph.x_min() as i32,
            height: glyph.y_min() as i32 - glyph.y_max() as i32,
        })
    }

    fn phantom_deltas(&self, gid: GlyphId, coords: &[F2Dot14]) -> Option<[Point<Fixed>; 4]> {
        let glyf = self.glyf.as_ref()?;
        let gvar = glyf.gvar.as_ref()?;
        gvar.phantom_point_deltas(&glyf.glyf, &glyf.loca, coords, gid)
            .ok()?
    }
}
