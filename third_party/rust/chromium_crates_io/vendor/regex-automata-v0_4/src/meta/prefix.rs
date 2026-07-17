/*!
Cheap HIR proofs that literal candidate order is compatible with regex match
order.

The reverse suffix and reverse inner strategies both scan for a required
literal before confirming the surrounding regex. These proofs are all
conservative. If they cannot establish that the first confirmed candidate is
the match the regex engine would report, then the corresponding reverse
strategy is not used.
*/

use regex_syntax::hir::{literal::Literal, Class, Hir, HirKind};

/// Return true when `hir` can match some string containing every byte in
/// `lit`.
///
/// This is deliberately low precision. If every byte in a literal can be
/// consumed somewhere in the HIR, this gives up and reports that the
/// literal might occur internally. That is, this returning true does not
/// necessarily mean that the `Hir` provided definitively matches `lit`.
pub(super) fn hir_can_contain_literal(hir: &Hir, lit: &[u8]) -> bool {
    lit.is_empty() || lit.iter().all(|&byte| hir_can_consume_byte(hir, byte))
}

/// Returns true when the given `Hir` has a fixed length.
///
/// That is, when its minimum and maximum lengths are both finite and
/// equivalent.
pub(super) fn hir_has_fixed_length(hir: &Hir) -> bool {
    let props = hir.properties();
    props
        .minimum_len()
        .and_then(|min| props.maximum_len().map(|max| min == max))
        .unwrap_or(false)
}

/// Return true when an edge of `prefix` provides a required separator between
/// the start of a match and each literal candidate.
///
/// More precisely, this looks at the first and last consuming children of a
/// top-level concatenation. One of those children must be either a character
/// class or a repetition of a character class that matches at least once. Let
/// that child be `S`. This returns true when both of the following hold:
///
/// * `S` is disjoint from everything consumed by the other children in
///   `prefix`.
/// * `S` is disjoint from every literal in `literals`.
///
/// In that case, `S` acts as a separator whose characters cannot be mistaken
/// for characters belonging to either the rest of the prefix or a literal
/// candidate. A reverse search therefore cannot slide `S` across either one
/// and produce a later match start from an earlier literal candidate.
///
/// For example, consider this prefix and literal:
///
/// ```text
/// prefix  = \w+\s+
/// literal = Holmes
/// ```
///
/// The trailing `\s+` is required, is disjoint from `\w+` and cannot match
/// anything in `Holmes`. Thus this proves the reverse suffix optimization safe
/// for `\w+\s+Holmes`. It also works with multiple literals, such as `Holmes`
/// and `Watson`, provided the separator is disjoint from all of them.
///
/// The separator may instead be the first consuming child:
///
/// ```text
/// prefix  = \s[A-Za-z]{0,12}
/// literal = ing
/// ```
///
/// Here, the leading `\s` is disjoint from both `[A-Za-z]{0,12}` and `ing`.
///
/// This returns false when the possible separator is optional, overlaps
/// another prefix component or can match a character in any literal. For
/// example, neither `\s*` in `[A-Za-z]*\s*` nor `\w+` in `\w+\w+` provides
/// the required separator.
pub(super) fn has_disjoint_class_separator(
    prefix: &Hir,
    literals: &[Literal],
) -> bool {
    let hirs = match uncapture(prefix).kind() {
        HirKind::Concat(hirs) if hirs.len() >= 2 => hirs,
        _ => return false,
    };
    let Some(first) = hirs.iter().position(|hir| !hir_matches_empty_only(hir))
    else {
        return false;
    };
    let last =
        hirs.iter().rposition(|hir| !hir_matches_empty_only(hir)).unwrap();
    has_disjoint_class_separator_at(hirs, last, literals)
        || (first != last
            && has_disjoint_class_separator_at(hirs, first, literals))
}

fn has_disjoint_class_separator_at(
    hirs: &[Hir],
    separator: usize,
    literals: &[Literal],
) -> bool {
    let Some(separator_class) = required_class(&hirs[separator]) else {
        return false;
    };
    class_is_disjoint_from_literals(separator_class, literals)
        && hirs.iter().enumerate().all(|(i, hir)| {
            i == separator || hir_is_disjoint_from_class(hir, separator_class)
        })
}

fn hir_matches_empty_only(hir: &Hir) -> bool {
    hir.properties().maximum_len() == Some(0)
}

fn required_class(hir: &Hir) -> Option<&Class> {
    let hir = uncapture(hir);
    match hir.kind() {
        HirKind::Class(cls) => Some(cls),
        HirKind::Repetition(rep) if rep.min > 0 => {
            match uncapture(&rep.sub).kind() {
                HirKind::Class(cls) => Some(cls),
                _ => None,
            }
        }
        _ => None,
    }
}

fn hir_is_disjoint_from_class(hir: &Hir, separator: &Class) -> bool {
    match hir.kind() {
        HirKind::Empty | HirKind::Look(_) => true,
        HirKind::Literal(lit) => {
            class_is_disjoint_from_literal(separator, &lit.0)
        }
        HirKind::Class(cls) => classes_are_disjoint(cls, separator),
        HirKind::Repetition(rep) => {
            hir_is_disjoint_from_class(&rep.sub, separator)
        }
        HirKind::Capture(capture) => {
            hir_is_disjoint_from_class(&capture.sub, separator)
        }
        HirKind::Concat(hirs) | HirKind::Alternation(hirs) => {
            hirs.iter().all(|hir| hir_is_disjoint_from_class(hir, separator))
        }
    }
}

fn uncapture(mut hir: &Hir) -> &Hir {
    while let HirKind::Capture(capture) = hir.kind() {
        hir = &capture.sub;
    }
    hir
}

fn classes_are_disjoint(left: &Class, right: &Class) -> bool {
    match (left, right) {
        (Class::Bytes(left), Class::Bytes(right)) => {
            let (mut intersection, other) =
                if left.ranges().len() <= right.ranges().len() {
                    (left.clone(), right)
                } else {
                    (right.clone(), left)
                };
            intersection.intersect(other);
            intersection.ranges().is_empty()
        }
        (Class::Unicode(left), Class::Unicode(right)) => {
            let (mut intersection, other) =
                if left.ranges().len() <= right.ranges().len() {
                    (left.clone(), right)
                } else {
                    (right.clone(), left)
                };
            intersection.intersect(other);
            intersection.ranges().is_empty()
        }
        _ => false,
    }
}

fn class_is_disjoint_from_literals(cls: &Class, literals: &[Literal]) -> bool {
    literals
        .iter()
        .all(|lit| class_is_disjoint_from_literal(cls, lit.as_bytes()))
}

fn class_is_disjoint_from_literal(cls: &Class, lit: &[u8]) -> bool {
    match cls {
        Class::Bytes(cls) => {
            lit.iter().all(|&byte| !byte_class_contains(cls, byte))
        }
        Class::Unicode(cls) => core::str::from_utf8(lit)
            .map_or(false, |lit| {
                lit.chars().all(|ch| !unicode_class_contains(cls, ch))
            }),
    }
}

fn hir_can_consume_byte(hir: &Hir, byte: u8) -> bool {
    match hir.kind() {
        HirKind::Empty | HirKind::Look(_) => false,
        HirKind::Literal(lit) => lit.0.contains(&byte),
        HirKind::Class(Class::Bytes(cls)) => byte_class_contains(cls, byte),
        HirKind::Class(Class::Unicode(cls)) => {
            // We don't check literals based on codepoints, so if
            // we have a non-ASCII byte, we assume here that any
            // Unicode class will match it. In practice, given
            // the prevalence of `\w`, this is not a terrible
            // approximation.
            if byte > 0x7F {
                return true;
            }
            let ch = char::from(byte);
            unicode_class_contains(cls, ch)
        }
        HirKind::Repetition(rep) => hir_can_consume_byte(&rep.sub, byte),
        HirKind::Capture(capture) => hir_can_consume_byte(&capture.sub, byte),
        HirKind::Concat(hirs) | HirKind::Alternation(hirs) => {
            hirs.iter().any(|hir| hir_can_consume_byte(hir, byte))
        }
    }
}

fn byte_class_contains(cls: &regex_syntax::hir::ClassBytes, byte: u8) -> bool {
    cls.ranges()
        .iter()
        .any(|range| range.start() <= byte && byte <= range.end())
}

fn unicode_class_contains(
    cls: &regex_syntax::hir::ClassUnicode,
    ch: char,
) -> bool {
    cls.ranges().iter().any(|range| range.start() <= ch && ch <= range.end())
}
