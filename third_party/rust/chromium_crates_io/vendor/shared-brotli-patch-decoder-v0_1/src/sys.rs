// Note: These manual bindings and the `cc` build dependency are a temporary
// measure. We can revert to using `brotlic-sys` if https://github.com/AronParker/brotlic/pull/5
// is ever merged.

#![allow(non_camel_case_types)]
#![allow(dead_code)]
#![allow(non_upper_case_globals)]

use std::os::raw::{c_int, c_void};

/// Opaque structure that holds decoder state.
pub type BrotliDecoderState = c_void;

// From brotli/c/include/brotli/decode.h
pub type BrotliDecoderResult = c_int;
pub const BrotliDecoderResult_BROTLI_DECODER_RESULT_ERROR: BrotliDecoderResult = 0;
pub const BrotliDecoderResult_BROTLI_DECODER_RESULT_SUCCESS: BrotliDecoderResult = 1;
pub const BrotliDecoderResult_BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT: BrotliDecoderResult = 2;
pub const BrotliDecoderResult_BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT: BrotliDecoderResult = 3;

// From brotli/c/include/brotli/shared_dictionary.h
pub type BrotliSharedDictionaryType = c_int;
pub const BrotliSharedDictionaryType_BROTLI_SHARED_DICTIONARY_RAW: BrotliSharedDictionaryType = 0;
pub const BrotliSharedDictionaryType_BROTLI_SHARED_DICTIONARY_SERIALIZED:
    BrotliSharedDictionaryType = 1;

// From brotli/c/include/brotli/types.h
pub type BROTLI_BOOL = c_int;
pub const BROTLI_TRUE: BROTLI_BOOL = 1;
pub const BROTLI_FALSE: BROTLI_BOOL = 0;

// From brotli/c/include/brotli/types.h
pub type brotli_alloc_func =
    Option<unsafe extern "C" fn(opaque: *mut c_void, size: usize) -> *mut c_void>;
pub type brotli_free_func = Option<unsafe extern "C" fn(opaque: *mut c_void, address: *mut c_void)>;

// Functions from brotli/c/include/brotli/decode.h
extern "C" {
    /// Creates an instance of [`BrotliDecoderState`] and initializes it.
    ///
    /// The instance can be used once for decoding and should then be destroyed
    /// with [`BrotliDecoderDestroyInstance`]. It cannot be reused for a new
    /// decoding session.
    ///
    /// # Safety
    ///
    /// `alloc_func` and `free_func` MUST be both `Some` or both `None`. In the
    /// case they are both `None`, default memory allocators are used. `opaque`
    /// is passed to `alloc_func` and `free_func` when they are called.
    /// `free_func` has to return without doing anything when asked to free
    /// a null pointer.
    ///
    /// # Arguments
    ///
    /// * `alloc_func` - Custom memory allocation function.
    /// * `free_func` - Custom memory free function.
    /// * `opaque` - Custom memory manager handle.
    ///
    /// # Returns
    ///
    /// `null` if the instance cannot be allocated or initialized; otherwise, a
    /// pointer to an initialized [`BrotliDecoderState`].
    pub fn BrotliDecoderCreateInstance(
        alloc_func: brotli_alloc_func,
        free_func: brotli_free_func,
        opaque: *mut c_void,
    ) -> *mut BrotliDecoderState;

    /// Adds LZ77 prefix dictionary, adds or replaces built-in static dictionary
    /// and transforms.
    ///
    /// Attached dictionary ownership is not transferred.
    ///
    /// # Note
    ///
    /// Dictionaries can NOT be attached after actual decoding is started.
    ///
    /// # Safety
    ///
    /// * `state` must be a valid pointer to a [`BrotliDecoderState`].
    /// * `data` must point to a valid memory region of at least `data_size`
    ///   bytes.
    /// * The memory pointed to by `data` must be kept accessible until decoding
    ///   is finished and the decoder instance is destroyed.
    ///
    /// # Arguments
    ///
    /// * `state` - Decoder instance.
    /// * `type_` - Dictionary data format.
    /// * `data_size` - Length of memory region pointed by `data`.
    /// * `data` - Dictionary data in format corresponding to `type_`.
    ///
    /// # Returns
    ///
    /// [`BROTLI_FALSE`] if the dictionary is corrupted, or the dictionary count
    /// limit is reached; otherwise, [`BROTLI_TRUE`].
    pub fn BrotliDecoderAttachDictionary(
        state: *mut BrotliDecoderState,
        type_: BrotliSharedDictionaryType,
        data_size: usize,
        data: *const u8,
    ) -> BROTLI_BOOL;

    /// Decompresses the input stream to the output stream.
    ///
    /// The values `available_in` and `available_out` must specify the number of
    /// bytes addressable at `next_in` and `next_out` respectively.
    ///
    /// After each call, `available_in` will be decremented by the amount of
    /// input bytes consumed, and the `next_in` pointer will be incremented
    /// by that amount. Similarly, `available_out` will be decremented by
    /// the amount of output bytes written, and the `next_out` pointer will
    /// be incremented by that amount.
    ///
    /// `total_out`, if it is not a null-pointer, will be set to the number
    /// of bytes decompressed since the last `state` initialization.
    ///
    /// # Note
    ///
    /// Input is never overconsumed, so `next_in` and `available_in` could be
    /// passed to the next consumer after decoding is complete.
    ///
    /// # Safety
    ///
    /// * `state` must be a valid pointer to a [`BrotliDecoderState`].
    /// * `available_in`, `next_in`, `available_out`, and `next_out` must all be
    ///   valid, non-null pointers to their respective types.
    /// * The memory region pointed to by `*next_in` must be at least
    ///   `*available_in` bytes long.
    /// * The memory region pointed to by `*next_out` must be at least
    ///   `*available_out` bytes long, unless `*available_out` is 0, in which
    ///   case `*next_out` may be `null`.
    /// * `total_out` may be `null`; if not, it must be a valid pointer to a
    ///   [`usize`].
    ///
    /// # Arguments
    ///
    /// * `state` - Decoder instance.
    /// * `available_in` - **in:** amount of available input; **out:** amount of
    ///   unused input.
    /// * `next_in` - **in/out:** pointer to the next compressed byte.
    /// * `available_out` - **in:** length of output buffer; **out:** remaining
    ///   size of output buffer.
    /// * `next_out` - **in/out:** output buffer cursor; can be `null` if
    ///   `available_out` is 0.
    /// * `total_out` - **out:** number of bytes decompressed so far; can be
    ///   `null`.
    ///
    /// # Returns
    ///
    /// * [`BrotliDecoderResult_BROTLI_DECODER_RESULT_ERROR`] - Input is
    ///   corrupted, memory allocation failed, arguments were invalid, etc.
    /// * [`BrotliDecoderResult_BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT`] -
    ///   Decoding is blocked until more input data is provided.
    /// * [`BrotliDecoderResult_BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT`] -
    ///   Decoding is blocked until more output space is provided.
    /// * [`BrotliDecoderResult_BROTLI_DECODER_RESULT_SUCCESS`] - Decoding is
    ///   finished; no more input might be consumed and no more output will be
    ///   produced.
    pub fn BrotliDecoderDecompressStream(
        state: *mut BrotliDecoderState,
        available_in: *mut usize,
        next_in: *mut *const u8,
        available_out: *mut usize,
        next_out: *mut *mut u8,
        total_out: *mut usize,
    ) -> BrotliDecoderResult;

    /// Deinitializes and frees a [`BrotliDecoderState`] instance.
    ///
    /// # Safety
    ///
    /// The `state` pointer must be either `null` or a pointer to a valid
    /// [`BrotliDecoderState`] previously allocated with
    /// [`BrotliDecoderCreateInstance`].
    ///
    /// # Arguments
    ///
    /// * `state` - Decoder instance to be cleaned up and deallocated.
    pub fn BrotliDecoderDestroyInstance(state: *mut BrotliDecoderState);
}
