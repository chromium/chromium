// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_locale_core::langid;
use icu_segmenter::options::{SentenceBreakOptions, WordBreakOptions};
use icu_segmenter::{SentenceSegmenter, WordSegmenter};

// Additional segmenter tests with locale.

#[test]
fn word_break_with_locale() {
    // MidLetter is different because U+0x3A isn't MidLetter on Swedish.
    let s = "hello:world";
    let mut options_sv = WordBreakOptions::default();
    let langid = langid!("sv");
    options_sv.content_locale = Some(&langid);
    let segmenter = WordSegmenter::try_new_auto(options_sv).expect("Loading should succeed!");
    let iter = segmenter.segment_str(s);
    assert_eq!(
        iter.collect::<Vec<usize>>(),
        vec![0, 11],
        "word segmenter with Swedish"
    );

    let mut options_en = WordBreakOptions::default();
    let langid = langid!("en");
    options_en.content_locale = Some(&langid);
    let segmenter = WordSegmenter::try_new_auto(options_en).expect("Loading should succeed!");
    let iter = segmenter.segment_str(s);
    assert_eq!(
        iter.collect::<Vec<usize>>(),
        vec![0, 5, 6, 11],
        "word segmenter with English"
    );
}

#[test]
fn sentence_break_with_locale() {
    // SB11 is different because U+0x3B is STerm on Greek.
    let s = "hello; world";
    let mut options_el = SentenceBreakOptions::default();
    let langid = langid!("el");
    options_el.content_locale = Some(&langid);
    let segmenter = SentenceSegmenter::try_new(options_el).expect("Loading should succeed!");
    let iter = segmenter.segment_str(s);
    assert_eq!(
        iter.collect::<Vec<usize>>(),
        vec![0, 7, 12],
        "sentence segmenter with Greek"
    );

    let mut options_en = SentenceBreakOptions::default();
    let langid = langid!("en");
    options_en.content_locale = Some(&langid);
    let segmenter = SentenceSegmenter::try_new(options_en).expect("Loading should succeed!");
    let iter = segmenter.segment_str(s);
    assert_eq!(
        iter.collect::<Vec<usize>>(),
        vec![0, 12],
        "sentence segmenter with English"
    );
}
