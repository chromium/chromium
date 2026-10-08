use crate::decode_error::DecodeError;
use brotli_decompressor::{BrotliDecompressStream, BrotliResult, BrotliState, StandardAlloc};

#[allow(dead_code)]
pub fn shared_brotli_decode_rust(
    encoded: &[u8],
    shared_dictionary: Option<&[u8]>,
    max_uncompressed_length: usize,
) -> Result<Vec<u8>, DecodeError> {
    if let Some(dict) = shared_dictionary {
        if dict.is_empty() {
            return Err(DecodeError::InvalidDictionary);
        }
    }

    let alloc_u8 = StandardAlloc::default();
    let alloc_u32 = StandardAlloc::default();
    let alloc_hc = StandardAlloc::default();

    let mut state = BrotliState::new(alloc_u8, alloc_u32, alloc_hc);
    if let Some(dict) = shared_dictionary {
        if !state.attach_dictionary(dict.to_vec().into()) {
            return Err(DecodeError::InvalidDictionary);
        }
    }

    let mut sink = vec![0u8; max_uncompressed_length];
    let mut available_in = encoded.len();
    let mut input_offset = 0;
    let mut available_out = sink.len();
    let mut output_offset = 0;
    let mut total_out = 0;

    loop {
        let result = BrotliDecompressStream(
            &mut available_in,
            &mut input_offset,
            encoded,
            &mut available_out,
            &mut output_offset,
            &mut sink,
            &mut total_out,
            &mut state,
        );

        match result {
            BrotliResult::ResultSuccess => break,
            BrotliResult::ResultFailure => {
                return Err(DecodeError::InvalidStream);
            }
            BrotliResult::NeedsMoreInput if available_in == 0 => {
                return Err(DecodeError::InvalidStream);
            }
            BrotliResult::NeedsMoreOutput if available_out == 0 => {
                return Err(DecodeError::MaxSizeExceeded);
            }
            _ => continue,
        }
    }

    if available_in > 0 {
        return Err(DecodeError::ExcessInputData);
    }

    if total_out > sink.len() {
        return Err(DecodeError::MaxSizeExceeded);
    }

    sink.resize(total_out, 0);
    Ok(sink)
}
