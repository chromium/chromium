use rstest::rstest;
use rstest_reuse::*;

#[template]
#[rstest(a,  b, case(2, 2), case(4/2, 2))]
fn two_simple_cases(a: u32, b: u32) {}

#[apply(two_simple_cases)]
fn it_works(a: u32, b: u32) {
    assert!(a == b);
}

#[apply(two_simple_cases)]
fn it_fail(a: u32, b: u32) {
    assert!(a != b);
}

#[apply(two_simple_cases)]
#[should_panic]
fn it_fail_but_ok(a: u32, b: u32) {
    assert!(a != b);
}
