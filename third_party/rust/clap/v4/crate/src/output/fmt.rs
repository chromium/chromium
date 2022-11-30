use crate::builder::StyledStr;
use crate::util::color::ColorChoice;

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub(crate) enum Stream {
    Stdout,
    Stderr,
}

#[derive(Clone, Debug)]
pub(crate) struct Colorizer {
    stream: Stream,
    #[allow(unused)]
    color_when: ColorChoice,
    content: StyledStr,
}

impl Colorizer {
    pub(crate) fn new(stream: Stream, color_when: ColorChoice) -> Self {
        Colorizer {
            stream,
            color_when,
            content: Default::default(),
        }
    }

    pub(crate) fn with_content(mut self, content: StyledStr) -> Self {
        self.content = content;
        self
    }
}

/// Printing methods.
impl Colorizer {
    #[cfg(feature = "color")]
    pub(crate) fn print(&self) -> std::io::Result<()> {
        use termcolor::{BufferWriter, ColorChoice as DepColorChoice};

        let color_when = match self.color_when {
            ColorChoice::Always => DepColorChoice::Always,
            ColorChoice::Auto if is_a_tty(self.stream) => DepColorChoice::Auto,
            _ => DepColorChoice::Never,
        };

        let writer = match self.stream {
            Stream::Stderr => BufferWriter::stderr(color_when),
            Stream::Stdout => BufferWriter::stdout(color_when),
        };

        let mut buffer = writer.buffer();
        ok!(self.content.write_colored(&mut buffer));
        writer.print(&buffer)
    }

    #[cfg(not(feature = "color"))]
    pub(crate) fn print(&self) -> std::io::Result<()> {
        use std::io::Write;

        // [e]println can't be used here because it panics
        // if something went wrong. We don't want that.
        match self.stream {
            Stream::Stdout => {
                let stdout = std::io::stdout();
                let mut stdout = stdout.lock();
                write!(stdout, "{}", self)
            }
            Stream::Stderr => {
                let stderr = std::io::stderr();
                let mut stderr = stderr.lock();
                write!(stderr, "{}", self)
            }
        }
    }
}

/// Color-unaware printing. Never uses coloring.
impl std::fmt::Display for Colorizer {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        self.content.fmt(f)
    }
}

#[cfg(feature = "color")]
fn is_a_tty(stream: Stream) -> bool {
    let stream = match stream {
        Stream::Stdout => atty::Stream::Stdout,
        Stream::Stderr => atty::Stream::Stderr,
    };

    atty::is(stream)
}
