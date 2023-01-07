//! This crate provides [`OwoColorize`](OwoColorize), an extension trait for colorizing a
//! given type.
//!
//! ## Example
//!
//! ```rust
//! use owo_colors::OwoColorize;
//!
//! fn main() {
//!     // Foreground colors
//!     println!("My number is {:#x}!", 10.green());
//!     // Background colors
//!     println!("My number is not {}!", 4.on_red());
//! }
//! ```
//!
//! ## Generically color
//!
//! ```rust
//! use owo_colors::OwoColorize;
//! use owo_colors::colors::*;
//!
//! fn main() {
//!     // Generically color
//!     println!("My number might be {}!", 4.fg::<Black>().bg::<Yellow>());
//! }
//! ```
//!
//! ## Stylize
//!
//! ```rust
//! use owo_colors::OwoColorize;
//!
//! println!("{}", "strikethrough".strikethrough());
//! ```
//!
//! ## Only Style on Supported Terminals
//!
//! ```rust
//! # #[cfg(feature = "supports-color")] {
//! use owo_colors::{OwoColorize, Stream::Stdout};
//!
//! println!(
//!     "{}",
//!     "colored blue if a supported terminal"
//!         .if_supports_color(Stdout, |text| text.bright_blue())
//! );
//! # }
//! ```
//!
//! Supports `NO_COLOR`/`FORCE_COLOR` environment variables, checks if it's a tty, checks
//! if it's running in CI (and thus likely supports color), and checks which terminal is being
//! used. (Note: requires `supports-colors` feature)
//!
//! ## Style Objects
//!
//! owo-colors also features the ability to create a [`Style`] object and use it to
//! apply the same set of colors/effects to any number of things to display.
//!
//! ```rust
//! use owo_colors::{OwoColorize, Style};
//!
//! let my_style = Style::new()
//!     .red()
//!     .on_white()
//!     .strikethrough();
//!
//! let text = "red text, white background, struck through";
//! println!("{}", text.style(my_style));
//! ```
#![cfg_attr(not(test), no_std)]
#![cfg_attr(doc_cfg, feature(doc_cfg))]
#![doc(html_logo_url = "https://jam1.re/img/rust_owo.svg")]
#![warn(missing_docs)]

pub mod colors;
mod combo;
mod dyn_colors;
mod dyn_styles;
pub mod styles;

#[cfg(feature = "supports-colors")]
mod overrides;

#[cfg(feature = "supports-colors")]
pub(crate) use overrides::OVERRIDE;

use core::fmt;
use core::marker::PhantomData;

/// A trait for describing a type which can be used with [`FgColorDisplay`](FgColorDisplay) or
/// [`BgCBgColorDisplay`](BgColorDisplay)
pub trait Color {
    /// The ANSI format code for setting this color as the foreground
    const ANSI_FG: &'static str;

    /// The ANSI format code for setting this color as the background
    const ANSI_BG: &'static str;

    /// The raw ANSI format for settings this color as the foreground without the ANSI
    /// delimiters ("\x1b" and "m")
    const RAW_ANSI_FG: &'static str;

    /// The raw ANSI format for settings this color as the background without the ANSI
    /// delimiters ("\x1b" and "m")
    const RAW_ANSI_BG: &'static str;

    #[doc(hidden)]
    type DynEquivelant: DynColor;

    #[doc(hidden)]
    const DYN_EQUIVELANT: Self::DynEquivelant;

    #[doc(hidden)]
    fn into_dyncolors() -> crate::DynColors;
}

/// A trait describing a runtime-configurable color which can displayed using [`FgDynColorDisplay`](FgDynColorDisplay)
/// or [`BgDynColorDisplay`](BgDynColorDisplay). If your color will be known at compile time it
/// is recommended you avoid this.
pub trait DynColor {
    /// A function to output a ANSI code to a formatter to set the foreground to this color
    fn fmt_ansi_fg(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result;
    /// A function to output a ANSI code to a formatter to set the background to this color
    fn fmt_ansi_bg(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result;

    /// A function to output a raw ANSI code to a formatter to set the foreground to this color,
    /// but without including the ANSI delimiters.
    fn fmt_raw_ansi_fg(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result;

    /// A function to output a raw ANSI code to a formatter to set the background to this color,
    /// but without including the ANSI delimiters.
    fn fmt_raw_ansi_bg(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result;

    #[doc(hidden)]
    fn get_dyncolors_fg(&self) -> DynColors;
    #[doc(hidden)]
    fn get_dyncolors_bg(&self) -> DynColors;
}

/// Transparent wrapper around a type which implements all the formatters the wrapped type does,
/// with the addition of changing the foreground color. Recommended to be constructed using
/// [`OwoColorize`](OwoColorize).
#[repr(transparent)]
pub struct FgColorDisplay<'a, C: Color, T>(&'a T, PhantomData<C>);

/// Transparent wrapper around a type which implements all the formatters the wrapped type does,
/// with the addition of changing the background color. Recommended to be constructed using
/// [`OwoColorize`](OwoColorize).
#[repr(transparent)]
pub struct BgColorDisplay<'a, C: Color, T>(&'a T, PhantomData<C>);

/// Wrapper around a type which implements all the formatters the wrapped type does,
/// with the addition of changing the foreground color. Is not recommended unless compile-time
/// coloring is not an option.
pub struct FgDynColorDisplay<'a, Color: DynColor, T>(&'a T, Color);

/// Wrapper around a type which implements all the formatters the wrapped type does,
/// with the addition of changing the background color. Is not recommended unless compile-time
/// coloring is not an option.
pub struct BgDynColorDisplay<'a, Color: DynColor, T>(&'a T, Color);

macro_rules! style_methods {
    ($(#[$meta:meta] $name:ident $ty:ident),* $(,)?) => {
        $(
            #[$meta]
            #[must_use]
            #[inline(always)]
            fn $name<'a>(&'a self) -> styles::$ty<'a, Self> {
                styles::$ty(self)
            }
         )*
    };
}

const _: () = (); // workaround for syntax highlighting bug

macro_rules! color_methods {
    ($(
        #[$fg_meta:meta] #[$bg_meta:meta] $color:ident $fg_method:ident $bg_method:ident
    ),* $(,)?) => {
        $(
            #[$fg_meta]
            #[must_use]
            #[inline(always)]
            fn $fg_method<'a>(&'a self) -> FgColorDisplay<'a, colors::$color, Self> {
                FgColorDisplay(self, PhantomData)
            }

            #[$bg_meta]
            #[must_use]
            #[inline(always)]
            fn $bg_method<'a>(&'a self) -> BgColorDisplay<'a, colors::$color, Self> {
                BgColorDisplay(self, PhantomData)
            }
         )*
    };
}

const _: () = (); // workaround for syntax highlighting bug

/// Extension trait for colorizing a type which implements any std formatter
/// ([`Display`](core::fmt::Display), [`Debug`](core::fmt::Debug), [`UpperHex`](core::fmt::UpperHex),
/// etc.)
///
/// ## Example
///
/// ```rust
/// use owo_colors::OwoColorize;
///
/// println!("My number is {:#x}!", 10.green());
/// println!("My number is not {}!", 4.on_red());
/// ```
///
/// ## How to decide which method to use
///
/// **Do you have a specific color you want to use?**
///
/// Use the specific color's method, such as [`blue`](OwoColorize::blue) or
/// [`on_green`](OwoColorize::on_green).
///
///
/// **Do you want your colors configurable via generics?**
///
/// Use [`fg`](OwoColorize::fg) and [`bg`](OwoColorize::bg) to make it compile-time configurable.
///
///
/// **Do you need to pick a color at runtime?**
///
/// Use the [`color`](OwoColorize::color), [`on_color`](OwoColorize::on_color),
/// [`truecolor`](OwoColorize::truecolor) or [`on_truecolor`](OwoColorize::on_truecolor).
///
/// **Do you need some other text modifier?**
///
/// * [`bold`](OwoColorize::bold)
/// * [`dimmed`](OwoColorize::dimmed)
/// * [`italic`](OwoColorize::italic)
/// * [`underline`](OwoColorize::underline)
/// * [`blink`](OwoColorize::blink)
/// * [`blink_fast`](OwoColorize::blink_fast)
/// * [`reversed`](OwoColorize::reversed)
/// * [`hidden`](OwoColorize::hidden)
/// * [`strikethrough`](OwoColorize::strikethrough)
///
/// **Do you want it to only display colors if it's a terminal?**
///
/// 1. Enable the `supports-colors` feature
/// 2. Colorize inside [`if_supports_color`](OwoColorize::if_supports_color)
///
/// **Do you need to store a set of colors/effects to apply to multiple things?**
///
/// Use [`style`](OwoColorize::style) to apply a [`Style`]
///
pub trait OwoColorize: Sized {
    /// Set the foreground color generically
    ///
    /// ```rust
    /// use owo_colors::{OwoColorize, colors::*};
    ///
    /// println!("{}", "red foreground".fg::<Red>());
    /// ```
    #[must_use]
    #[inline(always)]
    fn fg<C: Color>(&self) -> FgColorDisplay<'_, C, Self> {
        FgColorDisplay(self, PhantomData)
    }

    /// Set the background color generically.
    ///
    /// ```rust
    /// use owo_colors::{OwoColorize, colors::*};
    ///
    /// println!("{}", "black background".bg::<Black>());
    /// ```
    #[must_use]
    #[inline(always)]
    fn bg<C: Color>(&self) -> BgColorDisplay<'_, C, Self> {
        BgColorDisplay(self, PhantomData)
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

    style_methods! {
        /// Make the text bold
        bold BoldDisplay,
        /// Make the text dim
        dimmed DimDisplay,
        /// Make the text italicized
        italic ItalicDisplay,
        /// Make the text italicized
        underline UnderlineDisplay,
        /// Make the text blink
        blink BlinkDisplay,
        /// Make the text blink (but fast!)
        blink_fast BlinkFastDisplay,
        /// Swap the foreground and background colors
        reversed ReversedDisplay,
        /// Hide the text
        hidden HiddenDisplay,
        /// Cross out the text
        strikethrough StrikeThroughDisplay,
    }

    /// Set the foreground color at runtime. Only use if you do not know which color will be used at
    /// compile-time. If the color is constant, use either [`OwoColorize::fg`](OwoColorize::fg) or
    /// a color-specific method, such as [`OwoColorize::green`](OwoColorize::green),
    ///
    /// ```rust
    /// use owo_colors::{OwoColorize, AnsiColors};
    ///
    /// println!("{}", "green".color(AnsiColors::Green));
    /// ```
    #[must_use]
    #[inline(always)]
    fn color<Color: DynColor>(&self, color: Color) -> FgDynColorDisplay<'_, Color, Self> {
        FgDynColorDisplay(self, color)
    }

    /// Set the background color at runtime. Only use if you do not know what color to use at
    /// compile-time. If the color is constant, use either [`OwoColorize::bg`](OwoColorize::bg) or
    /// a color-specific method, such as [`OwoColorize::on_yellow`](OwoColorize::on_yellow),
    ///
    /// ```rust
    /// use owo_colors::{OwoColorize, AnsiColors};
    ///
    /// println!("{}", "yellow background".on_color(AnsiColors::BrightYellow));
    /// ```
    #[must_use]
    #[inline(always)]
    fn on_color<Color: DynColor>(&self, color: Color) -> BgDynColorDisplay<'_, Color, Self> {
        BgDynColorDisplay(self, color)
    }

    /// Set the foreground color to a specific RGB value.
    #[must_use]
    fn fg_rgb<const R: u8, const G: u8, const B: u8>(
        &self,
    ) -> FgColorDisplay<'_, colors::CustomColor<R, G, B>, Self> {
        FgColorDisplay(self, PhantomData)
    }

    /// Set the background color to a specific RGB value.
    #[must_use]
    fn bg_rgb<const R: u8, const G: u8, const B: u8>(
        &self,
    ) -> BgColorDisplay<'_, colors::CustomColor<R, G, B>, Self> {
        BgColorDisplay(self, PhantomData)
    }

    /// Sets the foreground color to an RGB value.
    #[must_use]
    #[inline(always)]
    fn truecolor(&self, r: u8, g: u8, b: u8) -> FgDynColorDisplay<'_, Rgb, Self> {
        FgDynColorDisplay(self, Rgb(r, g, b))
    }

    /// Sets the background color to an RGB value.
    #[must_use]
    #[inline(always)]
    fn on_truecolor(&self, r: u8, g: u8, b: u8) -> BgDynColorDisplay<'_, Rgb, Self> {
        BgDynColorDisplay(self, Rgb(r, g, b))
    }

    /// Apply a runtime-determined style
    #[must_use]
    fn style(&self, style: Style) -> Styled<&Self> {
        style.style(self)
    }

    /// Apply a given transformation function to all formatters if the given stream
    /// supports at least basic ANSI colors, allowing you to conditionally apply
    /// given styles/colors.
    ///
    /// Requires the `supports-colors` feature.
    ///
    /// ```rust
    /// use owo_colors::{OwoColorize, Stream};
    ///
    /// println!(
    ///     "{}",
    ///     "woah! error! if this terminal supports colors, it's blue"
    ///         .if_supports_color(Stream::Stdout, |text| text.bright_blue())
    /// );
    /// ```
    #[must_use]
    #[cfg(feature = "supports-colors")]
    fn if_supports_color<'a, Out, ApplyFn>(
        &'a self,
        stream: Stream,
        apply: ApplyFn,
    ) -> SupportsColorsDisplay<'a, Self, Out, ApplyFn>
    where
        ApplyFn: Fn(&'a Self) -> Out,
    {
        SupportsColorsDisplay(self, apply, stream)
    }
}

#[cfg(feature = "supports-colors")]
mod supports_colors;

#[cfg(feature = "supports-colors")]
pub use {
    overrides::{set_override, unset_override},
    supports_color::Stream,
    supports_colors::SupportsColorsDisplay,
};

pub use colors::{
    ansi_colors::AnsiColors, css::dynamic::CssColors, dynamic::Rgb, xterm::dynamic::XtermColors,
};

// TODO: figure out some wait to only implement for fmt::Display | fmt::Debug | ...
impl<D: Sized> OwoColorize for D {}

pub use {combo::ComboColorDisplay, dyn_colors::*, dyn_styles::*};

/// Module for drop-in [`colored`](https://docs.rs/colored) support to aid in porting code from
/// [`colored`](https://docs.rs/colored) to owo-colors.
///
/// Just replace:
///
/// ```rust
/// # mod colored {}
/// use colored::*;
/// ```
///
/// with
///
/// ```rust
/// use owo_colors::colored::*;
/// ```
pub mod colored {
    pub use crate::AnsiColors as Color;
    pub use crate::OwoColorize;

    /// A couple of functions to enable and disable coloring similarly to `colored`
    #[cfg(feature = "supports-colors")]
    pub mod control {
        pub use crate::{set_override, unset_override};
    }
}

#[cfg(test)]
mod tests;
