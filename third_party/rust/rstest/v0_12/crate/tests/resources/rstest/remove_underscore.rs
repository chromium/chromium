use rstest::*;

#[fixture]
fn can_be_ignored() {}

#[rstest]
fn ignore_input(_can_be_ignored: ()) {
    assert!(true);
}
