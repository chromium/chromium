use crate::{
    fmt::FmtKind,
    panic_val::{PanicClass, PanicVal, StrFmt},
    utils::{bytes_up_to, string_cap, WasTruncated},
};

/// Panics by concatenating the argument slice.
///
/// This is the function that the [`concat_panic`](macro@concat_panic) macro calls to panic.
///
/// # Example
///
/// Here's how to panic with formatting without using any macros:
///
/// ```compile_fail
/// use const_panic::{FmtArg, PanicVal, concat_panic};
///
/// const _: () = concat_panic(&[&[
///     PanicVal::write_str("\nthe error was "),
///     PanicVal::from_u8(100, FmtArg::DISPLAY),
///     PanicVal::write_str(" and "),
///     PanicVal::from_str("\nHello\tworld", FmtArg::DEBUG),
/// ]]);
///
///
/// ```
/// That fails to compile with this error message:
/// ```text
/// error[E0080]: evaluation of constant value failed
///   --> src/concat_panic_.rs:13:15
///    |
/// 6  |   const _: () = concat_panic(&[&[
///    |  _______________^
/// 7  | |     PanicVal::write_str("\nthe error was "),
/// 8  | |     PanicVal::from_u8(100, FmtArg::DISPLAY),
/// 9  | |     PanicVal::write_str(" and "),
/// 10 | |     PanicVal::from_str("\nHello\tworld", FmtArg::DEBUG),
/// 11 | | ]]);
///    | |___^ the evaluated program panicked at '
/// the error was 100 and "\nHello\tworld"', src/concat_panic_.rs:6:15
/// ```
///
#[cold]
#[inline(never)]
#[track_caller]
pub const fn concat_panic(args: &[&[PanicVal<'_>]]) -> ! {
    // The panic message capacity starts small and gets larger each time,
    // so that platforms with smaller stacks can call this at runtime.
    //
    // Also, given that most(?) panic messages are smaller than 1024 bytes long,
    // it's not going to be any less efficient in the common case.
    if let Err(_) = panic_inner::<(), 1024>(args) {}

    if let Err(_) = panic_inner::<(), { 1024 * 6 }>(args) {}

    match panic_inner::<_, MAX_PANIC_MSG_LEN>(args) {
        Ok(x) => x,
        Err(_) => panic!(
            "\
            unreachable:\n\
            the `write_panicval_to_buffer` macro must not return Err when \
            $capacity == $max_capacity\
        "
        ),
    }
}

/// The maximum length of panic messages (in bytes),
/// after which the message is truncated.
// this should probably be smaller on platforms where this
// const fn is called at runtime, and the stack is finy.
pub const MAX_PANIC_MSG_LEN: usize = 32768;

// writes a single PanicVal to an array
macro_rules! write_panicval {
    (
        $outer_label:lifetime,
        $mout:ident, $lout:ident, $tct:expr,
        (
            $len:expr,
            $capacity:expr,
            $max_capacity:expr,
            $not_enough_space:expr,
            $write_buffer:ident,
            $write_buffer_checked:ident,
        )
    ) => {
        let rem_space = $capacity - $len;
        let (strfmt, class, was_truncated) = $tct;
        let StrFmt {
            leftpad: mut lpad,
            rightpad: mut rpad,
            fmt_kind,
        } = strfmt;

        let ranged = match class {
            PanicClass::PreFmt(str) => str,
            PanicClass::Int(int) => {
                if int.len() <= string_cap::MEDIUM {
                    $mout = int.fmt::<{ string_cap::MEDIUM }>();
                    $mout.ranged()
                } else {
                    $lout = int.fmt::<{ string_cap::LARGE }>();
                    $lout.ranged()
                }
            }
            #[cfg(feature = "non_basic")]
            PanicClass::Slice(_) => unreachable!(),
        };

        let trunc_end = ranged.start + was_truncated.get_length(ranged.len());

        while lpad != 0 {
            $write_buffer! {b' '}
            lpad -= 1;
        }

        if let FmtKind::Display = fmt_kind {
            let mut i = ranged.start;
            while i < trunc_end {
                $write_buffer! {ranged.bytes[i]}
                i += 1;
            }
        } else if rem_space != 0 {
            $write_buffer! {b'"'}
            let mut i = 0;
            while i < trunc_end {
                use crate::debug_str_fmt::{hex_as_ascii, ForEscaping};

                let c = ranged.bytes[i];
                let mut written_c = c;
                if ForEscaping::is_escaped(c) {
                    $write_buffer! {b'\\'}
                    if ForEscaping::is_backslash_escaped(c) {
                        written_c = ForEscaping::get_backslash_escape(c);
                    } else {
                        $write_buffer! {b'x'}
                        $write_buffer! {hex_as_ascii(c >> 4)}
                        written_c = hex_as_ascii(c & 0b1111);
                    };
                }
                $write_buffer! {written_c}

                i += 1;
            }
            if let WasTruncated::No = was_truncated {
                $write_buffer_checked! {b'"'}
            }
        }

        while rpad != 0 {
            $write_buffer! {b' '}
            rpad -= 1;
        }

        if let WasTruncated::Yes(_) = was_truncated {
            if $capacity < $max_capacity {
                return $not_enough_space;
            } else {
                break $outer_label;
            }
        }
    };
}

macro_rules! write_to_buffer_inner {
    (
        $args:ident
        (
            $len:expr,
            $capacity:expr,
            $($_rem:tt)*
        )
        $wptb_args:tt
    ) => {
        let mut args = $args;

        let mut mout;
        let mut lout;

        'outer: while let [mut outer, ref nargs @ ..] = args {
            while let [arg, nouter @ ..] = outer {
                let tct = arg.to_class_truncated($capacity - $len);
                match tct.1 {
                    #[cfg(feature = "non_basic")]
                    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
                    PanicClass::Slice(slice) => {
                        let mut iter = slice.iter();

                        'iter: loop {
                            let (two_args, niter) = iter.next();

                            let mut two_args: &[_] = &two_args;
                            while let [arg, ntwo_args @ ..] = two_args {
                                let tct = arg.to_class_truncated($capacity - $len);
                                write_panicval! {'outer, mout, lout, tct, $wptb_args}
                                two_args = ntwo_args;
                            }

                            match niter {
                                Some(x) => iter = x,
                                None => break 'iter,
                            }
                        }
                    }
                    _ => {
                        write_panicval! {'outer, mout, lout, tct, $wptb_args}
                    }
                }

                outer = nouter;
            }
            args = nargs;
        }
    };
}

macro_rules! write_to_buffer {
    ($args:ident $wptb_args:tt) => {
        write_to_buffer_inner! {
            $args
            $wptb_args
            $wptb_args
        }
    };
}

macro_rules! make_buffer_writer_macros {
    ($buffer:ident, $len:ident) => {
        macro_rules! write_buffer {
            ($value:expr) => {
                __write_array! {$buffer, $len, $value}
            };
        }
        macro_rules! write_buffer_checked {
            ($value:expr) => {
                __write_array_checked! {$buffer, $len, $value}
            };
        }
    };
}

#[cold]
#[inline(never)]
#[track_caller]
const fn panic_inner<T, const LEN: usize>(args: &[&[PanicVal<'_>]]) -> Result<T, NotEnoughSpace> {
    let mut buffer = [0u8; LEN];
    let mut len = 0usize;

    make_buffer_writer_macros! {buffer, len}

    write_to_buffer! {
        args
        (
            len, LEN, MAX_PANIC_MSG_LEN, Err(NotEnoughSpace),
            write_buffer, write_buffer_checked,
        )
    }

    unsafe {
        let buffer = bytes_up_to(&buffer, len);
        let str = core::str::from_utf8_unchecked(buffer);
        panic!("{}", str)
    }
}

#[doc(hidden)]
#[derive(Debug)]
pub struct NotEnoughSpace;

#[cfg(feature = "test")]
use crate::test_utils::TestString;

#[doc(hidden)]
#[cfg(feature = "test")]
pub fn format_panic_message<const LEN: usize>(
    args: &[&[PanicVal<'_>]],
    capacity: usize,
    max_capacity: usize,
) -> Result<TestString<LEN>, NotEnoughSpace> {
    let mut buffer = [0u8; LEN];
    let mut len = 0usize;
    {
        // intentionally shadowed
        let buffer = &mut buffer[..capacity];

        make_buffer_writer_macros! {buffer, len}

        write_to_buffer! {
            args
            (
                len, capacity, max_capacity, Err(NotEnoughSpace),
                write_buffer, write_buffer_checked,
            )
        }
    }

    Ok(TestString { buffer, len })
}

#[cfg(feature = "non_basic")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
#[doc(hidden)]
pub(crate) const fn make_panic_string<const LEN: usize>(
    args: &[&[PanicVal<'_>]],
) -> Result<crate::ArrayString<LEN>, NotEnoughSpace> {
    let mut buffer = [0u8; LEN];
    let mut len = 0usize;

    make_buffer_writer_macros! {buffer, len}

    write_to_buffer! {
        args
        (len, LEN, LEN + 1, Err(NotEnoughSpace), write_buffer, write_buffer_checked,)
    }

    assert!(len as u32 as usize == len, "the panic message is too large");

    Ok(crate::ArrayString {
        buffer,
        len: len as u32,
    })
}

#[cfg(feature = "non_basic")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
#[doc(hidden)]
#[track_caller]
pub const fn make_panic_string_unwrapped<const LEN: usize>(
    args: &[&[PanicVal<'_>]],
) -> crate::ArrayString<LEN> {
    match make_panic_string(args) {
        Ok(x) => x,
        Err(_) => panic!("arguments are too large to fit in LEN"),
    }
}

#[cfg(feature = "non_basic")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
#[doc(hidden)]
pub const fn compute_length(args: &[&[PanicVal<'_>]]) -> usize {
    let mut len = 0usize;

    macro_rules! add_to_len {
        ($value:expr) => {{
            let _: u8 = $value;
            len += 1;
        }};
    }

    write_to_buffer! {
        args
        (
            len, usize::MAX - 1, usize::MAX, usize::MAX,
            add_to_len, add_to_len,
        )
    }

    len
}
