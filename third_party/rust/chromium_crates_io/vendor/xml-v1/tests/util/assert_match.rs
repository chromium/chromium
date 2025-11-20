#[macro_export]
macro_rules! assert_match {
    ($actual:expr, $( $expected:pat_param )|+ $( if $guard: expr )? $(,)?) => {
        assert_match!($actual, $( $expected )|+ $( if $guard )?, "assert_match failed");
    };
    ($actual:expr, $( $expected:pat_param )|+ $( if $guard: expr )?, $($arg:tt)+) => {
        #[allow(unused)]
        match $actual {
            $( $expected )|+ $( if $guard )? => {},
            ref actual => panic!("{msg}\nexpect: `{expected}`\nactual: `{actual:?}`",
                msg = format_args!($($arg)+), expected = stringify!($( $expected )|+ $( if $guard: expr )?), actual = actual),
        };
    };
}
