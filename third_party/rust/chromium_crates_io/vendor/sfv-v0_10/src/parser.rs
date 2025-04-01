use crate::utils;
use crate::{
    BareItem, Decimal, Dictionary, FromStr, InnerList, Item, List, ListEntry, Num, Parameters,
    SFVResult,
};
use std::iter::Peekable;
use std::str::{from_utf8, Chars};

/// Implements parsing logic for each structured field value type.
pub trait ParseValue {
    /// This method should not be used for parsing input into structured field value.
    /// Use `Parser::parse_item`, `Parser::parse_list` or `Parsers::parse_dictionary` for that.
    fn parse(input_chars: &mut Peekable<Chars>) -> SFVResult<Self>
    where
        Self: Sized;
}

/// If structured field value of List or Dictionary type is split into multiple lines,
/// allows to parse more lines and merge them into already existing structure field value.
pub trait ParseMore {
    /// If structured field value is split across lines,
    /// parses and merges next line into a single structured field value.
    /// # Examples
    /// ```
    /// # use sfv::{Parser, SerializeValue, ParseMore};
    ///
    /// let mut list_field = Parser::parse_list("11, (12 13)".as_bytes()).unwrap();
    /// list_field.parse_more("\"foo\",        \"bar\"".as_bytes()).unwrap();
    ///
    /// assert_eq!(list_field.serialize_value().unwrap(), "11, (12 13), \"foo\", \"bar\"");
    fn parse_more(&mut self, input_bytes: &[u8]) -> SFVResult<()>
    where
        Self: Sized;
}

impl ParseValue for Item {
    fn parse(input_chars: &mut Peekable<Chars>) -> SFVResult<Item> {
        // https://httpwg.org/specs/rfc8941.html#parse-item
        let bare_item = Parser::parse_bare_item(input_chars)?;
        let params = Parser::parse_parameters(input_chars)?;

        Ok(Item { bare_item, params })
    }
}

impl ParseValue for List {
    fn parse(input_chars: &mut Peekable<Chars>) -> SFVResult<List> {
        // https://httpwg.org/specs/rfc8941.html#parse-list
        // List represents an array of (item_or_inner_list, parameters)

        let mut members = vec![];

        while input_chars.peek().is_some() {
            members.push(Parser::parse_list_entry(input_chars)?);

            utils::consume_ows_chars(input_chars);

            if input_chars.peek().is_none() {
                return Ok(members);
            }

            if let Some(c) = input_chars.next() {
                if c != ',' {
                    return Err("parse_list: trailing characters after list member");
                }
            }

            utils::consume_ows_chars(input_chars);

            if input_chars.peek().is_none() {
                return Err("parse_list: trailing comma");
            }
        }

        Ok(members)
    }
}

impl ParseValue for Dictionary {
    fn parse(input_chars: &mut Peekable<Chars>) -> SFVResult<Dictionary> {
        let mut dict = Dictionary::new();

        while input_chars.peek().is_some() {
            let this_key = Parser::parse_key(input_chars)?;

            if let Some('=') = input_chars.peek() {
                input_chars.next();
                let member = Parser::parse_list_entry(input_chars)?;
                dict.insert(this_key, member);
            } else {
                let value = true;
                let params = Parser::parse_parameters(input_chars)?;
                let member = Item {
                    bare_item: BareItem::Boolean(value),
                    params,
                };
                dict.insert(this_key, member.into());
            }

            utils::consume_ows_chars(input_chars);

            if input_chars.peek().is_none() {
                return Ok(dict);
            }

            if let Some(c) = input_chars.next() {
                if c != ',' {
                    return Err("parse_dict: trailing characters after dictionary member");
                }
            }

            utils::consume_ows_chars(input_chars);

            if input_chars.peek().is_none() {
                return Err("parse_dict: trailing comma");
            }
        }
        Ok(dict)
    }
}

impl ParseMore for List {
    fn parse_more(&mut self, input_bytes: &[u8]) -> SFVResult<()> {
        let parsed_list = Parser::parse_list(input_bytes)?;
        self.extend(parsed_list);
        Ok(())
    }
}

impl ParseMore for Dictionary {
    fn parse_more(&mut self, input_bytes: &[u8]) -> SFVResult<()> {
        let parsed_dict = Parser::parse_dictionary(input_bytes)?;
        self.extend(parsed_dict);
        Ok(())
    }
}

/// Exposes methods for parsing input into structured field value.
pub struct Parser;

impl Parser {
    /// Parses input into structured field value of Dictionary type
    pub fn parse_dictionary(input_bytes: &[u8]) -> SFVResult<Dictionary> {
        Self::parse::<Dictionary>(input_bytes)
    }

    /// Parses input into structured field value of List type
    pub fn parse_list(input_bytes: &[u8]) -> SFVResult<List> {
        Self::parse::<List>(input_bytes)
    }

    /// Parses input into structured field value of Item type
    pub fn parse_item(input_bytes: &[u8]) -> SFVResult<Item> {
        Self::parse::<Item>(input_bytes)
    }

    // Generic parse method for checking input before parsing
    // and handling trailing text error
    fn parse<T: ParseValue>(input_bytes: &[u8]) -> SFVResult<T> {
        // https://httpwg.org/specs/rfc8941.html#text-parse
        if !input_bytes.is_ascii() {
            return Err("parse: non-ascii characters in input");
        }

        let mut input_chars = from_utf8(input_bytes)
            .map_err(|_| "parse: conversion from bytes to str failed")?
            .chars()
            .peekable();
        utils::consume_sp_chars(&mut input_chars);

        let output = T::parse(&mut input_chars)?;

        utils::consume_sp_chars(&mut input_chars);

        if input_chars.next().is_some() {
            return Err("parse: trailing characters after parsed value");
        };
        Ok(output)
    }

    fn parse_list_entry(input_chars: &mut Peekable<Chars>) -> SFVResult<ListEntry> {
        // https://httpwg.org/specs/rfc8941.html#parse-item-or-list
        // ListEntry represents a tuple (item_or_inner_list, parameters)

        match input_chars.peek() {
            Some('(') => {
                let parsed = Self::parse_inner_list(input_chars)?;
                Ok(ListEntry::InnerList(parsed))
            }
            _ => {
                let parsed = Item::parse(input_chars)?;
                Ok(ListEntry::Item(parsed))
            }
        }
    }

    pub(crate) fn parse_inner_list(input_chars: &mut Peekable<Chars>) -> SFVResult<InnerList> {
        // https://httpwg.org/specs/rfc8941.html#parse-innerlist

        if Some('(') != input_chars.next() {
            return Err("parse_inner_list: input does not start with '('");
        }

        let mut inner_list = Vec::new();
        while input_chars.peek().is_some() {
            utils::consume_sp_chars(input_chars);

            if Some(&')') == input_chars.peek() {
                input_chars.next();
                let params = Self::parse_parameters(input_chars)?;
                return Ok(InnerList {
                    items: inner_list,
                    params,
                });
            }

            let parsed_item = Item::parse(input_chars)?;
            inner_list.push(parsed_item);

            if let Some(c) = input_chars.peek() {
                if c != &' ' && c != &')' {
                    return Err("parse_inner_list: bad delimitation");
                }
            }
        }

        Err("parse_inner_list: the end of the inner list was not found")
    }

    pub(crate) fn parse_bare_item(input_chars: &mut Peekable<Chars>) -> SFVResult<BareItem> {
        // https://httpwg.org/specs/rfc8941.html#parse-bare-item
        if input_chars.peek().is_none() {
            return Err("parse_bare_item: empty item");
        }

        match input_chars.peek() {
            Some(&'?') => Ok(BareItem::Boolean(Self::parse_bool(input_chars)?)),
            Some(&'"') => Ok(BareItem::String(Self::parse_string(input_chars)?)),
            Some(&':') => Ok(BareItem::ByteSeq(Self::parse_byte_sequence(input_chars)?)),
            Some(&c) if c == '*' || c.is_ascii_alphabetic() => {
                Ok(BareItem::Token(Self::parse_token(input_chars)?))
            }
            Some(&c) if c == '-' || c.is_ascii_digit() => match Self::parse_number(input_chars)? {
                Num::Decimal(val) => Ok(BareItem::Decimal(val)),
                Num::Integer(val) => Ok(BareItem::Integer(val)),
            },
            _ => Err("parse_bare_item: item type can't be identified"),
        }
    }

    pub(crate) fn parse_bool(input_chars: &mut Peekable<Chars>) -> SFVResult<bool> {
        // https://httpwg.org/specs/rfc8941.html#parse-boolean

        if input_chars.next() != Some('?') {
            return Err("parse_bool: first character is not '?'");
        }

        match input_chars.next() {
            Some('0') => Ok(false),
            Some('1') => Ok(true),
            _ => Err("parse_bool: invalid variant"),
        }
    }

    pub(crate) fn parse_string(input_chars: &mut Peekable<Chars>) -> SFVResult<String> {
        // https://httpwg.org/specs/rfc8941.html#parse-string

        if input_chars.next() != Some('\"') {
            return Err("parse_string: first character is not '\"'");
        }

        let mut output_string = String::from("");
        while let Some(curr_char) = input_chars.next() {
            match curr_char {
                '\"' => return Ok(output_string),
                '\x7f' | '\x00'..='\x1f' => return Err("parse_string: not a visible character"),
                '\\' => match input_chars.next() {
                    Some(c) if c == '\\' || c == '\"' => {
                        output_string.push(c);
                    }
                    None => return Err("parse_string: last input character is '\\'"),
                    _ => return Err("parse_string: disallowed character after '\\'"),
                },
                _ => output_string.push(curr_char),
            }
        }
        Err("parse_string: no closing '\"'")
    }

    pub(crate) fn parse_token(input_chars: &mut Peekable<Chars>) -> SFVResult<String> {
        // https://httpwg.org/specs/rfc8941.html#parse-token

        if let Some(first_char) = input_chars.peek() {
            if !first_char.is_ascii_alphabetic() && first_char != &'*' {
                return Err("parse_token: first character is not ALPHA or '*'");
            }
        } else {
            return Err("parse_token: empty input string");
        }

        let mut output_string = String::from("");
        while let Some(curr_char) = input_chars.peek() {
            if !utils::is_tchar(*curr_char) && curr_char != &':' && curr_char != &'/' {
                return Ok(output_string);
            }

            match input_chars.next() {
                Some(c) => output_string.push(c),
                None => return Err("parse_token: end of the string"),
            }
        }
        Ok(output_string)
    }

    pub(crate) fn parse_byte_sequence(input_chars: &mut Peekable<Chars>) -> SFVResult<Vec<u8>> {
        // https://httpwg.org/specs/rfc8941.html#parse-binary

        if input_chars.next() != Some(':') {
            return Err("parse_byte_seq: first char is not ':'");
        }

        if !input_chars.clone().any(|c| c == ':') {
            return Err("parse_byte_seq: no closing ':'");
        }

        let b64_content = input_chars.take_while(|c| c != &':').collect::<String>();
        if !b64_content.chars().all(utils::is_allowed_b64_content) {
            return Err("parse_byte_seq: invalid char in byte sequence");
        }
        match base64::Engine::decode(&utils::BASE64, b64_content) {
            Ok(content) => Ok(content),
            Err(_) => Err("parse_byte_seq: decoding error"),
        }
    }

    pub(crate) fn parse_number(input_chars: &mut Peekable<Chars>) -> SFVResult<Num> {
        // https://httpwg.org/specs/rfc8941.html#parse-number

        let mut sign = 1;
        if let Some('-') = input_chars.peek() {
            sign = -1;
            input_chars.next();
        }

        match input_chars.peek() {
            Some(c) if !c.is_ascii_digit() => {
                return Err("parse_number: input number does not start with a digit")
            }
            None => return Err("parse_number: input number lacks a digit"),
            _ => (),
        }

        // Get number from input as a string and identify whether it's a decimal or integer
        let (is_integer, input_number) = Self::extract_digits(input_chars)?;

        // Parse input_number from string into integer
        if is_integer {
            let output_number = input_number
                .parse::<i64>()
                .map_err(|_err| "parse_number: parsing i64 failed")?
                * sign;

            let (min_int, max_int) = (-999_999_999_999_999_i64, 999_999_999_999_999_i64);
            if !(min_int <= output_number && output_number <= max_int) {
                return Err("parse_number: integer number is out of range");
            }

            return Ok(Num::Integer(output_number));
        }

        // Parse input_number from string into decimal
        let chars_after_dot = input_number
            .find('.')
            .map(|dot_pos| input_number.len() - dot_pos - 1);

        match chars_after_dot {
            Some(0) => Err("parse_number: decimal ends with '.'"),
            Some(1..=3) => {
                let mut output_number = Decimal::from_str(&input_number)
                    .map_err(|_err| "parse_number: parsing f64 failed")?;

                if sign == -1 {
                    output_number.set_sign_negative(true)
                }

                Ok(Num::Decimal(output_number))
            }
            _ => Err("parse_number: invalid decimal fraction length"),
        }
    }

    fn extract_digits(input_chars: &mut Peekable<Chars>) -> SFVResult<(bool, String)> {
        let mut is_integer = true;
        let mut input_number = String::from("");
        while let Some(curr_char) = input_chars.peek() {
            if curr_char.is_ascii_digit() {
                input_number.push(*curr_char);
                input_chars.next();
            } else if curr_char == &'.' && is_integer {
                if input_number.len() > 12 {
                    return Err(
                        "parse_number: decimal too long, illegal position for decimal point",
                    );
                }
                input_number.push(*curr_char);
                is_integer = false;
                input_chars.next();
            } else {
                break;
            }

            if is_integer && input_number.len() > 15 {
                return Err("parse_number: integer too long, length > 15");
            }

            if !is_integer && input_number.len() > 16 {
                return Err("parse_number: decimal too long, length > 16");
            }
        }
        Ok((is_integer, input_number))
    }

    pub(crate) fn parse_parameters(input_chars: &mut Peekable<Chars>) -> SFVResult<Parameters> {
        // https://httpwg.org/specs/rfc8941.html#parse-param

        let mut params = Parameters::new();

        while let Some(curr_char) = input_chars.peek() {
            if curr_char == &';' {
                input_chars.next();
            } else {
                break;
            }

            utils::consume_sp_chars(input_chars);

            let param_name = Self::parse_key(input_chars)?;
            let param_value = match input_chars.peek() {
                Some('=') => {
                    input_chars.next();
                    Self::parse_bare_item(input_chars)?
                }
                _ => BareItem::Boolean(true),
            };
            params.insert(param_name, param_value);
        }

        // If parameters already contains a name param_name (comparing character-for-character), overwrite its value.
        // Note that when duplicate Parameter keys are encountered, this has the effect of ignoring all but the last instance.
        Ok(params)
    }

    pub(crate) fn parse_key(input_chars: &mut Peekable<Chars>) -> SFVResult<String> {
        match input_chars.peek() {
            Some(c) if c == &'*' || c.is_ascii_lowercase() => (),
            _ => return Err("parse_key: first character is not lcalpha or '*'"),
        }

        let mut output = String::new();
        while let Some(curr_char) = input_chars.peek() {
            if !curr_char.is_ascii_lowercase()
                && !curr_char.is_ascii_digit()
                && !"_-*.".contains(*curr_char)
            {
                return Ok(output);
            }

            output.push(*curr_char);
            input_chars.next();
        }
        Ok(output)
    }
}
