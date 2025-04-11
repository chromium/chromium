use super::lexerspec::{LexemeClass, LexemeIdx, LexerSpec};
use crate::api::{GenGrammarOptions, GrammarId, NodeProps};
use crate::{HashMap, HashSet};
use anyhow::{bail, ensure, Result};
use std::fmt::Display;
use std::{fmt::Debug, hash::Hash};

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
}

#[derive(Debug, Clone, PartialEq)]
pub struct SymbolProps {
    pub max_tokens: usize,
    pub capture_name: Option<String>,
    pub stop_capture_name: Option<String>,
    pub temperature: f32,
    pub grammar_id: LexemeClass,
    pub is_start: bool,
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
        SymbolProps {
            max_tokens: self.max_tokens,
            capture_name: None,
            stop_capture_name: None,
            temperature: self.temperature,
            grammar_id: self.grammar_id,
            is_start: false,
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
    rhs: Vec<SymIdx>,
}

impl Rule {
    fn lhs(&self) -> SymIdx {
        self.lhs
    }
}

#[derive(Clone)]
pub struct Grammar {
    name: Option<String>,
    symbols: Vec<Symbol>,
    symbol_count_cache: HashMap<String, usize>,
    symbol_by_name: HashMap<String, SymIdx>,
}

impl Grammar {
    pub fn new(name: Option<String>) -> Self {
        Grammar {
            name,
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

    pub fn add_rule(&mut self, lhs: SymIdx, rhs: Vec<SymIdx>) -> Result<()> {
        let sym = self.sym_data_mut(lhs);
        ensure!(!sym.is_terminal(), "terminal symbol {}", sym.name);
        sym.rules.push(Rule { lhs, rhs });
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

    pub fn check_empty_symbol(&self, sym: SymIdx) -> Result<()> {
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
            .map(|s| &self.sym_data(*s).props);
        rule_to_string(
            if is_first {
                self.sym_name(rule.lhs())
            } else {
                ""
            },
            rule.rhs
                .iter()
                .map(|s| self.sym_data(*s).short_name())
                .collect(),
            dot,
            &ldata.props,
            dot_data,
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
            if sym.rules.len() == 1 && sym.rules[0].rhs.len() == 1 {
                uf_union(&mut definition, sym.idx, sym.rules[0].rhs[0]);
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

        let mut the_user_of = vec![None; self.symbols.len()];
        for sym in &self.symbols {
            if definition[sym.idx.as_usize()].is_some() {
                continue;
            }
            for r in sym.rules.iter() {
                for s in &r.rhs {
                    let s = definition[s.as_usize()].unwrap_or(*s);
                    let idx = s.as_usize();
                    if the_user_of[idx].is_none() {
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
            if sym.rules.len() == 1 && the_user_of[sym.idx.as_usize()].is_some() {
                // we will eliminate sym.idx
                repl.insert(
                    sym.idx,
                    sym.rules[0]
                        .rhs
                        .iter()
                        .map(|e| definition[e.as_usize()].unwrap_or(*e))
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

        let mut new_repl = HashMap::default();

        let mut stack = vec![];
        for sym in repl_roots {
            stack.push(vec![sym]);
            let mut res = vec![];
            while let Some(mut lst) = stack.pop() {
                while let Some(e) = lst.pop() {
                    if let Some(mut lst2) = repl.remove(&e) {
                        lst2.reverse();
                        if !lst.is_empty() {
                            stack.push(lst);
                        }
                        stack.push(lst2);
                        break;
                    }
                    assert!(!to_eliminate.contains(&e));
                    res.push(e);
                }
            }
            // println!("res: {:?} -> {:?}", sym, res);
            new_repl.insert(sym, res);
        }

        repl = new_repl;

        for (idx, m) in definition.iter().enumerate() {
            if let Some(trg) = m {
                if !to_eliminate.contains(trg) {
                    repl.insert(SymIdx(idx as u32), vec![*trg]);
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
                    if let Some(repl) = repl.get(s) {
                        rhs.extend(repl.iter().map(|s| outp.copy_from(self, *s)));
                    } else {
                        rhs.push(outp.copy_from(self, *s));
                    }
                }
                outp.add_rule(lhs, rhs).unwrap();
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

    pub fn compile(&self, lexer_spec: LexerSpec) -> CGrammar {
        CGrammar::from_grammar(self, lexer_spec)
    }

    pub fn resolve_grammar_refs(
        &mut self,
        lexer_spec: &mut LexerSpec,
        ctx: &HashMap<GrammarId, (SymIdx, LexemeClass)>,
    ) -> Result<()> {
        let mut rules = vec![];
        let mut temperatures: HashMap<LexemeClass, f32> = HashMap::default();
        for sym in &mut self.symbols {
            if let Some(opts) = &sym.gen_grammar {
                if sym.rules.len() == 1 {
                    // ignore already-resolved grammars
                    continue;
                }
                if let Some((idx, cls)) = ctx.get(&opts.grammar).cloned() {
                    rules.push((sym.idx, idx));
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
                } else {
                    bail!("unknown grammar {}", opts.grammar);
                }
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

    pub fn apply_props(&mut self, sym: SymIdx, props: SymbolProps) {
        let sym = self.sym_data_mut(sym);
        if props.is_special() {
            assert!(!sym.is_terminal(), "special terminal");
        }
        sym.props = props;
    }

    pub fn fresh_symbol_ext(&mut self, name0: &str, symprops: SymbolProps) -> SymIdx {
        let mut name = name0.to_string();
        let mut idx = self.symbol_count_cache.get(&name).cloned().unwrap_or(2);
        // don't allow empty names
        while name.is_empty() || self.symbol_by_name.contains_key(&name) {
            name = format!("{}#{}", name0, idx);
            idx += 1;
        }
        self.symbol_count_cache.insert(name0.to_string(), idx);

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
            "{} terminals; {} non-terminals with {} rules with {} symbols",
            num_term, num_non_term, num_rules, size
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
    pub props: SymbolProps,
    pub gen_grammar: Option<GenGrammarOptions>,
    // this points to the first element of rhs of each rule
    // note that null rules (with rhs == epsilon) are not stored
    pub rules: Vec<RhsPtr>,
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
    start_symbol: CSymIdx,
    lexer_spec: LexerSpec,
    // indexed by CSymIdx
    symbols: Vec<CSymbol>,
    // This is rhs of rules, indexed by RhsPtr (CSymbol::rules)
    // Each rhs ends with CSymIdx::NULL
    // A pointer into this array represents an Earley item:
    // the dot is before rhs_elements[rhs_ptr]; when it points at CSymIdx::NULL, the item is complete
    rhs_elements: Vec<CSymIdx>,
    // given a pointer into rhs_elements[] (shifted by RULE_SHIFT),
    // this gives the index of the lhs symbol
    rhs_ptr_to_sym_idx: Vec<CSymIdx>,
    // this is cache, rhs_ptr_to_sym_flags[x] == symbols[rhs_ptr_to_sym_idx[x]].sym_flags
    rhs_ptr_to_sym_flags: Vec<SymFlags>,
}

const RULE_SHIFT: usize = 2;

impl CGrammar {
    pub fn lexer_spec(&self) -> &LexerSpec {
        &self.lexer_spec
    }

    pub fn sym_idx_lhs(&self, rule: RhsPtr) -> CSymIdx {
        self.rhs_ptr_to_sym_idx[rule.as_index() >> RULE_SHIFT]
    }

    pub fn sym_flags_lhs(&self, rule: RhsPtr) -> SymFlags {
        self.rhs_ptr_to_sym_flags[rule.as_index() >> RULE_SHIFT]
    }

    pub fn rule_rhs(&self, rule: RhsPtr) -> (&[CSymIdx], usize) {
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
        (&self.rhs_elements[start..stop], idx - start)
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

    fn from_grammar(grammar: &Grammar, lexer_spec: LexerSpec) -> Self {
        let mut outp = CGrammar {
            start_symbol: CSymIdx::NULL, // replaced
            lexer_spec,
            symbols: vec![],
            rhs_elements: vec![CSymIdx::NULL], // make sure RhsPtr::NULL is invalid
            rhs_ptr_to_sym_idx: vec![],
            rhs_ptr_to_sym_flags: vec![],
        };
        outp.add_symbol(CSymbol {
            idx: CSymIdx::NULL,
            name: "NULL".to_string(),
            is_terminal: true,
            is_nullable: false,
            rules: vec![],
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
                    rules: vec![],
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
                is_nullable: sym.rules.iter().any(|r| r.rhs.is_empty()),
                rules: vec![],
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
                    continue;
                }
                let curr = RhsPtr(outp.rhs_elements.len().try_into().unwrap());
                outp.sym_data_mut(idx).rules.push(curr);
                // outp.rules.push(idx);
                for r in &rule.rhs {
                    outp.rhs_elements.push(sym_map[r]);
                }
                outp.rhs_elements.push(CSymIdx::NULL);
            }
            while outp.rhs_elements.len() % (1 << RULE_SHIFT) != 0 {
                outp.rhs_elements.push(CSymIdx::NULL);
            }
            let rlen = outp.rhs_elements.len() >> RULE_SHIFT;
            while outp.rhs_ptr_to_sym_idx.len() < rlen {
                outp.rhs_ptr_to_sym_idx.push(idx);
            }
        }

        for sym in &mut outp.symbols {
            sym.sym_flags = SymFlags::from_csymbol(sym);
        }

        outp.rhs_ptr_to_sym_flags = outp
            .rhs_ptr_to_sym_idx
            .iter()
            .map(|s| outp.sym_data(*s).sym_flags)
            .collect();

        loop {
            let mut to_null = vec![];
            for sym in &outp.symbols {
                if sym.is_nullable {
                    continue;
                }
                for rule in sym.rules.iter() {
                    if outp
                        .rule_rhs(*rule)
                        .0
                        .iter()
                        .all(|elt| outp.sym_data(*elt).is_nullable)
                    {
                        to_null.push(sym.idx);
                    }
                }
            }
            if to_null.is_empty() {
                break;
            }
            for sym in to_null {
                outp.sym_data_mut(sym).is_nullable = true;
            }
        }

        outp
    }

    pub fn sym_name(&self, sym: CSymIdx) -> &str {
        &self.symbols[sym.0 as usize].name
    }

    pub fn rule_to_string(&self, rule: RhsPtr) -> String {
        let sym = self.sym_idx_lhs(rule);
        let symdata = self.sym_data(sym);
        let lhs = self.sym_name(sym);
        let (rhs, dot) = self.rule_rhs(rule);
        let dot_prop = if !rhs.is_empty() {
            Some(&self.sym_data_dot(rule).props)
        } else {
            None
        };
        rule_to_string(
            lhs,
            rhs.iter().map(|s| self.sym_data(*s).short_name()).collect(),
            Some(dot),
            &symdata.props,
            dot_prop,
        )
    }
}

impl Debug for CGrammar {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for s in &self.symbols {
            for r in &s.rules {
                writeln!(f, "{}", self.rule_to_string(*r))?;
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
) -> String {
    if rhs.is_empty() {
        rhs.push("ϵ".to_string());
        if dot == Some(0) {
            rhs.push("•".to_string());
        }
    } else if let Some(dot) = dot {
        rhs.insert(dot, "•".to_string());
    }
    format!("{:15} ⇦ {}  {}", lhs, rhs.join(" "), props)
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
        while let Some(q) = std::mem::replace(&mut map[p.as_usize()], Some(root)) {
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
        let r = std::mem::replace(&mut map[a.as_usize()], Some(b));
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
