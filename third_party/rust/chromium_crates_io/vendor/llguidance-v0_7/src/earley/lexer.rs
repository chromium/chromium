use anyhow::Result;
use std::fmt::Debug;
use toktrie::{Recognizer, SimpleVob, TokTrie};

use crate::api::ParserLimits;

use super::{
    lexerspec::{Lexeme, LexemeIdx, LexerSpec},
    regexvec::{LexemeSet, MatchingLexemes, NextByte, RegexVec, StateDesc},
};

const DEBUG: bool = true;

macro_rules! debug {
    ($($arg:tt)*) => {
        if cfg!(feature = "logging") && DEBUG {
            eprintln!($($arg)*);
        }
    }
}

#[derive(Clone)]
pub struct Lexer {
    pub(crate) dfa: RegexVec,
    // set of bytes that are allowed in any of the lexemes
    // this is used to fail states quickly
    allowed_first_byte: SimpleVob,
    spec: LexerSpec,
}

pub type StateID = derivre::StateID;

/// PreLexeme contains index of the lexeme but not the bytes.
#[derive(Debug, Clone, Copy)]
pub struct PreLexeme {
    pub idx: MatchingLexemesIdx,
    pub byte: Option<u8>,
    /// Does the 'byte' above belong to the next lexeme?
    pub byte_next_row: bool,
    // Length in bytes of the hidden part of the lexeme.
    // pub hidden_len: u32,
}

impl PreLexeme {
    pub fn just_idx(idx: MatchingLexemesIdx) -> Self {
        PreLexeme {
            idx,
            byte: None,
            byte_next_row: false,
        }
    }
}

#[derive(Debug)]
pub enum LexerResult {
    Lexeme(PreLexeme),
    SpecialToken(StateID),
    State(StateID, u8),
    Error,
}

struct LexerPrecomputer<'a> {
    states: Vec<StateID>,
    lex: &'a mut Lexer,
}

impl Recognizer for LexerPrecomputer<'_> {
    fn collapse(&mut self) {}
    fn trie_finished(&mut self) {}
    fn pop_bytes(&mut self, num: usize) {
        self.states.truncate(self.states.len() - num);
    }
    fn try_push_byte(&mut self, byte: u8) -> bool {
        let state = *self.states.last().unwrap();
        match self.lex.advance(state, byte, false) {
            LexerResult::State(next_state, _) => {
                self.states.push(next_state);
                true
            }
            _ => false,
        }
    }
}

impl Lexer {
    pub fn from(spec: &LexerSpec, limits: &mut ParserLimits, dbg: bool) -> Result<Self> {
        let mut dfa = spec.to_regex_vec(limits)?;

        if dbg {
            debug!("lexer: {:?}\n  ==> dfa: {:?}", spec, dfa);
        }

        let s0 = dfa.initial_state(&spec.all_lexemes());
        let mut allowed_first_byte = SimpleVob::alloc(256);
        for i in 0..=255 {
            if !dfa.transition(s0, i).is_dead() {
                allowed_first_byte.allow_token(i as u32);
            }
        }

        let lex = Lexer {
            dfa,
            allowed_first_byte,
            spec: spec.clone(), // TODO check perf of Rc<> ?
        };

        Ok(lex)
    }

    pub fn lexer_spec(&self) -> &LexerSpec {
        &self.spec
    }

    pub fn start_state(&mut self, allowed_lexemes: &LexemeSet) -> StateID {
        self.dfa.initial_state(allowed_lexemes)
    }

    pub fn precompute_for(&mut self, trie: &TokTrie, allowed_lexemes: &LexemeSet) {
        let state = self.start_state(allowed_lexemes);
        let mut states = Vec::with_capacity(300);
        states.push(state);
        let mut pre = LexerPrecomputer { states, lex: self };
        let mut toks = trie.alloc_token_set();
        trie.add_bias(&mut pre, &mut toks, &[]);
    }

    pub fn transition_start_state(&mut self, s: StateID, first_byte: Option<u8>) -> StateID {
        first_byte.map(|b| self.dfa.transition(s, b)).unwrap_or(s)
    }

    pub fn a_dead_state(&self) -> StateID {
        StateID::DEAD
    }

    pub fn possible_hidden_len(&mut self, state: StateID) -> usize {
        self.dfa.possible_lookahead_len(state)
    }

    fn state_info(&self, state: StateID) -> &StateDesc {
        self.dfa.state_desc(state)
    }

    pub fn allows_eos(&mut self, state: StateID) -> bool {
        let l = self.spec.eos_ending_lexemes();
        for lexeme in self.state_info(state).greedy_accepting.as_slice() {
            if l.contains(*lexeme) {
                return true;
            }
        }
        false
    }

    pub fn limit_state_to(&mut self, state: StateID, allowed_lexemes: &LexemeSet) -> StateID {
        self.dfa.limit_state_to(state, allowed_lexemes)
    }

    pub fn possible_lexemes(&self, state: StateID) -> &LexemeSet {
        &self.state_info(state).possible
    }

    pub fn force_lexeme_end(&self, prev: StateID) -> LexerResult {
        let info = self.state_info(prev);
        match info.possible.first() {
            Some(idx) => LexerResult::Lexeme(PreLexeme::just_idx(MatchingLexemesIdx::Single(idx))),
            None => LexerResult::Error,
        }
    }

    pub fn try_lexeme_end(&mut self, prev: StateID) -> LexerResult {
        if self.state_info(prev).greedy_accepting.is_some() {
            LexerResult::Lexeme(PreLexeme::just_idx(MatchingLexemesIdx::GreedyAccepting(
                prev,
            )))
        } else {
            LexerResult::Error
        }
    }

    pub fn check_for_single_byte_lexeme(&mut self, state: StateID, b: u8) -> Option<PreLexeme> {
        if self.dfa.next_byte(state) == NextByte::ForcedEOI {
            Some(PreLexeme {
                idx: MatchingLexemesIdx::GreedyAccepting(state),
                byte: Some(b),
                byte_next_row: false,
            })
        } else {
            None
        }
    }

    pub fn subsume_possible(&mut self, state: StateID) -> bool {
        self.dfa.subsume_possible(state)
    }

    pub fn check_subsume(&mut self, state: StateID, extra_idx: usize, budget: u64) -> Result<bool> {
        self.dfa
            .check_subsume(state, self.spec.extra_lexeme(extra_idx), budget)
    }

    pub fn next_byte(&mut self, state: StateID) -> NextByte {
        // there should be no transition from a state with a lazy match
        // - it should have generated a lexeme
        assert!(!state.has_lowest_match());

        let mut forced = self.dfa.next_byte(state);

        let info = self.dfa.state_desc(state);
        if info.greedy_accepting.is_some() {
            // with lowest accepting present, any transition to DEAD state
            // (of which they are likely many) would generate a lexeme
            forced = forced.make_fuzzy();
        }

        forced
    }

    #[inline(always)]
    pub fn advance(&mut self, prev: StateID, byte: u8, enable_logging: bool) -> LexerResult {
        let state = self.dfa.transition(prev, byte);

        if enable_logging {
            let info = self.state_info(state);
            debug!(
                "lex: {:?} -{:?}-> {:?}, acpt={:?}",
                prev, byte as char, state, info.greedy_accepting
            );
        }

        if state.is_dead() {
            // if the left-over byte is not allowed as the first byte of any lexeme, we can fail early
            if !self.allowed_first_byte.is_allowed(byte as u32) {
                return LexerResult::Error;
            }
            let info = self.dfa.state_desc(prev);
            // we take the first token that matched
            // (eg., "while" will match both keyword and identifier, but keyword is first)
            if info.greedy_accepting.is_some() {
                LexerResult::Lexeme(PreLexeme {
                    idx: MatchingLexemesIdx::GreedyAccepting(prev),
                    byte: Some(byte),
                    byte_next_row: true,
                })
            } else {
                LexerResult::Error
            }
        } else if state.has_lowest_match() {
            let info = self.dfa.state_desc(state);
            assert!(info.lazy_accepting.is_some());
            if info.has_special_token {
                return LexerResult::SpecialToken(state);
            }
            LexerResult::Lexeme(PreLexeme {
                idx: MatchingLexemesIdx::LazyAccepting(state),
                byte: Some(byte),
                byte_next_row: false,
            })
        } else {
            LexerResult::State(state, byte)
        }
    }

    pub fn lexemes_from_idx(&self, idx: MatchingLexemesIdx) -> &MatchingLexemes {
        match idx {
            MatchingLexemesIdx::Single(idx) => &self.spec.lexeme_spec(idx).single_set,
            MatchingLexemesIdx::GreedyAccepting(state_id) => {
                &self.dfa.state_desc(state_id).greedy_accepting
            }
            MatchingLexemesIdx::LazyAccepting(state_id) => {
                &self.dfa.state_desc(state_id).lazy_accepting
            }
        }
    }

    #[inline(always)]
    pub fn lexeme_props(&self, idx: MatchingLexemesIdx) -> (u32, bool) {
        match idx {
            MatchingLexemesIdx::Single(_) | MatchingLexemesIdx::GreedyAccepting(_) => (0, false),
            MatchingLexemesIdx::LazyAccepting(state_id) => {
                let info = self.dfa.state_desc(state_id);
                let hidden = info.lazy_hidden_len;
                if hidden > 0 {
                    let spec = self.spec.lexeme_spec(info.lazy_accepting.first().unwrap());
                    (hidden, spec.is_suffix)
                } else {
                    (0, false)
                }
            }
        }
    }

    pub fn dbg_lexeme(&self, lex: &Lexeme) -> String {
        let set = self.lexemes_from_idx(lex.idx);

        // let info = &self.lexemes[lex.idx.as_usize()];
        // if matches!(info.rx, RegexAst::Literal(_)) && lex.hidden_len == 0 {
        //     format!("[{}]", info.name)
        // }

        format!("{:?} {:?}", lex, set)
    }
}

impl LexerResult {
    #[inline(always)]
    pub fn is_error(&self) -> bool {
        matches!(self, LexerResult::Error)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MatchingLexemesIdx {
    Single(LexemeIdx),
    GreedyAccepting(StateID),
    LazyAccepting(StateID),
}
