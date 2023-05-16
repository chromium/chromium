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

#[rstest(::trace)]
fn single_fail(fu32: u32, fstring: String, ftuple: (A, String, i32)) {
    assert!(false);
}

#[rstest]
fn no_trace_single_fail(fu32: u32, fstring: String, ftuple: (A, String, i32)) {
    assert!(false);
}

#[rstest(u, s, t,
    case(42, "str", ("ss", -12)),
    case(24, "trs", ("tt", -24))
    ::trace
)]
fn cases_fail(u: u32, s: &str, t: (&str, i32)) {
    assert!(false);
}

#[rstest(u, s, t,
    case(42, "str", ("ss", -12)),
    case(24, "trs", ("tt", -24))
)]
fn no_trace_cases_fail(u: u32, s: &str, t: (&str, i32)) {
    assert!(false);
}

#[rstest(
    u => [1, 2],
    s => ["rst", "srt"],
    t => [("SS", -12), ("TT", -24)]
    ::trace
)]
fn matrix_fail(u: u32, s: &str, t: (&str, i32)) {
    assert!(false);
}

#[rstest(
    u => [1, 2],
    s => ["rst", "srt"],
    t => [("SS", -12), ("TT", -24)]
)]
fn no_trace_matrix_fail(u: u32, s: &str, t: (&str, i32)) {
    assert!(false);
}
