//! Paint callbacks with HarfBuzz-compatible ownership and dispatch.
// The C drawing/painting ABI uses f32 coordinates, including integer font scales.
#![allow(clippy::cast_precision_loss)]
use crate::color::hr_color_t;
use crate::draw::hr_draw_funcs_t;
use crate::font_funcs::{callback_taking, Callback};
use crate::object::{self, Empty, Object, ObjectHeader};
use crate::{hr_blob_t, hr_glyph_extents_t, hr_tag_t};
use crate::{hr_bool_t, hr_codepoint_t, hr_destroy_func_t, hr_font_t, hr_user_data_key_t};
use core::ffi::{c_uint, c_void};
use std::sync::OnceLock;
/// Callback for push transform; data must be safe to use on any thread.
pub type hr_paint_push_transform_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        xx: f32,
        yx: f32,
        xy: f32,
        yy: f32,
        dx: f32,
        dy: f32,
        user_data: *mut c_void,
    ),
>;
/// Callback for pop transform; data must be safe to use on any thread.
pub type hr_paint_pop_transform_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        user_data: *mut c_void,
    ),
>;
/// Callback for color glyph; data must be safe to use on any thread.
pub type hr_paint_color_glyph_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        glyph: hr_codepoint_t,
        font: *mut hr_font_t,
        user_data: *mut c_void,
    ) -> hr_bool_t,
>;
/// Callback for push clip glyph; data must be safe to use on any thread.
pub type hr_paint_push_clip_glyph_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        glyph: hr_codepoint_t,
        font: *mut hr_font_t,
        user_data: *mut c_void,
    ),
>;
/// Callback for push clip rectangle; data must be safe to use on any thread.
pub type hr_paint_push_clip_rectangle_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        xmin: f32,
        ymin: f32,
        xmax: f32,
        ymax: f32,
        user_data: *mut c_void,
    ),
>;
/// Callback for push clip path start; data must be safe to use on any thread.
pub type hr_paint_push_clip_path_start_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        draw_data: *mut *mut c_void,
        user_data: *mut c_void,
    ) -> *mut hr_draw_funcs_t,
>;
/// Callback for push clip path end; data must be safe to use on any thread.
pub type hr_paint_push_clip_path_end_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        user_data: *mut c_void,
    ),
>;
/// Callback for pop clip; data must be safe to use on any thread.
pub type hr_paint_pop_clip_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        user_data: *mut c_void,
    ),
>;
/// Callback for color; data must be safe to use on any thread.
pub type hr_paint_color_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        is_foreground: hr_bool_t,
        color: hr_color_t,
        user_data: *mut c_void,
    ),
>;
/// Callback for fill glyph; data must be safe to use on any thread.
pub type hr_paint_fill_glyph_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        glyph: hr_codepoint_t,
        font: *mut hr_font_t,
        is_foreground: hr_bool_t,
        color: hr_color_t,
        user_data: *mut c_void,
    ),
>;
/// Callback for image; data must be safe to use on any thread.
pub type hr_paint_image_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        image: *mut hr_blob_t,
        width: c_uint,
        height: c_uint,
        format: hr_tag_t,
        slant: f32,
        extents: *mut hr_glyph_extents_t,
        user_data: *mut c_void,
    ) -> hr_bool_t,
>;
/// Callback for linear gradient; data must be safe to use on any thread.
pub type hr_paint_linear_gradient_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        color_line: *mut hr_color_line_t,
        x0: f32,
        y0: f32,
        x1: f32,
        y1: f32,
        x2: f32,
        y2: f32,
        user_data: *mut c_void,
    ),
>;
/// Callback for radial gradient; data must be safe to use on any thread.
pub type hr_paint_radial_gradient_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        color_line: *mut hr_color_line_t,
        x0: f32,
        y0: f32,
        r0: f32,
        x1: f32,
        y1: f32,
        r1: f32,
        user_data: *mut c_void,
    ),
>;
/// Callback for sweep gradient; data must be safe to use on any thread.
pub type hr_paint_sweep_gradient_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        color_line: *mut hr_color_line_t,
        x0: f32,
        y0: f32,
        start_angle: f32,
        end_angle: f32,
        user_data: *mut c_void,
    ),
>;
/// Callback for push group; data must be safe to use on any thread.
pub type hr_paint_push_group_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        user_data: *mut c_void,
    ),
>;
/// Callback for push group for; data must be safe to use on any thread.
pub type hr_paint_push_group_for_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        mode: hr_paint_composite_mode_t,
        user_data: *mut c_void,
    ),
>;
/// Callback for pop group; data must be safe to use on any thread.
pub type hr_paint_pop_group_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        mode: hr_paint_composite_mode_t,
        user_data: *mut c_void,
    ),
>;
/// Callback for custom palette color; data must be safe to use on any thread.
pub type hr_paint_custom_palette_color_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        color_index: c_uint,
        color: *mut hr_color_t,
        user_data: *mut c_void,
    ) -> hr_bool_t,
>;
/// A reference-counted, optionally immutable collection of paint callbacks.
#[derive(Default)]
pub struct hr_paint_funcs_t {
    header: ObjectHeader,
    push_transform: Option<Callback<hr_paint_push_transform_func_t>>,
    pop_transform: Option<Callback<hr_paint_pop_transform_func_t>>,
    color_glyph: Option<Callback<hr_paint_color_glyph_func_t>>,
    push_clip_glyph: Option<Callback<hr_paint_push_clip_glyph_func_t>>,
    push_clip_rectangle: Option<Callback<hr_paint_push_clip_rectangle_func_t>>,
    push_clip_path_start: Option<Callback<hr_paint_push_clip_path_start_func_t>>,
    push_clip_path_end: Option<Callback<hr_paint_push_clip_path_end_func_t>>,
    pop_clip: Option<Callback<hr_paint_pop_clip_func_t>>,
    color: Option<Callback<hr_paint_color_func_t>>,
    fill_glyph: Option<Callback<hr_paint_fill_glyph_func_t>>,
    image: Option<Callback<hr_paint_image_func_t>>,
    linear_gradient: Option<Callback<hr_paint_linear_gradient_func_t>>,
    radial_gradient: Option<Callback<hr_paint_radial_gradient_func_t>>,
    sweep_gradient: Option<Callback<hr_paint_sweep_gradient_func_t>>,
    push_group: Option<Callback<hr_paint_push_group_func_t>>,
    push_group_for: Option<Callback<hr_paint_push_group_for_func_t>>,
    pop_group: Option<Callback<hr_paint_pop_group_func_t>>,
    custom_palette_color: Option<Callback<hr_paint_custom_palette_color_func_t>>,
}
// SAFETY: callback registration requires thread-safe callbacks and opaque data.
unsafe impl Send for hr_paint_funcs_t {}
unsafe impl Sync for hr_paint_funcs_t {}
impl Object for hr_paint_funcs_t {
    fn header(&self) -> &ObjectHeader {
        &self.header
    }
    fn empty() -> *mut Self {
        static EMPTY: OnceLock<Empty<hr_paint_funcs_t>> = OnceLock::new();
        EMPTY
            .get_or_init(|| {
                Empty::new(hr_paint_funcs_t {
                    header: ObjectHeader::immortal(),
                    ..Default::default()
                })
            })
            .get()
    }
}
/// Creates an owned callback collection.
#[no_mangle]
pub extern "C" fn hr_paint_funcs_create() -> *mut hr_paint_funcs_t {
    object::create(hr_paint_funcs_t::default())
}
/// Returns the immortal empty collection.
#[no_mangle]
pub extern "C" fn hr_paint_funcs_get_empty() -> *mut hr_paint_funcs_t {
    hr_paint_funcs_t::empty()
}
/// Takes a reference. Accepts NULL.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_reference(
    funcs: *mut hr_paint_funcs_t,
) -> *mut hr_paint_funcs_t {
    unsafe { object::reference(funcs) }
}
/// Releases an owned reference and its callback data. Accepts NULL.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_destroy(funcs: *mut hr_paint_funcs_t) {
    unsafe { object::destroy(funcs) }
}
/// Rejects subsequent callback changes.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_make_immutable(funcs: *mut hr_paint_funcs_t) {
    unsafe { object::make_immutable(funcs) }
}
/// Reports whether callback changes are forbidden.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_is_immutable(funcs: *const hr_paint_funcs_t) -> hr_bool_t {
    i32::from(unsafe { object::is_immutable(funcs) })
}
/// Attaches address-keyed metadata, taking ownership only on success.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_user_data(
    funcs: *mut hr_paint_funcs_t,
    key: *const hr_user_data_key_t,
    data: *mut c_void,
    destroy: hr_destroy_func_t,
    replace: hr_bool_t,
) -> hr_bool_t {
    i32::from(unsafe { object::set_user_data(funcs, key, data, destroy, replace != 0) })
}
/// Returns borrowed metadata.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_get_user_data(
    funcs: *const hr_paint_funcs_t,
    key: *const hr_user_data_key_t,
) -> *mut c_void {
    unsafe { object::get_user_data(funcs.cast_mut(), key) }
}
/// Registers push transform. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_push_transform_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_push_transform_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.push_transform,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches push transform; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_push_transform(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
    xx: f32,
    yx: f32,
    xy: f32,
    yy: f32,
    dx: f32,
    dy: f32,
) {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.push_transform {
        if let Some(func) = callback.func {
            unsafe {
                func(
                    funcs,
                    paint_data,
                    xx,
                    yx,
                    xy,
                    yy,
                    dx,
                    dy,
                    callback.user_data,
                );
            }
        }
    }
}
/// Registers pop transform. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_pop_transform_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_pop_transform_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.pop_transform,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches pop transform; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_pop_transform(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
) {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.pop_transform {
        if let Some(func) = callback.func {
            unsafe { func(funcs, paint_data, callback.user_data) }
        }
    }
}
/// Registers color glyph. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_color_glyph_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_color_glyph_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.color_glyph,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches color glyph; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_color_glyph(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
    glyph: hr_codepoint_t,
    font: *mut hr_font_t,
) -> hr_bool_t {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.color_glyph {
        if let Some(func) = callback.func {
            return unsafe { func(funcs, paint_data, glyph, font, callback.user_data) };
        }
    }
    0
}
/// Registers push clip glyph. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_push_clip_glyph_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_push_clip_glyph_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.push_clip_glyph,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches push clip glyph; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_push_clip_glyph(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
    glyph: hr_codepoint_t,
    font: *mut hr_font_t,
) {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.push_clip_glyph {
        if let Some(func) = callback.func {
            unsafe { func(funcs, paint_data, glyph, font, callback.user_data) }
        }
    }
}
/// Registers push clip rectangle. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_push_clip_rectangle_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_push_clip_rectangle_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.push_clip_rectangle,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches push clip rectangle; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_push_clip_rectangle(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
    xmin: f32,
    ymin: f32,
    xmax: f32,
    ymax: f32,
) {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.push_clip_rectangle {
        if let Some(func) = callback.func {
            unsafe {
                func(
                    funcs,
                    paint_data,
                    xmin,
                    ymin,
                    xmax,
                    ymax,
                    callback.user_data,
                );
            }
        }
    }
}
/// Registers push clip path start. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_push_clip_path_start_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_push_clip_path_start_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.push_clip_path_start,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches push clip path start; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_push_clip_path_start(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
    draw_data: *mut *mut c_void,
) -> *mut hr_draw_funcs_t {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.push_clip_path_start {
        if let Some(func) = callback.func {
            return unsafe { func(funcs, paint_data, draw_data, callback.user_data) };
        }
    }
    core::ptr::null_mut()
}
/// Registers push clip path end. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_push_clip_path_end_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_push_clip_path_end_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.push_clip_path_end,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches push clip path end; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_push_clip_path_end(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
) {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.push_clip_path_end {
        if let Some(func) = callback.func {
            unsafe { func(funcs, paint_data, callback.user_data) }
        }
    }
}
/// Registers pop clip. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_pop_clip_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_pop_clip_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.pop_clip,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches pop clip; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_pop_clip(funcs: *mut hr_paint_funcs_t, paint_data: *mut c_void) {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.pop_clip {
        if let Some(func) = callback.func {
            unsafe { func(funcs, paint_data, callback.user_data) }
        }
    }
}
/// Registers color. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_color_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_color_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(&mut state.color, callback_taking(func, user_data, destroy));
    drop(previous);
}
/// Dispatches color; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_color(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
    is_foreground: hr_bool_t,
    color: hr_color_t,
) {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.color {
        if let Some(func) = callback.func {
            unsafe { func(funcs, paint_data, is_foreground, color, callback.user_data) }
        }
    }
}
/// Registers fill glyph. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_fill_glyph_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_fill_glyph_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.fill_glyph,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches fill glyph; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_fill_glyph(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
    glyph: hr_codepoint_t,
    font: *mut hr_font_t,
    is_foreground: hr_bool_t,
    color: hr_color_t,
) {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.fill_glyph {
        if let Some(func) = callback.func {
            return unsafe {
                func(
                    funcs,
                    paint_data,
                    glyph,
                    font,
                    is_foreground,
                    color,
                    callback.user_data,
                );
            };
        }
    }
    unsafe {
        hr_paint_push_clip_glyph(funcs, paint_data, glyph, font);
        hr_paint_color(funcs, paint_data, is_foreground, color);
        hr_paint_pop_clip(funcs, paint_data);
    }
}
/// Registers image. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_image_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_image_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(&mut state.image, callback_taking(func, user_data, destroy));
    drop(previous);
}
/// Dispatches image; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_image(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
    image: *mut hr_blob_t,
    width: c_uint,
    height: c_uint,
    format: hr_tag_t,
    slant: f32,
    extents: *mut hr_glyph_extents_t,
) -> hr_bool_t {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.image {
        if let Some(func) = callback.func {
            return unsafe {
                func(
                    funcs,
                    paint_data,
                    image,
                    width,
                    height,
                    format,
                    slant,
                    extents,
                    callback.user_data,
                )
            };
        }
    }
    0
}
/// Registers linear gradient. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_linear_gradient_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_linear_gradient_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.linear_gradient,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches linear gradient; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_linear_gradient(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
    color_line: *mut hr_color_line_t,
    x0: f32,
    y0: f32,
    x1: f32,
    y1: f32,
    x2: f32,
    y2: f32,
) {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.linear_gradient {
        if let Some(func) = callback.func {
            unsafe {
                func(
                    funcs,
                    paint_data,
                    color_line,
                    x0,
                    y0,
                    x1,
                    y1,
                    x2,
                    y2,
                    callback.user_data,
                );
            }
        }
    }
}
/// Registers radial gradient. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_radial_gradient_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_radial_gradient_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.radial_gradient,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches radial gradient; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_radial_gradient(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
    color_line: *mut hr_color_line_t,
    x0: f32,
    y0: f32,
    r0: f32,
    x1: f32,
    y1: f32,
    r1: f32,
) {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.radial_gradient {
        if let Some(func) = callback.func {
            unsafe {
                func(
                    funcs,
                    paint_data,
                    color_line,
                    x0,
                    y0,
                    r0,
                    x1,
                    y1,
                    r1,
                    callback.user_data,
                );
            }
        }
    }
}
/// Registers sweep gradient. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_sweep_gradient_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_sweep_gradient_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.sweep_gradient,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches sweep gradient; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_sweep_gradient(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
    color_line: *mut hr_color_line_t,
    x0: f32,
    y0: f32,
    start_angle: f32,
    end_angle: f32,
) {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.sweep_gradient {
        if let Some(func) = callback.func {
            unsafe {
                func(
                    funcs,
                    paint_data,
                    color_line,
                    x0,
                    y0,
                    start_angle,
                    end_angle,
                    callback.user_data,
                );
            }
        }
    }
}
/// Registers push group. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_push_group_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_push_group_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.push_group,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches push group; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_push_group(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
) {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.push_group {
        if let Some(func) = callback.func {
            unsafe { func(funcs, paint_data, callback.user_data) }
        }
    }
}
/// Registers push group for. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_push_group_for_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_push_group_for_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.push_group_for,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches push group for; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_push_group_for(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
    mode: hr_paint_composite_mode_t,
) {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.push_group_for {
        if let Some(func) = callback.func {
            return unsafe { func(funcs, paint_data, mode, callback.user_data) };
        }
    }
    unsafe { hr_paint_push_group(funcs, paint_data) }
}
/// Registers pop group. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_pop_group_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_pop_group_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.pop_group,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches pop group; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_pop_group(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
    mode: hr_paint_composite_mode_t,
) {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.pop_group {
        if let Some(func) = callback.func {
            unsafe { func(funcs, paint_data, mode, callback.user_data) }
        }
    }
}
/// Registers custom palette color. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_funcs_set_custom_palette_color_func(
    funcs: *mut hr_paint_funcs_t,
    func: hr_paint_custom_palette_color_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(funcs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) }
        }
        return;
    };
    let previous = core::mem::replace(
        &mut state.custom_palette_color,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Dispatches custom palette color; omitted callbacks use the HarfBuzz default.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_custom_palette_color(
    funcs: *mut hr_paint_funcs_t,
    paint_data: *mut c_void,
    color_index: c_uint,
    color: *mut hr_color_t,
) -> hr_bool_t {
    let state = unsafe { object::or_empty(funcs) };
    if let Some(callback) = &state.custom_palette_color {
        if let Some(func) = callback.func {
            return unsafe { func(funcs, paint_data, color_index, color, callback.user_data) };
        }
    }
    0
}

/// Gradient extend mode.
pub type hr_paint_extend_t = c_uint;
/// PNG image bytes, decoded by the client.
pub const HR_PAINT_IMAGE_FORMAT_PNG: hr_tag_t = 0x706e_6720;
/// SVG document bytes, rendered by the client.
pub const HR_PAINT_IMAGE_FORMAT_SVG: hr_tag_t = 0x7376_6720;
/// Premultiplied BGRA pixels in the sRGBA color space.
pub const HR_PAINT_IMAGE_FORMAT_BGRA: hr_tag_t = 0x4247_5241;
pub const HR_PAINT_EXTEND_PAD: hr_paint_extend_t = 0;
pub const HR_PAINT_EXTEND_REPEAT: hr_paint_extend_t = 1;
pub const HR_PAINT_EXTEND_REFLECT: hr_paint_extend_t = 2;
/// COLR compositing mode, with the OpenType/HarfBuzz numeric values.
pub type hr_paint_composite_mode_t = c_uint;
/// One gradient stop.
#[repr(C)]
#[derive(Default, Clone, Copy, Debug)]
pub struct hr_color_stop_t {
    pub offset: f32,
    pub is_foreground: hr_bool_t,
    pub color: hr_color_t,
}
pub type hr_color_line_get_color_stops_func_t = Option<
    unsafe extern "C" fn(
        line: *mut hr_color_line_t,
        data: *mut c_void,
        start: c_uint,
        count: *mut c_uint,
        stops: *mut hr_color_stop_t,
        user_data: *mut c_void,
    ) -> c_uint,
>;
pub type hr_color_line_get_extend_func_t = Option<
    unsafe extern "C" fn(
        line: *mut hr_color_line_t,
        data: *mut c_void,
        user_data: *mut c_void,
    ) -> hr_paint_extend_t,
>;
/// Borrowed gradient information, valid only for the enclosing paint callback.
#[repr(C)]
pub struct hr_color_line_t {
    pub data: *mut c_void,
    pub get_color_stops: hr_color_line_get_color_stops_func_t,
    pub get_color_stops_user_data: *mut c_void,
    pub get_extend: hr_color_line_get_extend_func_t,
    pub get_extend_user_data: *mut c_void,
    reserved: [*mut c_void; 8],
}
/// Returns the total number of stops and copies a paginated slice if requested.
#[no_mangle]
pub unsafe extern "C" fn hr_color_line_get_color_stops(
    line: *mut hr_color_line_t,
    start: c_uint,
    count: *mut c_uint,
    stops: *mut hr_color_stop_t,
) -> c_uint {
    let Some(l) = (unsafe { line.as_ref() }) else {
        if !count.is_null() {
            unsafe { *count = 0 }
        }
        return 0;
    };
    if let Some(call) = l.get_color_stops {
        unsafe {
            call(
                line,
                l.data,
                start,
                count,
                stops,
                l.get_color_stops_user_data,
            )
        }
    } else {
        if !count.is_null() {
            unsafe { *count = 0 }
        }
        0
    }
}
/// Returns the extend mode, or PAD when absent.
#[no_mangle]
pub unsafe extern "C" fn hr_color_line_get_extend(line: *mut hr_color_line_t) -> hr_paint_extend_t {
    let Some(l) = (unsafe { line.as_ref() }) else {
        return HR_PAINT_EXTEND_PAD;
    };
    l.get_extend.map_or(HR_PAINT_EXTEND_PAD, |call| unsafe {
        call(line, l.data, l.get_extend_user_data)
    })
}
/// Applies the font's scale relative to design units.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_push_font_transform(
    funcs: *mut hr_paint_funcs_t,
    data: *mut c_void,
    font: *mut hr_font_t,
) {
    let (x, y) = unsafe { crate::rendering::scale(font) };
    unsafe { hr_paint_push_transform(funcs, data, x, 0.0, 0.0, y, 0.0, 0.0) }
}
/// Applies the inverse font scale; zero axes stay zero.
#[no_mangle]
pub unsafe extern "C" fn hr_paint_push_inverse_font_transform(
    funcs: *mut hr_paint_funcs_t,
    data: *mut c_void,
    font: *mut hr_font_t,
) {
    let (x, y) = unsafe { crate::rendering::scale(font) };
    unsafe {
        hr_paint_push_transform(
            funcs,
            data,
            if x == 0.0 { 0.0 } else { 1.0 / x },
            0.0,
            0.0,
            if y == 0.0 { 0.0 } else { 1.0 / y },
            0.0,
            0.0,
        );
    }
}
pub const HR_PAINT_COMPOSITE_MODE_CLEAR: hr_paint_composite_mode_t = 0;
pub const HR_PAINT_COMPOSITE_MODE_SRC: hr_paint_composite_mode_t = 1;
pub const HR_PAINT_COMPOSITE_MODE_DEST: hr_paint_composite_mode_t = 2;
pub const HR_PAINT_COMPOSITE_MODE_SRC_OVER: hr_paint_composite_mode_t = 3;
pub const HR_PAINT_COMPOSITE_MODE_DEST_OVER: hr_paint_composite_mode_t = 4;
pub const HR_PAINT_COMPOSITE_MODE_SRC_IN: hr_paint_composite_mode_t = 5;
pub const HR_PAINT_COMPOSITE_MODE_DEST_IN: hr_paint_composite_mode_t = 6;
pub const HR_PAINT_COMPOSITE_MODE_SRC_OUT: hr_paint_composite_mode_t = 7;
pub const HR_PAINT_COMPOSITE_MODE_DEST_OUT: hr_paint_composite_mode_t = 8;
pub const HR_PAINT_COMPOSITE_MODE_SRC_ATOP: hr_paint_composite_mode_t = 9;
pub const HR_PAINT_COMPOSITE_MODE_DEST_ATOP: hr_paint_composite_mode_t = 10;
pub const HR_PAINT_COMPOSITE_MODE_XOR: hr_paint_composite_mode_t = 11;
pub const HR_PAINT_COMPOSITE_MODE_PLUS: hr_paint_composite_mode_t = 12;
pub const HR_PAINT_COMPOSITE_MODE_SCREEN: hr_paint_composite_mode_t = 13;
pub const HR_PAINT_COMPOSITE_MODE_OVERLAY: hr_paint_composite_mode_t = 14;
pub const HR_PAINT_COMPOSITE_MODE_DARKEN: hr_paint_composite_mode_t = 15;
pub const HR_PAINT_COMPOSITE_MODE_LIGHTEN: hr_paint_composite_mode_t = 16;
pub const HR_PAINT_COMPOSITE_MODE_COLOR_DODGE: hr_paint_composite_mode_t = 17;
pub const HR_PAINT_COMPOSITE_MODE_COLOR_BURN: hr_paint_composite_mode_t = 18;
pub const HR_PAINT_COMPOSITE_MODE_HARD_LIGHT: hr_paint_composite_mode_t = 19;
pub const HR_PAINT_COMPOSITE_MODE_SOFT_LIGHT: hr_paint_composite_mode_t = 20;
pub const HR_PAINT_COMPOSITE_MODE_DIFFERENCE: hr_paint_composite_mode_t = 21;
pub const HR_PAINT_COMPOSITE_MODE_EXCLUSION: hr_paint_composite_mode_t = 22;
pub const HR_PAINT_COMPOSITE_MODE_MULTIPLY: hr_paint_composite_mode_t = 23;
pub const HR_PAINT_COMPOSITE_MODE_HSL_HUE: hr_paint_composite_mode_t = 24;
pub const HR_PAINT_COMPOSITE_MODE_HSL_SATURATION: hr_paint_composite_mode_t = 25;
pub const HR_PAINT_COMPOSITE_MODE_HSL_COLOR: hr_paint_composite_mode_t = 26;
pub const HR_PAINT_COMPOSITE_MODE_HSL_LUMINOSITY: hr_paint_composite_mode_t = 27;

mod backend;

/// Legacy color-glyph callback, assumed to succeed.
pub type hr_font_paint_glyph_func_t = Option<
    unsafe extern "C" fn(
        font: *mut hr_font_t,
        font_data: *mut c_void,
        glyph: hr_codepoint_t,
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        palette: c_uint,
        foreground: hr_color_t,
        user_data: *mut c_void,
    ),
>;
/// Custom color-glyph callback. Returns true if painting succeeded.
pub type hr_font_paint_glyph_or_fail_func_t = Option<
    unsafe extern "C" fn(
        font: *mut hr_font_t,
        font_data: *mut c_void,
        glyph: hr_codepoint_t,
        funcs: *mut hr_paint_funcs_t,
        paint_data: *mut c_void,
        palette: c_uint,
        foreground: hr_color_t,
        user_data: *mut c_void,
    ) -> hr_bool_t,
>;
/// Registers custom painting, owning its data even on rejection or clearing.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_paint_glyph_func(
    ffuncs: *mut crate::hr_font_funcs_t,
    func: hr_font_paint_glyph_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(ffuncs) }) else {
        unsafe { crate::rendering::reject_data(user_data, destroy) };
        return;
    };
    let old = core::mem::replace(
        &mut state.paint_glyph_legacy,
        callback_taking(func, user_data, destroy),
    );
    let other = state.paint_glyph.take();
    drop(old);
    drop(other);
}
/// Registers custom painting returning success.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_paint_glyph_or_fail_func(
    ffuncs: *mut crate::hr_font_funcs_t,
    func: hr_font_paint_glyph_or_fail_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(ffuncs) }) else {
        unsafe { crate::rendering::reject_data(user_data, destroy) };
        return;
    };
    let old = core::mem::replace(
        &mut state.paint_glyph,
        callback_taking(func, user_data, destroy),
    );
    let other = state.paint_glyph_legacy.take();
    drop(old);
    drop(other);
}
/// Paints COLRv0/v1, SVG, CBDT or sbix through client callbacks.
/// Returns false on malformed data, unsupported images, or paint failure.
/// Pointers must be NULL or live; callbacks must not mutate the font or funcs.
#[no_mangle]
pub unsafe extern "C" fn hr_font_paint_glyph_or_fail(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    funcs: *mut hr_paint_funcs_t,
    data: *mut c_void,
    palette: c_uint,
    foreground: hr_color_t,
) -> hr_bool_t {
    let Some(state) = (unsafe { font.as_ref() }) else {
        return 0;
    };
    if let Some(ffuncs) = unsafe { state.funcs.as_ref() } {
        if let Some(cb) = &ffuncs.paint_glyph_legacy {
            if let Some(call) = cb.func {
                unsafe {
                    call(
                        font,
                        state.callback_data(),
                        glyph,
                        funcs,
                        data,
                        palette,
                        foreground,
                        cb.user_data,
                    );
                }
                return 1;
            }
        }
        if let Some(cb) = &ffuncs.paint_glyph {
            if let Some(call) = cb.func {
                return unsafe {
                    call(
                        font,
                        state.callback_data(),
                        glyph,
                        funcs,
                        data,
                        palette,
                        foreground,
                        cb.user_data,
                    )
                };
            }
        }
        if state.funcs != crate::font_funcs::builtin_funcs() && state.parent.is_null() {
            return 0;
        }
    }
    if state.funcs != crate::font_funcs::builtin_funcs() && !state.parent.is_null() {
        let parent = unsafe { &*state.parent };
        unsafe {
            hr_paint_push_transform(
                funcs,
                data,
                if parent.x_scale == 0 {
                    0.0
                } else {
                    state.x_scale as f32 / parent.x_scale as f32
                },
                0.0,
                0.0,
                if parent.y_scale == 0 {
                    0.0
                } else {
                    state.y_scale as f32 / parent.y_scale as f32
                },
                0.0,
                0.0,
            );
        };
        let result = unsafe {
            hr_font_paint_glyph_or_fail(state.parent, glyph, funcs, data, palette, foreground)
        };
        unsafe { hr_paint_pop_transform(funcs, data) };
        return result;
    }
    unsafe { backend::paint(font, glyph, funcs, data, palette, foreground) }
}
/// Compatibility entry point ignoring the success result.
#[no_mangle]
pub unsafe extern "C" fn hr_font_paint_glyph(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    funcs: *mut hr_paint_funcs_t,
    data: *mut c_void,
    palette: c_uint,
    foreground: hr_color_t,
) {
    unsafe { hr_font_paint_glyph_or_fail(font, glyph, funcs, data, palette, foreground) };
}
