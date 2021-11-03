use syn::ext::IdentExt;
use syn::parse::{ParseStream, Result};
use syn::{Ident, LitStr, Token};

pub struct QualifiedName {
    pub segments: Vec<Ident>,
}

impl QualifiedName {
    pub fn parse_unquoted(input: ParseStream) -> Result<Self> {
        let mut segments = Vec::new();
        let mut trailing_punct = true;
        let leading_colons: Option<Token![::]> = input.parse()?;
        while trailing_punct && input.peek(Ident::peek_any) {
            let ident = Ident::parse_any(input)?;
            segments.push(ident);
            let colons: Option<Token![::]> = input.parse()?;
            trailing_punct = colons.is_some();
        }
        if segments.is_empty() && leading_colons.is_none() {
            return Err(input.error("expected path"));
        } else if trailing_punct {
            return Err(input.error("expected path segment"));
        }
        Ok(QualifiedName { segments })
    }

    pub fn parse_quoted_or_unquoted(input: ParseStream) -> Result<Self> {
        if input.peek(LitStr) {
            let lit: LitStr = input.parse()?;
            if lit.value().is_empty() {
                let segments = Vec::new();
                Ok(QualifiedName { segments })
            } else {
                lit.parse_with(Self::parse_unquoted)
            }
        } else {
            Self::parse_unquoted(input)
        }
    }
}
