use core::mem::ManuallyDrop;

#[repr(C)]
pub(crate) union PtrToMut<'a, P: ?Sized> {
    pub(crate) ptr: *mut P,
    pub(crate) mutt: ManuallyDrop<&'a mut P>,
}

macro_rules! __priv_transmute_mut {
    ($from:ty, $to:ty, $reference:expr) => {
        match $reference {
            ptr => {
                let ptr: *mut $from = ptr;
                core::mem::ManuallyDrop::into_inner(
                    crate::utils_mut::PtrToMut::<$to> {
                        ptr: ptr as *mut $to,
                    }
                    .mutt,
                )
            }
        }
    };
}
pub(crate) use __priv_transmute_mut;
