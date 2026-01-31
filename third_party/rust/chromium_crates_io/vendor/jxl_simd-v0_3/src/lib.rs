// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(clippy::too_many_arguments)]

use std::{
    fmt::Debug,
    mem::MaybeUninit,
    ops::{
        Add, AddAssign, BitAnd, BitAndAssign, BitOr, BitOrAssign, BitXor, BitXorAssign, Div,
        DivAssign, Mul, MulAssign, Neg, Sub, SubAssign,
    },
};

#[cfg(target_arch = "x86_64")]
mod x86_64;

#[cfg(target_arch = "aarch64")]
mod aarch64;

pub mod float16;
pub mod scalar;

pub use float16::f16;

#[cfg(all(target_arch = "x86_64", feature = "avx"))]
pub use x86_64::avx::AvxDescriptor;
#[cfg(all(target_arch = "x86_64", feature = "avx512"))]
pub use x86_64::avx512::Avx512Descriptor;
#[cfg(all(target_arch = "x86_64", feature = "sse42"))]
pub use x86_64::sse42::Sse42Descriptor;

#[cfg(all(target_arch = "aarch64", feature = "neon"))]
pub use aarch64::neon::NeonDescriptor;

pub use scalar::ScalarDescriptor;

pub trait SimdDescriptor: Sized + Copy + Debug + Send + Sync {
    type F32Vec: F32SimdVec<Descriptor = Self>;

    type I32Vec: I32SimdVec<Descriptor = Self>;

    type U32Vec: U32SimdVec<Descriptor = Self>;

    type Mask: SimdMask<Descriptor = Self>;

    /// Prepared 8-entry BF16 lookup table for fast approximate lookups.
    /// Use `F32SimdVec::prepare_table_bf16_8` to create and
    /// `F32SimdVec::table_lookup_bf16_8` to use.
    type Bf16Table8: Copy;

    type Descriptor256: SimdDescriptor<Descriptor256 = Self::Descriptor256>;
    type Descriptor128: SimdDescriptor<Descriptor128 = Self::Descriptor128>;

    fn new() -> Option<Self>;

    /// Returns a vector descriptor suitable for operations on vectors of length 256 (Self if the
    /// current vector type is suitable). Note that it might still be beneficial to use `Self` for
    /// .call(), as the compiler could make use of features from more advanced instruction sets.
    fn maybe_downgrade_256bit(self) -> Self::Descriptor256;

    /// Same as Self::maybe_downgrade_256bit, but for 128 bits.
    fn maybe_downgrade_128bit(self) -> Self::Descriptor128;

    /// Calls the given closure within a target feature context.
    /// This enables establishing an unbroken chain of inline functions from the feature-annotated
    /// gateway up to the closure, allowing SIMD intrinsics to be used safely.
    fn call<R>(self, f: impl FnOnce(Self) -> R) -> R;
}

/// # Safety
///
/// Implementors are required to respect the safety promises of the methods in this trait.
/// Specifically, this applies to the store_*_uninit methods.
pub unsafe trait F32SimdVec:
    Sized
    + Copy
    + Debug
    + Send
    + Sync
    + Add<Self, Output = Self>
    + Mul<Self, Output = Self>
    + Sub<Self, Output = Self>
    + Div<Self, Output = Self>
    + AddAssign<Self>
    + MulAssign<Self>
    + SubAssign<Self>
    + DivAssign<Self>
{
    type Descriptor: SimdDescriptor;

    const LEN: usize;

    /// An array of f32 of length Self::LEN.
    type UnderlyingArray: Copy + Default + Debug;

    /// Converts v to an array of v.
    fn splat(d: Self::Descriptor, v: f32) -> Self;

    fn zero(d: Self::Descriptor) -> Self;

    fn mul_add(self, mul: Self, add: Self) -> Self;

    /// Computes `add - self * mul`, equivalent to `self * (-mul) + add`.
    /// Uses fused multiply-add with negation when available (FMA3 fnmadd).
    fn neg_mul_add(self, mul: Self, add: Self) -> Self;

    // Requires `mem.len() >= Self::LEN` or it will panic.
    fn load(d: Self::Descriptor, mem: &[f32]) -> Self;

    fn load_array(d: Self::Descriptor, mem: &Self::UnderlyingArray) -> Self;

    // Requires `mem.len() >= Self::LEN` or it will panic.
    fn store(&self, mem: &mut [f32]);

    fn store_array(&self, mem: &mut Self::UnderlyingArray);

    /// Stores two vectors interleaved: [a0, b0, a1, b1, a2, b2, ...].
    /// Requires `dest.len() >= 2 * Self::LEN` or it will panic.
    #[inline(always)]
    fn store_interleaved_2(a: Self, b: Self, dest: &mut [f32]) {
        // SAFETY: f32 and MaybeUninit<f32> have the same layout.
        // We are writing to initialized memory, so treating it as uninit for writing is fine.
        let dest = unsafe {
            std::slice::from_raw_parts_mut(dest.as_mut_ptr() as *mut MaybeUninit<f32>, dest.len())
        };
        Self::store_interleaved_2_uninit(a, b, dest);
    }

    /// Stores three vectors interleaved: [a0, b0, c0, a1, b1, c1, ...].
    /// Requires `dest.len() >= 3 * Self::LEN` or it will panic.
    #[inline(always)]
    fn store_interleaved_3(a: Self, b: Self, c: Self, dest: &mut [f32]) {
        // SAFETY: f32 and MaybeUninit<f32> have the same layout.
        // We are writing to initialized memory, so treating it as uninit for writing is fine.
        let dest = unsafe {
            std::slice::from_raw_parts_mut(dest.as_mut_ptr() as *mut MaybeUninit<f32>, dest.len())
        };
        Self::store_interleaved_3_uninit(a, b, c, dest);
    }

    /// Stores four vectors interleaved: [a0, b0, c0, d0, a1, b1, c1, d1, ...].
    /// Requires `dest.len() >= 4 * Self::LEN` or it will panic.
    #[inline(always)]
    fn store_interleaved_4(a: Self, b: Self, c: Self, d: Self, dest: &mut [f32]) {
        // SAFETY: f32 and MaybeUninit<f32> have the same layout.
        // We are writing to initialized memory, so treating it as uninit for writing is fine.
        let dest = unsafe {
            std::slice::from_raw_parts_mut(dest.as_mut_ptr() as *mut MaybeUninit<f32>, dest.len())
        };
        Self::store_interleaved_4_uninit(a, b, c, d, dest);
    }

    /// Stores two vectors interleaved: [a0, b0, a1, b1, a2, b2, ...].
    /// Requires `dest.len() >= 2 * Self::LEN` or it will panic.
    ///
    /// Safety note:
    /// Does not write uninitialized data into `dest`.
    fn store_interleaved_2_uninit(a: Self, b: Self, dest: &mut [MaybeUninit<f32>]);

    /// Stores three vectors interleaved: [a0, b0, c0, a1, b1, c1, ...].
    /// Requires `dest.len() >= 3 * Self::LEN` or it will panic.
    /// Safety note:
    /// Does not write uninitialized data into `dest`.
    fn store_interleaved_3_uninit(a: Self, b: Self, c: Self, dest: &mut [MaybeUninit<f32>]);

    /// Stores four vectors interleaved: [a0, b0, c0, d0, a1, b1, c1, d1, ...].
    /// Requires `dest.len() >= 4 * Self::LEN` or it will panic.
    /// Safety note:
    /// Does not write uninitialized data into `dest`.
    fn store_interleaved_4_uninit(
        a: Self,
        b: Self,
        c: Self,
        d: Self,
        dest: &mut [MaybeUninit<f32>],
    );

    /// Stores eight vectors interleaved: [a0, b0, c0, d0, e0, f0, g0, h0, a1, ...].
    /// Requires `dest.len() >= 8 * Self::LEN` or it will panic.
    fn store_interleaved_8(
        a: Self,
        b: Self,
        c: Self,
        d: Self,
        e: Self,
        f: Self,
        g: Self,
        h: Self,
        dest: &mut [f32],
    );

    /// Loads two vectors from interleaved data: [a0, b0, a1, b1, a2, b2, ...].
    /// Returns (a, b) where a = [a0, a1, a2, ...] and b = [b0, b1, b2, ...].
    /// Requires `src.len() >= 2 * Self::LEN` or it will panic.
    fn load_deinterleaved_2(d: Self::Descriptor, src: &[f32]) -> (Self, Self);

    /// Loads three vectors from interleaved data: [a0, b0, c0, a1, b1, c1, ...].
    /// Returns (a, b, c) where a = [a0, a1, ...], b = [b0, b1, ...], c = [c0, c1, ...].
    /// Requires `src.len() >= 3 * Self::LEN` or it will panic.
    fn load_deinterleaved_3(d: Self::Descriptor, src: &[f32]) -> (Self, Self, Self);

    /// Loads four vectors from interleaved data: [a0, b0, c0, d0, a1, b1, c1, d1, ...].
    /// Returns (a, b, c, d) where each vector contains the deinterleaved components.
    /// Requires `src.len() >= 4 * Self::LEN` or it will panic.
    fn load_deinterleaved_4(d: Self::Descriptor, src: &[f32]) -> (Self, Self, Self, Self);

    /// Rounds to nearest integer and stores as u8.
    /// Behavior is unspecified if values would overflow u8.
    /// Requires `dest.len() >= Self::LEN` or it will panic.
    fn round_store_u8(self, dest: &mut [u8]);

    /// Rounds to nearest integer and stores as u16.
    /// Behavior is unspecified if values would overflow u16.
    /// Requires `dest.len() >= Self::LEN` or it will panic.
    fn round_store_u16(self, dest: &mut [u16]);

    fn abs(self) -> Self;

    fn floor(self) -> Self;

    fn sqrt(self) -> Self;

    /// Negates all elements. Currently unused but kept for API completeness.
    #[allow(dead_code)]
    fn neg(self) -> Self;

    fn copysign(self, sign: Self) -> Self;

    fn max(self, other: Self) -> Self;

    fn min(self, other: Self) -> Self;

    fn gt(self, other: Self) -> <<Self as F32SimdVec>::Descriptor as SimdDescriptor>::Mask;

    fn as_i32(self) -> <<Self as F32SimdVec>::Descriptor as SimdDescriptor>::I32Vec;

    fn bitcast_to_i32(self) -> <<Self as F32SimdVec>::Descriptor as SimdDescriptor>::I32Vec;

    /// Prepares an 8-entry f32 table for fast approximate lookups.
    /// Values are converted to BF16 format (loses lower 16 mantissa bits).
    ///
    /// Use this when you need to perform multiple lookups with the same table.
    /// The prepared table can be reused with [`table_lookup_bf16_8`].
    fn prepare_table_bf16_8(
        d: Self::Descriptor,
        table: &[f32; 8],
    ) -> <<Self as F32SimdVec>::Descriptor as SimdDescriptor>::Bf16Table8;

    /// Performs fast approximate table lookup using a prepared BF16 table.
    ///
    /// This is the fastest lookup method when the same table is used multiple times.
    /// Use [`prepare_table_bf16_8`] to create the prepared table.
    ///
    /// # Panics
    /// May panic or produce undefined results if indices contain values outside 0..8 range.
    fn table_lookup_bf16_8(
        d: Self::Descriptor,
        table: <<Self as F32SimdVec>::Descriptor as SimdDescriptor>::Bf16Table8,
        indices: <<Self as F32SimdVec>::Descriptor as SimdDescriptor>::I32Vec,
    ) -> Self;

    /// Converts a slice of f32 into a slice of Self::UnderlyingArray. If slice.len() is not a
    /// multiple of `Self::LEN` this will panic.
    fn make_array_slice(slice: &[f32]) -> &[Self::UnderlyingArray];

    /// Converts a mut slice of f32 into a slice of Self::UnderlyingArray. If slice.len() is not a
    /// multiple of `Self::LEN` this will panic.
    fn make_array_slice_mut(slice: &mut [f32]) -> &mut [Self::UnderlyingArray];

    /// Transposes the Self::LEN x Self::LEN matrix formed by array elements
    /// `data[stride * i]` for i = 0..Self::LEN.
    fn transpose_square(d: Self::Descriptor, data: &mut [Self::UnderlyingArray], stride: usize);

    /// Loads f16 values (stored as u16 bit patterns) and converts them to f32.
    /// Uses hardware conversion instructions when available (F16C on x86, NEON fp16 on ARM).
    /// Requires `mem.len() >= Self::LEN` or it will panic.
    fn load_f16_bits(d: Self::Descriptor, mem: &[u16]) -> Self;

    /// Converts f32 values to f16 and stores as u16 bit patterns.
    /// Uses hardware conversion instructions when available (F16C on x86, NEON fp16 on ARM).
    /// Requires `dest.len() >= Self::LEN` or it will panic.
    fn store_f16_bits(self, dest: &mut [u16]);
}

pub trait I32SimdVec:
    Sized
    + Copy
    + Debug
    + Send
    + Sync
    + Add<Self, Output = Self>
    + Mul<Self, Output = Self>
    + Sub<Self, Output = Self>
    + Neg<Output = Self>
    + BitAnd<Self, Output = Self>
    + BitOr<Self, Output = Self>
    + BitXor<Self, Output = Self>
    + AddAssign<Self>
    + MulAssign<Self>
    + SubAssign<Self>
    + BitAndAssign<Self>
    + BitOrAssign<Self>
    + BitXorAssign<Self>
{
    type Descriptor: SimdDescriptor;

    #[allow(dead_code)]
    const LEN: usize;

    /// Converts v to an array of v.
    fn splat(d: Self::Descriptor, v: i32) -> Self;

    // Requires `mem.len() >= Self::LEN` or it will panic.
    fn load(d: Self::Descriptor, mem: &[i32]) -> Self;

    // Requires `mem.len() >= Self::LEN` or it will panic.
    fn store(&self, mem: &mut [i32]);

    fn abs(self) -> Self;

    fn as_f32(self) -> <<Self as I32SimdVec>::Descriptor as SimdDescriptor>::F32Vec;

    fn bitcast_to_f32(self) -> <<Self as I32SimdVec>::Descriptor as SimdDescriptor>::F32Vec;

    fn bitcast_to_u32(self) -> <<Self as I32SimdVec>::Descriptor as SimdDescriptor>::U32Vec;

    fn gt(self, other: Self) -> <<Self as I32SimdVec>::Descriptor as SimdDescriptor>::Mask;

    fn lt_zero(self) -> <<Self as I32SimdVec>::Descriptor as SimdDescriptor>::Mask;

    fn eq(self, other: Self) -> <<Self as I32SimdVec>::Descriptor as SimdDescriptor>::Mask;

    fn eq_zero(self) -> <<Self as I32SimdVec>::Descriptor as SimdDescriptor>::Mask;

    fn shl<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self;

    fn shr<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self;

    fn mul_wide_take_high(self, rhs: Self) -> Self;

    /// Stores the lower 16 bits of each i32 lane as u16 values.
    /// Requires `dest.len() >= Self::LEN` or it will panic.
    fn store_u16(self, dest: &mut [u16]);
}

pub trait U32SimdVec: Sized + Copy + Debug + Send + Sync {
    type Descriptor: SimdDescriptor;

    #[allow(dead_code)]
    const LEN: usize;

    fn bitcast_to_i32(self) -> <<Self as U32SimdVec>::Descriptor as SimdDescriptor>::I32Vec;

    fn shr<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self;
}

#[macro_export]
macro_rules! shl {
    ($val: expr, $amount: literal) => {
        $val.shl::<{ $amount as u32 }, { $amount as i32 }>()
    };
}

#[macro_export]
macro_rules! shr {
    ($val: expr, $amount: literal) => {
        $val.shr::<{ $amount as u32 }, { $amount as i32 }>()
    };
}

pub trait SimdMask:
    Sized + Copy + Debug + Send + Sync + BitAnd<Self, Output = Self> + BitOr<Self, Output = Self>
{
    type Descriptor: SimdDescriptor;

    fn if_then_else_f32(
        self,
        if_true: <<Self as SimdMask>::Descriptor as SimdDescriptor>::F32Vec,
        if_false: <<Self as SimdMask>::Descriptor as SimdDescriptor>::F32Vec,
    ) -> <<Self as SimdMask>::Descriptor as SimdDescriptor>::F32Vec;

    fn if_then_else_i32(
        self,
        if_true: <<Self as SimdMask>::Descriptor as SimdDescriptor>::I32Vec,
        if_false: <<Self as SimdMask>::Descriptor as SimdDescriptor>::I32Vec,
    ) -> <<Self as SimdMask>::Descriptor as SimdDescriptor>::I32Vec;

    fn maskz_i32(
        self,
        v: <<Self as SimdMask>::Descriptor as SimdDescriptor>::I32Vec,
    ) -> <<Self as SimdMask>::Descriptor as SimdDescriptor>::I32Vec;

    fn all(self) -> bool;

    // !self & rhs
    fn andnot(self, rhs: Self) -> Self;
}

macro_rules! impl_f32_array_interface {
    () => {
        type UnderlyingArray = [f32; Self::LEN];

        #[inline(always)]
        fn make_array_slice(slice: &[f32]) -> &[Self::UnderlyingArray] {
            let (ret, rem) = slice.as_chunks();
            assert!(rem.is_empty());
            ret
        }

        #[inline(always)]
        fn make_array_slice_mut(slice: &mut [f32]) -> &mut [Self::UnderlyingArray] {
            let (ret, rem) = slice.as_chunks_mut();
            assert!(rem.is_empty());
            ret
        }

        #[inline(always)]
        fn load_array(d: Self::Descriptor, mem: &Self::UnderlyingArray) -> Self {
            Self::load(d, mem)
        }

        #[inline(always)]
        fn store_array(&self, mem: &mut Self::UnderlyingArray) {
            self.store(mem);
        }
    };
}

pub(crate) use impl_f32_array_interface;

#[cfg(test)]
mod test {
    use arbtest::arbitrary::Unstructured;

    use crate::{
        F32SimdVec, I32SimdVec, ScalarDescriptor, SimdDescriptor, test_all_instruction_sets,
    };

    enum Distribution {
        Floats,
        NonZeroFloats,
    }

    fn arb_vec<D: SimdDescriptor>(_: D, u: &mut Unstructured, dist: Distribution) -> Vec<f32> {
        let mut res = vec![0.0; D::F32Vec::LEN];
        for v in res.iter_mut() {
            match dist {
                Distribution::Floats => {
                    *v = u.arbitrary::<i32>().unwrap() as f32
                        / (1.0 + u.arbitrary::<u32>().unwrap() as f32)
                }
                Distribution::NonZeroFloats => {
                    let sign = if u.arbitrary::<bool>().unwrap() {
                        1.0
                    } else {
                        -1.0
                    };
                    *v = sign * (1.0 + u.arbitrary::<u32>().unwrap() as f32)
                        / (1.0 + u.arbitrary::<u32>().unwrap() as f32);
                }
            }
        }
        res
    }

    fn compare_scalar_simd(scalar: f32, simd: f32, max_abs: f32, max_rel: f32) {
        let abs = (simd - scalar).abs();
        let max = simd.abs().max(scalar.abs());
        let rel = abs / max;
        assert!(
            abs < max_abs || rel < max_rel,
            "simd {simd}, scalar {scalar}, abs {abs:?} rel {rel:?}",
        );
    }

    macro_rules! test_instruction {
        ($name:ident, |$a:ident: $a_dist:ident| $block:expr) => {
            fn $name<D: SimdDescriptor>(d: D) {
                fn compute<D: SimdDescriptor>(d: D, a: &[f32]) -> Vec<f32> {
                    let closure = |$a: D::F32Vec| $block;
                    let mut res = vec![0f32; a.len()];
                    for idx in (0..a.len()).step_by(D::F32Vec::LEN) {
                        closure(D::F32Vec::load(d, &a[idx..])).store(&mut res[idx..]);
                    }
                    res
                }
                arbtest::arbtest(|u| {
                    let a = arb_vec(d, u, Distribution::$a_dist);
                    let scalar_res = compute(ScalarDescriptor::new().unwrap(), &a);
                    let simd_res = compute(d, &a);
                    for (scalar, simd) in scalar_res.iter().zip(simd_res.iter()) {
                        compare_scalar_simd(*scalar, *simd, 1e-6, 1e-6);
                    }
                    Ok(())
                })
                .size_min(64);
            }
            test_all_instruction_sets!($name);
        };
        ($name:ident, |$a:ident: $a_dist:ident, $b:ident: $b_dist:ident| $block:expr) => {
            fn $name<D: SimdDescriptor>(d: D) {
                fn compute<D: SimdDescriptor>(d: D, a: &[f32], b: &[f32]) -> Vec<f32> {
                    let closure = |$a: D::F32Vec, $b: D::F32Vec| $block;
                    let mut res = vec![0f32; a.len()];
                    for idx in (0..a.len()).step_by(D::F32Vec::LEN) {
                        closure(D::F32Vec::load(d, &a[idx..]), D::F32Vec::load(d, &b[idx..]))
                            .store(&mut res[idx..]);
                    }
                    res
                }
                arbtest::arbtest(|u| {
                    let a = arb_vec(d, u, Distribution::$a_dist);
                    let b = arb_vec(d, u, Distribution::$b_dist);
                    let scalar_res = compute(ScalarDescriptor::new().unwrap(), &a, &b);
                    let simd_res = compute(d, &a, &b);
                    for (scalar, simd) in scalar_res.iter().zip(simd_res.iter()) {
                        compare_scalar_simd(*scalar, *simd, 1e-6, 1e-6);
                    }
                    Ok(())
                })
                .size_min(128);
            }
            test_all_instruction_sets!($name);
        };
        ($name:ident, |$a:ident: $a_dist:ident, $b:ident: $b_dist:ident, $c:ident: $c_dist:ident| $block:expr) => {
            fn $name<D: SimdDescriptor>(d: D) {
                fn compute<D: SimdDescriptor>(d: D, a: &[f32], b: &[f32], c: &[f32]) -> Vec<f32> {
                    let closure = |$a: D::F32Vec, $b: D::F32Vec, $c: D::F32Vec| $block;
                    let mut res = vec![0f32; a.len()];
                    for idx in (0..a.len()).step_by(D::F32Vec::LEN) {
                        closure(
                            D::F32Vec::load(d, &a[idx..]),
                            D::F32Vec::load(d, &b[idx..]),
                            D::F32Vec::load(d, &c[idx..]),
                        )
                        .store(&mut res[idx..]);
                    }
                    res
                }
                arbtest::arbtest(|u| {
                    let a = arb_vec(d, u, Distribution::$a_dist);
                    let b = arb_vec(d, u, Distribution::$b_dist);
                    let c = arb_vec(d, u, Distribution::$c_dist);
                    let scalar_res = compute(ScalarDescriptor::new().unwrap(), &a, &b, &c);
                    let simd_res = compute(d, &a, &b, &c);
                    for (scalar, simd) in scalar_res.iter().zip(simd_res.iter()) {
                        // Less strict requirements because of fma.
                        compare_scalar_simd(*scalar, *simd, 2e-5, 2e-5);
                    }
                    Ok(())
                })
                .size_min(172);
            }
            test_all_instruction_sets!($name);
        };
    }

    test_instruction!(add, |a: Floats, b: Floats| { a + b });
    test_instruction!(mul, |a: Floats, b: Floats| { a * b });
    test_instruction!(sub, |a: Floats, b: Floats| { a - b });
    test_instruction!(div, |a: Floats, b: NonZeroFloats| { a / b });

    test_instruction!(add_assign, |a: Floats, b: Floats| {
        let mut res = a;
        res += b;
        res
    });
    test_instruction!(mul_assign, |a: Floats, b: Floats| {
        let mut res = a;
        res *= b;
        res
    });
    test_instruction!(sub_assign, |a: Floats, b: Floats| {
        let mut res = a;
        res -= b;
        res
    });
    test_instruction!(div_assign, |a: Floats, b: NonZeroFloats| {
        let mut res = a;
        res /= b;
        res
    });

    test_instruction!(mul_add, |a: Floats, b: Floats, c: Floats| {
        a.mul_add(b, c)
    });

    test_instruction!(neg_mul_add, |a: Floats, b: Floats, c: Floats| {
        a.neg_mul_add(b, c)
    });

    // Validate that neg_mul_add computes c - a * b correctly
    fn test_neg_mul_add_correctness<D: SimdDescriptor>(d: D) {
        let a_vals = [
            2.0, 3.0, 4.0, 5.0, 1.5, 2.5, 3.5, 4.5, 2.5, 3.5, 4.5, 5.5, 1.0, 2.0, 3.0, 4.0,
        ];
        let b_vals = [
            1.0, 2.0, 3.0, 4.0, 0.5, 1.5, 2.5, 3.5, 1.5, 2.5, 3.5, 4.5, 0.25, 0.75, 1.25, 1.75,
        ];
        let c_vals = [
            10.0, 20.0, 30.0, 40.0, 5.0, 15.0, 25.0, 35.0, 12.0, 22.0, 32.0, 42.0, 6.0, 16.0, 26.0,
            36.0,
        ];

        let a = D::F32Vec::load(d, &a_vals[..D::F32Vec::LEN]);
        let b = D::F32Vec::load(d, &b_vals[..D::F32Vec::LEN]);
        let c = D::F32Vec::load(d, &c_vals[..D::F32Vec::LEN]);

        let result = a.neg_mul_add(b, c);
        let expected = c - a * b;

        let mut result_vals = [0.0; 16];
        let mut expected_vals = [0.0; 16];
        result.store(&mut result_vals[..D::F32Vec::LEN]);
        expected.store(&mut expected_vals[..D::F32Vec::LEN]);

        for i in 0..D::F32Vec::LEN {
            assert!(
                (result_vals[i] - expected_vals[i]).abs() < 1e-5,
                "neg_mul_add correctness failed at index {}: got {}, expected {}",
                i,
                result_vals[i],
                expected_vals[i]
            );
        }
    }

    test_all_instruction_sets!(test_neg_mul_add_correctness);

    test_instruction!(abs, |a: Floats| { a.abs() });
    test_instruction!(max, |a: Floats, b: Floats| { a.max(b) });
    test_instruction!(min, |a: Floats, b: Floats| { a.min(b) });

    // Test that the call method works, compiles, and can capture arguments
    fn test_call<D: SimdDescriptor>(d: D) {
        // Test basic call functionality
        let result = d.call(|_d| 42);
        assert_eq!(result, 42);

        // Test with capturing variables
        let multiplier = 3.0f32;
        let addend = 5.0f32;

        // Test SIMD operations inside call with captures
        let input = vec![1.0f32; D::F32Vec::LEN * 4];
        let mut output = vec![0.0f32; D::F32Vec::LEN * 4];

        d.call(|d| {
            let mult_vec = D::F32Vec::splat(d, multiplier);
            let add_vec = D::F32Vec::splat(d, addend);

            for idx in (0..input.len()).step_by(D::F32Vec::LEN) {
                let vec = D::F32Vec::load(d, &input[idx..]);
                let result = vec * mult_vec + add_vec;
                result.store(&mut output[idx..]);
            }
        });

        // Verify results
        for &val in &output {
            assert_eq!(val, 1.0 * multiplier + addend);
        }
    }
    test_all_instruction_sets!(test_call);

    fn test_neg<D: SimdDescriptor>(d: D) {
        // Test negation operation with enough elements for any SIMD size
        let len = D::F32Vec::LEN * 2; // Ensure we have at least 2 full vectors
        let input: Vec<f32> = (0..len)
            .map(|i| if i % 2 == 0 { i as f32 } else { -(i as f32) })
            .collect();
        let expected: Vec<f32> = (0..len)
            .map(|i| if i % 2 == 0 { -(i as f32) } else { i as f32 })
            .collect();
        let mut output = vec![0.0f32; input.len()];

        for idx in (0..input.len()).step_by(D::F32Vec::LEN) {
            let vec = D::F32Vec::load(d, &input[idx..]);
            let negated = vec.neg();
            negated.store(&mut output[idx..]);
        }

        for (i, (&out, &exp)) in output.iter().zip(expected.iter()).enumerate() {
            assert_eq!(
                out, exp,
                "Mismatch at index {}: expected {}, got {}",
                i, exp, out
            );
        }
    }
    test_all_instruction_sets!(test_neg);

    fn test_transpose_square<D: SimdDescriptor>(d: D) {
        // Test square matrix transpose
        let len = D::F32Vec::LEN;
        // Input: sequential values 0..
        let mut input = vec![0.0f32; len * len];
        for (i, val) in input.iter_mut().enumerate() {
            *val = i as f32;
        }

        let mut output = input.clone();
        D::F32Vec::transpose_square(d, D::F32Vec::make_array_slice_mut(&mut output), 1);

        // Verify transpose: output[i*len+j] should equal input[j*len+i]
        for i in 0..len {
            for j in 0..len {
                let expected = input[j * len + i];
                let actual = output[i * len + j];
                assert_eq!(
                    actual, expected,
                    "Mismatch at position ({}, {}): expected {}, got {}",
                    i, j, expected, actual
                );
            }
        }
    }
    test_all_instruction_sets!(test_transpose_square);

    fn test_store_interleaved_2<D: SimdDescriptor>(d: D) {
        let len = D::F32Vec::LEN;
        let a: Vec<f32> = (0..len).map(|i| i as f32).collect();
        let b: Vec<f32> = (0..len).map(|i| (i + 100) as f32).collect();
        let mut output = vec![0.0f32; 2 * len];

        let a_vec = D::F32Vec::load(d, &a);
        let b_vec = D::F32Vec::load(d, &b);
        D::F32Vec::store_interleaved_2(a_vec, b_vec, &mut output);

        // Verify interleaved output: [a0, b0, a1, b1, ...]
        for i in 0..len {
            assert_eq!(
                output[2 * i],
                a[i],
                "store_interleaved_2 failed at position {}: expected a[{}]={}, got {}",
                2 * i,
                i,
                a[i],
                output[2 * i]
            );
            assert_eq!(
                output[2 * i + 1],
                b[i],
                "store_interleaved_2 failed at position {}: expected b[{}]={}, got {}",
                2 * i + 1,
                i,
                b[i],
                output[2 * i + 1]
            );
        }
    }
    test_all_instruction_sets!(test_store_interleaved_2);

    fn test_store_interleaved_3<D: SimdDescriptor>(d: D) {
        let len = D::F32Vec::LEN;
        let a: Vec<f32> = (0..len).map(|i| i as f32).collect();
        let b: Vec<f32> = (0..len).map(|i| (i + 100) as f32).collect();
        let c: Vec<f32> = (0..len).map(|i| (i + 200) as f32).collect();
        let mut output = vec![0.0f32; 3 * len];

        let a_vec = D::F32Vec::load(d, &a);
        let b_vec = D::F32Vec::load(d, &b);
        let c_vec = D::F32Vec::load(d, &c);
        D::F32Vec::store_interleaved_3(a_vec, b_vec, c_vec, &mut output);

        // Verify interleaved output: [a0, b0, c0, a1, b1, c1, ...]
        for i in 0..len {
            assert_eq!(
                output[3 * i],
                a[i],
                "store_interleaved_3 failed at position {}: expected a[{}]={}, got {}",
                3 * i,
                i,
                a[i],
                output[3 * i]
            );
            assert_eq!(
                output[3 * i + 1],
                b[i],
                "store_interleaved_3 failed at position {}: expected b[{}]={}, got {}",
                3 * i + 1,
                i,
                b[i],
                output[3 * i + 1]
            );
            assert_eq!(
                output[3 * i + 2],
                c[i],
                "store_interleaved_3 failed at position {}: expected c[{}]={}, got {}",
                3 * i + 2,
                i,
                c[i],
                output[3 * i + 2]
            );
        }
    }
    test_all_instruction_sets!(test_store_interleaved_3);

    fn test_store_interleaved_4<D: SimdDescriptor>(d: D) {
        let len = D::F32Vec::LEN;
        let a: Vec<f32> = (0..len).map(|i| i as f32).collect();
        let b: Vec<f32> = (0..len).map(|i| (i + 100) as f32).collect();
        let c: Vec<f32> = (0..len).map(|i| (i + 200) as f32).collect();
        let e: Vec<f32> = (0..len).map(|i| (i + 300) as f32).collect();
        let mut output = vec![0.0f32; 4 * len];

        let a_vec = D::F32Vec::load(d, &a);
        let b_vec = D::F32Vec::load(d, &b);
        let c_vec = D::F32Vec::load(d, &c);
        let d_vec = D::F32Vec::load(d, &e);
        D::F32Vec::store_interleaved_4(a_vec, b_vec, c_vec, d_vec, &mut output);

        // Verify interleaved output: [a0, b0, c0, d0, a1, b1, c1, d1, ...]
        for i in 0..len {
            assert_eq!(
                output[4 * i],
                a[i],
                "store_interleaved_4 failed at position {}: expected a[{}]={}, got {}",
                4 * i,
                i,
                a[i],
                output[4 * i]
            );
            assert_eq!(
                output[4 * i + 1],
                b[i],
                "store_interleaved_4 failed at position {}: expected b[{}]={}, got {}",
                4 * i + 1,
                i,
                b[i],
                output[4 * i + 1]
            );
            assert_eq!(
                output[4 * i + 2],
                c[i],
                "store_interleaved_4 failed at position {}: expected c[{}]={}, got {}",
                4 * i + 2,
                i,
                c[i],
                output[4 * i + 2]
            );
            assert_eq!(
                output[4 * i + 3],
                e[i],
                "store_interleaved_4 failed at position {}: expected d[{}]={}, got {}",
                4 * i + 3,
                i,
                e[i],
                output[4 * i + 3]
            );
        }
    }
    test_all_instruction_sets!(test_store_interleaved_4);

    fn test_store_interleaved_8<D: SimdDescriptor>(d: D) {
        let len = D::F32Vec::LEN;
        let arr_a: Vec<f32> = (0..len).map(|i| i as f32).collect();
        let arr_b: Vec<f32> = (0..len).map(|i| (i + 100) as f32).collect();
        let arr_c: Vec<f32> = (0..len).map(|i| (i + 200) as f32).collect();
        let arr_d: Vec<f32> = (0..len).map(|i| (i + 300) as f32).collect();
        let arr_e: Vec<f32> = (0..len).map(|i| (i + 400) as f32).collect();
        let arr_f: Vec<f32> = (0..len).map(|i| (i + 500) as f32).collect();
        let arr_g: Vec<f32> = (0..len).map(|i| (i + 600) as f32).collect();
        let arr_h: Vec<f32> = (0..len).map(|i| (i + 700) as f32).collect();
        let mut output = vec![0.0f32; 8 * len];

        let a = D::F32Vec::load(d, &arr_a);
        let b = D::F32Vec::load(d, &arr_b);
        let c = D::F32Vec::load(d, &arr_c);
        let dv = D::F32Vec::load(d, &arr_d);
        let e = D::F32Vec::load(d, &arr_e);
        let f = D::F32Vec::load(d, &arr_f);
        let g = D::F32Vec::load(d, &arr_g);
        let h = D::F32Vec::load(d, &arr_h);
        D::F32Vec::store_interleaved_8(a, b, c, dv, e, f, g, h, &mut output);

        // Verify interleaved output: [a0, b0, c0, d0, e0, f0, g0, h0, a1, ...]
        let arrays = [
            &arr_a, &arr_b, &arr_c, &arr_d, &arr_e, &arr_f, &arr_g, &arr_h,
        ];
        for i in 0..len {
            for (j, arr) in arrays.iter().enumerate() {
                assert_eq!(
                    output[8 * i + j],
                    arr[i],
                    "store_interleaved_8 failed at position {}: expected {}[{}]={}, got {}",
                    8 * i + j,
                    ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'][j],
                    i,
                    arr[i],
                    output[8 * i + j]
                );
            }
        }
    }
    test_all_instruction_sets!(test_store_interleaved_8);

    fn test_load_deinterleaved_2<D: SimdDescriptor>(d: D) {
        let len = D::F32Vec::LEN;
        // Create interleaved input: [a0, b0, a1, b1, ...]
        let mut interleaved = vec![0.0f32; 2 * len];
        let expected_a: Vec<f32> = (0..len).map(|i| i as f32).collect();
        let expected_b: Vec<f32> = (0..len).map(|i| (i + 100) as f32).collect();
        for i in 0..len {
            interleaved[2 * i] = expected_a[i];
            interleaved[2 * i + 1] = expected_b[i];
        }

        let (a_vec, b_vec) = D::F32Vec::load_deinterleaved_2(d, &interleaved);

        let mut out_a = vec![0.0f32; len];
        let mut out_b = vec![0.0f32; len];
        a_vec.store(&mut out_a);
        b_vec.store(&mut out_b);

        for i in 0..len {
            assert_eq!(
                out_a[i], expected_a[i],
                "load_deinterleaved_2 failed for channel a at {}: expected {}, got {}",
                i, expected_a[i], out_a[i]
            );
            assert_eq!(
                out_b[i], expected_b[i],
                "load_deinterleaved_2 failed for channel b at {}: expected {}, got {}",
                i, expected_b[i], out_b[i]
            );
        }
    }
    test_all_instruction_sets!(test_load_deinterleaved_2);

    fn test_load_deinterleaved_3<D: SimdDescriptor>(d: D) {
        let len = D::F32Vec::LEN;
        // Create interleaved input: [a0, b0, c0, a1, b1, c1, ...]
        let mut interleaved = vec![0.0f32; 3 * len];
        let expected_a: Vec<f32> = (0..len).map(|i| i as f32).collect();
        let expected_b: Vec<f32> = (0..len).map(|i| (i + 100) as f32).collect();
        let expected_c: Vec<f32> = (0..len).map(|i| (i + 200) as f32).collect();
        for i in 0..len {
            interleaved[3 * i] = expected_a[i];
            interleaved[3 * i + 1] = expected_b[i];
            interleaved[3 * i + 2] = expected_c[i];
        }

        let (a_vec, b_vec, c_vec) = D::F32Vec::load_deinterleaved_3(d, &interleaved);

        let mut out_a = vec![0.0f32; len];
        let mut out_b = vec![0.0f32; len];
        let mut out_c = vec![0.0f32; len];
        a_vec.store(&mut out_a);
        b_vec.store(&mut out_b);
        c_vec.store(&mut out_c);

        for i in 0..len {
            assert_eq!(
                out_a[i], expected_a[i],
                "load_deinterleaved_3 failed for channel a at {}: expected {}, got {}",
                i, expected_a[i], out_a[i]
            );
            assert_eq!(
                out_b[i], expected_b[i],
                "load_deinterleaved_3 failed for channel b at {}: expected {}, got {}",
                i, expected_b[i], out_b[i]
            );
            assert_eq!(
                out_c[i], expected_c[i],
                "load_deinterleaved_3 failed for channel c at {}: expected {}, got {}",
                i, expected_c[i], out_c[i]
            );
        }
    }
    test_all_instruction_sets!(test_load_deinterleaved_3);

    fn test_load_deinterleaved_4<D: SimdDescriptor>(d: D) {
        let len = D::F32Vec::LEN;
        // Create interleaved input: [a0, b0, c0, d0, a1, b1, c1, d1, ...]
        let mut interleaved = vec![0.0f32; 4 * len];
        let expected_a: Vec<f32> = (0..len).map(|i| i as f32).collect();
        let expected_b: Vec<f32> = (0..len).map(|i| (i + 100) as f32).collect();
        let expected_c: Vec<f32> = (0..len).map(|i| (i + 200) as f32).collect();
        let expected_d: Vec<f32> = (0..len).map(|i| (i + 300) as f32).collect();
        for i in 0..len {
            interleaved[4 * i] = expected_a[i];
            interleaved[4 * i + 1] = expected_b[i];
            interleaved[4 * i + 2] = expected_c[i];
            interleaved[4 * i + 3] = expected_d[i];
        }

        let (a_vec, b_vec, c_vec, d_vec) = D::F32Vec::load_deinterleaved_4(d, &interleaved);

        let mut out_a = vec![0.0f32; len];
        let mut out_b = vec![0.0f32; len];
        let mut out_c = vec![0.0f32; len];
        let mut out_d = vec![0.0f32; len];
        a_vec.store(&mut out_a);
        b_vec.store(&mut out_b);
        c_vec.store(&mut out_c);
        d_vec.store(&mut out_d);

        for i in 0..len {
            assert_eq!(
                out_a[i], expected_a[i],
                "load_deinterleaved_4 failed for channel a at {}: expected {}, got {}",
                i, expected_a[i], out_a[i]
            );
            assert_eq!(
                out_b[i], expected_b[i],
                "load_deinterleaved_4 failed for channel b at {}: expected {}, got {}",
                i, expected_b[i], out_b[i]
            );
            assert_eq!(
                out_c[i], expected_c[i],
                "load_deinterleaved_4 failed for channel c at {}: expected {}, got {}",
                i, expected_c[i], out_c[i]
            );
            assert_eq!(
                out_d[i], expected_d[i],
                "load_deinterleaved_4 failed for channel d at {}: expected {}, got {}",
                i, expected_d[i], out_d[i]
            );
        }
    }
    test_all_instruction_sets!(test_load_deinterleaved_4);

    // Roundtrip tests: verify store_interleaved + load_deinterleaved returns original data
    fn test_interleave_roundtrip_2<D: SimdDescriptor>(d: D) {
        let len = D::F32Vec::LEN;
        let a: Vec<f32> = (0..len).map(|i| (i * 7 + 3) as f32).collect();
        let b: Vec<f32> = (0..len).map(|i| (i * 11 + 5) as f32).collect();

        let a_vec = D::F32Vec::load(d, &a);
        let b_vec = D::F32Vec::load(d, &b);

        let mut interleaved = vec![0.0f32; 2 * len];
        D::F32Vec::store_interleaved_2(a_vec, b_vec, &mut interleaved);

        let (a_out, b_out) = D::F32Vec::load_deinterleaved_2(d, &interleaved);

        let mut out_a = vec![0.0f32; len];
        let mut out_b = vec![0.0f32; len];
        a_out.store(&mut out_a);
        b_out.store(&mut out_b);

        assert_eq!(out_a, a, "interleave_roundtrip_2 failed for channel a");
        assert_eq!(out_b, b, "interleave_roundtrip_2 failed for channel b");
    }
    test_all_instruction_sets!(test_interleave_roundtrip_2);

    fn test_interleave_roundtrip_3<D: SimdDescriptor>(d: D) {
        let len = D::F32Vec::LEN;
        let a: Vec<f32> = (0..len).map(|i| (i * 7 + 3) as f32).collect();
        let b: Vec<f32> = (0..len).map(|i| (i * 11 + 5) as f32).collect();
        let c: Vec<f32> = (0..len).map(|i| (i * 13 + 9) as f32).collect();

        let a_vec = D::F32Vec::load(d, &a);
        let b_vec = D::F32Vec::load(d, &b);
        let c_vec = D::F32Vec::load(d, &c);

        let mut interleaved = vec![0.0f32; 3 * len];
        D::F32Vec::store_interleaved_3(a_vec, b_vec, c_vec, &mut interleaved);

        let (a_out, b_out, c_out) = D::F32Vec::load_deinterleaved_3(d, &interleaved);

        let mut out_a = vec![0.0f32; len];
        let mut out_b = vec![0.0f32; len];
        let mut out_c = vec![0.0f32; len];
        a_out.store(&mut out_a);
        b_out.store(&mut out_b);
        c_out.store(&mut out_c);

        assert_eq!(out_a, a, "interleave_roundtrip_3 failed for channel a");
        assert_eq!(out_b, b, "interleave_roundtrip_3 failed for channel b");
        assert_eq!(out_c, c, "interleave_roundtrip_3 failed for channel c");
    }
    test_all_instruction_sets!(test_interleave_roundtrip_3);

    fn test_interleave_roundtrip_4<D: SimdDescriptor>(d: D) {
        let len = D::F32Vec::LEN;
        let a: Vec<f32> = (0..len).map(|i| (i * 7 + 3) as f32).collect();
        let b: Vec<f32> = (0..len).map(|i| (i * 11 + 5) as f32).collect();
        let c: Vec<f32> = (0..len).map(|i| (i * 13 + 9) as f32).collect();
        let e: Vec<f32> = (0..len).map(|i| (i * 17 + 1) as f32).collect();

        let a_vec = D::F32Vec::load(d, &a);
        let b_vec = D::F32Vec::load(d, &b);
        let c_vec = D::F32Vec::load(d, &c);
        let d_vec = D::F32Vec::load(d, &e);

        let mut interleaved = vec![0.0f32; 4 * len];
        D::F32Vec::store_interleaved_4(a_vec, b_vec, c_vec, d_vec, &mut interleaved);

        let (a_out, b_out, c_out, d_out) = D::F32Vec::load_deinterleaved_4(d, &interleaved);

        let mut out_a = vec![0.0f32; len];
        let mut out_b = vec![0.0f32; len];
        let mut out_c = vec![0.0f32; len];
        let mut out_d = vec![0.0f32; len];
        a_out.store(&mut out_a);
        b_out.store(&mut out_b);
        c_out.store(&mut out_c);
        d_out.store(&mut out_d);

        assert_eq!(out_a, a, "interleave_roundtrip_4 failed for channel a");
        assert_eq!(out_b, b, "interleave_roundtrip_4 failed for channel b");
        assert_eq!(out_c, c, "interleave_roundtrip_4 failed for channel c");
        assert_eq!(out_d, e, "interleave_roundtrip_4 failed for channel d");
    }
    test_all_instruction_sets!(test_interleave_roundtrip_4);

    fn test_prepare_table_bf16_8<D: SimdDescriptor>(d: D) {
        // Create an 8-entry lookup table with known values
        // Use integer values that are exactly representable in BF16
        let lut: [f32; 8] = [0.0, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0];
        let len = D::F32Vec::LEN;

        // Prepare the table once
        let prepared = D::F32Vec::prepare_table_bf16_8(d, &lut);

        // Create indices that are valid for the LUT (0..8)
        let indices: Vec<i32> = (0..len).map(|i| (i % 8) as i32).collect();
        let expected: Vec<f32> = indices.iter().map(|&i| lut[i as usize]).collect();

        // Perform table lookup with prepared table
        let indices_vec = D::I32Vec::load(d, &indices);
        let result = D::F32Vec::table_lookup_bf16_8(d, prepared, indices_vec);

        let mut output = vec![0.0f32; len];
        result.store(&mut output);

        // Verify results - prepared lookup may have BF16 precision loss
        // BF16 has ~0.4% relative error for typical values
        for i in 0..len {
            let tolerance = if expected[i] == 0.0 {
                0.01
            } else {
                expected[i].abs() * 0.01
            };
            assert!(
                (output[i] - expected[i]).abs() < tolerance,
                "table_lookup_bf16_8 failed at position {}: expected {}, got {}",
                i,
                expected[i],
                output[i]
            );
        }
    }
    test_all_instruction_sets!(test_prepare_table_bf16_8);

    /// Test that I32 multiplication operates on all elements, not just alternating lanes.
    /// This catches the bug where _mm_mul_epi32 was used instead of _mm_mullo_epi32.
    fn test_i32_mul_all_elements<D: SimdDescriptor>(d: D) {
        let len = D::I32Vec::LEN;

        // Create input vectors where each lane has a unique value
        let mut a_data = vec![0i32; len];
        let mut b_data = vec![0i32; len];
        for i in 0..len {
            a_data[i] = (i + 1) as i32; // [1, 2, 3, 4, ...]
            b_data[i] = (i + 10) as i32; // [10, 11, 12, 13, ...]
        }

        let a = D::I32Vec::load(d, &a_data);
        let b = D::I32Vec::load(d, &b_data);
        let result = a * b;

        let mut result_data = vec![0i32; len];
        result.store(&mut result_data);

        // Verify EVERY element was multiplied correctly
        for i in 0..len {
            let expected = a_data[i] * b_data[i];
            assert_eq!(
                result_data[i], expected,
                "I32 mul failed at index {}: {} * {} = {}, got {}",
                i, a_data[i], b_data[i], expected, result_data[i]
            );
        }
    }
    test_all_instruction_sets!(test_i32_mul_all_elements);

    fn test_store_u16<D: SimdDescriptor>(d: D) {
        let data = [
            0xbabau32 as i32,
            0x1234u32 as i32,
            0xdeadbabau32 as i32,
            0xdead1234u32 as i32,
            0x1111babau32 as i32,
            0x11111234u32 as i32,
            0x76543210u32 as i32,
            0x01234567u32 as i32,
            0x00000000u32 as i32,
            0xffffffffu32 as i32,
            0x23949289u32 as i32,
            0xf9371913u32 as i32,
            0xdeadbeefu32 as i32,
            0xbeefdeadu32 as i32,
            0xaaaaaaaau32 as i32,
            0xbbbbbbbbu32 as i32,
        ];
        let mut output = [0u16; 16];
        for i in (0..16).step_by(D::I32Vec::LEN) {
            let vec = D::I32Vec::load(d, &data[i..]);
            vec.store_u16(&mut output[i..]);
        }

        for i in 0..16 {
            let expected = data[i] as u16;
            assert_eq!(
                output[i], expected,
                "store_u16 failed at index {}: expected {}, got {}",
                i, expected, output[i]
            );
        }
    }
    test_all_instruction_sets!(test_store_u16);
}
