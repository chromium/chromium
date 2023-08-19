use super::{
    super::{Error, NormalizedCoord, Result, UniqueId, GLYF_COMPOSITE_RECURSION_LIMIT},
    Context, Outline, Point,
};

#[cfg(feature = "hinting")]
use {
    super::{super::Hinting, hint},
    read_fonts::tables::glyf::PointMarker,
};

use read_fonts::{
    tables::{
        glyf::{Anchor, CompositeGlyph, CompositeGlyphFlags, Glyf, Glyph, SimpleGlyph},
        gvar::Gvar,
        hmtx::Hmtx,
        hvar::Hvar,
        loca::Loca,
    },
    types::{BigEndian, F26Dot6, F2Dot14, Fixed, GlyphId, Tag},
    TableProvider,
};

/// TrueType glyph scaler for a specific font and configuration.
pub struct Scaler<'a> {
    /// Backing context.
    context: &'a mut Context,
    /// Current font data.
    font: ScalerFont<'a>,
    /// State for tracking current hinting mode and cache slot.
    #[cfg(feature = "hinting")]
    hint_config: hint::HintConfig,
    /// Phantom points. These are 4 extra points appended to the end of an
    /// outline that allow the bytecode interpreter to produce hinted
    /// metrics.
    ///
    /// See <https://learn.microsoft.com/en-us/typography/opentype/spec/tt_instructing_glyphs#phantom-points>
    phantom: [Point<F26Dot6>; 4],
}

impl<'a> Scaler<'a> {
    /// Creates a new scaler for extracting outlines with the specified font
    /// and configuration.
    pub fn new(
        context: &'a mut Context,
        font: &impl TableProvider<'a>,
        unique_id: Option<UniqueId>,
        size: f32,
        #[cfg(feature = "hinting")] hinting: Option<Hinting>,
        coords: &'a [NormalizedCoord],
    ) -> Result<Self> {
        let font = ScalerFont::new(font, unique_id, size, coords)?;
        Ok(Self {
            context,
            font,
            #[cfg(feature = "hinting")]
            hint_config: hint::HintConfig::new(hinting),
            phantom: Default::default(),
        })
    }

    /// Loads an outline for the specified glyph identifier to the preallocated
    /// target.
    pub fn load(&mut self, glyph_id: GlyphId, outline: &mut Outline) -> Result<()> {
        outline.clear();
        self.context.unscaled.clear();
        self.context.original.clear();
        self.context.deltas.clear();
        if glyph_id.to_u16() >= self.font.glyph_count {
            return Err(Error::GlyphNotFound(glyph_id));
        }
        #[cfg(feature = "hinting")]
        {
            self.hint_config.reset();
        }
        self.phantom = Default::default();
        self.load_glyph(glyph_id, outline, 0)?;
        let x_shift = self.phantom[0].x;
        if x_shift != F26Dot6::ZERO {
            for point in outline.points.iter_mut() {
                point.x -= x_shift;
            }
        }
        Ok(())
    }
}

// Loading
impl<'a> Scaler<'a> {
    fn load_glyph(
        &mut self,
        glyph_id: GlyphId,
        outline: &mut Outline,
        recurse_depth: usize,
    ) -> Result<()> {
        if recurse_depth > GLYF_COMPOSITE_RECURSION_LIMIT {
            return Err(Error::RecursionLimitExceeded(glyph_id));
        }
        let Some(glyph) = self.font.glyph(glyph_id) else {
            return Err(Error::GlyphNotFound(glyph_id));
        };
        let glyph = match glyph {
            Some(glyph) => glyph,
            // This is a valid empty glyph
            None => return Ok(()),
        };
        let bounds = [glyph.x_min(), glyph.x_max(), glyph.y_min(), glyph.y_max()];
        self.setup_phantom(bounds, glyph_id);
        match glyph {
            Glyph::Simple(simple) => self.load_simple(&simple, glyph_id, outline),
            Glyph::Composite(composite) => {
                self.load_composite(&composite, glyph_id, outline, recurse_depth)
            }
        }
    }

    fn load_simple(
        &mut self,
        simple: &SimpleGlyph,
        glyph_id: GlyphId,
        outline: &mut Outline,
    ) -> Result<()> {
        // The base indices of the points and contours for the current glyph.
        let point_base = outline.points.len();
        let contour_base = outline.contours.len();
        let end_pts = simple.end_pts_of_contours();
        let contour_count = end_pts.len();
        let contour_end = contour_base + contour_count;
        outline
            .contours
            .extend(end_pts.iter().map(|end_pt| end_pt.get()));
        let mut point_count = simple.num_points();
        outline
            .flags
            .resize(outline.flags.len() + point_count, Default::default());
        self.context.unscaled.clear();
        self.context.unscaled.reserve(point_count + 4);
        self.context.unscaled.resize(point_count, Point::default());
        simple.read_points_fast(
            &mut self.context.unscaled[..],
            &mut outline.flags[point_base..],
        )?;
        #[cfg(feature = "hinting")]
        let ins = simple.instructions();
        for point in &self.phantom {
            self.context
                .unscaled
                .push(Point::new(point.x.to_bits(), point.y.to_bits()));
            outline.flags.push(Default::default());
        }
        point_count += 4;
        let point_end = point_base + point_count;
        outline.points.resize(point_end, Point::default());
        // Compute deltas, but don't apply them yet.
        let mut have_deltas = false;
        if self.font.has_variations() {
            let gvar = self.font.gvar.clone().unwrap();
            let deltas = &mut self.context.deltas;
            let working_points = &mut self.context.working_points;
            deltas.clear();
            deltas.resize(point_count, Default::default());
            working_points.clear();
            working_points.resize(point_count, Default::default());
            let glyph = super::deltas::SimpleGlyph {
                points: &self.context.unscaled,
                flags: &mut outline.flags[point_base..],
                contours: &outline.contours[contour_base..],
            };
            if super::deltas::simple_glyph(
                &gvar,
                glyph_id,
                self.font.coords,
                self.font.has_var_lsb,
                glyph,
                &mut working_points[..],
                &mut deltas[..],
            )
            .is_ok()
            {
                have_deltas = true;
            }
        }
        #[cfg(feature = "hinting")]
        let hinted = self.hint_config.is_enabled() && !ins.is_empty();
        let scale = self.font.scale;
        if self.font.is_scaled {
            if have_deltas {
                for ((point, unscaled), delta) in outline.points[point_base..]
                    .iter_mut()
                    .zip(self.context.unscaled.iter_mut())
                    .zip(&self.context.deltas)
                {
                    let delta = delta.map(Fixed::to_f26dot6);
                    let scaled = (unscaled.map(F26Dot6::from_i32) + delta) * scale;
                    // The computed scale factor has an i32 -> 26.26 conversion built in. This undoes the
                    // extra shift.
                    *point = scaled.map(|v| F26Dot6::from_bits(v.to_i32()));
                }
                #[cfg(feature = "hinting")]
                if hinted {
                    // For hinting, we need to adjust the unscaled points as well.
                    // Round off deltas for unscaled outlines.
                    for (unscaled, delta) in
                        self.context.unscaled.iter_mut().zip(&self.context.deltas)
                    {
                        *unscaled += delta.map(Fixed::to_i32);
                    }
                }
            } else {
                for (point, unscaled) in outline.points[point_base..]
                    .iter_mut()
                    .zip(&self.context.unscaled)
                {
                    *point = unscaled.map(|v| F26Dot6::from_bits(v) * scale);
                }
            }
        } else {
            if have_deltas {
                // Round off deltas for unscaled outlines.
                for (unscaled, delta) in self.context.unscaled.iter_mut().zip(&self.context.deltas)
                {
                    *unscaled += delta.map(Fixed::to_i32);
                }
            }
            // Unlike FreeType, we also store unscaled outlines in 26.6.
            for (point, unscaled) in outline.points[point_base..]
                .iter_mut()
                .zip(&self.context.unscaled)
            {
                *point = unscaled.map(F26Dot6::from_i32);
            }
        }
        // Save the phantom points.
        self.save_phantom(outline, point_base, point_count);
        #[cfg(feature = "hinting")]
        if hinted {
            // Hinting requires a copy of the scaled points. These are used
            // as references when modifying an outline.
            self.context.original.clear();
            self.context
                .original
                .extend_from_slice(&outline.points[point_base..point_end]);
            // When hinting, round the components of the phantom points.
            for point in &mut outline.points[point_end - 4..] {
                point.x = point.x.round();
                point.y = point.y.round();
            }
            // Apply hinting to the set of contours for this outline.
            if !self.hint(outline, point_base, contour_base, ins, false) {
                return Err(Error::HintingFailed(glyph_id));
            }
        }
        if point_base != 0 {
            // If we're not the first component, shift our contour end points.
            for contour_end in &mut outline.contours[contour_base..contour_end] {
                *contour_end += point_base as u16;
            }
        }
        // We're done with the phantom points, so drop them.
        self.drop_phantom(outline);
        Ok(())
    }

    fn load_composite(
        &mut self,
        composite: &CompositeGlyph,
        glyph_id: GlyphId,
        outline: &mut Outline,
        recurse_depth: usize,
    ) -> Result<()> {
        // The base indices of the points and contours for the current glyph.
        let point_base = outline.points.len();
        #[cfg(feature = "hinting")]
        let contour_base = outline.contours.len();
        let scale = self.font.scale;
        if self.font.is_scaled {
            for point in self.phantom.iter_mut() {
                *point *= scale;
            }
        } else {
            for point in self.phantom.iter_mut() {
                *point = point.map(|x| F26Dot6::from_i32(x.to_bits()));
            }
        }
        // Compute the per component deltas. Since composites can be nested, we
        // use a stack and keep track of the base.
        let mut have_deltas = false;
        let delta_base = self.context.composite_deltas.len();
        if self.font.has_variations() {
            let gvar = self.font.gvar.as_ref().unwrap();
            let count = composite.components().count() + 4;
            let deltas = &mut self.context.composite_deltas;
            deltas.resize(delta_base + count, Default::default());
            if super::deltas::composite_glyph(
                gvar,
                glyph_id,
                self.font.coords,
                &mut deltas[delta_base..],
            )
            .is_ok()
            {
                // If the font is missing variation data for LSBs in HVAR then we
                // apply the delta to the first phantom point.
                if !self.font.has_var_lsb {
                    self.phantom[0].x +=
                        F26Dot6::from_bits(deltas[delta_base + count - 4].x.to_i32());
                }
                have_deltas = true;
            }
        }
        for (i, component) in composite.components().enumerate() {
            // Loading a component glyph will override phantom points so save a copy. We'll
            // restore them unless the USE_MY_METRICS flag is set.
            let phantom = self.phantom;
            // Load the component glyph and keep track of the points range.
            let start_point = outline.points.len();
            self.load_glyph(component.glyph, outline, recurse_depth + 1)?;
            let end_point = outline.points.len();
            if !component
                .flags
                .contains(CompositeGlyphFlags::USE_MY_METRICS)
            {
                // If the USE_MY_METRICS flag is missing, we restore the phantom points we
                // saved at the start of the loop.
                self.phantom = phantom;
            }
            // Prepares the transform components for our conversion math below.
            fn scale_component(x: F2Dot14) -> F26Dot6 {
                F26Dot6::from_bits(x.to_bits() as i32 * 4)
            }
            let xform = &component.transform;
            let xx = scale_component(xform.xx);
            let yx = scale_component(xform.yx);
            let xy = scale_component(xform.xy);
            let yy = scale_component(xform.yy);
            let have_xform = component.flags.intersects(
                CompositeGlyphFlags::WE_HAVE_A_SCALE
                    | CompositeGlyphFlags::WE_HAVE_AN_X_AND_Y_SCALE
                    | CompositeGlyphFlags::WE_HAVE_A_TWO_BY_TWO,
            );
            if have_xform {
                if self.font.is_scaled {
                    for point in &mut outline.points[start_point..end_point] {
                        let x = point.x * xx + point.y * xy;
                        let y = point.x * yx + point.y * yy;
                        point.x = x;
                        point.y = y;
                    }
                } else {
                    for point in &mut outline.points[start_point..end_point] {
                        // This juggling is necessary because, unlike FreeType, we also
                        // return unscaled outlines in 26.6 format for a consistent interface.
                        let unscaled = point.map(|c| F26Dot6::from_bits(c.to_i32()));
                        let x = unscaled.x * xx + unscaled.y * xy;
                        let y = unscaled.x * yx + unscaled.y * yy;
                        *point = Point::new(x, y).map(|c| F26Dot6::from_i32(c.to_bits()));
                    }
                }
            }
            let anchor_offset = match component.anchor {
                Anchor::Offset { x, y } => {
                    let (mut x, mut y) = (x as i32, y as i32);
                    if have_xform
                        && component.flags
                            & (CompositeGlyphFlags::SCALED_COMPONENT_OFFSET
                                | CompositeGlyphFlags::UNSCALED_COMPONENT_OFFSET)
                            == CompositeGlyphFlags::SCALED_COMPONENT_OFFSET
                    {
                        // According to FreeType, this algorithm is a "guess" and
                        // works better than the one documented by Apple.
                        // https://github.com/freetype/freetype/blob/b1c90733ee6a04882b133101d61b12e352eeb290/src/truetype/ttgload.c#L1259
                        fn hypot(a: F26Dot6, b: F26Dot6) -> Fixed {
                            let a = a.to_bits().abs();
                            let b = b.to_bits().abs();
                            Fixed::from_bits(if a > b {
                                a + ((3 * b) >> 3)
                            } else {
                                b + ((3 * a) >> 3)
                            })
                        }
                        // FreeType uses a fixed point multiplication here.
                        x = (Fixed::from_bits(x) * hypot(xx, xy)).to_bits();
                        y = (Fixed::from_bits(y) * hypot(yy, yx)).to_bits();
                    }
                    if have_deltas {
                        let delta = self
                            .context
                            .composite_deltas
                            .get(delta_base + i)
                            .copied()
                            .unwrap_or_default();
                        // For composite glyphs, we copy FreeType and round off the fractional parts of deltas.
                        x += delta.x.to_i32();
                        y += delta.y.to_i32();
                    }
                    if self.font.is_scaled {
                        // This only needs to be mutable when hinting is enabled. Ignore the warning.
                        #[allow(unused_mut)]
                        let mut offset = Point::new(x, y).map(F26Dot6::from_bits) * scale;
                        #[cfg(feature = "hinting")]
                        if self.hint_config.is_enabled()
                            && component
                                .flags
                                .contains(CompositeGlyphFlags::ROUND_XY_TO_GRID)
                        {
                            // Only round the y-coordinate, per FreeType.
                            offset.y = offset.y.round();
                        }
                        offset
                    } else {
                        Point::new(x, y).map(F26Dot6::from_i32)
                    }
                }
                Anchor::Point { base, component } => {
                    let (base_offset, component_offset) = (base as usize, component as usize);
                    let base_point = outline
                        .points
                        .get(point_base + base_offset)
                        .ok_or(Error::InvalidAnchorPoint(glyph_id, base))?;
                    let component_point = outline
                        .points
                        .get(start_point + component_offset)
                        .ok_or(Error::InvalidAnchorPoint(glyph_id, component))?;
                    *base_point - *component_point
                }
            };
            if anchor_offset.x != F26Dot6::ZERO || anchor_offset.y != F26Dot6::ZERO {
                for point in &mut outline.points[start_point..end_point] {
                    *point += anchor_offset;
                }
            }
        }
        if have_deltas {
            self.context.composite_deltas.truncate(delta_base);
        }
        #[cfg(feature = "hinting")]
        if self.hint_config.is_enabled() {
            let ins = composite.instructions().unwrap_or_default();
            if !ins.is_empty() {
                // Append the current phantom points to the outline.
                self.push_phantom(outline);
                // For composite glyphs, the unscaled and original points are simply
                // copies of the current point set.
                self.context.unscaled.clear();
                self.context.unscaled.extend(
                    outline.points[point_base..]
                        .iter()
                        .map(|point| Point::new(point.x.to_bits(), point.y.to_bits())),
                );
                self.context.original.clear();
                self.context
                    .original
                    .extend_from_slice(&outline.points[point_base..]);
                let point_end = outline.points.len();
                // Round the phantom points.
                for p in &mut outline.points[point_end - 4..] {
                    p.x = p.x.round();
                    p.y = p.y.round();
                }
                // Clear the "touched" flags that are used during IUP processing.
                for flag in &mut outline.flags[point_base..] {
                    flag.clear_marker(PointMarker::TOUCHED);
                }
                if !self.hint(outline, point_base, contour_base, ins, true) {
                    return Err(Error::HintingFailed(glyph_id));
                }
                // As in simple outlines, drop the phantom points.
                self.drop_phantom(outline);
            }
        }
        Ok(())
    }
}

// Phantom point management.
impl<'a> Scaler<'a> {
    fn setup_phantom(&mut self, bounds: [i16; 4], glyph_id: GlyphId) {
        let font = &self.font;
        let lsb = font.lsb(glyph_id);
        let advance = font.advance_width(glyph_id);
        // Vertical metrics aren't significant to the glyph loading process, so
        // they are ignored.
        let vadvance = 0;
        let tsb = 0;
        // The four "phantom" points as computed by FreeType.
        self.phantom[0].x = F26Dot6::from_bits(bounds[0] as i32 - lsb);
        self.phantom[0].y = F26Dot6::ZERO;
        self.phantom[1].x = self.phantom[0].x + F26Dot6::from_bits(advance);
        self.phantom[1].y = F26Dot6::ZERO;
        self.phantom[2].x = F26Dot6::from_bits(advance / 2);
        self.phantom[2].y = F26Dot6::from_bits(bounds[3] as i32 + tsb);
        self.phantom[3].x = F26Dot6::from_bits(advance / 2);
        self.phantom[3].y = self.phantom[2].y - F26Dot6::from_bits(vadvance);
    }

    #[cfg(feature = "hinting")]
    fn push_phantom(&mut self, outline: &mut Outline) {
        for i in 0..4 {
            outline.points.push(self.phantom[i]);
            outline.flags.push(Default::default());
        }
    }

    fn save_phantom(&mut self, outline: &mut Outline, point_base: usize, point_count: usize) {
        for i in 0..4 {
            self.phantom[3 - i] = outline.points[point_base + point_count - i - 1];
        }
    }

    fn drop_phantom(&self, outline: &mut Outline) {
        outline.points.truncate(outline.points.len() - 4);
        outline.flags.truncate(outline.flags.len() - 4);
    }
}

// Hinting
#[cfg(feature = "hinting")]
impl<'a> Scaler<'a> {
    fn hint(
        &mut self,
        outline: &mut Outline,
        point_base: usize,
        contour_base: usize,
        ins: &[u8],
        is_composite: bool,
    ) -> bool {
        let glyph = hint::HintGlyph {
            font: &self.font,
            config: &mut self.hint_config,
            points: &mut outline.points[..],
            original: &mut self.context.original[..],
            unscaled: &mut self.context.unscaled[..],
            flags: &mut outline.flags[..],
            contours: &mut outline.contours[..],
            phantom: &mut self.phantom[..],
            point_base,
            contour_base,
            ins,
            is_composite,
        };
        self.context.hint_context.hint(glyph)
    }
}

/// Representation of a font instance for the TrueType scaler.
///
/// Contains unique id, size, variation coordinates and the necessary
/// table references for loading, scaling and hinting a glyph outline.
#[derive(Clone)]
pub struct ScalerFont<'a> {
    pub id: Option<UniqueId>,
    pub is_scaled: bool,
    pub ppem: u16,
    pub scale: F26Dot6,
    pub coords: &'a [NormalizedCoord],
    pub glyf: Glyf<'a>,
    pub loca: Loca<'a>,
    pub gvar: Option<Gvar<'a>>,
    pub hmtx: Hmtx<'a>,
    pub hvar: Option<Hvar<'a>>,
    pub fpgm: &'a [u8],
    pub prep: &'a [u8],
    pub cvt: &'a [BigEndian<i16>],
    pub units_per_em: u16,
    pub glyph_count: u16,
    pub max_storage: u16,
    pub max_stack: u16,
    pub max_function_defs: u16,
    pub max_instruction_defs: u16,
    pub max_twilight: u16,
    pub axis_count: u16,
    pub has_var_lsb: bool,
}

impl<'a> ScalerFont<'a> {
    fn new(
        font: &impl TableProvider<'a>,
        id: Option<UniqueId>,
        size: f32,
        coords: &'a [NormalizedCoord],
    ) -> Result<Self> {
        let glyf = font.glyf()?;
        let loca = font.loca(None)?;
        let gvar = font.gvar().ok();
        let hmtx = font.hmtx()?;
        let hvar = font.hvar().ok();
        let units_per_em = font.head()?.units_per_em();
        let size = size.abs();
        let ppem = size as u16;
        let (is_scaled, scale) = if size != 0. && units_per_em != 0 {
            (
                true,
                F26Dot6::from_bits((size * 64.) as i32) / F26Dot6::from_bits(units_per_em as i32),
            )
        } else {
            (false, F26Dot6::from_bits(0x10000))
        };
        let fpgm = font
            .data_for_tag(Tag::new(b"fpgm"))
            .map(|data| data.read_array(0..data.len()).unwrap())
            .unwrap_or_default();
        let prep = font
            .data_for_tag(Tag::new(b"prep"))
            .map(|data| data.read_array(0..data.len()).unwrap())
            .unwrap_or_default();
        let cvt = font
            .data_for_tag(Tag::new(b"cvt "))
            .and_then(|data| data.read_array(0..data.len()).ok())
            .unwrap_or_default();
        let maxp = font.maxp()?;
        let glyph_count = maxp.num_glyphs();
        let axis_count = font.fvar().map(|fvar| fvar.axis_count()).unwrap_or(0);
        let has_var_lsb = hvar
            .as_ref()
            .map(|hvar| hvar.lsb_mapping().is_some())
            .unwrap_or_default();
        Ok(Self {
            id,
            is_scaled,
            ppem,
            scale,
            coords,
            glyf,
            loca,
            gvar,
            hmtx,
            hvar,
            fpgm,
            prep,
            cvt,
            glyph_count,
            units_per_em,
            max_storage: maxp.max_storage().unwrap_or(0),
            max_stack: maxp.max_stack_elements().unwrap_or(0),
            max_function_defs: maxp.max_function_defs().unwrap_or(0),
            max_instruction_defs: maxp.max_instruction_defs().unwrap_or(0),
            max_twilight: maxp.max_twilight_points().unwrap_or(0),
            axis_count,
            has_var_lsb,
        })
    }

    fn glyph(&self, gid: GlyphId) -> Option<Option<Glyph<'a>>> {
        self.loca.get_glyf(gid, &self.glyf).ok()
    }

    fn has_variations(&self) -> bool {
        !self.coords.is_empty() && self.gvar.is_some()
    }

    fn advance_width(&self, gid: GlyphId) -> i32 {
        let default_advance = self
            .hmtx
            .h_metrics()
            .last()
            .map(|metric| metric.advance())
            .unwrap_or(0);
        let mut advance = self
            .hmtx
            .h_metrics()
            .get(gid.to_u16() as usize)
            .map(|metric| metric.advance())
            .unwrap_or(default_advance) as i32;
        if let Some(hvar) = &self.hvar {
            advance += hvar
                .advance_width_delta(gid, self.coords)
                // FreeType truncates metric deltas...
                .map(|delta| delta.to_f64() as i32)
                .unwrap_or(0);
        }
        advance
    }

    fn lsb(&self, gid: GlyphId) -> i32 {
        let gid_index = gid.to_u16() as usize;
        let mut lsb = self
            .hmtx
            .h_metrics()
            .get(gid_index)
            .map(|metric| metric.side_bearing())
            .unwrap_or_else(|| {
                self.hmtx
                    .left_side_bearings()
                    .get(gid_index.saturating_sub(self.hmtx.h_metrics().len()))
                    .map(|lsb| lsb.get())
                    .unwrap_or(0)
            }) as i32;
        if let Some(hvar) = &self.hvar {
            lsb += hvar
                .lsb_delta(gid, self.coords)
                // FreeType truncates metric deltas...
                .map(|delta| delta.to_f64() as i32)
                .unwrap_or(0);
        }
        lsb
    }

    #[cfg(feature = "hinting")]
    #[allow(dead_code)]
    pub(crate) fn scale_cvt(&self, scale: Option<i32>, scaled_cvt: &mut Vec<i32>) {
        if scaled_cvt.len() < self.cvt.len() {
            scaled_cvt.resize(self.cvt.len(), 0);
        }
        for (src, dest) in self.cvt.iter().zip(scaled_cvt.iter_mut()) {
            *dest = src.get() as i32 * 64;
        }
        if let Some(scale) = scale {
            let scale = F26Dot6::from_bits(scale >> 6);
            for value in &mut scaled_cvt[..] {
                *value = (F26Dot6::from_bits(*value) * scale).to_bits();
            }
        }
    }
}
