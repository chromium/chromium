use rstest::*;

#[fixture]
pub fn fixture() -> u32 { 42 }

#[rstest]
#[should_panic]
fn should_success(fixture: u32) {
    assert_ne!(fixture, 42);
}

#[rstest]
#[should_panic]
fn should_fail(fixture: u32) {
    assert_eq!(fixture, 42);
}

#[rstest(
    expected, input,
    case(4, 5),
    case(3, 2),
    case(3, 3)
)]
#[should_panic]
fn fail(expected: i32, input: i32) {
    assert_eq!(expected, input);
}
