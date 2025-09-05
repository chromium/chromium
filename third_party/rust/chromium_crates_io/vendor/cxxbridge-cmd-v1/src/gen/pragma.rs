use crate::gen::out::{Content, OutFile};
use std::collections::BTreeSet;

#[derive(Default)]
pub(crate) struct Pragma<'a> {
    pub gnu_diagnostic_ignore: BTreeSet<&'a str>,
    pub clang_diagnostic_ignore: BTreeSet<&'a str>,
    pub dollar_in_identifier: bool,
    pub begin: Content<'a>,
    pub end: Content<'a>,
}

impl<'a> Pragma<'a> {
    pub fn new() -> Self {
        Pragma::default()
    }
}

pub(super) fn write(out: &mut OutFile) {
    if out.pragma.dollar_in_identifier {
        out.pragma
            .clang_diagnostic_ignore
            .insert("-Wdollar-in-identifier-extension");
    }

    let begin = &mut out.pragma.begin;
    if !out.pragma.gnu_diagnostic_ignore.is_empty() {
        writeln!(begin, "#ifdef __GNUC__");
        if out.header {
            writeln!(begin, "#pragma GCC diagnostic push");
        }
        for diag in &out.pragma.gnu_diagnostic_ignore {
            writeln!(begin, "#pragma GCC diagnostic ignored \"{diag}\"");
        }
    }
    if !out.pragma.clang_diagnostic_ignore.is_empty() {
        writeln!(begin, "#ifdef __clang__");
        if out.header && out.pragma.gnu_diagnostic_ignore.is_empty() {
            writeln!(begin, "#pragma clang diagnostic push");
        }
        for diag in &out.pragma.clang_diagnostic_ignore {
            writeln!(begin, "#pragma clang diagnostic ignored \"{diag}\"");
        }
        writeln!(begin, "#endif // __clang__");
    }
    if !out.pragma.gnu_diagnostic_ignore.is_empty() {
        writeln!(begin, "#endif // __GNUC__");
    }

    if out.header {
        let end = &mut out.pragma.end;
        if !out.pragma.gnu_diagnostic_ignore.is_empty() {
            writeln!(end, "#ifdef __GNUC__");
            writeln!(end, "#pragma GCC diagnostic pop");
            writeln!(end, "#endif // __GNUC__");
        } else if !out.pragma.clang_diagnostic_ignore.is_empty() {
            writeln!(end, "#ifdef __clang__");
            writeln!(end, "#pragma clang diagnostic pop");
            writeln!(end, "#endif // __clang__");
        }
    }
}
