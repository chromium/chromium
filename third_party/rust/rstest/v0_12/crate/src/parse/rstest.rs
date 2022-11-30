use syn::{
    parse::{Parse, ParseStream},
    Ident, ItemFn, Token,
};

use super::testcase::TestCase;
use super::{
    extract_case_args, extract_cases, extract_excluded_trace, extract_fixtures, extract_value_list,
    parse_vector_trailing_till_double_comma, Attribute, Attributes, ExtendWithFunctionAttrs,
    Fixture,
};
use crate::parse::vlist::ValueList;
use crate::{
    error::ErrorsVec,
    refident::{MaybeIdent, RefIdent},
};
use proc_macro2::{Span, TokenStream};
use quote::{format_ident, ToTokens};

#[derive(PartialEq, Debug, Default)]
pub(crate) struct RsTestInfo {
    pub(crate) data: RsTestData,
    pub(crate) attributes: RsTestAttributes,
}

impl Parse for RsTestInfo {
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
            }
        })
    }
}

impl ExtendWithFunctionAttrs for RsTestInfo {
    fn extend_with_function_attrs(&mut self, item_fn: &mut ItemFn) -> Result<(), ErrorsVec> {
        let (_, excluded) = merge_errors!(
            self.data.extend_with_function_attrs(item_fn),
            extract_excluded_trace(item_fn)
        )?;
        self.attributes.add_notraces(excluded);
        Ok(())
    }
}

#[derive(PartialEq, Debug, Default)]
pub(crate) struct RsTestData {
    pub(crate) items: Vec<RsTestItem>,
}

impl RsTestData {
    pub(crate) fn case_args(&self) -> impl Iterator<Item = &Ident> {
        self.items.iter().filter_map(|it| match it {
            RsTestItem::CaseArgName(ref arg) => Some(arg),
            _ => None,
        })
    }

    #[allow(dead_code)]
    pub(crate) fn has_case_args(&self) -> bool {
        self.case_args().next().is_some()
    }

    pub(crate) fn cases(&self) -> impl Iterator<Item = &TestCase> {
        self.items.iter().filter_map(|it| match it {
            RsTestItem::TestCase(ref case) => Some(case),
            _ => None,
        })
    }

    pub(crate) fn has_cases(&self) -> bool {
        self.cases().next().is_some()
    }

    pub(crate) fn fixtures(&self) -> impl Iterator<Item = &Fixture> {
        self.items.iter().filter_map(|it| match it {
            RsTestItem::Fixture(ref fixture) => Some(fixture),
            _ => None,
        })
    }

    #[allow(dead_code)]
    pub(crate) fn has_fixtures(&self) -> bool {
        self.fixtures().next().is_some()
    }

    pub(crate) fn list_values(&self) -> impl Iterator<Item = &ValueList> {
        self.items.iter().filter_map(|mv| match mv {
            RsTestItem::ValueList(ref value_list) => Some(value_list),
            _ => None,
        })
    }

    pub(crate) fn has_list_values(&self) -> bool {
        self.list_values().next().is_some()
    }
}

impl Parse for RsTestData {
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

impl ExtendWithFunctionAttrs for RsTestData {
    fn extend_with_function_attrs(&mut self, item_fn: &mut ItemFn) -> Result<(), ErrorsVec> {
        let composed_tuple!(fixtures, case_args, cases, value_list) = merge_errors!(
            extract_fixtures(item_fn),
            extract_case_args(item_fn),
            extract_cases(item_fn),
            extract_value_list(item_fn)
        )?;

        self.items.extend(fixtures.into_iter().map(|f| f.into()));
        self.items.extend(case_args.into_iter().map(|f| f.into()));
        self.items.extend(cases.into_iter().map(|f| f.into()));
        self.items.extend(value_list.into_iter().map(|f| f.into()));
        Ok(())
    }
}

#[derive(PartialEq, Debug)]
pub(crate) enum RsTestItem {
    Fixture(Fixture),
    CaseArgName(Ident),
    TestCase(TestCase),
    ValueList(ValueList),
}

impl From<Fixture> for RsTestItem {
    fn from(f: Fixture) -> Self {
        RsTestItem::Fixture(f)
    }
}

impl From<Ident> for RsTestItem {
    fn from(ident: Ident) -> Self {
        RsTestItem::CaseArgName(ident)
    }
}

impl From<TestCase> for RsTestItem {
    fn from(case: TestCase) -> Self {
        RsTestItem::TestCase(case)
    }
}

impl From<ValueList> for RsTestItem {
    fn from(value_list: ValueList) -> Self {
        RsTestItem::ValueList(value_list)
    }
}

impl Parse for RsTestItem {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        if input.fork().parse::<TestCase>().is_ok() {
            input.parse::<TestCase>().map(RsTestItem::TestCase)
        } else if input.peek2(Token![=>]) {
            input.parse::<ValueList>().map(RsTestItem::ValueList)
        } else if input.fork().parse::<Fixture>().is_ok() {
            input.parse::<Fixture>().map(RsTestItem::Fixture)
        } else if input.fork().parse::<Ident>().is_ok() {
            input.parse::<Ident>().map(RsTestItem::CaseArgName)
        } else {
            Err(syn::Error::new(Span::call_site(), "Cannot parse it"))
        }
    }
}

impl MaybeIdent for RsTestItem {
    fn maybe_ident(&self) -> Option<&Ident> {
        use RsTestItem::*;
        match self {
            Fixture(ref fixture) => Some(fixture.ident()),
            CaseArgName(ref case_arg) => Some(case_arg),
            ValueList(ref value_list) => Some(value_list.ident()),
            TestCase(_) => None,
        }
    }
}

impl ToTokens for RsTestItem {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        use RsTestItem::*;
        match self {
            Fixture(ref fixture) => fixture.to_tokens(tokens),
            CaseArgName(ref case_arg) => case_arg.to_tokens(tokens),
            TestCase(ref case) => case.to_tokens(tokens),
            ValueList(ref list) => list.to_tokens(tokens),
        }
    }
}

wrap_attributes!(RsTestAttributes);

impl RsTestAttributes {
    const TRACE_VARIABLE_ATTR: &'static str = "trace";
    const NOTRACE_VARIABLE_ATTR: &'static str = "notrace";

    pub(crate) fn trace_me(&self, ident: &Ident) -> bool {
        if self.should_trace() {
            !self.iter().any(|m| Self::is_notrace(ident, m))
        } else {
            false
        }
    }

    fn is_notrace(ident: &Ident, m: &Attribute) -> bool {
        match m {
            Attribute::Tagged(i, args) if i == Self::NOTRACE_VARIABLE_ATTR => {
                args.iter().any(|a| a == ident)
            }
            _ => false,
        }
    }

    pub(crate) fn should_trace(&self) -> bool {
        self.iter().any(Self::is_trace)
    }

    pub(crate) fn add_trace(&mut self, trace: Ident) {
        self.inner.attributes.push(Attribute::Attr(trace));
    }

    pub(crate) fn add_notraces(&mut self, notraces: Vec<Ident>) {
        if notraces.is_empty() {
            return;
        }
        self.inner.attributes.push(Attribute::Tagged(
            format_ident!("{}", Self::NOTRACE_VARIABLE_ATTR),
            notraces,
        ));
    }

    fn is_trace(m: &Attribute) -> bool {
        matches!(m, Attribute::Attr(i) if i == Self::TRACE_VARIABLE_ATTR)
    }
}

impl Parse for RsTestAttributes {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        Ok(input.parse::<Attributes>()?.into())
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::test::*;

    mod parse_rstest_data {
        use super::assert_eq;
        use super::*;

        fn parse_rstest_data<S: AsRef<str>>(fixtures: S) -> RsTestData {
            parse_meta(fixtures)
        }

        #[test]
        fn one_arg() {
            let fixtures = parse_rstest_data("my_fixture(42)");

            let expected = RsTestData {
                items: vec![fixture("my_fixture", &["42"]).into()],
            };

            assert_eq!(expected, fixtures);
        }
    }

    fn parse_rstest<S: AsRef<str>>(rstest_data: S) -> RsTestInfo {
        parse_meta(rstest_data)
    }

    mod no_cases {
        use super::{assert_eq, *};
        use crate::parse::{Attribute, Attributes};

        #[test]
        fn happy_path() {
            let data = parse_rstest(
                r#"my_fixture(42, "other"), other(vec![42])
            :: trace :: no_trace(some)"#,
            );

            let expected = RsTestInfo {
                data: vec![
                    fixture("my_fixture", &["42", r#""other""#]).into(),
                    fixture("other", &["vec![42]"]).into(),
                ]
                .into(),
                attributes: Attributes {
                    attributes: vec![
                        Attribute::attr("trace"),
                        Attribute::tagged("no_trace", vec!["some"]),
                    ],
                }
                .into(),
            };

            assert_eq!(expected, data);
        }

        mod fixture_extraction {
            use super::{assert_eq, *};

            #[test]
            fn rename() {
                let data = parse_rstest(
                    r#"long_fixture_name(42, "other") as short, simple as s, no_change()"#,
                );

                let expected = RsTestInfo {
                    data: vec![
                        fixture("short", &["42", r#""other""#])
                            .with_resolve("long_fixture_name")
                            .into(),
                        fixture("s", &[]).with_resolve("simple").into(),
                        fixture("no_change", &[]).into(),
                    ]
                    .into(),
                    ..Default::default()
                };

                assert_eq!(expected, data);
            }

            #[test]
            fn rename_with_attributes() {
                let mut item_fn = r#"
                    fn test_fn(
                        #[from(long_fixture_name)] 
                        #[with(42, "other")] short: u32, 
                        #[from(simple)]
                        s: &str,
                        no_change: i32) {
                    }
                    "#
                .ast();

                let expected = RsTestInfo {
                    data: vec![
                        fixture("short", &["42", r#""other""#])
                            .with_resolve("long_fixture_name")
                            .into(),
                        fixture("s", &[]).with_resolve("simple").into(),
                    ]
                    .into(),
                    ..Default::default()
                };

                let mut data = RsTestInfo::default();

                data.extend_with_function_attrs(&mut item_fn).unwrap();

                assert_eq!(expected, data);
            }

            #[test]
            fn defined_via_with_attributes() {
                let mut item_fn = r#"
                    fn test_fn(#[with(42, "other")] my_fixture: u32, #[with(vec![42])] other: &str) {
                    }
                    "#
                .ast();

                let expected = RsTestInfo {
                    data: vec![
                        fixture("my_fixture", &["42", r#""other""#]).into(),
                        fixture("other", &["vec![42]"]).into(),
                    ]
                    .into(),
                    ..Default::default()
                };

                let mut data = RsTestInfo::default();

                data.extend_with_function_attrs(&mut item_fn).unwrap();

                assert_eq!(expected, data);
            }
        }

        #[test]
        fn empty_fixtures() {
            let data = parse_rstest(r#"::trace::no_trace(some)"#);

            let expected = RsTestInfo {
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
            let data = parse_rstest(r#"my_fixture(42, "other")"#);

            let expected = RsTestInfo {
                data: vec![fixture("my_fixture", &["42", r#""other""#]).into()].into(),
                ..Default::default()
            };

            assert_eq!(expected, data);
        }

        #[test]
        fn extract_notrace_args_atttribute() {
            let mut item_fn = r#"
            fn test_fn(#[notrace] a: u32, #[something_else] b: &str, #[notrace] c: i32) {
            }
            "#
            .ast();

            let mut info = RsTestInfo::default();

            info.extend_with_function_attrs(&mut item_fn).unwrap();
            info.attributes.add_trace(ident("trace"));

            assert!(!info.attributes.trace_me(&ident("a")));
            assert!(info.attributes.trace_me(&ident("b")));
            assert!(!info.attributes.trace_me(&ident("c")));
            let b_args = item_fn
                .sig
                .inputs
                .into_iter()
                .nth(1)
                .and_then(|id| match id {
                    syn::FnArg::Typed(arg) => Some(arg.attrs),
                    _ => None,
                })
                .unwrap();
            assert_eq!(attrs("#[something_else]"), b_args);
        }
    }

    mod parametrize_cases {
        use super::{assert_eq, *};
        use std::iter::FromIterator;

        #[test]
        fn one_simple_case_one_arg() {
            let data = parse_rstest(r#"arg, case(42)"#).data;

            let args = data.case_args().collect::<Vec<_>>();
            let cases = data.cases().collect::<Vec<_>>();

            assert_eq!(1, args.len());
            assert_eq!(1, cases.len());
            assert_eq!("arg", &args[0].to_string());
            assert_eq!(to_args!(["42"]), cases[0].args())
        }

        #[test]
        fn happy_path() {
            let info = parse_rstest(
                r#"
                my_fixture(42,"foo"),
                arg1, arg2, arg3,
                case(1,2,3),
                case(11,12,13),
                case(21,22,23)
            "#,
            );

            let data = info.data;
            let fixtures = data.fixtures().cloned().collect::<Vec<_>>();

            assert_eq!(vec![fixture("my_fixture", &["42", r#""foo""#])], fixtures);
            assert_eq!(
                to_strs!(vec!["arg1", "arg2", "arg3"]),
                data.case_args()
                    .map(ToString::to_string)
                    .collect::<Vec<_>>()
            );

            let cases = data.cases().collect::<Vec<_>>();

            assert_eq!(3, cases.len());
            assert_eq!(to_args!(["1", "2", "3"]), cases[0].args());
            assert_eq!(to_args!(["11", "12", "13"]), cases[1].args());
            assert_eq!(to_args!(["21", "22", "23"]), cases[2].args());
        }

        mod defined_via_with_attributes {
            use super::{assert_eq, *};

            #[test]
            fn one_case() {
                let mut item_fn = r#"
                #[case::first(42, "first")]
                fn test_fn(#[case] arg1: u32, #[case] arg2: &str) {
                }
                "#
                .ast();

                let mut info = RsTestInfo::default();

                info.extend_with_function_attrs(&mut item_fn).unwrap();

                let case_args = info.data.case_args().cloned().collect::<Vec<_>>();
                let cases = info.data.cases().cloned().collect::<Vec<_>>();

                assert_eq!(to_idents!(["arg1", "arg2"]), case_args);
                assert_eq!(
                    vec![
                        TestCase::from_iter(["42", r#""first""#].iter()).with_description("first"),
                    ],
                    cases
                );
            }

            #[test]
            fn parse_tuple_value() {
                let mut item_fn = r#"
                #[case(42, (24, "first"))]
                fn test_fn(#[case] arg1: u32, #[case] tupled: (u32, &str)) {
                }
                "#
                .ast();

                let mut info = RsTestInfo::default();

                info.extend_with_function_attrs(&mut item_fn).unwrap();

                let cases = info.data.cases().cloned().collect::<Vec<_>>();

                assert_eq!(
                    vec![TestCase::from_iter(["42", r#"(24, "first")"#].iter()),],
                    cases
                );
            }

            #[test]
            fn more_cases() {
                let mut item_fn = r#"
                #[case::first(42, "first")]
                #[case(24, "second")]
                #[case::third(0, "third")]
                fn test_fn(#[case] arg1: u32, #[case] arg2: &str) {
                }
                "#
                .ast();

                let mut info = RsTestInfo::default();

                info.extend_with_function_attrs(&mut item_fn).unwrap();

                let case_args = info.data.case_args().cloned().collect::<Vec<_>>();
                let cases = info.data.cases().cloned().collect::<Vec<_>>();

                assert_eq!(to_idents!(["arg1", "arg2"]), case_args);
                assert_eq!(
                    vec![
                        TestCase::from_iter(["42", r#""first""#].iter()).with_description("first"),
                        TestCase::from_iter(["24", r#""second""#].iter()),
                        TestCase::from_iter(["0", r#""third""#].iter()).with_description("third"),
                    ],
                    cases
                );
            }

            #[test]
            fn should_collect_attributes() {
                let mut item_fn = r#"
                    #[first]
                    #[first2(42)]
                    #[case(42)]
                    #[second]
                    #[case(24)]
                    #[global]
                    fn test_fn(#[case] arg: u32) {
                    }
                "#
                .ast();

                let mut info = RsTestInfo::default();

                info.extend_with_function_attrs(&mut item_fn).unwrap();

                let cases = info.data.cases().cloned().collect::<Vec<_>>();

                assert_eq!(
                    vec![
                        TestCase::from_iter(["42"].iter()).with_attrs(attrs(
                            "
                                #[first]
                                #[first2(42)]
                            "
                        )),
                        TestCase::from_iter(["24"].iter()).with_attrs(attrs(
                            "
                            #[second]
                        "
                        )),
                    ],
                    cases
                );
            }

            #[test]
            fn should_consume_all_used_attributes() {
                let mut item_fn = r#"
                    #[first]
                    #[first2(42)]
                    #[case(42)]
                    #[second]
                    #[case(24)]
                    #[global]
                    fn test_fn(#[case] arg: u32) {
                    }
                "#
                .ast();

                let mut info = RsTestInfo::default();

                info.extend_with_function_attrs(&mut item_fn).unwrap();

                assert_eq!(
                    item_fn.attrs,
                    attrs(
                        "
                        #[global]
                        "
                    )
                );
                assert!(!format!("{:?}", item_fn).contains("case"));
            }

            #[test]
            fn should_report_all_errors() {
                let mut item_fn = r#"
                    #[case(#case_error#)]
                    fn test_fn(#[case] arg: u32, #[with(#fixture_error#)] err_fixture: u32) {
                    }
                "#
                .ast();

                let mut info = RsTestInfo::default();

                let errors = info.extend_with_function_attrs(&mut item_fn).unwrap_err();

                assert_eq!(2, errors.len());
            }
        }

        #[test]
        fn should_accept_comma_at_the_end_of_cases() {
            let data = parse_rstest(
                r#"
                arg,
                case(42),
            "#,
            )
            .data;

            let args = data.case_args().collect::<Vec<_>>();
            let cases = data.cases().collect::<Vec<_>>();

            assert_eq!(1, args.len());
            assert_eq!(1, cases.len());
            assert_eq!("arg", &args[0].to_string());
            assert_eq!(to_args!(["42"]), cases[0].args())
        }

        #[test]
        #[should_panic]
        fn should_not_accept_invalid_separator_from_args_and_cases() {
            parse_rstest(
                r#"
                ret
                case::should_success(Ok(())),
                case::should_fail(Err("Return Error"))
            "#,
            );
        }

        #[test]
        fn case_could_be_arg_name() {
            let data = parse_rstest(
                r#"
                case,
                case(42)
            "#,
            )
            .data;

            assert_eq!("case", &data.case_args().next().unwrap().to_string());

            let cases = data.cases().collect::<Vec<_>>();

            assert_eq!(1, cases.len());
            assert_eq!(to_args!(["42"]), cases[0].args());
        }
    }

    mod matrix_cases {
        use crate::parse::Attribute;

        use super::{assert_eq, *};

        #[test]
        fn happy_path() {
            let info = parse_rstest(
                r#"
                    expected => [12, 34 * 2],
                    input => [format!("aa_{}", 2), "other"],
                "#,
            );

            let value_ranges = info.data.list_values().collect::<Vec<_>>();
            assert_eq!(2, value_ranges.len());
            assert_eq!(to_args!(["12", "34 * 2"]), value_ranges[0].args());
            assert_eq!(
                to_args!([r#"format!("aa_{}", 2)"#, r#""other""#]),
                value_ranges[1].args()
            );
            assert_eq!(info.attributes, Default::default());
        }

        #[test]
        fn should_parse_attributes_too() {
            let info = parse_rstest(
                r#"
                                        a => [12, 24, 42]
                                        ::trace
                                    "#,
            );

            assert_eq!(
                info.attributes,
                Attributes {
                    attributes: vec![Attribute::attr("trace")]
                }
                .into()
            );
        }

        #[test]
        fn should_parse_injected_fixtures_too() {
            let info = parse_rstest(
                r#"
                a => [12, 24, 42],
                fixture_1(42, "foo"),
                fixture_2("bar")
                "#,
            );

            let fixtures = info.data.fixtures().cloned().collect::<Vec<_>>();

            assert_eq!(
                vec![
                    fixture("fixture_1", &["42", r#""foo""#]),
                    fixture("fixture_2", &[r#""bar""#])
                ],
                fixtures
            );
        }

        #[test]
        #[should_panic(expected = "should not be empty")]
        fn should_not_compile_if_empty_expression_slice() {
            parse_rstest(
                r#"
                invalid => []
                "#,
            );
        }

        mod defined_via_with_attributes {
            use super::{assert_eq, *};

            #[test]
            fn one_arg() {
                let mut item_fn = r#"
                fn test_fn(#[values(1, 2, 1+2)] arg1: u32, #[values(format!("a"), "b b".to_owned(), String::new())] arg2: String) {
                }
                "#
                .ast();

                let mut info = RsTestInfo::default();

                info.extend_with_function_attrs(&mut item_fn).unwrap();

                let list_values = info.data.list_values().cloned().collect::<Vec<_>>();

                assert_eq!(2, list_values.len());
                assert_eq!(to_args!(["1", "2", "1+2"]), list_values[0].args());
                assert_eq!(
                    to_args!([r#"format!("a")"#, r#""b b".to_owned()"#, "String::new()"]),
                    list_values[1].args()
                );
            }
        }
    }

    mod integrated {
        use super::{assert_eq, *};

        #[test]
        fn should_parse_fixture_cases_and_matrix_in_any_order() {
            let data = parse_rstest(
                r#"
                u,
                m => [1, 2],
                case(42, A{}, D{}),
                a,
                case(43, A{}, D{}),
                the_fixture(42),
                mm => ["f", "oo", "BAR"],
                d
            "#,
            )
            .data;

            let fixtures = data.fixtures().cloned().collect::<Vec<_>>();
            assert_eq!(vec![fixture("the_fixture", &["42"])], fixtures);

            assert_eq!(
                to_strs!(vec!["u", "a", "d"]),
                data.case_args()
                    .map(ToString::to_string)
                    .collect::<Vec<_>>()
            );

            let cases = data.cases().collect::<Vec<_>>();
            assert_eq!(2, cases.len());
            assert_eq!(to_args!(["42", "A{}", "D{}"]), cases[0].args());
            assert_eq!(to_args!(["43", "A{}", "D{}"]), cases[1].args());

            let value_ranges = data.list_values().collect::<Vec<_>>();
            assert_eq!(2, value_ranges.len());
            assert_eq!(to_args!(["1", "2"]), value_ranges[0].args());
            assert_eq!(
                to_args!([r#""f""#, r#""oo""#, r#""BAR""#]),
                value_ranges[1].args()
            );
        }
    }
}
