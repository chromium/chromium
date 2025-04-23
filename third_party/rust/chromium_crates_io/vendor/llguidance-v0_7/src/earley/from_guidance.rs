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
        } else if let Some(mut json_schema) = input.json_schema {
            let mut opts = JsonCompileOptions::default();
            if let Some(x_guidance) = json_schema.get("x-guidance") {
                opts = serde_json::from_value(x_guidance.clone())?;
                // TODO not removing it causes oneOf to be handled as anyOf in Github_medium---o61004.json
                json_schema.as_object_mut().unwrap().remove("x-guidance");
            }
            opts.json_to_llg(builder, json_schema)?
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
        let mut grammar = builder.grammar;
        let mut lexer_spec = builder.regex.spec;

        grammar.resolve_grammar_refs(&mut lexer_spec, &grammar_by_idx)?;

        Ok((grammar, lexer_spec))
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

    pub fn to_cgrammar(
        self,
        tok_env: Option<TokEnv>,
        logger: &mut Logger,
        limits: ParserLimits,
        extra_lexemes: Vec<String>,
    ) -> Result<Arc<CGrammar>> {
        let t0 = Instant::now();
        let (grammar, mut lexer_spec) = self.to_internal(tok_env, limits)?;
        lexer_spec.add_extra_lexemes(&extra_lexemes);
        compile_grammar(t0, grammar, lexer_spec, logger)
    }
}

fn compile_grammar(
    t0: Instant,
    mut grammar: Grammar,
    lexer_spec: LexerSpec,
    logger: &mut Logger,
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

    let grammars = Arc::new(grammar.compile(lexer_spec));

    loginfo!(
        logger,
        "build grammar: {:?}; optimize: {:?}",
        t1 - t0,
        t1.elapsed()
    );

    Ok(grammars)
}
