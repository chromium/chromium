#![feature(test)]
extern crate rustfft;
extern crate test;

use rustfft::num_complex::Complex;
use rustfft::num_traits::Zero;
use rustfft::Fft;
use std::sync::Arc;
use test::Bencher;


/// Times just the FFT execution (not allocation and pre-calculation)
/// for a given length
fn bench_planned_f32(b: &mut Bencher, len: usize) {
    let mut planner = rustfft::FftPlannerNeon::new().unwrap();
    let fft: Arc<dyn Fft<f32>> = planner.plan_fft_forward(len);
    assert_eq!(fft.len(), len);

    let mut buffer = vec![Complex::zero(); len];
    let mut scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| {
        fft.process_with_scratch(&mut buffer, &mut scratch);
    });
}
fn bench_planned_f64(b: &mut Bencher, len: usize) {
    let mut planner = rustfft::FftPlannerNeon::new().unwrap();
    let fft: Arc<dyn Fft<f64>> = planner.plan_fft_forward(len);
    assert_eq!(fft.len(), len);

    let mut buffer = vec![Complex::zero(); len];
    let mut scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| {
        fft.process_with_scratch(&mut buffer, &mut scratch);
    });
}

/// Times just the FFT execution (not allocation and pre-calculation)
/// for a given length.
/// Run the fft on a 10*len vector, similar to how the butterflies are often used.
fn bench_planned_multi_f32(b: &mut Bencher, len: usize) {
    let mut planner = rustfft::FftPlannerNeon::new().unwrap();
    let fft: Arc<dyn Fft<f32>> = planner.plan_fft_forward(len);

    let mut buffer = vec![Complex::zero(); len * 10];
    let mut scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| {
        fft.process_with_scratch(&mut buffer, &mut scratch);
    });
}
fn bench_planned_multi_f64(b: &mut Bencher, len: usize) {
    let mut planner = rustfft::FftPlannerNeon::new().unwrap();
    let fft: Arc<dyn Fft<f64>> = planner.plan_fft_forward(len);

    let mut buffer = vec![Complex::zero(); len * 10];
    let mut scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| {
        fft.process_with_scratch(&mut buffer, &mut scratch);
    });
}

// All butterflies
#[bench] fn neon_butterfly32_02(b: &mut Bencher) { bench_planned_multi_f32(b, 2);}
#[bench] fn neon_butterfly32_03(b: &mut Bencher) { bench_planned_multi_f32(b, 3);}
#[bench] fn neon_butterfly32_04(b: &mut Bencher) { bench_planned_multi_f32(b, 4);}
#[bench] fn neon_butterfly32_05(b: &mut Bencher) { bench_planned_multi_f32(b, 5);}
#[bench] fn neon_butterfly32_06(b: &mut Bencher) { bench_planned_multi_f32(b, 6);}
#[bench] fn neon_butterfly32_07(b: &mut Bencher) { bench_planned_multi_f32(b, 7);}
#[bench] fn neon_butterfly32_08(b: &mut Bencher) { bench_planned_multi_f32(b, 8);}
#[bench] fn neon_butterfly32_09(b: &mut Bencher) { bench_planned_multi_f32(b, 9);}
#[bench] fn neon_butterfly32_10(b: &mut Bencher) { bench_planned_multi_f32(b, 10);}
#[bench] fn neon_butterfly32_11(b: &mut Bencher) { bench_planned_multi_f32(b, 11);}
#[bench] fn neon_butterfly32_12(b: &mut Bencher) { bench_planned_multi_f32(b, 12);}
#[bench] fn neon_butterfly32_13(b: &mut Bencher) { bench_planned_multi_f32(b, 13);}
#[bench] fn neon_butterfly32_15(b: &mut Bencher) { bench_planned_multi_f32(b, 15);}
#[bench] fn neon_butterfly32_16(b: &mut Bencher) { bench_planned_multi_f32(b, 16);}
#[bench] fn neon_butterfly32_17(b: &mut Bencher) { bench_planned_multi_f32(b, 17);}
#[bench] fn neon_butterfly32_19(b: &mut Bencher) { bench_planned_multi_f32(b, 19);}
#[bench] fn neon_butterfly32_23(b: &mut Bencher) { bench_planned_multi_f32(b, 23);}
#[bench] fn neon_butterfly32_29(b: &mut Bencher) { bench_planned_multi_f32(b, 29);}
#[bench] fn neon_butterfly32_31(b: &mut Bencher) { bench_planned_multi_f32(b, 31);}
#[bench] fn neon_butterfly32_32(b: &mut Bencher) { bench_planned_multi_f32(b, 32);}


#[bench] fn neon_prime_butterfly32_07(b: &mut Bencher) { bench_planned_multi_f32(b, 7);}
#[bench] fn neon_prime_butterfly32_11(b: &mut Bencher) { bench_planned_multi_f32(b, 11);}
#[bench] fn neon_prime_butterfly32_13(b: &mut Bencher) { bench_planned_multi_f32(b, 13);}
#[bench] fn neon_prime_butterfly32_17(b: &mut Bencher) { bench_planned_multi_f32(b, 17);}
#[bench] fn neon_prime_butterfly32_19(b: &mut Bencher) { bench_planned_multi_f32(b, 19);}
#[bench] fn neon_prime_butterfly32_23(b: &mut Bencher) { bench_planned_multi_f32(b, 23);}
#[bench] fn neon_prime_butterfly32_29(b: &mut Bencher) { bench_planned_multi_f32(b, 29);}
#[bench] fn neon_prime_butterfly32_31(b: &mut Bencher) { bench_planned_multi_f32(b, 31);}

#[bench] fn neon_prime_butterfly64_07(b: &mut Bencher) { bench_planned_multi_f64(b, 7);}
#[bench] fn neon_prime_butterfly64_11(b: &mut Bencher) { bench_planned_multi_f64(b, 11);}
#[bench] fn neon_prime_butterfly64_13(b: &mut Bencher) { bench_planned_multi_f64(b, 13);}
#[bench] fn neon_prime_butterfly64_17(b: &mut Bencher) { bench_planned_multi_f64(b, 17);}
#[bench] fn neon_prime_butterfly64_19(b: &mut Bencher) { bench_planned_multi_f64(b, 19);}
#[bench] fn neon_prime_butterfly64_23(b: &mut Bencher) { bench_planned_multi_f64(b, 23);}
#[bench] fn neon_prime_butterfly64_29(b: &mut Bencher) { bench_planned_multi_f64(b, 29);}
#[bench] fn neon_prime_butterfly64_31(b: &mut Bencher) { bench_planned_multi_f64(b, 31);}

#[bench] fn neon_butterfly64_02(b: &mut Bencher) { bench_planned_multi_f64(b, 2);}
#[bench] fn neon_butterfly64_03(b: &mut Bencher) { bench_planned_multi_f64(b, 3);}
#[bench] fn neon_butterfly64_04(b: &mut Bencher) { bench_planned_multi_f64(b, 4);}
#[bench] fn neon_butterfly64_05(b: &mut Bencher) { bench_planned_multi_f64(b, 5);}
#[bench] fn neon_butterfly64_06(b: &mut Bencher) { bench_planned_multi_f64(b, 6);}
#[bench] fn neon_butterfly64_07(b: &mut Bencher) { bench_planned_multi_f64(b, 7);}
#[bench] fn neon_butterfly64_08(b: &mut Bencher) { bench_planned_multi_f64(b, 8);}
#[bench] fn neon_butterfly64_09(b: &mut Bencher) { bench_planned_multi_f64(b, 9);}
#[bench] fn neon_butterfly64_10(b: &mut Bencher) { bench_planned_multi_f64(b, 10);}
#[bench] fn neon_butterfly64_11(b: &mut Bencher) { bench_planned_multi_f64(b, 11);}
#[bench] fn neon_butterfly64_12(b: &mut Bencher) { bench_planned_multi_f64(b, 12);}
#[bench] fn neon_butterfly64_13(b: &mut Bencher) { bench_planned_multi_f64(b, 13);}
#[bench] fn neon_butterfly64_15(b: &mut Bencher) { bench_planned_multi_f64(b, 15);}
#[bench] fn neon_butterfly64_16(b: &mut Bencher) { bench_planned_multi_f64(b, 16);}
#[bench] fn neon_butterfly64_17(b: &mut Bencher) { bench_planned_multi_f64(b, 17);}
#[bench] fn neon_butterfly64_19(b: &mut Bencher) { bench_planned_multi_f64(b, 19);}
#[bench] fn neon_butterfly64_23(b: &mut Bencher) { bench_planned_multi_f64(b, 23);}
#[bench] fn neon_butterfly64_29(b: &mut Bencher) { bench_planned_multi_f64(b, 29);}
#[bench] fn neon_butterfly64_31(b: &mut Bencher) { bench_planned_multi_f64(b, 31);}
#[bench] fn neon_butterfly64_32(b: &mut Bencher) { bench_planned_multi_f64(b, 32);}

// Powers of 2
#[bench] fn neon_planned32_p2_00000064(b: &mut Bencher) { bench_planned_f32(b, 64); }
#[bench] fn neon_planned32_p2_00000128(b: &mut Bencher) { bench_planned_f32(b, 128); }
#[bench] fn neon_planned32_p2_00000256(b: &mut Bencher) { bench_planned_f32(b, 256); }
#[bench] fn neon_planned32_p2_00000512(b: &mut Bencher) { bench_planned_f32(b, 512); }
#[bench] fn neon_planned32_p2_00001024(b: &mut Bencher) { bench_planned_f32(b, 1024); }
#[bench] fn neon_planned32_p2_00002048(b: &mut Bencher) { bench_planned_f32(b, 2048); }
#[bench] fn neon_planned32_p2_00004096(b: &mut Bencher) { bench_planned_f32(b, 4096); }
#[bench] fn neon_planned32_p2_00016384(b: &mut Bencher) { bench_planned_f32(b, 16384); }
#[bench] fn neon_planned32_p2_00065536(b: &mut Bencher) { bench_planned_f32(b, 65536); }
#[bench] fn neon_planned32_p2_01048576(b: &mut Bencher) { bench_planned_f32(b, 1048576); }

#[bench] fn neon_planned64_p2_00000064(b: &mut Bencher) { bench_planned_f64(b, 64); }
#[bench] fn neon_planned64_p2_00000128(b: &mut Bencher) { bench_planned_f64(b, 128); }
#[bench] fn neon_planned64_p2_00000256(b: &mut Bencher) { bench_planned_f64(b, 256); }
#[bench] fn neon_planned64_p2_00000512(b: &mut Bencher) { bench_planned_f64(b, 512); }
#[bench] fn neon_planned64_p2_00001024(b: &mut Bencher) { bench_planned_f64(b, 1024); }
#[bench] fn neon_planned64_p2_00002048(b: &mut Bencher) { bench_planned_f64(b, 2048); }
#[bench] fn neon_planned64_p2_00004096(b: &mut Bencher) { bench_planned_f64(b, 4096); }
#[bench] fn neon_planned64_p2_00016384(b: &mut Bencher) { bench_planned_f64(b, 16384); }
#[bench] fn neon_planned64_p2_00065536(b: &mut Bencher) { bench_planned_f64(b, 65536); }
#[bench] fn neon_planned64_p2_01048576(b: &mut Bencher) { bench_planned_f64(b, 1048576); }


// Powers of 7
#[bench] fn neon_planned32_p7_00343(b: &mut Bencher) { bench_planned_f32(b,   343); }
#[bench] fn neon_planned32_p7_02401(b: &mut Bencher) { bench_planned_f32(b,  2401); }
#[bench] fn neon_planned32_p7_16807(b: &mut Bencher) { bench_planned_f32(b, 16807); }

#[bench] fn neon_planned64_p7_00343(b: &mut Bencher) { bench_planned_f64(b,   343); }
#[bench] fn neon_planned64_p7_02401(b: &mut Bencher) { bench_planned_f64(b,  2401); }
#[bench] fn neon_planned64_p7_16807(b: &mut Bencher) { bench_planned_f64(b, 16807); }

// Prime lengths
#[bench] fn neon_planned32_prime_0149(b: &mut Bencher)     { bench_planned_f32(b,  149); }
#[bench] fn neon_planned32_prime_0151(b: &mut Bencher)     { bench_planned_f32(b,  151); }
#[bench] fn neon_planned32_prime_0251(b: &mut Bencher)     { bench_planned_f32(b,  251); }
#[bench] fn neon_planned32_prime_0257(b: &mut Bencher)     { bench_planned_f32(b,  257); }
#[bench] fn neon_planned32_prime_2017(b: &mut Bencher)     { bench_planned_f32(b,  2017); }
#[bench] fn neon_planned32_prime_2879(b: &mut Bencher)     { bench_planned_f32(b,  2879); }
#[bench] fn neon_planned32_prime_65521(b: &mut Bencher)    { bench_planned_f32(b, 65521); }
#[bench] fn neon_planned32_prime_746497(b: &mut Bencher)   { bench_planned_f32(b,746497); }

#[bench] fn neon_planned64_prime_0149(b: &mut Bencher)     { bench_planned_f64(b,  149); }
#[bench] fn neon_planned64_prime_0151(b: &mut Bencher)     { bench_planned_f64(b,  151); }
#[bench] fn neon_planned64_prime_0251(b: &mut Bencher)     { bench_planned_f64(b,  251); }
#[bench] fn neon_planned64_prime_0257(b: &mut Bencher)     { bench_planned_f64(b,  257); }
#[bench] fn neon_planned64_prime_2017(b: &mut Bencher)     { bench_planned_f64(b,  2017); }
#[bench] fn neon_planned64_prime_2879(b: &mut Bencher)     { bench_planned_f64(b,  2879); }
#[bench] fn neon_planned64_prime_65521(b: &mut Bencher)    { bench_planned_f64(b, 65521); }
#[bench] fn neon_planned64_prime_746497(b: &mut Bencher)   { bench_planned_f64(b,746497); }

// small mixed composites
#[bench] fn neon_planned32_composite_000018(b: &mut Bencher) { bench_planned_f32(b,  00018); }
#[bench] fn neon_planned32_composite_000360(b: &mut Bencher) { bench_planned_f32(b,  00360); }
#[bench] fn neon_planned32_composite_001200(b: &mut Bencher) { bench_planned_f32(b,  01200); }
#[bench] fn neon_planned32_composite_044100(b: &mut Bencher) { bench_planned_f32(b,  44100); }
#[bench] fn neon_planned32_composite_048000(b: &mut Bencher) { bench_planned_f32(b,  48000); }
#[bench] fn neon_planned32_composite_046656(b: &mut Bencher) { bench_planned_f32(b,  46656); }

#[bench] fn neon_planned64_composite_000018(b: &mut Bencher) { bench_planned_f64(b,  00018); }
#[bench] fn neon_planned64_composite_000360(b: &mut Bencher) { bench_planned_f64(b,  00360); }
#[bench] fn neon_planned64_composite_001200(b: &mut Bencher) { bench_planned_f64(b,  01200); }
#[bench] fn neon_planned64_composite_044100(b: &mut Bencher) { bench_planned_f64(b,  44100); }
#[bench] fn neon_planned64_composite_048000(b: &mut Bencher) { bench_planned_f64(b,  48000); }
#[bench] fn neon_planned64_composite_046656(b: &mut Bencher) { bench_planned_f64(b,  46656); }



