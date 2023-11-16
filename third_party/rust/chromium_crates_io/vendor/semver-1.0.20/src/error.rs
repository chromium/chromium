use crate::parse::Error;
use core::fmt::{self, Debug, Display};

pub(crate) enum ErrorKind {
    UnexpectedEnd(Position),
    UnexpectedChar(Position, char),
    UnexpectedCharAfter(Position, char),
    ExpectedCommaFound(Position, char),
    LeadingZero(Position),
    Overflow(Position),
    EmptySegment(Position),
    IllegalCharacter(Position),
    UnexpectedAfterWildcard,
    ExcessiveComparators,
}

#[derive(Copy, Clone, Eq, PartialEq)]
pub(crate) enum Position {
    Major,
    Minor,
    Patch,
    Pre,
    Build,
}

#[cfg(feature = "std")]
#[cfg_attr(doc_cfg, doc(cfg(feature = "std")))]
impl std::error::Error for Error {}

impl Display for Error {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        match &self.kind {
            ErrorKind::UnexpectedEnd(pos) => {
                write!(formatter, "unexpected end of input while parsing {}", pos)
            }
            ErrorKind::UnexpectedChar(pos, ch) => {
                write!(
                    formatter,
                    "unexpected character {:?} while parsing {}",
                    ch, pos,
                )
            }
            ErrorKind::UnexpectedCharAfter(pos, ch) => {
                write!(formatter, "unexpected character {:?} after {}", ch, pos)
            }
            ErrorKind::ExpectedCommaFound(pos, ch) => {
                write!(formatter, "expected comma after {}, found {:?}", pos, ch)
            }
            ErrorKind::LeadingZero(pos) => {
                write!(formatter, "invalid leading zero in {}", pos)
            }
            ErrorKind::Overflow(pos) => {
                write!(formatter, "value of {} exceeds u64::MAX", pos)
            }
            ErrorKind::EmptySegment(pos) => {
                write!(formatter, "empty identifier segment in {}", pos)
            }
            ErrorKind::IllegalCharacter(pos) => {
                write!(formatter, "unexpected character in {}", pos)
            }
            ErrorKind::UnexpectedAfterWildcard => {
                formatter.write_str("unexpected character after wildcard in version req")
            }
            ErrorKind::ExcessiveComparators => {
                formatter.write_str("excessive number of version comparators")
            }
        }
    }
}

impl Display for Position {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str(match self {
            Position::Major => "major version number",
            Position::Minor => "minor version number",
            Position::Patch => "patch version number",
            Position::Pre => "pre-release identifier",
            Position::Build => "build metadata",
        })
    }
}

impl Debug for Error {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("Error(\"")?;
        Display::fmt(self, formatter)?;
        formatter.write_str("\")")?;
        Ok(())
    }
}
