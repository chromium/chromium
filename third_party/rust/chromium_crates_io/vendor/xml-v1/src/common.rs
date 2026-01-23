//! Contains common types and functions used throughout the library.

use std::fmt;

/// Represents a position inside some textual document.
#[derive(Copy, Clone, PartialEq, Eq)]
pub struct TextPosition {
    #[doc(hidden)]
    pub row: u64,

    #[doc(hidden)]
    pub column: u64,
}

impl TextPosition {
    /// Creates a new position initialized to the beginning of the document
    #[inline]
    #[must_use]
    pub const fn new() -> Self {
        Self { row: 0, column: 0 }
    }

    /// Advances the position in a line
    #[inline]
    pub fn advance(&mut self, count: u8) {
        self.column += u64::from(count);
    }

    #[doc(hidden)]
    #[deprecated]
    pub fn advance_to_tab(&mut self, width: u8) {
        let width = u64::from(width);
        self.column += width - self.column % width;
    }

    /// Advances the position to the beginning of the next line
    #[inline]
    pub fn new_line(&mut self) {
        self.column = 0;
        self.row += 1;
    }

    /// Row, counting from 0. Add 1 to display as users expect!
    #[must_use]
    pub fn row(&self) -> u64 {
        self.row
    }

    /// Column, counting from 0. Add 1 to display as users expect!
    #[must_use]
    pub fn column(&self) -> u64 {
        self.column
    }
}

impl fmt::Debug for TextPosition {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(self, f)
    }
}

impl fmt::Display for TextPosition {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}:{}", self.row + 1, self.column + 1)
    }
}

/// Get the position in the document corresponding to the object
///
/// This trait is implemented by parsers, lexers and errors.
pub trait Position {
    /// Returns the current position or a position corresponding to the object.
    fn position(&self) -> TextPosition;
}

impl Position for TextPosition {
    #[inline]
    fn position(&self) -> TextPosition {
        *self
    }
}

/// XML version enumeration.
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub enum XmlVersion {
    /// XML version 1.0, or any 1.x version other than 1.1
    ///
    /// All future versions are disallowed since XML 1.1, so any version beyond 1.1 is an error tolerated only in XML 1.0.
    /// <https://www.w3.org/TR/REC-xml/#sec-prolog-dtd>
    Version10,

    /// XML version 1.1.
    Version11,
}

impl XmlVersion {
    /// Convenience helper which returns a string representation of the given version.
    ///
    /// ```
    /// # use xml::common::XmlVersion;
    /// assert_eq!(XmlVersion::Version10.as_str(), "1.0");
    /// assert_eq!(XmlVersion::Version11.as_str(), "1.1");
    /// ```
    #[must_use] 
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Version10 => "1.0",
            Self::Version11 => "1.1",
        }
    }
}

impl fmt::Display for XmlVersion {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.as_str().fmt(f)
    }
}

impl fmt::Debug for XmlVersion {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(self, f)
    }
}

/// Checks whether the given character is a white space character (`S`)
/// as is defined by XML 1.1 specification, [section 2.3][1].
///
/// [1]: http://www.w3.org/TR/2006/REC-xml11-20060816/#sec-common-syn
#[must_use]
#[inline]
pub const fn is_whitespace_char(c: char) -> bool {
    matches!(c, '\x20' | '\x0a' | '\x09' | '\x0d')
}

/// Matches the PubIdChar production.
pub (crate) fn is_pubid_char(c: char) -> bool {
    matches!(c, '\x20' | '\x0D' | '\x0A' | 'a'..='z' | 'A'..='Z' | '0'..='9' |
        '-' | '\'' | '(' | ')' | '+' | ',' | '.' | '/' | ':' | '=' | '?' | ';' |
        '!' | '*' | '#' | '@' | '$' | '_' | '%')
}

/// Checks whether the given string is compound only by white space
/// characters (`S`) using the previous `is_whitespace_char` to check
/// all characters of this string
pub fn is_whitespace_str(s: &str) -> bool {
    s.chars().all(is_whitespace_char)
}

/// Is it a valid character in XML 1.0
#[must_use]
pub const fn is_xml10_char(c: char) -> bool {
    matches!(c, '\u{09}' | '\u{0A}' | '\u{0D}' | '\u{20}'..='\u{D7FF}' | '\u{E000}'..='\u{FFFD}' | '\u{10000}'..)
}

/// Is it a valid character in XML 1.1
#[must_use]
pub const fn is_xml11_char(c: char) -> bool {
    matches!(c, '\u{01}'..='\u{D7FF}' | '\u{E000}'..='\u{FFFD}' | '\u{10000}'..)
}

/// Is it a valid character in XML 1.1 but not part of the restricted character set
#[must_use]
pub const fn is_xml11_char_not_restricted(c: char) -> bool {
    is_xml11_char(c) &&
        !matches!(c, '\u{01}'..='\u{08}' | '\u{0B}'..='\u{0C}' | '\u{0E}'..='\u{1F}' | '\u{7F}'..='\u{84}' | '\u{86}'..='\u{9F}')
}

/// Checks whether the given character is a name start character (`NameStartChar`)
/// as is defined by XML 1.1 specification, [section 2.3][1].
///
/// [1]: http://www.w3.org/TR/2006/REC-xml11-20060816/#sec-common-syn
#[must_use]
pub const fn is_name_start_char(c: char) -> bool {
    matches!(c,
        ':' | 'A'..='Z' | '_' | 'a'..='z' |
        '\u{C0}'..='\u{D6}' | '\u{D8}'..='\u{F6}' | '\u{F8}'..='\u{2FF}' |
        '\u{370}'..='\u{37D}' | '\u{37F}'..='\u{1FFF}' |
        '\u{200C}'..='\u{200D}' | '\u{2070}'..='\u{218F}' |
        '\u{2C00}'..='\u{2FEF}' | '\u{3001}'..='\u{D7FF}' |
        '\u{F900}'..='\u{FDCF}' | '\u{FDF0}'..='\u{FFFD}' |
        '\u{10000}'..='\u{EFFFF}'
    )
}

/// Checks whether the given character is a name character (`NameChar`)
/// as is defined by XML 1.1 specification, [section 2.3][1].
///
/// [1]: http://www.w3.org/TR/2006/REC-xml11-20060816/#sec-common-syn
#[must_use]
pub const fn is_name_char(c: char) -> bool {
    if is_name_start_char(c) {
        return true;
    }
    matches!(c,
        '-' | '.' | '0'..='9' | '\u{B7}' |
        '\u{300}'..='\u{36F}' | '\u{203F}'..='\u{2040}'
    )
}
