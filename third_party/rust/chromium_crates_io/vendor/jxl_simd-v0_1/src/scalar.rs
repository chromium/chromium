// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{U32SimdVec, impl_f32_array_interface};

use super::{F32SimdVec, I32SimdVec, SimdDescriptor, SimdMask};

#[derive(Clone, Copy, Debug)]
pub struct ScalarDescriptor;

impl SimdDescriptor for ScalarDescriptor {
    type F32Vec = f32;
    type I32Vec = i32;
    type U32Vec = u32;
    type Mask = bool;

    type Descriptor256 = Self;
    type Descriptor128 = Self;

    fn maybe_downgrade_256bit(self) -> Self::Descriptor256 {
        self
    }

    fn maybe_downgrade_128bit(self) -> Self::Descriptor128 {
        self
    }

    fn new() -> Option<Self> {
        Some(Self)
    }

    fn call<R>(self, f: impl FnOnce(Self) -> R) -> R {
        // No special features needed for scalar implementation
        f(self)
    }
}

impl F32SimdVec for f32 {
    type Descriptor = ScalarDescriptor;

    const LEN: usize = 1;

    #[inline(always)]
    fn load(_d: Self::Descriptor, mem: &[f32]) -> Self {
        mem[0]
    }

    #[inline(always)]
    fn store(&self, mem: &mut [f32]) {
        mem[0] = *self;
    }

    #[inline(always)]
    fn mul_add(self, mul: Self, add: Self) -> Self {
        (self * mul) + add
    }

    #[inline(always)]
    fn neg_mul_add(self, mul: Self, add: Self) -> Self {
        -(self * mul) + add
    }

    #[inline(always)]
    fn splat(_d: Self::Descriptor, v: f32) -> Self {
        v
    }

    #[inline(always)]
    fn zero(_d: Self::Descriptor) -> Self {
        0.0
    }

    #[inline(always)]
    fn abs(self) -> Self {
        self.abs()
    }

    #[inline(always)]
    fn floor(self) -> Self {
        self.floor()
    }

    #[inline(always)]
    fn sqrt(self) -> Self {
        self.sqrt()
    }

    #[inline(always)]
    fn neg(self) -> Self {
        -self
    }

    #[inline(always)]
    fn copysign(self, sign: Self) -> Self {
        self.copysign(sign)
    }

    #[inline(always)]
    fn max(self, other: Self) -> Self {
        self.max(other)
    }

    #[inline(always)]
    fn gt(self, other: Self) -> bool {
        self > other
    }

    #[inline(always)]
    fn as_i32(self) -> i32 {
        self as i32
    }

    #[inline(always)]
    fn bitcast_to_i32(self) -> i32 {
        self.to_bits() as i32
    }

    impl_f32_array_interface!();

    #[inline(always)]
    fn transpose_square(_d: Self::Descriptor, _data: &mut [Self::UnderlyingArray], _stride: usize) {
        // Nothing to do.
    }
}

impl I32SimdVec for i32 {
    type Descriptor = ScalarDescriptor;

    const LEN: usize = 1;

    #[inline(always)]
    fn splat(_d: Self::Descriptor, v: i32) -> Self {
        v
    }

    #[inline(always)]
    fn load(_d: Self::Descriptor, mem: &[i32]) -> Self {
        mem[0]
    }

    #[inline(always)]
    fn store(&self, mem: &mut [i32]) {
        mem[0] = *self;
    }

    #[inline(always)]
    fn abs(self) -> Self {
        self.abs()
    }

    #[inline(always)]
    fn as_f32(self) -> f32 {
        self as f32
    }

    #[inline(always)]
    fn bitcast_to_f32(self) -> f32 {
        f32::from_bits(self as u32)
    }

    #[inline(always)]
    fn bitcast_to_u32(self) -> u32 {
        self as u32
    }

    #[inline(always)]
    fn gt(self, other: Self) -> bool {
        self > other
    }

    #[inline(always)]
    fn lt_zero(self) -> bool {
        self < 0
    }

    #[inline(always)]
    fn eq(self, other: Self) -> bool {
        self == other
    }

    #[inline(always)]
    fn eq_zero(self) -> bool {
        self == 0
    }

    #[inline(always)]
    fn shl<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        self << AMOUNT_U
    }

    #[inline(always)]
    fn shr<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        self >> AMOUNT_U
    }

    #[inline(always)]
    fn mul_wide_take_high(self, rhs: Self) -> Self {
        ((self as i64 * rhs as i64) >> 32) as i32
    }
}

impl U32SimdVec for u32 {
    type Descriptor = ScalarDescriptor;

    const LEN: usize = 1;

    #[inline(always)]
    fn bitcast_to_i32(self) -> i32 {
        self as i32
    }

    #[inline(always)]
    fn shr<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        self >> AMOUNT_U
    }
}

impl SimdMask for bool {
    type Descriptor = ScalarDescriptor;

    #[inline(always)]
    fn if_then_else_f32(self, if_true: f32, if_false: f32) -> f32 {
        if self { if_true } else { if_false }
    }

    #[inline(always)]
    fn if_then_else_i32(self, if_true: i32, if_false: i32) -> i32 {
        if self { if_true } else { if_false }
    }

    #[inline(always)]
    fn maskz_i32(self, v: i32) -> i32 {
        if self { 0 } else { v }
    }

    #[inline(always)]
    fn all(self) -> bool {
        self
    }

    #[inline(always)]
    fn andnot(self, rhs: Self) -> Self {
        (!self) & rhs
    }
}

#[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64")))]
#[macro_export]
macro_rules! simd_function {
    (
        $dname:ident,
        $descr:ident: $descr_ty:ident,
        $(#[$($attr:meta)*])*
        $pub:vis fn $name:ident($($arg:ident: $ty:ty),* $(,)?) $(-> $ret:ty )? $body: block
    ) => {
        $(#[$($attr)*])*
        $pub fn $name<$descr_ty: $crate::SimdDescriptor>($descr: $descr_ty, $($arg: $ty),*) $(-> $ret)? $body
        $(#[$($attr)*])*
        $pub fn $dname($($arg: $ty),*) $(-> $ret)? {
            use $crate::SimdDescriptor;
            $name($crate::ScalarDescriptor::new().unwrap(), $($arg),*)
        }
    };
}

#[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64")))]
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
    };
}

#[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64")))]
#[macro_export]
macro_rules! bench_all_instruction_sets {
    (
        $name:ident,
        $criterion:ident
    ) => {
        use $crate::SimdDescriptor;
        $name(
            $crate::ScalarDescriptor::new().unwrap(),
            $criterion,
            "scalar",
        );
    };
}
