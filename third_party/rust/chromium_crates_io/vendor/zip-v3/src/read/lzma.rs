use lzma_rs::decompress::{Options, Stream, UnpackedSize};
use std::collections::VecDeque;
use std::io::{BufRead, Error, ErrorKind, Read, Result, Write};

const OPTIONS: Options = Options {
    unpacked_size: UnpackedSize::ReadFromHeader,
    memlimit: None,
    allow_incomplete: true,
};

#[derive(Debug)]
pub struct LzmaDecoder<R> {
    compressed_reader: R,
    stream: Stream<VecDeque<u8>>,
}

impl<R: Read> LzmaDecoder<R> {
    pub fn new(inner: R) -> Self {
        LzmaDecoder {
            compressed_reader: inner,
            stream: Stream::new_with_options(&OPTIONS, VecDeque::new()),
        }
    }

    pub fn into_inner(self) -> R {
        self.compressed_reader
    }
}

impl<R: BufRead> Read for LzmaDecoder<R> {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize> {
        let mut bytes_read = self
            .stream
            .get_output_mut()
            .ok_or(Error::new(ErrorKind::InvalidData, "Invalid LZMA stream"))?
            .read(buf)?;
        while bytes_read < buf.len() {
            let compressed_bytes = self.compressed_reader.fill_buf()?;
            if compressed_bytes.is_empty() {
                break;
            }
            self.stream.write_all(compressed_bytes)?;
            bytes_read += self
                .stream
                .get_output_mut()
                .unwrap()
                .read(&mut buf[bytes_read..])?;
        }
        Ok(bytes_read)
    }
}
