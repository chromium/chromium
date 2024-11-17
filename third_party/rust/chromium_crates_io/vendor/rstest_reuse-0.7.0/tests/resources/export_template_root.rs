pub use rstest_reuse;
use rstest_reuse::template;

#[template]
#[export]
#[rstest]
#[case("bar")]
fn root_level(#[case] s: &str) {}
