//! The survey that sizes an outline before it is built.

use super::{
    load, BufferLengths, Buffers, Hinter, OutlineContext, OutlineError, OutlineRef, Scale,
};
use crate::{
    limits::{MAX_COMPOSITE_EDGES, MAX_OUTLINE_POINTS, MAX_RECURSION_DEPTH},
    tables::glyf::{CompositeGlyphFlags, Glyph, PHANTOM_POINT_COUNT},
    types::GlyphId,
};

/// What an outline is made of, and how much scratch space building it needs.
///
/// Produced by walking the composite tree without reading point data, which
/// is cheap enough to do per draw. [`Self::buffer_lengths`] sizes the buffers
/// and [`Self::load`] fills them.
#[derive(Clone, Default)]
pub struct OutlinePlan<'a> {
    /// The glyph this describes.
    pub glyph: GlyphId,
    /// The root glyph's `glyf` data, or `None` for an empty glyph such as a
    /// space.
    ///
    /// A missing or unreadable glyph is an error from [`OutlinePlan::new`],
    /// not `None` here.
    pub glyph_data: Option<Glyph<'a>>,
    /// Total points in the outline, summed over every simple glyph in the
    /// tree, plus the phantom points.
    pub num_points: u32,
    /// Total contours, summed over every simple glyph in the tree.
    pub num_contours: u32,
    /// Largest point count, including phantom points, of any one simple glyph
    /// in the tree.
    pub max_simple_points: u32,
    /// Largest point count, including phantom points, contributed by any one
    /// composite glyph in the tree that carries instructions.
    ///
    /// Zero when no composite is instructed. Only a hinted composite keeps
    /// its loaded points in font units, so this is tracked separately from
    /// [`Self::max_simple_points`].
    pub max_hinted_composite_points: u32,
    /// Deepest the per-component delta stack gets while loading nested
    /// composites.
    pub max_component_delta_stack: u32,
    /// True if any glyph in the tree carries TrueType instructions.
    pub has_hinting: bool,
    /// True if any glyph in the tree is flagged as having overlapping contours
    /// or components.
    pub has_overlaps: bool,
    /// True when `gvar` deltas will be applied: the font has a `gvar` table
    /// and the context names a location in it.
    ///
    /// A variable font read at its default instance needs no delta buffers.
    /// This is the one field taken from the context rather than the walk.
    pub applies_variations: bool,
}

impl core::fmt::Debug for OutlinePlan<'_> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        // `Glyph` has no `Debug`, and printing a whole glyph here would not be
        // useful anyway; note whether one is present instead.
        f.debug_struct("OutlinePlan")
            .field("glyph", &self.glyph)
            .field("is_empty", &self.glyph_data.is_none())
            .field("num_points", &self.num_points)
            .field("num_contours", &self.num_contours)
            .field("max_simple_points", &self.max_simple_points)
            .field(
                "max_hinted_composite_points",
                &self.max_hinted_composite_points,
            )
            .field("max_component_delta_stack", &self.max_component_delta_stack)
            .field("has_hinting", &self.has_hinting)
            .field("has_overlaps", &self.has_overlaps)
            .field("applies_variations", &self.applies_variations)
            .finish()
    }
}

impl<'a> OutlinePlan<'a> {
    /// Walks the glyph, and the tree below a composite, collecting what
    /// building its outline requires.
    pub fn new(context: &dyn OutlineContext<'a>, glyph: GlyphId) -> Result<Self, OutlineError> {
        let mut plan = Self {
            glyph,
            applies_variations: context.has_gvar() && !context.coords().is_empty(),
            ..Default::default()
        };
        let glyph_data = context
            .glyph(glyph)
            .ok_or(OutlineError::MissingGlyph)?
            .into_glyph();
        if let Some(data) = glyph_data.as_ref() {
            let mut components = 0;
            plan.walk(context, data, 0, 0, &mut components)?;
        }
        // The phantom points belong to the outline as a whole, not to any one
        // glyph in the tree.
        plan.num_points += PHANTOM_POINT_COUNT as u32;
        plan.glyph_data = glyph_data;
        Ok(plan)
    }

    /// Returns how long each scratch buffer has to be to load at `S`.
    pub fn buffer_lengths<S: Scale>(&self) -> BufferLengths {
        let applies_variations = self.applies_variations;
        BufferLengths {
            points: self.num_points,
            contours: self.num_contours,
            unscaled: if S::NEEDS_UNSCALED {
                self.max_simple_points.max(self.max_hinted_composite_points)
            } else {
                0
            },
            deltas: if applies_variations {
                self.max_simple_points
            } else {
                0
            },
            iup: if applies_variations {
                self.max_simple_points
            } else {
                0
            },
            composite_deltas: if applies_variations {
                self.max_component_delta_stack
            } else {
                0
            },
        }
    }

    /// Builds the outline this plan describes.
    ///
    /// The buffers must be at least as long as [`buffer_lengths`] says. The
    /// location comes from `context`. The result borrows `buffers` and
    /// nothing else.
    ///
    /// [`buffer_lengths`]: OutlinePlan::buffer_lengths
    ///
    /// ```
    /// # fn main() -> Result<(), Box<dyn std::error::Error>> {
    /// use read_fonts::{
    ///     tables::glyf::outline::{Buffers, OutlinePlan, OutlineTables, Scale26Dot6},
    ///     types::{GlyphId, Point},
    ///     FontRef,
    /// };
    ///
    /// let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE)?;
    /// let tables = OutlineTables::new(&font)?;
    /// let plan = OutlinePlan::new(&tables, GlyphId::new(1))?;
    ///
    /// // A static draw, and 26.6 keeps a font unit copy of the points.
    /// let lengths = plan.buffer_lengths::<Scale26Dot6>();
    /// let mut points = vec![Point::default(); lengths.points as usize];
    /// let mut flags = vec![Default::default(); lengths.points as usize];
    /// let mut contours = vec![0; lengths.contours as usize];
    /// let mut unscaled = vec![Point::default(); lengths.unscaled as usize];
    /// let buffers = Buffers {
    ///     points: &mut points,
    ///     flags: &mut flags,
    ///     contours: &mut contours,
    ///     unscaled: &mut unscaled,
    ///     // Only needed when varying.
    ///     deltas: &mut [],
    ///     iup: &mut [],
    ///     composite_deltas: &mut [],
    /// };
    ///
    /// let scale = Scale26Dot6::new(Some(16.0), tables.units_per_em);
    /// let outline = plan.load(&tables, &scale, buffers, None)?;
    /// assert_eq!(outline.points().len(), outline.flags().len());
    /// # Ok(())
    /// # }
    /// ```
    pub fn load<'s, 'buf, S: Scale>(
        &'s self,
        context: &'a dyn OutlineContext<'a>,
        scale: &'s S,
        buffers: Buffers<'buf, S>,
        hinter: Option<&mut dyn Hinter<S::Coord>>,
    ) -> Result<OutlineRef<'buf, S::Coord>, OutlineError> {
        load::load(self, context, scale, buffers, hinter)
    }

    fn walk(
        &mut self,
        context: &dyn OutlineContext<'a>,
        glyph: &Glyph<'a>,
        component_depth: u32,
        recurse_depth: usize,
        components: &mut usize,
    ) -> Result<(), OutlineError> {
        if recurse_depth > MAX_RECURSION_DEPTH {
            return Err(OutlineError::RecursionLimitExceeded);
        }
        match glyph {
            Glyph::Simple(simple) => {
                let point_count = simple.num_points() as u32;
                let with_phantom = point_count + PHANTOM_POINT_COUNT as u32;
                self.max_simple_points = self.max_simple_points.max(with_phantom);
                self.num_points += point_count;
                if self.num_points as usize > MAX_OUTLINE_POINTS {
                    return Err(OutlineError::TooManyPoints);
                }
                self.num_contours += simple.end_pts_of_contours().len() as u32;
                self.has_hinting |= simple.instruction_length() != 0;
                self.has_overlaps |= simple.has_overlapping_contours();
            }
            Glyph::Composite(composite) => {
                let (component_count, instructions) = composite.count_and_instructions();
                // Each level of the tree contributes its own components plus a
                // set of phantom points to the delta stack.
                let level_size = component_count as u32 + PHANTOM_POINT_COUNT as u32;
                let point_base = self.num_points;
                for (component, flags) in composite.component_glyphs_and_flags() {
                    self.has_overlaps |= flags.contains(CompositeGlyphFlags::OVERLAP_COMPOUND);
                    let Some(component_glyph) = context
                        .glyph(component.into())
                        .ok_or(OutlineError::MissingGlyph)?
                        .into_glyph()
                    else {
                        continue;
                    };
                    *components += 1;
                    if *components > MAX_COMPOSITE_EDGES {
                        return Err(OutlineError::TooManyComponents);
                    }
                    self.walk(
                        context,
                        &component_glyph,
                        component_depth + level_size,
                        recurse_depth + 1,
                        components,
                    )?;
                }
                if !instructions.unwrap_or_default().is_empty() {
                    // A hinted composite needs its whole loaded point set
                    // kept in font units.
                    let loaded = self.num_points - point_base + PHANTOM_POINT_COUNT as u32;
                    self.max_hinted_composite_points = self.max_hinted_composite_points.max(loaded);
                    self.has_hinting = true;
                }
                self.max_component_delta_stack = self
                    .max_component_delta_stack
                    .max(component_depth + level_size);
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::super::{OutlineTables, Scale26Dot6, ScaleF32};
    use super::*;
    use crate::{
        limits::{MAX_COMPOSITE_EDGES, MAX_RECURSION_DEPTH},
        tables::{glyf::Glyf, loca::Loca},
        FontData, FontRead,
    };
    use crate::{FontRef, TableProvider};
    use alloc::{vec, vec::Vec};

    /// `glyf_components.ttf` has, in order: a two contour simple glyph, a one
    /// contour simple glyph, four composites referencing the latter, one
    /// composite referencing it twice, and two composites referencing another
    /// composite, which is the only nesting in the file.
    fn components_font() -> FontRef<'static> {
        FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap()
    }

    fn info_for(font: &FontRef<'static>, gid: u32) -> OutlinePlan<'static> {
        let context = OutlineTables::new(font).unwrap();
        OutlinePlan::new(&context, GlyphId::new(gid)).unwrap()
    }

    #[test]
    fn empty_glyph_has_only_phantom_points() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        // Glyph 0 in this font has no outline.
        let plan = OutlinePlan::new(&context, GlyphId::new(0)).unwrap();
        assert!(plan.glyph_data.is_none());
        assert_eq!(plan.num_points, PHANTOM_POINT_COUNT as u32);
        assert_eq!(plan.num_contours, 0);
        assert_eq!(plan.max_simple_points, 0);
        assert_eq!(plan.max_component_delta_stack, 0);
        assert!(!plan.has_hinting);
    }

    #[test]
    fn simple_glyph() {
        let font = components_font();
        // Glyph 1: one contour, four points.
        let plan = info_for(&font, 1);
        assert_eq!(plan.num_points, 4 + PHANTOM_POINT_COUNT as u32);
        assert_eq!(plan.num_contours, 1);
        assert_eq!(plan.max_simple_points, 4 + PHANTOM_POINT_COUNT as u32);
        assert_eq!(plan.max_hinted_composite_points, 0);
        assert_eq!(plan.max_component_delta_stack, 0);
        assert!(!plan.has_hinting);
    }

    #[test]
    fn simple_glyph_with_two_contours() {
        let font = components_font();
        // Glyph 0: two contours, eight points.
        let plan = info_for(&font, 0);
        assert_eq!(plan.num_points, 8 + PHANTOM_POINT_COUNT as u32);
        assert_eq!(plan.num_contours, 2);
        assert_eq!(plan.max_simple_points, 8 + PHANTOM_POINT_COUNT as u32);
    }

    #[test]
    fn composite_with_one_component() {
        let font = components_font();
        // Glyph 2 references glyph 1, so it inherits its points and contours.
        let plan = info_for(&font, 2);
        assert_eq!(plan.num_points, 4 + PHANTOM_POINT_COUNT as u32);
        assert_eq!(plan.num_contours, 1);
        assert_eq!(plan.max_simple_points, 4 + PHANTOM_POINT_COUNT as u32);
        // One component plus the phantom points.
        assert_eq!(
            plan.max_component_delta_stack,
            1 + PHANTOM_POINT_COUNT as u32
        );
        // Nothing in this font is instructed.
        assert_eq!(plan.max_hinted_composite_points, 0);
        assert!(!plan.has_hinting);
    }

    #[test]
    fn composite_with_two_components() {
        let font = components_font();
        // Glyph 5 references glyph 1 twice.
        let plan = info_for(&font, 5);
        assert_eq!(plan.num_points, 8 + PHANTOM_POINT_COUNT as u32);
        assert_eq!(plan.num_contours, 2);
        // The largest single simple glyph is still glyph 1.
        assert_eq!(plan.max_simple_points, 4 + PHANTOM_POINT_COUNT as u32);
        assert_eq!(
            plan.max_component_delta_stack,
            2 + PHANTOM_POINT_COUNT as u32
        );
    }

    /// Nesting accumulates on the delta stack: each level contributes its own
    /// component count plus a set of phantom points.
    #[test]
    fn nested_composite_deepens_the_delta_stack() {
        let font = components_font();
        // Glyph 7 references glyph 4, which references glyph 1.
        let plan = info_for(&font, 7);
        assert_eq!(plan.num_points, 4 + PHANTOM_POINT_COUNT as u32);
        assert_eq!(plan.num_contours, 1);
        // Two levels, each one component plus phantom points.
        assert_eq!(
            plan.max_component_delta_stack,
            2 * (1 + PHANTOM_POINT_COUNT as u32)
        );
    }

    #[test]
    fn buffer_lengths_follow_the_walk() {
        let font = components_font();
        let plan = info_for(&font, 5);

        // Static, at a scale that does not keep font units.
        let plain = plan.buffer_lengths::<ScaleF32>();
        assert_eq!(plain.points, plan.num_points);
        assert_eq!(plain.contours, plan.num_contours);
        assert_eq!(plain.unscaled, 0);
        assert_eq!(plain.deltas, 0);
        assert_eq!(plain.iup, 0);
        assert_eq!(plain.composite_deltas, 0);

        // A scale that keeps them sizes one more buffer, and nothing else.
        let unscaled = plan.buffer_lengths::<Scale26Dot6>();
        assert_eq!(unscaled.unscaled, plan.max_simple_points);
        assert_eq!(unscaled.deltas, 0);

        // Variations size the three delta buffers. The plan settled that
        // against its context, so this is the same walk at a location.
        let mut plan = plan;
        plan.applies_variations = true;
        let varied = plan.buffer_lengths::<ScaleF32>();
        assert_eq!(varied.unscaled, 0);
        assert_eq!(varied.deltas, plan.max_simple_points);
        assert_eq!(varied.iup, plan.max_simple_points);
        assert_eq!(varied.composite_deltas, plan.max_component_delta_stack);
    }

    /// The unscaled buffer has to hold whichever is larger: the biggest simple
    /// glyph, or the loaded points of an instructed composite.
    #[test]
    fn buffer_lengths_unscaled_takes_the_larger() {
        let mut plan = OutlinePlan {
            max_simple_points: 10,
            max_hinted_composite_points: 40,
            ..Default::default()
        };
        assert_eq!(plan.buffer_lengths::<Scale26Dot6>().unscaled, 40);
        plan.max_hinted_composite_points = 4;
        assert_eq!(plan.buffer_lengths::<Scale26Dot6>().unscaled, 10);
    }

    #[test]
    fn detects_hinting() {
        let font = FontRef::new(font_test_data::COUSINE_HINT_SUBSET).unwrap();
        let context = OutlineTables::new(&font).unwrap();
        let n = font.maxp().unwrap().num_glyphs();
        let hinted = (0..n)
            .filter(|gid| {
                OutlinePlan::new(&context, GlyphId::from(*gid))
                    .map(|plan| plan.has_hinting)
                    .unwrap_or(false)
            })
            .count();
        assert!(hinted > 0, "expected some instructed glyphs");
    }

    /// Builds a `glyf`/`loca` pair from raw glyph records.
    fn tables_from_glyphs(glyphs: &[Vec<u8>]) -> (Vec<u8>, Vec<u8>) {
        let mut glyf = Vec::new();
        let mut loca: Vec<u8> = Vec::new();
        for glyph in glyphs {
            loca.extend_from_slice(&(glyf.len() as u32).to_be_bytes());
            glyf.extend_from_slice(glyph);
        }
        loca.extend_from_slice(&(glyf.len() as u32).to_be_bytes());
        (glyf, loca)
    }

    /// A composite glyph record referencing each of `components` in turn.
    fn composite(components: &[u16]) -> Vec<u8> {
        const ARGS_ARE_WORDS: u16 = 0x0001;
        const MORE_COMPONENTS: u16 = 0x0020;
        let mut out = Vec::new();
        out.extend_from_slice(&(-1i16).to_be_bytes()); // numberOfContours
        out.extend_from_slice(&[0u8; 8]); // bounding box
        for (i, &gid) in components.iter().enumerate() {
            let mut flags = ARGS_ARE_WORDS;
            if i + 1 < components.len() {
                flags |= MORE_COMPONENTS;
            }
            out.extend_from_slice(&flags.to_be_bytes());
            out.extend_from_slice(&gid.to_be_bytes());
            out.extend_from_slice(&0i16.to_be_bytes()); // arg1
            out.extend_from_slice(&0i16.to_be_bytes()); // arg2
        }
        out
    }

    /// An empty simple glyph record: zero contours, so it contributes nothing.
    fn empty_simple() -> Vec<u8> {
        let mut out = Vec::new();
        out.extend_from_slice(&0i16.to_be_bytes()); // numberOfContours
        out.extend_from_slice(&[0u8; 8]); // bounding box
        out.extend_from_slice(&0u16.to_be_bytes()); // instructionLength
        out
    }

    fn info_from_raw(glyf_bytes: &[u8], loca_bytes: &[u8], gid: u32) -> Result<(), OutlineError> {
        let glyf = Glyf::read(FontData::new(glyf_bytes)).unwrap();
        let loca = Loca::read(FontData::new(loca_bytes), true).unwrap();
        let context = OutlineTables {
            glyf,
            loca,
            gvar: None,
            hmtx: None,
            os2: None,
            hvar: None,
            units_per_em: 1000,
            coords: &[],
            gvar_scalars: &[],
        };
        OutlinePlan::new(&context, GlyphId::new(gid)).map(|_| ())
    }

    /// A composite that references itself recurses forever without the depth
    /// limit.
    #[test]
    fn self_referencing_composite_hits_the_recursion_limit() {
        let (glyf, loca) = tables_from_glyphs(&[composite(&[0])]);
        assert_eq!(
            info_from_raw(&glyf, &loca, 0),
            Err(OutlineError::RecursionLimitExceeded)
        );
    }

    /// A chain shorter than the limit is fine, and one past it is not.
    #[test]
    fn recursion_limit_boundary() {
        // A chain of composites, each referencing the next, ending in a simple
        // glyph. Depth is the number of composites.
        let chain = |depth: usize| -> (Vec<u8>, Vec<u8>) {
            let mut glyphs: Vec<Vec<u8>> =
                (0..depth).map(|i| composite(&[(i + 1) as u16])).collect();
            glyphs.push(empty_simple());
            tables_from_glyphs(&glyphs)
        };
        let (glyf, loca) = chain(MAX_RECURSION_DEPTH);
        assert!(info_from_raw(&glyf, &loca, 0).is_ok());
        let (glyf, loca) = chain(MAX_RECURSION_DEPTH + 2);
        assert_eq!(
            info_from_raw(&glyf, &loca, 0),
            Err(OutlineError::RecursionLimitExceeded)
        );
    }

    /// Fanning out wide stays shallow, so only the component limit catches it.
    #[test]
    fn wide_composite_hits_the_component_limit() {
        let within: Vec<u16> = vec![1; MAX_COMPOSITE_EDGES];
        let (glyf, loca) = tables_from_glyphs(&[composite(&within), empty_simple()]);
        assert!(info_from_raw(&glyf, &loca, 0).is_ok());

        let beyond: Vec<u16> = vec![1; MAX_COMPOSITE_EDGES + 1];
        let (glyf, loca) = tables_from_glyphs(&[composite(&beyond), empty_simple()]);
        assert_eq!(
            info_from_raw(&glyf, &loca, 0),
            Err(OutlineError::TooManyComponents)
        );
    }

    /// A component pointing at an empty glyph is skipped rather than counted
    /// or treated as an error.
    #[test]
    fn empty_components_are_skipped() {
        // Glyph 1 has a zero length loca entry, so it is empty.
        let mut glyf = Vec::new();
        let mut loca: Vec<u8> = Vec::new();
        let root = composite(&[1]);
        loca.extend_from_slice(&0u32.to_be_bytes());
        glyf.extend_from_slice(&root);
        // Both remaining offsets point at the end, giving glyph 1 no data.
        loca.extend_from_slice(&(glyf.len() as u32).to_be_bytes());
        loca.extend_from_slice(&(glyf.len() as u32).to_be_bytes());
        assert!(info_from_raw(&glyf, &loca, 0).is_ok());
    }
}
