use crate::{json::schema::OptSchemaExt, regex_to_lark, HashMap, HashSet};
use anyhow::{anyhow, bail, Result};
use derivre::{Regex, RegexAst, RegexBuilder};

use super::{
    context::Context,
    schema::{ObjectSchema, Schema, IMPLEMENTED, META_AND_ANNOTATIONS},
};

pub struct SharedContext {
    defs: HashMap<String, Schema>,
    seen: HashSet<String>,
    n_compiled: usize,
    pending_warnings: Vec<String>,
    pattern_cache: PatternPropertyCache,
}

#[derive(Default)]
pub struct PatternPropertyCache {
    inner: HashMap<String, Regex>,
}

const CHECK_LIMIT: u64 = 10_000;

impl PatternPropertyCache {
    pub fn is_match(&mut self, regex: &str, value: &str) -> Result<bool> {
        if let Some(cached_regex) = self.inner.get_mut(regex) {
            return Ok(cached_regex.is_match(value));
        }

        let regex = regex_to_lark(regex, "dw");
        let mut builder = RegexBuilder::new();
        let eref = builder.mk_regex_for_serach(regex.as_str())?;
        let mut rx = builder.to_regex_limited(eref, CHECK_LIMIT)?;
        let res = rx.is_match(value);
        self.inner.insert(regex.to_string(), rx);
        Ok(res)
    }

    pub fn check_disjoint(&mut self, regexes: &[&String]) -> Result<()> {
        // TODO cache something?
        let mut builder = RegexBuilder::new();
        let erefs = regexes
            .iter()
            .map(|regex| {
                let regex = regex_to_lark(regex, "dw");
                builder.mk_regex_for_serach(regex.as_str())
            })
            .collect::<Result<Vec<_>>>()?;
        for (ai, a) in erefs.iter().enumerate() {
            for (bi, b) in erefs.iter().enumerate() {
                if ai >= bi {
                    continue;
                }
                let intersect = builder.mk(&RegexAst::And(vec![
                    RegexAst::ExprRef(*a),
                    RegexAst::ExprRef(*b),
                ]))?;
                let mut rx = builder
                    .to_regex_limited(intersect, CHECK_LIMIT)
                    .map_err(|_| {
                        anyhow!(
                            "can't determine if patternProperty regexes /{}/ and /{}/ are disjoint",
                            regex_to_lark(regexes[ai], ""),
                            regex_to_lark(regexes[bi], "")
                        )
                    })?;
                if !rx.always_empty() {
                    return Err(anyhow!(
                        "patternProperty regexes /{}/ and /{}/ are not disjoint",
                        regex_to_lark(regexes[ai], ""),
                        regex_to_lark(regexes[bi], "")
                    ));
                }
            }
        }

        Ok(())
    }

    pub fn property_schema<'a>(&mut self, obj: &'a ObjectSchema, prop: &str) -> Result<&'a Schema> {
        if let Some(schema) = obj.properties.get(prop) {
            return Ok(schema);
        }

        for (key, schema) in obj.pattern_properties.iter() {
            if self.is_match(key, prop)? {
                return Ok(schema);
            }
        }

        Ok(obj.additional_properties.schema_ref())
    }
}

impl SharedContext {
    pub fn new() -> Self {
        SharedContext {
            defs: HashMap::default(),
            seen: HashSet::default(),
            n_compiled: 0,
            pending_warnings: Vec::new(),
            pattern_cache: PatternPropertyCache::default(),
        }
    }
}

impl Context<'_> {
    pub fn insert_ref(&self, uri: &str, schema: Schema) {
        self.shared
            .borrow_mut()
            .defs
            .insert(uri.to_string(), schema);
    }

    pub fn get_ref_cloned(&self, uri: &str) -> Option<Schema> {
        self.shared.borrow().defs.get(uri).cloned()
    }

    pub fn mark_seen(&self, uri: &str) {
        self.shared.borrow_mut().seen.insert(uri.to_string());
    }

    pub fn been_seen(&self, uri: &str) -> bool {
        self.shared.borrow().seen.contains(uri)
    }

    pub fn is_valid_keyword(&self, keyword: &str) -> bool {
        if !self.draft.is_known_keyword(keyword)
            || IMPLEMENTED.contains(&keyword)
            || META_AND_ANNOTATIONS.contains(&keyword)
        {
            return true;
        }
        false
    }

    pub fn increment(&self) -> Result<()> {
        let mut shared = self.shared.borrow_mut();
        shared.n_compiled += 1;
        if shared.n_compiled > self.options.max_size {
            bail!("schema too large");
        }
        Ok(())
    }

    pub fn record_warning(&self, msg: String) {
        self.shared.borrow_mut().pending_warnings.push(msg);
    }

    pub fn property_schema<'a>(&self, obj: &'a ObjectSchema, prop: &str) -> Result<&'a Schema> {
        self.shared
            .borrow_mut()
            .pattern_cache
            .property_schema(obj, prop)
    }

    pub fn check_disjoint_pattern_properties(&self, regexes: &[&String]) -> Result<()> {
        self.shared
            .borrow_mut()
            .pattern_cache
            .check_disjoint(regexes)
    }

    pub fn into_result(self, schema: Schema) -> BuiltSchema {
        let mut shared = self.shared.borrow_mut();
        BuiltSchema {
            schema,
            definitions: std::mem::take(&mut shared.defs),
            warnings: std::mem::take(&mut shared.pending_warnings),
            pattern_cache: std::mem::take(&mut shared.pattern_cache),
        }
    }
}

pub struct BuiltSchema {
    pub schema: Schema,
    pub definitions: HashMap<String, Schema>,
    pub warnings: Vec<String>,
    pub pattern_cache: PatternPropertyCache,
}

impl BuiltSchema {
    pub fn simple(schema: Schema) -> Self {
        BuiltSchema {
            schema,
            definitions: HashMap::default(),
            warnings: Vec::new(),
            pattern_cache: PatternPropertyCache::default(),
        }
    }
}
