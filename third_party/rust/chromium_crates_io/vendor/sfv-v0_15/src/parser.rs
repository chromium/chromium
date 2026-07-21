use std::{borrow::Cow, string::String as StdString};

use crate::{
    error, utils,
    visitor::{
        DictionaryVisitor, EntryVisitor, InnerListVisitor, ItemVisitor, ListVisitor,
        MakeDictionaryVisitor, MakeItemVisitor, MakeListVisitor, ParameterVisitor,
    },
    BareItemFromInput, Date, Decimal, Integer, KeyRef, Num, SFVResult, String, StringRef, TokenRef,
    Version,
};

fn parse_item<'de, V>(parser: &mut Parser<'de>, visitor: V) -> Result<V::Out, error::Repr>
where
    V: ItemVisitor<'de>,
{
    // https://httpwg.org/specs/rfc9651.html#parse-item
    let param_visitor = visitor.bare_item(parser.parse_bare_item()?)?;
    parser.parse_parameters(param_visitor)
}

fn parse_comma_separated<'de>(
    parser: &mut Parser<'de>,
    mut parse_member: impl FnMut(&mut Parser<'de>) -> Result<(), error::Repr>,
) -> Result<(), error::Repr> {
    while parser.peek().is_some() {
        parse_member(parser)?;

        parser.consume_ows_chars();

        if parser.peek().is_none() {
            return Ok(());
        }

        let comma_index = parser.index;

        if let Some(c) = parser.peek() {
            if c != b',' {
                return Err(error::Repr::TrailingCharactersAfterMember(parser.index));
            }
            parser.next();
        }

        parser.consume_ows_chars();

        if parser.peek().is_none() {
            // Report the error at the position of the comma itself, rather
            // than at the end of input.
            return Err(error::Repr::TrailingComma(comma_index));
        }
    }

    Ok(())
}

/// Exposes methods for parsing input into a structured field value.
#[derive(Debug)]
#[must_use]
pub struct Parser<'de> {
    input: &'de [u8],
    index: usize,
    version: Version,
    lenient: bool,
}

impl<'de> Parser<'de> {
    /// Creates a parser from the given input with [`Version::Rfc9651`].
    pub fn new(input: &'de (impl ?Sized + AsRef<[u8]>)) -> Self {
        Self {
            input: input.as_ref(),
            index: 0,
            version: Version::Rfc9651,
            lenient: false,
        }
    }

    /// Sets the parser's version and returns it.
    pub fn with_version(mut self, version: Version) -> Self {
        self.version = version;
        self
    }

    /// Sets whether the parser is in lenient mode and returns it.
    ///
    /// Lenient mode tolerates some specification violations found in legacy
    /// implementations (e.g. trailing decimal points and internal whitespace
    /// in byte sequences).
    pub fn with_lenient_mode(mut self, lenient: bool) -> Self {
        self.lenient = lenient;
        self
    }

    /// Parses a structured field value.
    ///
    /// # Errors
    /// When the parsing process is unsuccessful.
    #[cfg(feature = "parsed-types")]
    pub fn parse<T: crate::FieldType>(self) -> SFVResult<T> {
        T::parse(self)
    }

    /// Parses input into a structured field value of `Dictionary` type,
    /// returning the result as type `T`.
    ///
    /// # Errors
    /// When the parsing process is unsuccessful, including any error raised by a visitor.
    pub fn parse_dictionary<T>(self) -> SFVResult<T>
    where
        T: MakeDictionaryVisitor<'de>,
    {
        self.parse_dictionary_with_visitor(T::make_dictionary_visitor())
    }

    /// Parses input into a structured field value of `Dictionary` type, using
    /// the given visitor.
    #[cfg_attr(
        feature = "parsed-types",
        doc = r#"

This can also be used to parse a dictionary that is split into multiple lines by merging
them into an existing structure:

```
# use sfv::{Dictionary, FieldType, Parser};
# fn main() -> Result<(), sfv::Error> {
let mut dict: Dictionary = Parser::new("a=1").parse()?;

Parser::new("b=2").parse_dictionary_with_visitor(&mut dict)?;

assert_eq!(
    dict.serialize().as_deref(),
    Some("a=1, b=2"),
);
# Ok(())
# }
```
"#
    )]
    ///
    /// # Errors
    /// When the parsing process is unsuccessful, including any error raised by a visitor.
    pub fn parse_dictionary_with_visitor<V>(self, mut visitor: V) -> SFVResult<V::Out>
    where
        V: DictionaryVisitor<'de>,
    {
        // https://httpwg.org/specs/rfc9651.html#parse-dictionary
        self.parse_internal(|parser| {
            parse_comma_separated(parser, |parser| {
                // Note: It is up to the visitor to properly handle duplicate keys.
                let entry_visitor = visitor.entry(parser.parse_key()?)?;

                if let Some(b'=') = parser.peek() {
                    parser.next();
                    parser.parse_list_entry(entry_visitor)
                } else {
                    let param_visitor = entry_visitor
                        .item()?
                        .bare_item(BareItemFromInput::from(true))?;
                    parser.parse_parameters(param_visitor)?;
                    Ok(())
                }
            })?;

            Ok(visitor.finish()?)
        })
    }

    /// Parses input into a structured field value of `List` type, returning the
    /// result as type `T`.
    ///
    /// # Errors
    /// When the parsing process is unsuccessful, including any error raised by a visitor.
    pub fn parse_list<T>(self) -> SFVResult<T>
    where
        T: MakeListVisitor<'de>,
    {
        self.parse_list_with_visitor(T::make_list_visitor())
    }

    /// Parses input into a structured field value of `List` type, using the
    /// given visitor.
    #[allow(clippy::needless_raw_string_hashes)] // false positive: https://github.com/rust-lang/rust-clippy/issues/11737
    #[cfg_attr(
        feature = "parsed-types",
        doc = r##"

This can also be used to parse a list that is split into multiple lines by merging them
into an existing structure:
```
# use sfv::{FieldType, List, Parser};
# fn main() -> Result<(), sfv::Error> {
let mut list: List = Parser::new("11, (12 13)").parse()?;

Parser::new(r#""foo",        "bar""#).parse_list_with_visitor(&mut list)?;

assert_eq!(
    list.serialize().as_deref(),
    Some(r#"11, (12 13), "foo", "bar""#),
);
# Ok(())
# }
```
"##
    )]
    ///
    /// # Errors
    /// When the parsing process is unsuccessful, including any error raised by a visitor.
    pub fn parse_list_with_visitor<V>(self, mut visitor: V) -> SFVResult<V::Out>
    where
        V: ListVisitor<'de>,
    {
        // https://httpwg.org/specs/rfc9651.html#parse-list
        self.parse_internal(|parser| {
            parse_comma_separated(parser, |parser| parser.parse_list_entry(visitor.entry()?))?;
            Ok(visitor.finish()?)
        })
    }

    /// Parses input into a structured field value of `Item` type, returning the
    /// result as type `T`.
    ///
    /// # Errors
    /// When the parsing process is unsuccessful, including any error raised by a visitor.
    pub fn parse_item<T>(self) -> SFVResult<T>
    where
        T: MakeItemVisitor<'de>,
    {
        self.parse_item_with_visitor(T::make_item_visitor())
    }

    /// Parses input into a structured field value of `Item` type, using the
    /// given visitor.
    ///
    /// # Errors
    /// When the parsing process is unsuccessful, including any error raised by a visitor.
    pub fn parse_item_with_visitor<V>(self, visitor: V) -> SFVResult<V::Out>
    where
        V: ItemVisitor<'de>,
    {
        self.parse_internal(|parser| parse_item(parser, visitor))
    }

    fn peek(&self) -> Option<u8> {
        self.input.get(self.index).copied()
    }

    fn next(&mut self) -> Option<u8> {
        self.peek().inspect(|_| self.index += 1)
    }

    // Generic parse method for checking input before parsing
    // and handling trailing text error
    fn parse_internal<T>(
        mut self,
        f: impl FnOnce(&mut Self) -> Result<T, error::Repr>,
    ) -> SFVResult<T> {
        // https://httpwg.org/specs/rfc9651.html#text-parse

        self.consume_sp_chars();

        let value = f(&mut self)?;

        self.consume_sp_chars();

        if self.peek().is_some() {
            return Err(error::Repr::TrailingCharactersAfterParsedValue(self.index).into());
        }

        Ok(value)
    }

    fn parse_list_entry(&mut self, visitor: impl EntryVisitor<'de>) -> Result<(), error::Repr> {
        // https://httpwg.org/specs/rfc9651.html#parse-item-or-list
        // ListEntry represents a tuple (item_or_inner_list, parameters)

        if let Some(b'(') = self.peek() {
            self.parse_inner_list(visitor.inner_list()?)
        } else {
            parse_item(self, visitor.item()?)?;
            Ok(())
        }
    }

    pub(crate) fn parse_inner_list(
        &mut self,
        mut visitor: impl InnerListVisitor<'de>,
    ) -> Result<(), error::Repr> {
        // https://httpwg.org/specs/rfc9651.html#parse-innerlist

        if Some(b'(') != self.peek() {
            return Err(error::Repr::ExpectedStartOfInnerList(self.index));
        }

        self.next();

        while self.peek().is_some() {
            self.consume_sp_chars();

            if Some(b')') == self.peek() {
                self.next();
                let param_visitor = visitor.finish()?;
                self.parse_parameters(param_visitor)?;
                return Ok(());
            }

            parse_item(self, visitor.item()?)?;

            if let Some(c) = self.peek() {
                if c != b' ' && c != b')' {
                    return Err(error::Repr::ExpectedInnerListDelimiter(self.index));
                }
            }
        }

        Err(error::Repr::UnterminatedInnerList(self.index))
    }

    pub(crate) fn parse_bare_item(&mut self) -> Result<BareItemFromInput<'de>, error::Repr> {
        // https://httpwg.org/specs/rfc9651.html#parse-bare-item

        Ok(match self.peek() {
            Some(b'?') => BareItemFromInput::Boolean(self.parse_bool()?),
            Some(b'"') => BareItemFromInput::String(self.parse_string()?),
            Some(b':') => BareItemFromInput::ByteSequence(self.parse_byte_sequence()?),
            Some(b'@') => BareItemFromInput::Date(self.parse_date()?),
            Some(b'%') => BareItemFromInput::DisplayString(self.parse_display_string()?),
            Some(c) if utils::is_allowed_start_token_char(c) => {
                BareItemFromInput::Token(self.parse_token()?)
            }
            Some(c) if c == b'-' || c.is_ascii_digit() => match self.parse_number()? {
                Num::Decimal(val) => BareItemFromInput::Decimal(val),
                Num::Integer(val) => BareItemFromInput::Integer(val),
            },
            _ => return Err(error::Repr::ExpectedStartOfBareItem(self.index)),
        })
    }

    pub(crate) fn parse_bool(&mut self) -> Result<bool, error::Repr> {
        // https://httpwg.org/specs/rfc9651.html#parse-boolean

        if self.peek() != Some(b'?') {
            return Err(error::Repr::ExpectedStartOfBoolean(self.index));
        }

        self.next();

        match self.peek() {
            Some(b'0') => {
                self.next();
                Ok(false)
            }
            Some(b'1') => {
                self.next();
                Ok(true)
            }
            _ => Err(error::Repr::ExpectedBoolean(self.index)),
        }
    }

    pub(crate) fn parse_string(&mut self) -> Result<Cow<'de, StringRef>, error::Repr> {
        // https://httpwg.org/specs/rfc9651.html#parse-string

        if self.peek() != Some(b'"') {
            return Err(error::Repr::ExpectedStartOfString(self.index));
        }

        self.next();

        let start = self.index;
        let mut output = Vec::new();

        while let Some(curr_char) = self.peek() {
            match curr_char {
                b'"' => {
                    let end = self.index;
                    self.next();
                    // TODO: The UTF-8 validation is redundant with the preceding character checks, but
                    // its removal is only possible with unsafe code.
                    return Ok(if output.is_empty() {
                        let slice = &self.input[start..end];
                        let output = std::str::from_utf8(slice).unwrap();
                        Cow::Borrowed(StringRef::from_validated_str(output))
                    } else {
                        let output = StdString::from_utf8(output).unwrap();
                        Cow::Owned(String::from_validated_string(output))
                    });
                }
                0x00..=0x1f | 0x7f..=0xff => {
                    return Err(error::Repr::InvalidStringCharacter(self.index));
                }
                b'\\' => {
                    let escape_index = self.index;
                    self.next();
                    match self.peek() {
                        Some(c @ (b'\\' | b'"')) => {
                            self.next();
                            if output.is_empty() {
                                output = self.input[start..escape_index].to_vec();
                            }
                            output.push(c);
                        }
                        None => return Err(error::Repr::UnterminatedEscapeSequence(self.index)),
                        Some(_) => return Err(error::Repr::InvalidEscapeSequence(self.index)),
                    }
                }
                _ => {
                    self.next();
                    if !output.is_empty() {
                        output.push(curr_char);
                    }
                }
            }
        }
        Err(error::Repr::UnterminatedString(self.index))
    }

    fn parse_non_empty_str(
        &mut self,
        is_allowed_start_char: impl FnOnce(u8) -> bool,
        is_allowed_inner_char: impl Fn(u8) -> bool,
    ) -> Option<&'de str> {
        let start = self.index;

        match self.peek() {
            Some(c) if is_allowed_start_char(c) => {
                self.next();
            }
            _ => return None,
        }

        loop {
            match self.peek() {
                Some(c) if is_allowed_inner_char(c) => {
                    self.next();
                }
                // TODO: The UTF-8 validation is redundant with the preceding character checks, but
                // its removal is only possible with unsafe code.
                _ => return Some(std::str::from_utf8(&self.input[start..self.index]).unwrap()),
            }
        }
    }

    pub(crate) fn parse_token(&mut self) -> Result<&'de TokenRef, error::Repr> {
        // https://httpwg.org/specs/rfc9651.html#parse-token

        match self.parse_non_empty_str(
            utils::is_allowed_start_token_char,
            utils::is_allowed_inner_token_char,
        ) {
            None => Err(error::Repr::ExpectedStartOfToken(self.index)),
            Some(str) => Ok(TokenRef::from_validated_str(str)),
        }
    }

    pub(crate) fn parse_byte_sequence(&mut self) -> Result<Vec<u8>, error::Repr> {
        // https://httpwg.org/specs/rfc9651.html#parse-binary

        if self.peek() != Some(b':') {
            return Err(error::Repr::ExpectedStartOfByteSequence(self.index));
        }

        self.next();
        let start = self.index;

        loop {
            match self.next() {
                Some(b':') => break,
                Some(_) => {}
                None => return Err(error::Repr::UnterminatedByteSequence(self.index)),
            }
        }

        let colon_index = self.index - 1;

        if !self.lenient {
            return match base64::Engine::decode(&utils::BASE64, &self.input[start..colon_index]) {
                Ok(content) => Ok(content),
                Err(err) => {
                    let index = match err {
                        base64::DecodeError::InvalidByte(offset, _)
                        | base64::DecodeError::InvalidLastSymbol(offset, _) => start + offset,
                        // Report these two at the position of the last base64
                        // character, since they correspond to errors in the input
                        // as a whole.
                        base64::DecodeError::InvalidLength(_)
                        | base64::DecodeError::InvalidPadding => colon_index - 1,
                    };

                    Err(error::Repr::InvalidByteSequence(index))
                }
            };
        }

        // Replicate Quiche's bug-for-bug compatible base64 decoding.
        // 1. Quiche synthesizes padding by resizing to a multiple of 4.
        // 2. It uses absl::Base64Unescape which allows '.' as padding.
        // 3. It skips ASCII whitespace anywhere in the input.
        let mut base64_vec = self.input[start..colon_index].to_vec();
        let padding_len = (base64_vec.len() + 3) / 4 * 4;
        base64_vec.resize(padding_len, b'=');

        let mut dest = Vec::new();
        let mut temp: u32 = 0;
        let mut state = 0;
        let mut equals = 0;
        let mut in_padding = false;

        for &ch in &base64_vec {
            if ch.is_ascii_whitespace() || ch == 0x0b {
                continue;
            }
            if ch == b'=' || ch == b'.' {
                in_padding = true;
                equals += 1;
                continue;
            }
            if in_padding {
                return Err(error::Repr::InvalidByteSequence(start));
            }
            let val = match ch {
                b'A'..=b'Z' => ch - b'A',
                b'a'..=b'z' => ch - b'a' + 26,
                b'0'..=b'9' => ch - b'0' + 52,
                b'+' => 62,
                b'/' => 63,
                _ => return Err(error::Repr::InvalidByteSequence(start)),
            };
            temp = (temp << 6) | u32::from(val);
            state += 1;
            if state == 4 {
                dest.push((temp >> 16) as u8);
                dest.push((temp >> 8) as u8);
                dest.push(temp as u8);
                temp = 0;
                state = 0;
            }
        }
        let expected_equals = match state {
            0 => 0,
            2 => 2,
            3 => 1,
            _ => return Err(error::Repr::InvalidByteSequence(start)),
        };
        if equals != 0 && equals != expected_equals {
            return Err(error::Repr::InvalidByteSequence(start));
        }
        if state == 2 {
            dest.push((temp >> 4) as u8);
        } else if state == 3 {
            dest.push((temp >> 10) as u8);
            dest.push((temp >> 2) as u8);
        }
        Ok(dest)
    }

    pub(crate) fn parse_number(&mut self) -> Result<Num, error::Repr> {
        // https://httpwg.org/specs/rfc9651.html#parse-number

        fn char_to_i64(c: u8) -> i64 {
            i64::from(c - b'0')
        }

        let sign = if let Some(b'-') = self.peek() {
            self.next();
            -1
        } else {
            1
        };

        let mut magnitude = match self.peek() {
            Some(c @ b'0'..=b'9') => {
                self.next();
                char_to_i64(c)
            }
            _ => return Err(error::Repr::ExpectedDigit(self.index)),
        };

        let mut digits = 1;

        loop {
            match self.peek() {
                Some(b'.') => {
                    if digits > 12 {
                        return Err(error::Repr::TooManyDigitsBeforeDecimalPoint(self.index));
                    }
                    self.next();
                    break;
                }
                Some(c @ b'0'..=b'9') => {
                    digits += 1;
                    if digits > 15 {
                        return Err(error::Repr::TooManyDigits(self.index));
                    }
                    self.next();
                    magnitude = magnitude * 10 + char_to_i64(c);
                }
                _ => return Ok(Num::Integer(Integer::from_validated_i64(sign * magnitude))),
            }
        }

        magnitude *= 1000;
        let mut scale = 100;

        while let Some(c @ b'0'..=b'9') = self.peek() {
            if scale == 0 {
                return Err(error::Repr::TooManyDigitsAfterDecimalPoint(self.index));
            }

            self.next();
            magnitude += char_to_i64(c) * scale;
            scale /= 10;
        }

        if scale == 100 && !self.lenient {
            // Report the error at the position of the decimal itself, rather
            // than the next position.
            //
            // RFC 8941 requires at least one digit after the decimal point.
            // However, legacy Quiche allows trailing decimal points (e.g. "1.")
            // if followed by a delimiter or EOF.
            Err(error::Repr::TrailingDecimalPoint(self.index - 1))
        } else {
            Ok(Num::Decimal(Decimal::from_integer_scaled_1000(
                Integer::from_validated_i64(sign * magnitude),
            )))
        }
    }

    pub(crate) fn parse_date(&mut self) -> Result<Date, error::Repr> {
        // https://httpwg.org/specs/rfc9651.html#parse-date

        if self.peek() != Some(b'@') {
            return Err(error::Repr::ExpectedStartOfDate(self.index));
        }

        match self.version {
            Version::Rfc8941 => return Err(error::Repr::Rfc8941Date(self.index)),
            Version::Rfc9651 => {}
        }

        let start = self.index;
        self.next();

        match self.parse_number()? {
            Num::Integer(seconds) => Ok(Date::from_unix_seconds(seconds)),
            Num::Decimal(_) => Err(error::Repr::NonIntegerDate(start)),
        }
    }

    pub(crate) fn parse_display_string(&mut self) -> Result<Cow<'de, str>, error::Repr> {
        // https://httpwg.org/specs/rfc9651.html#parse-display

        if self.peek() != Some(b'%') {
            return Err(error::Repr::ExpectedStartOfDisplayString(self.index));
        }

        match self.version {
            Version::Rfc8941 => return Err(error::Repr::Rfc8941DisplayString(self.index)),
            Version::Rfc9651 => {}
        }

        self.next();

        if self.peek() != Some(b'"') {
            return Err(error::Repr::ExpectedQuote(self.index));
        }

        self.next();

        let start = self.index;
        let mut output = Vec::new();

        while let Some(curr_char) = self.peek() {
            match curr_char {
                b'"' => {
                    let end = self.index;
                    self.next();
                    return if output.is_empty() {
                        let slice = &self.input[start..end];
                        match std::str::from_utf8(slice) {
                            Ok(output) => Ok(Cow::Borrowed(output)),
                            Err(err) => Err(error::Repr::InvalidUtf8InDisplayString(
                                start + err.valid_up_to(),
                            )),
                        }
                    } else {
                        match StdString::from_utf8(output) {
                            Ok(output) => Ok(Cow::Owned(output)),
                            Err(err) => Err(error::Repr::InvalidUtf8InDisplayString(
                                start + err.utf8_error().valid_up_to(),
                            )),
                        }
                    };
                }
                0x00..=0x1f | 0x7f..=0xff => {
                    return Err(error::Repr::InvalidDisplayStringCharacter(self.index));
                }
                b'%' => {
                    let escape_index = self.index;
                    self.next();

                    let mut octet = 0;

                    for _ in 0..2 {
                        octet = (octet << 4)
                            + match self.peek() {
                                Some(c @ b'0'..=b'9') => {
                                    self.next();
                                    c - b'0'
                                }
                                Some(c @ b'a'..=b'f') => {
                                    self.next();
                                    c - b'a' + 10
                                }
                                None => {
                                    return Err(error::Repr::UnterminatedEscapeSequence(self.index))
                                }
                                Some(_) => {
                                    return Err(error::Repr::InvalidEscapeSequence(self.index))
                                }
                            };
                    }

                    if output.is_empty() {
                        output = self.input[start..escape_index].to_vec();
                    }
                    output.push(octet);
                }
                _ => {
                    self.next();
                    if !output.is_empty() {
                        output.push(curr_char);
                    }
                }
            }
        }
        Err(error::Repr::UnterminatedDisplayString(self.index))
    }

    pub(crate) fn parse_parameters<V>(&mut self, mut visitor: V) -> Result<V::Out, error::Repr>
    where
        V: ParameterVisitor<'de>,
    {
        // https://httpwg.org/specs/rfc9651.html#parse-param

        while let Some(b';') = self.peek() {
            self.next();
            self.consume_sp_chars();

            let param_name = self.parse_key()?;
            let param_value = match self.peek() {
                Some(b'=') => {
                    self.next();
                    self.parse_bare_item()?
                }
                _ => BareItemFromInput::Boolean(true),
            };
            // Note: It is up to the visitor to properly handle duplicate keys.
            visitor.parameter(param_name, param_value)?;
        }

        Ok(visitor.finish()?)
    }

    pub(crate) fn parse_key(&mut self) -> Result<&'de KeyRef, error::Repr> {
        // https://httpwg.org/specs/rfc9651.html#parse-key

        match self.parse_non_empty_str(
            utils::is_allowed_start_key_char,
            utils::is_allowed_inner_key_char,
        ) {
            None => Err(error::Repr::ExpectedStartOfKey(self.index)),
            Some(str) => Ok(KeyRef::from_validated_str(str)),
        }
    }

    fn consume_ows_chars(&mut self) {
        while let Some(b' ' | b'\t') = self.peek() {
            self.next();
        }
    }

    fn consume_sp_chars(&mut self) {
        while let Some(b' ') = self.peek() {
            self.next();
        }
    }

    #[cfg(test)]
    pub(crate) fn remaining(&self) -> &[u8] {
        &self.input[self.index..]
    }
}
