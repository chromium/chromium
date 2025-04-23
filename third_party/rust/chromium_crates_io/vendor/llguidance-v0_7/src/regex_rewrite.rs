fn class_for(c: char) -> Option<&'static str> {
    match c {
        'd' => Some("0-9"),
        'w' => Some("0-9a-zA-Z_"),
        's' => Some(" \\t\\n\\r\\f\\v"),
        _ => None,
    }
}

/// Make sure given regex can be used inside /.../ in Lark syntax.
/// Also if `use_ascii.contains('d')` replace `\d` with `[0-9]` and `\D` with `[^0-9]`.
/// Similarly for `\w`/`\W` (`[0-9a-zA-Z_]`) and `\s`/`\S` (`[ \t\n\r\f\v]`).
/// For standard Unicode Python3 or Rust regex crate semantics `use_ascii = ""`
/// For JavaScript or JSON Schema semantics `use_ascii = "dw"`
/// For Python2 or byte patters in Python3 semantics `use_ascii = "dws"`
/// More flags may be added in future.
pub fn regex_to_lark(rx: &str, use_ascii: &str) -> String {
    let mut is_q = false;
    let mut res = String::new();
    for c in rx.chars() {
        let prev_q = is_q;
        is_q = false;
        match c {
            // make sure we don't terminate on /
            '/' => res.push_str("\\/"),

            // these are optional, but nice
            '\n' => res.push_str("\\n"),
            '\r' => res.push_str("\\r"),
            '\t' => res.push_str("\\t"),

            '\\' if !prev_q => {
                is_q = true;
            }

            'd' | 'w' | 's' | 'D' | 'W' | 'S' if prev_q => {
                let c2 = c.to_ascii_lowercase();
                if use_ascii.contains(c2) {
                    let class = class_for(c2).unwrap();
                    res.push('[');
                    if c != c2 {
                        res.push('^');
                    }
                    res.push_str(class);
                    res.push(']');
                } else {
                    res.push('\\');
                    res.push(c);
                }
            }

            _ => {
                if prev_q {
                    res.push('\\');
                }
                res.push(c);
            }
        }
    }
    res
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_digit_conversion_with_ascii() {
        // \d => [0-9], \D => [^0-9]
        assert_eq!(regex_to_lark(r"\d", "d"), "[0-9]");
        assert_eq!(regex_to_lark(r"\D", "d"), "[^0-9]");
    }

    #[test]
    fn test_word_conversion_with_ascii() {
        // Only convert if use_ascii contains corresponding letter.
        assert_eq!(regex_to_lark(r"\w", "w"), "[0-9a-zA-Z_]");
        assert_eq!(regex_to_lark(r"\W", "w"), "[^0-9a-zA-Z_]");
    }

    #[test]
    fn test_space_conversion_with_ascii() {
        // \s and \S should convert accordingly.
        assert_eq!(regex_to_lark(r"\s", "s"), "[ \\t\\n\\r\\f\\v]");
        assert_eq!(regex_to_lark(r"\S", "s"), "[^ \\t\\n\\r\\f\\v]");
    }

    #[test]
    fn test_no_conversion_when_missing_in_use_ascii() {
        // If the ascii flag doesn't contain the letter, leave escape as-is.
        assert_eq!(regex_to_lark(r"\d", ""), r"\d");
        assert_eq!(regex_to_lark(r"\w", "d"), r"\w");
    }

    #[test]
    fn test_escaped_slashes_and_whitespace() {
        // '/' should be escaped; newline, tab, carriage return are escaped.
        let input = "/a\nb\rc\td";
        let expected = r"\/a\nb\rc\td";
        assert_eq!(regex_to_lark(input, "dws"), expected);
    }

    #[test]
    fn test_combined_conversions() {
        // Combined sequence with all conversions.
        let input = r"\d\w\s\D\W\S";
        let expected = "[0-9][0-9a-zA-Z_][ \\t\\n\\r\\f\\v][^0-9][^0-9a-zA-Z_][^ \\t\\n\\r\\f\\v]";
        assert_eq!(regex_to_lark(input, "dws"), expected);
    }

    #[test]
    fn test_miscellaneous_escapes() {
        // \X and \@ are not recognized as special, so they should pass through.
        assert_eq!(regex_to_lark(r"\X", ""), r"\X");
        assert_eq!(regex_to_lark(r"\@", ""), r"\@");

        // Forward slash is escaped.
        assert_eq!(regex_to_lark(r"/", ""), r"\/");
        assert_eq!(regex_to_lark(r"\/", ""), r"\/");
        assert_eq!(regex_to_lark(r"\//", ""), r"\/\/");
        assert_eq!(regex_to_lark(r"/\//", ""), r"\/\/\/");

        // Double backslash should be preserved.
        assert_eq!(regex_to_lark(r"\\", ""), r"\\");

        // Quotes should pass through unchanged.
        assert_eq!(regex_to_lark("\"", ""), "\"");
        assert_eq!(regex_to_lark(r#"a"b"#, ""), r#"a"b"#);
    }
}
