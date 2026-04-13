use toktrie::recognizer::FunctionalRecognizer;
use toktrie::{SimpleVob, TokRxInfo, TokTrie, TokenId};

// ── Vocabulary definition ──────────────────────────────────────────────────────

pub const EOS_TOKEN: TokenId = 25;
pub const VOCAB_SIZE: u32 = 26;

/// Build the test vocabulary. Index == token ID.
pub fn vocab() -> Vec<Vec<u8>> {
    vec![
        b"".to_vec(),      //  0: empty/padding
        b"a".to_vec(),     //  1
        b"an".to_vec(),    //  2
        b"ant".to_vec(),   //  3
        b"app".to_vec(),   //  4
        b"apple".to_vec(), //  5
        b"apply".to_vec(), //  6
        b"apps".to_vec(),  //  7
        b"b".to_vec(),     //  8
        b"ba".to_vec(),    //  9
        b"bat".to_vec(),   // 10
        b"ban".to_vec(),   // 11
        b"band".to_vec(),  // 12
        b"c".to_vec(),     // 13
        b"ca".to_vec(),    // 14
        b"cat".to_vec(),   // 15
        b"car".to_vec(),   // 16
        b"card".to_vec(),  // 17
        b"d".to_vec(),     // 18
        b"do".to_vec(),    // 19
        b"dog".to_vec(),   // 20
        b"dot".to_vec(),   // 21
        b"e".to_vec(),     // 22
        b" ".to_vec(),     // 23
        b"th".to_vec(),    // 24
        b"the".to_vec(),   // 25  (also EOS)
    ]
}

/// Construct a TokTrie from our test vocabulary.
pub fn build_test_trie() -> TokTrie {
    let words = vocab();
    let info = TokRxInfo {
        vocab_size: VOCAB_SIZE,
        tok_eos: EOS_TOKEN,
        tok_bos: None,
        tok_pad: None,
        tok_unk: None,
        tok_end_of_turn: None,
    };
    TokTrie::from(&info, &words)
}

// ── Helper: collect allowed token IDs from a SimpleVob ─────────────────────────

pub fn allowed_set(vob: &SimpleVob) -> Vec<TokenId> {
    let mut v = Vec::new();
    for i in 0..VOCAB_SIZE {
        if vob.is_allowed(i) {
            v.push(i);
        }
    }
    v
}

/// A recognizer that only accepts lowercase ASCII letters (a-z).
/// This rejects spaces, digits, special characters, and the empty token.
#[derive(Clone, Copy)]
pub struct AlphaOnly;

impl FunctionalRecognizer<()> for AlphaOnly {
    fn initial(&self) {}

    fn try_append(&self, _state: (), byte: u8) -> Option<()> {
        if byte.is_ascii_lowercase() {
            Some(())
        } else {
            None
        }
    }

    fn get_error(&self, _state: ()) -> Option<String> {
        Some("AlphaOnly: expected lowercase ASCII letter (a-z)".to_string())
    }
}

/// A recognizer that only allows strings starting with "ca" followed by
/// at most one more lowercase letter. This simulates a simple grammar
/// constraint: the next token must match /^ca[a-z]?$/.
///
/// States: 0 = initial (expecting 'c'), 1 = seen "c" (expecting 'a'),
/// 2 = seen "ca" (expecting a-z), 3 = complete (no further bytes accepted).
#[derive(Clone, Copy)]
pub struct CaPrefix;

impl FunctionalRecognizer<u8> for CaPrefix {
    fn initial(&self) -> u8 {
        0 // state: number of bytes consumed so far
    }

    fn try_append(&self, state: u8, byte: u8) -> Option<u8> {
        match state {
            0 if byte == b'c' => Some(1),
            1 if byte == b'a' => Some(2),
            2 if byte.is_ascii_lowercase() => Some(3),
            _ => None,
        }
    }

    fn get_error(&self, state: u8) -> Option<String> {
        match state {
            0 => Some("CaPrefix: expected 'c'".to_string()),
            1 => Some("CaPrefix: expected 'a' after 'c'".to_string()),
            2 => Some("CaPrefix: expected lowercase letter after \"ca\"".to_string()),
            3 => Some("CaPrefix: pattern complete, no further bytes accepted".to_string()),
            _ => Some(format!("CaPrefix: unexpected state {state}")),
        }
    }
}
