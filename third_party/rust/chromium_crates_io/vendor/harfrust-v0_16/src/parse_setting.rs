use crate::{text_parser::TextParser, Direction, Feature, Language, Script};
use read_fonts::model::Variation;

/// Parses a HarfBuzz-style setting from text.
///
/// This includes font variations, whose type is defined by `read-fonts` and
/// therefore cannot implement [`FromStr`](core::str::FromStr) here.
pub trait ParseSetting: Sized {
    /// Parses a setting from its textual representation.
    ///
    /// # Errors
    ///
    /// Returns an error if the text is not a valid setting.
    fn parse_setting(s: &str) -> Result<Self, &'static str>;
}

impl ParseSetting for Direction {
    fn parse_setting(s: &str) -> Result<Self, &'static str> {
        s.parse()
    }
}

impl ParseSetting for Script {
    fn parse_setting(s: &str) -> Result<Self, &'static str> {
        s.parse()
    }
}

impl ParseSetting for Language {
    fn parse_setting(s: &str) -> Result<Self, &'static str> {
        s.parse()
    }
}

impl ParseSetting for Feature {
    fn parse_setting(s: &str) -> Result<Self, &'static str> {
        s.parse()
    }
}

impl ParseSetting for Variation {
    fn parse_setting(s: &str) -> Result<Self, &'static str> {
        fn parse(s: &str) -> Option<Variation> {
            if s.is_empty() {
                return None;
            }

            let mut p = TextParser::new(s);
            p.skip_spaces();
            let quote = p.consume_quote();
            let tag = p.consume_tag()?;
            if let Some(quote) = quote {
                p.consume_byte(quote)?;
            }
            let _ = p.consume_byte(b'=');
            let value = p.consume_f32()?;
            p.skip_spaces();
            p.at_end().then_some(Variation::new(tag, value))
        }

        parse(s).ok_or("invalid variation")
    }
}
