use rstest::*;

#[fixture]
fn f1() -> u32 { 0 }
#[fixture]
fn f2() -> u32 { 0 }
#[fixture]
fn f3() -> u32 { 0 }

#[fixture]
fn fixture(f1: u32, f2: u32, f3: u32) -> u32 { f1 + 10 * f2 + 100 * f3 }

#[fixture(fixture(7))]
fn partial_1(fixture: u32) -> u32 { fixture }

#[fixture(fixture(2, 4))]
fn partial_2(fixture: u32) -> u32 { fixture }

#[fixture(fixture(2, 4, 5))]
fn complete(fixture: u32) -> u32 { fixture }

#[rstest]
fn default(fixture: u32) {
    assert_eq!(fixture, 0);
}

#[rstest]
fn t_partial_1(partial_1: u32) {
    assert_eq!(partial_1, 7);
}

#[rstest]
fn t_partial_2(partial_2: u32) {
    assert_eq!(partial_2, 42);
}

#[rstest]
fn t_complete(complete: u32) {
    assert_eq!(complete, 542);
}
