// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_collections::char16trie::Char16Trie;
use zerovec::ZeroVec;

#[test]
fn empty() {
    let trie_data = toml::from_str::<TestFile>(include_str!("data/char16trie/empty.toml"))
        .unwrap()
        .ucharstrie
        .data;
    let trie = Char16Trie::new(ZeroVec::from_slice_or_alloc(trie_data.as_slice()));
    let mut cursor = trie.cursor();
    cursor.step('h');
    assert_eq!(cursor.value(), None);
    assert!(cursor.is_empty());
}

#[test]
fn a() {
    let trie_data = toml::from_str::<TestFile>(include_str!("data/char16trie/test_a.toml"))
        .unwrap()
        .ucharstrie
        .data;
    let trie = Char16Trie::new(ZeroVec::from_slice_or_alloc(trie_data.as_slice()));

    let mut cursor = trie.cursor();
    cursor.step('h');
    assert_eq!(cursor.value(), None);
    assert!(cursor.is_empty());

    let mut cursor = trie.cursor();
    cursor.step('a');
    assert_eq!(cursor.value(), Some(1));
    assert!(cursor.is_empty());
    cursor.step('a');
    assert_eq!(cursor.value(), None);
    assert!(cursor.is_empty());
}

#[test]
fn a_b() {
    let trie_data = toml::from_str::<TestFile>(include_str!("data/char16trie/test_a_ab.toml"))
        .unwrap()
        .ucharstrie
        .data;
    let trie = Char16Trie::new(ZeroVec::from_slice_or_alloc(trie_data.as_slice()));

    let mut cursor = trie.cursor();
    cursor.step('a');
    assert_eq!(cursor.value(), Some(1));
    assert!(!cursor.is_empty());
    cursor.step('a');
    assert_eq!(cursor.value(), None);
    assert!(cursor.is_empty());

    let mut cursor = trie.cursor();
    cursor.step('a');
    assert_eq!(cursor.value(), Some(1));
    assert!(!cursor.is_empty());
    cursor.step('b');
    assert_eq!(cursor.value(), Some(100));
    assert!(cursor.is_empty());
    cursor.step('b');
    assert_eq!(cursor.value(), None);
    assert!(cursor.is_empty());
}

#[test]
fn shortest_branch() {
    let trie_data =
        toml::from_str::<TestFile>(include_str!("data/char16trie/test_shortest_branch.toml"))
            .unwrap()
            .ucharstrie
            .data;
    let trie = Char16Trie::new(ZeroVec::from_slice_or_alloc(trie_data.as_slice()));

    let mut cursor = trie.cursor();
    cursor.step('a');
    assert_eq!(cursor.value(), Some(1000));
    assert!(cursor.is_empty());
    cursor.step('b');
    assert_eq!(cursor.value(), None);
    assert!(cursor.is_empty());

    let mut cursor = trie.cursor();
    cursor.step('b');
    assert_eq!(cursor.value(), Some(2000));
    assert!(cursor.is_empty());
    cursor.step('a');
    assert_eq!(cursor.value(), None);
    assert!(cursor.is_empty());
}

#[test]
fn branches() {
    let trie_data = toml::from_str::<TestFile>(include_str!("data/char16trie/test_branches.toml"))
        .unwrap()
        .ucharstrie
        .data;
    let trie = Char16Trie::new(ZeroVec::from_slice_or_alloc(trie_data.as_slice()));

    for (query, expected) in [
        ("a", (Some(0x10), true)),
        ("cc", (Some(0x40), true)),
        ("e", (Some(0x100), true)),
        ("ggg", (Some(0x400), true)),
        ("i", (Some(0x1000), true)),
        ("kkkk", (Some(0x4000), true)),
        ("n", (Some(0x10000), true)),
        ("ppppp", (Some(0x40000), true)),
        ("r", (Some(0x100000), true)),
        ("sss", (Some(0x200000), true)),
        ("t", (Some(0x400000), true)),
        ("uu", (Some(0x800000), true)),
        ("vv", (Some(0x7fffffff), true)),
        ("zz", (Some(-2147483648), true)),
    ] {
        let mut cursor = trie.cursor();
        for (i, chr) in query.chars().enumerate() {
            cursor.step(chr);
            let res = (cursor.value(), cursor.is_empty());
            if i + 1 == query.len() {
                assert_eq!(res, expected);
            } else {
                assert_eq!(res, (None, false));
            }
        }
    }
}

#[test]
fn long_sequence() {
    let trie_data =
        toml::from_str::<TestFile>(include_str!("data/char16trie/test_long_sequence.toml"))
            .unwrap()
            .ucharstrie
            .data;
    let trie = Char16Trie::new(ZeroVec::from_slice_or_alloc(trie_data.as_slice()));

    for (query, expected) in [
        ("a", (Some(-1), false)),
        // sequence of linear-match nodes
        (
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
            (Some(-2), false),
        ),
        // more than 256 units
        (
            concat!(
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
            ),
            (Some(-3), true),
        ),
    ] {
        let mut cursor = trie.cursor();
        for (i, chr) in query.chars().enumerate() {
            cursor.step(chr);
            let res = (cursor.value(), cursor.is_empty());
            if i + 1 == query.len() {
                assert_eq!(res, expected);
            } else if i == 0 {
                assert_eq!(res, (Some(-1), false));
            } else if i == 51 {
                assert_eq!(res, (Some(-2), false));
            } else {
                assert_eq!(res, (None, false));
            }
        }
    }
}

#[test]
fn long_branch() {
    let trie_data =
        toml::from_str::<TestFile>(include_str!("data/char16trie/test_long_branch.toml"))
            .unwrap()
            .ucharstrie
            .data;
    let trie = Char16Trie::new(ZeroVec::from_slice_or_alloc(trie_data.as_slice()));

    for (query, expected) in [
        ("a", (Some(-2), true)),
        ("b", (Some(-1), true)),
        ("c", (Some(0), true)),
        ("d2", (Some(1), true)),
        ("f", (Some(0x3f), true)),
        ("g", (Some(0x40), true)),
        ("h", (Some(0x41), true)),
        ("j23", (Some(0x1900), true)),
        ("j24", (Some(0x19ff), true)),
        ("j25", (Some(0x1a00), true)),
        ("k2", (Some(0x1a80), true)),
        ("k3", (Some(0x1aff), true)),
        ("l234567890", (Some(0x1b00), false)),
        ("l234567890123", (Some(0x1b01), true)),
        (
            "nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn",
            (Some(0x10ffff), true),
        ),
        (
            "oooooooooooooooooooooooooooooooooooooooooooooooooooooo",
            (Some(0x110000), true),
        ),
        (
            "pppppppppppppppppppppppppppppppppppppppppppppppppppppp",
            (Some(0x120000), true),
        ),
        ("r", (Some(0x333333), true)),
        ("s2345", (Some(0x4444444), true)),
        ("t234567890", (Some(0x77777777), true)),
        ("z", (Some(-2147483647), true)),
    ] {
        let mut cursor = trie.cursor();
        for (i, chr) in query.chars().enumerate() {
            cursor.step(chr);
            let res = (cursor.value(), cursor.is_empty());
            if i + 1 == query.len() {
                assert_eq!(res, expected);
            } else if query == "l234567890123" && i == 9 {
                assert_eq!(res, (Some(0x1b00), false));
            } else {
                assert_eq!(res, (None, false));
            }
        }
    }
}

#[test]
fn compact() {
    let trie_data = toml::from_str::<TestFile>(include_str!("data/char16trie/test_compact.toml"))
        .unwrap()
        .ucharstrie
        .data;
    let trie = Char16Trie::new(ZeroVec::from_slice_or_alloc(trie_data.as_slice()));

    for (query, expected) in [
        ("+", (Some(0), false)),
        ("+august", (Some(8), true)),
        ("+december", (Some(12), true)),
        ("+july", (Some(7), true)),
        ("+june", (Some(6), true)),
        ("+november", (Some(11), true)),
        ("+october", (Some(10), true)),
        ("+september", (Some(9), true)),
        ("-", (Some(0), false)),
        ("-august", (Some(8), true)),
        ("-december", (Some(12), true)),
        ("-july", (Some(7), true)),
        ("-june", (Some(6), true)),
        ("-november", (Some(11), true)),
        ("-october", (Some(10), true)),
        ("-september", (Some(9), true)),
        ("xjuly", (Some(7), true)),
        ("xjune", (Some(6), true)),
    ] {
        let mut cursor = trie.cursor();
        for (i, chr) in query.chars().enumerate() {
            cursor.step(chr);
            let res = (cursor.value(), cursor.is_empty());
            if i + 1 == query.len() {
                assert_eq!(res, expected);
            } else if chr == '-' || chr == '+' {
                assert_eq!(res, (Some(0), false));
            } else {
                assert_eq!(res, (None, false));
            }
        }
    }
}

#[test]
fn months() {
    let trie_data = toml::from_str::<TestFile>(include_str!("data/char16trie/months.toml"))
        .unwrap()
        .ucharstrie
        .data;
    let trie = Char16Trie::new(ZeroVec::from_slice_or_alloc(trie_data.as_slice()));

    let mut cursor = trie.cursor();
    for (chr, expected) in [
        ('j', (None, false)),
        ('u', (None, false)),
        ('n', (Some(6), false)),
        ('e', (Some(6), true)),
    ] {
        cursor.step(chr);
        assert_eq!((cursor.value(), cursor.is_empty()), expected);
    }
    cursor.step('h');
    assert_eq!((cursor.value(), cursor.is_empty()), (None, true));

    let mut cursor = trie.cursor();
    for (chr, expected) in [
        ('j', (None, false)),
        ('u', (None, false)),
        ('l', (None, false)),
        ('y', (Some(7), true)),
    ] {
        cursor.step(chr);
        assert_eq!((cursor.value(), cursor.is_empty()), expected);
    }
    cursor.step('h');
    assert_eq!((cursor.value(), cursor.is_empty()), (None, true));
}

#[derive(serde::Deserialize)]
struct TestFile {
    ucharstrie: Char16TrieVec,
}

#[derive(serde::Deserialize)]
struct Char16TrieVec {
    data: Vec<u16>,
}
