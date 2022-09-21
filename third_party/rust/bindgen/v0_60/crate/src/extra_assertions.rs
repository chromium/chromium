//! Macros for defining extra assertions that should only be checked in testing
//! and/or CI when the `testing_only_extra_assertions` feature is enabled.

/// Simple macro that forwards to assert! when using
/// testing_only_extra_assertions.
#[macro_export]
macro_rules! extra_assert {
    ( $cond:expr ) => {
        if cfg!(feature = "testing_only_extra_assertions") {
            assert!($cond);
        }
    };
    ( $cond:expr , $( $arg:tt )+ ) => {
        if cfg!(feature = "testing_only_extra_assertions") {
            assert!($cond, $( $arg )* )
        }
    };
}

/// Simple macro that forwards to assert_eq! when using
/// testing_only_extra_assertions.
#[macro_export]
macro_rules! extra_assert_eq {
    ( $lhs:expr , $rhs:expr ) => {
        if cfg!(feature = "testing_only_extra_assertions") {
            assert_eq!($lhs, $rhs);
        }
    };
    ( $lhs:expr , $rhs:expr , $( $arg:tt )+ ) => {
        if cfg!(feature = "testing_only_extra_assertions") {
            assert!($lhs, $rhs, $( $arg )* );
        }
    };
}
