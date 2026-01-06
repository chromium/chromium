use std::collections::BTreeMap;
use std::fmt;

use crate::compiler::tokens::Span;
use crate::error::ErrorKind;
use crate::value::Value;

/// This is a snapshot of the debug information.
#[cfg_attr(docsrs, doc(cfg(feature = "debug")))]
#[derive(Default)]
pub(crate) struct DebugInfo {
    pub(crate) template_source: Option<String>,
    pub(crate) referenced_locals: BTreeMap<String, Value>,
}

struct VarPrinter<'x>(&'x BTreeMap<String, Value>);

impl fmt::Debug for VarPrinter<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.0.is_empty() {
            return f.write_str("No referenced variables");
        }
        let mut m = f.debug_struct("Referenced variables:");
        let mut vars = self.0.iter().collect::<Vec<_>>();
        vars.sort_by_key(|x| x.0);
        for (key, value) in vars {
            m.field(key, value);
        }
        m.finish()
    }
}

impl DebugInfo {
    pub fn source(&self) -> Option<&str> {
        self.template_source.as_deref()
    }
}

pub(super) fn render_debug_info(
    f: &mut fmt::Formatter,
    name: Option<&str>,
    kind: ErrorKind,
    line: Option<usize>,
    span: Option<Span>,
    info: &DebugInfo,
) -> fmt::Result {
    if let Some(source) = info.source() {
        let title = format!(
            " {} ",
            name.unwrap_or_default()
                .rsplit(&['/', '\\'])
                .next()
                .unwrap_or("Template Source")
        );
        ok!(writeln!(f));
        writeln!(f, "{:-^1$}", title, 79).unwrap();
        let lines: Vec<_> = source.lines().enumerate().collect();
        let idx = line.unwrap_or(1).saturating_sub(1);
        let skip = idx.saturating_sub(3);
        let pre = lines.iter().skip(skip).take(3.min(idx)).collect::<Vec<_>>();
        let post = lines.iter().skip(idx + 1).take(3).collect::<Vec<_>>();
        for (idx, line) in pre {
            writeln!(f, "{:>4} | {}", idx + 1, line).unwrap();
        }

        if let Some(line) = lines.get(idx) {
            writeln!(f, "{:>4} > {}", idx + 1, line.1).unwrap();
        }
        if let Some(span) = span {
            if span.start_line == span.end_line {
                ok!(writeln!(
                    f,
                    "     i {}{} {}",
                    " ".repeat(span.start_col as usize),
                    "^".repeat(span.end_col as usize - span.start_col as usize),
                    kind,
                ));
            }
        }

        for (idx, line) in post {
            writeln!(f, "{:>4} | {}", idx + 1, line).unwrap();
        }
        write!(f, "{:~^1$}", "", 79).unwrap();
    }
    ok!(writeln!(f));
    ok!(writeln!(f, "{:#?}", VarPrinter(&info.referenced_locals)));
    write!(f, "{:-^1$}", "", 79).unwrap();
    Ok(())
}
