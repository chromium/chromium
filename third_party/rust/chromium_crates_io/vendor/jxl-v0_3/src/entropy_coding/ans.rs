// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
//
// Originally written for jxl-oxide.

use crate::bit_reader::BitReader;
use crate::error::{Error, Result};

const LOG_SUM_PROBS: usize = 12;
const SUM_PROBS: u16 = 1 << LOG_SUM_PROBS;

const RLE_MARKER_SYM: u16 = LOG_SUM_PROBS as u16 + 1;

#[derive(Debug)]
struct AnsHistogram {
    buckets: Vec<Bucket>,
    log_bucket_size: usize,
    bucket_mask: u32,
    // For optimizing fast-lossless case.
    single_symbol: Option<u32>,
}

// log_alphabet_size <= 8 and log_bucket_size <= 7, so u8 is sufficient for symbols and cutoffs.
#[derive(Debug, Copy, Clone)]
#[repr(C)]
struct Bucket {
    alias_symbol: u8,
    alias_cutoff: u8,
    dist: u16,
    alias_offset: u16,
    alias_dist_xor: u16,
}

impl AnsHistogram {
    fn decode_dist_two_symbols(br: &mut BitReader, dist: &mut [u16]) -> Result<usize> {
        let table_size = dist.len();

        let v0 = Self::read_u8(br)? as usize;
        let v1 = Self::read_u8(br)? as usize;
        if v0 == v1 {
            return Err(Error::InvalidAnsHistogram);
        }

        let alphabet_size = v0.max(v1) + 1;
        if alphabet_size > table_size {
            return Err(Error::InvalidAnsHistogram);
        }

        let prob = br.read(LOG_SUM_PROBS)? as u16;
        dist[v0] = prob;
        dist[v1] = SUM_PROBS - prob;

        Ok(alphabet_size)
    }

    fn decode_dist_single_symbol(br: &mut BitReader, dist: &mut [u16]) -> Result<usize> {
        let table_size = dist.len();

        let val = Self::read_u8(br)? as usize;
        let alphabet_size = val + 1;
        if alphabet_size > table_size {
            return Err(Error::InvalidAnsHistogram);
        }

        dist[val] = SUM_PROBS;

        Ok(alphabet_size)
    }

    fn decode_dist_evenly_distributed(br: &mut BitReader, dist: &mut [u16]) -> Result<usize> {
        let table_size = dist.len();

        let alphabet_size = Self::read_u8(br)? as usize + 1;
        if alphabet_size > table_size {
            return Err(Error::InvalidAnsHistogram);
        }

        let base = SUM_PROBS as usize / alphabet_size;
        let remainder = SUM_PROBS as usize % alphabet_size;
        dist[0..remainder].fill(base as u16 + 1);
        dist[remainder..alphabet_size].fill(base as u16);

        Ok(alphabet_size)
    }

    fn decode_dist_complex(br: &mut BitReader, dist: &mut [u16]) -> Result<usize> {
        let table_size = dist.len();

        let mut len = 0usize;
        while len < 3 {
            if br.read(1)? != 0 {
                len += 1;
            } else {
                break;
            }
        }

        let shift = (br.read(len)? + (1 << len) - 1) as i16;
        if shift > 13 {
            return Err(Error::InvalidAnsHistogram);
        }

        let alphabet_size = Self::read_u8(br)? as usize + 3;
        if alphabet_size > table_size {
            return Err(Error::InvalidAnsHistogram);
        }

        // TODO(tirr-c): This could be an array of length `SUM_PROB / 4` (4 is from the minimum
        // value of `repeat_count`). Change if using array is faster.
        let mut repeat_ranges = Vec::new();
        let mut omit_data = None;
        let mut idx = 0;
        while idx < alphabet_size {
            dist[idx] = Self::read_prefix(br)?;
            if dist[idx] == RLE_MARKER_SYM {
                let repeat_count = Self::read_u8(br)? as usize + 4;
                if idx + repeat_count > alphabet_size {
                    return Err(Error::InvalidAnsHistogram);
                }
                repeat_ranges.push(idx..(idx + repeat_count));
                idx += repeat_count;
                continue;
            }
            match &mut omit_data {
                Some((log, pos)) => {
                    if dist[idx] > *log {
                        *log = dist[idx];
                        *pos = idx;
                    }
                }
                data => {
                    *data = Some((dist[idx], idx));
                }
            }
            idx += 1;
        }
        let Some((_, omit_pos)) = omit_data else {
            return Err(Error::InvalidAnsHistogram);
        };
        if dist.get(omit_pos + 1) == Some(&RLE_MARKER_SYM) {
            return Err(Error::InvalidAnsHistogram);
        }

        let mut repeat_range_idx = 0usize;
        let mut acc = 0;
        let mut prev_dist = 0u16;
        for (idx, code) in dist.iter_mut().enumerate() {
            if repeat_range_idx < repeat_ranges.len()
                && repeat_ranges[repeat_range_idx].start <= idx
            {
                if repeat_ranges[repeat_range_idx].end == idx {
                    repeat_range_idx += 1;
                } else {
                    *code = prev_dist;
                    acc += *code;
                    // dist[omit_pos] > 0
                    if acc >= SUM_PROBS {
                        return Err(Error::InvalidAnsHistogram);
                    }
                    continue;
                }
            }

            if *code == 0 {
                prev_dist = 0;
                continue;
            }
            if idx == omit_pos {
                prev_dist = 0;
                continue;
            }
            if *code > 1 {
                let zeros = (*code - 1) as i16;
                let bitcount = (shift - ((LOG_SUM_PROBS as i16 - zeros) >> 1)).clamp(0, zeros);
                *code = (1 << zeros) + ((br.read(bitcount as usize)? as u16) << (zeros - bitcount));
            }

            prev_dist = *code;
            acc += *code;
            // dist[omit_pos] > 0
            if acc >= SUM_PROBS {
                return Err(Error::InvalidAnsHistogram);
            }
        }
        dist[omit_pos] = SUM_PROBS - acc;

        Ok(alphabet_size)
    }

    fn build_alias_map(alphabet_size: usize, log_bucket_size: usize, dist: &[u16]) -> Vec<Bucket> {
        #[derive(Debug)]
        struct WorkingBucket {
            dist: u16,
            alias_symbol: u16,
            alias_offset: u16,
            alias_cutoff: u16,
        }

        let bucket_size = 1u16 << log_bucket_size;
        let mut buckets: Vec<_> = dist
            .iter()
            .enumerate()
            .map(|(i, &dist)| WorkingBucket {
                dist,
                alias_symbol: if i < alphabet_size { i as u16 } else { 0 },
                alias_offset: 0,
                alias_cutoff: dist,
            })
            .collect();

        let mut underfull = Vec::new();
        let mut overfull = Vec::new();
        for (idx, &WorkingBucket { dist, .. }) in buckets.iter().enumerate() {
            match dist.cmp(&bucket_size) {
                std::cmp::Ordering::Less => underfull.push(idx),
                std::cmp::Ordering::Equal => {}
                std::cmp::Ordering::Greater => overfull.push(idx),
            }
        }
        while let (Some(o), Some(u)) = (overfull.pop(), underfull.pop()) {
            let by = bucket_size - buckets[u].alias_cutoff;
            buckets[o].alias_cutoff -= by;
            buckets[u].alias_symbol = o as u16;
            buckets[u].alias_offset = buckets[o].alias_cutoff;
            match buckets[o].alias_cutoff.cmp(&bucket_size) {
                std::cmp::Ordering::Less => underfull.push(o),
                std::cmp::Ordering::Equal => {}
                std::cmp::Ordering::Greater => overfull.push(o),
            }
        }

        // Assertion failure happens only if `dist` doesn't sum to `SUM_PROB`, which is checked
        // before building alias map.
        assert!(overfull.is_empty() && underfull.is_empty());

        buckets
            .iter()
            .enumerate()
            .map(|(idx, bucket)| {
                if bucket.alias_cutoff == bucket_size {
                    Bucket {
                        dist: bucket.dist,
                        alias_symbol: idx as u8,
                        alias_offset: 0,
                        alias_cutoff: 0,
                        alias_dist_xor: 0,
                    }
                } else {
                    Bucket {
                        dist: bucket.dist,
                        alias_symbol: bucket.alias_symbol as u8,
                        alias_offset: bucket.alias_offset - bucket.alias_cutoff,
                        alias_cutoff: bucket.alias_cutoff as u8,
                        alias_dist_xor: bucket.dist ^ buckets[bucket.alias_symbol as usize].dist,
                    }
                }
            })
            .collect()
    }

    // log_alphabet_size: 5 + u(2)
    pub fn decode(br: &mut BitReader, log_alpha_size: usize) -> Result<Self> {
        debug_assert!((5..=8).contains(&log_alpha_size));
        let table_size = (1u16 << log_alpha_size) as usize;
        // 4 <= log_bucket_size <= 7
        let log_bucket_size = LOG_SUM_PROBS - log_alpha_size;
        let bucket_size = 1u16 << log_bucket_size;
        let bucket_mask = bucket_size as u32 - 1;

        let mut dist = vec![0u16; table_size];
        let alphabet_size = if br.read(1)? != 0 {
            if br.read(1)? != 0 {
                Self::decode_dist_two_symbols(br, &mut dist)?
            } else {
                Self::decode_dist_single_symbol(br, &mut dist)?
            }
        } else if br.read(1)? != 0 {
            Self::decode_dist_evenly_distributed(br, &mut dist)?
        } else {
            Self::decode_dist_complex(br, &mut dist)?
        };

        if let Some(single_sym_idx) = dist.iter().position(|&d| d == SUM_PROBS) {
            let buckets = dist
                .into_iter()
                .enumerate()
                .map(|(i, dist)| Bucket {
                    dist,
                    alias_symbol: single_sym_idx as u8,
                    alias_offset: bucket_size * i as u16,
                    alias_cutoff: 0,
                    alias_dist_xor: dist ^ SUM_PROBS,
                })
                .collect();
            return Ok(Self {
                buckets,
                log_bucket_size,
                bucket_mask,
                single_symbol: Some(single_sym_idx as u32),
            });
        }

        Ok(Self {
            buckets: Self::build_alias_map(alphabet_size, log_bucket_size, &dist),
            log_bucket_size,
            bucket_mask,
            single_symbol: None,
        })
    }

    fn read_u8(bitstream: &mut BitReader) -> Result<u8> {
        Ok(if bitstream.read(1)? != 0 {
            let n = bitstream.read(3)?;
            ((1 << n) + bitstream.read(n as usize)?) as u8
        } else {
            0
        })
    }

    fn read_prefix(br: &mut BitReader) -> Result<u16> {
        // Prefix code lookup table.
        #[rustfmt::skip]
        const TABLE: [(u8, u8); 128] = [
            (10, 3), (12, 7), (7, 3), (3, 4), (6, 3), (8, 3), (9, 3), (5, 4),
            (10, 3), ( 4, 4), (7, 3), (1, 4), (6, 3), (8, 3), (9, 3), (2, 4),
            (10, 3), ( 0, 5), (7, 3), (3, 4), (6, 3), (8, 3), (9, 3), (5, 4),
            (10, 3), ( 4, 4), (7, 3), (1, 4), (6, 3), (8, 3), (9, 3), (2, 4),
            (10, 3), (11, 6), (7, 3), (3, 4), (6, 3), (8, 3), (9, 3), (5, 4),
            (10, 3), ( 4, 4), (7, 3), (1, 4), (6, 3), (8, 3), (9, 3), (2, 4),
            (10, 3), ( 0, 5), (7, 3), (3, 4), (6, 3), (8, 3), (9, 3), (5, 4),
            (10, 3), ( 4, 4), (7, 3), (1, 4), (6, 3), (8, 3), (9, 3), (2, 4),
            (10, 3), (13, 7), (7, 3), (3, 4), (6, 3), (8, 3), (9, 3), (5, 4),
            (10, 3), ( 4, 4), (7, 3), (1, 4), (6, 3), (8, 3), (9, 3), (2, 4),
            (10, 3), ( 0, 5), (7, 3), (3, 4), (6, 3), (8, 3), (9, 3), (5, 4),
            (10, 3), ( 4, 4), (7, 3), (1, 4), (6, 3), (8, 3), (9, 3), (2, 4),
            (10, 3), (11, 6), (7, 3), (3, 4), (6, 3), (8, 3), (9, 3), (5, 4),
            (10, 3), ( 4, 4), (7, 3), (1, 4), (6, 3), (8, 3), (9, 3), (2, 4),
            (10, 3), ( 0, 5), (7, 3), (3, 4), (6, 3), (8, 3), (9, 3), (5, 4),
            (10, 3), ( 4, 4), (7, 3), (1, 4), (6, 3), (8, 3), (9, 3), (2, 4),
        ];

        let index = br.peek(7);
        let (sym, bits) = TABLE[index as usize];
        br.consume(bits as usize)?;
        Ok(sym as u16)
    }
}

impl AnsHistogram {
    #[inline]
    pub fn read(&self, br: &mut BitReader, state: &mut u32) -> u32 {
        let idx = *state & 0xfff;
        let i = (idx >> self.log_bucket_size) as usize;
        let pos = idx & self.bucket_mask;

        debug_assert!(self.buckets.len().is_power_of_two());
        let bucket = self.buckets[i & (self.buckets.len() - 1)];
        let alias_symbol = bucket.alias_symbol as u32;
        let alias_cutoff = bucket.alias_cutoff as u32;
        let dist = bucket.dist as u32;

        let map_to_alias = (pos >= alias_cutoff) as u32;
        let offset = (bucket.alias_offset as u32) * map_to_alias;
        let dist_xor = (bucket.alias_dist_xor as u32) * map_to_alias;

        let dist = dist ^ dist_xor;
        let symbol = (alias_symbol * map_to_alias) | (i as u32 * (1 - map_to_alias));
        let offset = offset + pos;

        let next_state = (*state >> LOG_SUM_PROBS) * dist + offset;
        let select_appended = (next_state < (1 << 16)) as u32;
        let appended_state = (next_state << 16) | (br.peek(16) as u32);
        *state = (appended_state * select_appended) | (next_state * (1 - select_appended));
        br.consume_optimistic((16 * select_appended) as usize);
        symbol
    }

    // For optimizing fast-lossless case.
    #[inline]
    pub fn single_symbol(&self) -> Option<u32> {
        self.single_symbol
    }
}

#[derive(Debug)]
pub struct AnsCodes {
    histograms: Vec<AnsHistogram>,
}

impl AnsCodes {
    pub fn decode(num: usize, log_alpha_size: usize, br: &mut BitReader) -> Result<AnsCodes> {
        let histograms = (0..num)
            .map(|_| AnsHistogram::decode(br, log_alpha_size))
            .collect::<Result<_>>()?;
        Ok(Self { histograms })
    }

    pub fn single_symbol(&self, ctx: usize) -> Option<u32> {
        self.histograms[ctx].single_symbol()
    }
}

#[derive(Debug)]
pub struct AnsReader(u32);

impl AnsReader {
    /// Expected final ANS state.
    const CHECKSUM: u32 = 0x130000;

    pub fn new_unused() -> Self {
        Self(Self::CHECKSUM)
    }

    pub fn init(br: &mut BitReader) -> Result<Self> {
        let initial_state = br.read(32)? as u32;
        Ok(Self(initial_state))
    }

    #[inline]
    pub fn read(&mut self, codes: &AnsCodes, br: &mut BitReader, ctx: usize) -> u32 {
        codes.histograms[ctx].read(br, &mut self.0)
    }

    pub fn check_final_state(self) -> Result<()> {
        if self.0 == Self::CHECKSUM {
            Ok(())
        } else {
            Err(Error::AnsChecksumMismatch)
        }
    }

    pub(super) fn checkpoint(&self) -> Self {
        Self(self.0)
    }
}

#[cfg(test)]
mod test {
    use super::*;

    fn validate_buckets(buckets: &[Bucket]) {
        let dist_sum: u16 = buckets.iter().map(|bucket| bucket.dist).sum();
        assert_eq!(dist_sum, SUM_PROBS);
    }

    #[test]
    fn single_symbol() {
        // Single symbol of 20
        let mut br = BitReader::new(&[0b00100101, 0b01]);
        let histogram = AnsHistogram::decode(&mut br, 5).unwrap();
        validate_buckets(&histogram.buckets);
        assert_eq!(histogram.buckets[20].dist, SUM_PROBS);
        assert_eq!(histogram.single_symbol, Some(20));

        // Single symbol of 32 (invalid)
        let mut br = BitReader::new(&[0b00101101, 0b000]);
        assert!(AnsHistogram::decode(&mut br, 5).is_err());
    }

    #[test]
    fn two_symbols() {
        // two symbols of 10 and 20, where the prob of symbol 10 is 256
        let mut br = BitReader::new(&[0b10011111, 0b10010010, 0b00000000, 0b00010]);
        let histogram = AnsHistogram::decode(&mut br, 5).unwrap();
        validate_buckets(&histogram.buckets);
        assert_eq!(histogram.buckets[10].dist, 256);
        assert_eq!(histogram.buckets[20].dist, SUM_PROBS - 256);
    }

    #[test]
    fn decode_arb() {
        arbtest::arbtest(|u| {
            let total_len = u.arbitrary_len::<u8>()?;
            let mut buf = vec![0u8; total_len];
            u.fill_buffer(&mut buf)?;
            let mut br = BitReader::new(&buf);

            loop {
                match AnsHistogram::decode(&mut br, 8) {
                    Ok(histogram) => validate_buckets(&histogram.buckets),
                    Err(Error::OutOfBounds(_)) => break,
                    Err(_) => {}
                }
            }

            Ok(())
        });
    }
}
