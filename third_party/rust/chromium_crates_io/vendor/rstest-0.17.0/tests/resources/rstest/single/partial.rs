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

#[rstest]
fn default(fixture: u32) {
    assert_eq!(fixture, 0);
}

#[rstest(fixture(7))]
fn partial_1(fixture: u32) {
    assert_eq!(fixture, 7);
}

#[rstest]
fn partial_attr_1(#[with(7)] fixture: u32) {
    assert_eq!(fixture, 7);
}

#[rstest(fixture(2, 4))]
fn partial_2(fixture: u32) {
    assert_eq!(fixture, 42);
}

#[rstest]
fn partial_attr_2(#[with(2, 4)] fixture: u32) {
    assert_eq!(fixture, 42);
}

#[rstest(fixture(2, 4, 5))]
fn complete(fixture: u32) {
    assert_eq!(fixture, 542);
}

#[rstest]
fn complete_attr(#[with(2, 4, 5)] fixture: u32) {
    assert_eq!(fixture, 542);
}
