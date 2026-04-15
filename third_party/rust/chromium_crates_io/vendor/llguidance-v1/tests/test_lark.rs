use llguidance::substring::chunk_into_words;
use rand::{rngs::SmallRng, Rng, SeedableRng};
use serde_json::json;

use llg_test_utils::*;

#[test]
fn test_dot_unicode() {
    lark_str_test_many(
        r#"start: /.../ "abc" /.../"#,
        &[
            "abcabcabc",
            "aaaabcccc",
            // NOTE: Also ensures that multi-byte characters still count as a single character
            "🔵🟠✅abc❌🟠🔵",
        ],
        &[
            "aaabcccc",
            "aaaaabcccc",
            "FINAL_REJECT:aaaabccc",
            "aaaabccccc",
            "🔵🟠✅❌abc❌✅🟠🔵",
            "🔵🟠abc🟠🔵",
        ],
    );
}

#[test]
fn test_lark_syntax_general() {
    lark_err_test(r#"root: "abc" "def""#, "no start");

    lark_err_test(
        r#"
            start: foo{7,6}
            foo: "a" | "b"
        "#,
        "range end must be >= start",
    );
    lark_err_test(
        r#"
            start: foo{-1,}
            foo: "a" | "b"
        "#,
        "range start must be >= 0",
    );
    lark_err_test(
        r#"
            start: foo{0,-1}
            foo: "a" | "b"
        "#,
        "range end must be >= start",
    );

    lark_err_test(
        r#"
            start: FOO
            FOO: ("a" | "b"){7,6}
        "#,
        "range end must be >= start",
    );
    lark_err_test(
        r#"
            start: FOO
            FOO: ("a" | "b"){-1,}
        "#,
        "range start must be >= 0",
    );
    lark_err_test(
        r#"
            start: FOO
            FOO: ("a" | "b"){0,-1}
        "#,
        "range end must be >= start",
    );

    lark_err_test(
        r#"
            start: FOO
            FOO: "a" | BAR
            BAR: "b" FOO
        "#,
        "circular reference in token",
    );

    lark_ok(
        r#"
            start: foo
            foo: "a" | bar
            bar: "b" foo
        "#,
    );

    lark_err_test(
        r#"
            start: FOO
            BAR: "b"
        "#,
        "unknown name",
    );

    lark_err_test(
        r#"
            start: foo
            bar: "b"
        "#,
        "unknown name",
    );

    lark_err_test(
        r#"
            start: BAR
            BAR: BAZ "a"
        "#,
        r#"unknown name: "BAZ""#,
    );

    lark_ok(
        r#"
            %import common.INT
            start: INT
        "#,
    );
    lark_err_test(
        r#"
            %import common.BLAH
            start: BLAH
        "#,
        "Unknown common",
    );

    lark_err_test(r#" start: /[abc/ "#, "invalid regex");
    lark_ok(r#" start: /[abc]/ "#);
    lark_err_test(r#" start: /[abc]/l "#, "l-flag is not supported");

    lark_err_test(
        r#"
            start: FOO
            FOO: @qux
        "#,
        "cannot be used in terminals",
    );
    lark_err_test(
        r#"
            start: FOO
            FOO: %json { }
        "#,
        "cannot be used in terminals",
    );
    lark_err_test(
        r#"
            start: FOO
            FOO: <[1234]>
        "#,
        "cannot be used in terminals",
    );
    lark_err_test(
        r#"
            start: FOO
            FOO: <|assistant|>
        "#,
        "cannot be used in terminals",
    );
    lark_err_test(
        r#"
            start: "A" | <|foobarbaz|>
        "#,
        "unknown special token",
    );

    lark_err_test(
        r#" start: "ab".."c" "#,
        "range start must be a single character",
    );
    lark_err_test(
        r#" start: "a".."cd" "#,
        "range end must be a single character",
    );
    lark_err_test(r#"  start: "d".."a" "#, "invalid range order");

    lark_err_test(r#"start: <[100-200-300]>"#, "invalid token range");
    lark_ok(r#"start: <[100-200,300-4002]>"#);
    lark_err_test(r#"start: <[100-200,100-200-300]>"#, "invalid token range");
    lark_err_test(r#"start: <[,]>"#, "empty token range");
    lark_err_test(r#"start: <[200-100]>"#, "invalid token range");
    lark_err_test(r#"start: <[200 - 100]>"#, "lexer error");
    lark_ok(r#"start: <[*]>"#);
    lark_err_test(
        r#"start: <[^*]>"#,
        "negated wildcard token <[^*]> is not supported",
    );
    lark_err_test(
        r#"start: <[*,100]>"#,
        "wildcard token range '*' must not contain additional tokens",
    );
    lark_ok(r#"start: <[^100,200-300]>"#);
    lark_ok(r#"start: <[^100-200,100-300]>"#);

    lark_err_test(
        r#"
            start: foo
            foo: "a" | "b"
            foo: "c"
        "#,
        "duplicate rule",
    );
    lark_err_test(
        r#"
            start: FOO
            FOO: "a" | "b"
            FOO: "c"
        "#,
        "duplicate token",
    );
    // Unterminated regex
    lark_err_test(
        r#"
        start: / "test"
        blah: "xyz"
    "#,
        "lexer error",
    );
    // Unterminated string
    lark_err_test(
        r#"
        start: "test
        blah: "xyz"
    "#,
        "lexer error",
    );
}

#[test]
fn test_lark_syntax_perc() {
    lark_err_test(r#"start: %json {"#, "EOF while parsing an object");
    lark_err_test(r#"start: %json { foo"#, "key must be a string");
    lark_err_test(r#"start: %json []"#, "failed to compile JSON schema");
    lark_err_test(
        r#"start: %json { "if": {} }"#,
        "failed to compile JSON schema",
    );

    lark_err_test(
        r#"
            %llguidance { "no_forcing": "yadda-dada"}
            start: "a" | "b"
        "#,
        "failed to parse %llguidance declaration",
    );

    lark_ok(r#" start: %regex { "substring_words": "foo bar" } "#);
    lark_ok(r#" start: %regex { "substring_chars": "foo bar" } "#);
    lark_ok(r#" start: %regex { "substring_chunks": ["foo", "bar"] } "#);

    lark_err_test(
        r#" start: %regex { "substring_words": true } "#,
        "failed to parse %regex",
    );

    lark_err_test(r#" start: %regex { "foobar": true } "#, "unknown field");

    lark_err_test(
        r#" start: %regex { "substring_words": "aa", "substring_chars": "bb" } "#,
        "only one field can be set on %regex",
    );

    lark_err_test(r#" start: %regex {  } "#, "no fields set on %regex");
}

#[test]
fn test_lark_syntax_attributes() {
    lark_ok(
        r#" start: foo
            foo[stop=""]: /.*/ "#,
    );

    lark_ok(
        r#" start: foo
            foo[stop="",max_tokens=12]: /.*/ "#,
    );

    lark_ok(
        r#" start: foo
            foo[capture,stop=""]: /.*/ "#,
    );

    lark_ok(
        r#" start: foo
            foo[capture="bar" , stop=""]: /.*/ "#,
    );

    lark_ok(
        r#" start: foo
            foo[stop = "foobar"]: /.*/ "#,
    );

    lark_ok(
        r#" start: foo
            foo[stop = /foobar/]: /.*/ "#,
    );

    lark_ok(
        r#" start: foo
            foo[stop = STOP]: /.*/
            STOP: "foobar"
        "#,
    );

    lark_ok(
        r#"
              start: %json {
                "x-guidance": {
                   "lenient": true
                },
                "oneOf": [
                    { "type": "object", "properties": { "foo": { "type": "string" } } },
                    { "type": "object", "properties": { "bar": { "type": "string" } } }
                ]
            }
        "#,
    );

    lark_err_test(
        r#" start: foo
            foo[foobar=12]: /.*/ "#,
        "Unknown attribute",
    );

    lark_err_test(
        r#" start: foo
            foo[stop=""="foo"]: /.*/ "#,
        "Expected token",
    );

    lark_err_test(
        r#" start: foo
            foo[max_tokens="foo"]: /.*/ "#,
        "Expected token",
    );
}

#[test]
fn test_repeat() {
    lark_str_test_many(
        r#"start:  ab{3,5}
           ab:  "a" | "b"
        "#,
        &["aba", "abaa", "aaaaa", "aabaa"],
        &["FINAL_REJECT:aa", "FINAL_REJECT:ab", "aaaaaa"],
    );

    lark_str_test_many(
        r#"start:  ab{3,}
           ab:  "a" | "b"
        "#,
        &["aba", "abaa", "aaaaa", "aabaa", "aaaaaa"],
        &["FINAL_REJECT:aa", "FINAL_REJECT:ab"],
    );

    lark_str_test_many(
        r#"start:  ab{,5}
           ab:  "a" | "b"
        "#,
        &["", "aa", "b", "aba", "abaa", "aaaaa", "aabaa"],
        &["aaaaaa"],
    );
}

#[test]
fn test_lexeme_substring_general() {
    for grm in &[
        r#" start: "A" %regex { "substring_words": "foo bar baz" } "B" "#,
        r#" start: SUB
            SUB: "A" %regex { "substring_words": "foo bar baz" } "B" "#,
    ] {
        lark_str_test_many(
            grm,
            &[
                "AfooB",
                "Abar bazB",
                "AbazB",
                "Afoo bar bazB",
                "Afoo bar B",
                "A bar bazB",
                "AB",
            ],
            &["FINAL_REJECT:Afoo bar baz", "AfoB"],
        );
    }

    lark_str_test_many(
        r#" start: "A" %regex { "substring_chunks": ["foo", " bar", " baz"] } "B" "#,
        &[
            "AfooB",
            "A bar bazB",
            "A bazB",
            "Afoo bar bazB",
            "Afoo barB",
            "AB",
            "A bar bazB",
        ],
        &["FINAL_REJECT:Afoo bar baz", "AfoB"],
    );
}

#[test]
fn test_lexeme_substring_chars_ascii() {
    lark_str_test_many(
        r#"start: %regex { "substring_chars": "The quick brown fox jumps over the lazy dog." }"#,
        &[
            "The quick brown fox jumps over the lazy dog.",
            "The quick brown fox",
            "he quick brow",
            "fox jump",
            "dog.",
        ],
        &["brown fx"],
    );
}

#[test]
fn test_lexeme_substring_chars_unicode() {
    lark_str_test_many(
        r#"start: %regex { "substring_chars": "빠른 갈색 여우가 게으른 개를 뛰어넘었다." }"#,
        &[
            "빠른 갈색 여우가 게으른 개를 뛰어넘었다.",
            "빠른 갈색 여우가 게으른",
            "른 갈색 여우",
            "여우가 게으",
            "뛰어넘었다.",
        ],
        &["갈색 여가"],
    );
}

#[test]
fn test_lexeme_substring_words_ascii() {
    lark_str_test_many(
        r#"start: %regex { "substring_words": "The quick brown fox jumps over the lazy dog." }"#,
        &[
            "The quick brown fox jumps over the lazy dog.",
            "The quick brown fox",
            "dog.",
        ],
        &["he quick brow", "FINAL_REJECT:fox jump", "brown fx"],
    );
}

#[test]
fn test_lexeme_substring_words_unicode() {
    lark_str_test_many(
        r#"start: %regex { "substring_words": "빠른 갈색 여우가 게으른 개를 뛰어넘었다." }"#,
        &[
            "빠른 갈색 여우가 게으른 개를 뛰어넘었다.",
            "빠른 갈색 여우가 게으른",
            "뛰어넘었다.",
        ],
        &["른 갈색 여우", "FINAL_REJECT:여우가 게으", "갈색 여가"],
    );
}

fn gen_words(seed: u32, num_words: usize) -> String {
    let letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.";
    let mut rnd = SmallRng::seed_from_u64((seed + 1) as u64);
    let mut words = vec![];
    let num_words = rnd.random_range((num_words / 2)..num_words);
    for _ in 0..num_words {
        let mut word = String::new();
        let len = rnd.random_range(1..15);
        for _ in 0..len {
            let idx = rnd.random_range(0..letters.len());
            word.push(letters.as_bytes()[idx] as char);
        }
        words.push(word);
    }
    words.join(" ")
}

fn quote_str(s: &str) -> String {
    serde_json::to_string(s).unwrap()
}

#[test]
fn test_large_select() {
    // it's kind of slow in non-release mode
    let num_words = if cfg!(debug_assertions) { 100 } else { 500 };
    let num_opt = if cfg!(debug_assertions) { 100 } else { 1500 };

    let t0 = std::time::Instant::now();
    let mut grm_sz = 0;

    for start in &["start: OPTS\nOPTS: ", "start: opts\nopts: "] {
        let mut grm_head = start.to_string();
        let mut grm_tail = "".to_string();
        let options = (0..num_opt)
            .map(|i| gen_words(i, num_words))
            .collect::<Vec<_>>();
        for (i, opt) in options.iter().enumerate() {
            grm_head.push_str(&format!("OPT{i} | "));
            grm_tail.push_str(&format!("OPT{}: {}\n", i, quote_str(opt)));
        }
        grm_head.push_str(" \"\"\n");
        let grm = format!("{grm_head}{grm_tail}");
        grm_sz = grm.len();

        lark_str_test_many_quiet(
            &grm,
            //&options.iter().map(|s| s.as_str()).collect::<Vec<_>>(),
            &[options[2].as_str(), options[7].as_str()],
            &["something that is unlikely to be in the options"],
        );
    }

    println!("large_select: {:?}; grm={}kB", t0.elapsed(), grm_sz / 1024);
}

#[test]
fn test_large_substring_words() {
    let words_str = gen_words(1, 5000);
    let words = chunk_into_words(&words_str);
    let grm = format!(
        "start: %regex {{ \"substring_words\": {} }}",
        quote_str(&words_str)
    );

    let mtch = words[50..100].to_vec().join("");
    let no_mtch = format!("{}{}", mtch, "XXX");
    lark_str_test_many_quiet(&grm, &[&mtch], &[&no_mtch]);
}

#[test]
fn test_large_substring_chars() {
    let chars = gen_words(2, 15000)[..10000].to_string();
    let grm = format!(
        "start: %regex {{ \"substring_chars\": {} }}",
        quote_str(&chars)
    );
    let mtch = chars[50..100].to_string();
    let no_mtch = format!("{}{}", mtch, "XXX");
    lark_str_test_many_quiet(&grm, &[&mtch], &[&no_mtch]);
}

#[test]
fn test_lexer_amb() {
    lark_str_test_many(
        r#"start: "'foo'" /a+/ | STRING /b+/
           STRING: /'[^']*'/
        "#,
        &["'foo'a", "'foo'aaa", "'bar'b", "'bar'bbb", "'foo'bb"],
        &["'bar'a", "'bar'c"],
    );
}

#[test]
fn test_edits() {
    let grm = r#"
start: ( step "\n" )* step ( "\n" final_comments )?
step: plan "\n" a_file
plan[lazy]: /((.|\n)*\n)?```/
final_comments: /[^`]*/

replace: "=======\n" repl_inner " REPLACE\n```"
repl_inner[lazy]: /(.|\n)*\n>>>>>>>/
SEARCH: "\n<<<<<<< SEARCH\n"

// "generated"

file_0: "gbnf_to_lark.py" SEARCH FILE_0 replace
a_file: file_0

FILE_0: %regex {
  "substring_chunks": [
    "foo\n",
    "bar\n",
    "baz\n",
    "line\n",
    "line\n"
  ]
}
"#;

    fn repl_block(filename: &str, src: &str, dst: &str) -> String {
        format!(
            "```\n{}\n<<<<<<< SEARCH\n{}\n=======\n{}\n>>>>>>> REPLACE\n```",
            filename,
            src.trim_end_matches("\n"),
            dst.trim_end_matches("\n")
        )
    }

    let filename = "gbnf_to_lark.py";
    let repl = repl_block(filename, "foo\nbar", "qux");

    lark_str_test_many(
        grm,
        &[
            &repl,
            &format!("{repl}\n"),
            &format!("{repl}\n\n"),
            &format!("Some text\n{repl}"),
            &format!("Some text\nMore\n{repl}"),
            &format!("Some text\nMore\n{repl}\n"),
            &format!("Some text\nMore\n{repl}\nAnd then some"),
            &format!("Some text\nMore\n{repl}\nAnd then some\n"),
        ],
        &[
            "FINAL_REJECT:Some text\nSome more\n",
            "FINAL_REJECT:Some text\nSome more",
            "Foo\n```\nbar",
            "FINAL_REJECT:Foo\n```\ngbnf",
            &repl_block(filename, "fooz\nbar", "quux"),
        ],
    );
}

#[test]
fn test_json_dw_pattern() {
    lark_str_test_many(
        r#"
            start: %json { "type": "string", "pattern": "^\\d$" }
        "#,
        &["\"1\"", "\"2\""],
        &[
            "1", "\"12\"", "\"a\"", "\"\"",  // simple stuff
            "\"১\"", // unicode should not match
        ],
    );

    lark_str_test_many(
        r#"
            start: %json { "type": "string", "pattern": "^\\w$" }
        "#,
        &["\"1\"", "\"2\"", "\"a\"", "\"A\""],
        &[
            "1", "\"12\"", "\"\"", // simple stuff
            "\"১\"", "\"ł\"", // unicode should not match
        ],
    );

    lark_str_test_many(
        r#"
            start: %json { "type": "string", "pattern": "^\\s$" }
        "#,
        &[
            // regular escapes:
            "\" \"",
            "\"\\t\"",
            "\"\\n\"",
            "\"\\r\"",
            // unicode escapes:
            "\"\\u0009\"",
            "\"\\u000A\"",
            "\"\\u000B\"",
            "\"\\u000C\"",
            // unicode whitespace:
            "\"\u{00A0}\"",
            "\"\u{2000}\"",
            "\"\u{2008}\"",
        ],
        &[
            // simple stuff
            "1",
            "\"12\"",
            "\"\"",
            // non-whitespace unicode
            "\"১\"",
            "\"ł\"",
            // we do not allow \uXXXX outside of \u0000-0x001F
            "\"\\u00A0\"",
        ],
    );
}

#[test]
fn test_json_anchoring() {
    lark_str_test_many(
        r#"
            start: %json { "type": "string", "pattern": "[ab]" }
        "#,
        &["\"a\"", "\"foobar\"", "\"\\nb\\n\\n\""],
        &["1", "\"12\"", "\"\"", "\"১\"", "\"ł\""],
    );

    lark_str_test_many(
        r#"
            start: %json { "type": "string", "pattern": "^foo" }
        "#,
        &["\"foo\"", "\"foobar\""],
        &["1", "\"afoo\"", "\"afooa\""],
    );
    lark_str_test_many(
        r#"
            start: %json { "type": "string", "pattern": "foo$" }
        "#,
        &["\"foo\"", "\"barfoo\""],
        &["1", "\"fooa\"", "\"afooa\""],
    );
    lark_str_test_many(
        r#"
            start: %json { "type": "string", "pattern": "^foo$" }
        "#,
        &["\"foo\""],
        &["1", "\"fooa\"", "\"afoo\"", "\"afooa\""],
    );
}

#[test]
fn test_nested_lark() {
    lark_str_test_many(
        r#"
            start: /[ab]+/ foobar
            foobar: %lark {
                start: "foo" | "Bar"
            }
        "#,
        &["afoo", "abfoo", "aaaaaaBar"],
        &["FINAL_REJECT:a", "afooa"],
    );
}

#[test]
fn test_large_real_substring() {
    let data = include_str!("data/ulysses.md");
    // 240k is the limit for 1M fuel
    let data = data[..200_000].to_string();
    let grm = format!(
        r#"
            start: %regex {{ "substring_words": {} }}
        "#,
        quote_str(&data)
    );
    let mtch = data.split_inclusive(' ').collect::<Vec<_>>()[50..250]
        .to_vec()
        .join("");
    let no_mtch = format!("{}{}", mtch, "XXX");
    lark_str_test_many_quiet(&grm, &[&mtch], &[&no_mtch]);
}

#[test]
fn test_json_pattern_properties() {
    json_err_test(
        &json!({
            "type": "object",
            "patternProperties": {
                "^fo": { "type": "integer" },
                "^foo": { "type": "number" },
            },
        }),
        "are not disjoint",
    );

    json_err_test(
        &json!({
            "type": "object",
            "patternProperties": {
                "foo": { "type": "integer" },
                "bar": { "type": "number" },
            },
        }),
        "are not disjoint",
    );

    json_err_test(
        &json!({
            "allOf": [
                {
                    "type": "object",
                    "patternProperties": {
                        "^fo": { "type": "integer" },
                    },
                },
                {
                    "type": "object",
                    "patternProperties": {
                        "^foo": { "type": "number" },
                    },
                },
            ],
        }),
        "are not disjoint",
    );

    json_err_test(
        &json!({
            "allOf": [
                {
                    "type": "object",
                    "properties": {
                        "foo": { "type": "string" },
                    },
                    "required": ["foo"],
                },
                {
                    "type": "object",
                    "patternProperties": {
                        "^f": { "type": "number" },
                    },
                },
            ],
        }),
        "required property 'foo' is unsatisfiable",
    );

    // "foo" matches patternProperties "^foo" (type: integer), but properties says
    // type: string. Per JSON Schema spec, both must be satisfied — string ∩ integer
    // is unsatisfiable. Since "foo" is optional, objects without "foo" are still valid.
    json_test_many(
        &json!({
            "type": "object",
            "properties": {
                "foo": { "type": "string" },
            },
            "patternProperties": {
                "^foo": { "type": "integer" },
                "^bar": { "type": "array" },
            },
            "additionalProperties": {
                "type": "boolean",
            },
        }),
        &[
            json!({}),
            json!({
                "foo1": 123,
                "bar": [],
                "qux": true,
                "foo2": 456,
                "bar1": [],
                "mux": false,
            }),
            json!({
                "bar": []
            }),
            json!({
                "muxxx": false
            }),
        ],
        &[
            json!({
                "foo": "bar"
            }),
            json!({
                "foo": 123
            }),
            json!({
                "foo1": "blah"
            }),
            json!({
                "foo1": true
            }),
            json!({
                "bar11": true
            }),
        ],
    );

    // "count" matches both properties (integer, minimum: 0) and patternProperties
    // "^c" (integer, multipleOf: 5). The intersection produces non-negative multiples of 5.
    json_test_many(
        &json!({
            "type": "object",
            "properties": {
                "count": { "type": "integer", "minimum": 0 },
            },
            "patternProperties": {
                "^c": { "type": "integer", "multipleOf": 5 },
            },
            "required": ["count"],
        }),
        &[
            json!({"count": 0}),
            json!({"count": 10}),
            json!({"count": 25}),
        ],
        &[
            json!({"count": 3}),
            json!({"count": 7}),
            json!({"count": -5}),
        ],
    );

    // "foo" is required but matches both properties (string) and patternProperties
    // "^foo" (integer) — the intersection is unsatisfiable, so the schema errors.
    json_err_test(
        &json!({
            "type": "object",
            "properties": {
                "foo": { "type": "string" },
            },
            "patternProperties": {
                "^foo": { "type": "integer" },
                "^bar": { "type": "array" },
            },
            "additionalProperties": {
                "type": "boolean",
            },
            "required": ["foo", "mux", "foo1", "bar1"],
        }),
        "required property 'foo' is unsatisfiable",
    );

    // "name" doesn't match any pattern, so properties + patternProperties +
    // additionalProperties all coexist. Tests required property ordering and
    // type enforcement across all three keyword types.
    json_test_many(
        &json!({
            "type": "object",
            "properties": {
                "name": { "type": "string" },
            },
            "patternProperties": {
                "^foo": { "type": "integer" },
                "^bar": { "type": "array" },
            },
            "additionalProperties": {
                "type": "boolean",
            },
            "required": ["name", "mux", "foo1", "bar1"],
        }),
        &[
            json!({
                "name": "hello",
                "mux": false,
                "foo1": 123,
                "bar1": [],
            }),
            json!({
                "name": "hello",
                "mux": false,
                "foo1": 123,
                "bar1": [],
                "blah": true
            }),
        ],
        &[
            // wrong order
            json!({
                "name": "hello",
                "mux": false,
                "bar1": [],
                "foo1": 123,
            }),
            // mux wrong type (must be boolean via additionalProperties)
            json!({
                "name": "hello",
                "mux": "blah",
                "foo1": 123,
                "bar1": [],
            }),
            // foo1 wrong type (must be integer via ^foo pattern)
            json!({
                "name": "hello",
                "mux": false,
                "foo1": "aaa",
                "bar1": [],
            }),
        ],
    );

    // allOf: one schema has additionalProperties: false (no properties/patterns),
    // the other has patternProperties. Since the first schema rejects all properties,
    // the pattern becomes unsatisfiable for any matching property.
    json_test_many(
        &json!({
            "allOf": [
                { "additionalProperties": false },
                { "patternProperties": { "^f": { "type": "integer" } } },
            ],
        }),
        &[json!({})],
        &[
            json!({"foo": 42}),
            json!({"bar": 1}),
            json!({"foo": 42, "bar": 1}),
        ],
    );

    // allOf: one schema allows only "foo" (additionalProperties: false), the other
    // has patternProperties "^f". Only "foo" survives (it's a named property in the
    // first schema), while other "^f" matches like "fxx" are rejected.
    json_test_many(
        &json!({
            "allOf": [
                {
                    "properties": { "foo": { "type": "string" } },
                    "additionalProperties": false,
                },
                {
                    "patternProperties": { "^f": { "type": "string" } },
                },
            ],
        }),
        &[json!({}), json!({"foo": "bar"})],
        &[json!({"foo": 42}), json!({"fxx": "bar"})],
    );

    // allOf: both schemas have the same pattern key "^f". The pattern schemas
    // are intersected: integer ∩ multipleOf(3) = integers that are multiples of 3.
    json_test_many(
        &json!({
            "allOf": [
                { "patternProperties": { "^f": { "type": "integer", "minimum": 0 } } },
                { "patternProperties": { "^f": { "type": "integer", "multipleOf": 3 } } },
            ],
        }),
        &[
            json!({}),
            json!({"foo": 0}),
            json!({"fox": 9}),
            json!({"foo": 3, "fox": 6}),
        ],
        &[
            json!({"foo": 1}),
            json!({"foo": -3}),
            json!({"foo": 3, "fox": 5}),
        ],
    );

    // allOf: different pattern keys from each schema. Both patterns must
    // coexist (and be disjoint).
    json_test_many(
        &json!({
            "allOf": [
                { "patternProperties": { "^f": { "type": "integer" } } },
                { "patternProperties": { "^b": { "type": "string" } } },
            ],
        }),
        &[
            json!({}),
            json!({"foo": 42}),
            json!({"bar": "hello"}),
            json!({"foo": 1, "bar": "x"}),
        ],
        &[json!({"foo": "nope"}), json!({"bar": 42})],
    );
}

#[test]
fn test_json_min_max_properties() {
    json_err_test(
        &json!({
            "type": "object",
            "properties": {
                "foo": { "type": "string" },
            },
            "maxProperties": 7
        }),
        "min/maxProperties only supported when all keys listed in \"properties\" are required",
    );

    json_err_test(
        &json!({
            "type": "object",
            "properties": {
                "foo": { "type": "string" },
            },
            "minProperties": 1
        }),
        "min/maxProperties only supported when all keys listed in \"properties\" are required",
    );

    json_err_test(
        &json!({
            "type": "object",
            "minProperties": 7,
            "maxProperties": 1,
        }),
        "minProperties > maxProperties",
    );

    json_err_test(
        &json!({
            "type": "object",
            "required": ["foo", "bar"],
            "maxProperties": 1,
        }),
        "required > maxProperties",
    );

    json_test_many(
        &json!({
            "type": "object",
            "properties": {
                "foo": { "type": "string" },
                "bar": { "type": "array" },
            },
            "additionalProperties": {
                "type": "integer",
            },
            "required": ["foo", "mux"],
            // doesn't actually do anything, since at least 2 are required
            "minProperties": 2,
        }),
        &[
            json!({
                "foo": "bar",
                "mux": 7,
            }),
            json!({
                "foo": "bar",
                "mux": 7,
                "mux2": 7,
            }),
        ],
        &[json!({
            "foo": "bar",
        })],
    );

    json_test_many(
        &json!({
            "type": "object",
            "minProperties": 1,
        }),
        &[
            json!({
                "foo": "bar",
            }),
            json!({
                "foo": "bar",
                "mux": 7,
            }),
        ],
        &[json!({})],
    );

    json_test_many(
        &json!( {
            "type": "object",
            "description": "Output file",
            "patternProperties": {
              "^.*$": {
                "type": "object"
              }
            },
            "additionalProperties": false,
            "maxProperties": 1
        }),
        &[
            json!({}),
            json!({
                "foo": {},
            }),
            json!({
                "metric.txt": {"bar": 42},
            }),
        ],
        &[
            json!({
                "foo": 7,
            }),
            json!({
                "foo": {},
                "bar": {},
            }),
        ],
    );

    json_test_many(
        &json!({
            "type": "object",
            "properties": {
                "foo": { "type": "string" },
            },
            "additionalProperties": {
                "type": "integer",
            },
            "required": ["foo", "mux"],
            "minProperties": 3,
            "maxProperties": 5,
        }),
        &[
            json!({
                "foo": "bar",
                "mux": 7,
                "mux2": 7,
            }),
            json!({
                "foo": "bar",
                "mux": 7,
                "mux2": 7,
                "mux3": 7,
                "mux4": 7,
            }),
        ],
        &[
            json!({
                "foo": "bar",
            }),
            json!({
                "foo": "bar",
                "mux": 7,
            }),
            json!({
                "foo": "bar",
                "mux": 7,
                "mux2": 7,
                "mux3": 7,
                "mux4": true,
            }),
            json!({
                "foo": "bar",
                "mux": 7,
                "mux2": 7,
                "mux3": 7,
                "mux4": 7,
                "mux5": 7,
            }),
        ],
    );

    json_test_many(
        &json!({
            "type": "object",
            "properties": {
                "foo": { "type": "string" },
                "bar": { "type": "array" },
                "mux": { "type": "integer" },
            },
            "minProperties": 1,
            "additionalProperties": false
        }),
        &[
            json!({
                "foo": "bar",
            }),
            json!({
                "bar": [],
            }),
            json!({
                "foo": "bar",
                "mux": 7,
            }),
        ],
        &[json!({}), json!({ "foo": 7 })],
    );

    json_test_many(
        &json!({
            "type": "object",
            "properties": {
                "foo": { "type": "string" },
                "bar": { "type": "array" },
                "mux": { "type": "integer" },
            },
            "minProperties": 1,
            "maxProperties": 1,
            "additionalProperties": false
        }),
        &[json!({ "foo": "bar" }), json!({ "bar": [] })],
        &[
            json!({}),
            json!({ "foo": 7 }),
            json!({
                "foo": "bar",
                "mux": 7,
            }),
        ],
    );

    json_test_many(
        &json!({
            "type": "object",
            "properties": {
                "foo": { "type": "string" },
                "bar": { "type": "array" },
                "mux": { "type": "integer" },
            },
            "maxProperties": 1,
            "additionalProperties": false
        }),
        &[json!({}), json!({ "foo": "bar" }), json!({ "bar": [] })],
        &[
            json!({ "foo": 7 }),
            json!({
                "foo": "bar",
                "mux": 7,
            }),
        ],
    );

    json_test_many(
        &json!({
            "type": "object",
            "properties": {
                "foo": { "type": "string" },
                "bar": { "type": "array" },
                "mux": { "type": "integer" },
            },
            "required": ["bar"],
            "minProperties": 2, // we subtract required properties
            "additionalProperties": false
        }),
        &[
            json!({ "foo": "a", "bar": [] }),
            json!({ "bar": [], "mux": 1 }),
            json!({ "foo": "a", "bar": [], "mux": 1 }),
        ],
        &[
            json!({ "foo": 7 }),
            json!({ "bar": [] }),
            json!({
                "foo": "bar",
                "mux": 7,
            }),
        ],
    );

    json_test_many(
        &json!({
            "type": "object",
            "properties": {
                "foo": { "type": "string" },
                "bar": { "type": "array" },
                "mux": { "type": "integer" },
            },
            "required": ["bar"],
            "maxProperties": 2, // we subtract required properties
            "additionalProperties": false
        }),
        &[
            json!({ "foo": "a", "bar": [] }),
            json!({ "bar": [], "mux": 1 }),
            json!({ "bar": [] }),
        ],
        &[
            json!({ "foo": 7 }),
            json!({
                "foo": "bar",
                "mux": 7,
            }),
            json!({ "foo": "a", "bar": [], "mux": 1 }),
        ],
    );
}

#[test]
fn test_json_format_email() {
    json_test_many(
        &json!({
            "type": "string",
            "format": "email",
        }),
        &[
            json!("test@example.com"),
            json!("foo.bar@example.com"),
            json!("foo.bar@example-123.com"),
            json!("foo+bar@example-123.com"),
            json!("f$o#o`b-a!r@example-123.com"),
            json!("fo%o#bar@example-123.com"),
            json!("test@[192.168.1.1]"),
        ],
        &[
            json!(""),
            json!(" @example.com"),
            json!("test@"),
            json!("@example.com"),
            json!("test@.com"),
            //json!("test@com"), // allowed by the regex
            json!("test@com."),
            json!("test@example..com"),
            //json!("test@example.c"), // allowed by the regex
            json!("test@example.c."),
            json!("test@.example.com"),
            json!("test:2@example.com"),
            json!("test[2]@example.com"),
        ],
    );
}

#[test]
fn test_regex_and() {
    lark_str_test_many(
        r#"
            start: ST
            ST: LOWER & ABC
            LOWER: /[a-z]+/
            ABC: /[abcABC]+/
        "#,
        &["abc", "a", "bbb"],
        &["A", "d", "1"],
    );

    lark_err_test(
        r#"
            start: LOWER & ABC
            LOWER: /[a-z]+/
            ABC: /[abcABC]+/
        "#,
        "& is only supported for tokens, not rules",
    );

    lark_str_test_many(
        r#"
            start: ST
            ST: /[a-z]+/ & /[abcABC]+/
        "#,
        &["abc", "a", "bbb"],
        &["A", "d", "1"],
    );

    lark_str_test_many(
        r#"
            start: ST
            ST: /[ab]+/ | /[cd]+/ & /[def]+/
        "#,
        &["ab", "dd"],
        &["c", "e", "f"],
    );

    lark_str_test_many(
        r#"
            start: ST
            ST: /[ab]+/ | ( /[cd]+/ & /[def]+/ )
        "#,
        &["ab", "dd"],
        &["c", "e", "f"],
    );

    lark_str_test_many(
        r#"
            start: ST
            ST: ( /[abf]+/ | /[cd]+/ ) & /[def]+/
        "#,
        &["d", "f"],
        &["a", "b"],
    );

    lark_str_test_many(
        r#"
            start: ST
            ST: /[ab]/+ | /[cd]/+ & /[def]/+
        "#,
        &["ab", "dd"],
        &["c", "e", "f"],
    );

    lark_str_test_many(
        r#"
            start: ST
            ST: /[abc]/ & /[bcd]/ & /[cde]/
        "#,
        &["c"],
        &["a", "b", "d", "e"],
    );

    lark_str_test_many(
        r#"
            start: ST
            ST: /a*/ & /a+/
        "#,
        &["a", "aa"],
        &["FINAL_REJECT:", "b"],
    );

    lark_str_test_many(
        r#"
            start: "foo" | ST
            ST: /a/? & /a{2}/
        "#,
        &["foo"],
        &["FINAL_REJECT:", "a", "aa"],
    );

    lark_str_test_many(
        r#"
            start: "foo" | ST
            ST: /a/ /b/ & /b/
        "#,
        &["foo"],
        &["b", "a", "abb", "ab"],
    );

    lark_str_test_many(
        r#"
            start: "foo" | ST
            ST: /ab/ & /b/
        "#,
        &["foo"],
        &["ab", "b", "a"],
    );
}

#[test]
fn test_regex_not() {
    lark_str_test_many(
        r#"
            start: ST
            ST: /[abcd]+/ & ~/(.*)[ab](.*)/
        "#,
        &["cd", "c"],
        &["x", "aa", "ca", "b"],
    );

    lark_str_test_many(
        r#"
            start: ASCII_LINES
            ASCII_LINES: /[a-zA-Z \n]*/ & ~/(?s:.*)\n\n(?s:.*)/
        "#,
        &["foo", "bar\nbaz", "hello world\n", "aaa\nbbb\nccc"],
        &[".", "a\n\na"],
    );
}

#[test]
fn test_parametric_0() {
    lark_str_test_many(
        r#"
            start    :  perm::0x0
            perm::_  :  "X"                     %if is_ones([0:3])
                     |  a0 perm::set_bit(0)     %if bit_clear(0)
                     |  a1 perm::set_bit(1)     %if bit_clear(1)
                     |  a2 perm::set_bit(2)     %if bit_clear(2)
            a0: "a"
            a1: "b"
            a2: "c"
        "#,
        &["abcX", "bcaX", "cbaX", "cabX", "acbX", "bacX"],
        &["z", "X", "aX", "abX", "abb", "aa", "bb", "cc"],
    );

    lark_str_test_many(
        r#"
            start    :  perm::0x0 "Y"
            perm::_  :  "X"                     %if is_ones([0:3])
                     |  a0 perm::set_bit(0)     %if bit_clear(0)
                     |  a1 perm::set_bit(1)     %if bit_clear(1)
                     |  a2 perm::set_bit(2)     %if bit_clear(2)
            a0: "a"
            a1: "b"
            a2: "c"
        "#,
        &["abcXY", "bcaXY", "cbaXY", "cabXY", "acbXY", "bacXY"],
        &["z", "X", "aX", "abX", "abb", "aa", "bb", "cc"],
    );
}

#[test]
fn test_parametric_1() {
    lark_str_test_many(
        r#"
            start  : aa::0 "X"
            aa::_  : a aa::incr(_)    %if lt(_, 6)
                   | bb::_
            bb::_  : b bb::incr(_)    %if lt(_, 6)
                   | ""
            a: "a"
            b: "b"
        "#,
        &[
            "X", "aX", "bX", "abX", "aabX", "aaabX", "aaaabbX", "aaabbbX", "aaaaaaX", "bbbbbbX",
        ],
        &["z", "ba", "aaaaaaa", "bbbbbbb", "aaabbbbb"],
    );
}

#[test]
fn test_parametric_cnt() {
    // at most 3 a, 3 b, 2 c (2 bits each)
    lark_str_test_many(
        r#"
            start  : lst::0x0
            lst::_ : "a" lst::incr([0:2])  %if lt([0:2], 3)
                   | "b" lst::incr([2:4])  %if lt([2:4], 3)
                   | "c" lst::incr([4:6])  %if lt([4:6], 2)
                   | "X"

        "#,
        &[
            "X",
            "aX",
            "bX",
            "abX",
            "aabX",
            "aaabX",
            "bbbaaaX",
            "ccaaabbbX",
            "abcababcX",
        ],
        &["z", "aaaa", "bbbb", "ccc", "aaaccc", "aaabbbb"],
    );
}

#[test]
fn test_parametric_pick_3() {
    // allow for at last 1 and at most 3 unique elements
    lark_str_test_many(
        r#"
            start    :  perm::0x0
            perm::_  :  "X"                      %if bit_count_ge(_, 1)
                     |  "a" perm::set_bit(0)     %if and(bit_clear(0), bit_count_lt(_, 3))
                     |  "b" perm::set_bit(1)     %if and(bit_clear(1), bit_count_lt(_, 3))
                     |  "c" perm::set_bit(2)     %if and(bit_clear(2), bit_count_lt(_, 3))
                     |  "d" perm::set_bit(3)     %if and(bit_clear(3), bit_count_lt(_, 3))
                     |  "e" perm::set_bit(4)     %if and(bit_clear(4), bit_count_lt(_, 3))

        "#,
        &["aX", "bX", "bacX", "adeX"],
        &["X", "z", "aa", "bb", "abcd", "aba"],
    );
}

#[test]
fn test_parametric_null() {
    let matching = &["abcX", "bcaX", "cbaX", "cabX", "acbX", "bacX"];
    let not_matching = &[
        "z", "X", "aX", "bX", "cX", "abX", "acX", "cbX", "abb", "aa", "bb", "cc",
    ];

    lark_str_test_many(
        r#"
            start    :  perm::0x0 "X"
            perm::_  :  ""                       %if is_ones([0:3])
                     |  "a" perm::set_bit(0)     %if bit_clear(0)
                     |  "b" perm::set_bit(1)     %if bit_clear(1)
                     |  "c" perm::set_bit(2)     %if bit_clear(2)
        "#,
        matching,
        not_matching,
    );

    // complicate a bit with nested empty rules
    lark_str_test_many(
        r#"
            start    :  perm::0x0 "X"
            perm::_  :  ae::_ be::_              %if is_zeros([10:12])
                     |  "a" perm::set_bit(0)     %if bit_clear(0)
                     |  "b" perm::set_bit(1)     %if bit_clear(1)
                     |  "c" perm::set_bit(2)     %if bit_clear(2)
            ae::_ : ""    %if is_ones([0:1])
            be::_ : ce::_ %if is_ones([1:2])
            ce::_ : ""    %if is_ones([2:3])
        "#,
        matching,
        not_matching,
    );

    lark_str_test_many(
        r#"
            start    :  perm::0x0 "X"
            perm::_  :  ae::_ be::_
                     |  "a" perm::set_bit(0)     %if bit_clear(0)
                     |  "b" perm::set_bit(1)     %if bit_clear(1)
                     |  "c" perm::set_bit(2)     %if bit_clear(2)
            ae::_ : ""    %if is_ones([0:1])
            be::_ : ce::_ %if is_ones([1:2])
            ce::_ : ""    %if is_ones([2:3])
        "#,
        matching,
        not_matching,
    );
}

#[test]
fn test_parametric_syntax() {
    // Missing underscore after '::' in rule header
    lark_err_test(
        r#"
            start: foo
            foo:: "a"
        "#,
        "Expected token '_'",
    );

    // Invoking non-parametric rule with a parameter
    lark_err_test(
        r#"
            start: foo
            foo: a::1
            a: "a"
        "#,
        "rule 'a' is not parametric",
    );

    // Bit-range syntax errors in '%if' conditions
    lark_err_test(
        r#"
            start: foo
            foo::_: "a" %if eq([3:3], 0)
        "#,
        "end bit index 3 must be > start bit index 3",
    );
    lark_err_test(
        r#"
            start: foo
            foo::_: "a" %if eq([64:65], 0)
        "#,
        "number 64 is too large; must be <= 63",
    );
    lark_err_test(
        r#"
            start: foo
            foo::_: "a" %if eq([0:65], 0)
        "#,
        "number 65 is too large; must be <= 64",
    );

    // '%if' not allowed in terminal definitions
    lark_err_test(
        r#"
            BAR: "b" %if and(true,true)
            start: bar
            bar: BAR
        "#,
        "'%if' is not supported in terminals",
    );

    // 'name::param' invocation not allowed in terminals
    lark_err_test(
        r#"
            BAZ: qux::0
            start: baz
            baz: BAZ
        "#,
        "name::param cannot be used in terminals",
    );

    // 2. Parametric rule with non-parametric body
    lark_err_test(
        r#"
            start: foo
            foo::_: bar
            bar: "a"
        "#,
        "rule \"foo\" is parametric, but its body doesn't need parameters",
    );

    // 3. Non-parametric rule with parametric body
    lark_err_test(
        r#"
            start: foo
            foo: bar::_
            bar::_: bar::_
        "#,
        "rule \"foo\" is not parametric, but its body requires parameters",
    );

    lark_err_test(
        r#"
            start: foo
            foo: bar
            bar::_: bar::_
        "#,
        "rule 'bar' is parametric, but no parameter provided",
    );

    // 4. stop= on a parametric rule
    lark_err_test(
        r#"
            start: foo
            foo::_[stop="X"]: "a"
        "#,
        "stop-like is not supported for parametric rules",
    );

    // 5. temperature= on a parametric rule
    lark_err_test(
        r#"
            start: foo
            foo::_[temperature=1.0]: "a"
        "#,
        "temperature= is not supported for parametric rules",
    );

    // 6. max_tokens= on a parametric rule
    lark_err_test(
        r#"
            start: foo
            foo::_[max_tokens=10]: "a"
        "#,
        "max_tokens= is not supported for parametric rules",
    );

    // 8. name::param not allowed in %token
    lark_err_test(
        r#"
            BAZ: qux::0
            start: BAZ
            qux::_: "baz"
        "#,
        "name::param cannot be used in terminals",
    );

    // 11. bracket syntax required for ParamRef
    lark_err_test(
        r#"
            start: foo
            foo::_: "a" %if eq(0, 0)
        "#,
        "expected '_' or '[start_bit:stop_bit]'",
    );

    // 12. unknown '%if' condition
    lark_err_test(
        r#"
            start: foo
            foo::_: "a" %if foo([0:1], 0)
        "#,
        "Unexpected condition 'foo'",
    );
}

#[test]
fn test_parametric_long() {
    // this is designed for the default max_items_in_row of 2000

    let mut s = String::new();

    let n = if cfg!(debug_assertions) { 90 } else { 900 };
    let m = if cfg!(debug_assertions) { 50 } else { 200 };
    let n2 = n * 2;

    for _ in 0..n {
        s.push_str("a ");
    }
    for _ in 0..n {
        s.push_str("b ");
    }
    let mut s2 = s.clone();
    s2.push_str("b ");
    s2.push_str("b ");
    s.push('X');

    lark_str_test_many(
        &format!(
            r#"
                start  : aa::0 "X"
                aa::_  : "a " aa::incr(_)    %if lt(_, {n2})
                       | bb::_
                bb::_  : "b " bb::incr(_)    %if lt(_, {n2})
                       | ""
            "#
        ),
        &[&s],
        &[&s2],
    );

    s.clear();
    for _ in 0..m {
        s.push_str("a b c d e f g ");
    }
    let mut s2 = s.clone();
    s2.push_str("a ");
    s.push('X');

    lark_str_test_many(
        &format!(
            r#"
                start  : lst::0x0
                lst::_ : "a " lst::incr([0:8])  %if lt([0:8], {m})
                       | "b " lst::incr([8:16])  %if lt([8:16], {m})
                       | "c " lst::incr([16:24])  %if lt([16:24], {m})
                       | "d " lst::incr([24:32])  %if lt([24:32], {m})
                       | "e " lst::incr([32:40])  %if lt([32:40], {m})
                       | "f " lst::incr([40:48])  %if lt([40:48], {m})
                       | "g " lst::incr([48:56])  %if lt([48:56], {m})
                       | "X"
            "#
        ),
        &[&s],
        &[&s2],
    );
}
