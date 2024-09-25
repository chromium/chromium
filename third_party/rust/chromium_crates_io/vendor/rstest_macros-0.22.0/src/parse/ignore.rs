use syn::{visit_mut::VisitMut, ItemFn, Pat};

use crate::error::ErrorsVec;

use super::just_once::JustOnceFnArgAttributeExtractor;

pub(crate) fn extract_ignores(item_fn: &mut ItemFn) -> Result<Vec<Pat>, ErrorsVec> {
    let mut extractor = JustOnceFnArgAttributeExtractor::from("ignore");
    extractor.visit_item_fn_mut(item_fn);
    extractor.take()
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
    fn not_change_anything_if_no_ignore_attribute_found(#[case] item_fn: &str) {
        let mut item_fn: ItemFn = item_fn.ast();
        let orig = item_fn.clone();

        let by_refs = extract_ignores(&mut item_fn).unwrap();

        assert_eq!(orig, item_fn);
        assert!(by_refs.is_empty());
    }

    #[rstest]
    #[case::simple("fn f(#[ignore] a: u32) {}", "fn f(a: u32) {}", &["a"])]
    #[case::more_than_one(
        "fn f(#[ignore] a: u32, #[ignore] b: String, #[ignore] c: std::collection::HashMap<usize, String>) {}",
        r#"fn f(a: u32, 
                b: String, 
                c: std::collection::HashMap<usize, String>) {}"#,
        &["a", "b", "c"])]
    fn extract(#[case] item_fn: &str, #[case] expected: &str, #[case] expected_refs: &[&str]) {
        let mut item_fn: ItemFn = item_fn.ast();
        let expected: ItemFn = expected.ast();

        let by_refs = extract_ignores(&mut item_fn).unwrap();

        assert_eq!(expected, item_fn);
        assert_eq!(by_refs, to_pats!(expected_refs));
    }

    #[test]
    fn raise_error() {
        let mut item_fn: ItemFn = "fn f(#[ignore] #[ignore] a: u32) {}".ast();

        let err = extract_ignores(&mut item_fn).unwrap_err();

        assert_in!(format!("{:?}", err), "more than once");
    }
}
