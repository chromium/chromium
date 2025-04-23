use anyhow::{bail, ensure, Result};
use toktrie::{StepResult, TokenId};

use crate::{
    api::StopReason,
    loginfo,
    output::{ParserOutput, Reporter},
    panic_utils, TokenParser,
};

#[derive(Clone)]
pub struct Constraint {
    pub parser: TokenParser,
    pub log_json_progress: bool,
    pub temperature: f32,
    reporter: Reporter,
    last_res: StepResult,
    started: bool,
    pending_stop: bool,
}

#[derive(Debug, Clone, Default)]
pub struct CommitResult {
    pub stop: bool,
    pub backtrack: u32,
    pub ff_tokens: Vec<TokenId>,
}

impl CommitResult {
    pub fn stop() -> Self {
        Self {
            stop: true,
            backtrack: 0,
            ff_tokens: vec![],
        }
    }

    pub fn from_step_result(res: &StepResult) -> Self {
        let mut r = CommitResult {
            stop: res.is_stop(),
            backtrack: 0,
            ff_tokens: vec![],
        };
        if let Some(s) = res.unconditional_splice() {
            r.backtrack = s.backtrack;
            r.ff_tokens = s.ff_tokens.clone();
        }
        r
    }
}

impl Constraint {
    /// Construct a state machine for a sequence constraint.
    pub fn new(parser: TokenParser) -> Self {
        assert!(parser.is_fresh(), "Parser was already used");
        Self {
            parser,
            reporter: Reporter::default(),
            last_res: StepResult::noop(),
            started: false,
            log_json_progress: false,
            temperature: 0.0,
            pending_stop: false,
        }
    }

    pub fn deep_clone(&self) -> Self {
        let mut copy = self.clone();
        copy.parser = self.parser.deep_clone();
        copy
    }

    fn save_progress_and_result(&mut self, res: StepResult) {
        self.last_res = res;
        if self.log_json_progress {
            for p in self.reporter.get_progress(&self.parser, &self.last_res) {
                self.parser.logger.write_buffer("JSON-OUT: ");
                self.parser
                    .logger
                    .write_buffer(&serde_json::to_string(&p).unwrap());
                self.parser.logger.write_buffer("\n");
            }
        }
        self.save_temperature();
    }

    fn save_temperature(&mut self) {
        if let Some(temp) = self.parser.parser.temperature() {
            self.temperature = temp;
        }
    }

    /// You can call this first with the prompt from the user, when not
    /// running in chat mode.
    /// This will return a new prompt, possibly with some tokens added as per
    /// the grammar (and possibly with some tokens removed, for token healing).
    pub fn process_prompt(&mut self, prompt: Vec<TokenId>) -> Vec<TokenId> {
        assert!(!self.started);
        self.started = true;
        let r = if self.parser.token_env.tokenize_is_canonical() {
            self.parser.process_prompt(prompt)
        } else {
            self.parser.start_without_prompt();
            prompt
        };
        self.save_temperature();
        r
    }

    pub fn start_without_prompt(&mut self) {
        assert!(!self.started);
        self.started = true;
        self.parser.start_without_prompt();
        self.save_temperature();
    }

    /// This can be called before the first compute_mask() to walk forward the
    /// parser with tokens generated in some previous run.
    pub fn force_tokens(&mut self, tokens: &[TokenId]) -> Result<()> {
        for &t in tokens {
            self.parser.consume_token(t)?;
        }
        Ok(())
    }

    pub fn has_pending_stop(&self) -> bool {
        self.pending_stop
    }

    /// This computes token sampling mask.
    /// It typically takes up to a millisecond for a 100k tokenizer.
    /// It will return an error when the order of calls is violated.
    /// The result will be either:
    ///     - a mask of allowed tokens to sample, or
    ///     - an unconditional splice result, indicating that the parser wants to append tokens, or
    ///     - a stop result, indicating that the parser is done
    /// The splice is never returned when ff_tokens are disabled in InferenceCapabilities.
    /// After this returns, commit_token() must be called with the sampled token if any.
    pub fn compute_mask(&mut self) -> Result<&StepResult> {
        panic_utils::catch_unwind(std::panic::AssertUnwindSafe(|| self.compute_mask_inner()))
            .map(|_| &self.last_res)
    }

    fn compute_mask_inner(&mut self) -> Result<()> {
        loginfo!(self.parser.logger, "\ncompute_mask()");

        if !self.started {
            self.started = true;
            self.parser.start_without_prompt();
            self.save_temperature();
        }

        ensure!(!self.last_res.is_stop(), "compute_mask() called after stop");

        if self.parser.check_stop()? {
            self.pending_stop = true;
            self.save_progress_and_result(StepResult::stop());
        } else {
            let mask = self.parser.compute_mask();
            if mask.is_err() && self.parser.stop_reason() == StopReason::NoExtensionBias {
                self.save_progress_and_result(StepResult::stop());
            } else {
                self.save_progress_and_result(StepResult::sample(mask?, self.parser.temperature()));
            }
        }

        Ok(())
    }

    pub fn step_result(&self) -> &StepResult {
        &self.last_res
    }

    fn res_commit_result(&mut self) -> Result<CommitResult> {
        Ok(CommitResult::from_step_result(&self.last_res))
    }

    pub fn validate_tokens_raw(&mut self, tokens: &[TokenId]) -> Result<usize> {
        if self.last_res.unconditional_splice().is_some() {
            self.save_progress_and_result(StepResult::sample(
                self.tok_trie().alloc_token_set(),
                self.parser.temperature(),
            ));
        }
        self.parser.validate_tokens_raw(tokens)
    }

    /// commit_token() is a top-level method in this file and is called by
    /// the LLInterpreter::commit_token().
    ///
    /// commit_token() commits the sampled token (if any), and sees if this forces any more tokens
    /// on the output (if ff_tokens are enabled in InferenceCapabilities).
    ///
    /// It only returns 'STOP' if previous compute_mask() already returned 'STOP'
    /// (in which case there's little point calling commit_token()).
    pub fn commit_token(&mut self, sampled_token: Option<TokenId>) -> Result<CommitResult> {
        panic_utils::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.commit_token_inner(sampled_token)
        }))
    }

    fn commit_token_inner(&mut self, sampled_token: Option<TokenId>) -> Result<CommitResult> {
        let n_tokens = self.parser.num_tokens();
        loginfo!(
            self.parser.logger,
            "\ncommit_token({}) at #{}",
            sampled_token
                .map(|t| self.parser.token_env.tok_trie().token_dbg(t))
                .unwrap_or("None".to_string()),
            n_tokens
        );

        // ensure!(
        //     self.step_arg.is_none(),
        //     "commit_token() called twice or without compute_mask()"
        // );

        // if last result was to stop or to unconditionally splice, we're done already
        if self.last_res.is_stop() {
            return self.res_commit_result();
        }

        // dead code?
        if self.last_res.unconditional_splice().is_some() {
            // if ff_tokens are not supported, there should always be sampling mask
            // and not an unconditional splice
            assert!(self.parser.inference_caps.ff_tokens);
            return self.res_commit_result();
        }

        if self.last_res.sample_mask.is_some() {
            let t = sampled_token.ok_or_else(|| {
                anyhow::anyhow!("sampled_token is required when mask was present")
            })?;

            let mut bt = self.parser.consume_token(t)?;
            let mut tokens = vec![t];
            if bt > 0 {
                loginfo!(self.parser.logger, "backtrack sampled");
                tokens.clear();
                bt -= 1;
            }
            if self.parser.inference_caps.ff_tokens {
                tokens.extend(self.parser.consume_ff_tokens()?);
            }

            if self.parser.check_stop()? {
                loginfo!(self.parser.logger, "set pending stop");
                self.pending_stop = true;
            }

            // save any logs
            self.save_progress_and_result(StepResult::splice(bt as u32, tokens));
            self.res_commit_result()
        } else {
            bail!("internal error: invalid compute_mask() result");
        }
    }

    /// This returns parser outputs to be passed back to the user.
    /// You can use that for structured output, or set log_json_progress to true
    /// and then use flush_logs() to get a string, from which the user
    /// can extract the JSON of the outputs.
    pub fn flush_progress(&mut self) -> Vec<ParserOutput> {
        self.reporter.get_progress(&self.parser, &self.last_res)
    }

    /// Logs to be sent to the user.
    pub fn flush_logs(&mut self) -> String {
        self.parser.logger.get_and_clear_logs()
    }

    // Utility functions

    pub fn tok_trie(&self) -> &toktrie::TokTrie {
        self.parser.token_env.tok_trie()
    }
}
