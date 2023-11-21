//! Scaling support for TrueType outlines.

mod deltas;
mod hint;
mod mem;
mod outline;

pub use hint::HinterOutline;
pub use mem::OutlineMemory;
pub use outline::{Outline, ScaledOutline};

use super::{Error, Hinting};

use read_fonts::{
    tables::{
        glyf::{
            Anchor, CompositeGlyph, CompositeGlyphFlags, Glyf, Glyph, PointMarker, SimpleGlyph,
        },
        gvar::Gvar,
        hmtx::Hmtx,
        hvar::Hvar,
        loca::Loca,
    },
    types::{BigEndian, F26Dot6, F2Dot14, Fixed, GlyphId, Point, Tag},
    TableProvider,
};

/// Recursion limit for processing composite outlines.
///
/// In reality, most fonts contain shallow composite graphs with a nesting
/// depth of 1 or 2. This is set as a hard limit to avoid stack overflow
/// and infinite recursion.
pub const COMPOSITE_RECURSION_LIMIT: usize = 32;

/// Number of phantom points generated at the end of an outline.
pub const PHANTOM_POINT_COUNT: usize = 4;

/// Scaler state for TrueType outlines.
#[derive(Clone)]
pub struct Outlines<'a> {
    loca: Loca<'a>,
    glyf: Glyf<'a>,
    gvar: Option<Gvar<'a>>,
    hmtx: Hmtx<'a>,
    hvar: Option<Hvar<'a>>,
    fpgm: &'a [u8],
    prep: &'a [u8],
    cvt: &'a [BigEndian<i16>],
    units_per_em: u16,
    has_var_lsb: bool,
}

impl<'a> Outlines<'a> {
    pub fn new(font: &impl TableProvider<'a>) -> Option<Self> {
        let hvar = font.hvar().ok();
        let has_var_lsb = hvar
            .as_ref()
            .map(|hvar| hvar.lsb_mapping().is_some())
            .unwrap_or_default();
        Some(Self {
            loca: font.loca(None).ok()?,
            glyf: font.glyf().ok()?,
            gvar: font.gvar().ok(),
            hmtx: font.hmtx().ok()?,
            hvar,
            fpgm: font
                .data_for_tag(Tag::new(b"fpgm"))
                .unwrap_or_default()
                .as_bytes(),
            prep: font
                .data_for_tag(Tag::new(b"prep"))
                .unwrap_or_default()
                .as_bytes(),
            cvt: font
                .data_for_tag(Tag::new(b"cvt "))
                .and_then(|d| d.read_array(0..d.len()).ok())
                .unwrap_or_default(),
            units_per_em: font.head().ok()?.units_per_em(),
            has_var_lsb,
        })
    }

    pub fn outline(&self, glyph_id: GlyphId) -> Result<Outline, Error> {
        let mut info = Outline {
            glyph_id,
            has_variations: self.gvar.is_some(),
            ..Default::default()
        };
        let glyph = self.loca.get_glyf(glyph_id, &self.glyf)?;
        if glyph.is_none() {
            return Ok(info);
        }
        self.glyph_rec(glyph.as_ref().unwrap(), &mut info, 0, 0)?;
        if info.points != 0 {
            info.points += PHANTOM_POINT_COUNT;
        }
        info.glyph = glyph;
        Ok(info)
    }

    pub fn scale(
        &self,
        memory: OutlineMemory<'a>,
        info: &Outline,
        size: f32,
        coords: &'a [F2Dot14],
    ) -> Result<ScaledOutline<'a>, Error> {
        Scaler::new(self.clone(), memory, size, coords, |_| true, false)
            .scale(&info.glyph, info.glyph_id)
    }

    pub fn scale_hinted(
        &self,
        memory: OutlineMemory<'a>,
        info: &Outline,
        size: f32,
        coords: &'a [F2Dot14],
        hint_fn: impl FnMut(HinterOutline) -> bool,
    ) -> Result<ScaledOutline<'a>, Error> {
        Scaler::new(self.clone(), memory, size, coords, hint_fn, false)
            .scale(&info.glyph, info.glyph_id)
    }
}

impl<'a> Outlines<'a> {
    fn glyph_rec(
        &self,
        glyph: &Glyph,
        info: &mut Outline,
        component_depth: usize,
        recurse_depth: usize,
    ) -> Result<(), Error> {
        if recurse_depth > COMPOSITE_RECURSION_LIMIT {
            return Err(Error::RecursionLimitExceeded(info.glyph_id));
        }
        match glyph {
            Glyph::Simple(simple) => {
                let num_points = simple.num_points();
                let num_points_with_phantom = num_points + PHANTOM_POINT_COUNT;
                info.max_simple_points = info.max_simple_points.max(num_points_with_phantom);
                info.points += num_points;
                info.contours += simple.end_pts_of_contours().len();
                info.has_hinting = info.has_hinting || simple.instruction_length() != 0;
                info.max_other_points = info.max_other_points.max(num_points_with_phantom);
                info.has_overlaps |= simple.has_overlapping_contours();
            }
            Glyph::Composite(composite) => {
                let (mut count, instructions) = composite.count_and_instructions();
                count += PHANTOM_POINT_COUNT;
                let point_base = info.points;
                for (component, flags) in composite.component_glyphs_and_flags() {
                    info.has_overlaps |= flags.contains(CompositeGlyphFlags::OVERLAP_COMPOUND);
                    let component_glyph = self.loca.get_glyf(component, &self.glyf)?;
                    let Some(component_glyph) = component_glyph else {
                        continue;
                    };
                    self.glyph_rec(
                        &component_glyph,
                        info,
                        component_depth + count,
                        recurse_depth + 1,
                    )?;
                }
                let has_hinting = !instructions.unwrap_or_default().is_empty();
                if has_hinting {
                    // We only need the "other points" buffers if the
                    // composite glyph has instructions.
                    let num_points_in_composite = info.points - point_base + PHANTOM_POINT_COUNT;
                    info.max_other_points = info.max_other_points.max(num_points_in_composite);
                }
                info.max_component_delta_stack =
                    info.max_component_delta_stack.max(component_depth + count);
                info.has_hinting = info.has_hinting || has_hinting;
            }
        }
        Ok(())
    }

    fn advance_width(&self, gid: GlyphId, coords: &'a [F2Dot14]) -> i32 {
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
                .advance_width_delta(gid, coords)
                // FreeType truncates metric deltas...
                .map(|delta| delta.to_f64() as i32)
                .unwrap_or(0);
        }
        advance
    }

    fn lsb(&self, gid: GlyphId, coords: &'a [F2Dot14]) -> i32 {
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
                .lsb_delta(gid, coords)
                // FreeType truncates metric deltas...
                .map(|delta| delta.to_f64() as i32)
                .unwrap_or(0);
        }
        lsb
    }
}

struct Scaler<'a, H> {
    outlines: Outlines<'a>,
    memory: OutlineMemory<'a>,
    coords: &'a [F2Dot14],
    point_count: usize,
    contour_count: usize,
    component_delta_count: usize,
    scale: F26Dot6,
    is_scaled: bool,
    is_hinted: bool,
    /// Phantom points. These are 4 extra points appended to the end of an
    /// outline that allow the bytecode interpreter to produce hinted
    /// metrics.
    ///
    /// See <https://learn.microsoft.com/en-us/typography/opentype/spec/tt_instructing_glyphs#phantom-points>
    phantom: [Point<F26Dot6>; PHANTOM_POINT_COUNT],
    hint_fn: H,
}

impl<'a, H> Scaler<'a, H>
where
    H: FnMut(HinterOutline) -> bool,
{
    fn new(
        outlines: Outlines<'a>,
        memory: OutlineMemory<'a>,
        size: f32,
        coords: &'a [F2Dot14],
        hint_fn: H,
        is_hinted: bool,
    ) -> Self {
        let (is_scaled, scale) = if size > 0.0 && outlines.units_per_em > 0 {
            (
                true,
                F26Dot6::from_bits((size * 64.) as i32)
                    / F26Dot6::from_bits(outlines.units_per_em as i32),
            )
        } else {
            (false, F26Dot6::from_bits(0x10000))
        };
        Self {
            outlines,
            memory,
            coords,
            point_count: 0,
            contour_count: 0,
            component_delta_count: 0,
            scale,
            is_scaled,
            // We don't hint unscaled outlines
            is_hinted: is_hinted && is_scaled,
            phantom: Default::default(),
            hint_fn,
        }
    }

    fn scale(
        mut self,
        glyph: &Option<Glyph>,
        glyph_id: GlyphId,
    ) -> Result<ScaledOutline<'a>, Error> {
        self.load(glyph, glyph_id, 0)?;
        let outline = ScaledOutline {
            points: &mut self.memory.scaled[..self.point_count],
            flags: &mut self.memory.flags[..self.point_count],
            contours: &mut self.memory.contours[..self.contour_count],
        };
        let x_shift = self.phantom[0].x;
        if x_shift != F26Dot6::ZERO {
            for point in outline.points.iter_mut() {
                point.x -= x_shift;
            }
        }
        Ok(outline)
    }

    fn load(
        &mut self,
        glyph: &Option<Glyph>,
        glyph_id: GlyphId,
        recurse_depth: usize,
    ) -> Result<(), Error> {
        if recurse_depth > COMPOSITE_RECURSION_LIMIT {
            return Err(Error::RecursionLimitExceeded(glyph_id));
        }
        let glyph = match &glyph {
            Some(glyph) => glyph,
            // This is a valid empty glyph
            None => return Ok(()),
        };
        let bounds = [glyph.x_min(), glyph.x_max(), glyph.y_min(), glyph.y_max()];
        self.setup_phantom_points(bounds, glyph_id);
        match glyph {
            Glyph::Simple(simple) => self.load_simple(simple, glyph_id),
            Glyph::Composite(composite) => self.load_composite(composite, glyph_id, recurse_depth),
        }
    }

    fn load_simple(&mut self, glyph: &SimpleGlyph, glyph_id: GlyphId) -> Result<(), Error> {
        use Error::InsufficientMemory;
        // Compute the ranges for our point/flag buffers and slice them.
        let points_start = self.point_count;
        let point_count = glyph.num_points();
        let phantom_start = point_count;
        let points_end = points_start + point_count + PHANTOM_POINT_COUNT;
        let point_range = points_start..points_end;
        let other_points_end = point_count + PHANTOM_POINT_COUNT;
        // Scaled points and flags are accumulated as we load the outline.
        let scaled = self
            .memory
            .scaled
            .get_mut(point_range.clone())
            .ok_or(InsufficientMemory)?;
        let flags = self
            .memory
            .flags
            .get_mut(point_range)
            .ok_or(InsufficientMemory)?;
        // Unscaled points are temporary and are allocated as needed. We only
        // ever need one copy in memory for any simple or composite glyph so
        // allocate from the base of the buffer.
        let unscaled = self
            .memory
            .unscaled
            .get_mut(..other_points_end)
            .ok_or(InsufficientMemory)?;
        // Read our unscaled points and flags (up to point_count which does not
        // include phantom points).
        glyph.read_points_fast(&mut unscaled[..point_count], &mut flags[..point_count])?;
        // Compute the range for our contour end point buffer and slice it.
        let contours_start = self.contour_count;
        let contour_end_pts = glyph.end_pts_of_contours();
        let contour_count = contour_end_pts.len();
        let contours_end = contours_start + contour_count;
        let contours = self
            .memory
            .contours
            .get_mut(contours_start..contours_end)
            .ok_or(InsufficientMemory)?;
        // Read the contour end points.
        for (end_pt, contour) in contour_end_pts.iter().zip(contours.iter_mut()) {
            *contour = end_pt.get();
        }
        // Adjust the running point/contour total counts
        self.point_count += point_count;
        self.contour_count += contour_count;
        // Append phantom points to the outline.
        for (i, phantom) in self.phantom.iter().enumerate() {
            unscaled[phantom_start + i] = phantom.map(|x| x.to_bits());
            flags[phantom_start + i] = Default::default();
        }
        let mut have_deltas = false;
        if self.outlines.gvar.is_some() && !self.coords.is_empty() {
            let gvar = self.outlines.gvar.as_ref().unwrap();
            let glyph = deltas::SimpleGlyph {
                points: &mut unscaled[..],
                flags: &mut flags[..],
                contours,
            };
            let deltas = self
                .memory
                .deltas
                .get_mut(..point_count + PHANTOM_POINT_COUNT)
                .ok_or(InsufficientMemory)?;
            let iup_buffer = self
                .memory
                .iup_buffer
                .get_mut(..point_count + PHANTOM_POINT_COUNT)
                .ok_or(InsufficientMemory)?;
            if deltas::simple_glyph(
                gvar,
                glyph_id,
                self.coords,
                self.outlines.has_var_lsb,
                glyph,
                iup_buffer,
                deltas,
            )
            .is_ok()
            {
                have_deltas = true;
            }
        }
        let ins = glyph.instructions();
        let is_hinted = self.is_hinted && !ins.is_empty();
        if self.is_scaled {
            let scale = self.scale;
            if have_deltas {
                for ((point, unscaled), delta) in scaled
                    .iter_mut()
                    .zip(unscaled.iter_mut())
                    .zip(self.memory.deltas.iter())
                {
                    let delta = delta.map(Fixed::to_f26dot6);
                    let scaled = (unscaled.map(F26Dot6::from_i32) + delta) * scale;
                    // The computed scale factor has an i32 -> 26.26 conversion built in. This undoes the
                    // extra shift.
                    *point = scaled.map(|v| F26Dot6::from_bits(v.to_i32()));
                }
                if is_hinted {
                    // For hinting, we need to adjust the unscaled points as well.
                    // Round off deltas for unscaled outlines.
                    for (unscaled, delta) in unscaled.iter_mut().zip(self.memory.deltas.iter()) {
                        *unscaled += delta.map(Fixed::to_i32);
                    }
                }
            } else {
                for (point, unscaled) in scaled.iter_mut().zip(unscaled.iter_mut()) {
                    *point = unscaled.map(|v| F26Dot6::from_bits(v) * scale);
                }
            }
        } else {
            if have_deltas {
                // Round off deltas for unscaled outlines.
                for (unscaled, delta) in unscaled.iter_mut().zip(self.memory.deltas.iter()) {
                    *unscaled += delta.map(Fixed::to_i32);
                }
            }
            // Unlike FreeType, we also store unscaled outlines in 26.6.
            for (point, unscaled) in scaled.iter_mut().zip(unscaled.iter()) {
                *point = unscaled.map(F26Dot6::from_i32);
            }
        }
        // Commit our potentially modified phantom points.
        for (i, point) in scaled[phantom_start..]
            .iter()
            .enumerate()
            .take(PHANTOM_POINT_COUNT)
        {
            self.phantom[i] = *point;
        }
        if is_hinted {
            // Create a copy of our scaled points in original_scaled.
            let original_scaled = self
                .memory
                .original_scaled
                .get_mut(..other_points_end)
                .ok_or(InsufficientMemory)?;
            original_scaled.copy_from_slice(scaled);
            // When hinting, round the phantom points.
            for point in &mut scaled[phantom_start..] {
                point.x = point.x.round();
                point.y = point.y.round();
            }
            if !(self.hint_fn)(HinterOutline {
                unscaled,
                scaled,
                original_scaled,
                flags,
                contours,
                bytecode: ins,
                phantom: &mut self.phantom,
                is_composite: false,
                coords: self.coords,
            }) {
                return Err(Error::HintingFailed(glyph_id));
            }
        }
        if points_start != 0 {
            // If we're not the first component, shift our contour end points.
            for contour_end in contours.iter_mut() {
                *contour_end += points_start as u16;
            }
        }
        Ok(())
    }

    fn load_composite(
        &mut self,
        glyph: &CompositeGlyph,
        glyph_id: GlyphId,
        recurse_depth: usize,
    ) -> Result<(), Error> {
        use Error::InsufficientMemory;
        let scale = self.scale;
        // The base indices of the points and contours for the current glyph.
        let point_base = self.point_count;
        let contour_base = self.contour_count;
        // Compute the per component deltas. Since composites can be nested, we
        // use a stack and keep track of the base.
        let mut have_deltas = false;
        let delta_base = self.component_delta_count;
        if self.outlines.gvar.is_some() && !self.coords.is_empty() {
            let gvar = self.outlines.gvar.as_ref().unwrap();
            let count = glyph.components().count() + PHANTOM_POINT_COUNT;
            let deltas = self
                .memory
                .composite_deltas
                .get_mut(delta_base..delta_base + count)
                .ok_or(InsufficientMemory)?;
            if deltas::composite_glyph(gvar, glyph_id, self.coords, &mut deltas[..]).is_ok() {
                // If the font is missing variation data for LSBs in HVAR then we
                // apply the delta to the first phantom point.
                if !self.outlines.has_var_lsb {
                    self.phantom[0].x += F26Dot6::from_bits(deltas[count - 4].x.to_i32());
                }
                have_deltas = true;
            }
            self.component_delta_count += count;
        }
        if self.is_scaled {
            for point in self.phantom.iter_mut() {
                *point *= scale;
            }
        } else {
            for point in self.phantom.iter_mut() {
                *point = point.map(|x| F26Dot6::from_i32(x.to_bits()));
            }
        }
        for (i, component) in glyph.components().enumerate() {
            // Loading a component glyph will override phantom points so save a copy. We'll
            // restore them unless the USE_MY_METRICS flag is set.
            let phantom = self.phantom;
            // Load the component glyph and keep track of the points range.
            let start_point = self.point_count;
            let component_glyph = self
                .outlines
                .loca
                .get_glyf(component.glyph, &self.outlines.glyf)?;
            self.load(&component_glyph, component.glyph, recurse_depth + 1)?;
            let end_point = self.point_count;
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
                let scaled = &mut self.memory.scaled[start_point..end_point];
                if self.is_scaled {
                    for point in scaled {
                        let x = point.x * xx + point.y * xy;
                        let y = point.x * yx + point.y * yy;
                        point.x = x;
                        point.y = y;
                    }
                } else {
                    for point in scaled {
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
                        // According to FreeType, this algorithm is a "guess"
                        // and works better than the one documented by Apple.
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
                            .memory
                            .composite_deltas
                            .get(delta_base + i)
                            .copied()
                            .unwrap_or_default();
                        // For composite glyphs, we copy FreeType and round off
                        // the fractional parts of deltas.
                        x += delta.x.to_i32();
                        y += delta.y.to_i32();
                    }
                    if self.is_scaled {
                        let mut offset = Point::new(x, y).map(F26Dot6::from_bits) * scale;
                        if self.is_hinted
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
                    let base_point = self
                        .memory
                        .scaled
                        .get(point_base + base_offset)
                        .ok_or(Error::InvalidAnchorPoint(glyph_id, base))?;
                    let component_point = self
                        .memory
                        .scaled
                        .get(start_point + component_offset)
                        .ok_or(Error::InvalidAnchorPoint(glyph_id, component))?;
                    *base_point - *component_point
                }
            };
            if anchor_offset.x != F26Dot6::ZERO || anchor_offset.y != F26Dot6::ZERO {
                for point in &mut self.memory.scaled[start_point..end_point] {
                    *point += anchor_offset;
                }
            }
        }
        if have_deltas {
            self.component_delta_count = delta_base;
        }
        if self.is_hinted {
            let ins = glyph.instructions().unwrap_or_default();
            if !ins.is_empty() {
                let start_point = point_base;
                let end_point = self.point_count + PHANTOM_POINT_COUNT;
                let point_range = start_point..end_point;
                let scaled = &mut self.memory.scaled[point_range.clone()];
                let flags = self
                    .memory
                    .flags
                    .get_mut(point_range.clone())
                    .ok_or(InsufficientMemory)?;
                // Append the current phantom points to the outline.
                let phantom_start = self.point_count;
                for (i, phantom) in self.phantom.iter().enumerate() {
                    scaled[phantom_start + i] = *phantom;
                    flags[phantom_start + i] = Default::default();
                }
                // For composite glyphs, the unscaled and original points are
                // simply copies of the current point set.
                let other_points_end = point_range.len();
                let unscaled = self
                    .memory
                    .unscaled
                    .get_mut(..other_points_end)
                    .ok_or(InsufficientMemory)?;
                for (scaled, unscaled) in scaled.iter().zip(unscaled.iter_mut()) {
                    *unscaled = scaled.map(|x| x.to_bits());
                }
                let original_scaled = self
                    .memory
                    .original_scaled
                    .get_mut(..other_points_end)
                    .ok_or(InsufficientMemory)?;
                original_scaled.copy_from_slice(scaled);
                let contours = self
                    .memory
                    .contours
                    .get_mut(contour_base..self.contour_count)
                    .ok_or(InsufficientMemory)?;
                // Round the phantom points.
                for p in &mut scaled[self.point_count..] {
                    p.x = p.x.round();
                    p.y = p.y.round();
                }
                // Clear the "touched" flags that are used during IUP processing.
                for flag in flags.iter_mut() {
                    flag.clear_marker(PointMarker::TOUCHED);
                }
                // Make sure our contour end points accurately reflect the
                // outline slices.
                if point_base != 0 {
                    let delta = point_base as u16;
                    for contour in contours.iter_mut() {
                        *contour -= delta;
                    }
                }
                if !(self.hint_fn)(HinterOutline {
                    unscaled,
                    scaled,
                    original_scaled,
                    flags,
                    contours,
                    bytecode: ins,
                    phantom: &mut self.phantom,
                    is_composite: true,
                    coords: self.coords,
                }) {
                    return Err(Error::HintingFailed(glyph_id));
                }
                // Undo the contour shifts if we applied them above.
                if point_base != 0 {
                    let delta = point_base as u16;
                    for contour in contours.iter_mut() {
                        *contour += delta;
                    }
                }
            }
        }
        Ok(())
    }
}

impl<'a, H> Scaler<'a, H> {
    fn setup_phantom_points(&mut self, bounds: [i16; 4], glyph_id: GlyphId) {
        let lsb = self.outlines.lsb(glyph_id, self.coords);
        let advance = self.outlines.advance_width(glyph_id, self.coords);
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
}

#[cfg(test)]
mod tests {
    use super::*;
    use read_fonts::{FontRef, TableProvider};

    #[test]
    fn overlap_flags() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let scaler = Outlines::new(&font).unwrap();
        let glyph_count = font.maxp().unwrap().num_glyphs();
        // GID 2 is a composite glyph with the overlap bit on a component
        // GID 3 is a simple glyph with the overlap bit on the first flag
        let expected_gids_with_overlap = vec![2, 3];
        assert_eq!(
            expected_gids_with_overlap,
            (0..glyph_count)
                .filter(|gid| scaler.outline(GlyphId::new(*gid)).unwrap().has_overlaps)
                .collect::<Vec<_>>()
        );
    }
}
