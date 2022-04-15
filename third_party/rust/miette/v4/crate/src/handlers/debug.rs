use std::fmt;

use crate::{protocol::Diagnostic, ReportHandler};

/**
[`ReportHandler`] that renders plain text and avoids extraneous graphics.
It's optimized for screen readers and braille users, but is also used in any
non-graphical environments, such as non-TTY output.
*/
#[derive(Debug, Clone)]
pub struct DebugReportHandler;

impl DebugReportHandler {
    /// Create a new [`NarratableReportHandler`](crate::NarratableReportHandler)
    /// There are no customization options.
    pub fn new() -> Self {
        Self
    }
}

impl Default for DebugReportHandler {
    fn default() -> Self {
        Self::new()
    }
}

impl DebugReportHandler {
    /// Render a [`Diagnostic`]. This function is mostly internal and meant to
    /// be called by the toplevel [`ReportHandler`] handler, but is made public
    /// to make it easier (possible) to test in isolation from global state.
    pub fn render_report(
        &self,
        f: &mut fmt::Formatter<'_>,
        diagnostic: &(dyn Diagnostic),
    ) -> fmt::Result {
        let mut diag = f.debug_struct("Diagnostic");
        diag.field("message", &format!("{}", diagnostic));
        if let Some(code) = diagnostic.code() {
            diag.field("code", &code.to_string());
        }
        if let Some(severity) = diagnostic.severity() {
            diag.field("severity", &format!("{:?}", severity));
        }
        if let Some(url) = diagnostic.url() {
            diag.field("url", &url.to_string());
        }
        if let Some(help) = diagnostic.help() {
            diag.field("help", &help.to_string());
        }
        if let Some(labels) = diagnostic.labels() {
            let labels: Vec<_> = labels.collect();
            diag.field("labels", &format!("{:?}", labels));
        }
        diag.finish()?;
        writeln!(f)?;
        writeln!(f, "NOTE: If you're looking for the fancy error reports, install miette with the `fancy` feature, or write your own and hook it up with miette::set_hook().")
    }
}

impl ReportHandler for DebugReportHandler {
    fn debug(&self, diagnostic: &(dyn Diagnostic), f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if f.alternate() {
            return fmt::Debug::fmt(diagnostic, f);
        }

        self.render_report(f, diagnostic)
    }
}
