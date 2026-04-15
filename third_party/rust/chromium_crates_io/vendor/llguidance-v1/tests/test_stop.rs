use llg_test_utils::*;
use llguidance::{toktrie::TokenId, StopController};

fn check_stop(s: &str, exp_ss: &[&str], stop_rx: &str) {
    check_stop_tok(s, exp_ss, stop_rx, vec![], vec![]);
}

fn check_stop_tok(
    s: &str,
    exp_ss: &[&str],
    stop_rx: &str,
    stop_tokens: Vec<TokenId>,
    stop_strings: Vec<String>,
) {
    let tok_env = get_tok_env().clone();
    let trie = tok_env.tok_trie();
    let tokens = tok_env.tokenize(s);
    println!(
        "check tokens: {:?} {} stop={:?}",
        tokens,
        trie.tokens_dbg(&tokens),
        stop_tokens
    );
    let stop_rx = if stop_rx.is_empty() {
        None
    } else {
        Some(stop_rx.to_string())
    };
    let mut stop_ctrl =
        StopController::new(tok_env.clone(), stop_tokens, stop_rx, stop_strings).unwrap();
    let mut ss = vec![];
    for &t in &tokens {
        let s = stop_ctrl.commit_token(t);
        ss.push(s);
        if stop_ctrl.is_stopped() {
            break;
        }
    }
    assert_eq!(ss, exp_ss);
}

fn check_utf8(s: &str, exp_ss: &[&str]) {
    check_stop(s, exp_ss, "");
}

#[test]
fn test_stop_controller_utf() {
    check_utf8("hello", &["hello"]);
    check_utf8("hello world", &["hello", " world"]);
    check_utf8("🫠", &["", "", "", "🫠"]);
    check_utf8(
        "hello🫠🪱world",
        &["hello", "", "", "", "🫠", "", "", "", "🪱", "world"],
    );
}

#[test]
fn test_stop_controller_stop() {
    check_stop(
        "h e l l o w o r l d h e l l a",
        &[
            "",
            "",
            "",
            "",
            "h e l l o",
            " w",
            " o",
            " r",
            " l",
            " d",
            " ",
            "",
            "",
            "",
            "",
        ],
        "h e l l a",
    );

    check_stop(
        "heLLo world heLLa",
        &["", "", "heLLo", " world", " ", "", ""],
        "heLLa",
    );

    check_stop(
        "heLL🫠world heLLa",
        &["", "", "heLL", "", "", "🫠", "world", " ", "", ""],
        "heLLa",
    );

    check_stop(
        "heLLo world heXLa",
        &["", "", "heLLo", " world", " ", "", ""],
        r#"he[XL]La"#,
    );

    check_stop(
        "heLLo wOrLd heXLa",
        &["", "", "heLLo", " ", "", "wOrL", "d", " ", "", ""],
        r#"(he[XL]La|wOrld)"#,
    );

    check_stop(
        "heLLLLLLL🫠world heLLLLLLLLLLLa",
        &[
            "",
            "",
            "",
            "",
            "",
            "heLLLLLLL",
            "",
            "",
            "🫠",
            "world",
            " ",
            "",
            "",
            "",
            "",
            "",
            "",
        ],
        r#"he(L+)a"#,
    );

    check_stop_tok(
        "heLLo world heLLa",
        &["", "", "heLLo", " world", " ", "", ""],
        "",
        vec![],
        vec!["heLLa".to_string()],
    );

    check_stop_tok(
        "heQLo world heLLa",
        &["", "", "heQLo", " world", " ", "", ""],
        "",
        vec![],
        vec!["heLLa".to_string(), "heQLa".to_string()],
    );

    check_stop_tok(
        "heLLo world heQLa",
        &["", "", "heLLo", " world", " ", "", ""],
        "",
        vec![],
        vec!["heLLa".to_string(), "heQLa".to_string()],
    );

    check_stop_tok(
        "heLLo wOr[123]ld heQLa",
        &[
            "", "", "heLLo", " ", "wOr", "[", "1", "2", "3", "]", "ld", " ", "", "",
        ],
        "wor[123]",
        vec![],
        vec!["heLLa".to_string(), "heQLa".to_string()],
    );

    check_stop_tok(
        "heLLo world he+La",
        &["", "", "heLLo", " world", " ", "he+", "La"],
        "",
        vec![],
        vec!["heLLa".to_string(), "he+La".to_string()],
    );
}

#[test]
fn test_stop_controller_tokens() {
    let tok_env = get_tok_env().clone();
    // let trie = tok_env.tok_trie();

    let stop_tokens = tok_env.tokenize_special("<|end|><|user|>");

    check_stop_tok(
        "hello<|system|><|end|>",
        &["hello", "<|system|>", ""],
        "",
        stop_tokens.clone(),
        vec![],
    );

    check_stop_tok(
        "heLLo world heXL<|user|>",
        &["", "", "heLLo", " world", " ", "", "", "heXL"],
        r#"he[XL]La"#,
        stop_tokens.clone(),
        vec![],
    );

    check_stop_tok(
        "heLLo world heXLa",
        &["", "", "heLLo", " world", " ", "", ""],
        r#"he[XL]La"#,
        stop_tokens.clone(),
        vec![],
    );

    check_stop_tok(
        "he<|system|>LLo world heXLa",
        &["", "he<|system|>", "LL", "o", " world", " ", "", ""],
        r#"he[XL]La"#,
        stop_tokens.clone(),
        vec![],
    );
}
