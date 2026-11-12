#![cfg(test)]
#![cfg(all(feature = "ffi-api", feature = "std"))]

use std::alloc::{alloc, dealloc, Layout};
use std::mem;
use std::ptr;
use std::vec::Vec;

use brotli_decompressor::ffi::interface::{c_void, BrotliDecoderResult};
use brotli_decompressor::ffi::{
  BrotliDecoderAttachDictionary, BrotliDecoderCreateInstance,
  BrotliDecoderDecompressStream, BrotliDecoderDestroyInstance,
  BrotliDecoderErrorCode, BrotliDecoderFreeU8, BrotliDecoderFreeUsize,
  BrotliDecoderIsUsed,
};
use brotli_decompressor::SliceWrapper;

struct FailingAllocator {
  allocation_calls: usize,
  fail_at: Option<usize>,
  allocations: Vec<(*mut u8, Layout)>,
}

impl FailingAllocator {
  fn new(fail_at: Option<usize>) -> Self {
    FailingAllocator {
      allocation_calls: 0,
      fail_at: fail_at,
      allocations: Vec::new(),
    }
  }

  fn fail_next(&mut self) {
    self.fail_at = Some(self.allocation_calls);
  }
}

extern "C" fn test_alloc(opaque: *mut c_void, size: usize) -> *mut c_void {
  let allocator = unsafe { &mut *(opaque as *mut FailingAllocator) };
  let call = allocator.allocation_calls;
  allocator.allocation_calls += 1;
  if allocator.fail_at == Some(call) {
    return ptr::null_mut();
  }
  let layout = Layout::from_size_align(size, 64).unwrap();
  let allocation = unsafe { alloc(layout) };
  if !allocation.is_null() {
    allocator.allocations.push((allocation, layout));
  }
  allocation as *mut c_void
}

extern "C" fn test_free(opaque: *mut c_void, allocation: *mut c_void) {
  if allocation.is_null() {
    return;
  }
  let allocator = unsafe { &mut *(opaque as *mut FailingAllocator) };
  let allocation = allocation as *mut u8;
  let index = allocator.allocations.iter()
    .position(|&(candidate, _)| candidate == allocation)
    .expect("free of unknown test allocation");
  let (_, layout) = allocator.allocations.swap_remove(index);
  unsafe { dealloc(allocation, layout) };
}

#[test]
fn create_instance_returns_null_when_custom_allocator_is_exhausted() {
  // Creating an instance allocates only the outer BrotliDecoderState; the
  // decoder's tables are allocated as decoding needs them.
  for fail_at in 0..1 {
    let mut allocator = FailingAllocator::new(Some(fail_at));
    let opaque = &mut allocator as *mut FailingAllocator as *mut c_void;
    let state = unsafe {
      BrotliDecoderCreateInstance(Some(test_alloc), Some(test_free), opaque)
    };

    assert!(state.is_null(), "allocation {} unexpectedly succeeded", fail_at);
    assert!(allocator.allocations.is_empty(),
            "allocation {} leaked memory", fail_at);
  }
}

// Upstream's raw attach is `dict->prefix[n] = data` and nothing else, so it
// cannot fail for want of memory. Attaching must therefore still succeed
// against an allocator that can no longer hand out a single byte.
#[test]
fn attach_raw_dictionary_needs_no_allocation() {
  let mut allocator = FailingAllocator::new(None);
  let opaque = &mut allocator as *mut FailingAllocator as *mut c_void;
  let state = unsafe {
    BrotliDecoderCreateInstance(Some(test_alloc), Some(test_free), opaque)
  };
  assert!(!state.is_null());
  let persistent_allocations = allocator.allocations.len();
  let allocation_calls = allocator.allocation_calls;

  allocator.fail_next();
  let dictionary = include_bytes!("../../testdata/issue42.dict");
  assert_eq!(unsafe {
    BrotliDecoderAttachDictionary(state, 0, dictionary.len(), dictionary.as_ptr())
  }, 1);
  assert_eq!(allocator.allocation_calls, allocation_calls,
             "raw attach must not call the allocator at all");
  assert_eq!(allocator.allocations.len(), persistent_allocations);

  unsafe { BrotliDecoderDestroyInstance(state) };
  assert!(allocator.allocations.is_empty());
}

// Ownership is not transferred: the decoder must end up pointing at the
// caller's buffer, exactly as upstream's `dict->prefix[n] = data` does for a
// raw dictionary and `&encoded[pos]` does for a serialized one.
#[test]
fn attach_dictionary_references_the_callers_buffer() {
  let raw = include_bytes!("../../testdata/issue42.dict");
  let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
  assert!(!state.is_null());
  assert_eq!(unsafe {
    BrotliDecoderAttachDictionary(state, 0, raw.len(), raw.as_ptr())
  }, 1);
  assert_eq!(unsafe {
    (*state).decompressor.compound_dictionary.chunks[0].slice().as_ptr()
  }, raw.as_ptr());
  unsafe { BrotliDecoderDestroyInstance(state) };

  let serialized = include_bytes!("../../testdata/shared_custom.dict");
  let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
  assert!(!state.is_null());
  assert_eq!(unsafe {
    BrotliDecoderAttachDictionary(state, 1, serialized.len(), serialized.as_ptr())
  }, 1);
  // The blob is referenced in place and the LZ77 prefix is a subslice of it.
  assert_eq!(unsafe { (*state).decompressor.dictionary.blob.slice().as_ptr() },
             serialized.as_ptr());
  let prefix = unsafe {
    (*state).decompressor.compound_dictionary.chunks[0].slice()
  };
  let base = serialized.as_ptr() as usize;
  let ptr = prefix.as_ptr() as usize;
  assert!(ptr > base && ptr + prefix.len() <= base + serialized.len());
  unsafe { BrotliDecoderDestroyInstance(state) };
}

#[test]
fn serialized_attach_frees_everything_at_each_allocation_failure() {
  // Borrowing leaves exactly one attach-time allocation for shared_custom: the
  // u32 metadata arena. The blob and its LZ77 prefix are referenced in place.
  // Before the allocator null check this class of failure aborted inside
  // slice::from_raw_parts_mut.
  let dictionary = include_bytes!("../../testdata/shared_custom.dict");
  for attach_allocation in 0..1 {
    let mut allocator = FailingAllocator::new(None);
    let opaque = &mut allocator as *mut FailingAllocator as *mut c_void;
    let state = unsafe {
      BrotliDecoderCreateInstance(Some(test_alloc), Some(test_free), opaque)
    };
    assert!(!state.is_null());
    let persistent_allocations = allocator.allocations.len();
    allocator.fail_at = Some(allocator.allocation_calls + attach_allocation);

    assert_eq!(unsafe {
      BrotliDecoderAttachDictionary(state, 1, dictionary.len(), dictionary.as_ptr())
    }, 0, "attach allocation {} unexpectedly succeeded", attach_allocation);
    assert_eq!(allocator.allocations.len(), persistent_allocations,
               "attach allocation {} leaked memory", attach_allocation);

    unsafe { BrotliDecoderDestroyInstance(state) };
    assert!(allocator.allocations.is_empty());
  }
}

#[test]
fn create_instance_rejects_mismatched_allocator_callbacks_without_allocating() {
  let mut allocator = FailingAllocator::new(None);
  let opaque = &mut allocator as *mut FailingAllocator as *mut c_void;
  let state = unsafe { BrotliDecoderCreateInstance(Some(test_alloc), None, opaque) };
  assert!(state.is_null());
  assert_eq!(allocator.allocation_calls, 0);
  assert!(allocator.allocations.is_empty());
}

fn ffi_attach_and_decode(dict_type: i32,
                         dictionary: &[u8],
                         compressed: &[u8],
                         expected: &[u8]) {
  let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
  assert!(!state.is_null());
  assert_eq!(unsafe {
    BrotliDecoderAttachDictionary(state, dict_type, dictionary.len(), dictionary.as_ptr())
  }, 1);

  let mut available_in = compressed.len();
  let mut input = compressed.as_ptr();
  let mut decoded = vec![0u8; expected.len() + 1];
  let mut available_out = decoded.len();
  let mut output = decoded.as_mut_ptr();
  let mut total_out = 0usize;
  let result = unsafe {
    BrotliDecoderDecompressStream(
      state,
      &mut available_in,
      &mut input,
      &mut available_out,
      &mut output,
      &mut total_out,
    )
  };
  assert_eq!(result as i32,
             BrotliDecoderResult::BROTLI_DECODER_RESULT_SUCCESS as i32);
  assert_eq!(available_in, 0);
  assert_eq!(total_out, expected.len());
  assert_eq!(&decoded[..total_out], expected);
  unsafe { BrotliDecoderDestroyInstance(state) };
}

#[test]
fn ffi_raw_dictionary_attach_decodes_reference_stream() {
  let dictionary = include_bytes!("../../testdata/issue42.dict");
  let compressed = include_bytes!("../../testdata/issue42.compressed");
  let mut expected = Vec::<u8>::new();
  for _ in 0..16 {
    expected.extend_from_slice(dictionary);
  }
  ffi_attach_and_decode(0, dictionary, compressed, &expected);
}

#[test]
fn ffi_serialized_dictionary_attach_decodes_reference_stream() {
  ffi_attach_and_decode(
      1,
      include_bytes!("../../testdata/shared_custom.dict"),
      include_bytes!("../../testdata/shared_custom.compressed"),
      include_bytes!("../../testdata/shared_content"));
}

#[test]
fn ffi_attach_validates_type_state_pointer_data_pointer_and_empty_input() {
  assert_eq!(unsafe {
    BrotliDecoderAttachDictionary(ptr::null_mut(), 0, 0, ptr::null())
  }, 0);

  let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
  assert!(!state.is_null());
  for invalid_type in [-1, 2, i32::max_value()].iter() {
    assert_eq!(unsafe {
      BrotliDecoderAttachDictionary(state, *invalid_type, 0, ptr::null())
    }, 0);
  }
  assert_eq!(unsafe {
    BrotliDecoderAttachDictionary(state, 0, 1, ptr::null())
  }, 0);
  // Empty raw dictionaries are successful no-ops and do not consume a chunk;
  // an empty serialized dictionary is malformed.
  assert_eq!(unsafe {
    BrotliDecoderAttachDictionary(state, 0, 0, ptr::null())
  }, 1);
  assert_eq!(unsafe {
    BrotliDecoderAttachDictionary(state, 1, 0, ptr::null())
  }, 0);
  unsafe { BrotliDecoderDestroyInstance(state) };
  unsafe { BrotliDecoderDestroyInstance(ptr::null_mut()) };
}

#[test]
fn ffi_attach_rejects_second_custom_dictionary_and_sixteenth_raw_chunk() {
  let serialized = include_bytes!("../../testdata/shared_custom.dict");
  let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
  assert!(!state.is_null());
  assert_eq!(unsafe {
    BrotliDecoderAttachDictionary(state, 1, serialized.len(), serialized.as_ptr())
  }, 1);
  assert_eq!(unsafe {
    BrotliDecoderAttachDictionary(state, 1, serialized.len(), serialized.as_ptr())
  }, 0);
  unsafe { BrotliDecoderDestroyInstance(state) };

  let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
  assert!(!state.is_null());
  let byte = [0x61u8];
  for chunk in 0..15 {
    assert_eq!(unsafe {
      BrotliDecoderAttachDictionary(state, 0, byte.len(), byte.as_ptr())
    }, 1, "raw chunk {} was rejected", chunk);
  }
  assert_eq!(unsafe {
    BrotliDecoderAttachDictionary(state, 0, byte.len(), byte.as_ptr())
  }, 0);
  unsafe { BrotliDecoderDestroyInstance(state) };
}

#[test]
fn ffi_attach_after_decoding_is_rejected() {
  static ENCODED_FF_BYTES: &'static [u8] = b"\x1f\x07\x00\xf8\x27\xfe\x43\x84\x00\x00";
  let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
  assert!(!state.is_null());
  let mut available_in = ENCODED_FF_BYTES.len();
  let mut input = ENCODED_FF_BYTES.as_ptr();
  let mut decoded = [0u8; 8];
  let mut available_out = decoded.len();
  let mut output = decoded.as_mut_ptr();
  let result = unsafe {
    BrotliDecoderDecompressStream(
      state,
      &mut available_in,
      &mut input,
      &mut available_out,
      &mut output,
      ptr::null_mut(),
    )
  };
  assert_eq!(result as i32,
             BrotliDecoderResult::BROTLI_DECODER_RESULT_SUCCESS as i32);
  let dictionary = [0x61u8];
  assert_eq!(unsafe {
    BrotliDecoderAttachDictionary(state, 0, dictionary.len(), dictionary.as_ptr())
  }, 0);
  unsafe { BrotliDecoderDestroyInstance(state) };
}

#[test]
fn default_allocator_free_ignores_null() {
  let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
  assert!(!state.is_null());

  unsafe {
    BrotliDecoderFreeU8(state, ptr::null_mut(), 64);
    BrotliDecoderFreeUsize(state, ptr::null_mut(), 64);
    BrotliDecoderDestroyInstance(state);
  }
}

#[test]
fn stream_rejects_null_input_buffer_with_nonzero_length() {
  let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
  assert!(!state.is_null());

  let mut available_in = 1usize;
  let mut input = ptr::null();
  let mut available_out = 0usize;
  let mut output = ptr::null_mut();
  let result = unsafe {
    BrotliDecoderDecompressStream(
      state,
      &mut available_in,
      &mut input,
      &mut available_out,
      &mut output,
      ptr::null_mut(),
    )
  };

  assert_eq!(
    result as i32,
    BrotliDecoderResult::BROTLI_DECODER_RESULT_ERROR as i32,
  );
  assert_eq!(
    unsafe { (*state).decompressor.error_code } as i32,
    BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS as i32,
  );
  unsafe { BrotliDecoderDestroyInstance(state) };
}

#[test]
fn stream_rejects_wrapping_input_range() {
  let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
  assert!(!state.is_null());

  let mut available_in = 1usize;
  let mut input = usize::MAX as *const u8;
  let mut available_out = 0usize;
  let mut output = ptr::null_mut();
  let result = unsafe {
    BrotliDecoderDecompressStream(
      state,
      &mut available_in,
      &mut input,
      &mut available_out,
      &mut output,
      ptr::null_mut(),
    )
  };

  assert_eq!(
    result as i32,
    BrotliDecoderResult::BROTLI_DECODER_RESULT_ERROR as i32,
  );
  assert_eq!(
    unsafe { (*state).decompressor.error_code } as i32,
    BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS as i32,
  );
  unsafe { BrotliDecoderDestroyInstance(state) };
}

#[test]
fn stream_advances_the_original_output_pointer() {
  static ENCODED_FF_BYTES: &'static [u8] = b"\x1f\x07\x00\xf8\x27\xfe\x43\x84\x00\x00";

  let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
  assert!(!state.is_null());

  let mut available_in = ENCODED_FF_BYTES.len();
  let mut input = ENCODED_FF_BYTES.as_ptr();
  let mut available_out = mem::size_of::<*mut u8>();
  let mut output = ptr::null_mut();
  let output_storage = &mut output as *mut *mut u8 as *mut u8;
  output = output_storage;

  // Decoding 0xff bytes overwrites `output` with usize::MAX. The cursor
  // must advance the original pointer instead of offsetting that value.
  let result = unsafe {
    BrotliDecoderDecompressStream(
      state,
      &mut available_in,
      &mut input,
      &mut available_out,
      &mut output,
      ptr::null_mut(),
    )
  };

  assert_ne!(
    result as i32,
    BrotliDecoderResult::BROTLI_DECODER_RESULT_ERROR as i32,
  );
  assert_eq!(available_out, 0);
  assert_eq!(
    output,
    output_storage.wrapping_add(mem::size_of::<*mut u8>()),
  );
  unsafe { BrotliDecoderDestroyInstance(state) };
}

#[test]
fn is_used_remains_true_after_byte_aligned_decode() {
  static ENCODED_FF_BYTES: &'static [u8] = b"\x1f\x07\x00\xf8\x27\xfe\x43\x84\x00\x00";

  let state = unsafe { BrotliDecoderCreateInstance(None, None, ptr::null_mut()) };
  assert!(!state.is_null());
  assert_eq!(unsafe { BrotliDecoderIsUsed(state) }, 0);

  let mut available_in = ENCODED_FF_BYTES.len();
  let mut input = ENCODED_FF_BYTES.as_ptr();
  let mut decoded = [0u8; 8];
  let mut available_out = decoded.len();
  let mut output = decoded.as_mut_ptr();
  let result = unsafe {
    BrotliDecoderDecompressStream(
      state,
      &mut available_in,
      &mut input,
      &mut available_out,
      &mut output,
      ptr::null_mut(),
    )
  };

  assert_eq!(
    result as i32,
    BrotliDecoderResult::BROTLI_DECODER_RESULT_SUCCESS as i32,
  );
  assert_eq!(decoded, [0xff; 8]);
  assert_eq!(unsafe { BrotliDecoderIsUsed(state) }, 1);
  unsafe { BrotliDecoderDestroyInstance(state) };
}
