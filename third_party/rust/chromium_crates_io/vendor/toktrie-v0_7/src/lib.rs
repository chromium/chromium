use serde::{Deserialize, Serialize};

pub mod bytes;
pub mod recognizer;
pub mod rng;
mod svob;
mod tokenv;
mod toktree;

pub use svob::{SimpleVob, SimpleVobIter};
pub use tokenv::{parse_numeric_token, ApproximateTokEnv, TokEnv, TokEnvWithTrie, TokenizerEnv};
pub use toktree::{AnythingGoes, Recognizer, TokRxInfo, TokTrie, TokenId, TrieNode, INVALID_TOKEN};

/// Defines what is allowed in Branch
#[derive(Serialize, Deserialize, Clone, Debug, Default)]
pub struct InferenceCapabilities {
    /// Unconditional splice is allowed.
    #[serde(default)]
    pub ff_tokens: bool,

    /// Conditional (and unconditional) splices are allowed.
    #[serde(default)]
    pub conditional_ff_tokens: bool,

    /// Backtracking is allowed.
    #[serde(default)]
    pub backtrack: bool,

    /// More than one branch is allowed.
    #[serde(default)]
    pub fork: bool,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct StepArg {
    /// Sampling result for the previous iteration.
    /// For simple sampled token 't', backtrack==0 and tokens==[t].
    /// For first request, backtrack==0 and tokens==[] (prompt is passed separately, before).
    /// Can be more complex when splices are used.
    pub backtrack: u32,
    pub tokens: Vec<TokenId>,
    /// The token that was sampled (after applying the mask), before any splicing.
    pub sampled: Option<TokenId>,
}

impl StepArg {
    pub fn empty() -> Self {
        StepArg {
            backtrack: 0,
            tokens: vec![],
            sampled: None,
        }
    }

    pub fn save_tokens(&self, acc_tokens: &mut Vec<TokenId>) {
        let bt = self.backtrack as usize;
        assert!(
            bt <= acc_tokens.len(),
            "attempting to backtrack past beginning"
        );
        acc_tokens.truncate(acc_tokens.len() - bt);
        acc_tokens.extend_from_slice(&self.tokens);
    }

    pub fn from_splice(s: &Splice, sampled: Option<TokenId>) -> Self {
        StepArg {
            backtrack: s.backtrack,
            tokens: s.ff_tokens.clone(),
            sampled,
        }
    }

    pub fn from_sampled_token(tok: TokenId) -> Self {
        StepArg {
            backtrack: 0,
            tokens: vec![tok],
            sampled: Some(tok),
        }
    }
}

/*
For example, if we're generating JSON, according to the following schema:
{
  "type": "object",
  "properties": {
    "name": {"type": "string"},
    "age": {"type": "integer"}
  }
}

Let's say we have generated: {"name": "something
We would use a single splice:
    when_sampled: ['"', '",', '", '],
    backtrack: 1,
    ff_tokens: tokenize('", "age": ')
Which means: when any token starting with '"' is sampled, we remove it (backtrack: 1)
and then append the next full fragment of JSON '", "age": '

If the tokenizers has tokens like 'a"', 'b"' etc, then we would need many splices
(there may be limits how many we want to pass over the IPC boundary).
*/

/// Describes what to do after sampling.
#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct Splice {
    /// If one of the tokens in when_sampled is sampled, this sequence is appended.
    /// When empty, this sequence is appended unconditionally, regardless of sampling.
    pub when_sampled: Vec<TokenId>,
    /// Backtrack this much before appending this sequence (this includes sampled token if any).
    pub backtrack: u32,
    /// Append these tokens after backtracking.
    pub ff_tokens: Vec<TokenId>,
}

impl Splice {
    pub fn noop() -> Self {
        Splice {
            when_sampled: vec![],
            backtrack: 0,
            ff_tokens: vec![],
        }
    }

    pub fn tokens(ff_tokens: Vec<TokenId>) -> Self {
        Splice {
            when_sampled: vec![],
            backtrack: 0,
            ff_tokens,
        }
    }
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Branch<S> {
    /// If None, no sampling is performed.
    /// If Some(set), only tokens from the set are allowed.
    pub sample_mask: Option<S>,
    /// Override temperature for sampling. It may or may not be sticky.
    pub temperature: Option<f32>,
    /// Describes what to do after sampling.
    /// If no sampling, there should be exactly one splice, with empty `when_sampled`.
    pub splices: Vec<Splice>,
}

impl<S: Clone> Clone for Branch<S> {
    fn clone(&self) -> Self {
        Branch {
            sample_mask: self.sample_mask.clone(),
            temperature: self.temperature,
            splices: self.splices.clone(),
        }
    }
}

impl<S> Branch<S> {
    pub fn map_mask<F, T>(&self, f: F) -> Branch<T>
    where
        F: FnOnce(&S) -> T,
    {
        Branch {
            sample_mask: self.sample_mask.as_ref().map(f),
            temperature: self.temperature,
            splices: self.splices.clone(),
        }
    }

    pub fn find_splice(&self, sampled: TokenId) -> Option<&Splice> {
        self.splices
            .iter()
            .find(|s| s.when_sampled.is_empty() || s.when_sampled.contains(&sampled))
    }

    pub fn spliced(&self, sampled: TokenId) -> Splice {
        self.find_splice(sampled)
            .cloned()
            .unwrap_or_else(|| Splice {
                when_sampled: vec![],
                backtrack: 0,
                ff_tokens: vec![sampled],
            })
    }

    pub fn unconditional_splice(&self) -> Option<&Splice> {
        if self.splices.len() == 1 && self.splices[0].when_sampled.is_empty() {
            Some(&self.splices[0])
        } else {
            None
        }
    }

    pub fn has_backtrack(&self) -> bool {
        let max_bt = if self.sample_mask.is_none() { 0 } else { 1 };
        self.splices.iter().any(|s| s.backtrack > max_bt)
    }

    pub fn has_ff_tokens(&self) -> bool {
        !self.splices.is_empty()
    }

    pub fn stop() -> Self {
        Branch {
            sample_mask: None,
            temperature: None,
            splices: vec![],
        }
    }

    pub fn is_stop(&self) -> bool {
        self.sample_mask.is_none() && self.splices.is_empty()
    }

    pub fn splice(backtrack: u32, ff_tokens: Vec<TokenId>) -> Self {
        Branch {
            sample_mask: None,
            temperature: None,
            splices: vec![Splice {
                when_sampled: vec![],
                backtrack,
                ff_tokens,
            }],
        }
    }

    pub fn noop() -> Self {
        Self::splice(0, vec![])
    }

    pub fn sample(set: S, temperature: Option<f32>) -> Self {
        Branch {
            sample_mask: Some(set),
            temperature,
            splices: vec![],
        }
    }
}

pub type StepResult = Branch<SimpleVob>;
