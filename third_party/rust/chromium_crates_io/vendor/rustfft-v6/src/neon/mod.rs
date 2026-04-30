#[macro_use]
mod neon_common;
#[macro_use]
mod neon_vector;

#[macro_use]
pub mod neon_butterflies;
pub mod neon_prime_butterflies;
pub mod neon_radix4;

mod neon_utils;

pub mod neon_planner;

use std::arch::aarch64::{float32x4_t, float64x2_t};

use crate::FftNum;
use neon_vector::NeonVector;

pub trait NeonNum: FftNum {
    type VectorType: NeonVector<ScalarType = Self>;
}

impl NeonNum for f32 {
    type VectorType = float32x4_t;
}
impl NeonNum for f64 {
    type VectorType = float64x2_t;
}
