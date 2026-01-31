use std::fmt::Debug;

use once_cell::sync::OnceCell;

use crate::interval_set::IntervalSet;
use crate::transition::Transition;

pub(crate) const ATNSTATE_INVALID_TYPE: i32 = 0;
pub(crate) const ATNSTATE_BASIC: i32 = 1;
pub(crate) const ATNSTATE_RULE_START: i32 = 2;
pub(crate) const ATNSTATE_BLOCK_START: i32 = 3;
pub(crate) const ATNSTATE_PLUS_BLOCK_START: i32 = 4;
pub(crate) const ATNSTATE_STAR_BLOCK_START: i32 = 5;
pub(crate) const ATNSTATE_TOKEN_START: i32 = 6;
pub(crate) const ATNSTATE_RULE_STOP: i32 = 7;
pub(crate) const ATNSTATE_BLOCK_END: i32 = 8;
pub(crate) const ATNSTATE_STAR_LOOP_BACK: i32 = 9;
pub(crate) const ATNSTATE_STAR_LOOP_ENTRY: i32 = 10;
pub(crate) const ATNSTATE_PLUS_LOOP_BACK: i32 = 11;
pub(crate) const ATNSTATE_LOOP_END: i32 = 12;
pub(crate) const ATNSTATE_INVALID_STATE_NUMBER: i32 = -1;

//might be changed later
#[doc(hidden)]
#[derive(Debug, Eq, PartialEq)]
pub enum ATNStateType {
    RuleStartState {
        stop_state: ATNStateRef,
        is_left_recursive: bool,
    },
    RuleStopState,
    BlockEndState(ATNStateRef),
    LoopEndState(ATNStateRef),
    StarLoopbackState,
    BasicState,
    DecisionState {
        decision: i32,
        nongreedy: bool,
        state: ATNDecisionState,
    },
    InvalidState,
}

#[doc(hidden)]
#[derive(Debug, Eq, PartialEq)]
pub enum ATNDecisionState {
    StarLoopEntry {
        loop_back_state: ATNStateRef,
        is_precedence: bool,
    },
    TokenStartState,
    PlusLoopBack,
    BlockStartState {
        end_state: ATNStateRef,
        en: ATNBlockStart,
    },
}

#[doc(hidden)]
#[derive(Debug, Eq, PartialEq)]
pub enum ATNBlockStart {
    BasicBlockStart,
    StarBlockStart,
    PlusBlockStart(ATNStateRef),
}

pub type ATNStateRef = i32;

// todo no need for trait here, it is too slow for hot code
pub trait ATNState: Sync + Send + Debug {
    fn has_epsilon_only_transitions(&self) -> bool;

    fn get_rule_index(&self) -> i32;
    fn set_rule_index(&self, v: i32);

    fn get_next_tokens_within_rule(&self) -> &OnceCell<IntervalSet>;
    //    fn set_next_token_within_rule(&mut self, v: IntervalSet);

    fn get_state_type(&self) -> &ATNStateType;
    fn get_state_type_mut(&mut self) -> &mut ATNStateType;

    fn get_state_type_id(&self) -> i32;

    fn get_state_number(&self) -> i32;
    fn set_state_number(&self, state_number: i32);

    fn get_transitions(&self) -> &Vec<Box<dyn Transition>>;
    fn set_transitions(&self, t: Vec<Box<dyn Transition>>);
    fn add_transition(&mut self, trans: Box<dyn Transition>);
}

#[derive(Debug)]
pub struct BaseATNState {
    next_tokens_within_rule: OnceCell<IntervalSet>,

    //    atn: Box<ATN>,
    epsilon_only_transitions: bool,

    pub rule_index: i32,

    pub state_number: i32,

    pub state_type_id: i32,

    pub state_type: ATNStateType,

    transitions: Vec<Box<dyn Transition>>,
}

impl BaseATNState {
    pub fn new_base_atnstate() -> BaseATNState {
        BaseATNState {
            next_tokens_within_rule: OnceCell::new(),
            epsilon_only_transitions: false,
            rule_index: 0,
            state_number: 0,
            state_type_id: 0,
            state_type: ATNStateType::InvalidState,
            transitions: Vec::new(),
        }
    }
}

impl ATNState for BaseATNState {
    fn has_epsilon_only_transitions(&self) -> bool {
        self.epsilon_only_transitions
    }
    fn get_rule_index(&self) -> i32 {
        self.rule_index
    }

    fn set_rule_index(&self, _v: i32) {
        unimplemented!()
    }

    fn get_next_tokens_within_rule(&self) -> &OnceCell<IntervalSet> {
        &self.next_tokens_within_rule
    }

    fn get_state_type(&self) -> &ATNStateType {
        &self.state_type
    }

    fn get_state_type_mut(&mut self) -> &mut ATNStateType {
        &mut self.state_type
    }

    fn get_state_type_id(&self) -> i32 {
        self.state_type_id
    }

    fn get_state_number(&self) -> i32 {
        self.state_number
    }

    fn set_state_number(&self, _state_number: i32) {
        unimplemented!()
    }

    fn get_transitions(&self) -> &Vec<Box<dyn Transition>> {
        &self.transitions
    }

    fn set_transitions(&self, _t: Vec<Box<dyn Transition>>) {
        unimplemented!()
    }

    fn add_transition(&mut self, trans: Box<dyn Transition>) {
        if self.transitions.is_empty() {
            self.epsilon_only_transitions = trans.is_epsilon()
        } else {
            self.epsilon_only_transitions &= trans.is_epsilon()
        }

        let mut already_present = false;
        for existing in self.transitions.iter() {
            if existing.get_target() == trans.get_target() {
                if existing.get_label().is_some()
                    && trans.get_label().is_some()
                    && existing.get_label() == trans.get_label()
                {
                    already_present = true;
                    break;
                } else if existing.is_epsilon() && trans.is_epsilon() {
                    already_present = true;
                    break;
                }
            }
        }
        if !already_present {
            self.transitions.push(trans);
        }
    }
}
//pub struct BasicState {
//    base: BaseATNState,
//}
//
//fn new_basic_state() -> BasicState { unimplemented!() }
//
//pub trait DecisionState:ATNState {
//
//    fn get_decision(&self) -> i32;
//    fn set_decision(&self, b: i32);
//
//    fn get_non_greedy(&self) -> bool;
//    fn set_non_greedy(&self, b: bool);
//}
//
//pub struct BaseDecisionState {
//    base: BaseATNState,
//    decision: i32,
//    non_greedy: bool,
//}

//
//fn new_base_decision_state() -> BaseDecisionState { unimplemented!() }
//impl DecisionState for BaseDecisionState {
//    fn get_decision(&self) -> i32 { unimplemented!() }
//
//    fn set_decision(&self, b: i32) { unimplemented!() }
//
//    fn get_non_greedy(&self) -> bool { unimplemented!() }
//
//    fn set_non_greedy(&self, b: bool) { unimplemented!() }
//}
//
//impl ATNState for BaseDecisionState{
//    fn get_epsilon_only_transitions(&self) -> bool {
//        self.base.get_epsilon_only_transitions()
//    }
//
//    fn get_rule_index(&self) -> i32 {
//        self.base.get_rule_index()
//    }
//
//    fn set_rule_index(&self, v: i32) {
//        self.base.set_rule_index(v)
//    }
//
//    fn get_next_token_within_rule(&self) -> IntervalSet {
//        self.base.get_next_token_within_rule()
//    }
//
//    fn set_next_token_within_rule(&self, v: IntervalSet) {
//        self.base.set_next_token_within_rule(v)
//    }
//
//    fn get_atn(&self) -> Arc<ATN> {
//        self.base.get_atn()
//    }
//
//    fn set_atn(&self, atn: Box<ATN>) {
//        self.base.set_atn(atn)
//    }
//
//    fn get_state_type(&self) -> &ATNStateType {
//        self.base.get_state_type()
//    }
//
//    fn get_state_number(&self) -> i32 {
//        self.base.get_state_number()
//    }
//
//    fn set_state_number(&self, stateNumber: i32) {
//        self.base.set_state_number(stateNumber)
//    }
//
//    fn get_transitions(&self) -> Vec<&Transition> {
//        self.base.get_transitions()
//    }
//
//    fn set_transitions(&self, t: Vec<Box<Transition>>) {
//        self.base.set_transitions(t)
//    }
//
//    fn add_transition(&self, trans: Box<Transition>, index: i32) {
//        self.base.add_transition(trans, index)
//    }
//}
//pub trait BlockStartState :DecisionState{
//
//    fn get_end_state(&self) -> &BlockEndState;
//    fn set_end_state(&self, b: Box<BlockEndState>);
//}
//
//pub struct BaseBlockStartState {
//    base: BaseDecisionState,
//    end_state: Box<BlockEndState>,
//}
//
//fn new_block_start_state() -> BaseBlockStartState { unimplemented!() }
//
//impl BlockStartState for BaseBlockStartState {
//    fn get_end_state(&self) -> &BlockEndState { unimplemented!() }
//
//    fn set_end_state(&self, b: Box<BlockEndState>) { unimplemented!() }
//}
//
//impl DecisionState for BaseBlockStartState{
//    fn get_decision(&self) -> i32 {
//        self.base.get_decision()
//    }
//
//    fn set_decision(&self, b: i32) {
//        self.base.set_decision(b)
//    }
//
//    fn get_non_greedy(&self) -> bool {
//        self.base.get_non_greedy()
//    }
//
//    fn set_non_greedy(&self, b: bool) {
//        self.base.set_non_greedy(b)
//    }
//}
//
//impl ATNState for BaseBlockStartState{
//    fn get_epsilon_only_transitions(&self) -> bool {
//        self.base.get_epsilon_only_transitions()
//    }
//
//    fn get_rule_index(&self) -> i32 {
//        self.base.get_rule_index()
//    }
//
//    fn set_rule_index(&self, v: i32) {
//        self.base.set_rule_index(v)
//    }
//
//    fn get_next_token_within_rule(&self) -> IntervalSet {
//        self.base.get_next_token_within_rule()
//    }
//
//    fn set_next_token_within_rule(&self, v: IntervalSet) {
//        self.base.set_next_token_within_rule(v)
//    }
//
//    fn get_atn(&self) -> Arc<ATN> {
//        self.base.get_atn()
//    }
//
//    fn set_atn(&self, atn: Box<ATN>) {
//        self.base.set_atn(atn)
//    }
//
//    fn get_state_type(&self) -> &ATNStateType {
//        self.base.get_state_type()
//    }
//
//    fn get_state_number(&self) -> i32 {
//        self.base.get_state_number()
//    }
//
//    fn set_state_number(&self, stateNumber: i32) {
//        self.base.set_state_number(stateNumber)
//    }
//
//    fn get_transitions(&self) -> Vec<&Transition> {
//        self.base.get_transitions()
//    }
//
//    fn set_transitions(&self, t: Vec<Box<Transition>>) {
//        self.base.set_transitions(t)
//    }
//
//    fn add_transition(&self, trans: Box<Transition>, index: i32) {
//        self.base.add_transition(trans, index)
//    }
//}
//
//pub struct BasicBlockStartState {
//    base: BaseBlockStartState,
//}
//
//fn new_basic_block_start_state() -> BasicBlockStartState { unimplemented!() }
//
//pub struct BlockEndState {
//    base: BaseATNState,
//    start_state: Box<ATNState>,
//}
//
//fn new_block_end_state() -> BlockEndState { unimplemented!() }
//
//pub struct RuleStopState {
//    base: BaseATNState,
//}
//
//fn new_rule_stop_state() -> RuleStopState { unimplemented!() }
//
//pub struct RuleStartState {
//    base: BaseATNState,
//    stop_state: Box<ATNState>,
//    is_precedence_rule: bool,
//}
//
//fn new_rule_start_state() -> RuleStartState { unimplemented!() }
//
//pub struct PlusLoopbackState {
//    base: BaseDecisionState,
//}
//
//fn new_plus_loopback_state() -> PlusLoopbackState { unimplemented!() }
//
//pub struct PlusBlockStartState {
//    base: BaseBlockStartState,
//    loop_back_state: Box<ATNState>,
//}
//
//fn new_plus_block_start_state() -> PlusBlockStartState { unimplemented!() }
//
//pub struct StarBlockStartState {
//    base: BaseBlockStartState,
//}
//
//fn new_star_block_start_state() -> StarBlockStartState { unimplemented!() }
//
//pub struct StarLoopbackState {
//    base: BaseATNState,
//}
//
//fn new_star_loopback_state() -> StarLoopbackState { unimplemented!() }
//
//pub struct StarLoopEntryState {
//    base: BaseDecisionState,
//    loop_back_state: Box<ATNState>,
//    precedence_rule_decision: bool,
//}
//
//fn new_star_loop_entry_state() -> StarLoopEntryState { unimplemented!() }
//
//pub struct LoopEndState {
//    base: BaseATNState,
//    loop_back_state: Box<ATNState>,
//}
//
//fn new_loop_end_state() -> LoopEndState { unimplemented!() }
//
//pub struct TokensStartState {
//    base: BaseDecisionState,
//}
//
//fn new_tokens_start_state() -> TokensStartState { unimplemented!() }
