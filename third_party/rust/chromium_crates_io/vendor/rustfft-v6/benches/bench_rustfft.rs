#![allow(bare_trait_objects)]
#![allow(non_snake_case)]
#![feature(test)]
extern crate test;
extern crate rustfft;

use std::sync::Arc;
use test::Bencher;
use rustfft::{Direction, FftNum, Fft, FftDirection, Length};
use rustfft::num_complex::Complex;
use rustfft::num_traits::Zero;
use rustfft::algorithm::*;
use rustfft::algorithm::butterflies::*;


#[allow(unused)]
struct Noop {
    len: usize,
    direction: FftDirection,
}
impl<T: FftNum> Fft<T> for Noop {
    fn process_with_scratch(&self, _buffer: &mut [Complex<T>], _scratch: &mut [Complex<T>]) {}
    fn process_outofplace_with_scratch(&self, _input: &mut [Complex<T>], _output: &mut [Complex<T>], _scratch: &mut [Complex<T>]) {}
    fn get_inplace_scratch_len(&self) -> usize { self.len }
    fn get_outofplace_scratch_len(&self) -> usize { 0 }
    fn process_immutable_with_scratch(
        &self,
        _input: &[Complex<T>],
        _output: &mut [Complex<T>],
        _scratch: &mut [Complex<T>],
    ) {}
    fn get_immutable_scratch_len(&self) -> usize { 0 }
}
impl Length for Noop {
    fn len(&self) -> usize { self.len }
}
impl Direction for Noop {
    fn fft_direction(&self) -> FftDirection { self.direction }
}

/// Times just the FFT execution (not allocation and pre-calculation)
/// for a given length
fn bench_planned_f32(b: &mut Bencher, len: usize) {

    let mut planner = rustfft::FftPlanner::new();
    let fft: Arc<dyn Fft<f32>> = planner.plan_fft_forward(len);
    assert_eq!(fft.len(), len);

    let mut buffer = vec![Complex::zero(); len];
    let mut scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| {
        fft.process_with_scratch(&mut buffer, &mut scratch); 
    });
}


// Powers of 4
#[bench] fn planned32_p2_00000064(b: &mut Bencher) { bench_planned_f32(b,       64); }
#[bench] fn planned32_p2_00000128(b: &mut Bencher) { bench_planned_f32(b,      128); }
#[bench] fn planned32_p2_00000256(b: &mut Bencher) { bench_planned_f32(b,      256); }
#[bench] fn planned32_p2_00000512(b: &mut Bencher) { bench_planned_f32(b,      512); }
#[bench] fn planned32_p2_00001024(b: &mut Bencher) { bench_planned_f32(b,     1024); }
#[bench] fn planned32_p2_00002048(b: &mut Bencher) { bench_planned_f32(b,     2048); }
#[bench] fn planned32_p2_00004096(b: &mut Bencher) { bench_planned_f32(b,     4096); }
#[bench] fn planned32_p2_00016384(b: &mut Bencher) { bench_planned_f32(b,    16384); }
#[bench] fn planned32_p2_00065536(b: &mut Bencher) { bench_planned_f32(b,    65536); }
#[bench] fn planned32_p2_01048576(b: &mut Bencher) { bench_planned_f32(b,  1048576); }
#[bench] fn planned32_p2_16777216(b: &mut Bencher) { bench_planned_f32(b, 16777216); }


// Powers of 5
#[bench] fn planned32_p5_00125(b: &mut Bencher) { bench_planned_f32(b, 125); }
#[bench] fn planned32_p5_00625(b: &mut Bencher) { bench_planned_f32(b, 625); }
#[bench] fn planned32_p5_03125(b: &mut Bencher) { bench_planned_f32(b, 3125); }
#[bench] fn planned32_p5_15625(b: &mut Bencher) { bench_planned_f32(b, 15625); }

// Powers of 7
#[bench] fn planned32_p7_00343(b: &mut Bencher) { bench_planned_f32(b,   343); }
#[bench] fn planned32_p7_02401(b: &mut Bencher) { bench_planned_f32(b,  2401); }
#[bench] fn planned32_p7_16807(b: &mut Bencher) { bench_planned_f32(b, 16807); }

// Prime lengths
// Prime lengths
#[bench] fn planned32_prime_0005(b: &mut Bencher)     { bench_planned_f32(b,  5); }
#[bench] fn planned32_prime_0017(b: &mut Bencher)     { bench_planned_f32(b,  17); }
#[bench] fn planned32_prime_0149(b: &mut Bencher)     { bench_planned_f32(b,  149); }
#[bench] fn planned32_prime_0151(b: &mut Bencher)     { bench_planned_f32(b,  151); }
#[bench] fn planned32_prime_0251(b: &mut Bencher)     { bench_planned_f32(b,  251); }
#[bench] fn planned32_prime_0257(b: &mut Bencher)     { bench_planned_f32(b,  257); }
#[bench] fn planned32_prime_1009(b: &mut Bencher)     { bench_planned_f32(b,  1009); }
#[bench] fn planned32_prime_1201(b: &mut Bencher)     { bench_planned_f32(b,  1201); }
#[bench] fn planned32_prime_2017(b: &mut Bencher)     { bench_planned_f32(b,  2017); }
#[bench] fn planned32_prime_2879(b: &mut Bencher)     { bench_planned_f32(b,  2879); }
#[bench] fn planned32_prime_32767(b: &mut Bencher)    { bench_planned_f32(b, 32767); }
#[bench] fn planned32_prime_65521(b: &mut Bencher)    { bench_planned_f32(b, 65521); }
#[bench] fn planned32_prime_65537(b: &mut Bencher)    { bench_planned_f32(b, 65537); }
#[bench] fn planned32_prime_746483(b: &mut Bencher)   { bench_planned_f32(b,746483); }
#[bench] fn planned32_prime_746497(b: &mut Bencher)   { bench_planned_f32(b,746497); }

//primes raised to a power
#[bench] fn planned32_primepower_044521(b: &mut Bencher) { bench_planned_f32(b, 44521); } // 211^2
#[bench] fn planned32_primepower_160801(b: &mut Bencher) { bench_planned_f32(b, 160801); } // 401^2

// numbers times powers of two
#[bench] fn planned32_composite_024576(b: &mut Bencher) { bench_planned_f32(b,  24576); }
#[bench] fn planned32_composite_020736(b: &mut Bencher) { bench_planned_f32(b,  20736); }

// power of 2 times large prime
#[bench] fn planned32_composite_032192(b: &mut Bencher) { bench_planned_f32(b,  32192); }
#[bench] fn planned32_composite_024028(b: &mut Bencher) { bench_planned_f32(b,  24028); }

// small mixed composites times a large prime
#[bench] fn planned32_composite_005472(b: &mut Bencher) { bench_planned_f32(b,  5472); }
#[bench] fn planned32_composite_030270(b: &mut Bencher) { bench_planned_f32(b,  30270); }

// small mixed composites
#[bench] fn planned32_composite_000018(b: &mut Bencher) { bench_planned_f32(b,  00018); }
#[bench] fn planned32_composite_000360(b: &mut Bencher) { bench_planned_f32(b,  00360); }
#[bench] fn planned32_composite_001200(b: &mut Bencher) { bench_planned_f32(b,  01200); }
#[bench] fn planned32_composite_044100(b: &mut Bencher) { bench_planned_f32(b,  44100); }
#[bench] fn planned32_composite_048000(b: &mut Bencher) { bench_planned_f32(b,  48000); }
#[bench] fn planned32_composite_046656(b: &mut Bencher) { bench_planned_f32(b,  46656); }
#[bench] fn planned32_composite_100000(b: &mut Bencher) { bench_planned_f32(b,  100000); }

/// Times just the FFT execution (not allocation and pre-calculation)
/// for a given length
fn bench_planned_f64(b: &mut Bencher, len: usize) {

    let mut planner = rustfft::FftPlanner::new();
    let fft: Arc<dyn Fft<f64>> = planner.plan_fft_forward(len);

    let mut buffer = vec![Complex::zero(); len];
    let mut scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| { fft.process_with_scratch(&mut buffer, &mut scratch); });
}

#[bench] fn planned64_p2_00000064(b: &mut Bencher) { bench_planned_f64(b,       64); }
#[bench] fn planned64_p2_00000128(b: &mut Bencher) { bench_planned_f64(b,      128); }
#[bench] fn planned64_p2_00000256(b: &mut Bencher) { bench_planned_f64(b,      256); }
#[bench] fn planned64_p2_00000512(b: &mut Bencher) { bench_planned_f64(b,      512); }
#[bench] fn planned64_p2_00001024(b: &mut Bencher) { bench_planned_f64(b,     1024); }
#[bench] fn planned64_p2_00002048(b: &mut Bencher) { bench_planned_f64(b,     2048); }
#[bench] fn planned64_p2_00004096(b: &mut Bencher) { bench_planned_f64(b,     4096); }
#[bench] fn planned64_p2_00016384(b: &mut Bencher) { bench_planned_f64(b,    16384); }
#[bench] fn planned64_p2_00065536(b: &mut Bencher) { bench_planned_f64(b,    65536); }
#[bench] fn planned64_p2_01048576(b: &mut Bencher) { bench_planned_f64(b,  1048576); }
//#[bench] fn planned64_p2_16777216(b: &mut Bencher) { bench_planned_f64(b, 16777216); }

// Powers of 5
#[bench] fn planned64_p5_00125(b: &mut Bencher) { bench_planned_f64(b, 125); }
#[bench] fn planned64_p5_00625(b: &mut Bencher) { bench_planned_f64(b, 625); }
#[bench] fn planned64_p5_03125(b: &mut Bencher) { bench_planned_f64(b, 3125); }
#[bench] fn planned64_p5_15625(b: &mut Bencher) { bench_planned_f64(b, 15625); }

#[bench] fn planned64_p7_00343(b: &mut Bencher) { bench_planned_f64(b,   343); }
#[bench] fn planned64_p7_02401(b: &mut Bencher) { bench_planned_f64(b,  2401); }
#[bench] fn planned64_p7_16807(b: &mut Bencher) { bench_planned_f64(b, 16807); }

// Prime lengths
#[bench] fn planned64_prime_0005(b: &mut Bencher)     { bench_planned_f64(b,  5); }
#[bench] fn planned64_prime_0017(b: &mut Bencher)     { bench_planned_f64(b,  17); }
#[bench] fn planned64_prime_0149(b: &mut Bencher)     { bench_planned_f64(b,  149); }
#[bench] fn planned64_prime_0151(b: &mut Bencher)     { bench_planned_f64(b,  151); }
#[bench] fn planned64_prime_0251(b: &mut Bencher)     { bench_planned_f64(b,  251); }
#[bench] fn planned64_prime_0257(b: &mut Bencher)     { bench_planned_f64(b,  257); }
#[bench] fn planned64_prime_1009(b: &mut Bencher)     { bench_planned_f64(b,  1009); }
#[bench] fn planned64_prime_2017(b: &mut Bencher)     { bench_planned_f64(b,  2017); }
#[bench] fn planned64_prime_2879(b: &mut Bencher)     { bench_planned_f64(b,  2879); }
#[bench] fn planned64_prime_32767(b: &mut Bencher)    { bench_planned_f64(b, 32767); }
#[bench] fn planned64_prime_65521(b: &mut Bencher)    { bench_planned_f64(b, 65521); }
#[bench] fn planned64_prime_65537(b: &mut Bencher)    { bench_planned_f64(b, 65537); }
#[bench] fn planned64_prime_746483(b: &mut Bencher)   { bench_planned_f64(b,746483); }
#[bench] fn planned64_prime_746497(b: &mut Bencher)   { bench_planned_f64(b,746497); }

//primes raised to a power
#[bench] fn planned64_primepower_044521(b: &mut Bencher) { bench_planned_f64(b, 44521); } // 211^2
#[bench] fn planned64_primepower_160801(b: &mut Bencher) { bench_planned_f64(b, 160801); } // 401^2

// numbers times powers of two
#[bench] fn planned64_composite_024576(b: &mut Bencher) { bench_planned_f64(b,  24576); }
#[bench] fn planned64_composite_020736(b: &mut Bencher) { bench_planned_f64(b,  20736); }

// power of 2 times large prime
#[bench] fn planned64_composite_032192(b: &mut Bencher) { bench_planned_f64(b,  32192); }
#[bench] fn planned64_composite_024028(b: &mut Bencher) { bench_planned_f64(b,  24028); }

// small mixed composites times a large prime
#[bench] fn planned64_composite_030270(b: &mut Bencher) { bench_planned_f64(b,  30270); }

// small mixed composites
#[bench] fn planned64_composite_000018(b: &mut Bencher) { bench_planned_f64(b,  00018); }
#[bench] fn planned64_composite_000360(b: &mut Bencher) { bench_planned_f64(b,  00360); }
#[bench] fn planned64_composite_044100(b: &mut Bencher) { bench_planned_f64(b,  44100); }
#[bench] fn planned64_composite_048000(b: &mut Bencher) { bench_planned_f64(b,  48000); }
#[bench] fn planned64_composite_046656(b: &mut Bencher) { bench_planned_f64(b,  46656); }
#[bench] fn planned64_composite_100000(b: &mut Bencher) { bench_planned_f64(b,  100000); }

/// Times just the FFT execution (not allocation and pre-calculation)
/// for a given length, specific to the Good-Thomas algorithm
fn bench_good_thomas(b: &mut Bencher, width: usize, height: usize) {

    let mut planner = rustfft::FftPlanner::new();
    let width_fft = planner.plan_fft_forward(width);
    let height_fft = planner.plan_fft_forward(height);

    let fft : Arc<dyn Fft<f32>> = Arc::new(GoodThomasAlgorithm::new(width_fft, height_fft));

    let mut buffer = vec![Complex::zero(); width * height];
    let mut scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| {fft.process_with_scratch(&mut buffer, &mut scratch);} );
}

#[bench] fn good_thomas_0002_3(b: &mut Bencher) { bench_good_thomas(b,  2, 3); }
#[bench] fn good_thomas_0003_4(b: &mut Bencher) { bench_good_thomas(b,  3, 4); }
#[bench] fn good_thomas_0004_5(b: &mut Bencher) { bench_good_thomas(b,  4, 5); }
#[bench] fn good_thomas_0007_32(b: &mut Bencher) { bench_good_thomas(b, 7, 32); }
#[bench] fn good_thomas_0032_27(b: &mut Bencher) { bench_good_thomas(b,  32, 27); }
#[bench] fn good_thomas_0256_243(b: &mut Bencher) { bench_good_thomas(b,  256, 243); }
#[bench] fn good_thomas_2048_3(b: &mut Bencher) { bench_good_thomas(b,  2048, 3); }
#[bench] fn good_thomas_2048_2187(b: &mut Bencher) { bench_good_thomas(b,  2048, 2187); }

/// Times just the FFT setup (not execution)
/// for a given length, specific to the Good-Thomas algorithm
fn bench_good_thomas_setup(b: &mut Bencher, width: usize, height: usize) {

    let mut planner = rustfft::FftPlanner::new();
    let width_fft = planner.plan_fft_forward(width);
    let height_fft = planner.plan_fft_forward(height);

    b.iter(|| { 
        let fft : Arc<dyn Fft<f32>> = Arc::new(GoodThomasAlgorithm::new(Arc::clone(&width_fft), Arc::clone(&height_fft)));
        test::black_box(fft);
    });
}

#[bench] fn good_thomas_setup_0002_3(b: &mut Bencher) { bench_good_thomas_setup(b,  2, 3); }
#[bench] fn good_thomas_setup_0003_4(b: &mut Bencher) { bench_good_thomas_setup(b,  3, 4); }
#[bench] fn good_thomas_setup_0004_5(b: &mut Bencher) { bench_good_thomas_setup(b,  4, 5); }
#[bench] fn good_thomas_setup_0007_32(b: &mut Bencher) { bench_good_thomas_setup(b, 7, 32); }
#[bench] fn good_thomas_setup_0032_27(b: &mut Bencher) { bench_good_thomas_setup(b,  32, 27); }
#[bench] fn good_thomas_setup_0256_243(b: &mut Bencher) { bench_good_thomas_setup(b,  256, 243); }
#[bench] fn good_thomas_setup_2048_3(b: &mut Bencher) { bench_good_thomas_setup(b,  2048, 3); }
#[bench] fn good_thomas_setup_2048_2187(b: &mut Bencher) { bench_good_thomas_setup(b,  2048, 2187); }

/// Times just the FFT setup (not execution)
/// for a given length, specific to MixedRadix
fn bench_mixed_radix_setup(b: &mut Bencher, width: usize, height: usize) {

    let mut planner = rustfft::FftPlanner::new();
    let width_fft = planner.plan_fft_forward(width);
    let height_fft = planner.plan_fft_forward(height);

    b.iter(|| { 
        let fft : Arc<dyn Fft<f32>> = Arc::new(MixedRadix::new(Arc::clone(&width_fft), Arc::clone(&height_fft)));
        test::black_box(fft);
    });
}

#[bench] fn setup_mixed_radix_0002_3(b: &mut Bencher) { bench_mixed_radix_setup(b,  2, 3); }
#[bench] fn setup_mixed_radix_0003_4(b: &mut Bencher) { bench_mixed_radix_setup(b,  3, 4); }
#[bench] fn setup_mixed_radix_0004_5(b: &mut Bencher) { bench_mixed_radix_setup(b,  4, 5); }
#[bench] fn setup_mixed_radix_0007_32(b: &mut Bencher) { bench_mixed_radix_setup(b, 7, 32); }
#[bench] fn setup_mixed_radix_0032_27(b: &mut Bencher) { bench_mixed_radix_setup(b,  32, 27); }
#[bench] fn setup_mixed_radix_0256_243(b: &mut Bencher) { bench_mixed_radix_setup(b,  256, 243); }
#[bench] fn setup_mixed_radix_2048_3(b: &mut Bencher) { bench_mixed_radix_setup(b,  2048, 3); }
#[bench] fn setup_mixed_radix_2048_2187(b: &mut Bencher) { bench_mixed_radix_setup(b,  2048, 2187); }

/// Times just the FFT setup (not execution)
/// for a given length, specific to MixedRadix
fn bench_small_mixed_radix_setup(b: &mut Bencher, width: usize, height: usize) {

    let mut planner = rustfft::FftPlanner::new();
    let width_fft = planner.plan_fft_forward(width);
    let height_fft = planner.plan_fft_forward(height);

    b.iter(|| { 
        let fft : Arc<dyn Fft<f32>> = Arc::new(MixedRadixSmall::new(Arc::clone(&width_fft), Arc::clone(&height_fft)));
        test::black_box(fft);
    });
}

#[bench] fn setup_small_mixed_radix_0002_3(b: &mut Bencher) { bench_small_mixed_radix_setup(b,  2, 3); }
#[bench] fn setup_small_mixed_radix_0003_4(b: &mut Bencher) { bench_small_mixed_radix_setup(b,  3, 4); }
#[bench] fn setup_small_mixed_radix_0004_5(b: &mut Bencher) { bench_small_mixed_radix_setup(b,  4, 5); }
#[bench] fn setup_small_mixed_radix_0007_32(b: &mut Bencher) { bench_small_mixed_radix_setup(b, 7, 32); }
#[bench] fn setup_small_mixed_radix_0032_27(b: &mut Bencher) { bench_small_mixed_radix_setup(b,  32, 27); }


/// Times just the FFT execution (not allocation and pre-calculation)
/// for a given length, specific to the Mixed-Radix algorithm
fn bench_mixed_radix(b: &mut Bencher, width: usize, height: usize) {

    let mut planner = rustfft::FftPlanner::new();
    let width_fft = planner.plan_fft_forward(width);
    let height_fft = planner.plan_fft_forward(height);

    let fft : Arc<dyn Fft<_>> = Arc::new(MixedRadix::new(width_fft, height_fft));

    let mut buffer = vec![Complex{re: 0_f32, im: 0_f32}; fft.len()];
    let mut scratch = vec![Complex{re: 0_f32, im: 0_f32}; fft.get_inplace_scratch_len()];
    b.iter(|| {fft.process_with_scratch(&mut buffer, &mut scratch);} );
}

#[bench] fn mixed_radix_0002_3(b: &mut Bencher) { bench_mixed_radix(b,  2, 3); }
#[bench] fn mixed_radix_0003_4(b: &mut Bencher) { bench_mixed_radix(b,  3, 4); }
#[bench] fn mixed_radix_0004_5(b: &mut Bencher) { bench_mixed_radix(b,  4, 5); }
#[bench] fn mixed_radix_0007_32(b: &mut Bencher) { bench_mixed_radix(b, 7, 32); }
#[bench] fn mixed_radix_0032_27(b: &mut Bencher) { bench_mixed_radix(b,  32, 27); }
#[bench] fn mixed_radix_0256_243(b: &mut Bencher) { bench_mixed_radix(b,  256, 243); }
#[bench] fn mixed_radix_2048_3(b: &mut Bencher) { bench_mixed_radix(b,  2048, 3); }
#[bench] fn mixed_radix_2048_2187(b: &mut Bencher) { bench_mixed_radix(b,  2048, 2187); }

fn plan_butterfly_fft(len: usize) -> Arc<dyn Fft<f32>> {
    match len {
        2 => Arc::new(Butterfly2::new(FftDirection::Forward)),
        3 => Arc::new(Butterfly3::new(FftDirection::Forward)),
        4 => Arc::new(Butterfly4::new(FftDirection::Forward)),
        5 => Arc::new(Butterfly5::new(FftDirection::Forward)),
        6 => Arc::new(Butterfly6::new(FftDirection::Forward)),
        7 => Arc::new(Butterfly7::new(FftDirection::Forward)),
        8 => Arc::new(Butterfly8::new(FftDirection::Forward)),
        16 => Arc::new(Butterfly16::new(FftDirection::Forward)),
        32 => Arc::new(Butterfly32::new(FftDirection::Forward)),
        _ => panic!("Invalid butterfly size: {}", len),
    }
}

/// Times just the FFT execution (not allocation and pre-calculation)
/// for a given length, specific to the MixedRadixSmall algorithm
fn bench_mixed_radix_small(b: &mut Bencher, width: usize, height: usize) {

    let width_fft = plan_butterfly_fft(width);
    let height_fft = plan_butterfly_fft(height);

    let fft : Arc<dyn Fft<_>> = Arc::new(MixedRadixSmall::new(width_fft, height_fft));

    let mut signal = vec![Complex{re: 0_f32, im: 0_f32}; width * height];
    let mut spectrum = signal.clone();
    b.iter(|| {fft.process_with_scratch(&mut signal, &mut spectrum);} );
}

#[bench] fn mixed_radix_small_0002_3(b: &mut Bencher) { bench_mixed_radix_small(b,  2, 3); }
#[bench] fn mixed_radix_small_0003_4(b: &mut Bencher) { bench_mixed_radix_small(b,  3, 4); }
#[bench] fn mixed_radix_small_0004_5(b: &mut Bencher) { bench_mixed_radix_small(b,  4, 5); }
#[bench] fn mixed_radix_small_0007_32(b: &mut Bencher) { bench_mixed_radix_small(b, 7, 32); }

/// Times just the FFT execution (not allocation and pre-calculation)
/// for a given length, specific to the Mixed-Radix Double Butterfly algorithm
fn bench_good_thomas_small(b: &mut Bencher, width: usize, height: usize) {

    let width_fft = plan_butterfly_fft(width);
    let height_fft = plan_butterfly_fft(height);

    let fft : Arc<dyn Fft<_>> = Arc::new(GoodThomasAlgorithmSmall::new(width_fft, height_fft));

    let mut signal = vec![Complex{re: 0_f32, im: 0_f32}; width * height];
    let mut spectrum = signal.clone();
    b.iter(|| {fft.process_with_scratch(&mut signal, &mut spectrum);} );
}

#[bench] fn good_thomas_small_0002_3(b: &mut Bencher) { bench_good_thomas_small(b,  2, 3); }
#[bench] fn good_thomas_small_0003_4(b: &mut Bencher) { bench_good_thomas_small(b,  3, 4); }
#[bench] fn good_thomas_small_0004_5(b: &mut Bencher) { bench_good_thomas_small(b,  4, 5); }
#[bench] fn good_thomas_small_0007_32(b: &mut Bencher) { bench_good_thomas_small(b, 7, 32); }


/// Times just the FFT execution (not allocation and pre-calculation)
/// for a given length, specific to Rader's algorithm
fn bench_raders_scalar(b: &mut Bencher, len: usize) {

    let mut planner = rustfft::FftPlanner::new();
    let inner_fft = planner.plan_fft_forward(len - 1);

    let fft : Arc<dyn Fft<_>> = Arc::new(RadersAlgorithm::new(inner_fft));

    let mut buffer = vec![Complex{re: 0_f32, im: 0_f32}; len];
    let mut scratch = vec![Complex{re: 0_f32, im: 0_f32}; fft.get_inplace_scratch_len()];
    b.iter(|| {fft.process_with_scratch(&mut buffer, &mut scratch);} );
}

#[bench] fn raders_fft_scalar_prime_0005(b: &mut Bencher) { bench_raders_scalar(b,  5); }
#[bench] fn raders_fft_scalar_prime_0017(b: &mut Bencher) { bench_raders_scalar(b,  17); }
#[bench] fn raders_fft_scalar_prime_0149(b: &mut Bencher) { bench_raders_scalar(b,  149); }
#[bench] fn raders_fft_scalar_prime_0151(b: &mut Bencher) { bench_raders_scalar(b,  151); }
#[bench] fn raders_fft_scalar_prime_0251(b: &mut Bencher) { bench_raders_scalar(b,  251); }
#[bench] fn raders_fft_scalar_prime_0257(b: &mut Bencher) { bench_raders_scalar(b,  257); }
#[bench] fn raders_fft_scalar_prime_1009(b: &mut Bencher) { bench_raders_scalar(b,  1009); }
#[bench] fn raders_fft_scalar_prime_2017(b: &mut Bencher) { bench_raders_scalar(b,  2017); }
#[bench] fn raders_fft_scalar_prime_12289(b: &mut Bencher) { bench_raders_scalar(b, 12289); }
#[bench] fn raders_fft_scalar_prime_18433(b: &mut Bencher) { bench_raders_scalar(b, 18433); }
#[bench] fn raders_fft_scalar_prime_65521(b: &mut Bencher) { bench_raders_scalar(b, 65521); }
#[bench] fn raders_fft_scalar_prime_65537(b: &mut Bencher) { bench_raders_scalar(b, 65537); }
#[bench] fn raders_fft_scalar_prime_746483(b: &mut Bencher) { bench_raders_scalar(b,746483); }
#[bench] fn raders_fft_scalar_prime_746497(b: &mut Bencher) { bench_raders_scalar(b,746497); }


/// Times just the FFT execution (not allocation and pre-calculation)
/// for a given length, specific to Bluestein's Algorithm
fn bench_bluesteins_scalar_prime(b: &mut Bencher, len: usize) {
    let mut planner = rustfft::FftPlanner::new();
    let inner_fft = planner.plan_fft_forward((len * 2 - 1).checked_next_power_of_two().unwrap());
    let fft : Arc<dyn Fft<f32>> = Arc::new(BluesteinsAlgorithm::new(len, inner_fft));

    let mut buffer = vec![Zero::zero(); len];
    let mut scratch = vec![Zero::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| { fft.process_with_scratch(&mut buffer, &mut scratch);} );
}

#[bench] fn bench_bluesteins_scalar_prime_0005(b: &mut Bencher) { bench_bluesteins_scalar_prime(b,  5); }
#[bench] fn bench_bluesteins_scalar_prime_0017(b: &mut Bencher) { bench_bluesteins_scalar_prime(b,  17); }
#[bench] fn bench_bluesteins_scalar_prime_0149(b: &mut Bencher) { bench_bluesteins_scalar_prime(b,  149); }
#[bench] fn bench_bluesteins_scalar_prime_0151(b: &mut Bencher) { bench_bluesteins_scalar_prime(b,  151); }
#[bench] fn bench_bluesteins_scalar_prime_0251(b: &mut Bencher) { bench_bluesteins_scalar_prime(b,  251); }
#[bench] fn bench_bluesteins_scalar_prime_0257(b: &mut Bencher) { bench_bluesteins_scalar_prime(b,  257); }
#[bench] fn bench_bluesteins_scalar_prime_1009(b: &mut Bencher) { bench_bluesteins_scalar_prime(b,  1009); }
#[bench] fn bench_bluesteins_scalar_prime_2017(b: &mut Bencher) { bench_bluesteins_scalar_prime(b,  2017); }
#[bench] fn bench_bluesteins_scalar_prime_32767(b: &mut Bencher) { bench_bluesteins_scalar_prime(b, 32767); }
#[bench] fn bench_bluesteins_scalar_prime_65521(b: &mut Bencher) { bench_bluesteins_scalar_prime(b, 65521); }
#[bench] fn bench_bluesteins_scalar_prime_65537(b: &mut Bencher) { bench_bluesteins_scalar_prime(b, 65537); }
#[bench] fn bench_bluesteins_scalar_prime_746483(b: &mut Bencher) { bench_bluesteins_scalar_prime(b,746483); }
#[bench] fn bench_bluesteins_scalar_prime_746497(b: &mut Bencher) { bench_bluesteins_scalar_prime(b,746497); }


/// Times just the FFT execution (not allocation and pre-calculation)
/// for a given length, specific to Rader's algorithm
fn bench_radix4(b: &mut Bencher, len: usize) {
    assert!(len % 4 == 0);

    let fft = Radix4::new(len, FftDirection::Forward);

    let mut signal = vec![Complex{re: 0_f32, im: 0_f32}; len];
    let mut spectrum = signal.clone();
    b.iter(|| {fft.process_outofplace_with_scratch(&mut signal, &mut spectrum, &mut []);} );
}

#[bench] fn radix4_______64(b: &mut Bencher) { bench_radix4(b, 64); }
#[bench] fn radix4______256(b: &mut Bencher) { bench_radix4(b, 256); }
#[bench] fn radix4_____1024(b: &mut Bencher) { bench_radix4(b, 1024); }
#[bench] fn radix4____65536(b: &mut Bencher) { bench_radix4(b, 65536); }
#[bench] fn radix4__1048576(b: &mut Bencher) { bench_radix4(b, 1048576); }
//#[bench] fn radix4_16777216(b: &mut Bencher) { bench_radix4(b, 16777216); }

fn get_mixed_radix_power2(len: usize) -> Arc<dyn Fft<f32>> {
    match len {
        8 => Arc::new(Butterfly8::new( FftDirection::Forward)),
        16 => Arc::new(Butterfly16::new(FftDirection::Forward)),
        32 => Arc::new(Butterfly32::new(FftDirection::Forward)),
        _ => {
            let zeroes = len.trailing_zeros();
            assert!(zeroes % 2 == 0);
            let half_zeroes = zeroes / 2;
            let inner = get_mixed_radix_power2(1 << half_zeroes);
            Arc::new(MixedRadix::new(Arc::clone(&inner), inner))
        }
    }
}

/// Times just the FFT execution (not allocation and pre-calculation)
/// for a given length, specific to Rader's algorithm
fn bench_mixed_radix_power2(b: &mut Bencher, len: usize) {
    let fft = get_mixed_radix_power2(len);

    let mut buffer = vec![Zero::zero(); len];
    let mut scratch = vec![Zero::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| {
        fft.process_with_scratch(&mut buffer, &mut scratch);
    });
}

#[bench] fn mixed_radix_power2__00000256(b: &mut Bencher) { bench_mixed_radix_power2(b, 256); }
#[bench] fn mixed_radix_power2__00001024(b: &mut Bencher) { bench_mixed_radix_power2(b, 1024); }
#[bench] fn mixed_radix_power2__00004096(b: &mut Bencher) { bench_mixed_radix_power2(b, 4096); }
#[bench] fn mixed_radix_power2__00065536(b: &mut Bencher) { bench_mixed_radix_power2(b, 65536); }
#[bench] fn mixed_radix_power2__01048576(b: &mut Bencher) { bench_mixed_radix_power2(b, 1048576); }
#[bench] fn mixed_radix_power2__16777216(b: &mut Bencher) { bench_mixed_radix_power2(b, 16777216); }


fn get_mixed_radix_inline_power2(len: usize) -> Arc<dyn Fft<f32>> {
    match len {
        8 => Arc::new(Butterfly8::new( FftDirection::Forward)),
        16 => Arc::new(Butterfly16::new(FftDirection::Forward)),
        32 => Arc::new(Butterfly32::new(FftDirection::Forward)),
        _ => {
            let zeroes = len.trailing_zeros();
            assert!(zeroes % 2 == 0);
            let half_zeroes = zeroes / 2;
            let inner = get_mixed_radix_inline_power2(1 << half_zeroes);
            Arc::new(MixedRadix::new(Arc::clone(&inner), inner))
        }
    }
}

/// Times just the FFT execution (not allocation and pre-calculation)
/// for a given length, specific to Rader's algorithm
fn bench_mixed_radix_inline_power2(b: &mut Bencher, len: usize) {
    let fft = get_mixed_radix_inline_power2(len);

    let mut buffer = vec![Zero::zero(); len];
    let mut scratch = vec![Zero::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| {
        fft.process_with_scratch(&mut buffer, &mut scratch);
    });
}

#[bench] fn mixed_radix_power2_inline__00000256(b: &mut Bencher) { bench_mixed_radix_inline_power2(b, 256); }
#[bench] fn mixed_radix_power2_inline__00001024(b: &mut Bencher) { bench_mixed_radix_inline_power2(b, 1024); }
#[bench] fn mixed_radix_power2_inline__00004096(b: &mut Bencher) { bench_mixed_radix_inline_power2(b, 4096); }
#[bench] fn mixed_radix_power2_inline__00065536(b: &mut Bencher) { bench_mixed_radix_inline_power2(b, 65536); }
#[bench] fn mixed_radix_power2_inline__01048576(b: &mut Bencher) { bench_mixed_radix_inline_power2(b, 1048576); }
#[bench] fn mixed_radix_power2_inline__16777216(b: &mut Bencher) { bench_mixed_radix_inline_power2(b, 16777216); }

/// Times just the FFT execution (not allocation and pre-calculation)
/// for a given length
fn bench_butterfly32(b: &mut Bencher, len: usize) {

    let mut planner = rustfft::FftPlanner::new();
    let fft: Arc<dyn Fft<f32>> = planner.plan_fft_forward(len);

    let mut buffer = vec![Complex::zero(); len * 10];
    let mut scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| { fft.process_with_scratch(&mut buffer, &mut scratch); });
}

#[bench] fn butterfly32_02(b: &mut Bencher) { bench_butterfly32(b, 2); }
#[bench] fn butterfly32_03(b: &mut Bencher) { bench_butterfly32(b, 3); }
#[bench] fn butterfly32_04(b: &mut Bencher) { bench_butterfly32(b, 4); }
#[bench] fn butterfly32_05(b: &mut Bencher) { bench_butterfly32(b, 5); }
#[bench] fn butterfly32_06(b: &mut Bencher) { bench_butterfly32(b, 6); }
#[bench] fn butterfly32_07(b: &mut Bencher) { bench_butterfly32(b, 7); }
#[bench] fn butterfly32_08(b: &mut Bencher) { bench_butterfly32(b, 8); }
#[bench] fn butterfly32_09(b: &mut Bencher) { bench_butterfly32(b, 9); }
#[bench] fn butterfly32_11(b: &mut Bencher) { bench_butterfly32(b, 11); }
#[bench] fn butterfly32_12(b: &mut Bencher) { bench_butterfly32(b, 12); }
#[bench] fn butterfly32_16(b: &mut Bencher) { bench_butterfly32(b, 16); }
#[bench] fn butterfly32_24(b: &mut Bencher) { bench_butterfly32(b, 24); }
#[bench] fn butterfly32_27(b: &mut Bencher) { bench_butterfly32(b, 27); }
#[bench] fn butterfly32_32(b: &mut Bencher) { bench_butterfly32(b, 32); }
#[bench] fn butterfly32_36(b: &mut Bencher) { bench_butterfly32(b, 36); }
#[bench] fn butterfly32_48(b: &mut Bencher) { bench_butterfly32(b, 48); }
#[bench] fn butterfly32_54(b: &mut Bencher) { bench_butterfly32(b, 54); }
#[bench] fn butterfly32_64(b: &mut Bencher) { bench_butterfly32(b, 64); }
#[bench] fn butterfly32_72(b: &mut Bencher) { bench_butterfly32(b, 72); }
#[bench] fn butterfly32_128(b: &mut Bencher) { bench_butterfly32(b, 128); }
#[bench] fn butterfly32_256(b: &mut Bencher) { bench_butterfly32(b, 256); }
#[bench] fn butterfly32_512(b: &mut Bencher) { bench_butterfly32(b, 512); }

/// Times just the FFT execution (not allocation and pre-calculation)
/// for a given length
fn bench_butterfly64(b: &mut Bencher, len: usize) {

    let mut planner = rustfft::FftPlanner::new();
    let fft: Arc<dyn Fft<f64>> = planner.plan_fft_forward(len);

    let mut buffer = vec![Complex::zero(); len * 10];
    let mut scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| { fft.process_with_scratch(&mut buffer, &mut scratch); });
}

#[bench] fn butterfly64_02(b: &mut Bencher) { bench_butterfly64(b, 2); }
#[bench] fn butterfly64_03(b: &mut Bencher) { bench_butterfly64(b, 3); }
#[bench] fn butterfly64_04(b: &mut Bencher) { bench_butterfly64(b, 4); }
#[bench] fn butterfly64_05(b: &mut Bencher) { bench_butterfly64(b, 5); }
#[bench] fn butterfly64_06(b: &mut Bencher) { bench_butterfly64(b, 6); }
#[bench] fn butterfly64_07(b: &mut Bencher) { bench_butterfly64(b, 7); }
#[bench] fn butterfly64_08(b: &mut Bencher) { bench_butterfly64(b, 8); }
#[bench] fn butterfly64_09(b: &mut Bencher) { bench_butterfly64(b, 9); }
#[bench] fn butterfly64_11(b: &mut Bencher) { bench_butterfly64(b, 11); }
#[bench] fn butterfly64_12(b: &mut Bencher) { bench_butterfly64(b, 12); }
#[bench] fn butterfly64_16(b: &mut Bencher) { bench_butterfly64(b, 16); }
#[bench] fn butterfly64_18(b: &mut Bencher) { bench_butterfly64(b, 18); }
#[bench] fn butterfly64_24(b: &mut Bencher) { bench_butterfly64(b, 24); }
#[bench] fn butterfly64_27(b: &mut Bencher) { bench_butterfly64(b, 27); }
#[bench] fn butterfly64_32(b: &mut Bencher) { bench_butterfly64(b, 32); }
#[bench] fn butterfly64_36(b: &mut Bencher) { bench_butterfly64(b, 36); }
#[bench] fn butterfly64_64(b: &mut Bencher) { bench_butterfly64(b, 64); }
#[bench] fn butterfly64_128(b: &mut Bencher) { bench_butterfly64(b, 128); }
#[bench] fn butterfly64_256(b: &mut Bencher) { bench_butterfly64(b, 256); }
#[bench] fn butterfly64_512(b: &mut Bencher) { bench_butterfly64(b, 512); }
