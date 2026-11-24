//! CPAL version 0/1 palettes, shared records, flags, and pagination.
use crate::{table_tests::TableFace, *};
use std::ptr;

fn cpal(version: u16) -> Vec<u8> {
    let color_offset: u32 = if version == 0 { 16 } else { 28 };
    let mut data: Vec<_> = [version, 3, 2, 5]
        .iter()
        .flat_map(|value| value.to_be_bytes())
        .collect();
    data.extend(color_offset.to_be_bytes());
    for index in [0u16, 2] {
        data.extend(index.to_be_bytes());
    }
    if version != 0 {
        data.extend(48u32.to_be_bytes());
        data.extend([0; 8]);
    }
    for color in [
        0x1234_5601u32,
        0xABCD_EF00,
        0x0000_00FF,
        0x1020_3040,
        0xF0E0_D0C0,
    ] {
        data.extend(color.to_be_bytes());
    }
    if version != 0 {
        data.extend(HR_OT_COLOR_PALETTE_FLAG_USABLE_WITH_LIGHT_BACKGROUND.to_be_bytes());
        data.extend(HR_OT_COLOR_PALETTE_FLAG_USABLE_WITH_DARK_BACKGROUND.to_be_bytes());
    }
    data
}

#[test]
fn palettes_preserve_unpremultiplied_colors_and_shared_record_indices() {
    unsafe {
        for version in [0, 1] {
            let face = TableFace::new(vec![(u32::from_be_bytes(*b"CPAL"), cpal(version))]);
            assert_eq!(hr_ot_color_has_palettes(face.0), 1);
            assert_eq!(hr_ot_color_palette_get_count(face.0), 2);
            for palette in 0..2 {
                let mut colors = [99; 4];
                let mut count = 4;
                assert_eq!(
                    hr_ot_color_palette_get_colors(
                        face.0,
                        palette,
                        0,
                        &raw mut count,
                        colors.as_mut_ptr()
                    ),
                    3
                );
                assert_eq!(count, 3);
                assert_eq!(
                    colors,
                    if palette == 0 {
                        [0x1234_5601, 0xABCD_EF00, 0x0000_00FF, 99]
                    } else {
                        [0x0000_00FF, 0x1020_3040, 0xF0E0_D0C0, 99]
                    }
                );
                assert_eq!(
                    hr_ot_color_palette_get_flags(face.0, palette),
                    if version == 0 { 0 } else { 1 << palette }
                );
            }
            assert_eq!(hr_ot_color_palette_get_flags(face.0, 2), 0);
        }
    }
}

#[test]
fn color_pages_and_null_array_queries_follow_harfbuzz_counts() {
    unsafe {
        let face = TableFace::new(vec![(u32::from_be_bytes(*b"CPAL"), cpal(1))]);
        let mut colors = [99; 3];
        let mut count = 3;
        assert_eq!(
            hr_ot_color_palette_get_colors(face.0, 1, 2, &raw mut count, colors.as_mut_ptr()),
            3
        );
        assert_eq!((count, colors), (1, [0xF0E0_D0C0, 99, 99]));
        count = 3;
        assert_eq!(
            hr_ot_color_palette_get_colors(
                face.0,
                0,
                u32::MAX,
                &raw mut count,
                colors.as_mut_ptr()
            ),
            3
        );
        assert_eq!(count, 0);
        count = 99;
        assert_eq!(
            hr_ot_color_palette_get_colors(face.0, 0, 0, &raw mut count, ptr::null_mut()),
            3
        );
        assert_eq!(count, 99);
        assert_eq!(
            hr_ot_color_palette_get_colors(face.0, 0, 0, ptr::null_mut(), colors.as_mut_ptr()),
            3
        );
        assert_eq!(colors[0], 0xF0E0_D0C0);
        assert_eq!(
            hr_ot_color_palette_get_colors(face.0, 2, 0, &raw mut count, ptr::null_mut()),
            0
        );
        assert_eq!(count, 0);
    }
}

#[test]
fn absent_empty_and_truncated_palettes_return_no_data() {
    unsafe {
        let mut empty = cpal(0);
        empty[4..6].copy_from_slice(&0u16.to_be_bytes());
        let empty = TableFace::new(vec![(u32::from_be_bytes(*b"CPAL"), empty)]);
        let broken = TableFace::new(vec![(u32::from_be_bytes(*b"CPAL"), vec![0; 11])]);
        for face in [ptr::null_mut(), hr_face_get_empty(), empty.0, broken.0] {
            assert_eq!(hr_ot_color_has_palettes(face), 0);
            assert_eq!(hr_ot_color_palette_get_count(face), 0);
            assert_eq!(hr_ot_color_palette_get_flags(face, 0), 0);
            let mut count = 99;
            assert_eq!(
                hr_ot_color_palette_get_colors(face, 0, 0, &raw mut count, ptr::null_mut()),
                0
            );
            assert_eq!(count, 0);
        }
    }
}
