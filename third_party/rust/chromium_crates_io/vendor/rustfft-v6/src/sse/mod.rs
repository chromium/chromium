#[macro_use]
mod sse_common;
#[macro_use]
mod sse_vector;

#[macro_use]
pub mod sse_butterflies;
pub mod sse_prime_butterflies;
pub mod sse_radix4;

mod sse_utils;

pub mod sse_planner;

use std::arch::x86_64::__m128;
use std::arch::x86_64::__m128d;

use crate::FftNum;

use sse_vector::SseVector;

pub trait SseNum: FftNum {
    type VectorType: SseVector<ScalarType = Self>;
}

impl SseNum for f32 {
    type VectorType = __m128;
}
impl SseNum for f64 {
    type VectorType = __m128d;
}
