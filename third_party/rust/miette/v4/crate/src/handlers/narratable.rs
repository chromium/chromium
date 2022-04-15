use std::fmt;

use unicode_width::{UnicodeWidthChar, UnicodeWidthStr};

use crate::chain::Chain;
use crate::protocol::{Diagnostic, Severity};
use crate::{LabeledSpan, MietteError, ReportHandler, SourceCode, SourceSpan, SpanContents};

/**
[`ReportHandler`] that renders plain text and avoids extraneous graphics.
It's optimized for screen readers and braille users, but is also used in any
non-graphical environments, such as non-TTY output.
*/
#[derive(Debug, Clone)]
pub struct NarratableReportHandler {
    context_lines: usize,
    footer: Option<String>,
}

impl NarratableReportHandler {
    /// Create a new [`NarratableReportHandler`]. There are no customization
    /// options.
    pub fn new() -> Self {
        Self {
            footer: None,
            context_lines: 1,
        }
    }

    /// Set the footer to be displayed at the end of the report.
    pub fn with_footer(mut self, footer: String) -> Self {
        self.footer = Some(footer);
        self
    }

    /// Sets the number of lines of context to show around each error.
    pub fn with_context_lines(mut self, lines: usize) -> Self {
        self.context_lines = lines;
        self
    }
}

impl Default for NarratableReportHandler {
    fn default() -> Self {
        Self::new()
    }
}

impl NarratableReportHandler {
    /// Render a [`Diagnostic`]. This function is mostly internal and meant to
    /// be called by the toplevel [`ReportHandler`] handler, but is
    /// made public to make it easier (possible) to test in isolation from
    /// global state.
    pub fn render_report(
        &self,
        f: &mut impl fmt::Write,
        diagnostic: &(dyn Diagnostic),
    ) -> fmt::Result {
        self.render_header(f, diagnostic)?;
        self.render_causes(f, diagnostic)?;
        let src = diagnostic.source_code();
        self.render_snippets(f, diagnostic, src)?;
        self.render_footer(f, diagnostic)?;
        self.render_related(f, diagnostic, src)?;
        if let Some(footer) = &self.footer {
            writeln!(f, "{}", footer)?;
        }
        Ok(())
    }

    fn render_header(&self, f: &mut impl fmt::Write, diagnostic: &(dyn Diagnostic)) -> fmt::Result {
        writeln!(f, "{}", diagnostic)?;
        let severity = match diagnostic.severity() {
            Some(Severity::Error) | None => "error",
            Some(Severity::Warning) => "warning",
            Some(Severity::Advice) => "advice",
        };
        writeln!(f, "    Diagnostic severity: {}", severity)?;
        Ok(())
    }

    fn render_causes(&self, f: &mut impl fmt::Write, diagnostic: &(dyn Diagnostic)) -> fmt::Result {
        if let Some(cause) = diagnostic.source() {
            for error in Chain::new(cause) {
                writeln!(f, "    Caused by: {}", error)?;
            }
        }

        Ok(())
    }

    fn render_footer(&self, f: &mut impl fmt::Write, diagnostic: &(dyn Diagnostic)) -> fmt::Result {
        if let Some(help) = diagnostic.help() {
            writeln!(f, "diagnostic help: {}", help)?;
        }
        if let Some(code) = diagnostic.code() {
            writeln!(f, "diagnostic code: {}", code)?;
        }
        if let Some(url) = diagnostic.url() {
            writeln!(f, "For more details, see {}", url)?;
        }
        Ok(())
    }

    fn render_related(
        &self,
        f: &mut impl fmt::Write,
        diagnostic: &(dyn Diagnostic),
        parent_src: Option<&dyn SourceCode>,
    ) -> fmt::Result {
        if let Some(related) = diagnostic.related() {
            writeln!(f)?;
            for rel in related {
                write!(f, "Error: ")?;
                self.render_header(f, rel)?;
                writeln!(f)?;
                self.render_causes(f, rel)?;
                let src = rel.source_code().or(parent_src);
                self.render_snippets(f, rel, src)?;
                self.render_footer(f, rel)?;
                self.render_related(f, rel, src)?;
            }
        }
        Ok(())
    }

    fn render_snippets(
        &self,
        f: &mut impl fmt::Write,
        diagnostic: &(dyn Diagnostic),
        source_code: Option<&dyn SourceCode>,
    ) -> fmt::Result {
        if let Some(source) = source_code {
            if let Some(labels) = diagnostic.labels() {
                let mut labels = labels.collect::<Vec<_>>();
                labels.sort_unstable_by_key(|l| l.inner().offset());
                if !labels.is_empty() {
                    let contents = labels
                        .iter()
                        .map(|label| {
                            source.read_span(label.inner(), self.context_lines, self.context_lines)
                        })
                        .collect::<Result<Vec<Box<dyn SpanContents<'_>>>, MietteError>>()
                        .map_err(|_| fmt::Error)?;
                    let mut contexts = Vec::new();
                    for (right, right_conts) in labels.iter().cloned().zip(contents.iter()) {
                        if contexts.is_empty() {
                            contexts.push((right, right_conts));
                        } else {
                            let (left, left_conts) = contexts.last().unwrap().clone();
                            let left_end = left.offset() + left.len();
                            let right_end = right.offset() + right.len();
                            if left_conts.line() + left_conts.line_count() >= right_conts.line() {
                                // The snippets will overlap, so we create one Big Chunky Boi
                                let new_span = LabeledSpan::new(
                                    left.label().map(String::from),
                                    left.offset(),
                                    if right_end >= left_end {
                                        // Right end goes past left end
                                        right_end - left.offset()
                                    } else {
                                        // right is contained inside left
                                        left.len()
                                    },
                                );
                                if source
                                    .read_span(
                                        new_span.inner(),
                                        self.context_lines,
                                        self.context_lines,
                                    )
                                    .is_ok()
                                {
                                    contexts.pop();
                                    contexts.push((
                                        new_span, // We'll throw this away later
                                        left_conts,
                                    ));
                                } else {
                                    contexts.push((right, right_conts));
                                }
                            } else {
                                contexts.push((right, right_conts));
                            }
                        }
                    }
                    for (ctx, _) in contexts {
                        self.render_context(f, source, &ctx, &labels[..])?;
                    }
                }
            }
        }
        Ok(())
    }

    fn render_context<'a>(
        &self,
        f: &mut impl fmt::Write,
        source: &'a dyn SourceCode,
        context: &LabeledSpan,
        labels: &[LabeledSpan],
    ) -> fmt::Result {
        let (contents, lines) = self.get_lines(source, context.inner())?;
        write!(f, "Begin snippet")?;
        if let Some(filename) = contents.name() {
            write!(f, " for {}", filename,)?;
        }
        writeln!(
            f,
            " starting at line {}, column {}",
            contents.line() + 1,
            contents.column() + 1
        )?;
        writeln!(f)?;
        for line in &lines {
            writeln!(f, "snippet line {}: {}", line.line_number, line.text)?;
            let relevant = labels
                .iter()
                .filter_map(|l| line.span_attach(l.inner()).map(|a| (a, l)));
            for (attach, label) in relevant {
                match attach {
                    SpanAttach::Contained { col_start, col_end } if col_start == col_end => {
                        write!(
                            f,
                            "    label at line {}, column {}",
                            line.line_number, col_start,
                        )?;
                    }
                    SpanAttach::Contained { col_start, col_end } => {
                        write!(
                            f,
                            "    label at line {}, columns {} to {}",
                            line.line_number, col_start, col_end,
                        )?;
                    }
                    SpanAttach::Starts { col_start } => {
                        write!(
                            f,
                            "    label starting at line {}, column {}",
                            line.line_number, col_start,
                        )?;
                    }
                    SpanAttach::Ends { col_end } => {
                        write!(
                            f,
                            "    label ending at line {}, column {}",
                            line.line_number, col_end,
                        )?;
                    }
                }
                if let Some(label) = label.label() {
                    write!(f, ": {}", label)?;
                }
                writeln!(f)?;
            }
        }
        Ok(())
    }

    fn get_lines<'a>(
        &'a self,
        source: &'a dyn SourceCode,
        context_span: &'a SourceSpan,
    ) -> Result<(Box<dyn SpanContents<'a> + 'a>, Vec<Line>), fmt::Error> {
        let context_data = source
            .read_span(context_span, self.context_lines, self.context_lines)
            .map_err(|_| fmt::Error)?;
        let context = std::str::from_utf8(context_data.data()).expect("Bad utf8 detected");
        let mut line = context_data.line();
        let mut column = context_data.column();
        let mut offset = context_data.span().offset();
        let mut line_offset = offset;
        let mut iter = context.chars().peekable();
        let mut line_str = String::new();
        let mut lines = Vec::new();
        while let Some(char) = iter.next() {
            offset += char.len_utf8();
            let mut at_end_of_file = false;
            match char {
                '\r' => {
                    if iter.next_if_eq(&'\n').is_some() {
                        offset += 1;
                        line += 1;
                        column = 0;
                    } else {
                        line_str.push(char);
                        column += 1;
                    }
                    at_end_of_file = iter.peek().is_none();
                }
                '\n' => {
                    at_end_of_file = iter.peek().is_none();
                    line += 1;
                    column = 0;
                }
                _ => {
                    line_str.push(char);
                    column += 1;
                }
            }

            if iter.peek().is_none() && !at_end_of_file {
                line += 1;
            }

            if column == 0 || iter.peek().is_none() {
                lines.push(Line {
                    line_number: line,
                    offset: line_offset,
                    text: line_str.clone(),
                    at_end_of_file,
                });
                line_str.clear();
                line_offset = offset;
            }
        }
        Ok((context_data, lines))
    }
}

impl ReportHandler for NarratableReportHandler {
    fn debug(&self, diagnostic: &(dyn Diagnostic), f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if f.alternate() {
            return fmt::Debug::fmt(diagnostic, f);
        }

        self.render_report(f, diagnostic)
    }
}

/*
Support types
*/

struct Line {
    line_number: usize,
    offset: usize,
    text: String,
    at_end_of_file: bool,
}

enum SpanAttach {
    Contained { col_start: usize, col_end: usize },
    Starts { col_start: usize },
    Ends { col_end: usize },
}

/// Returns column at offset, and nearest boundary if offset is in the middle of
/// the character
fn safe_get_column(text: &str, offset: usize, start: bool) -> usize {
    let mut column = text.get(0..offset).map(|s| s.width()).unwrap_or_else(|| {
        let mut column = 0;
        for (idx, c) in text.char_indices() {
            if offset <= idx {
                break;
            }
            column += c.width().unwrap_or(0);
        }
        column
    });
    if start {
        // Offset are zero-based, so plus one
        column += 1;
    } // On the other hand for end span, offset refers for the next column
      // So we should do -1. column+1-1 == column
    column
}

impl Line {
    fn span_attach(&self, span: &SourceSpan) -> Option<SpanAttach> {
        let span_end = span.offset() + span.len();
        let line_end = self.offset + self.text.len();

        let start_after = span.offset() >= self.offset;
        let end_before = self.at_end_of_file || span_end <= line_end;

        if start_after && end_before {
            let col_start = safe_get_column(&self.text, span.offset() - self.offset, true);
            let col_end = if span.is_empty() {
                col_start
            } else {
                // span_end refers to the next character after token
                // while col_end refers to the exact character, so -1
                safe_get_column(&self.text, span_end - self.offset, false)
            };
            return Some(SpanAttach::Contained { col_start, col_end });
        }
        if start_after && span.offset() <= line_end {
            let col_start = safe_get_column(&self.text, span.offset() - self.offset, true);
            return Some(SpanAttach::Starts { col_start });
        }
        if end_before && span_end >= self.offset {
            let col_end = safe_get_column(&self.text, span_end - self.offset, false);
            return Some(SpanAttach::Ends { col_end });
        }
        None
    }
}
