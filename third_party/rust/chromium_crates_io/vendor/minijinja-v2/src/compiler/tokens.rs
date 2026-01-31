use std::fmt;

/// Represents a token in the stream.
#[derive(Debug)]
#[cfg_attr(
    feature = "unstable_machinery_serde",
    derive(serde::Serialize),
    serde(tag = "name", content = "payload")
)]
pub enum Token<'a> {
    /// Raw template data.
    TemplateData(&'a str),
    /// Variable block start.
    VariableStart,
    /// Variable block end
    VariableEnd,
    /// Statement block start
    BlockStart,
    /// Statement block end
    BlockEnd,
    /// An identifier.
    Ident(&'a str),
    /// A borrowed string.
    Str(&'a str),
    /// An allocated string.
    String(Box<str>),
    /// An integer (limited to i64)
    Int(u64),
    /// A large integer
    Int128(Box<u128>),
    /// A float
    Float(f64),
    /// A plus (`+`) operator.
    Plus,
    /// A plus (`-`) operator.
    Minus,
    /// A mul (`*`) operator.
    Mul,
    /// A div (`/`) operator.
    Div,
    /// A floor division (`//`) operator.
    FloorDiv,
    /// Power operator (`**`).
    Pow,
    /// A mod (`%`) operator.
    Mod,
    /// A dot operator (`.`)
    Dot,
    /// The comma operator (`,`)
    Comma,
    /// The colon operator (`:`)
    Colon,
    /// The tilde operator (`~`)
    Tilde,
    /// The assignment operator (`=`)
    Assign,
    /// The pipe symbol.
    Pipe,
    /// `==` operator
    Eq,
    /// `!=` operator
    Ne,
    /// `>` operator
    Gt,
    /// `>=` operator
    Gte,
    /// `<` operator
    Lt,
    /// `<=` operator
    Lte,
    /// Open Bracket
    BracketOpen,
    /// Close Bracket
    BracketClose,
    /// Open Parenthesis
    ParenOpen,
    /// Close Parenthesis
    ParenClose,
    /// Open Brace
    BraceOpen,
    /// Close Brace
    BraceClose,
}

impl fmt::Display for Token<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Token::TemplateData(_) => f.write_str("template-data"),
            Token::VariableStart => f.write_str("start of variable block"),
            Token::VariableEnd => f.write_str("end of variable block"),
            Token::BlockStart => f.write_str("start of block"),
            Token::BlockEnd => f.write_str("end of block"),
            Token::Ident(_) => f.write_str("identifier"),
            Token::Str(_) | Token::String(_) => f.write_str("string"),
            Token::Int(_) | Token::Int128(_) => f.write_str("integer"),
            Token::Float(_) => f.write_str("float"),
            Token::Plus => f.write_str("`+`"),
            Token::Minus => f.write_str("`-`"),
            Token::Mul => f.write_str("`*`"),
            Token::Div => f.write_str("`/`"),
            Token::FloorDiv => f.write_str("`//`"),
            Token::Pow => f.write_str("`**`"),
            Token::Mod => f.write_str("`%`"),
            Token::Dot => f.write_str("`.`"),
            Token::Comma => f.write_str("`,`"),
            Token::Colon => f.write_str("`:`"),
            Token::Tilde => f.write_str("`~`"),
            Token::Assign => f.write_str("`=`"),
            Token::Pipe => f.write_str("`|`"),
            Token::Eq => f.write_str("`==`"),
            Token::Ne => f.write_str("`!=`"),
            Token::Gt => f.write_str("`>`"),
            Token::Gte => f.write_str("`>=`"),
            Token::Lt => f.write_str("`<`"),
            Token::Lte => f.write_str("`<=`"),
            Token::BracketOpen => f.write_str("`[`"),
            Token::BracketClose => f.write_str("`]`"),
            Token::ParenOpen => f.write_str("`(`"),
            Token::ParenClose => f.write_str("`)`"),
            Token::BraceOpen => f.write_str("`{`"),
            Token::BraceClose => f.write_str("`}`"),
        }
    }
}

/// Token span information
#[derive(Clone, Copy, Default, PartialEq, Eq)]
#[cfg_attr(feature = "unstable_machinery_serde", derive(serde::Serialize))]
pub struct Span {
    pub start_line: u16,
    pub start_col: u16,
    pub start_offset: u32,
    pub end_line: u16,
    pub end_col: u16,
    pub end_offset: u32,
}

impl fmt::Debug for Span {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if f.alternate() {
            f.debug_struct("Span")
                .field("start_line", &self.start_line)
                .field("start_col", &self.start_col)
                .field("start_offset", &self.start_offset)
                .field("end_line", &self.end_line)
                .field("end_col", &self.end_col)
                .field("end_offset", &self.end_offset)
                .finish()
        } else {
            write!(
                f,
                " @ {}:{}-{}:{}",
                self.start_line, self.start_col, self.end_line, self.end_col
            )
        }
    }
}
