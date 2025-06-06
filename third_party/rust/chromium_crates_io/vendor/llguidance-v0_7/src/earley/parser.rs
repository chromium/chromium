// In this file, "Kallmeyer 2018" refers to the
// slides for "Parsing: Earley parsing", Winter 2017/2018,
// Laura Kallmeyer, Heinrich Heine Universitaet, Dusseldorf,
// https://user.phil-fak.uni-duesseldorf.de/~kallmeyer/Parsing/earley.pdf
// (Retrieved 18 Sep 2024).

use std::{
    fmt::{Debug, Display},
    hash::Hash,
    ops::Range,
    sync::{Arc, Mutex},
};

use crate::{earley::lexer::MatchingLexemesIdx, HashMap, HashSet, Instant};
use anyhow::{bail, ensure, Result};
use derivre::{NextByte, RegexAst, StateID};
use serde::{Deserialize, Serialize};
use toktrie::{
    parse_numeric_token, Recognizer, SimpleVob, TokEnv, TokTrie, TokenId, INVALID_TOKEN,
};

use crate::{
    api::{ParserLimits, StopReason},
    earley::{lexer::Lexer, lexerspec::LexemeClass},
    id32_type,
};

use super::{
    grammar::{CGrammar, CSymIdx, CSymbol, RhsPtr},
    lexer::{LexerResult, PreLexeme},
    lexerspec::{Lexeme, LexemeIdx, LexemeSpec, LexerSpec},
    perf::ParserPerfCounters,
    regexvec::{LexemeSet, LexerStats},
};

const TRACE: bool = false;
const DEBUG: bool = true;
pub(crate) const ITEM_TRACE: bool = false;

macro_rules! trace {
    ($($arg:tt)*) => {
        if cfg!(feature = "logging") && TRACE {
            eprintln!($($arg)*);
        }
    }
}

macro_rules! debug {
    ($($arg:tt)*) => {
        if cfg!(feature = "logging") && DEBUG {
            eprintln!($($arg)*);
        }
    }
}

macro_rules! debug_def {
    ($s:expr, $($arg:tt)*) => {
        if cfg!(feature = "logging") && DEBUG && $s.scratch.log_enabled() {
            eprintln!($($arg)*);
        }
    }
}

macro_rules! item_trace {
    ($($arg:tt)*) => {
        if ITEM_TRACE {
            eprint!("    ");
            eprintln!($($arg)*);
        }
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Hash)]
struct Item {
    data: u64,
}

#[derive(Clone)]
struct SavedParserState {
    lexer_stack_length: usize,
}

#[derive(Debug, Default, Serialize, Deserialize, Clone)]
pub struct ParserStats {
    pub compute_time_us: u64,
    pub rows: usize,
    pub cached_rows: usize,
    pub all_items: usize,
    pub lexer_cost: u64,
    pub slices_applied: usize,
    pub trie_nodes_walked: usize,

    pub definitive_bytes: usize,
    pub lexer_ops: usize,
    pub num_lex_errors: usize,
    pub num_lexemes: usize,
}

#[derive(Debug, Clone)]
pub struct XorShift {
    seed: u32,
}

impl XorShift {
    pub fn new(seed: u32) -> Self {
        XorShift { seed }
    }

    pub fn new_str(s: &str) -> Self {
        XorShift {
            seed: XorShift::fnv1a_32(s.as_bytes()),
        }
    }

    #[allow(clippy::should_implement_trait)]
    pub fn next(&mut self) -> u32 {
        let mut x = self.seed;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        self.seed = x;
        x
    }

    pub fn from_range(&mut self, r: Range<usize>) -> usize {
        assert!(r.start < r.end);
        assert!(r.end < u32::MAX as usize);
        r.start + (self.next() as usize) % (r.end - r.start)
    }

    pub fn one_in(&mut self, n: u32) -> bool {
        self.next() % n == 0
    }

    pub fn next_alt(&mut self) -> u32 {
        let mut x = self.seed;
        x ^= x << 15;
        x ^= x >> 4;
        x ^= x << 23;
        self.seed = x;
        x
    }

    pub fn fnv1a_32(s: &[u8]) -> u32 {
        let mut hash: u32 = 0x811c9dc5;
        for byte in s {
            hash ^= *byte as u32;
            hash = hash.wrapping_mul(0x01000193);
        }
        hash
    }

    pub fn sample_from_vob(&mut self, vob: &SimpleVob) -> u32 {
        let nset = vob.num_set();
        assert!(nset > 0);
        if nset > vob.len() / 10 {
            loop {
                let idx = self.from_range(0..vob.len());
                if vob[idx] {
                    return idx as u32;
                }
            }
        } else {
            let choices = vob.to_list();
            choices[self.from_range(0..choices.len())]
        }
    }
}

impl Default for XorShift {
    fn default() -> Self {
        XorShift { seed: 0xdeadf00d }
    }
}

#[derive(Debug, Default, Clone)]
pub struct ParserMetrics {
    pub rand: XorShift,
    pub message: String,
    pub slicer_leftover_us: usize,
}

impl ParserStats {
    pub fn delta(&self, previous: &ParserStats) -> ParserStats {
        ParserStats {
            rows: self.rows.saturating_sub(previous.rows),
            cached_rows: self.cached_rows.saturating_sub(previous.cached_rows),
            definitive_bytes: self
                .definitive_bytes
                .saturating_sub(previous.definitive_bytes),
            lexer_ops: self.lexer_ops.saturating_sub(previous.lexer_ops),
            num_lexemes: self.num_lexemes.saturating_sub(previous.num_lexemes),
            num_lex_errors: self.num_lex_errors.saturating_sub(previous.num_lex_errors),
            all_items: self.all_items.saturating_sub(previous.all_items),
            lexer_cost: self.lexer_cost.saturating_sub(previous.lexer_cost),
            compute_time_us: self
                .compute_time_us
                .saturating_sub(previous.compute_time_us),
            slices_applied: self.slices_applied.saturating_sub(previous.slices_applied),
            trie_nodes_walked: self
                .trie_nodes_walked
                .saturating_sub(previous.trie_nodes_walked),
        }
    }

    pub fn max(&self, other: &ParserStats) -> ParserStats {
        ParserStats {
            rows: self.rows.max(other.rows),
            cached_rows: self.cached_rows.max(other.cached_rows),
            definitive_bytes: self.definitive_bytes.max(other.definitive_bytes),
            lexer_ops: self.lexer_ops.max(other.lexer_ops),
            num_lexemes: self.num_lexemes.max(other.num_lexemes),
            num_lex_errors: self.num_lex_errors.max(other.num_lex_errors),
            all_items: self.all_items.max(other.all_items),
            lexer_cost: self.lexer_cost.max(other.lexer_cost),
            compute_time_us: self.compute_time_us.max(other.compute_time_us),
            slices_applied: self.slices_applied.max(other.slices_applied),
            trie_nodes_walked: self.trie_nodes_walked.max(other.trie_nodes_walked),
        }
    }
}

impl Display for ParserStats {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", serde_json::to_string_pretty(self).unwrap())
    }
}

id32_type!(GrammarStackPtr);

#[derive(Clone, Debug)]
struct GrammarStackNode {
    back_ptr: GrammarStackPtr,
    token_horizon: u32,
    grammar_id: LexemeClass,
    start_item: Item,
    start_item_idx: usize,
}

// In this, code a "Row" is what is usually called an Earley set in the literature.
// The term "row" comes from Kallmeyer 2018, which uses a chart parsing algorithm
// in which the rows are Earley sets.
#[derive(Clone)]
struct Row {
    first_item: u32,
    last_item: u32,

    grammar_stack_ptr: GrammarStackPtr,

    // The lexer state below only allows certain lexemes.
    // The allowed lexemes (aka acceptable
    // lexemes, aka relevant lexemes) are those which the recognizer
    // will accept in the next row.  They are all and only those lexemes
    // which can lead to a successful parse.
    lexer_start_state: StateID,

    lexeme_idx: MatchingLexemesIdx,
}

impl Row {
    fn item_indices(&self) -> Range<usize> {
        self.first_item as usize..self.last_item as usize
    }
}

// In this code, an "Item" is what is called in the literature, an
// "Earley item".
impl Item {
    #[allow(dead_code)]
    const NULL: Self = Item { data: 0 };

    fn new(rule: RhsPtr, start: usize) -> Self {
        Item {
            data: rule.as_index() as u64 | ((start as u64) << 32),
        }
    }

    fn rhs_ptr(&self) -> RhsPtr {
        RhsPtr::from_index(self.data as u32)
    }

    fn start_pos(&self) -> usize {
        (self.data >> 32) as usize
    }

    fn advance_dot(&self) -> Self {
        Item {
            data: self.data + 1,
        }
    }

    fn rewind_dot(&self) -> Self {
        Item {
            data: self.data - 1,
        }
    }
}

impl Debug for Item {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let rule = self.rhs_ptr();
        write!(f, "Item(rhs={} @{})", rule.as_index(), self.start_pos())
    }
}

// This structure implements the Earley table, and in particular working data
// used when processing a row.
#[derive(Clone)]
struct Scratch {
    grammar: Arc<CGrammar>,

    // The current "working row"
    row_start: usize,
    row_end: usize,

    // these two are not really "scratch" - they are just here for convenience
    // grammar_stack only grows, until the trie is finished
    items: Vec<Item>,
    grammar_stack: Vec<GrammarStackNode>,

    push_allowed_grammar_ids: SimpleVob,
    push_allowed_lexemes: LexemeSet,
    push_grm_top: GrammarStackPtr,
    push_lexeme_idx: MatchingLexemesIdx,

    // Is this Earley table in "definitive" mode?
    // 'definitive' is set when the new lexeme is being 'defined',
    // as indicated by the creation of a 'struct Rowinfo' to track
    // the lexeme.  The opposite of definitive mode is "speculative"
    // mode, which is used for computing the token mask on the
    // pre-lexemes.
    definitive: bool,

    log_override: bool,
}

#[derive(Clone)]
struct RowInfo {
    // TODO: possibly use u32 not usize here
    start_byte_idx: usize,
    lexeme: Lexeme,
    token_idx_start: usize,
    token_idx_stop: usize,
}

impl RowInfo {
    fn apply_token_idx(&mut self, idx: usize) {
        self.token_idx_start = self.token_idx_start.min(idx);
        self.token_idx_stop = self.token_idx_stop.max(idx);
    }

    fn set_token_idx(&mut self, idx: usize) {
        self.token_idx_start = idx;
        self.token_idx_stop = idx;
    }

    fn dbg(&self, lexer: &Lexer) -> String {
        format!(
            "token_idx: {}-{}; b:{}; {}",
            self.token_idx_start,
            self.token_idx_stop,
            self.start_byte_idx,
            lexer.dbg_lexeme(&self.lexeme),
        )
    }
}

// State transition is:
// if (next_lexeme, next_lexer_state) := lexer(top.lexer_state, next_byte) {
//     row_idx = scan(top.row_idx, next_lexeme)
//     push(LexerState { row_idx, next_byte, next_lexer_state })
// }
//
// The LLM thinks in tokens, while the parser only deals with lexemes.
// There is no easy translation between these, and the parser cannot work
// with tokens. On the other hand, forcing the LLM to deal with lexemes will increase
// token perplexity and degrade the quality of the LLM's output.
//
// The data structure used to resolve this "impedance mismatch" is a stack of 'LexerState' items.
// Tokens are broken down into single bytes when they go into this stack,
// and the bytes are assembled into lexemes by the lexer.
// The 'LexerState' items are left on the stack (unless backtracking).
//
// The stack of lexer states also manages a virtual stack of Earley sets, via the
// 'row_idx' field.  The current Earley table/stack is rows 0 through 'row_idx'.
#[derive(Clone, Copy, Debug)]
struct LexerState {
    row_idx: u32,         // Index of corresponding row (Earley set)
    lexer_state: StateID, // state after consuming 'byte'
    byte: Option<u8>,
}

#[derive(Clone)]
struct Captures {
    capture_list: Vec<(String, Vec<u8>)>,
    capture_map: HashMap<String, Vec<u8>>,
}

impl Captures {
    fn new() -> Self {
        Captures {
            capture_list: vec![],
            capture_map: HashMap::default(),
        }
    }

    fn push(&mut self, cap: (String, Vec<u8>)) {
        let (name, bytes) = cap;
        // in Guidance, the __LIST_APPEND: ones are supposed to be appended not overwritten
        if !name.starts_with("__LIST_APPEND:") {
            if let Some(old) = self.capture_map.get(&name) {
                if old == &bytes {
                    return;
                }
            }
        }
        self.capture_list.push((name.clone(), bytes.clone()));
        self.capture_map.insert(name, bytes);
    }
}

#[derive(Clone)]
struct ParserState {
    grammar: Arc<CGrammar>,
    tok_env: TokEnv,
    scratch: Scratch,
    trie_lexer_stack: usize,
    trie_grammar_stack: usize,
    captures: Captures,
    special_token_marker_token: TokenId,

    // These are updated also in speculative mode.
    // Both are stacks only in the sense that items can be popped on backtracking
    // (when walking the token trie). Otherwise, they represent the full parsing
    // history - items are not popped in definitive mode.
    lexer_stack: Vec<LexerState>,
    lexer_stack_top_eos: bool,
    lexer_stack_flush_position: usize,
    rows: Vec<Row>,
    rows_valid_end: usize,

    trace_byte_stack: Vec<u8>,
    trace_stats0: ParserStats,
    trace_start: Instant,

    // These are only updated in definitive mode.
    row_infos: Vec<RowInfo>,
    token_idx: usize,
    bytes: Vec<u8>,
    // use u32 to save space
    byte_to_token_idx: Vec<u32>,

    last_force_bytes_len: usize,

    stats: ParserStats,
    perf_counters: Arc<ParserPerfCounters>,
    limits: ParserLimits,
    metrics: ParserMetrics,
    max_all_items: usize,
    parser_error: Option<String>,
    backtrack_byte_count: usize,

    shared_box: Box<SharedState>,
}

#[derive(Clone, Default)]
struct SharedState {
    lexer_opt: Option<Lexer>,
}

impl SharedState {
    #[inline(always)]
    fn lexer_mut(&mut self) -> &mut Lexer {
        self.lexer_opt.as_mut().unwrap()
    }

    #[inline(always)]
    fn lexer(&self) -> &Lexer {
        self.lexer_opt.as_ref().unwrap()
    }
}

#[derive(Clone)]
pub struct Parser {
    shared: Arc<Mutex<Box<SharedState>>>,
    state: ParserState,
}

impl Scratch {
    fn new(grammar: Arc<CGrammar>) -> Self {
        Scratch {
            push_allowed_lexemes: grammar.lexer_spec().alloc_lexeme_set(),
            push_allowed_grammar_ids: grammar.lexer_spec().alloc_grammar_set(),
            push_grm_top: GrammarStackPtr::new(0),
            push_lexeme_idx: MatchingLexemesIdx::Single(LexemeIdx::new(0)),
            grammar,
            row_start: 0,
            row_end: 0,
            items: vec![],
            grammar_stack: vec![],
            definitive: true,
            log_override: false,
        }
    }

    fn log_enabled(&self) -> bool {
        self.definitive || self.log_override
    }

    // Set current working Earley to empty set
    // The set backing data is at `pos`
    fn new_row(&mut self, pos: usize) {
        self.row_start = pos;
        self.row_end = pos;
    }

    // Number of items in the current working Earley set
    fn row_len(&self) -> usize {
        self.row_end - self.row_start
    }

    // Add a new row to the Earley table.  It will be the
    // current, working, row.
    fn work_row(&self, lexer_start_state: StateID) -> Row {
        Row {
            first_item: self.row_start as u32,
            last_item: self.row_end as u32,
            grammar_stack_ptr: self.push_grm_top,
            lexer_start_state,
            lexeme_idx: self.push_lexeme_idx,
        }
    }

    // Make sure there is enough space in the Earley table,
    // usually in preparation for adding Earley items.
    #[inline(always)]
    fn ensure_items(&mut self, n: usize) {
        self.items.reserve(n.saturating_sub(self.items.len()));
    }

    fn push_grammar_stack(&mut self, node: GrammarStackNode) {
        if self.log_enabled() {
            debug!("push_grammar_stack: {:?}", node);
        }
        let ptr = GrammarStackPtr::new(self.grammar_stack.len());
        self.grammar_stack.push(node);
        self.push_grm_top = ptr;
    }

    // Add a new Earley item with default values to the Earley table.  It is
    // "just" added in the sense that no checks are performed, except the one
    // that ensures there is enough space in the table.  The other checks are
    // assumed to be unnecessary or to have been performed.  For example, it
    // is assumed the caller knows that this Earley item will be unique.
    #[inline(always)]
    fn just_add(&mut self, item: Item, _origin_item_idx: usize, info: &str) {
        if self.items.len() == self.row_end {
            self.items.push(item);
        } else {
            self.items[self.row_end] = item;
        }
        if self.log_enabled() {
            debug!(
                "      addu: {} ({})",
                self.item_to_string(self.row_end),
                info
            );
        }
        self.row_end += 1;
    }

    // Find 'item' in the current, working, row.
    #[inline(always)]
    fn find_item(&self, item: Item) -> Option<usize> {
        self.items[self.row_start..self.row_end]
            .iter()
            .position(|&x| x == item)
            .map(|x| x + self.row_start)
    }

    // Ensure that Earley table 'self' contains
    // Earley item 'item'.  That is, look for 'item' in 'self',
    // and add 'item' to 'self' if it is not there already.
    #[inline(always)]
    fn add_unique(&mut self, item: Item, origin_item_idx: usize, info: &str) {
        if self.find_item(item).is_none() {
            self.just_add(item, origin_item_idx, info);
        }
    }

    // Write item at index 'idx' as a string.
    fn item_to_string(&self, idx: usize) -> String {
        item_to_string(&self.grammar, &self.items[idx])
    }
}

macro_rules! ensure_internal {
    ($cond:expr, $msg:expr) => {
        ensure!($cond, "Internal error: {}", $msg)
    };
}

impl ParserState {
    // Create a new state for an empty parser.
    // The parser starts in definitive mode.
    fn new(
        tok_env: TokEnv,
        grammar: Arc<CGrammar>,
        mut limits: ParserLimits,
        perf_counters: Arc<ParserPerfCounters>,
    ) -> Result<(Self, Lexer)> {
        let start = grammar.start();
        let mut lexer = Lexer::from(grammar.lexer_spec(), &mut limits, true)?;
        if limits.precompute_large_lexemes {
            let t0 = crate::Instant::now();
            lexer.dfa.set_fuel(limits.initial_lexer_fuel);
            for spec in &grammar.lexer_spec().lexemes {
                let w = lexer.dfa.lexeme_weight(spec.idx);
                if w > 1000 {
                    // println!(
                    //     "precomputing lexeme {} (w={w}) f={}",
                    //     lexer.lexer_spec().lexeme_def_to_string(spec.idx),
                    //     lexer.dfa.get_fuel()
                    // );
                    let mut allowed = grammar.lexer_spec().alloc_lexeme_set();
                    allowed.add(spec.idx);
                    lexer.precompute_for(tok_env.tok_trie(), &allowed);
                    // println!("fuel={}", lexer.dfa.get_fuel());
                }
            }
            perf_counters.precompute.record(t0.elapsed());
            if lexer.dfa.has_error() {
                bail!("lexer precomputation failed; either increase limits.initial_lexer_fuel or disable limits.precompute_large_lexemes");
            }
        }
        let scratch = Scratch::new(Arc::clone(&grammar));
        let lexer_state = lexer.a_dead_state(); // placeholder
        let spec_tok = tok_env
            .tok_trie()
            .greedy_tokenize(&[TokTrie::SPECIAL_TOKEN_MARKER]);
        let special_marker_token = if spec_tok.len() == 1 {
            spec_tok[0]
        } else {
            INVALID_TOKEN
        };
        let mut r = ParserState {
            grammar,
            tok_env,
            special_token_marker_token: special_marker_token,
            trie_lexer_stack: usize::MAX,
            rows: vec![],
            rows_valid_end: 0,
            row_infos: vec![],
            captures: Captures::new(),
            scratch,
            stats: ParserStats::default(),
            metrics: ParserMetrics::default(),
            trace_stats0: ParserStats::default(),
            trace_byte_stack: vec![],
            trace_start: Instant::now(),
            token_idx: 0,
            byte_to_token_idx: vec![],
            bytes: vec![],
            last_force_bytes_len: usize::MAX,
            max_all_items: usize::MAX,
            limits,
            backtrack_byte_count: 0,
            lexer_stack_top_eos: false,
            lexer_stack_flush_position: 0,
            lexer_stack: vec![LexerState {
                row_idx: 0,
                lexer_state,
                byte: None,
            }],
            trie_grammar_stack: 0,
            parser_error: None,
            shared_box: Box::new(SharedState {
                lexer_opt: Some(lexer),
            }),
            perf_counters,
        };

        r.scratch.grammar_stack.push(GrammarStackNode {
            back_ptr: GrammarStackPtr::new(0),
            token_horizon: u32::MAX,
            grammar_id: LexemeClass::ROOT,
            start_item: Item::new(RhsPtr::from_index(0), 0),
            start_item_idx: 0,
        });

        // Initialize the Earley table with the predictions in
        // row 0.
        for rule in r.grammar.rules_of(start) {
            r.scratch.add_unique(Item::new(*rule, 0), 0, "init");
        }
        debug!("initial push");
        let _ = r.push_row(0, &Lexeme::bogus());
        ensure_internal!(
            r.num_rows() == 1 && r.rows.len() == 1,
            "initial push failed"
        );
        assert!(r.lexer_stack.len() == 1);

        // Set the correct initial lexer state

        if !r.lexer_spec().allow_initial_skip {
            // Disallow initial SKIP if asked to.
            // This is done, for example, we are trying to force
            // the generation of JSON to start.
            let skip_id = r.lexer_spec().skip_id(LexemeClass::ROOT);
            let mut possible = r
                .lexer()
                .possible_lexemes(r.rows[0].lexer_start_state)
                .clone();
            possible.remove(skip_id);
            let new_state = r.lexer_mut().start_state(&possible);
            r.rows[0].lexer_start_state = new_state;
            debug!(
                "disallowing initial SKIP; {}",
                r.allowed_lexemes_dbg(new_state)
            );
        }

        let state = r.rows[0].lexer_start_state;
        r.lexer_stack[0].lexer_state = state;
        r.assert_definitive();

        let lexer = std::mem::take(&mut r.shared_box).lexer_opt.unwrap();

        r.stats.lexer_cost = lexer.dfa.total_fuel_spent();

        Ok((r, lexer))
    }

    #[inline(always)]
    fn lexer(&self) -> &Lexer {
        self.shared_box.lexer()
    }

    #[inline(always)]
    fn lexer_mut(&mut self) -> &mut Lexer {
        self.shared_box.lexer_mut()
    }

    fn with_items_limit<T>(
        &mut self,
        limit: usize,
        lbl: &str,
        f: impl FnOnce(&mut Self) -> T,
    ) -> T {
        self.max_all_items = self.stats.all_items + limit;

        let r = f(self);

        if self.stats.all_items > self.max_all_items && self.parser_error.is_none() {
            self.parser_error = Some(format!(
                "Too many items (limit {}; {}); try avoiding single-byte/short lexemes",
                limit, lbl
            ));
        }

        self.max_all_items = usize::MAX;

        r
    }

    fn compute_bias(&mut self, computer: &dyn BiasComputer, start: &[u8]) -> SimpleVob {
        let t0 = Instant::now();
        let limits = self.limits.clone();
        let dfa = &mut self.lexer_mut().dfa;
        dfa.set_fuel(limits.step_lexer_fuel);
        dfa.set_max_states(limits.max_lexer_states);

        let mut set = self.with_items_limit(limits.step_max_items, "mask", |state| {
            let mut r = ParserRecognizer { state };
            computer.compute_bias(&mut r, start)
        });

        self.stats.lexer_cost = self.lexer().dfa.total_fuel_spent();

        // The SPECIAL_TOKEN_MARKER should never be allowed by itself
        if self.special_token_marker_token != INVALID_TOKEN {
            set.disallow_token(self.special_token_marker_token);
        }

        if start.is_empty() {
            self.run_speculative("token_ranges", |state| {
                if state.flush_lexer() {
                    for spec in state.token_range_lexemes() {
                        for range in &spec.token_ranges {
                            set.allow_range(range.clone());
                        }
                    }
                }
            });
        }

        let eos = computer.trie().eos_token();
        if eos != INVALID_TOKEN && start.is_empty() && self.lexer_allows_eos() {
            set.allow_token(eos);
        }

        let d = t0.elapsed();
        self.stats.compute_time_us += d.as_micros() as u64;
        self.perf_counters.compute_bias.record(d);

        set
    }

    fn after_dots(&self) -> impl Iterator<Item = RhsPtr> + '_ {
        self.curr_row()
            .item_indices()
            .map(|i| self.scratch.items[i].rhs_ptr())
    }

    fn after_dots_symdata(&self) -> impl Iterator<Item = &CSymbol> + '_ {
        self.after_dots().map(|pos| self.grammar.sym_data_dot(pos))
    }

    fn can_advance_inner(&self) -> bool {
        for data in self.after_dots_symdata() {
            if data.idx == CSymIdx::NULL {
                continue;
            }
            if data.is_terminal || data.gen_grammar.is_some() {
                return true;
            }
        }
        false
    }

    pub fn can_advance(&self) -> bool {
        self.has_pending_lexeme_bytes() || self.can_advance_inner()
    }

    pub fn has_pending_lexeme_bytes(&self) -> bool {
        let row_idx = self.num_rows() - 1;
        for back in self.lexer_stack.iter().rev() {
            if back.row_idx as usize != row_idx {
                break;
            }
            if back.byte.is_some() {
                return true;
            }
        }
        false
    }

    // Does the parse succeed in this Earley set?
    // That is, does this Earley set contain a completed
    // start rule?
    fn row_is_accepting(&self) -> bool {
        for pos in self.after_dots() {
            let after_dot = self.grammar.sym_idx_dot(pos);
            if after_dot == CSymIdx::NULL {
                let lhs = self.grammar.sym_idx_lhs(pos);
                if lhs == self.grammar.start() {
                    return true;
                }
            }
        }
        false
    }

    pub fn lexer_allows_eos(&mut self) -> bool {
        if self.has_pending_lexeme_bytes() {
            let lexer_state = self.lexer_state().lexer_state;
            self.lexer_mut().allows_eos(lexer_state)
        } else {
            // empty lexemes are not allowed
            false
        }
    }

    fn item_to_string(&self, idx: usize) -> String {
        self.scratch.item_to_string(idx)
    }

    fn print_row(&self, row_idx: usize) {
        let row = &self.rows[row_idx];
        println!(
            "row {}; lexer_stack={} top_state={:?}",
            row_idx,
            self.lexer_stack.len(),
            self.lexer_stack.last().unwrap().lexer_state
        );

        println!(
            "  allowed: {}",
            self.allowed_lexemes_dbg(row.lexer_start_state)
        );

        if row_idx < self.row_infos.len() {
            let info = &self.row_infos[row_idx];
            if info.lexeme.is_bogus() {
                println!("  lexeme: placeholder");
            } else {
                println!("  lexeme: {}", self.lexer().dbg_lexeme(&info.lexeme));
            }
        } else {
            println!("  speculative");
        }
        for i in row.item_indices() {
            println!("  {}", self.item_to_string(i));
        }
    }

    #[inline(always)]
    fn lexer_state(&self) -> LexerState {
        self.lexer_stack[self.lexer_stack.len() - 1]
    }

    /// Current size of the Earley table -- that is,
    /// the number of Earley sets.
    #[inline(always)]
    pub fn num_rows(&self) -> usize {
        // The number of rows is taken, not from the physical Earley table,
        // but from the virtual Earley stack kept in the lexer state.
        self.lexer_state().row_idx as usize + 1
    }

    #[inline(always)]
    fn pop_lexer_states(&mut self, n: usize) {
        self.lexer_stack
            .truncate(self.lexer_stack.len().saturating_sub(n));
    }

    #[allow(dead_code)]
    pub fn print_stats(&mut self) {
        println!("stats: {:?}", self.stats);
        self.stats = ParserStats::default();
    }

    fn assert_definitive_inner(&self) {
        assert!(self.scratch.definitive);
        assert!(self.backtrack_byte_count == 0);
        if self.num_rows() != self.row_infos.len() {
            panic!(
                "num_rows={} row_infos={}",
                self.num_rows(),
                self.row_infos.len()
            );
        }
    }

    fn assert_definitive(&self) {
        self.assert_definitive_inner();

        if self.lexer_spec().can_rollback() {
            self.check_lexer_bytes_invariant();
        }
    }

    pub fn get_bytes(&self) -> &[u8] {
        &self.bytes
    }

    fn item_lhs(&self, item: &Item) -> CSymIdx {
        self.grammar.sym_idx_lhs(item.rhs_ptr())
    }

    #[allow(dead_code)]
    fn item_sym_data(&self, item: &Item) -> &CSymbol {
        self.grammar.sym_data(self.item_lhs(item))
    }

    fn hidden_start(&self, lexer: &mut Lexer) -> usize {
        let lexer_state = self.lexer_state().lexer_state;
        let hidden_len = lexer.possible_hidden_len(lexer_state);
        if hidden_len == 0 {
            return usize::MAX;
        }
        let last_lexeme_visible_len = self.curr_row_bytes().len() - hidden_len;
        let prefix_len = self.row_infos[self.num_rows() - 1].start_byte_idx;
        prefix_len + last_lexeme_visible_len
    }

    pub fn temperature(&self) -> Option<f32> {
        let mut temp = -1000.0f32;
        for data in self.after_dots_symdata() {
            if data.is_terminal {
                temp = temp.max(data.props.temperature);
            }
        }
        if temp < 0.00000001 {
            None
        } else {
            Some(temp)
        }
    }

    pub fn rollback(&mut self, n_bytes: usize) -> Result<()> {
        debug!("rollback: {} bytes", n_bytes);
        ensure!(self.parser_error.is_none(), "rollback: parser error");
        self.assert_definitive();
        ensure!(
            n_bytes <= self.byte_to_token_idx.len(),
            "rollback: too many bytes {} > {}",
            n_bytes,
            self.byte_to_token_idx.len()
        );

        let new_len = self.byte_to_token_idx.len() - n_bytes;

        self.byte_to_token_idx.truncate(new_len);
        self.bytes.truncate(new_len);
        self.lexer_stack.truncate(new_len + 1);

        self.row_infos.truncate(self.num_rows());
        self.token_idx = *self.byte_to_token_idx.last().unwrap_or(&0) as usize;
        self.last_force_bytes_len = usize::MAX;
        self.lexer_stack_top_eos = false;
        self.rows_valid_end = self.num_rows();

        self.assert_definitive();

        Ok(())
    }

    pub fn validate_tokens(&mut self, tokens: &[TokenId]) -> usize {
        self.assert_definitive();
        self.run_speculative("validate_tokens", |state| {
            state.scratch.log_override = true;
            let mut applied_idx = state.byte_to_token_idx.len();
            let tok_env = state.tok_env.clone();
            let trie = tok_env.tok_trie();
            let eos = trie.eos_token();
            let mut recog = ParserRecognizer { state };
            for (tidx, &tok) in tokens.iter().enumerate() {
                let state = &mut recog.state;
                if tok == eos {
                    if applied_idx == state.bytes.len() && state.is_accepting_inner() {
                        return tidx + 1;
                    } else {
                        return tidx;
                    }
                }

                if applied_idx >= state.bytes.len() {
                    let saved_parser_state = state.save_state();

                    if let Some(idx) = state.flush_and_check_numeric(tok) {
                        let numeric_bytes = trie.decode_as_special(tok);
                        let ok = state.add_numeric_token(idx, &numeric_bytes);
                        assert!(ok.is_ok());
                        continue; // next token please
                    }

                    state.restore_state(saved_parser_state);
                }

                let token_bytes = trie.decode_raw(&[tok]);

                let token_bytes = if applied_idx < state.bytes.len()
                    && state.bytes[applied_idx] == TokTrie::SPECIAL_TOKEN_MARKER
                {
                    trie.decode_as_special(tok)
                } else {
                    token_bytes
                };

                for &b in &token_bytes {
                    if applied_idx < recog.state.bytes.len() {
                        if recog.state.bytes[applied_idx] == b {
                            applied_idx += 1;
                        } else {
                            return tidx;
                        }
                    } else {
                        // never push FF
                        if b != TokTrie::SPECIAL_TOKEN_MARKER && recog.try_push_byte(b) {
                            // normal path
                            continue;
                        } else {
                            return tidx;
                        }
                    }
                }
            }

            tokens.len() // all ok!
        })
    }

    fn add_numeric_token(&mut self, idx: LexemeIdx, tok_bytes: &[u8]) -> Result<()> {
        let lexer_state = self.lexer_state();
        // the last lexer state will be pushed by advance_parser() below
        for &b in &tok_bytes[0..tok_bytes.len() - 1] {
            self.lexer_stack.push(LexerState {
                byte: Some(b),
                ..lexer_state
            });
        }

        if self.scratch.definitive {
            self.bytes.extend_from_slice(tok_bytes);
            for _ in 0..tok_bytes.len() {
                self.byte_to_token_idx
                    .push(self.token_idx.try_into().unwrap());
            }
        }
        debug_def!(
            self,
            "add_numeric_token: idx={:?} bytes={:?}",
            idx,
            tok_bytes
        );
        let ok = self.advance_parser(PreLexeme::just_idx(MatchingLexemesIdx::Single(idx)));
        ensure!(
            ok,
            "failed to advance parser after adding bytes ignoring lexer"
        );
        if self.scratch.definitive {
            let row_idx = self.num_rows() - 1;
            self.row_infos[row_idx].apply_token_idx(self.token_idx);
        }
        Ok(())
    }

    fn flush_and_check_numeric(&mut self, tok_id: TokenId) -> Option<LexemeIdx> {
        if self.flush_lexer() {
            for spec in self.token_range_lexemes() {
                if spec.contains_token(tok_id) {
                    return Some(spec.idx);
                }
            }
        }
        None
    }

    // apply_tokens() "pushes" the bytes in 'tokens' into the lexer and parser.  It is a top-level
    // method in this file.  It is well below llguidance's top-level methods, but in the llguidance
    // LLInterpreter interface, it is called indirectly via the commit_token() method.
    pub fn apply_token(&mut self, tok_bytes: &[u8], tok_id: TokenId) -> Result<usize> {
        self.assert_definitive();

        item_trace!("apply_token: {:?}", String::from_utf8_lossy(tok_bytes));

        let mut check_lexer_max_tokens = false;

        let mut row_to_apply = self.num_rows() - 1;

        // find first row to apply new token idx
        let applied_idx0 = self.byte_to_token_idx.len();
        while row_to_apply > 0 {
            if self.row_infos[row_to_apply].start_byte_idx <= applied_idx0 {
                break;
            }
            row_to_apply -= 1;
        }

        // tok_id normally matches tok_bytes, except when the caller was handling
        // some byte prefix
        if self.tok_env.tok_trie().token(tok_id) == tok_bytes
            && self.byte_to_token_idx.len() == self.bytes.len()
        {
            let applies = self
                .run_speculative("numeric_apply_token", |state| {
                    state.flush_and_check_numeric(tok_id)
                })
                .is_some();
            if applies {
                // non-speculative now
                let row_idx = self.num_rows() - 1;
                self.row_infos[row_idx].apply_token_idx(self.token_idx);

                self.lexer_stack_flush_position = 0;
                let idx = self.flush_and_check_numeric(tok_id).unwrap();
                self.add_numeric_token(idx, tok_bytes)?;

                // if flush_lexer() added a stack entry
                if self.lexer_stack_flush_position > 0 {
                    // we make sure it's not on the top
                    assert!(self.lexer_stack_flush_position + 1 < self.lexer_stack.len());
                    // and remove it
                    self.lexer_stack.remove(self.lexer_stack_flush_position);
                }

                self.assert_definitive();

                return Ok(0);
            }
        }

        for (bidx, &b) in tok_bytes.iter().enumerate() {
            check_lexer_max_tokens = false;
            let applied_idx = self.byte_to_token_idx.len();
            if applied_idx >= self.bytes.len() {
                assert!(applied_idx == self.bytes.len());

                let row_idx = self.num_rows() - 1;

                self.row_infos[row_idx].apply_token_idx(self.token_idx);

                let (ok, bt) = self.try_push_byte_definitive(Some(b));
                if !ok {
                    bail!(
                        "token {:?} doesn't satisfy the grammar; byte {:?} fails parse",
                        String::from_utf8_lossy(tok_bytes),
                        b as char,
                    );
                }
                if bt > 0 {
                    self.byte_to_token_idx.truncate(self.bytes.len());
                    let bt = bt + (tok_bytes.len() - bidx - 1);
                    return Ok(bt);
                }
                if row_idx == self.num_rows() - 1 {
                    // if we didn't push a new row, and are at the end of the current token,
                    // check on max_tokens
                    check_lexer_max_tokens = true;
                }
            } else {
                if bidx == 0 && self.bytes[applied_idx] == TokTrie::SPECIAL_TOKEN_MARKER {
                    if let Some(tid) = self.tok_env.tok_trie().token_id_at_bytes(tok_bytes) {
                        if let Some((len, tid2)) =
                            parse_numeric_token(&self.bytes[applied_idx + 1..])
                        {
                            if tid == tid2 {
                                let tokidx = self.token_idx.try_into().unwrap();
                                for _ in 0..len + 1 {
                                    self.byte_to_token_idx.push(tokidx);
                                }
                                break;
                            }
                        }
                    }
                }
                if self.bytes[applied_idx] != b {
                    bail!(
                        "token {:?} doesn't satisfy the grammar; forced bytes: got {:?}; applying {:?}",
                        String::from_utf8_lossy(tok_bytes),
                        self.bytes[applied_idx] as char,
                        b as char,
                    );
                }
            }

            self.byte_to_token_idx
                .push(self.token_idx.try_into().unwrap());
        }

        item_trace!(
            "apply_token: ok, {}/{}",
            self.byte_to_token_idx.len(),
            self.bytes.len()
        );

        for idx in row_to_apply..self.num_rows() {
            // for all rows fully contained (so far) in the new token, reset token idx
            if self.row_infos[idx].start_byte_idx >= applied_idx0 {
                self.row_infos[idx].set_token_idx(self.token_idx);
            } else {
                // otherwise, just apply it
                self.row_infos[idx].apply_token_idx(self.token_idx);
            }
        }

        if check_lexer_max_tokens {
            let row_idx = self.num_rows() - 1;

            let mut pop_classes = HashSet::default();
            let mut stack_ptr = self.rows[row_idx].grammar_stack_ptr;
            while stack_ptr.as_usize() > 0 {
                let grm_top = &self.scratch.grammar_stack[stack_ptr.as_usize()];
                if grm_top.token_horizon <= self.token_idx as u32 + 1 {
                    pop_classes.insert(grm_top.grammar_id);
                    stack_ptr = grm_top.back_ptr;
                } else {
                    break;
                }
            }

            let info = &self.row_infos[row_idx];
            let info_tokens = std::cmp::max(
                0,
                self.token_idx as isize + 1 - info.token_idx_start as isize,
            ) as usize;
            let lex_state = self.lexer_state().lexer_state;
            let mut limit = self.lexer_spec().alloc_lexeme_set();
            let mut num_limit = 0;
            {
                let possible_lexemes = self.lexer().possible_lexemes(lex_state);
                for lex in possible_lexemes.iter() {
                    let lex_spec = self.lexer_spec().lexeme_spec(lex);
                    let max_tokens = lex_spec.max_tokens();
                    let class_ok = !pop_classes.contains(&lex_spec.class());
                    // let max_tokens = *info.max_tokens.get(&lex).unwrap_or(&usize::MAX);
                    debug!(
                        "  max_tokens: {} max={} info={} class_ok={}",
                        self.lexer().dbg_lexeme(&Lexeme::single_idx(lex)),
                        max_tokens,
                        info_tokens,
                        class_ok
                    );
                    if info_tokens < max_tokens && class_ok {
                        limit.add(lex);
                    } else {
                        num_limit += 1;
                    }
                }
            }
            if num_limit > 0 {
                debug!(
                    "  max_tokens limiting to: {}",
                    self.lexer_spec().dbg_lexeme_set(&limit)
                );
                let new_state = self.lexer_mut().limit_state_to(lex_state, &limit);
                if new_state.is_dead() {
                    debug!("  limited everything; forcing EOI");
                    let (ok, bt) = self.try_push_byte_definitive(None);
                    assert!(bt == 0);
                    if !ok {
                        debug!("parse reject on max_tokens");
                        return Ok(0);
                    }
                } else {
                    self.lexer_stack.last_mut().unwrap().lexer_state = new_state;
                }
            }
        }

        let item_count = self.curr_row().item_indices().count();
        if item_count > self.limits.max_items_in_row {
            bail!(
                "Current row has {} items; max is {}; consider making your grammar left-recursive if it's right-recursive",
                item_count,
                self.limits.max_items_in_row,
            );
        }

        if false {
            self.print_row(self.num_rows() - 1);
        }

        self.assert_definitive();

        Ok(0)
    }

    fn token_range_lexemes(&self) -> Vec<&LexemeSpec> {
        let state = self.lexer_state().lexer_state;
        let possible = self.lexer().possible_lexemes(state);
        self.lexer_spec().token_range_lexemes(possible)
    }

    pub fn needs_force_bytes(&self) -> bool {
        self.bytes.len() != self.last_force_bytes_len
    }

    /// force_bytes() forces bytes into the parser, definitively.
    /// They must be, at each point, the only bytes allowed by
    /// the parser.  force_bytes() returns a 'Vec' of the bytes pushed.
    pub fn force_bytes(&mut self) {
        self.assert_definitive();
        if !self.needs_force_bytes() {
            return;
        }
        trace!("force_bytes lexer_stack {}", self.lexer_stack.len());
        self.with_items_limit(self.limits.step_max_items, "ff_tokens", |s| {
            while let Some(b) = s.forced_byte() {
                debug!("  forced: {:?} 0x{:x}", b as char, b);
                if b == TokTrie::SPECIAL_TOKEN_MARKER {
                    assert!(!s.has_pending_lexeme_bytes());
                    let specs = s.token_range_lexemes();
                    let mut unique_token_id = None;
                    'spec: for s in specs {
                        for range in &s.token_ranges {
                            if range.start() == range.end() {
                                let t = *range.start();
                                if unique_token_id.is_none() || unique_token_id == Some(t) {
                                    unique_token_id = Some(t);
                                } else {
                                    unique_token_id = None;
                                    break 'spec;
                                }
                            } else {
                                unique_token_id = None;
                                break 'spec;
                            }
                        }
                    }
                    if let Some(token_id) = unique_token_id {
                        let mut bytes = format!("X[{}]", token_id).into_bytes();
                        bytes[0] = TokTrie::SPECIAL_TOKEN_MARKER;
                        let mut all_ok = true;
                        for b in bytes {
                            let (ok, bt) = s.try_push_byte_definitive(Some(b));
                            assert!(bt == 0);
                            if !ok {
                                all_ok = false;
                                break;
                            }
                        }

                        if !all_ok {
                            // shouldn't happen?
                            debug!(
                                "  force_bytes reject, special token {}, byte = {}",
                                token_id, b as char
                            );
                            break;
                        } else {
                            continue;
                        }
                    } else {
                        debug!("  non-determined special token");
                        break;
                    }
                }

                let (ok, bt) = s.try_push_byte_definitive(Some(b));
                assert!(bt == 0);
                if !ok {
                    // shouldn't happen?
                    debug!("  force_bytes reject {}", b as char);
                    break;
                }
            }
        });
        self.assert_definitive();
        self.last_force_bytes_len = self.bytes.len();
        let bytes = &self.bytes[self.byte_to_token_idx.len()..];
        trace!(
            "force_bytes exit {} lexer_stack={}",
            bytes.len(),
            self.lexer_stack.len()
        );
    }

    fn special_pre_lexeme(&mut self, state: StateID) -> bool {
        let possible = self.lexer().possible_lexemes(state);
        let specs = self.lexer_spec().token_range_lexemes(possible);
        let bytes = self.curr_row_bytes();
        debug!("special_pre_lexeme: {:?}", String::from_utf8_lossy(&bytes));
        // we get here "FF [ 1 2 3 4", no final ']'
        let bytes = &bytes[2..bytes.len()];
        if let Ok(tok_id) = std::str::from_utf8(bytes).unwrap().parse::<u32>() {
            let idx = specs.iter().position(|spec| {
                spec.token_ranges
                    .iter()
                    .any(|range| range.contains(&tok_id))
            });
            debug!("  >> tok_id={} idx={:?}", tok_id, idx);
            if let Some(idx) = idx {
                let pre = PreLexeme {
                    idx: MatchingLexemesIdx::Single(specs[idx].idx),
                    byte: Some(b']'),
                    byte_next_row: false,
                };
                self.advance_parser(pre)
            } else {
                false
            }
        } else {
            debug!("  >> not a number; should never happen?");
            false
        }
    }

    // Advance the parser or the lexer, depending on whether 'lex_result'
    // is a pre-lexeme or not.
    #[inline(always)]
    fn advance_lexer_or_parser(&mut self, lex_result: LexerResult, curr: LexerState) -> bool {
        match lex_result {
            LexerResult::State(next_state, byte) => {
                // lexer advanced, but no lexeme - fast path
                self.lexer_stack.push(LexerState {
                    row_idx: curr.row_idx,
                    lexer_state: next_state,
                    byte: Some(byte),
                });
                true
            }
            LexerResult::Error => false,
            LexerResult::Lexeme(pre_lexeme) => self.advance_parser(pre_lexeme),
            LexerResult::SpecialToken(state) => self.special_pre_lexeme(state),
        }
    }

    fn check_lexer_bytes_invariant(&self) {
        let off = if self.lexer_stack_top_eos { 2 } else { 1 };
        if self.lexer_stack.len() != self.bytes.len() + off {
            panic!(
                "lexer_stack={:?} bytes={:?} {}!={}+{off}",
                self.lexer_stack,
                String::from_utf8_lossy(&self.bytes),
                self.lexer_stack.len(),
                self.bytes.len()
            );
        }
    }

    fn trie_started_inner(&mut self, lbl: &str) {
        // debug!("trie_started: rows={} lexer={}", self.num_rows(), self.lexer_stack.len());
        self.assert_definitive();

        self.trie_lexer_stack = self.lexer_stack.len();
        self.trie_grammar_stack = self.scratch.grammar_stack.len();
        self.scratch.definitive = false;
        if ITEM_TRACE {
            self.trace_stats0 = self.stats.clone();
            self.trace_start = Instant::now();
            self.trace_byte_stack.clear();
            item_trace!("trie started; {}", lbl);
        }
        self.rows_valid_end = self.num_rows();
    }

    fn trie_finished_inner(&mut self) {
        // debug!("trie_finished: rows={} lexer={}", self.num_rows(), self.lexer_stack.len());
        assert!(!self.scratch.definitive);
        assert!(self.row_infos.len() <= self.num_rows());

        // cleanup excessive grammar items (perf)
        assert!(self.scratch.grammar_stack.len() >= self.trie_grammar_stack);
        self.scratch.grammar_stack.truncate(self.trie_grammar_stack);

        if ITEM_TRACE {
            let mut st = self.stats.clone();
            st.lexer_cost = self.lexer().dfa.total_fuel_spent();
            st = st.delta(&self.trace_stats0);
            st.compute_time_us = self.trace_start.elapsed().as_micros() as u64;
            item_trace!("trie finished: {}", serde_json::to_string(&st).unwrap());
            self.trace_byte_stack.clear();
        }

        // clean up stack
        self.pop_lexer_states(self.lexer_stack.len() - self.trie_lexer_stack);
        self.scratch.definitive = true;
        self.assert_definitive();
        self.rows_valid_end = self.num_rows();
        self.scratch.log_override = false; // reset
        self.lexer_stack_flush_position = 0;
    }

    fn run_speculative<T>(&mut self, lbl: &str, f: impl FnOnce(&mut Self) -> T) -> T {
        self.trie_started_inner(lbl);
        let r = f(self);
        self.trie_finished_inner();
        r
    }

    fn is_accepting_inner(&mut self) -> bool {
        self.flush_lexer() && self.row_is_accepting()
    }

    pub fn is_accepting(&mut self) -> bool {
        self.run_speculative("is_accepting", |s| s.is_accepting_inner())
    }

    // try_push_byte_definitive() attempts to 'push' a byte (that is advance
    // the parse with 'byte') into the parse in definitive mode.
    // Returns 'false' if this is not possible.
    fn try_push_byte_definitive(&mut self, byte: Option<u8>) -> (bool, usize) {
        assert!(self.scratch.definitive);

        let curr = self.lexer_state();

        let res = if byte.is_none() {
            let lexeme = self.lexer_mut().force_lexeme_end(curr.lexer_state);
            if lexeme.is_error() {
                debug!(
                    "    lexer fail on forced end; allowed: {}",
                    self.allowed_lexemes_dbg(self.rows[curr.row_idx as usize].lexer_start_state)
                );
            }
            lexeme
        } else {
            self.stats.definitive_bytes += 1;
            self.lexer_mut()
                .advance(curr.lexer_state, byte.unwrap(), true)
        };

        if res.is_error() {
            debug!(
                "  lexer fail; allowed: {}",
                self.allowed_lexemes_dbg(self.rows[curr.row_idx as usize].lexer_start_state)
            );
        }

        assert!(self.backtrack_byte_count == 0);
        if self.advance_lexer_or_parser(res, curr) {
            if let Some(b) = byte {
                self.bytes.push(b);
            }
            let bt = std::mem::take(&mut self.backtrack_byte_count);
            if bt > 0 {
                assert!(self.lexer_spec().has_stop);
                // reset cache in case we hit the same length again in future
                self.last_force_bytes_len = usize::MAX;
                self.bytes.truncate(self.bytes.len() - bt);
            }
            (true, bt)
        } else {
            (false, 0)
        }
    }

    /// The current Earley set (row) as kept track of
    /// in the lexer stack.
    fn curr_row(&self) -> &Row {
        &self.rows[self.lexer_state().row_idx as usize]
    }

    /// forced_byte() finds the unique byte allowed by the
    /// parser at this point, and returns it.  If there is
    /// no such byte, forced_byte() returns 'None'.
    fn forced_byte(&mut self) -> Option<u8> {
        if self.is_accepting() {
            debug!("  in accept state, not forcing");
            return None;
        }

        // self.print_row(self.num_rows() - 1);

        //let t0 = Instant::now();
        let lex_state = self.lexer_state().lexer_state;
        let quick_res = self.lexer_mut().next_byte(lex_state);
        if let NextByte::ForcedByte(b) = quick_res {
            return Some(b);
        }

        let slow_res = self.run_speculative("forced_byte", |state| {
            let mut r = ParserRecognizer { state };

            // if we've got two byte hint from the lexer, try both bytes
            if let NextByte::SomeBytes2([a, b]) = quick_res {
                if r.try_push_byte(a) {
                    r.pop_bytes(1);
                    if r.try_push_byte(b) {
                        r.pop_bytes(1);
                        //r.state.perf_counters.forced_byte_miss.record(t0.elapsed());
                        return None;
                    }
                }
            }

            // let alpha = r.state.lexer().dfa.alpha().unique_bytes();

            // otherwise, start iterating from any hint from the lexer,
            // otherwise from ' '
            let b0 = quick_res.some_bytes().first().cloned().unwrap_or(b' ');
            let mut b = b0;
            let mut byte_sym = None;
            loop {
                if r.try_push_byte(b) {
                    r.pop_bytes(1);
                    // debug!("  forced: {:?}", b as char);
                    if byte_sym.is_some() {
                        // debug!("  forced multiple");
                        return None; // more than one option
                    } else {
                        byte_sym = Some(b);
                    }
                }
                b = b.wrapping_add(1);
                if b == b0 {
                    break;
                }
            }
            byte_sym
        });

        // if quick_res.is_some() {
        //     assert_eq!(quick_res, slow_res);
        // } else if slow_res.is_none() {
        //     self.perf_counters.forced_byte_miss.record(t0.elapsed());
        // }

        slow_res
    }

    fn save_state(&self) -> SavedParserState {
        SavedParserState {
            lexer_stack_length: self.lexer_stack.len(),
        }
    }

    fn restore_state(&mut self, state: SavedParserState) {
        self.lexer_stack.truncate(state.lexer_stack_length);
    }

    /// Advance the parser as if the current lexeme (if any)
    /// finished right here.
    /// Returns true if the parser was able to advance (or there were no pending bytes for a lexeme).
    fn flush_lexer(&mut self) -> bool {
        if !self.has_pending_lexeme_bytes() {
            return true;
        }
        let curr = self.lexer_state();
        let lex_result = self.lexer_mut().try_lexeme_end(curr.lexer_state);
        let prev_len = self.lexer_stack.len();
        let r = self.advance_lexer_or_parser(lex_result, curr);
        if self.lexer_stack.len() != prev_len {
            assert!(self.lexer_stack.len() == prev_len + 1);
            assert!(prev_len > 0);
            self.lexer_stack_flush_position = prev_len;
        }
        assert!(self.backtrack_byte_count == 0);
        r
    }

    pub fn scan_eos(&mut self) -> bool {
        self.assert_definitive(); // ???

        let lexer_eos = self.lexer_allows_eos();

        debug!("  scan eos: lexer_eos={}", lexer_eos);

        let prev_stack = self.lexer_stack.len();
        if !self.flush_lexer() {
            assert_eq!(self.lexer_stack.len(), prev_stack);
            debug!("  flush_lexer() failed");
            return false;
        }

        debug!("  flush_lexer() OK");

        if lexer_eos {
            return true;
        }
        // This is really for EOS tokens in the middle of the grammar
        // that need to be eaten; so don't check for accepting state here
        // if self.is_accepting() {
        //     return true;
        // }

        if self.lexer_stack.len() != prev_stack {
            assert_eq!(self.lexer_stack.len(), prev_stack + 1);
            self.lexer_stack_top_eos = true;
        }

        self.assert_definitive(); // ???

        false
    }

    // this just copies current row
    fn scan_skip_lexeme(&mut self, lexeme: &Lexeme) -> bool {
        let src = self.curr_row().item_indices();
        let n = src.len();
        if n == 0 {
            return false;
        }
        self.scratch.ensure_items(src.end + n + 10);
        self.scratch.new_row(src.end);
        self.scratch.push_lexeme_idx = lexeme.idx;

        // we'll not re-run process_agenda() for the newly added row, so save its allowed lexemes
        // (this is unless we hit max_tokens case)
        let mut lex_start = Some(self.rows[self.num_rows() - 1].lexer_start_state);

        for i in src {
            self.scratch
                .just_add(self.scratch.items[i], i, "skip_lexeme");
        }

        let (grammar_id, max_token_ptr) = self.maybe_pop_grammar_stack(lexeme.idx);

        // no process_agenda() in the normal case

        if let Some(ptr) = max_token_ptr {
            // but we have to do it if we hit the max tokens case
            self.process_max_tokens(ptr, lexeme);
            // process_agenda() will recompute push_allowed_lexemes etc
            lex_start = None;
        }

        let push_res = self.just_push_row(grammar_id, lex_start);
        assert!(push_res);

        true
    }

    // scan() implements the version of Earley described in Kallmeyer 2018.
    // An important difference between the algorithm implemented here
    // and Kallmeyer's is that in scan(), the token scan is performed
    // first, while in Kallmeyer it is performed last.

    // Returns false if the parse is exhausted, true otherwise.

    // lexeme body only used for captures (in definitive mode)
    // and debugging (lexeme.idx used always)
    fn scan(&mut self, lexeme: &Lexeme) -> bool {
        let set = self.shared_box.lexer().lexemes_from_idx(lexeme.idx);

        let lex_spec = self.lexer_spec();
        for lx in set.as_slice() {
            if lex_spec.lexeme_spec(*lx).is_skip {
                return self.scan_skip_lexeme(lexeme);
            }
        }

        let row_idx = self.num_rows() - 1;
        let items = self.rows[row_idx].item_indices();
        self.scratch.ensure_items(items.end + items.len() + 10);
        self.scratch.new_row(items.end);
        self.scratch.push_lexeme_idx = lexeme.idx;

        debug_def!(
            self,
            "  scan: {} at row={} token={}",
            self.lexer().dbg_lexeme(lexeme),
            row_idx,
            self.token_idx,
        );

        // This loop performs the scan inference rule
        // (slide 21 of Kallmeyer 2018).  It is an
        // initialization inference rule, performed "just
        // in time" at the beginning of the creation of
        // each row
        for i in items {
            let item = self.scratch.items[i];
            let sym = self.grammar.sym_data_dot(item.rhs_ptr());
            if let Some(idx) = sym.lexeme {
                if set.contains(idx) {
                    self.scratch.just_add(item.advance_dot(), i, "scan");
                }
            }
        }

        // Perform the other inference rules on this Earley set.
        self.push_row(self.num_rows(), lexeme)
    }

    fn mk_capture(&self, var_name: &str, bytes: &[u8]) -> (String, Vec<u8>) {
        debug!(
            "      capture: {} {:?}",
            var_name,
            String::from_utf8_lossy(bytes)
        );

        let bytes = self.tok_env.tok_trie().decode_raw_to_decode(bytes);
        (var_name.to_string(), bytes)
    }

    fn process_one_capture(
        &mut self,
        lhs: CSymIdx,
        curr_idx: usize,
        lexeme: &Lexeme,
        is_lexeme: bool,
        capture_start: usize,
    ) {
        let sym_data = self.grammar.sym_data(lhs);

        debug!(
            "      process_one_capture: {} {}-{} {}",
            self.grammar.sym_name(lhs),
            capture_start,
            curr_idx,
            if is_lexeme { "lexeme" } else { "full" }
        );

        if let Some(var_name) = sym_data.props.stop_capture_name.as_ref() {
            let bytes = lexeme.hidden_bytes();
            self.captures.push(self.mk_capture(var_name, bytes));
        }

        if let Some(var_name) = sym_data.props.capture_name.as_ref() {
            let mut bytes = Vec::new();
            if capture_start < curr_idx {
                bytes = self.row_infos[capture_start..curr_idx]
                    .iter()
                    .map(|ri| ri.lexeme.upper_visible_bytes(is_lexeme))
                    .collect::<Vec<_>>()
                    .concat();
            }
            if is_lexeme || capture_start < curr_idx {
                bytes.extend_from_slice(lexeme.upper_visible_bytes(is_lexeme));
            }
            self.captures.push(self.mk_capture(var_name, &bytes));
        }
    }

    fn process_captures(&mut self, item: Item, curr_idx: usize, lexeme: &Lexeme, for_lexeme: bool) {
        let rule = item.rhs_ptr();
        let for_full_rule = self.grammar.sym_idx_dot(rule) == CSymIdx::NULL;

        debug!(
            "    process_captures: for_full_rule={} for_lexeme={}; {:?}",
            for_full_rule, for_lexeme, lexeme
        );

        if for_full_rule {
            let lhs = self.grammar.sym_idx_lhs(rule);
            self.process_one_capture(lhs, curr_idx, lexeme, false, item.start_pos());
        }

        if for_lexeme {
            // let (_, dot_pos) = self.grammar.rule_rhs(rule);
            // assert!(dot_pos > 0);
            let prev_item = item.rewind_dot();
            let lex_idx = self.grammar.sym_idx_dot(prev_item.rhs_ptr());
            assert!(lex_idx != CSymIdx::NULL);
            self.process_one_capture(lex_idx, curr_idx, lexeme, true, curr_idx);
        }
    }

    #[inline(always)]
    fn process_agenda(&mut self, curr_idx: usize, lexeme: &Lexeme) {
        let mut agenda_ptr = self.scratch.row_start;

        self.scratch.push_allowed_lexemes.clear();
        self.scratch.push_allowed_grammar_ids.set_all(false);

        let lexemes_end = if self.scratch.row_start == 0 {
            // initial push - no lexemes scanned yet
            0
        } else {
            // here, items[self.scratch.row_start..lexemes_end] are all lexemes
            self.scratch.row_end
        };

        // Agenda retrieval is a simplification of Kallmeyer 2018.
        // There is no separate data structure for the agenda --
        // the Earley table is used, so that adding to the Earley
        // table (aka chart) also adds an item to the agenda.  No duplicate
        // agenda items are added.  Agenda items are never removed --
        // instead 'agenda_ptr' is advanced through the combined agenda/chart.
        // Only one pass is made.
        while agenda_ptr < self.scratch.row_end {
            let item_idx = agenda_ptr;
            let item = self.scratch.items[agenda_ptr];
            agenda_ptr += 1;
            debug_def!(self, "    agenda: {}", self.item_to_string(item_idx));

            let rule = item.rhs_ptr();
            let after_dot = self.grammar.sym_idx_dot(rule);

            if self.scratch.definitive {
                let is_lexeme = agenda_ptr <= lexemes_end;
                if is_lexeme || after_dot == CSymIdx::NULL {
                    self.process_captures(item, curr_idx, lexeme, is_lexeme);
                }
            }

            // If 'rule' is a complete Earley item ...
            if after_dot == CSymIdx::NULL {
                let lhs = self.grammar.sym_idx_lhs(rule);
                if item.start_pos() < curr_idx {
                    // if item.start_pos() == curr_idx, then we handled it below in the nullable check

                    // The main completion inference rule (slide 21 in Kallmeyer 2018)
                    for i in self.rows[item.start_pos()].item_indices() {
                        let item = self.scratch.items[i];
                        if self.grammar.sym_idx_dot(item.rhs_ptr()) == lhs {
                            self.scratch.add_unique(item.advance_dot(), i, "complete");
                        }
                    }
                }
            } else {
                // ... if 'rule' is an incompletion
                let sym_data = self.grammar.sym_data(after_dot);
                if let Some(lx) = sym_data.lexeme {
                    self.scratch
                        .push_allowed_grammar_ids
                        .set(sym_data.props.grammar_id.as_usize(), true);
                    self.scratch.push_allowed_lexemes.add(lx);
                }

                // The completion inference rule for nullable symbols
                // (slide 20 in Kallmeyer 2018).
                if sym_data.is_nullable {
                    self.scratch
                        .add_unique(item.advance_dot(), item_idx, "null");
                    if self.scratch.definitive && sym_data.props.capture_name.is_some() {
                        // nullable capture
                        let var_name = sym_data.props.capture_name.as_ref().unwrap();
                        debug!("      capture: {} NULL", var_name);
                        self.captures.push((var_name.clone(), vec![]));
                    }
                }

                if sym_data.gen_grammar.is_some() {
                    let mut node = self.mk_grammar_stack_node(sym_data, curr_idx);
                    self.scratch
                        .add_unique(node.start_item, item_idx, "gen_grammar");
                    node.start_item_idx = self.scratch.find_item(node.start_item).unwrap();
                    self.scratch.push_grammar_stack(node);
                } else {
                    // The top-down, or prediction, inference rule.
                    // (slide 20 in Kallmeyer 2018)
                    for rule in &sym_data.rules {
                        let new_item = Item::new(*rule, curr_idx);
                        self.scratch.add_unique(new_item, item_idx, "predict");
                    }
                }
            }
        }
    }

    #[inline(always)]
    fn just_push_row(&mut self, grammar_id: LexemeClass, lex_start: Option<StateID>) -> bool {
        let row_len = self.scratch.row_len();

        self.stats.rows += 1;

        if row_len == 0 {
            false
        } else {
            self.stats.all_items += row_len;

            let lex_start = if let Some(l) = lex_start {
                l
            } else {
                // accept a SKIP lexeme, if the grammar didn't finish
                if self
                    .scratch
                    .push_allowed_grammar_ids
                    .get(grammar_id.as_usize())
                {
                    let skip = self.lexer_spec().skip_id(grammar_id);
                    self.scratch.push_allowed_lexemes.add(skip);
                }

                self.shared_box
                    .lexer_mut()
                    .start_state(&self.scratch.push_allowed_lexemes)
            };

            debug_def!(
                self,
                "  push row: {} {:?}",
                self.allowed_lexemes_dbg(lex_start),
                grammar_id
            );

            // Add the working row to the parser state
            let idx = self.num_rows();

            let row = self.scratch.work_row(lex_start);
            if self.rows.is_empty() || self.rows.len() == idx {
                // If the physical 'rows' Vec is full, we push a new row
                // otherwise ...
                self.rows.push(row);
            } else {
                // ... we put the new row at the end of the virtual
                // stack as tracked by the lexer.
                self.rows[idx] = row;
            }
            self.rows_valid_end = idx + 1;

            if self.scratch.definitive {
                // Clear all row info data after the
                // working row.
                if self.row_infos.len() > idx {
                    self.row_infos.drain(idx..);
                }

                // Typically, the current byte was not yet pushed,
                // yet it's part of the previous lexeme.
                // This is not true for the first row (which is checked here),
                // or when there is a transition byte (which is corrected in
                // lexer_state_for_added_row())
                let mut start_byte_idx = self.bytes.len();
                if start_byte_idx > 0 {
                    start_byte_idx += 1;
                }

                self.row_infos.push(RowInfo {
                    lexeme: Lexeme::bogus(),
                    token_idx_start: self.token_idx,
                    token_idx_stop: self.token_idx,
                    start_byte_idx,
                });
                // debug!("  push: {idx} {} {}", self.rows.len(), self.row_infos.len());
            }

            true
        }
    }

    fn process_max_tokens(&mut self, ptr: GrammarStackPtr, lexeme: &Lexeme) {
        debug_def!(self, "  process_max_tokens");
        let curr_idx = self.num_rows();
        let top = &self.scratch.grammar_stack[ptr.as_usize()];
        self.scratch.push_grm_top = top.back_ptr;
        let item = top.start_item.advance_dot();
        // remove everything from the current row
        self.scratch.row_end = self.scratch.row_start;
        self.scratch
            .just_add(item, top.start_item_idx, "max_tokens");
        self.process_agenda(curr_idx, lexeme);
    }

    // push_row() does the agenda processing.  There is an agenda for
    // each Earley set (aka row).

    // Returns false if an empty Earley set is added (and therefore
    // the parse is exhausted); and true otherwise.

    // lexeme value only used for captures (in definitive mode)
    #[inline(always)]
    fn push_row(&mut self, curr_idx: usize, lexeme: &Lexeme) -> bool {
        let (grammar_id, max_token_ptr) = self.maybe_pop_grammar_stack(lexeme.idx);

        self.process_agenda(curr_idx, lexeme);

        if let Some(ptr) = max_token_ptr {
            assert!(curr_idx == self.num_rows(), "max_tokens on first row");
            self.process_max_tokens(ptr, lexeme);
        }

        self.just_push_row(grammar_id, None)
    }

    fn mk_grammar_stack_node(&self, sym_data: &CSymbol, curr_idx: usize) -> GrammarStackNode {
        // TODO check if grammar is already on the stack - if so bail
        // there should be only one rule
        assert!(sym_data.rules.len() == 1);
        let start_item = Item::new(sym_data.rules[0], curr_idx);
        // with one symbol
        assert!(self.grammar.sym_idx_dot(start_item.advance_dot().rhs_ptr()) == CSymIdx::NULL);
        let nested_sym = self.grammar.sym_data_dot(start_item.rhs_ptr());
        let token_horizon = sym_data.props.max_tokens.saturating_add(self.token_idx);
        GrammarStackNode {
            back_ptr: self.scratch.push_grm_top,
            token_horizon: token_horizon as u32,
            grammar_id: nested_sym.props.grammar_id,
            start_item,
            start_item_idx: usize::MAX,
        }
    }

    // when this is called, the current row has only rules with lx at the dot
    #[inline(always)]
    fn maybe_pop_grammar_stack(
        &mut self,
        lx: MatchingLexemesIdx,
    ) -> (LexemeClass, Option<GrammarStackPtr>) {
        let set = self.lexer().lexemes_from_idx(lx);
        let lex_spec = &self.lexer_spec();
        let grammar_ids = HashSet::from_iter(
            set.as_slice()
                .iter()
                .map(|&e| lex_spec.lexeme_spec(e).class()),
        );
        let mut max_token_ptr = None;

        let mut grm_stack_top = if !self.rows.is_empty() {
            self.rows[self.num_rows() - 1].grammar_stack_ptr
        } else {
            GrammarStackPtr::new(0)
        };

        while grm_stack_top.as_usize() > 0 {
            let grm_top = &self.scratch.grammar_stack[grm_stack_top.as_usize()];
            debug_def!(
                self,
                "  pop grammar_stack: top={:?}, curr={:?}, #{}",
                grm_top.grammar_id,
                grammar_ids,
                self.token_idx
            );
            if grammar_ids.contains(&grm_top.grammar_id) {
                // token_idx is one behind
                if grm_top.token_horizon <= self.token_idx as u32 {
                    // mark that we need to do the max_token processing
                    // and where to pop the stack
                    // We only pop one grammar off the stack.
                    // If more grammars have the same token horizon, they will get popped
                    // in the next step - we might overrun a bit.
                    debug_def!(
                        self,
                        "  hit token limit horizon={} token_idx={}",
                        grm_top.token_horizon,
                        self.token_idx
                    );
                    max_token_ptr = Some(grm_stack_top);
                }
                break;
            }
            grm_stack_top = grm_top.back_ptr;
        }

        if grm_stack_top.as_usize() == 0 {
            assert!(
                grammar_ids.contains(&LexemeClass::ROOT),
                "grammar stack empty for non-root grammar: {:?}",
                grammar_ids
            );
        }

        self.scratch.push_grm_top = grm_stack_top;

        let top_id = self.scratch.grammar_stack[grm_stack_top.as_usize()].grammar_id;
        (top_id, max_token_ptr)
    }

    // curr_row_bytes() looks in the lexer stack, and returns
    // the bytes for the current row as a 'Vec'.
    #[inline(always)]
    fn curr_row_bytes(&self) -> Vec<u8> {
        let mut bytes = vec![];
        let row_idx = self.num_rows() - 1;
        for back in self.lexer_stack.iter().rev() {
            if back.row_idx as usize != row_idx {
                break;
            }
            if let Some(b) = back.byte {
                bytes.push(b);
            }
        }
        bytes.reverse();
        bytes
    }

    fn lexer_spec(&self) -> &LexerSpec {
        self.grammar.lexer_spec()
    }

    fn allowed_lexemes_dbg(&self, lex_state: StateID) -> String {
        self.lexer_spec()
            .dbg_lexeme_set(self.lexer().possible_lexemes(lex_state))
    }

    // mk_lexeme() converts a pre-lexeme for the current row into
    // a lexeme (ie., it determines the bytes that go into the lexeme), and returns it.
    #[inline(always)]
    fn mk_lexeme(&self, byte: Option<u8>, pre_lexeme: PreLexeme) -> Lexeme {
        let mut bytes = self.curr_row_bytes();
        if let Some(byte) = byte {
            bytes.push(byte);
        }

        let (hidden, is_suffix) = self.lexer().lexeme_props(pre_lexeme.idx);
        Lexeme::new(pre_lexeme.idx, bytes, hidden, is_suffix)
    }

    fn has_forced_bytes(&self, allowed_lexemes: &LexemeSet, bytes: &[u8]) -> bool {
        // note that this is also used when computing token mask
        if allowed_lexemes.is_empty() {
            return false;
        }
        let mut matched_something = false;
        for lexeme_idx in allowed_lexemes.iter() {
            let lex_spec = &self.lexer_spec().lexemes[lexeme_idx.as_usize()];
            if lex_spec.is_skip && matches!(lex_spec.rx, RegexAst::NoMatch) {
                continue;
            }

            if !self.lexer_spec().has_forced_bytes(lex_spec, bytes) {
                return false;
            }
            matched_something = true;
        }
        // debug!("   forced ok {:?}", String::from_utf8_lossy(bytes));
        matched_something
    }

    #[inline(always)]
    fn lexer_state_for_added_row(
        &mut self,
        lexeme: Lexeme,
        transition_byte: Option<u8>,
    ) -> LexerState {
        // note, that while self.rows[] is updated, the lexer stack is not
        // so the last added row is at self.num_rows(), and not self.num_rows() - 1
        let added_row = self.num_rows();
        let added_row_start_state = self.rows[added_row].lexer_start_state;

        let no_hidden = LexerState {
            row_idx: added_row as u32,
            lexer_state: self
                .shared_box
                .lexer_mut()
                .transition_start_state(added_row_start_state, transition_byte),
            byte: transition_byte,
        };

        if self.scratch.definitive {
            // save lexeme at the last row, before we mess with the stack
            self.row_infos[added_row - 1].lexeme = lexeme;
            // if there is a transition byte it means it goes to the next lexeme,
            // and thus we were overeager assigning start_byte_idx,
            // so we need to correct it
            if transition_byte.is_some() {
                let new_start = self.row_infos[added_row - 1]
                    .start_byte_idx
                    .saturating_sub(1);
                self.row_infos[added_row].start_byte_idx -= new_start;
            }
        }
        debug_def!(
            self,
            "lex: re-start {:?} (via {:?}); allowed: {}",
            no_hidden.lexer_state,
            transition_byte.map(|b| b as char),
            self.allowed_lexemes_dbg(added_row_start_state)
        );

        no_hidden
    }

    #[inline(always)]
    fn handle_hidden_bytes(
        &mut self,
        no_hidden: LexerState,
        lexeme_byte: Option<u8>,
        pre_lexeme: PreLexeme,
    ) -> bool {
        let added_row_start_state = self.rows[self.num_rows()].lexer_start_state;

        // make sure we have a real lexeme
        let lexeme = self.mk_lexeme(lexeme_byte, pre_lexeme);

        let hidden_bytes = lexeme.hidden_bytes();

        let trace_here = self.scratch.log_enabled();

        if trace_here {
            trace!(
                "  hidden_bytes: {} {:?}",
                self.allowed_lexemes_dbg(added_row_start_state),
                String::from_utf8_lossy(hidden_bytes)
            );
        }

        if self.has_forced_bytes(
            self.lexer().possible_lexemes(added_row_start_state),
            hidden_bytes,
        ) {
            if trace_here {
                trace!("  hidden forced");
            }
            let mut lexer_state = added_row_start_state;
            // if the bytes are forced, we just advance the lexer
            // by replacing the top lexer states
            self.pop_lexer_states(hidden_bytes.len() - 1);
            for idx in 0..hidden_bytes.len() {
                let b = hidden_bytes[idx];
                match self
                    .shared_box
                    .lexer_mut()
                    .advance(lexer_state, b, trace_here)
                {
                    LexerResult::State(next_state, _) => {
                        lexer_state = next_state;
                    }
                    LexerResult::SpecialToken(_) => panic!("hidden byte resulted in special token"),
                    LexerResult::Error => panic!("hidden byte failed; {:?}", hidden_bytes),
                    LexerResult::Lexeme(second_lexeme) => {
                        if trace_here {
                            debug!("hidden bytes lexeme: {:?}", second_lexeme);
                        }
                        assert!(
                            idx == hidden_bytes.len() - 1,
                            "lexeme in the middle of hidden bytes"
                        );

                        // save current state, we'll need to pop it later
                        self.lexer_stack.push(LexerState {
                            lexer_state,
                            byte: None,
                            ..no_hidden
                        });
                        let r = self.advance_parser(second_lexeme);
                        // println!("hidden bytes lexeme: {:?} -> {r}", second_lexeme);
                        if r {
                            // here, advance_parser() has pushed a state; we replace our state with it
                            let new_top = self.lexer_stack.pop().unwrap();
                            *self.lexer_stack.last_mut().unwrap() = new_top;
                            return true;
                        } else {
                            // otherwise, we just pop our state
                            // This shouldn't happen though
                            // (the parser was allowing this lexeme and now it doesn't like it)
                            self.lexer_stack.pop();
                            return false;
                        }
                    }
                }
                self.lexer_stack.push(LexerState {
                    lexer_state,
                    byte: Some(b),
                    ..no_hidden
                });
            }
            if self.scratch.definitive {
                self.assert_definitive_inner();
            }
        } else {
            if trace_here {
                debug!("  hidden not forced");
            }
            if self.scratch.definitive {
                // set it up for matching after backtrack
                self.lexer_stack.push(LexerState {
                    lexer_state: added_row_start_state,
                    byte: None,
                    ..no_hidden
                });
                self.assert_definitive_inner();
                self.backtrack_byte_count = hidden_bytes.len();
            } else {
                // prevent any further matches in this branch
                self.lexer_stack.push(LexerState {
                    lexer_state: self.shared_box.lexer_mut().a_dead_state(),
                    byte: None,
                    ..no_hidden
                });
            }
            // panic!("hidden bytes not forced");
        }

        true
    }

    fn lexer_stack_top(&self) -> String {
        String::from_utf8_lossy(&self.trace_byte_stack).to_string()
    }

    /// Advance the parser with given 'pre_lexeme'.
    /// On return, the lexer_state will be the state *after* consuming
    /// 'pre_lexeme'.  As a special case, a following single byte lexeme
    /// is also consumed.
    ///
    // The new lexer state will be an initial lexer states when the lexing
    // is lazy.  If the lexing was greedy, it will be an initial lexer state
    // advanced to the byte which produced the greedy lexeme.
    // This is never inlined anyways, so better make it formal
    #[inline(never)]
    fn advance_parser(&mut self, pre_lexeme: PreLexeme) -> bool {
        if self.stats.all_items > self.max_all_items {
            return false;
        }

        // this byte will be applied to the next lexeme
        let transition_byte = if pre_lexeme.byte_next_row {
            pre_lexeme.byte
        } else {
            None
        };
        // this is the last byte of the lexeme
        let lexeme_byte = if pre_lexeme.byte_next_row {
            None
        } else {
            pre_lexeme.byte
        };
        let lexeme_idx = pre_lexeme.idx;

        let lexeme = if self.scratch.definitive {
            self.mk_lexeme(lexeme_byte, pre_lexeme)
        } else {
            Lexeme::just_idx(lexeme_idx)
        };

        let scan_res = if !self.scratch.definitive
            && self.num_rows() < self.rows_valid_end
            && self.rows[self.num_rows()].lexeme_idx == lexeme_idx
        {
            // re-use pushed row
            self.stats.cached_rows += 1;
            true
        } else {
            // Process this lexeme with the parser
            let scan_res = self.scan(&lexeme);

            if scan_res && ITEM_TRACE {
                let added_row = self.num_rows();
                let row = &self.rows[added_row];
                item_trace!(
                    "  row: {:?} -> {}",
                    self.lexer_stack_top(),
                    row.item_indices().len()
                );

                if self.stats.all_items > self.max_all_items {
                    panic!("max items exceeded");
                }
            }

            scan_res
        };

        if scan_res {
            let mut no_hidden = self.lexer_state_for_added_row(lexeme, transition_byte);

            let (hidden, is_suffix) = self.lexer().lexeme_props(lexeme_idx);
            if hidden > 0 && !is_suffix {
                return self.handle_hidden_bytes(no_hidden, lexeme_byte, pre_lexeme);
            } else {
                if pre_lexeme.byte_next_row && no_hidden.lexer_state.is_dead() {
                    if self.scratch.definitive {
                        // clean up row infos if needed
                        self.row_infos.drain(no_hidden.row_idx as usize..);
                    }
                    return false;
                }
                if let Some(b) = transition_byte {
                    // At this point there may be a single-byte lexeme after the one
                    // we just recognized.  For example, assuming C language, in the
                    // token "foo(", once we recognize the "foo" lexeme, we immediately
                    // have a single byte "(" lexeme.  We deal with these here.
                    let single = self
                        .lexer_mut()
                        .check_for_single_byte_lexeme(no_hidden.lexer_state, b);
                    if let Some(second_lexeme) = single {
                        debug_def!(self, "single byte lexeme: {:?}", second_lexeme);
                        no_hidden.byte = None;
                        self.lexer_stack.push(no_hidden);

                        // disallow recursion depth > 2
                        assert!(pre_lexeme.byte_next_row);
                        assert!(!second_lexeme.byte_next_row);

                        let r = self.advance_parser(second_lexeme);
                        if r {
                            let new_top = self.lexer_stack.pop().unwrap();
                            *self.lexer_stack.last_mut().unwrap() = new_top;
                            return true;
                        } else {
                            self.lexer_stack.pop();
                            return false;
                        }
                    }
                }
                debug_def!(self, "  push normal: {no_hidden:?}");
                self.lexer_stack.push(no_hidden);
            }
            if self.scratch.definitive {
                self.assert_definitive_inner();
            }
            true
        } else {
            debug_def!(self, "  scan failed");
            false
        }
    }
}

pub struct ParserRecognizer<'a> {
    state: &'a mut ParserState,
}

impl ParserRecognizer<'_> {
    pub fn lexer_mut(&mut self) -> &mut Lexer {
        self.state.lexer_mut()
    }
    pub fn lexer(&self) -> &Lexer {
        self.state.lexer()
    }
    pub fn lexer_state(&self) -> StateID {
        self.state.lexer_state().lexer_state
    }
    pub fn stats_mut(&mut self) -> &mut ParserStats {
        &mut self.state.stats
    }
    pub fn metrics_mut(&mut self) -> &mut ParserMetrics {
        &mut self.state.metrics
    }
}

pub trait BiasComputer: Send + Sync {
    fn compute_bias(&self, rec: &mut ParserRecognizer<'_>, start: &[u8]) -> SimpleVob;
    fn trie(&self) -> &TokTrie;
}

// Processing of the parser and the lexer is heavily interlocked.
// The 'Recognizer' trait is used as the interface for this.
// See the documentation for TokTrie in README.md and toktrie.md:
// https://github.com/microsoft/llguidance/blob/main/toktrie/README.md
// and
// https://github.com/microsoft/llguidance/blob/main/docs/toktrie.md .
impl Recognizer for ParserRecognizer<'_> {
    #[inline(always)]
    fn pop_bytes(&mut self, num: usize) {
        if ITEM_TRACE {
            self.state
                .trace_byte_stack
                .truncate(self.state.trace_byte_stack.len() - num);
        }
        self.state.pop_lexer_states(num);
    }

    // For this Earley parser, collapse does nothing -- it is a no-op
    fn collapse(&mut self) {
        // This actually means "commit" - can no longer backtrack past this point.
        // However, this parser ignores it.
    }

    fn trie_started(&mut self, lbl: &str) {
        self.state.trie_started_inner(lbl);
    }

    fn trie_finished(&mut self) {
        self.state.trie_finished_inner();
    }

    // try_push_byte() is the "speculative" version of try_push_byte_definitive().
    // It attempts to advance the lexer and parser one byte.  It returns true
    // if it succeeds in doing this, true otherwise.  It is often invoked indirectly by the
    // add_bias_inner() method of TokTrie.  In this file, that can happen via the add_bias()
    // and the various compute_bias() methods.
    #[inline(always)]
    fn try_push_byte(&mut self, byte: u8) -> bool {
        let stats = false;

        let lexer_logging = false;
        let curr = self.state.lexer_state();
        let res = self
            .state
            .lexer_mut()
            .advance(curr.lexer_state, byte, lexer_logging);

        if ITEM_TRACE {
            self.state.trace_byte_stack.push(byte);
        }

        if stats {
            // this is always true (not only with stats) but checking it has significant cost
            assert!(!self.state.scratch.definitive);

            self.state.stats.lexer_ops += 1;
            match res {
                LexerResult::State(_, _) => {}
                LexerResult::Error => self.state.stats.num_lex_errors += 1,
                LexerResult::Lexeme(_) | LexerResult::SpecialToken(_) => {
                    self.state.stats.num_lexemes += 1
                }
            }
        }

        let r = self.state.advance_lexer_or_parser(res, curr);

        if ITEM_TRACE && !r {
            self.state.trace_byte_stack.pop();
        }

        r
    }

    fn save_stats(&mut self, nodes_walked: usize) {
        self.state.stats.trie_nodes_walked += nodes_walked;
    }
}

fn item_to_string(g: &CGrammar, item: &Item) -> String {
    format!("{} @{}", g.rule_to_string(item.rhs_ptr()), item.start_pos(),)
}

pub enum ParserError {
    LexerError(String),
    ParserError(String),
}

impl ParserError {
    pub fn stop_reason(&self) -> StopReason {
        match self {
            ParserError::LexerError(_) => StopReason::LexerTooComplex,
            ParserError::ParserError(_) => StopReason::ParserTooComplex,
        }
    }

    pub fn message(&self) -> String {
        match self {
            ParserError::LexerError(s) => format!("lexer error: {}", s),
            ParserError::ParserError(s) => format!("parser error: {}", s),
        }
    }
}

impl Parser {
    pub fn new(
        tok_env: TokEnv,
        grammar: Arc<CGrammar>,
        limits: ParserLimits,
        perf_counters: Arc<ParserPerfCounters>,
    ) -> Result<Self> {
        let (state, lexer) = ParserState::new(tok_env, grammar, limits, perf_counters)?;
        let shared = Arc::new(Mutex::new(Box::new(SharedState {
            lexer_opt: Some(lexer),
        })));
        Ok(Parser { shared, state })
    }

    /// This is a top-level method in this file.  It is called by compute_mask_inner()
    /// in TokenParser in tokenparser.rs.  It is used by the compute_mask() method of
    /// the LLInterpreter interface.
    pub fn compute_bias(&mut self, computer: &dyn BiasComputer, start: &[u8]) -> SimpleVob {
        self.with_shared(|state| state.compute_bias(computer, start))
    }

    pub fn captures(&self) -> &[(String, Vec<u8>)] {
        &self.state.captures.capture_list
    }

    pub fn get_capture(&self, name: &str) -> Option<&[u8]> {
        self.state.captures.capture_map.get(name).map(|v| &v[..])
    }

    pub fn stats(&self) -> &ParserStats {
        &self.state.stats
    }

    #[inline(always)]
    pub fn perf_counters(&self) -> &ParserPerfCounters {
        &self.state.perf_counters
    }

    pub fn metrics_mut(&mut self) -> &mut ParserMetrics {
        &mut self.state.metrics
    }

    // The "hidden" feature must be supported for historical reasons.
    // It is used for 'gen(stop="foo')'.  The result of this 'gen'
    // must not include 'foo', even though the LLM generated 'foo'.
    // The bytes in 'foo' are therefore said to be "hidden".
    pub fn hidden_start(&self) -> usize {
        let mut shared = self.shared.lock().unwrap();
        self.state.hidden_start(shared.lexer_mut())
    }

    pub fn lexer_stats(&self) -> LexerStats {
        self.shared.lock().unwrap().lexer().dfa.stats()
    }

    pub fn get_error(&self) -> Option<ParserError> {
        let shared = self.shared.lock().unwrap();
        if let Some(e) = shared.lexer().dfa.get_error() {
            return Some(ParserError::LexerError(e));
        }
        if let Some(e) = &self.state.parser_error {
            return Some(ParserError::ParserError(e.clone()));
        }
        None
    }

    pub fn with_recognizer<T>(&mut self, f: impl FnOnce(&mut ParserRecognizer) -> T) -> T {
        self.with_shared(|state| {
            let mut rec = ParserRecognizer { state };
            f(&mut rec)
        })
    }

    pub fn get_bytes(&self) -> &[u8] {
        self.state.get_bytes()
    }

    pub fn force_bytes(&mut self) -> &[u8] {
        if !self.state.needs_force_bytes() {
            self.currently_forced_bytes()
        } else {
            let t0 = Instant::now();
            let prev_len = self.currently_forced_bytes().len();
            self.with_shared(|state| state.force_bytes());
            let r = self.currently_forced_bytes();
            if r.len() > prev_len {
                self.state.perf_counters.force_bytes.record(t0.elapsed());
            } else {
                self.state
                    .perf_counters
                    .force_bytes_empty
                    .record(t0.elapsed());
            }
            r
        }
    }

    pub fn scan_eos(&mut self) -> bool {
        self.with_shared(|state| state.scan_eos())
    }

    pub fn grammar_warnings(&mut self) -> Vec<String> {
        self.with_shared(|state| state.lexer_spec().render_warnings())
    }

    pub(crate) fn apply_forced(&mut self, byte_idx: usize) {
        self.state.byte_to_token_idx.resize(byte_idx, 0);
    }

    pub(crate) fn additional_backtrack(&mut self, n_bytes: usize) {
        // we can be sometimes asked to backtrack more than we have
        // in case the prompt was token-healed; see https://github.com/guidance-ai/guidance/issues/1131
        let new_len = self.state.byte_to_token_idx.len().saturating_sub(n_bytes);
        self.state.byte_to_token_idx.truncate(new_len);
    }

    pub fn apply_token(&mut self, tok_bytes: &[u8], tok_id: TokenId) -> Result<usize> {
        let r = self.with_shared(|state| state.apply_token(tok_bytes, tok_id));
        self.state.token_idx += 1;
        r
    }

    fn with_shared<T>(&mut self, f: impl FnOnce(&mut ParserState) -> T) -> T {
        let mut shared = self.shared.lock().unwrap();
        self.state.shared_box = std::mem::take(&mut *shared);
        let r = f(&mut self.state);
        *shared = std::mem::take(&mut self.state.shared_box);
        assert!(shared.lexer_opt.is_some());
        r
    }

    pub fn rollback(&mut self, n_bytes: usize) -> Result<()> {
        self.state.lexer_spec().check_rollback()?;
        self.with_shared(|state| state.rollback(n_bytes))
    }

    /// Returns how many tokens can be applied.
    pub fn validate_tokens(&mut self, tokens: &[TokenId]) -> usize {
        self.with_shared(|state| {
            let r = state.validate_tokens(tokens);
            debug!(
                "validate_tokens: {} -> {}/{}",
                state.tok_env.tok_trie().tokens_dbg(tokens),
                r,
                tokens.len()
            );
            r
        })
    }

    pub fn log_row_infos(&mut self, label: &str) {
        if cfg!(feature = "logging") && DEBUG {
            self.with_shared(|state| {
                debug!(
                    "row infos {}: token_idx: {}; applied bytes: {}/{}",
                    label,
                    state.token_idx,
                    state.byte_to_token_idx.len(),
                    state.bytes.len()
                );
                for infos in state.row_infos.iter() {
                    debug!("  {}", infos.dbg(state.lexer()));
                }
            })
        }
    }

    pub fn is_accepting(&mut self) -> bool {
        self.with_shared(|state| state.is_accepting())
    }

    pub fn currently_forced_bytes(&self) -> &[u8] {
        &self.state.bytes[self.state.byte_to_token_idx.len()..]
    }

    pub fn has_pending_lexeme_bytes(&self) -> bool {
        self.state.has_pending_lexeme_bytes()
    }

    pub fn grammar(&self) -> &CGrammar {
        &self.state.grammar
    }

    pub fn can_advance(&self) -> bool {
        self.state.can_advance()
    }

    pub fn temperature(&self) -> Option<f32> {
        self.state.temperature()
    }

    pub fn deep_clone(&self) -> Self {
        let mut copy = self.clone();
        let shared = self.shared.lock().unwrap();
        copy.shared = Arc::new(Mutex::new(shared.clone()));
        copy
    }

    pub fn test_trigger_lexer_error(&mut self) -> Result<()> {
        self.with_shared(|_state| {
            panic!("synthetic error");
        })
    }
}
