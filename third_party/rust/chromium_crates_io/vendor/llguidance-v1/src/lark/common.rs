// based on https://github.com/lark-parser/lark/blob/24f19a35f376b9320d53f4d987793fb8b1765f37/lark/grammars/common.lark

use anyhow::Result;

const COMMON_REGEX: &[(&str, &str)] = &[
    ("common.DIGIT", r#"[0-9]"#),
    ("common.HEXDIGIT", r#"[a-fA-F0-9]"#),
    ("common.INT", r#"[0-9]+"#),
    ("common.SIGNED_INT", r#"(\+|-)?[0-9]+"#),
    ("common.DECIMAL", r#"([0-9]+\.[0-9]*)|(\.[0-9]+)"#),
    ("common._EXP", r#"[eE](\+|-)?[0-9]+"#),
    (
        "common.FLOAT",
        r#"([0-9]+\.[0-9]*|\.[0-9]+)([eE](\+|-)?[0-9]+)?|[0-9]+[eE](\+|-)?[0-9]+"#,
    ),
    (
        "common.SIGNED_FLOAT",
        r#"(\+|-)?(([0-9]+\.[0-9]*|\.[0-9]+)([eE](\+|-)?[0-9]+)?|[0-9]+[eE](\+|-)?[0-9]+)"#,
    ),
    (
        "common.NUMBER",
        r#"([0-9]+)|([0-9]+\.[0-9]*|\.[0-9]+)([eE](\+|-)?[0-9]+)?|[0-9]+[eE](\+|-)?[0-9]+"#,
    ),
    (
        "common.SIGNED_NUMBER",
        r#"(\+|-)?(([0-9]+)|([0-9]+\.[0-9]*|\.[0-9]+)([eE](\+|-)?[0-9]+)?|[0-9]+[eE](\+|-)?[0-9]+)"#,
    ),
    ("common.ESCAPED_STRING", r#"\"([^\"\\]|\\.)*\""#),
    ("common.LCASE_LETTER", r#"[a-z]"#),
    ("common.UCASE_LETTER", r#"[A-Z]"#),
    ("common.LETTER", r#"[A-Za-z]"#),
    ("common.WORD", r#"[A-Za-z]+"#),
    ("common.CNAME", r#"[_A-Za-z][_A-Za-z0-9]*"#),
    ("common.WS_INLINE", r#"[ \t]+"#),
    ("common.WS", r#"[ \t\f\r\n]+"#),
    ("common.CR", r#"\r"#),
    ("common.LF", r#"\n"#),
    ("common.NEWLINE", r#"(\r?\n)+"#),
    ("common.SH_COMMENT", r#"#[^\n]*"#),
    ("common.CPP_COMMENT", r#"//[^\n]*"#),
    ("common.C_COMMENT", r#"\/\*[^*]*\*+(?:[^/*][^*]*\*+)*\/"#),
    ("common.SQL_COMMENT", r#"--[^\n]*"#),
];

pub fn lookup_common_regex(name: &str) -> Result<&str> {
    COMMON_REGEX
        .iter()
        .find_map(|(n, r)| if *n == name { Some(*r) } else { None })
        .ok_or_else(|| {
            anyhow::anyhow!(
                "Unknown common regex: {}; following are available: {}",
                name,
                COMMON_REGEX
                    .iter()
                    .map(|(n, _)| *n)
                    .collect::<Vec<_>>()
                    .join(", ")
            )
        })
}
