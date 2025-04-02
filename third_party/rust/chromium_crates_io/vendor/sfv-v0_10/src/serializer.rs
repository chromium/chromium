use crate::utils;
use crate::{
    BareItem, Decimal, Dictionary, InnerList, Item, List, ListEntry, Parameters, RefBareItem,
    SFVResult,
};
use std::fmt::Write as _;

/// Serializes structured field value into String.
pub trait SerializeValue {
    /// Serializes structured field value into String.
    /// # Examples
    /// ```
    /// # use sfv::{Parser, SerializeValue, ParseValue};
    ///
    /// let parsed_list_field = Parser::parse_list("\"london\", \t\t\"berlin\"".as_bytes());
    /// assert!(parsed_list_field.is_ok());
    ///
    /// assert_eq!(
    ///     parsed_list_field.unwrap().serialize_value().unwrap(),
    ///     "\"london\", \"berlin\""
    /// );
    /// ```
    fn serialize_value(&self) -> SFVResult<String>;
}

impl SerializeValue for Dictionary {
    fn serialize_value(&self) -> SFVResult<String> {
        let mut output = String::new();
        Serializer::serialize_dict(self, &mut output)?;
        Ok(output)
    }
}

impl SerializeValue for List {
    fn serialize_value(&self) -> SFVResult<String> {
        let mut output = String::new();
        Serializer::serialize_list(self, &mut output)?;
        Ok(output)
    }
}

impl SerializeValue for Item {
    fn serialize_value(&self) -> SFVResult<String> {
        let mut output = String::new();
        Serializer::serialize_item(self, &mut output)?;
        Ok(output)
    }
}

/// Container serialization functions
pub(crate) struct Serializer;

impl Serializer {
    pub(crate) fn serialize_item(input_item: &Item, output: &mut String) -> SFVResult<()> {
        // https://httpwg.org/specs/rfc8941.html#ser-item

        Self::serialize_bare_item(&input_item.bare_item, output)?;
        Self::serialize_parameters(&input_item.params, output)?;
        Ok(())
    }

    #[allow(clippy::ptr_arg)]
    pub(crate) fn serialize_list(input_list: &List, output: &mut String) -> SFVResult<()> {
        // https://httpwg.org/specs/rfc8941.html#ser-list
        if input_list.is_empty() {
            return Err("serialize_list: serializing empty field is not allowed");
        }

        for (idx, member) in input_list.iter().enumerate() {
            match member {
                ListEntry::Item(item) => {
                    Self::serialize_item(item, output)?;
                }
                ListEntry::InnerList(inner_list) => {
                    Self::serialize_inner_list(inner_list, output)?;
                }
            };

            // If more items remain in input_list:
            //      Append “,” to output.
            //      Append a single SP to output.
            if idx < input_list.len() - 1 {
                output.push_str(", ");
            }
        }
        Ok(())
    }

    pub(crate) fn serialize_dict(input_dict: &Dictionary, output: &mut String) -> SFVResult<()> {
        // https://httpwg.org/specs/rfc8941.html#ser-dictionary
        if input_dict.is_empty() {
            return Err("serialize_dictionary: serializing empty field is not allowed");
        }

        for (idx, (member_name, member_value)) in input_dict.iter().enumerate() {
            Serializer::serialize_key(member_name, output)?;

            match member_value {
                ListEntry::Item(ref item) => {
                    // If dict member is boolean true, no need to serialize it: only its params must be serialized
                    // Otherwise serialize entire item with its params
                    if item.bare_item == BareItem::Boolean(true) {
                        Self::serialize_parameters(&item.params, output)?;
                    } else {
                        output.push('=');
                        Self::serialize_item(item, output)?;
                    }
                }
                ListEntry::InnerList(inner_list) => {
                    output.push('=');
                    Self::serialize_inner_list(inner_list, output)?;
                }
            }

            // If more items remain in input_dictionary:
            //      Append “,” to output.
            //      Append a single SP to output.
            if idx < input_dict.len() - 1 {
                output.push_str(", ");
            }
        }
        Ok(())
    }

    fn serialize_inner_list(input_inner_list: &InnerList, output: &mut String) -> SFVResult<()> {
        // https://httpwg.org/specs/rfc8941.html#ser-innerlist

        let items = &input_inner_list.items;
        let inner_list_parameters = &input_inner_list.params;

        output.push('(');
        for (idx, item) in items.iter().enumerate() {
            Self::serialize_item(item, output)?;

            // If more values remain in inner_list, append a single SP to output
            if idx < items.len() - 1 {
                output.push(' ');
            }
        }
        output.push(')');
        Self::serialize_parameters(inner_list_parameters, output)?;
        Ok(())
    }

    pub(crate) fn serialize_bare_item(
        input_bare_item: &BareItem,
        output: &mut String,
    ) -> SFVResult<()> {
        // https://httpwg.org/specs/rfc8941.html#ser-bare-item

        let ref_bare_item = input_bare_item.to_ref_bare_item();
        Self::serialize_ref_bare_item(&ref_bare_item, output)
    }

    pub(crate) fn serialize_ref_bare_item(
        value: &RefBareItem,
        output: &mut String,
    ) -> SFVResult<()> {
        match value {
            RefBareItem::Boolean(value) => Self::serialize_bool(*value, output)?,
            RefBareItem::String(value) => Self::serialize_string(value, output)?,
            RefBareItem::ByteSeq(value) => Self::serialize_byte_sequence(value, output)?,
            RefBareItem::Token(value) => Self::serialize_token(value, output)?,
            RefBareItem::Integer(value) => Self::serialize_integer(*value, output)?,
            RefBareItem::Decimal(value) => Self::serialize_decimal(*value, output)?,
        };
        Ok(())
    }

    pub(crate) fn serialize_parameters(
        input_params: &Parameters,
        output: &mut String,
    ) -> SFVResult<()> {
        // https://httpwg.org/specs/rfc8941.html#ser-params

        for (param_name, param_value) in input_params.iter() {
            Self::serialize_ref_parameter(param_name, &param_value.to_ref_bare_item(), output)?;
        }
        Ok(())
    }

    pub(crate) fn serialize_ref_parameter(
        name: &str,
        value: &RefBareItem,
        output: &mut String,
    ) -> SFVResult<()> {
        output.push(';');
        Self::serialize_key(name, output)?;

        if value != &RefBareItem::Boolean(true) {
            output.push('=');
            Self::serialize_ref_bare_item(value, output)?;
        }
        Ok(())
    }

    pub(crate) fn serialize_key(input_key: &str, output: &mut String) -> SFVResult<()> {
        // https://httpwg.org/specs/rfc8941.html#ser-key

        match input_key.chars().next() {
            None => return Err("serialize_key: key is empty"),
            Some(char) => {
                if !(char.is_ascii_lowercase() || char == '*') {
                    return Err("serialize_key: first character is not lcalpha or '*'");
                }
            }
        }

        let disallowed_chars =
            |c: char| !(c.is_ascii_lowercase() || c.is_ascii_digit() || "_-*.".contains(c));

        if input_key.chars().any(disallowed_chars) {
            return Err("serialize_key: disallowed character in input");
        }

        output.push_str(input_key);
        Ok(())
    }

    pub(crate) fn serialize_integer(value: i64, output: &mut String) -> SFVResult<()> {
        //https://httpwg.org/specs/rfc8941.html#ser-integer

        let (min_int, max_int) = (-999_999_999_999_999_i64, 999_999_999_999_999_i64);
        if !(min_int <= value && value <= max_int) {
            return Err("serialize_integer: integer is out of range");
        }
        write!(output, "{}", value).unwrap();
        Ok(())
    }

    pub(crate) fn serialize_decimal(value: Decimal, output: &mut String) -> SFVResult<()> {
        // https://httpwg.org/specs/rfc8941.html#ser-decimal

        let fraction_length = 3;

        let decimal = value.round_dp(fraction_length);
        let int_comp = decimal.trunc();
        let fract_comp = decimal.fract();

        if int_comp.abs() > Decimal::from(999_999_999_999_i64) {
            return Err("serialize_decimal: integer component > 12 digits");
        }

        if fract_comp.is_zero() {
            write!(output, "{}.0", int_comp).unwrap();
        } else {
            write!(output, "{}", decimal).unwrap();
        }

        Ok(())
    }

    pub(crate) fn serialize_string(value: &str, output: &mut String) -> SFVResult<()> {
        // https://httpwg.org/specs/rfc8941.html#ser-integer

        if !value.is_ascii() {
            return Err("serialize_string: non-ascii character");
        }

        let vchar_or_sp = |char| char == '\x7f' || ('\x00'..='\x1f').contains(&char);
        if value.chars().any(vchar_or_sp) {
            return Err("serialize_string: not a visible character");
        }

        output.push('\"');
        for char in value.chars() {
            if char == '\\' || char == '\"' {
                output.push('\\');
            }
            output.push(char);
        }
        output.push('\"');

        Ok(())
    }

    pub(crate) fn serialize_token(value: &str, output: &mut String) -> SFVResult<()> {
        // https://httpwg.org/specs/rfc8941.html#ser-token

        if !value.is_ascii() {
            return Err("serialize_token: non-ascii character");
        }

        match value.chars().next() {
            None => return Err("serialize_token: token is empty"),
            Some(char) => {
                if !(char.is_ascii_alphabetic() || char == '*') {
                    return Err("serialize_token: first character is not ALPHA or '*'");
                }
            }
        }

        if value
            .chars()
            .any(|c| !(utils::is_tchar(c) || c == ':' || c == '/'))
        {
            return Err("serialize_token: disallowed character");
        }

        output.push_str(value);
        Ok(())
    }

    pub(crate) fn serialize_byte_sequence(value: &[u8], output: &mut String) -> SFVResult<()> {
        // https://httpwg.org/specs/rfc8941.html#ser-binary

        output.push(':');
        base64::Engine::encode_string(&utils::BASE64, value, output);
        output.push(':');
        Ok(())
    }

    pub(crate) fn serialize_bool(value: bool, output: &mut String) -> SFVResult<()> {
        // https://httpwg.org/specs/rfc8941.html#ser-boolean

        let val = if value { "?1" } else { "?0" };
        output.push_str(val);
        Ok(())
    }
}
