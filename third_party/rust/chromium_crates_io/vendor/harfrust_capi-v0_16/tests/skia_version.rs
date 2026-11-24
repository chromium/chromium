use crate::*;

#[test]
fn harfbuzz_compatibility_version_is_separate_from_package_version() {
    unsafe {
        let (mut major, mut minor, mut micro) = (0, 0, 0);
        hr_harfbuzz_version(&raw mut major, &raw mut minor, &raw mut micro);
        assert_eq!((major, minor, micro), (15, 1, 0));
        assert_eq!(
            std::ffi::CStr::from_ptr(hr_harfbuzz_version_string()).to_bytes(),
            b"15.1.0"
        );
        assert_eq!(hr_harfbuzz_version_atleast(15, 1, 0), 1);
        assert_eq!(hr_harfbuzz_version_atleast(15, 1, 1), 0);
        assert_eq!(hr_harfbuzz_version_atleast(4, 4, 0), 1);
        assert_eq!(hr_version_atleast(4, 4, 0), 0);
    }
}
