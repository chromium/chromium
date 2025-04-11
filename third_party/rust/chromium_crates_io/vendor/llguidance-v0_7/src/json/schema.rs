use crate::HashMap;
use anyhow::{anyhow, bail, Result};
use derivre::RegexAst;
use indexmap::{IndexMap, IndexSet};
use serde_json::Value;
use std::mem;

use super::context::{Context, Draft, PreContext, ResourceRef};
use super::formats::lookup_format;
use super::numeric::Decimal;
use super::RetrieveWrapper;

const TYPES: [&str; 6] = ["null", "boolean", "number", "string", "array", "object"];

// Keywords that are implemented in this module
pub(crate) const IMPLEMENTED: [&str; 24] = [
    // Core
    "anyOf",
    "oneOf",
    "allOf",
    "$ref",
    "const",
    "enum",
    "type",
    // Array
    "items",
    "additionalItems",
    "prefixItems",
    "minItems",
    "maxItems",
    // Object
    "properties",
    "additionalProperties",
    "required",
    // String
    "minLength",
    "maxLength",
    "pattern",
    "format",
    // Number
    "minimum",
    "maximum",
    "exclusiveMinimum",
    "exclusiveMaximum",
    "multipleOf",
];

// Keywords that are used for metadata or annotations, not directly driving validation.
// Note that some keywords like $id and $schema affect the behavior of other keywords, but
// they can safely be ignored if other keywords aren't present
pub(crate) const META_AND_ANNOTATIONS: [&str; 15] = [
    "$anchor",
    "$defs",
    "definitions",
    "$schema",
    "$id",
    "id",
    "$comment",
    "title",
    "description",
    "default",
    "readOnly",
    "writeOnly",
    "examples",
    "contentMediaType",
    "contentEncoding",
];

fn limited_str(node: &Value) -> String {
    let s = node.to_string();
    if s.len() > 100 {
        format!("{}...", &s[..100])
    } else {
        s
    }
}

#[derive(Debug, Clone)]
pub enum Schema {
    Any,
    Unsatisfiable {
        reason: String,
    },
    Null,
    Boolean,
    Number {
        minimum: Option<f64>,
        maximum: Option<f64>,
        exclusive_minimum: Option<f64>,
        exclusive_maximum: Option<f64>,
        multiple_of: Option<Decimal>,
        integer: bool,
    },
    String {
        min_length: u64,
        max_length: Option<u64>,
        regex: Option<RegexAst>,
    },
    Array {
        min_items: u64,
        max_items: Option<u64>,
        prefix_items: Vec<Schema>,
        items: Option<Box<Schema>>,
    },
    Object {
        properties: IndexMap<String, Schema>,
        additional_properties: Option<Box<Schema>>,
        required: IndexSet<String>,
    },
    LiteralBool {
        value: bool,
    },
    AnyOf {
        options: Vec<Schema>,
    },
    OneOf {
        options: Vec<Schema>,
    },
    Ref {
        uri: String,
    },
}

impl Schema {
    pub fn false_schema() -> Schema {
        Schema::Unsatisfiable {
            reason: "schema is false".to_string(),
        }
    }

    /// Shallowly normalize the schema, removing any unnecessary nesting or empty options.
    fn normalize(self) -> Schema {
        match self {
            Schema::AnyOf { options } => {
                let mut unsats = Vec::new();
                let mut valid = Vec::new();
                for option in options.into_iter() {
                    match option {
                        Schema::Any => {
                            return Schema::Any;
                        }
                        Schema::Unsatisfiable { reason } => {
                            unsats.push(Schema::Unsatisfiable { reason })
                        }
                        Schema::AnyOf { options: nested } => valid.extend(nested),
                        other => valid.push(other),
                    }
                }
                if valid.is_empty() {
                    // Return the first unsatisfiable schema for debug-ability
                    if let Some(unsat) = unsats.into_iter().next() {
                        return unsat;
                    }
                    // We must not have had any schemas to begin with
                    return Schema::Unsatisfiable {
                        reason: "anyOf is empty".to_string(),
                    };
                }
                if valid.len() == 1 {
                    // Unwrap singleton
                    return valid.swap_remove(0);
                }
                Schema::AnyOf { options: valid }
            }
            Schema::OneOf { options } => {
                let mut unsats = Vec::new();
                let mut valid = Vec::new();
                for option in options.into_iter() {
                    match option {
                        Schema::Unsatisfiable { reason } => {
                            unsats.push(Schema::Unsatisfiable { reason })
                        }
                        // Flatten nested oneOfs: (A⊕B)⊕(C⊕D) = A⊕B⊕C⊕D
                        Schema::OneOf { options: nested } => valid.extend(nested),
                        other => valid.push(other),
                    }
                }
                if valid.is_empty() {
                    // Return the first unsatisfiable schema for debug-ability
                    if let Some(unsat) = unsats.into_iter().next() {
                        return unsat;
                    }
                    // We must not have had any schemas to begin with
                    return Schema::Unsatisfiable {
                        reason: "oneOf is empty".to_string(),
                    };
                }
                if valid.len() == 1 {
                    // Unwrap singleton
                    return valid.swap_remove(0);
                }
                if valid.iter().enumerate().all(|(i, x)| {
                    valid
                        .iter()
                        .skip(i + 1) // "upper diagonal"
                        .all(|y| x.is_verifiably_disjoint_from(y))
                }) {
                    Schema::AnyOf { options: valid }
                } else {
                    Schema::OneOf { options: valid }
                }
            }
            other_schema => other_schema,
        }
    }

    /// Intersect two schemas, returning a new (normalized) schema that represents the intersection of the two.
    fn intersect(self, other: Schema, ctx: &Context, stack_level: usize) -> Result<Schema> {
        ctx.increment()?;
        if stack_level > ctx.options.max_stack_level {
            bail!("Schema intersection stack level exceeded");
        }

        let merged = match (self, other) {
            (Schema::Any, schema1) => schema1,
            (schema0, Schema::Any) => schema0,
            (Schema::Unsatisfiable { reason }, _) => Schema::Unsatisfiable { reason },
            (_, Schema::Unsatisfiable { reason }) => Schema::Unsatisfiable { reason },
            (Schema::Ref { uri }, schema1) => {
                intersect_ref(ctx, &uri, schema1, true, stack_level + 1)?
            }
            (schema0, Schema::Ref { uri }) => {
                intersect_ref(ctx, &uri, schema0, false, stack_level + 1)?
            }
            (Schema::OneOf { options }, schema1) => Schema::OneOf {
                options: options
                    .into_iter()
                    .map(|opt| opt.intersect(schema1.clone(), ctx, stack_level + 1))
                    .collect::<Result<Vec<_>>>()?,
            },
            (schema0, Schema::OneOf { options }) => Schema::OneOf {
                options: options
                    .into_iter()
                    .map(|opt| schema0.clone().intersect(opt, ctx, stack_level + 1))
                    .collect::<Result<Vec<_>>>()?,
            },
            (Schema::AnyOf { options }, schema1) => Schema::AnyOf {
                options: options
                    .into_iter()
                    .map(|opt| opt.intersect(schema1.clone(), ctx, stack_level + 1))
                    .collect::<Result<Vec<_>>>()?,
            },
            (schema0, Schema::AnyOf { options }) => Schema::AnyOf {
                options: options
                    .into_iter()
                    .map(|opt| schema0.clone().intersect(opt, ctx, stack_level + 1))
                    .collect::<Result<Vec<_>>>()?,
            },
            (Schema::Null, Schema::Null) => Schema::Null,
            (Schema::Boolean, Schema::Boolean) => Schema::Boolean,
            (Schema::Boolean, Schema::LiteralBool { value }) => Schema::LiteralBool { value },
            (Schema::LiteralBool { value }, Schema::Boolean) => Schema::LiteralBool { value },
            (Schema::LiteralBool { value: value1 }, Schema::LiteralBool { value: value2 }) => {
                if value1 == value2 {
                    Schema::LiteralBool { value: value1 }
                } else {
                    Schema::Unsatisfiable {
                        reason: "incompatible boolean values".to_string(),
                    }
                }
            }
            (
                Schema::Number {
                    minimum: min1,
                    maximum: max1,
                    exclusive_minimum: emin1,
                    exclusive_maximum: emax1,
                    integer: int1,
                    multiple_of: mult1,
                },
                Schema::Number {
                    minimum: min2,
                    maximum: max2,
                    exclusive_minimum: emin2,
                    exclusive_maximum: emax2,
                    integer: int2,
                    multiple_of: mult2,
                },
            ) => Schema::Number {
                minimum: opt_max(min1, min2),
                maximum: opt_min(max1, max2),
                exclusive_minimum: opt_max(emin1, emin2),
                exclusive_maximum: opt_min(emax1, emax2),
                integer: int1 || int2,
                multiple_of: match (mult1, mult2) {
                    (None, None) => None,
                    (None, Some(mult)) => Some(mult),
                    (Some(mult), None) => Some(mult),
                    (Some(mult1), Some(mult2)) => Some(mult1.lcm(&mult2)),
                },
            },
            (
                Schema::String {
                    min_length: min1,
                    max_length: max1,
                    regex: r1,
                },
                Schema::String {
                    min_length: min2,
                    max_length: max2,
                    regex: r2,
                },
            ) => Schema::String {
                min_length: min1.max(min2),
                max_length: opt_min(max1, max2),
                regex: match (r1, r2) {
                    (None, None) => None,
                    (None, Some(r)) => Some(r),
                    (Some(r), None) => Some(r),
                    (Some(r1), Some(r2)) => Some(RegexAst::And(vec![r1, r2])),
                },
            },
            (
                Schema::Array {
                    min_items: min1,
                    max_items: max1,
                    prefix_items: mut prefix1,
                    items: items1,
                },
                Schema::Array {
                    min_items: min2,
                    max_items: max2,
                    prefix_items: mut prefix2,
                    items: items2,
                },
            ) => Schema::Array {
                min_items: min1.max(min2),
                max_items: opt_min(max1, max2),
                prefix_items: {
                    let len = prefix1.len().max(prefix2.len());
                    prefix1.resize_with(len, || items1.as_deref().cloned().unwrap_or(Schema::Any));
                    prefix2.resize_with(len, || items2.as_deref().cloned().unwrap_or(Schema::Any));
                    prefix1
                        .into_iter()
                        .zip(prefix2.into_iter())
                        .map(|(item1, item2)| item1.intersect(item2, ctx, stack_level + 1))
                        .collect::<Result<Vec<_>>>()?
                },
                items: match (items1, items2) {
                    (None, None) => None,
                    (None, Some(item)) => Some(item),
                    (Some(item), None) => Some(item),
                    (Some(item1), Some(item2)) => Some(Box::new((*item1).intersect(
                        *item2,
                        ctx,
                        stack_level + 1,
                    )?)),
                },
            },
            (
                Schema::Object {
                    properties: props1,
                    additional_properties: add1,
                    required: req1,
                },
                Schema::Object {
                    properties: mut props2,
                    additional_properties: add2,
                    required: req2,
                },
            ) => {
                let mut new_props = IndexMap::new();
                for (key, prop1) in props1.into_iter() {
                    let prop2 = props2
                        .shift_remove(&key)
                        .or_else(|| add2.as_deref().cloned())
                        .unwrap_or(Schema::Any);
                    new_props.insert(key, prop1.intersect(prop2, ctx, stack_level + 1)?);
                }
                for (key, prop2) in props2.into_iter() {
                    let prop1 = add1.as_deref().cloned().unwrap_or(Schema::Any);
                    new_props.insert(key, prop1.intersect(prop2, ctx, stack_level + 1)?);
                }
                let mut required = req1;
                required.extend(req2);
                Schema::Object {
                    properties: new_props,
                    additional_properties: match (add1, add2) {
                        (None, None) => None,
                        (None, Some(add2)) => Some(add2),
                        (Some(add1), None) => Some(add1),
                        (Some(add1), Some(add2)) => {
                            Some(Box::new((*add1).intersect(*add2, ctx, stack_level + 1)?))
                        }
                    },
                    required,
                }
            }
            //TODO: get types for error message
            _ => Schema::Unsatisfiable {
                reason: "incompatible types".to_string(),
            },
        };
        Ok(merged.normalize())
    }

    fn is_verifiably_disjoint_from(&self, other: &Schema) -> bool {
        match (self, other) {
            (Schema::Unsatisfiable { .. }, _) => true,
            (_, Schema::Unsatisfiable { .. }) => true,
            (Schema::Any, _) => false,
            (_, Schema::Any) => false,
            (Schema::Boolean, Schema::LiteralBool { .. }) => false,
            (Schema::LiteralBool { .. }, Schema::Boolean) => false,
            (Schema::Ref { .. }, _) => false, // TODO: could resolve
            (_, Schema::Ref { .. }) => false, // TODO: could resolve
            (Schema::LiteralBool { value: value1 }, Schema::LiteralBool { value: value2 }) => {
                value1 != value2
            }
            (Schema::AnyOf { options }, _) => options
                .iter()
                .all(|opt| opt.is_verifiably_disjoint_from(other)),
            (_, Schema::AnyOf { options }) => options
                .iter()
                .all(|opt| self.is_verifiably_disjoint_from(opt)),
            (Schema::OneOf { options }, _) => options
                .iter()
                .all(|opt| opt.is_verifiably_disjoint_from(other)),
            (_, Schema::OneOf { options }) => options
                .iter()
                .all(|opt| self.is_verifiably_disjoint_from(opt)),
            // TODO: could actually compile the regexes and check for overlap
            (
                Schema::String {
                    regex: Some(RegexAst::Literal(lit1)),
                    ..
                },
                Schema::String {
                    regex: Some(RegexAst::Literal(lit2)),
                    ..
                },
            ) => lit1 != lit2,
            (
                Schema::Object {
                    properties: props1,
                    required: req1,
                    additional_properties: add1,
                },
                Schema::Object {
                    properties: props2,
                    required: req2,
                    additional_properties: add2,
                },
            ) => req1.union(req2).any(|key| {
                let prop1 = props1
                    .get(key)
                    .unwrap_or(add1.as_deref().unwrap_or(&Schema::Any));
                let prop2 = props2
                    .get(key)
                    .unwrap_or(add2.as_deref().unwrap_or(&Schema::Any));
                prop1.is_verifiably_disjoint_from(prop2)
            }),
            _ => {
                // Except for in the cases above, it should suffice to check that the types are different
                mem::discriminant(self) != mem::discriminant(other)
            }
        }
    }

    fn apply(self, applicator: (&str, &Value), ctx: &Context) -> Result<Schema> {
        let mut result = self;
        let (k, v) = applicator;
        match k {
            // TODO: Do const and enum really belong here? Maybe they should always take precedence as they are "literal" constraints?
            "const" => {
                let schema = compile_const(v)?;
                result = result.intersect(schema, ctx, 0)?
            }
            "enum" => {
                let instances = v
                    .as_array()
                    .ok_or_else(|| anyhow!("enum must be an array"))?;
                let options = instances
                    .iter()
                    .map(compile_const)
                    .collect::<Result<Vec<_>>>()?;
                result = result.intersect(Schema::AnyOf { options }, ctx, 0)?;
            }
            "allOf" => {
                let all_of = v
                    .as_array()
                    .ok_or_else(|| anyhow!("allOf must be an array"))?;
                for value in all_of {
                    let schema = compile_resource(ctx, ctx.as_resource_ref(value))?;
                    result = result.intersect(schema, ctx, 0)?;
                }
            }
            "anyOf" => {
                let any_of = v
                    .as_array()
                    .ok_or_else(|| anyhow!("anyOf must be an array"))?;
                let options = any_of
                    .iter()
                    .map(|value| compile_resource(ctx, ctx.as_resource_ref(value)))
                    .collect::<Result<Vec<_>>>()?;
                result = result.intersect(Schema::AnyOf { options }, ctx, 0)?;
            }
            "oneOf" => {
                let one_of = v
                    .as_array()
                    .ok_or_else(|| anyhow!("oneOf must be an array"))?;
                let options = one_of
                    .iter()
                    .map(|value| compile_resource(ctx, ctx.as_resource_ref(value)))
                    .collect::<Result<Vec<_>>>()?;
                result = result.intersect(Schema::OneOf { options }, ctx, 0)?;
            }
            "$ref" => {
                let reference = v
                    .as_str()
                    .ok_or_else(|| anyhow!("$ref must be a string, got {}", limited_str(v)))?
                    .to_string();
                let uri: String = ctx.normalize_ref(&reference)?;
                if matches!(result, Schema::Any) {
                    define_ref(ctx, &uri)?;
                    result = Schema::Ref { uri };
                } else {
                    result = intersect_ref(ctx, &uri, result, false, 0)?;
                }
            }
            _ => bail!("Unknown applicator: {}", applicator.0),
        };
        Ok(result)
    }
}

#[derive(Clone)]
pub struct SchemaBuilderOptions {
    pub max_size: usize,
    pub max_stack_level: usize,
}

impl Default for SchemaBuilderOptions {
    fn default() -> Self {
        SchemaBuilderOptions {
            max_size: 50_000,
            max_stack_level: 128, // consumes ~2.5k of stack per level
        }
    }
}

pub fn build_schema(
    contents: Value,
    retriever: Option<RetrieveWrapper>,
) -> Result<(Schema, HashMap<String, Schema>)> {
    if let Some(b) = contents.as_bool() {
        if b {
            return Ok((Schema::Any, HashMap::default()));
        } else {
            return Ok((Schema::false_schema(), HashMap::default()));
        }
    }

    let pre_ctx = PreContext::new(contents, retriever)?;
    let ctx = Context::new(&pre_ctx)?;

    let root_resource = ctx.lookup_resource(&pre_ctx.base_uri)?;
    let schema = compile_resource(&ctx, root_resource)?;
    Ok((schema, ctx.take_defs()))
}

fn compile_resource(ctx: &Context, resource: ResourceRef) -> Result<Schema> {
    let ctx = ctx.in_subresource(resource)?;
    compile_contents(&ctx, resource.contents())
}

fn compile_contents(ctx: &Context, contents: &Value) -> Result<Schema> {
    compile_contents_inner(ctx, contents).map(|schema| schema.normalize())
}

fn compile_contents_inner(ctx: &Context, contents: &Value) -> Result<Schema> {
    if let Some(b) = contents.as_bool() {
        if b {
            return Ok(Schema::Any);
        } else {
            return Ok(Schema::false_schema());
        }
    }

    // Get the schema as an object
    // TODO: validate against metaschema & check for unimplemented keys
    let schemadict = contents
        .as_object()
        .ok_or_else(|| anyhow!("schema must be an object or boolean"))?;

    // Make a mutable copy of the schema so we can modify it
    let schemadict = schemadict
        .iter()
        .map(|(k, v)| (k.as_str(), v))
        .collect::<IndexMap<_, _>>();

    compile_contents_map(ctx, schemadict)
}

fn compile_contents_map(ctx: &Context, schemadict: IndexMap<&str, &Value>) -> Result<Schema> {
    ctx.increment()?;

    // We don't need to compile the schema if it's just meta and annotations
    if schemadict
        .keys()
        .all(|k| META_AND_ANNOTATIONS.contains(k) || !ctx.draft.is_known_keyword(k))
    {
        return Ok(Schema::Any);
    }

    // Check for unimplemented keys and bail if any are found
    let mut unimplemented_keys = schemadict
        .keys()
        .filter(|k| !ctx.is_valid_keyword(k))
        .collect::<Vec<_>>();
    if !unimplemented_keys.is_empty() {
        // ensure consistent order for tests
        unimplemented_keys.sort();
        bail!("Unimplemented keys: {:?}", unimplemented_keys);
    }

    // Some dummy values to use for properties and prefixItems if we need to apply additionalProperties or items
    // before we know what they are. Define them here so we can reference them in the loop below without borrow-checker
    // issues.
    let dummy_properties = match schemadict.get("properties") {
        Some(properties) => {
            let properties = properties
                .as_object()
                .ok_or_else(|| anyhow!("properties must be an object"))?;
            Some(Value::from_iter(
                properties
                    .iter()
                    .map(|(k, _)| (k.as_str(), Value::Bool(true))),
            ))
        }
        None => None,
    };
    let dummy_prefix_items = match schemadict.get("prefixItems") {
        Some(prefix_items) => {
            let prefix_items = prefix_items
                .as_array()
                .ok_or_else(|| anyhow!("prefixItems must be an array"))?;
            Some(Value::from_iter(
                prefix_items.iter().map(|_| Value::Bool(true)),
            ))
        }
        None => None,
    };

    let mut result = Schema::Any;
    let mut current = HashMap::default();
    let in_place_applicator_kwds = ["const", "enum", "allOf", "anyOf", "oneOf", "$ref"];
    for (k, v) in schemadict.iter() {
        if in_place_applicator_kwds.contains(k) {
            if !current.is_empty() {
                if let Some(&types) = schemadict.get("type") {
                    // Make sure we always give type information to ensure we get the smallest union we can
                    current.insert("type", types);
                }
                let current_schema = compile_contents_simple(ctx, std::mem::take(&mut current))?;
                result = result.intersect(current_schema, ctx, 0)?;
            }
            // Finally apply the applicator
            result = result.apply((k, v), ctx)?;
        } else if !META_AND_ANNOTATIONS.contains(k) {
            current.insert(k, v);
            if *k == "additionalProperties" && !current.contains_key("properties") {
                // additionalProperties needs to know about properties
                // Insert a dummy version of properties into current
                // (not the real deal, as we don't want to intersect it out of order)
                if let Some(dummy_props) = &dummy_properties {
                    current.insert("properties", dummy_props);
                }
            }
            if *k == "items" && !current.contains_key("prefixItems") {
                // items needs to know about prefixItems
                // Insert a dummy version of prefixItems into current
                // (not the real deal, as we don't want to intersect it out of order)
                if let Some(dummy_itms) = &dummy_prefix_items {
                    current.insert("prefixItems", dummy_itms);
                }
            }
        }
    }
    if !current.is_empty() {
        if let Some(&types) = schemadict.get("type") {
            // Make sure we always give type information to ensure we get the smallest union we can
            current.insert("type", types);
        }
        let current_schema = compile_contents_simple(ctx, std::mem::take(&mut current))?;
        result = result.intersect(current_schema, ctx, 0)?;
    }
    Ok(result)
}

fn compile_contents_simple(ctx: &Context, schemadict: HashMap<&str, &Value>) -> Result<Schema> {
    if schemadict.is_empty() {
        Ok(Schema::Any)
    } else {
        let types = schemadict.get("type");
        match types {
            Some(Value::String(tp)) => compile_type(ctx, tp, &schemadict),
            Some(Value::Array(types)) => {
                let options = types
                    .iter()
                    .map(|type_value| {
                        type_value
                            .as_str()
                            .ok_or_else(|| anyhow!("type must be a string"))
                    })
                    .collect::<Result<Vec<_>>>()?;
                compile_types(ctx, options, &schemadict)
            }
            None => compile_types(ctx, TYPES.to_vec(), &schemadict),
            Some(_) => {
                bail!("type must be a string or array of strings");
            }
        }
    }
}

fn define_ref(ctx: &Context, ref_uri: &str) -> Result<()> {
    if !ctx.been_seen(ref_uri) {
        ctx.mark_seen(ref_uri);
        let resource = ctx.lookup_resource(ref_uri)?;
        let resolved_schema = compile_resource(ctx, resource)?;
        ctx.insert_ref(ref_uri, resolved_schema);
    }
    Ok(())
}

fn intersect_ref(
    ctx: &Context,
    ref_uri: &str,
    schema: Schema,
    ref_first: bool,
    stack_level: usize,
) -> Result<Schema> {
    define_ref(ctx, ref_uri)?;
    let resolved_schema = ctx
        .get_ref_cloned(ref_uri)
        // The ref might not have been defined if we're in a recursive loop and every ref in the loop
        // has a sibling key.
        // TODO: add an extra layer of indirection by defining a URI for the current location (e.g. by hashing the serialized sibling schema)
        // and returning a ref to that URI here to break the loop.
        .ok_or_else(|| {
            anyhow!(
                "circular references with sibling keys are not supported: {}",
                ref_uri
            )
        })?;
    if ref_first {
        resolved_schema.intersect(schema, ctx, stack_level + 1)
    } else {
        schema.intersect(resolved_schema, ctx, stack_level + 1)
    }
}

fn compile_const(instance: &Value) -> Result<Schema> {
    match instance {
        Value::Null => Ok(Schema::Null),
        Value::Bool(b) => Ok(Schema::LiteralBool { value: *b }),
        Value::Number(n) => {
            let value = n.as_f64().ok_or_else(|| {
                anyhow!(
                    "Expected f64 for numeric const, got {}",
                    limited_str(instance)
                )
            })?;
            Ok(Schema::Number {
                minimum: Some(value),
                maximum: Some(value),
                exclusive_minimum: None,
                exclusive_maximum: None,
                integer: n.is_i64(),
                multiple_of: None,
            })
        }
        Value::String(s) => Ok(Schema::String {
            min_length: 0,
            max_length: None,
            regex: Some(RegexAst::Literal(s.to_string())),
        }),
        Value::Array(items) => {
            let prefix_items = items
                .iter()
                .map(compile_const)
                .collect::<Result<Vec<Schema>>>()?;
            Ok(Schema::Array {
                min_items: prefix_items.len() as u64,
                max_items: Some(prefix_items.len() as u64),
                prefix_items,
                items: Some(Box::new(Schema::false_schema())),
            })
        }
        Value::Object(mapping) => {
            let properties = mapping
                .iter()
                .map(|(k, v)| Ok((k.clone(), compile_const(v)?)))
                .collect::<Result<IndexMap<String, Schema>>>()?;
            let required = properties.keys().cloned().collect();
            Ok(Schema::Object {
                properties,
                additional_properties: Some(Box::new(Schema::false_schema())),
                required,
            })
        }
    }
}

fn compile_types(
    ctx: &Context,
    types: Vec<&str>,
    schema: &HashMap<&str, &Value>,
) -> Result<Schema> {
    let mut options = Vec::new();
    for tp in types {
        let option = compile_type(ctx, tp, schema)?;
        options.push(option);
    }
    if options.len() == 1 {
        Ok(options.swap_remove(0))
    } else {
        Ok(Schema::AnyOf { options })
    }
}

fn compile_type(ctx: &Context, tp: &str, schema: &HashMap<&str, &Value>) -> Result<Schema> {
    ctx.increment()?;

    let get = |key: &str| schema.get(key).copied();

    match tp {
        "null" => Ok(Schema::Null),
        "boolean" => Ok(Schema::Boolean),
        "number" | "integer" => compile_numeric(
            get("minimum"),
            get("maximum"),
            get("exclusiveMinimum"),
            get("exclusiveMaximum"),
            tp == "integer",
            get("multipleOf"),
        ),
        "string" => compile_string(
            get("minLength"),
            get("maxLength"),
            get("pattern"),
            get("format"),
        ),
        "array" => compile_array(
            ctx,
            get("minItems"),
            get("maxItems"),
            get("prefixItems"),
            get("items"),
            get("additionalItems"),
        ),
        "object" => compile_object(
            ctx,
            get("properties"),
            get("additionalProperties"),
            get("required"),
        ),
        _ => bail!("Invalid type: {}", tp),
    }
}

fn compile_numeric(
    minimum: Option<&Value>,
    maximum: Option<&Value>,
    exclusive_minimum: Option<&Value>,
    exclusive_maximum: Option<&Value>,
    integer: bool,
    multiple_of: Option<&Value>,
) -> Result<Schema> {
    let minimum = match minimum {
        None => None,
        Some(val) => Some(
            val.as_f64()
                .ok_or_else(|| anyhow!("Expected f64 for 'minimum', got {}", limited_str(val)))?,
        ),
    };
    let maximum = match maximum {
        None => None,
        Some(val) => Some(
            val.as_f64()
                .ok_or_else(|| anyhow!("Expected f64 for 'maximum', got {}", limited_str(val)))?,
        ),
    };
    // TODO: actually use ctx.draft to determine which style of exclusiveMinimum/Maximum to use
    let exclusive_minimum = match exclusive_minimum {
        // Draft4-style boolean values
        None | Some(Value::Bool(false)) => None,
        Some(Value::Bool(true)) => minimum,
        // Draft2020-12-style numeric values
        Some(value) => Some(value.as_f64().ok_or_else(|| {
            anyhow!(
                "Expected f64 for 'exclusiveMinimum', got {}",
                limited_str(value)
            )
        })?),
    };
    let exclusive_maximum = match exclusive_maximum {
        // Draft4-style boolean values
        None | Some(Value::Bool(false)) => None,
        Some(Value::Bool(true)) => maximum,
        // Draft2020-12-style numeric values
        Some(value) => Some(value.as_f64().ok_or_else(|| {
            anyhow!(
                "Expected f64 for 'exclusiveMaximum', got {}",
                limited_str(value)
            )
        })?),
    };
    let multiple_of = match multiple_of {
        None => None,
        Some(val) => {
            let f = val.as_f64().ok_or_else(|| {
                anyhow!("Expected f64 for 'multipleOf', got {}", limited_str(val))
            })?;
            // Can discard the sign of f
            Some(Decimal::try_from(f.abs())?)
        }
    };
    Ok(Schema::Number {
        minimum,
        maximum,
        exclusive_minimum,
        exclusive_maximum,
        integer,
        multiple_of,
    })
}

fn pattern_to_regex(pattern: &str) -> RegexAst {
    let left_anchored = pattern.starts_with('^');
    let right_anchored = pattern.ends_with('$');
    let trimmed = pattern.trim_start_matches('^').trim_end_matches('$');
    let mut result = String::new();
    if !left_anchored {
        result.push_str(".*");
    }
    // without parens, for a|b we would get .*a|b.* which is (.*a)|(b.*)
    result.push('(');
    result.push_str(trimmed);
    result.push(')');
    if !right_anchored {
        result.push_str(".*");
    }
    RegexAst::Regex(result)
}

fn compile_string(
    min_length: Option<&Value>,
    max_length: Option<&Value>,
    pattern: Option<&Value>,
    format: Option<&Value>,
) -> Result<Schema> {
    let min_length = match min_length {
        None => 0,
        Some(val) => val
            .as_u64()
            .ok_or_else(|| anyhow!("Expected u64 for 'minLength', got {}", limited_str(val)))?,
    };
    let max_length = match max_length {
        None => None,
        Some(val) => Some(
            val.as_u64()
                .ok_or_else(|| anyhow!("Expected u64 for 'maxLength', got {}", limited_str(val)))?,
        ),
    };
    let pattern_rx = match pattern {
        None => None,
        Some(val) => Some({
            let s = val
                .as_str()
                .ok_or_else(|| anyhow!("Expected string for 'pattern', got {}", limited_str(val)))?
                .to_string();
            pattern_to_regex(&s)
        }),
    };
    let format_rx = match format {
        None => None,
        Some(val) => Some({
            let key = val
                .as_str()
                .ok_or_else(|| anyhow!("Expected string for 'format', got {}", limited_str(val)))?
                .to_string();
            let fmt = lookup_format(&key).ok_or_else(|| anyhow!("Unknown format: {}", key))?;
            pattern_to_regex(fmt)
        }),
    };
    let regex = match (pattern_rx, format_rx) {
        (None, None) => None,
        (None, Some(fmt)) => Some(fmt),
        (Some(pat), None) => Some(pat),
        (Some(pat), Some(fmt)) => Some(RegexAst::And(vec![pat, fmt])),
    };
    Ok(Schema::String {
        min_length,
        max_length,
        regex,
    })
}

fn compile_array(
    ctx: &Context,
    min_items: Option<&Value>,
    max_items: Option<&Value>,
    prefix_items: Option<&Value>,
    items: Option<&Value>,
    additional_items: Option<&Value>,
) -> Result<Schema> {
    let (prefix_items, items) = {
        // Note that draft detection falls back to Draft202012 if the draft is unknown, so let's relax the draft constraint a bit
        // and assume we're in an old draft if additionalItems is present or items is an array
        if ctx.draft <= Draft::Draft201909
            || additional_items.is_some()
            || matches!(items, Some(Value::Array(..)))
        {
            match (items, additional_items) {
                // Treat array items as prefixItems and additionalItems as items in draft 2019-09 and earlier
                (Some(Value::Array(..)), _) => (items, additional_items),
                // items is treated as items, and additionalItems is ignored if items is not an array (or is missing)
                _ => (None, items),
            }
        } else {
            (prefix_items, items)
        }
    };
    let min_items = match min_items {
        None => 0,
        Some(val) => val
            .as_u64()
            .ok_or_else(|| anyhow!("Expected u64 for 'minItems', got {}", limited_str(val)))?,
    };
    let max_items = match max_items {
        None => None,
        Some(val) => Some(
            val.as_u64()
                .ok_or_else(|| anyhow!("Expected u64 for 'maxItems', got {}", limited_str(val)))?,
        ),
    };
    let prefix_items = match prefix_items {
        None => vec![],
        Some(val) => val
            .as_array()
            .ok_or_else(|| anyhow!("Expected array for 'prefixItems', got {}", limited_str(val)))?
            .iter()
            .map(|item| compile_resource(ctx, ctx.as_resource_ref(item)))
            .collect::<Result<Vec<Schema>>>()?,
    };
    let items = match items {
        None => None,
        Some(val) => Some(Box::new(compile_resource(ctx, ctx.as_resource_ref(val))?)),
    };
    Ok(Schema::Array {
        min_items,
        max_items,
        prefix_items,
        items,
    })
}

fn compile_object(
    ctx: &Context,
    properties: Option<&Value>,
    additional_properties: Option<&Value>,
    required: Option<&Value>,
) -> Result<Schema> {
    let properties = match properties {
        None => IndexMap::new(),
        Some(val) => val
            .as_object()
            .ok_or_else(|| anyhow!("Expected object for 'properties', got {}", limited_str(val)))?
            .iter()
            .map(|(k, v)| compile_resource(ctx, ctx.as_resource_ref(v)).map(|v| (k.clone(), v)))
            .collect::<Result<IndexMap<String, Schema>>>()?,
    };
    let additional_properties = match additional_properties {
        None => None,
        Some(val) => Some(Box::new(compile_resource(ctx, ctx.as_resource_ref(val))?)),
    };
    let required = match required {
        None => IndexSet::new(),
        Some(val) => val
            .as_array()
            .ok_or_else(|| anyhow!("Expected array for 'required', got {}", limited_str(val)))?
            .iter()
            .map(|item| {
                item.as_str()
                    .ok_or_else(|| {
                        anyhow!(
                            "Expected string for 'required' item, got {}",
                            limited_str(item)
                        )
                    })
                    .map(|s| s.to_string())
            })
            .collect::<Result<IndexSet<String>>>()?,
    };
    Ok(Schema::Object {
        properties,
        additional_properties,
        required,
    })
}

fn opt_max<T: PartialOrd>(a: Option<T>, b: Option<T>) -> Option<T> {
    match (a, b) {
        (Some(a), Some(b)) => {
            if a >= b {
                Some(a)
            } else {
                Some(b)
            }
        }
        (Some(a), None) => Some(a),
        (None, Some(b)) => Some(b),
        (None, None) => None,
    }
}

fn opt_min<T: PartialOrd>(a: Option<T>, b: Option<T>) -> Option<T> {
    match (a, b) {
        (Some(a), Some(b)) => {
            if a <= b {
                Some(a)
            } else {
                Some(b)
            }
        }
        (Some(a), None) => Some(a),
        (None, Some(b)) => Some(b),
        (None, None) => None,
    }
}

#[cfg(all(test, feature = "referencing"))]
mod test_retriever {
    use crate::json::{Retrieve, RetrieveWrapper};

    use super::{build_schema, Schema};
    use serde_json::{json, Value};
    use std::{fmt, sync::Arc};

    #[derive(Debug, Clone)]
    struct TestRetrieverError(String);
    impl fmt::Display for TestRetrieverError {
        fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
            write!(f, "Could not retrieve URI: {}", self.0)
        }
    }
    impl std::error::Error for TestRetrieverError {}

    struct TestRetriever {
        schemas: std::collections::HashMap<String, serde_json::Value>,
    }
    impl Retrieve for TestRetriever {
        fn retrieve(&self, uri: &str) -> Result<Value, Box<dyn std::error::Error + Send + Sync>> {
            let key = uri;
            match self.schemas.get(key) {
                Some(schema) => Ok(schema.clone()),
                None => Err(Box::new(TestRetrieverError(key.to_string()))),
            }
        }
    }

    #[test]
    fn test_retriever() {
        let key: &str = "http://example.com/schema";

        let schema = json!({
            "$ref": key
        });
        let retriever = TestRetriever {
            schemas: vec![(
                key.to_string(),
                json!({
                    "type": "string"
                }),
            )]
            .into_iter()
            .collect(),
        };
        let wrapper = RetrieveWrapper::new(Arc::new(retriever));
        let (schema, defs) = build_schema(schema, Some(wrapper)).unwrap();
        match schema {
            Schema::Ref { uri } => {
                assert_eq!(uri, key);
            }
            _ => panic!("Unexpected schema: {:?}", schema),
        }
        assert_eq!(defs.len(), 1);
        let val = defs.get(key).unwrap();
        // poor-man's partial_eq
        match val {
            Schema::String {
                min_length: 0,
                max_length: None,
                regex: None,
            } => {}
            _ => panic!("Unexpected schema: {:?}", val),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn test_problem_child() {
        let schema = json!({
            "allOf" : [
                {"$ref": "#/$defs/tree1"},
                {"$ref": "#/$defs/tree2"}
            ],
            "$defs" : {
                "tree1": {
                    "type": "object",
                    "properties": {
                        "child": {
                            "$ref": "#/$defs/tree1"
                        }
                    }
                },
                "tree2": {
                    "type": "object",
                    "properties": {
                        "child": {
                            "$ref": "#/$defs/tree2"
                        }
                    }
                }
            }
        });
        // Test failure amounts to this resulting in a stack overflow
        let _ = build_schema(schema, None);
    }
}
