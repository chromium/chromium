use proc_macro2::TokenStream;
use syn::{
    parse::{Parse, ParseStream},
    parse_quote,
    punctuated::Punctuated,
    token::{self, Async, Paren},
    visit_mut::VisitMut,
    FnArg, Ident, ItemFn, Pat, Token,
};

use crate::{
    error::ErrorsVec,
    parse::just_once::{AttrBuilder, JustOnceFnAttributeExtractor, Validator},
    refident::{IntoPat, MaybeIdent, MaybePat},
    utils::{attr_is, attr_starts_with},
};
use fixture::{ArgumentValue, FixtureModifiers, FixturesFunctionExtractor};
use quote::ToTokens;
use testcase::TestCase;

use self::{
    expressions::Expressions, just_once::JustOnceFnArgAttributeExtractor, vlist::ValueList,
};

// To use the macros this should be the first one module
#[macro_use]
pub(crate) mod macros;

pub(crate) mod arguments;
pub(crate) mod by_ref;
pub(crate) mod expressions;
pub(crate) mod fixture;
pub(crate) mod future;
pub(crate) mod ignore;
pub(crate) mod just_once;
pub(crate) mod rstest;
pub(crate) mod testcase;
pub(crate) mod vlist;

pub(crate) trait ExtendWithFunctionAttrs {
    fn extend_with_function_attrs(
        &mut self,
        item_fn: &mut ItemFn,
    ) -> std::result::Result<(), ErrorsVec>;
}

#[derive(Default, Debug, PartialEq, Clone)]
pub(crate) struct Attributes {
    pub(crate) attributes: Vec<Attribute>,
}

impl Parse for Attributes {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let vars = Punctuated::<Attribute, Token![::]>::parse_terminated(input)?;
        Ok(Attributes {
            attributes: vars.into_iter().collect(),
        })
    }
}

#[derive(Debug, PartialEq, Clone)]
pub(crate) enum Attribute {
    Attr(Ident),
    Tagged(Ident, Vec<Pat>),
    Type(Ident, Box<syn::Type>),
}

impl Parse for Attribute {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        if input.peek2(Token![<]) {
            let tag = input.parse()?;
            let _open = input.parse::<Token![<]>()?;
            let inner = input.parse()?;
            let _close = input.parse::<Token![>]>()?;
            Ok(Attribute::Type(tag, inner))
        } else if input.peek2(Token![::]) {
            let inner = input.parse()?;
            Ok(Attribute::Attr(inner))
        } else if input.peek2(token::Paren) {
            let tag = input.parse()?;
            let content;
            let _ = syn::parenthesized!(content in input);
            let args = Punctuated::<Ident, Token![,]>::parse_terminated(&content)?
                .into_iter()
                .map(IntoPat::into_pat)
                .collect();

            Ok(Attribute::Tagged(tag, args))
        } else {
            Ok(Attribute::Attr(input.parse()?))
        }
    }
}

fn parse_vector_trailing_till_double_comma<T, P>(input: ParseStream) -> syn::Result<Vec<T>>
where
    T: Parse,
    P: syn::token::Token + Parse,
{
    Ok(
        Punctuated::<Option<T>, P>::parse_separated_nonempty_with(input, |input_tokens| {
            if input_tokens.is_empty() || input_tokens.peek(Token![::]) {
                Ok(None)
            } else {
                T::parse(input_tokens).map(Some)
            }
        })?
        .into_iter()
        .flatten()
        .collect(),
    )
}

#[allow(dead_code)]
pub(crate) fn drain_stream(input: ParseStream) {
    // JUST TO SKIP ALL
    let _ = input.step(|cursor| {
        let mut rest = *cursor;
        while let Some((_, next)) = rest.token_tree() {
            rest = next
        }
        Ok(((), rest))
    });
}

#[derive(PartialEq, Debug, Clone, Default)]
pub(crate) struct Positional(pub(crate) Vec<syn::Expr>);

impl Parse for Positional {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        Ok(Self(
            Punctuated::<syn::Expr, Token![,]>::parse_terminated(input)?
                .into_iter()
                .collect(),
        ))
    }
}

#[derive(PartialEq, Debug, Clone)]
pub(crate) struct Fixture {
    pub(crate) arg: Pat,
    pub(crate) resolve: syn::Path,
    pub(crate) positional: Positional,
}

impl Fixture {
    pub(crate) fn new(arg: Pat, resolve: syn::Path, positional: Positional) -> Self {
        Self {
            arg,
            resolve,
            positional,
        }
    }
}

impl Parse for Fixture {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let resolve: syn::Path = input.parse()?;
        if input.peek(Paren) || input.peek(Token![as]) {
            let positional = if input.peek(Paren) {
                let content;
                let _ = syn::parenthesized!(content in input);
                content.parse()?
            } else {
                Default::default()
            };

            if input.peek(Token![as]) {
                let _: Token![as] = input.parse()?;
                let ident: Ident = input.parse()?;
                Ok(Self::new(ident.into_pat(), resolve, positional))
            } else {
                let name = resolve.get_ident().ok_or_else(|| {
                    syn::Error::new_spanned(
                        resolve.to_token_stream(),
                        "Should be an ident".to_string(),
                    )
                })?;
                Ok(Self::new(
                    name.clone().into_pat(),
                    name.clone().into(),
                    positional,
                ))
            }
        } else {
            Err(syn::Error::new(
                input.span(),
                "fixture need arguments or 'as new_name' format",
            ))
        }
    }
}

impl ToTokens for Fixture {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        self.arg.to_tokens(tokens)
    }
}

pub(crate) fn extract_fixtures(item_fn: &mut ItemFn) -> Result<Vec<Fixture>, ErrorsVec> {
    let mut fixtures_extractor = FixturesFunctionExtractor::default();
    fixtures_extractor.visit_item_fn_mut(item_fn);

    if fixtures_extractor.1.is_empty() {
        Ok(fixtures_extractor.0)
    } else {
        Err(fixtures_extractor.1.into())
    }
}
pub(crate) fn extract_defaults(item_fn: &mut ItemFn) -> Result<Vec<ArgumentValue>, ErrorsVec> {
    struct DefaultBuilder;
    impl AttrBuilder<Pat> for DefaultBuilder {
        type Out = ArgumentValue;

        fn build(attr: syn::Attribute, name: &Pat) -> syn::Result<Self::Out> {
            attr.parse_args::<syn::Expr>()
                .map(|e| ArgumentValue::new(name.clone(), e))
        }
    }
    impl Validator<syn::FnArg> for DefaultBuilder {}

    let mut extractor = JustOnceFnArgAttributeExtractor::<DefaultBuilder>::new("default");
    extractor.visit_item_fn_mut(item_fn);

    extractor.take()
}

pub(crate) fn extract_default_return_type(
    item_fn: &mut ItemFn,
) -> Result<Option<syn::Type>, ErrorsVec> {
    struct DefaultTypeBuilder;
    impl AttrBuilder<ItemFn> for DefaultTypeBuilder {
        type Out = syn::Type;

        fn build(attr: syn::Attribute, _extra: &ItemFn) -> syn::Result<Self::Out> {
            attr.parse_args::<syn::Type>()
        }
    }
    impl Validator<syn::ItemFn> for DefaultTypeBuilder {}

    let mut extractor =
        JustOnceFnAttributeExtractor::<DefaultTypeBuilder>::new(FixtureModifiers::DEFAULT_RET_ATTR);

    extractor.visit_item_fn_mut(item_fn);
    extractor.take()
}

pub(crate) fn extract_partials_return_type(
    item_fn: &mut ItemFn,
) -> Result<Vec<(usize, syn::Type)>, ErrorsVec> {
    let mut partials_type_extractor = PartialsTypeFunctionExtractor::default();
    partials_type_extractor.visit_item_fn_mut(item_fn);
    partials_type_extractor.take()
}

pub(crate) fn extract_once(item_fn: &mut ItemFn) -> Result<Option<syn::Attribute>, ErrorsVec> {
    let mut extractor = JustOnceFnAttributeExtractor::from("once");

    extractor.visit_item_fn_mut(item_fn);
    extractor.take()
}

pub(crate) fn extract_argument_attrs<'a, B: 'a + std::fmt::Debug>(
    node: &mut FnArg,
    is_valid_attr: fn(&syn::Attribute) -> bool,
    build: impl Fn(syn::Attribute) -> syn::Result<B> + 'a,
) -> Box<dyn Iterator<Item = syn::Result<B>> + 'a> {
    let name = node.maybe_ident().cloned();
    if name.is_none() {
        return Box::new(std::iter::empty());
    }

    if let FnArg::Typed(ref mut arg) = node {
        // Extract interesting attributes
        let attrs = std::mem::take(&mut arg.attrs);
        let (extracted, remain): (Vec<_>, Vec<_>) = attrs.into_iter().partition(is_valid_attr);

        arg.attrs = remain;

        // Parse attrs
        Box::new(extracted.into_iter().map(build))
    } else {
        Box::new(std::iter::empty())
    }
}

/// Simple struct used to visit function attributes and extract default return
/// type
struct PartialsTypeFunctionExtractor(Result<Vec<(usize, syn::Type)>, ErrorsVec>);

impl PartialsTypeFunctionExtractor {
    fn take(self) -> Result<Vec<(usize, syn::Type)>, ErrorsVec> {
        self.0
    }
}

impl Default for PartialsTypeFunctionExtractor {
    fn default() -> Self {
        Self(Ok(Vec::default()))
    }
}

impl VisitMut for PartialsTypeFunctionExtractor {
    fn visit_item_fn_mut(&mut self, node: &mut ItemFn) {
        let attrs = std::mem::take(&mut node.attrs);
        let (partials, remain): (Vec<_>, Vec<_>) =
            attrs
                .into_iter()
                .partition(|attr| match attr.path().get_ident() {
                    Some(name) => name
                        .to_string()
                        .starts_with(FixtureModifiers::PARTIAL_RET_ATTR),
                    None => false,
                });

        node.attrs = remain;
        let mut errors = ErrorsVec::default();
        let mut data: Vec<(usize, syn::Type)> = Vec::default();
        for attr in partials {
            match attr.parse_args::<syn::Type>() {
                Ok(t) => {
                    match attr.path().get_ident().unwrap().to_string()
                        [FixtureModifiers::PARTIAL_RET_ATTR.len()..]
                        .parse()
                    {
                        Ok(id) => data.push((id, t)),
                        Err(_) => errors.push(syn::Error::new_spanned(
                            attr,
                            "Invalid partial syntax: should be partial_<n_arguments>",
                        )),
                    }
                }
                Err(e) => errors.push(e),
            }
        }
        self.0 = if errors.len() > 0 {
            Err(errors)
        } else {
            Ok(data)
        };
    }
}

pub(crate) fn extract_case_args(item_fn: &mut ItemFn) -> Result<Vec<Pat>, ErrorsVec> {
    let mut extractor = JustOnceFnArgAttributeExtractor::from("case");
    extractor.visit_item_fn_mut(item_fn);

    extractor.take()
}

/// Simple struct used to visit function attributes and extract cases and
/// eventualy parsing errors
#[derive(Default)]
struct CasesFunctionExtractor(Vec<TestCase>, Vec<syn::Error>);

impl VisitMut for CasesFunctionExtractor {
    fn visit_item_fn_mut(&mut self, node: &mut ItemFn) {
        let attrs = std::mem::take(&mut node.attrs);
        let mut attrs_buffer = Default::default();
        let case: syn::PathSegment = parse_quote! { case };
        for attr in attrs.into_iter() {
            if attr_starts_with(&attr, &case) {
                match attr.parse_args::<Expressions>() {
                    Ok(expressions) => {
                        let description =
                            attr.path().segments.iter().nth(1).map(|p| p.ident.clone());
                        self.0.push(TestCase {
                            args: expressions.into(),
                            attrs: std::mem::take(&mut attrs_buffer),
                            description,
                        });
                    }
                    Err(err) => self.1.push(err),
                };
            } else {
                attrs_buffer.push(attr)
            }
        }
        node.attrs = std::mem::take(&mut attrs_buffer);
    }
}

pub(crate) fn extract_cases(item_fn: &mut ItemFn) -> Result<Vec<TestCase>, ErrorsVec> {
    let mut cases_extractor = CasesFunctionExtractor::default();
    cases_extractor.visit_item_fn_mut(item_fn);

    if cases_extractor.1.is_empty() {
        Ok(cases_extractor.0)
    } else {
        Err(cases_extractor.1.into())
    }
}

pub(crate) fn extract_value_list(item_fn: &mut ItemFn) -> Result<Vec<ValueList>, ErrorsVec> {
    struct ValueListBuilder;
    impl AttrBuilder<Pat> for ValueListBuilder {
        type Out = ValueList;

        fn build(attr: syn::Attribute, extra: &Pat) -> syn::Result<Self::Out> {
            attr.parse_args::<Expressions>().map(|v| ValueList {
                arg: extra.clone(),
                values: v.take().into_iter().map(|e| e.into()).collect(),
            })
        }
    }
    impl Validator<FnArg> for ValueListBuilder {}

    let mut extractor = JustOnceFnArgAttributeExtractor::<ValueListBuilder>::new("values");

    extractor.visit_item_fn_mut(item_fn);
    extractor.take()
}

/// Simple struct used to visit function args attributes to extract the
/// excluded ones and eventualy parsing errors
struct ExcludedTraceAttributesFunctionExtractor(Result<Vec<Pat>, ErrorsVec>);
impl From<Result<Vec<Pat>, ErrorsVec>> for ExcludedTraceAttributesFunctionExtractor {
    fn from(inner: Result<Vec<Pat>, ErrorsVec>) -> Self {
        Self(inner)
    }
}

impl ExcludedTraceAttributesFunctionExtractor {
    pub(crate) fn take(self) -> Result<Vec<Pat>, ErrorsVec> {
        self.0
    }

    fn update_error(&mut self, mut errors: ErrorsVec) {
        match &mut self.0 {
            Ok(_) => self.0 = Err(errors),
            Err(err) => err.append(&mut errors),
        }
    }

    fn update_excluded(&mut self, value: Pat) {
        if let Some(inner) = self.0.iter_mut().next() {
            inner.push(value);
        }
    }
}

impl Default for ExcludedTraceAttributesFunctionExtractor {
    fn default() -> Self {
        Self(Ok(Default::default()))
    }
}

impl VisitMut for ExcludedTraceAttributesFunctionExtractor {
    fn visit_fn_arg_mut(&mut self, node: &mut FnArg) {
        let pat = match node.maybe_pat().cloned() {
            Some(pat) => pat,
            None => return,
        };
        for r in extract_argument_attrs(node, |a| attr_is(a, "notrace"), |_a| Ok(())) {
            match r {
                Ok(_) => self.update_excluded(pat.clone()),
                Err(err) => self.update_error(err.into()),
            }
        }

        syn::visit_mut::visit_fn_arg_mut(self, node);
    }
}

pub(crate) fn extract_excluded_trace(item_fn: &mut ItemFn) -> Result<Vec<Pat>, ErrorsVec> {
    let mut excluded_trace_extractor = ExcludedTraceAttributesFunctionExtractor::default();
    excluded_trace_extractor.visit_item_fn_mut(item_fn);
    excluded_trace_extractor.take()
}

/// Simple struct used to visit function args attributes to check timeout syntax
struct CheckTimeoutAttributesFunction(Result<(), ErrorsVec>);
impl From<ErrorsVec> for CheckTimeoutAttributesFunction {
    fn from(errors: ErrorsVec) -> Self {
        Self(Err(errors))
    }
}

impl CheckTimeoutAttributesFunction {
    pub(crate) fn take(self) -> Result<(), ErrorsVec> {
        self.0
    }

    fn check_if_can_implement_timeous(
        &self,
        timeouts: &[&syn::Attribute],
        asyncness: Option<&Async>,
    ) -> Option<syn::Error> {
        if cfg!(feature = "async-timeout") || timeouts.is_empty() {
            None
        } else {
            asyncness.map(|a| {
                syn::Error::new(
                    a.span,
                    "Enable async-timeout feature to use timeout in async tests",
                )
            })
        }
    }
}

impl Default for CheckTimeoutAttributesFunction {
    fn default() -> Self {
        Self(Ok(()))
    }
}

impl VisitMut for CheckTimeoutAttributesFunction {
    fn visit_item_fn_mut(&mut self, node: &mut ItemFn) {
        let timeouts = node
            .attrs
            .iter()
            .filter(|&a| attr_is(a, "timeout"))
            .collect::<Vec<_>>();
        let mut errors = timeouts
            .iter()
            .map(|&attr| attr.parse_args::<syn::Expr>())
            .filter_map(Result::err)
            .collect::<Vec<_>>();

        if let Some(e) =
            self.check_if_can_implement_timeous(timeouts.as_slice(), node.sig.asyncness.as_ref())
        {
            errors.push(e);
        }
        if !errors.is_empty() {
            *self = Self(Err(errors.into()));
        }
    }
}

pub(crate) fn check_timeout_attrs(item_fn: &mut ItemFn) -> Result<(), ErrorsVec> {
    let mut checker = CheckTimeoutAttributesFunction::default();
    checker.visit_item_fn_mut(item_fn);
    checker.take()
}

#[cfg(test)]
mod should {
    use super::*;
    use crate::test::*;

    mod parse_attributes {
        use super::assert_eq;
        use super::*;

        fn parse_attributes<S: AsRef<str>>(attributes: S) -> Attributes {
            parse_meta(attributes)
        }

        #[test]
        fn one_simple_ident() {
            let attributes = parse_attributes("my_ident");

            let expected = Attributes {
                attributes: vec![Attribute::attr("my_ident")],
            };

            assert_eq!(expected, attributes);
        }

        #[test]
        fn one_simple_group() {
            let attributes = parse_attributes("group_tag(first, second)");

            let expected = Attributes {
                attributes: vec![Attribute::tagged("group_tag", vec!["first", "second"])],
            };

            assert_eq!(expected, attributes);
        }

        #[test]
        fn one_simple_type() {
            let attributes = parse_attributes("type_tag<(u32, T, (String, i32))>");

            let expected = Attributes {
                attributes: vec![Attribute::typed("type_tag", "(u32, T, (String, i32))")],
            };

            assert_eq!(expected, attributes);
        }

        #[test]
        fn integrated() {
            let attributes = parse_attributes(
                r#"
            simple :: tagged(first, second) :: type_tag<(u32, T, (std::string::String, i32))> :: more_tagged(a,b)"#,
            );

            let expected = Attributes {
                attributes: vec![
                    Attribute::attr("simple"),
                    Attribute::tagged("tagged", vec!["first", "second"]),
                    Attribute::typed("type_tag", "(u32, T, (std::string::String, i32))"),
                    Attribute::tagged("more_tagged", vec!["a", "b"]),
                ],
            };

            assert_eq!(expected, attributes);
        }
    }
}
