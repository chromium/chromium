extern crate brotli_decompressor;

use brotli_decompressor::{
  Allocator, BrotliDecoderIsFinished, BrotliDecompressStream, BrotliResult, BrotliState,
  HuffmanCode, SliceWrapper, SliceWrapperMut,
};
use std::marker::PhantomData;

// Supply our own allocator so these streaming tests also exercise the no_std
// library build, without enabling its optional std allocator dependency.
struct Memory<T>(Vec<T>);

impl<T> Default for Memory<T> {
  fn default() -> Self {
    Memory(Vec::new())
  }
}
impl<T> SliceWrapper<T> for Memory<T> {
  fn slice(&self) -> &[T] {
    &self.0
  }
}
impl<T> SliceWrapperMut<T> for Memory<T> {
  fn slice_mut(&mut self) -> &mut [T] {
    &mut self.0
  }
}

#[derive(Default)]
struct Heap<T>(PhantomData<T>);

impl<T: Clone + Default> Allocator<T> for Heap<T> {
  type AllocatedMemory = Memory<T>;
  fn alloc_cell(&mut self, size: usize) -> Memory<T> {
    Memory(vec![T::default(); size])
  }
  fn free_cell(&mut self, _memory: Memory<T>) {}
}

struct Decoder {
  state: BrotliState<Heap<u8>, Heap<u32>, Heap<HuffmanCode>>,
  output: Vec<u8>,
  output_limit: usize,
  calls: usize,
}

impl Decoder {
  fn new(output_limit: usize) -> Self {
    Decoder {
      state: BrotliState::new(Heap::default(), Heap::default(), Heap::default()),
      output: Vec::new(),
      output_limit,
      calls: 0,
    }
  }

  fn feed(
    &mut self,
    input: &[u8],
    output_chunk: usize,
    pause_output: bool,
  ) -> (BrotliResult, usize) {
    assert!(output_chunk != 0);
    let mut available_in = input.len();
    let mut input_offset = 0;
    let mut output = vec![0u8; output_chunk];
    loop {
      let out_len = if pause_output && self.calls % 5 == 0 {
        0
      } else {
        output.len()
      };
      let mut available_out = out_len;
      let mut output_offset = 0;
      let mut total_out = self.output.len();
      let result = BrotliDecompressStream(
        &mut available_in,
        &mut input_offset,
        input,
        &mut available_out,
        &mut output_offset,
        &mut output[..out_len],
        &mut total_out,
        &mut self.state,
      );
      self.calls += 1;
      assert_eq!(available_in + input_offset, input.len());
      assert_eq!(available_out + output_offset, out_len);
      self.output.extend_from_slice(&output[..output_offset]);
      // Together with the full-output assertion below, this bounds retries:
      // each non-paused NeedsMoreOutput call must produce bytes.
      assert!(
        self.output.len() <= self.output_limit,
        "decoded beyond the expected output"
      );
      assert_eq!(total_out, self.output.len());
      match result {
        BrotliResult::NeedsMoreOutput => assert_eq!(available_out, 0),
        BrotliResult::NeedsMoreInput => {
          assert_eq!(available_in, 0);
          return (result, input_offset);
        }
        _ => return (result, input_offset),
      }
    }
  }
}

fn fixtures() -> Vec<(&'static str, &'static [u8], &'static [u8])> {
  vec![
    (
      "empty",
      include_bytes!("../testdata/empty.compressed"),
      include_bytes!("../testdata/empty"),
    ),
    (
      "literal",
      include_bytes!("../testdata/x.compressed"),
      include_bytes!("../testdata/x"),
    ),
    (
      "copies",
      include_bytes!("../testdata/10x10y.compressed"),
      include_bytes!("../testdata/10x10y"),
    ),
    (
      "text",
      include_bytes!("../testdata/quickfox.compressed"),
      include_bytes!("../testdata/quickfox"),
    ),
    (
      "utf8",
      include_bytes!("../testdata/ukkonooa.compressed"),
      include_bytes!("../testdata/ukkonooa"),
    ),
    (
      "long distance",
      include_bytes!("../testdata/backward65536.compressed"),
      include_bytes!("../testdata/backward65536"),
    ),
    (
      "dictionary",
      include_bytes!("../testdata/alice29.txt.compressed"),
      include_bytes!("../testdata/alice29.txt"),
    ),
    (
      "mixed contexts",
      include_bytes!("../testdata/random_then_unicode.compressed"),
      include_bytes!("../testdata/random_then_unicode"),
    ),
    (
      "metablocks",
      include_bytes!("../testdata/metablock_reset.compressed"),
      include_bytes!("../testdata/metablock_reset"),
    ),
  ]
}

#[test]
fn streaming_matches_fixtures_at_refill_and_command_boundaries() {
  // The fast command path needs 28 input bytes. Also cross the 4- and 8-byte
  // load boundaries and keep input/output fragmentation independent.
  for (name, input, expected) in fixtures() {
    for &(input_chunk, output_chunk) in &[
      (1, 1),
      (4, 7),
      (7, 13),
      (8, 31),
      (27, 257),
      (28, 4096),
      (29, 17),
      (32, 33),
      (4096, 8192),
    ] {
      // Short fixtures cover byte-by-byte operation; large fixtures concentrate
      // on transitions into the fast command path and bulk input.
      if expected.len() > 1024 && input_chunk < 27 {
        continue;
      }
      let mut decoder = Decoder::new(expected.len());
      let mut consumed = 0;
      for chunk in input.chunks(input_chunk) {
        let (result, used) = decoder.feed(chunk, output_chunk, false);
        consumed += used;
        if consumed == input.len() {
          assert!(matches!(result, BrotliResult::ResultSuccess), "{}", name);
        } else {
          assert!(matches!(result, BrotliResult::NeedsMoreInput), "{}", name);
        }
      }
      assert_eq!(consumed, input.len(), "{}", name);
      assert!(
        decoder.output == expected,
        "{}: input {}, output {}",
        name,
        input_chunk,
        output_chunk
      );
      assert!(BrotliDecoderIsFinished(&decoder.state));
    }
  }
}

#[test]
fn every_short_stream_prefix_can_resume() {
  for (name, input, expected) in fixtures()
    .into_iter()
    .filter(|&(_, input, output)| input.len() <= 80 && output.len() <= 1024)
  {
    for split in 0..input.len() {
      let mut decoder = Decoder::new(expected.len());
      let (result, used) = decoder.feed(&input[..split], 3, false);
      assert!(
        matches!(result, BrotliResult::NeedsMoreInput),
        "{}: split {}",
        name,
        split
      );
      assert_eq!(used, split);
      assert!(
        expected.starts_with(&decoder.output),
        "{}: split {}",
        name,
        split
      );
      assert!(!BrotliDecoderIsFinished(&decoder.state));
      let (result, used) = decoder.feed(&input[split..], 7, false);
      assert!(
        matches!(result, BrotliResult::ResultSuccess),
        "{}: split {}",
        name,
        split
      );
      assert_eq!(used, input.len() - split);
      assert!(decoder.output == expected, "{}: split {}", name, split);
    }
  }
}

#[test]
fn output_backpressure_preserves_trailing_bytes() {
  const TRAILER: &[u8] = b"trailing data belongs to the caller";
  for (name, input, expected) in fixtures() {
    let mut storage = vec![0x55; 7]; // Also start the input at an unaligned address.
    storage.extend_from_slice(input);
    storage.extend_from_slice(TRAILER);
    for &output_chunk in &[1, 17, 4096] {
      if expected.len() > 1024 && output_chunk == 1 {
        continue;
      }
      let mut decoder = Decoder::new(expected.len());
      let (result, consumed) = decoder.feed(&storage[7..], output_chunk, true);
      assert!(matches!(result, BrotliResult::ResultSuccess), "{}", name);
      assert_eq!(consumed, input.len(), "{}", name);
      assert_eq!(&storage[7 + consumed..], TRAILER);
      assert!(decoder.output == expected, "{}", name);
      assert!(BrotliDecoderIsFinished(&decoder.state));
    }
  }
}

#[test]
fn truncated_fast_path_tails_resume_with_one_byte_chunks() {
  for (name, input, expected) in fixtures()
    .into_iter()
    .filter(|&(_, input, _)| input.len() > 80)
  {
    for tail in 1..33 {
      let split = input.len() - tail;
      let mut decoder = Decoder::new(expected.len());
      let (result, used) = decoder.feed(&input[..split], 4096, true);
      assert!(
        matches!(result, BrotliResult::NeedsMoreInput),
        "{}: tail {}",
        name,
        tail
      );
      assert_eq!(used, split);
      for (index, byte) in input[split..].chunks(1).enumerate() {
        let (result, used) = decoder.feed(byte, 17, true);
        assert_eq!(used, 1);
        if index + 1 == tail {
          assert!(
            matches!(result, BrotliResult::ResultSuccess),
            "{}: tail {}",
            name,
            tail
          );
        } else {
          assert!(
            matches!(result, BrotliResult::NeedsMoreInput),
            "{}: tail {}",
            name,
            tail
          );
        }
      }
      assert!(decoder.output == expected, "{}: tail {}", name, tail);
    }
  }
}
