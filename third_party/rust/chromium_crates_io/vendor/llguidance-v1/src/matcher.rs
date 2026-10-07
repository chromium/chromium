use anyhow::{anyhow, bail, ensure, Result};
use toktrie::{SimpleVob, TokEnv, TokenId};

use crate::{
    api::StopReason, earley::ParserStats, panic_utils, CancellationHandle, Cancelled, TokenParser,
};

struct MatcherInner {
    parser: TokenParser,
}

#[allow(clippy::large_enum_variant)]
enum MatcherState {
    Normal(MatcherInner),
    Error(String),
    Cancelled,
}

/// This is meant to be used in server-side scenarios.
/// The Constraint interface is more for usage in Python Guidance.
pub struct Matcher(MatcherState, Option<CancellationHandle>);

impl Clone for Matcher {
    fn clone(&self) -> Self {
        self.clone_inner(false)
    }
}

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
                    let cancellation = parser.cancellation_handle();
                    Matcher(MatcherState::Normal(MatcherInner { parser }), cancellation)
                }
            }
            Err(e) => Matcher(MatcherState::Error(e.to_string()), None),
        }
    }

    /// Enable cancellation for this matcher.
    pub fn into_cancellable(mut self) -> Self {
        self.enable_cancellation();
        self
    }

    pub(crate) fn enable_cancellation(&mut self) {
        if self.1.is_some() {
            return;
        }
        let cancellation = match &mut self.0 {
            MatcherState::Normal(inner) => inner.parser.enable_cancellation(),
            _ => CancellationHandle::default(),
        };
        self.1 = Some(cancellation);
    }

    /// Return a handle when cancellation is enabled. Cloned handles share its cancellation state.
    ///
    /// Matcher clones sample the request when cloning starts. Each usable clone gets
    /// independent cancellation state. A clone that observes cancellation stays cancelled.
    pub fn cancellation_handle(&self) -> Option<CancellationHandle> {
        self.1.clone()
    }

    /// An existing error keeps its original cause after a later cancellation request.
    pub fn is_cancelled(&self) -> bool {
        !matches!(self.0, MatcherState::Error(_))
            && self
                .1
                .as_ref()
                .is_some_and(CancellationHandle::is_cancelled)
    }

    fn with_inner<T>(&mut self, f: impl FnOnce(&mut MatcherInner) -> Result<T>) -> Result<T> {
        match &mut self.0 {
            MatcherState::Normal(inner) => {
                let r = self
                    .1
                    .as_ref()
                    .map_or(Ok(()), CancellationHandle::check)
                    .map_err(anyhow::Error::from)
                    .and_then(|()| {
                        panic_utils::catch_unwind(std::panic::AssertUnwindSafe(|| f(inner)))
                    });
                #[cfg(test)]
                crate::cancellation::checkpoint("publication");
                if self
                    .1
                    .as_ref()
                    .is_some_and(CancellationHandle::is_cancelled)
                    || r.as_ref().is_err_and(|e| e.is::<Cancelled>())
                {
                    self.0 = MatcherState::Cancelled;
                    return Err(Cancelled.into());
                }
                match r {
                    Ok(r) => Ok(r),
                    Err(e) => {
                        let msg = inner.parser.augment_err(e);
                        self.0 = MatcherState::Error(msg.clone());
                        bail!(msg);
                    }
                }
            }
            MatcherState::Error(e) => Err(anyhow!("{}", e)),
            MatcherState::Cancelled => Err(Cancelled.into()),
        }
    }

    // Sampling the signal defines the clone's independence from later requests.
    fn clone_inner(&self, deep: bool) -> Self {
        let cancellation = self.1.as_ref().map(CancellationHandle::snapshot);
        let state = match &self.0 {
            MatcherState::Error(e) => MatcherState::Error(e.clone()),
            MatcherState::Cancelled => MatcherState::Cancelled,
            MatcherState::Normal(_)
                if cancellation
                    .as_ref()
                    .is_some_and(CancellationHandle::is_cancelled) =>
            {
                MatcherState::Cancelled
            }
            MatcherState::Normal(inner) => {
                let mut parser = if deep {
                    inner.parser.deep_clone()
                } else {
                    inner.parser.clone()
                };
                if let Some(cancellation) = cancellation.as_ref() {
                    parser.parser.set_cancellation_handle(cancellation.clone());
                }
                MatcherState::Normal(MatcherInner { parser })
            }
        };
        Self(state, cancellation)
    }

    /// Clone the parser and its lexer caches for independent execution.
    ///
    /// This samples cancellation when cloning starts, as [`Clone::clone`] does.
    /// Later requests do not affect the clone. An observed cancellation remains terminal.
    pub fn deep_clone(&self) -> Self {
        self.clone_inner(true)
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

    pub fn test_trigger_lexer_error(&mut self) -> Result<()> {
        self.with_inner(|inner| inner.parser.parser.test_trigger_lexer_error())
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
                Ok(inner.parser.token_env.tok_trie().eos_token_set())
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
        if self.is_cancelled() {
            return true;
        }
        match &self.0 {
            MatcherState::Normal(inner) => inner.parser.stop_reason() != StopReason::NotStopped,
            MatcherState::Error(_) | MatcherState::Cancelled => true,
        }
    }

    pub fn stop_reason(&self) -> StopReason {
        if self.is_cancelled() {
            return StopReason::Cancelled;
        }
        match &self.0 {
            MatcherState::Normal(inner) => inner.parser.stop_reason(),
            MatcherState::Error(_) => StopReason::InternalError,
            MatcherState::Cancelled => StopReason::Cancelled,
        }
    }

    /// Return forced tokens, or an empty list for non-canonical tokenizers or failed operations.
    /// Use `is_error`, `is_cancelled`, `get_error`, or `stop_reason` to inspect failures.
    pub fn compute_ff_tokens(&mut self) -> Vec<TokenId> {
        self.with_inner(|inner| Ok(inner.parser.compute_ff_tokens()))
            .unwrap_or_default()
    }

    pub fn consume_ff_tokens(&mut self) -> Vec<TokenId> {
        let toks = self.compute_ff_tokens();
        if !toks.is_empty() && self.consume_tokens(&toks).is_err() {
            return Vec::new();
        }
        toks
    }

    /// Return bytes forced by the current parser state, or an empty list if the operation fails.
    /// This also works for non-canonical tokenizers.
    /// Use `is_error`, `is_cancelled`, `get_error`, or `stop_reason` to inspect failures.
    pub fn compute_ff_bytes(&mut self) -> Vec<u8> {
        self.with_inner(|inner| Ok(inner.parser.force_bytes()))
            .unwrap_or_default()
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
                let _ = inner.parser.check_stop()?;
                ensure!(bt == 0, "unexpected backtracking");
            }
            Ok(tokens.len())
        })
    }

    pub fn validate_tokens(&mut self, tokens: &[TokenId]) -> Result<usize> {
        self.with_inner(|inner| inner.parser.validate_tokens_raw(tokens))
    }

    pub fn is_error(&self) -> bool {
        self.is_cancelled() || matches!(self.0, MatcherState::Error(_) | MatcherState::Cancelled)
    }

    pub fn get_error(&self) -> Option<String> {
        if self.is_cancelled() {
            return Some(Cancelled.to_string());
        }
        match &self.0 {
            MatcherState::Normal(_) => None,
            MatcherState::Error(e) => Some(e.clone()),
            MatcherState::Cancelled => Some(Cancelled.to_string()),
        }
    }

    pub fn grammar_warnings(&mut self) -> Vec<String> {
        match &mut self.0 {
            MatcherState::Normal(inner) => inner.parser.grammar_warnings(),
            MatcherState::Error(_) | MatcherState::Cancelled => vec![],
        }
    }

    pub fn tok_env(&self) -> Result<TokEnv> {
        match &self.0 {
            MatcherState::Normal(inner) => Ok(inner.parser.token_env.clone()),
            MatcherState::Error(e) => Err(anyhow!("{}", e)),
            MatcherState::Cancelled => Err(Cancelled.into()),
        }
    }

    pub fn last_step_stats(&self) -> Result<&ParserStats> {
        match &self.0 {
            MatcherState::Normal(inner) => Ok(inner.parser.last_step_stats()),
            MatcherState::Error(e) => Err(anyhow!("{}", e)),
            MatcherState::Cancelled => Err(Cancelled.into()),
        }
    }

    pub fn invalidate_bias_cache(&mut self) {
        if let MatcherState::Normal(inner) = &mut self.0 {
            inner.parser.invalidate_bias_cache();
        }
    }

    pub fn get_capture(&self, name: &str) -> Option<&[u8]> {
        match &self.0 {
            MatcherState::Normal(inner) => inner.parser.get_capture(name),
            MatcherState::Error(_) | MatcherState::Cancelled => None,
        }
    }

    pub fn captures(&self) -> &[(String, Vec<u8>)] {
        match &self.0 {
            MatcherState::Normal(inner) => inner.parser.captures(),
            MatcherState::Error(_) | MatcherState::Cancelled => &[],
        }
    }
}
