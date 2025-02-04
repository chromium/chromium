// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// The bit layout of `CollatorOptions` is adapted from ICU4C and, therefore,
// is subject to the ICU license as described in LICENSE.

//! This module contains the types that are part of the API for setting
//! the options for the collator.

use crate::elements::{CASE_MASK, TERTIARY_MASK};

/// The collation strength that indicates how many levels to compare.
///
/// If an earlier level isn't equal, the earlier level is decisive.
/// If the result is equal on a level, but the strength is higher,
/// the comparison proceeds to the next level.
///
/// Note: The bit layout of `CollatorOptions` requires `Strength`
/// to fit in 3 bits.
#[derive(Eq, PartialEq, Debug, Copy, Clone, PartialOrd, Ord)]
#[repr(u8)]
#[non_exhaustive]
pub enum Strength {
    /// Compare only on the level of base letters. This level
    /// corresponds to the ECMA-402 sensitivity "base" with
    /// [`CaseLevel::Off`] (the default for [`CaseLevel`]) and
    /// to ECMA-402 sensitivity "case" with [`CaseLevel::On`].
    ///
    /// ```
    /// use icu::collator::*;
    ///
    /// let mut options = CollatorOptions::default();
    /// options.strength = Some(Strength::Primary);
    /// let collator = Collator::try_new(Default::default(), options).unwrap();
    /// assert_eq!(collator.compare("E", "é"), core::cmp::Ordering::Equal);
    /// ```
    Primary = 0,

    /// Compare also on the secondary level, which corresponds
    /// to diacritics in scripts that use them. This level corresponds
    /// to the ECMA-402 sensitivity "accent".
    ///
    /// ```
    /// use icu::collator::*;
    ///
    /// let mut options = CollatorOptions::default();
    /// options.strength = Some(Strength::Secondary);
    /// let collator = Collator::try_new(Default::default(), options).unwrap();
    /// assert_eq!(collator.compare("E", "e"), core::cmp::Ordering::Equal);
    /// assert_eq!(collator.compare("e", "é"), core::cmp::Ordering::Less);
    /// assert_eq!(collator.compare("あ", "ア"), core::cmp::Ordering::Equal);
    /// assert_eq!(collator.compare("ァ", "ア"), core::cmp::Ordering::Equal);
    /// assert_eq!(collator.compare("ア", "ｱ"), core::cmp::Ordering::Equal);
    /// ```
    Secondary = 1,

    /// Compare also on the tertiary level. By default, if the separate
    /// case level is disabled, this corresponds to case for bicameral
    /// scripts. This level distinguishes Hiragana and Katakana. This
    /// also captures other minor differences, such as half-width vs.
    /// full-width when the Japanese tailoring isn't in use.
    ///
    /// This is the default comparison level and appropriate for
    /// most scripts. This level corresponds to the ECMA-402
    /// sensitivity "variant".
    ///
    /// ```
    /// use icu::collator::*;
    /// use icu::locale::locale;
    ///
    /// let mut options = CollatorOptions::default();
    /// options.strength = Some(Strength::Tertiary);
    /// let collator =
    ///   Collator::try_new(Default::default(),
    ///                     options).unwrap();
    /// assert_eq!(collator.compare("E", "e"),
    ///            core::cmp::Ordering::Greater);
    /// assert_eq!(collator.compare("e", "é"),
    ///            core::cmp::Ordering::Less);
    /// assert_eq!(collator.compare("あ", "ア"),
    ///            core::cmp::Ordering::Less);
    /// assert_eq!(collator.compare("ァ", "ア"),
    ///            core::cmp::Ordering::Less);
    /// assert_eq!(collator.compare("ア", "ｱ"),
    ///            core::cmp::Ordering::Less);
    /// assert_eq!(collator.compare("e", "ｅ"), // Full-width e
    ///            core::cmp::Ordering::Less);
    ///
    /// let ja_collator =
    ///   Collator::try_new(locale!("ja").into(), options).unwrap();
    /// assert_eq!(ja_collator.compare("E", "e"),
    ///            core::cmp::Ordering::Greater);
    /// assert_eq!(ja_collator.compare("e", "é"),
    ///            core::cmp::Ordering::Less);
    /// assert_eq!(ja_collator.compare("あ", "ア"),
    ///            core::cmp::Ordering::Equal); // Unlike root!
    /// assert_eq!(ja_collator.compare("ァ", "ア"),
    ///            core::cmp::Ordering::Less);
    /// assert_eq!(ja_collator.compare("ア", "ｱ"),
    ///            core::cmp::Ordering::Equal); // Unlike root!
    /// assert_eq!(ja_collator.compare("e", "ｅ"), // Full-width e
    ///            core::cmp::Ordering::Equal); // Unlike root!
    /// ```
    Tertiary = 2,

    /// Compare also on the quaternary level. For Japanese, Higana
    /// and Katakana are distinguished at the quaternary level. Also,
    /// if `AlternateHandling::Shifted` is used, the collation
    /// elements whose level gets shifted are shifted to this
    /// level.
    ///
    /// ```
    /// use icu::collator::*;
    /// use icu::locale::locale;
    ///
    /// let mut options = CollatorOptions::default();
    /// options.strength = Some(Strength::Quaternary);
    ///
    /// let ja_collator =
    ///   Collator::try_new(locale!("ja").into(), options).unwrap();
    /// assert_eq!(ja_collator.compare("あ", "ア"),
    ///            core::cmp::Ordering::Less);
    /// assert_eq!(ja_collator.compare("ア", "ｱ"),
    ///            core::cmp::Ordering::Equal);
    /// assert_eq!(ja_collator.compare("e", "ｅ"), // Full-width e
    ///            core::cmp::Ordering::Equal);
    ///
    /// // Even this level doesn't distinguish everything,
    /// // e.g. Hebrew cantillation marks are still ignored.
    /// let collator =
    ///   Collator::try_new(Default::default(),
    ///                     options).unwrap();
    /// assert_eq!(collator.compare("דחי", "דחי֭"),
    ///            core::cmp::Ordering::Equal);
    /// ```
    /// TODO: Thai example.
    Quaternary = 3,

    /// Compare the NFD form by code point order as the quinary
    /// level. This level makes the comparison slower and should
    /// not be used in the general case. However, it can be used
    /// to distinguish full-width and half-width forms when the
    /// Japanese tailoring is in use and to distinguish e.g.
    /// Hebrew cantillation markse. Use this level if you need
    /// JIS X 4061-1996 compliance for Japanese on the level of
    /// distinguishing full-width and half-width forms.
    ///
    /// ```
    /// use icu::collator::*;
    /// use icu::locale::locale;
    ///
    /// let mut options = CollatorOptions::default();
    /// options.strength = Some(Strength::Identical);
    ///
    /// let ja_collator =
    ///   Collator::try_new(locale!("ja").into(), options).unwrap();
    /// assert_eq!(ja_collator.compare("ア", "ｱ"),
    ///            core::cmp::Ordering::Less);
    /// assert_eq!(ja_collator.compare("e", "ｅ"), // Full-width e
    ///            core::cmp::Ordering::Less);
    ///
    /// let collator =
    ///   Collator::try_new(Default::default(),
    ///                     options).unwrap();
    /// assert_eq!(collator.compare("דחי", "דחי֭"),
    ///            core::cmp::Ordering::Less);
    /// ```
    Identical = 7,
}

/// What to do about characters whose comparison level can be
/// varied dynamically.
#[derive(Eq, PartialEq, Debug, Copy, Clone, PartialOrd, Ord)]
#[repr(u8)]
#[non_exhaustive]
pub enum AlternateHandling {
    /// Keep the characters whose level can be varied on the
    /// primary level.
    NonIgnorable = 0,
    /// Shift the characters at or below `MaxVariable` to the
    /// quaternary level.
    Shifted = 1,
    // Possible future values: ShiftTrimmed, Blanked
}

/// Treatment of case. (Large and small kana
/// differences are treated as case differences.)
#[derive(Eq, PartialEq, Debug, Copy, Clone, PartialOrd, Ord)]
#[repr(u8)]
#[non_exhaustive]
pub enum CaseFirst {
    /// Use the default tertiary weights.
    Off = 0,
    /// Lower case first.
    LowerFirst = 1,
    /// Upper case first.
    UpperFirst = 2,
}

/// What characters get shifted to the quaternary level
/// with `AlternateHandling::Shifted`.
#[derive(Eq, PartialEq, Debug, Copy, Clone)]
#[repr(u8)]
#[non_exhaustive]
pub enum MaxVariable {
    /// Characters classified as spaces are shifted.
    Space = 0,
    /// Characters classified as spaces or punctuation
    /// are shifted.
    Punctuation = 1,
    /// Characters classified as spaces, punctuation,
    /// or symbols are shifted.
    Symbol = 2,
    /// Characters classified as spaces, punctuation,
    /// symbols, or currency symbols are shifted.
    Currency = 3,
}

/// Whether to distinguish case in sorting, even for sorting levels higher
/// than tertiary, without having to use tertiary level just to enable case level differences.
#[derive(Eq, PartialEq, Debug, Copy, Clone)]
#[repr(u8)]
#[non_exhaustive]
pub enum CaseLevel {
    /// Leave off the case level option.  Case differences will be handled by default
    /// in tertiary strength.
    Off = 0,
    /// Turn on the case level option, thereby making a separate level for case
    /// differences, positioned between secondary and tertiary.
    ///
    /// When used together with [`Strength::Primary`], this corresponds to the
    /// ECMA-402 sensitivity "case".
    On = 1,
}

/// When set to `On`, any sequence of decimal digits is sorted at a primary level according to the numeric value.
#[derive(Eq, PartialEq, Debug, Copy, Clone)]
#[repr(u8)]
#[non_exhaustive]
pub enum Numeric {
    /// Leave off the numeric option.  Decimal digits will be treated as characters by the default
    /// algorithm.
    Off = 0,
    /// Turn on numeric sorting for any sequence of decimal digits, sorting at
    /// a primary level according to the numeric value.
    On = 1,
}

/// Whether second level compares the last accent difference
/// instead of the first accent difference.
#[derive(Eq, PartialEq, Debug, Copy, Clone)]
#[repr(u8)]
#[non_exhaustive]
pub enum BackwardSecondLevel {
    /// Leave off the backward second level option. Diacritics in the second level will be ordered by
    /// default from beginning to end.
    Off = 0,
    /// Turn on backward second level ordering so that the second level compares backwards, starting
    /// from the last diacritic letter and moving towards the beginning.
    On = 1,
}

/// Options settable by the user of the API.
///
/// With the exception of reordering (BCP47 `kr`), options that can by implied by locale are
/// also settable via [`CollatorOptions`].
///
/// See the [spec](https://www.unicode.org/reports/tr35/tr35-collation.html#Setting_Options).
///
/// The setters take an `Option` so that `None` can be used to go back to default.
///
/// # Options
///
/// Examples for using the different options below can be found in the [crate-level docs](crate).
///
/// ## ECMA-402 Sensitivity
///
/// ECMA-402 `sensitivity` maps to a combination of [`Strength`] and [`CaseLevel`] as follows:
///
/// <dl>
/// <dt><code>sensitivity: "base"</code></dt>
/// <dd><a href="enum.Strength.html#variant.Primary"><code>Strength::Primary</code></a></dd>
/// <dt><code>sensitivity: "accent"</code></dt>
/// <dd><a href="enum.Strength.html#variant.Secondary"><code>Strength::Secondary</code></a></dd>
/// <dt><code>sensitivity: "case"</code></dt>
/// <dd><a href="enum.Strength.html#variant.Primary"><code>Strength::Primary</code></a> and <a href="enum.CaseLevel.html#variant.On"><code>CaseLevel::On</code></a></dd>
/// <dt><code>sensitivity: "variant"</code></dt>
/// <dd><a href="enum.Strength.html#variant.Tertiary"><code>Strength::Tertiary</code></a></dd>
/// </dl>
///
/// ## Strength
///
/// This is the BCP47 key `ks`. The default is [`Strength::Tertiary`].
///
/// ## Alternate Handling
///
/// This is the BCP47 key `ka`. Note that `AlternateHandling::ShiftTrimmed` and
/// `AlternateHandling::Blanked` are unimplemented. The default is
/// [`AlternateHandling::NonIgnorable`], except
/// for Thai, whose default is [`AlternateHandling::Shifted`].
///
/// ## Case Level
///
/// See the [spec](https://www.unicode.org/reports/tr35/tr35-collation.html#Case_Parameters).
/// This is the BCP47 key `kc`. The default is [`CaseLevel::Off`].
///
/// ## Case First
///
/// See the [spec](https://www.unicode.org/reports/tr35/tr35-collation.html#Case_Parameters).
/// This is the BCP47 key `kf`. Three possibilities: [`CaseFirst::Off`] (default,
/// except for Danish and Maltese), [`CaseFirst::LowerFirst`], and [`CaseFirst::UpperFirst`]
/// (default for Danish and Maltese).
///
/// ## Backward second level
///
/// Compare the second level in backward order. This is the BCP47 key `kb`. `kb`
/// is prohibited by ECMA-402. The default is [`BackwardSecondLevel::Off`], except
/// for Canadian French.
///
/// ## Numeric
///
/// This is the BCP47 key `kn`. When set to [`Numeric::On`], any sequence of decimal
/// digits (General_Category = Nd) is sorted at the primary level according to the
/// numeric value. The default is [`Numeric::Off`].
///
/// # Unsupported BCP47 options
///
/// Reordering (BCP47 `kr`) currently cannot be set via the API and is implied
/// by the locale of the collation. `kr` is prohibited by ECMA-402.
///
/// Normalization is always enabled and cannot be turned off. Therefore, there
/// is no option corresponding to BCP47 `kk`. `kk` is prohibited by ECMA-402.
///
/// Hiragana quaternary handling is part of the strength for the Japanese
/// tailoring. The BCP47 key `kh` is unsupported. `kh` is deprecated and
/// prohibited by ECMA-402.
///
/// Variable top (BCP47 `vt`) is unsupported (use Max Variable instead). `vt`
/// is deprecated and prohibited by ECMA-402.
///
/// ## ECMA-402 Usage
///
/// ECMA-402 `usage: "search"` is represented as `-u-co-search` as part of the
/// locale in ICU4X. However, neither ECMA-402 nor ICU4X provides prefix matching
/// or substring matching API surface. This makes the utility of search collations
/// very narrow: With `-u-co-search`, [`Strength::Primary`], and observing whether
/// comparison output is [`core::cmp::Ordering::Equal`] (making no distinction between
/// [`core::cmp::Ordering::Less`] and [`core::cmp::Ordering::Greater`]), it is
/// possible to check if a set of human-readable strings contains a full-string
/// fuzzy match of a user-entered string, where "fuzzy" means case-insensitive and
/// accent-insensitive for scripts that have such concepts and something roughly
/// similar for other scripts.
///
/// Due to the very limited utility, ICU4X data does not include search collations
/// by default.
#[non_exhaustive]
#[derive(Debug, Copy, Clone, Default)]
pub struct CollatorOptions {
    /// User-specified strength collation option.
    pub strength: Option<Strength>,
    /// User-specified alternate handling collation option.
    pub alternate_handling: Option<AlternateHandling>,
    /// User-specified case first collation option.
    pub case_first: Option<CaseFirst>,
    /// User-specified max variable collation option.
    pub max_variable: Option<MaxVariable>,
    /// User-specified case level collation option.
    pub case_level: Option<CaseLevel>,
    /// User-specified numeric collation option.
    pub numeric: Option<Numeric>,
    /// User-specified backward second level collation option.
    pub backward_second_level: Option<BackwardSecondLevel>,
}

impl CollatorOptions {
    /// Create a new `CollatorOptions` with the defaults.
    pub const fn default() -> Self {
        Self {
            strength: None,
            alternate_handling: None,
            case_first: None,
            max_variable: None,
            case_level: None,
            numeric: None,
            backward_second_level: None,
        }
    }
}

// Make it possible to easily copy the resolved options of
// one collator into another collator.
impl From<ResolvedCollatorOptions> for CollatorOptions {
    /// Convenience conversion for copying the options from an
    /// existing collator into a new one (overriding any locale-provided
    /// defaults of the new one!).
    fn from(options: ResolvedCollatorOptions) -> CollatorOptions {
        Self {
            strength: Some(options.strength),
            alternate_handling: Some(options.alternate_handling),
            case_first: Some(options.case_first),
            max_variable: Some(options.max_variable),
            case_level: Some(options.case_level),
            numeric: Some(options.numeric),
            backward_second_level: Some(options.backward_second_level),
        }
    }
}

/// The resolved (actually used) options used by the collator.
///
/// See the documentation of `CollatorOptions`.
#[non_exhaustive]
#[derive(Debug, Copy, Clone)]
pub struct ResolvedCollatorOptions {
    /// Resolved strength collation option.
    pub strength: Strength,
    /// Resolved alternate handling collation option.
    pub alternate_handling: AlternateHandling,
    /// Resolved case first collation option.
    pub case_first: CaseFirst,
    /// Resolved max variable collation option.
    pub max_variable: MaxVariable,
    /// Resolved case level collation option.
    pub case_level: CaseLevel,
    /// Resolved numeric collation option.
    pub numeric: Numeric,
    /// Resolved backward second level collation option.
    pub backward_second_level: BackwardSecondLevel,
}

impl From<CollatorOptionsBitField> for ResolvedCollatorOptions {
    fn from(options: CollatorOptionsBitField) -> ResolvedCollatorOptions {
        Self {
            strength: options.strength(),
            alternate_handling: options.alternate_handling(),
            case_first: options.case_first(),
            max_variable: options.max_variable(),
            case_level: if options.case_level() {
                CaseLevel::On
            } else {
                CaseLevel::Off
            },
            numeric: if options.numeric() {
                Numeric::On
            } else {
                Numeric::Off
            },
            backward_second_level: if options.backward_second_level() {
                BackwardSecondLevel::On
            } else {
                BackwardSecondLevel::Off
            },
        }
    }
}

#[derive(Copy, Clone, Debug)]
pub(crate) struct CollatorOptionsBitField(u32);

impl Default for CollatorOptionsBitField {
    fn default() -> Self {
        Self::default()
    }
}

impl CollatorOptionsBitField {
    /// Bits 0..2 : Strength
    const STRENGTH_MASK: u32 = 0b111;
    /// Bits 3..4 : Alternate handling: 00 non-ignorable, 01 shifted,
    ///             10 reserved for shift-trimmed, 11 reserved for blanked.
    ///             In other words, bit 4 is currently always 0.
    const ALTERNATE_HANDLING_MASK: u32 = 1 << 3;
    /// Bits 5..6 : 2-bit max variable value to be shifted by `MAX_VARIABLE_SHIFT`.
    const MAX_VARIABLE_MASK: u32 = 0b01100000;
    const MAX_VARIABLE_SHIFT: u32 = 5;
    /// Bit     7 : Reserved for extending max variable.
    /// Bit     8 : Sort uppercase first if case level or case first is on.
    const UPPER_FIRST_MASK: u32 = 1 << 8;
    /// Bit     9 : Keep the case bits in the tertiary weight (they trump
    ///             other tertiary values)
    ///             unless case level is on (when they are *moved* into the separate case level).
    ///             By default, the case bits are removed from the tertiary weight (ignored).
    ///             When CASE_FIRST is off, UPPER_FIRST must be off too, corresponding to
    ///             the tri-value UCOL_CASE_FIRST attribute: UCOL_OFF vs. UCOL_LOWER_FIRST vs.
    ///             UCOL_UPPER_FIRST.
    const CASE_FIRST_MASK: u32 = 1 << 9;
    /// Bit    10 : Insert the case level between the secondary and tertiary levels.
    const CASE_LEVEL_MASK: u32 = 1 << 10;
    /// Bit    11 : Backward secondary level
    const BACKWARD_SECOND_LEVEL_MASK: u32 = 1 << 11;
    /// Bit    12 : Numeric
    const NUMERIC_MASK: u32 = 1 << 12;

    /// Whether strength is explicitly set.
    const EXPLICIT_STRENGTH_MASK: u32 = 1 << 31;
    /// Whether max variable is explicitly set.
    const EXPLICIT_MAX_VARIABLE_MASK: u32 = 1 << 30;
    /// Whether alternate handling is explicitly set.
    const EXPLICIT_ALTERNATE_HANDLING_MASK: u32 = 1 << 29;
    /// Whether case level is explicitly set.
    const EXPLICIT_CASE_LEVEL_MASK: u32 = 1 << 28;
    /// Whether case first is explicitly set.
    const EXPLICIT_CASE_FIRST_MASK: u32 = 1 << 27;
    /// Whether backward secondary is explicitly set.
    const EXPLICIT_BACKWARD_SECOND_LEVEL_MASK: u32 = 1 << 26;
    /// Whether numeric is explicitly set.
    const EXPLICIT_NUMERIC_MASK: u32 = 1 << 25;

    /// Create a new [`CollatorOptionsBitField`] with the defaults.
    pub const fn default() -> Self {
        Self(Strength::Tertiary as u32)
    }

    /// This is the BCP47 key `ks`.
    pub fn strength(&self) -> Strength {
        let mut bits = self.0 & CollatorOptionsBitField::STRENGTH_MASK;
        if !(bits <= 3 || bits == 7) {
            debug_assert!(false, "Bad value for strength.");
            // If the bits say higher than `Quaternary` but
            // lower than `Identical`, clamp to `Quaternary`.
            bits = 3;
        }
        // By construction above in range and, therefore,
        // never UB.
        unsafe { core::mem::transmute(bits as u8) }
    }

    /// This is the BCP47 key `ks`. See the enum for examples.
    pub fn set_strength(&mut self, strength: Option<Strength>) {
        self.0 &= !CollatorOptionsBitField::STRENGTH_MASK;
        if let Some(strength) = strength {
            self.0 |= CollatorOptionsBitField::EXPLICIT_STRENGTH_MASK;
            self.0 |= strength as u32;
        } else {
            self.0 &= !CollatorOptionsBitField::EXPLICIT_STRENGTH_MASK;
        }
    }

    /// The maximum character class that `AlternateHandling::Shifted`
    /// applies to.
    pub fn max_variable(&self) -> MaxVariable {
        // Safe, because we mask two bits and shift them to the low
        // two bits and the enum has values for 0 to 3, inclusive.
        unsafe {
            core::mem::transmute(
                ((self.0 & CollatorOptionsBitField::MAX_VARIABLE_MASK)
                    >> CollatorOptionsBitField::MAX_VARIABLE_SHIFT) as u8,
            )
        }
    }

    /// The maximum character class that `AlternateHandling::Shifted`
    /// applies to. See the enum for examples.
    pub fn set_max_variable(&mut self, max_variable: Option<MaxVariable>) {
        self.0 &= !CollatorOptionsBitField::MAX_VARIABLE_MASK;
        if let Some(max_variable) = max_variable {
            self.0 |= CollatorOptionsBitField::EXPLICIT_MAX_VARIABLE_MASK;
            self.0 |= (max_variable as u32) << CollatorOptionsBitField::MAX_VARIABLE_SHIFT;
        } else {
            self.0 &= !CollatorOptionsBitField::EXPLICIT_MAX_VARIABLE_MASK;
        }
    }

    /// Whether certain characters are moved from the primary level to
    /// the quaternary level.
    pub fn alternate_handling(&self) -> AlternateHandling {
        if (self.0 & CollatorOptionsBitField::ALTERNATE_HANDLING_MASK) != 0 {
            AlternateHandling::Shifted
        } else {
            AlternateHandling::NonIgnorable
        }
    }

    /// Whether certain characters are moved from the primary level to
    /// the quaternary level. See the enum for examples.
    pub fn set_alternate_handling(&mut self, alternate_handling: Option<AlternateHandling>) {
        self.0 &= !CollatorOptionsBitField::ALTERNATE_HANDLING_MASK;
        if let Some(alternate_handling) = alternate_handling {
            self.0 |= CollatorOptionsBitField::EXPLICIT_ALTERNATE_HANDLING_MASK;
            if alternate_handling == AlternateHandling::Shifted {
                self.0 |= CollatorOptionsBitField::ALTERNATE_HANDLING_MASK;
            }
        } else {
            self.0 &= !CollatorOptionsBitField::EXPLICIT_ALTERNATE_HANDLING_MASK;
        }
    }

    /// Whether there's a dedicated case level.
    pub fn case_level(&self) -> bool {
        (self.0 & CollatorOptionsBitField::CASE_LEVEL_MASK) != 0
    }

    /// Whether there's a dedicated case level. If `true`, detaches
    /// the case aspect of the tertiary level and inserts it between
    /// the secondary and tertiary levels. Can be combined with the
    /// primary-only strength. Setting this to `true` with
    /// `Strength::Primary` corresponds to the ECMA-402 sensitivity
    /// "case".
    ///
    /// See [the ICU guide](https://unicode-org.github.io/icu/userguide/collation/concepts.html#caselevel).
    pub fn set_case_level(&mut self, case_level: Option<bool>) {
        self.0 &= !CollatorOptionsBitField::CASE_LEVEL_MASK;
        if let Some(case_level) = case_level {
            self.0 |= CollatorOptionsBitField::EXPLICIT_CASE_LEVEL_MASK;
            if case_level {
                self.0 |= CollatorOptionsBitField::CASE_LEVEL_MASK;
            }
        } else {
            self.0 &= !CollatorOptionsBitField::EXPLICIT_CASE_LEVEL_MASK;
        }
    }

    pub fn set_case_level_from_enum(&mut self, case_level: Option<CaseLevel>) {
        match case_level {
            Some(CaseLevel::On) => {
                self.set_case_level(Some(true));
            }
            Some(CaseLevel::Off) => {
                self.set_case_level(Some(false));
            }
            _ => self.set_case_level(None),
        }
    }

    fn case_first(&self) -> CaseFirst {
        if (self.0 & CollatorOptionsBitField::CASE_FIRST_MASK) != 0 {
            if (self.0 & CollatorOptionsBitField::UPPER_FIRST_MASK) != 0 {
                CaseFirst::UpperFirst
            } else {
                CaseFirst::LowerFirst
            }
        } else {
            CaseFirst::Off
        }
    }

    /// Whether case is the most significant part of the tertiary
    /// level.
    ///
    /// See [the ICU guide](https://unicode-org.github.io/icu/userguide/collation/concepts.html#caselevel).
    pub fn set_case_first(&mut self, case_first: Option<CaseFirst>) {
        self.0 &=
            !(CollatorOptionsBitField::CASE_FIRST_MASK | CollatorOptionsBitField::UPPER_FIRST_MASK);
        if let Some(case_first) = case_first {
            self.0 |= CollatorOptionsBitField::EXPLICIT_CASE_FIRST_MASK;
            match case_first {
                CaseFirst::Off => {}
                CaseFirst::LowerFirst => {
                    self.0 |= CollatorOptionsBitField::CASE_FIRST_MASK;
                }
                CaseFirst::UpperFirst => {
                    self.0 |= CollatorOptionsBitField::CASE_FIRST_MASK;
                    self.0 |= CollatorOptionsBitField::UPPER_FIRST_MASK;
                }
            }
        } else {
            self.0 &= !CollatorOptionsBitField::EXPLICIT_CASE_FIRST_MASK;
        }
    }

    /// Whether second level compares the last accent difference
    /// instead of the first accent difference.
    pub fn backward_second_level(&self) -> bool {
        (self.0 & CollatorOptionsBitField::BACKWARD_SECOND_LEVEL_MASK) != 0
    }

    /// Whether second level compares the last accent difference
    /// instead of the first accent difference.
    pub fn set_backward_second_level(&mut self, backward_second_level: Option<bool>) {
        self.0 &= !CollatorOptionsBitField::BACKWARD_SECOND_LEVEL_MASK;
        if let Some(backward_second_level) = backward_second_level {
            self.0 |= CollatorOptionsBitField::EXPLICIT_BACKWARD_SECOND_LEVEL_MASK;
            if backward_second_level {
                self.0 |= CollatorOptionsBitField::BACKWARD_SECOND_LEVEL_MASK;
            }
        } else {
            self.0 &= !CollatorOptionsBitField::EXPLICIT_BACKWARD_SECOND_LEVEL_MASK;
        }
    }

    pub fn set_backward_second_level_from_enum(
        &mut self,
        backward_second_level: Option<BackwardSecondLevel>,
    ) {
        match backward_second_level {
            Some(BackwardSecondLevel::On) => {
                self.set_backward_second_level(Some(true));
            }
            Some(BackwardSecondLevel::Off) => {
                self.set_backward_second_level(Some(false));
            }
            None => self.set_backward_second_level(None),
        }
    }

    /// Whether sequences of decimal digits are compared according
    /// to their numeric value.
    pub fn numeric(&self) -> bool {
        (self.0 & CollatorOptionsBitField::NUMERIC_MASK) != 0
    }

    /// Whether sequences of decimal digits are compared according
    /// to their numeric value.
    pub fn set_numeric(&mut self, numeric: Option<bool>) {
        self.0 &= !CollatorOptionsBitField::NUMERIC_MASK;
        if let Some(numeric) = numeric {
            self.0 |= CollatorOptionsBitField::EXPLICIT_NUMERIC_MASK;
            if numeric {
                self.0 |= CollatorOptionsBitField::NUMERIC_MASK;
            }
        } else {
            self.0 &= !CollatorOptionsBitField::EXPLICIT_NUMERIC_MASK;
        }
    }

    pub fn set_numeric_from_enum(&mut self, numeric: Option<Numeric>) {
        match numeric {
            Some(Numeric::On) => {
                self.set_numeric(Some(true));
            }
            Some(Numeric::Off) => {
                self.set_numeric(Some(false));
            }
            None => self.set_numeric(None),
        }
    }

    /// If strength is <= secondary, returns `None`.
    /// Otherwise, returns the appropriate mask.
    pub(crate) fn tertiary_mask(&self) -> Option<u16> {
        if self.strength() <= Strength::Secondary {
            None
        } else if (self.0
            & (CollatorOptionsBitField::CASE_FIRST_MASK | CollatorOptionsBitField::CASE_LEVEL_MASK))
            == CollatorOptionsBitField::CASE_FIRST_MASK
        {
            Some(CASE_MASK | TERTIARY_MASK)
        } else {
            Some(TERTIARY_MASK)
        }
    }

    /// Internal upper first getter
    pub(crate) fn upper_first(&self) -> bool {
        (self.0 & CollatorOptionsBitField::UPPER_FIRST_MASK) != 0
    }

    /// For options left as defaults in this `CollatorOptions`,
    /// set the value from `other`. Values taken from `other`
    /// are marked as explicitly set if they were explicitly
    /// set in `other`.
    pub fn set_defaults(&mut self, other: CollatorOptionsBitField) {
        if self.0 & CollatorOptionsBitField::EXPLICIT_STRENGTH_MASK == 0 {
            self.0 &= !CollatorOptionsBitField::STRENGTH_MASK;
            self.0 |= other.0 & CollatorOptionsBitField::STRENGTH_MASK;
            self.0 |= other.0 & CollatorOptionsBitField::EXPLICIT_STRENGTH_MASK;
        }
        if self.0 & CollatorOptionsBitField::EXPLICIT_MAX_VARIABLE_MASK == 0 {
            self.0 &= !CollatorOptionsBitField::MAX_VARIABLE_MASK;
            self.0 |= other.0 & CollatorOptionsBitField::MAX_VARIABLE_MASK;
            self.0 |= other.0 & CollatorOptionsBitField::EXPLICIT_MAX_VARIABLE_MASK;
        }
        if self.0 & CollatorOptionsBitField::EXPLICIT_ALTERNATE_HANDLING_MASK == 0 {
            self.0 &= !CollatorOptionsBitField::ALTERNATE_HANDLING_MASK;
            self.0 |= other.0 & CollatorOptionsBitField::ALTERNATE_HANDLING_MASK;
            self.0 |= other.0 & CollatorOptionsBitField::EXPLICIT_ALTERNATE_HANDLING_MASK;
        }
        if self.0 & CollatorOptionsBitField::EXPLICIT_CASE_LEVEL_MASK == 0 {
            self.0 &= !CollatorOptionsBitField::CASE_LEVEL_MASK;
            self.0 |= other.0 & CollatorOptionsBitField::CASE_LEVEL_MASK;
            self.0 |= other.0 & CollatorOptionsBitField::EXPLICIT_CASE_LEVEL_MASK;
        }
        if self.0 & CollatorOptionsBitField::EXPLICIT_CASE_FIRST_MASK == 0 {
            self.0 &= !(CollatorOptionsBitField::CASE_FIRST_MASK
                | CollatorOptionsBitField::UPPER_FIRST_MASK);
            self.0 |= other.0
                & (CollatorOptionsBitField::CASE_FIRST_MASK
                    | CollatorOptionsBitField::UPPER_FIRST_MASK);
            self.0 |= other.0 & CollatorOptionsBitField::EXPLICIT_CASE_FIRST_MASK;
        }
        if self.0 & CollatorOptionsBitField::EXPLICIT_BACKWARD_SECOND_LEVEL_MASK == 0 {
            self.0 &= !CollatorOptionsBitField::BACKWARD_SECOND_LEVEL_MASK;
            self.0 |= other.0 & CollatorOptionsBitField::BACKWARD_SECOND_LEVEL_MASK;
            self.0 |= other.0 & CollatorOptionsBitField::EXPLICIT_BACKWARD_SECOND_LEVEL_MASK;
        }
        if self.0 & CollatorOptionsBitField::EXPLICIT_NUMERIC_MASK == 0 {
            self.0 &= !CollatorOptionsBitField::NUMERIC_MASK;
            self.0 |= other.0 & CollatorOptionsBitField::NUMERIC_MASK;
            self.0 |= other.0 & CollatorOptionsBitField::EXPLICIT_NUMERIC_MASK;
        }
    }
}

impl From<CollatorOptions> for CollatorOptionsBitField {
    fn from(options: CollatorOptions) -> CollatorOptionsBitField {
        let mut result = Self::default();
        result.set_strength(options.strength);
        result.set_max_variable(options.max_variable);
        result.set_alternate_handling(options.alternate_handling);
        result.set_case_level_from_enum(options.case_level);
        result.set_case_first(options.case_first);
        result.set_numeric_from_enum(options.numeric);
        result.set_backward_second_level_from_enum(options.backward_second_level);
        result
    }
}
