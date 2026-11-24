//! Variation axes, platform/language name selection, and UTF-16 buffer sizing.
use crate::{table_tests::TableFace, *};
use core::mem::{offset_of, size_of};
use std::{ffi::CString, ptr};

fn fvar() -> Vec<u8> {
    let mut data = Vec::new();
    for value in [1u16, 0, 16, 2, 2, 20, 0, 12] {
        data.extend(value.to_be_bytes());
    }
    for (tag, min, default, max, flags, name) in [
        (b"wght", 100i32, 400, 900, 0u16, 256u16),
        (b"ital", 0, 0, 1, 1, 257),
    ] {
        data.extend(tag);
        for value in [min, default, max] {
            data.extend((value * 65536).to_be_bytes());
        }
        data.extend(flags.to_be_bytes());
        data.extend(name.to_be_bytes());
    }
    data
}

fn utf16(text: &str) -> Vec<u8> {
    text.encode_utf16().flat_map(u16::to_be_bytes).collect()
}

fn names() -> Vec<u8> {
    let mut records = vec![
        (3u16, 1u16, 1033u16, 256u16, utf16("Weight")),
        (3, 10, 1033, 256, utf16("Wide Weight")),
        (1, 0, 0, 256, b"Mac Weight".to_vec()),
        (0, 4, 0, 256, utf16("Unicode Weight")),
        (3, 1, 1036, 256, utf16("Graisse")),
        (3, 1, 1033, 258, utf16("A😀B")),
        (1, 0, 0, 257, vec![b'X', 0x80]),
        (0, 4, 1, 259, utf16("Unicode name")),
    ];
    records.sort_by_key(|record| (record.0, record.1, record.2, record.3));
    let mut data = Vec::new();
    for value in [0u16, records.len() as u16, (6 + records.len() * 12) as u16] {
        data.extend(value.to_be_bytes());
    }
    let mut storage = Vec::new();
    for (platform, encoding, language, name, bytes) in records {
        for value in [
            platform,
            encoding,
            language,
            name,
            bytes.len() as u16,
            storage.len() as u16,
        ] {
            data.extend(value.to_be_bytes());
        }
        storage.extend(bytes);
    }
    data.extend(storage);
    data
}

fn name_face() -> TableFace {
    // Apple's ltag version 1, two language records pointing into the table.
    let mut ltag = Vec::new();
    for value in [1u32, 0, 2] {
        ltag.extend(value.to_be_bytes());
    }
    for value in [20u16, 5, 25, 2] {
        ltag.extend(value.to_be_bytes());
    }
    ltag.extend(b"en-USen");
    TableFace::new(vec![
        (u32::from_be_bytes(*b"name"), names()),
        (u32::from_be_bytes(*b"ltag"), ltag),
    ])
}

unsafe fn name(face: *mut hr_face_t, id: u32, language: Option<&str>) -> String {
    let language = language.map(|text| CString::new(text).unwrap());
    let language = unsafe {
        language.as_ref().map_or(ptr::null(), |text| {
            hr_language_from_string(text.as_ptr(), -1)
        })
    };
    let mut count = 0;
    let total =
        unsafe { hr_ot_name_get_utf16(face, id, language, &raw mut count, ptr::null_mut()) };
    assert_eq!(count, 0);
    count = total + 1;
    let mut output = vec![99; count as usize];
    assert_eq!(
        unsafe { hr_ot_name_get_utf16(face, id, language, &raw mut count, output.as_mut_ptr()) },
        total
    );
    assert_eq!(count, total);
    assert_eq!(output[total as usize], 0);
    String::from_utf16(&output[..total as usize]).unwrap()
}

#[test]
fn axis_metadata_keeps_axis_indices_flags_and_float_bounds() {
    unsafe {
        let face = TableFace::new(vec![(u32::from_be_bytes(*b"fvar"), fvar())]);
        assert_eq!(hr_ot_var_get_axis_count(face.0), 2);
        let mut output = [hr_ot_var_axis_info_t::default(); 2];
        let mut count = 2;
        assert_eq!(
            hr_ot_var_get_axis_infos(face.0, 0, &raw mut count, output.as_mut_ptr()),
            2
        );
        assert_eq!(count, 2);
        assert_eq!(
            output[0],
            hr_ot_var_axis_info_t {
                axis_index: 0,
                tag: u32::from_be_bytes(*b"wght"),
                name_id: 256,
                flags: 0,
                min_value: 100.0,
                default_value: 400.0,
                max_value: 900.0,
                reserved: 0
            }
        );
        assert_eq!(output[1].flags, HR_OT_VAR_AXIS_FLAG_HIDDEN);
        count = 2;
        assert_eq!(
            hr_ot_var_get_axis_infos(face.0, 1, &raw mut count, output.as_mut_ptr()),
            2
        );
        assert_eq!((count, output[0].axis_index), (1, 1));
        count = 99;
        assert_eq!(
            hr_ot_var_get_axis_infos(face.0, 0, &raw mut count, ptr::null_mut()),
            2
        );
        assert_eq!(count, 99);
        assert_eq!(
            hr_ot_var_get_axis_infos(face.0, u32::MAX, &raw mut count, output.as_mut_ptr()),
            2
        );
        assert_eq!(count, 0);
        assert_eq!(size_of::<hr_ot_var_axis_info_t>(), 32);
        assert_eq!(offset_of!(hr_ot_var_axis_info_t, min_value), 16);
    }
}

#[test]
fn names_prefer_exact_languages_then_encoding_and_support_ltag() {
    unsafe {
        let face = name_face();
        assert_eq!(name(face.0, 256, None), "Wide Weight");
        assert_eq!(name(face.0, 256, Some("en")), "Wide Weight");
        assert_eq!(name(face.0, 256, Some("en-US")), "Unicode Weight");
        assert_eq!(name(face.0, 256, Some("en-GB")), "Wide Weight");
        assert_eq!(name(face.0, 256, Some("fr-FR")), "Graisse");
        assert_eq!(name(face.0, 256, Some("de")), "");
        assert_eq!(name(face.0, 257, None), "X�");
        assert_eq!(name(face.0, 259, None), "Unicode name");
        assert_eq!(name(face.0, HR_OT_NAME_ID_INVALID, None), "");
    }
}

#[test]
fn ltag_tags_are_cut_at_the_length_harfbuzz_reads() {
    unsafe {
        // One Unicode-platform record for name 256, whose ltag entry is a
        // 103-byte tag; only its first 63 bytes take part in matching.
        let text = utf16("Long");
        let mut name_table = Vec::new();
        for value in [0u16, 1, 18, 0, 4, 0, 256, text.len() as u16, 0] {
            name_table.extend(value.to_be_bytes());
        }
        name_table.extend(text);
        let tag = format!("en-{}", "a".repeat(100));
        let mut ltag = Vec::new();
        for value in [1u32, 0, 1] {
            ltag.extend(value.to_be_bytes());
        }
        for value in [16u16, tag.len() as u16] {
            ltag.extend(value.to_be_bytes());
        }
        ltag.extend(tag.bytes());
        let face = TableFace::new(vec![
            (u32::from_be_bytes(*b"name"), name_table),
            (u32::from_be_bytes(*b"ltag"), ltag),
        ]);
        assert_eq!(name(face.0, 256, Some(&tag[..63])), "Long");
        assert_eq!(name(face.0, 256, Some(&tag[..62])), "");
        assert_eq!(name(face.0, 256, Some(&tag)), "");
    }
}

#[test]
fn utf16_names_reserve_a_terminator_and_never_split_surrogate_pairs() {
    unsafe {
        let face = name_face();
        for (capacity, written) in [(0, 0), (1, 0), (2, 1), (3, 1), (4, 3), (5, 4), (6, 4)] {
            let mut count = capacity;
            let mut output = [99; 7];
            assert_eq!(
                hr_ot_name_get_utf16(
                    face.0,
                    258,
                    ptr::null(),
                    &raw mut count,
                    output.as_mut_ptr()
                ),
                4
            );
            assert_eq!(count, written);
            if capacity != 0 {
                assert_eq!(output[written as usize], 0);
            }
            assert_eq!(output[capacity as usize], 99);
            let expected: Vec<_> = "A😀B".encode_utf16().take(written as usize).collect();
            assert_eq!(&output[..written as usize], expected);
        }
    }
}

#[test]
fn missing_names_and_axes_clear_outputs_and_accept_empty_faces() {
    unsafe {
        for face in [ptr::null_mut(), hr_face_get_empty()] {
            assert_eq!(hr_ot_var_get_axis_count(face), 0);
            let mut count = 1;
            let mut axis = hr_ot_var_axis_info_t::default();
            assert_eq!(
                hr_ot_var_get_axis_infos(face, 0, &raw mut count, &raw mut axis),
                0
            );
            assert_eq!(count, 0);
            count = 1;
            let mut unit = 99;
            assert_eq!(
                hr_ot_name_get_utf16(face, 256, ptr::null(), &raw mut count, &raw mut unit),
                0
            );
            assert_eq!((count, unit), (0, 0));
        }
    }
}
