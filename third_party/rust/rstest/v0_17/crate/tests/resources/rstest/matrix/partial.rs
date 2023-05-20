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
    f1 + f2 + 2 * f3
}

#[rstest(a => [0, 1], b => [0, 2])]
fn default(fixture: u32, a: u32, b: u32) {
    assert_eq!(fixture, a * b);
}

#[rstest(a => [0, 1], b => [0, 2], fixture(1))]
fn partial_1(fixture: u32, a: u32, b: u32) {
    assert_eq!(fixture, a * b);
}

#[rstest(a => [0, 1], b => [0, 2])]
fn partial_attr_1(#[with(1)] fixture: u32, a: u32, b: u32) {
    assert_eq!(fixture, a * b);
}

#[rstest(a => [0, 1], b => [0, 2], fixture(0, 2))]
fn partial_2(fixture: u32, a: u32, b: u32) {
    assert_eq!(fixture, a * b);
}

#[rstest(a => [0, 1], b => [0, 2])]
fn partial_attr_2(#[with(0, 2)] fixture: u32, a: u32, b: u32) {
    assert_eq!(fixture, a * b);
}

#[rstest(a => [0, 1], b => [0, 2], fixture(0, 0, 1))]
fn complete(fixture: u32, a: u32, b: u32) {
    assert_eq!(fixture, a * b);
}

#[rstest(a => [0, 1], b => [0, 2])]
fn complete_attr(#[with(0, 0, 1)] fixture: u32, a: u32, b: u32) {
    assert_eq!(fixture, a * b);
}
