use std::sync::Arc;

use num_complex::Complex;

use crate::array_utils::{factor_transpose, Load, LoadStore, TransposeFactor};
use crate::common::RadixFactor;
use crate::{common::FftNum, twiddles, FftDirection};
use crate::{Direction, Fft, Length};

use super::butterflies::{Butterfly2, Butterfly3, Butterfly4, Butterfly5, Butterfly6, Butterfly7};

#[repr(u8)]
enum InternalRadixFactor<T> {
    Factor2(Butterfly2<T>),
    Factor3(Butterfly3<T>),
    Factor4(Butterfly4<T>),
    Factor5(Butterfly5<T>),
    Factor6(Butterfly6<T>),
    Factor7(Butterfly7<T>),
}
impl<T> InternalRadixFactor<T> {
    pub const fn radix(&self) -> usize {
        // note: if we had rustc 1.66, we could just turn these values explicit discriminators on the enum
        match self {
            InternalRadixFactor::Factor2(_) => 2,
            InternalRadixFactor::Factor3(_) => 3,
            InternalRadixFactor::Factor4(_) => 4,
            InternalRadixFactor::Factor5(_) => 5,
            InternalRadixFactor::Factor6(_) => 6,
            InternalRadixFactor::Factor7(_) => 7,
        }
    }
}

pub(crate) struct RadixN<T> {
    twiddles: Box<[Complex<T>]>,

    base_fft: Arc<dyn Fft<T>>,
    base_len: usize,

    factors: Box<[TransposeFactor]>,
    butterflies: Box<[InternalRadixFactor<T>]>,

    len: usize,
    direction: FftDirection,

    inplace_scratch_len: usize,
    outofplace_scratch_len: usize,
    immut_scratch_len: usize,
}

impl<T: FftNum> RadixN<T> {
    /// Constructs a RadixN instance which computes FFTs of length `factor_product * base_fft.len()`
    pub fn new(factors: &[RadixFactor], base_fft: Arc<dyn Fft<T>>) -> Self {
        let base_len = base_fft.len();
        let direction = base_fft.fft_direction();

        // set up our cross FFT butterfly instances. simultaneously, compute the number of twiddle factors
        let mut butterflies = Vec::with_capacity(factors.len());
        let mut cross_fft_len = base_len;
        let mut twiddle_count = 0;

        for factor in factors {
            // compute how many twiddles this cross-FFT needs
            let cross_fft_rows = factor.radix();
            let cross_fft_columns = cross_fft_len;

            twiddle_count += cross_fft_columns * (cross_fft_rows - 1);

            // set up the butterfly for this cross-FFT
            let butterfly = match factor {
                RadixFactor::Factor2 => InternalRadixFactor::Factor2(Butterfly2::new(direction)),
                RadixFactor::Factor3 => InternalRadixFactor::Factor3(Butterfly3::new(direction)),
                RadixFactor::Factor4 => InternalRadixFactor::Factor4(Butterfly4::new(direction)),
                RadixFactor::Factor5 => InternalRadixFactor::Factor5(Butterfly5::new(direction)),
                RadixFactor::Factor6 => InternalRadixFactor::Factor6(Butterfly6::new(direction)),
                RadixFactor::Factor7 => InternalRadixFactor::Factor7(Butterfly7::new(direction)),
            };
            butterflies.push(butterfly);

            cross_fft_len *= cross_fft_rows;
        }
        let len = cross_fft_len;

        // set up our list of transpose factors - it's the same list but reversed, and we want to collapse duplicates
        // Note that we are only de-duplicating adjacent factors. If we're passed 7 * 2 * 7, we can't collapse the sevens
        // because the exact order of factors is is important for the transpose
        let mut transpose_factors: Vec<TransposeFactor> = Vec::with_capacity(factors.len());
        for f in factors.iter().rev() {
            // I really want let chains for this!
            let mut push_new = true;
            if let Some(last) = transpose_factors.last_mut() {
                if last.factor == *f {
                    last.count += 1;
                    push_new = false;
                }
            }
            if push_new {
                transpose_factors.push(TransposeFactor {
                    factor: *f,
                    count: 1,
                });
            }
        }

        // precompute the twiddle factors this algorithm will use.
        // we're doing the same precomputation of twiddle factors as the mixed radix algorithm where width=factor.radix() and height=len/factor.radix()
        // but mixed radix only does one step and then calls itself recusrively, and this algorithm does every layer all the way down
        // so we're going to pack all the "layers" of twiddle factors into a single array, starting with the bottom layer and going up
        let mut cross_fft_len = base_len;
        let mut twiddle_factors = Vec::with_capacity(twiddle_count);

        for factor in factors {
            // Compute the twiddle factors for the cross FFT
            let cross_fft_columns = cross_fft_len;
            cross_fft_len *= factor.radix();

            for i in 0..cross_fft_columns {
                for k in 1..factor.radix() {
                    let twiddle = twiddles::compute_twiddle(i * k, cross_fft_len, direction);
                    twiddle_factors.push(twiddle);
                }
            }
        }

        // figure out how much scratch space we need to request from callers
        let base_inplace_scratch = base_fft.get_inplace_scratch_len();
        let inplace_scratch_len = if base_inplace_scratch > len {
            len + base_inplace_scratch
        } else {
            len
        };
        let outofplace_scratch_len = if base_inplace_scratch > len {
            base_inplace_scratch
        } else {
            0
        };

        Self {
            twiddles: twiddle_factors.into_boxed_slice(),

            base_fft,
            base_len,

            factors: transpose_factors.into_boxed_slice(),
            butterflies: butterflies.into_boxed_slice(),

            len,
            direction,

            inplace_scratch_len,
            outofplace_scratch_len,
            immut_scratch_len: base_inplace_scratch,
        }
    }

    fn inplace_scratch_len(&self) -> usize {
        self.inplace_scratch_len
    }
    fn outofplace_scratch_len(&self) -> usize {
        self.outofplace_scratch_len
    }
    fn immut_scratch_len(&self) -> usize {
        self.immut_scratch_len
    }

    fn perform_fft_immut(
        &self,
        input: &[Complex<T>],
        output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    ) {
        if let Some(unroll_factor) = self.factors.first() {
            // for performance, we really, really want to unroll the transpose, but we need to make sure the output length is divisible by the unroll amount
            // choosing the first factor seems to reliably perform well
            match unroll_factor.factor {
                RadixFactor::Factor2 => {
                    factor_transpose::<Complex<T>, 2>(self.base_len, input, output, &self.factors)
                }
                RadixFactor::Factor3 => {
                    factor_transpose::<Complex<T>, 3>(self.base_len, input, output, &self.factors)
                }
                RadixFactor::Factor4 => {
                    factor_transpose::<Complex<T>, 4>(self.base_len, input, output, &self.factors)
                }
                RadixFactor::Factor5 => {
                    factor_transpose::<Complex<T>, 5>(self.base_len, input, output, &self.factors)
                }
                RadixFactor::Factor6 => {
                    factor_transpose::<Complex<T>, 6>(self.base_len, input, output, &self.factors)
                }
                RadixFactor::Factor7 => {
                    factor_transpose::<Complex<T>, 7>(self.base_len, input, output, &self.factors)
                }
            }
        } else {
            // no factors, so just pass data straight to our base
            output.copy_from_slice(input);
        }

        // Base-level FFTs
        self.base_fft.process_with_scratch(output, scratch);

        let mut cross_fft_len = self.base_len;
        let mut layer_twiddles: &[Complex<T>] = &self.twiddles;

        for factor in self.butterflies.iter() {
            let cross_fft_columns = cross_fft_len;
            cross_fft_len *= factor.radix();

            match factor {
                InternalRadixFactor::Factor2(butterfly2) => {
                    for data in output.chunks_exact_mut(cross_fft_len) {
                        unsafe { butterfly_2(data, layer_twiddles, cross_fft_columns, butterfly2) }
                    }
                }
                InternalRadixFactor::Factor3(butterfly3) => {
                    for data in output.chunks_exact_mut(cross_fft_len) {
                        unsafe { butterfly_3(data, layer_twiddles, cross_fft_columns, butterfly3) }
                    }
                }
                InternalRadixFactor::Factor4(butterfly4) => {
                    for data in output.chunks_exact_mut(cross_fft_len) {
                        unsafe { butterfly_4(data, layer_twiddles, cross_fft_columns, butterfly4) }
                    }
                }
                InternalRadixFactor::Factor5(butterfly5) => {
                    for data in output.chunks_exact_mut(cross_fft_len) {
                        unsafe { butterfly_5(data, layer_twiddles, cross_fft_columns, butterfly5) }
                    }
                }
                InternalRadixFactor::Factor6(butterfly6) => {
                    for data in output.chunks_exact_mut(cross_fft_len) {
                        unsafe { butterfly_6(data, layer_twiddles, cross_fft_columns, butterfly6) }
                    }
                }
                InternalRadixFactor::Factor7(butterfly7) => {
                    for data in output.chunks_exact_mut(cross_fft_len) {
                        unsafe { butterfly_7(data, layer_twiddles, cross_fft_columns, butterfly7) }
                    }
                }
            }

            // skip past all the twiddle factors used in this layer
            let twiddle_offset = cross_fft_columns * (factor.radix() - 1);
            layer_twiddles = &layer_twiddles[twiddle_offset..];
        }
    }

    fn perform_fft_out_of_place(
        &self,
        input: &mut [Complex<T>],
        output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    ) {
        if let Some(unroll_factor) = self.factors.first() {
            // for performance, we really, really want to unroll the transpose, but we need to make sure the output length is divisible by the unroll amount
            // choosing the first factor seems to reliably perform well
            match unroll_factor.factor {
                RadixFactor::Factor2 => {
                    factor_transpose::<Complex<T>, 2>(self.base_len, input, output, &self.factors)
                }
                RadixFactor::Factor3 => {
                    factor_transpose::<Complex<T>, 3>(self.base_len, input, output, &self.factors)
                }
                RadixFactor::Factor4 => {
                    factor_transpose::<Complex<T>, 4>(self.base_len, input, output, &self.factors)
                }
                RadixFactor::Factor5 => {
                    factor_transpose::<Complex<T>, 5>(self.base_len, input, output, &self.factors)
                }
                RadixFactor::Factor6 => {
                    factor_transpose::<Complex<T>, 6>(self.base_len, input, output, &self.factors)
                }
                RadixFactor::Factor7 => {
                    factor_transpose::<Complex<T>, 7>(self.base_len, input, output, &self.factors)
                }
            }
        } else {
            // no factors, so just pass data straight to our base
            output.copy_from_slice(input);
        }

        // Base-level FFTs
        let base_scratch = if scratch.len() > 0 { scratch } else { input };
        self.base_fft.process_with_scratch(output, base_scratch);

        // cross-FFTs
        let mut cross_fft_len = self.base_len;
        let mut layer_twiddles: &[Complex<T>] = &self.twiddles;

        for factor in self.butterflies.iter() {
            let cross_fft_columns = cross_fft_len;
            cross_fft_len *= factor.radix();

            match factor {
                InternalRadixFactor::Factor2(butterfly2) => {
                    for data in output.chunks_exact_mut(cross_fft_len) {
                        unsafe { butterfly_2(data, layer_twiddles, cross_fft_columns, butterfly2) }
                    }
                }
                InternalRadixFactor::Factor3(butterfly3) => {
                    for data in output.chunks_exact_mut(cross_fft_len) {
                        unsafe { butterfly_3(data, layer_twiddles, cross_fft_columns, butterfly3) }
                    }
                }
                InternalRadixFactor::Factor4(butterfly4) => {
                    for data in output.chunks_exact_mut(cross_fft_len) {
                        unsafe { butterfly_4(data, layer_twiddles, cross_fft_columns, butterfly4) }
                    }
                }
                InternalRadixFactor::Factor5(butterfly5) => {
                    for data in output.chunks_exact_mut(cross_fft_len) {
                        unsafe { butterfly_5(data, layer_twiddles, cross_fft_columns, butterfly5) }
                    }
                }
                InternalRadixFactor::Factor6(butterfly6) => {
                    for data in output.chunks_exact_mut(cross_fft_len) {
                        unsafe { butterfly_6(data, layer_twiddles, cross_fft_columns, butterfly6) }
                    }
                }
                InternalRadixFactor::Factor7(butterfly7) => {
                    for data in output.chunks_exact_mut(cross_fft_len) {
                        unsafe { butterfly_7(data, layer_twiddles, cross_fft_columns, butterfly7) }
                    }
                }
            }

            // skip past all the twiddle factors used in this layer
            let twiddle_offset = cross_fft_columns * (factor.radix() - 1);
            layer_twiddles = &layer_twiddles[twiddle_offset..];
        }
    }
}
boilerplate_fft_oop!(RadixN, |this: &RadixN<_>| this.len);

#[inline(never)]
pub(crate) unsafe fn butterfly_2<T: FftNum>(
    mut data: impl LoadStore<T>,
    twiddles: impl Load<T>,
    num_columns: usize,
    butterfly2: &Butterfly2<T>,
) {
    for idx in 0..num_columns {
        let mut scratch = [
            data.load(idx + 0 * num_columns),
            data.load(idx + 1 * num_columns) * twiddles.load(idx),
        ];

        butterfly2.perform_fft_butterfly(&mut scratch);

        data.store(scratch[0], idx + num_columns * 0);
        data.store(scratch[1], idx + num_columns * 1);
    }
}

#[inline(never)]
pub(crate) unsafe fn butterfly_3<T: FftNum>(
    mut data: impl LoadStore<T>,
    twiddles: impl Load<T>,
    num_columns: usize,
    butterfly3: &Butterfly3<T>,
) {
    for idx in 0..num_columns {
        let tw_idx = idx * 2;
        let mut scratch = [
            data.load(idx + 0 * num_columns),
            data.load(idx + 1 * num_columns) * twiddles.load(tw_idx + 0),
            data.load(idx + 2 * num_columns) * twiddles.load(tw_idx + 1),
        ];

        butterfly3.perform_fft_butterfly(&mut scratch);

        data.store(scratch[0], idx + 0 * num_columns);
        data.store(scratch[1], idx + 1 * num_columns);
        data.store(scratch[2], idx + 2 * num_columns);
    }
}

#[inline(never)]
pub(crate) unsafe fn butterfly_4<T: FftNum>(
    mut data: impl LoadStore<T>,
    twiddles: impl Load<T>,
    num_columns: usize,
    butterfly4: &Butterfly4<T>,
) {
    for idx in 0..num_columns {
        let tw_idx = idx * 3;
        let mut scratch = [
            data.load(idx + 0 * num_columns),
            data.load(idx + 1 * num_columns) * twiddles.load(tw_idx + 0),
            data.load(idx + 2 * num_columns) * twiddles.load(tw_idx + 1),
            data.load(idx + 3 * num_columns) * twiddles.load(tw_idx + 2),
        ];

        butterfly4.perform_fft_butterfly(&mut scratch);

        data.store(scratch[0], idx + 0 * num_columns);
        data.store(scratch[1], idx + 1 * num_columns);
        data.store(scratch[2], idx + 2 * num_columns);
        data.store(scratch[3], idx + 3 * num_columns);
    }
}

#[inline(never)]
pub(crate) unsafe fn butterfly_5<T: FftNum>(
    mut data: impl LoadStore<T>,
    twiddles: impl Load<T>,
    num_columns: usize,
    butterfly5: &Butterfly5<T>,
) {
    for idx in 0..num_columns {
        let tw_idx = idx * 4;
        let mut scratch = [
            data.load(idx + 0 * num_columns),
            data.load(idx + 1 * num_columns) * twiddles.load(tw_idx + 0),
            data.load(idx + 2 * num_columns) * twiddles.load(tw_idx + 1),
            data.load(idx + 3 * num_columns) * twiddles.load(tw_idx + 2),
            data.load(idx + 4 * num_columns) * twiddles.load(tw_idx + 3),
        ];

        butterfly5.perform_fft_butterfly(&mut scratch);

        data.store(scratch[0], idx + 0 * num_columns);
        data.store(scratch[1], idx + 1 * num_columns);
        data.store(scratch[2], idx + 2 * num_columns);
        data.store(scratch[3], idx + 3 * num_columns);
        data.store(scratch[4], idx + 4 * num_columns);
    }
}

#[inline(never)]
pub(crate) unsafe fn butterfly_6<T: FftNum>(
    mut data: impl LoadStore<T>,
    twiddles: impl Load<T>,
    num_columns: usize,
    butterfly6: &Butterfly6<T>,
) {
    for idx in 0..num_columns {
        let tw_idx = idx * 5;
        let mut scratch = [
            data.load(idx + 0 * num_columns),
            data.load(idx + 1 * num_columns) * twiddles.load(tw_idx + 0),
            data.load(idx + 2 * num_columns) * twiddles.load(tw_idx + 1),
            data.load(idx + 3 * num_columns) * twiddles.load(tw_idx + 2),
            data.load(idx + 4 * num_columns) * twiddles.load(tw_idx + 3),
            data.load(idx + 5 * num_columns) * twiddles.load(tw_idx + 4),
        ];

        butterfly6.perform_fft_butterfly(&mut scratch);

        data.store(scratch[0], idx + 0 * num_columns);
        data.store(scratch[1], idx + 1 * num_columns);
        data.store(scratch[2], idx + 2 * num_columns);
        data.store(scratch[3], idx + 3 * num_columns);
        data.store(scratch[4], idx + 4 * num_columns);
        data.store(scratch[5], idx + 5 * num_columns);
    }
}

#[inline(never)]
pub(crate) unsafe fn butterfly_7<T: FftNum>(
    mut data: impl LoadStore<T>,
    twiddles: impl Load<T>,
    num_columns: usize,
    butterfly7: &Butterfly7<T>,
) {
    for idx in 0..num_columns {
        let tw_idx = idx * 6;
        let mut scratch = [
            data.load(idx + 0 * num_columns),
            data.load(idx + 1 * num_columns) * twiddles.load(tw_idx + 0),
            data.load(idx + 2 * num_columns) * twiddles.load(tw_idx + 1),
            data.load(idx + 3 * num_columns) * twiddles.load(tw_idx + 2),
            data.load(idx + 4 * num_columns) * twiddles.load(tw_idx + 3),
            data.load(idx + 5 * num_columns) * twiddles.load(tw_idx + 4),
            data.load(idx + 6 * num_columns) * twiddles.load(tw_idx + 5),
        ];

        butterfly7.perform_fft_butterfly(&mut scratch);

        data.store(scratch[0], idx + 0 * num_columns);
        data.store(scratch[1], idx + 1 * num_columns);
        data.store(scratch[2], idx + 2 * num_columns);
        data.store(scratch[3], idx + 3 * num_columns);
        data.store(scratch[4], idx + 4 * num_columns);
        data.store(scratch[5], idx + 5 * num_columns);
        data.store(scratch[6], idx + 6 * num_columns);
    }
}

#[cfg(test)]
mod unit_tests {
    use super::*;
    use crate::test_utils::{check_fft_algorithm, construct_base};

    #[test]
    fn test_scalar_radixn() {
        let factor_list = &[
            RadixFactor::Factor2,
            RadixFactor::Factor3,
            RadixFactor::Factor4,
            RadixFactor::Factor5,
            RadixFactor::Factor6,
            RadixFactor::Factor7,
        ];

        for base in 1..7 {
            let base_forward = construct_base(base, FftDirection::Forward);
            let base_inverse = construct_base(base, FftDirection::Inverse);

            // test just the base with no factors
            test_radixn(&[], Arc::clone(&base_forward));
            test_radixn(&[], Arc::clone(&base_inverse));

            // test one factor
            for factor_a in factor_list {
                let factors = &[*factor_a];
                test_radixn(factors, Arc::clone(&base_forward));
                test_radixn(factors, Arc::clone(&base_inverse));
            }

            // test two factors
            for factor_a in factor_list {
                for factor_b in factor_list {
                    let factors = &[*factor_a, *factor_b];
                    test_radixn(factors, Arc::clone(&base_forward));
                    test_radixn(factors, Arc::clone(&base_inverse));
                }
            }
        }
    }

    fn test_radixn(factors: &[RadixFactor], base_fft: Arc<dyn Fft<f64>>) {
        let len = base_fft.len() * factors.iter().map(|f| f.radix()).product::<usize>();
        let direction = base_fft.fft_direction();
        let fft = RadixN::new(factors, base_fft);

        check_fft_algorithm::<f64>(&fft, len, direction);
    }
}
