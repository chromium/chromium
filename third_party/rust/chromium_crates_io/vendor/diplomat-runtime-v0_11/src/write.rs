use alloc::boxed::Box;
use alloc::vec::Vec;
use core::ffi::c_void;
use core::{fmt, ptr};

/// An object that can one can write UTF-8 strings to
///
/// This allows the C API to write to arbitrary kinds of objects, for example a
/// C++ std::string or a char buffer.
///
/// The way to use this object is to fill out the `buf`, `len`, `cap` fields with
/// appropriate values for the buffer, its current length, and its current capacity,
/// and `flush` and `grow` with appropriate callbacks (using `context` to reference any
/// state they need). This object will be passed by mutable reference to the Rust side,
/// and Rust will write to it, calling `grow()` as necessary. Once done, it will call `flush()`
/// to update any state on `context` (e.g. adding a null terminator, updating the length).
/// The object on the foreign side will be directly usable after this, the foreign side
/// need not perform additional state updates after passing an [`DiplomatWrite`] to
/// a function.
///
/// [`diplomat_simple_write()`] can be used to write to a fixed-size char buffer.
///
/// May be extended in the future to support further invariants
///
/// DiplomatWrite will not perform any cleanup on `context` or `buf`, these are logically
/// "borrows" from the FFI side.
///
/// # Safety invariants:
///  - `flush()` and `grow()` will be passed `self` including `context` and it should always be safe to do so.
///    `context` may be  null, however `flush()` and `grow()` must then be ready to receive it as such.
///  - `buf` must be `cap` bytes long
///  - `grow()` must either return false or update `buf` and `cap` for a valid buffer
///    of at least the requested buffer size
///  - `DiplomatWrite::flush()` will be automatically called by Diplomat. `flush()` might also be called
///    (erroneously) on the Rust side (it's a public method), so it must be idempotent.
#[repr(C)]
pub struct DiplomatWrite {
    /// Context pointer for additional data needed by `grow()` and `flush()`. May be `null`.
    ///
    /// The pointer may reference structured data on the foreign side,
    /// such as C++ std::string, used to reallocate buf.
    context: *mut c_void,
    /// The raw string buffer, which will be mutated on the Rust side.
    buf: *mut u8,
    /// The current filled size of the buffer
    len: usize,
    /// The current capacity of the buffer
    cap: usize,
    /// Set to true if `grow` ever fails.
    grow_failed: bool,
    /// Called by Rust to indicate that there is no more data to write.
    ///
    /// May be called multiple times.
    ///
    /// Arguments:
    /// - `self` (`*mut DiplomatWrite`): This `DiplomatWrite`
    flush: extern "C" fn(*mut DiplomatWrite),
    /// Called by Rust to request more capacity in the buffer. The implementation should allocate a new
    /// buffer and copy the contents of the old buffer into the new buffer, updating `self.buf` and `self.cap`
    ///
    /// Arguments:
    /// - `self` (`*mut DiplomatWrite`): This `DiplomatWrite`
    /// - `capacity` (`usize`): The requested capacity.
    ///
    /// Returns: `true` if the allocation succeeded. Should not update any state if it failed.
    grow: extern "C" fn(*mut DiplomatWrite, usize) -> bool,
}

impl DiplomatWrite {
    /// Call this function before releasing the buffer to C
    pub fn flush(&mut self) {
        (self.flush)(self);
    }
}
impl fmt::Write for DiplomatWrite {
    fn write_str(&mut self, s: &str) -> Result<(), fmt::Error> {
        if self.grow_failed {
            return Ok(());
        }
        let needed_len = self.len + s.len();
        if needed_len > self.cap {
            let success = (self.grow)(self, needed_len);
            if !success {
                self.grow_failed = true;
                return Ok(());
            }
        }
        debug_assert!(needed_len <= self.cap);
        unsafe {
            ptr::copy_nonoverlapping(s.as_bytes().as_ptr(), self.buf.add(self.len), s.len());
        }
        self.len = needed_len;
        Ok(())
    }
}

/// Create an `DiplomatWrite` that can write to a fixed-length stack allocated `u8` buffer.
///
/// Once done, this will append a null terminator to the written string.
///
/// # Safety
///
///  - `buf` must be a valid pointer to a region of memory that can hold at `buf_size` bytes
#[no_mangle]
pub unsafe extern "C" fn diplomat_simple_write(buf: *mut u8, buf_size: usize) -> DiplomatWrite {
    extern "C" fn grow(_this: *mut DiplomatWrite, _cap: usize) -> bool {
        false
    }
    extern "C" fn flush(this: *mut DiplomatWrite) {
        unsafe {
            debug_assert!((*this).len <= (*this).cap);
            let buf = (*this).buf;
            ptr::write(buf.add((*this).len), 0)
        }
    }
    DiplomatWrite {
        context: ptr::null_mut(),
        buf,
        len: 0,
        grow_failed: false,
        // keep an extra byte in our pocket for the null terminator
        cap: buf_size - 1,
        flush,
        grow,
    }
}

/// Create an [`DiplomatWrite`] that can write to a dynamically allocated buffer managed by Rust.
///
/// Use [`diplomat_buffer_write_destroy()`] to free the writable and its underlying buffer.
#[no_mangle]
pub extern "C" fn diplomat_buffer_write_create(cap: usize) -> *mut DiplomatWrite {
    extern "C" fn grow(this: *mut DiplomatWrite, new_cap: usize) -> bool {
        unsafe {
            let this = this.as_mut().unwrap();
            let mut vec = Vec::from_raw_parts(this.buf, 0, this.cap);
            vec.reserve(new_cap);
            this.cap = vec.capacity();
            this.buf = vec.as_mut_ptr();
            core::mem::forget(vec);
        }
        true
    }

    extern "C" fn flush(_: *mut DiplomatWrite) {}

    let mut vec = Vec::<u8>::with_capacity(cap);
    let ret = DiplomatWrite {
        context: ptr::null_mut(),
        buf: vec.as_mut_ptr(),
        len: 0,
        grow_failed: false,
        cap,
        flush,
        grow,
    };

    core::mem::forget(vec);
    Box::into_raw(Box::new(ret))
}

/// Grabs a pointer to the underlying buffer of a writable.
///
/// Returns null if there was an allocation error during the write construction.
///
/// # Safety
/// - The returned pointer is valid until the passed writable is destroyed.
/// - `this` must be a pointer to a valid [`DiplomatWrite`] constructed by
///   [`diplomat_buffer_write_create()`].
#[no_mangle]
pub extern "C" fn diplomat_buffer_write_get_bytes(this: &DiplomatWrite) -> *mut u8 {
    if this.grow_failed {
        core::ptr::null_mut()
    } else {
        this.buf
    }
}

/// Gets the length in bytes of the content written to the writable.
///
/// Returns 0 if there was an allocation error during the write construction.
///
/// # Safety
/// - `this` must be a pointer to a valid [`DiplomatWrite`] constructed by
///   [`diplomat_buffer_write_create()`].
#[no_mangle]
pub extern "C" fn diplomat_buffer_write_len(this: &DiplomatWrite) -> usize {
    if this.grow_failed {
        0
    } else {
        this.len
    }
}

/// Destructor for Rust-memory backed writables.
///
/// # Safety
/// - `this` must be a pointer to a valid [`DiplomatWrite`] constructed by
///   [`diplomat_buffer_write_create()`].
#[no_mangle]
pub unsafe extern "C" fn diplomat_buffer_write_destroy(this: *mut DiplomatWrite) {
    let this = Box::from_raw(this);
    let vec = Vec::from_raw_parts(this.buf, 0, this.cap);
    drop(vec);
    drop(this);
}
