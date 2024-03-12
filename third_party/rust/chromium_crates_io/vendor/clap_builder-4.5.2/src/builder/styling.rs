//! Terminal [`Styles`] for help and error output

pub use anstyle::*;

/// Terminal styling definitions
///
/// See also [`Command::styles`][crate::Command::styles].
///
/// # Example
///
/// clap v3 styling
/// ```rust
/// # use clap_builder as clap;
/// # use clap::builder::styling::*;
/// let styles = Styles::styled()
///     .header(AnsiColor::Yellow.on_default())
///     .usage(AnsiColor::Green.on_default())
///     .literal(AnsiColor::Green.on_default())
///     .placeholder(AnsiColor::Green.on_default());
/// ```
#[derive(Clone, Debug)]
#[allow(missing_copy_implementations)] // Large enough type that I want an explicit `clone()` for now
pub struct Styles {
    header: anstyle::Style,
    error: anstyle::Style,
    usage: anstyle::Style,
    literal: anstyle::Style,
    placeholder: anstyle::Style,
    valid: anstyle::Style,
    invalid: anstyle::Style,
}

impl Styles {
    /// No terminal styling
    pub const fn plain() -> Self {
        Self {
            header: anstyle::Style::new(),
            error: anstyle::Style::new(),
            usage: anstyle::Style::new(),
            literal: anstyle::Style::new(),
            placeholder: anstyle::Style::new(),
            valid: anstyle::Style::new(),
            invalid: anstyle::Style::new(),
        }
    }

    /// Default terminal styling
    pub const fn styled() -> Self {
        #[cfg(feature = "color")]
        {
            Self {
                header: anstyle::Style::new().bold().underline(),
                error: anstyle::Style::new()
                    .fg_color(Some(anstyle::Color::Ansi(anstyle::AnsiColor::Red)))
                    .bold(),
                usage: anstyle::Style::new().bold().underline(),
                literal: anstyle::Style::new().bold(),
                placeholder: anstyle::Style::new(),
                valid: anstyle::Style::new()
                    .fg_color(Some(anstyle::Color::Ansi(anstyle::AnsiColor::Green))),
                invalid: anstyle::Style::new()
                    .fg_color(Some(anstyle::Color::Ansi(anstyle::AnsiColor::Yellow))),
            }
        }
        #[cfg(not(feature = "color"))]
        {
            Self::plain()
        }
    }

    /// General Heading style, e.g. [`help_heading`][crate::Arg::help_heading]
    #[inline]
    pub const fn header(mut self, style: anstyle::Style) -> Self {
        self.header = style;
        self
    }

    /// Error heading
    #[inline]
    pub const fn error(mut self, style: anstyle::Style) -> Self {
        self.error = style;
        self
    }

    /// Usage heading
    #[inline]
    pub const fn usage(mut self, style: anstyle::Style) -> Self {
        self.usage = style;
        self
    }

    /// Literal command-line syntax, e.g. `--help`
    #[inline]
    pub const fn literal(mut self, style: anstyle::Style) -> Self {
        self.literal = style;
        self
    }

    /// Descriptions within command-line syntax, e.g. [`value_name`][crate::Arg::value_name]
    #[inline]
    pub const fn placeholder(mut self, style: anstyle::Style) -> Self {
        self.placeholder = style;
        self
    }

    /// Highlight suggested usage
    #[inline]
    pub const fn valid(mut self, style: anstyle::Style) -> Self {
        self.valid = style;
        self
    }

    /// Highlight invalid usage
    #[inline]
    pub const fn invalid(mut self, style: anstyle::Style) -> Self {
        self.invalid = style;
        self
    }
}

/// Reflection
impl Styles {
    /// General Heading style, e.g. [`help_heading`][crate::Arg::help_heading]
    #[inline(always)]
    pub const fn get_header(&self) -> &anstyle::Style {
        &self.header
    }

    /// Error heading
    #[inline(always)]
    pub const fn get_error(&self) -> &anstyle::Style {
        &self.error
    }

    /// Usage heading
    #[inline(always)]
    pub const fn get_usage(&self) -> &anstyle::Style {
        &self.usage
    }

    /// Literal command-line syntax, e.g. `--help`
    #[inline(always)]
    pub const fn get_literal(&self) -> &anstyle::Style {
        &self.literal
    }

    /// Descriptions within command-line syntax, e.g. [`value_name`][crate::Arg::value_name]
    #[inline(always)]
    pub const fn get_placeholder(&self) -> &anstyle::Style {
        &self.placeholder
    }

    /// Highlight suggested usage
    #[inline(always)]
    pub const fn get_valid(&self) -> &anstyle::Style {
        &self.valid
    }

    /// Highlight invalid usage
    #[inline(always)]
    pub const fn get_invalid(&self) -> &anstyle::Style {
        &self.invalid
    }
}

impl super::AppTag for Styles {}

impl Default for Styles {
    fn default() -> Self {
        Self::styled()
    }
}

impl Default for &'_ Styles {
    fn default() -> Self {
        const STYLES: Styles = Styles::styled();
        &STYLES
    }
}
