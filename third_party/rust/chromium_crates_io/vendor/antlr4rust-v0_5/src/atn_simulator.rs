use std::fmt::{Debug, Error, Formatter};
use std::ops::Deref;
use std::sync::Arc;

use crate::atn::ATN;
use crate::dfa::DFA;
use crate::prediction_context::PredictionContextCache;
use parking_lot::RwLock;

pub trait IATNSimulator {
    fn shared_context_cache(&self) -> &PredictionContextCache;
    fn atn(&self) -> &ATN;
    fn decision_to_dfa(&self) -> &Vec<RwLock<DFA>>;
}

pub struct BaseATNSimulator {
    pub atn: Arc<ATN>,
    pub shared_context_cache: Arc<PredictionContextCache>,
    pub decision_to_dfa: Arc<Vec<RwLock<DFA>>>,
}

impl Debug for BaseATNSimulator {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), Error> {
        f.write_str("BaseATNSimulator { .. }")
    }
}

impl BaseATNSimulator {
    pub fn new_base_atnsimulator(
        atn: Arc<ATN>,
        decision_to_dfa: Arc<Vec<RwLock<DFA>>>,
        shared_context_cache: Arc<PredictionContextCache>,
    ) -> BaseATNSimulator {
        BaseATNSimulator {
            atn,
            shared_context_cache,
            decision_to_dfa,
        }
    }
}

impl IATNSimulator for BaseATNSimulator {
    fn shared_context_cache(&self) -> &PredictionContextCache {
        self.shared_context_cache.deref()
    }

    fn atn(&self) -> &ATN {
        self.atn.as_ref()
    }

    fn decision_to_dfa(&self) -> &Vec<RwLock<DFA>> {
        self.decision_to_dfa.as_ref()
    }
}
