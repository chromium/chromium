//! An outline that owns the buffers it is loaded into.

use super::{
    outline_to_path, Buffers, Hinter, OutlineContext, OutlineError, OutlinePlan, OutlineRef,
    PathContourStart, Scale, Scale26Dot6, ScaleF32, Unscaled,
};
use crate::{
    model::glyph::outline::OutlinePen,
    tables::glyf::{PointFlags, PHANTOM_POINT_COUNT},
    types::{F26Dot6, GlyphId, Point},
};
use alloc::vec::Vec;

/// An outline and the buffers that hold it.
///
/// [`OutlinePlan::load`] requires the caller to size and supply seven
/// buffers. This owns them, grows them as needed and reuses them across
/// glyphs, so loading stops allocating after the first few.
///
/// Loading returns an [`OutlineRef`] borrowing those buffers. The same data is
/// reachable through [`points`](Self::points) and its neighbours once that
/// borrow ends.
///
/// ```
/// # fn main() -> Result<(), Box<dyn std::error::Error>> {
/// use read_fonts::{
///     tables::glyf::outline::{Outline, OutlineTables, Scale26Dot6},
///     types::GlyphId,
///     FontRef,
/// };
///
/// let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE)?;
/// let tables = OutlineTables::new(&font)?;
/// let mut outline = Outline::<Scale26Dot6>::new();
///
/// // Borrows `outline`'s buffers for as long as it is held.
/// let loaded = outline.load(&tables, GlyphId::new(1), Some(16.0))?;
/// for (point, flags) in loaded.points().iter().zip(loaded.flags()) {
///     let _ = (point, flags.is_off_curve_quad());
/// }
///
/// // Once it is dropped, the outline can be read through its accessors.
/// assert_eq!(outline.points().len(), outline.flags().len());
/// # Ok(())
/// # }
/// ```
pub struct Outline<S: Scale> {
    points: Vec<Point<S::Coord>>,
    flags: Vec<PointFlags>,
    contours: Vec<u16>,
    phantom: [Point<S::Coord>; PHANTOM_POINT_COUNT],
    unscaled: Vec<Point<i32>>,
    deltas: Vec<Point<S::Delta>>,
    iup: Vec<Point<S::Delta>>,
    composite_deltas: Vec<Point<S::Delta>>,
    point_count: u32,
    contour_count: u32,
}

impl<S: Scale> Outline<S> {
    /// Creates an empty outline. The buffers are allocated on first use.
    pub fn new() -> Self {
        Self {
            points: Vec::new(),
            flags: Vec::new(),
            contours: Vec::new(),
            phantom: Default::default(),
            unscaled: Vec::new(),
            deltas: Vec::new(),
            iup: Vec::new(),
            composite_deltas: Vec::new(),
            point_count: 0,
            contour_count: 0,
        }
    }

    /// The loaded points, excluding phantom points.
    pub fn points(&self) -> &[Point<S::Coord>] {
        &self.points[..self.point_count as usize]
    }

    /// The flag for each point in [`Self::points`].
    pub fn flags(&self) -> &[PointFlags] {
        &self.flags[..self.point_count as usize]
    }

    /// The index of the last point of each contour.
    pub fn contours(&self) -> &[u16] {
        &self.contours[..self.contour_count as usize]
    }

    /// The four phantom points, after scaling, variation and hinting.
    pub fn phantom_points(&self) -> &[Point<S::Coord>; PHANTOM_POINT_COUNT] {
        &self.phantom
    }

    /// The left side bearing after scaling, variation and hinting.
    pub fn adjusted_lsb(&self) -> S::Coord {
        self.phantom[0].x
    }

    /// The advance width after scaling, variation and hinting.
    pub fn adjusted_advance_width(&self) -> S::Coord {
        self.phantom[1].x - self.phantom[0].x
    }

    /// Converts the outline to path commands, calling `pen` for each.
    ///
    /// See [`OutlineRef::to_path`] for what `start` decides and when this
    /// returns `false`.
    #[must_use]
    pub fn to_path(&self, start: PathContourStart, pen: &mut impl OutlinePen) -> bool {
        outline_to_path(self.points(), self.flags(), self.contours(), start, pen)
    }

    /// Loads `glyph` at `scale`, replacing any previous contents.
    ///
    /// The per-scale `load` methods build the scale. Use this to supply a
    /// hinter, or to drive a scale generically.
    pub fn load_with<'a>(
        &mut self,
        context: &'a dyn OutlineContext<'a>,
        glyph: GlyphId,
        scale: &S,
        hinter: Option<&mut dyn Hinter<S::Coord>>,
    ) -> Result<OutlineRef<'_, S::Coord>, OutlineError> {
        let plan = OutlinePlan::new(context, glyph)?;
        let lengths = plan.buffer_lengths::<S>();
        grow(&mut self.points, lengths.points);
        grow(&mut self.flags, lengths.points);
        grow(&mut self.contours, lengths.contours);
        grow(&mut self.unscaled, lengths.unscaled);
        grow(&mut self.deltas, lengths.deltas);
        grow(&mut self.iup, lengths.iup);
        grow(&mut self.composite_deltas, lengths.composite_deltas);
        let buffers = Buffers {
            points: &mut self.points,
            flags: &mut self.flags,
            contours: &mut self.contours,
            unscaled: &mut self.unscaled,
            deltas: &mut self.deltas,
            iup: &mut self.iup,
            composite_deltas: &mut self.composite_deltas,
        };
        let outline = plan.load(context, scale, buffers, hinter)?;
        // Kept so the accessors work once the returned borrow is released.
        self.point_count = outline.points().len() as u32;
        self.contour_count = outline.contours().len() as u32;
        self.phantom = *outline.phantom_points();
        Ok(outline)
    }
}

impl<S: Scale> Default for Outline<S> {
    fn default() -> Self {
        Self::new()
    }
}

impl Outline<Scale26Dot6> {
    /// Loads `glyph` at `ppem`, in 26.6 pixel coordinates.
    ///
    /// A `ppem` of `None` leaves the result in font units, shifted left by
    /// six. The location comes from `context`.
    pub fn load<'a>(
        &mut self,
        context: &'a dyn OutlineContext<'a>,
        glyph: GlyphId,
        ppem: Option<f32>,
    ) -> Result<OutlineRef<'_, F26Dot6>, OutlineError> {
        let scale = Scale26Dot6::new(ppem, context.units_per_em());
        self.load_with(context, glyph, &scale, None)
    }

    /// Loads `glyph` at `ppem`, running the glyph's TrueType
    /// instructions through `hinter`.
    pub fn load_hinted<'a>(
        &mut self,
        context: &'a dyn OutlineContext<'a>,
        glyph: GlyphId,
        ppem: Option<f32>,
        hinter: &mut dyn Hinter<F26Dot6>,
    ) -> Result<OutlineRef<'_, F26Dot6>, OutlineError> {
        let scale = Scale26Dot6::new(ppem, context.units_per_em());
        self.load_with(context, glyph, &scale, Some(hinter))
    }
}

impl Outline<ScaleF32> {
    /// Loads `glyph` at `ppem`, in floating point pixel coordinates.
    ///
    /// A `ppem` of `None` leaves the result in font units. The location comes
    /// from `context`.
    pub fn load<'a>(
        &mut self,
        context: &'a dyn OutlineContext<'a>,
        glyph: GlyphId,
        ppem: Option<f32>,
    ) -> Result<OutlineRef<'_, f32>, OutlineError> {
        let scale = ScaleF32::new(ppem, context.units_per_em());
        self.load_with(context, glyph, &scale, None)
    }
}

impl Outline<Unscaled> {
    /// Loads `glyph` in font units.
    ///
    /// There is no size to pass: variations apply and components are placed,
    /// but nothing is scaled. The location comes from `context`.
    pub fn load<'a>(
        &mut self,
        context: &'a dyn OutlineContext<'a>,
        glyph: GlyphId,
    ) -> Result<OutlineRef<'_, i32>, OutlineError> {
        self.load_with(context, glyph, &Unscaled, None)
    }
}

/// Grows `buf` to at least `len`, without ever shrinking it.
fn grow<T: Copy + Default>(buf: &mut Vec<T>, len: u32) {
    let len = len as usize;
    if buf.len() < len {
        buf.resize(len, T::default());
    }
}

#[cfg(test)]
mod tests {
    use super::super::OutlineTables;
    use super::*;
    use crate::{types::F2Dot14, FontRef, TableProvider};
    use alloc::{vec, vec::Vec};

    /// The owning API agrees with driving [`OutlinePlan::load`] by hand.
    #[test]
    fn matches_the_low_level_api() {
        let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        let scale = Scale26Dot6::new(Some(16.0), context.units_per_em);
        let mut owned = Outline::<Scale26Dot6>::new();
        for gid in 0..font.maxp().unwrap().num_glyphs() {
            let gid = GlyphId::from(gid);
            let plan = OutlinePlan::new(&context, gid).unwrap();
            let lengths = plan.buffer_lengths::<Scale26Dot6>();
            let mut points = vec![Point::default(); lengths.points as usize];
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
            let expected = plan.load(&context, &scale, buffers, None).unwrap();
            let expected = (
                expected.points().to_vec(),
                expected.contours().to_vec(),
                expected.phantom_points(),
            );
            let actual = owned.load(&context, gid, Some(16.0)).unwrap();
            assert_eq!(actual.points(), expected.0, "gid {gid}");
            assert_eq!(actual.contours(), expected.1, "gid {gid}");
            assert_eq!(actual.phantom_points(), expected.2, "gid {gid}");
        }
    }

    /// The accessors see the same outline as the returned borrow.
    #[test]
    fn accessors_match_the_returned_outline() {
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        let mut owned = Outline::<Scale26Dot6>::new();
        let (points, contours, phantom, lsb, advance) = {
            let out = owned.load(&context, GlyphId::new(5), None).unwrap();
            (
                out.points().to_vec(),
                out.contours().to_vec(),
                *out.phantom_points(),
                out.adjusted_lsb(),
                out.adjusted_advance_width(),
            )
        };
        assert!(!points.is_empty());
        assert_eq!(owned.points(), points);
        assert_eq!(owned.contours(), contours);
        assert_eq!(owned.phantom_points(), &phantom);
        assert_eq!(owned.adjusted_lsb(), lsb);
        assert_eq!(owned.adjusted_advance_width(), advance);
    }

    /// Buffers grow to fit the largest glyph and are then reused; a smaller
    /// glyph after a larger one still reports only its own points.
    #[test]
    fn buffers_grow_and_are_reused() {
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        let mut owned = Outline::<Scale26Dot6>::new();
        // Glyph 5 is a composite of two copies of glyph 1, so it is larger.
        owned.load(&context, GlyphId::new(5), None).unwrap();
        let big = owned.points().len();
        let capacity = owned.points.len();
        owned.load(&context, GlyphId::new(1), None).unwrap();
        assert!(owned.points().len() < big);
        assert_eq!(owned.points.len(), capacity, "buffers must not shrink");
        assert_eq!(owned.points().len(), owned.flags().len());
    }

    /// Nothing carries over from one glyph to the next.
    #[test]
    fn reuse_does_not_leak_between_glyphs() {
        let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        let mut owned = Outline::<Scale26Dot6>::new();
        let first = owned
            .load(&context, GlyphId::new(1), Some(24.0))
            .unwrap()
            .points
            .to_vec();
        // Run a different glyph through the same buffers, then come back.
        owned.load(&context, GlyphId::new(3), Some(24.0)).unwrap();
        let again = owned.load(&context, GlyphId::new(1), Some(24.0)).unwrap();
        assert_eq!(again.points(), first);
    }

    /// Font units are what 26.6 produces at no size, shifted right by six.
    #[test]
    fn unscaled_agrees_with_26dot6_at_no_size() {
        let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        let mut fixed = Outline::<Scale26Dot6>::new();
        let mut raw = Outline::<Unscaled>::new();
        for gid in 0..font.maxp().unwrap().num_glyphs() {
            let gid = GlyphId::from(gid);
            let expected: Vec<_> = fixed
                .load(&context, gid, None)
                .unwrap()
                .points
                .iter()
                .map(|point| point.map(|coord| coord.to_bits() >> 6))
                .collect();
            let actual = raw.load(&context, gid).unwrap();
            assert_eq!(actual.points(), expected, "gid {gid}");
        }
    }

    /// The f32 scale tracks the 26.6 scale to within a 26.6 unit.
    #[test]
    fn f32_tracks_26dot6() {
        let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        let mut fixed = Outline::<Scale26Dot6>::new();
        let mut float = Outline::<ScaleF32>::new();
        for gid in 0..font.maxp().unwrap().num_glyphs() {
            let gid = GlyphId::from(gid);
            let expected = fixed
                .load(&context, gid, Some(32.0))
                .unwrap()
                .points
                .to_vec();
            let actual = float.load(&context, gid, Some(32.0)).unwrap();
            for (expected, actual) in expected.iter().zip(actual.points()) {
                assert!(
                    (expected.x.to_f32() - actual.x).abs() < 1.0 / 64.0,
                    "gid {gid}"
                );
                assert!(
                    (expected.y.to_f32() - actual.y).abs() < 1.0 / 64.0,
                    "gid {gid}"
                );
            }
        }
    }

    /// Variation coordinates reach the loader and move the outline.
    #[test]
    fn coords_vary_the_outline() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        assert!(context.gvar.is_some());
        let mut owned = Outline::<Scale26Dot6>::new();
        let default = owned
            .load(&context, GlyphId::new(2), Some(16.0))
            .unwrap()
            .points
            .to_vec();
        let coords = [F2Dot14::from_f32(1.0)];
        let context = context.at(&coords, &[]);
        let varied = owned.load(&context, GlyphId::new(2), Some(16.0)).unwrap();
        assert_eq!(varied.points().len(), default.len());
        assert_ne!(varied.points(), default);
    }

    /// `OutlineTables` supplies the metrics, so phantom points carry the
    /// glyph's real advance.
    #[test]
    fn tables_supply_phantom_metrics() {
        let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        let hmtx = font.hmtx().unwrap();
        let mut owned = Outline::<Unscaled>::new();
        for gid in 0..font.maxp().unwrap().num_glyphs() {
            let gid = GlyphId::from(gid);
            let out = owned.load(&context, gid).unwrap();
            assert_eq!(
                out.adjusted_advance_width(),
                hmtx.advance(gid).unwrap() as i32,
                "gid {gid}"
            );
        }
    }
}
