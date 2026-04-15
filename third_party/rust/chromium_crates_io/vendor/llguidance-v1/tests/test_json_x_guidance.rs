use llg_test_utils::lark_str_test;
use rstest::rstest;
use serde_json::json;
use serde_json_fmt::JsonFormat;

#[rstest]
// Ok
#[case::with_spaces(r#"{"a": 1, "b": 2}"#, true)]
// Bad
#[case::no_spaces(r#"{"a":1,"b":2}"#, false)]
#[case::two_spaces_around_colon(r#"{"a"  :  1 , "b":2}"#, false)]
#[case::spaces_before_comma(r#"{"a":1 ,  "b":2}"#, false)]
#[case::two_spaces_after_colon(r#"{"a":1,"b":  2}"#, false)]
#[case::three_spaces_after_comma(r#"{"a":1,   "b":2}"#, false)]
#[case::four_spaces_after_colon(r#"{"a":1,"b":    2}"#, false)]
fn test_simple_separators(#[case] input: &str, #[case] should_succeed: bool) {
    let options = json!({
        "item_separator": ", ",
        "key_separator": ": ",
        "whitespace_flexible": false,
    });
    let lark = format!(
        r#"
        start: %json {{
            "x-guidance": {options}
        }}
    "#
    );
    lark_str_test(&lark, should_succeed, input, true);
}

#[rstest]
// Ok
#[case::with_spaces(r#"{"a": 1, "b": 2}"#, true)]
// Bad
#[case::no_spaces(r#"{"a":1,"b":2}"#, false)]
#[case::two_spaces_around_colon(r#"{"a"  :  1 , "b":2}"#, false)]
#[case::spaces_before_comma(r#"{"a":1 ,  "b":2}"#, false)]
#[case::two_spaces_after_colon(r#"{"a":1,"b":  2}"#, false)]
#[case::three_spaces_after_comma(r#"{"a":1,   "b":2}"#, false)]
#[case::four_spaces_after_colon(r#"{"a":1,"b":    2}"#, false)]
fn test_simple_separators_with_object_schema(#[case] input: &str, #[case] should_succeed: bool) {
    let object_schema = json!({
        "type": "object",
        "properties": {
            "a": { "type": "integer" },
            "b": { "type": "integer" }
        },
        "required": ["a", "b"],
        "additionalProperties": false,
        "x-guidance": {
            "item_separator": r", ",
            "key_separator": r": ",
            "whitespace_flexible": false,
        }
    });
    let lark = format!(
        r#"
        start: %json {object_schema}
    "#
    );
    lark_str_test(&lark, should_succeed, input, true);
}

#[rstest]
#[case::regular_json(r#"{"a": 1, "b": 2}"#, false)]
#[case::alternate_json(r#"{"a"_ 1-"b"_ 2}"#, true)]
#[case::alternate_json(r#"{"a"_ 1-"b"_2}"#, false)]
fn test_alternate_separators(#[case] input: &str, #[case] should_succeed: bool) {
    let options = json!({
        "item_separator": r"-",
        "key_separator": r"_ ",
        "whitespace_flexible": false,
    });
    let lark = format!(
        r#"
        start: %json {{
            "x-guidance": {options}
        }}
    "#
    );
    lark_str_test(&lark, should_succeed, input, true);
}

#[rstest]
#[case::regular_json(r#"{"a": 1, "b": 2}"#, false)]
#[case::alternate_json(r#"{"a"_ 1-"b"_ 2}"#, true)]
#[case::alternate_json(r#"{"a"_ 1-"b"_2}"#, false)]
fn test_alternate_separators_with_object_schema(#[case] input: &str, #[case] should_succeed: bool) {
    let object_schema = json!({
        "type": "object",
        "properties": {
            "a": { "type": "integer" },
            "b": { "type": "integer" }
        },
        "required": ["a", "b"],
        "additionalProperties": false,
        "x-guidance": {
            "item_separator": r"-",
            "key_separator": r"_ ",
            "whitespace_flexible": true,
        }
    });
    let lark = format!(
        r#"
        start: %json {object_schema}
    "#
    );
    lark_str_test(&lark, should_succeed, input, true);
}

#[rstest]
// Ok
#[case::with_spaces(r#"{"a": 1, "b": 2}"#, true)]
#[case::no_spaces(r#"{"a":1,"b":2}"#, true)]
#[case::two_spaces_around_colon(r#"{"a"  :  1 , "b":2}"#, true)]
#[case::spaces_before_comma(r#"{"a":1 ,  "b":2}"#, true)]
#[case::two_spaces_after_colon(r#"{"a":1,"b":  2}"#, true)]
// Bad
#[case::three_spaces_after_comma(r#"{"a":1,   "b":2}"#, false)]
#[case::four_spaces_after_colon(r#"{"a":1,"b":    2}"#, false)]
fn test_pattern_separators(#[case] input: &str, #[case] should_succeed: bool) {
    let options = json!({
        "item_separator": r"\s{0,2},\s{0,2}",
        "key_separator": r"\s{0,2}:\s{0,2}",
        "whitespace_flexible": false,
    });
    let lark = format!(
        r#"
        start: %json {{
            "x-guidance": {options}
        }}
    "#
    );
    lark_str_test(&lark, should_succeed, input, true);
}

#[rstest]
// Ok
#[case::with_spaces(r#"{"a": 1, "b": 2}"#, true)]
#[case::no_spaces(r#"{"a":1,"b":2}"#, true)]
#[case::two_spaces_around_colon(r#"{"a"  :  1 , "b":2}"#, true)]
#[case::spaces_before_comma(r#"{"a":1 ,  "b":2}"#, true)]
#[case::two_spaces_after_colon(r#"{"a":1,"b":  2}"#, true)]
#[case::three_spaces_after_comma(r#"{"a":1,   "b":2}"#, true)]
#[case::four_spaces_after_colon(r#"{"a":1,"b":    2}"#, true)]
#[case::multi_line("{\n\"a\"\n:\n1,\n\"b\"\n:\n2\n}", true)]
#[case::pretty_print("{\n  \"a\": 1,\n  \"b\": 2\n}", true)]
#[case::pretty_print_extra_spaces("{\n  \"a\" : 1 , \n  \"b\" : 2\n}", true)]
fn test_flexible_separators(#[case] input: &str, #[case] should_succeed: bool) {
    let options = json!({
        "item_separator": r",",
        "key_separator": r":",
        "whitespace_flexible": true,
    });
    let lark = format!(
        r#"
        start: %json {{
            "x-guidance": {options}
        }}
    "#
    );
    lark_str_test(&lark, should_succeed, input, true);
}

#[rstest]
// Ok
#[case::with_spaces(r#"{"a": 1, "b": 2}"#, true)]
#[case::no_spaces(r#"{"a":1,"b":2}"#, true)]
#[case::two_spaces_around_colon(r#"{"a"  :  1 , "b":2}"#, true)]
#[case::spaces_before_comma(r#"{"a":1 ,  "b":2}"#, true)]
#[case::two_spaces_after_colon(r#"{"a":1,"b":  2}"#, true)]
#[case::three_spaces_after_comma(r#"{"a":1,   "b":2}"#, true)]
#[case::four_spaces_after_colon(r#"{"a":1,"b":    2}"#, true)]
#[case::multi_line("{\n\"a\"\n:\n1,\n\"b\"\n:\n2\n}", true)]
#[case::pretty_print("{\n  \"a\": 1,\n  \"b\": 2\n}", true)]
#[case::pretty_print_extra_spaces("{\n  \"a\" : 1 , \n  \"b\" : 2\n}", true)]
fn test_flexible_separators_with_object_schema(#[case] input: &str, #[case] should_succeed: bool) {
    let object_schema = json!({
        "type": "object",
        "properties": {
            "a": { "type": "integer" },
            "b": { "type": "integer" }
        },
        "required": ["a", "b"],
        "additionalProperties": false,
        "x-guidance": {
            "item_separator": r",",
            "key_separator": r":",
            "whitespace_flexible": true,
        }
    });
    let lark = format!(
        r#"
        start: %json 
            {object_schema}
    "#
    );
    lark_str_test(&lark, should_succeed, input, true);
}

#[rstest]
#[case::with_spaces(r#"{"a": 1, "b": 2}"#, true)]
#[case::no_spaces(r#"{"a":1,"b":2}"#, false)]
#[case::two_spaces_around_colon(r#"{"a"  :  1 , "b":2}"#, false)]
#[case::two_spaces_around_both_colons(r#"{"a"  :  1 , "b"  :  2}"#, true)]
#[case::spaces_before_comma(r#"{"a":1 ,  "b":2}"#, false)]
#[case::spaces_before_comma_2(r#"{"a": 1 , "b": 2}"#, true)]
#[case::two_spaces_after_colon(r#"{"a":1,"b":  2}"#, false)]
#[case::three_spaces_after_comma(r#"{"a":1,   "b":2}"#, false)]
#[case::four_spaces_after_colon(r#"{"a":1,"b":    2}"#, false)]
#[case::multi_line("{\n\"a\"\n:\n1,\n\"b\"\n:\n2\n}", false)] // Note newline after separator, not space
#[case::multi_line_2("{\n\"a\"\n: \n1, \n\"b\"\n: \n2\n}", true)]
#[case::pretty_print("{\n  \"a\":\n 1,\n  \"b\":\n 2\n}", false)] // Note newline after separators, not space
#[case::pretty_print_2("{\n  \"a\": 1,\n  \"b\": 2\n}", false)] // Note newline after comma, not space
#[case::pretty_print_3("{\n  \"a\": 1, \n  \"b\": 2\n}", true)]
#[case::pretty_print_extra_spaces("{\n  \"a\" : 1 , \n  \"b\" : 2\n}", true)]
fn test_flexible_separators_with_spaces(#[case] input: &str, #[case] should_succeed: bool) {
    let options = json!({
        "item_separator": r", ",
        "key_separator": r": ",
        "whitespace_flexible": true,
    });
    let lark = format!(
        r#"
        start: %json {{
            "x-guidance": {options}
        }}
    "#
    );
    lark_str_test(&lark, should_succeed, input, true);
}

#[rstest]
#[case::with_spaces(r#"{"a": 1, "b": 2}"#, true)]
#[case::no_spaces(r#"{"a":1,"b":2}"#, false)]
#[case::two_spaces_around_colon(r#"{"a"  :  1 , "b":2}"#, false)]
#[case::two_spaces_around_both_colons(r#"{"a"  :  1 , "b"  :  2}"#, true)]
#[case::spaces_before_comma(r#"{"a":1 ,  "b":2}"#, false)]
#[case::spaces_before_comma_2(r#"{"a": 1 , "b": 2}"#, true)]
#[case::two_spaces_after_colon(r#"{"a":1,"b":  2}"#, false)]
#[case::three_spaces_after_comma(r#"{"a":1,   "b":2}"#, false)]
#[case::four_spaces_after_colon(r#"{"a":1,"b":    2}"#, false)]
fn test_flexible_separators_with_spaces_with_object_schema(
    #[case] input: &str,
    #[case] should_succeed: bool,
) {
    let object_schema = json!({
        "type": "object",
        "properties": {
            "a": { "type": "integer" },
            "b": { "type": "integer" }
        },
        "required": ["a", "b"],
        "additionalProperties": false,
        "x-guidance": {
            "item_separator": r", ",
            "key_separator": r": ",
            "whitespace_flexible": true,
        }
    });
    let lark = format!(
        r#"
        start: %json {object_schema}
    "#
    );
    lark_str_test(&lark, should_succeed, input, true);
}

#[rstest]
#[case::with_spaces(r#"{"a": 1, "b": 2}"#, true)]
#[case::no_spaces(r#"{"a":1,"b":2}"#, true)]
#[case::two_spaces_around_colon(r#"{"a"  :  1 , "b":2}"#, true)]
#[case::two_spaces_around_both_colons(r#"{"a"  :  1 , "b"  :  2}"#, true)]
#[case::spaces_before_comma(r#"{"a":1 ,  "b":2}"#, true)]
#[case::spaces_before_comma_2(r#"{"a": 1 , "b": 2}"#, true)]
#[case::two_spaces_after_colon(r#"{"a":1,"b":  2}"#, true)]
#[case::three_spaces_after_comma(r#"{"a":1,   "b":2}"#, true)]
#[case::four_spaces_after_colon(r#"{"a":1,"b":    2}"#, true)]
#[case::multi_line("{\n\"a\"\n:\n1,\n\"b\"\n:\n2\n}", true)]
#[case::multi_line_2("{\n\"a\"\n: \n1, \n\"b\"\n: \n2\n}", true)]
#[case::pretty_print("{\n  \"a\":\n 1,\n  \"b\":\n 2\n}", true)]
#[case::pretty_print_2("{\n  \"a\": 1,\n  \"b\": 2\n}", true)]
#[case::pretty_print_3("{\n  \"a\": 1, \n  \"b\": 2\n}", true)]
#[case::pretty_print_extra_spaces("{\n  \"a\" : 1 , \n  \"b\" : 2\n}", true)]
#[case::pretty_print_extra_spaces_2("{\n    \"a\" :  1 ,  \n    \"b\" :  2\n}", true)]
fn flexible_whitespace_unspecified_separators(#[case] input: &str, #[case] should_succeed: bool) {
    let object_schema = json!({
        "type": "object",
        "properties": {
            "a": { "type": "integer" },
            "b": { "type": "integer" }
        },
        "required": ["a", "b"],
        "additionalProperties": false,
        "x-guidance": {
            "whitespace_flexible": true,
        }
    });
    let lark = format!(
        r#"
        start: %json {object_schema}
    "#
    );
    lark_str_test(&lark, should_succeed, input, true);
}

#[rstest]
fn whitespace_flexible_many_formats(
    #[values(Option::None, Option::Some(2), Option::Some(4))] desired_indent: Option<usize>,
    #[values(
        Option::None,
        Option::Some(","),
        Option::Some(", "),
        Option::Some(" ,"),
        Option::Some(" , "),
        Option::Some(" ,\n"),
        Option::Some("\n,\n")
    )]
    comma: Option<&str>,
    #[values(
        Option::None,
        Option::Some(":"),
        Option::Some(": "),
        Option::Some(" :"),
        Option::Some(" : "),
        Option::Some(" :\n"),
        Option::Some("\n:\n")
    )]
    colon: Option<&str>,
) {
    let object_schema = json!({
        "type": "object",
        "properties": {
            "a": { "type": "integer" },
            "b": { "type": "integer" }
        },
        "required": ["a", "b"],
        "additionalProperties": false,
        "x-guidance": {
            "whitespace_flexible": true,
        }
    });
    let target_object = json!({
        "a": 1,
        "b": 2
    });

    let mut formatter = JsonFormat::new().indent_width(desired_indent);
    if let Some(comma_str) = comma {
        formatter = formatter.comma(comma_str).unwrap();
    }
    if let Some(colon_str) = colon {
        formatter = formatter.colon(colon_str).unwrap();
    }

    let target_str = formatter.format_to_string(&target_object).unwrap();
    println!("Testing with target string:\n{}", target_str);

    let lark = format!(
        r#"
        start: %json {object_schema}
    "#
    );
    lark_str_test(&lark, true, &target_str, true);
}
