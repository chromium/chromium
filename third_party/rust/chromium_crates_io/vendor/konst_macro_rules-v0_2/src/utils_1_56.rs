use core::mem::{ManuallyDrop, MaybeUninit};

#[repr(C)]
pub union Transmuter<F, T> {
    pub from: ManuallyDrop<F>,
    pub to: ManuallyDrop<T>,
}

#[repr(C)]
pub union PtrToRef<'a, P: ?Sized> {
    pub ptr: *const P,
    pub reff: &'a P,
}

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_transmute {
    ($from:ty, $to:ty, $value:expr) => {{
        $crate::__::ManuallyDrop::into_inner(
            $crate::utils_1_56::Transmuter::<$from, $to> {
                from: $crate::__::ManuallyDrop::new($value),
            }
            .to,
        )
    }};
}

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_transmute_ref {
    ($from:ty, $to:ty, $reference:expr) => {
        match $reference {
            ptr => {
                let ptr: *const $from = ptr;
                $crate::utils_1_56::PtrToRef::<$to> {
                    ptr: ptr as *const $to,
                }
                .reff
            }
        }
    };
}

#[inline(always)]
pub const fn uninit_array<T, const LEN: usize>() -> [MaybeUninit<T>; LEN] {
    union MakeMUArray<T, const LEN: usize> {
        unit: (),
        array: ManuallyDrop<[MaybeUninit<T>; LEN]>,
    }

    unsafe { ManuallyDrop::into_inner(MakeMUArray { unit: () }.array) }
}

#[inline(always)]
pub const unsafe fn array_assume_init<T, const N: usize>(md: [MaybeUninit<T>; N]) -> [T; N] {
    crate::__priv_transmute! {[MaybeUninit<T>; N], [T; N], md}
}
