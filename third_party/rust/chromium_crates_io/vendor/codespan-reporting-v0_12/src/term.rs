//! Terminal back-end for emitting diagnostics.

#[cfg(feature = "termcolor")]
use termcolor::WriteColor;

use crate::diagnostic::Diagnostic;
use crate::files::Files;

mod config;
mod renderer;
mod views;

#[cfg(feature = "termcolor")]
pub use termcolor;

pub use self::config::{Chars, Config, DisplayStyle};

#[cfg(feature = "termcolor")]
pub use self::config::Styles;

/// Emit a diagnostic using the given writer, context, config, and files.
///
/// The return value covers all error cases. These error case can arise if:
/// * a file was removed from the file database.
/// * a file was changed so that it is too small to have an index
/// * IO fails
pub fn emit<'files, F: Files<'files> + ?Sized>(
    #[cfg(feature = "termcolor")] writer: &mut dyn WriteColor,
    #[cfg(all(not(feature = "termcolor"), feature = "std"))] writer: &mut dyn std::io::Write,
    #[cfg(all(not(feature = "termcolor"), not(feature = "std")))] writer: &mut dyn core::fmt::Write,
    config: &Config,
    files: &'files F,
    diagnostic: &Diagnostic<F::FileId>,
) -> Result<(), super::files::Error> {
    use self::renderer::Renderer;
    use self::views::{RichDiagnostic, ShortDiagnostic};

    let mut renderer = Renderer::new(writer, config);
    match config.display_style {
        DisplayStyle::Rich => RichDiagnostic::new(diagnostic, config).render(files, &mut renderer),
        DisplayStyle::Medium => ShortDiagnostic::new(diagnostic, true).render(files, &mut renderer),
        DisplayStyle::Short => ShortDiagnostic::new(diagnostic, false).render(files, &mut renderer),
    }
}

#[cfg(all(test, feature = "termcolor"))]
mod tests {
    use alloc::{vec, vec::Vec};

    use super::*;

    use crate::diagnostic::Label;
    use crate::files::SimpleFiles;

    #[test]
    fn unsized_emit() {
        let mut files = SimpleFiles::new();

        let id = files.add("test", "");
        let mut writer = termcolor::NoColor::new(Vec::<u8>::new());
        let diagnostic = Diagnostic::bug().with_labels(vec![Label::primary(id, 0..0)]);

        emit(&mut writer, &Config::default(), &files, &diagnostic).unwrap();
    }
}
