//! Caller-supplied font callbacks, mirroring the parts of HarfBuzz's
//! `hb-font.h` that HarfRust's [`FontFuncs`] trait can back.
//!
//! A callback that is not set falls through to HarfRust's own implementation,
//! which reads the font's tables. This matches HarfBuzz's parent-funcs
//! chaining for the common case of overriding one or two callbacks.

use core::ffi::{c_uint, c_void};
use core::mem::size_of;
use core::ptr;
use std::sync::OnceLock;

use harfrust::ShaperFont;
use harfrust::{Advances, FontFuncs, NominalGlyphs};
use harfrust::{GlyphExtents, GlyphId};

use crate::common::{hr_bool_t, hr_codepoint_t, hr_glyph_extents_t, hr_position_t};
use crate::font::hr_font_t;
use crate::object::{self, hr_destroy_func_t, hr_user_data_key_t, Empty, Object, ObjectHeader};

/// Maps a Unicode scalar value to a glyph. Returns false if there is none.
pub type hr_font_get_nominal_glyph_func_t = Option<
    unsafe extern "C" fn(
        font: *mut hr_font_t,
        font_data: *mut c_void,
        unicode: hr_codepoint_t,
        glyph: *mut hr_codepoint_t,
        user_data: *mut c_void,
    ) -> hr_bool_t,
>;

/// Maps a strided batch of codepoints, stopping at the first missing glyph.
/// Returns the number of consecutive entries mapped.
pub type hr_font_get_nominal_glyphs_func_t = Option<
    unsafe extern "C" fn(
        font: *mut hr_font_t,
        font_data: *mut c_void,
        count: c_uint,
        first_unicode: *const hr_codepoint_t,
        unicode_stride: c_uint,
        first_glyph: *mut hr_codepoint_t,
        glyph_stride: c_uint,
        user_data: *mut c_void,
    ) -> c_uint,
>;

/// Maps a Unicode scalar value and variation selector to a glyph.
pub type hr_font_get_variation_glyph_func_t = Option<
    unsafe extern "C" fn(
        font: *mut hr_font_t,
        font_data: *mut c_void,
        unicode: hr_codepoint_t,
        variation_selector: hr_codepoint_t,
        glyph: *mut hr_codepoint_t,
        user_data: *mut c_void,
    ) -> hr_bool_t,
>;

/// Returns a glyph's advance along the current direction.
pub type hr_font_get_glyph_advance_func_t = Option<
    unsafe extern "C" fn(
        font: *mut hr_font_t,
        font_data: *mut c_void,
        glyph: hr_codepoint_t,
        user_data: *mut c_void,
    ) -> hr_position_t,
>;

/// Fills a strided run of horizontal glyph advances.
pub type hr_font_get_glyph_advances_func_t = Option<
    unsafe extern "C" fn(
        font: *mut hr_font_t,
        font_data: *mut c_void,
        count: c_uint,
        first_glyph: *const hr_codepoint_t,
        glyph_stride: c_uint,
        first_advance: *mut hr_position_t,
        advance_stride: c_uint,
        user_data: *mut c_void,
    ),
>;
pub type hr_font_get_glyph_h_advances_func_t = hr_font_get_glyph_advances_func_t;

/// Returns the spacing adjustment for a pair of glyphs.
pub type hr_font_get_glyph_kerning_func_t = Option<
    unsafe extern "C" fn(
        font: *mut hr_font_t,
        font_data: *mut c_void,
        first_glyph: hr_codepoint_t,
        second_glyph: hr_codepoint_t,
        user_data: *mut c_void,
    ) -> hr_position_t,
>;
pub type hr_font_get_glyph_h_kerning_func_t = hr_font_get_glyph_kerning_func_t;
pub type hr_font_get_glyph_v_kerning_func_t = hr_font_get_glyph_kerning_func_t;
pub type hr_font_get_glyph_h_origin_func_t = hr_font_get_glyph_origin_func_t;

/// Returns a glyph's origin along the current direction.
pub type hr_font_get_glyph_origin_func_t = Option<
    unsafe extern "C" fn(
        font: *mut hr_font_t,
        font_data: *mut c_void,
        glyph: hr_codepoint_t,
        x: *mut hr_position_t,
        y: *mut hr_position_t,
        user_data: *mut c_void,
    ) -> hr_bool_t,
>;

/// Returns a glyph's ink extents.
pub type hr_font_get_glyph_extents_func_t = Option<
    unsafe extern "C" fn(
        font: *mut hr_font_t,
        font_data: *mut c_void,
        glyph: hr_codepoint_t,
        extents: *mut hr_glyph_extents_t,
        user_data: *mut c_void,
    ) -> hr_bool_t,
>;

/// One callback and the data it was registered with.
pub(crate) struct Callback<F> {
    pub(crate) func: F,
    pub(crate) user_data: *mut c_void,
    destroy: hr_destroy_func_t,
}

impl<F> Drop for Callback<F> {
    fn drop(&mut self) {
        if let Some(destroy) = self.destroy {
            // SAFETY: `destroy` was supplied alongside `user_data`.
            unsafe { destroy(self.user_data) };
        }
    }
}

/// Pairs a callback with the data it was given, or releases that data when
/// there is no callback to own it.
///
/// Every setter takes ownership of `user_data`, including the ones clearing a
/// callback: HarfBuzz runs the destructor either way, and a caller that had
/// to know which calls take ownership could not free anything safely.
pub(crate) fn callback_taking<F>(
    func: Option<F>,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) -> Option<Callback<Option<F>>> {
    if func.is_none() {
        if let Some(destroy) = destroy {
            // SAFETY: `destroy` was supplied alongside `user_data`.
            unsafe { destroy(user_data) };
        }
        return None;
    }
    Some(Callback {
        func,
        user_data,
        destroy,
    })
}

/// A set of font callbacks.
#[derive(Default)]
pub struct hr_font_funcs_t {
    header: ObjectHeader,
    nominal_glyph: Option<Callback<hr_font_get_nominal_glyph_func_t>>,
    nominal_glyphs: Option<Callback<hr_font_get_nominal_glyphs_func_t>>,
    variation_glyph: Option<Callback<hr_font_get_variation_glyph_func_t>>,
    h_advance: Option<Callback<hr_font_get_glyph_advance_func_t>>,
    h_advances: Option<Callback<hr_font_get_glyph_h_advances_func_t>>,
    v_advance: Option<Callback<hr_font_get_glyph_advance_func_t>>,
    h_origin: Option<Callback<hr_font_get_glyph_h_origin_func_t>>,
    v_origin: Option<Callback<hr_font_get_glyph_origin_func_t>>,
    h_kerning: Option<Callback<hr_font_get_glyph_h_kerning_func_t>>,
    v_kerning: Option<Callback<hr_font_get_glyph_v_kerning_func_t>>,
    extents: Option<Callback<hr_font_get_glyph_extents_func_t>>,
    #[cfg(feature = "draw")]
    pub(crate) draw_glyph: Option<Callback<crate::draw::hr_font_draw_glyph_or_fail_func_t>>,
    #[cfg(feature = "draw")]
    pub(crate) draw_glyph_legacy: Option<Callback<crate::draw::hr_font_draw_glyph_func_t>>,
    #[cfg(feature = "paint")]
    pub(crate) paint_glyph: Option<Callback<crate::paint::hr_font_paint_glyph_or_fail_func_t>>,
    #[cfg(feature = "paint")]
    pub(crate) paint_glyph_legacy: Option<Callback<crate::paint::hr_font_paint_glyph_func_t>>,
}

// SAFETY: `hr_font_set_funcs` documents that callbacks and their user data
// must be safe to use from any thread.
unsafe impl Send for hr_font_funcs_t {}
unsafe impl Sync for hr_font_funcs_t {}

static EMPTY_FONT_FUNCS: OnceLock<Empty<hr_font_funcs_t>> = OnceLock::new();

impl Object for hr_font_funcs_t {
    fn header(&self) -> &ObjectHeader {
        &self.header
    }

    fn empty() -> *mut Self {
        EMPTY_FONT_FUNCS
            .get_or_init(|| {
                Empty::new(hr_font_funcs_t {
                    header: ObjectHeader::immortal(),
                    ..Default::default()
                })
            })
            .get()
    }
}

/// Creates an empty set of font callbacks.
#[no_mangle]
pub extern "C" fn hr_font_funcs_create() -> *mut hr_font_funcs_t {
    object::create(hr_font_funcs_t {
        header: ObjectHeader::new(),
        ..Default::default()
    })
}

/// The callbacks that read the font's own tables, as an object a font can
/// carry.
///
/// A font holding this reads its own tables; a font holding nothing at all
/// asks its parent, and one holding a funcs object answers from that. Without
/// a third thing to hold, `hr_ot_font_set_funcs` on a sub-font could not be
/// told from a sub-font that had never been given callbacks.
pub(crate) fn builtin_funcs() -> *mut hr_font_funcs_t {
    static BUILTIN_FONT_FUNCS: OnceLock<Empty<hr_font_funcs_t>> = OnceLock::new();
    BUILTIN_FONT_FUNCS
        .get_or_init(|| {
            Empty::new(hr_font_funcs_t {
                header: ObjectHeader::immortal(),
                ..Default::default()
            })
        })
        .get()
}

/// Returns the immortal empty set of font callbacks.
#[no_mangle]
pub extern "C" fn hr_font_funcs_get_empty() -> *mut hr_font_funcs_t {
    hr_font_funcs_t::empty()
}

/// Increments the reference count of a set of font callbacks.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_reference(
    ffuncs: *mut hr_font_funcs_t,
) -> *mut hr_font_funcs_t {
    unsafe { object::reference(ffuncs) }
}

/// Decrements the reference count, freeing at zero and running every
/// registered destroy callback.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live, and the caller must own the reference.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_destroy(ffuncs: *mut hr_font_funcs_t) {
    unsafe { object::destroy(ffuncs) };
}

/// Attaches user data to a set of font callbacks.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live, and `key` must outlive it.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_user_data(
    ffuncs: *mut hr_font_funcs_t,
    key: *const hr_user_data_key_t,
    data: *mut c_void,
    destroy: hr_destroy_func_t,
    replace: hr_bool_t,
) -> hr_bool_t {
    unsafe { object::set_user_data(ffuncs, key, data, destroy, replace != 0) }.into()
}

/// Retrieves user data attached to a set of font callbacks.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_get_user_data(
    ffuncs: *mut hr_font_funcs_t,
    key: *const hr_user_data_key_t,
) -> *mut c_void {
    unsafe { object::get_user_data(ffuncs, key) }
}

/// Marks a set of font callbacks immutable.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_make_immutable(ffuncs: *mut hr_font_funcs_t) {
    unsafe { object::make_immutable(ffuncs) };
}

/// Returns whether a set of font callbacks has been marked immutable.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_is_immutable(ffuncs: *mut hr_font_funcs_t) -> hr_bool_t {
    unsafe { object::is_immutable(ffuncs.cast_const()) }.into()
}

/// Sets the callback mapping a Unicode scalar value to a glyph.
///
/// Takes ownership of `user_data`, releasing it through `destroy` when the
/// callback is replaced or the funcs object is freed. Passing a `NULL` callback
/// clears any previously set one, after which the parent font answers, or
/// nothing does: no glyph, no extents, and an advance of the font's own
/// scale, which is what HarfBuzz answers with. Setting a
/// callback on an immutable object is ignored, and releases `user_data`
/// immediately.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live, and the callback must be safe to call with
/// `user_data` from any thread.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_nominal_glyph_func(
    ffuncs: *mut hr_font_funcs_t,
    func: hr_font_get_nominal_glyph_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(ffuncs) = (unsafe { object::as_mutable(ffuncs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) };
        }
        return;
    };
    let previous = core::mem::replace(
        &mut ffuncs.nominal_glyph,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}

/// Sets the callback mapping a strided batch of Unicode codepoints.
///
/// Takes ownership of `user_data`, releasing it on replacement or destruction.
/// An immutable funcs object rejects the callback and releases its data.
/// A missing batch callback uses the scalar callback before asking the parent.
///
/// # Safety
/// `ffuncs` must be `NULL` or live, and the callback and data must be safe
/// to invoke from any thread.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_nominal_glyphs_func(
    ffuncs: *mut hr_font_funcs_t,
    func: hr_font_get_nominal_glyphs_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(ffuncs) = (unsafe { object::as_mutable(ffuncs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) };
        }
        return;
    };
    let previous = core::mem::replace(
        &mut ffuncs.nominal_glyphs,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}

/// Sets the callback mapping a Unicode scalar value and variation selector
/// to a glyph.
///
/// Takes ownership of `user_data`, releasing it through `destroy` when the
/// callback is replaced or the funcs object is freed. Passing a `NULL` callback
/// clears any previously set one, after which the parent font answers, or
/// nothing does: no glyph, no extents, and an advance of the font's own
/// scale, which is what HarfBuzz answers with. Setting a
/// callback on an immutable object is ignored, and releases `user_data`
/// immediately.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live, and the callback must be safe to call with
/// `user_data` from any thread.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_variation_glyph_func(
    ffuncs: *mut hr_font_funcs_t,
    func: hr_font_get_variation_glyph_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(ffuncs) = (unsafe { object::as_mutable(ffuncs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) };
        }
        return;
    };
    let previous = core::mem::replace(
        &mut ffuncs.variation_glyph,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}

/// Sets the callback returning a glyph's horizontal advance.
///
/// Takes ownership of `user_data`, releasing it through `destroy` when the
/// callback is replaced or the funcs object is freed. Passing a `NULL` callback
/// clears any previously set one, after which the parent font answers, or
/// nothing does: no glyph, no extents, and an advance of the font's own
/// scale, which is what HarfBuzz answers with. Setting a
/// callback on an immutable object is ignored, and releases `user_data`
/// immediately.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live, and the callback must be safe to call with
/// `user_data` from any thread.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_glyph_h_advance_func(
    ffuncs: *mut hr_font_funcs_t,
    func: hr_font_get_glyph_advance_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(ffuncs) = (unsafe { object::as_mutable(ffuncs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) };
        }
        return;
    };
    let previous = core::mem::replace(
        &mut ffuncs.h_advance,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}

/// Installs the batched horizontal advance callback.
/// Takes ownership of `user_data`, including when the callback is cleared.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live; the callback and its data must be thread safe.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_glyph_h_advances_func(
    ffuncs: *mut hr_font_funcs_t,
    func: hr_font_get_glyph_h_advances_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(ffuncs) = (unsafe { object::as_mutable(ffuncs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) };
        }
        return;
    };
    let previous = core::mem::replace(
        &mut ffuncs.h_advances,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}

/// Installs the horizontal origin callback.
/// Takes ownership of `user_data`, including when the callback is cleared.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live; the callback and its data must be thread safe.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_glyph_h_origin_func(
    ffuncs: *mut hr_font_funcs_t,
    func: hr_font_get_glyph_h_origin_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(ffuncs) = (unsafe { object::as_mutable(ffuncs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) };
        }
        return;
    };
    let previous = core::mem::replace(
        &mut ffuncs.h_origin,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}

/// Installs the horizontal kerning callback.
/// Takes ownership of `user_data`, including when the callback is cleared.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live; the callback and its data must be thread safe.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_glyph_h_kerning_func(
    ffuncs: *mut hr_font_funcs_t,
    func: hr_font_get_glyph_h_kerning_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(ffuncs) = (unsafe { object::as_mutable(ffuncs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) };
        }
        return;
    };
    let previous = core::mem::replace(
        &mut ffuncs.h_kerning,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}

/// Installs the vertical kerning callback.
/// Takes ownership of `user_data`, including when the callback is cleared.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live; the callback and its data must be thread safe.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_glyph_v_kerning_func(
    ffuncs: *mut hr_font_funcs_t,
    func: hr_font_get_glyph_v_kerning_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(ffuncs) = (unsafe { object::as_mutable(ffuncs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) };
        }
        return;
    };
    let previous = core::mem::replace(
        &mut ffuncs.v_kerning,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}

/// Sets the callback returning a glyph's vertical advance.
///
/// Takes ownership of `user_data`, releasing it through `destroy` when the
/// callback is replaced or the funcs object is freed. Passing a `NULL` callback
/// clears any previously set one, after which the parent font answers, or
/// nothing does: no glyph, no extents, and an advance of the font's own
/// scale, which is what HarfBuzz answers with. Setting a
/// callback on an immutable object is ignored, and releases `user_data`
/// immediately.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live, and the callback must be safe to call with
/// `user_data` from any thread.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_glyph_v_advance_func(
    ffuncs: *mut hr_font_funcs_t,
    func: hr_font_get_glyph_advance_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(ffuncs) = (unsafe { object::as_mutable(ffuncs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) };
        }
        return;
    };
    let previous = core::mem::replace(
        &mut ffuncs.v_advance,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}

/// Sets the callback returning a glyph's vertical origin.
///
/// Takes ownership of `user_data`, releasing it through `destroy` when the
/// callback is replaced or the funcs object is freed. Passing a `NULL` callback
/// clears any previously set one, after which the parent font answers, or
/// nothing does: no glyph, no extents, and an advance of the font's own
/// scale, which is what HarfBuzz answers with. Setting a
/// callback on an immutable object is ignored, and releases `user_data`
/// immediately.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live, and the callback must be safe to call with
/// `user_data` from any thread.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_glyph_v_origin_func(
    ffuncs: *mut hr_font_funcs_t,
    func: hr_font_get_glyph_origin_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(ffuncs) = (unsafe { object::as_mutable(ffuncs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) };
        }
        return;
    };
    let previous = core::mem::replace(
        &mut ffuncs.v_origin,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}

/// Sets the callback returning a glyph's ink extents.
///
/// Takes ownership of `user_data`, releasing it through `destroy` when the
/// callback is replaced or the funcs object is freed. Passing a `NULL` callback
/// clears any previously set one, after which the parent font answers, or
/// nothing does: no glyph, no extents, and an advance of the font's own
/// scale, which is what HarfBuzz answers with. Setting a
/// callback on an immutable object is ignored, and releases `user_data`
/// immediately.
///
/// # Safety
///
/// `ffuncs` must be `NULL` or live, and the callback must be safe to call with
/// `user_data` from any thread.
#[no_mangle]
pub unsafe extern "C" fn hr_font_funcs_set_glyph_extents_func(
    ffuncs: *mut hr_font_funcs_t,
    func: hr_font_get_glyph_extents_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(ffuncs) = (unsafe { object::as_mutable(ffuncs) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) };
        }
        return;
    };
    let previous = core::mem::replace(
        &mut ffuncs.extents,
        callback_taking(func, user_data, destroy),
    );
    drop(previous);
}

/// Bridges a [`hr_font_funcs_t`] into HarfRust's [`FontFuncs`] trait for the
/// duration of one shaping call.
///
/// A funcs object is authoritative: once one is installed on a font, every
/// callback comes from it, and a callback that was never set yields the same
/// "not available" answer HarfBuzz's nil implementation gives — no glyph, a
/// zero advance, a zero origin, no extents. HarfRust's own table-driven
/// implementation is used only when a font has no funcs object at all.
pub(crate) struct FontFuncsAdapter<'a> {
    /// The C API does not support callbacks mutating or freeing their font, so
    /// the font itself keeps these values alive for the duration of shaping.
    state: &'a hr_font_t,
    font: *mut hr_font_t,
}

/// What came back: either a value, or the fact that nobody in the chain
/// carries callbacks and the font's own tables should answer.
pub(crate) enum Answer<T> {
    Builtin,
    Value(T),
}

/// Where a callback was found, and what it expects to be handed.
enum Resolved<'c, F> {
    /// No font in the chain carries callbacks, so the font's own tables
    /// answer.
    Builtin,
    /// Callbacks were installed somewhere, and none of them answer for this.
    Missing,
    Found {
        font: *mut hr_font_t,
        data: *mut c_void,
        callback: &'c Callback<F>,
        /// The scale the answer will be in, against this font's own.
        scale: (i32, i32),
    },
}

enum NominalFuncs<'a> {
    Builtin,
    Missing,
    Found(*mut hr_font_t, &'a hr_font_t, &'a hr_font_funcs_t),
}

/// Finds the nearest font in the chain whose callbacks answer for one field.
///
/// A funcs object that does not carry the callback delegates to the parent,
/// as HarfBuzz's do, and so does a font carrying no callbacks at all. A chain
/// with none anywhere falls to the built-in implementation, while one that
/// has callbacks but not this one reports nothing available.
macro_rules! resolve {
    ($adapter:expr, $field:ident) => {{
        // Reborrowed, so that a caller holding `&mut self` keeps it.
        let adapter: &FontFuncsAdapter<'_> = &*$adapter;
        let mut font = adapter.font;
        let mut state: &hr_font_t = adapter.state;
        loop {
            // A font reading its own tables answers here, and nothing above
            // it is asked: that is what installing the built-in callbacks on
            // a sub-font means.
            if state.funcs == crate::font_funcs::builtin_funcs() {
                break Resolved::Builtin;
            }
            // SAFETY: each font owns its reference to its callbacks and to
            // its parent, and outlives this adapter.
            if let Some(funcs) = unsafe { state.funcs.as_ref() } {
                if let Some(callback) = funcs.$field.as_ref() {
                    break Resolved::Found {
                        font,
                        data: state.callback_data(),
                        callback,
                        // The scale the answer comes back in, which is the
                        // answering font's own.
                        scale: (state.x_scale, state.y_scale),
                    };
                }
            }
            match unsafe { state.parent.as_ref() } {
                Some(parent) => {
                    font = state.parent;
                    state = parent;
                }
                // Nothing above to ask. A font with callbacks that do not
                // cover this reports nothing available, as HarfBuzz's empty
                // font does; one carrying none at all reads its own tables.
                None => {
                    break if state.funcs.is_null() {
                        Resolved::Builtin
                    } else {
                        Resolved::Missing
                    }
                }
            }
        }
    }};
}

/// Rescales a distance answered in `from` units into `into` units, which is
/// what a parent's answer needs when the sub-font asking is a different size.
fn rescale(value: i32, from: i32, into: i32) -> i32 {
    if from == into || from == 0 {
        return value;
    }
    ((i64::from(value) * i64::from(into)) / i64::from(from)) as i32
}

impl<'a> FontFuncsAdapter<'a> {
    pub(crate) fn new(font: *mut hr_font_t, state: &'a hr_font_t) -> Self {
        Self { state, font }
    }

    // Scalar and batch callbacks are alternatives at each font in the chain.
    // A child's scalar callback must take precedence over a parent's batch.
    fn nominal_funcs(&self) -> NominalFuncs<'_> {
        self.nearest_funcs(|funcs| funcs.nominal_glyph.is_some() || funcs.nominal_glyphs.is_some())
    }

    // As for the nominal glyph callbacks.
    fn h_advance_funcs(&self) -> NominalFuncs<'_> {
        self.nearest_funcs(|funcs| funcs.h_advance.is_some() || funcs.h_advances.is_some())
    }

    fn nearest_funcs(&self, answers: impl Fn(&hr_font_funcs_t) -> bool) -> NominalFuncs<'_> {
        let mut font = self.font;
        let mut state = self.state;
        loop {
            if state.funcs == builtin_funcs() {
                return NominalFuncs::Builtin;
            }
            if let Some(funcs) = unsafe { state.funcs.as_ref() } {
                if answers(funcs) {
                    return NominalFuncs::Found(font, state, funcs);
                }
            }
            match unsafe { state.parent.as_ref() } {
                Some(parent) => {
                    font = state.parent;
                    state = parent;
                }
                None => {
                    return if state.funcs.is_null() {
                        NominalFuncs::Builtin
                    } else {
                        NominalFuncs::Missing
                    }
                }
            }
        }
    }

    /// What the installed nominal-glyph callback answers, or `None` when
    /// there is no such callback to ask.
    ///
    /// Shaping and the public getters both go through here, so that a font
    /// answers the same whichever of them is asking.
    pub(crate) fn call_nominal_glyph(&self, c: u32) -> Option<Answer<hr_codepoint_t>> {
        let (font, state, funcs) = match self.nominal_funcs() {
            NominalFuncs::Builtin => return Some(Answer::Builtin),
            NominalFuncs::Missing => return None,
            NominalFuncs::Found(font, state, funcs) => (font, state, funcs),
        };
        let mut glyph: hr_codepoint_t = 0;
        let found = if let Some(cb) = &funcs.nominal_glyph {
            unsafe {
                cb.func?(
                    font,
                    state.callback_data(),
                    c,
                    ptr::from_mut(&mut glyph),
                    cb.user_data,
                )
            }
        } else {
            let cb = funcs.nominal_glyphs.as_ref()?;
            (unsafe {
                cb.func?(
                    font,
                    state.callback_data(),
                    1,
                    &raw const c,
                    0,
                    &raw mut glyph,
                    0,
                    cb.user_data,
                )
            }) as i32
        };
        (found != 0).then_some(Answer::Value(glyph))
    }

    /// Maps a raw batch through the nearest scalar or batch callback.
    ///
    /// # Safety
    /// The strided arrays must be valid for `count` entries.
    pub(crate) unsafe fn call_nominal_glyphs(
        &self,
        count: c_uint,
        unicodes: *const u32,
        unicode_stride: c_uint,
        glyphs: *mut u32,
        glyph_stride: c_uint,
    ) -> c_uint {
        let resolved = self.nominal_funcs();
        if let NominalFuncs::Found(font, state, funcs) = &resolved {
            if let Some(cb) = &funcs.nominal_glyphs {
                return unsafe {
                    cb.func.unwrap()(
                        *font,
                        state.callback_data(),
                        count,
                        unicodes,
                        unicode_stride,
                        glyphs,
                        glyph_stride,
                        cb.user_data,
                    )
                }
                .min(count);
            }
        }
        for i in 0..count as usize {
            let unicode = unsafe {
                unicodes
                    .byte_add(i * unicode_stride as usize)
                    .read_unaligned()
            };
            let output = glyphs.wrapping_byte_add(i * glyph_stride as usize);
            let glyph = match &resolved {
                NominalFuncs::Builtin => self.state.builtin_nominal_glyph(unicode),
                NominalFuncs::Missing => None,
                NominalFuncs::Found(font, state, funcs) => {
                    let cb = funcs.nominal_glyph.as_ref().unwrap();
                    let mut glyph = 0;
                    let found = unsafe {
                        cb.func.unwrap()(
                            *font,
                            state.callback_data(),
                            unicode,
                            &raw mut glyph,
                            cb.user_data,
                        )
                    };
                    (found != 0).then_some(glyph)
                }
            };
            let Some(glyph) = glyph else {
                return i as c_uint;
            };
            unsafe { output.write_unaligned(glyph) };
        }
        count
    }

    /// As [`FontFuncsAdapter::call_nominal_glyph`], for a variation
    /// selector.
    pub(crate) fn call_variation_glyph(&self, c: u32, vs: u32) -> Option<Answer<hr_codepoint_t>> {
        let (font, data, cb) = match resolve!(self, variation_glyph) {
            Resolved::Builtin => return Some(Answer::Builtin),
            Resolved::Missing => return None,
            Resolved::Found {
                font,
                data,
                callback,
                ..
            } => (font, data, callback),
        };
        let func = cb.func?;
        let mut glyph: hr_codepoint_t = 0;
        // SAFETY: as above.
        let found = unsafe { func(font, data, c, vs, ptr::from_mut(&mut glyph), cb.user_data) };
        (found != 0).then_some(Answer::Value(glyph))
    }

    /// How far a glyph advances horizontally, through whichever callback
    /// answers. There is always an answer: a chain carrying callbacks that do
    /// not cover this reports one em, as HarfBuzz's nil implementation does.
    pub(crate) fn call_h_advance(&self, glyph: u32) -> Answer<i32> {
        let (font, state, funcs) = match self.h_advance_funcs() {
            NominalFuncs::Builtin => return Answer::Builtin,
            // Nothing answers, so every glyph is as wide as the font is
            // tall. HarfBuzz's own answer, and not zero.
            NominalFuncs::Missing => return Answer::Value(self.state.x_scale),
            NominalFuncs::Found(font, state, funcs) => (font, state, funcs),
        };
        // The batch callback answers a scalar query only where there is no
        // scalar callback to ask, so that one written over scalar queries
        // does not ask itself.
        let Some(cb) = &funcs.h_advance else {
            let mut advance = 0;
            unsafe {
                self.call_h_advances(
                    1,
                    &raw const glyph,
                    size_of::<u32>() as c_uint,
                    &raw mut advance,
                    size_of::<i32>() as c_uint,
                );
            }
            return Answer::Value(advance);
        };
        let Some(func) = cb.func else {
            return Answer::Value(0);
        };
        // SAFETY: the callback was registered by the caller for this purpose.
        let advance = unsafe { func(font, state.callback_data(), glyph, cb.user_data) };
        Answer::Value(rescale(advance, state.x_scale, self.state.x_scale))
    }

    /// Calls a batched callback directly on the caller's strided storage.
    /// Returns false when there is no batched callback to use, which includes
    /// a nearer font answering through a scalar one.
    ///
    /// # Safety
    ///
    /// The glyph and advance pointers must describe writable runs of `count`
    /// entries at the given byte strides.
    pub(crate) unsafe fn call_h_advances(
        &self,
        count: c_uint,
        first_glyph: *const u32,
        glyph_stride: c_uint,
        first_advance: *mut i32,
        advance_stride: c_uint,
    ) -> bool {
        let NominalFuncs::Found(font, state, funcs) = self.h_advance_funcs() else {
            return false;
        };
        let Some(cb) = &funcs.h_advances else {
            return false;
        };
        let Some(func) = cb.func else {
            return false;
        };
        unsafe {
            func(
                font,
                state.callback_data(),
                count,
                first_glyph,
                glyph_stride,
                first_advance,
                advance_stride,
                cb.user_data,
            );
        };
        if state.x_scale != self.state.x_scale && state.x_scale != 0 {
            let mut advance = first_advance.cast::<u8>();
            for _ in 0..count {
                let value = unsafe { advance.cast::<i32>().read_unaligned() };
                unsafe {
                    advance.cast::<i32>().write_unaligned(rescale(
                        value,
                        state.x_scale,
                        self.state.x_scale,
                    ));
                };
                advance = advance.wrapping_add(advance_stride as usize);
            }
        }
        true
    }

    pub(crate) fn call_h_origin(&self, glyph: u32) -> Option<(i32, i32)> {
        let (font, data, cb, scale) = match resolve!(self, h_origin) {
            Resolved::Builtin => return Some((0, 0)),
            Resolved::Missing => return None,
            Resolved::Found {
                font,
                data,
                callback,
                scale,
            } => (font, data, callback, scale),
        };
        let func = cb.func?;
        let (mut x, mut y) = (0, 0);
        let found = unsafe { func(font, data, glyph, &raw mut x, &raw mut y, cb.user_data) };
        (found != 0).then_some((
            rescale(x, scale.0, self.state.x_scale),
            rescale(y, scale.1, self.state.y_scale),
        ))
    }

    pub(crate) fn call_kerning(&self, first: u32, second: u32, vertical: bool) -> i32 {
        let resolved = if vertical {
            resolve!(self, v_kerning)
        } else {
            resolve!(self, h_kerning)
        };
        let Resolved::Found {
            font,
            data,
            callback: cb,
            scale,
        } = resolved
        else {
            return 0;
        };
        let Some(func) = cb.func else {
            return 0;
        };
        let value = unsafe { func(font, data, first, second, cb.user_data) };
        if vertical {
            rescale(value, scale.1, self.state.y_scale)
        } else {
            rescale(value, scale.0, self.state.x_scale)
        }
    }

    /// As [`FontFuncsAdapter::call_h_advance`], downwards.
    pub(crate) fn call_v_advance(&self, glyph: u32) -> Answer<i32> {
        let (font, data, cb, scale) = match resolve!(self, v_advance) {
            Resolved::Builtin => return Answer::Builtin,
            Resolved::Missing => return Answer::Value(-self.state.y_scale),
            Resolved::Found {
                font,
                data,
                callback,
                scale,
            } => (font, data, callback, scale),
        };
        let Some(func) = cb.func else {
            return Answer::Value(0);
        };
        // SAFETY: as above.
        let advance = unsafe { func(font, data, glyph, cb.user_data) };
        Answer::Value(rescale(advance, scale.1, self.state.y_scale))
    }

    /// Where a glyph hangs from when text runs vertically, through whichever
    /// callback answers.
    ///
    /// The inner `None` is HarfBuzz's nil answer: no origin available, which
    /// shaping reads as the origin being nowhere and the public getter
    /// reports as false.
    pub(crate) fn call_v_origin(&self, glyph: u32) -> Answer<Option<(i32, i32)>> {
        let (font, data, cb, scale) = match resolve!(self, v_origin) {
            Resolved::Builtin => return Answer::Builtin,
            Resolved::Missing => return Answer::Value(None),
            Resolved::Found {
                font,
                data,
                callback,
                scale,
            } => (font, data, callback, scale),
        };
        let Some(func) = cb.func else {
            return Answer::Value(None);
        };
        let (mut x, mut y) = (0, 0);
        // SAFETY: as above.
        let found = unsafe {
            func(
                font,
                data,
                glyph,
                ptr::from_mut(&mut x),
                ptr::from_mut(&mut y),
                cb.user_data,
            )
        };
        if found == 0 {
            return Answer::Value(None);
        }
        Answer::Value(Some((
            rescale(x, scale.0, self.state.x_scale),
            rescale(y, scale.1, self.state.y_scale),
        )))
    }

    /// What a glyph's extents are, through whichever callback answers.
    pub(crate) fn call_extents(&self, glyph: u32) -> Option<Answer<hr_glyph_extents_t>> {
        let (font, data, cb, scale) = match resolve!(self, extents) {
            Resolved::Builtin => return Some(Answer::Builtin),
            Resolved::Missing => return None,
            Resolved::Found {
                font,
                data,
                callback,
                scale,
            } => (font, data, callback, scale),
        };
        let func = cb.func?;
        let mut extents = hr_glyph_extents_t::default();
        // SAFETY: as above.
        let found = unsafe { func(font, data, glyph, ptr::from_mut(&mut extents), cb.user_data) };
        if found == 0 {
            return None;
        }
        extents.x_bearing = rescale(extents.x_bearing, scale.0, self.state.x_scale);
        extents.width = rescale(extents.width, scale.0, self.state.x_scale);
        extents.y_bearing = rescale(extents.y_bearing, scale.1, self.state.y_scale);
        extents.height = rescale(extents.height, scale.1, self.state.y_scale);
        Some(Answer::Value(extents))
    }
}

impl FontFuncs for FontFuncsAdapter<'_> {
    fn nominal_glyph(&self, font: &ShaperFont, c: u32) -> Option<GlyphId> {
        match self.call_nominal_glyph(c)? {
            Answer::Builtin => font.default_nominal_glyph(c),
            Answer::Value(glyph) => Some(GlyphId::from(glyph)),
        }
    }

    fn nominal_glyphs(&self, _font: &ShaperFont, glyphs: NominalGlyphs<'_>) -> usize {
        let raw = glyphs.into_raw();
        (unsafe {
            self.call_nominal_glyphs(
                raw.len as c_uint,
                raw.codepoints,
                raw.codepoint_stride as c_uint,
                raw.glyphs,
                raw.glyph_stride as c_uint,
            )
        }) as usize
    }

    fn variation_glyph(&self, font: &ShaperFont, c: u32, vs: u32) -> Option<GlyphId> {
        match self.call_variation_glyph(c, vs)? {
            Answer::Builtin => font.default_variation_glyph(c, vs),
            Answer::Value(glyph) => Some(GlyphId::from(glyph)),
        }
    }

    fn glyph_h_advance(&self, font: &ShaperFont, glyph: GlyphId) -> i32 {
        match self.call_h_advance(glyph.to_u32()) {
            Answer::Builtin => font.default_glyph_h_advance(glyph),
            Answer::Value(advance) => advance,
        }
    }

    fn glyph_h_advances(&self, font: &ShaperFont, advances: Advances<'_>) {
        let raw = advances.into_raw();
        if unsafe {
            self.call_h_advances(
                raw.len as c_uint,
                raw.gids,
                raw.gid_stride as c_uint,
                raw.advances,
                raw.advance_stride as c_uint,
            )
        } {
            return;
        }
        for index in 0..raw.len {
            let glyph = unsafe {
                raw.gids
                    .cast::<u8>()
                    .add(index * raw.gid_stride as usize)
                    .cast::<u32>()
                    .read_unaligned()
            };
            let advance = self.glyph_h_advance(font, GlyphId::from(glyph));
            unsafe {
                raw.advances
                    .cast::<u8>()
                    .add(index * raw.advance_stride as usize)
                    .cast::<i32>()
                    .write_unaligned(advance);
            };
        }
    }

    fn glyph_v_advance(&self, font: &ShaperFont, glyph: GlyphId) -> i32 {
        match self.call_v_advance(glyph.to_u32()) {
            Answer::Builtin => font.default_glyph_v_advance(glyph),
            Answer::Value(advance) => advance,
        }
    }

    fn glyph_v_origin(&self, font: &ShaperFont, glyph: GlyphId) -> (i32, i32) {
        match self.call_v_origin(glyph.to_u32()) {
            Answer::Builtin => font.default_glyph_v_origin(glyph),
            // Nowhere in particular is the origin shaping uses.
            Answer::Value(origin) => origin.unwrap_or((0, 0)),
        }
    }

    fn glyph_extents(&self, font: &ShaperFont, glyph: GlyphId) -> Option<GlyphExtents> {
        match self.call_extents(glyph.to_u32())? {
            Answer::Builtin => font.default_glyph_extents(glyph),
            Answer::Value(extents) => Some(GlyphExtents {
                x_bearing: extents.x_bearing,
                y_bearing: extents.y_bearing,
                width: extents.width,
                height: extents.height,
            }),
        }
    }
}
