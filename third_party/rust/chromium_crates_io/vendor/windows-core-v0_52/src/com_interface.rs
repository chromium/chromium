use super::*;

/// Provides low-level access to a COM interface.
///
/// This trait is automatically implemented by the generated bindings and should not be
/// implemented manually.
///
/// # Safety
///
/// It is only safe to implement this trait if the implementing type is a valid COM interface pointer meaning
/// the following criteria are met:
/// * its in-memory representation is equal to `NonNull<NonNull<Self::VTable>>`
/// * the vtable is correctly structured beginning with the `IUnknown` function pointers
/// * the vtable must be the correct vtable for the supplied IID
pub unsafe trait ComInterface: Interface + Clone {
    /// A unique identifier representing this interface.
    const IID: GUID;

    // Casts the `ComInterface` to a `IUnknown`.
    fn as_unknown(&self) -> &IUnknown {
        // SAFETY: it is always safe to treat a `ComInterface` as an `IUnknown`.
        unsafe { std::mem::transmute(self) }
    }

    /// Attempts to cast the current interface to another interface using `QueryInterface`.
    ///
    /// The name `cast` is preferred to `query` because there is a WinRT method named query but not one
    /// named cast.
    fn cast<T: ComInterface>(&self) -> Result<T> {
        let mut result = None;
        // SAFETY: `result` is valid for writing an interface pointer and it is safe
        // to cast the `result` pointer as `T` on success because we are using the `IID` tied
        // to `T` which the implementor of `ComInterface` has guaranteed is correct
        unsafe { self.query(&T::IID, &mut result as *mut _ as _).and_some(result) }
    }

    /// Attempts to create a [`Weak`] reference to this object.
    fn downgrade(&self) -> Result<Weak<Self>> {
        self.cast::<crate::imp::IWeakReferenceSource>().and_then(|source| Weak::downgrade(&source))
    }

    /// Call `QueryInterface` on this interface
    ///
    /// # Safety
    ///
    /// `interface` must be a non-null, valid pointer for writing an interface pointer.
    unsafe fn query(&self, iid: *const GUID, interface: *mut *mut std::ffi::c_void) -> HRESULT {
        (self.assume_vtable::<IUnknown>().QueryInterface)(self.as_raw(), iid, interface)
    }
}
