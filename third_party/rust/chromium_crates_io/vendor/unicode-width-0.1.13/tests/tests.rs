// Copyright 2012-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    fs::File,
    io::{BufRead, BufReader},
};

use unicode_width::{UnicodeWidthChar, UnicodeWidthStr};

#[test]
fn test_str() {
    assert_eq!("ｈｅｌｌｏ".width(), 10);
    assert_eq!("ｈｅｌｌｏ".width_cjk(), 10);
    assert_eq!("\0\0\0\x01\x01".width(), 5);
    assert_eq!("\0\0\0\x01\x01".width_cjk(), 5);
    assert_eq!("".width(), 0);
    assert_eq!("".width_cjk(), 0);
    assert_eq!("\u{2081}\u{2082}\u{2083}\u{2084}".width(), 4);
    assert_eq!("\u{2081}\u{2082}\u{2083}\u{2084}".width_cjk(), 8);
}

#[test]
fn test_emoji() {
    // Example from the README.
    assert_eq!("👩".width(), 2); // Woman
    assert_eq!("🔬".width(), 2); // Microscope
    assert_eq!("👩‍🔬".width(), 4); // Woman scientist
}

#[test]
fn test_char() {
    assert_eq!('ｈ'.width(), Some(2));
    assert_eq!('ｈ'.width_cjk(), Some(2));
    assert_eq!('\x00'.width(), None);
    assert_eq!('\x00'.width_cjk(), None);
    assert_eq!('\x01'.width(), None);
    assert_eq!('\x01'.width_cjk(), None);
    assert_eq!('\u{2081}'.width(), Some(1));
    assert_eq!('\u{2081}'.width_cjk(), Some(2));
}

#[test]
fn test_char2() {
    assert_eq!('\x0A'.width(), None);
    assert_eq!('\x0A'.width_cjk(), None);

    assert_eq!('w'.width(), Some(1));
    assert_eq!('w'.width_cjk(), Some(1));

    assert_eq!('ｈ'.width(), Some(2));
    assert_eq!('ｈ'.width_cjk(), Some(2));

    assert_eq!('\u{AD}'.width(), Some(0));
    assert_eq!('\u{AD}'.width_cjk(), Some(0));

    assert_eq!('\u{1160}'.width(), Some(0));
    assert_eq!('\u{1160}'.width_cjk(), Some(0));

    assert_eq!('\u{a1}'.width(), Some(1));
    assert_eq!('\u{a1}'.width_cjk(), Some(2));

    assert_eq!('\u{300}'.width(), Some(0));
    assert_eq!('\u{300}'.width_cjk(), Some(0));
}

#[test]
fn unicode_12() {
    assert_eq!('\u{1F971}'.width(), Some(2));
}

#[test]
fn test_default_ignorable() {
    assert_eq!('\u{E0000}'.width(), Some(0));

    assert_eq!('\u{1160}'.width(), Some(0));
    assert_eq!('\u{3164}'.width(), Some(0));
    assert_eq!('\u{FFA0}'.width(), Some(0));
}

#[test]
fn test_jamo() {
    assert_eq!('\u{1100}'.width(), Some(2));
    assert_eq!('\u{A97C}'.width(), Some(2));
    // Special case: U+115F HANGUL CHOSEONG FILLER
    assert_eq!('\u{115F}'.width(), Some(2));
    assert_eq!('\u{1160}'.width(), Some(0));
    assert_eq!('\u{D7C6}'.width(), Some(0));
    assert_eq!('\u{11A8}'.width(), Some(0));
    assert_eq!('\u{D7FB}'.width(), Some(0));
}

#[test]
fn test_prepended_concatenation_marks() {
    for c in [
        '\u{0600}',
        '\u{0601}',
        '\u{0602}',
        '\u{0603}',
        '\u{0604}',
        '\u{06DD}',
        '\u{110BD}',
        '\u{110CD}',
    ] {
        assert_eq!(c.width(), Some(1), "{c:?} should have width 1");
    }

    for c in ['\u{0605}', '\u{070F}', '\u{0890}', '\u{0891}', '\u{08E2}'] {
        assert_eq!(c.width(), Some(0), "{c:?} should have width 0");
    }
}

#[test]
fn test_interlinear_annotation_chars() {
    assert_eq!('\u{FFF9}'.width(), Some(1));
    assert_eq!('\u{FFFA}'.width(), Some(1));
    assert_eq!('\u{FFFB}'.width(), Some(1));
}

#[test]
fn test_hieroglyph_format_controls() {
    assert_eq!('\u{13430}'.width(), Some(1));
    assert_eq!('\u{13436}'.width(), Some(1));
    assert_eq!('\u{1343C}'.width(), Some(1));
}

#[test]
fn test_marks() {
    // Nonspacing marks have 0 width
    assert_eq!('\u{0301}'.width(), Some(0));
    // Enclosing marks have 0 width
    assert_eq!('\u{20DD}'.width(), Some(0));
    // Some spacing marks have width 1
    assert_eq!('\u{09CB}'.width(), Some(1));
    // But others have width 0
    assert_eq!('\u{09BE}'.width(), Some(0));
}

#[test]
fn test_devanagari_caret() {
    assert_eq!('\u{A8FA}'.width(), Some(0));
}

#[test]
fn test_canonical_equivalence() {
    let norm_file = BufReader::new(
        File::open("tests/NormalizationTest.txt")
            .expect("run `unicode.py` first to download `NormalizationTest.txt`"),
    );
    for line in norm_file.lines() {
        let line = line.unwrap();
        if line.is_empty() || line.starts_with('#') || line.starts_with('@') {
            continue;
        }

        let mut forms_iter = line.split(';').map(|substr| -> String {
            substr
                .split(' ')
                .map(|s| char::try_from(u32::from_str_radix(s, 16).unwrap()).unwrap())
                .collect()
        });

        let orig = forms_iter.next().unwrap();
        let nfc = forms_iter.next().unwrap();
        let nfd = forms_iter.next().unwrap();
        let nfkc = forms_iter.next().unwrap();
        let nfkd = forms_iter.next().unwrap();

        assert_eq!(
            orig.width(),
            nfc.width(),
            "width of X == {orig:?} differs from toNFC(X) == {nfc:?}"
        );

        assert_eq!(
            orig.width(),
            nfd.width(),
            "width of X == {orig:?} differs from toNFD(X) == {nfd:?}"
        );

        assert_eq!(
            nfkc.width(),
            nfkd.width(),
            "width of toNFKC(X) == {nfkc:?} differs from toNFKD(X) == {nfkd:?}"
        );

        assert_eq!(
            orig.width_cjk(),
            nfc.width_cjk(),
            "CJK width of X == {orig:?} differs from toNFC(X) == {nfc:?}"
        );

        assert_eq!(
            orig.width_cjk(),
            nfd.width_cjk(),
            "CJK width of X == {orig:?} differs from toNFD(X) == {nfd:?}"
        );

        assert_eq!(
            nfkc.width_cjk(),
            nfkd.width_cjk(),
            "CJK width of toNFKC(X) == {nfkc:?} differs from toNFKD(X) == {nfkd:?}"
        );
    }
}

#[test]
fn test_emoji_presentation() {
    assert_eq!('\u{0023}'.width(), Some(1));
    assert_eq!('\u{FE0F}'.width(), Some(0));
    assert_eq!(UnicodeWidthStr::width("\u{0023}\u{FE0F}"), 2);
    assert_eq!(UnicodeWidthStr::width("a\u{0023}\u{FE0F}a"), 4);
    assert_eq!(UnicodeWidthStr::width("\u{0023}a\u{FE0F}"), 2);
    assert_eq!(UnicodeWidthStr::width("a\u{FE0F}"), 1);
    assert_eq!(UnicodeWidthStr::width("\u{0023}\u{0023}\u{FE0F}a"), 4);

    assert_eq!(UnicodeWidthStr::width("\u{002A}\u{FE0F}"), 2);
    assert_eq!(UnicodeWidthStr::width("\u{23F9}\u{FE0F}"), 2);
    assert_eq!(UnicodeWidthStr::width("\u{24C2}\u{FE0F}"), 2);
    assert_eq!(UnicodeWidthStr::width("\u{1F6F3}\u{FE0F}"), 2);
    assert_eq!(UnicodeWidthStr::width("\u{1F700}\u{FE0F}"), 1);
}

#[test]
fn test_text_presentation() {
    assert_eq!('\u{FE0E}'.width(), Some(0));

    assert_eq!('\u{2648}'.width(), Some(2));
    assert_eq!("\u{2648}\u{FE0E}".width(), 1);
    assert_eq!("\u{2648}\u{FE0E}".width_cjk(), 2);

    assert_eq!("\u{1F21A}\u{FE0E}".width(), 2);
    assert_eq!("\u{1F21A}\u{FE0E}".width_cjk(), 2);

    assert_eq!("\u{0301}\u{FE0E}".width(), 0);
    assert_eq!("\u{0301}\u{FE0E}".width_cjk(), 0);

    assert_eq!("a\u{FE0E}".width(), 1);
    assert_eq!("a\u{FE0E}".width_cjk(), 1);

    assert_eq!("𘀀\u{FE0E}".width(), 2);
    assert_eq!("𘀀\u{FE0E}".width_cjk(), 2);
}

#[test]
fn test_control_line_break() {
    assert_eq!('\u{2028}'.width(), Some(1));
    assert_eq!('\u{2029}'.width(), Some(1));
    assert_eq!("\r".width(), 1);
    assert_eq!("\n".width(), 1);
    assert_eq!("\r\n".width(), 1);
    assert_eq!("\0".width(), 1);
    assert_eq!("1\t2\r\n3\u{85}4".width(), 7);
}

#[test]
fn char_str_consistent() {
    let mut s = String::with_capacity(4);
    for c in '\0'..=char::MAX {
        s.clear();
        s.push(c);
        assert_eq!(c.width().unwrap_or(1), s.width())
    }
}

#[test]
fn test_lisu_tones() {
    for c in '\u{A4F8}'..='\u{A4FD}' {
        assert_eq!(c.width(), Some(1));
        assert_eq!(String::from(c).width(), 1);
    }
    for c1 in '\u{A4F8}'..='\u{A4FD}' {
        for c2 in '\u{A4F8}'..='\u{A4FD}' {
            let mut s = String::with_capacity(8);
            s.push(c1);
            s.push(c2);
            match (c1, c2) {
                ('\u{A4F8}'..='\u{A4FB}', '\u{A4FC}'..='\u{A4FD}') => assert_eq!(s.width(), 1),
                _ => assert_eq!(s.width(), 2),
            }
        }
    }

    assert_eq!("ꓪꓹ".width(), 2);
    assert_eq!("ꓪꓹꓼ".width(), 2);
    assert_eq!("ꓪꓹꓹ".width(), 3);
    assert_eq!("ꓪꓼꓼ".width(), 3);
}
