use crate::{
    grammar_builder::{GrammarResult, RegexId},
    substring::substring,
    HashMap, HashSet,
};
use anyhow::{anyhow, bail, ensure, Result};
use derivre::RegexAst;

use crate::{
    api::{GenGrammarOptions, GenOptions, GrammarId, LLGuidanceOptions, NodeProps, RegexExt},
    json::json_merge,
    substring::{chunk_into_chars, chunk_into_words},
    GrammarBuilder, JsonCompileOptions, NodeRef,
};

use super::{
    ast::*,
    common::lookup_common_regex,
    lexer::Location,
    parser::{parse_lark, ParsedLark},
};

#[derive(Debug)]
struct Grammar {
    rules: HashMap<String, Rule>,
    tokens: HashMap<String, TokenDef>,
    ignore: Vec<Expansions>,
    llguidance_options: serde_json::Value,
}

impl Default for Grammar {
    fn default() -> Self {
        Self {
            rules: HashMap::default(),
            tokens: HashMap::default(),
            ignore: vec![],
            llguidance_options: serde_json::Value::Object(serde_json::Map::new()),
        }
    }
}

struct Compiler {
    builder: GrammarBuilder,
    parsed: ParsedLark,
    grammar: Grammar,
    node_ids: HashMap<String, NodeRef>,
    regex_ids: HashMap<String, RegexId>,
    in_progress: HashSet<String>,
    pending_json_grammars: Vec<(NodeRef, Location, serde_json::Value)>,
}

fn compile_lark(builder: GrammarBuilder, parsed: ParsedLark) -> Result<GrammarResult> {
    let c = Compiler {
        builder,
        parsed,
        grammar: Grammar::default(),
        node_ids: HashMap::default(),
        regex_ids: HashMap::default(),
        in_progress: HashSet::default(),
        pending_json_grammars: vec![],
    };
    c.execute()
}

/// Make sure given regex can be used inside /.../ in Lark syntax.
pub fn lark_regex_quote(rx: &str) -> String {
    let mut is_q = false;
    let mut res = String::new();
    for c in rx.chars() {
        let prev_q = is_q;
        is_q = false;
        match c {
            // make sure we don't terminate on /
            '/' => res.push_str("\\/"),
            // these are optional, but nice
            '\n' => res.push_str("\\n"),
            '\r' => res.push_str("\\r"),
            '\t' => res.push_str("\\t"),

            '\\' if !prev_q => {
                is_q = true;
            }
            _ => {
                if prev_q {
                    res.push('\\');
                }
                res.push(c);
            }
        }
    }
    res
}

pub fn lark_to_llguidance(mut builder: GrammarBuilder, lark: &str) -> Result<GrammarResult> {
    let parsed = parse_lark(lark)?;

    let n = std::cmp::min(lark.len() / 8, 1_000_000);
    builder.regex.spec.regex_builder.reserve(n);

    compile_lark(builder, parsed)
}

impl Compiler {
    fn do_token(&mut self, name: &str) -> Result<RegexId> {
        if let Some(id) = self.regex_ids.get(name) {
            return Ok(*id);
        }
        if self.in_progress.contains(name) {
            bail!("circular reference in token {:?} definition", name);
        }
        self.in_progress.insert(name.to_string());
        let token = self
            .grammar
            .tokens
            .remove(name)
            .ok_or_else(|| anyhow!("unknown name: {:?}", name))?;
        let id = self.do_token_expansions(token.expansions)?;
        self.regex_ids.insert(name.to_string(), id);
        self.in_progress.remove(name);
        Ok(id)
    }

    fn mk_regex(&mut self, info: &str, rx: String) -> Result<RegexId> {
        self.builder
            .regex
            .regex(&rx)
            .map_err(|e| anyhow!("invalid regex {rx:?} (in {info}): {e}"))
    }

    fn do_token_atom(&mut self, atom: Atom) -> Result<RegexId> {
        self.builder.check_limits()?;
        match atom {
            Atom::Group(expansions) => self.do_token_expansions(expansions),
            Atom::Maybe(expansions) => {
                let id = self.do_token_expansions(expansions)?;
                Ok(self.builder.regex.optional(id))
            }
            Atom::Value(value) => match value {
                Value::LiteralRange(a, b) => {
                    ensure!(
                        a.chars().count() == 1,
                        "range start must be a single character"
                    );
                    ensure!(
                        b.chars().count() == 1,
                        "range end must be a single character"
                    );
                    let a = a.chars().next().unwrap();
                    let b = b.chars().next().unwrap();
                    if a <= b {
                        self.mk_regex(
                            "range",
                            format!(
                                "[{}-{}]",
                                regex_syntax::escape(&a.to_string()),
                                regex_syntax::escape(&b.to_string())
                            ),
                        )
                    } else {
                        bail!("invalid range order: {:?}..{:?}", a, b);
                    }
                }
                Value::Name(n) => self.do_token(&n),
                Value::LiteralString(val, flags) => {
                    if flags.contains("i") {
                        self.mk_regex(
                            "string with i-flag",
                            format!("(?i){}", regex_syntax::escape(&val)),
                        )
                    } else {
                        Ok(self.builder.regex.literal(val))
                    }
                }
                Value::LiteralRegex(val, flags) => {
                    ensure!(!flags.contains("l"), "l-flag is not supported in regexes");
                    let rx = if flags.is_empty() {
                        val
                    } else {
                        format!("(?{}){}", flags, val)
                    };
                    self.mk_regex("regex", rx)
                }
                Value::RegexExt(s) => compile_lark_regex(&mut self.builder, s),
                Value::SpecialToken(s) => {
                    bail!("special tokens (like {:?}) cannot be used in terminals", s);
                }
                Value::Json(_) => {
                    bail!("%json literals cannot be used in terminals");
                }
                Value::GrammarRef(g) => {
                    bail!(
                        "grammar references (like {:?}) cannot be used in terminals",
                        g
                    );
                }
                Value::TemplateUsage { .. } => bail!("template usage not supported yet"),
            },
        }
    }

    fn do_token_expr(&mut self, expr: Expr) -> Result<RegexId> {
        let atom = self.do_token_atom(expr.atom)?;
        if let Some(range) = &expr.range {
            ensure!(expr.op.is_none(), "ranges not supported with operators");
            ensure!(range.0 >= 0, "range start must be >= 0, got {:?}", range);
            ensure!(
                range.1 >= range.0,
                "range end must be >= start, got {:?}",
                range
            );
            Ok(self.builder.regex.repeat(
                atom,
                range.0 as u32,
                if range.1 == i32::MAX {
                    None
                } else {
                    Some(range.1 as u32)
                },
            ))
        } else {
            match &expr.op {
                Some(op) => match op.0.as_str() {
                    "*" => Ok(self.builder.regex.zero_or_more(atom)),
                    "+" => Ok(self.builder.regex.one_or_more(atom)),
                    "?" => Ok(self.builder.regex.optional(atom)),
                    _ => {
                        bail!("unsupported operator: {:?}", op.0);
                    }
                },
                None => Ok(atom),
            }
        }
    }

    fn do_token_expansions(&mut self, expansions: Expansions) -> Result<RegexId> {
        self.builder.check_limits()?;
        let options = expansions
            .1
            .into_iter()
            .map(|alias| {
                let args = alias
                    .expansion
                    .0
                    .into_iter()
                    .map(|e| self.do_token_expr(e))
                    .collect::<Result<Vec<_>>>()?;
                Ok(self.builder.regex.concat(args))
            })
            .collect::<Result<Vec<_>>>()
            .map_err(|e| expansions.0.augment(e))?;
        Ok(self.builder.regex.select(options))
    }

    fn lift_regex(&mut self, rx_id: RegexId) -> Result<NodeRef> {
        Ok(self.builder.lexeme(rx_id))
    }

    fn get_grammar_id(g: &str) -> Result<GrammarId> {
        assert!(g.starts_with("@"));
        // see if g[1..] is an integer
        if g[1..].parse::<usize>().is_ok() {
            bail!("numeric grammar references no longer supported");
        } else {
            Ok(GrammarId::Name(g[1..].to_string()))
        }
    }

    fn do_atom(&mut self, loc: &Location, expr: Atom) -> Result<NodeRef> {
        match expr {
            Atom::Group(expansions) => self.do_expansions(expansions),
            Atom::Maybe(expansions) => {
                let id = self.do_expansions(expansions)?;
                Ok(self.builder.optional(id))
            }
            Atom::Value(value) => {
                match &value {
                    Value::Name(n) => {
                        if self.is_rule(n) {
                            return self.do_rule(n);
                        } else {
                            // OK -> treat as token
                        }
                    }
                    Value::SpecialToken(s) => {
                        if s.starts_with("<[") && s.ends_with("]>") {
                            let s = &s[2..s.len() - 2];
                            let mut ranges = vec![];
                            for range in s.split(",") {
                                let ends: Vec<&str> = range.split('-').map(|s| s.trim()).collect();
                                ensure!(
                                    ends.len() == 1 || ends.len() == 2,
                                    "invalid token range: {:?}",
                                    range
                                );
                                if ends.len() == 1 && ends[0].is_empty() {
                                    continue;
                                }
                                let start = ends[0].parse::<u32>()?;
                                let end = if ends.len() == 2 {
                                    ends[1].parse::<u32>()?
                                } else {
                                    start
                                };
                                ensure!(start <= end, "invalid token range: {:?}", range);
                                ranges.push(start..=end);
                            }
                            ensure!(!ranges.is_empty(), "empty token range");
                            return self.builder.token_ranges(ranges);
                        }
                        return self.builder.special_token(s);
                    }
                    Value::GrammarRef(g) => {
                        return Ok(self.builder.gen_grammar(
                            GenGrammarOptions {
                                grammar: Compiler::get_grammar_id(g)?,
                                temperature: None,
                            },
                            NodeProps::default(),
                        ));
                    }
                    Value::Json(_) => {
                        // consume value
                        let json_schema = match value {
                            Value::Json(s) => s,
                            _ => unreachable!(),
                        };

                        let name = format!("%json---{}", self.builder.num_nodes());
                        let gg = self.builder.gen_grammar(
                            GenGrammarOptions {
                                grammar: GrammarId::Name(name),
                                temperature: None, // TODO?
                            },
                            NodeProps::default(),
                        );
                        self.pending_json_grammars
                            .push((gg, loc.clone(), json_schema));
                        return Ok(gg);
                    }
                    // special case "" literal, so it doesn't pollute grammar with epsilon regex
                    Value::LiteralString(s, _) if s.is_empty() => return Ok(self.builder.empty()),
                    Value::RegexExt(_)
                    | Value::LiteralRange(_, _)
                    | Value::LiteralString(_, _)
                    | Value::LiteralRegex(_, _) => {
                        // treat as token
                    }
                    Value::TemplateUsage { .. } => {
                        bail!("template usage not supported yet");
                    }
                };
                let rx = self.do_token_atom(Atom::Value(value))?;
                Ok(self.lift_regex(rx)?)
            }
        }
    }

    fn do_expr(&mut self, loc: &Location, expr: Expr) -> Result<NodeRef> {
        let atom = self.do_atom(loc, expr.atom)?;

        if let Some((a, b)) = expr.range {
            ensure!(expr.op.is_none(), "ranges not supported with operators");
            ensure!(a <= b, "range end must be >= start, got {:?}", (a, b));
            ensure!(a >= 0, "range start must be >= 0, got {:?}", a);
            Ok(self.builder.repeat(
                atom,
                a as usize,
                if b == i32::MAX {
                    None
                } else {
                    Some(b as usize)
                },
            ))
        } else {
            match &expr.op {
                Some(op) => match op.0.as_str() {
                    "*" => Ok(self.builder.zero_or_more(atom)),
                    "+" => Ok(self.builder.one_or_more(atom)),
                    "?" => Ok(self.builder.optional(atom)),
                    _ => {
                        bail!("unsupported operator: {}", op.0);
                    }
                },
                None => Ok(atom),
            }
        }
    }

    fn do_expansions(&mut self, expansions: Expansions) -> Result<NodeRef> {
        self.builder.check_limits()?;
        let loc = expansions.0;
        let options = expansions
            .1
            .into_iter()
            .map(|alias| {
                let args = alias
                    .expansion
                    .0
                    .into_iter()
                    .map(|e| self.do_expr(&loc, e))
                    .collect::<Result<Vec<_>>>()?;
                Ok(self.builder.join(&args))
            })
            .collect::<Result<Vec<_>>>()
            .map_err(|e| loc.augment(e))?;
        Ok(self.builder.select(&options))
    }

    fn is_rule(&self, name: &str) -> bool {
        self.node_ids.contains_key(name)
            || self.in_progress.contains(name)
            || self.grammar.rules.contains_key(name)
    }

    fn do_rule(&mut self, name: &str) -> Result<NodeRef> {
        if let Some(id) = self.node_ids.get(name) {
            return Ok(*id);
        }
        if self.in_progress.contains(name) {
            let id = self.builder.new_node(name);
            self.node_ids.insert(name.to_string(), id);
            return Ok(id);
        }
        self.in_progress.insert(name.to_string());

        let id = self.do_rule_core(name)?;

        if let Some(placeholder) = self.node_ids.get(name) {
            self.builder.set_placeholder(*placeholder, id);
        }
        self.node_ids.insert(name.to_string(), id);
        self.in_progress.remove(name);
        Ok(id)
    }

    fn do_rule_core(&mut self, name: &str) -> Result<NodeRef> {
        let rule = self
            .grammar
            .rules
            .remove(name)
            .ok_or_else(|| anyhow!("rule {:?} not found", name))?;

        let props = NodeProps {
            max_tokens: rule.max_tokens,
            capture_name: rule.capture_name.clone(),
            ..Default::default()
        };

        if rule.stop.is_some() && rule.suffix.is_some() {
            bail!("stop= and suffix= cannot be used together");
        }

        let id = if let Some(stop) = rule.stop_like() {
            let is_empty = matches!(stop, Value::LiteralString(s, _) if s.is_empty());
            let stop_val = Atom::Value(stop.clone());
            let lazy = rule.is_lazy();
            let rx_id = self.do_token_expansions(rule.expansions)?;
            let stop_id = self.do_token_atom(stop_val)?;

            self.builder.gen(
                GenOptions {
                    body_rx: RegexAst::ExprRef(rx_id),
                    stop_rx: if is_empty {
                        RegexAst::EmptyString
                    } else {
                        RegexAst::ExprRef(stop_id)
                    },
                    stop_capture_name: rule.stop_capture_name.clone(),
                    lazy: Some(lazy),
                    temperature: rule.temperature,
                    is_suffix: Some(rule.suffix.is_some()),
                },
                props,
            )?
        } else {
            ensure!(
                rule.stop_capture_name.is_none(),
                "stop_capture_name requires stop= or suffix="
            );
            if rule.temperature.is_some() || rule.max_tokens.is_some() {
                match rule.expansions.single_atom() {
                    Some(Atom::Value(Value::GrammarRef(g))) => {
                        return Ok(self.builder.gen_grammar(
                            GenGrammarOptions {
                                grammar: Compiler::get_grammar_id(g)?,
                                temperature: rule.temperature,
                            },
                            props,
                        ));
                    }
                    _ => {
                        // try as terminal
                        let rx_id = self.do_token_expansions(rule.expansions).map_err(|e| {
                            anyhow::anyhow!(
                                "{}; temperature= and max_tokens= only \
                                supported on TERMINALS and @subgrammars",
                                e
                            )
                        })?;
                        return Ok(self.builder.lexeme_ext(rx_id, rule.temperature, props));
                    }
                }
            }

            let inner = self.do_expansions(rule.expansions)?;
            #[allow(clippy::assertions_on_constants)]
            if let Some(max_tokens) = rule.max_tokens {
                assert!(false, "max_tokens handled above for now");
                self.builder.join_props(
                    &[inner],
                    NodeProps {
                        max_tokens: Some(max_tokens),
                        // assume the user also wants capture
                        capture_name: Some(name.to_string()),
                        ..Default::default()
                    },
                )
            } else if rule.capture_name.is_some() {
                self.builder.join_props(&[inner], props)
            } else {
                inner
            }
        };
        Ok(id)
    }

    fn execute(mut self) -> Result<GrammarResult> {
        let mut grm = Grammar::default();
        for item in std::mem::take(&mut self.parsed.items) {
            let loc = item.location().clone();
            grm.process_item(item).map_err(|e| loc.augment(e))?;
        }
        let start_name = "start";
        ensure!(
            grm.rules.contains_key(start_name),
            "no {} rule found",
            start_name
        );
        let ignore = std::mem::take(&mut grm.ignore);
        self.grammar = grm;

        let opts: LLGuidanceOptions =
            serde_json::from_value(self.grammar.llguidance_options.clone())
                .map_err(|e| anyhow!("failed to parse %llguidance declaration: {}", e))?;

        let ignore = ignore
            .into_iter()
            .map(|exp| Ok(RegexAst::ExprRef(self.do_token_expansions(exp)?)))
            .collect::<Result<Vec<_>>>()?;
        let id = self.builder.add_grammar(opts, RegexAst::Or(ignore))?;

        let start = self.do_rule(start_name)?;
        self.builder.set_start_node(start);

        let mut builder = self.builder;
        for (gg, loc, json_schema) in self.pending_json_grammars {
            let opts = JsonCompileOptions::default();
            let res = opts
                .json_to_llg_no_validate(builder, json_schema)
                .map_err(|e| loc.augment(anyhow!("failed to compile JSON schema: {}", e)))?;
            builder = res.builder;
            builder.link_gen_grammar(gg, res.start_node)?;
        }

        Ok(builder.finalize(id))
    }
}

impl Grammar {
    fn add_token_def(&mut self, loc: &Location, local_name: String, regex: &str) -> Result<()> {
        ensure!(
            !self.tokens.contains_key(&local_name),
            "duplicate token (in import): {:?}",
            local_name
        );

        let t = TokenDef {
            name: local_name,
            params: None,
            priority: None,
            expansions: Expansions(
                loc.clone(),
                vec![Alias {
                    expansion: Expansion(vec![Expr {
                        atom: Atom::Value(Value::LiteralRegex(regex.to_string(), "".to_string())),
                        op: None,
                        range: None,
                    }]),
                    alias: None,
                }],
            ),
        };
        self.tokens.insert(t.name.clone(), t.clone());
        Ok(())
    }

    fn do_statement(&mut self, loc: &Location, statement: Statement) -> Result<()> {
        match statement {
            Statement::Ignore(exp) => {
                self.ignore.push(exp);
            }
            Statement::Import { path, alias } => {
                let regex = lookup_common_regex(&path)?;
                let local_name =
                    alias.unwrap_or_else(|| path.split('.').last().unwrap().to_string());
                self.add_token_def(loc, local_name, regex)?;
            }
            Statement::MultiImport { path, names } => {
                for n in names {
                    let qname = format!("{}.{}", path, n);
                    let regex = lookup_common_regex(&qname)?;
                    self.add_token_def(loc, n.to_string(), regex)?;
                }
            }
            Statement::LLGuidance(json_value) => {
                // merge-in at the JSON level
                json_merge(&mut self.llguidance_options, &json_value);
                // but also check if it's valid format and all the right types
                let _v: LLGuidanceOptions = serde_json::from_value(json_value)
                    .map_err(|e| anyhow!("failed to parse %llguidance declaration: {}", e))?;
            }
            Statement::OverrideRule(_) => {
                bail!("override statement not supported yet");
            }
            Statement::Declare(_) => {
                bail!("declare statement not supported yet");
            }
        }
        Ok(())
    }

    fn process_item(&mut self, item: Item) -> Result<()> {
        match item {
            Item::Rule(rule) => {
                ensure!(rule.params.is_none(), "params not supported yet");
                ensure!(rule.priority.is_none(), "priority not supported yet");
                ensure!(
                    !self.rules.contains_key(&rule.name),
                    "duplicate rule: {:?}",
                    rule.name
                );
                self.rules.insert(rule.name.clone(), rule);
            }
            Item::Token(token_def) => {
                ensure!(token_def.params.is_none(), "params not supported yet");
                ensure!(token_def.priority.is_none(), "priority not supported yet");
                ensure!(
                    !self.tokens.contains_key(&token_def.name),
                    "duplicate token: {:?}",
                    token_def.name
                );
                self.tokens.insert(token_def.name.clone(), token_def);
            }
            Item::Statement(loc, statement) => {
                self.do_statement(&loc, statement)?;
            }
        }
        Ok(())
    }
}

fn compile_lark_regex(builder: &mut GrammarBuilder, l: RegexExt) -> Result<RegexId> {
    let mut fields_set = vec![];
    if l.substring_chunks.is_some() {
        fields_set.push("substring_chunks");
    }
    if l.substring_words.is_some() {
        fields_set.push("substring_words");
    }
    if l.substring_chars.is_some() {
        fields_set.push("substring_chars");
    }
    if fields_set.is_empty() {
        bail!("no fields set on %regex");
    }
    if fields_set.len() > 1 {
        bail!("only one field can be set on %regex; got {:?}", fields_set);
    }

    let bld = &mut builder.regex.spec.regex_builder;

    let eref = if let Some(s) = l.substring_words {
        substring(bld, chunk_into_words(&s))?
    } else if let Some(s) = l.substring_chars {
        substring(bld, chunk_into_chars(&s))?
    } else if let Some(s) = l.substring_chunks {
        substring(bld, s.iter().map(|s| s.as_str()).collect())?
    } else {
        unreachable!()
    };

    Ok(eref)
}
