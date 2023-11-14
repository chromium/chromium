use rstest::*;

#[derive(Debug)]
struct A {}

#[fixture]
fn fu32() -> u32 {
    42
}
#[fixture]
fn fstring() -> String {
    "A String".to_string()
}
#[fixture]
fn ftuple() -> (A, String, i32) {
    (A {}, "A String".to_string(), -12)
}

#[rstest]
#[trace]
fn single_fail(fu32: u32, fstring: String, ftuple: (A, String, i32)) {
    assert!(false);
}

#[rstest]
fn no_trace_single_fail(fu32: u32, fstring: String, ftuple: (A, String, i32)) {
    assert!(false);
}

#[rstest]
#[case(42, "str", ("ss", -12))]
#[case(24, "trs", ("tt", -24))]
#[trace]
fn cases_fail(#[case] u: u32, #[case] s: &str, #[case] t: (&str, i32)) {
    assert!(false);
}

#[rstest]
#[case(42, "str", ("ss", -12))]
#[case(24, "trs", ("tt", -24))]
fn no_trace_cases_fail(#[case] u: u32, #[case] s: &str, #[case] t: (&str, i32)) {
    assert!(false);
}

#[rstest]
#[trace]
fn matrix_fail(
    #[values(1, 3)] u: u32,
    #[values("rst", "srt")] s: &str,
    #[values(("SS", -12), ("TT", -24))] t: (&str, i32),
) {
    assert!(false);
}

#[rstest]
fn no_trace_matrix_fail(
    #[values(1, 3)] u: u32,
    #[values("rst", "srt")] s: &str,
    #[values(("SS", -12), ("TT", -24))] t: (&str, i32),
) {
    assert!(false);
}
