use lazy_static::lazy_static;
use llguidance::{
    api::TopLevelGrammar,
    earley::SlicedBiasComputer,
    toktrie::{ApproximateTokEnv, InferenceCapabilities, TokEnv, TokenizerEnv},
    Matcher, ParserFactory, TokenParser,
};
use serde_json::{json, Value};
use std::sync::Arc;

lazy_static! {
    static ref PARSER_FACTORY_PHI: ParserFactory = {
        let env = llg_test_utils::get_tok_env();
        let mut fact = ParserFactory::new(
            env,
            InferenceCapabilities {
                ff_tokens: false,
                backtrack: false,
                conditional_ff_tokens: false,
                fork: false,
            },
            &SlicedBiasComputer::general_slices(),
        )
        .unwrap();
        fact.set_stderr_log_level(2);
        fact.set_buffer_log_level(0);
        fact
    };
}

lazy_static! {
    static ref PARSER_FACTORY: ParserFactory = {
        let env =
            toktrie_hf_downloader::tok_env_from_name("unsloth/Meta-Llama-3.1-8B-Instruct").unwrap();
        let mut fact = ParserFactory::new(
            &env,
            InferenceCapabilities {
                ff_tokens: false,
                backtrack: false,
                conditional_ff_tokens: false,
                fork: false,
            },
            &SlicedBiasComputer::general_slices(),
        )
        .unwrap();
        fact.set_stderr_log_level(2);
        fact.set_buffer_log_level(0);
        fact
    };
}

fn make_parser(lark: &str) -> TokenParser {
    let grm = TopLevelGrammar::from_lark(lark.to_string());
    let mut parser = PARSER_FACTORY.create_parser(grm).unwrap();
    parser.start_without_prompt();
    parser
}

fn consume(parser: &mut TokenParser, tok: u32) {
    let n = parser.consume_token(tok).unwrap();
    assert!(n == 0);
}

#[test]
fn test_ff_tokens() {
    let lark = r#"
        start: <[1111]> <[311]> ( <[366]> | "s" ) <[311]> <[1111]>
    "#;
    let grm = TopLevelGrammar::from_lark(lark.to_string());
    let mut parser = PARSER_FACTORY_PHI.create_parser(grm).unwrap();
    parser.start_without_prompt();

    let t = parser.compute_ff_tokens();
    assert_eq!(t, vec![1111, 311]);
    let n = parser.validate_tokens_raw(&t).unwrap();
    assert_eq!(n, 2);
    consume(&mut parser, 1111);
    consume(&mut parser, 311);

    let n = parser.validate_tokens_raw(&[366, 311, 1111]).unwrap();
    assert_eq!(n, 3);

    let n = parser.validate_tokens_raw(&[29879, 311, 1111]).unwrap();
    assert_eq!(n, 3);

    consume(&mut parser, 29879);

    let t = parser.compute_ff_tokens();
    assert_eq!(t, vec![311, 1111]);
    let n = parser.validate_tokens_raw(&t).unwrap();
    assert_eq!(n, 2);
}

fn get_tok_env() -> &'static TokEnv {
    PARSER_FACTORY.tok_env()
}

fn json_fwd_test(schema: Value, obj: Value) {
    let mut p = make_parser(&format!(
        "start: %json {}",
        serde_json::to_string(&schema).unwrap()
    ));

    let trie = get_tok_env().tok_trie();
    let tokens = get_tok_env().tokenize(serde_json::to_string(&obj).unwrap().as_str());
    println!("\n\ntokens: {}\n", trie.tokens_dbg(&tokens));

    for tok in tokens.iter() {
        let m = p.compute_mask().unwrap();
        assert!(m.is_allowed(*tok));
        consume(&mut p, *tok);
    }
}

#[test]
fn test_ff_json1() {
    json_fwd_test(
        json!({
            "type": "object",
            "properties": {
                "someLongPropertyName": {
                    "type": "string"
                }
            },
            "additionalProperties": false
        }),
        json!({
            "someLongPropertyName": "123"
        }),
    );
}

#[test]
fn test_ff_json2() {
    json_fwd_test(
        json!({
            "additionalProperties": false,
            "properties": {
              "path": {
                "pattern": "^/contributions",
                "type": "string"
              }
            },
            "required": ["path"],
            "type": "object"
        }
        ),
        json!({"path": "/contributions/foo"}),
    );
}

#[test]
fn test_ff_json3() {
    json_fwd_test(
        json!({
            "additionalProperties": false,
            "properties": {
              "location": { "type": "string" },
              "retries": { "type": "number" },
              "retrieveDate": { "type": "string" },
              "retryInterval": { "type": "number" }
            },
            "required": [ "location", "retrieveDate" ],
            "type": "object"
        }),
        json!({
            "location": "https://example.com/firmware.bin",
            "retrieveDate": "2022-01-01T12:00:00Z",
            "retryInterval": 300
        }),
    );
}

#[test]
fn test_ff_json4() {
    let schema = json!({
        "anyOf":[{
            "type": "object",
            "properties": {
                "foo": { "type": "number" }
            },
            // "required": ["foo"], -> with required it passes
            "additionalProperties": { "type": "string" },
        }, {
            "type": "object",
            "properties": {
                "bar": { "type": "number" }
            },
            "additionalProperties": false,
        }]
    });

    json_fwd_test(schema.clone(), json!({ "foo": 123, "baz": "hello" }));
    json_fwd_test(schema.clone(), json!({ "bar": 123 }));
}

#[test]
fn test_ff_early() {
    let lark = r#"
        start: lst
        lst: "," lst | ""
    "#;

    let mut parser = make_parser(lark);
    let tokens = get_tok_env().tokenize(",,,,,,,");

    for tok in tokens.iter() {
        parser.consume_token(*tok).unwrap();
    }
}

#[test]
fn test_err_state() {
    let lark = r#"
        start: /[a-z]*/
    "#;

    let tokens = get_tok_env().tokenize("fobarbazqu123");
    let mut t2 = vec![];
    for _ in 0..100 {
        t2.push(tokens[0]);
        t2.push(tokens[1]);
        t2.push(tokens[2]);
    }
    t2.extend_from_slice(&tokens);
    let mut matcher = Matcher::new(Ok(make_parser(lark)));

    for tok in t2.iter() {
        if let Err(e) = matcher.consume_token(*tok) {
            let e = e.to_string();
            println!("Error: {e}");
            assert!(e.contains("<state>"));
            assert!(e.contains("Tokens:"));
            return;
        }
    }
    unreachable!();
}

#[test]
fn test_trigger_lexer_error() {
    let lark = r#"
        start: /[a-z]*/
    "#;

    let tokens = get_tok_env().tokenize("fobarbazqu");
    let mut matcher = Matcher::new(Ok(make_parser(lark)));

    for tok in tokens.iter() {
        matcher.consume_token(*tok).unwrap();
    }

    if let Err(e) = matcher.test_trigger_lexer_error() {
        let e = e.to_string();
        println!("Error: {e}");
        assert!(e.contains("<state>"));
        assert!(e.contains("synthetic error"));
    } else {
        unreachable!();
    }

    // now all calls should return the same error
    if let Err(e) = matcher.consume_token(123) {
        let e = e.to_string();
        println!("Error: {e}");
        assert!(e.contains("<state>"));
        assert!(e.contains("synthetic error"));
    } else {
        unreachable!();
    }
}

#[test]
fn test_lexer_inv_crash() {
    let tokenv = get_tok_env();
    let t1 = tokenv.tokenize("#");
    let t2 = tokenv.tokenize("?");
    let tokens = tokenv.tokenize("a#");
    assert!(t1.len() == 1);
    assert!(t2.len() == 1);
    let grm = format!("start: /[a-z]+/ ( <[{}]> | <[{}]> )", t1[0], t2[0]);
    let parser = make_parser(&grm);
    let mut matcher = Matcher::new(Ok(parser));
    for t in tokens {
        matcher.consume_token(t).unwrap();
    }
}

#[test]
fn test_stop_when_try_consume_fails() {
    let lark = r#"
        start: "blah"* "stop"
    "#;

    let parser = make_parser(lark);
    let tokens = get_tok_env().tokenize("blahblahblahblahstopblah");
    let mut matcher = Matcher::new(Ok(parser));

    // When try_consume_tokens only consumes part of the tokens before hitting the end of the grammar,
    // the parser should end up in a stopped state.
    let consumed = matcher.try_consume_tokens(&tokens).unwrap();
    assert!(consumed < tokens.len());
    assert!(matcher.is_stopped());

    // Likewise, if we consume exactly the right number of tokens,
    // the parser should also end up in a stopped state.
    matcher.reset().unwrap();
    assert!(!matcher.is_stopped());
    matcher.try_consume_tokens(&tokens[..consumed]).unwrap();
    assert!(matcher.is_stopped());
}

#[test]
fn test_try_consume_after_stop() {
    let lark = r#"
        start: "blah"* "stop"
    "#;

    let parser = make_parser(lark);
    let tokens = get_tok_env().tokenize("blahblahblahblahstopblah");
    let mut matcher = Matcher::new(Ok(parser));

    for tok in tokens.iter() {
        let is_stopped = matcher.is_stopped();
        matcher.try_consume_tokens(&[*tok]).unwrap();
        if is_stopped {
            assert!(!matcher.is_error());
            return;
        }
    }
    unreachable!();
}

#[test]
/// Test that try_consume_tokens accepts a consistent number of tokens whether
/// 1. the EOS token is included in the input
/// 2. the EOS token is not included, but is consumed separately afterwards
///
/// Note that this test is not opinionated about whether the EOS token is accepted
/// in either case, just that the total number of tokens consumed is the same.
fn test_try_consume_eos_consistency() {
    let lark = r#"start: "a""#;
    let parser = make_parser(lark);
    let tokens = get_tok_env().tokenize("a");
    let eos = get_tok_env().eos_token();
    let tokens_with_eos = [tokens.as_slice(), &[eos]].concat();

    let mut matcher = Matcher::new(Ok(parser));
    let n_consumed_all = matcher.try_consume_tokens(&tokens_with_eos).unwrap();

    matcher.reset().unwrap();

    let n_consumed_no_eos = matcher.try_consume_tokens(&tokens).unwrap();
    assert!(n_consumed_no_eos <= n_consumed_all);
    let eos_consumed = matcher.try_consume_tokens(&[eos]).unwrap();
    assert!(eos_consumed <= 1);
    assert_eq!(n_consumed_no_eos + eos_consumed, n_consumed_all);
}

#[test]
fn test_multi_eos_mask_when_stopped() {
    // Build a byte-level tokenizer with two EOS tokens
    let base = ApproximateTokEnv::single_byte();
    let base_trie = base.tok_trie();
    let primary_eos = base_trie.eos_token();
    // Pick a special token as the second EOS
    let extra_eos = primary_eos - 1;
    let multi_trie = base_trie.clone().with_eos_tokens(&[primary_eos, extra_eos]);
    let tok_env: TokEnv = Arc::new(ApproximateTokEnv::new(multi_trie));

    let factory = ParserFactory::new(
        &tok_env,
        InferenceCapabilities::default(),
        &SlicedBiasComputer::general_slices(),
    )
    .unwrap();

    let grm = TopLevelGrammar::from_lark(r#"start: "a""#.to_string());
    let mut parser = factory.create_parser(grm).unwrap();
    parser.start_without_prompt();
    let mut matcher = Matcher::new(Ok(parser));

    // Consume "a" — grammar should accept
    let mask = matcher.compute_mask().unwrap();
    assert!(mask.is_allowed(b'a' as u32));
    matcher.consume_token(b'a' as u32).unwrap();

    // Parser stops after accepting the full input.
    // compute_mask_or_eos should include BOTH EOS tokens.
    let mask = matcher.compute_mask_or_eos().unwrap();
    assert!(
        mask.is_allowed(primary_eos),
        "primary EOS should be in stopped mask"
    );
    assert!(
        mask.is_allowed(extra_eos),
        "extra EOS should be in stopped mask"
    );
    assert!(matcher.is_stopped());
}
