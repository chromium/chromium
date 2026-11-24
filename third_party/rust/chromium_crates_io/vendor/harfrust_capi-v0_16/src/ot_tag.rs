//! Conversion between Unicode script/language values and OpenType tags.

use core::ffi::c_uint;

use harfrust::{Language, Script, Tag};

use crate::common::{
    hr_language_t, hr_script_t, hr_tag_t, language_from_owned, language_ref, script_to_rust,
    tag_from_rust, tag_to_rust,
};

/// Converts an OpenType language-system tag to an interned language.
#[no_mangle]
pub extern "C" fn hr_ot_tag_to_language(tag: hr_tag_t) -> hr_language_t {
    language_from_owned(Language::from_tag(tag_to_rust(tag)))
}

/// Converts a script and language to their preferred OpenType tags.
///
/// # Safety
/// `language` must be NULL or an interned language returned by this API.
/// Each non-NULL output array must have the capacity in its associated count.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_tags_from_script_and_language(
    script: hr_script_t,
    language: hr_language_t,
    script_count: *mut c_uint,
    script_tags: *mut hr_tag_t,
    language_count: *mut c_uint,
    language_tags: *mut hr_tag_t,
) {
    let language = unsafe { language_ref(language) };
    let (scripts, languages) = Script::tags_for_language(script_to_rust(script), language);
    unsafe {
        write_tags(scripts.as_slice(), script_count, script_tags);
        write_tags(languages.as_slice(), language_count, language_tags);
    }
}

/// Matches HarfBuzz's in/out count behavior: a missing or zero-capacity
/// output leaves the count untouched.
unsafe fn write_tags(tags: &[Tag], count: *mut c_uint, output: *mut hr_tag_t) {
    let Some(count) = (unsafe { count.as_mut() }) else {
        return;
    };
    if output.is_null() || *count == 0 {
        return;
    }
    let written = (*count as usize).min(tags.len());
    for (index, tag) in tags[..written].iter().copied().enumerate() {
        unsafe { *output.add(index) = tag_from_rust(tag) };
    }
    *count = written as c_uint;
}
