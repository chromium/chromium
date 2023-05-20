use export_template_root::root_level;
use rstest::*;
use rstest_reuse::apply;

#[apply(root_level)]
fn test(#[case] s: &str) {
    assert_eq!("bar", s);
}
