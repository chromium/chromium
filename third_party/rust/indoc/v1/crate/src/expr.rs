use crate::error::{Error, Result};
use proc_macro::token_stream::IntoIter as TokenIter;
use proc_macro::{Spacing, Span, TokenStream, TokenTree};
use std::iter;

pub struct Expr(TokenStream);

pub fn parse(input: &mut TokenIter) -> Result<Expr> {
    #[derive(PartialEq)]
    enum Lookbehind {
        JointColon,
        DoubleColon,
        Other,
    }

    let mut expr = TokenStream::new();
    let mut lookbehind = Lookbehind::Other;
    let mut angle_bracket_depth = 0;

    loop {
        match input.next() {
            Some(TokenTree::Punct(punct)) => {
                let ch = punct.as_char();
                let spacing = punct.spacing();
                expr.extend(iter::once(TokenTree::Punct(punct)));
                lookbehind = match ch {
                    ',' if angle_bracket_depth == 0 => return Ok(Expr(expr)),
                    ':' if lookbehind == Lookbehind::JointColon => Lookbehind::DoubleColon,
                    ':' if spacing == Spacing::Joint => Lookbehind::JointColon,
                    '<' if lookbehind == Lookbehind::DoubleColon => {
                        angle_bracket_depth += 1;
                        Lookbehind::Other
                    }
                    '>' if angle_bracket_depth > 0 => {
                        angle_bracket_depth -= 1;
                        Lookbehind::Other
                    }
                    _ => Lookbehind::Other,
                };
            }
            Some(token) => expr.extend(iter::once(token)),
            None => {
                return Err(Error::new(
                    Span::call_site(),
                    "unexpected end of macro input",
                ))
            }
        }
    }
}

impl Expr {
    pub fn into_tokens(self) -> TokenStream {
        self.0
    }
}
