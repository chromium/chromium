#![cfg(test)]

use syn::{
    parse::{Parse, ParseStream, Result},
    parse2, parse_str,
    visit::Visit,
    ItemFn, ItemMod, LocalInit,
};

use super::*;
use crate::test::{assert_eq, fixture, *};
use crate::utils::*;

trait SetAsync {
    fn set_async(&mut self, is_async: bool);
}

impl SetAsync for ItemFn {
    fn set_async(&mut self, is_async: bool) {
        self.sig.asyncness = if is_async {
            Some(parse_quote! { async })
        } else {
            None
        };
    }
}

fn trace_argument_code_string(arg_name: &str) -> String {
    let arg_name = ident(arg_name);
    let statement: Stmt = parse_quote! {
        println!("{} = {:?}", stringify!(#arg_name) ,#arg_name);
    };
    statement.display_code()
}

mod single_test_should {
    use rstest_test::{assert_in, assert_not_in};

    use crate::{
        parse::arguments::{ArgumentsInfo, FutureArg},
        test::{assert_eq, *},
    };

    use super::*;

    #[test]
    fn add_return_type_if_any() {
        let input_fn: ItemFn = "fn function(fix: String) -> Result<i32, String> { Ok(42) }".ast();

        let result: ItemFn = single(input_fn.clone(), Default::default()).ast();

        assert_eq!(result.sig.output, input_fn.sig.output);
    }

    fn extract_inner_test_function(outer: &ItemFn) -> ItemFn {
        let first_stmt = outer.block.stmts.get(0).unwrap();

        parse_quote! {
            #first_stmt
        }
    }

    #[test]
    fn include_given_function() {
        let input_fn: ItemFn = r#"
                pub fn test<R: AsRef<str>, B>(mut s: String, v: &u32, a: &mut [i32], r: R) -> (u32, B, String, &str)
                        where B: Borrow<u32>
                {
                    let some = 42;
                    assert_eq!(42, some);
                }
                "#.ast();

        let result: ItemFn = single(input_fn.clone(), Default::default()).ast();

        let inner_fn = extract_inner_test_function(&result);
        let inner_fn_impl: Stmt = inner_fn.block.stmts.last().cloned().unwrap();

        assert_eq!(inner_fn.sig, input_fn.sig);
        assert_eq!(inner_fn_impl.display_code(), input_fn.block.display_code());
    }

    #[test]
    fn not_remove_lifetimes() {
        let input_fn: ItemFn = r#"
                pub fn test<'a, 'b, 'c: 'a + 'b>(a: A<'a>, b: A<'b>, c: A<'c>) -> A<'c>
                {
                }
                "#
        .ast();

        let result: ItemFn = single(input_fn.clone(), Default::default()).ast();

        assert_eq!(3, result.sig.generics.lifetimes().count());
    }

    #[rstest]
    fn not_copy_any_attributes(
        #[values(
            "#[test]",
            "#[very::complicated::path]",
            "#[test]#[should_panic]",
            "#[should_panic]#[test]",
            "#[a]#[b]#[c]"
        )]
        attributes: &str,
    ) {
        let attributes = attrs(attributes);
        let mut input_fn: ItemFn = r#"pub fn test(_s: String){}"#.ast();
        input_fn.attrs = attributes;

        let result: ItemFn = single(input_fn.clone(), Default::default()).ast();
        let first_stmt = result.block.stmts.get(0).unwrap();

        let inner_fn: ItemFn = parse_quote! {
            #first_stmt
        };

        assert!(inner_fn.attrs.is_empty());
    }

    #[rstest]
    #[case::sync(false)]
    #[case::async_fn(true)]
    fn use_injected_test_attribute_to_mark_test_functions_if_any(
        #[case] is_async: bool,
        #[values(
            "#[test]",
            "#[other::test]",
            "#[very::complicated::path::test]",
            "#[prev]#[test]",
            "#[test]#[after]",
            "#[prev]#[other::test]"
        )]
        attributes: &str,
    ) {
        let attributes = attrs(attributes);
        let mut input_fn: ItemFn = r#"fn test(_s: String) {} "#.ast();
        input_fn.set_async(is_async);
        input_fn.attrs = attributes.clone();

        let result: ItemFn = single(input_fn.clone(), Default::default()).ast();

        assert_eq!(result.attrs, attributes);
    }

    #[test]
    fn use_global_await() {
        let input_fn: ItemFn = r#"fn test(a: i32, b:i32, c:i32) {} "#.ast();
        let mut info: RsTestInfo = Default::default();
        info.arguments.set_global_await(true);
        info.arguments.add_future(pat("a"));
        info.arguments.add_future(pat("b"));

        let item_fn: ItemFn = single(input_fn.clone(), info).ast();

        assert_in!(
            item_fn.block.display_code(),
            await_argument_code_string("a")
        );
        assert_in!(
            item_fn.block.display_code(),
            await_argument_code_string("b")
        );
        assert_not_in!(
            item_fn.block.display_code(),
            await_argument_code_string("c")
        );
    }

    #[test]
    fn use_selective_await() {
        let input_fn: ItemFn = r#"fn test(a: i32, b:i32, c:i32) {} "#.ast();
        let mut info: RsTestInfo = Default::default();
        info.arguments.set_future(pat("a"), FutureArg::Define);
        info.arguments.set_future(pat("b"), FutureArg::Await);

        let item_fn: ItemFn = single(input_fn.clone(), info).ast();

        assert_not_in!(
            item_fn.block.display_code(),
            await_argument_code_string("a",)
        );
        assert_in!(
            item_fn.block.display_code(),
            await_argument_code_string("b")
        );
        assert_not_in!(
            item_fn.block.display_code(),
            await_argument_code_string("c")
        );
    }

    #[test]
    fn use_ref_if_any() {
        let input_fn: ItemFn = r#"fn test(a: i32, b:i32, c:i32) {} "#.ast();
        let mut info: RsTestInfo = Default::default();
        info.arguments.set_by_ref(pat("a"));
        info.arguments.set_by_ref(pat("c"));

        let item_fn: ItemFn = single(input_fn.clone(), info).ast();

        assert_in!(
            item_fn.block.stmts.last().display_code(),
            ref_argument_code_string("a")
        );
        assert_not_in!(
            item_fn.block.stmts.last().display_code(),
            ref_argument_code_string("b")
        );
        assert_in!(
            item_fn.block.stmts.last().display_code(),
            ref_argument_code_string("c")
        );
    }

    #[test]
    fn trace_arguments_values() {
        let input_fn: ItemFn = r#"#[trace]fn test(s: String, a:i32) {} "#.ast();

        let item_fn: ItemFn = single(input_fn.clone(), Default::default()).ast();

        assert_in!(
            item_fn.block.display_code(),
            trace_argument_code_string("s")
        );
        assert_in!(
            item_fn.block.display_code(),
            trace_argument_code_string("a")
        );
    }

    #[test]
    fn trace_not_all_arguments_values() {
        let input_fn: ItemFn =
            r#"#[trace] fn test(a_trace: i32, b_no_trace:i32, c_no_trace:i32, d_trace:i32) {} "#
                .ast();

        let mut attributes = RsTestAttributes::default();
        attributes.add_notraces(vec![pat("b_no_trace"), pat("c_no_trace")]);

        let item_fn: ItemFn = single(
            input_fn.clone(),
            RsTestInfo {
                attributes,
                ..Default::default()
            },
        )
        .ast();

        assert_in!(
            item_fn.block.display_code(),
            trace_argument_code_string("a_trace")
        );
        assert_not_in!(
            item_fn.block.display_code(),
            trace_argument_code_string("b_no_trace")
        );
        assert_not_in!(
            item_fn.block.display_code(),
            trace_argument_code_string("c_no_trace")
        );
        assert_in!(
            item_fn.block.display_code(),
            trace_argument_code_string("d_trace")
        );
    }

    #[rstest]
    #[case::sync("", parse_quote! { #[test] })]
    #[case::async_fn("async", parse_quote! { #[async_std::test] })]
    fn add_default_test_attribute(
        #[case] prefix: &str,
        #[case] test_attribute: Attribute,
        #[values(
            "",
            "#[no_one]",
            "#[should_panic]",
            "#[should_panic]#[other]",
            "#[a::b::c]#[should_panic]"
        )]
        attributes: &str,
    ) {
        let attributes = attrs(attributes);
        let mut input_fn: ItemFn = format!(r#"{} fn test(_s: String) {{}} "#, prefix).ast();
        input_fn.attrs = attributes.clone();

        let result: ItemFn = single(input_fn.clone(), Default::default()).ast();

        assert_eq!(result.attrs[0], test_attribute);
        assert_eq!(&result.attrs[1..], attributes.as_slice());
    }

    #[rstest]
    #[case::sync(false, false)]
    #[case::async_fn(true, true)]
    fn use_await_for_no_async_test_function(#[case] is_async: bool, #[case] use_await: bool) {
        let mut input_fn: ItemFn = r#"fn test(_s: String) {} "#.ast();
        input_fn.set_async(is_async);

        let result: ItemFn = single(input_fn.clone(), Default::default()).ast();

        let last_stmt = result.block.stmts.last().unwrap();

        assert_eq!(use_await, last_stmt.is_await());
    }
    #[test]
    fn add_future_boilerplate_if_requested() {
        let item_fn: ItemFn = r#"
                    async fn test(async_ref_u32: &u32, async_u32: u32,simple: u32)
                    { }
                     "#
        .ast();

        let mut arguments = ArgumentsInfo::default();
        arguments.add_future(pat("async_ref_u32"));
        arguments.add_future(pat("async_u32"));

        let info = RsTestInfo {
            arguments,
            ..Default::default()
        };

        let result: ItemFn = single(item_fn.clone(), info).ast();
        let inner_fn = extract_inner_test_function(&result);

        let expected = parse_str::<syn::ItemFn>(
            r#"async fn test<'_async_ref_u32>(
                        async_ref_u32: impl std::future::Future<Output = &'_async_ref_u32 u32>, 
                        async_u32: impl std::future::Future<Output = u32>, 
                        simple: u32
                    )
                    { }
                    "#,
        )
        .unwrap();

        assert_eq!(inner_fn.sig, expected.sig);
    }
}

struct TestsGroup {
    requested_test: ItemFn,
    module: ItemMod,
}

impl Parse for TestsGroup {
    fn parse(input: ParseStream) -> Result<Self> {
        Ok(Self {
            requested_test: input.parse()?,
            module: input.parse()?,
        })
    }
}

trait QueryAttrs {
    #[allow(dead_code)]
    fn has_attr(&self, attr: &syn::Path) -> bool;
    fn has_attr_that_ends_with(&self, attr: &syn::PathSegment) -> bool;
}

impl QueryAttrs for ItemFn {
    fn has_attr(&self, attr: &syn::Path) -> bool {
        self.attrs.iter().find(|a| a.path() == attr).is_some()
    }

    fn has_attr_that_ends_with(&self, name: &syn::PathSegment) -> bool {
        self.attrs
            .iter()
            .find(|a| attr_ends_with(a, name))
            .is_some()
    }
}

/// To extract all test functions
struct TestFunctions(Vec<ItemFn>);

fn is_test_fn(item_fn: &ItemFn) -> bool {
    item_fn.has_attr_that_ends_with(&parse_quote! { test })
}

impl TestFunctions {
    fn is_test_fn(item_fn: &ItemFn) -> bool {
        is_test_fn(item_fn)
    }
}

impl<'ast> Visit<'ast> for TestFunctions {
    //noinspection RsTypeCheck
    fn visit_item_fn(&mut self, item_fn: &'ast ItemFn) {
        if Self::is_test_fn(item_fn) {
            self.0.push(item_fn.clone())
        }
    }
}

trait Named {
    fn name(&self) -> String;
}

impl Named for Ident {
    fn name(&self) -> String {
        self.to_string()
    }
}

impl Named for ItemFn {
    fn name(&self) -> String {
        self.sig.ident.name()
    }
}

impl Named for ItemMod {
    fn name(&self) -> String {
        self.ident.name()
    }
}

trait Names {
    fn names(&self) -> Vec<String>;
}

impl<T: Named> Names for Vec<T> {
    fn names(&self) -> Vec<String> {
        self.iter().map(Named::name).collect()
    }
}

trait ModuleInspector {
    fn get_all_tests(&self) -> Vec<ItemFn>;
    fn get_tests(&self) -> Vec<ItemFn>;
    fn get_modules(&self) -> Vec<ItemMod>;
}

impl ModuleInspector for ItemMod {
    fn get_tests(&self) -> Vec<ItemFn> {
        self.content
            .as_ref()
            .map(|(_, items)| {
                items
                    .iter()
                    .filter_map(|it| match it {
                        syn::Item::Fn(item_fn) if is_test_fn(item_fn) => Some(item_fn.clone()),
                        _ => None,
                    })
                    .collect()
            })
            .unwrap_or_default()
    }

    fn get_all_tests(&self) -> Vec<ItemFn> {
        let mut f = TestFunctions(vec![]);
        f.visit_item_mod(&self);
        f.0
    }

    fn get_modules(&self) -> Vec<ItemMod> {
        self.content
            .as_ref()
            .map(|(_, items)| {
                items
                    .iter()
                    .filter_map(|it| match it {
                        syn::Item::Mod(item_mod) => Some(item_mod.clone()),
                        _ => None,
                    })
                    .collect()
            })
            .unwrap_or_default()
    }
}

impl ModuleInspector for TestsGroup {
    fn get_all_tests(&self) -> Vec<ItemFn> {
        self.module.get_all_tests()
    }

    fn get_tests(&self) -> Vec<ItemFn> {
        self.module.get_tests()
    }

    fn get_modules(&self) -> Vec<ItemMod> {
        self.module.get_modules()
    }
}

#[derive(Default, Debug)]
struct Assignments(HashMap<String, syn::Expr>);

impl<'ast> Visit<'ast> for Assignments {
    //noinspection RsTypeCheck
    fn visit_local(&mut self, assign: &syn::Local) {
        match &assign {
            syn::Local {
                pat: syn::Pat::Ident(pat),
                init: Some(LocalInit { expr, .. }),
                ..
            } => {
                self.0.insert(pat.ident.to_string(), expr.as_ref().clone());
            }
            _ => {}
        }
    }
}

impl Assignments {
    pub fn collect_assignments(item_fn: &ItemFn) -> Self {
        let mut collect = Self::default();
        collect.visit_item_fn(item_fn);
        collect
    }
}

impl From<TokenStream> for TestsGroup {
    fn from(tokens: TokenStream) -> Self {
        syn::parse2::<TestsGroup>(tokens).unwrap()
    }
}

mod cases_should {

    use rstest_test::{assert_in, assert_not_in};

    use crate::parse::{
        arguments::{ArgumentsInfo, FutureArg},
        rstest::{RsTestData, RsTestItem},
    };

    use super::{assert_eq, *};

    fn into_rstest_data(item_fn: &ItemFn) -> RsTestData {
        RsTestData {
            items: fn_args_pats(item_fn)
                .cloned()
                .map(RsTestItem::CaseArgName)
                .collect(),
        }
    }

    struct TestCaseBuilder {
        item_fn: ItemFn,
        info: RsTestInfo,
    }

    impl TestCaseBuilder {
        fn new(item_fn: ItemFn) -> Self {
            let info: RsTestInfo = into_rstest_data(&item_fn).into();
            Self { item_fn, info }
        }

        fn from<S: AsRef<str>>(s: S) -> Self {
            Self::new(s.as_ref().ast())
        }

        fn set_async(mut self, is_async: bool) -> Self {
            self.item_fn.set_async(is_async);
            self
        }

        fn push_case<T: Into<TestCase>>(mut self, case: T) -> Self {
            self.info.push_case(case.into());
            self
        }

        fn extend<T: Into<TestCase>>(mut self, cases: impl Iterator<Item = T>) -> Self {
            self.info.extend(cases.map(Into::into));
            self
        }

        fn take(self) -> (ItemFn, RsTestInfo) {
            (self.item_fn, self.info)
        }

        fn add_notrace(mut self, pats: Vec<Pat>) -> Self {
            self.info.attributes.add_notraces(pats);
            self
        }
    }

    fn one_simple_case() -> (ItemFn, RsTestInfo) {
        TestCaseBuilder::from(r#"fn test(mut fix: String) { println!("user code") }"#)
            .push_case(r#"String::from("3")"#)
            .take()
    }

    fn some_simple_cases(cases: i32) -> (ItemFn, RsTestInfo) {
        TestCaseBuilder::from(r#"fn test(mut fix: String) { println!("user code") }"#)
            .extend((0..cases).map(|_| r#"String::from("3")"#))
            .take()
    }

    #[test]
    fn create_a_module_named_as_test_function() {
        let (item_fn, info) =
            TestCaseBuilder::from("fn should_be_the_module_name(mut fix: String) {}").take();

        let tokens = parametrize(item_fn, info);

        let output = TestsGroup::from(tokens);

        assert_eq!(output.module.ident, "should_be_the_module_name");
    }

    #[test]
    fn copy_user_function() {
        let (item_fn, info) = TestCaseBuilder::from(
            r#"fn should_be_the_module_name(mut fix: String) { println!("user code") }"#,
        )
        .take();

        let tokens = parametrize(item_fn.clone(), info);

        let mut output = TestsGroup::from(tokens);
        let test_impl: Stmt = output.requested_test.block.stmts.last().cloned().unwrap();

        output.requested_test.attrs = vec![];
        assert_eq!(output.requested_test.sig, item_fn.sig);
        assert_eq!(test_impl.display_code(), item_fn.block.display_code());
    }

    #[test]
    fn should_not_copy_should_panic_attribute() {
        let (item_fn, info) = TestCaseBuilder::from(
            r#"#[should_panic] fn with_should_panic(mut fix: String) { println!("user code") }"#,
        )
        .take();

        let tokens = parametrize(item_fn.clone(), info);

        let output = TestsGroup::from(tokens);

        assert!(!format!("{:?}", output.requested_test.attrs).contains("should_panic"));
    }

    #[test]
    fn should_mark_test_with_given_attributes() {
        let (item_fn, info) =
            TestCaseBuilder::from(r#"#[should_panic] #[other(value)] fn test(s: String){}"#)
                .push_case(r#"String::from("3")"#)
                .take();

        let tokens = parametrize(item_fn.clone(), info);

        let tests = TestsGroup::from(tokens).get_all_tests();

        // Sanity check
        assert!(tests.len() > 0);

        for t in tests {
            assert_eq!(item_fn.attrs, &t.attrs[1..]);
        }
    }

    #[rstest]
    #[case::empty("")]
    #[case::some_attrs("#[a]#[b::c]#[should_panic]")]
    fn should_add_attributes_given_in_the_test_case(
        #[case] fnattrs: &str,
        #[values("", "#[should_panic]", "#[first]#[second(arg)]")] case_attrs: &str,
    ) {
        let given_attrs = attrs(fnattrs);
        let case_attrs = attrs(case_attrs);
        let (mut item_fn, info) = TestCaseBuilder::from(r#"fn test(v: i32){}"#)
            .push_case(TestCase::from("42").with_attrs(case_attrs.clone()))
            .take();

        item_fn.attrs = given_attrs.clone();

        let tokens = parametrize(item_fn, info);

        let test_attrs = &TestsGroup::from(tokens).get_all_tests()[0].attrs[1..];

        let l = given_attrs.len();

        assert_eq!(case_attrs.as_slice(), &test_attrs[l..]);
        assert_eq!(given_attrs.as_slice(), &test_attrs[..l]);
    }

    #[test]
    fn mark_user_function_as_test() {
        let (item_fn, info) = TestCaseBuilder::from(
            r#"fn should_be_the_module_name(mut fix: String) { println!("user code") }"#,
        )
        .take();
        let tokens = parametrize(item_fn, info);

        let output = TestsGroup::from(tokens);

        assert_eq!(
            output.requested_test.attrs,
            vec![parse_quote! {#[cfg(test)]}]
        );
    }

    #[test]
    fn mark_module_as_test() {
        let (item_fn, info) = TestCaseBuilder::from(
            r#"fn should_be_the_module_name(mut fix: String) { println!("user code") }"#,
        )
        .take();
        let tokens = parametrize(item_fn, info);

        let output = TestsGroup::from(tokens);

        assert_eq!(output.module.attrs, vec![parse_quote! {#[cfg(test)]}]);
    }

    #[test]
    fn add_a_test_case() {
        let (item_fn, info) = one_simple_case();

        let tokens = parametrize(item_fn, info);

        let tests = TestsGroup::from(tokens).get_all_tests();

        assert_eq!(1, tests.len());
        assert!(&tests[0].sig.ident.to_string().starts_with("case_"))
    }

    #[test]
    fn add_return_type_if_any() {
        let (item_fn, info) =
            TestCaseBuilder::from("fn function(fix: String) -> Result<i32, String> { Ok(42) }")
                .push_case(r#"String::from("3")"#)
                .take();

        let tokens = parametrize(item_fn.clone(), info);

        let tests = TestsGroup::from(tokens).get_all_tests();

        assert_eq!(tests[0].sig.output, item_fn.sig.output);
    }

    #[test]
    fn not_copy_user_function() {
        let t_name = "test_name";
        let (item_fn, info) = TestCaseBuilder::from(format!(
            "fn {}(fix: String) -> Result<i32, String> {{ Ok(42) }}",
            t_name
        ))
        .push_case(r#"String::from("3")"#)
        .take();

        let tokens = parametrize(item_fn, info);

        let test = &TestsGroup::from(tokens).get_all_tests()[0];
        let inner_functions = extract_inner_functions(&test.block);

        assert_eq!(0, inner_functions.filter(|f| f.sig.ident == t_name).count());
    }

    #[test]
    fn starts_case_number_from_1() {
        let (item_fn, info) = one_simple_case();

        let tokens = parametrize(item_fn.clone(), info);

        let tests = TestsGroup::from(tokens).get_all_tests();

        assert!(
            &tests[0].sig.ident.to_string().starts_with("case_1"),
            "Should starts with case_1 but is {}",
            tests[0].sig.ident.to_string()
        )
    }

    #[test]
    fn add_all_test_cases() {
        let (item_fn, info) = some_simple_cases(5);

        let tokens = parametrize(item_fn.clone(), info);

        let tests = TestsGroup::from(tokens).get_all_tests();

        let valid_names = tests
            .iter()
            .filter(|it| it.sig.ident.to_string().starts_with("case_"));
        assert_eq!(5, valid_names.count())
    }

    #[test]
    fn left_pad_case_number_by_zeros() {
        let (item_fn, info) = some_simple_cases(1000);

        let tokens = parametrize(item_fn.clone(), info);

        let tests = TestsGroup::from(tokens).get_all_tests();

        let first_name = tests[0].sig.ident.to_string();
        let last_name = tests[999].sig.ident.to_string();

        assert!(
            first_name.ends_with("_0001"),
            "Should ends by _0001 but is {}",
            first_name
        );
        assert!(
            last_name.ends_with("_1000"),
            "Should ends by _1000 but is {}",
            last_name
        );

        let valid_names = tests
            .iter()
            .filter(|it| it.sig.ident.to_string().len() == first_name.len());
        assert_eq!(1000, valid_names.count())
    }

    #[test]
    fn use_description_if_any() {
        let (item_fn, mut info) = one_simple_case();
        let description = "show_this_description";

        if let &mut RsTestItem::TestCase(ref mut case) = &mut info.data.items[1] {
            case.description = Some(parse_str::<Ident>(description).unwrap());
        } else {
            panic!("Test case should be the second one");
        }

        let tokens = parametrize(item_fn.clone(), info);

        let tests = TestsGroup::from(tokens).get_all_tests();

        assert!(tests[0]
            .sig
            .ident
            .to_string()
            .ends_with(&format!("_{}", description)));
    }

    #[rstest]
    #[case::sync(false)]
    #[case::async_fn(true)]
    fn use_injected_test_attribute_to_mark_test_functions_if_any(
        #[case] is_async: bool,
        #[values(
            "#[test]",
            "#[other::test]",
            "#[very::complicated::path::test]",
            "#[prev]#[test]",
            "#[test]#[after]",
            "#[prev]#[other::test]"
        )]
        attributes: &str,
    ) {
        let attributes = attrs(attributes);
        let (mut item_fn, info) = TestCaseBuilder::from(r#"fn test(s: String){}"#)
            .push_case(r#"String::from("3")"#)
            .set_async(is_async)
            .take();
        item_fn.attrs = attributes.clone();
        item_fn.set_async(is_async);

        let tokens = parametrize(item_fn.clone(), info);

        let test = &TestsGroup::from(tokens).get_all_tests()[0];

        assert_eq!(attributes, test.attrs);
    }

    #[rstest]
    #[case::sync(false, parse_quote! { #[test] })]
    #[case::async_fn(true, parse_quote! { #[async_std::test] })]
    fn add_default_test_attribute(
        #[case] is_async: bool,
        #[case] test_attribute: Attribute,
        #[values(
            "",
            "#[no_one]",
            "#[should_panic]",
            "#[should_panic]#[other]",
            "#[a::b::c]#[should_panic]"
        )]
        attributes: &str,
    ) {
        let attributes = attrs(attributes);
        let (mut item_fn, info) = TestCaseBuilder::from(
            r#"fn should_be_the_module_name(mut fix: String) { println!("user code") }"#,
        )
        .push_case("42")
        .set_async(is_async)
        .take();
        item_fn.attrs = attributes.clone();

        let tokens = parametrize(item_fn, info);

        let tests = TestsGroup::from(tokens).get_all_tests();

        assert_eq!(tests[0].attrs[0], test_attribute);
        assert_eq!(&tests[0].attrs[1..], attributes.as_slice());
    }

    #[test]
    fn add_future_boilerplate_if_requested() {
        let (item_fn, mut info) = TestCaseBuilder::from(
            r#"async fn test(async_ref_u32: &u32, async_u32: u32,simple: u32) { }"#,
        )
        .take();

        let mut arguments = ArgumentsInfo::default();
        arguments.add_future(pat("async_ref_u32"));
        arguments.add_future(pat("async_u32"));

        info.arguments = arguments;

        let tokens = parametrize(item_fn.clone(), info);
        let test_function = TestsGroup::from(tokens).requested_test;

        let expected = parse_str::<syn::ItemFn>(
            r#"async fn test<'_async_ref_u32>(
                        async_ref_u32: impl std::future::Future<Output = &'_async_ref_u32 u32>, 
                        async_u32: impl std::future::Future<Output = u32>, 
                        simple: u32
                    )
                    { }
                    "#,
        )
        .unwrap();

        assert_eq!(test_function.sig, expected.sig);
    }

    #[rstest]
    #[case::sync(false, false)]
    #[case::async_fn(true, true)]
    fn use_await_for_async_test_function(#[case] is_async: bool, #[case] use_await: bool) {
        let (item_fn, info) =
            TestCaseBuilder::from(r#"fn test(mut fix: String) { println!("user code") }"#)
                .set_async(is_async)
                .push_case(r#"String::from("3")"#)
                .take();

        let tokens = parametrize(item_fn, info);

        let tests = TestsGroup::from(tokens).get_all_tests();

        let last_stmt = tests[0].block.stmts.last().unwrap();

        assert_eq!(use_await, last_stmt.is_await());
    }

    #[test]
    fn trace_arguments_value() {
        let (item_fn, info) =
            TestCaseBuilder::from(r#"#[trace] fn test(a_trace_me: i32, b_trace_me: i32) {}"#)
                .push_case(TestCase::from_iter(vec!["1", "2"]))
                .push_case(TestCase::from_iter(vec!["3", "4"]))
                .take();

        let tokens = parametrize(item_fn, info);

        let tests = TestsGroup::from(tokens).get_all_tests();

        assert!(tests.len() > 0);
        for test in tests {
            for name in &["a_trace_me", "b_trace_me"] {
                assert_in!(test.block.display_code(), trace_argument_code_string(name));
            }
        }
    }

    #[test]
    fn trace_just_some_arguments_value() {
        let (item_fn, info) =
            TestCaseBuilder::from(r#"#[trace] fn test(a_trace_me: i32, b_no_trace_me: i32, c_no_trace_me: i32, d_trace_me: i32) {}"#)
                .push_case(TestCase::from_iter(vec!["1", "2", "1", "2"]))
                .push_case(TestCase::from_iter(vec!["3", "4", "3", "4"]))
                .add_notrace(to_pats!(["b_no_trace_me", "c_no_trace_me"]))
                .take();

        let tokens = parametrize(item_fn, info);

        let tests = TestsGroup::from(tokens).get_all_tests();

        assert!(tests.len() > 0);
        for test in tests {
            for should_be_present in &["a_trace_me", "d_trace_me"] {
                assert_in!(
                    test.block.display_code(),
                    trace_argument_code_string(should_be_present)
                );
            }
            for should_not_be_present in &["b_trace_me", "c_trace_me"] {
                assert_not_in!(
                    test.block.display_code(),
                    trace_argument_code_string(should_not_be_present)
                );
            }
        }
    }

    #[test]
    fn trace_just_one_case() {
        let (item_fn, info) =
            TestCaseBuilder::from(r#"fn test(a_no_trace_me: i32, b_trace_me: i32) {}"#)
                .push_case(TestCase::from_iter(vec!["1", "2"]))
                .push_case(TestCase::from_iter(vec!["3", "4"]).with_attrs(attrs("#[trace]")))
                .add_notrace(to_pats!(["a_no_trace_me"]))
                .take();

        let tokens = parametrize(item_fn, info);

        let tests = TestsGroup::from(tokens).get_all_tests();

        assert_not_in!(
            tests[0].block.display_code(),
            trace_argument_code_string("b_trace_me")
        );
        assert_in!(
            tests[1].block.display_code(),
            trace_argument_code_string("b_trace_me")
        );
        assert_not_in!(
            tests[1].block.display_code(),
            trace_argument_code_string("a_no_trace_me")
        );
    }

    #[test]
    fn use_global_await() {
        let (item_fn, mut info) = TestCaseBuilder::from(r#"fn test(a: i32, b:i32, c:i32) {}"#)
            .push_case(TestCase::from_iter(vec!["1", "2", "3"]))
            .push_case(TestCase::from_iter(vec!["1", "2", "3"]))
            .take();
        info.arguments.set_global_await(true);
        info.arguments.add_future(pat("a"));
        info.arguments.add_future(pat("b"));

        let tokens = parametrize(item_fn, info);

        let tests = TestsGroup::from(tokens);

        let code = tests.requested_test.block.display_code();

        assert_in!(code, await_argument_code_string("a"));
        assert_in!(code, await_argument_code_string("b"));
        assert_not_in!(code, await_argument_code_string("c"));
    }

    #[test]
    fn use_selective_await() {
        let (item_fn, mut info) = TestCaseBuilder::from(r#"fn test(a: i32, b:i32, c:i32) {}"#)
            .push_case(TestCase::from_iter(vec!["1", "2", "3"]))
            .push_case(TestCase::from_iter(vec!["1", "2", "3"]))
            .take();
        info.arguments.set_future(pat("a"), FutureArg::Define);
        info.arguments.set_future(pat("b"), FutureArg::Await);

        let tokens = parametrize(item_fn, info);

        let tests = TestsGroup::from(tokens);

        let code = tests.requested_test.block.display_code();

        assert_not_in!(code, await_argument_code_string("a"));
        assert_in!(code, await_argument_code_string("b"));
        assert_not_in!(code, await_argument_code_string("c"));
    }
}

mod matrix_cases_should {
    use rstest_test::{assert_in, assert_not_in};

    use crate::parse::{
        arguments::{ArgumentsInfo, FutureArg},
        rstest::RsTestData,
    };

    /// Should test matrix tests render without take in account MatrixInfo to RsTestInfo
    /// transformation
    use super::{assert_eq, *};

    fn into_rstest_data(item_fn: &ItemFn) -> RsTestData {
        RsTestData {
            items: fn_args_pats(item_fn)
                .cloned()
                .map(|it| {
                    ValueList {
                        arg: it,
                        values: vec![],
                    }
                    .into()
                })
                .collect(),
        }
    }

    #[test]
    fn create_a_module_named_as_test_function() {
        let item_fn = "fn should_be_the_module_name(mut fix: String) {}".ast();
        let data = into_rstest_data(&item_fn);

        let tokens = matrix(item_fn.clone(), data.into());

        let output = TestsGroup::from(tokens);

        assert_eq!(output.module.ident, "should_be_the_module_name");
    }

    #[test]
    fn copy_user_function() {
        let item_fn =
            r#"fn should_be_the_module_name(mut fix: String) { println!("user code") }"#.ast();
        let data = into_rstest_data(&item_fn);

        let tokens = matrix(item_fn.clone(), data.into());

        let mut output = TestsGroup::from(tokens);
        let test_impl: Stmt = output.requested_test.block.stmts.last().cloned().unwrap();

        output.requested_test.attrs = vec![];
        assert_eq!(output.requested_test.sig, item_fn.sig);
        assert_eq!(test_impl.display_code(), item_fn.block.display_code());
    }

    #[test]
    fn not_copy_user_function() {
        let t_name = "test_name";
        let item_fn: ItemFn = format!(
            "fn {}(fix: String) -> Result<i32, String> {{ Ok(42) }}",
            t_name
        )
        .ast();
        let info = RsTestInfo {
            data: RsTestData {
                items: vec![values_list("fix", &["1"]).into()].into(),
            },
            ..Default::default()
        };

        let tokens = matrix(item_fn, info);

        let test = &TestsGroup::from(tokens).get_all_tests()[0];
        let inner_functions = extract_inner_functions(&test.block);

        assert_eq!(0, inner_functions.filter(|f| f.sig.ident == t_name).count());
    }

    #[test]
    fn not_copy_should_panic_attribute() {
        let item_fn =
            r#"#[should_panic] fn with_should_panic(mut fix: String) { println!("user code") }"#
                .ast();
        let info = RsTestInfo {
            data: RsTestData {
                items: vec![values_list("fix", &["1"]).into()].into(),
            },
            ..Default::default()
        };

        let tokens = matrix(item_fn, info);

        let output = TestsGroup::from(tokens);

        assert!(!format!("{:?}", output.requested_test.attrs).contains("should_panic"));
    }

    #[test]
    fn should_mark_test_with_given_attributes() {
        let item_fn: ItemFn = r#"#[should_panic] #[other(value)] fn test(_s: String){}"#.ast();

        let info = RsTestInfo {
            data: RsTestData {
                items: vec![values_list("fix", &["1"]).into()].into(),
            },
            ..Default::default()
        };
        let tokens = matrix(item_fn.clone(), info);

        let tests = TestsGroup::from(tokens).get_all_tests();

        // Sanity check
        assert!(tests.len() > 0);

        for t in tests {
            let end = t.attrs.len() - 1;
            assert_eq!(item_fn.attrs, &t.attrs[1..end]);
        }
    }

    #[test]
    fn add_return_type_if_any() {
        let item_fn: ItemFn = "fn function(fix: String) -> Result<i32, String> { Ok(42) }".ast();
        let info = RsTestInfo {
            data: RsTestData {
                items: vec![values_list("fix", &["1", "2", "3"]).into()].into(),
            },
            ..Default::default()
        };

        let tokens = matrix(item_fn.clone(), info);

        let tests = TestsGroup::from(tokens).get_tests();

        assert_eq!(tests[0].sig.output, item_fn.sig.output);
        assert_eq!(tests[1].sig.output, item_fn.sig.output);
        assert_eq!(tests[2].sig.output, item_fn.sig.output);
    }

    #[test]
    fn mark_user_function_as_test() {
        let item_fn =
            r#"fn should_be_the_module_name(mut fix: String) { println!("user code") }"#.ast();
        let data = into_rstest_data(&item_fn);

        let tokens = matrix(item_fn.clone(), data.into());

        let output = TestsGroup::from(tokens);

        let expected = parse2::<ItemFn>(quote! {
            #[cfg(test)]
            fn some() {}
        })
        .unwrap()
        .attrs;

        assert_eq!(expected, output.requested_test.attrs);
    }

    #[test]
    fn mark_module_as_test() {
        let item_fn =
            r#"fn should_be_the_module_name(mut fix: String) { println!("user code") }"#.ast();
        let data = into_rstest_data(&item_fn);

        let tokens = matrix(item_fn.clone(), data.into());

        let output = TestsGroup::from(tokens);

        let expected = parse2::<ItemMod>(quote! {
            #[cfg(test)]
            mod some {}
        })
        .unwrap()
        .attrs;

        assert_eq!(expected, output.module.attrs);
    }

    #[test]
    fn with_just_one_arg() {
        let arg_name = "fix";
        let info = RsTestInfo {
            data: RsTestData {
                items: vec![values_list(arg_name, &["1", "2", "3"]).into()].into(),
            },
            ..Default::default()
        };

        let item_fn = format!(r#"fn test({}: u32) {{ println!("user code") }}"#, arg_name).ast();

        let tokens = matrix(item_fn, info);

        let tests = TestsGroup::from(tokens).get_tests();

        assert_eq!(3, tests.len());
        assert!(&tests[0].sig.ident.to_string().starts_with("fix_"))
    }

    #[rstest]
    #[case::sync(false)]
    #[case::async_fn(true)]
    fn use_injected_test_attribute_to_mark_test_functions_if_any(
        #[case] is_async: bool,
        #[values(
            "#[test]",
            "#[other::test]",
            "#[very::complicated::path::test]",
            "#[prev]#[test]",
            "#[test]#[after]",
            "#[prev]#[other::test]"
        )]
        attributes: &str,
    ) {
        let attributes = attrs(attributes);
        let filter = attrs("#[allow(non_snake_case)]");
        let data = RsTestData {
            items: vec![values_list("v", &["1", "2", "3"]).into()].into(),
        };
        let mut item_fn: ItemFn = r#"fn test(v: u32) {{ println!("user code") }}"#.ast();
        item_fn.set_async(is_async);
        item_fn.attrs = attributes.clone();

        let tokens = matrix(item_fn, data.into());

        let tests = TestsGroup::from(tokens).get_all_tests();

        // Sanity check
        assert!(tests.len() > 0);

        for test in tests {
            let filtered: Vec<_> = test
                .attrs
                .into_iter()
                .filter(|a| !filter.contains(a))
                .collect();
            assert_eq!(attributes, filtered);
        }
    }

    #[rstest]
    #[case::sync(false, parse_quote! { #[test] })]
    #[case::async_fn(true, parse_quote! { #[async_std::test] })]
    fn add_default_test_attribute(
        #[case] is_async: bool,
        #[case] test_attribute: Attribute,
        #[values(
            "",
            "#[no_one]",
            "#[should_panic]",
            "#[should_panic]#[other]",
            "#[a::b::c]#[should_panic]"
        )]
        attributes: &str,
    ) {
        let attributes = attrs(attributes);
        let data = RsTestData {
            items: vec![values_list("v", &["1", "2", "3"]).into()].into(),
        };

        let mut item_fn: ItemFn = r#"fn test(v: u32) {{ println!("user code") }}"#.ast();
        item_fn.set_async(is_async);
        item_fn.attrs = attributes.clone();

        let tokens = matrix(item_fn, data.into());

        let tests = TestsGroup::from(tokens).get_all_tests();

        // Sanity check
        assert!(tests.len() > 0);

        for test in tests {
            assert_eq!(test.attrs[0], test_attribute);
            assert_eq!(&test.attrs[1..test.attrs.len() - 1], attributes.as_slice());
        }
    }

    #[test]
    fn add_future_boilerplate_if_requested() {
        let item_fn = r#"async fn test(async_ref_u32: &u32, async_u32: u32,simple: u32) { }"#.ast();

        let mut arguments = ArgumentsInfo::default();
        arguments.add_future(pat("async_ref_u32"));
        arguments.add_future(pat("async_u32"));

        let info = RsTestInfo {
            arguments,
            ..Default::default()
        };

        let tokens = matrix(item_fn, info);

        let test_function = TestsGroup::from(tokens).requested_test;

        let expected = parse_str::<syn::ItemFn>(
            r#"async fn test<'_async_ref_u32>(
                        async_ref_u32: impl std::future::Future<Output = &'_async_ref_u32 u32>, 
                        async_u32: impl std::future::Future<Output = u32>, 
                        simple: u32
                    )
                    { }
                    "#,
        )
        .unwrap();

        assert_eq!(test_function.sig, expected.sig);
    }

    #[rstest]
    fn add_allow_non_snake_case(
        #[values(
            "",
            "#[no_one]",
            "#[should_panic]",
            "#[should_panic]#[other]",
            "#[a::b::c]#[should_panic]"
        )]
        attributes: &str,
    ) {
        let attributes = attrs(attributes);
        let non_snake_case = &attrs("#[allow(non_snake_case)]")[0];
        let data = RsTestData {
            items: vec![values_list("v", &["1", "2", "3"]).into()].into(),
        };

        let mut item_fn: ItemFn = r#"fn test(v: u32) {{ println!("user code") }}"#.ast();
        item_fn.attrs = attributes.clone();

        let tokens = matrix(item_fn, data.into());

        let tests = TestsGroup::from(tokens).get_all_tests();

        // Sanity check
        assert!(tests.len() > 0);

        for test in tests {
            assert_eq!(test.attrs.last().unwrap(), non_snake_case);
            assert_eq!(&test.attrs[1..test.attrs.len() - 1], attributes.as_slice());
        }
    }

    #[rstest]
    #[case::sync(false, false)]
    #[case::async_fn(true, true)]
    fn use_await_for_async_test_function(#[case] is_async: bool, #[case] use_await: bool) {
        let data = RsTestData {
            items: vec![values_list("v", &["1", "2", "3"]).into()].into(),
        };

        let mut item_fn: ItemFn = r#"fn test(v: u32) {{ println!("user code") }}"#.ast();
        item_fn.set_async(is_async);

        let tokens = matrix(item_fn, data.into());

        let tests = TestsGroup::from(tokens).get_all_tests();

        // Sanity check
        assert!(tests.len() > 0);

        for test in tests {
            let last_stmt = test.block.stmts.last().unwrap();
            assert_eq!(use_await, last_stmt.is_await());
        }
    }

    #[test]
    fn trace_arguments_value() {
        let data = RsTestData {
            items: vec![
                values_list("a_trace_me", &["1", "2"]).into(),
                values_list("b_trace_me", &["3", "4"]).into(),
            ]
            .into(),
        };
        let item_fn: ItemFn = r#"#[trace] fn test(a_trace_me: u32, b_trace_me: u32) {}"#.ast();

        let tokens = matrix(item_fn, data.into());

        let tests = TestsGroup::from(tokens).get_all_tests();

        assert!(tests.len() > 0);
        for test in tests {
            for name in &["a_trace_me", "b_trace_me"] {
                assert_in!(test.block.display_code(), trace_argument_code_string(name));
            }
        }
    }

    #[test]
    fn trace_just_some_arguments_value() {
        let data = RsTestData {
            items: vec![
                values_list("a_trace_me", &["1", "2"]).into(),
                values_list("b_no_trace_me", &["3", "4"]).into(),
                values_list("c_no_trace_me", &["5", "6"]).into(),
                values_list("d_trace_me", &["7", "8"]).into(),
            ]
            .into(),
        };
        let mut attributes: RsTestAttributes = Default::default();
        attributes.add_notraces(vec![pat("b_no_trace_me"), pat("c_no_trace_me")]);
        let item_fn: ItemFn = r#"#[trace] fn test(a_trace_me: u32, b_no_trace_me: u32, c_no_trace_me: u32, d_trace_me: u32) {}"#.ast();

        let tokens = matrix(
            item_fn,
            RsTestInfo {
                data,
                attributes,
                ..Default::default()
            },
        );

        let tests = TestsGroup::from(tokens).get_all_tests();

        assert!(tests.len() > 0);
        for test in tests {
            for should_be_present in &["a_trace_me", "d_trace_me"] {
                assert_in!(
                    test.block.display_code(),
                    trace_argument_code_string(should_be_present)
                );
            }
            for should_not_be_present in &["b_no_trace_me", "c_no_trace_me"] {
                assert_not_in!(
                    test.block.display_code(),
                    trace_argument_code_string(should_not_be_present)
                );
            }
        }
    }

    #[test]
    fn use_global_await() {
        let item_fn: ItemFn = r#"fn test(a: i32, b:i32, c:i32) {}"#.ast();
        let data = RsTestData {
            items: vec![
                values_list("a", &["1"]).into(),
                values_list("b", &["2"]).into(),
                values_list("c", &["3"]).into(),
            ]
            .into(),
        };
        let mut info = RsTestInfo {
            data,
            attributes: Default::default(),
            arguments: Default::default(),
        };
        info.arguments.set_global_await(true);
        info.arguments.add_future(pat("a"));
        info.arguments.add_future(pat("b"));

        let tokens = matrix(item_fn, info);

        let tests = TestsGroup::from(tokens);

        let code = tests.requested_test.block.display_code();

        assert_in!(code, await_argument_code_string("a"));
        assert_in!(code, await_argument_code_string("b"));
        assert_not_in!(code, await_argument_code_string("c"));
    }

    #[test]
    fn use_selective_await() {
        let item_fn: ItemFn = r#"fn test(a: i32, b:i32, c:i32) {}"#.ast();
        let data = RsTestData {
            items: vec![
                values_list("a", &["1"]).into(),
                values_list("b", &["2"]).into(),
                values_list("c", &["3"]).into(),
            ]
            .into(),
        };
        let mut info = RsTestInfo {
            data,
            attributes: Default::default(),
            arguments: Default::default(),
        };

        info.arguments.set_future(pat("a"), FutureArg::Define);
        info.arguments.set_future(pat("b"), FutureArg::Await);

        let tokens = matrix(item_fn, info);

        let tests = TestsGroup::from(tokens);

        let code = tests.requested_test.block.display_code();

        assert_not_in!(code, await_argument_code_string("a"));
        assert_in!(code, await_argument_code_string("b"));
        assert_not_in!(code, await_argument_code_string("c"));
    }

    mod two_args_should {
        /// Should test matrix tests render without take in account MatrixInfo to RsTestInfo
        /// transformation
        use super::{assert_eq, *};

        fn fixture<'a>() -> (Vec<&'a str>, ItemFn, RsTestInfo) {
            let names = vec!["first", "second"];
            (
                names.clone(),
                format!(
                    r#"fn test({}: u32, {}: u32) {{ println!("user code") }}"#,
                    names[0], names[1]
                )
                .ast(),
                RsTestInfo {
                    data: RsTestData {
                        items: vec![
                            values_list(names[0], &["1", "2", "3"]).into(),
                            values_list(names[1], &["1", "2"]).into(),
                        ],
                    },
                    ..Default::default()
                },
            )
        }

        #[test]
        fn contain_a_module_for_each_first_arg() {
            let (names, item_fn, info) = fixture();

            let tokens = matrix(item_fn, info);

            let modules = TestsGroup::from(tokens).module.get_modules().names();

            let expected = (1..=3)
                .map(|i| format!("{}_{}", names[0], i))
                .collect::<Vec<_>>();

            assert_eq!(expected.len(), modules.len());
            for (e, m) in expected.into_iter().zip(modules.into_iter()) {
                assert_in!(m, e);
            }
        }

        #[test]
        fn annotate_modules_with_allow_non_snake_name() {
            let (_, item_fn, info) = fixture();
            let non_snake_case = &attrs("#[allow(non_snake_case)]")[0];

            let tokens = matrix(item_fn, info);

            let modules = TestsGroup::from(tokens).module.get_modules();

            for module in modules {
                assert!(module.attrs.contains(&non_snake_case));
            }
        }

        #[test]
        fn create_all_tests() {
            let (_, item_fn, info) = fixture();

            let tokens = matrix(item_fn, info);

            let tests = TestsGroup::from(tokens).module.get_all_tests().names();

            assert_eq!(6, tests.len());
        }

        #[test]
        fn create_all_modules_with_the_same_functions() {
            let (_, item_fn, info) = fixture();

            let tokens = matrix(item_fn, info);

            let tests = TestsGroup::from(tokens)
                .module
                .get_modules()
                .into_iter()
                .map(|m| m.get_tests().names())
                .collect::<Vec<_>>();

            assert_eq!(tests[0], tests[1]);
            assert_eq!(tests[1], tests[2]);
        }

        #[test]
        fn test_name_should_contain_argument_name() {
            let (names, item_fn, info) = fixture();

            let tokens = matrix(item_fn, info);

            let tests = TestsGroup::from(tokens).module.get_modules()[0]
                .get_tests()
                .names();

            let expected = (1..=2)
                .map(|i| format!("{}_{}", names[1], i))
                .collect::<Vec<_>>();

            assert_eq!(expected.len(), tests.len());
            for (e, m) in expected.into_iter().zip(tests.into_iter()) {
                assert_in!(m, e);
            }
        }
    }

    #[test]
    fn three_args_should_create_all_function_4_mods_at_the_first_level_and_3_at_the_second() {
        let (first, second, third) = ("first", "second", "third");
        let info = RsTestInfo {
            data: RsTestData {
                items: vec![
                    values_list(first, &["1", "2", "3", "4"]).into(),
                    values_list(second, &["1", "2", "3"]).into(),
                    values_list(third, &["1", "2"]).into(),
                ],
            },
            ..Default::default()
        };
        let item_fn = format!(
            r#"fn test({}: u32, {}: u32, {}: u32) {{ println!("user code") }}"#,
            first, second, third
        )
        .ast();

        let tokens = matrix(item_fn, info);

        let tg = TestsGroup::from(tokens);

        assert_eq!(24, tg.module.get_all_tests().len());
        assert_eq!(4, tg.module.get_modules().len());
        assert_eq!(3, tg.module.get_modules()[0].get_modules().len());
        assert_eq!(3, tg.module.get_modules()[3].get_modules().len());
        assert_eq!(
            2,
            tg.module.get_modules()[0].get_modules()[0]
                .get_tests()
                .len()
        );
        assert_eq!(
            2,
            tg.module.get_modules()[3].get_modules()[1]
                .get_tests()
                .len()
        );
    }

    #[test]
    fn pad_case_index() {
        let item_fn: ItemFn =
            r#"fn test(first: u32, second: u32, third: u32) { println!("user code") }"#.ast();
        let values = (1..=100).map(|i| i.to_string()).collect::<Vec<_>>();
        let info = RsTestInfo {
            data: RsTestData {
                items: vec![
                    values_list("first", values.as_ref()).into(),
                    values_list("second", values[..10].as_ref()).into(),
                    values_list("third", values[..2].as_ref()).into(),
                ],
            },
            ..Default::default()
        };

        let tokens = matrix(item_fn.clone(), info);

        let tg = TestsGroup::from(tokens);

        let mods = tg.get_modules().names();

        assert_in!(mods[0], "first_001");
        assert_in!(mods[99], "first_100");

        let mods = tg.get_modules()[0].get_modules().names();

        assert_in!(mods[0], "second_01");
        assert_in!(mods[9], "second_10");

        let functions = tg.get_modules()[0].get_modules()[1].get_tests().names();

        assert_in!(functions[0], "third_1");
        assert_in!(functions[1], "third_2");
    }
}

mod complete_should {
    use crate::parse::rstest::RsTestData;

    use super::{assert_eq, *};

    fn rendered_case(fn_name: &str) -> TestsGroup {
        let item_fn: ItemFn = format!(
            r#"         #[first]
                        #[second(arg)]
                        fn {}(
                            fix: u32,
                            a: f64, b: f32,
                            x: i32, y: i32) {{}}"#,
            fn_name
        )
        .ast();
        let data = RsTestData {
            items: vec![
                fixture("fix", &["2"]).into(),
                ident("a").into(),
                ident("b").into(),
                vec!["1f64", "2f32"]
                    .into_iter()
                    .collect::<TestCase>()
                    .into(),
                TestCase {
                    description: Some(ident("description")),
                    ..vec!["3f64", "4f32"].into_iter().collect::<TestCase>()
                }
                .with_attrs(attrs("#[third]#[forth(other)]"))
                .into(),
                values_list("x", &["12", "-2"]).into(),
                values_list("y", &["-3", "42"]).into(),
            ],
        };

        matrix(item_fn.clone(), data.into()).into()
    }

    fn test_case() -> TestsGroup {
        rendered_case("test_function")
    }

    #[test]
    fn use_function_name_as_outer_module() {
        let rendered = rendered_case("should_be_the_outer_module_name");

        assert_eq!(rendered.module.ident, "should_be_the_outer_module_name")
    }

    #[test]
    fn have_one_module_for_each_parametrized_case() {
        let rendered = test_case();

        assert_eq!(
            vec!["case_1", "case_2_description"],
            rendered
                .get_modules()
                .iter()
                .map(|m| m.ident.to_string())
                .collect::<Vec<_>>()
        );
    }

    #[test]
    fn implement_exactly_8_tests() {
        let rendered = test_case();

        assert_eq!(8, rendered.get_all_tests().len());
    }

    #[test]
    fn implement_exactly_4_tests_in_each_module() {
        let modules = test_case().module.get_modules();

        assert_eq!(4, modules[0].get_all_tests().len());
        assert_eq!(4, modules[1].get_all_tests().len());
    }

    #[test]
    fn assign_same_case_value_for_each_test() {
        let modules = test_case().module.get_modules();

        for f in modules[0].get_all_tests() {
            let assignments = Assignments::collect_assignments(&f);
            assert_eq!(assignments.0["a"], expr("1f64"));
            assert_eq!(assignments.0["b"], expr("2f32"));
        }

        for f in modules[1].get_all_tests() {
            let assignments = Assignments::collect_assignments(&f);
            assert_eq!(assignments.0["a"], expr("3f64"));
            assert_eq!(assignments.0["b"], expr("4f32"));
        }
    }

    #[test]
    fn assign_all_case_combination_in_tests() {
        let modules = test_case().module.get_modules();

        let cases = vec![("12", "-3"), ("12", "42"), ("-2", "-3"), ("-2", "42")];
        for module in modules {
            for ((x, y), f) in cases.iter().zip(module.get_all_tests().iter()) {
                let assignments = Assignments::collect_assignments(f);
                assert_eq!(assignments.0["x"], expr(x));
                assert_eq!(assignments.0["y"], expr(y));
            }
        }
    }

    #[test]
    fn mark_test_with_given_attributes() {
        let modules = test_case().module.get_modules();
        let attrs = attrs("#[first]#[second(arg)]");

        for f in modules[0].get_all_tests() {
            let end = f.attrs.len() - 1;
            assert_eq!(attrs, &f.attrs[1..end]);
        }
        for f in modules[1].get_all_tests() {
            assert_eq!(attrs, &f.attrs[1..3]);
        }
    }
    #[test]
    fn should_add_attributes_given_in_the_test_case() {
        let modules = test_case().module.get_modules();
        let attrs = attrs("#[third]#[forth(other)]");

        for f in modules[1].get_all_tests() {
            assert_eq!(attrs, &f.attrs[3..5]);
        }
    }
}
