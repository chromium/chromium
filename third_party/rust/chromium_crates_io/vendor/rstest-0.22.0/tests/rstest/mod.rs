use std::{fs::File, io::Write, path::Path};

use mytest::*;
use rstest_test::*;
use unindent::Unindent;

pub fn resources(res: impl AsRef<Path>) -> std::path::PathBuf {
    let path = Path::new("rstest").join(res.as_ref());
    super::resources(path)
}

fn prj(res: impl AsRef<Path>) -> Project {
    crate::prj().set_code_file(resources(res))
}

fn run_test(res: impl AsRef<Path>) -> (std::process::Output, String) {
    let prj = prj(res);
    (
        prj.run_tests().unwrap(),
        prj.get_name().to_owned().to_string(),
    )
}

#[test]
fn files() {
    let prj = prj("files.rs");
    let files_path = prj.path().join("files");
    let sub_folder = files_path.join("sub");
    let up_sub_folder = prj.path().join("../files_test_sub_folder");
    std::fs::create_dir(&files_path).unwrap();
    std::fs::create_dir(&sub_folder).unwrap();
    std::fs::create_dir(&up_sub_folder).unwrap();

    for n in 0..4 {
        let name = format!("element_{}.txt", n);
        let path = files_path.join(&name);
        let mut out = File::create(path).unwrap();
        out.write_all(name.as_bytes()).unwrap();
        out.write_all(b"--\n").unwrap();
        out.write_all(b"something else\n").unwrap();
    }
    let dot = files_path.join(".ignore_me.txt");
    File::create(dot)
        .unwrap()
        .write_all(b".ignore_me.txt--\n")
        .unwrap();
    let exclude = files_path.join("exclude.txt");
    File::create(exclude)
        .unwrap()
        .write_all(b"excluded\n")
        .unwrap();
    let sub = sub_folder.join("sub_dir_file.txt");
    File::create(sub)
        .unwrap()
        .write_all(b"sub_dir_file.txt--\nmore")
        .unwrap();
    let down_from_parent_folder = up_sub_folder.join("from_parent_folder.txt");
    File::create(down_from_parent_folder)
        .unwrap()
        .write_all(b"from_parent_folder.txt--\nmore")
        .unwrap();
    let output = prj.run_tests().unwrap();

    TestResults::new()
        .ok("start_with_name::path_1__UP_files_test_sub_folder_from_parent_folder_txt")
        .ok("start_with_name::path_2_files_element_0_txt")
        .ok("start_with_name::path_3_files_element_1_txt")
        .ok("start_with_name::path_4_files_element_2_txt")
        .ok("start_with_name::path_5_files_element_3_txt")
        .ok("start_with_name::path_6_files_sub_sub_dir_file_txt")
        .ok("start_with_name_with_include::path_1_files__ignore_me_txt")
        .ok("start_with_name_with_include::path_2_files_element_0_txt")
        .ok("start_with_name_with_include::path_3_files_element_1_txt")
        .ok("start_with_name_with_include::path_4_files_element_2_txt")
        .ok("start_with_name_with_include::path_5_files_element_3_txt")
        .ok("start_with_name_with_include::path_6_files_sub_sub_dir_file_txt")
        .ok("module::pathbuf_need_not_be_in_scope::path_1_files__ignore_me_txt")
        .ok("module::pathbuf_need_not_be_in_scope::path_2_files_element_0_txt")
        .ok("module::pathbuf_need_not_be_in_scope::path_3_files_element_1_txt")
        .ok("module::pathbuf_need_not_be_in_scope::path_4_files_element_2_txt")
        .ok("module::pathbuf_need_not_be_in_scope::path_5_files_element_3_txt")
        .ok("module::pathbuf_need_not_be_in_scope::path_6_files_sub_sub_dir_file_txt")
        .assert(output);
}

#[test]
fn mutable_input() {
    let (output, _) = run_test("mut.rs");

    TestResults::new()
        .ok("should_success")
        .fail("should_fail")
        .ok("add_test::case_1")
        .ok("add_test::case_2")
        .fail("add_test::case_3")
        .assert(output);
}

#[test]
fn test_with_return_type() {
    let (output, _) = run_test("return_result.rs");

    TestResults::new()
        .ok("should_success")
        .fail("should_fail")
        .ok("return_type::case_1_should_success")
        .fail("return_type::case_2_should_fail")
        .assert(output);
}

#[test]
fn should_panic() {
    let (output, _) = run_test("panic.rs");

    TestResults::new()
        .ok("should_success")
        .fail("should_fail")
        .ok("fail::case_1")
        .ok("fail::case_2")
        .fail("fail::case_3")
        .assert(output);
}

#[test]
fn should_not_show_a_warning_for_should_panic_attribute() {
    let (output, _) = run_test("panic.rs");

    assert!(!output.stderr.str().contains("unused attribute"));
}

#[test]
fn should_not_show_a_warning_for_values_test_names() {
    let (output, _) = run_test("values_tests_name.rs");

    assert_not_in!(output.stderr.str(), "warning:");
}

#[test]
fn should_map_fixture_by_remove_first_underscore_if_any() {
    let (output, _) = run_test("remove_underscore.rs");

    TestResults::new().ok("ignore_input").assert(output);
}

#[test]
fn generic_input() {
    let (output, _) = run_test("generic.rs");

    TestResults::new()
        .ok("simple")
        .ok("strlen_test::case_1")
        .ok("strlen_test::case_2")
        .assert(output);
}

#[test]
fn impl_input() {
    let (output, _) = run_test("impl_param.rs");

    TestResults::new()
        .ok("simple")
        .ok("strlen_test::case_1")
        .ok("strlen_test::case_2")
        .assert(output);
}

#[test]
fn use_mutable_fixture_in_parametric_arguments() {
    let (output, _) = run_test("use_mutable_fixture_in_parametric_arguments.rs");

    TestResults::new()
        .with_contains(true)
        .ok("use_mutate_fixture::case_1::b_1")
        .assert(output);
}

#[test]
fn should_not_remove_lifetimes() {
    let (output, _) = run_test("lifetimes.rs");

    TestResults::new()
        .with_contains(true)
        .ok("case")
        .ok("values")
        .ok("fixture")
        .assert(output);
}

#[test]
fn should_reject_no_item_function() {
    let (output, name) = run_test("reject_no_item_function.rs");

    assert_in!(
        output.stderr.str(),
        format!(
            "
        error: expected `fn`
         --> {}/src/lib.rs:4:1
          |
        4 | struct Foo;
          | ^^^^^^
        ",
            name
        )
        .unindent()
    );

    assert_in!(
        output.stderr.str(),
        format!(
            "
        error: expected `fn`
         --> {}/src/lib.rs:7:1
          |
        7 | impl Foo {{}}
          | ^^^^
        ",
            name
        )
        .unindent()
    );

    assert_in!(
        output.stderr.str(),
        format!(
            "
        error: expected `fn`
          --> {}/src/lib.rs:10:1
           |
        10 | mod mod_baz {{}}
           | ^^^
        ",
            name
        )
        .unindent()
    );
}

mod dump_input_values {
    use super::*;

    #[rstest]
    #[case::compact_syntax("dump_debug_compact.rs")]
    #[case::attr_syntax("dump_debug.rs")]
    fn if_implements_debug(#[case] source: &str) {
        let (output, _) = run_test(source);
        let out = output.stdout.str().to_string();

        TestResults::new()
            .fail("single_fail")
            .fail("no_trace_single_fail")
            .fail("cases_fail::case_1")
            .fail("cases_fail::case_2")
            .fail("no_trace_cases_fail::case_1")
            .fail("no_trace_cases_fail::case_2")
            .fail_with("matrix_fail::u_1", false, 8)
            .fail_with("matrix_fail::u_2", false, 8)
            .assert(output);

        assert_in!(out, "fu32 = 42");
        assert_in!(out, r#"fstring = "A String""#);
        assert_in!(out, r#"ftuple = (A, "A String", -12"#);

        assert_in!(out, "u = 42");
        assert_in!(out, r#"s = "str""#);
        assert_in!(out, r#"t = ("ss", -12)"#);

        assert_in!(out, "u = 24");
        assert_in!(out, r#"s = "trs""#);
        assert_in!(out, r#"t = ("tt", -24)"#);

        assert_in!(out, "u = 1");
        assert_in!(out, r#"s = "rst""#);
        assert_in!(out, r#"t = ("SS", -12)"#);

        assert_in!(out, "u = 2");
        assert_in!(out, r#"s = "srt""#);
        assert_in!(out, r#"t = ("TT", -24)"#);

        let expected = 11;
        for marker in ["TEST START", "TEST ARGUMENTS"] {
            let n_found = out.lines().filter(|l| l.contains(marker)).count();
            assert_eq!(
                n_found, expected,
                "Should contain {expected} '{marker}' but found {n_found}. [Should not enclose output if no trace]"
            );
        }
    }

    #[rstest]
    #[case::compact_syntax("dump_not_debug_compact.rs")]
    #[case::attr_syntax("dump_not_debug.rs")]
    fn should_not_compile_if_not_implement_debug(#[case] source: &str) {
        let (output, name) = run_test(source);

        assert_all_in!(
            output.stderr.str(),
            format!("--> {}/src/lib.rs:10:11", name),
            "fn single(fixture: S) {}",
            "^^^^^^^ `S` cannot be formatted using `{:?}`"
        );

        assert_in!(
            output.stderr.str(),
            format!("--> {}/src/lib.rs:15:10", name),
            "fn cases(s: S) {}",
            "^ `S` cannot be formatted using `{:?}`"
        );

        assert_in!(
            output.stderr.str(),
            format!("--> {}/src/lib.rs:20:11", name),
            "fn matrix(s: S) {}",
            "^ `S` cannot be formatted using `{:?}`"
        );
    }

    #[rstest]
    #[case::compact_syntax("dump_exclude_some_inputs_compact.rs")]
    #[case::attr_syntax("dump_exclude_some_inputs.rs")]
    fn can_exclude_some_inputs(#[case] source: &str) {
        let (output, _) = run_test(source);
        let out = output.stdout.str().to_string();

        TestResults::new()
            .fail("simple")
            .fail("cases::case_1")
            .fail_in("matrix::a_1")
            .assert(output);

        assert_in!(out, "fu32 = 42");
        assert_in!(out, "d = D");
        assert_in!(out, "fd = D");
        assert_in!(out, "dd = D");
    }

    #[test]
    fn should_be_enclosed_in_an_explicit_session() {
        let (output, _) = run_test(Path::new("single").join("dump_debug.rs"));
        let out = output.stdout.str().to_string();

        TestResults::new().fail("should_fail").assert(output);

        let lines = out
            .lines()
            .skip_while(|l| !l.contains("TEST ARGUMENTS"))
            .take_while(|l| !l.contains("TEST START"))
            .collect::<Vec<_>>();

        let expected = 4;
        assert_eq!(
            expected,
            lines.len(),
            "Not contains {expected} lines but {}: '{}'",
            lines.len(),
            lines.join("\n")
        );
    }
}

mod single {
    use super::*;

    fn res(name: impl AsRef<Path>) -> impl AsRef<Path> {
        Path::new("single").join(name.as_ref())
    }

    #[test]
    fn one_success_and_one_fail() {
        let (output, _) = run_test(res("simple.rs"));

        TestResults::new()
            .ok("should_success")
            .fail("should_fail")
            .assert(output);
    }

    #[test]
    fn should_resolve_generics_fixture_outputs() {
        let (output, _) = run_test(res("resolve.rs"));

        TestResults::new()
            .ok("generics_u32")
            .ok("generics_i32")
            .assert(output);
    }

    #[test]
    fn should_apply_partial_fixture() {
        let (output, _) = run_test(res("partial.rs"));

        TestResults::new()
            .ok("default")
            .ok("partial_1")
            .ok("partial_attr_1")
            .ok("partial_2")
            .ok("partial_attr_2")
            .ok("complete")
            .ok("complete_attr")
            .assert(output);
    }

    #[rstest]
    #[case("async.rs")]
    #[case("async_awt.rs")]
    #[case("async_awt_global.rs")]
    fn should_run_async_function(#[case] name: &str) {
        let prj = prj(res(name));
        prj.add_dependency("async-std", r#"{version="*", features=["attributes"]}"#);

        let output = prj.run_tests().unwrap();

        TestResults::new()
            .ok("should_pass")
            .fail("should_fail")
            .ok("should_panic_pass")
            .fail("should_panic_fail")
            .assert(output);
    }

    #[test]
    fn should_use_injected_test_attr() {
        let prj = prj(res("inject.rs"));
        prj.add_dependency("actix-rt", r#""1.1.0""#);

        let output = prj.run_tests().unwrap();

        TestResults::new()
            .ok("sync_case")
            .ok("sync_case_panic")
            .fail("sync_case_fail")
            .fail("sync_case_panic_fail")
            .ok("async_case")
            .ok("async_case_panic")
            .fail("async_case_fail")
            .fail("async_case_panic_fail")
            .assert(output);
    }
}

mod cases {
    use super::*;

    fn res(name: impl AsRef<Path>) -> impl AsRef<Path> {
        Path::new("cases").join(name.as_ref())
    }

    #[test]
    fn should_compile() {
        let output = prj(res("simple.rs")).compile().unwrap();

        assert_eq!(
            Some(0),
            output.status.code(),
            "Compile error due: {}",
            output.stderr.str()
        )
    }

    #[test]
    fn happy_path() {
        let (output, _) = run_test(res("simple.rs"));

        TestResults::new()
            .ok("strlen_test::case_1")
            .ok("strlen_test::case_2")
            .assert(output);
    }

    #[test]
    fn use_attr() {
        let (output, _) = run_test(res("use_attr.rs"));

        TestResults::new()
            .ok("all::case_1_ciao")
            .ok("all::case_2_panic")
            .ok("all::case_3_foo")
            .ok("just_cases::case_1_ciao")
            .ok("just_cases::case_2_foo")
            .ok("just_cases::case_3_panic")
            .ok("just_args::case_1_ciao")
            .ok("just_args::case_2_foo")
            .ok("just_args::case_3_panic")
            .ok("all_panic::case_1")
            .ok("all_panic::case_2")
            .assert(output);
    }

    #[test]
    fn case_description() {
        let (output, _) = run_test(res("description.rs"));

        TestResults::new()
            .ok("description::case_1_user_test_description")
            .ok("description::case_2")
            .fail("description::case_3_user_test_description_fail")
            .assert(output);
    }

    #[test]
    fn should_apply_partial_fixture() {
        let (output, _) = run_test(res("partial.rs"));

        TestResults::new()
            .ok("default::case_1")
            .ok("partial_1::case_1")
            .ok("partial_2::case_1")
            .ok("complete::case_1")
            .ok("partial_attr_1::case_1")
            .ok("partial_attr_2::case_1")
            .ok("complete_attr::case_1")
            .fail("default::case_2")
            .fail("partial_1::case_2")
            .fail("partial_2::case_2")
            .fail("complete::case_2")
            .fail("partial_attr_1::case_2")
            .fail("partial_attr_2::case_2")
            .fail("complete_attr::case_2")
            .assert(output);
    }

    #[test]
    fn should_use_case_attributes() {
        let (output, _) = run_test(res("case_attributes.rs"));

        TestResults::new()
            .ok("attribute_per_case::case_1_no_panic")
            .ok("attribute_per_case::case_2_panic")
            .ok("attribute_per_case::case_3_panic_with_message")
            .fail("attribute_per_case::case_4_no_panic_but_fail")
            .fail("attribute_per_case::case_5_panic_but_fail")
            .fail("attribute_per_case::case_6_panic_with_wrong_message")
            .assert(output);
    }

    #[rstest]
    #[case("async.rs")]
    #[case("async_awt.rs")]
    #[case("async_awt_global.rs")]
    fn should_run_async_function(#[case] name: &str) {
        let prj = prj(res(name));
        prj.add_dependency("async-std", r#"{version="*", features=["attributes"]}"#);

        let output = prj.run_tests().unwrap();

        TestResults::new()
            .ok("my_async_test::case_1_pass")
            .fail("my_async_test::case_2_fail")
            .ok("my_async_test::case_3_pass_panic")
            .fail("my_async_test::case_4_fail_panic")
            .ok("my_async_test_revert::case_1_pass")
            .assert(output);
    }

    #[rstest]
    fn should_run_async_mut() {
        let prj = prj(res("async_awt_mut.rs"));
        prj.add_dependency("async-std", r#"{version="*", features=["attributes"]}"#);

        let output = prj.run_tests().unwrap();

        TestResults::new()
            .ok("my_mut_test_global_awt::case_1_pass")
            .ok("my_mut_test_local_awt::case_1_pass")
            .assert(output);
    }

    #[test]
    fn should_use_injected_test_attr() {
        let prj = prj(res("inject.rs"));
        prj.add_dependency("actix-rt", r#""1.1.0""#);

        let output = prj.run_tests().unwrap();

        TestResults::new()
            .ok("sync::case_1_pass")
            .ok("sync::case_2_panic")
            .fail("sync::case_3_fail")
            .ok("fn_async::case_1_pass")
            .ok("fn_async::case_2_panic")
            .fail("fn_async::case_3_fail")
            .assert(output);
    }

    #[test]
    fn trace_just_one_test() {
        let (output, _) = run_test(res("dump_just_one_case.rs"));
        let out = output.stdout.str().to_string();

        TestResults::new()
            .fail("cases::case_1_first_no_dump")
            .fail("cases::case_2_dump_me")
            .fail("cases::case_3_last_no_dump")
            .assert(output);

        assert_in!(out, r#"s = "Trace it!""#);
        assert_not_in!(out, r#"s = "Please don't trace me""#);
    }

    mod not_compile_if_missed_arguments {
        use super::*;

        #[test]
        fn happy_path() {
            let (output, _) = run_test(res("missed_argument.rs"));
            let stderr = output.stderr.str();

            assert_ne!(Some(0), output.status.code());
            assert_in!(stderr, "Missed argument");
            assert_in!(
                stderr,
                "
                  |
                4 | #[rstest(f, case(42), case(24))]
                  |          ^
                "
                .unindent()
            );
        }

        #[test]
        fn should_reports_all() {
            let (output, _) = run_test(res("missed_some_arguments.rs"));
            let stderr = output.stderr.str();

            assert_in!(
                stderr,
                "
                  |
                4 | #[rstest(a,b,c, case(1,2,3), case(3,2,1))]
                  |          ^
                "
                .unindent()
            );
            assert_in!(
                stderr,
                "
                  |
                4 | #[rstest(a,b,c, case(1,2,3), case(3,2,1))]
                  |              ^
                "
                .unindent()
            );

            assert_eq!(
                2,
                stderr.count("Missed argument"),
                "Should contain message exactly 2 occurrences in error message:\n{}",
                stderr
            )
        }

        #[test]
        fn should_report_just_one_error_message_for_all_test_cases() {
            let (output, _) = run_test(res("missed_argument.rs"));
            let stderr = output.stderr.str();

            assert_eq!(
                1,
                stderr.count("Missed argument"),
                "More than one message occurrence in error message:\n{}",
                stderr
            )
        }

        #[test]
        fn should_not_report_error_in_macro_syntax() {
            let (output, _) = run_test(res("missed_argument.rs"));
            let stderr = output.stderr.str();

            assert!(!stderr.contains("macros that expand to items"));
        }
    }

    mod not_compile_if_a_case_has_a_wrong_signature {
        use std::process::Output;

        use lazy_static::lazy_static;

        use super::*;

        //noinspection RsTypeCheck
        fn execute() -> &'static (Output, String) {
            lazy_static! {
                static ref OUTPUT: (Output, String) = run_test(res("case_with_wrong_args.rs"));
            }
            assert_ne!(Some(0), OUTPUT.0.status.code(), "Should not compile");
            &OUTPUT
        }

        #[test]
        fn with_too_much_arguments() {
            let (output, _) = execute();
            let stderr = output.stderr.str();

            assert_in!(
                stderr,
                "
                  |
                8 | #[rstest(a, case(42, 43), case(12), case(24, 34))]
                  |                  ^^^^^^
                "
                .unindent()
            );

            assert_in!(
                stderr,
                "
                  |
                8 | #[rstest(a, case(42, 43), case(12), case(24, 34))]
                  |                                          ^^^^^^
                "
                .unindent()
            );
        }

        #[test]
        fn with_less_arguments() {
            let (output, _) = execute();
            let stderr = output.stderr.str();

            assert_in!(
                stderr,
                "
                  |
                4 | #[rstest(a, b, case(42), case(1, 2), case(43))]
                  |                     ^^
                "
                .unindent()
            );

            assert_in!(
                stderr,
                "
                  |
                4 | #[rstest(a, b, case(42), case(1, 2), case(43))]
                  |                                           ^^
                "
                .unindent()
            );
        }

        #[test]
        fn and_reports_all_errors() {
            let (output, _) = execute();
            let stderr = output.stderr.str();

            // Exactly 4 cases are wrong
            assert_eq!(
                4,
                stderr.count("Wrong case signature: should match the given parameters list."),
                "Should contain message exactly 4 occurrences in error message:\n{}",
                stderr
            );
        }
    }

    mod not_compile_if_args_but_no_cases {
        use std::process::Output;

        use lazy_static::lazy_static;

        use super::*;

        //noinspection RsTypeCheck
        fn execute() -> &'static (Output, String) {
            lazy_static! {
                static ref OUTPUT: (Output, String) = run_test(res("args_with_no_cases.rs"));
            }
            assert_ne!(Some(0), OUTPUT.0.status.code(), "Should not compile");
            &OUTPUT
        }

        #[test]
        fn report_error() {
            let (output, name) = execute();
            let stderr = output.stderr.str();

            assert_in!(
                stderr,
                format!(
                    "
                error: No cases for this argument.
                 --> {}/src/lib.rs:3:10
                  |
                3 | #[rstest(one, two, three)]
                  |          ^^^
                ",
                    name
                )
                .unindent()
            );
        }

        #[test]
        fn and_reports_all_errors() {
            let (output, _) = execute();
            let stderr = output.stderr.str();

            // Exactly 3 cases are wrong
            assert_eq!(
                3,
                stderr.count("No cases for this argument."),
                "Should contain message exactly 3 occurrences in error message:\n{}",
                stderr
            );
        }
    }
}

mod matrix {
    use super::*;

    fn res(name: impl AsRef<Path>) -> impl AsRef<Path> {
        Path::new("matrix").join(name.as_ref())
    }

    #[test]
    fn should_compile() {
        let output = prj(res("simple.rs")).compile().unwrap();

        assert_eq!(
            Some(0),
            output.status.code(),
            "Compile error due: {}",
            output.stderr.str()
        )
    }

    #[test]
    fn happy_path() {
        let (output, _) = run_test(res("simple.rs"));

        TestResults::new()
            .with_contains(true)
            .ok("strlen_test::expected_1_4::input_1___ciao__")
            .ok("strlen_test::expected_1_4::input_2___buzz__")
            .ok("strlen_test::expected_2_2_3_2::input_1___ciao__")
            .ok("strlen_test::expected_2_2_3_2::input_2___buzz__")
            .assert(output);
    }

    #[test]
    fn should_apply_partial_fixture() {
        let (output, _) = run_test(res("partial.rs"));

        TestResults::new()
            .with_contains(true)
            .ok_times("default::a_1", 2)
            .ok("default::a_2")
            .ok("partial_2::a_2")
            .ok("partial_attr_2::a_2")
            .ok("complete::a_2")
            .ok("complete_attr::a_2")
            .fail("default::a_2")
            .fail_times("partial_1::a_1", 2)
            .fail_times("partial_1::a_2", 2)
            .fail_times("partial_2::a_1", 2)
            .fail("partial_2::a_2")
            .fail_times("complete::a_1", 2)
            .fail("complete::a_2")
            .fail_times("partial_attr_1::a_1", 2)
            .fail_times("partial_attr_1::a_2", 2)
            .fail_times("partial_attr_2::a_1", 2)
            .fail("partial_attr_2::a_2")
            .fail_times("complete_attr::a_1", 2)
            .fail("complete_attr::a_2")
            .assert(output);
    }

    #[rstest]
    #[case("async.rs")]
    #[case("async_awt.rs")]
    #[case("async_awt_global.rs")]
    fn should_run_async_function(#[case] name: &str) {
        let prj = prj(res(name));
        prj.add_dependency("async-std", r#"{version="*", features=["attributes"]}"#);

        let output = prj.run_tests().unwrap();

        TestResults::new()
            .with_contains(true)
            .ok("my_async_test::first_1")
            .fail("my_async_test::first_1")
            .fail("my_async_test::first_2")
            .ok("my_async_test::first_2")
            .assert(output);
    }

    #[test]
    fn should_use_injected_test_attr() {
        let prj = prj(res("inject.rs"));
        prj.add_dependency("actix-rt", r#""1.1.0""#);

        let output = prj.run_tests().unwrap();

        TestResults::new()
            .with_contains(true)
            .ok("sync::first_1")
            .fail("sync::first_1")
            .fail("sync::first_2")
            .ok("sync::first_2")
            .ok("fn_async::first_1")
            .fail("fn_async::first_1")
            .fail("fn_async::first_2")
            .ok("fn_async::first_2")
            .assert(output);
    }

    #[test]
    fn use_args_attributes() {
        let (output, _) = run_test(res("use_attr.rs"));

        TestResults::new()
            .ok("both::expected_1_4::input_1___ciao__")
            .ok("both::expected_1_4::input_2___buzz__")
            .ok("both::expected_2_2_3_2::input_1___ciao__")
            .ok("both::expected_2_2_3_2::input_2___buzz__")
            .ok("first::input_1___ciao__::expected_1_4")
            .ok("first::input_2___buzz__::expected_1_4")
            .ok("first::input_1___ciao__::expected_2_2_3_2")
            .ok("first::input_2___buzz__::expected_2_2_3_2")
            .ok("second::expected_1_4::input_1___ciao__")
            .ok("second::expected_1_4::input_2___buzz__")
            .ok("second::expected_2_2_3_2::input_1___ciao__")
            .ok("second::expected_2_2_3_2::input_2___buzz__")
            .assert(output);
    }
}

#[test]
fn convert_string_literal() {
    let (output, _) = run_test("convert_string_literal.rs");

    assert_regex!(
        "Cannot parse 'error' to get [a-z:_0-9]*MyType",
        output.stdout.str()
    );

    TestResults::new()
        .ok("cases::case_1")
        .ok("cases::case_2")
        .ok("cases::case_3")
        .ok("cases::case_4")
        .fail("cases::case_5")
        .fail("cases::case_6")
        .ok_in("values::addr_1")
        .ok_in("values::addr_2")
        .fail_in("values::addr_3")
        .fail_in("values::addr_4")
        .ok_in("not_convert_byte_array::case_1::values_1")
        .ok("not_convert_impl::case_1")
        .ok("not_convert_generics::case_1")
        .ok("not_convert_generics::case_2")
        .ok("convert_without_debug::case_1")
        .fail("convert_without_debug::case_2")
        .assert(output);
}

#[test]
fn happy_path() {
    let (output, _) = run_test("happy_path.rs");

    TestResults::new()
        .ok("happy::case_1::expected_1_4::input_1___ciao__")
        .ok("happy::case_1::expected_1_4::input_2___buzz__")
        .ok("happy::case_1::expected_2_2_3_2::input_1___ciao__")
        .ok("happy::case_1::expected_2_2_3_2::input_2___buzz__")
        .ok("happy::case_2_second::expected_1_4::input_1___ciao__")
        .ok("happy::case_2_second::expected_1_4::input_2___buzz__")
        .ok("happy::case_2_second::expected_2_2_3_2::input_1___ciao__")
        .ok("happy::case_2_second::expected_2_2_3_2::input_2___buzz__")
        .assert(output);
}

#[test]
fn destruct() {
    let (output, _) = run_test("destruct.rs");

    TestResults::new()
        .ok("cases_destruct::case_1_two_times_twenty_one::__destruct_3_1_T__new_42_1_")
        .ok("cases_destruct::case_1_two_times_twenty_one::__destruct_3_2_T_a_3_b_14_")
        .ok("cases_destruct::case_2_six_times_seven::__destruct_3_1_T__new_42_1_")
        .ok("cases_destruct::case_2_six_times_seven::__destruct_3_2_T_a_3_b_14_")
        .ok("cases_destruct_named_tuple::case_1_two_times_twenty_one::__destruct_3_1_S_42_1_")
        .ok("cases_destruct_named_tuple::case_1_two_times_twenty_one::__destruct_3_2_S_3_14_")
        .ok("cases_destruct_named_tuple::case_2_six_times_seven::__destruct_3_1_S_42_1_")
        .ok("cases_destruct_named_tuple::case_2_six_times_seven::__destruct_3_2_S_3_14_")
        .ok("cases_destruct_tuple::case_1_two_times_twenty_one::__destruct_3_1__42_1_")
        .ok("cases_destruct_tuple::case_1_two_times_twenty_one::__destruct_3_2__3_14_")
        .ok("cases_destruct_tuple::case_2_six_times_seven::__destruct_3_1__42_1_")
        .ok("cases_destruct_tuple::case_2_six_times_seven::__destruct_3_2__3_14_")
        .ok("swapped")
        .assert(output);
}

#[test]
fn rename() {
    let (output, _) = run_test("rename.rs");

    TestResults::new()
        .ok("compact")
        .ok("compact_mod")
        .ok("compact_injected")
        .ok("attribute")
        .ok("attribute_mod")
        .ok("attribute_injected")
        .assert(output);
}

#[test]
fn ignore_underscore_args() {
    let (output, _) = run_test("ignore_args.rs");

    TestResults::new()
        .with_contains(true)
        .ok("test::case_1::_ignore3_1")
        .ok("test::case_1::_ignore3_2")
        .ok("test::case_1::_ignore3_3")
        .ok("test::case_1::_ignore3_4")
        .ok("test::case_2::_ignore3_1")
        .ok("test::case_2::_ignore3_2")
        .ok("test::case_2::_ignore3_3")
        .ok("test::case_2::_ignore3_4")
        .assert(output);
}

#[test]
fn ignore_args_not_fixtures() {
    let prj = prj("ignore_not_fixture_arg.rs");
    prj.add_dependency(
        "sqlx",
        r#"{version="*", features=["sqlite","macros","runtime-tokio"]}"#,
    );

    let output = prj.run_tests().unwrap();

    TestResults::new()
        .with_contains(true)
        .ok("test_db")
        .assert(output);
}

#[test]
fn timeout() {
    let mut prj = prj("timeout.rs");
    prj.add_dependency("async-std", r#"{version="*", features=["attributes"]}"#);
    prj.set_default_timeout(1);
    let output = prj.run_tests().unwrap();

    TestResults::new()
        .ok("thread::single_pass")
        .fail("thread::single_fail_value")
        .ok("thread::fail_with_user_message")
        .fail("thread::single_fail_timeout")
        .ok("thread::one_pass::case_1")
        .fail("thread::one_fail_value::case_1")
        .fail("thread::one_fail_timeout::case_1")
        .ok("thread::group_same_timeout::case_1_pass")
        .fail("thread::group_same_timeout::case_2_fail_timeout")
        .fail("thread::group_same_timeout::case_3_fail_value")
        .ok("thread::group_single_timeout::case_1_pass")
        .fail("thread::group_single_timeout::case_2_fail_timeout")
        .fail("thread::group_single_timeout::case_3_fail_value")
        .ok("thread::group_one_timeout_override::case_1_pass")
        .fail("thread::group_one_timeout_override::case_2_fail_timeout")
        .fail("thread::group_one_timeout_override::case_3_fail_value")
        .ok("thread::compile_with_no_copy_arg::case_1")
        .ok("thread::compile_with_no_copy_fixture")
        .fail("thread::default_timeout_failure")
        .ok("async_std_cases::single_pass")
        .fail("async_std_cases::single_fail_value")
        .ok("async_std_cases::fail_with_user_message")
        .fail("async_std_cases::single_fail_timeout")
        .ok("async_std_cases::one_pass::case_1")
        .fail("async_std_cases::one_fail_value::case_1")
        .fail("async_std_cases::one_fail_timeout::case_1")
        .ok("async_std_cases::group_same_timeout::case_1_pass")
        .fail("async_std_cases::group_same_timeout::case_2_fail_timeout")
        .fail("async_std_cases::group_same_timeout::case_3_fail_value")
        .ok("async_std_cases::group_single_timeout::case_1_pass")
        .fail("async_std_cases::group_single_timeout::case_2_fail_timeout")
        .fail("async_std_cases::group_single_timeout::case_3_fail_value")
        .ok("async_std_cases::group_one_timeout_override::case_1_pass")
        .fail("async_std_cases::group_one_timeout_override::case_2_fail_timeout")
        .fail("async_std_cases::group_one_timeout_override::case_3_fail_value")
        .ok("async_std_cases::compile_with_no_copy_arg::case_1")
        .ok("async_std_cases::compile_with_no_copy_fixture")
        .ok("async_std_cases::compile_with_async_fixture")
        .ok("async_std_cases::compile_with_async_awt_fixture")
        .assert(output);
}

mod import_crate_with_other_name {
    use super::*;

    fn prj(res: &str, features: Option<&[&str]>) -> Project {
        let prj = crate::base_prj();
        let default_features = features.is_none();
        let features = features
            .map(|features| {
                features
                    .iter()
                    .map(|f| format!(r#""{}""#, f))
                    .collect::<Vec<_>>()
                    .join(",")
            })
            .unwrap_or_else(|| "".to_string());
        prj.add_dependency(
            "other_name",
            &format!(
                r#"{{path="{}", package = "rstest", default-features = {}, features = [{}]}}"#,
                prj.exec_dir_str().as_str(),
                default_features,
                features
            ),
        );
        prj.set_code_file(resources(res))
    }

    #[test]
    fn should_fails_to_compile_if_crate_name_feature_is_not_enabled() {
        let prj = prj("timeout_other_name.rs", Some(&[]));
        assert!(!prj.compile().unwrap().status.success());
    }

    #[test]
    fn should_always_compile_project_that_use_default_name() {
        let prj = crate::base_prj();
        prj.add_dependency(
            "rstest",
            &format!(
                r#"{{path="{}", default-features = false}}"#,
                prj.exec_dir_str().as_str(),
            ),
        );
        let prj = prj.set_code_file(resources("convert_string_literal.rs"));

        assert!(prj.compile().unwrap().status.success());
    }

    #[rstest]
    #[case::default_features(None)]
    #[case::with_crate_name_feature(Some(["crate-name"].as_slice()))]
    fn timeout_should_compile_and_run(#[case] features: Option<&[&str]>) {
        let prj = prj("timeout_other_name.rs", features);
        assert!(prj.compile().unwrap().status.success());
    }

    #[rstest]
    #[case::default(None)]
    #[case::with_crate_name_feature(Some(["crate-name"].as_slice()))]
    fn convert_string_literal_should_compile_and_run(#[case] features: Option<&[&str]>) {
        let prj = prj("convert_string_literal_other_name.rs", features);
        assert!(prj.compile().unwrap().status.success());
    }
}

#[test]
fn local_lifetime() {
    let (output, _) = run_test("local_lifetime.rs");

    TestResults::new()
        .ok("tests::it_works::case_1")
        .assert(output);
}

#[test]
fn by_ref() {
    let prj = prj("by_ref.rs");
    let files_path = prj.path().join("files");
    std::fs::create_dir(&files_path).unwrap();
    let name = "my_name.txt";
    let mut out = File::create(files_path.join(name)).unwrap();
    out.write_all(name.as_bytes()).unwrap();
    let output = prj.run_tests().unwrap();

    TestResults::new()
        .ok("test::case_1::v_1_42")
        .ok("test::case_1::v_2_142")
        .ok("start_with_name::path_1_files_my_name_txt")
        .assert(output);
}

mod async_timeout_feature {
    use super::*;

    fn build_prj(features: &[&str]) -> Project {
        let prj = crate::base_prj();
        let features = match features.is_empty() {
            true => String::new(),
            false => format!(r#", features=["{}"]"#, features.join(r#"",""#)),
        };
        prj.add_dependency(
            "rstest",
            &format!(
                r#"{{path="{}", default-features = false {}}}"#,
                prj.exec_dir_str().as_str(),
                features
            ),
        );
        prj.add_dependency("async-std", r#"{version="*", features=["attributes"]}"#);
        prj
    }

    #[test]
    fn should_not_compile_if_feature_disable() {
        let prj = build_prj(&[]);
        let output = prj
            .set_code_file(resources("timeout_async.rs"))
            .run_tests()
            .unwrap();

        assert_in!(output.stderr.str(), "error: Enable async-timeout feature");
    }

    #[test]
    fn should_work_if_feature_enabled() {
        let prj = build_prj(&["async-timeout"]);

        let output = prj
            .set_code_file(resources("timeout_async.rs"))
            .run_tests()
            .unwrap();

        TestResults::new().ok("single_pass").assert(output);
    }
}

mod should_show_correct_errors {
    use std::process::Output;

    use lazy_static::lazy_static;

    use super::*;

    //noinspection RsTypeCheck
    fn execute() -> &'static (Output, String) {
        lazy_static! {
            static ref OUTPUT: (Output, String) = run_test("errors.rs");
        }
        &OUTPUT
    }

    #[test]
    fn if_no_fixture() {
        let (output, name) = execute();

        assert_in!(output.stderr.str(), "error[E0433]: ");
        assert_in!(
            output.stderr.str(),
            format!(
                "
                  --> {}/src/lib.rs:13:33
                   |
                13 | fn error_cannot_resolve_fixture(no_fixture: u32, f: u32) {{}}",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_inject_wrong_fixture() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: Missed argument: 'not_a_fixture' should be a test function argument.
                  --> {}/src/lib.rs:28:23
                   |
                28 | #[rstest(f, case(42), not_a_fixture(24))]
                   |                       ^^^^^^^^^^^^^
                ",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_wrong_type() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                r#"
                error[E0308]: mismatched types
                 --> {}/src/lib.rs:9:18
                  |
                9 |     let a: u32 = "";
                "#,
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_wrong_type_fixture() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error[E0308]: mismatched types
                  --> {}/src/lib.rs:16:29
                   |
                16 | fn error_fixture_wrong_type(fixture: String, f: u32) {{}}
                ",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_wrong_type_case_param() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error[E0308]: mismatched types
                  --> {}/src/lib.rs:19:26
                   |
                19 | fn error_case_wrong_type(f: &str) {{}}",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_wrong_type_matrix_param() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error[E0308]: mismatched types
                  --> {}/src/lib.rs:51:28
                   |
                51 | fn error_matrix_wrong_type(f: &str) {{}}",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_arbitrary_rust_code_has_some_errors() {
        let (output, name) = execute();

        assert_regex!(
            format!(
                r#"error\[E0308\]: mismatched types
                \s+--> {}/src/lib\.rs:22:31"#,
                name
            )
            .unindent(),
            output.stderr.str()
        );
        assert_regex!(
            r#"22\s+|\s+case\(vec!\[1,2,3\]\.contains\(2\)\)\)"#,
            output.stderr.str()
        );

        assert_regex!(
            format!(
                r#"error\[E0308\]: mismatched types
                \s+--> {}/src/lib\.rs:53:45"#,
                name
            )
            .unindent(),
            output.stderr.str()
        );
        assert_regex!(
            r#"53\s+|\s+#\[rstest\(condition => \[vec!\[1,2,3\]\.contains\(2\)\] \)\]"#,
            output.stderr.str()
        );
    }

    #[test]
    fn if_inject_a_fixture_that_is_already_a_case() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: Duplicate argument: 'f' is already defined.
                  --> {}/src/lib.rs:41:13
                   |
                41 | #[rstest(f, f(42), case(12))]
                   |             ^",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_define_a_case_arg_that_is_already_an_injected_fixture() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: Duplicate argument: 'f' is already defined.
                  --> {}/src/lib.rs:44:17
                   |
                44 | #[rstest(f(42), f, case(12))]
                   |                 ^",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_inject_a_fixture_more_than_once() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: Duplicate argument: 'f' is already defined.
                  --> {}/src/lib.rs:47:20
                   |
                47 | #[rstest(v, f(42), f(42), case(12))]
                   |                    ^",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_list_argument_dont_match_function_signature() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: Missed argument: 'not_exist_1' should be a test function argument.
                  --> {}/src/lib.rs:61:10
                   |
                61 | #[rstest(not_exist_1 => [42],
                   |          ^^^^^^^^^^^",
                name
            )
            .unindent()
        );

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: Missed argument: 'not_exist_2' should be a test function argument.
                  --> {}/src/lib.rs:62:10
                   |
                62 |          not_exist_2 => [42])]
                   |          ^^^^^^^^^^^",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_inject_a_fixture_that_is_already_a_value_list() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: Duplicate argument: 'f' is already defined.
                  --> {}/src/lib.rs:65:25
                   |
                65 | #[rstest(f => [41, 42], f(42))]
                   |                         ^",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_define_value_list_more_that_once() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: Duplicate argument: 'a' is already defined.
                  --> {}/src/lib.rs:77:25
                   |
                77 | #[rstest(a => [42, 24], a => [24, 42])]
                   |                         ^",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_define_value_list_that_is_already_an_injected_fixture() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: Duplicate argument: 'f' is already defined.
                  --> {}/src/lib.rs:68:17
                   |
                68 | #[rstest(f(42), f => [41, 42])]
                   |                 ^",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_define_value_list_that_is_already_a_case_arg() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: Duplicate argument: 'a' is already defined.
                  --> {}/src/lib.rs:71:23
                   |
                71 | #[rstest(a, case(42), a => [42])]
                   |                       ^",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_define_a_case_arg_that_is_already_a_value_list() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: Duplicate argument: 'a' is already defined.
                  --> {}/src/lib.rs:74:21
                   |
                74 | #[rstest(a => [42], a, case(42))]
                   |                     ^",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_define_a_case_arg_more_that_once() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: Duplicate argument: 'a' is already defined.
                  --> {}/src/lib.rs:80:13
                   |
                80 | #[rstest(a, a, case(42))]
                   |             ^",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_a_value_contains_empty_list() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: Values list should not be empty
                  --> {}/src/lib.rs:58:19
                   |
                58 | #[rstest(empty => [])]
                   |                   ^^",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_try_to_convert_literal_string_to_a_type_that_not_implement_from_str() {
        let (output, name) = execute();

        assert_in!(output.stderr.str(), format!("--> {}/src/lib.rs:84:1", name));
        assert_in!(
            output.stderr.str(),
            "| -------- doesn't satisfy `S: FromStr`"
        );
    }

    #[test]
    fn if_try_to_use_future_on_an_impl() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                  --> {}/src/lib.rs:93:8
                   |
                93 |     s: impl AsRef<str>,
                   |        ^^^^^^^^^^^^^^^
                ",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_try_to_use_future_more_that_once() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                   --> {}/src/lib.rs:102:5
                    |
                102 |     #[future]
                    |     ^^^^^^^^^
                ",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_use_timeout_without_arg() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: expected attribute arguments in parentheses: #[timeout(...)]
                   --> {}/src/lib.rs:108:3
                    |
                108 | #[timeout]
                    |   ^^^^^^^
                ",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_timeout_is_not_an_expression() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: expected an expression
                   --> {}/src/lib.rs:112:17
                    |
                112 | #[timeout(some -> strange -> invalid -> expression)]
                    |                 ^
                ",
                name
            )
            .unindent()
        );
    }

    #[test]
    fn if_timeout_is_not_a_duration() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error[E0308]: mismatched types
                   --> {}/src/lib.rs:116:11",
                name
            )
            .unindent()
        );

        assert_in!(
            output.stderr.str(),
            "
            116 | #[timeout(42)]
                |           ^^ expected `Duration`, found integer
            "
            .unindent()
        );
    }

    #[test]
    fn if_files_contains_absolute_path() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                "
                error: Invalid glob path: path contains non-relative component
                   --> {}/src/lib.rs:120:30",
                name
            )
            .unindent()
        );

        assert_in!(
            output.stderr.str(),
            r#"
                120 | fn error_absolute_path_files(#[files("/tmp/tmp.Q81idVZYAV/*.txt")] path: std::path::PathBuf) {}
                    |                              ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
            "#
            .unindent()
        );
    }

    #[test]
    fn try_to_destruct_implicit_fixture() {
        let (output, name) = execute();

        assert_in!(
            output.stderr.str(),
            format!(
                r#"
                error: To destruct a fixture you should provide a path to resolve it by '#[from(...)]' attribute.
                   --> {name}/src/lib.rs:126:27
                    |
                126 | fn wrong_destruct_fixture(T(a, b): T, #[with(42)] T(c, d): T) {{}}
                    |                           ^^^^^^^^^^"#,
            )
            .unindent()
        );

        assert_in!(
            output.stderr.str(),
            format!(
                r#"
                error: To destruct a fixture you should provide a path to resolve it by '#[from(...)]' attribute.
                   --> {name}/src/lib.rs:126:51
                    |
                126 | fn wrong_destruct_fixture(T(a, b): T, #[with(42)] T(c, d): T) {{}}
                    |                                                   ^^^^^^^^^^"#,
            )
            .unindent()
        );

        assert_not_in!(output.stderr.str(), "#[case] T(e, f): T");
        assert_not_in!(output.stderr.str(), "#[values(T(1, 2))] T(g, h): T");
    }
}
