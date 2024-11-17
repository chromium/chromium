use proc_macro2::TokenStream;
use quote::ToTokens;
use syn::{
    parse::{Parse, ParseStream, Result},
    Expr, Ident, Pat, Token,
};

use crate::refident::IntoPat;

use super::expressions::Expressions;

#[derive(Debug, PartialEq, Clone)]
pub(crate) struct Value {
    pub(crate) expr: Expr,
    pub(crate) description: Option<String>,
}

impl Value {
    pub(crate) fn new(expr: Expr, description: Option<String>) -> Self {
        Self { expr, description }
    }

    pub(crate) fn description(&self) -> String {
        self.description
            .clone()
            .unwrap_or_else(|| self.expr.to_token_stream().to_string())
    }
}

impl From<Expr> for Value {
    fn from(expr: Expr) -> Self {
        Self::new(expr, None)
    }
}

#[derive(Debug, PartialEq, Clone)]
pub(crate) struct ValueList {
    pub(crate) arg: Pat,
    pub(crate) values: Vec<Value>,
}

impl Parse for ValueList {
    fn parse(input: ParseStream) -> Result<Self> {
        let ident: Ident = input.parse()?;
        let _to: Token![=>] = input.parse()?;
        let content;
        let paren = syn::bracketed!(content in input);
        let values: Expressions = content.parse()?;

        let ret = Self {
            arg: ident.into_pat(),
            values: values.take().into_iter().map(|e| e.into()).collect(),
        };
        if ret.values.is_empty() {
            Err(syn::Error::new(
                paren.span.join(),
                "Values list should not be empty",
            ))
        } else {
            Ok(ret)
        }
    }
}

impl ToTokens for ValueList {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        self.arg.to_tokens(tokens)
    }
}

#[cfg(test)]
mod should {
    use crate::test::{assert_eq, *};

    use super::*;

    mod parse_values_list {
        use super::assert_eq;
        use super::*;

        fn parse_values_list<S: AsRef<str>>(values_list: S) -> ValueList {
            parse_meta(values_list)
        }

        #[test]
        fn some_literals() {
            let literals = literal_expressions_str();
            let name = "argument";

            let values_list = parse_values_list(format!(
                r#"{} => [{}]"#,
                name,
                literals
                    .iter()
                    .map(ToString::to_string)
                    .collect::<Vec<String>>()
                    .join(", ")
            ));

            assert_eq!(name, &values_list.arg.display_code());
            assert_eq!(values_list.args(), to_args!(literals));
        }

        #[test]
        fn raw_code() {
            let values_list = parse_values_list(r#"no_mater => [vec![1,2,3]]"#);

            assert_eq!(values_list.args(), to_args!(["vec![1, 2, 3]"]));
        }

        #[test]
        #[should_panic]
        fn raw_code_with_parsing_error() {
            parse_values_list(r#"other => [some:<>(1,2,3)]"#);
        }

        #[test]
        #[should_panic(expected = r#"expected square brackets"#)]
        fn forget_brackets() {
            parse_values_list(r#"other => 42"#);
        }
    }
}
