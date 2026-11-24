//! Shaping, mirroring HarfBuzz's `hb-shape.h`.

use core::ffi::{c_char, c_uint};
use core::ptr;
use std::panic::{catch_unwind, AssertUnwindSafe};

use harfrust::{shape, Buffer, Direction, Feature, ShapeOptions};
use smallvec::SmallVec;

use crate::buffer::{hr_buffer_t, CStrArray};
use crate::common::{hr_bool_t, hr_feature_t};
use crate::font::hr_font_t;
use crate::font_funcs::FontFuncsAdapter;
use crate::object;

/// Runs `f` over `buffer`, catching any panic rather than letting it unwind
/// into C, which would be undefined behaviour. Returns `None` if it panicked.
///
/// A panic also clears the buffer. The shaper rewrites it in place, labelling
/// it as glyphs long before it is done, so what an unwind leaves behind is
/// neither the text that went in nor glyphs for it, and a caller told that
/// shaping did not run would abort on that label by shaping it again.
fn guard<T>(buffer: &mut Buffer, f: impl FnOnce(&mut Buffer) -> T) -> Option<T> {
    let outcome = catch_unwind(AssertUnwindSafe(|| f(&mut *buffer))).ok();
    if outcome.is_none() {
        buffer.clear();
    }
    outcome
}

/// Reads `num_features` features, or none if `features` is `NULL`.
/// Most shaping calls use only a handful of features; keep them inline.
///
/// # Safety
///
/// `features` must be `NULL` or point to `num_features` readable entries.
pub(crate) unsafe fn collect_features(
    features: *const hr_feature_t,
    num_features: c_uint,
) -> SmallVec<[Feature; 4]> {
    if features.is_null() || num_features == 0 {
        return SmallVec::new();
    }
    // SAFETY: the caller guarantees the array is readable.
    unsafe { core::slice::from_raw_parts(features, num_features as usize) }
        .iter()
        .map(|feature| Feature::from(*feature))
        .collect()
}

/// Shapes a buffer with a plan the caller has already chosen.
///
/// `font` is the pointer the font callbacks are handed; `font_ref` is that same
/// font, borrowed.
pub(crate) fn shape_with_plan(
    font: *mut hr_font_t,
    buffer_ref: &mut hr_buffer_t,
    features: &[Feature],
    plan: &harfrust::ShapePlan,
) -> hr_bool_t {
    let Some(font_ref) = (unsafe { font.as_ref() }) else {
        return false.into();
    };
    let Some(shaper) = font_ref.shaper() else {
        return false.into();
    };
    let has_funcs = font_ref.has_callbacks();
    let adapter = FontFuncsAdapter::new(font, font_ref);

    guard(&mut buffer_ref.buffer, |buffer| {
        // The prepared view already has this font's scale. Only callbacks
        // need a temporary view, because the adapter borrows this call.
        let shaping_font = has_funcs.then(|| (*shaper).clone().with_font_funcs(Some(&adapter)));
        let shaping_font = shaping_font.as_ref().unwrap_or(shaper);
        let mut options = ShapeOptions::new().features(features).plan(Some(plan));
        if font_ref.ptem > 0.0 {
            options = options.point_size(Some(font_ref.ptem));
        }
        shape(shaping_font, buffer, options)
    })
    .is_some_and(|result| result.is_ok())
    .into()
}

/// Shapes a buffer with a font, applying the given features.
///
/// The buffer's direction, script and language must be set beforehand; call
/// `hr_buffer_guess_segment_properties` to fill in whatever is missing, which
/// this does for the direction on your behalf. On return the buffer holds
/// glyphs, or nothing if the shaper panicked; see [`hr_shape_full`].
///
/// Call `hr_buffer_allocation_successful` if you want to know whether the
/// buffer ran out of room.
///
/// # Aborts
///
/// See [`hr_shape_full`].
///
/// # Safety
///
/// `font` and `buffer` must be `NULL` or live, and `features` must point to
/// `num_features` readable entries.
#[no_mangle]
pub unsafe extern "C" fn hr_shape(
    font: *mut hr_font_t,
    buffer: *mut hr_buffer_t,
    features: *const hr_feature_t,
    num_features: c_uint,
) {
    unsafe { hr_shape_full(font, buffer, features, num_features, ptr::null()) };
}

/// Shapes a buffer, selecting from a list of shaper names.
///
/// This library has a single shaper, named "ot", so `shaper_list` is honoured
/// only to the extent of failing when it does not name it. An empty list
/// names no available shaper and so fails too, as it does in HarfBuzz;
/// `NULL` asks for no particular shaper and always succeeds.
///
/// Returns false only when no shaper could be run: `shaper_list` names none
/// that this library provides, or there is nothing to shape with. Shaping
/// itself never fails the call, which is what HarfBuzz does -- its
/// `hb_shape_full` reports failure only when it cannot find or instantiate a
/// shaper, and every shaper it does run returns true whatever became of the
/// buffer. So a buffer that ran past its length, operation or nesting limits
/// still returns true; call `hr_buffer_allocation_successful` to detect that.
///
/// A panic inside the shaper is the one other way to get false. That is a
/// bug in this library rather than a property of the input, and the buffer
/// comes back cleared, as by `hr_buffer_clear_contents`: what the shaper
/// left in it was neither the text nor glyphs for it, and a buffer still
/// labelled as glyphs could not be shaped again.
///
/// # Aborts
///
/// Misusing the API aborts the process, as HarfBuzz's assertions do: passing a
/// buffer that already holds glyphs, or a font with nothing to shape with.
/// `hr_shape` returns nothing and so could not otherwise report these at
/// all.
///
/// # Safety
///
/// `font` and `buffer` must be `NULL` or live, `features` must point to
/// `num_features` readable entries, and `shaper_list` must be `NULL` or a
/// `NULL`-terminated array of NUL-terminated strings.
#[no_mangle]
pub unsafe extern "C" fn hr_shape_full(
    font: *mut hr_font_t,
    buffer: *mut hr_buffer_t,
    features: *const hr_feature_t,
    num_features: c_uint,
    shaper_list: *const *const c_char,
) -> hr_bool_t {
    let Some(buffer_ro) = (unsafe { buffer.as_ref() }) else {
        return false.into();
    };
    // Nothing to shape is nothing to fail at, whatever else was asked for:
    // HarfBuzz answers an empty buffer before it looks at anything else.
    if buffer_ro.buffer.is_empty() {
        return true.into();
    }
    let Some(buffer_ref) = (unsafe { object::as_mutable(buffer) }) else {
        return false.into();
    };
    if !unsafe { shaper_list_allows_ot(shaper_list) } {
        return false.into();
    }
    let features = unsafe { collect_features(features, num_features) };

    let Some(font_ref) = (unsafe { font.as_ref() }) else {
        return false.into();
    };
    let Some(instance) = font_ref.instance() else {
        return false.into();
    };
    let Some(shaper) = font_ref.shaper() else {
        return false.into();
    };
    // The plan cache lives on the face, which outlives the font.
    let face = font_ref.face();
    let has_funcs = font_ref.has_callbacks();
    let adapter = FontFuncsAdapter::new(font, font_ref);

    let outcome = guard(&mut buffer_ref.buffer, |buffer| {
        // Building a plan requires a direction; HarfBuzz tolerates an unset one,
        // so fill in whatever the caller left out rather than failing.
        if buffer.direction() == Direction::Invalid {
            crate::buffer::guess_segment_properties(buffer);
        }

        // Reuse a plan from the face's cache, as HarfBuzz's `hb_shape` does.
        let plan = unsafe { face.as_ref() }.map(|face| {
            face.plans.get(
                instance,
                buffer.direction(),
                buffer.script(),
                buffer.language(),
                &features,
            )
        });

        // The prepared view already has this font's scale. Only callbacks
        // need a temporary view, because the adapter borrows this call.
        let shaping_font = has_funcs.then(|| (*shaper).clone().with_font_funcs(Some(&adapter)));
        let shaping_font = shaping_font.as_ref().unwrap_or(shaper);
        let mut options = ShapeOptions::new()
            .features(&features)
            .plan(plan.map(|plan| &**plan));
        if font_ref.ptem > 0.0 {
            options = options.point_size(Some(font_ref.ptem));
        }
        shape(shaping_font, buffer, options)
    });

    match outcome {
        // A panic that unwound out of the shaper; `guard` has cleared the
        // buffer. Nothing useful to say.
        None => false.into(),
        // Shaping ran. Whether it gave up partway through on pathological
        // input does not change the answer: HarfBuzz's shapers return true
        // whatever became of the buffer, and a caller that cares about it
        // reads `hr_buffer_allocation_successful`.
        Some(Ok(())) => true.into(),
        // Every way shaping can fail is a misuse of the API, which HarfBuzz
        // asserts on. Aborting rather than returning false means `hr_shape`,
        // which cannot report anything, does not swallow it.
        Some(Err(err)) => panic!("hr_shape: {err}"),
    }
}

/// Returns whether `shaper_list` permits this library's shaper.
///
/// `NULL` permits it; any list, including an empty one, permits it only by
/// naming "ot".
///
/// # Safety
///
/// `shaper_list` must be `NULL` or a `NULL`-terminated array of
/// NUL-terminated strings.
pub(crate) unsafe fn shaper_list_allows_ot(shaper_list: *const *const c_char) -> bool {
    if shaper_list.is_null() {
        return true;
    }
    // Walk the caller's pointer rather than a reference to its first element,
    // whose provenance would not cover the rest of the array.
    let mut entry = shaper_list;
    loop {
        let name = unsafe { *entry };
        if name.is_null() {
            break;
        }
        if let Ok(name) = unsafe { core::ffi::CStr::from_ptr(name) }.to_str() {
            if name == "ot" {
                return true;
            }
        }
        entry = unsafe { entry.add(1) };
    }
    // A list that named shapers, none of them ours, rules us out -- and so
    // does one that named none at all, which HarfBuzz reads as naming no
    // available shaper rather than as placing no restriction.
    false
}

/// Returns the shapers this library provides, as a `NULL`-terminated array of
/// names.
#[no_mangle]
pub extern "C" fn hr_shape_list_shapers() -> *mut *const c_char {
    static SHAPERS: CStrArray<2> = CStrArray([c"ot".as_ptr(), ptr::null()]);
    SHAPERS.0.as_ptr().cast_mut()
}

/// The major version of this library.
///
/// These are literals so that they reach the generated header, and a test
/// keeps them in step with the crate version.
pub const HR_VERSION_MAJOR: c_uint = 0;
/// The minor version of this library.
pub const HR_VERSION_MINOR: c_uint = 16;
/// The micro version of this library.
pub const HR_VERSION_MICRO: c_uint = 0;
/// The version of this library, as a string.
pub const HR_VERSION_STRING: &str = "0.16.0";

/// The HarfBuzz release matched by the shaping implementation.
pub const HR_HARFBUZZ_VERSION_MAJOR: c_uint = 15;
/// The minor version of the matched HarfBuzz release.
pub const HR_HARFBUZZ_VERSION_MINOR: c_uint = 1;
/// The micro version of the matched HarfBuzz release.
pub const HR_HARFBUZZ_VERSION_MICRO: c_uint = 0;

/// Returns the HarfBuzz version matched by the shaping implementation.
///
/// This is a behavior/version baseline, not a promise that every HarfBuzz
/// API exists. The supported surface is declared in `hr.h`.
///
/// # Safety
/// Each output must be `NULL` or writable.
#[no_mangle]
pub unsafe extern "C" fn hr_harfbuzz_version(
    major: *mut c_uint,
    minor: *mut c_uint,
    micro: *mut c_uint,
) {
    for (out, value) in [
        (major, HR_HARFBUZZ_VERSION_MAJOR),
        (minor, HR_HARFBUZZ_VERSION_MINOR),
        (micro, HR_HARFBUZZ_VERSION_MICRO),
    ] {
        if let Some(out) = unsafe { out.as_mut() } {
            *out = value;
        }
    }
}

/// Returns the matched HarfBuzz version as a zero-terminated string.
#[no_mangle]
pub extern "C" fn hr_harfbuzz_version_string() -> *const c_char {
    c"15.1.0".as_ptr()
}

/// Returns whether the matched HarfBuzz version is at least the given version.
#[no_mangle]
pub extern "C" fn hr_harfbuzz_version_atleast(
    major: c_uint,
    minor: c_uint,
    micro: c_uint,
) -> hr_bool_t {
    ([
        HR_HARFBUZZ_VERSION_MAJOR,
        HR_HARFBUZZ_VERSION_MINOR,
        HR_HARFBUZZ_VERSION_MICRO,
    ] >= [major, minor, micro])
    .into()
}

/// Returns the version of the underlying HarfRust library.
///
/// # Safety
///
/// Each of `major`, `minor` and `micro` must be `NULL` or writable.
#[no_mangle]
pub unsafe extern "C" fn hr_version(major: *mut c_uint, minor: *mut c_uint, micro: *mut c_uint) {
    let parse = |s: &str| s.parse::<c_uint>().unwrap_or(0);
    let mut parts = env!("CARGO_PKG_VERSION").split('.');
    let values = [
        parts.next().map_or(0, parse),
        parts.next().map_or(0, parse),
        parts.next().map_or(0, parse),
    ];
    for (out, value) in [major, minor, micro].into_iter().zip(values) {
        if let Some(out) = unsafe { out.as_mut() } {
            *out = value;
        }
    }
}

/// Returns the version of the underlying HarfRust library as a string.
#[no_mangle]
pub extern "C" fn hr_version_string() -> *const c_char {
    // Kept in step with the crate version, NUL-terminated for C.
    concat!(env!("CARGO_PKG_VERSION"), "\0")
        .as_ptr()
        .cast::<c_char>()
}

/// Returns whether the library is at least the given version.
#[no_mangle]
pub extern "C" fn hr_version_atleast(major: c_uint, minor: c_uint, micro: c_uint) -> hr_bool_t {
    let mut have = [0; 3];
    // SAFETY: all three pointers are valid.
    unsafe {
        hr_version(
            ptr::from_mut(&mut have[0]),
            ptr::from_mut(&mut have[1]),
            ptr::from_mut(&mut have[2]),
        );
    };
    (have >= [major, minor, micro]).into()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn version_constants_match_the_crate() {
        assert_eq!(
            format!("{HR_VERSION_MAJOR}.{HR_VERSION_MINOR}.{HR_VERSION_MICRO}"),
            env!("CARGO_PKG_VERSION"),
        );
        assert_eq!(HR_VERSION_STRING, env!("CARGO_PKG_VERSION"));
    }

    /// A caller told that shaping failed must be able to shape the buffer
    /// again, rather than abort on one left labelled as glyphs.
    #[test]
    fn guard_clears_a_buffer_the_shaper_panicked_in() {
        let mut buffer = Buffer::new();
        buffer.push('a' as u32, 0);
        let outcome = guard(&mut buffer, |buffer| {
            buffer.set_content_type(Some(harfrust::ContentType::Glyphs));
            panic!("shaper bug");
        });
        assert!(outcome.is_none());
        assert!(buffer.is_empty());
        assert_eq!(buffer.content_type(), None);
    }

    fn harfbuzz_version() -> String {
        format!(
            "{HR_HARFBUZZ_VERSION_MAJOR}.{HR_HARFBUZZ_VERSION_MINOR}.{HR_HARFBUZZ_VERSION_MICRO}"
        )
    }

    /// The header is generated and committed, so it can fall behind the crate.
    #[test]
    fn generated_header_matches_the_crate_version() {
        let header = std::fs::read_to_string(concat!(env!("CARGO_MANIFEST_DIR"), "/include/hr.h"))
            .expect("include/hr.h should be committed alongside the crate");

        for (name, value) in [
            ("HR_VERSION_MAJOR", HR_VERSION_MAJOR.to_string()),
            ("HR_VERSION_MINOR", HR_VERSION_MINOR.to_string()),
            ("HR_VERSION_MICRO", HR_VERSION_MICRO.to_string()),
            ("HR_VERSION_STRING", format!("{HR_VERSION_STRING:?}")),
            (
                "HR_HARFBUZZ_VERSION_MAJOR",
                HR_HARFBUZZ_VERSION_MAJOR.to_string(),
            ),
            (
                "HR_HARFBUZZ_VERSION_MINOR",
                HR_HARFBUZZ_VERSION_MINOR.to_string(),
            ),
            (
                "HR_HARFBUZZ_VERSION_MICRO",
                HR_HARFBUZZ_VERSION_MICRO.to_string(),
            ),
            (
                "HR_HARFBUZZ_VERSION_STRING",
                format!("{:?}", harfbuzz_version()),
            ),
        ] {
            let expected = format!("#define {name} {value}");
            assert!(
                header.contains(&expected),
                "include/hr.h is out of date: expected `{expected}`.                  Regenerate it with cbindgen, then run                  scripts/gen-hb-compat-header.py."
            );
        }
    }
}
