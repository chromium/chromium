use quote::{format_ident, ToTokens};
use syn::{visit_mut::VisitMut, FnArg, Ident, ItemFn, PatType, Type};

use crate::{error::ErrorsVec, refident::MaybeType, utils::attr_is};

use super::{arguments::FutureArg, extract_argument_attrs};

pub(crate) fn extract_futures(
    item_fn: &mut ItemFn,
) -> Result<(Vec<(Ident, FutureArg)>, bool), ErrorsVec> {
    let mut extractor = FutureFunctionExtractor::default();
    extractor.visit_item_fn_mut(item_fn);
    extractor.take()
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

/// Simple struct used to visit function attributes and extract future args to
/// implement the boilerplate.
#[derive(Default)]
struct FutureFunctionExtractor {
    futures: Vec<(Ident, FutureArg)>,
    awt: bool,
    errors: Vec<syn::Error>,
}

impl FutureFunctionExtractor {
    pub(crate) fn take(self) -> Result<(Vec<(Ident, FutureArg)>, bool), ErrorsVec> {
        if self.errors.is_empty() {
            Ok((self.futures, self.awt))
        } else {
            Err(self.errors.into())
        }
    }
}

impl VisitMut for FutureFunctionExtractor {
    fn visit_item_fn_mut(&mut self, node: &mut ItemFn) {
        let attrs = std::mem::take(&mut node.attrs);
        let (awts, remain): (Vec<_>, Vec<_>) = attrs.into_iter().partition(|a| attr_is(a, "awt"));
        self.awt = match awts.len().cmp(&1) {
            std::cmp::Ordering::Equal => true,
            std::cmp::Ordering::Greater => {
                self.errors.extend(awts.into_iter().skip(1).map(|a| {
                    syn::Error::new_spanned(
                        a.into_token_stream(),
                        "Cannot use #[awt] more than once.".to_owned(),
                    )
                }));
                false
            }
            std::cmp::Ordering::Less => false,
        };
        node.attrs = remain;
        syn::visit_mut::visit_item_fn_mut(self, node);
    }

    fn visit_fn_arg_mut(&mut self, node: &mut FnArg) {
        if matches!(node, FnArg::Receiver(_)) {
            return;
        }
        match extract_argument_attrs(
            node,
            |a| attr_is(a, "future"),
            |arg, name| {
                let kind = if arg.tokens.is_empty() {
                    FutureArg::Define
                } else {
                    match arg.parse_args::<Option<Ident>>()? {
                        Some(awt) if awt == format_ident!("awt") => FutureArg::Await,
                        None => FutureArg::Define,
                        Some(invalid) => {
                            return Err(syn::Error::new_spanned(
                                arg.parse_args::<Option<Ident>>()?.into_token_stream(),
                                format!("Invalid '{invalid}' #[future(...)] arg."),
                            ));
                        }
                    }
                };
                Ok((arg, name.clone(), kind))
            },
        )
        .collect::<Result<Vec<_>, _>>()
        {
            Ok(futures) => match futures.len().cmp(&1) {
                std::cmp::Ordering::Equal => match node.as_future_impl_type() {
                    Some(_) => self.futures.push((futures[0].1.clone(), futures[0].2)),
                    None => self.errors.push(syn::Error::new_spanned(
                        node.maybe_type().unwrap().into_token_stream(),
                        "This type cannot used to generate impl Future.".to_owned(),
                    )),
                },
                std::cmp::Ordering::Greater => {
                    self.errors
                        .extend(futures.iter().skip(1).map(|(attr, _ident, _type)| {
                            syn::Error::new_spanned(
                                attr.into_token_stream(),
                                "Cannot use #[future] more than once.".to_owned(),
                            )
                        }));
                }
                std::cmp::Ordering::Less => {}
            },
            Err(e) => {
                self.errors.push(e);
            }
        };
    }
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

        let (futures, awt) = extract_futures(&mut item_fn).unwrap();

        assert_eq!(orig, item_fn);
        assert!(futures.is_empty());
        assert!(!awt);
    }

    #[rstest]
    #[case::simple("fn f(#[future] a: u32) {}", "fn f(a: u32) {}", &[("a", FutureArg::Define)], false)]
    #[case::global_awt("#[awt] fn f(a: u32) {}", "fn f(a: u32) {}", &[], true)]
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

        let (futures, awt) = extract_futures(&mut item_fn).unwrap();

        assert_eq!(expected, item_fn);
        assert_eq!(
            futures,
            expected_futures
                .into_iter()
                .map(|(id, a)| (ident(id), *a))
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

        let _ = extract_futures(&mut item_fn);

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

        let err = extract_futures(&mut item_fn).unwrap_err();

        assert_in!(format!("{:?}", err), message);
    }
}
