use std::{
    borrow::Borrow,
    fmt::{Debug, Display, Formatter},
    str::FromStr,
};

/// An OpenType tag.
///
/// A tag is a 4-byte array where each byte is in the printable ascii range
/// (0x20..=0x7E).
#[derive(Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct Tag([u8; 4]);

impl Tag {
    /// Generate a `Tag` from a byte slice, verifying it conforms to the
    /// OpenType spec.
    ///
    /// The argument must be a non-empty slice, containing at most four
    /// bytes in the printable ascii range, `0x20..=0x7E`.
    ///
    /// If the input has fewer than four bytes, it will be padded with spaces
    /// (`0x20`).
    ///
    /// # Panics
    ///
    /// This method panics if the tag is not valid per the requirements above.
    pub const fn new(src: &[u8]) -> Tag {
        match Tag::new_checked(src) {
            Ok(tag) => tag,
            Err(InvalidTag::InvalidLength(_)) => panic!("invalid length for tag"),
            Err(InvalidTag::InvalidByte { .. }) => panic!("tag contains invalid byte"),
        }
    }

    /// Attempt to create a `Tag` from raw bytes.
    ///
    /// The slice must contain between 1 and 4 bytes, each in the printable
    /// ascii range (`0x20..=0x7E`).
    ///
    /// If the input has fewer than four bytes, it will be padded with spaces.
    pub const fn new_checked(src: &[u8]) -> Result<Self, InvalidTag> {
        if src.is_empty() || src.len() > 4 {
            return Err(InvalidTag::InvalidLength(src.len()));
        }
        let mut raw = [0x20; 4];
        let mut i = 0;
        let mut seen_space = false;
        while i < src.len() {
            let byte = match src[i] {
                byte @ 0x20 if i == 0 => return Err(InvalidTag::InvalidByte { pos: i, byte }),
                byte @ 0..=0x1F | byte @ 0x7f.. => {
                    return Err(InvalidTag::InvalidByte { pos: i, byte })
                }
                byte @ 0x21..=0x7e if seen_space => {
                    return Err(InvalidTag::InvalidByte { pos: i, byte })
                }
                byte => byte,
            };

            seen_space |= byte == 0x20;

            raw[i] = byte;
            i += 1;
        }
        Ok(Tag(raw))
    }

    // for symmetry with integer types / other things we encode/decode
    /// Return the memory representation of this tag.
    pub const fn to_be_bytes(self) -> [u8; 4] {
        self.0
    }

    /// Create a tag from raw big-endian bytes.
    ///
    /// Prefer to use [`Tag::new`] (in const contexts) or [`Tag::new_checked`]
    /// when creating a `Tag`.
    ///
    /// This does not check the input, and is only intended to be used during
    /// parsing, where invalid inputs are accepted.
    pub const fn from_be_bytes(bytes: [u8; 4]) -> Self {
        Self(bytes)
    }

    /// Return the raw byte array representing this tag.
    pub fn into_bytes(self) -> [u8; 4] {
        self.0
    }
}

/// An error representing an invalid tag.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum InvalidTag {
    InvalidLength(usize),
    InvalidByte { pos: usize, byte: u8 },
}

impl FromStr for Tag {
    type Err = InvalidTag;

    fn from_str(src: &str) -> Result<Self, Self::Err> {
        Tag::new_checked(src.as_bytes())
    }
}

impl crate::raw::Scalar for Tag {
    type Raw = [u8; 4];

    fn to_raw(self) -> Self::Raw {
        self.to_be_bytes()
    }

    fn from_raw(raw: Self::Raw) -> Self {
        Self::from_be_bytes(raw)
    }
}

impl Borrow<[u8; 4]> for Tag {
    fn borrow(&self) -> &[u8; 4] {
        &self.0
    }
}

impl PartialEq<[u8; 4]> for Tag {
    fn eq(&self, other: &[u8; 4]) -> bool {
        &self.0 == other
    }
}

impl PartialEq<str> for Tag {
    fn eq(&self, other: &str) -> bool {
        self.0 == other.as_bytes()
    }
}

impl PartialEq<&str> for Tag {
    fn eq(&self, other: &&str) -> bool {
        self == *other
    }
}

impl PartialEq<&[u8]> for Tag {
    fn eq(&self, other: &&[u8]) -> bool {
        self.0.as_ref() == *other
    }
}

impl AsRef<[u8]> for Tag {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

impl Display for Tag {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        // a dumb no-std way of ensuring this string is valid utf-8
        let mut bytes = [b'-'; 4];
        for (i, b) in self.0.iter().enumerate() {
            if b.is_ascii() {
                bytes[i] = *b;
            }
        }
        Display::fmt(&std::str::from_utf8(&bytes).unwrap(), f)
    }
}

impl Display for InvalidTag {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        match self {
            InvalidTag::InvalidByte { pos, byte } => {
                write!(f, "Invalid byte 0x{byte:X} at index {pos}")
            }
            InvalidTag::InvalidLength(len) => write!(f, "Invalid length ({len})"),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for InvalidTag {}

impl Debug for Tag {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        let mut dbg = f.debug_tuple("Tag");
        let mut bytes = [b'-'; 4];
        for (i, b) in self.0.iter().enumerate() {
            if b.is_ascii() {
                bytes[i] = *b;
            }
        }
        dbg.field(&std::str::from_utf8(&bytes).unwrap());
        dbg.finish()
    }
}

// a meaningless placeholder value.
impl Default for Tag {
    fn default() -> Self {
        Tag([b' '; 4])
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn smoke_test() {
        Tag::new(b"head");
        assert!(Tag::new_checked(b"").is_err());
        assert!(Tag::new_checked(b" ").is_err());
        assert!(Tag::new_checked(b"a").is_ok());
        assert!(Tag::new_checked(b"ab").is_ok());
        assert!(Tag::new_checked(b"abc").is_ok());
        assert!(Tag::new_checked(b"abcd").is_ok());
        assert!(Tag::new_checked(b"abcde").is_err());
        assert!(Tag::new_checked(b" bc").is_err()); // space invalid in first position
        assert!(Tag::new_checked(b"b c").is_err()); // non-space cannot follow space
        assert_eq!(Tag::new_checked(b"bc  "), Ok(Tag::new(b"bc")));

        // ascii only:
        assert!(Tag::new_checked(&[0x19]).is_err());
        assert!(Tag::new_checked(&[0x21]).is_ok());
        assert!(Tag::new_checked(&[0x7E]).is_ok());
        assert!(Tag::new_checked(&[0x7F]).is_err());
    }

    #[test]
    #[should_panic]
    fn name() {
        let _ = Tag::new(&[0x19, 0x69]);
    }
}
