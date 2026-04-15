use lazy_static::lazy_static;
use rstest::*;
use serde_json::{json, Value};

use llg_test_utils::{json_err_test, json_schema_check};

lazy_static! {
    static ref INTEGER_ARRAY: Value = json!({"type":"array", "items": {"type":"integer"}});
}

#[rstest]
#[case::empty_list(&json!([]),)]
#[case::single_item(&json!([1]),)]
#[case(&json!([1, 2, 3]),)]
fn array_integer(#[case] sample_array: &Value) {
    json_schema_check(&INTEGER_ARRAY, sample_array, true);
}
#[rstest]
#[case(&json!([1, "Hello"]),)]
#[case(&json!([true, false]),)]
#[case(&json!([1.0, 3.0]),)]
fn array_integer_failures(#[case] sample_array: &Value) {
    json_schema_check(&INTEGER_ARRAY, sample_array, false);
}

lazy_static! {
    static ref BOOLEAN_ARRAY: Value = json!({"type":"array", "items": {"type":"boolean"}});
}

#[rstest]
#[case::empty_list(&json!([]),)]
#[case::single_item(&json!([true]),)]
#[case(&json!([false]),)]
#[case(&json!([false, true]),)]
fn array_boolean(#[case] sample_array: &Value) {
    json_schema_check(&BOOLEAN_ARRAY, sample_array, true);
}
#[rstest]
#[case(&json!([true, 0]),)]
#[case(&json!([false, 1]),)]
#[case(&json!([1.0, 0.0]),)]
fn array_boolean_failures(#[case] sample_array: &Value) {
    json_schema_check(&BOOLEAN_ARRAY, sample_array, false);
}

lazy_static! {
    static ref LENGTH_CONSTRAINED_ARRAY: Value =
        json!({"type":"array", "items": {"type":"integer"}, "minItems": 2, "maxItems": 4});
}

#[rstest]
#[case::lower_bound(&json!([1,2]))]
#[case::between_bounds(&json!([1,2, 3]))]
#[case::upper_bound(&json!([1,2, 3, 4]))]
fn array_length_constraints(#[case] sample_array: &Value) {
    json_schema_check(&LENGTH_CONSTRAINED_ARRAY, sample_array, true);
}

#[rstest]
#[case::empty_list(&json!([]))]
#[case::single_item(&json!([1]))]
#[case::too_long(&json!([1,2,3,4,5]))]
fn array_length_failures(#[case] sample_array: &Value) {
    json_schema_check(&LENGTH_CONSTRAINED_ARRAY, sample_array, false);
}

#[test]
fn array_length_bad_constraints() {
    json_err_test(
        &json!({"type":"array", "items": {"type":"integer"}, "minItems": 2, "maxItems": 1}),
        "Unsatisfiable schema: minItems (2) is greater than maxItems (1)",
    );
}

lazy_static! {
    static ref NESTED_ARRAY: Value =
        json!({"type":"array", "items": {"type":"array", "items": {"type":"integer"}}});
}

#[rstest]
#[case::empty_list(&json!([]))]
#[case(&json!([[1]]))]
#[case(&json!([[1], []]))]
#[case(&json!([[], [1]]))]
#[case(&json!([[1, 2], [3, 4]]))]
#[case(&json!([[0], [1, 2, 3]]))]
#[case(&json!([[0], [1, 2, 3], [4, 5]]))]
fn nested_array(#[case] sample_array: &Value) {
    json_schema_check(&NESTED_ARRAY, sample_array, true);
}

#[rstest]
#[case(&json!([[1, "Hello"]]))]
#[case(&json!([[true, false]]))]
#[case(&json!([[1.0, 2.0]]))]
#[case(&json!([[1], [2.0]]))]
fn nested_array_failures(#[case] sample_array: &Value) {
    json_schema_check(&NESTED_ARRAY, sample_array, false);
}

lazy_static! {
    static ref ARRAY_OF_OBJECTS: Value = json!({
        "type":"array",
        "items": {
            "type":"object",
            "properties":
             {
                "a": {"type":"integer"}
            },
            "required": ["a"]
        }
    });
}

#[rstest]
#[case::empty_list(&json!([]))]
#[case::single_item(&json!([{"a": 1}]))]
#[case::multiple_items(&json!([{"a": 1}, {"a": 2}]))]
fn array_of_objects(#[case] sample_array: &Value) {
    json_schema_check(&ARRAY_OF_OBJECTS, sample_array, true);
}

#[rstest]
#[case(&json!([{"b": 1}]))]
#[case(&json!([{"a": "Hello"}]))]
#[case(&json!([{"a": 1}, {"b": 2}]))]
fn array_of_objects_failures(#[case] sample_array: &Value) {
    json_schema_check(&ARRAY_OF_OBJECTS, sample_array, false);
}

lazy_static! {
    static ref SIMPLE_PREFIXED_ARRAY: Value = json!({ "type": "array",
      "prefixItems": [
        { "type": "string" }, // First item must be a string
        { "type": "number" }  // Second item must be a number
      ],
      "items": { "type": "boolean" } // Remaining items must be booleans
    });
}

#[rstest]
#[case::only_prefix(&json!(["Hello", 42]))]
#[case::prefix_one_item(&json!(["Cruel", 817.2, true]))]
#[case::prefix_multiple_items(&json!(["World", 41.3, false, true, false]))]
fn array_with_prefix_items(#[case] sample_array: &Value) {
    json_schema_check(&SIMPLE_PREFIXED_ARRAY, sample_array, true);
}

#[rstest]
#[case(&json!([41.3]))]
#[case(&json!(["Hello", 42, 41.3]))]
#[case(&json!(["Hello", 42, true, "Not a boolean"]))]
fn array_with_prefix_items_failures(#[case] sample_array: &Value) {
    json_schema_check(&SIMPLE_PREFIXED_ARRAY, sample_array, false);
}

lazy_static! {
    static ref SIMPLE_PREFIXED_ARRAY_FIXED: Value = json!({ "type": "array",
      "prefixItems": [
        { "type": "string" }, // First item must be a string
        { "type": "number" }  // Second item must be a number
      ],
      "items": false // No additional items allowed
    });
}

#[rstest]
#[case(&json!(["A"]))]
#[case(&json!(["B", 42]))]
fn array_with_prefix_items_fixed(#[case] sample_array: &Value) {
    json_schema_check(&SIMPLE_PREFIXED_ARRAY_FIXED, sample_array, true);
}

#[rstest]
#[case(&json!(["Hello", 42, true]))]
#[case(&json!([41.3]))]
fn array_with_prefix_items_fixed_failures(#[case] sample_array: &Value) {
    json_schema_check(&SIMPLE_PREFIXED_ARRAY_FIXED, sample_array, false);
}

lazy_static! {
    static ref SIMPLE_PREFIXED_ARRAY_LENGTH_CONSTRAINED: Value = json!({ "type": "array",
      "prefixItems": [
        { "type": "string" }, // First item must be a string
        { "type": "number" }  // Second item must be a number
      ],
      "items": { "type": "boolean" }, // Remaining items must be booleans
      "minItems": 2,
        "maxItems": 4
    });
}

#[rstest]
#[case(&json!(["Hello", 3.13]))]
#[case(&json!(["World", 817.2, false]))]
#[case(&json!(["Test", 1.0, true, false]))]
fn array_with_prefix_items_length_constrained(#[case] sample_array: &Value) {
    json_schema_check(
        &SIMPLE_PREFIXED_ARRAY_LENGTH_CONSTRAINED,
        sample_array,
        true,
    );
}

#[rstest]
#[case::too_short(&json!(["Hello"]))]
#[case::too_long(&json!(["Hello", 41.3, false, true, true]))]
#[case::prefix_schema_violation(&json!([817.2, false]))]
fn array_with_prefix_items_length_constrained_failures(#[case] sample_array: &Value) {
    json_schema_check(
        &SIMPLE_PREFIXED_ARRAY_LENGTH_CONSTRAINED,
        sample_array,
        false,
    );
}
