use std::fmt::{Debug, Display};

use anyhow::{bail, Result};
use derivre::RegexAst;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

use crate::{
    earley::{lexerspec::LexerSpec, Grammar},
    regex_to_lark,
};

/// This represents a collection of grammars, with a designated
/// "start" grammar at first position.
/// Grammars can refer to each other via GenGrammar nodes.
#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct TopLevelGrammar {
    pub grammars: Vec<GrammarWithLexer>,
    pub max_tokens: Option<usize>,
}

#[allow(clippy::large_enum_variant)]
#[derive(Clone)]
pub enum GrammarInit {
    Serialized(TopLevelGrammar),
    Internal(Grammar, LexerSpec),
}

/// cbindgen:ignore
pub const DEFAULT_CONTEXTUAL: bool = true;

/// In lark syntax, this can be specified as JSON object after '%llguidance' declaration in the grammar.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct LLGuidanceOptions {
    /// Normally, when a sequence of bytes is forced by grammar, it is tokenized
    /// canonically and forced as tokens.
    /// With `no_forcing`, we let the model decide on tokenization.
    /// This generally reduces both quality and speed, so should not be used
    /// outside of testing.
    #[serde(default)]
    pub no_forcing: bool,

    /// If set, the grammar will allow invalid utf8 byte sequences.
    /// Any Unicode regex will cause an error.
    /// This is very unlikely what you need.
    #[serde(default)]
    pub allow_invalid_utf8: bool,
}

impl LLGuidanceOptions {
    pub fn apply(&mut self, other: &LLGuidanceOptions) {
        if other.no_forcing {
            self.no_forcing = true;
        }
        if other.allow_invalid_utf8 {
            self.allow_invalid_utf8 = true;
        }
    }
}

#[derive(Serialize, Deserialize, Clone, Default)]
pub struct GrammarWithLexer {
    /// The name of this grammar, can be used in GenGrammar nodes.
    pub name: Option<String>,

    /// The JSON schema that the grammar should generate.
    /// When this is set, nodes and rx_nodes must be empty.
    pub json_schema: Option<Value>,

    /// The Lark grammar that the grammar should generate.
    /// When this is set, nodes and rx_nodes must be empty.
    pub lark_grammar: Option<String>,
    // #[serde(flatten)]
    // pub options: LLGuidanceOptions,
}

impl Debug for GrammarWithLexer {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "GrammarWithLexer [{}]",
            if self.lark_grammar.is_some() {
                "lark"
            } else {
                "json"
            }
        )
    }
}

// /// If false, all other lexemes are excluded when this lexeme is recognized.
// /// This is normal behavior for keywords in programming languages.
// /// Set to true for eg. a JSON schema with both `/"type"/` and `/"[^"]*"/` as lexemes,
// /// or for "get"/"set" contextual keywords in C#.
// /// Default value set in GrammarWithLexer.
// contextual: Option<bool>,

// /// It lists the allowed escape sequences, typically one of:
// /// "nrbtf\\\"u" - to allow all JSON escapes, including \u00XX for control characters
// ///     this is the default
// /// "nrbtf\\\"" - to disallow \u00XX control characters
// /// "nrt\\\"" - to also disallow unusual escapes (\f and \b)
// /// "" - to disallow all escapes
// /// Note that \uXXXX for non-control characters (code points above U+001F) are never allowed,
// /// as they never have to be quoted in JSON.
// json_allowed_escapes: Option<String>,

/// Optional fields allowed on any Node
#[derive(Serialize, Deserialize, Default, Clone, PartialEq, Eq)]
pub struct NodeProps {
    pub max_tokens: Option<usize>,
    pub name: Option<String>,
    pub capture_name: Option<String>,
}

#[derive(Clone)]
pub struct GenOptions {
    /// Regular expression matching the body of generation.
    pub body_rx: RegexAst,

    /// The whole generation must match `body_rx + stop_rx`.
    /// Whatever matched `stop_rx` is discarded.
    /// If `stop_rx` is empty, it's assumed to be EOS.
    pub stop_rx: RegexAst,

    /// When set, the string matching `stop_rx` will be output as a capture
    /// with the given name.
    pub stop_capture_name: Option<String>,

    /// Lazy gen()s take the shortest match. Non-lazy take the longest.
    /// If not specified, the gen() is lazy if stop_rx is non-empty.
    pub lazy: Option<bool>,

    /// Treat stop_rx as suffix, i.e., do not hide it from the LLM
    /// (but do not include it in the capture).
    pub is_suffix: Option<bool>,

    /// Override sampling temperature.
    pub temperature: Option<f32>,
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct GenGrammarOptions {
    pub grammar: GrammarId,

    /// Override sampling temperature.
    pub temperature: Option<f32>,
}

#[derive(Serialize, Deserialize, Hash, PartialEq, Eq, Clone, Debug)]
#[serde(untagged)]
pub enum GrammarId {
    Name(String),
}

impl Display for GrammarId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            GrammarId::Name(s) => write!(f, "@{}", s),
        }
    }
}

#[derive(Serialize, Deserialize, Clone, Debug)]
#[serde(deny_unknown_fields)]
pub struct RegexExt {
    /// The lexeme should accept any (possibly empty) contiguous sequence of these chunks.
    pub substring_chunks: Option<Vec<String>>,
    /// Similar to `substring_chunks: s.split(/\s+/)`
    pub substring_words: Option<String>,
    /// Similar to `substring_chunks: s.split('')`
    pub substring_chars: Option<String>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum StopReason {
    /// Parser has not emitted stop() yet.
    NotStopped,
    /// max_tokens limit on the total number of tokens has been reached.
    MaxTokensTotal,
    /// max_tokens limit on the number of tokens in the top-level parser has been reached. (no longer used)
    MaxTokensParser,
    /// Top-level parser indicates that no more bytes can be added.
    NoExtension,
    /// Top-level parser indicates that no more bytes can be added, however it was recognized late.
    NoExtensionBias,
    /// Top-level parser allowed EOS (as it was in an accepting state), and EOS was generated.
    EndOfSentence,
    /// Something went wrong with creating a nested parser.
    InternalError,
    /// The lexer is too complex
    LexerTooComplex,
    /// The parser is too complex
    ParserTooComplex,
}

impl Display for StopReason {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}",
            serde_json::to_value(self).unwrap().as_str().unwrap()
        )
    }
}

impl StopReason {
    pub fn is_ok(&self) -> bool {
        matches!(
            self,
            StopReason::NotStopped
                | StopReason::EndOfSentence
                | StopReason::NoExtension
                | StopReason::NoExtensionBias
        )
    }
}

#[derive(Clone, Serialize, Deserialize, Debug)]
#[serde(default)]
#[repr(C)]
pub struct ParserLimits {
    /// For non-ambiguous grammars, this is the maximum "branching factor" of the grammar.
    /// For ambiguous grammars, this might get hit much quicker.
    /// Default: 2000
    pub max_items_in_row: usize,

    /// How much "fuel" are we willing to spend to build initial lexer regex AST nodes.
    /// Default: 1_000_000
    /// Speed: 50k/ms
    pub initial_lexer_fuel: u64,

    /// Maximum lexer fuel for computation of the whole token mask.
    /// Default: 200_000
    /// Speed: 14k/ms
    pub step_lexer_fuel: u64,

    /// Number of Earley items created for the whole token mask.
    /// Default: 50_000
    /// Speed: 20k/ms
    pub step_max_items: usize,

    /// Maximum number of lexer states.
    /// Affects memory consumption, but not the speed for the most part.
    /// Default: 250_000
    /// Speed: ~1-2kB of memory per state
    pub max_lexer_states: usize,

    /// Maximum size of the grammar (symbols in productions)
    /// Default: 500_000 (a few megabytes of JSON)
    pub max_grammar_size: usize,

    /// If true, we'll run any extremely large regexes against the whole
    /// trie of the tokenizer while constructing the lexer.
    /// This reduces future mask computation time, but increases
    /// the time it takes to construct the lexer.
    /// Default: true
    pub precompute_large_lexemes: bool,
}

impl Default for ParserLimits {
    fn default() -> Self {
        Self {
            max_items_in_row: 2000,
            initial_lexer_fuel: 1_000_000, // fhir schema => 500k
            step_lexer_fuel: 200_000,      //
            max_lexer_states: 250_000,     //
            max_grammar_size: 500_000,     // fhir schema => 200k
            step_max_items: 50_000,        //
            precompute_large_lexemes: true,
        }
    }
}

impl TopLevelGrammar {
    pub fn from_lark_or_grammar_list(s: &str) -> Result<Self> {
        let first_non_whitespace = s.chars().find(|c| !c.is_whitespace());
        if first_non_whitespace.is_none() {
            bail!("Empty grammar");
        }
        if first_non_whitespace == Some('{') {
            Ok(serde_json::from_str(s)?)
        } else {
            Ok(TopLevelGrammar::from_lark(s.to_string()))
        }
    }

    pub fn from_regex(rx: &str) -> Self {
        Self::from_grammar(GrammarWithLexer::from_regex(rx))
    }

    pub fn from_lark(lark_grammar: String) -> Self {
        Self::from_grammar(GrammarWithLexer::from_lark(lark_grammar))
    }

    pub fn from_json_schema(json_schema: Value) -> Self {
        Self::from_grammar(GrammarWithLexer::from_json_schema(json_schema))
    }

    pub fn from_grammar(grammar: GrammarWithLexer) -> Self {
        TopLevelGrammar {
            grammars: vec![grammar],
            max_tokens: None,
        }
    }

    /// The data is of different format, depending on tag:
    /// - "regex" - data is regular expression in rust regex format
    ///   see https://docs.rs/regex/latest/regex/#syntax
    /// - "json" or "json_schema" - data is (stringifed) JSON schema
    ///   see https://github.com/guidance-ai/llguidance/blob/main/docs/json_schema.md
    /// - "json_object" - equivalent to JSON schema: {"type":"object"}
    /// - "lark" - data is grammar in a variant of Lark syntax
    ///   see https://github.com/guidance-ai/llguidance/blob/main/docs/syntax.md
    /// - "llguidance" or "guidance" - data is a list of Lark or JSON schemas in JSON format
    pub fn from_tagged_str(tag: &str, data: &str) -> Result<Self> {
        match tag {
            "regex" => Ok(Self::from_regex(data)),
            "json" | "json_schema" => Ok(Self::from_json_schema(serde_json::from_str(data)?)),
            "json_object" => Ok(Self::from_json_schema(json!({"type": "object"}))),
            "lark" => Ok(Self::from_lark(data.to_string())),
            "llguidance" | "guidance" => Self::from_lark_or_grammar_list(data),
            _ => bail!("unknown constraint type: {tag}"),
        }
    }
}

impl GrammarWithLexer {
    pub fn from_lark(lark_grammar: String) -> Self {
        GrammarWithLexer {
            name: Some("lark_grammar".to_string()),
            lark_grammar: Some(lark_grammar),
            ..GrammarWithLexer::default()
        }
    }

    pub fn from_json_schema(json_schema: Value) -> Self {
        GrammarWithLexer {
            name: Some("json_schema".to_string()),
            json_schema: Some(json_schema),
            ..GrammarWithLexer::default()
        }
    }

    pub fn from_regex(rx: &str) -> Self {
        let rx = regex_to_lark(rx, "");
        let mut r = Self::from_lark(format!("start: /{}/", rx));
        r.name = Some("regex".to_string());
        r
    }
}
