//! pretty hex diffs, for comparing binary output.
//!
//! This is adapted from the pretty_assertions crate.
//!
//! source: https://github.com/colin-kiegel/rust-pretty-assertions/blob/main/pretty_assertions/src/lib.rs (MIT/Apache)

use std::fmt;

use nu_ansi_term::{Color, Color::Red, Style};

#[macro_export]
macro_rules! assert_hex_eq {
    ($left:expr, $right:expr$(,)?) => ({
        $crate::assert_hex_eq!(@ $left, $right, "", "");
    });
    ($left:expr, $right:expr, $($arg:tt)*) => ({
        $crate::assert_hex_eq!(@ $left, $right, ": ", $($arg)+);
    });
    (@ $left:expr, $right:expr, $maybe_semicolon:expr, $($arg:tt)*) => ({
        let to_diff = $crate::hex_diff::ToDiff { left: $left, right: $right };
        if (to_diff.left != to_diff.right) {
            ::std::panic!("assertion failed: `(left == right)`{}{}\
               \n\
               \n{}\
               \n",
               $maybe_semicolon,
               format_args!($($arg)*),
               to_diff,
            )
        }
    });
}

pub struct ToDiff<'a> {
    pub left: &'a [u8],
    pub right: &'a [u8],
}

impl std::fmt::Display for ToDiff<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write_diff(f, self.make_diff(), 4)
    }
}

enum DiffItem {
    Same(u8),
    Different(u8),
    Blank,
}

struct DiffResult {
    left: Vec<DiffItem>,
    right: Vec<DiffItem>,
}

impl ToDiff<'_> {
    fn make_diff(&self) -> DiffResult {
        let mut left = Vec::new();
        let mut right = Vec::new();

        for item in diff::slice(self.left, self.right) {
            match item {
                diff::Result::Both(byte, _) => {
                    while left.len() != right.len() {
                        if left.len() < right.len() {
                            left.push(DiffItem::Blank);
                        } else {
                            right.push(DiffItem::Blank);
                        }
                    }
                    left.push(DiffItem::Same(*byte));
                    right.push(DiffItem::Same(*byte));
                }
                diff::Result::Left(byte) => left.push(DiffItem::Different(*byte)),
                diff::Result::Right(byte) => right.push(DiffItem::Different(*byte)),
            }
        }

        while left.len() < right.len() {
            left.push(DiffItem::Blank);
        }
        while right.len() < left.len() {
            right.push(DiffItem::Blank);
        }

        DiffResult { left, right }
    }
}

fn write_diff(f: &mut impl fmt::Write, diff: DiffResult, width: usize) -> fmt::Result {
    let DiffResult { left, right } = diff;
    assert_eq!(left.len(), right.len());
    for (left, right) in left.chunks(width).zip(right.chunks(width)) {
        write!(f, " ")?;
        let mut writer = InlineWriter::new(f);
        for item in left {
            writer.write_item(item)?;
        }
        // pad out last line:
        for _ in 0..width - left.len() {
            writer.write_padding()?;
        }

        writer.write_with_style(&'|', Color::White.dimmed())?;
        writer.write_with_style(&' ', Color::White.dimmed())?;

        for item in right {
            writer.write_item(item)?;
        }

        for _ in 0..width - left.len() {
            writer.write_padding()?;
        }

        writer.finish()?;
    }
    Ok(())
}

/// Group character styling for an inline diff, to prevent wrapping each single
/// character in terminal styling codes.
///
/// Styles are applied automatically each time a new style is given in `write_with_style`.
struct InlineWriter<'a, W> {
    f: &'a mut W,
    style: Style,
}

impl<'a, W> InlineWriter<'a, W>
where
    W: fmt::Write,
{
    fn new(f: &'a mut W) -> Self {
        InlineWriter {
            f,
            style: Style::new(),
        }
    }

    /// Push a new character into the buffer, specifying the style it should be written in.
    fn write_with_style(&mut self, c: &char, style: Style) -> fmt::Result {
        // If the style is the same as previously, just write character
        if style == self.style {
            write!(self.f, "{c}")?;
        } else {
            // Close out previous style
            write!(self.f, "{}", self.style.suffix())?;

            // Store new style and start writing it
            write!(self.f, "{}{c}", style.prefix())?;
            self.style = style;
        }
        Ok(())
    }

    fn write_padding(&mut self) -> fmt::Result {
        write!(self.f, "   ")
    }

    fn write_item(&mut self, item: &DiffItem) -> fmt::Result {
        let style = match item {
            DiffItem::Same(_) | DiffItem::Blank => Style::default(),
            DiffItem::Different(_) => Red.into(),
        };

        let chars = match item {
            DiffItem::Same(val) | DiffItem::Different(val) => to_hex_digits(*val),
            DiffItem::Blank => ['_', '_'],
        };

        self.write_with_style(&chars[0], style)?;
        self.write_with_style(&chars[1], style)?;
        self.write_with_style(&' ', style)
    }

    /// Finish any existing style and reset to default state.
    fn finish(&mut self) -> fmt::Result {
        // Close out previous style
        writeln!(self.f, "{}", self.style.suffix())?;
        self.style = Default::default();
        Ok(())
    }
}

fn to_hex_digits(byte: u8) -> [char; 2] {
    let mut out = ['0', '0'];
    let first = byte & 0x0f;
    out[1] = to_hex(first);
    let second = (byte & 0xf0) >> 4;
    out[0] = to_hex(second);
    out
}

fn to_hex(int: u8) -> char {
    debug_assert!(int <= 15);
    if int < 10 {
        (b'0' + int).into()
    } else {
        (b'a' + int - 10).into()
    }
}
