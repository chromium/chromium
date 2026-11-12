#![cfg(test)]
extern crate core;
use std::io::{self, Write, Read};
use core::cmp;


struct Buffer {
  data: Vec<u8>,
  read_offset: usize,
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
    self.data.extend(buf);
    return Ok(buf.len());
  }
  fn flush(self: &mut Self) -> io::Result<()> {
    return Ok(());
  }
}

fn copy_from_to<R: io::Read, W: io::Write>(mut r: R, mut w: W) -> io::Result<usize> {
  let mut buffer: [u8; 65536] = [0; 65536];
  let mut out_size: usize = 0;
  loop {
    match r.read(&mut buffer[..]) {
      Err(e) => {
        match e.kind() {
          io::ErrorKind::Interrupted => continue,
          _ => {}
        }
        return Err(e);
      }
      Ok(size) => {
        if size == 0 {
          break;
        } else {
          match w.write_all(&buffer[..size]) {
            Err(e) => {
              match e.kind() {
                io::ErrorKind::Interrupted => continue,
                _ => {}
              }
              return Err(e);
            }
            Ok(_) => out_size += size,
          }
        }
      }
    }
  }
  return Ok(out_size);
}


fn ok_one_byte_brotli(b: u8) -> bool{
    b == 6 || b == 26 || b== 51 ||
        b == 53 || b == 55 || b == 57 ||
        b == 59 || b == 61 || b == 63
}

#[test]
fn test_one_byte_copier() {
    for b in 0..256 {
        let in_buf = [b as u8];
        let mut output = Buffer::new(&[]);
        let mut input = super::BrotliDecompressor::new(Buffer::new(&in_buf), 4096);
        match copy_from_to(&mut input, &mut output) {
            Ok(_) => if ok_one_byte_brotli(in_buf[0]) {
                assert_eq!(output.data, &[])
            } else {
                panic!("Expected error not {}", b)
            },
            Err(e) => assert_eq!(e.kind(), io::ErrorKind::InvalidData),
        }
    }
}

#[cfg(feature="std")]
#[test]
fn test_one_byte_writer() {
    for b in 0..256 {
        let in_buf = [b as u8];
        let mut output = Buffer::new(&[]);
        let mut writer = super::brotli_decompressor::DecompressorWriter::new(&mut output, 4096);
        match writer.write(&in_buf) {
            Ok(v) => {
                if ok_one_byte_brotli(b as u8) {
                    writer.close().unwrap();
                } else {
                    assert_eq!(writer.close().unwrap_err().kind(), io::ErrorKind::InvalidData);
                }
                assert_eq!(v, 1);
            },
            Err(e) => {
                assert_eq!(e.kind(), io::ErrorKind::InvalidData);
                assert!(!ok_one_byte_brotli(b as u8));
            }
        }
    }
}


#[cfg(feature="std")]
#[test]
fn test_error_byte_writer() {
    let in_buf = b"\x8f\x02\x80\x68\x65\x6c\x6c\x6f\x0a\x03\x67\x6f\x6f\x64\x62\x79\x65\x0a";
    let mut output = Buffer::new(&[]);
    let mut writer = super::brotli_decompressor::DecompressorWriter::new(&mut output, 4096);
    match writer.write_all(&in_buf[..]) {
        Ok(_) => panic!("trailing bytes after stream end should not be accepted"),
        Err(e) => assert_eq!(e.kind(), io::ErrorKind::WriteZero),
    }
}

#[cfg(feature="std")]
#[test]
fn test_one_byte_reader() {
    for b in 0..256 {
        let in_buf = [b as u8];
        let mut output = [0u8;1];
        let mut reader = super::brotli_decompressor::Decompressor::new(&in_buf[..], 4096);
        match reader.read(&mut output) {
            Ok(v) => {
                assert!(ok_one_byte_brotli(b as u8));
                assert_eq!(v, 0);
            },
            Err(e) => {
                assert_eq!(e.kind(), io::ErrorKind::InvalidData);
                assert!(!ok_one_byte_brotli(b as u8));
            }
        }
    }
}

#[test]
fn test_10x_10y() {
  let in_buf: [u8; 12] = [0x1b, 0x13, 0x00, 0x00, 0xa4, 0xb0, 0xb2, 0xea, 0x81, 0x47, 0x02, 0x8a];

  let mut output = Buffer::new(&[]);
  let mut input = super::BrotliDecompressor::new(Buffer::new(&in_buf), 4096);
  match copy_from_to(&mut input, &mut output) {
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
}


#[test]
fn test_alice() {
  let in_buf = include_bytes!("../../testdata/alice29.txt.compressed");

  let mut output = Buffer::new(&[]);
  let mut input = super::BrotliDecompressor::new(Buffer::new(in_buf), 1);
  match copy_from_to(&mut input, &mut output) {
    Ok(_) => {}
    Err(e) => panic!("Error {:?}", e),
  }
  let mut i: usize = 0;
  let truth = include_bytes!("../../testdata/alice29.txt");
  while i < truth.len() {
    assert_eq!(output.data[i], truth[i]);
    i += 1;
  }
  assert_eq!(truth.len(), output.data.len());
}
