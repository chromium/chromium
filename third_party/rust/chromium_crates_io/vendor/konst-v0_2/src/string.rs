//! `const fn` equivalents of `str` methods.

#[cfg(feature = "rust_1_64")]
mod splitting;

#[cfg(feature = "rust_1_64")]
pub use splitting::*;

#[cfg(feature = "rust_1_64")]
mod split_terminator_items;

#[cfg(feature = "rust_1_64")]
pub use split_terminator_items::*;

__declare_string_cmp_fns! {
    import_path = "konst",
    equality_fn = eq_str,
    ordering_fn = cmp_str,
    ordering_fn_inner = cmp_str_inner,
}

#[cfg(feature = "cmp")]
__declare_fns_with_docs! {
    (Option<&'a str>, (eq_option_str, cmp_option_str))

    docs(default)

    macro = __impl_option_cmp_fns!(
        #[cfg_attr(feature = "docsrs", doc(cfg(feature = "cmp")))]
        for['a,]
        params(l, r)
        eq_comparison = crate::polymorphism::CmpWrapper(l).const_eq(r),
        cmp_comparison = crate::polymorphism::CmpWrapper(l).const_cmp(r),
        parameter_copyability = copy,
    ),
}

/// Reexports for `0.2.*` patch releases, will be removed in `0.3.0`
#[deprecated(
    since = "0.2.10",
    note = "reexports for `0.2.*` patch releases, will be removed in `0.3.0`"
)]
pub mod deprecated_reexports {
    macro_rules! declare_deprecated {
        (
            $deprecation:literal
            fn $fn_name:ident($($arg:ident : $arg_ty:ty),*) -> $ret:ty {
                $delegating_to:ident
            }
        ) => {
            #[deprecated(
                since = "0.2.10",
                note = $deprecation,
            )]
            #[doc = $deprecation]
            #[inline(always)]
            pub const fn $fn_name($($arg: $arg_ty,)*) -> $ret {
                super::$delegating_to($($arg),*)
            }
        };
    }

    declare_deprecated! {
        "renamed to `starts_with`, full path: `konst::string::starts_with`"
        fn str_starts_with(left: &str, right: &str) -> bool {
            starts_with
        }
    }

    declare_deprecated! {
        "renamed to `ends_with`, full path: `konst::string::ends_with`"
        fn str_ends_with(left: &str, right: &str) -> bool {
            ends_with
        }
    }

    declare_deprecated! {
        "renamed to `find`, full path: `konst::string::find`"
        fn str_find(left: &str, right: &str, from: usize) -> Option<usize> {
            find
        }
    }

    declare_deprecated! {
        "renamed to `contains`, full path: `konst::string::contains`"
        fn str_contains(left: &str, right: &str, from: usize) -> bool {
            contains
        }
    }

    declare_deprecated! {
        "renamed to `rfind`, full path: `konst::string::rfind`"
        fn str_rfind(left: &str, right: &str, from: usize) -> Option<usize> {
            rfind
        }
    }

    declare_deprecated! {
         "renamed to `rcontains`, full path: `konst::string::rcontains`"
        fn str_rcontains(left: &str, right: &str, from: usize) -> bool {
            rcontains
        }
    }
}

#[allow(deprecated)]
pub use deprecated_reexports::*;

#[doc(hidden)]
pub use konst_macro_rules::string::check_utf8 as __priv_check_utf8;

/// A const equivalent of [`std::str::from_utf8`],
/// usable *only in `const`s and `static`s.
///
/// \* This can be only used in `const fn`s when the
/// `"rust_1_55"` feature is enabled.
///
/// For an equivalent function, which requires Rust 1.55.0
/// (while this macro only requires Rust 1.46.0) and the `"rust_1_55"` crate feature,
/// there is the [`from_utf8` function].
///
/// # Example
///
/// ```rust
/// use konst::{string, unwrap_ctx};
///
/// const OK: &str = unwrap_ctx!(string::from_utf8!(b"foo bar"));
/// assert_eq!(OK, "foo bar");
///
/// const ERR: Result<&str, string::Utf8Error> = string::from_utf8!(b"what\xFA");
/// assert_eq!(ERR.unwrap_err().valid_up_to(), 4);
///
/// ```
///
/// [`std::str::from_utf8`]: https://doc.rust-lang.org/std/str/fn.from_utf8.html
/// [`from_utf8` function]: ./fn.from_utf8.html
pub use konst_macro_rules::from_utf8_macro as from_utf8;

/// A const equivalent of [`std::str::from_utf8`],
/// requires Rust 1.55 and the `"rust_1_55"` feature.
///
/// For an alternative that works in Rust 1.46.0,
/// there is the [`from_utf8`](./macro.from_utf8.html) macro,
/// but it can only be used in `const`s, not in `const fn`s .
///
/// # Example
///
/// ```rust
/// use konst::{string, unwrap_ctx};
///
/// const OK: &str = unwrap_ctx!(string::from_utf8(b"hello world"));
/// assert_eq!(OK, "hello world");
///
/// const ERR: Result<&str, string::Utf8Error> = string::from_utf8(&[32, 34, 255]);
/// assert_eq!(ERR.unwrap_err().valid_up_to(), 2);
///
/// ```
///
/// [`std::str::from_utf8`]: https://doc.rust-lang.org/std/str/fn.from_utf8.html
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub use konst_macro_rules::string::from_utf8_fn as from_utf8;

/// Error returned by the `from_utf8` [function](fn.from_utf8.html) and
/// [macro](macro.from_utf8.html) when the
/// input byte slice isn't valid utf8.
pub use konst_macro_rules::string::Utf8Error;

/// A const equivalent of
/// [`str::starts_with`](https://doc.rust-lang.org/std/primitive.str.html#method.starts_with)
/// , taking a `&str` parameter.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// assert!( string::starts_with("foo,bar,baz", "foo,"));
///
/// assert!(!string::starts_with("foo,bar,baz", "bar"));
/// assert!(!string::starts_with("foo,bar,baz", "baz"));
///
/// ```
///
#[inline(always)]
pub const fn starts_with(left: &str, right: &str) -> bool {
    crate::slice::bytes_start_with(left.as_bytes(), right.as_bytes())
}

/// A const equivalent of
/// [`str::ends_with`](https://doc.rust-lang.org/std/primitive.str.html#method.ends_with)
/// , taking a `&str` parameter.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// assert!( string::ends_with("foo,bar,baz", ",baz"));
///
/// assert!(!string::ends_with("foo,bar,baz", "bar"));
/// assert!(!string::ends_with("foo,bar,baz", "foo"));
///
/// ```
///
#[inline(always)]
pub const fn ends_with(left: &str, right: &str) -> bool {
    crate::slice::bytes_end_with(left.as_bytes(), right.as_bytes())
}

/// A const equivalent of
/// [`str::find`](https://doc.rust-lang.org/std/primitive.str.html#method.find)
/// , taking a `&str` parameter, searching in `&left[from..]`.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// assert_eq!(string::find("foo-bar-baz-foo", "foo", 0), Some(0));
/// assert_eq!(string::find("foo-bar-baz-foo", "foo", 4), Some(12));
///
/// assert_eq!(string::find("foo-bar-baz-foo-bar", "bar", 0), Some(4));
/// assert_eq!(string::find("foo-bar-baz-foo-bar", "bar", 4), Some(4));
/// assert_eq!(string::find("foo-bar-baz-foo-bar", "bar", 5), Some(16));
/// assert_eq!(string::find("foo-bar-baz-foo-bar", "bar", 16), Some(16));
/// assert_eq!(string::find("foo-bar-baz-foo-bar", "bar", 17), None);
///
/// ```
///
#[inline]
pub const fn find(left: &str, right: &str, from: usize) -> Option<usize> {
    crate::slice::bytes_find(left.as_bytes(), right.as_bytes(), from)
}

/// A const equivalent of
/// [`str::contains`](https://doc.rust-lang.org/std/primitive.str.html#method.contains)
/// , taking a `&str` parameter, searching in `&left[from..]`.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// assert!(string::contains("foo-bar-baz-foo", "foo", 0));
/// assert!(string::contains("foo-bar-baz-foo", "foo", 4));
///
/// assert!( string::contains("foo-bar-baz-foo-bar", "bar", 0));
/// assert!( string::contains("foo-bar-baz-foo-bar", "bar", 4));
/// assert!( string::contains("foo-bar-baz-foo-bar", "bar", 5));
/// assert!( string::contains("foo-bar-baz-foo-bar", "bar", 16));
/// assert!(!string::contains("foo-bar-baz-foo-bar", "bar", 17));
///
/// ```
///
#[inline(always)]
pub const fn contains(left: &str, right: &str, from: usize) -> bool {
    matches!(
        crate::slice::bytes_find(left.as_bytes(), right.as_bytes(), from),
        Some(_)
    )
}

/// A const equivalent of
/// [`str::rfind`](https://doc.rust-lang.org/std/primitive.str.html#method.rfind)
/// , taking a `&str` parameter, searching in `&left[..=from]`.
///
/// You can pass `usize::MAX` as the `from` argument to search from the end of `left`
/// regardless of its length.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// assert_eq!(string::rfind("foo-bar-baz-foo", "foo", 0), None);
/// assert_eq!(string::rfind("foo-bar-baz-foo", "foo", 1), None);
///
/// assert_eq!(string::rfind("foo-bar-baz-foo", "foo", 2), Some(0));
/// assert_eq!(string::rfind("foo-bar-baz-foo", "foo", 3), Some(0));
/// assert_eq!(string::rfind("foo-bar-baz-foo", "foo", 4), Some(0));
///
/// assert_eq!(string::rfind("foo-bar-baz-foo", "foo", 15), Some(12));
/// assert_eq!(string::rfind("foo-bar-baz-foo", "foo", 20000), Some(12));
///
/// ```
///
#[inline]
pub const fn rfind(left: &str, right: &str, from: usize) -> Option<usize> {
    crate::slice::bytes_rfind(left.as_bytes(), right.as_bytes(), from)
}

/// A const equivalent of
/// [`str::contains`](https://doc.rust-lang.org/std/primitive.str.html#method.contains)
/// , taking a `&str` parameter, searching in `&left[..=from]` from the end.
///
/// You can pass `usize::MAX` as the `from` argument to search from the end of `left`
/// regardless of its length.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// assert!(!string::rcontains("foo-bar-baz-foo", "foo", 0));
/// assert!(!string::rcontains("foo-bar-baz-foo", "foo", 1));
///
/// assert!(string::rcontains("foo-bar-baz-foo", "foo", 2));
/// assert!(string::rcontains("foo-bar-baz-foo", "foo", 3));
/// assert!(string::rcontains("foo-bar-baz-foo", "foo", 4));
///
/// assert!(string::rcontains("foo-bar-baz-foo", "foo", 15));
/// assert!(string::rcontains("foo-bar-baz-foo", "foo", 20000));
///
/// ```
///
#[inline(always)]
pub const fn rcontains(left: &str, right: &str, from: usize) -> bool {
    matches!(
        crate::slice::bytes_rfind(left.as_bytes(), right.as_bytes(), from),
        Some(_)
    )
}

/// A const equivalent of `&string[..len]`.
///
/// If `string.len() < len`, this simply returns `string` back.
///
/// # Performance
///
/// This has the same performance as
/// [`konst::slice::slice_up_to`](../slice/fn.slice_up_to.html#performance)
///
/// # Panics
///
/// This function panics if `len` is inside the string and doesn't fall on a char boundary.
///
/// # Example
///
/// ```
/// use konst::string::str_up_to;
///
/// const STR: &str = "foo bar baz";
///
/// const SUB0: &str = str_up_to(STR, 3);
/// assert_eq!(SUB0, "foo");
///
/// const SUB1: &str = str_up_to(STR, 7);
/// assert_eq!(SUB1, "foo bar");
///
/// const SUB2: &str = str_up_to(STR, 11);
/// assert_eq!(SUB2, STR);
///
/// const SUB3: &str = str_up_to(STR, 100);
/// assert_eq!(SUB3, STR);
///
///
/// ```
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn str_up_to(string: &str, len: usize) -> &str {
    let bytes = string.as_bytes();
    if is_char_boundary(bytes, len) {
        // Safety: is_char_boundary checks that `len` falls on a char boundary.
        unsafe { core::str::from_utf8_unchecked(crate::slice::slice_up_to(bytes, len)) }
    } else {
        [/* len is not on a char boundary */][len]
    }
}

/// A const equivalent of `string.get(..len)`.
///
/// # Performance
///
/// This has the same performance as
/// [`konst::slice::slice_up_to`](../slice/fn.slice_up_to.html#performance)
///
/// # Example
///
/// ```
/// use konst::string;
///
/// const STR: &str = "foo bar baz";
///
/// const SUB0: Option<&str> = string::get_up_to(STR, 3);
/// assert_eq!(SUB0, Some("foo"));
///
/// const SUB1: Option<&str> = string::get_up_to(STR, 7);
/// assert_eq!(SUB1, Some("foo bar"));
///
/// const SUB2: Option<&str> = string::get_up_to(STR, 11);
/// assert_eq!(SUB2, Some(STR));
///
/// const SUB3: Option<&str> = string::get_up_to(STR, 100);
/// assert_eq!(SUB3, None);
///
///
/// ```
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn get_up_to(string: &str, len: usize) -> Option<&str> {
    let bytes = string.as_bytes();

    crate::option::and_then!(
        crate::slice::get_up_to(bytes, len),
        |x| if is_char_boundary_get(bytes, len) {
            // Safety: is_char_boundary_get checks that `len` falls on a char boundary.
            unsafe { Some(core::str::from_utf8_unchecked(x)) }
        } else {
            None
        }
    )
}

/// A const equivalent of `&string[start..]`.
///
/// If `string.len() < start`, this simply returns an empty string` back.
///
/// # Performance
///
/// This has the same performance as
/// [`konst::slice::slice_from`](../slice/fn.slice_from.html#performance)
///
/// # Panics
///
/// This function panics if `start` is inside the string and doesn't fall on a char boundary.
///
/// # Example
///
/// ```
/// use konst::string::str_from;
///
/// const STR: &str = "foo bar baz";
///
/// const SUB0: &str = str_from(STR, 0);
/// assert_eq!(SUB0, STR);
///
/// const SUB1: &str = str_from(STR, 4);
/// assert_eq!(SUB1, "bar baz");
///
/// const SUB2: &str = str_from(STR, 8);
/// assert_eq!(SUB2, "baz");
///
/// const SUB3: &str = str_from(STR, 11);
/// assert_eq!(SUB3, "");
///
/// const SUB4: &str = str_from(STR, 1000);
/// assert_eq!(SUB3, "");
///
///
/// ```
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn str_from(string: &str, start: usize) -> &str {
    let bytes = string.as_bytes();
    if is_char_boundary(bytes, start) {
        // Safety: is_char_boundary checks that `start` falls on a char boundary.
        unsafe { core::str::from_utf8_unchecked(crate::slice::slice_from(bytes, start)) }
    } else {
        [/* start is not on a char boundary */][start]
    }
}

/// A const equivalent of `string.get(from..)`.
///
/// # Performance
///
/// This has the same performance as
/// [`konst::slice::slice_from`](../slice/fn.slice_from.html#performance)
///
/// # Example
///
/// ```
/// use konst::string;
///
/// const STR: &str = "foo bar baz";
///
/// const SUB0: Option<&str> = string::get_from(STR, 0);
/// assert_eq!(SUB0, Some(STR));
///
/// const SUB1: Option<&str> = string::get_from(STR, 4);
/// assert_eq!(SUB1, Some("bar baz"));
///
/// const SUB2: Option<&str> = string::get_from(STR, 8);
/// assert_eq!(SUB2, Some("baz"));
///
/// const SUB3: Option<&str> = string::get_from(STR, 100);
/// assert_eq!(SUB3, None);
///
///
/// ```
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn get_from(string: &str, from: usize) -> Option<&str> {
    let bytes = string.as_bytes();

    crate::option::and_then!(
        crate::slice::get_from(bytes, from),
        |x| if is_char_boundary_get(bytes, from) {
            // Safety: is_char_boundary_get checks that `from` falls on a char boundary.
            unsafe { Some(core::str::from_utf8_unchecked(x)) }
        } else {
            None
        }
    )
}

/// A const equivalent of [`str::split_at`]
///
/// If `at > string.len()` this returns `(string, "")`.
///
/// # Performance
///
/// This has the same performance as [`konst::slice::split_at`](crate::slice::split_at)
///
/// # Panics
///
/// This function panics if `at` is inside the string and doesn't fall on a char boundary.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// const IN: &str = "foo bar baz";
///
/// {
///     const SPLIT0: (&str, &str) = string::split_at(IN, 0);
///     assert_eq!(SPLIT0, ("", "foo bar baz"));
/// }
/// {
///     const SPLIT1: (&str, &str) = string::split_at(IN, 4);
///     assert_eq!(SPLIT1, ("foo ", "bar baz"));
/// }
/// {
///     const SPLIT2: (&str, &str) = string::split_at(IN, 8);
///     assert_eq!(SPLIT2, ("foo bar ", "baz"));
/// }
/// {
///     const SPLIT3: (&str, &str) = string::split_at(IN, 11);
///     assert_eq!(SPLIT3, ("foo bar baz", ""));
/// }
/// {
///     const SPLIT4: (&str, &str) = string::split_at(IN, 13);
///     assert_eq!(SPLIT4, ("foo bar baz", ""));
/// }
///
/// ```
///
/// [`str::split_at`]: https://doc.rust-lang.org/std/primitive.str.html#method.split_at
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn split_at(string: &str, at: usize) -> (&str, &str) {
    (str_up_to(string, at), str_from(string, at))
}

/// A const equivalent of `&string[start..end]`.
///
/// If `start >= end ` or `string.len() < start `, this returns an empty string.
///
/// If `string.len() < end`, this returns the string from `start`.
///
/// # Alternatives
///
/// For a const equivalent of `&string[start..]` there's [`str_from`].
///
/// For a const equivalent of `&string[..end]` there's [`str_up_to`].
///
/// [`str_from`]: ./fn.str_from.html
/// [`str_up_to`]: ./fn.str_up_to.html
///
/// # Performance
///
/// This has the same performance as
/// [`konst::slice::slice_range`](../slice/fn.slice_range.html#performance)
///
/// # Panics
///
/// This function panics if either `start` or `end` are inside the string and
/// don't fall on a char boundary.
///
/// # Example
///
/// ```
/// use konst::string::str_range;
///
/// const STR: &str = "foo bar baz";
///
/// const SUB0: &str = str_range(STR, 0, 3);
/// assert_eq!(SUB0, "foo");
///
/// const SUB1: &str = str_range(STR, 0, 7);
/// assert_eq!(SUB1, "foo bar");
///
/// const SUB2: &str = str_range(STR, 4, 11);
/// assert_eq!(SUB2, "bar baz");
///
/// const SUB3: &str = str_range(STR, 0, 1000);
/// assert_eq!(SUB3, STR);
///
///
/// ```
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn str_range(string: &str, start: usize, end: usize) -> &str {
    let bytes = string.as_bytes();
    let start_inbounds = is_char_boundary(bytes, start);
    if start_inbounds && is_char_boundary(bytes, end) {
        // Safety: is_char_boundary checks that `start` and `end` fall on a char boundaries.
        unsafe { core::str::from_utf8_unchecked(crate::slice::slice_range(bytes, start, end)) }
    } else if start_inbounds {
        [/* end is not on a char boundary */][end]
    } else {
        [/* start is not on a char boundary */][start]
    }
}

/// A const equivalent of `string.get(start..end)`.
///
/// # Alternatives
///
/// For a const equivalent of `string.get(start..)` there's [`get_from`].
///
/// For a const equivalent of `string.get(..end)` there's [`get_up_to`].
///
/// [`get_from`]: ./fn.get_from.html
/// [`get_up_to`]: ./fn.get_up_to.html
///
/// # Performance
///
/// This has the same performance as
/// [`konst::slice::slice_range`](../slice/fn.slice_range.html#performance)
///
/// # Example
///
/// ```
/// use konst::string;
///
/// const STR: &str = "foo bar baz";
///
/// const SUB0: Option<&str> = string::get_range(STR, 0, 3);
/// assert_eq!(SUB0, Some("foo"));
///
/// const SUB1: Option<&str> = string::get_range(STR, 0, 7);
/// assert_eq!(SUB1, Some("foo bar"));
///
/// const SUB2: Option<&str> = string::get_range(STR, 4, 11);
/// assert_eq!(SUB2, Some("bar baz"));
///
/// const SUB3: Option<&str> = string::get_range(STR, 0, 1000);
/// assert_eq!(SUB3, None);
///
///
/// ```
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn get_range(string: &str, start: usize, end: usize) -> Option<&str> {
    let bytes = string.as_bytes();

    crate::option::and_then!(
        crate::slice::get_range(bytes, start, end),
        |x| if is_char_boundary_get(bytes, start) && is_char_boundary_get(bytes, end) {
            // Safety: is_char_boundary_get checks that `start` and `end` fall on a char boundary.
            unsafe { Some(core::str::from_utf8_unchecked(x)) }
        } else {
            None
        }
    )
}

/// A const subset of [`str::strip_prefix`], this only takes a `&str` pattern.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// {
///     const STRIP: Option<&str> = string::strip_prefix("3 5 8", "3");
///     assert_eq!(STRIP, Some(" 5 8"));
/// }
/// {
///     const STRIP: Option<&str> = string::strip_prefix("3 5 8", "3 5 ");
///     assert_eq!(STRIP, Some("8"));
/// }
/// {
///     const STRIP: Option<&str> = string::strip_prefix("3 5 8", "hello");
///     assert_eq!(STRIP, None);
/// }
///
///
/// ```
///
/// [`str::strip_prefix`]: https://doc.rust-lang.org/std/primitive.str.html#method.strip_prefix
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn strip_prefix<'a>(string: &'a str, prefix: &str) -> Option<&'a str> {
    // Safety: because `prefix` is a `&str`, removing it should result in a valid `&str`
    unsafe {
        crate::option::map!(
            crate::slice::bytes_strip_prefix(string.as_bytes(), prefix.as_bytes()),
            core::str::from_utf8_unchecked,
        )
    }
}

/// A const subset of [`str::strip_suffix`], this only takes a `&str` pattern.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// {
///     const STRIP: Option<&str> = string::strip_suffix("3 5 8", "8");
///     assert_eq!(STRIP, Some("3 5 "));
/// }
/// {
///     const STRIP: Option<&str> = string::strip_suffix("3 5 8", " 5 8");
///     assert_eq!(STRIP, Some("3"));
/// }
/// {
///     const STRIP: Option<&str> = string::strip_suffix("3 5 8", "hello");
///     assert_eq!(STRIP, None);
/// }
///
///
/// ```
///
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn strip_suffix<'a>(string: &'a str, suffix: &str) -> Option<&'a str> {
    // Safety: because `suffix` is a `&str`, removing it should result in a valid `&str`
    unsafe {
        crate::option::map!(
            crate::slice::bytes_strip_suffix(string.as_bytes(), suffix.as_bytes()),
            core::str::from_utf8_unchecked,
        )
    }
}

#[cfg(feature = "rust_1_55")]
const fn is_char_boundary(bytes: &[u8], position: usize) -> bool {
    position >= bytes.len() || (bytes[position] as i8) >= -0x40
}

#[cfg(feature = "rust_1_55")]
const fn is_char_boundary_get(bytes: &[u8], position: usize) -> bool {
    let len = bytes.len();

    position == len || (bytes[position] as i8) >= -0x40
}

#[cfg(feature = "rust_1_64")]
const fn find_next_char_boundary(bytes: &[u8], mut position: usize) -> usize {
    loop {
        position += 1;

        if is_char_boundary(bytes, position) {
            break position;
        }
    }
}

#[cfg(feature = "rust_1_64")]
const fn find_prev_char_boundary(bytes: &[u8], mut position: usize) -> usize {
    position = position.saturating_sub(1);

    while !is_char_boundary(bytes, position) {
        position -= 1;
    }

    position
}

/// A const subset of [`str::trim`] which only removes ascii whitespace.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// const TRIMMED: &str = string::trim("\nhello world  ");
///
/// assert_eq!(TRIMMED, "hello world");
///
/// ```
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn trim(this: &str) -> &str {
    let trimmed = crate::slice::bytes_trim(this.as_bytes());
    // safety: bytes_trim only removes ascii bytes
    unsafe { core::str::from_utf8_unchecked(trimmed) }
}

/// A const subset of [`str::trim_start`] which only removes ascii whitespace.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// const TRIMMED: &str = string::trim_start("\rfoo bar  ");
///
/// assert_eq!(TRIMMED, "foo bar  ");
///
/// ```
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn trim_start(this: &str) -> &str {
    let trimmed = crate::slice::bytes_trim_start(this.as_bytes());
    // safety: bytes_trim_start only removes ascii bytes
    unsafe { core::str::from_utf8_unchecked(trimmed) }
}

/// A const subset of [`str::trim_end`] which only removes ascii whitespace.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// const TRIMMED: &str = string::trim_end("\rfoo bar  ");
///
/// assert_eq!(TRIMMED, "\rfoo bar");
///
/// ```
///
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn trim_end(this: &str) -> &str {
    let trimmed = crate::slice::bytes_trim_end(this.as_bytes());
    // safety: bytes_trim_end only removes ascii bytes
    unsafe { core::str::from_utf8_unchecked(trimmed) }
}

/// A const subset of [`str::trim_matches`] which only takes a `&str` pattern.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// const TRIMMED: &str = string::trim_matches("<>baz qux<><><>", "<>");
///
/// assert_eq!(TRIMMED, "baz qux");
///
/// ```
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn trim_matches<'a>(this: &'a str, needle: &str) -> &'a str {
    let trimmed = crate::slice::bytes_trim_matches(this.as_bytes(), needle.as_bytes());
    // safety:
    // because bytes_trim_matches was passed `&str`s casted to `&[u8]`s,
    // it returns a valid utf8 sequence.
    unsafe { core::str::from_utf8_unchecked(trimmed) }
}

/// A const subset of [`str::trim_start_matches`] which only takes a `&str` pattern.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// const TRIMMED: &str = string::trim_start_matches("#####huh###", "##");
///
/// assert_eq!(TRIMMED, "#huh###");
///
/// ```
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn trim_start_matches<'a>(this: &'a str, needle: &str) -> &'a str {
    let trimmed = crate::slice::bytes_trim_start_matches(this.as_bytes(), needle.as_bytes());
    // safety:
    // because bytes_trim_start_matches was passed `&str`s casted to `&[u8]`s,
    // it returns a valid utf8 sequence.
    unsafe { core::str::from_utf8_unchecked(trimmed) }
}

/// A const subset of [`str::trim_end_matches`] which only takes a `&str` pattern.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// const TRIMMED: &str = string::trim_end_matches("oowowooooo", "oo");
///
/// assert_eq!(TRIMMED, "oowowo");
///
/// ```
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn trim_end_matches<'a>(this: &'a str, needle: &str) -> &'a str {
    let trimmed = crate::slice::bytes_trim_end_matches(this.as_bytes(), needle.as_bytes());
    // safety:
    // because bytes_trim_end_matches was passed `&str`s casted to `&[u8]`s,
    // it returns a valid utf8 sequence.
    unsafe { core::str::from_utf8_unchecked(trimmed) }
}

/// Advances `this` past the first instance of `needle`.
///
/// Return `None` if no instance of `needle` is found.
///
/// Return `Some(this)` if `needle` is empty.
///
/// # Motivation
///
/// This function exists because calling
/// [`find`](crate::string::find) + [`str_from`]
/// when the `"rust_1_64"` feature is disabled
/// is slower than it could be, since the slice has to be traversed twice.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// {
///     const FOUND: Option<&str> = string::find_skip("foo bar baz", "bar");
///     assert_eq!(FOUND, Some(" baz"));
/// }
/// {
///     const NOT_FOUND: Option<&str> = string::find_skip("foo bar baz", "qux");
///     assert_eq!(NOT_FOUND, None);
/// }
/// {
///     const EMPTY_NEEDLE: Option<&str> = string::find_skip("foo bar baz", "");
///     assert_eq!(EMPTY_NEEDLE, Some("foo bar baz"));
/// }
/// ```
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn find_skip<'a>(this: &'a str, needle: &str) -> Option<&'a str> {
    unsafe {
        crate::option::map!(
            crate::slice::bytes_find_skip(this.as_bytes(), needle.as_bytes()),
            // safety:
            // because bytes_find_skip was passed `&str`s casted to `&[u8]`s,
            // it returns a valid utf8 sequence.
            core::str::from_utf8_unchecked,
        )
    }
}

/// Advances `this` up to the first instance of `needle`.
///
/// Return `None` if no instance of `needle` is found.
///
/// Return `Some(this)` if `needle` is empty.
///
/// # Motivation
///
/// This function exists because calling [`find`](crate::string::find) + [`str_from`]
/// when the `"rust_1_64"` feature is disabled
/// is slower than it could be, since the slice has to be traversed twice.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// {
///     const FOUND: Option<&str> = string::find_keep("foo bar baz", "bar");
///     assert_eq!(FOUND, Some("bar baz"));
/// }
/// {
///     const NOT_FOUND: Option<&str> = string::find_keep("foo bar baz", "qux");
///     assert_eq!(NOT_FOUND, None);
/// }
/// {
///     const EMPTY_NEEDLE: Option<&str> = string::find_keep("foo bar baz", "");
///     assert_eq!(EMPTY_NEEDLE, Some("foo bar baz"));
/// }
/// ```
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn find_keep<'a>(this: &'a str, needle: &str) -> Option<&'a str> {
    unsafe {
        crate::option::map!(
            crate::slice::bytes_find_keep(this.as_bytes(), needle.as_bytes()),
            // safety:
            // because bytes_find_keep was passed `&str`s casted to `&[u8]`s,
            // it returns a valid utf8 sequence.
            core::str::from_utf8_unchecked,
        )
    }
}

/// Truncates `this` to before the last instance of `needle`.
///
/// Return `None` if no instance of `needle` is found.
///
/// Return `Some(this)` if `needle` is empty.
///
/// # Motivation
///
/// This function exists because calling [`rfind`](crate::string::rfind) + [`str_up_to`]
/// when the `"rust_1_64"` feature is disabled
/// is slower than it could be, since the slice has to be traversed twice.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// {
///     const FOUND: Option<&str> = string::rfind_skip("foo bar _ bar baz", "bar");
///     assert_eq!(FOUND, Some("foo bar _ "));
/// }
/// {
///     const NOT_FOUND: Option<&str> = string::rfind_skip("foo bar baz", "qux");
///     assert_eq!(NOT_FOUND, None);
/// }
/// {
///     const EMPTY_NEEDLE: Option<&str> = string::rfind_skip("foo bar baz", "");
///     assert_eq!(EMPTY_NEEDLE, Some("foo bar baz"));
/// }
/// ```
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn rfind_skip<'a>(this: &'a str, needle: &str) -> Option<&'a str> {
    unsafe {
        crate::option::map!(
            crate::slice::bytes_rfind_skip(this.as_bytes(), needle.as_bytes()),
            // safety:
            // because bytes_rfind_skip was passed `&str`s casted to `&[u8]`s,
            // it returns a valid utf8 sequence.
            core::str::from_utf8_unchecked,
        )
    }
}

/// Truncates `this` to the last instance of `needle`.
///
/// Return `None` if no instance of `needle` is found.
///
/// Return `Some(this)` if `needle` is empty.
///
/// # Motivation
///
/// This function exists because calling [`rfind`](crate::string::rfind) + [`str_up_to`]
/// when the `"rust_1_64"` feature is disabled
/// is slower than it could be, since the slice has to be traversed twice.
///
/// # Example
///
/// ```rust
/// use konst::string;
///
/// {
///     const FOUND: Option<&str> = string::rfind_keep("foo bar _ bar baz", "bar");
///     assert_eq!(FOUND, Some("foo bar _ bar"));
/// }
/// {
///     const NOT_FOUND: Option<&str> = string::rfind_keep("foo bar baz", "qux");
///     assert_eq!(NOT_FOUND, None);
/// }
/// {
///     const EMPTY_NEEDLE: Option<&str> = string::rfind_keep("foo bar baz", "");
///     assert_eq!(EMPTY_NEEDLE, Some("foo bar baz"));
/// }
/// ```
#[cfg(feature = "rust_1_55")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_55")))]
pub const fn rfind_keep<'a>(this: &'a str, needle: &str) -> Option<&'a str> {
    unsafe {
        crate::option::map!(
            crate::slice::bytes_rfind_keep(this.as_bytes(), needle.as_bytes()),
            // safety:
            // because bytes_rfind_keep was passed `&str`s casted to `&[u8]`s,
            // it returns a valid utf8 sequence.
            core::str::from_utf8_unchecked,
        )
    }
}
