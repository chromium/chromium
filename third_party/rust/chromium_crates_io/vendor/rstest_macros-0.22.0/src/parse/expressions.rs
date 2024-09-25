use syn::{
    parse::{Parse, ParseStream, Result},
    Expr, Token,
};

pub(crate) struct Expressions(Vec<Expr>);

impl Expressions {
    pub(crate) fn take(self) -> Vec<Expr> {
        self.0
    }
}

impl Parse for Expressions {
    fn parse(input: ParseStream) -> Result<Self> {
        let values = input
            .parse_terminated(Parse::parse, Token![,])?
            .into_iter()
            .collect();
        Ok(Self(values))
    }
}

impl From<Expressions> for Vec<Expr> {
    fn from(expressions: Expressions) -> Self {
        expressions.0
    }
}
