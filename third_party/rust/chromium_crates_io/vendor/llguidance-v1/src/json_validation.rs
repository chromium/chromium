use anyhow::{anyhow, Result};
use jsonschema::Validator;
use lazy_static::lazy_static;
use serde_json::{json, Value};

struct DummyResolver {}
impl jsonschema::Retrieve for DummyResolver {
    fn retrieve(
        &self,
        uri: &jsonschema::Uri<String>,
    ) -> std::result::Result<Value, Box<dyn std::error::Error + Send + Sync>> {
        Err(anyhow!("external resolver disabled (url: {})", uri).into())
    }
}

lazy_static! {
    static ref SCHEMA_VALIDATOR: Validator = {
        Validator::options()
            .with_draft(jsonschema::Draft::Draft7)
            .with_retriever(DummyResolver {})
            .build(&json!({
                "$ref": "http://json-schema.org/draft-07/schema"
            }))
            .unwrap()
    };
}

pub fn validate_schema(schema: &Value) -> Result<()> {
    SCHEMA_VALIDATOR
        .validate(schema)
        .map_err(|e| anyhow!("Invalid schema: {}", e))
}
