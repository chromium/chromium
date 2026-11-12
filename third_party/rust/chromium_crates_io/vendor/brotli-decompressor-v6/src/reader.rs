#[cfg(feature="std")]
use std::io::{self, Error, ErrorKind, Read};
#[cfg(feature="std")]
pub use alloc_stdlib::StandardAlloc;
#[cfg(all(feature="unsafe",feature="std"))]
pub use alloc_stdlib::HeapAlloc;
pub use huffman::{HuffmanCode, HuffmanTreeGroup};
pub use state::BrotliState;
use state::BrotliDecoderParameter;
// use io_wrappers::write_all;
pub use io_wrappers::{CustomRead, CustomWrite};
#[cfg(feature="std")]
pub use io_wrappers::{IntoIoReader, IoReaderWrapper, IoWriterWrapper};
pub use super::decode::{BrotliDecompressStream, BrotliResult};
pub use alloc::{AllocatedStackMemory, Allocator, SliceWrapper, SliceWrapperMut, StackAllocator};

#[cfg(feature="std")]
pub struct DecompressorCustomAlloc<R: Read,
     BufferType : SliceWrapperMut<u8>,
     AllocU8 : Allocator<u8>,
     AllocU32 : Allocator<u32>,
     AllocHC : Allocator<HuffmanCode> >(DecompressorCustomIo<io::Error,
                                                             IntoIoReader<R>,
                                                             BufferType,
                                                             AllocU8, AllocU32, AllocHC>);


#[cfg(feature="std")]
impl<R: Read,
     BufferType : SliceWrapperMut<u8>,
     AllocU8,
     AllocU32,
     AllocHC> DecompressorCustomAlloc<R, BufferType, AllocU8, AllocU32, AllocHC>
 where AllocU8 : Allocator<u8>, AllocU32 : Allocator<u32>, AllocHC : Allocator<HuffmanCode>
    {

    pub fn new(r: R, buffer : BufferType,
               alloc_u8 : AllocU8, alloc_u32 : AllocU32, alloc_hc : AllocHC) -> Self {
        DecompressorCustomAlloc::<R, BufferType, AllocU8, AllocU32, AllocHC>(
          DecompressorCustomIo::<Error,
                                 IntoIoReader<R>,
                                 BufferType,
                                 AllocU8, AllocU32, AllocHC>::new(IntoIoReader::<R>(r),
                                                                  buffer,
                                                                  alloc_u8, alloc_u32, alloc_hc,
                                                                  Error::new(ErrorKind::InvalidData,
                                                                             "Invalid Data")))
    }

    pub fn new_with_custom_dictionary(r: R, buffer : BufferType,
               alloc_u8 : AllocU8, alloc_u32 : AllocU32, alloc_hc : AllocHC,
               dict: AllocU8::AllocatedMemory) -> Self {
        DecompressorCustomAlloc::<R, BufferType, AllocU8, AllocU32, AllocHC>(
          DecompressorCustomIo::<Error,
                                 IntoIoReader<R>,
                                 BufferType,
                                 AllocU8, AllocU32, AllocHC>::new_with_custom_dictionary(IntoIoReader::<R>(r),
                                                                                   buffer,
                                                                                   alloc_u8, alloc_u32, alloc_hc,
                                                                                   dict,
                                                                                   Error::new(ErrorKind::InvalidData,
                                                                                              "Invalid Data")))
    }

    pub fn attach_dictionary(&mut self, dict: AllocU8::AllocatedMemory) -> bool {
      self.0.attach_dictionary(dict)
    }
    pub fn attach_serialized_dictionary(&mut self, dict: AllocU8::AllocatedMemory) -> bool {
      self.0.attach_serialized_dictionary(dict)
    }
    pub fn set_parameter(&mut self, param: BrotliDecoderParameter, value: u32) -> bool {
      self.0.set_parameter(param, value)
    }
    pub fn set_initial_ring_buffer_size(&mut self, size: u32) -> bool {
      self.0.set_initial_ring_buffer_size(size)
    }

    pub fn get_ref(&self) -> &R {
      &self.0.get_ref().0
    }
    pub fn get_mut(&mut self) -> &mut R {
      &mut self.0.get_mut().0
    }
    pub fn into_inner(self) -> R {
      self.0.into_inner().0
    }
}
#[cfg(feature="std")]
impl<R: Read,
     BufferType : SliceWrapperMut<u8>,
     AllocU8 : Allocator<u8>,
     AllocU32 : Allocator<u32>,
     AllocHC : Allocator<HuffmanCode> > Read for DecompressorCustomAlloc<R,
                                                                         BufferType,
                                                                         AllocU8,
                                                                         AllocU32,
                                                                         AllocHC> {
  	fn read(&mut self, buf: &mut [u8]) -> Result<usize, Error> {
       self.0.read(buf)
    }
}


#[cfg(not(any(feature="unsafe", not(feature="std"))))]
pub struct Decompressor<R: Read>(DecompressorCustomAlloc<R,
                                                         <StandardAlloc
                                                          as Allocator<u8>>::AllocatedMemory,
                                                         StandardAlloc,
                                                         StandardAlloc,
                                                         StandardAlloc>);


#[cfg(not(any(feature="unsafe", not(feature="std"))))]
impl<R: Read> Decompressor<R> {
  pub fn new(r: R, buffer_size: usize) -> Self {
     let dict = <StandardAlloc as Allocator<u8>>::AllocatedMemory::default();
     Self::new_with_custom_dict(r, buffer_size, dict)
  }
  pub fn new_with_custom_dict(r: R, buffer_size: usize, dict: <StandardAlloc as Allocator<u8>>::AllocatedMemory) -> Self {
    let mut alloc = StandardAlloc::default();
    let buffer = <StandardAlloc as Allocator<u8>>::alloc_cell(&mut alloc, if buffer_size == 0 {4096} else {buffer_size});
    Decompressor::<R>(DecompressorCustomAlloc::<R,
                                                <StandardAlloc
                                                 as Allocator<u8>>::AllocatedMemory,
                                                StandardAlloc,
                                                StandardAlloc,
                                                StandardAlloc>::new_with_custom_dictionary(r,
                                                                              buffer,
                                                                              alloc,
                                                                              StandardAlloc::default(),
                                                                              StandardAlloc::default(),
                                                                              dict))
  }

  // Attaches an additional raw LZ77 prefix dictionary; only allowed before
  // the first read. Returns false if the dictionary could not be attached.
  pub fn attach_dictionary(&mut self, dict: <StandardAlloc as Allocator<u8>>::AllocatedMemory) -> bool {
    self.0.attach_dictionary(dict)
  }

  // Attaches a serialized shared dictionary (magic 0x91 0x00, may contain an
  // LZ77 prefix dictionary and custom word/transform lists); only allowed
  // before the first read.
  pub fn attach_serialized_dictionary(&mut self, dict: <StandardAlloc as Allocator<u8>>::AllocatedMemory) -> bool {
    self.0.attach_serialized_dictionary(dict)
  }

  // Sets a decoder parameter (see BrotliState::set_parameter); only allowed
  // before the first read.
  pub fn set_parameter(&mut self, param: BrotliDecoderParameter, value: u32) -> bool {
    self.0.set_parameter(param, value)
  }

  // Sets the smallest ring buffer the decoder starts from (see
  // BrotliState::set_initial_ring_buffer_size); only allowed before the first
  // read.
  pub fn set_initial_ring_buffer_size(&mut self, size: u32) -> bool {
    self.0.set_initial_ring_buffer_size(size)
  }

  pub fn get_ref(&self) -> &R {
    &self.0.get_ref()
  }
  pub fn get_mut(&mut self) -> &mut R {
    &mut ((self.0).0).get_mut().0
  }
  pub fn into_inner(self) -> R {
    self.0.into_inner()
  }
}


#[cfg(all(feature="unsafe", feature="std"))]
pub struct Decompressor<R: Read>(DecompressorCustomAlloc<R,
                                                         <HeapAlloc<u8>
                                                          as Allocator<u8>>::AllocatedMemory,
                                                         HeapAlloc<u8>,
                                                         HeapAlloc<u32>,
                                                         HeapAlloc<HuffmanCode> >);


#[cfg(all(feature="unsafe", feature="std"))]
impl<R: Read> Decompressor<R> {
  pub fn new(r: R, buffer_size: usize) -> Self {
     let dict = <HeapAlloc<u8> as Allocator<u8>>::AllocatedMemory::default();
     Self::new_with_custom_dictionary(r, buffer_size, dict)
  }
  pub fn new_with_custom_dictionary(r: R, buffer_size: usize, dict: <HeapAlloc<u8>
                                                 as Allocator<u8>>::AllocatedMemory) -> Self {
    let mut alloc_u8 = HeapAlloc::<u8>::new(0);
    let buffer = alloc_u8.alloc_cell(if buffer_size == 0 {4096} else {buffer_size});
    let alloc_u32 = HeapAlloc::<u32>::new(0);
    let alloc_hc = HeapAlloc::<HuffmanCode>::new(HuffmanCode{
        bits:0, value: 0,
    });
    Decompressor::<R>(DecompressorCustomAlloc::<R,
                                                <HeapAlloc<u8>
                                                 as Allocator<u8>>::AllocatedMemory,
                                                HeapAlloc<u8>,
                                                HeapAlloc<u32>,
                                                HeapAlloc<HuffmanCode> >
      ::new_with_custom_dictionary(r, buffer, alloc_u8, alloc_u32, alloc_hc, dict))
  }

  // Attaches an additional raw LZ77 prefix dictionary; only allowed before
  // the first read. Returns false if the dictionary could not be attached.
  pub fn attach_dictionary(&mut self, dict: <HeapAlloc<u8> as Allocator<u8>>::AllocatedMemory) -> bool {
    self.0.attach_dictionary(dict)
  }

  // Attaches a serialized shared dictionary (magic 0x91 0x00, may contain an
  // LZ77 prefix dictionary and custom word/transform lists); only allowed
  // before the first read.
  pub fn attach_serialized_dictionary(&mut self, dict: <HeapAlloc<u8> as Allocator<u8>>::AllocatedMemory) -> bool {
    self.0.attach_serialized_dictionary(dict)
  }

  // Sets a decoder parameter (see BrotliState::set_parameter); only allowed
  // before the first read.
  pub fn set_parameter(&mut self, param: BrotliDecoderParameter, value: u32) -> bool {
    self.0.set_parameter(param, value)
  }

  // Sets the smallest ring buffer the decoder starts from (see
  // BrotliState::set_initial_ring_buffer_size); only allowed before the first
  // read.
  pub fn set_initial_ring_buffer_size(&mut self, size: u32) -> bool {
    self.0.set_initial_ring_buffer_size(size)
  }

  pub fn get_ref(&self) -> &R {
    self.0.get_ref()
  }
  pub fn get_mut(&mut self) -> &mut R {
    &mut (self.0).0.get_mut().0
  }
  pub fn into_inner(self) -> R {
    self.0.into_inner()
  }
}


#[cfg(feature="std")]
impl<R: Read> Read for Decompressor<R> {
  fn read(&mut self, buf: &mut [u8]) -> Result<usize, Error> {
    self.0.read(buf)
  }
}

pub struct DecompressorCustomIo<ErrType,
                                R: CustomRead<ErrType>,
                                BufferType: SliceWrapperMut<u8>,
                                AllocU8: Allocator<u8>,
                                AllocU32: Allocator<u32>,
                                AllocHC: Allocator<HuffmanCode>>
{
  input_buffer: BufferType,
  total_out: usize,
  input_offset: usize,
  input_len: usize,
  input: R,
  error_if_invalid_data: Option<ErrType>,
  state: BrotliState<AllocU8, AllocU32, AllocHC>,
  done: bool,
}

impl<ErrType,
     R: CustomRead<ErrType>,
     BufferType : SliceWrapperMut<u8>,
     AllocU8,
     AllocU32,
     AllocHC> DecompressorCustomIo<ErrType, R, BufferType, AllocU8, AllocU32, AllocHC>
 where AllocU8 : Allocator<u8>, AllocU32 : Allocator<u32>, AllocHC : Allocator<HuffmanCode>
{

    pub fn new(r: R, buffer : BufferType,
               alloc_u8 : AllocU8, alloc_u32 : AllocU32, alloc_hc : AllocHC,
               invalid_data_error_type : ErrType) -> Self {
     let dict = AllocU8::AllocatedMemory::default();
     Self::new_with_custom_dictionary(r, buffer, alloc_u8, alloc_u32, alloc_hc, dict, invalid_data_error_type)
    }
    pub fn new_with_custom_dictionary(r: R, buffer : BufferType,
               alloc_u8 : AllocU8, alloc_u32 : AllocU32, alloc_hc : AllocHC,
               dict: AllocU8::AllocatedMemory,
               invalid_data_error_type : ErrType) -> Self {
        DecompressorCustomIo::<ErrType, R, BufferType, AllocU8, AllocU32, AllocHC>{
            input_buffer : buffer,
            total_out : 0,
            input_offset : 0,
            input_len : 0,
            input: r,
            state : BrotliState::new_with_custom_dictionary(alloc_u8,
                                     alloc_u32,
                                     alloc_hc,
                                     dict),
            error_if_invalid_data : Some(invalid_data_error_type),
            done: false,
        }
    }

    // Attaches an additional raw LZ77 prefix dictionary; only allowed before
    // the first read. Returns false if the dictionary could not be attached.
    pub fn attach_dictionary(&mut self, dict: AllocU8::AllocatedMemory) -> bool {
      self.state.attach_dictionary(dict)
    }

    // Attaches a serialized shared dictionary (magic 0x91 0x00, may contain
    // an LZ77 prefix dictionary and custom word/transform lists); only
    // allowed before the first read.
    pub fn attach_serialized_dictionary(&mut self, dict: AllocU8::AllocatedMemory) -> bool {
      self.state.attach_serialized_dictionary(dict)
    }

    // Sets a decoder parameter (see BrotliState::set_parameter); only
    // allowed before the first read.
    pub fn set_parameter(&mut self, param: BrotliDecoderParameter, value: u32) -> bool {
      self.state.set_parameter(param, value)
    }

    // Sets the smallest ring buffer the decoder starts from (see
    // BrotliState::set_initial_ring_buffer_size); only allowed before the
    // first read.
    pub fn set_initial_ring_buffer_size(&mut self, size: u32) -> bool {
      self.state.set_initial_ring_buffer_size(size)
    }

    pub fn get_ref(&self) -> &R {
      &self.input
    }
    pub fn get_mut(&mut self) -> &mut R {
      &mut self.input
    }
    pub fn into_inner(self) -> R {
      match self {
        DecompressorCustomIo {
          input_buffer: _ib,
          total_out: _to,
          state: _state,
          input_offset: _io,
          input_len: _il,
          error_if_invalid_data:_eiid,
          input,
          done: _done,
        } =>{
          input
        }
    }
    }

    pub fn copy_to_front(&mut self) {
        let avail_in = self.input_len - self.input_offset;
        if self.input_offset == self.input_buffer.slice_mut().len() {
            self.input_offset = 0;
            self.input_len = 0;
        } else if self.input_offset + 256 > self.input_buffer.slice_mut().len() && avail_in < self.input_offset {
            let (first, second) = self.input_buffer.slice_mut().split_at_mut(self.input_offset);
            self.input_len -= self.input_offset;
            first[0..avail_in].clone_from_slice(&second[0..avail_in]);
            self.input_offset = 0;
        }
    }
}

impl<ErrType,
     R: CustomRead<ErrType>,
     BufferType : SliceWrapperMut<u8>,
     AllocU8 : Allocator<u8>,
     AllocU32 : Allocator<u32>,
     AllocHC : Allocator<HuffmanCode> > CustomRead<ErrType> for DecompressorCustomIo<ErrType,
                                                                                     R,
                                                                                     BufferType,
                                                                                     AllocU8,
                                                                                     AllocU32,
                                                                                     AllocHC> {
  /// This variant of read will return Ok(number of bytes read) until the file
  /// Is completed at which point it will return Ok(0).
  /// However if there are additional unconsumed bytes in the buffer, it will
  /// return Err(InvalidData) at that point. Otherwise it will keep returning
  /// Ok(0).
  ///
  /// # Arguments
  ///
  /// * `buf` - The buffer to read into
  ///
  /// # Errors
  ///
  /// Returns Ok(0) if the file has been fully decompressed.
  /// If the file has been fully decompressed but there are additional
  /// non-brotli bytes in the buffer, then return an InvalidData error.
  /// Also upstream errors from the reader are returned.
  fn read(&mut self, buf: &mut [u8]) -> Result<usize, ErrType > {
    let mut output_offset : usize = 0;
    let mut avail_out = buf.len() - output_offset;
    let mut avail_in = self.input_len - self.input_offset;
    while avail_out == buf.len() {
      match BrotliDecompressStream(&mut avail_in,
                                   &mut self.input_offset,
                                   &self.input_buffer.slice_mut()[..],
                                   &mut avail_out,
                                   &mut output_offset,
                                   buf,
                                   &mut self.total_out,
                                   &mut self.state) {
        BrotliResult::NeedsMoreInput => {
          self.copy_to_front();
          if output_offset != 0 {
            // The decompressor successfully decoded some bytes, but still requires more
            // we do not wish to risk self.input.read returning an error, so instead we
            // opt to return what we have and do not invoke the read trait method
            return Ok(output_offset);
          }
          match self.input.read(&mut self.input_buffer.slice_mut()[self.input_len..]) {
            Err(e) => {
              return Err(e);
            },
            Ok(size) => if size == 0 {
              return self.error_if_invalid_data.take().map(|e| Err(e)).unwrap_or(Ok(0));
            }else {
              self.input_len += size;
              avail_in = self.input_len - self.input_offset;
            },
          }
        },
        BrotliResult::NeedsMoreOutput => {
          break;
        },
        BrotliResult::ResultSuccess => {
            if output_offset == 0 {
                if !self.done {
                    self.done = true;
                } else if self.input_len != self.input_offset {
                    // Did not consume entire input; report error.
                    return self.error_if_invalid_data.take().map(|e| Err(e)).unwrap_or(Ok(output_offset));
                }
            }
            return Ok(output_offset);
        }
        BrotliResult::ResultFailure => return self.error_if_invalid_data.take().map(|e| Err(e)).unwrap_or(Ok(0)),
      }
    }
    Ok(output_offset)
  }
}

#[cfg(feature="std")]
#[test]
fn test_no_vanishing_bytes() {
    use std::string::ToString;

    // Output from this command:
    let compressed_with_extra = b"\x8f\x02\x80\x68\x65\x6c\x6c\x6f\x0a\x03\x67\x6f\x6f\x64\x62\x79\x65\x0a";
    // Make sure that read_to_string returns the data.
    let cursor = std::io::Cursor::new(compressed_with_extra);
    let mut reader = super::Decompressor::new(cursor, 8000);
    let mut out = std::string::String::new();
    std::io::Read::read_to_string(&mut reader, &mut out).unwrap();
    assert_eq!(out, "hello\n");

    // However you can call read extra times to make sure there's no data.
    let cursor = std::io::Cursor::new(compressed_with_extra);
    let mut reader = super::Decompressor::new(cursor, 8000);
    let mut data = std::vec::Vec::<u8>::default();
    loop {
        let mut buf = [0u8;5];
        let offset = reader.read(&mut buf).unwrap();
        if offset == 0 {
            break;
        }
        data.extend_from_slice(&buf[..offset]);
    }
    assert_eq!(
        &data,
        &['h' as u8, 'e' as u8, 'l' as u8, 'l' as u8, 'o' as u8, '\n' as u8]);

    // But calling read, one last time, results in an error because there
    // were leftover bytes in the buffer.
    let mut buf = [0u8;5];
    assert_eq!(reader.read(&mut buf).unwrap_err().kind(),
               io::ErrorKind::InvalidData);
    data.clear();


}

#[cfg(feature="std")]
#[test]
fn test_repeated_read_returns_zero() {
    use std::string::ToString;

    // Output from this command:
    let compressed_without_extra = b"\x8f\x02\x80\x68\x65\x6c\x6c\x6f\x0a\x03";
    // Make sure that read_to_string returns the data.
    let cursor = std::io::Cursor::new(compressed_without_extra);
    let mut reader = super::Decompressor::new(cursor, 8000);
    let mut out = std::string::String::new();
    std::io::Read::read_to_string(&mut reader, &mut out).unwrap();
    assert_eq!(out, "hello\n");

    // However you can call read extra times to make sure there's no data.
    let cursor = std::io::Cursor::new(compressed_without_extra);
    let mut reader = super::Decompressor::new(cursor, 8000);
    let mut data = std::vec::Vec::<u8>::default();
    loop {
        let mut buf = [0u8;5];
        let offset = reader.read(&mut buf).unwrap();
        if offset == 0 {
            break;
        }
        data.extend_from_slice(&buf[..offset]);
    }
    assert_eq!(&data, &['h' as u8, 'e' as u8, 'l' as u8, 'l' as u8, 'o' as u8, '\n' as u8]);
    let mut buf = [0u8;5];
    assert_eq!(reader.read(&mut buf).unwrap(), 0);
    data.clear();


}

