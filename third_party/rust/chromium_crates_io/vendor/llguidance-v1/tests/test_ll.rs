// syntax:
// token separator: ‧
// token disallowed: ✖
// backtrack: 1↶ (one token)
// end of string: ≺EOS≻

use llg_test_utils::*;
use serde_json::json;

#[test]
fn test_ll_skip() {
    check_lark_grammar(
        r#"start: "A" "!"
           %ignore /[ \t]+/"#,
        &["A", " ‧ ‧!"],
    );

    check_lark_grammar(
        r#"
            start: "A: " NUMBER
            NUMBER: /[0-9]+/
            %ignore /[ \t]+/
        "#,
        &["A‧:", "✖!‧ ‧ ‧5✖!‧6‧≺EOS≻"],
    );

    check_lark_grammar_nested(
        r#"start: "." @sub"#,
        r#"start: "A" "!"
           %ignore /[ \t]+/"#,
        &[".‧A", " ‧ ‧!"],
    );
}

#[test]
fn test_ll_format() {
    check_lark_json(
        r#"start: "JSON" @sub
        "#,
        json!({
            "type": "object",
            "properties": {
                "a": {
                    "type": "string",
                    "format": "date-time"
                }
            }
        }),
        &[
            "JSON",
            "{\"‧a‧\":‧ ‧\"‧2‧0‧2‧0",
            "-",
            "0‧2",
            "-",
            "✖3‧2‧9‧T‧1‧0",
            ":",
            "3‧3",
            ":",
            "2‧2‧Z‧\"‧}",
        ],
    );

    check_lark_json(
        r#"start: "JSON" @sub
        "#,
        json!({
            "type": "object",
            "properties": {
                "a": {
                    "type": "string",
                    "format": "date"
                }
            }
        }),
        &[
            "JSON",
            "{\"‧a‧\":‧ ‧\"‧2‧0‧2‧0",
            "-",
            "0‧2",
            "-",
            "✖3‧2‧9‧\"‧}",
        ],
    );
}

#[test]
fn test_ll_json() {
    // basic JSON parsing
    check_lark_json(
        r#"start: "JSON" @sub
        "#,
        json!({
            "type": "object",
            "properties": {
                "a": {
                    "type": "number"
                }
            }
        }),
        &["JSON", "{\"‧a‧\":‧ ‧✖true‧5‧}"],
    );

    // check for forcing the field name
    check_lark_json(
        r#"start: "JSON" @sub
        "#,
        json!({
            "type": "object",
            "properties": {
                "a_long_prop_name": {
                    "type": "number"
                }
            },
            "required": ["a_long_prop_name"]
        }),
        &["JSON", "{\"", "a‧_‧long‧_‧prop‧_‧name", "\":‧ ‧5‧}"],
    );

    check_lark_json(
        r#"start: "JSON" @sub "END"
        "#,
        json!({
            "type": "array"
        }),
        &["JSON", "✖{‧[‧1‧,‧2‧,‧3‧,‧4‧,‧5‧,‧6‧,‧7‧,‧8‧]", "END"],
    );

    // again, off by one
    let c = check_lark_json(
        r#"start: "JSON" j "END"
               j[capture,max_tokens=3]: @sub
            "#,
        json!({
            "type": "array"
        }),
        &["JSON", "[‧1‧,‧2", "END"],
    );
    check_capture(&c, "j", "[1,2");

    let c = check_lark_json(
        r#"start: "JSON" j
               j[capture,max_tokens=3]: @sub
            "#,
        json!({
            "type": "array"
        }),
        &["JSON", "[‧1‧,‧2"],
    );
    check_capture(&c, "j", "[1,2");
}

#[test]
fn test_ll_ff_json() {
    check_lark_json(
        r#"start: "JSON" @sub
        "#,
        json!({
            "type": "object",
            "properties": {
                "a_long_property_name": {
                    "type": "number"
                }
            },
            "additionalProperties": false
        }),
        &["JSON", "{\"", "a‧_‧long‧_‧property‧_‧name", "\":‧ ‧5‧}"],
    );

    check_lark_json(
        r#"start: "JSON" @sub
        "#,
        json!({
            "type": "object",
            "properties": {
                "a_long_property_name": {
                    "type": "number"
                },
                "b_something_property_name": {
                    "type": "number"
                }
            },
            "additionalProperties": false
        }),
        &["JSON", "{\"‧a", "_‧long‧_‧property‧_‧name", "\":‧ ‧5‧}"],
    );
}

#[test]
fn test_ll_llg_options() {
    check_lark_json(
        r#"
            %llguidance { "no_forcing": true }
            start: "JSON" @sub
        "#,
        json!({
            "type": "object",
            "properties": {
                "a_long_property_name": {
                    "type": "number"
                }
            },
            "additionalProperties": false
        }),
        &["", "JSON‧{\"‧a‧_‧long‧_‧property‧_‧name‧\":‧ ‧5‧}"],
    );
    check_lark_grammar(
        r#"
            %llguidance { "allow_initial_skip": true }
            start: "a"*
            IGNORED: "b"
            %ignore IGNORED
        "#,
        &["", "bb‧a‧bb‧b‧aa‧a‧≺EOS≻"],
    );
}

#[test]
fn test_ll_enum_json() {
    // check for proper quoting of the enum value
    check_lark_json(
        r#"start: "JSON" @sub
    "#,
        json!({
            "type": "object",
            "properties": {
                "a": {
                    // the list of values is so weird so that no tokens are forced
                    "enum": [
                        "https://example.com",
                        "https://example.co.pl",
                        "https://exampleco.pl",
                        "https://foo.com.pl",
                        "https1://example.org",
                        "ftp://example.org"
                    ]
                }
            }
        }),
        &["JSON", "{\"‧a‧\":‧ ‧\"‧✖x‧https‧://‧example‧.‧com‧\"‧}"],
    );
}

#[test]
fn test_ll_subgrammar_max_tokens() {
    // TODO test this - should return an error from prompt processing
    // check_lark_grammar(
    //     r#"start: " x" aa " y"
    //        aa: " a" aa
    //        "#,
    //     &[" x", " a‧ a‧ a‧ a‧ b", " y"],
    // );

    // voluntary stop of the subgrammar
    for max_tokens in &[3, 4, 5] {
        let c = check_lark_grammar_nested(
            &format!(
                r#"start: " x x x" (" q")* " x" ab " y"
                   ab[capture,max_tokens={max_tokens}]: @sub
                "#,
            ),
            r#"start: (" a")* " b""#,
            &[" x‧ x‧ x", " q‧ q‧ q‧ q‧ x‧ a‧ a‧ b", " y"],
        );
        check_capture(&c, "ab", " a a b");

        // no unique start marker
        let c = check_lark_grammar_nested(
            &format!(
                r#"start: " x x x" (" q")* ab " y"
                   ab[capture,max_tokens={max_tokens}]: @sub
                "#,
            ),
            r#"start: (" a")* " b""#,
            &[" x‧ x‧ x", " q‧ q‧ q‧ q‧ a‧ a‧ b", " y"],
        );
        check_capture(&c, "ab", " a a b");
    }

    // forced stop of the subgrammar
    let c = check_lark_grammar_nested(
        r#"start: " x x x" (" q")* " x" ab " y"
           ab[capture,max_tokens=3]: @sub
        "#,
        r#"start: (" a")* " b""#,
        &[" x‧ x‧ x", " q‧ q‧ q‧ q‧ x‧ a‧ a‧ a", " y"],
    );
    check_capture(&c, "ab", " a a a");
    // and with no unique start marker
    let c = check_lark_grammar_nested(
        r#"start: " x x x" (" q")* ab " y"
           ab[capture,max_tokens=3]: @sub
        "#,
        r#"start: (" a")* " b""#,
        &[" x‧ x‧ x", " q‧ q‧ q‧ q‧ a‧ a‧ a", " y"],
    );
    check_capture(&c, "ab", " a a a");

    // TODO we're off by one here
    let c = check_lark_grammar_nested(
        r#"start: " x x x" ab " y"
           ab[capture,max_tokens=2]: @sub
        "#,
        r#"start: (" a")* " b""#,
        &[" x‧ x‧ x", " a‧ a‧ a", " y"],
    );
    check_capture(&c, "ab", " a a a");

    // TODO we're off by one here
    let c = check_lark_grammar_nested(
        r#"start: ab " y"
           ab[capture,max_tokens=2]: @sub
        "#,
        r#"start: (" a")* " b""#,
        &["", " a‧ a‧ a", " y"],
    );
    check_capture(&c, "ab", " a a a");
}

#[test]
fn test_ll_lexeme_subgrammar_max_tokens() {
    check_lark_grammar_nested(
        r#"start: " x" ab " y"
           ab[max_tokens=3]: @sub
        "#,
        r#"start: TEXT
           TEXT: (" a")* " b"
        "#,
        &[" x", " a‧ a‧ a", " y"],
    );

    // TODO check_tokens() should increment token_idx and we should somehow test it
}

#[test]
fn test_ll_nested_temp() {
    check_lark_grammar(
        r#"start: /[xy]/ sub_temp
           sub_temp[temperature=0.5]: %lark {
                start: "[" ("A")* "]"
                %ignore /[ \t]+/
           }
        "#,
        &["", "x‧[‧]"],
    );
}

#[test]
fn test_ll_temperature() {
    check_lark_json(
        r#"start: "JSON" j
               j[temperature=0.3]: @sub
            "#,
        json!({
            "type": "array"
        }),
        &["JSON", "[‧1‧,‧2‧]"],
    );

    check_lark_grammar_nested(
        r#"start: /[xy]/ sub_temp
           sub_temp[temperature=0.5]: @sub
        "#,
        r#"start: "[" ("A")* "]"
           %ignore /[ \t]+/"#,
        &["", "x‧[‧]"],
    );

    check_lark_grammar_nested(
        r#"start: sub_temp
           sub_temp[temperature=0.5]: @sub
        "#,
        r#"start: "[" ("A")* "]"
           %ignore /[ \t]+/"#,
        &["", "[‧]"],
    );

    check_lark_grammar_nested(
        r#"start: sub_temp
           sub_temp[temperature=0.5]: @sub
        "#,
        r#"start: "[" ("A")* "]"
           %ignore /[ \t]+/"#,
        &["", "[]"],
    );

    check_lark_grammar_nested(
        r#"start: sub_temp
           sub_temp[temperature=0.5]: @sub
        "#,
        r#"start: "[" ("A")* "]"
        "#,
        &["", "[‧]"],
    );

    check_lark_grammar(
        r#"start: gen
           gen[temperature=0.5, stop=""]: /.*/
        "#,
        &["", "foo‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: foo | bar
           foo[temperature=0.5]: "foo"
           bar[temperature=0.5]: "bar"
        "#,
        &["", "foo"],
    );
}

#[test]
fn test_ll_backtrack_stop() {
    check_lark_grammar(
        r#"
            start: "Count to 10: 1, 2, 3, 4, 5, 6, 7, " text "\nNot quite."
            text[stop=","]: /.+/
        "#,
        &[
            "Count‧ to‧ ‧1‧0‧:‧ ‧1‧,‧ ‧2‧,‧ ‧3‧,‧ ‧4‧,‧ ‧5‧,‧ ‧6‧,‧ ‧7‧,",
            " ‧8‧,",
            "1↶\n‧Not‧ quite‧.",
        ],
    );

    check_lark_grammar(
        r#"
            start: "Name: " name "\nName: " name
            name[stop=STOP]: /E[a-z]+/
            STOP: /[a-b]/ | /[x-z]/
        "#,
        &["Name‧:", " Em‧ily", "1↶il‧\n‧Name‧:", " Emil‧ie‧a", "1↶"],
    );
}

#[test]
fn test_ll_stop_heal() {
    // https://github.com/guidance-ai/guidance/issues/1131
    check_lark_grammar_prompt(
        r#"
            start: gen "foo"
            gen[stop=/"/]: /.*/
        "#,
        "Hello, text: ",
        &["Hello‧,‧ text‧:", " \"", "1↶ foo"],
    );
}

#[test]
fn test_llparser() {
    check_lark_grammar_prompt(
        r#"
            start: gen
            gen[stop=""]: /.*/
        "#,
        "2 + 2 =",
        &["2‧ +‧ ‧2", " =>‧ ‧4‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"
            start: "Power frequency is " num "Hz; voltage is " num "V"
            num[stop="", max_tokens=5]: /[0-9]+/
        "#,
        &[
            "Power‧ frequency‧ is‧ ",
            "5‧0‧Hz", // no EoS needed on 50Hz
            ";‧ voltage‧ is‧ ",
            "2‧2‧0‧V",
        ],
    );

    // version with no stop=""; see https://github.com/guidance-ai/llguidance/issues/129
    check_lark_grammar(
        r#"
            start: "Power frequency is " num "Hz; voltage is " num "V"
            num[max_tokens=5]: /[0-9]+/
        "#,
        &[
            "Power‧ frequency‧ is‧ ",
            "5‧0‧Hz",
            ";‧ voltage‧ is‧ ",
            "2‧2‧0‧V",
        ],
    );

    check_lark_grammar(
        r#"
            start: "Power frequency is " num "Hz; voltage is " num "V"
            num[stop="", max_tokens=3]: /[0-9]+/
        "#,
        &[
            "Power‧ frequency‧ is‧ ",
            "5‧0‧Hz", // no EoS needed on 50Hz
            ";‧ voltage‧ is‧ ",
            "2‧2‧0",
            "V", // V is forced since max_tokens=3
        ],
    );

    check_lark_grammar(
        r#"
            start: "Q: Are dolphins fish?\nA: " ANSWER "\nQ: Are sharks fish?\nA: " ANSWER
            ANSWER: "Yes" | "No"
        "#,
        &[
            "Q‧:‧ Are‧ dol‧ph‧ins‧ fish‧?‧\n‧A‧:",
            " No", // note the prefix space - moved by token healing
            "\n‧Q‧:‧ Are‧ sh‧arks‧ fish‧?‧\n‧A‧:",
            " Yes",
        ],
    );

    check_lark_grammar(
        r#"
            start: "Q: 7 * 8\nA: " NUMBER
            NUMBER: /[0-9]+/
        "#,
        &["Q‧:‧ ‧7‧ *‧ ‧8‧\n‧A‧:‧ ", "5‧6‧≺EOS≻"],
    );
}

#[test]
fn test_ll_nullable_lexeme() {
    // make sure 'a' is not forced
    check_lark_grammar(
        r#"start: gen
           gen[stop=""]: /a*/"#,
        &["", "a‧≺EOS≻"],
    );

    // this one doesn't work - no lexeme was scanned by EOS, so we allow more lexemes...
    check_lark_grammar(
        r#"start: gen
           gen[stop=""]: /a*/"#,
        &["", "≺EOS≻"],
    );

    // see that we can skip 5*
    check_lark_grammar(
        r#"start: "6 * 7 = " five_seq num "\n"
           five_seq[stop=""]: /5*/
           num[stop=""]: /[1-4][0-9]/"#,
        &["6‧ *‧ ‧7‧ =‧ ", "4‧2", "\n"],
    );

    check_lark_grammar_nested(
        r#"start: "Here: 2 + 2 = " @sub"#,
        r#"start: /[0-9]+/"#,
        &["Here‧:‧ ‧2‧ +‧ ‧2‧ =‧ ", "4‧≺EOS≻"],
    );

    // make sure it stops at EOS
    check_lark_grammar_nested(
        r#"start: "Here: 2 + 2 = " @sub"#,
        r#"start: num q
           num: /[0-9]+/
           q: /Q?/
        "#,
        &["Here‧:‧ ‧2‧ +‧ ‧2‧ =‧ ", "4‧≺EOS≻"],
    );

    let float_grammar = r#"
        start: num1 | num2
        num1: /-?(?:0|[1-9][0-9]*)/
        num2: /-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)/
    "#;
    check_lark_grammar_nested(r#"start: @sub"#, float_grammar, &["", "1‧≺EOS≻"]);
    check_lark_grammar_nested(r#"start: @sub"#, float_grammar, &["", "0‧≺EOS≻"]);
    check_lark_grammar_nested(r#"start: @sub"#, float_grammar, &["", "1‧.‧1‧≺EOS≻"]);
    check_lark_grammar_nested(r#"start: @sub"#, float_grammar, &["", "0‧.‧1‧≺EOS≻"]);
}

#[test]
fn test_ll_pop_tokens() {
    // check_grammar(grm, ["6‧ *‧ ‧7‧ =‧ ", "4‧2‧\n"])
    // grm = "6 * 7 = " + subgrammar(body=lexeme("[0-9]{1,3}")) + "\n"
    check_lark_grammar(
        r#"start: "6 * 7 = " NUM "\n"
           NUM: /[0-9]{1,3}/
        "#,
        &["6‧ *‧ ‧7‧ =‧ ", "4‧2‧\n"],
    );
}

#[test]
fn test_ll_nice_man() {
    let grm = r#"start: ("a" | "ab" | "c")"#;
    let grm_d = r#"start: ("a" | "ab" | "c") ("d")"#;
    let grm_opt_d = r#"start: ("a" | "ab" | "c") ("d" | "")"#;

    check_lark_grammar(grm, &["", "a‧b"]);
    check_lark_grammar(grm, &["", "a‧≺EOS≻"]);
    check_lark_grammar(grm_d, &["", "a‧d"]);
    check_lark_grammar(grm_d, &["", "a‧b", "d"]);

    check_lark_grammar(grm_opt_d, &["", "a‧b‧d"]);
    check_lark_grammar(grm_opt_d, &["", "a‧b‧≺EOS≻"]);
    check_lark_grammar(grm_opt_d, &["", "a‧≺EOS≻"]);

    // TODO: this should also work for "abq" as a single lexeme
    // https://github.com/guidance-ai/llguidance/issues/2
    let abq = r#"start: ("a" | "a" "bq" | "c") ("bQ" | "")"#;
    check_lark_grammar(abq, &["", "a‧b‧q‧≺EOS≻"]);
    check_lark_grammar(abq, &["", "a‧b‧Q"]);
}

#[test]
fn test_ll_stop_quote_comma() {
    let grm = r#"
        start: "{ \"items\": [\"" ap "\",\n   \"" bp "\"] }"
        ap[stop="\""]: /a+/
        bp[stop="\""]: /b+/
    "#;

    // make sure we allow ", as a single token; also "]
    check_lark_grammar(
        grm,
        &["{‧ \"‧items‧\":‧ [\"", "a‧\",", "\n‧  ‧ \"", "b‧\"]", " }"],
    );

    // and as seprate tokens
    check_lark_grammar(
        grm,
        &[
            "{‧ \"‧items‧\":‧ [\"",
            "a‧\"",
            ",‧\n‧  ‧ \"",
            "b‧\"",
            "]‧ }",
        ],
    );
}

#[test]
fn test_ll_nullable_bug() {
    let c = check_lark_grammar(
        r#"start: s | "foo"
           s[capture]: maybe_a maybe_a maybe_a maybe_a
           maybe_a: "a" | ""
        "#,
        &["", "a‧≺EOS≻"],
    );
    check_capture(&c, "s", "a");
}

#[test]
fn test_ll_max_tokens() {
    check_lark_grammar(
        r#"start: "Name: " name " Height: " height
           name[max_tokens=3, stop=""]: /.*/
           height[max_tokens=3, stop=""]: /.*/
        "#,
        &["Name‧:", " Em‧ily‧ Carter", " Height‧:", " ‧5‧'‧6"],
    );

    // here we have two gen() with the same regex (so they are the same lexeme)
    // but different max_tokens limits
    check_lark_grammar(
        r#"start: "Name: " name " Height: " height
           name[max_tokens=2, stop=""]: /.*/
           height[max_tokens=3, stop=""]: /.*/
        "#,
        &["Name‧:", " Em‧ily", " Height‧:", " ‧5‧'‧6"],
    );

    // now this is a strange case, where gen() is allowed together with the following
    // string, and gen() runs out of tokens, so the fixed string takes over
    // note how Emily is not repeated
    check_lark_grammar(
        r#"start: "Name: " name "Emily Carter is great; Height: " height
           name[max_tokens=2, stop=""]: /.*/
           height[max_tokens=3, stop=""]: /.*/
        "#,
        &[
            "Name‧:",
            " Em‧ily",
            " Carter‧ is‧ great‧;‧ Height‧:",
            " ‧5‧'‧6",
        ],
    );
}

#[test]
fn test_ll_special_token() {
    check_lark_grammar(
        r#"start: <|system|> /.*/
        "#,
        &["<|system|>", "✖<|system|>‧foo‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: hd /.*/
           hd: <|system|> | <|user|>
        "#,
        &["", "✖<|assistant|>‧<|system|>‧foo‧✖<|system|>‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: hd /.*/
           hd: <|system|> | <|user|>
        "#,
        &["", "<|user|>‧foo‧✖<|system|>‧foo‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: /.*/ <|system|>
        "#,
        &["", "foo‧✖<|user|>‧bar‧✖≺EOS≻‧<|system|>"],
    );

    check_lark_grammar(
        r#"start: /.*/ <|system|>
        "#,
        &["", "✖<|user|>‧<|system|>"],
    );

    check_lark_grammar_prompt(
        r#"
            start: /[a-z]/
        "#,
        "<|system|>",
        &["<|system|>", "a"],
    );

    check_lark_grammar_prompt(
        r#"
            start: /[a-z]/
        "#,
        "</s>",
        &["</s>", "a"],
    );

    check_lark_grammar(
        r#"start: /.*/ <|system|>
        "#,
        &["", "✖<|end|>‧foo‧✖<|end|>‧<|system|>"],
    );
}

#[test]
fn test_ll_token_ranges() {
    // 32001 <|assistant|>
    // 32006 <|system|>
    // 32010 <|user|>

    check_lark_grammar(
        r#"start: hd /.*/
           hd: <[32006,32010]>
        "#,
        &["", "✖<|assistant|>‧<|system|>‧foo‧✖<|system|>‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: hd /.*/
           hd: <[32002-32010]>
        "#,
        &["", "✖<|assistant|>‧<|system|>‧foo‧✖<|system|>‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: hd /.*/
           hd: <[32002-32006,32010]>
        "#,
        &["", "✖<|assistant|>‧<|system|>‧foo‧✖<|system|>‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: <[32006]> /.*/
        "#,
        &["<|system|>", "✖<|system|>‧foo‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: <[*]> /.*/
        "#,
        &["", "<|system|>‧foo‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: <[*]> /.*/
        "#,
        &["", "foo‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: <[^32001-32005,32007-32010]> /.*/
        "#,
        &["", "foo‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: <[^32001-32005,32007-32010]> /.*/
        "#,
        &["", "<|system|>‧foo‧≺EOS≻"],
    );
}

#[test]
fn test_ll_inline_json() {
    let grm = r#"
start: hd obj
obj: %json {
    "type": "object",
    "properties": {
        "a": {
            "type": "number"
        }
    }
}
hd: "JSON"
"#;

    check_lark_grammar(grm, &["JSON", "{\"‧a‧\":‧ ‧✖true‧5‧}"]);

    let grm = r#"
start: "JSON" obj
obj: %json{
    "type": "object",
    "properties": {
        "a": {
            "type": "number"
        }
    }
} "END"
"#;

    check_lark_grammar(grm, &["JSON", "{\"‧a‧\":‧ ‧✖true‧5‧}", "END"]);
}

#[test]
fn test_ll_numeric_token_for_text() {
    // 5431 foo
    // 5432 _calling
    // 5426 long
    // 5427 asta
    // 32006 <|system|>

    check_lark_grammar(
        r#"start: <[5431]>* <[34001]> <[34100-34110]>*
        "#,
        &["", "<[5431]>‧<[34001]>‧<[34100]>‧<[34101]>‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: <[5431]>* <[5432]> <[5426-5427]>*
        "#,
        &["", "✖<|assistant|>✖f✖long‧foo‧ calling‧long‧asta‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: <[5431]>* <[5432]> <[5426-5427]>*
        "#,
        &[
            "",
            "✖<|assistant|>✖f✖l‧<[5431]>‧<[5432]>‧<[5426]>‧<[5427]>‧<[5426]>‧≺EOS≻",
        ],
    );

    check_lark_grammar(
        r#"start: foo | bar
           foo: <[5431-5432]> /.*/
           bar: <[32006]> /.*/
        "#,
        &["", "✖<|assistant|>✖f‧foo‧bar‧long‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: foo | bar
           foo: <[5431-5432,9000-9010]> /.*/
           bar: <[32006]> /.*/
        "#,
        &["", "✖<|assistant|>✖f‧foo‧bar‧long‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: foo
           foo: <[32006,9000-9010]> /.*/
        "#,
        &["", "✖<|assistant|>✖f‧reh‧bar‧long‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: foo | bar
           foo: <[5431]> <[5426-5427]> /.*/
           bar: <[32006]> /.*/
        "#,
        &["", "✖<|assistant|>✖f‧foo‧✖bar‧long‧✖<|system|>‧cat‧≺EOS≻"],
    );

    check_lark_grammar(
        r#"start: f | foo | bar
           f: <[29730]> <[105]>
           foo: <[29730]> <[5431]>
           bar: <[29842]>
        "#,
        &["", "zott‧foo"],
    );

    // check_lark_grammar(
    //     r#"start: f | foo | bar
    //        f: <[29730]> <[105]> <[29659]>
    //        foo: <[29730]> <[5431]> <[29659]>
    //        bar: <[29842]>
    //     "#,
    //     &["", "zott‧foo", "coded"],
    // );
}

#[test]
fn test_ll_numeric_and_text() {
    check_lark_grammar(
        r#"start: <[5432]> <[5426]> | "qux"
        "#,
        &["", "<[5432]>", "long"],
    );

    check_lark_grammar(
        r#"start: <[5432]> <[5426]> | " qux"
        "#,
        &["", "<[5432]>", "long"],
    );
}

#[test]
fn test_ll_dolphin() {
    let grms = &[
        r#"start: "Dolphin name: " NAME ","
           NAME: "\"" /[A-Z]/ /[a-z]+/ "\""
        "#,
        // regular gen()
        r#"start: "Dolphin name: " name ","
           name[stop=""]: /"[A-Z][a-z]+"/
        "#,
        // regular gen(), comma in regex
        r#"start: "Dolphin name: " name
           name[stop=""]: /"[A-Z][a-z]+",/
        "#,
        // regular gen(), quotes outside
        r#"start: "Dolphin name: \"" name "\","
           name[stop=""]: /[A-Z][a-z]+/
        "#,
    ];

    // separate comma
    for g in grms {
        check_lark_grammar(g, &["D‧olph‧in‧ name‧:‧ \"", "F‧li‧pper‧\"", ","]);
    }

    // check that we allow `",` as a single token:
    for g in grms {
        check_lark_grammar(g, &["D‧olph‧in‧ name‧:‧ \"", "F‧li‧pper‧\","]);
    }
}

#[test]
fn test_two_jsons() {
    let grm = r#"
start: "JSON" (obj1 | obj2)
obj1: %json {
    "type": "object",
    "properties": {
        "a": {},
        "tp": { "const": 1 }
    },
    "required": ["tp"]
}
obj2: %json {
    "type": "object",
    "properties": {
        "b": {},
        "tp": { "const": 2 }
    },
    "required": ["tp"]
}
"#;

    check_lark_grammar(grm, &["JSON", "{\"‧tp‧\":‧ ‧1‧}"]);
    check_lark_grammar(grm, &["JSON", "{\"‧tp‧\":‧ ‧2‧}"]);

    // workaround
    let grm = r#"
    start: "JSON" obj
    obj: %json {
        "anyOf": [{
            "type": "object",
            "properties": {
                "a": {},
                "tp": { "const": 1 }
            },
            "required": ["tp"]
        }, {
            "type": "object",
            "properties": {
                "b": {},
                "tp": { "const": 2 }
            },
            "required": ["tp"]
        }]
    }
    "#;

    check_lark_grammar(grm, &["JSON", "{\"‧tp‧\":‧ ‧1‧}"]);
    check_lark_grammar(grm, &["JSON", "{\"‧tp‧\":‧ ‧2‧}"]);
}

#[test]
fn test_ll_special_capture() {
    let c = check_lark_grammar(
        r#"start: cap | foo
           cap[capture]: <|system|> %json { }
           foo: "foo"
        "#,
        &["", "<|system|>‧{‧}"],
    );
    // TODO https://github.com/guidance-ai/llgtrt/issues/12
    check_capture(&c, "cap", "<|system|>{}");
}

#[test]
fn test_lazy_tool() {
    let c = check_lark_grammar(
        r#"
            start: "<tool_name>" name_c "<tool_data>" data_c "</tool_data>"
            name_c[capture]: name
            name[lazy]: /.*/ "</tool_name>"
            data_c[capture]: data
            data: %json {
                "properties": {
                    "foo": { "type": "string" }
                },
                "required": ["foo"]
            }
        "#,
        &[
            "<‧tool‧_‧name",
            ">‧foo‧<‧bar‧></‧tool‧_‧name‧><",
            "tool‧_‧data",
            ">{‧\"",
            "foo",
            "\":‧ \"‧bar‧\"}",
            "</‧tool‧_‧data‧>",
        ],
    );

    check_capture(&c, "name_c", "foo<bar></tool_name>");
    check_capture(&c, "data_c", "{\"foo\": \"bar\"}");
}

// this crashes
// #[test]
// fn test_stop_tool() {
//     check_lark_grammar(
//         r#"
//             start: "<tool_name>" name_c "</tool_name><tool_data>" data_c "</tool_data>"
//             name_c[capture]: name
//             name[stop="</tool_name>"]: /.*/
//             data_c[capture]: data
//             data: %json {
//                 "properties": {
//                     "foo": { "type": "string" }
//                 },
//                 "required": ["foo"]
//             }
//         "#,
//         &[
//             "<‧tool‧_‧name",
//             ">‧foo‧<‧bar‧></‧tool‧_‧name‧><‧tool‧_‧data‧>{‧\"‧foo‧\":‧ \"‧bar‧\"}‧</‧tool‧_‧data‧>",
//             "",
//         ],
//     );
// }

#[test]
fn test_suffix_tool() {
    let c = check_lark_grammar(
        r#"
            start: "<tool_name>" name "<tool_data>" data "</tool_data>"
            name[capture]: name_inner
            name_inner[capture, stop_capture="stop_c", suffix="</tool_name>"]: /.*/
            data[capture]: %json {
                "properties": {
                    "foo": { "type": "string" }
                },
                "required": ["foo"]
            }
        "#,
        &[
            "<‧tool‧_‧name",
            ">‧foo‧<‧bar‧></‧tool‧_‧name‧><",
            "tool‧_‧data",
            ">{‧\"",
            "foo",
            "\":‧ \"‧bar‧\"}",
            "</‧tool‧_‧data‧>",
        ],
    );
    check_capture(&c, "data", "{\"foo\": \"bar\"}");
    check_capture(&c, "stop_c", "</tool_name>");
    check_capture(&c, "name_inner", "foo<bar>");
    check_capture(&c, "name", "foo<bar></tool_name>");
}

#[test]
fn test_ll_initial_capture() {
    // https://github.com/guidance-ai/llguidance/issues/122
    let c = check_lark_grammar(
        r#"start: "This is a" text
           text[capture]: "nope" | ""
        "#,
        &["This‧ is", " ano", "pe"],
    );
    check_capture(&c, "text", "nope");

    let c = check_lark_grammar(
        r#"start: "This is a" text
           text[capture]: "nope" | ""
        "#,
        &["This‧ is", " a‧≺EOS≻"],
    );
    check_capture(&c, "text", "");
}

const TOOL_STR_GRAMMAR: &str = r#"
start: ( f1 | f2 )* f_end

// this will just run for as long as it takes
f_end: TEXT

// <function=name>..args..</function>
f1: f1_hd ( f1_foo | f1_bar ) "</function>"
f1_hd[lazy]: TEXT "<function"
f1_foo[capture]: "=foo>" /[a-z]+/
f1_bar[capture]: "=bar>" /[A-Z]+/

// assume model also does this
// <tool=name>..args..</tool>
f2: f2_hd f2_baz "</tool>"
f2_hd[lazy]: TEXT "<tool"
f2_baz[capture]: "=baz>" /[0-9]+/

TEXT: /(\n|.)*/
"#;

const TOOL_STR_GRAMMAR_2: &str = r#"
start: ( f_foo | f_bar | f_baz )* f_end

// this will just run for as long as it takes
f_end: TEXT

f_foo_hd[lazy]: TEXT "<function"
f_foo: f_foo_hd "=foo>" /[a-z]+/ "</function>"

f_bar_hd[lazy]: TEXT "<function"
f_bar: f_bar_hd "=bar>" /[A-Z]+/ "</function>"

f_baz_hd[lazy]: TEXT "<tool"
f_baz: f_baz_hd "=baz>" /[0-9]+/ "</tool>"

TEXT: /(\n|.)*/
"#;

#[test]
fn test_ll_tool_str_prototype() {
    let tool_chk = &[
        "",
        "Some‧ text‧<‧function",
        "=",
        "✖baz‧foo",
        ">",
        "✖≺EOS≻✖7‧abc‧</",
        "function",
        "✖7✖≺EOS≻‧>‧Text‧ between‧<‧tool",
        "=‧baz‧>",
        "✖abc‧1‧2‧3‧</",
        "tool",
        ">‧<‧function",
        "=",
        "bar",
        ">",
        "ABC‧</",
        "function",
        ">‧More‧ text‧≺EOS≻",
    ];

    let mut tool_chk2 = tool_chk.to_vec();
    tool_chk2.pop();
    tool_chk2.push(">‧≺EOS≻");

    let c = check_lark_grammar(TOOL_STR_GRAMMAR, tool_chk);
    check_capture(&c, "f1_foo", "=foo>abc");
    check_capture(&c, "f1_bar", "=bar>ABC");
    check_capture(&c, "f2_baz", "=baz>123");

    check_lark_grammar(TOOL_STR_GRAMMAR, &tool_chk2);

    check_lark_grammar(TOOL_STR_GRAMMAR, &["", "More‧ text‧≺EOS≻"]);

    check_lark_grammar(TOOL_STR_GRAMMAR_2, tool_chk);
    check_lark_grammar(TOOL_STR_GRAMMAR_2, &tool_chk2);
    check_lark_grammar(TOOL_STR_GRAMMAR_2, &["", "More‧ text‧≺EOS≻"]);
}

const TOOL_STR_GRAMMAR_JSON: &str = r#"
start: ( f_foo | f_bar | f_baz )* f_end

// this will just run for as long as it takes
f_end: TEXT

f_foo_hd[lazy]: TEXT "<function"
f_foo: f_foo_hd "=foo>" %json { "type": "object" } "</function>"

f_bar_hd[lazy]: TEXT "<function"
f_bar: f_bar_hd "=bar>" /[A-Z]+/ "</function>"

f_baz_hd[lazy]: TEXT "<tool"
f_baz: f_baz_hd "=baz>" /[0-9]+/ "</tool>"

TEXT: /(\n|.)*/
"#;

#[test]
fn test_ll_tool_str_json() {
    let tool_chk = &[
        "",
        "Some‧ text‧<‧function",
        "=",
        // we do not allow whitespace in front of json
        // we do allow it inside
        "✖baz‧foo‧>✖≺EOS≻✖7✖ ‧{‧ ✖7‧}",
        // we do not allow it after
        "</‧function",
        "✖7✖≺EOS≻‧>‧Text‧ between‧<‧tool",
        "=‧baz‧>",
        "✖abc‧1‧2‧3‧</",
        "tool",
        ">‧<‧function",
        "=",
        "bar",
        ">",
        "ABC‧</",
        "function",
        ">‧More‧ text‧≺EOS≻",
    ];

    check_lark_grammar(TOOL_STR_GRAMMAR_JSON, tool_chk);
}

const TOOL_STR_SPEC_GRAMMAR: &str = r#"
start: ( f_foo | f_bar | f_baz | f_qux | f_mux )* f_end

// this will just run for as long as it takes
f_end: TEXT

f_foo_hd[lazy]: TEXT "<function"
f_foo: f_foo_hd "=foo>" /[a-z]+/ "</function>"

f_bar_hd[lazy]: TEXT "<function"
f_bar: f_bar_hd "=bar>" /[A-Z]+/ "</function>"

f_baz_hd[lazy]: TEXT "<tool"
f_baz: f_baz_hd "=baz>" /[0-9]+/ "</tool>"

f_qux: TEXT <|placeholder1|> "qux(" /[0-9]+/ ")"
f_mux: TEXT <|placeholder1|> "mux(" /[0-9]+/ ")"

TEXT: /(\n|.)*/
"#;

#[test]
fn test_ll_tool_str_spec() {
    let tool_chk = &[
        "",
        "Some‧ text‧<‧function",
        "=",
        "✖baz‧foo",
        ">",
        "✖≺EOS≻✖7‧abc‧</",
        "function",
        "✖7✖≺EOS≻‧>‧Text‧ between‧<‧tool",
        "=‧baz‧>",
        "✖abc‧1‧2‧3‧</",
        "tool",
        ">‧<‧function",
        "=",
        "bar",
        ">",
        "ABC‧</",
        "function",
        ">‧More‧ text‧<|placeholder1|>‧qu",
        "x‧(",
        "1‧7‧)‧≺EOS≻",
    ];

    check_lark_grammar(TOOL_STR_SPEC_GRAMMAR, tool_chk);
}

#[test]
fn test_ll_numeric_bug() {
    check_lark_grammar(
        r#"
            start: text
            text: (text_tokens)* <[33000]> ap
            ap: <[33001]> (atok*)
            atok: <[400-410]>
            text_tokens: <[300-310]>
        "#,
        &[
            "",
            "<[300]>‧<[33000]>",
            "<[33001]>",
            "<[401]>‧<[402]>‧≺EOS≻",
        ],
    );

    check_lark_grammar(
        r#"
            start: text
            text: (text_tokens)* ( (<[33000]> ap) | (<[33002]> (atok*)) )
            ap: <[33001]> (atok*)
            atok: <[400-410]>
            text_tokens: <[300-310]>
        "#,
        &[
            "",
            "<[300]>‧<[33000]>",
            "<[33001]>",
            "<[401]>‧<[402]>‧≺EOS≻",
        ],
    );

    check_lark_grammar(
        r#"
            start: text
            text: (text_tokens)* ( (<[33000]> ap) | (<[33002]> (atok*)) )
            ap: <[33001,33003]> (atok*)
            atok: <[400-410]>
            text_tokens: <[300-310]>
        "#,
        &[
            "",
            "✖<[33001]>‧<[300]>‧<[33000]>✖<[33002]>✖<[300]>‧<[33001]>✖<[33002]>✖<[300]>‧<[401]>‧<[402]>‧≺EOS≻",
        ],
    );
}

#[test]
fn test_stop_crash() {
    // this fails, see https://github.com/guidance-ai/llguidance/issues/182
    // check_lark_grammar(
    //     r#"
    //         start: prosandcons "Best=" best
    //         prosandcons[capture, temperature=0.0, max_tokens=600, stop="Best="]: /(?s:.*)/
    //         best[capture]: /[0-9]+/
    //     "#,
    //     &["", " wait‧ times‧.‧\n‧Best‧=‧3‧≺EOS≻"],
    // );

    check_lark_grammar(
        r#"
            start: prosandcons best
            prosandcons[capture, temperature=0.0, max_tokens=600, suffix="Best="]: /(?s:.*)/
            best[capture]: /[0-9]+/
        "#,
        &["", " wait‧ times‧.‧\n‧Best‧=‧3‧≺EOS≻"],
    );
}
