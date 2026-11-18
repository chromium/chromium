//! TrueType hinting for a [`ScalerFont`](super::ScalerFont).
//!
//! read-fonts loads the outline, and this crate's interpreter hints it
//! through the loader's [`Hinter`] hook.

use super::{DrawnMetrics, HintOptions, ScaleError};
use crate::{
    font::Font,
    outline::glyf::{
        ClearType, ClearTypeState, DropoutControl, HintError, HintInstance, HintOutline,
        HintTables, RasterPolicy,
    },
};
use alloc::sync::Arc;
use core::{
    mem::{align_of, size_of},
    ops::Range,
};
use raw::model::Error;
use raw::{
    mem::{take_slice, with_scratch},
    model::{
        _font_interop,
        glyph::outline::{glyf::GlyfGlyph, PathContourStart, PathSink},
        Tables,
    },
    tables::{
        glyf::{
            outline::{Buffers, GlyphZone, Hinter, Scale26Dot6},
            PointFlags, PHANTOM_POINT_COUNT,
        },
        head::Flags as HeadFlags,
    },
    types::{F26Dot6, Fixed, GlyphId, Point},
    TableProvider,
};
use yoke::Yoke;

/// The interpreter, configured for one font at one size.
#[derive(Default)]
pub(super) struct TrueTypeHinter {
    instance: HintInstance,
    /// The tables the interpreter reads, parsed once per font and yoked to
    /// the tables they borrow.
    tables: Option<Yoke<HintTables<'static>, Arc<Tables>>>,
    scale: Option<Scale26Dot6>,
    /// The interpreter's size, rounded for a font that requests whole pixel
    /// sizes.
    ppem: f32,
    pedantic: bool,
    /// The byte range of the widths in the `hdmx` record for the size.
    hdmx_widths: Option<Range<usize>>,
    scratch_lengths: ScratchLengths,
}

/// The working memory a glyph program needs beyond the outline.
#[derive(Copy, Clone, Default)]
struct ScratchLengths {
    stack: usize,
    cvt: usize,
    storage: usize,
    twilight: usize,
}

impl ScratchLengths {
    /// Returns the bytes a block needs for [`carve`](Self::carve) to
    /// succeed with an outline of `points` points.
    fn packed_len(&self, points: usize) -> usize {
        let point = size_of::<Point<F26Dot6>>();
        points * point
            + (self.stack + self.cvt + self.storage) * size_of::<i32>()
            + self.twilight * (2 * point + size_of::<PointFlags>())
            // Seven slices are carved out, and each may need aligning.
            + 7 * align_of::<Point<F26Dot6>>()
    }

    /// Splits `buf` into the slices a glyph program uses.
    ///
    /// Returns `None` if `buf` is shorter than
    /// [`packed_len`](Self::packed_len).
    fn carve<'a>(&self, buf: &'a mut [u8], points: usize) -> Option<HintScratch<'a>> {
        let (original_scaled, rest) = take_slice::<Point<F26Dot6>>(buf, points)?;
        let (twilight_scaled, rest) = take_slice::<Point<F26Dot6>>(rest, self.twilight)?;
        let (twilight_original_scaled, rest) = take_slice::<Point<F26Dot6>>(rest, self.twilight)?;
        let (stack, rest) = take_slice::<i32>(rest, self.stack)?;
        let (cvt, rest) = take_slice::<i32>(rest, self.cvt)?;
        let (storage, rest) = take_slice::<i32>(rest, self.storage)?;
        let (twilight_flags, _) = take_slice::<PointFlags>(rest, self.twilight)?;
        Some(HintScratch {
            original_scaled,
            stack,
            cvt,
            storage,
            twilight_scaled,
            twilight_original_scaled,
            twilight_flags,
        })
    }
}

/// The working memory for a glyph program, carved from the caller's block.
///
/// Everything a glyph program may write lives here rather than in the
/// hinter, so drawing leaves the hinter unchanged.
struct HintScratch<'a> {
    /// The scaled points before hinting moved them.
    original_scaled: &'a mut [Point<F26Dot6>],
    stack: &'a mut [i32],
    cvt: &'a mut [i32],
    storage: &'a mut [i32],
    twilight_scaled: &'a mut [Point<F26Dot6>],
    twilight_original_scaled: &'a mut [Point<F26Dot6>],
    twilight_flags: &'a mut [PointFlags],
}

impl TrueTypeHinter {
    /// Runs the font and control value programs for `font` at `ppem`.
    pub(super) fn reconfigure(
        &mut self,
        font: &Font,
        ppem: f32,
        hinting: &HintOptions,
    ) -> Result<(), ScaleError> {
        // The font may be another one than last time. Finding its programs
        // only borrows slices of tables the font already holds.
        let host = _font_interop::_table_cache_host(font).ok_or(Error::MissingResource)?;
        let hint_tables = self
            .tables
            .insert(Yoke::attach_to_cart(host, |tables| hint_tables(tables)))
            .get();
        let tables = font.tables();
        let units_per_em = font.units_per_em();
        // Bit 3 of head.flags asks for whole pixel sizes, in which case the
        // size is rounded before hinting, as FreeType does.
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/truetype/ttobjs.c#L1424>
        let fractional_sizes = !tables
            .head()
            .map(|head| head.flags().contains(HeadFlags::FORCE_INTEGER_PPEM))
            .unwrap_or_default();
        let requested_ppem = ppem;
        let ppem = if fractional_sizes {
            ppem
        } else {
            F26Dot6::from_f64(ppem as f64).round().to_f32()
        };
        let scale = Scale26Dot6::new(Some(ppem), units_per_em);
        let scale_bits = scale.to_bits();
        // The size the MPPEM instruction reports is rounded in fixed point,
        // as FreeType rounds it.
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/base/ftobjs.c#L3349>
        let rounded_ppem =
            ((Fixed::from_bits(scale_bits) * Fixed::from_bits(units_per_em as i32)).to_bits() + 32)
                >> 6;
        let policy = RasterPolicy::resolve(&tables, rounded_ppem.clamp(0, u16::MAX as i32) as u16);
        let mode = if hinting.classic_cleartype() {
            ClearType::Classic
        } else {
            ClearType::Enhanced
        };
        let cleartype = ClearTypeState::new(mode, font.is_hint_reliant(), policy);
        self.instance.reconfigure(
            hint_tables,
            scale_bits,
            rounded_ppem,
            hinting.outline_target(),
            font.normalized_coords(),
            cleartype,
        )?;
        self.scale = Some(scale);
        self.ppem = ppem;
        self.pedantic = hinting.pedantic();
        self.hdmx_widths = if hinting.classic_cleartype() {
            // The classic behavior takes advances from `hdmx` only at a
            // requested size that is a whole number of pixels, as
            // `HintingInstance` does, for a fixed pitch font too.
            let size = requested_ppem as u8;
            (size as f32 == requested_ppem)
                .then(|| hdmx_widths(tables, size as i32))
                .flatten()
        } else {
            // FreeType never takes advances from `hdmx` for a fixed pitch
            // font.
            // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/a5877f48fff8dd6cf8d53c505ce055499376aee2/src/truetype/ttgload.c#L2324>
            let fixed_pitch = tables.post().is_ok_and(|post| post.is_fixed_pitch() != 0);
            (!fixed_pitch)
                .then(|| hdmx_widths(tables, rounded_ppem))
                .flatten()
        };
        self.scratch_lengths = ScratchLengths {
            stack: hint_tables.max_stack_elements as usize,
            cvt: hint_tables.cvt.len(),
            storage: hint_tables.max_storage as usize,
            twilight: hint_tables.max_twilight_points as usize,
        };
        Ok(())
    }

    /// Returns true if glyphs are hinted at this size.
    ///
    /// The control value program or the `gasp` table can disable hinting
    /// for a size.
    pub(super) fn is_enabled(&self) -> bool {
        self.scale.is_some() && self.instance.is_enabled()
    }

    /// Returns the size for an unhinted draw when the control value program
    /// disabled hinting.
    ///
    /// This is the interpreter's size, because FreeType chooses its scale
    /// before it reads the program's flag. Returns `None` when hinting is on
    /// or when the `gasp` table disabled it. FreeType checks `gasp` before
    /// choosing a scale, so it draws at the requested size.
    pub(super) fn program_unhinted_ppem(&self) -> Option<f32> {
        (self.scale.is_some() && self.instance.is_disabled_by_program()).then_some(self.ppem)
    }

    /// Returns whether glyph programs start in backward compatibility mode.
    #[cfg(test)]
    pub(super) fn backward_compatibility(&self) -> bool {
        self.instance.backward_compatibility()
    }

    /// Makes the control value program disable hinting, as a font might.
    #[cfg(test)]
    pub(super) fn simulate_program_disabling_hinting(&mut self) {
        self.instance.simulate_prep_flag_suppress_hinting();
    }

    /// Returns the size the interpreter hints at, once configured.
    pub(super) fn ppem(&self) -> Option<f32> {
        self.scale.is_some().then_some(self.ppem)
    }

    pub(super) fn raster_policy(&self) -> RasterPolicy {
        self.instance.raster_policy()
    }

    /// Returns the bytes of working memory needed to draw `outline`.
    ///
    /// This covers the loader's buffers and the interpreter's.
    pub(super) fn required_memory_size(&self, outline: &GlyfGlyph<'_>) -> usize {
        let lengths = outline.plan().buffer_lengths::<Scale26Dot6>();
        lengths.packed_len::<Scale26Dot6>()
            + self.scratch_lengths.packed_len(lengths.points as usize)
    }

    pub(super) fn draw(
        &self,
        outline: &GlyfGlyph<'_>,
        memory: Option<&mut [u8]>,
        sink: &mut dyn PathSink,
    ) -> Result<DrawnMetrics, ScaleError> {
        match memory {
            Some(buf) => self.draw_in(outline, buf, sink),
            None => with_scratch(self.required_memory_size(outline), |buf| {
                self.draw_in(outline, buf, sink)
            }),
        }
    }

    fn draw_in(
        &self,
        outline: &GlyfGlyph<'_>,
        buf: &mut [u8],
        mut sink: &mut dyn PathSink,
    ) -> Result<DrawnMetrics, ScaleError> {
        let (Some(tables), Some(scale)) = (&self.tables, &self.scale) else {
            return Err(Error::MissingResource.into());
        };
        let insufficient = ScaleError::InsufficientMemory {
            required: self.required_memory_size(outline),
            supplied: buf.len(),
        };
        let tables = tables.get();
        let glyph = outline.glyph();
        let font = outline.font();
        let plan = outline.plan();
        let lengths = plan.buffer_lengths::<Scale26Dot6>();
        let loader_len = lengths.packed_len::<Scale26Dot6>();
        if buf.len() < loader_len {
            return Err(insufficient);
        }
        let (loader_buf, rest) = buf.split_at_mut(loader_len);
        let buffers = Buffers::from_bytes(loader_buf, &lengths).map_err(|error| match error {
            Error::InsufficientMemory => insufficient.clone(),
            error => error.into(),
        })?;
        let scratch = self
            .scratch_lengths
            .carve(rest, lengths.points as usize)
            .ok_or_else(|| insufficient.clone())?;
        let mut hinter = FontHinter {
            instance: &self.instance,
            tables,
            scratch,
            pedantic: self.pedantic,
            // A glyph with no instructions of its own keeps what the control
            // value program chose.
            dropout_control: self.instance.dropout_control(),
            error: None,
        };
        let loaded = plan
            .load(&font, scale, buffers, Some(&mut hinter))
            .map_err(|error| match hinter.error.take() {
                Some(error) => ScaleError::Hinting(error),
                None if error == Error::InsufficientMemory => insufficient.clone(),
                None => error.into(),
            })?;
        // FreeType takes the advance from hdmx when hinting outside backward
        // compatibility mode, as the control value program left it; a glyph
        // program that changes the mode does so after the choice is made.
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/a5877f48fff8dd6cf8d53c505ce055499376aee2/src/truetype/ttgload.c#L2321>
        let hdmx_width = if self.instance.backward_compatibility() {
            None
        } else {
            self.hdmx_width(font.tables(), glyph)
        };
        let advance = hdmx_width
            .map(F26Dot6::from_i32)
            .unwrap_or_else(|| loaded.adjusted_advance_width());
        if !loaded.to_path(PathContourStart::ScanBackward, &mut sink) {
            return Err(Error::Malformed.into());
        }
        let mut metrics = DrawnMetrics::default();
        // A hinted advance is rounded, as FreeType rounds it.
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/base/ftobjs.c#L889>
        metrics.h_advance = Some(advance.round().to_f32());
        metrics.x_bearing = Some(loaded.adjusted_lsb().to_f32());
        metrics.has_overlaps = plan.has_overlaps;
        metrics.dropout_control = hinter.dropout_control;
        Ok(metrics)
    }

    /// Returns the `hdmx` advance for `glyph` at the size, if any.
    fn hdmx_width(&self, tables: &Tables, glyph: GlyphId) -> Option<i32> {
        let widths = tables.hdmx_data()?.get(self.hdmx_widths.clone()?)?;
        Some(*widths.get(glyph.to_u32() as usize)? as i32)
    }
}

/// The interpreter attached to the loader's hook for one draw.
///
/// Owns the per glyph working memory that the zone does not hold.
struct FontHinter<'a> {
    instance: &'a HintInstance,
    tables: &'a HintTables<'a>,
    scratch: HintScratch<'a>,
    pedantic: bool,
    dropout_control: DropoutControl,
    /// The failure of a glyph program, kept because the load reports only
    /// its kind.
    error: Option<HintError>,
}

impl Hinter<F26Dot6> for FontHinter<'_> {
    fn backward_compatibility(&self) -> bool {
        self.instance.backward_compatibility()
    }

    fn round_composite_x_offset(&self) -> bool {
        self.instance.round_composite_x_offset()
    }

    fn hint(&mut self, zone: &mut GlyphZone<'_, F26Dot6>) -> Result<(), HintError> {
        // The zone hands over the unhinted scaled outline, so the originals
        // are a copy taken on entry. Except for the phantom points: `points`
        // carries them rounded for the interpreter to work on, while the
        // original outline keeps them unrounded, which `phantom` still has.
        let original_scaled = &mut self.scratch.original_scaled[..zone.points.len()];
        original_scaled.copy_from_slice(zone.points);
        let phantom_start = original_scaled.len() - PHANTOM_POINT_COUNT;
        original_scaled[phantom_start..].copy_from_slice(&zone.phantom[..]);
        let mut input = HintOutline {
            glyph_id: zone.glyph,
            unscaled: zone.unscaled,
            scaled: zone.points,
            original_scaled,
            flags: zone.flags,
            contours: zone.contours,
            bytecode: zone.bytecode,
            phantom: &mut zone.phantom[..],
            stack: &mut *self.scratch.stack,
            cvt: &mut *self.scratch.cvt,
            storage: &mut *self.scratch.storage,
            twilight_scaled: &mut *self.scratch.twilight_scaled,
            twilight_original_scaled: &mut *self.scratch.twilight_original_scaled,
            twilight_flags: &mut *self.scratch.twilight_flags,
            is_composite: zone.is_composite,
            coords: zone.coords,
        };
        match self.instance.hint(self.tables, &mut input, self.pedantic) {
            Ok(dropout_control) => {
                self.dropout_control = dropout_control;
                Ok(())
            }
            // Only a pedantic hinter reports a failing program.
            Err(error) => {
                self.error = Some(error.clone());
                Err(error)
            }
        }
    }
}

/// Returns the tables the interpreter reads from `tables`.
pub(super) fn hint_tables(tables: &Tables) -> HintTables<'_> {
    let maxp = tables.selected_maxp().ok();
    HintTables {
        fpgm: tables.fpgm().unwrap_or_default(),
        prep: tables.prep().unwrap_or_default(),
        cvt: tables.cvt().unwrap_or_default(),
        cvar: tables.cvar().ok(),
        max_function_defs: maxp
            .as_ref()
            .and_then(|maxp| maxp.max_function_defs())
            .unwrap_or_default(),
        max_instruction_defs: maxp
            .as_ref()
            .and_then(|maxp| maxp.max_instruction_defs())
            .unwrap_or_default(),
        // Four more for the phantom points, as FreeType reserves.
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/truetype/ttobjs.c#L1188>
        max_twilight_points: maxp
            .as_ref()
            .and_then(|maxp| maxp.max_twilight_points())
            .unwrap_or_default()
            .saturating_add(4),
        // Thirty two more, FreeType's allowance for buggy fonts.
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/truetype/ttinterp.c#L356>
        max_stack_elements: maxp
            .as_ref()
            .and_then(|maxp| maxp.max_stack_elements())
            .unwrap_or_default()
            .saturating_add(32),
        max_storage: maxp
            .as_ref()
            .and_then(|maxp| maxp.max_storage())
            .unwrap_or_default(),
        axis_count: tables
            .fvar()
            .map(|fvar| fvar.axis_count())
            .unwrap_or_default(),
    }
}

/// Returns true if FreeType would run the interpreter for `tables` rather
/// than the automatic hinter.
///
/// This is the case when the font has glyph instructions, or a font or
/// control value program.
/// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/base/ftobjs.c#L1001>
pub(super) fn prefer_interpreter(tables: &Tables) -> bool {
    let max_instructions = tables
        .selected_maxp()
        .ok()
        .and_then(|maxp| maxp.max_size_of_instructions())
        .unwrap_or_default();
    !(max_instructions == 0
        && tables.fpgm().unwrap_or_default().is_empty()
        && tables.prep().unwrap_or_default().is_empty())
}

/// Returns the byte range of the widths in the `hdmx` record for `ppem`, if
/// any.
///
/// Only a whole size that fits in a byte has a record.
/// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/truetype/ttgload.c#L1996>
fn hdmx_widths(tables: &Tables, ppem: i32) -> Option<Range<usize>> {
    // FreeType looks the record up by the size rounded to a whole pixel,
    // the one MPPEM reports, so a fractional size still finds its record.
    let size = u8::try_from(ppem).ok()?;
    let data = tables.hdmx_data()?;
    let widths = tables.hdmx().ok()?.record_for_size(size)?.widths();
    // The record borrows the same bytes, so its position is an offset.
    let start = (widths.as_ptr() as usize).checked_sub(data.as_ptr() as usize)?;
    let end = start.checked_add(widths.len())?;
    (end <= data.len()).then_some(start..end)
}
