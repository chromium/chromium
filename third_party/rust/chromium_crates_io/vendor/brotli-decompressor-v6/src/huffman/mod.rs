#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]
mod tests;
use ::core;
use alloc;
use alloc::Allocator;
use alloc::SliceWrapper;
use alloc::SliceWrapperMut;
use core::convert::TryInto;
use core::default::Default;
pub const BROTLI_HUFFMAN_MAX_CODE_LENGTH: usize = 15;

// For current format this constant equals to kNumInsertAndCopyCodes
pub const BROTLI_HUFFMAN_MAX_CODE_LENGTHS_SIZE: usize = 704;

// Outlined from `load_huffman_code` (no `#[inline]`) so rustc's MIR inliner
// leaves `dst` as a pointer parameter until LLVM's CGSCC pass inlines
// `copy_from_slice_impl` and folds the 4-byte `memcpy` into a single 32-bit
// `load i32` + `store i32` before SROA sees the caller's stack slot.
#[cfg(not(target_pointer_width = "64"))]
fn load_huffman_code_into(dst: &mut [HuffmanCode; 1], table: &[HuffmanCode], idx: usize) {
  match table.get(idx..idx + 1) {
    Some(sub) => dst.clone_from_slice(sub),
    None => debug_assert!(false, "Huffman table index {} out of range", idx),
  }
}

#[inline(always)]
pub fn load_huffman_code(table: &[HuffmanCode], idx: usize) -> HuffmanCode {
  #[cfg(target_pointer_width = "64")]
  {
    fast!((table)[idx])
  }
  #[cfg(not(target_pointer_width = "64"))]
  {
    // Table builders fill every entry a valid code can reach, so the fallback
    // is unreachable; it avoids a panic path in the hot loop.
    let mut slot = [HuffmanCode { value: 0, bits: 0 }];
    load_huffman_code_into(&mut slot, table, idx);
    slot[0]
  }
}

// Huffman table lookup.  A slice is bounds checked.  A fixed window clamps the
// index to its last entry instead, which the compiler can see is in bounds, so
// the hot decode loops pay no check: root lookups are below 256 already, and
// second-level ones cost a conditional move.  A window holds a whole tree when
// it is BrotliHuffmanMaxTableSize long, so a valid index is never clamped.
pub trait HuffmanTable {
  fn entry(&self, index: usize) -> HuffmanCode;
}

impl HuffmanTable for [HuffmanCode] {
  #[inline(always)]
  fn entry(&self, index: usize) -> HuffmanCode {
    load_huffman_code(self, index)
  }
}

// Window sizes for the literal and insert-and-copy trees, the ones decoded
// several times per command: the largest tables of their 256- and 704-symbol
// alphabets.  HuffmanTreeGroupDecode reserves BrotliHuffmanMaxTableSize of
// the group's alphabet past every tree, so deriving the windows from the same
// function keeps that room and the windows equal.  DISTANCE_ROOT_WINDOW covers
// the 256-entry 8-bit root table of a distance tree.
pub const LITERAL_TABLE_WINDOW: usize = BrotliHuffmanMaxTableSize(256);
pub const INSERT_COPY_TABLE_WINDOW: usize = BrotliHuffmanMaxTableSize(704);
pub const DISTANCE_ROOT_WINDOW: usize = 256;

static EMPTY_TABLE: [HuffmanCode; INSERT_COPY_TABLE_WINDOW] =
  [HuffmanCode { value: 0, bits: 0 }; INSERT_COPY_TABLE_WINDOW];

// A fixed window type, cut out of a table group's codes by table_window.
pub trait TableWindow: HuffmanTable {
  fn window(codes: &[HuffmanCode], offset: Option<usize>) -> &Self;
}

// One impl per window size rather than over [HuffmanCode; N]: const generics
// need Rust 1.51, and these three sizes are the only ones.
macro_rules! fixed_table_window {
  ($n:expr) => {
    impl HuffmanTable for [HuffmanCode; $n] {
      #[inline(always)]
      fn entry(&self, index: usize) -> HuffmanCode {
        // A valid table never reaches the clamp; it only keeps the lookup
        // free of a bounds check.
        debug_assert!(index < $n, "Huffman table index {} in a {}-entry window", index, $n);
        load_huffman_code(self, core::cmp::min(index, $n - 1))
      }
    }

    impl TableWindow for [HuffmanCode; $n] {
      #[inline(always)]
      fn window(codes: &[HuffmanCode], offset: Option<usize>) -> &Self {
        let window = offset.and_then(|off| codes.get(off..).and_then(|s| s.get(..$n)));
        debug_assert!(offset.is_none() || offset == Some(u32::MAX as usize) || window.is_some(),
                      "no {}-entry window at offset {:?} of {} codes", $n, offset, codes.len());
        match window {
          Some(w) => w.try_into().unwrap(),
          None => (&EMPTY_TABLE[..$n]).try_into().unwrap(),
        }
      }
    }
  };
}
fixed_table_window!(LITERAL_TABLE_WINDOW);
fixed_table_window!(INSERT_COPY_TABLE_WINDOW);
fixed_table_window!(DISTANCE_ROOT_WINDOW);

// Upper bound on the table BrotliBuildHuffmanTable produces for an alphabet of
// `alphabet_size_limit` symbols: a 256-entry root table plus at most
// 4 + 7 + 15 + 31 + 63 second-level entries, as the C decoder computes it.
// For the largest alphabet (704 insert-and-copy codes) it equals
// INSERT_COPY_TABLE_WINDOW.
pub const fn BrotliHuffmanMaxTableSize(alphabet_size_limit: u16) -> usize {
  alphabet_size_limit as usize + 376
}
pub const BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH: u32 = 5;

// Largest table (root table plus second-level tables) for a complete prefix
// code over at most N symbols, with root bits 8 and code lengths up to 15, as
// in c/dec/huffman.h. Block type codes have up to 258 symbols, block length
// codes 26 and context map codes 272.
pub const BROTLI_HUFFMAN_MAX_SIZE_26: u32 = 396;
pub const BROTLI_HUFFMAN_MAX_SIZE_258: u32 = 632;
pub const BROTLI_HUFFMAN_MAX_SIZE_272: u32 = 646;

// No fixed table size exceeds INSERT_COPY_TABLE_WINDOW, which the no_std
// brotli_decode sizes its table storage by.  Each line fails to compile, as an
// array length mismatch, if one does (assert! in a const needs Rust 1.57).
const _: [(); 1] = [(); (LITERAL_TABLE_WINDOW <= INSERT_COPY_TABLE_WINDOW) as usize];
const _: [(); 1] = [(); (DISTANCE_ROOT_WINDOW <= INSERT_COPY_TABLE_WINDOW) as usize];
const _: [(); 1] = [(); (BROTLI_HUFFMAN_MAX_SIZE_26 as usize <= INSERT_COPY_TABLE_WINDOW) as usize];
const _: [(); 1] = [(); (BROTLI_HUFFMAN_MAX_SIZE_258 as usize <= INSERT_COPY_TABLE_WINDOW) as usize];
const _: [(); 1] = [(); (BROTLI_HUFFMAN_MAX_SIZE_272 as usize <= INSERT_COPY_TABLE_WINDOW) as usize];

// 4-byte aligned so that a copy, and the fill of a new table, is one 32-bit
// access rather than separate 16- and 8-bit ones. On 64-bit targets `value` is
// kept at offset 0 and `bits` at offset 2 (which produces faster codegen on
// x86-64 and aarch64). On 32-bit targets `bits` is placed in the low byte and
// `value` in the upper 16 bits (matching C's `BROTLI_HUFFMAN_CODE_FAST_LOAD`)
// so a single 32-bit load extracts `bits` via `uxtb`/`uxtab` and `value` via
// `lsr #16`. Keep `HuffmanCodeStruct` in `c/brotli/decode.h` in sync.
#[repr(C, align(4))]
#[derive(PartialEq, Copy, Clone, Debug)]
pub struct HuffmanCode {
  #[cfg(target_pointer_width = "64")]
  pub value: u16, // symbol value or table offset
  pub bits: u8, // number of bits used for this symbol
  #[cfg(not(target_pointer_width = "64"))]
  pub value: u16, // symbol value or table offset
}

impl HuffmanCode {
  pub fn eq(&self, other: &Self) -> bool {
    self.value == other.value && self.bits == other.bits
  }
}

impl Default for HuffmanCode {
  fn default() -> Self {
    HuffmanCode {
      value: 0,
      bits: 0,
    }
  }
}

// Contains a collection of Huffman trees with the same alphabet size.
pub struct HuffmanTreeGroup<Alloc32: Allocator<u32>, AllocHC: Allocator<HuffmanCode>> {
  pub htrees: Alloc32::AllocatedMemory,
  pub codes: AllocHC::AllocatedMemory,
  pub alphabet_size: u16,
  pub max_symbol: u16,
  pub num_htrees: u16,
}

impl<AllocU32 : alloc::Allocator<u32>,
     AllocHC : alloc::Allocator<HuffmanCode> > HuffmanTreeGroup<AllocU32, AllocHC> {
    // Buffers from a previous metablock are kept when they are large enough.
    // Every table entry a valid stream can reach is written by the table
    // builder before it is read, and trees past num_htrees are never looked
    // up, so stale contents are not observable.
    pub fn init(self : &mut Self, alloc_u32 : &mut AllocU32, alloc_hc : &mut AllocHC,
                alphabet_size : u16, max_symbol: u16, ntrees : u16) {
        self.alphabet_size = alphabet_size;
        self.max_symbol = max_symbol;
        self.num_htrees = ntrees;
        let nt = ntrees as usize;
        // Keep `htrees` when large enough; mark entries past `nt` with `u32::MAX`
        // so no lookup can reach a tree of an earlier metablock.
        if self.htrees.slice().len() < nt {
            alloc_u32.free_cell(core::mem::take(&mut self.htrees));
            self.htrees = alloc_u32.alloc_cell(nt);
        } else if self.htrees.slice().len() > nt {
            for slot in self.htrees.slice_mut()[nt..].iter_mut() {
                *slot = u32::MAX;
            }
        }
        let codes_len = self.initial_codes_len();
        if self.codes.slice().len() < codes_len {
            alloc_hc.free_cell(core::mem::take(&mut self.codes));
            self.codes = alloc_hc.alloc_cell(codes_len);
        }
    }

    pub fn needs_codes_growth(&self, alphabet_size: u16, max_symbol: u16, ntrees: u16) -> bool {
        let max = BrotliHuffmanMaxTableSize(alphabet_size.max(max_symbol));
        let typical = 256 + (max - 256) / 4;
        let codes_len = max + (ntrees as usize).saturating_sub(1) * typical;
        let cur_len = self.codes.slice().len();
        cur_len != 0 && cur_len < codes_len
    }

    pub fn max_table_size(&self) -> usize {
        BrotliHuffmanMaxTableSize(self.alphabet_size.max(self.max_symbol))
    }

    // Room for one worst-case table and typical tables for the rest.  Real
    // tables are rarely much over the 256-entry root table, and allocators
    // must initialize what they return, so sizing every tree for the worst
    // case cost more than decoding small streams did.  ensure_codes_room
    // grows the buffer when a stream needs more.
    fn initial_codes_len(&self) -> usize {
        let max = self.max_table_size();
        let typical = 256 + (max - 256) / 4;
        max + (self.num_htrees as usize).saturating_sub(1) * typical
    }
}

// The window of `codes` starting at `offset`; HuffmanTreeGroupDecode makes
// room for it when it builds the tree there.  An all-zero table if there is no
// such tree.
#[inline(always)]
pub fn table_window<T: TableWindow>(codes: &[HuffmanCode], offset: Option<usize>) -> &T {
  T::window(codes, offset)
}

// Makes room for one more worst-case table at `offset` in `codes`, keeping the
// tables already built below `offset`.  False if the allocation fails.
pub fn ensure_codes_room<AllocHC: Allocator<HuffmanCode>>(codes: &mut AllocHC::AllocatedMemory,
                                                          alloc_hc: &mut AllocHC,
                                                          offset: usize, max_table_size: usize)
                                                          -> bool {
    let needed = offset + max_table_size;
    let len = codes.slice().len();
    if len >= needed {
        return true;
    }
    let mut grown = alloc_hc.alloc_cell(core::cmp::max(needed, len + len / 2));
    if grown.slice().len() < needed {
        return false;
    }
    grown.slice_mut()[..offset].copy_from_slice(&codes.slice()[..offset]);
    alloc_hc.free_cell(core::mem::replace(codes, grown));
    true
}

impl<AllocU32 : alloc::Allocator<u32>,
     AllocHC : alloc::Allocator<HuffmanCode> > HuffmanTreeGroup<AllocU32, AllocHC> {

//  pub fn get_tree_mut<'a>(self :&'a mut Self, index : u32, mut tree_out : &'a mut [HuffmanCode]) {
//        let start : usize = fast!((self.htrees)[index as usize]) as usize;
//        let _ = core::mem::replace(&mut tree_out, fast_mut!((self.codes.slice_mut())[start;]));
//    }
//    pub fn get_tree<'a>(self :&'a Self, index : u32, mut tree_out : &'a [HuffmanCode]) {
//        let start : usize = fast!((self.htrees)[index as usize]) as usize;
//        let _ = core::mem::replace(&mut tree_out, fast_slice!((self.codes)[start;]));
//    }
    #[allow(dead_code)]
    pub fn get_tree_mut(&mut self, index : u32) -> &mut [HuffmanCode] {
        let start : usize = fast_slice!((self.htrees)[index as usize]) as usize;
        fast_mut!((self.codes.slice_mut())[start;])
    }
    #[allow(dead_code)]
    pub fn get_tree(&self, index : u32) -> &[HuffmanCode] {
        let start : usize = fast_slice!((self.htrees)[index as usize]) as usize;
        fast_slice!((self.codes)[start;])
    }
    pub fn reset(self : &mut Self, alloc_u32 : &mut AllocU32, alloc_hc : &mut AllocHC) {
        alloc_u32.free_cell(core::mem::replace(&mut self.htrees,
                                               AllocU32::AllocatedMemory::default()));
        alloc_hc.free_cell(core::mem::replace(&mut self.codes,
                                              AllocHC::AllocatedMemory::default()));

// for mut iter in self.htrees[0..self.num_htrees as usize].iter_mut() {
//    if iter.slice().len() > 0 {
//        alloc_hc.free_cell(core::mem::replace(&mut iter,
//                                              AllocHC::AllocatedMemory::default()));
//    }
// }

    }
    pub fn build_hgroup_cache(&self) -> [&[HuffmanCode]; 256] {
      let mut ret : [&[HuffmanCode]; 256] = [&[]; 256];
      self.fill_hgroup_cache(&mut ret);
      ret
    }
    // As build_hgroup_cache, into an array the caller provides: returning the
    // 4 KiB array by value costs a copy per call, and the decoder builds three
    // of them each time it resumes decoding commands.
    #[inline(always)]
    pub(crate) fn fill_hgroup_cache<'a>(&'a self, cache: &mut [&'a [HuffmanCode]; 256]) {
      for (entry, htree) in cache.iter_mut().zip(self.htrees.slice().iter()) {
          *entry = fast_slice!((&self.codes)[*htree as usize ; ]);
      }
    }
}

impl<AllocU32 : alloc::Allocator<u32>,
     AllocHC : alloc::Allocator<HuffmanCode> > Default for HuffmanTreeGroup<AllocU32, AllocHC> {
    fn default() -> Self {
        HuffmanTreeGroup::<AllocU32, AllocHC> {
          htrees : AllocU32::AllocatedMemory::default(),
          codes : AllocHC::AllocatedMemory::default(),
          max_symbol: 0,
          alphabet_size : 0,
          num_htrees : 0,
        }
    }
}



const BROTLI_REVERSE_BITS_MAX: usize = 8;

const BROTLI_REVERSE_BITS_BASE: u8 = 0;

const BROTLI_REVERSE_BITS_LOWEST: u32 =
  (1u32 << (BROTLI_REVERSE_BITS_MAX as u32 - 1 + BROTLI_REVERSE_BITS_BASE as u32));

// Callers narrow their key with `as u8`, which is sound only because this port
// uses BROTLI_REVERSE_BITS_BASE == 0: keys accumulate from
// BROTLI_REVERSE_BITS_LOWEST and so stay under
// 1 << (BROTLI_REVERSE_BITS_MAX + BROTLI_REVERSE_BITS_BASE).
//
// That is not a spec guarantee. The reference implementation sets
// BROTLI_REVERSE_BITS_BASE = (sizeof(brotli_reg_t) << 3) - BROTLI_REVERSE_BITS_MAX
// (56 on 64-bit) wherever BROTLI_RBIT is available, which puts the key in the
// high bits of a full register and makes it far larger than a u8. If that is
// ever mirrored here the narrowing below would silently truncate, so fail the
// build instead.
const _REVERSE_BITS_KEYS_FIT_IN_U8: [(); 256] =
  [(); 1usize << (BROTLI_REVERSE_BITS_MAX + BROTLI_REVERSE_BITS_BASE as usize)];

// Returns reverse(num >> BROTLI_REVERSE_BITS_BASE, BROTLI_REVERSE_BITS_MAX),
// where reverse(value, len) is the bit-wise reversal of the len least
// significant bits of value.
//
// Callers narrow to u8, which is sound because BrotliBuildHuffmanTable rejects
// root_bits outside [1, BROTLI_REVERSE_BITS_MAX] up front and
// BrotliBuildCodeLengthsHuffmanTable requires
// BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH <= BROTLI_REVERSE_BITS_MAX, which
// bounds every key by 1 << 8.
#[inline(always)]
fn BrotliReverseBits(num: u8) -> u32 {
  num.reverse_bits() as u32
}

// Stores `code` in `table[offset]`, `table[offset + step]`, ...,
// `table[offset + end - step]`.
// Assumes that `end` is a positive integer multiple of `step`.
//
// `BrotliBuildCodeLengthsHuffmanTable` and `BrotliBuildHuffmanTable` always
// pass `step = 1 << bits` and `end = 1 << table_bits` with `1 <= bits <= table_bits`,
// so they call `replicate_value_pow2` directly to avoid per-symbol sign and
// divisibility checks. `ReplicateValue` retains the full input validation for
// white-box guard tests.
#[cfg(test)]
#[inline(always)]
fn ReplicateValue(table: &mut [HuffmanCode],
                  offset: u32,
                  step: i32,
                  end: i32,
                  code: HuffmanCode) -> bool {
  if step <= 0 || end <= 0 {
    return false;
  }
  let step_u = step as usize;
  let end_u = end as usize;
  if if (step_u & (step_u - 1)) == 0 {
    (end_u & (step_u - 1)) != 0
  } else {
    end_u % step_u != 0
  } {
    return false;
  }
  replicate_value_pow2(table, offset as usize, step_u, end_u, code)
}

// Do not mark `#[inline]` or `#[inline(always)]`: if rustc's MIR inliner
// inlines this before LLVM IR lowering, `*slot = HuffmanCode::new(...)` is
// lowered as three separate field stores (`bits`, `pad`, `value`). Leaving
// inlining to LLVM's CGSCC pass coerces the 4-byte `#[repr(C, align(4))]`
// `HuffmanCode` argument to `i32` on 32-bit targets and emits a single 32-bit
// `str.w` per table slot.
// Two-step slicing (`get_mut(offset..)` then `get_mut(..=last)`) uses
// `RangeToInclusive` so the length `last + 1` is known non-zero and avoids
// `RangeInclusive`'s extra overflow/empty-slice checks.
fn replicate_value_pow2(table: &mut [HuffmanCode],
                        offset: usize,
                        step: usize,
                        end: usize,
                        code: HuffmanCode) -> bool {
  debug_assert!(step > 0 && end >= step && (end % step) == 0);
  let last = end.wrapping_sub(step);
  let slice = match table.get_mut(offset..) {
    Some(s) => match s.get_mut(..=last) {
      Some(s) => s,
      None => return false,
    },
    None => return false,
  };
  let mut pos = 0;
  while let Some(slot) = slice.get_mut(pos) {
    *slot = code;
    pos += step;
  }
  true
}

// Returns the table width of the next 2nd level table. count is the histogram
// of bit lengths for the remaining symbols, len is the code length of the next
// processed symbol.
//
// The scan runs len up to BROTLI_HUFFMAN_MAX_CODE_LENGTH, which can be past the
// caller's max_length, so `count` may run out underneath it.
#[inline(always)]
fn NextTableBitSize(count: &[u16], mut len: i32, root_bits: i32) -> Option<i32> {
  debug_assert!(len > root_bits);
  let mut left: i32 = 1i32 << (len - root_bits);
  while len < BROTLI_HUFFMAN_MAX_CODE_LENGTH as i32 {
    let c = match count.get(len as usize) {
      Some(&c) => c,
      None => return None,
    };
    left -= i32::from(c);
    if left <= 0 {
      break;
    }
    len += 1;
    left <<= 1;
  }
  Some(len - root_bits)
}

// `symbol_lists` is indexed relative to `symbol_lists_offset`, and the relative
// index is deliberately negative for the per-length list heads (the reference
// implementation indexes backwards off a raw pointer here). Wrapping addition
// in `isize` cast to `usize` maps negative out-of-bounds indices above
// `isize::MAX as usize`, so a single `symbol_lists.get(index)` check covers
// both directions without 64-bit arithmetic on 32-bit targets.
#[inline(always)]
fn symbol_list_value(symbol_lists: &[u16],
                     symbol_lists_offset: usize,
                     relative_index: i32) -> Option<u16> {
  let index = (symbol_lists_offset as isize).wrapping_add(relative_index as isize) as usize;
  symbol_lists.get(index).copied()
}


pub fn BrotliBuildCodeLengthsHuffmanTable(mut table: &mut [HuffmanCode],
                                          code_lengths: &[u8],
                                          count: &[u16]) -> bool {
  let mut sorted: [i32; 18] = fast_uninitialized![18];     /* symbols sorted by code length */
  // offsets in sorted table for each length
  let mut offset: [i32; (BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH + 1) as usize] =
    fast_uninitialized![(BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH + 1) as usize];
  const table_size: i32 = 1 << BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH;
  if BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH as usize > BROTLI_REVERSE_BITS_MAX ||
     table.len() < table_size as usize ||
     code_lengths.len() < sorted.len() ||
     count.len() <= BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH as usize {
    return false;
  }
  let mut actual_count =
    [0u16; (BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH + 1) as usize];
  for code_length in code_lengths.iter().take(sorted.len()) {
    let code_length_index = *code_length as usize;
    if code_length_index >= actual_count.len() {
      return false;
    }
    actual_count[code_length_index] += 1;
  }
  // Avoid a `slice != slice` comparison, which lowers to a `bcmp` call for the
  // 5-element (10-byte) histogram check on every Huffman table build.
  let mut count_diff = 0u16;
  for (a, c) in actual_count[1..].iter().zip(count[1..actual_count.len()].iter()) {
    count_diff |= *a ^ *c;
  }
  if count_diff != 0 {
    return false;
  }

  // generate offsets into sorted symbol table by code length
  let mut symbol: i32 = -1;         /* symbol index in original or sorted table */
  let mut bits: i32 = 1;
  for _ in 0..BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH {
    symbol += fast!((count)[bits as usize]) as i32;
    fast_mut!((offset)[bits as usize]) = symbol;
    bits += 1;
  }
  // Symbols with code length 0 are placed after all other symbols.
  fast_mut!((offset)[0]) = 17;

  // sort symbols by length, by symbol order within each length
  symbol = 18;
  loop {
    for _ in 0..6 {
      symbol -= 1;
      let index = fast!((offset)[fast_inner!((code_lengths)[symbol as usize]) as usize]);
      fast_mut!((offset)[fast_inner!((code_lengths)[symbol as usize]) as usize]) -= 1;
      fast_mut!((sorted)[index as usize]) = symbol;
    }
    if symbol == 0 {
      break;
    }
  }

  // Special case: all symbols but one have 0 code length.
  if fast!((offset)[0]) == 0 {
    let code: HuffmanCode = HuffmanCode {
      bits: 0,
      value: fast!((sorted)[0]) as u16,
    };
    for val in fast_mut!((table)[0 ; table_size as usize]).iter_mut() {
      *val = code;
    }
    return true;
  }

  // fill in table
  //
  // The caller only gets here with a complete prefix code (it rejects
  // space != 0), so the keys below sum to exactly 1 << BROTLI_REVERSE_BITS_MAX
  // and the last one used is 0xFF: `key as u8` cannot truncate.
  let mut key: u32 = 0; /* prefix code */
  let mut key_step: u32 = BROTLI_REVERSE_BITS_LOWEST; /* prefix code addend */
  symbol = 0;
  bits = 1;
  let mut step: i32 = 2;
  loop {
    let mut code: HuffmanCode = HuffmanCode {
      bits: (bits as u8),
      value: 0,
    };
    let mut bits_count: i32 = fast!((count)[bits as usize]) as i32;

    while bits_count != 0 {
      code.value = fast!((sorted)[symbol as usize]) as u16;
      symbol += 1;
      let reversed_key = BrotliReverseBits(key as u8) as usize;
      // Cannot fail while `table_size` is 32: `step` is `1 << bits` and
      // `reversed_key < 1 << bits`, so the last index written is below `table_size`.
      if !replicate_value_pow2(&mut table, reversed_key, step as usize, table_size as usize, code) {
        return false;
      }
      key += key_step;
      bits_count -= 1;
    }
    step <<= 1;
    key_step >>= 1;
    bits += 1;
    if !(bits <= BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH as i32) {
      break;
    }
  }
  true
}

pub fn BrotliBuildHuffmanTable(mut root_table: &mut [HuffmanCode],
                               root_bits: i32,
                               symbol_lists: &[u16],
                               symbol_lists_offset: usize, /* need negative-index to symbol_lists */
                               count: &mut [u16])
                               -> u32 {
  let mut code: HuffmanCode = HuffmanCode {
    bits: 0,
    value: 0,
  };       /* current table entry */
  let mut max_length: i32 = -1;

  // Entry preconditions. The body's index bounds all derive from these plus the
  // max_length check below: root_bits <= 8 keeps every prefix key under 1 << 8,
  // and max_length < count.len() keeps every code-length index in range.
  if root_bits <= 0 ||
     root_bits as usize > BROTLI_REVERSE_BITS_MAX ||
     BROTLI_HUFFMAN_MAX_CODE_LENGTH as i32 - root_bits >
       BROTLI_REVERSE_BITS_MAX as i32 ||
     symbol_lists_offset >= symbol_lists.len() {
    return 0;
  }

  while match symbol_list_value(symbol_lists, symbol_lists_offset, max_length) {
    Some(value) => value == 0xFFFF,
    None => return 0,
  } {
    max_length -= 1;
  }
  max_length += BROTLI_HUFFMAN_MAX_CODE_LENGTH as i32 + 1;
  // The scan above can walk off the front of symbol_lists' head region, which is
  // the one way max_length ends up negative. Callers may pass a count histogram
  // only as long as the code lengths they used, hence the second bound.
  if max_length < 0 || max_length as usize >= count.len() {
    return 0;
  }
  debug_assert!(max_length <= BROTLI_HUFFMAN_MAX_CODE_LENGTH as i32);

  let mut table_free_offset: u32 = 0;
  let mut table_bits: i32 = root_bits;      /* key length of current table */
  // root_bits <= 8, so every `1 << table_bits` below is at most 1 << 8.
  let mut table_size: i32 = 1 << table_bits;/* size of current table */
  let mut total_size: i32 = table_size;     /* sum of root table size and 2nd level table sizes */

  // fill in root table
  // let's reduce the table size to a smaller size if possible, and
  // create the repetitions by memcpy if possible in the coming loop
  if table_bits > max_length {
    table_bits = max_length;
    table_size = 1 << table_bits;
  }
  let mut key: u32 = 0; /* prefix code */
  let mut key_step: u32 = BROTLI_REVERSE_BITS_LOWEST; /* prefix code addend */
  let mut bits: i32 = 1;
  let mut step: i32 = 2; /* step size to replicate values in current table */
  loop {
    code.bits = bits as u8;
    let mut symbol: i32 = bits - (BROTLI_HUFFMAN_MAX_CODE_LENGTH as i32 + 1);
    let mut bits_count: i32 = fast!((count)[bits as usize]) as i32;
    while bits_count != 0 {
      symbol = match symbol_list_value(symbol_lists, symbol_lists_offset, symbol) {
        Some(symbol) => symbol as i32,
        None => return 0,
      };
      code.value = symbol as u16;
      let reversed_key = BrotliReverseBits(key as u8);
      if !replicate_value_pow2(
        &mut root_table,
        (table_free_offset + reversed_key) as usize,
        step as usize,
        table_size as usize,
        code,
      ) {
        return 0;
      }
      key += key_step;
      bits_count -= 1;
    }
    step <<= 1;
    key_step >>= 1;
    bits += 1;
    if !(bits <= table_bits) {
      break;
    }
  }

  // if root_bits != table_bits we only created one fraction of the
  // table, and we need to replicate it now.
  while total_size != table_size {
    // Doubling the filled prefix in place; total_size bounds the loop.
    let base = table_free_offset as usize;
    let size = table_size as usize;
    if base + 2 * size > root_table.len() {
      return 0;
    }
    let (head, tail) = root_table.split_at_mut(base + size);
    tail[..size].clone_from_slice(&head[base..]);
    table_size <<= 1;
  }

  // fill in 2nd level tables and add pointers to root table
  key_step = BROTLI_REVERSE_BITS_LOWEST >> (root_bits - 1);
  let mut sub_key: u32 = BROTLI_REVERSE_BITS_LOWEST << 1;       /* 2nd level table prefix code */
  let mut sub_key_step: u32 = BROTLI_REVERSE_BITS_LOWEST;   /* 2nd level table prefix code addend */

  step = 2;

  let mut len: i32 = root_bits + 1; /* current code length */
  while len <= max_length {
    let mut symbol: i32 = len - (BROTLI_HUFFMAN_MAX_CODE_LENGTH as i32 + 1);
    while fast!((count)[len as usize]) != 0 {
      if sub_key == (BROTLI_REVERSE_BITS_LOWEST << 1u32) {
        table_free_offset += table_size as u32;
        table_bits = match NextTableBitSize(count, len, root_bits) {
          Some(table_bits) => table_bits,
          None => return 0,
        };
        table_size = 1 << table_bits;
        total_size += table_size;
        sub_key = BrotliReverseBits(key as u8);
        key += key_step;
        // Checked rather than assumed: this is narrowed into the u16
        // HuffmanCode::value, and sub_key then indexes a write.
        let table_value = match (table_free_offset as usize).checked_sub(sub_key as usize) {
          Some(table_value) if table_value <= u16::MAX as usize => table_value as u16,
          _ => return 0,
        };
        match root_table.get_mut(sub_key as usize) {
          Some(entry) => {
            entry.bits = (table_bits + root_bits) as u8;
            entry.value = table_value;
          },
          None => return 0,
        }
        sub_key = 0;
      }
      code.bits = (len - root_bits) as u8;
      symbol = match symbol_list_value(symbol_lists, symbol_lists_offset, symbol) {
        Some(symbol) => symbol as i32,
        None => return 0,
      };
      code.value = symbol as u16;
      let reversed_sub_key = BrotliReverseBits(sub_key as u8);
      if !replicate_value_pow2(
        &mut root_table,
        (table_free_offset + reversed_sub_key) as usize,
        step as usize,
        table_size as usize,
        code,
      ) {
        return 0;
      }
      sub_key += sub_key_step;
      // len <= max_length < count.len(), and the loop condition just read a
      // nonzero count[len].
      fast_mut!((count)[len as usize]) -= 1;
    }
    step <<= 1;
    sub_key_step >>= 1;
    len += 1
  }
  total_size as u32
}



pub fn BrotliBuildSimpleHuffmanTable(table: &mut [HuffmanCode],
                                     root_bits: i32,
                                     val: &[u16],
                                     num_symbols: u32)
                                     -> u32 {
  if root_bits <= 0 || root_bits >= 32 {
    return 0;
  }
  // num_symbols is the raw 2-bit field (plus one extra bit when it reads 3), so
  // 0..=4 are the only encodable values. This match rejects anything larger,
  // which is why the branch chain below needs no final catch-all.
  let required_symbols = match num_symbols {
    0 => 1,
    1 => 2,
    2 | 3 => 3,
    4 => 4,
    _ => return 0,
  };
  if val.len() < required_symbols {
    return 0;
  }
  let mut table_size: u32;
  // root_bits is in 1..32 by the check above, so the shift is in range.
  let goal_size: u32 = 1u32 << root_bits;
  if table.len() < goal_size as usize {
    return 0;
  }
  let table = &mut table[..goal_size as usize];
  if num_symbols == 0 {
    let code = HuffmanCode {
      bits: 0,
      value: fast!((val)[0]),
    };
    for entry in table.iter_mut() {
      *entry = code;
    }
    return goal_size;
  } else if num_symbols == 1 {
    fast_mut!((table)[0]).bits = 1;
    fast_mut!((table)[1]).bits = 1;
    if fast!((val)[1]) > fast!((val)[0]) {
      fast_mut!((table)[0]).value = fast!((val)[0]);
      fast_mut!((table)[1]).value = fast!((val)[1]);
    } else {
      fast_mut!((table)[0]).value = fast!((val)[1]);
      fast_mut!((table)[1]).value = fast!((val)[0]);
    }
    table_size = 2;
  } else if num_symbols == 2 {
    fast_mut!((table)[0]).bits = 1;
    fast_mut!((table)[0]).value = fast!((val)[0]);
    fast_mut!((table)[2]).bits = 1;
    fast_mut!((table)[2]).value = fast!((val)[0]);
    if fast!((val)[2]) > fast!((val)[1]) {
      fast_mut!((table)[1]).value = fast!((val)[1]);
      fast_mut!((table)[3]).value = fast!((val)[2]);
    } else {
      fast_mut!((table)[1]).value = fast!((val)[2]);
      fast_mut!((table)[3]).value = fast!((val)[1]);
    }
    fast_mut!((table)[1]).bits = 2;
    fast_mut!((table)[3]).bits = 2;
    table_size = 4;
  } else if num_symbols == 3 {
    let last: u16 = if val.len() > 3 { fast!((val)[3]) } else { 65535 };
    let mut mval: [u16; 4] = [fast!((val)[0]), fast!((val)[1]), fast!((val)[2]), last];
    for i in 0..3 {
      for k in i + 1..4 {
        if mval[k] < mval[i] {
          mval.swap(k, i);
        }
      }
    }
    for i in 0..4 {
      fast_mut!((table)[i]).bits = 2;
    }
    fast_mut!((table)[0]).value = mval[0];
    fast_mut!((table)[2]).value = mval[1];
    fast_mut!((table)[1]).value = mval[2];
    fast_mut!((table)[3]).value = mval[3];
    table_size = 4;
  } else {
    debug_assert_eq!(num_symbols, 4);
    let mut mval: [u16; 4] = [fast!((val)[0]), fast!((val)[1]), fast!((val)[2]), fast!((val)[3])];
    if mval[3] < mval[2] {
      mval.swap(3, 2)
    }
    for i in 0..7 {
      fast_mut!((table)[i]).value = mval[0];
      fast_mut!((table)[i]).bits = (1 + (i & 1)) as u8;
    }
    fast_mut!((table)[1]).value = mval[1];
    fast_mut!((table)[3]).value = mval[2];
    fast_mut!((table)[5]).value = mval[1];
    fast_mut!((table)[7]).value = mval[3];
    fast_mut!((table)[3]).bits = 3;
    fast_mut!((table)[7]).bits = 3;
    table_size = 8;
  }
  while table_size != goal_size {
    let size = table_size as usize;
    let (head, tail) = table.split_at_mut(size);
    tail[..size].clone_from_slice(head);
    table_size <<= 1;
  }
  goal_size
}
