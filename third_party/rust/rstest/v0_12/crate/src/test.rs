#![macro_use]

/// Unit testing utility module. Collect a bunch of functions&macro and impls to simplify unit
/// testing bolilerplate.
///
use std::borrow::Cow;
use std::iter::FromIterator;

pub(crate) use mytest::{fixture, rstest};
pub(crate) use pretty_assertions::assert_eq;
use proc_macro2::TokenTree;
use quote::quote;
use syn::{parse::Parse, parse2, parse_str, Error, Expr, Ident, ItemFn, Stmt};

use super::*;
use crate::parse::{
    fixture::{FixtureData, FixtureItem},
    rstest::{RsTestData, RsTestItem},
    testcase::TestCase,
    vlist::ValueList,
    Attribute, Fixture, Positional,
};
use crate::resolver::Resolver;
use crate::utils::fn_args_idents;
use parse::fixture::ArgumentValue;

macro_rules! to_args {
    ($e:expr) => {{
        $e.iter()
            .map(|s| s as &dyn AsRef<str>)
            .map(expr)
            .collect::<Vec<_>>()
    }};
}

macro_rules! to_exprs {
    ($e:expr) => {
        $e.iter().map(|s| expr(s)).collect::<Vec<_>>()
    };
}

macro_rules! to_strs {
    ($e:expr) => {
        $e.iter().map(ToString::to_string).collect::<Vec<_>>()
    };
}

macro_rules! to_idents {
    ($e:expr) => {
        $e.iter().map(|s| ident(s)).collect::<Vec<_>>()
    };
}

struct Outer<T>(T);
impl<T: Parse> Parse for Outer<T> {
    fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
        let outer: Ident = input.parse()?;
        if outer == "outer" {
            let content;
            let _ = syn::parenthesized!(content in input);
            content.parse().map(Outer)
        } else {
            Err(Error::new(outer.span(), "Expected 'outer'"))
        }
    }
}

pub(crate) fn parse_meta<T: syn::parse::Parse, S: AsRef<str>>(test_case: S) -> T {
    let to_parse = format!(
        r#"
        #[outer({})]
        fn to_parse() {{}}
        "#,
        test_case.as_ref()
    );

    let item_fn = parse_str::<ItemFn>(&to_parse).expect(&format!("Cannot parse '{}'", to_parse));

    let tokens = quote!(
        #item_fn
    );

    let tt = tokens.into_iter().skip(1).next().unwrap();

    if let TokenTree::Group(g) = tt {
        let ts = g.stream();
        parse2::<Outer<T>>(ts).unwrap().0
    } else {
        panic!("Cannot find group in {:#?}", tt)
    }
}

pub(crate) trait ToAst {
    fn ast<T: Parse>(self) -> T;
}

impl ToAst for &str {
    fn ast<T: Parse>(self) -> T {
        parse_str(self).unwrap()
    }
}

impl ToAst for String {
    fn ast<T: Parse>(self) -> T {
        parse_str(&self).unwrap()
    }
}

impl ToAst for proc_macro2::TokenStream {
    fn ast<T: Parse>(self) -> T {
        parse2(self).unwrap()
    }
}

pub(crate) fn ident(s: impl AsRef<str>) -> syn::Ident {
    s.as_ref().ast()
}

pub(crate) fn expr(s: impl AsRef<str>) -> syn::Expr {
    s.as_ref().ast()
}

pub(crate) fn attrs(s: impl AsRef<str>) -> Vec<syn::Attribute> {
    parse_str::<ItemFn>(&format!(
        r#"{}
           fn _no_name_() {{}}   
        "#,
        s.as_ref()
    ))
    .unwrap()
    .attrs
}

pub(crate) fn fixture(name: impl AsRef<str>, args: &[&str]) -> Fixture {
    Fixture::new(ident(name), None, Positional(to_exprs!(args)))
}

pub(crate) fn arg_value(name: impl AsRef<str>, value: impl AsRef<str>) -> ArgumentValue {
    ArgumentValue::new(ident(name), expr(value))
}

pub(crate) fn values_list<S: AsRef<str>>(arg: &str, values: &[S]) -> ValueList {
    ValueList {
        arg: ident(arg),
        values: values.into_iter().map(|s| expr(s)).collect(),
    }
}

pub(crate) fn first_arg_ident(ast: &ItemFn) -> &Ident {
    fn_args_idents(&ast).next().unwrap()
}

pub(crate) fn extract_inner_functions(block: &syn::Block) -> impl Iterator<Item = &syn::ItemFn> {
    block.stmts.iter().filter_map(|s| match s {
        syn::Stmt::Item(syn::Item::Fn(f)) => Some(f),
        _ => None,
    })
}

pub(crate) fn literal_expressions_str() -> Vec<&'static str> {
    vec![
        "42",
        "42isize",
        "1.0",
        "-1",
        "-1.0",
        "true",
        "1_000_000u64",
        "0b10100101u8",
        r#""42""#,
        "b'H'",
    ]
}

pub(crate) trait ExtractArgs {
    fn args(&self) -> Vec<Expr>;
}

impl ExtractArgs for TestCase {
    fn args(&self) -> Vec<Expr> {
        self.args.iter().cloned().collect()
    }
}

impl ExtractArgs for ValueList {
    fn args(&self) -> Vec<Expr> {
        self.values.iter().cloned().collect()
    }
}

impl Attribute {
    pub fn attr<S: AsRef<str>>(s: S) -> Self {
        Attribute::Attr(ident(s))
    }

    pub fn tagged<SI: AsRef<str>, SA: AsRef<str>>(tag: SI, attrs: Vec<SA>) -> Self {
        Attribute::Tagged(ident(tag), attrs.into_iter().map(|a| ident(a)).collect())
    }

    pub fn typed<S: AsRef<str>, T: AsRef<str>>(tag: S, inner: T) -> Self {
        Attribute::Type(ident(tag), parse_str(inner.as_ref()).unwrap())
    }
}

impl RsTestInfo {
    pub fn push_case(&mut self, case: TestCase) {
        self.data.items.push(RsTestItem::TestCase(case));
    }

    pub fn extend(&mut self, cases: impl Iterator<Item = TestCase>) {
        self.data.items.extend(cases.map(RsTestItem::TestCase));
    }
}

impl Fixture {
    pub fn with_resolve(mut self, resolve_ident: &str) -> Self {
        self.resolve = Some(ident(resolve_ident));
        self
    }
}

impl TestCase {
    pub fn with_description(mut self, description: &str) -> Self {
        self.description = Some(ident(description));
        self
    }

    pub fn with_attrs(mut self, attrs: Vec<syn::Attribute>) -> Self {
        self.attrs = attrs;
        self
    }
}

impl<A: AsRef<str>> FromIterator<A> for TestCase {
    fn from_iter<T: IntoIterator<Item = A>>(iter: T) -> Self {
        TestCase {
            args: iter.into_iter().map(expr).collect(),
            attrs: Default::default(),
            description: None,
        }
    }
}

impl<'a> From<&'a str> for TestCase {
    fn from(argument: &'a str) -> Self {
        std::iter::once(argument).collect()
    }
}

impl From<Vec<RsTestItem>> for RsTestData {
    fn from(items: Vec<RsTestItem>) -> Self {
        Self { items }
    }
}

impl From<RsTestData> for RsTestInfo {
    fn from(data: RsTestData) -> Self {
        Self {
            data,
            attributes: Default::default(),
        }
    }
}

impl From<Vec<Expr>> for Positional {
    fn from(data: Vec<Expr>) -> Self {
        Positional(data)
    }
}

impl From<Vec<FixtureItem>> for FixtureData {
    fn from(fixtures: Vec<FixtureItem>) -> Self {
        Self { items: fixtures }
    }
}

pub(crate) struct EmptyResolver;

impl<'a> Resolver for EmptyResolver {
    fn resolve(&self, _ident: &Ident) -> Option<Cow<Expr>> {
        None
    }
}

pub(crate) trait IsAwait {
    fn is_await(&self) -> bool;
}

impl IsAwait for Stmt {
    fn is_await(&self) -> bool {
        match self {
            Stmt::Expr(Expr::Await(_)) => true,
            _ => false,
        }
    }
}

pub(crate) trait DisplayCode {
    fn display_code(&self) -> String;
}

impl<T: ToTokens> DisplayCode for T {
    fn display_code(&self) -> String {
        self.to_token_stream().to_string()
    }
}

impl crate::parse::fixture::FixtureInfo {
    pub(crate) fn with_once(mut self) -> Self {
        self.attributes = self.attributes.with_once();
        self
    }
}

impl crate::parse::fixture::FixtureModifiers {
    pub(crate) fn with_once(mut self) -> Self {
        self.append(Attribute::attr("once"));
        self
    }
}
