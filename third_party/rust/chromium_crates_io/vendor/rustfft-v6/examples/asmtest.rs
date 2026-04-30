//! This example is mean to be used for inspecting the generated assembly.
//! This can be interesting when working with simd intrinsics.
//!
//! To use:
//! - Mark the function that should be investigated with `#[inline(never)]`.
//! - If needed, add any required feature to the function, for example `#[target_feature(enable = "sse4.1")]`
//! - Change the code below to use the changed function.
//!   Currently it is set up to look at the f32 version of the SSE 4-point butterfly.  
//!   It uses the FftPlannerSse to plan a length 4 FFT, that will use the modified butterfly.
//! - Ask rustc to output assembly code:
//!   `cargo rustc --release --features sse --example asmtest -- --emit=asm`
//! - This will create a file at `target/release/examples/asmtest-0123456789abcdef.s` (with a random number in the filename).
//! - Open this file and search for the function.

use rustfft::num_complex::Complex32;
//use rustfft::num_complex::Complex64;
//use rustfft::FftPlannerScalar;
use rustfft::FftPlannerSse;
//use rustfft::FftPlannerNeon;

fn main() {
    //let mut planner = FftPlannerScalar::new();
    let mut planner = FftPlannerSse::new().unwrap();
    //let mut planner = FftPlannerNeon::new().unwrap();
    let fft = planner.plan_fft_forward(4);

    let mut buffer = vec![Complex32::new(0.0, 0.0); 100];
    fft.process(&mut buffer);
}
