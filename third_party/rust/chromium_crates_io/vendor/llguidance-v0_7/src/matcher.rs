use anyhow::{anyhow, ensure, Result};
use toktrie::{SimpleVob, TokEnv, TokenId};

use crate::{api::StopReason, earley::ParserStats, panic_utils, TokenParser};

#[derive(Clone)]
struct MatcherInner {
    parser: TokenParser,
}

#[derive(Clone)]
#[allow(clippy::large_enum_variant)]
enum MatcherState {
    Normal(MatcherInner),
    Error(String),
}

/// This is meant to be used in server-side scenarios.
/// The Constraint interface is more for usage in Python Guidance.
#[derive(Clone)]
pub struct Matcher(MatcherState);

impl Matcher {
    pub fn new(parser: Result<TokenParser>) -> Self {
        match parser {
            Ok(mut parser) => {
                let caps = &parser.inference_caps;
                if caps.backtrack {
                    Self::new(Err(anyhow!("backtracking not supported")))
                } else {
                    // rest of caps is ignored
                    if parser.is_fresh() {
                        parser.start_without_prompt();
                    }
                    Matcher(MatcherState::Normal(MatcherInner { parser }))
                }
            }
            Err(e) => Matcher(MatcherState::Error(e.to_string())),
        }
    }

    fn with_inner<T>(&mut self, f: impl FnOnce(&mut MatcherInner) -> Result<T>) -> Result<T> {
        match &mut self.0 {
            MatcherState::Normal(ref mut inner) => {
                // We catch any panics here and transform them into regular errors.
                // They shouldn't happen, but if they do, we don't want to crash the whole program.
                let r = panic_utils::catch_unwind(std::panic::AssertUnwindSafe(|| f(inner)));
                match r {
                    Ok(r) => Ok(r),
                    Err(e) => {
                        self.0 = MatcherState::Error(e.to_string());
                        Err(e)
                    }
                }
            }
            MatcherState::Error(e) => Err(anyhow!("{}", e)),
        }
    }

    pub fn deep_clone(&self) -> Self {
        match &self.0 {
            MatcherState::Normal(inner) => {
                let parser = inner.parser.deep_clone();
                Self::new(Ok(parser))
            }
            MatcherState::Error(_) => self.clone(),
        }
    }

    /// Advance the parser by one token.
    /// Also checks if the parser should stop after consuming the tokens
    /// and puts the parser in stop state if necessary.
    pub fn consume_tokens(&mut self, tokens: &[TokenId]) -> Result<()> {
        self.with_inner(|inner| {
            for &t in tokens {
                let bt = inner.parser.consume_token(t)?;
                ensure!(bt == 0, "unexpected backtracking");
            }
            let _ = inner.parser.check_stop()?;
            Ok(())
        })
    }

    pub fn consume_token(&mut self, token: TokenId) -> Result<()> {
        self.consume_tokens(&[token])
    }

    pub fn rollback(&mut self, num_tokens: usize) -> Result<()> {
        self.with_inner(|inner| inner.parser.rollback(num_tokens))
    }

    pub fn reset(&mut self) -> Result<()> {
        self.with_inner(|inner| inner.parser.reset())
    }

    /// Compute which tokens can be consumed in the current state.
    pub fn compute_mask(&mut self) -> Result<SimpleVob> {
        self.with_inner(|inner| inner.parser.compute_mask())
    }

    /// Compute which tokens can be consumed in the current state.
    /// Returns a mask with just the EOS token if the parser is stopped.
    /// May still fail if the parser is in an error state.
    pub fn compute_mask_or_eos(&mut self) -> Result<SimpleVob> {
        self.with_inner(|inner| {
            if inner.parser.stop_reason() != StopReason::NotStopped {
                let trie = inner.parser.token_env.tok_trie();
                Ok(trie.singleton_token_set(trie.eos_token()))
            } else {
                inner.parser.compute_mask()
            }
        })
    }

    /// Can the grammar be finished in the current state?
    /// In other words, would the current token mask allow EOS token?
    pub fn is_accepting(&mut self) -> Result<bool> {
        self.with_inner(|inner| Ok(inner.parser.is_accepting()))
    }

    pub fn is_stopped(&self) -> bool {
        match &self.0 {
            MatcherState::Normal(inner) => inner.parser.stop_reason() != StopReason::NotStopped,
            MatcherState::Error(_) => true,
        }
    }

    pub fn stop_reason(&self) -> StopReason {
        match &self.0 {
            MatcherState::Normal(inner) => inner.parser.stop_reason(),
            MatcherState::Error(_) => StopReason::InternalError,
        }
    }

    /// This will always return [] for non-canonical tokenizers.
    pub fn compute_ff_tokens(&mut self) -> Vec<TokenId> {
        self.with_inner(|inner| Ok(inner.parser.compute_ff_tokens()))
            .unwrap_or_else(|_| vec![])
    }

    pub fn consume_ff_tokens(&mut self) -> Vec<TokenId> {
        let toks = self.compute_ff_tokens();
        if !toks.is_empty() {
            let _ = self.consume_tokens(&toks);
        }
        toks
    }

    /// Return any bytes that are forced by the current parser state.
    /// This also works for non-canonical tokenizers.
    pub fn compute_ff_bytes(&mut self) -> Vec<u8> {
        self.with_inner(|inner| Ok(inner.parser.force_bytes()))
            .unwrap_or_else(|_| vec![])
    }

    /// Tries to advance the parser by consuming the given tokens.
    /// Returns the number of tokens consumed.
    /// Also checks if the parser should stop after consuming the tokens
    /// and puts the parser in stop state if necessary.
    pub fn try_consume_tokens(&mut self, tokens: &[TokenId]) -> Result<usize> {
        self.with_inner(|inner| {
            for (idx, &t) in tokens.iter().enumerate() {
                if !inner.parser.validate_token(t)? {
                    return Ok(idx);
                }
                let bt = inner.parser.consume_token(t)?;
                ensure!(bt == 0, "unexpected backtracking");
            }
            let _ = inner.parser.check_stop()?;
            Ok(tokens.len())
        })
    }

    pub fn validate_tokens(&mut self, tokens: &[TokenId]) -> Result<usize> {
        self.with_inner(|inner| inner.parser.validate_tokens_raw(tokens))
    }

    pub fn is_error(&self) -> bool {
        matches!(self.0, MatcherState::Error(_))
    }

    pub fn get_error(&self) -> Option<String> {
        match &self.0 {
            MatcherState::Normal(_) => None,
            MatcherState::Error(e) => Some(e.clone()),
        }
    }

    pub fn tok_env(&self) -> Result<TokEnv> {
        match &self.0 {
            MatcherState::Normal(inner) => Ok(inner.parser.token_env.clone()),
            MatcherState::Error(e) => Err(anyhow!("{}", e)),
        }
    }

    pub fn last_step_stats(&self) -> Result<&ParserStats> {
        match &self.0 {
            MatcherState::Normal(inner) => Ok(inner.parser.last_step_stats()),
            MatcherState::Error(e) => Err(anyhow!("{}", e)),
        }
    }
}
