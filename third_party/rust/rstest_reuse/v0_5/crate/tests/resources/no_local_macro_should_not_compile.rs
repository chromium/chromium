use rstest_reuse;

mod foo {
    use rstest_reuse::{self, *};

    #[template]
    #[rstest]
    #[case("bar")]
    fn my_template(#[case] s: &str) {}
}
use rstest::rstest;
use rstest_reuse::apply;

#[apply(my_template)]
fn test(#[case] s: &str) {
    assert_eq!("bar", s);
}
