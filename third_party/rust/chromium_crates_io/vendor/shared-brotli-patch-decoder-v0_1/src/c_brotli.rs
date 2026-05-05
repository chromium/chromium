use crate::decode_error::DecodeError;
use crate::sys::{
    BrotliDecoderAttachDictionary, BrotliDecoderCreateInstance, BrotliDecoderDecompressStream,
    BrotliDecoderDestroyInstance, BrotliDecoderResult_BROTLI_DECODER_RESULT_ERROR,
    BrotliDecoderResult_BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT,
    BrotliDecoderResult_BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT,
    BrotliDecoderResult_BROTLI_DECODER_RESULT_SUCCESS, BrotliDecoderState,
    BrotliSharedDictionaryType_BROTLI_SHARED_DICTIONARY_RAW, BROTLI_FALSE,
};
use core::ptr;
use std::ptr::NonNull;

pub fn shared_brotli_decode_c(
    encoded: &[u8],
    shared_dictionary: Option<&[u8]>,
    max_uncompressed_length: usize,
) -> Result<Vec<u8>, DecodeError> {
    // Safety: Both `alloc_func` and `free_func` are `None` which is allowed. This
    // also makes the `opaque` (3rd argument) irrelevant.
    let decoder = NonNull::new(unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) })
        .ok_or(DecodeError::InitFailure)?;
    let res = decode(decoder, encoded, shared_dictionary, max_uncompressed_length);
    // Safety: decoder points to a valid decoder object.
    unsafe { BrotliDecoderDestroyInstance(decoder.as_ptr()) }
    res
}

#[inline(always)]
fn decode(
    decoder: NonNull<BrotliDecoderState>,
    encoded: &[u8],
    shared_dictionary: Option<&[u8]>,
    max_uncompressed_length: usize,
) -> Result<Vec<u8>, DecodeError> {
    if let Some(shared_dictionary) = shared_dictionary {
        // Safety: Decoder points to a valid `BrotliDecoderState` object. The shared
        // dictionary pointer and length are valid as they come from a Rust
        // slice.
        let attach_dictionary_succeeded = unsafe {
            BrotliDecoderAttachDictionary(
                decoder.as_ptr(),
                BrotliSharedDictionaryType_BROTLI_SHARED_DICTIONARY_RAW,
                shared_dictionary.len(),
                shared_dictionary.as_ptr(),
            )
        };
        if attach_dictionary_succeeded == BROTLI_FALSE {
            return Err(DecodeError::InvalidDictionary);
        }
    }

    let mut sink = vec![0u8; max_uncompressed_length];

    let mut next_in = encoded.as_ptr();
    let mut available_in = encoded.len();
    let mut next_out = sink.as_mut_ptr();
    let mut available_out = sink.len();
    let mut total_out = 0;

    loop {
        // Safety: All pointers are valid and non-null. `next_out` is a pointer with at
        // `available_in` bytes available within `sink`. This is managed by
        // `BrotliDecoderDecompressStream` itself and the variables are not mutated
        // outside of this context.
        let result = unsafe {
            BrotliDecoderDecompressStream(
                decoder.as_ptr(),
                &mut available_in,
                &mut next_in,
                &mut available_out,
                &mut next_out,
                &mut total_out,
            )
        };

        #[allow(non_upper_case_globals)]
        match result {
            BrotliDecoderResult_BROTLI_DECODER_RESULT_SUCCESS => break,
            BrotliDecoderResult_BROTLI_DECODER_RESULT_ERROR => {
                return Err(DecodeError::InvalidStream);
            }
            BrotliDecoderResult_BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT if available_in == 0 => {
                // Needs more input and none is available.
                return Err(DecodeError::InvalidStream);
            }
            BrotliDecoderResult_BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT if available_out == 0 => {
                // Needs more output space, but none is available.
                return Err(DecodeError::MaxSizeExceeded);
            }
            _ => continue,
        }
    }

    // There's is data left in the input stream, which is not allowed
    if available_in > 0 {
        return Err(DecodeError::ExcessInputData);
    }

    if total_out > sink.len() {
        return Err(DecodeError::MaxSizeExceeded);
    }

    sink.resize(total_out, 0);
    Ok(sink)
}
