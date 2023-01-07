//! Contains code for selecting features

#![deny(missing_docs)]
#![deny(unused_extern_crates)]

use std::io;
use std::str::FromStr;

/// Define RustTarget struct definition, Default impl, and conversions
/// between RustTarget and String.
macro_rules! rust_target_def {
    ( $( $( #[$attr:meta] )* => $release:ident => $value:expr; )* ) => {
        /// Represents the version of the Rust language to target.
        ///
        /// To support a beta release, use the corresponding stable release.
        ///
        /// This enum will have more variants added as necessary.
        #[derive(Debug, Copy, Clone, Eq, PartialEq, PartialOrd, Hash)]
        #[allow(non_camel_case_types)]
        pub enum RustTarget {
            $(
                $(
                    #[$attr]
                )*
                $release,
            )*
        }

        impl Default for RustTarget {
            /// Gives the latest stable Rust version
            fn default() -> RustTarget {
                LATEST_STABLE_RUST
            }
        }

        impl FromStr for RustTarget {
            type Err = io::Error;

            /// Create a `RustTarget` from a string.
            ///
            /// * The stable/beta versions of Rust are of the form "1.0",
            /// "1.19", etc.
            /// * The nightly version should be specified with "nightly".
            fn from_str(s: &str) -> Result<Self, Self::Err> {
                match s.as_ref() {
                    $(
                        stringify!($value) => Ok(RustTarget::$release),
                    )*
                    _ => Err(
                        io::Error::new(
                            io::ErrorKind::InvalidInput,
                            concat!(
                                "Got an invalid rust target. Accepted values ",
                                "are of the form ",
                                "\"1.0\" or \"nightly\"."))),
                }
            }
        }

        impl From<RustTarget> for String {
            fn from(target: RustTarget) -> Self {
                match target {
                    $(
                        RustTarget::$release => stringify!($value),
                    )*
                }.into()
            }
        }
    }
}

/// Defines an array slice with all RustTarget values
macro_rules! rust_target_values_def {
    ( $( $( #[$attr:meta] )* => $release:ident => $value:expr; )* ) => {
        /// Strings of allowed `RustTarget` values
        pub static RUST_TARGET_STRINGS: &'static [&str] = &[
            $(
                stringify!($value),
            )*
        ];
    }
}

/// Defines macro which takes a macro
macro_rules! rust_target_base {
    ( $x_macro:ident ) => {
        $x_macro!(
            /// Rust stable 1.0
            => Stable_1_0 => 1.0;
            /// Rust stable 1.17
            ///  * Static lifetime elision ([RFC 1623](https://github.com/rust-lang/rfcs/blob/master/text/1623-static.md))
            => Stable_1_17 => 1.17;
            /// Rust stable 1.19
            ///  * Untagged unions ([RFC 1444](https://github.com/rust-lang/rfcs/blob/master/text/1444-union.md))
            => Stable_1_19 => 1.19;
            /// Rust stable 1.20
            ///  * Associated constants ([PR](https://github.com/rust-lang/rust/pull/42809))
            => Stable_1_20 => 1.20;
            /// Rust stable 1.21
            ///  * Builtin impls for `Clone` ([PR](https://github.com/rust-lang/rust/pull/43690))
            => Stable_1_21 => 1.21;
            /// Rust stable 1.25
            ///  * `repr(align)` ([PR](https://github.com/rust-lang/rust/pull/47006))
            => Stable_1_25 => 1.25;
            /// Rust stable 1.26
            ///  * [i128 / u128 support](https://doc.rust-lang.org/std/primitive.i128.html)
            => Stable_1_26 => 1.26;
            /// Rust stable 1.27
            ///  * `must_use` attribute on functions ([PR](https://github.com/rust-lang/rust/pull/48925))
            => Stable_1_27 => 1.27;
            /// Rust stable 1.28
            ///  * `repr(transparent)` ([PR](https://github.com/rust-lang/rust/pull/51562))
            => Stable_1_28 => 1.28;
            /// Rust stable 1.30
            ///  * `const fn` support for limited cases ([PR](https://github.com/rust-lang/rust/pull/54835/)
            /// *  [c_void available in core](https://doc.rust-lang.org/core/ffi/enum.c_void.html)
            => Stable_1_30 => 1.30;
            /// Rust stable 1.33
            ///  * repr(packed(N)) ([PR](https://github.com/rust-lang/rust/pull/57049))
            => Stable_1_33 => 1.33;
            /// Rust stable 1.36
            ///  * `MaybeUninit` instead of `mem::uninitialized()` ([PR](https://github.com/rust-lang/rust/pull/60445))
            => Stable_1_36 => 1.36;
            /// Rust stable 1.40
            /// * `non_exhaustive` enums/structs ([Tracking issue](https://github.com/rust-lang/rust/issues/44109))
            => Stable_1_40 => 1.40;
            /// Rust stable 1.47
            /// * `larger_arrays` ([Tracking issue](https://github.com/rust-lang/rust/pull/74060))
            => Stable_1_47 => 1.47;
            /// Nightly rust
            ///  * `thiscall` calling convention ([Tracking issue](https://github.com/rust-lang/rust/issues/42202))
            ///  * `vectorcall` calling convention (no tracking issue)
            => Nightly => nightly;
        );
    }
}

rust_target_base!(rust_target_def);
rust_target_base!(rust_target_values_def);

/// Latest stable release of Rust
pub const LATEST_STABLE_RUST: RustTarget = RustTarget::Stable_1_47;

/// Create RustFeatures struct definition, new(), and a getter for each field
macro_rules! rust_feature_def {
    (
        $( $rust_target:ident {
            $( $( #[$attr:meta] )* => $feature:ident; )*
        } )*
    ) => {
        /// Features supported by a rust target
        #[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
        #[allow(missing_docs)] // Documentation should go into the relevant variants.
        pub(crate) struct RustFeatures {
            $( $(
                $(
                    #[$attr]
                )*
                pub $feature: bool,
            )* )*
        }

        impl RustFeatures {
            /// Gives a RustFeatures struct with all features disabled
            fn new() -> Self {
                RustFeatures {
                    $( $(
                        $feature: false,
                    )* )*
                }
            }
        }

        impl From<RustTarget> for RustFeatures {
            fn from(rust_target: RustTarget) -> Self {
                let mut features = RustFeatures::new();

                $(
                if rust_target >= RustTarget::$rust_target {
                    $(
                    features.$feature = true;
                    )*
                }
                )*

                features
            }
        }
    }
}

// NOTE(emilio): When adding or removing features here, make sure to update the
// documentation for the relevant variant in the rust_target_base macro
// definition.
rust_feature_def!(
    Stable_1_17 {
        => static_lifetime_elision;
    }
    Stable_1_19 {
        => untagged_union;
    }
    Stable_1_20 {
        => associated_const;
    }
    Stable_1_21 {
        => builtin_clone_impls;
    }
    Stable_1_25 {
        => repr_align;
    }
    Stable_1_26 {
        => i128_and_u128;
    }
    Stable_1_27 {
        => must_use_function;
    }
    Stable_1_28 {
        => repr_transparent;
    }
    Stable_1_30 {
        => min_const_fn;
        => core_ffi_c_void;
    }
    Stable_1_33 {
        => repr_packed_n;
    }
    Stable_1_36 {
        => maybe_uninit;
    }
    Stable_1_40 {
        => non_exhaustive;
    }
    Stable_1_47 {
        => larger_arrays;
    }
    Nightly {
        => thiscall_abi;
        => vectorcall_abi;
    }
);

impl Default for RustFeatures {
    fn default() -> Self {
        let default_rust_target: RustTarget = Default::default();
        Self::from(default_rust_target)
    }
}

#[cfg(test)]
mod test {
    #![allow(unused_imports)]
    use super::*;

    #[test]
    fn target_features() {
        let f_1_0 = RustFeatures::from(RustTarget::Stable_1_0);
        assert!(
            !f_1_0.static_lifetime_elision &&
                !f_1_0.core_ffi_c_void &&
                !f_1_0.untagged_union &&
                !f_1_0.associated_const &&
                !f_1_0.builtin_clone_impls &&
                !f_1_0.repr_align &&
                !f_1_0.thiscall_abi &&
                !f_1_0.vectorcall_abi
        );
        let f_1_21 = RustFeatures::from(RustTarget::Stable_1_21);
        assert!(
            f_1_21.static_lifetime_elision &&
                !f_1_21.core_ffi_c_void &&
                f_1_21.untagged_union &&
                f_1_21.associated_const &&
                f_1_21.builtin_clone_impls &&
                !f_1_21.repr_align &&
                !f_1_21.thiscall_abi &&
                !f_1_21.vectorcall_abi
        );
        let f_nightly = RustFeatures::from(RustTarget::Nightly);
        assert!(
            f_nightly.static_lifetime_elision &&
                f_nightly.core_ffi_c_void &&
                f_nightly.untagged_union &&
                f_nightly.associated_const &&
                f_nightly.builtin_clone_impls &&
                f_nightly.maybe_uninit &&
                f_nightly.repr_align &&
                f_nightly.thiscall_abi &&
                f_nightly.vectorcall_abi
        );
    }

    fn test_target(target_str: &str, target: RustTarget) {
        let target_string: String = target.into();
        assert_eq!(target_str, target_string);
        assert_eq!(target, RustTarget::from_str(target_str).unwrap());
    }

    #[test]
    fn str_to_target() {
        test_target("1.0", RustTarget::Stable_1_0);
        test_target("1.17", RustTarget::Stable_1_17);
        test_target("1.19", RustTarget::Stable_1_19);
        test_target("1.21", RustTarget::Stable_1_21);
        test_target("1.25", RustTarget::Stable_1_25);
        test_target("nightly", RustTarget::Nightly);
    }
}
