use std::{hint::black_box, panic::AssertUnwindSafe, sync::Arc, time::Duration};

use crate::{
    api::{GrammarInit, ParserLimits, StopReason},
    earley::{BiasComputer, Parser, ParserError, ParserStats},
    infoln, panic_utils, warn, Instant, Logger, ParserFactory,
};
use anyhow::{ensure, Result};
use toktrie::{InferenceCapabilities, SimpleVob, TokEnv, TokenId, INVALID_TOKEN};

#[derive(Clone)]
pub struct TokenParser {
    pub token_env: TokEnv,
    pub parser: Parser,
    pub compute_mask_start_time: Instant,
    pub last_bias_time: Duration,
    pub inference_caps: InferenceCapabilities,
    pub logger: Logger,
    pub limits: ParserLimits,
    pub bias_computer: Arc<dyn BiasComputer>,
    last_step_stats: ParserStats,
    max_step_stats: ParserStats,
    eos_token: TokenId,

    is_accepting_cache: Option<bool>,
    ff_tokens_cache: Option<(Vec<TokenId>, Vec<u8>)>,
    stop_reason: StopReason,
    error_message: Option<String>,
    max_tokens_total: usize,

    // tokens currently in KV cache
    llm_tokens: Vec<TokenId>,
    llm_bytes: Vec<u8>,

    grm_prefix: Vec<u8>,
    is_fresh: bool,
}

impl TokenParser {
    // use ParserFactory externally
    pub(crate) fn from_init(
        factory: &ParserFactory,
        grammar_init: GrammarInit,
        logger: Logger,
        inference_caps: InferenceCapabilities,
        limits: ParserLimits,
    ) -> Result<Self> {
        panic_utils::catch_unwind(AssertUnwindSafe(|| {
            Self::init_inner(factory, grammar_init, logger, inference_caps, limits)
        }))
    }

    fn init_inner(
        factory: &ParserFactory,
        grammar_init: GrammarInit,
        mut logger: Logger,
        inference_caps: InferenceCapabilities,
        limits: ParserLimits,
    ) -> Result<Self> {
        let token_env = factory.tok_env().clone();
        ensure!(
            token_env.tokenize_is_canonical() || !inference_caps.ff_tokens,
            "ff_tokens requires canonical tokenization"
        );
        ensure!(
            !inference_caps.backtrack || inference_caps.ff_tokens,
            "backtrack requires ff_tokens"
        );

        let compute_mask_start_time = Instant::now();
        let mut max_tokens = usize::MAX;
        if let GrammarInit::Serialized(input) = &grammar_init {
            if let Some(m) = input.max_tokens {
                max_tokens = m;
            }
        }
        let compiled_grammar = grammar_init.to_cgrammar(
            Some(token_env.clone()),
            &mut logger,
            limits.clone(),
            factory.extra_lexemes(),
        )?;
        let mut parser = Parser::new(
            token_env.clone(),
            compiled_grammar,
            limits.clone(),
            factory.perf_counters(),
        )?;
        parser.metrics_mut().rand = factory.next_rng();
        let eos_token = token_env.tok_trie().eos_token();

        Ok(TokenParser {
            bias_computer: factory.slicer().clone(),
            logger,
            token_env,
            inference_caps,
            limits,
            max_step_stats: ParserStats::default(),
            last_step_stats: ParserStats::default(),
            compute_mask_start_time,
            is_accepting_cache: None,
            ff_tokens_cache: None,
            stop_reason: StopReason::NotStopped,
            error_message: None,
            parser,
            eos_token,
            llm_tokens: Vec::new(),
            llm_bytes: Vec::new(),
            grm_prefix: Vec::new(),
            max_tokens_total: max_tokens,
            last_bias_time: Duration::from_secs(0),
            is_fresh: true,
        })
    }

    pub fn grammar_warnings(&mut self) -> Vec<String> {
        self.parser.grammar_warnings()
    }

    pub fn get_capture(&self, name: &str) -> Option<&[u8]> {
        self.parser.get_capture(name)
    }

    // regular .clone() uses a shared lexer state
    pub fn deep_clone(&self) -> Self {
        let mut copy = self.clone();
        copy.parser = self.parser.deep_clone();
        copy
    }

    pub fn stop_reason(&self) -> StopReason {
        self.stop_reason
    }

    pub fn is_fresh(&self) -> bool {
        self.is_fresh
    }

    pub fn parser_stats(&self) -> &ParserStats {
        self.parser.stats()
    }

    pub fn last_step_stats(&self) -> &ParserStats {
        &self.last_step_stats
    }

    pub fn max_step_stats(&self) -> &ParserStats {
        &self.max_step_stats
    }

    pub fn num_tokens(&self) -> usize {
        self.llm_tokens.len()
    }

    pub fn final_bytes(&self) -> &[u8] {
        &self.llm_bytes[self.grm_prefix.len()..]
    }

    pub fn is_accepting(&mut self) -> bool {
        if let Some(acc) = self.is_accepting_cache {
            acc
        } else {
            let r = !self.has_ff_bytes() && self.parser.is_accepting();
            self.is_accepting_cache = Some(r);
            r
        }
    }

    pub fn bytes_since(&self, mut idx: usize) -> &[u8] {
        idx += self.grm_prefix.len();
        let endp = std::cmp::min(self.llm_bytes.len(), self.parser.hidden_start());
        if idx >= self.llm_bytes.len() || idx >= endp {
            return &[];
        }
        &self.llm_bytes[idx..endp]
    }

    pub fn start_without_prompt(&mut self) {
        infoln!(
            self,
            "initial lexer cost: {} (no prompt)",
            self.parser.lexer_stats()
        );

        assert!(self.is_fresh);
        self.is_fresh = false;
    }

    fn tokenize_and_chop(
        &mut self,
        mut tokens: Vec<TokenId>,
        num_fixed: usize,
    ) -> (Vec<TokenId>, usize) {
        let trie = self.token_env.tok_trie();
        let (chop_tokens, chop_bytes) = self
            .parser
            .with_recognizer(|r| trie.chop_tokens(r, &tokens[num_fixed..]));
        infoln!(
            self,
            "tokenize -> {}; chop: {} tokens, {} bytes",
            trie.tokens_dbg(&tokens),
            chop_tokens,
            chop_bytes
        );
        // here we remove a suffix from tokens that could be possibly tokenized differently
        tokens.truncate(tokens.len() - chop_tokens);
        (tokens, chop_bytes)
    }

    pub fn process_prompt(&mut self, prompt: Vec<TokenId>) -> Vec<TokenId> {
        infoln!(self, "initial lexer cost: {}", self.parser.lexer_stats());

        assert!(self.token_env.tokenize_is_canonical());
        assert!(self.is_fresh);
        self.is_fresh = false;

        assert!(self.llm_tokens.is_empty());

        let trie = self.token_env.tok_trie();
        infoln!(self, "prompt: {}", trie.tokens_dbg(&prompt));
        let mut prompt_bytes = trie.decode_raw(&prompt);
        if self.can_force_bytes() {
            self.parser.force_bytes();
        }
        let grm_bytes = self.parser.get_bytes().to_vec();
        prompt_bytes.extend_from_slice(&grm_bytes);

        let (tokens, num_fixed) = self.token_env.tokenize_bytes_marker(&prompt_bytes);
        let (res_prompt, chop_bytes) = self.tokenize_and_chop(tokens, num_fixed);

        let trie = self.token_env.tok_trie();
        infoln!(
            self,
            "prompt+grm: {} {}",
            trie.tokens_dbg(&res_prompt),
            self.parser.grammar().lexer_spec().no_forcing
        );

        // if we moved a bunch of grammar to the prompt, update llm_tokens to reflect that
        if chop_bytes <= grm_bytes.len() {
            self.llm_bytes = grm_bytes[0..grm_bytes.len() - chop_bytes].to_vec();
            self.llm_tokens = self.token_env.tokenize_bytes_marker(&self.llm_bytes).0;
            self.parser.apply_forced(self.llm_bytes.len());
            let decoded = self.tok_trie().decode_raw(&self.llm_tokens);
            if !self.llm_bytes.is_empty()
                && !decoded.is_empty()
                && decoded[1..] == self.llm_bytes
                && decoded[0] == b' '
            {
                infoln!(self, "applying <s>space hack");
                self.grm_prefix = decoded[0..1].to_vec();
                self.llm_bytes = decoded;
            }
            infoln!(self, "ini_tokens: {}", trie.tokens_dbg(&self.llm_tokens));
        } else {
            // pretend the final bit of prompt was the prefix of the grammar
            self.grm_prefix = prompt_bytes
                [prompt_bytes.len() - chop_bytes..prompt_bytes.len() - grm_bytes.len()]
                .to_vec();
            infoln!(
                self,
                "force_prefix: {:?}",
                String::from_utf8_lossy(&self.grm_prefix)
            );
        }

        infoln!(self, "res_prompt: {}", trie.tokens_dbg(&res_prompt));
        res_prompt
    }

    fn clear_caches(&mut self) {
        self.is_accepting_cache = None;
        self.ff_tokens_cache = None;
    }

    fn stop(&mut self, warn: &str, reason: StopReason) -> anyhow::Error {
        if !warn.is_empty() {
            self.error_message = Some(warn.to_string());
            warn!(self, "{}; stopping", warn);
        }
        self.stop_reason = reason;
        self.anyhow_error()
    }

    fn tok_trie(&self) -> &toktrie::TokTrie {
        self.token_env.tok_trie()
    }

    pub fn error_message(&self) -> Option<String> {
        self.error_message.clone()
    }

    fn check_initialized(&self, lbl: &str) -> Result<()> {
        ensure!(!self.is_fresh, "process_prompt() not called in {}", lbl);
        ensure!(
            self.stop_reason == StopReason::NotStopped,
            "parser stopped in {}; {}",
            lbl,
            self.error_message()
                .unwrap_or("no error message".to_string())
        );
        Ok(())
    }

    pub fn validate_token(&mut self, token: TokenId) -> Result<bool> {
        self.check_initialized("validate_token")?;
        self.validate_tokens_raw(&[token]).map(|n| n > 0)
    }

    pub fn reset(&mut self) -> Result<()> {
        self.rollback(self.llm_tokens.len())
    }

    pub fn rollback(&mut self, n_tokens: usize) -> Result<()> {
        if n_tokens == 0 {
            return Ok(());
        }

        ensure!(
            n_tokens <= self.llm_tokens.len(),
            "rollback: {} > {}",
            n_tokens,
            self.llm_tokens.len()
        );

        if self.stop_reason.is_ok() {
            // if we're stopped in "normal" way (e.g. end of grammar reached),
            // pretend we're not stopped
            self.stop_reason = StopReason::NotStopped;
        }

        // this will fail in case we're in error state or not initialized
        self.check_initialized("rollback")?;

        let new_len = self.llm_tokens.len() - n_tokens;
        let mut bytes_to_drop = 0;
        for tok in &self.llm_tokens[new_len..] {
            if *tok == self.eos_token {
                // doesn't count; we hope it's last though...
                bytes_to_drop += 0;
            } else {
                bytes_to_drop += self.tok_trie().token_len(*tok);
            }
        }
        ensure!(
            bytes_to_drop <= self.llm_bytes.len(),
            "rollback bytes: {} > {}",
            bytes_to_drop,
            self.llm_bytes.len()
        );

        self.parser.rollback(bytes_to_drop)?;

        self.max_tokens_total = self.max_tokens_total.saturating_add(n_tokens);
        self.llm_tokens.truncate(new_len);
        self.llm_bytes
            .truncate(self.llm_bytes.len() - bytes_to_drop);
        self.clear_caches();

        Ok(())
    }

    /// Returns how many of the passed tokens can be accepted by the parser.
    /// It does not tokenize forced bytes, so will accept non-canonical tokenizations.
    /// If called with more than one token, it may ignore max_tokens constraints.
    pub fn validate_tokens_raw(&mut self, tokens: &[TokenId]) -> Result<usize> {
        self.check_initialized("validate_tokens_raw")?;

        if tokens.is_empty() {
            return Ok(0);
        }

        let n_vocab = self.tok_trie().vocab_size();
        for &t in tokens {
            if t as usize >= n_vocab {
                return Err(self.stop(
                    &format!("token id {} out of range", t),
                    StopReason::InternalError,
                ));
            }
        }

        let n_valid = self.parser.validate_tokens(tokens);
        Ok(n_valid)
    }

    fn anyhow_error(&self) -> anyhow::Error {
        anyhow::anyhow!(self
            .error_message
            .clone()
            .unwrap_or(self.stop_reason.to_string()))
    }

    // compute_mask() is a top-level method in this file.
    // compute_mask() is called by Constraint::compute_mask().
    pub fn compute_mask(&mut self) -> Result<SimpleVob> {
        self.compute_mask_start_time = Instant::now();
        let r = self.compute_mask_inner();
        self.parser
            .perf_counters()
            .compute_mask
            .record(self.compute_mask_start_time.elapsed());
        r
    }

    fn compute_mask_inner(&mut self) -> Result<SimpleVob> {
        self.check_initialized("compute_mask")?;

        infoln!(self, "compute_mask");

        let prefix = if self.can_force_bytes() {
            let (ff_tokens, token_prefix) = self
                .ff_tokens_cache
                .take()
                .unwrap_or_else(|| self.ff_tokens());
            if !ff_tokens.is_empty() {
                let t = ff_tokens[0];
                infoln!(self, "forcing ff_token by mask: {}", t);
                let mask = self.tok_trie().singleton_token_set(t);
                self.last_step_stats = ParserStats::default();
                return Ok(mask);
            } else {
                // no tokens, so we got all our bytes back
                token_prefix
            }
        } else {
            let mut trg = Vec::new();
            self.compute_ff_bytes_to(&mut trg);
            trg
        };

        let mut allowed_tokens = self.compute_bias(&prefix);

        if let Some(s) = self.parser.get_error() {
            return Err(self.stop_for_parser_error("", s));
        }

        if self.eos_token != INVALID_TOKEN && self.is_accepting() {
            allowed_tokens.allow_token(self.eos_token);
        }

        self.log_final(&prefix, &allowed_tokens);

        if allowed_tokens.is_zero() {
            infoln!(self, "no tokens allowed, stopping");
            return Err(self.stop("", StopReason::NoExtensionBias));
        }

        Ok(allowed_tokens)
    }

    fn stop_for_parser_error(&mut self, pref: &str, err: ParserError) -> anyhow::Error {
        self.stop(&format!("{}{}", pref, err.message()), err.stop_reason())
    }

    fn apply_token(&mut self, tok_id: TokenId) -> Result<usize> {
        self.clear_caches();

        let trie = self.token_env.tok_trie();

        if (tok_id as usize) >= trie.vocab_size() {
            return Err(self.stop(
                &format!("token id {} out of range", tok_id),
                StopReason::InternalError,
            ));
        }

        self.llm_tokens.push(tok_id);

        let tok_bytes = trie.decode_raw(&[tok_id]);

        // first, check we're still in grm_prefix
        let prefix_len = self.grm_prefix.len().saturating_sub(self.llm_bytes.len());

        infoln!(
            self,
            "consume_token: {} {} prefix={}",
            tok_id,
            trie.token_dbg(tok_id),
            prefix_len
        );

        let tok_bytes = if prefix_len > 0 {
            let to_apply = &tok_bytes[0..std::cmp::min(tok_bytes.len(), prefix_len)];
            self.llm_bytes.extend_from_slice(to_apply);

            if self.grm_prefix[0..self.llm_bytes.len()] != self.llm_bytes {
                return Err(self.stop(
                    &format!(
                        "prefix mismatch: applying {:?}; {:?} vs {:?}",
                        String::from_utf8_lossy(to_apply),
                        String::from_utf8_lossy(&self.grm_prefix),
                        String::from_utf8_lossy(&self.llm_bytes)
                    ),
                    StopReason::InternalError,
                ));
            }

            if prefix_len < tok_bytes.len() {
                &tok_bytes[prefix_len..]
            } else {
                // still completely in prefix, nothing more to apply
                return Ok(0);
            }
        } else {
            &tok_bytes
        };

        if let Some(err) = self.parser.get_error() {
            return Err(self.stop_for_parser_error("", err));
        }

        // now apply normally
        match self.parser.apply_token(tok_bytes, tok_id) {
            Err(e) => {
                return Err(self.stop(
                    &format!("Parser Error: {}", e),
                    StopReason::ParserTooComplex, // TODO - there are other reasons
                ));
            }
            Ok(backtrack_bytes0) => {
                self.llm_bytes.extend_from_slice(tok_bytes);

                if backtrack_bytes0 != 0 {
                    let mut backtrack_bytes: isize = backtrack_bytes0.try_into().unwrap();
                    let mut backtrack_tokens = 0;
                    while backtrack_bytes > 0 {
                        let tok_off = self.llm_tokens.len() - backtrack_tokens;
                        if tok_off == 0 {
                            break; // we can't backtrack any further
                        }
                        let tok = self.llm_tokens[tok_off - 1];
                        backtrack_bytes -= trie.token_len(tok) as isize;
                        backtrack_tokens += 1;
                    }
                    assert!(backtrack_tokens > 0);
                    let additional_backtrack_bytes: usize = (-backtrack_bytes).try_into().unwrap();
                    let full_backtrack_bytes = backtrack_bytes0 + additional_backtrack_bytes;

                    let byte_ptr = self.llm_bytes.len() - full_backtrack_bytes;
                    infoln!(
                        self,
                        "backtrack: {} tokens / {}+{} bytes (deletes: {:?})",
                        backtrack_tokens,
                        backtrack_bytes0,
                        additional_backtrack_bytes,
                        String::from_utf8_lossy(&self.llm_bytes[byte_ptr..])
                    );
                    self.llm_bytes.truncate(byte_ptr);

                    let token_ptr = self.llm_tokens.len() - backtrack_tokens;
                    if !self.inference_caps.backtrack {
                        warn!(
                            self,
                            "can't backtrack over {}; this may confuse the model",
                            trie.tokens_dbg(&self.llm_tokens[token_ptr..])
                        );
                        // pretend there's no backtrack
                        backtrack_tokens = 0;
                    } else {
                        // make sure the parser know we actually don't have
                        // the non-backtracked bytes of backtracked token
                        self.parser.additional_backtrack(additional_backtrack_bytes);
                    }
                    self.llm_tokens.truncate(token_ptr);
                    return Ok(backtrack_tokens);
                }
            }
        }

        Ok(0)
    }

    fn pending_grm_prefix(&self) -> &[u8] {
        &self.grm_prefix[std::cmp::min(self.grm_prefix.len(), self.llm_bytes.len())..]
    }

    fn has_ff_bytes(&self) -> bool {
        !self.pending_grm_prefix().is_empty() || !self.parser.currently_forced_bytes().is_empty()
    }

    fn can_force_bytes(&self) -> bool {
        !self.parser.grammar().lexer_spec().no_forcing && self.token_env.tokenize_is_canonical()
    }

    pub fn force_bytes(&mut self) -> Vec<u8> {
        self.parser.force_bytes();
        let mut trg = Vec::new();
        self.compute_ff_bytes_inner(&mut trg);
        trg
    }

    fn compute_ff_bytes_to(&mut self, trg: &mut Vec<u8>) {
        // PERF: in some cases, this may be long
        if self.can_force_bytes() {
            self.parser.force_bytes();
        }
        self.compute_ff_bytes_inner(trg);
    }

    fn compute_ff_bytes_inner(&mut self, trg: &mut Vec<u8>) {
        // handle grm_prefix we might have injected
        if self.llm_bytes.len() < self.grm_prefix.len() {
            let inject = &self.grm_prefix[self.llm_bytes.len()..];
            trg.extend_from_slice(inject);
            infoln!(
                self,
                "injecting prefix: {:?}",
                String::from_utf8_lossy(inject)
            );
        }

        trg.extend_from_slice(self.parser.currently_forced_bytes());
    }

    /// Converts forced bytes into tokens.
    /// Also returns any bytes that need to be prefix of the
    /// next sampled token (token healing).
    fn ff_tokens(&mut self) -> (Vec<TokenId>, Vec<u8>) {
        let mut forced_bytes = Vec::new();
        let mut existing_tokens = if self.llm_tokens.is_empty() {
            Vec::new()
        } else {
            let r = self.llm_tokens[self.llm_tokens.len() - 1..].to_vec();
            let trie = self.token_env.tok_trie();
            forced_bytes = trie.decode_raw(&r);
            r
        };
        let num_existing_bytes = forced_bytes.len();

        self.compute_ff_bytes_to(&mut forced_bytes);

        let mut token_prefix = Vec::new();

        let do_force =
            forced_bytes.len() > num_existing_bytes && self.token_env.tokenize_is_canonical();
        if do_force {
            let t0 = Instant::now();
            let (mut tokens, mut num_fixed) = self.token_env.tokenize_bytes_marker(&forced_bytes);
            if !tokens.starts_with(&existing_tokens) {
                // whoops, re-tokenize without the prefix
                let trie = self.token_env.tok_trie();
                infoln!(
                    self,
                    "re-tokenizing without prefix: {}; because we got {}",
                    trie.tokens_dbg(&existing_tokens),
                    trie.tokens_dbg(&tokens),
                );
                (tokens, num_fixed) = self
                    .token_env
                    .tokenize_bytes_marker(&forced_bytes[num_existing_bytes..]);
                infoln!(
                    self,
                    "re-tokenized: {} from: {:?}",
                    trie.tokens_dbg(&tokens),
                    &forced_bytes[num_existing_bytes..]
                );
                existing_tokens.clear();
            } else {
                num_fixed = std::cmp::max(existing_tokens.len(), num_fixed);
            }

            let (mut grm_tokens, chop_bytes) = self.tokenize_and_chop(tokens, num_fixed);
            assert!(grm_tokens.starts_with(&existing_tokens));
            grm_tokens.drain(..existing_tokens.len());

            let trie = self.token_env.tok_trie();
            infoln!(
                self,
                "forced: {} bytes:{:?} tokens:{:?}",
                trie.tokens_dbg(&grm_tokens),
                &forced_bytes[num_existing_bytes..],
                grm_tokens
            );
            token_prefix = forced_bytes[forced_bytes.len() - chop_bytes..].to_vec();

            self.parser.perf_counters().tokenize_ff.record(t0.elapsed());

            if !grm_tokens.is_empty() {
                infoln!(
                    self,
                    "fixed_tokens: {}; prefix len {}",
                    trie.tokens_dbg(&grm_tokens),
                    token_prefix.len()
                );
                return (grm_tokens, token_prefix);
            } else {
                infoln!(self, "no fixed tokens; prefix len {}", token_prefix.len());
            }
        } else if forced_bytes.len() > num_existing_bytes {
            infoln!(self, "not-forcing {} bytes", forced_bytes.len());
            token_prefix = forced_bytes[num_existing_bytes..].to_vec();
        }

        (Vec::new(), token_prefix)
    }

    fn compute_bias(&mut self, token_prefix: &[u8]) -> SimpleVob {
        let pre_stats = self.parser.stats().clone();
        let set = self.parser.compute_bias(&*self.bias_computer, token_prefix);
        let p_stats = self.parser.stats().delta(&pre_stats);
        self.last_bias_time = Duration::from_micros(p_stats.compute_time_us);
        self.last_step_stats = p_stats.clone();
        self.max_step_stats = self.max_step_stats.max(&p_stats);
        set
    }

    fn log_final(&mut self, token_prefix: &[u8], allowed_tokens: &SimpleVob) {
        infoln!(
            self,
            "step-stats: {}us; {} lex fuel; {} items; {}",
            self.compute_mask_start_time.elapsed().as_micros(),
            self.last_step_stats.lexer_cost,
            self.last_step_stats.all_items,
            self.parser.lexer_stats(),
        );

        infoln!(
            self,
            "bias: (pref: {:?}; accpt: {}; temp: {:.3}) {}",
            String::from_utf8_lossy(token_prefix),
            self.is_accepting_cache.unwrap(),
            self.parser.temperature().unwrap_or(0.0),
            self.token_env.tok_trie().token_set_dbg(allowed_tokens)
        );
    }

    pub fn temperature(&self) -> Option<f32> {
        self.parser.temperature()
    }

    /// Extend the current state of the parser with given token.
    /// Returns number of tokens to backtrack if any.
    pub fn consume_token(&mut self, token: TokenId) -> Result<usize> {
        self.check_initialized("consume_token")?;

        if self.max_tokens_total == 0 {
            return Err(self.stop("max_tokens_total reached", StopReason::MaxTokensTotal));
        }
        self.max_tokens_total -= 1;

        if token == self.eos_token {
            if self.parser.scan_eos() {
                // it got scanned correctly, so we remove it
                // this only happens for gen() terminated by EOS
                infoln!(self, "consume_token: scanned eos_token");
                // if self.inference_caps.backtrack {
                //     return Ok(1);
                // } else {
                //     warn!(self, "can't backtrack over eos_token");
                //     return Ok(0);
                // }
                // don't backtrack it for now, fails tests
                return Ok(0);
            } else {
                let accepting = self.is_accepting();
                infoln!(
                    self,
                    "consume_token: eos_token not eaten by parser; accept={}",
                    accepting
                );
                if accepting {
                    self.llm_tokens.push(token);
                    return Ok(0);
                }
            }
        }

        let apply_res = self.apply_token(token);
        self.parser.log_row_infos("post-apply");
        match apply_res {
            Err(_) => Err(self.anyhow_error()),
            Ok(n) => Ok(n),
        }
    }

    /// Check whether the current parser state forces the sequence to stop.
    /// If so, puts the parser in stop state and returns true.
    /// Otherwise, returns false.
    /// This generally should be called after consume_token().
    pub fn check_stop(&mut self) -> Result<bool> {
        let empty_token_prefix = !self.has_ff_bytes();
        let pending_eos = self.llm_tokens.last() == Some(&self.eos_token);
        let lexer_bytes = self.parser.has_pending_lexeme_bytes();
        let is_accepting = self.is_accepting();
        let can_advance = self.parser.can_advance();
        let parser_done = is_accepting && (!can_advance || pending_eos);
        infoln!(
            self,
            "parser_done: {parser_done}; lexer_bytes: {lexer_bytes}; \
                can_advance: {can_advance} (eos:{pending_eos}); \
                accept: {is_accepting}; \
                empty_token_prefix: {empty_token_prefix}"
        );
        assert!(!is_accepting || empty_token_prefix);

        if parser_done {
            infoln!(
                self,
                "only eos token allowed, stopping; accepting: {}",
                is_accepting
            );
            let reason = if pending_eos {
                StopReason::EndOfSentence
            } else {
                StopReason::NoExtension
            };
            self.stop("", reason);
            Ok(true)
        } else {
            Ok(false)
        }
    }

    /// Check if there are any tokens to fast-forward, forced by the current
    /// parser state.
    pub fn compute_ff_tokens(&mut self) -> Vec<TokenId> {
        let r = self.ff_tokens();
        if self.can_force_bytes() {
            self.ff_tokens_cache = Some(r.clone());
        }
        r.0
    }

    /// Compute and then consume fast-forward tokens.
    pub fn consume_ff_tokens(&mut self) -> Result<Vec<TokenId>> {
        let ff_tokens = self.compute_ff_tokens();
        for &t in &ff_tokens {
            let num_backtrack = self.consume_token(t)?;
            if num_backtrack > 0 {
                return Err(self.stop(
                    &format!("backtrack required after ff_token: {}", t),
                    StopReason::InternalError,
                ));
            }
        }
        Ok(ff_tokens)
    }

    /// This function documents typical use of this interface.
    /// The `tokens` array simulates tokens being sampled.
    #[allow(dead_code)]
    fn typical_use(&mut self, prompt: Vec<TokenId>) -> Result<()> {
        // First, check if we need to token-heal the prompt,
        // and if there are some tokens forced by the beginning
        // of the grammar.
        let new_prompt = self.process_prompt(prompt);

        // pass new prompt to inference engine
        black_box(new_prompt);

        let mut tokens = vec![];

        loop {
            let temp = self.temperature();
            let mask = self.compute_mask()?;

            // model forward pass in parallel with compute_mask() goes here

            // simulate sampling a token with given mask
            black_box((temp, mask));
            let sampled_token = 42;

            let num_backtrack = self.consume_token(sampled_token)?;

            if num_backtrack == 0 {
                // normal situation - the token was accepted
                tokens.push(sampled_token);
            } else {
                // this will only happen if you enable backtrack
                assert!(self.inference_caps.backtrack);
                if num_backtrack == 1 {
                    // don't add the token to the list
                } else if num_backtrack > 1 {
                    // backtrack
                    tokens.truncate(tokens.len() - num_backtrack - 1);
                }
            }

            // This is optional; if you don't check, compute_mask() will
            // return an error when it cannot continue anymore.
            // If you check here, you can distinguish between normal stop
            // and an error.
            if self.check_stop()? {
                break;
            }

            // This is optional - call if you have the ability to append
            // several tokens at once.
            let forced = self.consume_ff_tokens()?;
            tokens.extend_from_slice(&forced);
        }

        Ok(())
    }
}
