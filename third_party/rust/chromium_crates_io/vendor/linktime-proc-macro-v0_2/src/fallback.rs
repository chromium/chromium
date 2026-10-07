#![allow(unstable_name_collisions)] // Intentional!

use proc_macro::Span;
use std::path::{Path, PathBuf};

/// Trait providing a fallback method for `Span` methods.
#[allow(unused)]
pub(crate) trait FallbackSpan {
    fn line(&self) -> usize;
    fn column(&self) -> usize;
    fn file(&self) -> String;
    fn local_file(&self) -> Option<PathBuf>;
    fn source_text(&self) -> Option<String>;
}

/// Implement it for references of Span using autoref, giving it lower priority
/// than inherent methods.
impl FallbackSpan for &Span {
    #[inline(always)]
    fn line(&self) -> usize {
        0
    }
    #[inline(always)]
    fn column(&self) -> usize {
        0
    }
    #[inline(always)]
    fn file(&self) -> String {
        String::new()
    }
    #[inline(always)]
    fn local_file(&self) -> Option<PathBuf> {
        None
    }
    #[inline(always)]
    fn source_text(&self) -> Option<String> {
        None
    }
}

#[allow(unused, clippy::needless_borrow)]
pub(crate) fn has_1_88_span_methods(span: Span) -> bool {
    let line_val = (&span).line();

    // If it returns 0, we are on an older compiler.
    line_val != 0
}

#[inline(always)]
pub(crate) fn file(span: &Span) -> String {
    let file = span.file();
    let path = Path::new(&file);
    if path.is_absolute() {
        // Rust 1.88..1.94 reports dependency-crate spans as absolute paths when
        // inspected from a downstream crate, while the defining crate sees the
        // working-directory-relative form. Relativise against rustc's working
        // directory so both agree, fall back to the file name otherwise (e.g.
        // `cargo expand`, whose paths are not under the working directory).
        std::env::current_dir()
            .ok()
            .and_then(|cwd| path.strip_prefix(&cwd).ok().map(Path::to_path_buf))
            .map(|rel| rel.to_string_lossy().into_owned())
            .unwrap_or_else(|| {
                path.file_name()
                    .unwrap_or_default()
                    .to_string_lossy()
                    .into_owned()
            })
    } else {
        file
    }
}

#[inline(always)]
pub(crate) fn line(span: &Span) -> usize {
    span.line()
}

#[inline(always)]
pub(crate) fn local_file(span: &Span) -> Option<PathBuf> {
    span.local_file()
}

#[inline(always)]
pub(crate) fn column(span: &Span) -> usize {
    span.column()
}

#[inline(always)]
pub(crate) fn source_text(span: &Span) -> Option<String> {
    span.source_text()
}
