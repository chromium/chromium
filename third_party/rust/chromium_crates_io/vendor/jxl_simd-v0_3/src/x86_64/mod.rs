// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(unsafe_code)]
#![allow(clippy::identity_op)]

#[cfg(feature = "avx")]
pub(super) mod avx;
#[cfg(feature = "avx512")]
pub(super) mod avx512;
#[cfg(feature = "sse42")]
pub(super) mod sse42;

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
            $crate::simd_function_body_avx512!($name($($arg: $ty),*) $(-> $ret)?; ($($arg),*));
            $crate::simd_function_body_avx!($name($($arg: $ty),*) $(-> $ret)?; ($($arg),*));
            $crate::simd_function_body_sse42!($name($($arg: $ty),*) $(-> $ret)?; ($($arg),*));
            $name($crate::ScalarDescriptor::new().unwrap(), $($arg),*)
        }
    };
}

#[cfg(feature = "sse42")]
#[doc(hidden)]
#[macro_export]
macro_rules! simd_function_body_sse42 {
    ($name:ident($($arg:ident: $ty:ty),* $(,)?) $(-> $ret:ty )?; ($($val:expr),* $(,)?)) => {
        if cfg!(target_feature = "sse4.2") {
            // SAFETY: we just checked for sse4.2.
            let d = unsafe { $crate::Sse42Descriptor::new_unchecked() };
            return $name(d, $($val),*);
        } else if let Some(d) = $crate::Sse42Descriptor::new() {
            #[target_feature(enable = "sse4.2")]
            fn sse42(d: $crate::Sse42Descriptor, $($arg: $ty),*) $(-> $ret)? {
                $name(d, $($val),*)
            }
            // SAFETY: we just checked for sse4.2.
            return unsafe { sse42(d, $($arg),*) };
        }
    };
}

#[cfg(feature = "avx")]
#[doc(hidden)]
#[macro_export]
macro_rules! simd_function_body_avx {
    ($name:ident($($arg:ident: $ty:ty),* $(,)?) $(-> $ret:ty )?; ($($val:expr),* $(,)?)) => {
        if cfg!(all(target_feature = "avx2", target_feature = "fma", target_feature = "f16c")) {
            // SAFETY: we just checked for avx2, fma and f16c.
            let d = unsafe { $crate::AvxDescriptor::new_unchecked() };
            return $name(d, $($val),*);
        } else if let Some(d) = $crate::AvxDescriptor::new() {
            #[target_feature(enable = "avx2,fma,f16c")]
            fn avx(d: $crate::AvxDescriptor, $($arg: $ty),*) $(-> $ret)? {
                $name(d, $($val),*)
            }
            // SAFETY: we just checked for avx2, fma and f16c.
            return unsafe { avx(d, $($arg),*) };
        }
    };
}

#[cfg(feature = "avx512")]
#[doc(hidden)]
#[macro_export]
macro_rules! simd_function_body_avx512 {
    ($name:ident($($arg:ident: $ty:ty),* $(,)?) $(-> $ret:ty )?; ($($val:expr),* $(,)?)) => {
        if cfg!(target_feature = "avx512f") {
            // SAFETY: we just checked for avx512f.
            let d = unsafe { $crate::Avx512Descriptor::new_unchecked() };
            return $name(d, $($val),*);
        } else if let Some(d) = $crate::Avx512Descriptor::new() {
            #[target_feature(enable = "avx512f")]
            fn avx512(d: $crate::Avx512Descriptor, $($arg: $ty),*) $(-> $ret)? {
                $name(d, $($val),*)
            }
            // SAFETY: we just checked for avx512f.
            return unsafe { avx512(d, $($arg),*) };
        }
    };
}

#[cfg(not(feature = "sse42"))]
#[doc(hidden)]
#[macro_export]
macro_rules! simd_function_body_sse42 {
    ($($ignore:tt)*) => {};
}

#[cfg(not(feature = "avx"))]
#[doc(hidden)]
#[macro_export]
macro_rules! simd_function_body_avx {
    ($($ignore:tt)*) => {};
}

#[cfg(not(feature = "avx512"))]
#[doc(hidden)]
#[macro_export]
macro_rules! simd_function_body_avx512 {
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

        $crate::test_sse42!($name);
        $crate::test_avx!($name);
        $crate::test_avx512!($name);
    };
}

#[cfg(feature = "sse42")]
#[doc(hidden)]
#[macro_export]
macro_rules! test_sse42 {
    ($name:ident) => {
        paste::paste! {
            #[allow(unsafe_code)]
            #[test]
            fn [<$name _sse42>]() {
                use $crate::SimdDescriptor;
                let Some(d) = $crate::Sse42Descriptor::new() else { return; };
                #[target_feature(enable = "sse4.2")]
                fn inner(d: $crate::Sse42Descriptor) {
                    $name(d)
                }
                // SAFETY: we just checked for sse4.2.
                return unsafe { inner(d) };
            }
        }
    };
}

#[cfg(feature = "avx")]
#[doc(hidden)]
#[macro_export]
macro_rules! test_avx {
    ($name:ident) => {
        paste::paste! {
            #[allow(unsafe_code)]
            #[test]
            fn [<$name _avx>]() {
                use $crate::SimdDescriptor;
                let Some(d) = $crate::AvxDescriptor::new() else { return; };
                #[target_feature(enable = "avx2,fma,f16c")]
                fn inner(d: $crate::AvxDescriptor) {
                    $name(d)
                }
                // SAFETY: we just checked for avx2, fma and f16c.
                return unsafe { inner(d) };
            }
        }
    };
}

#[cfg(feature = "avx512")]
#[doc(hidden)]
#[macro_export]
macro_rules! test_avx512 {
    ($name:ident) => {
        paste::paste! {
            #[allow(unsafe_code)]
            #[test]
            fn [<$name _avx512>]() {
                use $crate::SimdDescriptor;
                let Some(d) = $crate::Avx512Descriptor::new() else { return; };
                #[target_feature(enable = "avx512f")]
                fn inner(d: $crate::Avx512Descriptor) {
                    $name(d)
                }
                // SAFETY: we just checked for avx512f.
                return unsafe { inner(d) };
            }
        }
    };
}

#[cfg(not(feature = "sse42"))]
#[doc(hidden)]
#[macro_export]
macro_rules! test_sse42 {
    ($name:ident) => {};
}

#[cfg(not(feature = "avx"))]
#[doc(hidden)]
#[macro_export]
macro_rules! test_avx {
    ($name:ident) => {};
}

#[cfg(not(feature = "avx512"))]
#[doc(hidden)]
#[macro_export]
macro_rules! test_avx512 {
    ($name:ident) => {};
}

#[macro_export]
macro_rules! bench_all_instruction_sets {
    (
        $name:ident,
        $criterion:ident
    ) => {
        #[allow(unused)]
        use $crate::SimdDescriptor;
        // `simd_function_body_*` does early return; wrap it with an immediately-invoked closure
        (|| {
            $crate::simd_function_body_avx512!(
                $name($criterion: &mut ::criterion::BenchmarkGroup<'_, impl ::criterion::measurement::Measurement>);
                ($criterion, "avx512")
            );
        })();
        (|| {
            $crate::simd_function_body_avx!(
                $name($criterion: &mut ::criterion::BenchmarkGroup<'_, impl ::criterion::measurement::Measurement>);
                ($criterion, "avx")
            );
        })();
        (|| {
            $crate::simd_function_body_sse42!(
                $name($criterion: &mut ::criterion::BenchmarkGroup<'_, impl ::criterion::measurement::Measurement>);
                ($criterion, "sse42")
            );
        })();
        $name(
            $crate::ScalarDescriptor::new().unwrap(),
            $criterion,
            "scalar",
        );
    };
}
