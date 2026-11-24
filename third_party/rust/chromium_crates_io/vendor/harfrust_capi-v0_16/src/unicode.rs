//! Access to the Unicode script data used by HarfRust.
//!
//! Only the default provider is exposed; custom Unicode callbacks are not
//! supported. Its lifetime is the lifetime of the process.

use crate::common::{hr_codepoint_t, hr_script_t, script_from_rust};

/// The immutable default Unicode provider.
pub struct hr_unicode_funcs_t {
    _private: (),
}

static DEFAULT_UNICODE_FUNCS: hr_unicode_funcs_t = hr_unicode_funcs_t { _private: () };

/// Returns the immutable Unicode provider used by shaping.
#[no_mangle]
pub extern "C" fn hr_unicode_funcs_get_default() -> *mut hr_unicode_funcs_t {
    core::ptr::from_ref(&DEFAULT_UNICODE_FUNCS).cast_mut()
}

/// Returns the Unicode script for `unicode`.
///
/// `ufuncs` must be the default provider or `NULL`, which also selects it.
/// Values outside the Unicode range return the unknown script.
#[no_mangle]
pub extern "C" fn hr_unicode_script(
    _ufuncs: *mut hr_unicode_funcs_t,
    unicode: hr_codepoint_t,
) -> hr_script_t {
    script_from_rust(harfrust::unicode::script(unicode))
}
