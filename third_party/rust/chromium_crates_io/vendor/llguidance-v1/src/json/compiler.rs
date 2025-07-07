use crate::api::LLGuidanceOptions;
use crate::grammar_builder::GrammarResult;
use crate::json::schema::{NumberSchema, StringSchema};
use crate::{regex_to_lark, HashMap};
use anyhow::{anyhow, bail, Context, Result};
use derivre::{JsonQuoteOptions, RegexAst};
use indexmap::{IndexMap, IndexSet};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

use super::numeric::{check_number_bounds, rx_float_range, rx_int_range};
use super::schema::{build_schema, ArraySchema, ObjectSchema, OptSchemaExt, Schema};
use super::shared_context::PatternPropertyCache;
use super::RetrieveWrapper;

use crate::{GrammarBuilder, NodeRef};

// TODO: grammar size limit
// TODO: array maxItems etc limits
// TODO: schemastore/src/schemas/json/BizTalkServerApplicationSchema.json - this breaks 1M fuel on lexer, why?!

#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(default, deny_unknown_fields)]
pub struct JsonCompileOptions {
    pub item_separator: String,
    pub key_separator: String,
    pub whitespace_flexible: bool,
    pub whitespace_pattern: Option<String>,
    pub coerce_one_of: bool,
    pub lenient: bool,
    #[serde(skip)]
    pub retriever: Option<RetrieveWrapper>,
}

fn json_dumps(target: &serde_json::Value) -> String {
    serde_json::to_string(target).unwrap()
}

#[derive(Debug)]
struct UnsatisfiableSchemaError {
    message: String,
}

impl std::fmt::Display for UnsatisfiableSchemaError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Unsatisfiable schema: {}", self.message)
    }
}

const CHAR_REGEX: &str = r#"(\\([\"\\\/bfnrt]|u[a-fA-F0-9]{4})|[^\"\\\x00-\x1F\x7F])"#;

struct Compiler {
    builder: GrammarBuilder,
    options: JsonCompileOptions,
    definitions: HashMap<String, NodeRef>,
    pending_definitions: Vec<(String, NodeRef)>,
    pattern_cache: PatternPropertyCache,

    any_cache: Option<NodeRef>,
    string_cache: Option<NodeRef>,
}

macro_rules! cache {
    ($field:expr, $gen:expr) => {{
        if $field.is_none() {
            $field = Some($gen);
        };
        return ($field).unwrap();
    }};
}

impl Default for JsonCompileOptions {
    fn default() -> Self {
        Self {
            item_separator: ",".to_string(),
            key_separator: ":".to_string(),
            whitespace_pattern: None,
            whitespace_flexible: true,
            coerce_one_of: false,
            lenient: false,
            retriever: None,
        }
    }
}

impl JsonCompileOptions {
    pub fn json_to_llg_with_overrides(
        &self,
        builder: GrammarBuilder,
        mut schema: Value,
    ) -> Result<GrammarResult> {
        if let Some(x_guidance) = schema.get("x-guidance") {
            let opts: Self = serde_json::from_value(x_guidance.clone())?;
            // TODO: figure out why not removing this still causes problems in maskbench
            schema.as_object_mut().unwrap().remove("x-guidance");
            opts.json_to_llg(builder, schema)
        } else {
            self.json_to_llg(builder, schema)
        }
    }

    pub fn json_to_llg(&self, builder: GrammarBuilder, schema: Value) -> Result<GrammarResult> {
        let compiler = Compiler::new(self.clone(), builder);
        #[cfg(feature = "jsonschema_validation")]
        {
            use crate::json_validation::validate_schema;
            validate_schema(&schema)?;
        }

        compiler.execute(schema)
    }

    pub fn json_to_llg_no_validate(
        &self,
        builder: GrammarBuilder,
        schema: Value,
    ) -> Result<GrammarResult> {
        let compiler = Compiler::new(self.clone(), builder);
        compiler.execute(schema)
    }

    pub fn apply_to(&self, schema: &mut Value) {
        schema.as_object_mut().unwrap().insert(
            "x-guidance".to_string(),
            serde_json::to_value(self).unwrap(),
        );
    }
}

impl Compiler {
    pub fn new(options: JsonCompileOptions, builder: GrammarBuilder) -> Self {
        Self {
            builder,
            options,
            definitions: HashMap::default(),
            pending_definitions: vec![],
            any_cache: None,
            string_cache: None,
            pattern_cache: PatternPropertyCache::default(),
        }
    }

    pub fn execute(mut self, schema: Value) -> Result<GrammarResult> {
        let skip = if let Some(pattern) = &self.options.whitespace_pattern {
            RegexAst::Regex(pattern.clone())
        } else if self.options.whitespace_flexible {
            RegexAst::Regex(r"[\x20\x0A\x0D\x09]+".to_string())
        } else {
            RegexAst::NoMatch
        };
        let id = self
            .builder
            .add_grammar(LLGuidanceOptions::default(), skip)?;

        let built = build_schema(schema, &self.options)?;
        self.pattern_cache = built.pattern_cache;

        for w in built.warnings {
            self.builder.add_warning(w);
        }

        let root = self.gen_json(&built.schema)?;
        self.builder.set_start_node(root);

        while let Some((path, pl)) = self.pending_definitions.pop() {
            let schema = built
                .definitions
                .get(&path)
                .ok_or_else(|| anyhow!("Definition not found: {}", path))?;
            let compiled = self.gen_json(schema).map_err(|e| {
                let top_level = anyhow!("{e}\n  while processing {path}");
                e.context(top_level)
            })?;
            self.builder.set_placeholder(pl, compiled);
        }

        Ok(self.builder.finalize(id))
    }

    fn gen_json(&mut self, json_schema: &Schema) -> Result<NodeRef> {
        if let Some(ast) = self.regex_compile(json_schema)? {
            return self.ast_lexeme(ast);
        }
        match json_schema {
            Schema::Any => Ok(self.gen_json_any()),
            Schema::Unsatisfiable(reason) => Err(anyhow!(UnsatisfiableSchemaError {
                message: reason.to_string(),
            })),

            Schema::Array(arr) => self.gen_json_array(arr),
            Schema::Object(obj) => self.gen_json_object(obj),
            Schema::AnyOf(options) => self.process_any_of(options),
            Schema::OneOf(options) => self.process_one_of(options),
            Schema::Ref(uri) => self.get_definition(uri),

            Schema::Null | Schema::Boolean(_) | Schema::String(_) | Schema::Number(_) => {
                unreachable!("should be handled in regex_compile()")
            }
        }
    }

    fn process_one_of(&mut self, options: &[Schema]) -> Result<NodeRef> {
        if self.options.coerce_one_of || self.options.lenient {
            self.builder
                .add_warning("oneOf not fully supported, falling back to anyOf. This may cause validation errors in some cases.".to_string());
            self.process_any_of(options)
        } else {
            Err(anyhow!("oneOf constraints are not supported. Enable 'coerce_one_of' option to approximate oneOf with anyOf"))
        }
    }

    fn process_option(
        &mut self,
        option: &Schema,
        regex_nodes: &mut Vec<RegexAst>,
        cfg_nodes: &mut Vec<NodeRef>,
    ) -> Result<()> {
        match self.regex_compile(option)? {
            Some(c) => regex_nodes.push(c),
            None => cfg_nodes.push(self.gen_json(option)?),
        }
        Ok(())
    }

    fn process_any_of(&mut self, options: &[Schema]) -> Result<NodeRef> {
        let mut regex_nodes = vec![];
        let mut cfg_nodes = vec![];
        let mut errors = vec![];

        for option in options.iter() {
            if let Err(err) = self.process_option(option, &mut regex_nodes, &mut cfg_nodes) {
                match err.downcast_ref::<UnsatisfiableSchemaError>() {
                    Some(_) => errors.push(err),
                    None => return Err(err),
                }
            }
        }

        self.builder.check_limits()?;

        if !regex_nodes.is_empty() {
            let node = RegexAst::Or(regex_nodes);
            let lex = self.ast_lexeme(node)?;
            cfg_nodes.push(lex);
        }

        if !cfg_nodes.is_empty() {
            Ok(self.builder.select(&cfg_nodes))
        } else if let Some(e) = errors.pop() {
            Err(anyhow!(UnsatisfiableSchemaError {
                message: "All options in anyOf are unsatisfiable".to_string(),
            })
            .context(e))
        } else {
            Err(anyhow!(UnsatisfiableSchemaError {
                message: "No options in anyOf".to_string(),
            }))
        }
    }

    fn json_int(&mut self, num: &NumberSchema) -> Result<RegexAst> {
        check_number_bounds(num).map_err(|e| {
            anyhow!(UnsatisfiableSchemaError {
                message: e.to_string(),
            })
        })?;
        let minimum = match num.get_minimum() {
            (Some(min_val), true) => {
                if min_val.fract() != 0.0 {
                    Some(min_val.ceil())
                } else {
                    Some(min_val + 1.0)
                }
            }
            (Some(min_val), false) => Some(min_val.ceil()),
            _ => None,
        }
        .map(|val| val as i64);
        let maximum = match num.get_maximum() {
            (Some(max_val), true) => {
                if max_val.fract() != 0.0 {
                    Some(max_val.floor())
                } else {
                    Some(max_val - 1.0)
                }
            }
            (Some(max_val), false) => Some(max_val.floor()),
            _ => None,
        }
        .map(|val| val as i64);
        let rx = rx_int_range(minimum, maximum).with_context(|| {
            format!("Failed to generate regex for integer range: min={minimum:?}, max={maximum:?}")
        })?;
        let mut ast = RegexAst::Regex(rx);
        if let Some(d) = num.multiple_of.as_ref() {
            ast = RegexAst::And(vec![ast, RegexAst::MultipleOf(d.coef, d.exp)]);
        }
        Ok(ast)
    }

    fn json_number(&mut self, num: &NumberSchema) -> Result<RegexAst> {
        check_number_bounds(num).map_err(|e| {
            anyhow!(UnsatisfiableSchemaError {
                message: e.to_string(),
            })
        })?;
        let (minimum, exclusive_minimum) = num.get_minimum();
        let (maximum, exclusive_maximum) = num.get_maximum();
        let rx = rx_float_range(minimum, maximum, !exclusive_minimum, !exclusive_maximum)
            .with_context(|| {
                format!(
                    "Failed to generate regex for float range: min={minimum:?}, max={maximum:?}"
                )
            })?;
        let mut ast = RegexAst::Regex(rx);
        if let Some(d) = num.multiple_of.as_ref() {
            ast = RegexAst::And(vec![ast, RegexAst::MultipleOf(d.coef, d.exp)]);
        }
        Ok(ast)
    }

    fn ast_lexeme(&mut self, ast: RegexAst) -> Result<NodeRef> {
        let id = self.builder.regex.add_ast(ast)?;
        Ok(self.builder.lexeme(id))
    }

    fn json_simple_string(&mut self) -> NodeRef {
        cache!(self.string_cache, {
            let ast = self.json_quote(RegexAst::Regex("(?s:.*)".to_string()));
            self.ast_lexeme(ast).unwrap()
        })
    }

    fn get_definition(&mut self, reference: &str) -> Result<NodeRef> {
        if let Some(definition) = self.definitions.get(reference) {
            return Ok(*definition);
        }
        let r = self.builder.new_node(reference);
        self.definitions.insert(reference.to_string(), r);
        self.pending_definitions.push((reference.to_string(), r));
        Ok(r)
    }

    fn gen_json_any(&mut self) -> NodeRef {
        cache!(self.any_cache, {
            let json_any = self.builder.new_node("json_any");
            self.any_cache = Some(json_any); // avoid infinite recursion
            let num = self.json_number(&NumberSchema::default()).unwrap();
            let tf = self.builder.regex.regex("true|false").unwrap();
            let options = vec![
                self.builder.string("null"),
                self.builder.lexeme(tf),
                self.ast_lexeme(num).unwrap(),
                self.json_simple_string(),
                self.gen_json_array(&ArraySchema {
                    min_items: 0,
                    max_items: None,
                    prefix_items: vec![],
                    items: Schema::any_box(),
                })
                .unwrap(),
                self.gen_json_object(&ObjectSchema {
                    properties: IndexMap::new(),
                    additional_properties: Schema::any_box(),
                    required: IndexSet::new(),
                    pattern_properties: IndexMap::new(),
                    min_properties: 0,
                    max_properties: None,
                })
                .unwrap(),
            ];
            let inner = self.builder.select(&options);
            self.builder.set_placeholder(json_any, inner);
            json_any
        })
    }

    fn gen_json_object(&mut self, obj: &ObjectSchema) -> Result<NodeRef> {
        let mut taken_names: Vec<String> = vec![];
        let mut unquoted_taken_names: Vec<String> = vec![];
        let mut items: Vec<(NodeRef, bool)> = vec![];

        let colon = self.builder.string(&self.options.key_separator);

        let mut num_required = 0;
        let mut num_optional = 0;

        for name in obj.properties.keys().chain(
            obj.required
                .iter()
                .filter(|n| !obj.properties.contains_key(n.as_str())),
        ) {
            let property_schema = self.pattern_cache.property_schema(obj, name)?;
            let is_required = obj.required.contains(name);
            if !obj.pattern_properties.is_empty() {
                unquoted_taken_names.push(name.to_string());
            }
            // Quote (and escape) the name
            let quoted_name = json_dumps(&json!(name));
            let property = match self.gen_json(property_schema) {
                Ok(node) => node,
                Err(e) => match e.downcast_ref::<UnsatisfiableSchemaError>() {
                    // If it's not an UnsatisfiableSchemaError, just propagate it normally
                    None => return Err(e),
                    // Property is optional; don't raise UnsatisfiableSchemaError but mark name as taken
                    Some(_) if !is_required => {
                        taken_names.push(quoted_name);
                        continue;
                    }
                    // Property is required; add context and propagate UnsatisfiableSchemaError
                    Some(_) => {
                        return Err(e.context(UnsatisfiableSchemaError {
                            message: format!("required property '{name}' is unsatisfiable"),
                        }));
                    }
                },
            };
            let name = self.builder.string(&quoted_name);
            taken_names.push(quoted_name);
            let item = self.builder.join(&[name, colon, property]);
            items.push((item, is_required));
            if is_required {
                num_required += 1;
            } else {
                num_optional += 1;
            }
        }

        let min_properties = obj.min_properties.saturating_sub(num_required);
        let max_properties = obj.max_properties.map(|v| v.saturating_sub(num_required));

        if num_optional > 0 && (min_properties > 0 || max_properties.is_some()) {
            // special case for min/maxProperties == 1
            // this is sometimes used to indicate that at least one property is required
            if min_properties <= 1
                && max_properties.unwrap_or(1) == 1
                && obj.pattern_properties.is_empty()
                && obj
                    .additional_properties
                    .as_ref()
                    .map(|s| s.is_unsat())
                    .unwrap_or(false)
            {
                let mut options: Vec<Vec<(NodeRef, bool)>> = vec![];
                if max_properties == Some(1) {
                    // at most one
                    for idx in 0..items.len() {
                        let (_, required) = items[idx];
                        if !required {
                            options.push(
                                items
                                    .iter()
                                    .enumerate()
                                    .filter_map(|(i2, (n, r))| {
                                        if i2 == idx || *r {
                                            Some((*n, true))
                                        } else {
                                            None
                                        }
                                    })
                                    .collect(),
                            );
                        }
                    }
                    if min_properties == 1 {
                        // exactly one - done
                    } else {
                        // at most one - add empty option
                        assert!(min_properties == 0);
                        options.push(items.into_iter().filter(|(_, r)| *r).collect());
                    }
                } else {
                    assert!(max_properties.is_none());
                    assert!(min_properties == 1);
                    // at least one
                    for idx in 0..items.len() {
                        let (_, required) = items[idx];
                        if !required {
                            options.push(
                                items
                                    .iter()
                                    .enumerate()
                                    .map(|(i2, (n, r))| (*n, *r || i2 == idx))
                                    .collect(),
                            );
                        }
                    }
                }
                let sel_options = options
                    .iter()
                    .map(|v| self.object_fields(v))
                    .collect::<Vec<_>>();
                return Ok(self.builder.select(&sel_options));
            }

            let msg = "min/maxProperties only supported when all keys listed in \"properties\" are required";
            if self.options.lenient {
                self.builder.add_warning(msg.to_string());
            } else {
                bail!(msg);
            }
        }

        let mut taken_name_ids = taken_names
            .iter()
            .map(|n| self.builder.regex.literal(n.to_string()))
            .collect::<Vec<_>>();

        let mut pattern_options = vec![];
        for (pattern, schema) in obj.pattern_properties.iter() {
            let regex = self
                .builder
                .regex
                .add_ast(self.json_quote(RegexAst::SearchRegex(regex_to_lark(pattern, "dw"))))?;
            taken_name_ids.push(regex);

            let schema = match self.gen_json(schema) {
                Ok(r) => r,
                Err(e) => match e.downcast_ref::<UnsatisfiableSchemaError>() {
                    // If it's not an UnsatisfiableSchemaError, just propagate it normally
                    None => return Err(e),
                    // Property is optional; don't raise UnsatisfiableSchemaError but mark name as taken
                    Some(_) => continue,
                },
            };

            let exclude_names = unquoted_taken_names
                .iter()
                .enumerate()
                .filter(|(_, name)| self.pattern_cache.is_match(pattern, name).unwrap_or(true))
                .map(|(idx, _)| taken_name_ids[idx])
                .collect::<Vec<_>>();
            let regex = if exclude_names.is_empty() {
                regex
            } else {
                let options = self.builder.regex.select(exclude_names);
                let not_taken = self.builder.regex.not(options);
                self.builder.regex.and(vec![regex, not_taken])
            };

            let name = self.builder.lexeme(regex);
            pattern_options.push(self.builder.join(&[name, colon, schema]));
        }

        match self.gen_json(obj.additional_properties.schema_ref()) {
            Err(e) => {
                if e.downcast_ref::<UnsatisfiableSchemaError>().is_none() {
                    // Propagate errors that aren't UnsatisfiableSchemaError
                    return Err(e);
                }
                // Ignore UnsatisfiableSchemaError for additionalProperties
            }
            Ok(property) => {
                let name = if taken_name_ids.is_empty() {
                    self.json_simple_string()
                } else {
                    let taken = self.builder.regex.select(taken_name_ids);
                    let not_taken = self.builder.regex.not(taken);
                    let valid = self.builder.regex.regex(&format!("\"({CHAR_REGEX})*\""))?;
                    let valid_and_not_taken = self.builder.regex.and(vec![valid, not_taken]);
                    self.builder.lexeme(valid_and_not_taken)
                };
                let item = self.builder.join(&[name, colon, property]);
                pattern_options.push(item);
            }
        }

        if !pattern_options.is_empty() && max_properties != Some(0) {
            let pattern = self.builder.select(&pattern_options);
            let required = min_properties > 0;
            let seq = self.bounded_sequence(pattern, min_properties, max_properties);
            items.push((seq, required));
        } else if min_properties > 0 {
            return Err(anyhow!(UnsatisfiableSchemaError {
                message: format!(
                    "minProperties ({min_properties}) is greater than number of properties ({num_required})"
                ),
            }));
        }

        Ok(self.object_fields(&items))
    }

    fn object_fields(&mut self, items: &[(NodeRef, bool)]) -> NodeRef {
        let opener = self.builder.string("{");
        let inner = self.ordered_sequence(items, false, &mut HashMap::default());
        let closer = self.builder.string("}");
        self.builder.join(&[opener, inner, closer])
    }

    #[allow(clippy::type_complexity)]
    fn ordered_sequence<'a>(
        &mut self,
        items: &'a [(NodeRef, bool)],
        prefixed: bool,
        cache: &mut HashMap<(&'a [(NodeRef, bool)], bool), NodeRef>,
    ) -> NodeRef {
        // Cache to reduce number of nodes from O(n^2) to O(n)
        if let Some(node) = cache.get(&(items, prefixed)) {
            return *node;
        }
        if items.is_empty() {
            return self.builder.string("");
        }
        let comma = self.builder.string(&self.options.item_separator);
        let (item, required) = items[0];
        let rest = &items[1..];

        let node = match (prefixed, required) {
            (true, true) => {
                // If we know we have preceeding elements, we can safely just add a (',' + e)
                let rest_seq = self.ordered_sequence(rest, true, cache);
                self.builder.join(&[comma, item, rest_seq])
            }
            (true, false) => {
                // If we know we have preceeding elements, we can safely just add an optional(',' + e)
                // TODO optimization: if the rest is all optional, we can nest the rest in the optional
                let comma_item = self.builder.join(&[comma, item]);
                let optional_comma_item = self.builder.optional(comma_item);
                let rest_seq = self.ordered_sequence(rest, true, cache);
                self.builder.join(&[optional_comma_item, rest_seq])
            }
            (false, true) => {
                // No preceding elements, so we just add the element (no comma)
                let rest_seq = self.ordered_sequence(rest, true, cache);
                self.builder.join(&[item, rest_seq])
            }
            (false, false) => {
                // No preceding elements, but our element is optional. If we add the element, the remaining
                // will be prefixed, else they are not.
                // TODO: same nested optimization as above
                let prefixed_rest = self.ordered_sequence(rest, true, cache);
                let unprefixed_rest = self.ordered_sequence(rest, false, cache);
                let opts = [self.builder.join(&[item, prefixed_rest]), unprefixed_rest];
                self.builder.select(&opts)
            }
        };
        cache.insert((items, prefixed), node);
        node
    }

    fn bounded_sequence(
        &mut self,
        item: NodeRef,
        min_elts: usize,
        max_elts: Option<usize>,
    ) -> NodeRef {
        let min_elts = min_elts.saturating_sub(1);
        let max_elts = max_elts.map(|v| v.saturating_sub(1));
        let comma = self.builder.string(&self.options.item_separator);
        let item_comma = self.builder.join(&[item, comma]);
        let item_comma_rep = self.builder.repeat(item_comma, min_elts, max_elts);
        self.builder.join(&[item_comma_rep, item])
    }

    fn sequence(&mut self, item: NodeRef) -> NodeRef {
        let comma = self.builder.string(&self.options.item_separator);
        let item_comma = self.builder.join(&[item, comma]);
        let item_comma_star = self.builder.zero_or_more(item_comma);
        self.builder.join(&[item_comma_star, item])
    }

    fn json_quote(&self, ast: RegexAst) -> RegexAst {
        RegexAst::JsonQuote(
            Box::new(ast),
            JsonQuoteOptions {
                allowed_escapes: "nrbtf\\\"u".to_string(),
                raw_mode: false,
            },
        )
    }

    fn regex_compile(&mut self, schema: &Schema) -> Result<Option<RegexAst>> {
        fn literal_regex(rx: &str) -> Option<RegexAst> {
            Some(RegexAst::Literal(rx.to_string()))
        }

        self.builder.check_limits()?;

        let r = match schema {
            Schema::Null => literal_regex("null"),
            Schema::Boolean(None) => Some(RegexAst::Regex("true|false".to_string())),
            Schema::Boolean(Some(value)) => literal_regex(if *value { "true" } else { "false" }),

            Schema::Number(num) => Some(if num.integer {
                self.json_int(num)?
            } else {
                self.json_number(num)?
            }),

            Schema::String(opts) => return self.gen_json_string(opts.clone()).map(Some),

            Schema::Any
            | Schema::Unsatisfiable(_)
            | Schema::Array(_)
            | Schema::Object(_)
            | Schema::AnyOf(_)
            | Schema::OneOf(_)
            | Schema::Ref(_) => None,
        };
        Ok(r)
    }

    fn gen_json_string(&self, opts: StringSchema) -> Result<RegexAst> {
        let min_length = opts.min_length;
        let max_length = opts.max_length;
        if let Some(max_length) = max_length {
            if min_length > max_length {
                return Err(anyhow!(UnsatisfiableSchemaError {
                    message: format!(
                        "minLength ({min_length}) is greater than maxLength ({max_length})"
                    ),
                }));
            }
        }
        if min_length == 0 && max_length.is_none() && opts.regex.is_none() {
            return Ok(self.json_quote(RegexAst::Regex("(?s:.*)".to_string())));
        }
        if let Some(mut ast) = opts.regex {
            let mut positive = false;

            fn mk_rx_repr(ast: &RegexAst) -> String {
                let mut rx_repr = String::new();
                ast.write_to_str(&mut rx_repr, 1_000, None);
                rx_repr
            }

            // special-case literals - the length is easy to check
            if let RegexAst::Literal(s) = &ast {
                let l = s.chars().count();

                if l < min_length || l > max_length.unwrap_or(usize::MAX) {
                    return Err(anyhow!(UnsatisfiableSchemaError {
                        message: format!("Constant {s:?} doesn't match length constraints")
                    }));
                }

                positive = true;
            } else if min_length != 0 || max_length.is_some() {
                ast = RegexAst::And(vec![
                    ast,
                    RegexAst::Regex(format!(
                        "(?s:.{{{},{}}})",
                        min_length,
                        max_length.map_or("".to_string(), |v| v.to_string())
                    )),
                ]);
            } else {
                positive = always_non_empty(&ast);
                // eprintln!("positive:{} {}", positive, mk_rx_repr(&ast));
            }

            if !positive {
                // Check if the regex is empty
                let mut builder = derivre::RegexBuilder::new();
                let expr = builder.mk(&ast)?;
                // if regex is not positive, do the more expensive non-emptiness check
                if !builder.exprset().is_positive(expr) {
                    // in JSB, 13 cases above 2000;
                    // 1 case above 5000:
                    // "format": "email",
                    // "pattern": "^\\w+([\\.-]?\\w+)*@\\w+([\\.-]?\\w+)*(\\.\\w{2,})+$",
                    // "minLength": 6,
                    //
                    // (excluding two handwritten examples with minLength:10000)
                    let mut regex = builder.to_regex_limited(expr, 10_000).map_err(|_| {
                        anyhow!(
                            "Unable to determine if regex is empty: {}",
                            mk_rx_repr(&ast)
                        )
                    })?;
                    if regex.always_empty() {
                        return Err(anyhow!(UnsatisfiableSchemaError {
                            message: format!("Regex is empty: {}", mk_rx_repr(&ast))
                        }));
                    }
                }
            }

            ast = self.json_quote(ast);

            Ok(ast)
        } else {
            Ok(self.json_quote(RegexAst::Regex(format!(
                "(?s:.{{{},{}}})",
                min_length,
                max_length.map_or("".to_string(), |v| v.to_string())
            ))))
        }
    }

    fn gen_json_array(&mut self, arr: &ArraySchema) -> Result<NodeRef> {
        let mut max_items = arr.max_items;
        let min_items = arr.min_items;

        if let Some(max_items) = max_items {
            if min_items > max_items {
                return Err(anyhow!(UnsatisfiableSchemaError {
                    message: format!(
                        "minItems ({min_items}) is greater than maxItems ({max_items})"
                    ),
                }));
            }
        }

        let additional_item_grm = match self.gen_json(arr.items.schema_ref()) {
            Ok(node) => Some(node),
            Err(e) => match e.downcast_ref::<UnsatisfiableSchemaError>() {
                // If it's not an UnsatisfiableSchemaError, just propagate it normally
                None => return Err(e),
                // Item is optional; don't raise UnsatisfiableSchemaError
                Some(_) if arr.prefix_items.len() >= min_items => None,
                // Item is required; add context and propagate UnsatisfiableSchemaError
                Some(_) => {
                    return Err(e.context(UnsatisfiableSchemaError {
                        message: "required item is unsatisfiable".to_string(),
                    }));
                }
            },
        };

        let mut required_items = vec![];
        let mut optional_items = vec![];

        // If max_items is None, we can add an infinite tail of items later
        let n_to_add = max_items.map_or(arr.prefix_items.len().max(min_items), |max| max);

        for i in 0..n_to_add {
            let item = if i < arr.prefix_items.len() {
                match self.gen_json(&arr.prefix_items[i]) {
                    Ok(node) => node,
                    Err(e) => match e.downcast_ref::<UnsatisfiableSchemaError>() {
                        // If it's not an UnsatisfiableSchemaError, just propagate it normally
                        None => return Err(e),
                        // Item is optional; don't raise UnsatisfiableSchemaError.
                        // Set max_items to the current index, as we can't satisfy any more items.
                        Some(_) if i >= min_items => {
                            max_items = Some(i);
                            break;
                        }
                        // Item is required; add context and propagate UnsatisfiableSchemaError
                        Some(_) => {
                            return Err(e.context(UnsatisfiableSchemaError {
                                message: format!(
                                    "prefixItems[{i}] is unsatisfiable but minItems is {min_items}"
                                ),
                            }));
                        }
                    },
                }
            } else if let Some(compiled) = &additional_item_grm {
                *compiled
            } else {
                break;
            };

            if i < min_items {
                required_items.push(item);
            } else {
                optional_items.push(item);
            }
        }

        if max_items.is_none() {
            if let Some(additional_item) = additional_item_grm {
                // Add an infinite tail of items
                optional_items.push(self.sequence(additional_item));
            }
        }

        let mut grammars: Vec<NodeRef> = vec![self.builder.string("[")];
        let comma = self.builder.string(&self.options.item_separator);

        if !required_items.is_empty() {
            grammars.push(required_items[0]);
            for item in &required_items[1..] {
                grammars.push(comma);
                grammars.push(*item);
            }
        }

        if !optional_items.is_empty() {
            let first = optional_items[0];
            let tail =
                optional_items
                    .into_iter()
                    .skip(1)
                    .rev()
                    .fold(self.builder.empty(), |acc, item| {
                        let j = self.builder.join(&[comma, item, acc]);
                        self.builder.optional(j)
                    });
            let tail = self.builder.join(&[first, tail]);

            if !required_items.is_empty() {
                let j = self.builder.join(&[comma, tail]);
                grammars.push(self.builder.optional(j));
            } else {
                grammars.push(self.builder.optional(tail));
            }
        }

        grammars.push(self.builder.string("]"));
        Ok(self.builder.join(&grammars))
    }
}

fn always_non_empty(ast: &RegexAst) -> bool {
    match ast {
        RegexAst::Or(asts) => asts.iter().any(always_non_empty),
        RegexAst::Concat(asts) => asts.iter().all(always_non_empty),
        RegexAst::Repeat(ast, _, _) | RegexAst::JsonQuote(ast, _) | RegexAst::LookAhead(ast) => {
            always_non_empty(ast)
        }

        RegexAst::EmptyString
        | RegexAst::Literal(_)
        | RegexAst::ByteLiteral(_)
        | RegexAst::Byte(_)
        | RegexAst::ByteSet(_)
        | RegexAst::MultipleOf(_, _) => true,

        RegexAst::And(_)
        | RegexAst::Not(_)
        | RegexAst::NoMatch
        | RegexAst::Regex(_)
        | RegexAst::SearchRegex(_)
        | RegexAst::ExprRef(_) => false,
    }
}
