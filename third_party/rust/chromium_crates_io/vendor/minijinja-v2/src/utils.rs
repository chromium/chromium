use std::char::decode_utf16;
use std::cmp::Ordering;
use std::collections::BTreeMap;
use std::fmt;
use std::iter::{once, repeat};
use std::str::Chars;

use crate::error::{Error, ErrorKind};
use crate::value::{StringType, UndefinedType, Value, ValueIter, ValueKind, ValueRepr};
use crate::Output;

/// internal marker to seal up some trait methods
pub struct SealedMarker;

pub fn memchr(haystack: &[u8], needle: u8) -> Option<usize> {
    haystack.iter().position(|&x| x == needle)
}

pub fn memstr(haystack: &[u8], needle: &[u8]) -> Option<usize> {
    haystack
        .windows(needle.len())
        .position(|window| window == needle)
}

/// Helper for dealing with untrusted size hints.
#[inline(always)]
pub(crate) fn untrusted_size_hint(value: usize) -> usize {
    value.min(1024)
}

fn write_with_html_escaping(out: &mut Output, value: &Value) -> fmt::Result {
    if let Some(s) = value.as_str() {
        write!(out, "{}", HtmlEscape(s))
    } else if matches!(
        value.kind(),
        ValueKind::Undefined | ValueKind::None | ValueKind::Bool | ValueKind::Number
    ) {
        write!(out, "{value}")
    } else {
        write!(out, "{}", HtmlEscape(&value.to_string()))
    }
}

#[cold]
fn invalid_autoescape(name: &str) -> Result<(), Error> {
    Err(Error::new(
        ErrorKind::InvalidOperation,
        format!("Default formatter does not know how to format to custom format '{name}'"),
    ))
}

#[cfg(feature = "json")]
fn json_escape_write(out: &mut Output, value: &Value) -> Result<(), Error> {
    let value = ok!(serde_json::to_string(&value).map_err(|err| {
        Error::new(ErrorKind::BadSerialization, "unable to format to JSON").with_source(err)
    }));
    write!(out, "{value}").map_err(Error::from)
}

#[inline(always)]
pub fn write_escaped(
    out: &mut Output,
    auto_escape: AutoEscape,
    value: &Value,
) -> Result<(), Error> {
    // string strings bypass all of this
    if let ValueRepr::String(ref s, StringType::Safe) = value.0 {
        return out.write_str(s).map_err(Error::from);
    }

    match auto_escape {
        AutoEscape::None => write!(out, "{value}").map_err(Error::from),
        AutoEscape::Html => write_with_html_escaping(out, value).map_err(Error::from),
        #[cfg(feature = "json")]
        AutoEscape::Json => json_escape_write(out, value),
        AutoEscape::Custom(name) => invalid_autoescape(name),
    }
}

/// Controls the autoescaping behavior.
///
/// For more information see
/// [`set_auto_escape_callback`](crate::Environment::set_auto_escape_callback).
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum AutoEscape {
    /// Do not apply auto escaping.
    None,
    /// Use HTML auto escaping rules.
    ///
    /// Any value will be converted into a string and the following characters
    /// will be escaped in ways compatible to XML and HTML: `<`, `>`, `&`, `"`,
    /// `'`, and `/`.
    Html,
    /// Use escaping rules suitable for JSON/JavaScript or YAML.
    ///
    /// Any value effectively ends up being serialized to JSON upon printing.  The
    /// serialized values will be compatible with JavaScript and YAML as well.
    #[cfg(feature = "json")]
    #[cfg_attr(docsrs, doc(cfg(feature = "json")))]
    Json,
    /// A custom auto escape format.
    ///
    /// The default formatter does not know how to deal with a custom escaping
    /// format and would error.  The use of these requires a custom formatter.
    /// See [`set_formatter`](crate::Environment::set_formatter).
    Custom(&'static str),
}

/// Defines the behavior of undefined values in the engine.
///
/// At present there are three types of behaviors available which mirror the
/// behaviors that Jinja2 provides out of the box and an extra option called
/// `SemiStrict` which is a slightly less strict undefined.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Default)]
#[non_exhaustive]
pub enum UndefinedBehavior {
    /// The default, somewhat lenient undefined behavior.
    ///
    /// * **printing:** allowed (returns empty string)
    /// * **iteration:** allowed (returns empty array)
    /// * **attribute access of undefined values:** fails
    /// * **if true:** allowed (is considered false)
    #[default]
    Lenient,
    /// Like `Lenient`, but also allows chaining of undefined lookups.
    ///
    /// * **printing:** allowed (returns empty string)
    /// * **iteration:** allowed (returns empty array)
    /// * **attribute access of undefined values:** allowed (returns [`undefined`](Value::UNDEFINED))
    /// * **if true:** allowed (is considered false)
    Chainable,
    /// Like strict, but does not error when the undefined is checked for truthyness.
    ///
    /// * **printing:** fails
    /// * **iteration:** fails
    /// * **attribute access of undefined values:** fails
    /// * **if true:** allowed (is considered false)
    SemiStrict,
    /// Complains very quickly about undefined values.
    ///
    /// * **printing:** fails
    /// * **iteration:** fails
    /// * **attribute access of undefined values:** fails
    /// * **if true:** fails
    Strict,
}

impl UndefinedBehavior {
    /// Utility method used in the engine to determine what to do when an undefined is
    /// encountered.
    ///
    /// The flag indicates if this is the first or second level of undefined value.  The
    /// parent value is passed too.
    pub(crate) fn handle_undefined(self, parent_was_undefined: bool) -> Result<Value, Error> {
        match (self, parent_was_undefined) {
            (UndefinedBehavior::Lenient, false)
            | (UndefinedBehavior::Strict, false)
            | (UndefinedBehavior::SemiStrict, false)
            | (UndefinedBehavior::Chainable, _) => Ok(Value::UNDEFINED),
            (UndefinedBehavior::Lenient, true)
            | (UndefinedBehavior::Strict, true)
            | (UndefinedBehavior::SemiStrict, true) => Err(Error::from(ErrorKind::UndefinedError)),
        }
    }

    /// Utility method to check if something is true.
    ///
    /// This fails only for strict undefined values.
    #[inline]
    pub(crate) fn is_true(self, value: &Value) -> Result<bool, Error> {
        match (self, &value.0) {
            // silent undefined doesn't error, even in strict mode
            (UndefinedBehavior::Strict, &ValueRepr::Undefined(UndefinedType::Default)) => {
                Err(Error::from(ErrorKind::UndefinedError))
            }
            _ => Ok(value.is_true()),
        }
    }

    /// Tries to iterate over a value while handling the undefined value.
    ///
    /// If the value is undefined, then iteration fails if the behavior is set to strict,
    /// otherwise it succeeds with an empty iteration.  This is also internally used in the
    /// engine to convert values to lists.
    #[inline]
    pub(crate) fn try_iter(self, value: Value) -> Result<ValueIter, Error> {
        self.assert_iterable(&value).and_then(|_| value.try_iter())
    }

    /// Are we strict on iteration?
    #[inline]
    pub(crate) fn assert_iterable(self, value: &Value) -> Result<(), Error> {
        match (self, &value.0) {
            // silent undefined doesn't error, even in strict mode
            (
                UndefinedBehavior::Strict | UndefinedBehavior::SemiStrict,
                &ValueRepr::Undefined(UndefinedType::Default),
            ) => Err(Error::from(ErrorKind::UndefinedError)),
            _ => Ok(()),
        }
    }
}

/// Helper to HTML escape a string.
pub struct HtmlEscape<'a>(pub &'a str);

impl fmt::Display for HtmlEscape<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        #[cfg(feature = "v_htmlescape")]
        {
            fmt::Display::fmt(&v_htmlescape::escape(self.0), f)
        }
        // this is taken from askama-escape
        #[cfg(not(feature = "v_htmlescape"))]
        {
            let bytes = self.0.as_bytes();
            let mut start = 0;

            for (i, b) in bytes.iter().enumerate() {
                macro_rules! escaping_body {
                    ($quote:expr) => {{
                        if start < i {
                            // SAFETY: this is safe because we only push valid utf-8 bytes over
                            ok!(f.write_str(unsafe {
                                std::str::from_utf8_unchecked(&bytes[start..i])
                            }));
                        }
                        ok!(f.write_str($quote));
                        start = i + 1;
                    }};
                }
                if b.wrapping_sub(b'"') <= b'>' - b'"' {
                    match *b {
                        b'<' => escaping_body!("&lt;"),
                        b'>' => escaping_body!("&gt;"),
                        b'&' => escaping_body!("&amp;"),
                        b'"' => escaping_body!("&quot;"),
                        b'\'' => escaping_body!("&#x27;"),
                        b'/' => escaping_body!("&#x2f;"),
                        _ => (),
                    }
                }
            }

            if start < bytes.len() {
                // SAFETY: this is safe because we only push valid utf-8 bytes over
                f.write_str(unsafe { std::str::from_utf8_unchecked(&bytes[start..]) })
            } else {
                Ok(())
            }
        }
    }
}

struct Unescaper {
    out: String,
    pending_surrogate: u16,
}

impl Unescaper {
    fn unescape(mut self, s: &str) -> Result<String, Error> {
        let mut char_iter = s.chars();

        while let Some(c) = char_iter.next() {
            if c == '\\' {
                match char_iter.next() {
                    None => return Err(ErrorKind::BadEscape.into()),
                    Some(d) => match d {
                        '"' | '\\' | '/' | '\'' => ok!(self.push_char(d)),
                        'b' => ok!(self.push_char('\x08')),
                        'f' => ok!(self.push_char('\x0C')),
                        'n' => ok!(self.push_char('\n')),
                        'r' => ok!(self.push_char('\r')),
                        't' => ok!(self.push_char('\t')),
                        'u' => {
                            let val = ok!(self.parse_u16(&mut char_iter));
                            ok!(self.push_u16(val));
                        }
                        'x' => {
                            let val = ok!(self.parse_hex_byte(&mut char_iter));
                            ok!(self.push_char(val as char));
                        }
                        '0'..='7' => {
                            let val = ok!(self.parse_octal_byte(d, &mut char_iter));
                            ok!(self.push_char(val as char));
                        }
                        _ => return Err(ErrorKind::BadEscape.into()),
                    },
                }
            } else {
                ok!(self.push_char(c));
            }
        }

        if self.pending_surrogate != 0 {
            Err(ErrorKind::BadEscape.into())
        } else {
            Ok(self.out)
        }
    }

    fn parse_u16(&self, chars: &mut Chars) -> Result<u16, Error> {
        let hexnum = chars.chain(repeat('\0')).take(4).collect::<String>();
        u16::from_str_radix(&hexnum, 16).map_err(|_| ErrorKind::BadEscape.into())
    }

    fn parse_hex_byte(&self, chars: &mut Chars) -> Result<u8, Error> {
        let hexnum = chars.take(2).collect::<String>();
        if hexnum.len() != 2 {
            return Err(ErrorKind::BadEscape.into());
        }
        u8::from_str_radix(&hexnum, 16).map_err(|_| ErrorKind::BadEscape.into())
    }

    fn parse_octal_byte(&self, first_digit: char, chars: &mut Chars) -> Result<u8, Error> {
        let mut octal_str = String::new();
        octal_str.push(first_digit);

        // Collect up to 2 more octal digits (0-7)
        for _ in 0..2 {
            let next_char = chars.as_str().chars().next();
            if let Some(c) = next_char {
                if ('0'..='7').contains(&c) {
                    octal_str.push(c);
                    chars.next(); // consume the character
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        u8::from_str_radix(&octal_str, 8).map_err(|_| ErrorKind::BadEscape.into())
    }

    fn push_u16(&mut self, c: u16) -> Result<(), Error> {
        match (self.pending_surrogate, (0xD800..=0xDFFF).contains(&c)) {
            (0, false) => match decode_utf16(once(c)).next() {
                Some(Ok(c)) => self.out.push(c),
                _ => return Err(ErrorKind::BadEscape.into()),
            },
            (_, false) => return Err(ErrorKind::BadEscape.into()),
            (0, true) => self.pending_surrogate = c,
            (prev, true) => match decode_utf16(once(prev).chain(once(c))).next() {
                Some(Ok(c)) => {
                    self.out.push(c);
                    self.pending_surrogate = 0;
                }
                _ => return Err(ErrorKind::BadEscape.into()),
            },
        }
        Ok(())
    }

    fn push_char(&mut self, c: char) -> Result<(), Error> {
        if self.pending_surrogate != 0 {
            Err(ErrorKind::BadEscape.into())
        } else {
            self.out.push(c);
            Ok(())
        }
    }
}

/// Un-escape a string, following JSON rules.
pub fn unescape(s: &str) -> Result<String, Error> {
    Unescaper {
        out: String::new(),
        pending_surrogate: 0,
    }
    .unescape(s)
}

pub struct BTreeMapKeysDebug<'a, K: fmt::Debug, V>(pub &'a BTreeMap<K, V>);

impl<K: fmt::Debug, V> fmt::Debug for BTreeMapKeysDebug<'_, K, V> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_list().entries(self.0.iter().map(|x| x.0)).finish()
    }
}

pub struct OnDrop<F: FnOnce()>(Option<F>);

impl<F: FnOnce()> OnDrop<F> {
    pub fn new(f: F) -> Self {
        Self(Some(f))
    }
}

impl<F: FnOnce()> Drop for OnDrop<F> {
    fn drop(&mut self) {
        self.0.take().unwrap()();
    }
}

#[cfg(feature = "builtins")]
pub fn splitn_whitespace(s: &str, maxsplits: usize) -> impl Iterator<Item = &str> + '_ {
    let mut splits = 1;
    let mut skip_ws = true;
    let mut split_start = None;
    let mut last_split_end = 0;
    let mut chars = s.char_indices();

    std::iter::from_fn(move || {
        for (idx, c) in chars.by_ref() {
            if splits >= maxsplits && !skip_ws {
                continue;
            } else if c.is_whitespace() {
                if let Some(old) = split_start {
                    let rv = &s[old..idx];
                    split_start = None;
                    last_split_end = idx;
                    splits += 1;
                    skip_ws = true;
                    return Some(rv);
                }
            } else {
                skip_ws = false;
                if split_start.is_none() {
                    split_start = Some(idx);
                    last_split_end = idx;
                }
            }
        }

        let rest = &s[last_split_end..];
        if !rest.is_empty() {
            last_split_end = s.len();
            Some(rest)
        } else {
            None
        }
    })
}

/// Because the Python crate violates our ordering guarantees by design
/// we want to catch failed sorts in a landing pad.  This is not ideal but
/// it at least gives us error context for when invalid search operations
/// are taking place.
#[cfg_attr(not(feature = "internal_safe_search"), inline)]
pub fn safe_sort<T, F>(seq: &mut [T], f: F) -> Result<(), Error>
where
    F: FnMut(&T, &T) -> Ordering,
{
    #[cfg(feature = "internal_safe_search")]
    {
        if let Err(panic) = std::panic::catch_unwind(std::panic::AssertUnwindSafe(move || {
            seq.sort_by(f);
        })) {
            let msg = panic
                .downcast_ref::<&str>()
                .copied()
                .or_else(|| panic.downcast_ref::<String>().map(|x| x.as_str()));
            return Err(Error::new(
                ErrorKind::InvalidOperation,
                format!(
                    "failed to sort: {}",
                    msg.unwrap_or("comparator does not implement total order")
                ),
            ));
        }
    }
    #[cfg(not(feature = "internal_safe_search"))]
    {
        seq.sort_by(f);
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    use similar_asserts::assert_eq;

    #[test]
    fn test_html_escape() {
        let input = "<>&\"'/";
        let output = HtmlEscape(input).to_string();
        assert_eq!(output, "&lt;&gt;&amp;&quot;&#x27;&#x2f;");
    }

    #[test]
    fn test_unescape() {
        assert_eq!(unescape(r"foo\u2603bar").unwrap(), "foo\u{2603}bar");
        assert_eq!(unescape(r"\t\b\f\r\n\\\/").unwrap(), "\t\x08\x0c\r\n\\/");
        assert_eq!(unescape("foobarbaz").unwrap(), "foobarbaz");
        assert_eq!(unescape(r"\ud83d\udca9").unwrap(), "💩");

        // Test new escape sequences
        assert_eq!(unescape(r"\0").unwrap(), "\0");
        assert_eq!(unescape(r"foo\0bar").unwrap(), "foo\0bar");
        assert_eq!(unescape(r"\x00").unwrap(), "\0");
        assert_eq!(unescape(r"\x42").unwrap(), "B");
        assert_eq!(unescape(r"\xab").unwrap(), "\u{ab}");
        assert_eq!(unescape(r"foo\x42bar").unwrap(), "fooBbar");
        assert_eq!(unescape(r"\x0a").unwrap(), "\n");
        assert_eq!(unescape(r"\x0d").unwrap(), "\r");

        // Test truncation
        assert!(unescape(r"\x").is_err()); // truncated \x
        assert!(unescape(r"\x1").is_err()); // truncated \x1
        assert!(unescape(r"\x1g").is_err()); // invalid hex digit
        assert!(unescape(r"\x1G").is_err()); // invalid hex digit

        // Test octal escape sequences
        assert_eq!(unescape(r"\0").unwrap(), "\0"); // octal 0 = null
        assert_eq!(unescape(r"\1").unwrap(), "\x01"); // octal 1 = SOH
        assert_eq!(unescape(r"\12").unwrap(), "\n"); // octal 12 = 10 decimal = LF
        assert_eq!(unescape(r"\123").unwrap(), "S"); // octal 123 = 83 decimal = 'S'
        assert_eq!(unescape(r"\141").unwrap(), "a"); // octal 141 = 97 decimal = 'a'
        assert_eq!(unescape(r"\177").unwrap(), "\x7f"); // octal 177 = 127 decimal = DEL
        assert_eq!(unescape(r"foo\123bar").unwrap(), "fooSbar"); // 'S' in the middle
        assert_eq!(unescape(r"\101\102\103").unwrap(), "ABC"); // octal for A, B, C
    }

    #[test]
    #[cfg(feature = "builtins")]
    fn test_splitn_whitespace() {
        fn s(s: &str, n: usize) -> Vec<&str> {
            splitn_whitespace(s, n).collect::<Vec<_>>()
        }

        assert_eq!(s("a b c", 1), vec!["a b c"]);
        assert_eq!(s("a b c", 2), vec!["a", "b c"]);
        assert_eq!(s("a    b c", 2), vec!["a", "b c"]);
        assert_eq!(s("a    b c   ", 2), vec!["a", "b c   "]);
        assert_eq!(s("a   b   c", 3), vec!["a", "b", "c"]);
        assert_eq!(s("a   b   c", 4), vec!["a", "b", "c"]);
        assert_eq!(s("   a   b   c", 3), vec!["a", "b", "c"]);
        assert_eq!(s("   a   b   c", 4), vec!["a", "b", "c"]);
    }
}
