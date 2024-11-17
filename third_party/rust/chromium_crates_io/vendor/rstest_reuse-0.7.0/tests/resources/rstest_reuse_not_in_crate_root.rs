use rstest::rstest;
use rstest_reuse::apply;
use rstest_reuse::template;

#[template]
#[rstest]
#[case(42)]
fn test_template(#[case] n: u32) {}

#[apply(test_template)]
fn test(#[case] n: u32) {
    assert_eq!(n, 42);
}
