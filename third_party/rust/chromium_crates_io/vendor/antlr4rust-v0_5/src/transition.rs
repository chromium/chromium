use std::any::{Any, TypeId};
use std::borrow::Cow;
use std::fmt::Debug;

use crate::atn_state::ATNStateRef;
use crate::interval_set::IntervalSet;
use crate::lexer::{LEXER_MAX_CHAR_VALUE, LEXER_MIN_CHAR_VALUE};
use crate::semantic_context::SemanticContext;

const _TRANSITION_NAMES: [&str; 11] = [
    "INVALID",
    "EPSILON",
    "RANGE",
    "RULE",
    "PREDICATE",
    "ATOM",
    "ACTION",
    "SET",
    "NOT_SET",
    "WILDCARD",
    "PRECEDENCE",
];

pub const TRANSITION_EPSILON: i32 = 1;
pub const TRANSITION_RANGE: i32 = 2;
pub const TRANSITION_RULE: i32 = 3;
pub const TRANSITION_PREDICATE: i32 = 4;
pub const TRANSITION_ATOM: i32 = 5;
pub const TRANSITION_ACTION: i32 = 6;
pub const TRANSITION_SET: i32 = 7;
pub const TRANSITION_NOTSET: i32 = 8;
pub const TRANSITION_WILDCARD: i32 = 9;
pub const TRANSITION_PRECEDENCE: i32 = 10;

#[allow(non_camel_case_types)]
#[derive(Debug, Eq, PartialEq)]
pub enum TransitionType {
    TRANSITION_EPSILON = 1,
    TRANSITION_RANGE,
    TRANSITION_RULE,
    TRANSITION_PREDICATE,
    TRANSITION_ATOM,
    TRANSITION_ACTION,
    TRANSITION_SET,
    TRANSITION_NOTSET,
    TRANSITION_WILDCARD,
    TRANSITION_PRECEDENCE,
}

// todo remove trait because it is too slow
/// Transition between ATNStates
pub trait Transition: Sync + Send + Debug + Any {
    fn get_target(&self) -> ATNStateRef;
    fn set_target(&mut self, s: ATNStateRef);
    fn is_epsilon(&self) -> bool {
        false
    }
    fn get_label(&self) -> Option<Cow<'_, IntervalSet>> {
        None
    }
    fn get_serialization_type(&self) -> TransitionType;
    fn matches(&self, symbol: i32, min_vocab_symbol: i32, max_vocab_symbol: i32) -> bool;
    fn get_predicate(&self) -> Option<SemanticContext> {
        None
    }
    fn get_reachable_target(&self, symbol: i32) -> Option<ATNStateRef> {
        //        println!("reachable target called on {:?}", self);
        if self.matches(symbol, LEXER_MIN_CHAR_VALUE, LEXER_MAX_CHAR_VALUE) {
            return Some(self.get_target());
        }
        None
    }
}

impl dyn Transition {
    #[inline]
    pub fn cast<T: Transition>(&self) -> &T {
        assert_eq!(self.type_id(), TypeId::of::<T>());
        unsafe { &*(self as *const dyn Transition as *const T) }
    }
}

#[derive(Debug)]
pub struct AtomTransition {
    pub target: ATNStateRef,
    pub label: i32,
}

impl Transition for AtomTransition {
    fn get_target(&self) -> ATNStateRef {
        self.target
    }

    fn set_target(&mut self, s: ATNStateRef) {
        self.target = s
    }

    fn get_label(&self) -> Option<Cow<'_, IntervalSet>> {
        let mut r = IntervalSet::new();
        r.add_one(self.label);
        Some(Cow::Owned(r))
    }

    fn get_serialization_type(&self) -> TransitionType {
        TransitionType::TRANSITION_ATOM
    }

    fn matches(&self, _symbol: i32, _min_vocab_symbol: i32, _max_vocab_symbol: i32) -> bool {
        _symbol == self.label
    }
}

#[derive(Debug)]
pub struct RuleTransition {
    pub target: ATNStateRef,
    pub follow_state: ATNStateRef,
    pub rule_index: i32,
    pub precedence: i32,
}

impl Transition for RuleTransition {
    fn get_target(&self) -> ATNStateRef {
        self.target
    }
    fn set_target(&mut self, s: ATNStateRef) {
        self.target = s
    }

    fn is_epsilon(&self) -> bool {
        true
    }

    fn get_serialization_type(&self) -> TransitionType {
        TransitionType::TRANSITION_RULE
    }

    fn matches(&self, _symbol: i32, _min_vocab_symbol: i32, _max_vocab_symbol: i32) -> bool {
        unimplemented!()
    }
}

#[derive(Debug)]
pub struct EpsilonTransition {
    pub target: ATNStateRef,
    pub outermost_precedence_return: i32,
}

impl Transition for EpsilonTransition {
    fn get_target(&self) -> ATNStateRef {
        self.target
    }
    fn set_target(&mut self, s: ATNStateRef) {
        self.target = s
    }

    fn is_epsilon(&self) -> bool {
        true
    }

    fn get_serialization_type(&self) -> TransitionType {
        TransitionType::TRANSITION_EPSILON
    }

    fn matches(&self, _symbol: i32, _min_vocab_symbol: i32, _max_vocab_symbol: i32) -> bool {
        false
    }
}

#[derive(Debug)]
pub struct RangeTransition {
    pub target: ATNStateRef,
    pub start: i32,
    pub stop: i32,
}

impl Transition for RangeTransition {
    fn get_target(&self) -> ATNStateRef {
        self.target
    }
    fn set_target(&mut self, s: ATNStateRef) {
        self.target = s
    }

    fn get_label(&self) -> Option<Cow<'_, IntervalSet>> {
        let mut r = IntervalSet::new();
        r.add_range(self.start, self.stop);
        Some(Cow::Owned(r))
    }

    fn get_serialization_type(&self) -> TransitionType {
        TransitionType::TRANSITION_RANGE
    }

    fn matches(&self, _symbol: i32, _min_vocab_symbol: i32, _max_vocab_symbol: i32) -> bool {
        _symbol >= self.start && _symbol <= self.stop
    }
}

#[derive(Debug)]
pub struct ActionTransition {
    pub target: ATNStateRef,
    pub is_ctx_dependent: bool,
    pub rule_index: i32,
    pub action_index: i32,
    pub pred_index: i32,
}

impl Transition for ActionTransition {
    fn get_target(&self) -> ATNStateRef {
        self.target
    }
    fn set_target(&mut self, s: ATNStateRef) {
        self.target = s
    }

    fn is_epsilon(&self) -> bool {
        true
    }

    fn get_serialization_type(&self) -> TransitionType {
        TransitionType::TRANSITION_ACTION
    }

    fn matches(&self, _symbol: i32, _min_vocab_symbol: i32, _max_vocab_symbol: i32) -> bool {
        false
    }
}

#[derive(Debug)]
pub struct SetTransition {
    pub target: ATNStateRef,
    pub set: IntervalSet,
}

impl Transition for SetTransition {
    fn get_target(&self) -> ATNStateRef {
        self.target
    }
    fn set_target(&mut self, s: ATNStateRef) {
        self.target = s
    }

    fn get_label(&self) -> Option<Cow<'_, IntervalSet>> {
        Some(Cow::Borrowed(&self.set))
    }

    fn get_serialization_type(&self) -> TransitionType {
        TransitionType::TRANSITION_SET
    }

    fn matches(&self, _symbol: i32, _min_vocab_symbol: i32, _max_vocab_symbol: i32) -> bool {
        self.set.contains(_symbol)
    }
}

#[derive(Debug)]
pub struct NotSetTransition {
    pub target: ATNStateRef,
    pub set: IntervalSet,
}

impl Transition for NotSetTransition {
    fn get_target(&self) -> ATNStateRef {
        self.target
    }
    fn set_target(&mut self, s: ATNStateRef) {
        self.target = s
    }

    fn get_label(&self) -> Option<Cow<'_, IntervalSet>> {
        Some(Cow::Borrowed(&self.set))
    }

    fn get_serialization_type(&self) -> TransitionType {
        TransitionType::TRANSITION_NOTSET
    }

    fn matches(&self, _symbol: i32, _min_vocab_symbol: i32, _max_vocab_symbol: i32) -> bool {
        _symbol >= _min_vocab_symbol && _symbol <= _max_vocab_symbol && !self.set.contains(_symbol)
    }
}

#[derive(Debug)]
pub struct WildcardTransition {
    pub target: ATNStateRef,
}

impl Transition for WildcardTransition {
    fn get_target(&self) -> ATNStateRef {
        self.target
    }
    fn set_target(&mut self, s: ATNStateRef) {
        self.target = s
    }

    fn get_serialization_type(&self) -> TransitionType {
        TransitionType::TRANSITION_WILDCARD
    }

    fn matches(&self, _symbol: i32, _min_vocab_symbol: i32, _max_vocab_symbol: i32) -> bool {
        _symbol < _max_vocab_symbol && _symbol > _min_vocab_symbol
    }
}

#[derive(Debug)]
pub struct PredicateTransition {
    pub target: ATNStateRef,
    pub is_ctx_dependent: bool,
    pub rule_index: i32,
    pub pred_index: i32,
}

impl Transition for PredicateTransition {
    fn get_target(&self) -> ATNStateRef {
        self.target
    }

    fn set_target(&mut self, s: ATNStateRef) {
        self.target = s
    }

    fn is_epsilon(&self) -> bool {
        true
    }

    fn get_serialization_type(&self) -> TransitionType {
        TransitionType::TRANSITION_PREDICATE
    }

    fn matches(&self, _symbol: i32, _min_vocab_symbol: i32, _max_vocab_symbol: i32) -> bool {
        false
    }

    fn get_predicate(&self) -> Option<SemanticContext> {
        Some(SemanticContext::Predicate {
            rule_index: self.rule_index,
            pred_index: self.pred_index,
            is_ctx_dependent: self.is_ctx_dependent,
        })
    }
}

#[derive(Debug)]
pub struct PrecedencePredicateTransition {
    pub target: ATNStateRef,
    pub precedence: i32,
}

impl Transition for PrecedencePredicateTransition {
    fn get_target(&self) -> ATNStateRef {
        self.target
    }
    fn set_target(&mut self, s: ATNStateRef) {
        self.target = s
    }

    fn is_epsilon(&self) -> bool {
        true
    }

    fn get_serialization_type(&self) -> TransitionType {
        TransitionType::TRANSITION_PRECEDENCE
    }

    fn matches(&self, _symbol: i32, _min_vocab_symbol: i32, _max_vocab_symbol: i32) -> bool {
        false
    }

    fn get_predicate(&self) -> Option<SemanticContext> {
        Some(SemanticContext::Precedence(self.precedence))
    }
}
