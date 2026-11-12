#![cfg(test)]

extern crate alloc_no_stdlib as alloc;
use alloc::{AllocatedStackMemory, Allocator, SliceWrapper, SliceWrapperMut, StackAllocator, bzero};
#[cfg(feature="std")]
use std::vec::Vec;
#[cfg(feature="std")]
use std::io;

use core::ops;


pub use super::{BrotliDecompressStream, BrotliResult, BrotliState, HuffmanCode};

declare_stack_allocator_struct!(MemPool, 4096, stack);

struct FailSecondBlockTreeAllocation<AllocHC> {
  alloc: AllocHC,
  block_tree_allocations: u8,
}

struct FailFirstHtreeAllocation<AllocU32> {
  alloc: AllocU32,
  failed: bool,
}

impl<AllocU32> FailFirstHtreeAllocation<AllocU32> {
  fn new(alloc: AllocU32) -> Self {
    FailFirstHtreeAllocation {
      alloc,
      failed: false,
    }
  }
}

impl<AllocU32: Allocator<u32>> Allocator<u32> for FailFirstHtreeAllocation<AllocU32> {
  type AllocatedMemory = AllocU32::AllocatedMemory;

  fn alloc_cell(&mut self, len: usize) -> Self::AllocatedMemory {
    if !self.failed {
      self.failed = true;
      return Self::AllocatedMemory::default();
    }
    self.alloc.alloc_cell(len)
  }

  fn free_cell(&mut self, data: Self::AllocatedMemory) {
    self.alloc.free_cell(data);
  }
}

impl<AllocHC> FailSecondBlockTreeAllocation<AllocHC> {
  fn new(alloc: AllocHC) -> Self {
    FailSecondBlockTreeAllocation {
      alloc,
      block_tree_allocations: 0,
    }
  }
}

impl<AllocHC: Allocator<HuffmanCode>> Allocator<HuffmanCode>
  for FailSecondBlockTreeAllocation<AllocHC> {
  type AllocatedMemory = AllocHC::AllocatedMemory;

  fn alloc_cell(&mut self, len: usize) -> Self::AllocatedMemory {
    if len == 3 * super::huffman::BROTLI_HUFFMAN_MAX_SIZE_258 as usize ||
       len == 3 * super::huffman::BROTLI_HUFFMAN_MAX_SIZE_26 as usize {
      self.block_tree_allocations += 1;
      if self.block_tree_allocations == 2 {
        return Self::AllocatedMemory::default();
      }
    }
    self.alloc.alloc_cell(len)
  }

  fn free_cell(&mut self, data: Self::AllocatedMemory) {
    self.alloc.free_cell(data);
  }
}

fn oneshot(input: &mut [u8], mut output: &mut [u8]) -> (BrotliResult, usize, usize) {
  let mut available_out: usize = output.len();
  let mut stack_u8_buffer = define_allocator_memory_pool!(4096, u8, [0; 16 * 1024], stack);
  let mut stack_u32_buffer = define_allocator_memory_pool!(4096, u32, [0; 4 * 1024], stack);
  let mut stack_hc_buffer = define_allocator_memory_pool!(4096,
                                                          super::HuffmanCode,
                                                          [HuffmanCode::default(); 20 * 1024],
                                                          stack);
  let stack_u8_allocator = MemPool::<u8>::new_allocator(&mut stack_u8_buffer, bzero);
  let stack_u32_allocator = MemPool::<u32>::new_allocator(&mut stack_u32_buffer, bzero);
  let stack_hc_allocator = MemPool::<HuffmanCode>::new_allocator(&mut stack_hc_buffer, bzero);
  let mut available_in: usize = input.len();
  let mut input_offset: usize = 0;
  let mut output_offset: usize = 0;
  let mut written: usize = 0;
  let mut brotli_state =
    BrotliState::new(stack_u8_allocator, stack_u32_allocator, stack_hc_allocator);
  let result = BrotliDecompressStream(&mut available_in,
                                      &mut input_offset,
                                      &input[..],
                                      &mut available_out,
                                      &mut output_offset,
                                      &mut output,
                                      &mut written,
                                      &mut brotli_state);
  return (result, input_offset, output_offset);
}

// As oneshot, with at most in_chunk bytes of input and out_chunk bytes of
// output space per call.
fn decode_in_chunks(input: &[u8],
                    output: &mut [u8],
                    in_chunk: usize,
                    out_chunk: usize)
                    -> (BrotliResult, usize, usize) {
  let mut stack_u8_buffer = define_allocator_memory_pool!(4096, u8, [0; 16 * 1024], stack);
  let mut stack_u32_buffer = define_allocator_memory_pool!(4096, u32, [0; 4 * 1024], stack);
  let mut stack_hc_buffer = define_allocator_memory_pool!(4096,
                                                          super::HuffmanCode,
                                                          [HuffmanCode::default(); 20 * 1024],
                                                          stack);
  let stack_u8_allocator = MemPool::<u8>::new_allocator(&mut stack_u8_buffer, bzero);
  let stack_u32_allocator = MemPool::<u32>::new_allocator(&mut stack_u32_buffer, bzero);
  let stack_hc_allocator = MemPool::<HuffmanCode>::new_allocator(&mut stack_hc_buffer, bzero);
  let mut brotli_state =
    BrotliState::new(stack_u8_allocator, stack_u32_allocator, stack_hc_allocator);
  let mut input_offset: usize = 0;
  let mut output_offset: usize = 0;
  let mut written: usize = 0;
  loop {
    let mut available_in = core::cmp::min(in_chunk, input.len() - input_offset);
    let mut available_out = core::cmp::min(out_chunk, output.len() - output_offset);
    let result = BrotliDecompressStream(&mut available_in,
                                        &mut input_offset,
                                        input,
                                        &mut available_out,
                                        &mut output_offset,
                                        output,
                                        &mut written,
                                        &mut brotli_state);
    match result {
      BrotliResult::NeedsMoreInput if input_offset < input.len() => {}
      BrotliResult::NeedsMoreOutput if output_offset < output.len() => {}
      _ => return (result, input_offset, output_offset),
    }
  }
}

#[test]
fn test_block_len_trees_allocation_failure() {
  // testdata/fuzz502.compressed: its first metablock has several block types,
  // which is when the block type and length trees are allocated.
  let input = [0xe2u8, 0xe1, 0x84, 0x88, 0x6a, 0x56, 0x30, 0x80, 0xe0, 0x39, 0x16, 0x03,
               0xe4, 0x30, 0xf9];
  let mut output = [0u8; 1];
  let mut stack_u8_buffer = define_allocator_memory_pool!(4096, u8, [0; 16 * 1024], stack);
  let mut stack_u32_buffer = define_allocator_memory_pool!(4096, u32, [0; 4 * 1024], stack);
  let mut stack_hc_buffer = define_allocator_memory_pool!(4096,
                                                          HuffmanCode,
                                                          [HuffmanCode::default(); 20 * 1024],
                                                          stack);
  let stack_u8_allocator = MemPool::<u8>::new_allocator(&mut stack_u8_buffer, bzero);
  let stack_u32_allocator = MemPool::<u32>::new_allocator(&mut stack_u32_buffer, bzero);
  let stack_hc_allocator =
    FailSecondBlockTreeAllocation::new(MemPool::<HuffmanCode>::new_allocator(&mut stack_hc_buffer,
                                                                             bzero));
  let mut state = BrotliState::new(stack_u8_allocator, stack_u32_allocator, stack_hc_allocator);
  let mut available_in = input.len();
  let mut input_offset = 0;
  let mut available_out = output.len();
  let mut output_offset = 0;
  let mut total_out = 0;

  let result = BrotliDecompressStream(&mut available_in,
                                      &mut input_offset,
                                      &input,
                                      &mut available_out,
                                      &mut output_offset,
                                      &mut output,
                                      &mut total_out,
                                      &mut state);

  match result {
    BrotliResult::ResultFailure => {}
    _ => panic!("block_len_trees allocation failure must fail decoding"),
  }
  match state.error_code {
    super::state::BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_BLOCK_TYPE_TREES => {}
    _ => panic!("unexpected decoder error after block_len_trees allocation failure"),
  }
}

#[test]
fn test_stream_without_block_switches_needs_no_block_trees() {
  // An empty stream decodes even if the block tree allocations would fail.
  let input = [0x06u8];
  let mut output = [0u8; 1];
  let mut stack_u8_buffer = define_allocator_memory_pool!(4096, u8, [0; 16 * 1024], stack);
  let mut stack_u32_buffer = define_allocator_memory_pool!(4096, u32, [0; 4 * 1024], stack);
  let mut stack_hc_buffer = define_allocator_memory_pool!(4096,
                                                          HuffmanCode,
                                                          [HuffmanCode::default(); 20 * 1024],
                                                          stack);
  let stack_u8_allocator = MemPool::<u8>::new_allocator(&mut stack_u8_buffer, bzero);
  let stack_u32_allocator = MemPool::<u32>::new_allocator(&mut stack_u32_buffer, bzero);
  let stack_hc_allocator =
    FailSecondBlockTreeAllocation::new(MemPool::<HuffmanCode>::new_allocator(&mut stack_hc_buffer,
                                                                             bzero));
  let mut state = BrotliState::new(stack_u8_allocator, stack_u32_allocator, stack_hc_allocator);
  let mut available_in = input.len();
  let mut input_offset = 0;
  let mut available_out = output.len();
  let mut output_offset = 0;
  let mut total_out = 0;
  match BrotliDecompressStream(&mut available_in, &mut input_offset, &input,
                               &mut available_out, &mut output_offset, &mut output,
                               &mut total_out, &mut state) {
    BrotliResult::ResultSuccess => {}
    _ => panic!("an empty stream must decode without the block trees"),
  }
  assert_eq!(state.alloc_hc.block_tree_allocations, 0);
}

// Records whether the block trees are allocated after a HuffmanCode allocation
// has been freed, as the tree groups are at the end of each metablock.
#[cfg(feature="std")]
struct BlockTreesAfterFree<AllocHC> {
  alloc: AllocHC,
  freed: bool,
  block_trees: usize,
  block_trees_after_free: bool,
}

#[cfg(feature="std")]
impl<AllocHC: Allocator<HuffmanCode>> Allocator<HuffmanCode> for BlockTreesAfterFree<AllocHC> {
  type AllocatedMemory = AllocHC::AllocatedMemory;

  fn alloc_cell(&mut self, len: usize) -> Self::AllocatedMemory {
    if len == 3 * super::huffman::BROTLI_HUFFMAN_MAX_SIZE_258 as usize ||
       len == 3 * super::huffman::BROTLI_HUFFMAN_MAX_SIZE_26 as usize {
      self.block_trees += 1;
      self.block_trees_after_free |= self.freed;
    }
    self.alloc.alloc_cell(len)
  }

  fn free_cell(&mut self, data: Self::AllocatedMemory) {
    self.freed |= data.slice().len() != 0;
    self.alloc.free_cell(data);
  }
}

// The C encoder at quality 5, window 16, given the first 300 bytes of
// alice29.txt, a flush, then the next 8000: the flushed first metablock has no
// block switches, the second has, so the block trees are first needed after
// the first tree groups are freed.
#[test]
#[cfg(feature="std")]
fn test_block_switches_after_a_flushed_metablock() {
  let mut input = include_bytes!("../testdata/flushed_then_block_switches.compressed").to_vec();
  let expected = &include_bytes!("../testdata/alice29.txt")[..8300];
  let mut output = vec![0u8; expected.len() + 1];
  let (result, _, written) = oneshot_std(&mut input, &mut output);
  match result {
    BrotliResult::ResultSuccess => {}
    _ => panic!("decoding failed"),
  }
  assert_eq!(&output[..written], expected);
}

// The block trees are allocated only when first needed, but never after a
// tree group has been freed: in a pool allocator that never merges what it
// gets back, they then cannot take a gap that a later tree group would have
// reused, so the pool never needs more than when they were allocated first.
#[test]
#[cfg(feature="std")]
fn test_block_trees_are_never_allocated_after_a_free() {
  let dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("testdata");
  let mut paths: Vec<_> = std::fs::read_dir(&dir).unwrap().map(|e| e.unwrap().path())
    .filter(|p| p.extension().map_or(false, |e| e == "compressed")).collect();
  paths.sort();
  let (mut with_trees, mut without_trees) = (0, 0);
  for path in paths {
    let input = std::fs::read(&path).unwrap();
    let mut state = BrotliState::new(super::StandardAlloc::default(),
                                     super::StandardAlloc::default(),
                                     BlockTreesAfterFree { alloc: super::StandardAlloc::default(),
                                                           freed: false,
                                                           block_trees: 0,
                                                           block_trees_after_free: false });
    let mut available_in = input.len();
    let mut input_offset = 0;
    let mut total_out = 0;
    let mut output = [0u8; 65536];
    loop {
      let mut available_out = output.len();
      let mut output_offset = 0;
      match BrotliDecompressStream(&mut available_in, &mut input_offset, &input,
                                   &mut available_out, &mut output_offset, &mut output,
                                   &mut total_out, &mut state) {
        BrotliResult::NeedsMoreOutput => {}
        _ => break,
      }
    }
    assert!(!state.alloc_hc.block_trees_after_free, "{}", path.display());
    if state.alloc_hc.block_trees != 0 { with_trees += 1 } else { without_trees += 1 }
  }
  // Guard against the corpus not covering both cases.
  assert!(with_trees >= 5 && without_trees >= 5, "{} {}", with_trees, without_trees);
}

#[test]
fn test_hgroup_htrees_allocation_failure() {
  let input = [0x1b, 0x13, 0x00, 0x00, 0xa4, 0xb0, 0xb2, 0xea, 0x81, 0x47, 0x02, 0x8a];
  let mut output = [0u8; 20];
  let mut stack_u8_buffer = define_allocator_memory_pool!(4096, u8, [0; 16 * 1024], stack);
  let mut stack_u32_buffer = define_allocator_memory_pool!(4096, u32, [0; 4 * 1024], stack);
  let mut stack_hc_buffer = define_allocator_memory_pool!(4096,
                                                          HuffmanCode,
                                                          [HuffmanCode::default(); 20 * 1024],
                                                          stack);
  let stack_u8_allocator = MemPool::<u8>::new_allocator(&mut stack_u8_buffer, bzero);
  let stack_u32_allocator =
    FailFirstHtreeAllocation::new(MemPool::<u32>::new_allocator(&mut stack_u32_buffer, bzero));
  let stack_hc_allocator = MemPool::<HuffmanCode>::new_allocator(&mut stack_hc_buffer, bzero);
  let mut state = BrotliState::new(stack_u8_allocator, stack_u32_allocator, stack_hc_allocator);
  let mut available_in = input.len();
  let mut input_offset = 0;
  let mut available_out = output.len();
  let mut output_offset = 0;
  let mut total_out = 0;

  let result = BrotliDecompressStream(&mut available_in,
                                      &mut input_offset,
                                      &input,
                                      &mut available_out,
                                      &mut output_offset,
                                      &mut output,
                                      &mut total_out,
                                      &mut state);

  match result {
    BrotliResult::ResultFailure => {}
    _ => panic!("hgroup htrees allocation failure must fail decoding"),
  }
  match state.error_code {
    super::state::BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_TREE_GROUPS => {}
    _ => panic!("unexpected decoder error after hgroup htrees allocation failure"),
  }
}

#[test]
fn test_rejects_invalid_carry_buffer_length() {
  let input = [];
  let mut output = [];
  let mut stack_u8_buffer = define_allocator_memory_pool!(4096, u8, [0; 16 * 1024], stack);
  let mut stack_u32_buffer = define_allocator_memory_pool!(4096, u32, [0; 4 * 1024], stack);
  let mut stack_hc_buffer = define_allocator_memory_pool!(4096,
                                                          HuffmanCode,
                                                          [HuffmanCode::default(); 20 * 1024],
                                                          stack);
  let stack_u8_allocator = MemPool::<u8>::new_allocator(&mut stack_u8_buffer, bzero);
  let stack_u32_allocator = MemPool::<u32>::new_allocator(&mut stack_u32_buffer, bzero);
  let stack_hc_allocator = MemPool::<HuffmanCode>::new_allocator(&mut stack_hc_buffer, bzero);
  let mut state = BrotliState::new(stack_u8_allocator, stack_u32_allocator, stack_hc_allocator);
  state.buffer_length = state.buffer.len() as u32 + 1;
  let mut available_in = 0;
  let mut input_offset = 0;
  let mut available_out = 0;
  let mut output_offset = 0;
  let mut total_out = 0;

  let result = BrotliDecompressStream(&mut available_in,
                                      &mut input_offset,
                                      &input,
                                      &mut available_out,
                                      &mut output_offset,
                                      &mut output,
                                      &mut total_out,
                                      &mut state);

  match result {
    BrotliResult::ResultFailure => {}
    _ => panic!("invalid carry-buffer length must fail decoding"),
  }
  match state.error_code {
    super::state::BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE => {}
    _ => panic!("unexpected decoder error for invalid carry-buffer length"),
  }
}

// no-std variant of oneshot that seeds the decoder with a custom dictionary.
fn oneshot_dict(input: &mut [u8], dict: &[u8], mut output: &mut [u8]) -> (BrotliResult, usize, usize) {
  let mut available_out: usize = output.len();
  let mut stack_u8_buffer = define_allocator_memory_pool!(4096, u8, [0; 16 * 1024], stack);
  let mut stack_u32_buffer = define_allocator_memory_pool!(4096, u32, [0; 4 * 1024], stack);
  let mut stack_hc_buffer = define_allocator_memory_pool!(4096,
                                                          super::HuffmanCode,
                                                          [HuffmanCode::default(); 20 * 1024],
                                                          stack);
  let mut stack_u8_allocator = MemPool::<u8>::new_allocator(&mut stack_u8_buffer, bzero);
  let stack_u32_allocator = MemPool::<u32>::new_allocator(&mut stack_u32_buffer, bzero);
  let stack_hc_allocator = MemPool::<HuffmanCode>::new_allocator(&mut stack_hc_buffer, bzero);
  let mut custom_dict = stack_u8_allocator.alloc_cell(dict.len());
  custom_dict.slice_mut().clone_from_slice(dict);
  let mut available_in: usize = input.len();
  let mut input_offset: usize = 0;
  let mut output_offset: usize = 0;
  let mut written: usize = 0;
  let mut brotli_state = BrotliState::new_with_custom_dictionary(stack_u8_allocator,
                                                                 stack_u32_allocator,
                                                                 stack_hc_allocator,
                                                                 custom_dict);
  let result = BrotliDecompressStream(&mut available_in,
                                      &mut input_offset,
                                      &input[..],
                                      &mut available_out,
                                      &mut output_offset,
                                      &mut output,
                                      &mut written,
                                      &mut brotli_state);
  return (result, input_offset, output_offset);
}

// Heap-backed (StandardAlloc) variant of oneshot for files that require more
// than 100KB of buffer to decompress.
#[cfg(feature="std")]
fn oneshot_std(input: &mut [u8], mut output: &mut [u8]) -> (BrotliResult, usize, usize) {
  let mut available_out: usize = output.len();
  let mut available_in: usize = input.len();
  let mut input_offset: usize = 0;
  let mut output_offset: usize = 0;
  let mut written: usize = 0;
  let mut brotli_state = BrotliState::new(super::StandardAlloc::default(),
                                          super::StandardAlloc::default(),
                                          super::StandardAlloc::default());
  let result = BrotliDecompressStream(&mut available_in,
                                      &mut input_offset,
                                      &input[..],
                                      &mut available_out,
                                      &mut output_offset,
                                      &mut output,
                                      &mut written,
                                      &mut brotli_state);
  return (result, input_offset, output_offset);
}

#[test]
fn test_10x10y() {
  const BUFFER_SIZE: usize = 2048;
  let mut input: [u8; 12] = [0x1b, 0x13, 0x00, 0x00, 0xa4, 0xb0, 0xb2, 0xea, 0x81, 0x47, 0x02,
                             0x8a];
  let mut output = [0u8; BUFFER_SIZE];
  let (result, input_offset, output_offset) = oneshot(&mut input[..], &mut output[..]);
  match result {
    BrotliResult::ResultSuccess => {}
    _ => assert!(false),
  }
  let mut i: usize = 0;
  while i < 10 {
    assert_eq!(output[i], 'X' as u8);
    assert_eq!(output[i + 10], 'Y' as u8);
    i += 1;
  }
  assert_eq!(output_offset, 20);
  assert_eq!(input_offset, input.len());
}



#[test]
fn test_x() {
  const BUFFER_SIZE: usize = 128;
  let mut input: [u8; 5] = [0x0b, 0x00, 0x80, 0x58, 0x03];
  let mut output = [0u8; BUFFER_SIZE];
  let (result, input_offset, output_offset) = oneshot(&mut input[..], &mut output[..]);
  match result {
    BrotliResult::ResultSuccess => {}
    _ => assert!(false),
  }
  assert_eq!(output[0], 'X' as u8);
  assert_eq!(output_offset, 1);
  assert_eq!(input_offset, input.len());
}

// 60 lines of "The quick brown fox jumps over the lazy dog <i> times.\n",
// compressed at quality 5 with a 4 MiB window (lgwin 22), flushing every 700
// bytes. As with fast or flushing encoders, the data sits in metablocks that
// are not the last one. The ring buffer grows only as that data needs, here
// 1 KiB, then 2 KiB, then 4 KiB, where it used to take the whole 4 MiB window.
// So the stream decodes within a 16 KiB pool, and every later metablock
// copies from bytes decoded before a growth.
#[test]
fn test_flushed_stream_grows_ring_buffer_in_small_pool() {
  let input: [u8; 181] = [
    0x8b, 0x5d, 0x01, 0x00, 0xc4, 0x60, 0x97, 0x9e, 0xd0, 0x01, 0xb5, 0x86,
    0x78, 0x20, 0xf5, 0x63, 0x0c, 0xc4, 0xc1, 0x01, 0xd8, 0x40, 0xe4, 0xe4,
    0x41, 0x12, 0x58, 0x06, 0x01, 0x50, 0x30, 0x91, 0xa3, 0x80, 0x5b, 0x72,
    0x90, 0x27, 0xb6, 0x15, 0x9a, 0xbf, 0xc8, 0xfb, 0xf2, 0xcc, 0xa8, 0x97,
    0x17, 0xfd, 0x35, 0xad, 0x07, 0x96, 0xbb, 0xda, 0x3f, 0x30, 0x5e, 0x63,
    0xfa, 0x3f, 0x94, 0x4b, 0x03, 0x0e, 0x65, 0xed, 0xa0, 0x44, 0xe4, 0x82,
    0x4b, 0xae, 0xb8, 0xe6, 0x86, 0x5b, 0xee, 0xb8, 0xe7, 0x61, 0x40, 0xcc,
    0xa2, 0x9e, 0x5e, 0x60, 0x00, 0xd8, 0x15, 0x00, 0x40, 0x2a, 0x46, 0xa6,
    0x9d, 0x71, 0xa2, 0x08, 0x25, 0x46, 0x31, 0x7b, 0x9e, 0xe7, 0x79, 0x9e,
    0xd7, 0xef, 0xf7, 0xfb, 0xfd, 0xd7, 0x00, 0xd8, 0x15, 0x00, 0x40, 0x4a,
    0x66, 0x26, 0x9d, 0x48, 0xc2, 0x88, 0x00, 0x02, 0xcf, 0xf3, 0x3c, 0xdf,
    0xf7, 0x7d, 0xdf, 0xf7, 0xfd, 0x37, 0x00, 0xd8, 0x15, 0x00, 0x40, 0x92,
    0x66, 0xa6, 0x26, 0x9d, 0x40, 0xc2, 0x88, 0x00, 0x12, 0x9f, 0xe7, 0x79,
    0x9e, 0xe7, 0x79, 0x9e, 0xe7, 0xf9, 0xbf, 0x01, 0xd1, 0x1a, 0x00, 0x40,
    0xa2, 0x26, 0x9d, 0x30, 0xe2, 0x81, 0x00, 0x26, 0xef, 0xbd, 0xf7, 0xde,
    0x07];
  let mut expected = [0u8; 3230];
  let mut len = 0;
  for i in 0..60u8 {
    let digits = [b'0' + i / 10, b'0' + i % 10];
    let number: &[u8] = if i < 10 { &digits[1..] } else { &digits[..] };
    for part in [&b"The quick brown fox jumps over the lazy dog "[..], number, &b" times.\n"[..]].iter() {
      expected[len..len + part.len()].clone_from_slice(part);
      len += part.len();
    }
  }
  assert_eq!(len, expected.len());
  // One call, then byte by byte, then odd chunks, each in a fresh 16 KiB pool.
  for &(in_chunk, out_chunk) in &[(usize::MAX, usize::MAX), (1, 1), (7, 13)] {
    let mut output = [0u8; 4096];
    let (result, input_offset, output_offset) =
      decode_in_chunks(&input[..], &mut output[..], in_chunk, out_chunk);
    match result {
      BrotliResult::ResultSuccess => {}
      _ => assert!(false),
    }
    assert_eq!(output_offset, expected.len());
    assert_eq!(input_offset, input.len());
    assert!(output[..output_offset] == expected[..]);
  }
}

#[test]
fn test_corrupt_input_large_distance_code() {
  const BUFFER_SIZE: usize = 128;
  let mut input: [u8; 46] = [17, 139, 32, 255, 8, 0, 136, 255, 32, 46, 146, 32, 255, 255, 255, 255, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32];
  let mut output = [0u8; BUFFER_SIZE];
  let (result, _, _) = oneshot(&mut input[..], &mut output[..]);
  match result {
    BrotliResult::ResultFailure => {}
    _ => assert!(false),
  }
}

#[test]
fn test_empty() {
  const BUFFER_SIZE: usize = 128;
  let mut input: [u8; 1] = [0x06];
  let mut output = [0u8; BUFFER_SIZE];
  let (result, input_offset, output_offset) = oneshot(&mut input[..], &mut output[..]);
  match result {
    BrotliResult::ResultSuccess => {}
    _ => assert!(false),
  }
  assert_eq!(output_offset, 0);
  assert_eq!(input_offset, input.len());
}
#[cfg(feature="std")]
const QF_BUFFER_SIZE: usize = 180 * 1024;

/// The decompressor needs a 256K ring buffer so should use std heap allocation.
#[test]
#[cfg(feature="std")]
fn test_quickfox_repeated_custom() {
  let mut input: [u8; 58] =
    [0x5B, 0xFF, 0xAF, 0x02, 0xC0, 0x22, 0x79, 0x5C, 0xFB, 0x5A, 0x8C, 0x42, 0x3B, 0xF4, 0x25,
     0x55, 0x19, 0x5A, 0x92, 0x99, 0xB1, 0x35, 0xC8, 0x19, 0x9E, 0x9E, 0x0A, 0x7B, 0x4B, 0x90,
     0xB9, 0x3C, 0x98, 0xC8, 0x09, 0x40, 0xF3, 0xE6, 0xD9, 0x4D, 0xE4, 0x6D, 0x65, 0x1B, 0x27,
     0x87, 0x13, 0x5F, 0xA6, 0xE9, 0x30, 0x96, 0x7B, 0x3C, 0x15, 0xD8, 0x53, 0x1C];

  let mut quick_fox_output: Vec<u8> = vec![0u8; QF_BUFFER_SIZE];
  let (result, input_offset, output_offset) = oneshot_std(&mut input[..], &mut quick_fox_output[..]);
  match result {
    BrotliResult::ResultSuccess => {}
    _ => assert!(false),
  }
  assert_eq!(output_offset, 176128);
  assert_eq!(input_offset, input.len());
  const fox: [u8; 0x2b] = [0x54, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, 0x6B, 0x20, 0x62, 0x72,
                           0x6F, 0x77, 0x6E, 0x20, 0x66, 0x6F, 0x78, 0x20, 0x6A, 0x75, 0x6D, 0x70,
                           0x73, 0x20, 0x6F, 0x76, 0x65, 0x72, 0x20, 0x74, 0x68, 0x65, 0x20, 0x6C,
                           0x61, 0x7A, 0x79, 0x20, 0x64, 0x6F, 0x67];
  let mut index: usize = 0;
  for item in quick_fox_output[0..176128].iter() {
    assert_eq!(*item, fox[index]);
    index += 1;
    if index == 0x2b {
      index = 0;
    }
  }
}

#[test]
#[cfg(feature="std")]
fn test_quickfox_repeated_exported() {
  let input: [u8; 58] =
    [0x5B, 0xFF, 0xAF, 0x02, 0xC0, 0x22, 0x79, 0x5C, 0xFB, 0x5A, 0x8C, 0x42, 0x3B, 0xF4, 0x25,
     0x55, 0x19, 0x5A, 0x92, 0x99, 0xB1, 0x35, 0xC8, 0x19, 0x9E, 0x9E, 0x0A, 0x7B, 0x4B, 0x90,
     0xB9, 0x3C, 0x98, 0xC8, 0x09, 0x40, 0xF3, 0xE6, 0xD9, 0x4D, 0xE4, 0x6D, 0x65, 0x1B, 0x27,
     0x87, 0x13, 0x5F, 0xA6, 0xE9, 0x30, 0x96, 0x7B, 0x3C, 0x15, 0xD8, 0x53, 0x1C];
  let mut quick_fox_exported_output: Vec<u8> = vec![0u8; QF_BUFFER_SIZE * 3];
  let res = ::brotli_decode(&input[..], &mut quick_fox_exported_output[..]);
  match res.result {
    BrotliResult::ResultSuccess => {}
    _ => assert!(false),
  }
  assert_eq!(res.decoded_size, 176128);
  const fox: [u8; 0x2b] = [0x54, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, 0x6B, 0x20, 0x62, 0x72,
                           0x6F, 0x77, 0x6E, 0x20, 0x66, 0x6F, 0x78, 0x20, 0x6A, 0x75, 0x6D, 0x70,
                           0x73, 0x20, 0x6F, 0x76, 0x65, 0x72, 0x20, 0x74, 0x68, 0x65, 0x20, 0x6C,
                           0x61, 0x7A, 0x79, 0x20, 0x64, 0x6F, 0x67];
  let mut index: usize = 0;
  for item in quick_fox_exported_output[0..176128].iter() {
    assert_eq!(*item, fox[index]);
    index += 1;
    if index == 0x2b {
      index = 0;
    }
  }
}

#[test]
#[cfg(feature="std")]
fn test_quickfox_repeated_exported_prealloc() {
  let input: [u8; 58] =
    [0x5B, 0xFF, 0xAF, 0x02, 0xC0, 0x22, 0x79, 0x5C, 0xFB, 0x5A, 0x8C, 0x42, 0x3B, 0xF4, 0x25,
     0x55, 0x19, 0x5A, 0x92, 0x99, 0xB1, 0x35, 0xC8, 0x19, 0x9E, 0x9E, 0x0A, 0x7B, 0x4B, 0x90,
     0xB9, 0x3C, 0x98, 0xC8, 0x09, 0x40, 0xF3, 0xE6, 0xD9, 0x4D, 0xE4, 0x6D, 0x65, 0x1B, 0x27,
     0x87, 0x13, 0x5F, 0xA6, 0xE9, 0x30, 0x96, 0x7B, 0x3C, 0x15, 0xD8, 0x53, 0x1C];
  let mut qf: Vec<u8> = vec![0u8; QF_BUFFER_SIZE];
  let mut scratch: Vec<u8> = vec![0u8; QF_BUFFER_SIZE * 2];
  let mut scratch_u32: Vec<u32> = vec![0u32; 65536];
  let mut scratch_hc: Vec<HuffmanCode> = vec![HuffmanCode::default(); 65536];
  let res = ::brotli_decode_prealloc(&input[..], &mut qf[..], &mut scratch[..], &mut scratch_u32[..], &mut scratch_hc[..]);
  match res.result {
    BrotliResult::ResultSuccess => {}
    _ => assert!(false),
  }
  assert_eq!(res.decoded_size, 176128);
  const fox: [u8; 0x2b] = [0x54, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, 0x6B, 0x20, 0x62, 0x72,
                           0x6F, 0x77, 0x6E, 0x20, 0x66, 0x6F, 0x78, 0x20, 0x6A, 0x75, 0x6D, 0x70,
                           0x73, 0x20, 0x6F, 0x76, 0x65, 0x72, 0x20, 0x74, 0x68, 0x65, 0x20, 0x6C,
                           0x61, 0x7A, 0x79, 0x20, 0x64, 0x6F, 0x67];
  let mut index: usize = 0;
  for item in qf[0..176128].iter() {
    assert_eq!(*item, fox[index]);
    index += 1;
    if index == 0x2b {
      index = 0;
    }
  }
}


#[cfg(feature="std")]
struct Buffer {
  data: Vec<u8>,
  read_offset: usize,
}
#[cfg(feature="std")]
impl Buffer {
  pub fn new(buf: &[u8]) -> Buffer {
    let mut ret = Buffer {
      data: Vec::<u8>::new(),
      read_offset: 0,
    };
    ret.data.extend(buf);
    return ret;
  }
}
#[cfg(feature="std")]
impl io::Read for Buffer {
  fn read(self: &mut Self, buf: &mut [u8]) -> io::Result<usize> {
    let bytes_to_read = ::core::cmp::min(buf.len(), self.data.len() - self.read_offset);
    if bytes_to_read > 0 {
      buf[0..bytes_to_read]
        .clone_from_slice(&self.data[self.read_offset..self.read_offset + bytes_to_read]);
    }
    self.read_offset += bytes_to_read;
    return Ok(bytes_to_read);
  }
}
#[cfg(feature="std")]
impl io::Write for Buffer {
  fn write(self: &mut Self, buf: &[u8]) -> io::Result<usize> {
    self.data.extend(buf);
    return Ok(buf.len());
  }
  fn flush(self: &mut Self) -> io::Result<()> {
    return Ok(());
  }
}


#[test]
#[cfg(feature="std")]
fn test_reader_quickfox_repeated() {
  let in_buf: [u8; 58] = [0x5B, 0xFF, 0xAF, 0x02, 0xC0, 0x22, 0x79, 0x5C, 0xFB, 0x5A, 0x8C, 0x42,
                          0x3B, 0xF4, 0x25, 0x55, 0x19, 0x5A, 0x92, 0x99, 0xB1, 0x35, 0xC8, 0x19,
                          0x9E, 0x9E, 0x0A, 0x7B, 0x4B, 0x90, 0xB9, 0x3C, 0x98, 0xC8, 0x09, 0x40,
                          0xF3, 0xE6, 0xD9, 0x4D, 0xE4, 0x6D, 0x65, 0x1B, 0x27, 0x87, 0x13, 0x5F,
                          0xA6, 0xE9, 0x30, 0x96, 0x7B, 0x3C, 0x15, 0xD8, 0x53, 0x1C];

  let mut output = Buffer::new(&[]);
  let mut input = super::Decompressor::new(Buffer::new(&in_buf), 4096);
  match super::copy_from_to(&mut input, &mut output) {
    Ok(_) => {}
    Err(e) => panic!("Error {:?}", e),
  }

  assert_eq!(output.data.len(), 176128);
  const fox: [u8; 0x2b] = [0x54, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, 0x6B, 0x20, 0x62, 0x72,
                           0x6F, 0x77, 0x6E, 0x20, 0x66, 0x6F, 0x78, 0x20, 0x6A, 0x75, 0x6D, 0x70,
                           0x73, 0x20, 0x6F, 0x76, 0x65, 0x72, 0x20, 0x74, 0x68, 0x65, 0x20, 0x6C,
                           0x61, 0x7A, 0x79, 0x20, 0x64, 0x6F, 0x67];
  let mut index: usize = 0;
  for item in output.data[0..176128].iter_mut() {
    assert_eq!(*item, fox[index]);
    index += 1;
    if index == 0x2b {
      index = 0;
    }
  }
}

// The stream declares a large window (~128KB ring buffer) before hitting EOF,
// so it runs on the heap-backed path to keep that off the stack (std only).
#[test]
#[cfg(feature="std")]
fn test_early_eof() {
  const BUFFER_SIZE: usize = 128;
  let mut input: [u8; 47] = [17, 17, 32, 32, 109, 109, 32, 32, 32, 181, 2, 0, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 151, 32, 42, 181, 32, 149, 59, 0, 0, 0, 0, 42, 42, 42, 42, 42, 5, 255, 255, 255, 255, 255];
  let mut output = [0u8; BUFFER_SIZE];
  let (result, input_offset, _output_offset) = oneshot_std(&mut input[..], &mut output[..]);
  match result {
    BrotliResult::ResultFailure => {}
    _ => assert!(false),
  }
  assert_eq!(input_offset, input.len());
}

#[test]
#[cfg(feature="std")]
fn test_run_out_of_writer_space() {
  // this is a valid compression of [0u8; 2048];
  let compression = [27, 255, 7, 0, 36, 0, 194, 177, 64, 114, 7];
  // output buffer doesn't have enough space
  let mut output_buffer = [0u8; 2047];

  super::BrotliDecompress(
    &mut io::Cursor::new(compression),
    &mut io::Cursor::new(&mut output_buffer[..]),
  )
  .unwrap_err();
}

#[test]
fn test_dict() {
  let patch: &[u8] = &[
    27, 103, 0, 96, 47, 14, 120, 211, 142, 228, 22, 15, 167, 193, 55, 28, 228, 226, 254, 54, 10,
    36, 226, 192, 19, 76, 50, 8, 169, 92, 9, 197, 47, 12, 211, 114, 34, 175, 18, 241, 122, 134,
    170, 32, 189, 4, 112, 153, 119, 12, 237, 23, 120, 130, 2,
  ];

  let dict: &[u8] = &[
    2, 0, 0, 0, 0, 213, 195, 31, 121, 231, 225, 250, 238, 34, 174, 158, 246, 208, 145, 187, 92, 2,
    0, 0, 4, 0, 0, 0, 46, 0, 0, 0, 0, 0, 11, 123, 105, 100, 125, 46, 105, 102, 116, 95, 116, 107,
    20, 0, 0, 52, 40, 103, 221, 215, 223, 255, 95, 54, 15, 13, 85, 53, 206, 115, 249, 165, 159,
    159, 16, 29, 37, 17, 114, 1, 163, 2, 16, 33, 51, 4, 32, 0, 226, 29, 19, 88, 254, 195, 129, 23,
    25, 22, 8, 19, 21, 41, 130, 136, 51, 8, 67, 209, 52, 204, 204, 70, 199, 130, 252, 47, 16, 40,
    186, 251, 62, 63, 19, 236, 147, 240, 211, 215, 59,
  ];

  let expected: &[u8] = &[
    0x02, 0x00, 0x00, 0x00, 0x00, 0x8c, 0x16, 0xa6, 0x25, 0x18, 0xf8, 0x68, 0x63, 0x4e, 0xe4,
    0x09, 0x2b, 0xa1, 0xe2, 0x4b, 0xba, 0x02, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x2e, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0b, 0x7b, 0x69, 0x64, 0x7d, 0x2e, 0x69, 0x66, 0x74, 0x5f, 0x74,
    0x6b, 0x14, 0x00, 0x00, 0x38, 0x1d, 0x25, 0x11, 0x72, 0x01, 0xa3, 0x02, 0x10, 0x21, 0x33,
    0x04, 0x20, 0x00, 0xe2, 0x1d, 0x13, 0x58, 0xfe, 0xc3, 0x81, 0x17, 0x19, 0x16, 0x08, 0x13,
    0x15, 0x29, 0x82, 0x88, 0x33, 0x08, 0x43, 0xd1, 0x34, 0xcc, 0xcc, 0x46, 0xc7, 0x82, 0xfc,
    0x2f, 0x10, 0x28, 0xba, 0xfb, 0x3e, 0x3f, 0x13, 0xec, 0x93, 0xf0, 0xd3, 0xd7, 0x3b,
  ];

  let mut input = [0u8; 64];
  input[..patch.len()].clone_from_slice(patch);
  let mut output = [0u8; 256];

  let (result, _input_offset, output_offset) =
    oneshot_dict(&mut input[..patch.len()], dict, &mut output[..]);

  match result {
    BrotliResult::ResultSuccess => {}
    _ => panic!("Unexpected result {:?}", result),
  }
  assert_eq!(output_offset, expected.len());
  assert_eq!(&output[..output_offset], expected);
}



#[test]
fn test_dict_medium() {
  let br: &[u8] = &[
      27, 250, 0, 64, 44, 11, 108, 247, 52, 24, 142, 163, 204, 142, 80, 252, 182, 120, 165, 79, 250, 13, 158, 3, 30, 234, 40, 250, 29, 20, 77, 120, 230, 200, 129, 115, 130, 14, 64, 215, 56, 237, 122, 86, 138, 52, 110, 119, 30, 215, 82, 74, 30, 171, 105, 88, 99, 31, 14, 167, 214, 226, 231, 246, 248, 42, 94, 190, 205, 223, 231, 243, 213, 253, 63, 108, 192, 137, 120, 140, 143, 190, 202, 64, 147, 222, 31, 143, 132, 147, 173, 58, 1, 126, 218, 171, 171, 199, 239, 64, 16, 232, 46, 13, 155, 237, 189, 161, 186, 4, 147, 245, 53, 148, 218, 183, 80, 50, 59, 249, 130, 113, 103, 219, 228, 206, 36, 150, 127, 93, 210, 225, 40, 54, 247, 51, 28, 139, 149, 194, 210, 171, 62, 190, 158, 203, 35, 87, 91, 43, 9, 5, 0, 28, 217, 82, 157, 50, 63, 118, 229, 72, 167, 108, 155, 216, 214, 2, 116, 200, 103, 42, 194, 63, 159, 85, 202, 72, 167, 142, 139, 27, 106, 104, 251, 151, 64, 122, 231, 226, 114, 39, 28, 49, 117, 70, 13, 65, 119, 69, 181, 42, 87, 152, 223, 0
  ];

  let mut dict: [u8; 256] = [0u8; 256];
  for (index, val) in dict[..].iter_mut().enumerate()
  {
    *val = index as u8;
  }

  let expected: &[u8] = &[148, 100, 52, 4, 5, 6, 7, 214, 165, 116, 67, 18, 225, 176, 127, 78, 29, 235, 185, 135, 85, 35, 241, 191, 141, 91, 41, 247, 196, 145, 94, 43, 248, 197, 146, 95, 44, 249, 198, 146, 94, 42, 246, 194, 142, 90, 38, 242, 190, 138, 85, 32, 235, 182, 129, 76, 23, 226, 173, 120, 67, 13, 215, 161, 107, 53, 255, 201, 147, 93, 39, 241, 186, 131, 76, 21, 222, 167, 112, 57, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 215, 158, 101, 44, 243, 186, 129, 72, 15, 16, 17, 215, 157, 99, 41, 239, 181, 123, 65, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 215, 155, 95, 35, 231, 171, 111, 51, 247, 187, 127, 66, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 216, 154, 92, 29, 222, 159, 96, 33, 226, 163, 100, 37, 230, 167, 103, 39, 231, 167, 103, 39, 231, 167, 103, 39, 231, 166, 101, 36, 227, 162, 97, 32, 223, 158, 93, 28, 218, 152, 86, 20, 21, 22, 23, 24, 25, 26, 27, 216, 149, 82, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 217, 149, 81, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 217, 147, 77, 7, 8, 9, 10];

  let mut input = [0u8; 256];
  input[..br.len()].clone_from_slice(br);
  let mut output = [0u8; 512];

  let (result, _input_offset, output_offset) =
    oneshot_dict(&mut input[..br.len()], &dict[..], &mut output[..]);

  match result {
    BrotliResult::ResultSuccess => {}
    _ => panic!("Unexpected result {:?}", result),
  }
  assert_eq!(output_offset, expected.len());
  assert_eq!(&output[..output_offset], expected);
}

#[cfg(feature="std")]
fn decode_stream_chunked_std(
  compressed: &[u8],
  in_chunks: &[usize],
  out_chunk: usize,
) -> Result<Vec<u8>, (BrotliResult, Vec<u8>)> {
  let mut state = BrotliState::new(
    super::StandardAlloc::default(),
    super::StandardAlloc::default(),
    super::StandardAlloc::default(),
  );
  let mut out = Vec::new();
  let mut out_buf = vec![0u8; out_chunk.max(1)];
  let mut in_pos = 0usize;
  let mut step = 0usize;
  let mut avail_in = 0usize;
  let mut in_off = 0usize;
  let mut slice_end = 0usize;

  loop {
    if avail_in == 0 && in_pos < compressed.len() {
      let c = in_chunks[step % in_chunks.len()].max(1);
      step += 1;
      slice_end = in_pos.saturating_add(c).min(compressed.len());
      avail_in = slice_end - in_pos;
      in_off = 0;
    }
    let in_slice = &compressed[in_pos..slice_end];
    let mut avail_out = out_buf.len();
    let mut out_off = 0usize;
    let mut total_out = 0usize;
    let res = BrotliDecompressStream(
      &mut avail_in,
      &mut in_off,
      in_slice,
      &mut avail_out,
      &mut out_off,
      &mut out_buf,
      &mut total_out,
      &mut state,
    );
    out.extend_from_slice(&out_buf[..out_off]);
    if avail_in == 0 {
      in_pos = slice_end;
      in_off = 0;
    }
    match res {
      BrotliResult::ResultSuccess => return Ok(out),
      BrotliResult::NeedsMoreInput => {
        if in_pos >= compressed.len() && avail_in == 0 {
          return Err((res, out));
        }
      }
      BrotliResult::NeedsMoreOutput => {
        if out_off == 0 {
          return Err((res, out));
        }
      }
      BrotliResult::ResultFailure => return Err((res, out)),
    }
  }
}

// Exercises the fast-to-safe bit-reader handoff at chunk boundaries
// (`27`/`28`/`29` bytes for `ProcessCommands`, `17`/`18`/`19` bytes for
// `ReadSymbolCodeLengths`, `1`-byte in/out, and prime steps), plus
// deterministic truncations and single-bit corruptions.
#[test]
#[cfg(feature="std")]
fn test_streaming_chunk_boundaries_and_corruptions() {
  let cases: &[(&str, &[u8], &[u8])] = &[
    (
      "alice29",
      include_bytes!("../testdata/alice29.txt.compressed"),
      include_bytes!("../testdata/alice29.txt"),
    ),
    (
      "asyoulik",
      include_bytes!("../testdata/asyoulik.txt.compressed"),
      include_bytes!("../testdata/asyoulik.txt"),
    ),
    (
      "lcet10",
      include_bytes!("../testdata/lcet10.txt.compressed"),
      include_bytes!("../testdata/lcet10.txt"),
    ),
    (
      "plrabn12",
      include_bytes!("../testdata/plrabn12.txt.compressed"),
      include_bytes!("../testdata/plrabn12.txt"),
    ),
    (
      "mapsdatazrh",
      include_bytes!("../testdata/mapsdatazrh.compressed"),
      include_bytes!("../testdata/mapsdatazrh"),
    ),
    (
      "flushed_then_block_switches",
      include_bytes!("../testdata/flushed_then_block_switches.compressed"),
      &include_bytes!("../testdata/alice29.txt")[..8300],
    ),
    (
      "monkey",
      include_bytes!("../testdata/monkey.compressed"),
      include_bytes!("../testdata/monkey"),
    ),
    (
      "ukkonooa",
      include_bytes!("../testdata/ukkonooa.compressed"),
      include_bytes!("../testdata/ukkonooa"),
    ),
    (
      "quickfox_repeated",
      include_bytes!("../testdata/quickfox_repeated.compressed"),
      include_bytes!("../testdata/quickfox_repeated"),
    ),
    (
      "empty",
      include_bytes!("../testdata/empty.compressed"),
      include_bytes!("../testdata/empty"),
    ),
  ];

  let patterns: &[(&[usize], usize)] = &[
    (&[usize::MAX], 65536),
    (&[27, 28, 29], 64),
    (&[17, 18, 19], 257),
    (&[7, 13, 1], 3),
    (&[1, 2, 3, 5, 8, 13, 21, 34], 128),
  ];

  for &(name, compressed, expected) in cases {
    for &(in_chunks, out_chunk) in patterns {
      let got = decode_stream_chunked_std(compressed, in_chunks, out_chunk)
        .unwrap_or_else(|(e, _)| panic!("{} failed with {:?} on {:?}", name, e, in_chunks));
      assert_eq!(got.as_slice(), expected, "{} mismatch on {:?}", name, in_chunks);
    }
    // 1-byte in / 1-byte out on the smaller streams to keep debug test time fast.
    if expected.len() <= 32768 {
      let got = decode_stream_chunked_std(compressed, &[1], 1)
        .unwrap_or_else(|(e, _)| panic!("{} failed on 1/1: {:?}", name, e));
      assert_eq!(got.as_slice(), expected, "{} mismatch on 1/1", name);
    }

    // Deterministic truncations must never succeed, panic, or trip a
    // `debug_assert!`, and any partial output emitted before `NeedsMoreInput`
    // must be an exact prefix of `expected`.
    let mut rng: u64 = 0x9e37_79b9_7f4a_7c15 ^ (compressed.len() as u64);
    let mut next_u64 = || {
      rng ^= rng << 13;
      rng ^= rng >> 7;
      rng ^= rng << 17;
      rng
    };
    for _ in 0..32 {
      let cut = (next_u64() as usize) % compressed.len();
      match decode_stream_chunked_std(&compressed[..cut], &[19, 28, 1], 64) {
        Ok(_) => panic!("{} truncated to {} bytes unexpectedly succeeded", name, cut),
        Err((res, out)) => {
          assert!(
            !matches!(res, BrotliResult::ResultSuccess),
            "{} truncated to {} bytes returned {:?}",
            name,
            cut,
            res
          );
          assert!(
            out.len() <= expected.len() && &out[..] == &expected[..out.len()],
            "{} truncated to {} bytes emitted non-prefix output ({} bytes)",
            name,
            cut,
            out.len()
          );
        }
      }
    }
    // Single-bit corruptions must never panic or trip a `debug_assert!`.
    let mut mutated = compressed.to_vec();
    for _ in 0..32 {
      let idx = (next_u64() as usize) % mutated.len();
      let bit = 1u8 << ((next_u64() as usize) & 7);
      mutated[idx] ^= bit;
      let _ = decode_stream_chunked_std(&mutated, &[17, 28, 5], 128);
      mutated[idx] ^= bit;
    }
  }
}
