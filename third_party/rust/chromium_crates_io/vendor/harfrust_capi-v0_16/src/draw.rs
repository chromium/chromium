//! Draw callbacks with HarfBuzz-compatible ownership and dispatch.
// The C drawing/painting ABI uses f32 coordinates, including integer font scales.
#![allow(clippy::cast_precision_loss)]
use crate::font_funcs::{callback_taking, Callback};
use crate::object::{self, Empty, Object, ObjectHeader};
use crate::{hr_bool_t, hr_codepoint_t, hr_destroy_func_t, hr_font_t, hr_user_data_key_t};
use core::ffi::c_void;
use std::sync::OnceLock;
/// Callback for move to; data must be safe to use on any thread.
pub type hr_draw_move_to_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_draw_funcs_t,
        draw_data: *mut c_void,
        st: *mut hr_draw_state_t,
        to_x: f32,
        to_y: f32,
        user_data: *mut c_void,
    ),
>;
/// Callback for line to; data must be safe to use on any thread.
pub type hr_draw_line_to_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_draw_funcs_t,
        draw_data: *mut c_void,
        st: *mut hr_draw_state_t,
        to_x: f32,
        to_y: f32,
        user_data: *mut c_void,
    ),
>;
/// Callback for quadratic to; data must be safe to use on any thread.
pub type hr_draw_quadratic_to_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_draw_funcs_t,
        draw_data: *mut c_void,
        st: *mut hr_draw_state_t,
        control_x: f32,
        control_y: f32,
        to_x: f32,
        to_y: f32,
        user_data: *mut c_void,
    ),
>;
/// Callback for cubic to; data must be safe to use on any thread.
pub type hr_draw_cubic_to_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_draw_funcs_t,
        draw_data: *mut c_void,
        st: *mut hr_draw_state_t,
        control1_x: f32,
        control1_y: f32,
        control2_x: f32,
        control2_y: f32,
        to_x: f32,
        to_y: f32,
        user_data: *mut c_void,
    ),
>;
/// Callback for close path; data must be safe to use on any thread.
pub type hr_draw_close_path_func_t = Option<
    unsafe extern "C" fn(
        funcs: *mut hr_draw_funcs_t,
        draw_data: *mut c_void,
        st: *mut hr_draw_state_t,
        user_data: *mut c_void,
    ),
>;
/// A reference-counted, optionally immutable collection of draw callbacks.
#[derive(Default)]
pub struct hr_draw_funcs_t {
    header: ObjectHeader,
    move_to: Option<Callback<hr_draw_move_to_func_t>>,
    line_to: Option<Callback<hr_draw_line_to_func_t>>,
    quadratic_to: Option<Callback<hr_draw_quadratic_to_func_t>>,
    cubic_to: Option<Callback<hr_draw_cubic_to_func_t>>,
    close_path: Option<Callback<hr_draw_close_path_func_t>>,
}
// SAFETY: callback registration requires thread-safe callbacks and opaque data.
unsafe impl Send for hr_draw_funcs_t {}
unsafe impl Sync for hr_draw_funcs_t {}
impl Object for hr_draw_funcs_t {
    fn header(&self) -> &ObjectHeader {
        &self.header
    }
    fn empty() -> *mut Self {
        static EMPTY: OnceLock<Empty<hr_draw_funcs_t>> = OnceLock::new();
        EMPTY
            .get_or_init(|| {
                Empty::new(hr_draw_funcs_t {
                    header: ObjectHeader::immortal(),
                    ..Default::default()
                })
            })
            .get()
    }
}
/// Creates an owned callback collection.
#[no_mangle]
pub extern "C" fn hr_draw_funcs_create() -> *mut hr_draw_funcs_t {
    object::create(hr_draw_funcs_t::default())
}
/// Returns the immortal empty collection.
#[no_mangle]
pub extern "C" fn hr_draw_funcs_get_empty() -> *mut hr_draw_funcs_t {
    hr_draw_funcs_t::empty()
}
/// Takes a reference. Accepts NULL.
#[no_mangle]
pub unsafe extern "C" fn hr_draw_funcs_reference(
    funcs: *mut hr_draw_funcs_t,
) -> *mut hr_draw_funcs_t {
    unsafe { object::reference(funcs) }
}
/// Releases an owned reference and its callback data. Accepts NULL.
#[no_mangle]
pub unsafe extern "C" fn hr_draw_funcs_destroy(funcs: *mut hr_draw_funcs_t) {
    unsafe { object::destroy(funcs) }
}
/// Rejects subsequent callback changes.
#[no_mangle]
pub unsafe extern "C" fn hr_draw_funcs_make_immutable(funcs: *mut hr_draw_funcs_t) {
    unsafe { object::make_immutable(funcs) }
}
/// Reports whether callback changes are forbidden.
#[no_mangle]
pub unsafe extern "C" fn hr_draw_funcs_is_immutable(funcs: *const hr_draw_funcs_t) -> hr_bool_t {
    i32::from(unsafe { object::is_immutable(funcs) })
}
/// Attaches address-keyed metadata, taking ownership only on success.
#[no_mangle]
pub unsafe extern "C" fn hr_draw_funcs_set_user_data(
    funcs: *mut hr_draw_funcs_t,
    key: *const hr_user_data_key_t,
    data: *mut c_void,
    destroy: hr_destroy_func_t,
    replace: hr_bool_t,
) -> hr_bool_t {
    i32::from(unsafe { object::set_user_data(funcs, key, data, destroy, replace != 0) })
}
/// Returns borrowed metadata.
#[no_mangle]
pub unsafe extern "C" fn hr_draw_funcs_get_user_data(
    funcs: *const hr_draw_funcs_t,
    key: *const hr_user_data_key_t,
) -> *mut c_void {
    unsafe { object::get_user_data(funcs.cast_mut(), key) }
}
/// Registers move to. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_draw_funcs_set_move_to_func(
    funcs: *mut hr_draw_funcs_t,
    func: hr_draw_move_to_func_t,
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
        &mut state.move_to,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Registers line to. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_draw_funcs_set_line_to_func(
    funcs: *mut hr_draw_funcs_t,
    func: hr_draw_line_to_func_t,
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
        &mut state.line_to,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Registers quadratic to. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_draw_funcs_set_quadratic_to_func(
    funcs: *mut hr_draw_funcs_t,
    func: hr_draw_quadratic_to_func_t,
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
        &mut state.quadratic_to,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Registers cubic to. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_draw_funcs_set_cubic_to_func(
    funcs: *mut hr_draw_funcs_t,
    func: hr_draw_cubic_to_func_t,
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
        &mut state.cubic_to,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}
/// Registers close path. Takes ownership of data, including when clearing or rejected.
/// Mutation requires exclusive access; funcs must be NULL or live.
#[no_mangle]
pub unsafe extern "C" fn hr_draw_funcs_set_close_path_func(
    funcs: *mut hr_draw_funcs_t,
    func: hr_draw_close_path_func_t,
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
        &mut state.close_path,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}

/// Drawing state. Its layout matches HarfBuzz, including private reserved words.
#[repr(C)]
#[derive(Default, Clone, Copy, Debug)]
pub struct hr_draw_state_t {
    pub path_open: hr_bool_t,
    pub path_start_x: f32,
    pub path_start_y: f32,
    pub current_x: f32,
    pub current_y: f32,
    reserved: [u32; 7],
}
unsafe fn emit_move(
    funcs: *mut hr_draw_funcs_t,
    data: *mut c_void,
    st: *mut hr_draw_state_t,
    x: f32,
    y: f32,
) {
    let f = unsafe { object::or_empty(funcs) };
    if let Some(cb) = &f.move_to {
        if let Some(call) = cb.func {
            unsafe { call(funcs, data, st, x, y, cb.user_data) }
        }
    }
}
unsafe fn emit_line(
    funcs: *mut hr_draw_funcs_t,
    data: *mut c_void,
    st: *mut hr_draw_state_t,
    x: f32,
    y: f32,
) {
    let f = unsafe { object::or_empty(funcs) };
    if let Some(cb) = &f.line_to {
        if let Some(call) = cb.func {
            unsafe { call(funcs, data, st, x, y, cb.user_data) }
        }
    }
}
unsafe fn start(funcs: *mut hr_draw_funcs_t, data: *mut c_void, st: *mut hr_draw_state_t) {
    if st.is_null() {
        return;
    }
    unsafe {
        if (*st).path_open == 0 {
            emit_move(funcs, data, st, (*st).current_x, (*st).current_y);
            (*st).path_open = 1;
            (*st).path_start_x = (*st).current_x;
            (*st).path_start_y = (*st).current_y;
        }
    }
}
/// Begins a contour, closing any open contour. Move callbacks are emitted lazily.
#[no_mangle]
pub unsafe extern "C" fn hr_draw_move_to(
    funcs: *mut hr_draw_funcs_t,
    data: *mut c_void,
    st: *mut hr_draw_state_t,
    to_x: f32,
    to_y: f32,
) {
    if st.is_null() {
        return;
    }
    unsafe {
        if (*st).path_open != 0 {
            hr_draw_close_path(funcs, data, st);
        }
        (*st).current_x = to_x;
        (*st).current_y = to_y;
    }
}
/// Adds a line, opening a contour when needed.
#[no_mangle]
pub unsafe extern "C" fn hr_draw_line_to(
    funcs: *mut hr_draw_funcs_t,
    data: *mut c_void,
    st: *mut hr_draw_state_t,
    to_x: f32,
    to_y: f32,
) {
    if st.is_null() {
        return;
    }
    unsafe {
        start(funcs, data, st);
        emit_line(funcs, data, st, to_x, to_y);
        (*st).current_x = to_x;
        (*st).current_y = to_y;
    }
}
/// Adds a cubic Bezier segment.
#[no_mangle]
pub unsafe extern "C" fn hr_draw_cubic_to(
    funcs: *mut hr_draw_funcs_t,
    data: *mut c_void,
    st: *mut hr_draw_state_t,
    c1x: f32,
    c1y: f32,
    c2x: f32,
    c2y: f32,
    to_x: f32,
    to_y: f32,
) {
    if st.is_null() {
        return;
    }
    unsafe { start(funcs, data, st) };
    let f = unsafe { object::or_empty(funcs) };
    if let Some(cb) = &f.cubic_to {
        if let Some(call) = cb.func {
            unsafe {
                call(
                    funcs,
                    data,
                    st,
                    c1x,
                    c1y,
                    c2x,
                    c2y,
                    to_x,
                    to_y,
                    cb.user_data,
                );
            }
        }
    }
    unsafe {
        (*st).current_x = to_x;
        (*st).current_y = to_y;
    }
}
/// Adds a quadratic segment, converting to cubic when no quadratic callback exists.
#[no_mangle]
pub unsafe extern "C" fn hr_draw_quadratic_to(
    funcs: *mut hr_draw_funcs_t,
    data: *mut c_void,
    st: *mut hr_draw_state_t,
    cx: f32,
    cy: f32,
    to_x: f32,
    to_y: f32,
) {
    if st.is_null() {
        return;
    }
    unsafe { start(funcs, data, st) };
    let f = unsafe { object::or_empty(funcs) };
    if let Some(cb) = &f.quadratic_to {
        if let Some(call) = cb.func {
            unsafe { call(funcs, data, st, cx, cy, to_x, to_y, cb.user_data) }
        }
    } else {
        unsafe {
            hr_draw_cubic_to(
                funcs,
                data,
                st,
                ((*st).current_x + 2.0 * cx) / 3.0,
                ((*st).current_y + 2.0 * cy) / 3.0,
                (to_x + 2.0 * cx) / 3.0,
                (to_y + 2.0 * cy) / 3.0,
                to_x,
                to_y,
            );
        }
    }
    unsafe {
        (*st).current_x = to_x;
        (*st).current_y = to_y;
    }
}
/// Closes a contour, emitting its final line before close when needed.
#[no_mangle]
#[allow(clippy::float_cmp)] // Exact equality determines whether a closing segment exists.
pub unsafe extern "C" fn hr_draw_close_path(
    funcs: *mut hr_draw_funcs_t,
    data: *mut c_void,
    st: *mut hr_draw_state_t,
) {
    if st.is_null() {
        return;
    }
    if unsafe { (*st).path_open } != 0 {
        if unsafe { (*st).current_x != (*st).path_start_x || (*st).current_y != (*st).path_start_y }
        {
            unsafe { hr_draw_line_to(funcs, data, st, (*st).path_start_x, (*st).path_start_y) }
        }
        let f = unsafe { object::or_empty(funcs) };
        if let Some(cb) = &f.close_path {
            if let Some(call) = cb.func {
                unsafe { call(funcs, data, st, cb.user_data) }
            }
        }
    }
    unsafe { st.write(hr_draw_state_t::default()) };
}

/// Legacy callback, assumed to succeed.
pub type hr_font_draw_glyph_func_t = Option<
    unsafe extern "C" fn(
        font: *mut hr_font_t,
        font_data: *mut c_void,
        glyph: hr_codepoint_t,
        funcs: *mut hr_draw_funcs_t,
        draw_data: *mut c_void,
        user_data: *mut c_void,
    ),
>;
/// Custom glyph-outline callback. Returns true if the glyph was drawn.
pub type hr_font_draw_glyph_or_fail_func_t = Option<
    unsafe extern "C" fn(
        font: *mut hr_font_t,
        font_data: *mut c_void,
        glyph: hr_codepoint_t,
        funcs: *mut hr_draw_funcs_t,
        draw_data: *mut c_void,
        user_data: *mut c_void,
    ) -> hr_bool_t,
>;
/// Sets a custom outline callback. Takes ownership of its data even when rejected.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_draw_glyph_func(
    ffuncs: *mut crate::hr_font_funcs_t,
    func: hr_font_draw_glyph_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(ffuncs) }) else {
        unsafe { crate::rendering::reject_data(user_data, destroy) };
        return;
    };
    let old = core::mem::replace(
        &mut state.draw_glyph_legacy,
        callback_taking(func, user_data, destroy),
    );
    let other = state.draw_glyph.take();
    drop(old);
    drop(other);
}
/// Sets a custom outline callback returning success.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_draw_glyph_or_fail_func(
    ffuncs: *mut crate::hr_font_funcs_t,
    func: hr_font_draw_glyph_or_fail_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(state) = (unsafe { object::as_mutable(ffuncs) }) else {
        unsafe { crate::rendering::reject_data(user_data, destroy) };
        return;
    };
    let old = core::mem::replace(
        &mut state.draw_glyph,
        callback_taking(func, user_data, destroy),
    );
    let other = state.draw_glyph_legacy.take();
    drop(old);
    drop(other);
}
/// Draws an unhinted glyph using Skrifa, or the installed font callback.
/// Supported outlines are glyf, CFF, CFF2, and VARC. Returns false on invalid input.
/// All pointers must be NULL or live; callbacks must not mutate the font or funcs.
#[no_mangle]
pub unsafe extern "C" fn hr_font_draw_glyph_or_fail(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    funcs: *mut hr_draw_funcs_t,
    draw_data: *mut c_void,
) -> hr_bool_t {
    let Some(state) = (unsafe { font.as_ref() }) else {
        return 0;
    };
    if let Some(ffuncs) = unsafe { state.funcs.as_ref() } {
        if let Some(cb) = &ffuncs.draw_glyph_legacy {
            if let Some(call) = cb.func {
                unsafe {
                    call(
                        font,
                        state.callback_data(),
                        glyph,
                        funcs,
                        draw_data,
                        cb.user_data,
                    );
                }
                return 1;
            }
        }
        if let Some(cb) = &ffuncs.draw_glyph {
            if let Some(call) = cb.func {
                return unsafe {
                    call(
                        font,
                        state.callback_data(),
                        glyph,
                        funcs,
                        draw_data,
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
        return unsafe { parent_draw(font, state.parent, glyph, funcs, draw_data) };
    }
    let Some(render_face) = (unsafe { crate::hr_font_get_face(font).as_ref() }) else {
        return 0;
    };
    let Some(outlines) = (unsafe { crate::rendering::outlines(render_face) }) else {
        return 0;
    };
    let Some(outline) = outlines.get(read_fonts::types::GlyphId::new(glyph)) else {
        return 0;
    };
    let coords = unsafe { crate::rendering::coords(font) };
    let (x, y) = unsafe { crate::rendering::scale(font) };
    let settings = skrifa::outline::DrawSettings::unhinted(
        skrifa::instance::Size::new(state.upem() as f32),
        coords.as_slice(),
    )
    .with_path_style(skrifa::outline::pen::PathStyle::HarfBuzz);
    let mut pen = crate::rendering::Pen {
        funcs,
        data: draw_data,
        state: hr_draw_state_t::default(),
        start: None,
        current: (0.0, 0.0),
        x,
        y,
    };
    i32::from(outline.draw(settings, &mut pen).is_ok())
}
/// Compatibility entry point ignoring the success result.
#[no_mangle]
pub unsafe extern "C" fn hr_font_draw_glyph(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    funcs: *mut hr_draw_funcs_t,
    draw_data: *mut c_void,
) {
    unsafe { hr_font_draw_glyph_or_fail(font, glyph, funcs, draw_data) };
}

unsafe fn parent_draw(
    font: *mut hr_font_t,
    parent: *mut hr_font_t,
    glyph: hr_codepoint_t,
    funcs: *mut hr_draw_funcs_t,
    data: *mut c_void,
) -> hr_bool_t {
    use crate::rendering::Pen;
    use skrifa::outline::OutlinePen;
    unsafe extern "C" fn forward_move(
        _: *mut hr_draw_funcs_t,
        data: *mut c_void,
        _: *mut hr_draw_state_t,
        x: f32,
        y: f32,
        _: *mut c_void,
    ) {
        unsafe { &mut *data.cast::<Pen>() }.move_to(x, y);
    }
    unsafe extern "C" fn forward_line(
        _: *mut hr_draw_funcs_t,
        data: *mut c_void,
        _: *mut hr_draw_state_t,
        x: f32,
        y: f32,
        _: *mut c_void,
    ) {
        unsafe { &mut *data.cast::<Pen>() }.line_to(x, y);
    }
    unsafe extern "C" fn forward_quad(
        _: *mut hr_draw_funcs_t,
        data: *mut c_void,
        _: *mut hr_draw_state_t,
        a: f32,
        b: f32,
        x: f32,
        y: f32,
        _: *mut c_void,
    ) {
        unsafe { &mut *data.cast::<Pen>() }.quad_to(a, b, x, y);
    }
    unsafe extern "C" fn forward_curve(
        _: *mut hr_draw_funcs_t,
        data: *mut c_void,
        _: *mut hr_draw_state_t,
        a: f32,
        b: f32,
        c: f32,
        d: f32,
        x: f32,
        y: f32,
        _: *mut c_void,
    ) {
        unsafe { &mut *data.cast::<Pen>() }.curve_to(a, b, c, d, x, y);
    }
    unsafe extern "C" fn forward_close(
        _: *mut hr_draw_funcs_t,
        data: *mut c_void,
        _: *mut hr_draw_state_t,
        _: *mut c_void,
    ) {
        unsafe { &mut *data.cast::<Pen>() }.close();
    }
    let forwarding = hr_draw_funcs_create();
    unsafe {
        hr_draw_funcs_set_move_to_func(forwarding, Some(forward_move), core::ptr::null_mut(), None);
        hr_draw_funcs_set_line_to_func(forwarding, Some(forward_line), core::ptr::null_mut(), None);
        hr_draw_funcs_set_quadratic_to_func(
            forwarding,
            Some(forward_quad),
            core::ptr::null_mut(),
            None,
        );
        hr_draw_funcs_set_cubic_to_func(
            forwarding,
            Some(forward_curve),
            core::ptr::null_mut(),
            None,
        );
        hr_draw_funcs_set_close_path_func(
            forwarding,
            Some(forward_close),
            core::ptr::null_mut(),
            None,
        );
    }
    let (sx, sy) = unsafe { crate::rendering::scale(font) };
    let (px, py) = unsafe { crate::rendering::scale(parent) };
    let mut pen = Pen {
        funcs,
        data,
        state: hr_draw_state_t::default(),
        start: None,
        current: (0.0, 0.0),
        x: if px == 0.0 { 0.0 } else { sx / px },
        y: if py == 0.0 { 0.0 } else { sy / py },
    };
    // Stream each segment through the pen as the parent emits it: an outline
    // can be far longer than the font that produced it, so it is never held.
    let result =
        unsafe { hr_font_draw_glyph_or_fail(parent, glyph, forwarding, (&raw mut pen).cast()) };
    unsafe { hr_draw_funcs_destroy(forwarding) };
    result
}
