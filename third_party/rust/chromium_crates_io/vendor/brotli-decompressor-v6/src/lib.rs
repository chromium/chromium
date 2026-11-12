#![no_std]
#![allow(non_snake_case)]
#![allow(unused_parens)]
#![allow(unused_imports)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]
#![cfg_attr(feature="no-stdlib-ffi-binding",cfg_attr(not(feature="std"), feature(lang_items)))]
#![cfg_attr(feature="no-stdlib-ffi-binding",cfg_attr(not(feature="std"), feature(panic_handler)))]
// Assert at compile time that the default build contains no unsafe code:
// the only unsafe in this crate lives behind the "unsafe" and "ffi-api" features.
#![cfg_attr(not(any(feature="unsafe", feature="ffi-api")), forbid(unsafe_code))]


#[macro_use]
// <-- for debugging, remove xprintln from bit_reader and replace with println
#[cfg(feature="std")]
extern crate std;
#[cfg(feature="std")]
use std::io::{self, Error, ErrorKind, Read, Write};
#[cfg(feature="std")]
extern crate alloc_stdlib;
#[macro_use]
extern crate alloc_no_stdlib as alloc;
pub use alloc::{AllocatedStackMemory, Allocator, SliceWrapper, SliceWrapperMut, StackAllocator, bzero};
use core::ops;

#[cfg(feature="std")]
pub use alloc_stdlib::StandardAlloc;
#[cfg(all(feature="unsafe",feature="std"))]
pub use alloc_stdlib::HeapAlloc;
#[macro_use]
mod memory;
pub mod dictionary;
mod brotli_alloc;
#[macro_use]
mod bit_reader;
mod huffman;
mod state;
mod prefix;
mod context;
mod shared_dictionary;
pub mod transform;
mod test;
mod test_metadata;
mod decode;
pub mod io_wrappers;
pub mod reader;
pub mod writer;
pub use huffman::{HuffmanCode, HuffmanTreeGroup};
pub use state::BrotliState;
pub use state::BrotliDecoderErrorCode;
pub use state::BrotliDecoderParameter;
pub use shared_dictionary::BrotliSharedDictionary;
#[cfg(feature="ffi-api")]
pub mod ffi;
pub use reader::{DecompressorCustomIo};

#[cfg(feature="std")]
pub use reader::{Decompressor};

pub use writer::{DecompressorWriterCustomIo};
#[cfg(feature="std")]
pub use writer::{DecompressorWriter};

// use io_wrappers::write_all;
pub use io_wrappers::{CustomRead, CustomWrite};
#[cfg(feature="std")]
pub use io_wrappers::{IntoIoReader, IoReaderWrapper, IntoIoWriter, IoWriterWrapper};

// interface
// pub fn BrotliDecompressStream(mut available_in: &mut usize,
//                               input_offset: &mut usize,
//                               input: &[u8],
//                               mut available_out: &mut usize,
//                               mut output_offset: &mut usize,
//                               mut output: &mut [u8],
//                               mut total_out: &mut usize,
//                               mut s: &mut BrotliState<AllocU8, AllocU32, AllocHC>);

pub use decode::{BrotliDecompressStream, BrotliDecompressStreamWithDictionary, BrotliResult, BrotliDecoderHasMoreOutput, BrotliDecoderIsFinished, BrotliDecoderTakeOutput};




#[cfg(not(any(feature="unsafe", not(feature="std"))))]
pub fn BrotliDecompress<InputType, OutputType>(r: &mut InputType,
                                               w: &mut OutputType)
                                               -> Result<(), io::Error>
  where InputType: Read,
        OutputType: Write
{
  let mut input_buffer: [u8; 4096] = [0; 4096];
  let mut output_buffer: [u8; 4096] = [0; 4096];
  BrotliDecompressCustomAlloc(r,
                              w,
                              &mut input_buffer[..],
                              &mut output_buffer[..],
                              StandardAlloc::default(),
                              StandardAlloc::default(),
                              StandardAlloc::default(),
  )
}

#[cfg(feature="std")]
pub fn BrotliDecompressCustomDict<InputType, OutputType>(r: &mut InputType,
                                                         w: &mut OutputType,
                                                         input_buffer:&mut [u8],
                                                         output_buffer:&mut [u8],
                                                         custom_dictionary:std::vec::Vec<u8>)
                                                          -> Result<(), io::Error>
  where InputType: Read,
        OutputType: Write
{
  let mut alloc_u8 = brotli_alloc::BrotliAlloc::<u8>::new();
  let mut input_buffer_backing;
  let mut output_buffer_backing;
  {
  let mut borrowed_input_buffer = input_buffer;
  let mut borrowed_output_buffer = output_buffer;
  if borrowed_input_buffer.len() == 0 {
     input_buffer_backing = alloc_u8.alloc_cell(4096);
     borrowed_input_buffer = input_buffer_backing.slice_mut();
  }
  if borrowed_output_buffer.len() == 0 {
     output_buffer_backing = alloc_u8.alloc_cell(4096);
     borrowed_output_buffer = output_buffer_backing.slice_mut();
  }
  let dict = alloc_u8.take_ownership(custom_dictionary);
  BrotliDecompressCustomIoCustomDict(&mut IoReaderWrapper::<InputType>(r),
                              &mut IoWriterWrapper::<OutputType>(w),
                              borrowed_input_buffer,
                              borrowed_output_buffer,
                              alloc_u8,
                              brotli_alloc::BrotliAlloc::<u32>::new(),
                              brotli_alloc::BrotliAlloc::<HuffmanCode>::new(),
                              dict,
                              Error::new(ErrorKind::UnexpectedEof, "Unexpected EOF"))
  }
}

#[cfg(all(feature="unsafe",feature="std"))]
pub fn BrotliDecompress<InputType, OutputType>(r: &mut InputType,
                                               w: &mut OutputType)
                                               -> Result<(), io::Error>
  where InputType: Read,
        OutputType: Write
{
  let mut input_buffer: [u8; 4096] = [0; 4096];
  let mut output_buffer: [u8; 4096] = [0; 4096];
  BrotliDecompressCustomAlloc(r,
                              w,
                              &mut input_buffer[..],
                              &mut output_buffer[..],
                              HeapAlloc::<u8>::new(0),
                              HeapAlloc::<u32>::new(0),
                              HeapAlloc::<HuffmanCode>::new(HuffmanCode{ bits:2, value: 1}))
}


#[cfg(feature="std")]
pub fn BrotliDecompressCustomAlloc<InputType,
                                   OutputType,
                                   AllocU8: Allocator<u8>,
                                   AllocU32: Allocator<u32>,
                                   AllocHC: Allocator<HuffmanCode>>
  (r: &mut InputType,
   w: &mut OutputType,
   input_buffer: &mut [u8],
   output_buffer: &mut [u8],
   alloc_u8: AllocU8,
   alloc_u32: AllocU32,
   alloc_hc: AllocHC)
   -> Result<(), io::Error>
  where InputType: Read,
        OutputType: Write
{
  BrotliDecompressCustomIo(&mut IoReaderWrapper::<InputType>(r),
                           &mut IoWriterWrapper::<OutputType>(w),
                           input_buffer,
                           output_buffer,
                           alloc_u8,
                           alloc_u32,
                           alloc_hc,
                           Error::new(ErrorKind::UnexpectedEof, "Unexpected EOF"))
}
pub fn BrotliDecompressCustomIo<ErrType,
                                InputType,
                                OutputType,
                                AllocU8: Allocator<u8>,
                                AllocU32: Allocator<u32>,
                                AllocHC: Allocator<HuffmanCode>>
  (r: &mut InputType,
   w: &mut OutputType,
   input_buffer: &mut [u8],
   output_buffer: &mut [u8],
   alloc_u8: AllocU8,
   alloc_u32: AllocU32,
   alloc_hc: AllocHC,
   unexpected_eof_error_constant: ErrType)
   -> Result<(), ErrType>
  where InputType: CustomRead<ErrType>,
        OutputType: CustomWrite<ErrType>
{
  BrotliDecompressCustomIoCustomDict(r, w, input_buffer, output_buffer, alloc_u8, alloc_u32, alloc_hc, AllocU8::AllocatedMemory::default(), unexpected_eof_error_constant)
}
pub fn BrotliDecompressCustomIoCustomDict<ErrType,
                                InputType,
                                OutputType,
                                AllocU8: Allocator<u8>,
                                AllocU32: Allocator<u32>,
                                AllocHC: Allocator<HuffmanCode>>
  (r: &mut InputType,
   w: &mut OutputType,
   input_buffer: &mut [u8],
   output_buffer: &mut [u8],
   alloc_u8: AllocU8,
   alloc_u32: AllocU32,
   alloc_hc: AllocHC,
   custom_dictionary: AllocU8::AllocatedMemory,
   unexpected_eof_error_constant: ErrType)
   -> Result<(), ErrType>
  where InputType: CustomRead<ErrType>,
        OutputType: CustomWrite<ErrType>
{
  let mut brotli_state = BrotliState::new_with_custom_dictionary(alloc_u8, alloc_u32, alloc_hc, custom_dictionary);
  assert!(input_buffer.len() != 0);
  assert!(output_buffer.len() != 0);
  let mut available_out: usize = output_buffer.len();

  let mut available_in: usize = 0;
  let mut input_offset: usize = 0;
  let mut output_offset: usize = 0;
  let mut result: BrotliResult = BrotliResult::NeedsMoreInput;
  loop {
    match result {
      BrotliResult::NeedsMoreInput => {
        input_offset = 0;
        match r.read(input_buffer) {
          Err(e) => {
            return Err(e);
          },
          Ok(size) => {
            if size == 0 {
              return Err(unexpected_eof_error_constant);
            }
            available_in = size;
          }
        }
      }
      BrotliResult::NeedsMoreOutput => {
        let mut total_written: usize = 0;
        while total_written < output_offset {
          // this would be a call to write_all
          match w.write(&output_buffer[total_written..output_offset]) {
            Err(e) => {
              return Result::Err(e);
            },
            Ok(0) => {
              return Result::Err(unexpected_eof_error_constant);
            }
            Ok(cur_written) => {
              total_written += cur_written;
            }
          }
        }

        output_offset = 0;
      }
      BrotliResult::ResultSuccess => break,
      BrotliResult::ResultFailure => {
        return Err(unexpected_eof_error_constant);
      }
    }
    let mut written: usize = 0;
    result = BrotliDecompressStream(&mut available_in,
                                    &mut input_offset,
                                    input_buffer,
                                    &mut available_out,
                                    &mut output_offset,
                                    output_buffer,
                                    &mut written,
                                    &mut brotli_state);

    if output_offset != 0 {
      let mut total_written: usize = 0;
      while total_written < output_offset {
        match w.write(&output_buffer[total_written..output_offset]) {
          Err(e) => {
            return Result::Err(e);
          },
          // CustomResult::Transient(e) => continue,
          Ok(0) => {
            return Result::Err(unexpected_eof_error_constant);
          }
          Ok(cur_written) => {
            total_written += cur_written;
          }
        }
      }
      output_offset = 0;
      available_out = output_buffer.len()
    }
  }
  Ok(())
}


#[cfg(feature="std")]
pub fn copy_from_to<R: io::Read, W: io::Write>(mut r: R, mut w: W) -> io::Result<usize> {
  let mut buffer: [u8; 65536] = [0; 65536];
  let mut out_size: usize = 0;
  loop {
    match r.read(&mut buffer[..]) {
      Err(e) => {
        if let io::ErrorKind::Interrupted =  e.kind() {
          continue
        }
        return Err(e);
      }
      Ok(size) => {
        if size == 0 {
          break;
        } else {
          match w.write_all(&buffer[..size]) {
            Err(e) => {
              if let io::ErrorKind::Interrupted = e.kind() {
                continue
              }
              return Err(e);
            }
            Ok(_) => out_size += size,
          }
        }
      }
    }
  }
  Ok(out_size)
}

#[repr(C)]
pub struct BrotliDecoderReturnInfo {
    pub decoded_size: usize,
    pub error_string: [u8;256],
    pub result: BrotliResult,
    pub error_code: state::BrotliDecoderErrorCode,
}
impl BrotliDecoderReturnInfo {
    fn new<AllocU8: Allocator<u8>,
           AllocU32: Allocator<u32>,
           AllocHC: Allocator<HuffmanCode>>(
        state: &BrotliState<AllocU8, AllocU32, AllocHC>,
        result: BrotliResult,
        output_size: usize,
    ) -> Self {
        let mut ret = BrotliDecoderReturnInfo{
            result: result,
            decoded_size: output_size,
            error_code: decode::BrotliDecoderGetErrorCode(&state),  
            error_string: if let &Err(msg) = &state.mtf_or_error_string {
                msg
            } else {
                [0u8;256]
            },
        };
        if ret.error_string[0] == 0 {
            let error_string = state::BrotliDecoderErrorStr(ret.error_code);
            let to_copy = core::cmp::min(error_string.len(), ret.error_string.len() - 1);
            for (dst, src) in ret.error_string[..to_copy].iter_mut().zip(error_string[..to_copy].bytes()) {
                *dst = src;
            }
        }
        ret
    }
}

declare_stack_allocator_struct!(MemPool, 512, stack);

pub fn brotli_decode_prealloc(
  input: &[u8],
  mut output: &mut[u8],
  scratch_u8: &mut [u8],
  scratch_u32: &mut [u32],
  scratch_hc: &mut [HuffmanCode],
) -> BrotliDecoderReturnInfo {
  let u8_pool_size = scratch_u8.len();
  let stack_u8_allocator = MemPool::<u8>::new_allocator(scratch_u8, bzero);
  let stack_u32_allocator = MemPool::<u32>::new_allocator(scratch_u32, bzero);
  let stack_hc_allocator = MemPool::<HuffmanCode>::new_allocator(scratch_hc, bzero);
  let mut available_out = output.len();
  let mut available_in: usize = input.len();
  let mut input_offset: usize = 0;
  let mut output_offset: usize = 0;
  let mut written: usize = 0;
  let mut brotli_state =
    BrotliState::new(stack_u8_allocator, stack_u32_allocator, stack_hc_allocator);
  // MemPool never merges freed blocks, which the ring buffer sizing must know.
  brotli_state.u8_pool_size = u8_pool_size;
  let result = ::BrotliDecompressStream(&mut available_in,
                                      &mut input_offset,
                                      &input[..],
                                      &mut available_out,
                                      &mut output_offset,
                                      &mut output,
                                      &mut written,
                                      &mut brotli_state);
  let return_info = BrotliDecoderReturnInfo::new(&brotli_state, result.into(), output_offset);
  return_info    
}

#[cfg(not(feature="std"))]
pub fn brotli_decode(
    input: &[u8],
    output_and_scratch: &mut[u8],
) -> BrotliDecoderReturnInfo {
  let mut stack_u32_buffer = [0u32; 12 * 1024 * 6];
  let mut stack_hc_buffer = [HuffmanCode::default(); 128 * (decode::kNumInsertAndCopyCodes as usize + decode::kNumLiteralCodes as usize) + 6 * decode::kNumBlockLengthCodes as usize * huffman::INSERT_COPY_TABLE_WINDOW];
  let mut guessed_output_size = core::cmp::min(
    core::cmp::max(input.len(), // shouldn't shrink too much
                   output_and_scratch.len() / 3),
      output_and_scratch.len());
  if input.len() > 2 {
      let scratch_len = output_and_scratch.len() - guessed_output_size;
      if let Ok(lgwin) = decode::lg_window_size(input[0], input[1]) {
          let extra_window_size = 65536 + (decode::kNumLiteralCodes + decode::kNumInsertAndCopyCodes) as usize * 256 + (1usize << lgwin.0) * 5 / 4;
          if extra_window_size < scratch_len {
              guessed_output_size += (scratch_len - extra_window_size) * 3/4;
          }
      }
  }
  let (mut output, mut scratch_space) = output_and_scratch.split_at_mut(guessed_output_size);
  let u8_pool_size = scratch_space.len();
  let stack_u8_allocator = MemPool::<u8>::new_allocator(&mut scratch_space, bzero);
  let stack_u32_allocator = MemPool::<u32>::new_allocator(&mut stack_u32_buffer, bzero);
  let stack_hc_allocator = MemPool::<HuffmanCode>::new_allocator(&mut stack_hc_buffer, bzero);
  let mut available_out = output.len();
  let mut available_in: usize = input.len();
  let mut input_offset: usize = 0;
  let mut output_offset: usize = 0;
  let mut written: usize = 0;
  let mut brotli_state =
    BrotliState::new(stack_u8_allocator, stack_u32_allocator, stack_hc_allocator);
  // MemPool never merges freed blocks, which the ring buffer sizing must know.
  brotli_state.u8_pool_size = u8_pool_size;
  let result = ::BrotliDecompressStream(&mut available_in,
                                      &mut input_offset,
                                      &input[..],
                                      &mut available_out,
                                      &mut output_offset,
                                      &mut output,
                                      &mut written,
                                      &mut brotli_state);
  let return_info = BrotliDecoderReturnInfo::new(&brotli_state, result.into(), output_offset);
  return_info    
}

#[cfg(feature="std")]
pub fn brotli_decode(
    input: &[u8],
    output: &mut[u8],
) -> BrotliDecoderReturnInfo {
  brotli_decode_with_allocators(input, output,
                                StandardAlloc::default(),
                                StandardAlloc::default(),
                                StandardAlloc::default())
}

#[cfg(feature="std")]
pub(crate) fn brotli_decode_with_allocators<AllocU8: Allocator<u8>,
                                            AllocU32: Allocator<u32>,
                                            AllocHC: Allocator<HuffmanCode>>(
    input: &[u8],
    mut output: &mut[u8],
    alloc_u8: AllocU8,
    alloc_u32: AllocU32,
    alloc_hc: AllocHC,
) -> BrotliDecoderReturnInfo {
  let mut available_out = output.len();
  let mut available_in: usize = input.len();
  let mut input_offset: usize = 0;
  let mut output_offset: usize = 0;
  let mut written: usize = 0;
  let mut brotli_state = BrotliState::new(alloc_u8, alloc_u32, alloc_hc);
  // The output buffer bounds a successful decode, but may be much larger than
  // the stream. Use it as a staged ring-buffer growth hint instead of sizing
  // the first ring buffer from the full output capacity.
  brotli_state.one_shot_output_size =
    core::cmp::min(available_out, u32::MAX as usize) as u32;
  let result = ::BrotliDecompressStream(&mut available_in,
                                      &mut input_offset,
                                      &input[..],
                                      &mut available_out,
                                      &mut output_offset,
                                      &mut output,
                                      &mut written,
                                      &mut brotli_state);
  let return_info = BrotliDecoderReturnInfo::new(&brotli_state, result.into(), output_offset);
  return_info
}
