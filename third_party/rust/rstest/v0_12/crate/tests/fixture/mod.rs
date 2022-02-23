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
    use rstest_test::CountMessageOccurrence;

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

    #[test]
    fn resolve_async_fixture() {
        let prj = prj("async_fixture.rs");
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

        assert_in!(output.stdout.str(), "Cannot parse 'error' to get MyType");

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

        let occurences = output.stderr.str().count("Exec fixture() just once");

        assert_eq!(1, occurences);
    }

    #[test]
    fn show_correct_errors() {
        let prj = prj("errors.rs");
        let output = prj.run_tests().unwrap();
        let name = prj.get_name();

        assert_in!(output.stderr.str(), "error[E0433]: ");
        assert_in!(
            output.stderr.str(),
            format!(
                r#"
                  --> {}/src/lib.rs:12:33
                   |
                12 | fn error_cannot_resolve_fixture(no_fixture: u32) {{"#,
                name
            )
            .unindent()
        );

        assert_in!(
            output.stderr.str(),
            format!(
                r#"
                error[E0308]: mismatched types
                 --> {}/src/lib.rs:8:18
                  |
                8 |     let a: u32 = "";
                "#,
                name
            )
            .unindent()
        );

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error[E0308]: mismatched types
                  --> {}/src/lib.rs:16:29
                   |
                16 | fn error_fixture_wrong_type(fixture: String) {{
                   |                             ^^^^^^^",
                name
            )
            .unindent()
        );

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: Missed argument: 'not_a_fixture' should be a test function argument.
                  --> {}/src/lib.rs:19:11
                   |
                19 | #[fixture(not_a_fixture(24))]
                   |           ^^^^^^^^^^^^^
                ",
                name
            )
            .unindent()
        );

        assert_in!(
            output.stderr.str(),
            format!(
                r#"
                error: Duplicate argument: 'f' is already defined.
                  --> {}/src/lib.rs:33:23
                   |
                33 | #[fixture(f("first"), f("second"))]
                   |                       ^
                "#,
                name
            )
            .unindent()
        );

        assert_in!(
            output.stderr.str(),
            format!(
                r#"
                error: Cannot apply #[once] to async fixture.
                  --> {}/src/lib.rs:38:3
                   |
                38 | #[once]
                   |   ^^^^
                "#,
                name
            )
            .unindent()
        );

        assert_in!(
            output.stderr.str(),
            format!(
                r#"
                error: Cannot apply #[once] on generic fixture.
                  --> {}/src/lib.rs:43:3
                   |
                43 | #[once]
                   |   ^^^^
                "#,
                name
            )
            .unindent()
        );

        assert_in!(
            output.stderr.str(),
            format!(
                r#"
                error: Cannot apply #[once] on generic fixture.
                  --> {}/src/lib.rs:49:3
                   |
                49 | #[once]
                   |   ^^^^
                "#,
                name
            )
            .unindent()
        );
    }
}
