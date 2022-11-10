use std::fmt::{self, Write};

use crate::{
    diagnostic_chain::DiagnosticChain, protocol::Diagnostic, ReportHandler, Severity, SourceCode,
};

/**
[`ReportHandler`] that renders JSON output. It's a machine-readable output.
*/
#[derive(Debug, Clone)]
pub struct JSONReportHandler;

impl JSONReportHandler {
    /// Create a new [`JSONReportHandler`]. There are no customization
    /// options.
    pub fn new() -> Self {
        Self
    }
}

impl Default for JSONReportHandler {
    fn default() -> Self {
        Self::new()
    }
}

struct Escape<'a>(&'a str);

impl fmt::Display for Escape<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        for c in self.0.chars() {
            let escape = match c {
                '\\' => Some(r"\\"),
                '"' => Some(r#"\""#),
                '\r' => Some(r"\r"),
                '\n' => Some(r"\n"),
                '\t' => Some(r"\t"),
                '\u{08}' => Some(r"\b"),
                '\u{0c}' => Some(r"\f"),
                _ => None,
            };
            if let Some(escape) = escape {
                f.write_str(escape)?;
            } else {
                f.write_char(c)?;
            }
        }
        Ok(())
    }
}

fn escape(input: &'_ str) -> Escape<'_> {
    Escape(input)
}

impl JSONReportHandler {
    /// Render a [`Diagnostic`]. This function is mostly internal and meant to
    /// be called by the toplevel [`ReportHandler`] handler, but is made public
    /// to make it easier (possible) to test in isolation from global state.
    pub fn render_report(
        &self,
        f: &mut impl fmt::Write,
        diagnostic: &(dyn Diagnostic),
    ) -> fmt::Result {
        self._render_report(f, diagnostic, None)
    }

    fn _render_report(
        &self,
        f: &mut impl fmt::Write,
        diagnostic: &(dyn Diagnostic),
        parent_src: Option<&dyn SourceCode>,
    ) -> fmt::Result {
        write!(f, r#"{{"message": "{}","#, escape(&diagnostic.to_string()))?;
        if let Some(code) = diagnostic.code() {
            write!(f, r#""code": "{}","#, escape(&code.to_string()))?;
        }
        let severity = match diagnostic.severity() {
            Some(Severity::Error) | None => "error",
            Some(Severity::Warning) => "warning",
            Some(Severity::Advice) => "advice",
        };
        write!(f, r#""severity": "{:}","#, severity)?;
        if let Some(cause_iter) = diagnostic
            .diagnostic_source()
            .map(DiagnosticChain::from_diagnostic)
            .or_else(|| diagnostic.source().map(DiagnosticChain::from_stderror))
        {
            write!(f, r#""causes": ["#)?;
            let mut add_comma = false;
            for error in cause_iter {
                if add_comma {
                    write!(f, ",")?;
                } else {
                    add_comma = true;
                }
                write!(f, r#""{}""#, escape(&error.to_string()))?;
            }
            write!(f, "],")?
        } else {
            write!(f, r#""causes": [],"#)?;
        }
        if let Some(url) = diagnostic.url() {
            write!(f, r#""url": "{}","#, &url.to_string())?;
        }
        if let Some(help) = diagnostic.help() {
            write!(f, r#""help": "{}","#, escape(&help.to_string()))?;
        }
        let src = diagnostic.source_code().or(parent_src);
        if let Some(src) = src {
            self.render_snippets(f, diagnostic, src)?;
        }
        if let Some(labels) = diagnostic.labels() {
            write!(f, r#""labels": ["#)?;
            let mut add_comma = false;
            for label in labels {
                if add_comma {
                    write!(f, ",")?;
                } else {
                    add_comma = true;
                }
                write!(f, "{{")?;
                if let Some(label_name) = label.label() {
                    write!(f, r#""label": "{}","#, escape(label_name))?;
                }
                write!(f, r#""span": {{"#)?;
                write!(f, r#""offset": {},"#, label.offset())?;
                write!(f, r#""length": {}"#, label.len())?;

                write!(f, "}}}}")?;
            }
            write!(f, "],")?;
        } else {
            write!(f, r#""labels": [],"#)?;
        }
        if let Some(relateds) = diagnostic.related() {
            write!(f, r#""related": ["#)?;
            let mut add_comma = false;
            for related in relateds {
                if add_comma {
                    write!(f, ",")?;
                } else {
                    add_comma = true;
                }
                self._render_report(f, related, src)?;
            }
            write!(f, "]")?;
        } else {
            write!(f, r#""related": []"#)?;
        }
        write!(f, "}}")
    }

    fn render_snippets(
        &self,
        f: &mut impl fmt::Write,
        diagnostic: &(dyn Diagnostic),
        source: &dyn SourceCode,
    ) -> fmt::Result {
        if let Some(mut labels) = diagnostic.labels() {
            if let Some(label) = labels.next() {
                if let Ok(span_content) = source.read_span(label.inner(), 0, 0) {
                    let filename = span_content.name().unwrap_or_default();
                    return write!(f, r#""filename": "{}","#, escape(filename));
                }
            }
        }
        write!(f, r#""filename": "","#)
    }
}

impl ReportHandler for JSONReportHandler {
    fn debug(&self, diagnostic: &(dyn Diagnostic), f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.render_report(f, diagnostic)
    }
}

#[test]
fn test_escape() {
    assert_eq!(escape("a\nb").to_string(), r"a\nb");
    assert_eq!(escape("C:\\Miette").to_string(), r"C:\\Miette");
}
