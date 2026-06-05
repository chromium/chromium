use crate::std::fmt;

/// A general error that can occur when working with UUIDs.
#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct Error(pub(crate) ErrorKind);

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub(crate) enum ErrorKind {
    /// Invalid character in the [`Uuid`] string.
    ///
    /// [`Uuid`]: ../struct.Uuid.html
    ParseChar { character: char, index: usize },
    /// A byte array didn't contain 16 bytes.
    ParseByteLength { len: usize },
    /// A hyphenated [`Uuid`] didn't contain 5 groups
    ///
    /// [`Uuid`]: ../struct.Uuid.html
    ParseGroupCount { count: usize },
    /// A hyphenated [`Uuid`] had a group that wasn't the right length.
    ///
    /// [`Uuid`]: ../struct.Uuid.html
    ParseGroupLength {
        group: usize,
        len: usize,
        index: usize,
    },
    /// The input was not a valid UTF8 string.
    ParseInvalidUTF8,
    /// The input has an invalid length.
    ParseLength { len: usize },
    /// Some other parsing error occurred.
    ParseOther,
    /// The UUID is nil.
    Nil,
    /// A system time was invalid.
    #[cfg(feature = "std")]
    InvalidSystemTime(&'static str),
}

/// A string that is guaranteed to fail to parse to a [`Uuid`].
///
/// This type acts as a lightweight error indicator, suggesting
/// that the string cannot be parsed but offering no error
/// details. To get details, use `InvalidUuid::into_err`.
///
/// [`Uuid`]: ../struct.Uuid.html
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub(crate) struct InvalidUuid<'a>(pub(crate) &'a [u8], pub(crate) RequestedUuid);

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub(crate) enum RequestedUuid {
    Any,
    Simple,
    Hyphenated,
    Braced,
    Urn,
}

impl<'a> InvalidUuid<'a> {
    /// Converts the lightweight error type into detailed diagnostics.
    pub fn into_err(self) -> Error {
        if self.0.len() == 0 || self.0.len() > 45 {
            // Don't waste time looking at strings that may be enormous
            return Error(ErrorKind::ParseLength { len: self.0.len() });
        }

        // Check whether or not the input was ever actually a valid UTF8 string
        let input_str = match std::str::from_utf8(self.0) {
            Ok(s) => s,
            Err(_) => return Error(ErrorKind::ParseInvalidUTF8),
        };

        let (bounds, mut format) = match (self.1, self.0) {
            (RequestedUuid::Any | RequestedUuid::Braced, [b'{', s @ .., b'}']) => {
                (1..s.len() - 1, RequestedUuid::Braced)
            }
            (RequestedUuid::Braced, _) => {
                if self.0[0] != b'{' {
                    // The first character is invalid
                    let (index, character) = input_str.char_indices().next().unwrap();

                    return Error(ErrorKind::ParseChar { character, index });
                } else {
                    // The last character is invalid
                    let (index, character) = input_str.char_indices().last().unwrap();

                    return Error(ErrorKind::ParseChar { character, index });
                }
            }
            (
                RequestedUuid::Any | RequestedUuid::Urn,
                [b'u', b'r', b'n', b':', b'u', b'u', b'i', b'd', b':', s @ ..],
            ) => ("urn:uuid:".len()..s.len(), RequestedUuid::Urn),
            (RequestedUuid::Urn, _) => {
                return Error(ErrorKind::ParseChar {
                    character: input_str.chars().next().unwrap(),
                    index: 0,
                })
            }
            (r, s) => (0..s.len(), r),
        };

        let mut hyphen_count = 0;
        let mut group_bounds = [0; 4];

        // SAFETY: the byte array came from a valid utf8 string,
        // and is aligned along char boundaries.
        let uuid_str = unsafe { std::str::from_utf8_unchecked(self.0) };

        for (index, character) in uuid_str[bounds.clone()].char_indices() {
            let byte = character as u8;

            match (format, byte.to_ascii_lowercase()) {
                (_, b'0'..=b'9' | b'a'..=b'f') => (),
                (RequestedUuid::Simple, b'-') => {
                    return Error(ErrorKind::ParseChar {
                        character: '-',
                        index: index + bounds.start,
                    })
                }
                (_, b'-') => {
                    if format == RequestedUuid::Any {
                        format = RequestedUuid::Hyphenated;
                    }

                    if hyphen_count < 4 {
                        // While we search, also count group breaks
                        group_bounds[hyphen_count] = index;
                    }
                    hyphen_count += 1;
                }
                _ => {
                    return Error(ErrorKind::ParseChar {
                        character,
                        index: index + bounds.start,
                    })
                }
            }
        }

        if format == RequestedUuid::Any || format == RequestedUuid::Simple {
            // This means that we tried and failed to parse a simple uuid.
            // Since we verified that all the characters are valid, this means
            // that it MUST have an invalid length.
            Error(ErrorKind::ParseLength {
                len: input_str.len(),
            })
        } else if hyphen_count != 4 {
            // We tried to parse a hyphenated variant, but there weren't
            // 5 groups (4 hyphen splits).
            Error(ErrorKind::ParseGroupCount {
                count: hyphen_count + 1,
            })
        } else {
            // There are 5 groups, one of them has an incorrect length
            const BLOCK_STARTS: [usize; 5] = [0, 9, 14, 19, 24];
            for i in 0..4 {
                if group_bounds[i] != BLOCK_STARTS[i + 1] - 1 {
                    return Error(ErrorKind::ParseGroupLength {
                        group: i,
                        len: group_bounds[i] - BLOCK_STARTS[i],
                        index: bounds.start + BLOCK_STARTS[i] + 1,
                    });
                }
            }

            // The last group must be too long
            Error(ErrorKind::ParseGroupLength {
                group: 4,
                len: input_str.len() - BLOCK_STARTS[4],
                index: bounds.start + BLOCK_STARTS[4] + 1,
            })
        }
    }
}

// NOTE: This impl is part of the public API. Breaking changes to it should be carefully considered
impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self.0 {
            ErrorKind::ParseChar {
                character, index, ..
            } => {
                write!(f, "invalid character: found `{}` at {}", character, index)
            }
            ErrorKind::ParseByteLength { len } => {
                write!(f, "invalid length: expected 16 bytes, found {}", len)
            }
            ErrorKind::ParseGroupCount { count } => {
                write!(f, "invalid group count: expected 5, found {}", count)
            }
            ErrorKind::ParseGroupLength { group, len, .. } => {
                let expected = [8, 4, 4, 4, 12][group];
                write!(
                    f,
                    "invalid group length in group {}: expected {}, found {}",
                    group, expected, len
                )
            }
            ErrorKind::ParseInvalidUTF8 => write!(f, "non-UTF8 input"),
            ErrorKind::Nil => write!(f, "the UUID is nil"),
            ErrorKind::ParseLength { len } => write!(f, "invalid length: found {}", len),
            ErrorKind::ParseOther => write!(f, "failed to parse a UUID"),
            #[cfg(feature = "std")]
            ErrorKind::InvalidSystemTime(ref e) => {
                write!(f, "the system timestamp is invalid: {e}")
            }
        }
    }
}

impl crate::std::error::Error for Error {}
