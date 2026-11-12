#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]

// Port of the shared-brotli serialized dictionary format
// (https://datatracker.ietf.org/doc/draft-vandevenne-shared-brotli-format/)
// from c/common/shared_dictionary.c in the reference implementation.
//
// A serialized dictionary may carry an LZ77 prefix dictionary (which becomes
// a compound dictionary chunk, see state.rs), plus optional custom word lists
// and transform lists that replace or augment the built-in static dictionary,
// optionally selected per-literal-context through a 64-entry context map.

use core;
use alloc;
use alloc::{Allocator, SliceWrapper, SliceWrapperMut};
use bit_reader;
use dictionary::{kBrotliDictionary, kBrotliDictionaryOffsetsByLength, kBrotliDictionarySizeBitsByLength,
                 kBrotliMaxDictionaryWordLength};
use state;
use transform::{kNumTransforms, TransformDictionaryWord, ToUpperCase};

pub const SHARED_BROTLI_MIN_DICTIONARY_WORD_LENGTH: u32 = 4;
pub const SHARED_BROTLI_MAX_DICTIONARY_WORD_LENGTH: u32 = 31;
// Longest prefix or suffix a transform can prepend/append to a dictionary
// word. Custom affixes come from the transform list's prefix/suffix table as
// "stringlets", each stored as a single length byte followed by that many
// bytes, so 255 is the hard maximum. The built-in transform list
// (transform.rs) only ever uses 5-byte prefixes and 8-byte suffixes;
// serialized shared dictionaries may use the full range.
pub const SHARED_BROTLI_MAX_TRANSFORM_AFFIX_LENGTH: u32 = 255;
pub const SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS: usize = 64;
// Max allowed by spec for custom word list size bits.
const BROTLI_MAX_SIZE_BITS: u8 = 15;

// Transform types beyond the ones used by the built-in transform list
// (see transform.rs for 0..=20).
pub const BROTLI_TRANSFORM_SHIFT_FIRST: u8 = 21;
pub const BROTLI_TRANSFORM_SHIFT_ALL: u8 = 22;
pub const BROTLI_NUM_TRANSFORM_TYPES: u8 = 23;
const BROTLI_TRANSFORM_OMIT_LAST_9: u8 = 9;
const BROTLI_TRANSFORM_UPPERCASE_FIRST: u8 = 10;
const BROTLI_TRANSFORM_UPPERCASE_ALL: u8 = 11;
const BROTLI_TRANSFORM_OMIT_FIRST_1: u8 = 12;
const BROTLI_TRANSFORM_OMIT_FIRST_9: u8 = 20;

// Parsed word lists and transform lists are packed into a single u32 arena
// so that they can be stored with the decoder's existing u32 allocator (this
// crate runs without a global allocator). Word list i occupies the
// WORD_LIST_STRIDE u32s starting at i * WORD_LIST_STRIDE; transform list j
// occupies the TRANSFORM_LIST_STRIDE u32s starting at
// num_word_lists * WORD_LIST_STRIDE + j * TRANSFORM_LIST_STRIDE.
//
// Word list layout (indexes within its stride):
//   0..8    size_bits_by_length for lengths 0..32, one byte per length
//   8..40   offsets_by_length for lengths 0..32
//   40      offset of the concatenated word data within the blob
pub(crate) const WORD_LIST_STRIDE: usize = 41;
const WL_SIZE_BITS: usize = 0;
const WL_OFFSETS: usize = 8;
const WL_DATA_OFFSET: usize = 40;
// Transform list layout:
//   0       num_transforms (0..=255)
//   1       offset of the 3*num_transforms transform triplets in the blob
//   2       offset of the 2*num_transforms SHIFT parameters, or u32::MAX
//   3       offset of the prefix/suffix stringlet region in the blob
//   4       cutOffTransforms[0] (index of ["", IDENTITY, ""]) as i16, or -1
//   5..133  prefix_suffix_map: 256 u16 offsets, two per u32, little-end first
pub(crate) const TRANSFORM_LIST_STRIDE: usize = 133;
const TL_NUM_TRANSFORMS: usize = 0;
const TL_TRANSFORMS_OFFSET: usize = 1;
const TL_PARAMS_OFFSET: usize = 2;
const TL_PREFIX_SUFFIX_OFFSET: usize = 3;
const TL_CUTOFF_IDENTITY: usize = 4;
const TL_PREFIX_SUFFIX_MAP: usize = 5;

// Byte layout of one entry in a serialized TRANSFORMS_LIST, and the ONLY place
// in this crate that knows it. Validation, the identity-cutoff scan and the
// decode-time lookup all go through transform_triplet_at.
//
// SPEC DISCREPANCY -- read before "fixing" this:
//   draft-vandevenne-shared-brotli-format describes each transform as
//     1 byte: index of prefix in prefix/suffix data
//     1 byte: index of suffix in prefix/suffix data
//     1 byte: operation index
//   i.e. PREFIX_ID, SUFFIX_ID, TYPE. The reference C implementation instead
//   reads PREFIX_ID, TYPE, SUFFIX_ID:
//     #define BROTLI_TRANSFORM_PREFIX_ID(T, I) ((T)->transforms[((I) * 3) + 0])
//     #define BROTLI_TRANSFORM_TYPE(T, I)      ((T)->transforms[((I) * 3) + 1])
//     #define BROTLI_TRANSFORM_SUFFIX_ID(T, I) ((T)->transforms[((I) * 3) + 2])
//   (c/common/transform.h:65-67, used by ParseTransformsList and by the
//   encoder that writes these dictionaries).
//
// This port follows the C implementation, because that is what actually
// produces and consumes serialized dictionaries in practice. Following the
// draft instead would reject dictionaries the reference encoder emits, and --
// worse -- would silently mis-apply any transform whose type byte and suffix
// id are each in range for the other's field, with no error to show for it.
// Switching to the draft's order is therefore a one-line edit to the
// constructor below, and nothing else: callers name the fields, so none of
// them encodes a byte position.
const TRANSFORM_TRIPLET_SIZE: usize = 3;

#[derive(Clone, Copy)]
struct TransformTriplet {
  prefix_id: u8,
  transform_type: u8,
  suffix_id: u8,
}

// Decodes transform `idx` of the list whose triplets begin at byte `base` of
// `data`. The three field assignments here are the entire definition of the
// on-the-wire order.
fn transform_triplet_at(data: &[u8], base: usize, idx: usize) -> TransformTriplet {
  let offset = base + idx * TRANSFORM_TRIPLET_SIZE;
  TransformTriplet {
    prefix_id: data[offset],
    transform_type: data[offset + 1],
    suffix_id: data[offset + 2],
  }
}

pub struct BrotliSharedDictionary<AllocU8: alloc::Allocator<u8>,
                                  AllocU32: alloc::Allocator<u32>> {
  // If set, the context map selects the dictionary for each word, from the
  // 64 literal contexts; if not set only dictionary 0 is used.
  pub context_based: bool,
  pub num_dictionaries: u8,
  pub num_word_lists: u8,
  pub num_transform_lists: u8,
  pub context_map: [u8; SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS],
  // Per dictionary: index of its word list, or num_word_lists for the
  // built-in word list.
  pub words_index: [u8; SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS],
  // Per dictionary: index of its transform list, or num_transform_lists for
  // the built-in transform list.
  pub transforms_index: [u8; SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS],
  // The serialized dictionary; word data, stringlets, transform triplets and
  // parameters are referenced by offset into this buffer.
  pub blob: state::MaybeOwnedSlice<AllocU8>,
  // The parsed word list / transform list arena described above.
  pub meta: AllocU32::AllocatedMemory,
}

impl<AllocU8: alloc::Allocator<u8>,
     AllocU32: alloc::Allocator<u32>> Default for BrotliSharedDictionary<AllocU8, AllocU32> {
  fn default() -> Self {
    BrotliSharedDictionary::<AllocU8, AllocU32> {
      context_based: false,
      num_dictionaries: 1,
      num_word_lists: 0,
      num_transform_lists: 0,
      context_map: [0; SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS],
      words_index: [0; SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS],
      transforms_index: [0; SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS],
      blob: state::MaybeOwnedSlice::default(),
      meta: AllocU32::AllocatedMemory::default(),
    }
  }
}

impl<AllocU8: alloc::Allocator<u8>,
     AllocU32: alloc::Allocator<u32>> BrotliSharedDictionary<AllocU8, AllocU32> {
  // True if a custom word list or transform list replaces the built-in
  // static dictionary; the decoder then takes the generalized lookup path.
  pub(crate) fn is_custom(&self) -> bool {
    self.num_word_lists != 0 || self.num_transform_lists != 0
  }
  pub(crate) fn words_of(&self, dict_id: u8) -> DictWords<'_> {
    let index = self.words_index[dict_id as usize];
    if index >= self.num_word_lists {
      DictWords::Builtin
    } else {
      let start = index as usize * WORD_LIST_STRIDE;
      DictWords::Custom(CustomWords {
        meta: &self.meta.slice()[start..start + WORD_LIST_STRIDE],
        blob: self.blob.slice(),
      })
    }
  }
  // Resolves `address` in the flat dictionary address space to the word and
  // transform that own it, for a word of length `len`.
  //
  // The space is partitioned across the dictionaries: each owns a contiguous
  // run of ((1 << shift) & !1) * num_transforms addresses, which is zero when
  // its word list cannot encode `len` at all (shift == 0). They are searched
  // in the order the format prescribes -- `dict_id` first, then the rest by
  // index -- with `remaining` walking past the runs the address is not in.
  //
  // c/dec/decode.c writes this as a first lookup plus a separate rescan loop
  // that re-derives the same quantities; this is that arithmetic stated once.
  // The reference's `transform_idx >= num_transforms` test is exactly
  // `remaining >= count` for the first candidate, and its two post-lookup
  // guards need no counterpart here: a candidate is selected only when
  // remaining < count, which for count > 0 forces shift >= 1 and
  // transform_idx < num_transforms.
  pub(crate) fn lookup(&self, dict_id: u8, len: i32, address: i32)
      -> Result<DictionaryLookup<'_>, DictionaryLookupError> {
    let mut remaining = address;
    for candidate in core::iter::once(dict_id)
        .chain((0..self.num_dictionaries).filter(|&d| d != dict_id)) {
      let words = self.words_of(candidate);
      let shift = words.size_bits_by_length(len) as u32;
      let transforms = self.transforms_of(candidate);
      let count = (((1u32 << shift) & !1u32) as i32) * transforms.num_transforms();
      if remaining < count {
        return Ok(DictionaryLookup {
          words: words,
          transforms: transforms,
          word_idx: remaining & bit_reader::BitMask(shift) as i32,
          transform_idx: remaining >> shift,
        });
      }
      remaining -= count;
    }
    Err(if self.words_of(dict_id).size_bits_by_length(len) == 0 {
      DictionaryLookupError::LengthNotEncodable
    } else {
      DictionaryLookupError::AddressOutOfRange
    })
  }
  pub(crate) fn transforms_of(&self, dict_id: u8) -> DictTransforms<'_> {
    let index = self.transforms_index[dict_id as usize];
    if index >= self.num_transform_lists {
      DictTransforms::Builtin
    } else {
      let start = self.num_word_lists as usize * WORD_LIST_STRIDE +
                  index as usize * TRANSFORM_LIST_STRIDE;
      DictTransforms::Custom(CustomTransforms {
        meta: &self.meta.slice()[start..start + TRANSFORM_LIST_STRIDE],
        blob: self.blob.slice(),
      })
    }
  }
}

#[derive(Clone, Copy)]
pub(crate) struct CustomWords<'a> {
  meta: &'a [u32],
  blob: &'a [u8],
}

impl<'a> CustomWords<'a> {
  fn size_bits_by_length(&self, len: i32) -> u8 {
    (self.meta[WL_SIZE_BITS + (len >> 2) as usize] >> ((len & 3) * 8)) as u8
  }
  fn word(&self, len: i32, word_idx: i32) -> &'a [u8] {
    let offset = self.meta[WL_DATA_OFFSET] as usize +
                 self.meta[WL_OFFSETS + len as usize] as usize +
                 (word_idx * len) as usize;
    &self.blob[offset..offset + len as usize]
  }
}

// Which word list and transform list own a dictionary-space address, and the
// indices within them. Named fields on purpose: nothing outside this file
// should have to remember an order.
pub(crate) struct DictionaryLookup<'a> {
  pub(crate) words: DictWords<'a>,
  pub(crate) transforms: DictTransforms<'a>,
  pub(crate) word_idx: i32,
  pub(crate) transform_idx: i32,
}

// Why an address resolved to no dictionary at all. The two cases map to the
// two distinct format errors the reference implementation reports.
#[derive(Clone, Copy)]
pub(crate) enum DictionaryLookupError {
  // The context-selected dictionary's word list cannot encode this length.
  LengthNotEncodable,
  // The address runs past the end of every candidate's transforms.
  AddressOutOfRange,
}

#[derive(Clone, Copy)]
pub(crate) enum DictWords<'a> {
  Builtin,
  Custom(CustomWords<'a>),
}

impl<'a> DictWords<'a> {
  pub(crate) fn size_bits_by_length(&self, len: i32) -> u8 {
    match *self {
      DictWords::Builtin => {
        if len as u32 > kBrotliMaxDictionaryWordLength as u32 {
          0
        } else {
          kBrotliDictionarySizeBitsByLength[len as usize]
        }
      }
      DictWords::Custom(ref words) => words.size_bits_by_length(len),
    }
  }
  pub(crate) fn word(&self, len: i32, word_idx: i32) -> &'a [u8] {
    match *self {
      DictWords::Builtin => {
        let offset = kBrotliDictionaryOffsetsByLength[len as usize] as usize +
                     (word_idx * len) as usize;
        &kBrotliDictionary[offset..offset + len as usize]
      }
      DictWords::Custom(ref words) => words.word(len, word_idx),
    }
  }
}

#[derive(Clone, Copy)]
pub(crate) struct CustomTransforms<'a> {
  meta: &'a [u32],
  blob: &'a [u8],
}

impl<'a> CustomTransforms<'a> {
  fn num_transforms(&self) -> i32 {
    self.meta[TL_NUM_TRANSFORMS] as i32
  }
  fn cutoff_identity(&self) -> i32 {
    self.meta[TL_CUTOFF_IDENTITY] as i32 as i16 as i32
  }
  fn stringlet(&self, id: u8) -> &'a [u8] {
    let map_entry = self.meta[TL_PREFIX_SUFFIX_MAP + (id >> 1) as usize] >> ((id & 1) * 16);
    let offset = self.meta[TL_PREFIX_SUFFIX_OFFSET] as usize + (map_entry as u16) as usize;
    let len = self.blob[offset] as usize;
    &self.blob[offset + 1..offset + 1 + len]
  }
  fn transform_triplet(&self, idx: i32) -> TransformTriplet {
    transform_triplet_at(self.blob, self.meta[TL_TRANSFORMS_OFFSET] as usize, idx as usize)
  }
  fn param(&self, idx: i32) -> u16 {
    let offset = self.meta[TL_PARAMS_OFFSET] as usize + (idx as usize) * 2;
    self.blob[offset] as u16 | ((self.blob[offset + 1] as u16) << 8)
  }
  fn apply(&self, dst: &mut [u8], mut word: &[u8], mut len: i32, transform_idx: i32) -> i32 {
    let triplet = self.transform_triplet(transform_idx);
    let t = triplet.transform_type;
    let mut idx: usize = 0;
    for &b in self.stringlet(triplet.prefix_id).iter() {
      dst[idx] = b;
      idx += 1;
    }
    {
      if t <= BROTLI_TRANSFORM_OMIT_LAST_9 {
        len -= t as i32;
      } else if t >= BROTLI_TRANSFORM_OMIT_FIRST_1 && t <= BROTLI_TRANSFORM_OMIT_FIRST_9 {
        let skip = (t - (BROTLI_TRANSFORM_OMIT_FIRST_1 - 1)) as i32;
        if skip > len {
          // cannot happen for valid words (len >= 4) but keep safe
          len = 0;
        } else {
          word = &word[skip as usize..];
          len -= skip;
        }
      }
      let mut i: usize = 0;
      while (i as i32) < len {
        dst[idx] = word[i];
        idx += 1;
        i += 1;
      }
      if len > 0 {
        let transformed = &mut dst[idx - len as usize..];
        if t == BROTLI_TRANSFORM_UPPERCASE_FIRST {
          ToUpperCase(transformed);
        } else if t == BROTLI_TRANSFORM_UPPERCASE_ALL {
          let mut offset: usize = 0;
          let mut remaining = len;
          while remaining > 0 {
            let step = ToUpperCase(&mut transformed[offset..]);
            offset += step as usize;
            remaining -= step;
          }
        } else if t == BROTLI_TRANSFORM_SHIFT_FIRST {
          let param = self.param(transform_idx);
          Shift(transformed, len, param);
        } else if t == BROTLI_TRANSFORM_SHIFT_ALL {
          let param = self.param(transform_idx);
          let mut offset: usize = 0;
          let mut remaining = len;
          while remaining > 0 {
            let step = Shift(&mut transformed[offset..], remaining, param);
            offset += step as usize;
            remaining -= step;
          }
        }
      }
    }
    for &b in self.stringlet(triplet.suffix_id).iter() {
      dst[idx] = b;
      idx += 1;
    }
    idx as i32
  }
}

#[derive(Clone, Copy)]
pub(crate) enum DictTransforms<'a> {
  Builtin,
  Custom(CustomTransforms<'a>),
}

impl<'a> DictTransforms<'a> {
  pub(crate) fn num_transforms(&self) -> i32 {
    match *self {
      DictTransforms::Builtin => kNumTransforms,
      DictTransforms::Custom(ref transforms) => transforms.num_transforms(),
    }
  }
  // The transform index that is ["", IDENTITY, ""] (plain copy), or -1.
  pub(crate) fn cutoff_identity(&self) -> i32 {
    match *self {
      DictTransforms::Builtin => 0,
      DictTransforms::Custom(ref transforms) => transforms.cutoff_identity(),
    }
  }
  pub(crate) fn apply(&self, dst: &mut [u8], word: &[u8], len: i32, transform_idx: i32) -> i32 {
    match *self {
      DictTransforms::Builtin => TransformDictionaryWord(dst, word, len, transform_idx),
      DictTransforms::Custom(ref transforms) => transforms.apply(dst, word, len, transform_idx),
    }
  }
}

// UTF-8-aware scalar shift, used by the SHIFT_FIRST and SHIFT_ALL transforms;
// direct port of Shift() in c/common/transform.c.
fn Shift(word: &mut [u8], word_len: i32, parameter: u16) -> i32 {
  // Limited sign extension: scalar < (1 << 24).
  let mut scalar: u32 = (parameter as u32 & 0x7FFF).wrapping_add(0x1000000 - (parameter as u32 & 0x8000));
  if word[0] < 0x80 {
    // 1-byte rune / 0sssssss / 7 bit scalar (ASCII).
    scalar = scalar.wrapping_add(word[0] as u32);
    word[0] = (scalar & 0x7F) as u8;
    1
  } else if word[0] < 0xC0 {
    // Continuation / 10AAAAAA.
    1
  } else if word[0] < 0xE0 {
    // 2-byte rune / 110sssss AAssssss / 11 bit scalar.
    if word_len < 2 {
      return 1;
    }
    scalar = scalar.wrapping_add((word[1] as u32 & 0x3F) | ((word[0] as u32 & 0x1F) << 6));
    word[0] = (0xC0 | ((scalar >> 6) & 0x1F)) as u8;
    word[1] = ((word[1] as u32 & 0xC0) | (scalar & 0x3F)) as u8;
    2
  } else if word[0] < 0xF0 {
    // 3-byte rune / 1110ssss AAssssss BBssssss / 16 bit scalar.
    if word_len < 3 {
      return word_len;
    }
    scalar = scalar.wrapping_add((word[2] as u32 & 0x3F) | ((word[1] as u32 & 0x3F) << 6) |
                                 ((word[0] as u32 & 0x0F) << 12));
    word[0] = (0xE0 | ((scalar >> 12) & 0x0F)) as u8;
    word[1] = ((word[1] as u32 & 0xC0) | ((scalar >> 6) & 0x3F)) as u8;
    word[2] = ((word[2] as u32 & 0xC0) | (scalar & 0x3F)) as u8;
    3
  } else if word[0] < 0xF8 {
    // 4-byte rune / 11110sss AAssssss BBssssss CCssssss / 21 bit scalar.
    if word_len < 4 {
      return word_len;
    }
    scalar = scalar.wrapping_add((word[3] as u32 & 0x3F) | ((word[2] as u32 & 0x3F) << 6) |
                                 ((word[1] as u32 & 0x3F) << 12) |
                                 ((word[0] as u32 & 0x07) << 18));
    word[0] = (0xF0 | ((scalar >> 18) & 0x07)) as u8;
    word[1] = ((word[1] as u32 & 0xC0) | ((scalar >> 12) & 0x3F)) as u8;
    word[2] = ((word[2] as u32 & 0xC0) | ((scalar >> 6) & 0x3F)) as u8;
    word[3] = ((word[3] as u32 & 0xC0) | (scalar & 0x3F)) as u8;
    4
  } else {
    1
  }
}

// ---------------------------------------------------------------------------
// Serialized format parsing.

struct Reader<'a> {
  data: &'a [u8],
  pos: usize,
}

impl<'a> Reader<'a> {
  fn read_u8(&mut self) -> Result<u8, ()> {
    if self.pos >= self.data.len() {
      return Err(());
    }
    let v = self.data[self.pos];
    self.pos += 1;
    Ok(v)
  }
  fn read_bool(&mut self) -> Result<bool, ()> {
    match self.read_u8()? {
      0 => Ok(false),
      1 => Ok(true),
      _ => Err(()),
    }
  }
  fn read_u16(&mut self) -> Result<u16, ()> {
    if self.pos + 2 > self.data.len() {
      return Err(());
    }
    let v = self.data[self.pos] as u16 | ((self.data[self.pos + 1] as u16) << 8);
    self.pos += 2;
    Ok(v)
  }
  // Reads a varint into a u32, erroring if it is too large.
  fn read_varint32(&mut self) -> Result<u32, ()> {
    let mut result: u32 = 0;
    let mut num = 0;
    loop {
      let byte = self.read_u8()?;
      if num == 4 && byte > 15 {
        return Err(());
      }
      result |= ((byte & 127) as u32) << (num * 7);
      if byte < 128 {
        return Ok(result);
      }
      num += 1;
    }
  }
}

// Walks one word list. If `out` is provided, fills its WORD_LIST_STRIDE u32s.
fn parse_word_list(reader: &mut Reader, mut out: Option<&mut [u32]>) -> Result<(), ()> {
  if let Some(ref mut meta) = out {
    for entry in meta.iter_mut() {
      *entry = 0;
    }
  }
  let num_encoded_lengths = (SHARED_BROTLI_MAX_DICTIONARY_WORD_LENGTH -
                             SHARED_BROTLI_MIN_DICTIONARY_WORD_LENGTH + 1) as usize;
  if reader.pos + num_encoded_lengths > reader.data.len() {
    return Err(());
  }
  let mut size_bits = [0u8; (SHARED_BROTLI_MAX_DICTIONARY_WORD_LENGTH + 1) as usize];
  for i in 0..num_encoded_lengths {
    let bits = reader.data[reader.pos + i];
    if bits > BROTLI_MAX_SIZE_BITS {
      return Err(());
    }
    size_bits[SHARED_BROTLI_MIN_DICTIONARY_WORD_LENGTH as usize + i] = bits;
  }
  reader.pos += num_encoded_lengths;
  let mut total: u32 = 0;
  if let Some(ref mut meta) = out {
    meta[WL_DATA_OFFSET] = reader.pos as u32;
  }
  for (i, &bits) in size_bits.iter().enumerate() {
    if let Some(ref mut meta) = out {
      meta[WL_SIZE_BITS + (i >> 2)] |= (bits as u32) << ((i & 3) * 8);
      meta[WL_OFFSETS + i] = total;
    }
    if bits != 0 {
      total += (i as u32) << bits;
    }
  }
  if reader.pos + total as usize > reader.data.len() {
    return Err(());
  }
  reader.pos += total as usize;
  Ok(())
}

// Walks the prefix/suffix stringlet table of one transform list. If `out` is
// provided, fills TL_PREFIX_SUFFIX_OFFSET and the prefix_suffix_map entries.
// Returns the number of stringlets.
fn parse_prefix_suffix_table(reader: &mut Reader, mut out: Option<&mut [u32]>) -> Result<usize, ()> {
  let data_length = reader.read_u16()? as usize;
  // Must at least have space for the null terminator.
  if data_length < 1 {
    return Err(());
  }
  if reader.pos + data_length >= reader.data.len() {
    return Err(());
  }
  if let Some(ref mut meta) = out {
    meta[TL_PREFIX_SUFFIX_OFFSET] = reader.pos as u32;
  }
  let mut offset: usize = 0;
  let mut stringlet_count: usize = 0;
  loop {
    let stringlet_len = reader.data[reader.pos + offset] as usize;
    if let Some(ref mut meta) = out {
      meta[TL_PREFIX_SUFFIX_MAP + (stringlet_count >> 1)] |=
        (offset as u32) << ((stringlet_count & 1) * 16);
    }
    stringlet_count += 1;
    offset += 1;
    if stringlet_len == 0 {
      if offset == data_length {
        break;
      } else {
        return Err(());
      }
    }
    if stringlet_count > 255 {
      return Err(());
    }
    offset += stringlet_len;
    if offset >= data_length {
      return Err(());
    }
  }
  reader.pos += data_length;
  Ok(stringlet_count)
}

// Walks one transform list. If `out` is provided, fills its
// TRANSFORM_LIST_STRIDE u32s including the identity cutoff.
fn parse_transforms_list(reader: &mut Reader, mut out: Option<&mut [u32]>) -> Result<(), ()> {
  if let Some(ref mut meta) = out {
    for entry in meta.iter_mut() {
      *entry = 0;
    }
  }
  if reader.pos >= reader.data.len() {
    return Err(());
  }
  let stringlet_count = parse_prefix_suffix_table(reader, out.as_deref_mut())?;
  let num_transforms = reader.read_u8()? as usize;
  let transforms_offset = reader.pos;
  if reader.pos + num_transforms * TRANSFORM_TRIPLET_SIZE > reader.data.len() {
    return Err(());
  }
  reader.pos += num_transforms * TRANSFORM_TRIPLET_SIZE;
  let mut has_params = false;
  for i in 0..num_transforms {
    let triplet = transform_triplet_at(reader.data, transforms_offset, i);
    let transform_type = triplet.transform_type;
    if triplet.prefix_id as usize >= stringlet_count ||
       triplet.suffix_id as usize >= stringlet_count {
      return Err(());
    }
    if transform_type >= BROTLI_NUM_TRANSFORM_TYPES {
      return Err(());
    }
    if transform_type == BROTLI_TRANSFORM_SHIFT_FIRST ||
       transform_type == BROTLI_TRANSFORM_SHIFT_ALL {
      has_params = true;
    }
  }
  let params_offset = reader.pos;
  if has_params {
    if reader.pos + num_transforms * 2 > reader.data.len() {
      return Err(());
    }
    reader.pos += num_transforms * 2;
    for i in 0..num_transforms {
      let transform_type =
        transform_triplet_at(reader.data, transforms_offset, i).transform_type;
      if transform_type != BROTLI_TRANSFORM_SHIFT_FIRST &&
         transform_type != BROTLI_TRANSFORM_SHIFT_ALL {
        if reader.data[params_offset + i * 2] != 0 ||
           reader.data[params_offset + i * 2 + 1] != 0 {
          return Err(());
        }
      }
    }
  }
  if let Some(meta) = out {
    meta[TL_NUM_TRANSFORMS] = num_transforms as u32;
    meta[TL_TRANSFORMS_OFFSET] = transforms_offset as u32;
    meta[TL_PARAMS_OFFSET] = if has_params { params_offset as u32 } else { u32::max_value() };
    // Compute the identity cutoff transform: the first ["", IDENTITY, ""].
    let mut cutoff: i16 = -1;
    {
      let view = CustomTransforms { meta: &*meta, blob: reader.data };
      for i in 0..num_transforms {
        let triplet = view.transform_triplet(i as i32);
        if triplet.transform_type == 0 &&
           view.stringlet(triplet.prefix_id).is_empty() &&
           view.stringlet(triplet.suffix_id).is_empty() {
          cutoff = i as i16;
          break;
        }
      }
    }
    meta[TL_CUTOFF_IDENTITY] = cutoff as u16 as u32;
  }
  Ok(())
}

pub(crate) struct ParsedSerializedDictionary {
  // Offset and length of the embedded LZ77 prefix dictionary, if any.
  pub(crate) prefix: Option<(usize, usize)>,
  pub(crate) num_word_lists: u8,
  pub(crate) num_transform_lists: u8,
}

// First pass: validates the container far enough to learn the word list and
// transform list counts (and thus the arena size), like DryParseDictionary.
pub(crate) fn dry_parse_serialized_dictionary(data: &[u8]) -> Result<ParsedSerializedDictionary, ()> {
  // Check magic header bytes.
  if data.len() < 2 || data[0] != 0x91 || data[1] != 0 {
    return Err(());
  }
  let mut reader = Reader { data: data, pos: 2 };
  let chunk_size = reader.read_varint32()?;
  let mut prefix: Option<(usize, usize)> = None;
  if chunk_size != 0 {
    // This limitation is not specified but the 32-bit Brotli decoder for now.
    if chunk_size > 1073741823 {
      return Err(());
    }
    if reader.pos + chunk_size as usize > data.len() {
      return Err(());
    }
    prefix = Some((reader.pos, chunk_size as usize));
    reader.pos += chunk_size as usize;
  }
  let num_word_lists = reader.read_u8()?;
  if num_word_lists as usize > SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS {
    return Err(());
  }
  for _ in 0..num_word_lists {
    parse_word_list(&mut reader, None)?;
  }
  let num_transform_lists = reader.read_u8()?;
  if num_transform_lists as usize > SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS {
    return Err(());
  }
  for _ in 0..num_transform_lists {
    parse_transforms_list(&mut reader, None)?;
  }
  Ok(ParsedSerializedDictionary {
    prefix: prefix,
    num_word_lists: num_word_lists,
    num_transform_lists: num_transform_lists,
  })
}

// Second pass: fills `dict` (everything except the blob, which the caller
// moves in afterwards) using a pre-allocated arena of
// num_word_lists * WORD_LIST_STRIDE + num_transform_lists * TRANSFORM_LIST_STRIDE
// zero-initialized u32s. Must be called with the summary returned by
// dry_parse_serialized_dictionary for the same data.
pub(crate) fn parse_serialized_dictionary_into<AllocU8: alloc::Allocator<u8>,
                                               AllocU32: alloc::Allocator<u32>>(
    data: &[u8],
    summary: &ParsedSerializedDictionary,
    dict: &mut BrotliSharedDictionary<AllocU8, AllocU32>)
    -> Result<(), ()> {
  if data.len() < 2 || data[0] != 0x91 || data[1] != 0 {
    return Err(());
  }
  if summary.num_word_lists as usize > SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS ||
     summary.num_transform_lists as usize > SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS {
    return Err(());
  }
  let arena_size = (summary.num_word_lists as usize)
      .checked_mul(WORD_LIST_STRIDE)
      .and_then(|word_size| {
        (summary.num_transform_lists as usize)
            .checked_mul(TRANSFORM_LIST_STRIDE)
            .and_then(|transform_size| word_size.checked_add(transform_size))
      })
      .ok_or(())?;
  if dict.meta.slice().len() != arena_size {
    return Err(());
  }

  let mut reader = Reader { data: data, pos: 2 };
  let chunk_size = reader.read_varint32()?;
  if chunk_size > 1073741823 {
    return Err(());
  }
  let prefix = if chunk_size == 0 {
    None
  } else {
    Some((reader.pos, chunk_size as usize))
  };
  reader.pos = reader.pos.checked_add(chunk_size as usize).ok_or(())?;
  if reader.pos > data.len() || prefix != summary.prefix {
    return Err(());
  }
  let num_word_lists = reader.read_u8()?;
  if num_word_lists != summary.num_word_lists {
    return Err(());
  }
  dict.num_word_lists = num_word_lists;
  for i in 0..num_word_lists as usize {
    let start = i * WORD_LIST_STRIDE;
    parse_word_list(&mut reader,
                    Some(&mut dict.meta.slice_mut()[start..start + WORD_LIST_STRIDE]))?;
  }
  let num_transform_lists = reader.read_u8()?;
  if num_transform_lists != summary.num_transform_lists {
    return Err(());
  }
  dict.num_transform_lists = num_transform_lists;
  let tl_base = num_word_lists as usize * WORD_LIST_STRIDE;
  for i in 0..num_transform_lists as usize {
    let start = tl_base + i * TRANSFORM_LIST_STRIDE;
    parse_transforms_list(&mut reader,
                          Some(&mut dict.meta.slice_mut()[start..start + TRANSFORM_LIST_STRIDE]))?;
  }
  if num_word_lists != 0 || num_transform_lists != 0 {
    let num_dictionaries = reader.read_u8()?;
    if num_dictionaries == 0 ||
       num_dictionaries as usize > SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS {
      return Err(());
    }
    dict.num_dictionaries = num_dictionaries;
    for i in 0..num_dictionaries as usize {
      let words_index = reader.read_u8()?;
      if words_index > num_word_lists {
        return Err(());
      }
      let transforms_index = reader.read_u8()?;
      if transforms_index > num_transform_lists {
        return Err(());
      }
      dict.words_index[i] = words_index;
      dict.transforms_index[i] = transforms_index;
    }
    dict.context_based = reader.read_bool()?;
    if dict.context_based {
      for i in 0..SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS {
        let context_value = reader.read_u8()?;
        if context_value >= num_dictionaries {
          return Err(());
        }
        dict.context_map[i] = context_value;
      }
    }
  } else {
    dict.context_based = false;
    dict.num_dictionaries = 1;
    // words_index[0] == num_word_lists == 0 selects the built-in dictionary,
    // and likewise for transforms; that is what Default already encodes.
  }
  Ok(())
}

#[cfg(all(test, feature="std"))]
mod tests {
  use super::*;
  use alloc_stdlib::StandardAlloc;
  use std::vec::Vec;

  // Builds a transform list with stringlets ["up", "ing", ""] and transforms
  // [["", IDENTITY, ""], ["", UPPERCASE_ALL, ""], ["up", IDENTITY, "ing"],
  //  ["", SHIFT_FIRST, ""] param 3, ["", SHIFT_ALL, ""] param 3,
  //  ["", OMIT_LAST_2, ""], ["", OMIT_FIRST_2, ""]].
  fn transform_list_bytes() -> Vec<u8> {
    let stringlets = b"\x02up\x03ing\x00";
    let mut out = Vec::<u8>::new();
    out.push(stringlets.len() as u8);
    out.push(0);
    out.extend_from_slice(stringlets);
    // [prefix_id, transform_type, suffix_id] -- see transform_triplet_at for
    // why this is the C implementation's order and not the draft's.
    let transforms: &[[u8; 3]] = &[[2, 0, 2], [2, 11, 2], [0, 0, 1],
                                   [2, BROTLI_TRANSFORM_SHIFT_FIRST, 2],
                                   [2, BROTLI_TRANSFORM_SHIFT_ALL, 2],
                                   [2, 2, 2], [2, 13, 2]];
    out.push(transforms.len() as u8);
    for t in transforms.iter() {
      out.extend_from_slice(t);
    }
    let params: &[u16] = &[0, 0, 0, 3, 3, 0, 0];
    for p in params.iter() {
      out.push(*p as u8);
      out.push((*p >> 8) as u8);
    }
    out
  }

  fn serialized_with_custom_lists() -> Vec<u8> {
    let mut blob = Vec::<u8>::new();
    blob.extend_from_slice(&[0x91, 0x00]);
    blob.push(0); // no LZ77 prefix
    blob.push(1); // NUM_WORD_LISTS
    let mut size_bits = [0u8; 28]; // lengths 4..=31
    size_bits[0] = 1; // two words of length 4
    blob.extend_from_slice(&size_bits);
    blob.extend_from_slice(b"frobquxx");
    blob.push(1); // NUM_TRANSFORM_LISTS
    blob.extend_from_slice(&transform_list_bytes());
    blob.push(1); // NUM_DICTIONARIES
    blob.push(0); // words index
    blob.push(0); // transforms index
    blob.push(0); // CONTEXT_ENABLED = false
    blob
  }

  fn parse(blob: &[u8]) -> Result<BrotliSharedDictionary<StandardAlloc, StandardAlloc>, ()> {
    let summary = dry_parse_serialized_dictionary(blob)?;
    let mut dict = BrotliSharedDictionary::<StandardAlloc, StandardAlloc>::default();
    let arena = summary.num_word_lists as usize * WORD_LIST_STRIDE +
                summary.num_transform_lists as usize * TRANSFORM_LIST_STRIDE;
    let mut alloc = StandardAlloc::default();
    dict.meta = <StandardAlloc as Allocator<u32>>::alloc_cell(&mut alloc, arena);
    parse_serialized_dictionary_into(blob, &summary, &mut dict)?;
    let mut blob_mem = <StandardAlloc as Allocator<u8>>::alloc_cell(&mut alloc, blob.len());
    blob_mem.slice_mut().clone_from_slice(blob);
    dict.blob = state::MaybeOwnedSlice::Owned(blob_mem);
    Ok(dict)
  }

  #[test]
  fn test_parse_and_lookup_custom_words() {
    let dict = parse(&serialized_with_custom_lists()[..]).unwrap();
    assert!(dict.is_custom());
    assert_eq!(dict.num_dictionaries, 1);
    let words = dict.words_of(0);
    assert_eq!(words.size_bits_by_length(4), 1);
    assert_eq!(words.size_bits_by_length(8), 0);
    assert_eq!(words.word(4, 0), b"frob");
    assert_eq!(words.word(4, 1), b"quxx");
    let transforms = dict.transforms_of(0);
    assert_eq!(transforms.num_transforms(), 7);
    assert_eq!(transforms.cutoff_identity(), 0);
  }

  #[test]
  fn test_custom_transforms_apply() {
    let dict = parse(&serialized_with_custom_lists()[..]).unwrap();
    let transforms = dict.transforms_of(0);
    let mut dst = [0u8; 64];
    // identity
    assert_eq!(transforms.apply(&mut dst, b"frob", 4, 0), 4);
    assert_eq!(&dst[..4], b"frob");
    // uppercase all
    assert_eq!(transforms.apply(&mut dst, b"frob", 4, 1), 4);
    assert_eq!(&dst[..4], b"FROB");
    // prefix "up", suffix "ing"
    assert_eq!(transforms.apply(&mut dst, b"frob", 4, 2), 9);
    assert_eq!(&dst[..9], b"upfrobing");
    // shift first by 3: 'f' -> 'i'
    assert_eq!(transforms.apply(&mut dst, b"frob", 4, 3), 4);
    assert_eq!(&dst[..4], b"irob");
    // shift all by 3
    assert_eq!(transforms.apply(&mut dst, b"frob", 4, 4), 4);
    assert_eq!(&dst[..4], b"iure");
    // omit last 2
    assert_eq!(transforms.apply(&mut dst, b"frob", 4, 5), 2);
    assert_eq!(&dst[..2], b"fr");
    // omit first 2
    assert_eq!(transforms.apply(&mut dst, b"frob", 4, 6), 2);
    assert_eq!(&dst[..2], b"ob");
  }

  #[test]
  fn test_shift_all_multibyte_utf8() {
    let dict = parse(&serialized_with_custom_lists()[..]).unwrap();
    let transforms = dict.transforms_of(0);
    // U+00E9 (0xC3 0xA9) shifted by +3 is U+00EC (0xC3 0xAC).
    let mut dst = [0u8; 64];
    let len = transforms.apply(&mut dst, &[0xC3, 0xA9, b'a', b'b'], 4, 4);
    assert_eq!(len, 4);
    assert_eq!(&dst[..4], &[0xC3, 0xAC, b'd', b'e']);
  }

  #[test]
  fn test_parse_rejects_malformed() {
    // bad magic
    assert!(dry_parse_serialized_dictionary(&[0x90, 0x00, 0x00, 0x00, 0x00]).is_err());
    // truncated
    assert!(dry_parse_serialized_dictionary(&[0x91]).is_err());
    // prefix chunk larger than the data
    assert!(dry_parse_serialized_dictionary(&[0x91, 0x00, 0x10, 0x00, 0x00]).is_err());
    // size_bits over the limit
    let mut blob = serialized_with_custom_lists();
    blob[4] = 16; // size_bits for length 4
    assert!(dry_parse_serialized_dictionary(&blob[..]).is_err());
    // transform type out of range
    let mut blob = serialized_with_custom_lists();
    let pos = blob.iter().position(|&b| b == BROTLI_TRANSFORM_SHIFT_ALL).unwrap();
    blob[pos] = BROTLI_NUM_TRANSFORM_TYPES;
    assert!(dry_parse_serialized_dictionary(&blob[..]).is_err());
    // nonzero param for a non-shift transform
    let mut blob = serialized_with_custom_lists();
    let len = blob.len();
    blob[len - 17] = 1; // first param byte (identity transform)
    assert!(dry_parse_serialized_dictionary(&blob[..]).is_err());
    // dictionary index out of range (validated by the full parse)
    let mut blob = serialized_with_custom_lists();
    let len = blob.len();
    blob[len - 3] = 2; // words index; only one word list, so 2 > 1 is invalid
    assert!(parse(&blob[..]).is_err());
  }

  #[test]
  fn test_second_pass_rejects_mismatched_summary_before_arena_access() {
    let blob = serialized_with_custom_lists();
    // Deliberately claim there are no lists and provide the corresponding
    // zero-length arena. The count must be rejected before list 0 is indexed.
    let summary = ParsedSerializedDictionary {
      prefix: None,
      num_word_lists: 0,
      num_transform_lists: 0,
    };
    let mut dict = BrotliSharedDictionary::<StandardAlloc, StandardAlloc>::default();
    assert!(parse_serialized_dictionary_into(&blob, &summary, &mut dict).is_err());

    // A matching summary with a short arena is likewise a normal parse error,
    // not an indexing panic.
    let summary = dry_parse_serialized_dictionary(&blob).unwrap();
    let arena_size = summary.num_word_lists as usize * WORD_LIST_STRIDE +
                     summary.num_transform_lists as usize * TRANSFORM_LIST_STRIDE;
    let mut alloc = StandardAlloc::default();
    dict.meta = <StandardAlloc as Allocator<u32>>::alloc_cell(
        &mut alloc, arena_size - 1);
    assert!(parse_serialized_dictionary_into(&blob, &summary, &mut dict).is_err());
  }

  #[test]
  fn test_parse_prefix_only_is_not_custom() {
    let mut blob = Vec::<u8>::new();
    blob.extend_from_slice(&[0x91, 0x00]);
    blob.push(4);
    blob.extend_from_slice(b"wxyz");
    blob.push(0);
    blob.push(0);
    let summary = dry_parse_serialized_dictionary(&blob[..]).unwrap();
    assert_eq!(summary.prefix, Some((3, 4)));
    assert_eq!(summary.num_word_lists, 0);
    assert_eq!(summary.num_transform_lists, 0);
  }
}
