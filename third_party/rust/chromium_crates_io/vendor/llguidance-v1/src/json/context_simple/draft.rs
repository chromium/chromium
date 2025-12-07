// based on https://github.com/Stranger6667/jsonschema/blob/b025051b3c4dd4df4d30b83d23e6c8a13717b62b/crates/jsonschema-referencing/src/specification/mod.rs

// MIT License

// Copyright (c) 2020-2024 Dmitry Dygalo

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

use anyhow::Error;
use serde_json::Value;

/// JSON Schema specification versions.
#[non_exhaustive]
#[derive(Debug, Default, PartialEq, Copy, Clone, Hash, Eq, PartialOrd, Ord)]
pub enum Draft {
    /// JSON Schema Draft 4
    Draft4,
    /// JSON Schema Draft 6
    Draft6,
    /// JSON Schema Draft 7
    Draft7,
    /// JSON Schema Draft 2019-09
    Draft201909,
    /// JSON Schema Draft 2020-12
    #[default]
    Draft202012,
}

impl Draft {
    #[must_use]
    pub fn create_resource_ref(self, contents: &Value) -> ResourceRef<'_> {
        ResourceRef::new(contents, self)
    }

    pub fn detect(self, contents: &Value) -> Result<Draft, Error> {
        if let Some(schema) = contents
            .as_object()
            .and_then(|contents| contents.get("$schema"))
            .and_then(|schema| schema.as_str())
        {
            Ok(match schema.trim_end_matches('#') {
                "https://json-schema.org/draft/2020-12/schema" => Draft::Draft202012,
                "https://json-schema.org/draft/2019-09/schema" => Draft::Draft201909,
                "http://json-schema.org/draft-07/schema" => Draft::Draft7,
                "http://json-schema.org/draft-06/schema" => Draft::Draft6,
                "http://json-schema.org/draft-04/schema" => Draft::Draft4,
                value => return Err(anyhow::anyhow!("Unknown specification: {}", value)),
            })
        } else {
            Ok(self)
        }
    }

    /// Identifies known JSON schema keywords per draft.
    #[must_use]
    pub fn is_known_keyword(&self, keyword: &str) -> bool {
        match keyword {
            "$ref"
            | "$schema"
            | "additionalItems"
            | "additionalProperties"
            | "allOf"
            | "anyOf"
            | "dependencies"
            | "enum"
            | "exclusiveMaximum"
            | "exclusiveMinimum"
            | "format"
            | "items"
            | "maxItems"
            | "maxLength"
            | "maxProperties"
            | "maximum"
            | "minItems"
            | "minLength"
            | "minProperties"
            | "minimum"
            | "multipleOf"
            | "not"
            | "oneOf"
            | "pattern"
            | "patternProperties"
            | "properties"
            | "required"
            | "type"
            | "uniqueItems" => true,

            "id" if *self == Draft::Draft4 => true,

            "$id" | "const" | "contains" | "propertyNames" if *self >= Draft::Draft6 => true,

            "contentEncoding" | "contentMediaType"
                if matches!(self, Draft::Draft6 | Draft::Draft7) =>
            {
                true
            }

            "else" | "if" | "then" if *self >= Draft::Draft7 => true,

            "$anchor"
            | "$defs"
            | "$recursiveAnchor"
            | "$recursiveRef"
            | "dependentRequired"
            | "dependentSchemas"
            | "maxContains"
            | "minContains"
            | "prefixItems"
            | "unevaluatedItems"
            | "unevaluatedProperties"
                if *self >= Draft::Draft201909 =>
            {
                true
            }

            "$dynamicAnchor" | "$dynamicRef" if *self == Draft::Draft202012 => true,

            _ => false,
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct ResourceRef<'a> {
    contents: &'a Value,
    draft: Draft,
}

impl<'a> ResourceRef<'a> {
    #[must_use]
    pub fn new(contents: &'a Value, draft: Draft) -> Self {
        Self { contents, draft }
    }
    #[must_use]
    pub fn contents(&self) -> &'a Value {
        self.contents
    }
    #[must_use]
    #[allow(dead_code)]
    pub fn draft(&self) -> Draft {
        self.draft
    }
}
