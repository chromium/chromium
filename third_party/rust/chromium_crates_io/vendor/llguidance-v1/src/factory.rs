use std::sync::{Arc, Mutex};

use anyhow::Result;
use toktrie::{InferenceCapabilities, TokEnv};

use crate::{
    api::{GrammarInit, ParserLimits, TopLevelGrammar},
    earley::{perf::ParserPerfCounters, SlicedBiasComputer, XorShift},
    Logger, TokenParser,
};

pub struct ParserFactory {
    tok_env: TokEnv,
    slicer: Arc<SlicedBiasComputer>,
    inference_caps: InferenceCapabilities,
    stderr_log_level: u32,
    buffer_log_level: u32,
    limits: ParserLimits,
    seed: Mutex<XorShift>,
    perf_counters: Arc<ParserPerfCounters>,
}

impl ParserFactory {
    pub fn new(
        tok_env: &TokEnv,
        inference_caps: InferenceCapabilities,
        regexes: &[String],
    ) -> Result<Self> {
        let slicer = Arc::new(SlicedBiasComputer::new(tok_env, regexes)?);
        Ok(ParserFactory {
            tok_env: tok_env.clone(),
            slicer,
            inference_caps,
            stderr_log_level: 1,
            buffer_log_level: 0,
            seed: Mutex::new(XorShift::default()),
            limits: ParserLimits::default(),
            perf_counters: Arc::new(ParserPerfCounters::default()),
        })
    }

    pub fn perf_counters(&self) -> Arc<ParserPerfCounters> {
        self.perf_counters.clone()
    }

    pub fn new_simple(tok_env: &TokEnv) -> Result<Self> {
        Self::new(
            tok_env,
            InferenceCapabilities::default(),
            &SlicedBiasComputer::general_slices(),
        )
    }

    pub fn with_slices(&self, slices: &[String]) -> Result<Self> {
        let slicer = Arc::new(SlicedBiasComputer::new(&self.tok_env, slices)?);
        Ok(ParserFactory {
            tok_env: self.tok_env.clone(),
            slicer,
            inference_caps: self.inference_caps.clone(),
            stderr_log_level: self.stderr_log_level,
            buffer_log_level: self.buffer_log_level,
            seed: Mutex::new(XorShift::default()),
            limits: self.limits.clone(),
            perf_counters: self.perf_counters.clone(),
        })
    }

    pub fn limits_mut(&mut self) -> &mut ParserLimits {
        &mut self.limits
    }

    pub fn limits(&self) -> &ParserLimits {
        &self.limits
    }

    pub fn tok_env(&self) -> &TokEnv {
        &self.tok_env
    }

    pub fn quiet(&mut self) -> &mut Self {
        self.stderr_log_level = 0;
        self.buffer_log_level = 0;
        self
    }

    pub fn buffer_log_level(&self) -> u32 {
        self.buffer_log_level
    }

    pub fn stderr_log_level(&self) -> u32 {
        self.stderr_log_level
    }

    pub fn set_buffer_log_level(&mut self, level: u32) -> &mut Self {
        self.buffer_log_level = level;
        self
    }

    pub fn set_stderr_log_level(&mut self, level: u32) -> &mut Self {
        self.stderr_log_level = level;
        self
    }

    pub fn extra_lexemes(&self) -> Vec<String> {
        self.slicer.extra_lexemes()
    }

    pub fn slicer(&self) -> Arc<SlicedBiasComputer> {
        self.slicer.clone()
    }

    pub(crate) fn next_rng(&self) -> XorShift {
        let mut rng = self.seed.lock().unwrap();
        rng.next_alt();
        rng.clone()
    }

    pub fn create_parser(&self, grammar: TopLevelGrammar) -> Result<TokenParser> {
        self.create_parser_from_init_default(GrammarInit::Serialized(grammar))
    }

    pub fn create_parser_from_init_default(&self, init: GrammarInit) -> Result<TokenParser> {
        self.create_parser_from_init(init, self.buffer_log_level, self.stderr_log_level)
    }

    pub fn create_parser_from_init_ext(
        &self,
        grammar_init: GrammarInit,
        logger: Logger,
        inference_caps: InferenceCapabilities,
        limits: ParserLimits,
    ) -> Result<TokenParser> {
        TokenParser::from_init(self, grammar_init, logger, inference_caps, limits)
    }

    pub fn create_parser_from_init(
        &self,
        init: GrammarInit,
        buffer_log_level: u32,
        stderr_log_level: u32,
    ) -> Result<TokenParser> {
        self.create_parser_from_init_ext(
            init,
            Logger::new(buffer_log_level, stderr_log_level),
            self.inference_caps.clone(),
            self.limits.clone(),
        )
    }
}
