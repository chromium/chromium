use std::collections::HashSet;
use std::ops::Deref;
use std::sync::Arc;

use bit_set::BitSet;

use crate::atn::ATN;
use crate::atn_config::ATNConfig;
use crate::atn_state::{ATNState, ATNStateType};
use crate::interval_set::IntervalSet;
use crate::parser::ParserNodeType;
use crate::prediction_context::PredictionContext;
use crate::prediction_context::EMPTY_PREDICTION_CONTEXT;
use crate::token::{TOKEN_EOF, TOKEN_EPSILON, TOKEN_INVALID_TYPE, TOKEN_MIN_USER_TOKEN_TYPE};
use crate::transition::TransitionType::TRANSITION_NOTSET;
use crate::transition::{RuleTransition, TransitionType};

pub struct LL1Analyzer<'a> {
    atn: &'a ATN,
}

impl LL1Analyzer<'_> {
    pub fn new(atn: &ATN) -> LL1Analyzer<'_> {
        LL1Analyzer { atn }
    }

    //    fn get_decision_lookahead(&self, _s: &dyn ATNState) -> &Vec<IntervalSet> { unimplemented!() }

    pub fn look<'input, Ctx: ParserNodeType<'input>>(
        &self,
        s: &dyn ATNState,
        stop_state: Option<&dyn ATNState>,
        ctx: Option<&Ctx::Type>,
    ) -> IntervalSet {
        let mut r = IntervalSet::new();
        let look_ctx = ctx.map(|x| PredictionContext::from_rule_context::<Ctx>(self.atn, x));
        let mut looks_busy: HashSet<ATNConfig> = HashSet::new();
        let mut called_rule_stack = BitSet::new();
        self.look_work(
            s,
            stop_state,
            look_ctx,
            &mut r,
            &mut looks_busy,
            &mut called_rule_stack,
            true,
            true,
        );
        r
    }

    fn look_work(
        &self,
        //                 atn:&ATN,
        s: &dyn ATNState,
        stop_state: Option<&dyn ATNState>,
        ctx: Option<Arc<PredictionContext>>,
        look: &mut IntervalSet,
        look_busy: &mut HashSet<ATNConfig>,
        called_rule_stack: &mut BitSet,
        see_thru_preds: bool,
        add_eof: bool,
    ) {
        let c = ATNConfig::new(s.get_state_number(), 0, ctx.clone());
        if !look_busy.insert(c) {
            return;
        }

        if Some(s.get_state_number()) == stop_state.map(|x| x.get_state_number()) {
            match ctx {
                None => {
                    look.add_one(TOKEN_EPSILON);
                    return;
                }
                Some(x) if x.is_empty() && add_eof => {
                    look.add_one(TOKEN_EOF);
                    return;
                }
                _ => {}
            }
        }

        if let ATNStateType::RuleStopState = s.get_state_type() {
            match ctx {
                None => {
                    look.add_one(TOKEN_EPSILON);
                    return;
                }
                Some(x) if x.is_empty() && add_eof => {
                    look.add_one(TOKEN_EOF);
                    return;
                }
                Some(ctx) if ctx != *EMPTY_PREDICTION_CONTEXT => {
                    let removed = called_rule_stack.contains(s.get_rule_index() as usize);
                    called_rule_stack.remove(s.get_rule_index() as usize);
                    for i in 0..ctx.length() {
                        self.look_work(
                            self.atn.states[ctx.get_return_state(i) as usize].as_ref(),
                            stop_state,
                            ctx.get_parent(i).cloned(),
                            look,
                            look_busy,
                            called_rule_stack,
                            see_thru_preds,
                            add_eof,
                        )
                    }
                    if removed {
                        called_rule_stack.insert(s.get_rule_index() as usize);
                    }

                    return;
                }
                _ => {}
            }
        }

        for tr in s.get_transitions() {
            let target = self.atn.states[tr.get_target() as usize].as_ref();
            match tr.get_serialization_type() {
                TransitionType::TRANSITION_RULE => {
                    let rule_tr = tr.as_ref().cast::<RuleTransition>();
                    if called_rule_stack.contains(target.get_rule_index() as usize) {
                        continue;
                    }

                    let new_ctx = Arc::new(PredictionContext::new_singleton(
                        ctx.clone(),
                        rule_tr.follow_state as i32,
                    ));

                    called_rule_stack.insert(target.get_rule_index() as usize);
                    self.look_work(
                        target,
                        stop_state,
                        Some(new_ctx),
                        look,
                        look_busy,
                        called_rule_stack,
                        see_thru_preds,
                        add_eof,
                    );
                    called_rule_stack.remove(target.get_rule_index() as usize);
                }
                TransitionType::TRANSITION_PREDICATE | TransitionType::TRANSITION_PRECEDENCE => {
                    if see_thru_preds {
                        self.look_work(
                            target,
                            stop_state,
                            ctx.clone(),
                            look,
                            look_busy,
                            called_rule_stack,
                            see_thru_preds,
                            add_eof,
                        )
                    } else {
                        look.add_one(TOKEN_INVALID_TYPE);
                    }
                }
                TransitionType::TRANSITION_WILDCARD => {
                    look.add_range(TOKEN_MIN_USER_TOKEN_TYPE, self.atn.max_token_type)
                }
                _ if tr.is_epsilon() => self.look_work(
                    target,
                    stop_state,
                    ctx.clone(),
                    look,
                    look_busy,
                    called_rule_stack,
                    see_thru_preds,
                    add_eof,
                ),
                _ => {
                    if let Some(mut set) = tr.get_label() {
                        if tr.get_serialization_type() == TRANSITION_NOTSET {
                            let complement =
                                set.complement(TOKEN_MIN_USER_TOKEN_TYPE, self.atn.max_token_type);
                            *set.to_mut() = complement;
                        }
                        look.add_set(set.deref())
                    }
                }
            }
        }
    }
}
