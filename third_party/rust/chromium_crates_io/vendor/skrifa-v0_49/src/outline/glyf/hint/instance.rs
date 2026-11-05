//! Instance state for TrueType hinting.

use super::cleartype::ClearType;
use super::{
    cleartype::{ClearTypeState, RasterPolicy},
    cow_slice::CowSlice,
    definition::{Definition, DefinitionMap, DefinitionState},
    dropout,
    engine::Engine,
    graphics::RetainedGraphicsState,
    program::{Program, ProgramState},
    value_stack::ValueStack,
    zone::Zone,
    DropoutControl, HintError, HintOutline, HintTables, PointFlags, Target,
};
use alloc::vec::Vec;
use raw::types::{F26Dot6, F2Dot14, Fixed, Point};

#[derive(Clone, Default)]
pub struct HintInstance {
    functions: Vec<Definition>,
    instructions: Vec<Definition>,
    cvt: Vec<i32>,
    storage: Vec<i32>,
    graphics: RetainedGraphicsState,
    twilight_scaled: Vec<Point<F26Dot6>>,
    twilight_original_scaled: Vec<Point<F26Dot6>>,
    twilight_flags: Vec<PointFlags>,
    axis_count: u16,
    max_stack: usize,
    cleartype: ClearTypeState,
}

impl HintInstance {
    /// Runs the font and control value programs for a size and location.
    ///
    /// The caller resolves `cleartype` because it depends on the font's
    /// settings at the size.
    pub fn reconfigure(
        &mut self,
        tables: &HintTables,
        scale: i32,
        ppem: i32,
        target: Target,
        coords: &[F2Dot14],
        cleartype: ClearTypeState,
    ) -> Result<(), HintError> {
        self.setup(tables, scale, coords);
        self.cleartype = cleartype;
        let twilight_contours = [self.twilight_scaled.len() as u16];
        let twilight = Zone::new(
            &[],
            &mut self.twilight_original_scaled,
            &mut self.twilight_scaled,
            &mut self.twilight_flags,
            &twilight_contours,
        );
        let glyph = Zone::default();
        let mut stack_buf = vec![0; self.max_stack];
        let value_stack = ValueStack::new(&mut stack_buf, false);
        let graphics = RetainedGraphicsState::new(scale, ppem, target);
        let mut engine = Engine::new(
            tables.cvt.len(),
            ProgramState::new(tables.fpgm, tables.prep, &[], Program::Font),
            graphics,
            DefinitionState::new(
                DefinitionMap::Mut(&mut self.functions),
                DefinitionMap::Mut(&mut self.instructions),
            ),
            CowSlice::new_mut(&mut self.cvt),
            CowSlice::new_mut(&mut self.storage),
            value_stack,
            twilight,
            glyph,
            self.axis_count,
            coords,
            false,
            self.cleartype,
        );
        // Run the font program (fpgm)
        engine.run_program(Program::Font, false)?;
        // Run the control value program (prep)
        engine.run_program(Program::ControlValue, false)?;
        // Save the retained state from the CV program
        self.graphics = *engine.retained_graphics_state();
        Ok(())
    }

    /// Returns true if we should actually apply hinting.
    ///
    /// The control value program can disable hinting completely. For an
    /// antialiased target, the ClearType `gasp` policy can also disable it.
    pub fn is_enabled(&self) -> bool {
        if self.is_disabled_by_program() {
            return false;
        }
        // A font that clears the ClearType grid fitting flag for this size is
        // asking not to be grid fit at all, whatever the antialiasing.
        if self.cleartype.is_native_enhanced()
            && self.graphics.target.is_smooth()
            && !self.cleartype.policy.grid_fit
        {
            return false;
        }
        true
    }

    /// Returns true if the control value program disabled hinting for the
    /// size.
    ///
    /// The program does this through bit 0 of the instruction control flags.
    pub fn is_disabled_by_program(&self) -> bool {
        self.graphics.instruct_control & 1 != 0
    }

    /// Returns the dropout control left by the control value program.
    ///
    /// It applies to any glyph that does not set its own.
    pub fn dropout_control(&self) -> DropoutControl {
        dropout::from_scan_state(self.graphics.scan_control, self.graphics.scan_type)
    }

    /// Returns the smoothing and grid fitting the font requests at this size.
    pub fn raster_policy(&self) -> RasterPolicy {
        self.cleartype.policy
    }

    /// Returns true if the horizontal offset of a composite component with
    /// `ROUND_XY_TO_GRID` should be rounded to a full pixel.
    pub(crate) fn round_composite_x_offset(&self) -> bool {
        // ClearType positions components on a finer horizontal grid, which
        // the interpreter approximates by not rounding at all. Monochrome
        // rendering and hint reliant fonts keep the full pixel grid.
        self.cleartype.mode == ClearType::Enhanced
            && (!self.graphics.target.is_smooth() || self.cleartype.is_hint_reliant)
    }

    /// Returns true if backward compatibility mode has been activated
    /// by the hinter settings or the `prep` table.
    pub fn backward_compatibility(&self) -> bool {
        // As the glyph program sets it up.
        if self.cleartype.is_hint_reliant {
            false
        } else if self.graphics.target.preserve_linear_metrics() {
            true
        } else if self.graphics.target.is_smooth() {
            (self.graphics.instruct_control & 0x4) == 0
        } else {
            false
        }
    }

    /// Runs the glyph program and returns the dropout control it requests.
    ///
    /// A failing program is an error only when `is_pedantic` is set.
    pub fn hint(
        &self,
        tables: &HintTables,
        outline: &mut HintOutline,
        is_pedantic: bool,
    ) -> Result<DropoutControl, HintError> {
        // Twilight zone
        let twilight_count = outline.twilight_scaled.len();
        let twilight_contours = [twilight_count as u16];
        outline
            .twilight_original_scaled
            .copy_from_slice(&self.twilight_original_scaled);
        outline
            .twilight_scaled
            .copy_from_slice(&self.twilight_scaled);
        outline.twilight_flags.copy_from_slice(&self.twilight_flags);
        let twilight = Zone::new(
            &[],
            outline.twilight_original_scaled,
            outline.twilight_scaled,
            outline.twilight_flags,
            &twilight_contours,
        );
        // Glyph zone
        let glyph = Zone::new(
            outline.unscaled,
            outline.original_scaled,
            outline.scaled,
            outline.flags,
            outline.contours,
        );
        let value_stack = ValueStack::new(outline.stack, is_pedantic);
        let cvt = CowSlice::new(&self.cvt, outline.cvt).unwrap();
        let storage = CowSlice::new(&self.storage, outline.storage).unwrap();
        let mut engine = Engine::new(
            tables.cvt.len(),
            ProgramState::new(tables.fpgm, tables.prep, outline.bytecode, Program::Glyph),
            self.graphics,
            DefinitionState::new(
                DefinitionMap::Ref(&self.functions),
                DefinitionMap::Ref(&self.instructions),
            ),
            cvt,
            storage,
            value_stack,
            twilight,
            glyph,
            self.axis_count,
            outline.coords,
            outline.is_composite,
            self.cleartype,
        );
        // A failure stops the outline only when hinting is pedantic.
        // Otherwise the glyph keeps whatever the program did before it
        // failed, including its dropout control and phantom points, as in
        // FreeType.
        if let Err(mut error) = engine.run_program(Program::Glyph, is_pedantic) {
            if is_pedantic {
                error.glyph_id = Some(outline.glyph_id);
                return Err(error);
            }
        }
        // Scan conversion state is retained, so the glyph program may have
        // changed it from whatever the control value program left.
        let retained = engine.retained_graphics_state();
        let dropout_control = dropout::from_scan_state(retained.scan_control, retained.scan_type);
        let backward_compatibility = engine.backward_compatibility();
        // If we're not running in backward compatibility mode, capture
        // modified phantom points.
        if !backward_compatibility {
            for (i, p) in (outline.scaled[outline.scaled.len() - 4..])
                .iter()
                .enumerate()
            {
                outline.phantom[i] = *p;
            }
        }
        Ok(dropout_control)
    }

    /// Captures limits, resizes buffers and scales the CVT.
    fn setup(&mut self, tables: &HintTables, scale: i32, coords: &[F2Dot14]) {
        let axis_count = tables.axis_count;
        self.functions.clear();
        self.functions
            .resize(tables.max_function_defs as usize, Definition::default());
        self.instructions
            .resize(tables.max_instruction_defs as usize, Definition::default());
        self.cvt.clear();
        let cvt = tables.cvt;
        if let Some(cvar) = &tables.cvar {
            // First accumulate all the deltas in 16.16
            self.cvt.resize(cvt.len(), 0);
            let _ = cvar.deltas(axis_count, coords, &mut self.cvt);
            // Now add the base CVT values
            for (value, base_value) in self.cvt.iter_mut().zip(cvt.iter()) {
                // Deltas are converted from 16.16 to 26.6
                // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/truetype/ttgxvar.c#L3822>
                let delta = Fixed::from_bits(*value).to_f26dot6().to_bits();
                let base_value = base_value.get() as i32 * 64;
                *value = base_value + delta;
            }
        } else {
            // CVT values are converted to 26.6 on load
            // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/truetype/ttpload.c#L350>
            self.cvt
                .extend(cvt.iter().map(|value| (value.get() as i32) * 64));
        }
        // More weird scaling. This is due to the fact that CVT values are
        // already in 26.6
        // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/truetype/ttobjs.c#L996>
        let scale = Fixed::from_bits(scale >> 6);
        for value in &mut self.cvt {
            *value = (Fixed::from_bits(*value) * scale).to_bits();
        }
        self.storage.clear();
        self.storage.resize(tables.max_storage as usize, 0);
        let max_twilight_points = tables.max_twilight_points as usize;
        self.twilight_scaled.clear();
        self.twilight_scaled
            .resize(max_twilight_points, Default::default());
        self.twilight_original_scaled.clear();
        self.twilight_original_scaled
            .resize(max_twilight_points, Default::default());
        self.twilight_flags.clear();
        self.twilight_flags
            .resize(max_twilight_points, Default::default());
        self.axis_count = axis_count;
        self.max_stack = tables.max_stack_elements as usize;
        self.graphics = RetainedGraphicsState::default();
    }
}

#[cfg(test)]
impl HintInstance {
    /// Enable instruct control bit 1 which effectively disables hinting.
    ///
    /// This mimics what the `prep` table might do for various configurations
    /// and font sizes. Used for testing.    
    pub fn simulate_prep_flag_suppress_hinting(&mut self) {
        self.graphics.instruct_control |= 1;
    }
}

#[cfg(test)]
mod tests {
    use super::{super::super::Outlines, HintInstance};
    use read_fonts::{types::F2Dot14, FontRef};

    #[test]
    fn scaled_cvar_cvt() {
        let font = FontRef::new(font_test_data::CVAR).unwrap();
        let outlines = Outlines::new(&font).unwrap();
        let mut instance = HintInstance::default();
        let coords = [0.5, -0.5].map(F2Dot14::from_f32);
        let ppem = 16;
        // ppem * 64 / upem
        let scale = 67109;
        instance
            .reconfigure(
                &outlines.hint_tables(),
                scale,
                ppem,
                Default::default(),
                &coords,
                Default::default(),
            )
            .unwrap();
        let expected = [
            778, 10, 731, 0, 731, 10, 549, 10, 0, 0, 0, -10, 0, -10, -256, -10, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 95, 137, 99, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 60, 0, 81, 0, 0, 0, 0, 0, 51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        ];
        assert_eq!(&instance.cvt, &expected);
    }
}
