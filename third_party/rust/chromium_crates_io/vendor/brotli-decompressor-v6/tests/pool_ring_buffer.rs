// The one-shot decoders that draw from caller-supplied scratch memory,
// brotli_decode_prealloc (the C API's BrotliDecoderDecompressPrealloc) and the
// no_std brotli_decode, keep their u8 allocations in a pool that never merges
// freed blocks and panics once it runs out, which under panic=abort ends the
// process. Growing the ring buffer on demand can take up to twice the window
// from such a pool. With the same scratch memory, these decoders must still
// decode everything they decoded before the ring buffer grew on demand. So must
// a caller's own pool, for a stream whose data is all in its last metablock,
// and for any stream once the caller starts the ring buffer at the whole
// window with set_initial_ring_buffer_size.

extern crate brotli_decompressor;
#[macro_use]
extern crate alloc_no_stdlib;

use alloc_no_stdlib::{AllocatedStackMemory, Allocator, SliceWrapper, SliceWrapperMut,
                      StackAllocator, bzero};
use brotli_decompressor::{brotli_decode_prealloc, BrotliDecompressStream, BrotliResult,
                          BrotliState, HuffmanCode};
use std::ops;

// The pool allocator brotli_decode_prealloc uses, for a caller that brings its
// own pools to the streaming API.
declare_stack_allocator_struct!(MemPool, 512, stack);

// Bytes allocated past the ring buffer: the write-ahead slack plus one more
// maximal dictionary word.
const RING_BUFFER_SLACK: usize = 542 + 24;

const WINDOW: usize = 1 << 16;

// testdata/x.compressed: one uncompressed byte, "X", in a metablock that is
// not the last one, then an empty last metablock, with a 4 MiB window.
const X_COMPRESSED: [u8; 5] = [0x0b, 0x00, 0x80, 0x58, 0x03];

// Packs fields least significant bit first, as brotli streams are laid out.
struct BitWriter {
  bytes: Vec<u8>,
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

// A stream with a WINDOW-byte window (WBITS=16) of uncompressed metablocks of
// the given lengths, then an empty last one; and the pseudo-random data it
// holds.
fn uncompressed_stream(lengths: &[usize]) -> (Vec<u8>, Vec<u8>) {
  let mut writer = BitWriter { bytes: Vec::new(), bits: 0 };
  writer.put(1, 0); // WBITS=16
  let mut data = Vec::new();
  let mut seed = 1u32;
  for &length in lengths {
    let block: Vec<u8> = (0..length).map(|_| {
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

// Metablocks that add up to more than half the window. Growing on demand, the
// ring buffer would take 1 KiB, 4 KiB, 16 KiB and then the whole window from
// the pool, about 87 KiB in all; before, it took the whole window at once.
fn growing_stream() -> (Vec<u8>, Vec<u8>) {
  uncompressed_stream(&[1000, 3000, 10000, 30000])
}

fn decode_prealloc(input: &[u8], output_len: usize, scratch_u8_len: usize) -> Vec<u8> {
  let mut output = vec![0u8; output_len];
  let mut scratch_u8 = vec![0u8; scratch_u8_len];
  let mut scratch_u32 = vec![0u32; 4 << 10];
  let mut scratch_hc = vec![HuffmanCode::default(); 20 << 10];
  let info = brotli_decode_prealloc(input, &mut output, &mut scratch_u8, &mut scratch_u32,
                                    &mut scratch_hc);
  assert!(matches!(info.result, BrotliResult::ResultSuccess), "decoding failed");
  output.truncate(info.decoded_size);
  output
}

// Exactly the ring buffer this stream needed before: the whole window.
#[test]
fn prealloc_pool_that_holds_the_window_decodes_a_growing_stream() {
  let (input, expected) = growing_stream();
  let output = decode_prealloc(&input, expected.len(), WINDOW + RING_BUFFER_SLACK);
  assert!(output == expected);
}

// Exactly the 32-byte ring buffer this stream needed before, rather than the
// C decoder's 1 KiB minimum.
#[test]
fn prealloc_tiny_pool_decodes_a_tiny_stream() {
  let output = decode_prealloc(&X_COMPRESSED, 1, 32 + RING_BUFFER_SLACK);
  assert_eq!(&output[..], b"X");
}

// A caller's own pool gets no hint about its size, but a stream whose data is
// all in its last metablock still needs no more of it than before: the ring
// buffer never grows after the last metablock, so it can be under 1 KiB. This
// is "hello hello hello" at quality 5, which needed a 64-byte ring buffer plus
// slack, and 1 + 64 + 4 bytes of context modes and maps.
#[test]
fn caller_pool_decodes_a_tiny_last_metablock_as_before() {
  let input = [0xa1, 0x80, 0x00, 0x00, 0x20, 0x01, 0x52, 0x83, 0x20, 0x53, 0x87, 0xe7, 0xf4,
               0x00];
  let mut u8_pool = vec![0u8; 64 + RING_BUFFER_SLACK + 1 + 64 + 4];
  let mut u32_pool = vec![0u32; 4 << 10];
  let mut hc_pool = vec![HuffmanCode::default(); 20 << 10];
  let mut state = BrotliState::new(MemPool::<u8>::new_allocator(&mut u8_pool, bzero),
                                   MemPool::<u32>::new_allocator(&mut u32_pool, bzero),
                                   MemPool::<HuffmanCode>::new_allocator(&mut hc_pool, bzero));
  let mut output = [0u8; 32];
  let mut available_in = input.len();
  let mut input_offset = 0;
  let mut available_out = output.len();
  let mut output_offset = 0;
  let mut total_out = 0;
  let result = BrotliDecompressStream(&mut available_in, &mut input_offset, &input,
                                      &mut available_out, &mut output_offset, &mut output,
                                      &mut total_out, &mut state);
  assert!(matches!(result, BrotliResult::ResultSuccess));
  assert_eq!(&output[..output_offset], b"hello hello hello");
}

// Decodes in one call from the caller's own pools, as the README's manual
// memory management does, with the ring buffer starting at
// initial_ring_buffer_size (0 for the default). None if decoding fails.
fn decode_with_caller_pool(input: &[u8], output_len: usize, u8_pool_len: usize,
                           initial_ring_buffer_size: u32) -> Option<Vec<u8>> {
  let mut u8_pool = vec![0u8; u8_pool_len];
  let mut u32_pool = vec![0u32; 4 << 10];
  let mut hc_pool = vec![HuffmanCode::default(); 20 << 10];
  let mut state = BrotliState::new(MemPool::<u8>::new_allocator(&mut u8_pool, bzero),
                                   MemPool::<u32>::new_allocator(&mut u32_pool, bzero),
                                   MemPool::<HuffmanCode>::new_allocator(&mut hc_pool, bzero));
  assert!(state.set_initial_ring_buffer_size(initial_ring_buffer_size));
  let mut output = vec![0u8; output_len];
  let mut available_in = input.len();
  let mut input_offset = 0;
  let mut available_out = output.len();
  let mut output_offset = 0;
  let mut total_out = 0;
  match BrotliDecompressStream(&mut available_in, &mut input_offset, input,
                               &mut available_out, &mut output_offset, &mut output,
                               &mut total_out, &mut state) {
    BrotliResult::ResultSuccess => {
      output.truncate(output_offset);
      Some(output)
    }
    _ => None,
  }
}

// A caller's own pool that holds the window, all this stream needed before
// growth on demand, decodes it when the ring buffer starts from the whole
// window, needing no more of the pool than before.
#[test]
fn caller_pool_that_holds_the_window_decodes_a_growing_stream_from_the_whole_window() {
  let (input, expected) = growing_stream();
  let pool_len = WINDOW + RING_BUFFER_SLACK;
  assert!(decode_with_caller_pool(&input, expected.len(), pool_len, u32::MAX).as_ref() ==
          Some(&expected));
}

// Growing the ring buffer from the default size instead, the same pool runs
// out, since it cannot reuse the smaller ring buffers left behind, and MemPool
// panics. This is a should_panic test rather than a catch_unwind, so that it
// also runs with panic=abort, where the test harness runs it in its own
// process and nothing can catch the panic.
#[test]
#[should_panic(expected = "OOM")]
fn caller_pool_that_holds_the_window_runs_out_growing_from_the_default_size() {
  let (input, expected) = growing_stream();
  let _ = decode_with_caller_pool(&input, expected.len(), WINDOW + RING_BUFFER_SLACK, 0);
}

// Small or incompressible input, which encoders store as an uncompressed
// metablock and an empty last one, needs no more of a caller's own pool than
// before growth on demand: here a 32-byte ring buffer rather than 1 KiB, or
// the whole 4 MiB window when the ring buffer starts from the whole window.
#[test]
fn caller_pool_decodes_a_tiny_uncompressed_stream_as_before() {
  for &initial_ring_buffer_size in &[0, u32::MAX] {
    let output = decode_with_caller_pool(&X_COMPRESSED, 1, 32 + RING_BUFFER_SLACK,
                                         initial_ring_buffer_size);
    assert!(output.as_ref().map(|output| &output[..]) == Some(&b"X"[..]));
  }
}

// The no_std brotli_decode splits one buffer between the output and the u8
// pool. Each buffer size here is one that decoded before growth on demand but
// leaves the pool too small for growing on demand.
#[cfg(not(feature = "std"))]
#[test]
fn no_std_decode_needs_no_bigger_buffer() {
  fn decode(input: &[u8], buffer_len: usize) -> Vec<u8> {
    let mut buffer = vec![0u8; buffer_len];
    let info = brotli_decompressor::brotli_decode(input, &mut buffer);
    assert!(matches!(info.result, BrotliResult::ResultSuccess), "decoding failed");
    buffer.truncate(info.decoded_size);
    buffer
  }
  // brotli_decode keeps its u32 and HuffmanCode pools, about 1.4 MiB, on the
  // stack.
  std::thread::Builder::new().stack_size(16 << 20).spawn(|| {
    // 44013 bytes of output space, and a 73747-byte pool: room for the whole
    // window but not for the growth.
    let (input, expected) = growing_stream();
    assert!(decode(&input, 115 << 10) == expected);
    // 682 bytes of output space, and a 1366-byte pool.
    assert_eq!(&decode(&X_COMPRESSED, 2048)[..], b"X");
  }).unwrap().join().unwrap();
}
