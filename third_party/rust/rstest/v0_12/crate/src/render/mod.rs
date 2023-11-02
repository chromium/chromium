pub(crate) mod fixture;
mod test;
mod wrapper;

use std::collections::HashMap;
use syn::token::Async;

use proc_macro2::{Span, TokenStream};
use syn::{parse_quote, Attribute, Expr, FnArg, Ident, ItemFn, Path, ReturnType, Stmt};

use quote::{format_ident, quote};

use crate::utils::attr_ends_with;
use crate::{
    parse::{
        rstest::{RsTestAttributes, RsTestData, RsTestInfo},
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
pub(crate) mod inject;

pub(crate) fn single(mut test: ItemFn, info: RsTestInfo) -> TokenStream {
    let resolver = resolver::fixtures::get(info.data.fixtures());
    let args = test.sig.inputs.iter().cloned().collect::<Vec<_>>();
    let attrs = std::mem::take(&mut test.attrs);
    let asyncness = test.sig.asyncness;
    let generic_types = test
        .sig
        .generics
        .type_params()
        .map(|tp| &tp.ident)
        .cloned()
        .collect::<Vec<_>>();

    single_test_case(
        &test.sig.ident,
        &test.sig.ident,
        &args,
        &attrs,
        &test.sig.output,
        asyncness,
        Some(&test),
        resolver,
        &info.attributes,
        &generic_types,
    )
}

pub(crate) fn parametrize(test: ItemFn, info: RsTestInfo) -> TokenStream {
    let RsTestInfo { data, attributes } = info;
    let resolver_fixtures = resolver::fixtures::get(data.fixtures());

    let rendered_cases = cases_data(&data, test.sig.ident.span())
        .map(|(name, attrs, resolver)| {
            TestCaseRender::new(name, attrs, (resolver, &resolver_fixtures))
        })
        .map(|case| case.render(&test, &attributes))
        .collect();

    test_group(test, rendered_cases)
}

impl ValueList {
    fn render(
        &self,
        test: &ItemFn,
        resolver: &dyn Resolver,
        attrs: &[syn::Attribute],
        attributes: &RsTestAttributes,
    ) -> TokenStream {
        let span = test.sig.ident.span();
        let test_cases = self
            .argument_data(resolver)
            .map(|(name, r)| TestCaseRender::new(Ident::new(&name, span), attrs, r))
            .map(|test_case| test_case.render(test, attributes));

        quote! { #(#test_cases)* }
    }

    fn argument_data<'a>(
        &'a self,
        resolver: &'a dyn Resolver,
    ) -> impl Iterator<Item = (String, Box<(&'a dyn Resolver, (String, Expr))>)> + 'a {
        let max_len = self.values.len();
        self.values.iter().enumerate().map(move |(index, expr)| {
            let name = format!(
                "{}_{:0len$}",
                self.arg,
                index + 1,
                len = max_len.display_len()
            );
            let resolver_this = (self.arg.to_string(), expr.clone());
            (name, Box::new((resolver, resolver_this)))
        })
    }
}

fn _matrix_recursive<'a>(
    test: &ItemFn,
    list_values: &'a [&'a ValueList],
    resolver: &dyn Resolver,
    attrs: &'a [syn::Attribute],
    attributes: &RsTestAttributes,
) -> TokenStream {
    if list_values.is_empty() {
        return Default::default();
    }
    let vlist = list_values[0];
    let list_values = &list_values[1..];

    if list_values.is_empty() {
        vlist.render(test, resolver, attrs, attributes)
    } else {
        let span = test.sig.ident.span();
        let modules = vlist.argument_data(resolver).map(move |(name, resolver)| {
            _matrix_recursive(test, list_values, &resolver, attrs, attributes)
                .wrap_by_mod(&Ident::new(&name, span))
        });

        quote! { #(#modules)* }
    }
}

pub(crate) fn matrix(test: ItemFn, info: RsTestInfo) -> TokenStream {
    let RsTestInfo {
        data, attributes, ..
    } = info;
    let span = test.sig.ident.span();

    let cases = cases_data(&data, span).collect::<Vec<_>>();

    let resolver = resolver::fixtures::get(data.fixtures());
    let rendered_cases = if cases.is_empty() {
        let list_values = data.list_values().collect::<Vec<_>>();
        _matrix_recursive(&test, &list_values, &resolver, &[], &attributes)
    } else {
        cases
            .into_iter()
            .map(|(case_name, attrs, case_resolver)| {
                let list_values = data.list_values().collect::<Vec<_>>();
                _matrix_recursive(
                    &test,
                    &list_values,
                    &(case_resolver, &resolver),
                    attrs,
                    &attributes,
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

fn render_exec_call(fn_path: Path, args: &[Ident], is_async: bool) -> TokenStream {
    if is_async {
        quote! {#fn_path(#(#args),*).await}
    } else {
        quote! {#fn_path(#(#args),*)}
    }
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
/// * `attributes` - Test attributes to select test behaviour
/// * `generic_types` - The genrics type used in signature
///
// Ok I need some refactoring here but now that not a real issue
#[allow(clippy::too_many_arguments)]
fn single_test_case<'a>(
    name: &Ident,
    testfn_name: &Ident,
    args: &[FnArg],
    attrs: &[Attribute],
    output: &ReturnType,
    asyncness: Option<Async>,
    test_impl: Option<&ItemFn>,
    resolver: impl Resolver,
    attributes: &'a RsTestAttributes,
    generic_types: &[Ident],
) -> TokenStream {
    let (attrs, trace_me): (Vec<_>, Vec<_>) =
        attrs.iter().cloned().partition(|a| !attr_is(a, "trace"));
    let mut attributes = attributes.clone();
    if !trace_me.is_empty() {
        attributes.add_trace(format_ident!("trace"));
    }
    let inject = inject::resolve_aruments(args.iter(), &resolver, generic_types);
    let args = args
        .iter()
        .filter_map(MaybeIdent::maybe_ident)
        .cloned()
        .collect::<Vec<_>>();
    let trace_args = trace_arguments(args.iter(), &attributes);

    let is_async = asyncness.is_some();
    // If no injected attribut provided use the default one
    let test_attr = if attrs
        .iter()
        .any(|a| attr_ends_with(a, &parse_quote! {test}))
    {
        None
    } else {
        Some(resolve_default_test_attr(is_async))
    };
    let execute = render_exec_call(testfn_name.clone().into(), &args, is_async);

    quote! {
        #test_attr
        #(#attrs)*
        #asyncness fn #name() #output {
            #test_impl
            #inject
            #trace_args
            println!("{:-^40}", " TEST START ");
            #execute
        }
    }
}

fn trace_arguments<'a>(
    args: impl Iterator<Item = &'a Ident>,
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

    fn render(self, testfn: &ItemFn, attributes: &RsTestAttributes) -> TokenStream {
        let args = testfn.sig.inputs.iter().cloned().collect::<Vec<_>>();
        let mut attrs = testfn.attrs.clone();
        attrs.extend(self.attrs.iter().cloned());
        let asyncness = testfn.sig.asyncness;
        let generic_types = testfn
            .sig
            .generics
            .type_params()
            .map(|tp| &tp.ident)
            .cloned()
            .collect::<Vec<_>>();

        single_test_case(
            &self.name,
            &testfn.sig.ident,
            &args,
            &attrs,
            &testfn.sig.output,
            asyncness,
            None,
            self.resolver,
            attributes,
            &generic_types,
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
        format!("{}", self).len()
    }
}

fn format_case_name(case: &TestCase, index: usize, display_len: usize) -> String {
    let description = case
        .description
        .as_ref()
        .map(|d| format!("_{}", d))
        .unwrap_or_default();
    format!(
        "case_{:0len$}{d}",
        index,
        len = display_len,
        d = description
    )
}

fn cases_data(
    data: &RsTestData,
    name_span: Span,
) -> impl Iterator<Item = (Ident, &[syn::Attribute], HashMap<String, &syn::Expr>)> {
    let display_len = data.cases().count().display_len();
    data.cases().enumerate().map({
        move |(n, case)| {
            let resolver_case = data
                .case_args()
                .map(|a| a.to_string())
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
