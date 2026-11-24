//! AAT feat metadata: exclusive defaults, enable/disable pairs, and pagination.
use crate::{table_tests::TableFace, *};
use core::mem::{offset_of, size_of};
use std::ptr;

fn feat() -> Vec<u8> {
    let mut data = Vec::new();
    for word in [1u16, 0, 4, 0, 0, 0] {
        data.extend(word.to_be_bytes());
    }
    for (feature, count, offset, flags, name) in [
        (0u16, 1u16, 60u32, 0u16, 260u16),
        (1, 1, 64, 0, 256),
        (3, 3, 68, 0x8000, 262),
        (6, 2, 80, 0xC001, 258),
    ] {
        data.extend(feature.to_be_bytes());
        data.extend(count.to_be_bytes());
        data.extend(offset.to_be_bytes());
        data.extend(flags.to_be_bytes());
        data.extend(name.to_be_bytes());
    }
    for word in [0u16, 261, 2, 257, 0, 268, 3, 264, 4, 265, 0, 259, 1, 260] {
        data.extend(word.to_be_bytes());
    }
    data
}

#[test]
fn feature_types_page_in_table_order_and_preserve_null_query_counts() {
    unsafe {
        let face = TableFace::new(vec![(u32::from_be_bytes(*b"feat"), feat())]);
        let mut count = 5;
        let mut types = [99; 5];
        assert_eq!(
            hr_aat_layout_get_feature_types(face.0, 0, &raw mut count, types.as_mut_ptr()),
            4
        );
        assert_eq!((count, types), (4, [0, 1, 3, 6, 99]));
        count = 5;
        assert_eq!(
            hr_aat_layout_get_feature_types(face.0, 3, &raw mut count, types.as_mut_ptr()),
            4
        );
        assert_eq!((count, types[0]), (1, 6));
        count = 99;
        assert_eq!(
            hr_aat_layout_get_feature_types(face.0, 0, &raw mut count, ptr::null_mut()),
            4
        );
        assert_eq!(count, 99);
        assert_eq!(
            hr_aat_layout_get_feature_types(face.0, u32::MAX, &raw mut count, types.as_mut_ptr()),
            4
        );
        assert_eq!(count, 0);
    }
}

#[test]
fn exclusive_features_disable_to_the_global_default_selector() {
    unsafe {
        let face = TableFace::new(vec![(u32::from_be_bytes(*b"feat"), feat())]);
        for (feature, expected_default, expected) in [
            (
                HR_AAT_LAYOUT_FEATURE_TYPE_LETTER_CASE,
                0,
                vec![(268, 0, 0), (264, 3, 0), (265, 4, 0)],
            ),
            (
                HR_AAT_LAYOUT_FEATURE_TYPE_NUMBER_SPACING,
                1,
                vec![(259, 0, 1), (260, 1, 1)],
            ),
            (
                HR_AAT_LAYOUT_FEATURE_TYPE_LIGATURES,
                HR_AAT_LAYOUT_NO_SELECTOR_INDEX,
                vec![(257, 2, 3)],
            ),
        ] {
            let mut count = 4;
            let mut default = 99;
            let mut selectors = [hr_aat_layout_feature_selector_info_t::default(); 4];
            assert_eq!(
                hr_aat_layout_feature_type_get_selector_infos(
                    face.0,
                    feature,
                    0,
                    &raw mut count,
                    selectors.as_mut_ptr(),
                    &raw mut default
                ),
                expected.len() as u32
            );
            assert_eq!(default, expected_default);
            assert_eq!(count as usize, expected.len());
            for (actual, (name_id, enable, disable)) in selectors.iter().zip(expected) {
                assert_eq!(
                    *actual,
                    hr_aat_layout_feature_selector_info_t {
                        name_id,
                        enable,
                        disable,
                        reserved: 0
                    }
                );
            }
        }
        let mut count = 3;
        let mut default = 99;
        let mut selectors = [hr_aat_layout_feature_selector_info_t::default(); 3];
        assert_eq!(
            hr_aat_layout_feature_type_get_selector_infos(
                face.0,
                6,
                1,
                &raw mut count,
                selectors.as_mut_ptr(),
                &raw mut default
            ),
            2
        );
        assert_eq!((count, default, selectors[0].enable), (1, 1, 1));
        count = 99;
        assert_eq!(
            hr_aat_layout_feature_type_get_selector_infos(
                face.0,
                6,
                0,
                &raw mut count,
                ptr::null_mut(),
                &raw mut default
            ),
            2
        );
        assert_eq!((count, default), (99, 1));
        assert_eq!(size_of::<hr_aat_layout_feature_selector_info_t>(), 16);
        assert_eq!(
            offset_of!(hr_aat_layout_feature_selector_info_t, reserved),
            12
        );
    }
}

#[test]
fn missing_features_and_faces_return_empty_pages_and_no_default() {
    unsafe {
        let face = TableFace::new(vec![(u32::from_be_bytes(*b"feat"), feat())]);
        for feature in [
            HR_AAT_LAYOUT_FEATURE_TYPE_LOWER_CASE,
            HR_AAT_LAYOUT_FEATURE_TYPE_UPPER_CASE,
            0xFFFF,
            0x0001_0000,
            u32::MAX,
        ] {
            let mut count = 1;
            let mut default = 99;
            let mut info = hr_aat_layout_feature_selector_info_t::default();
            assert_eq!(
                hr_aat_layout_feature_type_get_selector_infos(
                    face.0,
                    feature,
                    0,
                    &raw mut count,
                    &raw mut info,
                    &raw mut default
                ),
                0
            );
            assert_eq!((count, default), (0, HR_AAT_LAYOUT_NO_SELECTOR_INDEX));
        }
        let broken = TableFace::new(vec![(u32::from_be_bytes(*b"feat"), vec![0; 11])]);
        for face in [ptr::null_mut(), hr_face_get_empty(), broken.0] {
            let mut count = 1;
            let mut feature = 99;
            assert_eq!(
                hr_aat_layout_get_feature_types(face, 0, &raw mut count, &raw mut feature),
                0
            );
            assert_eq!((count, feature), (0, 99));
            count = 1;
            let mut default = 99;
            let mut info = hr_aat_layout_feature_selector_info_t::default();
            assert_eq!(
                hr_aat_layout_feature_type_get_selector_infos(
                    face,
                    3,
                    0,
                    &raw mut count,
                    &raw mut info,
                    &raw mut default
                ),
                0
            );
            assert_eq!((count, default), (0, HR_AAT_LAYOUT_NO_SELECTOR_INDEX));
        }
    }
}

#[test]
fn setting_arrays_that_alias_out_of_proportion_drop_the_table() {
    // Every feature claims the feature records themselves as its settings.
    let aliased = |features: u16| {
        let mut data = Vec::new();
        for word in [1u16, 0, features, 0, 0, 0] {
            data.extend(word.to_be_bytes());
        }
        for feature in 0..features {
            data.extend(feature.to_be_bytes());
            data.extend((features * 3).to_be_bytes());
            data.extend(12u32.to_be_bytes());
            data.extend([0; 4]);
        }
        data
    };
    unsafe {
        for (features, listed, selectors) in [(10, 10, 30), (2000, 0, 0)] {
            let face = TableFace::new(vec![(u32::from_be_bytes(*b"feat"), aliased(features))]);
            let mut count = 0;
            assert_eq!(
                hr_aat_layout_get_feature_types(face.0, 0, &raw mut count, ptr::null_mut()),
                listed
            );
            assert_eq!(
                hr_aat_layout_feature_type_get_selector_infos(
                    face.0,
                    0,
                    0,
                    ptr::null_mut(),
                    ptr::null_mut(),
                    ptr::null_mut()
                ),
                selectors
            );
        }
    }
}
