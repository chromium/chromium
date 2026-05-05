use std::io::{self, Cursor, Write};

use crate::decode_error::DecodeError;
use brotli_decompressor::BrotliDecompressCustomDict;

#[allow(dead_code)]
pub fn shared_brotli_decode_rust(
    encoded: &[u8],
    shared_dictionary: Option<&[u8]>,
    max_uncompressed_length: usize,
) -> Result<Vec<u8>, DecodeError> {
    let mut input_buffer: [u8; 4096] = [0; 4096];
    let mut output_buffer: [u8; 4096] = [0; 4096];

    let mut cursor = Cursor::new(encoded);
    let mut output = BoundedOutput(Default::default(), max_uncompressed_length);

    let mut dict: Vec<u8> = Default::default();
    if let Some(dict_data) = shared_dictionary {
        dict.extend_from_slice(dict_data);
    }

    BrotliDecompressCustomDict(
        &mut cursor,
        &mut output,
        &mut input_buffer,
        &mut output_buffer,
        dict,
    )
    .map_err(DecodeError::from_io_error)?;

    if cursor.get_ref().len() as u64 > cursor.position() {
        return Err(DecodeError::ExcessInputData);
    }

    Ok(output.0)
}

#[allow(dead_code)]
struct BoundedOutput(Vec<u8>, usize);

#[allow(dead_code)]
impl Write for BoundedOutput {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        if self.1 < buf.len() {
            // hit the write bound, return an error.
            return Err(io::Error::new(io::ErrorKind::OutOfMemory, "Max output size reached."));
        }
        self.1 -= buf.len();
        self.0.write(buf)
    }

    fn flush(&mut self) -> std::io::Result<()> {
        self.0.flush()
    }
}
