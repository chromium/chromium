//! Parsing for PostScript charstrings.

use super::{dict::FontMatrix, BlendState, Error, Index, Stack};
use crate::{
    tables::{cff::Cff, postscript::StringId},
    types::{Fixed, Point},
    Cursor, FontData, FontRead,
};

/// Maximum nesting depth for subroutine calls.
///
/// See "Appendix B Type 2 Charstring Implementation Limits" at
/// <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=33>
pub const NESTING_DEPTH_LIMIT: u32 = 10;

/// The type of a PostScript charstring.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum CharstringKind {
    /// Type1 charstring.
    ///
    /// See reference at <https://adobe-type-tools.github.io/font-tech-notes/pdfs/T1_SPEC.pdf>.
    Type1,
    /// Type2 charstring.
    ///
    /// See reference at <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf>.
    Type2,
}

/// Trait that provides context for charstring evaluation.
pub trait CharstringContext {
    /// Returns the type of the charstring.
    fn kind(&self) -> CharstringKind;

    /// Returns the base and accent charstrings for the `seac` (standard
    /// encoded accented character) operator.
    fn seac_components(&self, base_code: i32, accent_code: i32) -> Result<[&[u8]; 2], Error>;

    /// Returns the charstring for the global subroutine at the given index as
    /// encoded in the calling charstring.
    fn global_subr(&self, index: i32) -> Result<&[u8], Error>;

    /// Returns the charstring for the local subroutine at the given index as
    /// encoded in the calling charstring.
    fn subr(&self, index: i32) -> Result<&[u8], Error>;

    /// Returns the current active weight vector for a multiple master font.
    fn weight_vector(&self) -> &[Fixed] {
        &[]
    }
}

// Ugly temporary impl to support existing skrifa code until it is replaced
// with CffFontRef.
//
// Types are (cff_blob, charstrings, global_subrs, subrs)
impl<'a> CharstringContext for (&'a [u8], &'a Index<'a>, &'a Index<'a>, &'a Index<'a>) {
    fn kind(&self) -> CharstringKind {
        CharstringKind::Type2
    }

    fn seac_components(&self, base_code: i32, accent_code: i32) -> Result<[&[u8]; 2], Error> {
        let cff = Cff::read(FontData::new(self.0))?;
        let charset = cff.charset(0)?.ok_or(Error::MissingCharset)?;
        let seac_to_gid = |code: i32| {
            let code: u8 = code.try_into().ok()?;
            let sid = *super::encoding::STANDARD_ENCODING.get(code as usize)?;
            charset.glyph_id(StringId::new(sid as u16)).ok()
        };
        let accent_gid = seac_to_gid(accent_code).ok_or(Error::InvalidSeacCode(accent_code))?;
        let base_gid = seac_to_gid(base_code).ok_or(Error::InvalidSeacCode(base_code))?;
        let accent_charstring = self.1.get(accent_gid.to_u32() as usize)?;
        let base_charstring = self.1.get(base_gid.to_u32() as usize)?;
        Ok([base_charstring, accent_charstring])
    }

    fn global_subr(&self, index: i32) -> Result<&[u8], Error> {
        self.2.get((index + self.2.subr_bias()) as usize)
    }

    fn subr(&self, index: i32) -> Result<&[u8], Error> {
        self.3.get((index + self.3.subr_bias()) as usize)
    }
}

/// Trait for processing commands resulting from charstring evaluation.
///
/// During processing, the path construction operators (see "4.1 Path
/// Construction Operators" at <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=15>)
/// are simplified into the basic move, line, curve and close commands.
///
/// This also has optional callbacks for processing hint operators. See "4.3
/// Hint Operators" at <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=21>
/// for more detail.
#[allow(unused_variables)]
pub trait CommandSink {
    // Path construction operators.
    fn move_to(&mut self, x: Fixed, y: Fixed);
    fn line_to(&mut self, x: Fixed, y: Fixed);
    fn curve_to(&mut self, cx0: Fixed, cy0: Fixed, cx1: Fixed, cy1: Fixed, x: Fixed, y: Fixed);
    fn close(&mut self);
    // Hint operators.
    /// Horizontal stem hint at `y` with height `dy`.
    fn hstem(&mut self, y: Fixed, dy: Fixed) {}
    /// Vertical stem hint at `x` with width `dx`.
    fn vstem(&mut self, x: Fixed, dx: Fixed) {}
    /// Bitmask defining the hints that should be made active for the
    /// commands that follow.
    fn hint_mask(&mut self, mask: &[u8]) {}
    /// Bitmask defining the counter hints that should be made active for the
    /// commands that follow.
    fn counter_mask(&mut self, mask: &[u8]) {}
    /// Clear accumulated stem hints and all data derived from them.
    fn clear_hints(&mut self) {}
    /// Called when charstring evaluation is complete.
    fn finish(&mut self) {}
}

/// Evaluates the given charstring and emits the resulting commands to the
/// specified sink.
///
/// If the Private DICT associated with this charstring contains local
/// subroutines, then the `subrs` index must be provided, otherwise
/// `Error::MissingSubroutines` will be returned if a callsubr operator
/// is present.
///
/// If evaluating a CFF2 charstring and the top-level table contains an
/// item variation store, then `blend_state` must be provided, otherwise
/// `Error::MissingBlendState` will be returned if a blend operator is
/// present.
pub fn evaluate<'a>(
    context: &'a impl CharstringContext,
    blend_state: Option<BlendState<'a>>,
    charstring_data: &[u8],
    sink: &'a mut impl CommandSink,
) -> Result<Option<Fixed>, Error> {
    let mut evaluator = Evaluator::new(context, blend_state, sink);
    evaluator.evaluate(charstring_data, 0)?;
    let width = evaluator.have_read_width.then_some(evaluator.wx);
    sink.finish();
    Ok(width)
}

/// Specifies how the seac operation was invoked.
#[derive(PartialEq)]
enum SeacMode {
    /// Through the `seac` operator.
    Explicit,
    /// Implicitly with extra arguments on the stack through the
    /// `endchar` operator.
    Implicit,
}

/// Transient state for evaluating a charstring and handling recursive
/// subroutine calls.
struct Evaluator<'a, S> {
    context: &'a dyn CharstringContext,
    is_type1: bool,
    blend_state: Option<BlendState<'a>>,
    sink: &'a mut S,
    is_open: bool,
    /// When the flex state is active, moveto commands simply
    /// accumulate vectors on the stack which will be used
    /// to emit curves when the flex is finalized
    is_flexing: bool,
    have_read_width: bool,
    stem_count: usize,
    x: Fixed,
    y: Fixed,
    /// X side-bearing
    sbx: Fixed,
    /// X width
    wx: Fixed,
    stack: Stack,
    stack_ix: usize,
}

impl<'a, S> Evaluator<'a, S>
where
    S: CommandSink,
{
    fn new(
        context: &'a dyn CharstringContext,
        blend_state: Option<BlendState<'a>>,
        sink: &'a mut S,
    ) -> Self {
        let is_type1 = context.kind() == CharstringKind::Type1;
        Self {
            context,
            is_type1,
            blend_state,
            sink,
            is_open: false,
            is_flexing: false,
            have_read_width: false,
            stem_count: 0,
            stack: Stack::new(),
            x: Fixed::ZERO,
            y: Fixed::ZERO,
            sbx: Fixed::ZERO,
            wx: Fixed::ZERO,
            stack_ix: 0,
        }
    }

    fn evaluate(&mut self, charstring_data: &[u8], nesting_depth: u32) -> Result<(), Error> {
        if nesting_depth > NESTING_DEPTH_LIMIT {
            return Err(Error::CharstringNestingDepthLimitExceeded);
        }
        let mut cursor = crate::FontData::new(charstring_data).cursor();
        while cursor.remaining_bytes() != 0 {
            let b0 = cursor.read::<u8>()?;
            match b0 {
                // See "3.2 Charstring Number Encoding" <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=12>
                //
                // Push an integer to the stack
                28 | 32..=254 => {
                    self.stack.push(super::dict::parse_int(&mut cursor, b0)?)?;
                }
                // Push a fixed point value to the stack
                255 => {
                    let val = cursor.read::<i32>()?;
                    if self.is_type1 {
                        // Type1 interprets this as an integer
                        self.stack.push(val)?;
                    } else {
                        // Type2 interprets this as a raw 16.16 fixed point
                        // value
                        self.stack.push(Fixed::from_bits(val))?;
                    }
                }
                _ => {
                    // FreeType ignores reserved (unknown) operators.
                    // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L703>
                    // and fontations issue <https://github.com/googlefonts/fontations/issues/1680>
                    if let Ok(operator) = Operator::read(&mut cursor, b0) {
                        if !self.evaluate_operator(operator, &mut cursor, nesting_depth)? {
                            break;
                        }
                    } else {
                        // Clear the stack for unknown operators
                        self.reset_stack();
                    }
                }
            }
        }
        Ok(())
    }

    /// Evaluates a single charstring operator.
    ///
    /// Returns `Ok(true)` if evaluation should continue.
    fn evaluate_operator(
        &mut self,
        operator: Operator,
        cursor: &mut Cursor,
        nesting_depth: u32,
    ) -> Result<bool, Error> {
        use Operator::*;
        use PointMode::*;
        match operator {
            // The following "flex" operators are intended to emit
            // either two curves or a straight line depending on
            // a "flex depth" parameter and the distance from the
            // joining point to the chord connecting the two
            // end points. In practice, we just emit the two curves,
            // following FreeType:
            // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L335>
            //
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=18>
            Flex => {
                self.emit_curves([DxDy; 6])?;
                self.reset_stack();
            }
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=19>
            HFlex => {
                self.emit_curves([DxY, DxDy, DxY, DxY, DxInitialY, DxY])?;
                self.reset_stack();
            }
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=19>
            HFlex1 => {
                self.emit_curves([DxDy, DxDy, DxY, DxY, DxDy, DxInitialY])?;
                self.reset_stack();
            }
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=20>
            Flex1 => {
                self.emit_curves([DxDy, DxDy, DxDy, DxDy, DxDy, DLargerCoordDist])?;
                self.reset_stack();
            }
            // Set the variation store index
            // <https://learn.microsoft.com/en-us/typography/opentype/spec/cff2charstr#syntax-for-font-variations-support-operators>
            VariationStoreIndex => {
                if !self.is_type1 {
                    let blend_state = self.blend_state.as_mut().ok_or(Error::MissingBlendState)?;
                    let store_index = self.stack.pop_i32()? as u16;
                    blend_state.set_store_index(store_index)?;
                }
            }
            // Apply blending to the current operand stack
            // <https://learn.microsoft.com/en-us/typography/opentype/spec/cff2charstr#syntax-for-font-variations-support-operators>
            Blend => {
                if !self.is_type1 {
                    let blend_state = self.blend_state.as_ref().ok_or(Error::MissingBlendState)?;
                    self.stack.apply_blend(blend_state)?;
                }
            }
            // Return from the current subroutine
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=29>
            Return => {
                return Ok(false);
            }
            // End the current charstring
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=21>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L2463>
            EndChar => {
                if self.stack.len() == 4 || self.stack.len() == 5 && !self.have_read_width {
                    self.handle_seac(SeacMode::Implicit, nesting_depth)?;
                } else if !self.stack.is_empty() && !self.have_read_width {
                    self.read_width()?;
                    self.stack.clear();
                }
                if self.is_open {
                    self.is_open = false;
                    self.sink.close();
                }
                return Ok(false);
            }
            // Emits a sequence of stem hints
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=21>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L777>
            HStem | VStem | HStemHm | VStemHm => {
                let mut i = 0;
                let len = if self.stack.len_is_odd() && !self.have_read_width {
                    self.read_width()?;
                    i = 1;
                    self.stack.len() - 1
                } else {
                    self.stack.len()
                };
                let is_horizontal = matches!(operator, HStem | HStemHm);
                let mut u = Fixed::ZERO;
                while i < self.stack.len() {
                    let args = self.stack.fixed_array::<2>(i)?;
                    u += args[0];
                    let w = args[1];
                    let v = u.wrapping_add(w);
                    if is_horizontal {
                        self.sink.hstem(u, v);
                    } else {
                        self.sink.vstem(u, v);
                    }
                    u = v;
                    i += 2;
                }
                self.stem_count += len / 2;
                self.reset_stack();
            }
            // Applies a hint or counter mask.
            // If there are arguments on the stack, this is also an
            // implied series of VSTEMHM operators.
            // Hint and counter masks are bitstrings that determine
            // the currently active set of hints.
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=24>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L2580>
            HintMask | CntrMask => {
                let mut i = 0;
                let len = if self.stack.len_is_odd() && !self.have_read_width {
                    self.read_width()?;
                    i = 1;
                    self.stack.len() - 1
                } else {
                    self.stack.len()
                };
                let mut u = Fixed::ZERO;
                while i < self.stack.len() {
                    let args = self.stack.fixed_array::<2>(i)?;
                    u += args[0];
                    let w = args[1];
                    let v = u + w;
                    self.sink.vstem(u, v);
                    u = v;
                    i += 2;
                }
                self.stem_count += len / 2;
                let count = self.stem_count.div_ceil(8);
                let mask = cursor.read_array::<u8>(count)?;
                if operator == HintMask {
                    self.sink.hint_mask(mask);
                } else {
                    self.sink.counter_mask(mask);
                }
                self.reset_stack();
            }
            // Starts a new subpath
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=16>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L2653>
            RMoveTo => {
                let mut i = 0;
                if self.stack.len() == 3 && !self.have_read_width {
                    self.read_width()?;
                    i = 1;
                }
                if !self.is_flexing {
                    let [dx, dy] = self.stack.fixed_array::<2>(i)?;
                    self.x += dx;
                    self.y += dy;
                    if !self.is_open {
                        self.is_open = true;
                    } else {
                        self.sink.close();
                    }
                    self.sink.move_to(self.x, self.y);
                    self.reset_stack();
                }
            }
            // Starts a new subpath by moving the current point in the
            // horizontal or vertical direction
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=16>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L839>
            HMoveTo | VMoveTo => {
                let mut i = 0;
                if self.stack.len() == 2 && !self.have_read_width {
                    self.read_width()?;
                    i = 1;
                }
                if self.is_flexing {
                    // We need to add the other coordinate to the stack so we
                    // have a full flex vector
                    self.stack.push(0)?;
                    if operator == VMoveTo {
                        // For vertical move, the coordinates are in the wrong
                        // order so swap them
                        self.stack.exch()?;
                    }
                } else {
                    let delta = self.stack.get_fixed(i)?;
                    if operator == HMoveTo {
                        self.x += delta;
                    } else {
                        self.y += delta;
                    }
                    if !self.is_open {
                        self.is_open = true;
                    } else {
                        self.sink.close();
                    }
                    self.sink.move_to(self.x, self.y);
                    self.reset_stack();
                }
            }
            // Emits a sequence of lines
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=16>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L863>
            RLineTo => {
                let mut i = 0;
                while i < self.stack.len() {
                    let [dx, dy] = self.stack.fixed_array::<2>(i)?;
                    self.x += dx;
                    self.y += dy;
                    self.emit_line(self.x, self.y);
                    i += 2;
                }
                self.reset_stack();
            }
            // Emits a sequence of alternating horizontal and vertical
            // lines
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=16>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L885>
            HLineTo | VLineTo => {
                let mut is_x = operator == HLineTo;
                for i in 0..self.stack.len() {
                    let delta = self.stack.get_fixed(i)?;
                    if is_x {
                        self.x += delta;
                    } else {
                        self.y += delta;
                    }
                    is_x = !is_x;
                    self.emit_line(self.x, self.y);
                }
                self.reset_stack();
            }
            // Emits curves that start and end horizontal, unless
            // the stack count is odd, in which case the first
            // curve may start with a vertical tangent
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=17>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L2789>
            HhCurveTo => {
                let count1 = self.stack.len();
                let count = count1 & !2;
                self.stack_ix = count1 - count;
                while self.stack_ix < count {
                    if (count - self.stack_ix) & 1 != 0 {
                        self.y += self.stack.get_fixed(self.stack_ix)?;
                        self.stack_ix += 1;
                    }
                    self.emit_curves([DxY, DxDy, DxY])?;
                }
                self.reset_stack();
            }
            // Alternates between curves with horizontal and vertical
            // tangents
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=17>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L2834>
            HvCurveTo | VhCurveTo => {
                let count1 = self.stack.len();
                let count = count1 & !2;
                let mut is_horizontal = operator == HvCurveTo;
                self.stack_ix = count1 - count;
                while self.stack_ix < count {
                    let do_last_delta = count - self.stack_ix == 5;
                    if is_horizontal {
                        self.emit_curves([DxY, DxDy, MaybeDxDy(do_last_delta)])?;
                    } else {
                        self.emit_curves([XDy, DxDy, DxMaybeDy(do_last_delta)])?;
                    }
                    is_horizontal = !is_horizontal;
                }
                self.reset_stack();
            }
            // Emits a sequence of curves possibly followed by a line
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=17>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L915>
            RrCurveTo | RCurveLine => {
                while self.coords_remaining() >= 6 {
                    self.emit_curves([DxDy; 3])?;
                }
                if operator == RCurveLine {
                    let [dx, dy] = self.stack.fixed_array::<2>(self.stack_ix)?;
                    self.x += dx;
                    self.y += dy;
                    self.emit_line(self.x, self.y);
                }
                self.reset_stack();
            }
            // Emits a sequence of lines followed by a curve
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=18>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L2702>
            RLineCurve => {
                while self.coords_remaining() > 6 {
                    let [dx, dy] = self.stack.fixed_array::<2>(self.stack_ix)?;
                    self.x += dx;
                    self.y += dy;
                    self.emit_line(self.x, self.y);
                    self.stack_ix += 2;
                }
                self.emit_curves([DxDy; 3])?;
                self.reset_stack();
            }
            // Emits curves that start and end vertical, unless
            // the stack count is odd, in which case the first
            // curve may start with a horizontal tangent
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=18>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L2744>
            VvCurveTo => {
                let count1 = self.stack.len();
                let count = count1 & !2;
                self.stack_ix = count1 - count;
                while self.stack_ix < count {
                    if (count - self.stack_ix) & 1 != 0 {
                        self.x += self.stack.get_fixed(self.stack_ix)?;
                        self.stack_ix += 1;
                    }
                    self.emit_curves([XDy, DxDy, XDy])?;
                }
                self.reset_stack();
            }
            // Call local or global subroutine
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=29>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L972>
            CallSubr | CallGsubr => {
                let index = self.stack.pop_i32()?;
                let subr_charstring = if operator == CallSubr {
                    self.context.subr(index)?
                } else {
                    self.context.global_subr(index)?
                };
                self.evaluate(subr_charstring, nesting_depth + 1)?;
            }
            // Sets the left sidebearing point to (sbx, 0) and the character
            // width vector to (wx, 0) in character space. Also sets current
            // point to (sbx, 0).
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/T1_SPEC.pdf#page=56>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L2429>
            Hsbw => {
                if self.is_type1 {
                    let [sbx, wx] = self.stack.fixed_array(0)?;
                    self.sbx += sbx;
                    self.x += sbx;
                    self.wx = wx;
                    self.have_read_width = true;
                    self.reset_stack();
                }
            }
            // Standard Encoding Accented Character.
            // Makes an accented character from two other characters in the
            // font program.
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/T1_SPEC.pdf#page=56>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L1294>
            Seac => {
                self.handle_seac(SeacMode::Explicit, nesting_depth)?;
            }
            // Sets the left sidebearing point to (sbx, sby) and the character
            // width vector to (wx, wy) in character space. Also sets current
            // point to (sbx, sby).
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/T1_SPEC.pdf#page=57>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L1496>
            Sbw => {
                if self.is_type1 {
                    let [x, y, wx, _wy] = self.stack.fixed_array(0)?;
                    self.x += x;
                    self.y += y;
                    self.sbx += x;
                    self.wx = wx;
                    self.have_read_width = true;
                    self.reset_stack();
                }
            }
            // Brackets an outline section for dots in letters such as 'i',
            // 'j' and '!'. Purely metadata that a hinter can use.
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/T1_SPEC.pdf#page=58>
            DotSection => {
                // Nothing to do.
            }
            // Declares ranges for three horizontal or vertical stem zones.
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/T1_SPEC.pdf#page=59>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L1199>
            HStem3 | VStem3 => {
                // Currently unimplemented.
                self.reset_stack();
            }
            // Division operator.
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/T1_SPEC.pdf#page=60>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L1586>
            Div => {
                self.stack.div(self.is_type1)?;
            }
            // Mechanism for making calls into the PostScript interpreter.
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/T1_SPEC.pdf#page=61>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L1644>
            CallOtherSubr => {
                let subr_idx = self.stack.pop_i32()?;
                let num_args = self.stack.pop_i32()? as usize;
                let weight_vector = self.context.weight_vector();
                match (subr_idx, num_args) {
                    // End flex. Emit curves from accumulated vectors on the
                    // stack.
                    (0, 3) => {
                        self.is_flexing = false;
                        self.ensure_open();
                        self.handle_flex()?;
                    }
                    // Begin flex. Accumulate vectors from moveto operators.
                    (1, 0) => {
                        self.is_flexing = true;
                    }
                    // Counter control hints.
                    // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L1817>
                    (12 | 13, _) => {
                        self.reset_stack();
                    }
                    // Handle blends for multiple masters.
                    // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L1823>
                    (14..=18, _) if weight_vector.len() > 1 => {
                        self.handle_mm_blend(subr_idx, num_args)?;
                    }
                    _ => {
                        // Unknown othersubr, so simply drop the arguments
                        // from the stack and hopefully we can keep going
                        self.stack.drop(num_args);
                    }
                }
            }
            // Removes a number from the PostScript interpreter stack and
            // pushes that number to the BuildChar stack. Only used to
            // retrieve results from OtherSubrs procedures and those are
            // handled explicitly so this is a nop.
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/T1_SPEC.pdf#page=61>
            Pop => {
                // Nothing to do.
            }
            // Sets the current point without performing a move command.
            // Spec: <https://adobe-type-tools.github.io/font-tech-notes/pdfs/T1_SPEC.pdf#page=62>
            // FT: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L2379>
            SetCurrentPoint => {
                if self.is_type1 {
                    let [x, y] = self.stack.fixed_array(0)?;
                    self.x = x;
                    self.y = y;
                    self.reset_stack();
                }
            }
        }
        Ok(true)
    }

    fn read_width(&mut self) -> Result<(), Error> {
        self.wx = self.stack.get_fixed(0)?;
        self.have_read_width = true;
        Ok(())
    }

    /// See `endchar` in Appendix C at <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf#page=35>
    fn handle_seac(&mut self, mode: SeacMode, nesting_depth: u32) -> Result<(), Error> {
        // handle seac operator
        let accent_code = self.stack.pop_i32()?;
        let base_code = self.stack.pop_i32()?;
        let [base_charstring, accent_charstring] =
            self.context.seac_components(base_code, accent_code)?;
        let dy = self.stack.pop_fixed()?;
        let dx = self.stack.pop_fixed()?;
        let sb = if self.is_type1 {
            // Type1 has an additional side bearing argument
            self.stack.pop_fixed()?
        } else if !self.stack.is_empty() && !self.have_read_width {
            self.wx = self.stack.pop_fixed()?;
            self.have_read_width = true;
            Fixed::ZERO
        } else {
            Fixed::ZERO
        };
        // Save metrics to potentially restore later.
        let mut sbx = self.sbx;
        let mut wx = self.wx;
        let have_metrics = self.have_read_width;
        struct Component<'a> {
            charstring: &'a [u8],
            x: Fixed,
            y: Fixed,
            /// True if we want to use metrics from this component
            /// if the original charstring does not provide any
            maybe_use_metrics: bool,
        }
        let x = self.x;
        let y = self.y;
        // Base components for explicit seac are always 0 in FreeType
        let [bx, by] = if mode == SeacMode::Explicit {
            [Fixed::ZERO; 2]
        } else {
            [x, y]
        };
        let mut components = [
            Component {
                charstring: base_charstring,
                x: bx,
                y: by,
                // In explicit seac mode, use the metrics of the base component
                // if the original charstring didn't provide any
                maybe_use_metrics: mode == SeacMode::Explicit,
            },
            Component {
                charstring: accent_charstring,
                // Adjustments only for type1 but these will be 0 for type2
                // anyway
                x: dx + self.sbx - sb,
                y: dy,
                maybe_use_metrics: false,
            },
        ];
        // FreeType evaluates accent first for implicit seac but base first
        // for explicit so swap if necessary.
        if mode == SeacMode::Implicit {
            components.swap(0, 1);
        }
        // FreeType calls cf2_interpT2CharString for each component
        // which uses a fresh set of stem hints. Since our hinter is in
        // a separate crate, we signal this through the sink. Also
        // reset our own stem count so we read the correct number of
        // bytes for each hint mask instruction.
        // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L1443>
        // and <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L540>
        for component in components {
            self.have_read_width = false;
            self.sink.clear_hints();
            self.stem_count = 0;
            self.x = component.x;
            self.y = component.y;
            self.evaluate(component.charstring, nesting_depth + 1)?;
            if component.maybe_use_metrics && !have_metrics {
                sbx = self.sbx;
                wx = self.wx;
            }
        }
        self.sbx = sbx;
        self.wx = wx;
        Ok(())
    }

    /// Emit two curves for the accumulated flex vectors.
    fn handle_flex(&mut self) -> Result<(), Error> {
        // FreeType does weird accounting for flex vectors
        // that we don't wish to copy so do the equivalent
        // thing from fonttools instead:
        // <https://github.com/fonttools/fonttools/blob/9cec77d49bdb1a1ca346ac5fefdc5e7c30929026/Lib/fontTools/misc/psCharStrings.py#L1066>
        let final_y = self.stack.pop_fixed()?;
        let final_x = self.stack.pop_fixed()?;
        // Flex height is unused
        let _ = self.stack.pop_fixed()?;
        let p3y = self.stack.pop_fixed()?;
        let p3x = self.stack.pop_fixed()?;
        let bcp4y = self.stack.pop_fixed()?;
        let bcp4x = self.stack.pop_fixed()?;
        let bcp3y = self.stack.pop_fixed()?;
        let bcp3x = self.stack.pop_fixed()?;
        let p2y = self.stack.pop_fixed()?;
        let p2x = self.stack.pop_fixed()?;
        let bcp2y = self.stack.pop_fixed()?;
        let bcp2x = self.stack.pop_fixed()?;
        let bcp1y = self.stack.pop_fixed()?;
        let bcp1x = self.stack.pop_fixed()?;
        let rpy = self.stack.pop_fixed()?;
        let rpx = self.stack.pop_fixed()?;
        self.reset_stack();
        self.stack.push(bcp1x + rpx)?;
        self.stack.push(bcp1y + rpy)?;
        self.stack.push(bcp2x)?;
        self.stack.push(bcp2y)?;
        self.stack.push(p2x)?;
        self.stack.push(p2y)?;
        self.emit_curves([PointMode::DxDy; 3])?;
        self.reset_stack();
        self.stack.push(bcp3x)?;
        self.stack.push(bcp3y)?;
        self.stack.push(bcp4x)?;
        self.stack.push(bcp4y)?;
        self.stack.push(p3x)?;
        self.stack.push(p3y)?;
        self.emit_curves([PointMode::DxDy; 3])?;
        self.reset_stack();
        // Push final position back on the stack
        self.stack.push(final_x)?;
        self.stack.push(final_y)?;
        Ok(())
    }

    /// Handle point blending for multiple master fonts.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L1823>
    fn handle_mm_blend(&mut self, subr_idx: i32, num_args: usize) -> Result<(), Error> {
        let weight_vector = self.context.weight_vector();
        let num_points = (subr_idx - 13) as usize + (subr_idx == 18) as usize;
        if num_args != num_points * weight_vector.len() {
            return Err(Error::Read(crate::ReadError::MalformedData(
                "incorrect number of multiple masters arguments",
            )));
        }
        // The stack is setup to contain `num_points` values followed
        // by `num_points * (num_weights - 1)` deltas for each point.
        //
        // The blend algorithm is p[0] + d[0]*w[1] + d[1]*w[2]...
        // where p = points, d = deltas and w = weights
        //
        // The first weight is always ignored per FT:
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psintrp.c#L1880>
        let stack_base = self
            .stack
            .len()
            .checked_sub(num_args)
            .ok_or(Error::StackUnderflow)?;
        let mut delta_idx = stack_base + num_points;
        for i in 0..num_points {
            let mut val = self.stack.get_fixed(stack_base + i)?;
            for &weight in &weight_vector[1..] {
                val += self.stack.get_fixed(delta_idx)? * weight;
                delta_idx += 1;
            }
            self.stack.set(stack_base + i, val)?;
        }
        self.stack.drop(num_args.saturating_sub(num_points));
        Ok(())
    }

    fn coords_remaining(&self) -> usize {
        // This is overly defensive to avoid overflow but in the case of
        // broken fonts, just return 0 when stack_ix > stack_len to prevent
        // potential runaway while loops in the evaluator if this wraps
        self.stack.len().saturating_sub(self.stack_ix)
    }

    fn ensure_open(&mut self) {
        if !self.is_open {
            self.sink.move_to(Fixed::ZERO, Fixed::ZERO);
            self.is_open = true;
        }
    }

    fn emit_line(&mut self, x: Fixed, y: Fixed) {
        self.ensure_open();
        self.sink.line_to(x, y);
    }

    fn emit_curves<const N: usize>(&mut self, modes: [PointMode; N]) -> Result<(), Error> {
        use PointMode::*;
        let initial_x = self.x;
        let initial_y = self.y;
        let mut count = 0;
        let mut points = [Point::default(); 2];
        self.ensure_open();
        for mode in modes {
            let stack_used = match mode {
                DxDy => {
                    self.x += self.stack.get_fixed(self.stack_ix)?;
                    self.y += self.stack.get_fixed(self.stack_ix + 1)?;
                    2
                }
                XDy => {
                    self.y += self.stack.get_fixed(self.stack_ix)?;
                    1
                }
                DxY => {
                    self.x += self.stack.get_fixed(self.stack_ix)?;
                    1
                }
                DxInitialY => {
                    self.x += self.stack.get_fixed(self.stack_ix)?;
                    self.y = initial_y;
                    1
                }
                // Emits a delta for the coordinate with the larger distance
                // from the original value. Sets the other coordinate to the
                // original value.
                DLargerCoordDist => {
                    let delta = self.stack.get_fixed(self.stack_ix)?;
                    if (self.x - initial_x).abs() > (self.y - initial_y).abs() {
                        self.x += delta;
                        self.y = initial_y;
                    } else {
                        self.y += delta;
                        self.x = initial_x;
                    }
                    1
                }
                // Apply delta to y if `do_dy` is true.
                DxMaybeDy(do_dy) => {
                    self.x += self.stack.get_fixed(self.stack_ix)?;
                    if do_dy {
                        self.y += self.stack.get_fixed(self.stack_ix + 1)?;
                        2
                    } else {
                        1
                    }
                }
                // Apply delta to x if `do_dx` is true.
                MaybeDxDy(do_dx) => {
                    self.y += self.stack.get_fixed(self.stack_ix)?;
                    if do_dx {
                        self.x += self.stack.get_fixed(self.stack_ix + 1)?;
                        2
                    } else {
                        1
                    }
                }
            };
            self.stack_ix += stack_used;
            if count == 2 {
                self.sink.curve_to(
                    points[0].x,
                    points[0].y,
                    points[1].x,
                    points[1].y,
                    self.x,
                    self.y,
                );
                count = 0;
            } else {
                points[count] = Point::new(self.x, self.y);
                count += 1;
            }
        }
        Ok(())
    }

    fn reset_stack(&mut self) {
        self.stack.clear();
        self.stack_ix = 0;
    }
}

/// Specifies how point coordinates for a curve are computed.
#[derive(Copy, Clone)]
enum PointMode {
    DxDy,
    XDy,
    DxY,
    DxInitialY,
    DLargerCoordDist,
    DxMaybeDy(bool),
    MaybeDxDy(bool),
}

/// PostScript charstring operator.
///
/// See <https://learn.microsoft.com/en-us/typography/opentype/spec/cff2charstr#appendix-a-cff2-charstring-command-codes>
// TODO: This is currently missing legacy math and logical operators.
// fonttools doesn't even implement these: <https://github.com/fonttools/fonttools/blob/65598197c8afd415781f6667a7fb647c2c987fff/Lib/fontTools/misc/psCharStrings.py#L409>
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
enum Operator {
    HStem,
    VStem,
    VMoveTo,
    RLineTo,
    HLineTo,
    VLineTo,
    RrCurveTo,
    CallSubr,
    Return,
    Hsbw,
    EndChar,
    VariationStoreIndex,
    Blend,
    HStemHm,
    HintMask,
    CntrMask,
    RMoveTo,
    HMoveTo,
    VStemHm,
    RCurveLine,
    RLineCurve,
    VvCurveTo,
    HhCurveTo,
    CallGsubr,
    VhCurveTo,
    HvCurveTo,
    DotSection,
    VStem3,
    HStem3,
    Seac,
    Sbw,
    Div,
    CallOtherSubr,
    Pop,
    SetCurrentPoint,
    HFlex,
    Flex,
    HFlex1,
    Flex1,
}

impl Operator {
    fn read(cursor: &mut Cursor, b0: u8) -> Result<Self, Error> {
        // Escape opcode for accessing two byte operators
        const ESCAPE: u8 = 12;
        let (opcode, operator) = if b0 == ESCAPE {
            let b1 = cursor.read::<u8>()?;
            (b1, Self::from_two_byte_opcode(b1))
        } else {
            (b0, Self::from_opcode(b0))
        };
        operator.ok_or(Error::InvalidCharstringOperator(opcode))
    }

    /// Creates an operator from the given opcode.
    fn from_opcode(opcode: u8) -> Option<Self> {
        use Operator::*;
        Some(match opcode {
            1 => HStem,
            3 => VStem,
            4 => VMoveTo,
            5 => RLineTo,
            6 => HLineTo,
            7 => VLineTo,
            8 => RrCurveTo,
            10 => CallSubr,
            11 => Return,
            13 => Hsbw,
            14 => EndChar,
            15 => VariationStoreIndex,
            16 => Blend,
            18 => HStemHm,
            19 => HintMask,
            20 => CntrMask,
            21 => RMoveTo,
            22 => HMoveTo,
            23 => VStemHm,
            24 => RCurveLine,
            25 => RLineCurve,
            26 => VvCurveTo,
            27 => HhCurveTo,
            29 => CallGsubr,
            30 => VhCurveTo,
            31 => HvCurveTo,
            _ => return None,
        })
    }

    /// Creates an operator from the given extended opcode.
    ///
    /// These are preceded by a byte containing the escape value of 12.
    pub fn from_two_byte_opcode(opcode: u8) -> Option<Self> {
        use Operator::*;
        Some(match opcode {
            0 => DotSection,
            1 => VStem3,
            2 => HStem3,
            6 => Seac,
            7 => Sbw,
            12 => Div,
            16 => CallOtherSubr,
            17 => Pop,
            33 => SetCurrentPoint,
            34 => HFlex,
            35 => Flex,
            36 => HFlex1,
            37 => Flex1,
            _ => return None,
        })
    }
}

// Used for scaling sink below
const ONE_OVER_64: Fixed = Fixed::from_bits(0x400);

/// Command sink adapter that applies a matrix and optional scale.
pub struct TransformSink<'a, S> {
    inner: &'a mut S,
    matrix: Option<FontMatrix>,
    scale: Option<Fixed>,
}

impl<'a, S> TransformSink<'a, S> {
    /// Creates a new sink for the given matrix and optional scale.
    pub fn new(sink: &'a mut S, matrix: FontMatrix, scale: Option<Fixed>) -> Self {
        Self {
            inner: sink,
            matrix: (matrix != FontMatrix::IDENTITY).then_some(matrix),
            scale,
        }
    }

    fn transform(&self, x: Fixed, y: Fixed) -> (Fixed, Fixed) {
        // The following dance is necessary to exactly match FreeType's
        // application of scaling factors. This seems to be the result
        // of merging the contributed Adobe code while not breaking the
        // FreeType public API.
        //
        // The first two steps apply to both scaled and unscaled outlines:
        //
        // 1. Multiply by 1/64
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psft.c#L284>
        let ax = x * ONE_OVER_64;
        let ay = y * ONE_OVER_64;
        // 2. Truncate the bottom 10 bits. Combined with the division by 64,
        // converts to font units.
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psobjs.c#L2219>
        let bx = Fixed::from_bits(ax.to_bits() >> 10);
        let by = Fixed::from_bits(ay.to_bits() >> 10);
        // 3. Apply the transform. It must be done here to match FreeType.
        let (cx, cy) = self
            .matrix
            .as_ref()
            .map(|mat| mat.transform(bx, by))
            .unwrap_or((bx, by));
        if let Some(scale) = self.scale {
            // Scaled case:
            // 4. Multiply by the original scale factor (to 26.6)
            // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/cff/cffgload.c#L721>
            let dx = cx * scale;
            let dy = cy * scale;
            // 5. Convert from 26.6 to 16.16
            (
                Fixed::from_bits(dx.to_bits() << 10),
                Fixed::from_bits(dy.to_bits() << 10),
            )
        } else {
            // Unscaled case:
            // 4. Convert from integer to 16.16
            (
                Fixed::from_bits(cx.to_bits() << 16),
                Fixed::from_bits(cy.to_bits() << 16),
            )
        }
    }
}

impl<S: CommandSink> CommandSink for TransformSink<'_, S> {
    fn hstem(&mut self, y: Fixed, dy: Fixed) {
        self.inner.hstem(y, dy);
    }

    fn vstem(&mut self, x: Fixed, dx: Fixed) {
        self.inner.vstem(x, dx);
    }

    fn hint_mask(&mut self, mask: &[u8]) {
        self.inner.hint_mask(mask);
    }

    fn counter_mask(&mut self, mask: &[u8]) {
        self.inner.counter_mask(mask);
    }

    fn clear_hints(&mut self) {
        self.inner.clear_hints();
    }

    fn move_to(&mut self, x: Fixed, y: Fixed) {
        let (x, y) = self.transform(x, y);
        self.inner.move_to(x, y);
    }

    fn line_to(&mut self, x: Fixed, y: Fixed) {
        let (x, y) = self.transform(x, y);
        self.inner.line_to(x, y);
    }

    fn curve_to(&mut self, cx1: Fixed, cy1: Fixed, cx2: Fixed, cy2: Fixed, x: Fixed, y: Fixed) {
        let (cx1, cy1) = self.transform(cx1, cy1);
        let (cx2, cy2) = self.transform(cx2, cy2);
        let (x, y) = self.transform(x, y);
        self.inner.curve_to(cx1, cy1, cx2, cy2, x, y);
    }

    fn close(&mut self) {
        self.inner.close();
    }

    fn finish(&mut self) {
        self.inner.finish();
    }
}

#[derive(Copy, Clone)]
enum PendingElement {
    Move([Fixed; 2]),
    Line([Fixed; 2]),
    Curve([Fixed; 6]),
}

impl PendingElement {
    fn target_point(&self) -> [Fixed; 2] {
        match self {
            Self::Move(xy) | Self::Line(xy) => *xy,
            Self::Curve([.., x, y]) => [*x, *y],
        }
    }
}

/// Command sink adapter that suppresses degenerate move and line commands.
///
/// FreeType avoids emitting empty contours and zero length lines to prevent
/// artifacts when stem darkening is enabled. We don't support stem darkening
/// because it's not enabled by any of our clients but we remove the degenerate
/// elements regardless to match the output.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/pshints.c#L1786>
pub struct NopFilterSink<'a, S> {
    is_open: bool,
    start: Option<(Fixed, Fixed)>,
    pending_element: Option<PendingElement>,
    inner: &'a mut S,
}

impl<'a, S> NopFilterSink<'a, S>
where
    S: CommandSink,
{
    /// Creates a new sink that suppresses degenerate move and line commands
    /// before forwarding the result to the given inner sink.
    pub fn new(inner: &'a mut S) -> Self {
        Self {
            is_open: false,
            start: None,
            pending_element: None,
            inner,
        }
    }

    fn flush_pending(&mut self, for_close: bool) {
        if let Some(pending) = self.pending_element.take() {
            match pending {
                PendingElement::Move([x, y]) => {
                    if !for_close {
                        self.is_open = true;
                        self.inner.move_to(x, y);
                        self.start = Some((x, y));
                    }
                }
                PendingElement::Line([x, y]) => {
                    if !for_close || self.start != Some((x, y)) {
                        self.inner.line_to(x, y);
                    }
                }
                PendingElement::Curve([cx0, cy0, cx1, cy1, x, y]) => {
                    self.inner.curve_to(cx0, cy0, cx1, cy1, x, y);
                }
            }
        }
    }
}

impl<S> CommandSink for NopFilterSink<'_, S>
where
    S: CommandSink,
{
    fn hstem(&mut self, y: Fixed, dy: Fixed) {
        self.inner.hstem(y, dy);
    }

    fn vstem(&mut self, x: Fixed, dx: Fixed) {
        self.inner.vstem(x, dx);
    }

    fn hint_mask(&mut self, mask: &[u8]) {
        self.inner.hint_mask(mask);
    }

    fn counter_mask(&mut self, mask: &[u8]) {
        self.inner.counter_mask(mask);
    }

    fn clear_hints(&mut self) {
        self.inner.clear_hints();
    }

    fn move_to(&mut self, x: Fixed, y: Fixed) {
        self.pending_element = Some(PendingElement::Move([x, y]));
    }

    fn line_to(&mut self, x: Fixed, y: Fixed) {
        // Omit the line if we're already at the given position
        if self
            .pending_element
            .map(|element| element.target_point() == [x, y])
            .unwrap_or_default()
        {
            return;
        }
        self.flush_pending(false);
        self.pending_element = Some(PendingElement::Line([x, y]));
    }

    fn curve_to(&mut self, cx1: Fixed, cy1: Fixed, cx2: Fixed, cy2: Fixed, x: Fixed, y: Fixed) {
        self.flush_pending(false);
        self.pending_element = Some(PendingElement::Curve([cx1, cy1, cx2, cy2, x, y]));
    }

    fn close(&mut self) {
        self.flush_pending(true);
        if self.is_open {
            self.inner.close();
            self.is_open = false;
        }
    }

    fn finish(&mut self) {
        self.close();
        self.inner.finish();
    }
}

#[cfg(test)]
pub(super) mod test_helpers {
    use super::{CommandSink, Fixed};

    #[derive(Copy, Clone, PartialEq, Debug)]
    #[allow(clippy::enum_variant_names)]
    pub enum Command {
        MoveTo(Fixed, Fixed),
        LineTo(Fixed, Fixed),
        CurveTo(Fixed, Fixed, Fixed, Fixed, Fixed, Fixed),
    }

    #[derive(PartialEq, Default, Debug)]
    pub struct CaptureCommandSink(pub Vec<Command>);

    impl CommandSink for CaptureCommandSink {
        fn move_to(&mut self, x: Fixed, y: Fixed) {
            self.0.push(Command::MoveTo(x, y))
        }

        fn line_to(&mut self, x: Fixed, y: Fixed) {
            self.0.push(Command::LineTo(x, y))
        }

        fn curve_to(&mut self, cx0: Fixed, cy0: Fixed, cx1: Fixed, cy1: Fixed, x: Fixed, y: Fixed) {
            self.0.push(Command::CurveTo(cx0, cy0, cx1, cy1, x, y))
        }

        fn close(&mut self) {
            // For testing purposes, replace the close command
            // with a line to the most recent move or (0, 0)
            // if none exists
            let mut last_move = [Fixed::ZERO; 2];
            for command in self.0.iter().rev() {
                if let Command::MoveTo(x, y) = command {
                    last_move = [*x, *y];
                    break;
                }
            }
            self.0.push(Command::LineTo(last_move[0], last_move[1]));
        }
    }

    impl CaptureCommandSink {
        pub fn to_svg(&self) -> String {
            use core::fmt::Write;
            let mut buf = String::default();
            for cmd in &self.0 {
                if !buf.is_empty() {
                    buf.push(' ');
                }
                match cmd {
                    Command::MoveTo(x, y) => write!(buf, "M{},{}", x.to_f32(), y.to_f32()).unwrap(),
                    Command::LineTo(x, y) => write!(buf, "L{},{}", x.to_f32(), y.to_f32()).unwrap(),
                    Command::CurveTo(x0, y0, x1, y1, x, y) => write!(
                        buf,
                        "C{},{} {},{} {},{}",
                        x0.to_f32(),
                        y0.to_f32(),
                        x1.to_f32(),
                        y1.to_f32(),
                        x.to_f32(),
                        y.to_f32()
                    )
                    .unwrap(),
                }
            }
            buf
        }
    }

    #[derive(Default)]
    pub struct CharstringCommandCounter(pub usize);

    impl CommandSink for CharstringCommandCounter {
        fn move_to(&mut self, _x: Fixed, _y: Fixed) {
            self.0 += 1;
        }

        fn line_to(&mut self, _x: Fixed, _y: Fixed) {
            self.0 += 1;
        }

        fn curve_to(
            &mut self,
            _cx0: Fixed,
            _cy0: Fixed,
            _cx1: Fixed,
            _cy1: Fixed,
            _x: Fixed,
            _y: Fixed,
        ) {
            self.0 += 1;
        }

        fn close(&mut self) {
            self.0 += 1;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{test_helpers::*, *};
    use crate::{tables::variations::ItemVariationStore, types::F2Dot14, FontData, FontRead};

    #[test]
    fn cff2_example_subr() {
        use Command::*;
        let charstring = &font_test_data::cff2::EXAMPLE[0xc8..=0xe1];
        let store =
            ItemVariationStore::read(FontData::new(&font_test_data::cff2::EXAMPLE[18..])).unwrap();
        let coords = &[F2Dot14::from_f32(0.0)];
        let blend_state = BlendState::new(store, coords, 0).unwrap();
        let mut commands = CaptureCommandSink::default();
        evaluate(
            &NullContext(CharstringKind::Type2),
            Some(blend_state),
            charstring,
            &mut commands,
        )
        .unwrap();
        // 50 50 100 1 blend 0 rmoveto
        // 500 -100 -200 1 blend hlineto
        // 500 vlineto
        // -500 100 200 1 blend hlineto
        //
        // applying blends at default location results in:
        // 50 0 rmoveto
        // 500 hlineto
        // 500 vlineto
        // -500 hlineto
        //
        // applying relative operators:
        // 50 0 moveto
        // 550 0 lineto
        // 550 500 lineto
        // 50 500 lineto
        let expected = &[
            MoveTo(Fixed::from_f64(50.0), Fixed::ZERO),
            LineTo(Fixed::from_f64(550.0), Fixed::ZERO),
            LineTo(Fixed::from_f64(550.0), Fixed::from_f64(500.0)),
            LineTo(Fixed::from_f64(50.0), Fixed::from_f64(500.0)),
        ];
        assert_eq!(&commands.0, expected);
    }

    #[test]
    fn all_path_ops() {
        // This charstring was manually constructed in
        // font-test-data/test_data/ttx/charstring_path_ops.ttx
        //
        // The encoded version was extracted from the font and inlined below
        // for simplicity.
        //
        // The geometry is arbitrary but includes the full set of path
        // construction operators:
        // --------------------------------------------------------------------
        // -137 -632 rmoveto
        // 34 -5 20 -6 rlineto
        // 1 2 3 hlineto
        // -179 -10 3 vlineto
        // -30 15 22 8 -50 26 -14 -42 -41 19 -15 25 rrcurveto
        // -30 15 22 8 hhcurveto
        // 8 -30 15 22 8 hhcurveto
        // 24 20 15 41 42 -20 14 -24 -25 -19 -14 -42 -41 19 -15 25 hvcurveto
        // 20 vmoveto
        // -20 14 -24 -25 -19 -14 4 5 rcurveline
        // -20 14 -24 -25 -19 -14 4 5 rlinecurve
        // -55 -23 -22 -59 vhcurveto
        // -30 15 22 8 vvcurveto
        // 8 -30 15 22 8 vvcurveto
        // 24 20 15 41 42 -20 14 -24 -25 -19 -14 -42 23 flex
        // 24 20 15 41 42 -20 14 hflex
        // 13 hmoveto
        // 41 42 -20 14 -24 -25 -19 -14 -42 hflex1
        // 15 41 42 -20 14 -24 -25 -19 -14 -42 8 flex1
        // endchar
        let charstring = &[
            251, 29, 253, 12, 21, 173, 134, 159, 133, 5, 140, 141, 142, 6, 251, 71, 129, 142, 7,
            109, 154, 161, 147, 89, 165, 125, 97, 98, 158, 124, 164, 8, 109, 154, 161, 147, 27,
            147, 109, 154, 161, 147, 27, 163, 159, 154, 180, 181, 119, 153, 115, 114, 120, 125, 97,
            98, 158, 124, 164, 31, 159, 4, 119, 153, 115, 114, 120, 125, 143, 144, 24, 119, 153,
            115, 114, 120, 125, 143, 144, 25, 84, 116, 117, 80, 30, 109, 154, 161, 147, 26, 147,
            109, 154, 161, 147, 26, 163, 159, 154, 180, 181, 119, 153, 115, 114, 120, 125, 97, 162,
            12, 35, 163, 159, 154, 180, 181, 119, 153, 12, 34, 152, 22, 180, 181, 119, 153, 115,
            114, 120, 125, 97, 12, 36, 154, 180, 181, 119, 153, 115, 114, 120, 125, 97, 147, 12,
            37, 14,
        ];
        use Command::*;
        let mut commands = CaptureCommandSink::default();
        evaluate(
            &NullContext(CharstringKind::Type2),
            None,
            charstring,
            &mut commands,
        )
        .unwrap();
        // Expected results from extracted glyph data in
        // font-test-data/test_data/extracted/charstring_path_ops-glyphs.txt
        // --------------------------------------------------------------------
        // m  -137,-632
        // l  -103,-637
        // l  -83,-643
        // l  -82,-643
        // l  -82,-641
        // l  -79,-641
        // l  -79,-820
        // l  -89,-820
        // l  -89,-817
        // c  -119,-802 -97,-794 -147,-768
        // c  -161,-810 -202,-791 -217,-766
        // c  -247,-766 -232,-744 -224,-744
        // c  -254,-736 -239,-714 -231,-714
        // c  -207,-714 -187,-699 -187,-658
        // c  -187,-616 -207,-602 -231,-602
        // c  -256,-602 -275,-616 -275,-658
        // c  -275,-699 -256,-714 -231,-714
        // l  -137,-632
        // m  -231,-694
        // c  -251,-680 -275,-705 -294,-719
        // l  -290,-714
        // l  -310,-700
        // c  -334,-725 -353,-739 -349,-734
        // c  -349,-789 -372,-811 -431,-811
        // c  -431,-841 -416,-819 -416,-811
        // c  -408,-841 -393,-819 -393,-811
        // c  -369,-791 -354,-750 -312,-770
        // c  -298,-794 -323,-813 -337,-855
        // c  -313,-855 -293,-840 -252,-840
        // c  -210,-840 -230,-855 -216,-855
        // l  -231,-694
        // m  -203,-855
        // c  -162,-813 -182,-799 -206,-799
        // c  -231,-799 -250,-813 -292,-855
        // c  -277,-814 -235,-834 -221,-858
        // c  -246,-877 -260,-919 -292,-911
        // l  -203,-855
        let expected = &[
            MoveTo(Fixed::from_i32(-137), Fixed::from_i32(-632)),
            LineTo(Fixed::from_i32(-103), Fixed::from_i32(-637)),
            LineTo(Fixed::from_i32(-83), Fixed::from_i32(-643)),
            LineTo(Fixed::from_i32(-82), Fixed::from_i32(-643)),
            LineTo(Fixed::from_i32(-82), Fixed::from_i32(-641)),
            LineTo(Fixed::from_i32(-79), Fixed::from_i32(-641)),
            LineTo(Fixed::from_i32(-79), Fixed::from_i32(-820)),
            LineTo(Fixed::from_i32(-89), Fixed::from_i32(-820)),
            LineTo(Fixed::from_i32(-89), Fixed::from_i32(-817)),
            CurveTo(
                Fixed::from_i32(-119),
                Fixed::from_i32(-802),
                Fixed::from_i32(-97),
                Fixed::from_i32(-794),
                Fixed::from_i32(-147),
                Fixed::from_i32(-768),
            ),
            CurveTo(
                Fixed::from_i32(-161),
                Fixed::from_i32(-810),
                Fixed::from_i32(-202),
                Fixed::from_i32(-791),
                Fixed::from_i32(-217),
                Fixed::from_i32(-766),
            ),
            CurveTo(
                Fixed::from_i32(-247),
                Fixed::from_i32(-766),
                Fixed::from_i32(-232),
                Fixed::from_i32(-744),
                Fixed::from_i32(-224),
                Fixed::from_i32(-744),
            ),
            CurveTo(
                Fixed::from_i32(-254),
                Fixed::from_i32(-736),
                Fixed::from_i32(-239),
                Fixed::from_i32(-714),
                Fixed::from_i32(-231),
                Fixed::from_i32(-714),
            ),
            CurveTo(
                Fixed::from_i32(-207),
                Fixed::from_i32(-714),
                Fixed::from_i32(-187),
                Fixed::from_i32(-699),
                Fixed::from_i32(-187),
                Fixed::from_i32(-658),
            ),
            CurveTo(
                Fixed::from_i32(-187),
                Fixed::from_i32(-616),
                Fixed::from_i32(-207),
                Fixed::from_i32(-602),
                Fixed::from_i32(-231),
                Fixed::from_i32(-602),
            ),
            CurveTo(
                Fixed::from_i32(-256),
                Fixed::from_i32(-602),
                Fixed::from_i32(-275),
                Fixed::from_i32(-616),
                Fixed::from_i32(-275),
                Fixed::from_i32(-658),
            ),
            CurveTo(
                Fixed::from_i32(-275),
                Fixed::from_i32(-699),
                Fixed::from_i32(-256),
                Fixed::from_i32(-714),
                Fixed::from_i32(-231),
                Fixed::from_i32(-714),
            ),
            LineTo(Fixed::from_i32(-137), Fixed::from_i32(-632)),
            MoveTo(Fixed::from_i32(-231), Fixed::from_i32(-694)),
            CurveTo(
                Fixed::from_i32(-251),
                Fixed::from_i32(-680),
                Fixed::from_i32(-275),
                Fixed::from_i32(-705),
                Fixed::from_i32(-294),
                Fixed::from_i32(-719),
            ),
            LineTo(Fixed::from_i32(-290), Fixed::from_i32(-714)),
            LineTo(Fixed::from_i32(-310), Fixed::from_i32(-700)),
            CurveTo(
                Fixed::from_i32(-334),
                Fixed::from_i32(-725),
                Fixed::from_i32(-353),
                Fixed::from_i32(-739),
                Fixed::from_i32(-349),
                Fixed::from_i32(-734),
            ),
            CurveTo(
                Fixed::from_i32(-349),
                Fixed::from_i32(-789),
                Fixed::from_i32(-372),
                Fixed::from_i32(-811),
                Fixed::from_i32(-431),
                Fixed::from_i32(-811),
            ),
            CurveTo(
                Fixed::from_i32(-431),
                Fixed::from_i32(-841),
                Fixed::from_i32(-416),
                Fixed::from_i32(-819),
                Fixed::from_i32(-416),
                Fixed::from_i32(-811),
            ),
            CurveTo(
                Fixed::from_i32(-408),
                Fixed::from_i32(-841),
                Fixed::from_i32(-393),
                Fixed::from_i32(-819),
                Fixed::from_i32(-393),
                Fixed::from_i32(-811),
            ),
            CurveTo(
                Fixed::from_i32(-369),
                Fixed::from_i32(-791),
                Fixed::from_i32(-354),
                Fixed::from_i32(-750),
                Fixed::from_i32(-312),
                Fixed::from_i32(-770),
            ),
            CurveTo(
                Fixed::from_i32(-298),
                Fixed::from_i32(-794),
                Fixed::from_i32(-323),
                Fixed::from_i32(-813),
                Fixed::from_i32(-337),
                Fixed::from_i32(-855),
            ),
            CurveTo(
                Fixed::from_i32(-313),
                Fixed::from_i32(-855),
                Fixed::from_i32(-293),
                Fixed::from_i32(-840),
                Fixed::from_i32(-252),
                Fixed::from_i32(-840),
            ),
            CurveTo(
                Fixed::from_i32(-210),
                Fixed::from_i32(-840),
                Fixed::from_i32(-230),
                Fixed::from_i32(-855),
                Fixed::from_i32(-216),
                Fixed::from_i32(-855),
            ),
            LineTo(Fixed::from_i32(-231), Fixed::from_i32(-694)),
            MoveTo(Fixed::from_i32(-203), Fixed::from_i32(-855)),
            CurveTo(
                Fixed::from_i32(-162),
                Fixed::from_i32(-813),
                Fixed::from_i32(-182),
                Fixed::from_i32(-799),
                Fixed::from_i32(-206),
                Fixed::from_i32(-799),
            ),
            CurveTo(
                Fixed::from_i32(-231),
                Fixed::from_i32(-799),
                Fixed::from_i32(-250),
                Fixed::from_i32(-813),
                Fixed::from_i32(-292),
                Fixed::from_i32(-855),
            ),
            CurveTo(
                Fixed::from_i32(-277),
                Fixed::from_i32(-814),
                Fixed::from_i32(-235),
                Fixed::from_i32(-834),
                Fixed::from_i32(-221),
                Fixed::from_i32(-858),
            ),
            CurveTo(
                Fixed::from_i32(-246),
                Fixed::from_i32(-877),
                Fixed::from_i32(-260),
                Fixed::from_i32(-919),
                Fixed::from_i32(-292),
                Fixed::from_i32(-911),
            ),
            LineTo(Fixed::from_i32(-203), Fixed::from_i32(-855)),
        ];
        assert_eq!(&commands.0, expected);
    }

    /// Fuzzer caught subtract with overflow
    /// <https://g-issues.oss-fuzz.com/issues/383609770>
    #[test]
    fn coords_remaining_avoid_overflow() {
        // Test case:
        // Evaluate HHCURVETO operator with 2 elements on the stack
        let mut commands = CaptureCommandSink::default();
        let mut evaluator =
            Evaluator::new(&NullContext(CharstringKind::Type2), None, &mut commands);
        evaluator.stack.push(0).unwrap();
        evaluator.stack.push(0).unwrap();
        let mut cursor = FontData::new(&[]).cursor();
        // Just don't panic
        let _ = evaluator.evaluate_operator(Operator::HhCurveTo, &mut cursor, 0);
    }

    #[test]
    fn ignore_reserved_operators() {
        let charstring = &[
            0u8, // reserved
            32,  // push -107
            22,  // hmoveto
            2,   // reserved
        ];
        let mut commands = CaptureCommandSink::default();
        evaluate(
            &NullContext(CharstringKind::Type2),
            None,
            charstring,
            &mut commands,
        )
        .unwrap();
        assert_eq!(
            commands.0,
            [Command::MoveTo(Fixed::from_i32(-107), Fixed::ZERO)]
        );
    }

    #[test]
    fn op_div() {
        let mut commands = CaptureCommandSink::default();
        let mut eval = Evaluator::new(&NullContext(CharstringKind::Type2), None, &mut commands);
        let mut cursor = FontData::new(&[]).cursor();
        eval.stack.push(Fixed::from_f64(512.5)).unwrap();
        eval.stack.push(2).unwrap();
        eval.evaluate_operator(Operator::Div, &mut cursor, 0)
            .unwrap();
        assert_eq!(
            eval.stack.pop_fixed().unwrap(),
            Fixed::from_f64(512.5 / 2.0)
        );
    }

    #[test]
    fn op_div_type1_large_int() {
        let mut commands = CaptureCommandSink::default();
        let mut eval = Evaluator::new(&NullContext(CharstringKind::Type1), None, &mut commands);
        let mut cursor = FontData::new(&[]).cursor();
        // Greater than 32,000 which triggers "large int div" behavior for type1.
        eval.stack.push(32001).unwrap();
        eval.stack.push(2).unwrap();
        eval.evaluate_operator(Operator::Div, &mut cursor, 0)
            .unwrap();
        assert_eq!(
            eval.stack.pop_fixed().unwrap(),
            Fixed::from_f64(32001.0 / 2.0)
        );
    }

    /// Shared code for the (h)sbw tests.
    ///
    /// Returns [sbx, wx]
    fn eval_h_sbw(operator: Operator, kind: CharstringKind) -> [Fixed; 2] {
        let mut commands = CaptureCommandSink::default();
        let ctx = &NullContext(kind);
        let mut eval = Evaluator::new(ctx, None, &mut commands);
        let mut cursor = FontData::new(&[]).cursor();
        eval.stack.push(Fixed::from_f64(42.5)).unwrap();
        if operator == Operator::Sbw {
            // sbw includes y coords
            eval.stack.push(0).unwrap();
        }
        eval.stack.push(501).unwrap();
        eval.stack.push(1000).unwrap();
        eval.evaluate_operator(operator, &mut cursor, 0).unwrap();
        [eval.sbx, eval.wx]
    }

    #[test]
    fn op_sbw_type1() {
        let [sbx, wx] = eval_h_sbw(Operator::Sbw, CharstringKind::Type1);
        assert_eq!(sbx, Fixed::from_f64(42.5));
        assert_eq!(wx, Fixed::from_f64(501.0));
    }

    #[test]
    fn op_hsbw_type1() {
        let [sbx, wx] = eval_h_sbw(Operator::Hsbw, CharstringKind::Type1);
        assert_eq!(sbx, Fixed::from_f64(42.5));
        assert_eq!(wx, Fixed::from_f64(501.0));
    }

    /// sbw is ignored in type 2
    #[test]
    fn op_sbw_type2_no_effect() {
        let [sbx, wx] = eval_h_sbw(Operator::Sbw, CharstringKind::Type2);
        assert_eq!(sbx, Fixed::ZERO);
        assert_eq!(wx, Fixed::ZERO);
    }

    /// hsbw is ignored in type 2
    #[test]
    fn op_hsbw_type2_no_effect() {
        let [sbx, wx] = eval_h_sbw(Operator::Hsbw, CharstringKind::Type2);
        assert_eq!(sbx, Fixed::ZERO);
        assert_eq!(wx, Fixed::ZERO);
    }

    #[test]
    fn op_callothersubr_flex() {
        let mut commands = CaptureCommandSink::default();
        let mut eval = Evaluator::new(&NullContext(CharstringKind::Type1), None, &mut commands);
        let mut cursor = FontData::new(&[]).cursor();
        // push some numbers and optionally evaluate an operator
        macro_rules! op {
            ($nums:expr) => {
                for n in $nums {
                    eval.stack.push(n).unwrap();
                }
            };
            ($nums:expr, $op:ident) => {
                op!($nums);
                eval.evaluate_operator(Operator::$op, &mut cursor, 0)
                    .unwrap();
            };
        }
        // Emulate a flex vector call
        // begin flex
        op!([0, 1], CallOtherSubr);
        // emit flex vectors with a series of moves
        for vec in [[1, 2]; 7] {
            op!(vec, RMoveTo);
        }
        // flex_height, final_x, final_y
        op!([0, 100, 200]);
        // end flex
        op!([3, 0], CallOtherSubr);
        // flex usually ends with a subr call to setcurrentpoint
        // which makes use of the final coords pushed to the stack
        let none: [i32; 0] = [];
        op!(none, SetCurrentPoint);
        let expected = [
            Command::MoveTo(Fixed::ZERO, Fixed::ZERO),
            Command::CurveTo(
                Fixed::from_i32(2),
                Fixed::from_i32(4),
                Fixed::from_i32(3),
                Fixed::from_i32(6),
                Fixed::from_i32(4),
                Fixed::from_i32(8),
            ),
            Command::CurveTo(
                Fixed::from_i32(5),
                Fixed::from_i32(10),
                Fixed::from_i32(6),
                Fixed::from_i32(12),
                Fixed::from_i32(7),
                Fixed::from_i32(14),
            ),
        ];
        assert_eq!(eval.x, Fixed::from_i32(100));
        assert_eq!(eval.y, Fixed::from_i32(200));
        assert_eq!(commands.0, expected);
    }

    struct NullContext(CharstringKind);

    impl CharstringContext for NullContext {
        fn kind(&self) -> CharstringKind {
            self.0
        }

        fn seac_components(&self, base_code: i32, _accent_code: i32) -> Result<[&[u8]; 2], Error> {
            Err(Error::InvalidSeacCode(base_code))
        }

        fn global_subr(&self, _index: i32) -> Result<&[u8], Error> {
            Err(Error::MissingSubroutines)
        }

        fn subr(&self, _index: i32) -> Result<&[u8], Error> {
            Err(Error::MissingSubroutines)
        }
    }

    #[test]
    fn nop_filter_sink() {
        let mut commands = CaptureCommandSink::default();
        let mut nop_filter = NopFilterSink::new(&mut commands);
        let (sx, sy) = (Fixed::from_f64(10.2), Fixed::from_f64(20.4));
        // filtered
        nop_filter.move_to(Fixed::from_f64(0.0), Fixed::from_f64(0.0));
        nop_filter.move_to(sx, sy);
        // filtered
        nop_filter.line_to(sx, sy);
        nop_filter.curve_to(
            Fixed::from_f64(5.0),
            Fixed::from_f64(-5.0),
            Fixed::from_f64(1.5),
            Fixed::from_f64(2.0),
            Fixed::from_f64(4.5),
            Fixed::from_f64(-10.0),
        );
        // filtered
        nop_filter.line_to(Fixed::from_f64(4.5), Fixed::from_f64(-10.0));
        // filtered due to next close
        nop_filter.line_to(sx, sy);
        nop_filter.close();
        assert_eq!(
            commands.0,
            [
                Command::MoveTo(sx, sy),
                Command::CurveTo(
                    Fixed::from_f64(5.0),
                    Fixed::from_f64(-5.0),
                    Fixed::from_f64(1.5),
                    Fixed::from_f64(2.0),
                    Fixed::from_f64(4.5),
                    Fixed::from_f64(-10.0),
                ),
                Command::LineTo(sx, sy),
            ]
        )
    }

    #[test]
    fn scaled_matrix_transform_sink() {
        // A few points taken from the test font in <https://github.com/googlefonts/fontations/issues/1581>
        // Inputs and expected values extracted from FreeType
        let input = [(150i32, 46i32), (176, 8), (217, -13), (267, -13)]
            .map(|(x, y)| (Fixed::from_bits(x << 16), Fixed::from_bits(y << 16)));
        let expected = [(404, 118i32), (453, 20), (550, -33), (678, -33)]
            .map(|(x, y)| (Fixed::from_bits(x << 10), Fixed::from_bits(y << 10)));
        let mut dummy = ();
        let sink = TransformSink::new(&mut dummy, TRANSFORM, Some(Fixed::from_bits(167772)));
        let transformed = input.map(|(x, y)| sink.transform(x, y));
        assert_eq!(transformed, expected);
    }

    #[test]
    fn unscaled_matrix_transform_sink() {
        // A few points taken from the test font in <https://github.com/googlefonts/fontations/issues/1581>
        // Inputs and expected values extracted from FreeType
        let input = [(150i32, 46i32), (176, 8), (217, -13), (267, -13)]
            .map(|(x, y)| (Fixed::from_bits(x << 16), Fixed::from_bits(y << 16)));
        let expected = [(158, 46i32), (177, 8), (215, -13), (265, -13)]
            .map(|(x, y)| (Fixed::from_bits(x << 16), Fixed::from_bits(y << 16)));
        let mut dummy = ();
        let sink = TransformSink::new(&mut dummy, TRANSFORM, None);
        let transformed = input.map(|(x, y)| sink.transform(x, y));
        assert_eq!(transformed, expected);
    }

    const TRANSFORM: FontMatrix = FontMatrix([
        Fixed::ONE,
        Fixed::ZERO,
        // 0.167007446289062
        Fixed::from_bits(10945),
        Fixed::ONE,
        Fixed::ZERO,
        Fixed::ZERO,
    ]);

    #[test]
    fn unscaled_transform_sink_produces_integers() {
        let nothing = &mut ();
        let sink = TransformSink::new(nothing, FontMatrix::IDENTITY, None);
        for coord in [50.0, 50.1, 50.125, 50.5, 50.9] {
            assert_eq!(
                sink.transform(Fixed::from_f64(coord), Fixed::ZERO)
                    .0
                    .to_f32(),
                50.0
            );
        }
    }

    #[test]
    fn scaled_transform_sink() {
        let ppem = 20.0;
        let upem = 1000.0;
        // match FreeType scaling with intermediate conversion to 26.6
        let scale = Fixed::from_bits((ppem * 64.) as i32) / Fixed::from_bits(upem as i32);
        let nothing = &mut ();
        let sink = TransformSink::new(nothing, FontMatrix::IDENTITY, Some(scale));
        let inputs = [
            // input coord, expected scaled output
            (0.0, 0.0),
            (8.0, 0.15625),
            (16.0, 0.3125),
            (32.0, 0.640625),
            (72.0, 1.4375),
            (128.0, 2.5625),
        ];
        for (coord, expected) in inputs {
            assert_eq!(
                sink.transform(Fixed::from_f64(coord), Fixed::ZERO)
                    .0
                    .to_f32(),
                expected,
                "scaling coord {coord}"
            );
        }
    }

    #[test]
    fn mm_blend() {
        let mut commands = CaptureCommandSink::default();
        let ctx = MmContext([0.0, -0.25, 1.0].map(Fixed::from_f64));
        let mut eval = Evaluator::new(&ctx, None, &mut commands);
        let mut cursor = FontData::new(&[]).cursor();
        // First two values are base coords. Next four are deltas
        // for those coords (two each). Last two values are arg
        // count and othersubr number.
        for i in [[1, 0], [2, 3], [4, -8], [6, 15]].into_iter().flatten() {
            eval.stack.push(i).unwrap();
        }
        eval.evaluate_operator(Operator::CallOtherSubr, &mut cursor, 0)
            .unwrap();
        let [a, b] = eval.stack.fixed_array(0).unwrap();
        // a = 1 + -0.25*2 + 1*3 = 3.5
        assert_eq!(a.to_f32(), 3.5);
        // b = 0 + -0.25*4 + 1*-8 = -9.0
        assert_eq!(b.to_f32(), -9.0);
    }

    struct MmContext([Fixed; 3]);

    impl CharstringContext for MmContext {
        fn kind(&self) -> CharstringKind {
            CharstringKind::Type1
        }

        fn seac_components(&self, base_code: i32, _accent_code: i32) -> Result<[&[u8]; 2], Error> {
            Err(Error::InvalidSeacCode(base_code))
        }

        fn global_subr(&self, _index: i32) -> Result<&[u8], Error> {
            Err(Error::MissingSubroutines)
        }

        fn subr(&self, _index: i32) -> Result<&[u8], Error> {
            Err(Error::MissingSubroutines)
        }

        fn weight_vector(&self) -> &[Fixed] {
            &self.0
        }
    }
}
