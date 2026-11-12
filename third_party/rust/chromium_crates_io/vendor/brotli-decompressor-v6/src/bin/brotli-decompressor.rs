#![allow(unused_imports)]
// Assert at compile time that the default build contains no unsafe code:
// the only unsafe in this binary lives behind the "unsafe" and "seccomp"
// features, plus the ffi-api test module.
#![cfg_attr(not(any(feature="unsafe", feature="seccomp", all(test, feature="ffi-api"))), forbid(unsafe_code))]

mod integration_tests;
mod error_handling_tests;
mod ffi_stream_tests;
mod tests;
extern crate brotli_decompressor;
extern crate core;
#[macro_use]
extern crate alloc_no_stdlib;
use core::ops;
use brotli_decompressor::CustomRead;
pub struct Rebox<T> {
  b: Box<[T]>,
}
impl<T> From<Vec<T>> for Rebox<T> {
  #[inline(always)]
  fn from(val: Vec<T>) -> Self {
    Rebox::<T>{b:val.into_boxed_slice()}
  }
}
impl<T> core::default::Default for Rebox<T> {
  #[inline(always)]
  fn default() -> Self {
    let v: Vec<T> = Vec::new();
    let b = v.into_boxed_slice();
    Rebox::<T> { b: b }
  }
}

impl<T> ops::Index<usize> for Rebox<T> {
  type Output = T;
  #[inline(always)]
  fn index(&self, index: usize) -> &T {
    &(*self.b)[index]
  }
}

impl<T> ops::IndexMut<usize> for Rebox<T> {
  #[inline(always)]
  fn index_mut(&mut self, index: usize) -> &mut T {
    &mut (*self.b)[index]
  }
}

impl<T> alloc_no_stdlib::SliceWrapper<T> for Rebox<T> {
  #[inline(always)]
  fn slice(&self) -> &[T] {
    &*self.b
  }
}

impl<T> alloc_no_stdlib::SliceWrapperMut<T> for Rebox<T> {
  #[inline(always)]
  fn slice_mut(&mut self) -> &mut [T] {
    &mut *self.b
  }
}

pub struct HeapAllocator<T: core::clone::Clone> {
  pub default_value: T,
}

#[cfg(not(feature="unsafe"))]
impl<T: core::clone::Clone> alloc_no_stdlib::Allocator<T> for HeapAllocator<T> {
  type AllocatedMemory = Rebox<T>;
  fn alloc_cell(self: &mut HeapAllocator<T>, len: usize) -> Rebox<T> {
    let v: Vec<T> = vec![self.default_value.clone();len];
    let b = v.into_boxed_slice();
    Rebox::<T> { b: b }
  }
  fn free_cell(self: &mut HeapAllocator<T>, _data: Rebox<T>) {}
}

#[cfg(feature="unsafe")]
impl<T: core::clone::Clone> alloc_no_stdlib::Allocator<T> for HeapAllocator<T> {
  type AllocatedMemory = Rebox<T>;
  fn alloc_cell(self: &mut HeapAllocator<T>, len: usize) -> Rebox<T> {
    let mut v: Vec<T> = Vec::with_capacity(len);
    unsafe {
      v.set_len(len);
    }
    let b = v.into_boxed_slice();
    Rebox::<T> { b: b }
  }
  fn free_cell(self: &mut HeapAllocator<T>, _data: Rebox<T>) {}
}


#[allow(unused_imports)]
use alloc_no_stdlib::{SliceWrapper,SliceWrapperMut, StackAllocator, AllocatedStackMemory, Allocator, bzero};
use brotli_decompressor::HuffmanCode;

use std::io::{self, Error, ErrorKind, Read, Write};

use std::env;

use std::fs::File;

use std::path::Path;


// declare_stack_allocator_struct!(MemPool, 4096, global);



struct IoWriterWrapper<'a, OutputType: Write + 'a>(&'a mut OutputType);


struct IoReaderWrapper<'a, OutputType: Read + 'a>(&'a mut OutputType);

impl<'a, OutputType: Write> brotli_decompressor::CustomWrite<io::Error> for IoWriterWrapper<'a, OutputType> {
  fn flush(self: &mut Self) -> Result<(), io::Error> {
    loop {
      match self.0.flush() {
        Err(e) => {
          match e.kind() {
            ErrorKind::Interrupted => continue,
            _ => return Err(e),
          }
        }
        Ok(_) => return Ok(()),
      }
    }
  }

  fn write(self: &mut Self, buf: &[u8]) -> Result<usize, io::Error> {
    loop {
      match self.0.write(buf) {
        Err(e) => {
          match e.kind() {
            ErrorKind::Interrupted => continue,
            _ => return Err(e),
          }
        }
        Ok(cur_written) => return Ok(cur_written),
      }
    }
  }
}


impl<'a, InputType: Read> brotli_decompressor::CustomRead<io::Error> for IoReaderWrapper<'a, InputType> {
  fn read(self: &mut Self, buf: &mut [u8]) -> Result<usize, io::Error> {
    loop {
      match self.0.read(buf) {
        Err(e) => {
          match e.kind() {
            ErrorKind::Interrupted => continue,
            _ => return Err(e),
          }
        }
        Ok(cur_read) => return Ok(cur_read),
      }
    }
  }
}

struct IntoIoReader<OutputType: Read>(OutputType);

impl<InputType: Read> brotli_decompressor::CustomRead<io::Error> for IntoIoReader<InputType> {
  fn read(self: &mut Self, buf: &mut [u8]) -> Result<usize, io::Error> {
    loop {
      match self.0.read(buf) {
        Err(e) => {
          match e.kind() {
            ErrorKind::Interrupted => continue,
            _ => return Err(e),
          }
        }
        Ok(cur_read) => return Ok(cur_read),
      }
    }
  }
}
#[cfg(not(feature="seccomp"))]
pub fn decompress<InputType, OutputType>(r: &mut InputType,
                                         w: &mut OutputType,
                                         buffer_size: usize,
                                         dict: Vec<u8>)
                                         -> Result<(), io::Error>
  where InputType: Read,
        OutputType: Write
{
  let mut alloc_u8 = HeapAllocator::<u8> { default_value: 0 };
  let mut input_buffer = alloc_u8.alloc_cell(buffer_size);
  let mut output_buffer = alloc_u8.alloc_cell(buffer_size);
  brotli_decompressor::BrotliDecompressCustomIoCustomDict(&mut IoReaderWrapper::<InputType>(r),
                                          &mut IoWriterWrapper::<OutputType>(w),
                                          input_buffer.slice_mut(),
                                          output_buffer.slice_mut(),
                                          alloc_u8,
                                          HeapAllocator::<u32> { default_value: 0 },
                                          HeapAllocator::<HuffmanCode> {
                                            default_value: HuffmanCode::default(),
                                          },
                                          Rebox::<u8>::from(dict),
                                          Error::new(ErrorKind::UnexpectedEof, "Unexpected EOF"))
}
#[cfg(feature="seccomp")]
extern {
  fn calloc(n_elem : usize, el_size : usize) -> *mut u8;
  fn free(ptr : *mut u8);
  fn syscall(value : i32) -> i32;
  fn prctl(operation : i32, flags : u32) -> i32;
}
#[cfg(feature="seccomp")]
const PR_SET_SECCOMP : i32 = 22;
#[cfg(feature="seccomp")]
const SECCOMP_MODE_STRICT : u32 = 1;

#[cfg(feature="seccomp")]
declare_stack_allocator_struct!(CallocAllocatedFreelist, 8192, calloc);

#[cfg(feature="seccomp")]
pub fn decompress<InputType, OutputType>(r: &mut InputType,
                                         w: &mut OutputType,
                                         buffer_size: usize,
                                         dict: Vec<u8>)
                                         -> Result<(), io::Error>
  where InputType: Read,
        OutputType: Write
{

  // The u8 pool holds the ring buffer, which grows on demand up to the window
  // size. This pool never merges the blocks that growth frees, so reaching a
  // window can take twice the window from it. The extra 129 MiB covers the
  // ring buffers left behind on the way to the largest window that fitted
  // before growth on demand, 128 MiB (lgwin 27): each power of two from 1 KiB
  // to 64 MiB, plus 566 bytes of slack each, is 128 MiB + 8,598 bytes.
  let mut u8_buffer = unsafe {define_allocator_memory_pool!(4, u8, [0; 1024 * 1024 * (200 + 129)], calloc)};
  let mut u32_buffer = unsafe {define_allocator_memory_pool!(4, u32, [0; 16384], calloc)};
  let mut hc_buffer = unsafe {define_allocator_memory_pool!(4, HuffmanCode, [0; 1024 * 1024 * 16], calloc)};
  let mut alloc_u8 = CallocAllocatedFreelist::<u8>::new_allocator(u8_buffer.data, bzero);
  let alloc_u32 = CallocAllocatedFreelist::<u32>::new_allocator(u32_buffer.data, bzero);
  let alloc_hc = CallocAllocatedFreelist::<HuffmanCode>::new_allocator(hc_buffer.data, bzero);
  // Allocate the dictionary cell first, while the pool is one contiguous slice,
  // so the stack allocator splits it to exactly dict.len(): the custom dictionary
  // length is taken from the cell's slice length, so any trailing slack would
  // corrupt the dictionary.
  let mut custom_dict = alloc_u8.alloc_cell(dict.len());
  custom_dict.slice_mut().clone_from_slice(&dict[..]);
  let mut input_buffer = alloc_u8.alloc_cell(buffer_size);
  let mut output_buffer = alloc_u8.alloc_cell(buffer_size);
  let ret = unsafe{prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT)};
  if ret != 0 {
     panic!("Unable to activate seccomp");
  }
  // Reborrow w so it survives the call: the success path exits via a raw
  // syscall(60) that bypasses the writer flush a normal main() return would do,
  // so we must flush the output buffer ourselves before exiting.
  let result = brotli_decompressor::BrotliDecompressCustomIoCustomDict(&mut IoReaderWrapper::<InputType>(r),
                                          &mut IoWriterWrapper::<OutputType>(&mut *w),
                                          input_buffer.slice_mut(),
                                          output_buffer.slice_mut(),
                                          alloc_u8,
                                          alloc_u32,
                                          alloc_hc,
                                          custom_dict,
                                          Error::new(ErrorKind::UnexpectedEof, "Unexpected EOF"));
  match result {
      Err(e) => Err(e),
      Ok(()) => {
        w.flush()?;
        unsafe{syscall(60);};
        unreachable!()
      },
   }
}





// This decompressor is defined unconditionally on whether std is defined
// so we can exercise the code in any case
pub struct BrotliDecompressor<R: Read>(brotli_decompressor::DecompressorCustomIo<io::Error,
                                                                    IntoIoReader<R>,
                                                                    Rebox<u8>,
                                                                    HeapAllocator<u8>,
                                                                    HeapAllocator<u32>,
                                                                    HeapAllocator<HuffmanCode>>);



impl<R: Read> BrotliDecompressor<R> {
  pub fn new(r: R, buffer_size: usize) -> Self {
    let mut alloc_u8 = HeapAllocator::<u8> { default_value: 0 };
    let buffer = alloc_u8.alloc_cell(buffer_size);
    let alloc_u32 = HeapAllocator::<u32> { default_value: 0 };
    let alloc_hc = HeapAllocator::<HuffmanCode> { default_value: HuffmanCode::default() };
    BrotliDecompressor::<R>(
          brotli_decompressor::DecompressorCustomIo::<Error,
                                 IntoIoReader<R>,
                                 Rebox<u8>,
                                 HeapAllocator<u8>, HeapAllocator<u32>, HeapAllocator<HuffmanCode> >
                                 ::new(IntoIoReader::<R>(r),
                                                         buffer,
                                                         alloc_u8, alloc_u32, alloc_hc,
                                                         io::Error::new(ErrorKind::InvalidData,
                                                                        "Invalid Data")))
  }
}

impl<R: Read> Read for BrotliDecompressor<R> {
  fn read(&mut self, buf: &mut [u8]) -> Result<usize, Error> {
    self.0.read(buf)
  }
}

#[cfg(test)]
fn writeln0<OutputType: Write>(strm: &mut OutputType,
                               data: &str)
                               -> core::result::Result<(), io::Error> {
  writeln!(strm, "{:}", data)
}
#[cfg(test)]
fn writeln_time<OutputType: Write>(strm: &mut OutputType,
                                   data: &str,
                                   v0: u64,
                                   v1: u64,
                                   v2: u32)
                                   -> core::result::Result<(), io::Error> {
  writeln!(strm, "{:} {:} {:}.{:09}", v0, data, v1, v2)
}

// Decompresses with a serialized shared dictionary (and optionally a raw
// LZ77 prefix dictionary) attached.
#[cfg(not(feature="seccomp"))]
pub fn decompress_serialized_dict<InputType, OutputType>(r: &mut InputType,
                                                         w: &mut OutputType,
                                                         buffer_size: usize,
                                                         dict: Vec<u8>,
                                                         serialized_dict: Vec<u8>)
                                                         -> Result<(), io::Error>
  where InputType: Read,
        OutputType: Write
{
  let mut alloc_u8 = HeapAllocator::<u8> { default_value: 0 };
  let mut raw_dict_alloc = alloc_u8.alloc_cell(dict.len());
  raw_dict_alloc.slice_mut().clone_from_slice(&dict[..]);
  let mut serialized_alloc = alloc_u8.alloc_cell(serialized_dict.len());
  serialized_alloc.slice_mut().clone_from_slice(&serialized_dict[..]);
  let mut reader = brotli_decompressor::DecompressorCustomIo::new_with_custom_dictionary(
    IoReaderWrapper::<InputType>(r),
    alloc_u8.alloc_cell(buffer_size),
    alloc_u8,
    HeapAllocator::<u32> { default_value: 0 },
    HeapAllocator::<HuffmanCode> { default_value: HuffmanCode::default() },
    raw_dict_alloc,
    Error::new(ErrorKind::InvalidData, "Invalid Data"));
  if !reader.attach_serialized_dictionary(serialized_alloc) {
    return Err(Error::new(ErrorKind::InvalidData, "Invalid serialized dictionary"));
  }
  let mut buf = vec![0u8; buffer_size];
  loop {
    match brotli_decompressor::CustomRead::read(&mut reader, &mut buf[..]) {
      Err(e) => return Err(e),
      Ok(0) => return Ok(()),
      Ok(size) => w.write_all(&buf[..size])?,
    }
  }
}

fn main() {
  let mut dictionary = Vec::<u8>::new();
  let mut serialized_dictionary: Option<Vec<u8>> = None;
  let mut double_dash = false;
  let mut input: Option<File> = None;
  let mut output: Option<File> = None;
  for argument in env::args().skip(1) {
    if argument == "--" {
      double_dash = true;
      continue;
    }
    if argument.starts_with("-dict=") && !double_dash {
      let mut dict_file = File::open(&Path::new(&argument[6..])).unwrap();
      dict_file.read_to_end(&mut dictionary).unwrap();
      if dictionary.len() > 50331660 {
          panic!("Dictionary larger than 50331660");
      }
      continue;
    }
    if argument.starts_with("-serialized_dict=") && !double_dash {
      if serialized_dictionary.is_some() {
        panic!("Only one serialized dictionary may be attached");
      }
      let mut dict_file = File::open(&Path::new(&argument[17..])).unwrap();
      let mut serialized = Vec::<u8>::new();
      dict_file.read_to_end(&mut serialized).unwrap();
      serialized_dictionary = Some(serialized);
      continue;
    }
    if input.is_none() {
       input = Some(File::open(&Path::new(&argument)).unwrap());
    } else if output.is_none() {
       output = Some(File::create(&Path::new(&argument)).unwrap());
    } else {
       panic!("Cannot specify more than 2 filename args (input, output)")
    }
  }
  #[cfg(not(feature="seccomp"))]
  {
    if let Some(serialized_dictionary) = serialized_dictionary {
      match (input, output) {
        (None, _) => decompress_serialized_dict(&mut io::stdin(), &mut io::stdout(), 65536, dictionary, serialized_dictionary).unwrap(),
        (Some(mut i), None) => decompress_serialized_dict(&mut i, &mut io::stdout(), 65536, dictionary, serialized_dictionary).unwrap(),
        (Some(mut i), Some(mut o)) => decompress_serialized_dict(&mut i, &mut o, 65536, dictionary, serialized_dictionary).unwrap(),
      }
      return;
    }
  }
  #[cfg(feature="seccomp")]
  {
    if serialized_dictionary.is_some() {
      panic!("serialized dictionaries unsupported with seccomp");
    }
  }
  match (input, output) {
    (None, _) => decompress(&mut io::stdin(), &mut io::stdout(), 65536, dictionary).unwrap(),
    (Some(mut i), None) => decompress(&mut i, &mut io::stdout(), 65536, dictionary).unwrap(),
    (Some(mut i), Some(mut o)) => decompress(&mut i, &mut o, 65536, dictionary).unwrap(),
  }
}
