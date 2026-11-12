#[cfg(feature="std")]
use std::io::{self, ErrorKind, Read, Write};

/// this trait does not allow for transient errors: they must be retried in the underlying layer
pub trait CustomWrite<ErrType> {
  fn write(self: &mut Self, data: &[u8]) -> Result<usize, ErrType>;
  fn flush(self: &mut Self) -> Result<(), ErrType>;
}
/// this trait does not allow for transient errors: they must be retried in the underlying layer
pub trait CustomRead<ErrType> {
  fn read(self: &mut Self, data: &mut [u8]) -> Result<usize, ErrType>;
}

#[allow(dead_code)] // prefer to replace 2 inlines in BrotliDecompressCustomIo once traits work
pub fn write_all<ErrType, OutputType>(w: &mut OutputType, buf: &[u8]) -> Result<(), ErrType>
  where OutputType: CustomWrite<ErrType>
{
  let mut total_written: usize = 0;
  while total_written < buf.len() {
    match w.write(&buf[total_written..]) {
      Err(e) => return Result::Err(e),
      // CustomResult::Transient(e) => continue,
      Ok(cur_written) => {
        assert_eq!(cur_written == 0, false); // not allowed by the contract
        total_written += cur_written;
      }
    }
  }
  Ok(())
}

#[cfg(feature="std")]
pub struct IntoIoReader<InputType: Read>(pub InputType);

#[cfg(feature="std")]
pub struct IntoIoWriter<InputType: Write>(pub InputType);


#[cfg(feature="std")]
pub struct IoWriterWrapper<'a, OutputType: Write + 'a>(pub &'a mut OutputType);


#[cfg(feature="std")]
pub struct IoReaderWrapper<'a, OutputType: Read + 'a>(pub &'a mut OutputType);

#[cfg(feature="std")]
impl<'a, OutputType: Write> CustomWrite<io::Error> for IoWriterWrapper<'a, OutputType> {
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
}


#[cfg(feature="std")]
impl<'a, InputType: Read> CustomRead<io::Error> for IoReaderWrapper<'a, InputType> {
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

#[cfg(feature="std")]
impl<InputType: Read> CustomRead<io::Error> for IntoIoReader<InputType> {
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

#[cfg(feature="std")]
impl<InputType: Write> CustomWrite<io::Error> for IntoIoWriter<InputType> {
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
