use crate::ArrayString;

use typewit::{MakeTypeWitness, TypeEq, TypeWitnessTypeArg};

/// Concatenates [`PanicFmt`] constants into a `&'static str`
///
/// This formats arguments the same as the [`concat_panic`] macro,
/// also requiring the arguments to be constant expressions.
///
/// # Example
///
/// ### Basic
///
/// ```rust
/// use const_panic::concat_;
///
/// assert_eq!(concat_!("array: ", &[3u8, 5, 8, 13]), "array: [3, 5, 8, 13]");
///
/// ```
///
/// ### Formatted
///
/// ```rust
/// use const_panic::concat_;
///
/// assert_eq!(concat_!({?}: get_message(), {}: get_message()), r#""hello"hello"#);
///
/// const fn get_message() -> &'static str {
///     "hello"
/// }
///
/// ```
///
/// [`PanicFmt`]: crate::fmt::PanicFmt
/// [`concat_panic`]: macro@crate::concat_panic
///
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
#[macro_export]
macro_rules! concat_ {
    () => ("");
    ($($args:tt)*) => ({
        const fn __func_zxe7hgbnjs<Ret_ZXE7HGBNJS, const CAP_ZXE7HGBNJS: $crate::__::usize>(
            cmd: $crate::__::ConcatCmd<Ret_ZXE7HGBNJS, CAP_ZXE7HGBNJS>
        ) -> Ret_ZXE7HGBNJS {
            $crate::__concat_func_setup!{
                (|args| {
                    match cmd {
                        $crate::__::ConcatCmd::ComputeLength(te) =>
                            te.to_left($crate::__::compute_length(args)),
                        $crate::__::ConcatCmd::BuildArray(te) =>
                            te.to_left($crate::__::make_panic_string_unwrapped(args)),
                    }
                })
                []
                [$($args)*,]
            }
        }

        const LEN_ZXE7HGBNJS: $crate::__::usize = __func_zxe7hgbnjs($crate::__::MakeTypeWitness::MAKE);

        const AS_ZXE7HGBNJS: $crate::ArrayString<LEN_ZXE7HGBNJS> =
            __func_zxe7hgbnjs($crate::__::MakeTypeWitness::MAKE);

        const S_ZXE7HGBNJS: &$crate::__::str = AS_ZXE7HGBNJS.to_str();

        S_ZXE7HGBNJS
    })
}

#[doc(hidden)]
pub enum ConcatCmd<Ret, const CAP: usize> {
    ComputeLength(TypeEq<Ret, usize>),
    BuildArray(TypeEq<Ret, ArrayString<CAP>>),
}

impl<Ret, const CAP: usize> TypeWitnessTypeArg for ConcatCmd<Ret, CAP> {
    type Arg = Ret;
}

impl MakeTypeWitness for ConcatCmd<usize, 0> {
    const MAKE: Self = Self::ComputeLength(TypeEq::NEW);
}

impl<const CAP: usize> MakeTypeWitness for ConcatCmd<ArrayString<CAP>, CAP> {
    const MAKE: Self = Self::BuildArray(TypeEq::NEW);
}
