//! Coordinate representation and rounding policy.
//!
//! [`Scale`] holds the arithmetic that differs between target coordinate
//! types. Loading takes the same steps whichever one is in use.
//!
//! Three are provided:
//!
//! - [`Scale26Dot6`] matches FreeType: 26.6 coordinates, 16.16 deltas, and
//!   rounding where FreeType rounds.
//! - [`ScaleF32`] matches HarfBuzz: floating point throughout, no rounding.
//! - [`Unscaled`] stays in font units, which is what an autohinter needs. It
//!   matches [`Scale26Dot6`] at no size, without the round trip through 26.6.

use bytemuck::NoUninit;

use super::OutlineError;
use crate::{
    tables::{
        glyf::{PointCoord, PointFlags, SimpleGlyph, Transform, PHANTOM_POINT_COUNT},
        gvar::{DeltaBuffers, GlyphVariationData},
    },
    types::{F26Dot6, F2Dot14, Fixed, Point},
    ReadError,
};

/// How coordinates are represented and rounded while loading an outline.
///
/// This trait is sealed. [`load_simple_points`](Self::load_simple_points)
/// takes a type this crate does not export, so the implementations here are
/// the only ones.
pub trait Scale: Sized {
    /// Coordinate type of the loaded outline.
    type Coord: PointCoord + NoUninit;
    /// Accumulator type for variation deltas.
    type Delta: PointCoord + NoUninit;

    /// True if the scale keeps a parallel copy of the points in font units,
    /// which hinting needs and which sparse delta interpolation uses as its
    /// reference coordinates.
    const NEEDS_UNSCALED: bool;

    /// True if a size was supplied. When false the outline comes back in font
    /// units, expressed in [`Self::Coord`].
    fn is_scaled(&self) -> bool;

    // -- phantom points, which hold font units until they are scaled --------

    /// Converts a font unit value into the representation phantom points use
    /// before they are scaled.
    fn phantom_from_font_units(&self, value: i32) -> Self::Coord;

    /// Reinterprets an output coordinate as the integer a hinter's font unit
    /// buffer holds.
    ///
    /// A composite has no font unit points of its own, so FreeType fills that
    /// buffer with the loaded coordinates as they stand. For [`Scale26Dot6`]
    /// that means raw fixed point bits rather than font units, which is what
    /// the interpreter expects.
    fn coord_to_hinting_raw(&self, value: Self::Coord) -> i32;

    /// Scales a phantom point that still holds font units.
    fn scale_phantom(&self, point: Point<Self::Coord>) -> Point<Self::Coord>;

    /// Adds a composite glyph's phantom point delta.
    fn add_phantom_delta(
        &self,
        point: Point<Self::Coord>,
        delta: Point<Self::Delta>,
    ) -> Point<Self::Coord>;

    /// Rounds a phantom point, which FreeType does before hinting.
    fn round_phantom(&self, point: Point<Self::Coord>) -> Point<Self::Coord>;

    // -- whole passes, where the modes differ in more than arithmetic -------

    /// Sets the phantom points of a glyph with no outline.
    ///
    /// `deltas` are this glyph's phantom point deltas, if the font is variable
    /// and has them. They apply whether or not the font has `HVAR`: the
    /// phantom points are seeded from unvaried metrics, so `gvar` is their
    /// only source of variation.
    fn load_empty_phantom(
        &self,
        phantom: &mut [Point<Self::Coord>; PHANTOM_POINT_COUNT],
        deltas: Option<[Point<Fixed>; PHANTOM_POINT_COUNT]>,
    );

    /// Reads a simple glyph's points, applies variation deltas, appends the
    /// phantom points and writes the scaled result.
    ///
    /// Returns the phantom points as they ended up, which the caller carries
    /// forward.
    fn load_simple_points(
        &self,
        pass: SimplePass<'_, '_, Self>,
    ) -> Result<[Point<Self::Coord>; PHANTOM_POINT_COUNT], OutlineError>;

    // -- composite components ----------------------------------------------

    /// Applies a component's 2x2 transform to points that have already been
    /// placed.
    ///
    /// Takes the whole slice so a scale converts the transform once rather
    /// than once per point.
    fn transform_points(&self, points: &mut [Point<Self::Coord>], transform: &Transform);

    /// Computes the translation for a component given as an offset.
    ///
    /// `scale_offset` is set when the component carries
    /// `SCALED_COMPONENT_OFFSET` without `UNSCALED_COMPONENT_OFFSET`,
    /// `delta` is the component's variation delta if there is one, and
    /// `round_y` when the component carries `ROUND_XY_TO_GRID` and the outline
    /// is being hinted.
    fn component_offset(
        &self,
        offset: Point<i32>,
        transform: &Transform,
        scale_offset: bool,
        delta: Option<Point<Self::Delta>>,
        round_y: bool,
    ) -> Point<Self::Coord>;
}

/// Everything [`Scale::load_simple_points`] needs beyond the glyph itself.
pub struct SimplePass<'a, 'v, S: Scale> {
    /// The glyph being read.
    pub glyph: &'a SimpleGlyph<'a>,
    /// Output coordinates, sized for the glyph's points plus phantom points.
    pub scaled: &'a mut [Point<S::Coord>],
    /// Font unit coordinates, empty unless [`Scale::NEEDS_UNSCALED`].
    pub unscaled: &'a mut [Point<i32>],
    /// Point flags, the same length as `scaled`.
    pub flags: &'a mut [PointFlags],
    /// Contour end points, already read and relative to this glyph.
    pub contours: &'a [u16],
    /// The phantom points to append, in font units.
    pub phantom: [Point<S::Coord>; PHANTOM_POINT_COUNT],
    /// Delta and interpolation buffers, empty for a static instance.
    pub deltas: DeltaBuffers<'a, S::Delta>,
    /// The variation data to apply, or `None` if this glyph has none.
    ///
    /// Has its own lifetime because it is invariant and cannot shrink to
    /// match the borrows above.
    pub var_data: Option<GlyphVariationData<'v>>,
    /// True if the font varies, whatever this glyph has.
    ///
    /// A glyph with no variation data still takes the delta path, on zeroed
    /// deltas, because the two paths round differently.
    pub varies: bool,
    /// Normalized variation coordinates, empty for a static instance.
    pub coords: &'a [F2Dot14],
    /// True if the font has `HVAR`, which changes how FreeType rounds the
    /// deltas it applies to phantom points.
    pub has_hvar: bool,
    /// True if the loaded outline will be hinted.
    pub is_hinted: bool,
}

// -------------------------------------------------------------------------
// FreeType: 26.6 coordinates, 16.16 deltas
// -------------------------------------------------------------------------

/// Scales font units to 26.6 fixed point, reproducing FreeType's arithmetic.
#[derive(Copy, Clone, Debug)]
pub struct Scale26Dot6 {
    scale: Fixed,
    is_scaled: bool,
}

impl Scale26Dot6 {
    /// Creates a scale for the given size in pixels per em.
    ///
    /// `None` produces an unscaled outline with coordinates expressed in 26.6.
    pub fn new(ppem: Option<f32>, units_per_em: u16) -> Self {
        if let Some(ppem) = ppem {
            if units_per_em > 0 {
                return Self {
                    scale: Fixed::from_bits((ppem * 64.) as i32)
                        / Fixed::from_bits(units_per_em as i32),
                    is_scaled: true,
                };
            }
        }
        Self {
            scale: Fixed::from_bits(0x10000),
            is_scaled: false,
        }
    }

    /// The scale factor that converts font units to scaled 26.6, in 16.16.
    pub fn to_bits(self) -> i32 {
        self.scale.to_bits()
    }

    /// Multiplies a raw value, in font units or 26.6, by the scale factor.
    ///
    /// The factor converts as it scales, so this serves both cases.
    fn mul(&self, value: i32) -> F26Dot6 {
        F26Dot6::from_bits((Fixed::from_bits(value) * self.scale).to_bits())
    }

    fn mul_point(&self, value: Point<i32>) -> Point<F26Dot6> {
        Point::new(self.mul(value.x), self.mul(value.y))
    }

    /// Scales a point in font units that carries a delta, given in 26.6.
    ///
    /// The scale factor has an i32 to 26.6 conversion built into it, so the
    /// product is shifted back down afterwards.
    fn mul_point_with_delta(
        &self,
        font_units: Point<i32>,
        delta: Point<F26Dot6>,
    ) -> Point<F26Dot6> {
        let scaled =
            self.mul_point((font_units.map(F26Dot6::from_i32) + delta).map(|v| v.to_bits()));
        scaled.map(|v| F26Dot6::from_bits(v.to_i32()))
    }
}

impl Scale for Scale26Dot6 {
    type Coord = F26Dot6;
    type Delta = Fixed;

    const NEEDS_UNSCALED: bool = true;

    fn is_scaled(&self) -> bool {
        self.is_scaled
    }

    fn phantom_from_font_units(&self, value: i32) -> F26Dot6 {
        F26Dot6::from_bits(value)
    }

    fn coord_to_hinting_raw(&self, value: F26Dot6) -> i32 {
        value.to_bits()
    }

    fn scale_phantom(&self, point: Point<F26Dot6>) -> Point<F26Dot6> {
        if self.is_scaled {
            self.mul_point(point.map(|v| v.to_bits()))
        } else {
            point.map(|v| F26Dot6::from_i32(v.to_bits()))
        }
    }

    fn add_phantom_delta(&self, point: Point<F26Dot6>, delta: Point<Fixed>) -> Point<F26Dot6> {
        // FreeType rounds off the fractional part of a composite's deltas.
        point + delta.map(Fixed::to_i32).map(F26Dot6::from_bits)
    }

    fn round_phantom(&self, point: Point<F26Dot6>) -> Point<F26Dot6> {
        point.map(|v| v.round())
    }

    fn load_empty_phantom(
        &self,
        phantom: &mut [Point<F26Dot6>; PHANTOM_POINT_COUNT],
        deltas: Option<[Point<Fixed>; PHANTOM_POINT_COUNT]>,
    ) {
        let mut font_units = phantom.map(|point| point.map(|v| v.to_bits()));
        if let Some(deltas) = deltas {
            font_units[0] += deltas[0].map(Fixed::to_i32);
            font_units[1] += deltas[1].map(Fixed::to_i32);
        }
        for (out, font_units) in phantom.iter_mut().zip(&font_units) {
            *out = if self.is_scaled {
                self.mul_point(*font_units)
            } else {
                font_units.map(F26Dot6::from_i32)
            };
        }
    }

    fn load_simple_points(
        &self,
        pass: SimplePass<'_, '_, Self>,
    ) -> Result<[Point<F26Dot6>; PHANTOM_POINT_COUNT], OutlineError> {
        let SimplePass {
            glyph,
            scaled,
            unscaled,
            flags,
            contours,
            phantom,
            mut deltas,
            var_data,
            varies,
            coords,
            has_hvar,
            is_hinted,
        } = pass;
        let phantom_start = glyph.num_points();
        // Phantom points hold font units in the low bits until they are scaled.
        let raw_phantom = phantom.map(|point| point.map(|v| v.to_bits()));
        read_points_and_phantom(glyph, unscaled, flags, &raw_phantom)?;
        let have_deltas = simple_deltas(
            var_data,
            varies,
            coords,
            unscaled,
            flags,
            contours,
            &mut deltas,
        );
        if self.is_scaled {
            if have_deltas {
                for ((point, unscaled), delta) in scaled
                    .iter_mut()
                    .zip(unscaled.iter())
                    .zip(deltas.deltas.iter())
                {
                    *point = self.mul_point_with_delta(*unscaled, delta.map(Fixed::to_f26dot6));
                }
                // FreeType applies different rounding to HVAR deltas. Since we
                // only use gvar, mimic that for phantom point deltas when an
                // HVAR table is present.
                if has_hvar {
                    for ((point, unscaled), delta) in scaled[phantom_start..]
                        .iter_mut()
                        .zip(&unscaled[phantom_start..])
                        .zip(&deltas.deltas[phantom_start..])
                    {
                        // Whole font units, rather than the 26.6 above.
                        let delta = delta.map(Fixed::to_i32).map(F26Dot6::from_i32);
                        *point = self.mul_point_with_delta(*unscaled, delta);
                    }
                }
                if is_hinted {
                    // Hinting reads the font unit points, so those need the
                    // deltas as well.
                    fold_deltas_into_font_units(unscaled, deltas.deltas);
                }
            } else {
                for (point, unscaled) in scaled.iter_mut().zip(unscaled.iter()) {
                    *point = self.mul_point(*unscaled);
                }
            }
        } else {
            if have_deltas {
                fold_deltas_into_font_units(unscaled, deltas.deltas);
            }
            // Unlike FreeType, we also return unscaled outlines in 26.6.
            for (point, unscaled) in scaled.iter_mut().zip(unscaled.iter()) {
                *point = unscaled.map(F26Dot6::from_i32);
            }
        }
        let mut out = [Point::default(); PHANTOM_POINT_COUNT];
        out.copy_from_slice(&scaled[phantom_start..]);
        Ok(out)
    }

    fn transform_points(&self, points: &mut [Point<F26Dot6>], transform: &Transform) {
        let t = FixedTransform::new(transform);
        if self.is_scaled {
            for point in points.iter_mut() {
                let p = point.map(|c| Fixed::from_bits(c.to_bits()));
                *point = Point::new(p.x * t.xx + p.y * t.xy, p.x * t.yx + p.y * t.yy)
                    .map(|c| F26Dot6::from_bits(c.to_bits()));
            }
        } else {
            // This juggling is necessary because, unlike FreeType, we also
            // return unscaled outlines in 26.6 format.
            for point in points.iter_mut() {
                let p = point.map(|c| Fixed::from_bits(c.to_i32()));
                *point = Point::new(p.x * t.xx + p.y * t.xy, p.x * t.yx + p.y * t.yy)
                    .map(|c| F26Dot6::from_i32(c.to_bits()));
            }
        }
    }

    fn component_offset(
        &self,
        offset: Point<i32>,
        transform: &Transform,
        scale_offset: bool,
        delta: Option<Point<Fixed>>,
        round_y: bool,
    ) -> Point<F26Dot6> {
        let (mut x, mut y) = (offset.x, offset.y);
        if scale_offset {
            let t = FixedTransform::new(transform);
            // FreeType uses a fixed point multiplication here. Scale x by the
            // magnitude of the x basis and y by the y basis.
            x = (Fixed::from_bits(x) * hypot_fixed(t.xx, t.xy)).to_bits();
            y = (Fixed::from_bits(y) * hypot_fixed(t.yy, t.yx)).to_bits();
        }
        if let Some(delta) = delta {
            // For composite glyphs, we copy FreeType and round off the
            // fractional parts of deltas.
            x += delta.x.to_i32();
            y += delta.y.to_i32();
        }
        if self.is_scaled {
            let mut offset = self.mul_point(Point::new(x, y));
            if round_y {
                // Only round the y-coordinate, per FreeType.
                offset.y = offset.y.round();
            }
            offset
        } else {
            Point::new(x, y).map(F26Dot6::from_i32)
        }
    }
}

// -------------------------------------------------------------------------
// HarfBuzz: floating point throughout
// -------------------------------------------------------------------------

/// Scales font units to floating point pixels, reproducing HarfBuzz's
/// arithmetic.
#[derive(Copy, Clone, Debug)]
pub struct ScaleF32 {
    scale: f32,
}

impl ScaleF32 {
    /// Creates a scale for the given size in pixels per em.
    pub fn new(ppem: Option<f32>, units_per_em: u16) -> Self {
        let scale = if units_per_em == 0 {
            1.0
        } else {
            ppem.map(|ppem| ppem / units_per_em as f32).unwrap_or(1.0)
        };
        Self { scale }
    }
}

impl Scale for ScaleF32 {
    type Coord = f32;
    type Delta = f32;

    const NEEDS_UNSCALED: bool = false;

    fn is_scaled(&self) -> bool {
        self.scale != 1.0
    }

    fn phantom_from_font_units(&self, value: i32) -> f32 {
        value as f32
    }

    fn coord_to_hinting_raw(&self, value: f32) -> i32 {
        value as i32
    }

    fn scale_phantom(&self, point: Point<f32>) -> Point<f32> {
        if self.scale != 1.0 {
            point * self.scale
        } else {
            point
        }
    }

    fn add_phantom_delta(&self, point: Point<f32>, delta: Point<f32>) -> Point<f32> {
        point + delta
    }

    fn round_phantom(&self, point: Point<f32>) -> Point<f32> {
        point
    }

    fn load_empty_phantom(
        &self,
        phantom: &mut [Point<f32>; PHANTOM_POINT_COUNT],
        deltas: Option<[Point<Fixed>; PHANTOM_POINT_COUNT]>,
    ) {
        let mut font_units = *phantom;
        // HarfBuzz has no equivalent of this, so it follows FreeType.
        if let Some(deltas) = deltas {
            font_units[0] += deltas[0].map(Fixed::to_f32);
            font_units[1] += deltas[1].map(Fixed::to_f32);
        }
        for (out, font_units) in phantom.iter_mut().zip(&font_units) {
            *out = *font_units * self.scale;
        }
    }

    fn load_simple_points(
        &self,
        pass: SimplePass<'_, '_, Self>,
    ) -> Result<[Point<f32>; PHANTOM_POINT_COUNT], OutlineError> {
        let SimplePass {
            glyph,
            scaled,
            flags,
            contours,
            phantom,
            mut deltas,
            var_data,
            varies,
            coords,
            ..
        } = pass;
        // `ScaleF32` works in place: the points start out as font
        // units and are scaled at the end.
        let phantom_start = glyph.num_points();
        read_points_and_phantom(glyph, scaled, flags, &phantom)?;
        if simple_deltas(
            var_data,
            varies,
            coords,
            scaled,
            flags,
            contours,
            &mut deltas,
        ) {
            for (point, delta) in scaled.iter_mut().zip(deltas.deltas.iter()) {
                *point += *delta;
            }
        }
        if self.scale != 1.0 {
            for point in scaled.iter_mut() {
                *point *= self.scale;
            }
        }
        let mut out = [Point::default(); PHANTOM_POINT_COUNT];
        out.copy_from_slice(&scaled[phantom_start..]);
        Ok(out)
    }

    fn transform_points(&self, points: &mut [Point<f32>], transform: &Transform) {
        let (xx, yx, xy, yy) = (
            transform.xx.to_f32(),
            transform.yx.to_f32(),
            transform.xy.to_f32(),
            transform.yy.to_f32(),
        );
        for point in points.iter_mut() {
            *point = Point::new(point.x * xx + point.y * xy, point.x * yx + point.y * yy);
        }
    }

    fn component_offset(
        &self,
        offset: Point<i32>,
        transform: &Transform,
        scale_offset: bool,
        delta: Option<Point<f32>>,
        _round_y: bool,
    ) -> Point<f32> {
        let (mut x, mut y) = (offset.x as f32, offset.y as f32);
        if scale_offset {
            // FreeType implements hypot; we can just use the provided one.
            fn hypot(a: f32, b: f32) -> f32 {
                #[cfg(not(feature = "libm"))]
                {
                    a.hypot(b)
                }
                #[cfg(feature = "libm")]
                {
                    // Unused when `std` is also on, which is allowed: the
                    // inherent method wins there. Matches `tables::varc`.
                    #[allow(unused_imports)]
                    use core_maths::CoreFloat;
                    a.hypot(b)
                }
            }
            // Scale x by the magnitude of the x-basis, y by the y-basis.
            x *= hypot(transform.xx.to_f32(), transform.xy.to_f32());
            y *= hypot(transform.yx.to_f32(), transform.yy.to_f32());
        }
        let mut offset = Point::new(x, y);
        if let Some(delta) = delta {
            // Composite deltas are in font units too, so this stays consistent.
            offset += delta;
        }
        // The component's points have already been scaled, so the offset has to
        // be as well.
        offset * self.scale
    }
}

// ---------------------------------------------------------------------------
// Font units, unscaled
// ---------------------------------------------------------------------------

/// Leaves coordinates in font units.
///
/// Variations still apply, and composite components are still transformed and
/// placed, but nothing is scaled or rounded to a pixel grid. This is what an
/// autohinter wants: the outline as the designer drew it, at the requested
/// location in variation space.
///
/// The arithmetic matches [`Scale26Dot6`] at no size, without the round trip
/// through 26.6 that getting font units back out would otherwise need.
#[derive(Copy, Clone, Debug, Default)]
pub struct Unscaled;

impl Scale for Unscaled {
    type Coord = i32;
    type Delta = Fixed;

    // The output is already in font units, so there is nothing to keep a
    // second copy of; the points serve as their own interpolation reference.
    const NEEDS_UNSCALED: bool = false;

    fn is_scaled(&self) -> bool {
        false
    }

    fn phantom_from_font_units(&self, value: i32) -> i32 {
        value
    }

    fn coord_to_hinting_raw(&self, value: i32) -> i32 {
        value
    }

    fn scale_phantom(&self, point: Point<i32>) -> Point<i32> {
        point
    }

    fn add_phantom_delta(&self, point: Point<i32>, delta: Point<Fixed>) -> Point<i32> {
        point + delta.map(Fixed::to_i32)
    }

    fn round_phantom(&self, point: Point<i32>) -> Point<i32> {
        point
    }

    fn load_empty_phantom(
        &self,
        phantom: &mut [Point<i32>; PHANTOM_POINT_COUNT],
        deltas: Option<[Point<Fixed>; PHANTOM_POINT_COUNT]>,
    ) {
        if let Some(deltas) = deltas {
            phantom[0] += deltas[0].map(Fixed::to_i32);
            phantom[1] += deltas[1].map(Fixed::to_i32);
        }
    }

    fn load_simple_points(
        &self,
        pass: SimplePass<'_, '_, Self>,
    ) -> Result<[Point<i32>; PHANTOM_POINT_COUNT], OutlineError> {
        let SimplePass {
            glyph,
            scaled,
            flags,
            contours,
            phantom,
            mut deltas,
            var_data,
            varies,
            coords,
            ..
        } = pass;
        // The points are their own font unit copy, so this works in place.
        let phantom_start = glyph.num_points();
        read_points_and_phantom(glyph, scaled, flags, &phantom)?;
        let have_deltas = simple_deltas(
            var_data,
            varies,
            coords,
            scaled,
            flags,
            contours,
            &mut deltas,
        );
        if have_deltas {
            fold_deltas_into_font_units(scaled, deltas.deltas);
        }
        let mut out = [Point::default(); PHANTOM_POINT_COUNT];
        out.copy_from_slice(&scaled[phantom_start..]);
        Ok(out)
    }

    fn transform_points(&self, points: &mut [Point<i32>], transform: &Transform) {
        let t = FixedTransform::new(transform);
        // Font units are treated as raw 16.16 for the multiply, which is what
        // FreeType does for an unscaled outline.
        for point in points.iter_mut() {
            let p = point.map(Fixed::from_bits);
            *point =
                Point::new(p.x * t.xx + p.y * t.xy, p.x * t.yx + p.y * t.yy).map(|c| c.to_bits());
        }
    }

    fn component_offset(
        &self,
        offset: Point<i32>,
        transform: &Transform,
        scale_offset: bool,
        delta: Option<Point<Fixed>>,
        _round_y: bool,
    ) -> Point<i32> {
        let mut offset = offset;
        if scale_offset {
            let t = FixedTransform::new(transform);
            offset = Point::new(
                (Fixed::from_bits(offset.x) * hypot_fixed(t.xx, t.xy)).to_bits(),
                (Fixed::from_bits(offset.y) * hypot_fixed(t.yy, t.yx)).to_bits(),
            );
        }
        if let Some(delta) = delta {
            offset += delta.map(Fixed::to_i32);
        }
        offset
    }
}

// -------------------------------------------------------------------------
// Shared helpers
// -------------------------------------------------------------------------

/// Reads a simple glyph's points into `dest` and appends the phantom points
/// after them.
///
/// `dest` and `flags` cover the glyph's points plus the four phantom points.
/// The staging type differs by scale -- font units for 26.6, the output
/// coordinates themselves for the floating point one -- so this is generic over
/// it, with the caller supplying phantom points already in that form.
fn read_points_and_phantom<C: PointCoord>(
    glyph: &SimpleGlyph,
    dest: &mut [Point<C>],
    flags: &mut [PointFlags],
    phantom: &[Point<C>; PHANTOM_POINT_COUNT],
) -> Result<(), ReadError> {
    let point_count = glyph.num_points();
    glyph.read_points_fast(&mut dest[..point_count], &mut flags[..point_count])?;
    for (i, phantom) in phantom.iter().enumerate() {
        dest[point_count + i] = *phantom;
        flags[point_count + i] = Default::default();
    }
    Ok(())
}

/// Folds variation deltas into the font unit points, rounding them off the way
/// FreeType does.
fn fold_deltas_into_font_units(font_units: &mut [Point<i32>], deltas: &[Point<Fixed>]) {
    for (point, delta) in font_units.iter_mut().zip(deltas) {
        *point += delta.map(Fixed::to_i32);
    }
}

/// A component's 2x2 transform in the 16.16 form FreeType uses.
struct FixedTransform {
    xx: Fixed,
    yx: Fixed,
    xy: Fixed,
    yy: Fixed,
}

impl FixedTransform {
    fn new(transform: &Transform) -> Self {
        fn widen(x: F2Dot14) -> Fixed {
            Fixed::from_bits(x.to_bits() as i32 * 4)
        }
        Self {
            xx: widen(transform.xx),
            yx: widen(transform.yx),
            xy: widen(transform.xy),
            yy: widen(transform.yy),
        }
    }
}

/// Approximate magnitude of a transform basis vector.
///
/// According to FreeType this algorithm is a "guess" that works better than the
/// one Apple documents, so both fixed point modes use it to scale a component
/// offset that asks to be scaled.
///
/// <https://github.com/freetype/freetype/blob/b1c90733ee6a04882b133101d61b12e352eeb290/src/truetype/ttgload.c#L1259>
fn hypot_fixed(a: Fixed, b: Fixed) -> Fixed {
    let a = a.to_bits().abs();
    let b = b.to_bits().abs();
    Fixed::from_bits(if a > b {
        a + ((3 * b) >> 3)
    } else {
        b + ((3 * a) >> 3)
    })
}

/// Computes the deltas for a simple glyph, and reports whether the caller
/// should take its delta path.
///
/// A glyph of a varying font takes that path even with no variation data of
/// its own, on zeroed deltas. That looks wasteful and is not: [`Scale26Dot6`]
/// scales `(font units + delta)` and shifts back down, which does not round
/// the same way as scaling the font units alone, so a whole 26.6 unit rides on
/// which path runs. FreeType makes the same distinction.
fn simple_deltas<C, D>(
    var_data: Option<GlyphVariationData<'_>>,
    varies: bool,
    coords: &[F2Dot14],
    points: &[Point<C>],
    flags: &mut [PointFlags],
    contours: &[u16],
    buffers: &mut DeltaBuffers<'_, D>,
) -> bool
where
    C: PointCoord,
    D: PointCoord + From<C>,
{
    match var_data {
        Some(var_data) => var_data
            .simple_deltas(coords, points, flags, contours, buffers)
            .is_some(),
        None if varies => {
            for delta in buffers.deltas.iter_mut() {
                *delta = Default::default();
            }
            true
        }
        None => false,
    }
}

#[cfg(test)]
mod tests {
    use super::super::testing::build;
    use super::super::{OutlineContext, OutlinePlan, OutlineTables};
    use super::*;
    use crate::tables::loca::LocaGlyph;
    use crate::{tables::glyf::Glyph, types::GlyphId, FontRef};
    use alloc::{vec, vec::Vec};

    /// At no size [`Scale26Dot6`] returns font units shifted left by six.
    #[test]
    fn unscaled_simple_glyph_is_font_units() {
        let scale = Scale26Dot6::new(None, 1000);
        let built = build(font_test_data::GLYF_COMPONENTS, 1, &scale);
        let (points, contours) = (built.points, built.contours);
        assert_eq!(contours, vec![3]);
        assert_eq!(points.len(), 4);
        for point in &points {
            assert_eq!(point.x.to_bits() & 0x3F, 0, "expected whole font units");
        }
    }

    /// True where the test font's glyph is a composite.
    fn is_composite(gid: u32) -> bool {
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        matches!(
            context.glyph(GlyphId::new(gid)).unwrap().into_glyph(),
            Some(Glyph::Composite(_))
        )
    }

    /// At no size the two modes agree exactly on a simple glyph, and within a
    /// font unit on a composite: 26.6 rounds a component's offset where
    /// floating point does not, which is FreeType and HarfBuzz respectively.
    #[test]
    fn modes_agree_without_a_size() {
        for gid in [0, 1, 5, 7] {
            let slack = i32::from(is_composite(gid));
            let fixed = build(
                font_test_data::GLYF_COMPONENTS,
                gid,
                &Scale26Dot6::new(None, 1000),
            );
            let float = build(
                font_test_data::GLYF_COMPONENTS,
                gid,
                &ScaleF32::new(None, 1000),
            );
            assert_eq!(fixed.contours, float.contours, "gid {gid}");
            assert_eq!(fixed.points.len(), float.points.len(), "gid {gid}");
            for (a, b) in fixed.points.iter().zip(&float.points) {
                assert!(
                    ((a.x.to_bits() >> 6) - b.x as i32).abs() <= slack,
                    "gid {gid}"
                );
                assert!(
                    ((a.y.to_bits() >> 6) - b.y as i32).abs() <= slack,
                    "gid {gid}"
                );
            }
        }
    }

    /// Phantom points are scaled by their own trait methods, which the
    /// contour points never reach.
    #[test]
    fn modes_agree_on_phantom_points() {
        for gid in [0, 1, 5, 7] {
            let fixed = build(
                font_test_data::GLYF_COMPONENTS,
                gid,
                &Scale26Dot6::new(None, 1000),
            );
            let float = build(
                font_test_data::GLYF_COMPONENTS,
                gid,
                &ScaleF32::new(None, 1000),
            );
            for (a, b) in fixed.phantom.iter().zip(&float.phantom) {
                assert_eq!(a.x.to_bits() >> 6, b.x as i32, "gid {gid}");
                assert_eq!(a.y.to_bits() >> 6, b.y as i32, "gid {gid}");
            }
        }
    }

    /// Every input to `component_offset` is bounded, so the arithmetic cannot
    /// overflow whatever the font says.
    ///
    /// An anchor offset is an `i16`. `FixedTransform` widens an `F2Dot14` to
    /// at most two, so `hypot_fixed` of a pair is at most 2.75. `to_i32` is a
    /// shift by sixteen, so a delta contributes at most 2^15. The sum stays
    /// near 2^17; the measured worst case is 122,880, which `i32` clears by a
    /// factor of seventeen thousand. A debug build panics
    /// on overflow, so running this is most of the check; the bound is
    /// asserted so a later change that widens an input is caught here.
    #[test]
    fn component_offset_stays_far_inside_i32() {
        const BOUND: i32 = 1 << 18;
        let extremes = [i16::MIN, i16::MAX];
        let mut worst = 0i32;
        for x in extremes {
            for y in extremes {
                for basis in extremes {
                    let transform = Transform {
                        xx: F2Dot14::from_bits(basis),
                        yx: F2Dot14::from_bits(basis),
                        xy: F2Dot14::from_bits(basis),
                        yy: F2Dot14::from_bits(basis),
                    };
                    let offset = Point::new(x as i32, y as i32);
                    let delta = Point::new(Fixed::from_bits(i32::MAX), Fixed::from_bits(i32::MIN));
                    for scale_offset in [false, true] {
                        let raw = Unscaled.component_offset(
                            offset,
                            &transform,
                            scale_offset,
                            Some(delta),
                            false,
                        );
                        worst = worst.max(raw.x.abs()).max(raw.y.abs());
                        // The scaled modes run the same sum before scaling it.
                        let fixed = Scale26Dot6::new(Some(2048.0), 16).component_offset(
                            offset,
                            &transform,
                            scale_offset,
                            Some(delta),
                            true,
                        );
                        assert!(fixed.x.to_bits().abs() < i32::MAX / 2);
                        assert!(fixed.y.to_bits().abs() < i32::MAX / 2);
                        let float = ScaleF32::new(Some(2048.0), 16).component_offset(
                            offset,
                            &transform,
                            scale_offset,
                            Some(Point::new(f32::from(i16::MAX), f32::from(i16::MIN))),
                            false,
                        );
                        assert!(float.x.is_finite() && float.y.is_finite());
                    }
                }
            }
        }
        assert!(worst < BOUND, "worst offset was {worst}");
    }

    /// Whether a size was asked for decides whether [`OutlinePlan::load`](super::super::OutlinePlan::load)
    /// hints, so this is checked directly rather than through its effect.
    #[test]
    fn a_scale_knows_whether_it_has_a_size() {
        assert!(!Scale26Dot6::new(None, 1000).is_scaled());
        assert!(Scale26Dot6::new(Some(16.0), 1000).is_scaled());
        assert!(!ScaleF32::new(None, 1000).is_scaled());
        assert!(ScaleF32::new(Some(16.0), 1000).is_scaled());
        assert!(!Unscaled.is_scaled());
    }

    /// [`Unscaled`] is measured against `glyf` itself rather than another
    /// scale, so a change that moved both is still caught.
    #[test]
    fn unscaled_output_is_the_glyf_coordinates() {
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        let Some(LocaGlyph::Glyph(Glyph::Simple(simple))) = context.glyph(GlyphId::new(1)) else {
            panic!("glyph 1 is simple");
        };
        let raw: Vec<_> = simple.points().map(|point| (point.x, point.y)).collect();
        let built = build(font_test_data::GLYF_COMPONENTS, 1, &Unscaled);
        assert_eq!(built.points.len(), raw.len());
        for (point, (x, y)) in built.points.iter().zip(&raw) {
            assert_eq!((point.x, point.y), (i32::from(*x), i32::from(*y)));
        }
    }

    /// [`Unscaled`] needs no separate font unit buffer, since its output
    /// already is one.
    #[test]
    fn unscaled_needs_no_second_buffer() {
        const { assert!(!Unscaled::NEEDS_UNSCALED) };
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        let plan = OutlinePlan::new(&context, GlyphId::new(1)).unwrap();
        assert_eq!(plan.buffer_lengths::<Unscaled>().unscaled, 0);
    }

    /// Scaling halves at half the size, for a composite as well as a simple
    /// glyph: a component's offset is scaled before it reaches points that
    /// are already scaled.
    #[test]
    fn scaling_is_proportional() {
        for gid in [1, 5] {
            let small = build(
                font_test_data::GLYF_COMPONENTS,
                gid,
                &Scale26Dot6::new(Some(16.0), 1000),
            )
            .points;
            let large = build(
                font_test_data::GLYF_COMPONENTS,
                gid,
                &Scale26Dot6::new(Some(32.0), 1000),
            )
            .points;
            // Within a 26.6 unit of exactly double, and one more for a
            // composite, whose offset is rounded once at each size.
            let slack = 1 + i32::from(is_composite(gid));
            for (s, l) in small.iter().zip(&large) {
                assert!(
                    (l.x.to_bits() - 2 * s.x.to_bits()).abs() <= slack,
                    "gid {gid}"
                );
                assert!(
                    (l.y.to_bits() - 2 * s.y.to_bits()).abs() <= slack,
                    "gid {gid}"
                );
            }
        }
    }
}
