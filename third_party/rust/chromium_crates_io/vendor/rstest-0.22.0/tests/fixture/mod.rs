use std::path::Path;
pub use unindent::Unindent;

use super::resources;
use mytest::*;
use rstest_test::{assert_in, assert_not_in, Project, Stringable, TestResults};

fn prj(res: &str) -> Project {
    let path = Path::new("fixture").join(res);
    crate::prj().set_code_file(resources(path))
}

fn run_test(res: &str) -> (std::process::Output, String) {
    let prj = prj(res);
    (
        prj.run_tests().unwrap(),
        prj.get_name().to_owned().to_string(),
    )
}

mod should {
    use rstest_test::{assert_regex, CountMessageOccurrence};

    use super::*;

    #[test]
    fn use_input_fixtures() {
        let (output, _) = run_test("simple_injection.rs");

        TestResults::new().ok("success").fail("fail").assert(output);
    }

    #[test]
    fn create_a_struct_that_return_the_fixture() {
        let (output, _) = run_test("fixture_struct.rs");

        TestResults::new()
            .ok("resolve_new")
            .ok("resolve_default")
            .ok("injected_new")
            .ok("injected_default")
            .assert(output);
    }

    #[test]
    fn be_accessible_from_other_module() {
        let (output, _) = run_test("from_other_module.rs");

        TestResults::new().ok("struct_access").assert(output);
    }

    #[test]
    fn not_show_any_warning() {
        let (output, _) = run_test("no_warning.rs");

        assert_not_in!(output.stderr.str(), "warning:");
    }

    #[test]
    fn rename() {
        let (output, _) = run_test("rename.rs");

        TestResults::new().ok("test").assert(output);
    }

    mod accept_and_return {
        use super::*;

        #[test]
        fn impl_traits() {
            let (output, _) = run_test("impl.rs");

            TestResults::new()
                .ok("base_impl_return")
                .ok("nested_impl_return")
                .ok("nested_multiple_impl_return")
                .ok("base_impl_input")
                .ok("nested_impl_input")
                .ok("nested_multiple_impl_input")
                .assert(output);
        }

        #[test]
        fn dyn_traits() {
            let (output, _) = run_test("dyn.rs");

            TestResults::new()
                .ok("test_dyn_box")
                .ok("test_dyn_ref")
                .ok("test_dyn_box_resolve")
                .ok("test_dyn_ref_resolve")
                .assert(output);
        }
    }

    #[rstest]
    #[case::base("async_fixture.rs")]
    #[case::use_global("await_complete_fixture.rs")]
    #[case::use_selective("await_partial_fixture.rs")]
    fn resolve_async_fixture(#[case] code: &str) {
        let prj = prj(code);
        prj.add_dependency("async-std", r#"{version="*", features=["attributes"]}"#);

        let output = prj.run_tests().unwrap();

        TestResults::new()
            .ok("default_is_async")
            .ok("use_async_fixture")
            .ok("use_async_impl_output")
            .ok("use_async_nest_fixture_default")
            .ok("use_async_nest_fixture_injected")
            .ok("use_async_nest_fixture_with_default")
            .ok("use_two_args_mix_fixture")
            .ok("use_two_args_mix_fixture_inject_first")
            .ok("use_two_args_mix_fixture_inject_both")
            .assert(output);
    }

    #[test]
    fn resolve_fixture_generics_by_fixture_input() {
        let (output, _) = run_test("resolve.rs");

        TestResults::new()
            .ok("test_u32")
            .ok("test_i32")
            .assert(output);
    }

    #[test]
    fn use_defined_return_type_if_any() {
        let (output, _) = run_test("defined_return_type.rs");

        TestResults::new()
            .ok("resolve")
            .ok("resolve_partial")
            .ok("resolve_attrs")
            .ok("resolve_partial_attrs")
            .assert(output);
    }

    #[test]
    fn clean_up_default_from_unused_generics() {
        let (output, _) = run_test("clean_up_default_generics.rs");

        TestResults::new()
            .ok("resolve")
            .ok("resolve_partial")
            .assert(output);
    }

    #[test]
    fn apply_partial_fixture() {
        let (output, _) = run_test("partial.rs");

        TestResults::new()
            .ok("default")
            .ok("t_partial_1")
            .ok("t_partial_2")
            .ok("t_complete")
            .assert(output);
    }

    #[test]
    fn apply_partial_fixture_from_value_attribute() {
        let (output, _) = run_test("partial_in_attr.rs");

        TestResults::new()
            .ok("default")
            .ok("t_partial_1")
            .ok("t_partial_2")
            .ok("t_complete")
            .assert(output);
    }

    #[rstest]
    #[case::compact_form("default.rs")]
    #[case::attrs_form("default_in_attrs.rs")]
    fn use_input_values_if_any(#[case] file: &str) {
        let (output, _) = run_test(file);

        TestResults::new()
            .ok("test_simple")
            .ok("test_simple_changed")
            .ok("test_double")
            .ok("test_double_changed")
            .ok("test_mixed")
            .assert(output);
    }

    #[test]
    fn convert_literal_string_for_default_values() {
        let (output, _) = run_test("default_conversion.rs");

        assert_regex!(
            "Cannot parse 'error' to get [a-z:_0-9]*MyType",
            output.stdout.str()
        );

        TestResults::new()
            .ok("test_base")
            .ok("test_byte_array")
            .ok("test_convert_custom")
            .fail("test_fail_conversion")
            .assert(output);
    }

    #[rstest]
    #[case("once.rs")]
    #[case::no_return("once_no_return.rs")]
    #[case::defined_type("once_defined_type.rs")]
    fn accept_once_attribute_and_call_fixture_just_once(#[case] fname: &str) {
        let project = prj(fname).with_nocapture();

        let output = project.run_tests().unwrap();

        // Just to see the errors if fixture doesn't compile
        assert_in!(output.stderr.str(), "Exec fixture() just once");

        let occurrences = output.stderr.str().count("Exec fixture() just once");

        assert_eq!(1, occurrences);
    }

    mod show_correct_errors {
        use super::*;
        use std::process::Output;

        use rstest::{fixture, rstest};

        #[fixture]
        #[once]
        fn errors_rs() -> (Output, String) {
            run_test("errors.rs")
        }

        #[rstest]
        fn when_cannot_resolve_fixture(errors_rs: &(Output, String)) {
            let (output, name) = errors_rs.clone();

            assert_in!(output.stderr.str(), "error[E0433]: ");
            assert_in!(
                output.stderr.str(),
                format!(
                    r#"
                      --> {name}/src/lib.rs:14:33
                       |
                    14 | fn error_cannot_resolve_fixture(no_fixture: u32) {{"#
                )
                .unindent()
            );
        }

        #[rstest]
        fn on_mismatched_types_inner(errors_rs: &(Output, String)) {
            let (output, name) = errors_rs.clone();

            assert_in!(
                output.stderr.str(),
                format!(
                    r#"
                    error[E0308]: mismatched types
                      --> {name}/src/lib.rs:10:18
                       |
                    10 |     let a: u32 = "";
                    "#
                )
                .unindent()
            );
        }

        #[rstest]
        fn on_mismatched_types_argument(errors_rs: &(Output, String)) {
            let (output, name) = errors_rs.clone();

            assert_in!(
                output.stderr.str(),
                format!(
                    r#"
                    error[E0308]: mismatched types
                      --> {name}/src/lib.rs:17:29
                    "#
                )
                .unindent()
            );

            assert_in!(
                output.stderr.str(),
                r#"
                17 | fn error_fixture_wrong_type(fixture: String) {}
                   |                             ^^^^^^"#
                    .unindent()
            );
        }

        #[rstest]
        fn on_invalid_fixture(errors_rs: &(Output, String)) {
            let (output, name) = errors_rs.clone();

            assert_in!(
                output.stderr.str(),
                format!(
                    "
                    error: Missed argument: 'not_a_fixture' should be a test function argument.
                      --> {name}/src/lib.rs:19:11
                       |
                    19 | #[fixture(not_a_fixture(24))]
                       |           ^^^^^^^^^^^^^
                    "
                )
                .unindent()
            );
        }

        #[rstest]
        fn on_duplicate_fixture_argument(errors_rs: &(Output, String)) {
            let (output, name) = errors_rs.clone();

            assert_in!(
                output.stderr.str(),
                format!(
                    r#"
                    error: Duplicate argument: 'f' is already defined.
                      --> {name}/src/lib.rs:32:23
                       |
                    32 | #[fixture(f("first"), f("second"))]
                       |                       ^
                    "#
                )
                .unindent()
            );
        }

        #[rstest]
        fn on_destruct_implicit_fixture(errors_rs: &(Output, String)) {
            let (output, name) = errors_rs.clone();

            assert_in!(
                output.stderr.str(),
                format!(
                    r#"
                    error: To destruct a fixture you should provide a path to resolve it by '#[from(...)]' attribute.
                      --> {name}/src/lib.rs:48:35
                       |
                    48 | fn error_destruct_without_resolve(T(a): T) {{}}
                       |                                   ^^^^^^^
                    "#
                )
                .unindent()
            );
        }

        #[rstest]
        fn on_destruct_explicit_fixture_without_from(errors_rs: &(Output, String)) {
            let (output, name) = errors_rs.clone();

            assert_in!(
                output.stderr.str(),
                format!(
                    r#"
                    error: To destruct a fixture you should provide a path to resolve it by '#[from(...)]' attribute.
                      --> {name}/src/lib.rs:51:57
                       |
                    51 | fn error_destruct_without_resolve_also_with(#[with(21)] T(a): T) {{}}
                       |                                                         ^^^^^^^
                    "#
                )
                .unindent()
            );
            assert_eq!(
                1,
                output.stderr.str().count("51 | fn error_destruct_without")
            )
        }

        #[fixture]
        #[once]
        fn errors_once_rs() -> (Output, String) {
            run_test("errors_once.rs")
        }

        #[rstest]
        fn once_async(errors_once_rs: &(Output, String)) {
            let (output, name) = errors_once_rs.clone();
            assert_in!(
                output.stderr.str(),
                format!(
                    r#"
                    error: Cannot apply #[once] to async fixture.
                     --> {}/src/lib.rs:4:1
                      |
                    4 | #[once]
                    "#,
                    name
                )
                .unindent()
            );
        }

        #[rstest]
        fn once_generic_type(errors_once_rs: &(Output, String)) {
            let (output, name) = errors_once_rs.clone();

            assert_in!(
                output.stderr.str(),
                format!(
                    r#"
                    error: Cannot apply #[once] on generic fixture.
                     --> {}/src/lib.rs:9:1
                      |
                    9 | #[once]
                    "#,
                    name
                )
                .unindent()
            );
        }

        #[rstest]
        fn once_generic_impl(errors_once_rs: &(Output, String)) {
            let (output, name) = errors_once_rs.clone();
            assert_in!(
                output.stderr.str(),
                format!(
                    r#"
                error: Cannot apply #[once] on generic fixture.
                  --> {}/src/lib.rs:15:1
                   |
                15 | #[once]
                "#,
                    name
                )
                .unindent()
            );
        }

        #[rstest]
        fn once_on_not_sync_type(errors_once_rs: &(Output, String)) {
            let (output, name) = errors_once_rs.clone();
            assert_in!(
                output.stderr.str(),
                format!(
                    r#"
                    error[E0277]: `Cell<u32>` cannot be shared between threads safely
                      --> {}/src/lib.rs:20:1
                       |
                    20 | #[fixture]
                       | ^^^^^^^^^^ `Cell<u32>` cannot be shared between threads safely
                    "#,
                    name,
                )
                .unindent(),
            );
        }
    }
}
