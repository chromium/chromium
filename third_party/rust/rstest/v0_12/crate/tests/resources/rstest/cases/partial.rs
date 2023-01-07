use rstest::*;

#[fixture]
fn f1() -> u32 {
    0
}
#[fixture]
fn f2() -> u32 {
    0
}
#[fixture]
fn f3() -> u32 {
    0
}

#[fixture]
fn fixture(f1: u32, f2: u32, f3: u32) -> u32 {
    f1 + 10 * f2 + 100 * f3
}

#[rstest(expected, case(0), case(1000))]
fn default(fixture: u32, expected: u32) {
    assert_eq!(fixture, expected);
}

#[rstest(fixture(7), expected, case(7), case(1000))]
fn partial_1(fixture: u32, expected: u32) {
    assert_eq!(fixture, expected);
}

#[rstest(expected, case(7), case(1000))]
fn partial_attr_1(#[with(7)] fixture: u32, expected: u32) {
    assert_eq!(fixture, expected);
}

#[rstest(fixture(2, 4), expected, case(42), case(1000))]
fn partial_2(fixture: u32, expected: u32) {
    assert_eq!(fixture, expected);
}

#[rstest(expected, case(42), case(1000))]
fn partial_attr_2(#[with(2, 4)] fixture: u32, expected: u32) {
    assert_eq!(fixture, expected);
}

#[rstest(fixture(2, 4, 5), expected, case(542), case(1000))]
fn complete(fixture: u32, expected: u32) {
    assert_eq!(fixture, expected);
}

#[rstest(expected, case(542), case(1000))]
fn complete_attr(#[with(2, 4, 5)] fixture: u32, expected: u32) {
    assert_eq!(fixture, expected);
}
