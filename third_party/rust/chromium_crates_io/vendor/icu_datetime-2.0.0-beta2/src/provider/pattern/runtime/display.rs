// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Set of `Display` implementations for reference and runtime `Pattern`.

use super::{
    super::{GenericPatternItem, PatternItem},
    GenericPattern, Pattern,
};
use crate::provider::fields::FieldSymbol;
use alloc::string::String;
use core::fmt::{self, Write};
use writeable::{impl_display_with_writeable, Writeable};

/// A helper function optimized to dump string buffers into `Pattern`
/// serialization wrapping minimal chunks of the buffer in escaping `'`
/// literals to produce valid UTF35 pattern string.
fn dump_buffer_into_formatter<W: Write + ?Sized>(literal: &str, formatter: &mut W) -> fmt::Result {
    if literal.is_empty() {
        return Ok(());
    }
    // Determine if the literal contains any characters that would need to be escaped.
    let mut needs_escaping = false;
    for ch in literal.chars() {
        if ch.is_ascii_alphabetic() || ch == '\'' {
            needs_escaping = true;
            break;
        }
    }

    if needs_escaping {
        let mut ch_iter = literal.trim_end().chars().peekable();

        // Do not escape the leading whitespace.
        while let Some(ch) = ch_iter.peek() {
            if ch.is_whitespace() {
                formatter.write_char(*ch)?;
                ch_iter.next();
            } else {
                break;
            }
        }

        // Wrap in "'" and escape "'".
        formatter.write_char('\'')?;
        for ch in ch_iter {
            if ch == '\'' {
                // Escape a single quote.
                formatter.write_char('\\')?;
            }
            formatter.write_char(ch)?;
        }
        formatter.write_char('\'')?;

        // Add the trailing whitespace
        for ch in literal.chars().rev() {
            if ch.is_whitespace() {
                formatter.write_char(ch)?;
            } else {
                break;
            }
        }
    } else {
        formatter.write_str(literal)?;
    }
    Ok(())
}

/// This trait is implemented in order to provide the machinery to convert a [`Pattern`] to a UTS 35
/// pattern string.
impl Writeable for Pattern<'_> {
    fn write_to<W: Write + ?Sized>(&self, formatter: &mut W) -> fmt::Result {
        let mut buffer = String::new();
        for pattern_item in self.items.iter() {
            match pattern_item {
                PatternItem::Field(field) => {
                    dump_buffer_into_formatter(&buffer, formatter)?;
                    buffer.clear();
                    let ch: char = field.symbol.into();
                    for _ in 0..field.length.to_len() {
                        formatter.write_char(ch)?;
                    }
                    if let FieldSymbol::DecimalSecond(decimal_second) = field.symbol {
                        formatter.write_char('.')?;
                        for _ in 0..(decimal_second as u8) {
                            formatter.write_char('S')?;
                        }
                    }
                }
                PatternItem::Literal(ch) => {
                    buffer.push(ch);
                }
            }
        }
        dump_buffer_into_formatter(&buffer, formatter)?;
        buffer.clear();
        Ok(())
    }
}

impl_display_with_writeable!(Pattern<'_>);

impl Writeable for GenericPattern<'_> {
    fn write_to<W: Write + ?Sized>(&self, formatter: &mut W) -> fmt::Result {
        let mut buffer = alloc::string::String::new();
        for pattern_item in self.items.iter() {
            match pattern_item {
                GenericPatternItem::Placeholder(idx) => {
                    dump_buffer_into_formatter(&buffer, formatter)?;
                    buffer.clear();
                    write!(formatter, "{{{idx}}}")?;
                }
                GenericPatternItem::Literal(ch) => {
                    buffer.push(ch);
                }
            }
        }
        dump_buffer_into_formatter(&buffer, formatter)?;
        buffer.clear();
        Ok(())
    }
}

impl_display_with_writeable!(GenericPattern<'_>);
