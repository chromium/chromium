/// Define `Resolver` trait and implement it on some hashmaps and also define the `Resolver` tuple
/// composition. Provide also some utility functions related to how to create a `Resolver` and
/// resolving render.
///
use std::borrow::Cow;
use std::collections::HashMap;

use proc_macro2::Ident;
use syn::{parse_quote, Expr};

use crate::parse::Fixture;

pub(crate) mod fixtures {
    use quote::format_ident;

    use super::*;

    pub(crate) fn get<'a>(fixtures: impl Iterator<Item = &'a Fixture>) -> impl Resolver + 'a {
        fixtures
            .map(|f| (f.name.to_string(), extract_resolve_expression(f)))
            .collect::<HashMap<_, Expr>>()
    }

    fn extract_resolve_expression(fixture: &Fixture) -> syn::Expr {
        let resolve = fixture.resolve.as_ref().unwrap_or(&fixture.name);
        let positional = &fixture.positional.0;
        let f_name = match positional.len() {
            0 => format_ident!("default"),
            l => format_ident!("partial_{}", l),
        };
        parse_quote! { #resolve::#f_name(#(#positional), *) }
    }

    #[cfg(test)]
    mod should {
        use super::*;
        use crate::test::{assert_eq, *};

        #[rstest]
        #[case(&[], "default()")]
        #[case(&["my_expression"], "partial_1(my_expression)")]
        #[case(&["first", "other"], "partial_2(first, other)")]
        fn resolve_by_use_the_given_name(#[case] args: &[&str], #[case] expected: &str) {
            let data = vec![fixture("pippo", args)];
            let resolver = get(data.iter());

            let resolved = resolver.resolve(&ident("pippo")).unwrap().into_owned();

            assert_eq!(resolved, format!("pippo::{}", expected).ast());
        }

        #[rstest]
        #[case(&[], "default()")]
        #[case(&["my_expression"], "partial_1(my_expression)")]
        #[case(&["first", "other"], "partial_2(first, other)")]
        fn resolve_by_use_the_resolve_field(#[case] args: &[&str], #[case] expected: &str) {
            let data = vec![fixture("pippo", args).with_resolve("pluto")];
            let resolver = get(data.iter());

            let resolved = resolver.resolve(&ident("pippo")).unwrap().into_owned();

            assert_eq!(resolved, format!("pluto::{}", expected).ast());
        }
    }
}

pub(crate) mod values {
    use super::*;
    use crate::parse::fixture::ArgumentValue;

    pub(crate) fn get<'a>(values: impl Iterator<Item = &'a ArgumentValue>) -> impl Resolver + 'a {
        values
            .map(|av| (av.name.to_string(), &av.expr))
            .collect::<HashMap<_, &'a Expr>>()
    }

    #[cfg(test)]
    mod should {
        use super::*;
        use crate::test::{assert_eq, *};

        #[test]
        fn resolve_by_use_the_given_name() {
            let data = vec![
                arg_value("pippo", "42"),
                arg_value("donaldduck", "vec![1,2]"),
            ];
            let resolver = get(data.iter());

            assert_eq!(
                resolver.resolve(&ident("pippo")).unwrap().into_owned(),
                "42".ast()
            );
            assert_eq!(
                resolver.resolve(&ident("donaldduck")).unwrap().into_owned(),
                "vec![1,2]".ast()
            );
        }
    }
}

/// A trait that `resolve` the given ident to expression code to assign the value.
pub(crate) trait Resolver {
    fn resolve(&self, ident: &Ident) -> Option<Cow<Expr>>;
}

impl<'a> Resolver for HashMap<String, &'a Expr> {
    fn resolve(&self, ident: &Ident) -> Option<Cow<Expr>> {
        let ident = ident.to_string();
        self.get(&ident).map(|&c| Cow::Borrowed(c))
    }
}

impl<'a> Resolver for HashMap<String, Expr> {
    fn resolve(&self, ident: &Ident) -> Option<Cow<Expr>> {
        let ident = ident.to_string();
        self.get(&ident).map(Cow::Borrowed)
    }
}

impl<R1: Resolver, R2: Resolver> Resolver for (R1, R2) {
    fn resolve(&self, ident: &Ident) -> Option<Cow<Expr>> {
        self.0.resolve(ident).or_else(|| self.1.resolve(ident))
    }
}

impl<R: Resolver + ?Sized> Resolver for &R {
    fn resolve(&self, ident: &Ident) -> Option<Cow<Expr>> {
        (*self).resolve(ident)
    }
}

impl<R: Resolver + ?Sized> Resolver for Box<R> {
    fn resolve(&self, ident: &Ident) -> Option<Cow<Expr>> {
        (**self).resolve(ident)
    }
}

impl Resolver for (String, Expr) {
    fn resolve(&self, ident: &Ident) -> Option<Cow<Expr>> {
        if *ident == self.0 {
            Some(Cow::Borrowed(&self.1))
        } else {
            None
        }
    }
}

#[cfg(test)]
mod should {
    use super::*;
    use crate::test::{assert_eq, *};
    use syn::parse_str;

    #[test]
    fn return_the_given_expression() {
        let ast = parse_str("fn function(mut foo: String) {}").unwrap();
        let arg = first_arg_ident(&ast);
        let expected = expr("bar()");
        let mut resolver = HashMap::new();

        resolver.insert("foo".to_string(), &expected);

        assert_eq!(expected, (&resolver).resolve(&arg).unwrap().into_owned())
    }

    #[test]
    fn return_none_for_unknown_argument() {
        let ast = "fn function(mut fix: String) {}".ast();
        let arg = first_arg_ident(&ast);

        assert!(EmptyResolver.resolve(&arg).is_none())
    }
}
