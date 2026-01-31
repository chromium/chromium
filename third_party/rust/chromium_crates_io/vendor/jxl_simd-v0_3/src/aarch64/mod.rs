// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(unsafe_code)]
#![allow(clippy::identity_op)]

#[cfg(feature = "neon")]
pub(super) mod neon;

#[macro_export]
macro_rules! simd_function {
    (
        $dname:ident,
        $descr:ident: $descr_ty:ident,
        $(#[$($attr:meta)*])*
        $pub:vis fn $name:ident($($arg:ident: $ty:ty),* $(,)?) $(-> $ret:ty )? $body: block
    ) => {
        #[inline(always)]
        $(#[$($attr)*])*
        $pub fn $name<$descr_ty: $crate::SimdDescriptor>($descr: $descr_ty, $($arg: $ty),*) $(-> $ret)? $body
        #[allow(unsafe_code)]
        $(#[$($attr)*])*
        $pub fn $dname($($arg: $ty),*) $(-> $ret)? {
            #[allow(unused)]
            use $crate::SimdDescriptor;
            $crate::simd_function_body_neon!($name($($arg: $ty),*) $(-> $ret)?; ($($arg),*));
            $name($crate::ScalarDescriptor::new().unwrap(), $($arg),*)
        }
    };
}

#[cfg(feature = "neon")]
#[doc(hidden)]
#[macro_export]
macro_rules! simd_function_body_neon {
    ($name:ident($($arg:ident: $ty:ty),* $(,)?) $(-> $ret:ty )?; ($($val:expr),* $(,)?)) => {
        if cfg!(target_feature = "neon") {
            // SAFETY: we just checked for neon.
            let d = unsafe { $crate::NeonDescriptor::new_unchecked() };
            return $name(d, $($val),*);
        } else if let Some(d) = $crate::NeonDescriptor::new() {
            #[target_feature(enable = "neon")]
            fn neon(d: $crate::NeonDescriptor, $($arg: $ty),*) $(-> $ret)? {
                $name(d, $($val),*)
            }
            // SAFETY: we just checked for neon.
            return unsafe { neon(d, $($arg),*) };
        }
    };
}

#[cfg(not(feature = "neon"))]
#[doc(hidden)]
#[macro_export]
macro_rules! simd_function_body_neon {
    ($($ignore:tt)*) => {};
}

#[macro_export]
macro_rules! test_all_instruction_sets {
    (
        $name:ident
    ) => {
        paste::paste! {
            #[test]
            fn [<$name _scalar>]() {
                use $crate::SimdDescriptor;
                $name($crate::ScalarDescriptor::new().unwrap())
            }
        }

        $crate::test_neon!($name);
    };
}

#[cfg(feature = "neon")]
#[doc(hidden)]
#[macro_export]
macro_rules! test_neon {
    ($name:ident) => {
        paste::paste! {
            #[allow(unsafe_code)]
            #[test]
            fn [<$name _neon>]() {
                use $crate::SimdDescriptor;
                let Some(d) = $crate::NeonDescriptor::new() else { return; };
                #[target_feature(enable = "neon")]
                fn inner(d: $crate::NeonDescriptor) {
                    $name(d)
                }
                // SAFETY: we just checked for neon.
                return unsafe { inner(d) };

            }
        }
    };
}

#[cfg(not(feature = "neon"))]
#[doc(hidden)]
#[macro_export]
macro_rules! test_neon {
    ($name:ident) => {};
}

#[macro_export]
macro_rules! bench_all_instruction_sets {
    (
        $name:ident,
        $criterion:ident
    ) => {
        use $crate::SimdDescriptor;
        // `simd_function_body_*` does early return; wrap it with an immediately-invoked closure
        (|| {
            $crate::simd_function_body_neon!(
                $name($criterion: &mut ::criterion::BenchmarkGroup<'_, impl ::criterion::measurement::Measurement>);
                ($criterion, "neon")
            );
        })();
        $name(
            $crate::ScalarDescriptor::new().unwrap(),
            $criterion,
            "scalar",
        );
    };
}
