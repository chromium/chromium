use std::fmt::{Debug, Error, Formatter};
use std::hash::{Hash, Hasher};
use std::sync::Arc;

use murmur3::murmur3_32::MurmurHasher;

use crate::atn_config::ATNConfigType::LexerATNConfig;
use crate::atn_state::{ATNState, ATNStateRef, ATNStateType};
use crate::dfa::ScopeExt;
use crate::lexer_action_executor::LexerActionExecutor;
use crate::prediction_context::PredictionContext;
use crate::semantic_context::SemanticContext;

#[derive(Clone)]
pub struct ATNConfig {
    precedence_filter_suppressed: bool,
    //todo since ATNState is immutable when we started working with ATNConfigs
    // looks like it is possible to have usual reference here
    state: ATNStateRef,
    alt: i32,
    //todo maybe option is unnecessary and PredictionContext::EMPTY would be enough
    //another todo check arena alloc
    context: Option<Arc<PredictionContext>>,
    pub semantic_context: Box<SemanticContext>,
    pub reaches_into_outer_context: i32,
    pub(crate) config_type: ATNConfigType,
}

impl Eq for ATNConfig {}

impl PartialEq for ATNConfig {
    fn eq(&self, other: &Self) -> bool {
        self.get_state() == other.get_state()
            && self.get_alt() == other.get_alt()
            // Arc is optimized to not do a deep equalitiy if arc pointers are equal so that's enough
            && self.context == other.context
            && self.get_type() == other.get_type()
            && self.semantic_context == other.semantic_context
            && self.precedence_filter_suppressed == other.precedence_filter_suppressed
    }
}

impl Hash for ATNConfig {
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_i32(self.get_state() as i32);
        state.write_i32(self.get_alt() as i32);
        match self.get_context() {
            None => state.write_i32(0),
            Some(c) => c.hash(state),
        }
        self.semantic_context.hash(state);
        if let LexerATNConfig {
            lexer_action_executor,
            passed_through_non_greedy_decision,
        } = &self.config_type
        {
            state.write_i32(if *passed_through_non_greedy_decision {
                1
            } else {
                0
            });
            match lexer_action_executor {
                None => state.write_i32(0),
                Some(ex) => ex.hash(state),
            }
        }
    }
}

impl Debug for ATNConfig {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), Error> {
        f.write_fmt(format_args!(
            "({},{},[{}]",
            self.state,
            self.alt,
            self.context.as_deref().unwrap()
        ))?;
        if self.reaches_into_outer_context > 0 {
            f.write_fmt(format_args!(",up={}", self.reaches_into_outer_context))?;
        }

        f.write_str(")")
    }
}

#[derive(Eq, PartialEq, Clone, Debug)]
pub(crate) enum ATNConfigType {
    BaseATNConfig,
    LexerATNConfig {
        lexer_action_executor: Option<Box<LexerActionExecutor>>,
        passed_through_non_greedy_decision: bool,
    },
}

impl ATNConfig {
    pub(crate) fn get_lexer_executor(&self) -> Option<&LexerActionExecutor> {
        match &self.config_type {
            ATNConfigType::BaseATNConfig => None,
            ATNConfigType::LexerATNConfig {
                lexer_action_executor,
                ..
            } => lexer_action_executor.as_deref(),
        }
    }

    pub fn default_hash(&self) -> u64 {
        MurmurHasher::default().convert_with(|mut x| {
            self.hash(&mut x);
            x.finish()
        })
    }

    pub fn new(
        state: ATNStateRef,
        alt: i32,
        context: Option<Arc<PredictionContext>>,
    ) -> ATNConfig {
        ATNConfig {
            precedence_filter_suppressed: false,
            state,
            alt,
            context,
            semantic_context: Box::new(SemanticContext::NONE),
            reaches_into_outer_context: 0,
            config_type: ATNConfigType::BaseATNConfig,
        }
    }

    pub fn new_with_semantic(
        state: ATNStateRef,
        alt: i32,
        context: Option<Arc<PredictionContext>>,
        semantic_context: Box<SemanticContext>,
    ) -> ATNConfig {
        let mut new = Self::new(state, alt, context);
        new.semantic_context = semantic_context;
        new
    }

    pub fn new_lexer_atnconfig6(
        _state: ATNStateRef,
        _alt: i32,
        _context: Arc<PredictionContext>,
    ) -> ATNConfig {
        let mut atnconfig = ATNConfig::new(_state, _alt, Some(_context));
        atnconfig.config_type = ATNConfigType::LexerATNConfig {
            lexer_action_executor: None,
            passed_through_non_greedy_decision: false,
        };
        atnconfig
    }

    pub fn cloned_with_new_semantic(
        &self,
        target: &dyn ATNState,
        ctx: Box<SemanticContext>,
    ) -> ATNConfig {
        let mut new = self.cloned(target);
        new.semantic_context = ctx;
        new
    }

    pub fn cloned(&self, target: &dyn ATNState) -> ATNConfig {
        //        println!("depth {}",PredictionContext::size(self.context.as_deref()));
        let mut new = self.clone();
        new.state = target.get_state_number();
        if let ATNConfigType::LexerATNConfig {
            passed_through_non_greedy_decision,
            ..
        } = &mut new.config_type
        {
            *passed_through_non_greedy_decision = check_non_greedy_decision(self, target);
        }
        new
    }

    pub fn cloned_with_new_ctx(
        &self,
        target: &dyn ATNState,
        ctx: Option<Arc<PredictionContext>>,
    ) -> ATNConfig {
        let mut new = self.cloned(target);
        new.context = ctx;

        new
    }

    pub(crate) fn cloned_with_new_exec(
        &self,
        target: &dyn ATNState,
        exec: Option<LexerActionExecutor>,
    ) -> ATNConfig {
        let mut new = self.cloned(target);
        if let ATNConfigType::LexerATNConfig {
            lexer_action_executor,
            passed_through_non_greedy_decision: _,
        } = &mut new.config_type
        {
            *lexer_action_executor = exec.map(Box::new);
            //            *passed_through_non_greedy_decision = check_non_greedy_decision(self, target);
        }
        new
    }

    pub fn get_state(&self) -> ATNStateRef {
        self.state
    }

    pub fn get_alt(&self) -> i32 {
        self.alt
    }

    pub(crate) fn get_type(&self) -> &ATNConfigType {
        &self.config_type
    }

    pub fn get_context(&self) -> Option<&Arc<PredictionContext>> {
        self.context.as_ref()
    }

    pub fn take_context(&mut self) -> Arc<PredictionContext> {
        self.context.take().unwrap()
    }

    pub fn set_context(&mut self, _v: Arc<PredictionContext>) {
        self.context = Some(_v);
    }

    pub fn get_reaches_into_outer_context(&self) -> i32 {
        self.reaches_into_outer_context
    }

    pub fn set_reaches_into_outer_context(&mut self, _v: i32) {
        self.reaches_into_outer_context = _v
    }

    pub fn is_precedence_filter_suppressed(&self) -> bool {
        self.precedence_filter_suppressed
    }

    pub fn set_precedence_filter_suppressed(&mut self, _v: bool) {
        self.precedence_filter_suppressed = _v;
    }
}

fn check_non_greedy_decision(source: &ATNConfig, target: &dyn ATNState) -> bool {
    if let LexerATNConfig {
        passed_through_non_greedy_decision: true,
        ..
    } = source.get_type()
    {
        return true;
    }
    if let ATNStateType::DecisionState {
        nongreedy: true, ..
    } = target.get_state_type()
    {
        return true;
    }
    false
}
