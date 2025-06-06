use crate::api::RegexExt;

use super::lexer::Location;

/// Represents an item in the grammar (rule, token, or statement).
#[derive(Debug)]
#[allow(clippy::large_enum_variant)]
pub enum Item {
    Rule(Rule),
    Token(TokenDef),
    Statement(Location, Statement),
}

impl Item {
    pub fn location(&self) -> &Location {
        match self {
            Item::Rule(rule) => &rule.expansions.0,
            Item::Token(token) => &token.expansions.0,
            Item::Statement(loc, _) => loc,
        }
    }
}

/// Represents a grammar rule.
#[derive(Debug)]
pub struct Rule {
    pub name: String,
    #[allow(dead_code)]
    pub cond_inline: bool,
    #[allow(dead_code)]
    pub pin_terminals: bool,
    pub params: Option<RuleParams>,
    pub priority: Option<i32>,
    pub expansions: Expansions,

    pub stop: Option<Value>,
    pub suffix: Option<Value>,
    pub max_tokens: Option<usize>,
    pub temperature: Option<f32>,
    pub capture_name: Option<String>,
    pub stop_capture_name: Option<String>,
}

/// Represents a token definition.
#[derive(Debug)]
pub struct TokenDef {
    pub name: String,
    pub params: Option<TokenParams>,
    pub priority: Option<i32>,
    pub expansions: Expansions,
}

/// Represents different types of statements.
#[derive(Debug)]
pub enum Statement {
    Ignore(Expansions),
    Import {
        path: String,
        alias: Option<String>,
    },
    MultiImport {
        path: String,
        names: Vec<String>,
    },
    LLGuidance(serde_json::Value),
    #[allow(dead_code)]
    OverrideRule(Box<Rule>),
    #[allow(dead_code)]
    Declare(Vec<String>),
}

/// Represents parameters for a rule.
#[allow(dead_code)]
#[derive(Debug, Clone)]
pub struct RuleParams(pub Vec<String>);

/// Represents parameters for a token.
#[allow(dead_code)]
#[derive(Debug, Clone)]
pub struct TokenParams(pub Vec<String>);

/// Represents an alternative (OR) of productions in a grammar.
#[derive(Debug)]
pub struct Expansions(pub Location, pub Vec<Alias>);

impl Expansions {
    pub fn single_atom(&self) -> Option<&Atom> {
        if self.1.len() == 1
            && self.1[0].conjuncts.len() == 1
            && self.1[0].conjuncts[0].0.len() == 1
        {
            Some(&self.1[0].conjuncts[0].0[0].atom)
        } else {
            None
        }
    }

    pub fn take_single_atom(&mut self) -> Option<Atom> {
        if self.single_atom().is_none() {
            None
        } else {
            Some(self.1[0].conjuncts.pop().unwrap().0.pop().unwrap().atom)
        }
    }
}

/// Represents an alias in the grammar.
/// Each alias consists of possibly multiple conjuncts (AND).
#[derive(Debug)]
pub struct Alias {
    pub conjuncts: Vec<Expansion>,
    #[allow(dead_code)]
    pub alias: Option<String>,
}

/// Represents a concatenation of expressions in the grammar.
#[derive(Debug)]
pub struct Expansion(pub Vec<Expr>);

/// Represents an expression.
#[derive(Debug)]
pub struct Expr {
    pub atom: Atom,
    pub op: Option<Op>,
    pub range: Option<(i32, i32)>,
}

/// Represents an atom in the grammar.
#[derive(Debug)]
pub enum Atom {
    Group(Expansions),
    Maybe(Expansions),
    Value(Value),
    Not(Box<Atom>),
}

/// Represents different values in the grammar.
#[derive(Debug)]
pub enum Value {
    LiteralRange(String, String),
    Name(String),
    LiteralString(String, String),
    LiteralRegex(String, String),
    GrammarRef(String),
    SpecialToken(String),
    Json(serde_json::Value),
    NestedLark(Vec<Item>),
    RegexExt(RegexExt),
    #[allow(dead_code)]
    TemplateUsage {
        name: String,
        values: Vec<Value>,
    },
}

/// Represents an operator.
#[derive(Debug, Clone)]
pub struct Op(pub String);

impl Rule {
    pub fn stop_like(&self) -> Option<&Value> {
        self.stop.as_ref().or(self.suffix.as_ref())
    }
    pub fn take_stop_like(&mut self) -> Option<Value> {
        self.stop.take().or_else(|| self.suffix.take())
    }
    // follow guidance: "lazy": node.stop_regex != "",
    pub fn is_lazy(&self) -> bool {
        match self.stop_like() {
            Some(Value::LiteralString(s, _)) => !s.is_empty(),
            Some(_) => true,
            None => false,
        }
    }
}
