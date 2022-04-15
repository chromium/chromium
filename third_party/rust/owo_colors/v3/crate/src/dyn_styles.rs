use crate::{AnsiColors, Color, DynColor, DynColors};
use core::fmt;

#[cfg(doc)]
use crate::OwoColorize;

/// A runtime-configurable text effect for use with [`Style`]
#[allow(missing_docs)]
#[derive(Debug, Copy, Clone)]
pub enum Effect {
    Bold,
    Dimmed,
    Italic,
    Underline,
    Blink,
    BlinkFast,
    Reversed,
    Hidden,
    Strikethrough,
}

macro_rules! color_methods {
    ($(
        #[$fg_meta:meta] #[$bg_meta:meta] $color:ident $fg_method:ident $bg_method:ident
    ),* $(,)?) => {
        $(
            #[$fg_meta]
            #[must_use]
            pub fn $fg_method(mut self) -> Self {
                self.fg = Some(DynColors::Ansi(AnsiColors::$color));
                self
            }

            #[$fg_meta]
            #[must_use]
            pub fn $bg_method(mut self) -> Self {
                self.bg = Some(DynColors::Ansi(AnsiColors::$color));
                self
            }
         )*
    };
}

macro_rules! style_methods {
    ($(#[$meta:meta] ($name:ident, $set_name:ident)),* $(,)?) => {
        $(
            #[$meta]
            #[must_use]
            pub fn $name(mut self) -> Self {
                self.style_flags.$set_name(true);
                self
            }
        )*
    };
}

const _: () = (); // workaround for syntax highlighting bug

/// A wrapper type which applies a [`Style`] when displaying the inner type
pub struct Styled<T> {
    target: T,
    style: Style,
}

/// A pre-computed style that can be applied to a struct using [`OwoColorize::style`]. Its
/// interface mimicks that of [`OwoColorize`], but instead of chaining methods on your
/// object, you instead chain them on the `Style` object before applying it.
///
/// ```rust
/// use owo_colors::{OwoColorize, Style};
///
/// let my_style = Style::new()
///     .red()
///     .on_white()
///     .strikethrough();
///
/// println!("{}", "red text, white background, struck through".style(my_style));
/// ```
#[derive(Debug, Default, Copy, Clone, PartialEq)]
pub struct Style {
    fg: Option<DynColors>,
    bg: Option<DynColors>,
    bold: bool,
    style_flags: StyleFlags,
}

#[repr(transparent)]
#[derive(Debug, Default, Copy, Clone, PartialEq)]
struct StyleFlags(u8);

const DIMMED_SHIFT: u8 = 0;
const ITALIC_SHIFT: u8 = 1;
const UNDERLINE_SHIFT: u8 = 2;
const BLINK_SHIFT: u8 = 3;
const BLINK_FAST_SHIFT: u8 = 4;
const REVERSED_SHIFT: u8 = 5;
const HIDDEN_SHIFT: u8 = 6;
const STRIKETHROUGH_SHIFT: u8 = 7;

macro_rules! style_flags_methods {
    ($(($shift:ident, $name:ident, $set_name:ident)),* $(,)?) => {
        $(
            fn $name(&self) -> bool {
                ((self.0 >> $shift) & 1) != 0
            }

            fn $set_name(&mut self, $name: bool) {
                self.0 = (self.0 & !(1 << $shift)) | (($name as u8) << $shift);
            }
        )*
    };
}

impl StyleFlags {
    style_flags_methods! {
        (DIMMED_SHIFT, dimmed, set_dimmed),
        (ITALIC_SHIFT, italic, set_italic),
        (UNDERLINE_SHIFT, underline, set_underline),
        (BLINK_SHIFT, blink, set_blink),
        (BLINK_FAST_SHIFT, blink_fast, set_blink_fast),
        (REVERSED_SHIFT, reversed, set_reversed),
        (HIDDEN_SHIFT, hidden, set_hidden),
        (STRIKETHROUGH_SHIFT, strikethrough, set_strikethrough),
    }
}

impl Style {
    /// Create a new style to be applied later
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Apply the style to a given struct to output
    pub fn style<T>(&self, target: T) -> Styled<T> {
        Styled {
            target,
            style: *self,
        }
    }

    /// Set the foreground color generically
    ///
    /// ```rust
    /// use owo_colors::{OwoColorize, colors::*};
    ///
    /// println!("{}", "red foreground".fg::<Red>());
    /// ```
    #[must_use]
    pub fn fg<C: Color>(mut self) -> Self {
        self.fg = Some(C::into_dyncolors());
        self
    }

    /// Set the background color generically.
    ///
    /// ```rust
    /// use owo_colors::{OwoColorize, colors::*};
    ///
    /// println!("{}", "black background".bg::<Black>());
    /// ```
    #[must_use]
    pub fn bg<C: Color>(mut self) -> Self {
        self.bg = Some(C::into_dyncolors());
        self
    }

    /// Removes the foreground color from the style. Note that this does not apply
    /// the default color, but rather represents not changing the current terminal color.
    ///
    /// If you wish to actively change the terminal color back to the default, see
    /// [`Style::default_color`].
    #[must_use]
    pub fn remove_fg(mut self) -> Self {
        self.fg = None;
        self
    }

    /// Removes the background color from the style. Note that this does not apply
    /// the default color, but rather represents not changing the current terminal color.
    ///
    /// If you wish to actively change the terminal color back to the default, see
    /// [`Style::on_default_color`].
    #[must_use]
    pub fn remove_bg(mut self) -> Self {
        self.bg = None;
        self
    }

    color_methods! {
        /// Change the foreground color to black
        /// Change the background color to black
        Black    black    on_black,
        /// Change the foreground color to red
        /// Change the background color to red
        Red      red      on_red,
        /// Change the foreground color to green
        /// Change the background color to green
        Green    green    on_green,
        /// Change the foreground color to yellow
        /// Change the background color to yellow
        Yellow   yellow   on_yellow,
        /// Change the foreground color to blue
        /// Change the background color to blue
        Blue     blue     on_blue,
        /// Change the foreground color to magenta
        /// Change the background color to magenta
        Magenta  magenta  on_magenta,
        /// Change the foreground color to purple
        /// Change the background color to purple
        Magenta  purple   on_purple,
        /// Change the foreground color to cyan
        /// Change the background color to cyan
        Cyan     cyan     on_cyan,
        /// Change the foreground color to white
        /// Change the background color to white
        White    white    on_white,

        /// Change the foreground color to the terminal default
        /// Change the background color to the terminal default
        Default default_color on_default_color,

        /// Change the foreground color to bright black
        /// Change the background color to bright black
        BrightBlack    bright_black    on_bright_black,
        /// Change the foreground color to bright red
        /// Change the background color to bright red
        BrightRed      bright_red      on_bright_red,
        /// Change the foreground color to bright green
        /// Change the background color to bright green
        BrightGreen    bright_green    on_bright_green,
        /// Change the foreground color to bright yellow
        /// Change the background color to bright yellow
        BrightYellow   bright_yellow   on_bright_yellow,
        /// Change the foreground color to bright blue
        /// Change the background color to bright blue
        BrightBlue     bright_blue     on_bright_blue,
        /// Change the foreground color to bright magenta
        /// Change the background color to bright magenta
        BrightMagenta  bright_magenta  on_bright_magenta,
        /// Change the foreground color to bright purple
        /// Change the background color to bright purple
        BrightMagenta  bright_purple   on_bright_purple,
        /// Change the foreground color to bright cyan
        /// Change the background color to bright cyan
        BrightCyan     bright_cyan     on_bright_cyan,
        /// Change the foreground color to bright white
        /// Change the background color to bright white
        BrightWhite    bright_white    on_bright_white,
    }

    /// Make the text bold
    #[must_use]
    pub fn bold(mut self) -> Self {
        self.bold = true;
        self
    }

    style_methods! {
        /// Make the text dim
        (dimmed, set_dimmed),
        /// Make the text italicized
        (italic, set_italic),
        /// Make the text italicized
        (underline, set_underline),
        /// Make the text blink
        (blink, set_blink),
        /// Make the text blink (but fast!)
        (blink_fast, set_blink_fast),
        /// Swap the foreground and background colors
        (reversed, set_reversed),
        /// Hide the text
        (hidden, set_hidden),
        /// Cross out the text
        (strikethrough, set_strikethrough),
    }

    fn set_effect(&mut self, effect: Effect, to: bool) {
        use Effect::*;
        match effect {
            Bold => self.bold = to,
            Dimmed => self.style_flags.set_dimmed(to),
            Italic => self.style_flags.set_italic(to),
            Underline => self.style_flags.set_underline(to),
            Blink => self.style_flags.set_blink(to),
            BlinkFast => self.style_flags.set_blink_fast(to),
            Reversed => self.style_flags.set_reversed(to),
            Hidden => self.style_flags.set_hidden(to),
            Strikethrough => self.style_flags.set_strikethrough(to),
        }
    }

    fn set_effects(&mut self, effects: &[Effect], to: bool) {
        for e in effects {
            self.set_effect(*e, to)
        }
    }

    /// Apply a given effect from the style
    #[must_use]
    pub fn effect(mut self, effect: Effect) -> Self {
        self.set_effect(effect, true);
        self
    }

    /// Remove a given effect from the style
    #[must_use]
    pub fn remove_effect(mut self, effect: Effect) -> Self {
        self.set_effect(effect, false);
        self
    }

    /// Apply a given set of effects to the style
    #[must_use]
    pub fn effects(mut self, effects: &[Effect]) -> Self {
        self.set_effects(effects, true);
        self
    }

    /// Remove a given set of effects from the style
    #[must_use]
    pub fn remove_effects(mut self, effects: &[Effect]) -> Self {
        self.set_effects(effects, false);
        self
    }

    /// Disables all the given effects from the style
    #[must_use]
    pub fn remove_all_effects(mut self) -> Self {
        self.bold = false;
        self.style_flags = StyleFlags::default();
        self
    }

    /// Set the foreground color at runtime. Only use if you do not know which color will be used at
    /// compile-time. If the color is constant, use either [`OwoColorize::fg`](crate::OwoColorize::fg) or
    /// a color-specific method, such as [`OwoColorize::green`](crate::OwoColorize::green),
    ///
    /// ```rust
    /// use owo_colors::{OwoColorize, AnsiColors};
    ///
    /// println!("{}", "green".color(AnsiColors::Green));
    /// ```
    #[must_use]
    pub fn color<Color: DynColor>(mut self, color: Color) -> Self {
        self.fg = Some(color.get_dyncolors_fg());
        self
    }

    /// Set the background color at runtime. Only use if you do not know what color to use at
    /// compile-time. If the color is constant, use either [`OwoColorize::bg`](crate::OwoColorize::bg) or
    /// a color-specific method, such as [`OwoColorize::on_yellow`](crate::OwoColorize::on_yellow),
    ///
    /// ```rust
    /// use owo_colors::{OwoColorize, AnsiColors};
    ///
    /// println!("{}", "yellow background".on_color(AnsiColors::BrightYellow));
    /// ```
    #[must_use]
    pub fn on_color<Color: DynColor>(mut self, color: Color) -> Self {
        self.bg = Some(color.get_dyncolors_bg());
        self
    }

    /// Set the foreground color to a specific RGB value.
    #[must_use]
    pub fn fg_rgb<const R: u8, const G: u8, const B: u8>(mut self) -> Self {
        self.fg = Some(DynColors::Rgb(R, G, B));

        self
    }

    /// Set the background color to a specific RGB value.
    #[must_use]
    pub fn bg_rgb<const R: u8, const G: u8, const B: u8>(mut self) -> Self {
        self.bg = Some(DynColors::Rgb(R, G, B));

        self
    }

    /// Sets the foreground color to an RGB value.
    #[must_use]
    pub fn truecolor(mut self, r: u8, g: u8, b: u8) -> Self {
        self.fg = Some(DynColors::Rgb(r, g, b));
        self
    }

    /// Sets the background color to an RGB value.
    #[must_use]
    pub fn on_truecolor(mut self, r: u8, g: u8, b: u8) -> Self {
        self.bg = Some(DynColors::Rgb(r, g, b));
        self
    }
}

/// Helper to create [`Style`]s more ergonomically
pub fn style() -> Style {
    Style::new()
}

macro_rules! text_effect_fmt {
    ($style:ident, $formatter:ident, $semicolon:ident, $(($attr:ident, $value:literal)),* $(,)?) => {
        $(
            if $style.style_flags.$attr() {
                if $semicolon {
                    $formatter.write_str(";")?;
                }
                $formatter.write_str($value)?;

                $semicolon = true;
            }
        )+
    }
}

macro_rules! impl_fmt {
    ($($trait:path),* $(,)?) => {
        $(
            impl<T: $trait> $trait for Styled<T> {
                #[allow(unused_assignments)]
                fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {

                    let s = &self.style;
                    let format_less_important_effects = s.style_flags != StyleFlags::default();
                    let format_effect = s.bold || format_less_important_effects;
                    let format_color = s.fg.is_some() || s.bg.is_some();
                    let format_any = format_color || format_effect;

                    let mut semicolon = false;

                    if format_any {
                        f.write_str("\x1b[")?;
                    }

                    if let Some(fg) = s.fg {
                        <DynColors as DynColor>::fmt_raw_ansi_fg(&fg, f)?;
                        semicolon = true;
                    }

                    if let Some(bg) = s.bg {
                        if s.fg.is_some() {
                            f.write_str(";")?;
                        }
                        <DynColors as DynColor>::fmt_raw_ansi_bg(&bg, f)?;
                    }

                    if format_effect {
                        if s.bold {
                            if semicolon {
                                f.write_str(";")?;
                            }

                            f.write_str("1")?;

                            semicolon = true;
                        }

                        if format_less_important_effects {
                            text_effect_fmt!{
                                s, f, semicolon,
                                (dimmed,        "2"),
                                (italic,        "3"),
                                (underline,     "4"),
                                (blink,         "5"),
                                (blink_fast,    "6"),
                                (reversed,      "7"),
                                (hidden,        "8"),
                                (strikethrough, "9"),
                            }
                        }
                    }

                    if format_any {
                        f.write_str("m")?;
                    }

                    <T as $trait>::fmt(&self.target, f)?;

                    if format_any {
                        f.write_str("\x1b[0m")?;
                    }

                    Ok(())
                }
            }
        )*
    };
}

impl_fmt! {
    fmt::Display,
    fmt::Debug,
    fmt::UpperHex,
    fmt::LowerHex,
    fmt::Binary,
    fmt::UpperExp,
    fmt::LowerExp,
    fmt::Octal,
    fmt::Pointer,
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{AnsiColors, OwoColorize};

    #[test]
    fn test_it() {
        let style = Style::new()
            .bright_white()
            .on_blue()
            .bold()
            .dimmed()
            .italic()
            .underline()
            .blink()
            //.blink_fast()
            //.reversed()
            //.hidden()
            .strikethrough();
        let s = style.style("TEST");
        let s2 = format!("{}", &s);
        println!("{}", &s2);
        assert_eq!(&s2, "\u{1b}[97;44;1;2;3;4;5;9mTEST\u{1b}[0m");
    }

    #[test]
    fn test_effects() {
        use Effect::*;
        let style = Style::new().effects(&[Strikethrough, Underline]);

        let s = style.style("TEST");
        let s2 = format!("{}", &s);
        println!("{}", &s2);
        assert_eq!(&s2, "\u{1b}[4;9mTEST\u{1b}[0m");
    }

    #[test]
    fn test_color() {
        let style = Style::new()
            .color(AnsiColors::White)
            .on_color(AnsiColors::Black);

        let s = style.style("TEST");
        let s2 = format!("{}", &s);
        println!("{}", &s2);
        assert_eq!(&s2, "\u{1b}[37;40mTEST\u{1b}[0m");
    }

    #[test]
    fn test_truecolor() {
        let style = Style::new().truecolor(255, 255, 255).on_truecolor(0, 0, 0);

        let s = style.style("TEST");
        let s2 = format!("{}", &s);
        println!("{}", &s2);
        assert_eq!(&s2, "\u{1b}[38;2;255;255;255;48;2;0;0;0mTEST\u{1b}[0m");
    }

    #[test]
    fn test_string_reference() {
        let style = Style::new().truecolor(255, 255, 255).on_truecolor(0, 0, 0);

        let string = String::from("TEST");
        let s = style.style(&string);
        let s2 = format!("{}", &s);
        println!("{}", &s2);
        assert_eq!(&s2, "\u{1b}[38;2;255;255;255;48;2;0;0;0mTEST\u{1b}[0m");
    }

    #[test]
    fn test_owocolorize() {
        let style = Style::new().bright_white().on_blue();

        let s = "TEST".style(style);
        let s2 = format!("{}", &s);
        println!("{}", &s2);
        assert_eq!(&s2, "\u{1b}[97;44mTEST\u{1b}[0m");
    }
}
