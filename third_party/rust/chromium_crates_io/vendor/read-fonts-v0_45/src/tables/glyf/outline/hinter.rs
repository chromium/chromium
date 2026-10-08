//! The hook for a TrueType interpreter.

use super::super::{bytecode::HintError, PointCoord, PointFlags, PHANTOM_POINT_COUNT};
use crate::types::{F2Dot14, GlyphId, Point};

/// Receives each instructed glyph from
/// [`OutlinePlan::load`](super::OutlinePlan::load).
pub trait Hinter<C: PointCoord> {
    /// Whether backward compatibility mode is active, which affects phantom
    /// point rounding even for a glyph with no instructions.
    fn backward_compatibility(&self) -> bool {
        true
    }

    /// Hints one glyph.
    ///
    /// A failure stops the load and is reported as
    /// [`OutlineError::Hinting`](super::OutlineError::Hinting). The caller
    /// decides whether to treat it as fatal.
    fn hint(&mut self, zone: &mut GlyphZone<'_, C>) -> Result<(), HintError>;
}

/// The points a [`Hinter`] works on, zone 1 in the specification's terms.
///
/// Unlike [`OutlineRef`](super::OutlineRef) this is mutable, stores its
/// phantom points alongside the rest, and is not shifted by the left side
/// bearing.
pub struct GlyphZone<'a, C: PointCoord> {
    /// The glyph being hinted.
    pub glyph: GlyphId,
    /// The glyph's TrueType instructions.
    pub bytecode: &'a [u8],
    /// Scaled points, including phantom points, which hinting moves.
    pub points: &'a mut [Point<C>],
    /// The same points in font units.
    pub unscaled: &'a mut [Point<i32>],
    /// The flag for each point, which hinting also marks up.
    pub flags: &'a mut [PointFlags],
    /// Contour end points, relative to the start of `points`.
    pub contours: &'a [u16],
    /// The four phantom points, which hinting may move.
    pub phantom: &'a mut [Point<C>; PHANTOM_POINT_COUNT],
    /// True when hinting a composite rather than a simple glyph.
    pub is_composite: bool,
    /// Normalized variation coordinates, empty for a static instance.
    pub coords: &'a [F2Dot14],
}

#[cfg(test)]
mod tests {
    /// Any scaled size will do; the hook does not care which.
    const PPEM: f32 = 16.0;

    use super::super::{Outline, OutlineError, OutlinePlan, OutlineTables, Scale26Dot6};
    use super::*;
    use crate::tables::glyf::bytecode::{HintErrorKind, Opcode, Program};
    use crate::{
        types::{F26Dot6, GlyphId},
        FontRef, TableProvider,
    };

    /// Records what the hinting hook was handed.
    #[derive(Default)]
    struct RecordingHinter {
        calls: usize,
        bytecode_len: usize,
        points: usize,
        composite: bool,
    }

    /// An instructed glyph reaches the hinter, with its bytecode and a point
    /// buffer that includes the phantom points.
    #[test]
    fn instructed_glyph_reaches_the_hinter() {
        let font = FontRef::new(font_test_data::COUSINE_HINT_SUBSET).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        let n = font.maxp().unwrap().num_glyphs();
        let mut outline = Outline::<Scale26Dot6>::new();
        let mut hinted = 0;
        for gid in 0..n {
            let plan = OutlinePlan::new(&context, GlyphId::from(gid)).unwrap();
            if !plan.has_hinting || plan.glyph_data.is_none() {
                continue;
            }
            let mut hinter = RecordingHinter::default();
            outline
                .load_hinted(&context, GlyphId::from(gid), Some(PPEM), &mut hinter)
                .unwrap();
            assert_eq!(hinter.calls, 1, "gid {gid}");
            assert!(hinter.bytecode_len > 0);
            assert_eq!(hinter.points as u32, plan.num_points);
            hinted += 1;
        }
        assert!(hinted > 0, "expected some instructed glyphs");
    }

    /// Without a hinter, an instructed glyph is simply loaded unhinted.
    #[test]
    fn hinting_is_optional() {
        let font = FontRef::new(font_test_data::COUSINE_HINT_SUBSET).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        let n = font.maxp().unwrap().num_glyphs();
        let gid = (0..n)
            .map(GlyphId::from)
            .find(|gid| {
                OutlinePlan::new(&context, *gid)
                    .map(|i| i.has_hinting && i.glyph_data.is_some())
                    .unwrap_or(false)
            })
            .unwrap();
        let mut outline = Outline::<Scale26Dot6>::new();
        assert!(outline.load(&context, gid, Some(PPEM)).is_ok());
    }

    impl Hinter<F26Dot6> for RecordingHinter {
        fn hint(&mut self, zone: &mut GlyphZone<'_, F26Dot6>) -> Result<(), HintError> {
            self.calls += 1;
            self.bytecode_len = zone.bytecode.len();
            self.points = zone.points.len();
            self.composite = zone.is_composite;
            // The font unit copy must line up with the scaled points.
            assert_eq!(zone.unscaled.len(), zone.points.len());
            assert!(!zone.contours.is_empty());
            Ok(())
        }
    }

    /// A hinter that always fails, at a place it can name.
    struct FailingHinter;

    impl Hinter<F26Dot6> for FailingHinter {
        fn hint(&mut self, zone: &mut GlyphZone<'_, F26Dot6>) -> Result<(), HintError> {
            Err(HintError {
                program: Program::Glyph,
                glyph_id: Some(zone.glyph),
                pc: 3,
                opcode: Some(Opcode::SVTCA0),
                kind: HintErrorKind::ValueStackOverflow,
            })
        }
    }

    /// What the hinter reported reaches the caller, rather than being
    /// flattened into something the loader made up.
    #[test]
    fn a_hinters_error_survives() {
        let font = FontRef::new(font_test_data::COUSINE_HINT_SUBSET).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        let n = font.maxp().unwrap().num_glyphs();
        let gid = (0..n)
            .map(GlyphId::from)
            .find(|gid| {
                OutlinePlan::new(&context, *gid)
                    .map(|i| i.has_hinting && i.glyph_data.is_some())
                    .unwrap_or(false)
            })
            .unwrap();
        let mut outline = Outline::<Scale26Dot6>::new();
        let err = outline
            .load_hinted(&context, gid, Some(PPEM), &mut FailingHinter)
            .unwrap_err();
        let OutlineError::Hinting(err) = err else {
            panic!("expected a hinting error, got {err:?}");
        };
        assert_eq!(err.kind, HintErrorKind::ValueStackOverflow);
        assert_eq!(err.glyph_id, Some(gid));
        assert_eq!(err.pc, 3);
    }
}
