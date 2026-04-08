use crate::{ArrayString, PanicVal};

use super::{FmtArg, IsCustomType, PanicFmt};

/// For outputting an [alternate flag]-aware delimiter.
///
/// This was created for formatting structs and enum variants,
/// so these delimiters have spaces around them to follow the <br>
/// `Foo { bar: baz }`, `Foo(bar)`, and `[foo, bar]` style.
///
/// # Example
///
/// ```rust
/// use const_panic::{
///     fmt::{self, FmtArg},
///     ArrayString,
///     flatten_panicvals,
/// };
///
/// // Debug formatting
/// assert_eq!(
///     const_panic::concat_!(FmtArg::DEBUG;
///         open: fmt::EmptyDelimiter,
///             100u8, fmt::COMMA_SEP,
///             false, fmt::COMMA_SEP,
///             [0u16; 0], fmt::COMMA_SEP,
///             // parenthesizing to pass this as a non-literal
///             // otherwise the string is Display formatted
///             ("really"), fmt::COMMA_TERM,
///         close: "",
///     ),
///     " 100, false, [], \"really\""
/// );
///
///
/// // Alternate-Debug formatting
/// assert_eq!(
///     const_panic::concat_!(FmtArg::ALT_DEBUG;
///         open: fmt::EmptyDelimiter,
///             100u8, fmt::COMMA_SEP,
///             false, fmt::COMMA_SEP,
///             [0u16; 0], fmt::COMMA_SEP,
///             ("really"), fmt::COMMA_TERM,
///         close: "",
///     ),
///     concat!(
///         "\n",
///         "    100,\n",
///         "    false,\n",
///         "    [],\n",
///         "    \"really\",\n",
///     )
/// );
///
/// ```
///
/// [alternate flag]: crate::FmtArg#structfield.is_alternate
#[non_exhaustive]
#[derive(Copy, Clone, PartialEq, Eq)]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
pub enum Delimiter {
    /// `(`
    OpenParen,
    /// `)`
    CloseParen,
    /// `[`
    OpenBracket,
    /// `]`
    CloseBracket,
    /// ` { `
    OpenBrace,
    /// ` }`
    CloseBrace,
    /// An empty delimiter,
    /// exists only to add whitespace on the next line when
    /// the alternate flag is enabled.
    Empty,
}

pub use self::Delimiter::{
    CloseBrace, CloseBracket, CloseParen, Empty as EmptyDelimiter, OpenBrace, OpenBracket,
    OpenParen,
};

impl Delimiter {
    /// Converts this `Delimiter` into an array of one `PanicVal`
    ///
    /// When the [alternate flag] is enabled, this and the `to_panicval` method output:
    /// - the delimiter
    /// - a newline
    /// - [fmtarg.indentation](crate::FmtArg#structfield.indentation) amount of spaces
    ///
    /// When the [alternate flag] is disabled,
    /// these methods output braces with spaces around them,
    /// the empty delimiter as one space,
    /// and the remaining delimiters with no spaces around them.
    ///
    /// [alternate flag]: crate::FmtArg#structfield.is_alternate
    ///
    pub const fn to_panicvals(self, f: FmtArg) -> [PanicVal<'static>; 1] {
        [self.to_panicval(f)]
    }
    /// Converts this `Delimiter` into a `PanicVal`
    pub const fn to_panicval(self, f: FmtArg) -> PanicVal<'static> {
        match (self, f.is_alternate) {
            (Self::OpenParen, false) => PanicVal::write_str("("),
            (Self::CloseParen, false) => PanicVal::write_str(")"),
            (Self::OpenBracket, false) => PanicVal::write_str("["),
            (Self::CloseBracket, false) => PanicVal::write_str("]"),
            (Self::OpenBrace, false) => PanicVal::write_str(" { "),
            (Self::CloseBrace, false) => PanicVal::write_str(" }"),
            (Self::Empty, false) => PanicVal::write_str(" "),
            (Self::OpenParen, true) => PanicVal::write_str("(\n").with_rightpad(f),
            (Self::CloseParen, true) => PanicVal::write_str(")").with_leftpad(f),
            (Self::OpenBracket, true) => PanicVal::write_str("[\n").with_rightpad(f),
            (Self::CloseBracket, true) => PanicVal::write_str("]").with_leftpad(f),
            (Self::OpenBrace, true) => PanicVal::write_str(" {\n").with_rightpad(f),
            (Self::CloseBrace, true) => PanicVal::write_str("}").with_leftpad(f),
            (Self::Empty, true) => PanicVal::write_str("\n").with_rightpad(f),
        }
    }
}

impl PanicFmt for Delimiter {
    type This = Self;
    type Kind = IsCustomType;
    const PV_COUNT: usize = 1;
}

////////////////////////////////////////////////////////////////////////////////

/// How much indentation (in spaces) is added with [`FmtArg::indent`],
/// and removed with [`FmtArg::unindent`].
///
/// [The FmtArg.indentation field](crate::FmtArg#structfield.indentation)
/// is used by [`fmt::Delimiter`](crate::fmt::Delimiter)
/// and by [`fmt::Separator`](crate::fmt::Separator),
/// when the [`is_alternate`](crate::FmtArg#structfield.is_alternate) flag is enabled.
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
pub const INDENTATION_STEP: u8 = 4;

////////////////////////////////////////////////////////////////////////////////

/// A stack allocated string type that's convertible into
/// [`PanicVal<'static>`](PanicVal),
/// with [`SHORT_STRING_CAP`] capacity.
///
/// # Example
///
/// ```rust
/// use const_panic::{
///     fmt::ShortString,
///     ArrayString, FmtArg, PanicVal,
/// };
///
/// let pv: PanicVal<'static> =
///     PanicVal::write_short_str(ShortString::new("3,13,21,34,55,89"));
///
/// assert_eq!(ArrayString::<20>::from_panicvals(&[pv]).unwrap(), "3,13,21,34,55,89");
///
///
/// let pv_debug: PanicVal<'static> =
///     PanicVal::from_short_str(ShortString::new("foo\n\0bar"), FmtArg::DEBUG);
///
/// assert_eq!(
///     ArrayString::<20>::from_panicvals(&[pv_debug]).unwrap(),
///     "\"foo\\n\\x00bar\"",
/// );
///
/// ```
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
pub type ShortString = ArrayString<SHORT_STRING_CAP>;

pub use crate::utils::string_cap::TINY as SHORT_STRING_CAP;

impl<'a> PanicVal<'a> {
    /// Constructs a `PanicVal` from a `ShortString`.
    pub const fn from_short_str(this: ShortString, f: FmtArg) -> PanicVal<'a> {
        use crate::panic_val::{PanicVariant, StrFmt};
        PanicVal::__new(PanicVariant::ShortString(StrFmt::new(f), this.to_compact()))
    }
}

////////////////////////////////////////////////////////////////////////////////

/// For computing the [`PanicFmt::PV_COUNT`] of a struct or enum variant,
/// with the [`call`](ComputePvCount::call) method.
///
/// This assumes that you write the `to_panicvals` method like in  the
/// [`flatten_panicvals` examples](crate::flatten_panicvals#examples)
///
/// # Examples
///
/// These examples demonstrates how to use this type by itself,
/// for a more complete example you can look at the
/// [`flatten_panicvals` examples](crate::flatten_panicvals#examples)
///
/// ### Struct
///
/// ```rust
/// use const_panic::{ComputePvCount, PanicFmt};
///
/// struct Foo<'a> {
///     x: u32,
///     y: &'a [&'a str],
///     z: Bar,
/// }
/// # type Bar = u8;
///
/// impl PanicFmt for Foo<'_> {
///     type This = Self;
///     type Kind = const_panic::IsCustomType;
///     
///     const PV_COUNT: usize = ComputePvCount{
///         field_amount: 3,
///         summed_pv_count:
///             u32::PV_COUNT +
///             <&[&str]>::PV_COUNT +
///             <Bar>::PV_COUNT,
///         delimiter: const_panic::TypeDelim::Braced,
///     }.call();
/// }
///
/// assert_eq!(Foo::PV_COUNT, 12);
///
/// ```
///
/// ### Enum
///
/// ```rust
/// use const_panic::{ComputePvCount, PanicFmt};
///
/// enum Foo {
///     Bar,
///     Baz(u32, u64),
/// }
/// # type Bar = u8;
///
/// impl PanicFmt for Foo {
///     type This = Self;
///     type Kind = const_panic::IsCustomType;
///     
///     const PV_COUNT: usize = const_panic::utils::slice_max_usize(&[
///         ComputePvCount{
///             field_amount: 0,
///             summed_pv_count: 0,
///             delimiter: const_panic::TypeDelim::Braced,
///         }.call(),
///         ComputePvCount{
///             field_amount: 2,
///             summed_pv_count: <u32>::PV_COUNT + <u64>::PV_COUNT,
///             delimiter: const_panic::TypeDelim::Tupled,
///         }.call(),
///     ]);
/// }
///
/// assert_eq!(Foo::PV_COUNT, 7);
///
/// ```
///
///
///
///
///
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
pub struct ComputePvCount {
    /// The amount of fields in the struct
    pub field_amount: usize,
    /// The summed up amount of `PanicVal`s that all the fields produce.
    ///
    /// Eg: for a struct with `Bar` and `Qux` fields, this would be
    /// `<Bar as PanicFmt>::PV_COUNT + <Qux as PanicFmt>::PV_COUNT`,
    ///
    pub summed_pv_count: usize,
    /// Whether it's a tupled or braced struct/variant.
    pub delimiter: TypeDelim,
}

impl ComputePvCount {
    /// Does the computation.
    pub const fn call(&self) -> usize {
        // field-less structs and variants don't output the empty delimiter
        if self.field_amount == 0 {
            return 1;
        }

        const TYPE_NAME: usize = 1;
        const DELIM_TOKENS: usize = 2;
        let field_tokens = match self.delimiter {
            TypeDelim::Tupled => self.field_amount,
            TypeDelim::Braced => 2 * self.field_amount,
        };

        TYPE_NAME + DELIM_TOKENS + field_tokens + self.summed_pv_count
    }
}

/// Whether a struct or variant is Tupled or Braced.
///
/// Unit structs/variants are considered braced.
///
/// # Example
///
/// ### Formatting
///
/// ```rust
/// use const_panic::{
///     fmt::{self, FmtArg, TypeDelim},
///     ArrayString,
///     flatten_panicvals,
/// };
///
/// {
///     assert_eq!(
///         const_panic::concat_!(FmtArg::DEBUG;
///             "Foo",
///             open: TypeDelim::Tupled.open(),
///                 10u8, fmt::COMMA_SEP,
///                 false, fmt::COMMA_TERM,
///             close: TypeDelim::Tupled.close(),
///         ),
///         "Foo(10, false)"
///     );
/// }
///
/// {
///     assert_eq!(
///         const_panic::concat_!(FmtArg::DEBUG;
///             "Bar",
///             open: TypeDelim::Braced.open(),
///                 "x: ", debug: "hello", fmt::COMMA_SEP,
///                 "y: ", true, fmt::COMMA_TERM,
///             close: TypeDelim::Braced.close(),
///         ),
///         "Bar { x: \"hello\", y: true }"
///     );
/// }
///
/// ```
///
#[derive(Copy, Clone, PartialEq, Eq)]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
pub enum TypeDelim {
    /// A `Foo(Bar)` type or variant
    Tupled,
    /// A `Foo{bar: Baz}` type or variant
    Braced,
}

impl TypeDelim {
    /// Gets the delimiters that surround the fields of a struct or variant.
    pub const fn get_open_and_close(self) -> (Delimiter, Delimiter) {
        (self.open(), self.close())
    }
    /// Gets the group delimiter that precedes the fields of a struct or variant.
    pub const fn open(self) -> Delimiter {
        match self {
            Self::Tupled => Delimiter::OpenParen,
            Self::Braced => Delimiter::OpenBrace,
        }
    }
    /// Gets the group delimiter that follows the fields of a struct or variant.
    pub const fn close(self) -> Delimiter {
        match self {
            Self::Tupled => Delimiter::CloseParen,
            Self::Braced => Delimiter::CloseBrace,
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

/// An [alternate-flag-aware] comma separator for use between fields or elements.
///
/// When the alternate flag is enabled, this puts each field/element on its own line.
///
/// # Example
///
/// ```rust
/// use const_panic::{
///     fmt::{self, FmtArg},
///     ArrayString,
///     flatten_panicvals,
/// };
///
/// // Debug formatting
/// assert_eq!(
///     const_panic::concat_!(FmtArg::DEBUG;
///         open: fmt::OpenBracket,
///             100u8, fmt::COMMA_SEP,
///             false, fmt::COMMA_SEP,
///             [0u16; 0], fmt::COMMA_SEP,
///             // fmt::COMMA_TERM always goes after the last field
///             debug: "really", fmt::COMMA_TERM,
///         close: fmt::CloseBracket,
///     ),
///     "[100, false, [], \"really\"]"
/// );
///
///
/// // Alternate-Debug formatting
/// assert_eq!(
///     const_panic::concat_!(FmtArg::ALT_DEBUG;
///         open: fmt::OpenBracket,
///             100u8, fmt::COMMA_SEP,
///             false, fmt::COMMA_SEP,
///             [0u16; 0], fmt::COMMA_SEP,
///             // fmt::COMMA_TERM always goes after the last field
///             debug: "really", fmt::COMMA_TERM,
///         close: fmt::CloseBracket,
///     ),
///     concat!(
///         "[\n",
///         "    100,\n",
///         "    false,\n",
///         "    [],\n",
///         "    \"really\",\n",
///         "]",
///     )
/// );
///
/// ```
///
/// [alternate-flag-aware]: crate::FmtArg#structfield.is_alternate
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
pub const COMMA_SEP: Separator<'_> = Separator::new(",", IsLast::No);

/// An [alternate-flag-aware] comma for use after the last field or element.
///
/// For an example, you can look at [the one for `COMMA_SEP`](COMMA_SEP#example)
///
/// When the alternate flag is enabled, this puts each field/element on its own line.
///
/// [alternate-flag-aware]: crate::FmtArg#structfield.is_alternate
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
pub const COMMA_TERM: Separator<'_> = Separator::new(",", IsLast::Yes);

/// For telling [`Separator`] whether it comes after the last field or not.
#[derive(Copy, Clone, PartialEq, Eq)]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
pub enum IsLast {
    ///
    Yes,
    ///
    No,
}

/// For [alternate flag]-aware separation of fields, collection elements, etc.
///
/// # Example
///
/// ```rust
/// use const_panic::{
///     fmt::{self, FmtArg, IsLast, Separator},
///     ArrayString,
///     flatten_panicvals,
/// };
///
/// const SEMICOLON_SEP: Separator = Separator::new(";", IsLast::No);
/// const SEMICOLON_TERM: Separator = Separator::new(";", IsLast::Yes);
///
/// // Debug formatting
/// assert_eq!(
///     const_panic::concat_!(FmtArg::DEBUG;
///         open: fmt::OpenBrace,
///             debug: "foo", SEMICOLON_SEP,
///             [3u8, 5, 8], SEMICOLON_SEP,
///             false, SEMICOLON_TERM,
///         close: fmt::CloseBrace,
///     ),
///     // the space before the brace is because Delimiter is intended for
///     // formatting structs and enum variants.
///     " { \"foo\"; [3, 5, 8]; false }"
/// );
///
///
/// // Alternate-Debug formatting
/// assert_eq!(
///     const_panic::concat_!(FmtArg::ALT_DEBUG;
///         open: fmt::OpenBrace,
///             debug: "foo", SEMICOLON_SEP,
///             debug: [3u8, 5, 8], SEMICOLON_SEP,
///             false, SEMICOLON_TERM,
///         close: fmt::CloseBrace,
///     ),
///     concat!(
///         " {\n",
///         "    \"foo\";\n",
///         "    [3, 5, 8];\n",
///         "    false;\n",
///         "}",
///     )
/// );
/// ```
///
/// [alternate flag]: crate::FmtArg#structfield.is_alternate
#[derive(Copy, Clone)]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
pub struct Separator<'a>(&'a str, IsLast);

impl<'a> Separator<'a> {
    /// Constructs a `Separator` from a custom separator.
    ///
    /// # Panics
    ///
    /// Panics if `string` is longer than 12 bytes.
    ///
    pub const fn new(string: &'a str, is_last_field: IsLast) -> Self {
        if string.len() > 12 {
            crate::concat_panic(&[&[
                PanicVal::write_str("expected a string shorter than 12 bytes,"),
                PanicVal::write_str("actual length: "),
                PanicVal::from_usize(string.len(), FmtArg::DISPLAY),
                PanicVal::write_str(", string: "),
                PanicVal::from_str(string, FmtArg::DEBUG),
            ]])
        }

        Separator(string, is_last_field)
    }

    /// Converts this `Separator` into an array of one `PanicVal`.
    /// Otherwise does the same as [`to_panicval`](Self::to_panicval)
    pub const fn to_panicvals(self, f: FmtArg) -> [PanicVal<'static>; 1] {
        [PanicVal::from_element_separator(self.0, self.1, f)]
    }

    /// Converts this `Separator` into a `PanicVal`.
    ///
    /// When the [alternate flag] is enabled, this and the `to_panicvals` method output:
    /// - the separator
    /// - a newline
    /// - [fmtarg.indentation](crate::FmtArg#structfield.indentation) amount of spaces
    /// if constructed with [`IsLast::No`]
    ///
    /// When the [alternate flag] is disabled,
    /// these methods output the separator and a single space
    /// if constructed with [`IsLast::No`],
    /// otherwise output nothing.
    ///
    /// [alternate flag]: crate::FmtArg#structfield.is_alternate
    pub const fn to_panicval(self, f: FmtArg) -> PanicVal<'static> {
        PanicVal::from_element_separator(self.0, self.1, f)
    }
}

impl PanicFmt for Separator<'_> {
    type This = Self;
    type Kind = IsCustomType;
    const PV_COUNT: usize = 1;
}
