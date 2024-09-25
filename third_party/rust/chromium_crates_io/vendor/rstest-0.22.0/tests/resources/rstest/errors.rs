use rstest::*;
#[fixture]
pub fn fixture() -> u32 {
    42
}

#[rstest(f, case(42))]
fn error_inner(f: i32) {
    let a: u32 = "";
}

#[rstest(f, case(42))]
fn error_cannot_resolve_fixture(no_fixture: u32, f: u32) {}

#[rstest(f, case(42))]
fn error_fixture_wrong_type(fixture: String, f: u32) {}

#[rstest(f, case(42))]
fn error_case_wrong_type(f: &str) {}

#[rstest(condition,
    case(vec![1,2,3].contains(2)))
]
fn error_in_arbitrary_rust_code_cases(condition: bool) {
    assert!(condition)
}

#[rstest(f, case(42), not_a_fixture(24))]
fn error_inject_an_invalid_fixture(f: u32) {}

#[fixture]
fn n() -> u32 {
    24
}

#[fixture]
fn f(n: u32) -> u32 {
    2 * n
}

#[rstest(f, f(42), case(12))]
fn error_inject_a_fixture_that_is_already_a_case(f: u32) {}

#[rstest(f(42), f, case(12))]
fn error_define_case_that_is_already_an_injected_fixture(f: u32) {}

#[rstest(v, f(42), f(42), case(12))]
fn error_inject_a_fixture_more_than_once(v: u32, f: u32) {}

#[rstest(f => [42])]
fn error_matrix_wrong_type(f: &str) {}

#[rstest(condition => [vec![1,2,3].contains(2)] )]
fn error_arbitrary_rust_code_matrix(condition: bool) {
    assert!(condition)
}

#[rstest(empty => [])]
fn error_empty_list(empty: &str) {}

#[rstest(not_exist_1 => [42],
         not_exist_2 => [42])]
fn error_no_match_args() {}

#[rstest(f => [41, 42], f(42))]
fn error_inject_a_fixture_that_is_already_a_value_list(f: u32) {}

#[rstest(f(42), f => [41, 42])]
fn error_define_a_value_list_that_is_already_an_injected_fixture(f: u32) {}

#[rstest(a, case(42), a => [42])]
fn error_define_a_value_list_that_is_already_a_case_arg(a: u32) {}

#[rstest(a => [42], a, case(42))]
fn error_define_a_case_arg_that_is_already_a_value_list(a: u32) {}

#[rstest(a => [42, 24], a => [24, 42])]
fn error_define_a_value_list_that_is_already_a_value_list(f: u32) {}

#[rstest(a, a, case(42))]
fn error_define_a_case_arg_that_is_already_a_case_arg(a: u32) {}

struct S;
#[rstest]
#[case("donald duck")]
fn error_convert_to_type_that_not_implement_from_str(#[case] s: S) {}

#[rstest]
#[case(async { "hello" } )]
async fn error_future_on_impl_type(
    #[case]
    #[future]
    s: impl AsRef<str>,
) {
}

#[rstest]
#[case(async { 42 } )]
async fn error_future_more_than_once(
    #[case]
    #[future]
    #[future]
    a: i32,
) {
}

#[rstest]
#[timeout]
fn error_timeout_without_arg() {}

#[rstest]
#[timeout(some -> strange -> invalid -> expression)]
fn error_timeout_without_expression_arg() {}

#[rstest]
#[timeout(42)]
fn error_timeout_without_duration() {}

#[rstest]
fn error_absolute_path_files(#[files("/tmp/tmp.Q81idVZYAV/*.txt")] path: std::path::PathBuf) {}

struct T(u32, u32);

#[rstest]
#[case(T(3, 4))]
fn wrong_destruct_fixture(T(a, b): T, #[with(42)] T(c, d): T) {}
