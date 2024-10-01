use simd_adler32::Adler32;

use crate::tables::{
    self, CLCL_ORDER, DIST_SYM_TO_DIST_BASE, DIST_SYM_TO_DIST_EXTRA, FDEFLATE_DIST_DECODE_TABLE,
    FDEFLATE_LITLEN_DECODE_TABLE, FIXED_CODE_LENGTHS, LEN_SYM_TO_LEN_BASE, LEN_SYM_TO_LEN_EXTRA,
};

/// An error encountered while decompressing a deflate stream.
#[derive(Debug, PartialEq)]
pub enum DecompressionError {
    /// The zlib header is corrupt.
    BadZlibHeader,
    /// All input was consumed, but the end of the stream hasn't been reached.
    InsufficientInput,
    /// A block header specifies an invalid block type.
    InvalidBlockType,
    /// An uncompressed block's NLEN value is invalid.
    InvalidUncompressedBlockLength,
    /// Too many literals were specified.
    InvalidHlit,
    /// Too many distance codes were specified.
    InvalidHdist,
    /// Attempted to repeat a previous code before reading any codes, or past the end of the code
    /// lengths.
    InvalidCodeLengthRepeat,
    /// The stream doesn't specify a valid huffman tree.
    BadCodeLengthHuffmanTree,
    /// The stream doesn't specify a valid huffman tree.
    BadLiteralLengthHuffmanTree,
    /// The stream doesn't specify a valid huffman tree.
    BadDistanceHuffmanTree,
    /// The stream contains a literal/length code that was not allowed by the header.
    InvalidLiteralLengthCode,
    /// The stream contains a distance code that was not allowed by the header.
    InvalidDistanceCode,
    /// The stream contains contains back-reference as the first symbol.
    InputStartsWithRun,
    /// The stream contains a back-reference that is too far back.
    DistanceTooFarBack,
    /// The deflate stream checksum is incorrect.
    WrongChecksum,
    /// Extra input data.
    ExtraInput,
}

struct BlockHeader {
    hlit: usize,
    hdist: usize,
    hclen: usize,
    num_lengths_read: usize,

    /// Low 3-bits are code length code length, high 5-bits are code length code.
    table: [u8; 128],
    code_lengths: [u8; 320],
}

const LITERAL_ENTRY: u32 = 0x8000;
const EXCEPTIONAL_ENTRY: u32 = 0x4000;
const SECONDARY_TABLE_ENTRY: u32 = 0x2000;

/// The Decompressor state for a compressed block.
///
/// The main litlen_table uses a 12-bit input to lookup the meaning of the symbol. The table is
/// split into 4 sections:
///
///   aaaaaaaa_bbbbbbbb_1000yyyy_0000xxxx  x = input_advance_bits, y = output_advance_bytes (literal)
///   0000000z_zzzzzzzz_00000yyy_0000xxxx  x = input_advance_bits, y = extra_bits, z = distance_base (length)
///   00000000_00000000_01000000_0000xxxx  x = input_advance_bits (EOF)
///   0000xxxx_xxxxxxxx_01100000_00000000  x = secondary_table_index
///   00000000_00000000_01000000_00000000  invalid code
///
/// The distance table is a 512-entry table that maps 9 bits of distance symbols to their meaning.
///
///   00000000_00000000_00000000_00000000     symbol is more than 9 bits
///   zzzzzzzz_zzzzzzzz_0000yyyy_0000xxxx     x = input_advance_bits, y = extra_bits, z = distance_base
#[repr(align(64))]
#[derive(Eq, PartialEq, Debug)]
struct CompressedBlock {
    litlen_table: [u32; 4096],
    dist_table: [u32; 512],

    dist_symbol_lengths: [u8; 30],
    dist_symbol_masks: [u16; 30],
    dist_symbol_codes: [u16; 30],

    secondary_table: Vec<u16>,
    eof_code: u16,
    eof_mask: u16,
    eof_bits: u8,
}

const FDEFLATE_COMPRESSED_BLOCK: CompressedBlock = CompressedBlock {
    litlen_table: FDEFLATE_LITLEN_DECODE_TABLE,
    dist_table: FDEFLATE_DIST_DECODE_TABLE,
    dist_symbol_lengths: [
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    ],
    dist_symbol_masks: [
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    ],
    dist_symbol_codes: [
        0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
        0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
        0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    ],
    secondary_table: Vec::new(),
    eof_code: 0x8ff,
    eof_mask: 0xfff,
    eof_bits: 0xc,
};

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
enum State {
    ZlibHeader,
    BlockHeader,
    CodeLengthCodes,
    CodeLengths,
    CompressedData,
    UncompressedData,
    Checksum,
    Done,
}

/// Decompressor for arbitrary zlib streams.
pub struct Decompressor {
    /// State for decoding a compressed block.
    compression: CompressedBlock,
    // State for decoding a block header.
    header: BlockHeader,
    // Number of bytes left for uncompressed block.
    uncompressed_bytes_left: u16,

    buffer: u64,
    nbits: u8,

    queued_rle: Option<(u8, usize)>,
    queued_backref: Option<(usize, usize)>,
    last_block: bool,

    state: State,
    checksum: Adler32,
    ignore_adler32: bool,
}

impl Default for Decompressor {
    fn default() -> Self {
        Self::new()
    }
}

impl Decompressor {
    /// Create a new decompressor.
    pub fn new() -> Self {
        Self {
            buffer: 0,
            nbits: 0,
            compression: CompressedBlock {
                litlen_table: [0; 4096],
                dist_table: [0; 512],
                secondary_table: Vec::new(),
                dist_symbol_lengths: [0; 30],
                dist_symbol_masks: [0; 30],
                dist_symbol_codes: [0xffff; 30],
                eof_code: 0,
                eof_mask: 0,
                eof_bits: 0,
            },
            header: BlockHeader {
                hlit: 0,
                hdist: 0,
                hclen: 0,
                table: [0; 128],
                num_lengths_read: 0,
                code_lengths: [0; 320],
            },
            uncompressed_bytes_left: 0,
            queued_rle: None,
            queued_backref: None,
            checksum: Adler32::new(),
            state: State::ZlibHeader,
            last_block: false,
            ignore_adler32: false,
        }
    }

    /// Ignore the checksum at the end of the stream.
    pub fn ignore_adler32(&mut self) {
        self.ignore_adler32 = true;
    }

    fn fill_buffer(&mut self, input: &mut &[u8]) {
        if input.len() >= 8 {
            self.buffer |= u64::from_le_bytes(input[..8].try_into().unwrap()) << self.nbits;
            *input = &input[(63 - self.nbits as usize) / 8..];
            self.nbits |= 56;
        } else {
            let nbytes = input.len().min((63 - self.nbits as usize) / 8);
            let mut input_data = [0; 8];
            input_data[..nbytes].copy_from_slice(&input[..nbytes]);
            self.buffer |= u64::from_le_bytes(input_data)
                .checked_shl(self.nbits as u32)
                .unwrap_or(0);
            self.nbits += nbytes as u8 * 8;
            *input = &input[nbytes..];
        }
    }

    fn peak_bits(&mut self, nbits: u8) -> u64 {
        debug_assert!(nbits <= 56 && nbits <= self.nbits);
        self.buffer & ((1u64 << nbits) - 1)
    }
    fn consume_bits(&mut self, nbits: u8) {
        debug_assert!(self.nbits >= nbits);
        self.buffer >>= nbits;
        self.nbits -= nbits;
    }

    fn read_block_header(&mut self, remaining_input: &mut &[u8]) -> Result<(), DecompressionError> {
        self.fill_buffer(remaining_input);
        if self.nbits < 3 {
            return Ok(());
        }

        let start = self.peak_bits(3);
        self.last_block = start & 1 != 0;
        match start >> 1 {
            0b00 => {
                let align_bits = (self.nbits - 3) % 8;
                let header_bits = 3 + 32 + align_bits;
                if self.nbits < header_bits {
                    return Ok(());
                }

                let len = (self.peak_bits(align_bits + 19) >> (align_bits + 3)) as u16;
                let nlen = (self.peak_bits(header_bits) >> (align_bits + 19)) as u16;
                if nlen != !len {
                    return Err(DecompressionError::InvalidUncompressedBlockLength);
                }

                self.state = State::UncompressedData;
                self.uncompressed_bytes_left = len;
                self.consume_bits(header_bits);
                Ok(())
            }
            0b01 => {
                self.consume_bits(3);
                // TODO: Do this statically rather than every time.
                Self::build_tables(288, &FIXED_CODE_LENGTHS, &mut self.compression, 6)?;
                self.state = State::CompressedData;
                Ok(())
            }
            0b10 => {
                if self.nbits < 17 {
                    return Ok(());
                }

                self.header.hlit = (self.peak_bits(8) >> 3) as usize + 257;
                self.header.hdist = (self.peak_bits(13) >> 8) as usize + 1;
                self.header.hclen = (self.peak_bits(17) >> 13) as usize + 4;
                if self.header.hlit > 286 {
                    return Err(DecompressionError::InvalidHlit);
                }
                if self.header.hdist > 30 {
                    return Err(DecompressionError::InvalidHdist);
                }

                self.consume_bits(17);
                self.state = State::CodeLengthCodes;
                Ok(())
            }
            0b11 => Err(DecompressionError::InvalidBlockType),
            _ => unreachable!(),
        }
    }

    fn read_code_length_codes(
        &mut self,
        remaining_input: &mut &[u8],
    ) -> Result<(), DecompressionError> {
        self.fill_buffer(remaining_input);
        if self.nbits as usize + remaining_input.len() * 8 < 3 * self.header.hclen {
            return Ok(());
        }

        let mut code_length_lengths = [0; 19];
        for i in 0..self.header.hclen {
            code_length_lengths[CLCL_ORDER[i]] = self.peak_bits(3) as u8;
            self.consume_bits(3);

            // We need to refill the buffer after reading 3 * 18 = 54 bits since the buffer holds
            // between 56 and 63 bits total.
            if i == 17 {
                self.fill_buffer(remaining_input);
            }
        }
        let code_length_codes: [u16; 19] = crate::compute_codes(&code_length_lengths)
            .ok_or(DecompressionError::BadCodeLengthHuffmanTree)?;

        self.header.table = [255; 128];
        for i in 0..19 {
            let length = code_length_lengths[i];
            if length > 0 {
                let mut j = code_length_codes[i];
                while j < 128 {
                    self.header.table[j as usize] = ((i as u8) << 3) | length;
                    j += 1 << length;
                }
            }
        }

        self.state = State::CodeLengths;
        self.header.num_lengths_read = 0;
        Ok(())
    }

    fn read_code_lengths(&mut self, remaining_input: &mut &[u8]) -> Result<(), DecompressionError> {
        let total_lengths = self.header.hlit + self.header.hdist;
        while self.header.num_lengths_read < total_lengths {
            self.fill_buffer(remaining_input);
            if self.nbits < 7 {
                return Ok(());
            }

            let code = self.peak_bits(7);
            let entry = self.header.table[code as usize];
            let length = entry & 0x7;
            let symbol = entry >> 3;

            debug_assert!(length != 0);
            match symbol {
                0..=15 => {
                    self.header.code_lengths[self.header.num_lengths_read] = symbol;
                    self.header.num_lengths_read += 1;
                    self.consume_bits(length);
                }
                16..=18 => {
                    let (base_repeat, extra_bits) = match symbol {
                        16 => (3, 2),
                        17 => (3, 3),
                        18 => (11, 7),
                        _ => unreachable!(),
                    };

                    if self.nbits < length + extra_bits {
                        return Ok(());
                    }

                    let value = match symbol {
                        16 => {
                            self.header.code_lengths[self
                                .header
                                .num_lengths_read
                                .checked_sub(1)
                                .ok_or(DecompressionError::InvalidCodeLengthRepeat)?]
                            // TODO: is this right?
                        }
                        17 => 0,
                        18 => 0,
                        _ => unreachable!(),
                    };

                    let repeat =
                        (self.peak_bits(length + extra_bits) >> length) as usize + base_repeat;
                    if self.header.num_lengths_read + repeat > total_lengths {
                        return Err(DecompressionError::InvalidCodeLengthRepeat);
                    }

                    for i in 0..repeat {
                        self.header.code_lengths[self.header.num_lengths_read + i] = value;
                    }
                    self.header.num_lengths_read += repeat;
                    self.consume_bits(length + extra_bits);
                }
                _ => unreachable!(),
            }
        }

        self.header
            .code_lengths
            .copy_within(self.header.hlit..total_lengths, 288);
        for i in self.header.hlit..288 {
            self.header.code_lengths[i] = 0;
        }
        for i in 288 + self.header.hdist..320 {
            self.header.code_lengths[i] = 0;
        }

        if self.header.hdist == 1
            && self.header.code_lengths[..286] == tables::HUFFMAN_LENGTHS
            && self.header.code_lengths[288] == 1
        {
            self.compression = FDEFLATE_COMPRESSED_BLOCK;
        } else {
            Self::build_tables(
                self.header.hlit,
                &self.header.code_lengths,
                &mut self.compression,
                6,
            )?;
        }
        self.state = State::CompressedData;
        Ok(())
    }

    fn build_tables(
        hlit: usize,
        code_lengths: &[u8],
        compression: &mut CompressedBlock,
        max_search_bits: u8,
    ) -> Result<(), DecompressionError> {
        // If there is no code assigned for the EOF symbol then the bitstream is invalid.
        if code_lengths[256] == 0 {
            // TODO: Return a dedicated error in this case.
            return Err(DecompressionError::BadLiteralLengthHuffmanTree);
        }

        // Build the literal/length code table.
        let lengths = &code_lengths[..288];
        let codes: [u16; 288] = crate::compute_codes(&lengths.try_into().unwrap())
            .ok_or(DecompressionError::BadLiteralLengthHuffmanTree)?;

        let table_bits = lengths.iter().cloned().max().unwrap().min(12).max(6);
        let table_size = 1 << table_bits;

        for i in 0..256 {
            let code = codes[i];
            let length = lengths[i];
            let mut j = code;

            while j < table_size && length != 0 && length <= 12 {
                compression.litlen_table[j as usize] =
                    ((i as u32) << 16) | LITERAL_ENTRY | (1 << 8) | length as u32;
                j += 1 << length;
            }

            if length > 0 && length <= max_search_bits {
                for ii in 0..256 {
                    let code2 = codes[ii];
                    let length2 = lengths[ii];
                    if length2 != 0 && length + length2 <= table_bits {
                        let mut j = code | (code2 << length);

                        while j < table_size {
                            compression.litlen_table[j as usize] = (ii as u32) << 24
                                | (i as u32) << 16
                                | LITERAL_ENTRY
                                | (2 << 8)
                                | ((length + length2) as u32);
                            j += 1 << (length + length2);
                        }
                    }
                }
            }
        }

        if lengths[256] != 0 && lengths[256] <= 12 {
            let mut j = codes[256];
            while j < table_size {
                compression.litlen_table[j as usize] = EXCEPTIONAL_ENTRY | lengths[256] as u32;
                j += 1 << lengths[256];
            }
        }

        let table_size = table_size as usize;
        for i in (table_size..4096).step_by(table_size) {
            compression.litlen_table.copy_within(0..table_size, i);
        }

        compression.eof_code = codes[256];
        compression.eof_mask = (1 << lengths[256]) - 1;
        compression.eof_bits = lengths[256];

        for i in 257..hlit {
            let code = codes[i];
            let length = lengths[i];
            if length != 0 && length <= 12 {
                let mut j = code;
                while j < 4096 {
                    compression.litlen_table[j as usize] = if i < 286 {
                        (LEN_SYM_TO_LEN_BASE[i - 257] as u32) << 16
                            | (LEN_SYM_TO_LEN_EXTRA[i - 257] as u32) << 8
                            | length as u32
                    } else {
                        EXCEPTIONAL_ENTRY
                    };
                    j += 1 << length;
                }
            }
        }

        for i in 0..hlit {
            if lengths[i] > 12 {
                compression.litlen_table[(codes[i] & 0xfff) as usize] = u32::MAX;
            }
        }

        let mut secondary_table_len = 0;
        for i in 0..hlit {
            if lengths[i] > 12 {
                let j = (codes[i] & 0xfff) as usize;
                if compression.litlen_table[j] == u32::MAX {
                    compression.litlen_table[j] =
                        (secondary_table_len << 16) | EXCEPTIONAL_ENTRY | SECONDARY_TABLE_ENTRY;
                    secondary_table_len += 8;
                }
            }
        }
        assert!(secondary_table_len <= 0x7ff);
        compression.secondary_table = vec![0; secondary_table_len as usize];
        for i in 0..hlit {
            let code = codes[i];
            let length = lengths[i];
            if length > 12 {
                let j = (codes[i] & 0xfff) as usize;
                let k = (compression.litlen_table[j] >> 16) as usize;

                let mut s = code >> 12;
                while s < 8 {
                    debug_assert_eq!(compression.secondary_table[k + s as usize], 0);
                    compression.secondary_table[k + s as usize] =
                        ((i as u16) << 4) | (length as u16);
                    s += 1 << (length - 12);
                }
            }
        }
        debug_assert!(compression
            .secondary_table
            .iter()
            .all(|&x| x != 0 && (x & 0xf) > 12));

        // Build the distance code table.
        let lengths = &code_lengths[288..320];
        if lengths == [0; 32] {
            compression.dist_symbol_masks = [0; 30];
            compression.dist_symbol_codes = [0xffff; 30];
            compression.dist_table.fill(0);
        } else {
            let codes: [u16; 32] = match crate::compute_codes(&lengths.try_into().unwrap()) {
                Some(codes) => codes,
                None => {
                    if lengths.iter().filter(|&&l| l != 0).count() != 1 {
                        return Err(DecompressionError::BadDistanceHuffmanTree);
                    }
                    [0; 32]
                }
            };

            compression.dist_symbol_codes.copy_from_slice(&codes[..30]);
            compression
                .dist_symbol_lengths
                .copy_from_slice(&lengths[..30]);
            compression.dist_table.fill(0);
            for i in 0..30 {
                let length = lengths[i];
                let code = codes[i];
                if length == 0 {
                    compression.dist_symbol_masks[i] = 0;
                    compression.dist_symbol_codes[i] = 0xffff;
                } else {
                    compression.dist_symbol_masks[i] = (1 << lengths[i]) - 1;
                    if lengths[i] <= 9 {
                        let mut j = code;
                        while j < 512 {
                            compression.dist_table[j as usize] = (DIST_SYM_TO_DIST_BASE[i] as u32)
                                << 16
                                | (DIST_SYM_TO_DIST_EXTRA[i] as u32) << 8
                                | length as u32;
                            j += 1 << lengths[i];
                        }
                    }
                }
            }
        }

        Ok(())
    }

    fn read_compressed(
        &mut self,
        remaining_input: &mut &[u8],
        output: &mut [u8],
        mut output_index: usize,
    ) -> Result<usize, DecompressionError> {
        while let State::CompressedData = self.state {
            self.fill_buffer(remaining_input);
            if output_index == output.len() {
                break;
            }

            let mut bits = self.buffer;
            let litlen_entry = self.compression.litlen_table[(bits & 0xfff) as usize];
            let litlen_code_bits = litlen_entry as u8;

            if litlen_entry & LITERAL_ENTRY != 0 {
                // Ultra-fast path: do 3 more consecutive table lookups and bail if any of them need the slow path.
                if self.nbits >= 48 {
                    let litlen_entry2 =
                        self.compression.litlen_table[(bits >> litlen_code_bits & 0xfff) as usize];
                    let litlen_code_bits2 = litlen_entry2 as u8;
                    let litlen_entry3 = self.compression.litlen_table
                        [(bits >> (litlen_code_bits + litlen_code_bits2) & 0xfff) as usize];
                    let litlen_code_bits3 = litlen_entry3 as u8;
                    let litlen_entry4 = self.compression.litlen_table[(bits
                        >> (litlen_code_bits + litlen_code_bits2 + litlen_code_bits3)
                        & 0xfff)
                        as usize];
                    let litlen_code_bits4 = litlen_entry4 as u8;
                    if litlen_entry2 & litlen_entry3 & litlen_entry4 & LITERAL_ENTRY != 0 {
                        let advance_output_bytes = ((litlen_entry & 0xf00) >> 8) as usize;
                        let advance_output_bytes2 = ((litlen_entry2 & 0xf00) >> 8) as usize;
                        let advance_output_bytes3 = ((litlen_entry3 & 0xf00) >> 8) as usize;
                        let advance_output_bytes4 = ((litlen_entry4 & 0xf00) >> 8) as usize;
                        if output_index
                            + advance_output_bytes
                            + advance_output_bytes2
                            + advance_output_bytes3
                            + advance_output_bytes4
                            < output.len()
                        {
                            self.consume_bits(
                                litlen_code_bits
                                    + litlen_code_bits2
                                    + litlen_code_bits3
                                    + litlen_code_bits4,
                            );

                            output[output_index] = (litlen_entry >> 16) as u8;
                            output[output_index + 1] = (litlen_entry >> 24) as u8;
                            output_index += advance_output_bytes;
                            output[output_index] = (litlen_entry2 >> 16) as u8;
                            output[output_index + 1] = (litlen_entry2 >> 24) as u8;
                            output_index += advance_output_bytes2;
                            output[output_index] = (litlen_entry3 >> 16) as u8;
                            output[output_index + 1] = (litlen_entry3 >> 24) as u8;
                            output_index += advance_output_bytes3;
                            output[output_index] = (litlen_entry4 >> 16) as u8;
                            output[output_index + 1] = (litlen_entry4 >> 24) as u8;
                            output_index += advance_output_bytes4;
                            continue;
                        }
                    }
                }

                // Fast path: the next symbol is <= 12 bits and a literal, the table specifies the
                // output bytes and we can directly write them to the output buffer.
                let advance_output_bytes = ((litlen_entry & 0xf00) >> 8) as usize;

                // match advance_output_bytes {
                //     1 => println!("[{output_index}] LIT1 {}", litlen_entry >> 16),
                //     2 => println!(
                //         "[{output_index}] LIT2 {} {} {}",
                //         (litlen_entry >> 16) as u8,
                //         litlen_entry >> 24,
                //         bits & 0xfff
                //     ),
                //     n => println!(
                //         "[{output_index}] LIT{n} {} {}",
                //         (litlen_entry >> 16) as u8,
                //         litlen_entry >> 24,
                //     ),
                // }

                if self.nbits < litlen_code_bits {
                    break;
                } else if output_index + 1 < output.len() {
                    output[output_index] = (litlen_entry >> 16) as u8;
                    output[output_index + 1] = (litlen_entry >> 24) as u8;
                    output_index += advance_output_bytes;
                    self.consume_bits(litlen_code_bits);
                    continue;
                } else if output_index + advance_output_bytes == output.len() {
                    debug_assert_eq!(advance_output_bytes, 1);
                    output[output_index] = (litlen_entry >> 16) as u8;
                    output_index += 1;
                    self.consume_bits(litlen_code_bits);
                    break;
                } else {
                    debug_assert_eq!(advance_output_bytes, 2);
                    output[output_index] = (litlen_entry >> 16) as u8;
                    self.queued_rle = Some(((litlen_entry >> 24) as u8, 1));
                    output_index += 1;
                    self.consume_bits(litlen_code_bits);
                    break;
                }
            }

            let (length_base, length_extra_bits, litlen_code_bits) =
                if litlen_entry & EXCEPTIONAL_ENTRY == 0 {
                    (
                        litlen_entry >> 16,
                        (litlen_entry >> 8) as u8,
                        litlen_code_bits,
                    )
                } else if litlen_entry & SECONDARY_TABLE_ENTRY != 0 {
                    let secondary_index = litlen_entry >> 16;
                    let secondary_entry = self.compression.secondary_table
                        [secondary_index as usize + ((bits >> 12) & 0x7) as usize];
                    let litlen_symbol = secondary_entry >> 4;
                    let litlen_code_bits = (secondary_entry & 0xf) as u8;

                    if self.nbits < litlen_code_bits {
                        break;
                    } else if litlen_symbol < 256 {
                        // println!("[{output_index}] LIT1b {} (val={:04x})", litlen_symbol, self.peak_bits(15));

                        self.consume_bits(litlen_code_bits);
                        output[output_index] = litlen_symbol as u8;
                        output_index += 1;
                        continue;
                    } else if litlen_symbol == 256 {
                        // println!("[{output_index}] EOF");
                        self.consume_bits(litlen_code_bits);
                        self.state = match self.last_block {
                            true => State::Checksum,
                            false => State::BlockHeader,
                        };
                        break;
                    }

                    (
                        LEN_SYM_TO_LEN_BASE[litlen_symbol as usize - 257] as u32,
                        LEN_SYM_TO_LEN_EXTRA[litlen_symbol as usize - 257],
                        litlen_code_bits,
                    )
                } else if litlen_code_bits == 0 {
                    return Err(DecompressionError::InvalidLiteralLengthCode);
                } else {
                    if self.nbits < litlen_code_bits {
                        break;
                    }
                    // println!("[{output_index}] EOF");
                    self.consume_bits(litlen_code_bits);
                    self.state = match self.last_block {
                        true => State::Checksum,
                        false => State::BlockHeader,
                    };
                    break;
                };
            bits >>= litlen_code_bits;

            let length_extra_mask = (1 << length_extra_bits) - 1;
            let length = length_base as usize + (bits & length_extra_mask) as usize;
            bits >>= length_extra_bits;

            let dist_entry = self.compression.dist_table[(bits & 0x1ff) as usize];
            let (dist_base, dist_extra_bits, dist_code_bits) = if dist_entry != 0 {
                (
                    (dist_entry >> 16) as u16,
                    (dist_entry >> 8) as u8,
                    dist_entry as u8,
                )
            } else if self.nbits > litlen_code_bits + length_extra_bits + 9 {
                let mut dist_extra_bits = 0;
                let mut dist_base = 0;
                let mut dist_advance_bits = 0;
                for i in 0..self.compression.dist_symbol_lengths.len() {
                    if bits as u16 & self.compression.dist_symbol_masks[i]
                        == self.compression.dist_symbol_codes[i]
                    {
                        dist_extra_bits = DIST_SYM_TO_DIST_EXTRA[i];
                        dist_base = DIST_SYM_TO_DIST_BASE[i];
                        dist_advance_bits = self.compression.dist_symbol_lengths[i];
                        break;
                    }
                }
                if dist_advance_bits == 0 {
                    return Err(DecompressionError::InvalidDistanceCode);
                }
                (dist_base, dist_extra_bits, dist_advance_bits)
            } else {
                break;
            };
            bits >>= dist_code_bits;

            let dist = dist_base as usize + (bits & ((1 << dist_extra_bits) - 1)) as usize;
            let total_bits =
                litlen_code_bits + length_extra_bits + dist_code_bits + dist_extra_bits;

            if self.nbits < total_bits {
                break;
            } else if dist > output_index {
                return Err(DecompressionError::DistanceTooFarBack);
            }

            // println!("[{output_index}] BACKREF len={} dist={} {:x}", length, dist, dist_entry);
            self.consume_bits(total_bits);

            let copy_length = length.min(output.len() - output_index);
            if dist == 1 {
                let last = output[output_index - 1];
                output[output_index..][..copy_length].fill(last);

                if copy_length < length {
                    self.queued_rle = Some((last, length - copy_length));
                    output_index = output.len();
                    break;
                }
            } else if output_index + length + 15 <= output.len() {
                let start = output_index - dist;
                output.copy_within(start..start + 16, output_index);

                if length > 16 || dist < 16 {
                    for i in (0..length).step_by(dist.min(16)).skip(1) {
                        output.copy_within(start + i..start + i + 16, output_index + i);
                    }
                }
            } else {
                if dist < copy_length {
                    for i in 0..copy_length {
                        output[output_index + i] = output[output_index + i - dist];
                    }
                } else {
                    output.copy_within(
                        output_index - dist..output_index + copy_length - dist,
                        output_index,
                    )
                }

                if copy_length < length {
                    self.queued_backref = Some((dist, length - copy_length));
                    output_index = output.len();
                    break;
                }
            }
            output_index += copy_length;
        }

        if self.state == State::CompressedData
            && self.queued_backref.is_none()
            && self.queued_rle.is_none()
            && self.nbits >= 15
            && self.peak_bits(15) as u16 & self.compression.eof_mask == self.compression.eof_code
        {
            self.consume_bits(self.compression.eof_bits);
            self.state = match self.last_block {
                true => State::Checksum,
                false => State::BlockHeader,
            };
        }

        Ok(output_index)
    }

    /// Decompresses a chunk of data.
    ///
    /// Returns the number of bytes read from `input` and the number of bytes written to `output`,
    /// or an error if the deflate stream is not valid. `input` is the compressed data. `output` is
    /// the buffer to write the decompressed data to, starting at index `output_position`.
    /// `end_of_input` indicates whether more data may be available in the future.
    ///
    /// The contents of `output` after `output_position` are ignored. However, this function may
    /// write additional data to `output` past what is indicated by the return value.
    ///
    /// When this function returns `Ok`, at least one of the following is true:
    /// - The input is fully consumed.
    /// - The output is full but there are more bytes to output.
    /// - The deflate stream is complete (and `is_done` will return true).
    ///
    /// # Panics
    ///
    /// This function will panic if `output_position` is out of bounds.
    pub fn read(
        &mut self,
        input: &[u8],
        output: &mut [u8],
        output_position: usize,
        end_of_input: bool,
    ) -> Result<(usize, usize), DecompressionError> {
        if let State::Done = self.state {
            return Ok((0, 0));
        }

        assert!(output_position <= output.len());

        let mut remaining_input = input;
        let mut output_index = output_position;

        if let Some((data, len)) = self.queued_rle.take() {
            let n = len.min(output.len() - output_index);
            output[output_index..][..n].fill(data);
            output_index += n;
            if n < len {
                self.queued_rle = Some((data, len - n));
                return Ok((0, n));
            }
        }
        if let Some((dist, len)) = self.queued_backref.take() {
            let n = len.min(output.len() - output_index);
            for i in 0..n {
                output[output_index + i] = output[output_index + i - dist];
            }
            output_index += n;
            if n < len {
                self.queued_backref = Some((dist, len - n));
                return Ok((0, n));
            }
        }

        // Main decoding state machine.
        let mut last_state = None;
        while last_state != Some(self.state) {
            last_state = Some(self.state);
            match self.state {
                State::ZlibHeader => {
                    self.fill_buffer(&mut remaining_input);
                    if self.nbits < 16 {
                        break;
                    }

                    let input0 = self.peak_bits(8);
                    let input1 = self.peak_bits(16) >> 8 & 0xff;
                    if input0 & 0x0f != 0x08
                        || (input0 & 0xf0) > 0x70
                        || input1 & 0x20 != 0
                        || (input0 << 8 | input1) % 31 != 0
                    {
                        return Err(DecompressionError::BadZlibHeader);
                    }

                    self.consume_bits(16);
                    self.state = State::BlockHeader;
                }
                State::BlockHeader => {
                    self.read_block_header(&mut remaining_input)?;
                }
                State::CodeLengthCodes => {
                    self.read_code_length_codes(&mut remaining_input)?;
                }
                State::CodeLengths => {
                    self.read_code_lengths(&mut remaining_input)?;
                }
                State::CompressedData => {
                    output_index =
                        self.read_compressed(&mut remaining_input, output, output_index)?
                }
                State::UncompressedData => {
                    // Drain any bytes from our buffer.
                    debug_assert_eq!(self.nbits % 8, 0);
                    while self.nbits > 0
                        && self.uncompressed_bytes_left > 0
                        && output_index < output.len()
                    {
                        output[output_index] = self.peak_bits(8) as u8;
                        self.consume_bits(8);
                        output_index += 1;
                        self.uncompressed_bytes_left -= 1;
                    }
                    // Buffer may contain one additional byte. Clear it to avoid confusion.
                    if self.nbits == 0 {
                        self.buffer = 0;
                    }

                    // Copy subsequent bytes directly from the input.
                    let copy_bytes = (self.uncompressed_bytes_left as usize)
                        .min(remaining_input.len())
                        .min(output.len() - output_index);
                    output[output_index..][..copy_bytes]
                        .copy_from_slice(&remaining_input[..copy_bytes]);
                    remaining_input = &remaining_input[copy_bytes..];
                    output_index += copy_bytes;
                    self.uncompressed_bytes_left -= copy_bytes as u16;

                    if self.uncompressed_bytes_left == 0 {
                        self.state = if self.last_block {
                            State::Checksum
                        } else {
                            State::BlockHeader
                        };
                    }
                }
                State::Checksum => {
                    self.fill_buffer(&mut remaining_input);

                    let align_bits = self.nbits % 8;
                    if self.nbits >= 32 + align_bits {
                        self.checksum.write(&output[output_position..output_index]);
                        if align_bits != 0 {
                            self.consume_bits(align_bits);
                        }
                        #[cfg(not(fuzzing))]
                        if !self.ignore_adler32
                            && (self.peak_bits(32) as u32).swap_bytes() != self.checksum.finish()
                        {
                            return Err(DecompressionError::WrongChecksum);
                        }
                        self.state = State::Done;
                        self.consume_bits(32);
                        break;
                    }
                }
                State::Done => unreachable!(),
            }
        }

        if !self.ignore_adler32 && self.state != State::Done {
            self.checksum.write(&output[output_position..output_index]);
        }

        if self.state == State::Done || !end_of_input || output_index == output.len() {
            let input_left = remaining_input.len();
            Ok((input.len() - input_left, output_index - output_position))
        } else {
            Err(DecompressionError::InsufficientInput)
        }
    }

    /// Returns true if the decompressor has finished decompressing the input.
    pub fn is_done(&self) -> bool {
        self.state == State::Done
    }
}

/// Decompress the given data.
pub fn decompress_to_vec(input: &[u8]) -> Result<Vec<u8>, DecompressionError> {
    match decompress_to_vec_bounded(input, usize::MAX) {
        Ok(output) => Ok(output),
        Err(BoundedDecompressionError::DecompressionError { inner }) => Err(inner),
        Err(BoundedDecompressionError::OutputTooLarge { .. }) => {
            unreachable!("Impossible to allocate more than isize::MAX bytes")
        }
    }
}

/// An error encountered while decompressing a deflate stream given a bounded maximum output.
pub enum BoundedDecompressionError {
    /// The input is not a valid deflate stream.
    DecompressionError {
        /// The underlying error.
        inner: DecompressionError,
    },

    /// The output is too large.
    OutputTooLarge {
        /// The output decoded so far.
        partial_output: Vec<u8>,
    },
}
impl From<DecompressionError> for BoundedDecompressionError {
    fn from(inner: DecompressionError) -> Self {
        BoundedDecompressionError::DecompressionError { inner }
    }
}

/// Decompress the given data, returning an error if the output is larger than
/// `maxlen` bytes.
pub fn decompress_to_vec_bounded(
    input: &[u8],
    maxlen: usize,
) -> Result<Vec<u8>, BoundedDecompressionError> {
    let mut decoder = Decompressor::new();
    let mut output = vec![0; 1024.min(maxlen)];
    let mut input_index = 0;
    let mut output_index = 0;
    loop {
        let (consumed, produced) =
            decoder.read(&input[input_index..], &mut output, output_index, true)?;
        input_index += consumed;
        output_index += produced;
        if decoder.is_done() || output_index == maxlen {
            break;
        }
        output.resize((output_index + 32 * 1024).min(maxlen), 0);
    }
    output.resize(output_index, 0);

    if decoder.is_done() {
        Ok(output)
    } else {
        Err(BoundedDecompressionError::OutputTooLarge {
            partial_output: output,
        })
    }
}

#[cfg(test)]
mod tests {
    use crate::tables::{LENGTH_TO_LEN_EXTRA, LENGTH_TO_SYMBOL};

    use super::*;
    use rand::Rng;

    fn roundtrip(data: &[u8]) {
        let compressed = crate::compress_to_vec(data);
        let decompressed = decompress_to_vec(&compressed).unwrap();
        assert_eq!(&decompressed, data);
    }

    fn roundtrip_miniz_oxide(data: &[u8]) {
        let compressed = miniz_oxide::deflate::compress_to_vec_zlib(data, 3);
        let decompressed = decompress_to_vec(&compressed).unwrap();
        assert_eq!(decompressed.len(), data.len());
        for (i, (a, b)) in decompressed.chunks(1).zip(data.chunks(1)).enumerate() {
            assert_eq!(a, b, "chunk {}..{}", i, i + 1);
        }
        assert_eq!(&decompressed, data);
    }

    #[allow(unused)]
    fn compare_decompression(data: &[u8]) {
        // let decompressed0 = flate2::read::ZlibDecoder::new(std::io::Cursor::new(&data))
        //     .bytes()
        //     .collect::<Result<Vec<_>, _>>()
        //     .unwrap();
        let decompressed = decompress_to_vec(data).unwrap();
        let decompressed2 = miniz_oxide::inflate::decompress_to_vec_zlib(data).unwrap();
        for i in 0..decompressed.len().min(decompressed2.len()) {
            if decompressed[i] != decompressed2[i] {
                panic!(
                    "mismatch at index {} {:?} {:?}",
                    i,
                    &decompressed[i.saturating_sub(1)..(i + 16).min(decompressed.len())],
                    &decompressed2[i.saturating_sub(1)..(i + 16).min(decompressed2.len())]
                );
            }
        }
        if decompressed != decompressed2 {
            panic!(
                "length mismatch {} {} {:x?}",
                decompressed.len(),
                decompressed2.len(),
                &decompressed2[decompressed.len()..][..16]
            );
        }
        //assert_eq!(decompressed, decompressed2);
    }

    #[test]
    fn tables() {
        for (i, &bits) in LEN_SYM_TO_LEN_EXTRA.iter().enumerate() {
            let len_base = LEN_SYM_TO_LEN_BASE[i];
            for j in 0..(1 << bits) {
                if i == 27 && j == 31 {
                    continue;
                }
                assert_eq!(LENGTH_TO_LEN_EXTRA[len_base + j - 3], bits, "{} {}", i, j);
                assert_eq!(
                    LENGTH_TO_SYMBOL[len_base + j - 3],
                    i as u16 + 257,
                    "{} {}",
                    i,
                    j
                );
            }
        }
    }

    #[test]
    fn fdeflate_table() {
        let mut compression = CompressedBlock {
            litlen_table: [0; 4096],
            dist_table: [0; 512],
            dist_symbol_lengths: [0; 30],
            dist_symbol_masks: [0; 30],
            dist_symbol_codes: [0; 30],
            secondary_table: Vec::new(),
            eof_code: 0,
            eof_mask: 0,
            eof_bits: 0,
        };
        let mut lengths = tables::HUFFMAN_LENGTHS.to_vec();
        lengths.resize(288, 0);
        lengths.push(1);
        lengths.resize(320, 0);
        Decompressor::build_tables(286, &lengths, &mut compression, 11).unwrap();

        assert_eq!(
            compression, FDEFLATE_COMPRESSED_BLOCK,
            "{:#x?}",
            compression
        );
    }

    #[test]
    fn it_works() {
        roundtrip(b"Hello world!");
    }

    #[test]
    fn constant() {
        roundtrip_miniz_oxide(&[0; 50]);
        roundtrip_miniz_oxide(&vec![5; 2048]);
        roundtrip_miniz_oxide(&vec![128; 2048]);
        roundtrip_miniz_oxide(&vec![254; 2048]);
    }

    #[test]
    fn random() {
        let mut rng = rand::thread_rng();
        let mut data = vec![0; 50000];
        for _ in 0..10 {
            for byte in &mut data {
                *byte = rng.gen::<u8>() % 5;
            }
            println!("Random data: {:?}", data);
            roundtrip_miniz_oxide(&data);
        }
    }

    #[test]
    fn ignore_adler32() {
        let mut compressed = crate::compress_to_vec(b"Hello world!");
        let last_byte = compressed.len() - 1;
        compressed[last_byte] = compressed[last_byte].wrapping_add(1);

        match decompress_to_vec(&compressed) {
            Err(DecompressionError::WrongChecksum) => {}
            r => panic!("expected WrongChecksum, got {:?}", r),
        }

        let mut decompressor = Decompressor::new();
        decompressor.ignore_adler32();
        let mut decompressed = vec![0; 1024];
        let decompressed_len = decompressor
            .read(&compressed, &mut decompressed, 0, true)
            .unwrap()
            .1;
        assert_eq!(&decompressed[..decompressed_len], b"Hello world!");
    }

    #[test]
    fn checksum_after_eof() {
        let input = b"Hello world!";
        let compressed = crate::compress_to_vec(input);

        let mut decompressor = Decompressor::new();
        let mut decompressed = vec![0; 1024];
        let (input_consumed, output_written) = decompressor
            .read(
                &compressed[..compressed.len() - 1],
                &mut decompressed,
                0,
                false,
            )
            .unwrap();
        assert_eq!(output_written, input.len());
        assert_eq!(input_consumed, compressed.len() - 1);

        let (input_consumed, output_written) = decompressor
            .read(
                &compressed[input_consumed..],
                &mut decompressed[..output_written],
                output_written,
                true,
            )
            .unwrap();
        assert!(decompressor.is_done());
        assert_eq!(input_consumed, 1);
        assert_eq!(output_written, 0);

        assert_eq!(&decompressed[..input.len()], input);
    }

    #[test]
    fn zero_length() {
        let mut compressed = crate::compress_to_vec(b"").to_vec();

        // Splice in zero-length non-compressed blocks.
        for _ in 0..10 {
            println!("compressed len: {}", compressed.len());
            compressed.splice(2..2, [0u8, 0, 0, 0xff, 0xff].into_iter());
        }

        // Ensure that the full input is decompressed, regardless of whether
        // `end_of_input` is set.
        for end_of_input in [true, false] {
            let mut decompressor = Decompressor::new();
            let (input_consumed, output_written) = decompressor
                .read(&compressed, &mut [], 0, end_of_input)
                .unwrap();

            assert!(decompressor.is_done());
            assert_eq!(input_consumed, compressed.len());
            assert_eq!(output_written, 0);
        }
    }

    mod test_utils;
    use test_utils::{decompress_by_chunks, TestDecompressionError};

    fn verify_no_sensitivity_to_input_chunking(
        input: &[u8],
    ) -> Result<Vec<u8>, TestDecompressionError> {
        let r_whole = decompress_by_chunks(input, vec![input.len()], false);
        let r_bytewise = decompress_by_chunks(input, std::iter::repeat(1), false);
        assert_eq!(r_whole, r_bytewise);
        r_whole // Returning an arbitrary result, since this is equal to `r_bytewise`.
    }

    /// This is a regression test found by the `buf_independent` fuzzer from the `png` crate.  When
    /// this test case was found, the results were unexpectedly different when 1) decompressing the
    /// whole input (successful result) vs 2) decompressing byte-by-byte
    /// (`Err(InvalidDistanceCode)`).
    #[test]
    fn test_input_chunking_sensitivity_when_handling_distance_codes() {
        let result = verify_no_sensitivity_to_input_chunking(include_bytes!(
            "../tests/input-chunking-sensitivity-example1.zz"
        ))
        .unwrap();
        assert_eq!(result.len(), 281);
        assert_eq!(simd_adler32::adler32(&result.as_slice()), 751299);
    }

    /// This is a regression test found by the `inflate_bytewise3` fuzzer from the `fdeflate`
    /// crate.  When this test case was found, the results were unexpectedly different when 1)
    /// decompressing the whole input (`Err(DistanceTooFarBack)`) vs 2) decompressing byte-by-byte
    /// (successful result)`).
    #[test]
    fn test_input_chunking_sensitivity_when_no_end_of_block_symbol_example1() {
        let err = verify_no_sensitivity_to_input_chunking(include_bytes!(
            "../tests/input-chunking-sensitivity-example2.zz"
        ))
        .unwrap_err();
        assert_eq!(
            err,
            TestDecompressionError::ProdError(DecompressionError::BadLiteralLengthHuffmanTree)
        );
    }

    /// This is a regression test found by the `inflate_bytewise3` fuzzer from the `fdeflate`
    /// crate.  When this test case was found, the results were unexpectedly different when 1)
    /// decompressing the whole input (`Err(InvalidDistanceCode)`) vs 2) decompressing byte-by-byte
    /// (successful result)`).
    #[test]
    fn test_input_chunking_sensitivity_when_no_end_of_block_symbol_example2() {
        let err = verify_no_sensitivity_to_input_chunking(include_bytes!(
            "../tests/input-chunking-sensitivity-example3.zz"
        ))
        .unwrap_err();
        assert_eq!(
            err,
            TestDecompressionError::ProdError(DecompressionError::BadLiteralLengthHuffmanTree)
        );
    }
}
