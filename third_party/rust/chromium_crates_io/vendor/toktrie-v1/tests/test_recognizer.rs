/// Sample-code tests for recognizers.
///
/// This test file demonstrates how to implement and use recognizers with
/// [`TokTrie`] for constrained token selection. It uses the shared test
/// vocabulary and helper types from the `common` module.
///
/// # Overview
///
/// A **recognizer** decides, byte by byte, which sequences are valid. The
/// trie walker calls into the recognizer as it descends through the token
/// trie, and the recognizer accepts or rejects each byte. Only tokens
/// whose complete byte sequence is accepted end up in the allowed set.
///
/// There are two ways to build a recognizer:
///
/// 1. **[`FunctionalRecognizer<S>`] + [`StackRecognizer`]** — implement a
///    pure state-transition function (`state × byte → Option<state>`), then
///    wrap it in `StackRecognizer` which manages the push/pop stack that
///    `TokTrie` requires. This is the easiest approach for most use cases.
///
/// 2. **Implement [`Recognizer`] directly** — manage your own stack. This
///    gives full control and is useful when the state is not cheaply
///    `Copy`-able or when you need custom stack behaviour.
///
/// [`FunctionalRecognizer<S>`]: toktrie::recognizer::FunctionalRecognizer
/// [`StackRecognizer`]: toktrie::recognizer::StackRecognizer
/// [`Recognizer`]: toktrie::Recognizer
/// [`TokTrie`]: toktrie::TokTrie
mod common;
use common::*;

use toktrie::recognizer::{FunctionalRecognizer, StackRecognizer};
use toktrie::{Recognizer, TokenId};

// ── FunctionalRecognizer: stateless (AlphaOnly) ────────────────────────────────

/// Demonstrates a **stateless** `FunctionalRecognizer` — one whose state
/// type is `()`.
///
/// `AlphaOnly` accepts any byte that is a lowercase ASCII letter (`a`–`z`)
/// and rejects everything else. Because the accept/reject decision depends
/// only on the current byte (not on any prior bytes), the state type is
/// the unit type `()`.
///
/// This is the simplest possible recognizer: implement
/// [`FunctionalRecognizer<()>`], wrap it in [`StackRecognizer`], and pass
/// it to [`TokTrie::add_bias`].
#[test]
fn sample_stateless_functional_recognizer() {
    // AlphaOnly is defined in common/mod.rs. It implements
    // FunctionalRecognizer<()> — accepting a-z, rejecting all else.
    let trie = build_test_trie();
    let mut set = trie.alloc_token_set();

    // Wrap the FunctionalRecognizer in a StackRecognizer so that TokTrie
    // can push/pop states during its trie walk.
    let mut rec = StackRecognizer::from(AlphaOnly);

    // add_bias walks the trie and sets a bit for every token whose bytes
    // are all accepted by the recognizer.
    trie.add_bias(&mut rec, &mut set, b"");

    let allowed = allowed_set(&set);

    // Every purely-alphabetic token is allowed (IDs 1–22, 24–25).
    // Rejected: token 0 (empty — has no bytes to accept) and token 23
    // (space " " — not a lowercase letter).
    let expected: Vec<TokenId> = (1..=22).chain(24..=25).collect();
    assert_eq!(allowed, expected);

    // Spot checks:
    assert!(!set.is_allowed(23), "space should be rejected");
    assert!(set.is_allowed(15), "\"cat\" should be allowed");
    assert!(set.is_allowed(5), "\"apple\" should be allowed");
}

// ── FunctionalRecognizer: stateful (CaPrefix) ──────────────────────────────────

/// Demonstrates a **stateful** `FunctionalRecognizer` — one whose state
/// type carries information about the bytes seen so far.
///
/// `CaPrefix` accepts byte sequences matching the pattern `/^ca[a-z]?$/`:
/// the string must start with `"ca"` optionally followed by exactly one
/// more lowercase letter. The state is a `u8` counter tracking how many
/// bytes have been consumed (0 → 1 → 2 → 3).
///
/// This shows that `FunctionalRecognizer` can encode multi-step
/// constraints via the state parameter `S`.
#[test]
fn sample_stateful_functional_recognizer() {
    // CaPrefix is defined in common/mod.rs. It implements
    // FunctionalRecognizer<u8> with states:
    //   0: initial (expecting 'c')
    //   1: seen 'c' (expecting 'a')
    //   2: seen "ca" (expecting a-z)
    //   3: seen "ca" + one letter (no more bytes accepted)
    let trie = build_test_trie();
    let mut set = trie.alloc_token_set();
    let mut rec = StackRecognizer::from(CaPrefix);

    trie.add_bias(&mut rec, &mut set, b"");

    let allowed = allowed_set(&set);

    // Tokens matching /^ca[a-z]?$/:
    //   "c"   (13) — partial match (state 1), but "c" is a complete token
    //   "ca"  (14) — matches through state 2
    //   "cat" (15) — matches through state 3
    //   "car" (16) — matches through state 3
    //
    // "card" (17) would need a 4th byte after state 3, which is rejected.
    // All other tokens fail at byte 0 (not 'c').
    assert_eq!(allowed, vec![13, 14, 15, 16]);
}

// ── StackRecognizer API ────────────────────────────────────────────────────────

/// Demonstrates the [`StackRecognizer`] API beyond just construction.
///
/// [`StackRecognizer::from`] creates the adapter. After `add_bias` has
/// walked the trie, the recognizer can be [`reset`](StackRecognizer::reset)
/// for reuse, and the underlying [`FunctionalRecognizer`] can be accessed
/// via [`recognizer`](StackRecognizer::recognizer) or
/// [`recognizer_mut`](StackRecognizer::recognizer_mut).
#[test]
fn sample_stack_recognizer_api() {
    let mut rec = StackRecognizer::from(CaPrefix);

    // The underlying FunctionalRecognizer is accessible:
    let inner: &CaPrefix = rec.recognizer();
    assert_eq!(inner.initial(), 0);

    // Use it for one add_bias call...
    let trie = build_test_trie();
    let mut set = trie.alloc_token_set();
    trie.add_bias(&mut rec, &mut set, b"");
    assert_eq!(allowed_set(&set).len(), 4); // c, ca, cat, car

    // ...then reset and reuse with a fresh token set:
    rec.reset();
    let mut set2 = trie.alloc_token_set();
    trie.add_bias(&mut rec, &mut set2, b"");
    assert_eq!(allowed_set(&set2), allowed_set(&set));
}

// ── AnythingGoes ───────────────────────────────────────────────────────────────

/// Demonstrates the two flavours of `AnythingGoes`.
///
/// There are two `AnythingGoes` types in the crate:
///
/// 1. [`toktrie::AnythingGoes`] (re-exported from `toktree`) — implements
///    [`Recognizer`] directly, so it can be passed to `add_bias` without
///    any wrapper. This is the most convenient form.
///
/// 2. [`toktrie::recognizer::AnythingGoes`] — implements
///    [`FunctionalRecognizer<()>`] and must be wrapped in
///    [`StackRecognizer`] before use with `TokTrie`.
///
/// Both accept every byte unconditionally.
#[test]
fn sample_anything_goes() {
    let trie = build_test_trie();

    // ── Flavour 1: toktrie::AnythingGoes (implements Recognizer directly) ──
    let mut set1 = trie.alloc_token_set();
    let mut goes = toktrie::AnythingGoes;
    trie.add_bias(&mut goes, &mut set1, b"");

    // Every non-empty token is allowed (IDs 1–25). Token 0 is the empty
    // token and is never in the trie.
    let all_tokens: Vec<TokenId> = (1..=25).collect();
    assert_eq!(allowed_set(&set1), all_tokens);

    // ── Flavour 2: toktrie::recognizer::AnythingGoes (FunctionalRecognizer) ──
    let mut set2 = trie.alloc_token_set();
    let mut rec = StackRecognizer::from(toktrie::recognizer::AnythingGoes {});
    trie.add_bias(&mut rec, &mut set2, b"");

    // Same result:
    assert_eq!(allowed_set(&set2), all_tokens);
}

// ── add_bias with a start prefix ───────────────────────────────────────────────

/// Demonstrates [`TokTrie::add_bias`] with a non-empty `start` prefix.
///
/// When `start` is non-empty, `add_bias` allows two categories of tokens:
///
/// 1. Tokens that are **prefixes** of `start` — these are tokens the model
///    might emit that would be consistent with eventually producing `start`.
/// 2. Tokens reachable by **extending** `start` in the trie — these are
///    tokens that begin with `start` as a prefix.
///
/// The recognizer is still consulted for category (2), but category (1)
/// is determined purely by the `start` bytes.
#[test]
fn sample_add_bias_with_start_prefix() {
    let trie = build_test_trie();

    // ── AnythingGoes + start=b"app" ──
    // Category 1 (prefixes of "app"): "a"(1), "app"(4)
    //   "an"(2) is NOT a prefix of "app" — rejected.
    // Category 2 (extensions of "app"): "apple"(5), "apply"(6), "apps"(7)
    let mut set = trie.alloc_token_set();
    let mut goes = toktrie::AnythingGoes;
    trie.add_bias(&mut goes, &mut set, b"app");
    assert_eq!(allowed_set(&set), vec![1, 4, 5, 6, 7]);

    // ── AlphaOnly + start=b"ba" ──
    // Category 1 (prefixes of "ba"): "b"(8), "ba"(9)
    //   These are always allowed regardless of the recognizer.
    // Category 2 (extensions of "ba" accepted by AlphaOnly):
    //   "bat"(10), "ban"(11), "band"(12) — all purely alphabetic.
    let mut set2 = trie.alloc_token_set();
    let mut rec = StackRecognizer::from(AlphaOnly);
    trie.add_bias(&mut rec, &mut set2, b"ba");
    assert_eq!(allowed_set(&set2), vec![8, 9, 10, 11, 12]);
}

// ── has_valid_extensions ───────────────────────────────────────────────────────

/// Demonstrates [`TokTrie::has_valid_extensions`].
///
/// This method checks whether, starting from a given byte prefix in the
/// trie, there is at least one complete token reachable whose remaining
/// bytes are all accepted by the recognizer. It does **not** compute the
/// full set — it returns as soon as it finds one valid token.
#[test]
fn sample_has_valid_extensions() {
    let trie = build_test_trie();

    // "app" has children "apple", "apply", "apps" — all alphabetic.
    let mut rec = StackRecognizer::from(AlphaOnly);
    assert!(trie.has_valid_extensions(&mut rec, b"app"));

    // "apple" is a leaf node — no children exist beyond it.
    let mut rec = StackRecognizer::from(AlphaOnly);
    assert!(!trie.has_valid_extensions(&mut rec, b"apple"));

    // "xyz" doesn't exist in the trie at all.
    let mut goes = toktrie::AnythingGoes;
    assert!(!trie.has_valid_extensions(&mut goes, b"xyz"));

    // Important: the recognizer starts in its *initial* state at the
    // prefix node — it does NOT see the prefix bytes. So with CaPrefix
    // and start=b"c", the recognizer starts at state 0 (expecting 'c')
    // but the trie feeds it 'a' (the child byte under 'c'). CaPrefix
    // rejects 'a' at state 0, so there are no valid extensions.
    let mut rec = StackRecognizer::from(CaPrefix);
    assert!(!trie.has_valid_extensions(&mut rec, b"c"));

    // With AlphaOnly and start=b"ba", extensions exist: the trie feeds
    // 't' (→ "bat") and 'n' (→ "ban", "band"), all lowercase letters.
    let mut rec = StackRecognizer::from(AlphaOnly);
    assert!(trie.has_valid_extensions(&mut rec, b"ba"));
}

// ── Implementing Recognizer directly ───────────────────────────────────────────

/// A recognizer that accepts at most `max_len` bytes of any content.
///
/// This demonstrates implementing [`Recognizer`] directly rather than
/// going through `FunctionalRecognizer` + `StackRecognizer`. Direct
/// implementation is useful when:
///
/// - The state is not cheaply `Copy`-able (e.g., it involves heap
///   allocations or complex structures).
/// - You want custom stack management (e.g., a more memory-efficient
///   encoding than a `Vec<S>`).
/// - You need access to the full stack history, not just the top element.
struct MaxLenRecognizer {
    max_len: usize,
    /// Stack of depths. The trie walker pushes when descending and pops
    /// when backtracking.
    stack: Vec<usize>,
}

impl MaxLenRecognizer {
    fn new(max_len: usize) -> Self {
        MaxLenRecognizer {
            max_len,
            stack: vec![0], // initial depth = 0
        }
    }
}

impl Recognizer for MaxLenRecognizer {
    fn pop_bytes(&mut self, num: usize) {
        self.stack.truncate(self.stack.len() - num);
    }

    fn collapse(&mut self) {
        let top = *self.stack.last().unwrap();
        self.stack.clear();
        self.stack.push(top);
    }

    fn trie_finished(&mut self) {
        self.stack.truncate(1);
    }

    fn try_push_byte(&mut self, _byte: u8) -> bool {
        let depth = *self.stack.last().unwrap();
        if depth < self.max_len {
            self.stack.push(depth + 1);
            true
        } else {
            false
        }
    }

    fn get_error(&mut self) -> Option<String> {
        let depth = *self.stack.last().unwrap();
        if depth >= self.max_len {
            Some(format!(
                "MaxLenRecognizer: reached maximum length of {} bytes",
                self.max_len
            ))
        } else {
            None
        }
    }
}

/// Demonstrates implementing [`Recognizer`] directly to constrain token
/// length.
///
/// `MaxLenRecognizer` accepts any byte content but limits the total
/// number of bytes. With `max_len=2`, only tokens of 1 or 2 bytes are
/// allowed.
#[test]
fn sample_direct_recognizer_impl() {
    let trie = build_test_trie();
    let mut set = trie.alloc_token_set();
    let mut rec = MaxLenRecognizer::new(2);

    trie.add_bias(&mut rec, &mut set, b"");

    let allowed = allowed_set(&set);

    // Tokens with ≤2 bytes:
    //   1-byte: "a"(1), "b"(8), "c"(13), "d"(18), "e"(22), " "(23)
    //   2-byte: "an"(2), "ba"(9), "ca"(14), "do"(19), "th"(24)
    //
    // Tokens with >2 bytes (e.g., "ant"(3), "app"(4), "apple"(5)) are
    // rejected.
    let expected: Vec<TokenId> = vec![1, 2, 8, 9, 13, 14, 18, 19, 22, 23, 24];
    assert_eq!(allowed, expected);
}

// ── Combining recognizers with alloc_token_set ─────────────────────────────────

/// Demonstrates using [`SimpleVob`] set operations to combine results
/// from multiple recognizers.
///
/// `TokTrie::alloc_token_set` allocates a zero-initialized bit vector
/// sized to the vocabulary. You can call `add_bias` multiple times on
/// different sets, then combine them with bitwise operations (`.or()`,
/// `.and()`, etc.).
#[test]
fn sample_combining_token_sets() {
    let trie = build_test_trie();

    // Set A: tokens accepted by AlphaOnly (all a-z tokens).
    let mut set_alpha = trie.alloc_token_set();
    let mut rec_alpha = StackRecognizer::from(AlphaOnly);
    trie.add_bias(&mut rec_alpha, &mut set_alpha, b"");

    // Set B: tokens accepted by CaPrefix (c, ca, cat, car).
    let mut set_ca = trie.alloc_token_set();
    let mut rec_ca = StackRecognizer::from(CaPrefix);
    trie.add_bias(&mut rec_ca, &mut set_ca, b"");

    // Intersection (AND): tokens that satisfy BOTH constraints.
    // CaPrefix tokens {13,14,15,16} are all alphabetic, so intersection
    // equals the CaPrefix set.
    let mut intersection = set_alpha.clone();
    intersection.and(&set_ca);
    assert_eq!(allowed_set(&intersection), vec![13, 14, 15, 16]);

    // Difference (SUB): AlphaOnly tokens that are NOT in CaPrefix.
    let mut diff = set_alpha.clone();
    diff.sub(&set_ca);
    let diff_set = allowed_set(&diff);
    assert!(!diff_set.contains(&13)); // "c" removed
    assert!(!diff_set.contains(&15)); // "cat" removed
    assert!(diff_set.contains(&1)); // "a" still present
    assert!(diff_set.contains(&5)); // "apple" still present
}

// ── get_error ──────────────────────────────────────────────────────────────────

/// Demonstrates [`FunctionalRecognizer::get_error`] for diagnostic reporting.
///
/// `get_error` is called on the *current* state — i.e., the state at the top
/// of the `StackRecognizer`'s stack — and returns a human-readable message
/// explaining what the recognizer expects. The system calls this after
/// `add_bias` or when a constraint fails, to surface a diagnostic to the user.
///
/// `AlphaOnly` has a single state `()` that is never a dead end (a-z
/// transitions are always possible), so its `get_error` returns a constant
/// hint describing the constraint.
///
/// `CaPrefix` has four states (0–3) and returns a state-specific message.
/// State 3 is a dead end — no transitions exist from it — so its message
/// explains that the pattern is complete.
#[test]
fn sample_get_error_alpha_only() {
    let mut rec = StackRecognizer::from(AlphaOnly);

    // In the initial state, get_error returns the constant diagnostic hint.
    let err = rec.get_error();
    assert_eq!(
        err.as_deref(),
        Some("AlphaOnly: expected lowercase ASCII letter (a-z)")
    );

    // After successfully pushing a byte, the state is still () — same message.
    assert!(rec.try_push_byte(b'h'));
    assert_eq!(
        rec.get_error().as_deref(),
        Some("AlphaOnly: expected lowercase ASCII letter (a-z)")
    );
}

/// Demonstrates state-dependent [`FunctionalRecognizer::get_error`] messages
/// from `CaPrefix`.
///
/// Each state produces a different diagnostic that describes what byte the
/// recognizer expects next.
#[test]
fn sample_get_error_ca_prefix() {
    let mut rec = StackRecognizer::from(CaPrefix);

    // State 0: initial — expecting 'c'.
    assert_eq!(rec.get_error().as_deref(), Some("CaPrefix: expected 'c'"));

    // Push 'c' → state 1: expecting 'a'.
    assert!(rec.try_push_byte(b'c'));
    assert_eq!(
        rec.get_error().as_deref(),
        Some("CaPrefix: expected 'a' after 'c'")
    );

    // Push 'a' → state 2: expecting a lowercase letter.
    assert!(rec.try_push_byte(b'a'));
    assert_eq!(
        rec.get_error().as_deref(),
        Some("CaPrefix: expected lowercase letter after \"ca\"")
    );

    // Push 't' → state 3: pattern complete, dead end.
    assert!(rec.try_push_byte(b't'));
    assert_eq!(
        rec.get_error().as_deref(),
        Some("CaPrefix: pattern complete, no further bytes accepted")
    );

    // Verify state 3 is indeed a dead end — no byte is accepted.
    assert!(!rec.try_push_byte(b'a'));
    assert!(!rec.try_push_byte(b'z'));
}

/// Demonstrates [`Recognizer::get_error`] on a direct `Recognizer`
/// implementation.
///
/// `MaxLenRecognizer` returns `None` while the depth is below the limit,
/// and an error message once the maximum length has been reached.
#[test]
fn sample_get_error_max_len() {
    let mut rec = MaxLenRecognizer::new(2);

    // Depth 0: no error yet.
    assert_eq!(rec.get_error(), None);

    // Push one byte → depth 1: still under the limit.
    assert!(rec.try_push_byte(b'x'));
    assert_eq!(rec.get_error(), None);

    // Push another → depth 2: at the limit, error reported.
    assert!(rec.try_push_byte(b'y'));
    assert_eq!(
        rec.get_error().as_deref(),
        Some("MaxLenRecognizer: reached maximum length of 2 bytes")
    );

    // No further bytes accepted.
    assert!(!rec.try_push_byte(b'z'));

    // After popping, we're back under the limit — no error.
    rec.pop_bytes(1);
    assert_eq!(rec.get_error(), None);
}
