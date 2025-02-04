// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_segmenter::WordSegmenter;

// Additional word segmenter tests with complex string.

#[test]
fn word_break_th() {
    for segmenter in [WordSegmenter::new_auto(), WordSegmenter::new_lstm()] {
        // http://wpt.live/css/css-text/word-break/word-break-normal-th-000.html
        let s = "ภาษาไทยภาษาไทย";
        let utf16: Vec<u16> = s.encode_utf16().collect();
        let iter = segmenter.segment_utf16(&utf16);
        assert_eq!(
            iter.collect::<Vec<usize>>(),
            vec![0, 4, 7, 11, 14],
            "word segmenter with Thai"
        );
        let iter = segmenter.segment_str(s);
        assert_eq!(
            iter.collect::<Vec<usize>>(),
            vec![0, 12, 21, 33, 42],
            "word segmenter with Thai"
        );

        // Combine non-Thai and Thai.
        let s = "aภาษาไทยภาษาไทยb";
        let utf16: Vec<u16> = s.encode_utf16().collect();
        let iter = segmenter.segment_utf16(&utf16);
        assert_eq!(
            iter.collect::<Vec<usize>>(),
            vec![0, 1, 5, 8, 12, 15, 16],
            "word segmenter with Thai and ascii"
        );
    }
}

#[test]
fn word_break_my() {
    let segmenter = WordSegmenter::new_auto();

    let s = "မြန်မာစာမြန်မာစာမြန်မာစာ";
    let utf16: Vec<u16> = s.encode_utf16().collect();
    let iter = segmenter.segment_utf16(&utf16);
    assert_eq!(
        iter.collect::<Vec<usize>>(),
        vec![0, 8, 16, 22, 24],
        "word segmenter with Burmese"
    );
}

#[test]
fn word_break_hiragana() {
    for segmenter in [WordSegmenter::new_auto(), WordSegmenter::new_dictionary()] {
        let s = "うなぎうなじ";
        let iter = segmenter.segment_str(s);
        assert_eq!(
            iter.collect::<Vec<usize>>(),
            vec![0, 9, 18],
            "word segmenter with Hiragana"
        );
    }
}

#[test]
fn word_break_mixed_han() {
    for segmenter in [WordSegmenter::new_auto(), WordSegmenter::new_dictionary()] {
        let s = "Welcome龟山岛龟山岛Welcome";
        let iter = segmenter.segment_str(s);
        assert_eq!(
            iter.collect::<Vec<usize>>(),
            vec![0, 7, 16, 25, 32],
            "word segmenter with Chinese and letter"
        );
    }
}

#[test]
fn word_line_th_wikipedia_auto() {
    use icu_segmenter::LineSegmenter;

    let text = "แพนด้าแดง (อังกฤษ: Red panda, Shining cat; จีน: 小熊貓; พินอิน: Xiǎo xióngmāo) สัตว์เลี้ยงลูกด้วยนมชนิดหนึ่ง มีชื่อวิทยาศาสตร์ว่า Ailurus fulgens";
    assert_eq!(text.len(), 297);
    let utf16: Vec<u16> = text.encode_utf16().collect();
    assert_eq!(utf16.len(), 142);

    let segmenter_word_auto = WordSegmenter::new_auto();
    let segmenter_line_auto = LineSegmenter::new_auto();

    let breakpoints_word_utf8 = segmenter_word_auto.segment_str(text).collect::<Vec<_>>();
    assert_eq!(
        breakpoints_word_utf8,
        [
            0, 9, 18, 27, 28, 29, 38, 47, 48, 49, 52, 53, 58, 59, 60, 67, 68, 71, 72, 73, 82, 83,
            84, 90, 93, 94, 95, 104, 113, 114, 115, 120, 121, 131, 132, 133, 148, 166, 175, 187,
            193, 205, 220, 221, 227, 239, 272, 281, 282, 289, 290, 297
        ]
    );

    let breakpoints_line_utf8 = segmenter_line_auto.segment_str(text).collect::<Vec<_>>();
    assert_eq!(
        breakpoints_line_utf8,
        [
            0, 9, 18, 27, 28, 38, 47, 49, 53, 60, 68, 73, 82, 84, 87, 90, 95, 104, 113, 115, 121,
            133, 148, 166, 175, 187, 193, 205, 220, 221, 227, 239, 272, 281, 282, 290, 297
        ]
    );

    let breakpoints_word_utf16 = segmenter_word_auto
        .segment_utf16(&utf16)
        .collect::<Vec<_>>();
    assert_eq!(
        breakpoints_word_utf16,
        [
            0, 3, 6, 9, 10, 11, 14, 17, 18, 19, 22, 23, 28, 29, 30, 37, 38, 41, 42, 43, 46, 47, 48,
            50, 51, 52, 53, 56, 59, 60, 61, 65, 66, 74, 75, 76, 81, 87, 90, 94, 96, 100, 105, 106,
            108, 112, 123, 126, 127, 134, 135, 142
        ]
    );

    let breakpoints_word_utf16 = segmenter_word_auto
        .segment_utf16(&utf16)
        .collect::<Vec<_>>();
    assert_eq!(
        breakpoints_word_utf16,
        [
            0, 3, 6, 9, 10, 11, 14, 17, 18, 19, 22, 23, 28, 29, 30, 37, 38, 41, 42, 43, 46, 47, 48,
            50, 51, 52, 53, 56, 59, 60, 61, 65, 66, 74, 75, 76, 81, 87, 90, 94, 96, 100, 105, 106,
            108, 112, 123, 126, 127, 134, 135, 142
        ]
    );
}
