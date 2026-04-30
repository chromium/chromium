#![feature(test)]
#![allow(non_snake_case)]
#![allow(unused)]

extern crate rustfft;
extern crate test;
use test::Bencher;

use rustfft::algorithm::butterflies::*;
use rustfft::algorithm::Dft;
use rustfft::num_complex::Complex;
use rustfft::num_traits::Zero;
use rustfft::{Fft, FftNum};
use rustfft::{FftPlanner, FftPlannerAvx};

use primal_check::miller_rabin;

use std::sync::Arc;

/// This benchmark's purpose is to build some programmer intuition for planner heuristics
/// We have mixed radix 2xn, 3xn, 4xn, 6xn, 8xn, 9x, 12xn, and 16xn implementations -- for a given FFT of the form 2^xn * 3^m, which combination is the fastest? Is 12xn -> 4xn faster than 6xn -> 8xn?
/// Is it faster to put 9xn as an outer FFT of 8xn or as an inner FFT? this file autogenerates benchmarks that answer these questions
///
/// The "generate_3n2m_comparison_benchmarks" benchmark will print benchmark code to the console which should be pasted back into this file, basically a low-budget procedural macro

#[derive(Clone, Debug)]
struct FftSize {
    len: usize,
    power2: u32,
    power3: u32,
}
impl FftSize {
    fn new(len: usize) -> Self {
        let power2 = len.trailing_zeros();
        let mut remaining_factors = len >> power2;
        let mut power3 = 0;
        while remaining_factors % 3 == 0 {
            power3 += 1;
            remaining_factors /= 3;
        }

        assert!(remaining_factors == 1);
        Self {
            power2,
            power3,
            len,
        }
    }

    fn divide(&self, other: &Self) -> Option<Self> {
        if self.power2 <= other.power2 && self.power3 <= other.power3 {
            Some(Self {
                power2: other.power2 - self.power2,
                power3: other.power3 - self.power3,
                len: other.len / self.len,
            })
        } else {
            None
        }
    }
}

// We don't need to generate a combinatoric explosion of tests that we know will be slow. filter_radix applies some dumb heuristics to filter out the most common slow cases
fn filter_radix(current_strategy: &[usize], potential_radix: &FftSize, is_butterfly: bool) -> bool {
    // if we've seen any radix larger than this before, reject. otherwise we'll get a million reorderings of the same radixex, with benchmarking showing that smaller being higher is typically faster
    if !is_butterfly
        && current_strategy
            .iter()
            .find(|i| **i > potential_radix.len && **i != 16)
            .is_some()
    {
        return false;
    }

    // apply filters to size 2
    if potential_radix.len == 2 {
        // if our strategy already contains any 2's, 3's, or 4's, reject -- because 4, 6, or 8 will be faster, respectively
        return !current_strategy.contains(&2)
            && !current_strategy.contains(&3)
            && !current_strategy.contains(&4);
    }
    // apply filters to size 3
    if potential_radix.len == 3 {
        // if our strategy already contains any 2's, 3's or 4s, reject -- because 6 and 9 and 12 will be faster, respectively
        return !current_strategy.contains(&2)
            && !current_strategy.contains(&3)
            && !current_strategy.contains(&4);
    }
    // apply filters to size 4
    if potential_radix.len == 4 {
        // if our strategy already contains any 2's, reject -- because 8 will be faster
        // if our strategy already contains 2 4's, don't add a third, because 2 8's would have been faster
        // if our strategy already contains a 16, reject -- because 2 8's will be faster (8s are seriously fast guys)
        return !current_strategy.contains(&2)
            && !current_strategy.contains(&3)
            && !current_strategy.contains(&4)
            && !current_strategy.contains(&16);
    }
    if potential_radix.len == 16 {
        // if our strategy already contains a 4, reject -- because 2 8's will be faster (8s are seriously fast guys)
        // if our strategy already contains a 16, reject -- benchmarking shows that 16s are very situational, and repeating them never helps)
        return !current_strategy.contains(&4) && !current_strategy.contains(&16);
    }
    return true;
}

fn recursive_strategy_builder(
    strategy_list: &mut Vec<Vec<usize>>,
    last_ditch_strategy_list: &mut Vec<Vec<usize>>,
    mut current_strategy: Vec<usize>,
    len: FftSize,
    butterfly_sizes: &[usize],
    last_ditch_butterflies: &[usize],
    available_radixes: &[FftSize],
) {
    if butterfly_sizes.contains(&len.len) {
        if filter_radix(&current_strategy, &len, true) {
            current_strategy.push(len.len);

            //If this strategy contains a 2 or 3, it's very unlikely to be the fastest. we don't want to rule it out, because it's required sometimes, but don't use it unless there aren't any other
            if current_strategy.contains(&2) || current_strategy.contains(&3) {
                strategy_list.push(current_strategy.clone());
            } else {
                strategy_list.push(current_strategy.clone());
            }
        }
    } else if last_ditch_butterflies.contains(&len.len) {
        if filter_radix(&current_strategy, &len, true) {
            current_strategy.push(len.len);
            last_ditch_strategy_list.push(current_strategy.clone());
        }
    } else if len.len > 1 {
        for radix in available_radixes {
            if filter_radix(&current_strategy, radix, false) {
                if let Some(inner) = radix.divide(&len) {
                    let mut cloned_strategy = current_strategy.clone();
                    cloned_strategy.push(radix.len);
                    recursive_strategy_builder(
                        strategy_list,
                        last_ditch_strategy_list,
                        cloned_strategy,
                        inner,
                        butterfly_sizes,
                        last_ditch_butterflies,
                        available_radixes,
                    );
                }
            }
        }
    }
}

// it's faster to filter strategies at the radix level since we can prune entire permutations, but some can only be done once the full plan is built
fn filter_strategy(strategy: &Vec<usize>) -> bool {
    if strategy.contains(&16) {
        let index = strategy.iter().position(|s| *s == 16).unwrap();
        index == 0
            || index == strategy.len() - 1
            || index == strategy.len() - 2
            || (strategy[index - 1] < 12 && strategy[index + 1] >= 12)
    } else {
        true
    }
}

// cargo bench generate_3n2m_comparison_benchmarks_32 -- --nocapture --ignored
#[ignore]
#[bench]
fn generate_3n2m_comparison_benchmarks_32(_: &mut test::Bencher) {
    let butterfly_sizes = [128, 256, 512, 72, 36, 48, 54, 64];
    let last_ditch_butterflies = [27, 9, 32, 24];
    let available_radixes = [
        FftSize::new(3),
        FftSize::new(4),
        FftSize::new(6),
        FftSize::new(8),
        FftSize::new(9),
        FftSize::new(12),
        FftSize::new(16),
    ];

    let max_len: usize = 1 << 21;
    let min_len = 64;
    let max_power2 = max_len.trailing_zeros();
    let max_power3 = (max_len as f32).log(3.0).ceil() as u32;

    for power3 in 1..2 {
        for power2 in 4..max_power2 {
            let len = 3usize.pow(power3) << power2;
            if len > max_len {
                continue;
            }

            //let planned_fft : Arc<dyn Fft<f32>> = rustfft::FftPlanner::new(false).plan_fft(len);

            // we want to catalog all the different possible ways there are to compute a FFT of size `len`
            // we can do that by recursively looping over each radix, dividing our length by that radix, then recursively trying rach radix again
            let mut strategies = vec![];
            let mut last_ditch_strategies = vec![];
            recursive_strategy_builder(
                &mut strategies,
                &mut last_ditch_strategies,
                Vec::new(),
                FftSize::new(len),
                &butterfly_sizes,
                &last_ditch_butterflies,
                &available_radixes,
            );

            if strategies.len() == 0 {
                strategies = last_ditch_strategies;
            }

            for mut s in strategies.into_iter().filter(filter_strategy) {
                s.reverse();
                let strategy_strings: Vec<_> = s.into_iter().map(|i| i.to_string()).collect();
                let test_id = strategy_strings.join("_");
                let strategy_array = strategy_strings.join(",");
                println!("#[bench] fn comparef32__2power{:02}__3power{:02}__len{:08}__{}(b: &mut Bencher) {{ compare_fft_f32(b, &[{}]); }}", power2, power3, len, test_id, strategy_array);
            }
        }
    }
}

// cargo bench generate_3n2m_comparison_benchmarks_64 -- --nocapture --ignored
#[ignore]
#[bench]
fn generate_3n2m_comparison_benchmarks_64(_: &mut test::Bencher) {
    let butterfly_sizes = [512, 256, 128, 64, 36, 27, 24, 18, 12];
    let last_ditch_butterflies = [32, 16, 8, 9];
    let available_radixes = [
        FftSize::new(3),
        FftSize::new(4),
        FftSize::new(6),
        FftSize::new(8),
        FftSize::new(9),
        FftSize::new(12),
    ];

    let max_len: usize = 1 << 21;
    let min_len = 64;
    let max_power2 = max_len.trailing_zeros();
    let max_power3 = (max_len as f32).log(3.0).ceil() as u32;

    for power3 in 0..1 {
        for power2 in 3..max_power2 {
            let len = 3usize.pow(power3) << power2;
            if len > max_len {
                continue;
            }

            //let planned_fft : Arc<dyn Fft<f32>> = rustfft::FftPlanner::new(false).plan_fft(len);

            // we want to catalog all the different possible ways there are to compute a FFT of size `len`
            // we can do that by recursively looping over each radix, dividing our length by that radix, then recursively trying rach radix again
            // we can do that by recursively looping over each radix, dividing our length by that radix, then recursively trying rach radix again
            let mut strategies = vec![];
            let mut last_ditch_strategies = vec![];
            recursive_strategy_builder(
                &mut strategies,
                &mut last_ditch_strategies,
                Vec::new(),
                FftSize::new(len),
                &butterfly_sizes,
                &last_ditch_butterflies,
                &available_radixes,
            );

            if strategies.len() == 0 {
                strategies = last_ditch_strategies;
            }

            for mut s in strategies.into_iter().filter(filter_strategy) {
                s.reverse();
                let strategy_strings: Vec<_> = s.into_iter().map(|i| i.to_string()).collect();
                let test_id = strategy_strings.join("_");
                let strategy_array = strategy_strings.join(",");
                println!("#[bench] fn comparef64__2power{:02}__3power{:02}__len{:08}__{}(b: &mut Bencher) {{ compare_fft_f64(b, &[{}]); }}", power2, power3, len, test_id, strategy_array);
            }
        }
    }
}

// cargo bench generate_3n2m_planned_benchmarks_32 -- --nocapture --ignored
#[ignore]
#[bench]
fn generate_3n2m_planned_benchmarks(_: &mut test::Bencher) {
    let mut fft_sizes = vec![];

    let max_len: usize = 1 << 23;
    let max_power2 = max_len.trailing_zeros();
    let max_power3 = (max_len as f32).log(3.0).ceil() as u32;
    for power2 in 0..max_power2 {
        for power3 in 0..max_power3 {
            let len = 3usize.pow(power3) << power2;
            if len > max_len {
                continue;
            }
            if power3 < 2 && power2 > 16 {
                continue;
            }
            if power3 < 3 && power2 > 17 {
                continue;
            }
            if power2 < 1 {
                continue;
            }
            fft_sizes.push(len);
        }
    }

    for len in fft_sizes {
        let power2 = len.trailing_zeros();
        let mut remaining_factors = len >> power2;
        let mut power3 = 0;
        while remaining_factors % 3 == 0 {
            power3 += 1;
            remaining_factors /= 3;
        }

        println!("#[bench] fn comparef32_len{:07}_2power{:02}_3power{:02}(b: &mut Bencher) {{ bench_planned_fft_f32(b, {}); }}",len, power2, power3, len);
    }
}

// cargo bench generate_3n2m_planned_benchmarks_64 -- --nocapture --ignored
#[ignore]
#[bench]
fn generate_3n2m_planned_benchmarks_64(_: &mut test::Bencher) {
    let mut fft_sizes = vec![];

    let max_len: usize = 1 << 23;
    let max_power2 = max_len.trailing_zeros();
    let max_power3 = (max_len as f32).log(3.0).ceil() as u32;
    for power2 in 0..max_power2 {
        for power3 in 0..max_power3 {
            let len = 3usize.pow(power3) << power2;
            if len > max_len {
                continue;
            }
            if power3 < 1 && power2 > 13 {
                continue;
            }
            if power3 < 4 && power2 > 14 {
                continue;
            }
            if power2 < 2 {
                continue;
            }
            fft_sizes.push(len);
        }
    }

    for len in fft_sizes {
        let power2 = len.trailing_zeros();
        let mut remaining_factors = len >> power2;
        let mut power3 = 0;
        while remaining_factors % 3 == 0 {
            power3 += 1;
            remaining_factors /= 3;
        }

        println!("#[bench] fn comparef64_len{:07}_2power{:02}_3power{:02}(b: &mut Bencher) {{ bench_planned_fft_f64(b, {}); }}",len, power2, power3, len);
    }
}

#[derive(Copy, Clone, Debug)]
pub struct PartialFactors {
    power2: u32,
    power3: u32,
    power5: u32,
    power7: u32,
    power11: u32,
    other_factors: usize,
}
impl PartialFactors {
    pub fn compute(len: usize) -> Self {
        let power2 = len.trailing_zeros();
        let mut other_factors = len >> power2;

        let mut power3 = 0;
        while other_factors % 3 == 0 {
            power3 += 1;
            other_factors /= 3;
        }

        let mut power5 = 0;
        while other_factors % 5 == 0 {
            power5 += 1;
            other_factors /= 5;
        }

        let mut power7 = 0;
        while other_factors % 7 == 0 {
            power7 += 1;
            other_factors /= 7;
        }

        let mut power11 = 0;
        while other_factors % 11 == 0 {
            power11 += 1;
            other_factors /= 11;
        }

        Self {
            power2,
            power3,
            power5,
            power7,
            power11,
            other_factors,
        }
    }

    pub fn get_power2(&self) -> u32 {
        self.power2
    }
    pub fn get_power3(&self) -> u32 {
        self.power3
    }
    pub fn get_power5(&self) -> u32 {
        self.power5
    }
    pub fn get_power7(&self) -> u32 {
        self.power7
    }
    pub fn get_power11(&self) -> u32 {
        self.power11
    }
    pub fn get_other_factors(&self) -> usize {
        self.other_factors
    }
    pub fn product(&self) -> usize {
        (self.other_factors
            * 3usize.pow(self.power3)
            * 5usize.pow(self.power5)
            * 7usize.pow(self.power7)
            * 11usize.pow(self.power11))
            << self.power2
    }
    pub fn product_power2power3(&self) -> usize {
        3usize.pow(self.power3) << self.power2
    }
    #[allow(unused)]
    pub fn divide_by(&self, divisor: &PartialFactors) -> Option<PartialFactors> {
        let two_divides = self.power2 >= divisor.power2;
        let three_divides = self.power3 >= divisor.power3;
        let five_divides = self.power5 >= divisor.power5;
        let seven_divides = self.power7 >= divisor.power7;
        let eleven_divides = self.power11 >= divisor.power11;
        let other_divides = self.other_factors % divisor.other_factors == 0;
        if two_divides
            && three_divides
            && five_divides
            && seven_divides
            && eleven_divides
            && other_divides
        {
            Some(Self {
                power2: self.power2 - divisor.power2,
                power3: self.power3 - divisor.power3,
                power5: self.power5 - divisor.power5,
                power7: self.power7 - divisor.power7,
                power11: self.power11 - divisor.power11,
                other_factors: if self.other_factors == divisor.other_factors {
                    1
                } else {
                    self.other_factors / divisor.other_factors
                },
            })
        } else {
            None
        }
    }
}

// cargo bench generate_raders_benchmarks -- --nocapture --ignored
#[ignore]
#[bench]
fn generate_raders_benchmarks(_: &mut test::Bencher) {
    for len in 10usize..100000 {
        if miller_rabin(len as u64) {
            let inner_factors = PartialFactors::compute(len - 1);
            if inner_factors.get_other_factors() == 1 && inner_factors.get_power11() > 0 {
                println!("#[bench] fn comparef64_len{:07}_11p{:02}_bluesteins(b: &mut Bencher) {{ bench_planned_bluesteins_f64(b, {}); }}", len, inner_factors.get_power11(), len);
                println!("#[bench] fn comparef64_len{:07}_11p{:02}_raders(b: &mut Bencher) {{ bench_planned_raders_f64(b, {}); }}", len, inner_factors.get_power11(), len);
            }
        }
    }
}

fn wrap_fft<T: FftNum>(fft: impl Fft<T> + 'static) -> Arc<dyn Fft<T>> {
    Arc::new(fft) as Arc<dyn Fft<T>>
}

// passes the given FFT length directly to the FFT planner
fn bench_planned_fft_f32(b: &mut Bencher, len: usize) {
    let mut planner: FftPlanner<f32> = FftPlanner::new();
    let fft = planner.plan_fft_forward(len);

    let mut buffer = vec![Complex::zero(); fft.len()];
    let mut scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| {
        fft.process_with_scratch(&mut buffer, &mut scratch);
    });
}

// passes the given FFT length directly to the FFT planner
fn bench_planned_fft_f64(b: &mut Bencher, len: usize) {
    let mut planner: FftPlanner<f64> = FftPlanner::new();
    let fft = planner.plan_fft_forward(len);

    let mut buffer = vec![Complex::zero(); fft.len()];
    let mut scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| {
        fft.process_with_scratch(&mut buffer, &mut scratch);
    });
}

/*

// Computes the given FFT length using Bluestein's Algorithm, using the planner to plan the inner FFT
fn bench_planned_bluesteins_f32(b: &mut Bencher, len: usize) {
    let mut planner : FftPlannerAvx<f32> = FftPlannerAvx::new(false).unwrap();
    let fft = planner.construct_bluesteins(len);

    let mut buffer = vec![Complex::zero(); fft.len()];
    let mut scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| { fft.process_with_scratch(&mut buffer, &mut scratch); });
}

// Computes the given FFT length using Rader's Algorithm, using the planner to plan the inner FFT
fn bench_planned_raders_f32(b: &mut Bencher, len: usize) {
    let mut planner : FftPlannerAvx<f32> = FftPlannerAvx::new(false).unwrap();
    let fft = planner.construct_raders(len);

    let mut buffer = vec![Complex::zero(); fft.len()];
    let mut scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| { fft.process_with_scratch(&mut buffer, &mut scratch); });
}

// Computes the given FFT length using Bluestein's Algorithm, using the planner to plan the inner FFT
fn bench_planned_bluesteins_f64(b: &mut Bencher, len: usize) {
    let mut planner : FftPlannerAvx<f64> = FftPlannerAvx::new(false).unwrap();
    let fft = planner.construct_bluesteins(len);

    let mut buffer = vec![Complex::zero(); fft.len()];
    let mut scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| { fft.process_with_scratch(&mut buffer, &mut scratch); });
}

// Computes the given FFT length using Rader's Algorithm, using the planner to plan the inner FFT
fn bench_planned_raders_f64(b: &mut Bencher, len: usize) {
    let mut planner : FftPlannerAvx<f64> = FftPlannerAvx::new(false).unwrap();
    let fft = planner.construct_raders(len);

    let mut buffer = vec![Complex::zero(); fft.len()];
    let mut scratch = vec![Complex::zero(); fft.get_inplace_scratch_len()];
    b.iter(|| { fft.process_with_scratch(&mut buffer, &mut scratch); });
}

#[bench] fn comparef64_len0000023_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 23); }
#[bench] fn comparef64_len0000023_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 23); }
#[bench] fn comparef64_len0000067_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 67); }
#[bench] fn comparef64_len0000067_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 67); }
#[bench] fn comparef64_len0000089_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 89); }
#[bench] fn comparef64_len0000089_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 89); }
#[bench] fn comparef64_len0000199_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 199); }
#[bench] fn comparef64_len0000199_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 199); }
#[bench] fn comparef64_len0000331_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 331); }
#[bench] fn comparef64_len0000331_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 331); }
#[bench] fn comparef64_len0000353_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 353); }
#[bench] fn comparef64_len0000353_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 353); }
#[bench] fn comparef64_len0000397_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 397); }
#[bench] fn comparef64_len0000397_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 397); }
#[bench] fn comparef64_len0000463_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 463); }
#[bench] fn comparef64_len0000463_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 463); }
#[bench] fn comparef64_len0000617_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 617); }
#[bench] fn comparef64_len0000617_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 617); }
#[bench] fn comparef64_len0000661_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 661); }
#[bench] fn comparef64_len0000661_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 661); }
#[bench] fn comparef64_len0000727_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 727); }
#[bench] fn comparef64_len0000727_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 727); }
#[bench] fn comparef64_len0000881_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 881); }
#[bench] fn comparef64_len0000881_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 881); }
#[bench] fn comparef64_len0000991_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 991); }
#[bench] fn comparef64_len0000991_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 991); }
#[bench] fn comparef64_len0001321_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 1321); }
#[bench] fn comparef64_len0001321_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 1321); }
#[bench] fn comparef64_len0001409_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 1409); }
#[bench] fn comparef64_len0001409_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 1409); }
#[bench] fn comparef64_len0001453_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 1453); }
#[bench] fn comparef64_len0001453_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 1453); }
#[bench] fn comparef64_len0001783_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 1783); }
#[bench] fn comparef64_len0001783_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 1783); }
#[bench] fn comparef64_len0002113_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 2113); }
#[bench] fn comparef64_len0002113_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 2113); }
#[bench] fn comparef64_len0002179_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 2179); }
#[bench] fn comparef64_len0002179_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 2179); }
#[bench] fn comparef64_len0002311_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 2311); }
#[bench] fn comparef64_len0002311_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 2311); }
#[bench] fn comparef64_len0002377_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 2377); }
#[bench] fn comparef64_len0002377_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 2377); }
#[bench] fn comparef64_len0002663_11p03_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 2663); }
#[bench] fn comparef64_len0002663_11p03_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 2663); }
#[bench] fn comparef64_len0002971_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 2971); }
#[bench] fn comparef64_len0002971_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 2971); }
#[bench] fn comparef64_len0003169_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 3169); }
#[bench] fn comparef64_len0003169_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 3169); }
#[bench] fn comparef64_len0003301_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 3301); }
#[bench] fn comparef64_len0003301_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 3301); }
#[bench] fn comparef64_len0003389_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 3389); }
#[bench] fn comparef64_len0003389_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 3389); }
#[bench] fn comparef64_len0003631_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 3631); }
#[bench] fn comparef64_len0003631_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 3631); }
#[bench] fn comparef64_len0003697_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 3697); }
#[bench] fn comparef64_len0003697_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 3697); }
#[bench] fn comparef64_len0003851_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 3851); }
#[bench] fn comparef64_len0003851_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 3851); }
#[bench] fn comparef64_len0004159_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 4159); }
#[bench] fn comparef64_len0004159_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 4159); }
#[bench] fn comparef64_len0004357_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 4357); }
#[bench] fn comparef64_len0004357_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 4357); }
#[bench] fn comparef64_len0004621_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 4621); }
#[bench] fn comparef64_len0004621_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 4621); }
#[bench] fn comparef64_len0004951_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 4951); }
#[bench] fn comparef64_len0004951_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 4951); }
#[bench] fn comparef64_len0005281_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 5281); }
#[bench] fn comparef64_len0005281_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 5281); }
#[bench] fn comparef64_len0005347_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 5347); }
#[bench] fn comparef64_len0005347_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 5347); }
#[bench] fn comparef64_len0005501_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 5501); }
#[bench] fn comparef64_len0005501_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 5501); }
#[bench] fn comparef64_len0006337_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 6337); }
#[bench] fn comparef64_len0006337_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 6337); }
#[bench] fn comparef64_len0006469_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 6469); }
#[bench] fn comparef64_len0006469_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 6469); }
#[bench] fn comparef64_len0007129_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 7129); }
#[bench] fn comparef64_len0007129_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 7129); }
#[bench] fn comparef64_len0007393_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 7393); }
#[bench] fn comparef64_len0007393_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 7393); }
#[bench] fn comparef64_len0007547_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 7547); }
#[bench] fn comparef64_len0007547_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 7547); }
#[bench] fn comparef64_len0008317_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 8317); }
#[bench] fn comparef64_len0008317_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 8317); }
#[bench] fn comparef64_len0008713_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 8713); }
#[bench] fn comparef64_len0008713_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 8713); }
#[bench] fn comparef64_len0009241_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 9241); }
#[bench] fn comparef64_len0009241_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 9241); }
#[bench] fn comparef64_len0009857_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 9857); }
#[bench] fn comparef64_len0009857_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 9857); }
#[bench] fn comparef64_len0009901_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 9901); }
#[bench] fn comparef64_len0009901_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 9901); }
#[bench] fn comparef64_len0010781_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 10781); }
#[bench] fn comparef64_len0010781_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 10781); }
#[bench] fn comparef64_len0010891_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 10891); }
#[bench] fn comparef64_len0010891_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 10891); }
#[bench] fn comparef64_len0011551_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 11551); }
#[bench] fn comparef64_len0011551_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 11551); }
#[bench] fn comparef64_len0011617_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 11617); }
#[bench] fn comparef64_len0011617_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 11617); }
#[bench] fn comparef64_len0012101_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 12101); }
#[bench] fn comparef64_len0012101_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 12101); }
#[bench] fn comparef64_len0013553_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 13553); }
#[bench] fn comparef64_len0013553_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 13553); }
#[bench] fn comparef64_len0013751_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 13751); }
#[bench] fn comparef64_len0013751_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 13751); }
#[bench] fn comparef64_len0014081_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 14081); }
#[bench] fn comparef64_len0014081_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 14081); }
#[bench] fn comparef64_len0014851_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 14851); }
#[bench] fn comparef64_len0014851_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 14851); }
#[bench] fn comparef64_len0015401_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 15401); }
#[bench] fn comparef64_len0015401_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 15401); }
#[bench] fn comparef64_len0015973_11p03_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 15973); }
#[bench] fn comparef64_len0015973_11p03_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 15973); }
#[bench] fn comparef64_len0016633_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 16633); }
#[bench] fn comparef64_len0016633_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 16633); }
#[bench] fn comparef64_len0018481_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 18481); }
#[bench] fn comparef64_len0018481_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 18481); }
#[bench] fn comparef64_len0019009_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 19009); }
#[bench] fn comparef64_len0019009_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 19009); }
#[bench] fn comparef64_len0019603_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 19603); }
#[bench] fn comparef64_len0019603_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 19603); }
#[bench] fn comparef64_len0019801_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 19801); }
#[bench] fn comparef64_len0019801_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 19801); }
#[bench] fn comparef64_len0021121_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 21121); }
#[bench] fn comparef64_len0021121_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 21121); }
#[bench] fn comparef64_len0022639_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 22639); }
#[bench] fn comparef64_len0022639_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 22639); }
#[bench] fn comparef64_len0023761_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 23761); }
#[bench] fn comparef64_len0023761_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 23761); }
#[bench] fn comparef64_len0025411_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 25411); }
#[bench] fn comparef64_len0025411_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 25411); }
#[bench] fn comparef64_len0025873_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 25873); }
#[bench] fn comparef64_len0025873_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 25873); }
#[bench] fn comparef64_len0026731_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 26731); }
#[bench] fn comparef64_len0026731_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 26731); }
#[bench] fn comparef64_len0026951_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 26951); }
#[bench] fn comparef64_len0026951_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 26951); }
#[bench] fn comparef64_len0028513_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 28513); }
#[bench] fn comparef64_len0028513_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 28513); }
#[bench] fn comparef64_len0029569_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 29569); }
#[bench] fn comparef64_len0029569_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 29569); }
#[bench] fn comparef64_len0030493_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 30493); }
#[bench] fn comparef64_len0030493_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 30493); }
#[bench] fn comparef64_len0030977_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 30977); }
#[bench] fn comparef64_len0030977_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 30977); }
#[bench] fn comparef64_len0032077_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 32077); }
#[bench] fn comparef64_len0032077_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 32077); }
#[bench] fn comparef64_len0032341_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 32341); }
#[bench] fn comparef64_len0032341_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 32341); }
#[bench] fn comparef64_len0034651_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 34651); }
#[bench] fn comparef64_len0034651_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 34651); }
#[bench] fn comparef64_len0034849_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 34849); }
#[bench] fn comparef64_len0034849_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 34849); }
#[bench] fn comparef64_len0035201_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 35201); }
#[bench] fn comparef64_len0035201_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 35201); }
#[bench] fn comparef64_len0037423_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 37423); }
#[bench] fn comparef64_len0037423_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 37423); }
#[bench] fn comparef64_len0038501_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 38501); }
#[bench] fn comparef64_len0038501_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 38501); }
#[bench] fn comparef64_len0047521_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 47521); }
#[bench] fn comparef64_len0047521_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 47521); }
#[bench] fn comparef64_len0047917_11p03_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 47917); }
#[bench] fn comparef64_len0047917_11p03_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 47917); }
#[bench] fn comparef64_len0050821_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 50821); }
#[bench] fn comparef64_len0050821_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 50821); }
#[bench] fn comparef64_len0055001_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 55001); }
#[bench] fn comparef64_len0055001_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 55001); }
#[bench] fn comparef64_len0055441_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 55441); }
#[bench] fn comparef64_len0055441_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 55441); }
#[bench] fn comparef64_len0055903_11p03_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 55903); }
#[bench] fn comparef64_len0055903_11p03_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 55903); }
#[bench] fn comparef64_len0057751_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 57751); }
#[bench] fn comparef64_len0057751_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 57751); }
#[bench] fn comparef64_len0063361_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 63361); }
#[bench] fn comparef64_len0063361_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 63361); }
#[bench] fn comparef64_len0064153_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 64153); }
#[bench] fn comparef64_len0064153_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 64153); }
#[bench] fn comparef64_len0066529_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 66529); }
#[bench] fn comparef64_len0066529_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 66529); }
#[bench] fn comparef64_len0068993_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 68993); }
#[bench] fn comparef64_len0068993_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 68993); }
#[bench] fn comparef64_len0069697_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 69697); }
#[bench] fn comparef64_len0069697_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 69697); }
#[bench] fn comparef64_len0076231_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 76231); }
#[bench] fn comparef64_len0076231_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 76231); }
#[bench] fn comparef64_len0077617_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 77617); }
#[bench] fn comparef64_len0077617_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 77617); }
#[bench] fn comparef64_len0079201_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 79201); }
#[bench] fn comparef64_len0079201_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 79201); }
#[bench] fn comparef64_len0079861_11p03_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 79861); }
#[bench] fn comparef64_len0079861_11p03_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 79861); }
#[bench] fn comparef64_len0080191_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 80191); }
#[bench] fn comparef64_len0080191_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 80191); }
#[bench] fn comparef64_len0084481_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 84481); }
#[bench] fn comparef64_len0084481_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 84481); }
#[bench] fn comparef64_len0084701_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 84701); }
#[bench] fn comparef64_len0084701_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 84701); }
#[bench] fn comparef64_len0087121_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 87121); }
#[bench] fn comparef64_len0087121_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 87121); }
#[bench] fn comparef64_len0088001_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 88001); }
#[bench] fn comparef64_len0088001_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 88001); }
#[bench] fn comparef64_len0089101_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 89101); }
#[bench] fn comparef64_len0089101_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 89101); }
#[bench] fn comparef64_len0092401_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 92401); }
#[bench] fn comparef64_len0092401_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 92401); }
#[bench] fn comparef64_len0097021_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 97021); }
#[bench] fn comparef64_len0097021_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 97021); }
#[bench] fn comparef64_len0098011_11p02_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 98011); }
#[bench] fn comparef64_len0098011_11p02_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 98011); }
#[bench] fn comparef64_len0098561_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 98561); }
#[bench] fn comparef64_len0098561_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 98561); }
#[bench] fn comparef64_len0099793_11p01_bluesteins(b: &mut Bencher) { bench_planned_bluesteins_f64(b, 99793); }
#[bench] fn comparef64_len0099793_11p01_raders(b: &mut Bencher) { bench_planned_raders_f64(b, 99793); }

*/
