/*!
`sfv` is an implementation of *Structured Field Values for HTTP*, as specified in [RFC 9651](https://httpwg.org/specs/rfc9651.html) for parsing and serializing HTTP field values.
It also exposes a set of types that might be useful for defining new structured fields.

# Data Structures

There are three types of structured fields:

- `Item` -- an `Integer`, `Decimal`, `String`, `Token`, `Byte Sequence`, `Boolean`, `Date`, or `Display String`. It can have associated `Parameters`.
- `List` -- an array of zero or more members, each of which can be an `Item` or an `InnerList`, both of which can have `Parameters`.
- `Dictionary` -- an ordered map of name-value pairs, where the names are short textual strings and the values are `Item`s or arrays of `Items` (represented with `InnerList`), both of which can have associated parameters. There can be zero or more members, and their names are unique in the scope of the `Dictionary` they occur within.

There are also a few lower-level types used to construct structured field values:
- `BareItem` is used as `Item`'s value or as a parameter value in `Parameters`.
- `Parameters` are an ordered map of key-value pairs that are associated with an `Item` or `InnerList`. The keys are unique within the scope the `Parameters` they occur within, and the values are `BareItem`.
- `InnerList` is an array of zero or more `Items`. Can have associated `Parameters`.
- `ListEntry` represents either `Item` or `InnerList` as a member of `List` or as member-value in `Dictionary`.

# Examples

*/
#![cfg_attr(
    feature = "parsed-types",
    doc = r##"
### Parsing

```
# use sfv::{Dictionary, Item, List, Parser};
# fn main() -> Result<(), sfv::Error> {
// Parsing a structured field value of Item type.
let input = "12.445;foo=bar";
let item: Item = Parser::new(input).parse()?;
println!("{:#?}", item);

// Parsing a structured field value of List type.
let input = r#"1;a=tok, ("foo" "bar");baz, ()"#;
let list: List = Parser::new(input).parse()?;
println!("{:#?}", list);

// Parsing a structured field value of Dictionary type.
let input = "a=?0, b, c; foo=bar, rating=1.5, fruits=(apple pear)";
let dict: Dictionary = Parser::new(input).parse()?;
println!("{:#?}", dict);
# Ok(())
# }
```

### Getting Parsed Value Members
```
# use sfv::*;
# fn main() -> Result<(), sfv::Error> {
let input = "u=2, n=(* foo 2)";
let dict: Dictionary = Parser::new(input).parse()?;

match dict.get("u") {
    Some(ListEntry::Item(item)) => match &item.bare_item {
        BareItem::Token(val) => { /* ... */ }
        BareItem::Integer(val) => { /* ... */ }
        BareItem::Boolean(val) => { /* ... */ }
        BareItem::Decimal(val) => { /* ... */ }
        BareItem::String(val) => { /* ... */ }
        BareItem::ByteSequence(val) => { /* ... */ }
        BareItem::Date(val) => { /* ... */ }
        BareItem::DisplayString(val) => { /* ... */ }
    },
    Some(ListEntry::InnerList(inner_list)) => { /* ... */ }
    None => { /* ... */ }
}
# Ok(())
# }
```
"##
)]
/*!
### Serialization
Serializes an `Item`:
```
use sfv::{Decimal, ItemSerializer, KeyRef, StringRef};

# fn main() -> Result<(), sfv::Error> {
let serialized_item = ItemSerializer::new()
    .bare_item(StringRef::from_str("foo")?)
    .parameter(KeyRef::from_str("key")?, Decimal::try_from(13.45655)?)
    .finish();

assert_eq!(serialized_item, r#""foo";key=13.457"#);
# Ok(())
# }
```

Serializes a `List`:
```
use sfv::{KeyRef, ListSerializer, StringRef, TokenRef};

# fn main() -> Result<(), sfv::Error> {
let mut ser = ListSerializer::new();

ser.bare_item(TokenRef::from_str("tok")?);

{
    let mut ser = ser.inner_list();

    ser.bare_item(99).parameter(KeyRef::from_str("key")?, false);

    ser.bare_item(StringRef::from_str("foo")?);

    ser.finish().parameter(KeyRef::from_str("bar")?, true);
}

assert_eq!(
    ser.finish().as_deref(),
    Some(r#"tok, (99;key=?0 "foo");bar"#),
);
# Ok(())
# }
```

Serializes a `Dictionary`:
```
use sfv::{DictSerializer, KeyRef, StringRef};

# fn main() -> Result<(), sfv::Error> {
let mut ser = DictSerializer::new();

ser.bare_item(KeyRef::from_str("key1")?, StringRef::from_str("apple")?);

ser.bare_item(KeyRef::from_str("key2")?, true);

ser.bare_item(KeyRef::from_str("key3")?, false);

assert_eq!(
    ser.finish().as_deref(),
    Some(r#"key1="apple", key2, key3=?0"#),
);
# Ok(())
# }
```

# Crate features

- `parsed-types` (enabled by default) -- When enabled, exposes fully owned types
  [`Item`], [`Dictionary`], [`List`], and their components, which can be obtained from
  [`Parser::parse`], etc. These types are implemented using the
  [`indexmap`](https://crates.io/crates/indexmap) crate, so disabling this
  feature can avoid that dependency if parsing using a visitor
  ([`Parser::parse_item_with_visitor`], etc.) is sufficient.

- `arbitrary` -- Implements the
  [`Arbitrary`](https://docs.rs/arbitrary/1.4.1/arbitrary/trait.Arbitrary.html)
  trait for this crate's types, making them easier to use with fuzzing.
*/

#![deny(missing_docs)]
#![deny(missing_debug_implementations)]

mod date;
mod decimal;
mod error;
mod integer;
mod key;
#[cfg(feature = "parsed-types")]
mod parsed;
mod parser;
mod ref_serializer;
mod serializer;
mod string;
mod token;
mod utils;
pub mod visitor;

#[cfg(test)]
mod test_decimal;
#[cfg(test)]
mod test_integer;
#[cfg(test)]
mod test_key;
#[cfg(test)]
mod test_parser;
#[cfg(test)]
mod test_ref_serializer;
#[cfg(test)]
mod test_serializer;
#[cfg(test)]
mod test_string;
#[cfg(test)]
mod test_token;

use std::borrow::{Borrow, Cow};
use std::fmt;
use std::string::String as StdString;

pub use date::Date;
pub use decimal::Decimal;
pub use error::Error;
pub use integer::{Integer, integer};
pub use key::{Key, KeyRef, key_ref};
#[cfg(feature = "parsed-types")]
pub use parsed::{Dictionary, FieldType, InnerList, Item, List, ListEntry, Parameters};
pub use parser::Parser;
pub use ref_serializer::{
    DictSerializer, InnerListSerializer, ItemSerializer, ListSerializer, ParameterSerializer,
};
pub use string::{String, StringRef, string_ref};
pub use token::{Token, TokenRef, token_ref};

type SFVResult<T> = std::result::Result<T, Error>;

/// An abstraction over multiple kinds of ownership of a [bare item].
///
/// In general most users will be interested in:
/// - [`BareItem`], for completely owned data
/// - [`RefBareItem`], for completely borrowed data
/// - [`BareItemFromInput`], for data borrowed from input when possible
///
/// [bare item]: <https://httpwg.org/specs/rfc9651.html#item>
#[derive(Debug, Clone, Copy)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
pub enum GenericBareItem<S, B, T, D> {
    /// A [decimal](https://httpwg.org/specs/rfc9651.html#decimal).
    // sf-decimal  = ["-"] 1*12DIGIT "." 1*3DIGIT
    Decimal(Decimal),
    /// An [integer](https://httpwg.org/specs/rfc9651.html#integer).
    // sf-integer = ["-"] 1*15DIGIT
    Integer(Integer),
    /// A [string](https://httpwg.org/specs/rfc9651.html#string).
    // sf-string = DQUOTE *chr DQUOTE
    // chr       = unescaped / escaped
    // unescaped = %x20-21 / %x23-5B / %x5D-7E
    // escaped   = "\" ( DQUOTE / "\" )
    String(S),
    /// A [byte sequence](https://httpwg.org/specs/rfc9651.html#binary).
    // ":" *(base64) ":"
    // base64    = ALPHA / DIGIT / "+" / "/" / "="
    ByteSequence(B),
    /// A [boolean](https://httpwg.org/specs/rfc9651.html#boolean).
    // sf-boolean = "?" boolean
    // boolean    = "0" / "1"
    Boolean(bool),
    /// A [token](https://httpwg.org/specs/rfc9651.html#token).
    // sf-token = ( ALPHA / "*" ) *( tchar / ":" / "/" )
    Token(T),
    /// A [date](https://httpwg.org/specs/rfc9651.html#date).
    ///
    /// [`Parser`] will never produce this variant when used with
    /// [`Version::Rfc8941`].
    // sf-date = "@" sf-integer
    Date(Date),
    /// A [display string](https://httpwg.org/specs/rfc9651.html#displaystring).
    ///
    /// Display Strings are similar to [`String`]s, in that they consist of zero
    /// or more characters, but they allow Unicode scalar values (i.e., all
    /// Unicode code points except for surrogates), unlike [`String`]s.
    ///
    /// [`Parser`] will never produce this variant when used with
    /// [`Version::Rfc8941`].
    ///
    /// [display string]: <https://httpwg.org/specs/rfc9651.html#displaystring>
    // sf-displaystring = "%" DQUOTE *( unescaped / "\" / pct-encoded ) DQUOTE
    // pct-encoded      = "%" lc-hexdig lc-hexdig
    // lc-hexdig        = DIGIT / %x61-66 ; 0-9, a-f
    DisplayString(D),
}

impl<S, B, T, D> GenericBareItem<S, B, T, D> {
    /// If the bare item is a decimal, returns it; otherwise returns `None`.
    #[must_use]
    pub fn as_decimal(&self) -> Option<Decimal> {
        match *self {
            Self::Decimal(val) => Some(val),
            _ => None,
        }
    }

    /// If the bare item is an integer, returns it; otherwise returns `None`.
    #[must_use]
    pub fn as_integer(&self) -> Option<Integer> {
        match *self {
            Self::Integer(val) => Some(val),
            _ => None,
        }
    }

    /// If the bare item is a string, returns a reference to it; otherwise returns `None`.
    #[must_use]
    pub fn as_string(&self) -> Option<&StringRef>
    where
        S: Borrow<StringRef>,
    {
        match *self {
            Self::String(ref val) => Some(val.borrow()),
            _ => None,
        }
    }

    /// If the bare item is a byte sequence, returns a reference to it; otherwise returns `None`.
    #[must_use]
    pub fn as_byte_sequence(&self) -> Option<&[u8]>
    where
        B: Borrow<[u8]>,
    {
        match *self {
            Self::ByteSequence(ref val) => Some(val.borrow()),
            _ => None,
        }
    }

    /// If the bare item is a boolean, returns it; otherwise returns `None`.
    #[must_use]
    pub fn as_boolean(&self) -> Option<bool> {
        match *self {
            Self::Boolean(val) => Some(val),
            _ => None,
        }
    }

    /// If the bare item is a token, returns a reference to it; otherwise returns `None`.
    #[must_use]
    pub fn as_token(&self) -> Option<&TokenRef>
    where
        T: Borrow<TokenRef>,
    {
        match *self {
            Self::Token(ref val) => Some(val.borrow()),
            _ => None,
        }
    }

    /// If the bare item is a date, returns it; otherwise returns `None`.
    #[must_use]
    pub fn as_date(&self) -> Option<Date> {
        match *self {
            Self::Date(val) => Some(val),
            _ => None,
        }
    }

    /// If the bare item is a display string, returns a reference to it; otherwise returns `None`.
    #[must_use]
    pub fn as_display_string(&self) -> Option<&D> {
        match *self {
            Self::DisplayString(ref val) => Some(val),
            _ => None,
        }
    }
}

impl<S, B, T, D> From<Integer> for GenericBareItem<S, B, T, D> {
    fn from(val: Integer) -> Self {
        Self::Integer(val)
    }
}

impl<S, B, T, D> From<bool> for GenericBareItem<S, B, T, D> {
    fn from(val: bool) -> Self {
        Self::Boolean(val)
    }
}

impl<S, B, T, D> From<Decimal> for GenericBareItem<S, B, T, D> {
    fn from(val: Decimal) -> Self {
        Self::Decimal(val)
    }
}

impl<S, B, T, D> From<Date> for GenericBareItem<S, B, T, D> {
    fn from(val: Date) -> Self {
        Self::Date(val)
    }
}

impl<S, B, T, D> TryFrom<f32> for GenericBareItem<S, B, T, D> {
    type Error = Error;

    fn try_from(val: f32) -> Result<Self, Error> {
        Decimal::try_from(val).map(Self::Decimal)
    }
}

impl<S, B, T, D> TryFrom<f64> for GenericBareItem<S, B, T, D> {
    type Error = Error;

    fn try_from(val: f64) -> Result<Self, Error> {
        Decimal::try_from(val).map(Self::Decimal)
    }
}

impl<S, T, D> From<Vec<u8>> for GenericBareItem<S, Vec<u8>, T, D> {
    fn from(val: Vec<u8>) -> Self {
        Self::ByteSequence(val)
    }
}

impl<S, B, D> From<Token> for GenericBareItem<S, B, Token, D> {
    fn from(val: Token) -> Self {
        Self::Token(val)
    }
}

impl<B, T, D> From<String> for GenericBareItem<String, B, T, D> {
    fn from(val: String) -> Self {
        Self::String(val)
    }
}

impl<'a, S, T, D> From<&'a [u8]> for GenericBareItem<S, Vec<u8>, T, D> {
    fn from(val: &'a [u8]) -> Self {
        Self::ByteSequence(val.to_owned())
    }
}

impl<'a, S, B, D> From<&'a TokenRef> for GenericBareItem<S, B, Token, D> {
    fn from(val: &'a TokenRef) -> Self {
        Self::Token(val.to_owned())
    }
}

impl<'a, B, T, D> From<&'a StringRef> for GenericBareItem<String, B, T, D> {
    fn from(val: &'a StringRef) -> Self {
        Self::String(val.to_owned())
    }
}

/// A [bare item] that owns its data.
///
/// [bare item]: <https://httpwg.org/specs/rfc9651.html#item>
#[cfg_attr(
    feature = "parsed-types",
    doc = "Used to construct an [`Item`] or [`Parameters`] values."
)]
///
/// Note: This type deliberately does not implement `From<StdString>` as a
/// shorthand for [`BareItem::DisplayString`] because it is too easy to confuse
/// with conversions from [`String`]:
///
/// ```compile_fail
/// # use sfv::BareItem;
/// let _: BareItem = "x".to_owned().into();
/// ```
///
/// Instead, use:
///
/// ```
/// # use sfv::BareItem;
/// let _ = BareItem::DisplayString("x".to_owned());
/// ```
pub type BareItem = GenericBareItem<String, Vec<u8>, Token, StdString>;

/// A [bare item] that borrows its data.
///
/// Used to serialize values via [`ItemSerializer`], [`ListSerializer`], and [`DictSerializer`].
///
/// [bare item]: <https://httpwg.org/specs/rfc9651.html#item>
///
/// Note: This type deliberately does not implement `From<&str>` as a shorthand
/// for [`RefBareItem::DisplayString`] because it is too easy to confuse with
/// conversions from [`StringRef`]:
///
/// ```compile_fail
/// # use sfv::RefBareItem;
/// let _: RefBareItem = "x".into();
/// ```
///
/// Instead, use:
///
/// ```
/// # use sfv::RefBareItem;
/// let _ = RefBareItem::DisplayString("x");
/// ```
pub type RefBareItem<'a> = GenericBareItem<&'a StringRef, &'a [u8], &'a TokenRef, &'a str>;

/// A [bare item] that borrows data from input when possible.
///
/// Used to parse input incrementally in the [`visitor`] module.
///
/// [bare item]: <https://httpwg.org/specs/rfc9651.html#item>
///
/// Note: This type deliberately does not implement `From<Cow<str>>` as a
/// shorthand for [`BareItemFromInput::DisplayString`] because it is too easy to
/// confuse with conversions from [`Cow<StringRef>`]:
///
/// ```compile_fail
/// # use sfv::BareItemFromInput;
/// # use std::borrow::Cow;
/// let _: BareItemFromInput = "x".to_owned().into();
/// ```
///
/// Instead, use:
///
/// ```
/// # use sfv::BareItemFromInput;
/// # use std::borrow::Cow;
/// let _ = BareItemFromInput::DisplayString(Cow::Borrowed("x"));
/// ```
pub type BareItemFromInput<'a> =
    GenericBareItem<Cow<'a, StringRef>, Vec<u8>, &'a TokenRef, Cow<'a, str>>;

impl<'a, S, B, T, D> From<&'a GenericBareItem<S, B, T, D>> for RefBareItem<'a>
where
    S: Borrow<StringRef>,
    B: Borrow<[u8]>,
    T: Borrow<TokenRef>,
    D: Borrow<str>,
{
    fn from(val: &'a GenericBareItem<S, B, T, D>) -> RefBareItem<'a> {
        match val {
            GenericBareItem::Integer(val) => RefBareItem::Integer(*val),
            GenericBareItem::Decimal(val) => RefBareItem::Decimal(*val),
            GenericBareItem::String(val) => RefBareItem::String(val.borrow()),
            GenericBareItem::ByteSequence(val) => RefBareItem::ByteSequence(val.borrow()),
            GenericBareItem::Boolean(val) => RefBareItem::Boolean(*val),
            GenericBareItem::Token(val) => RefBareItem::Token(val.borrow()),
            GenericBareItem::Date(val) => RefBareItem::Date(*val),
            GenericBareItem::DisplayString(val) => RefBareItem::DisplayString(val.borrow()),
        }
    }
}

impl<'a> From<BareItemFromInput<'a>> for BareItem {
    fn from(val: BareItemFromInput<'a>) -> BareItem {
        match val {
            BareItemFromInput::Integer(val) => BareItem::Integer(val),
            BareItemFromInput::Decimal(val) => BareItem::Decimal(val),
            BareItemFromInput::String(val) => BareItem::String(val.into_owned()),
            BareItemFromInput::ByteSequence(val) => BareItem::ByteSequence(val),
            BareItemFromInput::Boolean(val) => BareItem::Boolean(val),
            BareItemFromInput::Token(val) => BareItem::Token(val.to_owned()),
            BareItemFromInput::Date(val) => BareItem::Date(val),
            BareItemFromInput::DisplayString(val) => BareItem::DisplayString(val.into_owned()),
        }
    }
}

impl<'a> From<RefBareItem<'a>> for BareItem {
    fn from(val: RefBareItem<'a>) -> BareItem {
        match val {
            RefBareItem::Integer(val) => BareItem::Integer(val),
            RefBareItem::Decimal(val) => BareItem::Decimal(val),
            RefBareItem::String(val) => BareItem::String(val.to_owned()),
            RefBareItem::ByteSequence(val) => BareItem::ByteSequence(val.to_owned()),
            RefBareItem::Boolean(val) => BareItem::Boolean(val),
            RefBareItem::Token(val) => BareItem::Token(val.to_owned()),
            RefBareItem::Date(val) => BareItem::Date(val),
            RefBareItem::DisplayString(val) => BareItem::DisplayString(val.to_owned()),
        }
    }
}

impl<'a, S, T, D> From<&'a [u8]> for GenericBareItem<S, &'a [u8], T, D> {
    fn from(val: &'a [u8]) -> Self {
        Self::ByteSequence(val)
    }
}

impl<'a, S, B, D> From<&'a Token> for GenericBareItem<S, B, &'a TokenRef, D> {
    fn from(val: &'a Token) -> Self {
        Self::Token(val)
    }
}

impl<'a, S, B, D> From<&'a TokenRef> for GenericBareItem<S, B, &'a TokenRef, D> {
    fn from(val: &'a TokenRef) -> Self {
        Self::Token(val)
    }
}

impl<'a, B, T, D> From<&'a String> for GenericBareItem<&'a StringRef, B, T, D> {
    fn from(val: &'a String) -> Self {
        Self::String(val)
    }
}

impl<'a, B, T, D> From<&'a StringRef> for GenericBareItem<&'a StringRef, B, T, D> {
    fn from(val: &'a StringRef) -> Self {
        Self::String(val)
    }
}

impl<S1, B1, T1, D1, S2, B2, T2, D2> PartialEq<GenericBareItem<S2, B2, T2, D2>>
    for GenericBareItem<S1, B1, T1, D1>
where
    for<'a> RefBareItem<'a>: From<&'a Self>,
    for<'a> RefBareItem<'a>: From<&'a GenericBareItem<S2, B2, T2, D2>>,
{
    fn eq(&self, other: &GenericBareItem<S2, B2, T2, D2>) -> bool {
        match (RefBareItem::from(self), RefBareItem::from(other)) {
            (RefBareItem::Integer(a), RefBareItem::Integer(b)) => a == b,
            (RefBareItem::Decimal(a), RefBareItem::Decimal(b)) => a == b,
            (RefBareItem::String(a), RefBareItem::String(b)) => a == b,
            (RefBareItem::ByteSequence(a), RefBareItem::ByteSequence(b)) => a == b,
            (RefBareItem::Boolean(a), RefBareItem::Boolean(b)) => a == b,
            (RefBareItem::Token(a), RefBareItem::Token(b)) => a == b,
            (RefBareItem::Date(a), RefBareItem::Date(b)) => a == b,
            (RefBareItem::DisplayString(a), RefBareItem::DisplayString(b)) => a == b,
            _ => false,
        }
    }
}

/// A version for serialized structured field values.
///
/// Each HTTP specification that uses structured field values must indicate
/// which version it uses. See [the guidance from RFC 9651][RFC 9651] for details.
///
/// [RFC 9651]: <https://httpwg.org/specs/rfc9651.html#using-new-structured-types-in-extensions>
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
pub enum Version {
    /// [RFC 8941], which does not support dates or display strings.
    ///
    /// [RFC 8941]: <https://httpwg.org/specs/rfc8941.html>
    Rfc8941,
    /// [RFC 9651], which supports dates and display strings.
    ///
    /// [RFC 9651]: <https://httpwg.org/specs/rfc9651.html>
    Rfc9651,
}

impl fmt::Display for Version {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(match self {
            Self::Rfc8941 => "RFC 8941",
            Self::Rfc9651 => "RFC 9651",
        })
    }
}

mod private {
    #[allow(unused)]
    pub trait Sealed {}
}
