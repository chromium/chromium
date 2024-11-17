/// Define `Resolver` trait and implement it on some hashmaps and also define the `Resolver` tuple
/// composition. Provide also some utility functions related to how to create a `Resolver` and
/// resolving render.
///
use std::borrow::Cow;
use std::collections::HashMap;

use syn::{parse_quote, Expr, Pat};

use crate::parse::Fixture;

pub(crate) mod fixtures {
    use quote::format_ident;

    use crate::parse::arguments::ArgumentsInfo;

    use super::*;

    pub(crate) fn get<'a>(
        arguments: &ArgumentsInfo,
        fixtures: impl Iterator<Item = &'a Fixture>,
    ) -> impl Resolver + 'a {
        fixtures
            .map(|f| {
                (
                    arguments.inner_pat(&f.arg).clone(),
                    extract_resolve_expression(f),
                )
            })
            .collect::<HashMap<_, Expr>>()
    }

    fn extract_resolve_expression(fixture: &Fixture) -> syn::Expr {
        let resolve = fixture.resolve.clone();
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
        fn resolve_by_use_the_given_name(
            #[case] args: &[&str],
            #[case] expected: &str,
            #[values(None, Some("minnie"), Some("__destruct_1"))] inner_pat: Option<&str>,
        ) {
            let data = vec![fixture("pippo", args)];
            let mut arguments: ArgumentsInfo = Default::default();
            let mut request = pat("pippo");
            if let Some(inner) = inner_pat {
                arguments.set_inner_pat(pat("pippo"), pat(inner));
                request = pat(inner);
            }

            let resolver = get(&arguments, data.iter());

            let resolved = resolver.resolve(&request).unwrap().into_owned();

            assert_eq!(resolved, format!("pippo::{}", expected).ast());
        }

        #[rstest]
        #[case(&[], "default()")]
        #[case(&["my_expression"], "partial_1(my_expression)")]
        #[case(&["first", "other"], "partial_2(first, other)")]
        fn resolve_by_use_the_resolve_field(
            #[case] args: &[&str],
            #[case] expected: &str,
            #[values("pluto", "minnie::pluto")] resolver_path: &str,
            #[values(None, Some("minnie"), Some("__destruct_1"))] inner_pat: Option<&str>,
        ) {
            let data = vec![fixture("pippo", args).with_resolve(resolver_path)];
            let mut arguments: ArgumentsInfo = Default::default();
            let mut request = pat("pippo");
            if let Some(inner) = inner_pat {
                arguments.set_inner_pat(pat("pippo"), pat(inner));
                request = pat(inner);
            }
            let resolver = get(&arguments, data.iter());

            let resolved = resolver.resolve(&request).unwrap().into_owned();

            assert_eq!(resolved, format!("{}::{}", resolver_path, expected).ast());
        }
    }
}

pub(crate) mod values {
    use super::*;
    use crate::parse::fixture::ArgumentValue;

    pub(crate) fn get<'a>(values: impl Iterator<Item = &'a ArgumentValue>) -> impl Resolver + 'a {
        values
            .map(|av| (av.arg.clone(), &av.expr))
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
                resolver.resolve(&pat("pippo")).unwrap().into_owned(),
                "42".ast()
            );
            assert_eq!(
                resolver.resolve(&pat("donaldduck")).unwrap().into_owned(),
                "vec![1,2]".ast()
            );
        }
    }
}

/// A trait that `resolve` the given ident to expression code to assign the value.
pub(crate) trait Resolver {
    fn resolve(&self, arg: &Pat) -> Option<Cow<Expr>>;
}

impl<'a> Resolver for HashMap<Pat, &'a Expr> {
    fn resolve(&self, arg: &Pat) -> Option<Cow<Expr>> {
        self.get(arg)
            .or_else(|| self.get(&pat_invert_mutability(arg)))
            .map(|&c| Cow::Borrowed(c))
    }
}

impl Resolver for HashMap<Pat, Expr> {
    fn resolve(&self, arg: &Pat) -> Option<Cow<Expr>> {
        self.get(arg).map(Cow::Borrowed)
    }
}

impl<R1: Resolver, R2: Resolver> Resolver for (R1, R2) {
    fn resolve(&self, arg: &Pat) -> Option<Cow<Expr>> {
        self.0.resolve(arg).or_else(|| self.1.resolve(arg))
    }
}

impl<R: Resolver + ?Sized> Resolver for &R {
    fn resolve(&self, arg: &Pat) -> Option<Cow<Expr>> {
        (*self).resolve(arg)
    }
}

impl<R: Resolver + ?Sized> Resolver for Box<R> {
    fn resolve(&self, arg: &Pat) -> Option<Cow<Expr>> {
        (**self).resolve(arg)
    }
}

impl Resolver for (Pat, Expr) {
    fn resolve(&self, arg: &Pat) -> Option<Cow<Expr>> {
        if arg == &self.0 {
            Some(Cow::Borrowed(&self.1))
        } else {
            None
        }
    }
}

pub(crate) fn pat_invert_mutability(p: &Pat) -> Pat {
    match p.clone() {
        Pat::Ident(mut ident) => {
            ident.mutability = match ident.mutability {
                Some(_) => None,
                None => Some(syn::parse_quote! { mut }),
            };
            syn::Pat::Ident(ident)
        }
        p => p,
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
        let arg = first_arg_pat(&ast);
        let expected = expr("bar()");
        let mut resolver = HashMap::new();

        resolver.insert(pat("foo").with_mut(), &expected);

        assert_eq!(expected, (&resolver).resolve(&arg).unwrap().into_owned())
    }

    #[test]
    fn return_the_given_expression_also_if_not_mut_searched() {
        let ast = parse_str("fn function(foo: String) {}").unwrap();
        let arg = first_arg_pat(&ast);
        let expected = expr("bar()");
        let mut resolver = HashMap::new();

        resolver.insert(pat("foo").with_mut(), &expected);

        assert_eq!(expected, (&resolver).resolve(&arg).unwrap().into_owned())
    }

    #[test]
    fn return_none_for_unknown_argument() {
        let ast = "fn function(mut fix: String) {}".ast();
        let arg = first_arg_pat(&ast);

        assert!(EmptyResolver.resolve(&arg).is_none())
    }
}
