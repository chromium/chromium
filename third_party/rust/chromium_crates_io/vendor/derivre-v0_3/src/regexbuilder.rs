use std::fmt::Debug;

use crate::HashMap;
use anyhow::{ensure, Result};
use regex_syntax::ParserBuilder;

use crate::{
    ast::{
        byteset_256, byteset_clear, byteset_contains, byteset_from_range, byteset_set, Expr,
        ExprSet,
    },
    mapper::map_ast,
    pp::{byte_to_string, byteset_to_string},
    simplify::ConcatElement,
    ExprRef, Regex,
};

#[derive(Clone)]
pub struct RegexBuilder {
    parser_builder: ParserBuilder,
    exprset: ExprSet,
    json_quote_cache: HashMap<ExprRef, ExprRef>,
}

#[derive(Clone, Debug)]
pub struct JsonQuoteOptions {
    /// Which escapes to allow (after \).
    /// Represents a set of bytes. Allowed bytes:
    /// n, r, b, t, f, \, ", u
    /// Note that 'u' allows the \uXXXX form only for ASCII control
    /// characters, not general Unicode, in particular for characters
    /// \u0000-\u001F and \u007F (if they are allowed by the regex).
    pub allowed_escapes: String,

    /// When set, "..." will not be added around the final regular expression.
    pub raw_mode: bool,
}

impl JsonQuoteOptions {
    pub fn no_unicode_raw() -> Self {
        Self {
            // \uXXXX not allowed
            allowed_escapes: "nrbtf\\\"".to_string(),
            raw_mode: true,
        }
    }

    pub fn with_unicode_raw() -> Self {
        Self {
            // allow \uXXXX
            allowed_escapes: "nrbtf\\\"u".to_string(),
            raw_mode: true,
        }
    }

    pub fn regular() -> Self {
        Self {
            // allow \uXXXX
            allowed_escapes: "nrbtf\\\"u".to_string(),
            raw_mode: false,
        }
    }

    pub fn is_allowed(&self, b: u8) -> bool {
        self.allowed_escapes.as_bytes().contains(&b)
    }

    pub fn set_if_allowed(&self, bs: &mut [u32], b: u8) {
        if self.is_allowed(b) {
            byteset_set(bs, b as usize);
        }
    }
}

#[derive(Clone)]
pub enum RegexAst {
    /// Intersection of the regexes
    And(Vec<RegexAst>),
    /// Union of the regexes
    Or(Vec<RegexAst>),
    /// Concatenation of the regexes
    Concat(Vec<RegexAst>),
    /// Matches the regex; should be at the end of the main regex.
    /// The length of the lookahead can be recovered from the engine.
    LookAhead(Box<RegexAst>),
    /// Matches everything the regex doesn't match.
    /// Can lead to invalid utf8.
    Not(Box<RegexAst>),
    /// Repeat the regex at least min times, at most max times
    /// u32::MAX means infinity
    Repeat(Box<RegexAst>, u32, u32),
    /// MultipleOf(d, s) matches if the input, interpreted as decimal ASCII number, is a multiple of d*10^-s.
    /// EmptyString is not included.
    MultipleOf(u32, u32),
    /// Matches the empty string. Same as Concat([]).
    EmptyString,
    /// Matches nothing. Same as Or([]).
    NoMatch,
    /// Compile the regex using the regex_syntax crate.
    /// This assumes the regex is implicitly anchored.
    /// It allows ^$ only at the beginning and end of the regex.
    Regex(String),
    /// Compile the regex using the regex_syntax crate, but do not assume it's anchored.
    /// This will add (.*) to the beginning and end of the regex if it doesn't already have
    /// anchors.
    SearchRegex(String),
    /// Matches this string only
    Literal(String),
    /// Matches this string of bytes only. Can lead to invalid utf8.
    ByteLiteral(Vec<u8>),
    /// Matches this byte only. If byte is not in 0..127, it may lead to invalid utf8.
    Byte(u8),
    /// Matches any byte in the set, expressed as bitset.
    /// Can lead to invalid utf8 if the set is not a subset of 0..127
    ByteSet(Vec<u32>),
    /// Quote the regex as a JSON string.
    /// For example, [A-Z\n]+ becomes ([A-Z]|\\n)+
    JsonQuote(Box<RegexAst>, JsonQuoteOptions),
    /// Reference previously built regex
    ExprRef(ExprRef),
}

impl RegexAst {
    /// Regex is empty iff self ⊆ big
    pub fn contained_in(&self, big: &RegexAst) -> RegexAst {
        let small = self;
        RegexAst::And(vec![small.clone(), RegexAst::Not(Box::new(big.clone()))])
    }

    pub fn get_args(&self) -> &[RegexAst] {
        match self {
            RegexAst::And(asts) | RegexAst::Or(asts) | RegexAst::Concat(asts) => asts,
            RegexAst::LookAhead(ast)
            | RegexAst::Not(ast)
            | RegexAst::Repeat(ast, _, _)
            | RegexAst::JsonQuote(ast, _) => std::slice::from_ref(ast),
            RegexAst::EmptyString
            | RegexAst::MultipleOf(_, _)
            | RegexAst::NoMatch
            | RegexAst::Regex(_)
            | RegexAst::SearchRegex(_)
            | RegexAst::Literal(_)
            | RegexAst::ByteLiteral(_)
            | RegexAst::ExprRef(_)
            | RegexAst::Byte(_)
            | RegexAst::ByteSet(_) => &[],
        }
    }

    pub fn tag(&self) -> &'static str {
        match self {
            RegexAst::And(_) => "And",
            RegexAst::Or(_) => "Or",
            RegexAst::Concat(_) => "Concat",
            RegexAst::LookAhead(_) => "LookAhead",
            RegexAst::Not(_) => "Not",
            RegexAst::EmptyString => "EmptyString",
            RegexAst::NoMatch => "NoMatch",
            RegexAst::Regex(_) => "Regex",
            RegexAst::SearchRegex(_) => "SearchRegex",
            RegexAst::Literal(_) => "Literal",
            RegexAst::ByteLiteral(_) => "ByteLiteral",
            RegexAst::ExprRef(_) => "ExprRef",
            RegexAst::Repeat(_, _, _) => "Repeat",
            RegexAst::Byte(_) => "Byte",
            RegexAst::ByteSet(_) => "ByteSet",
            RegexAst::MultipleOf(_, _) => "MultipleOf",
            RegexAst::JsonQuote(_, _) => "JsonQuote",
        }
    }

    pub fn write_to_str(&self, dst: &mut String, max_len: usize, exprset: Option<&ExprSet>) {
        let mut todo = vec![Some(self)];
        while let Some(ast) = todo.pop() {
            if dst.len() >= max_len {
                dst.push_str("...");
                break;
            }
            if ast.is_none() {
                dst.push(')');
                continue;
            }
            let ast = ast.unwrap();
            dst.push_str(" (");
            dst.push_str(ast.tag());
            todo.push(None);
            match ast {
                RegexAst::And(_)
                | RegexAst::Or(_)
                | RegexAst::Concat(_)
                | RegexAst::LookAhead(_)
                | RegexAst::Not(_) => {}
                RegexAst::Byte(b) => {
                    dst.push(' ');
                    dst.push_str(&byte_to_string(*b));
                }
                RegexAst::ByteSet(bs) => {
                    dst.push(' ');
                    if bs.len() == 256 / 32 {
                        dst.push_str(&byteset_to_string(bs));
                    } else {
                        dst.push_str(&format!("invalid byteset len: {}", bs.len()))
                    }
                }
                RegexAst::SearchRegex(s) | RegexAst::Regex(s) => {
                    dst.push(' ');
                    write_regex(dst, s);
                }
                RegexAst::Literal(s) => {
                    dst.push_str(&format!(" {:?}", s));
                }
                RegexAst::ByteLiteral(s) => {
                    dst.push_str(&format!(" {:?}", String::from_utf8_lossy(s)));
                }
                RegexAst::ExprRef(r) => {
                    if let Some(es) = exprset {
                        let e_len = max_len.saturating_sub(dst.len());
                        dst.push_str(&format!(" {}", es.expr_to_string_max_len(*r, e_len)));
                    } else {
                        dst.push_str(&format!(" {}", r.as_usize()));
                    }
                }
                RegexAst::Repeat(_, min, max) => {
                    dst.push_str(&format!("{{{},{}}} ", min, max));
                }
                RegexAst::MultipleOf(d, s) => {
                    if *s == 0 {
                        dst.push_str(&format!(" % {} == 0 ", d));
                    } else {
                        dst.push_str(&format!(" % {}x10^-{} == 0", d, s));
                    }
                }
                RegexAst::JsonQuote(_, opts) => {
                    dst.push_str(&format!(" {:?}", opts));
                }
                RegexAst::EmptyString | RegexAst::NoMatch => {}
            }
            for c in ast.get_args().iter().rev() {
                todo.push(Some(c));
            }
        }
    }
}

pub(crate) fn write_regex(dst: &mut String, regex: &str) {
    dst.push('/');
    let mut escaped = false;
    for c in regex.chars() {
        match c {
            '\\' if !escaped => {
                escaped = true;
                continue;
            }
            '/' => {
                dst.push('\\');
                dst.push(c);
            }
            '\n' => {
                dst.push_str("\\n");
            }
            '\r' => {
                dst.push_str("\\r");
            }
            '\t' => {
                dst.push_str("\\t");
            }
            _ => {
                if c < ' ' {
                    dst.push_str(&format!("\\x{:02X}", c as u32));
                } else {
                    if escaped {
                        dst.push('\\');
                    }
                    dst.push(c);
                }
            }
        }
        escaped = false;
    }
    if escaped {
        dst.push_str("\\\\");
    }
    dst.push('/');
}

impl Debug for RegexAst {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut s = String::new();
        self.write_to_str(&mut s, 512, None);
        write!(f, "{}", s)
    }
}

impl Default for RegexBuilder {
    fn default() -> Self {
        Self::new()
    }
}

impl RegexBuilder {
    pub fn new() -> Self {
        Self {
            parser_builder: ParserBuilder::new(),
            exprset: ExprSet::new(256),
            json_quote_cache: HashMap::default(),
        }
    }

    pub fn to_regex_limited(&self, r: ExprRef, max_fuel: u64) -> Result<Regex> {
        Regex::new_with_exprset(self.exprset.clone(), r, max_fuel)
    }

    pub fn to_regex(&self, r: ExprRef) -> Regex {
        Regex::new_with_exprset(self.exprset.clone(), r, u64::MAX).unwrap()
    }

    pub fn into_regex_limited(self, r: ExprRef, max_fuel: u64) -> Result<Regex> {
        Regex::new_with_exprset(self.exprset, r, max_fuel)
    }

    pub fn into_regex(self, r: ExprRef) -> Regex {
        Regex::new_with_exprset(self.exprset, r, u64::MAX).unwrap()
    }

    pub fn exprset(&self) -> &ExprSet {
        &self.exprset
    }

    pub fn into_exprset(self) -> ExprSet {
        self.exprset
    }

    pub fn reserve(&mut self, size: usize) {
        self.exprset.reserve(size);
    }

    pub fn json_quote(&mut self, e: ExprRef, options: &JsonQuoteOptions) -> Result<ExprRef> {
        // returns Some(X) iff b should quoted as \X
        fn quote(b: u8) -> Option<u8> {
            match b {
                b'\\' => Some(b'\\'),
                b'"' => Some(b'"'),
                0x08 => Some(b'b'),
                0x0C => Some(b'f'),
                b'\n' => Some(b'n'),
                b'\r' => Some(b'r'),
                b'\t' => Some(b't'),
                _ => None,
            }
        }

        // byteset of all possible single-char quotes
        fn single_quote_byteset(include_nl: bool, options: &JsonQuoteOptions) -> Vec<u32> {
            let mut quoted_bs = byteset_256();
            for c in b"\"\\bfrt" {
                options.set_if_allowed(&mut quoted_bs, *c);
            }
            if include_nl {
                options.set_if_allowed(&mut quoted_bs, b'n');
            }
            quoted_bs
        }

        // all hex digits, including A or not
        fn hex_byteset(include_nl: bool) -> Vec<u32> {
            let mut hex_bs = byteset_256();
            for c in b"0123456789bcdefBCDEF" {
                byteset_set(&mut hex_bs, *c as usize);
            }
            if include_nl {
                byteset_set(&mut hex_bs, b'A' as usize);
                byteset_set(&mut hex_bs, b'a' as usize);
            }
            hex_bs
        }

        // all control characters, including \n or not
        fn quote_all_ctrl(
            exprset: &mut ExprSet,
            include_nl: bool,
            options: &JsonQuoteOptions,
        ) -> ExprRef {
            let upref = exprset.mk_literal("u00");
            let backslash = exprset.mk_byte(b'\\');
            let single_quote = exprset.mk_byte_set(&single_quote_byteset(include_nl, options));
            let u0000 = if !options.is_allowed(b'u') {
                ExprRef::NO_MATCH
            } else if include_nl {
                let hex0 = exprset.mk_byte_set(&byteset_from_range(b'0', b'1'));
                let hex1 = exprset.mk_byte_set(&hex_byteset(include_nl));
                exprset.mk_concat_vec(&[upref, hex0, hex1])
            } else {
                let n0 = exprset.mk_byte(b'0');
                let n1 = exprset.mk_byte(b'1');
                let hex0 = exprset.mk_byte_set(&hex_byteset(false));
                let hex0 = exprset.mk_concat(n0, hex0);
                let hex1 = exprset.mk_byte_set(&hex_byteset(true));
                let hex1 = exprset.mk_concat(n1, hex1);
                let hex01 = exprset.mk_or(&mut vec![hex0, hex1]);
                exprset.mk_concat(upref, hex01)
            };

            let u_or_single = exprset.mk_or(&mut vec![u0000, single_quote]);
            exprset.mk_concat(backslash, u_or_single)
        }

        fn quote_byteset(
            exprset: &mut ExprSet,
            bs: Vec<u32>,
            options: &JsonQuoteOptions,
        ) -> ExprRef {
            let upref = exprset.mk_literal("u00");
            let backslash = exprset.mk_byte(b'\\');

            let quoted = if bs[0] == !(1 << b'\n') {
                // everything except for \n
                quote_all_ctrl(exprset, false, options)
            } else if bs[0] == 0xffff_ffff {
                // everything
                quote_all_ctrl(exprset, true, options)
            } else {
                let mut quoted_bs = byteset_256();
                let mut other_bytes = vec![];
                for b in 0..32 {
                    if byteset_contains(&bs, b) {
                        if let Some(q) = quote(b as u8) {
                            options.set_if_allowed(&mut quoted_bs, q);
                        }
                        if options.is_allowed(b'u') {
                            let other = exprset.mk_literal(&format!("{:02x}", b));
                            other_bytes.push(other);
                            let other = exprset.mk_literal(&format!("{:02X}", b));
                            other_bytes.push(other);
                        }
                    }
                }

                let quoted_bs = exprset.mk_byte_set(&quoted_bs);
                let other_bytes = exprset.mk_or(&mut other_bytes);
                let other_bytes = exprset.mk_concat(upref, other_bytes);

                let quoted_or_other = exprset.mk_or(&mut vec![quoted_bs, other_bytes]);
                exprset.mk_concat(backslash, quoted_or_other)
            };

            let mut bs_without_ctrl = bs;
            bs_without_ctrl[0] = 0;
            let mut alts = vec![quoted];
            if byteset_contains(&bs_without_ctrl, b'\\' as usize) {
                if options.is_allowed(b'\\') {
                    alts.push(exprset.mk_literal("\\\\"));
                }
                byteset_clear(&mut bs_without_ctrl, b'\\' as usize);
            }
            if byteset_contains(&bs_without_ctrl, b'"' as usize) {
                if options.is_allowed(b'"') {
                    alts.push(exprset.mk_literal("\\\""));
                }
                byteset_clear(&mut bs_without_ctrl, b'"' as usize);
            }
            if byteset_contains(&bs_without_ctrl, 0x7F) {
                if options.is_allowed(b'u') {
                    alts.push(exprset.mk_literal("\\u007F"));
                    alts.push(exprset.mk_literal("\\u007f"));
                }
                byteset_clear(&mut bs_without_ctrl, 0x7F);
            }
            let bs_without_ctrl = exprset.mk_byte_set(&bs_without_ctrl);
            alts.push(bs_without_ctrl);
            exprset.mk_or(&mut alts)
        }

        for c in options.allowed_escapes.as_bytes() {
            ensure!(
                b"\"\\bfnrtu".contains(c),
                "invalid escape character in allowed_escapes: {}",
                *c as char
            );
        }

        fn byte_needs_quote(b: u8) -> bool {
            matches!(b, b'\\' | b'"' | 0x7F | 0..0x20)
        }

        let r = self.exprset.map(
            e,
            &mut self.json_quote_cache,
            false,
            |e| e,
            |exprset, args, e| -> ExprRef {
                match exprset.get(e) {
                    Expr::ByteSet(bs) => {
                        let has_bytes_below_0x20 = bs[0] != 0;
                        if has_bytes_below_0x20
                            || byteset_contains(bs, b'\\' as usize)
                            || byteset_contains(bs, b'"' as usize)
                            || byteset_contains(bs, 0x7F)
                        {
                            let bs = bs.to_vec();
                            quote_byteset(exprset, bs, options)
                        } else {
                            // no need to quote
                            e
                        }
                    }
                    Expr::Byte(b) => {
                        if byte_needs_quote(b) {
                            quote_byteset(exprset, byteset_from_range(b, b), options)
                        } else {
                            // no need to quote
                            e
                        }
                    }
                    Expr::ByteConcat(_, bytes, args0) => {
                        if bytes.iter().any(|b| byte_needs_quote(*b)) {
                            let mut acc = vec![];
                            let mut idx = 0;
                            let bytes = bytes.to_vec();
                            while idx < bytes.len() {
                                let idx0 = idx;
                                while idx < bytes.len() && !byte_needs_quote(bytes[idx]) {
                                    idx += 1;
                                }
                                let slice = &bytes[idx0..idx];
                                if !slice.is_empty() {
                                    ConcatElement::Bytes(slice).push_owned_to(&mut acc);
                                }
                                if idx < bytes.len() {
                                    let b = bytes[idx];
                                    let q =
                                        quote_byteset(exprset, byteset_from_range(b, b), options);
                                    ConcatElement::Expr(q).push_owned_to(&mut acc);
                                    idx += 1;
                                }
                            }
                            exprset._mk_concat_vec(acc)
                        } else if args[0] == args0 {
                            e
                        } else {
                            let copy = bytes.to_vec();
                            exprset.mk_byte_concat(&copy, args[0])
                        }
                    }
                    // always identity
                    Expr::EmptyString | Expr::NoMatch | Expr::RemainderIs { .. } => e,
                    // if all args map to themselves, return back the same expression
                    x if x.args() == args => e,
                    // otherwise, actually map the args
                    Expr::And(_, _) => exprset.mk_and(args),
                    Expr::Or(_, _) => exprset.mk_or(args),
                    Expr::Concat(_, _) => exprset.mk_concat(args[0], args[1]),
                    Expr::Not(_, _) => exprset.mk_not(args[0]),
                    Expr::Lookahead(_, _, _) => exprset.mk_lookahead(args[0], 0),
                    Expr::Repeat(_, _, min, max) => exprset.mk_repeat(args[0], min, max),
                }
            },
        );

        let quote = self.exprset.mk_byte(b'"');
        let r = if options.raw_mode {
            r
        } else {
            self.exprset.mk_concat_vec(&[quote, r, quote])
        };
        Ok(r)
    }

    pub fn mk_regex(&mut self, s: &str) -> Result<ExprRef> {
        let parser = self.parser_builder.build();
        self.exprset.parse_expr(parser, s, false)
    }

    pub fn mk_regex_for_serach(&mut self, s: &str) -> Result<ExprRef> {
        let parser = self.parser_builder.build();
        self.exprset.parse_expr(parser, s, true)
    }

    pub fn mk_regex_and(&mut self, s: &[&str]) -> Result<ExprRef> {
        let args = s
            .iter()
            .map(|s| Ok(RegexAst::ExprRef(self.mk_regex(s)?)))
            .collect::<Result<Vec<_>>>()?;
        self.mk(&RegexAst::And(args))
    }

    pub fn mk_contained_in(&mut self, small: &str, big: &str) -> Result<ExprRef> {
        let a = RegexAst::ExprRef(self.mk_regex(small)?);
        let b = RegexAst::ExprRef(self.mk_regex(big)?);
        self.mk(&a.contained_in(&b))
    }

    pub fn mk_contained_in_ast(&mut self, small: &RegexAst, big: &RegexAst) -> Result<ExprRef> {
        let a = RegexAst::ExprRef(self.mk(small)?);
        let b = RegexAst::ExprRef(self.mk(big)?);
        self.mk(&a.contained_in(&b))
    }

    pub fn is_contained_in(&mut self, small: &str, big: &str, max_fuel: u64) -> Result<bool> {
        let r = self.mk_contained_in(small, big)?;
        Ok(self.clone().to_regex_limited(r, max_fuel)?.always_empty())
    }

    pub fn mk_prefix_tree(&mut self, branches: Vec<(Vec<u8>, ExprRef)>) -> Result<ExprRef> {
        Ok(self.exprset.mk_prefix_tree(branches))
    }

    pub fn mk(&mut self, ast: &RegexAst) -> Result<ExprRef> {
        map_ast(
            ast,
            |ast| ast.get_args(),
            |ast, new_args| {
                let r = match ast {
                    RegexAst::Regex(s) => self.mk_regex(s)?,
                    RegexAst::SearchRegex(s) => self.mk_regex_for_serach(s)?,
                    RegexAst::JsonQuote(_, opts) => self.json_quote(new_args[0], opts)?,
                    RegexAst::ExprRef(r) => {
                        ensure!(self.exprset.is_valid(*r), "invalid ref");
                        *r
                    }
                    RegexAst::And(_) => self.exprset.mk_and(new_args),
                    RegexAst::Or(_) => self.exprset.mk_or(new_args),
                    RegexAst::Concat(_) => self.exprset.mk_concat_vec(new_args),
                    RegexAst::Not(_) => self.exprset.mk_not(new_args[0]),
                    RegexAst::LookAhead(_) => self.exprset.mk_lookahead(new_args[0], 0),
                    RegexAst::EmptyString => ExprRef::EMPTY_STRING,
                    RegexAst::NoMatch => ExprRef::NO_MATCH,
                    RegexAst::Literal(s) => self.exprset.mk_literal(s),
                    RegexAst::ByteLiteral(s) => self.exprset.mk_byte_literal(s),
                    RegexAst::Repeat(_, min, max) => {
                        self.exprset.mk_repeat(new_args[0], *min, *max)
                    }
                    RegexAst::MultipleOf(d, s) => {
                        ensure!(*d > 0, "invalid multiple of");
                        self.exprset.mk_remainder_is(*d, *d, *s, false)
                    }
                    RegexAst::Byte(b) => self.exprset.mk_byte(*b),
                    RegexAst::ByteSet(bs) => {
                        ensure!(
                            bs.len() == self.exprset.alphabet_words,
                            "invalid byteset len"
                        );
                        self.exprset.mk_byte_set(bs)
                    }
                };
                Ok(r)
            },
        )
    }

    pub fn is_nullable(&self, r: ExprRef) -> bool {
        self.exprset.is_nullable(r)
    }
}

// regex flags; docs copied from regex_syntax crate
impl RegexBuilder {
    /// Enable or disable the Unicode flag (`u`) by default.
    ///
    /// By default this is **enabled**. It may alternatively be selectively
    /// disabled in the regular expression itself via the `u` flag.
    ///
    /// Note that unless `utf8` is disabled (it's enabled by default), a
    /// regular expression will fail to parse if Unicode mode is disabled and a
    /// sub-expression could possibly match invalid UTF-8.
    pub fn unicode(&mut self, unicode: bool) -> &mut Self {
        self.parser_builder.unicode(unicode);
        self
    }

    /// When disabled, translation will permit the construction of a regular
    /// expression that may match invalid UTF-8.
    ///
    /// When enabled (the default), the translator is guaranteed to produce an
    /// expression that, for non-empty matches, will only ever produce spans
    /// that are entirely valid UTF-8 (otherwise, the translator will return an
    /// error).
    pub fn utf8(&mut self, utf8: bool) -> &mut Self {
        self.parser_builder.utf8(utf8);
        self
    }

    /// Enable verbose mode in the regular expression.
    ///
    /// When enabled, verbose mode permits insignificant whitespace in many
    /// places in the regular expression, as well as comments. Comments are
    /// started using `#` and continue until the end of the line.
    ///
    /// By default, this is disabled. It may be selectively enabled in the
    /// regular expression by using the `x` flag regardless of this setting.
    pub fn ignore_whitespace(&mut self, ignore_whitespace: bool) -> &mut Self {
        self.parser_builder.ignore_whitespace(ignore_whitespace);
        self
    }

    /// Enable or disable the case insensitive flag by default.
    ///
    /// By default this is disabled. It may alternatively be selectively
    /// enabled in the regular expression itself via the `i` flag.
    pub fn case_insensitive(&mut self, case_insensitive: bool) -> &mut Self {
        self.parser_builder.case_insensitive(case_insensitive);
        self
    }

    /// Enable or disable the "dot matches any character" flag by default.
    ///
    /// By default this is disabled. It may alternatively be selectively
    /// enabled in the regular expression itself via the `s` flag.
    pub fn dot_matches_new_line(&mut self, dot_matches_new_line: bool) -> &mut Self {
        self.parser_builder
            .dot_matches_new_line(dot_matches_new_line);
        self
    }
}
