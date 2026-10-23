// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module defines all available properties.
//!
//! Properties may be empty marker types and implement [`BinaryProperty`], or enumerations[^1]
//! and implement [`EnumeratedProperty`].
//!
//! [`BinaryProperty`]s are queried through a [`CodePointSetData`](crate::CodePointSetData),
//! while [`EnumeratedProperty`]s are queried through [`CodePointMapData`](crate::CodePointMapData).
//!
//! In addition, some [`EnumeratedProperty`]s also implement [`ParseableEnumeratedProperty`] or
//! [`NamedEnumeratedProperty`]. For these properties, [`PropertyParser`](crate::PropertyParser),
//! [`PropertyNamesLong`](crate::PropertyNamesLong), and [`PropertyNamesShort`](crate::PropertyNamesShort)
//! can be constructed.
//!
//! [^1]: either Rust `enum`s, or Rust `struct`s with associated constants (open enums)

pub use crate::names::{NamedEnumeratedProperty, ParseableEnumeratedProperty};

pub use crate::bidi::{BidiMirroringGlyph, BidiPairedBracketType};
pub use crate::code_point_map::EnumeratedProperty;

macro_rules! make_enumerated_property {
    (
        name: $name:literal;
        short_name: $short_name:literal;
        ident: $value_ty:path;
        data_marker: $data_marker:ty;
        singleton: $singleton:ident;
        $(ule_ty: $ule_ty:ty;)?
    ) => {
        impl crate::private::Sealed for $value_ty {}

        impl EnumeratedProperty for $value_ty {
            type DataMarker = $data_marker;
            #[cfg(feature = "compiled_data")]
            const SINGLETON: &'static crate::provider::PropertyCodePointMap<'static, Self> =
                crate::provider::Baked::$singleton;
            const NAME: &'static [u8] = $name.as_bytes();
            const SHORT_NAME: &'static [u8] = $short_name.as_bytes();
        }

        $(
            impl zerovec::ule::AsULE for $value_ty {
                type ULE = $ule_ty;

                fn to_unaligned(self) -> Self::ULE {
                    self.0.to_unaligned()
                }
                fn from_unaligned(unaligned: Self::ULE) -> Self {
                    Self(zerovec::ule::AsULE::from_unaligned(unaligned))
                }
            }
        )?
    };
}

/// Enumerated property `Bidi_Class`
///
/// These are the categories required by the Unicode Bidirectional Algorithm.
/// For the property values, see [Bidirectional Class Values](https://unicode.org/reports/tr44/#Bidi_Class_Values).
/// For more information, see [Unicode Standard Annex #9](https://unicode.org/reports/tr41/tr41-28.html#UAX9).
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::BidiClass};
///
/// assert_eq!(
///     CodePointMapData::<BidiClass>::new().get('y'),
///     BidiClass::LeftToRight
/// ); // U+0079
/// assert_eq!(
///     CodePointMapData::<BidiClass>::new().get('ع'),
///     BidiClass::ArabicLetter
/// ); // U+0639
/// ```
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct BidiClass(pub(crate) u8);

impl BidiClass {
    /// Returns an ICU4C `UBidiClass` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn to_icu4c_value(self) -> u8 {
        self.0
    }
    /// Constructor from an ICU4C `UBidiClass` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn from_icu4c_value(value: u8) -> Self {
        Self(value)
    }
}

impl Default for BidiClass {
    fn default() -> Self {
        Self::LeftToRight
    }
}

make_enumerated_property! {
    name: "Bidi_Class";
    short_name: "bc";
    ident: BidiClass;
    data_marker: crate::provider::PropertyEnumBidiClassV1;
    singleton: SINGLETON_PROPERTY_ENUM_BIDI_CLASS_V1;
    ule_ty: u8;
}

/// Enumerated property `Block`.
///
/// See Section 3.4, D10B in The Unicode Standard.
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::Block};
///
/// assert_eq!(
///     CodePointMapData::<Block>::new().get('α'),
///     Block::GreekAndCoptic,
/// );
/// assert_eq!(CodePointMapData::<Block>::new().get('a'), Block::BasicLatin,);
/// ```
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct Block(pub(crate) u16);

impl Default for Block {
    fn default() -> Self {
        Self::NoBlock
    }
}

make_enumerated_property! {
    name: "Block";
    short_name: "blk";
    ident: Block;
    data_marker: crate::provider::PropertyEnumBlockV1;
    singleton: SINGLETON_PROPERTY_ENUM_BLOCK_V1;
    ule_ty: <u16 as zerovec::ule::AsULE>::ULE;
}

/// Enumerated property `Numeric_Type`.
///
/// See Section 4.6, Numeric Value in The Unicode Standard for the summary of
/// each property value.
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::NumericType};
///
/// assert_eq!(
///     CodePointMapData::<NumericType>::new().get('0'),
///     NumericType::Decimal,
/// ); // U+0030
/// assert_eq!(
///     CodePointMapData::<NumericType>::new().get('½'),
///     NumericType::Numeric,
/// ); // U+00BD
/// ```
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct NumericType(pub(crate) u8);

impl NumericType {
    /// Returns an ICU4C `UNumericType` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn to_icu4c_value(self) -> u8 {
        self.0
    }
    /// Constructor from an ICU4C `UNumericType` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn from_icu4c_value(value: u8) -> Self {
        Self(value)
    }
}

impl Default for NumericType {
    fn default() -> Self {
        Self::None
    }
}

make_enumerated_property! {
    name: "Numeric_Type";
    short_name: "nt";
    ident: NumericType;
    data_marker: crate::provider::PropertyEnumNumericTypeV1;
    singleton: SINGLETON_PROPERTY_ENUM_NUMERIC_TYPE_V1;
    ule_ty: u8;
}

/// Enumerated property `General_Category`.
///
/// `General_Category` specifies the most general classification of a code point, usually
/// determined based on the primary characteristic of the assigned character. For example, is the
/// character a letter, a mark, a number, punctuation, or a symbol, and if so, of what type?
///
/// `GeneralCategory` only supports specific subcategories (eg `UppercaseLetter`).
/// It does not support grouped categories (eg `Letter`). For grouped categories, use [`GeneralCategoryGroup`].
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::GeneralCategory};
///
/// assert_eq!(
///     CodePointMapData::<GeneralCategory>::new().get('木'),
///     GeneralCategory::OtherLetter
/// ); // U+6728
/// assert_eq!(
///     CodePointMapData::<GeneralCategory>::new().get('🎃'),
///     GeneralCategory::OtherSymbol
/// ); // U+1F383 JACK-O-LANTERN
/// ```
pub use crate::enum_values::GeneralCategory;

#[allow(clippy::derivable_impls)] // declaration is codegen'd
impl Default for GeneralCategory {
    fn default() -> Self {
        Self::Unassigned
    }
}

#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Debug, Hash, Default)]
/// Error value for `impl TryFrom<u8> for GeneralCategory`.
#[non_exhaustive]
pub struct GeneralCategoryOutOfBoundsError;

impl TryFrom<u8> for GeneralCategory {
    type Error = GeneralCategoryOutOfBoundsError;
    /// Construct this [`GeneralCategory`] from an integer, returning
    /// an error if it is out of bounds
    fn try_from(val: u8) -> Result<Self, GeneralCategoryOutOfBoundsError> {
        GeneralCategory::new_from_u8(val).ok_or(GeneralCategoryOutOfBoundsError)
    }
}

make_enumerated_property! {
    name: "General_Category";
    short_name: "gc";
    ident: GeneralCategory;
    data_marker: crate::provider::PropertyEnumGeneralCategoryV1;
    singleton: SINGLETON_PROPERTY_ENUM_GENERAL_CATEGORY_V1;
}

/// Groupings of multiple `General_Category` property values.
///
/// Instances of `GeneralCategoryGroup` represent the defined multi-category
/// values that are useful for users in certain contexts, such as regex. In
/// other words, unlike [`GeneralCategory`], this supports groups of general
/// categories: for example, `Letter` /// is the union of `UppercaseLetter`,
/// `LowercaseLetter`, etc.
///
/// See <https://www.unicode.org/reports/tr44/> .
///
/// The discriminants correspond to the `U_GC_XX_MASK` constants in ICU4C.
/// Unlike [`GeneralCategory`], this supports groups of general categories: for example, `Letter`
/// is the union of `UppercaseLetter`, `LowercaseLetter`, etc.
///
/// See `UCharCategory` and `U_GET_GC_MASK` in ICU4C.
#[derive(Copy, Clone, PartialEq, Debug, Eq)]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct GeneralCategoryGroup(pub(crate) u32);

impl crate::private::Sealed for GeneralCategoryGroup {}

use GeneralCategory as GC;
use GeneralCategoryGroup as GCG;

#[allow(non_upper_case_globals, missing_docs)]
impl GeneralCategoryGroup {
    /// An uppercase letter
    pub const UppercaseLetter: GeneralCategoryGroup = GCG(1 << (GC::UppercaseLetter as u32));
    pub const Lu: GeneralCategoryGroup = Self::UppercaseLetter;
    /// A lowercase letter
    pub const Ll: Self = Self::LowercaseLetter;
    pub const LowercaseLetter: GeneralCategoryGroup = GCG(1 << (GC::LowercaseLetter as u32));
    /// A digraphic letter, with first part uppercase
    pub const TitlecaseLetter: GeneralCategoryGroup = GCG(1 << (GC::TitlecaseLetter as u32));
    pub const Lt: Self = Self::TitlecaseLetter;
    /// A modifier letter
    pub const ModifierLetter: GeneralCategoryGroup = GCG(1 << (GC::ModifierLetter as u32));
    pub const Lm: Self = Self::ModifierLetter;
    /// Other letters, including syllables and ideographs
    pub const OtherLetter: GeneralCategoryGroup = GCG(1 << (GC::OtherLetter as u32));
    pub const Lo: Self = Self::OtherLetter;
    /// The union of `UppercaseLetter`, `LowercaseLetter`, and `TitlecaseLetter`
    pub const CasedLetter: GeneralCategoryGroup = GCG((1 << (GC::UppercaseLetter as u32))
        | (1 << (GC::LowercaseLetter as u32))
        | (1 << (GC::TitlecaseLetter as u32)));
    pub const LC: Self = Self::CasedLetter;
    /// The union of all letter categories
    pub const Letter: GeneralCategoryGroup = GCG((1 << (GC::UppercaseLetter as u32))
        | (1 << (GC::LowercaseLetter as u32))
        | (1 << (GC::TitlecaseLetter as u32))
        | (1 << (GC::ModifierLetter as u32))
        | (1 << (GC::OtherLetter as u32)));
    pub const L: Self = Self::Letter;

    /// A nonspacing combining mark (zero advance width)
    pub const NonspacingMark: GeneralCategoryGroup = GCG(1 << (GC::NonspacingMark as u32));
    pub const Mn: Self = Self::NonspacingMark;
    /// An enclosing combining mark
    pub const EnclosingMark: GeneralCategoryGroup = GCG(1 << (GC::EnclosingMark as u32));
    pub const Me: Self = Self::EnclosingMark;
    /// A spacing combining mark (positive advance width)
    pub const SpacingMark: GeneralCategoryGroup = GCG(1 << (GC::SpacingMark as u32));
    pub const Mc: Self = Self::SpacingMark;
    /// The union of all mark categories
    pub const Mark: GeneralCategoryGroup = GCG((1 << (GC::NonspacingMark as u32))
        | (1 << (GC::EnclosingMark as u32))
        | (1 << (GC::SpacingMark as u32)));
    pub const M: Self = Self::Mark;

    /// A decimal digit
    pub const DecimalNumber: GeneralCategoryGroup = GCG(1 << (GC::DecimalNumber as u32));
    pub const Nd: Self = Self::DecimalNumber;
    /// A letterlike numeric character
    pub const LetterNumber: GeneralCategoryGroup = GCG(1 << (GC::LetterNumber as u32));
    pub const Nl: Self = Self::LetterNumber;
    /// A numeric character of other type
    pub const OtherNumber: GeneralCategoryGroup = GCG(1 << (GC::OtherNumber as u32));
    pub const No: Self = Self::OtherNumber;
    /// The union of all number categories
    pub const Number: GeneralCategoryGroup = GCG((1 << (GC::DecimalNumber as u32))
        | (1 << (GC::LetterNumber as u32))
        | (1 << (GC::OtherNumber as u32)));
    pub const N: Self = Self::Number;

    /// A space character (of various non-zero widths)
    pub const SpaceSeparator: GeneralCategoryGroup = GCG(1 << (GC::SpaceSeparator as u32));
    pub const Zs: Self = Self::SpaceSeparator;
    /// U+2028 LINE SEPARATOR only
    pub const LineSeparator: GeneralCategoryGroup = GCG(1 << (GC::LineSeparator as u32));
    pub const Zl: Self = Self::LineSeparator;
    /// U+2029 PARAGRAPH SEPARATOR only
    pub const ParagraphSeparator: GeneralCategoryGroup = GCG(1 << (GC::ParagraphSeparator as u32));
    pub const Zp: Self = Self::ParagraphSeparator;
    /// The union of all separator categories
    pub const Separator: GeneralCategoryGroup = GCG((1 << (GC::SpaceSeparator as u32))
        | (1 << (GC::LineSeparator as u32))
        | (1 << (GC::ParagraphSeparator as u32)));
    pub const Z: Self = Self::Separator;

    /// A C0 or C1 control code
    pub const Control: GeneralCategoryGroup = GCG(1 << (GC::Control as u32));
    pub const Cc: Self = Self::Control;
    /// A format control character
    pub const Format: GeneralCategoryGroup = GCG(1 << (GC::Format as u32));
    pub const Cf: Self = Self::Format;
    /// A private-use character
    pub const PrivateUse: GeneralCategoryGroup = GCG(1 << (GC::PrivateUse as u32));
    pub const Co: Self = Self::PrivateUse;
    /// A surrogate code point
    pub const Surrogate: GeneralCategoryGroup = GCG(1 << (GC::Surrogate as u32));
    pub const Cs: Self = Self::Surrogate;
    /// A reserved unassigned code point or a noncharacter
    pub const Unassigned: GeneralCategoryGroup = GCG(1 << (GC::Unassigned as u32));
    pub const Cn: Self = Self::Unassigned;
    /// The union of all control code, reserved, and unassigned categories
    pub const Other: GeneralCategoryGroup = GCG((1 << (GC::Control as u32))
        | (1 << (GC::Format as u32))
        | (1 << (GC::PrivateUse as u32))
        | (1 << (GC::Surrogate as u32))
        | (1 << (GC::Unassigned as u32)));
    pub const C: Self = Self::Other;

    /// A dash or hyphen punctuation mark
    pub const DashPunctuation: GeneralCategoryGroup = GCG(1 << (GC::DashPunctuation as u32));
    pub const Pd: Self = Self::DashPunctuation;
    /// An opening punctuation mark (of a pair)
    pub const OpenPunctuation: GeneralCategoryGroup = GCG(1 << (GC::OpenPunctuation as u32));
    pub const Ps: Self = Self::OpenPunctuation;
    /// A closing punctuation mark (of a pair)
    pub const ClosePunctuation: GeneralCategoryGroup = GCG(1 << (GC::ClosePunctuation as u32));
    pub const Pe: Self = Self::ClosePunctuation;
    /// A connecting punctuation mark, like a tie
    pub const ConnectorPunctuation: GeneralCategoryGroup =
        GCG(1 << (GC::ConnectorPunctuation as u32));
    pub const Pc: Self = Self::ConnectorPunctuation;
    /// An initial quotation mark
    pub const InitialPunctuation: GeneralCategoryGroup = GCG(1 << (GC::InitialPunctuation as u32));
    pub const Pi: Self = Self::InitialPunctuation;
    /// A final quotation mark
    pub const FinalPunctuation: GeneralCategoryGroup = GCG(1 << (GC::FinalPunctuation as u32));
    pub const Pf: Self = Self::FinalPunctuation;
    /// A punctuation mark of other type
    pub const OtherPunctuation: GeneralCategoryGroup = GCG(1 << (GC::OtherPunctuation as u32));
    pub const Po: Self = Self::OtherPunctuation;
    /// The union of all punctuation categories
    pub const Punctuation: GeneralCategoryGroup = GCG((1 << (GC::DashPunctuation as u32))
        | (1 << (GC::OpenPunctuation as u32))
        | (1 << (GC::ClosePunctuation as u32))
        | (1 << (GC::ConnectorPunctuation as u32))
        | (1 << (GC::OtherPunctuation as u32))
        | (1 << (GC::InitialPunctuation as u32))
        | (1 << (GC::FinalPunctuation as u32)));
    pub const P: Self = Self::Punctuation;

    /// A symbol of mathematical use
    pub const MathSymbol: GeneralCategoryGroup = GCG(1 << (GC::MathSymbol as u32));
    pub const Sm: Self = Self::MathSymbol;
    /// A currency sign
    pub const CurrencySymbol: GeneralCategoryGroup = GCG(1 << (GC::CurrencySymbol as u32));
    pub const Sc: Self = Self::CurrencySymbol;
    /// A non-letterlike modifier symbol
    pub const ModifierSymbol: GeneralCategoryGroup = GCG(1 << (GC::ModifierSymbol as u32));
    pub const Sk: Self = Self::ModifierSymbol;
    /// A symbol of other type
    pub const OtherSymbol: GeneralCategoryGroup = GCG(1 << (GC::OtherSymbol as u32));
    pub const So: Self = Self::OtherSymbol;
    /// The union of all symbol categories
    pub const Symbol: GeneralCategoryGroup = GCG((1 << (GC::MathSymbol as u32))
        | (1 << (GC::CurrencySymbol as u32))
        | (1 << (GC::ModifierSymbol as u32))
        | (1 << (GC::OtherSymbol as u32)));
    pub const S: Self = Self::Symbol;

    const ALL: u32 = (1 << (GC::FinalPunctuation as u32 + 1)) - 1;

    #[cfg(feature = "datagen")]
    #[doc(hidden)]
    pub fn names() -> impl Iterator<Item = (&'static str, Self)> {
        [
            ("Lu", Self::UppercaseLetter),
            ("Ll", Self::LowercaseLetter),
            ("Lt", Self::TitlecaseLetter),
            ("Lm", Self::ModifierLetter),
            ("Lo", Self::OtherLetter),
            ("LC", Self::CasedLetter),
            ("L", Self::Letter),
            ("Mn", Self::NonspacingMark),
            ("Me", Self::EnclosingMark),
            ("Mc", Self::SpacingMark),
            ("M", Self::Mark),
            ("Nd", Self::DecimalNumber),
            ("Nl", Self::LetterNumber),
            ("No", Self::OtherNumber),
            ("N", Self::Number),
            ("Zs", Self::SpaceSeparator),
            ("Zl", Self::LineSeparator),
            ("Zp", Self::ParagraphSeparator),
            ("Z", Self::Separator),
            ("Cc", Self::Control),
            ("Cf", Self::Format),
            ("Co", Self::PrivateUse),
            ("Cs", Self::Surrogate),
            ("Cn", Self::Unassigned),
            ("C", Self::Other),
            ("Pd", Self::DashPunctuation),
            ("Ps", Self::OpenPunctuation),
            ("Pe", Self::ClosePunctuation),
            ("Pc", Self::ConnectorPunctuation),
            ("Pi", Self::InitialPunctuation),
            ("Pf", Self::FinalPunctuation),
            ("Po", Self::OtherPunctuation),
            ("P", Self::Punctuation),
            ("Sm", Self::MathSymbol),
            ("Sc", Self::CurrencySymbol),
            ("Sk", Self::ModifierSymbol),
            ("So", Self::OtherSymbol),
            ("S", Self::Symbol),
        ]
        .into_iter()
    }

    /// Return whether the code point belongs in the provided multi-value category.
    ///
    /// ```
    /// use icu::properties::CodePointMapData;
    /// use icu::properties::props::{GeneralCategory, GeneralCategoryGroup};
    ///
    /// let gc = CodePointMapData::<GeneralCategory>::new();
    ///
    /// assert_eq!(gc.get('A'), GeneralCategory::UppercaseLetter);
    /// assert!(GeneralCategoryGroup::CasedLetter.contains(gc.get('A')));
    ///
    /// // U+0B1E ORIYA LETTER NYA
    /// assert_eq!(gc.get('ଞ'), GeneralCategory::OtherLetter);
    /// assert!(GeneralCategoryGroup::Letter.contains(gc.get('ଞ')));
    /// assert!(!GeneralCategoryGroup::CasedLetter.contains(gc.get('ଞ')));
    ///
    /// // U+0301 COMBINING ACUTE ACCENT
    /// assert_eq!(gc.get('\u{0301}'), GeneralCategory::NonspacingMark);
    /// assert!(GeneralCategoryGroup::Mark.contains(gc.get('\u{0301}')));
    /// assert!(!GeneralCategoryGroup::Letter.contains(gc.get('\u{0301}')));
    ///
    /// assert_eq!(gc.get('0'), GeneralCategory::DecimalNumber);
    /// assert!(GeneralCategoryGroup::Number.contains(gc.get('0')));
    /// assert!(!GeneralCategoryGroup::Mark.contains(gc.get('0')));
    ///
    /// assert_eq!(gc.get('('), GeneralCategory::OpenPunctuation);
    /// assert!(GeneralCategoryGroup::Punctuation.contains(gc.get('(')));
    /// assert!(!GeneralCategoryGroup::Number.contains(gc.get('(')));
    ///
    /// // U+2713 CHECK MARK
    /// assert_eq!(gc.get('✓'), GeneralCategory::OtherSymbol);
    /// assert!(GeneralCategoryGroup::Symbol.contains(gc.get('✓')));
    /// assert!(!GeneralCategoryGroup::Punctuation.contains(gc.get('✓')));
    ///
    /// assert_eq!(gc.get(' '), GeneralCategory::SpaceSeparator);
    /// assert!(GeneralCategoryGroup::Separator.contains(gc.get(' ')));
    /// assert!(!GeneralCategoryGroup::Symbol.contains(gc.get(' ')));
    ///
    /// // U+E007F CANCEL TAG
    /// assert_eq!(gc.get('\u{E007F}'), GeneralCategory::Format);
    /// assert!(GeneralCategoryGroup::Other.contains(gc.get('\u{E007F}')));
    /// assert!(!GeneralCategoryGroup::Separator.contains(gc.get('\u{E007F}')));
    /// ```
    pub const fn contains(self, val: GeneralCategory) -> bool {
        0 != (1 << (val as u32)) & self.0
    }

    /// Produce a `GeneralCategoryGroup` that is the inverse of this one
    ///
    /// # Example
    ///
    /// ```rust
    /// use icu::properties::props::{GeneralCategory, GeneralCategoryGroup};
    ///
    /// let letter = GeneralCategoryGroup::Letter;
    /// let not_letter = letter.complement();
    ///
    /// assert!(not_letter.contains(GeneralCategory::MathSymbol));
    /// assert!(!letter.contains(GeneralCategory::MathSymbol));
    /// assert!(not_letter.contains(GeneralCategory::OtherPunctuation));
    /// assert!(!letter.contains(GeneralCategory::OtherPunctuation));
    /// assert!(!not_letter.contains(GeneralCategory::UppercaseLetter));
    /// assert!(letter.contains(GeneralCategory::UppercaseLetter));
    /// ```
    pub const fn complement(self) -> Self {
        // Mask off things not in Self::ALL to guarantee the mask
        // values stay in-range
        GeneralCategoryGroup(!self.0 & Self::ALL)
    }

    /// Return the group representing all `GeneralCategory` values
    ///
    /// # Example
    ///
    /// ```rust
    /// use icu::properties::props::{GeneralCategory, GeneralCategoryGroup};
    ///
    /// let all = GeneralCategoryGroup::all();
    ///
    /// assert!(all.contains(GeneralCategory::MathSymbol));
    /// assert!(all.contains(GeneralCategory::OtherPunctuation));
    /// assert!(all.contains(GeneralCategory::UppercaseLetter));
    /// ```
    pub const fn all() -> Self {
        Self(Self::ALL)
    }

    /// Return the empty group
    ///
    /// # Example
    ///
    /// ```rust
    /// use icu::properties::props::{GeneralCategory, GeneralCategoryGroup};
    ///
    /// let empty = GeneralCategoryGroup::empty();
    ///
    /// assert!(!empty.contains(GeneralCategory::MathSymbol));
    /// assert!(!empty.contains(GeneralCategory::OtherPunctuation));
    /// assert!(!empty.contains(GeneralCategory::UppercaseLetter));
    /// ```
    pub const fn empty() -> Self {
        Self(0)
    }

    /// Take the union of two groups
    ///
    /// # Example
    ///
    /// ```rust
    /// use icu::properties::props::{GeneralCategory, GeneralCategoryGroup};
    ///
    /// let letter = GeneralCategoryGroup::Letter;
    /// let symbol = GeneralCategoryGroup::Symbol;
    /// let union = letter.union(symbol);
    ///
    /// assert!(union.contains(GeneralCategory::MathSymbol));
    /// assert!(!union.contains(GeneralCategory::OtherPunctuation));
    /// assert!(union.contains(GeneralCategory::UppercaseLetter));
    /// ```
    pub const fn union(self, other: Self) -> Self {
        Self(self.0 | other.0)
    }

    /// Take the intersection of two groups
    ///
    /// # Example
    ///
    /// ```rust
    /// use icu::properties::props::{GeneralCategory, GeneralCategoryGroup};
    ///
    /// let letter = GeneralCategoryGroup::Letter;
    /// let lu = GeneralCategoryGroup::UppercaseLetter;
    /// let intersection = letter.intersection(lu);
    ///
    /// assert!(!intersection.contains(GeneralCategory::MathSymbol));
    /// assert!(!intersection.contains(GeneralCategory::OtherPunctuation));
    /// assert!(intersection.contains(GeneralCategory::UppercaseLetter));
    /// assert!(!intersection.contains(GeneralCategory::LowercaseLetter));
    /// ```
    pub const fn intersection(self, other: Self) -> Self {
        Self(self.0 & other.0)
    }
}

impl From<GeneralCategory> for GeneralCategoryGroup {
    fn from(subcategory: GeneralCategory) -> Self {
        GeneralCategoryGroup(1 << (subcategory as u32))
    }
}
impl From<u32> for GeneralCategoryGroup {
    fn from(mask: u32) -> Self {
        // Mask off things not in Self::ALL to guarantee the mask
        // values stay in-range
        GeneralCategoryGroup(mask & Self::ALL)
    }
}
impl From<GeneralCategoryGroup> for u32 {
    fn from(group: GeneralCategoryGroup) -> Self {
        group.0
    }
}

/// Enumerated property Script.
///
/// This is used with both the Script and `Script_Extensions` Unicode properties.
/// Each character is assigned a single Script, but characters that are used in
/// a particular subset of scripts will be in more than one `Script_Extensions` set.
/// For example, `DEVANAGARI DIGIT NINE` has `Script=Devanagari`, but is also in the
/// `Script_Extensions` set for `Dogra`, `Kaithi`, and `Mahajani`. If you are trying to
/// determine whether a code point belongs to a certain script, you should use
/// [`ScriptWithExtensionsBorrowed::has_script`].
///
/// For more information, see UAX #24: <https://www.unicode.org/reports/tr24/>.
///
/// Additional constants are provided for ISO 15924 script codes, even if these are not encoded in
/// Unicode. For example, `Han` is a Unicode script (corresponding to the ISO 15924 code `Hani`),
/// but ISO 15924 also defines `Hans` and `Hant` for simplified and traditional Han.
///
/// Such constants are documented as non-Unicode constants, and are not returned as the `Script`
/// property for any code point.
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::Script};
///
/// assert_eq!(CodePointMapData::<Script>::new().get('木'), Script::Han);  // U+6728
/// assert_eq!(CodePointMapData::<Script>::new().get('🎃'), Script::Common);  // U+1F383 JACK-O-LANTERN
/// ```
/// [`ScriptWithExtensionsBorrowed::has_script`]: crate::script::ScriptWithExtensionsBorrowed::has_script
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct Script(pub(crate) u16);

impl Script {
    /// Returns an ICU4C `UScriptCode` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn to_icu4c_value(self) -> u16 {
        self.0
    }
    /// Constructor from an ICU4C `UScriptCode` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn from_icu4c_value(value: u16) -> Self {
        Self(value)
    }
    /// Deprecated: non-canonical spelling
    #[deprecated(since = "2.3.0", note = "use Script::Ethiopic instead")]
    #[allow(non_upper_case_globals)]
    pub const Ethiopian: Self = Self::Ethiopic;
    /// Deprecated: non-canonical spelling
    #[deprecated(since = "2.3.0", note = "use Script::ArabicNastaliq instead")]
    #[allow(non_upper_case_globals)]
    pub const Nastaliq: Self = Self::ArabicNastaliq;
    /// Deprecated: non-canonical spelling
    #[deprecated(since = "2.4.0", note = "use Script::Blissymbols instead")]
    #[allow(non_upper_case_globals)]
    pub const BlisSymbols: Self = Self::Blissymbols;
}

impl Default for Script {
    fn default() -> Self {
        Self::Unknown
    }
}

impl Script {
    // Doesn't actually exist!
    #[doc(hidden)]
    #[allow(non_upper_case_globals)]
    #[deprecated]
    // Some high value that ICU4C will not use anytime soon
    pub const Chisoi: Script = Self(60_000);
}

/// ✨ *Enabled with the `compiled_data` Cargo feature.*
#[cfg(feature = "compiled_data")]
impl From<Script> for icu_locale_core::subtags::Script {
    fn from(value: Script) -> Self {
        crate::PropertyNamesShort::new()
            .get_locale_script(value)
            .unwrap_or(icu_locale_core::subtags::script!("Zzzz"))
    }
}

/// ✨ *Enabled with the `compiled_data` Cargo feature.*
#[cfg(feature = "compiled_data")]
impl From<icu_locale_core::subtags::Script> for Script {
    fn from(value: icu_locale_core::subtags::Script) -> Self {
        crate::PropertyParser::new()
            .get_strict(value.as_str())
            .unwrap_or(Self::Unknown)
    }
}

make_enumerated_property! {
    name: "Script";
    short_name: "sc";
    ident: Script;
    data_marker: crate::provider::PropertyEnumScriptV1;
    singleton: SINGLETON_PROPERTY_ENUM_SCRIPT_V1;
    ule_ty: <u16 as zerovec::ule::AsULE>::ULE;
}

/// Enumerated property `Hangul_Syllable_Type`
///
/// The Unicode standard provides both precomposed Hangul syllables and conjoining Jamo to compose
/// arbitrary Hangul syllables. This property provides that ontology of Hangul code points.
///
/// For more information, see the [Unicode Korean FAQ](https://www.unicode.org/faq/korean.html).
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::HangulSyllableType};
///
/// assert_eq!(
///     CodePointMapData::<HangulSyllableType>::new().get('ᄀ'),
///     HangulSyllableType::LeadingJamo
/// ); // U+1100
/// assert_eq!(
///     CodePointMapData::<HangulSyllableType>::new().get('가'),
///     HangulSyllableType::LVSyllable
/// ); // U+AC00
/// ```
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct HangulSyllableType(pub(crate) u8);

impl HangulSyllableType {
    /// Returns an ICU4C `UHangulSyllableType` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn to_icu4c_value(self) -> u8 {
        self.0
    }
    /// Constructor from an ICU4C `UHangulSyllableType` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn from_icu4c_value(value: u8) -> Self {
        Self(value)
    }
    /// Deprecated: non-canonical spelling
    #[deprecated(since = "2.3.0", note = "use HangulSyllableType::LVSyllable instead")]
    #[allow(non_upper_case_globals)]
    pub const LeadingVowelSyllable: Self = Self::LVSyllable;
    /// Deprecated: non-canonical spelling
    #[deprecated(since = "2.3.0", note = "use HangulSyllableType::LVTSyllable instead")]
    #[allow(non_upper_case_globals)]
    pub const LeadingVowelTrailingSyllable: Self = Self::LVTSyllable;
}

impl Default for HangulSyllableType {
    fn default() -> Self {
        Self::NotApplicable
    }
}

make_enumerated_property! {
    name: "Hangul_Syllable_Type";
    short_name: "hst";
    ident: HangulSyllableType;
    data_marker: crate::provider::PropertyEnumHangulSyllableTypeV1;
    singleton: SINGLETON_PROPERTY_ENUM_HANGUL_SYLLABLE_TYPE_V1;
    ule_ty: u8;

}

/// Enumerated property `East_Asian_Width`.
///
/// See "Definition" in UAX #11 for the summary of each property value:
/// <https://www.unicode.org/reports/tr11/#Definitions>
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::EastAsianWidth};
///
/// assert_eq!(
///     CodePointMapData::<EastAsianWidth>::new().get('ｱ'),
///     EastAsianWidth::Halfwidth
/// ); // U+FF71: Halfwidth Katakana Letter A
/// assert_eq!(
///     CodePointMapData::<EastAsianWidth>::new().get('ア'),
///     EastAsianWidth::Wide
/// ); //U+30A2: Katakana Letter A
/// ```
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct EastAsianWidth(pub(crate) u8);

impl EastAsianWidth {
    /// Returns an ICU4C `UEastAsianWidth` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn to_icu4c_value(self) -> u8 {
        self.0
    }
    /// Constructor from an ICU4C `UEastAsianWidth` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn from_icu4c_value(value: u8) -> Self {
        Self(value)
    }
}

impl Default for EastAsianWidth {
    fn default() -> Self {
        Self::Neutral
    }
}

make_enumerated_property! {
    name: "East_Asian_Width";
    short_name: "ea";
    ident: EastAsianWidth;
    data_marker: crate::provider::PropertyEnumEastAsianWidthV1;
    singleton: SINGLETON_PROPERTY_ENUM_EAST_ASIAN_WIDTH_V1;
    ule_ty: u8;
}

/// Enumerated property `Line_Break`.
///
/// See "Line Breaking Properties" in UAX #14 for the summary of each property
/// value: <https://www.unicode.org/reports/tr14/#Properties>
///
/// The numeric value is compatible with `ULineBreak` in ICU4C.
///
/// **Note:** Use `icu::segmenter` for an all-in-one break iterator implementation.
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::LineBreak};
///
/// assert_eq!(
///     CodePointMapData::<LineBreak>::new().get(')'),
///     LineBreak::CloseParenthesis
/// ); // U+0029: Right Parenthesis
/// assert_eq!(
///     CodePointMapData::<LineBreak>::new().get('ぁ'),
///     LineBreak::ConditionalJapaneseStarter
/// ); //U+3041: Hiragana Letter Small A
/// ```
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct LineBreak(pub(crate) u8);

impl LineBreak {
    /// Returns an ICU4C `ULineBreak` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn to_icu4c_value(self) -> u8 {
        self.0
    }
    /// Constructor from an ICU4C `ULineBreak` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn from_icu4c_value(value: u8) -> Self {
        Self(value)
    }
}

impl Default for LineBreak {
    fn default() -> Self {
        Self::Unknown
    }
}

make_enumerated_property! {
    name: "Line_Break";
    short_name: "lb";
    ident: LineBreak;
    data_marker: crate::provider::PropertyEnumLineBreakV1;
    singleton: SINGLETON_PROPERTY_ENUM_LINE_BREAK_V1;
    ule_ty: u8;
}

/// Enumerated property `Grapheme_Cluster_Break`.
///
/// See "Default Grapheme Cluster Boundary Specification" in UAX #29 for the
/// summary of each property value:
/// <https://www.unicode.org/reports/tr29/#Default_Grapheme_Cluster_Table>
///
/// **Note:** Use `icu::segmenter` for an all-in-one break iterator implementation.
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::GraphemeClusterBreak};
///
/// assert_eq!(
///     CodePointMapData::<GraphemeClusterBreak>::new().get('🇦'),
///     GraphemeClusterBreak::RegionalIndicator
/// ); // U+1F1E6: Regional Indicator Symbol Letter A
/// assert_eq!(
///     CodePointMapData::<GraphemeClusterBreak>::new().get('ำ'),
///     GraphemeClusterBreak::SpacingMark
/// ); //U+0E33: Thai Character Sara Am
/// ```
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // this type is stable
#[repr(transparent)]
pub struct GraphemeClusterBreak(pub(crate) u8);

impl GraphemeClusterBreak {
    /// Returns an ICU4C `UGraphemeClusterBreak` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn to_icu4c_value(self) -> u8 {
        self.0
    }
    /// Constructor from an ICU4C `UGraphemeClusterBreak` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn from_icu4c_value(value: u8) -> Self {
        Self(value)
    }
}

impl Default for GraphemeClusterBreak {
    fn default() -> Self {
        Self::Other
    }
}

make_enumerated_property! {
    name: "Grapheme_Cluster_Break";
    short_name: "GCB";
    ident: GraphemeClusterBreak;
    data_marker: crate::provider::PropertyEnumGraphemeClusterBreakV1;
    singleton: SINGLETON_PROPERTY_ENUM_GRAPHEME_CLUSTER_BREAK_V1;
    ule_ty: u8;
}

/// Enumerated property `Word_Break`.
///
/// See "Default Word Boundary Specification" in UAX #29 for the summary of
/// each property value:
/// <https://www.unicode.org/reports/tr29/#Default_Word_Boundaries>.
///
/// **Note:** Use `icu::segmenter` for an all-in-one break iterator implementation.
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::WordBreak};
///
/// assert_eq!(
///     CodePointMapData::<WordBreak>::new().get('.'),
///     WordBreak::MidNumLet
/// ); // U+002E: Full Stop
/// assert_eq!(
///     CodePointMapData::<WordBreak>::new().get('，'),
///     WordBreak::MidNum
/// ); // U+FF0C: Fullwidth Comma
/// ```
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct WordBreak(pub(crate) u8);

impl WordBreak {
    /// Returns an ICU4C `UWordBreak` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn to_icu4c_value(self) -> u8 {
        self.0
    }
    /// Constructor from an ICU4C `UWordBreak` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn from_icu4c_value(value: u8) -> Self {
        Self(value)
    }
}

impl Default for WordBreak {
    fn default() -> Self {
        Self::Other
    }
}

make_enumerated_property! {
    name: "Word_Break";
    short_name: "WB";
    ident: WordBreak;
    data_marker: crate::provider::PropertyEnumWordBreakV1;
    singleton: SINGLETON_PROPERTY_ENUM_WORD_BREAK_V1;
    ule_ty: u8;
}

/// Enumerated property `Sentence_Break`.
///
/// See "Default Sentence Boundary Specification" in UAX #29 for the summary of
/// each property value:
/// <https://www.unicode.org/reports/tr29/#Default_Word_Boundaries>.
///
/// **Note:** Use `icu::segmenter` for an all-in-one break iterator implementation.
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::SentenceBreak};
///
/// assert_eq!(
///     CodePointMapData::<SentenceBreak>::new().get('９'),
///     SentenceBreak::Numeric
/// ); // U+FF19: Fullwidth Digit Nine
/// assert_eq!(
///     CodePointMapData::<SentenceBreak>::new().get(','),
///     SentenceBreak::SContinue
/// ); // U+002C: Comma
/// ```
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct SentenceBreak(pub(crate) u8);

impl SentenceBreak {
    /// Returns an ICU4C `USentenceBreak` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn to_icu4c_value(self) -> u8 {
        self.0
    }
    /// Constructor from an ICU4C `USentenceBreak` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn from_icu4c_value(value: u8) -> Self {
        Self(value)
    }
}

impl Default for SentenceBreak {
    fn default() -> Self {
        Self::Other
    }
}

make_enumerated_property! {
    name: "Sentence_Break";
    short_name: "SB";
    ident: SentenceBreak;
    data_marker: crate::provider::PropertyEnumSentenceBreakV1;
    singleton: SINGLETON_PROPERTY_ENUM_SENTENCE_BREAK_V1;
    ule_ty: u8;
}

/// Property `Canonical_Combining_Class`.
/// See UAX #15:
/// <https://www.unicode.org/reports/tr15/>.
///
/// **Note:** See `icu::normalizer::properties::CanonicalCombiningClassMap` for the preferred API
/// to look up the `Canonical_Combining_Class` property by scalar value.
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::CanonicalCombiningClass};
///
/// assert_eq!(
///     CodePointMapData::<CanonicalCombiningClass>::new().get('a'),
///     CanonicalCombiningClass::NotReordered
/// ); // U+0061: LATIN SMALL LETTER A
/// assert_eq!(
///     CodePointMapData::<CanonicalCombiningClass>::new().get('\u{0301}'),
///     CanonicalCombiningClass::Above
/// ); // U+0301: COMBINING ACUTE ACCENT
/// ```
//
// NOTE: The Pernosco debugger has special knowledge
// of this struct. Please do not change the bit layout
// or the crate-module-qualified name of this struct
// without coordination.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct CanonicalCombiningClass(pub u8);

impl CanonicalCombiningClass {
    /// Returns an ICU4C `UCanonicalCombiningClass` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn to_icu4c_value(self) -> u8 {
        self.0
    }
    /// Constructor from an ICU4C `UCanonicalCombiningClass` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn from_icu4c_value(value: u8) -> Self {
        Self(value)
    }
}

impl Default for CanonicalCombiningClass {
    fn default() -> Self {
        Self::NotReordered
    }
}

make_enumerated_property! {
    name: "Canonical_Combining_Class";
    short_name: "ccc";
    ident: CanonicalCombiningClass;
    data_marker: crate::provider::PropertyEnumCanonicalCombiningClassV1;
    singleton: SINGLETON_PROPERTY_ENUM_CANONICAL_COMBINING_CLASS_V1;
    ule_ty: u8;
}

/// Property `Indic_Conjunct_Break`.
/// See UAX #44:
/// <https://www.unicode.org/reports/tr44/#Indic_Conjunct_Break>.
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::IndicConjunctBreak};
///
/// assert_eq!(
///     CodePointMapData::<IndicConjunctBreak>::new().get('a'),
///     IndicConjunctBreak::None
/// );
/// assert_eq!(
///     CodePointMapData::<IndicConjunctBreak>::new().get('\u{094d}'),
///     IndicConjunctBreak::Linker
/// );
/// assert_eq!(
///     CodePointMapData::<IndicConjunctBreak>::new().get('\u{0915}'),
///     IndicConjunctBreak::Consonant
/// );
/// assert_eq!(
///     CodePointMapData::<IndicConjunctBreak>::new().get('\u{0300}'),
///     IndicConjunctBreak::Extend
/// );
/// ```
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct IndicConjunctBreak(pub(crate) u8);

impl IndicConjunctBreak {
    /// Returns an ICU4C `UIndicConjunctBreak` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn to_icu4c_value(self) -> u8 {
        self.0
    }
    /// Constructor from an ICU4C `UIndicConjunctBreak` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn from_icu4c_value(value: u8) -> Self {
        Self(value)
    }
}

impl Default for IndicConjunctBreak {
    fn default() -> Self {
        Self::None
    }
}

make_enumerated_property! {
    name: "Indic_Conjunct_Break";
    short_name: "InCB";
    ident: IndicConjunctBreak;
    data_marker: crate::provider::PropertyEnumIndicConjunctBreakV1;
    singleton: SINGLETON_PROPERTY_ENUM_INDIC_CONJUNCT_BREAK_V1;
    ule_ty: u8;
}

/// Property `Indic_Syllabic_Category`.
/// See UAX #44:
/// <https://www.unicode.org/reports/tr44/#Indic_Syllabic_Category>.
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::IndicSyllabicCategory};
///
/// assert_eq!(
///     CodePointMapData::<IndicSyllabicCategory>::new().get('a'),
///     IndicSyllabicCategory::Other
/// );
/// assert_eq!(
///     CodePointMapData::<IndicSyllabicCategory>::new().get('\u{0900}'),
///     IndicSyllabicCategory::Bindu
/// ); // U+0900: DEVANAGARI SIGN INVERTED CANDRABINDU
/// ```
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct IndicSyllabicCategory(pub(crate) u8);

impl IndicSyllabicCategory {
    /// Returns an ICU4C `UIndicSyllabicCategory` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn to_icu4c_value(self) -> u8 {
        self.0
    }
    /// Constructor from an ICU4C `UIndicSyllabicCategory` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn from_icu4c_value(value: u8) -> Self {
        Self(value)
    }
}

impl Default for IndicSyllabicCategory {
    fn default() -> Self {
        Self::Other
    }
}

make_enumerated_property! {
    name: "Indic_Syllabic_Category";
    short_name: "InSC";
    ident: IndicSyllabicCategory;
    data_marker: crate::provider::PropertyEnumIndicSyllabicCategoryV1;
    singleton: SINGLETON_PROPERTY_ENUM_INDIC_SYLLABIC_CATEGORY_V1;
    ule_ty: u8;
}

/// Enumerated property `Joining_Group`.
///
/// See Section 9.2, Arabic Joining Groups in The Unicode Standard for the summary of
/// each property value.
///
/// ```
/// use icu::properties::{CodePointMapData, props::JoiningGroup};
///
/// assert_eq!(
///     CodePointMapData::<JoiningGroup>::new().get('ع'),
///     JoiningGroup::Ain,
/// ); // U+0639: Arabic Letter Ain
/// assert_eq!(
///     CodePointMapData::<JoiningGroup>::new().get('ظ'),
///     JoiningGroup::Tah,
/// ); // U+0638: Arabic Letter Zah
/// ```
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct JoiningGroup(pub(crate) u8);

impl JoiningGroup {
    /// Returns an ICU4C `UJoiningType` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn to_icu4c_value(self) -> u8 {
        self.0
    }
    /// Constructor from an ICU4C `UJoiningType` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn from_icu4c_value(value: u8) -> Self {
        Self(value)
    }
}

impl Default for JoiningGroup {
    fn default() -> Self {
        Self::NoJoiningGroup
    }
}

make_enumerated_property! {
    name: "Joining_Group";
    short_name: "jg";
    ident: JoiningGroup;
    data_marker: crate::provider::PropertyEnumJoiningGroupV1;
    singleton: SINGLETON_PROPERTY_ENUM_JOINING_GROUP_V1;
    ule_ty: u8;
}

/// Enumerated property `Joining_Type`.
///
/// See Section 9.2, Arabic Cursive Joining in The Unicode Standard for the summary of
/// each property value.
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::JoiningType};
///
/// assert_eq!(
///     CodePointMapData::<JoiningType>::new().get('ؠ'),
///     JoiningType::DualJoining
/// ); // U+0620: Arabic Letter Kashmiri Yeh
/// assert_eq!(
///     CodePointMapData::<JoiningType>::new().get('𐫍'),
///     JoiningType::LeftJoining
/// ); // U+10ACD: Manichaean Letter Heth
/// ```
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct JoiningType(pub(crate) u8);

impl JoiningType {
    /// Returns an ICU4C `UJoiningType` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn to_icu4c_value(self) -> u8 {
        self.0
    }
    /// Constructor from an ICU4C `UJoiningType` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn from_icu4c_value(value: u8) -> Self {
        Self(value)
    }
}

impl Default for JoiningType {
    fn default() -> Self {
        Self::NonJoining
    }
}

make_enumerated_property! {
    name: "Joining_Type";
    short_name: "jt";
    ident: JoiningType;
    data_marker: crate::provider::PropertyEnumJoiningTypeV1;
    singleton: SINGLETON_PROPERTY_ENUM_JOINING_TYPE_V1;
    ule_ty: u8;
}

/// Property `Vertical_Orientation`
///
/// See UTR #50:
/// <https://www.unicode.org/reports/tr50/#vo>
///
/// # Example
///
/// ```
/// use icu::properties::{CodePointMapData, props::VerticalOrientation};
///
/// assert_eq!(
///     CodePointMapData::<VerticalOrientation>::new().get('a'),
///     VerticalOrientation::Rotated
/// );
/// assert_eq!(
///     CodePointMapData::<VerticalOrientation>::new().get('§'),
///     VerticalOrientation::Upright
/// );
/// assert_eq!(
///     CodePointMapData::<VerticalOrientation>::new().get32(0x2329),
///     VerticalOrientation::TransformedRotated
/// );
/// assert_eq!(
///     CodePointMapData::<VerticalOrientation>::new().get32(0x3001),
///     VerticalOrientation::TransformedUpright
/// );
/// ```
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // newtype
#[repr(transparent)]
pub struct VerticalOrientation(pub(crate) u8);

impl VerticalOrientation {
    /// Returns an ICU4C `UVerticalOrientation` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn to_icu4c_value(self) -> u8 {
        self.0
    }
    /// Constructor from an ICU4C `UVerticalOrientation` value.
    #[deprecated(
        since = "2.3.0",
        note = "please comment on https://github.com/unicode-org/icu4x/issues/6067 if you need this"
    )]
    pub const fn from_icu4c_value(value: u8) -> Self {
        Self(value)
    }
}

impl Default for VerticalOrientation {
    fn default() -> Self {
        Self::Rotated
    }
}

make_enumerated_property! {
    name: "Vertical_Orientation";
    short_name: "vo";
    ident: VerticalOrientation;
    data_marker: crate::provider::PropertyEnumVerticalOrientationV1;
    singleton: SINGLETON_PROPERTY_ENUM_VERTICAL_ORIENTATION_V1;
    ule_ty: u8;
}

pub use crate::code_point_set::BinaryProperty;

macro_rules! make_binary_property {
    (
        name: $name:literal;
        short_name: $short_name:literal;
        ident: $ident:ident;
        data_marker: $data_marker:ty;
        singleton: $singleton:ident;
            $(#[$doc:meta])+
    ) => {
        $(#[$doc])+
        #[derive(Debug)]
        #[non_exhaustive]
        pub struct $ident;

        #[allow(deprecated)]
        impl crate::private::Sealed for $ident {}

        #[allow(deprecated)]
        impl BinaryProperty for $ident {
            type DataMarker = $data_marker;
            #[cfg(feature = "compiled_data")]
            const SINGLETON: &'static crate::provider::PropertyCodePointSet<'static> =
                &crate::provider::Baked::$singleton;
            const NAME: &'static [u8] = $name.as_bytes();
            const SHORT_NAME: &'static [u8] = $short_name.as_bytes();
        }
    };
}

make_binary_property! {
    name: "ASCII_Hex_Digit";
    short_name: "AHex";
    ident: AsciiHexDigit;
    data_marker: crate::provider::PropertyBinaryAsciiHexDigitV1;
    singleton: SINGLETON_PROPERTY_BINARY_ASCII_HEX_DIGIT_V1;
    /// ASCII characters commonly used for the representation of hexadecimal numbers.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::AsciiHexDigit;
    ///
    /// let ascii_hex_digit = CodePointSetData::new::<AsciiHexDigit>();
    ///
    /// assert!(ascii_hex_digit.contains('3'));
    /// assert!(!ascii_hex_digit.contains('੩'));  // U+0A69 GURMUKHI DIGIT THREE
    /// assert!(ascii_hex_digit.contains('A'));
    /// assert!(!ascii_hex_digit.contains('Ä'));  // U+00C4 LATIN CAPITAL LETTER A WITH DIAERESIS
    /// ```
}

make_binary_property! {
    name: "alnum";
    short_name: "alnum";
    ident: Alnum;
    data_marker: crate::provider::PropertyBinaryAlnumV1;
    singleton: SINGLETON_PROPERTY_BINARY_ALNUM_V1;
    /// Characters with the `Alphabetic` or `Decimal_Number` property.
    ///
    /// This is defined for POSIX compatibility.
}

make_binary_property! {
    name: "Alphabetic";
    short_name: "Alpha";
    ident: Alphabetic;
    data_marker: crate::provider::PropertyBinaryAlphabeticV1;
    singleton: SINGLETON_PROPERTY_BINARY_ALPHABETIC_V1;
    /// Alphabetic characters.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::Alphabetic;
    ///
    /// let alphabetic = CodePointSetData::new::<Alphabetic>();
    ///
    /// assert!(!alphabetic.contains('3'));
    /// assert!(!alphabetic.contains('੩'));  // U+0A69 GURMUKHI DIGIT THREE
    /// assert!(alphabetic.contains('A'));
    /// assert!(alphabetic.contains('Ä'));  // U+00C4 LATIN CAPITAL LETTER A WITH DIAERESIS
    /// ```

}

make_binary_property! {
    name: "Bidi_Control";
    short_name: "Bidi_C";
    ident: BidiControl;
    data_marker: crate::provider::PropertyBinaryBidiControlV1;
    singleton: SINGLETON_PROPERTY_BINARY_BIDI_CONTROL_V1;
    /// Format control characters which have specific functions in the Unicode Bidirectional
    /// Algorithm.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::BidiControl;
    ///
    /// let bidi_control = CodePointSetData::new::<BidiControl>();
    ///
    /// assert!(bidi_control.contains('\u{200F}'));  // RIGHT-TO-LEFT MARK
    /// assert!(!bidi_control.contains('ش'));  // U+0634 ARABIC LETTER SHEEN
    /// ```

}

make_binary_property! {
    name: "Bidi_Mirrored";
    short_name: "Bidi_M";
    ident: BidiMirrored;
    data_marker: crate::provider::PropertyBinaryBidiMirroredV1;
    singleton: SINGLETON_PROPERTY_BINARY_BIDI_MIRRORED_V1;
    /// Characters that are mirrored in bidirectional text.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::BidiMirrored;
    ///
    /// let bidi_mirrored = CodePointSetData::new::<BidiMirrored>();
    ///
    /// assert!(bidi_mirrored.contains('['));
    /// assert!(bidi_mirrored.contains(']'));
    /// assert!(bidi_mirrored.contains('∑'));  // U+2211 N-ARY SUMMATION
    /// assert!(!bidi_mirrored.contains('ཉ'));  // U+0F49 TIBETAN LETTER NYA
    /// ```

}

make_binary_property! {
    name: "blank";
    short_name: "blank";
    ident: Blank;
    data_marker: crate::provider::PropertyBinaryBlankV1;
    singleton: SINGLETON_PROPERTY_BINARY_BLANK_V1;
    /// Horizontal whitespace characters

}

make_binary_property! {
    name: "Cased";
    short_name: "Cased";
    ident: Cased;
    data_marker: crate::provider::PropertyBinaryCasedV1;
    singleton: SINGLETON_PROPERTY_BINARY_CASED_V1;
    /// Uppercase, lowercase, and titlecase characters.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::Cased;
    ///
    /// let cased = CodePointSetData::new::<Cased>();
    ///
    /// assert!(cased.contains('Ꙡ'));  // U+A660 CYRILLIC CAPITAL LETTER REVERSED TSE
    /// assert!(!cased.contains('ދ'));  // U+078B THAANA LETTER DHAALU
    /// ```

}

make_binary_property! {
    name: "Case_Ignorable";
    short_name: "CI";
    ident: CaseIgnorable;
    data_marker: crate::provider::PropertyBinaryCaseIgnorableV1;
    singleton: SINGLETON_PROPERTY_BINARY_CASE_IGNORABLE_V1;
    /// Characters which are ignored for casing purposes.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::CaseIgnorable;
    ///
    /// let case_ignorable = CodePointSetData::new::<CaseIgnorable>();
    ///
    /// assert!(case_ignorable.contains(':'));
    /// assert!(!case_ignorable.contains('λ'));  // U+03BB GREEK SMALL LETTER LAMBDA
    /// ```

}

make_binary_property! {
    name: "Full_Composition_Exclusion";
    short_name: "Comp_Ex";
    ident: FullCompositionExclusion;
    data_marker: crate::provider::PropertyBinaryFullCompositionExclusionV1;
    singleton: SINGLETON_PROPERTY_BINARY_FULL_COMPOSITION_EXCLUSION_V1;
    /// Characters that are excluded from composition.
    ///
    /// See <https://unicode.org/Public/UNIDATA/CompositionExclusions.txt>

}

make_binary_property! {
    name: "Changes_When_Casefolded";
    short_name: "CWCF";
    ident: ChangesWhenCasefolded;
    data_marker: crate::provider::PropertyBinaryChangesWhenCasefoldedV1;
    singleton: SINGLETON_PROPERTY_BINARY_CHANGES_WHEN_CASEFOLDED_V1;
    /// Characters whose normalized forms are not stable under case folding.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::ChangesWhenCasefolded;
    ///
    /// let changes_when_casefolded = CodePointSetData::new::<ChangesWhenCasefolded>();
    ///
    /// assert!(changes_when_casefolded.contains('ß'));  // U+00DF LATIN SMALL LETTER SHARP S
    /// assert!(!changes_when_casefolded.contains('ᜉ'));  // U+1709 TAGALOG LETTER PA
    /// ```

}

make_binary_property! {
    name: "Changes_When_Casemapped";
    short_name: "CWCM";
    ident: ChangesWhenCasemapped;
    data_marker: crate::provider::PropertyBinaryChangesWhenCasemappedV1;
    singleton: SINGLETON_PROPERTY_BINARY_CHANGES_WHEN_CASEMAPPED_V1;
    /// Characters which may change when they undergo case mapping.

}

make_binary_property! {
    name: "Changes_When_NFKC_Casefolded";
    short_name: "CWKCF";
    ident: ChangesWhenNfkcCasefolded;
    data_marker: crate::provider::PropertyBinaryChangesWhenNfkcCasefoldedV1;
    singleton: SINGLETON_PROPERTY_BINARY_CHANGES_WHEN_NFKC_CASEFOLDED_V1;
    /// Characters which are not identical to their `NFKC_Casefold` mapping.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::ChangesWhenNfkcCasefolded;
    ///
    /// let changes_when_nfkc_casefolded = CodePointSetData::new::<ChangesWhenNfkcCasefolded>();
    ///
    /// assert!(changes_when_nfkc_casefolded.contains('🄵'));  // U+1F135 SQUARED LATIN CAPITAL LETTER F
    /// assert!(!changes_when_nfkc_casefolded.contains('f'));
    /// ```

}

make_binary_property! {
    name: "Changes_When_Lowercased";
    short_name: "CWL";
    ident: ChangesWhenLowercased;
    data_marker: crate::provider::PropertyBinaryChangesWhenLowercasedV1;
    singleton: SINGLETON_PROPERTY_BINARY_CHANGES_WHEN_LOWERCASED_V1;
    /// Characters whose normalized forms are not stable under a `toLowercase` mapping.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::ChangesWhenLowercased;
    ///
    /// let changes_when_lowercased = CodePointSetData::new::<ChangesWhenLowercased>();
    ///
    /// assert!(changes_when_lowercased.contains('Ⴔ'));  // U+10B4 GEORGIAN CAPITAL LETTER PHAR
    /// assert!(!changes_when_lowercased.contains('ფ'));  // U+10E4 GEORGIAN LETTER PHAR
    /// ```

}

make_binary_property! {
    name: "Changes_When_Titlecased";
    short_name: "CWT";
    ident: ChangesWhenTitlecased;
    data_marker: crate::provider::PropertyBinaryChangesWhenTitlecasedV1;
    singleton: SINGLETON_PROPERTY_BINARY_CHANGES_WHEN_TITLECASED_V1;
    /// Characters whose normalized forms are not stable under a `toTitlecase` mapping.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::ChangesWhenTitlecased;
    ///
    /// let changes_when_titlecased = CodePointSetData::new::<ChangesWhenTitlecased>();
    ///
    /// assert!(changes_when_titlecased.contains('æ'));  // U+00E6 LATIN SMALL LETTER AE
    /// assert!(!changes_when_titlecased.contains('Æ'));  // U+00E6 LATIN CAPITAL LETTER AE
    /// ```

}

make_binary_property! {
    name: "Changes_When_Uppercased";
    short_name: "CWU";
    ident: ChangesWhenUppercased;
    data_marker: crate::provider::PropertyBinaryChangesWhenUppercasedV1;
    singleton: SINGLETON_PROPERTY_BINARY_CHANGES_WHEN_UPPERCASED_V1;
    /// Characters whose normalized forms are not stable under a `toUppercase` mapping.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::ChangesWhenUppercased;
    ///
    /// let changes_when_uppercased = CodePointSetData::new::<ChangesWhenUppercased>();
    ///
    /// assert!(changes_when_uppercased.contains('ւ'));  // U+0582 ARMENIAN SMALL LETTER YIWN
    /// assert!(!changes_when_uppercased.contains('Ւ'));  // U+0552 ARMENIAN CAPITAL LETTER YIWN
    /// ```

}

make_binary_property! {
    name: "Dash";
    short_name: "Dash";
    ident: Dash;
    data_marker: crate::provider::PropertyBinaryDashV1;
    singleton: SINGLETON_PROPERTY_BINARY_DASH_V1;
    /// Punctuation characters explicitly called out as dashes in the Unicode Standard, plus
    /// their compatibility equivalents.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::Dash;
    ///
    /// let dash = CodePointSetData::new::<Dash>();
    ///
    /// assert!(dash.contains('⸺'));  // U+2E3A TWO-EM DASH
    /// assert!(dash.contains('-'));  // U+002D
    /// assert!(!dash.contains('='));  // U+003D
    /// ```

}

make_binary_property! {
    name: "Deprecated";
    short_name: "Dep";
    ident: Deprecated;
    data_marker: crate::provider::PropertyBinaryDeprecatedV1;
    singleton: SINGLETON_PROPERTY_BINARY_DEPRECATED_V1;
    /// Deprecated characters.
    ///
    /// No characters will ever be removed from the standard, but the
    /// usage of deprecated characters is strongly discouraged.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::Deprecated;
    ///
    /// let deprecated = CodePointSetData::new::<Deprecated>();
    ///
    /// assert!(deprecated.contains('ឣ'));  // U+17A3 KHMER INDEPENDENT VOWEL QAQ
    /// assert!(!deprecated.contains('A'));
    /// ```

}

make_binary_property! {
    name: "Default_Ignorable_Code_Point";
    short_name: "DI";
    ident: DefaultIgnorableCodePoint;
    data_marker: crate::provider::PropertyBinaryDefaultIgnorableCodePointV1;
    singleton: SINGLETON_PROPERTY_BINARY_DEFAULT_IGNORABLE_CODE_POINT_V1;
    /// For programmatic determination of default ignorable code points.
    ///
    /// New characters that
    /// should be ignored in rendering (unless explicitly supported) will be assigned in these
    /// ranges, permitting programs to correctly handle the default rendering of such
    /// characters when not otherwise supported.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::DefaultIgnorableCodePoint;
    ///
    /// let default_ignorable_code_point = CodePointSetData::new::<DefaultIgnorableCodePoint>();
    ///
    /// assert!(default_ignorable_code_point.contains('\u{180B}'));  // MONGOLIAN FREE VARIATION SELECTOR ONE
    /// assert!(!default_ignorable_code_point.contains('E'));
    /// ```

}

make_binary_property! {
    name: "Diacritic";
    short_name: "Dia";
    ident: Diacritic;
    data_marker: crate::provider::PropertyBinaryDiacriticV1;
    singleton: SINGLETON_PROPERTY_BINARY_DIACRITIC_V1;
    /// Characters that linguistically modify the meaning of another character to which they apply.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::Diacritic;
    ///
    /// let diacritic = CodePointSetData::new::<Diacritic>();
    ///
    /// assert!(diacritic.contains('\u{05B3}'));  // HEBREW POINT HATAF QAMATS
    /// assert!(!diacritic.contains('א'));  // U+05D0 HEBREW LETTER ALEF
    /// ```

}

make_binary_property! {
    name: "Emoji_Modifier_Base";
    short_name: "EBase";
    ident: EmojiModifierBase;
    data_marker: crate::provider::PropertyBinaryEmojiModifierBaseV1;
    singleton: SINGLETON_PROPERTY_BINARY_EMOJI_MODIFIER_BASE_V1;
    /// Characters that can serve as a base for emoji modifiers.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::EmojiModifierBase;
    ///
    /// let emoji_modifier_base = CodePointSetData::new::<EmojiModifierBase>();
    ///
    /// assert!(emoji_modifier_base.contains('✊'));  // U+270A RAISED FIST
    /// assert!(!emoji_modifier_base.contains('⛰'));  // U+26F0 MOUNTAIN
    /// ```

}

make_binary_property! {
    name: "Emoji_Component";
    short_name: "EComp";
    ident: EmojiComponent;
    data_marker: crate::provider::PropertyBinaryEmojiComponentV1;
    singleton: SINGLETON_PROPERTY_BINARY_EMOJI_COMPONENT_V1;
    /// Characters used in emoji sequences that normally do not appear on emoji keyboards as
    /// separate choices, such as base characters for emoji keycaps.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::EmojiComponent;
    ///
    /// let emoji_component = CodePointSetData::new::<EmojiComponent>();
    ///
    /// assert!(emoji_component.contains('🇹'));  // U+1F1F9 REGIONAL INDICATOR SYMBOL LETTER T
    /// assert!(emoji_component.contains('\u{20E3}'));  // COMBINING ENCLOSING KEYCAP
    /// assert!(emoji_component.contains('7'));
    /// assert!(!emoji_component.contains('T'));
    /// ```

}

make_binary_property! {
    name: "Emoji_Modifier";
    short_name: "EMod";
    ident: EmojiModifier;
    data_marker: crate::provider::PropertyBinaryEmojiModifierV1;
    singleton: SINGLETON_PROPERTY_BINARY_EMOJI_MODIFIER_V1;
    /// Characters that are emoji modifiers.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::EmojiModifier;
    ///
    /// let emoji_modifier = CodePointSetData::new::<EmojiModifier>();
    ///
    /// assert!(emoji_modifier.contains('\u{1F3FD}'));  // EMOJI MODIFIER FITZPATRICK TYPE-4
    /// assert!(!emoji_modifier.contains('\u{200C}'));  // ZERO WIDTH NON-JOINER
    /// ```

}

make_binary_property! {
    name: "Emoji";
    short_name: "Emoji";
    ident: Emoji;
    data_marker: crate::provider::PropertyBinaryEmojiV1;
    singleton: SINGLETON_PROPERTY_BINARY_EMOJI_V1;
    /// Characters that are emoji.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::Emoji;
    ///
    /// let emoji = CodePointSetData::new::<Emoji>();
    ///
    /// assert!(emoji.contains('🔥'));  // U+1F525 FIRE
    /// assert!(!emoji.contains('V'));
    /// ```

}

make_binary_property! {
    name: "Emoji_Presentation";
    short_name: "EPres";
    ident: EmojiPresentation;
    data_marker: crate::provider::PropertyBinaryEmojiPresentationV1;
    singleton: SINGLETON_PROPERTY_BINARY_EMOJI_PRESENTATION_V1;
    /// Characters that have emoji presentation by default.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::EmojiPresentation;
    ///
    /// let emoji_presentation = CodePointSetData::new::<EmojiPresentation>();
    ///
    /// assert!(emoji_presentation.contains('🦬')); // U+1F9AC BISON
    /// assert!(!emoji_presentation.contains('♻'));  // U+267B BLACK UNIVERSAL RECYCLING SYMBOL
    /// ```

}

make_binary_property! {
    name: "Extender";
    short_name: "Ext";
    ident: Extender;
    data_marker: crate::provider::PropertyBinaryExtenderV1;
    singleton: SINGLETON_PROPERTY_BINARY_EXTENDER_V1;
    /// Characters whose principal function is to extend the value of a preceding alphabetic
    /// character or to extend the shape of adjacent characters.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::Extender;
    ///
    /// let extender = CodePointSetData::new::<Extender>();
    ///
    /// assert!(extender.contains('ヾ'));  // U+30FE KATAKANA VOICED ITERATION MARK
    /// assert!(extender.contains('ー'));  // U+30FC KATAKANA-HIRAGANA PROLONGED SOUND MARK
    /// assert!(!extender.contains('・'));  // U+30FB KATAKANA MIDDLE DOT
    /// ```

}

make_binary_property! {
    name: "Extended_Pictographic";
    short_name: "ExtPict";
    ident: ExtendedPictographic;
    data_marker: crate::provider::PropertyBinaryExtendedPictographicV1;
    singleton: SINGLETON_PROPERTY_BINARY_EXTENDED_PICTOGRAPHIC_V1;
    /// Pictographic symbols, as well as reserved ranges in blocks largely associated with
    /// emoji characters
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::ExtendedPictographic;
    ///
    /// let extended_pictographic = CodePointSetData::new::<ExtendedPictographic>();
    ///
    /// assert!(extended_pictographic.contains('🥳')); // U+1F973 FACE WITH PARTY HORN AND PARTY HAT
    /// assert!(!extended_pictographic.contains('🇪'));  // U+1F1EA REGIONAL INDICATOR SYMBOL LETTER E
    /// ```

}

make_binary_property! {
    name: "graph";
    short_name: "graph";
    ident: Graph;
    data_marker: crate::provider::PropertyBinaryGraphV1;
    singleton: SINGLETON_PROPERTY_BINARY_GRAPH_V1;
    /// Invisible characters.
    ///
    /// This is defined for POSIX compatibility.

}

make_binary_property! {
    name: "Grapheme_Base";
    short_name: "Gr_Base";
    ident: GraphemeBase;
    data_marker: crate::provider::PropertyBinaryGraphemeBaseV1;
    singleton: SINGLETON_PROPERTY_BINARY_GRAPHEME_BASE_V1;
    /// Property used together with the definition of Standard Korean Syllable Block to define
    /// "Grapheme base".
    ///
    /// See D58 in Chapter 3, Conformance in the Unicode Standard.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::GraphemeBase;
    ///
    /// let grapheme_base = CodePointSetData::new::<GraphemeBase>();
    ///
    /// assert!(grapheme_base.contains('ക'));  // U+0D15 MALAYALAM LETTER KA
    /// assert!(grapheme_base.contains('\u{0D3F}'));  // U+0D3F MALAYALAM VOWEL SIGN I
    /// assert!(!grapheme_base.contains('\u{0D3E}'));  // U+0D3E MALAYALAM VOWEL SIGN AA
    /// ```

}

make_binary_property! {
    name: "Grapheme_Extend";
    short_name: "Gr_Ext";
    ident: GraphemeExtend;
    data_marker: crate::provider::PropertyBinaryGraphemeExtendV1;
    singleton: SINGLETON_PROPERTY_BINARY_GRAPHEME_EXTEND_V1;
    /// Property used to define "Grapheme extender".
    ///
    /// See D59 in Chapter 3, Conformance in the
    /// Unicode Standard.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::GraphemeExtend;
    ///
    /// let grapheme_extend = CodePointSetData::new::<GraphemeExtend>();
    ///
    /// assert!(!grapheme_extend.contains('ക'));  // U+0D15 MALAYALAM LETTER KA
    /// assert!(!grapheme_extend.contains('\u{0D3F}'));  // U+0D3F MALAYALAM VOWEL SIGN I
    /// assert!(grapheme_extend.contains('\u{0D3E}'));  // U+0D3E MALAYALAM VOWEL SIGN AA
    /// ```

}

make_binary_property! {
    name: "Grapheme_Link";
    short_name: "Gr_Link";
    ident: GraphemeLink;
    data_marker: crate::provider::PropertyBinaryGraphemeLinkV1;
    singleton: SINGLETON_PROPERTY_BINARY_GRAPHEME_LINK_V1;
    /// Deprecated property.
    ///
    /// Formerly proposed for programmatic determination of grapheme
    /// cluster boundaries.
}

make_binary_property! {
    name: "Hex_Digit";
    short_name: "Hex";
    ident: HexDigit;
    data_marker: crate::provider::PropertyBinaryHexDigitV1;
    singleton: SINGLETON_PROPERTY_BINARY_HEX_DIGIT_V1;
    /// Characters commonly used for the representation of hexadecimal numbers, plus their
    /// compatibility equivalents.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::HexDigit;
    ///
    /// let hex_digit = CodePointSetData::new::<HexDigit>();
    ///
    /// assert!(hex_digit.contains('0'));
    /// assert!(!hex_digit.contains('੩'));  // U+0A69 GURMUKHI DIGIT THREE
    /// assert!(hex_digit.contains('f'));
    /// assert!(hex_digit.contains('ｆ'));  // U+FF46 FULLWIDTH LATIN SMALL LETTER F
    /// assert!(hex_digit.contains('Ｆ'));  // U+FF26 FULLWIDTH LATIN CAPITAL LETTER F
    /// assert!(!hex_digit.contains('Ä'));  // U+00C4 LATIN CAPITAL LETTER A WITH DIAERESIS
    /// ```
}

make_binary_property! {
    name: "Hyphen";
    short_name: "Hyphen";
    ident: Hyphen;
    data_marker: crate::provider::PropertyBinaryHyphenV1;
    singleton: SINGLETON_PROPERTY_BINARY_HYPHEN_V1;
    /// Deprecated property.
    ///
    /// Dashes which are used to mark connections between pieces of
    /// words, plus the Katakana middle dot.
}

make_binary_property! {
    name: "ID_Compat_Math_Continue";
    short_name: "ID_Compat_Math_Continue";
    ident: IdCompatMathContinue;
    data_marker: crate::provider::PropertyBinaryIdCompatMathContinueV1;
    singleton: SINGLETON_PROPERTY_BINARY_ID_COMPAT_MATH_CONTINUE_V1;
    /// `ID_Compat_Math_Continue` Property
}

make_binary_property! {
    name: "ID_Compat_Math_Start";
    short_name: "ID_Compat_Math_Start";
    ident: IdCompatMathStart;
    data_marker: crate::provider::PropertyBinaryIdCompatMathStartV1;
    singleton: SINGLETON_PROPERTY_BINARY_ID_COMPAT_MATH_START_V1;
    /// `ID_Compat_Math_Start` Property
}

make_binary_property! {
    name: "ID_Continue";
    short_name: "IDC";
    ident: IdContinue;
    data_marker: crate::provider::PropertyBinaryIdContinueV1;
    singleton: SINGLETON_PROPERTY_BINARY_ID_CONTINUE_V1;
    /// Characters that can come after the first character in an identifier.
    ///
    /// If using NFKC to
    /// fold differences between characters, use [`XidContinue`] instead.  See
    /// [`Unicode Standard Annex #31`](https://www.unicode.org/reports/tr31/tr31-35.html) for
    /// more details.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::IdContinue;
    ///
    /// let id_continue = CodePointSetData::new::<IdContinue>();
    ///
    /// assert!(id_continue.contains('x'));
    /// assert!(id_continue.contains('1'));
    /// assert!(id_continue.contains('_'));
    /// assert!(id_continue.contains('ߝ'));  // U+07DD NKO LETTER FA
    /// assert!(!id_continue.contains('ⓧ'));  // U+24E7 CIRCLED LATIN SMALL LETTER X
    /// assert!(id_continue.contains('\u{FC5E}'));  // ARABIC LIGATURE SHADDA WITH DAMMATAN ISOLATED FORM
    /// ```
}

make_binary_property! {
    name: "Ideographic";
    short_name: "Ideo";
    ident: Ideographic;
    data_marker: crate::provider::PropertyBinaryIdeographicV1;
    singleton: SINGLETON_PROPERTY_BINARY_IDEOGRAPHIC_V1;
    /// Characters considered to be CJKV (Chinese, Japanese, Korean, and Vietnamese)
    /// ideographs, or related siniform ideographs
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::Ideographic;
    ///
    /// let ideographic = CodePointSetData::new::<Ideographic>();
    ///
    /// assert!(ideographic.contains('川'));  // U+5DDD CJK UNIFIED IDEOGRAPH-5DDD
    /// assert!(!ideographic.contains('밥'));  // U+BC25 HANGUL SYLLABLE BAB
    /// ```
}

make_binary_property! {
    name: "ID_Start";
    short_name: "IDS";
    ident: IdStart;
    data_marker: crate::provider::PropertyBinaryIdStartV1;
    singleton: SINGLETON_PROPERTY_BINARY_ID_START_V1;
    /// Characters that can begin an identifier.
    ///
    /// If using NFKC to fold differences between
    /// characters, use [`XidStart`] instead.  See [`Unicode Standard Annex
    /// #31`](https://www.unicode.org/reports/tr31/tr31-35.html) for more details.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::IdStart;
    ///
    /// let id_start = CodePointSetData::new::<IdStart>();
    ///
    /// assert!(id_start.contains('x'));
    /// assert!(!id_start.contains('1'));
    /// assert!(!id_start.contains('_'));
    /// assert!(id_start.contains('ߝ'));  // U+07DD NKO LETTER FA
    /// assert!(!id_start.contains('ⓧ'));  // U+24E7 CIRCLED LATIN SMALL LETTER X
    /// assert!(id_start.contains('\u{FC5E}'));  // ARABIC LIGATURE SHADDA WITH DAMMATAN ISOLATED FORM
    /// ```
}

make_binary_property! {
    name: "IDS_Binary_Operator";
    short_name: "IDSB";
    ident: IdsBinaryOperator;
    data_marker: crate::provider::PropertyBinaryIdsBinaryOperatorV1;
    singleton: SINGLETON_PROPERTY_BINARY_IDS_BINARY_OPERATOR_V1;
    /// Characters used in Ideographic Description Sequences.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::IdsBinaryOperator;
    ///
    /// let ids_binary_operator = CodePointSetData::new::<IdsBinaryOperator>();
    ///
    /// assert!(ids_binary_operator.contains('\u{2FF5}'));  // IDEOGRAPHIC DESCRIPTION CHARACTER SURROUND FROM ABOVE
    /// assert!(!ids_binary_operator.contains('\u{3006}'));  // IDEOGRAPHIC CLOSING MARK
    /// ```
}

make_binary_property! {
    name: "IDS_Trinary_Operator";
    short_name: "IDST";
    ident: IdsTrinaryOperator;
    data_marker: crate::provider::PropertyBinaryIdsTrinaryOperatorV1;
    singleton: SINGLETON_PROPERTY_BINARY_IDS_TRINARY_OPERATOR_V1;
    /// Characters used in Ideographic Description Sequences.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::IdsTrinaryOperator;
    ///
    /// let ids_trinary_operator = CodePointSetData::new::<IdsTrinaryOperator>();
    ///
    /// assert!(ids_trinary_operator.contains('\u{2FF2}'));  // IDEOGRAPHIC DESCRIPTION CHARACTER LEFT TO MIDDLE AND RIGHT
    /// assert!(ids_trinary_operator.contains('\u{2FF3}'));  // IDEOGRAPHIC DESCRIPTION CHARACTER ABOVE TO MIDDLE AND BELOW
    /// assert!(!ids_trinary_operator.contains('\u{2FF4}'));
    /// assert!(!ids_trinary_operator.contains('\u{2FF5}'));  // IDEOGRAPHIC DESCRIPTION CHARACTER SURROUND FROM ABOVE
    /// assert!(!ids_trinary_operator.contains('\u{3006}'));  // IDEOGRAPHIC CLOSING MARK
    /// ```
}

make_binary_property! {
    name: "IDS_Unary_Operator";
    short_name: "IDSU";
    ident: IdsUnaryOperator;
    data_marker: crate::provider::PropertyBinaryIdsUnaryOperatorV1;
    singleton: SINGLETON_PROPERTY_BINARY_IDS_UNARY_OPERATOR_V1;
    /// `IDS_Unary_Operator` Property
}

make_binary_property! {
    name: "Join_Control";
    short_name: "Join_C";
    ident: JoinControl;
    data_marker: crate::provider::PropertyBinaryJoinControlV1;
    singleton: SINGLETON_PROPERTY_BINARY_JOIN_CONTROL_V1;
    /// Format control characters which have specific functions for control of cursive joining
    /// and ligation.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::JoinControl;
    ///
    /// let join_control = CodePointSetData::new::<JoinControl>();
    ///
    /// assert!(join_control.contains('\u{200C}'));  // ZERO WIDTH NON-JOINER
    /// assert!(join_control.contains('\u{200D}'));  // ZERO WIDTH JOINER
    /// assert!(!join_control.contains('\u{200E}'));
    /// ```
}

make_binary_property! {
    name: "Logical_Order_Exception";
    short_name: "LOE";
    ident: LogicalOrderException;
    data_marker: crate::provider::PropertyBinaryLogicalOrderExceptionV1;
    singleton: SINGLETON_PROPERTY_BINARY_LOGICAL_ORDER_EXCEPTION_V1;
    /// A small number of spacing vowel letters occurring in certain Southeast Asian scripts such as Thai and Lao.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::LogicalOrderException;
    ///
    /// let logical_order_exception = CodePointSetData::new::<LogicalOrderException>();
    ///
    /// assert!(logical_order_exception.contains('ແ'));  // U+0EC1 LAO VOWEL SIGN EI
    /// assert!(!logical_order_exception.contains('ະ'));  // U+0EB0 LAO VOWEL SIGN A
    /// ```
}

make_binary_property! {
    name: "Lowercase";
    short_name: "Lower";
    ident: Lowercase;
    data_marker: crate::provider::PropertyBinaryLowercaseV1;
    singleton: SINGLETON_PROPERTY_BINARY_LOWERCASE_V1;
    /// Lowercase characters.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::Lowercase;
    ///
    /// let lowercase = CodePointSetData::new::<Lowercase>();
    ///
    /// assert!(lowercase.contains('a'));
    /// assert!(!lowercase.contains('A'));
    /// ```
}

make_binary_property! {
    name: "Math";
    short_name: "Math";
    ident: Math;
    data_marker: crate::provider::PropertyBinaryMathV1;
    singleton: SINGLETON_PROPERTY_BINARY_MATH_V1;
    /// Characters used in mathematical notation.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::Math;
    ///
    /// let math = CodePointSetData::new::<Math>();
    ///
    /// assert!(math.contains('='));
    /// assert!(math.contains('+'));
    /// assert!(!math.contains('-'));
    /// assert!(math.contains('−'));  // U+2212 MINUS SIGN
    /// assert!(!math.contains('/'));
    /// assert!(math.contains('∕'));  // U+2215 DIVISION SLASH
    /// ```
}

make_binary_property! {
    name: "Modifier_Combining_Mark";
    short_name: "MCM";
    ident: ModifierCombiningMark;
    data_marker: crate::provider::PropertyBinaryModifierCombiningMarkV1;
    singleton: SINGLETON_PROPERTY_BINARY_MODIFIER_COMBINING_MARK_V1;
    /// `Modifier_Combining_Mark` Property
}

make_binary_property! {
    name: "Noncharacter_Code_Point";
    short_name: "NChar";
    ident: NoncharacterCodePoint;
    data_marker: crate::provider::PropertyBinaryNoncharacterCodePointV1;
    singleton: SINGLETON_PROPERTY_BINARY_NONCHARACTER_CODE_POINT_V1;
    /// Code points permanently reserved for internal use.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::NoncharacterCodePoint;
    ///
    /// let noncharacter_code_point = CodePointSetData::new::<NoncharacterCodePoint>();
    ///
    /// assert!(noncharacter_code_point.contains('\u{FDD0}'));
    /// assert!(noncharacter_code_point.contains('\u{FFFF}'));
    /// assert!(!noncharacter_code_point.contains('\u{10000}'));
    /// ```
}

make_binary_property! {
    name: "NFC_Inert";
    short_name: "nfcinert";
    ident: NfcInert;
    data_marker: crate::provider::PropertyBinaryNfcInertV1;
    singleton: SINGLETON_PROPERTY_BINARY_NFC_INERT_V1;
    /// Characters that are inert under NFC, i.e., they do not interact with adjacent characters.
    #[deprecated(since = "2.3.0", note = "not a UCD property")]
}

make_binary_property! {
    name: "NFD_Inert";
    short_name: "nfdinert";
    ident: NfdInert;
    data_marker: crate::provider::PropertyBinaryNfdInertV1;
    singleton: SINGLETON_PROPERTY_BINARY_NFD_INERT_V1;
    /// Characters that are inert under NFD, i.e., they do not interact with adjacent characters.
    #[deprecated(since = "2.3.0", note = "not a UCD property")]
}

make_binary_property! {
    name: "NFKC_Inert";
    short_name: "nfkcinert";
    ident: NfkcInert;
    data_marker: crate::provider::PropertyBinaryNfkcInertV1;
    singleton: SINGLETON_PROPERTY_BINARY_NFKC_INERT_V1;
    /// Characters that are inert under NFKC, i.e., they do not interact with adjacent characters.
    #[deprecated(since = "2.3.0", note = "not a UCD property")]
}

make_binary_property! {
    name: "NFKD_Inert";
    short_name: "nfkdinert";
    ident: NfkdInert;
    data_marker: crate::provider::PropertyBinaryNfkdInertV1;
    singleton: SINGLETON_PROPERTY_BINARY_NFKD_INERT_V1;
    /// Characters that are inert under NFKD, i.e., they do not interact with adjacent characters.
    #[deprecated(since = "2.3.0", note = "not a UCD property")]
}

make_binary_property! {
    name: "Pattern_Syntax";
    short_name: "Pat_Syn";
    ident: PatternSyntax;
    data_marker: crate::provider::PropertyBinaryPatternSyntaxV1;
    singleton: SINGLETON_PROPERTY_BINARY_PATTERN_SYNTAX_V1;
    /// Characters used as syntax in patterns (such as regular expressions).
    ///
    /// See [`Unicode
    /// Standard Annex #31`](https://www.unicode.org/reports/tr31/tr31-35.html) for more
    /// details.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::PatternSyntax;
    ///
    /// let pattern_syntax = CodePointSetData::new::<PatternSyntax>();
    ///
    /// assert!(pattern_syntax.contains('{'));
    /// assert!(pattern_syntax.contains('⇒'));  // U+21D2 RIGHTWARDS DOUBLE ARROW
    /// assert!(!pattern_syntax.contains('0'));
    /// ```
}

make_binary_property! {
    name: "Pattern_White_Space";
    short_name: "Pat_WS";
    ident: PatternWhiteSpace;
    data_marker: crate::provider::PropertyBinaryPatternWhiteSpaceV1;
    singleton: SINGLETON_PROPERTY_BINARY_PATTERN_WHITE_SPACE_V1;
    /// Characters used as whitespace in patterns (such as regular expressions).
    ///
    /// See
    /// [`Unicode Standard Annex #31`](https://www.unicode.org/reports/tr31/tr31-35.html) for
    /// more details.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::PatternWhiteSpace;
    ///
    /// let pattern_white_space = CodePointSetData::new::<PatternWhiteSpace>();
    ///
    /// assert!(pattern_white_space.contains(' '));
    /// assert!(pattern_white_space.contains('\u{2029}'));  // PARAGRAPH SEPARATOR
    /// assert!(pattern_white_space.contains('\u{000A}'));  // NEW LINE
    /// assert!(!pattern_white_space.contains('\u{00A0}'));  // NO-BREAK SPACE
    /// ```
}

make_binary_property! {
    name: "Prepended_Concatenation_Mark";
    short_name: "PCM";
    ident: PrependedConcatenationMark;
    data_marker: crate::provider::PropertyBinaryPrependedConcatenationMarkV1;
    singleton: SINGLETON_PROPERTY_BINARY_PREPENDED_CONCATENATION_MARK_V1;
    /// A small class of visible format controls, which precede and then span a sequence of
    /// other characters, usually digits.
}

make_binary_property! {
    name: "print";
    short_name: "print";
    ident: Print;
    data_marker: crate::provider::PropertyBinaryPrintV1;
    singleton: SINGLETON_PROPERTY_BINARY_PRINT_V1;
    /// Printable characters (visible characters and whitespace).
    ///
    /// This is defined for POSIX compatibility.
}

make_binary_property! {
    name: "Quotation_Mark";
    short_name: "QMark";
    ident: QuotationMark;
    data_marker: crate::provider::PropertyBinaryQuotationMarkV1;
    singleton: SINGLETON_PROPERTY_BINARY_QUOTATION_MARK_V1;
    /// Punctuation characters that function as quotation marks.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::QuotationMark;
    ///
    /// let quotation_mark = CodePointSetData::new::<QuotationMark>();
    ///
    /// assert!(quotation_mark.contains('\''));
    /// assert!(quotation_mark.contains('„'));  // U+201E DOUBLE LOW-9 QUOTATION MARK
    /// assert!(!quotation_mark.contains('<'));
    /// ```
}

make_binary_property! {
    name: "Radical";
    short_name: "Radical";
    ident: Radical;
    data_marker: crate::provider::PropertyBinaryRadicalV1;
    singleton: SINGLETON_PROPERTY_BINARY_RADICAL_V1;
    /// Characters used in the definition of Ideographic Description Sequences.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::Radical;
    ///
    /// let radical = CodePointSetData::new::<Radical>();
    ///
    /// assert!(radical.contains('⺆'));  // U+2E86 CJK RADICAL BOX
    /// assert!(!radical.contains('丹'));  // U+F95E CJK COMPATIBILITY IDEOGRAPH-F95E
    /// ```
}

make_binary_property! {
    name: "Regional_Indicator";
    short_name: "RI";
    ident: RegionalIndicator;
    data_marker: crate::provider::PropertyBinaryRegionalIndicatorV1;
    singleton: SINGLETON_PROPERTY_BINARY_REGIONAL_INDICATOR_V1;
    /// Regional indicator characters, `U+1F1E6..U+1F1FF`.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::RegionalIndicator;
    ///
    /// let regional_indicator = CodePointSetData::new::<RegionalIndicator>();
    ///
    /// assert!(regional_indicator.contains('🇹'));  // U+1F1F9 REGIONAL INDICATOR SYMBOL LETTER T
    /// assert!(!regional_indicator.contains('Ⓣ'));  // U+24C9 CIRCLED LATIN CAPITAL LETTER T
    /// assert!(!regional_indicator.contains('T'));
    /// ```
}

make_binary_property! {
    name: "Soft_Dotted";
    short_name: "SD";
    ident: SoftDotted;
    data_marker: crate::provider::PropertyBinarySoftDottedV1;
    singleton: SINGLETON_PROPERTY_BINARY_SOFT_DOTTED_V1;
    /// Characters with a "soft dot", like i or j.
    ///
    /// An accent placed on these characters causes
    /// the dot to disappear.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::SoftDotted;
    ///
    /// let soft_dotted = CodePointSetData::new::<SoftDotted>();
    ///
    /// assert!(soft_dotted.contains('і'));  //U+0456 CYRILLIC SMALL LETTER BYELORUSSIAN-UKRAINIAN I
    /// assert!(!soft_dotted.contains('ı'));  // U+0131 LATIN SMALL LETTER DOTLESS I
    /// ```
}

make_binary_property! {
    name: "Segment_Starter";
    short_name: "segstart";
    ident: SegmentStarter;
    data_marker: crate::provider::PropertyBinarySegmentStarterV1;
    singleton: SINGLETON_PROPERTY_BINARY_SEGMENT_STARTER_V1;
    /// Characters that are starters in terms of Unicode normalization and combining character
    /// sequences.
    #[deprecated(since = "2.3.0", note = "not a UCD property")]
}

make_binary_property! {
    name: "Case_Sensitive";
    short_name: "Sensitive";
    ident: CaseSensitive;
    data_marker: crate::provider::PropertyBinaryCaseSensitiveV1;
    singleton: SINGLETON_PROPERTY_BINARY_CASE_SENSITIVE_V1;
    /// Characters that are either the source of a case mapping or in the target of a case
    /// mapping.
    #[deprecated(since = "2.3.0", note = "not a UCD property")]
}

make_binary_property! {
    name: "Sentence_Terminal";
    short_name: "STerm";
    ident: SentenceTerminal;
    data_marker: crate::provider::PropertyBinarySentenceTerminalV1;
    singleton: SINGLETON_PROPERTY_BINARY_SENTENCE_TERMINAL_V1;
    /// Punctuation characters that generally mark the end of sentences.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::SentenceTerminal;
    ///
    /// let sentence_terminal = CodePointSetData::new::<SentenceTerminal>();
    ///
    /// assert!(sentence_terminal.contains('.'));
    /// assert!(sentence_terminal.contains('?'));
    /// assert!(sentence_terminal.contains('᪨'));  // U+1AA8 TAI THAM SIGN KAAN
    /// assert!(!sentence_terminal.contains(','));
    /// assert!(!sentence_terminal.contains('¿'));  // U+00BF INVERTED QUESTION MARK
    /// ```
}

make_binary_property! {
    name: "Terminal_Punctuation";
    short_name: "Term";
    ident: TerminalPunctuation;
    data_marker: crate::provider::PropertyBinaryTerminalPunctuationV1;
    singleton: SINGLETON_PROPERTY_BINARY_TERMINAL_PUNCTUATION_V1;
    /// Punctuation characters that generally mark the end of textual units.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::TerminalPunctuation;
    ///
    /// let terminal_punctuation = CodePointSetData::new::<TerminalPunctuation>();
    ///
    /// assert!(terminal_punctuation.contains('.'));
    /// assert!(terminal_punctuation.contains('?'));
    /// assert!(terminal_punctuation.contains('᪨'));  // U+1AA8 TAI THAM SIGN KAAN
    /// assert!(terminal_punctuation.contains(','));
    /// assert!(!terminal_punctuation.contains('¿'));  // U+00BF INVERTED QUESTION MARK
    /// ```
}

make_binary_property! {
    name: "Unified_Ideograph";
    short_name: "UIdeo";
    ident: UnifiedIdeograph;
    data_marker: crate::provider::PropertyBinaryUnifiedIdeographV1;
    singleton: SINGLETON_PROPERTY_BINARY_UNIFIED_IDEOGRAPH_V1;
    /// A property which specifies the exact set of Unified CJK Ideographs in the standard.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::UnifiedIdeograph;
    ///
    /// let unified_ideograph = CodePointSetData::new::<UnifiedIdeograph>();
    ///
    /// assert!(unified_ideograph.contains('川'));  // U+5DDD CJK UNIFIED IDEOGRAPH-5DDD
    /// assert!(unified_ideograph.contains('木'));  // U+6728 CJK UNIFIED IDEOGRAPH-6728
    /// assert!(!unified_ideograph.contains('𛅸'));  // U+1B178 NUSHU CHARACTER-1B178
    /// ```
}

make_binary_property! {
    name: "Uppercase";
    short_name: "Upper";
    ident: Uppercase;
    data_marker: crate::provider::PropertyBinaryUppercaseV1;
    singleton: SINGLETON_PROPERTY_BINARY_UPPERCASE_V1;
    /// Uppercase characters.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::Uppercase;
    ///
    /// let uppercase = CodePointSetData::new::<Uppercase>();
    ///
    /// assert!(uppercase.contains('U'));
    /// assert!(!uppercase.contains('u'));
    /// ```
}

make_binary_property! {
    name: "Variation_Selector";
    short_name: "VS";
    ident: VariationSelector;
    data_marker: crate::provider::PropertyBinaryVariationSelectorV1;
    singleton: SINGLETON_PROPERTY_BINARY_VARIATION_SELECTOR_V1;
    /// Characters that are Variation Selectors.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::VariationSelector;
    ///
    /// let variation_selector = CodePointSetData::new::<VariationSelector>();
    ///
    /// assert!(variation_selector.contains('\u{180D}'));  // MONGOLIAN FREE VARIATION SELECTOR THREE
    /// assert!(!variation_selector.contains('\u{303E}'));  // IDEOGRAPHIC VARIATION INDICATOR
    /// assert!(variation_selector.contains('\u{FE0F}'));  // VARIATION SELECTOR-16
    /// assert!(!variation_selector.contains('\u{FE10}'));  // PRESENTATION FORM FOR VERTICAL COMMA
    /// assert!(variation_selector.contains('\u{E01EF}'));  // VARIATION SELECTOR-256
    /// ```
}

make_binary_property! {
    name: "White_Space";
    short_name: "WSpace";
    ident: WhiteSpace;
    data_marker: crate::provider::PropertyBinaryWhiteSpaceV1;
    singleton: SINGLETON_PROPERTY_BINARY_WHITE_SPACE_V1;
    /// Spaces, separator characters and other control characters which should be treated by
    /// programming languages as "white space" for the purpose of parsing elements.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::WhiteSpace;
    ///
    /// let white_space = CodePointSetData::new::<WhiteSpace>();
    ///
    /// assert!(white_space.contains(' '));
    /// assert!(white_space.contains('\u{000A}'));  // NEW LINE
    /// assert!(white_space.contains('\u{00A0}'));  // NO-BREAK SPACE
    /// assert!(!white_space.contains('\u{200B}'));  // ZERO WIDTH SPACE
    /// ```
}

make_binary_property! {
    name: "xdigit";
    short_name: "xdigit";
    ident: Xdigit;
    data_marker: crate::provider::PropertyBinaryXdigitV1;
    singleton: SINGLETON_PROPERTY_BINARY_XDIGIT_V1;
    /// Hexadecimal digits
    ///
    /// This is defined for POSIX compatibility.
}

make_binary_property! {
    name: "XID_Continue";
    short_name: "XIDC";
    ident: XidContinue;
    data_marker: crate::provider::PropertyBinaryXidContinueV1;
    singleton: SINGLETON_PROPERTY_BINARY_XID_CONTINUE_V1;
    /// Characters that can come after the first character in an identifier.
    ///
    /// See [`Unicode Standard Annex
    /// #31`](https://www.unicode.org/reports/tr31/tr31-35.html) for more details.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::XidContinue;
    ///
    /// let xid_continue = CodePointSetData::new::<XidContinue>();
    ///
    /// assert!(xid_continue.contains('x'));
    /// assert!(xid_continue.contains('1'));
    /// assert!(xid_continue.contains('_'));
    /// assert!(xid_continue.contains('ߝ'));  // U+07DD NKO LETTER FA
    /// assert!(!xid_continue.contains('ⓧ'));  // U+24E7 CIRCLED LATIN SMALL LETTER X
    /// assert!(!xid_continue.contains('\u{FC5E}'));  // ARABIC LIGATURE SHADDA WITH DAMMATAN ISOLATED FORM
    /// ```
}

make_binary_property! {
    name: "XID_Start";
    short_name: "XIDS";
    ident: XidStart;
    data_marker: crate::provider::PropertyBinaryXidStartV1;
    singleton: SINGLETON_PROPERTY_BINARY_XID_START_V1;
    /// Characters that can begin an identifier.
    ///
    /// See [`Unicode
    /// Standard Annex #31`](https://www.unicode.org/reports/tr31/tr31-35.html) for more
    /// details.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::CodePointSetData;
    /// use icu::properties::props::XidStart;
    ///
    /// let xid_start = CodePointSetData::new::<XidStart>();
    ///
    /// assert!(xid_start.contains('x'));
    /// assert!(!xid_start.contains('1'));
    /// assert!(!xid_start.contains('_'));
    /// assert!(xid_start.contains('ߝ'));  // U+07DD NKO LETTER FA
    /// assert!(!xid_start.contains('ⓧ'));  // U+24E7 CIRCLED LATIN SMALL LETTER X
    /// assert!(!xid_start.contains('\u{FC5E}'));  // ARABIC LIGATURE SHADDA WITH DAMMATAN ISOLATED FORM
    /// ```
}

pub use crate::emoji::EmojiSet;

macro_rules! make_emoji_set {
    (
        name: $name:literal;
        short_name: $short_name:literal;
        ident: $ident:ident;
        data_marker: $data_marker:ty;
        singleton: $singleton:ident;
        $(#[$doc:meta])+
    ) => {
        $(#[$doc])+
        #[derive(Debug)]
        #[non_exhaustive]
        pub struct $ident;

        impl crate::private::Sealed for $ident {}

        impl EmojiSet for $ident {
            type DataMarker = $data_marker;
            #[cfg(feature = "compiled_data")]
            const SINGLETON: &'static crate::provider::PropertyUnicodeSet<'static> =
                &crate::provider::Baked::$singleton;
            const NAME: &'static [u8] = $name.as_bytes();
            const SHORT_NAME: &'static [u8] = $short_name.as_bytes();
        }
    }
}

make_emoji_set! {
    name: "Basic_Emoji";
    short_name: "Basic_Emoji";
    ident: BasicEmoji;
    data_marker: crate::provider::PropertyBinaryBasicEmojiV1;
    singleton: SINGLETON_PROPERTY_BINARY_BASIC_EMOJI_V1;
    /// Characters and character sequences intended for general-purpose, independent, direct input.
    ///
    /// See [`Unicode Technical Standard #51`](https://unicode.org/reports/tr51/) for more
    /// details.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::EmojiSetData;
    /// use icu::properties::props::BasicEmoji;
    ///
    /// let basic_emoji = EmojiSetData::new::<BasicEmoji>();
    ///
    /// assert!(!basic_emoji.contains('\u{0020}'));
    /// assert!(!basic_emoji.contains('\n'));
    /// assert!(basic_emoji.contains('🦃')); // U+1F983 TURKEY
    /// assert!(basic_emoji.contains_str("\u{1F983}"));
    /// assert!(basic_emoji.contains_str("\u{1F6E4}\u{FE0F}")); // railway track
    /// assert!(!basic_emoji.contains_str("\u{0033}\u{FE0F}\u{20E3}"));  // Emoji_Keycap_Sequence, keycap 3
    /// ```
}
