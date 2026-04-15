use lazy_static::lazy_static;
use rstest::*;
use serde_json::{json, Value};

use llg_test_utils::{json_err_test, json_schema_check};

lazy_static! {
    static ref SINGLE_PROPERTY_SCHEMA: Value =
        json!({"type":"object", "properties": {"a": {"type":"integer"}}, "required": ["a"]});
}

#[rstest]
#[case(&json!({"a":123}))]
#[case(&json!({"a":0}))]
fn single_property(#[case] obj: &Value) {
    json_schema_check(&SINGLE_PROPERTY_SCHEMA, obj, true);
}

#[rstest]
#[case(&json!({"a":"Hello"}))]
#[case(&json!({"b":0}))]
fn single_property_failures(#[case] obj: &Value) {
    json_schema_check(&SINGLE_PROPERTY_SCHEMA, obj, false);
}

lazy_static! {
    static ref MULTIPLE_PROPERTY_SCHEMA: Value = json!({"type":"object", "properties": {
            "a": {"type":"integer"},
            "b": {"type":"string"}
        }, "required": ["a", "b"]});
}

#[rstest]
#[case(&json!({"a":123, "b": "Hello"}))]
#[case(&json!({"a":0, "b": "World"}))]
fn multiple_properties(#[case] obj: &Value) {
    json_schema_check(&MULTIPLE_PROPERTY_SCHEMA, obj, true);
}

#[rstest]
#[case(&json!({"a":123}))]
#[case(&json!({"b": "Hello"}))]
#[case(&json!({"c": 1}))]
fn multiple_properties_failures(#[case] obj: &Value) {
    json_schema_check(&MULTIPLE_PROPERTY_SCHEMA, obj, false);
}

lazy_static! {
    static ref NESTED_SCHEMA: Value = json!({
            "type": "object",
            "properties": {
                "name": {"type": "string"},
                "info": {
                    "type": "object",
                "properties": {
                    "a": {"type": "integer"},
                    "b": {"type": "integer"}
                },
                "required": ["a", "b"]
            }
        },
        "required": ["name", "info"]
    });
}

#[rstest]
#[case(&json!({"name": "Test", "info": {"a": 123, "b": 456}}))]
fn nested(#[case] obj: &Value) {
    json_schema_check(&NESTED_SCHEMA, obj, true);
}

#[rstest]
#[case(&json!({"name": "Test", "info": {"a": 123}}))]
#[case(&json!({"name": "Test", "info": {"a": "123", "b":20}}))]
#[case(&json!({"name": "Test", "info": {"a": 123, "b": "456"}}))]
#[case(&json!({"name": "Test", "info": {"b": 456}}))]
#[case(&json!({"name": "Test", "info": {"c": 1}}))]
fn nested_failures(#[case] obj: &Value) {
    json_schema_check(&NESTED_SCHEMA, obj, false);
}

lazy_static! {
    static ref OBJECT_WITH_ARRAY: Value = json!({"type":"object", "properties": {
            "name" : {"type": "string"},
            "values": {
                "type": "array",
                "items": {"type": "integer"}
            }
        },
        "required": ["name", "values"]
    });
}

#[rstest]
#[case(&json!({"name": "Test", "values": [1, 2, 3]}))]
fn object_with_array(#[case] obj: &Value) {
    json_schema_check(&OBJECT_WITH_ARRAY, obj, true);
}

#[rstest]
#[case(&json!({"name": "Test", "values": [1, 2, "Hello"]}))]
#[case(&json!({"name": "Test", "values": [1.0, 2.0]}))]
#[case(&json!({"name": "Test"}))]
#[case(&json!({"values": [1, 2, 3]}))]
fn object_with_array_failures(#[case] obj: &Value) {
    json_schema_check(&OBJECT_WITH_ARRAY, obj, false);
}

lazy_static! {
    static ref FALSE_PROPERTY: Value = json!({
        "type": "object",
        "properties": {"a": {"type": "integer"}, "b": false},
        "additionalProperties": false,
    });
}

#[rstest]
#[case(&json!({"a": 42}))]
fn object_false_property(#[case] obj: &Value) {
    json_schema_check(&FALSE_PROPERTY, obj, true);
}

#[rstest]
#[case(&json!({"a": 42, "b": 43}))]
fn object_false_property_failures(#[case] obj: &Value) {
    json_schema_check(&FALSE_PROPERTY, obj, false);
}

#[rstest]
#[case(&json!({
            "type": "object",
            "properties": {"a": {"type": "integer"}, "b": false},
            "required": ["b"],
            "additionalProperties": false,
        }))]
#[case(&json!({
            "type": "object",
            "properties": {"a": {"type": "integer"}},
            "required": ["a", "b"],
            "additionalProperties": false,
        }))]
fn object_unsatisfiable_schema(#[case] schema: &Value) {
    json_err_test(schema, "Unsatisfiable schema");
}

lazy_static! {
    static ref LINKED_LIST: Value = json!({
        "$defs": {
            "A": {
                "properties": {
                    "my_str": {
                        "default": "me",
                        "title": "My Str",
                        "type": "string"
                    },
                    "next": {
                        "anyOf": [
                            {
                                "$ref": "#/$defs/A"
                            },
                            {
                                "type": "null"
                            }
                        ]
                    }
                },
                "required": ["my_str", "next"],
                "type": "object"
            }
        },
        "type": "object",
        "properties": {
            "my_list": {
                "anyOf": [
                    {
                        "$ref": "#/$defs/A"
                    },
                    {
                        "type": "null"
                    }
                ]
            }
        },
        "required": ["my_list"]
    });
}

#[rstest]
#[case::null(&json!({"my_list": null}))]
#[case::single_node(&json!({"my_list": {"my_str": "first", "next": null}}))]
#[case::two_nodes(&json!({"my_list": {"my_str": "first", "next": {"my_str": "second", "next": null}}}))]
#[case::three_nodes(&json!({"my_list": {"my_str": "first", "next": {"my_str": "second", "next": {"my_str": "third", "next": null}}}}))]
fn linked_list(#[case] obj: &Value) {
    json_schema_check(&LINKED_LIST, obj, true);
}

#[rstest]
#[case::invalid_type(&json!({"my_list": {"my_str": 1}}))]
#[case::invalid_next_type(&json!({"my_list": {"my_str": "first", "next": "second"}}))]
fn linked_list_failures(#[case] obj: &Value) {
    json_schema_check(&LINKED_LIST, obj, false);
}
