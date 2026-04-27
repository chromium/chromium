/// Compares two values for equality.
///
/// The arguments must implement the [`ConstCmpMarker`] trait.
/// Non-standard library types must define a `const_eq` method taking a reference.
///
/// # Limitations
///
/// The arguments must be concrete types, and have a fully inferred type.
/// eg: if you pass an integer literal it must have a suffix to indicate its type.
///
/// # Example
///
/// ```rust
/// use konst::{const_eq, impl_cmp};
///
/// use std::ops::Range;
///
/// struct Fields<'a> {
///     foo: u32,
///     bar: Option<bool>,
///     baz: Range<usize>,
///     qux: &'a str,
/// }
///
/// impl_cmp!{
///     impl['a] Fields<'a>;
///     pub const fn const_eq(&self, other: &Self) -> bool {
///         self.foo == other.foo &&
///         const_eq!(self.bar, other.bar) &&
///         const_eq!(self.baz, other.baz) &&
///         const_eq!(self.qux, other.qux)
///     }
/// }
///
/// const CMPS: [bool; 4] = {
///     let foo = Fields {
///         foo: 10,
///         bar: None,
///         baz: 10..20,
///         qux: "hello",
///     };
///     
///     let bar = Fields {
///         foo: 99,
///         bar: Some(true),
///         baz: 0..5,
///         qux: "world",
///     };
///     
///     [const_eq!(foo, foo), const_eq!(foo, bar), const_eq!(bar, foo), const_eq!(bar, bar)]
/// };
///
/// assert_eq!(CMPS, [true, false, false, true]);
///
///
///
/// ```
///
/// [`ConstCmpMarker`]: ./polymorphism/trait.ConstCmpMarker.html
#[cfg(feature = "cmp")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "cmp")))]
#[macro_export]
macro_rules! const_eq {
    ($left:expr, $right:expr $(,)*) => {
        match $crate::coerce_to_cmp!($left, $right) {
            (left, right) => left.const_eq(right),
        }
    };
}

/// Compares two standard library types for equality,
/// that can't be compared with [`const_eq`].
///
/// <span id = "types-section"></span>
/// # Types
///
/// This macro supports multiple types with different prefixes:
///
/// - `slice`: for comparing `&[T]`. [example](#compare_slices_structs)
///
/// - `option`: for comparing `Option<T>`. [example](#compare_options)
///
/// - `range`: for comparing `Range<T>`. [example](#compare_ranges)
///
/// - `range_inclusive`: for comparing `RangeInclusive<T>`.
/// [example](#compare_ranges_incluside)
///
/// <span id = "limitations-section"></span>
/// # Limitations
///
/// The arguments must be concrete types, and have a fully inferred type.
/// eg: if you pass an integer literal it must have a suffix to indicate its type.
///
/// <span id = "arguments-section"></span>
/// # Arguments
///
/// The arguments take this form
///
/// ```text
/// const_eq_for!(type; left_value, right_value <comparator> )
/// ```
///
/// ### Comparator argument
///
/// The `<comparator>` argument can be any of:
///
/// - ` `(passing nothing): Compares the item using the [`const_eq`] macro.
/// [example](#compare_slices_structs)
///
/// - `, |item| <expression>`:
/// Converts the item with `<expression>` to a type that can be compared using the
/// [`const_eq`] macro.
/// [example](#compare_slices_fieldless_enums)
///
/// - `, |left_item, right_item| <expression>`:
/// Compares the items  with `<expression>`, which must evaluate to a `bool`.
/// [example](#compare_options)
///
/// - `, path::to::function`:
/// Compares the items using the passed function, which must evaluate to a `bool`.
/// [example](#compare_ranges_incluside)
///
///
///
/// # Examples
///
/// <span id = "compare_slices_structs"></span>
/// ### Comparing slices of structs
///
/// ```
/// use konst::{const_eq_for, eq_str};
///
/// #[derive(Debug, Copy, Clone, PartialEq, Eq)]
/// pub struct Location {
///     pub file: &'static str,
///     pub column: u32,
///     pub line: u32,
/// }
///
/// konst::impl_cmp! {
///     impl Location;
///     
///     pub const fn const_eq(&self, other: &Self) -> bool {
///         eq_str(self.file, other.file) &&
///         self.column == other.column &&
///         self.line == other.line
///     }
/// }
/// #
/// # macro_rules! here {
/// #   () => {
/// #       $crate::Location{file: file!(), column: column!(), line: line!()}
/// #   }
/// # }
/// #
///
/// # fn main () {
/// const HERE: &[Location] = &[here!(), here!(), here!(), here!()];
///
/// const THERE: &[Location] = &[here!(), here!(), here!(), here!()];
///
/// const CMP_HERE: bool = const_eq_for!(slice; HERE, HERE);
/// assert!( CMP_HERE );
///
/// const CMP_HERE_THERE: bool = const_eq_for!(slice; HERE, THERE);
/// assert!( !CMP_HERE_THERE );
///
/// const CMP_THERE_THERE: bool = const_eq_for!(slice; THERE, THERE);
/// assert!( CMP_THERE_THERE );
///
///
/// # }
///
/// ```
///
/// <span id = "compare_slices_fieldless_enums"></span>
/// ### Comparing slices of field-less enums
///
/// ```rust
/// #[derive(Copy, Clone)]
/// enum Direction {
///     Left,
///     Right,
///     Up,
///     Down,
/// }
///
/// use Direction::*;
///
/// const fn eq_slice_direction(left: &[Direction], right: &[Direction]) -> bool {
///     konst::const_eq_for!(slice; left, right, |&x| x as u8)
/// }
///
/// const CHEAT_CODE: &[Direction] = &[Up, Up, Down, Down, Left, Right, Left, Right];
///
/// const CLOCKWISE: &[Direction] = &[Up, Right, Down, Left];
///
///
/// const CMP_CHEAT: bool = eq_slice_direction(CHEAT_CODE, CHEAT_CODE);
/// assert!( CMP_CHEAT );
///
/// const CMP_CHEAT_CLOCK: bool = eq_slice_direction(CHEAT_CODE, CLOCKWISE);
/// assert!( !CMP_CHEAT_CLOCK );
///
/// const CMP_CLOCK_CLOCK: bool = eq_slice_direction(CLOCKWISE, CLOCKWISE);
/// assert!( CMP_CLOCK_CLOCK );
///
/// ```
///
/// <span id = "compare_options"></span>
/// ### Comparing `Option`s
///
/// ```rust
/// use konst::const_eq_for;
///
/// const SOME: Option<(u32, u32)> = Some((3, 5));
/// const NONE: Option<(u32, u32)> = None;
///
/// const fn eq_opt_tuple(left: &Option<(u32, u32)>, right: &Option<(u32, u32)>) -> bool {
///     const_eq_for!(option; left, right, |l, r| l.0 == r.0 && l.1 == r.1 )
/// }
///
/// const SOME_SOME: bool = eq_opt_tuple(&SOME, &SOME);
/// assert!( SOME_SOME );
///
/// const SOME_NONE: bool = eq_opt_tuple(&SOME, &NONE);
/// assert!( !SOME_NONE );
///
/// const NONE_NONE: bool = eq_opt_tuple(&NONE, &NONE);
/// assert!( NONE_NONE );
///
/// ```
///
///
/// <span id = "compare_ranges"></span>
/// ### Comparing `Range`s
///
/// ```rust
/// use konst::{const_eq_for, impl_cmp};
///
/// use std::ops::Range;
///
/// #[derive(Copy, Clone)]
/// pub enum Month {
///     January,
///     February,
///     March,
///     April,
///     May,
///     June,
///     July,
///     August,
///     September,
///     October,
///     November,
///     December,
/// }
///
/// use Month::*;
///
/// konst::impl_cmp! {
///     impl Month;
///     
///     pub const fn const_eq(&self, other: &Self) -> bool {
///         *self as u8 == *other as u8
///     }
/// }
///
/// const FOO: Range<Month> = January..April;
/// const BAR: Range<Month> = October..December;
///
/// const FOO_FOO: bool = const_eq_for!(range; FOO, FOO);
/// assert!( FOO_FOO );
///
/// const FOO_BAR: bool = const_eq_for!(range; FOO, BAR);
/// assert!( !FOO_BAR );
///
/// const BAR_BAR: bool = const_eq_for!(range; BAR, BAR);
/// assert!( BAR_BAR );
///
/// ```
///
/// <span id = "compare_ranges_incluside"></span>
/// ### Comparing `RangeInclusive`s
///
/// ```rust
/// use konst::{const_eq_for, impl_cmp};
///
/// use std::ops::RangeInclusive;
///
/// #[derive(Copy, Clone)]
/// pub enum WeekDay {
///     Monday,
///     Tuesday,
///     Wednesday,
///     Thursday,
///     Friday,
///     Saturday,
///     Sunday,
/// }
///
/// use WeekDay::*;
///
/// konst::impl_cmp! {
///     impl WeekDay;
///     
///     pub const fn const_eq(&self, other: &Self) -> bool {
///         *self as u8 == *other as u8
///     }
/// }
///
/// const FOO: RangeInclusive<WeekDay> = Monday..=Thursday;
/// const BAR: RangeInclusive<WeekDay> = Friday..=Sunday;
///
/// const FOO_FOO: bool = const_eq_for!(range_inclusive; FOO, FOO);
/// assert!( FOO_FOO );
///
/// const FOO_BAR: bool = const_eq_for!(range_inclusive; FOO, BAR, WeekDay::const_eq);
/// assert!( !FOO_BAR );
///
/// const BAR_BAR: bool = const_eq_for!(range_inclusive; BAR, BAR, WeekDay::const_eq);
/// assert!( BAR_BAR );
///
/// ```
///
/// [`ConstCmpMarker`]: ./polymorphism/trait.ConstCmpMarker.html
/// [`const_eq`]: macro.const_eq.html
#[cfg(feature = "cmp")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "cmp")))]
#[macro_export]
macro_rules! const_eq_for {
    (
        slice;
        $left_slice:expr,
        $right_slice:expr
        $(, $($comparison:tt)* )?
    ) => {
        match ($left_slice, $right_slice) {
            (left_slice, right_slice) => {
                let mut returned = left_slice.len() == right_slice.len();
                if returned {
                    let mut i = 0;
                    while i != left_slice.len() {
                        let are_eq = $crate::__priv_const_eq_for!(
                            left_slice[i],
                            right_slice[i],
                            $( $($comparison)* )?
                        );
                        if !are_eq {
                            returned = false;
                            break;
                        }
                        i += 1;
                    }
                }
                returned
            }
        }
    };
    (
        option;
        $left_opt:expr,
        $right_opt:expr
        $(, $($comparison:tt)* )?
    ) => {
        match (&$left_opt, &$right_opt) {
            (Some(l), Some(r)) =>
                $crate::__priv_const_eq_for!(*l, *r, $( $($comparison)* )?),
            (None, None) => true,
            _ => false,
        }
    };
    (
        range;
        $left_range:expr,
        $right_range:expr
        $(, $($comparison:tt)* )?
    ) => {
        match (&$left_range, &$right_range) {
            (left_range, right_range) => {
                $crate::__priv_const_eq_for!(
                    left_range.start,
                    right_range.start,
                    $( $($comparison)* )?
                ) &&
                $crate::__priv_const_eq_for!(
                    left_range.end,
                    right_range.end,
                    $( $($comparison)* )?
                )
            }
        }
    };
    (
        range_inclusive;
        $left_range:expr,
        $right_range:expr
        $(, $($comparison:tt)* )?
    ) => {
        match (&$left_range, &$right_range) {
            (left_range, right_range) => {
                $crate::__priv_const_eq_for!(
                    left_range.start(),
                    right_range.start(),
                    $( $($comparison)* )?
                ) &&
                $crate::__priv_const_eq_for!(
                    left_range.end(),
                    right_range.end(),
                    $( $($comparison)* )?
                )
            }
        }
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_const_eq_for {
    ($left:expr, $right:expr, ) => {
        $crate::coerce_to_cmp!($left).const_eq(&$right)
    };
    ($left:expr, $right:expr, |$l: pat| $key_expr:expr $(,)*) => {
        $crate::coerce_to_cmp!({
            let $l = &$left;
            $key_expr
        })
        .const_eq(&{
            let $l = &$right;
            $key_expr
        })
    };
    ($left:expr, $right:expr, |$l: pat, $r: pat| $eq_expr:expr $(,)*) => {{
        let $l = &$left;
        let $r = &$right;
        $eq_expr
    }};
    ($left:expr, $right:expr, $func:path $(,)*) => {
        $func(&$left, &$right)
    };
}
