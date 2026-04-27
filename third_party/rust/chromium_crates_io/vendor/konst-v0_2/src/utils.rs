#[cfg(feature = "mut_refs")]
mod mut_refs {
    use core::mem::ManuallyDrop;

    #[repr(C)]
    #[doc(hidden)]
    pub(crate) union BorrowMut<'a, T: ?Sized> {
        ptr: *mut T,
        reff: ManuallyDrop<&'a mut T>,
    }

    pub(crate) const unsafe fn deref_raw_mut_ptr<'a, T: ?Sized>(ptr: *mut T) -> &'a mut T {
        ManuallyDrop::into_inner(BorrowMut { ptr }.reff)
    }

    pub(crate) const unsafe fn slice_from_raw_parts_mut<'a, T>(
        ptr: *mut T,
        len: usize,
    ) -> &'a mut [T] {
        let ptr = core::ptr::slice_from_raw_parts_mut(ptr, len);
        ManuallyDrop::into_inner(BorrowMut { ptr }.reff)
    }
}


#[cfg(feature = "rust_1_56")]
#[repr(C)]
pub(crate) union __TransmuteCopy<T: Copy, U: Copy> {
    pub(crate) from: T,
    pub(crate) to: U,
}

#[doc(hidden)]
#[cfg(feature = "mut_refs")]
pub(crate) use mut_refs::{deref_raw_mut_ptr, slice_from_raw_parts_mut, BorrowMut};

#[doc(hidden)]
#[cfg(feature = "rust_1_64")]
pub(crate) const unsafe fn slice_from_raw_parts<'a, T>(ptr: *const T, len: usize) -> &'a [T] {
    let ptr = core::ptr::slice_from_raw_parts(ptr, len);
    crate::utils_1_56::PtrToRef { ptr }.reff
}

#[allow(dead_code)]
#[inline]
pub(crate) const fn saturating_sub(l: usize, r: usize) -> usize {
    let (sub, overflowed) = l.overflowing_sub(r);
    if overflowed {
        0
    } else {
        sub
    }
}

#[inline]
#[cfg(feature = "rust_1_64")]
#[allow(dead_code)]
pub(crate) const fn min_usize(l: usize, r: usize) -> usize {
    if l < r {
        l
    } else {
        r
    }
}
