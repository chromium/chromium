//! Different display styles (strikethrough, bold, etc.)
use core::fmt;

#[allow(unused_imports)]
use crate::OwoColorize;

macro_rules! impl_fmt_for_style {
    ($(($ty:ident, $trait:path, $ansi:literal)),* $(,)?) => {
        $(
            impl<'a, T: $trait> $trait for $ty<'a, T> {
                fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                    f.write_str($ansi)?;
                    <_ as $trait>::fmt(&self.0, f)?;
                    f.write_str("\x1b[0m")
                }
            }
        )*
    };
}

/// Transparent wrapper around a type which implements all the formatters the wrapped type does,
/// with the addition of boldening it. Recommended to be constructed using
/// [`OwoColorize`](OwoColorize::bold).
#[repr(transparent)]
pub struct BoldDisplay<'a, T>(pub &'a T);

/// Transparent wrapper around a type which implements all the formatters the wrapped type does,
/// with the addition of dimming it. Recommended to be constructed using
/// [`OwoColorize`](OwoColorize::dimmed).
#[repr(transparent)]
pub struct DimDisplay<'a, T>(pub &'a T);

/// Transparent wrapper around a type which implements all the formatters the wrapped type does,
/// with the addition of italicizing it. Recommended to be constructed using
/// [`OwoColorize`](OwoColorize::italic).
#[repr(transparent)]
pub struct ItalicDisplay<'a, T>(pub &'a T);

/// Transparent wrapper around a type which implements all the formatters the wrapped type does,
/// with the addition of underlining it. Recommended to be constructed using
/// [`OwoColorize`](OwoColorize::underline).
#[repr(transparent)]
pub struct UnderlineDisplay<'a, T>(pub &'a T);

/// Transparent wrapper around a type which implements all the formatters the wrapped type does,
/// with the addition of making it blink. Recommended to be constructed using
/// [`OwoColorize`](OwoColorize::blink).
#[repr(transparent)]
pub struct BlinkDisplay<'a, T>(pub &'a T);

/// Transparent wrapper around a type which implements all the formatters the wrapped type does,
/// with the addition of making it blink fast. Recommended to be constructed using
/// [`OwoColorize`](OwoColorize::blink_fast).
#[repr(transparent)]
pub struct BlinkFastDisplay<'a, T>(pub &'a T);

/// Transparent wrapper around a type which implements all the formatters the wrapped type does,
/// with the addition of swapping foreground and background colors. Recommended to be constructed
/// using [`OwoColorize`](OwoColorize::reversed).
#[repr(transparent)]
pub struct ReversedDisplay<'a, T>(pub &'a T);

/// Transparent wrapper around a type which implements all the formatters the wrapped type does,
/// with the addition of hiding the text. Recommended to be constructed
/// using [`OwoColorize`](OwoColorize::reversed).
#[repr(transparent)]
pub struct HiddenDisplay<'a, T>(pub &'a T);

/// Transparent wrapper around a type which implements all the formatters the wrapped type does,
/// with the addition of crossing out the given text. Recommended to be constructed using
/// [`OwoColorize`](OwoColorize::strikethrough).
#[repr(transparent)]
pub struct StrikeThroughDisplay<'a, T>(pub &'a T);

impl_fmt_for_style! {
    // Bold
    (BoldDisplay, fmt::Display,  "\x1b[1m"),
    (BoldDisplay, fmt::Debug,    "\x1b[1m"),
    (BoldDisplay, fmt::UpperHex, "\x1b[1m"),
    (BoldDisplay, fmt::LowerHex, "\x1b[1m"),
    (BoldDisplay, fmt::Binary,   "\x1b[1m"),
    (BoldDisplay, fmt::UpperExp, "\x1b[1m"),
    (BoldDisplay, fmt::LowerExp, "\x1b[1m"),
    (BoldDisplay, fmt::Octal,    "\x1b[1m"),
    (BoldDisplay, fmt::Pointer,  "\x1b[1m"),

    // Dim
    (DimDisplay, fmt::Display,  "\x1b[2m"),
    (DimDisplay, fmt::Debug,    "\x1b[2m"),
    (DimDisplay, fmt::UpperHex, "\x1b[2m"),
    (DimDisplay, fmt::LowerHex, "\x1b[2m"),
    (DimDisplay, fmt::Binary,   "\x1b[2m"),
    (DimDisplay, fmt::UpperExp, "\x1b[2m"),
    (DimDisplay, fmt::LowerExp, "\x1b[2m"),
    (DimDisplay, fmt::Octal,    "\x1b[2m"),
    (DimDisplay, fmt::Pointer,  "\x1b[2m"),

    // Italic
    (ItalicDisplay, fmt::Display,  "\x1b[3m"),
    (ItalicDisplay, fmt::Debug,    "\x1b[3m"),
    (ItalicDisplay, fmt::UpperHex, "\x1b[3m"),
    (ItalicDisplay, fmt::LowerHex, "\x1b[3m"),
    (ItalicDisplay, fmt::Binary,   "\x1b[3m"),
    (ItalicDisplay, fmt::UpperExp, "\x1b[3m"),
    (ItalicDisplay, fmt::LowerExp, "\x1b[3m"),
    (ItalicDisplay, fmt::Octal,    "\x1b[3m"),
    (ItalicDisplay, fmt::Pointer,  "\x1b[3m"),

    // Underline
    (UnderlineDisplay, fmt::Display,  "\x1b[4m"),
    (UnderlineDisplay, fmt::Debug,    "\x1b[4m"),
    (UnderlineDisplay, fmt::UpperHex, "\x1b[4m"),
    (UnderlineDisplay, fmt::LowerHex, "\x1b[4m"),
    (UnderlineDisplay, fmt::Binary,   "\x1b[4m"),
    (UnderlineDisplay, fmt::UpperExp, "\x1b[4m"),
    (UnderlineDisplay, fmt::LowerExp, "\x1b[4m"),
    (UnderlineDisplay, fmt::Octal,    "\x1b[4m"),
    (UnderlineDisplay, fmt::Pointer,  "\x1b[4m"),

    // Blink
    (BlinkDisplay, fmt::Display,  "\x1b[5m"),
    (BlinkDisplay, fmt::Debug,    "\x1b[5m"),
    (BlinkDisplay, fmt::UpperHex, "\x1b[5m"),
    (BlinkDisplay, fmt::LowerHex, "\x1b[5m"),
    (BlinkDisplay, fmt::Binary,   "\x1b[5m"),
    (BlinkDisplay, fmt::UpperExp, "\x1b[5m"),
    (BlinkDisplay, fmt::LowerExp, "\x1b[5m"),
    (BlinkDisplay, fmt::Octal,    "\x1b[5m"),
    (BlinkDisplay, fmt::Pointer,  "\x1b[5m"),

    // Blink fast
    (BlinkFastDisplay, fmt::Display,  "\x1b[6m"),
    (BlinkFastDisplay, fmt::Debug,    "\x1b[6m"),
    (BlinkFastDisplay, fmt::UpperHex, "\x1b[6m"),
    (BlinkFastDisplay, fmt::LowerHex, "\x1b[6m"),
    (BlinkFastDisplay, fmt::Binary,   "\x1b[6m"),
    (BlinkFastDisplay, fmt::UpperExp, "\x1b[6m"),
    (BlinkFastDisplay, fmt::LowerExp, "\x1b[6m"),
    (BlinkFastDisplay, fmt::Octal,    "\x1b[6m"),
    (BlinkFastDisplay, fmt::Pointer,  "\x1b[6m"),

    // Reverse video
    (ReversedDisplay, fmt::Display,  "\x1b[7m"),
    (ReversedDisplay, fmt::Debug,    "\x1b[7m"),
    (ReversedDisplay, fmt::UpperHex, "\x1b[7m"),
    (ReversedDisplay, fmt::LowerHex, "\x1b[7m"),
    (ReversedDisplay, fmt::Binary,   "\x1b[7m"),
    (ReversedDisplay, fmt::UpperExp, "\x1b[7m"),
    (ReversedDisplay, fmt::LowerExp, "\x1b[7m"),
    (ReversedDisplay, fmt::Octal,    "\x1b[7m"),
    (ReversedDisplay, fmt::Pointer,  "\x1b[7m"),

    // Hide the text
    (HiddenDisplay, fmt::Display,  "\x1b[8m"),
    (HiddenDisplay, fmt::Debug,    "\x1b[8m"),
    (HiddenDisplay, fmt::UpperHex, "\x1b[8m"),
    (HiddenDisplay, fmt::LowerHex, "\x1b[8m"),
    (HiddenDisplay, fmt::Binary,   "\x1b[8m"),
    (HiddenDisplay, fmt::UpperExp, "\x1b[8m"),
    (HiddenDisplay, fmt::LowerExp, "\x1b[8m"),
    (HiddenDisplay, fmt::Octal,    "\x1b[8m"),
    (HiddenDisplay, fmt::Pointer,  "\x1b[8m"),

    // StrikeThrough
    (StrikeThroughDisplay, fmt::Display,  "\x1b[9m"),
    (StrikeThroughDisplay, fmt::Debug,    "\x1b[9m"),
    (StrikeThroughDisplay, fmt::UpperHex, "\x1b[9m"),
    (StrikeThroughDisplay, fmt::LowerHex, "\x1b[9m"),
    (StrikeThroughDisplay, fmt::Binary,   "\x1b[9m"),
    (StrikeThroughDisplay, fmt::UpperExp, "\x1b[9m"),
    (StrikeThroughDisplay, fmt::LowerExp, "\x1b[9m"),
    (StrikeThroughDisplay, fmt::Octal,    "\x1b[9m"),
    (StrikeThroughDisplay, fmt::Pointer,  "\x1b[9m"),
}
