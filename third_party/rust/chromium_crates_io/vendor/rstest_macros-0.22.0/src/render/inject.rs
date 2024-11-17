use std::borrow::Cow;

use proc_macro2::TokenStream;
use quote::quote;
use syn::{parse_quote, Expr, FnArg, Ident, Pat, Stmt, Type};

use crate::{
    refident::{IntoPat, MaybeIdent, MaybePat, MaybeType},
    render::crate_resolver::crate_name,
    resolver::Resolver,
    utils::{fn_arg_mutability, IsLiteralExpression},
};

pub(crate) fn resolve_arguments<'a>(
    args: impl Iterator<Item = &'a FnArg>,
    resolver: &impl Resolver,
    generic_types: &[Ident],
) -> TokenStream {
    let define_vars = args.map(|arg| ArgumentResolver::new(resolver, generic_types).resolve(arg));
    quote! {
        #(#define_vars)*
    }
}

struct ArgumentResolver<'resolver, 'idents, 'f, R>
where
    R: Resolver + 'resolver,
{
    resolver: &'resolver R,
    generic_types_names: &'idents [Ident],
    magic_conversion: &'f dyn Fn(Cow<Expr>, &Type) -> Expr,
}

impl<'resolver, 'idents, 'f, R> ArgumentResolver<'resolver, 'idents, 'f, R>
where
    R: Resolver + 'resolver,
{
    fn new(resolver: &'resolver R, generic_types_names: &'idents [Ident]) -> Self {
        Self {
            resolver,
            generic_types_names,
            magic_conversion: &handling_magic_conversion_code,
        }
    }

    fn resolve(&self, arg: &FnArg) -> Option<Stmt> {
        let pat = arg.maybe_pat()?;
        let mutability = fn_arg_mutability(arg);
        let unused_mut: Option<syn::Attribute> = mutability
            .as_ref()
            .map(|_| parse_quote! {#[allow(unused_mut)]});
        let arg_type = arg.maybe_type()?;
        let fixture_name = self.fixture_name(pat);

        let mut fixture = self
            .resolver
            .resolve(pat)
            .or_else(|| self.resolver.resolve(&fixture_name.clone().into_pat()))
            .unwrap_or_else(|| default_fixture_resolve(&fixture_name));

        if fixture.is_literal() && self.type_can_be_get_from_literal_str(arg_type) {
            fixture = Cow::Owned((self.magic_conversion)(fixture, arg_type));
        }
        Some(parse_quote! {
            #unused_mut
            let #pat = #fixture;
        })
    }

    fn fixture_name(&self, ident: &Pat) -> Ident {
        let ident = ident
            .maybe_ident()
            .cloned()
            .expect("BUG: Here all arguments should be PatIdent types");
        let id_str = ident.to_string();
        if id_str.starts_with('_') && !id_str.starts_with("__") {
            Ident::new(&id_str[1..], ident.span())
        } else {
            ident
        }
    }

    fn type_can_be_get_from_literal_str(&self, t: &Type) -> bool {
        // Check valid type to apply magic conversion
        match t {
            Type::ImplTrait(_)
            | Type::TraitObject(_)
            | Type::Infer(_)
            | Type::Group(_)
            | Type::Macro(_)
            | Type::Never(_)
            | Type::Paren(_)
            | Type::Verbatim(_)
            | Type::Slice(_) => return false,
            _ => {}
        }
        match t.maybe_ident() {
            Some(id) => !self.generic_types_names.contains(id),
            None => true,
        }
    }
}

fn default_fixture_resolve(ident: &Ident) -> Cow<Expr> {
    Cow::Owned(parse_quote! { #ident::default() })
}

fn handling_magic_conversion_code(fixture: Cow<Expr>, arg_type: &Type) -> Expr {
    let rstest_path = crate_name();
    parse_quote! {
        {
            use #rstest_path::magic_conversion::*;
            (&&&Magic::<#arg_type>(std::marker::PhantomData)).magic_conversion(#fixture)
        }
    }
}

#[cfg(test)]
mod should {
    use super::*;
    use crate::{
        test::{assert_eq, *},
        utils::fn_args,
    };

    #[rstest]
    #[case::as_is("fix: String", "let fix = fix::default();")]
    #[case::without_underscore("_fix: String", "let _fix = fix::default();")]
    #[case::do_not_remove_inner_underscores("f_i_x: String", "let f_i_x = f_i_x::default();")]
    #[case::do_not_remove_double_underscore("__fix: String", "let __fix = __fix::default();")]
    #[case::preserve_mut_but_annotate_as_allow_unused_mut(
        "mut fix: String",
        "#[allow(unused_mut)] let mut fix = fix::default();"
    )]
    fn call_fixture(#[case] arg_str: &str, #[case] expected: &str) {
        let arg = arg_str.ast();

        let injected = ArgumentResolver::new(&EmptyResolver {}, &[])
            .resolve(&arg)
            .unwrap();

        assert_eq!(injected, expected.ast());
    }

    #[rstest]
    #[case::as_is("fix: String", ("fix", expr("bar()")), "let fix = bar();")]
    #[case::with_allow_unused_mut("mut fix: String", ("fix", expr("bar()")), "#[allow(unused_mut)] let mut fix = bar();")]
    #[case::without_underscore("_fix: String", ("fix", expr("bar()")), "let _fix = bar();")]
    #[case::without_remove_underscore_if_value("_orig: S", ("_orig", expr("S{}")), r#"let _orig = S{};"#)]
    fn call_given_fixture(
        #[case] arg_str: &str,
        #[case] rule: (&str, Expr),
        #[case] expected: &str,
    ) {
        let arg = arg_str.ast();
        let mut resolver = std::collections::HashMap::new();
        resolver.insert(pat(rule.0), &rule.1);

        let injected = ArgumentResolver::new(&resolver, &[]).resolve(&arg).unwrap();

        assert_eq!(injected, expected.ast());
    }

    fn _mock_conversion_code(fixture: Cow<Expr>, arg_type: &Type) -> Expr {
        parse_quote! {
            #fixture as #arg_type
        }
    }

    #[rstest]
    #[case::implement_it(
        "fn test(arg: MyType){}",
        0,
        r#"let arg = "value to convert" as MyType;"#
    )]
    #[case::discard_impl(
        "fn test(arg: impl AsRef<str>){}",
        0,
        r#"let arg = "value to convert";"#
    )]
    #[case::discard_generic_type(
        "fn test<S: AsRef<str>>(arg: S){}",
        0,
        r#"let arg = "value to convert";"#
    )]
    fn handle_magic_conversion(#[case] fn_str: &str, #[case] n_arg: usize, #[case] expected: &str) {
        let function = fn_str.ast();
        let arg = fn_args(&function).nth(n_arg).unwrap();
        let generics = function
            .sig
            .generics
            .type_params()
            .map(|tp| &tp.ident)
            .cloned()
            .collect::<Vec<_>>();

        let mut resolver = std::collections::HashMap::new();
        let expr = expr(r#""value to convert""#);
        resolver.insert(arg.maybe_pat().unwrap().clone(), &expr);

        let ag = ArgumentResolver {
            resolver: &resolver,
            generic_types_names: &generics,
            magic_conversion: &_mock_conversion_code,
        };

        let injected = ag.resolve(&arg).unwrap();

        assert_eq!(injected, expected.ast());
    }
}
