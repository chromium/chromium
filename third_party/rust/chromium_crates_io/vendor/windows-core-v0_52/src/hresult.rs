use super::*;

/// An error code value returned by most COM functions.
#[repr(transparent)]
#[derive(Copy, Clone, Default, Eq, PartialEq)]
#[must_use]
#[allow(non_camel_case_types)]
pub struct HRESULT(pub i32);

impl HRESULT {
    /// Returns [`true`] if `self` is a success code.
    #[inline]
    pub const fn is_ok(self) -> bool {
        self.0 >= 0
    }

    /// Returns [`true`] if `self` is a failure code.
    #[inline]
    pub const fn is_err(self) -> bool {
        !self.is_ok()
    }

    /// Asserts that `self` is a success code.
    ///
    /// This will invoke the [`panic!`] macro if `self` is a failure code and display
    /// the [`HRESULT`] value for diagnostics.
    #[inline]
    #[track_caller]
    pub fn unwrap(self) {
        assert!(self.is_ok(), "HRESULT 0x{:X}", self.0);
    }

    /// Converts the [`HRESULT`] to [`Result<()>`][Result<_>].
    #[inline]
    pub fn ok(self) -> Result<()> {
        if self.is_ok() {
            Ok(())
        } else {
            Err(Error::from(self))
        }
    }

    /// Returns the [`Option`] as a [`Result`] if the option is a [`Some`] value, returning
    /// a suitable error if not.
    pub fn and_some<T: Interface>(self, some: Option<T>) -> Result<T> {
        if self.is_ok() {
            if let Some(result) = some {
                Ok(result)
            } else {
                Err(Error::OK)
            }
        } else {
            Err(Error::from(self))
        }
    }

    /// Calls `op` if `self` is a success code, otherwise returns [`HRESULT`]
    /// converted to [`Result<T>`].
    #[inline]
    pub fn and_then<F, T>(self, op: F) -> Result<T>
    where
        F: FnOnce() -> T,
    {
        self.ok()?;
        Ok(op())
    }

    /// If the [`Result`] is [`Ok`] converts the `T::Abi` into `T`.
    ///
    /// # Safety
    ///
    /// Safe to call if
    /// * `abi` is initialized if `self` is `Ok`
    /// * `abi` can be safely transmuted to `T`
    pub unsafe fn from_abi<T: Type<T>>(self, abi: T::Abi) -> Result<T> {
        if self.is_ok() {
            T::from_abi(abi)
        } else {
            Err(Error::from(self))
        }
    }

    /// The error message describing the error.
    pub fn message(&self) -> HSTRING {
        let mut message = HeapString(std::ptr::null_mut());

        unsafe {
            let size = crate::imp::FormatMessageW(crate::imp::FORMAT_MESSAGE_ALLOCATE_BUFFER | crate::imp::FORMAT_MESSAGE_FROM_SYSTEM | crate::imp::FORMAT_MESSAGE_IGNORE_INSERTS, std::ptr::null(), self.0 as u32, 0, &mut message.0 as *mut _ as *mut _, 0, std::ptr::null());

            HSTRING::from_wide(crate::imp::wide_trim_end(std::slice::from_raw_parts(message.0 as *const u16, size as usize))).unwrap_or_default()
        }
    }

    /// Maps a Win32 error code to an HRESULT value.
    pub const fn from_win32(error: u32) -> Self {
        Self(if error as i32 <= 0 { error } else { (error & 0x0000_FFFF) | (7 << 16) | 0x8000_0000 } as i32)
    }
}

impl RuntimeType for HRESULT {
    const SIGNATURE: crate::imp::ConstBuffer = crate::imp::ConstBuffer::from_slice(b"struct(Windows.Foundation.HResult;i32)");
}

impl TypeKind for HRESULT {
    type TypeKind = CopyType;
}

impl<T> std::convert::From<Result<T>> for HRESULT {
    fn from(result: Result<T>) -> Self {
        if let Err(error) = result {
            return error.into();
        }

        HRESULT(0)
    }
}

impl std::fmt::Display for HRESULT {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_fmt(format_args!("{:#010X}", self.0))
    }
}

impl std::fmt::Debug for HRESULT {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_fmt(format_args!("HRESULT({})", self))
    }
}

struct HeapString(*mut u16);

impl Drop for HeapString {
    fn drop(&mut self) {
        if !self.0.is_null() {
            unsafe {
                crate::imp::heap_free(self.0 as _);
            }
        }
    }
}
