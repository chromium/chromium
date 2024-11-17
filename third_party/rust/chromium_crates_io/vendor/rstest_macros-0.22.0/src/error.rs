/// Module for error rendering stuff
use std::collections::HashMap;

use proc_macro2::TokenStream;
use syn::{spanned::Spanned, visit::Visit};
use syn::{visit, ItemFn, Pat};

use crate::parse::{
    fixture::FixtureInfo,
    rstest::{RsTestData, RsTestInfo},
};
use crate::refident::{MaybeIdent, MaybePat};

use super::utils::fn_args_has_pat;

pub mod messages {
    pub const DESTRUCT_WITHOUT_FROM : &str = "To destruct a fixture you should provide a path to resolve it by '#[from(...)]' attribute.";
    pub fn use_more_than_once(name: &str) -> String {
        format!("You cannot use '{name}' attribute more than once for the same argument")
    }
}

pub(crate) fn rstest(test: &ItemFn, info: &RsTestInfo) -> TokenStream {
    missed_arguments(test, info.data.items.iter())
        .chain(duplicate_arguments(info.data.items.iter()))
        .chain(invalid_cases(&info.data))
        .chain(case_args_without_cases(&info.data))
        .chain(destruct_fixture_without_from(test, info))
        .map(|e| e.to_compile_error())
        .collect()
}

pub(crate) fn fixture(test: &ItemFn, info: &FixtureInfo) -> TokenStream {
    missed_arguments(test, info.data.items.iter())
        .chain(duplicate_arguments(info.data.items.iter()))
        .chain(async_once(test, info))
        .chain(generics_once(test, info))
        .chain(destruct_fixture_without_from(test, info))
        .map(|e| e.to_compile_error())
        .collect()
}

fn async_once<'a>(test: &'a ItemFn, info: &FixtureInfo) -> Errors<'a> {
    match (test.sig.asyncness, info.arguments.get_once()) {
        (Some(_asyncness), Some(once)) => Box::new(std::iter::once(syn::Error::new_spanned(
            once,
            "Cannot apply #[once] to async fixture.",
        ))),
        _ => Box::new(std::iter::empty()),
    }
}

#[derive(Default)]
struct SearchImpl(bool);

impl<'ast> Visit<'ast> for SearchImpl {
    fn visit_type(&mut self, i: &'ast syn::Type) {
        if self.0 {
            return;
        }
        if let syn::Type::ImplTrait(_) = i {
            self.0 = true
        }
        visit::visit_type(self, i);
    }
}

impl SearchImpl {
    fn function_has_some_impl(f: &ItemFn) -> bool {
        let mut s = SearchImpl::default();
        visit::visit_item_fn(&mut s, f);
        s.0
    }
}

fn has_some_generics(test: &ItemFn) -> bool {
    !test.sig.generics.params.is_empty() || SearchImpl::function_has_some_impl(test)
}

fn generics_once<'a>(test: &'a ItemFn, info: &FixtureInfo) -> Errors<'a> {
    match (has_some_generics(test), info.arguments.get_once()) {
        (true, Some(once)) => Box::new(std::iter::once(syn::Error::new_spanned(
            once,
            "Cannot apply #[once] on generic fixture.",
        ))),
        _ => Box::new(std::iter::empty()),
    }
}

trait IsImplicitFixture {
    fn is_implicit_fixture(&self, pat: &Pat) -> bool;
}

impl IsImplicitFixture for FixtureInfo {
    fn is_implicit_fixture(&self, pat: &Pat) -> bool {
        !self.data.fixtures().any(|f| &f.arg == pat)
    }
}

impl IsImplicitFixture for RsTestInfo {
    fn is_implicit_fixture(&self, pat: &Pat) -> bool {
        !self.data.case_args().any(|a| a == pat)
            && !self.data.list_values().any(|a| &a.arg == pat)
            && !self.data.fixtures().any(|f| &f.arg == pat)
    }
}

fn destruct_fixture_without_from<'a>(
    function: &'a ItemFn,
    info: &'a impl IsImplicitFixture,
) -> Errors<'a> {
    Box::new(
        function
            .sig
            .inputs
            .iter()
            .filter_map(|a| a.maybe_pat().map(|p| (a, p)))
            .filter(|&(_, p)| p.maybe_ident().is_none())
            .filter(|&(_, p)| info.is_implicit_fixture(p))
            .map(|(a, _)| syn::Error::new_spanned(a, messages::DESTRUCT_WITHOUT_FROM)),
    )
}

#[derive(Debug, Default)]
pub struct ErrorsVec(Vec<syn::Error>);

pub(crate) fn _merge_errors<R1, R2>(
    r1: Result<R1, ErrorsVec>,
    r2: Result<R2, ErrorsVec>,
) -> Result<(R1, R2), ErrorsVec> {
    match (r1, r2) {
        (Ok(r1), Ok(r2)) => Ok((r1, r2)),
        (Ok(_), Err(e)) | (Err(e), Ok(_)) => Err(e),
        (Err(mut e1), Err(mut e2)) => {
            e1.append(&mut e2);
            Err(e1)
        }
    }
}

macro_rules! merge_errors {
    ($e:expr) => {
        $e
    };
    ($e:expr, $($es:expr), +) => {
        crate::error::_merge_errors($e, merge_errors!($($es),*))
    };
}

macro_rules! composed_tuple {
    ($i:ident) => {
        $i
    };
    ($i:ident, $($is:ident), +) => {
        ($i, composed_tuple!($($is),*))
    };
}

impl std::ops::Deref for ErrorsVec {
    type Target = Vec<syn::Error>;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl std::ops::DerefMut for ErrorsVec {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl From<syn::Error> for ErrorsVec {
    fn from(errors: syn::Error) -> Self {
        vec![errors].into()
    }
}

impl From<Vec<syn::Error>> for ErrorsVec {
    fn from(errors: Vec<syn::Error>) -> Self {
        Self(errors)
    }
}

impl From<ErrorsVec> for Vec<syn::Error> {
    fn from(v: ErrorsVec) -> Self {
        v.0
    }
}

impl quote::ToTokens for ErrorsVec {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        tokens.extend(self.0.iter().map(|e| e.to_compile_error()))
    }
}

impl From<ErrorsVec> for proc_macro::TokenStream {
    fn from(v: ErrorsVec) -> Self {
        use quote::ToTokens;
        v.into_token_stream().into()
    }
}

type Errors<'a> = Box<dyn Iterator<Item = syn::Error> + 'a>;

fn missed_arguments<'a, I: MaybePat + Spanned + 'a>(
    test: &'a ItemFn,
    args: impl Iterator<Item = &'a I> + 'a,
) -> Errors<'a> {
    Box::new(
        args.filter_map(|it| it.maybe_pat().map(|pat| (it, pat)))
            .filter(move |(_, pat)| !fn_args_has_pat(test, pat))
            .map(|(missed, pat)| {
                syn::Error::new(
                    missed.span(),
                    format!(
                        "Missed argument: '{}' should be a test function argument.",
                        pat.render_type()
                    ),
                )
            }),
    )
}

fn duplicate_arguments<'a, I: MaybePat + Spanned + 'a>(
    args: impl Iterator<Item = &'a I> + 'a,
) -> Errors<'a> {
    let mut used = HashMap::new();
    Box::new(
        args.filter_map(|it| it.maybe_pat().map(|pat| (it, pat)))
            .filter_map(move |(it, pat)| {
                let is_duplicate = used.contains_key(&pat);
                used.insert(pat, it);
                match is_duplicate {
                    true => Some((it, pat)),
                    false => None,
                }
            })
            .map(|(duplicate, pat)| {
                syn::Error::new(
                    duplicate.span(),
                    format!(
                        "Duplicate argument: '{}' is already defined.",
                        pat.render_type()
                    ),
                )
            }),
    )
}

fn invalid_cases(params: &RsTestData) -> Errors {
    let n_args = params.case_args().count();
    Box::new(
        params
            .cases()
            .filter(move |case| case.args.len() != n_args)
            .map(|case| {
                syn::Error::new_spanned(
                    case,
                    "Wrong case signature: should match the given parameters list.",
                )
            }),
    )
}

fn case_args_without_cases(params: &RsTestData) -> Errors {
    if !params.has_cases() {
        return Box::new(
            params
                .case_args()
                .map(|a| syn::Error::new(a.span(), "No cases for this argument.")),
        );
    }
    Box::new(std::iter::empty())
}

trait RenderType {
    fn render_type(&self) -> String;
}

impl RenderType for syn::Pat {
    fn render_type(&self) -> String {
        match self {
            syn::Pat::Ident(ref i) => i.ident.to_string(),
            other => format!("{other:?}"),
        }
    }
}

#[cfg(test)]
mod test {
    use crate::{
        parse::ExtendWithFunctionAttrs,
        test::{assert_eq, *},
    };
    use rstest_test::assert_in;

    use super::*;

    #[rstest]
    #[case::generics("fn f<G: SomeTrait>(){}")]
    #[case::const_generics("fn f<const N: usize>(){}")]
    #[case::lifetimes("fn f<'a>(){}")]
    #[case::use_impl_in_answer("fn f() -> impl Iterator<Item=u32>{}")]
    #[case::use_impl_in_arguments("fn f(it: impl Iterator<Item=u32>){}")]
    #[should_panic]
    #[case::sanity_check_with_no_generics("fn f() {}")]
    fn generics_once_should_return_error(#[case] f: &str) {
        let f: ItemFn = f.ast();
        let info = FixtureInfo::default().with_once();

        let errors = generics_once(&f, &info);

        let out = errors
            .map(|e| format!("{:?}", e))
            .collect::<Vec<_>>()
            .join("-----------------------\n");

        assert_in!(out, "Cannot apply #[once] on generic fixture.");
    }

    #[rstest]
    #[case::generics("fn f<G: SomeTrait>(){}")]
    #[case::const_generics("fn f<const N: usize>(){}")]
    #[case::lifetimes("fn f<'a>(){}")]
    #[case::use_impl_in_answer("fn f() -> impl Iterator<Item=u32>{}")]
    #[case::use_impl_in_arguments("fn f(it: impl Iterator<Item=u32>){}")]
    fn generics_once_should_not_return_if_no_once(#[case] f: &str) {
        let f: ItemFn = f.ast();
        let info = FixtureInfo::default();

        let errors = generics_once(&f, &info);

        assert_eq!(0, errors.count());
    }

    #[rstest]
    #[case::base_in_fixture("fn f(T{a}: T){}", FixtureInfo::default(), 1)]
    #[case::one_of_two("fn f(T{a}: T, #[from(f)] T{a: c}: T){}", FixtureInfo::default(), 1)]
    #[case::find_all(
        "fn f(T{a}: T, z: u32, S(a,b): S, x: u32, (f , g, h): (u32, String, f32)){}",
        FixtureInfo::default(),
        3
    )]
    #[case::base_in_test("fn f(T{a}: T){}", RsTestInfo::default(), 1)]
    #[case::not_case_or_values(
        "fn f(#[case] T{a}: T, #[values(T::a(),T::b())] T{v}: T, S{e}: S){}",
        RsTestInfo::default(),
        1
    )]
    #[case::mixed_more(
        r#"fn wrong_destruct_fixture(
            T(a, b): T,
            #[case] T(e, f): T,
            #[values(T(1, 2))] T(g, h): T,
        ) {}"#,
        RsTestInfo::default(),
        1
    )]
    fn destruct_implicit_from_should_return_error(
        #[case] f: &str,
        #[case] mut info: impl ExtendWithFunctionAttrs + IsImplicitFixture,
        #[case] n: usize,
    ) {
        let mut f: ItemFn = f.ast();

        info.extend_with_function_attrs(&mut f).unwrap();

        let errors = destruct_fixture_without_from(&f, &info);

        let out = errors
            .map(|e| format!("{:?}", e))
            .collect::<Vec<_>>()
            .join("\n");

        assert_in!(out, messages::DESTRUCT_WITHOUT_FROM);
        assert_eq!(n, out.lines().count())
    }

    #[rstest]
    #[case::happy_fixture("fn f(#[from(b)] T{a}: T){}", FixtureInfo::default())]
    #[case::happy_test("fn f(#[from(b)] T{a}: T){}", RsTestInfo::default())]
    #[case::some_cases_or_values(
        "fn f(#[case] T{a}: T, #[values(T::a(),T::b())] T{v}: T){}",
        RsTestInfo::default()
    )]
    fn destruct_not_implicit_should_not_return_error(
        #[case] f: &str,
        #[case] mut info: impl ExtendWithFunctionAttrs + IsImplicitFixture,
    ) {
        let mut f: ItemFn = f.ast();

        info.extend_with_function_attrs(&mut f).unwrap();

        let errors = destruct_fixture_without_from(&f, &info);

        assert_eq!(0, errors.count());
    }
}
