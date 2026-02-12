use opusic_sys::opus_get_version_string;

use core::ffi::CStr;

#[test]
fn check_version() {
    let version = unsafe {
        CStr::from_ptr(opus_get_version_string())
    };
    let version = version.to_str().expect("utf-8 string");
    assert_eq!("libopus 1.5.2", version);
}
