use proc_macro2::TokenStream;
use quote::ToTokens;
use syn::{
    parse::{Parse, ParseStream, Result},
    Expr, Ident, Token,
};

use crate::refident::RefIdent;

use super::expressions::Expressions;

#[derive(Debug, PartialEq, Clone)]
pub(crate) struct ValueList {
    pub(crate) arg: Ident,
    pub(crate) values: Vec<Expr>,
}

impl Parse for ValueList {
    fn parse(input: ParseStream) -> Result<Self> {
        let arg = input.parse()?;
        let _to: Token![=>] = input.parse()?;
        let content;
        let paren = syn::bracketed!(content in input);
        let values: Expressions = content.parse()?;

        let ret = Self {
            arg,
            values: values.take(),
        };
        if ret.values.is_empty() {
            Err(syn::Error::new(
                paren.span,
                "Values list should not be empty",
            ))
        } else {
            Ok(ret)
        }
    }
}

impl RefIdent for ValueList {
    fn ident(&self) -> &Ident {
        &self.arg
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

            assert_eq!(name, &values_list.arg.to_string());
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
