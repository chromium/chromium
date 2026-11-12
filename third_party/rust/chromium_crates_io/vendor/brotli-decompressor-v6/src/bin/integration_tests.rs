#![cfg(test)]
extern crate core;
use std::io;
#[cfg(feature="std")]
use std::io::{Read,Write};
use core::cmp;
use super::brotli_decompressor::BrotliResult;
use super::brotli_decompressor::BrotliDecompressStream;
use super::brotli_decompressor::BrotliDecompressStreamWithDictionary;
#[cfg(feature="std")]
use super::brotli_decompressor::{Decompressor, DecompressorWriter};
use super::brotli_decompressor::BrotliState;
use super::brotli_decompressor::BrotliDecoderErrorCode;
use super::brotli_decompressor::HuffmanCode;
use super::HeapAllocator;

#[allow(unused_imports)]
use super::alloc_no_stdlib::{Allocator, SliceWrapper, SliceWrapperMut};
use std::time::Duration;
#[cfg(not(feature="disable-timer"))]
use std::time::SystemTime;

struct Buffer {
  data: Vec<u8>,
  read_offset: usize,
}
#[cfg(feature="std")]
struct UnlimitedBuffer {
  data: Vec<u8>,
  read_offset: usize,
}

#[cfg(feature="std")]
impl UnlimitedBuffer {
  pub fn new(buf: &[u8]) -> Self {
    let mut ret = UnlimitedBuffer {
      data: Vec::<u8>::new(),
      read_offset: 0,
    };
    ret.data.extend(buf);
    return ret;
  }
}

#[cfg(feature="std")]
impl io::Read for UnlimitedBuffer {
  fn read(self: &mut Self, buf: &mut [u8]) -> io::Result<usize> {
    let bytes_to_read = cmp::min(buf.len(), self.data.len() - self.read_offset);
    if bytes_to_read > 0 {
      buf[0..bytes_to_read].clone_from_slice(&self.data[self.read_offset..
                                              self.read_offset + bytes_to_read]);
    }
    self.read_offset += bytes_to_read;
    return Ok(bytes_to_read);
  }
}

#[cfg(feature="std")]
impl io::Write for UnlimitedBuffer {
  fn write(self: &mut Self, buf: &[u8]) -> io::Result<usize> {
    self.data.extend(buf);
    return Ok(buf.len());
  }
  fn flush(self: &mut Self) -> io::Result<()> {
    return Ok(());
  }
}


#[cfg(feature="disable-timer")]
fn now() -> Duration {
  return Duration::new(0, 0);
}
#[cfg(not(feature="disable-timer"))]
fn now() -> SystemTime {
  return SystemTime::now();
}

#[cfg(not(feature="disable-timer"))]
fn elapsed(start: SystemTime) -> (Duration, bool) {
  match start.elapsed() {
    Ok(delta) => return (delta, false),
    _ => return (Duration::new(0, 0), true),
  }
}

#[cfg(feature="disable-timer")]
fn elapsed(_start: Duration) -> (Duration, bool) {
  return (Duration::new(0, 0), true);
}


fn _write_all<OutputType>(w: &mut OutputType, buf: &[u8]) -> Result<(), io::Error>
  where OutputType: io::Write
{
  let mut total_written: usize = 0;
  while total_written < buf.len() {
    match w.write(&buf[total_written..]) {
      Err(e) => {
        match e.kind() {
          io::ErrorKind::Interrupted => continue,
          _ => return Err(e),
        }
      }
      Ok(cur_written) => {
        if cur_written == 0 {
          return Err(io::Error::new(io::ErrorKind::UnexpectedEof, "Write EOF"));
        }
        total_written += cur_written;
      }
    }
  }
  Ok(())
}


#[cfg(feature="benchmark")]
const NUM_BENCHMARK_ITERATIONS: usize = 1000;
#[cfg(not(feature="benchmark"))]
const NUM_BENCHMARK_ITERATIONS: usize = 2;

// option_env!("BENCHMARK_MODE").is_some()

pub fn decompress_internal<InputType, OutputType>(r: &mut InputType,
                                                  mut w: &mut OutputType,
                                                  input_buffer_limit: usize,
                                                  output_buffer_limit: usize,
                                                  benchmark_mode: bool)
                                                  -> Result<(), io::Error>
  where InputType: io::Read,
        OutputType: io::Write
{
  let mut total = Duration::new(0, 0);
  let range: usize;
  let mut timing_error: bool = false;
  if benchmark_mode {
    range = NUM_BENCHMARK_ITERATIONS;
  } else {
    range = 1;
  }
  for _i in 0..range {
    let mut brotli_state =
      BrotliState::new(HeapAllocator::<u8> { default_value: 0 },
                       HeapAllocator::<u32> { default_value: 0 },
                       HeapAllocator::<HuffmanCode> { default_value: HuffmanCode::default() });
    let mut input = brotli_state.alloc_u8.alloc_cell(input_buffer_limit);
    let mut output = brotli_state.alloc_u8.alloc_cell(output_buffer_limit);
    let mut available_out: usize = output.slice().len();

    // let amount = try!(r.read(&mut buf));
    let mut available_in: usize = 0;
    let mut input_offset: usize = 0;
    let mut output_offset: usize = 0;
    let mut result: BrotliResult = BrotliResult::NeedsMoreInput;
    loop {
      match result {
        BrotliResult::NeedsMoreInput => {
          input_offset = 0;
          match r.read(input.slice_mut()) {
            Err(e) => {
              match e.kind() {
                io::ErrorKind::Interrupted => continue,
                _ => return Err(e),
              }
            }
            Ok(size) => {
              if size == 0 {
                return Err(io::Error::new(io::ErrorKind::UnexpectedEof, "Read EOF"));
              }
              available_in = size;
            }
          }
        }
        BrotliResult::NeedsMoreOutput => {
            if let Err(e) = _write_all(&mut w, &output.slice()[..output_offset]) {
                return Err(e)
            }
            output_offset = 0;
        }
        BrotliResult::ResultSuccess => break,
        BrotliResult::ResultFailure => panic!("FAILURE"),
      }
      let mut written: usize = 0;
      let start = now();
      result = BrotliDecompressStream(&mut available_in,
                                      &mut input_offset,
                                      &input.slice(),
                                      &mut available_out,
                                      &mut output_offset,
                                      &mut output.slice_mut(),
                                      &mut written,
                                      &mut brotli_state);

      let (delta, err) = elapsed(start);
      if err {
        timing_error = true;
      }
      total = total + delta;
      if output_offset != 0 {
        if let Err(e) = _write_all(&mut w, &output.slice()[..output_offset]) {
          return Err(e)
        }
        output_offset = 0;
        available_out = output.slice().len()
      }
    }
  }
  if timing_error {
    let _r = super::writeln0(&mut io::stderr(), "Timing error");
  } else {
    let _r = super::writeln_time(&mut io::stderr(),
                                 "Iterations; Time",
                                 range as u64,
                                 total.as_secs(),
                                 total.subsec_nanos());
  }
  Ok(())
}

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
impl io::Read for Buffer {
  fn read(self: &mut Self, buf: &mut [u8]) -> io::Result<usize> {
    if self.read_offset == self.data.len() {
      self.read_offset = 0;
    }
    let bytes_to_read = cmp::min(buf.len(), self.data.len() - self.read_offset);
    if bytes_to_read > 0 {
      buf[0..bytes_to_read]
        .clone_from_slice(&self.data[self.read_offset..self.read_offset + bytes_to_read]);
    }
    self.read_offset += bytes_to_read;
    return Ok(bytes_to_read);
  }
}
impl io::Write for Buffer {
  fn write(self: &mut Self, buf: &[u8]) -> io::Result<usize> {
    if self.read_offset == self.data.len() {
      return Ok(buf.len());
    }
    self.data.extend(buf);
    return Ok(buf.len());
  }
  fn flush(self: &mut Self) -> io::Result<()> {
    return Ok(());
  }
}
#[test]
fn test_10x_10y() {
  let in_buf: [u8; 12] = [0x1b, 0x13, 0x00, 0x00, 0xa4, 0xb0, 0xb2, 0xea, 0x81, 0x47, 0x02, 0x8a];
  let mut input = Buffer::new(&in_buf);
  let mut output = Buffer::new(&[]);
  output.read_offset = 20;
  match super::decompress(&mut input, &mut output, 65536, Vec::new()) {
    Ok(_) => {}
    Err(e) => panic!("Error {:?}", e),
  }
  let mut i: usize = 0;
  while i < 10 {
    assert_eq!(output.data[i], 'X' as u8);
    assert_eq!(output.data[i + 10], 'Y' as u8);
    i += 1;
  }
  assert_eq!(output.data.len(), 20);
  assert_eq!(input.read_offset, in_buf.len());
}

#[test]
fn test_10x_10y_one_out_byte() {
  let in_buf: [u8; 12] = [0x1b, 0x13, 0x00, 0x00, 0xa4, 0xb0, 0xb2, 0xea, 0x81, 0x47, 0x02, 0x8a];
  let mut input = Buffer::new(&in_buf);
  let mut output = Buffer::new(&[]);
  output.read_offset = 20;
  match decompress_internal(&mut input, &mut output, 12, 1, false) {
    Ok(_) => {}
    Err(e) => panic!("Error {:?}", e),
  }
  let mut i: usize = 0;
  while i < 10 {
    assert_eq!(output.data[i], 'X' as u8);
    assert_eq!(output.data[i + 10], 'Y' as u8);
    i += 1;
  }
  assert_eq!(output.data.len(), 20);
  assert_eq!(input.read_offset, in_buf.len());
}
#[cfg(feature="std")]
fn reader_helper(in_buf: &[u8], mut desired_buf: &[u8], bufsize : usize) {
  let mut cmp = [0u8; 178];
  let mut input = UnlimitedBuffer::new(&in_buf);
  {
  let mut rdec = Decompressor::new(&mut input, bufsize);
  loop {
    match rdec.read(&mut cmp[..]) {
      Ok(size) => {
        if size == 0 {
          break;
        }
        assert_eq!(cmp[..size], desired_buf[..size]);
        desired_buf = &desired_buf[size..];
      }
      Err(e) => panic!("Error {:?}", e),
    }
  }
  }
  assert_eq!(desired_buf.len(), 0);
}

#[test]
#[cfg(feature="std")]
fn test_reader_64x() {
  reader_helper(include_bytes!("../../testdata/64x.compressed"),
                                           include_bytes!("../../testdata/64x"), 181)

}

#[test]
#[cfg(feature="std")]
fn test_streaming_leftover_buffer() {
  reader_helper(include_bytes!("../../testdata/reducetostream.map.compressed"),
                                           include_bytes!("../../testdata/reducetostream.map"), 8192)
}

#[test]
#[cfg(feature="std")]
fn test_reader_uni() {
  reader_helper(include_bytes!("../../testdata/random_then_unicode.compressed"),
                                           include_bytes!("../../testdata/random_then_unicode"), 121)

}


#[cfg(feature="std")]
fn writer_helper(mut in_buf: &[u8], desired_out_buf: &[u8], buf_size: usize) {
  let output = UnlimitedBuffer::new(&[]);

  {let mut wdec = DecompressorWriter::new(output, 517);
  while in_buf.len() > 0 {
    match wdec.write(&in_buf[..cmp::min(in_buf.len(), buf_size)]) {
      Ok(size) => {
        if size == 0 {
          break;
        }
        in_buf = &in_buf[size..];
      }
      Err(e) => panic!("Error {:?}", e),
    }
  }
   let ub = match wdec.into_inner() {
     Ok(w) => w,
     Err(_) => panic!("error with into_inner"),
   };
     assert_eq!(ub.data.len(), desired_out_buf.len());
  for i in 0..cmp::min(desired_out_buf.len(), ub.data.len()) {
    assert_eq!(ub.data[i], desired_out_buf[i]);
  }

  }
}

#[cfg(feature="std")]
fn writer_early_out_helper(in_buf: &[u8], desired_out_buf: &[u8], buf_size: usize, ibuf: usize) {
  let output = UnlimitedBuffer::new(&[]);

  {let mut wdec = DecompressorWriter::new(output, ibuf);
  if in_buf.len() > 0 {
    match wdec.write(&in_buf[..cmp::min(in_buf.len(), buf_size)]) {
      Ok(_size) => {
      }
      Err(e) => panic!("Error {:?}", e),
    }
  }
   match wdec.into_inner() {
     Err(ub) => {
       assert!(ub.data.len() != 0);
       for i in 0..cmp::min(desired_out_buf.len(), ub.data.len()) {
         assert_eq!(ub.data[i], desired_out_buf[i]);
       }
     },
     Ok(_) => panic!("unreachable"),
   }
  }
}

#[test]
#[cfg(feature="std")]
fn test_writer_64x() {
  writer_helper(include_bytes!("../../testdata/64x.compressed"),
                                           include_bytes!("../../testdata/64x"), 1)

}



#[test]
#[cfg(feature="std")]
fn test_writer_mapsdatazrh() {
  writer_helper(include_bytes!("../../testdata/mapsdatazrh.compressed"),
                include_bytes!("../../testdata/mapsdatazrh"), 512)
    
}

#[test]
#[cfg(feature="std")]
fn test_writer_mapsdatazrh_truncated() {
  writer_early_out_helper(include_bytes!("../../testdata/mapsdatazrh.compressed"),
                                           include_bytes!("../../testdata/mapsdatazrh"), 65536, 65536)

}


#[test]
fn test_10x_10y_byte_by_byte() {
  let in_buf: [u8; 12] = [0x1b, 0x13, 0x00, 0x00, 0xa4, 0xb0, 0xb2, 0xea, 0x81, 0x47, 0x02, 0x8a];
  let mut input = Buffer::new(&in_buf);
  let mut output = Buffer::new(&[]);
  output.read_offset = 20;
  match decompress_internal(&mut input, &mut output, 1, 1, false) {
    Ok(_) => {}
    Err(e) => panic!("Error {:?}", e),
  }
  let mut i: usize = 0;
  while i < 10 {
    assert_eq!(output.data[i], 'X' as u8);
    assert_eq!(output.data[i + 10], 'Y' as u8);
    i += 1;
  }
  assert_eq!(output.data.len(), 20);
  assert_eq!(input.read_offset, in_buf.len());
}


fn assert_decompressed_input_matches_output(input_slice: &[u8],
                                            output_slice: &[u8],
                                            input_buffer_size: usize,
                                            output_buffer_size: usize) {
  let mut input = Buffer::new(input_slice);
  let mut output = Buffer::new(&[]);
  output.read_offset = output_slice.len();
  if input_buffer_size == output_buffer_size {
    match super::decompress(&mut input, &mut output, input_buffer_size, Vec::new()) {
      Ok(_) => {}
      Err(e) => panic!("Error {:?}", e),
    }
  } else {
    match decompress_internal(&mut input,
                              &mut output,
                              input_buffer_size,
                              output_buffer_size,
                              false) {
      Ok(_) => {}
      Err(e) => panic!("Error {:?}", e),
    }
  }
  assert_eq!(output.data.len(), output_slice.len());
  assert_eq!(output.data, output_slice)
}

fn assert_huge_file_input_matches_output(input_slice: &[u8],
                                         output_prefix: &[u8],
                                         output_postfix: &[u8],
                                         size: usize,
                                         rep_gap: usize,
                                         input_buffer_size: usize,
                                         output_buffer_size: usize) {
  let mut input = Buffer::new(input_slice);
  let mut output = Buffer::new(&[]);
  output.read_offset = size;
  if input_buffer_size == output_buffer_size {
    match super::decompress(&mut input, &mut output, input_buffer_size, Vec::new()) {
      Ok(_) => {}
      Err(e) => panic!("Error {:?}", e),
    }
  } else {
    match decompress_internal(&mut input,
                              &mut output,
                              input_buffer_size,
                              output_buffer_size,
                              false) {
      Ok(_) => {}
      Err(e) => panic!("Error {:?}", e),
    }
  }
  assert_eq!(output.data.len(), size);
  assert_eq!(output.data.split_at(output_prefix.len()).0, output_prefix);
  assert_eq!(output.data.split_at(output.data.len() - output_postfix.len()).1, output_postfix);
  assert_eq!(output.data.split_at(output_prefix.len() + rep_gap).1.split_at(output_prefix.len()).0, output_prefix);
  let mut zero_count: usize = 0;
  for item in output.data {
      if item == 0 {
          zero_count += 1;
      }
  }
  let mut nulls_in_input: usize = 0;
  for item in output_prefix.iter().chain(output_prefix.iter().chain(output_postfix.iter())) {
      if *item == 0 {
          nulls_in_input += 1;
      }
  }
  assert_eq!(zero_count - nulls_in_input, size - output_prefix.len() * 2 - output_postfix.len());
}

fn benchmark_decompressed_input(input_slice: &[u8],
                                output_slice: &[u8],
                                input_buffer_size: usize,
                                output_buffer_size: usize) {
  let mut input = Buffer::new(input_slice);
  let mut output = Buffer::new(&[]);
  output.read_offset = output_slice.len();
  match decompress_internal(&mut input,
                            &mut output,
                            input_buffer_size,
                            output_buffer_size,
                            true) {
    Ok(_) => {}
    Err(e) => panic!("Error {:?}", e),
  }
  assert_eq!(output.data.len(), output_slice.len());
  assert_eq!(output.data, output_slice)
}

#[test]
fn test_64x() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/64x.compressed"),
                                           include_bytes!("../../testdata/64x"),
                                           3,
                                           3);
}

#[test]
fn test_random1024() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/random1024.br"),
                                           include_bytes!("../../testdata/random1024"),
                                           65536,
                                           65536);
}
#[test]
fn test_random1024small_buffer() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/random1024.br"),
                                           include_bytes!("../../testdata/random1024"),
                                           1024,
                                           1024);
}

#[test]
fn test_as_you_like_it() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/asyoulik.txt.compressed"),
                                           include_bytes!("../../testdata/asyoulik.txt"),
                                           65536,
                                           65536);
}


#[test]
#[should_panic]
fn test_negative_hypothesis() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/64x"),
                                           include_bytes!("../../testdata/64x"),
                                           3,
                                           3);
}
static ALICE29_BR: &'static [u8] = include_bytes!("../../testdata/alice29.txt.compressed");
static ALICE29: &'static [u8] = include_bytes!("../../testdata/alice29.txt");
#[test]
fn test_alice29() {
  assert_decompressed_input_matches_output(ALICE29_BR, ALICE29, 65536, 65536);
}

#[test]
fn benchmark_alice29() {
  benchmark_decompressed_input(ALICE29_BR, ALICE29, 65536, 65536);
}

#[test]
fn test_alice1() {
  assert_decompressed_input_matches_output(ALICE29_BR, ALICE29, 1, 65536);
}

#[test]
fn test_backward65536() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/backward65536.compressed"),
                                           include_bytes!("../../testdata/backward65536"),
                                           65536,
                                           65536);
}


#[test]
fn test_compressed_file() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/compressed_file.compressed"),
                                           include_bytes!("../../testdata/compressed_file"),
                                           65536,
                                           65536);
}

#[test]
fn test_compressed_repeated() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/compressed_repeated.\
                                                           compressed"),
                                           include_bytes!("../../testdata/compressed_repeated"),
                                           65536,
                                           65536);
}

#[test]
fn test_empty() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty0() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.00"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty1() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.01"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty2() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.02"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty3() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.03"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty4() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.04"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty5() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.05"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty6() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.06"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty7() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.07"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty8() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.08"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty9() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.09"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty10() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.10"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty11() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.11"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty12() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.12"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty13() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.13"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty14() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.14"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty15() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.15"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty16() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.16"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty17() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.17"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}
#[test]
fn test_empty18() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/empty.compressed.18"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}

#[test]
fn lcet10() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/lcet10.txt.compressed"),
                                           include_bytes!("../../testdata/lcet10.txt"),
                                           65536,
                                           65536);
}

#[test]
fn test_mapsdatazrh() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/mapsdatazrh.compressed"),
                                           include_bytes!("../../testdata/mapsdatazrh"),
                                           65536,
                                           65536);
}

#[test]
fn test_monkey() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/monkey.compressed"),
                                           include_bytes!("../../testdata/monkey"),
                                           65536,
                                           65536);
}

#[test]
fn test_monkey1() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/monkey.compressed"),
                                           include_bytes!("../../testdata/monkey"),
                                           1,
                                           1);
}

#[test]
fn test_monkey3() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/monkey.compressed"),
                                           include_bytes!("../../testdata/monkey"),
                                           3,
                                           65536);
}

#[test]
fn test_plrabn12() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/plrabn12.txt.compressed"),
                                           include_bytes!("../../testdata/plrabn12.txt"),
                                           65536,
                                           65536);
}

#[test]
fn test_random_org_10k() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/random_org_10k.bin.\
                                                           compressed"),
                                           include_bytes!("../../testdata/random_org_10k.bin"),
                                           65536,
                                           65536);
}

#[test]
fn test_ukkonooa() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/ukkonooa.compressed"),
                                           include_bytes!("../../testdata/ukkonooa"),
                                           65536,
                                           65536);
}

#[test]
fn test_ukkonooa3() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/ukkonooa.compressed"),
                                           include_bytes!("../../testdata/ukkonooa"),
                                           3,
                                           3);
}

#[test]
fn test_ukkonooa1() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/ukkonooa.compressed"),
                                           include_bytes!("../../testdata/ukkonooa"),
                                           1,
                                           1);
}

#[test]
fn test_x() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/x.compressed"),
                                           include_bytes!("../../testdata/x"),
                                           65536,
                                           65536);
}
#[test]
fn test_x_0() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/x.compressed.00"),
                                           include_bytes!("../../testdata/x"),
                                           65536,
                                           65536);
}
#[test]
fn test_x_1() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/x.compressed.01"),
                                           include_bytes!("../../testdata/x"),
                                           65536,
                                           65536);
}
#[test]
fn test_x_2() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/x.compressed.02"),
                                           include_bytes!("../../testdata/x"),
                                           65536,
                                           65536);
}
#[test]
fn test_x_3() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/x.compressed.03"),
                                           include_bytes!("../../testdata/x"),
                                           65536,
                                           65536);
}

#[test]
fn test_xyzzy() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/xyzzy.compressed"),
                                           include_bytes!("../../testdata/xyzzy"),
                                           65536,
                                           65536);
}

#[test]
fn test_zeros() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/zeros.compressed"),
                                           include_bytes!("../../testdata/zeros"),
                                           65536,
                                           65536);
}


#[test]
fn test_metablock_reset() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/metablock_reset.compressed"),
                                           include_bytes!("../../testdata/metablock_reset"),
                                           65536,
                                           65536);
}

#[test]
fn test_metablock_section_4_distance_symbol_0() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/fuzz502.compressed"),
                                           include_bytes!("../../testdata/fuzz502"),
                                           65536,
                                           65536);
}

#[test]
fn test_intact_distance_ring_buffer0() {
  static BR:&'static[u8] = &[0x1b, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x80, 0xe3, 0xb4, 0x0d, 0x00, 0x00,
                            0x07, 0x5b, 0x26, 0x31, 0x40, 0x02, 0x00, 0xe0, 0x4e, 0x1b, 0xa1, 0x80,
                            0x20, 0x00];
  static OUT:&'static[u8] = b"himselfself";
  assert_decompressed_input_matches_output(BR,
                                           OUT,
                                           65536,
                                           65536);
}

#[test]
fn test_intact_distance_ring_buffer1() {
  static BR:&'static[u8] = &[0x1b, 0x09, 0x00, 0x00, 0x00, 0x00, 0x80, 0xe3, 0xb4, 0x0d, 0x00, 0x00,
    0x07, 0x5b, 0x26, 0x31, 0x40, 0x02, 0x00, 0xe0, 0x4e, 0x1b, 0x21, 0xa0,
    0x20, 0x00
  ];
  static OUT:&'static[u8] = b"scrollroll";
  assert_decompressed_input_matches_output(BR,
                                           OUT,
                                           65536,
                                           65536);
}

#[test]
fn test_intact_distance_ring_buffer2() {
  static BR:&'static[u8] = &[0x1b, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x80, 0xe3, 0xb4, 0x0d, 0x00, 0x00,
    0x07, 0x5b, 0x26, 0x31, 0x40, 0x02, 0x00, 0xe0, 0x4e, 0x1b, 0x41, 0x80,
    0x20, 0x50, 0x10, 0x24, 0x08, 0x06];
  static OUT:&'static[u8] = b"leftdatadataleft";
  assert_decompressed_input_matches_output(BR,
                                           OUT,
                                           65536,
                                           65536);
}

#[test]
fn test_metablock_reset1_65536() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/metablock_reset.compressed"),
                                           include_bytes!("../../testdata/metablock_reset"),
                                           1,
                                           65536);
}

#[test]
fn test_metablock_reset65536_1() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/metablock_reset.compressed"),
                                           include_bytes!("../../testdata/metablock_reset"),
                                           65536,
                                           1);
}

#[test]
fn test_metablock_reset1() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/metablock_reset.compressed"),
                                           include_bytes!("../../testdata/metablock_reset"),
                                           1,
                                           1);
}

#[test]
fn test_metablock_reset3() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/metablock_reset.compressed"),
                                           include_bytes!("../../testdata/metablock_reset"),
                                           3,
                                           3);
}

#[test]
#[should_panic]
fn test_broken_file() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/borked.compressed"),
                                           include_bytes!("../../testdata/empty"),
                                           65536,
                                           65536);
}

#[test]
fn test_ends_with_truncated_dictionary() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/ends_with_truncated_dictionary.\
                                                           compressed"),
                                           include_bytes!("../../testdata/ends_with_truncated_dictionary"),
                                           65536,
                                           65536);
}

// Issue #42: compressed by the C tool with `brotli -w 10 -q 9 -D issue42.dict`;
// the output (the dictionary repeated 16 times, 64KiB) plus the 4KiB dictionary
// far exceeds the 1KiB window, so dictionary references remain in use long
// after the ring buffer has wrapped.
fn issue42_expanded() -> Vec<u8> {
  let dict = include_bytes!("../../testdata/issue42.dict");
  let mut expanded = Vec::<u8>::new();
  for _ in 0..16 {
    expanded.extend_from_slice(dict);
  }
  expanded
}

type DictionaryTestState = BrotliState<HeapAllocator<u8>,
                                       HeapAllocator<u32>,
                                       HeapAllocator<HuffmanCode>>;

fn new_dictionary_test_state() -> DictionaryTestState {
  BrotliState::new(HeapAllocator::<u8> { default_value: 0 },
                   HeapAllocator::<u32> { default_value: 0 },
                   HeapAllocator::<HuffmanCode> { default_value: HuffmanCode::default() })
}

fn new_dictionary_test_state_with_custom_dict(bytes: &[u8]) -> DictionaryTestState {
  let mut alloc_u8 = HeapAllocator::<u8> { default_value: 0 };
  let mut dict = alloc_u8.alloc_cell(bytes.len());
  dict.slice_mut().clone_from_slice(bytes);
  BrotliState::new_with_custom_dictionary(
      alloc_u8,
      HeapAllocator::<u32> { default_value: 0 },
      HeapAllocator::<HuffmanCode> { default_value: HuffmanCode::default() },
      dict)
}

fn attach_test_dictionary(state: &mut DictionaryTestState,
                          bytes: &[u8],
                          serialized: bool) -> bool {
  let mut dict = state.alloc_u8.alloc_cell(bytes.len());
  dict.slice_mut().clone_from_slice(bytes);
  if serialized {
    state.attach_serialized_dictionary(dict)
  } else {
    state.attach_dictionary(dict)
  }
}

// Exercise the allocator-generic streaming core directly. In an `unsafe`
// feature build this reaches the unchecked indexing macros, unlike the former
// StandardAlloc-only reader tests.
fn decode_dictionary_test_state(state: &mut DictionaryTestState,
                                compressed: &[u8]) -> Result<Vec<u8>, ()> {
  let mut available_in = compressed.len();
  let mut input_offset = 0usize;
  let mut total_out = 0usize;
  let mut decoded = Vec::<u8>::new();
  loop {
    let mut output = [0u8; 4096];
    let mut available_out = output.len();
    let mut output_offset = 0usize;
    let result = BrotliDecompressStream(&mut available_in,
                                        &mut input_offset,
                                        compressed,
                                        &mut available_out,
                                        &mut output_offset,
                                        &mut output,
                                        &mut total_out,
                                        state);
    decoded.extend_from_slice(&output[..output_offset]);
    match result {
      BrotliResult::ResultSuccess => return Ok(decoded),
      BrotliResult::ResultFailure => return Err(()),
      BrotliResult::NeedsMoreOutput => {},
      BrotliResult::NeedsMoreInput => {
        if available_in == 0 {
          return Err(());
        }
      },
    }
  }
}

fn decompress_issue42_helper(buffer_size: usize) {
  let mut input = Buffer::new(include_bytes!("../../testdata/issue42.compressed"));
  let mut output = Buffer::new(&[]);
  output.read_offset = 65536;
  match super::decompress(&mut input,
                          &mut output,
                          buffer_size,
                          include_bytes!("../../testdata/issue42.dict").to_vec()) {
    Ok(_) => {}
    Err(e) => panic!("Error {:?}", e),
  }
  assert_eq!(output.data.len(), 65536);
  assert_eq!(output.data, issue42_expanded());
}

#[test]
fn test_custom_dict_exceeds_window() {
  decompress_issue42_helper(65536);
}

#[test]
fn test_custom_dict_exceeds_window_tiny_buffers() {
  // Tiny IO buffers force the interrupted-dictionary-copy resume path
  // (BROTLI_STATE_COMMAND_POST_WRITE_1) and streaming re-entry.
  decompress_issue42_helper(1);
  decompress_issue42_helper(333);
}

#[test]
fn test_custom_dict_exceeds_window_state_constructor() {
  let dict_bytes = include_bytes!("../../testdata/issue42.dict");
  let mut state = new_dictionary_test_state_with_custom_dict(dict_bytes);
  let decoded = decode_dictionary_test_state(
      &mut state, include_bytes!("../../testdata/issue42.compressed")).unwrap();
  assert_eq!(decoded.len(), 65536);
  assert_eq!(decoded, issue42_expanded());
}

// Matches google-brotli/java/org/brotli/dec/CompoundDictionaryTest.java.
const GOOGLE_BROTLI_ONE_COPY: [u8; 11] = [
  0xa1, 0xa8, 0x00, 0xc0, 0x2f, 0x01, 0x10, 0xc4, 0x44, 0x09, 0x00
];
const GOOGLE_BROTLI_COMPOUND_TEXT: &[u8] = b"Kot lomom kolol slona!";

#[test]
fn test_google_brotli_compound_dictionary_one_piece() {
  let mut state = new_dictionary_test_state();
  assert!(attach_test_dictionary(&mut state, GOOGLE_BROTLI_COMPOUND_TEXT, false));
  let decoded = decode_dictionary_test_state(&mut state, &GOOGLE_BROTLI_ONE_COPY).unwrap();
  assert_eq!(decoded, GOOGLE_BROTLI_COMPOUND_TEXT);
}

#[test]
fn test_google_brotli_compound_dictionary_two_pieces() {
  let mut state = new_dictionary_test_state();
  assert!(attach_test_dictionary(
      &mut state, &GOOGLE_BROTLI_COMPOUND_TEXT[..13], false));
  assert!(attach_test_dictionary(
      &mut state, &GOOGLE_BROTLI_COMPOUND_TEXT[13..], false));
  let decoded = decode_dictionary_test_state(&mut state, &GOOGLE_BROTLI_ONE_COPY).unwrap();
  assert_eq!(decoded, GOOGLE_BROTLI_COMPOUND_TEXT);
}

// Attaching a dictionary in pieces is equivalent to attaching it whole:
// chunks occupy consecutive ranges of the same backward-distance space.
#[test]
fn test_attach_dictionary_in_chunks() {
  let dict_bytes = include_bytes!("../../testdata/issue42.dict");
  let mut state = new_dictionary_test_state();
  // Uneven splits so copies cross chunk boundaries mid-command.
  for piece in [&dict_bytes[..1234], &dict_bytes[1234..1235], &dict_bytes[1235..]].iter() {
    assert!(attach_test_dictionary(&mut state, piece, false));
  }
  let decoded = decode_dictionary_test_state(
      &mut state, include_bytes!("../../testdata/issue42.compressed")).unwrap();
  assert_eq!(decoded.len(), 65536);
  assert_eq!(decoded, issue42_expanded());
}

#[test]
fn test_attach_dictionary_too_late_fails() {
  let dict_bytes = include_bytes!("../../testdata/issue42.dict");
  let mut state = new_dictionary_test_state_with_custom_dict(dict_bytes);
  let decoded = decode_dictionary_test_state(
      &mut state, include_bytes!("../../testdata/issue42.compressed")).unwrap();
  // Decoding has begun; further dictionaries must be rejected.
  assert!(!attach_test_dictionary(&mut state, b"late", false));
  assert_eq!(decoded, issue42_expanded());
}

// Serialized shared dictionaries (issue #27). The .dict fixtures are in the
// shared-brotli serialized format (magic 0x91 0x00); the .compressed fixtures
// were produced by the C implementation (BROTLI_EXPERIMENTAL) with the
// dictionary attached at q11, and verified to decode with the C decoder.
// shared_custom carries an LZ77 prefix plus custom word and transform lists;
// shared_context additionally selects between the custom and the built-in
// dictionary through a context map (and exercises the cross-dictionary
// fallback scan).
fn serialized_dict_helper(serialized_dict: &[u8], compressed: &[u8], expected: &[u8]) {
  let mut state = new_dictionary_test_state();
  assert!(attach_test_dictionary(&mut state, serialized_dict, true));
  let decoded = decode_dictionary_test_state(&mut state, compressed).unwrap();
  assert_eq!(decoded, expected);
}

#[test]
fn test_serialized_dictionary_custom_words() {
  serialized_dict_helper(include_bytes!("../../testdata/shared_custom.dict"),
                         include_bytes!("../../testdata/shared_custom.compressed"),
                         include_bytes!("../../testdata/shared_content"));
}

#[test]
fn test_serialized_dictionary_context_map() {
  serialized_dict_helper(include_bytes!("../../testdata/shared_context.dict"),
                         include_bytes!("../../testdata/shared_context.compressed"),
                         include_bytes!("../../testdata/shared_content"));
}

// The borrowed attach path must produce byte-identical output to the copying
// path while actually aliasing the caller's buffer -- that aliasing is the
// whole point, since it lets one dictionary image back an unbounded number of
// concurrent decoders. No unsafe is needed: the 'static bound on the argument
// is what discharges the lifetime obligation, and include_bytes! satisfies it.
fn assert_within(inner: &[u8], outer: &[u8]) {
  let base = outer.as_ptr() as usize;
  let ptr = inner.as_ptr() as usize;
  assert!(ptr >= base && ptr + inner.len() <= base + outer.len(),
          "expected a subslice of the caller's buffer, not a copy");
}

#[test]
fn test_attach_borrowed_dictionary_does_not_copy() {
  let dict_bytes = include_bytes!("../../testdata/issue42.dict");
  let mut state = new_dictionary_test_state();
  assert!(state.attach_dictionary_borrowed(&dict_bytes[..]));
  assert_eq!(state.compound_dictionary.chunks[0].slice().as_ptr(),
             dict_bytes.as_ptr());
  let decoded = decode_dictionary_test_state(
      &mut state, include_bytes!("../../testdata/issue42.compressed")).unwrap();
  assert_eq!(decoded, issue42_expanded());
}

// Two decoders sharing one buffer: neither may hold a private copy, and both
// must decode correctly.
#[test]
fn test_borrowed_dictionary_shared_between_decoders() {
  let dict_bytes = include_bytes!("../../testdata/issue42.dict");
  let compressed = include_bytes!("../../testdata/issue42.compressed");
  let expected = issue42_expanded();
  for _ in 0..2 {
    let mut state = new_dictionary_test_state();
    assert!(state.attach_dictionary_borrowed(&dict_bytes[..]));
    assert_eq!(state.compound_dictionary.chunks[0].slice().as_ptr(),
               dict_bytes.as_ptr());
    assert_eq!(decode_dictionary_test_state(&mut state, compressed).unwrap(), expected);
  }
}

// Borrowed chunks compose with the multi-chunk path, so copies still cross
// chunk boundaries mid-command.
#[test]
fn test_attach_borrowed_dictionary_in_chunks() {
  let dict_bytes = include_bytes!("../../testdata/issue42.dict");
  let mut state = new_dictionary_test_state();
  for piece in [&dict_bytes[..1234], &dict_bytes[1234..1235], &dict_bytes[1235..]].iter() {
    assert!(state.attach_dictionary_borrowed(*piece));
  }
  for i in 0..3 {
    assert_within(state.compound_dictionary.chunks[i].slice(), &dict_bytes[..]);
  }
  let decoded = decode_dictionary_test_state(
      &mut state, include_bytes!("../../testdata/issue42.compressed")).unwrap();
  assert_eq!(decoded, issue42_expanded());
}

// shared_custom carries an LZ77 prefix plus custom word and transform lists,
// so the borrowed path must alias twice: the prefix chunk as a subslice of the
// blob, and the retained blob itself (word/transform offsets index into it for
// the whole decode, not just for parsing).
#[test]
fn test_attach_borrowed_serialized_dictionary_does_not_copy() {
  let dict_bytes = include_bytes!("../../testdata/shared_custom.dict");
  let mut state = new_dictionary_test_state();
  assert!(state.attach_serialized_dictionary_borrowed(&dict_bytes[..]));
  assert_eq!(state.dictionary.blob.slice().as_ptr(), dict_bytes.as_ptr());
  assert_within(state.compound_dictionary.chunks[0].slice(), &dict_bytes[..]);
  let decoded = decode_dictionary_test_state(
      &mut state, include_bytes!("../../testdata/shared_custom.compressed")).unwrap();
  assert_eq!(&decoded[..], &include_bytes!("../../testdata/shared_content")[..]);
}

// Rejection paths must not try to free borrowed memory.
#[test]
fn test_attach_borrowed_dictionary_too_late_fails() {
  let dict_bytes = include_bytes!("../../testdata/issue42.dict");
  let mut state = new_dictionary_test_state();
  assert!(state.attach_dictionary_borrowed(&dict_bytes[..]));
  let decoded = decode_dictionary_test_state(
      &mut state, include_bytes!("../../testdata/issue42.compressed")).unwrap();
  assert!(!state.attach_dictionary_borrowed(b"late"));
  assert!(!state.attach_serialized_dictionary_borrowed(b"\x91\x00garbage"));
  assert_eq!(decoded, issue42_expanded());
}

// External dictionaries: only the size is attached, and the bytes are lent to
// every BrotliDecompressStreamWithDictionary call, so they can live in a
// buffer made at runtime.
const INVALID_ARGUMENTS: i32 =
    BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS as i32;

// Streams `compressed` through the decoder `input_step` bytes in and
// `output_step` bytes out at a time, lending `dictionary` to every call.
fn decode_with_external_dictionary(state: &mut DictionaryTestState,
                                   compressed: &[u8],
                                   dictionary: &[u8],
                                   input_step: usize,
                                   output_step: usize) -> Result<Vec<u8>, i32> {
  let mut consumed = 0usize;
  let mut total_out = 0usize;
  let mut decoded = Vec::<u8>::new();
  let mut output = vec![0u8; output_step];
  loop {
    let input = &compressed[consumed..cmp::min(compressed.len(), consumed + input_step)];
    let mut available_in = input.len();
    let mut input_offset = 0usize;
    let mut available_out = output.len();
    let mut output_offset = 0usize;
    let result = BrotliDecompressStreamWithDictionary(&mut available_in,
                                                      &mut input_offset,
                                                      input,
                                                      &mut available_out,
                                                      &mut output_offset,
                                                      &mut output,
                                                      &mut total_out,
                                                      dictionary,
                                                      state);
    consumed += input_offset;
    decoded.extend_from_slice(&output[..output_offset]);
    match result {
      BrotliResult::ResultSuccess => return Ok(decoded),
      BrotliResult::NeedsMoreOutput => {},
      BrotliResult::NeedsMoreInput if consumed < compressed.len() => {},
      _ => return Err(state.error_code as i32),
    }
  }
}

// Tiny buffers interrupt dictionary copies at the end of the ring buffer and
// resume them on a later call (BROTLI_STATE_COMMAND_POST_WRITE_1), with the
// dictionary lent anew.
#[test]
fn test_external_dictionary_from_a_runtime_buffer() {
  let dictionary = include_bytes!("../../testdata/issue42.dict").to_vec();
  let compressed = include_bytes!("../../testdata/issue42.compressed");
  for &(input_step, output_step) in [(74usize, 65536usize), (1, 1), (7, 333)].iter() {
    let mut state = new_dictionary_test_state();
    assert!(state.attach_external_dictionary(dictionary.len()));
    assert_eq!(decode_with_external_dictionary(
                   &mut state, compressed, &dictionary, input_step, output_step),
               Ok(issue42_expanded()),
               "steps {}/{}", input_step, output_step);
  }
}

// An external dictionary takes its place among the other attached ones, so
// copies cross into and out of it mid-command.
#[test]
fn test_external_dictionary_between_attached_chunks() {
  let dict_bytes = include_bytes!("../../testdata/issue42.dict");
  let compressed = include_bytes!("../../testdata/issue42.compressed");
  let pieces = [&dict_bytes[..1234], &dict_bytes[1234..1235], &dict_bytes[1235..]];
  for external_index in 0..pieces.len() {
    let external = pieces[external_index].to_vec();
    for &(input_step, output_step) in [(74usize, 65536usize), (1, 1), (7, 333)].iter() {
      let mut state = new_dictionary_test_state();
      for (i, piece) in pieces.iter().enumerate() {
        if i == external_index {
          assert!(state.attach_external_dictionary(external.len()));
        } else if i == 0 {
          assert!(attach_test_dictionary(&mut state, piece, false));
        } else {
          assert!(state.attach_dictionary_borrowed(*piece));
        }
      }
      assert_eq!(decode_with_external_dictionary(
                     &mut state, compressed, &external, input_step, output_step),
                 Ok(issue42_expanded()),
                 "external piece {}, steps {}/{}", external_index, input_step, output_step);
    }
  }
}

// Every call must lend a dictionary of exactly the attached size, or none if
// none is attached. Anything else fails, and the failure sticks.
#[test]
fn test_external_dictionary_of_another_size_fails() {
  let dictionary = include_bytes!("../../testdata/issue42.dict").to_vec();
  let compressed = include_bytes!("../../testdata/issue42.compressed");
  let mut longer = dictionary.clone();
  longer.push(0);
  for &(attached, lent) in [(dictionary.len(), &[][..]),
                            (dictionary.len(), &dictionary[1..]),
                            (dictionary.len(), &longer[..]),
                            (0, &dictionary[..])].iter() {
    let mut state = new_dictionary_test_state();
    assert!(state.attach_external_dictionary(attached));
    assert_eq!(decode_with_external_dictionary(&mut state, compressed, lent, 74, 65536),
               Err(INVALID_ARGUMENTS));
    assert_eq!(decode_with_external_dictionary(
                   &mut state, compressed, &dictionary[..attached], 74, 65536),
               Err(INVALID_ARGUMENTS));
  }

  // BrotliDecompressStream lends none.
  let mut state = new_dictionary_test_state();
  assert!(state.attach_external_dictionary(dictionary.len()));
  assert!(decode_dictionary_test_state(&mut state, compressed).is_err());
  assert_eq!(state.error_code as i32, INVALID_ARGUMENTS);

  // The size is checked on every call, not just the first: the first call
  // fills the output, the second fails.
  let mut state = new_dictionary_test_state();
  assert!(state.attach_external_dictionary(dictionary.len()));
  let mut available_in = compressed.len();
  let mut input_offset = 0usize;
  let mut output = [0u8; 100];
  let mut total_out = 0usize;
  for lent in [&dictionary[..], &dictionary[1..]].iter() {
    let mut available_out = output.len();
    let mut output_offset = 0usize;
    let _ = BrotliDecompressStreamWithDictionary(&mut available_in, &mut input_offset,
                                                 compressed, &mut available_out,
                                                 &mut output_offset, &mut output,
                                                 &mut total_out, lent, &mut state);
  }
  assert_eq!(total_out, 100);
  assert_eq!(state.error_code as i32, INVALID_ARGUMENTS);
}

#[test]
fn test_attach_external_dictionary_rejections() {
  let dictionary = include_bytes!("../../testdata/issue42.dict").to_vec();
  let compressed = include_bytes!("../../testdata/issue42.compressed");
  let mut state = new_dictionary_test_state();
  // A zero size is a no-op, and a size no dictionary may have is refused.
  assert!(state.attach_external_dictionary(0));
  assert!(!state.attach_external_dictionary(usize::MAX));
  assert_eq!(state.compound_dictionary.num_chunks, 0);
  // At most one may be attached ...
  assert!(state.attach_external_dictionary(dictionary.len()));
  assert!(!state.attach_external_dictionary(1));
  assert_eq!(decode_with_external_dictionary(
                 &mut state, compressed, &dictionary, 74, 65536),
             Ok(issue42_expanded()));
  // ... only before decoding begins ...
  let mut state = new_dictionary_test_state();
  assert_eq!(decode_with_external_dictionary(&mut state, &compressed[..1], &[], 1, 1),
             Err(BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT as i32));
  assert!(!state.attach_external_dictionary(dictionary.len()));
  // ... and within the limit on attached dictionaries.
  let mut state = new_dictionary_test_state();
  for _ in 0..15 {
    assert!(state.attach_dictionary_borrowed(b"x"));
  }
  assert!(!state.attach_external_dictionary(dictionary.len()));
}

// Other bytes of the attached size, even different ones on each call, give
// wrong output or a decoding error, but never a panic or, in the `unsafe`
// build, an out-of-bounds access.
#[test]
fn test_external_dictionary_with_other_bytes_of_the_same_size() {
  let dictionary = include_bytes!("../../testdata/issue42.dict").to_vec();
  let compressed = include_bytes!("../../testdata/issue42.compressed");
  let zeros = vec![0u8; dictionary.len()];
  let mut state = new_dictionary_test_state();
  assert!(state.attach_external_dictionary(dictionary.len()));
  let mut available_in = compressed.len();
  let mut input_offset = 0usize;
  let mut output = [0u8; 1];
  let mut total_out = 0usize;
  for call in 0.. {
    let lent = if call % 2 == 0 { &dictionary } else { &zeros };
    let mut available_out = 1usize;
    let mut output_offset = 0usize;
    match BrotliDecompressStreamWithDictionary(&mut available_in, &mut input_offset,
                                               compressed, &mut available_out,
                                               &mut output_offset, &mut output,
                                               &mut total_out, lent, &mut state) {
      BrotliResult::NeedsMoreOutput => {},
      _ => break,
    }
  }
}

// A serialized dictionary containing only an LZ77 prefix chunk is equivalent
// to attaching the same bytes as a raw dictionary.
#[test]
fn test_serialized_dictionary_prefix_only_matches_raw() {
  let raw = include_bytes!("../../testdata/issue42.dict");
  let mut serialized = Vec::<u8>::new();
  serialized.extend_from_slice(&[0x91, 0x00]);
  // varint length of the LZ77 prefix chunk
  let mut len = raw.len();
  loop {
    let b = (len & 127) as u8;
    len >>= 7;
    if len != 0 {
      serialized.push(b | 128);
    } else {
      serialized.push(b);
      break;
    }
  }
  serialized.extend_from_slice(raw);
  serialized.push(0); // NUM_WORD_LISTS
  serialized.push(0); // NUM_TRANSFORM_LISTS
  serialized_dict_helper(&serialized,
                         include_bytes!("../../testdata/issue42.compressed"),
                         &issue42_expanded());
}

#[test]
fn test_serialized_dictionary_rejects_garbage() {
  let mut state = new_dictionary_test_state();
  // bad magic
  assert!(!attach_test_dictionary(
      &mut state, &[0x90, 0x00, 0x00, 0x00], true));
  // truncated prefix chunk
  assert!(!attach_test_dictionary(
      &mut state, &[0x91, 0x00, 0x40, 0x00], true));
}

// Differential corpus against the reference C implementation. Every *.br in
// testdata/dict_corpus was compressed by the C encoder with a raw or
// serialized dictionary attached (randomized dictionaries, qualities and
// window sizes) and verified against the C decoder at generation time; see
// scripts/dict_corpus/generate.py for regeneration instructions.
#[test]
fn test_dictionary_corpus() {
  use std::fs;
  let dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
      .join("testdata").join("dict_corpus");
  let mut num_cases = 0;
  let mut entries: Vec<_> = fs::read_dir(&dir).unwrap()
      .map(|e| e.unwrap().path()).collect();
  entries.sort();
  for path in entries {
    let name = path.file_name().unwrap().to_str().unwrap().to_string();
    if !name.ends_with(".br") {
      continue;
    }
    let case_id = name.split('.').next().unwrap();
    let expected = fs::read(dir.join(format!("{}.content", case_id))).unwrap();
    let compressed = fs::read(&path).unwrap();
    let serialized_path = dir.join(format!("{}.serialized.dict", case_id));
    let raw_path = dir.join(format!("{}.raw.dict", case_id));
    let mut state = new_dictionary_test_state();
    if serialized_path.exists() {
      let dict_bytes = fs::read(&serialized_path).unwrap();
      assert!(attach_test_dictionary(&mut state, &dict_bytes, true),
              "attach failed: {}", name);
    } else {
      let dict_bytes = fs::read(&raw_path).unwrap();
      // Constructor-supplied and explicitly attached dictionaries must have
      // identical literal-context behavior; neither seeds the ring buffer.
      let mut constructor_state = new_dictionary_test_state_with_custom_dict(&dict_bytes);
      let constructor_decoded = decode_dictionary_test_state(
          &mut constructor_state, &compressed)
          .unwrap_or_else(|_| panic!("constructor decode failed: {}", name));
      assert_eq!(constructor_decoded, expected,
                 "constructor mismatch for {}", name);
      assert!(attach_test_dictionary(&mut state, &dict_bytes, false),
              "attach failed: {}", name);
    }
    let decoded = decode_dictionary_test_state(&mut state, &compressed)
        .unwrap_or_else(|_| panic!("decode failed: {}", name));
    assert_eq!(decoded, expected, "mismatch for {}", name);
    num_cases += 1;
  }
  // Guard against the corpus silently going missing.
  assert!(num_cases >= 20, "only {} corpus cases found", num_cases);
}

// Deterministic mutation sweep: corrupting any byte of a valid serialized
// dictionary may make attach or decode fail, but must never panic or
// produce out-of-bounds access.
// The shared-dictionary lookup reports two distinct failures, and which one it
// is depends on the context-selected dictionary alone: FORMAT_DICTIONARY when
// that dictionary's word list cannot encode the requested length at all
// (size_bits == 0), FORMAT_TRANSFORM when the address runs past the end of
// every candidate's transforms. Nothing else pinned that distinction, so a
// refactor of the search could collapse the two silently.
//
// The two mutations below were found by sweeping every single-byte change to
// shared_custom.compressed and recording the resulting error code; re-derive
// them the same way if the fixtures ever change.
#[test]
fn test_custom_dictionary_lookup_distinguishes_its_two_failures() {
  let dict_bytes = include_bytes!("../../testdata/shared_custom.dict");
  let compressed = include_bytes!("../../testdata/shared_custom.compressed");
  let cases = [(4usize, 74u8, BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_DICTIONARY),
               (6usize, 210u8, BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_TRANSFORM)];
  for &(pos, delta, expected) in cases.iter() {
    let mut mutated = compressed.to_vec();
    mutated[pos] = mutated[pos].wrapping_add(delta);
    let mut state = new_dictionary_test_state();
    assert!(attach_test_dictionary(&mut state, dict_bytes, true));
    assert!(decode_dictionary_test_state(&mut state, &mutated).is_err());
    assert_eq!(state.error_code as i32, expected as i32,
               "mutation pos={} delta={} gave {:?}", pos, delta, state.error_code);
  }
}

#[test]
fn test_serialized_dictionary_mutation_robustness() {
  let dict_bytes = include_bytes!("../../testdata/shared_custom.dict");
  let compressed = include_bytes!("../../testdata/shared_custom.compressed");
  for pos in 0..dict_bytes.len() {
    for delta in [1u8, 0x80].iter() {
      let mut mutated = dict_bytes.to_vec();
      mutated[pos] = mutated[pos].wrapping_add(*delta);
      let mut state = new_dictionary_test_state();
      if !attach_test_dictionary(&mut state, &mutated, true) {
        continue;
      }
      // Either outcome is fine; only panics/UB would be bugs.
      let _ = decode_dictionary_test_state(&mut state, compressed);
    }
  }
}

#[test]
fn test_random_then_unicode() {
  assert_decompressed_input_matches_output(include_bytes!("../../testdata/random_then_unicode.\
                                                           compressed"),
                                           include_bytes!("../../testdata/random_then_unicode"),
                                           65536,
                                           65536);
}
#[test]
fn test_large_window() {
  assert_huge_file_input_matches_output(include_bytes!("../../testdata/rnd_chunk.br"),
                                        include_bytes!("../../testdata/rnd_prefix"),
                                        include_bytes!("../../testdata/rnd_postfix"),
                                        100011280,
                                        100000000,
                                        1,
                                        16384);
}
