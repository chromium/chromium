//! Tests for bias cache correctness.
//!
//! These tests verify that the bias cache doesn't affect computed masks by:
//! 1. Cloning the matcher before each step
//! 2. Computing mask with the original (cached) matcher
//! 3. Invalidating cache on the clone and computing (uncached)
//! 4. Comparing the two masks for equality

use lazy_static::lazy_static;
use llg_test_utils::get_tok_env;
use llguidance::{
    api::TopLevelGrammar,
    earley::SlicedBiasComputer,
    toktrie::{InferenceCapabilities, SimpleVob},
    Matcher, ParserFactory,
};

lazy_static! {
    static ref PARSER_FACTORY: ParserFactory = {
        let tok_env = get_tok_env().clone();
        let mut fact = ParserFactory::new(
            &tok_env,
            InferenceCapabilities {
                ff_tokens: true,
                backtrack: false,
                conditional_ff_tokens: false,
                fork: false,
            },
            &SlicedBiasComputer::general_slices(),
        )
        .unwrap();
        fact.quiet();
        fact
    };
}

fn create_matcher(grammar: &str, max_tokens: Option<usize>) -> Matcher {
    let mut grm = TopLevelGrammar::from_lark(grammar.to_string());
    grm.max_tokens = max_tokens;
    let parser = PARSER_FACTORY.create_parser(grm);
    Matcher::new(parser)
}

/// Compare two masks for equality
fn masks_equal(a: &SimpleVob, b: &SimpleVob) -> bool {
    a.as_slice() == b.as_slice()
}

/// Compute mask with cache enabled, then compare against a fresh clone
/// with cache invalidated. This tests that caching doesn't affect results.
fn assert_cache_consistent(matcher: &mut Matcher) {
    // Clone the matcher to get a fresh copy at the same state
    let mut uncached = matcher.deep_clone();
    uncached.invalidate_bias_cache();

    // Compute with cache (original) and without cache (clone)
    let mask_cached = matcher.compute_mask_or_eos().unwrap();
    let mask_uncached = uncached.compute_mask_or_eos().unwrap();

    assert!(
        masks_equal(&mask_cached, &mask_uncached),
        "Cache inconsistency detected: cached and uncached masks differ"
    );
}

// ==================== Greedy Lexeme Tests ====================

#[test]
fn test_cache_greedy_lexeme_ambiguous() {
    let mut m = create_matcher(r#"start: /.+/ "END""#, None);
    assert_cache_consistent(&mut m);

    let tok_env = get_tok_env();
    for tok in tok_env.tokenize("abcdefghijklmnopEND") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
}

#[test]
fn test_cache_greedy_lexeme_ambiguous_nullable() {
    let mut m = create_matcher(r#"start: /.*/ "END""#, None);
    assert_cache_consistent(&mut m);

    let tok_env = get_tok_env();
    for tok in tok_env.tokenize("abcdefghijklmnopEND") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
}

#[test]
fn test_cache_greedy_lexeme_unambiguous() {
    let mut m = create_matcher(r#"start: /[a-z]+/ "END""#, None);
    assert_cache_consistent(&mut m);

    let tok_env = get_tok_env();
    for tok in tok_env.tokenize("abcdefghijklmnopEND") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
}

#[test]
fn test_cache_greedy_lexeme_unambiguous_nullable() {
    let mut m = create_matcher(r#"start: /[a-z]*/ "END""#, None);
    assert_cache_consistent(&mut m);

    let tok_env = get_tok_env();
    for tok in tok_env.tokenize("abcdefghijklmnopEND") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
}

// ==================== EOS Token Edge Cases ====================

#[test]
fn test_cache_eos_allowed() {
    // Grammar that accepts empty string (EOS immediately valid)
    let mut m = create_matcher(r#"start: /[a-z]*/"#, None);
    assert_cache_consistent(&mut m);

    let tok_env = get_tok_env();
    for tok in tok_env.tokenize("abcdefghijklmnop") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
}

#[test]
fn test_cache_eos_not_allowed() {
    // Grammar requires at least one char
    let mut m = create_matcher(r#"start: /[a-z]+/"#, None);
    assert_cache_consistent(&mut m);

    let tok_env = get_tok_env();
    for tok in tok_env.tokenize("abcdefghijklmnop") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
}
// ==================== Lazy Lexeme Tests ====================

#[test]
fn test_cache_lazy_lexeme_ambiguous() {
    let mut m = create_matcher(r#"start[lazy]: /.+/ "END""#, None);
    assert_cache_consistent(&mut m);

    let tok_env = get_tok_env();
    for tok in tok_env.tokenize("abcdefghijklmnopEND") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
}

#[test]
fn test_cache_lazy_lexeme_ambiguous_nullable() {
    let mut m = create_matcher(r#"start[lazy]: /.*/ "END""#, None);
    assert_cache_consistent(&mut m);

    let tok_env = get_tok_env();
    for tok in tok_env.tokenize("abcdefghijklmnopEND") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
}

#[test]
fn test_cache_lazy_lexeme_unambiguous() {
    let mut m = create_matcher(r#"start[lazy]: /[a-z]+/ "END""#, None);
    assert_cache_consistent(&mut m);

    let tok_env = get_tok_env();
    for tok in tok_env.tokenize("abcdefghijklmnopEND") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
}

#[test]
fn test_cache_lazy_lexeme_unambiguous_nullable() {
    let mut m = create_matcher(r#"start[lazy]: /[a-z]*/ "END""#, None);
    assert_cache_consistent(&mut m);

    let tok_env = get_tok_env();
    for tok in tok_env.tokenize("abcdefghijklmnopEND") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
}

// ==================== Lexeme Boundary Tests ====================

#[test]
fn test_cache_multiple_lexemes_sequential() {
    let mut m = create_matcher(
        r#"
        start: a b c
        a[lazy]: /.*/ "A"
        b[lazy]: /.*/ "B"  
        c[lazy]: /.*/ "C"
        "#,
        None,
    );
    assert_cache_consistent(&mut m);

    let tok_env = get_tok_env();
    // First lexeme
    for tok in tok_env.tokenize("first content hereA") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
    // Second lexeme
    for tok in tok_env.tokenize("second content hereB") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
    // Third lexeme (don't complete to avoid stop)
    for tok in tok_env.tokenize("third content hereC") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
}

// ==================== Mixed Greedy/Lazy Tests ====================

#[test]
fn test_cache_greedy_then_lazy() {
    let mut m = create_matcher(
        r#"
        start: greedy lazy_part
        greedy: /[0-9]+/
        lazy_part[lazy]: /.*/ "END"
        "#,
        None,
    );
    assert_cache_consistent(&mut m);

    let tok_env = get_tok_env();
    // Greedy part
    for tok in tok_env.tokenize("1234567890") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
    // Transition to lazy
    for tok in tok_env.tokenize("some lazy content here END") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
}

#[test]
fn test_cache_lazy_then_greedy() {
    let mut m = create_matcher(
        r#"
        start: lazy_part greedy
        lazy_part[lazy]: /[^0-9]*/ "X"
        greedy: /[0-9]+/
        "#,
        None,
    );
    assert_cache_consistent(&mut m);

    let tok_env = get_tok_env();
    // Lazy part
    for tok in tok_env.tokenize("lazy content before terminatorX") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
    // Greedy part
    for tok in tok_env.tokenize("1234567890") {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
}

// ==================== Max Tokens Tests ====================

#[test]
fn test_cache_with_max_tokens() {
    let mut m = create_matcher(r#"start: /[a-z]+[0-9]+/"#, Some(10));

    let tok_env = get_tok_env();
    for &tok in tok_env.tokenize("abcdefgh12345678910")[..10].iter() {
        m.consume_token(tok).unwrap();
        assert_cache_consistent(&mut m);
    }
}
