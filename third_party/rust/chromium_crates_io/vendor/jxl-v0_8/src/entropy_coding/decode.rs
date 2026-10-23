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

pub trait ClusteredRowSample: Copy {
    fn from_i32(v: i32) -> Self;
}

impl ClusteredRowSample for i32 {
    #[inline(always)]
    fn from_i32(v: i32) -> Self {
        v
    }
}

impl ClusteredRowSample for i16 {
    #[inline(always)]
    fn from_i32(v: i32) -> Self {
        v as i16
    }
}

#[derive(UnconditionalCoder, Debug, Clone, Copy)]
pub struct Lz77Params {
    pub enabled: bool,
    #[condition(enabled)]
    #[coder(u2S(224, 512, 4096, Bits(15) + 8))]
    pub min_symbol: Option<u32>,
    #[condition(enabled)]
    #[coder(u2S(3, 4, Bits(2) + 5, Bits(8) + 9))]
    pub min_length: Option<u32>,
}

#[derive(Debug)]
pub enum Codes {
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

    fn max_symbol_for_cluster(&self, cluster: usize) -> u32 {
        match self {
            Self::Huffman(hc) => hc.max_symbol_for_cluster(cluster),
            Self::Ans(ans) => ans.max_symbol_for_cluster(cluster),
        }
    }
}

#[derive(Debug)]
pub struct Histograms {
    lz77_params: Lz77Params,
    lz77_length_uint: Option<HybridUint>,
    context_map: Vec<u8>,
    /// Explicitly stored: context_map can get padded so we cannot use .last()
    lz_dist_cluster: u8,
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
            let dist = self
                .dist_multiplier
                .checked_mul(dist as u32)
                .and_then(|d| d.checked_add_signed(offset as i32 - 1));
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
    fn push_decoded_symbol_run(&mut self, token: u32, count: usize) {
        const WINDOW_SIZE: usize = 1 << Lz77State::LOG_WINDOW_SIZE;
        let start = if count < WINDOW_SIZE {
            (self.num_decoded & Self::WINDOW_MASK) as usize
        } else {
            0
        };
        let end = start + count;
        let clamped_end = end.min(WINDOW_SIZE);
        if self.window.len() < clamped_end {
            self.window[start..].fill(token);
            self.window.resize(clamped_end, token);
        } else {
            self.window[start..clamped_end].fill(token);
        }
        if end > WINDOW_SIZE && count < WINDOW_SIZE {
            self.window[..end - WINDOW_SIZE].fill(token);
        }
        self.copy_pos = self.copy_pos.wrapping_add(count as u32);
        self.num_decoded = self.num_decoded.wrapping_add(count as u32);
        self.num_to_copy -= count as u32;
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
enum SymbolReaderState {
    None,
    Lz77(Lz77State),
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
        lz77_scratch: &mut Vec<u32>,
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
            let mut window = std::mem::take(lz77_scratch);
            window.clear();

            SymbolReaderState::Lz77(Lz77State {
                min_symbol,
                min_length,
                dist_multiplier,
                window,
                num_to_copy: 0,
                copy_pos: 0,
                num_decoded: 0,
            })
        } else {
            SymbolReaderState::None
        };

        Ok(Self {
            state,
            ans_reader,
            errors: ErrorState::new(),
        })
    }

    pub fn reclaim_lz77_scratch(&mut self, lz77_scratch: &mut Vec<u32>) {
        if let SymbolReaderState::Lz77(lz77_state) = &mut self.state {
            lz77_state.window.clear();
            *lz77_scratch = std::mem::take(&mut lz77_state.window);
        }
    }
}

impl SymbolReader {
    #[inline(always)]
    pub fn has_lz77(&self) -> bool {
        matches!(self.state, SymbolReaderState::Lz77(_))
    }

    #[inline(always)]
    pub fn read_token_ans_no_lz77(
        &mut self,
        ans: &AnsCodes,
        br: &mut BitReader,
        cluster: usize,
    ) -> u32 {
        debug_assert!(matches!(self.state, SymbolReaderState::None));
        self.ans_reader.read(ans, br, cluster)
    }

    #[inline(always)]
    pub fn read_token_huffman_no_lz77(
        &mut self,
        hc: &HuffmanCodes,
        br: &mut BitReader,
        cluster: usize,
    ) -> u32 {
        debug_assert!(matches!(self.state, SymbolReaderState::None));
        hc.read(br, cluster)
    }

    #[inline(always)]
    pub fn read_unsigned_inline(
        &mut self,
        histograms: &Histograms,
        br: &mut BitReader,
        context: usize,
    ) -> u32 {
        let cluster = histograms.map_context_to_cluster(context);
        self.read_unsigned_clustered_inline(histograms, br, cluster)
    }

    #[inline(never)]
    pub fn read_unsigned(
        &mut self,
        histograms: &Histograms,
        br: &mut BitReader,
        context: usize,
    ) -> u32 {
        self.read_unsigned_inline(histograms, br, context)
    }

    #[inline(always)]
    pub fn read_signed_inline(
        &mut self,
        histograms: &Histograms,
        br: &mut BitReader,
        context: usize,
    ) -> i32 {
        let unsigned = self.read_unsigned_inline(histograms, br, context);
        unpack_signed(unsigned)
    }

    #[inline(never)]
    pub fn read_signed(
        &mut self,
        histograms: &Histograms,
        br: &mut BitReader,
        context: usize,
    ) -> i32 {
        self.read_signed_inline(histograms, br, context)
    }

    #[inline(always)]
    pub fn read_unsigned_clustered_inline(
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

                let lz_dist_cluster = histograms.lz_dist_cluster as usize;
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
        }
    }

    #[inline(always)]
    pub fn read_signed_clustered_inline(
        &mut self,
        histograms: &Histograms,
        br: &mut BitReader,
        cluster: usize,
    ) -> i32 {
        let unsigned = self.read_unsigned_clustered_inline(histograms, br, cluster);
        unpack_signed(unsigned)
    }

    pub fn read_signed_clustered_row<T: ClusteredRowSample>(
        &mut self,
        histograms: &Histograms,
        br: &mut BitReader,
        cluster: usize,
        row: &mut [T],
    ) {
        let uint_config = &histograms.uint_configs[cluster];
        match &mut self.state {
            SymbolReaderState::None => match (&histograms.codes, uint_config.is_config_420()) {
                (Codes::Huffman(hc), true) => {
                    let table = hc.table(cluster);
                    for r in row.iter_mut() {
                        let token = table.read(br);
                        *r = T::from_i32(unpack_signed(HybridUint::read_config_420(token, br)));
                    }
                }
                (Codes::Huffman(hc), false) => {
                    let table = hc.table(cluster);
                    for r in row.iter_mut() {
                        let token = table.read(br);
                        *r = T::from_i32(unpack_signed(uint_config.read(token, br)));
                    }
                }
                (Codes::Ans(ans), true) => {
                    for r in row.iter_mut() {
                        let token = self.ans_reader.read(ans, br, cluster);
                        *r = T::from_i32(unpack_signed(HybridUint::read_config_420(token, br)));
                    }
                }
                (Codes::Ans(ans), false) => {
                    for r in row.iter_mut() {
                        let token = self.ans_reader.read(ans, br, cluster);
                        *r = T::from_i32(unpack_signed(uint_config.read(token, br)));
                    }
                }
            },
            SymbolReaderState::Lz77(lz77_state) => {
                let lz_dist_cluster = histograms.lz_dist_cluster as usize;
                let lz_dist_uint = &histograms.uint_configs[lz_dist_cluster];
                let lz_len_uint = histograms.lz77_length_uint.as_ref().unwrap();
                let mut x = 0;
                while x < row.len() {
                    if lz77_state.num_to_copy > 0 {
                        let count = (lz77_state.num_to_copy as usize).min(row.len() - x);
                        if lz77_state.copy_pos.wrapping_add(1) == lz77_state.num_decoded {
                            let sym = lz77_state.window
                                [(lz77_state.copy_pos & Lz77State::WINDOW_MASK) as usize];
                            row[x..x + count].fill(T::from_i32(unpack_signed(sym)));
                            lz77_state.push_decoded_symbol_run(sym, count);
                        } else {
                            for dst in &mut row[x..x + count] {
                                let sym = lz77_state.window
                                    [(lz77_state.copy_pos & Lz77State::WINDOW_MASK) as usize];
                                lz77_state.copy_pos += 1;
                                lz77_state.push_decoded_symbol(sym);
                                *dst = T::from_i32(unpack_signed(sym));
                            }
                            lz77_state.num_to_copy -= count as u32;
                        }
                        x += count;
                        if x == row.len() {
                            break;
                        }
                        debug_assert_eq!(lz77_state.num_to_copy, 0);
                    }
                    let token = match &histograms.codes {
                        Codes::Huffman(hc) => hc.read(br, cluster),
                        Codes::Ans(ans) => self.ans_reader.read(ans, br, cluster),
                    };
                    let Some(lz77_token) = token.checked_sub(lz77_state.min_symbol) else {
                        let sym = uint_config.read(token, br);
                        lz77_state.push_decoded_symbol(sym);
                        row[x] = T::from_i32(unpack_signed(sym));
                        x += 1;
                        continue;
                    };
                    if lz77_state.num_decoded == 0 {
                        self.errors.lz77_repeat = true;
                        row[x] = T::from_i32(0);
                        x += 1;
                        continue;
                    }
                    let num_to_copy = lz_len_uint.read(lz77_token, br);
                    let Some(num_to_copy) = num_to_copy.checked_add(lz77_state.min_length) else {
                        warn!(
                            num_to_copy,
                            lz77_state.min_length, "LZ77 num_to_copy overflow"
                        );
                        self.errors.arithmetic_overflow = true;
                        row[x] = T::from_i32(0);
                        x += 1;
                        continue;
                    };
                    let distance_sym = match &histograms.codes {
                        Codes::Huffman(hc) => hc.read(br, lz_dist_cluster),
                        Codes::Ans(ans) => self.ans_reader.read(ans, br, lz_dist_cluster),
                    };
                    let distance_sym = lz_dist_uint.read(distance_sym, br);
                    lz77_state.apply_copy(distance_sym, num_to_copy);
                }
            }
        }
    }

    /// Specialized fast path for when all HybridUint configs are 420.
    ///
    /// # Preconditions
    /// - `histograms.can_use_config_420_fast_path()` must be true (no LZ77, all configs are 420)
    /// - This assumes `SymbolReaderState::None` (verified by debug_assert)
    #[inline(always)]
    pub fn read_unsigned_clustered_config_420(
        &mut self,
        histograms: &Histograms,
        br: &mut BitReader,
        cluster: usize,
    ) -> u32 {
        debug_assert!(matches!(self.state, SymbolReaderState::None));
        debug_assert!(histograms.can_use_config_420_fast_path());

        let token = match &histograms.codes {
            Codes::Huffman(hc) => hc.read(br, cluster),
            Codes::Ans(ans) => self.ans_reader.read(ans, br, cluster),
        };
        HybridUint::read_config_420(token, br)
    }

    /// Specialized fast path for signed reads when all configs are 420.
    /// See [`read_unsigned_clustered_config_420`] for preconditions.
    #[inline(always)]
    pub fn read_signed_clustered_config_420(
        &mut self,
        histograms: &Histograms,
        br: &mut BitReader,
        cluster: usize,
    ) -> i32 {
        let unsigned = self.read_unsigned_clustered_config_420(histograms, br, cluster);
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

    pub fn check_for_error(&self) -> Result<()> {
        self.errors.check_for_error()
    }

    pub fn checkpoint<const N: usize>(&self) -> Checkpoint<N> {
        let state = match &self.state {
            SymbolReaderState::None => StateCheckpoint::None,
            SymbolReaderState::Lz77(lz77_state) => {
                let mut window = [0u32; N];
                for (i, slot) in window.iter_mut().enumerate() {
                    let idx = ((lz77_state.num_decoded.wrapping_add(i as u32))
                        & Lz77State::WINDOW_MASK) as usize;
                    if let Some(&val) = lz77_state.window.get(idx) {
                        *slot = val;
                    }
                }
                StateCheckpoint::Lz77 {
                    num_to_copy: lz77_state.num_to_copy,
                    copy_pos: lz77_state.copy_pos,
                    num_decoded: lz77_state.num_decoded,
                    window,
                }
            }
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

                let num_rewind = lz77_state.num_decoded.wrapping_sub(num_decoded);
                for (i, &val) in window[..num_rewind as usize].iter().enumerate() {
                    let idx =
                        ((num_decoded.wrapping_add(i as u32)) & Lz77State::WINDOW_MASK) as usize;
                    lz77_state.window[idx] = val;
                }

                lz77_state.num_to_copy = num_to_copy;
                lz77_state.copy_pos = copy_pos;
                lz77_state.num_decoded = num_decoded;
            }
        }

        self.ans_reader = checkpoint.ans_reader;
        self.errors = checkpoint.errors;
    }
}

impl Histograms {
    pub fn decode(
        num_contexts: usize,
        br: &mut BitReader,
        allow_lz77: bool,
        lz77_scratch: &mut Vec<u32>,
    ) -> Result<Histograms> {
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
            decode_context_map(num_contexts, br, lz77_scratch)?
        } else {
            vec![0]
        };
        assert_eq!(context_map.len(), num_contexts);

        // Capture the LZ77 distance cluster BEFORE any later resize() can pad
        // context_map with zeros (see Histograms::resize and Frame::decode).
        let lz_dist_cluster = if lz77_params.enabled {
            *context_map.last().unwrap()
        } else {
            0
        };

        let use_prefix_code = br.read(1)? != 0;
        let log_alpha_size = if use_prefix_code {
            HUFFMAN_MAX_BITS
        } else {
            br.read(2)? as usize + 5
        };
        // Note: cluster indices are u8, so a context map that uses cluster 255
        // yields 256 histograms; compute the count in usize to avoid overflow.
        let num_histograms = *context_map.iter().max().unwrap() as usize + 1;
        let uint_configs = ((0..num_histograms).map(|_| HybridUint::decode(log_alpha_size, br)))
            .collect::<Result<_>>()?;

        let codes = if use_prefix_code {
            Codes::Huffman(HuffmanCodes::decode(num_histograms, br)?)
        } else {
            Codes::Ans(AnsCodes::decode(num_histograms, log_alpha_size, br)?)
        };

        Ok(Histograms {
            lz77_params,
            lz77_length_uint,
            context_map,
            lz_dist_cluster,
            log_alpha_size,
            uint_configs,
            codes,
        })
    }

    #[inline(always)]
    pub fn map_context_to_cluster(&self, context: usize) -> usize {
        self.context_map[context] as usize
    }

    #[allow(dead_code)]
    pub fn num_histograms(&self) -> usize {
        *self.context_map.iter().max().unwrap() as usize + 1
    }

    pub fn resize(&mut self, num_contexts: usize) {
        self.context_map.resize(num_contexts, 0);
    }

    /// Returns true if the config 420 fast path can be safely used.
    /// Config 420: split_exponent=4, msb_in_token=2, lsb_in_token=0 (common pattern)
    /// Requires: all configs are 420 AND LZ77 is disabled
    pub fn can_use_config_420_fast_path(&self) -> bool {
        !self.lz77_params.enabled && self.uint_configs.iter().all(|cfg| cfg.is_config_420())
    }

    pub fn max_num_bits(&self) -> usize {
        let mut max_bits = 0;
        let lz_min_sym = if self.lz77_params.enabled {
            self.lz77_params.min_symbol.unwrap()
        } else {
            u32::MAX
        };
        for cluster in 0..self.uint_configs.len() {
            if self.lz77_params.enabled && cluster as u8 == self.lz_dist_cluster {
                continue;
            }
            let uint = &self.uint_configs[cluster];
            let mut max_symbol = self.codes.max_symbol_for_cluster(cluster);
            if self.lz77_params.enabled {
                max_symbol = max_symbol.min(lz_min_sym.saturating_sub(1));
            }
            max_bits = max_bits.max(uint.max_bits_for_symbol(max_symbol));
        }
        max_bits
    }

    pub fn single_symbol(&self, ctx: usize) -> Option<u32> {
        self.codes.single_symbol(ctx)
    }

    pub fn codes(&self) -> &Codes {
        &self.codes
    }

    /// Whether every LZ77 copy has distance 1, i.e. the stream is a plain run-length
    /// encoding. False without LZ77: there is no distance cluster then (`lz_dist_cluster`
    /// falls back to cluster 0, which holds real data), and the parameters an RLE decoder
    /// needs (`min_symbol`, `min_length`, `lz77_length_uint`) are `None`.
    pub fn is_rle(&self) -> bool {
        if !self.lz77_params.enabled {
            return false;
        }
        let lz_dist_cluster = self.lz_dist_cluster as usize;
        let lz_conf = &self.uint_configs[lz_dist_cluster];
        self.codes.single_symbol(lz_dist_cluster) == Some(1) && lz_conf.is_split_exponent_zero()
    }

    pub fn lz77_params(&self) -> Lz77Params {
        self.lz77_params
    }
    pub fn lz77_length_uint(&self) -> HybridUint {
        self.lz77_length_uint.unwrap()
    }
    #[inline(always)]
    pub fn uint(&self, cluster: usize) -> &HybridUint {
        &self.uint_configs[cluster]
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
}

#[derive(Debug)]
pub struct Checkpoint<const N: usize> {
    state: StateCheckpoint<N>,
    ans_reader: AnsReader,
    errors: ErrorState,
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_histograms_max_num_bits() {
        let huff = HuffmanCodes::byte_histogram();
        let uint_config = HybridUint::new(4, 2, 0);
        let histograms = Histograms {
            lz77_params: Lz77Params {
                enabled: false,
                min_symbol: None,
                min_length: None,
            },
            lz77_length_uint: None,
            context_map: vec![0],
            lz_dist_cluster: 0,
            log_alpha_size: 15,
            uint_configs: vec![uint_config],
            codes: Codes::Huffman(huff),
        };
        assert_eq!(histograms.max_num_bits(), 32);

        // With LZ77 enabled and min_symbol = 16, non-LZ symbols are restricted to < 16.
        let huff_lz = HuffmanCodes::byte_histogram();
        let histograms_lz = Histograms {
            lz77_params: Lz77Params {
                enabled: true,
                min_symbol: Some(16),
                min_length: Some(3),
            },
            lz77_length_uint: None,
            context_map: vec![0],
            lz_dist_cluster: 1, // cluster 1 is dist cluster, cluster 0 is data
            log_alpha_size: 15,
            uint_configs: vec![uint_config, uint_config],
            codes: Codes::Huffman(huff_lz),
        };
        // Max symbol for data cluster 0 is clamped to min_symbol - 1 = 15.
        // For symbol 15 with config (4,2,0), max_bits is 4.
        assert_eq!(histograms_lz.max_num_bits(), 4);
    }

    #[test]
    fn test_lz77_row_run_exceeding_window_size() {
        let window_size = 1usize << Lz77State::LOG_WINDOW_SIZE;
        let uint_config = HybridUint::new(4, 2, 0);
        let histograms = Histograms {
            lz77_params: Lz77Params {
                enabled: true,
                min_symbol: Some(16),
                min_length: Some(3),
            },
            lz77_length_uint: Some(uint_config),
            context_map: vec![0, 0],
            lz_dist_cluster: 0,
            log_alpha_size: 15,
            uint_configs: vec![uint_config],
            codes: Codes::Huffman(HuffmanCodes::byte_histogram()),
        };

        let bits = [6u8.reverse_bits(), 8u8.reverse_bits()];
        let mut br = BitReader::new(&bits);
        let mut reader = SymbolReader {
            state: SymbolReaderState::Lz77(Lz77State {
                min_symbol: 16,
                min_length: 3,
                dist_multiplier: 0,
                window: vec![4],
                num_to_copy: (window_size + 5) as u32,
                copy_pos: 0,
                num_decoded: 1,
            }),
            ans_reader: AnsReader::new_unused(),
            errors: ErrorState::new(),
        };

        let mut row = vec![0i32; window_size + 7];
        reader.read_signed_clustered_row(&histograms, &mut br, 0, &mut row);
        assert!(row[..window_size + 5].iter().all(|&v| v == 2));
        assert_eq!(row[window_size + 5], 3);
        assert_eq!(row[window_size + 6], 4);
    }
}
