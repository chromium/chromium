/*!
Routines specific to the reverse suffix optimization.
*/

use alloc::vec::Vec;

use regex_syntax::hir::{literal::Literal, Hir, HirKind, Repetition};

use crate::meta::prefix;

/// Returns true when it's impossible for an earlier match to be detected after
/// a literal candidate (corresponding to `suffix`) has been found.
///
/// Specifically, that there is no earlier match than what a reverse scan of
/// `hirs` after a match of `suffix` reports.
///
/// At present, this always returns `false` when `hirs` has any length except
/// `1`. That is, this optimization does not apply to multi-regex.
pub(super) fn has_no_earlier_match(hirs: &[&Hir], suffix: &[u8]) -> bool {
    if hirs.len() != 1 || suffix.is_empty() {
        return false;
    }
    let hir = hirs[0];
    let Some(prefix) = strip_literal_suffix(hir, suffix) else { return false };

    let fixed_length = prefix::hir_has_fixed_length(&prefix);
    debug!("reverse suffix has fixed length prefix? {fixed_length}");
    if fixed_length {
        return true;
    }

    let class_separator = prefix::has_disjoint_class_separator(
        &prefix,
        &[Literal::inexact(suffix)],
    );
    debug!("reverse suffix has disjoint class separator? {class_separator}");
    if class_separator {
        return true;
    }

    let internal = prefix::hir_can_contain_literal(&prefix, suffix);
    debug!("reverse suffix has internal suffix? {internal}");
    if !internal {
        return true;
    }

    // We couldn't prove that the reverse suffix optimization
    // was safe, so bail out.
    false
}

/// Strip `suffix` from the end of `hir` and return the HIR that remains.
///
/// This is conservative. It returns `None` when the structure of the HIR does
/// not make the suffix straightforward to remove, even when `suffix` might be
/// required by the pattern.
fn strip_literal_suffix(hir: &Hir, suffix: &[u8]) -> Option<Hir> {
    let (prefix, suffix_cursor) =
        strip_literal_suffix_at(hir, suffix, suffix.len())?;
    if suffix_cursor == 0 {
        Some(prefix)
    } else {
        None
    }
}

/// Strip a terminal part of `suffix[..suffix_cursor]` from the end of `hir`.
///
/// The returned cursor marks the end of the part of `suffix` that remains to
/// be stripped. Thus, this call removes `suffix[after..suffix_cursor]`, where
/// `after` is the returned cursor. A non-zero cursor may be returned only when
/// the returned HIR cannot consume any bytes. This lets a concatenation
/// continue stripping from its preceding child without losing adjacency.
fn strip_literal_suffix_at(
    hir: &Hir,
    suffix: &[u8],
    suffix_cursor: usize,
) -> Option<(Hir, usize)> {
    if suffix_cursor == 0 {
        return Some((hir.clone(), 0));
    }
    match hir.kind() {
        HirKind::Literal(lit) => {
            let bytes = &lit.0;
            let mut len = 0;
            while len < bytes.len()
                && len < suffix_cursor
                && bytes[bytes.len() - len - 1]
                    == suffix[suffix_cursor - len - 1]
            {
                len += 1;
            }
            if len == 0 || (len < bytes.len() && len < suffix_cursor) {
                return None;
            }
            Some((
                Hir::literal(bytes[..bytes.len() - len].to_vec()),
                suffix_cursor - len,
            ))
        }
        HirKind::Capture(capture) => {
            strip_literal_suffix_at(&capture.sub, suffix, suffix_cursor)
        }
        HirKind::Concat(hirs) => {
            let mut prefix = hirs.to_vec();
            let mut cursor = suffix_cursor;
            for i in (0..prefix.len()).rev() {
                let (stripped, after) =
                    strip_literal_suffix_at(&prefix[i], suffix, cursor)?;
                if after == cursor {
                    return None;
                }
                prefix[i] = stripped;
                cursor = after;
                if cursor == 0 {
                    break;
                }
            }
            Some((Hir::concat(prefix), cursor))
        }
        HirKind::Repetition(rep) => {
            strip_repetition_literal_suffix(rep, suffix, suffix_cursor)
        }
        _ => None,
    }
}

fn strip_repetition_literal_suffix(
    rep: &Repetition,
    suffix: &[u8],
    suffix_cursor: usize,
) -> Option<(Hir, usize)> {
    let mut min = rep.min;
    let mut max = rep.max;
    let mut cursor = suffix_cursor;
    let mut tail = Vec::new();
    while cursor > 0 {
        if min == 0 {
            return None;
        }
        let before = cursor;
        let (stripped, after) =
            strip_literal_suffix_at(&rep.sub, suffix, cursor)?;
        if after == before {
            return None;
        }
        min = min.saturating_sub(1);
        max = max.map(|max| max.saturating_sub(1));
        if !matches!(stripped.kind(), HirKind::Empty) {
            tail.push(stripped);
        }
        cursor = after;
    }

    let rest = Hir::repetition(Repetition {
        min,
        max,
        greedy: rep.greedy,
        sub: rep.sub.clone(),
    });
    let mut prefix = Vec::with_capacity(tail.len() + 1);
    if !matches!(rest.kind(), HirKind::Empty) {
        prefix.push(rest);
    }
    tail.reverse();
    prefix.extend(tail);
    Some((Hir::concat(prefix), 0))
}

// We only test when we have `unicode-perl` here since some regexes require
// that.
#[cfg(all(feature = "unicode-perl", not(miri),))]
#[cfg(test)]
mod tests {
    use crate::util::syntax;

    use super::*;

    #[track_caller]
    fn assert_strip(pattern: &str, suffix: &str, expected: Option<&str>) {
        let hir = syntax::parse(pattern).unwrap();
        let got = strip_literal_suffix(&hir, suffix.as_bytes());
        let expected = expected.map(|pattern| syntax::parse(pattern).unwrap());
        assert_eq!(expected, got);
    }

    #[test]
    fn literal() {
        assert_strip("foobar", "bar", Some("foo"));
        assert_strip("foobar", "foobar", Some(""));
        assert_strip("foobar", "", Some("foobar"));
    }

    #[test]
    fn capture() {
        assert_strip(r".+foo(bar)", "foobar", Some(r".+"));
    }

    #[test]
    fn concat() {
        assert_strip(r".+(?:ab){2}cd", "ababcd", Some(r".+"));
        assert_strip(r"a(?:)cd", "acd", Some(r""));
        assert_strip(r"az{0}cd", "acd", Some(r""));
        assert_strip(r"az{2}cd", "acd", None);
        assert_strip(r"az{0,2}cd", "acd", None);
    }

    #[test]
    fn repetition() {
        assert_strip(r"x(?:ab){3}", "bab", Some("xaba"));
        assert_strip(r"x(?:ab){2,}", "bab", Some(r"x(?:ab)*a"));
    }

    #[test]
    fn mismatch() {
        assert_strip("foobar", "baz", None);
        assert_strip("foobar", "xfoobar", None);
        assert_strip(r"foo[a-z]", "a", None);
    }

    #[test]
    fn suffix_must_be_adjacent() {
        assert_strip(r"foo(xbar)", "foobar", None);
    }

    #[test]
    fn repetition_must_be_required() {
        assert_strip(r"x(?:ab)*cd", "abcd", None);
    }

    #[track_caller]
    fn assert_fixed_length_prefix(yes: bool, pattern: &str, suffix: &[u8]) {
        let hir = syntax::parse(pattern).unwrap();
        assert_eq!(
            yes,
            strip_literal_suffix(&hir, suffix)
                .map_or(false, |hir| prefix::hir_has_fixed_length(&hir)),
        );
    }

    #[track_caller]
    fn assert_internal_suffix(yes: bool, pattern: &str, suffix: &[u8]) {
        let hir = syntax::parse(pattern).unwrap();
        assert_eq!(
            yes,
            strip_literal_suffix(&hir, suffix).map_or(false, |hir| {
                prefix::hir_can_contain_literal(&hir, suffix)
            }),
        );
    }

    #[test]
    fn reverse_suffix_accepts_prefix_without_internal_suffix() {
        assert_internal_suffix(false, r"\d+XYZ", b"XYZ");
        assert_internal_suffix(false, r"[a-q][^u-z]{13}x", b"x");
    }

    #[test]
    fn reverse_suffix_accepts_fixed_length_prefix() {
        assert_fixed_length_prefix(true, r"(?:ab|cd)XYZ", b"XYZ");
        assert_internal_suffix(true, r"[A-Z][0-9]XYZ", b"XYZ");
        assert_fixed_length_prefix(true, r"[A-Z][0-9]XYZ", b"XYZ");
    }

    #[test]
    fn reverse_suffix_fixed_length_prefix_is_conservative() {
        assert_fixed_length_prefix(false, r"a{1,3}yy", b"yy");
        assert_fixed_length_prefix(false, r"a*yy", b"yy");
    }
}
