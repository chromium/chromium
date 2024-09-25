use rstest_reuse;

mod foo {
    pub(crate) mod bar {
        use rstest::rstest;
        use rstest_reuse::{self, *};

        #[template]
        #[export]
        #[rstest]
        #[case("bar")]
        fn my_template(#[case] s: &str) {}

        #[apply(my_template)]
        fn test(#[case] s: &str) {
            assert_eq!("bar", s);
        }
    }
}

use rstest::rstest;
use rstest_reuse::*;

#[apply(foo::bar::my_template)]
fn test_path(#[case] s: &str) {
    assert_eq!("bar", s);
}

use foo::bar::my_template;
#[apply(my_template)]
fn test_import(#[case] s: &str) {
    assert_eq!("bar", s);
}

#[template]
#[export]
#[rstest]
#[case("bar")]
fn root_level(#[case] s: &str) {}
