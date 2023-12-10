use rstest::*;

#[fixture]
pub fn fixture() -> u32 { 42 }

#[rstest]
fn should_success(mut fixture: u32) {
    fixture += 1;
    assert_eq!(fixture, 43);
}

#[rstest]
fn should_fail(mut fixture: u32) {
    fixture += 1;
    assert_ne!(fixture, 43);
}

#[rstest(
    expected, val,
    case(45, 1),
    case(46, 2),
    case(47, 2)
)]
fn add_test(mut fixture: u32, expected: u32, mut val: u32) {
    fixture += 1;
    val += fixture + 1;

    assert_eq!(expected, val);
}
