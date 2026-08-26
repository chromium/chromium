//! Computation of glyph point deltas from the `gvar` table.
//!
//! Tuples in a glyph's variation data come in two shapes. A *dense* tuple
//! carries a delta for every point and is simply accumulated. A *sparse* tuple
//! carries deltas for a subset, and the deltas for the remaining points have to
//! be inferred by interpolating between the nearest referenced points on either
//! side, in the manner of the `IUP` hinting instruction.
//!
//! See [inferred deltas for un-referenced point numbers](https://learn.microsoft.com/en-us/typography/opentype/spec/gvar#inferred-deltas-for-un-referenced-point-numbers).

use core::ops::Range;

use super::{GlyphDelta, GlyphVariationData, Gvar};
use crate::{
    tables::{
        glyf::{PointCoord, PointFlags, PointMarker, PHANTOM_POINT_COUNT},
        variations::TupleVariation,
    },
    types::{F2Dot14, Fixed, GlyphId, Point},
    ReadError,
};

/// Caller-provided storage for [`Gvar::simple_deltas`] and
/// [`GlyphVariationData::simple_deltas`].
///
/// Both slices must be at least as long as the glyph's point count, including
/// the phantom points.
pub struct DeltaBuffers<'a, D: PointCoord> {
    /// Receives the computed deltas.
    pub deltas: &'a mut [Point<D>],
    /// Working space used to interpolate the deltas of points that a sparse
    /// tuple does not reference.
    pub iup: &'a mut [Point<D>],
}

impl Gvar<'_> {
    /// Computes the deltas for the points of a simple glyph at the given
    /// location in variation space.
    ///
    /// Looks the glyph's variation data up and hands off to
    /// [`GlyphVariationData::simple_deltas`], which documents the arguments.
    ///
    /// Returns `true` if the glyph has variation data, and `false` if it does
    /// not — in which case the deltas are zeroed. Note that they are zeroed
    /// whichever it returns, so they never retain values from a previous call.
    pub fn simple_deltas<C, D>(
        &self,
        glyph_id: GlyphId,
        coords: &[F2Dot14],
        points: &[Point<C>],
        flags: &mut [PointFlags],
        contours: &[u16],
        buffers: &mut DeltaBuffers<'_, D>,
    ) -> Result<bool, ReadError>
    where
        C: PointCoord,
        D: PointCoord + From<C>,
    {
        check_simple_buffers(points, flags, buffers)?;
        let Ok(Some(var_data)) = self.glyph_variation_data(glyph_id) else {
            // Missing or malformed variation data for a glyph is not an error.
            zero(buffers.deltas);
            return Ok(false);
        };
        var_data.simple_deltas(coords, points, flags, contours, buffers)?;
        Ok(true)
    }

    /// Computes the deltas for the component offsets of a composite glyph at
    /// the given location in variation space.
    ///
    /// Looks the glyph's variation data up and hands off to
    /// [`GlyphVariationData::composite_deltas`].
    ///
    /// `deltas` must have one entry per component plus four for the phantom
    /// points.
    ///
    /// Returns `true` if the glyph has variation data, and `false` if it does
    /// not — in which case `deltas` is zeroed.
    pub fn composite_deltas<D: PointCoord>(
        &self,
        glyph_id: GlyphId,
        coords: &[F2Dot14],
        deltas: &mut [Point<D>],
    ) -> Result<bool, ReadError> {
        let Ok(Some(var_data)) = self.glyph_variation_data(glyph_id) else {
            zero(deltas);
            return Ok(false);
        };
        var_data.composite_deltas(coords, deltas)?;
        Ok(true)
    }
}

impl GlyphVariationData<'_> {
    /// Computes the deltas for the points of a simple glyph at the given
    /// location in variation space.
    ///
    /// Deltas for points that a sparse tuple does not reference are inferred by
    /// interpolation, so this needs the glyph's points and contour end points
    /// in addition to the buffers it writes.
    ///
    /// * `points` and `contours` describe the unvaried glyph. `points` must
    ///   include the four phantom points.
    /// * `flags` is used as scratch: the [`PointMarker::HAS_DELTA`] marker is
    ///   cleared and set as tuples are processed.
    /// * `buffers` supplies the output and interpolation storage.
    ///
    /// `flags` and both buffers must be at least as long as `points`, which
    /// is what everything here is indexed by.
    ///
    /// The deltas are zeroed before anything else happens, so they never
    /// retain values from a previous call.
    ///
    /// See [`Gvar::simple_deltas`] to look a glyph's data up first.
    pub fn simple_deltas<C, D>(
        &self,
        coords: &[F2Dot14],
        points: &[Point<C>],
        flags: &mut [PointFlags],
        contours: &[u16],
        buffers: &mut DeltaBuffers<'_, D>,
    ) -> Result<(), ReadError>
    where
        C: PointCoord,
        D: PointCoord + From<C>,
    {
        check_simple_buffers(points, flags, buffers)?;
        let DeltaBuffers { deltas, iup } = buffers;
        self.accumulate_deltas(coords, deltas, |scalar, tuple, deltas| {
            // Prepare the working buffer by converting the points to 16.16,
            // then drop the markers left by the previous tuple. Kept as two
            // passes: fused, the read-modify-write on the flags blocks the
            // conversion from vectorizing.
            for (point, iup_point) in points.iter().zip(&mut iup[..]) {
                *iup_point = point.map(D::from);
            }
            for flag in flags.iter_mut() {
                flag.clear_marker(PointMarker::HAS_DELTA);
            }
            tuple.accumulate_sparse_deltas(iup, flags, scalar)?;
            interpolate_deltas(points, flags, contours, &mut iup[..])
                .ok_or(ReadError::OutOfBounds)?;
            for ((delta, point), iup_point) in deltas.iter_mut().zip(points).zip(iup.iter()) {
                *delta += *iup_point - point.map(D::from);
            }
            Ok(())
        })
    }

    /// Computes the deltas for the component offsets of a composite glyph at
    /// the given location in variation space.
    ///
    /// Interpolation is meaningless for component offsets, so this skips the
    /// expensive part of [`Self::simple_deltas`] and needs no scratch.
    ///
    /// `deltas` must have one entry per component plus four for the phantom
    /// points, and is zeroed first.
    ///
    /// See [`Gvar::composite_deltas`] to look a glyph's data up first.
    pub fn composite_deltas<D: PointCoord>(
        &self,
        coords: &[F2Dot14],
        deltas: &mut [Point<D>],
    ) -> Result<(), ReadError> {
        self.accumulate_deltas(coords, deltas, |scalar, tuple, deltas| {
            for tuple_delta in tuple.deltas() {
                let ix = tuple_delta.position as usize;
                if let Some(delta) = deltas.get_mut(ix) {
                    *delta += tuple_delta.apply_scalar(scalar);
                }
            }
            Ok(())
        })
    }

    /// The parts shared by simple and composite glyph processing.
    ///
    /// Zeroes `deltas`, then accumulates every tuple that is active at
    /// `coords`. Dense tuples are handled here; sparse tuples are passed to
    /// `apply_sparse_tuple`, which differs between the two glyph kinds.
    fn accumulate_deltas<D: PointCoord>(
        &self,
        coords: &[F2Dot14],
        deltas: &mut [Point<D>],
        mut apply_sparse_tuple: impl FnMut(
            Fixed,
            TupleVariation<GlyphDelta>,
            &mut [Point<D>],
        ) -> Result<(), ReadError>,
    ) -> Result<(), ReadError> {
        // Callers must never observe values left over from a previous glyph.
        zero(deltas);
        for (tuple, scalar) in self.active_tuples_at(coords) {
            if tuple.has_deltas_for_all_points() {
                // Fast path: the tuple covers every point, so the deltas can be
                // accumulated directly with no interpolation.
                tuple.accumulate_dense_deltas(deltas, scalar)?;
            } else {
                apply_sparse_tuple(scalar, tuple, deltas)?;
            }
        }
        Ok(())
    }
}

/// The buffer requirements shared by both entry points to simple glyph
/// processing.
///
/// Everything hinges on `points`, which carries the glyph's real point count:
/// each of the others is indexed or zipped by point, so one that is short
/// either truncates the result silently or fails later with a less useful
/// error.
fn check_simple_buffers<C: PointCoord, D: PointCoord>(
    points: &[Point<C>],
    flags: &[PointFlags],
    buffers: &DeltaBuffers<'_, D>,
) -> Result<(), ReadError> {
    let count = points.len();
    if count < PHANTOM_POINT_COUNT
        || flags.len() < count
        || buffers.deltas.len() < count
        || buffers.iup.len() < count
    {
        return Err(ReadError::InvalidArrayLen);
    }
    Ok(())
}

fn zero<D: PointCoord>(deltas: &mut [Point<D>]) {
    for delta in deltas.iter_mut() {
        *delta = Default::default();
    }
}

/// Interpolates the points that the current tuple did not reference, in the
/// manner of the `IUP` hinting instruction.
///
/// Points carrying an explicit delta are marked with [`PointMarker::HAS_DELTA`]
/// in `flags`. Each contour is handled independently.
///
/// Modeled after the FreeType implementation:
/// <https://github.com/freetype/freetype/blob/bbfcd79eacb4985d4b68783565f4b494aa64516b/src/truetype/ttgxvar.c#L3881>
fn interpolate_deltas<C, D>(
    points: &[Point<C>],
    flags: &[PointFlags],
    contours: &[u16],
    out_points: &mut [Point<D>],
) -> Option<()>
where
    C: PointCoord,
    D: PointCoord + From<C>,
{
    let mut start = 0usize;
    for &end in contours {
        let end = end as usize;
        // A contour ending before the previous one did is malformed. Skip it
        // without advancing, so the contours that follow still line up with
        // the points they describe.
        if end < start {
            continue;
        }
        let range = start..end + 1;
        start = end + 1;
        // Slicing all three to the same range once means everything below can
        // work in contour local indices, with no further bounds checks.
        Contour {
            points: points.get(range.clone())?,
            flags: flags.get(range.clone())?,
            out_points: out_points.get_mut(range)?,
        }
        .interpolate_untouched();
    }
    Some(())
}

/// One contour's worth of points, as input coordinates and the moved
/// coordinates being derived from them.
///
/// All three slices have the same length, so indices are interchangeable
/// between them and are always in bounds.
struct Contour<'a, C, D>
where
    C: PointCoord,
    D: PointCoord + From<C>,
{
    points: &'a [Point<C>],
    flags: &'a [PointFlags],
    out_points: &'a mut [Point<D>],
}

impl<C, D> Contour<'_, C, D>
where
    C: PointCoord,
    D: PointCoord + From<C>,
{
    /// Infers the deltas of every point in this contour that the current tuple
    /// did not reference.
    fn interpolate_untouched(&mut self) {
        // Copy the reference out so iterating it does not borrow `self`.
        let flags = self.flags;
        let mut referenced = flags
            .iter()
            .enumerate()
            .filter(|(_, flag)| flag.has_marker(PointMarker::HAS_DELTA))
            .map(|(ix, _)| ix);
        let Some(first) = referenced.next() else {
            // Nothing in this contour is referenced, so the tuple does not
            // affect it at all.
            return;
        };
        // Interpolate each run of unreferenced points between two references.
        let mut last = first;
        for next in referenced {
            self.interpolate(last + 1..next, last, next);
            last = next;
        }
        if last == first {
            // A single reference carries the whole contour with it.
            self.shift(first);
        } else {
            // The points after the last reference and those before the first
            // wrap around, and are bracketed by that same pair. Both ranges are
            // empty when there is nothing to do.
            let len = self.out_points.len();
            self.interpolate(last + 1..len, last, first);
            self.interpolate(0..first, last, first);
        }
    }

    /// Shifts the whole contour by the delta of its one referenced point.
    ///
    /// Modeled after the FreeType implementation: <https://github.com/freetype/freetype/blob/bbfcd79eacb4985d4b68783565f4b494aa64516b/src/truetype/ttgxvar.c#L3776>
    fn shift(&mut self, reference: usize) {
        let delta = self.out_points[reference] - self.points[reference].map(D::from);
        if delta.x == D::zeroed() && delta.y == D::zeroed() {
            return;
        }
        // Every point but the reference itself, which already carries it.
        let (before, rest) = self.out_points.split_at_mut(reference);
        for out_point in before {
            *out_point += delta;
        }
        for out_point in &mut rest[1..] {
            *out_point += delta;
        }
    }

    /// Interpolates the unreferenced points in `range` between the referenced
    /// points at `ref1` and `ref2`.
    ///
    /// Modeled after the FreeType implementation: <https://github.com/freetype/freetype/blob/bbfcd79eacb4985d4b68783565f4b494aa64516b/src/truetype/ttgxvar.c#L3813>
    ///
    /// For details on the algorithm, see: <https://learn.microsoft.com/en-us/typography/opentype/spec/gvar#inferred-deltas-for-un-referenced-point-numbers>
    fn interpolate(&mut self, range: Range<usize>, ref1: usize, ref2: usize) {
        if range.is_empty() {
            return;
        }
        // FreeType uses pointer tricks to handle x and y with a single piece of
        // code. Try a macro instead.
        macro_rules! interp_coord {
            ($coord:ident) => {
                // Order the references by coordinate, which can differ between
                // the two axes.
                let (lo, hi) = if self.points[ref1].$coord > self.points[ref2].$coord {
                    (ref2, ref1)
                } else {
                    (ref1, ref2)
                };
                let in1 = D::from(self.points[lo].$coord);
                let in2 = D::from(self.points[hi].$coord);
                let out1 = self.out_points[lo].$coord;
                let out2 = self.out_points[hi].$coord;
                // If the references share a coordinate but moved apart, the
                // inferred delta is zero and the coordinate is left alone.
                if in1 != in2 || out1 == out2 {
                    let scale = if in1 != in2 {
                        (out2 - out1) / (in2 - in1)
                    } else {
                        D::zeroed()
                    };
                    let d1 = out1 - in1;
                    let d2 = out2 - in2;
                    for (point, out_point) in self.points[range.clone()]
                        .iter()
                        .zip(&mut self.out_points[range.clone()])
                    {
                        let coord = D::from(point.$coord);
                        // Outside the references the nearer delta is applied
                        // wholesale; between them it is interpolated.
                        out_point.$coord = if coord <= in1 {
                            coord + d1
                        } else if coord >= in2 {
                            coord + d2
                        } else {
                            out1 + (coord - in1) * scale
                        };
                    }
                }
            };
        }
        interp_coord!(x);
        interp_coord!(y);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        tables::{
            glyf::{Glyf, Glyph},
            loca::Loca,
        },
        FontRef, TableProvider,
    };
    use alloc::{vec, vec::Vec};

    fn make_points(tuples: &[(i32, i32)]) -> Vec<Point<i32>> {
        tuples.iter().map(|&(x, y)| Point::new(x, y)).collect()
    }

    /// Seeds the working buffer the way [`Gvar::simple_deltas`] does: every
    /// point converted to 16.16 with its explicit delta already applied, and
    /// `HAS_DELTA` set for the points that carry one.
    fn make_working_points_and_flags(
        points: &[Point<i32>],
        deltas: &[Point<i32>],
    ) -> (Vec<Point<Fixed>>, Vec<PointFlags>) {
        let working_points = points
            .iter()
            .zip(deltas)
            .map(|(point, delta)| point.map(Fixed::from_i32) + delta.map(Fixed::from_i32))
            .collect();
        let flags = deltas
            .iter()
            .map(|delta| {
                let mut flags = PointFlags::default();
                if delta.x != 0 || delta.y != 0 {
                    flags.set_marker(PointMarker::HAS_DELTA);
                }
                flags
            })
            .collect();
        (working_points, flags)
    }

    /// Runs interpolation and returns the resulting x coordinates as integers.
    fn interpolated_x(
        points: &[Point<i32>],
        deltas: &[Point<i32>],
        contours: &[u16],
    ) -> Option<Vec<i32>> {
        let (mut working, flags) = make_working_points_and_flags(points, deltas);
        interpolate_deltas(points, &flags, contours, &mut working)?;
        Some(working.iter().map(|p| p.x.to_i32()).collect())
    }

    #[test]
    fn shift() {
        let points = make_points(&[(245, 630), (260, 700), (305, 680)]);
        // Single delta triggers a full contour shift.
        let deltas = make_points(&[(20, -10), (0, 0), (0, 0)]);
        let (mut working_points, flags) = make_working_points_and_flags(&points, &deltas);
        interpolate_deltas(&points, &flags, &[2], &mut working_points).unwrap();
        let expected = &[
            Point::new(265, 620).map(Fixed::from_i32),
            Point::new(280, 690).map(Fixed::from_i32),
            Point::new(325, 670).map(Fixed::from_i32),
        ];
        assert_eq!(&working_points, expected);
    }

    #[test]
    fn interpolate() {
        // Test taken from the spec:
        // https://learn.microsoft.com/en-us/typography/opentype/spec/gvar#inferred-deltas-for-un-referenced-point-numbers
        // with a minor adjustment to account for the precision of our fixed point math.
        let points = make_points(&[(245, 630), (260, 700), (305, 680)]);
        let deltas = make_points(&[(28, -62), (0, 0), (-42, -57)]);
        let (mut working_points, flags) = make_working_points_and_flags(&points, &deltas);
        interpolate_deltas(&points, &flags, &[2], &mut working_points).unwrap();
        assert_eq!(
            working_points[1],
            Point::new(
                Fixed::from_f64(260.0 + 10.4999237060547),
                Fixed::from_f64(700.0 - 57.0)
            )
        );
    }

    /// Points below the first reference take its delta, points above the second
    /// take that one, and points between them are interpolated. Coordinates are
    /// chosen so the interpolation scale is exactly 2 and the 16.16 arithmetic
    /// is exact.
    #[test]
    fn interpolate_clamps_outside_the_reference_range() {
        //                       below    ref1    between   ref2     above
        let points = make_points(&[(0, 0), (10, 0), (15, 0), (20, 0), (30, 0)]);
        let deltas = make_points(&[(0, 0), (4, 0), (0, 0), (14, 0), (0, 0)]);
        assert_eq!(
            interpolated_x(&points, &deltas, &[4]).unwrap(),
            // 0 + d1, 10 + 4, 14 + (15-10)*2, 20 + 14, 30 + d2
            vec![4, 14, 24, 34, 44]
        );
    }

    /// A contour whose points carry no deltas at all is left exactly as it was.
    #[test]
    fn contour_without_deltas_is_untouched() {
        let points = make_points(&[(0, 0), (10, 0), (20, 0)]);
        let deltas = make_points(&[(0, 0), (0, 0), (0, 0)]);
        assert_eq!(
            interpolated_x(&points, &deltas, &[2]).unwrap(),
            vec![0, 10, 20]
        );
    }

    /// Every point having an explicit delta leaves nothing to interpolate.
    #[test]
    fn fully_referenced_contour_is_untouched() {
        let points = make_points(&[(0, 0), (10, 0), (20, 0)]);
        let deltas = make_points(&[(1, 0), (2, 0), (3, 0)]);
        assert_eq!(
            interpolated_x(&points, &deltas, &[2]).unwrap(),
            vec![1, 12, 23]
        );
    }

    /// Deltas in one contour must not leak into another.
    #[test]
    fn contours_are_independent() {
        let points = make_points(&[(0, 0), (10, 0), (20, 0), (100, 0), (110, 0), (120, 0)]);
        // Only the first contour has a delta, so it shifts as a whole while the
        // second stays put.
        let deltas = make_points(&[(5, 0), (0, 0), (0, 0), (0, 0), (0, 0), (0, 0)]);
        assert_eq!(
            interpolated_x(&points, &deltas, &[2, 5]).unwrap(),
            vec![5, 15, 25, 100, 110, 120]
        );
    }

    /// The points before the first referenced point wrap around and use the
    /// last referenced point as their other reference.
    #[test]
    fn points_before_first_reference_wrap_around() {
        let points = make_points(&[(0, 0), (10, 0), (20, 0), (30, 0)]);
        // References at 1 and 2; points 0 and 3 fall outside them and take the
        // delta of the nearer reference.
        let deltas = make_points(&[(0, 0), (6, 0), (6, 0), (0, 0)]);
        assert_eq!(
            interpolated_x(&points, &deltas, &[3]).unwrap(),
            vec![6, 16, 26, 36]
        );
    }

    /// When both references share a coordinate but move differently the
    /// inferred delta is zero, so that coordinate is left alone.
    #[test]
    fn equal_reference_coords_with_different_deltas_infer_nothing() {
        let points = make_points(&[(10, 0), (10, 5), (10, 10)]);
        let deltas = make_points(&[(2, 0), (0, 0), (6, 0)]);
        let (mut working, flags) = make_working_points_and_flags(&points, &deltas);
        interpolate_deltas(&points, &flags, &[2], &mut working).unwrap();
        // x: both references sit at 10 but move differently, so point 1 keeps
        // its x. y: interpolated normally, and with no y deltas it stays at 5.
        assert_eq!(working[1], Point::new(10, 5).map(Fixed::from_i32));
    }

    /// A contour end point that goes backwards is skipped rather than panicking
    /// or corrupting the points around it.
    #[test]
    fn out_of_order_contour_end_is_skipped() {
        let points = make_points(&[(0, 0), (10, 0), (20, 0), (30, 0)]);
        let deltas = make_points(&[(5, 0), (0, 0), (0, 0), (0, 0)]);
        // The second contour ends before the first one did.
        assert_eq!(
            interpolated_x(&points, &deltas, &[3, 1]).unwrap(),
            vec![5, 15, 25, 35]
        );
    }

    /// A contour end point past the end of the point array is reported rather
    /// than read out of bounds.
    #[test]
    fn contour_end_past_last_point_is_rejected() {
        let points = make_points(&[(0, 0), (10, 0)]);
        let deltas = make_points(&[(5, 0), (0, 0)]);
        assert!(interpolated_x(&points, &deltas, &[9]).is_none());
    }

    // ---- end to end, against a real variable font -------------------------

    /// Vazirmatn has a single `wght` axis. Glyph 1 is a simple glyph with
    /// variation data, glyph 2 is a composite with variation data, and glyph 0
    /// is empty and has none.
    const VAR_GID: GlyphId = GlyphId::new(1);
    const COMPOSITE_GID: GlyphId = GlyphId::new(2);
    const NO_VAR_GID: GlyphId = GlyphId::new(0);

    /// A simple glyph loaded with its point buffer sized to include the
    /// phantom points, as [`Gvar::simple_deltas`] requires.
    struct TestGlyph<'a> {
        gvar: Gvar<'a>,
        points: Vec<Point<i32>>,
        flags: Vec<PointFlags>,
        contours: Vec<u16>,
    }

    impl<'a> TestGlyph<'a> {
        fn new(font: &FontRef<'a>, gid: GlyphId) -> Self {
            let glyf: Glyf<'a> = font.glyf().unwrap();
            let loca: Loca<'a> = font.loca(None).unwrap();
            let Some(Glyph::Simple(simple)) = loca.get_glyf(gid, &glyf).unwrap() else {
                panic!("expected a simple glyph");
            };
            let n = simple.num_points();
            let total = n + PHANTOM_POINT_COUNT;
            let mut points = vec![Point::<i32>::default(); total];
            let mut flags = vec![PointFlags::default(); total];
            simple
                .read_points_fast(&mut points[..n], &mut flags[..n])
                .unwrap();
            let contours = simple
                .end_pts_of_contours()
                .iter()
                .map(|c| c.get())
                .collect();
            Self {
                gvar: font.gvar().unwrap(),
                points,
                flags,
                contours,
            }
        }

        fn deltas(&mut self, gid: GlyphId, coords: &[F2Dot14]) -> (bool, Vec<Point<Fixed>>) {
            let total = self.points.len();
            let mut deltas = vec![Point::<Fixed>::default(); total];
            let mut iup = vec![Point::<Fixed>::default(); total];
            let mut buffers = DeltaBuffers {
                deltas: &mut deltas,
                iup: &mut iup,
            };
            let varied = self
                .gvar
                .simple_deltas(
                    gid,
                    coords,
                    &self.points,
                    &mut self.flags,
                    &self.contours,
                    &mut buffers,
                )
                .unwrap();
            (varied, deltas)
        }
    }

    #[test]
    fn simple_deltas_at_default_location_are_zero() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let mut glyph = TestGlyph::new(&font, VAR_GID);
        // The glyph has variation data, but no tuple is active at the default
        // location, so every delta is zero.
        let (varied, deltas) = glyph.deltas(VAR_GID, &[F2Dot14::from_f32(0.0)]);
        assert!(varied);
        assert!(deltas.iter().all(|d| *d == Point::default()));
    }

    #[test]
    fn simple_deltas_at_extreme_move_points() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let mut glyph = TestGlyph::new(&font, VAR_GID);
        let (varied, deltas) = glyph.deltas(VAR_GID, &[F2Dot14::from_f32(1.0)]);
        assert!(varied);
        let outline_deltas = &deltas[..deltas.len() - PHANTOM_POINT_COUNT];
        assert!(
            outline_deltas.iter().any(|d| *d != Point::default()),
            "expected at least one non-zero outline delta"
        );
    }

    /// Deltas scale with position along the axis: half way along moves points,
    /// and by less than the extreme does.
    #[test]
    fn simple_deltas_scale_along_the_axis() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let mut glyph = TestGlyph::new(&font, VAR_GID);
        let (_, half) = glyph.deltas(VAR_GID, &[F2Dot14::from_f32(0.5)]);
        let (_, full) = glyph.deltas(VAR_GID, &[F2Dot14::from_f32(1.0)]);
        let magnitude = |ds: &[Point<Fixed>]| -> f64 {
            ds.iter()
                .map(|d| d.x.to_f64().abs() + d.y.to_f64().abs())
                .sum()
        };
        let (half_sum, full_sum) = (magnitude(&half), magnitude(&full));
        assert!(half_sum > 0.0);
        assert!(
            half_sum < full_sum,
            "half {half_sum} should be less than full {full_sum}"
        );
    }

    /// Repeated calls must not accumulate: each starts from a zeroed buffer.
    #[test]
    fn deltas_do_not_accumulate_across_calls() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let mut glyph = TestGlyph::new(&font, VAR_GID);
        let (_, once) = glyph.deltas(VAR_GID, &[F2Dot14::from_f32(1.0)]);
        let (_, twice) = glyph.deltas(VAR_GID, &[F2Dot14::from_f32(1.0)]);
        assert_eq!(once, twice);
    }

    /// A glyph with no variation data reports `false` and leaves the buffer
    /// zeroed rather than untouched, so a reused buffer cannot leak deltas from
    /// a previously processed glyph.
    #[test]
    fn simple_deltas_without_variation_data_zeroes_buffer() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let mut glyph = TestGlyph::new(&font, VAR_GID);
        let (varied, deltas) = glyph.deltas(NO_VAR_GID, &[F2Dot14::from_f32(1.0)]);
        assert!(!varied);
        assert!(deltas.iter().all(|d| *d == Point::default()));
    }

    #[test]
    fn simple_deltas_rejects_short_iup_buffer() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let gvar = font.gvar().unwrap();
        let points = [Point::<i32>::default(); 8];
        let mut flags = [PointFlags::default(); 8];
        let mut deltas = [Point::<Fixed>::default(); 8];
        let mut iup = [Point::<Fixed>::default(); 4];
        let mut buffers = DeltaBuffers {
            deltas: &mut deltas,
            iup: &mut iup,
        };
        assert!(matches!(
            gvar.simple_deltas(VAR_GID, &[], &points, &mut flags, &[7], &mut buffers),
            Err(ReadError::InvalidArrayLen)
        ));
    }

    #[test]
    fn simple_deltas_rejects_missing_phantom_points() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let gvar = font.gvar().unwrap();
        // Fewer points than there are phantom points.
        let points = [Point::<i32>::default(); 3];
        let mut flags = [PointFlags::default(); 3];
        let mut deltas = [Point::<Fixed>::default(); 3];
        let mut iup = [Point::<Fixed>::default(); 3];
        let mut buffers = DeltaBuffers {
            deltas: &mut deltas,
            iup: &mut iup,
        };
        assert!(matches!(
            gvar.simple_deltas(VAR_GID, &[], &points, &mut flags, &[2], &mut buffers),
            Err(ReadError::InvalidArrayLen)
        ));
    }

    #[test]
    fn composite_deltas_without_variation_data_zeroes_buffer() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let gvar = font.gvar().unwrap();
        let coords = [F2Dot14::from_f32(0.5)];
        let mut deltas = [Point::new(Fixed::from_i32(7), Fixed::from_i32(9)); 8];
        let varied = gvar
            .composite_deltas(NO_VAR_GID, &coords, &mut deltas)
            .unwrap();
        assert!(!varied);
        assert!(deltas.iter().all(|d| *d == Point::default()));
    }

    #[test]
    fn composite_deltas_with_variation_data_reports_true() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let gvar = font.gvar().unwrap();
        let coords = [F2Dot14::from_f32(1.0)];
        let mut deltas = [Point::<Fixed>::default(); 8];
        let varied = gvar
            .composite_deltas(COMPOSITE_GID, &coords, &mut deltas)
            .unwrap();
        assert!(varied);
    }

    /// Deltas whose position falls past the end of the buffer are ignored
    /// rather than treated as an error, since a composite may legitimately have
    /// fewer components than the variation data describes.
    #[test]
    fn composite_deltas_tolerates_short_buffer() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let gvar = font.gvar().unwrap();
        let coords = [F2Dot14::from_f32(1.0)];
        let mut deltas = [Point::<Fixed>::default(); 1];
        assert!(gvar
            .composite_deltas(COMPOSITE_GID, &coords, &mut deltas)
            .is_ok());
    }

    /// Going through `Gvar` is the same as looking the data up and calling
    /// `GlyphVariationData` directly.
    #[test]
    fn both_layers_agree() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let gvar = font.gvar().unwrap();
        let glyf = font.glyf().unwrap();
        let loca = font.loca(None).unwrap();
        let coords = [F2Dot14::from_f32(0.5)];
        for gid in 0..font.maxp().unwrap().num_glyphs() {
            let glyph_id = GlyphId::from(gid);
            let Some(Glyph::Simple(simple)) = loca.get_glyf(glyph_id, &glyf).unwrap() else {
                continue;
            };
            let count = simple.num_points() + PHANTOM_POINT_COUNT;
            let points = vec![Point::<i32>::default(); count];
            let contours: Vec<u16> = simple
                .end_pts_of_contours()
                .iter()
                .map(|e| e.get())
                .collect();

            let mut through_gvar = vec![Point::<Fixed>::default(); count];
            let mut iup = vec![Point::<Fixed>::default(); count];
            let mut flags = vec![PointFlags::default(); count];
            let had_data = gvar
                .simple_deltas(
                    glyph_id,
                    &coords,
                    &points,
                    &mut flags,
                    &contours,
                    &mut DeltaBuffers {
                        deltas: &mut through_gvar,
                        iup: &mut iup,
                    },
                )
                .unwrap();

            let var_data = gvar.glyph_variation_data(glyph_id).unwrap();
            assert_eq!(had_data, var_data.is_some(), "gid {gid}");
            let Some(var_data) = var_data else { continue };

            let mut direct = vec![Point::<Fixed>::default(); count];
            let mut iup = vec![Point::<Fixed>::default(); count];
            let mut flags = vec![PointFlags::default(); count];
            var_data
                .simple_deltas(
                    &coords,
                    &points,
                    &mut flags,
                    &contours,
                    &mut DeltaBuffers {
                        deltas: &mut direct,
                        iup: &mut iup,
                    },
                )
                .unwrap();
            assert_eq!(through_gvar, direct, "gid {gid}");
        }
    }

    /// The `Gvar` wrapper zeroes the deltas for a glyph with no variation
    /// data, so a reused buffer never leaks the previous glyph's values.
    #[test]
    fn a_glyph_without_data_still_zeroes() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let gvar = font.gvar().unwrap();
        // Past the end of the table, so there is certainly nothing there.
        let missing = GlyphId::new(0xFFFF);
        let mut deltas = vec![Point::new(Fixed::from_i32(7), Fixed::from_i32(9)); 8];
        assert!(!gvar.composite_deltas(missing, &[], &mut deltas).unwrap());
        assert!(deltas.iter().all(|d| *d == Point::default()));

        let points = vec![Point::<i32>::default(); 8];
        let mut deltas = vec![Point::new(Fixed::from_i32(7), Fixed::from_i32(9)); 8];
        let mut iup = vec![Point::<Fixed>::default(); 8];
        let mut flags = vec![PointFlags::default(); 8];
        assert!(!gvar
            .simple_deltas(
                missing,
                &[],
                &points,
                &mut flags,
                &[7],
                &mut DeltaBuffers {
                    deltas: &mut deltas,
                    iup: &mut iup,
                },
            )
            .unwrap());
        assert!(deltas.iter().all(|d| *d == Point::default()));
    }

    /// Every buffer that is indexed by point is checked against the point
    /// count, by both entry points, before anything is written.
    #[test]
    fn short_buffers_are_rejected_by_both() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let gvar = font.gvar().unwrap();
        let glyph_id = GlyphId::new(2);
        let var_data = gvar.glyph_variation_data(glyph_id).unwrap().unwrap();
        const COUNT: usize = 8;

        // Each case leaves one buffer one element short of `points`.
        for short in ["flags", "deltas", "iup", "points"] {
            let points = vec![Point::<i32>::default(); if short == "points" { 3 } else { COUNT }];
            let mut flags =
                vec![PointFlags::default(); if short == "flags" { COUNT - 1 } else { COUNT }];
            let mut deltas =
                vec![Point::<Fixed>::default(); if short == "deltas" { COUNT - 1 } else { COUNT }];
            let mut iup =
                vec![Point::<Fixed>::default(); if short == "iup" { COUNT - 1 } else { COUNT }];
            let mut buffers = DeltaBuffers {
                deltas: &mut deltas,
                iup: &mut iup,
            };
            assert!(
                matches!(
                    gvar.simple_deltas(glyph_id, &[], &points, &mut flags, &[7], &mut buffers),
                    Err(ReadError::InvalidArrayLen)
                ),
                "Gvar accepted a short {short}"
            );
            assert!(
                matches!(
                    var_data.simple_deltas(&[], &points, &mut flags, &[7], &mut buffers),
                    Err(ReadError::InvalidArrayLen)
                ),
                "GlyphVariationData accepted a short {short}"
            );
        }

        // The same sizes, none of them short, are accepted.
        let points = vec![Point::<i32>::default(); COUNT];
        let mut flags = vec![PointFlags::default(); COUNT];
        let mut deltas = vec![Point::<Fixed>::default(); COUNT];
        let mut iup = vec![Point::<Fixed>::default(); COUNT];
        let mut buffers = DeltaBuffers {
            deltas: &mut deltas,
            iup: &mut iup,
        };
        assert!(gvar
            .simple_deltas(glyph_id, &[], &points, &mut flags, &[7], &mut buffers)
            .is_ok());
    }
}
