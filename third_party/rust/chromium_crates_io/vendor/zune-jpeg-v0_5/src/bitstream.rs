/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

#![allow(
    clippy::if_not_else,
    clippy::similar_names,
    clippy::inline_always,
    clippy::doc_markdown,
    clippy::cast_sign_loss,
    clippy::cast_possible_truncation
)]

//! This file exposes a single struct that can decode a huffman encoded
//! Bitstream in a JPEG file
//!
//! This code is optimized for speed.
//! It's meant to be super duper super fast, because everyone else depends on this being fast.
//! It's (annoyingly) serial hence we cant use parallel bitstreams(it's variable length coding.)
//!
//! Furthermore, on the case of refills, we have to do bytewise processing because the standard decided
//! that we want to support markers in the middle of streams(seriously few people use RST markers).
//!
//! So we pull in all optimization steps:
//! - use `inline[always]`? ✅ ,
//! - pre-execute most common cases ✅,
//! - add random comments ✅
//! -  fast paths ✅.
//!
//! Speed-wise: It is probably the fastest JPEG BitStream decoder to ever sail the seven seas because of
//! a couple of optimization tricks.
//! 1. Fast refills from libjpeg-turbo
//! 2. As few as possible branches in decoder fast paths.
//! 3. Accelerated AC table decoding borrowed from stb_image.h written by Fabian Gissen (@ rygorous),
//!    improved by me to handle more cases.
//! 4. Safe and extensible routines(e.g. cool ways to eliminate bounds check)
//! 5. No unsafe here
//!
//! Readability comes as a second priority(I tried with variable names this time, and we are wayy better than libjpeg).
//!
//! Anyway if you are reading this it means your cool and I hope you get whatever part of the code you are looking for
//! (or learn something cool)
//!
//! Knock yourself out.
use alloc::format;
use alloc::string::ToString;
use core::cmp::min;

use zune_core::bytestream::{ZByteReaderTrait, ZReader};
use zune_core::log::warn;

use crate::decoder::EntropyTables;
use crate::errors::DecodeErrors;
use crate::huffman::{HuffmanTable, HUFF_LOOKAHEAD};
use crate::marker::Marker;
use crate::mcu::DCT_BLOCK;
use crate::misc::UN_ZIGZAG;

macro_rules! decode_huff {
    ($stream:tt,$symbol:tt,$table:tt) => {
        let mut code_length = $symbol >> HUFF_LOOKAHEAD;

        ($symbol) &= (1 << HUFF_LOOKAHEAD) - 1;

        if code_length > i32::from(HUFF_LOOKAHEAD)
        {
            // if the symbol cannot be resolved in the first HUFF_LOOKAHEAD bits,
            // we know it lies somewhere between HUFF_LOOKAHEAD and 16 bits since jpeg imposes 16 bit
            // limit, we can therefore look 16 bits ahead and try to resolve the symbol
            // starting from 1+HUFF_LOOKAHEAD bits.
            $symbol = ($stream).peek_bits::<16>() as i32;
            // (Credits to Sean T. Barrett stb library for this optimization)
            // maxcode is pre-shifted 16 bytes long so that it has (16-code_length)
            // zeroes at the end hence we do not need to shift in the inner loop.
            while code_length < 17{
                if $symbol < $table.maxcode[code_length as usize]  {
                    break;
                }
                code_length += 1;
            }

            if code_length == 17{
                $stream.corrupt_data = true;
                if $stream.strict {
                    return Err(DecodeErrors::FormatStatic("Bad Huffman Code Corrupt JPEG"));
                }
                warn!("Bad Huffman code in JPEG entropy");
                // The sequential recovery rule consumes the invalid 17-bit
                // code and substitutes a zero symbol, not the rest of the scan.
                $symbol = 0;
            } else {
                $symbol >>= (16-code_length);
                ($symbol) = i32::from(
                    ($table).values
                        [(($symbol + ($table).offset[code_length as usize]) & 0xFF) as usize],
                );
            }
        }
        if code_length> i32::from(($stream).bits_left){
            return Err(DecodeErrors::Format(format!("Code length {code_length}  more than bits left {}",($stream).bits_left)))
        }
        // drop bits read
        ($stream).drop_bits(code_length as u8);
    };
}

/// Opaque snapshot of Huffman bitstream state at an MCU boundary.
///
/// Mirrors libjpeg-turbo's `bitread_perm_state` — the minimum state needed to
/// resume decoding from a previously-completed MCU without re-reading earlier
/// entropy data.
#[derive(Clone, Copy)]
pub(crate) struct HuffmanBitstreamState {
    pub(crate) buffer:            u64,
    pub(crate) aligned_buffer:    u64,
    pub(crate) bits_left:         u8,
    pub(crate) marker:            Option<Marker>,
    pub(crate) overread_by:       usize,
    pub(crate) zeros_past_end:    usize,
    pub(crate) exhausted:         bool,
    pub(crate) stuffed_past_end:  bool,
    pub(crate) seen_eoi:          bool,
    pub(crate) eob_run:           i32,
    pub(crate) marker_bits:       Option<u8>,
    pub(crate) insufficient_data: bool,
    pub(crate) corrupt_data:      bool
}

/// Type-erased bitstream state snapshot stored in `ScanCheckpoint`.
///
/// This allows the checkpoint struct to remain non-generic while supporting
/// both Huffman and Arithmetic bitstream types.
#[derive(Clone, Copy)]
pub(crate) enum BitstreamStateSnapshot {
    Huffman(HuffmanBitstreamState),
    #[cfg(feature = "arith")]
    Arithmetic(crate::bitstream_arith::ArithBitstreamState),
    /// No bitstream state (checkpoint at scan start / RST boundary where
    /// the bitstream is freshly reset).
    None
}

/// A common `BitStream` interface abstracting both Huffman and Arithmetic bitstream decoding
pub(crate) trait BitStream {
    /// Snapshot type for per-MCU checkpoint/restore.
    type State: Copy;
    type DCEntropyTable;
    type ACEntropyTable;

    fn new() -> Self;

    fn begin_mcu(&mut self) {}

    fn has_corrupt_data(&self) -> bool {
        false
    }

    fn clear_huffman_code_warning(&mut self) {}

    /// Create a new Bitstream for progressive decoding
    fn new_progressive(al: u8, spec_start: u8, spec_end: u8) -> Self;

    /// Strict mode: reject malformed entropy data that lenient decoding recovers from.
    fn set_strict(&mut self, _strict: bool) {}

    fn get_dc_table(
        tables: &mut EntropyTables, dc_pos: usize
    ) -> Result<&mut Self::DCEntropyTable, DecodeErrors>;
    fn get_ac_table(
        tables: &mut EntropyTables, ac_pos: usize
    ) -> Result<&mut Self::ACEntropyTable, DecodeErrors>;
    fn get_dc_ac_tables(
        tables: &mut EntropyTables, dc_pos: usize, ac_pos: usize
    ) -> Result<(&mut Self::DCEntropyTable, &mut Self::ACEntropyTable), DecodeErrors>;

    fn reset_arith_tables(tables: &mut EntropyTables);

    fn refill<T>(&mut self, reader: &mut ZReader<T>) -> Result<bool, DecodeErrors>
    where
        T: ZByteReaderTrait;

    #[allow(clippy::too_many_arguments)]
    fn decode_mcu_block<T>(
        &mut self, reader: &mut ZReader<T>, dc_table: &mut Self::DCEntropyTable,
        ac_table: &mut Self::ACEntropyTable, qt_table: &[i32; DCT_BLOCK], block: &mut [i32; 64],
        dc_prediction: &mut i32, last_dc_diff: &mut i32
    ) -> Result<u16, DecodeErrors>
    where
        T: ZByteReaderTrait;

    fn discard_mcu_block<T>(
        &mut self, reader: &mut ZReader<T>, dc_table: &mut Self::DCEntropyTable,
        ac_table: &mut Self::ACEntropyTable, last_dc_diff: &mut i32
    ) -> Result<u16, DecodeErrors>
    where
        T: ZByteReaderTrait;

    fn decode_prog_dc_first<T>(
        &mut self, reader: &mut ZReader<T>, dc_table: &mut Self::DCEntropyTable, block: &mut i16,
        dc_prediction: &mut i32, last_dc_diff: &mut i32, suspend_at_eof: bool
    ) -> Result<(), DecodeErrors>
    where
        T: ZByteReaderTrait;

    fn decode_prog_dc_refine<T>(
        &mut self, reader: &mut ZReader<T>, block: &mut i16
    ) -> Result<(), DecodeErrors>
    where
        T: ZByteReaderTrait;

    fn decode_mcu_ac_first<T>(
        &mut self, reader: &mut ZReader<T>, ac_table: &mut Self::ACEntropyTable,
        block: &mut [i16; 64]
    ) -> Result<bool, DecodeErrors>
    where
        T: ZByteReaderTrait;

    fn decode_mcu_ac_refine<T>(
        &mut self, reader: &mut ZReader<T>, table: &mut Self::ACEntropyTable, block: &mut [i16; 64]
    ) -> Result<bool, DecodeErrors>
    where
        T: ZByteReaderTrait;

    fn update_progressive_params(&mut self, _ah: u8, al: u8, spec_start: u8, spec_end: u8);

    fn reset(&mut self);

    fn overread_by(&self) -> usize;

    /// Whether the decoder has consumed bits that were not part of the entropy-coded data.
    ///
    /// `overread_by()` turns positive when the reader's lookahead reaches the end of the data,
    /// which it does while the bits of the last MCUs are still in the buffer, unconsumed. This
    /// is true only once those buffered bits have been consumed past the end: until then every
    /// bit the decoder took was in the data, and a scan whose data holds all of its MCUs is
    /// decoded whole, with or without the `EOI` after it.
    fn consumed_past_end(&self) -> bool {
        self.overread_by() > 0
    }

    /// True if we have seen end of image marker.
    /// Don't read anything after that.
    fn seen_eoi(&mut self) -> &mut bool;

    /// Did we find a marker(RST/EOF) during decoding?
    fn marker(&mut self) -> &mut Option<Marker>;

    fn eob_run(&mut self) -> &mut i32;

    /// Tell us the bits left the two buffer
    fn bits_left(&self) -> u8;

    /// True when a latched marker plus `bits_left() == 0` means the entropy-coded
    /// data is used up. Huffman data ends at the marker; the arithmetic decoder
    /// keeps reading zero bytes after a marker until the scan is complete, and its
    /// `bits_left()` is a byte shift counter that is 0 between any two bytes.
    fn marker_ends_data() -> bool;

    /// Whether this bitstream type supports per-MCU checkpoint/restore.
    /// Huffman coding supports it; arithmetic coding does not (the A/C/CT
    /// registers are coupled to statistical context tables).
    fn supports_mcu_checkpoint() -> bool;

    /// Capture the bitstream state at an MCU boundary (libjpeg-turbo: BITREAD_SAVE_STATE).
    fn save_state(&self) -> Self::State;

    /// Restore a previously captured bitstream state (libjpeg-turbo: BITREAD_LOAD_STATE).
    fn restore_state(&mut self, state: Self::State);

    /// Capture and wrap the bitstream state into a type-erased snapshot for
    /// storage in `ScanCheckpoint`.
    fn snapshot_state(&self) -> BitstreamStateSnapshot;

    /// Restore from a type-erased snapshot. Panics if variant doesn't match.
    fn restore_snapshot(&mut self, snapshot: BitstreamStateSnapshot);
}

/// A `BitStream` struct, a bit by bit reader with super powers
///
#[rustfmt::skip]
pub(crate) struct BitStreamHuffman {
    /// A MSB type buffer that is used for some certain operations
    buffer:              u64,
    /// A TOP  aligned MSB type buffer that is used to accelerate some operations like
    /// peek_bits and get_bits.
    ///
    /// By top aligned, I mean the top bit (63) represents the top bit in the buffer.
    aligned_buffer:      u64,
    /// Tell us the bits left the two buffer
    bits_left:           u8,
    /// Did we find a marker(RST/EOF) during decoding?
    marker:              Option<Marker>,
    /// An i16 with the bit corresponding to successive_low set to 1, others 0.
    successive_low_mask: i16,
    spec_start:          u8,
    spec_end:            u8,
    eob_run:             i32,/// Did we find a marker(RST/EOF) during decoding?
    overread_by:         usize,
    /// Zero bits appended to the buffer after the data ended, which a scan read past its data
    /// would consume, and which a scan whose data held all of its MCUs never consumes.
    zeros_past_end:      usize,
    /// Set once the decoder takes more bits than the buffer holds, which only a scan read past
    /// its data does.
    exhausted:           bool,
    /// Set once a stuffing resolution — the byte read after an entropy `0xFF` — consumed a
    /// byte past the end of the data. The resolution byte is not appended to the buffer, so
    /// it reaches no counter of appended zeros, but it was still consumed: whether that
    /// `0xFF` was data or the first half of a marker is exactly what the missing byte would
    /// have said, so a row that reached it is not publishable.
    stuffed_past_end:    bool,
    /// True if we have seen end of image marker.
    /// Don't read anything after that.
    seen_eoi:            bool,
    /// Strict mode, see [`BitStream::set_strict`].
    strict:              bool,
    // Real entropy bits remaining before the latched marker. Lookahead may
    // include zero padding, but consuming it marks subsequent MCUs as missing.
    marker_bits:         Option<u8>,
    insufficient_data:   bool,
    // Latched at the MCU boundary, not between components of the same MCU.
    skip_mcu:            bool,
    corrupt_data:        bool,
}

impl BitStreamHuffman {
    /// The zig-zag position of an AC coefficient whose run ended at `pos`. A run past the
    /// last coefficient of the block (63) is malformed: strict mode rejects it, lenient mode
    /// puts the coefficient on that last position, as libjpeg-turbo does, instead of
    /// wrapping it onto the DC or a low-frequency coefficient. A run past the end of a
    /// progressive scan's band but within the block is left alone, as in libjpeg-turbo.
    #[inline(always)]
    fn ac_position(&self, pos: usize) -> Result<usize, DecodeErrors> {
        if pos <= 63 {
            return Ok(pos);
        }
        if self.strict {
            return Err(DecodeErrors::FormatStatic(
                "AC coefficient run past the end of the block"
            ));
        }
        Ok(63)
    }

    /// Get a single bit from the bitstream
    fn get_bit(&mut self) -> u8 {
        let k = (self.aligned_buffer >> 63) as u8;
        // discard a bit
        self.drop_bits(1);
        return k;
    }

    /// Peek `look_ahead` bits ahead without discarding them from the buffer
    #[inline(always)]
    #[allow(clippy::cast_possible_truncation)]
    const fn peek_bits<const LOOKAHEAD: u8>(&self) -> i32 {
        (self.aligned_buffer >> (64 - LOOKAHEAD)) as i32
    }

    /// Discard the next `N` bits without checking
    #[inline]
    fn drop_bits(&mut self, n: u8) {
        self.consume_marker_bits(n);
        // PS: Its a good check, but triggers fuzzer and a lot of false positives
        //debug_assert!(self.bits_left >= n);
        //self.bits_left -= n;
        self.bits_left = self.bits_left.saturating_sub(n);
        self.aligned_buffer <<= n;
    }

    /// Read `n_bits` from the buffer  and discard them
    #[inline(always)]
    #[allow(clippy::cast_possible_truncation)]
    fn get_bits(&mut self, n_bits: u8) -> i32 {
        self.consume_marker_bits(n_bits);
        let mask = (1_u64 << n_bits) - 1;

        self.aligned_buffer = self.aligned_buffer.rotate_left(u32::from(n_bits));
        let bits = (self.aligned_buffer & mask) as i32;
        let (left, exhausted) = self.bits_left.overflowing_sub(n_bits);
        self.bits_left = left;
        self.exhausted |= exhausted;
        bits
    }

    fn consume_marker_bits(&mut self, bits: u8) {
        if let Some(remaining) = &mut self.marker_bits {
            self.insufficient_data |= bits > *remaining;
            *remaining = remaining.saturating_sub(bits);
        }
    }

    /// Decode the DC coefficient in a MCU block.
    ///
    /// The decoded coefficient is written to `dc_prediction`
    ///
    #[allow(
        clippy::cast_possible_truncation,
        clippy::cast_sign_loss,
        clippy::unwrap_used
    )]
    #[inline(always)]
    fn decode_dc<T>(
        &mut self, reader: &mut ZReader<T>, dc_table: &HuffmanTable, dc_prediction: &mut i32
    ) -> Result<bool, DecodeErrors>
    where
    T: ZByteReaderTrait
    {
        let (mut symbol, r);

        if self.bits_left < 32 {
            self.refill(reader)?;
        }
        // look a head HUFF_LOOKAHEAD bits into the bitstream
        symbol = self.peek_bits::<HUFF_LOOKAHEAD>();
        symbol = dc_table.lookup[symbol as usize];

        decode_huff!(self, symbol, dc_table);

        if symbol != 0 {
            r = self.get_bits(symbol as u8);
            symbol = huff_extend(r, symbol);
        }
        // Update DC prediction
        *dc_prediction = dc_prediction.wrapping_add(symbol);

        return Ok(true);
    }

    /// Like `decode_dc` but we do not need the result of the component, we only want to remove it
    /// from the bitstream of the MCU.
    fn discard_dc<T>(
        &mut self, reader: &mut ZReader<T>, dc_table: &HuffmanTable
    ) -> Result<bool, DecodeErrors>
    where
    T: ZByteReaderTrait
    {
        let mut symbol;

        if self.bits_left < 32 {
            self.refill(reader)?;
        }
        // look a head HUFF_LOOKAHEAD bits into the bitstream
        symbol = self.peek_bits::<HUFF_LOOKAHEAD>();
        symbol = dc_table.lookup[symbol as usize];

        decode_huff!(self, symbol, dc_table);

        if symbol != 0 {
            let _ = self.get_bits(symbol as u8);
        }

        return Ok(true);
    }
}

impl BitStreamHuffman {
    #[inline(always)]
    fn refill_byte<T: ZByteReaderTrait, const SUSPEND: bool>(
        &mut self, reader: &mut ZReader<T>
    ) -> Result<bool, DecodeErrors> {
        let read_byte = |reader: &mut ZReader<T>| -> Result<u64, DecodeErrors> {
            if SUSPEND && reader.eof()? {
                return Err(DecodeErrors::ExhaustedData);
            }
            Ok(u64::from(reader.read_u8()))
        };
        let past_end = reader.eof()?;
        let byte = read_byte(reader)?;
        self.overread_by += usize::from(reader.eof()?);
        self.zeros_past_end += 8 * usize::from(past_end);
        self.buffer = (self.buffer << 8) | byte;
        self.bits_left += 8;
        if byte == 0xff {
            let stuffed_past_end = reader.eof()?;
            let mut next_byte = read_byte(reader)?;
            self.stuffed_past_end |= stuffed_past_end;
            while next_byte == 0xff {
                let more_past_end = reader.eof()?;
                next_byte = read_byte(reader)?;
                self.stuffed_past_end |= more_past_end;
            }
            if next_byte == 0 {
                self.overread_by += usize::from(reader.eof()?);
            }
            if next_byte != 0 {
                self.buffer >>= 8;
                self.bits_left -= 8;
                if self.bits_left != 0 {
                    self.aligned_buffer = self.buffer << (64 - self.bits_left);
                }
                let marker = Marker::from_u8(next_byte as u8);
                self.marker = marker;
                self.marker_bits = Some(self.bits_left);
                if let Some(Marker::UNKNOWN(_)) = marker {
                    return Err(DecodeErrors::Format(
                        "Unknown marker in bit stream".to_string()
                    ));
                }
                if next_byte == 0xd9 {
                    self.buffer <<= 8;
                    self.bits_left += 8;
                    self.aligned_buffer = self.buffer << (64 - self.bits_left);
                }
                return Ok(false);
            }
        }
        Ok(true)
    }

    fn refill_progressive_dc<T: ZByteReaderTrait>(
        &mut self, reader: &mut ZReader<T>
    ) -> Result<(), DecodeErrors> {
        if self.marker.is_some() || self.seen_eoi {
            self.refill(reader)?;
            return Ok(());
        }
        let minimum_bits = (usize::BITS - 7) as u8;
        while self.bits_left < minimum_bits {
            if !self.refill_byte::<T, true>(reader)? {
                break;
            }
        }
        if self.bits_left < 8 {
            self.refill(reader)?;
        }
        self.aligned_buffer = self.buffer << (64 - self.bits_left);
        Ok(())
    }

    fn decode_progressive_dc<T: ZByteReaderTrait>(
        &mut self, reader: &mut ZReader<T>, table: &HuffmanTable
    ) -> Result<i32, DecodeErrors> {
        // A suspending source cannot commit a partial refill. Use the native
        // eight-bit lookup and word-sized prefetch before publishing a DC.
        if self.bits_left < 8 {
            self.refill_progressive_dc(reader)?;
        }
        let mut entry = table.lookup[self.peek_bits::<HUFF_LOOKAHEAD>() as usize];
        let mut length = entry >> HUFF_LOOKAHEAD;
        if length > 8 && self.bits_left < HUFF_LOOKAHEAD {
            self.refill_progressive_dc(reader)?;
            entry = table.lookup[self.peek_bits::<HUFF_LOOKAHEAD>() as usize];
            length = entry >> HUFF_LOOKAHEAD;
        }
        let mut symbol = entry & ((1 << HUFF_LOOKAHEAD) - 1);
        if length > i32::from(HUFF_LOOKAHEAD) {
            while length < 17 {
                if i32::from(self.bits_left) < length {
                    self.refill_progressive_dc(reader)?;
                }
                let code = self.peek_bits::<16>() as i32;
                if code < table.maxcode[length as usize] {
                    symbol = i32::from(
                        table.values[(((code >> (16 - length)) + table.offset[length as usize])
                            & 0xff) as usize]
                    );
                    break;
                }
                length += 1;
            }
            if length == 17 {
                self.corrupt_data = true;
                if self.strict {
                    return Err(DecodeErrors::FormatStatic("Bad Huffman Code Corrupt JPEG"));
                }
                warn!("Bad Huffman code in progressive JPEG entropy");
                symbol = 0;
            }
        }
        if i32::from(self.bits_left) < length {
            self.refill_progressive_dc(reader)?;
        }
        self.drop_bits(length as u8);
        if symbol == 0 {
            return Ok(0);
        }
        if self.bits_left < symbol as u8 {
            self.refill_progressive_dc(reader)?;
        }
        Ok(huff_extend(self.get_bits(symbol as u8), symbol))
    }
}

impl BitStream for BitStreamHuffman {
    type State = HuffmanBitstreamState;
    type DCEntropyTable = HuffmanTable;
    type ACEntropyTable = HuffmanTable;

    #[inline(always)]
    fn get_dc_table(
        tables: &mut EntropyTables, dc_pos: usize
    ) -> Result<&mut Self::DCEntropyTable, DecodeErrors> {
        let dc_table = tables
            .dc_huffman
            .get_mut(dc_pos)
            .ok_or_else(|| {
                DecodeErrors::Format(format!("No huffman table for DC component: {dc_pos}"))
            })?
            .as_mut()
            .ok_or_else(|| {
                DecodeErrors::Format(format!(
                    "DC Huffman table at index {dc_pos} not initialized"
                ))
            })?;

        Ok(dc_table)
    }

    #[inline(always)]
    fn get_ac_table(
        tables: &mut EntropyTables, ac_pos: usize
    ) -> Result<&mut Self::ACEntropyTable, DecodeErrors> {
        let ac_table = tables
            .ac_huffman
            .get_mut(ac_pos)
            .ok_or_else(|| {
                DecodeErrors::Format(format!("No huffman table for AC component: {ac_pos}"))
            })?
            .as_mut()
            .ok_or_else(|| {
                DecodeErrors::Format(format!(
                    "AC Huffman table at index {ac_pos} not initialized"
                ))
            })?;

        Ok(ac_table)
    }
    #[inline(always)]
    fn get_dc_ac_tables(
        tables: &mut EntropyTables, dc_pos: usize, ac_pos: usize
    ) -> Result<(&mut Self::DCEntropyTable, &mut Self::ACEntropyTable), DecodeErrors> {
        let dc_table = tables
            .dc_huffman
            .get_mut(dc_pos)
            .ok_or_else(|| {
                DecodeErrors::Format(format!("No huffman table for DC component: {dc_pos}"))
            })?
            .as_mut()
            .ok_or_else(|| {
                DecodeErrors::Format(format!(
                    "DC Huffman table at index {dc_pos} not initialized"
                ))
            })?;

        let ac_table = tables
            .ac_huffman
            .get_mut(ac_pos)
            .ok_or_else(|| {
                DecodeErrors::Format(format!("No huffman table for AC component: {ac_pos}"))
            })?
            .as_mut()
            .ok_or_else(|| {
                DecodeErrors::Format(format!(
                    "AC Huffman table at index {ac_pos} not initialized"
                ))
            })?;

        Ok((dc_table, ac_table))
    }

    #[inline(always)]
    fn reset_arith_tables(_: &mut EntropyTables) {
        // do nothing
    }

    /// Create a new BitStream
    #[rustfmt::skip]
    #[inline(always)]
    fn new() -> BitStreamHuffman {
        BitStreamHuffman {
            buffer:              0,
            aligned_buffer:      0,
            bits_left:           0,
            marker:              None,
            successive_low_mask: 1,
            spec_start:          0,
            spec_end:            0,
            eob_run:             0,
            overread_by:         0,
            zeros_past_end:      0,
            exhausted:           false,
            stuffed_past_end:    false,
            seen_eoi:            false,
            strict:              false,
            marker_bits:         None,
            insufficient_data:   false,
            skip_mcu:            false,
            corrupt_data:        false,
        }
    }

    /// Create a new Bitstream for progressive decoding
    #[allow(clippy::redundant_field_names)]
    #[rustfmt::skip]
    #[inline(always)]
    fn new_progressive(al: u8, spec_start: u8, spec_end: u8) -> BitStreamHuffman {
        BitStreamHuffman {
            buffer:              0,
            aligned_buffer:      0,
            bits_left:           0,
            marker:              None,
            successive_low_mask: 1i16 << al,
            spec_start:          spec_start,
            spec_end:            spec_end,
            eob_run:             0,
            overread_by:         0,
            zeros_past_end:      0,
            exhausted:           false,
            stuffed_past_end:    false,
            seen_eoi:            false,
            strict:              false,
            marker_bits:         None,
            insufficient_data:   false,
            skip_mcu:            false,
            corrupt_data:        false,
        }
    }

    fn set_strict(&mut self, strict: bool) {
        self.strict = strict;
    }

    fn begin_mcu(&mut self) {
        self.skip_mcu = self.insufficient_data && !self.strict;
    }

    fn has_corrupt_data(&self) -> bool {
        self.corrupt_data || self.insufficient_data
    }

    fn clear_huffman_code_warning(&mut self) {
        self.corrupt_data = false;
    }

    #[inline(always)]
    fn overread_by(&self) -> usize {
        self.overread_by
    }
    #[inline(always)]
    fn consumed_past_end(&self) -> bool {
        self.overread_by > 0
            && (self.exhausted
                || self.zeros_past_end > usize::from(self.bits_left)
                || self.stuffed_past_end)
    }
    #[inline(always)]
    fn seen_eoi(&mut self) -> &mut bool {
        &mut self.seen_eoi
    }
    #[inline(always)]
    fn marker(&mut self) -> &mut Option<Marker> {
        &mut self.marker
    }
    #[inline(always)]
    fn eob_run(&mut self) -> &mut i32 {
        &mut self.eob_run
    }
    #[inline(always)]
    fn bits_left(&self) -> u8 {
        self.bits_left
    }

    #[inline(always)]
    fn marker_ends_data() -> bool {
        true
    }

    #[inline(always)]
    fn supports_mcu_checkpoint() -> bool {
        true
    }

    #[inline(always)]
    fn save_state(&self) -> HuffmanBitstreamState {
        HuffmanBitstreamState {
            buffer:         self.buffer,
            aligned_buffer: self.aligned_buffer,
            bits_left:      self.bits_left,
            marker:         self.marker,
            overread_by:      self.overread_by,
            zeros_past_end:   self.zeros_past_end,
            exhausted:        self.exhausted,
            stuffed_past_end: self.stuffed_past_end,
            seen_eoi:       self.seen_eoi,
            eob_run:        self.eob_run,
            marker_bits:    self.marker_bits,
            insufficient_data: self.insufficient_data,
            corrupt_data: self.corrupt_data
        }
    }

    #[inline(always)]
    fn restore_state(&mut self, state: HuffmanBitstreamState) {
        self.buffer = state.buffer;
        self.aligned_buffer = state.aligned_buffer;
        self.bits_left = state.bits_left;
        self.marker = state.marker;
        self.overread_by = state.overread_by;
        self.zeros_past_end = state.zeros_past_end;
        self.exhausted = state.exhausted;
        self.stuffed_past_end = state.stuffed_past_end;
        self.seen_eoi = state.seen_eoi;
        self.eob_run = state.eob_run;
        self.marker_bits = state.marker_bits;
        self.insufficient_data = state.insufficient_data;
        self.corrupt_data = state.corrupt_data;
    }

    #[inline(always)]
    fn snapshot_state(&self) -> BitstreamStateSnapshot {
        BitstreamStateSnapshot::Huffman(self.save_state())
    }

    #[inline(always)]
    fn restore_snapshot(&mut self, snapshot: BitstreamStateSnapshot) {
        match snapshot {
            BitstreamStateSnapshot::Huffman(s) => self.restore_state(s),
            BitstreamStateSnapshot::None => {}
            #[cfg(feature = "arith")]
            _ => unreachable!("Huffman stream given arithmetic snapshot")
        }
    }

    /// Refill the bit buffer by (a maximum of) 32 bits
    ///
    /// # Arguments
    ///  - `reader`:`&mut BufReader<R>`: A mutable reference to an underlying
    ///    File/Memory buffer containing a valid JPEG stream
    ///
    /// This function will only refill if `self.count` is less than 32
    #[inline(always)] // to many call sites? ( perf improvement by 4%)
    fn refill<T>(&mut self, reader: &mut ZReader<T>) -> Result<bool, DecodeErrors>
    where
        T: ZByteReaderTrait
    {
        // 32 bits is enough for a decode(16 bits) and receive_extend(max 16 bits)
        if self.bits_left < 32 {
            if self.marker.is_some() || self.seen_eoi {
                // found a marker, or we are in EOI
                // also we are in over-reading mode, where we fill it with zeroes

                // fill with zeroes
                self.buffer <<= 32;
                self.bits_left += 32;
                if self.overread_by > 0 {
                    self.zeros_past_end += 32;
                }
                self.aligned_buffer = self.buffer << (64 - self.bits_left);
                return Ok(true);
            }

            if self.overread_by > 0 {
                if self.bits_left == 0 {
                    return Err(DecodeErrors::ExhaustedData);
                }
                // We already hit EOF while refilling. Continue consuming the buffered bits
                // but don't synthesize additional bytes from zero-fill.
                return Ok(true);
            }

            // we optimize for the case where we don't have 255 in the stream and have 4 bytes left
            // as it is the common case
            //
            // so we always read 4 bytes, if read_fixed_bytes errors out, the cursor is
            // guaranteed not to advance in case of failure (is this true), so
            // we revert the read later on (if we have 255), if this fails, we use the normal
            // byte at a time read

            if let Ok(bytes) = reader.read_fixed_bytes_or_error::<4>() {
                // we have 4 bytes to spare, read the 4 bytes into a temporary buffer
                // create buffer
                let msb_buf = u32::from_be_bytes(bytes);
                // check if we have 0xff
                if !has_byte(msb_buf, 255) {
                    self.bits_left += 32;
                    self.buffer <<= 32;
                    self.buffer |= u64::from(msb_buf);
                    self.aligned_buffer = self.buffer << (64 - self.bits_left);
                    return Ok(true);
                }

                reader.rewind(4)?;
            }
            for _ in 0..4 {
                if !self.refill_byte::<T, false>(reader)? {
                    return Ok(false);
                }
            }
            // Construct an MSB buffer whose top bits are the bitstream we are currently holding.
            self.aligned_buffer = self.buffer << (64 - self.bits_left);
        }
        return Ok(true);
    }

    /// Decode a Minimum Code Unit(MCU) as quickly as possible
    ///
    /// # Arguments
    /// - reader: The bitstream from where we read more bits.
    /// - dc_table: The Huffman table used to decode the DC coefficient
    /// - ac_table: The Huffman table used to decode AC values
    /// - block: A memory region where we will write out the decoded values
    /// - DC prediction: Last DC value for this component
    ///
    #[allow(
        clippy::many_single_char_names,
        clippy::cast_possible_truncation,
        clippy::cast_sign_loss
    )]
    #[inline(never)]
    fn decode_mcu_block<T>(
        &mut self, reader: &mut ZReader<T>, dc_table: &mut HuffmanTable,
        ac_table: &mut HuffmanTable, qt_table: &[i32; DCT_BLOCK], block: &mut [i32; 64],
        dc_prediction: &mut i32, _last_dc_diff: &mut i32
    ) -> Result<u16, DecodeErrors>
    where
        T: ZByteReaderTrait
    {
        // Get fast AC table as a reference before we enter the hot path
        let ac_lookup = ac_table.ac_lookup.as_ref().unwrap();

        let (mut symbol, mut r, mut fast_ac);
        // Decode AC coefficients
        let mut pos: usize = 1;
        if self.skip_mcu {
            return Ok(0);
        }
        if self.strict && self.bits_left < 1 && self.marker.is_some() {
            return Err(DecodeErrors::Format(
                "No more bytes left in stream before marker".to_string()
            ));
        }
        // decode DC, dc prediction will contain the value
        self.decode_dc(reader, dc_table, dc_prediction)?;

        // set dc to be the dc prediction.
        block[0] = dc_prediction.wrapping_mul(qt_table[0]);

        while pos < 64 {
            self.refill(reader)?;
            symbol = self.peek_bits::<HUFF_LOOKAHEAD>();
            fast_ac = ac_lookup[symbol as usize];
            symbol = ac_table.lookup[symbol as usize];

            if fast_ac != 0 {
                //  FAST AC path
                pos += ((fast_ac >> 4) & 15) as usize; // run
                let t_pos = UN_ZIGZAG[self.ac_position(pos)?] & 63;

                block[t_pos] = i32::from(fast_ac >> 8).wrapping_mul (qt_table[t_pos]); // Value
                self.drop_bits((fast_ac & 15) as u8);
                pos += 1;
            } else {
                decode_huff!(self, symbol, ac_table);

                r = symbol >> 4;
                symbol &= 15;

                if symbol != 0 {
                    pos += r as usize;
                    r = self.get_bits(symbol as u8);
                    symbol = huff_extend(r, symbol);
                    let t_pos = UN_ZIGZAG[self.ac_position(pos)?] & 63;

                    block[t_pos] = symbol .wrapping_mul( qt_table[t_pos]);

                    pos += 1;
                } else if r != 15 {
                    if self.strict && self.insufficient_data {
                        return Err(DecodeErrors::FormatStatic("Premature marker in JPEG scan"));
                    }
                    return Ok(pos as u16);
                } else {
                    pos += 16;
                }
            }
        }

        if self.strict && self.insufficient_data {
            return Err(DecodeErrors::FormatStatic("Premature marker in JPEG scan"));
        }
        return Ok(64);
    }

    /// Advance the bitstream over a block but ignore the data contained.
    ///
    /// This updates DC prediction but we never dequantize and we never do any Zig-Zag translation
    /// either. Still returns the index of the last component read.
    fn discard_mcu_block<T>(
        &mut self, reader: &mut ZReader<T>, dc_table: &mut HuffmanTable,
        ac_table: &mut HuffmanTable, _last_dc_diff: &mut i32
    ) -> Result<u16, DecodeErrors>
    where
        T: ZByteReaderTrait
    {
        // Get fast AC table as a reference before we enter the hot path
        let ac_lookup = ac_table.ac_lookup.as_ref().unwrap();

        let (mut symbol, mut r, mut fast_ac);
        // Decode AC coefficients
        let mut pos: usize = 1;
        if self.skip_mcu {
            return Ok(0);
        }

        // decode DC, dc prediction will contain the value
        self.discard_dc(reader, dc_table)?;

        while pos < 64 {
            self.refill(reader)?;
            symbol = self.peek_bits::<HUFF_LOOKAHEAD>();
            fast_ac = ac_lookup[symbol as usize];
            symbol = ac_table.lookup[symbol as usize];

            if fast_ac != 0 {
                //  FAST AC path
                pos += ((fast_ac >> 4) & 15) as usize; // run
                // nothing is written, but strict mode must reject the same streams
                self.ac_position(pos)?;
                self.drop_bits((fast_ac & 15) as u8);
                pos += 1;
            } else {
                decode_huff!(self, symbol, ac_table);

                r = symbol >> 4;
                symbol &= 15;

                if symbol != 0 {
                    pos += r as usize;
                    self.ac_position(pos)?;
                    // Advance over bits but ignore.
                    let _ = self.get_bits(symbol as u8);

                    pos += 1;
                } else if r != 15 {
                    if self.strict && self.insufficient_data {
                        return Err(DecodeErrors::FormatStatic("Premature marker in JPEG scan"));
                    }
                    return Ok(pos as u16);
                } else {
                    pos += 16;
                }
            }
        }

        if self.strict && self.insufficient_data {
            return Err(DecodeErrors::FormatStatic("Premature marker in JPEG scan"));
        }
        return Ok(64);
    }

    /// Decode a DC block
    #[allow(clippy::cast_possible_truncation)]
    #[inline]
    fn decode_prog_dc_first<T>(
        &mut self, reader: &mut ZReader<T>, dc_table: &mut HuffmanTable, block: &mut i16,
        dc_prediction: &mut i32, _last_dc_diff: &mut i32, suspend_at_eof: bool
    ) -> Result<(), DecodeErrors>
    where
        T: ZByteReaderTrait
    {
        if suspend_at_eof {
            *dc_prediction =
                dc_prediction.wrapping_add(self.decode_progressive_dc(reader, dc_table)?);
        } else {
            self.decode_dc(reader, dc_table, dc_prediction)?;
        }
        *block = (*dc_prediction as i16).wrapping_mul(self.successive_low_mask);
        return Ok(());
    }
    #[inline]
    fn decode_prog_dc_refine<T>(
        &mut self, reader: &mut ZReader<T>, block: &mut i16
    ) -> Result<(), DecodeErrors>
    where
        T: ZByteReaderTrait
    {
        // refinement scan
        if self.bits_left < 1 {
            self.refill(reader)?;
            // if we find a marker, it may happens we don't refill.
            // So let's confirm again that refill worked
            if self.bits_left < 1 {
                return Err(DecodeErrors::Format(
                    "Marker found where not expected in refine bit".to_string()
                ));
            }
        }

        if self.get_bit() == 1 {
            *block = block.wrapping_add(self.successive_low_mask);
        }

        Ok(())
    }

    fn decode_mcu_ac_first<T>(
        &mut self, reader: &mut ZReader<T>, ac_table: &mut HuffmanTable, block: &mut [i16; 64]
    ) -> Result<bool, DecodeErrors>
    where
        T: ZByteReaderTrait
    {
        let fast_ac = ac_table.ac_lookup.as_ref().unwrap();
        let bit = self.successive_low_mask;

        let mut k = self.spec_start as usize;
        let (mut symbol, mut r, mut fac);

        // EOB runs are handled in mcu_prog.rs
        'block: loop {
            self.refill(reader)?;
            // Check for marker in the stream

            symbol = self.peek_bits::<HUFF_LOOKAHEAD>();
            fac = fast_ac[symbol as usize];
            symbol = ac_table.lookup[symbol as usize];

            if fac != 0 {
                // fast ac path
                k += ((fac >> 4) & 15) as usize; // run
                block[UN_ZIGZAG[self.ac_position(k)?] & 63] = (fac >> 8).wrapping_mul(bit); // value
                self.drop_bits((fac & 15) as u8);
                k += 1;
            } else {
                decode_huff!(self, symbol, ac_table);

                r = symbol >> 4;
                symbol &= 15;

                if symbol != 0 {
                    k += r as usize;
                    r = self.get_bits(symbol as u8);
                    symbol = huff_extend(r, symbol);
                    block[UN_ZIGZAG[self.ac_position(k)?] & 63] = (symbol as i16).wrapping_mul(bit);
                    k += 1;
                } else {
                    if r != 15 {
                        self.eob_run = 1 << r;
                        self.eob_run += self.get_bits(r as u8);
                        self.eob_run -= 1;
                        break;
                    }

                    k += 16;
                }
            }

            if k > self.spec_end as usize {
                break 'block;
            }
        }
        return Ok(true);
    }
    #[allow(clippy::too_many_lines, clippy::op_ref)]
    fn decode_mcu_ac_refine<T>(
        &mut self, reader: &mut ZReader<T>, table: &mut HuffmanTable, block: &mut [i16; 64]
    ) -> Result<bool, DecodeErrors>
    where
        T: ZByteReaderTrait
    {
        let bit = self.successive_low_mask;

        let mut k = self.spec_start;
        let (mut symbol, mut r);

        if self.eob_run == 0 {
            'no_eob: loop {
                // Decode a coefficient from the bit stream
                self.refill(reader)?;

                symbol = self.peek_bits::<HUFF_LOOKAHEAD>();
                symbol = table.lookup[symbol as usize];

                decode_huff!(self, symbol, table);

                r = symbol >> 4;
                symbol &= 15;

                if symbol == 0 {
                    if r != 15 {
                        // EOB run is 2^r + bits
                        self.eob_run = 1 << r;
                        self.eob_run += self.get_bits(r as u8);
                        // EOB runs are handled by the eob logic
                        break 'no_eob;
                    }
                } else {
                    // libjpeg-turbo also doesn't return an error here, so let's also warn.
                    if symbol != 1 {
                        warn!("Bad Huffman code, corrupt JPEG?");
                    }
                    // get sign bit
                    // We assume we have enough bits, which should be correct for sane images
                    // since we refill by 32 above
                    if self.get_bit() == 1 {
                        symbol = i32::from(bit);
                    } else {
                        symbol = i32::from(-bit);
                    }
                }

                // Advance over already nonzero coefficients  appending
                // correction bits to the non-zeroes.
                // A correction bit is 1 if the absolute value of the coefficient must be increased

                if k <= self.spec_end {
                    'advance_nonzero: loop {
                        let coefficient = &mut block[UN_ZIGZAG[k as usize & 63] & 63];

                        if *coefficient != 0 {
                            if self.bits_left < 1 {
                                self.refill(reader)?;
                                if self.bits_left < 1 && self.marker.is_some() {
                                    return Err(DecodeErrors::Format(
                                        "Marker found where not expected in refine bit".to_string()
                                    ));
                                }
                            }
                            if self.get_bit() == 1 && (*coefficient & bit) == 0 {
                                if *coefficient > 0 {
                                    *coefficient = coefficient.wrapping_add(bit);
                                } else {
                                    *coefficient = coefficient.wrapping_sub(bit);
                                }
                            }
                        } else {
                            r -= 1;

                            if r < 0 {
                                // reached target zero coefficient.
                                break 'advance_nonzero;
                            }
                        }

                        if k == self.spec_end {
                            break 'advance_nonzero;
                        }

                        k += 1;
                    }
                }

                if symbol != 0 {
                    let pos = UN_ZIGZAG[k as usize & 63];
                    // output new non-zero coefficient.
                    block[pos & 63] = symbol as i16;
                }

                k += 1;

                if k > self.spec_end {
                    break 'no_eob;
                }
            }
        }
        if self.eob_run > 0 {
            // only run if block does not consists of purely zeroes
            // changing this to iter_any makes perf regress by 10%
            //   time:   [+10.836% +11.589% +12.376%] (p = 0.00 < 0.05)
            if &block[1..] != &[0; 63] {
                self.refill(reader)?;

                while k <= self.spec_end {
                    let coefficient = &mut block[UN_ZIGZAG[k as usize & 63] & 63];

                    if *coefficient != 0 {
                        // Zero coefficients consume no correction bit, so do not refill for them.
                        if self.bits_left < 1 {
                            self.refill(reader)?;
                        }
                        if self.get_bit() == 1 {
                            // check if we already modified it, if so do nothing, otherwise
                            // append the correction bit.
                            if (*coefficient & bit) == 0 {
                                if *coefficient >= 0 {
                                    *coefficient = coefficient.wrapping_add(bit);
                                } else {
                                    *coefficient = coefficient.wrapping_sub(bit);
                                }
                            }
                        }
                    }
                    k += 1;
                }
            }
            // count a block completed in EOB run
            self.eob_run -= 1;
        }

        return Ok(true);
    }

    fn update_progressive_params(&mut self, _ah: u8, al: u8, spec_start: u8, spec_end: u8) {
        self.successive_low_mask = 1i16 << al;
        self.spec_start = spec_start;
        self.spec_end = spec_end;
    }

    /// Reset the stream if we have a restart marker
    ///
    /// Restart markers indicate drop those bits in the stream and zero out
    /// everything
    #[cold]
    fn reset(&mut self) {
        self.bits_left = 0;
        self.marker = None;
        self.buffer = 0;
        self.aligned_buffer = 0;
        self.eob_run = 0;
        // the zeros went with the buffer
        self.zeros_past_end = 0;
        self.marker_bits = None;
        self.insufficient_data = false;
        self.skip_mcu = false;
        self.corrupt_data = false;
    }
}

/// Do the equivalent of JPEG HUFF_EXTEND
#[inline(always)]
fn huff_extend(x: i32, s: i32) -> i32 {
    // if x<s return x else return x+offset[s] where offset[s] = ( (-1<<s)+1)
    (x) + ((((x) - (1 << ((s) - 1))) >> 31) & (((-1) << (s)) + 1))
}

const fn has_zero(v: u32) -> bool {
    // Retrieved from Stanford bithacks
    // @ https://graphics.stanford.edu/~seander/bithacks.html#ZeroInWord
    return !((((v & 0x7F7F_7F7F) + 0x7F7F_7F7F) | v) | 0x7F7F_7F7F) != 0;
}

const fn has_byte(b: u32, val: u8) -> bool {
    // Retrieved from Stanford bithacks
    // @ https://graphics.stanford.edu/~seander/bithacks.html#ZeroInWord
    has_zero(b ^ ((!0_u32 / 255) * (val as u32)))
}

// mod tests {
//     use zune_core::bytestream::ZCursor;
//     use zune_core::colorspace::ColorSpace;
//     use zune_core::options::DecoderOptions;
//
//     use crate::JpegDecoder;
//
//     #[test]
//     fn test_image() {
//         let img = "/Users/etemesi/Downloads/test_IDX_45_RAND_168601280367171438891916_minimized_837.jpg";
//         let data = std::fs::read(img).unwrap();
//         let options = DecoderOptions::new_cmd().jpeg_set_out_colorspace(ColorSpace::RGB);
//         let mut decoder = JpegDecoder::new_with_options(ZCursor::new(&data[..]), options);
//
//         decoder.decode().unwrap();
//         println!("{:?}", decoder.options.jpeg_get_out_colorspace())
//     }
// }
