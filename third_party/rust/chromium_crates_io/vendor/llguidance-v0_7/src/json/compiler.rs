use crate::api::LLGuidanceOptions;
use crate::grammar_builder::GrammarResult;
use crate::HashMap;
use anyhow::{anyhow, Context, Result};
use derivre::{JsonQuoteOptions, RegexAst};
use indexmap::IndexMap;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

use super::numeric::{check_number_bounds, rx_float_range, rx_int_range, Decimal};
use super::schema::{build_schema, Schema};
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
            retriever: None,
        }
    }
}

impl JsonCompileOptions {
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

        let (compiled_schema, definitions) = build_schema(schema, self.options.retriever.clone())?;

        let root = self.gen_json(&compiled_schema)?;
        self.builder.set_start_node(root);

        while let Some((path, pl)) = self.pending_definitions.pop() {
            let schema = definitions
                .get(&path)
                .ok_or_else(|| anyhow!("Definition not found: {}", path))?;
            let compiled = self.gen_json(schema)?;
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
            Schema::Unsatisfiable { reason } => Err(anyhow!(UnsatisfiableSchemaError {
                message: reason.to_string(),
            })),

            Schema::Array {
                min_items,
                max_items,
                prefix_items,
                items,
            } => self.gen_json_array(
                prefix_items,
                items.as_deref().unwrap_or(&Schema::Any),
                *min_items,
                *max_items,
            ),
            Schema::Object {
                properties,
                additional_properties,
                required,
            } => self.gen_json_object(
                properties,
                additional_properties.as_deref().unwrap_or(&Schema::Any),
                required.iter().cloned().collect(),
            ),

            Schema::AnyOf { options } => self.process_any_of(options),
            Schema::OneOf { options } => self.process_one_of(options),
            Schema::Ref { uri, .. } => self.get_definition(uri),

            Schema::Null
            | Schema::Boolean
            | Schema::LiteralBool { .. }
            | Schema::String { .. }
            | Schema::Number { .. } => {
                unreachable!("should be handled in regex_compile()")
            }
        }
    }

    fn process_one_of(&mut self, options: &[Schema]) -> Result<NodeRef> {
        if self.options.coerce_one_of {
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

    fn json_int(
        &mut self,
        minimum: Option<f64>,
        maximum: Option<f64>,
        exclusive_minimum: bool,
        exclusive_maximum: bool,
        multiple_of: Option<Decimal>,
    ) -> Result<RegexAst> {
        check_number_bounds(
            minimum,
            maximum,
            exclusive_minimum,
            exclusive_maximum,
            false,
            multiple_of.clone(),
        )
        .map_err(|e| {
            anyhow!(UnsatisfiableSchemaError {
                message: e.to_string(),
            })
        })?;
        let minimum = match (minimum, exclusive_minimum) {
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
        let maximum = match (maximum, exclusive_maximum) {
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
            format!(
                "Failed to generate regex for integer range: min={:?}, max={:?}",
                minimum, maximum
            )
        })?;
        let mut ast = RegexAst::Regex(rx);
        if let Some(d) = multiple_of {
            ast = RegexAst::And(vec![ast, RegexAst::MultipleOf(d.coef, d.exp)]);
        }
        Ok(ast)
    }

    fn json_number(
        &mut self,
        minimum: Option<f64>,
        maximum: Option<f64>,
        exclusive_minimum: bool,
        exclusive_maximum: bool,
        multiple_of: Option<Decimal>,
    ) -> Result<RegexAst> {
        check_number_bounds(
            minimum,
            maximum,
            exclusive_minimum,
            exclusive_maximum,
            false,
            multiple_of.clone(),
        )
        .map_err(|e| {
            anyhow!(UnsatisfiableSchemaError {
                message: e.to_string(),
            })
        })?;
        let rx = rx_float_range(minimum, maximum, !exclusive_minimum, !exclusive_maximum)
            .with_context(|| {
                format!(
                    "Failed to generate regex for float range: min={:?}, max={:?}",
                    minimum, maximum
                )
            })?;
        let mut ast = RegexAst::Regex(rx);
        if let Some(d) = multiple_of {
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
            let num = self.json_number(None, None, false, false, None).unwrap();
            let tf = self.builder.regex.regex("true|false").unwrap();
            let options = vec![
                self.builder.string("null"),
                self.builder.lexeme(tf),
                self.ast_lexeme(num).unwrap(),
                self.json_simple_string(),
                self.gen_json_array(&[], &Schema::Any, 0, None).unwrap(),
                self.gen_json_object(&IndexMap::new(), &Schema::Any, vec![])
                    .unwrap(),
            ];
            let inner = self.builder.select(&options);
            self.builder.set_placeholder(json_any, inner);
            json_any
        })
    }

    fn gen_json_object(
        &mut self,
        properties: &IndexMap<String, Schema>,
        additional_properties: &Schema,
        required: Vec<String>,
    ) -> Result<NodeRef> {
        let mut taken_names: Vec<String> = vec![];
        let mut items: Vec<(NodeRef, bool)> = vec![];
        for name in properties.keys().chain(
            required
                .iter()
                .filter(|n| !properties.contains_key(n.as_str())),
        ) {
            let property_schema = properties.get(name).unwrap_or(additional_properties);
            let is_required = required.contains(name);
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
                            message: format!("required property '{}' is unsatisfiable", name),
                        }));
                    }
                },
            };
            let name = self.builder.string(&quoted_name);
            taken_names.push(quoted_name);
            let colon = self.builder.string(&self.options.key_separator);
            let item = self.builder.join(&[name, colon, property]);
            items.push((item, is_required));
        }

        match self.gen_json(additional_properties) {
            Err(e) => {
                if e.downcast_ref::<UnsatisfiableSchemaError>().is_none() {
                    // Propagate errors that aren't UnsatisfiableSchemaError
                    return Err(e);
                }
                // Ignore UnsatisfiableSchemaError for additionalProperties
            }
            Ok(property) => {
                let name = if taken_names.is_empty() {
                    self.json_simple_string()
                } else {
                    let taken_name_ids = taken_names
                        .iter()
                        .map(|n| self.builder.regex.literal(n.to_string()))
                        .collect::<Vec<_>>();
                    let taken = self.builder.regex.select(taken_name_ids);
                    let not_taken = self.builder.regex.not(taken);
                    let valid = self
                        .builder
                        .regex
                        .regex(&format!("\"({})*\"", CHAR_REGEX))?;
                    let valid_and_not_taken = self.builder.regex.and(vec![valid, not_taken]);
                    self.builder.lexeme(valid_and_not_taken)
                };
                let colon = self.builder.string(&self.options.key_separator);
                let item = self.builder.join(&[name, colon, property]);
                let seq = self.sequence(item);
                items.push((seq, false));
            }
        }
        let opener = self.builder.string("{");
        let inner = self.ordered_sequence(&items, false, &mut HashMap::default());
        let closer = self.builder.string("}");
        Ok(self.builder.join(&[opener, inner, closer]))
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
            Schema::Boolean => Some(RegexAst::Regex("true|false".to_string())),
            Schema::LiteralBool { value } => literal_regex(if *value { "true" } else { "false" }),

            Schema::Number {
                minimum,
                maximum,
                exclusive_minimum,
                exclusive_maximum,
                integer,
                multiple_of,
            } => {
                let (minimum, exclusive_minimum) = match (minimum, exclusive_minimum) {
                    (Some(min), Some(xmin)) => {
                        if xmin >= min {
                            (Some(*xmin), true)
                        } else {
                            (Some(*min), false)
                        }
                    }
                    (Some(min), None) => (Some(*min), false),
                    (None, Some(xmin)) => (Some(*xmin), true),
                    (None, None) => (None, false),
                };
                let (maximum, exclusive_maximum) = match (maximum, exclusive_maximum) {
                    (Some(max), Some(xmax)) => {
                        if xmax <= max {
                            (Some(*xmax), true)
                        } else {
                            (Some(*max), false)
                        }
                    }
                    (Some(max), None) => (Some(*max), false),
                    (None, Some(xmax)) => (Some(*xmax), true),
                    (None, None) => (None, false),
                };
                Some(if *integer {
                    self.json_int(
                        minimum,
                        maximum,
                        exclusive_minimum,
                        exclusive_maximum,
                        multiple_of.clone(),
                    )?
                } else {
                    self.json_number(
                        minimum,
                        maximum,
                        exclusive_minimum,
                        exclusive_maximum,
                        multiple_of.clone(),
                    )?
                })
            }

            Schema::String {
                min_length,
                max_length,
                regex,
            } => {
                return self
                    .gen_json_string(*min_length, *max_length, regex.clone())
                    .map(Some)
            }

            Schema::Any
            | Schema::Unsatisfiable { .. }
            | Schema::Array { .. }
            | Schema::Object { .. }
            | Schema::AnyOf { .. }
            | Schema::OneOf { .. }
            | Schema::Ref { .. } => None,
        };
        Ok(r)
    }

    fn gen_json_string(
        &self,
        min_length: u64,
        max_length: Option<u64>,
        regex: Option<RegexAst>,
    ) -> Result<RegexAst> {
        if let Some(max_length) = max_length {
            if min_length > max_length {
                return Err(anyhow!(UnsatisfiableSchemaError {
                    message: format!(
                        "minLength ({}) is greater than maxLength ({})",
                        min_length, max_length
                    ),
                }));
            }
        }
        if min_length == 0 && max_length.is_none() && regex.is_none() {
            return Ok(self.json_quote(RegexAst::Regex("(?s:.*)".to_string())));
        }
        if let Some(mut ast) = regex {
            let mut positive = false;

            fn mk_rx_repr(ast: &RegexAst) -> String {
                let mut rx_repr = String::new();
                ast.write_to_str(&mut rx_repr, 1_000, None);
                rx_repr
            }

            // special-case literals - the length is easy to check
            if let RegexAst::Literal(s) = &ast {
                let l = s.chars().count() as u64;

                if l < min_length || l > max_length.unwrap_or(u64::MAX) {
                    return Err(anyhow!(UnsatisfiableSchemaError {
                        message: format!("Constant {:?} doesn't match length constraints", s)
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

    fn gen_json_array(
        &mut self,
        prefix_items: &[Schema],
        item_schema: &Schema,
        min_items: u64,
        max_items: Option<u64>,
    ) -> Result<NodeRef> {
        let mut max_items = max_items;

        if let Some(max_items) = max_items {
            if min_items > max_items {
                return Err(anyhow!(UnsatisfiableSchemaError {
                    message: format!(
                        "minItems ({}) is greater than maxItems ({})",
                        min_items, max_items
                    ),
                }));
            }
        }

        let additional_item_grm = match self.gen_json(item_schema) {
            Ok(node) => Some(node),
            Err(e) => match e.downcast_ref::<UnsatisfiableSchemaError>() {
                // If it's not an UnsatisfiableSchemaError, just propagate it normally
                None => return Err(e),
                // Item is optional; don't raise UnsatisfiableSchemaError
                Some(_) if prefix_items.len() >= min_items as usize => None,
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
        let n_to_add = max_items.map_or(prefix_items.len().max(min_items as usize), |max| {
            max as usize
        });

        for i in 0..n_to_add {
            let item = if i < prefix_items.len() {
                match self.gen_json(&prefix_items[i]) {
                    Ok(node) => node,
                    Err(e) => match e.downcast_ref::<UnsatisfiableSchemaError>() {
                        // If it's not an UnsatisfiableSchemaError, just propagate it normally
                        None => return Err(e),
                        // Item is optional; don't raise UnsatisfiableSchemaError.
                        // Set max_items to the current index, as we can't satisfy any more items.
                        Some(_) if i >= min_items as usize => {
                            max_items = Some(i as u64);
                            break;
                        }
                        // Item is required; add context and propagate UnsatisfiableSchemaError
                        Some(_) => {
                            return Err(e.context(UnsatisfiableSchemaError {
                                message: format!(
                                    "prefixItems[{}] is unsatisfiable but minItems is {}",
                                    i, min_items
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

            if i < min_items as usize {
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
        | RegexAst::ExprRef(_) => false,
    }
}
