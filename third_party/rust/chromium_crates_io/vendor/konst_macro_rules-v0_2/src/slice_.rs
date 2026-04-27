#![allow(non_camel_case_types)]

use core::{
    fmt::{self, Display},
    marker::PhantomData,
};

#[macro_export]
macro_rules! try_into_array {
    ($slice:expr, $len:expr$(,)*) => {
        match $slice {
            (_x) => unsafe { $crate::__priv_try_into_array!(explicit, _x, $len) },
        }
    };
    ($slice:expr $(,)*) => {
        match $slice {
            (_x) => unsafe { $crate::__priv_try_into_array!(infer, _x) },
        }
    };
}

#[macro_export]
#[cfg(not(feature = "rust_1_51"))]
macro_rules! __priv_try_into_array {
    // This implementation is used when const generics are disabled.
    (explicit, $slice:ident, $len:expr) => {{
        const __LEN: usize = $len;

        let slice = $crate::slice_::__priv_SliceLifetime($slice, $crate::slice_::Phantom::NEW);

        type __Constrainer<'a, T> = $crate::slice_::__priv_TypeLifetime<'a, [T; __LEN], T>;

        if slice.0.len() == __LEN {
            let ptr = slice.0.as_ptr() as *const [_; __LEN];

            let ret = __Constrainer {
                array: $crate::utils::Dereference { ptr }.reff,
                phantom: slice.1,
            };

            $crate::__::Ok(ret.array)
        } else {
            $crate::__::Err($crate::slice_::TryIntoArrayError::__priv__new())
        }
    }};
    (infer, $slice:ident) => {
        $crate::__::compile_error!(concat!(
            "\
                To infer the length of the returned array,\n\
                you must enable the  \"rust_1_51\" feature (which requires Rust 1.51.0).\n\
                \n\
                Otherwise you need to pass the length explicitly, \
                eg: try_into_array!(foo, 10)"
        ))
    };
}

#[macro_export]
#[cfg(feature = "rust_1_51")]
macro_rules! __priv_try_into_array {
    // This implementation is used when const generics are enabled,
    // and should work with arrays like ARR in
    // ```
    // impl Foo<T: Trait> {
    //      const ARR: &'static [u32; T::CONST] = try_into_array!(...);
    // }
    // ```
    // whenever array types like that are allowed.
    (explicit, $slice:ident, $len:expr) => {{
        let slice = $crate::slice_::__priv_SliceLifetime($slice, $crate::slice_::Phantom::NEW);
        let plen = $crate::slice_::PhantomUsize::<{ $len }>;

        $crate::__priv_try_into_array! {inner, slice, plen}
    }};
    (infer, $slice:ident) => {
        loop {
            let slice = $crate::slice_::__priv_SliceLifetime($slice, $crate::slice_::Phantom::NEW);
            let plen = $crate::slice_::PhantomUsize;

            if false {
                break $crate::slice_::get_length(plen);
            }

            break $crate::__priv_try_into_array! {inner, slice, plen};
        }
    };
    (inner, $slice:ident, $len:ident) => {
        if let (true, ptr) = $crate::slice_::check_length($slice.0, $len) {
            let array = $crate::utils::Dereference { ptr }.reff;

            $crate::__::Ok($crate::slice_::__priv_ArrayLifetime(array, $slice.1).0)
        } else {
            $crate::__::Err($crate::slice_::TryIntoArrayError::__priv__new())
        }
    };
}

pub struct Phantom<'a, T>(PhantomData<*mut *mut &'a [T]>);

impl<'a, T: 'a> Phantom<'a, T> {
    pub const NEW: Self = Self(PhantomData);
}

#[repr(transparent)]
pub struct __priv_TypeLifetime<'a, T, U> {
    pub array: &'a T,
    pub phantom: Phantom<'a, U>,
}

#[repr(transparent)]
pub struct __priv_SliceLifetime<'a, T>(pub &'a [T], pub Phantom<'a, T>);

#[cfg(feature = "rust_1_51")]
#[derive(Copy, Clone)]
pub struct PhantomUsize<const N: usize>;

#[cfg(feature = "rust_1_51")]
pub const fn get_length<'a, T, const N: usize>(
    _: PhantomUsize<N>,
) -> Result<&'a [T; N], TryIntoArrayError> {
    loop {}
}

#[inline(always)]
#[cfg(feature = "rust_1_51")]
pub const fn check_length<T, const N: usize>(
    slice: &[T],
    _len: PhantomUsize<N>,
) -> (bool, *const [T; N]) {
    (N == slice.len(), slice.as_ptr() as *const [T; N])
}

#[repr(transparent)]
#[cfg(feature = "rust_1_51")]
pub struct __priv_ArrayLifetime<'a, T, const N: usize>(pub &'a [T; N], pub Phantom<'a, T>);

////////////////////////////////////////////////////////////////////////////////

#[cfg(feature = "rust_1_56")]
#[inline]
pub const fn try_into_array_func<T, const N: usize>(
    slice: &[T],
) -> Result<&[T; N], TryIntoArrayError> {
    if slice.len() == N {
        let ptr = slice.as_ptr() as *const [T; N];
        unsafe { Ok(crate::utils::Dereference { ptr }.reff) }
    } else {
        Err(TryIntoArrayError { _priv: () })
    }
}

////////////////////////////////////////////////////////////////////////////////

#[cfg(feature = "mut_refs")]
#[inline]
pub const fn try_into_array_mut_func<T, const N: usize>(
    slice: &mut [T],
) -> Result<&mut [T; N], TryIntoArrayError> {
    if slice.len() == N {
        let ptr = slice as *mut [T] as *mut [T; N];
        unsafe { Ok(crate::utils::deref_raw_mut_ptr(ptr)) }
    } else {
        Err(TryIntoArrayError { _priv: () })
    }
}

////////////////////////////////////////////////////////////////////////////////

#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub struct TryIntoArrayError {
    _priv: (),
}

impl TryIntoArrayError {
    #[allow(non_snake_case)]
    #[doc(hidden)]
    #[inline]
    pub const fn __priv__new() -> Self {
        TryIntoArrayError { _priv: () }
    }

    /// For erroring with an error message.
    pub const fn panic(&self) -> ! {
        let offset = self.number();
        [/*Could not cast &[T] to &[T; N]*/][offset]
    }

    const fn number(&self) -> usize {
        0
    }
}

impl Display for TryIntoArrayError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("Could not cast slice to array reference")
    }
}
