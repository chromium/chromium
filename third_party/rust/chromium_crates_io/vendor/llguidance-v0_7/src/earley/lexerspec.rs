use anyhow::{ensure, Result};
use derivre::{raw::ExprSet, ExprRef, HashMap, JsonQuoteOptions, RegexAst, RegexBuilder};
use std::{fmt::Debug, hash::Hash, ops::RangeInclusive};
use toktrie::{bytes::limit_bytes, SimpleVob, TokTrie, TokenId};

use crate::{api::ParserLimits, id32_type};

use super::{
    lexer::MatchingLexemesIdx,
    regexvec::{LexemeSet, MatchingLexemes, RegexVec, RxLexeme},
};

#[derive(Clone)]
pub struct LexerSpec {
    pub lexemes: Vec<LexemeSpec>,
    pub regex_builder: RegexBuilder,
    pub no_forcing: bool,
    pub allow_initial_skip: bool,
    pub num_extra_lexemes: usize,
    pub skip_by_class: Vec<LexemeIdx>,
    class_by_skip: HashMap<ExprRef, LexemeClass>,
    pub current_class: LexemeClass,
    // regex for \xFF \[ [0-9]+ \]
    pub special_token_rx: Option<ExprRef>,
    pub has_stop: bool,
    pub has_max_tokens: bool,
    pub has_temperature: bool,
}

#[derive(Clone, Copy, Hash, PartialEq, Eq, Debug)]
pub struct LexemeClass(u8);

impl LexemeClass {
    pub const ROOT: LexemeClass = LexemeClass(0);

    pub fn as_usize(&self) -> usize {
        self.0 as usize
    }
    pub fn new(class: usize) -> Self {
        LexemeClass(class.try_into().expect("class too large"))
    }
}

#[derive(Clone)]
pub struct LexemeSpec {
    pub(crate) idx: LexemeIdx,
    pub(crate) single_set: MatchingLexemes,
    pub(crate) name: String,
    pub(crate) rx: RegexAst,
    class: LexemeClass,
    pub(crate) compiled_rx: ExprRef,
    ends_at_eos: bool,
    lazy: bool,
    contextual: bool,
    max_tokens: usize,
    pub(crate) is_extra: bool,
    pub(crate) is_suffix: bool,
    pub(crate) is_skip: bool,
    json_options: Option<JsonQuoteOptions>,
    pub(crate) token_ranges: Vec<RangeInclusive<TokenId>>,
}

// LexemeIdx is an index into the lexeme table.
// It corresponds to a category like IDENTIFIER or STRING,
// or to a very specific lexeme like WHILE or MULTIPLY.
id32_type!(LexemeIdx);

pub fn token_ranges_to_string(token_ranges: &Vec<RangeInclusive<TokenId>>) -> String {
    use std::fmt::Write;
    let mut s = "<[".to_string();
    for range in token_ranges {
        if s.len() > 2 {
            s.push(',');
        }
        if range.start() == range.end() {
            write!(s, "{:?}", range.start()).unwrap();
        } else {
            write!(s, "{:?}-{:?}", range.start(), range.end()).unwrap();
        }
    }
    s.push_str("]>");
    s
}

impl LexemeSpec {
    pub fn class(&self) -> LexemeClass {
        self.class
    }

    pub fn max_tokens(&self) -> usize {
        self.max_tokens
    }

    pub fn to_string(&self, max_len: usize, exprset: Option<&ExprSet>) -> String {
        use std::fmt::Write;
        let mut f = String::new();
        write!(f, "[{}] {} ", self.idx.0, self.name).unwrap();
        self.rx.write_to_str(&mut f, max_len, exprset);
        if self.lazy {
            f.push_str(" lazy");
        }
        if self.is_suffix {
            f.push_str(" suffix");
        }
        if self.contextual {
            f.push_str(" contextual");
        }
        if self.is_extra {
            f.push_str(" extra");
        }
        if !self.token_ranges.is_empty() {
            write!(f, " tokens={}", token_ranges_to_string(&self.token_ranges)).unwrap();
        }
        // write!(f, " compiled={:?}", self.compiled_rx).unwrap();
        f
    }

    pub fn contains_token(&self, token: TokenId) -> bool {
        self.token_ranges.iter().any(|range| range.contains(&token))
    }
}

impl Debug for LexemeSpec {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let s = self.to_string(512, None);
        f.write_str(&s)
    }
}

impl LexerSpec {
    pub fn new() -> Result<Self> {
        Ok(LexerSpec {
            lexemes: Vec::new(),
            special_token_rx: None,
            regex_builder: RegexBuilder::new(),
            no_forcing: false,
            allow_initial_skip: false,
            num_extra_lexemes: 0,
            skip_by_class: Vec::new(),
            current_class: LexemeClass(0),
            class_by_skip: HashMap::default(),
            has_stop: false,
            has_max_tokens: false,
            has_temperature: false,
        })
    }

    pub fn can_rollback(&self) -> bool {
        !self.has_stop && !self.has_max_tokens
    }

    pub fn check_rollback(&self) -> Result<()> {
        ensure!(
            self.can_rollback(),
            "rollback not supported with max_tokens=... or stop=... lexemes; suffix=... is OK"
        );
        Ok(())
    }

    /// Check if the lexeme always matches bytes.
    pub fn has_forced_bytes(&self, lex_spec: &LexemeSpec, bytes: &[u8]) -> bool {
        self.regex_builder
            .exprset()
            .has_simply_forced_bytes(lex_spec.compiled_rx, bytes)
    }

    pub fn setup_lexeme_class(&mut self, skip: RegexAst) -> Result<LexemeClass> {
        let skip_node = self.regex_builder.mk(&skip)?; // validate first

        if !self.has_max_tokens && !self.has_temperature {
            if let Some(&cls) = self.class_by_skip.get(&skip_node) {
                // re-use existing
                self.current_class = cls;
                return Ok(cls);
            }
        }

        self.current_class = LexemeClass::new(self.skip_by_class.len());
        self.class_by_skip.insert(skip_node, self.current_class);
        self.skip_by_class.push(LexemeIdx(0)); // avoid assert in empty_spec()
        let idx = self
            .add_lexeme_spec(LexemeSpec {
                name: format!("SKIP{}", self.current_class.as_usize()),
                rx: skip,
                is_skip: true,
                ..self.empty_spec()
            })
            .expect("already validated");
        self.skip_by_class.pop();
        self.skip_by_class.push(idx);
        Ok(self.current_class)
    }

    pub fn alloc_lexeme_set(&self) -> LexemeSet {
        LexemeSet::new(self.lexemes.len())
    }

    pub fn alloc_grammar_set(&self) -> SimpleVob {
        SimpleVob::alloc(self.skip_by_class.len())
    }

    pub fn lexeme_set(&self, cond: impl Fn(&LexemeSpec) -> bool) -> LexemeSet {
        let mut v = self.alloc_lexeme_set();
        for (idx, lex) in self.lexemes.iter().enumerate() {
            if cond(lex) {
                v.add(LexemeIdx::new(idx));
            }
        }
        v
    }

    pub fn all_lexemes(&self) -> LexemeSet {
        self.lexeme_set(|_| true)
    }

    pub fn lazy_lexemes(&self) -> LexemeSet {
        self.lexeme_set(|lex| lex.lazy)
    }

    pub fn eos_ending_lexemes(&self) -> LexemeSet {
        self.lexeme_set(|lex| lex.ends_at_eos)
    }

    pub fn token_range_lexemes(&self, possible: &LexemeSet) -> Vec<&LexemeSpec> {
        let mut res = Vec::new();
        for idx in possible.iter() {
            let spec = &self.lexemes[idx.as_usize()];
            if !spec.token_ranges.is_empty() {
                res.push(spec);
            }
        }
        res
    }

    pub fn is_nullable(&self, idx: LexemeIdx) -> bool {
        self.regex_builder
            .is_nullable(self.lexemes[idx.as_usize()].compiled_rx)
    }

    pub fn to_regex_vec(&self, limits: &mut ParserLimits) -> Result<RegexVec> {
        // TODO
        // Find all non-contextual lexemes that are literals (we call them 'keywords')
        // This assumes that this is the only possible conflict in the lexer that we want to catch.
        // For every non literals lexeme, find all keywords that match it.
        // Replace the regex R for the lexeme with (R & ~(K1|K2|...)) where K1...
        // are the conflicting keywords.
        let rx_list: Vec<_> = self
            .lexemes
            .iter()
            .map(|lex| RxLexeme {
                rx: lex.compiled_rx,
                priority: 0,
                lazy: lex.lazy,
            })
            .collect();
        RegexVec::new_with_exprset(
            self.regex_builder.exprset().clone(),
            rx_list,
            self.special_token_rx,
            limits,
        )
    }

    fn add_lexeme_spec(&mut self, mut spec: LexemeSpec) -> Result<LexemeIdx> {
        let compiled = if !spec.token_ranges.is_empty() {
            if let Some(rx) = self.special_token_rx {
                rx
            } else {
                let rx_ast = RegexAst::Concat(vec![
                    RegexAst::Byte(TokTrie::SPECIAL_TOKEN_MARKER),
                    RegexAst::Regex(r"\[[0-9]+\]".to_string()),
                ]);
                let compiled = self.regex_builder.mk(&rx_ast)?;
                self.special_token_rx = Some(compiled);
                compiled
            }
        } else {
            self.regex_builder.mk(&spec.rx)?
        };

        if !self.has_stop && !spec.is_suffix {
            self.has_stop = match &spec.rx {
                RegexAst::Concat(parts) => parts
                    .iter()
                    .any(|part| matches!(part, RegexAst::LookAhead(_))),
                _ => false,
            };
            if spec.ends_at_eos {
                self.has_stop = true;
            }
        }

        if spec.max_tokens < usize::MAX {
            self.has_max_tokens = true;
        }

        let compiled = if let Some(ref opts) = spec.json_options {
            self.regex_builder.json_quote(compiled, opts)?
        } else {
            compiled
        };

        if let Some(idx) = self.lexemes.iter().position(|lex| {
            lex.compiled_rx == compiled
                && lex.class == spec.class
                && lex.max_tokens == spec.max_tokens
                && lex.token_ranges == spec.token_ranges
                && lex.is_extra == spec.is_extra
        }) {
            return Ok(LexemeIdx::new(idx));
        }
        let idx = LexemeIdx::new(self.lexemes.len());
        spec.idx = idx;
        spec.single_set = MatchingLexemes::One(idx);
        spec.compiled_rx = compiled;
        if spec.name.is_empty() {
            spec.name = format!("[{}]", idx.as_usize());
        }
        self.lexemes.push(spec);
        Ok(idx)
    }

    fn empty_spec(&self) -> LexemeSpec {
        assert!(
            !self.skip_by_class.is_empty(),
            "new_lexeme_class() not called"
        );
        LexemeSpec {
            idx: LexemeIdx(0),
            single_set: MatchingLexemes::One(LexemeIdx(0)),
            name: "".to_string(),
            rx: RegexAst::NoMatch,
            compiled_rx: ExprRef::INVALID,
            lazy: false,
            contextual: false,
            ends_at_eos: false,
            is_skip: false,
            is_suffix: false,
            is_extra: false,
            json_options: None,
            class: self.current_class,
            max_tokens: usize::MAX,
            token_ranges: vec![],
        }
    }

    pub fn add_rx_and_stop(
        &mut self,
        name: String,
        body_rx: RegexAst,
        stop_rx: RegexAst,
        lazy: bool,
        max_tokens: usize,
        is_suffix: bool,
    ) -> Result<LexemeIdx> {
        let rx = if !matches!(stop_rx, RegexAst::EmptyString) {
            RegexAst::Concat(vec![body_rx, RegexAst::LookAhead(Box::new(stop_rx))])
        } else {
            body_rx
        };
        self.add_lexeme_spec(LexemeSpec {
            name,
            rx,
            lazy,
            ends_at_eos: !lazy,
            max_tokens,
            is_suffix,
            ..self.empty_spec()
        })
    }

    pub fn add_simple_literal(
        &mut self,
        name: String,
        literal: &str,
        contextual: bool,
    ) -> Result<LexemeIdx> {
        self.add_lexeme_spec(LexemeSpec {
            name,
            rx: RegexAst::Literal(literal.to_string()),
            contextual,
            ..self.empty_spec()
        })
    }

    pub fn add_special_token(
        &mut self,
        name: String,
        token_ranges: Vec<RangeInclusive<TokenId>>,
    ) -> Result<LexemeIdx> {
        self.add_lexeme_spec(LexemeSpec {
            name,
            token_ranges,
            ..self.empty_spec()
        })
    }

    pub fn add_greedy_lexeme(
        &mut self,
        name: String,
        rx: RegexAst,
        contextual: bool,
        json_options: Option<JsonQuoteOptions>,
        max_tokens: usize,
    ) -> Result<LexemeIdx> {
        self.add_lexeme_spec(LexemeSpec {
            name,
            rx,
            contextual,
            json_options,
            max_tokens,
            ..self.empty_spec()
        })
    }

    pub fn add_extra_lexemes(&mut self, extra_lexemes: &[String]) {
        assert!(self.num_extra_lexemes == 0);
        self.num_extra_lexemes = extra_lexemes.len();
        let lex0 = self.lexemes.len();
        for (idx, added) in extra_lexemes.iter().enumerate() {
            self.add_lexeme_spec(LexemeSpec {
                name: format!("$extra_{}", idx),
                rx: RegexAst::Regex(added.clone()),
                is_extra: true,
                ..self.empty_spec()
            })
            .expect("adding lexeme");
        }
        assert!(
            self.lexemes.len() - lex0 == self.num_extra_lexemes,
            "repeating slices?"
        );
    }

    pub fn extra_lexeme(&self, idx: usize) -> LexemeIdx {
        assert!(idx < self.num_extra_lexemes);
        self.lexemes[self.lexemes.len() - self.num_extra_lexemes + idx].idx
    }

    pub fn dbg_lexeme_set(&self, vob: &LexemeSet) -> String {
        format!(
            "Lexemes( {} )",
            vob.iter()
                .map(|idx| format!("[{}]", idx.as_usize()))
                .collect::<Vec<_>>()
                .join(", ")
        )
    }

    pub fn lexeme_spec(&self, idx: LexemeIdx) -> &LexemeSpec {
        &self.lexemes[idx.as_usize()]
    }

    pub fn cost(&self) -> u64 {
        self.regex_builder.exprset().cost()
    }

    pub fn skip_id(&self, class: LexemeClass) -> LexemeIdx {
        self.skip_by_class[class.as_usize()]
    }

    pub fn lexeme_def_to_string(&self, idx: LexemeIdx) -> String {
        self.lexemes[idx.as_usize()].to_string(512, Some(self.regex_builder.exprset()))
    }

    pub fn dbg_lexeme_set_ext(&self, vob: &SimpleVob) -> String {
        format!(
            "LexemesExt(\n    {}\n)",
            vob.iter()
                .map(|idx| self.lexeme_def_to_string(LexemeIdx::new(idx as usize)))
                .collect::<Vec<_>>()
                .join("\n    ")
        )
    }
}

impl Debug for LexerSpec {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(f, "LexerSpec {{ lexemes: [")?;
        for lex in &self.lexemes {
            let slex = lex.to_string(512, Some(self.regex_builder.exprset()));
            writeln!(f, "  {}", slex)?;
        }
        write!(
            f,
            "]{}{} }}",
            if self.has_stop { " has_stop" } else { "" },
            if self.has_max_tokens {
                " has_max_tokens"
            } else {
                ""
            }
        )
    }
}

#[derive(Clone)]
pub struct Lexeme {
    pub idx: MatchingLexemesIdx,
    bytes: Vec<u8>,
    hidden_len: u32,
    is_suffix: bool,
}

impl Debug for Lexeme {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "Lexeme({:?}, {:?} + {:?}{})",
            self.idx,
            limit_bytes(self.visible_bytes(), 100),
            limit_bytes(self.hidden_bytes(), 100),
            if self.is_suffix { " suffix" } else { "" }
        )
    }
}

impl Lexeme {
    pub fn new(idx: MatchingLexemesIdx, bytes: Vec<u8>, hidden_len: u32, is_suffix: bool) -> Self {
        Lexeme {
            idx,
            bytes,
            hidden_len,
            is_suffix,
        }
    }

    pub fn just_idx(idx: MatchingLexemesIdx) -> Self {
        Lexeme {
            idx,
            hidden_len: 0,
            bytes: Vec::new(),
            is_suffix: false,
        }
    }

    pub fn single_idx(idx: LexemeIdx) -> Self {
        Self::just_idx(MatchingLexemesIdx::Single(idx))
    }

    pub fn bogus() -> Self {
        Self::single_idx(LexemeIdx(0))
    }

    pub fn is_bogus(&self) -> bool {
        self.bytes.is_empty() && matches!(self.idx, MatchingLexemesIdx::Single(LexemeIdx(0)))
    }

    pub fn is_suffix(&self) -> bool {
        self.is_suffix
    }

    #[inline(always)]
    pub fn num_hidden_bytes(&self) -> usize {
        self.hidden_len as usize
    }

    pub fn num_visible_bytes(&self) -> usize {
        self.bytes.len() - self.num_hidden_bytes()
    }

    pub fn visible_bytes(&self) -> &[u8] {
        &self.bytes[0..self.num_visible_bytes()]
    }

    pub fn upper_visible_bytes(&self, is_lexeme: bool) -> &[u8] {
        if is_lexeme || !self.is_suffix {
            self.visible_bytes()
        } else {
            self.all_bytes()
        }
    }

    pub fn hidden_bytes(&self) -> &[u8] {
        &self.bytes[self.num_visible_bytes()..]
    }

    pub fn all_bytes(&self) -> &[u8] {
        &self.bytes
    }
}
