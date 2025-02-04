// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_segmenter::WordSegmenter;
use icu_segmenter::WordType;

#[test]
fn rule_status() {
    let segmenter = WordSegmenter::new_auto();
    let mut iter = segmenter.segment_str("hello world 123");

    assert_eq!(iter.next(), Some(0), "SOT");
    assert_eq!(iter.word_type(), WordType::None, "none");
    assert!(!iter.is_word_like(), "SOT is false");

    assert_eq!(iter.next(), Some(5), "after hello");
    assert_eq!(iter.word_type(), WordType::Letter, "letter");
    assert!(iter.is_word_like(), "Letter is true");

    assert_eq!(iter.next(), Some(6), "after space");
    assert_eq!(iter.word_type(), WordType::None, "none");
    assert!(!iter.is_word_like(), "None is false");

    assert_eq!(iter.next(), Some(11), "after world");
    assert_eq!(iter.word_type(), WordType::Letter, "letter");
    assert!(iter.is_word_like(), "Letter is true");

    assert_eq!(iter.next(), Some(12), "after space");
    assert_eq!(iter.word_type(), WordType::None, "none");
    assert!(!iter.is_word_like(), "None is false");

    assert_eq!(iter.next(), Some(15), "after number");
    assert_eq!(iter.word_type(), WordType::Number, "number");
    assert!(iter.is_word_like(), "Number is true");

    assert_eq!(iter.next(), None, "EOT");
    assert_eq!(iter.word_type(), WordType::None, "none");
    assert!(!iter.is_word_like(), "None is false");
}

#[test]
fn rule_status_letter_eof() {
    let segmenter = WordSegmenter::new_auto();
    let mut iter = segmenter.segment_str("one.");

    assert_eq!(iter.next(), Some(0), "SOT");
    assert_eq!(iter.word_type(), WordType::None, "none");
    assert!(!iter.is_word_like(), "SOT is false");

    assert_eq!(iter.next(), Some(3), "after one");
    assert_eq!(iter.word_type(), WordType::Letter, "letter");
    assert!(iter.is_word_like(), "Letter is true");

    assert_eq!(iter.next(), Some(4), "after full stop");
    assert_eq!(iter.word_type(), WordType::None, "none");
    assert!(!iter.is_word_like(), "None is false");

    assert_eq!(iter.next(), None, "EOT");
    assert_eq!(iter.word_type(), WordType::None, "none");
    assert!(!iter.is_word_like(), "None is false");
}

#[test]
fn rule_status_numeric_eof() {
    let segmenter = WordSegmenter::new_auto();
    let mut iter = segmenter.segment_str("42.");

    assert_eq!(iter.next(), Some(0), "SOT");
    assert_eq!(iter.word_type(), WordType::None, "none");
    assert!(!iter.is_word_like(), "SOT is false");

    assert_eq!(iter.next(), Some(2), "after 42");
    assert_eq!(iter.word_type(), WordType::Number, "Number");
    assert!(iter.is_word_like(), "Number is true");

    assert_eq!(iter.next(), Some(3), "after full stop");
    assert_eq!(iter.word_type(), WordType::None, "none");
    assert!(!iter.is_word_like(), "None is false");

    assert_eq!(iter.next(), None, "EOT");
    assert_eq!(iter.word_type(), WordType::None, "none");
    assert!(!iter.is_word_like(), "None is false");
}

#[test]
fn rule_status_th() {
    let segmenter = WordSegmenter::new_auto();
    let mut iter = segmenter.segment_str("ภาษาไทยภาษาไทย");

    assert_eq!(iter.next(), Some(0), "SOT");
    assert_eq!(iter.word_type(), WordType::None, "none");
    assert!(!iter.is_word_like(), "SOT is false");

    assert_eq!(iter.next(), Some(12), "after 1st word");
    assert_eq!(iter.word_type(), WordType::Letter, "letter");
    assert!(iter.is_word_like(), "Letter(Thai) is true");

    assert_eq!(iter.next(), Some(21), "after 2nd word");
    assert_eq!(iter.word_type(), WordType::Letter, "letter");
    assert!(iter.is_word_like(), "Letter(Thai) is true");

    assert_eq!(iter.next(), Some(33), "after 3rd word");
    assert_eq!(iter.word_type(), WordType::Letter, "letter");
    assert!(iter.is_word_like(), "Letter(Thai) is true");

    assert_eq!(iter.next(), Some(42), "after 4th word and next is EOT");
    assert_eq!(iter.word_type(), WordType::Letter, "letter");
    assert!(iter.is_word_like(), "Letter(Thai) is true");

    assert_eq!(iter.next(), None, "EOT");
    assert_eq!(iter.word_type(), WordType::None, "none");
    assert!(!iter.is_word_like(), "None is false");
}

/* The rule status functions are no longer public to non word break iterators.
#[test]
fn rule_status_no_word() {
    let segmenter =
        SentenceSegmenter::new();
    let mut iter = segmenter.segment_str("hello");

    assert_eq!(iter.next(), Some(0), "SOT");
    assert_eq!(iter.rule_status(), WordType::None, "none");
    assert!(!iter.is_word_like(), "always false");

    assert_eq!(iter.next(), Some(5), "1st sentence");
    assert_eq!(iter.rule_status(), WordType::None, "none");
    assert!(!iter.is_word_like(), "always false");
}
*/
