use crate::type_eq::TypeEq;

pub enum CollectorCmd<T, Ret, const CAP: usize> {
    ComputeLength(TypeEq<ComputedLength, Ret>),
    BuildArray(TypeEq<[T; CAP], Ret>),
}

impl<T> CollectorCmd<T, ComputedLength, 0> {
    pub const COMPUTE_LENGTH: Self = Self::ComputeLength(TypeEq::NEW);
}

impl<T, const CAP: usize> CollectorCmd<T, [T; CAP], CAP> {
    pub const BUILD_ARRAY: Self = Self::BuildArray(TypeEq::NEW);
}

pub struct ComputedLength {
    pub length: usize,
}

#[macro_export]
macro_rules! iter_collect_const {
    ($Item:ty => $($rem:tt)*) => {{
        const fn __func_zxe7hgbnjs<Ret_KO9Y329U2U, const CAP_KO9Y329U2U: usize>(
            cmd: $crate::__::CollectorCmd<$Item, Ret_KO9Y329U2U, CAP_KO9Y329U2U>,
        ) -> Ret_KO9Y329U2U {
            let mut array = $crate::utils_1_56::uninit_array::<_, CAP_KO9Y329U2U>();
            let mut length = 0usize;

            $crate::__process_iter_args!{
                ($crate::__iter_collect_const)
                ($Item, (cmd array length),)
                (
                    item,
                    'zxe7hgbnjs,
                    adapter,
                )
                $($rem)*
            }

            match cmd {
                $crate::__::CollectorCmd::ComputeLength(teq) => {
                    teq.to_right($crate::__::ComputedLength { length })
                }
                $crate::__::CollectorCmd::BuildArray(teq) => {
                    if length == CAP_KO9Y329U2U {
                        // SAFETY: The above condition ensures that
                        // all of the array is initialized
                        let array = unsafe{ $crate::utils_1_56::array_assume_init(array) };
                        teq.to_right(array)
                    } else {
                        let _: () = [/*initialization was skipped somehow*/][length];
                        loop{}
                    }
                }
            }
        }

        const __COUNT81608BFNA5: $crate::__::usize =
            __func_zxe7hgbnjs($crate::__::CollectorCmd::COMPUTE_LENGTH).length;

        const __ARR81608BFNA5: [$Item; __COUNT81608BFNA5] =
            __func_zxe7hgbnjs($crate::__::CollectorCmd::BUILD_ARRAY);

        __ARR81608BFNA5
    }};
}

#[doc(hidden)]
#[macro_export]
macro_rules! __iter_collect_const {
    (
        @each
        $Item:ty,
        ($cmd:ident $array:ident $length:ident),
        ($item:ident adapter),
        $(,)*
    ) => {
        if let $crate::__::CollectorCmd::BuildArray(teq) = $cmd {
            teq.reachability_hint();

            $array[$length] = $crate::__::MaybeUninit::new($item);
        }

        $length += 1;
    };
    (@end $($tt:tt)*) => {};
}
