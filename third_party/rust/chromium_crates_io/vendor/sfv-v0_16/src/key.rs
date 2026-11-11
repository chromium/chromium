use std::{borrow::Borrow, fmt};

use crate::{
    error::{Error, NonEmptyStringError},
    utils,
};

/// An owned structured field value [key].
///
/// Keys must match the following regular expression:
///
/// ```re
/// ^[a-z*][a-z*0-9_\-.]*$
/// ```
///
/// [key]: <https://httpwg.org/specs/rfc9651.html#key>
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Key(String);

/// A borrowed structured field value [key].
///
/// Keys must match the following regular expression:
///
/// ```re
/// ^[a-z*][a-z*0-9_\-.]*$
/// ```
///
/// This type is to [`Key`] as [`str`] is to [`String`].
///
/// [key]: <https://httpwg.org/specs/rfc9651.html#key>
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Hash, ref_cast::RefCastCustom)]
#[repr(transparent)]
pub struct KeyRef(str);

const fn validate(v: &[u8]) -> Result<(), NonEmptyStringError> {
    if v.is_empty() {
        return Err(NonEmptyStringError::empty());
    }

    if !utils::is_allowed_start_key_char(v[0]) {
        return Err(NonEmptyStringError::invalid_character(0));
    }

    let mut index = 1;

    while index < v.len() {
        if !utils::is_allowed_inner_key_char(v[index]) {
            return Err(NonEmptyStringError::invalid_character(index));
        }
        index += 1;
    }

    Ok(())
}

impl KeyRef {
    #[ref_cast::ref_cast_custom]
    const fn cast(v: &str) -> &Self;

    /// Creates a `&KeyRef` from a `&str`.
    ///
    /// # Errors
    /// If the input string validation fails.
    #[allow(clippy::should_implement_trait)]
    pub fn from_str(v: &str) -> Result<&Self, Error> {
        validate(v.as_bytes())?;
        Ok(Self::cast(v))
    }

    /// Creates a `&KeyRef` from a `&[u8]`.
    ///
    /// # Errors
    /// If the input byte slice validation fails.
    pub fn from_bytes(v: &[u8]) -> Result<&Self, Error> {
        validate(v)?;
        // TODO: The UTF-8 validation is redundant with the preceding character checks, but
        // its removal is only possible with unsafe code.
        let Ok(s) = std::str::from_utf8(v) else {
            unreachable!()
        };
        Ok(Self::cast(s))
    }

    // Like `from_str`, but assumes that the contents of the string have already
    // been validated as a key.
    pub(crate) fn from_validated_str(v: &str) -> &Self {
        debug_assert!(validate(v.as_bytes()).is_ok());
        Self::cast(v)
    }

    /// Creates a `&KeyRef`.
    ///
    /// This method is intended to be called from `const` contexts in which the
    /// value is known to be valid. Use [`KeyRef::from_str`] for non-panicking
    /// conversions.
    ///
    /// # Panics
    ///
    /// Panics if the value is invalid.
    #[must_use]
    pub const fn constant(v: &str) -> &Self {
        match validate(v.as_bytes()) {
            Ok(()) => Self::cast(v),
            Err(err) => panic!("{}", err.msg()),
        }
    }

    /// Returns the key as a `&str`.
    #[must_use]
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

impl ToOwned for KeyRef {
    type Owned = Key;

    fn to_owned(&self) -> Key {
        Key(self.0.to_owned())
    }

    fn clone_into(&self, target: &mut Key) {
        self.0.clone_into(&mut target.0);
    }
}

impl Borrow<KeyRef> for Key {
    fn borrow(&self) -> &KeyRef {
        self
    }
}

impl std::ops::Deref for Key {
    type Target = KeyRef;

    fn deref(&self) -> &KeyRef {
        KeyRef::cast(&self.0)
    }
}

impl From<Key> for String {
    fn from(v: Key) -> String {
        v.0
    }
}

impl TryFrom<String> for Key {
    type Error = Error;

    fn try_from(v: String) -> Result<Key, Error> {
        validate(v.as_bytes())?;
        Ok(Key(v))
    }
}

impl Key {
    /// Creates a `Key` from a `String`.
    ///
    /// Returns the original value if the conversion failed.
    ///
    /// # Errors
    /// If the input string validation fails.
    pub fn from_string(v: String) -> Result<Self, (Error, String)> {
        match validate(v.as_bytes()) {
            Ok(()) => Ok(Self(v)),
            Err(err) => Err((err.into(), v)),
        }
    }
}

/// Creates a `&KeyRef`, panicking if the value is invalid.
///
/// This is a convenience free function for [`KeyRef::constant`].
///
/// This method is intended to be called from `const` contexts in which the
/// value is known to be valid. Use [`KeyRef::from_str`] for non-panicking
/// conversions.
#[must_use]
pub const fn key_ref(v: &str) -> &KeyRef {
    KeyRef::constant(v)
}

impl fmt::Display for KeyRef {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

impl fmt::Display for Key {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        <KeyRef as fmt::Display>::fmt(self, f)
    }
}

macro_rules! impl_eq {
    ($a: ty, $b: ty) => {
        impl PartialEq<$a> for $b {
            fn eq(&self, other: &$a) -> bool {
                <KeyRef as PartialEq>::eq(self, other)
            }
        }
        impl PartialEq<$b> for $a {
            fn eq(&self, other: &$b) -> bool {
                <KeyRef as PartialEq>::eq(self, other)
            }
        }
    };
}

impl_eq!(Key, KeyRef);
impl_eq!(Key, &KeyRef);

impl<'a> TryFrom<&'a str> for &'a KeyRef {
    type Error = Error;

    fn try_from(v: &'a str) -> Result<&'a KeyRef, Error> {
        KeyRef::from_str(v)
    }
}

impl<'a> TryFrom<&'a [u8]> for &'a KeyRef {
    type Error = Error;

    fn try_from(v: &'a [u8]) -> Result<&'a KeyRef, Error> {
        KeyRef::from_bytes(v)
    }
}

impl Borrow<str> for Key {
    fn borrow(&self) -> &str {
        self.as_str()
    }
}

impl Borrow<str> for KeyRef {
    fn borrow(&self) -> &str {
        self.as_str()
    }
}

impl AsRef<KeyRef> for Key {
    fn as_ref(&self) -> &KeyRef {
        self
    }
}

impl AsRef<KeyRef> for KeyRef {
    fn as_ref(&self) -> &KeyRef {
        self
    }
}
