//! Reusable shape plans and the segment properties that identify them.
//! Mirrors HarfBuzz's `hb-shape-plan.h`.
//!
//! A plan captures everything that can be worked out from a face, a set of
//! segment properties and a list of features, before any text is seen. Building
//! one is the expensive half of shaping, so a plan built once can be executed
//! over many buffers that share those properties.
//!
//! [`hr_shape`](crate::hr_shape) already reuses plans through a per-face cache,
//! so reach for this only when you want to hold a plan yourself.

use core::ffi::{c_char, c_uint, c_void};
use std::sync::{Arc, OnceLock};

use harfrust::font::{Font, NormalizedCoord};
use harfrust::{Direction, Language, Script, ShapePlan};

use crate::buffer::hr_buffer_t;
use crate::common::{
    direction_from_rust, direction_to_rust, hr_bool_t, hr_direction_t, hr_feature_t, hr_language_t,
    hr_script_t, language_from_rust, language_to_rust, script_from_rust, script_to_rust,
    HR_DIRECTION_INVALID,
};
use crate::face::hr_face_t;
use crate::font::hr_font_t;
use crate::object::{self, hr_destroy_func_t, hr_user_data_key_t, Empty, Object, ObjectHeader};
use crate::shape::{collect_features, shaper_list_allows_ot};

/// The direction, script and language a run of text is set in.
///
/// Zeroing this struct leaves every field unset, which is what
/// `HR_SEGMENT_PROPERTIES_DEFAULT` expands to.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct hr_segment_properties_t {
    /// The direction the text is set in.
    pub direction: hr_direction_t,
    /// The script the text is written in.
    pub script: hr_script_t,
    /// The language the text is in, or `NULL` if unset.
    pub language: hr_language_t,
    /// Reserved; always set to `NULL`.
    pub reserved1: *mut c_void,
    /// Reserved; always set to `NULL`.
    pub reserved2: *mut c_void,
}

impl hr_segment_properties_t {
    pub(crate) fn to_rust(self) -> (Direction, Option<Script>, Option<Language>) {
        (
            direction_to_rust(self.direction),
            script_to_rust(self.script),
            // SAFETY: documented as `NULL` or a value from
            // `hr_language_from_string`, which is interned for the process.
            unsafe { language_to_rust(self.language) },
        )
    }
}

/// Returns whether two sets of segment properties are the same.
///
/// # Safety
///
/// Both pointers must be `NULL` or readable.
#[no_mangle]
pub unsafe extern "C" fn hr_segment_properties_equal(
    a: *const hr_segment_properties_t,
    b: *const hr_segment_properties_t,
) -> hr_bool_t {
    let (Some(a), Some(b)) = (unsafe { a.as_ref() }, unsafe { b.as_ref() }) else {
        return core::ptr::eq(a, b).into();
    };
    // Including the reserved fields, which a caller may be using to tell two
    // otherwise identical property sets apart.
    (a.direction == b.direction
        && a.script == b.script
        && a.language == b.language
        && a.reserved1 == b.reserved1
        && a.reserved2 == b.reserved2)
        .into()
}

/// Returns a hash of a set of segment properties.
///
/// # Safety
///
/// `p` must be `NULL` or readable.
#[no_mangle]
pub unsafe extern "C" fn hr_segment_properties_hash(p: *const hr_segment_properties_t) -> c_uint {
    let Some(p) = (unsafe { p.as_ref() }) else {
        return 0;
    };
    let language = p.language as usize as c_uint;
    p.direction
        .wrapping_mul(31)
        .wrapping_add(p.script)
        .wrapping_mul(31)
        .wrapping_add(language)
}

/// Fills in whichever of `p`'s fields are unset from `src`.
///
/// # Safety
///
/// Both pointers must be `NULL` or readable, and `p` must be writable.
#[no_mangle]
pub unsafe extern "C" fn hr_segment_properties_overlay(
    p: *mut hr_segment_properties_t,
    src: *const hr_segment_properties_t,
) {
    if p.is_null() || src.is_null() {
        return;
    }
    // Read before the destination is borrowed: nothing stops a caller from
    // passing the same properties as both, which HarfBuzz allows and which
    // holding a shared and an exclusive reference to at once would not be.
    // SAFETY: checked non-null above, and the caller guarantees readable.
    let src = unsafe { core::ptr::read(src) };
    // SAFETY: as above, and the caller guarantees `p` is writable.
    let p = unsafe { &mut *p };
    // Each property is filled in only while the two still agree on everything
    // before it: text the source describes differently is not text these
    // properties describe, so the rest of it says nothing about this.
    if p.direction == HR_DIRECTION_INVALID {
        p.direction = src.direction;
    }
    if p.direction != src.direction {
        return;
    }
    if p.script == crate::common::HR_SCRIPT_INVALID {
        p.script = src.script;
    }
    if p.script != src.script {
        return;
    }
    if p.language.is_null() {
        p.language = src.language;
    }
}

/// A reusable plan for shaping text with given properties.
pub struct hr_shape_plan_t {
    header: ObjectHeader,
    /// Owned reference to the face the plan was built over.
    face: *mut hr_face_t,
    /// `None` only for the immortal empty plan.
    pub(crate) plan: Option<Arc<ShapePlan>>,
}

impl Drop for hr_shape_plan_t {
    fn drop(&mut self) {
        // SAFETY: the plan owns this reference.
        unsafe { object::destroy(self.face) };
    }
}

static EMPTY_SHAPE_PLAN: OnceLock<Empty<hr_shape_plan_t>> = OnceLock::new();

impl Object for hr_shape_plan_t {
    fn header(&self) -> &ObjectHeader {
        &self.header
    }

    fn empty() -> *mut Self {
        EMPTY_SHAPE_PLAN
            .get_or_init(|| {
                Empty::new(hr_shape_plan_t {
                    header: ObjectHeader::immortal(),
                    face: hr_face_t::empty(),
                    plan: None,
                })
            })
            .get()
    }
}

/// Reads `num_coords` normalized coordinates, or none if `coords` is `NULL`.
///
/// # Safety
///
/// `coords` must be `NULL` or point to `num_coords` readable entries.
unsafe fn collect_coords(coords: *const i32, num_coords: c_uint) -> Vec<NormalizedCoord> {
    if coords.is_null() || num_coords == 0 {
        return Vec::new();
    }
    // SAFETY: the caller guarantees the array is readable.
    unsafe { core::slice::from_raw_parts(coords, num_coords as usize) }
        .iter()
        .map(|coord| NormalizedCoord::from_bits(*coord as i16))
        .collect()
}

/// Builds the font instance a plan is compiled against.
fn instance_for(face: &hr_face_t, coords: &[NormalizedCoord]) -> Option<Font> {
    let font = face.font()?;
    Some(
        font.instance_builder()
            .normalized_coords(coords.iter().copied())
            .build(),
    )
}

/// # Safety
///
/// See [`hr_shape_plan_create2`].
unsafe fn create_plan(
    face: *mut hr_face_t,
    props: *const hr_segment_properties_t,
    user_features: *const hr_feature_t,
    num_user_features: c_uint,
    coords: &[NormalizedCoord],
    shaper_list: *const *const c_char,
    cached: bool,
) -> *mut hr_shape_plan_t {
    if !unsafe { shaper_list_allows_ot(shaper_list) } {
        return hr_shape_plan_t::empty();
    }
    let (Some(face_ref), Some(props)) = (unsafe { face.as_ref() }, unsafe { props.as_ref() })
    else {
        return hr_shape_plan_t::empty();
    };
    let (direction, script, language) = props.to_rust();
    // Compiling a plan requires a direction; HarfBuzz's own planner assumes one
    // too, so refuse rather than guess on the caller's behalf.
    if direction == Direction::Invalid {
        return hr_shape_plan_t::empty();
    }

    let Some(instance) = instance_for(face_ref, coords) else {
        return hr_shape_plan_t::empty();
    };
    let features = unsafe { collect_features(user_features, num_user_features) };

    let plan = if cached {
        face_ref
            .plans
            .get(&instance, direction, script, language.as_ref(), &features)
            .clone()
    } else {
        Arc::new(ShapePlan::new(
            &instance,
            direction,
            script,
            language.as_ref(),
            &features,
        ))
    };

    object::create(hr_shape_plan_t {
        header: ObjectHeader::new(),
        face: unsafe { object::reference(face) },
        plan: Some(plan),
    })
}

/// Builds a plan for shaping text with the given properties over a face.
///
/// Never returns `NULL`; if a plan cannot be built, including when `props`
/// leaves the direction unset, this returns the empty plan.
///
/// # Safety
///
/// `face` must be `NULL` or a live face, `props` must be `NULL` or readable,
/// `user_features` must point to `num_user_features` readable entries, and
/// `shaper_list` must be `NULL` or a `NULL`-terminated array of NUL-terminated
/// strings.
#[no_mangle]
pub unsafe extern "C" fn hr_shape_plan_create(
    face: *mut hr_face_t,
    props: *const hr_segment_properties_t,
    user_features: *const hr_feature_t,
    num_user_features: c_uint,
    shaper_list: *const *const c_char,
) -> *mut hr_shape_plan_t {
    unsafe {
        create_plan(
            face,
            props,
            user_features,
            num_user_features,
            &[],
            shaper_list,
            false,
        )
    }
}

/// Builds a plan for a particular variation of a variable font.
///
/// Coordinates are 2.14 fixed point values in axis order, as
/// `hr_font_get_var_coords_normalized` reports them. They choose the variation
/// the plan is built for; executing it against a font set to another shapes
/// with the font's, as it does in HarfBuzz.
///
/// # Safety
///
/// As [`hr_shape_plan_create`], and `coords` must be `NULL` or point to
/// `num_coords` readable entries.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn hr_shape_plan_create2(
    face: *mut hr_face_t,
    props: *const hr_segment_properties_t,
    user_features: *const hr_feature_t,
    num_user_features: c_uint,
    coords: *const i32,
    num_coords: c_uint,
    shaper_list: *const *const c_char,
) -> *mut hr_shape_plan_t {
    unsafe {
        create_plan(
            face,
            props,
            user_features,
            num_user_features,
            &collect_coords(coords, num_coords),
            shaper_list,
            false,
        )
    }
}

/// Builds a plan, reusing one from the face's cache when an equal plan is
/// already there.
///
/// This is the cache [`hr_shape`](crate::hr_shape) uses, so a plan taken from
/// here costs nothing beyond the lookup once it has been built.
///
/// # Safety
///
/// See [`hr_shape_plan_create`].
#[no_mangle]
pub unsafe extern "C" fn hr_shape_plan_create_cached(
    face: *mut hr_face_t,
    props: *const hr_segment_properties_t,
    user_features: *const hr_feature_t,
    num_user_features: c_uint,
    shaper_list: *const *const c_char,
) -> *mut hr_shape_plan_t {
    unsafe {
        create_plan(
            face,
            props,
            user_features,
            num_user_features,
            &[],
            shaper_list,
            true,
        )
    }
}

/// Builds a plan for a particular variation, reusing one from the face's cache
/// when an equal plan is already there.
///
/// # Safety
///
/// See [`hr_shape_plan_create2`].
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn hr_shape_plan_create_cached2(
    face: *mut hr_face_t,
    props: *const hr_segment_properties_t,
    user_features: *const hr_feature_t,
    num_user_features: c_uint,
    coords: *const i32,
    num_coords: c_uint,
    shaper_list: *const *const c_char,
) -> *mut hr_shape_plan_t {
    unsafe {
        create_plan(
            face,
            props,
            user_features,
            num_user_features,
            &collect_coords(coords, num_coords),
            shaper_list,
            true,
        )
    }
}

/// Returns the immortal empty plan, which shapes nothing.
#[no_mangle]
pub extern "C" fn hr_shape_plan_get_empty() -> *mut hr_shape_plan_t {
    hr_shape_plan_t::empty()
}

/// Increments a plan's reference count.
///
/// # Safety
///
/// `shape_plan` must be `NULL` or a live plan.
#[no_mangle]
pub unsafe extern "C" fn hr_shape_plan_reference(
    shape_plan: *mut hr_shape_plan_t,
) -> *mut hr_shape_plan_t {
    unsafe { object::reference(shape_plan) }
}

/// Decrements a plan's reference count, freeing it at zero.
///
/// # Safety
///
/// `shape_plan` must be `NULL` or a live plan, and the caller must own the
/// reference being released.
#[no_mangle]
pub unsafe extern "C" fn hr_shape_plan_destroy(shape_plan: *mut hr_shape_plan_t) {
    unsafe { object::destroy(shape_plan) };
}

/// Attaches user data to a plan.
///
/// # Safety
///
/// `shape_plan` must be `NULL` or a live plan, and `key` must outlive it.
#[no_mangle]
pub unsafe extern "C" fn hr_shape_plan_set_user_data(
    shape_plan: *mut hr_shape_plan_t,
    key: *const hr_user_data_key_t,
    data: *mut c_void,
    destroy: hr_destroy_func_t,
    replace: hr_bool_t,
) -> hr_bool_t {
    unsafe { object::set_user_data(shape_plan, key, data, destroy, replace != 0) }.into()
}

/// Retrieves user data previously attached to a plan.
///
/// # Safety
///
/// `shape_plan` must be `NULL` or a live plan.
#[no_mangle]
pub unsafe extern "C" fn hr_shape_plan_get_user_data(
    shape_plan: *mut hr_shape_plan_t,
    key: *const hr_user_data_key_t,
) -> *mut c_void {
    unsafe { object::get_user_data(shape_plan, key) }
}

/// Returns the name of the shaper a plan will use, or `NULL` for the empty
/// plan.
///
/// # Safety
///
/// `shape_plan` must be `NULL` or a live plan.
#[no_mangle]
pub unsafe extern "C" fn hr_shape_plan_get_shaper(
    shape_plan: *mut hr_shape_plan_t,
) -> *const c_char {
    let plan = unsafe { object::or_empty(shape_plan.cast_const()) };
    if plan.plan.is_none() {
        return core::ptr::null();
    }
    c"ot".as_ptr()
}

/// Writes the properties a plan was built for into `props`.
///
/// # Safety
///
/// `shape_plan` must be `NULL` or a live plan, and `props` must be `NULL` or
/// writable.
#[no_mangle]
pub unsafe extern "C" fn hr_shape_plan_get_segment_properties(
    shape_plan: *mut hr_shape_plan_t,
    props: *mut hr_segment_properties_t,
) {
    let Some(out) = (unsafe { props.as_mut() }) else {
        return;
    };
    let plan = unsafe { object::or_empty(shape_plan.cast_const()) };
    let Some(inner) = plan.plan.as_ref() else {
        *out = hr_segment_properties_t {
            direction: HR_DIRECTION_INVALID,
            script: crate::common::HR_SCRIPT_INVALID,
            language: core::ptr::null(),
            reserved1: core::ptr::null_mut(),
            reserved2: core::ptr::null_mut(),
        };
        return;
    };
    *out = hr_segment_properties_t {
        direction: direction_from_rust(inner.direction()),
        script: inner
            .script()
            .map_or(crate::common::HR_SCRIPT_INVALID, script_from_rust),
        language: language_from_rust(inner.language()),
        reserved1: core::ptr::null_mut(),
        reserved2: core::ptr::null_mut(),
    };
}

/// Shapes a buffer using a plan that was built beforehand.
///
/// Returns false without shaping if the plan is the empty one, or if `font` or
/// `buffer` is `NULL`. An empty buffer returns true, having nothing to do.
/// If the shaper panics, this returns false and clears the buffer, as
/// `hr_shape_full` does.
///
/// # Aborts
///
/// Using a plan that does not apply aborts the process, as HarfBuzz's
/// assertions do: the plan must have been built over the same face, and for
/// the direction, script and language the buffer carries. These are
/// programming errors rather than conditions to recover from, and shaping
/// through a mismatched plan yields a wrong answer rather than a slow one.
/// Use `hr_shape` if you would rather have a plan chosen for you.
///
/// Variation settings are not among them: HarfBuzz does not compare those,
/// and shapes with the font's whatever the plan was built for.
///
/// # Safety
///
/// `shape_plan`, `font` and `buffer` must be `NULL` or live, and `features`
/// must point to `num_features` readable entries.
#[no_mangle]
pub unsafe extern "C" fn hr_shape_plan_execute(
    shape_plan: *mut hr_shape_plan_t,
    font: *mut hr_font_t,
    buffer: *mut hr_buffer_t,
    features: *const hr_feature_t,
    num_features: c_uint,
) -> hr_bool_t {
    // Nothing to shape is nothing to fail at, whatever plan was handed over
    // and whatever it was built for: HarfBuzz answers an empty buffer before
    // it looks at anything else, the empty plan included.
    if unsafe { buffer.as_ref() }.is_some_and(|buffer| buffer.buffer.is_empty()) {
        return true.into();
    }
    let (Some(plan_ref), Some(font_ref)) =
        (unsafe { shape_plan.as_ref() }, unsafe { font.as_ref() })
    else {
        return false.into();
    };
    let Some(plan) = plan_ref.plan.as_ref() else {
        return false.into();
    };
    // The plan is compiled against one face's tables ...
    assert!(
        font_ref.face() == plan_ref.face,
        "shape plan was built for a different face than this font"
    );
    // The variation the plan was built for is not checked against the
    // font's, as HarfBuzz does not check it either: the font's settings are
    // the ones shaping uses, and a plan built for another variation of the
    // same face still applies to it.
    if font_ref.instance().is_none() {
        return false.into();
    }
    // Take a share of the plan, so that a callback destroying it during
    // shaping cannot pull it out from under the call.
    let plan = Arc::clone(plan);
    let Some(buffer_ref) = (unsafe { object::as_mutable(buffer) }) else {
        return false.into();
    };
    // An empty buffer needs no work, and matches nothing either way.
    if buffer_ref.buffer.is_empty() {
        return true.into();
    }
    assert!(
        plan_matches_buffer(&plan, &buffer_ref.buffer),
        "shape plan properties do not match the buffer: \
         plan is {:?}/{:?}/{:?}, buffer is {:?}/{:?}/{:?}",
        plan.direction(),
        plan.script(),
        plan.language(),
        buffer_ref.buffer.direction(),
        buffer_ref.buffer.script(),
        buffer_ref.buffer.language(),
    );

    let features = unsafe { collect_features(features, num_features) };
    crate::shape::shape_with_plan(font, buffer_ref, &features, &plan)
}

/// Returns whether a buffer carries the properties a plan was built for.
///
/// HarfRust compares a plan's script against the buffer's with an unset script
/// standing in for `Zzzz`, so this normalizes the same way.
fn plan_matches_buffer(plan: &ShapePlan, buffer: &harfrust::Buffer) -> bool {
    let unset = Script::UNKNOWN;
    plan.direction() == buffer.direction()
        && plan.script().unwrap_or(unset) == buffer.script().unwrap_or(unset)
        && plan.language() == buffer.language()
}
