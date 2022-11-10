use atty::Stream;
use owo_colors::Style;

/**
Theme used by [`GraphicalReportHandler`](crate::GraphicalReportHandler) to
render fancy [`Diagnostic`](crate::Diagnostic) reports.

A theme consists of two things: the set of characters to be used for drawing,
and the
[`owo_colors::Style`](https://docs.rs/owo-colors/latest/owo_colors/struct.Style.html)s to be used to paint various items.

You can create your own custom graphical theme using this type, or you can use
one of the predefined ones using the methods below.
*/
#[derive(Debug, Clone)]
pub struct GraphicalTheme {
    /// Characters to be used for drawing.
    pub characters: ThemeCharacters,
    /// Styles to be used for painting.
    pub styles: ThemeStyles,
}

impl GraphicalTheme {
    /// ASCII-art-based graphical drawing, with ANSI styling.
    pub fn ascii() -> Self {
        Self {
            characters: ThemeCharacters::ascii(),
            styles: ThemeStyles::ansi(),
        }
    }

    /// Graphical theme that draws using both ansi colors and unicode
    /// characters.
    pub fn unicode() -> Self {
        Self {
            characters: ThemeCharacters::unicode(),
            styles: ThemeStyles::rgb(),
        }
    }

    /// Graphical theme that draws in monochrome, while still using unicode
    /// characters.
    pub fn unicode_nocolor() -> Self {
        Self {
            characters: ThemeCharacters::unicode(),
            styles: ThemeStyles::none(),
        }
    }

    /// A "basic" graphical theme that skips colors and unicode characters and
    /// just does monochrome ascii art. If you want a completely non-graphical
    /// rendering of your `Diagnostic`s, check out
    /// [crate::NarratableReportHandler], or write your own
    /// [crate::ReportHandler]!
    pub fn none() -> Self {
        Self {
            characters: ThemeCharacters::ascii(),
            styles: ThemeStyles::none(),
        }
    }
}

impl Default for GraphicalTheme {
    fn default() -> Self {
        match std::env::var("NO_COLOR") {
            _ if !atty::is(Stream::Stdout) || !atty::is(Stream::Stderr) => Self::ascii(),
            Ok(string) if string != "0" => Self::unicode_nocolor(),
            _ => Self::unicode(),
        }
    }
}

/**
Styles for various parts of graphical rendering for the [crate::GraphicalReportHandler].
*/
#[derive(Debug, Clone)]
pub struct ThemeStyles {
    /// Style to apply to things highlighted as "error".
    pub error: Style,
    /// Style to apply to things highlighted as "warning".
    pub warning: Style,
    /// Style to apply to things highlighted as "advice".
    pub advice: Style,
    /// Style to apply to the help text.
    pub help: Style,
    /// Style to apply to filenames/links/URLs.
    pub link: Style,
    /// Style to apply to line numbers.
    pub linum: Style,
    /// Styles to cycle through (using `.iter().cycle()`), to render the lines
    /// and text for diagnostic highlights.
    pub highlights: Vec<Style>,
}

fn style() -> Style {
    Style::new()
}

impl ThemeStyles {
    /// Nice RGB colors.
    /// [Credit](http://terminal.sexy/#FRUV0NDQFRUVrEFCkKlZ9L91ap-1qnWfdbWq0NDQUFBQrEFCkKlZ9L91ap-1qnWfdbWq9fX1).
    pub fn rgb() -> Self {
        Self {
            error: style().fg_rgb::<255, 30, 30>(),
            warning: style().fg_rgb::<244, 191, 117>(),
            advice: style().fg_rgb::<106, 159, 181>(),
            help: style().fg_rgb::<106, 159, 181>(),
            link: style().fg_rgb::<92, 157, 255>().underline().bold(),
            linum: style().dimmed(),
            highlights: vec![
                style().fg_rgb::<246, 87, 248>(),
                style().fg_rgb::<30, 201, 212>(),
                style().fg_rgb::<145, 246, 111>(),
            ],
        }
    }

    /// ANSI color-based styles.
    pub fn ansi() -> Self {
        Self {
            error: style().red(),
            warning: style().yellow(),
            advice: style().cyan(),
            help: style().cyan(),
            link: style().cyan().underline().bold(),
            linum: style().dimmed(),
            highlights: vec![
                style().magenta().bold(),
                style().yellow().bold(),
                style().green().bold(),
            ],
        }
    }

    /// No styling. Just regular ol' monochrome.
    pub fn none() -> Self {
        Self {
            error: style(),
            warning: style(),
            advice: style(),
            help: style(),
            link: style(),
            linum: style(),
            highlights: vec![style()],
        }
    }
}

// ----------------------------------------
// Most of these characters were taken from
// https://github.com/zesterer/ariadne/blob/e3cb394cb56ecda116a0a1caecd385a49e7f6662/src/draw.rs

/// Characters to be used when drawing when using
/// [crate::GraphicalReportHandler].
#[allow(missing_docs)]
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct ThemeCharacters {
    pub hbar: char,
    pub vbar: char,
    pub xbar: char,
    pub vbar_break: char,

    pub uarrow: char,
    pub rarrow: char,

    pub ltop: char,
    pub mtop: char,
    pub rtop: char,
    pub lbot: char,
    pub rbot: char,
    pub mbot: char,

    pub lbox: char,
    pub rbox: char,

    pub lcross: char,
    pub rcross: char,

    pub underbar: char,
    pub underline: char,

    pub error: String,
    pub warning: String,
    pub advice: String,
}

impl ThemeCharacters {
    /// Fancy unicode-based graphical elements.
    pub fn unicode() -> Self {
        Self {
            hbar: '─',
            vbar: '│',
            xbar: '┼',
            vbar_break: '·',
            uarrow: '▲',
            rarrow: '▶',
            ltop: '╭',
            mtop: '┬',
            rtop: '╮',
            lbot: '╰',
            mbot: '┴',
            rbot: '╯',
            lbox: '[',
            rbox: ']',
            lcross: '├',
            rcross: '┤',
            underbar: '┬',
            underline: '─',
            error: "×".into(),
            warning: "⚠".into(),
            advice: "☞".into(),
        }
    }

    /// Emoji-heavy unicode characters.
    pub fn emoji() -> Self {
        Self {
            hbar: '─',
            vbar: '│',
            xbar: '┼',
            vbar_break: '·',
            uarrow: '▲',
            rarrow: '▶',
            ltop: '╭',
            mtop: '┬',
            rtop: '╮',
            lbot: '╰',
            mbot: '┴',
            rbot: '╯',
            lbox: '[',
            rbox: ']',
            lcross: '├',
            rcross: '┤',
            underbar: '┬',
            underline: '─',
            error: "💥".into(),
            warning: "⚠️".into(),
            advice: "💡".into(),
        }
    }
    /// ASCII-art-based graphical elements. Works well on older terminals.
    pub fn ascii() -> Self {
        Self {
            hbar: '-',
            vbar: '|',
            xbar: '+',
            vbar_break: ':',
            uarrow: '^',
            rarrow: '>',
            ltop: ',',
            mtop: 'v',
            rtop: '.',
            lbot: '`',
            mbot: '^',
            rbot: '\'',
            lbox: '[',
            rbox: ']',
            lcross: '|',
            rcross: '|',
            underbar: '|',
            underline: '^',
            error: "x".into(),
            warning: "!".into(),
            advice: ">".into(),
        }
    }
}
