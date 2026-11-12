#![cfg(all(test, feature="std"))]
// Stream-level tests for metadata meta-blocks and the metadata callbacks
// (BrotliMetadataCallback, and BrotliDecoderSetMetadataCallbacks over FFI).
//
// No encoder emits metadata blocks, so the streams are assembled bit by bit by
// StreamBuilder from four kinds of meta-block: metadata, uncompressed, a tiny
// compressed block (1..=5 copies of one literal, coded with single-symbol
// prefix codes so it takes no bits per symbol), and the empty last block.
// Each test checks the callbacks against the contract in c/brotli/decode.h:
// start fires once per block with its size (possibly 0), then chunks of at
// least one byte follow whose lengths sum to that size.

use std::vec::Vec;
use ::decode::BrotliDecompressStreamWithDictionaryAndMetadata;
use ::state::{BrotliDecoderErrorCode, BrotliMetadataCallback, BrotliState};
use ::{BrotliResult, StandardAlloc};

pub struct StreamBuilder {
  bytes: Vec<u8>,
  bit_count: usize,
  // What a decoder must produce for the blocks added so far.
  pub output: Vec<u8>,
  pub metadata: Vec<Vec<u8>>,
}

impl StreamBuilder {
  // Starts a stream with WBITS = 16 (a single 0 bit).
  pub fn new() -> Self {
    let mut builder = StreamBuilder {
      bytes: Vec::new(),
      bit_count: 0,
      output: Vec::new(),
      metadata: Vec::new(),
    };
    builder.put(1, 0);
    builder
  }

  pub fn put(&mut self, n_bits: u32, value: u32) {
    for i in 0..n_bits {
      if self.bit_count % 8 == 0 {
        self.bytes.push(0);
      }
      let last = self.bytes.len() - 1;
      self.bytes[last] |= (((value >> i) & 1) as u8) << (self.bit_count % 8);
      self.bit_count += 1;
    }
  }

  fn align(&mut self) {
    self.bit_count = (self.bit_count + 7) & !7;
  }

  fn metadata_header(&mut self, is_last: bool, len: usize) {
    assert!(len <= 1 << 24);
    self.put(1, is_last as u32); // ISLAST
    if is_last {
      self.put(1, 0); // ISLASTEMPTY
    }
    self.put(2, 3); // MNIBBLES = 0, i.e. metadata
    self.put(1, 0); // reserved
    let skip_bytes = if len == 0 {
      0
    } else if len - 1 < 1 << 8 {
      1
    } else if len - 1 < 1 << 16 {
      2
    } else {
      3
    };
    self.put(2, skip_bytes); // MSKIPBYTES
    for i in 0..skip_bytes {
      self.put(8, (((len - 1) >> (8 * i)) & 0xff) as u32); // MSKIPLEN - 1
    }
    self.align();
  }

  pub fn metadata(&mut self, data: &[u8]) -> &mut Self {
    self.metadata_header(false, data.len());
    self.bytes.extend_from_slice(data);
    self.bit_count += 8 * data.len();
    self.metadata.push(data.to_vec());
    self
  }

  // A metadata block that is also the last meta-block; the stream is complete
  // afterwards.
  pub fn last_metadata(&mut self, data: &[u8]) -> Vec<u8> {
    self.metadata_header(true, data.len());
    self.bytes.extend_from_slice(data);
    self.bit_count += 8 * data.len();
    self.metadata.push(data.to_vec());
    self.bytes.clone()
  }

  pub fn uncompressed(&mut self, data: &[u8]) -> &mut Self {
    assert!(data.len() >= 1 && data.len() <= 1 << 16);
    self.put(1, 0); // ISLAST
    self.put(2, 0); // MNIBBLES = 4
    self.put(16, (data.len() - 1) as u32);
    self.put(1, 1); // ISUNCOMPRESSED
    self.align();
    self.bytes.extend_from_slice(data);
    self.bit_count += 8 * data.len();
    self.output.extend_from_slice(data);
    self
  }

  // A compressed meta-block of `count` copies of `literal`: one insert-and-copy
  // command (insert code `count`, copy code 0; the meta-block ends before the
  // copy) over single-symbol prefix codes.
  pub fn literals(&mut self, literal: u8, count: usize) -> &mut Self {
    assert!(count >= 1 && count <= 5);
    self.put(1, 0); // ISLAST
    self.put(2, 0); // MNIBBLES = 4
    self.put(16, (count - 1) as u32);
    self.put(1, 0); // ISUNCOMPRESSED
    self.put(1, 0); // NBLTYPESL = 1
    self.put(1, 0); // NBLTYPESI = 1
    self.put(1, 0); // NBLTYPESD = 1
    self.put(2, 0); // NPOSTFIX
    self.put(4, 0); // NDIRECT
    self.put(2, 0); // context mode of the literal block type
    self.put(1, 0); // NTREESL = 1
    self.put(1, 0); // NTREESD = 1
    // Simple prefix codes (HSKIP = 1) with one symbol each.
    self.put(2, 1); self.put(2, 0); self.put(8, literal as u32);
    self.put(2, 1); self.put(2, 0); self.put(10, (count * 8) as u32);
    self.put(2, 1); self.put(2, 0); self.put(6, 0);
    for _ in 0..count {
      self.output.push(literal);
    }
    self
  }

  // Ends the stream with an empty last meta-block.
  pub fn finish(&mut self) -> Vec<u8> {
    self.put(1, 1); // ISLAST
    self.put(1, 1); // ISLASTEMPTY
    self.bytes.clone()
  }
}

pub fn pattern(len: usize, seed: usize) -> Vec<u8> {
  (0..len).map(|i| (i.wrapping_mul(31).wrapping_add(seed * 7 + 1) ^ (i >> 8)) as u8).collect()
}

#[derive(Clone, Debug, PartialEq)]
pub enum Event {
  Start(usize),
  Chunk(Vec<u8>),
}

#[derive(Default)]
pub struct Recorder {
  pub events: Vec<Event>,
}

impl BrotliMetadataCallback for Recorder {
  fn start(&mut self, size: usize) {
    self.events.push(Event::Start(size));
  }
  fn chunk(&mut self, data: &[u8]) {
    self.events.push(Event::Chunk(data.to_vec()));
  }
}

// Checks the event order against the callback contract and returns each
// block's announced size and the bytes delivered for it. Only the last block
// may be short, and only if the stream was truncated inside it.
pub fn blocks(events: &[Event]) -> Vec<(usize, Vec<u8>)> {
  let mut blocks: Vec<(usize, Vec<u8>)> = Vec::new();
  for event in events {
    match *event {
      Event::Start(size) => {
        if let Some(last) = blocks.last() {
          assert_eq!(last.0, last.1.len(), "start before the previous block was complete");
        }
        blocks.push((size, Vec::new()));
      }
      Event::Chunk(ref data) => {
        assert!(!data.is_empty(), "empty chunk");
        let block = blocks.last_mut().expect("chunk before start");
        block.1.extend_from_slice(data);
        assert!(block.1.len() <= block.0, "chunks exceed the announced size");
      }
    }
  }
  blocks
}

pub fn complete_blocks(events: &[Event]) -> Vec<Vec<u8>> {
  blocks(events).into_iter().map(|(size, data)| {
    assert_eq!(size, data.len(), "incomplete metadata block");
    data
  }).collect()
}

pub struct Outcome {
  pub result: BrotliResult,
  pub error_code: BrotliDecoderErrorCode,
  pub output: Vec<u8>,
  // Input bytes the decoder reports consumed, and those left unconsumed.
  pub consumed: usize,
  pub remaining: usize,
  pub events: Vec<Event>,
}

impl Outcome {
  pub fn succeeded(&self) -> bool {
    match self.result {
      BrotliResult::ResultSuccess => true,
      _ => false,
    }
  }
  pub fn needs_more_input(&self) -> bool {
    match self.result {
      BrotliResult::NeedsMoreInput => true,
      _ => false,
    }
  }
}

// Decodes `input`, handing the decoder input up to each of `feed_points` in
// turn and then the rest, with at most `out_step` bytes of output space per
// call.
pub fn decode(input: &[u8], feed_points: &[usize], out_step: usize, with_callback: bool) -> Outcome {
  let mut s = BrotliState::new(StandardAlloc::default(), StandardAlloc::default(),
                               StandardAlloc::default());
  let mut recorder = Recorder::default();
  let mut output = Vec::new();
  let mut out_buf = vec![0u8; out_step];
  let mut input_offset = 0usize;
  let mut total_out = 0usize;
  let mut available_in = 0usize;
  let mut result = BrotliResult::NeedsMoreInput;
  let mut ends: Vec<usize> = feed_points.iter().map(|&p| p.min(input.len())).collect();
  ends.push(input.len());
  'feed: for &end in ends.iter() {
    if end < input_offset {
      continue;
    }
    available_in = end - input_offset;
    loop {
      let mut available_out = out_step;
      let mut output_offset = 0usize;
      result = BrotliDecompressStreamWithDictionaryAndMetadata(
        &mut available_in, &mut input_offset, &input[..end],
        &mut available_out, &mut output_offset, &mut out_buf[..],
        &mut total_out, &[],
        if with_callback { Some(&mut recorder as &mut dyn BrotliMetadataCallback) } else { None },
        &mut s);
      output.extend_from_slice(&out_buf[..output_offset]);
      match result {
        BrotliResult::NeedsMoreOutput => continue,
        BrotliResult::NeedsMoreInput => {
          assert_eq!(available_in, 0, "decoder asked for input it was already given");
          continue 'feed;
        }
        _ => break 'feed,
      }
    }
  }
  Outcome {
    result: result,
    error_code: s.error_code,
    output: output,
    consumed: input_offset,
    remaining: available_in,
    events: recorder.events,
  }
}

fn feed_every(step: usize, len: usize) -> Vec<usize> {
  (1..).map(|i| i * step).take_while(|&p| p < len).collect()
}

// Decodes `stream` whole, one byte at a time, and split once at every
// position, checking each run against what `builder` says it holds and
// against a run without a callback.
fn check_stream(stream: &[u8], builder: &StreamBuilder) {
  let mut schedules: Vec<Vec<usize>> = vec![Vec::new(), feed_every(1, stream.len()),
                                            feed_every(7, stream.len())];
  for split in 1..stream.len() {
    schedules.push(vec![split]);
  }
  for schedule in schedules.iter() {
    for &out_step in [1usize, 1 << 16].iter() {
      let with = decode(stream, schedule, out_step, true);
      assert!(with.succeeded(), "schedule {:?}: {:?} {:?}", schedule, with.result, with.error_code);
      assert_eq!(with.output, builder.output, "schedule {:?}", schedule);
      assert_eq!(complete_blocks(&with.events), builder.metadata, "schedule {:?}", schedule);
      assert_eq!(with.consumed, stream.len());

      let without = decode(stream, schedule, out_step, false);
      assert!(without.succeeded());
      assert_eq!(without.output, with.output);
      assert_eq!(without.consumed, with.consumed);
      assert!(without.events.is_empty());
    }
  }
}

#[test]
fn metadata_of_every_length_class() {
  // MSKIPBYTES = 0, 1, 2 and 3, with each byte-count boundary.
  for &len in [0usize, 1, 2, 7, 8, 9, 255, 256, 257, 65535, 65536, 65537, 70000].iter() {
    let data = pattern(len, len);
    let mut builder = StreamBuilder::new();
    let stream = builder.metadata(&data).finish();
    for schedule in [Vec::new(), feed_every(1, stream.len()), feed_every(4093, stream.len())].iter() {
      let outcome = decode(&stream, schedule, 1 << 16, true);
      assert!(outcome.succeeded(), "len {}: {:?}", len, outcome.error_code);
      assert!(outcome.output.is_empty());
      assert_eq!(outcome.consumed, stream.len());
      assert_eq!(complete_blocks(&outcome.events), vec![data.clone()]);
    }
  }
}

#[test]
fn empty_metadata_block_starts_without_chunks() {
  let mut builder = StreamBuilder::new();
  let stream = builder.metadata(&[]).metadata(&[]).finish();
  let outcome = decode(&stream, &[], 1 << 16, true);
  assert!(outcome.succeeded());
  assert_eq!(outcome.events, vec![Event::Start(0), Event::Start(0)]);
}

fn chunk_lengths(events: &[Event]) -> Vec<usize> {
  events.iter().filter_map(|e| match *e { Event::Chunk(ref data) => Some(data.len()), _ => None }).collect()
}

#[test]
fn whole_input_delivers_metadata_in_at_most_two_chunks() {
  // With the whole stream available, a block is delivered as the bytes still
  // in the bit reader's accumulator, then one direct slice of the input. The
  // compressed block's fast loop (which needs 28 bytes of input ahead) leaves
  // 4 bytes in the 64-bit accumulator.
  let mut builder = StreamBuilder::new();
  let data = pattern(100, 3);
  let stream = builder.literals(b'q', 3).metadata(&data).literals(b'r', 1).finish();
  let outcome = decode(&stream, &[], 1 << 16, true);
  assert!(outcome.succeeded());
  // A 32-bit target's 4-byte accumulator holds none of the block's bytes by
  // then, so the whole block is one direct slice.
  let expected: Vec<usize> =
    if ::core::mem::size_of::<::bit_reader::reg_t>() == 8 { vec![4, 96] } else { vec![100] };
  assert_eq!(chunk_lengths(&outcome.events), expected);
  assert_eq!(complete_blocks(&outcome.events), vec![data]);
}

#[test]
fn metadata_after_compressed_block_drains_the_accumulator() {
  // The compressed block's fast decoding loop prefetches whole words into
  // the bit reader, so the metadata begins inside the accumulator. Short
  // blocks end inside it too, leaving the next header's bits buffered. The
  // 40-byte block after it keeps enough input ahead for the fast loop to run.
  let mut split_deliveries = 0;
  let mut inside_accumulator = 0;
  for count in 1..6 {
    for len in 0..20 {
      let mut builder = StreamBuilder::new();
      let stream = builder.literals(b'a', count).metadata(&pattern(len, count))
                          .metadata(&pattern(40, 1)).literals(b'b', 6 - count).finish();
      check_stream(&stream, &builder);
      let whole = decode(&stream, &[], 1 << 16, true);
      match whole.events.iter().position(|e| *e == Event::Start(40)) {
        Some(2) if len != 0 => inside_accumulator += 1,
        Some(3) => split_deliveries += 1,
        _ => {}
      }
    }
  }
  // Make sure the cases this test is about were reached at all. They need
  // bytes left in the accumulator, which only the 64-bit one has here.
  if ::core::mem::size_of::<::bit_reader::reg_t>() == 8 {
    assert!(split_deliveries > 0);
    assert!(inside_accumulator > 0);
  }
}

#[test]
fn blocks_after_metadata_decode_with_a_cold_bit_reader() {
  // A metadata block bypasses the accumulator and can leave it empty, so the
  // next header is read with no bits buffered and the following compressed
  // block has to warm the bit reader up again.
  for len in [0usize, 1, 3, 7, 8, 9, 16, 33, 300].iter() {
    let data = pattern(*len, 11);
    let mut builder = StreamBuilder::new();
    let stream = builder.metadata(&data).literals(b'x', 5).finish();
    check_stream(&stream, &builder);

    // Enough input after the compressed block for its fast loop to run from
    // the cold reader.
    let mut builder = StreamBuilder::new();
    let stream = builder.metadata(&data).literals(b'w', 3).metadata(&pattern(40, 6)).finish();
    check_stream(&stream, &builder);

    let mut builder = StreamBuilder::new();
    let stream = builder.metadata(&data).uncompressed(b"plain").literals(b'y', 2)
                        .metadata(&data).metadata(&data).literals(b'z', 1).finish();
    check_stream(&stream, &builder);
  }
}

#[test]
fn metadata_as_the_last_meta_block() {
  // Ending the stream in a metadata block runs the end-of-stream byte
  // alignment and accumulator unload straight after the metadata reader.
  for count in 1..6 {
    for len in 0..12 {
      let mut builder = StreamBuilder::new();
      builder.literals(b'L', count);
      let stream = builder.last_metadata(&pattern(len, 5));
      check_stream(&stream, &builder);

      // Bytes after the stream must be left unconsumed, including any the
      // bit reader had already prefetched.
      let mut with_trailer = stream.clone();
      with_trailer.extend_from_slice(b"TRAILING");
      let outcome = decode(&with_trailer, &[], 1 << 16, true);
      assert!(outcome.succeeded(), "{:?}", outcome.error_code);
      assert_eq!(outcome.consumed, stream.len());
      assert_eq!(outcome.remaining, 8);
      assert_eq!(outcome.output, builder.output);
      assert_eq!(complete_blocks(&outcome.events), builder.metadata);
    }
  }
}

#[test]
fn mixed_stream_survives_every_split() {
  let mut builder = StreamBuilder::new();
  let stream = builder.metadata(b"abc").literals(b'1', 4).metadata(&[])
                      .uncompressed(&pattern(10, 1)).metadata(&pattern(300, 2))
                      .literals(b'2', 5).metadata(&pattern(9, 3)).finish();
  check_stream(&stream, &builder);
}

#[test]
fn truncated_metadata_reports_what_arrived() {
  let data = pattern(50, 9);
  let mut builder = StreamBuilder::new();
  let stream = builder.literals(b't', 2).metadata(&data).finish();
  let payload_end = stream.len() - 1; // the final byte is the empty last block
  let payload_start = payload_end - data.len();
  for cut in payload_start..payload_end {
    let outcome = decode(&stream[..cut], &[], 1 << 16, true);
    assert!(outcome.needs_more_input());
    let blocks = blocks(&outcome.events);
    assert_eq!(blocks.len(), 1);
    assert_eq!(blocks[0].0, data.len());
    assert_eq!(&blocks[0].1[..], &data[..cut - payload_start]);
  }
}

#[test]
fn invalid_metadata_headers_fail_before_start() {
  // Reserved bit set.
  let mut builder = StreamBuilder::new();
  builder.put(1, 0); builder.put(2, 3); builder.put(1, 1);
  let stream = builder.finish();
  let outcome = decode(&stream, &[], 1 << 16, true);
  match outcome.result { BrotliResult::ResultFailure => {}, _ => panic!("{:?}", outcome.result) }
  assert_eq!(outcome.error_code as i32,
             BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_RESERVED as i32);
  assert!(outcome.events.is_empty());

  // MSKIPBYTES = 2 whose last byte is zero.
  let mut builder = StreamBuilder::new();
  builder.put(1, 0); builder.put(2, 3); builder.put(1, 0); builder.put(2, 2);
  builder.put(8, 5); builder.put(8, 0);
  let stream = builder.finish();
  let outcome = decode(&stream, &[], 1 << 16, true);
  match outcome.result { BrotliResult::ResultFailure => {}, _ => panic!("{:?}", outcome.result) }
  assert_eq!(outcome.error_code as i32,
             BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_META_NIBBLE as i32);
  assert!(outcome.events.is_empty());

  // Nonzero padding before the metadata bytes.
  let mut builder = StreamBuilder::new();
  builder.put(1, 0); builder.put(2, 3); builder.put(1, 0); builder.put(2, 1);
  builder.put(8, 0); builder.put(1, 1);
  let stream = builder.finish();
  let outcome = decode(&stream, &[], 1 << 16, true);
  match outcome.result { BrotliResult::ResultFailure => {}, _ => panic!("{:?}", outcome.result) }
  // PADDING_1 as in the C reference; PADDING_2 is the end-of-stream padding.
  assert_eq!(outcome.error_code as i32,
             BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_PADDING_1 as i32);
  assert!(outcome.events.is_empty());
}

#[cfg(feature="ffi-api")]
mod ffi {
  use std::ptr;
  use std::vec::Vec;
  use ::ffi::interface::{c_void, BrotliDecoderResult};
  use ::ffi::{BrotliDecoderCreateInstance, BrotliDecoderDecompress, BrotliDecoderDecompressStream,
              BrotliDecoderDestroyInstance, BrotliDecoderSetMetadataCallbacks, BrotliDecoderState};
  use super::{blocks, complete_blocks, feed_every, pattern, Event, StreamBuilder};

  extern "C" fn record_start(opaque: *mut c_void, size: usize) {
    let events = unsafe { &mut *(opaque as *mut Vec<Event>) };
    events.push(Event::Start(size));
  }

  extern "C" fn record_chunk(opaque: *mut c_void, data: *const u8, size: usize) {
    let events = unsafe { &mut *(opaque as *mut Vec<Event>) };
    events.push(Event::Chunk(unsafe { ::core::slice::from_raw_parts(data, size) }.to_vec()));
  }

  fn opaque(events: &mut Vec<Event>) -> *mut c_void {
    events as *mut Vec<Event> as *mut c_void
  }

  // Feeds `input` in pieces ending at `feed_points`; `between` runs before
  // each piece after the first.
  fn stream_decode<F: FnMut(*mut BrotliDecoderState, usize)>(
      state: *mut BrotliDecoderState, input: &[u8], feed_points: &[usize], mut between: F)
      -> (BrotliDecoderResult, Vec<u8>) {
    let mut output = Vec::new();
    let mut ends = feed_points.to_vec();
    ends.push(input.len());
    let mut start = 0;
    let mut result = BrotliDecoderResult::BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT;
    for (piece, &end) in ends.iter().enumerate() {
      if piece != 0 {
        between(state, piece);
      }
      let mut available_in = end - start;
      let mut next_in = input[start..].as_ptr();
      loop {
        let mut buf = [0u8; 64];
        let mut available_out = buf.len();
        let mut next_out = buf.as_mut_ptr();
        result = unsafe {
          BrotliDecoderDecompressStream(state, &mut available_in, &mut next_in,
                                        &mut available_out, &mut next_out, ptr::null_mut())
        };
        output.extend_from_slice(&buf[..buf.len() - available_out]);
        match result {
          BrotliDecoderResult::BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT => continue,
          _ => break,
        }
      }
      start = end - available_in;
      match result {
        BrotliDecoderResult::BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT => {}
        _ => break,
      }
    }
    (result, output)
  }

  fn mixed_stream() -> (Vec<u8>, StreamBuilder) {
    let mut builder = StreamBuilder::new();
    let stream = builder.metadata(b"first").literals(b'c', 3).metadata(&[])
                        .metadata(&pattern(70, 4)).uncompressed(b"u").finish();
    (stream, builder)
  }

  fn succeeded(result: BrotliDecoderResult) -> bool {
    match result {
      BrotliDecoderResult::BROTLI_DECODER_RESULT_SUCCESS => true,
      _ => false,
    }
  }

  #[test]
  fn callbacks_receive_every_block_and_the_opaque_pointer() {
    let (stream, builder) = mixed_stream();
    for schedule in [Vec::new(), feed_every(1, stream.len()), feed_every(5, stream.len())].iter() {
      let mut events = Vec::new();
      let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
      unsafe {
        BrotliDecoderSetMetadataCallbacks(state, Some(record_start), Some(record_chunk),
                                          opaque(&mut events));
      }
      let (result, output) = stream_decode(state, &stream, schedule, |_, _| {});
      unsafe { BrotliDecoderDestroyInstance(state) };
      assert!(succeeded(result));
      assert_eq!(output, builder.output);
      assert_eq!(complete_blocks(&events), builder.metadata);
    }
  }

  #[test]
  fn either_callback_may_be_null() {
    let (stream, builder) = mixed_stream();

    let mut events = Vec::new();
    let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
    unsafe { BrotliDecoderSetMetadataCallbacks(state, Some(record_start), None, opaque(&mut events)) };
    let (result, _) = stream_decode(state, &stream, &[], |_, _| {});
    unsafe { BrotliDecoderDestroyInstance(state) };
    assert!(succeeded(result));
    let starts: Vec<Event> = builder.metadata.iter().map(|m| Event::Start(m.len())).collect();
    assert_eq!(events, starts);

    let mut events = Vec::new();
    let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
    unsafe { BrotliDecoderSetMetadataCallbacks(state, None, Some(record_chunk), opaque(&mut events)) };
    let (result, _) = stream_decode(state, &stream, &[], |_, _| {});
    unsafe { BrotliDecoderDestroyInstance(state) };
    assert!(succeeded(result));
    let mut bytes = Vec::new();
    for event in events.iter() {
      match *event {
        Event::Chunk(ref data) => bytes.extend_from_slice(data),
        Event::Start(_) => panic!("start callback was not set"),
      }
    }
    assert_eq!(bytes, builder.metadata.concat());

    let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
    unsafe { BrotliDecoderSetMetadataCallbacks(state, None, None, ptr::null_mut()) };
    let (result, output) = stream_decode(state, &stream, &[], |_, _| {});
    unsafe { BrotliDecoderDestroyInstance(state) };
    assert!(succeeded(result));
    assert_eq!(output, builder.output);
  }

  #[test]
  fn null_state_is_ignored() {
    unsafe {
      BrotliDecoderSetMetadataCallbacks(ptr::null_mut(), Some(record_start), Some(record_chunk),
                                        ptr::null_mut());
    }
  }

  #[test]
  fn callbacks_can_be_replaced_mid_stream() {
    // Swap callbacks between metadata blocks and, inside a block, between
    // chunks: each recorder sees exactly what arrived while it was installed.
    let mut builder = StreamBuilder::new();
    let first = pattern(20, 1);
    let second = pattern(20, 2);
    let stream = builder.metadata(&first).metadata(&second).finish();
    // WBITS and the first header fill 2 bytes, the second header 2 more.
    let second_data = 2 + first.len() + 2;
    assert_eq!(&stream[second_data..second_data + second.len()], &second[..]);
    let mut early = Vec::new();
    let mut late = Vec::new();
    let late_opaque = opaque(&mut late);
    let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
    unsafe { BrotliDecoderSetMetadataCallbacks(state, Some(record_start), Some(record_chunk), opaque(&mut early)) };
    let (result, _) = stream_decode(state, &stream, &[second_data + 4], |state, _| unsafe {
      BrotliDecoderSetMetadataCallbacks(state, Some(record_start), Some(record_chunk), late_opaque);
    });
    unsafe { BrotliDecoderDestroyInstance(state) };
    assert!(succeeded(result));
    assert_eq!(blocks(&early), vec![(20, first), (20, second[..4].to_vec())]);
    let late_bytes: Vec<u8> = late.iter().flat_map(|e| match *e {
      Event::Chunk(ref data) => data.clone(),
      Event::Start(_) => panic!("unexpected start"),
    }).collect();
    assert_eq!(&late_bytes[..], &second[4..]);
  }

  #[test]
  fn one_shot_decode_skips_metadata() {
    let (stream, builder) = mixed_stream();
    let mut output = vec![0u8; 64];
    let mut decoded_size = output.len();
    let result = unsafe {
      BrotliDecoderDecompress(stream.len(), stream.as_ptr(), &mut decoded_size, output.as_mut_ptr())
    };
    assert!(succeeded(result));
    assert_eq!(&output[..decoded_size], &builder.output[..]);
  }
}

