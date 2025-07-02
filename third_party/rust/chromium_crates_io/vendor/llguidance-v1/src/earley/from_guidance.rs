use std::fmt::Write;
use std::{sync::Arc, vec};

use super::grammar::SymIdx;
use super::lexerspec::LexerSpec;
use super::{CGrammar, Grammar};
use crate::api::{GrammarId, GrammarInit, GrammarWithLexer, ParserLimits, TopLevelGrammar};
use crate::earley::lexerspec::LexemeClass;
use crate::Instant;
use crate::{loginfo, JsonCompileOptions, Logger};
use crate::{GrammarBuilder, HashMap};
use anyhow::{bail, ensure, Result};
use toktrie::TokEnv;

struct CompileCtx {
    builder: Option<GrammarBuilder>,
    grammar_by_idx: HashMap<GrammarId, usize>,
    grammar_roots: Vec<(SymIdx, LexemeClass)>,
}

impl CompileCtx {
    fn run_one(&mut self, input: GrammarWithLexer) -> Result<(SymIdx, LexemeClass)> {
        let builder = std::mem::take(&mut self.builder).unwrap();

        let res = if let Some(lark) = input.lark_grammar {
            #[cfg(feature = "lark")]
            {
                use crate::lark::lark_to_llguidance;
                ensure!(
                    input.json_schema.is_none(),
                    "cannot have both lark_grammar and json_schema"
                );
                lark_to_llguidance(builder, &lark)?
            }
            #[cfg(not(feature = "lark"))]
            {
                let _ = lark;
                bail!("lark_grammar is not supported in this build")
            }
        } else if let Some(json_schema) = input.json_schema {
            JsonCompileOptions::default().json_to_llg_with_overrides(builder, json_schema)?
        } else {
            bail!("grammar must have either lark_grammar or json_schema");
        };

        res.builder.check_limits()?;

        let grammar_id = res.builder.grammar.sym_props(res.start_node).grammar_id;

        // restore builder
        self.builder = Some(res.builder);

        Ok((res.start_node, grammar_id))
    }

    fn run(mut self, input: TopLevelGrammar) -> Result<(Grammar, LexerSpec)> {
        for (idx, grm) in input.grammars.iter().enumerate() {
            if grm.lark_grammar.is_none() && grm.json_schema.is_none() {
                bail!("grammar must have either lark_grammar or json_schema");
            }
            if let Some(n) = &grm.name {
                let n = GrammarId::Name(n.to_string());
                if self.grammar_by_idx.contains_key(&n) {
                    bail!("duplicate grammar name: {}", n);
                }
                self.grammar_by_idx.insert(n, idx);
            }
        }

        for (idx, grm) in input.grammars.into_iter().enumerate() {
            let v = self.run_one(grm)?;
            self.grammar_roots[idx] = v;
        }

        let grammar_by_idx: HashMap<GrammarId, (SymIdx, LexemeClass)> = self
            .grammar_by_idx
            .into_iter()
            .map(|(k, v)| (k, self.grammar_roots[v]))
            .collect();

        let builder = self.builder.unwrap();
        let warnings = builder.get_warnings();
        let mut grammar = builder.grammar;
        let mut lexer_spec = builder.regex.spec;

        grammar.resolve_grammar_refs(&mut lexer_spec, &grammar_by_idx)?;

        assert!(lexer_spec.grammar_warnings.is_empty());
        lexer_spec.grammar_warnings = warnings;

        Ok((grammar, lexer_spec))
    }
}

#[derive(Debug, Clone)]
pub enum ValidationResult {
    Valid,
    Warnings(Vec<String>),
    Error(String),
}

impl ValidationResult {
    pub fn from_warning(w: Vec<String>) -> Self {
        if w.is_empty() {
            ValidationResult::Valid
        } else {
            ValidationResult::Warnings(w)
        }
    }

    pub fn into_tuple(self) -> (bool, Vec<String>) {
        match self {
            ValidationResult::Valid => (false, vec![]),
            ValidationResult::Warnings(w) => (false, w),
            ValidationResult::Error(e) => (true, vec![e]),
        }
    }

    pub fn into_error(self) -> Option<String> {
        match self {
            ValidationResult::Valid => None,
            ValidationResult::Warnings(_) => None,
            ValidationResult::Error(e) => Some(e),
        }
    }

    pub fn render(&self, with_warnings: bool) -> String {
        match self {
            ValidationResult::Valid => String::new(),
            ValidationResult::Warnings(w) => {
                if with_warnings {
                    w.iter()
                        .map(|w| format!("WARNING: {}", w))
                        .collect::<Vec<_>>()
                        .join("\n")
                } else {
                    String::new()
                }
            }
            ValidationResult::Error(e) => format!("ERROR: {}", e),
        }
    }
}

impl GrammarInit {
    pub fn to_internal(
        self,
        tok_env: Option<TokEnv>,
        limits: ParserLimits,
    ) -> Result<(Grammar, LexerSpec)> {
        match self {
            GrammarInit::Internal(g, l) => Ok((g, l)),

            GrammarInit::Serialized(input) => {
                ensure!(!input.grammars.is_empty(), "empty grammars array");

                let builder = GrammarBuilder::new(tok_env, limits.clone());

                let ctx = CompileCtx {
                    builder: Some(builder),
                    grammar_by_idx: HashMap::default(),
                    grammar_roots: vec![(SymIdx::BOGUS, LexemeClass::ROOT); input.grammars.len()],
                };

                ctx.run(input)
            }
        }
    }

    pub fn validate(self, tok_env: Option<TokEnv>, limits: ParserLimits) -> ValidationResult {
        match self.to_internal(tok_env, limits) {
            Ok((_, lex_spec)) => ValidationResult::from_warning(lex_spec.render_warnings()),
            Err(e) => ValidationResult::Error(e.to_string()),
        }
    }

    pub fn to_cgrammar(
        self,
        tok_env: Option<TokEnv>,
        logger: &mut Logger,
        limits: ParserLimits,
        extra_lexemes: Vec<String>,
    ) -> Result<Arc<CGrammar>> {
        let t0 = Instant::now();
        let (grammar, mut lexer_spec) = self.to_internal(tok_env, limits.clone())?;
        lexer_spec.add_extra_lexemes(&extra_lexemes);
        compile_grammar(t0, grammar, lexer_spec, logger, &limits)
    }
}

fn compile_grammar(
    t0: Instant,
    mut grammar: Grammar,
    lexer_spec: LexerSpec,
    logger: &mut Logger,
    limits: &ParserLimits,
) -> Result<Arc<CGrammar>> {
    let log_grammar = logger.level_enabled(3) || (logger.level_enabled(2) && grammar.is_small());
    if log_grammar {
        writeln!(
            logger.info_logger(),
            "{:?}\n{}\n",
            lexer_spec,
            grammar.to_string(Some(&lexer_spec))
        )
        .unwrap();
    } else if logger.level_enabled(2) {
        writeln!(
            logger.info_logger(),
            "Grammar: (skipping body; log_level=3 will print it); {}",
            grammar.stats()
        )
        .unwrap();
    }

    let t1 = Instant::now();
    grammar = grammar.optimize();

    if log_grammar {
        write!(
            logger.info_logger(),
            "  == Optimize ==>\n{}",
            grammar.to_string(Some(&lexer_spec))
        )
        .unwrap();
    } else if logger.level_enabled(2) {
        writeln!(logger.info_logger(), "  ==> {}", grammar.stats()).unwrap();
    }

    let grammars = Arc::new(grammar.compile(lexer_spec, limits)?);

    loginfo!(
        logger,
        "build grammar: {:?}; optimize: {:?}",
        t1 - t0,
        t1.elapsed()
    );

    Ok(grammars)
}
