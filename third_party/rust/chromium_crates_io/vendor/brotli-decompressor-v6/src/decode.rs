#![allow(non_snake_case)]
#![allow(unused_parens)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]
#![allow(unused_macros)]

// #[macro_use] //<- for debugging, remove xprintln from bit_reader and replace with println
// extern crate std;
use core;
use super::alloc;
pub use alloc::{AllocatedStackMemory, Allocator, SliceWrapper, SliceWrapperMut, StackAllocator};

use core::mem;

use super::bit_reader;
use super::huffman;
use super::state;
use super::prefix;

use super::transform::{TransformDictionaryWord, copy_bytes, kNumTransforms};
use state::{BlockTypeAndLengthState, BrotliRunningContextMapState, BrotliRunningDecodeUint8State,
            BrotliRunningHuffmanState, BrotliRunningMetablockHeaderState,
            BrotliRunningReadBlockLengthState, BrotliRunningState, BrotliRunningTreeGroupState,
            BrotliRunningUncompressedState, kLiteralContextBits,
            BrotliDecoderErrorCode, COMPOUND_DICTIONARY_BLOCK_MAP_UNINITIALIZED,
};
use context::{kContextLookup};
use ::dictionary::{kBrotliDictionary, kBrotliDictionaryOffsetsByLength,
                   kBrotliDictionarySizeBitsByLength, kBrotliMaxDictionaryWordLength,
                   kBrotliMinDictionaryWordLength};
use ::shared_dictionary::{SHARED_BROTLI_MIN_DICTIONARY_WORD_LENGTH,
                          SHARED_BROTLI_MAX_DICTIONARY_WORD_LENGTH,
                          SHARED_BROTLI_MAX_TRANSFORM_AFFIX_LENGTH,
                          DictionaryLookupError};
pub use huffman::{HuffmanCode, HuffmanTreeGroup};
use huffman::{HuffmanTable, LITERAL_TABLE_WINDOW, INSERT_COPY_TABLE_WINDOW, DISTANCE_ROOT_WINDOW};
#[repr(C)]
#[derive(Debug)]
pub enum BrotliResult {
  ResultSuccess = 1,
  NeedsMoreInput = 2,
  NeedsMoreOutput = 3,
  ResultFailure = 0,
}
const kBrotliWindowGap: u32 = 16;
const kBrotliLargeMinWbits: u32 = 10;
const kBrotliLargeMaxWbits: u32 = 30;
const kBrotliMaxPostfix: usize = 3;
const kBrotliMaxAllowedDistance: u32 = 0x7FFFFFFC;
const kDefaultCodeLength: u32 = 8;
const kCodeLengthRepeatCode: u32 = 16;
pub const kNumLiteralCodes: u16 = 256;
pub const kNumInsertAndCopyCodes: u16 = 704;
pub const kNumBlockLengthCodes: u32 = 26;
const kDistanceContextBits: i32 = 2;
const HUFFMAN_TABLE_BITS: u32 = 8;
const HUFFMAN_TABLE_MASK: u32 = 0xff;
const CODE_LENGTH_CODES: usize = 18;
const kCodeLengthCodeOrder: [u8; CODE_LENGTH_CODES] = [1, 2, 3, 4, 0, 5, 17, 6, 16, 7, 8, 9, 10,
                                                       11, 12, 13, 14, 15];

// Static prefix code for the complex code length code lengths.
const kCodeLengthPrefixLength: [u8; 16] = [2, 2, 2, 3, 2, 2, 2, 4, 2, 2, 2, 3, 2, 2, 2, 4];

const kCodeLengthPrefixValue: [u8; 16] = [0, 4, 3, 2, 0, 4, 3, 1, 0, 4, 3, 2, 0, 4, 3, 5];


macro_rules! BROTLI_LOG_UINT (
    ($num : expr) => {
       xprintln!("{:?} = {:?}", stringify!($num),  $num)
    };
);

macro_rules! BROTLI_LOG (
    ($str : expr, $num : expr) => {xprintln!("{:?} {:?}", $str, $num);};
    ($str : expr, $num0 : expr, $num1 : expr) => {xprintln!("{:?} {:?} {:?}", $str, $num0, $num1);};
    ($str : expr, $num0 : expr, $num1 : expr, $num2 : expr) => {
        xprintln!("{:?} {:?} {:?} {:?}", $str, $num0, $num1, $num2);
    };
    ($str : expr, $num0 : expr, $num1 : expr, $num2 : expr, $num3 : expr) => {
        xprintln!("{:?} {:?} {:?} {:?} {:?}", $str, $num0, $num1, $num2, $num3);
    };
);
fn is_fatal(e: BrotliDecoderErrorCode) -> bool {
  (e as i64) < 0
}
fn assign_error_code(output: &mut BrotliDecoderErrorCode, input: BrotliDecoderErrorCode) -> BrotliDecoderErrorCode {
  *output = input;
  input
}

#[allow(non_snake_case)]
macro_rules! SaveErrorCode {
  ($state: expr, $e:expr) => {
    match assign_error_code(&mut $state.error_code, $e) {
      BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS =>
        BrotliResult::ResultSuccess,
      BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT =>
        BrotliResult::NeedsMoreInput,
      BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_OUTPUT =>
        BrotliResult::NeedsMoreOutput,
      _ =>
        BrotliResult::ResultFailure,
    }
  }
}
macro_rules! SaveResult {
  ($state: expr, $e:expr) => {
    match ($state.error_code = match $e  {
      BrotliResult::ResultSuccess => BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS,
      BrotliResult::NeedsMoreInput => BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT,
      BrotliResult::NeedsMoreOutput => BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_OUTPUT,
      BrotliResult::ResultFailure => BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE,
    }) {
      BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS =>
        BrotliResult::ResultSuccess,
      BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT =>
        BrotliResult::NeedsMoreInput,
      BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_OUTPUT =>
        BrotliResult::NeedsMoreOutput,
      _ =>
        BrotliResult::ResultFailure,
    }
  }
}
macro_rules! BROTLI_LOG_ARRAY_INDEX (
    ($array : expr, $index : expr) => {
       xprintln!("{:?}[{:?}] = {:?}", stringify!($array), $index,  $array[$index as usize])
    };
);


const NUM_DISTANCE_SHORT_CODES: u32 = 16;
pub const BROTLI_MAX_DISTANCE_BITS:u32 = 24;

pub const BROTLI_LARGE_MAX_DISTANCE_BITS: u32 = 62;

pub fn BROTLI_DISTANCE_ALPHABET_SIZE(NPOSTFIX: u32, NDIRECT:u32, MAXNBITS: u32) -> u32 {
    NUM_DISTANCE_SHORT_CODES + (NDIRECT) +
        ((MAXNBITS) << ((NPOSTFIX) + 1))
}

// pub struct BrotliState {
// total_written : usize,
// }
//
pub use state::BrotliState;
// impl BrotliState {
// pub fn new() -> BrotliState {
// return BrotliState {total_written: 0 };
// }
// }

/* Decodes WBITS by reading 1 - 7 bits, or 0x11 for "Large Window Brotli".
   Precondition: bit-reader accumulator has at least 8 bits. */
fn DecodeWindowBits(s_large_window: &mut bool,
                    s_window_bits:&mut u32,
                    br: &mut bit_reader::BrotliBitReader) -> BrotliDecoderErrorCode {
  let mut n: u32 = 0;
  let large_window = *s_large_window;
  *s_large_window = false;
  bit_reader::BrotliTakeBits(br, 1, &mut n);
  if (n == 0) {
    *s_window_bits = 16;
    return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
  }
  bit_reader::BrotliTakeBits(br, 3, &mut n);
  if (n != 0) {
    *s_window_bits = 17 + n;
    return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
  }
  bit_reader::BrotliTakeBits(br, 3, &mut n);
  if (n == 1) {
    if (large_window) {
      bit_reader::BrotliTakeBits(br, 1, &mut n);
      if (n == 1) {
        return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS;
      }
      *s_large_window = true;
      return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
    } else {
      return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS;
    }
  }
  if (n != 0) {
    *s_window_bits = 8 + n;
    return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
  }
  *s_window_bits = 17;
  return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
}


#[cold]
fn mark_unlikely() {}

fn DecodeVarLenUint8(substate_decode_uint8: &mut state::BrotliRunningDecodeUint8State,
                     mut br: &mut bit_reader::BrotliBitReader,
                     value: &mut u32,
                     input: &[u8])
                     -> BrotliDecoderErrorCode {
  let mut bits: u32 = 0;
  loop {
    match *substate_decode_uint8 {
      BrotliRunningDecodeUint8State::BROTLI_STATE_DECODE_UINT8_NONE => {
        if !bit_reader::BrotliSafeReadBits(&mut br, 1, &mut bits, input) {
          mark_unlikely();
          return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
        }
        if (bits == 0) {
          *value = 0;
          return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
        }
        *substate_decode_uint8 = BrotliRunningDecodeUint8State::BROTLI_STATE_DECODE_UINT8_SHORT;
        // No break, transit to the next state.
      }
      BrotliRunningDecodeUint8State::BROTLI_STATE_DECODE_UINT8_SHORT => {
        if !bit_reader::BrotliSafeReadBits(&mut br, 3, &mut bits, input) {
          mark_unlikely();
          *substate_decode_uint8 = BrotliRunningDecodeUint8State::BROTLI_STATE_DECODE_UINT8_SHORT;
          return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
        }
        if (bits == 0) {
          *value = 1;
          *substate_decode_uint8 = BrotliRunningDecodeUint8State::BROTLI_STATE_DECODE_UINT8_NONE;
          return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
        }
        // Use output value as a temporary storage. It MUST be persisted.
        *value = bits;
        // No break, transit to the next state.
        *substate_decode_uint8 = BrotliRunningDecodeUint8State::BROTLI_STATE_DECODE_UINT8_LONG;
      }
      BrotliRunningDecodeUint8State::BROTLI_STATE_DECODE_UINT8_LONG => {
        if !bit_reader::BrotliSafeReadBits(&mut br, *value, &mut bits, input) {
          mark_unlikely();
          *substate_decode_uint8 = BrotliRunningDecodeUint8State::BROTLI_STATE_DECODE_UINT8_LONG;
          return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
        }
        *value = (1u32 << *value) + bits;
        *substate_decode_uint8 = BrotliRunningDecodeUint8State::BROTLI_STATE_DECODE_UINT8_NONE;
        return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
      }
    }
  }
}

fn DecodeMetaBlockLength<AllocU8: alloc::Allocator<u8>,
                         AllocU32: alloc::Allocator<u32>,
                         AllocHC: alloc::Allocator<HuffmanCode>>
  (s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   input: &[u8])
   -> BrotliDecoderErrorCode {
  let mut bits: u32 = 0;
  loop {
    match s.substate_metablock_header {
      BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_NONE => {
        if !bit_reader::BrotliSafeReadBits(&mut s.br, 1, &mut bits, input) {
          return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
        }
        s.is_last_metablock = bits as u8;
        s.meta_block_remaining_len = 0;
        s.is_uncompressed = 0;
        s.is_metadata = 0;
        if (s.is_last_metablock == 0) {
          s.substate_metablock_header =
            BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_NIBBLES;
          continue;
        }
        s.substate_metablock_header =
          BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_EMPTY;
        // No break, transit to the next state.
      }
      BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_EMPTY => {
        if !bit_reader::BrotliSafeReadBits(&mut s.br, 1, &mut bits, input) {
          return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
        }
        if bits != 0 {
          s.substate_metablock_header =
            BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_NONE;
          return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
        }
        s.substate_metablock_header =
          BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_NIBBLES;
        // No break, transit to the next state.
      }
      BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_NIBBLES => {
        if !bit_reader::BrotliSafeReadBits(&mut s.br, 2, &mut bits, input) {
          return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
        }
        s.size_nibbles = (bits + 4) as u8;
        s.loop_counter = 0;
        if (bits == 3) {
          s.is_metadata = 1;
          s.substate_metablock_header =
            BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_RESERVED;
          continue;
        }
        s.substate_metablock_header =
          BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_SIZE;
        // No break, transit to the next state.

      }
      BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_SIZE => {
        let mut i = s.loop_counter;
        while i < s.size_nibbles as i32 {
          if !bit_reader::BrotliSafeReadBits(&mut s.br, 4, &mut bits, input) {
            s.loop_counter = i;
            return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
          }
          if (i + 1 == s.size_nibbles as i32 && s.size_nibbles > 4 && bits == 0) {
            return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_NIBBLE;
          }
          s.meta_block_remaining_len |= (bits << (i * 4)) as i32;
          i += 1;
        }
        s.substate_metablock_header =
          BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_UNCOMPRESSED;
        // No break, transit to the next state.
      }
      BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_UNCOMPRESSED => {
        if (s.is_last_metablock == 0 && s.is_metadata == 0) {
          if !bit_reader::BrotliSafeReadBits(&mut s.br, 1, &mut bits, input) {
            return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
          }
          s.is_uncompressed = bits as u8;
        }
        s.meta_block_remaining_len += 1;
        s.substate_metablock_header =
          BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_NONE;
        return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
      }
      BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_RESERVED => {
        if !bit_reader::BrotliSafeReadBits(&mut s.br, 1, &mut bits, input) {
          return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
        }
        if (bits != 0) {
          return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_RESERVED;
        }
        s.substate_metablock_header =
          BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_BYTES;
        // No break, transit to the next state.
      }
      BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_BYTES => {
        if !bit_reader::BrotliSafeReadBits(&mut s.br, 2, &mut bits, input) {
          return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
        }
        if (bits == 0) {
          s.substate_metablock_header =
            BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_NONE;
          return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
        }
        s.size_nibbles = bits as u8;
        s.substate_metablock_header =
          BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_METADATA;
        // No break, transit to the next state.
      }
      BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_METADATA => {
        let mut i = s.loop_counter;
        while i < s.size_nibbles as i32 {
          if !bit_reader::BrotliSafeReadBits(&mut s.br, 8, &mut bits, input) {
            s.loop_counter = i;
            return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
          }
          if (i + 1 == s.size_nibbles as i32 && s.size_nibbles > 1 && bits == 0) {
            return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_META_NIBBLE;
          }
          s.meta_block_remaining_len |= (bits << (i * 8)) as i32;
          i += 1;
        }
        s.substate_metablock_header =
          BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_UNCOMPRESSED;
        continue;
      }
    }
  }
}
// Decodes the Huffman code.
// This method doesn't read data from the bit reader, BUT drops the amount of
// bits that correspond to the decoded symbol.
// bits MUST contain at least 15 (BROTLI_HUFFMAN_MAX_CODE_LENGTH) valid bits.
#[inline(always)]
fn DecodeSymbol<T: HuffmanTable + ?Sized>(bits: u32, table: &T, br: &mut bit_reader::BrotliBitReader) -> u32 {
  let mut table_index = bits & HUFFMAN_TABLE_MASK;
  let mut table_element = table.entry(table_index as usize);
  if table_element.bits > HUFFMAN_TABLE_BITS as u8 {
    mark_unlikely();
    let nbits = table_element.bits - HUFFMAN_TABLE_BITS as u8;
    bit_reader::BrotliDropBits(br, HUFFMAN_TABLE_BITS);
    table_index += table_element.value as u32;
    let ext_idx = (table_index
                   + ((bits >> HUFFMAN_TABLE_BITS)
                      & bit_reader::BitMask(nbits as u32))) as usize;
    table_element = table.entry(ext_idx);
  }
  bit_reader::BrotliDropBits(br, table_element.bits as u32);
  table_element.value as u32
}

// Reads and decodes the next Huffman code from bit-stream.
// This method peeks 16 bits of input and drops 0 - 15 of them.
#[inline(always)]
fn ReadSymbol<T: HuffmanTable + ?Sized>(table: &T, br: &mut bit_reader::BrotliBitReader, input: &[u8]) -> u32 {
  DecodeSymbol(bit_reader::BrotliGet16BitsUnmasked(br, input), table, br)
}

// Same as DecodeSymbol, but it is known that there is less than 15 bits of
// input are currently available.
#[cold]
fn SafeDecodeSymbol<T: HuffmanTable + ?Sized>(table: &T,
                    mut br: &mut bit_reader::BrotliBitReader,
                    result: &mut u32)
                    -> bool {
  let mut available_bits = bit_reader::BrotliGetAvailableBits(br);
  if (available_bits == 0) {
    let root0 = table.entry(0);
    if (root0.bits == 0) {
      *result = root0.value as u32;
      return true;
    }
    return false; /* No valid bits at all. */
  }
  let mut val = bit_reader::BrotliGetBitsUnmasked(br) as u32;
  let table_index = (val & HUFFMAN_TABLE_MASK) as usize;
  let table_element = table.entry(table_index);
  if (table_element.bits <= HUFFMAN_TABLE_BITS as u8) {
    if (table_element.bits as u32 <= available_bits) {
      bit_reader::BrotliDropBits(&mut br, table_element.bits as u32);
      *result = table_element.value as u32;
      return true;
    } else {
      return false; /* Not enough bits for the first level. */
    }
  }
  if (available_bits <= HUFFMAN_TABLE_BITS) {
    return false; /* Not enough bits to move to the second level. */
  }

  // Speculatively drop HUFFMAN_TABLE_BITS.
  val = (val & bit_reader::BitMask(table_element.bits as u32)) >> HUFFMAN_TABLE_BITS;
  available_bits -= HUFFMAN_TABLE_BITS;
  let table_sub_element =
    table.entry(table_index + table_element.value as usize + val as usize);
  if (available_bits < table_sub_element.bits as u32) {
    return false; /* Not enough bits for the second level. */
  }

  bit_reader::BrotliDropBits(&mut br, HUFFMAN_TABLE_BITS + table_sub_element.bits as u32);
  *result = table_sub_element.value as u32;
  true
}

#[inline(always)]
fn SafeReadSymbol<T: HuffmanTable + ?Sized>(table: &T,
                  br: &mut bit_reader::BrotliBitReader,
                  result: &mut u32,
                  input: &[u8])
                  -> bool {
  let mut val: u32 = 0;
  if (bit_reader::BrotliSafeGetBits(br, 15, &mut val, input)) {
    *result = DecodeSymbol(val, table, br);
    return true;
  } else {
    mark_unlikely();
  }
  SafeDecodeSymbol(table, br, result)
}

// The literal trees of the 64 contexts before any are looked up.
static EMPTY_LITERAL_TABLE: LiteralTable =
  [HuffmanCode { value: 0, bits: 0 }; LITERAL_TABLE_WINDOW];
static EMPTY_CONTEXT_TREES: [&'static LiteralTable; 64] = [&EMPTY_LITERAL_TABLE; 64];
static EMPTY_DISTANCE_ROOT_TABLE: DistanceRootTable =
  [HuffmanCode { value: 0, bits: 0 }; DISTANCE_ROOT_WINDOW];
static EMPTY_DISTANCE_ROOTS: [&'static DistanceRootTable; 4] = [&EMPTY_DISTANCE_ROOT_TABLE; 4];

#[inline(always)]
fn decode_symbol_bits<T: HuffmanTable + ?Sized>(bits: u32, table: &T) -> (u32, u32) {
  let table_index = (bits & HUFFMAN_TABLE_MASK) as usize;
  let table_element = table.entry(table_index);
  if table_element.bits > HUFFMAN_TABLE_BITS as u8 {
    mark_unlikely();
    let sub_idx = table_index
      + table_element.value as usize
      + (((bits & bit_reader::BitMask(table_element.bits as u32)) >> HUFFMAN_TABLE_BITS) as usize);
    let sub = table.entry(sub_idx);
    // A 2nd-level subtable entry has at most `15 - 8 = 7` bits; masking with
    // `& 7` proves `consumed <= 15 < 32` across both arms so LLVM elides the
    // `consumed & SLICE_SHIFT_MASK` instruction in `decode_symbol_no_fill`.
    return (sub.value as u32, HUFFMAN_TABLE_BITS + (sub.bits as u32 & 7));
  }
  (table_element.value as u32, table_element.bits as u32)
}

#[inline(always)]
fn decode_symbol_no_fill<T: HuffmanTable + ?Sized>(
  fbr: &mut bit_reader::BitReaderSlice,
  table: &T,
) -> u32 {
  let (sym, consumed) = decode_symbol_bits(fbr.val as u32, table);
  fbr.avail_bits = fbr.avail_bits.wrapping_sub(consumed);
  fbr.val >>= consumed & bit_reader::SLICE_SHIFT_MASK;
  sym
}

#[inline(always)]
fn read_symbol_slice<T: HuffmanTable + ?Sized>(
  fbr: &mut bit_reader::BitReaderSlice,
  table: &T,
) -> u32 {
  fbr.fill16();
  decode_symbol_no_fill(fbr, table)
}

fn Log2Floor(mut x: u32) -> u32 {
  let mut result: u32 = 0;
  while x != 0 {
    x >>= 1;
    result += 1;
  }
  result
}


// Reads (s->symbol + 1) symbols.
// Totally 1..4 symbols are read, 1..11 bits each.
// The list of symbols MUST NOT contain duplicates.
//
fn ReadSimpleHuffmanSymbols<AllocU8: alloc::Allocator<u8>,
                            AllocU32: alloc::Allocator<u32>,
                            AllocHC: alloc::Allocator<HuffmanCode>>
  (alphabet_size: u32, max_symbol: u32,
   s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   input: &[u8])
   -> BrotliDecoderErrorCode {

  // max_bits == 1..11; symbol == 0..3; 1..44 bits will be read.
  let max_bits = Log2Floor(alphabet_size - 1);
  let mut i = s.sub_loop_counter;
  let num_symbols = s.symbol;
  for symbols_lists_item in fast_mut!((s.symbols_lists_array)[s.sub_loop_counter as usize;
                                                  num_symbols as usize + 1])
    .iter_mut() {
    let mut v: u32 = 0;
    if !bit_reader::BrotliSafeReadBits(&mut s.br, max_bits, &mut v, input) {
      mark_unlikely();
      s.sub_loop_counter = i;
      s.substate_huffman = BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_SIMPLE_READ;
      return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
    }
    if (v >= max_symbol) {
      return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_ALPHABET;
    }
    *symbols_lists_item = v as u16;
    BROTLI_LOG_UINT!(v);
    i += 1;
  }
  i = 0;
  for symbols_list_item in fast!((s.symbols_lists_array)[0; num_symbols as usize]).iter() {
    for other_item in fast!((s.symbols_lists_array)[i as usize + 1 ; num_symbols as usize+ 1])
      .iter() {
      if (*symbols_list_item == *other_item) {
        return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_SAME;
      }
    }
    i += 1;
  }
  BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS
}

// `BrotliState::symbol_lists_index` is initialized once to
// `BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1` (16, the number of negative-indexed
// list-head slots at the front of `symbols_lists_array`) and never mutated.
// Using the compile-time constant avoids reloading `s.symbol_lists_index` and
// keeps one fewer live register in `ReadSymbolCodeLengths`.
const SYMBOL_LISTS_OFFSET: usize = huffman::BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1;

// Process single decoded symbol code length:
// A) reset the repeat variable
// B) remember code length (if it is not 0)
// C) extend corredponding index-chain
// D) reduce the huffman space
// E) update the histogram
//
#[inline(always)]
fn ProcessSingleCodeLength(code_len: u32,
                           symbol: &mut u32,
                           repeat: &mut u32,
                           space: &mut u32,
                           prev_code_len: &mut u32,
                           symbol_lists: &mut [u16; huffman::BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1 +
                                                    huffman::BROTLI_HUFFMAN_MAX_CODE_LENGTHS_SIZE],
                           code_length_histo: &mut [u16; 16],
                           next_symbol: &mut [i32; 32]) {
  *repeat = 0;
  if (code_len != 0) {
    // code_len == 1..15
    // next_symbol may be negative, hence we offset by `SYMBOL_LISTS_OFFSET`
    let cl = (code_len as usize) & 15;
    let idx = (SYMBOL_LISTS_OFFSET as isize).wrapping_add(next_symbol[cl] as isize) as usize;
    debug_assert!(idx < symbol_lists.len());
    if let Some(slot) = symbol_lists.get_mut(idx) {
      *slot = (*symbol) as u16;
    }
    next_symbol[cl] = (*symbol) as i32;
    *prev_code_len = code_len;
    *space = space.wrapping_sub(32768 >> code_len);
    code_length_histo[cl] += 1;
    BROTLI_LOG!("[ReadHuffmanCode] code_length[{:}]={:} histo[]={:}\n",
                *symbol, code_len, code_length_histo[cl]);
  }
  (*symbol) += 1;
}

// Process repeated symbol code length.
// A) Check if it is the extension of previous repeat sequence; if the decoded
// value is not kCodeLengthRepeatCode, then it is a new symbol-skip
// B) Update repeat variable
// C) Check if operation is feasible (fits alphapet)
// D) For each symbol do the same operations as in ProcessSingleCodeLength
//
// PRECONDITION: code_len == kCodeLengthRepeatCode or kCodeLengthRepeatCode + 1
//
#[inline(always)]
fn ProcessRepeatedCodeLength(code_len: u32,
                             mut repeat_delta: u32,
                             alphabet_size: u32,
                             symbol: &mut u32,
                             repeat: &mut u32,
                             space: &mut u32,
                             prev_code_len: &mut u32,
                             repeat_code_len: &mut u32,
                             symbol_lists: &mut [u16; huffman::BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1 +
                                                      huffman::BROTLI_HUFFMAN_MAX_CODE_LENGTHS_SIZE],
                             code_length_histo: &mut [u16; 16],
                             next_symbol: &mut [i32; 32]) {
  let old_repeat: u32;
  let extra_bits: u32;
  let new_len: u32;
  if (code_len == kCodeLengthRepeatCode) {
    extra_bits = 2;
    new_len = *prev_code_len
  } else {
    extra_bits = 3;
    new_len = 0
  }
  if (*repeat_code_len != new_len) {
    *repeat = 0;
    *repeat_code_len = new_len;
  }
  old_repeat = *repeat;
  if (*repeat > 0) {
    *repeat -= 2;
    *repeat <<= extra_bits;
  }
  *repeat += repeat_delta + 3;
  repeat_delta = *repeat - old_repeat;
  if (*symbol + repeat_delta > alphabet_size) {
    *symbol = alphabet_size;
    *space = 0xFFFFF;
    return;
  }
  BROTLI_LOG!("[ReadHuffmanCode] code_length[{:}..{:}] = {:}\n",
              *symbol, *symbol + repeat_delta - 1, *repeat_code_len);
  if (*repeat_code_len != 0) {
    let last: u32 = *symbol + repeat_delta;
    let rcl = (*repeat_code_len as usize) & 15;
    let mut next: i32 = next_symbol[rcl];
    loop {
      let idx = (SYMBOL_LISTS_OFFSET as isize).wrapping_add(next as isize) as usize;
      debug_assert!(idx < symbol_lists.len());
      if let Some(slot) = symbol_lists.get_mut(idx) {
        *slot = (*symbol) as u16;
      }
      next = (*symbol) as i32;
      (*symbol) += 1;
      if *symbol == last {
        break;
      }
    }
    next_symbol[rcl] = next;
    *space = space.wrapping_sub(repeat_delta << (15 - *repeat_code_len));
    code_length_histo[rcl] = (code_length_histo[rcl] as u32 + repeat_delta) as u16;
  } else {
    *symbol += repeat_delta;
  }
}

// Reads and decodes symbol codelengths.
fn ReadSymbolCodeLengths<AllocU8: alloc::Allocator<u8>,
                         AllocU32: alloc::Allocator<u32>,
                         AllocHC: alloc::Allocator<HuffmanCode>>
  (alphabet_size: u32,
   s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   input: &[u8])
   -> BrotliDecoderErrorCode {

  let mut symbol = s.symbol;
  let mut repeat = s.repeat;
  let mut space = s.space;
  let mut prev_code_len: u32 = s.prev_code_len;
  let mut repeat_code_len: u32 = s.repeat_code_len;
  if (!bit_reader::BrotliWarmupBitReader(&mut s.br, input)) {
    return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
  }
  let mut br = match bit_reader::BitReaderSlice::new(&s.br, input) {
    Some(r) => r,
    None => return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE,
  };
  while (symbol < alphabet_size && space > 0) {
    if br.avail_in() < bit_reader::BROTLI_SHORT_FILL_BIT_WINDOW_READ {
      br.sync_to(&mut s.br);
      s.symbol = symbol;
      s.repeat = repeat;
      s.prev_code_len = prev_code_len;
      s.repeat_code_len = repeat_code_len;
      s.space = space;
      return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
    }
    br.fill_short();
    let p_index = (br.val as usize)
      & ((1usize << huffman::BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH) - 1);
    #[cfg(target_pointer_width = "64")]
    let p = s.table[p_index];
    #[cfg(not(target_pointer_width = "64"))]
    let p = s.table[..].entry(p_index);
    br.drop_bits(p.bits as u32); /* Use 1..5 bits */
    let code_len: u32 = p.value as u32; /* code_len == 0..17 */
    if (code_len < kCodeLengthRepeatCode) {
      ProcessSingleCodeLength(code_len,
                              &mut symbol,
                              &mut repeat,
                              &mut space,
                              &mut prev_code_len,
                              &mut s.symbols_lists_array,
                              &mut s.code_length_histo,
                              &mut s.next_symbol);
    } else {
      // code_len == 16..17, extra_bits == 2..3
      let extra_bits: u32 = if code_len == kCodeLengthRepeatCode {
        2
      } else {
        3
      };
      let repeat_delta: u32 = br.take_bits(extra_bits);
      ProcessRepeatedCodeLength(code_len,
                                repeat_delta,
                                alphabet_size,
                                &mut symbol,
                                &mut repeat,
                                &mut space,
                                &mut prev_code_len,
                                &mut repeat_code_len,
                                &mut s.symbols_lists_array,
                                &mut s.code_length_histo,
                                &mut s.next_symbol);
    }
  }
  br.sync_to(&mut s.br);
  s.space = space;
  BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS
}

fn SafeReadSymbolCodeLengths<AllocU8: alloc::Allocator<u8>,
                             AllocU32: alloc::Allocator<u32>,
                             AllocHC: alloc::Allocator<HuffmanCode>>
  (alphabet_size: u32,
   s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   input: &[u8])
   -> BrotliDecoderErrorCode {
  while (s.symbol < alphabet_size && s.space > 0) {
    let mut p_index = 0;
    let code_len: u32;
    let mut bits: u32 = 0;
    let available_bits: u32 = bit_reader::BrotliGetAvailableBits(&s.br);
    if (available_bits != 0) {
      bits = bit_reader::BrotliGetBitsUnmasked(&s.br) as u32;
    }
    p_index += bits &
               bit_reader::BitMask(huffman::BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH as u32);
    #[cfg(target_pointer_width = "64")]
    let p = fast!((s.table)[p_index as usize]);
    #[cfg(not(target_pointer_width = "64"))]
    let p = s.table[..].entry(p_index as usize);
    if (p.bits as u32 > available_bits) {
      // pullMoreInput;
      if (!bit_reader::BrotliPullByte(&mut s.br, input)) {
        return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
      }
      continue;
    }
    code_len = p.value as u32; /* code_len == 0..17 */
    if (code_len < kCodeLengthRepeatCode) {
      bit_reader::BrotliDropBits(&mut s.br, p.bits as u32);
      ProcessSingleCodeLength(code_len,
                              &mut s.symbol,
                              &mut s.repeat,
                              &mut s.space,
                              &mut s.prev_code_len,
                              &mut s.symbols_lists_array,
                              &mut s.code_length_histo,
                              &mut s.next_symbol);
    } else {
      // code_len == 16..17, extra_bits == 2..3
      let extra_bits: u32 = code_len - 14;
      let repeat_delta: u32 = (bits >> p.bits) & bit_reader::BitMask(extra_bits);
      if (available_bits < p.bits as u32 + extra_bits) {
        // pullMoreInput;
        if (!bit_reader::BrotliPullByte(&mut s.br, input)) {
          return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
        }
        continue;
      }
      bit_reader::BrotliDropBits(&mut s.br, p.bits as u32 + extra_bits);
      ProcessRepeatedCodeLength(code_len,
                                repeat_delta,
                                alphabet_size,
                                &mut s.symbol,
                                &mut s.repeat,
                                &mut s.space,
                                &mut s.prev_code_len,
                                &mut s.repeat_code_len,
                                &mut s.symbols_lists_array,
                                &mut s.code_length_histo,
                                &mut s.next_symbol);
    }
  }
  BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS
}

// Reads and decodes 15..18 codes using static prefix code.
// Each code is 2..4 bits long. In total 30..72 bits are used.
fn ReadCodeLengthCodeLengths<AllocU8: alloc::Allocator<u8>,
                             AllocU32: alloc::Allocator<u32>,
                             AllocHC: alloc::Allocator<HuffmanCode>>
  (s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   input: &[u8])
   -> BrotliDecoderErrorCode {

  let mut num_codes: u32 = s.repeat;
  let mut space: u32 = s.space;
  let mut i = s.sub_loop_counter;
  for code_length_code_order in
      fast!((kCodeLengthCodeOrder)[s.sub_loop_counter as usize; CODE_LENGTH_CODES]).iter() {
    let code_len_idx = *code_length_code_order;
    let mut ix: u32 = 0;

    if !bit_reader::BrotliSafeGetBits(&mut s.br, 4, &mut ix, input) {
      mark_unlikely();
      let available_bits: u32 = bit_reader::BrotliGetAvailableBits(&s.br);
      if (available_bits != 0) {
        ix = bit_reader::BrotliGetBitsUnmasked(&s.br) as u32 & 0xF;
      } else {
        ix = 0;
      }
      if (fast!((kCodeLengthPrefixLength)[ix as usize]) as u32 > available_bits) {
        s.sub_loop_counter = i;
        s.repeat = num_codes;
        s.space = space;
        s.substate_huffman = BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_COMPLEX;
        return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
      }
    }
    BROTLI_LOG_UINT!(ix);
    let v: u32 = fast!((kCodeLengthPrefixValue)[ix as usize]) as u32;
    bit_reader::BrotliDropBits(&mut s.br,
                               fast!((kCodeLengthPrefixLength)[ix as usize]) as u32);
    fast_mut!((s.code_length_code_lengths)[code_len_idx as usize]) = v as u8;
    BROTLI_LOG_ARRAY_INDEX!(s.code_length_code_lengths, code_len_idx);
    if v != 0 {
      space = space.wrapping_sub(32 >> v);
      num_codes += 1;
      fast_mut!((s.code_length_histo)[v as usize]) += 1;
      if space.wrapping_sub(1) >= 32 {
        // space is 0 or wrapped around
        break;
      }
    }
    i += 1;
  }
  if (!(num_codes == 1 || space == 0)) {
    return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_CL_SPACE;
  }
  BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS
}


// Decodes the Huffman tables.
// There are 2 scenarios:
// A) Huffman code contains only few symbols (1..4). Those symbols are read
// directly; their code lengths are defined by the number of symbols.
// For this scenario 4 - 49 bits will be read.
//
// B) 2-phase decoding:
// B.1) Small Huffman table is decoded; it is specified with code lengths
// encoded with predefined entropy code. 32 - 74 bits are used.
// B.2) Decoded table is used to decode code lengths of symbols in resulting
// Huffman table. In worst case 3520 bits are read.
//
fn ReadHuffmanCode<AllocU8: alloc::Allocator<u8>,
                   AllocU32: alloc::Allocator<u32>,
                   AllocHC: alloc::Allocator<HuffmanCode>>
  (mut alphabet_size: u32,
   max_symbol: u32,
   table: &mut [HuffmanCode],
   offset: usize,
   opt_table_size: Option<&mut u32>,
   s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   input: &[u8])
   -> BrotliDecoderErrorCode {
  if offset > table.len() {
    return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE;
  }
  // Unnecessary masking, but might be good for safety.
  alphabet_size &= 0x7ff;
  // State machine
  loop {
    match s.substate_huffman {
      BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_NONE => {
        if !bit_reader::BrotliSafeReadBits(&mut s.br, 2, &mut s.sub_loop_counter, input) {
          return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
        }

        BROTLI_LOG_UINT!(s.sub_loop_counter);
        // The value is used as follows:
        // 1 for simple code;
        // 0 for no skipping, 2 skips 2 code lengths, 3 skips 3 code lengths
        if (s.sub_loop_counter != 1) {
          s.space = 32;
          s.repeat = 0; /* num_codes */
          let max_code_len_len = huffman::BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH as usize + 1;
          for code_length_histo in fast_mut!((s.code_length_histo)[0;max_code_len_len]).iter_mut() {
            *code_length_histo = 0; // memset
          }
          for code_length_code_length in s.code_length_code_lengths[..].iter_mut() {
            *code_length_code_length = 0;
          }
          s.substate_huffman = BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_COMPLEX;
          // goto Complex;
          continue;
        }
        s.substate_huffman = BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_SIMPLE_SIZE;
        // No break, transit to the next state.
      }
      BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_SIMPLE_SIZE => {
        // Read symbols, codes & code lengths directly.
        if (!bit_reader::BrotliSafeReadBits(&mut s.br, 2, &mut s.symbol, input)) {
          // num_symbols
          s.substate_huffman = BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_SIMPLE_SIZE;
          return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
        }
        s.sub_loop_counter = 0;
        // No break, transit to the next state.
        s.substate_huffman = BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_SIMPLE_READ;
      }
      BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_SIMPLE_READ => {
        let result = ReadSimpleHuffmanSymbols(alphabet_size, max_symbol, s, input);
        match result {
          BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
          _ => return result,
        }
        // No break, transit to the next state.
        s.substate_huffman = BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_SIMPLE_BUILD;
      }
      BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_SIMPLE_BUILD => {
        let table_size: u32;
        if (s.symbol == 3) {
          let mut bits: u32 = 0;
          if (!bit_reader::BrotliSafeReadBits(&mut s.br, 1, &mut bits, input)) {
            s.substate_huffman = BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_SIMPLE_BUILD;
            return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
          }
          s.symbol += bits;
        }
        BROTLI_LOG_UINT!(s.symbol);
        table_size = huffman::BrotliBuildSimpleHuffmanTable(&mut table[offset..],
                                                            HUFFMAN_TABLE_BITS as i32,
                                                            &s.symbols_lists_array[..],
                                                            s.symbol);
        if table_size == 0 {
          return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE;
        }
        if let Some(opt_table_size_ref) = opt_table_size {
          *opt_table_size_ref = table_size
        }
        s.substate_huffman = BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_NONE;
        return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
      }

      // Decode Huffman-coded code lengths.
      BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_COMPLEX => {

        let result = ReadCodeLengthCodeLengths(s, input);
        match result {
          BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
          _ => return result,
        }
        if !huffman::BrotliBuildCodeLengthsHuffmanTable(
          &mut s.table,
          &s.code_length_code_lengths,
          &s.code_length_histo,
        ) {
          return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE;
        }
        for code_length_histo in s.code_length_histo.iter_mut() {
          *code_length_histo = 0;
        }

        let max_code_length = huffman::BROTLI_HUFFMAN_MAX_CODE_LENGTH as usize + 1;
        for (i, next_symbol_mut) in s.next_symbol[..max_code_length]
          .iter_mut()
          .enumerate() {
          *next_symbol_mut = i as i32 - (max_code_length as i32);
        }
        for symbol_list in s.symbols_lists_array[..max_code_length].iter_mut() {
          *symbol_list = 0xFFFF;
        }

        s.symbol = 0;
        s.prev_code_len = kDefaultCodeLength;
        s.repeat = 0;
        s.repeat_code_len = 0;
        s.space = 32768;
        // No break, transit to the next state.
        s.substate_huffman = BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_LENGTH_SYMBOLS;
      }
      BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_LENGTH_SYMBOLS => {
        let table_size: u32;
        let mut result = ReadSymbolCodeLengths(max_symbol, s, input);
        if let BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT = result {
          result = SafeReadSymbolCodeLengths(max_symbol, s, input)
        }
        match result {
          BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
          _ => return result,
        }

        if (s.space != 0) {
          BROTLI_LOG!("[ReadHuffmanCode] space = %d\n", s.space);
          return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE;
        }
        // Pass `s.symbol_lists_index` rather than `SYMBOL_LISTS_OFFSET` here so
        // LLVM does not constant-propagate 16 into `BrotliBuildHuffmanTable` and
        // unroll its `max_length` scan loop (which grows its stack frame on ARMv7).
        table_size = huffman::BrotliBuildHuffmanTable(fast_mut!((table)[offset;]),
                                                      HUFFMAN_TABLE_BITS as i32,
                                                      &s.symbols_lists_array[..],
                                                      s.symbol_lists_index,
                                                      &mut s.code_length_histo);
        if table_size == 0 {
          return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE;
        }
        if let Some(opt_table_size_ref) = opt_table_size {
          *opt_table_size_ref = table_size
        }
        s.substate_huffman = BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_NONE;
        return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
      }
    }
  }
}

// Decodes a block length by reading 3..39 bits.
#[inline(always)]
fn ReadBlockLength(table: &[HuffmanCode],
                   br: &mut bit_reader::BrotliBitReader,
                   input: &[u8])
                   -> u32 {
  let code = ReadSymbol(table, br, input);
  let p = &prefix::kBlockLengthPrefixCode[code as usize & 31];
  let nbits = p.nbits as u32; /*nbits==2..24*/
  p.offset as u32 + bit_reader::BrotliReadBits(br, nbits, input)
}


// WARNING: if state is not BROTLI_STATE_READ_BLOCK_LENGTH_NONE, then
// reading can't be continued with ReadBlockLength.
fn SafeReadBlockLengthIndex(substate_read_block_length: &state::BrotliRunningReadBlockLengthState,
                            block_length_index: u32,
                            table: &[HuffmanCode],
                            mut br: &mut bit_reader::BrotliBitReader,
                            input: &[u8])
                            -> (bool, u32) {
  match *substate_read_block_length {
    state::BrotliRunningReadBlockLengthState::BROTLI_STATE_READ_BLOCK_LENGTH_NONE => {
      let mut index: u32 = 0;
      if (!SafeReadSymbol(table, &mut br, &mut index, input)) {
        return (false, 0);
      }
      (true, index)
    }
    _ => (true, block_length_index),
  }
}
fn SafeReadBlockLengthFromIndex<
    AllocHC : alloc::Allocator<HuffmanCode> >(s : &mut BlockTypeAndLengthState<AllocHC>,
                                              br : &mut bit_reader::BrotliBitReader,
                                              result : &mut u32,
                                              res_index : (bool, u32),
                                              input : &[u8]) -> bool{
  let (res, index) = res_index;
  if !res {
    return false;
  }
  let mut bits: u32 = 0;
  let p = &prefix::kBlockLengthPrefixCode[index as usize & 31];
  let nbits = p.nbits; /* nbits==2..24 */
  if (!bit_reader::BrotliSafeReadBits(br, nbits as u32, &mut bits, input)) {
    s.block_length_index = index;
    s.substate_read_block_length =
      state::BrotliRunningReadBlockLengthState::BROTLI_STATE_READ_BLOCK_LENGTH_SUFFIX;
    return false;
  }
  *result = p.offset as u32 + bits;
  s.substate_read_block_length =
    state::BrotliRunningReadBlockLengthState::BROTLI_STATE_READ_BLOCK_LENGTH_NONE;
  true
}
macro_rules! SafeReadBlockLength (
   ($state : expr, $result : expr , $table : expr) => {
       SafeReadBlockLengthFromIndex(&mut $state, &mut $result,
                                    SafeReadBlockLengthIndex($state.substate_read_block_length,
                                                             $state.block_length_index,
                                                             $table,
                                                             &mut $state.br))
   };
);

// Transform:
// 1) initialize list L with values 0, 1,... 255
// 2) For each input element X:
// 2.1) let Y = L[X]
// 2.2) remove X-th element from L
// 2.3) prepend Y to L
// 2.4) append Y to output
//
// In most cases max(Y) <= 7, so most of L remains intact.
// To reduce the cost of initialization, we reuse L, remember the upper bound
// of Y values, and reinitialize only first elements in L.
//
// Most input values are 0, so a `while index > 0` loop skips the shift when
// `index == 0`. Masking with `& 0xff` proves to the compiler that every access
// to the 256-entry `mtf` array is in bounds.
fn InverseMoveToFrontTransform(v: &mut [u8],
                               v_len: u32,
                               mtf: &mut [u8;256],
                               mtf_upper_bound: &mut u32) {
  // Reinitialize elements that could have been changed.
  let upper_bound = (*mtf_upper_bound as usize).min(255);
  for (i, item) in mtf[..=upper_bound].iter_mut().enumerate() {
    *item = i as u8;
  }

  // Transform the input.
  let mut new_upper_bound: u32 = 0;
  for v_i in fast_mut!((v)[0usize ; (v_len as usize)]).iter_mut() {
    let mut index = (*v_i) as usize;
    let value = mtf[index];
    new_upper_bound |= index as u32;
    *v_i = value;
    while index > 0 {
      mtf[index & 0xff] = mtf[(index - 1) & 0xff];
      index -= 1;
    }
    mtf[0] = value;
  }
  // Remember amount of elements to be reinitialized.
  *mtf_upper_bound = new_upper_bound;
}
// Decodes a series of Huffman table using ReadHuffmanCode function.
fn HuffmanTreeGroupDecode<AllocU8: alloc::Allocator<u8>,
                          AllocU32: alloc::Allocator<u32>,
                          AllocHC: alloc::Allocator<HuffmanCode>>
  (group_index: i32,
   mut s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   input: &[u8])
   -> BrotliDecoderErrorCode {
  let mut hcodes: AllocHC::AllocatedMemory;
  let mut htrees: AllocU32::AllocatedMemory;
  let alphabet_size: u16;
  let group_num_htrees: u16;
  let group_max_symbol;
  if group_index == 0 {
    hcodes = mem::replace(&mut s.literal_hgroup.codes,
                          AllocHC::AllocatedMemory::default());
    htrees = mem::replace(&mut s.literal_hgroup.htrees,
                          AllocU32::AllocatedMemory::default());
    group_num_htrees = s.literal_hgroup.num_htrees;
    alphabet_size = s.literal_hgroup.alphabet_size;
    group_max_symbol = s.literal_hgroup.max_symbol;
  } else if group_index == 1 {
    hcodes = mem::replace(&mut s.insert_copy_hgroup.codes,
                          AllocHC::AllocatedMemory::default());
    htrees = mem::replace(&mut s.insert_copy_hgroup.htrees,
                          AllocU32::AllocatedMemory::default());
    group_num_htrees = s.insert_copy_hgroup.num_htrees;
    alphabet_size = s.insert_copy_hgroup.alphabet_size;
    group_max_symbol = s.insert_copy_hgroup.max_symbol;
  } else {
    if group_index != 2 {
      let ret = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE;
      SaveErrorCode!(s, ret);
      return ret;
    }
    hcodes = mem::replace(&mut s.distance_hgroup.codes,
                          AllocHC::AllocatedMemory::default());
    htrees = mem::replace(&mut s.distance_hgroup.htrees,
                          AllocU32::AllocatedMemory::default());
    group_num_htrees = s.distance_hgroup.num_htrees;
    alphabet_size = s.distance_hgroup.alphabet_size;
    group_max_symbol = s.distance_hgroup.max_symbol;
  }
  match s.substate_tree_group {
    BrotliRunningTreeGroupState::BROTLI_STATE_TREE_GROUP_NONE => {
      s.htree_next_offset = 0;
      s.htree_index = 0;
      s.substate_tree_group = BrotliRunningTreeGroupState::BROTLI_STATE_TREE_GROUP_LOOP;
    }
    BrotliRunningTreeGroupState::BROTLI_STATE_TREE_GROUP_LOOP => {}
  }
  let mut result = BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
  // This room also holds the windows the command loop reads the literal and
  // insert-and-copy trees through (`LITERAL_TABLE_WINDOW` and
  // `INSERT_COPY_TABLE_WINDOW` are the max table sizes of their alphabets).
  let max_table_size = huffman::BrotliHuffmanMaxTableSize(alphabet_size.max(group_max_symbol));
  for htree_iter in
      fast_mut!((htrees.slice_mut())[s.htree_index as usize ; (group_num_htrees as usize)])
    .iter_mut() {
    // Growing frees the old buffer.  Block trees must not be allocated after a
    // free (see EnsureBlockTrees), so allocate them first in that rare case.
    let room_needed = s.htree_next_offset as usize + max_table_size;
    if hcodes.slice().len() < room_needed && !EnsureBlockTrees(&mut s) {
      result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_BLOCK_TYPE_TREES;
      break;
    }
    if !huffman::ensure_codes_room(&mut hcodes, &mut s.alloc_hc, s.htree_next_offset as usize,
                                   max_table_size) {
      result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_TREE_GROUPS;
      break;
    }
    let mut table_size: u32 = 0;
    result = ReadHuffmanCode(u32::from(alphabet_size), u32::from(group_max_symbol),
                             hcodes.slice_mut(),
                             s.htree_next_offset as usize,
                             Some(&mut table_size),
                             &mut s,
                             input);
    match result {
      BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
      _ => break, // break and return the result code
    }
    *htree_iter = s.htree_next_offset;
    s.htree_next_offset += table_size;
    s.htree_index += 1;
  }
  if group_index == 0 {
    let _ = mem::replace(&mut s.literal_hgroup.codes,
                 mem::replace(&mut hcodes, AllocHC::AllocatedMemory::default()));
    let _ = mem::replace(&mut s.literal_hgroup.htrees,
                 mem::replace(&mut htrees, AllocU32::AllocatedMemory::default()));
  } else if group_index == 1 {
    let _ = mem::replace(&mut s.insert_copy_hgroup.codes,
                 mem::replace(&mut hcodes, AllocHC::AllocatedMemory::default()));
    let _ = mem::replace(&mut s.insert_copy_hgroup.htrees,
                 mem::replace(&mut htrees, AllocU32::AllocatedMemory::default()));
  } else {
    let _ = mem::replace(&mut s.distance_hgroup.codes,
                 mem::replace(&mut hcodes, AllocHC::AllocatedMemory::default()));
    let _ = mem::replace(&mut s.distance_hgroup.htrees,
                 mem::replace(&mut htrees, AllocU32::AllocatedMemory::default()));
  }
  if let BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS = result {
    s.substate_tree_group = BrotliRunningTreeGroupState::BROTLI_STATE_TREE_GROUP_NONE
  }
  result
}
#[allow(dead_code)]
pub fn lg_window_size(first_byte: u8, second_byte: u8) -> Result<(u8, u8), ()> {
  if first_byte & 1 == 0 {
    return Ok((16, 1));
  }
  match first_byte & 15 {
    0x3 => return Ok((18, 4)),
    0x5 => return Ok((19, 4)),
    0x7 => return Ok((20, 4)),
    0x9 => return Ok((21, 4)),
    0xb => return Ok((22, 4)),
    0xd => return Ok((23, 4)),
    0xf => return Ok((24, 4)),
    _ => match first_byte & 127 {
      0x71 => return Ok((15, 7)),
      0x61 => return Ok((14, 7)),
      0x51 => return Ok((13, 7)),
      0x41 => return Ok((12, 7)),
      0x31 => return Ok((11, 7)),
      0x21 => return Ok((10, 7)),
      0x1 => return Ok((17, 7)),
      _ => {},
    }
  }
  if (first_byte & 0x80) != 0 {
    return Err(());
  }
  let ret  = second_byte & 0x3f;
  if ret < 10 || ret > 30 {
    return Err(());
  }
  Ok((ret, 14))

}


fn bzero(data: &mut [u8]) {
  for iter in data.iter_mut() {
    *iter = 0;
  }
}


// Decodes a context map.
// Decoding is done in 4 phases:
// 1) Read auxiliary information (6..16 bits) and allocate memory.
// In case of trivial context map, decoding is finished at this phase.
// 2) Decode Huffman table using ReadHuffmanCode function.
// This table will be used for reading context map items.
// 3) Read context map items; "0" values could be run-length encoded.
// 4) Optionally, apply InverseMoveToFront transform to the resulting map.
//
fn DecodeContextMapInner<AllocU8: alloc::Allocator<u8>,
                         AllocU32: alloc::Allocator<u32>,
                         AllocHC: alloc::Allocator<HuffmanCode>>
  (context_map_size: u32,
   num_htrees: &mut u32,
   context_map_arg: &mut AllocU8::AllocatedMemory,
   mut s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   input: &[u8])
   -> BrotliDecoderErrorCode {

  let mut result;
  loop {
    match s.substate_context_map {
      BrotliRunningContextMapState::BROTLI_STATE_CONTEXT_MAP_NONE => {
        result = DecodeVarLenUint8(&mut s.substate_decode_uint8, &mut s.br, num_htrees, input);
        match result {
          BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
          _ => return result,
        }
        (*num_htrees) += 1;
        s.context_index = 0;
        BROTLI_LOG_UINT!(context_map_size);
        BROTLI_LOG_UINT!(*num_htrees);
        if context_map_arg.slice().len() < context_map_size as usize {
          s.alloc_u8.free_cell(core::mem::take(context_map_arg));
          *context_map_arg = s.alloc_u8.alloc_cell(context_map_size as usize);
          if (context_map_arg.slice().len() < context_map_size as usize) {
            return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MAP;
          }
        }
        if (*num_htrees <= 1) {
          bzero(&mut context_map_arg.slice_mut()[..context_map_size as usize]);
          return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
        }
        s.substate_context_map = BrotliRunningContextMapState::BROTLI_STATE_CONTEXT_MAP_READ_PREFIX;
        // No break, continue to next state.
      }
      BrotliRunningContextMapState::BROTLI_STATE_CONTEXT_MAP_READ_PREFIX => {
        let mut bits: u32 = 0;
        // In next stage ReadHuffmanCode uses at least 4 bits, so it is safe
        // to peek 4 bits ahead.
        if (!bit_reader::BrotliSafeGetBits(&mut s.br, 5, &mut bits, input)) {
          return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
        }
        if ((bits & 1) != 0) {
          // Use RLE for zeroes.
          s.max_run_length_prefix = (bits >> 1) + 1;
          bit_reader::BrotliDropBits(&mut s.br, 5);
        } else {
          s.max_run_length_prefix = 0;
          bit_reader::BrotliDropBits(&mut s.br, 1);
        }
        BROTLI_LOG_UINT!(s.max_run_length_prefix);
        s.substate_context_map = BrotliRunningContextMapState::BROTLI_STATE_CONTEXT_MAP_HUFFMAN;
        // No break, continue to next state.
      }
      BrotliRunningContextMapState::BROTLI_STATE_CONTEXT_MAP_HUFFMAN => {
        if !EnsureContextMapTable(s) {
          return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MAP;
        }
        let mut local_context_map_table = mem::replace(&mut s.context_map_table,
                                                       AllocHC::AllocatedMemory::default());
        let alphabet_size = *num_htrees + s.max_run_length_prefix;
        result = ReadHuffmanCode(alphabet_size, alphabet_size,
                                 &mut local_context_map_table.slice_mut(),
                                 0,
                                 None,
                                 &mut s,
                                 input);
        let _ = mem::replace(&mut s.context_map_table,
                     mem::replace(&mut local_context_map_table,
                                  AllocHC::AllocatedMemory::default()));
        match result {
          BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
          _ => return result,
        }
        s.code = 0xFFFF;
        s.substate_context_map = BrotliRunningContextMapState::BROTLI_STATE_CONTEXT_MAP_DECODE;
        // No break, continue to next state.
      }
      BrotliRunningContextMapState::BROTLI_STATE_CONTEXT_MAP_DECODE => {
        let mut context_index: u32 = s.context_index;
        let max_run_length_prefix: u32 = s.max_run_length_prefix;
        let context_map = &mut context_map_arg.slice_mut()[..context_map_size as usize];
        let context_map_table = s.context_map_table.slice();
        let mut code: u32 = s.code;
        let mut rleCodeGoto = (code != 0xFFFF);
        while (rleCodeGoto || context_index < context_map_size) {
          if !rleCodeGoto {
            if (!SafeReadSymbol(context_map_table, &mut s.br, &mut code, input)) {
              s.code = 0xFFFF;
              s.context_index = context_index;
              return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
            }
            BROTLI_LOG_UINT!(code);

            if code == 0 {
              fast_mut!((context_map)[context_index as usize]) = 0;
              BROTLI_LOG_ARRAY_INDEX!(context_map, context_index as usize);
              context_index += 1;
              continue;
            }
            if code > max_run_length_prefix {
              fast_mut!((context_map)[context_index as usize]) =
                (code - max_run_length_prefix) as u8;
              BROTLI_LOG_ARRAY_INDEX!(context_map, context_index as usize);
              context_index += 1;
              continue;
            }
          }
          rleCodeGoto = false; // <- this was a goto beforehand... now we have reached label
          // we are treated like everyday citizens from this point forth
          {
            let mut reps: u32 = 0;
            if (!bit_reader::BrotliSafeReadBits(&mut s.br, code, &mut reps, input)) {
              s.code = code;
              s.context_index = context_index;
              return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
            }
            reps += 1u32 << code;
            BROTLI_LOG_UINT!(reps);
            if (context_index + reps > context_map_size) {
              return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_CONTEXT_MAP_REPEAT;
            }
            for slot in context_map[context_index as usize..(context_index + reps) as usize].iter_mut() {
              *slot = 0;
            }
            context_index += reps;
          }
        }
        // No break, continue to next state.
        s.substate_context_map = BrotliRunningContextMapState::BROTLI_STATE_CONTEXT_MAP_TRANSFORM;
      }
      BrotliRunningContextMapState::BROTLI_STATE_CONTEXT_MAP_TRANSFORM => {
        let mut bits: u32 = 0;
        if (!bit_reader::BrotliSafeReadBits(&mut s.br, 1, &mut bits, input)) {
          s.substate_context_map = BrotliRunningContextMapState::BROTLI_STATE_CONTEXT_MAP_TRANSFORM;
          return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
        }
        if (bits != 0) {
          if let Ok(ref mut mtf) = s.mtf_or_error_string {
            InverseMoveToFrontTransform(context_map_arg.slice_mut(),
                                        context_map_size,
                                        mtf,
                                        &mut s.mtf_upper_bound);
          } else {
            // the error state is stored here--we can't make it this deep with an active error
            return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE;
          }
        }
        s.substate_context_map = BrotliRunningContextMapState::BROTLI_STATE_CONTEXT_MAP_NONE;
        return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
      }
    }
  }
  // unreachable!(); (compiler will error if it's reachable due to the unit return type)
}

fn DecodeContextMap<AllocU8: alloc::Allocator<u8>,
                    AllocU32: alloc::Allocator<u32>,
                    AllocHC: alloc::Allocator<HuffmanCode>>
  (context_map_size: usize,
   is_dist_context_map: bool,
   mut s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   input: &[u8])
   -> BrotliDecoderErrorCode {

  match s.state {
    BrotliRunningState::BROTLI_STATE_CONTEXT_MAP_1 if !is_dist_context_map => {},
    BrotliRunningState::BROTLI_STATE_CONTEXT_MAP_2 if is_dist_context_map => {},
    _ => return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE,
  }
  let (mut num_htrees, mut context_map_arg) = if is_dist_context_map {
    (s.num_dist_htrees, mem::replace(&mut s.dist_context_map, AllocU8::AllocatedMemory::default()))
  } else {
    (s.num_literal_htrees, mem::replace(&mut s.context_map, AllocU8::AllocatedMemory::default()))
  };

  let retval = DecodeContextMapInner(context_map_size as u32,
                                     &mut num_htrees,
                                     &mut context_map_arg,
                                     &mut s,
                                     input);
  if is_dist_context_map {
    s.num_dist_htrees = num_htrees;
    let _ = mem::replace(&mut s.dist_context_map,
                 mem::replace(&mut context_map_arg, AllocU8::AllocatedMemory::default()));
  } else {
    s.num_literal_htrees = num_htrees;
    let _ = mem::replace(&mut s.context_map,
                 mem::replace(&mut context_map_arg, AllocU8::AllocatedMemory::default()));
  }
  retval
}

// Decodes a command or literal and updates block type ringbuffer.
// Reads 3..54 bits.
// A block switch in a category with a single block type is a format error,
// not a lack of input: reporting it as BROTLI_DECODER_NEEDS_MORE_INPUT makes
// the streaming decoder buffer input that will never complete the read
// (CVE-2026-111497 in the C decoder).
#[inline(always)]
fn DecodeBlockTypeAndLength<
  AllocHC : alloc::Allocator<HuffmanCode>> (safe : bool,
                                            s : &mut BlockTypeAndLengthState<AllocHC>,
                                            br : &mut bit_reader::BrotliBitReader,
                                            tree_type : i32,
                                            input : &[u8]) -> BrotliDecoderErrorCode {
  let max_block_type = fast!((s.num_block_types)[tree_type as usize]);
  let type_tree_offset = tree_type as usize * huffman::BROTLI_HUFFMAN_MAX_SIZE_258 as usize;
  let len_tree_offset = tree_type as usize * huffman::BROTLI_HUFFMAN_MAX_SIZE_26 as usize;

  let mut block_type: u32 = 0;
  if max_block_type <= 1 {
    return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_BLOCK_SWITCH;
  }
  // Read 0..15 + 3..39 bits
  if (!safe) {
    block_type = ReadSymbol(fast_slice!((s.block_type_trees)[type_tree_offset;]), br, input);
    fast_mut!((s.block_length)[tree_type as usize]) =
      ReadBlockLength(fast_slice!((s.block_len_trees)[len_tree_offset;]), br, input);
  } else {
    let memento = bit_reader::BrotliBitReaderSaveState(br);
    if (!SafeReadSymbol(fast_slice!((s.block_type_trees)[type_tree_offset;]),
                        br,
                        &mut block_type,
                        input)) {
      return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
    }
    let mut block_length_out: u32 = 0;

    let index_ret = SafeReadBlockLengthIndex(&s.substate_read_block_length,
                                             s.block_length_index,
                                             fast_slice!((s.block_len_trees)[len_tree_offset;]),
                                             br,
                                             input);
    if !SafeReadBlockLengthFromIndex(s, br, &mut block_length_out, index_ret, input) {
      s.substate_read_block_length =
        BrotliRunningReadBlockLengthState::BROTLI_STATE_READ_BLOCK_LENGTH_NONE;
      bit_reader::BrotliBitReaderRestoreState(br, &memento);
      return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
    }
    fast_mut!((s.block_length)[tree_type as usize]) = block_length_out;
  }
  let rb_idx = (tree_type as usize & 3) * 2;
  let rb0 = s.block_type_rb[rb_idx];
  let rb1 = s.block_type_rb[rb_idx + 1];
  if (block_type == 1) {
    block_type = rb1 + 1;
  } else if (block_type == 0) {
    block_type = rb0;
  } else {
    block_type -= 2;
  }
  if (block_type >= max_block_type) {
    block_type -= max_block_type;
  }
  s.block_type_rb[rb_idx] = rb1;
  s.block_type_rb[rb_idx + 1] = block_type;
  BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS
}
fn DetectTrivialLiteralBlockTypes<AllocU8: alloc::Allocator<u8>,
                                  AllocU32: alloc::Allocator<u32>,
                                  AllocHC: alloc::Allocator<HuffmanCode>>
  (s: &mut BrotliState<AllocU8, AllocU32, AllocHC>) {
  for current in s.trivial_literal_contexts.iter_mut() {
    *current = 0;
  }
  let num_block_types = fast!((s.block_type_length_state.num_block_types)[0]) as usize;
  let context_map = s.context_map.slice();
  for i in 0..num_block_types {
    let offset = i << kLiteralContextBits;
    if let Some(chunk) = context_map.get(offset..offset + (1usize << kLiteralContextBits)) {
      let sample = chunk[0];
      if chunk.iter().all(|&b| b == sample) {
        s.trivial_literal_contexts[i >> 5] |= 1u32 << (i & 31);
      }
    }
  }
}
#[inline(always)]
fn update_literal_block_state(
  block_type_rb: &[u32; 6],
  trivial_literal_contexts: &[u32; 8],
  context_map: &[u8],
  context_modes: &[u8],
  context_map_slice_index: &mut usize,
  trivial_literal_context: &mut i32,
  literal_htree_index: &mut u8,
  context_lookup: &mut &'static [u8; 512],
) {
  let block_type = fast!((block_type_rb)[1]) as usize;
  let context_offset = block_type << kLiteralContextBits;
  *context_map_slice_index = context_offset;
  let trivial = fast!((trivial_literal_contexts)[(block_type >> 5) & 7]);
  *trivial_literal_context = ((trivial >> (block_type & 31)) & 1) as i32;
  *literal_htree_index = fast!((context_map)[context_offset]);
  let context_mode_index = fast!((context_modes)[block_type]) & 3;
  *context_lookup = &kContextLookup[context_mode_index as usize];
}

#[inline(always)]
fn PrepareLiteralDecoding<AllocU8: alloc::Allocator<u8>,
                          AllocU32: alloc::Allocator<u32>,
                          AllocHC: alloc::Allocator<HuffmanCode>>
  (s: &mut BrotliState<AllocU8, AllocU32, AllocHC>) {
  update_literal_block_state(
    &s.block_type_length_state.block_type_rb,
    &s.trivial_literal_contexts,
    s.context_map.slice(),
    s.context_modes.slice(),
    &mut s.context_map_slice_index,
    &mut s.trivial_literal_context,
    &mut s.literal_htree_index,
    &mut s.context_lookup,
  );
}

// Bytes decoded into the ring buffer but not yet handed to the caller.
// Callers must have established CheckRingBufferConsistency.
fn UnwrittenBytes<AllocU8: alloc::Allocator<u8>,
                  AllocU32: alloc::Allocator<u32>,
                  AllocHC: alloc::Allocator<HuffmanCode>> (
  s: &BrotliState<AllocU8, AllocU32, AllocHC>,
  wrap: bool,
)  -> u64 {
  debug_assert!(s.pos >= 0 && s.ringbuffer_size > 0);
  let ringbuffer_size = s.ringbuffer_size as u64;
  let pos = if wrap && s.pos > s.ringbuffer_size {
    ringbuffer_size
  } else {
    s.pos as u64
  };
  let partial_pos_rb = s.rb_roundtrips * ringbuffer_size + pos;
  debug_assert!(partial_pos_rb >= s.partial_pos_out);
  partial_pos_rb - s.partial_pos_out
}

// The invariant every ring-buffer read depends on: a positive size matching the
// mask, and a `pos` inside the allocation. Only a decoder bug can break it, but
// it stays a runtime check because the `unsafe` build would read out of bounds
// rather than panic.
fn CheckRingBufferConsistency<AllocU8: alloc::Allocator<u8>,
                              AllocU32: alloc::Allocator<u32>,
                              AllocHC: alloc::Allocator<HuffmanCode>>(
  s: &BrotliState<AllocU8, AllocU32, AllocHC>,
) -> bool {
  s.pos >= 0 && s.ringbuffer_size > 0 && s.ringbuffer_mask >= 0 &&
    s.ringbuffer_mask as i64 + 1 == s.ringbuffer_size as i64 &&
    s.ringbuffer_size as usize <= s.ringbuffer.slice().len()
}
fn WriteRingBuffer<'a,
                   AllocU8: alloc::Allocator<u8>,
                   AllocU32: alloc::Allocator<u32>,
                   AllocHC: alloc::Allocator<HuffmanCode>>(
  available_out: &mut usize,
  opt_output: Option<&mut [u8]>,
  output_offset: &mut usize,
  total_out: &mut usize,
  force: bool,
  s: &'a mut BrotliState<AllocU8, AllocU32, AllocHC>,
) -> (BrotliDecoderErrorCode, &'a [u8]) {
  if s.meta_block_remaining_len < 0 {
    return (BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_1, &[]);
  }
  if !CheckRingBufferConsistency(s) {
    return (BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE, &[]);
  }
  // window_bits is 9..=24, or 10..=30 for large-window streams; masked so the
  // shift stays in range regardless.
  debug_assert!(s.window_bits < 64);
  let window_size = 1i64 << (s.window_bits & 63);
  let to_write = UnwrittenBytes(s, true);
  let num_written = core::cmp::min(*available_out as u64, to_write) as usize;
  let start_index = (s.partial_pos_out & s.ringbuffer_mask as u64) as usize;
  let start = match s.ringbuffer.slice().get(start_index..start_index + num_written) {
    Some(start) => start,
    None => return (BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE, &[]),
  };
  // output is caller-supplied, hence INVALID_ARGUMENTS rather than UNREACHABLE.
  let output_end = match (*output_offset).checked_add(num_written) {
    Some(output_end) => output_end,
    None => return (BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS, &[]),
  };
  if let Some(output) = opt_output {
    match output.get_mut(*output_offset..output_end) {
      Some(output_slice) => output_slice.clone_from_slice(start),
      None => return (BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS, &[]),
    }
  }
  *output_offset = output_end;
  *available_out -= num_written;
  BROTLI_LOG_UINT!(to_write);
  BROTLI_LOG_UINT!(num_written);
  s.partial_pos_out += num_written as u64;
  // The public API reports total_out as usize, matching C's size_t: on a 32-bit
  // target it truncates past 4GiB, while the internal accounting does not.
  *total_out = s.partial_pos_out as usize;
  if (num_written as u64) < to_write {
    if s.ringbuffer_size as i64 == window_size || force {
      // TakeOutput still needs the bytes consumed by this partial write.
      return (BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_OUTPUT, start);
    } else {
      return (BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS, start);
    }
  }
  if (s.ringbuffer_size as i64 == window_size &&
      s.pos >= s.ringbuffer_size) {
    s.pos -= s.ringbuffer_size;
    s.rb_roundtrips += 1;
    s.should_wrap_ringbuffer = s.pos != 0;
  }
  (BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS, start)
 }

fn WrapRingBuffer<AllocU8: alloc::Allocator<u8>,
                   AllocU32: alloc::Allocator<u32>,
                   AllocHC: alloc::Allocator<HuffmanCode>>(
  s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
) -> bool {
  if s.should_wrap_ringbuffer {
    if s.ringbuffer_size < 0 || s.pos < 0 {
      return false;
    }
    let ringbuffer_size = s.ringbuffer_size as usize;
    let pos = s.pos as usize;
    let ringbuffer_len = s.ringbuffer.slice().len();
    // A transformed custom-dictionary word can overshoot the end by at most
    // 540 bytes (255-byte prefix + 31-byte word + 255-byte suffix, starting at
    // the final ring-buffer byte), which is why `pos` may exceed
    // `ringbuffer_size`. Wrapping is only done at the full window size, whose
    // minimum is 1 << kBrotliLargeMinWbits == 1024, and the tail region is
    // kRingBufferWriteAheadSlack + kBrotliMaxDictionaryWordLength bytes long.
    if ringbuffer_size > ringbuffer_len ||
       pos > ringbuffer_size ||
       pos > ringbuffer_len - ringbuffer_size {
      return false;
    }
    let (ring_buffer_start, ring_buffer_end) =
      s.ringbuffer.slice_mut().split_at_mut(ringbuffer_size);
    ring_buffer_start[..pos].clone_from_slice(&ring_buffer_end[..pos]);
    s.should_wrap_ringbuffer = false;
  }
  true
}

fn CopyUncompressedBlockToOutput<AllocU8: alloc::Allocator<u8>,
                                 AllocU32: alloc::Allocator<u32>,
                                 AllocHC: alloc::Allocator<HuffmanCode>>
  (mut available_out: &mut usize,
   mut output: &mut [u8],
   mut output_offset: &mut usize,
   mut total_out: &mut usize,
   mut s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   input: &[u8])
   -> BrotliDecoderErrorCode {
  // Sizes the ring buffer for this metablock; a no-op once that is done, as
  // on re-entry after NEEDS_MORE_INPUT.
  match BrotliEnsureRingBuffer(s,
                               BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_1) {
    BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
    error => return error,
  }
  // State machine
  loop {
    match s.substate_uncompressed {
      BrotliRunningUncompressedState::BROTLI_STATE_UNCOMPRESSED_NONE => {
        let remaining = bit_reader::BrotliGetRemainingBytes(&s.br);
        let mut nbytes = core::cmp::min(remaining, s.meta_block_remaining_len as u32);
        nbytes = core::cmp::min(nbytes, (s.ringbuffer_size - s.pos) as u32);
        // Copy remaining bytes from s.br.buf_ to ringbuffer.
        bit_reader::BrotliCopyBytes(fast_mut!((s.ringbuffer.slice_mut())[s.pos as usize;]),
                                    &mut s.br,
                                    nbytes,
                                    input);
        s.pos += nbytes as i32;
        s.meta_block_remaining_len -= nbytes as i32;
        if s.pos < (1 << s.window_bits) {
          if (s.meta_block_remaining_len == 0) {
            return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
          }
          return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
        }
        s.substate_uncompressed = BrotliRunningUncompressedState::BROTLI_STATE_UNCOMPRESSED_WRITE;
        // s.partial_pos_rb += (size_t)s.ringbuffer_size;
        // No break, continue to next state by going aroudn the loop
      }
      BrotliRunningUncompressedState::BROTLI_STATE_UNCOMPRESSED_WRITE => {
        let (result, _) = WriteRingBuffer(&mut available_out,
                                          Some(&mut output),
                                          &mut output_offset,
                                          &mut total_out,
                                          false,
                                          &mut s);
        match result {
          BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
          _ => return result,
        }
        if s.ringbuffer_size == 1 << s.window_bits {
          s.max_distance = s.max_backward_distance;
        }
        s.substate_uncompressed = BrotliRunningUncompressedState::BROTLI_STATE_UNCOMPRESSED_NONE;
      }
    }
  }
}

// We need the slack region for the following reasons:
// - doing up to two 16-byte copies for fast backward copying (32 bytes,
//   subsumed by the dictionary-word case below)
// - inserting a transformed dictionary word past the end of the ring
//   buffer: prefix + base word + suffix. The built-in transforms need only
//   5 + 24 + 8 = 37, but a serialized shared dictionary may define
//   transforms with full-length affixes, so a word starting on the last
//   ring-buffer byte can write this far past it.
const kRingBufferWriteAheadSlack: i32 =
  SHARED_BROTLI_MAX_TRANSFORM_AFFIX_LENGTH as i32           // 255-byte prefix
  + (SHARED_BROTLI_MAX_DICTIONARY_WORD_LENGTH as i32 + 1)   //  32: 31-byte word, rounded up
  + SHARED_BROTLI_MAX_TRANSFORM_AFFIX_LENGTH as i32;        // 255-byte suffix => 542
// The C implementation hardcodes 542; fail the build if the two drift apart.
const _RING_BUFFER_SLACK_MATCHES_C: [(); 542] = [(); kRingBufferWriteAheadSlack as usize];
// Bytes allocated past ringbuffer_size: the write-ahead slack plus one more
// maximal built-in dictionary word, as this decoder has always allocated.
const kRingBufferAllocationSlack: usize =
  kRingBufferWriteAheadSlack as usize + kBrotliMaxDictionaryWordLength as usize;
// Smallest ring buffer allocated, as in the C decoder: starting at 1 KiB saves
// reallocations while the ring buffer grows. Any window is at least this large
// (1 << kBrotliLargeMinWbits).
const kRingBufferMinSize: i64 = 1024;
// A one-shot caller's output buffer may be far larger than the stream. Use it
// to grow at most four times the ring size needed by the decoded data alone.
const kOneShotRingBufferGrowthFactor: i64 = 4;
// Smallest ring buffer allocated for a last metablock, after which the ring
// buffer never grows, so the C decoder's larger minimum would buy nothing: the
// 32 bytes this decoder used for a stream of one short metablock before growth
// on demand. A fixed u8 pool (see u8_pool_size) starts every ring buffer this
// small, which also covers a short uncompressed metablock followed by an empty
// last one when BrotliCalculateRingBufferSize cannot see that last one yet.
const kSmallRingBufferMinSize: i64 = 32;

// Calculates the smallest feasible ring buffer; a port of the C decoder's
// BrotliCalculateRingBufferSize. The ring buffer starts small and, whenever a
// metablock needs more room, grows to the smallest power of two that holds
// everything decoded so far plus that metablock, up to the window size. So a
// short stream never pays for the whole window (4 MiB at the encoder's default
// lgwin of 22), even when its data is not in the last metablock. Once the ring
// buffer reaches the window size it is never reallocated again. Unlike C, a
// ring buffer first allocated for the last metablock may be smaller than 1 KiB.
//
// A fixed u8 pool (u8_pool_size != 0) cannot merge the blocks that growth
// frees, so growing to the whole window takes up to twice the window from it.
// For such a pool the ring buffer is sized so that it never needs more of the
// pool than before growth on demand: a pool that can hold the whole window
// gets it at once for any metablock but the last one, as it always did, and
// otherwise the ring buffer grows from 32 bytes.
//
// A caller can raise the size the ring buffer starts from with
// set_initial_ring_buffer_size; u32::MAX allocates the whole window at once.
// A last metablock is still sized to fit, as the ring buffer never grows
// after it.
//
// An uncompressed metablock is never the last one, but encoders store small or
// incompressible input as one followed by an empty last metablock. So, as this
// decoder did before growth on demand, the first ring buffer for an
// uncompressed metablock is sized as for a last one when the header after its
// data, peeked from `input`, is an empty last metablock. If that header is not
// in `input` yet, the metablock is sized as one that is not last.
//
// This only computes new_ringbuffer_size: BrotliEnsureRingBuffer (re)allocates
// once the metablock is about to write to the ring buffer.
//
// When this method is called, metablock size and flags MUST be decoded.
fn BrotliCalculateRingBufferSize<AllocU8: alloc::Allocator<u8>,
                                 AllocU32: alloc::Allocator<u32>,
                                 AllocHC: alloc::Allocator<HuffmanCode>>
  (s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   input: &[u8]) {
  // window_bits is 10..=24, or 10..=30 for large-window streams. The sizes are
  // computed as i64, and the shift masked, so no state can overflow them.
  let window_size = 1i64 << (s.window_bits & 63);
  let mut new_ringbuffer_size = window_size;
  let fixed_pool = s.u8_pool_size != 0;
  // Whether no data follows this metablock.
  let mut is_last = s.is_last_metablock != 0;
  if s.is_uncompressed != 0 && s.ringbuffer.slice().len() == 0 {
    let next_block_header =
      bit_reader::BrotliPeekByte(&mut s.br, s.meta_block_remaining_len as u32, input);
    // -1 if the header is not in the input yet; else ISLAST and ISLASTEMPTY
    // are its two low bits.
    if next_block_header != -1 && (next_block_header & 3) == 3 {
      is_last = true;
    }
  }
  // We need at least 2 bytes of ring buffer size to get the last two
  // bytes for context from there
  let mut min_size = if s.ringbuffer_size != 0 {
    s.ringbuffer_size as i64
  } else if is_last {
    kSmallRingBufferMinSize
  } else if s.initial_ringbuffer_size != 0 {
    // Rounded up to a power of two, and capped at the window, by the loop
    // below.
    core::cmp::max(s.initial_ringbuffer_size as i64, kSmallRingBufferMinSize)
  } else if fixed_pool {
    kSmallRingBufferMinSize
  } else {
    kRingBufferMinSize
  };

  // If maximum is already reached, no further extension is required.
  if s.ringbuffer_size as i64 == window_size {
    return;
  }

  // Metadata blocks do not touch the ring buffer.
  if s.is_metadata != 0 {
    return;
  }

  // Everything decoded so far stays in the ring buffer, which has not wrapped:
  // it wraps only once it is as large as the window.
  let mut output_size = if s.ringbuffer.slice().len() == 0 {
    0
  } else {
    s.pos as i64
  };
  output_size += s.meta_block_remaining_len as i64;
  if min_size < output_size {
    min_size = output_size;
  }
  if !is_last && s.one_shot_output_size != 0 {
    let hinted_size = core::cmp::min(s.one_shot_output_size as i64,
                                     output_size.saturating_mul(kOneShotRingBufferGrowthFactor));
    min_size = core::cmp::max(min_size, hinted_size);
  }

  // Whether this is a fixed pool that can hold the whole window. The sum is
  // taken in u64, which cannot overflow: window_size as u64 is at most 2^63.
  let pool_holds_window = fixed_pool &&
    s.u8_pool_size as u64 >= window_size as u64 + kRingBufferAllocationSlack as u64;
  if s.canny_ringbuffer_allocation && !(pool_holds_window && !is_last) {
    // Reduce ring buffer size to save memory when server is unscrupulous.
    // In worst case memory usage might be 1.5x bigger for a short period of
    // ring buffer reallocation. A min_size of at least 1 keeps the loop
    // finite whatever the state holds.
    let min_size = core::cmp::max(min_size, 1);
    while (new_ringbuffer_size >> 1) >= min_size {
      new_ringbuffer_size >>= 1;
    }
  }

  // Only a window_bits the decoder never accepts gives a size outside i32;
  // 0 then makes BrotliEnsureRingBuffer reject the state instead.
  s.new_ringbuffer_size = if new_ringbuffer_size > 0 &&
                             new_ringbuffer_size <= i32::MAX as i64 {
    new_ringbuffer_size as i32
  } else {
    0
  };
}

// Allocates the ring buffer, or grows it to new_ringbuffer_size keeping the
// bytes decoded so far; a port of the C decoder's BrotliEnsureRingBuffer.
//
// new_ringbuffer_size MUST be updated by BrotliCalculateRingBufferSize before
// this function is called.
//
// Last two bytes of ring buffer are initialized to 0, so context calculation
// could be done uniformly for the first two and all other positions.
//
// Nothing here can panic, and on failure the decoder keeps its current ring
// buffer untouched, so the state stays consistent and droppable. Returns
// `alloc_error` if the allocator cannot supply the memory, and UNREACHABLE
// for a state the decoder could not have produced.
fn BrotliEnsureRingBuffer<AllocU8: alloc::Allocator<u8>,
                          AllocU32: alloc::Allocator<u32>,
                          AllocHC: alloc::Allocator<HuffmanCode>>
  (s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   alloc_error: BrotliDecoderErrorCode)
   -> BrotliDecoderErrorCode {
  let new_size = s.new_ringbuffer_size;
  if s.ringbuffer_size == new_size && s.ringbuffer.slice().len() != 0 {
    return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
  }
  // The ring buffer is only reallocated before it first wraps (it wraps only
  // at the window size, which is final), so the decoded bytes are exactly
  // [0, pos) of the current allocation. The new size must be a power of two,
  // so that its mask works as a modulus, at least 2 for the context bytes, and
  // large enough to hold those bytes. These checks keep every index below in
  // bounds.
  let pos = s.pos;
  if new_size < 2 || (new_size & (new_size - 1)) != 0 || pos < 0 || pos > new_size ||
     pos as usize > s.ringbuffer.slice().len() || s.rb_roundtrips != 0 {
    return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE;
  }
  let size = new_size as usize;
  let decoded = pos as usize;
  let alloc_size = size + kRingBufferAllocationSlack;

  let mut new_ringbuffer = s.alloc_u8.alloc_cell(alloc_size);
  if new_ringbuffer.slice().len() < alloc_size {
    // Normally the empty cell of a failed allocation. Hand it back regardless:
    // a short cell from a misbehaving allocator is still the allocator's.
    s.alloc_u8.free_cell(new_ringbuffer);
    return alloc_error;
  }
  {
    let new_slice = new_ringbuffer.slice_mut();
    new_slice[size - 2] = 0;
    new_slice[size - 1] = 0;
    new_slice[..decoded].clone_from_slice(&s.ringbuffer.slice()[..decoded]);
  }
  let old_ringbuffer = mem::replace(&mut s.ringbuffer, new_ringbuffer);
  s.alloc_u8.free_cell(old_ringbuffer);

  s.ringbuffer_size = new_size;
  s.ringbuffer_mask = new_size - 1;
  BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS
}

// Lazily builds compound_dictionary.block_map, a 256-entry table that maps
// address >> block_bits to the first chunk that might contain that address.
fn EnsureCompoundDictionaryInitialized<AllocU8: alloc::Allocator<u8>>(
  addon: &mut state::BrotliDecoderCompoundDictionary<AllocU8>,
) {
  // 256 = (1 << 8) slots in block map.
  let mut block_bits: u32 = 8;
  let maximal_address = addon.total_size as u32 - 1;
  if addon.block_bits != COMPOUND_DICTIONARY_BLOCK_MAP_UNINITIALIZED {
    return;
  }
  while (maximal_address >> block_bits) != 0 {
    block_bits += 1;
  }
  block_bits -= 8;
  addon.block_bits = block_bits;
  let mut cursor: u32 = 0;
  let mut index: usize = 0;
  while cursor <= maximal_address {
    // chunk_offsets[num_chunks] is a sentinel equal to maximal_address + 1.
    while fast!((addon.chunk_offsets)[index + 1]) < cursor {
      index += 1;
    }
    fast_mut!((addon.block_map)[(cursor >> block_bits) as usize]) = index as u8;
    match cursor.checked_add(1 << block_bits) {
      Some(next) => cursor = next,
      None => break,
    }
  }
  // Now if X is in the range [0..maximal_address] then
  // block_map[X >> block_bits] is in [0..num_chunks).
}

// Prepares a copy of `length` bytes starting at compound dictionary address
// `address` (0 = first byte of the first chunk). Returns false if the copy
// would run off the end of the dictionary.
#[cold]
#[inline(never)]
fn InitializeCompoundDictionaryCopy<AllocU8: alloc::Allocator<u8>>(
  addon: &mut state::BrotliDecoderCompoundDictionary<AllocU8>,
  address: usize,
  length: usize,
) -> bool {
  EnsureCompoundDictionaryInitialized(addon);
  if length > addon.total_size - address {
    return false;
  }
  let mut index = fast!((addon.block_map)[address >> addon.block_bits]) as usize;
  // Several chunks might be mapped to the same block index.
  while address as u32 >= fast!((addon.chunk_offsets)[index + 1]) {
    index += 1;
  }
  addon.br_index = index;
  addon.br_offset = address - fast!((addon.chunk_offsets)[index]) as usize;
  addon.br_length = length;
  addon.br_copied = 0;
  true
}

// Copies the prepared dictionary reference into the ring buffer at `pos`,
// stopping at the end of the ring buffer if necessary (the caller flushes
// and re-invokes). An external chunk's bytes are `external`, whose size
// BrotliDecompressStreamWithDictionary has checked. Returns the number of
// bytes copied.
#[cold]
#[inline(never)]
fn CopyFromCompoundDictionary<AllocU8: alloc::Allocator<u8>>(
  addon: &mut state::BrotliDecoderCompoundDictionary<AllocU8>,
  ringbuffer: &mut [u8],
  ringbuffer_size: i32,
  pos: i32,
  external: &[u8],
) -> i32 {
  let ringbuffer_size = ringbuffer_size as usize;
  let mut pos = pos as usize;
  let orig_pos = pos;
  while addon.br_length != addon.br_copied {
    let chunk = match addon.chunks[addon.br_index] {
      state::MaybeOwnedSlice::External(_) => external,
      ref chunk => chunk.slice(),
    };
    let space = ringbuffer_size - pos;
    let rem_chunk_length = chunk.len() - addon.br_offset;
    let mut length = addon.br_length - addon.br_copied;
    if length > rem_chunk_length {
      length = rem_chunk_length;
    }
    if length > space {
      length = space;
    }
    fast_mut!((ringbuffer)[pos ; pos + length])
      .clone_from_slice(fast!((chunk)[addon.br_offset ; addon.br_offset + length]));
    pos += length;
    addon.br_offset += length;
    addon.br_copied += length;
    if length == rem_chunk_length {
      addon.br_index += 1;
      addon.br_offset = 0;
    }
    if pos == ringbuffer_size {
      break;
    }
  }
  (pos - orig_pos) as i32
}

#[cfg(all(test, feature="std"))]
mod tests {
  use super::*;

  // StandardAlloc for u8 cells, except that requests of at least `fail_at`
  // bytes fail and `short` hands out one byte less than asked. Records every
  // request and counts the cells handed out and taken back.
  struct CountingAlloc {
    fail_at: usize,
    short: bool,
    requests: std::vec::Vec<usize>,
    allocated: usize,
    freed: usize,
  }

  impl Default for CountingAlloc {
    fn default() -> Self {
      CountingAlloc {
        fail_at: usize::MAX,
        short: false,
        requests: std::vec::Vec::new(),
        allocated: 0,
        freed: 0,
      }
    }
  }

  impl Allocator<u8> for CountingAlloc {
    type AllocatedMemory = <::StandardAlloc as Allocator<u8>>::AllocatedMemory;
    fn alloc_cell(&mut self, len: usize) -> Self::AllocatedMemory {
      self.requests.push(len);
      if len >= self.fail_at {
        return Self::AllocatedMemory::default();
      }
      let len = if self.short && len != 0 { len - 1 } else { len };
      if len != 0 {
        self.allocated += 1;
      }
      ::StandardAlloc::default().alloc_cell(len)
    }
    fn free_cell(&mut self, data: Self::AllocatedMemory) {
      if data.slice().len() != 0 {
        self.freed += 1;
      }
    }
  }

  type CountingState = BrotliState<CountingAlloc, ::StandardAlloc, ::StandardAlloc>;

  fn counting_state(window_bits: u32) -> CountingState {
    let mut s = BrotliState::new(CountingAlloc::default(),
                                 ::StandardAlloc::default(),
                                 ::StandardAlloc::default());
    s.window_bits = window_bits;
    s
  }

  // What BrotliDecompressStream does for a metablock of `mlen` bytes: size the
  // ring buffer at the header, then allocate before writing to it.
  fn start_metablock(s: &mut CountingState, mlen: i32) -> BrotliDecoderErrorCode {
    s.meta_block_remaining_len = mlen;
    BrotliCalculateRingBufferSize(s, &[]);
    BrotliEnsureRingBuffer(s, BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_2)
  }

  // Stands in for decoding the metablock: writes its `mlen` bytes after `pos`.
  fn finish_metablock(s: &mut CountingState) {
    let (pos, mlen) = (s.pos as usize, s.meta_block_remaining_len as usize);
    for (i, byte) in s.ringbuffer.slice_mut()[pos..pos + mlen].iter_mut().enumerate() {
      *byte = ((pos + i) % 251) as u8 + 1;
    }
    s.pos += s.meta_block_remaining_len;
    s.meta_block_remaining_len = 0;
  }

  fn assert_holds_decoded_bytes(s: &CountingState) {
    let ringbuffer = s.ringbuffer.slice();
    for i in 0..s.pos as usize {
      assert_eq!(ringbuffer[i], (i % 251) as u8 + 1, "byte {}", i);
    }
  }

  fn assert_success(result: BrotliDecoderErrorCode) {
    assert_eq!(result as i32, BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS as i32);
  }

  fn assert_ring_buffer(s: &CountingState, size: i32) {
    assert_eq!(s.ringbuffer_size, size);
    assert_eq!(s.ringbuffer_mask, size - 1);
    assert_eq!(s.new_ringbuffer_size, size);
    assert_eq!(s.ringbuffer.slice().len(), size as usize + kRingBufferAllocationSlack);
  }

  #[test]
  fn ring_buffer_starts_small_and_grows_on_demand() {
    let mut s = counting_state(22);
    assert_success(start_metablock(&mut s, 10));
    assert_ring_buffer(&s, 1024);
    // The context bytes of the first two positions read the zeroed tail.
    assert_eq!(&s.ringbuffer.slice()[1022..1024], &[0, 0]);
    finish_metablock(&mut s);
    // A metablock that exactly fits the rest of the ring buffer reuses it.
    assert_success(start_metablock(&mut s, 1014));
    assert_ring_buffer(&s, 1024);
    finish_metablock(&mut s);
    assert_eq!(s.alloc_u8.allocated, 1);
    // One byte more than fits doubles it, keeping everything decoded so far.
    assert_success(start_metablock(&mut s, 1));
    assert_ring_buffer(&s, 2048);
    assert_holds_decoded_bytes(&s);
    assert_eq!(&s.ringbuffer.slice()[2046..2048], &[0, 0]);
    finish_metablock(&mut s);
    // It grows straight to the smallest power of two that fits.
    assert_success(start_metablock(&mut s, 5000));
    assert_ring_buffer(&s, 8192);
    assert_holds_decoded_bytes(&s);
    finish_metablock(&mut s);
    // No larger than the window...
    assert_success(start_metablock(&mut s, 3 << 20));
    assert_ring_buffer(&s, 1 << 22);
    assert_holds_decoded_bytes(&s);
    finish_metablock(&mut s);
    // ...which is final: later metablocks wrap it instead.
    assert_success(start_metablock(&mut s, 3 << 20));
    assert_ring_buffer(&s, 1 << 22);
    let slack = kRingBufferAllocationSlack;
    assert_eq!(s.alloc_u8.requests,
               [1024 + slack, 2048 + slack, 8192 + slack, (1 << 22) + slack]);
    // Each growth handed the previous ring buffer back.
    assert_eq!(s.alloc_u8.allocated, 4);
    assert_eq!(s.alloc_u8.freed, 3);
  }

  #[test]
  fn ring_buffer_sizing_matches_the_c_decoder() {
    let first_size = |window_bits: u32, mlen: i32, canny: bool, is_last: bool| {
      let mut s = counting_state(window_bits);
      s.canny_ringbuffer_allocation = canny;
      s.is_last_metablock = is_last as u8;
      assert_success(start_metablock(&mut s, mlen));
      s.ringbuffer_size
    };
    // A short first metablock gets 1 KiB whatever the window, even when it is
    // not the last one (which used to allocate the whole window).
    assert_eq!(first_size(16, 1, true, false), 1024);
    assert_eq!(first_size(24, 1024, true, false), 1024);
    assert_eq!(first_size(30, 1, true, false), 1024);
    assert_eq!(first_size(24, 1025, true, false), 2048);
    assert_eq!(first_size(16, 1 << 24, true, false), 1 << 16);
    // The smallest window is the smallest ring buffer.
    assert_eq!(first_size(10, 1, true, false), 1024);
    // BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION: always the window.
    assert_eq!(first_size(16, 1, false, false), 1 << 16);
    assert_eq!(first_size(24, 1, false, false), 1 << 24);
    assert_eq!(first_size(24, 1, false, true), 1 << 24);
    // The one departure from C: the ring buffer never grows after the last
    // metablock, so if that is the first to need it, it may be under 1 KiB.
    assert_eq!(first_size(22, 1, true, true), 32);
    assert_eq!(first_size(10, 17, true, true), 32);
    assert_eq!(first_size(22, 40, true, true), 64);
    assert_eq!(first_size(22, 513, true, true), 1024);
    assert_eq!(first_size(22, 1025, true, true), 2048);

    // Metadata does not touch the ring buffer.
    let mut s = counting_state(22);
    s.is_metadata = 1;
    s.meta_block_remaining_len = 100;
    BrotliCalculateRingBufferSize(&mut s, &[]);
    assert_eq!(s.new_ringbuffer_size, 0);
    assert!(s.alloc_u8.requests.is_empty());
  }

  // With a fixed u8 pool, the ring buffer never needs more of the pool than
  // before growth on demand, when the whole window came at once for any
  // metablock but the last one, and a last one got as little as 32 bytes.
  #[test]
  fn fixed_pool_ring_buffer_needs_no_more_than_before_growth() {
    let slack = kRingBufferAllocationSlack;
    let window = 1usize << 16;
    // A pool that holds the whole window gets it at once...
    let mut s = counting_state(16);
    s.u8_pool_size = window + slack;
    assert_success(start_metablock(&mut s, 10));
    assert_ring_buffer(&s, window as i32);
    assert_eq!(&s.ringbuffer.slice()[window - 2..window], &[0, 0]);
    finish_metablock(&mut s);
    assert_success(start_metablock(&mut s, 50000));
    assert_ring_buffer(&s, window as i32);
    assert_eq!(s.alloc_u8.requests, [window + slack]);
    // ...unless the metablock is the last one.
    let mut s = counting_state(16);
    s.u8_pool_size = window + slack;
    s.is_last_metablock = 1;
    assert_success(start_metablock(&mut s, 40));
    assert_ring_buffer(&s, 64);
    // A smaller pool could not hold the whole window, so the ring buffer
    // grows, starting from 32 bytes rather than 1 KiB.
    let mut s = counting_state(16);
    s.u8_pool_size = window + slack - 1;
    assert_success(start_metablock(&mut s, 1));
    assert_ring_buffer(&s, 32);
    finish_metablock(&mut s);
    assert_success(start_metablock(&mut s, 100));
    assert_ring_buffer(&s, 128);
    assert_holds_decoded_bytes(&s);
    finish_metablock(&mut s);
    assert_success(start_metablock(&mut s, 40000));
    assert_ring_buffer(&s, window as i32);
    assert_holds_decoded_bytes(&s);
    assert_eq!(s.alloc_u8.requests, [32 + slack, 128 + slack, window + slack]);
    assert_eq!((s.alloc_u8.allocated, s.alloc_u8.freed), (3, 2));
    // BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION still wins.
    let mut s = counting_state(16);
    s.u8_pool_size = 1;
    s.canny_ringbuffer_allocation = false;
    s.is_last_metablock = 1;
    assert_success(start_metablock(&mut s, 1));
    assert_ring_buffer(&s, window as i32);
  }

  #[test]
  fn failed_ring_buffer_growth_keeps_the_current_one() {
    let mut s = counting_state(24);
    s.alloc_u8.fail_at = 1 << 20;
    assert_success(start_metablock(&mut s, 700));
    finish_metablock(&mut s);
    // Needs a 4 MiB ring buffer, which the allocator refuses.
    assert_eq!(start_metablock(&mut s, 3 << 20) as i32,
               BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_2 as i32);
    assert_eq!(s.ringbuffer_size, 1024);
    assert_eq!(s.ringbuffer_mask, 1023);
    assert_eq!(s.ringbuffer.slice().len(), 1024 + kRingBufferAllocationSlack);
    assert_eq!(s.pos, 700);
    assert_holds_decoded_bytes(&s);
    assert_eq!((s.alloc_u8.allocated, s.alloc_u8.freed), (1, 0));
    // Nothing was lost, so once memory is available the growth goes through.
    s.alloc_u8.fail_at = usize::MAX;
    assert_success(BrotliEnsureRingBuffer(
      &mut s, BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_2));
    assert_ring_buffer(&s, 4 << 20);
    assert_holds_decoded_bytes(&s);
    assert_eq!((s.alloc_u8.allocated, s.alloc_u8.freed), (2, 1));
  }

  #[test]
  fn short_ring_buffer_allocation_is_returned_and_reported() {
    let mut s = counting_state(22);
    s.alloc_u8.short = true;
    assert_eq!(start_metablock(&mut s, 10) as i32,
               BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_2 as i32);
    assert_eq!(s.ringbuffer.slice().len(), 0);
    assert_eq!(s.ringbuffer_size, 0);
    assert_eq!((s.alloc_u8.allocated, s.alloc_u8.freed), (1, 1));
  }

  #[test]
  fn ensure_ring_buffer_rejects_states_the_decoder_cannot_produce() {
    let unreachable = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE as i32;
    let ensure = |s: &mut CountingState| {
      BrotliEnsureRingBuffer(s, BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_1)
        as i32
    };
    let mut s = counting_state(16);
    // No size was calculated.
    assert_eq!(ensure(&mut s), unreachable);
    // Not a power of two.
    s.new_ringbuffer_size = 1000;
    assert_eq!(ensure(&mut s), unreachable);
    // Decoded bytes that would not fit, or that were never allocated.
    s.new_ringbuffer_size = 1024;
    s.pos = 1025;
    assert_eq!(ensure(&mut s), unreachable);
    s.pos = 1;
    assert_eq!(ensure(&mut s), unreachable);
    s.pos = -1;
    assert_eq!(ensure(&mut s), unreachable);
    // A ring buffer that has wrapped can no longer be moved.
    s.pos = 0;
    s.rb_roundtrips = 1;
    assert_eq!(ensure(&mut s), unreachable);
    assert!(s.alloc_u8.requests.is_empty());
    s.rb_roundtrips = 0;
    assert_eq!(ensure(&mut s), BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS as i32);
  }

  // Packs fields least significant bit first, as brotli streams are laid out.
  struct BitWriter {
    bytes: std::vec::Vec<u8>,
    bits: usize,
  }

  impl BitWriter {
    fn put(&mut self, count: usize, value: u32) {
      for i in 0..count {
        if self.bits % 8 == 0 {
          self.bytes.push(0);
        }
        let last = self.bytes.len() - 1;
        self.bytes[last] |= (((value >> i) & 1) as u8) << (self.bits % 8);
        self.bits += 1;
      }
    }

    // Uncompressed data starts at the next byte boundary.
    fn raw(&mut self, data: &[u8]) {
      self.bytes.extend_from_slice(data);
      self.bits = self.bytes.len() * 8;
    }
  }

  // A stream with a 64 KiB window of uncompressed metablocks of the given
  // lengths, then an empty last one; and the pseudo-random data it holds.
  fn uncompressed_stream(lengths: &[usize]) -> (std::vec::Vec<u8>, std::vec::Vec<u8>) {
    let mut writer = BitWriter { bytes: std::vec::Vec::new(), bits: 0 };
    writer.put(1, 0); // WBITS=16
    let mut data = std::vec::Vec::new();
    let mut seed = 1u32;
    for &length in lengths {
      let block: std::vec::Vec<u8> = (0..length).map(|_| {
        seed = seed.wrapping_mul(1103515245).wrapping_add(12345);
        (seed >> 16) as u8
      }).collect();
      // ISLAST=0, MNIBBLES=4, MLEN-1, ISUNCOMPRESSED=1.
      writer.put(1, 0);
      writer.put(2, 0);
      writer.put(16, length as u32 - 1);
      writer.put(1, 1);
      writer.raw(&block);
      data.extend_from_slice(&block);
    }
    writer.put(2, 3); // ISLAST=1, ISLASTEMPTY=1
    (writer.bytes, data)
  }

  // Hands the decoder at most in_chunk bytes of input and out_chunk bytes of
  // output space per call.
  fn decode_in_chunks(s: &mut CountingState, input: &[u8], in_chunk: usize, out_chunk: usize)
                      -> std::vec::Vec<u8> {
    let mut output = std::vec::Vec::new();
    let mut buffer = vec![0u8; core::cmp::min(out_chunk, 1 << 20)];
    let mut input_offset = 0;
    loop {
      let mut available_in = core::cmp::min(in_chunk, input.len() - input_offset);
      let mut available_out = buffer.len();
      let mut output_offset = 0;
      let mut total_out = 0;
      let result = BrotliDecompressStream(&mut available_in, &mut input_offset, input,
                                          &mut available_out, &mut output_offset, &mut buffer,
                                          &mut total_out, s);
      output.extend_from_slice(&buffer[..output_offset]);
      match result {
        BrotliResult::ResultSuccess => return output,
        BrotliResult::NeedsMoreInput => assert!(input_offset < input.len(), "truncated"),
        BrotliResult::NeedsMoreOutput => {}
        BrotliResult::ResultFailure => panic!("decoding failed: {:?}", s.error_code),
      }
    }
  }

  #[test]
  fn ring_buffer_grows_across_metablocks_then_wraps() {
    // 10 + 1000 bytes fit in 1 KiB; 2510 need 4 KiB and 5510 need 8 KiB;
    // 35510 need the whole 64 KiB window, which the rest then wraps around.
    let (input, expected) = uncompressed_stream(&[10, 1000, 1500, 3000, 30000, 40000, 5000]);
    let slack = kRingBufferAllocationSlack;
    for &(in_chunk, out_chunk) in &[(usize::MAX, usize::MAX), (1, 1), (1, 4099), (4099, 1),
                                     (333, 777)] {
      let mut s = counting_state(0);
      let output = decode_in_chunks(&mut s, &input, in_chunk, out_chunk);
      assert!(output == expected, "chunks of {} and {}", in_chunk, out_chunk);
      assert_eq!(s.alloc_u8.requests, [1024 + slack, 4096 + slack, 8192 + slack, 65536 + slack]);
      assert_eq!((s.alloc_u8.allocated, s.alloc_u8.freed), (4, 3));
    }
  }

  #[test]
  fn set_parameter_disables_growth_only_before_decoding() {
    use state::BrotliDecoderParameter::{BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION,
                                        BROTLI_DECODER_PARAM_LARGE_WINDOW};
    let (input, expected) = uncompressed_stream(&[10, 1000, 1500, 3000, 30000, 40000, 5000]);
    let mut s = counting_state(0);
    assert!(s.set_parameter(BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION, 1));
    assert!(decode_in_chunks(&mut s, &input, usize::MAX, usize::MAX) == expected);
    // The whole window at once, where the stream above grows to it in four.
    assert_eq!(s.alloc_u8.requests, [65536 + kRingBufferAllocationSlack]);
    // Once decoding has started, nothing changes.
    let large_window = s.large_window;
    for &value in &[0, 1] {
      assert!(!s.set_parameter(BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION, value));
      assert!(!s.set_parameter(BROTLI_DECODER_PARAM_LARGE_WINDOW, value));
    }
    assert!(!s.canny_ringbuffer_allocation);
    assert_eq!(s.large_window, large_window);
  }

  #[test]
  fn initial_ring_buffer_size_sets_the_first_ring_buffer() {
    let first_size = |initial: u32, mlen: i32, is_last: bool| {
      let mut s = counting_state(22);
      assert!(s.set_initial_ring_buffer_size(initial));
      s.is_last_metablock = is_last as u8;
      assert_success(start_metablock(&mut s, mlen));
      s.ringbuffer_size
    };
    assert_eq!(first_size(0, 10, false), 1024);
    // Rounded up to a power of two of at least 32 bytes...
    assert_eq!(first_size(4096, 10, false), 4096);
    assert_eq!(first_size(5000, 10, false), 8192);
    assert_eq!(first_size(1, 10, false), 32);
    // ...that still holds the metablock...
    assert_eq!(first_size(4096, 5000, false), 8192);
    // ...and is no larger than the window.
    assert_eq!(first_size(u32::MAX, 10, false), 1 << 22);
    // A last metablock is sized to fit, as the ring buffer never grows after it.
    assert_eq!(first_size(u32::MAX, 10, true), 32);
    // Disabling reallocation still allocates the whole window.
    let mut s = counting_state(22);
    assert!(s.set_initial_ring_buffer_size(4096));
    s.canny_ringbuffer_allocation = false;
    assert_success(start_metablock(&mut s, 10));
    assert_eq!(s.ringbuffer_size, 1 << 22);
  }

  #[test]
  fn one_shot_output_hint_grows_in_bounded_steps() {
    let mut tiny = counting_state(22);
    tiny.one_shot_output_size = 1 << 20;
    assert_success(start_metablock(&mut tiny, 16));
    assert_ring_buffer(&tiny, 1024);

    let mut small = counting_state(22);
    small.one_shot_output_size = 1 << 20;
    assert_success(start_metablock(&mut small, 300));
    assert_ring_buffer(&small, 2048);

    let mut s = counting_state(22);
    s.one_shot_output_size = 1 << 20;
    let slack = kRingBufferAllocationSlack;

    assert_success(start_metablock(&mut s, 10));
    assert_ring_buffer(&s, 1024);
    finish_metablock(&mut s);

    // At 64 KiB of decoded output, jump fourfold instead of taking every
    // intermediate power of two on the way to the caller's output capacity.
    assert_success(start_metablock(&mut s, (64 << 10) - 10));
    assert_ring_buffer(&s, 256 << 10);
    assert_holds_decoded_bytes(&s);
    finish_metablock(&mut s);

    assert_success(start_metablock(&mut s, 192 << 10));
    assert_ring_buffer(&s, 1 << 20);
    assert_holds_decoded_bytes(&s);
    assert_eq!(s.alloc_u8.requests, [1024 + slack, (256 << 10) + slack,
                                     (1 << 20) + slack]);

    // Even with a much larger output buffer and a large window, a short
    // stream cannot force the full output capacity into the first ring.
    let mut oversized = counting_state(30);
    oversized.one_shot_output_size = 1 << 30;
    assert_success(start_metablock(&mut oversized, 10));
    assert_ring_buffer(&oversized, 1024);
    finish_metablock(&mut oversized);
    assert_success(start_metablock(&mut oversized, (64 << 10) - 10));
    assert_ring_buffer(&oversized, 256 << 10);
  }

  #[test]
  fn initial_ring_buffer_size_is_only_set_before_decoding() {
    let (input, expected) = uncompressed_stream(&[10, 1000, 1500, 3000, 30000, 40000, 5000]);
    let mut s = counting_state(0);
    assert!(s.set_initial_ring_buffer_size(4096));
    assert!(s.set_initial_ring_buffer_size(u32::MAX));
    // One byte starts decoding the stream header, before any ring buffer...
    let (mut available_in, mut input_offset) = (1, 0);
    let (mut available_out, mut output_offset, mut total_out) = (0, 0, 0);
    match BrotliDecompressStream(&mut available_in, &mut input_offset, &input,
                                 &mut available_out, &mut output_offset, &mut [],
                                 &mut total_out, &mut s) {
      BrotliResult::NeedsMoreInput => {}
      _ => panic!("expected NeedsMoreInput"),
    }
    assert!(s.alloc_u8.requests.is_empty());
    // ...after which the size no longer changes.
    assert!(!s.set_initial_ring_buffer_size(0));
    assert!(decode_in_chunks(&mut s, &input[1..], 333, 777) == expected);
    // The whole window at once, where by default this stream grows to it in
    // four allocations (see ring_buffer_grows_across_metablocks_then_wraps).
    assert_eq!(s.alloc_u8.requests, [65536 + kRingBufferAllocationSlack]);
  }

  // Encoders store small or incompressible input as an uncompressed metablock
  // and an empty last one. When the decoder can see the empty one, it sizes the
  // ring buffer for the uncompressed one as for a last metablock.
  #[test]
  fn uncompressed_metablock_before_an_empty_last_one_is_sized_to_fit() {
    let (input, expected) = uncompressed_stream(&[10]);
    let requests = |initial: u32, canny: bool, in_chunk: usize| {
      let mut s = counting_state(0);
      assert!(s.set_initial_ring_buffer_size(initial));
      s.canny_ringbuffer_allocation = canny;
      assert!(decode_in_chunks(&mut s, &input, in_chunk, usize::MAX) == expected);
      s.alloc_u8.requests.clone()
    };
    let slack = kRingBufferAllocationSlack;
    // With the empty last metablock in the input, whatever the initial size...
    assert_eq!(requests(0, true, usize::MAX), [32 + slack]);
    assert_eq!(requests(u32::MAX, true, usize::MAX), [32 + slack]);
    // ...though disabling reallocation still takes the whole window.
    assert_eq!(requests(0, false, usize::MAX), [65536 + slack]);
    // One byte at a time, the uncompressed metablock is sized as not last.
    assert_eq!(requests(0, true, 1), [1024 + slack]);
    assert_eq!(requests(u32::MAX, true, 1), [65536 + slack]);
  }

  #[test]
  fn take_output_returns_every_consumed_byte() {
    // The existing 10x10y stream, embedded so this regression needs no fixture.
    let input = [0x1b, 0x13, 0x00, 0x00, 0xa4, 0xb0, 0xb2, 0xea, 0x81, 0x47, 0x02, 0x8a];
    let expected = b"XXXXXXXXXXYYYYYYYYYY";
    let mut state = BrotliState::new(::StandardAlloc::default(),
                                     ::StandardAlloc::default(),
                                     ::StandardAlloc::default());
    let mut available_in = input.len();
    let mut input_offset = 0usize;
    let mut available_out = 0usize;
    let mut output_offset = 0usize;
    let mut total_out = 0usize;
    let result = BrotliDecompressStream(&mut available_in, &mut input_offset, &input,
                                        &mut available_out, &mut output_offset, &mut [],
                                        &mut total_out, &mut state);
    assert_eq!(result as i32, BrotliResult::NeedsMoreOutput as i32);
    assert_eq!(total_out, 0);

    let mut consumed = 0usize;
    for &limit in &[1usize, 3, 7, 0] {
      assert!(BrotliDecoderHasMoreOutput(&state));
      let mut size = limit;
      {
        let output = BrotliDecoderTakeOutput(&mut state, &mut size);
        assert_eq!(size, if limit == 0 { expected.len() - consumed } else { limit });
        assert_eq!(output.len(), size);
        assert_eq!(output, &expected[consumed..consumed + size]);
      }
      consumed += size;
      assert_eq!(state.partial_pos_out, consumed as u64);
    }
    assert_eq!(consumed, expected.len());
    assert!(!BrotliDecoderHasMoreOutput(&state));
    for &limit in &[1usize, 0] {
      let mut size = limit;
      assert!(BrotliDecoderTakeOutput(&mut state, &mut size).is_empty());
      assert_eq!(size, 0);
    }
  }

  fn assert_large_remaining_copy_is_clamped(meta_block_remaining_len: i32,
                                            pos: i32,
                                            expected_result: BrotliDecoderErrorCode) {
    let mut s = BrotliState::new(::StandardAlloc::default(),
                                 ::StandardAlloc::default(),
                                 ::StandardAlloc::default());
    s.ringbuffer_size = 16;
    s.new_ringbuffer_size = 16;
    s.window_bits = 5;
    s.ringbuffer = s.alloc_u8.alloc_cell(s.ringbuffer_size as usize);
    s.meta_block_remaining_len = meta_block_remaining_len;
    s.pos = pos;
    s.br.avail_in = 1u32 << 31;

    let input = [0x5au8];
    let mut available_out = 0usize;
    let mut output = [0u8; 0];
    let mut output_offset = 0usize;
    let mut total_out = 0usize;
    let result = CopyUncompressedBlockToOutput(&mut available_out,
                                               &mut output,
                                               &mut output_offset,
                                               &mut total_out,
                                               &mut s,
                                               &input);

    assert_eq!(result as i32, expected_result as i32);
    assert_eq!(s.br.avail_in, (1u32 << 31) - 1);
    assert_eq!(s.meta_block_remaining_len, meta_block_remaining_len - 1);
    assert_eq!(s.pos, pos + 1);
    assert_eq!(s.ringbuffer.slice()[pos as usize], input[0]);
  }

  #[test]
  fn uncompressed_copy_clamps_unsigned_remaining_bytes() {
    assert_large_remaining_copy_is_clamped(
      1, 0, BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS);
    assert_large_remaining_copy_is_clamped(
      16, 15, BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT);
  }

  #[test]
  fn stream_rejects_wrapping_output_range() {
    let mut state = BrotliState::new(::StandardAlloc::default(),
                                     ::StandardAlloc::default(),
                                     ::StandardAlloc::default());
    let input = [0x06u8];
    let mut output = [0u8; 1];
    let mut available_in = input.len();
    let mut input_offset = 0usize;
    let mut available_out = usize::MAX;
    let mut output_offset = 1usize;
    let mut total_out = 0usize;

    let result = BrotliDecompressStream(&mut available_in,
                                        &mut input_offset,
                                        &input,
                                        &mut available_out,
                                        &mut output_offset,
                                        &mut output,
                                        &mut total_out,
                                        &mut state);

    assert_eq!(result as i32, BrotliResult::ResultFailure as i32);
    assert_eq!(state.error_code as i32,
               BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS as i32);
  }

  #[test]
  fn stream_rejects_wrapping_input_range() {
    let mut state = BrotliState::new(::StandardAlloc::default(),
                                     ::StandardAlloc::default(),
                                     ::StandardAlloc::default());
    let input = [0x06u8];
    let mut output = [0u8; 1];
    // The end wraps to zero on 32-bit and is out of bounds on 64-bit.
    let mut available_in = u32::MAX as usize;
    let mut input_offset = 1usize;
    let mut available_out = output.len();
    let mut output_offset = 0usize;
    let mut total_out = 0usize;

    let result = BrotliDecompressStream(&mut available_in,
                                        &mut input_offset,
                                        &input,
                                        &mut available_out,
                                        &mut output_offset,
                                        &mut output,
                                        &mut total_out,
                                        &mut state);

    assert_eq!(result as i32, BrotliResult::ResultFailure as i32);
    assert_eq!(state.error_code as i32,
               BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS as i32);
  }

  #[test]
  fn format_distance_error_restores_huffman_tree_groups() {
    let mut state = BrotliState::new(::StandardAlloc::default(),
                                     ::StandardAlloc::default(),
                                     ::StandardAlloc::default());
    // The alphabets the decoder uses, so the groups have the room the
    // command loop's table windows expect.
    state.BrotliHuffmanTreeGroupInit(state::WhichTreeGroup::LITERAL,
                                     kNumLiteralCodes, kNumLiteralCodes, 1);
    state.BrotliHuffmanTreeGroupInit(state::WhichTreeGroup::INSERT_COPY,
                                     kNumInsertAndCopyCodes, kNumInsertAndCopyCodes, 2);
    state.BrotliHuffmanTreeGroupInit(state::WhichTreeGroup::DISTANCE, 64, 64, 3);
    let expected_code_lengths = [
      state.literal_hgroup.codes.slice().len(),
      state.insert_copy_hgroup.codes.slice().len(),
      state.distance_hgroup.codes.slice().len(),
    ];

    state.state = BrotliRunningState::BROTLI_STATE_COMMAND_POST_DECODE_LITERALS;
    state.distance_code = 1;
    state.dist_rb_idx = 1;
    state.dist_rb[0] = i32::MAX;
    state.max_distance = 0;
    state.max_backward_distance = 0;

    let result = SafeProcessCommands(&mut state, &[], &[]);

    assert_eq!(result as i32,
               BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_DISTANCE as i32);
    assert_eq!(state.literal_hgroup.codes.slice().len(), expected_code_lengths[0]);
    assert_eq!(state.insert_copy_hgroup.codes.slice().len(), expected_code_lengths[1]);
    assert_eq!(state.distance_hgroup.codes.slice().len(), expected_code_lengths[2]);
  }
}

// The table for decoding context maps, needed only by metablocks with more
// than one tree in a group.
fn EnsureContextMapTable<AllocU8: alloc::Allocator<u8>,
                         AllocU32: alloc::Allocator<u32>,
                         AllocHC: alloc::Allocator<HuffmanCode>>
  (s: &mut BrotliState<AllocU8, AllocU32, AllocHC>)
   -> bool {
  if s.context_map_table.slice().len() == 0 {
    s.context_map_table = s.alloc_hc.alloc_cell(huffman::BROTLI_HUFFMAN_MAX_SIZE_272 as usize);
  }
  s.context_map_table.slice().len() != 0
}

// Allocates the context-map table, block-type trees, and block-length trees
// unless they are already allocated. Returns false if an allocation fails.
// Callers invoke this before freeing/growing any reusable metablock buffer so
// fixed pool allocators never see an allocation after a free.
fn EnsureBlockTrees<AllocU8: alloc::Allocator<u8>,
                    AllocU32: alloc::Allocator<u32>,
                    AllocHC: alloc::Allocator<HuffmanCode>>
  (s: &mut BrotliState<AllocU8, AllocU32, AllocHC>)
   -> bool {
  if !EnsureContextMapTable(s) {
    return false;
  }
  if s.block_type_length_state.block_type_trees.slice().len() == 0 {
    s.block_type_length_state.block_type_trees =
      s.alloc_hc.alloc_cell(3 * huffman::BROTLI_HUFFMAN_MAX_SIZE_258 as usize);
    if s.block_type_length_state.block_type_trees.slice().len() == 0 {
      return false;
    }
  }
  if s.block_type_length_state.block_len_trees.slice().len() == 0 {
    s.block_type_length_state.block_len_trees =
      s.alloc_hc.alloc_cell(3 * huffman::BROTLI_HUFFMAN_MAX_SIZE_26 as usize);
    if s.block_type_length_state.block_len_trees.slice().len() == 0 {
      return false;
    }
  }
  true
}

// Reads 1..256 2-bit context modes.
pub fn ReadContextModes<AllocU8: alloc::Allocator<u8>,
                        AllocU32: alloc::Allocator<u32>,
                        AllocHC: alloc::Allocator<HuffmanCode>>
  (s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   input: &[u8])
   -> BrotliDecoderErrorCode {

  let mut i: i32 = s.loop_counter;

  for context_mode_iter in fast_mut!((s.context_modes.slice_mut())[i as usize ;
                                                       (s.block_type_length_state.num_block_types[0]
                                                        as usize)])
    .iter_mut() {
    let mut bits: u32 = 0;
    if (!bit_reader::BrotliSafeReadBits(&mut s.br, 2, &mut bits, input)) {
      mark_unlikely();
      s.loop_counter = i;
      return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
    }
    *context_mode_iter = bits as u8;
    BROTLI_LOG_UINT!(i);
    i += 1;
  }
  BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS
}

#[inline(always)]
fn TakeDistanceFromRingBuffer(
  distance_code: &mut i32,
  dist_rb_idx: &mut i32,
  dist_rb: &[i32; 4],
  distance_context: &mut i32,
) {
  if *distance_code == 0 {
    *dist_rb_idx -= 1;
    *distance_code = fast!((dist_rb)[(*dist_rb_idx & 3) as usize]);
    *distance_context = 1;
  } else {
    let dcode = *distance_code << 1;
    // kDistanceShortCodeIndexOffset has 2-bit values from LSB:
    // 3, 2, 1, 0, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2
    const kDistanceShortCodeIndexOffset: u32 = 0xaaafff1b;
    // kDistanceShortCodeValueOffset has 2-bit values from LSB:
    // -0, 0,-0, 0,-1, 1,-2, 2,-3, 3,-1, 1,-2, 2,-3, 3
    const kDistanceShortCodeValueOffset: u32 = 0xfa5fa500;
    let mut v = (*dist_rb_idx + (kDistanceShortCodeIndexOffset as i32 >> dcode)) & 0x3;
    *distance_code = fast!((dist_rb)[v as usize]);
    v = (kDistanceShortCodeValueOffset >> dcode) as i32 & 0x3;
    if (dcode & 0x3) != 0 {
      *distance_code += v;
    } else {
      *distance_code -= v;
      if *distance_code <= 0 {
        // A huge distance will cause a BROTLI_FAILURE() soon.
        // This is a little faster than failing here.
        *distance_code = 0x7fffffff;
      }
    }
  }
}

pub fn SafeReadBits(br: &mut bit_reader::BrotliBitReader,
                    n_bits: u32,
                    val: &mut u32,
                    input: &[u8])
                    -> bool {
  if (n_bits != 0) {
    bit_reader::BrotliSafeReadBits(br, n_bits, val, input)
  } else {
    *val = 0;
    true
  }
}

// Selects a command loop flavour at compile time. Associated consts rather than
// const generics: const generics need Rust 1.51.
trait CommandLoopMode {
  const SAFE: bool;
}
struct FastCommandLoop;
struct SafeCommandLoop;
impl CommandLoopMode for FastCommandLoop { const SAFE: bool = false; }
impl CommandLoopMode for SafeCommandLoop { const SAFE: bool = true; }

// Cold second-level distance Huffman lookup (`table_element.bits > 8`).
// Outlined so the common 8-bit root lookup in `read_distance` needs only the
// thin `&DistanceRootTable` pointer and never loads `distance_hgroup` or
// `dist_context_map` from `BrotliState` unless `root.bits > 8`.
#[cold]
#[inline(never)]
fn decode_distance_subtable<
  AllocU32: alloc::Allocator<u32>,
  AllocHC: alloc::Allocator<HuffmanCode>,
>(
  bits: u32,
  table_index: usize,
  root: HuffmanCode,
  distance_hgroup: &HuffmanTreeGroup<AllocU32, AllocHC>,
  dist_context_map: &[u8],
  dist_context_map_slice_index: usize,
  distance_context: i32,
) -> (usize, u32) {
  let tree_idx = dist_context_map
    .get(dist_context_map_slice_index + (distance_context as usize & 3))
    .copied()
    .unwrap_or(0) as usize;
  let distance_tree = huffman_tree(distance_hgroup, tree_idx);
  let sub_idx = table_index
    + root.value as usize
    + (((bits & bit_reader::BitMask(root.bits as u32)) >> HUFFMAN_TABLE_BITS) as usize);
  let sub = distance_tree.entry(sub_idx);
  (sub.value as usize, HUFFMAN_TABLE_BITS + (sub.bits as u32 & 7))
}

// Converts the decoded distance symbol `code` to `*distance_code`, reading its
// extra bits. Precondition: *distance_code < 0. `memento` is the reader state
// from before the symbol was read; only the safe path uses it, to roll back
// when the extra bits are not all available yet (the fast path passes a
// default state).
#[inline(always)]
fn read_distance<M: CommandLoopMode>(
  code: usize,
  memento: &bit_reader::BrotliBitReaderState,
  fbr: &mut bit_reader::BitReaderSlice,
  br: &mut bit_reader::BrotliBitReader,
  input: &[u8],
  distance_code: &mut i32,
  distance_context: &mut i32,
  dist_rb_idx: &mut i32,
  dist_rb: &[i32; 4],
  block_len_2: &mut u32,
  distance_postfix_bits: u32,
  distance_postfix_mask: i32,
  num_direct_distance_codes: u32,
) -> bool {
  if (code & !0xfusize) == 0 {
    *distance_code = code as i32;
    *distance_context = 0;
    TakeDistanceFromRingBuffer(distance_code, dist_rb_idx, dist_rb, distance_context);
    *block_len_2 -= 1;
    return true;
  }
  if !M::SAFE {
    if distance_postfix_bits == 0 {
      // NPOSTFIX == 0 (every metablock below q10, and many at q10-q11):
      // `checked_sub` handles direct codes (`16 <= code < ndirect`) and codes
      // with extra bits (`code >= ndirect`) with one comparison, in 32-bit
      // arithmetic (identical in the low 32 bits for the at most 30-bit extra
      // distances the alphabet limit allows).
      let ndirect = num_direct_distance_codes;
      let code = code as u32;
      *distance_code = match code.checked_sub(ndirect) {
        Some(distval) => {
          let nbits = (distval >> 1) + 1;
          let bits = fbr.read_bits(nbits);
          ((2 + (distval & 1)).wrapping_shl(nbits))
            .wrapping_add(ndirect)
            .wrapping_sub(4 + NUM_DISTANCE_SHORT_CODES as u32 - 1)
            .wrapping_add(bits) as i32
        }
        None => code.wrapping_sub(NUM_DISTANCE_SHORT_CODES as u32 - 1) as i32,
      };
    } else {
      // NPOSTFIX > 0 (primarily q10-q11, where NDIRECT is also often non-zero):
      // branchless decode avoids ~1.5M branch mispredictions on mixed direct /
      // extra-bit distance streams such as `wiki-Brotli.html` q11. When
      // `code < num_direct`, `distval < 0`, `mask == 0`, and `nbits == 0`, so
      // `read_bits(0)` consumes 0 bits and returns 0 while the masked formula
      // reduces to `code - 15`.
      let num_direct = num_direct_distance_codes as i32;
      let distval = code as i32 - num_direct;
      let mask = !(distval >> 31) as u32;
      let d = (distval as u32) & mask;
      let postfix_bits = distance_postfix_bits & 3;
      let postfix = d & (distance_postfix_mask as u32);
      let hcode = d >> postfix_bits;
      let nbits = ((hcode >> 1) + 1) & mask;
      let bits = fbr.read_bits(nbits);
      let offset = (((2u32 + (hcode & 1)).wrapping_shl(nbits)).wrapping_sub(4)) & mask;
      *distance_code = ((offset.wrapping_add(bits).wrapping_shl(postfix_bits))
        .wrapping_add(postfix) & mask)
        .wrapping_add((code as u32) & !mask)
        .wrapping_add((num_direct as u32) & mask)
        .wrapping_sub(NUM_DISTANCE_SHORT_CODES as u32 - 1) as i32;
    }
    *distance_context = 0;
    *block_len_2 -= 1;
    return true;
  }
  let num_direct = num_direct_distance_codes as usize;
  if code < num_direct {
    *distance_code = (code - (NUM_DISTANCE_SHORT_CODES as usize - 1)) as i32;
    *distance_context = 0;
    *block_len_2 -= 1;
    return true;
  }
  let distval = (code - num_direct) as u32;
  let postfix_bits = distance_postfix_bits & 3;
  let postfix = distval & (distance_postfix_mask as u32);
  let hcode = distval >> postfix_bits;
  let nbits = (hcode >> 1) + 1;
  if nbits > 29 {
    *distance_code = 0x7fffffff;
    *distance_context = 0;
    *block_len_2 -= 1;
    return true;
  }
  let mut bits: u32 = 0;
  if !SafeReadBits(br, nbits, &mut bits, input) {
    bit_reader::BrotliBitReaderRestoreState(br, memento);
    return false;
  }
  #[cfg(target_pointer_width = "64")]
  {
    let offset = ((2u64 + (hcode & 1) as u64) << (nbits & 63)).wrapping_sub(4);
    *distance_code = ((offset.wrapping_add(bits as u64) << postfix_bits)
      .wrapping_add(postfix as u64)
      .wrapping_add(num_direct as u64) as i32)
      .wrapping_sub(NUM_DISTANCE_SHORT_CODES as i32)
      .wrapping_add(1);
  }
  // 64-bit shifts are multi-instruction sequences on 32-bit targets; the low
  // 32 bits, all that the `i32` result keeps, are the same in u32.
  #[cfg(not(target_pointer_width = "64"))]
  {
    let offset = (2u32 + (hcode & 1)).wrapping_shl(nbits).wrapping_sub(4);
    *distance_code = (offset.wrapping_add(bits).wrapping_shl(postfix_bits)
      .wrapping_add(postfix)
      .wrapping_add(num_direct as u32) as i32)
      .wrapping_sub(NUM_DISTANCE_SHORT_CODES as i32)
      .wrapping_add(1);
  }
  *distance_context = 0;
  *block_len_2 -= 1;
  true
}

#[inline(always)]
fn read_command<M: CommandLoopMode>(
  fbr: &mut bit_reader::BitReaderSlice,
  br: &mut bit_reader::BrotliBitReader,
  input: &[u8],
  distance_code: &mut i32,
  distance_context: &mut i32,
  copy_len_out: &mut i32,
  block_len_1: &mut u32,
  insert_length: &mut i32,
  insert_copy_tree: &InsertCopyTable,
) -> bool {
  let memento = if M::SAFE {
    bit_reader::BrotliBitReaderSaveState(br)
  } else {
    bit_reader::BrotliBitReaderState::default()
  };
  let cmd_code = if !M::SAFE {
    *block_len_1 -= 1;
    read_symbol_slice(fbr, insert_copy_tree) as usize
  } else {
    let mut sym: u32 = 0;
    if !SafeReadSymbol(insert_copy_tree, br, &mut sym, input) {
      return false;
    }
    sym as usize
  };
  // `kCmdLut` is padded to 1024 entries, so masking (a single bit-field
  // clear) replaces a compare and branch; valid insert-and-copy codes are
  // below 704.
  let v = &prefix::kCmdLut[cmd_code & 1023];
  *distance_code = v.distance_code as i32;
  *distance_context = v.context as i32;
  if !M::SAFE {
    let mut insert_len = v.insert_len_offset as i32;
    if v.insert_len_extra_bits != 0 {
      mark_unlikely();
      insert_len += fbr.read_bits(v.insert_len_extra_bits as u32) as i32;
    }
    let copy_offset = v.copy_len_offset as i32;
    let copy_extra = fbr.read_bits(v.copy_len_extra_bits as u32) as i32;
    *insert_length = insert_len;
    *copy_len_out = copy_offset + copy_extra;
  } else {
    let mut insert_len = v.insert_len_offset as i32;
    let insert_extra_bits = v.insert_len_extra_bits as u32;
    if insert_extra_bits != 0 {
      let mut bits = 0u32;
      if !bit_reader::BrotliSafeReadBits(br, insert_extra_bits, &mut bits, input) {
        bit_reader::BrotliBitReaderRestoreState(br, &memento);
        return false;
      }
      insert_len += bits as i32;
    }
    let copy_extra_bits = v.copy_len_extra_bits as u32;
    let mut copy_bits = 0u32;
    if !SafeReadBits(br, copy_extra_bits, &mut copy_bits, input) {
      bit_reader::BrotliBitReaderRestoreState(br, &memento);
      return false;
    }
    *insert_length = insert_len;
    *copy_len_out = v.copy_len_offset as i32 + copy_bits as i32;
    *block_len_1 -= 1;
  }
  true
}


#[inline(always)]
fn WarmupBitReader(safe: bool, br: &mut bit_reader::BrotliBitReader, input: &[u8]) -> bool {
  safe || bit_reader::BrotliWarmupBitReader(br, input)
}

#[inline(always)]
fn CheckInputAmount(safe: bool, br: &bit_reader::BrotliBitReader, num: u32) -> bool {
  safe || bit_reader::BrotliCheckInputAmount(br, num)
}

// Copies 16 bytes `[0..16]` in the ring buffer while checking `<= len - 32` up
// front (the ring buffer always carries `kRingBufferAllocationSlack = 566 >= 32`
// bytes past `ringbuffer_size`). Using the exact same `len < 32 || max > len - 32`
// guard as `memmove16_offset16` lets LLVM GVN eliminate the second bounds check
// on the `17 <= copy_len <= 32` path and kills `max(off_dst, off_src)` before
// the copy, reducing register pressure in `ProcessCommands`.
#[inline(always)]
fn memmove16_from_ringbuffer(data: &mut [u8], u32off_dst: u32, u32off_src: u32) -> bool {
  let off_dst = u32off_dst as usize;
  let off_src = u32off_src as usize;
  let len = data.len();
  if len < 32 || core::cmp::max(off_dst, off_src) > len - 32 {
    return false;
  }
  #[cfg(target_pointer_width = "64")]
  {
    let dst_end = off_dst + 16;
    let src_end = off_src + 16;
    let mut local_array: [u8; 16] = fast_uninitialized!(16);
    local_array.clone_from_slice(&data[off_src..src_end]);
    data[off_dst..dst_end].clone_from_slice(&local_array);
    true
  }
  // On 32-bit targets (ARMv7), `clone_from_slice` on a 16-byte range emits
  // panicking slice bounds checks; matching a 16-element slice prefix lowers
  // to a single unaligned NEON `vld1.8`/`vst1.8` pair with no panic path.
  #[cfg(not(target_pointer_width = "64"))]
  {
    let local_array: [u8; 16] = match data.get(off_src..) {
      Some(&[a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, ..]) => {
        [a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15]
      }
      _ => return false,
    };
    match data.get_mut(off_dst..) {
      Some([d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13, d14, d15, ..]) => {
        *d0 = local_array[0];
        *d1 = local_array[1];
        *d2 = local_array[2];
        *d3 = local_array[3];
        *d4 = local_array[4];
        *d5 = local_array[5];
        *d6 = local_array[6];
        *d7 = local_array[7];
        *d8 = local_array[8];
        *d9 = local_array[9];
        *d10 = local_array[10];
        *d11 = local_array[11];
        *d12 = local_array[12];
        *d13 = local_array[13];
        *d14 = local_array[14];
        *d15 = local_array[15];
        true
      }
      _ => false,
    }
  }
}

// Copies bytes `[16..32]` relative to `u32off_src`/`u32off_dst` by matching a
// 32-byte slice prefix at `off_src`/`off_dst` rather than computing `off + 16`
// up front. This lets LLVM reuse the `&data[off_src]`/`&mut data[off_dst]`
// base pointers from the preceding `memmove16_from_ringbuffer` with an immediate `#16` offset
// instead of hoisting `dst_start + 16`/`src_start + 16` across `copy_len > 32`.
#[inline(always)]
fn memmove16_offset16(data: &mut [u8], u32off_dst: u32, u32off_src: u32) -> bool {
  let off_dst = u32off_dst as usize;
  let off_src = u32off_src as usize;
  let len = data.len();
  if len < 32 || core::cmp::max(off_dst, off_src) > len - 32 {
    return false;
  }
  #[cfg(target_pointer_width = "64")]
  {
    let mut local_array: [u8; 16] = fast_uninitialized!(16);
    local_array.clone_from_slice(&data[off_src + 16..off_src + 32]);
    data[off_dst + 16..off_dst + 32].clone_from_slice(&local_array);
    true
  }
  #[cfg(not(target_pointer_width = "64"))]
  {
    let local_array: [u8; 16] = match data.get(off_src..) {
      Some(
        &[
          _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
          a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
          ..,
        ],
      ) => [a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15],
      _ => return false,
    };
    match data.get_mut(off_dst..) {
      Some(
        [
          _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
          d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13, d14, d15,
          ..,
        ],
      ) => {
        *d0 = local_array[0];
        *d1 = local_array[1];
        *d2 = local_array[2];
        *d3 = local_array[3];
        *d4 = local_array[4];
        *d5 = local_array[5];
        *d6 = local_array[6];
        *d7 = local_array[7];
        *d8 = local_array[8];
        *d9 = local_array[9];
        *d10 = local_array[10];
        *d11 = local_array[11];
        *d12 = local_array[12];
        *d13 = local_array[13];
        *d14 = local_array[14];
        *d15 = local_array[15];
        true
      }
      _ => false,
    }
  }
}

#[inline(always)]
fn copy_overlapping_backreference(data: &mut [u8], dst: usize, distance: usize,
                                 size: usize) -> bool {
  if distance == 0 || distance > dst || dst > data.len() || size > data.len() - dst {
    return false;
  }
  let seed = core::cmp::min(distance, size);
  data.copy_within(dst - distance..dst - distance + seed, dst);
  let mut copied = seed;
  while copied < size {
    let step = core::cmp::min(copied, size - copied);
    data.copy_within(dst..dst + step, dst + copied);
    copied += step;
  }
  true
}

#[cfg_attr(target_pointer_width = "64", inline(always))]
#[cfg_attr(not(target_pointer_width = "64"), inline(never))]
fn memcpy_within_slice(data: &mut [u8], off_dst: usize, off_src: usize, size: usize) -> bool {
  // Both ranges must fit and must not overlap: the `unsafe` build lowers this to
  // copy_nonoverlapping, and the safe build's split_at_mut would panic.
  // Subtraction, not `off + size`, so no caller offset can wrap past the checks.
  let len = data.len();
  let apart = if off_dst > off_src { off_dst - off_src } else { off_src - off_dst };
  if size > len || off_dst > len - size || off_src > len - size || apart < size {
    return false;
  }

  #[cfg(not(feature="unsafe"))]
  {
    if off_dst > off_src {
      let (src, dst) = data.split_at_mut(off_dst);
      let src_slice = fast!((src)[off_src ; off_src + size]);
      fast_mut!((dst)[0;size]).clone_from_slice(src_slice);
    } else {
      let (dst, src) = data.split_at_mut(off_src);
      let src_slice = fast!((src)[0;size]);
      fast_mut!((dst)[off_dst;off_dst + size]).clone_from_slice(src_slice);
    }
  }

  #[cfg(feature="unsafe")]
  unsafe {
    let ptr = data.as_mut_ptr();
    let dst = ptr.add(off_dst);
    let src = ptr.add(off_src);
    core::ptr::copy_nonoverlapping(src, dst, size);
  }
  true
}

pub fn BrotliDecoderHasMoreOutput<AllocU8: alloc::Allocator<u8>,
                           AllocU32: alloc::Allocator<u32>,
                           AllocHC: alloc::Allocator<HuffmanCode>>
  (s: &BrotliState<AllocU8, AllocU32, AllocHC>) -> bool {
  /* After unrecoverable error remaining output is considered nonsensical. */
  if is_fatal(s.error_code) {
    return false;
  }
  s.ringbuffer.len() != 0 && CheckRingBufferConsistency(s) && UnwrittenBytes(s, false) != 0
}
pub fn BrotliDecoderTakeOutput<'a,
                               AllocU8: alloc::Allocator<u8>,
                               AllocU32: alloc::Allocator<u32>,
                               AllocHC: alloc::Allocator<HuffmanCode>>(
  s: &'a mut BrotliState<AllocU8, AllocU32, AllocHC>,
  size: &mut usize,
) -> &'a [u8] {
  let one:usize = 1;
  let mut available_out = if *size != 0 { *size } else { one << 24 };
  let requested_out = available_out;
  if (s.ringbuffer.len() == 0) || is_fatal(s.error_code) {
    *size = 0;
    return &[];
  }
  if !WrapRingBuffer(s) {
    s.error_code = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE;
    *size = 0;
    return &[];
  }
  let mut ign = 0usize;
  let mut ign2 = 0usize;
  let (status, result) = WriteRingBuffer(&mut available_out, None, &mut ign,&mut ign2, true, s);
  // Either WriteRingBuffer returns those "success" codes...
  match status {
    BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS |  BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_OUTPUT => {
      *size = requested_out - available_out;
    },
    _ => {
      // ... or stream is broken. Normally this should be caught by
      //   BrotliDecoderDecompressStream, this is just a safeguard.
      if is_fatal(status) {
        // SaveErrorCode!(s, status); //borrow checker doesn't like this
        // but since it's a safeguard--ignore
      }
      *size = 0;
      return &[];
    }
  }
  return result;
}

#[cfg(feature="ffi-api")]
pub fn BrotliDecoderIsUsed<AllocU8: alloc::Allocator<u8>,
                           AllocU32: alloc::Allocator<u32>,
                           AllocHC: alloc::Allocator<HuffmanCode>>(
  s: &BrotliState<AllocU8, AllocU32, AllocHC>) -> bool {
  !matches!(s.state, BrotliRunningState::BROTLI_STATE_UNINITED)
    || bit_reader::BrotliGetAvailableBits(&s.br) != 0
}

pub fn BrotliDecoderIsFinished<AllocU8: alloc::Allocator<u8>,
                               AllocU32: alloc::Allocator<u32>,
                               AllocHC: alloc::Allocator<HuffmanCode>>(
  s: &BrotliState<AllocU8, AllocU32, AllocHC>) -> bool {
  if let BrotliRunningState::BROTLI_STATE_DONE = s.state {
    !BrotliDecoderHasMoreOutput(s)
  } else {
    false
  }
}

pub fn BrotliDecoderGetErrorCode<AllocU8: alloc::Allocator<u8>,
                               AllocU32: alloc::Allocator<u32>,
                               AllocHC: alloc::Allocator<HuffmanCode>>(
  s: &BrotliState<AllocU8, AllocU32, AllocHC>) -> BrotliDecoderErrorCode {
  s.error_code
}

// The tree with the given index in a Huffman tree group, as
// HuffmanTreeGroup::build_hgroup_cache lists them; empty if there is none.
#[inline(always)]
fn huffman_tree<AllocU32: alloc::Allocator<u32>, AllocHC: alloc::Allocator<HuffmanCode>>(
  group: &HuffmanTreeGroup<AllocU32, AllocHC>,
  index: usize,
) -> &[HuffmanCode] {
  match group.htrees.slice().get(index) {
    Some(offset) => group.codes.slice().get(*offset as usize..).unwrap_or(&[]),
    None => &[],
  }
}

// As huffman_tree, as a fixed window (see huffman::HuffmanTable).
// Outlined on 32-bit targets so LLVM LICM does not hoist `&group.codes` and
// `&EMPTY_TABLE` out of cold block-switch paths into `ProcessCommands`'s
// loop preheader.
#[cfg_attr(target_pointer_width = "64", inline(always))]
#[cfg_attr(not(target_pointer_width = "64"), inline(never))]
fn huffman_window<AllocU32: alloc::Allocator<u32>, AllocHC: alloc::Allocator<HuffmanCode>,
                  T: huffman::TableWindow>(
  group: &HuffmanTreeGroup<AllocU32, AllocHC>,
  index: usize,
) -> &T {
  let offset = group.htrees.slice().get(index).map(|o| *o as usize);
  huffman::table_window(group.codes.slice(), offset)
}

type LiteralTable = [HuffmanCode; LITERAL_TABLE_WINDOW];
type InsertCopyTable = [HuffmanCode; INSERT_COPY_TABLE_WINDOW];
type DistanceRootTable = [HuffmanCode; DISTANCE_ROOT_WINDOW];

// The 256-entry root tables of the 4 distance contexts at `slice_index` in the
// distance context map, sharing the lookup when contexts use the same tree
// (all 4 do below q10). Thin `&DistanceRootTable` pointers avoid carrying a
// fat-slice length register and per-symbol bounds check in `read_distance`.
#[cfg_attr(target_pointer_width = "64", inline(always))]
#[cfg_attr(not(target_pointer_width = "64"), inline(never))]
fn load_distance_roots<'a, AllocU32: alloc::Allocator<u32>, AllocHC: alloc::Allocator<HuffmanCode>>(
  group: &'a HuffmanTreeGroup<AllocU32, AllocHC>,
  map: &[u8],
  slice_index: usize,
) -> [&'a DistanceRootTable; 4] {
  let idx: [u8; 4] = match map.get(slice_index..).and_then(|m| m.get(..4)) {
    Some(m) => [m[0], m[1], m[2], m[3]],
    None => return EMPTY_DISTANCE_ROOTS,
  };
  let htrees = group.htrees.slice();
  let codes = group.codes.slice();
  let t0: &DistanceRootTable =
    huffman::table_window(codes, htrees.get(idx[0] as usize).map(|&o| o as usize));
  let t1 = if idx[1] == idx[0] {
    t0
  } else {
    huffman::table_window(codes, htrees.get(idx[1] as usize).map(|&o| o as usize))
  };
  let t2 = if idx[2] == idx[1] {
    t1
  } else if idx[2] == idx[0] {
    t0
  } else {
    huffman::table_window(codes, htrees.get(idx[2] as usize).map(|&o| o as usize))
  };
  let t3 = if idx[3] == idx[2] {
    t2
  } else if idx[3] == idx[1] {
    t1
  } else if idx[3] == idx[0] {
    t0
  } else {
    huffman::table_window(codes, htrees.get(idx[3] as usize).map(|&o| o as usize))
  };
  [t0, t1, t2, t3]
}

// Fills `trees[context]` with the literal tree of each of the 64 contexts of
// the literal block type whose context map starts at `context_map[slice_index]`.
#[inline(never)]
fn fill_context_trees<'a, AllocU32: alloc::Allocator<u32>, AllocHC: alloc::Allocator<HuffmanCode>>(
  trees: &mut [&'a LiteralTable; 64],
  group: &'a HuffmanTreeGroup<AllocU32, AllocHC>,
  context_map: &[u8],
  slice_index: usize,
) {
  match context_map.get(slice_index..).and_then(|map| map.get(..64)) {
    Some(map) => {
      // Context maps are RLE-encoded and typically contain long runs of
      // identical tree indices; reuse the resolved table reference across
      // adjacent equal entries.
      let htrees = group.htrees.slice();
      let codes = group.codes.slice();
      let mut prev_index = usize::MAX;
      let mut prev_tree: &'a LiteralTable = &EMPTY_LITERAL_TABLE;
      for (tree, &index) in trees.iter_mut().zip(map.iter()) {
        let idx = index as usize;
        if idx != prev_index {
          prev_index = idx;
          prev_tree = huffman::table_window(codes, htrees.get(idx).map(|&o| o as usize));
        }
        *tree = prev_tree;
      }
    }
    None => *trees = EMPTY_CONTEXT_TREES,
  }
}

// Takes `fbr` by value and returns it (like `decode_literal_batch`) so
// outlining on 32-bit targets does not force `fbr` into a stack `alloca` in
// `ProcessCommands`.
#[cfg_attr(target_pointer_width = "64", inline(always))]
#[cfg_attr(not(target_pointer_width = "64"), inline(never))]
fn decode_block_type_and_length_slice<'a, AllocHC: alloc::Allocator<HuffmanCode>>(
  btl: &mut BlockTypeAndLengthState<AllocHC>,
  mut fbr: bit_reader::BitReaderSlice<'a>,
  tree_type: i32,
) -> (bit_reader::BitReaderSlice<'a>, BrotliDecoderErrorCode) {
  // A reader over the remaining input, in the same bit layout as the slice.
  let mut temp_br = bit_reader::BrotliBitReader {
    val_: fbr.val,
    bit_pos_: fbr.avail_bits,
    next_in: 0,
    avail_in: fbr.rem.len() as u32,
  };
  let status = DecodeBlockTypeAndLength(false, btl, &mut temp_br, tree_type, fbr.rem);
  fbr.val = temp_br.val_;
  fbr.avail_bits = temp_br.bit_pos_;
  fbr.rem = &fbr.rem[temp_br.next_in as usize..];
  (fbr, status)
}

#[derive(Clone, Copy)]
enum LiteralBatchStatus {
  Decoded,
  NotHandled,
  CorruptInput,
}

#[inline(always)]
fn decode_literal_batch<'b>(
  rb: &mut [u8],
  pos: usize,
  count: usize,
  trivial_literal_context: i32,
  literal_htree: &LiteralTable,
  context_trees: &[&LiteralTable; 64],
  context_lookup: &[u8; 512],
  mut fbr: bit_reader::BitReaderSlice<'b>,
) -> (bit_reader::BitReaderSlice<'b>, LiteralBatchStatus) {
  if trivial_literal_context != 0 {
    let ring_window = match rb.get_mut(pos..).and_then(|w| w.get_mut(..count)) {
      Some(w) => w,
      None => return (fbr, LiteralBatchStatus::NotHandled),
    };
    for dst in ring_window.iter_mut() {
      if !fbr.fill16_checked() {
        mark_unlikely();
        return (fbr, LiteralBatchStatus::CorruptInput);
      }
      *dst = decode_symbol_no_fill(&mut fbr, literal_htree) as u8;
    }
    (fbr, LiteralBatchStatus::Decoded)
  } else {
    let start = pos.wrapping_sub(2);
    let len = count + 2;
    let (p2_init, p1_init, ring_window) =
      match rb.get_mut(start..).and_then(|w| w.get_mut(..len)) {
        Some([p2_init, p1_init, ring_window @ ..]) => (p2_init, p1_init, ring_window),
        _ => return (fbr, LiteralBatchStatus::NotHandled),
      };
    let mut p1: u8 = *p1_init;
    let mut p2: u8 = *p2_init;
    for dst in ring_window.iter_mut() {
      if !fbr.fill16_checked() {
        mark_unlikely();
        return (fbr, LiteralBatchStatus::CorruptInput);
      }
      let context = (context_lookup[p1 as usize] | context_lookup[(p2 as usize) | 256]) as usize;
      let hc = context_trees[context & 63];
      p2 = p1;
      p1 = decode_symbol_no_fill(&mut fbr, hc) as u8;
      *dst = p1;
    }
    (fbr, LiteralBatchStatus::Decoded)
  }
}

#[cold]
#[inline(never)]
fn execute_custom_shared_dictionary_copy<AllocU8: alloc::Allocator<u8>, AllocU32: alloc::Allocator<u32>>(
  shared_dict: &::shared_dictionary::BrotliSharedDictionary<AllocU8, AllocU32>,
  context_lookup: &[u8; 512],
  dist_rb_idx: &mut i32,
  meta_block_remaining_len: &mut i32,
  mut pos: i32,
  rb: &mut [u8],
  rb_size: i32,
  rb_mask: i32,
  distance_code: i32,
  max_distance: i32,
  distance_context: i32,
  copy_len: i32,
  compound_dictionary_size: u32,
) -> Result<(i32, bool), BrotliDecoderErrorCode> {
  let dict_id = if shared_dict.context_based {
    let p1 = rb[(pos.wrapping_sub(1) as usize) & (rb_mask as usize)];
    let p2 = rb[(pos.wrapping_sub(2) as usize) & (rb_mask as usize)];
    let context = (context_lookup[p1 as usize] | context_lookup[p2 as usize | 256]) as usize;
    shared_dict.context_map[context]
  } else {
    0
  };
  let address = distance_code - max_distance - 1 - compound_dictionary_size as i32;
  *dist_rb_idx += distance_context;
  let lookup = shared_dict.lookup(dict_id, copy_len, address).map_err(|why| match why {
    DictionaryLookupError::LengthNotEncodable =>
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_DICTIONARY,
    DictionaryLookupError::AddressOutOfRange =>
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_TRANSFORM,
  })?;
  let mut len = copy_len;
  let word = lookup.words.word(len, lookup.word_idx);
  if lookup.transform_idx == lookup.transforms.cutoff_identity() {
    rb[pos as usize..(pos + len) as usize].clone_from_slice(word);
  } else {
    len = lookup.transforms.apply(&mut rb[pos as usize..], word, len, lookup.transform_idx);
    if len == 0 && distance_code <= 120 {
      return Err(BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_TRANSFORM);
    }
  }
  pos += len;
  *meta_block_remaining_len -= len;
  Ok((pos, pos >= rb_size))
}

// Executes a compound, custom shared, or static dictionary reference when
// `distance_code > max_distance`. Returns `Ok((new_pos, post_write_1))`.
#[inline(always)]
fn execute_dictionary_copy<AllocU8: alloc::Allocator<u8>, AllocU32: alloc::Allocator<u32>>(
  compound_dictionary: &mut state::BrotliDecoderCompoundDictionary<AllocU8>,
  shared_dict: &::shared_dictionary::BrotliSharedDictionary<AllocU8, AllocU32>,
  context_lookup: &[u8; 512],
  dist_rb: &mut [i32; 4],
  dist_rb_idx: &mut i32,
  meta_block_remaining_len: &mut i32,
  mut pos: i32,
  rb: &mut [u8],
  rb_size: i32,
  rb_mask: i32,
  distance_code: i32,
  max_distance: i32,
  distance_context: i32,
  copy_len: i32,
  external_dictionary: &[u8],
) -> Result<(i32, bool), BrotliDecoderErrorCode> {
  if distance_code > kBrotliMaxAllowedDistance as i32 {
    return Err(BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_DISTANCE);
  }
  let compound_dictionary_size = compound_dictionary.total_size as u32;
  // Distances in (max_distance, max_distance + compound_dictionary_size]
  // address the attached compound dictionary, furthest byte first.
  if (distance_code - max_distance) as u32 <= compound_dictionary_size {
    let address = compound_dictionary_size - (distance_code - max_distance) as u32;
    let address = address as usize;
    let length = copy_len as usize;
    // A dictionary of one chunk (the common case: one attached raw
    // dictionary) whose copy ends before the ring buffer does is a plain
    // slice copy, without the chunked block-map walk.
    if compound_dictionary.num_chunks == 1 && ((rb_size - pos) as usize) > length {
      if length > compound_dictionary.total_size - address {
        return Err(BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_COMPOUND_DICTIONARY);
      }
      let chunk = match compound_dictionary.chunks[0] {
        state::MaybeOwnedSlice::External(_) => external_dictionary,
        ref chunk => chunk.slice(),
      };
      let (dst, src) = match (rb.get_mut(pos as usize..pos as usize + length),
                              chunk.get(address..address + length)) {
        (Some(dst), Some(src)) => (dst, src),
        _ => return Err(BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_COMPOUND_DICTIONARY),
      };
      dist_rb[(*dist_rb_idx & 3) as usize] = distance_code;
      *dist_rb_idx += 1;
      *meta_block_remaining_len -= copy_len;
      copy_bytes(dst, src);
      return Ok((pos + copy_len, false));
    }
    if !InitializeCompoundDictionaryCopy(compound_dictionary, address, length) {
      return Err(BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_COMPOUND_DICTIONARY);
    }
    dist_rb[(*dist_rb_idx & 3) as usize] = distance_code;
    *dist_rb_idx += 1;
    *meta_block_remaining_len -= copy_len;
    pos += CopyFromCompoundDictionary(compound_dictionary, rb, rb_size, pos, external_dictionary);
    return Ok((pos, pos >= rb_size));
  }
  if shared_dict.is_custom()
    && copy_len >= SHARED_BROTLI_MIN_DICTIONARY_WORD_LENGTH as i32
    && copy_len <= SHARED_BROTLI_MAX_DICTIONARY_WORD_LENGTH as i32
  {
    return execute_custom_shared_dictionary_copy(
      shared_dict,
      context_lookup,
      dist_rb_idx,
      meta_block_remaining_len,
      pos,
      rb,
      rb_size,
      rb_mask,
      distance_code,
      max_distance,
      distance_context,
      copy_len,
      compound_dictionary_size,
    );
  }
  if copy_len >= kBrotliMinDictionaryWordLength as i32
    && copy_len <= kBrotliMaxDictionaryWordLength as i32
  {
    let mut offset = fast!((kBrotliDictionaryOffsetsByLength)[copy_len as usize]) as i32;
    let word_id = distance_code - max_distance - 1 - compound_dictionary_size as i32;
    let shift = fast!((kBrotliDictionarySizeBitsByLength)[copy_len as usize]);
    let mask = bit_reader::BitMask(shift as u32) as i32;
    let word_idx = word_id & mask;
    let transform_idx = word_id >> shift;
    *dist_rb_idx += distance_context;
    offset += word_idx * copy_len;
    if transform_idx >= kNumTransforms {
      return Err(BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_TRANSFORM);
    }
    let mut len = copy_len;
    let word = fast!((kBrotliDictionary)[offset as usize ; (offset + len) as usize]);
    if transform_idx == 0 {
      copy_bytes(&mut rb[pos as usize..(pos + len) as usize], word);
    } else {
      len = TransformDictionaryWord(&mut rb[pos as usize..], word, len, transform_idx);
    }
    pos += len;
    *meta_block_remaining_len -= len;
    return Ok((pos, pos >= rb_size));
  }
  Err(BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_DICTIONARY)
}

// Copies `rem` bytes from `pos - distance_code` (modulo `rb_mask`) into `rb`,
// stopping early and returning `(new_pos, new_rem, true)` if `pos` reaches `rb_size`.
#[cfg_attr(target_pointer_width = "64", inline(always))]
#[cfg_attr(not(target_pointer_width = "64"), inline(never))]
fn copy_wrap_or_overlapping(
  rb: &mut [u8],
  rb_size: i32,
  rb_mask: i32,
  mut pos: i32,
  mut rem: i32,
  distance_code: i32,
) -> (i32, i32, bool) {
  let rb_mask_usize = (rb_mask as usize) & 0x7fff_ffff;
  if rb.len() <= rb_mask_usize {
    return (pos, rem, false);
  }
  let mut wrap_guard = rb_size - pos;
  if rem >= 16 && pos >= 0 && wrap_guard > 0 && distance_code > 0 && distance_code <= pos {
    let count = core::cmp::min(rem, wrap_guard);
    if copy_overlapping_backreference(rb, pos as usize, distance_code as usize, count as usize) {
      pos += count;
      rem -= count;
      wrap_guard -= count;
      if wrap_guard == 0 {
        return (pos, rem, true);
      }
    }
  }
  while rem > 0 {
    rem -= 1;
    rb[(pos as usize) & rb_mask_usize] =
      rb[(pos.wrapping_sub(distance_code) as usize) & rb_mask_usize];
    pos += 1;
    wrap_guard -= 1;
    if wrap_guard == 0 {
      return (pos, rem, true);
    }
  }
  (pos, rem, false)
}

#[inline(always)]
fn save_mid_command_state<AllocU8: alloc::Allocator<u8>,
                          AllocU32: alloc::Allocator<u32>,
                          AllocHC: alloc::Allocator<HuffmanCode>>(
  s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
  i: i32,
  distance_code: i32,
  distance_context: i32,
  copy_length: i32,
) {
  s.loop_counter = i;
  s.distance_code = distance_code;
  s.distance_context = distance_context;
  s.copy_length = copy_length;
  if let Some(&idx) = s
    .dist_context_map
    .slice()
    .get(s.dist_context_map_slice_index + (distance_context as usize & 3))
  {
    s.dist_htree_index = idx;
  }
}

// Unified command loop parameterized over a `CommandLoopMode`:
// - `FastCommandLoop`: entered from `COMMAND_BEGIN` with `>= 28` input bytes,
//   compiling to a single-entry loop over `BitReaderSlice` with in-place
//   `HuffmanTreeGroup` borrows.
// - `SafeCommandLoop`: streaming/tail fallback handling arbitrary command entry
//   states, `< 28`-byte input tails, and single-command resumption.
#[inline(always)]
fn ProcessCommandsInternal<
  M: CommandLoopMode,
  AllocU8: alloc::Allocator<u8>,
  AllocU32: alloc::Allocator<u32>,
  AllocHC: alloc::Allocator<HuffmanCode>,
>(
  single_command: bool,
  s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
  input: &[u8],
  dictionary: &[u8],
) -> BrotliDecoderErrorCode {
  if M::SAFE {
    if (!CheckInputAmount(true, &s.br, 28)) || (!WarmupBitReader(true, &mut s.br, input)) {
      mark_unlikely();
      return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
    }
    if !matches!(
      s.state,
      BrotliRunningState::BROTLI_STATE_COMMAND_BEGIN
        | BrotliRunningState::BROTLI_STATE_COMMAND_INNER
        | BrotliRunningState::BROTLI_STATE_COMMAND_POST_DECODE_LITERALS
        | BrotliRunningState::BROTLI_STATE_COMMAND_POST_WRAP_COPY
    ) {
      mark_unlikely();
      return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE;
    }
  }

  let rb_size = s.ringbuffer_size;
  let rb_mask = s.ringbuffer_mask;
  let mut pos = s.pos;
  if !M::SAFE && (rb_size < 2 || (rb_size & (rb_size - 1)) != 0 || rb_mask != rb_size - 1 || pos < 0) {
    mark_unlikely();
    return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE;
  }
  let rb_mask_usize = (rb_mask as usize) & 0x7fff_ffff;
  let min_rb_len = rb_mask_usize + kRingBufferAllocationSlack;
  if !M::SAFE && (pos >= rb_size || s.ringbuffer.slice().len() < min_rb_len) {
    mark_unlikely();
    return SafeProcessCommands(s, input, dictionary);
  }

  let mut fbr = if !M::SAFE {
    match bit_reader::BitReaderSlice::new(&s.br, input) {
      Some(r) => r,
      None => {
        mark_unlikely();
        return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE;
      }
    }
  } else {
    bit_reader::BitReaderSlice::empty()
  };
  let rb: &mut [u8] = if !M::SAFE {
    match s.ringbuffer.slice_mut().get_mut(..min_rb_len) {
      Some(r) => r,
      None => {
        mark_unlikely();
        return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE;
      }
    }
  } else {
    s.ringbuffer.slice_mut()
  };

  let mut insert_copy_tree: &InsertCopyTable = huffman_window(&s.insert_copy_hgroup, s.htree_command_index as usize);
  let mut literal_htree: &LiteralTable = huffman_window(&s.literal_hgroup, s.literal_htree_index as usize);
  let mut context_trees: [&LiteralTable; 64] = EMPTY_CONTEXT_TREES;
  let mut context_trees_slice: Option<usize> = None;
  let mut distance_tree_index = s.dist_htree_index;
  let mut distance_tree: &[HuffmanCode] = if M::SAFE {
    huffman_tree(&s.distance_hgroup, distance_tree_index as usize)
  } else {
    &[]
  };
  // The 256-entry root tables of the 4 distance contexts of the current
  // distance block type, resolved once per block type instead of reading the
  // distance context map and looking a tree up whenever consecutive distances
  // use different contexts (most distances at q10-q11).
  let mut distance_roots: [&DistanceRootTable; 4] = if !M::SAFE {
    load_distance_roots(&s.distance_hgroup, s.dist_context_map.slice(), s.dist_context_map_slice_index)
  } else {
    EMPTY_DISTANCE_ROOTS
  };
  if !M::SAFE && s.trivial_literal_context == 0 {
    fill_context_trees(
      &mut context_trees,
      &s.literal_hgroup,
      s.context_map.slice(),
      s.context_map_slice_index,
    );
    context_trees_slice = Some(s.context_map_slice_index);
  }

  let mut state = if M::SAFE {
    core::mem::replace(&mut s.state, BrotliRunningState::BROTLI_STATE_COMMAND_BEGIN)
  } else {
    BrotliRunningState::BROTLI_STATE_COMMAND_BEGIN
  };
  if M::SAFE
    && !matches!(
      state,
      BrotliRunningState::BROTLI_STATE_COMMAND_BEGIN
        | BrotliRunningState::BROTLI_STATE_COMMAND_INNER
        | BrotliRunningState::BROTLI_STATE_COMMAND_POST_DECODE_LITERALS
        | BrotliRunningState::BROTLI_STATE_COMMAND_POST_WRAP_COPY
    )
  {
    s.state = state;
    return BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE;
  }
  let mut i: i32 = if M::SAFE { s.loop_counter } else { 0 };
  let mut distance_code: i32 = if M::SAFE { s.distance_code } else { 0 };
  let mut distance_context: i32 = if M::SAFE { s.distance_context } else { 0 };
  let mut copy_length: i32 = if M::SAFE { s.copy_length } else { 0 };

  let mut exit_state = BrotliRunningState::BROTLI_STATE_COMMAND_BEGIN;
  let mut result = BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
  let mut mid_cmd: Option<(i32, i32, i32, i32)> = None;

  'cmd: loop {
    if !M::SAFE {
      i = 0;
      distance_code = 0;
      distance_context = 0;
      copy_length = 0;
    }
    if !M::SAFE || matches!(state, BrotliRunningState::BROTLI_STATE_COMMAND_BEGIN) {
      if !M::SAFE && !fbr.has_input() {
        mark_unlikely();
        result = BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
        break 'cmd;
      }
      if s.block_type_length_state.block_length[1] == 0 {
        mark_unlikely();
        let status = if !M::SAFE {
          let (next_fbr, st) =
            decode_block_type_and_length_slice(&mut s.block_type_length_state, fbr, 1);
          fbr = next_fbr;
          if matches!(st, BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS) {
            s.htree_command_index = s.block_type_length_state.block_type_rb[3] as u16;
            insert_copy_tree = huffman_window(&s.insert_copy_hgroup, s.htree_command_index as usize);
          }
          st
        } else {
          let st = DecodeBlockTypeAndLength(
            true,
            &mut s.block_type_length_state,
            &mut s.br,
            1,
            input,
          );
          if matches!(st, BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS) {
            s.htree_command_index = s.block_type_length_state.block_type_rb[3] as u16;
            insert_copy_tree = huffman_window(&s.insert_copy_hgroup, s.htree_command_index as usize);
          }
          st
        };
        if !matches!(status, BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS) {
          result = status;
          break 'cmd;
        }
        continue 'cmd;
      }

      if !read_command::<M>(
        &mut fbr,
        &mut s.br,
        input,
        &mut distance_code,
        &mut distance_context,
        &mut copy_length,
        &mut s.block_type_length_state.block_length[1],
        &mut i,
        insert_copy_tree,
      ) {
        mark_unlikely();
        result = if M::SAFE {
          BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT
        } else {
          BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE
        };
        break 'cmd;
      }

      if M::SAFE {
        if i == 0 {
          state = BrotliRunningState::BROTLI_STATE_COMMAND_POST_DECODE_LITERALS;
        } else {
          s.meta_block_remaining_len -= i;
          state = BrotliRunningState::BROTLI_STATE_COMMAND_INNER;
        }
      } else if i != 0 {
        s.meta_block_remaining_len -= i;
      }
    }

    if (!M::SAFE && i != 0) || (M::SAFE && matches!(state, BrotliRunningState::BROTLI_STATE_COMMAND_INNER)) {
      'inner: loop {
        if !M::SAFE {
          let i_u32 = i as u32;
          let block_len = s.block_type_length_state.block_length[0];
          let mut handled = false;
          if i > 0 && i_u32 <= block_len && pos < rb_size - i && i_u32 * 2 + 34 <= fbr.avail_in() {
            let (next_fbr, status) = decode_literal_batch(
              rb,
              pos as usize,
              i as usize,
              s.trivial_literal_context,
              literal_htree,
              &context_trees,
              s.context_lookup,
              fbr,
            );
            fbr = next_fbr;
            match status {
              LiteralBatchStatus::Decoded => {
                pos += i;
                s.block_type_length_state.block_length[0] -= i_u32;
                handled = true;
              }
              LiteralBatchStatus::NotHandled => {}
              LiteralBatchStatus::CorruptInput => {
                result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE;
                break 'cmd;
              }
            }
          }
          if handled {
            break 'inner;
          }
          mark_unlikely();
        }

        let mut p1: u8 = if !M::SAFE {
          rb[(pos.wrapping_sub(1) as usize) & rb_mask_usize]
        } else if rb.is_empty() {
          0
        } else {
          fast!((rb)[((pos - 1) & rb_mask) as usize])
        };
        let mut p2: u8 = if !M::SAFE {
          rb[(pos.wrapping_sub(2) as usize) & rb_mask_usize]
        } else if rb.is_empty() {
          0
        } else {
          fast!((rb)[((pos - 2) & rb_mask) as usize])
        };
        loop {
          if !M::SAFE && !fbr.has_input() {
            exit_state = BrotliRunningState::BROTLI_STATE_COMMAND_INNER;
            mid_cmd = Some((i, distance_code, distance_context, copy_length));
            result = BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
            break 'cmd;
          }
          if s.block_type_length_state.block_length[0] == 0 {
            if !M::SAFE {
              mark_unlikely();
              let status = if !fbr.has_input() {
                BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT
              } else {
                let (next_fbr, st) =
                  decode_block_type_and_length_slice(&mut s.block_type_length_state, fbr, 0);
                fbr = next_fbr;
                st
              };
              if !matches!(status, BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS) {
                exit_state = BrotliRunningState::BROTLI_STATE_COMMAND_INNER;
                mid_cmd = Some((i, distance_code, distance_context, copy_length));
                result = status;
                break 'cmd;
              }
              update_literal_block_state(
                &s.block_type_length_state.block_type_rb,
                &s.trivial_literal_contexts,
                s.context_map.slice(),
                s.context_modes.slice(),
                &mut s.context_map_slice_index,
                &mut s.trivial_literal_context,
                &mut s.literal_htree_index,
                &mut s.context_lookup,
              );
              literal_htree = huffman_window(&s.literal_hgroup, s.literal_htree_index as usize);
              if s.trivial_literal_context == 0
                && context_trees_slice != Some(s.context_map_slice_index)
              {
                fill_context_trees(
                  &mut context_trees,
                  &s.literal_hgroup,
                  s.context_map.slice(),
                  s.context_map_slice_index,
                );
                context_trees_slice = Some(s.context_map_slice_index);
              }
              continue 'inner;
            } else {
              mark_unlikely();
              let status = DecodeBlockTypeAndLength(
                true,
                &mut s.block_type_length_state,
                &mut s.br,
                0,
                input,
              );
              if !matches!(status, BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS) {
                exit_state = BrotliRunningState::BROTLI_STATE_COMMAND_INNER;
                mid_cmd = Some((i, distance_code, distance_context, copy_length));
                result = status;
                break 'cmd;
              }
              update_literal_block_state(
                &s.block_type_length_state.block_type_rb,
                &s.trivial_literal_contexts,
                s.context_map.slice(),
                s.context_modes.slice(),
                &mut s.context_map_slice_index,
                &mut s.trivial_literal_context,
                &mut s.literal_htree_index,
                &mut s.context_lookup,
              );
              literal_htree = huffman_window(&s.literal_hgroup, s.literal_htree_index as usize);
              if s.block_type_length_state.block_length[0] == 0 {
                exit_state = BrotliRunningState::BROTLI_STATE_COMMAND_INNER;
                mid_cmd = Some((i, distance_code, distance_context, copy_length));
                result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS;
                break 'cmd;
              }
            }
          }
          let hc = if s.trivial_literal_context != 0 {
            literal_htree
          } else if !M::SAFE {
            let context = (s.context_lookup[p1 as usize] | s.context_lookup[p2 as usize | 256]) as usize;
            context_trees[context & 63]
          } else {
            let context = (s.context_lookup[p1 as usize] | s.context_lookup[p2 as usize | 256]) as usize;
            let idx = fast_slice!((s.context_map)[s.context_map_slice_index + (context & 63)]);
            huffman_window(&s.literal_hgroup, idx as usize)
          };
          let lit = if !M::SAFE {
            read_symbol_slice(&mut fbr, hc) as u8
          } else {
            let mut sym = 0u32;
            if !SafeReadSymbol(hc, &mut s.br, &mut sym, input) {
              exit_state = BrotliRunningState::BROTLI_STATE_COMMAND_INNER;
              mid_cmd = Some((i, distance_code, distance_context, copy_length));
              result = BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
              break 'cmd;
            }
            sym as u8
          };
          p2 = p1;
          p1 = lit;
          if !M::SAFE {
            rb[(pos as usize) & rb_mask_usize] = lit;
          } else {
            fast_mut!((rb)[pos as usize]) = lit;
          }
          s.block_type_length_state.block_length[0] -= 1;
          pos += 1;
          if pos == rb_size {
            exit_state = BrotliRunningState::BROTLI_STATE_COMMAND_INNER_WRITE;
            mid_cmd = Some((i - 1, distance_code, distance_context, copy_length));
            break 'cmd;
          }
          i -= 1;
          if i == 0 {
            break 'inner;
          }
        }
      }
      if s.meta_block_remaining_len <= 0 {
        mark_unlikely();
        exit_state = BrotliRunningState::BROTLI_STATE_METABLOCK_DONE;
        break 'cmd;
      }
      if M::SAFE {
        state = BrotliRunningState::BROTLI_STATE_COMMAND_POST_DECODE_LITERALS;
      }
    }

    if !M::SAFE || matches!(state, BrotliRunningState::BROTLI_STATE_COMMAND_POST_DECODE_LITERALS) {
      if distance_code >= 0 {
        distance_context = if distance_code != 0 { 0 } else { 1 };
        s.dist_rb_idx -= 1;
        distance_code = s.dist_rb[(s.dist_rb_idx & 3) as usize];
      } else {
        if s.block_type_length_state.block_length[2] == 0 {
          mark_unlikely();
          let status = if !M::SAFE {
            if !fbr.has_input() {
              BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT
            } else {
              let (next_fbr, st) =
                decode_block_type_and_length_slice(&mut s.block_type_length_state, fbr, 2);
              fbr = next_fbr;
              st
            }
          } else {
            DecodeBlockTypeAndLength(
              true,
              &mut s.block_type_length_state,
              &mut s.br,
              2,
              input,
            )
          };
          if !matches!(status, BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS) {
            exit_state = BrotliRunningState::BROTLI_STATE_COMMAND_POST_DECODE_LITERALS;
            mid_cmd = Some((0, distance_code, distance_context, copy_length));
            result = status;
            break 'cmd;
          }
          s.dist_context_map_slice_index =
            (s.block_type_length_state.block_type_rb[5] << kDistanceContextBits) as usize;
          if !M::SAFE {
            distance_roots = load_distance_roots(
              &s.distance_hgroup,
              s.dist_context_map.slice(),
              s.dist_context_map_slice_index,
            );
          }
        }
        let memento = if M::SAFE {
          bit_reader::BrotliBitReaderSaveState(&s.br)
        } else {
          bit_reader::BrotliBitReaderState::default()
        };
        let dist_sym = if !M::SAFE {
          let distance_root = distance_roots[(distance_context as usize) & 3];
          fbr.fill16();
          let bits = fbr.val as u32;
          let table_index = (bits & HUFFMAN_TABLE_MASK) as usize;
          let root = distance_root.entry(table_index);
          let (sym, consumed) = if root.bits > HUFFMAN_TABLE_BITS as u8 {
            decode_distance_subtable(
              bits,
              table_index,
              root,
              &s.distance_hgroup,
              s.dist_context_map.slice(),
              s.dist_context_map_slice_index,
              distance_context,
            )
          } else {
            (root.value as usize, root.bits as u32)
          };
          fbr.avail_bits = fbr.avail_bits.wrapping_sub(consumed);
          fbr.val >>= consumed & bit_reader::SLICE_SHIFT_MASK;
          sym
        } else {
          let idx = fast_slice!((s.dist_context_map)[s.dist_context_map_slice_index + (distance_context as usize & 3)]);
          s.dist_htree_index = idx;
          if idx != distance_tree_index {
            distance_tree_index = idx;
            distance_tree = huffman_tree(&s.distance_hgroup, idx as usize);
          }
          let mut sym: u32 = 0;
          if !SafeReadSymbol(distance_tree, &mut s.br, &mut sym, input) {
            exit_state = BrotliRunningState::BROTLI_STATE_COMMAND_POST_DECODE_LITERALS;
            mid_cmd = Some((0, distance_code, distance_context, copy_length));
            result = BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
            break 'cmd;
          }
          sym as usize
        };
        if !read_distance::<M>(
          dist_sym,
          &memento,
          &mut fbr,
          &mut s.br,
          input,
          &mut distance_code,
          &mut distance_context,
          &mut s.dist_rb_idx,
          &s.dist_rb,
          &mut s.block_type_length_state.block_length[2],
          s.distance_postfix_bits,
          s.distance_postfix_mask,
          s.num_direct_distance_codes,
        ) {
          exit_state = BrotliRunningState::BROTLI_STATE_COMMAND_POST_DECODE_LITERALS;
          mid_cmd = Some((0, distance_code, distance_context, copy_length));
          result = BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
          break 'cmd;
        }
      }

      let max_backward_distance = s.max_backward_distance;
      let mut max_distance = s.max_distance;
      if max_distance != max_backward_distance {
        max_distance = if pos < max_backward_distance {
          pos
        } else {
          max_backward_distance
        };
        s.max_distance = max_distance;
      }

      i = copy_length;
      if distance_code > max_distance {
        mark_unlikely();
        match execute_dictionary_copy(
          &mut s.compound_dictionary,
          &s.dictionary,
          s.context_lookup,
          &mut s.dist_rb,
          &mut s.dist_rb_idx,
          &mut s.meta_block_remaining_len,
          pos,
          rb,
          rb_size,
          rb_mask,
          distance_code,
          max_distance,
          distance_context,
          i,
          dictionary,
        ) {
          Ok((next_pos, true)) => {
            pos = next_pos;
            exit_state = BrotliRunningState::BROTLI_STATE_COMMAND_POST_WRITE_1;
            mid_cmd = Some((i, distance_code, distance_context, copy_length));
            break 'cmd;
          }
          Ok((next_pos, false)) => {
            pos = next_pos;
            if s.meta_block_remaining_len <= 0 {
              exit_state = BrotliRunningState::BROTLI_STATE_METABLOCK_DONE;
              break 'cmd;
            }
            if M::SAFE {
              state = BrotliRunningState::BROTLI_STATE_COMMAND_BEGIN;
              if single_command {
                break 'cmd;
              }
            }
            continue 'cmd;
          }
          Err(err) => {
            result = err;
            break 'cmd;
          }
        }
      }

      s.dist_rb[(s.dist_rb_idx & 3) as usize] = distance_code;
      s.dist_rb_idx += 1;
      s.meta_block_remaining_len -= i;

      let src_start = (pos.wrapping_sub(distance_code) as usize) & rb_mask_usize;
      let dst_start = (pos as usize) & rb_mask_usize;
      let src_start_u32 = src_start as u32;
      let dst_start_u32 = pos as u32;
      let copy_len_u32 = i as u32;
      let lo = core::cmp::min(dst_start_u32, src_start_u32);
      let hi = core::cmp::max(dst_start_u32, src_start_u32);
      // Check if the copy extends over the ringbuffer end or overlaps with
      // itself before the speculative 16-byte copy. `copy_wrap_or_overlapping`
      // overwrites `[pos..pos + i]` from the beginning, so skipping the
      // 16-byte copy on the wrap/overlap path is semantically identical and
      // lets the wrap check (`hi + copy_len_u32`), overlap check (`hi - lo <
      // copy_len_u32`), and slice bounds check (`hi <= len - 32`) share
      // `hi = max(dst_start_u32, src_start_u32)`.
      if hi - lo < copy_len_u32 || hi.wrapping_add(copy_len_u32) >= rb_size as u32 {
        mark_unlikely();
        if M::SAFE {
          if rb.len() < 32 {
            result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_DISTANCE;
            break 'cmd;
          }
          state = BrotliRunningState::BROTLI_STATE_COMMAND_POST_WRAP_COPY;
        } else {
          let (next_pos, next_i, wrapped) =
            copy_wrap_or_overlapping(rb, rb_size, rb_mask, pos, i, distance_code);
          pos = next_pos;
          i = next_i;
          if wrapped {
            exit_state = BrotliRunningState::BROTLI_STATE_COMMAND_POST_WRITE_2;
            mid_cmd = Some((i, distance_code, distance_context, copy_length));
            break 'cmd;
          }
        }
      } else {
        if !memmove16_from_ringbuffer(rb, dst_start_u32, src_start_u32) {
          mark_unlikely();
          result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_DISTANCE;
          break 'cmd;
        }
        pos += i;
        if i > 16 {
          let copy_ok = if i > 32 {
            memcpy_within_slice(rb, dst_start + 16, src_start + 16, (i - 16) as usize)
          } else {
            memmove16_offset16(rb, dst_start_u32, src_start_u32)
          };
          if !copy_ok {
            mark_unlikely();
            result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_DISTANCE;
            break 'cmd;
          }
        }
      }
    }

    if M::SAFE && matches!(state, BrotliRunningState::BROTLI_STATE_COMMAND_POST_WRAP_COPY) {
      let (next_pos, next_i, wrapped) =
        copy_wrap_or_overlapping(rb, rb_size, rb_mask, pos, i, distance_code);
      pos = next_pos;
      i = next_i;
      if wrapped {
        exit_state = BrotliRunningState::BROTLI_STATE_COMMAND_POST_WRITE_2;
        mid_cmd = Some((i, distance_code, distance_context, copy_length));
        break 'cmd;
      }
    }

    if s.meta_block_remaining_len <= 0 {
      mark_unlikely();
      exit_state = BrotliRunningState::BROTLI_STATE_METABLOCK_DONE;
      break 'cmd;
    }
    if M::SAFE {
      state = BrotliRunningState::BROTLI_STATE_COMMAND_BEGIN;
      if single_command {
        break 'cmd;
      }
    }
  }

  if !M::SAFE {
    fbr.sync_to(&mut s.br);
  }
  s.state = exit_state;
  s.pos = pos;
  if let Some((rem_i, d_code, d_ctx, c_len)) = mid_cmd {
    save_mid_command_state(s, rem_i, d_code, d_ctx, c_len);
  }
  result
}

// Kept out of line: inlined into BrotliDecompressStream, the command loop would
// share registers with the whole stream state machine and reload its state
// from the stack.
#[inline(never)]
fn ProcessCommands<AllocU8: alloc::Allocator<u8>,
                   AllocU32: alloc::Allocator<u32>,
                   AllocHC: alloc::Allocator<HuffmanCode>>
  (s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   input: &[u8],
   dictionary: &[u8])
   -> BrotliDecoderErrorCode {
  if (!CheckInputAmount(false, &s.br, 28)) || (!WarmupBitReader(false, &mut s.br, input)) {
    mark_unlikely();
    return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
  }
  if !matches!(s.state, BrotliRunningState::BROTLI_STATE_COMMAND_BEGIN) {
    mark_unlikely();
    let status = SafeProcessCommandsEntry(true, s, input, dictionary);
    if !matches!(status, BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS)
      || !matches!(s.state, BrotliRunningState::BROTLI_STATE_COMMAND_BEGIN)
    {
      return status;
    }
    if (!CheckInputAmount(false, &s.br, 28)) || (!WarmupBitReader(false, &mut s.br, input)) {
      return BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
    }
  }
  ProcessCommandsInternal::<FastCommandLoop, _, _, _>(false, s, input, dictionary)
}

#[inline(never)]
fn SafeProcessCommandsEntry<AllocU8: alloc::Allocator<u8>,
                            AllocU32: alloc::Allocator<u32>,
                            AllocHC: alloc::Allocator<HuffmanCode>>
  (single_command: bool,
   s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   input: &[u8],
   dictionary: &[u8])
   -> BrotliDecoderErrorCode {
  ProcessCommandsInternal::<SafeCommandLoop, _, _, _>(single_command, s, input, dictionary)
}

#[inline(always)]
fn SafeProcessCommands<AllocU8: alloc::Allocator<u8>,
                       AllocU32: alloc::Allocator<u32>,
                       AllocHC: alloc::Allocator<HuffmanCode>>
  (s: &mut BrotliState<AllocU8, AllocU32, AllocHC>,
   input: &[u8],
   dictionary: &[u8])
   -> BrotliDecoderErrorCode {
  SafeProcessCommandsEntry(false, s, input, dictionary)
}

/* Returns the maximum number of distance symbols which can only represent
   distances not exceeding BROTLI_MAX_ALLOWED_DISTANCE. */
pub fn BrotliMaxDistanceSymbol(ndirect: u32, npostfix: u32) -> u32{
  let bound:[u32;kBrotliMaxPostfix + 1] = [0, 4, 12, 28];
  let diff:[u32;kBrotliMaxPostfix + 1] = [73, 126, 228, 424];
  let postfix = 1 << npostfix;
  if (ndirect < bound[npostfix as usize ]) {
    return ndirect + diff[npostfix as usize] + postfix;
  } else if (ndirect > bound[npostfix as usize] + postfix) {
    return ndirect + diff[npostfix as usize];
  } else {
    return bound[npostfix as usize] + diff[npostfix as usize] + postfix;
  }
}

pub fn BrotliDecompressStream<AllocU8: alloc::Allocator<u8>,
                              AllocU32: alloc::Allocator<u32>,
                              AllocHC: alloc::Allocator<HuffmanCode>>
  (available_in: &mut usize,
   input_offset: &mut usize,
   xinput: &[u8],
   available_out: &mut usize,
   output_offset: &mut usize,
   output: &mut [u8],
   total_out: &mut usize,
   s: &mut BrotliState<AllocU8, AllocU32, AllocHC>)
   -> BrotliResult {
  BrotliDecompressStreamWithDictionaryAndMetadata(
      available_in, input_offset, xinput,
      available_out, output_offset, output,
      total_out, &[], None, s)
}

// As BrotliDecompressStream, lending the decoder `dictionary` for this call:
// the dictionary attached with BrotliState::attach_external_dictionary, or an
// empty slice if there is none. Any other size fails with
// BROTLI_DECODER_ERROR_INVALID_ARGUMENTS.
pub fn BrotliDecompressStreamWithDictionary<AllocU8: alloc::Allocator<u8>,
                                            AllocU32: alloc::Allocator<u32>,
                                            AllocHC: alloc::Allocator<HuffmanCode>>
  (available_in: &mut usize,
   input_offset: &mut usize,
   xinput: &[u8],
   available_out: &mut usize,
   output_offset: &mut usize,
   output: &mut [u8],
   total_out: &mut usize,
   dictionary: &[u8],
   s: &mut BrotliState<AllocU8, AllocU32, AllocHC>)
   -> BrotliResult {
  BrotliDecompressStreamWithDictionaryAndMetadata(
      available_in, input_offset, xinput,
      available_out, output_offset, output,
      total_out, dictionary, None, s)
}


// As BrotliDecompressStream, lending the decoder `dictionary` for this call:
// the dictionary attached with BrotliState::attach_external_dictionary, or an
// empty slice if there is none. Also pass in a callback to be called back with any
// metadata metablocks that are encountered.
pub fn BrotliDecompressStreamWithDictionaryAndMetadata<AllocU8: alloc::Allocator<u8>,
                                                       AllocU32: alloc::Allocator<u32>,
                                                       AllocHC: alloc::Allocator<HuffmanCode>>
  (available_in: &mut usize,
   input_offset: &mut usize,
   xinput: &[u8],
   mut available_out: &mut usize,
   mut output_offset: &mut usize,
   mut output: &mut [u8],
   mut total_out: &mut usize,
   dictionary: &[u8],
   mut metadata_cb: Option<&mut dyn state::BrotliMetadataCallback>,
   mut s: &mut BrotliState<AllocU8, AllocU32, AllocHC>)
   -> BrotliResult {

  let mut result = BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;

  let mut saved_buffer: [u8; 8] = s.buffer;
  let mut local_input: &[u8];
  if is_fatal(s.error_code) {
    return BrotliResult::ResultFailure;
  }
  if !bit_reader::is_valid_input_range(*input_offset, *available_in, xinput.len()) {
    return SaveErrorCode!(s, BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS);
  }
  if dictionary.len() != s.external_dictionary_len() {
    return SaveErrorCode!(s, BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS);
  }
  match output_offset.checked_add(*available_out) {
    Some(end) if end <= output.len() => {}
    _ => return SaveErrorCode!(s, BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS),
  }
  if s.buffer_length as usize > s.buffer.len() {
    return SaveErrorCode!(s, BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE);
  }
  if s.buffer_length == 0 {
    local_input = xinput;
    s.br.avail_in = *available_in as u32;
    s.br.next_in = *input_offset as u32;
    bit_reader::BrotliBitReaderClearUnclaimed(&mut s.br);
  } else {
    result = BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
    let copy_len = core::cmp::min(saved_buffer.len() - s.buffer_length as usize, *available_in);
    if copy_len > 0 {
      fast_mut!((saved_buffer)[s.buffer_length as usize ; (s.buffer_length as usize + copy_len)])
        .clone_from_slice(fast!((xinput)[*input_offset ; copy_len + *input_offset]));
      fast_mut!((s.buffer)[s.buffer_length as usize ; (s.buffer_length as usize + copy_len)])
        .clone_from_slice(fast!((xinput)[*input_offset ; copy_len + *input_offset]));
    }
    local_input = &saved_buffer[..];
    s.br.next_in = 0;
    bit_reader::BrotliBitReaderClearUnclaimed(&mut s.br);
  }
  loop {
    match result {
      BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
      _ => {
        match result {
          BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT => {
            if s.ringbuffer.slice().len() != 0 {
              let (intermediate_result, _) = WriteRingBuffer(available_out,
                                                             Some(&mut output),
                                                             &mut output_offset,
                                                             &mut total_out,
                                                             true,
                                                             &mut s);
              if is_fatal(intermediate_result) {
                result = intermediate_result;
                break;
              }
            }
            if s.buffer_length != 0 {
              // Used with internal buffer.
              if s.br.avail_in == 0 {
                // Successfully finished read transaction.
                // Accamulator contains less than 8 bits, because internal buffer
                // is expanded byte-by-byte until it is enough to complete read.
                s.buffer_length = 0;
                // Switch to input stream and restart.
                result = BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
                local_input = xinput;
                s.br.avail_in = *available_in as u32;
                s.br.next_in = *input_offset as u32;
                bit_reader::BrotliBitReaderClearUnclaimed(&mut s.br);
                continue;
              } else if *available_in != 0 {
                // Not enough data in buffer, but can take one more byte from
                // input stream.
                result = BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
                let new_byte = fast!((xinput)[*input_offset]);
                let buffer_length = s.buffer_length as usize;
                // This byte was copied into saved_buffer at function entry.
                if saved_buffer.get(buffer_length) != Some(&new_byte) {
                  result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE;
                  break;
                }
                s.buffer[buffer_length] = new_byte;
                s.buffer_length += 1;
                s.br.avail_in = s.buffer_length;
                (*input_offset) += 1;
                (*available_in) -= 1;
                // Retry with more data in buffer.
                // we can't re-borrow the saved buffer...so we have to do this recursively
                continue;
              }
              // Can't finish reading and no more input.

              // FIXME :: NOT SURE WHAT THIS MEANT
              // saved_buffer = core::mem::replace(
              //  &mut s.br.input_,
              //  &mut[]); // clear input
              break;
            } else {
              // Input stream doesn't contain enough input.
              // Copy tail to internal buffer and return.
              *input_offset = s.br.next_in as usize;
              *available_in = s.br.avail_in as usize;
              let buffer_length = s.buffer_length as usize;
              let input_end = *input_offset as u64 + *available_in as u64;
              let buffer_end = buffer_length as u64 + *available_in as u64;
              if input_end > xinput.len() as u64 || buffer_end > s.buffer.len() as u64 {
                result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE;
                break;
              }
              let (input_end, buffer_end) = (input_end as usize, buffer_end as usize);
              s.buffer[buffer_length..buffer_end]
                .clone_from_slice(&xinput[*input_offset..input_end]);
              s.buffer_length = buffer_end as u32;
              *input_offset = input_end;
              *available_in = 0;
              break;
            }
            // unreachable!(); <- dead code
          }
          _ => {
            // Fail or needs more output.
            if s.buffer_length != 0 {
              // Just consumed the buffered input and produced some output. Otherwise
              // it would result in "needs more input". Reset internal buffer.
              s.buffer_length = 0;
            } else {
              // Using input stream in last iteration. When decoder switches to input
              // stream it has less than 8 bits in accamulator, so it is safe to
              // return unused accamulator bits there.
              bit_reader::BrotliBitReaderUnload(&mut s.br);
              *available_in = s.br.avail_in as usize;
              *input_offset = s.br.next_in as usize;
            }
          }
        }
        break;
      }
    }
    if !bit_reader::is_valid_bit_reader(&s.br, local_input) {
      result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE;
      continue;
    }
    loop {
      // this emulates fallthrough behavior
      match s.state {
        BrotliRunningState::BROTLI_STATE_UNINITED => {
          // Prepare to the first read.
          if (!bit_reader::BrotliWarmupBitReader(&mut s.br, local_input)) {
            result = BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
            break;
          }
          // Decode window size.
          /* Reads 1..8 bits. */
          result = DecodeWindowBits(&mut s.large_window, &mut s.window_bits, &mut s.br);
          match result {
            BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
            _ => break,
          }
          if s.large_window {
              s.state = BrotliRunningState::BROTLI_STATE_LARGE_WINDOW_BITS;
          } else {
              s.state = BrotliRunningState::BROTLI_STATE_INITIALIZE;
          }
        }
        BrotliRunningState::BROTLI_STATE_LARGE_WINDOW_BITS => {
          if (!bit_reader::BrotliSafeReadBits(&mut s.br, 6, &mut s.window_bits, local_input)) {
            result = BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
            break;
          }
          if (s.window_bits < kBrotliLargeMinWbits ||
              s.window_bits > kBrotliLargeMaxWbits) {
            result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS;
            break;
          }
          s.state = BrotliRunningState::BROTLI_STATE_INITIALIZE;
        }
        BrotliRunningState::BROTLI_STATE_INITIALIZE => {
          s.max_backward_distance = (1 << s.window_bits) - kBrotliWindowGap as i32;
          // A dictionary passed at construction time becomes a compound
          // dictionary chunk; it is kept in its own buffer rather than
          // prepended to the ring buffer so that it remains addressable
          // after the ring buffer wraps.
          if s.custom_dict.slice().len() != 0 {
            let dict = mem::replace(&mut s.custom_dict, AllocU8::AllocatedMemory::default());
            if !s.attach_compound_dictionary_chunk(state::MaybeOwnedSlice::Owned(dict)) {
              result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_COMPOUND_DICTIONARY;
              break;
            }
          }

          // block_type_trees and block_len_trees are allocated when first
          // needed; see EnsureBlockTrees.

          s.state = BrotliRunningState::BROTLI_STATE_METABLOCK_BEGIN;
          // No break, continue to next state
        }
        BrotliRunningState::BROTLI_STATE_METABLOCK_BEGIN => {
          s.BrotliStateMetablockBegin();
          BROTLI_LOG_UINT!(s.pos);
          s.state = BrotliRunningState::BROTLI_STATE_METABLOCK_HEADER;
          // No break, continue to next state
        }
        BrotliRunningState::BROTLI_STATE_METABLOCK_HEADER => {
          result = DecodeMetaBlockLength(&mut s, local_input); // Reads 2 - 31 bits.
          match result {
            BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
            _ => break,
          }
          debug_assert!(s.meta_block_remaining_len <= 1 << 24);
          BROTLI_LOG_UINT!(s.is_last_metablock);
          BROTLI_LOG_UINT!(s.meta_block_remaining_len);
          BROTLI_LOG_UINT!(s.is_metadata);
          BROTLI_LOG_UINT!(s.is_uncompressed);
          if (s.is_metadata != 0 || s.is_uncompressed != 0) &&
             !bit_reader::BrotliJumpToByteBoundary(&mut s.br) {
            result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_PADDING_1;
            break;
          }
          if s.is_metadata != 0 {
            s.state = BrotliRunningState::BROTLI_STATE_METADATA;

            if let Some(ref mut callback) = metadata_cb {
              callback.start(s.meta_block_remaining_len as usize);
            }
            break;
          }
          if s.meta_block_remaining_len == 0 {
            s.state = BrotliRunningState::BROTLI_STATE_METABLOCK_DONE;
            break;
          }
          BrotliCalculateRingBufferSize(s, local_input);
          if s.is_uncompressed != 0 {
            s.state = BrotliRunningState::BROTLI_STATE_UNCOMPRESSED;
            break;
          }
          s.loop_counter = 0;
          s.state = BrotliRunningState::BROTLI_STATE_HUFFMAN_CODE_0;
          break;
        }
        BrotliRunningState::BROTLI_STATE_UNCOMPRESSED => {
          let mut _bytes_copied = s.meta_block_remaining_len;
          result = CopyUncompressedBlockToOutput(&mut available_out,
                                                 &mut output,
                                                 &mut output_offset,
                                                 &mut total_out,
                                                 &mut s,
                                                 local_input);
          _bytes_copied -= s.meta_block_remaining_len;
          match result {
            BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
            _ => break,
          }
          s.state = BrotliRunningState::BROTLI_STATE_METABLOCK_DONE;
          break;
        }
        BrotliRunningState::BROTLI_STATE_METADATA => {
          result = read_metadata_block(
              &mut s.br,
              &mut s.meta_block_remaining_len,
              local_input,
              &mut metadata_cb);
          if let BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS = result {
            s.state = BrotliRunningState::BROTLI_STATE_METABLOCK_DONE
          }
          break;
        }
        BrotliRunningState::BROTLI_STATE_HUFFMAN_CODE_0 => {
          if s.loop_counter >= 3 {
            s.state = BrotliRunningState::BROTLI_STATE_METABLOCK_HEADER_2;
            break;
          }
          // Reads 1..11 bits.
          {
            let index = s.loop_counter as usize;
            result =
              DecodeVarLenUint8(&mut s.substate_decode_uint8,
                                &mut s.br,
                                &mut fast_mut!((s.block_type_length_state.num_block_types)[index]),
                                local_input);
          }
          match result {
            BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
            _ => break,
          }
          fast_mut!((s.block_type_length_state.num_block_types)[s.loop_counter as usize]) += 1;
          BROTLI_LOG_UINT!(s.block_type_length_state.num_block_types[s.loop_counter as usize]);
          if fast!((s.block_type_length_state.num_block_types)[s.loop_counter as usize]) < 2 {
            s.loop_counter += 1;
            break;
          }
          s.state = BrotliRunningState::BROTLI_STATE_HUFFMAN_CODE_1;
          // No break, continue to next state
        }
        BrotliRunningState::BROTLI_STATE_HUFFMAN_CODE_1 => {
          // Only a metablock with more than one block type of some kind needs
          // the block type and block length trees, so a stream without block
          // switches, as most short ones are, may never allocate them.
          if !EnsureBlockTrees(&mut s) {
            result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_BLOCK_TYPE_TREES;
            break;
          }
          let tree_offset = s.loop_counter as u32 * huffman::BROTLI_HUFFMAN_MAX_SIZE_258 as u32;
          let mut new_huffman_table = mem::replace(&mut s.block_type_length_state.block_type_trees,
                                                   AllocHC::AllocatedMemory::default());
          let loop_counter = s.loop_counter as usize;
          let alphabet_size = fast!((s.block_type_length_state.num_block_types)[loop_counter]) + 2;
          result =
            ReadHuffmanCode(alphabet_size, alphabet_size,
                            new_huffman_table.slice_mut(),
                            tree_offset as usize,
                            None,
                            &mut s,
                            local_input);
          let _ = mem::replace(&mut s.block_type_length_state.block_type_trees,
                       new_huffman_table);
          match result {
            BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
            _ => break,
          }
          s.state = BrotliRunningState::BROTLI_STATE_HUFFMAN_CODE_2;
          // No break, continue to next state
        }
        BrotliRunningState::BROTLI_STATE_HUFFMAN_CODE_2 => {
          let tree_offset = s.loop_counter * huffman::BROTLI_HUFFMAN_MAX_SIZE_26 as i32;
          let mut new_huffman_table = mem::replace(&mut s.block_type_length_state.block_len_trees,
                                                   AllocHC::AllocatedMemory::default());
          result = ReadHuffmanCode(kNumBlockLengthCodes, kNumBlockLengthCodes,
                                   new_huffman_table.slice_mut(),
                                   tree_offset as usize,
                                   None,
                                   &mut s,
                                   local_input);
          let _ = mem::replace(&mut s.block_type_length_state.block_len_trees,
                       new_huffman_table);
          match result {
            BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
            _ => break,
          }
          s.state = BrotliRunningState::BROTLI_STATE_HUFFMAN_CODE_3;
          // No break, continue to next state
        }
        BrotliRunningState::BROTLI_STATE_HUFFMAN_CODE_3 => {
          let tree_offset = s.loop_counter * huffman::BROTLI_HUFFMAN_MAX_SIZE_26 as i32;

          let mut block_length_out: u32 = 0;
          let ind_ret: (bool, u32);
          
          ind_ret = SafeReadBlockLengthIndex(&s.block_type_length_state.substate_read_block_length,
                                             s.block_type_length_state.block_length_index,
                                             fast_slice!((s.block_type_length_state.block_len_trees)
                                                           [tree_offset as usize;]),
                                             &mut s.br, local_input);

          if !SafeReadBlockLengthFromIndex(&mut s.block_type_length_state,
                                           &mut s.br,
                                           &mut block_length_out,
                                           ind_ret,
                                           local_input) {
            result = BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
            break;
          }
          fast_mut!((s.block_type_length_state.block_length)[s.loop_counter as usize]) =
            block_length_out;
          BROTLI_LOG_UINT!(s.block_type_length_state.block_length[s.loop_counter as usize]);
          s.loop_counter += 1;
          s.state = BrotliRunningState::BROTLI_STATE_HUFFMAN_CODE_0;
          break;
        }
        BrotliRunningState::BROTLI_STATE_METABLOCK_HEADER_2 => {
          let mut bits: u32 = 0;
          if (!bit_reader::BrotliSafeReadBits(&mut s.br, 6, &mut bits, local_input)) {
            result = BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT;
            break;
          }
          s.distance_postfix_bits = bits & bit_reader::BitMask(2);
          bits >>= 2;
          s.num_direct_distance_codes = NUM_DISTANCE_SHORT_CODES +
                                        (bits << s.distance_postfix_bits);
          BROTLI_LOG_UINT!(s.num_direct_distance_codes);
          BROTLI_LOG_UINT!(s.distance_postfix_bits);
          s.distance_postfix_mask = bit_reader::BitMask(s.distance_postfix_bits) as i32;
          let num_literal_block_types =
            fast!((s.block_type_length_state.num_block_types)[0]) as usize;
          if s.context_modes.slice().len() < num_literal_block_types {
            s.alloc_u8.free_cell(core::mem::take(&mut s.context_modes));
            s.context_modes = s.alloc_u8.alloc_cell(num_literal_block_types);
            if s.context_modes.slice().len() < num_literal_block_types {
              result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODES;
              break;
            }
          }
          s.loop_counter = 0;
          s.state = BrotliRunningState::BROTLI_STATE_CONTEXT_MODES;
          // No break, continue to next state
        }
        BrotliRunningState::BROTLI_STATE_CONTEXT_MODES => {
          result = ReadContextModes(&mut s, local_input);
          match result {
            BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
            _ => break,
          }
          s.state = BrotliRunningState::BROTLI_STATE_CONTEXT_MAP_1;
          // No break, continue to next state
        }
        BrotliRunningState::BROTLI_STATE_CONTEXT_MAP_1 => {
          result =
            DecodeContextMap((fast!((s.block_type_length_state.num_block_types)[0]) as usize) <<
                             kLiteralContextBits as usize,
                             false,
                             &mut s,
                             local_input);
          match result {
            BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
            _ => break,
          }
          DetectTrivialLiteralBlockTypes(s);
          s.state = BrotliRunningState::BROTLI_STATE_CONTEXT_MAP_2;
          // No break, continue to next state
        }
        BrotliRunningState::BROTLI_STATE_CONTEXT_MAP_2 => {
            let num_direct_codes =
              s.num_direct_distance_codes - NUM_DISTANCE_SHORT_CODES;
            let num_distance_codes = BROTLI_DISTANCE_ALPHABET_SIZE(
              s.distance_postfix_bits, num_direct_codes,
                (if s.large_window { BROTLI_LARGE_MAX_DISTANCE_BITS } else {
                    BROTLI_MAX_DISTANCE_BITS}));
            let max_distance_symbol = if s.large_window {
                BrotliMaxDistanceSymbol(
                    num_direct_codes, s.distance_postfix_bits)
            } else {
                num_distance_codes
            };
            result =
              DecodeContextMap((fast!((s.block_type_length_state.num_block_types)[2]) as usize) <<
                               kDistanceContextBits as usize,
                               true,
                               s,
                               local_input);
            match result {
              BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
              _ => break,
            }
            let num_command_htrees =
              fast!((s.block_type_length_state.num_block_types)[1]) as u16;
            // Growing a tree group frees its old `codes` buffer. Block trees
            // must not be allocated after a free (see `EnsureBlockTrees`), so
            // allocate them first when a group from an earlier metablock needs
            // a larger `codes` buffer.
            if (s.literal_hgroup.needs_codes_growth(
                  kNumLiteralCodes, kNumLiteralCodes, s.num_literal_htrees as u16) ||
                s.insert_copy_hgroup.needs_codes_growth(
                  kNumInsertAndCopyCodes, kNumInsertAndCopyCodes, num_command_htrees) ||
                s.distance_hgroup.needs_codes_growth(
                  num_distance_codes as u16, max_distance_symbol as u16, s.num_dist_htrees as u16)) &&
               !EnsureBlockTrees(s) {
              return SaveErrorCode!(
                s, BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_BLOCK_TYPE_TREES);
            }
            s.literal_hgroup.init(&mut s.alloc_u32,
                                  &mut s.alloc_hc,
                                  kNumLiteralCodes,
                                  kNumLiteralCodes,
                                  s.num_literal_htrees as u16);
            s.insert_copy_hgroup.init(&mut s.alloc_u32,
                                      &mut s.alloc_hc,
                                      kNumInsertAndCopyCodes,
                                      kNumInsertAndCopyCodes,
                                      num_command_htrees);
            s.distance_hgroup.init(&mut s.alloc_u32,
                                   &mut s.alloc_hc,
                                   num_distance_codes as u16,
                                   max_distance_symbol as u16,
                                   s.num_dist_htrees as u16);
            if (s.literal_hgroup.htrees.slice().len() == 0 ||
                s.literal_hgroup.codes.slice().len() == 0 ||
                s.insert_copy_hgroup.htrees.slice().len() == 0 ||
                s.insert_copy_hgroup.codes.slice().len() == 0 ||
                s.distance_hgroup.htrees.slice().len() == 0 ||
                s.distance_hgroup.codes.slice().len() == 0) {
              return SaveErrorCode!(
                s, BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_TREE_GROUPS);
            }

          /*{
            let num_distance_codes: u32 = s.num_direct_distance_codes +
                                          (48u32 << s.distance_postfix_bits);
            result =
              DecodeContextMap((fast!((s.block_type_length_state.num_block_types)[2]) as usize) <<
                               kDistanceContextBits as usize,
                               true,
                               s,
                               local_input);
            match result {
              BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
              _ => break,
            }
            s.literal_hgroup.init(&mut s.alloc_u32,
                                  &mut s.alloc_hc,
                                  kNumLiteralCodes,
                                  s.num_literal_htrees as u16);
            s.insert_copy_hgroup.init(&mut s.alloc_u32,
                                      &mut s.alloc_hc,
                                      kNumInsertAndCopyCodes,
                                      fast!((s.block_type_length_state.num_block_types)[1]) as u16);
            s.distance_hgroup.init(&mut s.alloc_u32,
                                   &mut s.alloc_hc,
                                   num_distance_codes as u16,
                                   s.num_dist_htrees as u16);
            if (s.literal_hgroup.codes.slice().len() == 0 ||
                s.insert_copy_hgroup.codes.slice().len() == 0 ||
                s.distance_hgroup.codes.slice().len() == 0) {
              return SaveErrorCode!(s, BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_TREE_GROUPS);
            }
          }*/
          s.loop_counter = 0;
          s.state = BrotliRunningState::BROTLI_STATE_TREE_GROUP;
          // No break, continue to next state
        }
        BrotliRunningState::BROTLI_STATE_TREE_GROUP => {
          result = HuffmanTreeGroupDecode(s.loop_counter, &mut s, local_input);
          match result {
            BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
            _ => break,
          }
          s.loop_counter += 1;
          if (s.loop_counter >= 3) {
            PrepareLiteralDecoding(s);
            s.dist_context_map_slice_index = 0;
              /*
            s.context_map_slice_index = 0;
            let context_mode_index = fast!((s.block_type_length_state.block_type_rb)[1]);
            let context_mode = fast_slice!((s.context_modes)[context_mode_index as usize]);
            s.context_lookup = &kContextLookup[context_mode as usize & 3];
               */
            s.htree_command_index = 0;
            // look it up each time s.literal_htree=s.literal_hgroup.htrees[s.literal_htree_index];
            // The header is fully decoded: size the ring buffer for the body.
            result = BrotliEnsureRingBuffer(
              s, BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_2);
            match result {
              BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
              _ => break,
            }
            s.state = BrotliRunningState::BROTLI_STATE_COMMAND_BEGIN;
          }
          break;
        }
        BrotliRunningState::BROTLI_STATE_COMMAND_BEGIN |
        BrotliRunningState::BROTLI_STATE_COMMAND_INNER |
        BrotliRunningState::BROTLI_STATE_COMMAND_POST_DECODE_LITERALS |
        BrotliRunningState::BROTLI_STATE_COMMAND_POST_WRAP_COPY => {
          result = ProcessCommands(s, local_input, dictionary);
          if let BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT = result {
            result = SafeProcessCommands(s, local_input, dictionary)
          }
          break;
        }
        BrotliRunningState::BROTLI_STATE_COMMAND_INNER_WRITE |
        BrotliRunningState::BROTLI_STATE_COMMAND_POST_WRITE_1 |
        BrotliRunningState::BROTLI_STATE_COMMAND_POST_WRITE_2 => {
          let (xresult, _) = WriteRingBuffer(&mut available_out,
                                             Some(&mut output),
                                             &mut output_offset,
                                             &mut total_out,
                                             false,
                                             &mut s);
          result = xresult;
          match result {
            BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
            _ => break,
          }
          if !WrapRingBuffer(s) {
            result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE;
            break;
          }
          if s.ringbuffer_size == 1 << s.window_bits {
            s.max_distance = s.max_backward_distance;
          }
          match s.state {
            BrotliRunningState::BROTLI_STATE_COMMAND_POST_WRITE_1 => {
              if s.compound_dictionary.br_length != s.compound_dictionary.br_copied {
                // Resume an interrupted copy from the compound dictionary.
                let pos = s.pos;
                let rb_size = s.ringbuffer_size;
                let copied = CopyFromCompoundDictionary(
                  &mut s.compound_dictionary,
                  s.ringbuffer.slice_mut(),
                  rb_size,
                  pos,
                  dictionary,
                );
                s.pos += copied;
                if s.pos >= s.ringbuffer_size {
                  // Ring buffer is full again; flush it and continue copying.
                  continue;
                }
              }
              if (s.meta_block_remaining_len <= 0) {
                // Next metablock, if any
                s.state = BrotliRunningState::BROTLI_STATE_METABLOCK_DONE;
              } else {
                s.state = BrotliRunningState::BROTLI_STATE_COMMAND_BEGIN;
              }
              break;
            }
            BrotliRunningState::BROTLI_STATE_COMMAND_POST_WRITE_2 => {
              s.state = BrotliRunningState::BROTLI_STATE_COMMAND_POST_WRAP_COPY;
            }
            _ => {
              // BROTLI_STATE_COMMAND_INNER_WRITE
              if (s.loop_counter == 0) {
                if (s.meta_block_remaining_len <= 0) {
                  s.state = BrotliRunningState::BROTLI_STATE_METABLOCK_DONE;
                } else {
                  s.state = BrotliRunningState::BROTLI_STATE_COMMAND_POST_DECODE_LITERALS;
                }
                break;
              }
              s.state = BrotliRunningState::BROTLI_STATE_COMMAND_INNER;
            }
          }
          break;
        }
        BrotliRunningState::BROTLI_STATE_METABLOCK_DONE => {
          // RFC 7932 §9.3: "if the number of literals to insert, the copy
          // length, or the resulting dictionary word length would cause MLEN to
          // be exceeded, then the stream should be rejected as invalid." When a
          // command overshoots the declared meta-block length, the command loop
          // exits to this state with meta_block_remaining_len < 0. The only
          // negative-length guard was in WriteRingBuffer, which is not reached
          // when the whole output fits in the ring buffer before the final
          // flush, so such malformed streams decoded as success. The C
          // reference and andybalholm/brotli (Go) reject them as BLOCK_LENGTH_2;
          // mirror that check here, before cleanup.
          if (s.meta_block_remaining_len < 0) {
            result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_2;
            break;
          }
          if (s.is_last_metablock == 0) {
            // Keep `context_modes`, `context_map`, `dist_context_map`, and the
            // Huffman tree groups allocated so the next metablock can reuse
            // their buffers when they are large enough.
            s.state = BrotliRunningState::BROTLI_STATE_METABLOCK_BEGIN;
            break;
          }
          s.BrotliStateCleanupAfterMetablock();
          if (!bit_reader::BrotliJumpToByteBoundary(&mut s.br)) {
            // RFC 7932 §9.2: padding bits at the end of the final metablock
            // must be zero. Without `break`, control falls through to
            // BROTLI_STATE_DONE which calls WriteRingBuffer and unconditionally
            // overwrites `result` with that call's return value, silently
            // dropping the padding error.
            result = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_PADDING_2;
            break;
          }
          if (s.buffer_length == 0) {
            bit_reader::BrotliBitReaderUnload(&mut s.br);
            *available_in = s.br.avail_in as usize;
            *input_offset = s.br.next_in as usize;
          }
          s.state = BrotliRunningState::BROTLI_STATE_DONE;
          // No break, continue to next state
        }
        BrotliRunningState::BROTLI_STATE_DONE => {
          if (s.ringbuffer.slice().len() != 0) {
            let (xresult, _) = WriteRingBuffer(&mut available_out,
                                               Some(&mut output),
                                               &mut output_offset,
                                               &mut total_out,
                                               true,
                                               &mut s);
            result = xresult;
            match result {
              BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => {}
              _ => break,
            }
          }
          return SaveErrorCode!(s, result);
        }
      }
    }
  }

  SaveErrorCode!(s, result)
}

fn read_metadata_block(
    br: &mut bit_reader::BrotliBitReader,
    meta_block_remaining_len: &mut i32,
    local_input: &[u8],
    metadata_cb: &mut Option<&mut dyn state::BrotliMetadataCallback>,
) -> BrotliDecoderErrorCode {
    if *meta_block_remaining_len <= 0 {
        return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
    }

    let bit_reader_bits_avail = bit_reader::BrotliGetAvailableBits(br);
    debug_assert_eq!(bit_reader_bits_avail & 7, 0);
    let amt_in_bit_reader = core::cmp::min(
        (bit_reader_bits_avail / 8) as usize,
        *meta_block_remaining_len as usize);

    if amt_in_bit_reader > 0 {
        let mut scratch = [0u8; core::mem::size_of::<bit_reader::BrotliBitReader>()];
        bit_reader::BrotliCopyBytes(&mut scratch[..], br, amt_in_bit_reader as u32, local_input);
        *meta_block_remaining_len -= amt_in_bit_reader as i32;
        if let Some(callback) = metadata_cb {
            callback.chunk(&scratch[..amt_in_bit_reader]);
        }
    }
    let remaining_bytes = core::cmp::min(*meta_block_remaining_len as usize,
                                         br.avail_in as usize);
    if remaining_bytes != 0 {
        if let Some(callback) = metadata_cb {
            let all_remaining_input = local_input.split_at(br.next_in as usize).1;
            callback.chunk(&all_remaining_input.split_at(remaining_bytes).0);
        }
        br.next_in += remaining_bytes as u32;
        bit_reader::BrotliBitReaderClearUnclaimed(br);
        br.avail_in -= remaining_bytes as u32;
        *meta_block_remaining_len -= remaining_bytes as i32;
    }
    if *meta_block_remaining_len == 0 {
        return BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS;
    }
    BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT
}

#[cfg(test)]
mod safeguard_tests {
  use super::{copy_overlapping_backreference, memcpy_within_slice, memmove16_from_ringbuffer,
              memmove16_offset16};

  // ProcessCommandsInternal indexes the 64 literal trees of a block type with
  // `context & 63` for context = lookup[p1] | lookup[p2 + 256]: the mask
  // changes nothing only while every entry is below 64.
  #[test]
  fn context_lookup_values_fit_the_context_map() {
    for table in super::kContextLookup.iter() {
      for value in table.iter() {
        assert!(*value < 64);
      }
    }
  }

  #[test]
  fn overlapping_backreferences_match_sequential_copy() {
    for distance in 1..65 {
      for size in 0..257 {
        let mut data = [0u8; 384];
        for (index, byte) in data.iter_mut().enumerate() {
          *byte = (index.wrapping_mul(73) ^ (index >> 3)) as u8;
        }
        let mut expected = data;
        for index in 64..64 + size {
          expected[index] = expected[index - distance];
        }
        assert!(copy_overlapping_backreference(&mut data, 64, distance, size));
        assert_eq!(&data[..], &expected[..]);
      }
    }
  }

  #[test]
  fn overlapping_backreferences_reject_invalid_ranges() {
    let original = [37u8; 32];
    for &(dst, distance, size) in &[
        (8, 0, 4), (8, 9, 4), (33, 1, 0), (31, 1, 2),
        (usize::MAX, 1, 0), (8, usize::MAX, 1), (8, 1, usize::MAX)] {
      let mut data = original;
      assert!(!copy_overlapping_backreference(&mut data, dst, distance, size));
      assert_eq!(data, original);
    }
  }

  #[test]
  fn memmove16_helpers_reject_out_of_bounds_ranges() {
    // Both require 32 readable and writable bytes from each offset.
    let mut short = [0u8; 31];
    assert!(!memmove16_from_ringbuffer(&mut short, 0, 0));
    assert!(!memmove16_offset16(&mut short, 0, 0));
    let mut data = [7u8; 64];
    for &(dst, src) in &[(33u32, 0u32), (0, 33), (u32::MAX, 0), (0, u32::MAX)] {
      assert!(!memmove16_from_ringbuffer(&mut data, dst, src));
      assert!(!memmove16_offset16(&mut data, dst, src));
    }
    assert_eq!(data, [7u8; 64]);
    // The last offsets that fit.
    assert!(memmove16_from_ringbuffer(&mut data, 32, 0));
    assert!(memmove16_offset16(&mut data, 0, 32));
  }

  #[test]
  fn memmove16_helpers_copy_with_memmove_semantics() {
    let mut pattern = [0u8; 80];
    for (i, b) in pattern.iter_mut().enumerate() {
      *b = i as u8;
    }
    // Disjoint and overlapping ranges in both directions, including the
    // distances below 16 that LZ77 copies hit.
    for &(dst, src) in &[(40u32, 0u32), (0, 40), (20, 16), (16, 20), (17, 16), (5, 5)] {
      let mut data = pattern;
      let mut expected = pattern;
      expected.copy_within(src as usize..src as usize + 16, dst as usize);
      assert!(memmove16_from_ringbuffer(&mut data, dst, src));
      assert_eq!(&data[..], &expected[..], "memmove16_from_ringbuffer({}, {})", dst, src);

      let mut data = pattern;
      let mut expected = pattern;
      expected.copy_within(src as usize + 16..src as usize + 32, dst as usize + 16);
      assert!(memmove16_offset16(&mut data, dst, src));
      assert_eq!(&data[..], &expected[..], "memmove16_offset16({}, {})", dst, src);
    }
  }

  #[test]
  fn memcpy_within_slice_validates_nonoverlapping_ranges() {
    let mut data = [0u8, 1, 2, 3, 4, 5, 6, 7];
    assert!(memcpy_within_slice(&mut data, 4, 0, 2));
    assert_eq!(data, [0, 1, 2, 3, 0, 1, 6, 7]);

    let unchanged = data;
    assert!(!memcpy_within_slice(&mut data, 1, 0, 4));
    assert!(!memcpy_within_slice(&mut data, 7, 0, 2));
    assert!(!memcpy_within_slice(&mut data, usize::MAX, 0, 2));
    assert!(!memcpy_within_slice(&mut data, 0, usize::MAX, 2));
    assert_eq!(data, unchanged);
  }
}

// These need a live BrotliState, so they need an allocator: StandardAlloc is
// std-only, as in the `tests` module above.
#[cfg(all(test, feature="std"))]
mod state_guard_tests {
  use super::{CheckRingBufferConsistency, DecodeContextMap, ReadHuffmanCode, UnwrittenBytes,
              WrapRingBuffer, WriteRingBuffer};
  use super::{decode_block_type_and_length_slice, DecodeBlockTypeAndLength};
  use bit_reader::BitReaderSlice;
  use super::{BrotliDecoderErrorCode, BrotliRunningState, BrotliState, HuffmanCode};
  use ::state::BrotliRunningHuffmanState;
  use ::alloc::Allocator;
  use ::StandardAlloc;

  type TestState = BrotliState<StandardAlloc, StandardAlloc, StandardAlloc>;

  fn state() -> TestState {
    BrotliState::new(StandardAlloc::default(), StandardAlloc::default(), StandardAlloc::default())
  }

  // A decoder whose ring buffer is consistent: size is a power of two, mask is
  // size - 1, and the allocation is at least that long.
  fn state_with_ringbuffer(size: i32, window_bits: u32) -> TestState {
    let mut s = state();
    s.ringbuffer = s.alloc_u8.alloc_cell(size as usize);
    s.ringbuffer_size = size;
    s.ringbuffer_mask = size - 1;
    s.new_ringbuffer_size = size;
    s.window_bits = window_bits;
    s
  }

  // None of the guards below is reachable through BrotliDecompressStream, so
  // each is driven directly from an inconsistent decoder state. They exist
  // because the `unsafe` build turns an out-of-range index into an out-of-bounds
  // access rather than a panic.

  #[test]
  fn ring_buffer_consistency_rejects_inconsistent_state() {
    let mut s = state_with_ringbuffer(16, 4);
    assert!(CheckRingBufferConsistency(&s));

    s.pos = -1;
    assert!(!CheckRingBufferConsistency(&s));
    s.pos = 0;

    s.ringbuffer_size = 0;
    assert!(!CheckRingBufferConsistency(&s));
    s.ringbuffer_size = 16;

    s.ringbuffer_mask = -1;
    assert!(!CheckRingBufferConsistency(&s));

    // mask that does not match the size
    s.ringbuffer_mask = 7;
    assert!(!CheckRingBufferConsistency(&s));
    s.ringbuffer_mask = 15;

    // size larger than the allocation
    s.ringbuffer_size = 32;
    s.ringbuffer_mask = 31;
    assert!(!CheckRingBufferConsistency(&s));
  }

  #[test]
  fn unwritten_bytes_is_exact_past_a_32_bit_wrap() {
    // rb_roundtrips * ringbuffer_size alone overflows a 32-bit usize, so this
    // only stays exact because the counters are u64.
    let mut s = state_with_ringbuffer(1 << 16, 16);
    s.rb_roundtrips = 1 << 17; // 2^17 * 2^16 == 2^33 bytes decoded
    s.pos = 100;
    s.partial_pos_out = (1u64 << 33) + 40;
    assert_eq!(UnwrittenBytes(&s, false), 60);
  }

  #[test]
  fn write_ring_buffer_rejects_inconsistent_ring_buffer() {
    let mut s = state_with_ringbuffer(16, 4);
    s.ringbuffer_mask = 7; // no longer size - 1
    let mut available_out = 8usize;
    let mut output = [0u8; 8];
    let mut output_offset = 0usize;
    let mut total_out = 0usize;
    let (code, out) = WriteRingBuffer(&mut available_out, Some(&mut output[..]),
                                      &mut output_offset, &mut total_out, false, &mut s);
    match code {
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE => {}
      other => panic!("expected UNREACHABLE, got {:?}", other),
    }
    assert!(out.is_empty());
  }

  #[test]
  fn write_ring_buffer_rejects_negative_block_length() {
    let mut s = state_with_ringbuffer(16, 4);
    s.meta_block_remaining_len = -1;
    let mut available_out = 0usize;
    let mut output_offset = 0usize;
    let mut total_out = 0usize;
    let (code, _) = WriteRingBuffer(&mut available_out, None, &mut output_offset,
                                    &mut total_out, false, &mut s);
    match code {
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_1 => {}
      other => panic!("expected BLOCK_LENGTH_1, got {:?}", other),
    }
  }

  #[test]
  fn write_ring_buffer_rejects_output_slice_that_cannot_hold_the_write() {
    let mut s = state_with_ringbuffer(16, 4);
    s.pos = 8; // 8 bytes pending
    let mut available_out = 8usize;
    let mut output = [0u8; 8];
    // output_offset past the end of the caller's buffer: available_out claims
    // room the slice does not have.
    let mut output_offset = 4usize;
    let mut total_out = 0usize;
    let (code, _) = WriteRingBuffer(&mut available_out, Some(&mut output[..]),
                                    &mut output_offset, &mut total_out, false, &mut s);
    match code {
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS => {}
      other => panic!("expected INVALID_ARGUMENTS, got {:?}", other),
    }
  }

  #[test]
  fn wrap_ring_buffer_rejects_state_it_cannot_split() {
    let mut s = state_with_ringbuffer(16, 4);
    s.should_wrap_ringbuffer = true;

    s.pos = -1;
    assert!(!WrapRingBuffer(&mut s));

    s.pos = 0;
    s.ringbuffer_size = -1;
    assert!(!WrapRingBuffer(&mut s));

    // size beyond the allocation
    s.ringbuffer_size = 64;
    s.pos = 1;
    assert!(!WrapRingBuffer(&mut s));

    // pos past the tail region the wrap copies from
    s.ringbuffer_size = 16;
    s.pos = 8;
    assert!(!WrapRingBuffer(&mut s));

    // and a state it can handle: half the buffer is the tail region
    let mut ok = state_with_ringbuffer(16, 4);
    ok.ringbuffer_size = 8;
    ok.ringbuffer_mask = 7;
    ok.pos = 4;
    ok.should_wrap_ringbuffer = true;
    assert!(WrapRingBuffer(&mut ok));
    assert!(!ok.should_wrap_ringbuffer);
  }

  #[test]
  fn read_huffman_code_rejects_offset_past_table() {
    let mut s = state();
    let mut table = [HuffmanCode::default(); 4];
    let code = ReadHuffmanCode(256, 256, &mut table, 5, None, &mut s, &[]);
    match code {
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE => {}
      other => panic!("expected HUFFMAN_SPACE, got {:?}", other),
    }
  }

  #[test]
  fn read_huffman_code_reports_a_simple_table_that_will_not_fit() {
    // The simple-code builder needs 1 << HUFFMAN_TABLE_BITS entries; a caller
    // slice shorter than that must surface as an error, not a partial table.
    let mut s = state();
    s.substate_huffman = BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_SIMPLE_BUILD;
    s.symbol = 0;
    let mut table = [HuffmanCode::default(); 8];
    match ReadHuffmanCode(256, 256, &mut table, 0, None, &mut s, &[]) {
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE => {}
      other => panic!("expected HUFFMAN_SPACE, got {:?}", other),
    }
  }

  #[test]
  fn read_huffman_code_reports_a_complex_table_that_will_not_fit() {
    let mut s = state();
    s.substate_huffman = BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_LENGTH_SYMBOLS;
    // Nothing left to read, and a complete code, so the builder runs.
    s.symbol = 256;
    s.space = 0;
    s.code_length_histo[1] = 2;
    let mut table = [HuffmanCode::default(); 8];
    match ReadHuffmanCode(256, 256, &mut table, 0, None, &mut s, &[]) {
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE => {}
      other => panic!("expected HUFFMAN_SPACE, got {:?}", other),
    }
  }

  #[test]
  fn read_huffman_code_reports_a_code_length_histogram_mismatch() {
    // Resuming BROTLI_STATE_HUFFMAN_COMPLEX with a histogram that does not
    // describe code_length_code_lengths is exactly what the builder's
    // consistency check exists to catch: without it the sort loop indexes
    // `sorted` out of bounds.
    let mut s = state();
    s.substate_huffman = BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_COMPLEX;
    // Resume with every code-length code already read (sub_loop_counter at the
    // end of kCodeLengthCodeOrder) and a single code recorded, so
    // ReadCodeLengthCodeLengths returns success without consuming input.
    s.sub_loop_counter = super::CODE_LENGTH_CODES as u32;
    s.repeat = 1;
    s.space = 32;
    // ...but claim a length that code_length_code_lengths does not contain.
    s.code_length_histo[2] = 9;
    let mut table = [HuffmanCode::default(); 1080];
    match ReadHuffmanCode(256, 256, &mut table, 0, None, &mut s, &[]) {
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE => {}
      other => panic!("expected HUFFMAN_SPACE, got {:?}", other),
    }
  }

  #[test]
  fn write_ring_buffer_rejects_a_pending_run_longer_than_the_ring() {
    // partial_pos_out lagging by more than one full ring makes `to_write`
    // exceed what actually lives in the buffer.
    let mut s = state_with_ringbuffer(16, 4);
    s.rb_roundtrips = 1;
    s.pos = 16;
    s.partial_pos_out = 4;
    let mut available_out = 64usize;
    let mut output = [0u8; 64];
    let mut output_offset = 0usize;
    let mut total_out = 0usize;
    let (code, _) = WriteRingBuffer(&mut available_out, Some(&mut output[..]),
                                    &mut output_offset, &mut total_out, false, &mut s);
    match code {
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE => {}
      other => panic!("expected UNREACHABLE, got {:?}", other),
    }
  }

  #[test]
  fn write_ring_buffer_rejects_output_offset_that_would_overflow() {
    let mut s = state_with_ringbuffer(16, 4);
    s.pos = 8;
    let mut available_out = 8usize;
    let mut output_offset = usize::MAX - 1;
    let mut total_out = 0usize;
    let (code, _) = WriteRingBuffer(&mut available_out, None, &mut output_offset,
                                    &mut total_out, false, &mut s);
    match code {
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS => {}
      other => panic!("expected INVALID_ARGUMENTS, got {:?}", other),
    }
  }

  #[test]
  fn decode_context_map_rejects_mismatched_state() {
    let mut s = state();
    // Neither CONTEXT_MAP_1 with is_dist=false nor CONTEXT_MAP_2 with true.
    s.state = BrotliRunningState::BROTLI_STATE_CONTEXT_MAP_1;
    match DecodeContextMap(4, true, &mut s, &[]) {
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE => {}
      other => panic!("expected UNREACHABLE, got {:?}", other),
    }
    s.state = BrotliRunningState::BROTLI_STATE_CONTEXT_MAP_2;
    match DecodeContextMap(4, false, &mut s, &[]) {
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE => {}
      other => panic!("expected UNREACHABLE, got {:?}", other),
    }
    s.state = BrotliRunningState::BROTLI_STATE_UNINITED;
    match DecodeContextMap(4, false, &mut s, &[]) {
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE => {}
      other => panic!("expected UNREACHABLE, got {:?}", other),
    }
  }

  // CVE-2026-111497: the C decoder reported a block switch in a category with
  // a single block type as needing more input, and the streaming loop then
  // overran its 8-byte input buffer waiting for input that could not help.
  #[test]
  fn block_switch_with_one_block_type_is_a_format_error() {
    let mut s = state();
    s.block_type_length_state.num_block_types = [1, 1, 1];
    s.block_type_length_state.block_length = [0, 0, 0];
    for &safe in [false, true].iter() {
      for category in 0..3 {
        let result = DecodeBlockTypeAndLength(
          safe,
          &mut s.block_type_length_state,
          &mut s.br,
          category,
          &[],
        );
        match result {
          BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_BLOCK_SWITCH => {}
          other => panic!("category {} safe {}: expected FORMAT_BLOCK_SWITCH, got {:?}",
                          category, safe, other),
        }
      }
    }
    for category in 0..3 {
      let fbr = BitReaderSlice::new(&s.br, &[]).unwrap();
      let (fbr, status) =
        decode_block_type_and_length_slice(&mut s.block_type_length_state, fbr, category);
      fbr.sync_to(&mut s.br);
      match status {
        BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_BLOCK_SWITCH => {}
        other => panic!("category {} slice: expected FORMAT_BLOCK_SWITCH, got {:?}",
                        category, other),
      }
    }
    assert_eq!(s.br.avail_in, 0);
    assert_eq!(s.buffer_length, 0);
  }
}

// read_metadata_block driven directly from bit readers holding 0..=8 whole
// bytes in the accumulator, as a byte-aligned metadata header can leave them.
#[cfg(all(test, feature="std"))]
mod metadata_block_tests {
  use super::{read_metadata_block, BrotliDecoderErrorCode};
  use ::bit_reader::{self, BrotliBitReader};
  use ::state::BrotliMetadataCallback;
  use std::vec::Vec;

  #[derive(Default)]
  struct Chunks(Vec<Vec<u8>>);

  impl BrotliMetadataCallback for Chunks {
    fn start(&mut self, _size: usize) {
      panic!("read_metadata_block must not start a block");
    }
    fn chunk(&mut self, data: &[u8]) {
      assert!(!data.is_empty());
      self.0.push(data.to_vec());
    }
  }

  // Bytes the bit reader's accumulator holds: 8 on 64-bit targets, 4 on
  // 32-bit ones.
  const ACCUMULATOR_BYTES: u32 = ::core::mem::size_of::<bit_reader::reg_t>() as u32;

  fn input() -> Vec<u8> {
    (0..64u32).map(|i| (i * 37 + 11) as u8).collect()
  }

  // A reader over all of `input` with its first `buffered` bytes pulled into
  // the accumulator.
  fn reader(input: &[u8], buffered: u32) -> BrotliBitReader {
    let mut br = BrotliBitReader::default();
    bit_reader::BrotliInitBitReader(&mut br);
    br.avail_in = input.len() as u32;
    for _ in 0..buffered {
      assert!(bit_reader::BrotliPullByte(&mut br, input));
    }
    br
  }

  fn copy(br: &BrotliBitReader) -> BrotliBitReader {
    let mut copy = BrotliBitReader::default();
    bit_reader::BrotliBitReaderRestoreState(&mut copy, &bit_reader::BrotliBitReaderSaveState(br));
    copy
  }

  fn fields(br: &BrotliBitReader) -> (u64, u32, u32, u32) {
    (br.val_ as u64, br.bit_pos_, br.next_in, br.avail_in)
  }

  fn run(br: &mut BrotliBitReader, remaining: &mut i32, input: &[u8], with_callback: bool)
         -> (bool, Vec<Vec<u8>>) {
    let mut chunks = Chunks::default();
    let result = {
      let mut cb: Option<&mut dyn BrotliMetadataCallback> =
        if with_callback { Some(&mut chunks) } else { None };
      read_metadata_block(br, remaining, input, &mut cb)
    };
    let done = match result {
      BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => true,
      BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT => false,
      other => panic!("unexpected {:?}", other),
    };
    (done, chunks.0)
  }

  #[test]
  fn zero_length_block_touches_nothing() {
    let input = input();
    for buffered in 0..ACCUMULATOR_BYTES + 1 {
      let mut br = reader(&input, buffered);
      let before = fields(&br);
      let mut remaining = 0;
      let (done, chunks) = run(&mut br, &mut remaining, &input, true);
      assert!(done);
      assert!(chunks.is_empty());
      assert_eq!(fields(&br), before);
    }
  }

  #[test]
  fn block_inside_the_accumulator_leaves_the_rest_buffered() {
    let input = input();
    for buffered in 1..ACCUMULATOR_BYTES + 1 {
      for len in 1..buffered + 1 {
        let mut br = reader(&input, buffered);
        let mut remaining = len as i32;
        let (done, chunks) = run(&mut br, &mut remaining, &input, true);
        assert!(done);
        assert_eq!(remaining, 0);
        assert_eq!(chunks, vec![input[..len as usize].to_vec()]);
        // Input untouched; the bytes after the block are still buffered, in
        // order, ahead of the input.
        assert_eq!((br.next_in, br.avail_in), (buffered, input.len() as u32 - buffered));
        assert_eq!(bit_reader::BrotliGetAvailableBits(&br), (buffered - len) * 8);
        for expected in &input[len as usize..buffered as usize + 4] {
          let mut byte = 0u32;
          assert!(bit_reader::BrotliSafeReadBits(&mut br, 8, &mut byte, &input));
          assert_eq!(byte, *expected as u32);
        }
      }
    }
  }

  #[test]
  fn longer_block_drains_the_accumulator_then_reads_input_directly() {
    let input = input();
    for buffered in 0..ACCUMULATOR_BYTES + 1 {
      for len in buffered + 1..48 {
        let mut br = reader(&input, buffered);
        let mut remaining = len as i32;
        let (done, chunks) = run(&mut br, &mut remaining, &input, true);
        assert!(done);
        assert_eq!(remaining, 0);
        let mut expected = Vec::new();
        if buffered != 0 {
          expected.push(input[..buffered as usize].to_vec());
        }
        expected.push(input[buffered as usize..len as usize].to_vec());
        assert_eq!(chunks, expected);
        assert_eq!((br.next_in, br.avail_in), (len, input.len() as u32 - len));
        assert_eq!(bit_reader::BrotliGetAvailableBits(&br), 0);

        // The accumulator is now cold. Both ways the decoder reads next must
        // resume at the first byte after the block: the safe byte-at-a-time
        // reads of a meta-block header, and the warmed-up, window-filling
        // reads of the compressed fast path.
        let mut safe = copy(&br);
        let mut byte = 0u32;
        assert!(bit_reader::BrotliSafeReadBits(&mut safe, 8, &mut byte, &input));
        assert_eq!(byte, input[len as usize] as u32);

        let mut fast = copy(&br);
        assert!(bit_reader::BrotliWarmupBitReader(&mut fast, &input));
        for expected in &input[len as usize..len as usize + 8] {
          bit_reader::BrotliFillBitWindow(&mut fast, 8, &input);
          assert_eq!(bit_reader::BrotliReadBits(&mut fast, 8, &input), *expected as u32);
        }
      }
    }
  }

  #[test]
  fn block_longer_than_the_input_resumes_on_the_next_call() {
    let input = input();
    for buffered in 0..ACCUMULATOR_BYTES + 1 {
      for available in buffered..buffered + 6 {
        let len = available + 5;
        let first = &input[..available as usize];
        let mut br = reader(first, buffered);
        let mut remaining = len as i32;
        let (done, mut chunks) = run(&mut br, &mut remaining, first, true);
        assert!(!done);
        assert_eq!(remaining, 5);
        assert_eq!(br.avail_in, 0);
        assert_eq!(bit_reader::BrotliGetAvailableBits(&br), 0);
        assert!(chunks.iter().all(|c| !c.is_empty()));

        // The decoder hands over more input the same way: same buffer
        // extended, cursor where it stopped.
        br.avail_in = (input.len() - available as usize) as u32;
        let (done, more) = run(&mut br, &mut remaining, &input, true);
        assert!(done);
        assert_eq!(more, vec![input[available as usize..len as usize].to_vec()]);
        chunks.extend(more);
        assert_eq!(chunks.concat(), &input[..len as usize]);
        assert_eq!(br.next_in, len);
      }
    }
  }

  #[test]
  fn no_callback_has_the_same_effect_on_the_reader() {
    let input = input();
    for buffered in 0..ACCUMULATOR_BYTES + 1 {
      for len in 0..20i32 {
        for &available in [input.len() as u32, buffered + 2].iter() {
          let limited = &input[..available as usize];
          let mut with = reader(limited, buffered);
          let mut without = copy(&with);
          let (mut with_remaining, mut without_remaining) = (len, len);
          let (with_done, _) = run(&mut with, &mut with_remaining, limited, true);
          let (without_done, chunks) = run(&mut without, &mut without_remaining, limited, false);
          assert!(chunks.is_empty());
          assert_eq!(with_done, without_done);
          assert_eq!(with_remaining, without_remaining);
          assert_eq!(fields(&with), fields(&without));
        }
      }
    }
  }

  #[cfg(debug_assertions)]
  #[test]
  #[should_panic(expected = "assertion")]
  fn unaligned_accumulator_is_a_precondition_violation() {
    let input = input();
    let mut br = reader(&input, 2);
    let mut bits = 0u32;
    assert!(bit_reader::BrotliSafeReadBits(&mut br, 3, &mut bits, &input));
    let mut remaining = 4;
    run(&mut br, &mut remaining, &input, true);
  }
}
