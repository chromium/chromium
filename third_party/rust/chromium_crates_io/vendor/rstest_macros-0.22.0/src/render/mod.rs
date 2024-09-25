pub mod crate_resolver;
pub(crate) mod fixture;
mod test;
mod wrapper;

use std::collections::HashMap;

use syn::token::Async;

use proc_macro2::{Span, TokenStream};
use syn::{parse_quote, Attribute, Expr, FnArg, Ident, ItemFn, Pat, Path, ReturnType, Stmt};

use quote::{format_ident, quote};

use crate::refident::MaybePat;
use crate::utils::{attr_ends_with, sanitize_ident};
use crate::{
    parse::{
        rstest::{RsTestAttributes, RsTestInfo},
        testcase::TestCase,
        vlist::ValueList,
    },
    utils::attr_is,
};
use crate::{
    refident::MaybeIdent,
    resolver::{self, Resolver},
};
use wrapper::WrapByModule;

pub(crate) use fixture::render as fixture;

use self::apply_arguments::ApplyArguments;
use self::crate_resolver::crate_name;
pub(crate) mod apply_arguments;
pub(crate) mod inject;

pub(crate) fn single(mut test: ItemFn, mut info: RsTestInfo) -> TokenStream {
    test.apply_arguments(&mut info.arguments, &mut ());

    let resolver = resolver::fixtures::get(&info.arguments, info.data.fixtures());

    let args = test.sig.inputs.iter().cloned().collect::<Vec<_>>();
    let attrs = std::mem::take(&mut test.attrs);
    let asyncness = test.sig.asyncness;

    single_test_case(
        &test.sig.ident,
        &test.sig.ident,
        &args,
        &attrs,
        &test.sig.output,
        asyncness,
        Some(&test),
        resolver,
        &info,
        &test.sig.generics,
    )
}

pub(crate) fn parametrize(mut test: ItemFn, info: RsTestInfo) -> TokenStream {
    let mut arguments_info = info.arguments.clone();
    test.apply_arguments(&mut arguments_info, &mut ());

    let resolver_fixtures = resolver::fixtures::get(&info.arguments, info.data.fixtures());

    let rendered_cases = cases_data(&info, test.sig.ident.span())
        .map(|(name, attrs, resolver)| {
            TestCaseRender::new(name, attrs, (resolver, &resolver_fixtures))
        })
        .map(|case| case.render(&test, &info))
        .collect();

    test_group(test, rendered_cases)
}

impl ValueList {
    fn render(
        &self,
        test: &ItemFn,
        resolver: &dyn Resolver,
        attrs: &[syn::Attribute],
        info: &RsTestInfo,
    ) -> TokenStream {
        let span = test.sig.ident.span();
        let test_cases = self
            .argument_data(resolver, info)
            .map(|(name, r)| TestCaseRender::new(Ident::new(&name, span), attrs, r))
            .map(|test_case| test_case.render(test, info));

        quote! { #(#test_cases)* }
    }

    fn argument_data<'a>(
        &'a self,
        resolver: &'a dyn Resolver,
        info: &'a RsTestInfo,
    ) -> impl Iterator<Item = (String, Box<(&'a dyn Resolver, (Pat, Expr))>)> + 'a {
        let max_len = self.values.len();
        self.values.iter().enumerate().map(move |(index, value)| {
            let description = sanitize_ident(&value.description());
            let arg = info.arguments.inner_pat(&self.arg);

            let arg_name = arg
                .maybe_ident()
                .expect("BUG: Here all arguments should be PatIdent types")
                .to_string();

            let name = format!(
                "{}_{:0len$}_{description:.64}",
                arg_name,
                index + 1,
                len = max_len.display_len()
            );
            let resolver_this = (arg.clone(), value.expr.clone());
            (name, Box::new((resolver, resolver_this)))
        })
    }
}

fn _matrix_recursive<'a>(
    test: &ItemFn,
    list_values: &'a [&'a ValueList],
    resolver: &dyn Resolver,
    attrs: &'a [syn::Attribute],
    info: &RsTestInfo,
) -> TokenStream {
    if list_values.is_empty() {
        return Default::default();
    }
    let vlist = list_values[0];
    let list_values = &list_values[1..];

    if list_values.is_empty() {
        let mut attrs = attrs.to_vec();
        attrs.push(parse_quote!(
            #[allow(non_snake_case)]
        ));
        vlist.render(test, resolver, &attrs, info)
    } else {
        let span = test.sig.ident.span();
        let modules = vlist
            .argument_data(resolver, info)
            .map(move |(name, resolver)| {
                _matrix_recursive(test, list_values, &resolver, attrs, info)
                    .wrap_by_mod(&Ident::new(&name, span))
            });

        quote! { #(
            #[allow(non_snake_case)]
            #modules
        )* }
    }
}

pub(crate) fn matrix(mut test: ItemFn, mut info: RsTestInfo) -> TokenStream {
    test.apply_arguments(&mut info.arguments, &mut ());
    let span = test.sig.ident.span();

    let cases = cases_data(&info, span).collect::<Vec<_>>();

    let resolver = resolver::fixtures::get(&info.arguments, info.data.fixtures());
    let rendered_cases = if cases.is_empty() {
        let list_values = info.data.list_values().collect::<Vec<_>>();
        _matrix_recursive(&test, &list_values, &resolver, &[], &info)
    } else {
        cases
            .into_iter()
            .map(|(case_name, attrs, case_resolver)| {
                let list_values = info.data.list_values().collect::<Vec<_>>();
                _matrix_recursive(
                    &test,
                    &list_values,
                    &(case_resolver, &resolver),
                    attrs,
                    &info,
                )
                .wrap_by_mod(&case_name)
            })
            .collect()
    };

    test_group(test, rendered_cases)
}

fn resolve_default_test_attr(is_async: bool) -> TokenStream {
    if is_async {
        quote! { #[async_std::test] }
    } else {
        quote! { #[test] }
    }
}

fn render_exec_call(fn_path: Path, args: &[Expr], is_async: bool) -> TokenStream {
    if is_async {
        quote! {#fn_path(#(#args),*).await}
    } else {
        quote! {#fn_path(#(#args),*)}
    }
}

fn render_test_call(
    fn_path: Path,
    args: &[Expr],
    timeout: Option<Expr>,
    is_async: bool,
) -> TokenStream {
    let timeout = timeout.map(|x| quote! {#x}).or_else(|| {
        std::env::var("RSTEST_TIMEOUT")
            .ok()
            .map(|to| quote! { std::time::Duration::from_secs( (#to).parse().unwrap()) })
    });
    let rstest_path = crate_name();
    match (timeout, is_async) {
        (Some(to_expr), true) => quote! {
            use #rstest_path::timeout::*;
            execute_with_timeout_async(move || #fn_path(#(#args),*), #to_expr).await
        },
        (Some(to_expr), false) => quote! {
            use #rstest_path::timeout::*;
            execute_with_timeout_sync(move || #fn_path(#(#args),*), #to_expr)
        },
        _ => render_exec_call(fn_path, args, is_async),
    }
}

fn generics_types_ident(generics: &syn::Generics) -> impl Iterator<Item = &'_ Ident> {
    generics.type_params().map(|tp| &tp.ident)
}

/// Render a single test case:
///
/// * `name` - Test case name
/// * `testfn_name` - The name of test function to call
/// * `args` - The arguments of the test function
/// * `attrs` - The expected test attributes
/// * `output` - The expected test return type
/// * `asyncness` - The `async` fn token
/// * `test_impl` - If you want embed test function (should be the one called by `testfn_name`)
/// * `resolver` - The resolver used to resolve injected values
/// * `info` - `RsTestInfo` that's expose the requested test behavior
/// * `generic_types` - The generic types used in signature
///
// Ok I need some refactoring here but now that not a real issue
#[allow(clippy::too_many_arguments)]
fn single_test_case(
    name: &Ident,
    testfn_name: &Ident,
    args: &[FnArg],
    attrs: &[Attribute],
    output: &ReturnType,
    asyncness: Option<Async>,
    test_impl: Option<&ItemFn>,
    resolver: impl Resolver,
    info: &RsTestInfo,
    generics: &syn::Generics,
) -> TokenStream {
    let (attrs, trace_me): (Vec<_>, Vec<_>) =
        attrs.iter().cloned().partition(|a| !attr_is(a, "trace"));
    let mut attributes = info.attributes.clone();
    if !trace_me.is_empty() {
        attributes.add_trace(format_ident!("trace"));
    }

    let generics_types = generics_types_ident(generics).cloned().collect::<Vec<_>>();
    let args = info
        .arguments
        .replace_fn_args_with_related_inner_pat(args.iter().cloned())
        .collect::<Vec<_>>();

    let (injectable_args, ignored_args): (Vec<_>, Vec<_>) =
        args.iter().partition(|arg| match arg.maybe_pat() {
            Some(pat) => !info.arguments.is_ignore(pat),
            None => true,
        });

    let inject = inject::resolve_arguments(injectable_args.into_iter(), &resolver, &generics_types);

    let args = args
        .iter()
        .filter_map(MaybePat::maybe_pat)
        .cloned()
        .collect::<Vec<_>>();
    let trace_args = trace_arguments(args.iter(), &attributes);

    let is_async = asyncness.is_some();
    let (attrs, timeouts): (Vec<_>, Vec<_>) =
        attrs.iter().cloned().partition(|a| !attr_is(a, "timeout"));

    let timeout = timeouts
        .into_iter()
        .last()
        .map(|attribute| attribute.parse_args::<Expr>().unwrap());

    // If no injected attribute provided use the default one
    let test_attr = if attrs
        .iter()
        .any(|a| attr_ends_with(a, &parse_quote! {test}))
    {
        None
    } else {
        Some(resolve_default_test_attr(is_async))
    };

    let args = args
        .iter()
        .map(|arg| (arg, info.arguments.is_by_refs(arg)))
        .filter_map(|(a, by_refs)| a.maybe_ident().map(|id| (id, by_refs)))
        .map(|(arg, by_ref)| {
            if by_ref {
                parse_quote! { &#arg }
            } else {
                parse_quote! { #arg }
            }
        })
        .collect::<Vec<_>>();

    let execute = render_test_call(testfn_name.clone().into(), &args, timeout, is_async);
    let lifetimes = generics.lifetimes();

    quote! {
        #test_attr
        #(#attrs)*
        #asyncness fn #name<#(#lifetimes,)*>(#(#ignored_args,)*) #output {
            #test_impl
            #inject
            #trace_args
            #execute
        }
    }
}

fn trace_arguments<'a>(
    args: impl Iterator<Item = &'a Pat>,
    attributes: &RsTestAttributes,
) -> Option<TokenStream> {
    let mut statements = args
        .filter(|&arg| attributes.trace_me(arg))
        .map(|arg| {
            let s: Stmt = parse_quote! {
                println!("{} = {:?}", stringify!(#arg), #arg);
            };
            s
        })
        .peekable();
    if statements.peek().is_some() {
        Some(quote! {
            println!("{:-^40}", " TEST ARGUMENTS ");
            #(#statements)*
            println!("{:-^40}", " TEST START ");
        })
    } else {
        None
    }
}

struct TestCaseRender<'a> {
    name: Ident,
    attrs: &'a [syn::Attribute],
    resolver: Box<dyn Resolver + 'a>,
}

impl<'a> TestCaseRender<'a> {
    pub fn new<R: Resolver + 'a>(name: Ident, attrs: &'a [syn::Attribute], resolver: R) -> Self {
        TestCaseRender {
            name,
            attrs,
            resolver: Box::new(resolver),
        }
    }

    fn render(self, testfn: &ItemFn, info: &RsTestInfo) -> TokenStream {
        let args = testfn.sig.inputs.iter().cloned().collect::<Vec<_>>();
        let mut attrs = testfn.attrs.clone();
        attrs.extend(self.attrs.iter().cloned());
        let asyncness = testfn.sig.asyncness;

        single_test_case(
            &self.name,
            &testfn.sig.ident,
            &args,
            &attrs,
            &testfn.sig.output,
            asyncness,
            None,
            self.resolver,
            info,
            &testfn.sig.generics,
        )
    }
}

fn test_group(mut test: ItemFn, rendered_cases: TokenStream) -> TokenStream {
    let fname = &test.sig.ident;
    test.attrs = vec![];

    quote! {
        #[cfg(test)]
        #test

        #[cfg(test)]
        mod #fname {
            use super::*;

            #rendered_cases
        }
    }
}

trait DisplayLen {
    fn display_len(&self) -> usize;
}

impl<D: std::fmt::Display> DisplayLen for D {
    fn display_len(&self) -> usize {
        format!("{self}").len()
    }
}

fn format_case_name(case: &TestCase, index: usize, display_len: usize) -> String {
    let description = case
        .description
        .as_ref()
        .map(|d| format!("_{d}"))
        .unwrap_or_default();
    format!("case_{index:0display_len$}{description}")
}

fn cases_data(
    info: &RsTestInfo,
    name_span: Span,
) -> impl Iterator<Item = (Ident, &[syn::Attribute], HashMap<Pat, &syn::Expr>)> {
    let display_len = info.data.cases().count().display_len();
    info.data.cases().enumerate().map({
        move |(n, case)| {
            let resolver_case = info
                .data
                .case_args()
                .cloned()
                .map(|arg| info.arguments.inner_pat(&arg).clone())
                .zip(case.args.iter())
                .collect::<HashMap<_, _>>();
            (
                Ident::new(&format_case_name(case, n + 1, display_len), name_span),
                case.attrs.as_slice(),
                resolver_case,
            )
        }
    })
}
