// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::fmt::Debug;

use crate::bit_reader::BitReader;
use crate::entropy_coding::decode::*;
use crate::error::{Error, Result};
use crate::util::{CeilLog2, NewWithCapacity, tracing_wrappers::*};

pub const HUFFMAN_MAX_BITS: usize = 15;
const TABLE_BITS: usize = 8;
const TABLE_SIZE: usize = 1 << TABLE_BITS;
const CODE_LENGTHS_CODE: usize = 18;
const DEFAULT_CODE_LENGTH: u8 = 8;
const CODE_LENGTH_REPEAT_CODE: u8 = 16;

#[derive(Clone, Copy)]
struct TableEntry {
    bits: u8,
    value: u16,
}

impl Debug for TableEntry {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}l{}", self.value, self.bits)
    }
}

#[derive(Debug)]
struct Table {
    entries: Vec<TableEntry>,
}

/* Returns reverse(reverse(key, len) + 1, len), where reverse(key, len) is the
bit-wise reversal of the len least significant bits of key. */
fn get_next_key(key: u32, len: usize) -> u32 {
    let mut step = 1 << (len - 1);
    while key & step != 0 {
        step >>= 1;
    }
    (key & (step.wrapping_sub(1))) + step
}

/* Stores code in table[0], table[step], table[2*step], ..., table[end] */
/* Assumes that end is an integer multiple of step */
fn replicate_value(table: &mut [TableEntry], step: usize, value: TableEntry) {
    for v in table.iter_mut().step_by(step) {
        *v = value;
    }
}

/* Returns the table width of the next 2nd level table. count is the histogram
of bit lengths for the remaining symbols, len is the code length of the next
processed symbol */
fn next_table_bit_size(count: &[u16], len: usize, root_bits: usize) -> usize {
    let mut len = len;
    let mut left = 1 << (len - root_bits);
    while len < HUFFMAN_MAX_BITS {
        if left <= count[len] {
            break;
        }
        left -= count[len];
        len += 1;
        left <<= 1;
    }
    len - root_bits
}

impl Table {
    fn decode_simple_table(al_size: usize, br: &mut BitReader) -> Result<Vec<TableEntry>> {
        let max_bits = al_size.ceil_log2();
        let num_symbols = (br.read(2)? + 1) as usize;
        let mut symbols = [0u16; 4];
        for symbol in symbols.iter_mut().take(num_symbols) {
            let sym = br.read(max_bits)? as usize;
            if sym >= al_size {
                return Err(Error::InvalidHuffman);
            }
            *symbol = sym as u16;
        }
        if (0..num_symbols - 1).any(|i| symbols[..i].contains(&symbols[i + 1])) {
            return Err(Error::InvalidHuffman);
        }

        let special_4_symbols = if num_symbols == 4 {
            br.read(1)? != 0
        } else {
            false
        };
        debug!(symbols = ?symbols[..num_symbols]);
        match (num_symbols, special_4_symbols) {
            (1, _) => Ok(vec![
                TableEntry {
                    bits: 0,
                    value: symbols[0]
                };
                TABLE_SIZE
            ]),
            (2, _) => {
                let mut ret = Vec::new_with_capacity(TABLE_SIZE)?;
                symbols[0..2].sort_unstable();
                for _ in 0..(TABLE_SIZE >> 1) {
                    ret.push(TableEntry {
                        bits: 1,
                        value: symbols[0],
                    });
                    ret.push(TableEntry {
                        bits: 1,
                        value: symbols[1],
                    });
                }
                Ok(ret)
            }
            (3, _) => {
                let mut ret = Vec::new_with_capacity(TABLE_SIZE)?;
                symbols[1..3].sort_unstable();
                for _ in 0..(TABLE_SIZE >> 2) {
                    ret.push(TableEntry {
                        bits: 1,
                        value: symbols[0],
                    });
                    ret.push(TableEntry {
                        bits: 2,
                        value: symbols[1],
                    });
                    ret.push(TableEntry {
                        bits: 1,
                        value: symbols[0],
                    });
                    ret.push(TableEntry {
                        bits: 2,
                        value: symbols[2],
                    });
                }
                Ok(ret)
            }
            (4, false) => {
                let mut ret = Vec::new_with_capacity(TABLE_SIZE)?;
                symbols.sort_unstable();
                for _ in 0..(TABLE_SIZE >> 2) {
                    ret.push(TableEntry {
                        bits: 2,
                        value: symbols[0],
                    });
                    ret.push(TableEntry {
                        bits: 2,
                        value: symbols[2],
                    });
                    ret.push(TableEntry {
                        bits: 2,
                        value: symbols[1],
                    });
                    ret.push(TableEntry {
                        bits: 2,
                        value: symbols[3],
                    });
                }
                Ok(ret)
            }
            (4, true) => {
                let mut ret = Vec::new_with_capacity(TABLE_SIZE)?;
                symbols[2..4].sort_unstable();
                for _ in 0..(TABLE_SIZE >> 3) {
                    ret.push(TableEntry {
                        bits: 1,
                        value: symbols[0],
                    });
                    ret.push(TableEntry {
                        bits: 2,
                        value: symbols[1],
                    });
                    ret.push(TableEntry {
                        bits: 1,
                        value: symbols[0],
                    });
                    ret.push(TableEntry {
                        bits: 3,
                        value: symbols[2],
                    });
                    ret.push(TableEntry {
                        bits: 1,
                        value: symbols[0],
                    });
                    ret.push(TableEntry {
                        bits: 2,
                        value: symbols[1],
                    });
                    ret.push(TableEntry {
                        bits: 1,
                        value: symbols[0],
                    });
                    ret.push(TableEntry {
                        bits: 3,
                        value: symbols[3],
                    });
                }
                Ok(ret)
            }
            _ => unreachable!(),
        }
    }

    fn decode_huffman_code_lengths(
        code_length_code_lengths: [u8; CODE_LENGTHS_CODE],
        al_size: usize,
        br: &mut BitReader,
    ) -> Result<Vec<u8>> {
        let table = Table::build(5, &code_length_code_lengths)?;

        let mut symbol = 0;
        let mut prev_code_len = DEFAULT_CODE_LENGTH;
        let mut repeat = 0usize;
        let mut repeat_code_len = 0;
        let mut space = 1usize << 15;

        let mut code_lengths = vec![0u8; al_size];

        while symbol < al_size && space > 0 {
            let idx = br.peek(5) as usize;
            br.consume(table[idx].bits as usize)?;
            let code_len = table[idx].value as u8;
            if code_len < CODE_LENGTH_REPEAT_CODE {
                repeat = 0;
                code_lengths[symbol] = code_len;
                symbol += 1;
                if code_len != 0 {
                    prev_code_len = code_len;
                    space = space
                        .checked_sub(32768usize >> code_len)
                        .ok_or(Error::InvalidHuffman)?;
                }
            } else {
                let extra_bits = code_len - 14;

                let new_len = if code_len == CODE_LENGTH_REPEAT_CODE {
                    prev_code_len
                } else {
                    0
                };
                if repeat_code_len != new_len {
                    repeat = 0;
                    repeat_code_len = new_len;
                }
                let old_repeat = repeat;
                if repeat > 0 {
                    repeat -= 2;
                    repeat <<= extra_bits;
                }
                repeat += br.read(extra_bits as usize)? as usize + 3;
                let repeat_delta = repeat - old_repeat;
                if symbol + repeat_delta > al_size {
                    return Err(Error::InvalidHuffman);
                }
                for i in 0..repeat_delta {
                    code_lengths[symbol + i] = repeat_code_len;
                }
                symbol += repeat_delta;
                if repeat_code_len != 0 {
                    space = space
                        .checked_sub(repeat_delta << (15 - repeat_code_len))
                        .ok_or(Error::InvalidHuffman)?;
                }
            }
        }
        if space != 0 {
            return Err(Error::InvalidHuffman);
        }
        Ok(code_lengths)
    }

    #[instrument(level = "trace", ret, err)]
    fn build(root_bits: usize, code_lengths: &[u8]) -> Result<Vec<TableEntry>> {
        if code_lengths.len() > 1 << HUFFMAN_MAX_BITS {
            return Err(Error::InvalidHuffman);
        }
        let mut counts = [0u16; HUFFMAN_MAX_BITS + 1];
        for &v in code_lengths.iter() {
            counts[v as usize] += 1;
        }

        /* symbols sorted by code length */
        let mut sorted = vec![0u16; code_lengths.len()];

        /* offsets in sorted table for each length */
        let mut offset = [0; HUFFMAN_MAX_BITS + 1];
        let mut max_length = 1;

        /* generate offsets into sorted symbol table by code length */
        {
            let mut sum = 0;
            for len in 1..=HUFFMAN_MAX_BITS {
                offset[len] = sum;
                if counts[len] != 0 {
                    sum += counts[len];
                    max_length = len;
                }
            }
        }

        /* sort symbols by length, by symbol order within each length */
        for (symbol, len) in code_lengths.iter().enumerate() {
            if *len != 0 {
                sorted[offset[*len as usize] as usize] = symbol as u16;
                offset[*len as usize] += 1;
            }
        }

        let mut table_bits = root_bits;
        let mut table_size = 1 << table_bits;
        let mut table_pos = 0;
        let mut table = vec![TableEntry { bits: 0, value: 0 }; table_size];

        /* special case code with only one value */
        if offset[HUFFMAN_MAX_BITS] == 1 {
            for v in table.iter_mut() {
                v.bits = 0;
                v.value = sorted[0];
            }
            return Ok(table);
        }

        /* fill in root table */
        /* let's reduce the table size to a smaller size if possible, and */
        /* create the repetitions by memcpy if possible in the coming loop */
        if table_bits > max_length {
            table_bits = max_length;
            table_size = 1 << table_bits;
        }
        let mut key = 0u32;
        let mut symbol = 0;
        let mut bits = 1u8;
        let mut step = 2;
        loop {
            loop {
                if counts[bits as usize] == 0 {
                    break;
                }
                let value = sorted[symbol];
                symbol += 1;
                replicate_value(&mut table[key as usize..], step, TableEntry { bits, value });
                key = get_next_key(key, bits as usize);
                counts[bits as usize] -= 1;
            }
            step <<= 1;
            bits += 1;
            if bits as usize > table_bits {
                break;
            }
        }

        /* if root_bits != table_bits we only created one fraction of the */
        /* table, and we need to replicate it now. */
        while table.len() != table_size {
            for i in 0..table_size {
                table[i + table_size] = table[i];
            }
            table_size <<= 1;
        }
        trace!("table of length {}, table_size: {table_size}", table.len());

        /* fill in 2nd level tables and add pointers to root table */
        let mask = (table.len() - 1) as u32;
        let mut low = !0u32;
        let mut step = 2;
        for len in root_bits + 1..=max_length {
            loop {
                if counts[len] == 0 {
                    break;
                }
                if (key & mask) != low {
                    table_pos += table_size;
                    table_bits = next_table_bit_size(&counts, len, root_bits);
                    table_size = 1 << table_bits;
                    low = key & mask;
                    table[low as usize].bits = (table_bits + root_bits) as u8;
                    table[low as usize].value = (table_pos - low as usize) as u16;
                    if table.len() < table_pos + table_size {
                        table.resize(table_pos + table_size, TableEntry { bits: 0, value: 0 });
                    }
                }
                counts[len] -= 1;
                let bits = (len - root_bits) as u8;
                let value = sorted[symbol];
                symbol += 1;
                let pos = table_pos + (key as usize >> root_bits);
                trace!(
                    "filling 2nd level table of len {len} starting at position {pos} ({table_pos} + {}) of {}",
                    key as usize >> root_bits,
                    table.len()
                );
                replicate_value(&mut table[pos..], step, TableEntry { bits, value });
                key = get_next_key(key, len);
            }
            step <<= 1;
        }
        Ok(table)
    }

    #[instrument(level = "trace", skip(br), ret, err)]
    pub fn decode(al_size: usize, br: &mut BitReader) -> Result<Table> {
        let entries = if al_size == 1 {
            vec![TableEntry { bits: 0, value: 0 }; TABLE_SIZE]
        } else {
            assert!(al_size < 1 << HUFFMAN_MAX_BITS);
            let simple_code_or_skip = br.read(2)? as usize;
            if simple_code_or_skip == 1 {
                Table::decode_simple_table(al_size, br)?
            } else {
                let mut code_length_code_lengths = [0u8; CODE_LENGTHS_CODE];
                let mut space = 32;
                const STATIC_HUFF_BITS: [u8; 16] = [2, 2, 2, 3, 2, 2, 2, 4, 2, 2, 2, 3, 2, 2, 2, 4];
                const STATIC_HUFF_VALS: [u8; 16] = [0, 4, 3, 2, 0, 4, 3, 1, 0, 4, 3, 2, 0, 4, 3, 5];
                const CODE_LENGTH_CODE_ORDER: [u8; CODE_LENGTHS_CODE] =
                    [1, 2, 3, 4, 0, 5, 17, 6, 16, 7, 8, 9, 10, 11, 12, 13, 14, 15];
                let mut num_codes = 0;
                for i in simple_code_or_skip..CODE_LENGTHS_CODE {
                    if space <= 0 {
                        break;
                    }
                    let idx = br.peek(4) as usize;
                    br.consume(STATIC_HUFF_BITS[idx] as usize)?;
                    let v = STATIC_HUFF_VALS[idx];
                    code_length_code_lengths[CODE_LENGTH_CODE_ORDER[i] as usize] = v;
                    if v != 0 {
                        space -= 32 >> v;
                        num_codes += 1;
                    }
                }
                if num_codes != 1 && space != 0 {
                    return Err(Error::InvalidHuffman);
                }
                let code_lengths =
                    Table::decode_huffman_code_lengths(code_length_code_lengths, al_size, br)?;
                debug!(?code_lengths);
                Table::build(TABLE_BITS, &code_lengths)?
            }
        };
        Ok(Table { entries })
    }

    #[inline]
    pub fn read(&self, br: &mut BitReader) -> u32 {
        let mut pos = br.peek(TABLE_BITS) as usize;
        let mut n_bits = self.entries[pos].bits as usize;
        if n_bits > TABLE_BITS {
            br.consume_optimistic(TABLE_BITS);
            n_bits -= TABLE_BITS;
            pos += self.entries[pos].value as usize;
            pos += br.peek(n_bits) as usize;
        }
        br.consume_optimistic(self.entries[pos].bits as usize);
        self.entries[pos].value as u32
    }
}

#[derive(Debug)]
pub struct HuffmanCodes {
    tables: Vec<Table>,
}

impl HuffmanCodes {
    pub fn decode(num: usize, br: &mut BitReader) -> Result<HuffmanCodes> {
        let alphabet_sizes: Vec<usize> = (0..num)
            .map(|_| Ok(decode_varint16(br)? as usize + 1))
            .collect::<Result<_>>()?;
        let max = *alphabet_sizes.iter().max().unwrap();
        if max >= (1 << HUFFMAN_MAX_BITS) {
            return Err(Error::AlphabetTooLargeHuff(max));
        }
        let tables = alphabet_sizes
            .iter()
            .map(|sz| Table::decode(*sz, br))
            .collect::<Result<_>>()?;
        Ok(HuffmanCodes { tables })
    }

    #[inline]
    pub fn read(&self, br: &mut BitReader, ctx: usize) -> u32 {
        self.tables[ctx].read(br)
    }

    pub fn single_symbol(&self, ctx: usize) -> Option<u32> {
        if let TableEntry { bits: 0, value } = self.tables[ctx].entries[0] {
            Some(value as u32)
        } else {
            None
        }
    }
}

#[cfg(test)]
impl Table {
    fn new_single_symbol(sym: u16) -> Table {
        Table {
            entries: vec![
                TableEntry {
                    bits: 0,
                    value: sym
                };
                TABLE_SIZE
            ],
        }
    }
}

#[cfg(test)]
impl HuffmanCodes {
    /// Builds Huffman histogram of 256 8-bit symbols.
    pub(super) fn byte_histogram() -> HuffmanCodes {
        let mut br = BitReader::new(&[0b11101111, 0b00111111, 0, 1, 0, 0b10100000, 0b0110]);
        HuffmanCodes::decode(1, &mut br).unwrap()
    }

    pub(super) fn byte_histogram_rle() -> HuffmanCodes {
        let mut histogram = Self::byte_histogram();
        histogram.tables.push(Table::new_single_symbol(1));
        histogram
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use test_log::test;

    #[test]
    fn byte_histogram() {
        let codes = HuffmanCodes::byte_histogram();

        let expected_arr = [8u8, 13, 21, 34, 55, 89, 144, 233];
        let bits = expected_arr.map(|v| v.reverse_bits());
        let mut br = BitReader::new(&bits);

        for expected in expected_arr {
            assert_eq!(codes.read(&mut br, 0), expected as u32);
        }
    }

    #[test]
    fn long_code() {
        // This correctly sums to 4096 table entries
        const CODE: [u8; 520] = [
            3, 6, 7, 7, 7, 7, 7, 7, 7, 7, 8, 7, 8, 7, 8, 8, 8, 8, 8, 8, 8, 9, 9, 8, 9, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 8, 9, 9, 8, 9, 9, 9, 9, 9, 8, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
            8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
            8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 7, 8, 8, 8, 8, 8,
            8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 8, 9, 8, 8, 8, 8, 9, 9, 9, 9, 8, 8, 9, 9, 9, 9, 9, 9,
            9, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8, 4, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 10, 7, 9, 9, 11, 12, 12,
        ];
        assert!(Table::build(TABLE_BITS, &CODE).is_ok());
    }

    #[test]
    fn test_huffman_code_lengths_underflow() {
        let mut br = BitReader::new(&[0xff, 0xff, 0x7f, 0x7a]);
        let _ = Table::decode_huffman_code_lengths(
            [2, 0, 0, 0, 0, 4, 3, 4, 3, 0, 0, 4, 4, 4, 0, 0, 4, 3],
            1791,
            &mut br,
        );
    }
}
