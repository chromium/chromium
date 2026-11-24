//! OpenType MATH queries with HarfBuzz-compatible C signatures.

use core::ffi::c_uint;

use read_fonts::tables::math::{Math, MathConstant, MathKernCorner, MathValue, StretchAxis};
use read_fonts::types::GlyphId;
use read_fonts::TableProvider;

use crate::common::{
    hr_bool_t, hr_codepoint_t, hr_direction_t, hr_position_t, hr_tag_t, HR_DIRECTION_BTT,
    HR_DIRECTION_TTB,
};
use crate::face::hr_face_t;
use crate::font::hr_font_t;
use crate::object;

pub type hr_ot_math_constant_t = c_uint;
pub type hr_ot_math_kern_t = c_uint;
pub type hr_ot_math_glyph_part_flags_t = c_uint;

pub const HR_OT_MATH_CONSTANT_SCRIPT_PERCENT_SCALE_DOWN: hr_ot_math_constant_t = 0u32;
pub const HR_OT_MATH_CONSTANT_SCRIPT_SCRIPT_PERCENT_SCALE_DOWN: hr_ot_math_constant_t = 1u32;
pub const HR_OT_MATH_CONSTANT_DELIMITED_SUB_FORMULA_MIN_HEIGHT: hr_ot_math_constant_t = 2u32;
pub const HR_OT_MATH_CONSTANT_DISPLAY_OPERATOR_MIN_HEIGHT: hr_ot_math_constant_t = 3u32;
pub const HR_OT_MATH_CONSTANT_MATH_LEADING: hr_ot_math_constant_t = 4u32;
pub const HR_OT_MATH_CONSTANT_AXIS_HEIGHT: hr_ot_math_constant_t = 5u32;
pub const HR_OT_MATH_CONSTANT_ACCENT_BASE_HEIGHT: hr_ot_math_constant_t = 6u32;
pub const HR_OT_MATH_CONSTANT_FLATTENED_ACCENT_BASE_HEIGHT: hr_ot_math_constant_t = 7u32;
pub const HR_OT_MATH_CONSTANT_SUBSCRIPT_SHIFT_DOWN: hr_ot_math_constant_t = 8u32;
pub const HR_OT_MATH_CONSTANT_SUBSCRIPT_TOP_MAX: hr_ot_math_constant_t = 9u32;
pub const HR_OT_MATH_CONSTANT_SUBSCRIPT_BASELINE_DROP_MIN: hr_ot_math_constant_t = 10u32;
pub const HR_OT_MATH_CONSTANT_SUPERSCRIPT_SHIFT_UP: hr_ot_math_constant_t = 11u32;
pub const HR_OT_MATH_CONSTANT_SUPERSCRIPT_SHIFT_UP_CRAMPED: hr_ot_math_constant_t = 12u32;
pub const HR_OT_MATH_CONSTANT_SUPERSCRIPT_BOTTOM_MIN: hr_ot_math_constant_t = 13u32;
pub const HR_OT_MATH_CONSTANT_SUPERSCRIPT_BASELINE_DROP_MAX: hr_ot_math_constant_t = 14u32;
pub const HR_OT_MATH_CONSTANT_SUB_SUPERSCRIPT_GAP_MIN: hr_ot_math_constant_t = 15u32;
pub const HR_OT_MATH_CONSTANT_SUPERSCRIPT_BOTTOM_MAX_WITH_SUBSCRIPT: hr_ot_math_constant_t = 16u32;
pub const HR_OT_MATH_CONSTANT_SPACE_AFTER_SCRIPT: hr_ot_math_constant_t = 17u32;
pub const HR_OT_MATH_CONSTANT_UPPER_LIMIT_GAP_MIN: hr_ot_math_constant_t = 18u32;
pub const HR_OT_MATH_CONSTANT_UPPER_LIMIT_BASELINE_RISE_MIN: hr_ot_math_constant_t = 19u32;
pub const HR_OT_MATH_CONSTANT_LOWER_LIMIT_GAP_MIN: hr_ot_math_constant_t = 20u32;
pub const HR_OT_MATH_CONSTANT_LOWER_LIMIT_BASELINE_DROP_MIN: hr_ot_math_constant_t = 21u32;
pub const HR_OT_MATH_CONSTANT_STACK_TOP_SHIFT_UP: hr_ot_math_constant_t = 22u32;
pub const HR_OT_MATH_CONSTANT_STACK_TOP_DISPLAY_STYLE_SHIFT_UP: hr_ot_math_constant_t = 23u32;
pub const HR_OT_MATH_CONSTANT_STACK_BOTTOM_SHIFT_DOWN: hr_ot_math_constant_t = 24u32;
pub const HR_OT_MATH_CONSTANT_STACK_BOTTOM_DISPLAY_STYLE_SHIFT_DOWN: hr_ot_math_constant_t = 25u32;
pub const HR_OT_MATH_CONSTANT_STACK_GAP_MIN: hr_ot_math_constant_t = 26u32;
pub const HR_OT_MATH_CONSTANT_STACK_DISPLAY_STYLE_GAP_MIN: hr_ot_math_constant_t = 27u32;
pub const HR_OT_MATH_CONSTANT_STRETCH_STACK_TOP_SHIFT_UP: hr_ot_math_constant_t = 28u32;
pub const HR_OT_MATH_CONSTANT_STRETCH_STACK_BOTTOM_SHIFT_DOWN: hr_ot_math_constant_t = 29u32;
pub const HR_OT_MATH_CONSTANT_STRETCH_STACK_GAP_ABOVE_MIN: hr_ot_math_constant_t = 30u32;
pub const HR_OT_MATH_CONSTANT_STRETCH_STACK_GAP_BELOW_MIN: hr_ot_math_constant_t = 31u32;
pub const HR_OT_MATH_CONSTANT_FRACTION_NUMERATOR_SHIFT_UP: hr_ot_math_constant_t = 32u32;
pub const HR_OT_MATH_CONSTANT_FRACTION_NUMERATOR_DISPLAY_STYLE_SHIFT_UP: hr_ot_math_constant_t =
    33u32;
pub const HR_OT_MATH_CONSTANT_FRACTION_DENOMINATOR_SHIFT_DOWN: hr_ot_math_constant_t = 34u32;
pub const HR_OT_MATH_CONSTANT_FRACTION_DENOMINATOR_DISPLAY_STYLE_SHIFT_DOWN: hr_ot_math_constant_t =
    35u32;
pub const HR_OT_MATH_CONSTANT_FRACTION_NUMERATOR_GAP_MIN: hr_ot_math_constant_t = 36u32;
pub const HR_OT_MATH_CONSTANT_FRACTION_NUM_DISPLAY_STYLE_GAP_MIN: hr_ot_math_constant_t = 37u32;
pub const HR_OT_MATH_CONSTANT_FRACTION_RULE_THICKNESS: hr_ot_math_constant_t = 38u32;
pub const HR_OT_MATH_CONSTANT_FRACTION_DENOMINATOR_GAP_MIN: hr_ot_math_constant_t = 39u32;
pub const HR_OT_MATH_CONSTANT_FRACTION_DENOM_DISPLAY_STYLE_GAP_MIN: hr_ot_math_constant_t = 40u32;
pub const HR_OT_MATH_CONSTANT_SKEWED_FRACTION_HORIZONTAL_GAP: hr_ot_math_constant_t = 41u32;
pub const HR_OT_MATH_CONSTANT_SKEWED_FRACTION_VERTICAL_GAP: hr_ot_math_constant_t = 42u32;
pub const HR_OT_MATH_CONSTANT_OVERBAR_VERTICAL_GAP: hr_ot_math_constant_t = 43u32;
pub const HR_OT_MATH_CONSTANT_OVERBAR_RULE_THICKNESS: hr_ot_math_constant_t = 44u32;
pub const HR_OT_MATH_CONSTANT_OVERBAR_EXTRA_ASCENDER: hr_ot_math_constant_t = 45u32;
pub const HR_OT_MATH_CONSTANT_UNDERBAR_VERTICAL_GAP: hr_ot_math_constant_t = 46u32;
pub const HR_OT_MATH_CONSTANT_UNDERBAR_RULE_THICKNESS: hr_ot_math_constant_t = 47u32;
pub const HR_OT_MATH_CONSTANT_UNDERBAR_EXTRA_DESCENDER: hr_ot_math_constant_t = 48u32;
pub const HR_OT_MATH_CONSTANT_RADICAL_VERTICAL_GAP: hr_ot_math_constant_t = 49u32;
pub const HR_OT_MATH_CONSTANT_RADICAL_DISPLAY_STYLE_VERTICAL_GAP: hr_ot_math_constant_t = 50u32;
pub const HR_OT_MATH_CONSTANT_RADICAL_RULE_THICKNESS: hr_ot_math_constant_t = 51u32;
pub const HR_OT_MATH_CONSTANT_RADICAL_EXTRA_ASCENDER: hr_ot_math_constant_t = 52u32;
pub const HR_OT_MATH_CONSTANT_RADICAL_KERN_BEFORE_DEGREE: hr_ot_math_constant_t = 53u32;
pub const HR_OT_MATH_CONSTANT_RADICAL_KERN_AFTER_DEGREE: hr_ot_math_constant_t = 54u32;
pub const HR_OT_MATH_CONSTANT_RADICAL_DEGREE_BOTTOM_RAISE_PERCENT: hr_ot_math_constant_t = 55u32;

pub const HR_OT_MATH_KERN_TOP_RIGHT: hr_ot_math_kern_t = 0u32;
pub const HR_OT_MATH_KERN_TOP_LEFT: hr_ot_math_kern_t = 1u32;
pub const HR_OT_MATH_KERN_BOTTOM_RIGHT: hr_ot_math_kern_t = 2u32;
pub const HR_OT_MATH_KERN_BOTTOM_LEFT: hr_ot_math_kern_t = 3u32;
pub const HR_OT_MATH_GLYPH_PART_FLAG_EXTENDER: hr_ot_math_glyph_part_flags_t = 1u32;

/// Deprecated HarfBuzz spelling of `HR_OT_MATH_GLYPH_PART_FLAG_EXTENDER`.
pub const HR_MATH_GLYPH_PART_FLAG_EXTENDER: hr_ot_math_glyph_part_flags_t =
    HR_OT_MATH_GLYPH_PART_FLAG_EXTENDER;
pub const HR_OT_TAG_MATH: hr_tag_t = 0x4D41_5448u32;
pub const HR_OT_TAG_MATH_SCRIPT: hr_tag_t = 0x6D61_7468u32;

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq, Eq)]
pub struct hr_ot_math_kern_entry_t {
    pub max_correction_height: hr_position_t,
    pub kern_value: hr_position_t,
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq, Eq)]
pub struct hr_ot_math_glyph_variant_t {
    pub glyph: hr_codepoint_t,
    pub advance: hr_position_t,
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq, Eq)]
pub struct hr_ot_math_glyph_part_t {
    pub glyph: hr_codepoint_t,
    pub start_connector_length: hr_position_t,
    pub end_connector_length: hr_position_t,
    pub full_advance: hr_position_t,
    pub flags: hr_ot_math_glyph_part_flags_t,
}

fn face_math(face: &hr_face_t) -> Option<Math<'_>> {
    face.font()?.tables().math().ok()
}

fn font_math(font: &hr_font_t) -> Option<Math<'_>> {
    font.instance()?.tables().math().ok()
}

fn horizontal(direction: hr_direction_t) -> bool {
    !matches!(direction, HR_DIRECTION_TTB | HR_DIRECTION_BTT)
}

fn scale(font: &hr_font_t, value: i32, x_axis: bool) -> i32 {
    let scale = harfrust::Scale::new(Some((font.x_scale, font.y_scale)), font.upem());
    if x_axis {
        scale.scale_x(value)
    } else {
        scale.scale_y(value)
    }
}

fn ppem(font: &hr_font_t, x_axis: bool) -> u16 {
    u16::try_from(if x_axis { font.x_ppem } else { font.y_ppem }).unwrap_or(0)
}

fn position(font: &hr_font_t, value: MathValue, x_axis: bool) -> i32 {
    let axis_scale = if x_axis { font.x_scale } else { font.y_scale };
    let device = if ppem(font, x_axis) == 0 {
        0
    } else {
        (i64::from(value.delta_px) * i64::from(axis_scale) / i64::from(ppem(font, x_axis))) as i32
    };
    scale(font, value.value, x_axis).saturating_add(device)
}

fn corner(kern: hr_ot_math_kern_t) -> Option<MathKernCorner> {
    Some(match kern {
        HR_OT_MATH_KERN_TOP_RIGHT => MathKernCorner::TopRight,
        HR_OT_MATH_KERN_TOP_LEFT => MathKernCorner::TopLeft,
        HR_OT_MATH_KERN_BOTTOM_RIGHT => MathKernCorner::BottomRight,
        HR_OT_MATH_KERN_BOTTOM_LEFT => MathKernCorner::BottomLeft,
        _ => return None,
    })
}

fn axis(direction: hr_direction_t) -> StretchAxis {
    if horizontal(direction) {
        StretchAxis::Horizontal
    } else {
        StretchAxis::Vertical
    }
}

/// Whether a face has a MATH table with a nonzero version.
/// # Safety
/// `face` must be null or a live face.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_math_has_data(face: *mut hr_face_t) -> hr_bool_t {
    let face = unsafe { object::or_empty(face.cast_const()) };
    face_math(face).is_some_and(|math| math.version().major != 0 || math.version().minor != 0)
        as hr_bool_t
}

/// Returns a MATH constant, scaled on its specified axis.
/// # Safety
/// `font` must be null or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_math_get_constant(
    font: *mut hr_font_t,
    constant: hr_ot_math_constant_t,
) -> hr_position_t {
    let font = unsafe { object::or_empty(font.cast_const()) };
    let Some(math) = font_math(font) else {
        return 0;
    };
    let Ok(constants) = math.math_constants() else {
        return 0;
    };
    let mut index = constant;
    if math.has_swapped_min_heights() && (index == 2 || index == 3) {
        index = 5 - index;
    }
    let Some(kind) = u8::try_from(index).ok().and_then(MathConstant::new) else {
        return 0;
    };
    if matches!(index, 0 | 1 | 55) {
        return constants.constant(kind);
    }
    let x_axis = matches!(index, 17 | 41 | 53 | 54);
    position(
        font,
        constants.constant_for_ppem(kind, ppem(font, x_axis)),
        x_axis,
    )
}

/// Returns the glyph's MATH italics correction, or zero.
/// # Safety
/// `font` must be null or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_math_get_glyph_italics_correction(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
) -> hr_position_t {
    let font = unsafe { object::or_empty(font.cast_const()) };
    font_math(font)
        .and_then(|math| math.math_glyph_info().ok())
        .and_then(|info| info.math_italics_correction_info()?.ok())
        .and_then(|info| info.correction_for_ppem(GlyphId::new(glyph), ppem(font, true)))
        .map_or(0, |value| position(font, value, true))
}

/// Returns the top accent attachment, falling back to half the glyph advance.
/// # Safety
/// `font` must be null or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_math_get_glyph_top_accent_attachment(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
) -> hr_position_t {
    let state = unsafe { object::or_empty(font.cast_const()) };
    let value = font_math(state)
        .and_then(|math| math.math_glyph_info().ok())
        .and_then(|info| info.math_top_accent_attachment()?.ok())
        .and_then(|info| info.attachment_for_ppem(GlyphId::new(glyph), ppem(state, true)));
    value.map_or_else(
        || state.glyph_h_advance(font, glyph) / 2,
        |value| position(state, value, true),
    )
}

/// Whether the glyph is covered by MATH's extended-shape set.
/// # Safety
/// `face` must be null or a live face.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_math_is_glyph_extended_shape(
    face: *mut hr_face_t,
    glyph: hr_codepoint_t,
) -> hr_bool_t {
    let face = unsafe { object::or_empty(face.cast_const()) };
    face_math(face)
        .and_then(|math| math.math_glyph_info().ok())
        .is_some_and(|info| info.is_extended_shape(GlyphId::new(glyph))) as hr_bool_t
}

/// Returns a MATH kern selected in the font's scaled coordinate space.
/// # Safety
/// `font` must be null or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_math_get_glyph_kerning(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    kern: hr_ot_math_kern_t,
    correction_height: hr_position_t,
) -> hr_position_t {
    let state = unsafe { object::or_empty(font.cast_const()) };
    let Some(corner) = corner(kern) else { return 0 };
    let Some(table) = font_math(state)
        .and_then(|math| math.math_glyph_info().ok())
        .and_then(|info| info.math_kern_info()?.ok())
        .and_then(|info| info.kern(GlyphId::new(glyph), corner))
    else {
        return 0;
    };
    // The heights are sorted, so binary search them as HarfBuzz does. A height
    // count is 16 bits and every record may carry a device table, so reading
    // each one at its size would cost `heightCount * ppem` per query: only the
    // heights the search probes, and the one kern it selects, are read.
    let data = table.offset_data();
    let y_ppem = ppem(state, false);
    let band = table.correction_height().partition_point(|height| {
        let bound = position(state, height.value_for_ppem(data, y_ppem), false);
        if state.y_scale >= 0 {
            correction_height >= bound
        } else {
            correction_height <= bound
        }
    });
    table.kern_values().get(band).map_or(0, |kern| {
        position(state, kern.value_for_ppem(data, ppem(state, true)), true)
    })
}

/// Returns paginated raw MATH kern entries and their total count.
/// # Safety
/// `font` must be null or live; `entries_count` must be null or writable;
/// `kern_entries` must hold its input capacity when non-null.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_math_get_glyph_kernings(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    kern: hr_ot_math_kern_t,
    start_offset: c_uint,
    entries_count: *mut c_uint,
    kern_entries: *mut hr_ot_math_kern_entry_t,
) -> c_uint {
    let capacity = if kern_entries.is_null() {
        0
    } else {
        unsafe { entries_count.as_ref() }.copied().unwrap_or(0) as usize
    };
    let state = unsafe { object::or_empty(font.cast_const()) };
    let table = corner(kern).and_then(|corner| {
        font_math(state)
            .and_then(|math| math.math_glyph_info().ok())
            .and_then(|info| info.math_kern_info()?.ok())
            .and_then(|info| info.kern(GlyphId::new(glyph), corner))
    });
    let found = table.is_some();
    let mut written = 0;
    let mut total = 0;
    if let Some(table) = table {
        total = table.kern_values().len() as c_uint;
        if !kern_entries.is_null() {
            // Read only the entries written: each height at the y size and
            // each kern at the x size, skipping the records before the page
            // rather than reading them.
            let data = table.offset_data();
            let heights = table.correction_height();
            let (x_ppem, y_ppem) = (ppem(state, true), ppem(state, false));
            for (i, kern) in table
                .kern_values()
                .iter()
                .enumerate()
                .skip(start_offset as usize)
                .take(capacity)
            {
                let value = hr_ot_math_kern_entry_t {
                    max_correction_height: heights.get(i).map_or(i32::MAX, |height| {
                        position(state, height.value_for_ppem(data, y_ppem), false)
                    }),
                    kern_value: position(state, kern.value_for_ppem(data, x_ppem), true),
                };
                unsafe { kern_entries.add(written).write(value) };
                written += 1;
            }
        }
    }
    if (!kern_entries.is_null() || !found) && !entries_count.is_null() {
        unsafe { entries_count.write(written as c_uint) };
    }
    total
}

/// Returns paginated MATH size variants and their total count.
/// # Safety
/// `font` must be null or live; `variants_count` must be null or writable;
/// `variants` must hold its input capacity when non-null.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_math_get_glyph_variants(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    direction: hr_direction_t,
    start_offset: c_uint,
    variants_count: *mut c_uint,
    variants: *mut hr_ot_math_glyph_variant_t,
) -> c_uint {
    let capacity = if variants.is_null() {
        0
    } else {
        unsafe { variants_count.as_ref() }.copied().unwrap_or(0) as usize
    };
    let state = unsafe { object::or_empty(font.cast_const()) };
    let construction = font_math(state)
        .and_then(|math| math.math_variants().ok())
        .and_then(|table| table.glyph_construction(GlyphId::new(glyph), axis(direction)));
    let found = construction.is_some();
    let mut written = 0;
    let mut total = 0;
    if let Some(construction) = construction {
        let records = construction.math_glyph_variant_records();
        total = records.len() as c_uint;
        if !variants.is_null() {
            for record in records.iter().skip(start_offset as usize).take(capacity) {
                unsafe {
                    variants.add(written).write(hr_ot_math_glyph_variant_t {
                        glyph: record.variant_glyph().to_u32(),
                        advance: scale(
                            state,
                            record.advance_measurement().to_u16() as i32,
                            horizontal(direction),
                        ),
                    });
                };
                written += 1;
            }
        }
    }
    if (!variants.is_null() || !found) && !variants_count.is_null() {
        unsafe { variants_count.write(written as c_uint) };
    }
    total
}

/// Returns the minimum overlap needed between MATH assembly parts.
/// # Safety
/// `font` must be null or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_math_get_min_connector_overlap(
    font: *mut hr_font_t,
    direction: hr_direction_t,
) -> hr_position_t {
    let font = unsafe { object::or_empty(font.cast_const()) };
    font_math(font)
        .and_then(|math| math.math_variants().ok())
        .map_or(0, |variants| {
            scale(
                font,
                variants.min_connector_overlap().to_u16() as i32,
                horizontal(direction),
            )
        })
}

/// Returns paginated MATH assembly parts and their total count.
/// # Safety
/// `font` must be null or live; `parts_count` and `italics_correction` must be
/// null or writable; `parts` must hold the input capacity when non-null.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_math_get_glyph_assembly(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    direction: hr_direction_t,
    start_offset: c_uint,
    parts_count: *mut c_uint,
    parts: *mut hr_ot_math_glyph_part_t,
    italics_correction: *mut hr_position_t,
) -> c_uint {
    let capacity = if parts.is_null() {
        0
    } else {
        unsafe { parts_count.as_ref() }.copied().unwrap_or(0) as usize
    };
    let state = unsafe { object::or_empty(font.cast_const()) };
    let assembly = font_math(state)
        .and_then(|math| math.math_variants().ok())
        .and_then(|table| table.glyph_construction(GlyphId::new(glyph), axis(direction)))
        .and_then(|construction| construction.glyph_assembly()?.ok());
    let found = assembly.is_some();
    let mut written = 0;
    let mut total = 0;
    let mut correction = 0;
    if let Some(assembly) = assembly {
        let x_axis = horizontal(direction);
        correction = position(
            state,
            assembly.italics_correction_for_ppem(ppem(state, true)),
            true,
        );
        let records = assembly.part_records();
        total = records.len() as c_uint;
        if !parts.is_null() {
            for record in records.iter().skip(start_offset as usize).take(capacity) {
                unsafe {
                    parts.add(written).write(hr_ot_math_glyph_part_t {
                        glyph: record.glyph_id().to_u32(),
                        start_connector_length: scale(
                            state,
                            record.start_connector_length().to_u16() as i32,
                            x_axis,
                        ),
                        end_connector_length: scale(
                            state,
                            record.end_connector_length().to_u16() as i32,
                            x_axis,
                        ),
                        full_advance: scale(state, record.full_advance().to_u16() as i32, x_axis),
                        flags: c_uint::from(record.part_flags().bits()),
                    });
                };
                written += 1;
            }
        }
    }
    if (!parts.is_null() || !found) && !parts_count.is_null() {
        unsafe { parts_count.write(written as c_uint) };
    }
    if !italics_correction.is_null() {
        unsafe { italics_correction.write(correction) };
    }
    total
}
