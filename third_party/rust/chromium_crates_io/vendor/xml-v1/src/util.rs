use std::fmt;
use std::io::{self, Read};
use std::str::{self, FromStr};

#[derive(Debug)]
pub(crate) enum CharReadError {
    UnexpectedEof,
    Utf8(str::Utf8Error),
    Io(io::Error),
}

impl From<str::Utf8Error> for CharReadError {
    #[cold]
    fn from(e: str::Utf8Error) -> Self {
        Self::Utf8(e)
    }
}

impl From<io::Error> for CharReadError {
    #[cold]
    fn from(e: io::Error) -> Self {
        Self::Io(e)
    }
}

impl fmt::Display for CharReadError {
    #[cold]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        use self::CharReadError::{Io, UnexpectedEof, Utf8};
        match *self {
            UnexpectedEof => write!(f, "unexpected end of stream"),
            Utf8(ref e) => write!(f, "UTF-8 decoding error: {e}"),
            Io(ref e) => write!(f, "I/O error: {e}"),
        }
    }
}

/// Character encoding used for parsing
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
#[non_exhaustive]
pub enum Encoding {
    /// Explicitly UTF-8 only
    Utf8,
    /// UTF-8 fallback, but can be any 8-bit encoding
    Default,
    /// ISO-8859-1
    Latin1,
    /// US-ASCII
    Ascii,
    /// Big-Endian
    Utf16Be,
    /// Little-Endian
    Utf16Le,
    /// Unknown endianness yet, will be sniffed
    Utf16,
    /// Not determined yet, may be sniffed to be anything
    Unknown,
}

// Rustc inlines eq_ignore_ascii_case and creates kilobytes of code!
#[inline(never)]
fn icmp(lower: &str, varcase: &str) -> bool {
    lower.bytes().zip(varcase.bytes()).all(|(l, v)| l == v.to_ascii_lowercase())
}

impl FromStr for Encoding {
    type Err = &'static str;

    fn from_str(val: &str) -> Result<Self, Self::Err> {
        if ["utf-8", "utf8"].into_iter().any(move |label| icmp(label, val)) {
            Ok(Self::Utf8)
        } else if ["iso-8859-1", "latin1"].into_iter().any(move |label| icmp(label, val)) {
            Ok(Self::Latin1)
        } else if ["utf-16", "utf16"].into_iter().any(move |label| icmp(label, val)) {
            Ok(Self::Utf16)
        } else if ["ascii", "us-ascii"].into_iter().any(move |label| icmp(label, val)) {
            Ok(Self::Ascii)
        } else {
            Err("unknown encoding name")
        }
    }
}

impl fmt::Display for Encoding {
    #[cold]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Self::Utf8 |
            Self::Default => "UTF-8",
            Self::Latin1 => "ISO-8859-1",
            Self::Ascii => "US-ASCII",
            Self::Utf16Be |
            Self::Utf16Le |
            Self::Utf16 => "UTF-16",
            Self::Unknown => "(unknown)",
        })
    }
}

/// Size of the read-ahead buffer of `CharReader`.
const READ_AHEAD_SIZE: usize = 4096;

/// Classes of characters, based on productions of the XML grammar, that can be
/// scanned in bulk with `CharReader::buffered_ascii_run`.
///
/// Each class contains only those ASCII characters of the production that the
/// parser appends verbatim, without any further processing. `/` and `]` are
/// excluded from all classes, because they start lexer lookahead sequences
/// (`/>` and `]]>`).
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub(crate) enum AsciiRunClass {
    /// Characters of an `AttValue` ([10]), except for the quotes, `&` and `<`
    /// (delimiters, references and markup), and whitespace other than the
    /// space, which is normalized.
    AttValue,
    /// `NameChar` ([4a]), except for `:`, the namespace prefix separator.
    NameChar,
    /// `CharData` ([14]), except for whitespace other than the space.
    CharData,
}

impl AsciiRunClass {
    #[inline]
    pub const fn contains(self, b: u8) -> bool {
        match self {
            Self::AttValue => matches!(b, b' '..=b'~') && !matches!(b, b'"' | b'\'' | b'&' | b'<' | b'/' | b']'),
            Self::NameChar => matches!(b, b'a'..=b'z' | b'A'..=b'Z' | b'0'..=b'9' | b'_' | b'-' | b'.'),
            Self::CharData => matches!(b, b' '..=b'~') && !matches!(b, b'<' | b'&' | b'/' | b']'),
        }
    }
}

#[derive(Clone)]
pub(crate) struct CharReader {
    pub encoding: Encoding,
    /// Bytes that have been read from the source, but not consumed yet, are
    /// `read_ahead[read_ahead_pos..read_ahead_len]`. Reading ahead avoids the
    /// overhead of reading every single byte from the source, and allows
    /// scanning runs of bytes at once (see `buffered_ascii_run`). The buffer
    /// is allocated (with `READ_AHEAD_SIZE` bytes) on the first read.
    read_ahead: Vec<u8>,
    read_ahead_pos: usize,
    read_ahead_len: usize,
}

impl CharReader {
    pub fn new(encoding: Encoding) -> Self {
        Self {
            encoding,
            read_ahead: Vec::new(),
            read_ahead_pos: 0,
            read_ahead_len: 0,
        }
    }

    /// Refills the read-ahead buffer once it has been fully consumed.
    /// Returns `false` at the (possibly temporary) end of the source.
    #[inline(never)]
    fn fill_read_ahead<R: Read>(&mut self, source: &mut R) -> io::Result<bool> {
        debug_assert_eq!(self.read_ahead_pos, self.read_ahead_len);
        if self.read_ahead.is_empty() {
            self.read_ahead.resize(READ_AHEAD_SIZE, 0);
        }
        self.read_ahead_pos = 0;
        self.read_ahead_len = 0;
        loop {
            match source.read(&mut self.read_ahead[..]) {
                Ok(n) => {
                    // Don't trust `Read` implementations to stay in bounds.
                    self.read_ahead_len = n.min(READ_AHEAD_SIZE);
                    return Ok(n > 0);
                },
                Err(e) if e.kind() == io::ErrorKind::Interrupted => {},
                Err(e) => return Err(e),
            }
        }
    }

    #[inline]
    fn next_byte_from<R: Read>(&mut self, source: &mut R) -> io::Result<Option<u8>> {
        if self.read_ahead_pos == self.read_ahead_len && !self.fill_read_ahead(source)? {
            return Ok(None);
        }
        let b = self.read_ahead[self.read_ahead_pos];
        self.read_ahead_pos += 1;
        Ok(Some(b))
    }

    /// Returns the length of the longest run of upcoming characters that all
    /// belong to `class`, considering only bytes that are already buffered
    /// (refilling the buffer first if it is empty). The run can be consumed
    /// with `consume_ascii_run` after inspecting it with `ascii_run`. Returns
    /// `None` if the current encoding does not map ASCII bytes to ASCII
    /// characters one-to-one, or at the end of the source.
    #[inline]
    pub fn buffered_ascii_run<R: Read>(&mut self, source: &mut R, class: AsciiRunClass) -> Result<Option<usize>, CharReadError> {
        if !matches!(self.encoding, Encoding::Utf8 | Encoding::Default | Encoding::Latin1 | Encoding::Ascii) {
            return Ok(None);
        }
        if self.read_ahead_pos == self.read_ahead_len && !self.fill_read_ahead(source)? {
            return Ok(None);
        }
        let available = &self.read_ahead[self.read_ahead_pos..self.read_ahead_len];
        Ok(Some(available.iter().position(|&b| !class.contains(b)).unwrap_or(available.len())))
    }

    /// Returns the first `len` buffered bytes, which must have been checked to
    /// be ASCII by `buffered_ascii_run`.
    #[inline]
    pub fn ascii_run(&self, len: usize) -> &str {
        let run = &self.read_ahead[self.read_ahead_pos..self.read_ahead_pos + len];
        debug_assert!(run.is_ascii());
        // ASCII is always valid UTF-8.
        str::from_utf8(run).unwrap_or_default()
    }

    /// Consumes the first `len` buffered bytes, which must have been checked to
    /// belong to `class` by `buffered_ascii_run`.
    #[inline]
    pub fn consume_ascii_run(&mut self, len: usize, class: AsciiRunClass) {
        debug_assert!(self.read_ahead_pos + len <= self.read_ahead_len);
        debug_assert!(self.read_ahead[self.read_ahead_pos..self.read_ahead_pos + len].iter().all(|&b| class.contains(b)));
        self.read_ahead_pos += len;
    }

    #[inline]
    pub fn next_char_from<R: Read>(&mut self, source: &mut R) -> Result<Option<char>, CharReadError> {
        const MAX_CODEPOINT_LEN: usize = 4;

        let mut buf = [0u8; MAX_CODEPOINT_LEN];
        let mut pos = 0;
        while pos < MAX_CODEPOINT_LEN {
            let next = match self.next_byte_from(source) {
                Ok(Some(b)) => b,
                Err(e) => return Err(e.into()),
                Ok(None) if pos == 0 => return Ok(None),
                Ok(None) => return Err(CharReadError::UnexpectedEof),
            };

            match self.encoding {
                Encoding::Utf8 | Encoding::Default => {
                    // fast path for ASCII subset
                    if pos == 0 && next.is_ascii() {
                        return Ok(Some(next.into()));
                    }

                    buf[pos] = next;
                    pos += 1;

                    match str::from_utf8(&buf[..pos]) {
                        Ok(s) => return Ok(s.chars().next()), // always Some(..)
                        Err(_) if pos < MAX_CODEPOINT_LEN => continue,
                        Err(e) => return Err(e.into()),
                    }
                },
                Encoding::Latin1 => {
                    return Ok(Some(next.into()));
                },
                Encoding::Ascii => {
                    return if next.is_ascii() {
                        Ok(Some(next.into()))
                    } else {
                        Err(CharReadError::Io(io::Error::new(io::ErrorKind::InvalidData, "char is not ASCII")))
                    };
                },
                Encoding::Unknown | Encoding::Utf16 => {
                    buf[pos] = next;
                    pos += 1;
                    if let Some(value) = self.sniff_bom(&buf[..pos], &mut pos) {
                        return value;
                    }
                },
                Encoding::Utf16Be => {
                    buf[pos] = next;
                    pos += 1;
                    if pos == 2 {
                        if let Some(Ok(c)) = char::decode_utf16([u16::from_be_bytes(buf[..2].try_into().unwrap())]).next() {
                            return Ok(Some(c));
                        }
                    } else if pos == 4 {
                        return Self::surrogate([u16::from_be_bytes(buf[..2].try_into().unwrap()), u16::from_be_bytes(buf[2..4].try_into().unwrap())]);
                    }
                },
                Encoding::Utf16Le => {
                    buf[pos] = next;
                    pos += 1;
                    if pos == 2 {
                        if let Some(Ok(c)) = char::decode_utf16([u16::from_le_bytes(buf[..2].try_into().unwrap())]).next() {
                            return Ok(Some(c));
                        }
                    } else if pos == 4 {
                        return Self::surrogate([u16::from_le_bytes(buf[..2].try_into().unwrap()), u16::from_le_bytes(buf[2..4].try_into().unwrap())]);
                    }
                },
            }
        }
        Err(CharReadError::Io(io::ErrorKind::InvalidData.into()))
    }

    #[cold]
    fn sniff_bom(&mut self, buf: &[u8], pos: &mut usize) -> Option<Result<Option<char>, CharReadError>> {
        // sniff BOM
        if buf.len() <= 3 && [0xEF, 0xBB, 0xBF].starts_with(buf) {
            if buf.len() == 3 && self.encoding != Encoding::Utf16 {
                *pos = 0;
                self.encoding = Encoding::Utf8;
            }
        } else if buf.len() <= 2 && [0xFE, 0xFF].starts_with(buf) {
            if buf.len() == 2 {
                *pos = 0;
                self.encoding = Encoding::Utf16Be;
            }
        } else if buf.len() <= 2 && [0xFF, 0xFE].starts_with(buf) {
            if buf.len() == 2 {
                *pos = 0;
                self.encoding = Encoding::Utf16Le;
            }
        } else if buf.len() == 1 && self.encoding == Encoding::Utf16 {
            // sniff ASCII char in UTF-16
            self.encoding = if buf[0] == 0 { Encoding::Utf16Be } else { Encoding::Utf16Le };
        } else {
            // UTF-8 is the default, but XML decl can change it to other 8-bit encoding
            self.encoding = Encoding::Default;
            if buf.len() == 1 && buf[0].is_ascii() {
                return Some(Ok(Some(buf[0].into())));
            }
        }
        None
    }

    fn surrogate(buf: [u16; 2]) -> Result<Option<char>, CharReadError> {
        char::decode_utf16(buf).next().transpose()
            .map_err(|e| CharReadError::Io(io::Error::new(io::ErrorKind::InvalidData, e)))
    }
}

#[cfg(test)]
mod tests {
    use super::{CharReadError, CharReader, Encoding};

    #[test]
    fn test_next_char_from() {
        use std::io;

        let mut bytes: &[u8] = b"correct";    // correct ASCII
        assert_eq!(CharReader::new(Encoding::Unknown).next_char_from(&mut bytes).unwrap(), Some('c'));

        let mut bytes: &[u8] = b"\xEF\xBB\xBF\xE2\x80\xA2!";  // BOM
        assert_eq!(CharReader::new(Encoding::Unknown).next_char_from(&mut bytes).unwrap(), Some('•'));

        let mut bytes: &[u8] = b"\xEF\xBB\xBFx123";  // BOM
        assert_eq!(CharReader::new(Encoding::Unknown).next_char_from(&mut bytes).unwrap(), Some('x'));

        let mut bytes: &[u8] = b"\xEF\xBB\xBF";  // Nothing after BOM
        assert_eq!(CharReader::new(Encoding::Unknown).next_char_from(&mut bytes).unwrap(), None);

        let mut bytes: &[u8] = b"\xEF\xBB";  // Nothing after BO
        assert!(matches!(CharReader::new(Encoding::Unknown).next_char_from(&mut bytes), Err(CharReadError::UnexpectedEof)));

        let mut bytes: &[u8] = b"\xEF\xBB\x42";  // Nothing after BO
        assert!(CharReader::new(Encoding::Unknown).next_char_from(&mut bytes).is_err());

        let mut bytes: &[u8] = b"\xFE\xFF\x00\x42";  // UTF-16
        assert_eq!(CharReader::new(Encoding::Unknown).next_char_from(&mut bytes).unwrap(), Some('B'));

        let mut bytes: &[u8] = b"\xFF\xFE\x42\x00";  // UTF-16
        assert_eq!(CharReader::new(Encoding::Unknown).next_char_from(&mut bytes).unwrap(), Some('B'));

        let mut bytes: &[u8] = b"\xFF\xFE";  // UTF-16
        assert_eq!(CharReader::new(Encoding::Unknown).next_char_from(&mut bytes).unwrap(), None);

        let mut bytes: &[u8] = b"\xFF\xFE\x00";  // UTF-16
        assert!(matches!(CharReader::new(Encoding::Unknown).next_char_from(&mut bytes), Err(CharReadError::UnexpectedEof)));

        let mut bytes: &[u8] = "правильно".as_bytes();  // correct BMP
        assert_eq!(CharReader::new(Encoding::Unknown).next_char_from(&mut bytes).unwrap(), Some('п'));

        let mut bytes: &[u8] = "правильно".as_bytes();
        assert_eq!(CharReader::new(Encoding::Utf16Be).next_char_from(&mut bytes).unwrap(), Some('킿'));

        let mut bytes: &[u8] = "правильно".as_bytes();
        assert_eq!(CharReader::new(Encoding::Utf16Le).next_char_from(&mut bytes).unwrap(), Some('뿐'));

        let mut bytes: &[u8] = b"\xD8\xD8\x80";
        assert!(CharReader::new(Encoding::Utf16).next_char_from(&mut bytes).is_err());

        let mut bytes: &[u8] = b"\x00\x42";
        assert_eq!(CharReader::new(Encoding::Utf16).next_char_from(&mut bytes).unwrap(), Some('B'));

        let mut bytes: &[u8] = b"\x42\x00";
        assert_eq!(CharReader::new(Encoding::Utf16).next_char_from(&mut bytes).unwrap(), Some('B'));

        let mut bytes: &[u8] = &[0xEF, 0xBB, 0xBF, 0xFF, 0xFF];
        assert!(CharReader::new(Encoding::Utf16).next_char_from(&mut bytes).is_err());

        let mut bytes: &[u8] = b"\x00";
        assert!(CharReader::new(Encoding::Utf16Be).next_char_from(&mut bytes).is_err());

        let mut bytes: &[u8] = "😊".as_bytes();          // correct non-BMP
        assert_eq!(CharReader::new(Encoding::Unknown).next_char_from(&mut bytes).unwrap(), Some('😊'));

        let mut bytes: &[u8] = b"";                     // empty
        assert_eq!(CharReader::new(Encoding::Unknown).next_char_from(&mut bytes).unwrap(), None);

        let mut bytes: &[u8] = b"\xf0\x9f\x98";         // incomplete code point
        match CharReader::new(Encoding::Unknown).next_char_from(&mut bytes).unwrap_err() {
            CharReadError::UnexpectedEof => {},
            e => panic!("Unexpected result: {e:?}")
        }

        let mut bytes: &[u8] = b"\xff\x9f\x98\x32";     // invalid code point
        match CharReader::new(Encoding::Unknown).next_char_from(&mut bytes).unwrap_err() {
            CharReadError::Utf8(_) => {},
            e => panic!("Unexpected result: {e:?}"),
        }

        // error during read
        struct ErrorReader;
        impl io::Read for ErrorReader {
            fn read(&mut self, _: &mut [u8]) -> io::Result<usize> {
                Err(io::Error::new(io::ErrorKind::Other, "test error"))
            }
        }

        let mut r = ErrorReader;
        match CharReader::new(Encoding::Unknown).next_char_from(&mut r).unwrap_err() {
            CharReadError::Io(ref e) if e.kind() == io::ErrorKind::Other &&
                                               e.to_string().contains("test error") => {},
            e => panic!("Unexpected result: {e:?}")
        }
    }

    #[test]
    fn test_latin1_transcoding() {
        let mut bytes: &[u8] = &[0xE9, 0x20, 0xFC]; // é, space, ü
        let mut ch = CharReader::new(Encoding::Latin1);
        assert_eq!(ch.next_char_from(&mut bytes).unwrap(), Some('é'));
        assert_eq!(ch.next_char_from(&mut bytes).unwrap(), Some(' '));
        assert_eq!(ch.next_char_from(&mut bytes).unwrap(), Some('ü'));
        assert_eq!(ch.next_char_from(&mut bytes).unwrap(), None);
    }

    #[test]
    fn test_ascii_encoding() {
        let mut bytes: &[u8] = b"ok";
        let mut ch = CharReader::new(Encoding::Ascii);
        assert_eq!(ch.next_char_from(&mut bytes).unwrap(), Some('o'));
        assert_eq!(ch.next_char_from(&mut bytes).unwrap(), Some('k'));
        assert_eq!(ch.next_char_from(&mut bytes).unwrap(), None);

        let mut bytes: &[u8] = &[0x80];
        let mut ch = CharReader::new(Encoding::Ascii);
        assert!(ch.next_char_from(&mut bytes).is_err());
    }

    #[test]
    fn test_encoding_switch() {
        let data: &[u8] = &[b'<', b'?', 0xE9]; // <? then Latin1 é
        let mut bytes = data;
        let mut ch = CharReader::new(Encoding::Default);

        assert_eq!(ch.next_char_from(&mut bytes).unwrap(), Some('<'));
        assert_eq!(ch.next_char_from(&mut bytes).unwrap(), Some('?'));

        ch.encoding = Encoding::Latin1;

        assert_eq!(ch.next_char_from(&mut bytes).unwrap(), Some('é'));
    }
}
