use crate::GetTimezoneError;
use std::ffi::CStr;
use std::os::raw::c_char;
use std::ptr::NonNull;

extern "C" {
    fn emscripten_run_script_string(script: *const c_char) -> *mut c_char;
}

pub(crate) fn get_timezone_inner() -> Result<String, GetTimezoneError> {
    const SCRIPT: &CStr = {
        match CStr::from_bytes_with_nul(
            "Intl.DateTimeFormat().resolvedOptions().timeZone\0".as_bytes(),
        ) {
            Ok(s) => s,
            Err(_) => panic!("Invalid UTF-8 data"),
        }
    };

    unsafe {
        NonNull::new(emscripten_run_script_string(SCRIPT.as_ptr()))
            .ok_or_else(|| GetTimezoneError::OsError)
            .and_then(|ptr| {
                CStr::from_ptr(ptr.as_ptr())
                    .to_owned()
                    .into_string()
                    .map_err(|_| GetTimezoneError::FailedParsingString)
            })
    }
}
