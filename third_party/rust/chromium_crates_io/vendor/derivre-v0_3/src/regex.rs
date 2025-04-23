use std::fmt::Debug;

use crate::HashSet;
use anyhow::Result;

use crate::{
    ast::{ExprRef, ExprSet, NextByte},
    bytecompress::ByteCompressor,
    deriv::DerivCache,
    hashcons::VecHashCons,
    nextbyte::NextByteCache,
    pp::PrettyPrinter,
    relevance::RelevanceCache,
};

const DEBUG: bool = false;

macro_rules! debug {
    ($($arg:tt)*) => {
        if DEBUG {
            eprintln!($($arg)*);
        }
    };
}

#[derive(Clone, Copy, PartialEq, Eq, Hash)]
pub struct StateID(u32);

impl StateID {
    // DEAD state corresponds to empty vector
    pub const DEAD: StateID = StateID::new(0);
    // MISSING state corresponds to yet not computed entries in the state table
    pub const MISSING: StateID = StateID::new(1);

    pub fn as_usize(&self) -> usize {
        (self.0 >> 1) as usize
    }

    pub fn as_u32(&self) -> u32 {
        self.0 >> 1
    }

    pub fn is_valid(&self) -> bool {
        *self != Self::MISSING
    }

    #[inline(always)]
    pub fn is_dead(&self) -> bool {
        *self == Self::DEAD
    }

    #[inline(always)]
    pub fn has_lowest_match(&self) -> bool {
        (self.0 & 1) == 1
    }

    pub fn _set_lowest_match(self) -> Self {
        Self(self.0 | 1)
    }

    pub const fn new(id: u32) -> Self {
        Self(id << 1)
    }

    pub fn new_hash_cons() -> VecHashCons {
        let mut rx_sets = VecHashCons::new();
        let id = rx_sets.insert(&[]);
        assert!(id == StateID::DEAD.as_u32());
        let id = rx_sets.insert(&[ExprRef::INVALID.as_u32()]);
        assert!(id == StateID::MISSING.as_u32());
        rx_sets
    }
}

impl Debug for StateID {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if *self == StateID::DEAD {
            write!(f, "StateID(DEAD)")
        } else if *self == StateID::MISSING {
            write!(f, "StateID(MISSING)")
        } else {
            write!(f, "StateID({},{})", self.0 >> 1, self.0 & 1)
        }
    }
}

#[derive(Clone)]
pub struct AlphabetInfo {
    mapping: [u8; 256],
    size: usize,
}

#[derive(Clone)]
pub struct Regex {
    exprs: ExprSet,
    deriv: DerivCache,
    next_byte: NextByteCache,
    relevance: RelevanceCache,
    alpha: AlphabetInfo,
    initial: StateID,
    rx_sets: VecHashCons,
    state_table: Vec<StateID>,
    state_descs: Vec<StateDesc>,
    num_transitions: usize,
    num_ast_nodes: usize,
    max_states: usize,
}

#[derive(Clone, Debug, Default)]
struct StateDesc {
    lookahead_len: Option<Option<usize>>,
    next_byte: Option<NextByte>,
}

// public implementation
impl Regex {
    pub fn new(rx: &str) -> Result<Self> {
        let parser = regex_syntax::ParserBuilder::new().build();
        Self::new_with_parser(parser, rx)
    }

    pub fn new_with_parser(parser: regex_syntax::Parser, rx: &str) -> Result<Self> {
        let mut exprset = ExprSet::new(256);
        let rx = exprset.parse_expr(parser.clone(), rx, false)?;
        Self::new_with_exprset(exprset, rx, u64::MAX)
    }

    pub fn alpha(&self) -> &AlphabetInfo {
        &self.alpha
    }

    pub fn initial_state(&mut self) -> StateID {
        self.initial
    }

    pub fn always_empty(&mut self) -> bool {
        self.initial_state().is_dead()
    }

    pub fn is_accepting(&mut self, state: StateID) -> bool {
        self.lookahead_len_for_state(state).is_some()
    }

    fn resolve(rx_sets: &VecHashCons, state: StateID) -> ExprRef {
        ExprRef::new(rx_sets.get(state.as_u32())[0])
    }

    pub fn lookahead_len_for_state(&mut self, state: StateID) -> Option<usize> {
        if state == StateID::DEAD || state == StateID::MISSING {
            return None;
        }
        let desc = &mut self.state_descs[state.as_usize()];
        if let Some(len) = desc.lookahead_len {
            return len;
        }
        let expr = Self::resolve(&self.rx_sets, state);
        let mut res = None;
        if self.exprs.is_nullable(expr) {
            res = Some(self.exprs.lookahead_len(expr).unwrap_or(0));
        }
        desc.lookahead_len = Some(res);
        res
    }

    #[inline(always)]
    pub fn transition(&mut self, state: StateID, b: u8) -> StateID {
        let idx = self.alpha.map_state(state, b);
        let new_state = self.state_table[idx];
        if new_state != StateID::MISSING {
            new_state
        } else {
            let new_state = self.transition_inner(state, b);
            self.num_transitions += 1;
            self.state_table[idx] = new_state;
            new_state
        }
    }

    pub fn transition_bytes(&mut self, state: StateID, bytes: &[u8]) -> StateID {
        let mut state = state;
        for &b in bytes {
            state = self.transition(state, b);
        }
        state
    }

    pub fn is_match(&mut self, text: &str) -> bool {
        self.lookahead_len(text).is_some()
    }

    pub fn is_match_bytes(&mut self, text: &[u8]) -> bool {
        self.lookahead_len_bytes(text).is_some()
    }

    pub fn lookahead_len_bytes(&mut self, text: &[u8]) -> Option<usize> {
        let mut state = self.initial_state();
        for b in text {
            let b = *b;
            let new_state = self.transition(state, b);
            debug!("b: {:?} --{:?}--> {:?}", state, b as char, new_state);
            state = new_state;
            if state == StateID::DEAD {
                return None;
            }
        }
        self.lookahead_len_for_state(state)
    }

    pub fn lookahead_len(&mut self, text: &str) -> Option<usize> {
        self.lookahead_len_bytes(text.as_bytes())
    }

    /// Estimate the size of the regex tables in bytes.
    pub fn num_bytes(&self) -> usize {
        self.exprs.num_bytes()
            + self.deriv.num_bytes()
            + self.next_byte.num_bytes()
            + self.state_descs.len() * 100
            + self.state_table.len() * std::mem::size_of::<StateID>()
            + self.rx_sets.num_bytes()
    }

    pub fn cost(&self) -> u64 {
        self.exprs.cost()
    }

    /// Check if the there is only one transition out of state.
    /// This is an approximation - see docs for NextByte.
    pub fn next_byte(&mut self, state: StateID) -> NextByte {
        if state == StateID::DEAD || state == StateID::MISSING {
            return NextByte::Dead;
        }

        let desc = &mut self.state_descs[state.as_usize()];
        if let Some(next_byte) = desc.next_byte {
            return next_byte;
        }

        let e = Self::resolve(&self.rx_sets, state);
        let next_byte = self.next_byte.next_byte(&self.exprs, e);
        desc.next_byte = Some(next_byte);
        next_byte
    }

    pub fn stats(&self) -> String {
        format!(
            "regexp: {} nodes (+ {} derived via {} derivatives), states: {}; transitions: {}; bytes: {}; alphabet size: {}",
            self.num_ast_nodes,
            self.exprs.len() - self.num_ast_nodes,
            self.deriv.num_deriv,
            self.state_descs.len(),
            self.num_transitions,
            self.num_bytes(),
            self.alpha.len(),
        )
    }

    pub fn dfa(&mut self) -> Vec<u8> {
        let mut used = HashSet::default();
        let mut designated_bytes = vec![];
        for b in 0..=255 {
            let m = self.alpha.map(b);
            if !used.contains(&m) {
                used.insert(m);
                designated_bytes.push(b);
            }
        }

        let mut stack = vec![self.initial_state()];
        let mut visited = HashSet::default();
        while let Some(state) = stack.pop() {
            for b in &designated_bytes {
                let new_state = self.transition(state, *b);
                if !visited.contains(&new_state) {
                    stack.push(new_state);
                    visited.insert(new_state);
                    assert!(visited.len() < 250);
                }
            }
        }

        assert!(!self.state_table.contains(&StateID::MISSING));
        let mut res = self.alpha.mapping.to_vec();
        res.extend(self.state_table.iter().map(|s| s.as_u32() as u8));
        res
    }

    pub fn print_state_table(&self) {
        for (state, row) in self.state_table.chunks(self.alpha.len()).enumerate() {
            println!("state: {}", state);
            for (b, &new_state) in row.iter().enumerate() {
                println!("  s{:?} -> {:?}", b, new_state);
            }
        }
    }
}

impl AlphabetInfo {
    pub fn from_exprset(exprset: ExprSet, rx_list: &[ExprRef]) -> (Self, ExprSet, Vec<ExprRef>) {
        assert!(exprset.alphabet_size == 256);

        debug!("rx0: {}", exprset.expr_to_string_with_info(rx_list[0]));

        let ((mut exprset, rx_list), mapping, alphabet_size) = if cfg!(feature = "compress") {
            let mut compressor = ByteCompressor::new();
            let cost0 = exprset.cost;
            let (mut exprset, rx_list) = compressor.compress(exprset, rx_list);
            exprset.cost += cost0;
            exprset.set_pp(PrettyPrinter::new(
                compressor.mapping.clone(),
                compressor.alphabet_size,
            ));
            (
                (exprset, rx_list),
                compressor.mapping,
                compressor.alphabet_size,
            )
        } else {
            let alphabet_size = exprset.alphabet_size;
            (
                (exprset, rx_list.to_vec()),
                (0..=255).collect(),
                alphabet_size,
            )
        };

        // disable expensive optimizations after initial construction
        exprset.disable_optimizations();

        debug!(
            "compressed: {}",
            exprset.expr_to_string_with_info(rx_list[0])
        );

        let alpha = AlphabetInfo {
            mapping: mapping.try_into().unwrap(),
            size: alphabet_size,
        };
        (alpha, exprset, rx_list.to_vec())
    }

    #[inline(always)]
    pub fn map(&self, b: u8) -> usize {
        if cfg!(feature = "compress") {
            self.mapping[b as usize] as usize
        } else {
            b as usize
        }
    }

    #[inline(always)]
    pub fn map_state(&self, state: StateID, b: u8) -> usize {
        if cfg!(feature = "compress") {
            self.map(b) + state.as_usize() * self.len()
        } else {
            b as usize + state.as_usize() * 256
        }
    }

    #[inline(always)]
    pub fn len(&self) -> usize {
        self.size
    }

    #[inline(always)]
    pub fn is_empty(&self) -> bool {
        self.size == 0
    }

    pub fn has_error(&self) -> bool {
        self.size == 0
    }

    pub fn enter_error_state(&mut self) {
        self.size = 0;
    }
}

// private implementation
impl Regex {
    pub fn is_contained_in_prefixes(
        exprset: ExprSet,
        small: ExprRef,
        big: ExprRef,
        relevance_fuel: u64,
    ) -> Result<bool> {
        let (mut slf, rxes) = Self::prep_regex(exprset, &[small, big]);
        let small = rxes[0];
        let big = rxes[1];

        slf.relevance.is_contained_in_prefixes(
            &mut slf.exprs,
            &mut slf.deriv,
            small,
            big,
            relevance_fuel,
            false,
        )
    }

    fn prep_regex(exprset: ExprSet, top_rxs: &[ExprRef]) -> (Self, Vec<ExprRef>) {
        let (alpha, exprset, rx_list) = AlphabetInfo::from_exprset(exprset, top_rxs);
        let num_ast_nodes = exprset.len();

        let rx_sets = StateID::new_hash_cons();

        let mut slf = Regex {
            deriv: DerivCache::new(),
            next_byte: NextByteCache::new(),
            relevance: RelevanceCache::new(),
            exprs: exprset,
            alpha,
            rx_sets,
            state_table: vec![],
            state_descs: vec![],
            num_transitions: 0,
            num_ast_nodes,
            initial: StateID::MISSING,
            max_states: usize::MAX,
        };

        let desc = StateDesc {
            lookahead_len: Some(None),
            next_byte: Some(NextByte::Dead),
        };

        // DEAD
        slf.append_state(desc.clone());
        // also append state for the "MISSING"
        slf.append_state(desc);
        // in fact, transition from MISSING and DEAD should both lead to DEAD
        slf.state_table.fill(StateID::DEAD);
        assert!(!slf.alpha.is_empty());

        (slf, rx_list)
    }

    pub(crate) fn new_with_exprset(
        exprset: ExprSet,
        top_rx: ExprRef,
        relevance_fuel: u64,
    ) -> Result<Self> {
        let (mut r, top_rx) = Self::prep_regex(exprset, &[top_rx]);
        let top_rx = top_rx[0];

        if r.relevance
            .is_non_empty_limited(&mut r.exprs, top_rx, relevance_fuel)?
        {
            r.initial = r.insert_state(top_rx);
        } else {
            r.initial = StateID::DEAD;
        }

        Ok(r)
    }

    fn append_state(&mut self, state_desc: StateDesc) {
        let mut new_states = vec![StateID::MISSING; self.alpha.len()];
        self.state_table.append(&mut new_states);
        self.state_descs.push(state_desc);
        if self.state_descs.len() >= self.max_states {
            self.alpha.enter_error_state();
        }
    }

    fn insert_state(&mut self, d: ExprRef) -> StateID {
        let id = StateID::new(self.rx_sets.insert(&[d.as_u32()]));
        if id.as_usize() >= self.state_descs.len() {
            self.append_state(StateDesc::default());
        }
        id
    }

    fn transition_inner(&mut self, state: StateID, b: u8) -> StateID {
        assert!(state.is_valid());

        let e = Self::resolve(&self.rx_sets, state);
        let d = self.deriv.derivative(&mut self.exprs, e, b);
        if d == ExprRef::NO_MATCH {
            StateID::DEAD
        } else if self.relevance.is_non_empty(&mut self.exprs, d) {
            self.insert_state(d)
        } else {
            StateID::DEAD
        }
    }
}

impl Debug for Regex {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Regex({})", self.stats())
    }
}
