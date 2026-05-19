//! The gramatron grammar fuzzer
use alloc::{rc::Rc, string::String, vec::Vec};
use core::{
    cell::RefCell,
    hash::{BuildHasher, Hasher},
};

use ahash::RandomState;
use libafl_bolts::{Error, HasLen};
use serde::{Deserialize, Serialize};

use crate::{corpus::CorpusId, inputs::Input};

/// A terminal for gramatron grammar fuzzing
#[derive(Serialize, Deserialize, Debug, Clone, Default, PartialEq, Eq, Hash)]
pub struct Terminal {
    /// The state
    pub state: usize,
    /// The trigger index
    pub trigger_idx: usize,
    /// The symbol
    pub symbol: String,
}

impl Terminal {
    /// Creates a new [`Terminal`]
    #[must_use]
    pub fn new(state: usize, trigger_idx: usize, symbol: String) -> Self {
        Self {
            state,
            trigger_idx,
            symbol,
        }
    }
}

/// An input for gramatron grammar fuzzing
#[derive(Serialize, Deserialize, Debug, Clone, Default, PartialEq, Eq, Hash)]
pub struct GramatronInput {
    /// The input representation as list of terminals
    terms: Vec<Terminal>,
}

impl Input for GramatronInput {
    /// Generate a name for this input
    fn generate_name(&self, _id: Option<CorpusId>) -> String {
        let mut hasher = RandomState::with_seeds(0, 0, 0, 0).build_hasher();
        for term in &self.terms {
            hasher.write(term.symbol.as_bytes());
        }
        format!("{:016x}", hasher.finish())
    }
}

/// Rc Ref-cell from Input
impl From<GramatronInput> for Rc<RefCell<GramatronInput>> {
    fn from(input: GramatronInput) -> Self {
        Rc::new(RefCell::new(input))
    }
}

impl HasLen for GramatronInput {
    #[inline]
    fn len(&self) -> usize {
        self.terms.len()
    }
}

impl GramatronInput {
    /// Creates a new codes input using the given terminals
    #[must_use]
    pub fn new(terms: Vec<Terminal>) -> Self {
        Self { terms }
    }

    /// The terminals of this input
    #[must_use]
    pub fn terminals(&self) -> &[Terminal] {
        &self.terms
    }

    /// The terminals of this input, mutable
    #[must_use]
    pub fn terminals_mut(&mut self) -> &mut Vec<Terminal> {
        &mut self.terms
    }

    /// Create a bytes representation of this input
    pub fn unparse(&self, bytes: &mut Vec<u8>) {
        bytes.clear();
        for term in &self.terms {
            bytes.extend_from_slice(term.symbol.as_bytes());
        }
    }

    /// Crop the value to the given length
    pub fn crop(&self, from: usize, to: usize) -> Result<Self, Error> {
        if from < to && to <= self.terms.len() {
            let mut terms = vec![];
            terms.clone_from_slice(&self.terms[from..to]);
            Ok(Self { terms })
        } else {
            Err(Error::illegal_argument("Invalid from or to argument"))
        }
    }
}
