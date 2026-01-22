// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{
    arch::aarch64::*,
    mem::MaybeUninit,
    ops::{
        Add, AddAssign, BitAnd, BitAndAssign, BitOr, BitOrAssign, BitXor, BitXorAssign, Div,
        DivAssign, Mul, MulAssign, Neg, Sub, SubAssign,
    },
};

use crate::U32SimdVec;

use super::super::{F32SimdVec, I32SimdVec, SimdDescriptor, SimdMask};

// Safety invariant: this type is only ever constructed if neon is available.
#[derive(Clone, Copy, Debug)]
pub struct NeonDescriptor(());

impl NeonDescriptor {
    /// # Safety
    /// The caller must guarantee that the "neon" target feature is available.
    pub unsafe fn new_unchecked() -> Self {
        Self(())
    }
}

/// Prepared 8-entry BF16 lookup table for NEON.
/// Contains 8 BF16 values packed into 16 bytes (uint8x16_t).
#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct Bf16Table8Neon(uint8x16_t);

impl SimdDescriptor for NeonDescriptor {
    type F32Vec = F32VecNeon;

    type I32Vec = I32VecNeon;

    type U32Vec = U32VecNeon;

    type Mask = MaskNeon;
    type Bf16Table8 = Bf16Table8Neon;

    type Descriptor256 = Self;
    type Descriptor128 = Self;

    fn new() -> Option<Self> {
        if std::arch::is_aarch64_feature_detected!("neon") {
            // SAFETY: we just checked neon.
            Some(unsafe { Self::new_unchecked() })
        } else {
            None
        }
    }

    fn maybe_downgrade_256bit(self) -> Self {
        self
    }

    fn maybe_downgrade_128bit(self) -> Self {
        self
    }

    fn call<R>(self, f: impl FnOnce(Self) -> R) -> R {
        #[target_feature(enable = "neon")]
        #[inline(never)]
        unsafe fn inner<R>(d: NeonDescriptor, f: impl FnOnce(NeonDescriptor) -> R) -> R {
            f(d)
        }
        // SAFETY: the safety invariant on `self` guarantees neon.
        unsafe { inner(self, f) }
    }
}

// TODO: retire this macro once we have #[unsafe(target_feature)].
macro_rules! fn_neon {
    {} => {};
    {$(
        fn $name:ident($this:ident: $self_ty:ty $(, $arg:ident: $ty:ty)* $(,)?) $(-> $ret:ty )?
        $body: block
    )*} => {$(
        #[inline(always)]
        fn $name(self: $self_ty, $($arg: $ty),*) $(-> $ret)? {
            #[target_feature(enable = "neon")]
            #[inline]
            fn inner($this: $self_ty, $($arg: $ty),*) $(-> $ret)? {
                $body
            }
            // SAFETY: `self.1` is constructed iff neon is available.
            unsafe { inner(self, $($arg),*) }
        }
    )*};
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct F32VecNeon(float32x4_t, NeonDescriptor);

// SAFETY: The methods in this implementation that write to `MaybeUninit` (store_interleaved_*)
// ensure that they write valid data to the output slice without reading uninitialized memory.
unsafe impl F32SimdVec for F32VecNeon {
    type Descriptor = NeonDescriptor;

    const LEN: usize = 4;

    #[inline(always)]
    fn splat(d: Self::Descriptor, v: f32) -> Self {
        // SAFETY: We know neon is available from the safety invariant on `d`.
        Self(unsafe { vdupq_n_f32(v) }, d)
    }

    #[inline(always)]
    fn zero(d: Self::Descriptor) -> Self {
        // SAFETY: We know neon is available from the safety invariant on `d`.
        Self(unsafe { vdupq_n_f32(0.0) }, d)
    }

    #[inline(always)]
    fn load(d: Self::Descriptor, mem: &[f32]) -> Self {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know neon is available
        // from the safety invariant on `d`.
        Self(unsafe { vld1q_f32(mem.as_ptr()) }, d)
    }

    #[inline(always)]
    fn store(&self, mem: &mut [f32]) {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know neon is available
        // from the safety invariant on `d`.
        unsafe { vst1q_f32(mem.as_mut_ptr(), self.0) }
    }

    #[inline(always)]
    fn store_interleaved_2_uninit(a: Self, b: Self, dest: &mut [MaybeUninit<f32>]) {
        assert!(dest.len() >= 2 * Self::LEN);
        // SAFETY: we just checked that `dest` has enough space, and neon is available
        // from the safety invariant on the descriptor stored in `a`.
        unsafe {
            let dest_ptr = dest.as_mut_ptr() as *mut f32;
            vst2q_f32(dest_ptr, float32x4x2_t(a.0, b.0));
        }
    }

    #[inline(always)]
    fn store_interleaved_3_uninit(a: Self, b: Self, c: Self, dest: &mut [MaybeUninit<f32>]) {
        assert!(dest.len() >= 3 * Self::LEN);
        // SAFETY: `dest` has enough space and writing to `MaybeUninit<f32>` through `*mut f32` is valid.
        unsafe {
            let dest_ptr = dest.as_mut_ptr() as *mut f32;
            vst3q_f32(dest_ptr, float32x4x3_t(a.0, b.0, c.0));
        }
    }

    #[inline(always)]
    fn store_interleaved_4_uninit(
        a: Self,
        b: Self,
        c: Self,
        d: Self,
        dest: &mut [MaybeUninit<f32>],
    ) {
        assert!(dest.len() >= 4 * Self::LEN);
        // SAFETY: we just checked that `dest` has enough space, and neon is available
        // from the safety invariant on the descriptor stored in `a`.
        unsafe {
            let dest_ptr = dest.as_mut_ptr() as *mut f32;
            vst4q_f32(dest_ptr, float32x4x4_t(a.0, b.0, c.0, d.0));
        }
    }

    #[inline(always)]
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
    ) {
        #[target_feature(enable = "neon")]
        #[inline]
        fn store_interleaved_8_impl(
            a: float32x4_t,
            b: float32x4_t,
            c: float32x4_t,
            d: float32x4_t,
            e: float32x4_t,
            f: float32x4_t,
            g: float32x4_t,
            h: float32x4_t,
            dest: &mut [f32],
        ) {
            assert!(dest.len() >= 8 * F32VecNeon::LEN);
            // NEON doesn't have vst8, so we use manual interleaving
            // For 4-wide vectors, output is 32 elements: [a0,b0,c0,d0,e0,f0,g0,h0, a1,...]

            // Use zip to interleave pairs
            let ae_lo = vzip1q_f32(a, e); // [a0, e0, a1, e1]
            let ae_hi = vzip2q_f32(a, e); // [a2, e2, a3, e3]
            let bf_lo = vzip1q_f32(b, f);
            let bf_hi = vzip2q_f32(b, f);
            let cg_lo = vzip1q_f32(c, g);
            let cg_hi = vzip2q_f32(c, g);
            let dh_lo = vzip1q_f32(d, h);
            let dh_hi = vzip2q_f32(d, h);

            // Now interleave ae with bf, and cg with dh
            let aebf_0 = vzip1q_f32(ae_lo, bf_lo); // [a0, b0, e0, f0]
            let aebf_1 = vzip2q_f32(ae_lo, bf_lo); // [a1, b1, e1, f1]
            let aebf_2 = vzip1q_f32(ae_hi, bf_hi);
            let aebf_3 = vzip2q_f32(ae_hi, bf_hi);
            let cgdh_0 = vzip1q_f32(cg_lo, dh_lo); // [c0, d0, g0, h0]
            let cgdh_1 = vzip2q_f32(cg_lo, dh_lo);
            let cgdh_2 = vzip1q_f32(cg_hi, dh_hi);
            let cgdh_3 = vzip2q_f32(cg_hi, dh_hi);

            // Final interleave to get [a0,b0,c0,d0,e0,f0,g0,h0]
            let out0 = vreinterpretq_f32_f64(vzip1q_f64(
                vreinterpretq_f64_f32(aebf_0),
                vreinterpretq_f64_f32(cgdh_0),
            ));
            let out1 = vreinterpretq_f32_f64(vzip2q_f64(
                vreinterpretq_f64_f32(aebf_0),
                vreinterpretq_f64_f32(cgdh_0),
            ));
            let out2 = vreinterpretq_f32_f64(vzip1q_f64(
                vreinterpretq_f64_f32(aebf_1),
                vreinterpretq_f64_f32(cgdh_1),
            ));
            let out3 = vreinterpretq_f32_f64(vzip2q_f64(
                vreinterpretq_f64_f32(aebf_1),
                vreinterpretq_f64_f32(cgdh_1),
            ));
            let out4 = vreinterpretq_f32_f64(vzip1q_f64(
                vreinterpretq_f64_f32(aebf_2),
                vreinterpretq_f64_f32(cgdh_2),
            ));
            let out5 = vreinterpretq_f32_f64(vzip2q_f64(
                vreinterpretq_f64_f32(aebf_2),
                vreinterpretq_f64_f32(cgdh_2),
            ));
            let out6 = vreinterpretq_f32_f64(vzip1q_f64(
                vreinterpretq_f64_f32(aebf_3),
                vreinterpretq_f64_f32(cgdh_3),
            ));
            let out7 = vreinterpretq_f32_f64(vzip2q_f64(
                vreinterpretq_f64_f32(aebf_3),
                vreinterpretq_f64_f32(cgdh_3),
            ));

            // SAFETY: we just checked that dest has enough space.
            unsafe {
                let ptr = dest.as_mut_ptr();
                vst1q_f32(ptr, out0);
                vst1q_f32(ptr.add(4), out1);
                vst1q_f32(ptr.add(8), out2);
                vst1q_f32(ptr.add(12), out3);
                vst1q_f32(ptr.add(16), out4);
                vst1q_f32(ptr.add(20), out5);
                vst1q_f32(ptr.add(24), out6);
                vst1q_f32(ptr.add(28), out7);
            }
        }

        // SAFETY: neon is available from the safety invariant on the descriptor stored in `a`.
        unsafe { store_interleaved_8_impl(a.0, b.0, c.0, d.0, e.0, f.0, g.0, h.0, dest) }
    }

    #[inline(always)]
    fn load_deinterleaved_2(d: Self::Descriptor, src: &[f32]) -> (Self, Self) {
        assert!(src.len() >= 2 * Self::LEN);
        // SAFETY: we just checked that `src` has enough space, and neon is available
        // from the safety invariant on `d`.
        let float32x4x2_t(a, b) = unsafe { vld2q_f32(src.as_ptr()) };
        (Self(a, d), Self(b, d))
    }

    #[inline(always)]
    fn load_deinterleaved_3(d: Self::Descriptor, src: &[f32]) -> (Self, Self, Self) {
        assert!(src.len() >= 3 * Self::LEN);
        // SAFETY: we just checked that `src` has enough space, and neon is available
        // from the safety invariant on `d`.
        let float32x4x3_t(a, b, c) = unsafe { vld3q_f32(src.as_ptr()) };
        (Self(a, d), Self(b, d), Self(c, d))
    }

    #[inline(always)]
    fn load_deinterleaved_4(d: Self::Descriptor, src: &[f32]) -> (Self, Self, Self, Self) {
        assert!(src.len() >= 4 * Self::LEN);
        // SAFETY: we just checked that `src` has enough space, and neon is available
        // from the safety invariant on `d`.
        let float32x4x4_t(a, b, c, e) = unsafe { vld4q_f32(src.as_ptr()) };
        (Self(a, d), Self(b, d), Self(c, d), Self(e, d))
    }

    #[inline(always)]
    fn transpose_square(d: NeonDescriptor, data: &mut [[f32; 4]], stride: usize) {
        #[target_feature(enable = "neon")]
        #[inline]
        fn transpose4x4f32(d: NeonDescriptor, data: &mut [[f32; 4]], stride: usize) {
            assert!(data.len() > 3 * stride);

            let p0 = F32VecNeon::load_array(d, &data[0]).0;
            let p1 = F32VecNeon::load_array(d, &data[1 * stride]).0;
            let p2 = F32VecNeon::load_array(d, &data[2 * stride]).0;
            let p3 = F32VecNeon::load_array(d, &data[3 * stride]).0;

            // Stage 1: Transpose within each of 2x2 blocks
            let tr0 = vreinterpretq_f64_f32(vtrn1q_f32(p0, p1));
            let tr1 = vreinterpretq_f64_f32(vtrn2q_f32(p0, p1));
            let tr2 = vreinterpretq_f64_f32(vtrn1q_f32(p2, p3));
            let tr3 = vreinterpretq_f64_f32(vtrn2q_f32(p2, p3));

            // Stage 2: Transpose 2x2 grid of 2x2 blocks
            let p0 = vreinterpretq_f32_f64(vzip1q_f64(tr0, tr2));
            let p1 = vreinterpretq_f32_f64(vzip1q_f64(tr1, tr3));
            let p2 = vreinterpretq_f32_f64(vzip2q_f64(tr0, tr2));
            let p3 = vreinterpretq_f32_f64(vzip2q_f64(tr1, tr3));

            F32VecNeon(p0, d).store_array(&mut data[0]);
            F32VecNeon(p1, d).store_array(&mut data[1 * stride]);
            F32VecNeon(p2, d).store_array(&mut data[2 * stride]);
            F32VecNeon(p3, d).store_array(&mut data[3 * stride]);
        }

        /// Potentially faster variant of `transpose4x4f32` where `stride == 1`.
        #[target_feature(enable = "neon")]
        #[inline]
        fn transpose4x4f32_contiguous(d: NeonDescriptor, data: &mut [[f32; 4]]) {
            assert!(data.len() > 3);

            // Transposed load
            // SAFETY: input is verified to be large enough for this pointer.
            let float32x4x4_t(p0, p1, p2, p3) = unsafe { vld4q_f32(data.as_ptr().cast()) };

            F32VecNeon(p0, d).store_array(&mut data[0]);
            F32VecNeon(p1, d).store_array(&mut data[1]);
            F32VecNeon(p2, d).store_array(&mut data[2]);
            F32VecNeon(p3, d).store_array(&mut data[3]);
        }

        if stride == 1 {
            // SAFETY: the safety invariant on `d` guarantees neon
            unsafe {
                transpose4x4f32_contiguous(d, data);
            }
        } else {
            // SAFETY: the safety invariant on `d` guarantees neon
            unsafe {
                transpose4x4f32(d, data, stride);
            }
        }
    }

    crate::impl_f32_array_interface!();

    fn_neon! {
        fn mul_add(this: F32VecNeon, mul: F32VecNeon, add: F32VecNeon) -> F32VecNeon {
            F32VecNeon(vfmaq_f32(add.0, this.0, mul.0), this.1)
        }

        fn neg_mul_add(this: F32VecNeon, mul: F32VecNeon, add: F32VecNeon) -> F32VecNeon {
            F32VecNeon(vfmsq_f32(add.0, this.0, mul.0), this.1)
        }

        fn abs(this: F32VecNeon) -> F32VecNeon {
            F32VecNeon(vabsq_f32(this.0), this.1)
        }

        fn floor(this: F32VecNeon) -> F32VecNeon {
            F32VecNeon(vrndmq_f32(this.0), this.1)
        }

        fn sqrt(this: F32VecNeon) -> F32VecNeon {
            F32VecNeon(vsqrtq_f32(this.0), this.1)
        }

        fn neg(this: F32VecNeon) -> F32VecNeon {
            F32VecNeon(vnegq_f32(this.0), this.1)
        }

        fn copysign(this: F32VecNeon, sign: F32VecNeon) -> F32VecNeon {
            F32VecNeon(
                vbslq_f32(vdupq_n_u32(0x8000_0000), sign.0, this.0),
                this.1,
            )
        }

        fn max(this: F32VecNeon, other: F32VecNeon) -> F32VecNeon {
            F32VecNeon(vmaxq_f32(this.0, other.0), this.1)
        }

        fn min(this: F32VecNeon, other: F32VecNeon) -> F32VecNeon {
            F32VecNeon(vminq_f32(this.0, other.0), this.1)
        }

        fn gt(this: F32VecNeon, other: F32VecNeon) -> MaskNeon {
            MaskNeon(vcgtq_f32(this.0, other.0), this.1)
        }

        fn as_i32(this: F32VecNeon) -> I32VecNeon {
            I32VecNeon(vcvtq_s32_f32(this.0), this.1)
        }

        fn bitcast_to_i32(this: F32VecNeon) -> I32VecNeon {
            I32VecNeon(vreinterpretq_s32_f32(this.0), this.1)
        }

        fn round_store_u8(this: F32VecNeon, dest: &mut [u8]) {
            assert!(dest.len() >= F32VecNeon::LEN);
            // Round to nearest integer
            let rounded = vrndnq_f32(this.0);
            // Convert to i32, then to u16, then to u8
            let i32s = vcvtq_s32_f32(rounded);
            let u16s = vqmovun_s32(i32s);
            let u8s = vqmovn_u16(vcombine_u16(u16s, u16s));
            // Store lower 4 bytes
            // SAFETY: we checked dest has enough space
            unsafe {
                vst1_lane_u32::<0>(dest.as_mut_ptr() as *mut u32, vreinterpret_u32_u8(u8s));
            }
        }

        fn round_store_u16(this: F32VecNeon, dest: &mut [u16]) {
            assert!(dest.len() >= F32VecNeon::LEN);
            // Round to nearest integer
            let rounded = vrndnq_f32(this.0);
            // Convert to i32, then to u16
            let i32s = vcvtq_s32_f32(rounded);
            let u16s = vqmovun_s32(i32s);
            // Store 4 u16s (8 bytes)
            // SAFETY: we checked dest has enough space
            unsafe {
                vst1_u16(dest.as_mut_ptr(), u16s);
            }
        }

        fn store_f16_bits(this: F32VecNeon, dest: &mut [u16]) {
            assert!(dest.len() >= F32VecNeon::LEN);
            // Use inline asm because Rust stdarch incorrectly requires fp16 target feature
            // for vcvt_f16_f32 (fixed in https://github.com/rust-lang/stdarch/pull/1978)
            let f16_bits: uint16x4_t;
            // SAFETY: NEON is available (guaranteed by descriptor), dest has enough space
            unsafe {
                std::arch::asm!(
                    "fcvtn {out:v}.4h, {inp:v}.4s",
                    inp = in(vreg) this.0,
                    out = out(vreg) f16_bits,
                    options(pure, nomem, nostack),
                );
                vst1_u16(dest.as_mut_ptr(), f16_bits);
            }
        }
    }

    #[inline(always)]
    fn load_f16_bits(d: Self::Descriptor, mem: &[u16]) -> Self {
        assert!(mem.len() >= Self::LEN);
        // Use inline asm because Rust stdarch incorrectly requires fp16 target feature
        // for vcvt_f32_f16 (fixed in https://github.com/rust-lang/stdarch/pull/1978)
        let result: float32x4_t;
        // SAFETY: NEON is available (guaranteed by descriptor), mem has enough space
        unsafe {
            let f16_bits = vld1_u16(mem.as_ptr());
            std::arch::asm!(
                "fcvtl {out:v}.4s, {inp:v}.4h",
                inp = in(vreg) f16_bits,
                out = out(vreg) result,
                options(pure, nomem, nostack),
            );
        }
        F32VecNeon(result, d)
    }

    #[inline(always)]
    fn prepare_table_bf16_8(_d: NeonDescriptor, table: &[f32; 8]) -> Bf16Table8Neon {
        #[target_feature(enable = "neon")]
        #[inline]
        fn prepare_impl(table: &[f32; 8]) -> uint8x16_t {
            // Convert f32 table to BF16 packed in 128 bits (16 bytes for 8 entries)
            // BF16 is the high 16 bits of f32
            // SAFETY: neon is available from target_feature, and `table` is large
            // enough for the loads.
            let (table_lo, table_hi) =
                unsafe { (vld1q_f32(table.as_ptr()), vld1q_f32(table.as_ptr().add(4))) };

            // Reinterpret as u32 to extract high 16 bits
            let table_lo_u32 = vreinterpretq_u32_f32(table_lo);
            let table_hi_u32 = vreinterpretq_u32_f32(table_hi);

            // Shift right by 16 AND narrow to 16-bit in one instruction
            let bf16_lo_u16 = vshrn_n_u32::<16>(table_lo_u32);
            let bf16_hi_u16 = vshrn_n_u32::<16>(table_hi_u32);

            // Combine into 8 x u16 = 16 bytes
            let bf16_table_u16 = vcombine_u16(bf16_lo_u16, bf16_hi_u16);
            vreinterpretq_u8_u16(bf16_table_u16)
        }
        // SAFETY: neon is available from the safety invariant on the descriptor
        Bf16Table8Neon(unsafe { prepare_impl(table) })
    }

    #[inline(always)]
    fn table_lookup_bf16_8(d: NeonDescriptor, table: Bf16Table8Neon, indices: I32VecNeon) -> Self {
        #[target_feature(enable = "neon")]
        #[inline]
        fn lookup_impl(bf16_table: uint8x16_t, indices: int32x4_t) -> float32x4_t {
            // Build shuffle mask efficiently using arithmetic on 32-bit indices.
            // For each index i (0-7), we need to select bytes [2*i, 2*i+1] from bf16_table
            // and place them in the high 16 bits of each 32-bit f32 lane (bytes 2,3),
            // with bytes 0,1 set to zero (using 0x80 which gives 0 in vqtbl1q).
            //
            // Output byte pattern per lane (little-endian): [0x80, 0x80, 2*i, 2*i+1]
            // As a 32-bit value: 0x80 | (0x80 << 8) | (2*i << 16) | ((2*i+1) << 24)
            //                  = 0x8080 | (i << 17) | (i << 25) | (1 << 24)
            //                  = (i << 17) | (i << 25) | 0x01008080
            let indices_u32 = vreinterpretq_u32_s32(indices);
            let shl17 = vshlq_n_u32::<17>(indices_u32);
            let shl25 = vshlq_n_u32::<25>(indices_u32);
            let base = vdupq_n_u32(0x01008080);
            let shuffle_mask = vorrq_u32(vorrq_u32(shl17, shl25), base);

            // Perform the table lookup (out of range indices give 0)
            let result = vqtbl1q_u8(bf16_table, vreinterpretq_u8_u32(shuffle_mask));

            // Result has bf16 in high 16 bits of each 32-bit lane = valid f32
            vreinterpretq_f32_u8(result)
        }
        // SAFETY: neon is available from the safety invariant on the descriptor
        F32VecNeon(unsafe { lookup_impl(table.0, indices.0) }, d)
    }
}

impl Add<F32VecNeon> for F32VecNeon {
    type Output = Self;
    fn_neon! {
        fn add(this: F32VecNeon, rhs: F32VecNeon) -> F32VecNeon {
            F32VecNeon(vaddq_f32(this.0, rhs.0), this.1)
        }
    }
}

impl Sub<F32VecNeon> for F32VecNeon {
    type Output = Self;
    fn_neon! {
        fn sub(this: F32VecNeon, rhs: F32VecNeon) -> F32VecNeon {
            F32VecNeon(vsubq_f32(this.0, rhs.0), this.1)
        }
    }
}

impl Mul<F32VecNeon> for F32VecNeon {
    type Output = Self;
    fn_neon! {
        fn mul(this: F32VecNeon, rhs: F32VecNeon) -> F32VecNeon {
            F32VecNeon(vmulq_f32(this.0, rhs.0), this.1)
        }
    }
}

impl Div<F32VecNeon> for F32VecNeon {
    type Output = Self;
    fn_neon! {
        fn div(this: F32VecNeon, rhs: F32VecNeon) -> F32VecNeon {
            F32VecNeon(vdivq_f32(this.0, rhs.0), this.1)
        }
    }
}

impl AddAssign<F32VecNeon> for F32VecNeon {
    fn_neon! {
        fn add_assign(this: &mut F32VecNeon, rhs: F32VecNeon) {
            this.0 = vaddq_f32(this.0, rhs.0);
        }
    }
}

impl SubAssign<F32VecNeon> for F32VecNeon {
    fn_neon! {
        fn sub_assign(this: &mut F32VecNeon, rhs: F32VecNeon) {
            this.0 = vsubq_f32(this.0, rhs.0);
        }
    }
}

impl MulAssign<F32VecNeon> for F32VecNeon {
    fn_neon! {
        fn mul_assign(this: &mut F32VecNeon, rhs: F32VecNeon) {
            this.0 = vmulq_f32(this.0, rhs.0);
        }
    }
}

impl DivAssign<F32VecNeon> for F32VecNeon {
    fn_neon! {
        fn div_assign(this: &mut F32VecNeon, rhs: F32VecNeon) {
            this.0 = vdivq_f32(this.0, rhs.0);
        }
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct I32VecNeon(int32x4_t, NeonDescriptor);

impl I32SimdVec for I32VecNeon {
    type Descriptor = NeonDescriptor;

    const LEN: usize = 4;

    #[inline(always)]
    fn splat(d: Self::Descriptor, v: i32) -> Self {
        // SAFETY: We know neon is available from the safety invariant on `d`.
        Self(unsafe { vdupq_n_s32(v) }, d)
    }

    #[inline(always)]
    fn load(d: Self::Descriptor, mem: &[i32]) -> Self {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know neon is available
        // from the safety invariant on `d`.
        Self(unsafe { vld1q_s32(mem.as_ptr()) }, d)
    }

    #[inline(always)]
    fn store(&self, mem: &mut [i32]) {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know neon is available
        // from the safety invariant on `d`.
        unsafe { vst1q_s32(mem.as_mut_ptr(), self.0) }
    }

    fn_neon! {
        fn abs(this: I32VecNeon) -> I32VecNeon {
            I32VecNeon(vabsq_s32(this.0), this.1)
        }

        fn as_f32(this: I32VecNeon) -> F32VecNeon {
            F32VecNeon(vcvtq_f32_s32(this.0), this.1)
        }

        fn bitcast_to_f32(this: I32VecNeon) -> F32VecNeon {
            F32VecNeon(vreinterpretq_f32_s32(this.0), this.1)
        }

        fn bitcast_to_u32(this: I32VecNeon) -> U32VecNeon {
            U32VecNeon(vreinterpretq_u32_s32(this.0), this.1)
        }

        fn gt(this: I32VecNeon, other: I32VecNeon) -> MaskNeon {
            MaskNeon(vcgtq_s32(this.0, other.0), this.1)
        }

        fn lt_zero(this: I32VecNeon) -> MaskNeon {
            MaskNeon(vcltzq_s32(this.0), this.1)
        }

        fn eq(this: I32VecNeon, other: I32VecNeon) -> MaskNeon {
            MaskNeon(vceqq_s32(this.0, other.0), this.1)
        }

        fn eq_zero(this: I32VecNeon) -> MaskNeon {
            MaskNeon(vceqzq_s32(this.0), this.1)
        }

        fn mul_wide_take_high(this: I32VecNeon, rhs: I32VecNeon) -> I32VecNeon {
            let l = vmull_s32(vget_low_s32(this.0), vget_low_s32(rhs.0));
            let l = vreinterpretq_s32_s64(l);
            let h = vmull_high_s32(this.0, rhs.0);
            let h = vreinterpretq_s32_s64(h);
            I32VecNeon(vuzp2q_s32(l, h), this.1)
        }
    }

    #[inline(always)]
    fn shl<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        // SAFETY: We know neon is available from the safety invariant on `self.1`.
        unsafe { Self(vshlq_n_s32::<AMOUNT_I>(self.0), self.1) }
    }

    #[inline(always)]
    fn shr<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        // SAFETY: We know neon is available from the safety invariant on `self.1`.
        unsafe { Self(vshrq_n_s32::<AMOUNT_I>(self.0), self.1) }
    }

    #[inline(always)]
    fn store_u16(self, dest: &mut [u16]) {
        assert!(dest.len() >= Self::LEN);
        // SAFETY: We know neon is available from the safety invariant on `self.1`,
        // and we just checked that `dest` has enough space.
        unsafe {
            // vmovn narrows i32 to i16 by taking the lower 16 bits
            let narrowed = vmovn_s32(self.0);
            vst1_u16(dest.as_mut_ptr(), vreinterpret_u16_s16(narrowed));
        }
    }
}

impl Add<I32VecNeon> for I32VecNeon {
    type Output = I32VecNeon;
    fn_neon! {
        fn add(this: I32VecNeon, rhs: I32VecNeon) -> I32VecNeon {
            I32VecNeon(vaddq_s32(this.0, rhs.0), this.1)
        }
    }
}

impl Sub<I32VecNeon> for I32VecNeon {
    type Output = I32VecNeon;
    fn_neon! {
        fn sub(this: I32VecNeon, rhs: I32VecNeon) -> I32VecNeon {
            I32VecNeon(vsubq_s32(this.0, rhs.0), this.1)
        }
    }
}

impl Mul<I32VecNeon> for I32VecNeon {
    type Output = I32VecNeon;
    fn_neon! {
        fn mul(this: I32VecNeon, rhs: I32VecNeon) -> I32VecNeon {
            I32VecNeon(vmulq_s32(this.0, rhs.0), this.1)
        }
    }
}

impl Neg for I32VecNeon {
    type Output = I32VecNeon;
    fn_neon! {
        fn neg(this: I32VecNeon) -> I32VecNeon {
            I32VecNeon(vnegq_s32(this.0), this.1)
        }
    }
}

impl BitAnd<I32VecNeon> for I32VecNeon {
    type Output = I32VecNeon;
    fn_neon! {
        fn bitand(this: I32VecNeon, rhs: I32VecNeon) -> I32VecNeon {
            I32VecNeon(vandq_s32(this.0, rhs.0), this.1)
        }
    }
}

impl BitOr<I32VecNeon> for I32VecNeon {
    type Output = I32VecNeon;
    fn_neon! {
        fn bitor(this: I32VecNeon, rhs: I32VecNeon) -> I32VecNeon {
            I32VecNeon(vorrq_s32(this.0, rhs.0), this.1)
        }
    }
}

impl BitXor<I32VecNeon> for I32VecNeon {
    type Output = I32VecNeon;
    fn_neon! {
        fn bitxor(this: I32VecNeon, rhs: I32VecNeon) -> I32VecNeon {
            I32VecNeon(veorq_s32(this.0, rhs.0), this.1)
        }
    }
}

impl AddAssign<I32VecNeon> for I32VecNeon {
    fn_neon! {
        fn add_assign(this: &mut I32VecNeon, rhs: I32VecNeon) {
            this.0 = vaddq_s32(this.0, rhs.0)
        }
    }
}

impl SubAssign<I32VecNeon> for I32VecNeon {
    fn_neon! {
        fn sub_assign(this: &mut I32VecNeon, rhs: I32VecNeon) {
            this.0 = vsubq_s32(this.0, rhs.0)
        }
    }
}

impl MulAssign<I32VecNeon> for I32VecNeon {
    fn_neon! {
        fn mul_assign(this: &mut I32VecNeon, rhs: I32VecNeon) {
            this.0 = vmulq_s32(this.0, rhs.0)
        }
    }
}

impl BitAndAssign<I32VecNeon> for I32VecNeon {
    fn_neon! {
        fn bitand_assign(this: &mut I32VecNeon, rhs: I32VecNeon) {
            this.0 = vandq_s32(this.0, rhs.0);
        }
    }
}

impl BitOrAssign<I32VecNeon> for I32VecNeon {
    fn_neon! {
        fn bitor_assign(this: &mut I32VecNeon, rhs: I32VecNeon) {
            this.0 = vorrq_s32(this.0, rhs.0);
        }
    }
}

impl BitXorAssign<I32VecNeon> for I32VecNeon {
    fn_neon! {
        fn bitxor_assign(this: &mut I32VecNeon, rhs: I32VecNeon) {
            this.0 = veorq_s32(this.0, rhs.0);
        }
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct U32VecNeon(uint32x4_t, NeonDescriptor);

impl U32SimdVec for U32VecNeon {
    type Descriptor = NeonDescriptor;

    const LEN: usize = 4;

    fn_neon! {
        fn bitcast_to_i32(this: U32VecNeon) -> I32VecNeon {
            I32VecNeon(vreinterpretq_s32_u32(this.0), this.1)
        }
    }

    #[inline(always)]
    fn shr<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        // SAFETY: We know neon is available from the safety invariant on `self.1`.
        unsafe { Self(vshrq_n_u32::<AMOUNT_I>(self.0), self.1) }
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct MaskNeon(uint32x4_t, NeonDescriptor);

impl SimdMask for MaskNeon {
    type Descriptor = NeonDescriptor;

    fn_neon! {
        fn if_then_else_f32(
            this: MaskNeon,
            if_true: F32VecNeon,
            if_false: F32VecNeon,
        ) -> F32VecNeon {
            F32VecNeon(vbslq_f32(this.0, if_true.0, if_false.0), this.1)
        }

        fn if_then_else_i32(
            this: MaskNeon,
            if_true: I32VecNeon,
            if_false: I32VecNeon,
        ) -> I32VecNeon {
            I32VecNeon(vbslq_s32(this.0, if_true.0, if_false.0), this.1)
        }

        fn maskz_i32(this: MaskNeon, v: I32VecNeon) -> I32VecNeon {
            I32VecNeon(vbicq_s32(v.0, vreinterpretq_s32_u32(this.0)), this.1)
        }

        fn andnot(this: MaskNeon, rhs: MaskNeon) -> MaskNeon {
            MaskNeon(vbicq_u32(rhs.0, this.0), this.1)
        }

        fn all(this: MaskNeon) -> bool {
            vminvq_u32(this.0) == u32::MAX
        }
    }
}

impl BitAnd<MaskNeon> for MaskNeon {
    type Output = MaskNeon;
    fn_neon! {
        fn bitand(this: MaskNeon, rhs: MaskNeon) -> MaskNeon {
            MaskNeon(vandq_u32(this.0, rhs.0), this.1)
        }
    }
}

impl BitOr<MaskNeon> for MaskNeon {
    type Output = MaskNeon;
    fn_neon! {
        fn bitor(this: MaskNeon, rhs: MaskNeon) -> MaskNeon {
            MaskNeon(vorrq_u32(this.0, rhs.0), this.1)
        }
    }
}
