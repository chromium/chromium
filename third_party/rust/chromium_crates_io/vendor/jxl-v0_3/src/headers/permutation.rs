// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::borrow::Cow;

use crate::bit_reader::BitReader;
use crate::entropy_coding::decode::Histograms;
use crate::entropy_coding::decode::SymbolReader;
use crate::error::{Error, Result};
use crate::util::{CeilLog2, NewWithCapacity, tracing_wrappers::instrument, value_of_lowest_1_bit};

#[derive(Debug, PartialEq, Default, Clone)]
pub struct Permutation(pub Cow<'static, [u32]>);

impl std::ops::Deref for Permutation {
    type Target = [u32];

    fn deref(&self) -> &[u32] {
        &self.0
    }
}

impl Permutation {
    /// Decode a permutation from entropy-coded stream.
    pub fn decode(
        size: u32,
        skip: u32,
        histograms: &Histograms,
        br: &mut BitReader,
        entropy_reader: &mut SymbolReader,
    ) -> Result<Self> {
        let end = entropy_reader.read_unsigned(histograms, br, get_context(size));
        Self::decode_inner(size, skip, end, |ctx| -> Result<u32> {
            let r = entropy_reader.read_unsigned(histograms, br, ctx);
            br.check_for_error()?;
            Ok(r)
        })
    }

    fn decode_inner(
        size: u32,
        skip: u32,
        end: u32,
        mut read: impl FnMut(usize) -> Result<u32>,
    ) -> Result<Self> {
        if end > size - skip {
            return Err(Error::InvalidPermutationSize { size, skip, end });
        }

        let mut lehmer = Vec::new_with_capacity(end as usize)?;

        let mut prev_val = 0u32;
        for idx in skip..(skip + end) {
            let val = match read(get_context(prev_val)) {
                Ok(val) => val,
                Err(Error::OutOfBounds(_)) => {
                    // Estimate 1.5 bits for each remaining code
                    let bits = (((skip + end) - idx) as usize).saturating_mul(3) / 2;
                    return Err(Error::OutOfBounds(bits));
                }
                Err(e) => return Err(e),
            };
            if val >= size - idx {
                return Err(Error::InvalidPermutationLehmerCode {
                    size,
                    idx,
                    lehmer: val,
                });
            }
            lehmer.push(val);
            prev_val = val;
        }

        // Initialize the full permutation vector with skipped elements intact
        let mut permutation = Vec::new_with_capacity((size - skip) as usize)?;
        permutation.extend(0..size);

        // Decode the Lehmer code into the slice starting at `skip`
        let permuted_slice = decode_lehmer_code(&lehmer, &permutation[skip as usize..])?;

        // Replace the target slice in `permutation`
        permutation[skip as usize..].copy_from_slice(&permuted_slice);

        // Ensure the permutation has the correct size
        assert_eq!(permutation.len(), size as usize);

        Ok(Self(Cow::Owned(permutation)))
    }

    pub fn compose(&mut self, other: &Permutation) {
        assert_eq!(self.0.len(), other.0.len());
        let mut tmp: Vec<u32> = vec![0; self.0.len()];
        for (i, val) in tmp.iter_mut().enumerate().take(self.0.len()) {
            *val = self.0[other.0[i] as usize]
        }
        self.0.to_mut().copy_from_slice(&tmp[..]);
    }
}

// Decodes the Lehmer code in `code` and returns the permuted slice.
#[instrument(level = "debug", ret, err)]
fn decode_lehmer_code(code: &[u32], permutation_slice: &[u32]) -> Result<Vec<u32>> {
    let n = permutation_slice.len();
    if n == 0 {
        return Err(Error::InvalidPermutationLehmerCode {
            size: 0,
            idx: 0,
            lehmer: 0,
        });
    }

    let mut permuted = Vec::new_with_capacity(n)?;
    permuted.extend_from_slice(permutation_slice);

    let padded_n = (n as u32).next_power_of_two() as usize;

    // Allocate temp array inside the function
    let mut temp = Vec::new_with_capacity(padded_n)?;
    temp.extend((0..padded_n as u32).map(|x| value_of_lowest_1_bit(x + 1)));

    for (i, permuted_item) in permuted.iter_mut().enumerate() {
        let code_i = *code.get(i).unwrap_or(&0);

        // Adjust the maximum allowed value for code_i
        if code_i as usize > n - i - 1 {
            return Err(Error::InvalidPermutationLehmerCode {
                size: n as u32,
                idx: i as u32,
                lehmer: code_i,
            });
        }

        let mut rank = code_i + 1;

        // Extract i-th unused element via implicit order-statistics tree.
        let mut bit = padded_n;
        let mut next = 0usize;
        while bit != 0 {
            let cand = next + bit;
            if cand == 0 || cand > padded_n {
                return Err(Error::InvalidPermutationLehmerCode {
                    size: n as u32,
                    idx: i as u32,
                    lehmer: code_i,
                });
            }
            bit >>= 1;
            if temp[cand - 1] < rank {
                next = cand;
                rank -= temp[cand - 1];
            }
        }

        *permuted_item = permutation_slice[next];

        next += 1;
        while next <= padded_n {
            temp[next - 1] -= 1;
            next += value_of_lowest_1_bit(next as u32) as usize;
        }
    }

    Ok(permuted)
}

// Decodes the Lehmer code in `code` and returns the permuted vector.
#[cfg(test)]
fn decode_lehmer_code_naive(code: &[u32], permutation_slice: &[u32]) -> Result<Vec<u32>> {
    let n = code.len();
    if n == 0 {
        return Err(Error::InvalidPermutationLehmerCode {
            size: 0,
            idx: 0,
            lehmer: 0,
        });
    }

    // Ensure permutation_slice has sufficient length
    if permutation_slice.len() < n {
        return Err(Error::InvalidPermutationLehmerCode {
            size: n as u32,
            idx: 0,
            lehmer: 0,
        });
    }

    // Create temp array with values from permutation_slice
    let mut temp = permutation_slice.to_vec();
    let mut permuted = Vec::new_with_capacity(n)?;

    // Iterate over the Lehmer code
    for (i, &idx) in code.iter().enumerate() {
        if idx as usize >= temp.len() {
            return Err(Error::InvalidPermutationLehmerCode {
                size: n as u32,
                idx: i as u32,
                lehmer: idx,
            });
        }

        // Assign temp[idx] to permuted vector
        permuted.push(temp.remove(idx as usize));
    }

    // Append any remaining elements from temp to permuted
    permuted.extend(temp);

    Ok(permuted)
}

fn get_context(x: u32) -> usize {
    (x + 1).ceil_log2().min(7) as usize
}

#[cfg(test)]
mod test {
    use super::*;
    use arbtest::arbitrary::{self, Arbitrary, Unstructured};
    use core::assert_eq;
    use test_log::test;

    #[test]
    fn generate_permutation_arbtest() {
        arbtest::arbtest(|u| {
            let input = PermutationInput::arbitrary(u)?;

            let permutation_slice = input.permutation.as_slice();

            let perm1 = decode_lehmer_code(&input.code, permutation_slice);
            let perm2 = decode_lehmer_code_naive(&input.code, permutation_slice);

            assert_eq!(
                perm1.map_err(|x| x.to_string()),
                perm2.map_err(|x| x.to_string())
            );
            Ok(())
        });
    }

    #[derive(Debug)]
    struct PermutationInput {
        code: Vec<u32>,
        permutation: Vec<u32>,
    }

    impl<'a> Arbitrary<'a> for PermutationInput {
        fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, arbitrary::Error> {
            // Generate a reasonable size to prevent tests from taking too long
            let size_lehmer = u.int_in_range(1..=1000)?;

            let mut lehmer: Vec<u32> = Vec::with_capacity(size_lehmer as usize);
            for i in 0..size_lehmer {
                let max_val = size_lehmer - i - 1;
                let val = if max_val > 0 {
                    u.int_in_range(0..=max_val)?
                } else {
                    0
                };
                lehmer.push(val);
            }

            let mut permutation = Vec::new();
            let size_permutation = u.int_in_range(size_lehmer..=1000)?;
            permutation.extend(0..size_permutation);

            let num_of_swaps = u.int_in_range(0..=100)?;
            for _ in 0..num_of_swaps {
                // Randomly swap two positions
                let pos1 = u.int_in_range(0..=size_permutation - 1)?;
                let pos2 = u.int_in_range(0..=size_permutation - 1)?;
                permutation.swap(pos1 as usize, pos2 as usize);
            }

            Ok(PermutationInput {
                code: lehmer,
                permutation,
            })
        }
    }

    #[test]
    fn simple() {
        // Lehmer code: [1, 1, 2, 3, 3, 6, 0, 1]
        let code = vec![1u32, 1, 2, 3, 3, 6, 0, 1];
        let skip = 4;
        let size = 16;

        let permutation_slice: Vec<u32> = (skip..size).collect();

        let permuted = decode_lehmer_code(&code, &permutation_slice).unwrap();
        let permuted_naive = decode_lehmer_code_naive(&code, &permutation_slice).unwrap();

        let mut permutation = Vec::with_capacity(size as usize);
        permutation.extend(0..skip); // Add skipped elements
        permutation.extend(permuted.iter());
        let expected_permutation = vec![0, 1, 2, 3, 5, 6, 8, 10, 11, 15, 4, 9, 7, 12, 13, 14];

        assert_eq!(permutation, expected_permutation);
        assert_eq!(permuted, permuted_naive);
    }

    #[test]
    fn decode_lehmer_compare_different_length() -> Result<(), Box<dyn std::error::Error>> {
        // Lehmer code: [1, 1, 2, 3, 3, 6, 0, 1]
        let code = vec![1u32, 1, 2, 3, 3, 6, 0, 1];
        let skip = 4;
        let size = 16;

        let permutation_slice: Vec<u32> = (skip..size).collect();

        let permuted_optimized = decode_lehmer_code(&code, &permutation_slice)?;
        let permuted_naive = decode_lehmer_code_naive(&code, &permutation_slice)?;

        let expected_permuted = vec![5u32, 6, 8, 10, 11, 15, 4, 9, 7, 12, 13, 14];

        assert_eq!(permuted_optimized, expected_permuted);
        assert_eq!(permuted_naive, expected_permuted);
        assert_eq!(permuted_optimized, permuted_naive);

        Ok(())
    }

    #[test]
    fn decode_lehmer_compare_same_length() -> Result<(), Box<dyn std::error::Error>> {
        // Lehmer code: [2, 3, 0, 0, 0]
        let code = vec![2u32, 3, 0, 0, 0];
        let n = code.len();
        let permutation_slice: Vec<u32> = (0..n as u32).collect();

        let permuted_optimized = decode_lehmer_code(&code, &permutation_slice)?;
        let permuted_naive = decode_lehmer_code_naive(&code, &permutation_slice)?;

        let expected_permutation = vec![2u32, 4, 0, 1, 3];

        assert_eq!(permuted_optimized, expected_permutation);
        assert_eq!(permuted_naive, expected_permutation);
        assert_eq!(permuted_optimized, permuted_naive);

        Ok(())
    }

    #[test]
    fn lehmer_out_of_bounds() {
        let code = vec![4];
        let permutation_slice: Vec<u32> = (4..8).collect();

        let result = decode_lehmer_code(&code, &permutation_slice);
        assert!(result.is_err());
    }
}
