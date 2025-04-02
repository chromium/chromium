/// Provides low-level access to an interface vtable.
///
/// This trait is automatically implemented by the generated bindings and should not be
/// implemented manually.
///
/// # Safety
pub unsafe trait Interface: Sized {
    type Vtable;

    /// A reference to the interface's vtable
    #[doc(hidden)]
    fn vtable(&self) -> &Self::Vtable {
        // SAFETY: the implementor of the trait guarantees that `Self` is castable to its vtable
        unsafe { self.assume_vtable::<Self>() }
    }

    /// Cast this interface as a reference to the supplied interfaces `Vtable`
    ///
    /// # Safety
    ///
    /// This is safe if `T` is an equivalent interface to `Self` or a super interface.
    /// In other words, `T::Vtable` must be equivalent to the beginning of `Self::Vtable`.
    #[doc(hidden)]
    unsafe fn assume_vtable<T: Interface>(&self) -> &T::Vtable {
        &**(self.as_raw() as *mut *mut T::Vtable)
    }

    /// Returns the raw COM interface pointer. The resulting pointer continues to be owned by the `Interface` implementation.
    #[inline(always)]
    fn as_raw(&self) -> *mut std::ffi::c_void {
        // SAFETY: implementors of this trait must guarantee that the implementing type has a pointer in-memory representation
        unsafe { std::mem::transmute_copy(self) }
    }

    /// Returns the raw COM interface pointer and releases ownership. It the caller's responsibility to release the COM interface pointer.
    fn into_raw(self) -> *mut std::ffi::c_void {
        // SAFETY: implementors of this trait must guarantee that the implementing type has a pointer in-memory representation
        let raw = self.as_raw();
        std::mem::forget(self);
        raw
    }

    /// Creates an `Interface` by taking ownership of the `raw` COM interface pointer.
    ///
    /// # Safety
    ///
    /// The `raw` pointer must be owned by the caller and represent a valid COM interface pointer. In other words,
    /// it must point to a vtable beginning with the `IUnknown` function pointers and match the vtable of `Interface`.
    unsafe fn from_raw(raw: *mut std::ffi::c_void) -> Self {
        std::mem::transmute_copy(&raw)
    }

    /// Creates an `Interface` that is valid so long as the `raw` COM interface pointer is valid.
    ///
    /// # Safety
    ///
    /// The `raw` pointer must be a valid COM interface pointer. In other words, it must point to a vtable
    /// beginning with the `IUnknown` function pointers and match the vtable of `Interface`.
    unsafe fn from_raw_borrowed(raw: &*mut std::ffi::c_void) -> Option<&Self> {
        if raw.is_null() {
            None
        } else {
            Some(std::mem::transmute_copy(&raw))
        }
    }
}

/// # Safety
#[doc(hidden)]
pub unsafe fn from_raw_borrowed<T: Interface>(raw: &*mut std::ffi::c_void) -> Option<&T> {
    T::from_raw_borrowed(raw)
}
