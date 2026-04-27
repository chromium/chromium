/// Compares two values for ordering.
///
/// The arguments must implement the [`ConstCmpMarker`] trait.
/// Non-standard library types must define a `const_cmp` method taking a reference.
///
/// # Limitations
///
/// The arguments must be concrete types, and have a fully inferred type.
/// eg: if you pass an integer literal it must have a suffix to indicate its type.
///
/// # Example
///
/// ```rust
/// use konst::{const_cmp, impl_cmp, try_equal};
///
/// use std::cmp::Ordering;
///
/// struct Fields<'a> {
///     foo: u32,
///     bar: Option<bool>,
///     baz: Ordering,
///     qux: &'a str,
/// }
///
/// impl_cmp!{
///     impl['a] Fields<'a>;
///     pub const fn const_cmp(&self, other: &Self) -> Ordering {
///         try_equal!(const_cmp!(self.foo, other.foo));
///         try_equal!(const_cmp!(self.bar, other.bar));
///         try_equal!(const_cmp!(self.baz, other.baz));
///         try_equal!(const_cmp!(self.qux, other.qux))
///     }
/// }
///
/// const CMPS: [Ordering; 4] = {
///     let foo = Fields {
///         foo: 10,
///         bar: None,
///         baz: Ordering::Less,
///         qux: "hello",
///     };
///     
///     let bar = Fields {
///         foo: 99,
///         bar: Some(true),
///         baz: Ordering::Greater,
///         qux: "world",
///     };
///     
///     [const_cmp!(foo, foo), const_cmp!(foo, bar), const_cmp!(bar, foo), const_cmp!(bar, bar)]
/// };
///
/// assert_eq!(CMPS, [Ordering::Equal, Ordering::Less, Ordering::Greater, Ordering::Equal]);
///
///
///
/// ```
///
/// [`ConstCmpMarker`]: ./polymorphism/trait.ConstCmpMarker.html
#[cfg(feature = "cmp")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "cmp")))]
#[macro_export]
macro_rules! const_cmp {
    ($left:expr, $right:expr $(,)*) => {
        match $crate::coerce_to_cmp!($left, $right) {
            (left, right) => left.const_cmp(right),
        }
    };
}

/// Compares two standard library types for ordering,
/// that can't be compared with [`const_cmp`].
///
/// This macro takes the same
/// [types](macro.const_eq_for.html#types-section) (except for range types),
/// has the same  [limitations](macro.const_eq_for.html#limitations-section),
/// and takes [arguments of the same form](macro.const_eq_for.html#arguments-section)
/// as the [`const_eq_for`] macro
///
/// # Examples
///
/// ### Slices
///
/// ```rust
/// use konst::{const_cmp, const_cmp_for, try_equal};
///
/// use std::cmp::Ordering;
///
/// const fn cmp_slice_pair(left: &[(u32, u32)], right: &[(u32, u32)]) -> Ordering {
///     const_cmp_for!(slice; left, right, |l, r|{
///         try_equal!(const_cmp!(l.0, r.0));
///         try_equal!(const_cmp!(l.1, r.1))
///     })
/// }
///
/// const CMPS: [Ordering; 4] = {
///     let foo = &[(0, 1), (1, 2), (3, 4), (5, 6)];
///     let bar = &[(0, 1), (3, 4), (5, 6), (7, 8)];
///
///     [
///         cmp_slice_pair(foo, foo),
///         cmp_slice_pair(foo, bar),
///         cmp_slice_pair(bar, foo),
///         cmp_slice_pair(bar, bar),
///     ]
/// };
///
/// assert_eq!(CMPS, [Ordering::Equal, Ordering::Less, Ordering::Greater, Ordering::Equal])
///
/// ```
///
///
/// ### Options
///
/// ```rust
/// use konst::{const_cmp, const_cmp_for, try_equal};
///
/// use std::cmp::Ordering;
///
/// #[derive(Copy, Clone)]
/// enum Shape {
///     Square,
///     Circle,
///     Line,
/// }
///
/// const fn cmp_opt_pair(left: Option<Shape>, right: Option<Shape>) -> Ordering {
///     const_cmp_for!(option; left, right, |x| *x as u8 )
/// }
///
/// const CMPS: [Ordering; 9] = {
///     let foo = Some(Shape::Square);
///     let bar = Some(Shape::Circle);
///     let baz = Some(Shape::Line);
///
///     [
///         cmp_opt_pair(foo, foo),
///         cmp_opt_pair(foo, bar),
///         cmp_opt_pair(foo, baz),
///
///         cmp_opt_pair(bar, foo),
///         cmp_opt_pair(bar, bar),
///         cmp_opt_pair(bar, baz),
///
///         cmp_opt_pair(baz, foo),
///         cmp_opt_pair(baz, bar),
///         cmp_opt_pair(baz, baz),
///     ]
/// };
///
/// assert_eq!(
///     CMPS,
///     [
///         Ordering::Equal, Ordering::Less, Ordering::Less,
///         Ordering::Greater, Ordering::Equal, Ordering::Less,
///         Ordering::Greater, Ordering::Greater, Ordering::Equal,
///     ]
/// );
///
/// ```
///
/// [`ConstCmpMarker`]: ./polymorphism/trait.ConstCmpMarker.html
/// [`const_cmp`]: macro.const_cmp.html
/// [`cmp::Ordering`]: https://doc.rust-lang.org/core/cmp/enum.Ordering.html
///
#[cfg(feature = "cmp")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "cmp")))]
#[macro_export]
macro_rules! const_cmp_for {
    (
        slice;
        $left_slice:expr,
        $right_slice:expr
        $(, $($comparison:tt)* )?
    ) => {
        match ($left_slice, $right_slice) {(mut left_slice, mut right_slice) => {
            use $crate::__::Ordering as CmpOrdering;
            if left_slice.len() == right_slice.len() {
                loop{
                    if let ([l, l_rem@..], [r, r_rem@..]) = (left_slice, right_slice) {
                        left_slice = l_rem;
                        right_slice = r_rem;

                        let ord = $crate::__priv_const_cmp_for!{
                            *l,
                            *r,
                            $($($comparison)*)?
                        };
                        if !$crate::__::matches!(ord, $crate::__::Ordering::Equal) {
                            break ord;
                        }
                    } else {
                        break $crate::__::Ordering::Equal
                    }
                }
            } else if left_slice.len() < right_slice.len() {
                CmpOrdering::Less
            } else {
                CmpOrdering::Greater
            }
        }}
    };
    (
        option;
        $left_opt:expr,
        $right_opt:expr
        $(, $($comparison:tt)* )?
    ) => {
        match (&$left_opt, &$right_opt) {
            (Some(l), Some(r)) =>
                $crate::__priv_const_cmp_for!(*l, *r, $( $($comparison)* )?),
            (Some(_), None) => $crate::__::Greater,
            (None, Some(_)) => $crate::__::Less,
            (None, None) => $crate::__::Equal,
        }
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_const_cmp_for {
    ($left:expr, $right:expr, ) => {
        $crate::coerce_to_cmp!(&$left).const_cmp(&$right)
    };
    ($left:expr, $right:expr, |$l: pat| $key_expr:expr $(,)*) => {
        $crate::coerce_to_cmp!({
            let $l = &$left;
            $key_expr
        })
        .const_cmp(&{
            let $l = &$right;
            $key_expr
        })
    };
    ($left:expr, $right:expr, |$l: pat, $r: pat| $eq_expr:expr $(,)*) => {{
        let $l = &$left;
        let $r = &$right;
        $eq_expr
    }};
    ($left:expr, $right:expr, $func:path $(,)*) => {{
        $func(&$left, &$right)
    }};
}

/// Evaluates to `$ord` if it is `Ordering::Equal`,
/// otherwise returns it from the enclosing function.
///
/// # Example
///
/// ```rust
/// use konst::{const_cmp, impl_cmp, try_equal};
///
/// use std::cmp::Ordering;
///
/// struct Fields<'a> {
///     first: &'a [u8; 4],
///     second: bool,
///     third: Option<&'static str>,
/// }
///
/// impl_cmp!{
///     impl['a] Fields<'a>;
///     pub const fn const_cmp(&self, other: &Self) -> Ordering {
///         try_equal!(const_cmp!(self.first, other.first));
///         try_equal!(const_cmp!(self.second, other.second));
///         try_equal!(const_cmp!(self.third, other.third))
///     }
/// }
///
/// const CMPS: [Ordering; 4] = {
///     let foo = Fields {
///         first: &[3, 5, 8, 13],
///         second: false,
///         third: None,
///     };
///     
///     let bar = Fields {
///         first: &[5, 8, 13, 14],
///         second: true,
///         third: Some("what!?"),
///     };
///     
///     [const_cmp!(foo, foo), const_cmp!(foo, bar), const_cmp!(bar, foo), const_cmp!(bar, bar)]
/// };
///
/// assert_eq!(CMPS, [Ordering::Equal, Ordering::Less, Ordering::Greater, Ordering::Equal]);
///
/// ```
///
#[cfg(feature = "cmp")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "cmp")))]
#[macro_export]
macro_rules! try_equal {
    (break $ord:expr $(,)*) => {
        match $ord {
            $crate::__::Ordering::Equal => $crate::__::Ordering::Equal,
            ord => return ord,
        }
    };
    ($ord:expr $(,)*) => {
        match $ord {
            $crate::__::Ordering::Equal => $crate::__::Ordering::Equal,
            ord => return ord,
        }
    };
    (break; $ord:expr $(,)*) => {
        match $ord {
            $crate::__::Ordering::Equal => $crate::__::Ordering::Equal,
            ord => return ord,
        }
    };
}

#[cfg(feature = "cmp")]
macro_rules! cmp_int {
    ($l:expr, $r:expr $(,)*) => {{
        if $l == $r {
            Ordering::Equal
        } else if $l < $r {
            Ordering::Less
        } else {
            Ordering::Greater
        }
    }};
}
