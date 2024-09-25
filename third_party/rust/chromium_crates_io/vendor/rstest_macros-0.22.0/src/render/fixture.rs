use proc_macro2::{Span, TokenStream};
use syn::token::Async;
use syn::{parse_quote, FnArg, Generics, Ident, ItemFn, ReturnType};

use quote::quote;

use super::apply_arguments::ApplyArguments;
use super::{inject, render_exec_call};
use crate::refident::MaybeIdent;
use crate::resolver::{self, Resolver};
use crate::{parse::fixture::FixtureInfo, utils::generics_clean_up};

fn wrap_return_type_as_static_ref(rt: ReturnType) -> ReturnType {
    match rt {
        syn::ReturnType::Type(_, t) => parse_quote! {
           -> &'static #t
        },
        o => o,
    }
}

fn wrap_call_impl_with_call_once_impl(call_impl: TokenStream, rt: &ReturnType) -> TokenStream {
    match rt {
        syn::ReturnType::Type(_, t) => parse_quote! {
            static CELL: std::sync::OnceLock<#t> =
                std::sync::OnceLock::new();
            CELL.get_or_init(|| #call_impl )
        },
        _ => parse_quote! {
            static CELL: std::sync::Once = std::sync::Once::new();
            CELL.call_once(|| #call_impl );
        },
    }
}

pub(crate) fn render(mut fixture: ItemFn, info: FixtureInfo) -> TokenStream {
    let mut arguments = info.arguments.clone();
    fixture.apply_arguments(&mut arguments, &mut ());
    let name = &fixture.sig.ident;
    let asyncness = &fixture.sig.asyncness.clone();
    let inner_args = info
        .arguments
        .replace_fn_args_with_related_inner_pat(fixture.sig.inputs.iter().cloned())
        .collect::<Vec<_>>();
    let args_ident = inner_args
        .iter()
        .filter_map(MaybeIdent::maybe_ident)
        .cloned()
        .collect::<Vec<_>>();
    let orig_attrs = &fixture.attrs;
    let generics = &fixture.sig.generics;
    let mut default_output = info
        .attributes
        .extract_default_type()
        .unwrap_or_else(|| fixture.sig.output.clone());
    let default_generics =
        generics_clean_up(&fixture.sig.generics, std::iter::empty(), &default_output);
    let default_where_clause = &default_generics.where_clause;
    let where_clause = &fixture.sig.generics.where_clause;
    let mut output = fixture.sig.output.clone();
    let visibility = &fixture.vis;
    let resolver = (
        resolver::fixtures::get(&info.arguments, info.data.fixtures()),
        resolver::values::get(info.data.values()),
    );
    let generics_idents = generics
        .type_params()
        .map(|tp| &tp.ident)
        .cloned()
        .collect::<Vec<_>>();
    let inject = inject::resolve_arguments(inner_args.iter(), &resolver, &generics_idents);

    let partials = (1..=inner_args.len()).map(|n| {
        render_partial_impl(
            &inner_args,
            &fixture.sig.output,
            &fixture.sig.generics,
            fixture.sig.asyncness.as_ref(),
            n,
            &resolver,
            &info,
        )
    });

    let args = args_ident
        .iter()
        .map(|arg| parse_quote! { #arg })
        .collect::<Vec<_>>();
    let call_get = render_exec_call(parse_quote! { Self::get }, &args, asyncness.is_some());
    let mut call_impl = render_exec_call(parse_quote! { #name }, &args, asyncness.is_some());

    if info.arguments.is_once() {
        call_impl = wrap_call_impl_with_call_once_impl(call_impl, &output);
        output = wrap_return_type_as_static_ref(output);
        default_output = wrap_return_type_as_static_ref(default_output);
    }

    quote! {
        #[allow(non_camel_case_types)]
        #visibility struct #name {}

        impl #name {
            #(#orig_attrs)*
            #[allow(unused_mut)]
            pub #asyncness fn get #generics (#(#inner_args),*) #output #where_clause {
                #call_impl
            }

            pub #asyncness fn default #default_generics () #default_output #default_where_clause {
                #inject
                #call_get
            }

            #(#partials)*
        }

        #[allow(dead_code)]
        #fixture
    }
}

fn render_partial_impl(
    args: &[FnArg],
    output: &ReturnType,
    generics: &Generics,
    asyncness: Option<&Async>,
    n: usize,
    resolver: &impl Resolver,
    info: &FixtureInfo,
) -> TokenStream {
    let mut output = info
        .attributes
        .extract_partial_type(n)
        .unwrap_or_else(|| output.clone());

    if info.arguments.is_once() {
        output = wrap_return_type_as_static_ref(output);
    }

    let generics = generics_clean_up(generics, args.iter().take(n), &output);
    let where_clause = &generics.where_clause;
    let genercs_idents = generics
        .type_params()
        .map(|tp| &tp.ident)
        .cloned()
        .collect::<Vec<_>>();
    let inject = inject::resolve_arguments(args.iter().skip(n), resolver, &genercs_idents);

    let sign_args = args.iter().take(n);
    let fixture_args = args
        .iter()
        .filter_map(MaybeIdent::maybe_ident)
        .map(|arg| parse_quote! {#arg})
        .collect::<Vec<_>>();
    let name = Ident::new(&format!("partial_{n}"), Span::call_site());

    let call_get = render_exec_call(
        parse_quote! { Self::get },
        &fixture_args,
        asyncness.is_some(),
    );

    quote! {
        #[allow(unused_mut)]
        pub #asyncness fn #name #generics (#(#sign_args),*) #output #where_clause {
            #inject
            #call_get
        }
    }
}

#[cfg(test)]
mod should {
    use rstest_test::{assert_in, assert_not_in};
    use syn::{
        parse::{Parse, ParseStream},
        parse2, parse_str, ItemImpl, ItemStruct, Result,
    };

    use crate::parse::{
        arguments::{ArgumentsInfo, FutureArg},
        Attribute, Attributes,
    };

    use super::*;
    use crate::test::{assert_eq, *};
    use rstest_reuse::*;

    #[derive(Clone)]
    struct FixtureOutput {
        orig: ItemFn,
        fixture: ItemStruct,
        core_impl: ItemImpl,
    }

    impl Parse for FixtureOutput {
        fn parse(input: ParseStream) -> Result<Self> {
            Ok(FixtureOutput {
                fixture: input.parse()?,
                core_impl: input.parse()?,
                orig: input.parse()?,
            })
        }
    }

    fn parse_fixture<S: AsRef<str>>(code: S) -> (ItemFn, FixtureOutput) {
        let item_fn = parse_str::<ItemFn>(code.as_ref()).unwrap();

        let tokens = render(item_fn.clone(), Default::default());
        (item_fn, parse2(tokens).unwrap())
    }

    fn test_maintains_function_visibility(code: &str) {
        let (item_fn, out) = parse_fixture(code);

        assert_eq!(item_fn.vis, out.fixture.vis);
        assert_eq!(item_fn.vis, out.orig.vis);
    }

    fn select_method<S: AsRef<str>>(impl_code: ItemImpl, name: S) -> Option<syn::ImplItemFn> {
        impl_code
            .items
            .into_iter()
            .filter_map(|ii| match ii {
                syn::ImplItem::Fn(f) => Some(f),
                _ => None,
            })
            .find(|f| f.sig.ident == name.as_ref())
    }

    #[test]
    fn maintains_pub_visibility() {
        test_maintains_function_visibility(r#"pub fn test() { }"#);
    }

    #[test]
    fn maintains_no_pub_visibility() {
        test_maintains_function_visibility(r#"fn test() { }"#);
    }

    #[test]
    fn implement_a_get_method_with_input_fixture_signature() {
        let (item_fn, out) = parse_fixture(
            r#"
                    pub fn test<R: AsRef<str>, B>(mut s: String, v: &u32, a: &mut [i32], r: R) -> (u32, B, String, &str)
                            where B: Borrow<u32>
                    { }
                    "#,
        );

        let mut signature = select_method(out.core_impl, "get").unwrap().sig;

        signature.ident = item_fn.sig.ident.clone();

        assert_eq!(item_fn.sig, signature);
    }

    #[test]
    fn return_a_static_reference_if_once_attribute() {
        let item_fn = parse_str::<ItemFn>(r#"
                pub fn test<R: AsRef<str>, B>(mut s: String, v: &u32, a: &mut [i32], r: R) -> (u32, B, String, &str)
                            where B: Borrow<u32>
                    { }    
        "#).unwrap();
        let info = FixtureInfo::default().with_once();

        let out: FixtureOutput = parse2(render(item_fn.clone(), info)).unwrap();

        let signature = select_method(out.core_impl, "get").unwrap().sig;

        assert_eq!(signature.output, "-> &'static (u32, B, String, &str)".ast())
    }

    #[template]
    #[rstest(
        method => ["default", "get", "partial_1", "partial_2", "partial_3"])
    ]
    #[case::async_fn(true)]
    #[case::not_async_fn(false)]
    fn async_fixture_cases(#[case] is_async: bool, method: &str) {}

    #[apply(async_fixture_cases)]
    fn fixture_method_should_be_async_if_fixture_function_is_async(
        #[case] is_async: bool,
        method: &str,
    ) {
        let prefix = if is_async { "async" } else { "" };
        let (_, out) = parse_fixture(&format!(
            r#"
                    pub {} fn test(mut s: String, v: &u32, a: &mut [i32]) -> u32
                            where B: Borrow<u32>
                    {{ }}
                    "#,
            prefix
        ));

        let signature = select_method(out.core_impl, method).unwrap().sig;

        assert_eq!(is_async, signature.asyncness.is_some());
    }

    #[apply(async_fixture_cases)]
    fn fixture_method_should_use_await_if_fixture_function_is_async(
        #[case] is_async: bool,
        method: &str,
    ) {
        let prefix = if is_async { "async" } else { "" };
        let (_, out) = parse_fixture(&format!(
            r#"
                    pub {} fn test(mut s: String, v: &u32, a: &mut [i32]) -> u32
                    {{ }}
                    "#,
            prefix
        ));

        let body = select_method(out.core_impl, method).unwrap().block;
        let last_statement = body.stmts.last().unwrap();
        let is_await = match last_statement {
            syn::Stmt::Expr(syn::Expr::Await(_), _) => true,
            _ => false,
        };

        assert_eq!(is_async, is_await);
    }

    #[test]
    fn implement_a_default_method_with_input_cleaned_fixture_signature_and_no_args() {
        let (item_fn, out) = parse_fixture(
            r#"
                    pub fn test<R: AsRef<str>, B, F, H: Iterator<Item=u32>>(mut s: String, v: &u32, a: &mut [i32], r: R) -> (H, B, String, &str)
                        where F: ToString,
                        B: Borrow<u32>

                    { }
                    "#,
        );

        let default_decl = select_method(out.core_impl, "default").unwrap().sig;

        let expected = parse_str::<ItemFn>(
            r#"
                    pub fn default<B, H: Iterator<Item=u32>>() -> (H, B, String, &str)
                            where B: Borrow<u32>
                    { }
                    "#,
        )
        .unwrap();

        assert_eq!(expected.sig.generics, default_decl.generics);
        assert_eq!(item_fn.sig.output, default_decl.output);
        assert!(default_decl.inputs.is_empty());
    }

    #[test]
    fn use_default_return_type_if_any() {
        let item_fn = parse_str::<ItemFn>(
            r#"
                    pub fn test<R: AsRef<str>, B, F, H: Iterator<Item=u32>>() -> (H, B)
                            where F: ToString,
                            B: Borrow<u32>
                    { }
                    "#,
        )
        .unwrap();

        let tokens = render(
            item_fn.clone(),
            FixtureInfo {
                attributes: Attributes {
                    attributes: vec![Attribute::Type(
                        parse_str("default").unwrap(),
                        parse_str("(impl Iterator<Item=u32>, B)").unwrap(),
                    )],
                }
                .into(),
                ..Default::default()
            },
        );
        let out: FixtureOutput = parse2(tokens).unwrap();

        let expected = parse_str::<syn::ItemFn>(
            r#"
                    pub fn default<B>() -> (impl Iterator<Item=u32>, B)
                            where B: Borrow<u32>
                    { }
                    "#,
        )
        .unwrap();

        let default_decl = select_method(out.core_impl, "default").unwrap().sig;

        assert_eq!(expected.sig, default_decl);
    }

    #[test]
    fn implement_partial_methods() {
        let (item_fn, out) = parse_fixture(
            r#"
                    pub fn test(mut s: String, v: &u32, a: &mut [i32]) -> usize
                    { }
                    "#,
        );

        let partials = (1..=3)
            .map(|n| {
                select_method(out.core_impl.clone(), format!("partial_{}", n))
                    .unwrap()
                    .sig
            })
            .collect::<Vec<_>>();

        // All 3 methods found

        assert!(select_method(out.core_impl, "partial_4").is_none());

        let expected_1 = parse_str::<ItemFn>(
            r#"
                    pub fn partial_1(mut s: String) -> usize
                    { }
                    "#,
        )
        .unwrap();

        assert_eq!(expected_1.sig, partials[0]);
        for p in partials {
            assert_eq!(item_fn.sig.output, p.output);
        }
    }

    #[rstest]
    #[case::base("fn test<S: AsRef<str>, U: AsRef<u32>, F: ToString>(mut s: S, v: U) -> F {}",
        vec![
            "fn default<F: ToString>() -> F {}",
            "fn partial_1<S: AsRef<str>, F: ToString>(mut s: S) -> F {}",
            "fn partial_2<S: AsRef<str>, U: AsRef<u32>, F: ToString>(mut s: S, v: U) -> F {}",
        ]
    )]
    #[case::associated_type("fn test<T: IntoIterator>(mut i: T) where T::Item: Copy {}",
        vec![
            "fn default() {}",
            "fn partial_1<T: IntoIterator>(mut i: T) where T::Item: Copy {}",
        ]
    )]
    #[case::not_remove_const_generics("fn test<const N:usize>(v: [u32; N]) -> [i32; N] {}",
        vec![
            "fn default<const N:usize>() -> [i32; N] {}",
            "fn partial_1<const N:usize>(v: [u32; N]) -> [i32; N] {}",
        ]
    )]
    #[case::remove_const_generics("fn test<const N:usize>(a: i32, v: [u32; N]) {}",
        vec![
            "fn default() {}",
            "fn partial_1(a:i32) {}",
            "fn partial_2<const N:usize>(a:i32, v: [u32; N]) {}",
        ]
    )]

    fn clean_generics(#[case] code: &str, #[case] expected: Vec<&str>) {
        let (item_fn, out) = parse_fixture(code);
        let n_args = item_fn.sig.inputs.iter().count();

        let mut signatures = vec![select_method(out.core_impl.clone(), "default").unwrap().sig];
        signatures.extend((1..=n_args).map(|n| {
            select_method(out.core_impl.clone(), format!("partial_{}", n))
                .unwrap()
                .sig
        }));

        let expected = expected
            .into_iter()
            .map(parse_str::<ItemFn>)
            .map(|f| f.unwrap().sig)
            .collect::<Vec<_>>();

        assert_eq!(expected, signatures);
    }

    #[test]
    fn use_partial_return_type_if_any() {
        let item_fn = parse_str::<ItemFn>(
            r#"
                    pub fn test<R: AsRef<str>, B, F, H: Iterator<Item=u32>>(h: H, b: B) -> (H, B)
                            where F: ToString,
                            B: Borrow<u32>
                    { }
                     "#,
        )
        .unwrap();

        let tokens = render(
            item_fn.clone(),
            FixtureInfo {
                attributes: Attributes {
                    attributes: vec![Attribute::Type(
                        parse_str("partial_1").unwrap(),
                        parse_str("(H, impl Iterator<Item=u32>)").unwrap(),
                    )],
                }
                .into(),
                ..Default::default()
            },
        );
        let out: FixtureOutput = parse2(tokens).unwrap();

        let expected = parse_str::<syn::ItemFn>(
            r#"
                    pub fn partial_1<H: Iterator<Item=u32>>(h: H) -> (H, impl Iterator<Item=u32>)
                    { }
                    "#,
        )
        .unwrap();

        let partial = select_method(out.core_impl, "partial_1").unwrap();

        assert_eq!(expected.sig, partial.sig);
    }

    #[test]
    fn add_future_boilerplate_if_requested() {
        let item_fn: ItemFn =
            r#"async fn test(async_ref_u32: &u32, async_u32: u32,simple: u32) { }"#.ast();

        let mut arguments = ArgumentsInfo::default();
        arguments.add_future(pat("async_ref_u32"));
        arguments.add_future(pat("async_u32"));

        let tokens = render(
            item_fn.clone(),
            FixtureInfo {
                arguments,
                ..Default::default()
            },
        );
        let out: FixtureOutput = parse2(tokens).unwrap();

        let expected = parse_str::<syn::ItemFn>(
            r#"
                    async fn get<'_async_ref_u32>(
                        async_ref_u32: impl std::future::Future<Output = &'_async_ref_u32 u32>, 
                        async_u32: impl std::future::Future<Output = u32>, 
                        simple: u32
                    )
                    { }
                    "#,
        )
        .unwrap();

        let rendered = select_method(out.core_impl, "get").unwrap();

        assert_eq!(expected.sig, rendered.sig);
    }

    #[test]
    fn use_global_await() {
        let item_fn: ItemFn = r#"fn test(a: i32, b:i32, c:i32) {} "#.ast();
        let mut arguments: ArgumentsInfo = Default::default();
        arguments.set_global_await(true);
        arguments.add_future(pat("a"));
        arguments.add_future(pat("b"));

        let tokens = render(
            item_fn.clone(),
            FixtureInfo {
                arguments,
                ..Default::default()
            },
        );
        let out: FixtureOutput = parse2(tokens).unwrap();

        let code = out.orig.display_code();

        assert_in!(code, await_argument_code_string("a"));
        assert_in!(code, await_argument_code_string("b"));
        assert_not_in!(code, await_argument_code_string("c"));
    }

    #[test]
    fn use_selective_await() {
        let item_fn: ItemFn = r#"fn test(a: i32, b:i32, c:i32) {} "#.ast();
        let mut arguments: ArgumentsInfo = Default::default();
        arguments.set_future(pat("a"), FutureArg::Define);
        arguments.set_future(pat("b"), FutureArg::Await);

        let tokens = render(
            item_fn.clone(),
            FixtureInfo {
                arguments,
                ..Default::default()
            },
        );
        let out: FixtureOutput = parse2(tokens).unwrap();

        let code = out.orig.display_code();

        assert_not_in!(code, await_argument_code_string("a"));
        assert_in!(code, await_argument_code_string("b"));
        assert_not_in!(code, await_argument_code_string("c"));
    }
}
