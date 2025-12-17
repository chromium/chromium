// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use jxl_macros::UnconditionalCoder;

use crate::bit_reader::BitReader;
use crate::entropy_coding::ans::*;
use crate::entropy_coding::context_map::*;
use crate::entropy_coding::huffman::*;
use crate::entropy_coding::hybrid_uint::*;
use crate::error::{Error, Result};
use crate::headers::encodings::*;
use crate::util::tracing_wrappers::*;

pub fn decode_varint16(br: &mut BitReader) -> Result<u16> {
    if br.read(1)? != 0 {
        let nbits = br.read(4)? as usize;
        if nbits == 0 {
            Ok(1)
        } else {
            Ok((1 << nbits) + br.read(nbits)? as u16)
        }
    } else {
        Ok(0)
    }
}

pub fn unpack_signed(unsigned: u32) -> i32 {
    ((unsigned >> 1) ^ ((!unsigned) & 1).wrapping_sub(1)) as i32
}

#[derive(UnconditionalCoder, Debug)]
struct Lz77Params {
    pub enabled: bool,
    #[condition(enabled)]
    #[coder(u2S(224, 512, 4096, Bits(15) + 8))]
    pub min_symbol: Option<u32>,
    #[condition(enabled)]
    #[coder(u2S(3, 4, Bits(2) + 5, Bits(8) + 9))]
    pub min_length: Option<u32>,
}

#[derive(Debug)]
enum Codes {
    Huffman(HuffmanCodes),
    Ans(AnsCodes),
}

impl Codes {
    fn single_symbol(&self, ctx: usize) -> Option<u32> {
        match self {
            Self::Huffman(hc) => hc.single_symbol(ctx),
            Self::Ans(ans) => ans.single_symbol(ctx),
        }
    }
}

#[derive(Debug)]
pub struct Histograms {
    lz77_params: Lz77Params,
    lz77_length_uint: Option<HybridUint>,
    context_map: Vec<u8>,
    // TODO(veluca): figure out why this is unused.
    #[allow(dead_code)]
    log_alpha_size: usize,
    uint_configs: Vec<HybridUint>,
    codes: Codes,
}

#[derive(Debug)]
pub struct Lz77State {
    min_symbol: u32,
    min_length: u32,
    dist_multiplier: u32,
    window: Vec<u32>,
    num_to_copy: u32,
    copy_pos: u32,
    num_decoded: u32,
}

impl Lz77State {
    const LOG_WINDOW_SIZE: u32 = 20;
    const WINDOW_MASK: u32 = (1 << Self::LOG_WINDOW_SIZE) - 1;

    #[rustfmt::skip]
    const SPECIAL_DISTANCES: [(i8, u8); 120] = [
        ( 0, 1), ( 1, 0), ( 1, 1), (-1, 1), ( 0, 2), ( 2, 0), ( 1, 2), (-1, 2), ( 2, 1), (-2, 1),
        ( 2, 2), (-2, 2), ( 0, 3), ( 3, 0), ( 1, 3), (-1, 3), ( 3, 1), (-3, 1), ( 2, 3), (-2, 3),
        ( 3, 2), (-3, 2), ( 0, 4), ( 4, 0), ( 1, 4), (-1, 4), ( 4, 1), (-4, 1), ( 3, 3), (-3, 3),
        ( 2, 4), (-2, 4), ( 4, 2), (-4, 2), ( 0, 5), ( 3, 4), (-3, 4), ( 4, 3), (-4, 3), ( 5, 0),
        ( 1, 5), (-1, 5), ( 5, 1), (-5, 1), ( 2, 5), (-2, 5), ( 5, 2), (-5, 2), ( 4, 4), (-4, 4),
        ( 3, 5), (-3, 5), ( 5, 3), (-5, 3), ( 0, 6), ( 6, 0), ( 1, 6), (-1, 6), ( 6, 1), (-6, 1),
        ( 2, 6), (-2, 6), ( 6, 2), (-6, 2), ( 4, 5), (-4, 5), ( 5, 4), (-5, 4), ( 3, 6), (-3, 6),
        ( 6, 3), (-6, 3), ( 0, 7), ( 7, 0), ( 1, 7), (-1, 7), ( 5, 5), (-5, 5), ( 7, 1), (-7, 1),
        ( 4, 6), (-4, 6), ( 6, 4), (-6, 4), ( 2, 7), (-2, 7), ( 7, 2), (-7, 2), ( 3, 7), (-3, 7),
        ( 7, 3), (-7, 3), ( 5, 6), (-5, 6), ( 6, 5), (-6, 5), ( 8, 0), ( 4, 7), (-4, 7), ( 7, 4),
        (-7, 4), ( 8, 1), ( 8, 2), ( 6, 6), (-6, 6), ( 8, 3), ( 5, 7), (-5, 7), ( 7, 5), (-7, 5),
        ( 8, 4), ( 6, 7), (-6, 7), ( 7, 6), (-7, 6), ( 8, 5), ( 7, 7), (-7, 7), ( 8, 6), ( 8, 7),
    ];

    #[inline]
    fn apply_copy(&mut self, distance_sym: u32, num_to_copy: u32) {
        let distance_sub_1 = if self.dist_multiplier == 0 {
            distance_sym
        } else if let Some(distance) = distance_sym.checked_sub(120) {
            distance
        } else {
            let (offset, dist) = Lz77State::SPECIAL_DISTANCES[distance_sym as usize];
            let dist = (self.dist_multiplier * dist as u32).checked_add_signed(offset as i32 - 1);
            dist.unwrap_or(0)
        };

        let distance = (((1 << 20) - 1).min(distance_sub_1) + 1).min(self.num_decoded);
        self.copy_pos = self.num_decoded - distance;
        self.num_to_copy = num_to_copy;
    }

    #[inline]
    fn push_decoded_symbol(&mut self, token: u32) {
        let offset = (self.num_decoded & Self::WINDOW_MASK) as usize;
        if let Some(slot) = self.window.get_mut(offset) {
            *slot = token;
        } else {
            debug_assert_eq!(self.window.len(), offset);
            self.window.push(token);
        }
        self.num_decoded += 1;
    }

    #[inline]
    fn pull_symbol(&mut self) -> Option<u32> {
        if let Some(next_num_to_copy) = self.num_to_copy.checked_sub(1) {
            let sym = self.window[(self.copy_pos & Self::WINDOW_MASK) as usize];
            self.copy_pos += 1;
            self.num_to_copy = next_num_to_copy;
            Some(sym)
        } else {
            None
        }
    }
}

#[derive(Debug)]
struct RleState {
    min_symbol: u32,
    min_length: u32,
    last_sym: Option<u32>,
    repeat_count: u32,
}

impl RleState {
    #[inline]
    fn push_token(
        &mut self,
        token: u32,
        histograms: &Histograms,
        br: &mut BitReader,
        cluster: usize,
    ) {
        if let Some(token) = token.checked_sub(self.min_symbol) {
            let lz_length_conf = histograms.lz77_length_uint.as_ref().unwrap();
            let count = lz_length_conf.read(token, br);
            // If this calculation overflows, the bitstream is invalid (it would be rejected
            // on the LZ77 path), but we don't report an error.
            self.repeat_count = count.wrapping_add(self.min_length);
        } else {
            let sym = histograms.uint_configs[cluster].read(token, br);
            self.last_sym = Some(sym);
            self.repeat_count = 1;
        }
    }

    #[inline]
    fn pull_symbol(&mut self) -> Option<u32> {
        if self.repeat_count > 0 {
            self.repeat_count -= 1;
            self.last_sym
        } else {
            None
        }
    }
}

#[derive(Debug)]
enum SymbolReaderState {
    None,
    Lz77(Lz77State),
    Rle(RleState),
}

#[derive(Debug, Clone, Default)]
struct ErrorState {
    lz77_repeat: bool,
    arithmetic_overflow: bool,
}

impl ErrorState {
    fn new() -> Self {
        Self::default()
    }

    fn check_for_error(&self) -> Result<()> {
        if self.lz77_repeat {
            Err(Error::UnexpectedLz77Repeat)
        } else if self.arithmetic_overflow {
            Err(Error::ArithmeticOverflow)
        } else {
            Ok(())
        }
    }
}

#[derive(Debug)]
pub struct SymbolReader {
    state: SymbolReaderState,
    ans_reader: AnsReader,
    errors: ErrorState,
}

impl SymbolReader {
    pub fn new(
        histograms: &Histograms,
        br: &mut BitReader,
        image_width: Option<usize>,
    ) -> Result<Self> {
        let ans_reader = if matches!(histograms.codes, Codes::Ans(_)) {
            AnsReader::init(br)?
        } else {
            AnsReader::new_unused()
        };

        let Lz77Params {
            enabled: lz77_enabled,
            min_symbol,
            min_length,
        } = histograms.lz77_params;

        let state = if lz77_enabled {
            let min_symbol = min_symbol.unwrap();
            let min_length = min_length.unwrap();
            let dist_multiplier = image_width.unwrap_or(0) as u32;

            let lz_dist_cluster = *histograms.context_map.last().unwrap() as usize;
            let lz_conf = &histograms.uint_configs[lz_dist_cluster];
            let is_rle = histograms.codes.single_symbol(lz_dist_cluster) == Some(1)
                && lz_conf.is_split_exponent_zero();

            if is_rle {
                SymbolReaderState::Rle(RleState {
                    min_symbol,
                    min_length,
                    last_sym: None,
                    repeat_count: 0,
                })
            } else {
                SymbolReaderState::Lz77(Lz77State {
                    min_symbol,
                    min_length,
                    dist_multiplier,
                    window: Vec::new(),
                    num_to_copy: 0,
                    copy_pos: 0,
                    num_decoded: 0,
                })
            }
        } else {
            SymbolReaderState::None
        };

        Ok(Self {
            state,
            ans_reader,
            errors: ErrorState::new(),
        })
    }
}

impl SymbolReader {
    #[inline]
    pub fn read_unsigned(
        &mut self,
        histograms: &Histograms,
        br: &mut BitReader,
        context: usize,
    ) -> u32 {
        let cluster = histograms.map_context_to_cluster(context);
        self.read_unsigned_clustered(histograms, br, cluster)
    }

    #[inline(always)]
    pub fn read_signed(
        &mut self,
        histograms: &Histograms,
        br: &mut BitReader,
        context: usize,
    ) -> i32 {
        let unsigned = self.read_unsigned(histograms, br, context);
        unpack_signed(unsigned)
    }

    #[inline]
    pub fn read_unsigned_clustered(
        &mut self,
        histograms: &Histograms,
        br: &mut BitReader,
        cluster: usize,
    ) -> u32 {
        match &mut self.state {
            SymbolReaderState::None => {
                let token = match &histograms.codes {
                    Codes::Huffman(hc) => hc.read(br, cluster),
                    Codes::Ans(ans) => self.ans_reader.read(ans, br, cluster),
                };
                histograms.uint_configs[cluster].read(token, br)
            }

            SymbolReaderState::Lz77(lz77_state) => {
                if let Some(sym) = lz77_state.pull_symbol() {
                    lz77_state.push_decoded_symbol(sym);
                    return sym;
                }
                let token = match &histograms.codes {
                    Codes::Huffman(hc) => hc.read(br, cluster),
                    Codes::Ans(ans) => self.ans_reader.read(ans, br, cluster),
                };
                let Some(lz77_token) = token.checked_sub(lz77_state.min_symbol) else {
                    let sym = histograms.uint_configs[cluster].read(token, br);
                    lz77_state.push_decoded_symbol(sym);
                    return sym;
                };
                if lz77_state.num_decoded == 0 {
                    self.errors.lz77_repeat = true;
                    return 0;
                }

                let num_to_copy = histograms
                    .lz77_length_uint
                    .as_ref()
                    .unwrap()
                    .read(lz77_token, br);
                let Some(num_to_copy) = num_to_copy.checked_add(lz77_state.min_length) else {
                    warn!(
                        num_to_copy,
                        lz77_state.min_length, "LZ77 num_to_copy overflow"
                    );
                    self.errors.arithmetic_overflow = true;
                    return 0;
                };

                let lz_dist_cluster = *histograms.context_map.last().unwrap() as usize;
                let distance_sym = match &histograms.codes {
                    Codes::Huffman(hc) => hc.read(br, lz_dist_cluster),
                    Codes::Ans(ans) => self.ans_reader.read(ans, br, lz_dist_cluster),
                };
                let distance_sym = histograms.uint_configs[lz_dist_cluster].read(distance_sym, br);
                lz77_state.apply_copy(distance_sym, num_to_copy);

                let sym = lz77_state.pull_symbol().unwrap();
                lz77_state.push_decoded_symbol(sym);
                sym
            }

            SymbolReaderState::Rle(rle_state) => {
                if let Some(sym) = rle_state.pull_symbol() {
                    return sym;
                }

                let token = match &histograms.codes {
                    Codes::Huffman(hc) => hc.read(br, cluster),
                    Codes::Ans(ans) => self.ans_reader.read(ans, br, cluster),
                };
                rle_state.push_token(token, histograms, br, cluster);
                if let Some(sym) = rle_state.pull_symbol() {
                    sym
                } else {
                    self.errors.lz77_repeat = true;
                    0
                }
            }
        }
    }

    #[inline(always)]
    pub fn read_signed_clustered(
        &mut self,
        histograms: &Histograms,
        br: &mut BitReader,
        cluster: usize,
    ) -> i32 {
        let unsigned = self.read_unsigned_clustered(histograms, br, cluster);
        unpack_signed(unsigned)
    }

    pub fn check_final_state(self, histograms: &Histograms, br: &mut BitReader) -> Result<()> {
        self.errors.check_for_error()?;
        br.check_for_error()?;
        match &histograms.codes {
            Codes::Huffman(_) => Ok(()),
            Codes::Ans(_) => self.ans_reader.check_final_state(),
        }
    }

    pub fn checkpoint<const N: usize>(&self) -> Checkpoint<N> {
        let state = match &self.state {
            SymbolReaderState::None => StateCheckpoint::None,
            SymbolReaderState::Lz77(lz77_state) => {
                let mut window = [0u32; N];
                let start = (lz77_state.num_decoded & Lz77State::WINDOW_MASK) as usize;
                let end = ((lz77_state.num_decoded + N as u32) & Lz77State::WINDOW_MASK) as usize;
                if start < end {
                    let window_first = &lz77_state.window[start..];
                    let actual_size = window_first.len().min(N);
                    window[..actual_size].copy_from_slice(&window_first[..actual_size]);
                } else {
                    let window_first = &lz77_state.window[start..];
                    let first_len = window_first
                        .len()
                        .min((1 << Lz77State::LOG_WINDOW_SIZE) - start);
                    window[..first_len].copy_from_slice(&window_first[..first_len]);
                    window[N - end..].copy_from_slice(&lz77_state.window[..end]);
                }
                StateCheckpoint::Lz77 {
                    num_to_copy: lz77_state.num_to_copy,
                    copy_pos: lz77_state.copy_pos,
                    num_decoded: lz77_state.num_decoded,
                    window,
                }
            }
            SymbolReaderState::Rle(rle_state) => StateCheckpoint::Rle {
                last_sym: rle_state.last_sym,
                repeat_count: rle_state.repeat_count,
            },
        };

        Checkpoint {
            state,
            ans_reader: self.ans_reader.checkpoint(),
            errors: self.errors.clone(),
        }
    }

    pub fn restore<const N: usize>(&mut self, checkpoint: Checkpoint<N>) {
        match checkpoint.state {
            StateCheckpoint::None => {
                if !matches!(self.state, SymbolReaderState::None) {
                    panic!("checkpoint type mismatch");
                }
            }
            StateCheckpoint::Lz77 {
                num_to_copy,
                copy_pos,
                num_decoded,
                window,
            } => {
                let SymbolReaderState::Lz77(lz77_state) = &mut self.state else {
                    panic!("checkpoint type mismatch");
                };

                let num_rewind = lz77_state.num_decoded - num_decoded;
                let rewind_window = &window[..num_rewind as usize];

                let start = (num_decoded & Lz77State::WINDOW_MASK) as usize;
                let end = ((num_decoded + num_rewind) & Lz77State::WINDOW_MASK) as usize;
                if start < end {
                    lz77_state.window[start..end].copy_from_slice(rewind_window);
                } else {
                    let window_first = &mut lz77_state.window[start..];
                    let first_len = window_first.len();
                    window_first.copy_from_slice(&rewind_window[..first_len]);
                    lz77_state.window[..end].copy_from_slice(&rewind_window[first_len..]);
                }

                lz77_state.num_to_copy = num_to_copy;
                lz77_state.copy_pos = copy_pos;
                lz77_state.num_decoded = num_decoded;
            }
            StateCheckpoint::Rle {
                last_sym,
                repeat_count,
            } => {
                let SymbolReaderState::Rle(rle_state) = &mut self.state else {
                    panic!("checkpoint type mismatch");
                };

                rle_state.last_sym = last_sym;
                rle_state.repeat_count = repeat_count;
            }
        }

        self.ans_reader = checkpoint.ans_reader;
        self.errors = checkpoint.errors;
    }
}

impl Histograms {
    pub fn decode(num_contexts: usize, br: &mut BitReader, allow_lz77: bool) -> Result<Histograms> {
        let lz77_params = Lz77Params::read_unconditional(&(), br, &Empty {})?;
        if !allow_lz77 && lz77_params.enabled {
            return Err(Error::Lz77Disallowed);
        }
        let (num_contexts, lz77_length_uint) = if lz77_params.enabled {
            (
                num_contexts + 1,
                Some(HybridUint::decode(/*log_alpha_size=*/ 8, br)?),
            )
        } else {
            (num_contexts, None)
        };

        let context_map = if num_contexts > 1 {
            decode_context_map(num_contexts, br)?
        } else {
            vec![0]
        };
        assert_eq!(context_map.len(), num_contexts);

        let use_prefix_code = br.read(1)? != 0;
        let log_alpha_size = if use_prefix_code {
            HUFFMAN_MAX_BITS
        } else {
            br.read(2)? as usize + 5
        };
        let num_histograms = *context_map.iter().max().unwrap() + 1;
        let uint_configs = ((0..num_histograms).map(|_| HybridUint::decode(log_alpha_size, br)))
            .collect::<Result<_>>()?;

        let codes = if use_prefix_code {
            Codes::Huffman(HuffmanCodes::decode(num_histograms as usize, br)?)
        } else {
            Codes::Ans(AnsCodes::decode(
                num_histograms as usize,
                log_alpha_size,
                br,
            )?)
        };

        Ok(Histograms {
            lz77_params,
            lz77_length_uint,
            context_map,
            log_alpha_size,
            uint_configs,
            codes,
        })
    }

    pub fn map_context_to_cluster(&self, context: usize) -> usize {
        self.context_map[context] as usize
    }

    pub fn num_histograms(&self) -> usize {
        *self.context_map.iter().max().unwrap() as usize + 1
    }
}

#[cfg(test)]
impl Histograms {
    /// Builds a decoder that reads an octet at a time and emits its bit-reversed value.
    pub fn reverse_octet(num_contexts: usize) -> Self {
        let d = HuffmanCodes::byte_histogram();
        let codes = Codes::Huffman(d);
        let uint_configs = vec![HybridUint::new(8, 0, 0)];
        Self {
            lz77_params: Lz77Params {
                enabled: false,
                min_symbol: None,
                min_length: None,
            },
            lz77_length_uint: None,
            uint_configs,
            log_alpha_size: 15,
            context_map: vec![0u8; num_contexts],
            codes,
        }
    }

    pub fn rle(num_contexts: usize, min_symbol: u32, min_length: u32) -> Self {
        let d = HuffmanCodes::byte_histogram_rle();
        let codes = Codes::Huffman(d);
        let uint_configs = vec![HybridUint::new(8, 0, 0), HybridUint::new(0, 0, 0)];
        let mut context_map = vec![0u8; num_contexts + 1];
        *context_map.last_mut().unwrap() = 1;
        Self {
            lz77_params: Lz77Params {
                enabled: true,
                min_symbol: Some(min_symbol),
                min_length: Some(min_length),
            },
            lz77_length_uint: Some(HybridUint::new(8, 0, 0)),
            uint_configs,
            log_alpha_size: 15,
            context_map,
            codes,
        }
    }
}

#[derive(Debug)]
enum StateCheckpoint<const N: usize> {
    None,
    Lz77 {
        num_to_copy: u32,
        copy_pos: u32,
        num_decoded: u32,
        window: [u32; N],
    },
    Rle {
        last_sym: Option<u32>,
        repeat_count: u32,
    },
}

#[derive(Debug)]
pub struct Checkpoint<const N: usize> {
    state: StateCheckpoint<N>,
    ans_reader: AnsReader,
    errors: ErrorState,
}

#[cfg(test)]
mod test {
    use std::ops::ControlFlow;

    use test_log::test;

    use super::*;

    #[test]
    fn rle_arb() {
        let histograms = Histograms::rle(1, 240, 3);

        arbtest::arbtest(|u| {
            let width = u.int_in_range(1usize..=256)?;

            let mut bitstream = Vec::new();
            let mut expected_bytes = Vec::new();
            u.arbitrary_loop(None, None, |u| {
                let do_repeat = !expected_bytes.is_empty() && u.ratio(1, 4)?;
                let range = if do_repeat { 240u8..=255 } else { 0u8..=239 };
                let byte = u.int_in_range(range)?;
                bitstream.push(byte);

                if do_repeat {
                    let count = byte as usize - 237;
                    let sym = *expected_bytes.last().unwrap();
                    for _ in 0..count {
                        expected_bytes.push(sym);
                    }
                } else {
                    expected_bytes.push(byte);
                }

                Ok(if expected_bytes.len() >= 256 {
                    ControlFlow::Break(())
                } else {
                    ControlFlow::Continue(())
                })
            })?;
            for b in &mut bitstream {
                *b = b.reverse_bits();
            }

            // Read RLE
            let mut br = BitReader::new(&bitstream);
            let mut reader = SymbolReader::new(&histograms, &mut br, Some(width)).unwrap();

            for expected in expected_bytes {
                let actual = reader.read_unsigned_clustered(&histograms, &mut br, 0);
                assert_eq!(actual, expected as u32);
            }

            let SymbolReaderState::Rle(rle_state) = &reader.state else {
                panic!()
            };
            assert_eq!(rle_state.repeat_count, 0);
            assert!(reader.check_final_state(&histograms, &mut br).is_ok());
            assert_eq!(br.total_bits_available(), 0);

            Ok(())
        });
    }
}
