//! Utilities for manipulating C/C++ comments.

/// The type of a comment.
#[derive(Debug, PartialEq, Eq)]
enum Kind {
    /// A `///` comment, or something of the like.
    /// All lines in a comment should start with the same symbol.
    SingleLines,
    /// A `/**` comment, where each other line can start with `*` and the
    /// entire block ends with `*/`.
    MultiLine,
}

/// Preprocesses a C/C++ comment so that it is a valid Rust comment.
pub fn preprocess(comment: &str, indent: usize) -> String {
    match self::kind(comment) {
        Some(Kind::SingleLines) => preprocess_single_lines(comment, indent),
        Some(Kind::MultiLine) => preprocess_multi_line(comment, indent),
        None => comment.to_owned(),
    }
}

/// Gets the kind of the doc comment, if it is one.
fn kind(comment: &str) -> Option<Kind> {
    if comment.starts_with("/*") {
        Some(Kind::MultiLine)
    } else if comment.starts_with("//") {
        Some(Kind::SingleLines)
    } else {
        None
    }
}

fn make_indent(indent: usize) -> String {
    const RUST_INDENTATION: usize = 4;
    " ".repeat(indent * RUST_INDENTATION)
}

/// Preprocesses multiple single line comments.
///
/// Handles lines starting with both `//` and `///`.
fn preprocess_single_lines(comment: &str, indent: usize) -> String {
    debug_assert!(comment.starts_with("//"), "comment is not single line");

    let indent = make_indent(indent);
    let mut is_first = true;
    let lines: Vec<_> = comment
        .lines()
        .map(|l| l.trim().trim_start_matches('/'))
        .map(|l| {
            let indent = if is_first { "" } else { &*indent };
            is_first = false;
            format!("{}///{}", indent, l)
        })
        .collect();
    lines.join("\n")
}

fn preprocess_multi_line(comment: &str, indent: usize) -> String {
    let comment = comment
        .trim_start_matches('/')
        .trim_end_matches('/')
        .trim_end_matches('*');

    let indent = make_indent(indent);
    // Strip any potential `*` characters preceding each line.
    let mut is_first = true;
    let mut lines: Vec<_> = comment
        .lines()
        .map(|line| line.trim().trim_start_matches('*').trim_start_matches('!'))
        .skip_while(|line| line.trim().is_empty()) // Skip the first empty lines.
        .map(|line| {
            let indent = if is_first { "" } else { &*indent };
            is_first = false;
            format!("{}///{}", indent, line)
        })
        .collect();

    // Remove the trailing line corresponding to the `*/`.
    if lines
        .last()
        .map_or(false, |l| l.trim().is_empty() || l.trim() == "///")
    {
        lines.pop();
    }

    lines.join("\n")
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn picks_up_single_and_multi_line_doc_comments() {
        assert_eq!(kind("/// hello"), Some(Kind::SingleLines));
        assert_eq!(kind("/** world */"), Some(Kind::MultiLine));
    }

    #[test]
    fn processes_single_lines_correctly() {
        assert_eq!(preprocess("/// hello", 0), "/// hello");
        assert_eq!(preprocess("// hello", 0), "/// hello");
        assert_eq!(preprocess("//    hello", 0), "///    hello");
    }

    #[test]
    fn processes_multi_lines_correctly() {
        assert_eq!(
            preprocess("/** hello \n * world \n * foo \n */", 0),
            "/// hello\n/// world\n/// foo"
        );

        assert_eq!(
            preprocess("/**\nhello\n*world\n*foo\n*/", 0),
            "///hello\n///world\n///foo"
        );
    }
}
