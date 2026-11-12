#[cfg(test)]

use super::*;



#[test]
fn invalid_huffman_inputs_return_errors() {
  let mut table = [HuffmanCode::default(); 8];
  let values = [0u16; 4];
  assert_eq!(BrotliBuildSimpleHuffmanTable(&mut table, 3, &values, 5), 0);
  assert_eq!(BrotliBuildSimpleHuffmanTable(&mut table[..4], 3, &values, 4), 0);

  let code_lengths = [0u8; 18];
  let count = [0u16; 6];
  assert!(!BrotliBuildCodeLengthsHuffmanTable(
    &mut table,
    &code_lengths,
    &count,
  ));
}

// Each test below drives one rejection path in this module. They are white-box
// on purpose: none of these conditions is reachable through
// BrotliDecompressStream, so a direct call is the only way to exercise them.
#[cfg(test)]
mod guard_tests {
  // Maximum possible Huffman table size for an alphabet size of (index * 32),
  // max code length 15 and root table bits 8.
  // pub const kMaxHuffmanTableSize : [u16;23] = [
  // 256, 402, 436, 468, 500, 534, 566, 598, 630, 662, 694, 726, 758, 790, 822,
  // 854, 886, 920, 952, 984, 1016, 1048, 1080, 1112, 1144,1176,1208,1240,272,
  // 1304, 1336, 1368, 1400, 1432, 1464, 1496, 1528];
  //
  const BROTLI_HUFFMAN_MAX_TABLE_SIZE: usize = 1080;
  use super::super::*;

  fn code() -> HuffmanCode {
    HuffmanCode { bits: 1, value: 7 }
  }

  #[test]
  fn replicate_value_rejects_degenerate_step_and_end() {
    let mut table = [HuffmanCode::default(); 32];
    assert!(!ReplicateValue(&mut table, 0, 0, 8, code()));    // step == 0
    assert!(!ReplicateValue(&mut table, 0, -2, 8, code()));   // step < 0
    assert!(!ReplicateValue(&mut table, 0, 2, 0, code()));    // end == 0
    assert!(!ReplicateValue(&mut table, 0, 2, -4, code()));   // end < 0
    assert!(!ReplicateValue(&mut table, 0, 3, 8, code()));    // end % step != 0
    assert_eq!(table[..], [HuffmanCode::default(); 32][..]);
  }

  #[test]
  fn replicate_value_rejects_extent_past_table() {
    let mut table = [HuffmanCode::default(); 8];
    // Last index written would be offset + end - step == 2 + 8 - 2 == 8.
    assert!(!ReplicateValue(&mut table, 2, 2, 8, code()));
    assert_eq!(table[..], [HuffmanCode::default(); 8][..]);
    // One less, and it just fits.
    assert!(ReplicateValue(&mut table, 1, 2, 8, code()));
    assert_eq!(table[7], code());
  }

  #[test]
  fn next_table_bit_size_rejects_short_count() {
    // The scan runs len up to BROTLI_HUFFMAN_MAX_CODE_LENGTH, past whatever
    // the caller's max_length was, so count can run out underneath it.
    let short = [0u16; 10];
    assert_eq!(NextTableBitSize(&short, 9, 8), None);
    // Long enough, and it completes.
    let full = [0u16; BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1];
    assert!(NextTableBitSize(&full, 9, 8).is_some());
  }

  #[test]
  fn symbol_list_value_rejects_out_of_range_index() {
    let list = [1u16, 2, 3, 4];
    assert_eq!(symbol_list_value(&list, 2, -3), None); // before the start
    assert_eq!(symbol_list_value(&list, 2, 2), None);  // past the end
    assert_eq!(symbol_list_value(&list, 2, -2), Some(1));
    assert_eq!(symbol_list_value(&list, 2, 1), Some(4));
  }

  #[test]
  fn code_lengths_table_rejects_undersized_slices() {
    let mut table = [HuffmanCode::default(); 32];
    let code_lengths = [0u8; 18];
    let count = [0u16; 6];
    // table shorter than 1 << BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH
    assert!(!BrotliBuildCodeLengthsHuffmanTable(&mut table[..31], &code_lengths, &count));
    // fewer than 18 code lengths
    assert!(!BrotliBuildCodeLengthsHuffmanTable(&mut table, &code_lengths[..17], &count));
    // histogram too short to index by code length
    assert!(!BrotliBuildCodeLengthsHuffmanTable(&mut table, &code_lengths, &count[..6 - 1]));
  }

  #[test]
  fn code_lengths_table_rejects_out_of_range_code_length() {
    let mut table = [HuffmanCode::default(); 32];
    let mut code_lengths = [0u8; 18];
    // 6 is past the end of the length histogram; without this check the
    // sort below would index `offset` out of bounds.
    code_lengths[3] = BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH as u8 + 1;
    let count = [0u16; 6];
    assert!(!BrotliBuildCodeLengthsHuffmanTable(&mut table, &code_lengths, &count));
  }

  #[test]
  fn code_lengths_table_rejects_histogram_mismatch() {
    let mut table = [HuffmanCode::default(); 32];
    let mut code_lengths = [0u8; 18];
    code_lengths[0] = 1;
    code_lengths[1] = 1;
    // The true histogram is count[1] == 2; claiming anything else would send
    // the sort loop outside `sorted`.
    let mut count = [0u16; 6];
    count[1] = 3;
    assert!(!BrotliBuildCodeLengthsHuffmanTable(&mut table, &code_lengths, &count));
    count[1] = 2;
    assert!(BrotliBuildCodeLengthsHuffmanTable(&mut table, &code_lengths, &count));
  }

  // A symbol_lists image whose head region (the 16 entries before the offset)
  // is all 0xFFFF except for the length the caller wants to report.
  fn symbol_lists_with(max_len: usize, symbol: u16)
      -> ([u16; BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1 + 8], usize) {
    let mut list = [0xFFFFu16; BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1 + 8];
    let offset = BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1;
    list[offset - (BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1 - max_len)] = symbol;
    (list, offset)
  }

  #[test]
  fn build_huffman_table_rejects_bad_root_bits() {
    let (list, offset) = symbol_lists_with(1, 0);
    let mut table = [HuffmanCode::default(); BROTLI_HUFFMAN_MAX_TABLE_SIZE as usize];
    let mut count = [0u16; BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1];
    count[1] = 2;
    assert_eq!(BrotliBuildHuffmanTable(&mut table, 0, &list, offset, &mut count), 0);
    assert_eq!(BrotliBuildHuffmanTable(&mut table, -1, &list, offset, &mut count), 0);
    assert_eq!(
      BrotliBuildHuffmanTable(&mut table, BROTLI_REVERSE_BITS_MAX as i32 + 1, &list, offset,
                              &mut count),
      0);
    // BROTLI_HUFFMAN_MAX_CODE_LENGTH - root_bits must also fit the reversal.
    assert_eq!(
      BrotliBuildHuffmanTable(&mut table,
                              BROTLI_HUFFMAN_MAX_CODE_LENGTH as i32
                                - BROTLI_REVERSE_BITS_MAX as i32 - 1,
                              &list, offset, &mut count),
      0);
  }

  #[test]
  fn build_huffman_table_rejects_offset_past_symbol_lists() {
    let (list, _) = symbol_lists_with(1, 0);
    let mut table = [HuffmanCode::default(); BROTLI_HUFFMAN_MAX_TABLE_SIZE as usize];
    let mut count = [0u16; BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1];
    assert_eq!(BrotliBuildHuffmanTable(&mut table, 8, &list, list.len(), &mut count), 0);
  }

  #[test]
  fn build_huffman_table_rejects_head_scan_underflow() {
    // Every head entry is 0xFFFF, so the scan walks off the front of the
    // slice instead of finding a longest code length.
    let list = [0xFFFFu16; BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1 + 8];
    let mut table = [HuffmanCode::default(); BROTLI_HUFFMAN_MAX_TABLE_SIZE as usize];
    let mut count = [0u16; BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1];
    assert_eq!(BrotliBuildHuffmanTable(&mut table, 8, &list, 4, &mut count), 0);
  }

  #[test]
  fn build_huffman_table_rejects_count_shorter_than_max_length() {
    let (list, offset) = symbol_lists_with(9, 0);
    let mut table = [HuffmanCode::default(); BROTLI_HUFFMAN_MAX_TABLE_SIZE as usize];
    // max_length is 9, so a histogram of 9 entries cannot be indexed by it.
    let mut count = [0u16; 9];
    assert_eq!(BrotliBuildHuffmanTable(&mut table, 8, &list, offset, &mut count), 0);
  }

  #[test]
  fn build_huffman_table_rejects_undersized_root_table() {
    let (list, offset) = symbol_lists_with(1, 0);
    let mut count = [0u16; BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1];
    count[1] = 2;
    // root_bits 8 wants a 256-entry root table; give it far less and the
    // replication/ReplicateValue bounds must reject rather than write past.
    // It may write valid entries before it runs out of room; what matters is
    // that it reports failure instead of writing past the end.
    let mut small = [HuffmanCode::default(); 4];
    assert_eq!(BrotliBuildHuffmanTable(&mut small, 8, &list, offset, &mut count), 0);
  }

  // With symbol_lists_offset == BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1, the head
  // entry for code length L sits at index L, and each entry chains to the next
  // symbol of that length. 0xFFFF means "no symbol of this length".
  const OFFSET: usize = BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1;
  fn heads() -> [u16; 32] {
    [0xFFFF; 32]
  }
  fn counts() -> [u16; BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1] {
    [0u16; BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1]
  }

  // root_bits is pinned to 7..=8 by the entry check (both root_bits and
  // BROTLI_HUFFMAN_MAX_CODE_LENGTH - root_bits must fit the 8-bit reversal),
  // so these all use the real HUFFMAN_TABLE_BITS of 8. A max_length of 9 is
  // then what forces a second-level table.
  const ROOT_BITS: i32 = 8;

  #[test]
  fn build_huffman_table_rejects_symbol_chain_leaving_the_list() {
    // Root table pass: the second symbol of length 1 is chained to via
    // symbol_lists[OFFSET + 100], which is past the end of this list.
    let mut list = heads();
    list[9] = 0;   // longest code length is 9
    list[1] = 100; // ...and the length-1 chain leaves the list
    let mut count = counts();
    count[1] = 2;
    let mut table = [HuffmanCode::default(); BROTLI_HUFFMAN_MAX_TABLE_SIZE as usize];
    assert_eq!(BrotliBuildHuffmanTable(&mut table, ROOT_BITS, &list, OFFSET, &mut count), 0);
  }

  #[test]
  fn build_huffman_table_rejects_second_level_count_shorter_than_the_scan() {
    // A second-level table is needed (max_length 9 > root_bits 8), and
    // NextTableBitSize then scans past the end of this 10-entry histogram.
    let mut list = heads();
    list[9] = 0;
    let mut count = [0u16; 10];
    count[9] = 1;
    let mut table = [HuffmanCode::default(); BROTLI_HUFFMAN_MAX_TABLE_SIZE as usize];
    assert_eq!(BrotliBuildHuffmanTable(&mut table, ROOT_BITS, &list, OFFSET, &mut count), 0);
  }

  #[test]
  fn build_huffman_table_rejects_second_level_link_past_root_table() {
    // The link back into the root table is written at root_table[sub_key];
    // an empty root table has nowhere to put it.
    let mut list = heads();
    list[9] = 0;
    let mut count = counts();
    count[9] = 1;
    assert_eq!(BrotliBuildHuffmanTable(&mut [], ROOT_BITS, &list, OFFSET, &mut count), 0);
  }

  #[test]
  fn build_huffman_table_rejects_second_level_replicate_past_table() {
    let mut list = heads();
    list[9] = 5;
    let mut count = counts();
    count[9] = 2;
    // One entry is enough for the root link but not for the sub-table, which
    // starts at table_free_offset == 256.
    let mut table = [HuffmanCode::default(); 1];
    assert_eq!(BrotliBuildHuffmanTable(&mut table, ROOT_BITS, &list, OFFSET, &mut count), 0);
  }

  #[test]
  fn build_huffman_table_rejects_second_level_symbol_chain_leaving_the_list() {
    let mut list = heads();
    list[9] = 100; // chains past the end when the second symbol is fetched
    let mut count = counts();
    count[9] = 2;
    let mut table = [HuffmanCode::default(); BROTLI_HUFFMAN_MAX_TABLE_SIZE as usize];
    assert_eq!(BrotliBuildHuffmanTable(&mut table, ROOT_BITS, &list, OFFSET, &mut count), 0);
  }

  // The 2nd-level link is stored as a u16 offset in HuffmanCode::value, so a
  // table that grows past u16::MAX cannot be represented. Reaching it takes a
  // histogram claiming tens of thousands of codes at one length, hence the
  // heap-sized root table.
  #[cfg(feature="std")]
  #[test]
  fn build_huffman_table_rejects_second_level_offset_past_u16() {
    let mut list = heads();
    // A self-referential length-9 chain, so symbols never run out.
    list[9] = 9;
    list[OFFSET + 9] = 9;
    let mut count = counts();
    count[9] = u16::MAX;
    let mut table = std::vec::Vec::new();
    table.resize(1usize << 17, HuffmanCode::default());
    assert_eq!(BrotliBuildHuffmanTable(&mut table[..], ROOT_BITS, &list, OFFSET, &mut count), 0);
  }

  #[test]
  fn simple_table_rejects_bad_root_bits_and_symbol_count() {
    let mut table = [HuffmanCode::default(); 8];
    let val = [0u16; 4];
    assert_eq!(BrotliBuildSimpleHuffmanTable(&mut table, 0, &val, 0), 0);
    assert_eq!(BrotliBuildSimpleHuffmanTable(&mut table, -1, &val, 0), 0);
    assert_eq!(BrotliBuildSimpleHuffmanTable(&mut table, 32, &val, 0), 0);
    // num_symbols is only 0..=4; 5 and up are rejected by the same match that
    // decides how many entries of `val` are read.
    assert_eq!(BrotliBuildSimpleHuffmanTable(&mut table, 3, &val, 5), 0);
    assert_eq!(BrotliBuildSimpleHuffmanTable(&mut table, 3, &val, u32::MAX), 0);
  }

  #[test]
  fn simple_table_rejects_short_val_and_table() {
    let mut table = [HuffmanCode::default(); 8];
    let val = [0u16; 4];
    assert_eq!(BrotliBuildSimpleHuffmanTable(&mut table, 3, &val[..3], 4), 0);
    assert_eq!(BrotliBuildSimpleHuffmanTable(&mut table, 3, &val[..1], 1), 0);
    assert_eq!(BrotliBuildSimpleHuffmanTable(&mut table[..7], 3, &val, 4), 0);
  }
}

#[test]
fn code_length_ht() {
  let code_lengths: [u8; 19] = [0, 2, 3, 0, 2, 3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
  let count: [u16; 6] = [0, 0, 3, 2, 0, 0];
  let mut table: [HuffmanCode; 1 << BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH] =
    [HuffmanCode {
      value: 0,
      bits: 0,
    }; 1 << BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH];
  BrotliBuildCodeLengthsHuffmanTable(&mut table, &code_lengths, &count);
  let end_table: [HuffmanCode; 1 << BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH] = [HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 1,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 6,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 2,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 1,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 6,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 5,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 1,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 6,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 2,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 1,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 6,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 5,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 1,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 6,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 2,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 1,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 6,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 5,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 1,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 6,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 2,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 1,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 6,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 5,
                                                                                   }];
  for index in 0..end_table.len() {
    assert_eq!([index, table[index].bits as usize, table[index].value as usize],
               [index, end_table[index].bits as usize, end_table[index].value as usize]);
    assert!(table[index] == end_table[index]);
  }
}
#[test]
fn code_length_stringy_ht() {
  let code_lengths: [u8; 19] = [1, 0, 3, 3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
  let count: [u16; 6] = [0, 1, 1, 2, 0, 0];
  let mut table: [HuffmanCode; 1 << BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH] =
    [HuffmanCode {
      value: 0,
      bits: 0,
    }; 1 << BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH];
  BrotliBuildCodeLengthsHuffmanTable(&mut table, &code_lengths, &count);
  let end_table: [HuffmanCode; 1 << BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH] = [HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 2,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 3,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 2,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 3,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 2,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 3,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 2,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 2,
                                                                                     value: 4,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 1,
                                                                                     value: 0,
                                                                                   },
                                                                                   HuffmanCode {
                                                                                     bits: 3,
                                                                                     value: 3,
                                                                                   }];
  for index in 0..end_table.len() {
    assert_eq!([index, table[index].bits as usize, table[index].value as usize],
               [index, end_table[index].bits as usize, end_table[index].value as usize]);
    assert!(table[index] == end_table[index]);
  }
}
#[test]
fn code_length_single_value_ht() {
  let code_lengths: [u8; 19] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0];
  let count: [u16; 6] = [0, 1, 0, 0, 0, 0];
  let mut table: [HuffmanCode; 1 << BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH] =
    [HuffmanCode {
      value: 0,
      bits: 0,
    }; 1 << BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH];
  BrotliBuildCodeLengthsHuffmanTable(&mut table, &code_lengths, &count);
  let end_table: [HuffmanCode; 1 << BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH] =
    [HuffmanCode {
      bits: 0,
      value: 9,
    }; 1 << BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH];
  for index in 0..end_table.len() {
    assert_eq!([index, table[index].bits as usize, table[index].value as usize],
               [index, end_table[index].bits as usize, end_table[index].value as usize]);
    assert!(table[index] == end_table[index]);
  }
}

#[test]
fn complex() {
  let symbol_array: [u16; BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1 +
                          BROTLI_HUFFMAN_MAX_CODE_LENGTHS_SIZE] =
    [65535, 65535, 65535, 72, 15, 0, 65, 5, 6, 66, 19, 65535, 65535, 65535, 65535, 65535, 1, 2, 3,
     4, 57, 16, 7, 8, 9, 10, 11, 12, 13, 14, 17, 73, 69, 18, 64, 20, 21, 22, 23, 24, 25, 26, 27,
     28, 37, 31, 47, 34, 36, 103, 37, 36, 44, 38, 39, 40, 41, 49, 43, 46, 48, 50, 51, 50, 49, 138,
     55, 58, 68, 61, 56, 57, 62, 77, 88, 66, 76, 71, 63, 64, 67, 68, 127, 71, 117, 114, 73, 74,
     77, 76, 102, 86, 116, 255, 79, 80, 84, 82, 83, 87, 92, 93, 94, 95, 89, 90, 91, 96, 109, 117,
     102, 110, 97, 98, 99, 100, 122, 149, 115, 104, 105, 106, 107, 108, 167, 116, 111, 112, 113,
     114, 128, 118, 232, 254, 119, 120, 121, 127, 123, 142, 132, 133, 129, 129, 136, 130, 131,
     132, 133, 134, 135, 137, 224, 233, 139, 140, 141, 142, 143, 144, 162, 146, 147, 148, 169,
     157, 151, 152, 153, 154, 155, 156, 158, 181, 159, 160, 161, 162, 184, 164, 174, 180, 167,
     182, 212, 170, 171, 172, 190, 201, 175, 176, 177, 178, 179, 180, 188, 194, 183, 184, 185,
     186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203,
     236, 207, 221, 216, 208, 209, 210, 211, 214, 213, 220, 215, 218, 217, 219, 222, 223, 234,
     227, 229, 224, 235, 226, 231, 228, 244, 230, 233, 232, 255, 253, 237, 251, 237, 238, 239,
     240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 252, 252, 253, 254, 255, 0, 23040,
     3500, 1, 0, 0, 0, 0, 0, 8192, 3500, 1, 0, 0, 0, 0, 0, 16, 0, 0, 0, 38400, 3500, 1, 0, 61712,
     21033, 32767, 0, 60189, 35058, 32767, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0,
     51712, 3501, 1, 0, 15360, 0, 0, 0, 1536, 3502, 1, 0, 8192, 3500, 1, 0, 61792, 21033, 32767,
     0, 61458, 35058, 32767, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4102, 0, 0, 0, 38400, 3500, 1, 0, 0, 0, 0,
     0, 256, 0, 0, 0, 0, 0, 0, 0, 8192, 3500, 1, 0, 61792, 21033, 32767, 0, 18208, 35059, 32767,
     0, 4102, 0, 0, 65408, 65535, 0, 0, 0, 0, 0, 0, 0, 4102, 0, 0, 0, 16, 0, 0, 0, 9, 0, 0, 0,
     32768, 54143, 32705, 0, 0, 54016, 32705, 0, 0, 54016, 32705, 0, 3584, 0, 0, 0, 6, 0, 0,
     65408, 7, 3502, 32767, 0, 8192, 3500, 1, 0, 0, 54016, 32705, 0, 1, 0, 0, 0, 8192, 3500, 1, 0,
     12288, 54016, 32705, 0, 8192, 54016, 32705, 0, 61888, 21033, 32767, 0, 14237, 35059, 32767,
     0, 88, 53952, 32705, 0, 4607, 0, 0, 0, 8, 0, 0, 0, 4608, 54016, 32705, 0, 8192, 3500, 1, 0,
     8192, 3500, 1, 0, 62016, 21033, 32767, 0, 16544, 35059, 32767, 0, 4096, 0, 0, 0, 8192, 3500,
     1, 0, 61936, 21033, 32767, 0, 12502, 35059, 32767, 0, 0, 0, 0, 0, 43957, 36820, 32767, 0, 0,
     0, 0, 0, 44144, 37434, 32767, 0, 19984, 53952, 32705, 0, 0, 0, 0, 0, 62076, 21033, 32767, 0,
     62080, 21033, 32767, 0, 62084, 21033, 32767, 0, 64, 53952, 32705, 0, 65535, 65535, 65535,
     65535, 64, 0, 8296, 1, 62032, 21033, 32767, 0, 16900, 27601, 32767, 0, 18096, 27604, 32767,
     0, 0, 0, 0, 0, 62080, 21033, 32767, 0, 17447, 27601, 32767, 0, 62080, 21033, 32767, 0, 17246,
     27601, 32767, 0, 0, 0, 0, 0, 62784, 36821, 32767, 0, 62144, 21033, 32767, 0, 4085, 27601,
     32767, 0, 44624, 27603, 32767, 0, 62784, 36821, 32767, 0, 10112, 30246, 32767, 0, 46488,
     27603, 32767, 0, 18096, 27604, 32767, 0, 0, 0, 0, 0, 62256, 21033, 32767, 0, 23819, 27601,
     32767, 0, 62288, 21033, 32767, 0, 20326, 27600, 32767, 0, 15584, 36820, 32767, 0, 15096,
     36820, 32767, 0, 12288, 36820, 32767, 0, 15120, 36820, 32767, 0, 42681, 21547, 0, 0, 18096,
     27604, 32767, 0, 62400, 21033, 32767, 0, 1299, 0, 0, 0, 51315, 34237, 23510, 56320, 18096,
     27604, 32767, 0, 62400, 21033, 32767, 0, 11120, 27601, 32767, 0, 13272, 27604, 32767, 0, 28,
     0, 0, 0, 62384, 21033, 32767, 0, 10134, 27601, 32767, 0, 11072, 30235, 32767, 0, 0, 0, 50, 0];
  let mut counts: [u16; 11] = [0, 0, 0, 1, 6, 7, 4, 9, 17, 12, 60];
  let end_counts: [u16; 11] = [0, 0, 0, 1, 6, 7, 4, 9, 17, 0, 0];
  let mut table: [HuffmanCode; 328] = [HuffmanCode {
    bits: 0,
    value: 0,
  }; 328];
  let size = BrotliBuildHuffmanTable(&mut table, 8, &symbol_array, 16, &mut counts);
  assert_eq!(size, 328);
  assert_eq!(counts, end_counts);
  let end_table: [HuffmanCode; 328] = [HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 117,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 4,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 12,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 2,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 128,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 77,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 9,
                                         value: 251,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 5,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 57,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 74,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 251,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 65,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 265,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 254,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 4,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 18,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 2,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 224,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 77,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 233,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 69,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 57,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 9,
                                         value: 203,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 8,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 68,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 249,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 117,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 4,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 14,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 2,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 136,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 77,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 193,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 16,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 57,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 115,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 65,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 209,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 254,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 4,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 67,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 2,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 235,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 77,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 177,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 114,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 57,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 9,
                                         value: 143,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 68,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 193,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 117,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 4,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 13,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 2,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 128,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 77,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 125,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 5,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 57,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 102,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 251,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 65,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 141,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 254,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 4,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 64,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 2,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 224,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 77,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 109,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 69,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 57,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 9,
                                         value: 77,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 9,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 68,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 125,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 117,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 4,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 17,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 2,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 136,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 77,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 69,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 16,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 57,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 9,
                                         value: 41,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 7,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 65,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 85,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 254,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 4,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 71,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 2,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 235,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 77,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 53,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 114,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 57,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 9,
                                         value: 17,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 11,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 68,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 69,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 66,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 127,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 129,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 130,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 131,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 132,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 133,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 134,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 135,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 233,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 19,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 21,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 20,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 22,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 23,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 25,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 24,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 26,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 27,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 37,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 38,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 39,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 41,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 40,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 49,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 138,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 140,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 139,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 141,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 142,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 144,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 143,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 162,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 184,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 186,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 185,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 187,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 188,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 190,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 189,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 191,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 194,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 193,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 195,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 196,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 198,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 197,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 199,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 200,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 202,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 201,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 203,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 236,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 238,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 237,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 239,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 240,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 242,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 241,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 243,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 244,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 246,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 245,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 247,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 248,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 250,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 249,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 252,
                                       }];
  for index in 0..end_table.len() {
    assert_eq!([index, table[index].bits as usize, table[index].value as usize],
               [index, end_table[index].bits as usize, end_table[index].value as usize]);
    assert!(table[index] == end_table[index]);
  }
}
#[test]
fn multilevel() {
  let symbol_array: [u16; BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1 +
                          BROTLI_HUFFMAN_MAX_CODE_LENGTHS_SIZE] =
    [65535, 65535, 65535, 65535, 0, 1, 3, 2, 6, 14, 17, 21, 65535, 65535, 65535, 65535, 72, 48, 4,
     8, 5, 73, 7, 9, 16, 10, 11, 12, 13, 20, 15, 47, 24, 22, 19, 21, 77, 41, 25, 24, 32, 26, 27,
     28, 29, 30, 31, 33, 40, 34, 35, 36, 37, 38, 39, 49, 56, 42, 43, 44, 45, 46, 57, 65, 64, 50,
     51, 52, 53, 54, 55, 71, 76, 58, 59, 60, 61, 62, 63, 97, 80, 66, 67, 68, 69, 78, 82, 75, 73,
     137, 75, 81, 96, 102, 138, 80, 88, 82, 83, 84, 85, 86, 87, 89, 112, 90, 91, 92, 93, 94, 95,
     117, 104, 98, 99, 100, 101, 105, 116, 109, 120, 106, 107, 108, 109, 110, 111, 118, 114, 122,
     115, 116, 170, 129, 121, 120, 128, 122, 123, 124, 125, 126, 127, 145, 136, 130, 131, 132,
     133, 134, 135, 140, 144, 139, 191, 216, 141, 142, 143, 169, 152, 146, 147, 148, 149, 150,
     151, 153, 160, 154, 155, 156, 157, 158, 159, 161, 168, 162, 163, 164, 165, 166, 167, 171,
     176, 193, 225, 177, 0, 0, 0, 0, 184, 178, 179, 180, 181, 182, 183, 185, 192, 186, 187, 188,
     189, 190, 209, 251, 200, 194, 195, 196, 197, 198, 199, 201, 208, 202, 203, 204, 205, 206,
     207, 217, 232, 210, 211, 212, 213, 214, 215, 226, 224, 218, 219, 220, 221, 222, 223, 241,
     248, 233, 227, 228, 229, 230, 231, 234, 240, 247, 235, 236, 237, 238, 239, 0, 255, 242, 243,
     244, 245, 246, 250, 0, 252, 0, 0, 0, 253, 254, 0, 0, 27136, 22, 1, 0, 0, 0, 0, 0, 12288, 22,
     1, 0, 0, 0, 0, 0, 16, 0, 0, 0, 42496, 22, 1, 0, 57680, 24511, 32767, 0, 60189, 35058, 32767,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 55808, 23, 1, 0, 15360, 0, 0, 0, 5632,
     24, 1, 0, 12288, 22, 1, 0, 57760, 24511, 32767, 0, 61458, 35058, 32767, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 4102, 0, 0, 0, 42496, 22, 1, 0, 0, 0, 0, 0, 256, 0, 0, 0, 0, 0, 0, 0, 12288, 22, 1, 0,
     57760, 24511, 32767, 0, 18208, 35059, 32767, 0, 4102, 0, 0, 65408, 65535, 0, 0, 0, 0, 0, 0,
     0, 4102, 0, 0, 0, 16, 0, 0, 0, 9, 0, 0, 0, 32768, 255, 1, 0, 0, 128, 1, 0, 0, 128, 1, 0,
     3584, 0, 0, 0, 6, 0, 0, 65408, 7, 24, 32767, 0, 12288, 22, 1, 0, 0, 128, 1, 0, 1, 0, 0, 0,
     12288, 22, 1, 0, 12288, 128, 1, 0, 8192, 128, 1, 0, 57856, 24511, 32767, 0, 14237, 35059,
     32767, 0, 88, 32, 1, 0, 4607, 0, 0, 0, 8, 0, 0, 0, 4608, 128, 1, 0, 12288, 22, 1, 0, 12288,
     22, 1, 0, 57984, 24511, 32767, 0, 16544, 35059, 32767, 0, 4096, 0, 0, 0, 12288, 22, 1, 0,
     57904, 24511, 32767, 0, 12502, 35059, 32767, 0, 0, 0, 0, 0, 43957, 36820, 32767, 0, 0, 0, 0,
     0, 44144, 37434, 32767, 0, 19984, 32, 1, 0, 0, 0, 0, 0, 58044, 24511, 32767, 0, 58048, 24511,
     32767, 0, 58052, 24511, 32767, 0, 64, 32, 1, 0, 65535, 65535, 65535, 65535, 64, 0, 8296, 1,
     58000, 24511, 32767, 0, 4612, 24513, 32767, 0, 5784, 24516, 32767, 0, 0, 0, 0, 0, 58048,
     24511, 32767, 0, 5159, 24513, 32767, 0, 58048, 24511, 32767, 0, 4958, 24513, 32767, 0, 0, 0,
     0, 0, 62784, 36821, 32767, 0, 58112, 24511, 32767, 0, 57333, 24512, 32767, 0, 32336, 24515,
     32767, 0, 62784, 36821, 32767, 0, 10112, 30246, 32767, 0, 34200, 24515, 32767, 0, 5784,
     24516, 32767, 0, 0, 0, 0, 0, 58224, 24511, 32767, 0, 11531, 24513, 32767, 0, 58256, 24511,
     32767, 0, 8038, 24512, 32767, 0, 15584, 36820, 32767, 0, 15096, 36820, 32767, 0, 12288,
     36820, 32767, 0, 15120, 36820, 32767, 0, 42681, 21547, 0, 0, 5784, 24516, 32767, 0, 58368,
     24511, 32767, 0, 779, 0, 0, 0, 48416, 65151, 25138, 5632, 5784, 24516, 32767, 0, 58368,
     24511, 32767, 0, 64368, 24512, 32767, 0, 960, 24516, 32767, 0, 28, 0, 0, 0, 58352, 24511,
     32767, 0, 63382, 24512, 32767, 0, 11072, 30235, 32767, 0, 0, 0, 50, 0];
  let mut counts: [u16; 12] = [0, 0, 0, 0, 2, 6, 25, 12, 15, 12, 80, 88];
  let end_counts: [u16; 12] = [0, 0, 0, 0, 2, 6, 25, 12, 15, 0, 0, 0];
  let mut table: [HuffmanCode; 436] = [HuffmanCode {
    bits: 0,
    value: 0,
  }; 436];
  let size = BrotliBuildHuffmanTable(&mut table, 8, &symbol_array, 16, &mut counts);
  assert_eq!(size, 436);
  assert_eq!(counts, end_counts);
  let end_table: [HuffmanCode; 436] = [HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 136,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 88,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 216,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 200,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 9,
                                         value: 259,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 168,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 11,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 64,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 96,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 313,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 152,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 112,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 48,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 56,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 273,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 184,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 16,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 225,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 80,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 5,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 120,
                                       },
                                       HuffmanCode {
                                         bits: 11,
                                         value: 341,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 144,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 88,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 248,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 208,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 40,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 241,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 176,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 8,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 77,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 64,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 2,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 104,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 297,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 160,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 112,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 48,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 240,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 257,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 24,
                                       },
                                       HuffmanCode {
                                         bits: 9,
                                         value: 199,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 80,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 128,
                                       },
                                       HuffmanCode {
                                         bits: 11,
                                         value: 341,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 136,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 88,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 224,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 200,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 201,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 168,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 13,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 64,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 96,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 257,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 152,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 112,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 254,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 48,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 56,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 217,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 184,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 16,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 247,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 80,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 120,
                                       },
                                       HuffmanCode {
                                         bits: 11,
                                         value: 293,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 144,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 88,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 252,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 208,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 40,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 185,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 176,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 8,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 116,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 64,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 4,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 104,
                                       },
                                       HuffmanCode {
                                         bits: 11,
                                         value: 245,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 160,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 112,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 9,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 48,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 240,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 201,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 24,
                                       },
                                       HuffmanCode {
                                         bits: 9,
                                         value: 139,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 80,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 139,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 128,
                                       },
                                       HuffmanCode {
                                         bits: 11,
                                         value: 293,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 136,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 88,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 216,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 200,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 133,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 168,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 12,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 64,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 96,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 189,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 152,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 112,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 48,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 56,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 149,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 184,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 16,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 233,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 80,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 5,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 120,
                                       },
                                       HuffmanCode {
                                         bits: 11,
                                         value: 221,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 144,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 88,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 248,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 208,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 40,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 117,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 176,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 8,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 102,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 64,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 2,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 104,
                                       },
                                       HuffmanCode {
                                         bits: 11,
                                         value: 173,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 160,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 112,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 7,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 48,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 240,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 133,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 24,
                                       },
                                       HuffmanCode {
                                         bits: 9,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 80,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 128,
                                       },
                                       HuffmanCode {
                                         bits: 11,
                                         value: 221,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 136,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 88,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 224,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 200,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 77,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 168,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 20,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 64,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 96,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 133,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 152,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 112,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 254,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 48,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 232,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 56,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 93,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 184,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 16,
                                       },
                                       HuffmanCode {
                                         bits: 9,
                                         value: 37,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 80,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 73,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 120,
                                       },
                                       HuffmanCode {
                                         bits: 11,
                                         value: 173,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 144,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 88,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 252,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 208,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 40,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 61,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 176,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 8,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 170,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 64,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 4,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 104,
                                       },
                                       HuffmanCode {
                                         bits: 11,
                                         value: 125,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 160,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 112,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 48,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 240,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 76,
                                       },
                                       HuffmanCode {
                                         bits: 10,
                                         value: 77,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 24,
                                       },
                                       HuffmanCode {
                                         bits: 9,
                                         value: 13,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 80,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 139,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 128,
                                       },
                                       HuffmanCode {
                                         bits: 11,
                                         value: 173,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 14,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 47,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 65,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 66,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 67,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 68,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 69,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 78,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 138,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 191,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 251,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 17,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 25,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 22,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 26,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 27,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 29,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 30,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 31,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 34,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 35,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 36,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 38,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 37,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 39,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 49,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 50,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 52,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 53,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 55,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 54,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 71,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 75,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 82,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 81,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 83,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 84,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 86,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 85,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 87,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 89,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 91,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 90,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 92,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 93,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 95,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 94,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 117,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 129,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 131,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 130,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 132,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 133,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 135,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 134,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 140,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 141,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 143,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 142,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 169,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 193,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 195,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 194,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 196,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 197,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 199,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 198,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 201,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 202,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 204,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 203,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 205,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 206,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 217,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 207,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 218,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 221,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 220,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 222,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 223,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 242,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 241,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 243,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 244,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 246,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 245,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 250,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 21,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 44,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 42,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 46,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 41,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 45,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 43,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 57,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 58,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 62,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 60,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 97,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 63,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 61,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 98,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 99,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 106,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 101,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 108,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 100,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 107,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 105,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 109,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 110,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 122,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 124,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 111,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 123,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 121,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 125,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 126,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 147,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 145,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 149,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 127,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 148,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 150,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 151,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 156,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 154,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 158,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 153,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 157,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 155,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 159,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 161,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 165,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 163,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 167,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 162,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 166,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 164,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 171,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 177,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 181,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 179,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 183,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 178,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 182,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 180,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 185,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 186,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 190,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 188,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 210,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 187,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 209,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 189,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 211,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 212,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 226,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 214,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 228,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 213,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 227,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 215,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 229,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 230,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 236,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 234,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 238,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 231,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 237,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 235,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 239,
                                       }];
  for index in 0..end_table.len() {
    assert_eq!([index, table[index].bits as usize, table[index].value as usize],
               [index, end_table[index].bits as usize, end_table[index].value as usize]);
    assert!(table[index] == end_table[index]);
  }

}
#[test]
fn singlelevel() {
  let symbol_array: [u16; BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1 +
                          BROTLI_HUFFMAN_MAX_CODE_LENGTHS_SIZE] =
    [65535, 65535, 65535, 137, 0, 16, 2, 17, 33, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
     1, 253, 3, 4, 5, 6, 7, 8, 9, 10, 65, 12, 13, 14, 24, 72, 32, 31, 19, 20, 21, 22, 23, 57, 26,
     26, 32, 28, 29, 30, 31, 249, 184, 34, 35, 36, 37, 38, 39, 40, 48, 42, 43, 44, 45, 46, 47, 58,
     49, 50, 51, 52, 53, 54, 55, 56, 72, 58, 59, 60, 61, 62, 63, 64, 65, 130, 67, 68, 69, 70, 71,
     77, 129, 76, 193, 192, 200, 78, 79, 80, 81, 82, 83, 84, 90, 86, 87, 88, 89, 90, 102, 92, 118,
     94, 95, 96, 97, 98, 99, 100, 101, 103, 103, 104, 105, 106, 107, 108, 115, 110, 111, 112, 113,
     114, 115, 127, 117, 253, 119, 120, 121, 122, 123, 124, 140, 204, 127, 128, 129, 131, 156,
     138, 133, 134, 135, 136, 137, 255, 139, 140, 141, 150, 143, 144, 145, 146, 147, 148, 149,
     150, 152, 152, 159, 154, 155, 156, 208, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167,
     168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 192,
     194, 187, 188, 213, 190, 197, 196, 200, 195, 195, 196, 197, 198, 199, 200, 235, 202, 203,
     204, 205, 206, 207, 208, 209, 224, 212, 222, 218, 236, 215, 216, 217, 240, 219, 220, 221,
     225, 223, 224, 225, 226, 227, 228, 236, 230, 231, 232, 233, 234, 241, 254, 237, 238, 239,
     240, 248, 244, 243, 246, 250, 246, 247, 251, 249, 250, 251, 252, 253, 254, 255, 0, 23040,
     3500, 1, 0, 0, 0, 0, 0, 8192, 3500, 1, 0, 0, 0, 0, 0, 16, 0, 0, 0, 38400, 3500, 1, 0, 61712,
     21033, 32767, 0, 60189, 35058, 32767, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0,
     51712, 3501, 1, 0, 15360, 0, 0, 0, 1536, 3502, 1, 0, 8192, 3500, 1, 0, 61792, 21033, 32767,
     0, 61458, 35058, 32767, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4102, 0, 0, 0, 38400, 3500, 1, 0, 0, 0, 0,
     0, 256, 0, 0, 0, 0, 0, 0, 0, 8192, 3500, 1, 0, 61792, 21033, 32767, 0, 18208, 35059, 32767,
     0, 4102, 0, 0, 65408, 65535, 0, 0, 0, 0, 0, 0, 0, 4102, 0, 0, 0, 16, 0, 0, 0, 9, 0, 0, 0,
     32768, 54143, 32705, 0, 0, 54016, 32705, 0, 0, 54016, 32705, 0, 3584, 0, 0, 0, 6, 0, 0,
     65408, 7, 3502, 32767, 0, 8192, 3500, 1, 0, 0, 54016, 32705, 0, 1, 0, 0, 0, 8192, 3500, 1, 0,
     12288, 54016, 32705, 0, 8192, 54016, 32705, 0, 61888, 21033, 32767, 0, 14237, 35059, 32767,
     0, 88, 53952, 32705, 0, 4607, 0, 0, 0, 8, 0, 0, 0, 4608, 54016, 32705, 0, 8192, 3500, 1, 0,
     8192, 3500, 1, 0, 62016, 21033, 32767, 0, 16544, 35059, 32767, 0, 4096, 0, 0, 0, 8192, 3500,
     1, 0, 61936, 21033, 32767, 0, 12502, 35059, 32767, 0, 0, 0, 0, 0, 43957, 36820, 32767, 0, 0,
     0, 0, 0, 44144, 37434, 32767, 0, 19984, 53952, 32705, 0, 0, 0, 0, 0, 62076, 21033, 32767, 0,
     62080, 21033, 32767, 0, 62084, 21033, 32767, 0, 64, 53952, 32705, 0, 65535, 65535, 65535,
     65535, 64, 0, 8296, 1, 62032, 21033, 32767, 0, 16900, 27601, 32767, 0, 18096, 27604, 32767,
     0, 0, 0, 0, 0, 62080, 21033, 32767, 0, 17447, 27601, 32767, 0, 62080, 21033, 32767, 0, 17246,
     27601, 32767, 0, 0, 0, 0, 0, 62784, 36821, 32767, 0, 62144, 21033, 32767, 0, 4085, 27601,
     32767, 0, 44624, 27603, 32767, 0, 62784, 36821, 32767, 0, 10112, 30246, 32767, 0, 46488,
     27603, 32767, 0, 18096, 27604, 32767, 0, 0, 0, 0, 0, 62256, 21033, 32767, 0, 23819, 27601,
     32767, 0, 62288, 21033, 32767, 0, 20326, 27600, 32767, 0, 15584, 36820, 32767, 0, 15096,
     36820, 32767, 0, 12288, 36820, 32767, 0, 15120, 36820, 32767, 0, 42681, 21547, 0, 0, 18096,
     27604, 32767, 0, 62400, 21033, 32767, 0, 1299, 0, 0, 0, 51315, 34237, 23510, 56320, 18096,
     27604, 32767, 0, 62400, 21033, 32767, 0, 11120, 27601, 32767, 0, 13272, 27604, 32767, 0, 28,
     0, 0, 0, 62384, 21033, 32767, 0, 10134, 27601, 32767, 0, 11072, 30235, 32767, 0, 0, 0, 50, 0];
  let mut counts: [u16; 9] = [0, 0, 0, 2, 3, 7, 13, 6, 24];
  let end_counts: [u16; 9] = [0, 0, 0, 2, 3, 7, 13, 6, 24];
  let mut table: [HuffmanCode; 260] = [HuffmanCode {
    bits: 0,
    value: 0,
  }; 260];
  let size = BrotliBuildHuffmanTable(&mut table, 8, &symbol_array, 16, &mut counts);
  assert_eq!(size, 256);
  assert_eq!(counts, end_counts);
  let end_table: [HuffmanCode; 260] = [HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 184,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 8,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 254,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 249,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 200,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 130,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 4,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 16,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 48,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 2,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 235,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 208,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 56,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 184,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 9,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 254,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 251,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 200,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 156,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 5,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 16,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 52,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 65,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 37,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 235,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 17,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 7,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 138,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 184,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 8,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 254,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 250,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 200,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 130,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 4,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 16,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 50,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 2,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 35,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 235,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 208,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 129,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 184,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 9,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 254,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 252,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 200,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 156,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 5,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 16,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 54,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 65,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 39,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 235,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 31,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 7,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 140,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 184,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 8,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 254,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 249,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 200,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 130,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 4,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 16,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 49,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 2,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 34,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 235,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 208,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 72,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 184,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 9,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 254,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 251,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 200,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 156,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 5,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 16,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 53,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 65,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 38,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 235,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 17,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 7,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 139,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 184,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 8,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 254,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 250,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 200,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 130,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 4,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 16,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 2,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 36,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 235,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 208,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 131,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 184,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 9,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 254,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 252,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 200,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 156,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 5,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 16,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 55,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 65,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 3,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 253,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 40,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 137,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 235,
                                       },
                                       HuffmanCode {
                                         bits: 4,
                                         value: 1,
                                       },
                                       HuffmanCode {
                                         bits: 7,
                                         value: 31,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 255,
                                       },
                                       HuffmanCode {
                                         bits: 6,
                                         value: 7,
                                       },
                                       HuffmanCode {
                                         bits: 5,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 8,
                                         value: 141,
                                       },
                                       HuffmanCode {
                                         bits: 0,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 0,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 0,
                                         value: 0,
                                       },
                                       HuffmanCode {
                                         bits: 0,
                                         value: 0,
                                       }];
  for index in 0..end_table.len() {
    assert_eq!([index, table[index].bits as usize, table[index].value as usize],
               [index, end_table[index].bits as usize, end_table[index].value as usize]);
    assert!(table[index] == end_table[index]);
  }
}
#[test]
fn simple_0() {
  let mut table: [HuffmanCode; 256] = [HuffmanCode {
    bits: 0,
    value: 0,
  }; 256];
  let end_table: [HuffmanCode; 256] = [HuffmanCode {
    bits: 0,
    value: 32,
  }; 256];
  let mut val: [u16; 1] = [32];
  let goal_size = BrotliBuildSimpleHuffmanTable(&mut table, 8, &mut val, 0);
  assert_eq!(goal_size, 256);
  for index in 0..end_table.len() {
    assert_eq!([index, table[index].bits as usize, table[index].value as usize],
               [index, end_table[index].bits as usize, end_table[index].value as usize]);
    assert!(table[index] == end_table[index]);
  }
}
#[test]
fn simple_1() {
  let mut table: [HuffmanCode; 256] = [HuffmanCode {
    bits: 0,
    value: 0,
  }; 256];
  let end_table: [HuffmanCode; 256] = [HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 146,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 192,
                                       }];
  let mut val: [u16; 2] = [146, 192];
  let goal_size = BrotliBuildSimpleHuffmanTable(&mut table, 8, &mut val, 1);
  assert_eq!(goal_size, 256);
  for index in 0..end_table.len() {
    assert_eq!([index, table[index].bits as usize, table[index].value as usize],
               [index, end_table[index].bits as usize, end_table[index].value as usize]);
    assert!(table[index] == end_table[index]);
  }
}




#[test]
fn simple_2() {
  let mut table: [HuffmanCode; 256] = [HuffmanCode {
    bits: 0,
    value: 0,
  }; 256];
  let end_table: [HuffmanCode; 256] = [HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 28,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 32,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 33,
                                       }];
  let mut val: [u16; 3] = [32, 28, 33];
  let goal_size = BrotliBuildSimpleHuffmanTable(&mut table, 8, &mut val, 2);
  assert_eq!(goal_size, 256);
  for index in 0..end_table.len() {
    assert_eq!([index, table[index].bits as usize, table[index].value as usize],
               [index, end_table[index].bits as usize, end_table[index].value as usize]);
    assert!(table[index] == end_table[index]);
  }
}
#[test]
fn simple_3() {
  let mut table: [HuffmanCode; 256] = [HuffmanCode {
    bits: 0,
    value: 0,
  }; 256];
  let end_table: [HuffmanCode; 256] = [HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 59,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 51,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 219,
                                       }];
  let mut val: [u16; 4] = [51, 6, 59, 219];
  let goal_size = BrotliBuildSimpleHuffmanTable(&mut table, 8, &mut val, 3);
  assert_eq!(goal_size, 256);
  for index in 0..end_table.len() {
    assert_eq!([index, table[index].bits as usize, table[index].value as usize],
               [index, end_table[index].bits as usize, end_table[index].value as usize]);
    assert!(table[index] == end_table[index]);
  }
}


#[test]
fn simple_4() {
  let mut table: [HuffmanCode; 256] = [HuffmanCode {
    bits: 0,
    value: 0,
  }; 256];
  let end_table: [HuffmanCode; 256] = [HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 10,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 2,
                                         value: 118,
                                       },
                                       HuffmanCode {
                                         bits: 1,
                                         value: 6,
                                       },
                                       HuffmanCode {
                                         bits: 3,
                                         value: 15,
                                       }];
  let mut val: [u16; 5] = [6, 118, 15, 10, 65535];
  let goal_size = BrotliBuildSimpleHuffmanTable(&mut table, 8, &mut val, 4);
  assert_eq!(goal_size, 256);
  for index in 0..end_table.len() {
    assert_eq!([index, table[index].bits as usize, table[index].value as usize],
               [index, end_table[index].bits as usize, end_table[index].value as usize]);
    assert!(table[index] == end_table[index]);
  }
}
