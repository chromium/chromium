#[macro_use]
mod wasm_simd_common;
#[macro_use]
mod wasm_simd_vector;

#[macro_use]
pub mod wasm_simd_butterflies;
pub mod wasm_simd_prime_butterflies;
pub mod wasm_simd_radix4;

mod wasm_simd_utils;

pub mod wasm_simd_planner;

use crate::FftNum;
use core::arch::wasm32::v128;

pub use self::wasm_simd_butterflies::*;
pub use self::wasm_simd_radix4::*;
use self::wasm_simd_vector::WasmVector;
use self::wasm_simd_vector::WasmVector32;
use self::wasm_simd_vector::WasmVector64;

pub trait WasmNum: FftNum {
    type VectorType: WasmVector<ScalarType = Self>;
    fn wrap(input: v128) -> Self::VectorType;
}

impl WasmNum for f32 {
    type VectorType = WasmVector32;

    fn wrap(input: v128) -> Self::VectorType {
        WasmVector32(input)
    }
}
impl WasmNum for f64 {
    type VectorType = WasmVector64;

    fn wrap(input: v128) -> Self::VectorType {
        WasmVector64(input)
    }
}
