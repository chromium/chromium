use super::lexerspec::{LexemeClass, LexemeIdx, LexerSpec};
use crate::api::{GenGrammarOptions, GrammarId, NodeProps, ParserLimits};
use crate::hashcons::{HashCons, HashId};
use crate::{HashMap, HashSet};
use anyhow::{bail, ensure, Result};
use serde::{Deserialize, Serialize};
use std::fmt::Display;
use std::{fmt::Debug, hash::Hash};

const DEBUG: bool = true;
macro_rules! debug {
    ($($arg:tt)*) => {
        if cfg!(feature = "logging") && DEBUG {
            eprintln!($($arg)*);
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct SymIdx(u32);

impl SymIdx {
    pub const BOGUS: SymIdx = SymIdx(u32::MAX);
    pub fn as_usize(&self) -> usize {
        self.0 as usize
    }
}

impl Symbol {
    fn is_terminal(&self) -> bool {
        self.is_lexeme_terminal()
    }
    fn is_lexeme_terminal(&self) -> bool {
        self.lexeme.is_some()
    }
    fn short_name(&self) -> String {
        if let Some(lex) = self.lexeme {
            format!("[{}]", lex.as_usize())
        } else {
            self.name.clone()
        }
    }
    fn param_name(&self, param: &ParamExpr) -> String {
        let mut r = self.short_name();
        if param != &ParamExpr::Null {
            r.push_str(&format!("::{param}"));
        }
        r
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct SymbolProps {
    pub max_tokens: usize,
    pub capture_name: Option<String>,
    pub stop_capture_name: Option<String>,
    pub temperature: f32,
    pub grammar_id: LexemeClass,
    pub is_start: bool,
    pub parametric: bool,
}

impl Default for SymbolProps {
    fn default() -> Self {
        SymbolProps {
            max_tokens: usize::MAX,
            capture_name: None,
            stop_capture_name: None,
            temperature: 0.0,
            is_start: false,
            grammar_id: LexemeClass::ROOT,
            parametric: false,
        }
    }
}

impl SymbolProps {
    /// Special nodes can't be removed in grammar optimizations
    pub fn is_special(&self) -> bool {
        self.max_tokens < usize::MAX
            || self.capture_name.is_some()
            || self.stop_capture_name.is_some()
            || self.is_start
    }

    // this is used when a rule like 'self -> [self.for_wrapper()]` is added
    pub fn for_wrapper(&self) -> Self {
        assert!(!self.parametric);
        SymbolProps {
            max_tokens: self.max_tokens,
            capture_name: None,
            stop_capture_name: None,
            temperature: self.temperature,
            grammar_id: self.grammar_id,
            is_start: false,
            parametric: false,
        }
    }

    pub fn neutral_param(&self) -> ParamExpr {
        if self.parametric {
            ParamExpr::SelfRef
        } else {
            ParamExpr::Null
        }
    }
}

impl Display for SymbolProps {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.capture_name.is_some() {
            write!(f, " CAPTURE")?;
        }
        if self.stop_capture_name.is_some() {
            write!(
                f,
                " STOP-CAPTURE={}",
                self.stop_capture_name.as_ref().unwrap()
            )?;
        }
        if self.max_tokens < 10000 {
            write!(f, " max_tokens={}", self.max_tokens)?;
        }
        if self.temperature != 0.0 {
            write!(f, " temp={:.2}", self.temperature)?;
        }
        Ok(())
    }
}

#[derive(Debug, Clone)]
struct Symbol {
    idx: SymIdx,
    name: String,
    lexeme: Option<LexemeIdx>,
    gen_grammar: Option<GenGrammarOptions>,
    rules: Vec<Rule>,
    props: SymbolProps,
}

#[derive(Debug, Clone)]
struct Rule {
    lhs: SymIdx,
    rhs: Vec<(SymIdx, ParamExpr)>,
    condition: ParamCond,
}

impl Rule {
    fn lhs(&self) -> SymIdx {
        self.lhs
    }
}

#[derive(Clone)]
pub struct Grammar {
    name: Option<String>,
    parametric: bool,
    symbols: Vec<Symbol>,
    symbol_count_cache: HashMap<String, usize>,
    symbol_by_name: HashMap<String, SymIdx>,
}

#[derive(Clone, Default, Copy, PartialEq, Eq, Debug, Serialize, Deserialize, Hash)]
pub struct ParamValue(pub u64);

impl ParamValue {
    pub const NUM_BITS: usize = 64;
    pub fn is_default(&self) -> bool {
        self.0 == 0
    }
}

impl Display for ParamValue {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "0x{:x}", self.0)
    }
}

pub type BitIdx = u8;

#[derive(Copy, Clone, PartialEq, Eq, Debug, Serialize, Deserialize, Hash)]
pub struct ParamRef {
    start: BitIdx,
    end: BitIdx,
}

impl ParamRef {
    pub fn new(start: BitIdx, end: BitIdx) -> Self {
        assert!(start < ParamValue::NUM_BITS as u8);
        assert!(end <= ParamValue::NUM_BITS as u8);
        assert!(start < end);
        ParamRef { start, end }
    }

    pub fn full() -> Self {
        ParamRef {
            start: 0,
            end: ParamValue::NUM_BITS as u8,
        }
    }

    pub fn single_bit(k: BitIdx) -> Self {
        assert!(k < ParamValue::NUM_BITS as u8);
        ParamRef {
            start: k,
            end: k + 1,
        }
    }

    pub fn start(&self) -> BitIdx {
        self.start
    }

    pub fn end(&self) -> BitIdx {
        self.end
    }

    pub fn len(&self) -> usize {
        (self.end - self.start) as usize
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    #[inline(always)]
    pub fn mask(&self) -> u64 {
        let l = self.len();
        if l == 64 {
            u64::MAX
        } else {
            ((1u64 << l) - 1) << self.start()
        }
    }

    #[inline(always)]
    pub fn eval(&self, m: ParamValue) -> ParamValue {
        ParamValue((m.0 & self.mask()) >> self.start())
    }
}

impl Display for ParamRef {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.start == 0 && self.end == ParamValue::NUM_BITS as u8 {
            write!(f, "_")
        } else {
            write!(f, "[{}:{}]", self.start, self.end)
        }
    }
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize, Hash)]
pub enum ParamExpr {
    Null,
    Const(ParamValue),
    Incr(ParamRef),
    Decr(ParamRef),
    BitOr(ParamValue),
    BitAnd(ParamValue),
    SelfRef,
}

// we make this wide to minimize the number of branch mis-predictions
#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize, Hash)]
pub enum ParamCond {
    True,
    NE(ParamRef, ParamValue),
    EQ(ParamRef, ParamValue),
    LE(ParamRef, ParamValue),
    LT(ParamRef, ParamValue),
    GE(ParamRef, ParamValue),
    GT(ParamRef, ParamValue),
    BitCountNE(ParamRef, BitIdx),
    BitCountEQ(ParamRef, BitIdx),
    BitCountLE(ParamRef, BitIdx),
    BitCountLT(ParamRef, BitIdx),
    BitCountGE(ParamRef, BitIdx),
    BitCountGT(ParamRef, BitIdx),
    And(Box<ParamCond>, Box<ParamCond>),
    Or(Box<ParamCond>, Box<ParamCond>),
    Not(Box<ParamCond>),
}

impl ParamExpr {
    pub fn eval(&self, m: ParamValue) -> ParamValue {
        match self {
            ParamExpr::Null => ParamValue::default(),
            ParamExpr::Const(v) => *v,
            ParamExpr::SelfRef => m,
            ParamExpr::Incr(pr) => {
                if (m.0 & pr.mask()) == pr.mask() {
                    m
                } else {
                    ParamValue(m.0 + (1 << pr.start()))
                }
            }
            ParamExpr::Decr(pr) => {
                if (m.0 & pr.mask()) == 0 {
                    m
                } else {
                    ParamValue(m.0 - (1 << pr.start()))
                }
            }
            ParamExpr::BitOr(pv) => ParamValue(m.0 | pv.0),
            ParamExpr::BitAnd(pv) => ParamValue(m.0 & pv.0),
        }
    }

    pub fn is_null(&self) -> bool {
        matches!(self, ParamExpr::Null)
    }

    pub fn is_self_ref(&self) -> bool {
        matches!(self, ParamExpr::SelfRef)
    }

    pub fn needs_param(&self) -> bool {
        !matches!(self, ParamExpr::Null | ParamExpr::Const(_))
    }
}

impl Display for ParamExpr {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ParamExpr::Null => write!(f, "null"),
            ParamExpr::SelfRef => write!(f, "_"),
            ParamExpr::Const(v) => write!(f, "{v}"),
            ParamExpr::Incr(pr) => write!(f, "incr({pr})"),
            ParamExpr::Decr(pr) => write!(f, "decr({pr})"),
            ParamExpr::BitOr(v) => {
                if v.0.count_ones() == 1 {
                    write!(f, "set_bit({})", v.0.trailing_zeros())
                } else {
                    write!(f, "bit_or({v})")
                }
            }
            ParamExpr::BitAnd(v) => {
                if (!v.0).count_ones() == 1 {
                    write!(f, "clear_bit({})", (!v.0).trailing_zeros())
                } else {
                    write!(f, "bit_and({v})")
                }
            }
        }
    }
}

impl ParamCond {
    pub fn eval(&self, m: ParamValue) -> bool {
        match self {
            ParamCond::True => true,
            ParamCond::NE(pr, pv) => pr.eval(m) != *pv,
            ParamCond::EQ(pr, pv) => pr.eval(m) == *pv,
            ParamCond::LE(pr, pv) => pr.eval(m).0 <= pv.0,
            ParamCond::LT(pr, pv) => pr.eval(m).0 < pv.0,
            ParamCond::GE(pr, pv) => pr.eval(m).0 >= pv.0,
            ParamCond::GT(pr, pv) => pr.eval(m).0 > pv.0,
            ParamCond::BitCountNE(pr, bc) => pr.eval(m).0.count_ones() != *bc as u32,
            ParamCond::BitCountEQ(pr, bc) => pr.eval(m).0.count_ones() == *bc as u32,
            ParamCond::BitCountLE(pr, bc) => pr.eval(m).0.count_ones() <= *bc as u32,
            ParamCond::BitCountLT(pr, bc) => pr.eval(m).0.count_ones() < *bc as u32,
            ParamCond::BitCountGE(pr, bc) => pr.eval(m).0.count_ones() >= *bc as u32,
            ParamCond::BitCountGT(pr, bc) => pr.eval(m).0.count_ones() > *bc as u32,
            ParamCond::And(c1, c2) => c1.eval(m) && c2.eval(m),
            ParamCond::Or(c1, c2) => c1.eval(m) || c2.eval(m),
            ParamCond::Not(c) => !c.eval(m),
        }
    }

    pub fn is_true(&self) -> bool {
        matches!(self, ParamCond::True)
    }
}

impl Display for ParamCond {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ParamCond::True => write!(f, "true"),
            ParamCond::NE(pr, pv) => write!(f, "ne({pr}, {pv})"),
            ParamCond::EQ(pr, pv) => {
                if pr.len() == 1 && pv.0 == 0 {
                    write!(f, "bit_clear({})", pr.start())
                } else if pr.len() == 1 && pv.0 == 1 {
                    write!(f, "bit_set({})", pr.start())
                } else if pv.0 == 0 {
                    write!(f, "is_zeros({pr})")
                } else if pv.0 == (pr.mask() >> pr.start()) {
                    write!(f, "is_ones({pr})")
                } else {
                    write!(f, "eq({pr}, {pv})")
                }
            }
            ParamCond::LE(pr, pv) => write!(f, "le({pr}, {pv})"),
            ParamCond::LT(pr, pv) => write!(f, "lt({pr}, {pv})"),
            ParamCond::GE(pr, pv) => write!(f, "ge({pr}, {pv})"),
            ParamCond::GT(pr, pv) => write!(f, "gt({pr}, {pv})"),
            ParamCond::BitCountNE(pr, bc) => write!(f, "bit_count_ne({pr}, {bc})"),
            ParamCond::BitCountEQ(pr, bc) => write!(f, "bit_count_eq({pr}, {bc})"),
            ParamCond::BitCountLE(pr, bc) => write!(f, "bit_count_le({pr}, {bc})"),
            ParamCond::BitCountLT(pr, bc) => write!(f, "bit_count_lt({pr}, {bc})"),
            ParamCond::BitCountGE(pr, bc) => write!(f, "bit_count_ge({pr}, {bc})"),
            ParamCond::BitCountGT(pr, bc) => write!(f, "bit_count_gt({pr}, {bc})"),
            ParamCond::And(c1, c2) => write!(f, "and({c1}, {c2})"),
            ParamCond::Or(c1, c2) => write!(f, "or({c1}, {c2})"),
            ParamCond::Not(c) => write!(f, "not({c})"),
        }
    }
}

impl Grammar {
    pub fn new(name: Option<String>) -> Self {
        Grammar {
            name,
            parametric: false,
            symbols: vec![],
            symbol_by_name: HashMap::default(),
            symbol_count_cache: HashMap::default(),
        }
    }

    pub fn start(&self) -> SymIdx {
        self.symbols[0].idx
    }

    pub fn is_small(&self) -> bool {
        self.num_symbols() < 200
    }

    pub fn num_symbols(&self) -> usize {
        self.symbols.len()
    }

    fn sym_data(&self, sym: SymIdx) -> &Symbol {
        &self.symbols[sym.0 as usize]
    }

    fn sym_data_mut(&mut self, sym: SymIdx) -> &mut Symbol {
        &mut self.symbols[sym.0 as usize]
    }

    pub fn add_rule_ext(
        &mut self,
        lhs: SymIdx,
        condition: ParamCond,
        rhs: Vec<(SymIdx, ParamExpr)>,
    ) -> Result<()> {
        let sym = self.sym_data(lhs);
        ensure!(!sym.is_terminal(), "terminal symbol {}", sym.name);
        ensure!(
            condition.is_true() || sym.props.parametric,
            "non-parametric symbol {} with condition {}",
            sym.name,
            condition
        );
        for (s, p) in &rhs {
            let s = self.sym_data(*s);
            ensure!(
                s.props.parametric != p.is_null(),
                "symbol {} : {} with parametric {} and param {}",
                sym.name,
                s.name,
                s.props.parametric,
                p
            );
            ensure!(
                !p.needs_param() || sym.props.parametric,
                "symbol {} : {} with param {} needs param but is non-parametric",
                sym.name,
                s.name,
                p
            );
        }
        let sym = self.sym_data_mut(lhs);
        sym.rules.push(Rule {
            condition,
            lhs,
            rhs,
        });
        Ok(())
    }

    pub fn add_rule(&mut self, lhs: SymIdx, rhs: Vec<SymIdx>) -> Result<()> {
        let sym = self.sym_data_mut(lhs);
        ensure!(!sym.is_terminal(), "terminal symbol {}", sym.name);
        sym.rules.push(Rule {
            condition: ParamCond::True,
            lhs,
            rhs: rhs.into_iter().map(|r| (r, ParamExpr::Null)).collect(),
        });
        Ok(())
    }

    pub fn link_gen_grammar(&mut self, lhs: SymIdx, grammar: SymIdx) -> Result<()> {
        let sym = self.sym_data_mut(lhs);
        ensure!(
            sym.gen_grammar.is_some(),
            "no grammar options for {}",
            sym.name
        );
        ensure!(sym.rules.is_empty(), "symbol {} has rules", sym.name);
        self.add_rule(lhs, vec![grammar])?;
        Ok(())
    }

    pub fn check_empty_symbol_parametric_ok(&self, sym: SymIdx) -> Result<()> {
        let sym = self.sym_data(sym);
        ensure!(sym.rules.is_empty(), "symbol {} has rules", sym.name);
        ensure!(
            sym.gen_grammar.is_none(),
            "symbol {} has grammar options",
            sym.name
        );
        ensure!(sym.lexeme.is_none(), "symbol {} has lexeme", sym.name);
        Ok(())
    }

    pub fn check_empty_symbol(&self, sym: SymIdx) -> Result<()> {
        self.check_empty_symbol_parametric_ok(sym)?;
        let sym = self.sym_data(sym);
        ensure!(!sym.props.parametric, "symbol {} is parametric", sym.name);
        Ok(())
    }

    pub fn make_parametric(&mut self, lhs: SymIdx) -> Result<()> {
        self.check_empty_symbol(lhs)?;
        self.parametric = true;
        self.sym_data_mut(lhs).props.parametric = true;
        Ok(())
    }

    pub fn make_terminal(
        &mut self,
        lhs: SymIdx,
        lex: LexemeIdx,
        lexer_spec: &LexerSpec,
    ) -> Result<()> {
        self.check_empty_symbol(lhs)?;
        if lexer_spec.is_nullable(lex) {
            let wrap = self.fresh_symbol_ext(
                format!("rx_null_{}", self.sym_name(lhs)).as_str(),
                self.sym_data(lhs).props.for_wrapper(),
            );
            self.sym_data_mut(wrap).lexeme = Some(lex);
            self.add_rule(lhs, vec![wrap])?;
            self.add_rule(lhs, vec![])?;
        } else {
            self.sym_data_mut(lhs).lexeme = Some(lex);
        }
        Ok(())
    }

    pub fn set_temperature(&mut self, lhs: SymIdx, temp: f32) {
        self.sym_data_mut(lhs).props.temperature = temp;
    }

    pub fn make_gen_grammar(&mut self, lhs: SymIdx, data: GenGrammarOptions) -> Result<()> {
        self.check_empty_symbol(lhs)?;
        let sym = self.sym_data_mut(lhs);
        sym.gen_grammar = Some(data);
        Ok(())
    }

    pub fn apply_node_props(&mut self, lhs: SymIdx, props: NodeProps) {
        let sym = self.sym_data_mut(lhs);
        if let Some(max_tokens) = props.max_tokens {
            sym.props.max_tokens = max_tokens;
        }
        if let Some(capture_name) = props.capture_name {
            sym.props.capture_name = Some(capture_name);
        }
    }

    pub fn sym_props_mut(&mut self, sym: SymIdx) -> &mut SymbolProps {
        &mut self.sym_data_mut(sym).props
    }

    pub fn sym_props(&self, sym: SymIdx) -> &SymbolProps {
        &self.sym_data(sym).props
    }

    pub fn sym_name(&self, sym: SymIdx) -> &str {
        &self.symbols[sym.0 as usize].name
    }

    fn rule_to_string(&self, rule: &Rule, dot: Option<usize>, is_first: bool) -> String {
        let ldata = self.sym_data(rule.lhs());
        let dot_data = rule
            .rhs
            .get(dot.unwrap_or(0))
            .map(|(s, _)| &self.sym_data(*s).props);
        rule_to_string(
            if is_first {
                self.sym_name(rule.lhs())
            } else {
                ""
            },
            rule.rhs
                .iter()
                .map(|(s, p)| self.sym_data(*s).param_name(p))
                .collect(),
            dot,
            &ldata.props,
            dot_data,
            &rule.condition,
        )
    }

    fn copy_from(&mut self, other: &Grammar, sym: SymIdx) -> SymIdx {
        let sym_data = other.sym_data(sym);
        if let Some(sym) = self.symbol_by_name.get(&sym_data.name) {
            return *sym;
        }
        let r = self.fresh_symbol_ext(&sym_data.name, sym_data.props.clone());
        let self_sym = self.sym_data_mut(r);
        self_sym.lexeme = sym_data.lexeme;
        self_sym.gen_grammar = sym_data.gen_grammar.clone();
        r
    }

    fn rename(&mut self) {
        let name_repl = vec![("zero_or_more", "z"), ("one_or_more", "o")];
        for sym in &mut self.symbols {
            for (from, to) in &name_repl {
                if sym.name.starts_with(from) {
                    sym.name = format!("{}_{}", to, &sym.name[from.len()..]);
                }
            }
        }
        self.symbol_by_name = self
            .symbols
            .iter()
            .map(|s| (s.name.clone(), s.idx))
            .collect();
        assert!(self.symbols.len() == self.symbol_by_name.len());
    }

    fn is_special_symbol(&self, sym: &Symbol) -> bool {
        sym.idx == self.start() || sym.gen_grammar.is_some() || sym.props.is_special()
    }

    fn expand_shortcuts(&self) -> Self {
        let mut definition = vec![None; self.symbols.len()];
        for sym in &self.symbols {
            // don't inline special symbols (commit points, captures, ...) or start symbol
            if self.is_special_symbol(sym) {
                continue;
            }
            // 'sym : trg %if true' is the only rule -> we will replace sym with trg
            if sym.rules.len() == 1 && sym.rules[0].condition.is_true() {
                if let [(trg, param)] = sym.rules[0].rhs.as_slice() {
                    if param == &sym.props.neutral_param() {
                        uf_union(&mut definition, sym.idx, *trg)
                    }
                }
            }
        }

        uf_compress_all(&mut definition);

        // println!(
        //     "symbols: {:?}",
        //     self.symbols
        //         .iter()
        //         .map(|s| (s.idx, &s.name))
        //         .collect::<Vec<_>>()
        // );

        // println!("definition: {:?}", definition);

        let defn = |s: SymIdx| definition[s.as_usize()].unwrap_or(s);

        let mut the_user_of = vec![None; self.symbols.len()];
        for sym in &self.symbols {
            if definition[sym.idx.as_usize()].is_some() {
                continue;
            }
            let neutral_param = sym.props.neutral_param();
            for r in sym.rules.iter() {
                for (s, p) in &r.rhs {
                    let s = defn(*s);
                    let idx = s.as_usize();
                    if the_user_of[idx].is_none() && p == &neutral_param {
                        the_user_of[idx] = Some(r.lhs);
                    } else {
                        // use self-loop to indicate there are multiple users
                        the_user_of[idx] = Some(s);
                    }
                }
            }
        }

        // println!("the_user_of: {:?}", the_user_of);

        // clean up self loops to None
        for idx in 0..the_user_of.len() {
            if let Some(sym) = the_user_of[idx] {
                if sym.as_usize() == idx {
                    the_user_of[idx] = None;
                }
            }
        }

        // println!("the_user_of: {:?}", the_user_of);

        let mut repl = crate::HashMap::default();

        for sym in &self.symbols {
            if self.is_special_symbol(sym) {
                continue;
            }
            if sym.rules.len() == 1
                && the_user_of[sym.idx.as_usize()].is_some()
                && sym.rules[0].condition.is_true()
            {
                // we will eliminate sym.idx
                repl.insert(
                    sym.idx,
                    sym.rules[0]
                        .rhs
                        .iter()
                        .map(|e| (defn(e.0), e.1.clone()))
                        .collect::<Vec<_>>(),
                );
            }
        }

        // println!("repl: {:?}", repl);

        // these are keys of repl that may need to be used outside of repl itself
        let repl_roots = repl
            .keys()
            .filter(|s| !repl.contains_key(the_user_of[s.as_usize()].as_ref().unwrap()))
            .cloned()
            .collect::<Vec<_>>();

        // println!("repl_roots: {:?}", repl_roots);

        let mut to_eliminate = HashSet::from_iter(repl.keys().copied());
        for (idx, m) in definition.iter().enumerate() {
            if m.is_some() {
                let src = SymIdx(idx as u32);
                to_eliminate.insert(src);
            }
        }

        // the loop below creates a new map where the targets are already fully expanded
        let mut new_repl = HashMap::default();

        let mut stack = vec![];
        for sym in repl_roots {
            stack.push(vec![(sym, ParamExpr::Null)]);
            let mut res = vec![];
            while let Some(mut lst) = stack.pop() {
                while let Some((e, p)) = lst.pop() {
                    if let Some(mut lst2) = repl.remove(&e) {
                        assert!(p.is_null() || p.is_self_ref());
                        lst2.reverse();
                        if !lst.is_empty() {
                            stack.push(lst);
                        }
                        stack.push(lst2);
                        break;
                    }
                    assert!(!to_eliminate.contains(&e));
                    res.push((e, p));
                }
            }
            // println!("res: {:?} -> {:?}", sym, res);
            new_repl.insert(sym, res);
        }

        repl = new_repl;

        for (idx, m) in definition.iter().enumerate() {
            if let Some(trg) = m {
                if !to_eliminate.contains(trg) {
                    let param = self.sym_data(*trg).props.neutral_param();
                    repl.insert(SymIdx(idx as u32), vec![(*trg, param)]);
                }
            }
        }

        let mut outp = Grammar::new(self.name.clone());

        let start_data = self.sym_data(self.start());
        if start_data.is_terminal()
            || start_data.gen_grammar.is_some()
            || start_data.rules.iter().any(|r| r.rhs.is_empty())
        {
            let new_start = outp.fresh_symbol_ext(
                "_start_repl",
                SymbolProps {
                    grammar_id: start_data.props.grammar_id,
                    is_start: true,
                    ..Default::default()
                },
            );
            outp.add_rule(new_start, vec![SymIdx(1)]).unwrap();
        }

        for sym in &self.symbols {
            if repl.contains_key(&sym.idx) {
                continue;
            }
            let lhs = outp.copy_from(self, sym.idx);
            for rule in &sym.rules {
                let mut rhs = Vec::with_capacity(rule.rhs.len());
                for s in &rule.rhs {
                    if let Some(repl) = repl.get(&s.0) {
                        assert!(s.1.is_null() || s.1.is_self_ref());
                        rhs.extend(
                            repl.iter()
                                .map(|r| (outp.copy_from(self, r.0), r.1.clone())),
                        );
                    } else {
                        rhs.push((outp.copy_from(self, s.0), s.1.clone()));
                    }
                }
                outp.add_rule_ext(lhs, rule.condition.clone(), rhs).unwrap();
            }
        }
        outp
    }

    pub fn optimize(&self) -> Self {
        let mut r = self.expand_shortcuts();
        r = r.expand_shortcuts();
        r.rename();
        r
    }

    pub fn name(&self) -> Option<&str> {
        self.name.as_deref()
    }

    pub fn compile(&self, lexer_spec: LexerSpec, limits: &ParserLimits) -> Result<CGrammar> {
        CGrammar::from_grammar(self, lexer_spec, limits)
    }

    pub fn resolve_grammar_refs(
        &mut self,
        lexer_spec: &mut LexerSpec,
        ctx: &HashMap<GrammarId, (SymIdx, LexemeClass)>,
    ) -> Result<()> {
        let mut rules = vec![];
        let mut temperatures: HashMap<LexemeClass, f32> = HashMap::default();
        for sym in &self.symbols {
            if let Some(opts) = &sym.gen_grammar {
                let cls = if sym.rules.len() == 1 {
                    // fetch the class of already-resolved grammar
                    let rhs = &sym.rules[0].rhs[0];
                    assert!(rhs.1.is_null());
                    self.sym_data(rhs.0).props.grammar_id
                } else if let Some((idx, cls)) = ctx.get(&opts.grammar).cloned() {
                    rules.push((sym.idx, idx));
                    cls
                } else {
                    bail!("unknown grammar {}", opts.grammar);
                };

                let temp = opts.temperature.unwrap_or(0.0);
                if let Some(&existing) = temperatures.get(&cls) {
                    if existing != temp {
                        bail!(
                            "temperature mismatch for nested grammar {:?}: {} vs {}",
                            opts.grammar,
                            existing,
                            temp
                        );
                    }
                }
                temperatures.insert(cls, temp);
            }
        }
        for (lhs, rhs) in rules {
            self.add_rule(lhs, vec![rhs])?;
        }

        for sym in self.symbols.iter_mut() {
            if let Some(idx) = sym.lexeme {
                let spec = lexer_spec.lexeme_spec(idx);
                if let Some(&temp) = temperatures.get(&spec.class()) {
                    sym.props.temperature = temp;
                }
            }
        }

        Ok(())
    }

    fn fresh_name(&mut self, name0: &str) -> String {
        let mut name = name0.to_string();
        let mut idx = self.symbol_count_cache.get(&name).cloned().unwrap_or(2);
        // don't allow empty names
        while name.is_empty() || self.symbol_by_name.contains_key(&name) {
            name = format!("{name0}#{idx}");
            idx += 1;
        }
        self.symbol_count_cache.insert(name0.to_string(), idx);
        name
    }

    pub fn fresh_symbol_ext(&mut self, name0: &str, mut symprops: SymbolProps) -> SymIdx {
        let name = self.fresh_name(name0);

        let parametric = std::mem::take(&mut symprops.parametric);

        let idx = SymIdx(self.symbols.len() as u32);
        self.symbols.push(Symbol {
            name: name.clone(),
            lexeme: None,
            idx,
            rules: vec![],
            props: symprops,
            gen_grammar: None,
        });
        self.symbol_by_name.insert(name, idx);

        if parametric {
            self.make_parametric(idx).unwrap();
        }

        idx
    }

    pub fn stats(&self) -> String {
        let mut num_term = 0;
        let mut num_rules = 0;
        let mut num_non_term = 0;
        let mut size = 0;
        for sym in &self.symbols {
            size += 1;
            if sym.is_terminal() {
                num_term += 1;
            } else {
                size += 1;
                num_non_term += 1;
                num_rules += sym.rules.len();
                for r in &sym.rules {
                    size += r.rhs.len();
                }
            }
        }
        format!(
            "{num_term} terminals; {num_non_term} non-terminals with {num_rules} rules with {size} symbols"
        )
    }

    pub fn to_string(&self, lexer_spec: Option<&LexerSpec>) -> String {
        let mut outp = String::new();
        self.fmt_grammar(lexer_spec, &mut outp).unwrap();
        outp
    }

    fn fmt_grammar(&self, lexer_spec: Option<&LexerSpec>, f: &mut String) -> std::fmt::Result {
        use std::fmt::Write;
        writeln!(f, "Grammar:")?;
        for sym in &self.symbols {
            if let Some(ref opts) = sym.gen_grammar {
                writeln!(f, "{:15} ==> {:?}", sym.name, opts.grammar)?;
            }
            if false {
                if let Some(lx) = sym.lexeme {
                    write!(f, "{:15} ==>", sym.name)?;
                    if sym.props.temperature != 0.0 {
                        write!(f, " temp={:.2}", sym.props.temperature)?;
                    }
                    if let Some(lexer_spec) = lexer_spec {
                        writeln!(f, " {}", lexer_spec.lexeme_def_to_string(lx))?;
                    } else {
                        writeln!(f, " [{:?}]", lx.as_usize())?;
                    }
                }
            }
        }
        for sym in &self.symbols {
            if sym.rules.is_empty() {
                if sym.props.is_special() {
                    writeln!(f, "{:15} ⇦ {:?}  {}", sym.name, sym.lexeme, sym.props)?;
                }
            } else {
                for (idx, rule) in sym.rules.iter().enumerate() {
                    writeln!(f, "{}", self.rule_to_string(rule, None, idx == 0))?;
                }
            }
        }
        writeln!(f, "stats: {}\n", self.stats())?;
        Ok(())
    }

    pub fn rename_symbol(&mut self, idx: SymIdx, name: &str) {
        let curr_name = self.sym_name(idx).to_string();
        if name == curr_name {
            return; // nothing to do
        }
        self.symbol_by_name.remove(&curr_name);
        let name = self.fresh_name(name);
        self.sym_data_mut(idx).name = name.clone();
        self.symbol_by_name.insert(name, idx);
    }
}

impl Debug for Grammar {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.to_string(None))
    }
}

/// A unique ID of a symbol in the compiled grammar
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct CSymIdx(u16);

impl CSymIdx {
    pub const NULL: CSymIdx = CSymIdx(0);

    pub fn as_index(&self) -> usize {
        self.0 as usize
    }

    pub fn new_checked(idx: usize) -> Self {
        if idx >= u16::MAX as usize {
            panic!("CSymIdx out of range");
        }
        CSymIdx(idx as u16)
    }
}

/// This is a pointer into rhs_elements[] array, and represents a particular
/// element in the rhs of a rule (and thus by implication also a unique lhs).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct RhsPtr(u32);

impl RhsPtr {
    pub fn from_index(idx: u32) -> Self {
        RhsPtr(idx)
    }

    pub fn as_index(&self) -> usize {
        self.0 as usize
    }
}

#[derive(Clone)]
pub struct CSymbol {
    pub idx: CSymIdx,
    pub name: String,
    pub is_terminal: bool,
    pub is_nullable: bool,
    pub cond_nullable: Vec<ParamCond>, // this is OR
    pub props: SymbolProps,
    pub gen_grammar: Option<GenGrammarOptions>,
    // this points to the first element of rhs of each rule
    // note that null rules (with rhs == epsilon) are not stored
    pub rules: Vec<RhsPtr>,
    pub rules_cond: Vec<ParamCond>,
    pub sym_flags: SymFlags,
    pub lexeme: Option<LexemeIdx>,
}

impl CSymbol {
    fn short_name(&self) -> String {
        if let Some(lex) = self.lexeme {
            format!("[{}]", lex.as_usize())
        } else {
            self.name.clone()
        }
    }
    fn param_name(&self, param: &ParamExpr) -> String {
        let mut r = self.short_name();
        if param != &ParamExpr::Null {
            r.push_str(&format!("::{param}"));
        }
        r
    }
    fn iter_rules(&self) -> impl Iterator<Item = (RhsPtr, &ParamCond)> + '_ {
        self.rules.iter().copied().zip(self.rules_cond.iter())
    }
}

#[derive(Clone, Copy)]
pub struct SymFlags(u8);

impl SymFlags {
    const CAPTURE: u8 = 1 << 0;
    const GEN_GRAMMAR: u8 = 1 << 1;
    const STOP_CAPTURE: u8 = 1 << 2;
    const HAS_LEXEME: u8 = 1 << 3;

    fn from_csymbol(sym: &CSymbol) -> Self {
        let mut flags = 0;
        if sym.props.capture_name.is_some() {
            flags |= Self::CAPTURE;
        }
        if sym.gen_grammar.is_some() {
            flags |= Self::GEN_GRAMMAR;
        }
        if sym.props.stop_capture_name.is_some() {
            flags |= Self::STOP_CAPTURE;
        }
        if sym.lexeme.is_some() {
            flags |= Self::HAS_LEXEME;
        }
        SymFlags(flags)
    }

    #[inline(always)]
    pub fn capture(&self) -> bool {
        self.0 & Self::CAPTURE != 0
    }

    #[inline(always)]
    pub fn stop_capture(&self) -> bool {
        self.0 & Self::STOP_CAPTURE != 0
    }

    #[inline(always)]
    pub fn gen_grammar(&self) -> bool {
        self.0 & Self::GEN_GRAMMAR != 0
    }

    #[inline(always)]
    pub fn has_lexeme(&self) -> bool {
        self.0 & Self::HAS_LEXEME != 0
    }
}

#[derive(Clone)]
pub struct CGrammar {
    parametric: bool,
    start_symbol: CSymIdx,
    lexer_spec: LexerSpec,
    // indexed by CSymIdx
    symbols: Vec<CSymbol>,
    // This is rhs of rules, indexed by RhsPtr (CSymbol::rules)
    // Each rhs ends with CSymIdx::NULL
    // A pointer into this array represents an Earley item:
    // the dot is before rhs_elements[rhs_ptr]; when it points at CSymIdx::NULL, the item is complete
    rhs_elements: Vec<CSymIdx>,
    rhs_params: Vec<ParamExpr>,
    // given a pointer into rhs_elements[] (shifted by RULE_SHIFT),
    // this gives the index of the lhs symbol
    rhs_ptr_to_sym_idx: Vec<CSymIdx>,
    // this is cache, rhs_ptr_to_sym_flags[x] == symbols[rhs_ptr_to_sym_idx[x]].sym_flags
    rhs_ptr_to_sym_flags: Vec<SymFlags>,
}

const RULE_SHIFT: usize = 2;

impl CGrammar {
    pub fn parametric(&self) -> bool {
        self.parametric
    }

    pub fn lexer_spec(&self) -> &LexerSpec {
        &self.lexer_spec
    }

    pub fn sym_idx_lhs(&self, rule: RhsPtr) -> CSymIdx {
        self.rhs_ptr_to_sym_idx[rule.as_index() >> RULE_SHIFT]
    }

    pub fn sym_flags_lhs(&self, rule: RhsPtr) -> SymFlags {
        self.rhs_ptr_to_sym_flags[rule.as_index() >> RULE_SHIFT]
    }

    pub fn rule_rhs(&self, rule: RhsPtr) -> (&[CSymIdx], &[ParamExpr], usize) {
        let idx = rule.as_index();
        let mut start = idx - 1;
        while self.rhs_elements[start] != CSymIdx::NULL {
            start -= 1;
        }
        start += 1;
        let mut stop = idx;
        while self.rhs_elements[stop] != CSymIdx::NULL {
            stop += 1;
        }
        (
            &self.rhs_elements[start..stop],
            &self.rhs_params[start..stop],
            idx - start,
        )
    }

    pub fn sym_data(&self, sym: CSymIdx) -> &CSymbol {
        &self.symbols[sym.0 as usize]
    }

    fn sym_data_mut(&mut self, sym: CSymIdx) -> &mut CSymbol {
        &mut self.symbols[sym.0 as usize]
    }

    pub fn sym_idx_dot(&self, idx: RhsPtr) -> CSymIdx {
        self.rhs_elements[idx.0 as usize]
    }

    #[inline(always)]
    pub fn sym_data_dot(&self, idx: RhsPtr) -> &CSymbol {
        self.sym_data(self.sym_idx_dot(idx))
    }

    #[inline(always)]
    pub fn param_value_dot(&self, idx: RhsPtr) -> &ParamExpr {
        &self.rhs_params[idx.0 as usize]
    }

    pub fn start(&self) -> CSymIdx {
        self.start_symbol
    }

    pub fn rules_of(&self, sym: CSymIdx) -> &[RhsPtr] {
        &self.sym_data(sym).rules
    }

    fn add_symbol(&mut self, mut sym: CSymbol) -> CSymIdx {
        let idx = CSymIdx::new_checked(self.symbols.len());
        sym.idx = idx;
        self.symbols.push(sym);
        idx
    }

    fn from_grammar(
        grammar: &Grammar,
        lexer_spec: LexerSpec,
        limits: &ParserLimits,
    ) -> Result<Self> {
        let mut outp = CGrammar {
            start_symbol: CSymIdx::NULL, // replaced
            lexer_spec,
            parametric: grammar.parametric,
            symbols: vec![],
            rhs_elements: vec![CSymIdx::NULL], // make sure RhsPtr::NULL is invalid
            rhs_params: vec![ParamExpr::Null],
            rhs_ptr_to_sym_idx: vec![],
            rhs_ptr_to_sym_flags: vec![],
        };
        outp.add_symbol(CSymbol {
            idx: CSymIdx::NULL,
            name: "NULL".to_string(),
            is_terminal: true,
            is_nullable: false,
            cond_nullable: vec![],
            rules: vec![],
            rules_cond: vec![],
            props: SymbolProps::default(),
            sym_flags: SymFlags(0),
            gen_grammar: None,
            lexeme: None,
        });

        let mut sym_map = crate::HashMap::default();

        assert!(grammar.symbols.len() < u16::MAX as usize - 10);

        // lexemes go first
        for sym in grammar.symbols.iter() {
            if let Some(lx) = sym.lexeme {
                let new_idx = outp.add_symbol(CSymbol {
                    idx: CSymIdx::NULL,
                    name: sym.name.clone(),
                    is_terminal: true,
                    is_nullable: false,
                    cond_nullable: vec![],
                    rules: vec![],
                    rules_cond: vec![],
                    props: sym.props.clone(),
                    sym_flags: SymFlags(0),
                    gen_grammar: None,
                    lexeme: Some(lx),
                });
                sym_map.insert(sym.idx, new_idx);
            }
        }

        for sym in &grammar.symbols {
            if sym.lexeme.is_some() {
                continue;
            }
            let cidx = outp.add_symbol(CSymbol {
                idx: CSymIdx::NULL,
                name: sym.name.clone(),
                is_terminal: false,
                is_nullable: false,
                cond_nullable: vec![],
                rules: vec![],
                rules_cond: vec![],
                props: sym.props.clone(),
                sym_flags: SymFlags(0),
                gen_grammar: sym.gen_grammar.clone(),
                lexeme: None,
            });
            sym_map.insert(sym.idx, cidx);
        }

        outp.start_symbol = sym_map[&grammar.start()];
        for sym in &grammar.symbols {
            if sym.is_terminal() {
                assert!(sym.rules.is_empty());
                continue;
            }
            let idx = sym_map[&sym.idx];
            for rule in &sym.rules {
                // we handle the empty rule separately via is_nullable field
                if rule.rhs.is_empty() {
                    let csym = outp.sym_data_mut(idx);
                    if rule.condition.is_true() {
                        csym.is_nullable = true;
                    } else {
                        csym.cond_nullable.push(rule.condition.clone());
                    }
                    continue;
                }
                let curr = RhsPtr(outp.rhs_elements.len().try_into().unwrap());
                let csym = outp.sym_data_mut(idx);
                csym.rules.push(curr);
                csym.rules_cond.push(rule.condition.clone());
                // outp.rules.push(idx);
                for (r, e) in &rule.rhs {
                    outp.rhs_elements.push(sym_map[r]);
                    outp.rhs_params.push(e.clone());
                }
                outp.rhs_elements.push(CSymIdx::NULL);
                outp.rhs_params.push(ParamExpr::Null);
            }
            while outp.rhs_elements.len() % (1 << RULE_SHIFT) != 0 {
                outp.rhs_elements.push(CSymIdx::NULL);
                outp.rhs_params.push(ParamExpr::Null);
            }
            let rlen = outp.rhs_elements.len() >> RULE_SHIFT;
            while outp.rhs_ptr_to_sym_idx.len() < rlen {
                outp.rhs_ptr_to_sym_idx.push(idx);
            }
        }

        for sym in &mut outp.symbols {
            sym.sym_flags = SymFlags::from_csymbol(sym);
            if sym.is_nullable {
                // no need to keep conditions for always nullable symbols
                sym.cond_nullable.clear();
            }
        }

        outp.rhs_ptr_to_sym_flags = outp
            .rhs_ptr_to_sym_idx
            .iter()
            .map(|s| outp.sym_data(*s).sym_flags)
            .collect();

        outp.set_nullable();
        if outp.parametric {
            ParametricNullableCtx::new(limits).set_cond_nullable(&mut outp)?;
            debug!("parametric grammar:\n{:?}", outp);
        }

        Ok(outp)
    }

    fn set_nullable(&mut self) {
        loop {
            let mut to_null = vec![];
            for sym in &self.symbols {
                if sym.is_nullable {
                    continue;
                }
                for (rule, cond) in sym.iter_rules() {
                    if *cond != ParamCond::True {
                        continue; // skip rules with conditions
                    }
                    if self
                        .rule_rhs(rule)
                        .0
                        .iter()
                        .all(|elt| self.sym_data(*elt).is_nullable)
                    {
                        to_null.push(sym.idx);
                    }
                }
            }
            if to_null.is_empty() {
                break;
            }
            for sym in to_null {
                self.sym_data_mut(sym).is_nullable = true;
            }
        }
    }

    pub fn sym_name(&self, sym: CSymIdx) -> &str {
        &self.symbols[sym.0 as usize].name
    }

    pub fn null_rule(&self, sym: CSymIdx) -> Option<String> {
        let symdata = self.sym_data(sym);
        if !symdata.is_nullable && symdata.cond_nullable.is_empty() {
            None
        } else {
            let sym = symdata.idx;
            let lhs = self.sym_name(sym);
            let cond = if symdata.is_nullable {
                ParamCond::True
            } else {
                symdata
                    .cond_nullable
                    .iter()
                    .skip(1)
                    .fold(symdata.cond_nullable[0].clone(), |acc, c| {
                        ParamCond::Or(Box::new(acc), Box::new(c.clone()))
                    })
            };
            Some(rule_to_string(
                lhs,
                vec![],
                None,
                &symdata.props,
                None,
                &cond,
            ))
        }
    }

    pub fn rule_to_string(&self, rule: RhsPtr, cond: &ParamCond) -> String {
        let sym = self.sym_idx_lhs(rule);
        let symdata = self.sym_data(sym);
        let lhs = self.sym_name(sym);
        let (rhs, rhs_p, dot) = self.rule_rhs(rule);
        let dot_prop = if !rhs.is_empty() {
            Some(&self.sym_data_dot(rule).props)
        } else {
            None
        };
        rule_to_string(
            lhs,
            rhs.iter()
                .zip(rhs_p.iter())
                .map(|(s, p)| self.sym_data(*s).param_name(p))
                .collect(),
            Some(dot),
            &symdata.props,
            dot_prop,
            cond,
        )
    }
}

type Clause = Vec<HashId<ParamCond>>;
type Dnf = Vec<HashId<Clause>>;
struct ParametricNullableCtx {
    atoms: HashCons<ParamCond>,
    clauses: HashCons<Clause>,
    fuel: u64,
}

impl ParametricNullableCtx {
    fn simplify_dnf(&mut self, dnf: &mut Dnf) -> Result<()> {
        // if we do something smarter here (like clause subsumption),
        // make sure to update "len0 != n.len()" check in the fix-point loop
        self.sub_fuel(dnf.len())?;
        dnf.sort_unstable();
        dnf.dedup();
        Ok(())
    }

    fn sub_fuel(&mut self, n: usize) -> Result<()> {
        if n > self.fuel as usize {
            bail!("DNF too large, fuel exhausted");
        }
        self.fuel -= n as u64;
        Ok(())
    }

    fn dnf_and(&mut self, a: Dnf, b: Dnf) -> Result<Dnf> {
        if a.is_empty() || b.is_empty() {
            return Ok(vec![]);
        }
        let mut res = vec![];
        for a in &a {
            for b in &b {
                let mut c = self.clauses.get(*a).to_vec();
                c.extend_from_slice(self.clauses.get(*b));
                self.sub_fuel(c.len())?;
                c.sort_unstable();
                c.dedup();
                res.push(self.clauses.insert(c));
            }
        }
        self.simplify_dnf(&mut res)?;
        Ok(res)
    }

    fn dnf_or(&mut self, mut a: Dnf, mut b: Dnf) -> Result<Dnf> {
        a.append(&mut b);
        self.simplify_dnf(&mut a)?;
        Ok(a)
    }

    fn dnf(&mut self, cond: &ParamCond, neg: bool) -> Result<Dnf> {
        let r = match cond {
            ParamCond::True => vec![self.clauses.insert(vec![])],
            ParamCond::NE(_, _)
            | ParamCond::EQ(_, _)
            | ParamCond::LE(_, _)
            | ParamCond::LT(_, _)
            | ParamCond::GE(_, _)
            | ParamCond::GT(_, _)
            | ParamCond::BitCountNE(_, _)
            | ParamCond::BitCountEQ(_, _)
            | ParamCond::BitCountLE(_, _)
            | ParamCond::BitCountLT(_, _)
            | ParamCond::BitCountGE(_, _)
            | ParamCond::BitCountGT(_, _) => {
                let cond = if neg {
                    ParamCond::Not(Box::new(cond.clone()))
                } else {
                    cond.clone()
                };
                let a = self.atoms.insert(cond);
                vec![self.clauses.insert(vec![a])]
            }
            ParamCond::And(a, b) | ParamCond::Or(a, b) => {
                let a = self.dnf(a, neg)?;
                let b = self.dnf(b, neg)?;
                if neg != matches!(cond, ParamCond::And(_, _)) {
                    self.dnf_and(a, b)?
                } else {
                    self.dnf_or(a, b)?
                }
            }
            ParamCond::Not(cond) => self.dnf(cond, !neg)?,
        };
        Ok(r)
    }

    fn new(limits: &ParserLimits) -> ParametricNullableCtx {
        ParametricNullableCtx {
            atoms: HashCons::default(),
            clauses: HashCons::default(),
            fuel: limits.initial_lexer_fuel,
        }
    }

    fn and_list(&self, lst: &[HashId<ParamCond>]) -> ParamCond {
        if lst.is_empty() {
            ParamCond::True
        } else if lst.len() == 1 {
            self.atoms.get(lst[0]).clone()
        } else {
            let mid = lst.len() / 2;
            ParamCond::And(
                Box::new(self.and_list(&lst[0..mid])),
                Box::new(self.and_list(&lst[mid..])),
            )
        }
    }

    fn set_cond_nullable(&mut self, grm: &mut CGrammar) -> Result<()> {
        let mut null_cond: Vec<Dnf> = grm
            .symbols
            .iter()
            .map(|s| {
                let mut dnf = vec![];
                for c in s.cond_nullable.iter() {
                    let mut dnf2 = self.dnf(c, false)?;
                    dnf.append(&mut dnf2);
                }
                self.simplify_dnf(&mut dnf)?;
                Ok(dnf)
            })
            .collect::<Result<Vec<_>>>()?;

        loop {
            let mut num_added = 0;
            for sym in &grm.symbols {
                if sym.is_nullable || sym.rules.is_empty() {
                    continue;
                }
                for (rule, cond) in sym.iter_rules() {
                    let mut new_dnf = vec![];
                    if grm.rule_rhs(rule).0.iter().all(|&elt| {
                        elt != sym.idx
                            && (grm.sym_data(elt).is_nullable
                                || !null_cond[elt.as_index()].is_empty())
                    }) {
                        // everything in this rule is potentially nullable
                        let mut dnf = self.dnf(cond, false)?;
                        for &elt in grm.rule_rhs(rule).0.iter() {
                            if grm.sym_data(elt).is_nullable {
                                continue;
                            }
                            dnf = self.dnf_and(dnf, null_cond[elt.as_index()].clone())?;
                        }
                        new_dnf.append(&mut dnf);
                    }
                    if new_dnf.is_empty() {
                        continue; // no new nullable rules
                    }
                    let n = &mut null_cond[sym.idx.as_index()];
                    let len0 = n.len();
                    n.append(&mut new_dnf);
                    self.simplify_dnf(n)?;
                    if len0 != n.len() {
                        num_added += 1;
                    }
                }
            }

            if num_added == 0 {
                break; // no more nullable rules
            }
        }

        for sym in &mut grm.symbols {
            let dnf = &null_cond[sym.idx.as_index()];
            if !dnf.is_empty() {
                sym.cond_nullable = dnf
                    .iter()
                    .map(|c| self.and_list(self.clauses.get(*c)))
                    .collect();
            } else {
                assert!(sym.cond_nullable.is_empty());
            }
        }

        Ok(())
    }
}

impl Debug for CGrammar {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for s in &self.symbols {
            if let Some(ln) = self.null_rule(s.idx) {
                writeln!(f, "{ln}")?;
            }
            for (r, cond) in s.iter_rules() {
                writeln!(f, "{}", self.rule_to_string(r, cond))?;
            }
        }
        Ok(())
    }
}

fn rule_to_string(
    lhs: &str,
    mut rhs: Vec<String>,
    dot: Option<usize>,
    props: &SymbolProps,
    _dot_props: Option<&SymbolProps>,
    cond: &ParamCond,
) -> String {
    if rhs.is_empty() {
        rhs.push("ϵ".to_string());
        if dot == Some(0) {
            rhs.push("•".to_string());
        }
    } else if let Some(dot) = dot {
        rhs.insert(dot, "•".to_string());
    }
    let lhs = if props.parametric && !lhs.is_empty() {
        format!("{lhs}::_")
    } else {
        lhs.to_string()
    };
    format!(
        "{:15} ⇦ {}{}  {}",
        lhs,
        rhs.join(" "),
        if cond.is_true() {
            String::new()
        } else {
            format!("   %if {cond} ")
        },
        props
    )
}

fn uf_find(map: &mut [Option<SymIdx>], e: SymIdx) -> SymIdx {
    let mut root = e;
    let mut steps = 0;
    while let Some(q) = map[root.as_usize()] {
        root = q;
        steps += 1;
    }
    if steps > 1 {
        let mut p = e;
        assert!(p != root);
        while let Some(q) = map[p.as_usize()].replace(root) {
            if q == root {
                break;
            }
            p = q;
        }
    }
    root
}

fn uf_union(map: &mut [Option<SymIdx>], mut a: SymIdx, mut b: SymIdx) {
    a = uf_find(map, a);
    b = uf_find(map, b);
    if a != b {
        let r = map[a.as_usize()].replace(b);
        assert!(r.is_none());
    }
}

fn uf_compress_all(map: &mut [Option<SymIdx>]) {
    for idx in 0..map.len() {
        if map[idx].is_some() {
            uf_find(map, SymIdx(idx as u32));
        }
    }
}
