/// Sample-code test for TokTrie.
///
/// This test file builds a small, hand-crafted vocabulary and uses it to demonstrate
/// the core operations of TokTrie: construction, lookup, decoding, greedy tokenization,
/// trie navigation, and — most importantly — constrained token selection via `add_bias()`.
///
/// The vocabulary is deliberately small (~26 tokens) so that every assertion can be
/// verified by hand. The trie structure looks like this (tokens marked with *id*):
///
/// ```text
/// root
/// ├─ 'a' *1*
/// │  ├─ 'n' *2*  ("an")
/// │  │  └─ 't' *3*  ("ant")
/// │  └─ 'p'
/// │     └─ 'p' *4*  ("app")
/// │        ├─ 'l'
/// │        │  ├─ 'e' *5*  ("apple")
/// │        │  └─ 'y' *6*  ("apply")
/// │        └─ 's' *7*  ("apps")
/// ├─ 'b' *8*
/// │  └─ 'a' *9*  ("ba")
/// │     ├─ 't' *10* ("bat")
/// │     └─ 'n' *11* ("ban")
/// │        └─ 'd' *12* ("band")
/// ├─ 'c' *13*
/// │  └─ 'a' *14* ("ca")
/// │     ├─ 't' *15* ("cat")
/// │     └─ 'r' *16* ("car")
/// │        └─ 'd' *17* ("card")
/// ├─ 'd' *18*
/// │  └─ 'o' *19* ("do")
/// │     ├─ 'g' *20* ("dog")
/// │     └─ 't' *21* ("dot")
/// ├─ 'e' *22*
/// ├─ ' ' *23*
/// └─ 't'
///    └─ 'h' *24* ("th")
///       └─ 'e' *25* ("the")
/// ```
///
/// Token 0 is the empty/padding token, and token 25 doubles as EOS.
use toktrie::recognizer::{FunctionalRecognizer, StackRecognizer};
use toktrie::{SimpleVob, TokRxInfo, TokTrie, TokenId};

// ── Vocabulary definition ──────────────────────────────────────────────────────

const EOS_TOKEN: TokenId = 25;
const VOCAB_SIZE: u32 = 26;

/// Build the test vocabulary. Index == token ID.
fn vocab() -> Vec<Vec<u8>> {
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
fn build_test_trie() -> TokTrie {
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

fn allowed_set(vob: &SimpleVob) -> Vec<TokenId> {
    let mut v = Vec::new();
    for i in 0..VOCAB_SIZE {
        if vob.is_allowed(i) {
            v.push(i);
        }
    }
    v
}

// ── Tests ──────────────────────────────────────────────────────────────────────

#[test]
fn test_construction_and_lookups() {
    let trie = build_test_trie();

    // Vocabulary size must match what we declared.
    assert_eq!(trie.vocab_size(), VOCAB_SIZE as usize);

    // Spot-check: token bytes round-trip through token().
    assert_eq!(trie.token(0), b""); // empty token
    assert_eq!(trie.token(1), b"a");
    assert_eq!(trie.token(5), b"apple");
    assert_eq!(trie.token(15), b"cat");
    assert_eq!(trie.token(25), b"the");

    // token_id() finds exact matches.
    assert_eq!(trie.token_id(b"cat"), Some(15)); // "cat" → 15
    assert_eq!(trie.token_id(b"band"), Some(12)); // "band" → 12
    assert_eq!(trie.token_id(b"the"), Some(25)); // "the" → 25

    // token_id() returns None for strings that are not exact tokens.
    assert_eq!(trie.token_id(b"catz"), None); // no such token
    assert_eq!(trie.token_id(b"ap"), None); // "ap" is not a token (only "app" is)
    assert_eq!(trie.token_id(b"appl"), None); // "appl" is not a token
}

#[test]
fn test_decode() {
    let trie = build_test_trie();

    // decode() concatenates the raw bytes of each token.
    // tokens: "cat" (15) + " " (23) + "dog" (20) → "cat dog"
    assert_eq!(trie.decode(&[15, 23, 20]), b"cat dog");

    // tokens: "the" (25) + " " (23) + "app" (4) → "the app"
    assert_eq!(trie.decode(&[25, 23, 4]), b"the app");

    // Single token round-trip.
    assert_eq!(trie.decode(&[5]), b"apple");

    // Empty tokens are decoded as "<[id]>" markers (they represent special/padding tokens).
    assert_eq!(trie.decode(&[0, 15]), b"<[0]>cat");
}

#[test]
fn test_greedy_tokenize() {
    let trie = build_test_trie();

    // "apple" is a single token (ID 5).
    assert_eq!(trie.greedy_tokenize(b"apple"), vec![5]);

    // "ant" is a single token (ID 3).
    assert_eq!(trie.greedy_tokenize(b"ant"), vec![3]);

    // "the cat" → greedy picks "the"(25), " "(23), "cat"(15).
    assert_eq!(trie.greedy_tokenize(b"the cat"), vec![25, 23, 15]);

    // "band" is a single token (ID 12), even though "b","ba","ban" are also tokens —
    // greedy always picks the longest match.
    assert_eq!(trie.greedy_tokenize(b"band"), vec![12]);

    // "banana" → "ban"(11) + "an"(2) + "a"(1) — greedy from left to right.
    assert_eq!(trie.greedy_tokenize(b"banana"), vec![11, 2, 1]);

    // "card" is a single token (ID 17).
    assert_eq!(trie.greedy_tokenize(b"card"), vec![17]);
}

#[test]
fn test_trie_navigation() {
    let trie = build_test_trie();
    let root = trie.root();

    // The root's children correspond to the distinct first bytes of all non-empty tokens:
    // ' ' (0x20), 'a', 'b', 'c', 'd', 'e', 't'
    let root_child_bytes: Vec<u8> = trie.node_children(root).map(|n| n.byte()).collect();
    #[rustfmt::skip]
    assert_eq!(root_child_bytes, vec![b' ', b'a', b'b', b'c', b'd', b'e', b't']);

    // Walking a→p→p should land on the node for "app" (token 4).
    let node_a = trie.child_at_byte(root, b'a').expect("child 'a' exists");
    assert_eq!(node_a.token_id(), Some(1)); // "a" is token 1
    let node_ap = trie.child_at_byte(node_a, b'p').expect("child 'p' exists");
    assert_eq!(node_ap.token_id(), None); // "ap" is not a token
    let node_app = trie.child_at_byte(node_ap, b'p').expect("child 'p' exists");
    assert_eq!(node_app.token_id(), Some(4)); // "app" is token 4

    // child_at_bytes is a shortcut for the same walk.
    let node_app2 = trie.child_at_bytes(root, b"app").expect("path exists");
    assert_eq!(node_app2.token_id(), Some(4));

    // From "app", children are 'l' and 's' — leading to "apple"/"apply" and "apps".
    let app_child_bytes: Vec<u8> = trie.node_children(node_app).map(|n| n.byte()).collect();
    assert_eq!(app_child_bytes, vec![b'l', b's']);

    // There is no child 'z' from root.
    assert!(trie.child_at_byte(root, b'z').is_none());
}

/// A recognizer that only accepts lowercase ASCII letters (a-z).
/// This rejects spaces, digits, special characters, and the empty token.
#[derive(Clone, Copy)]
struct AlphaOnly;

impl FunctionalRecognizer<()> for AlphaOnly {
    fn initial(&self) {}

    fn try_append(&self, _state: (), byte: u8) -> Option<()> {
        if byte.is_ascii_lowercase() {
            Some(())
        } else {
            None
        }
    }
}

#[test]
fn test_add_bias_alpha_only() {
    let trie = build_test_trie();
    let mut set = trie.alloc_token_set();
    let mut rec = StackRecognizer::from(AlphaOnly);

    // add_bias walks the trie and enables every token whose bytes are all
    // accepted by the recognizer. With AlphaOnly, that means every token
    // consisting entirely of a-z letters.
    trie.add_bias(&mut rec, &mut set, b"");

    let allowed = allowed_set(&set);
    // All purely-alphabetic tokens should be allowed (IDs 1–22, 24–25).
    // Rejected: 0 (empty) and 23 (space " ").
    let expected: Vec<TokenId> = (1..=22).chain(24..=25).collect();
    assert_eq!(
        allowed, expected,
        "All alpha-only tokens should be allowed; space and empty should not.\n\
         allowed={allowed:?}\n\
         expected={expected:?}"
    );

    // Specifically verify space is NOT allowed.
    #[rustfmt::skip]
    assert!(!set.is_allowed(23), "space token should be rejected by AlphaOnly");
    // And that "cat" IS allowed.
    assert!(set.is_allowed(15), "\"cat\" should be allowed by AlphaOnly");
}

/// A recognizer that only allows strings starting with "ca" followed by
/// at most one more lowercase letter. This simulates a simple grammar
/// constraint: the next token must match /^ca[a-z]?$/.
#[derive(Clone, Copy)]
struct CaPrefix;

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
}

#[test]
fn test_add_bias_with_prefix_constraint() {
    let trie = build_test_trie();
    let mut set = trie.alloc_token_set();
    let mut rec = StackRecognizer::from(CaPrefix);

    // With start=b"" the recognizer sees every token from the root.
    // Tokens that match the CaPrefix pattern /^ca[a-z]?$/:
    //   "c"   (13) — matches state 0→1 (partial, but "c" is a complete token at state 1)
    //   "ca"  (14) — matches state 0→1→2
    //   "cat" (15) — matches state 0→1→2→3
    //   "car" (16) — matches state 0→1→2→3
    //
    // "card" (17) would need state 4, which doesn't exist → rejected.
    // All other tokens fail at their first byte (not 'c') → rejected.
    trie.add_bias(&mut rec, &mut set, b"");

    let allowed = allowed_set(&set);
    assert_eq!(
        allowed,
        vec![13, 14, 15, 16],
        "Only tokens matching /^ca[a-z]?$/ should be allowed"
    );
}

#[test]
fn test_add_bias_with_start_prefix() {
    let trie = build_test_trie();
    let mut set = trie.alloc_token_set();

    // AnythingGoes accepts every byte — so this is purely about the `start` prefix.
    // Calling add_bias with start=b"app" means:
    //   (a) all tokens that are *prefixes* of "app" are allowed:
    //       "a" (1), "app" (4)
    //   (b) all tokens that *extend* "app" are allowed (i.e., tokens reachable
    //       from the "app" node in the trie):
    //       "apple" (5), "apply" (6), "apps" (7)
    //
    // Note: "an"(2), "ant"(3) etc. are NOT prefixes of "app" so they are excluded.
    let mut goes = toktrie::AnythingGoes;
    trie.add_bias(&mut goes, &mut set, b"app");

    let allowed = allowed_set(&set);
    assert_eq!(
        allowed,
        vec![1, 4, 5, 6, 7],
        "start=b\"app\": prefixes of \"app\" (a, app) plus extensions (apple, apply, apps)"
    );
}

#[test]
fn test_prefix_token_id() {
    let trie = build_test_trie();

    // prefix_token_id returns the (token_id, byte_length) of the longest
    // token that is a prefix of the input bytes.

    // "apple" is an exact token (ID 5, length 5).
    assert_eq!(trie.prefix_token_id(b"apple"), (5, 5));

    // "applesauce" — longest matching prefix token is "apple" (5 bytes).
    assert_eq!(trie.prefix_token_id(b"applesauce"), (5, 5));

    // "application" — "app" (ID 4, 3 bytes) is the longest matching token;
    // "appl" is not a token, so we stop at "app".
    assert_eq!(trie.prefix_token_id(b"application"), (4, 3));

    // "banter" — "ban" (ID 11, 3 bytes) is longest, not "ba" (2) or "b" (1).
    assert_eq!(trie.prefix_token_id(b"banter"), (11, 3));

    // "them" — "the" (ID 25, 3 bytes) is longest, not "th" (2).
    assert_eq!(trie.prefix_token_id(b"them"), (25, 3));

    // "doodle" — "do" (ID 19, 2 bytes) is longest; "doo" is not a token.
    assert_eq!(trie.prefix_token_id(b"doodle"), (19, 2));
}

#[test]
fn test_all_prefixes() {
    let trie = build_test_trie();

    // all_prefixes walks the trie along the input bytes and collects
    // every token encountered along the way (not just the longest).

    // "apple" passes through: a(1) → ap(none) → app(4) → appl(none) → apple(5)
    assert_eq!(trie.all_prefixes(b"apple"), vec![1, 4, 5]);

    // "band" passes through: b(8) → ba(9) → ban(11) → band(12)
    assert_eq!(trie.all_prefixes(b"band"), vec![8, 9, 11, 12]);

    // "card" passes through: c(13) → ca(14) → car(16) → card(17)
    assert_eq!(trie.all_prefixes(b"card"), vec![13, 14, 16, 17]);

    // "the" passes through: t(none) → th(24) → the(25)
    assert_eq!(trie.all_prefixes(b"the"), vec![24, 25]);

    // "dog" passes through: d(18) → do(19) → dog(20)
    assert_eq!(trie.all_prefixes(b"dog"), vec![18, 19, 20]);

    // "xyz" — 'x' is not in the trie at all, so no prefixes found.
    assert_eq!(trie.all_prefixes(b"xyz"), vec![]);
}

#[test]
fn test_has_valid_extensions() {
    let trie = build_test_trie();

    // has_valid_extensions checks whether, starting from a given byte
    // prefix in the trie, there is at least one complete token reachable
    // that the recognizer accepts.

    // With AnythingGoes, "app" has extensions: "apple"(5), "apply"(6), "apps"(7).
    let mut goes = toktrie::AnythingGoes;
    assert!(trie.has_valid_extensions(&mut goes, b"app"));

    // "apple" is a leaf — no children beyond it, so no extensions.
    let mut goes = toktrie::AnythingGoes;
    assert!(!trie.has_valid_extensions(&mut goes, b"apple"));

    // "ba" has extensions: "bat"(10), "ban"(11), "band"(12).
    let mut goes = toktrie::AnythingGoes;
    assert!(trie.has_valid_extensions(&mut goes, b"ba"));

    // "xyz" doesn't exist in the trie at all → no extensions.
    let mut goes = toktrie::AnythingGoes;
    assert!(!trie.has_valid_extensions(&mut goes, b"xyz"));

    // With AlphaOnly recognizer, "app" still has extensions (apple, apply, apps
    // are all alphabetic).
    let mut rec = StackRecognizer::from(AlphaOnly);
    assert!(trie.has_valid_extensions(&mut rec, b"app"));

    // " " (space) has no children in the trie, so no extensions regardless.
    let mut goes = toktrie::AnythingGoes;
    assert!(!trie.has_valid_extensions(&mut goes, b" "));
}

#[test]
fn test_sorted_tokens() {
    let trie = build_test_trie();

    // sorted_tokens returns (token_id, bytes) pairs in trie DFS order
    // (i.e., lexicographically sorted by the token's byte content).
    let sorted = trie.sorted_tokens();

    // Extract just the bytes for readability.
    let sorted_bytes: Vec<&[u8]> = sorted.iter().map(|(_, b)| b.as_slice()).collect();

    // Expected lexicographic order of all non-empty tokens:
    // " " < "a" < "an" < "ant" < "app" < "apple" < "apply" < "apps"
    // < "b" < "ba" < "ban" < "band" < "bat"
    // < "c" < "ca" < "car" < "card" < "cat"
    // < "d" < "do" < "dog" < "dot"
    // < "e"
    // < "th" < "the"
    #[rustfmt::skip]
    let expected_bytes: Vec<&[u8]> = vec![
        b" ", b"a", b"an", b"ant", b"app", b"apple", b"apply", b"apps",
        b"b", b"ba", b"ban", b"band", b"bat",
        b"c", b"ca", b"car", b"card", b"cat",
        b"d", b"do", b"dog", b"dot",
        b"e",
        b"th", b"the",
    ];
    assert_eq!(sorted_bytes, expected_bytes);

    // The IDs should map back correctly.
    let sorted_ids: Vec<u32> = sorted.iter().map(|(id, _)| *id).collect();
    #[rustfmt::skip]
    assert_eq!(
        sorted_ids,
        vec![23, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 10, 13, 14, 16, 17, 15, 18, 19, 20, 21, 22, 24, 25]
    );
    // Note: token 0 (empty) is absent — it has no bytes so it's not in the trie.
    // "bat"(10) comes after "band"(12) because 't' > 'n' in byte order.
    // "cat"(15) comes after "card"(17) because 't' > 'r' in byte order.
}
