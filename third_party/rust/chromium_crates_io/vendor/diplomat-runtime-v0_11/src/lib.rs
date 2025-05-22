#![cfg_attr(not(any(target_arch = "wasm32")), no_std)]

extern crate alloc;

use alloc::alloc::Layout;

#[cfg(target_arch = "wasm32")]
// defines `extern "C" diplomat_init()`
mod wasm_glue;

mod write;
pub use write::DiplomatWrite;
pub use write::{diplomat_buffer_write_create, diplomat_buffer_write_destroy};
mod slices;
pub use slices::{
    DiplomatOwnedSlice, DiplomatOwnedStr16Slice, DiplomatOwnedStrSlice, DiplomatOwnedUTF8StrSlice,
    DiplomatSlice, DiplomatSliceMut, DiplomatStr16Slice, DiplomatStrSlice, DiplomatUtf8StrSlice,
};

mod callback;
pub use callback::DiplomatCallback;

mod result;
pub use result::{DiplomatOption, DiplomatResult};

/// Like [`char`], but unvalidated.
pub type DiplomatChar = u32;

/// Like [`str`], but unvalidated.
pub type DiplomatStr = [u8];

/// Like `Wstr`, but unvalidated.
pub type DiplomatStr16 = [u16];

/// Like [`u8`], but interpreted explicitly as a raw byte as opposed to a numerical value.
/// This matters for languages like JavaScript or Dart, where there's only a single numeric
/// type, but special types for byte buffers.
pub type DiplomatByte = u8;

/// Allocates a buffer of a given size in Rust's memory.
///
/// # Safety
/// - The allocated buffer must be freed with [`diplomat_free()`].
#[no_mangle]
pub unsafe extern "C" fn diplomat_alloc(size: usize, align: usize) -> *mut u8 {
    alloc::alloc::alloc(Layout::from_size_align(size, align).unwrap())
}

/// Frees a buffer that was allocated in Rust's memory.
/// # Safety
/// - `ptr` must be a pointer to a valid buffer allocated by [`diplomat_alloc()`].
#[no_mangle]
pub unsafe extern "C" fn diplomat_free(ptr: *mut u8, size: usize, align: usize) {
    alloc::alloc::dealloc(ptr, Layout::from_size_align(size, align).unwrap())
}

/// Whether a `&[u8]` is a `&str`
/// # Safety
/// - `ptr` and `size` must be a valid `&[u8]`
#[no_mangle]
pub unsafe extern "C" fn diplomat_is_str(ptr: *const u8, size: usize) -> bool {
    core::str::from_utf8(core::slice::from_raw_parts(ptr, size)).is_ok()
}
