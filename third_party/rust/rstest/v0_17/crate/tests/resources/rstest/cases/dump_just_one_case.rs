use rstest::*;

#[rstest]
#[case::first_no_dump("Please don't trace me")]
#[trace]
#[case::dump_me("Trace it!")]
#[case::last_no_dump("Please don't trace me")]
fn cases(#[case] s: &str) {
    assert!(false);
}
