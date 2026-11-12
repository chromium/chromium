use ::BrotliResult;
#[allow(non_camel_case_types)]
#[repr(u8)]
pub enum c_void{
    _Nothing = 0,
}

// Defined with BrotliState::set_parameter, which the C API wraps.
pub use ::state::BrotliDecoderParameter;


#[repr(C)]
#[allow(dead_code)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum BrotliSharedDictionaryType {
    // Raw LZ77 prefix dictionary.
    BROTLI_SHARED_DICTIONARY_RAW = 0,
    // Serialized shared dictionary in the 0x91 0x00 shared-brotli container.
    BROTLI_SHARED_DICTIONARY_SERIALIZED = 1,
}

#[repr(C)]
pub enum BrotliDecoderResult {
    BROTLI_DECODER_RESULT_ERROR = 0,
    BROTLI_DECODER_RESULT_SUCCESS = 1,
    BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT = 2,
    BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT = 3,
}

pub type brotli_decoder_metadata_start_func = Option<extern "C" fn(opaque: *mut c_void, size: usize) -> ()>;
pub type brotli_decoder_metadata_chunk_func = Option<extern "C" fn(opaque: *mut c_void, data: *const u8, size: usize) -> ()>;

#[derive(Clone, Copy)]
pub(crate) struct CBrotliMetadataCallback {
    start: brotli_decoder_metadata_start_func,
    chunk: brotli_decoder_metadata_chunk_func,
    opaque: *mut c_void,
}

impl CBrotliMetadataCallback {
    pub(crate) fn new(
        start: brotli_decoder_metadata_start_func,
        chunk: brotli_decoder_metadata_chunk_func,
        opaque: *mut c_void,
    ) -> CBrotliMetadataCallback {
        CBrotliMetadataCallback {
            start, chunk, opaque
        }
    }
}

impl ::state::BrotliMetadataCallback for CBrotliMetadataCallback {
    fn start(&mut self, size: usize) {
        if let Some(callback) = self.start {
            callback(self.opaque, size);
        }
    }
    fn chunk(&mut self, data: &[u8]) {
        if let Some(callback) = self.chunk {
            callback(self.opaque, data.as_ptr(), data.len());
        }
    }
}


impl From<BrotliResult> for BrotliDecoderResult {
  fn from(r: BrotliResult) -> Self {
    match r {
      BrotliResult::ResultSuccess => BrotliDecoderResult::BROTLI_DECODER_RESULT_SUCCESS,
      BrotliResult::ResultFailure => BrotliDecoderResult::BROTLI_DECODER_RESULT_ERROR,
      BrotliResult::NeedsMoreInput => BrotliDecoderResult::BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT ,
      BrotliResult::NeedsMoreOutput => BrotliDecoderResult::BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT ,
    }
  }
}
pub type brotli_alloc_func = Option<extern "C" fn(data: *mut c_void, size: usize) -> *mut c_void>;

pub type brotli_free_func = Option<extern "C" fn(data: *mut c_void, ptr: *mut c_void) -> ()>;


#[repr(C)]
#[derive(Clone)]
pub struct CAllocator {
    pub alloc_func: brotli_alloc_func,
    pub free_func: brotli_free_func,
    pub opaque: *mut c_void,
}

unsafe impl Send for CAllocator {
}
