//! Reference counting, user data and immutability, mirroring HarfBuzz's
//! object model (`hb-object.hh`).
//!
//! Every public object in this API carries an [`ObjectHeader`] as its first
//! field. Objects are heap allocated and handed to C as raw pointers, with the
//! usual `hr_*_reference` / `hr_*_destroy` pair managing their lifetime.
//!
//! Each type also has an immortal "empty" singleton, returned by
//! `hr_*_get_empty()` and used in place of `NULL` whenever a constructor
//! fails. Immortal objects carry a reference count of [`IMMORTAL`]; referencing
//! and destroying them are both no-ops, and they reject user data, exactly as
//! in HarfBuzz.

use core::ffi::c_void;
use core::sync::atomic::{AtomicBool, AtomicI64, Ordering};
use std::sync::{PoisonError, RwLock};

/// Reference count of an immortal (inert) object.
const IMMORTAL: i64 = -1;

/// Callback invoked to release a `user_data` value.
pub type hr_destroy_func_t = Option<unsafe extern "C" fn(user_data: *mut c_void)>;

/// Key used to attach and retrieve user data on an object.
///
/// Only the address of the key matters; its contents are never read.
#[repr(C)]
pub struct hr_user_data_key_t {
    /// Unused; present so the struct has a well-defined size in C.
    pub unused: core::ffi::c_char,
}

/// One `user_data` entry, owning its value.
struct UserDataItem {
    key: *const hr_user_data_key_t,
    data: *mut c_void,
    destroy: hr_destroy_func_t,
}

// SAFETY: the pointers are opaque to us; we only ever compare `key` by address
// and hand `data` back to the callbacks the caller supplied. Callers are
// required to make their user data safe to release from any thread, which is
// documented on `hr_*_set_user_data`.
unsafe impl Send for UserDataItem {}
unsafe impl Sync for UserDataItem {}

impl Drop for UserDataItem {
    fn drop(&mut self) {
        if let Some(destroy) = self.destroy {
            // SAFETY: `destroy` was supplied by the caller alongside `data`.
            unsafe { destroy(self.data) };
        }
    }
}

/// Header shared by every reference counted object in this API.
pub struct ObjectHeader {
    ref_count: AtomicI64,
    immutable: AtomicBool,
    /// Read far more often than written: user data is typically attached once
    /// and then looked up, so readers should not queue behind one another.
    user_data: RwLock<Vec<UserDataItem>>,
}

impl ObjectHeader {
    /// Creates a header for a live object with a reference count of one.
    pub fn new() -> Self {
        Self {
            ref_count: AtomicI64::new(1),
            immutable: AtomicBool::new(false),
            user_data: RwLock::new(Vec::new()),
        }
    }

    /// Creates a header for an immortal object.
    pub fn immortal() -> Self {
        Self {
            ref_count: AtomicI64::new(IMMORTAL),
            immutable: AtomicBool::new(true),
            user_data: RwLock::new(Vec::new()),
        }
    }

    fn is_immortal(&self) -> bool {
        self.ref_count.load(Ordering::Relaxed) == IMMORTAL
    }

    /// Runs the destroy callback of every user data item and forgets them.
    ///
    /// The callbacks are the caller's code and may come back into the API,
    /// so they run once the lock is released, as they do in [`set_user_data`].
    fn release_user_data(&self) {
        let items = {
            let mut items = self
                .user_data
                .write()
                .unwrap_or_else(PoisonError::into_inner);
            core::mem::take(&mut *items)
        };
        drop(items);
    }
}

impl Default for ObjectHeader {
    fn default() -> Self {
        Self::new()
    }
}

/// Implemented by every reference counted object in this API.
pub trait Object: Sized + 'static {
    /// Returns this object's header.
    fn header(&self) -> &ObjectHeader;

    /// Returns the immortal empty singleton for this type.
    fn empty() -> *mut Self;

    /// Releases callback-owned state before exclusive Box ownership is taken.
    ///
    /// # Safety
    ///
    /// `ptr` must point to this object after its final reference was released.
    /// No other thread may access it. Detach state before invoking callbacks,
    /// which may still query the object through its original pointer.
    unsafe fn finalize(_ptr: *mut Self) {}
}

/// Allocates a new object with a reference count of one.
pub fn create<T: Object>(value: T) -> *mut T {
    Box::into_raw(Box::new(value))
}

/// A leaked, immortal object, held so that `hr_*_get_empty` hands back the
/// same pointer every time.
///
/// This keeps the pointer itself rather than its address, so that the
/// provenance needed to dereference it survives.
pub struct Empty<T>(*mut T);

// SAFETY: the pointee is leaked and lives as long as the process. Immortal
// objects reject user data and are permanently immutable, so nothing a caller
// can reach through one is written after it is built.
unsafe impl<T> Send for Empty<T> {}
unsafe impl<T> Sync for Empty<T> {}

impl<T> Empty<T> {
    /// Leaks `value`, to be handed out for the life of the process.
    pub fn new(value: T) -> Self {
        Self(Box::into_raw(Box::new(value)))
    }

    /// Returns the singleton.
    pub fn get(&self) -> *mut T {
        self.0
    }
}

/// Increments the reference count and returns the object.
///
/// Returns `NULL` for a `NULL` argument, and is a no-op for immortal objects.
///
/// # Safety
///
/// `ptr` must be `NULL` or a pointer returned by this API and not yet
/// destroyed.
pub unsafe fn reference<T: Object>(ptr: *mut T) -> *mut T {
    let Some(obj) = (unsafe { ptr.as_ref() }) else {
        return core::ptr::null_mut();
    };
    let header = obj.header();
    if header.is_immortal() {
        return ptr;
    }
    header.ref_count.fetch_add(1, Ordering::Relaxed);
    ptr
}

/// Decrements the reference count, freeing the object when it reaches zero.
///
/// Ignores `NULL` and immortal objects.
///
/// # Safety
///
/// `ptr` must be `NULL` or a pointer returned by this API and not yet
/// destroyed. The caller must own the reference being released.
pub unsafe fn destroy<T: Object>(ptr: *mut T) {
    let Some(obj) = (unsafe { ptr.as_ref() }) else {
        return;
    };
    let header = obj.header();
    if header.is_immortal() {
        return;
    }
    if header.ref_count.fetch_sub(1, Ordering::AcqRel) != 1 {
        return;
    }
    // Last reference. As in HarfBuzz, the destroy callback of every attached
    // user data item runs first, while the object still holds everything it
    // owns: a callback on a font may look at the font's face. Only then is
    // ownership taken back and the object dropped.
    header.release_user_data();
    unsafe { T::finalize(ptr) };
    drop(unsafe { Box::from_raw(ptr) });
}

/// Attaches user data to an object, returning `false` if it could not be set.
///
/// # Safety
///
/// `ptr` must be `NULL` or a live pointer returned by this API. `key` must
/// outlive the object.
pub unsafe fn set_user_data<T: Object>(
    ptr: *mut T,
    key: *const hr_user_data_key_t,
    data: *mut c_void,
    destroy_func: hr_destroy_func_t,
    replace: bool,
) -> bool {
    let Some(obj) = (unsafe { ptr.as_ref() }) else {
        return false;
    };
    let header = obj.header();
    // Inert objects and objects releasing their final reference never take
    // user data. A destroy callback may query the detached metadata, but
    // must not attach callbacks that would later run inside Box::drop.
    if header.ref_count.load(Ordering::Relaxed) <= 0 || key.is_null() {
        return false;
    }
    // Whatever this displaces is dropped once the lock is released, because
    // dropping runs the caller's destructor, and HarfBuzz permits that to
    // come back in here. Releasing it under the lock would deadlock.
    let mut displaced = None;
    let stored = {
        let Ok(mut items) = header.user_data.write() else {
            return false;
        };
        // Neither data nor a destructor means the entry goes away, rather
        // than staying behind to answer with NULL: HarfBuzz removes the key,
        // and a key left in place could never be set again without `replace`.
        let clearing = data.is_null() && destroy_func.is_none();
        match items.iter().position(|item| item.key == key) {
            Some(_) if !replace => false,
            Some(at) if clearing => {
                displaced = Some(items.remove(at));
                true
            }
            Some(at) => {
                displaced = Some(core::mem::replace(
                    &mut items[at],
                    UserDataItem {
                        key,
                        data,
                        destroy: destroy_func,
                    },
                ));
                true
            }
            // Clearing a key that is not there has nothing to do, but is not
            // a failure.
            None if clearing && replace => true,
            None => {
                items.push(UserDataItem {
                    key,
                    data,
                    destroy: destroy_func,
                });
                true
            }
        }
    };
    drop(displaced);
    stored
}

/// Retrieves user data previously attached with [`set_user_data`].
///
/// # Safety
///
/// `ptr` must be `NULL` or a live pointer returned by this API.
pub unsafe fn get_user_data<T: Object>(ptr: *mut T, key: *const hr_user_data_key_t) -> *mut c_void {
    let Some(obj) = (unsafe { ptr.as_ref() }) else {
        return core::ptr::null_mut();
    };
    let Ok(items) = obj.header().user_data.read() else {
        return core::ptr::null_mut();
    };
    items
        .iter()
        .find(|item| item.key == key)
        .map_or(core::ptr::null_mut(), |item| item.data)
}

/// Marks an object immutable. Immutable objects reject all further mutation.
///
/// # Safety
///
/// `ptr` must be `NULL` or a live pointer returned by this API.
pub unsafe fn make_immutable<T: Object>(ptr: *mut T) {
    if let Some(obj) = unsafe { ptr.as_ref() } {
        obj.header().immutable.store(true, Ordering::Release);
    }
}

/// Returns whether an object has been marked immutable.
///
/// # Safety
///
/// `ptr` must be `NULL` or a live pointer returned by this API.
pub unsafe fn is_immutable<T: Object>(ptr: *const T) -> bool {
    unsafe { ptr.as_ref() }.is_some_and(|obj| obj.header().immutable.load(Ordering::Acquire))
}

/// Returns a shared reference to an object, falling back to its immortal empty
/// singleton when `ptr` is `NULL`.
///
/// This mirrors HarfBuzz, where passing `NULL` to a getter behaves like
/// passing the empty object rather than crashing.
///
/// # Safety
///
/// `ptr` must be `NULL` or a live pointer returned by this API.
pub unsafe fn or_empty<'a, T: Object>(ptr: *const T) -> &'a T {
    match unsafe { ptr.as_ref() } {
        Some(obj) => obj,
        // SAFETY: `empty()` always returns a live, leaked singleton.
        None => unsafe { &*T::empty().cast_const() },
    }
}

/// Returns a mutable reference to an object, or `None` when `ptr` is `NULL` or
/// the object has been marked immutable or is releasing its final reference.
///
/// Setters use this so that mutating an immutable object is silently ignored,
/// as it is in HarfBuzz.
///
/// # Safety
///
/// `ptr` must be `NULL` or a live pointer returned by this API, and the caller
/// must not alias the returned reference.
pub unsafe fn as_mutable<'a, T: Object>(ptr: *mut T) -> Option<&'a mut T> {
    // Reject shared immutable objects and finalization reentry before taking
    // an exclusive reference. Destroy callbacks may still query the object.
    let header = unsafe { ptr.as_ref() }?.header();
    if header.immutable.load(Ordering::Acquire) || header.ref_count.load(Ordering::Relaxed) <= 0 {
        return None;
    }
    unsafe { ptr.as_mut() }
}
