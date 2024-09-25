use rstest_test::{
    assert_in, assert_not_in, sanitize_name, testname, Project, Stringable, TestResults,
};

use lazy_static::lazy_static;

use rstest::rstest;
use std::path::{Path, PathBuf};
use temp_testdir::TempDir;

pub fn resources<O: AsRef<Path>>(name: O) -> PathBuf {
    Path::new("tests").join("resources").join(name)
}

fn create_prj(name: &str) -> Project {
    let prj = ROOT_PROJECT.subproject(name);
    prj.add_local_dependency("rstest_reuse");
    prj.add_dependency("rstest", r#""*""#);
    prj
}

fn prj(res: impl AsRef<Path>) -> Project {
    let prj_name = sanitize_name(testname());

    let prj = create_prj(&prj_name);
    prj.set_code_file(resources(res))
}

fn run_test(res: impl AsRef<Path>) -> (std::process::Output, String) {
    let prj = prj(res);
    (
        prj.run_tests().unwrap(),
        prj.get_name().to_owned().to_string(),
    )
}

#[test]
fn simple_example() {
    let (output, _) = run_test("simple_example.rs");

    TestResults::new()
        .ok("it_works::case_1")
        .ok("it_works::case_2")
        .fail("it_fail::case_1")
        .fail("it_fail::case_2")
        .ok("it_fail_but_ok::case_1")
        .ok("it_fail_but_ok::case_2")
        .assert(output);
}

#[test]
fn use_before_define() {
    let (output, _) = run_test("use_before_define.rs");

    TestResults::new()
        .ok("it_works::case_1")
        .ok("it_works::case_2")
        .assert(output);
}

#[rstest]
#[case::simple("simple_example.rs")]
#[case::export_not_used("export_not_used.rs")]
fn not_show_any_warning(#[case] path: &str) {
    let (output, _) = run_test(path);

    assert_not_in!(output.stderr.str(), "warning:");
}

#[test]
fn should_show_warning_if_not_used_template() {
    let (output, _) = run_test("not_used.rs");

    assert_in!(output.stderr.str(), "warning:");
}

#[test]
fn in_mod() {
    let (output, _) = run_test("in_mod.rs");

    TestResults::new()
        .ok("sub::it_works::case_1")
        .ok("sub::it_works::case_2")
        .fail("sub::it_fail::case_1")
        .fail("sub::it_fail::case_2")
        .assert(output);
}

#[test]
fn import_from_mod() {
    let (output, _) = run_test("qualify_template_use.rs");

    TestResults::new()
        .ok("user::it_works::case_1")
        .ok("user::it_works::case_2")
        .ok("qualify::it_works::case_1")
        .ok("qualify::it_works::case_2")
        .assert(output);
}

#[test]
fn copy_case_attributes_from_template() {
    let (output, _) = run_test("copy_args_attributes_from_template.rs");

    TestResults::new()
        .ok("cases::it_works::case_1")
        .ok("cases::it_works::case_2")
        .ok("cases::should_not_copy_attributes_if_already_present::case_1")
        .ok("cases::should_not_copy_attributes_if_already_present::case_2")
        .ok("cases::add_a_case::case_1")
        .ok("cases::add_a_case::case_2")
        .ok("cases::add_a_case::case_3_more")
        .ok_in("cases::add_values::case_1::_add_some_tests_1")
        .ok_in("cases::add_values::case_1::_add_some_tests_2")
        .ok_in("cases::add_values::case_1::_add_some_tests_3")
        .ok_in("cases::add_values::case_2::_add_some_tests_1")
        .ok_in("cases::add_values::case_2::_add_some_tests_2")
        .ok_in("cases::add_values::case_2::_add_some_tests_3")
        .ok("cases::should_copy_cases_also_from_underscored_attrs::case_1")
        .ok("cases::should_copy_cases_also_from_underscored_attrs::case_2")
        .ok_in("values::it_works::cases_1")
        .ok_in("values::it_works::cases_2")
        .ok_in("values::add_a_case::case_1_more::cases_1")
        .ok_in("values::add_a_case::case_1_more::cases_2")
        .ok_with("values::add_values::a_1", false, 2)
        .ok_with("values::add_values::a_2", false, 2)
        .ok_in("values::should_copy_values_also_from_underscored_attrs::_cases_1")
        .ok_in("values::should_copy_values_also_from_underscored_attrs::_cases_2")
        .assert(output);
}

#[test]
fn deny_docs() {
    let (output, _) = run_test("deny_docs.rs");

    TestResults::new()
        .ok("it_works::case_1")
        .ok("it_works::case_2")
        .assert(output);
}

#[test]
fn enable_export_macros() {
    let (output, _) = run_test("export_template.rs");

    TestResults::new()
        .ok("foo::bar::test::case_1")
        .ok("test_path::case_1")
        .ok("test_import::case_1")
        .assert(output);
}

#[test]
fn use_same_name_for_more_templates() {
    let (output, _) = run_test("templates_with_same_name.rs");

    TestResults::new()
        .ok("inner1::it_works::case_1")
        .ok("inner1::it_works::case_2")
        .ok("inner2::it_works::case_1")
        .ok("inner2::it_works::case_2")
        .assert(output);
}

#[test]
fn no_local_macro_should_not_compile() {
    let (output, _) = run_test("no_local_macro_should_not_compile.rs");

    assert!(!output.status.success());
}

#[test]
fn should_export_main_root() {
    // Add project with template
    let _prj_template =
        create_prj("export_template_root").set_code_file(resources("export_template_root.rs"));

    // Main test project that use template
    let prj = prj("import_template.rs");
    prj.add_path_dependency("export_template_root", "../export_template_root");

    let output = prj.run_tests().unwrap();
    TestResults::new().ok("test::case_1").assert(output);
}

#[test]
fn rstest_reuse_not_in_crate_root() {
    let (output, _) = run_test("rstest_reuse_not_in_crate_root.rs");

    TestResults::new()
        .ok("test::case_1")
        .assert(output);
}

lazy_static! {
    static ref ROOT_DIR: TempDir = TempDir::new(std::env::temp_dir().join("rstest_reuse"), false);
    static ref ROOT_PROJECT: Project = Project::new(ROOT_DIR.as_ref());
}
