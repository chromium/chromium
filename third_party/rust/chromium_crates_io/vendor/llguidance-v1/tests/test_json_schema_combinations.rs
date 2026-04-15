// This is for testing anyOf and allOf in JSON schema

use lazy_static::lazy_static;
use rstest::*;
use serde_json::{json, Value};

use llg_test_utils::{json_err_test, json_schema_check, NumericBounds};

lazy_static! {
    static ref SIMPLE_ANYOF: Value = json!({"anyOf": [
        {"type": "integer"},
        {"type": "boolean"}
    ]});
}

#[rstest]
fn simple_anyof(#[values(json!(42), json!(true))] sample: Value) {
    json_schema_check(&SIMPLE_ANYOF, &sample, true);
}

#[rstest]
fn simple_anyof_failures(#[values(json!("string"), json!(1.2), json!([1, 2]))] sample: Value) {
    json_schema_check(&SIMPLE_ANYOF, &sample, false);
}

#[rstest]
#[case(&json!(true), true)]
#[case(&json!(42), true)]
#[case(&json!("string"), false)]
#[case(&json!([1, 2]), false)]
fn type_as_list(#[case] sample: &Value, #[case] expected_pass: bool) {
    // Turns out that "type" can be a list, which acts like anyOf
    let schema = json!({"type": ["boolean", "integer"]});
    json_schema_check(&schema, sample, expected_pass);
}

lazy_static! {
    static ref SIMPLE_ALLOF: Value = json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "allOf": [
            {"properties": {"foo": {"type": "string"}}, "required": ["foo"]},
            {"properties": {"bar": {"type": "integer"}}, "required": ["bar"]},
        ],
    });
}

#[rstest]
fn simple_allof(#[values(json!({"foo": "hello", "bar": 42}))] sample: Value) {
    json_schema_check(&SIMPLE_ALLOF, &sample, true);
}

#[rstest]
fn simple_allof_failures(
    #[values(json!({"foo": "hello"}), json!({"bar": 42}), json!({"foo": "hello", "bar": "not a number"}) )]
    sample: Value,
) {
    json_schema_check(&SIMPLE_ALLOF, &sample, false);
}

lazy_static! {
    static ref ALLOF_WITH_BASE: Value = json!({
            "$schema": "https://json-schema.org/draft/2020-12/schema",
            "properties": {"bar": {"type": "integer"}},
            "required": ["bar"],
            "allOf": [
                {"properties": {"foo": {"type": "string"}}, "required": ["foo"]},
                {"properties": {"baz": {"type": "null"}}, "required": ["baz"]},
            ],
    });
}

#[rstest]
#[case(&json!({"bar": 2, "foo": "quux", "baz": null}), true)]
#[case(&json!({"foo": "quux", "baz": null}), false)]
#[case(&json!({"bar": 2, "baz": null}), false)]
#[case(&json!({"bar": 2, "foo": "quux"}), false)]
#[case(&json!({"bar": 2}), false)]
fn allof_with_base(#[case] sample: &Value, #[case] expected_pass: bool) {
    json_schema_check(&ALLOF_WITH_BASE, sample, expected_pass);
}

#[rstest]
#[case(-35, false)]
#[case(0, false)]
#[case(29, false)]
#[case(35, true)]
#[case(381925, true)]
fn allof_simple_minimum(
    #[values(NumericBounds::Inclusive, NumericBounds::Exclusive)] bound_type_1: NumericBounds,
    #[values(NumericBounds::Inclusive, NumericBounds::Exclusive)] bound_type_2: NumericBounds,
    #[case] value: i32,
    #[case] expected_pass: bool,
) {
    let b1_str = match bound_type_1 {
        NumericBounds::Inclusive => "minimum",
        NumericBounds::Exclusive => "exclusiveMinimum",
    };
    let b2_str = match bound_type_2 {
        NumericBounds::Inclusive => "minimum",
        NumericBounds::Exclusive => "exclusiveMinimum",
    };

    let schema = json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "allOf": [{b1_str: 30}, {b2_str: 20}],
    });
    json_schema_check(&schema, &json!(value), expected_pass);
}

#[rstest]
fn allof_additionalproperties(#[values(false, true)] additional_properties: bool) {
    let schema = json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
            "properties": {"bar": {"type": "integer"}},
            "additionalProperties": additional_properties,
    });

    let minimal_obj = json!({"bar": 2});
    json_schema_check(&schema, &minimal_obj, true);

    let extra_obj = json!({"bar": 2, "foo": "quux"});
    #[allow(clippy::bool_comparison)]
    json_schema_check(&schema, &extra_obj, additional_properties == true);
}

#[rstest]
#[case(3, false)]
#[case(5, false)]
#[case(9, false)]
#[case(15, true)]
#[case(20, false)]
#[case(45, true)]
fn allof_multipleof(#[case] value: i32, #[case] expected_pass: bool) {
    let schema = json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "allOf": [{"multipleOf": 3}, {"multipleOf": 5}],
    });
    json_schema_check(&schema, &json!(value), expected_pass);
}

#[rstest]
#[case("a", true)]
#[case("b", true)]
// Issue 224 #[case("bb", false)]
// Issue 224 #[case("aa", false)]
#[case("", false)]
#[case(" ", false)]
fn allof_string_patterns(#[case] value: &str, #[case] expected_pass: bool) {
    let schema = json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "allOf": [
            {"type": "string", "pattern": r"\w+"},
            {"type": "string", "pattern": r"\w?"}
        ]
    });
    json_schema_check(&schema, &json!(value), expected_pass);
}

#[rstest]
fn allof_unsatisfiable_false_schema(#[values(true, false)] other_schema: bool) {
    let schema = &json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "allOf": [other_schema, false],
    });
    json_err_test(schema, "Unsatisfiable schema: schema is false");
}

#[rstest]
fn allof_unsatisfiable() {
    let schema = &json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "allOf": [
            {"type": "integer", "minimum": 10},
            {"type": "integer", "maximum": 5}
        ]
    });
    json_err_test(
        schema,
        "Unsatisfiable schema: minimum (10) is greater than maximum (5)",
    );
}

#[rstest]
#[case(&json!("a"), false)]
#[case(&json!("ab"), true)]
#[case(&json!(1), false)]
#[case(&json!(2), true)]
fn anyof_allof_nested(#[case] value: &Value, #[case] expected_pass: bool) {
    let schema = &json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "anyOf": [
            {"allOf": [
                {"type": "string"},
                {"minLength": 2}
            ]},
           {"allOf": [
                {"type": "integer"},
                {"minimum": 2}
            ]}
        ]
    });
    json_schema_check(schema, value, expected_pass);
}

#[rstest]
#[case(&json!("a"), false)]
#[case(&json!("ab"), true)]
#[case(&json!(1), false)]
#[case(&json!(2), true)]
fn allof_anyof_nested(#[case] value: &Value, #[case] expected_pass: bool) {
    let schema = &json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "allOf": [
            {"anyOf": [
                {"type": "string"},
                {"minimum": 2}
            ]},
           {"anyOf": [
                {"type": "integer"},
                {"minLength": 2}
            ]}
        ]
    });
    json_schema_check(schema, value, expected_pass);
}

#[rstest]
#[case(&json!("a"), true)]
#[case(&json!("b"), true)]
#[case(&json!(1), false)]
fn anyof_one_unsatisfiable(#[case] value: &Value, #[case] expected_pass: bool) {
    let schema = &json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "anyOf": [
            {"allOf": [
                {"type": "integer", "minimum": 10},
                {"type": "integer", "maximum": 5}
            ]},
            {"type": "string"}
        ]
    });
    json_schema_check(schema, value, expected_pass);
}

#[rstest]
#[case(&json!("a"), true)]
#[case(&json!("b"), true)]
#[case(&json!(1), true)]
#[case(&json!(2.0), false)]
fn oneof_smoke(#[case] value: &Value, #[case] expected_pass: bool) {
    let schema = &json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "oneOf": [
            {"type": "string"},
            {"type": "integer"}
        ]
    });
    json_schema_check(schema, value, expected_pass);
}

#[rstest]
#[case(&json!("a"), true)]
#[case(&json!("b"), true)]
#[case(&json!(1), false)]
#[case(&json!(3), true)]
fn oneof_anyof_1(#[case] value: &Value, #[case] expected_pass: bool) {
    let schema = &json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "oneOf": [
            {"type": "string"},
            {"anyOf": [{"enum": [3, 6, 15, 30]}]}
        ]
    });
    json_schema_check(schema, value, expected_pass);
}

#[rstest]
#[case(&json!("a"), true)]
#[case(&json!("b"), true)]
#[case(&json!(1), false)]
#[case(&json!(3), true)]
fn oneof_anyof_2(#[case] value: &Value, #[case] expected_pass: bool) {
    // Reverses the order of the schema
    let schema = &json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "oneOf": [
            {"anyOf": [{"enum": [3, 6, 15, 30]}]},
            {"type": "string"},
        ]
    });
    json_schema_check(schema, value, expected_pass);
}

#[rstest]
#[case(&json!({"a": "hello"}), true)]
#[case(&json!({"a": "hello", "b": 42}), true)]
#[case(&json!({"a": "hello", "c": 817.2}), true)]
#[case(&json!({"a": "hello", "b": 42, "c": 41.3}), false)]
fn anyof_object_schema(#[case] value: &Value, #[case] expected_pass: bool) {
    let schema = &json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "anyOf": [
            {
                "type": "object",
                 "properties": {"a": {"type": "string"}, "b": {"type":"integer"}},
                  "required": ["a"],
                   "additionalProperties": false
            },
            {
                "type": "object",
                 "properties": {"a": {"type": "string"}, "c": {"type":"number"}},
                  "required": ["a"],
                   "additionalProperties": false
            }
        ],
    });

    json_schema_check(schema, value, expected_pass);
}

#[rstest]
#[case(&json!({"a": "hello"}), true)]
#[case(&json!({"a": "hello", "b": 42}), true)]
#[case(&json!({"a": "hello", "b": 42, "another":{}}), true)]
#[case(&json!({"a": "hello", "c": 817.2}), true)]
#[case(&json!({"a": "hello", "b": 42, "c": 41.3}), false)]
#[case(&json!({"a": "hello", "b": 42, "c": 41.3, "another": []}), false)]
fn anyof_object_schema_with_additional_properties(
    #[case] value: &Value,
    #[case] expected_pass: bool,
) {
    let schema = &json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "anyOf": [
            {
                "type": "object",
                 "properties":
                  {"a": {"type": "string"},
                   "b": {"type":"integer"}},
                    "required": ["a"],
                    "additionalProperties": {"type": "object"}
                },
            {
                "type": "object",
                "properties": {
                    "a": {"type": "string"},
                     "c": {"type":"number"}},
                      "required": ["a"],
                       "additionalProperties": {"type": "array"}
                    }
        ],
    });

    json_schema_check(schema, value, expected_pass);
}

#[rstest]
fn allof_anyof_oneof_combined() {
    let schema = &json!({
            "$schema": "https://json-schema.org/draft/2020-12/schema",
            "allOf": [{"enum": [2, 6, 10, 30]}],
            "anyOf": [{"enum": [3, 6, 15, 30]}],
            "oneOf": [{"enum": [5, 10, 15, 30]}],
    });

    for i in -35..=35 {
        let value = json!(i);
        let expected_pass = i == 30;
        json_schema_check(schema, &value, expected_pass);
    }
}
