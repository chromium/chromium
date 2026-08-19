//! Simple glyphs (glyphs which do not contain components)

use crate::{
    from_obj::{FromObjRef, FromTableRef, ToOwnedTable},
    util::{self, MultiZip, WrappingGet},
    FontWrite, OtRound,
};

use kurbo::BezPath;
use read_fonts::{tables::glyf::SimpleGlyphFlags, FontRead, ReadArgs};

pub use read_fonts::tables::glyf::CurvePoint;

use super::Bbox;

/// A simple (without components) glyph
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct SimpleGlyph {
    pub bbox: Bbox,
    pub contours: Vec<Contour>,
    pub instructions: Vec<u8>,
    /// If set, the contours of the glyph overlap
    ///
    /// The OVERLAP_SIMPLE bit will be set on the first point of the first contour if this is true.
    pub overlaps: bool,
}

/// A single contour, comprising only line and quadratic bezier segments
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Contour(Vec<CurvePoint>);

/// An error if an input curve is malformed
#[derive(Clone, Debug)]
#[non_exhaustive]
pub enum MalformedPath {
    HasCubic,
    TooSmall,
    MissingMove,
    UnequalNumberOfElements(Vec<usize>),
    InconsistentPathElements(usize, Vec<&'static str>),
}

impl SimpleGlyph {
    /// Attempt to create a simple glyph from a kurbo `BezPath`
    ///
    /// The path may contain only line and quadratic bezier segments. The caller
    /// is responsible for converting any cubic segments to quadratics before
    /// calling.
    ///
    /// Returns an error if the input path is malformed; that is, if it is empty,
    /// contains cubic segments, or does not begin with a 'move' instruction.
    ///
    /// **Context**
    ///
    /// * In the glyf table simple (contour based) glyph paths implicitly close when rendering.
    /// * In font sources, and svg, open and closed paths are distinct.
    ///    * In SVG closure matters due to influence on strokes, <https://www.w3.org/TR/SVG11/paths.html#PathDataClosePathCommand>.
    /// * An explicit closePath joins the first/last points of a contour
    ///    * This is not the same as ending with some other drawing command whose endpoint is the contour startpoint
    /// * In FontTools endPath says I'm done with this subpath, [BezPath] has no endPath.
    ///
    /// Context courtesy of @anthrotype.
    pub fn from_bezpath(path: &BezPath) -> Result<Self, MalformedPath> {
        Self::interpolatable_glyphs_from_bezpaths(std::slice::from_ref(path))
            .map(|mut x| x.pop().unwrap())
    }

    /// Attempt to create a set of interpolation-compatible glyphs from a set
    /// of paths.
    ///
    /// The paths are expected to be preprocessed, and interpolation compatible
    /// (i.e. they should have the same number and type of points, in the same
    /// order.) They should contain only line and quadratic segments; the caller
    /// is responsible for converting cubics to quadratics as needed.
    ///
    /// This method is provided for use when compiling variable fonts.
    /// The inputs are expected to be different instances of the same named
    /// glyph, each corresponding to a different location in the variation
    /// space.
    pub fn interpolatable_glyphs_from_bezpaths(
        paths: &[BezPath],
    ) -> Result<Vec<Self>, MalformedPath> {
        simple_glyphs_from_kurbo(paths)
    }

    /// Compute the flags and deltas for this glyph's points.
    ///
    /// This does not do the final binary encoding, and it also does not handle
    /// repeating flags, which doesn't really work when we're an iterator.
    ///
    // this is adapted from simon's implementation at
    // https://github.com/simoncozens/rust-font-tools/blob/105436d3a617ddbebd25f790b041ff506bd90d44/fonttools-rs/src/tables/glyf/glyph.rs#L268
    fn compute_point_deltas(
        &self,
    ) -> impl Iterator<Item = (SimpleGlyphFlags, CoordDelta, CoordDelta)> + '_ {
        // reused for x & y by passing in the flags
        fn flag_and_delta(
            value: i16,
            short_flag: SimpleGlyphFlags,
            same_or_pos: SimpleGlyphFlags,
        ) -> (SimpleGlyphFlags, CoordDelta) {
            const SHORT_MAX: i16 = u8::MAX as i16;
            const SHORT_MIN: i16 = -SHORT_MAX;
            match value {
                0 => (same_or_pos, CoordDelta::Skip),
                SHORT_MIN..=-1 => (short_flag, CoordDelta::Short(value.unsigned_abs() as u8)),
                1..=SHORT_MAX => (short_flag | same_or_pos, CoordDelta::Short(value as _)),
                _other => (SimpleGlyphFlags::empty(), CoordDelta::Long(value)),
            }
        }

        let (mut last_x, mut last_y) = (0, 0);
        let mut iter = self.contours.iter().flat_map(|c| c.iter());
        std::iter::from_fn(move || {
            let point = iter.next()?;
            let mut flag = SimpleGlyphFlags::empty();
            let d_x = point.x - last_x;
            let d_y = point.y - last_y;
            last_x = point.x;
            last_y = point.y;

            if point.on_curve {
                flag |= SimpleGlyphFlags::ON_CURVE_POINT;
            }
            let (x_flag, x_data) = flag_and_delta(
                d_x,
                SimpleGlyphFlags::X_SHORT_VECTOR,
                SimpleGlyphFlags::X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR,
            );
            let (y_flag, y_data) = flag_and_delta(
                d_y,
                SimpleGlyphFlags::Y_SHORT_VECTOR,
                SimpleGlyphFlags::Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR,
            );

            flag |= x_flag | y_flag;
            Some((flag, x_data, y_data))
        })
    }

    /// Recompute the Glyph's bounding box based on the current contours
    pub fn recompute_bounding_box(&mut self) {
        let mut points = self
            .contours
            .iter()
            .flat_map(|c| c.iter())
            .map(|p| (p.x, p.y));

        if let Some((mut x_min, mut y_min)) = points.next() {
            let mut x_max = x_min;
            let mut y_max = y_min;
            for (x, y) in points {
                x_min = x_min.min(x);
                y_min = y_min.min(y);
                x_max = x_max.max(x);
                y_max = y_max.max(y);
            }
            self.bbox = Bbox {
                x_min,
                y_min,
                x_max,
                y_max,
            };
        }
    }
}

impl Contour {
    /// The total number of points in this contour
    pub fn len(&self) -> usize {
        self.0.len()
    }

    /// `true` if this contour is empty
    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }

    pub fn iter(&self) -> impl Iterator<Item = &CurvePoint> {
        self.0.iter()
    }
}

impl From<Vec<CurvePoint>> for Contour {
    fn from(points: Vec<CurvePoint>) -> Self {
        Self(points)
    }
}

impl From<Contour> for Vec<CurvePoint> {
    fn from(contour: Contour) -> Self {
        contour.0
    }
}

impl MalformedPath {
    fn inconsistent_path_els(idx: usize, elements: &[kurbo::PathEl]) -> Self {
        fn el_types(elements: &[kurbo::PathEl]) -> Vec<&'static str> {
            elements
                .iter()
                .map(|el| match el {
                    kurbo::PathEl::MoveTo(_) => "M",
                    kurbo::PathEl::LineTo(_) => "L",
                    kurbo::PathEl::QuadTo(_, _) => "Q",
                    kurbo::PathEl::CurveTo(_, _, _) => "C",
                    kurbo::PathEl::ClosePath => "Z",
                })
                .collect()
        }

        MalformedPath::InconsistentPathElements(idx, el_types(elements))
    }
}

/// A little helper for managing how we're representing a given delta
#[derive(Clone, Copy, Debug)]
enum CoordDelta {
    // this is a repeat (set in the flag) and so we write nothing
    Skip,
    Short(u8),
    Long(i16),
}

impl FontWrite for CoordDelta {
    fn write_into(&self, writer: &mut crate::TableWriter) {
        match self {
            CoordDelta::Skip => (),
            CoordDelta::Short(val) => val.write_into(writer),
            CoordDelta::Long(val) => val.write_into(writer),
        }
    }
}

impl FromObjRef<read_fonts::tables::glyf::SimpleGlyph<'_>> for SimpleGlyph {
    fn from_obj_ref(
        from: &read_fonts::tables::glyf::SimpleGlyph,
        _data: read_fonts::FontData,
    ) -> Self {
        let bbox = Bbox {
            x_min: from.x_min(),
            y_min: from.y_min(),
            x_max: from.x_max(),
            y_max: from.y_max(),
        };
        let mut points = from.points();
        let mut last_end = 0;
        let mut contours = vec![];
        for end_pt in from.end_pts_of_contours() {
            let end = end_pt.get() as usize + 1;
            let count = end - last_end;
            last_end = end;
            contours.push(Contour(points.by_ref().take(count).collect()));
        }
        Self {
            bbox,
            contours,
            instructions: from.instructions().to_owned(),
            overlaps: from.has_overlapping_contours(),
        }
    }
}

impl FromTableRef<read_fonts::tables::glyf::SimpleGlyph<'_>> for SimpleGlyph {}

impl ReadArgs for SimpleGlyph {
    type Args = ();
}

impl<'a> FontRead<'a> for SimpleGlyph {
    fn read_with_args(
        data: read_fonts::FontData<'a>,
        _: (),
    ) -> Result<Self, read_fonts::ReadError> {
        read_fonts::tables::glyf::SimpleGlyph::read(data).map(|g| g.to_owned_table())
    }
}

impl FontWrite for SimpleGlyph {
    fn write_into(&self, writer: &mut crate::TableWriter) {
        assert!(self.contours.len() < i16::MAX as usize);
        assert!(self.instructions.len() < u16::MAX as usize);
        let n_contours = self.contours.len() as i16;
        if n_contours == 0 {
            // we don't bother writing empty glyphs
            return;
        }
        n_contours.write_into(writer);
        self.bbox.write_into(writer);
        // now write end points of contours:
        let mut cur = 0;
        for contour in &self.contours {
            cur += contour.len();
            (cur as u16 - 1).write_into(writer);
        }
        (self.instructions.len() as u16).write_into(writer);
        self.instructions.write_into(writer);

        let mut deltas = self.compute_point_deltas().collect::<Vec<_>>();
        if self.overlaps {
            if let Some((flags, _, _)) = deltas.first_mut() {
                flags.insert(SimpleGlyphFlags::OVERLAP_SIMPLE);
            }
        }
        RepeatableFlag::iter_from_flags(deltas.iter().map(|(flag, _, _)| *flag))
            .for_each(|flag| flag.write_into(writer));
        deltas.iter().for_each(|(_, x, _)| x.write_into(writer));
        deltas.iter().for_each(|(_, _, y)| y.write_into(writer));
        writer.pad_to_2byte_aligned();
    }
}

/// A little helper for writing flags that may have a 'repeat' byte
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct RepeatableFlag {
    flag: SimpleGlyphFlags,
    repeat: u8,
}

impl FontWrite for RepeatableFlag {
    fn write_into(&self, writer: &mut crate::TableWriter) {
        debug_assert_eq!(
            self.flag.contains(SimpleGlyphFlags::REPEAT_FLAG),
            self.repeat > 0
        );

        self.flag.bits().write_into(writer);
        if self.flag.contains(SimpleGlyphFlags::REPEAT_FLAG) {
            self.repeat.write_into(writer);
        }
    }
}

impl RepeatableFlag {
    /// given an iterator over raw flags, return an iterator over flags + repeat values
    // writing this as an iterator instead of just returning a vec is very marginal
    // gains, but I'm just in the habit at this point
    fn iter_from_flags(
        flags: impl IntoIterator<Item = SimpleGlyphFlags>,
    ) -> impl Iterator<Item = RepeatableFlag> {
        let mut iter = flags.into_iter();
        let mut prev = None;
        // if a flag repeats exactly once, then there is no (space) cost difference
        // between 1) using a repeat flag followed by a value of '1' and 2) just
        // repeating the flag (without setting the repeat bit).
        // It would be simplest for us to go with option 1), but fontmake goes
        // with 2). We like doing what fontmake does, so we add an extra step
        // where if we see a case where there's a single repeat, we split it into
        // two separate non-repeating flags.
        let mut decompose_single_repeat = None;

        std::iter::from_fn(move || loop {
            if let Some(repeat) = decompose_single_repeat.take() {
                return Some(repeat);
            }

            match (iter.next(), prev.take()) {
                (None, Some(RepeatableFlag { flag, repeat: 1 })) => {
                    let flag = flag & !SimpleGlyphFlags::REPEAT_FLAG;
                    decompose_single_repeat = Some(RepeatableFlag { flag, repeat: 0 });
                    return decompose_single_repeat;
                }
                (None, prev) => return prev,
                (Some(flag), None) => prev = Some(RepeatableFlag { flag, repeat: 0 }),
                (Some(flag), Some(mut last)) => {
                    if (last.flag & !SimpleGlyphFlags::REPEAT_FLAG) == flag && last.repeat < u8::MAX
                    {
                        last.repeat += 1;
                        last.flag |= SimpleGlyphFlags::REPEAT_FLAG;
                        prev = Some(last);
                    } else {
                        // split a single repeat into two non-repeat flags
                        if last.repeat == 1 {
                            last.flag &= !SimpleGlyphFlags::REPEAT_FLAG;
                            last.repeat = 0;
                            // stash the extra flag, which we'll use at the top
                            // of the next pass of the loop
                            decompose_single_repeat = Some(last);
                        }
                        prev = Some(RepeatableFlag { flag, repeat: 0 });
                        return Some(last);
                    }
                }
            }
        })
    }
}

impl crate::validate::Validate for SimpleGlyph {
    fn validate_impl(&self, ctx: &mut crate::codegen_prelude::ValidationCtx) {
        if self.instructions.len() > u16::MAX as usize {
            ctx.report("instructions len overflows");
        }
    }
}

/// Point with an associated on-curve flag.
///
/// Similar to read_fonts::tables::glyf::CurvePoint, but uses kurbo::Point directly
/// thus it does not require (x, y) coordinates to be rounded to integers.
#[derive(Clone, Copy, Debug, PartialEq)]
struct ContourPoint {
    point: kurbo::Point,
    on_curve: bool,
}

impl ContourPoint {
    fn new(point: kurbo::Point, on_curve: bool) -> Self {
        Self { point, on_curve }
    }

    fn on_curve(point: kurbo::Point) -> Self {
        Self::new(point, true)
    }

    fn off_curve(point: kurbo::Point) -> Self {
        Self::new(point, false)
    }
}

impl From<ContourPoint> for CurvePoint {
    fn from(pt: ContourPoint) -> Self {
        let (x, y) = pt.point.ot_round();
        CurvePoint::new(x, y, pt.on_curve)
    }
}
/// A helper struct for building interpolatable contours
///
/// Holds a vec of contour, one contour per glyph.
#[derive(Clone, Debug, PartialEq)]
struct InterpolatableContourBuilder(Vec<Vec<ContourPoint>>);

impl InterpolatableContourBuilder {
    /// Create new set of interpolatable contours beginning at the provided points
    fn new(move_pts: &[kurbo::Point]) -> Self {
        assert!(!move_pts.is_empty());
        Self(
            move_pts
                .iter()
                .map(|pt| vec![ContourPoint::on_curve(*pt)])
                .collect(),
        )
    }

    /// Number of interpolatable contours (one per glyph)
    fn len(&self) -> usize {
        self.0.len()
    }

    /// Add a line segment to all contours
    fn line_to(&mut self, pts: &[kurbo::Point]) {
        assert_eq!(pts.len(), self.len());
        for (i, pt) in pts.iter().enumerate() {
            self.0[i].push(ContourPoint::on_curve(*pt));
        }
    }

    /// Add a quadratic curve segment to all contours
    fn quad_to(&mut self, pts: &[(kurbo::Point, kurbo::Point)]) {
        for (i, (p0, p1)) in pts.iter().enumerate() {
            self.0[i].push(ContourPoint::off_curve(*p0));
            self.0[i].push(ContourPoint::on_curve(*p1));
        }
    }

    /// The total number of points in each interpolatable contour
    fn num_points(&self) -> usize {
        let n = self.0[0].len();
        assert!(self.0.iter().all(|c| c.len() == n));
        n
    }

    /// The first point in each contour
    fn first(&self) -> impl Iterator<Item = &ContourPoint> {
        self.0.iter().map(|v| v.first().unwrap())
    }

    /// The last point in each contour
    fn last(&self) -> impl Iterator<Item = &ContourPoint> {
        self.0.iter().map(|v| v.last().unwrap())
    }

    /// Remove the last point from each contour
    fn remove_last(&mut self) {
        self.0.iter_mut().for_each(|c| {
            c.pop().unwrap();
        });
    }

    fn is_implicit_on_curve(&self, idx: usize) -> bool {
        self.0
            .iter()
            .all(|points| is_implicit_on_curve(points, idx))
    }

    /// Build the contours, dropping any on-curve points that can be implied in all contours
    fn build(self) -> Vec<Contour> {
        let num_contours = self.len();
        let num_points = self.num_points();
        let mut contours = vec![Contour::default(); num_contours];
        contours.iter_mut().for_each(|c| c.0.reserve(num_points));
        for point_idx in (0..num_points).filter(|point_idx| !self.is_implicit_on_curve(*point_idx))
        {
            for (contour_idx, contour) in contours.iter_mut().enumerate() {
                contour
                    .0
                    .push(CurvePoint::from(self.0[contour_idx][point_idx]));
            }
        }
        contours
    }
}

/// True if p1 is the midpoint of p0 and p2.
///
/// We check both before and after rounding float coordinates to integer to avoid
/// false negatives due to rounding.
#[inline]
fn is_mid_point(p0: kurbo::Point, p1: kurbo::Point, p2: kurbo::Point) -> bool {
    let mid = p0.midpoint(p2);
    (util::isclose(mid.x, p1.x) && util::isclose(mid.y, p1.y))
        || p0.to_vec2().ot_round() + p2.to_vec2().ot_round() == p1.to_vec2().ot_round() * 2.0
}

fn is_implicit_on_curve(points: &[ContourPoint], idx: usize) -> bool {
    let p1 = &points[idx]; // user error if this is out of bounds
    if !p1.on_curve {
        return false;
    }
    let p0 = points.wrapping_prev(idx);
    let p2 = points.wrapping_next(idx);
    if p0.on_curve || p0.on_curve != p2.on_curve {
        return false;
    }
    // drop p1 if halfway between p0 and p2
    is_mid_point(p0.point, p1.point, p2.point)
}

// impl for SimpleGlyph::interpolatable_glyphs_from_paths
fn simple_glyphs_from_kurbo(paths: &[BezPath]) -> Result<Vec<SimpleGlyph>, MalformedPath> {
    // check that all paths have the same number of elements so we can zip them together
    let num_elements: Vec<usize> = paths.iter().map(|path| path.elements().len()).collect();
    if num_elements.iter().any(|n| *n != num_elements[0]) {
        return Err(MalformedPath::UnequalNumberOfElements(num_elements));
    }
    let path_iters = MultiZip::new(paths.iter().map(|path| path.iter()).collect());
    let mut contours: Vec<InterpolatableContourBuilder> = Vec::new();
    let mut current: Option<InterpolatableContourBuilder> = None;
    let num_glyphs = paths.len();
    let mut pts = Vec::with_capacity(num_glyphs);
    let mut quad_pts = Vec::with_capacity(num_glyphs);
    for (i, elements) in path_iters.enumerate() {
        // All i-th path elements are expected to have the same types.
        // elements is never empty (if it were, MultiZip would have stopped), hence the unwrap
        let first_el = elements.first().unwrap();
        match first_el {
            kurbo::PathEl::MoveTo(_) => {
                // we have a new contour, flush the current one
                if let Some(prev) = current.take() {
                    contours.push(prev);
                }
                pts.clear();
                for el in &elements {
                    match el {
                        &kurbo::PathEl::MoveTo(pt) => {
                            pts.push(pt);
                        }
                        _ => return Err(MalformedPath::inconsistent_path_els(i, &elements)),
                    }
                }
                current = Some(InterpolatableContourBuilder::new(&pts));
            }
            kurbo::PathEl::LineTo(_) => {
                pts.clear();
                for el in &elements {
                    match el {
                        &kurbo::PathEl::LineTo(pt) => {
                            pts.push(pt);
                        }
                        _ => return Err(MalformedPath::inconsistent_path_els(i, &elements)),
                    }
                }
                current
                    .as_mut()
                    .ok_or(MalformedPath::MissingMove)?
                    .line_to(&pts)
            }
            kurbo::PathEl::QuadTo(_, _) => {
                quad_pts.clear();
                for el in &elements {
                    match el {
                        &kurbo::PathEl::QuadTo(p0, p1) => {
                            quad_pts.push((p0, p1));
                        }
                        _ => return Err(MalformedPath::inconsistent_path_els(i, &elements)),
                    }
                }
                current
                    .as_mut()
                    .ok_or(MalformedPath::MissingMove)?
                    .quad_to(&quad_pts)
            }
            kurbo::PathEl::CurveTo(_, _, _) => return Err(MalformedPath::HasCubic),
            kurbo::PathEl::ClosePath => {
                let contour = current.as_mut().ok_or(MalformedPath::MissingMove)?;
                // remove last point in closed path if has same coords as the move point
                // matches FontTools handling @ https://github.com/fonttools/fonttools/blob/3b9a73ff8379ab49d3ce35aaaaf04b3a7d9d1655/Lib/fontTools/pens/pointPen.py#L321-L323
                // FontTools has an else case to support UFO glif's choice to not include 'move' for closed paths that does not apply here.
                if contour.num_points() > 1 && contour.last().eq(contour.first()) {
                    contour.remove_last();
                }
            }
        }
    }
    contours.extend(current);

    let mut glyph_contours = vec![Vec::new(); num_glyphs];
    for builder in contours {
        assert_eq!(builder.len(), num_glyphs);
        for (i, contour) in builder.build().into_iter().enumerate() {
            glyph_contours[i].push(contour);
        }
    }

    let mut glyphs = Vec::new();
    for (contours, path) in glyph_contours.into_iter().zip(paths.iter()) {
        // https://github.com/googlefonts/fontmake-rs/issues/285 we want control point box, not tight bbox
        // so don't call path.bounding_box
        glyphs.push(SimpleGlyph {
            bbox: path.control_box().into(),
            contours,
            instructions: Default::default(),
            overlaps: false,
        })
    }

    Ok(glyphs)
}

#[cfg(test)]
mod tests {
    use font_types::GlyphId;
    use kurbo::Affine;
    use read_fonts::{tables::glyf as read_glyf, FontRef, TableProvider};

    use super::*;

    // For `indexToLocFormat == 0` (short version), offset divided by 2 is stored, so add a padding
    // byte if the length is not even to ensure our computed bytes match those of our test glyphs.
    fn pad_for_loca_format(loca: &read_fonts::tables::loca::Loca, mut bytes: Vec<u8>) -> Vec<u8> {
        if matches!(loca, read_fonts::tables::loca::Loca::Short(_)) && bytes.len() & 1 != 0 {
            bytes.push(0);
        }
        bytes
    }

    #[test]
    fn bad_path_input() {
        let mut path = BezPath::new();
        path.move_to((0., 0.));
        path.curve_to((10., 10.), (20., 20.), (30., 30.));
        path.line_to((50., 50.));
        path.line_to((10., 10.));
        let err = SimpleGlyph::from_bezpath(&path).unwrap_err();
        assert!(matches!(err, MalformedPath::HasCubic));
    }

    #[test]
    fn read_write_simple() {
        let font = FontRef::new(font_test_data::SIMPLE_GLYF).unwrap();
        let loca = font.loca(None).unwrap();
        let glyf = font.glyf().unwrap();
        let read_glyf::Glyph::Simple(orig) =
            loca.get_glyf(GlyphId::new(0), &glyf).unwrap().unwrap()
        else {
            panic!("not a simple glyph")
        };
        let orig_bytes = orig.offset_data();

        let ours = SimpleGlyph::from_table_ref(&orig);
        let bytes = pad_for_loca_format(&loca, crate::dump_table(&ours).unwrap());
        let ours = read_glyf::SimpleGlyph::read(bytes.as_slice().into()).unwrap();

        let our_points = ours.points().collect::<Vec<_>>();
        let their_points = orig.points().collect::<Vec<_>>();
        assert_eq!(our_points, their_points);
        assert_eq!(orig_bytes.as_ref(), bytes);
        assert_eq!(orig.glyph_data(), ours.glyph_data());
        assert_eq!(orig_bytes.len(), bytes.len());
    }

    #[test]
    fn round_trip_simple() {
        let font = FontRef::new(font_test_data::SIMPLE_GLYF).unwrap();
        let loca = font.loca(None).unwrap();
        let glyf = font.glyf().unwrap();
        let read_glyf::Glyph::Simple(orig) =
            loca.get_glyf(GlyphId::new(2), &glyf).unwrap().unwrap()
        else {
            panic!("not a simple glyph")
        };
        let orig_bytes = orig.offset_data();

        let bezpath = BezPath::from_svg("M278,710 L278,470 L998,470 L998,710 Z").unwrap();

        let ours = SimpleGlyph::from_bezpath(&bezpath).unwrap();
        let bytes = pad_for_loca_format(&loca, crate::dump_table(&ours).unwrap());
        let ours = read_glyf::SimpleGlyph::read(bytes.as_slice().into()).unwrap();

        let our_points = ours.points().collect::<Vec<_>>();
        let their_points = orig.points().collect::<Vec<_>>();
        assert_eq!(our_points, their_points);
        assert_eq!(orig_bytes.as_ref(), bytes);
        assert_eq!(orig.glyph_data(), ours.glyph_data());
        assert_eq!(orig_bytes.len(), bytes.len());
    }

    #[test]
    fn round_trip_multi_contour() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let loca = font.loca(None).unwrap();
        let glyf = font.glyf().unwrap();
        let read_glyf::Glyph::Simple(orig) =
            loca.get_glyf(GlyphId::new(1), &glyf).unwrap().unwrap()
        else {
            panic!("not a simple glyph")
        };
        let orig_bytes = orig.offset_data();

        let bezpath = BezPath::from_svg("M708,1327 L226,0 L29,0 L584,1456 L711,1456 Z M1112,0 L629,1327 L626,1456 L753,1456 L1310,0 Z M1087,539 L1087,381 L269,381 L269,539 Z").unwrap();

        let ours = SimpleGlyph::from_bezpath(&bezpath).unwrap();
        let bytes = pad_for_loca_format(&loca, crate::dump_table(&ours).unwrap());
        let ours = read_glyf::SimpleGlyph::read(bytes.as_slice().into()).unwrap();

        let our_points = ours.points().collect::<Vec<_>>();
        let their_points = orig.points().collect::<Vec<_>>();
        dbg!(
            SimpleGlyphFlags::from_bits(1),
            SimpleGlyphFlags::from_bits(9)
        );
        assert_eq!(our_points, their_points);
        assert_eq!(orig.glyph_data(), ours.glyph_data());
        assert_eq!(orig_bytes.len(), bytes.len());
        assert_eq!(orig_bytes.as_ref(), bytes);
    }

    #[test]
    fn simple_glyph_open_path() {
        let mut path = BezPath::new();
        path.move_to((20., -100.));
        path.quad_to((1337., 1338.), (-50., -69.0));
        path.quad_to((13., 255.), (-255., 256.));
        // even if the last point is on top of the first, the path was not deliberately closed
        // hence there is going to be an extra point (6, not 5 in total)
        path.line_to((20., -100.));

        let glyph = SimpleGlyph::from_bezpath(&path).unwrap();
        let bytes = crate::dump_table(&glyph).unwrap();
        let read = read_fonts::tables::glyf::SimpleGlyph::read(bytes.as_slice().into()).unwrap();
        assert_eq!(read.number_of_contours(), 1);
        assert_eq!(read.num_points(), 6);
        assert_eq!(read.end_pts_of_contours(), &[5]);
        let points = read.points().collect::<Vec<_>>();
        assert_eq!(points[0].x, 20);
        assert_eq!(points[0].y, -100);
        assert!(points[0].on_curve);
        assert_eq!(points[1].x, 1337);
        assert_eq!(points[1].y, 1338);
        assert!(!points[1].on_curve);
        assert_eq!(points[4].x, -255);
        assert_eq!(points[4].y, 256);
        assert!(points[4].on_curve);
        assert_eq!(points[5].x, 20);
        assert_eq!(points[5].y, -100);
        assert!(points[5].on_curve);
    }

    #[test]
    fn simple_glyph_closed_path_implicit_vs_explicit_closing_line() {
        let mut path1 = BezPath::new();
        path1.move_to((20., -100.));
        path1.quad_to((1337., 1338.), (-50., -69.0));
        path1.quad_to((13., 255.), (-255., 256.));
        path1.close_path();

        let mut path2 = BezPath::new();
        path2.move_to((20., -100.));
        path2.quad_to((1337., 1338.), (-50., -69.0));
        path2.quad_to((13., 255.), (-255., 256.));
        // this line_to (absent from path1) makes no difference since in both cases the
        // path is closed with a close_path (5 points in total, not 6)
        path2.line_to((20., -100.));
        path2.close_path();

        for path in &[path1, path2] {
            let glyph = SimpleGlyph::from_bezpath(path).unwrap();
            let bytes = crate::dump_table(&glyph).unwrap();
            let read =
                read_fonts::tables::glyf::SimpleGlyph::read(bytes.as_slice().into()).unwrap();
            assert_eq!(read.number_of_contours(), 1);
            assert_eq!(read.num_points(), 5);
            assert_eq!(read.end_pts_of_contours(), &[4]);
            let points = read.points().collect::<Vec<_>>();
            assert_eq!(points[0].x, 20);
            assert_eq!(points[0].y, -100);
            assert!(points[0].on_curve);
            assert_eq!(points[1].x, 1337);
            assert_eq!(points[1].y, 1338);
            assert!(!points[1].on_curve);
            assert_eq!(points[4].x, -255);
            assert_eq!(points[4].y, 256);
            assert!(points[4].on_curve);
        }
    }

    #[test]
    fn keep_single_point_contours() {
        // single points may be meaningless, but are also harmless
        let mut path = BezPath::new();
        path.move_to((0.0, 0.0));
        // path.close_path();  // doesn't really matter if this is closed
        path.move_to((1.0, 2.0));
        path.close_path();

        let glyph = SimpleGlyph::from_bezpath(&path).unwrap();
        let bytes = crate::dump_table(&glyph).unwrap();
        let read = read_fonts::tables::glyf::SimpleGlyph::read(bytes.as_slice().into()).unwrap();
        assert_eq!(read.number_of_contours(), 2);
        assert_eq!(read.num_points(), 2);
        assert_eq!(read.end_pts_of_contours(), &[0, 1]);
        let points = read.points().collect::<Vec<_>>();
        assert_eq!(points[0].x, 0);
        assert_eq!(points[0].y, 0);
        assert!(points[0].on_curve);
        assert_eq!(points[1].x, 1);
        assert_eq!(points[1].y, 2);
        assert!(points[0].on_curve);
    }

    #[test]
    fn compile_repeatable_flags() {
        let mut path = BezPath::new();
        path.move_to((20., -100.));
        path.line_to((25., -90.));
        path.line_to((50., -69.));
        path.line_to((80., -20.));

        let glyph = SimpleGlyph::from_bezpath(&path).unwrap();
        let flags = glyph
            .compute_point_deltas()
            .map(|x| x.0)
            .collect::<Vec<_>>();
        let r_flags = RepeatableFlag::iter_from_flags(flags.iter().copied()).collect::<Vec<_>>();

        assert_eq!(r_flags.len(), 2, "{r_flags:?}");
        let bytes = crate::dump_table(&glyph).unwrap();
        let read = read_fonts::tables::glyf::SimpleGlyph::read(bytes.as_slice().into()).unwrap();
        assert_eq!(read.number_of_contours(), 1);
        assert_eq!(read.num_points(), 4);
        assert_eq!(read.end_pts_of_contours(), &[3]);
        let points = read.points().collect::<Vec<_>>();
        assert_eq!(points[0].x, 20);
        assert_eq!(points[0].y, -100);
        assert_eq!(points[1].x, 25);
        assert_eq!(points[1].y, -90);
        assert_eq!(points[2].x, 50);
        assert_eq!(points[2].y, -69);
        assert_eq!(points[3].x, 80);
        assert_eq!(points[3].y, -20);
    }

    #[test]
    fn simple_glyphs_from_kurbo_unequal_number_of_elements() {
        let mut path1 = BezPath::new();
        path1.move_to((0., 0.));
        path1.line_to((1., 1.));
        path1.line_to((2., 2.));
        path1.line_to((0., 0.));
        path1.close_path();
        assert_eq!(path1.elements().len(), 5);

        let mut path2 = BezPath::new();
        path2.move_to((3., 3.));
        path2.line_to((4., 4.));
        path2.line_to((5., 5.));
        path2.line_to((6., 6.));
        path2.line_to((3., 3.));
        path2.close_path();
        assert_eq!(path2.elements().len(), 6);

        let err = simple_glyphs_from_kurbo(&[path1, path2]).unwrap_err();
        assert!(matches!(err, MalformedPath::UnequalNumberOfElements(_)));
        assert_eq!(format!("{:?}", err), "UnequalNumberOfElements([5, 6])");
    }

    #[test]
    fn simple_glyphs_from_kurbo_inconsistent_path_elements() {
        let mut path1 = BezPath::new();
        path1.move_to((0., 0.));
        path1.line_to((1., 1.));
        path1.quad_to((2., 2.), (0., 0.));
        path1.close_path();
        let mut path2 = BezPath::new();
        path2.move_to((3., 3.));
        path2.quad_to((4., 4.), (5., 5.)); // elements at index 1 are inconsistent
        path2.line_to((3., 3.));
        path2.close_path();

        let err = simple_glyphs_from_kurbo(&[path1, path2]).unwrap_err();
        assert!(matches!(err, MalformedPath::InconsistentPathElements(1, _)));
        assert_eq!(
            format!("{:?}", err),
            "InconsistentPathElements(1, [\"L\", \"Q\"])"
        );
    }

    /// Create a number of interpolatable BezPaths with the given element types.
    /// The paths will be identical except for the point coordinates of the elements,
    /// which will be (0.0, 0.0), (1.0, 1.0), (2.0, 2.0), etc. for each subsequent
    /// element. If `last_pt_equal_move` is true, the last point of each sub-path
    /// will be equal to the first (M) point of that sub-path.
    /// E.g.:
    /// ```
    /// let paths = make_interpolatable_paths(2, "MLLZ", false);
    /// println!("{:?}", paths[0].to_svg());
    /// // "M0,0 L1,1 L2,2 Z"
    /// println!("{:?}", paths[1].to_svg());
    /// // "M3,3 L4,4 L5,5 Z"
    /// let paths = make_interpolatable_paths(3, "MLLLZMQQZ", true);
    /// println!("{:?}", paths[0].to_svg());
    /// // "M0,0 L1,1 L2,2 L0,0 Z M3,3 Q4,4 5,5 Q6,6 3,3 Z"
    /// println!("{:?}", paths[1].to_svg());
    /// // "M7,7 L8,8 L9,9 L7,7 Z M10,10 Q11,11 12,12 Q13,13 10,10 Z"
    /// println!("{:?}", paths[2].to_svg());
    /// // "M14,14 L15,15 L16,16 L14,14 Z M17,17 Q18,18 19,19 Q20,20 17,17 Z"
    /// ```
    fn make_interpolatable_paths(
        num_paths: usize,
        el_types: &str,
        last_pt_equal_move: bool,
    ) -> Vec<BezPath> {
        let mut paths = Vec::new();
        // we don't care about the actual coordinate values, just use a counter
        // that yields 0.0, 1.0, 2.0, 3.0, etc.
        let mut start = 0.0;
        let mut points = std::iter::from_fn(move || {
            let value = start;
            start += 1.0;
            Some((value, value))
        });
        let el_types = el_types.chars().collect::<Vec<_>>();
        assert!(!el_types.is_empty());
        for _ in 0..num_paths {
            let mut path = BezPath::new();
            let mut start_pt = None;
            // use peekable iterator so we can look ahead to next el_type
            let mut el_types_iter = el_types.iter().peekable();
            while let Some(&el_type) = el_types_iter.next() {
                let next_el_type = el_types_iter.peek().map(|x| **x).unwrap_or('M');
                match el_type {
                    'M' => {
                        start_pt = points.next();
                        path.move_to(start_pt.unwrap());
                    }
                    'L' => {
                        if matches!(next_el_type, 'Z' | 'M') && last_pt_equal_move {
                            path.line_to(start_pt.unwrap());
                        } else {
                            path.line_to(points.next().unwrap());
                        }
                    }
                    'Q' => {
                        let p1 = points.next().unwrap();
                        let p2 = if matches!(next_el_type, 'Z' | 'M') && last_pt_equal_move {
                            start_pt.unwrap()
                        } else {
                            points.next().unwrap()
                        };
                        path.quad_to(p1, p2);
                    }
                    'Z' => {
                        path.close_path();
                        start_pt = None;
                    }
                    _ => panic!("Unsupported element type {:?}", el_type),
                }
            }
            paths.push(path);
        }
        assert_eq!(paths.len(), num_paths);
        paths
    }

    fn assert_contour_points(glyph: &SimpleGlyph, all_points: Vec<Vec<CurvePoint>>) {
        let expected_num_contours = all_points.len();
        assert_eq!(glyph.contours.len(), expected_num_contours);
        for (contour, expected_points) in glyph.contours.iter().zip(all_points.iter()) {
            let points = contour.iter().copied().collect::<Vec<_>>();
            assert_eq!(points, *expected_points);
        }
    }

    #[test]
    fn simple_glyphs_from_kurbo_3_lines_closed() {
        // two triangles, each with 3 lines, explicitly closed
        let paths = make_interpolatable_paths(2, "MLLLZ", true);
        let glyphs = simple_glyphs_from_kurbo(&paths).unwrap();

        assert_contour_points(
            &glyphs[0],
            vec![vec![
                CurvePoint::on_curve(0, 0),
                CurvePoint::on_curve(1, 1),
                CurvePoint::on_curve(2, 2),
            ]],
        );
        assert_contour_points(
            &glyphs[1],
            vec![vec![
                CurvePoint::on_curve(3, 3),
                CurvePoint::on_curve(4, 4),
                CurvePoint::on_curve(5, 5),
            ]],
        );
    }

    #[test]
    fn simple_glyphs_from_kurbo_3_lines_implicitly_closed() {
        // two triangles, each with 2 lines plus the last implicit closing line
        let paths = make_interpolatable_paths(2, "MLLZ", false);
        let glyphs = simple_glyphs_from_kurbo(&paths).unwrap();

        assert_contour_points(
            &glyphs[0],
            vec![vec![
                CurvePoint::on_curve(0, 0),
                CurvePoint::on_curve(1, 1),
                CurvePoint::on_curve(2, 2),
            ]],
        );
        assert_contour_points(
            &glyphs[1],
            vec![vec![
                CurvePoint::on_curve(3, 3),
                CurvePoint::on_curve(4, 4),
                CurvePoint::on_curve(5, 5),
            ]],
        );
    }

    #[test]
    fn simple_glyphs_from_kurbo_2_quads_closed() {
        // two compatible paths each containing 2 consecutive quadratic bezier curves,
        // where the respective off-curves are placed at equal distance from the on-curve
        // point joining them; the paths are closed and the last quad point is the same
        // as the move point.
        let paths = make_interpolatable_paths(2, "MQQZ", true);
        let glyphs = simple_glyphs_from_kurbo(&paths).unwrap();

        assert_contour_points(
            &glyphs[0],
            vec![vec![
                CurvePoint::on_curve(0, 0),
                CurvePoint::off_curve(1, 1),
                // CurvePoint::on_curve(2, 2),  // implied oncurve point dropped
                CurvePoint::off_curve(3, 3),
            ]],
        );
        assert_contour_points(
            &glyphs[1],
            vec![vec![
                CurvePoint::on_curve(4, 4),
                CurvePoint::off_curve(5, 5),
                // CurvePoint::on_curve(6, 6),  // implied
                CurvePoint::off_curve(7, 7),
            ]],
        );
    }

    #[test]
    fn simple_glyphs_from_kurbo_2_quads_1_line_implicitly_closed() {
        // same path elements as above 'MQQZ' but with the last_pt_equal_move=false
        // thus this actually contains three segments: 2 quads plus the last implied
        // closing line. There is an additional on-curve point at the end of the path.
        let paths = make_interpolatable_paths(2, "MQQZ", false);
        let glyphs = simple_glyphs_from_kurbo(&paths).unwrap();

        assert_contour_points(
            &glyphs[0],
            vec![vec![
                CurvePoint::on_curve(0, 0),
                CurvePoint::off_curve(1, 1),
                // CurvePoint::on_curve(2, 2),
                CurvePoint::off_curve(3, 3),
                CurvePoint::on_curve(4, 4),
            ]],
        );
        assert_contour_points(
            &glyphs[1],
            vec![vec![
                CurvePoint::on_curve(5, 5),
                CurvePoint::off_curve(6, 6),
                // CurvePoint::on_curve(7, 7),
                CurvePoint::off_curve(8, 8),
                CurvePoint::on_curve(9, 9),
            ]],
        );
    }

    #[test]
    fn simple_glyphs_from_kurbo_multiple_contours_mixed_segments() {
        // four paths, each containing two sub-paths, with a mix of line and quad segments
        let paths = make_interpolatable_paths(4, "MLQQZMQLQLZ", true);
        let glyphs = simple_glyphs_from_kurbo(&paths).unwrap();

        assert_contour_points(
            &glyphs[0],
            vec![
                vec![
                    CurvePoint::on_curve(0, 0),
                    CurvePoint::on_curve(1, 1),
                    CurvePoint::off_curve(2, 2),
                    // CurvePoint::on_curve(3, 3),
                    CurvePoint::off_curve(4, 4),
                ],
                vec![
                    CurvePoint::on_curve(5, 5),
                    CurvePoint::off_curve(6, 6),
                    CurvePoint::on_curve(7, 7),
                    CurvePoint::on_curve(8, 8),
                    CurvePoint::off_curve(9, 9),
                    CurvePoint::on_curve(10, 10),
                ],
            ],
        );
    }

    #[test]
    fn simple_glyphs_from_kurbo_all_quad_off_curves() {
        // the following path contains only quadratic curves and all the on-curve points
        // can be implied, thus the resulting glyf contours contain only off-curves.
        let mut path1 = BezPath::new();
        path1.move_to((0.0, 1.0));
        path1.quad_to((1.0, 1.0), (1.0, 0.0));
        path1.quad_to((1.0, -1.0), (0.0, -1.0));
        path1.quad_to((-1.0, -1.0), (-1.0, 0.0));
        path1.quad_to((-1.0, 1.0), (0.0, 1.0));
        path1.close_path();

        let mut path2 = path1.clone();
        path2.apply_affine(Affine::scale(2.0));

        let glyphs = simple_glyphs_from_kurbo(&[path1, path2]).unwrap();

        assert_contour_points(
            &glyphs[0],
            vec![vec![
                CurvePoint::off_curve(1, 1),
                CurvePoint::off_curve(1, -1),
                CurvePoint::off_curve(-1, -1),
                CurvePoint::off_curve(-1, 1),
            ]],
        );
        assert_contour_points(
            &glyphs[1],
            vec![vec![
                CurvePoint::off_curve(2, 2),
                CurvePoint::off_curve(2, -2),
                CurvePoint::off_curve(-2, -2),
                CurvePoint::off_curve(-2, 2),
            ]],
        );
    }

    #[test]
    fn simple_glyphs_from_kurbo_keep_on_curve_unless_impliable_for_all() {
        let mut path1 = BezPath::new();
        path1.move_to((0.0, 0.0));
        path1.quad_to((0.0, 1.0), (1.0, 1.0)); // on-curve equidistant from prev/next off-curves
        path1.quad_to((2.0, 1.0), (2.0, 0.0));
        path1.line_to((0.0, 0.0));
        path1.close_path();

        // when making a SimpleGlyph from this path alone, the on-curve point at (1, 1)
        // can be implied/dropped.
        assert_contour_points(
            &SimpleGlyph::from_bezpath(&path1).unwrap(),
            vec![vec![
                CurvePoint::on_curve(0, 0),
                CurvePoint::off_curve(0, 1),
                // CurvePoint::on_curve(1, 1),  // implied
                CurvePoint::off_curve(2, 1),
                CurvePoint::on_curve(2, 0),
            ]],
        );

        let mut path2 = BezPath::new();
        path2.move_to((0.0, 0.0));
        path2.quad_to((0.0, 2.0), (2.0, 2.0)); // on-curve NOT equidistant from prev/next off-curves
        path2.quad_to((3.0, 2.0), (3.0, 0.0));
        path2.line_to((0.0, 0.0));
        path2.close_path();

        let glyphs = simple_glyphs_from_kurbo(&[path1, path2]).unwrap();

        // However, when making interpolatable SimpleGlyphs from both paths, the on-curve
        // can no longer be implied/dropped (for it is not impliable in the second path).
        assert_contour_points(
            &glyphs[0],
            vec![vec![
                CurvePoint::on_curve(0, 0),
                CurvePoint::off_curve(0, 1),
                CurvePoint::on_curve(1, 1), // NOT implied
                CurvePoint::off_curve(2, 1),
                CurvePoint::on_curve(2, 0),
            ]],
        );
        assert_contour_points(
            &glyphs[1],
            vec![vec![
                CurvePoint::on_curve(0, 0),
                CurvePoint::off_curve(0, 2),
                CurvePoint::on_curve(2, 2), // NOT implied
                CurvePoint::off_curve(3, 2),
                CurvePoint::on_curve(3, 0),
            ]],
        );
    }

    #[test]
    fn simple_glyphs_from_kurbo_2_lines_open() {
        // these contours contain two lines each and are not closed (no 'Z'); they still
        // produce three points and are treated as closed for the sake of TrueType glyf.
        let paths = make_interpolatable_paths(2, "MLL", false);
        let glyphs = simple_glyphs_from_kurbo(&paths).unwrap();

        assert_contour_points(
            &glyphs[0],
            vec![vec![
                CurvePoint::on_curve(0, 0),
                CurvePoint::on_curve(1, 1),
                CurvePoint::on_curve(2, 2),
            ]],
        );
        assert_contour_points(
            &glyphs[1],
            vec![vec![
                CurvePoint::on_curve(3, 3),
                CurvePoint::on_curve(4, 4),
                CurvePoint::on_curve(5, 5),
            ]],
        );
    }

    #[test]
    fn simple_glyphs_from_kurbo_3_lines_open_duplicate_last_pt() {
        // two paths with one open contour, containing three line segments with the last
        // point overlapping the first point; the last point gets duplicated (and not fused).
        // The special treatment for the last point is only applied to Z-ending contours,
        // not to open contours.
        let paths = make_interpolatable_paths(2, "MLLL", true);
        let glyphs = simple_glyphs_from_kurbo(&paths).unwrap();

        assert_contour_points(
            &glyphs[0],
            vec![vec![
                CurvePoint::on_curve(0, 0),
                CurvePoint::on_curve(1, 1),
                CurvePoint::on_curve(2, 2),
                CurvePoint::on_curve(0, 0),
            ]],
        );
        assert_contour_points(
            &glyphs[1],
            vec![vec![
                CurvePoint::on_curve(3, 3),
                CurvePoint::on_curve(4, 4),
                CurvePoint::on_curve(5, 5),
                CurvePoint::on_curve(3, 3),
            ]],
        );
    }

    #[test]
    fn simple_glyphs_from_kurbo_4_lines_closed_duplicate_last_pt() {
        for implicit_closing_line in &[true, false] {
            // both (closed) paths contain 4 line segments each, but the first path
            // looks like a triangle because the last segment has zero length (i.e.
            // last and first points are duplicates).
            let mut path1 = BezPath::new();
            path1.move_to((0.0, 0.0));
            path1.line_to((0.0, 1.0));
            path1.line_to((1.0, 1.0));
            path1.line_to((0.0, 0.0));
            if !*implicit_closing_line {
                path1.line_to((0.0, 0.0));
            }
            path1.close_path();

            let mut path2 = BezPath::new();
            path2.move_to((0.0, 0.0));
            path2.line_to((0.0, 2.0));
            path2.line_to((2.0, 2.0));
            path2.line_to((2.0, 0.0));
            if !*implicit_closing_line {
                path2.line_to((0.0, 0.0));
            }
            path2.close_path();

            let glyphs = simple_glyphs_from_kurbo(&[path1, path2]).unwrap();

            assert_contour_points(
                &glyphs[0],
                vec![vec![
                    CurvePoint::on_curve(0, 0),
                    CurvePoint::on_curve(0, 1),
                    CurvePoint::on_curve(1, 1),
                    CurvePoint::on_curve(0, 0), // duplicate last point retained
                ]],
            );
            assert_contour_points(
                &glyphs[1],
                vec![vec![
                    CurvePoint::on_curve(0, 0),
                    CurvePoint::on_curve(0, 2),
                    CurvePoint::on_curve(2, 2),
                    CurvePoint::on_curve(2, 0),
                ]],
            );
        }
    }

    #[test]
    fn simple_glyphs_from_kurbo_2_quads_1_line_closed_duplicate_last_pt() {
        for implicit_closing_line in &[true, false] {
            // the closed paths contain 2 quads and 1 line segments, but in the first path
            // the last segment has zero length (i.e. last and first points are duplicates).
            let mut path1 = BezPath::new();
            path1.move_to((0.0, 0.0));
            path1.quad_to((0.0, 1.0), (1.0, 1.0));
            path1.quad_to((1.0, 0.0), (0.0, 0.0));
            if !*implicit_closing_line {
                path1.line_to((0.0, 0.0));
            }
            path1.close_path();

            let mut path2 = BezPath::new();
            path2.move_to((0.0, 0.0));
            path2.quad_to((0.0, 2.0), (2.0, 2.0));
            path2.quad_to((2.0, 1.0), (1.0, 0.0));
            if !*implicit_closing_line {
                path2.line_to((0.0, 0.0));
            }
            path2.close_path();

            let glyphs = simple_glyphs_from_kurbo(&[path1, path2]).unwrap();

            assert_contour_points(
                &glyphs[0],
                vec![vec![
                    CurvePoint::on_curve(0, 0),
                    CurvePoint::off_curve(0, 1),
                    CurvePoint::on_curve(1, 1),
                    CurvePoint::off_curve(1, 0),
                    CurvePoint::on_curve(0, 0), // duplicate last point retained
                ]],
            );
            assert_contour_points(
                &glyphs[1],
                vec![vec![
                    CurvePoint::on_curve(0, 0),
                    CurvePoint::off_curve(0, 2),
                    CurvePoint::on_curve(2, 2),
                    CurvePoint::off_curve(2, 1),
                    CurvePoint::on_curve(1, 0),
                ]],
            );
        }
    }

    #[test]
    fn simple_glyph_from_kurbo_equidistant_but_not_collinear_points() {
        let mut path = BezPath::new();
        path.move_to((0.0, 0.0));
        path.quad_to((2.0, 2.0), (4.0, 3.0));
        path.quad_to((6.0, 2.0), (8.0, 0.0));
        path.close_path();

        let glyph = SimpleGlyph::from_bezpath(&path).unwrap();

        assert_contour_points(
            &glyph,
            vec![vec![
                CurvePoint::on_curve(0, 0),
                CurvePoint::off_curve(2, 2),
                // the following on-curve point is equidistant from the previous/next
                // off-curve points but it is not on the same line hence it must NOT
                // be dropped
                CurvePoint::on_curve(4, 3),
                CurvePoint::off_curve(6, 2),
                CurvePoint::on_curve(8, 0),
            ]],
        );
    }

    #[test]
    fn repeatable_flags_basic() {
        let flags = [
            SimpleGlyphFlags::ON_CURVE_POINT,
            SimpleGlyphFlags::X_SHORT_VECTOR,
            SimpleGlyphFlags::X_SHORT_VECTOR,
        ];
        let repeatable = RepeatableFlag::iter_from_flags(flags).collect::<Vec<_>>();
        let expected = flags
            .into_iter()
            .map(|flag| RepeatableFlag { flag, repeat: 0 })
            .collect::<Vec<_>>();

        // even though we have a repeating flag at the end, we should still produce
        // three flags, since we don't bother with repeat counts < 2.
        assert_eq!(repeatable, expected);
    }

    #[test]
    fn repeatable_flags_repeats() {
        let some_dupes = std::iter::repeat_n(SimpleGlyphFlags::ON_CURVE_POINT, 4);
        let many_dupes = std::iter::repeat_n(SimpleGlyphFlags::Y_SHORT_VECTOR, 257);
        let repeatable =
            RepeatableFlag::iter_from_flags(some_dupes.chain(many_dupes)).collect::<Vec<_>>();
        assert_eq!(repeatable.len(), 3);
        assert_eq!(
            repeatable[0],
            RepeatableFlag {
                flag: SimpleGlyphFlags::ON_CURVE_POINT | SimpleGlyphFlags::REPEAT_FLAG,
                repeat: 3
            }
        );
        assert_eq!(
            repeatable[1],
            RepeatableFlag {
                flag: SimpleGlyphFlags::Y_SHORT_VECTOR | SimpleGlyphFlags::REPEAT_FLAG,
                repeat: u8::MAX,
            }
        );

        assert_eq!(
            repeatable[2],
            RepeatableFlag {
                flag: SimpleGlyphFlags::Y_SHORT_VECTOR,
                repeat: 0,
            }
        )
    }

    #[test]
    fn mid_points() {
        // exactly in the middle
        assert!(is_mid_point(
            kurbo::Point::new(0.0, 0.0),
            kurbo::Point::new(1.0, 1.0),
            kurbo::Point::new(2.0, 2.0)
        ));
        // in the middle but rounding would make it not; take it anyway
        assert!(is_mid_point(
            kurbo::Point::new(0.5, 0.5),
            kurbo::Point::new(3.0, 3.0),
            kurbo::Point::new(5.5, 5.5)
        ));
        // very close to the middle
        assert!(is_mid_point(
            kurbo::Point::new(0.0, 0.0),
            kurbo::Point::new(1.00001, 0.99999),
            kurbo::Point::new(2.0, 2.0)
        ));
        // not quite in the middle but rounding would make it so; why throw it away?
        assert!(is_mid_point(
            kurbo::Point::new(0.0, 0.0),
            kurbo::Point::new(-1.499999, 0.500001),
            kurbo::Point::new(-2.0, 2.0)
        ));
        // not in the middle, neither before nor after rounding
        assert!(!is_mid_point(
            kurbo::Point::new(0.0, 0.0),
            kurbo::Point::new(1.0, 1.5),
            kurbo::Point::new(2.0, 2.0)
        ));
    }
}
