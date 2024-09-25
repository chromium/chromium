/// `fixture`'s related data and parsing
use syn::{
    parse::{Parse, ParseStream},
    parse_quote,
    visit_mut::VisitMut,
    Expr, FnArg, Ident, ItemFn, Pat, Token,
};

use super::{
    arguments::ArgumentsInfo,
    extract_default_return_type, extract_defaults, extract_fixtures, extract_partials_return_type,
    future::{extract_futures, extract_global_awt},
    parse_vector_trailing_till_double_comma, Attributes, ExtendWithFunctionAttrs, Fixture,
};
use crate::{
    error::ErrorsVec,
    parse::extract_once,
    refident::{IntoPat, MaybeIdent, MaybePat, MaybePatTypeMut, RefPat},
    utils::attr_is,
};
use crate::{parse::Attribute, utils::attr_in};
use proc_macro2::TokenStream;
use quote::{format_ident, ToTokens};

#[derive(PartialEq, Debug, Default)]
pub(crate) struct FixtureInfo {
    pub(crate) data: FixtureData,
    pub(crate) attributes: FixtureModifiers,
    pub(crate) arguments: ArgumentsInfo,
}

impl Parse for FixtureModifiers {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        Ok(input.parse::<Attributes>()?.into())
    }
}

impl Parse for FixtureInfo {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        Ok(if input.is_empty() {
            Default::default()
        } else {
            Self {
                data: input.parse()?,
                attributes: input
                    .parse::<Token![::]>()
                    .or_else(|_| Ok(Default::default()))
                    .and_then(|_| input.parse())?,
                arguments: Default::default(),
            }
        })
    }
}

impl ExtendWithFunctionAttrs for FixtureInfo {
    fn extend_with_function_attrs(
        &mut self,
        item_fn: &mut ItemFn,
    ) -> std::result::Result<(), ErrorsVec> {
        let composed_tuple!(
            fixtures,
            defaults,
            default_return_type,
            partials_return_type,
            once,
            futures,
            global_awt
        ) = merge_errors!(
            extract_fixtures(item_fn),
            extract_defaults(item_fn),
            extract_default_return_type(item_fn),
            extract_partials_return_type(item_fn),
            extract_once(item_fn),
            extract_futures(item_fn),
            extract_global_awt(item_fn)
        )?;
        self.data.items.extend(
            fixtures
                .into_iter()
                .map(|f| f.into())
                .chain(defaults.into_iter().map(|d| d.into())),
        );
        if let Some(return_type) = default_return_type {
            self.attributes.set_default_return_type(return_type);
        }
        for (id, return_type) in partials_return_type {
            self.attributes.set_partial_return_type(id, return_type);
        }
        self.arguments.set_once(once);
        self.arguments.set_global_await(global_awt);
        self.arguments.set_futures(futures.into_iter());
        self.arguments
            .register_inner_destructored_idents_names(item_fn);

        Ok(())
    }
}

fn parse_attribute_args_just_once<'a, T: Parse>(
    attributes: impl Iterator<Item = &'a syn::Attribute>,
    name: &str,
) -> (Option<T>, Vec<syn::Error>) {
    let mut errors = Vec::new();
    let val = attributes
        .filter(|&a| attr_is(a, name))
        .map(|a| (a, a.parse_args::<T>()))
        .fold(None, |first, (a, res)| match (first, res) {
            (None, Ok(parsed)) => Some(parsed),
            (first, Err(err)) => {
                errors.push(err);
                first
            }
            (first, _) => {
                errors.push(syn::Error::new_spanned(
                    a,
                    crate::error::messages::use_more_than_once(name),
                ));
                first
            }
        });
    (val, errors)
}

/// Simple struct used to visit function attributes and extract Fixtures and
/// eventually parsing errors
#[derive(Default)]
pub(crate) struct FixturesFunctionExtractor(pub(crate) Vec<Fixture>, pub(crate) Vec<syn::Error>);

impl VisitMut for FixturesFunctionExtractor {
    fn visit_fn_arg_mut(&mut self, node: &mut FnArg) {
        let arg = match node.maybe_pat_type_mut() {
            Some(pt) => pt,
            None => return,
        };
        let (extracted, remain): (Vec<_>, Vec<_>) = std::mem::take(&mut arg.attrs)
            .into_iter()
            .partition(|attr| attr_in(attr, &["with", "from"]));
        arg.attrs = remain;

        let (pos, errors) = parse_attribute_args_just_once(extracted.iter(), "with");
        self.1.extend(errors);
        let (resolve, errors): (Option<syn::Path>, _) =
            parse_attribute_args_just_once(extracted.iter(), "from");
        self.1.extend(errors);

        match (resolve, arg.pat.maybe_ident()) {
            (Some(res), _) => self.0.push(Fixture::new(
                arg.pat.as_ref().clone(),
                res,
                pos.unwrap_or_default(),
            )),
            (None, Some(ident)) if pos.is_some() => self.0.push(Fixture::new(
                arg.pat.as_ref().clone(),
                ident.clone().into(),
                pos.unwrap_or_default(),
            )),
            (None, None) if pos.is_some() => {
                self.1.push(syn::Error::new_spanned(
                    node,
                    crate::error::messages::DESTRUCT_WITHOUT_FROM,
                ));
            }
            _ => {}
        }
    }
}

#[derive(PartialEq, Debug, Default)]
pub(crate) struct FixtureData {
    pub items: Vec<FixtureItem>,
}

impl FixtureData {
    pub(crate) fn fixtures(&self) -> impl Iterator<Item = &Fixture> {
        self.items.iter().filter_map(|f| match f {
            FixtureItem::Fixture(ref fixture) => Some(fixture),
            _ => None,
        })
    }

    pub(crate) fn values(&self) -> impl Iterator<Item = &ArgumentValue> {
        self.items.iter().filter_map(|f| match f {
            FixtureItem::ArgumentValue(ref value) => Some(value.as_ref()),
            _ => None,
        })
    }
}

impl Parse for FixtureData {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        if input.peek(Token![::]) {
            Ok(Default::default())
        } else {
            Ok(Self {
                items: parse_vector_trailing_till_double_comma::<_, Token![,]>(input)?,
            })
        }
    }
}

#[derive(PartialEq, Debug)]
pub(crate) struct ArgumentValue {
    pub arg: Pat,
    pub expr: Expr,
}

impl ArgumentValue {
    pub(crate) fn new(arg: Pat, expr: Expr) -> Self {
        Self { arg, expr }
    }
}

#[derive(PartialEq, Debug)]
pub(crate) enum FixtureItem {
    Fixture(Fixture),
    ArgumentValue(Box<ArgumentValue>),
}

impl From<Fixture> for FixtureItem {
    fn from(f: Fixture) -> Self {
        FixtureItem::Fixture(f)
    }
}

impl Parse for FixtureItem {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        if input.peek2(Token![=]) {
            input.parse::<ArgumentValue>().map(|v| v.into())
        } else {
            input.parse::<Fixture>().map(|v| v.into())
        }
    }
}

impl RefPat for FixtureItem {
    fn pat(&self) -> &Pat {
        match self {
            FixtureItem::Fixture(Fixture { ref arg, .. }) => arg,
            FixtureItem::ArgumentValue(ref av) => &av.arg,
        }
    }
}

impl MaybePat for FixtureItem {
    fn maybe_pat(&self) -> Option<&syn::Pat> {
        Some(self.pat())
    }
}

impl ToTokens for FixtureItem {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        self.pat().to_tokens(tokens)
    }
}

impl From<ArgumentValue> for FixtureItem {
    fn from(av: ArgumentValue) -> Self {
        FixtureItem::ArgumentValue(Box::new(av))
    }
}

impl Parse for ArgumentValue {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let name: Ident = input.parse()?;
        let _eq: Token![=] = input.parse()?;
        let expr = input.parse()?;
        Ok(ArgumentValue::new(name.into_pat(), expr))
    }
}

wrap_attributes!(FixtureModifiers);

impl FixtureModifiers {
    pub(crate) const DEFAULT_RET_ATTR: &'static str = "default";
    pub(crate) const PARTIAL_RET_ATTR: &'static str = "partial_";

    pub(crate) fn extract_default_type(&self) -> Option<syn::ReturnType> {
        self.extract_type(Self::DEFAULT_RET_ATTR)
    }

    pub(crate) fn extract_partial_type(&self, pos: usize) -> Option<syn::ReturnType> {
        self.extract_type(&format!("{}{}", Self::PARTIAL_RET_ATTR, pos))
    }

    pub(crate) fn set_default_return_type(&mut self, return_type: syn::Type) {
        self.inner.attributes.push(Attribute::Type(
            format_ident!("{}", Self::DEFAULT_RET_ATTR),
            Box::new(return_type),
        ))
    }

    pub(crate) fn set_partial_return_type(&mut self, id: usize, return_type: syn::Type) {
        self.inner.attributes.push(Attribute::Type(
            format_ident!("{}{}", Self::PARTIAL_RET_ATTR, id),
            Box::new(return_type),
        ))
    }

    fn extract_type(&self, attr_name: &str) -> Option<syn::ReturnType> {
        self.iter()
            .filter_map(|m| match m {
                Attribute::Type(name, t) if name == attr_name => Some(parse_quote! { -> #t}),
                _ => None,
            })
            .next()
    }
}

#[cfg(test)]
mod should {
    use super::*;
    use crate::test::{assert_eq, *};

    mod parse {
        use super::{assert_eq, *};

        fn parse_fixture<S: AsRef<str>>(fixture_data: S) -> FixtureInfo {
            parse_meta(fixture_data)
        }

        #[test]
        fn happy_path() {
            let data = parse_fixture(
                r#"my_fixture(42, "other"), other(vec![42]), value=42, other_value=vec![1.0]
                    :: trace :: no_trace(some)"#,
            );

            let expected = FixtureInfo {
                data: vec![
                    fixture("my_fixture", &["42", r#""other""#]).into(),
                    fixture("other", &["vec![42]"]).into(),
                    arg_value("value", "42").into(),
                    arg_value("other_value", "vec![1.0]").into(),
                ]
                .into(),
                attributes: Attributes {
                    attributes: vec![
                        Attribute::attr("trace"),
                        Attribute::tagged("no_trace", vec!["some"]),
                    ],
                }
                .into(),
                arguments: Default::default(),
            };

            assert_eq!(expected, data);
        }

        #[test]
        fn some_literals() {
            let args_expressions = literal_expressions_str();
            let fixture = parse_fixture(&format!("my_fixture({})", args_expressions.join(", ")));
            let args = fixture.data.fixtures().next().unwrap().positional.clone();

            assert_eq!(to_args!(args_expressions), args.0);
        }

        #[test]
        fn empty_fixtures() {
            let data = parse_fixture(r#"::trace::no_trace(some)"#);

            let expected = FixtureInfo {
                attributes: Attributes {
                    attributes: vec![
                        Attribute::attr("trace"),
                        Attribute::tagged("no_trace", vec!["some"]),
                    ],
                }
                .into(),
                ..Default::default()
            };

            assert_eq!(expected, data);
        }

        #[test]
        fn empty_attributes() {
            let data = parse_fixture(r#"my_fixture(42, "other")"#);

            let expected = FixtureInfo {
                data: vec![fixture("my_fixture", &["42", r#""other""#]).into()].into(),
                ..Default::default()
            };

            assert_eq!(expected, data);
        }

        #[rstest]
        #[case("first(42),", 1)]
        #[case("first(42), second=42,", 2)]
        #[case(r#"fixture(42, "other"), :: trace"#, 1)]
        #[case(r#"second=42, fixture(42, "other"), :: trace"#, 2)]
        fn should_accept_trailing_comma(#[case] input: &str, #[case] expected: usize) {
            let info: FixtureInfo = input.ast();

            assert_eq!(
                expected,
                info.data.fixtures().count() + info.data.values().count()
            );
        }
    }
}

#[cfg(test)]
mod extend {
    use super::*;
    use crate::test::{assert_eq, *};
    use syn::ItemFn;

    mod should {
        use super::{assert_eq, *};

        #[test]
        fn use_with_attributes() {
            let to_parse = r#"
                fn my_fix(#[with(2)] f1: &str, #[with(vec![1,2], "s")] f2: u32) {}
            "#;

            let mut item_fn: ItemFn = to_parse.ast();
            let mut info = FixtureInfo::default();

            info.extend_with_function_attrs(&mut item_fn).unwrap();

            let expected = FixtureInfo {
                data: vec![
                    fixture("f1", &["2"]).into(),
                    fixture("f2", &["vec![1,2]", r#""s""#]).into(),
                ]
                .into(),
                ..Default::default()
            };

            assert!(!format!("{:?}", item_fn).contains("with"));
            assert_eq!(expected, info);
        }

        #[test]
        fn rename_with_attributes() {
            let mut item_fn = r#"
                    fn test_fn(
                        #[from(long_fixture_name)] 
                        #[with(42, "other")] short: u32, 
                        #[from(sub_module::fix)]
                        f: u32,
                        #[from(simple)]
                        s: &str,
                        no_change: i32) {
                    }
                    "#
            .ast();

            let expected = FixtureInfo {
                data: vec![
                    fixture("short", &["42", r#""other""#])
                        .with_resolve("long_fixture_name")
                        .into(),
                    fixture("f", &[]).with_resolve("sub_module::fix").into(),
                    fixture("s", &[]).with_resolve("simple").into(),
                ]
                .into(),
                ..Default::default()
            };

            let mut data = FixtureInfo::default();
            data.extend_with_function_attrs(&mut item_fn).unwrap();

            assert_eq!(expected, data);
        }

        #[test]
        fn use_default_values_attributes() {
            let to_parse = r#"
                fn my_fix(#[default(2)] f1: &str, #[default((vec![1,2], "s"))] f2: (Vec<u32>, &str)) {}
            "#;

            let mut item_fn: ItemFn = to_parse.ast();
            let mut info = FixtureInfo::default();

            info.extend_with_function_attrs(&mut item_fn).unwrap();

            let expected = FixtureInfo {
                data: vec![
                    arg_value("f1", "2").into(),
                    arg_value("f2", r#"(vec![1,2], "s")"#).into(),
                ]
                .into(),
                ..Default::default()
            };

            assert!(!format!("{:?}", item_fn).contains("default"));
            assert_eq!(expected, info);
        }

        #[test]
        fn find_default_return_type() {
            let mut item_fn: ItemFn = r#"
                #[simple]
                #[first(comp)]
                #[second::default]
                #[default(impl Iterator<Item=(u32, i32)>)]
                #[last::more]
                fn my_fix<I, J>(f1: I, f2: J) -> impl Iterator<Item=(I, J)> {}
            "#
            .ast();

            let mut info = FixtureInfo::default();

            info.extend_with_function_attrs(&mut item_fn).unwrap();

            assert_eq!(
                info.attributes.extract_default_type(),
                Some(parse_quote! { -> impl Iterator<Item=(u32, i32)> })
            );
            assert_eq!(
                attrs("#[simple]#[first(comp)]#[second::default]#[last::more]"),
                item_fn.attrs
            );
        }

        #[test]
        fn find_partials_return_type() {
            let mut item_fn: ItemFn = r#"
                #[simple]
                #[first(comp)]
                #[second::default]
                #[partial_1(impl Iterator<Item=(u32, J, K)>)]
                #[partial_2(impl Iterator<Item=(u32, i32, K)>)]
                #[last::more]
                fn my_fix<I, J, K>(f1: I, f2: J, f3: K) -> impl Iterator<Item=(I, J, K)> {}
            "#
            .ast();

            let mut info = FixtureInfo::default();

            info.extend_with_function_attrs(&mut item_fn).unwrap();

            assert_eq!(
                info.attributes.extract_partial_type(1),
                Some(parse_quote! { -> impl Iterator<Item=(u32, J, K)> })
            );
            assert_eq!(
                info.attributes.extract_partial_type(2),
                Some(parse_quote! { -> impl Iterator<Item=(u32, i32, K)> })
            );
            assert_eq!(
                attrs("#[simple]#[first(comp)]#[second::default]#[last::more]"),
                item_fn.attrs
            );
        }

        #[test]
        fn find_once_attribute() {
            let mut item_fn: ItemFn = r#"
                #[simple]
                #[first(comp)]
                #[second::default]
                #[once]
                #[last::more]
                fn my_fix<I, J, K>(f1: I, f2: J, f3: K) -> impl Iterator<Item=(I, J, K)> {}
            "#
            .ast();

            let mut info = FixtureInfo::default();

            info.extend_with_function_attrs(&mut item_fn).unwrap();

            assert!(info.arguments.is_once());
        }

        #[test]
        fn no_once_attribute() {
            let mut item_fn: ItemFn = r#"
                fn my_fix<I, J, K>(f1: I, f2: J, f3: K) -> impl Iterator<Item=(I, J, K)> {}
            "#
            .ast();

            let mut info = FixtureInfo::default();

            info.extend_with_function_attrs(&mut item_fn).unwrap();

            assert!(!info.arguments.is_once());
        }

        #[rstest]
        fn extract_future() {
            let mut item_fn = "fn f(#[future] a: u32, b: u32) {}".ast();
            let expected = "fn f(a: u32, b: u32) {}".ast();

            let mut info = FixtureInfo::default();

            info.extend_with_function_attrs(&mut item_fn).unwrap();

            assert_eq!(item_fn, expected);
            assert!(info.arguments.is_future(&pat("a")));
            assert!(!info.arguments.is_future(&pat("b")));
        }

        mod raise_error {
            use super::{assert_eq, *};
            use rstest_test::assert_in;

            #[test]
            fn for_invalid_expressions() {
                let mut item_fn: ItemFn = r#"
                fn my_fix(#[with(valid)] f1: &str, #[with(with(,.,))] f2: u32, #[with(with(use))] f3: u32) {}
                "#
                .ast();

                let errors = FixtureInfo::default()
                    .extend_with_function_attrs(&mut item_fn)
                    .unwrap_err();

                assert_eq!(2, errors.len());
            }

            #[test]
            fn for_invalid_default_type() {
                let mut item_fn: ItemFn = r#"
                    #[default(no<valid::>type)]
                    fn my_fix<I>() -> I {}
                "#
                .ast();

                let errors = FixtureInfo::default()
                    .extend_with_function_attrs(&mut item_fn)
                    .unwrap_err();

                assert_eq!(1, errors.len());
            }

            #[test]
            fn with_used_more_than_once() {
                let mut item_fn: ItemFn = r#"
                    fn my_fix(#[with(1)] #[with(2)] fixture1: &str, #[with(1)] #[with(2)] #[with(3)] fixture2: &str) {}
                "#
                .ast();

                let errors = FixtureInfo::default()
                    .extend_with_function_attrs(&mut item_fn)
                    .err()
                    .unwrap_or_default();

                assert_eq!(3, errors.len());
            }

            #[test]
            fn fixture_destruct_without_from() {
                let mut item_fn: ItemFn = r#"
                    fn my_fix(#[with(1)] T{a}: T) {}
                "#
                .ast();

                let errors = FixtureInfo::default()
                    .extend_with_function_attrs(&mut item_fn)
                    .err()
                    .unwrap_or_default();

                assert_in!(errors[0].to_string(), "destruct");
            }

            #[test]
            fn from_used_more_than_once() {
                let mut item_fn: ItemFn = r#"
                    fn my_fix(#[from(a)] #[from(b)] fixture1: &str, #[from(c)] #[from(d)] #[from(e)] fixture2: &str) {}
                "#
                .ast();

                let errors = FixtureInfo::default()
                    .extend_with_function_attrs(&mut item_fn)
                    .err()
                    .unwrap_or_default();

                assert_eq!(3, errors.len());
            }

            #[test]
            fn future_is_used_more_than_once() {
                let mut item_fn: ItemFn = r#"
                    fn my_fix(#[future] #[future] fixture1: u32) {}
                "#
                .ast();

                let errors = FixtureInfo::default()
                    .extend_with_function_attrs(&mut item_fn)
                    .err()
                    .unwrap_or_default();

                assert_eq!(1, errors.len());
                assert_in!(errors[0].to_string(), "more than once");
            }

            #[test]
            fn default_used_more_than_once() {
                let mut item_fn: ItemFn = r#"
                    fn my_fix(#[default(2)] #[default(3)] f1: u32) {}
                "#
                .ast();

                let errors = FixtureInfo::default()
                    .extend_with_function_attrs(&mut item_fn)
                    .err()
                    .unwrap_or_default();

                assert_eq!(1, errors.len());
                assert_in!(errors[0].to_string(), "more than once");
            }

            #[test]
            fn if_once_is_defined_more_than_once() {
                let mut item_fn: ItemFn = r#"
                    #[once]
                    #[once]
                    fn my_fix<I>() -> I {}
                    "#
                .ast();

                let mut info = FixtureInfo::default();

                let error = info.extend_with_function_attrs(&mut item_fn).unwrap_err();

                assert_in!(
                    format!("{:?}", error).to_lowercase(),
                    "cannot use #[once] more than once"
                );
            }

            #[test]
            fn if_default_is_defined_more_than_once() {
                let mut item_fn: ItemFn = r#"
                    #[default(u32)]
                    #[default(u32)]
                    fn my_fix<I>() -> I {}
                    "#
                .ast();

                let mut info = FixtureInfo::default();

                let error = info.extend_with_function_attrs(&mut item_fn).unwrap_err();

                assert_in!(
                    format!("{:?}", error).to_lowercase(),
                    "cannot use #[default] more than once"
                );
            }

            #[test]
            fn for_invalid_partial_type() {
                let mut item_fn: ItemFn = r#"
                    #[partial_1(no<valid::>type)]
                    fn my_fix<I>(x: I, y: u32) -> I {}
                "#
                .ast();

                let errors = FixtureInfo::default()
                    .extend_with_function_attrs(&mut item_fn)
                    .unwrap_err();

                assert_eq!(1, errors.len());
            }

            #[test]
            fn if_partial_is_not_correct() {
                let mut item_fn: ItemFn = r#"
                    #[partial_not_a_number(u32)]
                    fn my_fix<I, J>(f1: I, f2: &str) -> I {}
                    "#
                .ast();

                let mut info = FixtureInfo::default();

                let error = info.extend_with_function_attrs(&mut item_fn).unwrap_err();

                assert_in!(
                    format!("{:?}", error).to_lowercase(),
                    "invalid partial syntax"
                );
            }
        }
    }
}
