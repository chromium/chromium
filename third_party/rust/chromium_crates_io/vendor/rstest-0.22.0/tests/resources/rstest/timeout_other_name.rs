use other_name::*;
use std::time::Duration;

fn ms(ms: u32) -> Duration {
    Duration::from_millis(ms.into())
}

mod thread {
    use super::*;

    fn delayed_sum(a: u32, b: u32, delay: Duration) -> u32 {
        std::thread::sleep(delay);
        a + b
    }

    #[rstest]
    #[timeout(ms(80))]
    fn single_pass() {
        assert_eq!(4, delayed_sum(2, 2, ms(10)));
    }

    #[rstest]
    #[timeout(ms(100))]
    fn single_fail_value() {
        assert_eq!(5, delayed_sum(2, 2, ms(1)));
    }

    #[rstest]
    #[timeout(ms(1000))]
    #[should_panic = "user message"]
    fn fail_with_user_message() {
        panic!("user message");
    }

    #[rstest]
    #[timeout(ms(10))]
    fn single_fail_timeout() {
        assert_eq!(4, delayed_sum(2, 2, ms(80)));
    }

    #[rstest]
    #[timeout(ms(80))]
    #[case(ms(10))]
    fn one_pass(#[case] delay: Duration) {
        assert_eq!(4, delayed_sum(2, 2, delay));
    }

    #[rstest]
    #[timeout(ms(10))]
    #[case(ms(80))]
    fn one_fail_timeout(#[case] delay: Duration) {
        assert_eq!(4, delayed_sum(2, 2, delay));
    }

    #[rstest]
    #[timeout(ms(100))]
    #[case(ms(1))]
    fn one_fail_value(#[case] delay: Duration) {
        assert_eq!(5, delayed_sum(2, 2, delay));
    }

    #[rstest]
    #[case::pass(ms(1), 4)]
    #[case::fail_timeout(ms(80), 4)]
    #[case::fail_value(ms(1), 5)]
    #[timeout(ms(40))]
    fn group_same_timeout(#[case] delay: Duration, #[case] expected: u32) {
        assert_eq!(expected, delayed_sum(2, 2, delay));
    }

    #[rstest]
    #[timeout(ms(100))]
    #[case::pass(ms(1), 4)]
    #[timeout(ms(30))]
    #[case::fail_timeout(ms(70), 4)]
    #[timeout(ms(100))]
    #[case::fail_value(ms(1), 5)]
    fn group_single_timeout(#[case] delay: Duration, #[case] expected: u32) {
        assert_eq!(expected, delayed_sum(2, 2, delay));
    }

    #[rstest]
    #[case::pass(ms(1), 4)]
    #[timeout(ms(10))]
    #[case::fail_timeout(ms(60), 4)]
    #[case::fail_value(ms(1), 5)]
    #[timeout(ms(100))]
    fn group_one_timeout_override(#[case] delay: Duration, #[case] expected: u32) {
        assert_eq!(expected, delayed_sum(2, 2, delay));
    }

    struct S {}

    #[rstest]
    #[case(S{})]
    fn compile_with_no_copy_arg(#[case] _s: S) {
        assert!(true);
    }

    #[fixture]
    fn no_copy() -> S {
        S {}
    }

    #[rstest]
    fn compile_with_no_copy_fixture(no_copy: S) {
        assert!(true);
    }

    #[rstest]
    fn default_timeout_failure() {
        assert_eq!(4, delayed_sum(2, 2, ms(1100)));
    }
}
