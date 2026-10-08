//! The outline loading pass.
//!
//! Walks a glyph and the composite tree below it, filling the caller's buffers
//! with points, flags and contour end points. The arithmetic belongs to a
//! [`Scale`]; the structure lives here and is shared by every scale.

use super::{
    scale::{Scale, SimplePass},
    Buffers, GlyphZone, Hinter, OutlineContext, OutlineError, OutlinePlan, OutlineRef,
};
use crate::{
    limits::MAX_RECURSION_DEPTH,
    tables::{
        glyf::{
            Anchor, CompositeGlyph, CompositeGlyphFlags, Glyph, PointMarker, SimpleGlyph,
            PHANTOM_POINT_COUNT,
        },
        gvar::DeltaBuffers,
    },
    types::{F2Dot14, GlyphId, Point},
};

/// Builds the outline a plan describes.
///
/// The method on [`OutlinePlan`] is the entry point; this is where the work
/// lives, alongside the pass that does it.
pub(super) fn load<'a, 's, 'buf, S: Scale>(
    plan: &'s OutlinePlan<'a>,
    context: &'a dyn OutlineContext<'a>,
    scale: &'s S,
    buffers: Buffers<'buf, S>,
    hinter: Option<&mut dyn Hinter<S::Coord>>,
) -> Result<OutlineRef<'buf, S::Coord>, OutlineError> {
    let coords = context.coords();
    let varies = plan.applies_variations;
    let is_hinted = hinter.is_some() && scale.is_scaled();
    let mut pass = Pass {
        context,
        scale,
        varies,
        coords,
        has_hvar: context.has_hvar(),
        buffers,
        point_count: 0,
        contour_count: 0,
        component_delta_count: 0,
        phantom: Default::default(),
        hinter,
        is_hinted,
    };
    pass.load(&plan.glyph_data, plan.glyph, 0)?;
    Ok(pass.finish())
}

/// Slicing that reports a caller buffer sized too small, rather than panicking
/// or silently truncating.
trait TrySlice<T> {
    fn try_mut<I>(&mut self, index: I) -> Result<&mut [T], OutlineError>
    where
        I: core::slice::SliceIndex<[T], Output = [T]>;

    fn try_ref<I>(&self, index: I) -> Result<&[T], OutlineError>
    where
        I: core::slice::SliceIndex<[T], Output = [T]>;
}

impl<T> TrySlice<T> for [T] {
    fn try_mut<I>(&mut self, index: I) -> Result<&mut [T], OutlineError>
    where
        I: core::slice::SliceIndex<[T], Output = [T]>,
    {
        self.get_mut(index).ok_or(OutlineError::InsufficientMemory)
    }

    fn try_ref<I>(&self, index: I) -> Result<&[T], OutlineError>
    where
        I: core::slice::SliceIndex<[T], Output = [T]>,
    {
        self.get(index).ok_or(OutlineError::InsufficientMemory)
    }
}

struct Pass<'a, 's, 'buf, 'hint, S: Scale> {
    context: &'a dyn OutlineContext<'a>,
    scale: &'s S,
    varies: bool,
    coords: &'s [F2Dot14],
    has_hvar: bool,
    buffers: Buffers<'buf, S>,
    point_count: usize,
    contour_count: usize,
    component_delta_count: usize,
    phantom: [Point<S::Coord>; PHANTOM_POINT_COUNT],
    hinter: Option<&'hint mut dyn Hinter<S::Coord>>,
    is_hinted: bool,
}

impl<'a, 's, 'buf, S: Scale> Pass<'a, 's, 'buf, '_, S> {
    fn finish(self) -> OutlineRef<'buf, S::Coord> {
        let Self {
            buffers,
            point_count,
            contour_count,
            phantom,
            ..
        } = self;
        let points = &mut buffers.points[..point_count];
        // The outline is positioned relative to the left side bearing.
        let x_shift = phantom[0].x;
        if x_shift != S::Coord::default() {
            for point in points.iter_mut() {
                point.x = point.x - x_shift;
            }
        }
        OutlineRef {
            points,
            flags: &buffers.flags[..point_count],
            contours: &buffers.contours[..contour_count],
            phantom,
        }
    }

    fn load(
        &mut self,
        glyph: &Option<Glyph<'a>>,
        gid: GlyphId,
        recurse_depth: usize,
    ) -> Result<(), OutlineError> {
        // `OutlinePlan::new` checks this too, but it is a separate argument
        // to `load`, so nothing makes the tree walked here the one surveyed.
        if recurse_depth > MAX_RECURSION_DEPTH {
            return Err(OutlineError::RecursionLimitExceeded);
        }
        self.setup_phantom_points(glyph, gid);
        match glyph {
            Some(Glyph::Simple(simple)) => self.load_simple(simple, gid),
            Some(Glyph::Composite(composite)) => self.load_composite(composite, gid, recurse_depth),
            None => self.load_empty(gid),
        }
    }

    /// Sets up the four points appended to every outline that carry its
    /// metrics.
    ///
    /// See <https://learn.microsoft.com/en-us/typography/opentype/spec/tt_instructing_glyphs#phantom-points>
    /// and <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/truetype/ttgload.c#L1365>
    fn setup_phantom_points(&mut self, glyph: &Option<Glyph<'a>>, gid: GlyphId) {
        let (x_min, y_max) = match glyph {
            Some(glyph) => (glyph.x_min() as i32, glyph.y_max() as i32),
            None => (0, 0),
        };
        let (lsb, advance) = self.context.h_metrics(gid);
        let (tsb, vadvance) = self.context.v_metrics(gid).unwrap_or_else(|| {
            let (ascent, descent) = self.context.h_line_metrics();
            (ascent - y_max, ascent - descent)
        });
        let from = |value| self.scale.phantom_from_font_units(value);
        // horizontal:
        self.phantom[0] = Point::new(from(x_min - lsb), from(0));
        self.phantom[1] = Point::new(from(x_min - lsb + advance), from(0));
        // vertical:
        self.phantom[2] = Point::new(from(0), from(y_max + tsb));
        self.phantom[3] = Point::new(from(0), from(y_max + tsb - vadvance));
    }

    fn load_empty(&mut self, gid: GlyphId) -> Result<(), OutlineError> {
        let deltas = if self.varies {
            self.context.phantom_point_deltas(gid)
        } else {
            None
        };
        self.scale.load_empty_phantom(&mut self.phantom, deltas);
        Ok(())
    }

    /// Copies a glyph's contour end points into the buffer, checking that they
    /// ascend, and returns the range they occupy.
    ///
    /// They stay relative to the glyph until its points have been loaded; see
    /// the end of [`Self::load_simple`].
    fn read_contours(
        &mut self,
        glyph: &SimpleGlyph<'a>,
    ) -> Result<core::ops::Range<usize>, OutlineError> {
        let end_pts = glyph.end_pts_of_contours();
        let range = self.contour_count..self.contour_count + end_pts.len();
        let contours = self.buffers.contours.try_mut(range.clone())?;
        let mut previous = 0;
        for (end_pt, contour) in end_pts.iter().zip(contours.iter_mut()) {
            let end_pt = end_pt.get();
            if end_pt < previous {
                // Unordered contour end points.
                return Err(OutlineError::Malformed);
            }
            previous = end_pt;
            *contour = end_pt;
        }
        Ok(range)
    }

    fn load_simple(&mut self, glyph: &SimpleGlyph<'a>, gid: GlyphId) -> Result<(), OutlineError> {
        let points_start = self.point_count;
        let point_count = glyph.num_points();
        let with_phantom = point_count + PHANTOM_POINT_COUNT;
        let point_range = points_start..points_start + with_phantom;

        // Contour end points come first, so the delta pass can see them.
        let contours_range = self.read_contours(glyph)?;

        // Copied out so that the buffer borrows below do not conflict with
        // reading the rest of `self`.
        // Looked up here rather than held on the pass: a composite's
        // components each have their own variation data.
        let var_data = if self.varies {
            self.context.glyph_variation_data(gid)
        } else {
            None
        };
        let (is_hinted, phantom, coords, has_hvar, scale) = (
            self.is_hinted,
            self.phantom,
            self.coords,
            self.has_hvar,
            self.scale,
        );

        let scaled = self.buffers.points.try_mut(point_range.clone())?;
        let flags = self.buffers.flags.try_mut(point_range)?;
        let unscaled = if S::NEEDS_UNSCALED {
            self.buffers.unscaled.try_mut(..with_phantom)?
        } else {
            &mut []
        };
        let deltas = if self.varies {
            DeltaBuffers {
                deltas: self.buffers.deltas.try_mut(..with_phantom)?,
                iup: self.buffers.iup.try_mut(..with_phantom)?,
            }
        } else {
            DeltaBuffers {
                deltas: &mut [],
                iup: &mut [],
            }
        };
        let contours = self.buffers.contours.try_ref(contours_range.clone())?;

        self.phantom = scale.load_simple_points(SimplePass {
            glyph,
            scaled,
            unscaled,
            flags,
            contours,
            phantom,
            deltas,
            var_data,
            varies: self.varies,
            coords,
            has_hvar,
            is_hinted,
        })?;

        self.point_count += point_count;
        self.contour_count += contours_range.len();

        self.hint_glyph(
            glyph.instructions(),
            points_start,
            contours_range.clone(),
            point_count,
            gid,
            false,
        )?;

        if points_start != 0 {
            // Contour end points are read relative to this glyph, so shift
            // them into the outline's coordinate space.
            let contours = &mut self.buffers.contours[contours_range];
            for contour_end in contours.iter_mut() {
                *contour_end += points_start as u16;
            }
        }
        Ok(())
    }

    /// Runs the hinter over the points just written, if there are instructions
    /// and hinting is active.
    ///
    /// `points_start` is where this glyph's points begin in the outline, and
    /// `phantom_offset` is where its phantom points begin within them.
    fn hint_glyph(
        &mut self,
        bytecode: &'a [u8],
        points_start: usize,
        contours_range: core::ops::Range<usize>,
        phantom_offset: usize,
        gid: GlyphId,
        is_composite: bool,
    ) -> Result<(), OutlineError> {
        if !self.is_hinted {
            return Ok(());
        }
        let Some(hinter) = self.hinter.as_deref_mut() else {
            return Ok(());
        };
        let point_range = points_start..self.point_count + PHANTOM_POINT_COUNT;
        if bytecode.is_empty() {
            // Even without instructions FreeType uses rounded phantom points
            // when hinting is requested and backward compatibility is off.
            // It never does this for composites.
            if !is_composite && !hinter.backward_compatibility() {
                let scaled = &self.buffers.points[point_range.clone()];
                for (scaled, phantom) in scaled[phantom_offset..].iter().zip(&mut self.phantom) {
                    *phantom = self.scale.round_phantom(*scaled);
                }
            }
            return Ok(());
        }
        let scaled = self.buffers.points.try_mut(point_range.clone())?;
        let flags = self.buffers.flags.try_mut(point_range)?;
        let unscaled = self.buffers.unscaled.try_mut(..scaled.len())?;
        if is_composite {
            // A composite has no font unit points of its own, so the hinter
            // gets a copy of the loaded ones. Its phantom points are
            // appended here too, and the touched markers left by the
            // components are cleared.
            for (i, phantom) in self.phantom.iter().enumerate() {
                scaled[phantom_offset + i] = *phantom;
                flags[phantom_offset + i] = Default::default();
            }
            for (unscaled, scaled) in unscaled.iter_mut().zip(scaled.iter()) {
                *unscaled = Point::new(
                    self.scale.coord_to_hinting_raw(scaled.x),
                    self.scale.coord_to_hinting_raw(scaled.y),
                );
            }
            for flag in flags.iter_mut() {
                flag.clear_marker(PointMarker::TOUCHED);
            }
        }
        // Hinting rounds the phantom points.
        for point in &mut scaled[phantom_offset..] {
            *point = self.scale.round_phantom(*point);
        }
        // The hinter reads contour end points relative to the glyph it is
        // given, but they are stored relative to the whole outline. Rebase them
        // for the call and put them back afterwards. Only a composite can start
        // anywhere but zero.
        let rebase = if is_composite { points_start as u16 } else { 0 };
        if rebase != 0 {
            for contour in self.buffers.contours.try_mut(contours_range.clone())? {
                *contour -= rebase;
            }
        }
        let result = {
            let contours = self.buffers.contours.try_ref(contours_range.clone())?;
            let mut zone = GlyphZone {
                glyph: gid,
                bytecode,
                points: scaled,
                unscaled,
                flags,
                contours,
                phantom: &mut self.phantom,
                is_composite,
                coords: self.coords,
            };
            hinter.hint(&mut zone)
        };
        if rebase != 0 {
            for contour in self.buffers.contours.try_mut(contours_range)? {
                *contour += rebase;
            }
        }
        Ok(result?)
    }

    fn load_composite(
        &mut self,
        glyph: &CompositeGlyph<'a>,
        gid: GlyphId,
        recurse_depth: usize,
    ) -> Result<(), OutlineError> {
        let point_base = self.point_count;
        let contour_base = self.contour_count;
        let delta_base = self.component_delta_count;
        let mut have_deltas = false;
        // One parse of the component list gives both the count the delta stack
        // needs and the instructions used at the end.
        let (component_count, instructions) = glyph.count_and_instructions();
        let var_data = if self.varies {
            self.context.glyph_variation_data(gid)
        } else {
            None
        };
        if let Some(var_data) = var_data {
            let count = component_count + PHANTOM_POINT_COUNT;
            let deltas = self
                .buffers
                .composite_deltas
                .try_mut(delta_base..delta_base + count)?;
            if var_data.composite_deltas(self.coords, deltas).is_some() {
                for (phantom, delta) in self
                    .phantom
                    .iter_mut()
                    .zip(&deltas[count - PHANTOM_POINT_COUNT..])
                {
                    *phantom = self.scale.add_phantom_delta(*phantom, *delta);
                }
                have_deltas = true;
            }
            self.component_delta_count += count;
        }
        for point in self.phantom.iter_mut() {
            *point = self.scale.scale_phantom(*point);
        }
        for (i, component) in glyph.components().enumerate() {
            // Loading a component overwrites the phantom points, so save them
            // and restore unless the component says to keep its own.
            let phantom = self.phantom;
            let start_point = self.point_count;
            let component_glyph = self
                .context
                .glyph(component.glyph.into())
                .ok_or(OutlineError::MissingGlyph)?
                .into_glyph();
            self.load(&component_glyph, component.glyph.into(), recurse_depth + 1)?;
            let end_point = self.point_count;
            if !component
                .flags
                .contains(CompositeGlyphFlags::USE_MY_METRICS)
            {
                self.phantom = phantom;
            }
            let have_xform = component.flags.intersects(
                CompositeGlyphFlags::WE_HAVE_A_SCALE
                    | CompositeGlyphFlags::WE_HAVE_AN_X_AND_Y_SCALE
                    | CompositeGlyphFlags::WE_HAVE_A_TWO_BY_TWO,
            );
            if have_xform {
                let points = &mut self.buffers.points[start_point..end_point];
                self.scale.transform_points(points, &component.transform);
            }
            let offset = match component.anchor {
                Anchor::Offset { x, y } => {
                    let scale_offset = have_xform
                        && component.flags
                            & (CompositeGlyphFlags::SCALED_COMPONENT_OFFSET
                                | CompositeGlyphFlags::UNSCALED_COMPONENT_OFFSET)
                            == CompositeGlyphFlags::SCALED_COMPONENT_OFFSET;
                    let delta = have_deltas
                        .then(|| self.buffers.composite_deltas.get(delta_base + i).copied())
                        .flatten();
                    let round_y = self.is_hinted
                        && component
                            .flags
                            .contains(CompositeGlyphFlags::ROUND_XY_TO_GRID);
                    self.scale.component_offset(
                        Point::new(x as i32, y as i32),
                        &component.transform,
                        scale_offset,
                        delta,
                        round_y,
                    )
                }
                Anchor::Point { base, component } => {
                    let base_point = *self
                        .buffers
                        .points
                        .get(point_base + base as usize)
                        .ok_or(OutlineError::InvalidAnchorPoint)?;
                    let component_point = *self
                        .buffers
                        .points
                        .get(start_point + component as usize)
                        .ok_or(OutlineError::InvalidAnchorPoint)?;
                    base_point - component_point
                }
            };
            if offset.x != S::Coord::default() || offset.y != S::Coord::default() {
                for point in &mut self.buffers.points[start_point..end_point] {
                    *point += offset;
                }
            }
        }
        // Unconditional: the reservation above happens whenever this glyph has
        // variation data, but `have_deltas` is only set when reading it
        // succeeded. Guarding the restore leaks the reservation on malformed
        // data, and a later composite then starts from a higher base.
        self.component_delta_count = delta_base;
        // The hook decides whether there is anything to do; a composite with
        // no instructions, or a draw with no hinter, costs nothing here.
        self.hint_glyph(
            instructions.unwrap_or_default(),
            point_base,
            contour_base..self.contour_count,
            self.point_count - point_base,
            gid,
            true,
        )?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::super::testing::build;
    use super::*;
    use crate::{
        tables::glyf::{
            outline::{Outline, OutlineTables, Scale26Dot6, Unscaled},
            PointFlags,
        },
        tables::{glyf::Glyf, gvar::Gvar, loca::Loca},
        types::{F26Dot6, Fixed},
        FontData, FontRead, FontRef,
    };
    use alloc::{vec, vec::Vec};

    /// A composite whose variation data will not parse must not leak the
    /// delta stack reservation onto its siblings.
    ///
    /// `load_composite` reserves space on the composite delta stack whenever
    /// the glyph has variation data, but only sets `have_deltas` when reading
    /// that data succeeded. Restoring the stack pointer only in that case left
    /// the reservation in place for a malformed glyph, so the next composite
    /// at the same level started from a higher base and could run the buffer
    /// out. The restore is unconditional now.
    #[test]
    fn a_malformed_composite_does_not_leak_the_delta_stack() {
        // Two contours' worth of nothing, just to have a simple glyph.
        fn empty_simple() -> Vec<u8> {
            let mut out = vec![];
            out.extend_from_slice(&0i16.to_be_bytes()); // numberOfContours
            out.extend_from_slice(&[0u8; 8]); // bounding box
            out.extend_from_slice(&0u16.to_be_bytes()); // instructionLength
            out
        }
        fn composite(components: &[u16]) -> Vec<u8> {
            let mut out = vec![];
            out.extend_from_slice(&(-1i16).to_be_bytes()); // numberOfContours
            out.extend_from_slice(&[0u8; 8]); // bounding box
            for (i, gid) in components.iter().enumerate() {
                let last = i == components.len() - 1;
                // ARG_1_AND_2_ARE_WORDS | ARGS_ARE_XY_VALUES, MORE_COMPONENTS
                let flags = 0x0001u16 | 0x0002 | if last { 0 } else { 0x0020 };
                out.extend_from_slice(&flags.to_be_bytes());
                out.extend_from_slice(&gid.to_be_bytes());
                out.extend_from_slice(&0i16.to_be_bytes()); // dx
                out.extend_from_slice(&0i16.to_be_bytes()); // dy
            }
            out
        }
        fn tables_from_glyphs(glyphs: &[Vec<u8>]) -> (Vec<u8>, Vec<u8>) {
            let mut glyf = vec![];
            let mut loca = vec![];
            for glyph in glyphs {
                loca.extend_from_slice(&(glyf.len() as u32).to_be_bytes());
                glyf.extend_from_slice(glyph);
            }
            loca.extend_from_slice(&(glyf.len() as u32).to_be_bytes());
            (glyf, loca)
        }
        // gid 0 references two composites, each of which references the simple
        // glyph. Only gid 1 carries variation data, and it is malformed.
        let (glyf, loca) = tables_from_glyphs(&[
            composite(&[1, 2]),
            composite(&[3]),
            composite(&[3]),
            empty_simple(),
        ]);
        let glyf = Glyf::read(FontData::new(&glyf)).unwrap();
        let loca = Loca::read(FontData::new(&loca), true).unwrap();
        let gvar_bytes = malformed_gvar_for_composites();
        let gvar = Gvar::read(FontData::new(&gvar_bytes)).unwrap();
        // Reading has to fail for each of them, or this proves nothing.
        for gid in 0..3 {
            assert!(
                gvar.glyph_variation_data(GlyphId::new(gid))
                    .unwrap()
                    .unwrap()
                    .composite_deltas(
                        &[F2Dot14::from_f32(1.0)],
                        &mut [Point::<Fixed>::default(); 8]
                    )
                    .is_none(),
                "gid {gid} parsed when it should not"
            );
        }
        let coords = [F2Dot14::from_f32(1.0)];
        let context = OutlineTables {
            glyf,
            loca,
            gvar: Some(gvar),
            hmtx: None,
            os2: None,
            hvar: None,
            units_per_em: 1000,
            coords: &coords,
            gvar_scalars: &[],
        };
        let mut outline = Outline::<Unscaled>::new();
        // Before the fix this returned `InsufficientMemory`: gid 2 started
        // from the base gid 1 failed to give back.
        assert!(outline.load(&context, GlyphId::new(0)).is_ok());
    }

    /// A `gvar` giving gids 0, 1 and 2 a tuple whose packed deltas are
    /// truncated, so reading any of them fails.
    fn malformed_gvar_for_composites() -> Vec<u8> {
        use font_test_data::bebuffer::BeBuffer;
        // One glyph's worth of variation data: a single tuple that claims
        // deltas for every point and then supplies none.
        let mut g = BeBuffer::new();
        g = g.push(1u16); // tupleVariationCount
        g = g.push(10u16); // dataOffset
        g = g.push(2u16); // variationDataSize
        g = g.push(0xA000u16); // EMBEDDED_PEAK_TUPLE | PRIVATE_POINT_NUMBERS
        g = g.push(0x4000u16); // peak of 1.0 on the only axis
        g = g.push(0u8); // private point numbers: zero means every point
        g = g.push(0x3Fu8); // a run of 64 byte deltas, with none following
        let blob = g.to_vec();
        let len = blob.len() as u32;

        let mut buf = BeBuffer::new();
        buf = buf.push(1u16).push(0u16); // version 1.0
        buf = buf.push(1u16); // axisCount
                              // A shared tuple count of zero still needs a non-null offset.
        buf = buf.push(0u16).push(20u32);
        buf = buf.push(4u16); // glyphCount
        buf = buf.push(1u16); // flags: 32 bit offsets
        buf = buf.push(0u32); // glyphVariationDataArrayOffset, patched below
                              // Gids 0, 1 and 2 each get a copy; gid 3 is the simple glyph.
        buf = buf
            .push(0u32)
            .push(len)
            .push(len * 2)
            .push(len * 3)
            .push(len * 3);
        let mut bytes = buf.to_vec();
        let array_start = bytes.len() as u32;
        bytes[16..20].copy_from_slice(&array_start.to_be_bytes());
        for _ in 0..3 {
            bytes.extend_from_slice(&blob);
        }
        bytes
    }

    /// `Unscaled` does the same arithmetic `Scale26Dot6` does at no size,
    /// without the round trip through 26.6, so the two agree exactly.
    #[test]
    fn unscaled_matches_26dot6_without_a_size() {
        for gid in [0, 1, 5, 7] {
            let fixed = build(
                font_test_data::GLYF_COMPONENTS,
                gid,
                &Scale26Dot6::new(None, 1000),
            );
            let raw = build(font_test_data::GLYF_COMPONENTS, gid, &Unscaled);
            assert_eq!(raw.contours, fixed.contours, "gid {gid}");
            assert_eq!(raw.points.len(), fixed.points.len(), "gid {gid}");
            for (r, f) in raw.points.iter().zip(&fixed.points) {
                assert_eq!(r.x, f.x.to_bits() >> 6, "gid {gid}");
                assert_eq!(r.y, f.y.to_bits() >> 6, "gid {gid}");
            }
        }
    }

    /// A composite contributes its components' points and contours.
    #[test]
    fn composite_inherits_its_components() {
        let scale = Scale26Dot6::new(None, 1000);
        let simple = build(font_test_data::GLYF_COMPONENTS, 1, &scale).points;
        // Glyph 5 references glyph 1 twice, at different offsets.
        let built = build(font_test_data::GLYF_COMPONENTS, 5, &scale);
        let composite = built.points;
        assert_eq!(composite.len(), simple.len() * 2);
        assert_eq!(built.contours.len(), 2);
        // The two copies are placed differently.
        assert_ne!(composite[0], composite[simple.len()]);
    }

    /// An empty glyph produces no points but still carries metrics.
    #[test]
    fn empty_glyph_has_metrics_but_no_points() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        let mut outline = Outline::<Scale26Dot6>::new();
        let out = outline.load(&context, GlyphId::new(0), Some(16.0)).unwrap();
        assert!(out.points.is_empty());
        assert!(out.contours.is_empty());
        assert!(out.adjusted_advance_width() != F26Dot6::ZERO);
    }

    /// Buffers smaller than `buffer_lengths` asks for are reported rather than
    /// overrun.
    #[test]
    fn short_buffers_are_rejected() {
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        let plan = OutlinePlan::new(&context, GlyphId::new(1)).unwrap();
        let scale = Scale26Dot6::new(None, 1000);
        let lengths = plan.buffer_lengths::<Scale26Dot6>();
        // Deliberately one point long, which is why this cannot use `Outline`:
        // that sizes its buffers to fit.
        let mut points = vec![Point::default(); 1];
        let mut flags = vec![PointFlags::default(); lengths.points as usize];
        let mut contours = vec![0u16; lengths.contours as usize];
        let mut unscaled = vec![Point::default(); lengths.unscaled as usize];
        let buffers = Buffers {
            points: &mut points,
            flags: &mut flags,
            contours: &mut contours,
            unscaled: &mut unscaled,
            deltas: &mut [],
            iup: &mut [],
            composite_deltas: &mut [],
        };
        assert_eq!(
            plan.load(&context, &scale, buffers, None).err(),
            Some(OutlineError::InsufficientMemory)
        );
    }
}
