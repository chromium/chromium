use crate::{regex_to_lark, HashMap, JsonCompileOptions};
use anyhow::{anyhow, bail, ensure, Result};
use derivre::RegexAst;
use indexmap::{IndexMap, IndexSet};
use serde_json::Value;
use std::mem;

use super::context::{Context, Draft, PreContext, ResourceRef};
use super::formats::lookup_format;
use super::numeric::Decimal;
use super::shared_context::BuiltSchema;

const TYPES: [&str; 6] = ["null", "boolean", "number", "string", "array", "object"];

// Keywords that are implemented in this module
pub(crate) const IMPLEMENTED: [&str; 27] = [
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
    "patternProperties",
    "required",
    "minProperties",
    "maxProperties",
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
    Unsatisfiable(String),
    Null,
    Number(NumberSchema),
    String(StringSchema),
    Array(ArraySchema),
    Object(ObjectSchema),
    Boolean(Option<bool>),
    AnyOf(Vec<Schema>),
    OneOf(Vec<Schema>),
    Ref(String),
}

#[derive(Debug, Clone, Default)]
pub struct NumberSchema {
    pub minimum: Option<f64>,
    pub maximum: Option<f64>,
    pub exclusive_minimum: Option<f64>,
    pub exclusive_maximum: Option<f64>,
    pub integer: bool,
    pub multiple_of: Option<Decimal>,
}

impl NumberSchema {
    pub fn get_minimum(&self) -> (Option<f64>, bool) {
        match (self.minimum, self.exclusive_minimum) {
            (Some(min), Some(xmin)) => {
                if xmin >= min {
                    (Some(xmin), true)
                } else {
                    (Some(min), false)
                }
            }
            (Some(min), None) => (Some(min), false),
            (None, Some(xmin)) => (Some(xmin), true),
            (None, None) => (None, false),
        }
    }

    pub fn get_maximum(&self) -> (Option<f64>, bool) {
        match (self.maximum, self.exclusive_maximum) {
            (Some(max), Some(xmax)) => {
                if xmax <= max {
                    (Some(xmax), true)
                } else {
                    (Some(max), false)
                }
            }
            (Some(max), None) => (Some(max), false),
            (None, Some(xmax)) => (Some(xmax), true),
            (None, None) => (None, false),
        }
    }
}

#[derive(Debug, Clone)]
pub struct StringSchema {
    pub min_length: usize,
    pub max_length: Option<usize>,
    pub regex: Option<RegexAst>,
}

#[derive(Debug, Clone)]
pub struct ArraySchema {
    pub min_items: usize,
    pub max_items: Option<usize>,
    pub prefix_items: Vec<Schema>,
    pub items: Option<Box<Schema>>,
}

#[derive(Debug, Clone)]
pub struct ObjectSchema {
    pub properties: IndexMap<String, Schema>,
    pub pattern_properties: IndexMap<String, Schema>,
    pub additional_properties: Option<Box<Schema>>,
    pub required: IndexSet<String>,
    pub min_properties: usize,
    pub max_properties: Option<usize>,
}

pub trait OptSchemaExt {
    fn schema(&self) -> Schema;
    fn schema_ref(&self) -> &Schema;
}

impl OptSchemaExt for Option<Box<Schema>> {
    fn schema(&self) -> Schema {
        match self {
            Some(schema) => schema.as_ref().clone(),
            None => Schema::Any,
        }
    }

    fn schema_ref(&self) -> &Schema {
        match self {
            Some(schema) => schema.as_ref(),
            None => &Schema::Any,
        }
    }
}

impl Schema {
    pub fn unsat(reason: &str) -> Schema {
        Schema::Unsatisfiable(reason.to_string())
    }

    pub fn false_schema() -> Schema {
        Self::unsat("schema is false")
    }

    pub fn any_box() -> Option<Box<Schema>> {
        Some(Box::new(Schema::Any))
    }

    pub fn is_unsat(&self) -> bool {
        matches!(self, Schema::Unsatisfiable(_))
    }

    /// Shallowly normalize the schema, removing any unnecessary nesting or empty options.
    fn normalize(self, ctx: &Context) -> Schema {
        match self {
            Schema::AnyOf(options) => {
                let mut unsats = Vec::new();
                let mut valid = Vec::new();
                for option in options.into_iter() {
                    match option {
                        Schema::Any => {
                            return Schema::Any;
                        }
                        Schema::Unsatisfiable(reason) => unsats.push(Schema::Unsatisfiable(reason)),
                        Schema::AnyOf(nested) => valid.extend(nested),
                        other => valid.push(other),
                    }
                }
                if valid.is_empty() {
                    // Return the first unsatisfiable schema for debug-ability
                    if let Some(unsat) = unsats.into_iter().next() {
                        return unsat;
                    }
                    // We must not have had any schemas to begin with
                    return Schema::unsat("anyOf is empty");
                }
                if valid.len() == 1 {
                    // Unwrap singleton
                    return valid.swap_remove(0);
                }
                Schema::AnyOf(valid)
            }
            Schema::OneOf(options) => {
                let mut unsats = Vec::new();
                let mut valid = Vec::new();
                for option in options.into_iter() {
                    match option {
                        Schema::Unsatisfiable(reason) => unsats.push(Schema::Unsatisfiable(reason)),
                        // Flatten nested oneOfs: (A⊕B)⊕(C⊕D) = A⊕B⊕C⊕D
                        Schema::OneOf(nested) => valid.extend(nested),
                        other => valid.push(other),
                    }
                }
                if valid.is_empty() {
                    // Return the first unsatisfiable schema for debug-ability
                    if let Some(unsat) = unsats.into_iter().next() {
                        return unsat;
                    }
                    // We must not have had any schemas to begin with
                    return Schema::unsat("oneOf is empty");
                }
                if valid.len() == 1 {
                    // Unwrap singleton
                    return valid.swap_remove(0);
                }
                if valid.iter().enumerate().all(|(i, x)| {
                    valid
                        .iter()
                        .skip(i + 1) // "upper diagonal"
                        .all(|y| x.is_verifiably_disjoint_from(y, ctx))
                }) {
                    Schema::AnyOf(valid)
                } else {
                    Schema::OneOf(valid)
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
            (Schema::Unsatisfiable(reason), _) => Schema::Unsatisfiable(reason),
            (_, Schema::Unsatisfiable(reason)) => Schema::Unsatisfiable(reason),
            (Schema::Ref(uri), schema1) => {
                intersect_ref(ctx, &uri, schema1, true, stack_level + 1)?
            }
            (schema0, Schema::Ref(uri)) => {
                intersect_ref(ctx, &uri, schema0, false, stack_level + 1)?
            }
            (Schema::OneOf(options), schema1) => Schema::OneOf(
                options
                    .into_iter()
                    .map(|opt| opt.intersect(schema1.clone(), ctx, stack_level + 1))
                    .collect::<Result<Vec<_>>>()?,
            ),

            (schema0, Schema::OneOf(options)) => Schema::OneOf(
                options
                    .into_iter()
                    .map(|opt| schema0.clone().intersect(opt, ctx, stack_level + 1))
                    .collect::<Result<Vec<_>>>()?,
            ),
            (Schema::AnyOf(options), schema1) => Schema::AnyOf(
                options
                    .into_iter()
                    .map(|opt| opt.intersect(schema1.clone(), ctx, stack_level + 1))
                    .collect::<Result<Vec<_>>>()?,
            ),
            (schema0, Schema::AnyOf(options)) => Schema::AnyOf(
                options
                    .into_iter()
                    .map(|opt| schema0.clone().intersect(opt, ctx, stack_level + 1))
                    .collect::<Result<Vec<_>>>()?,
            ),
            (Schema::Null, Schema::Null) => Schema::Null,
            (Schema::Boolean(value1), Schema::Boolean(value2)) => {
                if value1 == value2 || value2.is_none() {
                    Schema::Boolean(value1)
                } else if value1.is_none() {
                    Schema::Boolean(value2)
                } else {
                    Schema::unsat("incompatible boolean values")
                }
            }

            (Schema::Number(n1), Schema::Number(n2)) => Schema::Number(NumberSchema {
                minimum: opt_max(n1.minimum, n2.minimum),
                maximum: opt_min(n1.maximum, n2.maximum),
                exclusive_minimum: opt_max(n1.exclusive_minimum, n2.exclusive_minimum),
                exclusive_maximum: opt_min(n1.exclusive_maximum, n2.exclusive_maximum),
                integer: n1.integer || n2.integer,
                multiple_of: match (n1.multiple_of, n2.multiple_of) {
                    (None, None) => None,
                    (None, Some(m)) | (Some(m), None) => Some(m),
                    (Some(m1), Some(m2)) => Some(m1.lcm(&m2)),
                },
            }),

            (Schema::String(s1), Schema::String(s2)) => Schema::String(StringSchema {
                min_length: s1.min_length.max(s2.min_length),
                max_length: opt_min(s1.max_length, s2.max_length),
                regex: match (s1.regex, s2.regex) {
                    (None, None) => None,
                    (None, Some(r)) | (Some(r), None) => Some(r),
                    (Some(r1), Some(r2)) => Some(RegexAst::And(vec![r1, r2])),
                },
            }),

            (Schema::Array(mut a1), Schema::Array(mut a2)) => Schema::Array(ArraySchema {
                min_items: a1.min_items.max(a2.min_items),
                max_items: opt_min(a1.max_items, a2.max_items),
                prefix_items: {
                    let len = a1.prefix_items.len().max(a2.prefix_items.len());
                    a1.prefix_items.resize_with(len, || a1.items.schema());
                    a2.prefix_items.resize_with(len, || a2.items.schema());
                    a1.prefix_items
                        .into_iter()
                        .zip(a2.prefix_items.into_iter())
                        .map(|(item1, item2)| item1.intersect(item2, ctx, stack_level + 1))
                        .collect::<Result<Vec<_>>>()?
                },
                items: match (a1.items, a2.items) {
                    (None, None) => None,
                    (None, Some(item)) | (Some(item), None) => Some(item),
                    (Some(item1), Some(item2)) => {
                        Some(Box::new(item1.intersect(*item2, ctx, stack_level + 1)?))
                    }
                },
            }),

            (Schema::Object(mut o1), Schema::Object(mut o2)) => {
                let mut properties = IndexMap::new();
                for (key, prop1) in std::mem::take(&mut o1.properties).into_iter() {
                    let prop2 = ctx.property_schema(&o2, &key)?;
                    properties.insert(key, prop1.intersect(prop2.clone(), ctx, stack_level + 1)?);
                }
                for (key, prop2) in o2.properties.into_iter() {
                    if properties.contains_key(&key) {
                        continue;
                    }
                    let prop1 = ctx.property_schema(&o1, &key)?;
                    properties.insert(key, prop1.clone().intersect(prop2, ctx, stack_level + 1)?);
                }
                let mut required = o1.required;
                required.extend(o2.required);

                let mut pattern_properties = IndexMap::new();
                for (key, prop1) in o1.pattern_properties.into_iter() {
                    if let Some(prop2) = o2.pattern_properties.get_mut(&key) {
                        let prop2 = std::mem::replace(prop2, Schema::Null);
                        pattern_properties.insert(
                            key.clone(),
                            prop1.intersect(prop2.clone(), ctx, stack_level + 1)?,
                        );
                    } else {
                        pattern_properties.insert(key.clone(), prop1);
                    }
                }
                for (key, prop2) in o2.pattern_properties.into_iter() {
                    if pattern_properties.contains_key(&key) {
                        continue;
                    }
                    pattern_properties.insert(key.clone(), prop2);
                }

                let keys = pattern_properties.keys().collect::<Vec<_>>();
                if !keys.is_empty() {
                    ctx.check_disjoint_pattern_properties(&keys)?;
                }

                let additional_properties =
                    match (o1.additional_properties, o2.additional_properties) {
                        (None, None) => None,
                        (None, Some(p)) | (Some(p), None) => Some(p),
                        (Some(p1), Some(p2)) => {
                            Some(Box::new((*p1).intersect(*p2, ctx, stack_level + 1)?))
                        }
                    };

                let min_properties = o1.min_properties.max(o2.min_properties);
                let max_properties = opt_min(o1.max_properties, o2.max_properties);

                mk_object_schema(ObjectSchema {
                    properties,
                    pattern_properties,
                    additional_properties,
                    required,
                    min_properties,
                    max_properties,
                })
            }

            //TODO: get types for error message
            _ => Schema::unsat("incompatible types"),
        };
        Ok(merged.normalize(ctx))
    }

    fn is_verifiably_disjoint_from(&self, other: &Schema, ctx: &Context) -> bool {
        match (self, other) {
            (Schema::Unsatisfiable(_), _) => true,
            (_, Schema::Unsatisfiable(_)) => true,
            (Schema::Any, _) => false,
            (_, Schema::Any) => false,
            (Schema::Ref(_), _) => false, // TODO: could resolve
            (_, Schema::Ref(_)) => false, // TODO: could resolve
            (Schema::Boolean(value1), Schema::Boolean(value2)) => {
                value1.is_some() && value2.is_some() && value1 != value2
            }
            (Schema::AnyOf(options), _) => options
                .iter()
                .all(|opt| opt.is_verifiably_disjoint_from(other, ctx)),
            (_, Schema::AnyOf(options)) => options
                .iter()
                .all(|opt| self.is_verifiably_disjoint_from(opt, ctx)),
            (Schema::OneOf(options), _) => options
                .iter()
                .all(|opt| opt.is_verifiably_disjoint_from(other, ctx)),
            (_, Schema::OneOf(options)) => options
                .iter()
                .all(|opt| self.is_verifiably_disjoint_from(opt, ctx)),
            // TODO: could actually compile the regexes and check for overlap
            (
                Schema::String(StringSchema {
                    regex: Some(RegexAst::Literal(lit1)),
                    ..
                }),
                Schema::String(StringSchema {
                    regex: Some(RegexAst::Literal(lit2)),
                    ..
                }),
            ) => lit1 != lit2,
            (Schema::Object(o1), Schema::Object(o2)) => {
                o1.required.union(&o2.required).any(|key| {
                    let prop1 = ctx.property_schema(o1, key).unwrap_or(&Schema::Any);
                    let prop2 = ctx.property_schema(o2, key).unwrap_or(&Schema::Any);
                    prop1.is_verifiably_disjoint_from(prop2, ctx)
                })
            }
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
                result = result.intersect(Schema::AnyOf(options), ctx, 0)?;
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
                result = result.intersect(Schema::AnyOf(options), ctx, 0)?;
            }
            "oneOf" => {
                let one_of = v
                    .as_array()
                    .ok_or_else(|| anyhow!("oneOf must be an array"))?;
                let options = one_of
                    .iter()
                    .map(|value| compile_resource(ctx, ctx.as_resource_ref(value)))
                    .collect::<Result<Vec<_>>>()?;
                result = result.intersect(Schema::OneOf(options), ctx, 0)?;
            }
            "$ref" => {
                let reference = v
                    .as_str()
                    .ok_or_else(|| anyhow!("$ref must be a string, got {}", limited_str(v)))?
                    .to_string();
                let uri: String = ctx.normalize_ref(&reference)?;
                if matches!(result, Schema::Any) {
                    define_ref(ctx, &uri)?;
                    result = Schema::Ref(uri);
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
    pub lenient: bool,
}

impl Default for SchemaBuilderOptions {
    fn default() -> Self {
        SchemaBuilderOptions {
            max_size: 50_000,
            max_stack_level: 128, // consumes ~2.5k of stack per level
            lenient: false,
        }
    }
}

pub fn build_schema(contents: Value, options: &JsonCompileOptions) -> Result<BuiltSchema> {
    if let Some(b) = contents.as_bool() {
        let s = if b {
            Schema::Any
        } else {
            Schema::false_schema()
        };
        return Ok(BuiltSchema::simple(s));
    }

    let pre_ctx = PreContext::new(contents, options.retriever.clone())?;
    let mut ctx = Context::new(&pre_ctx)?;

    ctx.options.lenient = options.lenient;

    let root_resource = ctx.lookup_resource(&pre_ctx.base_uri)?;
    let schema = compile_resource(&ctx, root_resource)?;
    Ok(ctx.into_result(schema))
}

fn compile_resource(ctx: &Context, resource: ResourceRef) -> Result<Schema> {
    let ctx = ctx.in_subresource(resource)?;
    compile_contents(&ctx, resource.contents())
}

fn compile_contents(ctx: &Context, contents: &Value) -> Result<Schema> {
    compile_contents_inner(ctx, contents).map(|schema| schema.normalize(ctx))
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
        let msg = format!("Unimplemented keys: {:?}", unimplemented_keys);
        if ctx.options.lenient {
            ctx.record_warning(msg);
        } else {
            bail!(msg);
        }
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
        Value::Bool(b) => Ok(Schema::Boolean(Some(*b))),
        Value::Number(n) => {
            let value = n.as_f64().ok_or_else(|| {
                anyhow!(
                    "Expected f64 for numeric const, got {}",
                    limited_str(instance)
                )
            })?;
            Ok(Schema::Number(NumberSchema {
                minimum: Some(value),
                maximum: Some(value),
                exclusive_minimum: None,
                exclusive_maximum: None,
                integer: n.is_i64(),
                multiple_of: None,
            }))
        }
        Value::String(s) => Ok(Schema::String(StringSchema {
            min_length: 0,
            max_length: None,
            regex: Some(RegexAst::Literal(s.to_string())),
        })),
        Value::Array(items) => {
            let prefix_items = items
                .iter()
                .map(compile_const)
                .collect::<Result<Vec<Schema>>>()?;
            Ok(Schema::Array(ArraySchema {
                min_items: prefix_items.len(),
                max_items: Some(prefix_items.len()),
                prefix_items,
                items: Some(Box::new(Schema::false_schema())),
            }))
        }
        Value::Object(mapping) => {
            let properties = mapping
                .iter()
                .map(|(k, v)| Ok((k.clone(), compile_const(v)?)))
                .collect::<Result<IndexMap<String, Schema>>>()?;
            let required = properties.keys().cloned().collect();
            Ok(Schema::Object(ObjectSchema {
                properties,
                pattern_properties: IndexMap::default(),
                additional_properties: Some(Box::new(Schema::false_schema())),
                required,
                min_properties: 0,
                max_properties: None,
            }))
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
        Ok(Schema::AnyOf(options))
    }
}

fn compile_type(ctx: &Context, tp: &str, schema: &HashMap<&str, &Value>) -> Result<Schema> {
    ctx.increment()?;

    match tp {
        "null" => Ok(Schema::Null),
        "boolean" => Ok(Schema::Boolean(None)),
        "number" | "integer" => compile_numeric(schema, tp == "integer"),
        "string" => compile_string(ctx, schema),
        "array" => compile_array(ctx, schema),
        "object" => compile_object(ctx, schema),
        _ => bail!("Invalid type: {}", tp),
    }
}

fn compile_numeric(schema: &HashMap<&str, &Value>, integer: bool) -> Result<Schema> {
    let minimum = schema.get("minimum").copied();
    let maximum = schema.get("maximum").copied();
    let exclusive_minimum = schema.get("exclusiveMinimum").copied();
    let exclusive_maximum = schema.get("exclusiveMaximum").copied();
    let multiple_of = schema.get("multipleOf").copied();

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
    Ok(Schema::Number(NumberSchema {
        minimum,
        maximum,
        exclusive_minimum,
        exclusive_maximum,
        integer,
        multiple_of,
    }))
}

fn compile_string(ctx: &Context, schema: &HashMap<&str, &Value>) -> Result<Schema> {
    let pattern = schema.get("pattern").copied();
    let format = schema.get("format").copied();

    let min_length = get_usize(schema, "minLength")?.unwrap_or(0);
    let max_length = get_usize(schema, "maxLength")?;

    let pattern_rx = match pattern {
        None => None,
        Some(val) => Some({
            let s = val
                .as_str()
                .ok_or_else(|| anyhow!("Expected string for 'pattern', got {}", limited_str(val)))?
                .to_string();
            RegexAst::SearchRegex(regex_to_lark(&s, "dw"))
        }),
    };
    let format_rx = match format {
        None => None,
        Some(val) => {
            let key = val
                .as_str()
                .ok_or_else(|| anyhow!("Expected string for 'format', got {}", limited_str(val)))?
                .to_string();

            if let Some(fmt) = lookup_format(&key) {
                Some(RegexAst::Regex(fmt.to_string()))
            } else {
                let msg = format!("Unknown format: {}", key);
                if ctx.options.lenient {
                    ctx.record_warning(msg);
                    None
                } else {
                    bail!(msg);
                }
            }
        }
    };
    let regex = match (pattern_rx, format_rx) {
        (None, None) => None,
        (None, Some(fmt)) => Some(fmt),
        (Some(pat), None) => Some(pat),
        (Some(pat), Some(fmt)) => Some(RegexAst::And(vec![pat, fmt])),
    };
    Ok(Schema::String(StringSchema {
        min_length,
        max_length,
        regex,
    }))
}

fn compile_array(ctx: &Context, schema: &HashMap<&str, &Value>) -> Result<Schema> {
    let min_items = get_usize(schema, "minItems")?.unwrap_or(0);
    let max_items = get_usize(schema, "maxItems")?;
    let prefix_items = schema.get("prefixItems").copied();
    let items = schema.get("items").copied();
    let additional_items = schema.get("additionalItems").copied();

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
    Ok(Schema::Array(ArraySchema {
        min_items,
        max_items,
        prefix_items,
        items,
    }))
}

fn compile_prop_map(
    ctx: &Context,
    lbl: &str,
    prop_map: Option<&Value>,
) -> Result<IndexMap<String, Schema>> {
    match prop_map {
        None => Ok(IndexMap::new()),
        Some(val) => val
            .as_object()
            .ok_or_else(|| anyhow!("Expected object for '{lbl}', got {}", limited_str(val)))?
            .iter()
            .map(|(k, v)| compile_resource(ctx, ctx.as_resource_ref(v)).map(|v| (k.clone(), v)))
            .collect(),
    }
}

fn get_usize(schema: &HashMap<&str, &Value>, name: &str) -> Result<Option<usize>> {
    if let Some(val) = schema.get(name) {
        if let Some(val) = val.as_u64() {
            ensure!(
                val <= usize::MAX as u64,
                "Value {val} for '{name}' is too large"
            );
            Ok(Some(val as usize))
        } else {
            bail!(
                "Expected positive integer for '{name}', got {}",
                limited_str(val)
            )
        }
    } else {
        Ok(None)
    }
}

fn compile_object(ctx: &Context, schema: &HashMap<&str, &Value>) -> Result<Schema> {
    let properties = schema.get("properties").copied();
    let pattern_properties = schema.get("patternProperties").copied();
    let additional_properties = schema.get("additionalProperties").copied();
    let required = schema.get("required").copied();
    let min_properties = get_usize(schema, "minProperties")?.unwrap_or(0);
    let max_properties = get_usize(schema, "maxProperties")?;

    let properties = compile_prop_map(ctx, "properties", properties)?;
    let pattern_properties = compile_prop_map(ctx, "patternProperties", pattern_properties)?;
    ctx.check_disjoint_pattern_properties(&pattern_properties.keys().collect::<Vec<_>>())?;
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

    Ok(mk_object_schema(ObjectSchema {
        properties,
        pattern_properties,
        additional_properties,
        required,
        min_properties,
        max_properties,
    }))
}

fn mk_object_schema(obj: ObjectSchema) -> Schema {
    if let Some(max) = obj.max_properties {
        if obj.min_properties > max {
            return Schema::unsat("minProperties > maxProperties");
        }
    }
    if obj.required.len() > obj.max_properties.unwrap_or(usize::MAX) {
        return Schema::unsat("required > maxProperties");
    }

    Schema::Object(obj)
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
    use crate::JsonCompileOptions;

    use super::{build_schema, Schema, StringSchema};
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
        let options = JsonCompileOptions {
            retriever: Some(wrapper.clone()),
            ..Default::default()
        };
        let r = build_schema(schema, &options).unwrap();
        let schema = r.schema;
        let defs = r.definitions;
        match schema {
            Schema::Ref(uri) => {
                assert_eq!(uri, key);
            }
            _ => panic!("Unexpected schema: {:?}", schema),
        }
        assert_eq!(defs.len(), 1);
        let val = defs.get(key).unwrap();
        // poor-man's partial_eq
        match val {
            Schema::String(StringSchema {
                min_length: 0,
                max_length: None,
                regex: None,
            }) => {}
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
        let options = JsonCompileOptions::default();
        let _ = build_schema(schema, &options);
    }
}
