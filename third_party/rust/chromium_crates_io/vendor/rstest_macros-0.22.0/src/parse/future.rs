use quote::{format_ident, ToTokens};
use syn::{visit_mut::VisitMut, FnArg, Ident, ItemFn, Pat, PatType, Type};

use crate::{error::ErrorsVec, refident::MaybeType};

use super::{
    arguments::FutureArg,
    just_once::{
        AttrBuilder, JustOnceFnArgAttributeExtractor, JustOnceFnAttributeExtractor, Validator,
    },
};

pub(crate) fn extract_futures(item_fn: &mut ItemFn) -> Result<Vec<(Pat, FutureArg)>, ErrorsVec> {
    let mut extractor = JustOnceFnArgAttributeExtractor::<FutureBuilder>::new("future");

    extractor.visit_item_fn_mut(item_fn);
    extractor.take()
}

pub(crate) fn extract_global_awt(item_fn: &mut ItemFn) -> Result<bool, ErrorsVec> {
    let mut extractor = JustOnceFnAttributeExtractor::<GlobalAwtBuilder>::new("awt");

    extractor.visit_item_fn_mut(item_fn);
    extractor.take().map(|inner| inner.is_some())
}

struct GlobalAwtBuilder;

impl AttrBuilder<ItemFn> for GlobalAwtBuilder {
    type Out = ();

    fn build(_attr: syn::Attribute, _ident: &ItemFn) -> syn::Result<Self::Out> {
        Ok(())
    }
}

impl Validator<ItemFn> for GlobalAwtBuilder {}

struct FutureBuilder;

impl AttrBuilder<Pat> for FutureBuilder {
    type Out = (Pat, FutureArg);

    fn build(attr: syn::Attribute, pat: &Pat) -> syn::Result<Self::Out> {
        Self::compute_arguments_kind(&attr).map(|kind| (pat.clone(), kind))
    }
}

impl Validator<FnArg> for FutureBuilder {
    fn validate(arg: &FnArg) -> syn::Result<()> {
        arg.as_future_impl_type().map(|_| ()).ok_or_else(|| {
            syn::Error::new_spanned(
                arg.maybe_type().unwrap().into_token_stream(),
                "This type cannot used to generate impl Future.".to_owned(),
            )
        })
    }
}

impl FutureBuilder {
    fn compute_arguments_kind(arg: &syn::Attribute) -> syn::Result<FutureArg> {
        if matches!(arg.meta, syn::Meta::Path(_)) {
            Ok(FutureArg::Define)
        } else {
            match arg.parse_args::<Option<Ident>>()? {
                Some(awt) if awt == format_ident!("awt") => Ok(FutureArg::Await),
                None => Ok(FutureArg::Define),
                Some(invalid) => Err(syn::Error::new_spanned(
                    arg.parse_args::<Option<Ident>>()?.into_token_stream(),
                    format!("Invalid '{invalid}' #[future(...)] arg."),
                )),
            }
        }
    }
}

pub(crate) trait MaybeFutureImplType {
    fn as_future_impl_type(&self) -> Option<&Type>;

    fn as_mut_future_impl_type(&mut self) -> Option<&mut Type>;
}

impl MaybeFutureImplType for FnArg {
    fn as_future_impl_type(&self) -> Option<&Type> {
        match self {
            FnArg::Typed(PatType { ty, .. }) if can_impl_future(ty.as_ref()) => Some(ty.as_ref()),
            _ => None,
        }
    }

    fn as_mut_future_impl_type(&mut self) -> Option<&mut Type> {
        match self {
            FnArg::Typed(PatType { ty, .. }) if can_impl_future(ty.as_ref()) => Some(ty.as_mut()),
            _ => None,
        }
    }
}

fn can_impl_future(ty: &Type) -> bool {
    use Type::*;
    !matches!(
        ty,
        Group(_)
            | ImplTrait(_)
            | Infer(_)
            | Macro(_)
            | Never(_)
            | Slice(_)
            | TraitObject(_)
            | Verbatim(_)
    )
}

#[cfg(test)]
mod should {
    use super::*;
    use crate::test::{assert_eq, *};
    use rstest_test::assert_in;

    #[rstest]
    #[case("fn simple(a: u32) {}")]
    #[case("fn more(a: u32, b: &str) {}")]
    #[case("fn gen<S: AsRef<str>>(a: u32, b: S) {}")]
    #[case("fn attr(#[case] a: u32, #[values(1,2)] b: i32) {}")]
    fn not_change_anything_if_no_future_attribute_found(#[case] item_fn: &str) {
        let mut item_fn: ItemFn = item_fn.ast();
        let orig = item_fn.clone();

        let composed_tuple!(futures, awt) = merge_errors!(
            extract_futures(&mut item_fn),
            extract_global_awt(&mut item_fn)
        )
        .unwrap();

        assert_eq!(orig, item_fn);
        assert!(futures.is_empty());
        assert!(!awt);
    }

    #[rstest]
    #[case::simple("fn f(#[future] a: u32) {}", "fn f(a: u32) {}", &[("a", FutureArg::Define)], false)]
    #[case::global_awt("#[awt] fn f(a: u32) {}", "fn f(a: u32) {}", &[], true)]
    #[case::global_awt_with_inner_function("#[awt] fn f(a: u32) { fn g(){} }", "fn f(a: u32) { fn g(){} }", &[], true)]
    #[case::simple_awaited("fn f(#[future(awt)] a: u32) {}", "fn f(a: u32) {}", &[("a", FutureArg::Await)], false)]
    #[case::simple_awaited_and_global("#[awt] fn f(#[future(awt)] a: u32) {}", "fn f(a: u32) {}", &[("a", FutureArg::Await)], true)]
    #[case::more_than_one(
        "fn f(#[future] a: u32, #[future(awt)] b: String, #[future()] c: std::collection::HashMap<usize, String>) {}",
        r#"fn f(a: u32, 
                b: String, 
                c: std::collection::HashMap<usize, String>) {}"#,
        &[("a", FutureArg::Define), ("b", FutureArg::Await), ("c", FutureArg::Define)],
        false,
    )]
    #[case::just_one(
        "fn f(a: u32, #[future] b: String) {}",
        r#"fn f(a: u32, b: String) {}"#,
        &[("b", FutureArg::Define)],
        false,
    )]
    #[case::just_one_awaited(
        "fn f(a: u32, #[future(awt)] b: String) {}",
        r#"fn f(a: u32, b: String) {}"#,
        &[("b", FutureArg::Await)],
        false,
    )]
    fn extract(
        #[case] item_fn: &str,
        #[case] expected: &str,
        #[case] expected_futures: &[(&str, FutureArg)],
        #[case] expected_awt: bool,
    ) {
        let mut item_fn: ItemFn = item_fn.ast();
        let expected: ItemFn = expected.ast();

        let composed_tuple!(futures, awt) = merge_errors!(
            extract_futures(&mut item_fn),
            extract_global_awt(&mut item_fn)
        )
        .unwrap();

        assert_eq!(expected, item_fn);
        assert_eq!(
            futures,
            expected_futures
                .into_iter()
                .map(|(id, a)| (pat(id), *a))
                .collect::<Vec<_>>()
        );
        assert_eq!(expected_awt, awt);
    }

    #[rstest]
    #[case::base(r#"#[awt] fn f(a: u32) {}"#, r#"fn f(a: u32) {}"#)]
    #[case::two(
        r#"
        #[awt]
        #[awt] 
        fn f(a: u32) {}
        "#,
        r#"fn f(a: u32) {}"#
    )]
    #[case::inner(
        r#"
        #[one]
        #[awt] 
        #[two]
        fn f(a: u32) {}
        "#,
        r#"
        #[one]
        #[two]
        fn f(a: u32) {}
        "#
    )]
    fn remove_all_awt_attributes(#[case] item_fn: &str, #[case] expected: &str) {
        let mut item_fn: ItemFn = item_fn.ast();
        let expected: ItemFn = expected.ast();

        let _ = extract_global_awt(&mut item_fn);

        assert_eq!(item_fn, expected);
    }

    #[rstest]
    #[case::no_more_than_one("fn f(#[future] #[future] a: u32) {}", "more than once")]
    #[case::no_impl("fn f(#[future] a: impl AsRef<str>) {}", "generate impl Future")]
    #[case::no_slice("fn f(#[future] a: [i32]) {}", "generate impl Future")]
    #[case::invalid_arg("fn f(#[future(other)] a: [i32]) {}", "Invalid 'other'")]
    #[case::no_more_than_one_awt("#[awt] #[awt] fn f(a: u32) {}", "more than once")]
    fn raise_error(#[case] item_fn: &str, #[case] message: &str) {
        let mut item_fn: ItemFn = item_fn.ast();

        let err = merge_errors!(
            extract_futures(&mut item_fn),
            extract_global_awt(&mut item_fn)
        )
        .unwrap_err();

        assert_in!(format!("{:?}", err), message);
    }
}
