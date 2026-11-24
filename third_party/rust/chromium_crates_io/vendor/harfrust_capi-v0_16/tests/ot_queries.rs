//! Script selection, language feature lookup, and feature-list pagination.

use crate::{table_tests::TableFace, *};
use std::ptr;

fn tag(bytes: &[u8; 4]) -> u32 {
    u32::from_be_bytes(*bytes)
}

fn words(values: &[u16]) -> Vec<u8> {
    values
        .iter()
        .flat_map(|value| value.to_be_bytes())
        .collect()
}

fn layout(scripts: &[&[u8; 4]]) -> Vec<u8> {
    // Each script has a default language with required rlig and optional smcp
    // and the second liga record. FRA has only the first liga record.
    let mut script_list = words(&[scripts.len() as u16]);
    for (i, name) in scripts.iter().enumerate() {
        script_list.extend_from_slice(*name);
        script_list.extend(words(&[(2 + 6 * scripts.len() + 28 * i) as u16]));
    }
    for _ in scripts {
        script_list.extend(words(&[10, 1]));
        script_list.extend_from_slice(b"FRA ");
        script_list.extend(words(&[20, 0, 3, 2, 4, 2, 0, 0xFFFF, 1, 1]));
    }
    let mut features = words(&[5]);
    for (i, name) in [b"kern", b"liga", b"liga", b"rlig", b"smcp"]
        .iter()
        .enumerate()
    {
        features.extend_from_slice(*name);
        features.extend(words(&[(32 + 4 * i) as u16]));
    }
    features.extend([0; 20]);
    let mut table = words(&[
        1,
        0,
        10,
        (10 + script_list.len()) as u16,
        (10 + script_list.len() + features.len()) as u16,
    ]);
    table.extend(script_list);
    table.extend(features);
    table.extend(words(&[0]));
    table
}

#[test]
fn requested_scripts_take_priority_and_fallbacks_return_false() {
    unsafe {
        for table in [HR_OT_TAG_GSUB, HR_OT_TAG_GPOS] {
            let face = TableFace::new(vec![(table, layout(&[b"DFLT", b"arab", b"dflt", b"latn"]))]);
            let mut index = 99;
            let mut chosen = 99;
            let requested = [tag(b"thai"), tag(b"latn"), tag(b"arab")];
            assert_eq!(
                hr_ot_layout_table_select_script(
                    face.0,
                    table,
                    3,
                    requested.as_ptr(),
                    &raw mut index,
                    &raw mut chosen
                ),
                1
            );
            assert_eq!((index, chosen), (3, tag(b"latn")));
            assert_eq!(
                hr_ot_layout_table_select_script(
                    face.0,
                    table,
                    1,
                    requested.as_ptr(),
                    &raw mut index,
                    &raw mut chosen
                ),
                0
            );
            assert_eq!((index, chosen), (0, tag(b"DFLT")));
            assert_eq!(
                hr_ot_layout_table_select_script(
                    face.0,
                    table,
                    0,
                    ptr::null(),
                    ptr::null_mut(),
                    ptr::null_mut()
                ),
                0
            );
        }
        for (scripts, expected) in [
            (vec![b"arab", b"dflt", b"latn"], tag(b"dflt")),
            (vec![b"arab", b"latn"], tag(b"latn")),
            (vec![b"arab"], 0),
        ] {
            let face = TableFace::new(vec![(HR_OT_TAG_GSUB, layout(&scripts))]);
            let mut index = 99;
            let mut chosen = 99;
            assert_eq!(
                hr_ot_layout_table_select_script(
                    face.0,
                    HR_OT_TAG_GSUB,
                    0,
                    ptr::null(),
                    &raw mut index,
                    &raw mut chosen
                ),
                0
            );
            assert_eq!(chosen, expected);
            assert_eq!(
                index,
                if expected == 0 {
                    HR_OT_LAYOUT_NO_SCRIPT_INDEX
                } else {
                    1
                }
            );
        }
    }
}

#[test]
fn language_features_keep_global_indices_and_exclude_required_features() {
    unsafe {
        for table in [HR_OT_TAG_GSUB, HR_OT_TAG_GPOS] {
            let face = TableFace::new(vec![(table, layout(&[b"latn"]))]);
            for (language, name, expected) in [
                (HR_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, b"smcp", Some(4)),
                (HR_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, b"liga", Some(2)),
                (0, b"liga", Some(1)),
                (0, b"smcp", None),
                (HR_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, b"rlig", None),
                (1, b"liga", None),
                (u32::MAX, b"liga", None),
            ] {
                let mut index = 99;
                assert_eq!(
                    hr_ot_layout_language_find_feature(
                        face.0,
                        table,
                        0,
                        language,
                        tag(name),
                        &raw mut index
                    ),
                    i32::from(expected.is_some())
                );
                assert_eq!(index, expected.unwrap_or(HR_OT_LAYOUT_NO_FEATURE_INDEX));
            }
            assert_eq!(
                hr_ot_layout_language_find_feature(
                    face.0,
                    table,
                    0,
                    0,
                    tag(b"liga"),
                    ptr::null_mut()
                ),
                1
            );
            let mut index = 99;
            assert_eq!(
                hr_ot_layout_language_find_feature(
                    face.0,
                    table,
                    1,
                    0,
                    tag(b"liga"),
                    &raw mut index
                ),
                0
            );
            assert_eq!(index, HR_OT_LAYOUT_NO_FEATURE_INDEX);
        }
    }
}

#[test]
fn feature_pagination_preserves_duplicates_and_null_query_counts() {
    unsafe {
        let face = TableFace::new(vec![(HR_OT_TAG_GSUB, layout(&[b"latn"]))]);
        let mut output = [99; 6];
        let mut count = 6;
        assert_eq!(
            hr_ot_layout_table_get_feature_tags(
                face.0,
                HR_OT_TAG_GSUB,
                0,
                &raw mut count,
                output.as_mut_ptr()
            ),
            5
        );
        assert_eq!(count, 5);
        assert_eq!(
            output,
            [
                tag(b"kern"),
                tag(b"liga"),
                tag(b"liga"),
                tag(b"rlig"),
                tag(b"smcp"),
                99
            ]
        );
        count = 2;
        assert_eq!(
            hr_ot_layout_table_get_feature_tags(
                face.0,
                HR_OT_TAG_GSUB,
                4,
                &raw mut count,
                output.as_mut_ptr()
            ),
            5
        );
        assert_eq!((count, output[0]), (1, tag(b"smcp")));
        count = 2;
        assert_eq!(
            hr_ot_layout_table_get_feature_tags(
                face.0,
                HR_OT_TAG_GSUB,
                u32::MAX,
                &raw mut count,
                output.as_mut_ptr()
            ),
            5
        );
        assert_eq!(count, 0);
        count = 99;
        assert_eq!(
            hr_ot_layout_table_get_feature_tags(
                face.0,
                HR_OT_TAG_GSUB,
                0,
                &raw mut count,
                ptr::null_mut()
            ),
            5
        );
        assert_eq!(count, 99);
        assert_eq!(
            hr_ot_layout_table_get_feature_tags(
                face.0,
                HR_OT_TAG_GSUB,
                0,
                ptr::null_mut(),
                output.as_mut_ptr()
            ),
            5
        );
    }
}

#[test]
fn absent_and_truncated_tables_return_empty_results() {
    unsafe {
        let broken = TableFace::new(vec![(HR_OT_TAG_GSUB, vec![0; 11])]);
        for face in [ptr::null_mut(), hr_face_get_empty(), broken.0] {
            for table in [HR_OT_TAG_GSUB, HR_OT_TAG_GPOS, 0] {
                let mut index = 99;
                let mut chosen = 99;
                assert_eq!(
                    hr_ot_layout_table_select_script(
                        face,
                        table,
                        0,
                        ptr::null(),
                        &raw mut index,
                        &raw mut chosen
                    ),
                    0
                );
                assert_eq!((index, chosen), (HR_OT_LAYOUT_NO_SCRIPT_INDEX, 0));
                assert_eq!(
                    hr_ot_layout_language_find_feature(
                        face,
                        table,
                        0,
                        0,
                        tag(b"smcp"),
                        &raw mut index
                    ),
                    0
                );
                assert_eq!(index, HR_OT_LAYOUT_NO_FEATURE_INDEX);
                let mut count = 1;
                assert_eq!(
                    hr_ot_layout_table_get_feature_tags(
                        face,
                        table,
                        0,
                        &raw mut count,
                        &raw mut chosen
                    ),
                    0
                );
                assert_eq!(count, 0);
            }
        }
    }
}
