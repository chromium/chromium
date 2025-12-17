use std::fmt::{Display, Error, Formatter};
use std::hash::{Hash, Hasher};

use murmur3::murmur3_32::MurmurHasher;

use crate::atn_config_set::ATNConfigSet;
use crate::lexer_action_executor::LexerActionExecutor;
use crate::semantic_context::SemanticContext;

#[derive(Eq, PartialEq, Debug)]
pub struct PredPrediction {
    pub(crate) alt: i32,
    pub(crate) pred: SemanticContext,
}

impl Display for PredPrediction {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), Error> {
        f.write_fmt(format_args!("({},{:?})", self.alt, self.pred))
    }
}

//index in DFA.states
pub type DFAStateRef = usize;

#[derive(Eq, Debug)]
pub struct DFAState {
    /// Number of this state in corresponding DFA
    pub state_number: usize,
    pub configs: Box<ATNConfigSet>,
    /// - 0 => no edge
    /// - usize::MAX => error edge
    /// - _ => actual edge
    pub edges: Vec<DFAStateRef>,
    pub is_accept_state: bool,

    pub prediction: i32,
    pub(crate) lexer_action_executor: Option<Box<LexerActionExecutor>>,
    pub requires_full_context: bool,
    pub predicates: Vec<PredPrediction>,
}

impl PartialEq for DFAState {
    fn eq(&self, other: &Self) -> bool {
        self.configs == other.configs
    }
}

impl Hash for DFAState {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.configs.hash(state);
    }
}

impl DFAState {
    pub fn default_hash(&self) -> u64 {
        let mut hasher = MurmurHasher::default();
        self.hash(&mut hasher);
        hasher.finish()
    }

    pub fn new_dfastate(state_number: usize, configs: Box<ATNConfigSet>) -> DFAState {
        DFAState {
            state_number,
            configs,
            //            edges: Vec::with_capacity((MAX_DFA_EDGE - MIN_DFA_EDGE + 1) as usize),
            edges: Vec::new(),
            is_accept_state: false,
            prediction: 0,
            lexer_action_executor: None,
            requires_full_context: false,
            predicates: Vec::new(),
        }
    }

    //    fn get_alt_set(&self) -> &Set { unimplemented!() }

    // fn set_prediction(&self, _v: i32) { unimplemented!() }
}
