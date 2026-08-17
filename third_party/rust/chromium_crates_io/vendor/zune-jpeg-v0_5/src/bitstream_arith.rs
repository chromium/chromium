/*
 * Copyright (c) 2026.
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
//! This file exposes a single struct that can decode an arithmetic encoded
//! Bitstream in a JPEG file
//!
//! The current implementation decodes closely following the spec; and skips a number
//! of possible optimizations to avoid branches, batch refill, improve memory
//! layout, etc.
//!
use alloc::format;
use alloc::string::ToString;

use zune_core::bytestream::{ZByteReaderTrait, ZReader};

use crate::bitstream::BitStream;
use crate::decoder::EntropyTables;
use crate::errors::DecodeErrors;
use crate::marker::Marker;
use crate::mcu::DCT_BLOCK;
use crate::misc::UN_ZIGZAG;

/// Table D.3 state machine, (qe, lps, mps, switch)
static STATE_MACHINE: [(u16, u8, u8, u8); 113] = [
    (0x5A1D, 1, 1, 1),
    (0x2586, 14, 2, 0),
    (0x1114, 16, 3, 0),
    (0x080B, 18, 4, 0),
    (0x03D8, 20, 5, 0),
    (0x01DA, 23, 6, 0),
    (0x00E5, 25, 7, 0),
    (0x006F, 28, 8, 0),
    (0x0036, 30, 9, 0),
    (0x001A, 33, 10, 0),
    (0x000D, 35, 11, 0),
    (0x0006, 9, 12, 0),
    (0x0003, 10, 13, 0),
    (0x0001, 12, 13, 0),
    (0x5A7F, 15, 15, 1),
    (0x3F25, 36, 16, 0),
    (0x2CF2, 38, 17, 0),
    (0x207C, 39, 18, 0),
    (0x17B9, 40, 19, 0),
    (0x1182, 42, 20, 0),
    (0x0CEF, 43, 21, 0),
    (0x09A1, 45, 22, 0),
    (0x072F, 46, 23, 0),
    (0x055C, 48, 24, 0),
    (0x0406, 49, 25, 0),
    (0x0303, 51, 26, 0),
    (0x0240, 52, 27, 0),
    (0x01B1, 54, 28, 0),
    (0x0144, 56, 29, 0),
    (0x00F5, 57, 30, 0),
    (0x00B7, 59, 31, 0),
    (0x008A, 60, 32, 0),
    (0x0068, 62, 33, 0),
    (0x004E, 63, 34, 0),
    (0x003B, 32, 35, 0),
    (0x002C, 33, 9, 0),
    (0x5AE1, 37, 37, 1),
    (0x484C, 64, 38, 0),
    (0x3A0D, 65, 39, 0),
    (0x2EF1, 67, 40, 0),
    (0x261F, 68, 41, 0),
    (0x1F33, 69, 42, 0),
    (0x19A8, 70, 43, 0),
    (0x1518, 72, 44, 0),
    (0x1177, 73, 45, 0),
    (0x0E74, 74, 46, 0),
    (0x0BFB, 75, 47, 0),
    (0x09F8, 77, 48, 0),
    (0x0861, 78, 49, 0),
    (0x0706, 79, 50, 0),
    (0x05CD, 48, 51, 0),
    (0x04DE, 50, 52, 0),
    (0x040F, 50, 53, 0),
    (0x0363, 51, 54, 0),
    (0x02D4, 52, 55, 0),
    (0x025C, 53, 56, 0),
    (0x01F8, 54, 57, 0),
    (0x01A4, 55, 58, 0),
    (0x0160, 56, 59, 0),
    (0x0125, 57, 60, 0),
    (0x00F6, 58, 61, 0),
    (0x00CB, 59, 62, 0),
    (0x00AB, 61, 63, 0),
    (0x008F, 61, 32, 0),
    (0x5B12, 65, 65, 1),
    (0x4D04, 80, 66, 0),
    (0x412C, 81, 67, 0),
    (0x37D8, 82, 68, 0),
    (0x2FE8, 83, 69, 0),
    (0x293C, 84, 70, 0),
    (0x2379, 86, 71, 0),
    (0x1EDF, 87, 72, 0),
    (0x1AA9, 87, 73, 0),
    (0x174E, 72, 74, 0),
    (0x1424, 72, 75, 0),
    (0x119C, 74, 76, 0),
    (0x0F6B, 74, 77, 0),
    (0x0D51, 75, 78, 0),
    (0x0BB6, 77, 79, 0),
    (0x0A40, 77, 48, 0),
    (0x5832, 80, 81, 1),
    (0x4D1C, 88, 82, 0),
    (0x438E, 89, 83, 0),
    (0x3BDD, 90, 84, 0),
    (0x34EE, 91, 85, 0),
    (0x2EAE, 92, 86, 0),
    (0x299A, 93, 87, 0),
    (0x2516, 86, 71, 0),
    (0x5570, 88, 89, 1),
    (0x4CA9, 95, 90, 0),
    (0x44D9, 96, 91, 0),
    (0x3E22, 97, 92, 0),
    (0x3824, 99, 93, 0),
    (0x32B4, 99, 94, 0),
    (0x2E17, 93, 86, 0),
    (0x56A8, 95, 96, 1),
    (0x4F46, 101, 97, 0),
    (0x47E5, 102, 98, 0),
    (0x41CF, 103, 99, 0),
    (0x3C3D, 104, 100, 0),
    (0x375E, 99, 93, 0),
    (0x5231, 105, 102, 0),
    (0x4C0F, 106, 103, 0),
    (0x4639, 107, 104, 0),
    (0x415E, 103, 99, 0),
    (0x5627, 105, 106, 1),
    (0x50E7, 108, 107, 0),
    (0x4B85, 109, 103, 0),
    (0x5597, 110, 109, 0),
    (0x504F, 111, 107, 0),
    (0x5A10, 110, 111, 1),
    (0x5522, 112, 109, 0),
    (0x59EB, 112, 111, 1),
];

/// A statistics entry; used to dynamically estimate the local probability of a decision
#[derive(Clone, Copy, Default, Debug)]
struct StatisticsEntry {
    qe_index: u8,
    /// MPS (more probable symbol) register, either false=0 or true=1
    mps: bool,
}

#[derive(Default, Clone, Copy)]
struct DCLeadingBins {
    /// For difference 0 decision
    s0: StatisticsEntry,
    /// For sign decision
    ss: StatisticsEntry,
    /// For first size threshold, + case
    sp: StatisticsEntry,
    /// For first size threshold, - case
    sn: StatisticsEntry,
}

#[derive(Clone, Copy)]
pub(crate) struct ArithDCTables {
    /// Conditionally chosen by previous DCT difference
    ///
    /// Entries: -large, -small, zero-ish, +small, +large
    leading: [DCLeadingBins; 5],
    /// X1..X15 for sz < 2^i
    x: [StatisticsEntry; 15],
    /// M2..M15 magnitude bits
    m: [StatisticsEntry; 14],

    pub(crate) l: u8,
    pub(crate) u: u8,
}

impl Default for ArithDCTables {
    fn default() -> Self {
        Self {
            leading: [DCLeadingBins::default(); 5],
            x: [StatisticsEntry::default(); 15],
            m: [StatisticsEntry::default(); 14],
            // L=0, U=1 per F.1.4.4.1.4
            l: 0,
            u: 1,
        }
    }
}

#[derive(Default, Clone, Copy)]
struct ACLeadingBins {
    /// For EOB decision
    se: StatisticsEntry,
    /// For "value zero" decision
    s0: StatisticsEntry,
    /// Has multiple roles (first size threshold, x1, refinement bits)
    spnx1: StatisticsEntry,
}

#[derive(Clone, Copy)]
pub(crate) struct ArithACTables {
    v: [ACLeadingBins; 63],
    /// X2..X15 for sz < 2^i, at <= Kx
    x_lo: [StatisticsEntry; 14],
    /// M2..M15 magnitude bits,
    m_lo: [StatisticsEntry; 14],
    /// X2..X15 for sz < 2^i, above Kx
    x_hi: [StatisticsEntry; 14],
    /// M2..M15 magnitude bits
    m_hi: [StatisticsEntry; 14],

    pub(crate) kx: u8,
}

impl Default for ArithACTables {
    fn default() -> Self {
        Self {
            v: [ACLeadingBins::default(); 63],
            x_lo: [StatisticsEntry::default(); 14],
            m_lo: [StatisticsEntry::default(); 14],
            x_hi: [StatisticsEntry::default(); 14],
            m_hi: [StatisticsEntry::default(); 14],
            // Kx=5 by default per F.1.4.4.2.1
            kx: 5,
        }
    }
}

/// A `BitStream` struct, a bit by bit reader with super powers
///
#[rustfmt::skip]
pub(crate) struct BitStreamArithmetic {
    /// Have the first two bytes been fed in?
    initialized: bool,
    /// `A` register. 0x10000 is initial/max value
    a: u32,
    /// `C` register, both high and low parts
    c: u32,
    /// Number of compressed bits in low part of C, range 0-7 (8 temporarily)
    ct: u8,
    /// Did we find a marker(RST/EOF) during decoding?
    marker:              Option<Marker>,
    /// An i16 with the bit corresponding to successive_low set to 1, others 0.
    successive_low_mask: i16, // progressive mode control
    spec_start:          u8, // progressive mode control
    spec_end:            u8, // progressive mode control
    eob_run:             i32,
    overread_by:         usize,
    /// True if we have seen end of image marker.
    /// Don't read anything after that.
    seen_eoi:            bool,
}

impl BitStreamArithmetic {
    fn init_dec<T>(&mut self, reader: &mut ZReader<T>) -> Result<(), DecodeErrors>
    where
        T: ZByteReaderTrait,
    {
        self.a = 0x10000;
        self.c = 0;

        self.byte_in(reader)?;
        self.c <<= 8;
        self.byte_in(reader)?;
        self.c <<= 8;
        self.ct = 0;
        self.initialized = true;
        Ok(())
    }

    fn byte_in<T>(&mut self, reader: &mut ZReader<T>) -> Result<(), DecodeErrors>
    where
        T: ZByteReaderTrait,
    {
        let b = if self.marker.is_some() {
            // When a marker is present, the decoder is repeatedly fed zero bytes
            0
        } else {
            reader.read_u8_err()?
        };

        if b == 0xFF {
            let b2 = reader.read_u8_err()?;
            if b2 == 0 {
                self.c |= 0xFF00;
            } else {
                // Found a marker
                self.marker = Marker::from_u8(b2);
            }
        } else {
            self.c += u32::from(b) << 8;
        }

        Ok(())
    }

    fn cond_lps_exchange(&mut self, context: &mut StatisticsEntry) -> u8 {
        let (qe, lps_next, mps_next, switch) = STATE_MACHINE[context.qe_index as usize];

        let d: u8;
        if self.a < u32::from(qe) {
            d = u8::from(context.mps);
            self.c -= self.a << 16;
            self.a = u32::from(qe);
            context.qe_index = mps_next;
        } else {
            d = 1 - u8::from(context.mps);
            self.c -= self.a << 16;
            self.a = u32::from(qe);
            context.mps ^= switch != 0;
            context.qe_index = lps_next;
        }

        d
    }

    fn cond_mps_exchange(&mut self, context: &mut StatisticsEntry) -> u8 {
        let (qe, lps_next, mps_next, switch) = STATE_MACHINE[context.qe_index as usize];

        let d: u8;
        if self.a < u32::from(qe) {
            d = 1 - u8::from(context.mps);
            context.mps ^= switch != 0;
            context.qe_index = lps_next;
        } else {
            d = u8::from(context.mps);
            context.qe_index = mps_next;
        }

        d
    }

    fn renorm_d<T>(&mut self, reader: &mut ZReader<T>) -> Result<(), DecodeErrors>
    where
        T: ZByteReaderTrait,
    {
        loop {
            if self.ct == 0 {
                self.byte_in(reader)?;
                self.ct = 8;
            }
            self.a <<= 1;
            self.c <<= 1;
            self.ct -= 1;
            if self.a >= 0x8000 {
                return Ok(());
            }
        }
    }

    /// Decode the next bit, using the provided context index
    fn decode_bit<T>(
        &mut self, context: &mut StatisticsEntry, reader: &mut ZReader<T>,
    ) -> Result<u8, DecodeErrors>
    where
        T: ZByteReaderTrait,
    {
        if !self.initialized {
            self.init_dec(reader)?;
        }

        let qe = u32::from(STATE_MACHINE[context.qe_index as usize].0);
        self.a -= qe;

        let cx = (self.c >> 16) as u16;
        let d = if u32::from(cx) < self.a {
            if self.a < 0x8000 {
                let d = self.cond_mps_exchange(context);
                self.renorm_d(reader)?;
                d
            } else {
                u8::from(context.mps)
            }
        } else {
            let d = self.cond_lps_exchange(context);
            self.renorm_d(reader)?;
            d
        };
        Ok(d)
    }

    /// Decode the DC coefficient in a MCU block.
    ///
    /// The decoded coefficient is written to `dc_prediction` and the difference to `last_dc_diff`.
    ///
    #[allow(
        clippy::cast_possible_truncation,
        clippy::cast_sign_loss,
        clippy::unwrap_used
    )]
    #[inline(always)]
    fn decode_dc<T>(
        &mut self, reader: &mut ZReader<T>, dc_table: &mut ArithDCTables, dc_prediction: &mut i32,
        last_dc_diff: &mut i32,
    ) -> Result<bool, DecodeErrors>
    where
        T: ZByteReaderTrait,
    {
        let prev_dc_difference: i32 = *last_dc_diff;
        let lthresh = i32::from(if dc_table.l == 0 { 0 } else { 1u16 << (dc_table.l - 1) });
        let uthresh = i32::from(1u16 << dc_table.u);
        let size_class: usize = if prev_dc_difference.abs() <= lthresh {
            2
        } else if prev_dc_difference.abs() <= uthresh {
            if prev_dc_difference < 0 {
                1
            } else {
                3
            }
        } else if prev_dc_difference < 0 {
            0
        } else {
            4
        };

        let leading_bins = &mut dc_table.leading[size_class];

        let d = self.decode_bit(&mut leading_bins.s0, reader)?;
        let dc_diff: i32 = if d == 0 { 0 } else { self.decode_v_dc(size_class, dc_table, reader)? };

        // a fully strict decoder would reject overflow
        *dc_prediction = dc_prediction.wrapping_add(dc_diff);
        *last_dc_diff = dc_diff;

        return Ok(true);
    }

    fn decode_v_dc<T>(
        &mut self, size_class: usize, dc_table: &mut ArithDCTables, reader: &mut ZReader<T>,
    ) -> Result<i32, DecodeErrors>
    where
        T: ZByteReaderTrait,
    {
        let ss = &mut dc_table.leading[size_class].ss;
        let sign_bit = self.decode_bit(ss, reader)?;
        let sign = if sign_bit == 0 { 1 } else { -1 };

        let s = if sign_bit == 1 {
            &mut dc_table.leading[size_class].sn
        } else {
            &mut dc_table.leading[size_class].sp
        };
        let mut m;
        let mut d = self.decode_bit(s, reader)?;

        let m_ctx: &mut StatisticsEntry = if d != 0 {
            let x1 = &mut dc_table.x[0];
            d = self.decode_bit(x1, reader)?;
            if d != 0 {
                m = 4;
                let mut j = 1;
                loop {
                    let xj = &mut dc_table.x[j]; // j=1 ->
                    d = self.decode_bit(xj, reader)?;
                    if d == 0 {
                        break &mut dc_table.m[j - 1];
                    }
                    m <<= 1;
                    j += 1;
                    if j >= 15 {
                        return Err(DecodeErrors::ArithmeticDecode(
                            "Arithmetic decoding of DC coefficient difference overflowed"
                                .to_string(),
                        ));
                    }
                }
            } else {
                return Ok(2 * sign);
            }
        } else {
            return Ok(sign);
        };
        m >>= 1;
        let mut sz = m;

        loop {
            m >>= 1;
            if m == 0 {
                break;
            }
            d = self.decode_bit(m_ctx, reader)?;
            if d != 0 {
                sz |= m;
            }
        }
        Ok((sz + 1) * sign)
    }

    fn decode_v_ac<T>(
        &mut self, pos: u8, k_low: bool, ac_table: &mut ArithACTables, reader: &mut ZReader<T>,
    ) -> Result<i32, DecodeErrors>
    where
        T: ZByteReaderTrait,
    {
        assert!((1..=63).contains(&pos));

        // sign bit uses default probability
        let mut ss = StatisticsEntry::default();
        let sign_bit = self.decode_bit(&mut ss, reader)?;
        let sign = if sign_bit == 0 { 1 } else { -1 };

        let s = &mut ac_table.v[(pos - 1) as usize].spnx1;
        let mut m;
        let mut d = self.decode_bit(s, reader)?;

        let (xtable, mtable) = if k_low {
            (&mut ac_table.x_lo, &mut ac_table.m_lo)
        } else {
            (&mut ac_table.x_hi, &mut ac_table.m_hi)
        };

        let m_ctx: &mut StatisticsEntry = if d != 0 {
            let x1 = &mut ac_table.v[(pos - 1) as usize].spnx1;
            d = self.decode_bit(x1, reader)?;
            if d != 0 {
                m = 4;
                let mut j = 0;
                loop {
                    let xj = &mut xtable[j];
                    d = self.decode_bit(xj, reader)?;
                    if d == 0 {
                        break &mut mtable[j];
                    }
                    m <<= 1;
                    j += 1;
                    if j >= 14 {
                        return Err(DecodeErrors::ArithmeticDecode(
                            "Arithmetic decoding of AC coefficient value overflowed".to_string(),
                        ));
                    }
                }
            } else {
                return Ok(2 * sign);
            }
        } else {
            return Ok(sign);
        };
        m >>= 1;
        let mut sz = m;

        loop {
            m >>= 1;
            if m == 0 {
                break;
            }
            d = self.decode_bit(m_ctx, reader)?;
            if d != 0 {
                sz |= m;
            }
        }
        Ok((sz + 1) * sign)
    }
}

/// Minimal snapshot for arithmetic bitstream — only captures enough to detect
/// whether the stream was exhausted. True per-MCU resume is not feasible for
/// arithmetic coding due to statistical context coupling.
#[derive(Clone, Copy)]
pub(crate) struct ArithBitstreamState {
    pub(crate) marker:      Option<Marker>,
    pub(crate) overread_by: usize,
    pub(crate) seen_eoi:    bool,
    pub(crate) eob_run:     i32
}

impl BitStream for BitStreamArithmetic {
    /// Arithmetic coding does not support per-MCU checkpoint/restore because
    /// the A/C/CT registers are tightly coupled to the statistical context
    /// tables which mutate during decoding. Use RST/scan-start granularity.
    type State = ArithBitstreamState;
    type DCEntropyTable = ArithDCTables;
    type ACEntropyTable = ArithACTables;

    #[inline(always)]
    fn get_dc_table(
        tables: &mut EntropyTables, dc_pos: usize,
    ) -> Result<&mut Self::DCEntropyTable, DecodeErrors> {
        tables.dc_arithmetic.get_mut(dc_pos).ok_or_else(|| {
            DecodeErrors::Format(format!(
                "No arithmetic coding conditioning table for DC component:{dc_pos}"
            ))
        })
    }

    #[inline(always)]
    fn get_ac_table(
        tables: &mut EntropyTables, ac_pos: usize,
    ) -> Result<&mut Self::ACEntropyTable, DecodeErrors> {
        tables.ac_arithmetic.get_mut(ac_pos).ok_or_else(|| {
            DecodeErrors::Format(format!(
                "No arithmetic coding conditioning table for AC component:{ac_pos}"
            ))
        })
    }

    #[inline(always)]
    fn get_dc_ac_tables(
        tables: &mut EntropyTables, dc_pos: usize, ac_pos: usize,
    ) -> Result<(&mut Self::DCEntropyTable, &mut Self::ACEntropyTable), DecodeErrors> {
        Ok((
            tables.dc_arithmetic.get_mut(dc_pos).ok_or_else(|| {
                DecodeErrors::Format(format!(
                    "No arithmetic coding conditioning table for DC component:{dc_pos}"
                ))
            })?,
            tables.ac_arithmetic.get_mut(ac_pos).ok_or_else(|| {
                DecodeErrors::Format(format!(
                    "No arithmetic coding conditioning table for AC component:{ac_pos}"
                ))
            })?,
        ))
    }

    fn reset_arith_tables(tables: &mut EntropyTables) {
        for dc in &mut tables.dc_arithmetic {
            let d = ArithDCTables::default();
            dc.leading = d.leading;
            dc.x = d.x;
            dc.m = d.m;
        }

        for ac in &mut tables.ac_arithmetic {
            let a = ArithACTables::default();
            ac.v = a.v;
            ac.x_lo = a.x_lo;
            ac.x_hi = a.x_hi;
            ac.m_lo = a.m_lo;
            ac.m_hi = a.m_hi;
        }
    }

    /// Create a new BitStream
    #[rustfmt::skip]
    #[inline(always)]
    fn new() -> BitStreamArithmetic {
        BitStreamArithmetic {
            initialized: false,
            a: 0x10000,
            c: 0,
            ct: 0,
            marker:              None,
            successive_low_mask: 1,
            spec_start:          0,
            spec_end:            0,
            eob_run:             0,
            overread_by:         0,
            seen_eoi:            false,
        }
    }

    /// Create a new Bitstream for progressive decoding
    #[allow(clippy::redundant_field_names)]
    #[rustfmt::skip]
    #[inline(always)]
    fn new_progressive(al: u8, spec_start: u8, spec_end: u8) -> BitStreamArithmetic {
        BitStreamArithmetic {
            initialized: false,
            a: 0x10000,
            c: 0,
            ct: 0,
            marker:              None,
            successive_low_mask: 1i16 << al,
            spec_start:          spec_start,
            spec_end:            spec_end,
            eob_run:             0,
            overread_by:         0,
            seen_eoi:            false,
        }
    }

    #[inline(always)]
    fn overread_by(&self) -> usize {
        self.overread_by
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
        self.ct
    }

    #[inline(always)]
    fn supports_mcu_checkpoint() -> bool {
        false
    }

    #[inline(always)]
    fn save_state(&self) -> ArithBitstreamState {
        ArithBitstreamState {
            marker:      self.marker,
            overread_by: self.overread_by,
            seen_eoi:    self.seen_eoi,
            eob_run:     self.eob_run
        }
    }

    /// Arithmetic restore is a no-op for the A/C/CT registers — those are
    /// only reset via `reset()` at RST/scan boundaries. This restores the
    /// marker/EOF detection state only.
    #[inline(always)]
    fn restore_state(&mut self, state: ArithBitstreamState) {
        self.marker = state.marker;
        self.overread_by = state.overread_by;
        self.seen_eoi = state.seen_eoi;
        self.eob_run = state.eob_run;
    }

    #[inline(always)]
    fn snapshot_state(&self) -> crate::bitstream::BitstreamStateSnapshot {
        crate::bitstream::BitstreamStateSnapshot::Arithmetic(self.save_state())
    }

    #[inline(always)]
    fn restore_snapshot(&mut self, snapshot: crate::bitstream::BitstreamStateSnapshot) {
        match snapshot {
            crate::bitstream::BitstreamStateSnapshot::Arithmetic(s) => self.restore_state(s),
            crate::bitstream::BitstreamStateSnapshot::None => {}
            crate::bitstream::BitstreamStateSnapshot::Huffman(_) => unreachable!("Arithmetic stream given Huffman snapshot"),
        }
    }

    /// Refill the bit buffer by (a maximum of) 32 bits
    ///
    /// # Arguments
    ///  - `reader`:`&mut BufReader<R>`: A mutable reference to an underlying
    ///    File/Memory buffer containing a valid JPEG stream
    ///
    /// This function will only refill if `self.count` is less than 32
    #[inline(always)]
    fn refill<T>(&mut self, _reader: &mut ZReader<T>) -> Result<bool, DecodeErrors>
    where
        T: ZByteReaderTrait,
    {
        // This optimization not implemented for arithmetic coding.
        return Ok(true);
    }

    /// Decode a Minimum Code Unit(MCU) as quickly as possible
    ///
    /// # Arguments
    /// - reader: The bitstream from where we read more bits.
    /// - dc_table: The arithmetic coding conditioning table used to decode the DC coefficient
    /// - ac_table: The arithmetic coding conditioning table used to decode AC values
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
        &mut self, reader: &mut ZReader<T>, dc_table: &mut ArithDCTables,
        ac_table: &mut ArithACTables, qt_table: &[i32; DCT_BLOCK], block: &mut [i32; 64],
        dc_prediction: &mut i32, last_dc_diff: &mut i32,
    ) -> Result<u16, DecodeErrors>
    where
        T: ZByteReaderTrait,
    {
        self.decode_dc(reader, dc_table, dc_prediction, last_dc_diff)?;
        block[0] = dc_prediction.wrapping_mul(qt_table[0]);

        let mut pos: u8 = 1;
        while pos < 64 {
            // EOB decision
            let dse = self.decode_bit(&mut ac_table.v[(pos - 1) as usize].se, reader)?;
            if dse == 1 {
                // Leave remaining bins as zeros
                break;
            }

            loop {
                let ds0 = self.decode_bit(&mut ac_table.v[(pos - 1) as usize].s0, reader)?;
                if ds0 == 0 {
                    // This entry is zero
                    pos += 1;
                    if pos >= 64 {
                        return Err(DecodeErrors::ArithmeticDecode(
                            "Overrun while decoding MCU, did not find end of block".to_string(),
                        ));
                    }
                } else {
                    break;
                }
            }

            let symbol = self.decode_v_ac(pos, pos <= ac_table.kx, ac_table, reader)?;

            let t_pos = UN_ZIGZAG[pos as usize & 63] & 63;

            block[t_pos] = symbol * qt_table[t_pos];
            pos += 1;
        }

        Ok(u16::from(pos))
    }

    /// Advance the bitstream over a block but ignore the data contained.
    ///
    /// This updates DC prediction but we never dequantize and we never do any Zig-Zag translation
    /// either. Still returns the index of the last component read.
    fn discard_mcu_block<T>(
        &mut self, reader: &mut ZReader<T>, dc_table: &mut ArithDCTables,
        ac_table: &mut ArithACTables, last_dc_diff: &mut i32,
    ) -> Result<u16, DecodeErrors>
    where
        T: ZByteReaderTrait,
    {
        let mut block = [0i32; 64];
        let qt_table = [0i32; DCT_BLOCK];
        let mut dc_prediction = 0;
        self.decode_mcu_block(
            reader,
            dc_table,
            ac_table,
            &qt_table,
            &mut block,
            &mut dc_prediction,
            last_dc_diff,
        )
    }

    /// Decode a DC block
    #[allow(clippy::cast_possible_truncation)]
    #[inline]
    fn decode_prog_dc_first<T>(
        &mut self, reader: &mut ZReader<T>, dc_table: &mut ArithDCTables, block: &mut i16,
        dc_prediction: &mut i32, last_dc_diff: &mut i32,
    ) -> Result<(), DecodeErrors>
    where
        T: ZByteReaderTrait,
    {
        self.decode_dc(reader, dc_table, dc_prediction, last_dc_diff)?;
        *block = (*dc_prediction as i16).wrapping_mul(self.successive_low_mask);
        return Ok(());
    }

    #[inline]
    fn decode_prog_dc_refine<T>(
        &mut self, reader: &mut ZReader<T>, block: &mut i16,
    ) -> Result<(), DecodeErrors>
    where
        T: ZByteReaderTrait,
    {
        // G1.3.1: least significant bits encoded with fixed probability estimate.
        let mut fixed_prob = StatisticsEntry {
            qe_index: 0,
            mps: false,
        };
        if self.decode_bit(&mut fixed_prob, reader)? == 1 {
            *block = block.wrapping_add(self.successive_low_mask);
        }

        Ok(())
    }

    fn decode_mcu_ac_first<T>(
        &mut self, reader: &mut ZReader<T>, ac_table: &mut ArithACTables, block: &mut [i16; 64],
    ) -> Result<bool, DecodeErrors>
    where
        T: ZByteReaderTrait,
    {
        let bit = self.successive_low_mask;

        let mut pos: u8 = self.spec_start;

        while pos <= self.spec_end {
            let dse = self.decode_bit(&mut ac_table.v[(pos - 1) as usize].se, reader)?;
            if dse == 1 {
                // Leave remaining bins as zeros
                break;
            }

            loop {
                let ds0 = self.decode_bit(&mut ac_table.v[(pos - 1) as usize].s0, reader)?;
                if ds0 == 0 {
                    // This entry is zero
                    pos += 1;
                    if pos > self.spec_end {
                        // Match libjpeg-turbo's JWRN_ARITH_BAD_CODE path: keep
                        // coefficients decoded so far, stop reading this scan,
                        // skip its remaining MCUs, and reinitialize arithmetic
                        // state at the next SOS. libjpeg does the same by
                        // setting `ct = -1`; later MCU calls become no-ops.
                        return Ok(false);
                    }
                } else {
                    break;
                }
            }

            let symbol = self.decode_v_ac(pos, pos <= ac_table.kx, ac_table, reader)?;

            let t_pos = UN_ZIGZAG[pos as usize & 63] & 63;

            block[t_pos] = (symbol as i16).wrapping_mul(bit);
            pos += 1;
        }
        Ok(true)
    }

    #[allow(clippy::too_many_lines, clippy::op_ref)]
    fn decode_mcu_ac_refine<T>(
        &mut self, reader: &mut ZReader<T>, ac_table: &mut ArithACTables, block: &mut [i16; 64],
    ) -> Result<bool, DecodeErrors>
    where
        T: ZByteReaderTrait,
    {
        let bit = self.successive_low_mask;

        let mut k = self.spec_start;
        assert!(k >= 1);

        // Derive end of band index from block contents (the first zero position
        // of the trailing run, or spec_end.)
        let mut eobx: u8 = self.spec_end + 1;
        while eobx > 0 && block[UN_ZIGZAG[(eobx - 1) as usize & 63]] == 0 {
            eobx -= 1;
        }

        loop {
            if k >= eobx {
                let dse = self.decode_bit(&mut ac_table.v[(k - 1) as usize].se, reader)?;
                if dse == 1 {
                    // Leave remaining bins unmodified (as zeros)
                    break;
                }
            }

            // Scan until next coefficient which will be nonzero after decoding
            let first_bit = loop {
                if block[UN_ZIGZAG[k as usize & 63] & 63] == 0 {
                    // S0 decision: will the initial bit be addded?
                    let ds0 = self.decode_bit(&mut ac_table.v[(k - 1) as usize].s0, reader)?;
                    if ds0 == 0 {
                        // This entry is zero, no changes
                        k += 1;

                        if k > self.spec_end {
                            return Err(DecodeErrors::ArithmeticDecode(
                                "Overrun while refining AC coefficients, did not find value before end of band".to_string()
                            ));
                        }
                    } else {
                        break true;
                    }
                } else {
                    // Always code a bit for nonzero entries
                    break false;
                }
            };

            // Note: the coefficient should satisfy (coef & bit == 0) in practice,
            // but the calling code does not currently check that all scans have
            // disjoint frequency bands and bits, so this cannot be relied upon.
            // These aren't valid images, so it's fine to output garbage (without
            // crashing).
            let coefficient = &mut block[UN_ZIGZAG[k as usize & 63] & 63];
            if first_bit {
                let mut ss = StatisticsEntry {
                    qe_index: 0,
                    mps: false,
                };
                let dss = self.decode_bit(&mut ss, reader)?;
                let sign: i16 = if dss == 0 { 1 } else { -1 };
                *coefficient = sign.wrapping_mul(bit);
            } else {
                let dsc = self.decode_bit(&mut ac_table.v[(k - 1) as usize].spnx1, reader)?;

                if dsc == 1 && (*coefficient & bit) == 0 {
                    if *coefficient > 0 {
                        *coefficient = coefficient.wrapping_add(bit);
                    } else {
                        *coefficient = coefficient.wrapping_sub(bit);
                    }
                }
            }

            k += 1;

            if k > self.spec_end {
                break;
            }
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
        self.marker = None;
        self.eob_run = 0;

        self.a = 0x10000;
        self.c = 0;
        self.ct = 0;
        self.initialized = false;
    }
}

#[cfg(test)]
mod tests {
    use zune_core::bytestream::ZReader;

    use crate::bitstream::BitStream;
    use crate::bitstream_arith::{BitStreamArithmetic, StatisticsEntry, STATE_MACHINE};

    #[test]
    fn reference_decode() {
        // Test sequence from K.4.1
        let decode_output: [u8; 32] = [
            0x00, 0x02, 0x00, 0x51, 0x00, 0x00, 0x00, 0xC0, 0x03, 0x52, 0x87, 0x2A, 0xAA, 0xAA,
            0xAA, 0xAA, 0x82, 0xC0, 0x20, 0x00, 0xFC, 0xD7, 0x9E, 0xF6, 0x74, 0xEA, 0xAB, 0xF7,
            0x69, 0x7E, 0xE7, 0x4C,
        ];
        let compressed: [u8; 32] = [
            0x65, 0x5B, 0x51, 0x44, 0xF7, 0x96, 0x9D, 0x51, 0x78, 0x55, 0xBF, 0xFF, 0x00, 0xFC,
            0x51, 0x84, 0xC7, 0xCE, 0xF9, 0x39, 0x00, 0x28, 0x7D, 0x46, 0x70, 0x8E, 0xCB, 0xC0,
            0xF6, 0xFF, 0xD9, 0x00,
        ];

        let input = std::io::Cursor::new(&compressed);
        let mut bitstream = BitStreamArithmetic::new();
        let mut output = [0u8; 32];
        let mut reader = ZReader::new(input);
        let mut context = StatisticsEntry {
            qe_index: 0,
            mps: false,
        };

        for i in 0..256 {
            println!(
                "{} {:x} {:x} {:x} {}",
                i,
                STATE_MACHINE[context.qe_index as usize].0,
                bitstream.a,
                bitstream.c,
                bitstream.ct
            );
            let val = bitstream.decode_bit(&mut context, &mut reader).unwrap();
            output[i / 8] |= val << (7 - i % 8);
        }
        assert_eq!(output, decode_output);
    }
}
