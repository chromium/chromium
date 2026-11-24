//! Binary data with a lifetime, mirroring HarfBuzz's `hb-blob.h`.

use core::ffi::{c_char, c_int, c_uint, c_void};
use std::sync::{Arc, OnceLock};

use harfrust::font::Blob;

use crate::common::hr_bool_t;
use crate::object::{self, hr_destroy_func_t, hr_user_data_key_t, Empty, Object, ObjectHeader};

/// How a blob relates to the memory it was created over.
///
/// An integer typedef rather than an enumeration. A value arriving from C need
/// not be one of the ones named here, and a Rust enumeration holding an
/// unnamed value is undefined behaviour rather than merely wrong.
pub type hr_memory_mode_t = c_int;

/// Copy the data. The caller keeps ownership of the original buffer.
pub const HR_MEMORY_MODE_DUPLICATE: hr_memory_mode_t = 0;
/// Use the data in place. It must outlive the blob and never change.
pub const HR_MEMORY_MODE_READONLY: hr_memory_mode_t = 1;
/// Use the data in place. Treated as read-only by this library.
pub const HR_MEMORY_MODE_WRITABLE: hr_memory_mode_t = 2;
/// Use the data in place. Treated as read-only by this library.
pub const HR_MEMORY_MODE_READONLY_MAY_MAKE_WRITABLE: hr_memory_mode_t = 3;

/// Bytes owned by the caller, released through a destroy callback.
struct ForeignBytes {
    data: *const u8,
    len: usize,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
}

// SAFETY: the caller promises, as documented on `hr_blob_create`, that the
// data stays valid and unchanging for the blob's lifetime, and that the
// destroy callback may run on any thread.
unsafe impl Send for ForeignBytes {}
unsafe impl Sync for ForeignBytes {}

impl AsRef<[u8]> for ForeignBytes {
    fn as_ref(&self) -> &[u8] {
        if self.data.is_null() || self.len == 0 {
            return &[];
        }
        // SAFETY: upheld by the caller of `hr_blob_create`.
        unsafe { core::slice::from_raw_parts(self.data, self.len) }
    }
}

impl Drop for ForeignBytes {
    fn drop(&mut self) {
        if let Some(destroy) = self.destroy {
            // SAFETY: `destroy` was supplied alongside `user_data`.
            unsafe { destroy(self.user_data) };
        }
    }
}

/// An owned reference to a blob, released when dropped.
///
/// This keeps the blob itself alive and not only its storage: the caller may
/// have tied the bytes to the blob through `hr_blob_set_user_data`, whose
/// destructors run when the blob is freed, so the bytes must not be read past
/// the blob's last reference.
struct BlobRef(*mut hr_blob_t);

// SAFETY: this owns one of the blob's references, so the blob stays live for
// as long as this does, and a live blob may be shared between threads: its
// reference count is atomic, its storage is never written, and callers are
// required to make their user data safe to release from any thread.
unsafe impl Send for BlobRef {}
unsafe impl Sync for BlobRef {}

impl AsRef<[u8]> for BlobRef {
    fn as_ref(&self) -> &[u8] {
        // SAFETY: the reference this owns keeps the blob alive.
        unsafe { &*self.0 }.bytes()
    }
}

impl Drop for BlobRef {
    fn drop(&mut self) {
        // SAFETY: this owns the reference it releases.
        unsafe { object::destroy(self.0) };
    }
}

/// A slice of another blob, keeping the parent alive.
struct SubBytes {
    parent: BlobRef,
    start: usize,
    end: usize,
}

impl AsRef<[u8]> for SubBytes {
    fn as_ref(&self) -> &[u8] {
        self.parent
            .as_ref()
            .get(self.start..self.end)
            .unwrap_or(&[])
    }
}

/// A slice of an owned, reference-counted blob, holding its own clone of it.
///
/// Unlike `SubBytes`, this keeps a cloned `Blob` rather than a reference to a
/// parent `hr_blob_t` object, so it is for storage that this library owns
/// outright (the assembled render SFNT), not for data a `hr_blob_set_user_data`
/// callback keeps alive through the blob object.
#[cfg(feature = "paint")]
struct SharedSub {
    parent: Blob,
    start: usize,
    end: usize,
}

#[cfg(feature = "paint")]
impl AsRef<[u8]> for SharedSub {
    fn as_ref(&self) -> &[u8] {
        self.parent
            .as_ref()
            .get(self.start..self.end)
            .unwrap_or(&[])
    }
}

/// Binary data with a lifetime.
pub struct hr_blob_t {
    header: ObjectHeader,
    pub(crate) blob: Blob,
}

impl hr_blob_t {
    pub(crate) fn new(blob: Blob) -> *mut Self {
        object::create(hr_blob_t {
            header: ObjectHeader::new(),
            blob,
        })
    }

    pub(crate) fn bytes(&self) -> &[u8] {
        self.blob.as_ref()
    }

    /// Takes over a reference to `blob` as data that keeps the blob alive.
    ///
    /// # Safety
    ///
    /// `blob` must be a live blob, and the caller must own the reference
    /// being handed over.
    pub(crate) unsafe fn into_data(blob: *mut Self) -> Blob {
        Blob::Shared(Arc::new(BlobRef(blob)) as Arc<dyn AsRef<[u8]> + Send + Sync>)
    }

    /// Creates a blob over a range of `parent`, sharing its storage and
    /// keeping `parent` alive.
    ///
    /// # Safety
    ///
    /// `parent` must be `NULL` or a live blob.
    pub(crate) unsafe fn sub(parent: *mut Self, start: usize, end: usize) -> *mut Self {
        let Some(parent_ref) = (unsafe { parent.as_ref() }) else {
            return Self::empty();
        };
        if start >= end || end > parent_ref.bytes().len() {
            return Self::empty();
        }
        let sub = SubBytes {
            // SAFETY: `parent` is live, and the reference taken here is the
            // sub-blob's own.
            parent: BlobRef(unsafe { object::reference(parent) }),
            start,
            end,
        };
        Self::new(Blob::Shared(
            Arc::new(sub) as Arc<dyn AsRef<[u8]> + Send + Sync>
        ))
    }

    /// Creates a blob over a range of an owned `Blob`, cloning it so the
    /// sub-blob keeps the storage alive on its own.
    ///
    /// For data this library owns and reference-counts (the assembled render
    /// SFNT), where there is no parent `hr_blob_t` object to reference the way
    /// `sub` does.
    #[cfg(feature = "paint")]
    pub(crate) fn sub_shared(parent: &Blob, start: usize, end: usize) -> *mut Self {
        if start >= end || end > parent.as_ref().len() {
            return Self::empty();
        }
        let sub = SharedSub {
            parent: parent.clone(),
            start,
            end,
        };
        Self::new(Blob::Shared(
            Arc::new(sub) as Arc<dyn AsRef<[u8]> + Send + Sync>
        ))
    }
}

static EMPTY_BLOB: OnceLock<Empty<hr_blob_t>> = OnceLock::new();

impl Object for hr_blob_t {
    fn header(&self) -> &ObjectHeader {
        &self.header
    }

    fn empty() -> *mut Self {
        EMPTY_BLOB
            .get_or_init(|| {
                Empty::new(hr_blob_t {
                    header: ObjectHeader::immortal(),
                    blob: Blob::Static(&[]),
                })
            })
            .get()
    }
}

/// Creates a blob over `length` bytes at `data`.
///
/// With [`hr_memory_mode_t::HR_MEMORY_MODE_DUPLICATE`] the data is copied and
/// `destroy` is called before returning. Otherwise the blob borrows the data,
/// which must stay valid and unchanged until `destroy` is invoked.
///
/// Never returns `NULL`; on failure it returns the empty blob.
///
/// # Safety
///
/// `data` must point to `length` readable bytes. `destroy`, if given, must be
/// safe to call from any thread.
#[no_mangle]
pub unsafe extern "C" fn hr_blob_create(
    data: *const c_char,
    length: c_uint,
    mode: hr_memory_mode_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) -> *mut hr_blob_t {
    // Nothing to hold is the shared empty blob, which holds nothing at all
    // and so cannot hold the caller's data either: that is released now
    // rather than kept for a blob that is never destroyed. Asking for a blob
    // of nothing that can be is what `hr_blob_create_or_fail` is for.
    if length == 0 {
        if let Some(destroy) = destroy {
            // SAFETY: `destroy` was supplied alongside `user_data`.
            unsafe { destroy(user_data) };
        }
        return hr_blob_t::empty();
    }
    let blob = unsafe { blob_from_raw(data, length, mode, user_data, destroy) };
    match blob {
        Some(blob) => hr_blob_t::new(blob),
        None => hr_blob_t::empty(),
    }
}

/// Creates a blob, returning `NULL` rather than the empty blob on failure.
///
/// # Safety
///
/// See [`hr_blob_create`].
#[no_mangle]
pub unsafe extern "C" fn hr_blob_create_or_fail(
    data: *const c_char,
    length: c_uint,
    mode: hr_memory_mode_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) -> *mut hr_blob_t {
    let blob = unsafe { blob_from_raw(data, length, mode, user_data, destroy) };
    match blob {
        Some(blob) => hr_blob_t::new(blob),
        None => core::ptr::null_mut(),
    }
}

/// # Safety
///
/// See [`hr_blob_create`].
unsafe fn blob_from_raw(
    data: *const c_char,
    length: c_uint,
    mode: hr_memory_mode_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) -> Option<Blob> {
    // A length that cannot be represented is the one thing HarfBuzz refuses
    // outright, handing the data back as it does so.
    if length >= 1 << 31 {
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) };
        }
        return None;
    }
    let empty = data.is_null() || length == 0;
    if mode == HR_MEMORY_MODE_DUPLICATE {
        let copy = if empty {
            Vec::new()
        } else {
            // SAFETY: the caller guarantees `length` readable bytes at `data`.
            unsafe { core::slice::from_raw_parts(data.cast::<u8>(), length as usize) }.to_vec()
        };
        // The copy is this blob's own, so the caller's data is released now.
        if let Some(destroy) = destroy {
            unsafe { destroy(user_data) };
        }
        return Some(Blob::from(copy));
    }
    // A blob of nothing is still a blob, and still holds the caller's data
    // until it is destroyed: HarfBuzz fails only on a length it cannot take.
    let bytes = ForeignBytes {
        data: data.cast::<u8>(),
        len: if empty { 0 } else { length as usize },
        user_data,
        destroy,
    };
    Some(Blob::Shared(
        Arc::new(bytes) as Arc<dyn AsRef<[u8]> + Send + Sync>
    ))
}

/// Creates a blob over the contents of a file.
///
/// Never returns `NULL`; on failure it returns the empty blob.
///
/// # Safety
///
/// `file_name` must be a NUL-terminated string.
#[no_mangle]
pub unsafe extern "C" fn hr_blob_create_from_file(file_name: *const c_char) -> *mut hr_blob_t {
    let blob = unsafe { hr_blob_create_from_file_or_fail(file_name) };
    if blob.is_null() {
        return hr_blob_t::empty();
    }
    blob
}

/// Creates a blob over the contents of a file, returning `NULL` on failure.
///
/// # Safety
///
/// `file_name` must be a NUL-terminated string.
#[no_mangle]
pub unsafe extern "C" fn hr_blob_create_from_file_or_fail(
    file_name: *const c_char,
) -> *mut hr_blob_t {
    if file_name.is_null() {
        return core::ptr::null_mut();
    }
    let path = unsafe { core::ffi::CStr::from_ptr(file_name) };
    let Ok(path) = path.to_str() else {
        return core::ptr::null_mut();
    };
    match std::fs::read(path) {
        Ok(data) => hr_blob_t::new(Blob::from(data)),
        Err(_) => core::ptr::null_mut(),
    }
}

/// Creates a blob over a range of another blob, sharing its storage.
///
/// The range is clamped to the parent's bounds. The sub-blob keeps the parent
/// alive, as in HarfBuzz. Never returns `NULL`.
///
/// # Safety
///
/// `parent` must be `NULL` or a live blob.
#[no_mangle]
pub unsafe extern "C" fn hr_blob_create_sub_blob(
    parent: *mut hr_blob_t,
    offset: c_uint,
    length: c_uint,
) -> *mut hr_blob_t {
    let Some(parent_ref) = (unsafe { parent.as_ref() }) else {
        return hr_blob_t::empty();
    };
    // The part cannot outlive what it is a part of being what it was, so the
    // parent is fixed as it stands, as `hr_blob_make_immutable` would.
    unsafe { hr_blob_make_immutable(parent) };
    let total = parent_ref.bytes().len();
    let start = (offset as usize).min(total);
    let end = start.saturating_add(length as usize).min(total);
    unsafe { hr_blob_t::sub(parent, start, end) }
}

/// Returns a new blob holding a private copy of `blob`'s data.
///
/// Returns `NULL` if the blob is empty or the copy could not be made.
///
/// # Safety
///
/// `blob` must be `NULL` or a live blob.
#[no_mangle]
pub unsafe extern "C" fn hr_blob_copy_writable_or_fail(blob: *mut hr_blob_t) -> *mut hr_blob_t {
    let Some(blob) = (unsafe { blob.as_ref() }) else {
        return core::ptr::null_mut();
    };
    let bytes = blob.bytes();
    if bytes.is_empty() {
        return core::ptr::null_mut();
    }
    hr_blob_t::new(Blob::from(bytes.to_vec()))
}

/// Returns the immortal empty blob.
#[no_mangle]
pub extern "C" fn hr_blob_get_empty() -> *mut hr_blob_t {
    hr_blob_t::empty()
}

/// Increments a blob's reference count.
///
/// # Safety
///
/// `blob` must be `NULL` or a live blob.
#[no_mangle]
pub unsafe extern "C" fn hr_blob_reference(blob: *mut hr_blob_t) -> *mut hr_blob_t {
    unsafe { object::reference(blob) }
}

/// Decrements a blob's reference count, freeing it at zero.
///
/// # Safety
///
/// `blob` must be `NULL` or a live blob, and the caller must own the
/// reference being released.
#[no_mangle]
pub unsafe extern "C" fn hr_blob_destroy(blob: *mut hr_blob_t) {
    unsafe { object::destroy(blob) };
}

/// Attaches user data to a blob.
///
/// # Safety
///
/// `blob` must be `NULL` or a live blob, and `key` must outlive it.
#[no_mangle]
pub unsafe extern "C" fn hr_blob_set_user_data(
    blob: *mut hr_blob_t,
    key: *const hr_user_data_key_t,
    data: *mut c_void,
    destroy: hr_destroy_func_t,
    replace: hr_bool_t,
) -> hr_bool_t {
    unsafe { object::set_user_data(blob, key, data, destroy, replace != 0) }.into()
}

/// Retrieves user data previously attached to a blob.
///
/// # Safety
///
/// `blob` must be `NULL` or a live blob.
#[no_mangle]
pub unsafe extern "C" fn hr_blob_get_user_data(
    blob: *mut hr_blob_t,
    key: *const hr_user_data_key_t,
) -> *mut c_void {
    unsafe { object::get_user_data(blob, key) }
}

/// Marks a blob immutable.
///
/// # Safety
///
/// `blob` must be `NULL` or a live blob.
#[no_mangle]
pub unsafe extern "C" fn hr_blob_make_immutable(blob: *mut hr_blob_t) {
    unsafe { object::make_immutable(blob) };
}

/// Returns whether a blob has been marked immutable.
///
/// # Safety
///
/// `blob` must be `NULL` or a live blob.
#[no_mangle]
pub unsafe extern "C" fn hr_blob_is_immutable(blob: *mut hr_blob_t) -> hr_bool_t {
    unsafe { object::is_immutable(blob.cast_const()) }.into()
}

/// Returns the length of a blob's data, in bytes.
///
/// # Safety
///
/// `blob` must be `NULL` or a live blob.
#[no_mangle]
pub unsafe extern "C" fn hr_blob_get_length(blob: *mut hr_blob_t) -> c_uint {
    unsafe { object::or_empty(blob.cast_const()) }.bytes().len() as c_uint
}

/// Returns a blob's data, writing its length to `length` when non-`NULL`.
///
/// # Safety
///
/// `blob` must be `NULL` or a live blob, and `length` must be `NULL` or
/// writable.
#[no_mangle]
pub unsafe extern "C" fn hr_blob_get_data(
    blob: *mut hr_blob_t,
    length: *mut c_uint,
) -> *const c_char {
    let bytes = unsafe { object::or_empty(blob.cast_const()) }.bytes();
    if let Some(length) = unsafe { length.as_mut() } {
        *length = bytes.len() as c_uint;
    }
    bytes.as_ptr().cast::<c_char>()
}

/// Always returns `NULL`, writing zero to `length`.
///
/// Blobs in this library are read-only, since the shaping API never needs to
/// modify font data in place. Use [`hr_blob_copy_writable_or_fail`] to obtain
/// a private, mutable copy instead.
///
/// # Safety
///
/// `length` must be `NULL` or writable.
#[no_mangle]
pub unsafe extern "C" fn hr_blob_get_data_writable(
    _blob: *mut hr_blob_t,
    length: *mut c_uint,
) -> *mut c_char {
    if let Some(length) = unsafe { length.as_mut() } {
        *length = 0;
    }
    core::ptr::null_mut()
}
