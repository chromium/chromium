#[cfg(feature="std")]
use std::{thread,panic, io, boxed, any, string};
#[cfg(feature="std")]
use std::io::Write;
use core;
use core::slice;
use core::ops;
pub mod interface;
pub mod alloc_util;
use self::alloc_util::SubclassableAllocator;
use alloc::{Allocator, SliceWrapper, SliceWrapperMut, StackAllocator, AllocatedStackMemory, bzero};
use self::interface::{CAllocator, c_void, BrotliDecoderParameter, BrotliDecoderResult, brotli_alloc_func, brotli_free_func, CBrotliMetadataCallback, brotli_decoder_metadata_start_func, brotli_decoder_metadata_chunk_func};
use ::BrotliResult;
use ::BrotliDecoderReturnInfo;
#[cfg(not(feature="std"))]
use ::brotli_decode;
pub use ::HuffmanCode;
pub use super::state::{BrotliDecoderErrorCode, BrotliState};

pub unsafe fn slice_from_raw_parts_or_nil<'a, T>(data: *const T, len: usize) -> &'a [T] {
    if len == 0 {
        return &[];
    }
    slice::from_raw_parts(data, len)
}

pub unsafe fn slice_from_raw_parts_or_nil_mut<'a, T>(data: *mut T, len: usize) -> &'a mut [T] {
    if len == 0 {
        return &mut [];
    }
    slice::from_raw_parts_mut(data, len)
}

trait MaxSliceLen {
    const MAX_SLICE_LEN: usize;
}

impl<T> MaxSliceLen for T {
    const MAX_SLICE_LEN: usize = if core::mem::size_of::<T>() == 0 {
        usize::MAX
    } else {
        (isize::MAX as usize) / core::mem::size_of::<T>()
    };
}

// Rejects the pointer/length pairs that would make `slice::from_raw_parts` trip a
// non-unwinding "unsafe precondition" panic not catchable by `catch_unwind`.
fn is_valid_slice_ptr<T>(data: *const T, len: usize) -> bool {
    if len == 0 {
        return true;
    }
    if data.is_null() {
        return false;
    }
    if (data as usize) % core::mem::align_of::<T>() != 0 {
        return false;
    }
    if len > T::MAX_SLICE_LEN {
        return false;
    }
    (data as usize).checked_add(len * core::mem::size_of::<T>()).is_some()
}

unsafe fn checked_slice_from_raw_parts_or_nil<'a, T>(
    data: *const T,
    len: usize,
) -> Option<&'a [T]> {
    if !is_valid_slice_ptr(data, len) {
        return None;
    }
    Some(slice_from_raw_parts_or_nil(data, len))
}

unsafe fn checked_slice_from_raw_parts_or_nil_mut<'a, T>(
    data: *mut T,
    len: usize,
) -> Option<&'a mut [T]> {
    if !is_valid_slice_ptr(data, len) {
        return None;
    }
    Some(slice_from_raw_parts_or_nil_mut(data, len))
}

#[cfg(feature="std")]
type BrotliAdditionalErrorData = boxed::Box<dyn any::Any + Send + 'static>;
#[cfg(not(feature="std"))]
type BrotliAdditionalErrorData = ();

#[repr(C)]
pub struct BrotliDecoderState {
    pub custom_allocator: CAllocator,
    pub decompressor: ::BrotliState<SubclassableAllocator,
                                    SubclassableAllocator,
                                    SubclassableAllocator>,
    metadata_cb: Option<CBrotliMetadataCallback>,
}

#[cfg(not(feature="std"))]
fn brotli_new_decompressor_without_custom_alloc(_to_box: BrotliDecoderState) -> *mut BrotliDecoderState{
    panic!("Must supply allocators if calling divans when compiled without features=std");
}

#[cfg(feature="std")]
#[inline(always)]
fn brotli_new_decompressor_without_custom_alloc(to_box: BrotliDecoderState) -> *mut BrotliDecoderState{
    // Box::new would abort if this fails.  Returning null drops to_box, which
    // releases the tables it already holds.  A Vec of exactly one element has
    // the layout of a Box of it, which Box::from_raw in
    // BrotliDecoderDestroyInstance frees.
    let mut state = std::vec::Vec::<BrotliDecoderState>::new();
    if state.try_reserve_exact(1).is_err() {
        return core::ptr::null_mut();
    }
    state.push(to_box);
    alloc_util::Box::into_raw(state.into_boxed_slice()) as *mut BrotliDecoderState
}


#[no_mangle]
pub unsafe extern fn BrotliDecoderCreateInstance(
    alloc_func: brotli_alloc_func,
    free_func: brotli_free_func,
    opaque: *mut c_void,
) -> *mut BrotliDecoderState {
    // The C API requires these callbacks to be supplied as a pair. Check it
    // before allocating anything, so an invalid pair reports failure the way
    // the rest of this API does rather than by unwinding across the C ABI.
    if alloc_func.is_some() != free_func.is_some() {
      return core::ptr::null_mut();
    }
    match catch_panic(|| {
      let allocators = CAllocator {
        alloc_func:alloc_func,
        free_func:free_func,
        opaque:opaque,
      };
      if let Some(alloc) = alloc_func {
        let ptr = alloc(allocators.opaque, core::mem::size_of::<BrotliDecoderState>());
        if ptr.is_null() {
            return core::ptr::null_mut();
        }
        let brotli_decoder_state_ptr = core::mem::transmute::<*mut c_void, *mut BrotliDecoderState>(ptr);
        core::ptr::write(brotli_decoder_state_ptr, BrotliDecoderState {
          custom_allocator: allocators.clone(),
          decompressor: ::BrotliState::new_strict(
            SubclassableAllocator::new(allocators.clone()),
            SubclassableAllocator::new(allocators.clone()),
            SubclassableAllocator::new(allocators),
          ),
          metadata_cb: None,
        });
        brotli_decoder_state_ptr
      } else {
        brotli_new_decompressor_without_custom_alloc(BrotliDecoderState {
          custom_allocator: allocators.clone(),
          decompressor: ::BrotliState::new_strict(
            SubclassableAllocator::new(allocators.clone()),
            SubclassableAllocator::new(allocators.clone()),
            SubclassableAllocator::new(allocators),
          ),
          metadata_cb: None,
        })
      }
    }) {
        Ok(ret) => ret,
        Err(mut e) => {
            error_print(core::ptr::null_mut(), &mut e);
            core::ptr::null_mut()
        },
    }
}

#[no_mangle]
pub unsafe extern "C" fn BrotliDecoderSetParameter(state_ptr: *mut BrotliDecoderState,
                                             selector: i32,
                                             value: u32) -> i32 {
  if state_ptr.is_null() {
    return 0;
  }
  // Unknown C enum values must return false, not create an invalid Rust enum
  // discriminant at the ABI boundary. Rust callers can cast the enum to i32.
  let param = match selector {
    x if x == BrotliDecoderParameter::BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION as i32 =>
      BrotliDecoderParameter::BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION,
    x if x == BrotliDecoderParameter::BROTLI_DECODER_PARAM_LARGE_WINDOW as i32 =>
      BrotliDecoderParameter::BROTLI_DECODER_PARAM_LARGE_WINDOW,
    _ => return 0,
  };
  if (*state_ptr).decompressor.set_parameter(param, value) {1} else {0}
}

#[no_mangle]
pub unsafe extern "C" fn BrotliDecoderSetMetadataCallbacks(
    state: *mut BrotliDecoderState,
    start_func: brotli_decoder_metadata_start_func,
    chunk_func: brotli_decoder_metadata_chunk_func,
    opaque: *mut c_void,
) {
    if let Some(state) = unsafe {state.as_mut()} {
        state.metadata_cb = Some(CBrotliMetadataCallback::new(
            start_func,
            chunk_func,
            opaque,
        ));
    }
}

#[no_mangle]
pub unsafe extern fn BrotliDecoderDecompressPrealloc(
  encoded_size: usize,
  encoded_buffer: *const u8,
  decoded_size: usize,
  decoded_buffer: *mut u8,
  scratch_u8_size: usize,
  scratch_u8_buffer: *mut u8,
  scratch_u32_size: usize,
  scratch_u32_buffer: *mut u32,
  scratch_hc_size: usize,
  scratch_hc_buffer: *mut HuffmanCode,
) -> BrotliDecoderReturnInfo {
  catch_panic_return_info(move || {
    let input = match checked_slice_from_raw_parts_or_nil(encoded_buffer, encoded_size) {
      Some(input) => input,
      None => return invalid_argument_return_info(),
    };
    let output = match checked_slice_from_raw_parts_or_nil_mut(decoded_buffer, decoded_size) {
      Some(output) => output,
      None => return invalid_argument_return_info(),
    };
    let scratch_u8 = match checked_slice_from_raw_parts_or_nil_mut(
      scratch_u8_buffer,
      scratch_u8_size,
    ) {
      Some(scratch_u8) => scratch_u8,
      None => return invalid_argument_return_info(),
    };
    let scratch_u32 = match checked_slice_from_raw_parts_or_nil_mut(
      scratch_u32_buffer,
      scratch_u32_size,
    ) {
      Some(scratch_u32) => scratch_u32,
      None => return invalid_argument_return_info(),
    };
    let scratch_hc = match checked_slice_from_raw_parts_or_nil_mut(
      scratch_hc_buffer,
      scratch_hc_size,
    ) {
      Some(scratch_hc) => scratch_hc,
      None => return invalid_argument_return_info(),
    };
    ::brotli_decode_prealloc(input, output, scratch_u8, scratch_u32, scratch_hc)
  })
}

unsafe fn brotli_decoder_decompress_with_return_info(
  encoded_size: usize,
  encoded_buffer: *const u8,
  decoded_size: usize,
  decoded_buffer: *mut u8,
) -> BrotliDecoderReturnInfo {
  let input = match checked_slice_from_raw_parts_or_nil(encoded_buffer, encoded_size) {
    Some(input) => input,
    None => return invalid_argument_return_info(),
  };
  let output_scratch = match checked_slice_from_raw_parts_or_nil_mut(
    decoded_buffer,
    decoded_size,
  ) {
    Some(output_scratch) => output_scratch,
    None => return invalid_argument_return_info(),
  };
  // Without C allocator callbacks, as in BrotliDecoderCreateInstance, so that
  // a failed allocation is a decoder error rather than an abort.  That
  // allocator zero-fills and then default-fills each buffer; the second pass
  // folds away when the optimizer can see the allocator, as under LTO (see
  // alloc_util::try_alloc_default_slice).
  #[cfg(feature="std")]
  {
    let allocator = || SubclassableAllocator::new(CAllocator {
      alloc_func: None,
      free_func: None,
      opaque: core::ptr::null_mut(),
    });
    ::brotli_decode_with_allocators(input, output_scratch, allocator(), allocator(), allocator())
  }
  #[cfg(not(feature="std"))]
  ::brotli_decode(input, output_scratch)
}

#[no_mangle]
pub unsafe extern fn BrotliDecoderDecompressWithReturnInfo(
  encoded_size: usize,
  encoded_buffer: *const u8,
  decoded_size: usize,
  decoded_buffer: *mut u8,
) -> BrotliDecoderReturnInfo {
  catch_panic_return_info(move || {
    brotli_decoder_decompress_with_return_info(
      encoded_size,
      encoded_buffer,
      decoded_size,
      decoded_buffer,
    )
  })
}

#[no_mangle]
pub unsafe extern fn BrotliDecoderDecompress(
  encoded_size: usize,
  encoded_buffer: *const u8,
  decoded_size: *mut usize,
  decoded_buffer: *mut u8,
) -> BrotliDecoderResult {
  if !is_valid_slice_ptr(decoded_size as *const usize, 1) {
    return BrotliDecoderResult::BROTLI_DECODER_RESULT_ERROR;
  }
  match catch_panic(move || {
    let res = brotli_decoder_decompress_with_return_info(
      encoded_size,
      encoded_buffer,
      *decoded_size,
      decoded_buffer,
    );
    *decoded_size = res.decoded_size;
    match res.result {
        BrotliResult::ResultSuccess => BrotliDecoderResult::BROTLI_DECODER_RESULT_SUCCESS,
        _ => BrotliDecoderResult::BROTLI_DECODER_RESULT_ERROR
    }
  }) {
      Ok(ret) => ret,
      Err(mut readable_err) => {
          error_print(core::ptr::null_mut(), &mut readable_err);
          *decoded_size = 0;
          BrotliDecoderResult::BROTLI_DECODER_RESULT_ERROR
      },
  }
}

#[cfg(all(feature="std", not(feature="pass-through-ffi-panics")))]
fn catch_panic<T, F>(f: F) -> thread::Result<T>
where F: FnOnce() -> T + panic::UnwindSafe {
    panic::catch_unwind(f)
}

fn copy_error_string(src: &[u8]) -> [u8;256] {
    let mut dst = [0u8;256];
    let xlen = core::cmp::min(src.len(), dst.len() - 1);
    dst.split_at_mut(xlen).0.clone_from_slice(src.split_at(xlen).0);
    dst
}

fn invalid_argument_return_info() -> BrotliDecoderReturnInfo {
    let error_code = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS;
    BrotliDecoderReturnInfo {
        decoded_size: 0,
        error_string: copy_error_string(::state::BrotliDecoderErrorStr(error_code).as_bytes()),
        error_code: error_code,
        result: BrotliResult::ResultFailure,
    }
}

#[cfg(all(feature="std", not(feature="pass-through-ffi-panics")))]
fn panic_return_info(err: &BrotliAdditionalErrorData) -> BrotliDecoderReturnInfo {
    let error_string = if let Some(st) = err.downcast_ref::<&str>() {
        copy_error_string(st.as_bytes())
    } else if let Some(st) = err.downcast_ref::<string::String>() {
        copy_error_string(st.as_bytes())
    } else {
        copy_error_string(
          ::state::BrotliDecoderErrorStr(
            BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE,
          ).as_bytes(),
        )
    };
    BrotliDecoderReturnInfo {
        decoded_size: 0,
        error_string: error_string,
        error_code: BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE,
        result: BrotliResult::ResultFailure,
    }
}

#[cfg(all(feature="std", not(feature="pass-through-ffi-panics")))]
fn catch_panic_return_info<F>(f: F) -> BrotliDecoderReturnInfo
where F: FnOnce() -> BrotliDecoderReturnInfo + panic::UnwindSafe {
    match catch_panic(f) {
        Ok(ret) => ret,
        Err(mut readable_err) => {
            let ret = panic_return_info(&readable_err);
            unsafe {
                error_print(core::ptr::null_mut(), &mut readable_err);
            }
            ret
        },
    }
}

#[cfg(all(feature="std", not(feature="pass-through-ffi-panics")))]
unsafe fn error_print(state_ptr: *mut BrotliDecoderState, err: &mut BrotliAdditionalErrorData) {
    if let Some(st) = err.downcast_ref::<&str>() {
        if !state_ptr.is_null() {
          (*state_ptr).decompressor.mtf_or_error_string = Err(copy_error_string(st.as_bytes()));
        }
        let _ign = writeln!(&mut io::stderr(), "panic: {}", st);
    } else {
        if let Some(st) = err.downcast_ref::<string::String>() {
          if !state_ptr.is_null() {
            (*state_ptr).decompressor.mtf_or_error_string = Err(copy_error_string(st.as_bytes()));
          }
          let _ign = writeln!(&mut io::stderr(), "Internal Error {:?}", st);
        } else {
            let _ign = writeln!(&mut io::stderr(), "Internal Error {:?}", err);
        }
    }
}

// can't catch panics in a reliable way without std:: configure with panic=abort. These shouldn't happen
#[cfg(any(not(feature="std"), feature="pass-through-ffi-panics"))]
fn catch_panic<T, F>(f: F) -> Result<T, BrotliAdditionalErrorData>
where F: FnOnce() -> T {
    Ok(f())
}

#[cfg(any(not(feature="std"), feature="pass-through-ffi-panics"))]
fn catch_panic_return_info<F>(f: F) -> BrotliDecoderReturnInfo
where F: FnOnce() -> BrotliDecoderReturnInfo {
    f()
}

#[cfg(any(not(feature="std"), feature="pass-through-ffi-panics"))]
fn error_print(_state_ptr: *mut BrotliDecoderState, _err: &mut BrotliAdditionalErrorData) {
}

#[no_mangle]
pub unsafe extern fn BrotliDecoderDecompressStream(
    state_ptr: *mut BrotliDecoderState,
    available_in: *mut usize,
    input_buf_ptr: *mut*const u8,
    available_out: *mut usize,
    output_buf_ptr: *mut*mut u8,
    mut total_out: *mut usize) -> BrotliDecoderResult {
    if state_ptr.is_null() ||
       available_in.is_null() ||
       input_buf_ptr.is_null() ||
       available_out.is_null() ||
       output_buf_ptr.is_null() {
        if !state_ptr.is_null() {
            (*state_ptr).decompressor.error_code =
                BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS;
        }
        return BrotliDecoderResult::BROTLI_DECODER_RESULT_ERROR;
    }
    match catch_panic(move || {
    let mut input_offset = 0usize;
    let mut output_offset = 0usize;
    let mut fallback_total_out = 0usize;
    if total_out.is_null() {
        total_out = &mut fallback_total_out;
    }
    let result: BrotliDecoderResult;
    let input_ptr = *input_buf_ptr;
    let output_ptr = *output_buf_ptr;
    {
        let input_buf = match checked_slice_from_raw_parts_or_nil(
            input_ptr,
            *available_in,
        ) {
            Some(input_buf) => input_buf,
            None => {
                (*state_ptr).decompressor.error_code =
                    BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS;
                return BrotliDecoderResult::BROTLI_DECODER_RESULT_ERROR;
            },
        };
        let output_buf = match checked_slice_from_raw_parts_or_nil_mut(
            output_ptr,
            *available_out,
        ) {
            Some(output_buf) => output_buf,
            None => {
                (*state_ptr).decompressor.error_code =
                    BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS;
                return BrotliDecoderResult::BROTLI_DECODER_RESULT_ERROR;
            },
        };
            result = super::decode::BrotliDecompressStreamWithDictionaryAndMetadata(
                &mut *available_in,
                &mut input_offset,
                input_buf,
                &mut *available_out,
                &mut output_offset,
                output_buf,
                &mut *total_out,
                &[], // In FFI, dictionaries are attached through BrotliDecoderAttachDicationary.
                     // This means only attach_external_dictionary is reachable through FFI, can
                     // can pass empty here.
                (*state_ptr).metadata_cb.as_mut().map(
                    |cb| cb as &mut dyn super::state::BrotliMetadataCallback
                ),
                &mut (*state_ptr).decompressor,
            ).into();
    }
    *input_buf_ptr = input_ptr.offset(input_offset as isize);
    *output_buf_ptr = output_ptr.offset(output_offset as isize);
                                           result
    }) {
        Ok(ret) => ret,
        Err(mut readable_err) => { // if we panic (completely unexpected) then we should report it back to C and print
            error_print(state_ptr, &mut readable_err);
            (*state_ptr).decompressor.error_code = BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE;
            BrotliDecoderResult::BROTLI_DECODER_RESULT_ERROR
        }
    }
}

/// Equivalent to BrotliDecoderDecompressStream but with no optional arg and no double indirect ptrs
#[no_mangle]
pub unsafe extern fn BrotliDecoderDecompressStreaming(
    state_ptr: *mut BrotliDecoderState,
    available_in: *mut usize,
    mut input_buf_ptr: *const u8,
    available_out: *mut usize,
    mut output_buf_ptr: *mut u8) -> BrotliDecoderResult {
    BrotliDecoderDecompressStream(state_ptr,
                                  available_in,
                                  &mut input_buf_ptr,
                                  available_out,
                                  &mut output_buf_ptr,
                                  core::ptr::null_mut())
}

#[cfg(feature="std")]
unsafe fn free_decompressor_no_custom_alloc(state_ptr: *mut BrotliDecoderState) {
    let _state = alloc_util::Box::from_raw(state_ptr);
}

#[cfg(not(feature="std"))]
unsafe fn free_decompressor_no_custom_alloc(_state_ptr: *mut BrotliDecoderState) {
    unreachable!();
}


#[no_mangle]
pub unsafe extern fn BrotliDecoderMallocU8(state_ptr: *mut BrotliDecoderState, size: usize) -> *mut u8 {
    if let Some(alloc_fn) = (*state_ptr).custom_allocator.alloc_func {
        return core::mem::transmute::<*mut c_void, *mut u8>(alloc_fn((*state_ptr).custom_allocator.opaque, size));
    } else {
        return alloc_util::alloc_stdlib(size);
    }
}

#[no_mangle]
pub unsafe extern fn BrotliDecoderFreeU8(state_ptr: *mut BrotliDecoderState, data: *mut u8, size: usize) {
    if let Some(free_fn) = (*state_ptr).custom_allocator.free_func {
        free_fn((*state_ptr).custom_allocator.opaque, core::mem::transmute::<*mut u8, *mut c_void>(data));
    } else {
        alloc_util::free_stdlib(data, size);
    }
}

#[no_mangle]
pub unsafe extern fn BrotliDecoderMallocUsize(state_ptr: *mut BrotliDecoderState, size: usize) -> *mut usize {
    if let Some(alloc_fn) = (*state_ptr).custom_allocator.alloc_func {
        let alloc_size = match size.checked_mul(core::mem::size_of::<usize>()) {
            Some(alloc_size) => alloc_size,
            None => return core::ptr::null_mut(),
        };
        return core::mem::transmute::<*mut c_void, *mut usize>(alloc_fn((*state_ptr).custom_allocator.opaque,
                                                                         alloc_size));
    } else {
        return alloc_util::alloc_stdlib(size);
    }
}
#[no_mangle]
pub unsafe extern fn BrotliDecoderFreeUsize(state_ptr: *mut BrotliDecoderState, data: *mut usize, size: usize) {
    if let Some(free_fn) = (*state_ptr).custom_allocator.free_func {
        free_fn((*state_ptr).custom_allocator.opaque, core::mem::transmute::<*mut usize, *mut c_void>(data));
    } else {
        alloc_util::free_stdlib(data, size);
    }
}

#[no_mangle]
pub unsafe extern fn BrotliDecoderDestroyInstance(state_ptr: *mut BrotliDecoderState) {
    if state_ptr.is_null() {
        return;
    }
    if (*state_ptr).custom_allocator.alloc_func.is_some() {
        // Capture the deallocator before `drop_in_place` runs: reading it back
        // through `state_ptr` afterwards would touch logically dropped memory.
        // Drop the state in place first so the child allocations are released
        // before the block holding the state itself.
        let free_fn = (*state_ptr).custom_allocator.free_func;
        let opaque = (*state_ptr).custom_allocator.opaque;
        core::ptr::drop_in_place(state_ptr);
        if let Some(free_fn) = free_fn {
            free_fn(opaque, state_ptr as *mut c_void);
        }
    } else {
        free_decompressor_no_custom_alloc(state_ptr);
    }
}

// Attaches a dictionary to the decoder, matching the C API of the same name.
// Must be called before any input is processed.
// Returns 1 on success, 0 on failure.
//
// Ownership is NOT transferred and the payload is never copied, exactly as in
// c/dec/decode.c: a raw dictionary is stored as a pointer to the caller's
// buffer (shared_dictionary.c stores `dict->prefix[n] = data`), and a
// serialized dictionary's LZ77 prefix, word lists and transform lists are all
// referenced in place inside the caller's blob (`&encoded[pos]`). The caller
// MUST therefore keep `data` allocated and unmodified until
// BrotliDecoderDestroyInstance returns.
//
// The consequence worth knowing: attaching costs zero allocations and zero
// bytes per decoder instance, so one dictionary image backs any number of
// concurrent decoders.
#[no_mangle]
pub unsafe extern "C" fn BrotliDecoderAttachDictionary(
    state_ptr: *mut BrotliDecoderState,
    dict_type: i32,
    data_size: usize,
    data: *const u8,
) -> i32 {
  if state_ptr.is_null() {
    return 0;
  }
  let is_serialized = match dict_type {
    0 => false,
    1 => true,
    _ => return 0,
  };
  // checked_slice_from_raw_parts_or_nil hands out an unconstrained lifetime.
  // Naming it 'static here is the whole of the unsafety behind this entry
  // point: it is sound exactly when the caller honors the contract documented
  // above. Confining it to this module is what keeps
  // BrotliState::attach_dictionary_borrowed a safe fn.
  let data_slice: &'static [u8] = match checked_slice_from_raw_parts_or_nil(data, data_size) {
    Some(data_slice) => data_slice,
    None => return 0,
  };
  match catch_panic(move || {
    match (*state_ptr).decompressor.state {
      super::state::BrotliRunningState::BROTLI_STATE_UNINITED => {},
      _ => return 0,
    }
    let ok = if is_serialized {
      (*state_ptr).decompressor.attach_serialized_dictionary_borrowed(data_slice)
    } else {
      (*state_ptr).decompressor.attach_dictionary_borrowed(data_slice)
    };
    if ok {1} else {0}
  }) {
    Ok(ret) => ret,
    Err(mut readable_err) => {
      error_print(state_ptr, &mut readable_err);
      0
    },
  }
}

#[no_mangle]
pub unsafe extern fn BrotliDecoderHasMoreOutput(state_ptr: *const BrotliDecoderState) -> i32 {
  if super::decode::BrotliDecoderHasMoreOutput(&(*state_ptr).decompressor) {1} else {0}
}

#[no_mangle]
pub unsafe extern fn BrotliDecoderTakeOutput(state_ptr: *mut BrotliDecoderState, size: *mut usize) -> *const u8 {
  let output = super::decode::BrotliDecoderTakeOutput(&mut (*state_ptr).decompressor, &mut *size);
  if output.is_empty() {
    core::ptr::null()
  } else {
    output.as_ptr()
  }
}



#[no_mangle]
pub unsafe extern fn BrotliDecoderIsUsed(state_ptr: *const BrotliDecoderState) -> i32 {
  if super::decode::BrotliDecoderIsUsed(&(*state_ptr).decompressor) {1} else {0}
}
#[no_mangle]
pub unsafe extern fn BrotliDecoderIsFinished(state_ptr: *const BrotliDecoderState) -> i32 {
  if super::decode::BrotliDecoderIsFinished(&(*state_ptr).decompressor) {1} else {0}
}
#[no_mangle]
pub unsafe extern fn BrotliDecoderGetErrorCode(state_ptr: *const BrotliDecoderState) -> BrotliDecoderErrorCode {
  super::decode::BrotliDecoderGetErrorCode(&(*state_ptr).decompressor)
}

#[no_mangle]
pub unsafe extern fn BrotliDecoderGetErrorString(state_ptr: *const BrotliDecoderState) -> *const u8 {
  if !state_ptr.is_null() {
    if let &Err(ref msg) = &(*state_ptr).decompressor.mtf_or_error_string {
      // important: this must be a ref
      // so stack memory is not returned
      return msg.as_ptr();
    }
  }
  BrotliDecoderErrorString(super::decode::BrotliDecoderGetErrorCode(&(*state_ptr).decompressor))
}
#[no_mangle]
pub extern fn BrotliDecoderErrorString(c: BrotliDecoderErrorCode) -> *const u8 {
    ::state::BrotliDecoderErrorStr(c).as_ptr()
}


#[no_mangle]
pub extern fn BrotliDecoderVersion() -> u32 {
  0x1000f00
}

#[cfg(test)]
mod tests {
  use super::*;

  fn assert_invalid_argument(ret: BrotliDecoderReturnInfo) {
    assert_eq!(ret.decoded_size, 0);
    assert_eq!(
      ret.error_code as i32,
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS as i32,
    );
    let expected = ::state::BrotliDecoderErrorStr(
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS,
    ).as_bytes();
    assert_eq!(&ret.error_string[..expected.len()], expected);
    match ret.result {
      BrotliResult::ResultFailure => {},
      _ => panic!("expected invalid arguments to return failure"),
    }
  }

  #[test]
  fn one_shot_rejects_null_input_buffer() {
    let ret = unsafe {
      BrotliDecoderDecompressWithReturnInfo(
        1,
        core::ptr::null(),
        0,
        core::ptr::null_mut(),
      )
    };

    assert_invalid_argument(ret);
  }

  #[test]
  fn prealloc_rejects_misaligned_scratch_buffer() {
    let mut scratch_u32 = [0u32; 2];
    let misaligned_scratch_u32 =
      unsafe { (scratch_u32.as_mut_ptr() as *mut u8).add(1) as *mut u32 };
    let ret = unsafe {
      BrotliDecoderDecompressPrealloc(
        0,
        core::ptr::null(),
        0,
        core::ptr::null_mut(),
        0,
        core::ptr::null_mut(),
        1,
        misaligned_scratch_u32,
        0,
        core::ptr::null_mut(),
      )
    };

    assert_invalid_argument(ret);
  }

  #[test]
  fn one_shot_rejects_null_decoded_size() {
    let ret = unsafe {
      BrotliDecoderDecompress(
        0,
        core::ptr::null(),
        core::ptr::null_mut(),
        core::ptr::null_mut(),
      )
    };

    assert_eq!(
      ret as i32,
      BrotliDecoderResult::BROTLI_DECODER_RESULT_ERROR as i32,
    );
  }

  // Needs unwinding: under panic=abort the panic ends the process instead.
  #[cfg(all(feature="std", not(feature="pass-through-ffi-panics"), panic="unwind"))]
  #[test]
  fn one_shot_panic_returns_error_info() {
    let ret = catch_panic_return_info(|| -> BrotliDecoderReturnInfo {
      panic!("ffi one-shot panic");
    });

    assert_eq!(ret.decoded_size, 0);
    assert_eq!(
      ret.error_code as i32,
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE as i32,
    );
    assert_eq!(&ret.error_string[..18], b"ffi one-shot panic");
    assert_eq!(ret.error_string[18], 0);
    match ret.result {
      BrotliResult::ResultFailure => {},
      _ => panic!("expected one-shot panic to return failure"),
    }
  }

  // Needs unwinding: the exhausted scratch allocator panics, and only an
  // unwinding build can turn that into an error return.
  #[cfg(all(feature="std", not(feature="pass-through-ffi-panics"), panic="unwind"))]
  #[test]
  fn prealloc_catches_scratch_exhaustion() {
    // Decoding needs tree groups and a ring buffer, which the empty scratch
    // buffers cannot provide.
    let input = include_bytes!("../../testdata/alice29.txt.compressed");
    let mut output = [0u8; 256];
    let ret = unsafe {
      BrotliDecoderDecompressPrealloc(
        input.len(),
        input.as_ptr(),
        output.len(),
        output.as_mut_ptr(),
        0,
        core::ptr::null_mut(),
        0,
        core::ptr::null_mut(),
        0,
        core::ptr::null_mut(),
      )
    };

    assert_eq!(ret.decoded_size, 0);
    assert_eq!(
      ret.error_code as i32,
      BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE as i32,
    );
    match ret.result {
      BrotliResult::ResultFailure => {},
      _ => panic!("expected scratch exhaustion to return failure"),
    }
  }

  #[cfg(feature="std")]
  #[test]
  fn set_parameter() {
    let set_parameter: unsafe extern "C" fn(
      *mut BrotliDecoderState,
      i32,
      u32,
    ) -> i32 = BrotliDecoderSetParameter;

    unsafe {
      let state = BrotliDecoderCreateInstance(None, None, core::ptr::null_mut());
      assert!(!state.is_null());
      assert!(!(*state).decompressor.large_window);
      assert!((*state).decompressor.canny_ringbuffer_allocation);

      assert_eq!(set_parameter(
        state,
        BrotliDecoderParameter::BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION as i32,
        1,
      ), 1);
      assert!(!(*state).decompressor.canny_ringbuffer_allocation);

      assert_eq!(set_parameter(
        state,
        BrotliDecoderParameter::BROTLI_DECODER_PARAM_LARGE_WINDOW as i32,
        1,
      ), 1);
      assert!((*state).decompressor.large_window);

      (*state).decompressor.state =
        super::super::state::BrotliRunningState::BROTLI_STATE_INITIALIZE;
      assert_eq!(set_parameter(
        state,
        BrotliDecoderParameter::BROTLI_DECODER_PARAM_LARGE_WINDOW as i32,
        0,
      ), 0);
      assert!((*state).decompressor.large_window);

      BrotliDecoderDestroyInstance(state);
    }
  }

  #[cfg(feature="std")]
  #[test]
  fn set_parameter_rejects_unknown_selectors_without_changing_state() {
    unsafe {
      let state = BrotliDecoderCreateInstance(None, None, core::ptr::null_mut());
      assert!(!state.is_null());
      for &selector in &[2, 42, -1, i32::MIN, i32::MAX] {
        for &value in &[0, 1, u32::MAX] {
          assert_eq!(BrotliDecoderSetParameter(state, selector, value), 0);
          assert!(!(*state).decompressor.large_window);
          assert!((*state).decompressor.canny_ringbuffer_allocation);
        }
      }
      BrotliDecoderDestroyInstance(state);
    }
  }

  #[test]
  fn set_parameter_rejects_null_state() {
    assert_eq!(unsafe {
      BrotliDecoderSetParameter(core::ptr::null_mut(), 0, 1)
    }, 0);
    assert_eq!(unsafe {
      BrotliDecoderSetParameter(core::ptr::null_mut(), -1, 1)
    }, 0);
  }

  #[cfg(feature="std")]
  #[test]
  fn take_output_returns_valid_partial_buffers_and_null_when_empty() {
    let input = [0x1b, 0x13, 0x00, 0x00, 0xa4, 0xb0, 0xb2, 0xea, 0x81, 0x47, 0x02, 0x8a];
    let expected = b"XXXXXXXXXXYYYYYYYYYY";
    unsafe {
      let state = BrotliDecoderCreateInstance(None, None, core::ptr::null_mut());
      assert!(!state.is_null());
      let mut size = 1usize;
      assert!(BrotliDecoderTakeOutput(state, &mut size).is_null());
      assert_eq!(size, 0);

      let mut available_in = input.len();
      let mut next_in = input.as_ptr();
      let mut available_out = 0usize;
      let mut next_out = core::ptr::null_mut();
      let mut total_out = 0usize;
      let result = BrotliDecoderDecompressStream(state, &mut available_in, &mut next_in,
                                                 &mut available_out, &mut next_out, &mut total_out);
      assert_eq!(result as i32, BrotliDecoderResult::BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT as i32);
      assert_eq!(total_out, 0);

      let mut consumed = 0usize;
      for &limit in &[1usize, 3, 7, 0] {
        let expected_ptr = (*state).decompressor.ringbuffer.slice().as_ptr().add(consumed);
        let mut size = limit;
        let output = BrotliDecoderTakeOutput(state, &mut size);
        assert_eq!(size, if limit == 0 { expected.len() - consumed } else { limit });
        // Check the pointer before dereferencing: the regression returned the
        // dangling pointer of an empty slice with a positive size.
        assert_eq!(output, expected_ptr);
        assert_eq!(slice::from_raw_parts(output, size), &expected[consumed..consumed + size]);
        consumed += size;
      }
      assert_eq!(consumed, expected.len());
      assert_eq!(BrotliDecoderHasMoreOutput(state), 0);
      for &limit in &[1usize, 0] {
        let mut size = limit;
        assert!(BrotliDecoderTakeOutput(state, &mut size).is_null());
        assert_eq!(size, 0);
      }
      BrotliDecoderDestroyInstance(state);
    }
  }
}
