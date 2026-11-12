use core;
#[cfg(feature="std")]
use std::io::{self, Error, ErrorKind, Write};
#[cfg(feature="std")]
pub use alloc_stdlib::StandardAlloc;
#[cfg(all(feature="unsafe",feature="std"))]
pub use alloc_stdlib::HeapAlloc;
pub use huffman::{HuffmanCode, HuffmanTreeGroup};
pub use state::BrotliState;
use state::BrotliDecoderParameter;
// use io_wrappers::write_all;
pub use io_wrappers::{CustomWrite};
#[cfg(feature="std")]
pub use io_wrappers::{IntoIoWriter, IoWriterWrapper};
pub use super::decode::{BrotliDecompressStream, BrotliResult};
pub use alloc::{AllocatedStackMemory, Allocator, SliceWrapper, SliceWrapperMut, StackAllocator};

#[cfg(feature="std")]
pub struct DecompressorWriterCustomAlloc<W: Write,
     BufferType : SliceWrapperMut<u8>,
     AllocU8 : Allocator<u8>,
     AllocU32 : Allocator<u32>,
     AllocHC : Allocator<HuffmanCode> >(DecompressorWriterCustomIo<io::Error,
                                                             IntoIoWriter<W>,
                                                             BufferType,
                                                             AllocU8, AllocU32, AllocHC>);


#[cfg(feature="std")]
impl<W: Write,
     BufferType : SliceWrapperMut<u8>,
     AllocU8,
     AllocU32,
     AllocHC> DecompressorWriterCustomAlloc<W, BufferType, AllocU8, AllocU32, AllocHC>
 where AllocU8 : Allocator<u8>, AllocU32 : Allocator<u32>, AllocHC : Allocator<HuffmanCode>
    {
    pub fn new(w: W, buffer : BufferType,
               alloc_u8 : AllocU8, alloc_u32 : AllocU32, alloc_hc : AllocHC) -> Self {
     let dict = AllocU8::AllocatedMemory::default();
     Self::new_with_custom_dictionary(w, buffer, alloc_u8, alloc_u32, alloc_hc, dict)

    }
    pub fn new_with_custom_dictionary(w: W, buffer : BufferType,
               alloc_u8 : AllocU8, alloc_u32 : AllocU32, alloc_hc : AllocHC, dict: AllocU8::AllocatedMemory) -> Self {
        DecompressorWriterCustomAlloc::<W, BufferType, AllocU8, AllocU32, AllocHC>(
          DecompressorWriterCustomIo::<Error,
                                 IntoIoWriter<W>,
                                 BufferType,
                                 AllocU8, AllocU32, AllocHC>::new_with_custom_dictionary(IntoIoWriter::<W>(w),
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

    pub fn get_ref(&self) -> &W {
        &self.0.get_ref().0
    }
    pub fn get_mut(&mut self) -> &mut W {
        &mut self.0.get_mut().0
    }
    pub fn into_inner(self) -> Result<W, W> {
        match self.0.into_inner() {
            Ok(w) => Ok(w.0),
            Err(w) => Err(w.0),
        }
    }
}
#[cfg(feature="std")]
impl<W: Write,
     BufferType : SliceWrapperMut<u8>,
     AllocU8 : Allocator<u8>,
     AllocU32 : Allocator<u32>,
     AllocHC : Allocator<HuffmanCode> > Write for DecompressorWriterCustomAlloc<W,
                                                                         BufferType,
                                                                         AllocU8,
                                                                         AllocU32,
                                                                         AllocHC> {
  	fn write(&mut self, buf: &[u8]) -> Result<usize, Error> {
       self.0.write(buf)
    }
  	fn flush(&mut self) -> Result<(), Error> {
       self.0.flush()
    }
}

#[cfg(feature="std")]
impl<W: Write,
     BufferType : SliceWrapperMut<u8>,
     AllocU8 : Allocator<u8>,
     AllocU32 : Allocator<u32>,
     AllocHC : Allocator<HuffmanCode> > DecompressorWriterCustomAlloc<W,
                                                                         BufferType,
                                                                         AllocU8,
                                                                         AllocU32,
                                                                         AllocHC> {
    pub fn close(&mut self) -> Result<(), Error>{
        self.0.close()
    }
}


#[cfg(not(any(feature="unsafe", not(feature="std"))))]
pub struct DecompressorWriter<W: Write>(DecompressorWriterCustomAlloc<W,
                                                         <StandardAlloc
                                                          as Allocator<u8>>::AllocatedMemory,
                                                         StandardAlloc,
                                                         StandardAlloc,
                                                         StandardAlloc>);


#[cfg(not(any(feature="unsafe", not(feature="std"))))]
impl<W: Write> DecompressorWriter<W> {
  pub fn new(w: W, buffer_size: usize) -> Self {
      Self::new_with_custom_dictionary(w, buffer_size, <StandardAlloc as Allocator<u8>>::AllocatedMemory::default())
  }
  pub fn new_with_custom_dictionary(w: W, buffer_size: usize, dict: <StandardAlloc as Allocator<u8>>::AllocatedMemory) -> Self {
    let mut alloc = StandardAlloc::default();
    let buffer = <StandardAlloc as Allocator<u8>>::alloc_cell(&mut alloc, if buffer_size == 0 {4096} else {buffer_size});
    DecompressorWriter::<W>(DecompressorWriterCustomAlloc::<W,
                                                <StandardAlloc
                                                 as Allocator<u8>>::AllocatedMemory,
                                                StandardAlloc,
                                                StandardAlloc,
                                                StandardAlloc>::new_with_custom_dictionary(w,
                                                                              buffer,
                                                                              alloc,
                                                                              StandardAlloc::default(),
                                                                              StandardAlloc::default(),
                                                                              dict))
  }

  // Attaches an additional raw LZ77 prefix dictionary; only allowed before
  // the first write. Returns false if the dictionary could not be attached.
  pub fn attach_dictionary(&mut self, dict: <StandardAlloc as Allocator<u8>>::AllocatedMemory) -> bool {
    self.0.attach_dictionary(dict)
  }

  // Attaches a serialized shared dictionary (magic 0x91 0x00, may contain an
  // LZ77 prefix dictionary and custom word/transform lists); only allowed
  // before the first write.
  pub fn attach_serialized_dictionary(&mut self, dict: <StandardAlloc as Allocator<u8>>::AllocatedMemory) -> bool {
    self.0.attach_serialized_dictionary(dict)
  }

  // Sets a decoder parameter (see BrotliState::set_parameter); only allowed
  // before the first write.
  pub fn set_parameter(&mut self, param: BrotliDecoderParameter, value: u32) -> bool {
    self.0.set_parameter(param, value)
  }

  // Sets the smallest ring buffer the decoder starts from (see
  // BrotliState::set_initial_ring_buffer_size); only allowed before the first
  // write.
  pub fn set_initial_ring_buffer_size(&mut self, size: u32) -> bool {
    self.0.set_initial_ring_buffer_size(size)
  }

  pub fn get_ref(&self) -> &W {
      self.0.get_ref()
  }
  pub fn get_mut(&mut self) -> &mut W {
      self.0.get_mut()
  }
  pub fn into_inner(self) -> Result<W, W> {
    self.0.into_inner()
  }
}


#[cfg(all(feature="unsafe", feature="std"))]
pub struct DecompressorWriter<W: Write>(DecompressorWriterCustomAlloc<W,
                                                         <HeapAlloc<u8>
                                                          as Allocator<u8>>::AllocatedMemory,
                                                         HeapAlloc<u8>,
                                                         HeapAlloc<u32>,
                                                         HeapAlloc<HuffmanCode> >);


#[cfg(all(feature="unsafe", feature="std"))]
impl<W: Write> DecompressorWriter<W> {
  pub fn new(w: W, buffer_size: usize) -> Self {
    let dict = <HeapAlloc<u8> as Allocator<u8>>::AllocatedMemory::default();
    Self::new_with_custom_dictionary(w, buffer_size, dict)
  }
  pub fn new_with_custom_dictionary(w: W, buffer_size: usize, dict: <HeapAlloc<u8> as Allocator<u8>>::AllocatedMemory) -> Self {
    let mut alloc_u8 = HeapAlloc::<u8>::new(0);
    let buffer = alloc_u8.alloc_cell(if buffer_size == 0 {4096} else {buffer_size});
    let alloc_u32 = HeapAlloc::<u32>::new(0);
    let alloc_hc = HeapAlloc::<HuffmanCode>::new(HuffmanCode{bits:2, value: 1});
    DecompressorWriter::<W>(DecompressorWriterCustomAlloc::<W,
                                                <HeapAlloc<u8>
                                                 as Allocator<u8>>::AllocatedMemory,
                                                HeapAlloc<u8>,
                                                HeapAlloc<u32>,
                                                HeapAlloc<HuffmanCode> >
      ::new_with_custom_dictionary(w, buffer, alloc_u8, alloc_u32, alloc_hc, dict))
  }

  // Attaches an additional raw LZ77 prefix dictionary; only allowed before
  // the first write. Returns false if the dictionary could not be attached.
  pub fn attach_dictionary(&mut self, dict: <HeapAlloc<u8> as Allocator<u8>>::AllocatedMemory) -> bool {
    self.0.attach_dictionary(dict)
  }

  // Attaches a serialized shared dictionary (magic 0x91 0x00, may contain an
  // LZ77 prefix dictionary and custom word/transform lists); only allowed
  // before the first write.
  pub fn attach_serialized_dictionary(&mut self, dict: <HeapAlloc<u8> as Allocator<u8>>::AllocatedMemory) -> bool {
    self.0.attach_serialized_dictionary(dict)
  }

  // Sets a decoder parameter (see BrotliState::set_parameter); only allowed
  // before the first write.
  pub fn set_parameter(&mut self, param: BrotliDecoderParameter, value: u32) -> bool {
    self.0.set_parameter(param, value)
  }

  // Sets the smallest ring buffer the decoder starts from (see
  // BrotliState::set_initial_ring_buffer_size); only allowed before the first
  // write.
  pub fn set_initial_ring_buffer_size(&mut self, size: u32) -> bool {
    self.0.set_initial_ring_buffer_size(size)
  }

  pub fn get_ref(&self) -> &W {
      self.0.get_ref()
  }
  pub fn get_mut(&mut self) -> &mut W {
    &mut (self.0).0.get_mut().0
  }
  pub fn into_inner(self) -> Result<W, W> {
    self.0.into_inner()
  }
}

#[cfg(feature="std")]
impl<W: Write> DecompressorWriter<W> {
    pub fn close(&mut self) -> Result<(), Error>{
        self.0.close()
    }
}
#[cfg(feature="std")]
impl<W: Write> Write for DecompressorWriter<W> {
  	fn write(&mut self, buf: &[u8]) -> Result<usize, Error> {
       self.0.write(buf)
    }
  	fn flush(&mut self) -> Result<(), Error> {
       self.0.flush()
    }
}

pub struct DecompressorWriterCustomIo<ErrType,
                                W: CustomWrite<ErrType>,
                                BufferType: SliceWrapperMut<u8>,
                                AllocU8: Allocator<u8>,
                                AllocU32: Allocator<u32>,
                                AllocHC: Allocator<HuffmanCode>>
{
  output_buffer: BufferType,
  total_out: usize,
  output: Option<W>,
  error_if_invalid_data: Option<ErrType>,
  state: BrotliState<AllocU8, AllocU32, AllocHC>,
}


pub fn write_all<ErrType, W: CustomWrite<ErrType>>(writer: &mut W, mut buf : &[u8]) -> Result<(), ErrType> {
    while buf.len() != 0 {
          match writer.write(buf) {
                Ok(bytes_written) => buf = &buf[bytes_written..],
                Err(e) => return Err(e),
          }
    }
    Ok(())
}


impl<ErrType,
     W: CustomWrite<ErrType>,
     BufferType : SliceWrapperMut<u8>,
     AllocU8,
     AllocU32,
     AllocHC> DecompressorWriterCustomIo<ErrType, W, BufferType, AllocU8, AllocU32, AllocHC>
 where AllocU8 : Allocator<u8>, AllocU32 : Allocator<u32>, AllocHC : Allocator<HuffmanCode>
{

    pub fn new(w: W, buffer : BufferType,
               alloc_u8 : AllocU8, alloc_u32 : AllocU32, alloc_hc : AllocHC,
               invalid_data_error_type : ErrType) -> Self {
           let dict = AllocU8::AllocatedMemory::default();
           Self::new_with_custom_dictionary(w, buffer, alloc_u8, alloc_u32, alloc_hc, dict, invalid_data_error_type)
    }
    pub fn new_with_custom_dictionary(w: W, buffer : BufferType,
               alloc_u8 : AllocU8, alloc_u32 : AllocU32, alloc_hc : AllocHC,
               dict: AllocU8::AllocatedMemory,
               invalid_data_error_type : ErrType) -> Self {
        DecompressorWriterCustomIo::<ErrType, W, BufferType, AllocU8, AllocU32, AllocHC>{
            output_buffer : buffer,
            total_out : 0,
            output: Some(w),
            state : BrotliState::new_with_custom_dictionary(alloc_u8,
                                                                 alloc_u32,
                                                                 alloc_hc,
                                                                 dict),
            error_if_invalid_data : Some(invalid_data_error_type),
        }
    }
    pub fn close(&mut self) -> Result<(), ErrType>{
        loop {
            let mut avail_in : usize = 0;
            let mut input_offset : usize = 0;
            let mut avail_out : usize = self.output_buffer.slice_mut().len();
            let mut output_offset : usize = 0;
            let ret = BrotliDecompressStream(
                &mut avail_in,
                &mut input_offset,
                &[],
                &mut avail_out,
                &mut output_offset,
                self.output_buffer.slice_mut(),                
                &mut self.total_out,
                &mut self.state);
          // already closed.
          if self.error_if_invalid_data.is_none() {
              return Ok(());
          }
          match write_all(self.output.as_mut().unwrap(), &self.output_buffer.slice_mut()[..output_offset]) {
            Ok(_) => {},
            Err(e) => return Err(e),
           }
           match ret {
           BrotliResult::NeedsMoreInput => return self.error_if_invalid_data.take().map(|e|Err(e)).unwrap_or(Ok(())),
           BrotliResult::NeedsMoreOutput => {},
           BrotliResult::ResultSuccess => {
               return Ok(());
           },
           BrotliResult::ResultFailure => return self.error_if_invalid_data.take().map(|e|Err(e)).unwrap_or(Ok(()))
           }
        }
    }

    // Attaches an additional raw LZ77 prefix dictionary; only allowed before
    // the first write. Returns false if the dictionary could not be attached.
    pub fn attach_dictionary(&mut self, dict: AllocU8::AllocatedMemory) -> bool {
      self.state.attach_dictionary(dict)
    }

    // Attaches a serialized shared dictionary (magic 0x91 0x00, may contain
    // an LZ77 prefix dictionary and custom word/transform lists); only
    // allowed before the first write.
    pub fn attach_serialized_dictionary(&mut self, dict: AllocU8::AllocatedMemory) -> bool {
      self.state.attach_serialized_dictionary(dict)
    }

    // Sets a decoder parameter (see BrotliState::set_parameter); only
    // allowed before the first write.
    pub fn set_parameter(&mut self, param: BrotliDecoderParameter, value: u32) -> bool {
      self.state.set_parameter(param, value)
    }

    // Sets the smallest ring buffer the decoder starts from (see
    // BrotliState::set_initial_ring_buffer_size); only allowed before the
    // first write.
    pub fn set_initial_ring_buffer_size(&mut self, size: u32) -> bool {
      self.state.set_initial_ring_buffer_size(size)
    }

    pub fn get_ref(&self) -> &W {
        self.output.as_ref().unwrap()
    }
    pub fn get_mut(&mut self) -> &mut W {
        self.output.as_mut().unwrap()
    }
    pub fn into_inner(mut self) -> Result<W, W> {
        match self.close() {
            Ok(_) => Ok((core::mem::replace(&mut self.output, None).unwrap())),
            Err(_) => Err((core::mem::replace(&mut self.output, None).unwrap())),
        }
    }
}

impl<ErrType,
     W: CustomWrite<ErrType>,
     BufferType : SliceWrapperMut<u8>,
     AllocU8 : Allocator<u8>,
     AllocU32 : Allocator<u32>,
     AllocHC : Allocator<HuffmanCode> > Drop for DecompressorWriterCustomIo<ErrType,
                                                                                     W,
                                                                                     BufferType,
                                                                                     AllocU8,
                                                                                     AllocU32,
                                                                                     AllocHC> {
    fn drop(&mut self) {
        if self.output.is_some() {
            match self.close() {
                Ok(_) => {},
                Err(_) => {},
            }
        }
    }
}

impl<ErrType,
     W: CustomWrite<ErrType>,
     BufferType : SliceWrapperMut<u8>,
     AllocU8 : Allocator<u8>,
     AllocU32 : Allocator<u32>,
     AllocHC : Allocator<HuffmanCode> > CustomWrite<ErrType> for DecompressorWriterCustomIo<ErrType,
                                                                                     W,
                                                                                     BufferType,
                                                                                     AllocU8,
                                                                                     AllocU32,
                                                                                     AllocHC> {
	fn write(&mut self, buf: &[u8]) -> Result<usize, ErrType > {
        let mut avail_in = buf.len();
        let mut input_offset : usize = 0;
        loop {
            let mut output_offset = 0;
            let mut avail_out = self.output_buffer.slice_mut().len();
            let op_result = BrotliDecompressStream(&mut avail_in,
                                     &mut input_offset,
                                     &buf[..],
                                     &mut avail_out,
                                     &mut output_offset,
                                     self.output_buffer.slice_mut(),
                                     &mut self.total_out,
                                     &mut self.state);
         match write_all(self.output.as_mut().unwrap(), &self.output_buffer.slice_mut()[..output_offset]) {
          Ok(_) => {},
          Err(e) => return Err(e),
         }
         match op_result {
          BrotliResult::NeedsMoreInput => assert_eq!(avail_in, 0),
          BrotliResult::NeedsMoreOutput => continue,
          BrotliResult::ResultSuccess => {
              return Ok(input_offset);
          }
          BrotliResult::ResultFailure => return self.error_if_invalid_data.take().map(|e|Err(e)).unwrap_or(Ok(0)),
       }
        if avail_in == 0 {
           break
        }
      }
      Ok(buf.len())
    }
    fn flush(&mut self) -> Result<(), ErrType> {
       self.output.as_mut().unwrap().flush()
    }
}

#[cfg(feature="std")]
#[cfg(test)]
mod test {
    use super::DecompressorWriter;
    use std::vec::Vec;
    use std::io::Write;
    // Brotli-compressed "hello\n" and 2 extra bytes


    #[test]
    fn write_extra() {
        let contents = b"\x8f\x02\x80\x68\x65\x6c\x6c\x6f\x0a\x03\x67\x6f\x6f\x64\x62\x79\x65\x0a";
        let mut decoder = DecompressorWriter::new(Vec::new(), 0);
        let n = decoder.write(contents).unwrap();
        assert_eq!(n, 10);
        // Ensure that we can continue to not send data to the writer
        // as it has consumed the entire file.
        let n = decoder.write(contents).unwrap();
        assert_eq!(n, 0);

        let mut decoder = DecompressorWriter::new(Vec::new(), 0);
        let e = decoder.write_all(contents).unwrap_err();
        assert!(e.kind() == std::io::ErrorKind::WriteZero);
        assert_eq!(decoder.get_ref().as_slice(), b"hello\n");
    }
}
