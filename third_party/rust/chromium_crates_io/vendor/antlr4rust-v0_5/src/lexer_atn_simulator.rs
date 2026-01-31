//! Implementation of lexer automata(DFA)
use std::cell::Cell;

use std::ops::Deref;
use std::rc::Rc;
use std::sync::Arc;
use std::usize;

use crate::atn::ATN;
use crate::atn_config::{ATNConfig, ATNConfigType};
use crate::atn_config_set::ATNConfigSet;
use crate::atn_simulator::{BaseATNSimulator, IATNSimulator};
use crate::atn_state::ATNStateType::RuleStopState;
use crate::atn_state::{ATNState, ATNStateType};

use crate::dfa::DFA;
use crate::dfa_state::{DFAState, DFAStateRef};
use crate::errors::ANTLRError;
use crate::errors::ANTLRError::LexerNoAltError;
use crate::int_stream::{IntStream, EOF};
use crate::lexer::{Lexer, LexerPosition, LEXER_MAX_CHAR_VALUE, LEXER_MIN_CHAR_VALUE};
use crate::lexer_action_executor::LexerActionExecutor;
use crate::prediction_context::EMPTY_PREDICTION_CONTEXT;
use crate::prediction_context::{
    PredictionContext, PredictionContextCache, PREDICTION_CONTEXT_EMPTY_RETURN_STATE,
};
use crate::token::TOKEN_EOF;

use crate::transition::{
    ActionTransition, PredicateTransition, RuleTransition, Transition, TransitionType,
};
use crate::utils::cell_update;
use parking_lot::{RwLock, RwLockUpgradableReadGuard, RwLockWriteGuard};

#[allow(missing_docs)]
pub const ERROR_DFA_STATE_REF: DFAStateRef = usize::MAX;

// todo rewrite this to be actually usable
#[doc(hidden)]
pub trait ILexerATNSimulator: IATNSimulator {
    fn reset(&mut self);
    fn match_token<'input>(
        &mut self,
        mode: usize,
        lexer: &mut impl Lexer<'input>,
    ) -> Result<i32, ANTLRError>;
    fn get_char_position_in_line(&self) -> isize;
    fn set_char_position_in_line(&mut self, column: isize);
    fn get_line(&self) -> isize;
    fn set_line(&mut self, line: isize);
    fn consume<T: IntStream + ?Sized>(&self, input: &mut T);
    #[cold]
    fn recover(&mut self, _re: ANTLRError, input: &mut impl IntStream) {
        if input.la(1) != EOF {
            self.consume(input)
        }
    }
}

/// Simple DFA implementation enough for lexer.
#[derive(Debug)]
pub struct LexerATNSimulator {
    base: BaseATNSimulator,

    //    merge_cache: DoubleDict,
    start_index: isize,
    pub(crate) current_pos: Rc<LexerPosition>,
    mode: usize,
    prev_accept: SimState,
    // lexer_action_executor: Option<Box<LexerActionExecutor>>,
}

impl ILexerATNSimulator for LexerATNSimulator {
    fn reset(&mut self) {
        self.prev_accept.reset()
    }

    fn match_token<'input>(
        &mut self,
        mode: usize,
        //        input:&mut dyn CharStream,
        lexer: &mut impl Lexer<'input>,
    ) -> Result<i32, ANTLRError> {
        self.mode = mode;
        let mark = lexer.input().mark();
        //        println!("start matching on mode {}",mode);
        let result = (|| {
            self.start_index = lexer.input().index();
            self.prev_accept.reset();
            let temp = self.base.decision_to_dfa.clone();
            let dfa = temp
                .get(mode)
                .ok_or_else(|| ANTLRError::IllegalStateError("invalid mode".into()))?;
            let dfa = dfa.upgradable_read();

            let s0 = dfa.s0;
            match s0 {
                None => self.match_atn(lexer, dfa),
                Some(s0) => self.exec_atn(s0, lexer, dfa),
                //                Err(_) => panic!("dfa rwlock error")
            }
        })();
        lexer.input().release(mark);
        result
    }

    fn get_char_position_in_line(&self) -> isize {
        self.current_pos.char_position_in_line.get()
    }

    fn set_char_position_in_line(&mut self, column: isize) {
        self.current_pos.char_position_in_line.set(column)
    }

    fn get_line(&self) -> isize {
        self.current_pos.line.get()
    }

    fn set_line(&mut self, line: isize) {
        self.current_pos.char_position_in_line.set(line)
    }

    fn consume<T: IntStream + ?Sized>(&self, _input: &mut T) {
        let ch = _input.la(1);
        if ch == '\n' as i32 {
            cell_update(&self.current_pos.line, |x| x + 1);
            self.current_pos.char_position_in_line.set(0);
        } else {
            cell_update(&self.current_pos.char_position_in_line, |x| x + 1);
        }
        _input.consume();
    }

    //    fn get_recog(&self) -> Rc<RefCell<Box<Recognizer>>>{
    //        Rc::clone(&self.recog)
    //    }
}

impl IATNSimulator for LexerATNSimulator {
    fn shared_context_cache(&self) -> &PredictionContextCache {
        self.base.shared_context_cache()
    }

    fn atn(&self) -> &ATN {
        self.base.atn()
    }

    fn decision_to_dfa(&self) -> &Vec<RwLock<DFA>> {
        self.base.decision_to_dfa()
    }
}

#[allow(missing_docs)]
pub const MIN_DFA_EDGE: i32 = 0;
#[allow(missing_docs)]
pub const MAX_DFA_EDGE: i32 = 127;

impl LexerATNSimulator {
    /// Creates `LexerATNSimulator` instance which creates DFA over `atn`
    ///
    /// Called from generated parser.
    pub fn new_lexer_atnsimulator(
        atn: Arc<ATN>,
        decision_to_dfa: Arc<Vec<RwLock<DFA>>>,
        shared_context_cache: Arc<PredictionContextCache>,
    ) -> LexerATNSimulator {
        LexerATNSimulator {
            base: BaseATNSimulator::new_base_atnsimulator(
                atn,
                decision_to_dfa,
                shared_context_cache,
            ),
            start_index: 0,
            current_pos: Rc::new(LexerPosition {
                line: Cell::new(0),
                char_position_in_line: Cell::new(0),
            }),
            mode: 0,
            prev_accept: SimState::new(),
            // lexer_action_executor: None,
        }
    }

    //    fn copy_state(&self, _simulator: &mut LexerATNSimulator) {
    //        unimplemented!()
    //    }

    #[cold]
    fn match_atn<'input>(
        &mut self,
        lexer: &mut impl Lexer<'input>,
        dfa: RwLockUpgradableReadGuard<'_, DFA>,
    ) -> Result<i32, ANTLRError> {
        //        let start_state = self.atn().mode_to_start_state.get(self.mode as usize).ok_or(ANTLRError::IllegalStateError("invalid mode".into()))?;
        let atn = self.atn();
        let start_state = *atn
            .mode_to_start_state
            .get(self.mode)
            .ok_or_else(|| ANTLRError::IllegalStateError("invalid mode".into()))?;

        let _old_mode = self.mode;
        let mut s0_closure = self.compute_start_state(atn.states[start_state as usize].as_ref(), lexer);
        let _supress_edge = s0_closure.has_semantic_context();
        s0_closure.set_has_semantic_context(false);

        let mut dfa_mut = RwLockUpgradableReadGuard::upgrade(dfa);

        let next_state = self.add_dfastate(&mut dfa_mut, s0_closure);
        if !_supress_edge {
            dfa_mut.s0 = Some(next_state);
        }

        self.exec_atn(
            next_state,
            lexer,
            RwLockWriteGuard::downgrade_to_upgradable(dfa_mut),
        )
    }

    fn exec_atn<'input>(
        &mut self,
        //        input: &'a mut dyn CharStream,
        ds0: DFAStateRef,
        lexer: &mut impl Lexer<'input>,
        dfa: RwLockUpgradableReadGuard<'_, DFA>,
    ) -> Result<i32, ANTLRError> {
        //        if self.get_dfa().states.read().unwrap().get(ds0).unwrap().is_accept_state{
        self.capture_sim_state(&dfa, lexer.input(), ds0);
        //        }
        let mut dfa = Some(dfa);
        let mut symbol = lexer.input().la(1);
        let mut s = ds0;
        loop {
            let target = Self::get_existing_target_state(dfa.as_ref().unwrap(), s, symbol);
            let target =
                target.unwrap_or_else(|| self.compute_target_state(&mut dfa, s, symbol, lexer));
            //              let target = dfastates.deref().get(s).unwrap() ;x

            if target == ERROR_DFA_STATE_REF {
                break;
            }
            //            println!(" --- target computed {:?}", self.get_dfa().states.read().unwrap()[target].configs.configs.iter().map(|it|it.get_state()).collect::<Vec<_>>());

            if symbol != EOF {
                self.consume(lexer.input());
            }

            if self.capture_sim_state(dfa.as_ref().unwrap(), lexer.input(), target) && symbol == EOF
            {
                break;
            }

            symbol = lexer.input().la(1);

            s = target;
        }
        // let _last = self.get_dfa().states.read().get(s).unwrap();

        self.fail_or_accept(symbol, lexer, dfa.unwrap())
    }

    #[inline(always)]
    fn get_existing_target_state(dfa: &DFA, _s: DFAStateRef, t: i32) -> Option<DFAStateRef> {
        // if t < MIN_DFA_EDGE || t > MAX_DFA_EDGE {
        //     return None;
        // }

        dfa.states[_s]
            .edges
            .get((t - MIN_DFA_EDGE) as usize)
            .and_then(|x| match x {
                0 => None,
                x => Some(x),
            })
            .copied()
    }

    #[cold]
    fn compute_target_state<'input>(
        &self,
        dfa: &mut Option<RwLockUpgradableReadGuard<'_, DFA>>,
        s: DFAStateRef,
        _t: i32,
        lexer: &mut impl Lexer<'input>,
    ) -> DFAStateRef {
        let mut reach = ATNConfigSet::new_ordered();
        self.get_reachable_config_set(
            &dfa.as_ref().unwrap().states[s].configs,
            &mut reach,
            _t,
            lexer,
        );
        //        println!(" --- target computed {:?}", reach.configs.iter().map(|it|it.get_state()).collect::<Vec<_>>());

        let mut dfa_mut = RwLockUpgradableReadGuard::upgrade(dfa.take().unwrap());
        // let mut states = dfa_mut.states;
        if reach.is_empty() {
            if !reach.has_semantic_context() {
                self.add_dfaedge(&mut dfa_mut.states[s], _t, ERROR_DFA_STATE_REF);
            }
            *dfa = Some(RwLockWriteGuard::downgrade_to_upgradable(dfa_mut));
            return ERROR_DFA_STATE_REF;
        }

        let supress_edge = reach.has_semantic_context();
        reach.set_has_semantic_context(false);
        let to = self.add_dfastate(&mut dfa_mut, Box::new(reach));
        if !supress_edge {
            let from = &mut dfa_mut.states[s];
            self.add_dfaedge(from, _t, to);
        }
        //        println!("target state computed from {:?} to {:?} on symbol {}", _s, to, char::try_from(_t as u32).unwrap());
        *dfa = Some(RwLockWriteGuard::downgrade_to_upgradable(dfa_mut));
        to
        //        states.get(to).unwrap()
    }

    fn get_reachable_config_set<'input>(
        &self,
        // _states: &V,
        //        _input: &mut dyn CharStream,
        _closure: &ATNConfigSet,
        _reach: &mut ATNConfigSet,
        _t: i32,
        lexer: &mut impl Lexer<'input>,
    ) {
        let mut skip_alt = 0;
        //        println!(" --- source {:?}", _closure.configs.iter().map(|it|it.get_state()).collect::<Vec<_>>());
        for config in _closure.get_items() {
            let current_alt_reached_accept_state = config.get_alt() == skip_alt;
            if current_alt_reached_accept_state {
                if let ATNConfigType::LexerATNConfig {
                    passed_through_non_greedy_decision: true,
                    ..
                } = config.get_type()
                {
                    continue;
                }
            }
            let atn_state = self.atn().states[config.get_state() as usize].as_ref();
            for tr in atn_state.get_transitions() {
                if let Some(target) = tr.get_reachable_target(_t) {
                    let exec = config.get_lexer_executor().map(|x| {
                        x.clone()
                            .fix_offset_before_match(lexer.input().index() - self.start_index)
                    });

                    let new = config.cloned_with_new_exec(self.atn().states[target as usize].as_ref(), exec);
                    if self.closure(
                        new,
                        _reach,
                        current_alt_reached_accept_state,
                        true,
                        _t == EOF,
                        lexer,
                    ) {
                        skip_alt = config.get_alt();
                        break;
                    }
                }
            }
        }
    }

    //    fn get_reachable_target<T>(&self, states: &T, _trans: &Transition, _t: i32) -> &ATNState
    //    where
    //        T: Deref<Target = Vec<DFAState>>,
    //    {
    //        unimplemented!()
    //    }

    fn fail_or_accept<'input>(
        &mut self,
        _t: i32,
        lexer: &mut impl Lexer<'input>,
        dfa: RwLockUpgradableReadGuard<'_, DFA>,
    ) -> Result<i32, ANTLRError> {
        //        println!("fail_or_accept");
        if let Some(state) = self.prev_accept.dfa_state {
            //            let lexer_action_executor;
            self.accept(lexer.input());

            let prediction = {
                let dfa_state_prediction = &dfa.states[state];
                //                println!("accepted, prediction = {}, on dfastate {}", dfa_state_prediction.prediction, dfa_state_prediction.state_number);
                //                lexer_action_executor = dfa_state_prediction.lexer_action_executor.clone();
                //                let recog = self.recog.clone();
                if let Some(x) = dfa_state_prediction.lexer_action_executor.as_ref() {
                    x.execute(lexer, self.start_index)
                }

                dfa_state_prediction.prediction
            };

            //            self.lexer_action_executor = lexer_action_executor;
            Ok(prediction)
        } else {
            if _t == EOF && lexer.input().index() == self.start_index {
                return Ok(TOKEN_EOF);
            }
            Err(LexerNoAltError {
                start_index: self.start_index,
            })
        }
    }

    fn accept<'input>(&mut self, input: &mut impl IntStream) {
        input.seek(self.prev_accept.index);
        self.current_pos.line.set(self.prev_accept.line);
        self.current_pos
            .char_position_in_line
            .set(self.prev_accept.column);
    }

    fn compute_start_state<'input>(
        &self,
        _p: &dyn ATNState,
        lexer: &mut impl Lexer<'input>,
    ) -> Box<ATNConfigSet> {
        //        let initial_context = &EMPTY_PREDICTION_CONTEXT;
        let mut config_set = ATNConfigSet::new_ordered();

        for (i, tr) in _p.get_transitions().iter().enumerate() {
            let target = tr.get_target();
            let atn_config = ATNConfig::new_lexer_atnconfig6(
                target,
                (i + 1) as i32,
                EMPTY_PREDICTION_CONTEXT.clone(),
            );
            self.closure(atn_config, &mut config_set, false, false, false, lexer);
        }

        Box::new(config_set)
    }

    fn closure<'input>(
        &self,
        //        _input: &mut dyn CharStream,
        mut config: ATNConfig,
        _configs: &mut ATNConfigSet,
        mut _current_alt_reached_accept_state: bool,
        _speculative: bool,
        _treat_eofas_epsilon: bool,
        lexer: &mut impl Lexer<'input>,
    ) -> bool {
        //        let config = &config;
        let atn = self.atn();
        let state = atn.states[config.get_state() as usize].as_ref();
        //        println!("closure called on state {} {:?}", state.get_state_number(), state.get_state_type());

        if let ATNStateType::RuleStopState = state.get_state_type() {
            //            println!("reached rulestopstate {}",state.get_state_number());
            if config.get_context().map(|x| x.has_empty_path()) != Some(false) {
                if config.get_context().map(|x| x.is_empty()) != Some(false) {
                    _configs.add(Box::new(config));
                    return true;
                } else {
                    _configs.add(Box::new(
                        config.cloned_with_new_ctx(state, Some(EMPTY_PREDICTION_CONTEXT.clone())),
                    ));
                    _current_alt_reached_accept_state = true
                }
            }

            if config.get_context().map(|x| x.is_empty()) == Some(false) {
                let ctx = config.take_context();
                for i in 0..ctx.length() {
                    if ctx.get_return_state(i) != PREDICTION_CONTEXT_EMPTY_RETURN_STATE {
                        let new_ctx = ctx.get_parent(i).cloned();
                        let return_state =
                            self.atn().states[ctx.get_return_state(i) as usize].as_ref();
                        let next_config = config.cloned_with_new_ctx(return_state, new_ctx);
                        _current_alt_reached_accept_state = self.closure(
                            next_config,
                            _configs,
                            _current_alt_reached_accept_state,
                            _speculative,
                            _treat_eofas_epsilon,
                            lexer,
                        )
                    }
                }
            }

            return _current_alt_reached_accept_state;
        }

        if !state.has_epsilon_only_transitions() {
            if let ATNConfigType::LexerATNConfig {
                passed_through_non_greedy_decision,
                ..
            } = config.config_type
            {
                if !_current_alt_reached_accept_state || !passed_through_non_greedy_decision {
                    _configs.add(Box::new(config.clone()));
                }
            }
        }

        let state = atn.states[config.get_state() as usize].as_ref();

        for tr in state.get_transitions() {
            let c = self.get_epsilon_target(
                &mut config,
                tr.as_ref(),
                _configs,
                _speculative,
                _treat_eofas_epsilon,
                lexer,
            );

            if let Some(c) = c {
                _current_alt_reached_accept_state = self.closure(
                    c,
                    _configs,
                    _current_alt_reached_accept_state,
                    _speculative,
                    _treat_eofas_epsilon,
                    lexer,
                );
            }
        }

        _current_alt_reached_accept_state
    }

    fn get_epsilon_target<'input>(
        &self,
        //        _input: &mut dyn CharStream,
        _config: &mut ATNConfig,
        _trans: &dyn Transition,
        _configs: &mut ATNConfigSet,
        _speculative: bool,
        _treat_eofas_epsilon: bool,
        lexer: &mut impl Lexer<'input>,
    ) -> Option<ATNConfig> {
        let mut result = None;
        let target = self.atn().states.get(_trans.get_target() as usize).unwrap().as_ref();
        //        println!("epsilon target for {:?} is {:?}", _trans, target.get_state_type());
        match _trans.get_serialization_type() {
            TransitionType::TRANSITION_EPSILON => {
                result = Some(_config.cloned(target));
            }
            TransitionType::TRANSITION_RULE => {
                let rt = _trans.cast::<RuleTransition>();
                //println!("rule transition follow state{}", rt.follow_state);
                let pred_ctx = PredictionContext::new_singleton(
                    Some(_config.get_context().unwrap().clone()),
                    rt.follow_state as i32,
                );
                result = Some(_config.cloned_with_new_ctx(target, Some(pred_ctx.into())));
            }
            TransitionType::TRANSITION_PREDICATE => {
                let tr = _trans.cast::<PredicateTransition>();
                _configs.set_has_semantic_context(true);
                if self.evaluate_predicate(tr.rule_index, tr.pred_index, _speculative, lexer) {
                    result = Some(_config.cloned(target));
                }
            }
            TransitionType::TRANSITION_ACTION => {
                //println!("action transition");
                if _config.get_context().map(|x| x.has_empty_path()) != Some(false) {
                    if let ATNConfigType::LexerATNConfig {
                        lexer_action_executor,
                        ..
                    } = _config.get_type()
                    {
                        let tr = _trans.cast::<ActionTransition>();
                        let lexer_action =
                            self.atn().lexer_actions[tr.action_index as usize].clone();
                        //dbg!(&lexer_action);
                        let lexer_action_executor = LexerActionExecutor::new_copy_append(
                            lexer_action_executor.as_deref(),
                            lexer_action,
                        );
                        result =
                            Some(_config.cloned_with_new_exec(target, Some(lexer_action_executor)))
                    }
                } else {
                    result = Some(_config.cloned(target));
                }
            }
            TransitionType::TRANSITION_RANGE
            | TransitionType::TRANSITION_SET
            | TransitionType::TRANSITION_ATOM => {
                if _treat_eofas_epsilon
                    && _trans.matches(EOF, LEXER_MIN_CHAR_VALUE, LEXER_MAX_CHAR_VALUE)
                {
                    let target = self.atn().states[_trans.get_target() as usize].as_ref();
                    result = Some(_config.cloned(target));
                }
            }
            TransitionType::TRANSITION_WILDCARD => {}
            TransitionType::TRANSITION_NOTSET => {}
            TransitionType::TRANSITION_PRECEDENCE => {
                panic!("precedence predicates are not supposed to be in lexer");
            }
        }

        result
    }

    fn evaluate_predicate<'input, T: Lexer<'input>>(
        &self,
        //        input: &mut dyn CharStream,
        rule_index: i32,
        pred_index: i32,
        speculative: bool,
        lexer: &mut T,
    ) -> bool {
        if !speculative {
            return lexer.sempred(None, rule_index, pred_index);
        }

        let saved_column = self.current_pos.char_position_in_line.get();
        let saved_line = self.current_pos.line.get();
        let index = lexer.input().index();
        let marker = lexer.input().mark();
        self.consume(lexer.input());

        let result = lexer.sempred(None, rule_index, pred_index);

        self.current_pos.char_position_in_line.set(saved_column);
        self.current_pos.line.set(saved_line);
        lexer.input().seek(index);
        lexer.input().release(marker);
        result
    }

    fn capture_sim_state(
        &mut self,
        dfa: &DFA,
        input: &impl IntStream,
        dfa_state: DFAStateRef,
    ) -> bool {
        if dfa.states[dfa_state].is_accept_state {
            self.prev_accept = SimState {
                index: input.index(),
                line: self.current_pos.line.get(),
                column: self.current_pos.char_position_in_line.get(),
                dfa_state: Some(dfa_state),
            };
            // self.prev_accept.index = input.index();
            // self.prev_accept.dfa_state = Some(dfa_state);
            return true;
        }
        false
    }

    fn add_dfaedge(&self, _from: &mut DFAState, t: i32, _to: DFAStateRef) {
        if !(MIN_DFA_EDGE..=MAX_DFA_EDGE).contains(&t) {
            return;
        }

        if _from.edges.len() < (MAX_DFA_EDGE - MIN_DFA_EDGE + 1) as usize {
            _from
                .edges
                .resize((MAX_DFA_EDGE - MIN_DFA_EDGE + 1) as usize, 0);
        }
        _from.edges[(t - MIN_DFA_EDGE) as usize] = _to;
    }

    fn add_dfastate(&self, dfa: &mut DFA, _configs: Box<ATNConfigSet>) -> DFAStateRef
// where
    //     V: DerefMut<Target = Vec<DFAState>>,
    {
        assert!(!_configs.has_semantic_context());
        let mut dfastate = DFAState::new_dfastate(usize::MAX, _configs);
        let rule_index = dfastate
            .configs //_configs
            .get_items()
            .find(|c| RuleStopState == *self.atn().states[c.get_state() as usize].get_state_type())
            .map(|c| {
                let rule_index = self.atn().states[c.get_state() as usize].get_rule_index();

                //println!("accepted rule {} on state {}",rule_index,c.get_state());
                (
                    self.atn().rule_to_token_type[rule_index as usize],
                    c.get_lexer_executor().cloned().map(Box::new),
                )
            });

        if let Some((prediction, exec)) = rule_index {
            dfastate.prediction = prediction;
            dfastate.lexer_action_executor = exec;
            dfastate.is_accept_state = true;
        }

        let states = &mut dfa.states;
        let key = dfastate.default_hash();
        let dfastate_index: DFAStateRef = if let Some(entry) = dfa.states_map.get(&key) {
            let find_result = entry.iter().find(|it| {
                states[**it].configs == dfastate.configs
            });
            if let Some(find_result) = find_result {
                *find_result
            } else {
                dfastate.state_number = states.deref().len();
                dfastate.configs.set_read_only(true);
                let state_number = dfastate.state_number;
                let mut new_vec = entry.to_vec();
                new_vec.push(state_number);
                dfa.states_map.insert(key, new_vec);
                //println!("inserting new DFA state {} with size {}", i, dfastate.configs.length());
                states.push(dfastate);
                state_number
            }
        } else {
            dfastate.state_number = states.deref().len();
            dfastate.configs.set_read_only(true);
            let state_number = dfastate.state_number;
            //println!("inserting new DFA state {} with size {}", i, dfastate.configs.length());
            states.push(dfastate);
            dfa.states_map.insert(key, vec![state_number]);
            state_number
        };
        //println!("new DFA state {}", dfastate_index);

        //        dfa.states.write().unwrap().get_mut(*dfastate_index).unwrap()
        dfastate_index
    }

    /// Returns current DFA that is currently used.
    pub fn get_dfa(&self) -> &RwLock<DFA> {
        &self.decision_to_dfa()[self.mode]
    }

    /// Returns current DFA for particular lexer mode
    pub fn get_dfa_for_mode(&self, mode: usize) -> &RwLock<DFA> {
        &self.decision_to_dfa()[mode]
    }

    // fn get_token_name(&self, _tt: i32) -> String { unimplemented!() }

    // fn reset_sim_state(_sim: &mut SimState) { unimplemented!() }
}

#[derive(Debug)]
pub(crate) struct SimState {
    index: isize,
    line: isize,
    column: isize,
    dfa_state: Option<usize>,
}

impl SimState {
    pub(crate) fn new() -> SimState {
        SimState {
            index: -1,
            line: 0,
            column: -1,
            dfa_state: None,
        }
    }

    pub(crate) fn reset(&mut self) {
        // self.index = -1;
        // self.line = 0;
        // self.column = -1;
        self.dfa_state = None;
    }
}
