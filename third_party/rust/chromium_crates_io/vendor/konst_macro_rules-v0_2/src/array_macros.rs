use core::mem::MaybeUninit;

#[macro_export]
macro_rules! array_map {
    ($array:expr, |$param:tt $(: $type:ty)?| $mapper:expr $(,)? ) => (
        match $array {
            ref array => {
                let array = $crate::__::assert_array(array);
                let len = array.len();
                let mut out = $crate::__::uninit_array(array);

                let mut i = 0;
                while i < len {
                    let $param $(: $type)? = array[i];
                    out[i] = $crate::__::MaybeUninit::new($mapper);
                    i += 1;
                }

                unsafe{
                    $crate::__::AssumInitCopyArray{uninit: out}.init
                }
            }
        }
    );
    ($array:expr, | $($anything:tt)* ) => {
        compile_error!("expected the closure to take an argument")
    };
    ($array:expr, $function:expr $(,)?) => {
        $crate::array_map!($array, |x| $function(x))
    };
}

#[inline(always)]
pub const fn assert_array<T, const N: usize>(array: &[T; N]) -> &[T; N] {
    array
}

struct UNINIT<T>(T);

impl<T> UNINIT<T> {
    pub const V: MaybeUninit<T> = MaybeUninit::uninit();
}

#[repr(C)]
pub union AssumInitCopyArray<T: Copy, const N: usize> {
    pub uninit: [MaybeUninit<T>; N],
    pub init: [T; N],
}

#[inline(always)]
pub const fn uninit_array<T, U, const N: usize>(_input: &[T; N]) -> [MaybeUninit<U>; N] {
    [UNINIT::V; N]
}
