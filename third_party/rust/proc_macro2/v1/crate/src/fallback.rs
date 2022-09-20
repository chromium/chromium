use crate::parse::{self, Cursor};
use crate::rcvec::{RcVec, RcVecBuilder, RcVecIntoIter, RcVecMut};
use crate::{Delimiter, Spacing, TokenTree};
#[cfg(span_locations)]
use core::cell::RefCell;
#[cfg(span_locations)]
use core::cmp;
use core::fmt::{self, Debug, Display, Write};
use core::iter::FromIterator;
use core::mem::ManuallyDrop;
use core::ops::RangeBounds;
use core::ptr;
use core::str::FromStr;
#[cfg(procmacro2_semver_exempt)]
use std::path::Path;
use std::path::PathBuf;

/// Force use of proc-macro2's fallback implementation of the API for now, even
/// if the compiler's implementation is available.
pub fn force() {
    #[cfg(wrap_proc_macro)]
    crate::detection::force_fallback();
}

/// Resume using the compiler's implementation of the proc macro API if it is
/// available.
pub fn unforce() {
    #[cfg(wrap_proc_macro)]
    crate::detection::unforce_fallback();
}

#[derive(Clone)]
pub(crate) struct TokenStream {
    inner: RcVec<TokenTree>,
}

#[derive(Debug)]
pub(crate) struct LexError {
    pub(crate) span: Span,
}

impl LexError {
    pub(crate) fn span(&self) -> Span {
        self.span
    }

    fn call_site() -> Self {
        LexError {
            span: Span::call_site(),
        }
    }
}

impl TokenStream {
    pub fn new() -> Self {
        TokenStream {
            inner: RcVecBuilder::new().build(),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.inner.len() == 0
    }

    fn take_inner(self) -> RcVecBuilder<TokenTree> {
        let nodrop = ManuallyDrop::new(self);
        unsafe { ptr::read(&nodrop.inner) }.make_owned()
    }
}

fn push_token_from_proc_macro(mut vec: RcVecMut<TokenTree>, token: TokenTree) {
    // https://github.com/dtolnay/proc-macro2/issues/235
    match token {
        #[cfg(not(no_bind_by_move_pattern_guard))]
        TokenTree::Literal(crate::Literal {
            #[cfg(wrap_proc_macro)]
                inner: crate::imp::Literal::Fallback(literal),
            #[cfg(not(wrap_proc_macro))]
                inner: literal,
            ..
        }) if literal.repr.starts_with('-') => {
            push_negative_literal(vec, literal);
        }
        #[cfg(no_bind_by_move_pattern_guard)]
        TokenTree::Literal(crate::Literal {
            #[cfg(wrap_proc_macro)]
                inner: crate::imp::Literal::Fallback(literal),
            #[cfg(not(wrap_proc_macro))]
                inner: literal,
            ..
        }) => {
            if literal.repr.starts_with('-') {
                push_negative_literal(vec, literal);
            } else {
                vec.push(TokenTree::Literal(crate::Literal::_new_stable(literal)));
            }
        }
        _ => vec.push(token),
    }

    #[cold]
    fn push_negative_literal(mut vec: RcVecMut<TokenTree>, mut literal: Literal) {
        literal.repr.remove(0);
        let mut punct = crate::Punct::new('-', Spacing::Alone);
        punct.set_span(crate::Span::_new_stable(literal.span));
        vec.push(TokenTree::Punct(punct));
        vec.push(TokenTree::Literal(crate::Literal::_new_stable(literal)));
    }
}

// Nonrecursive to prevent stack overflow.
impl Drop for TokenStream {
    fn drop(&mut self) {
        let mut inner = match self.inner.get_mut() {
            Some(inner) => inner,
            None => return,
        };
        while let Some(token) = inner.pop() {
            let group = match token {
                TokenTree::Group(group) => group.inner,
                _ => continue,
            };
            #[cfg(wrap_proc_macro)]
            let group = match group {
                crate::imp::Group::Fallback(group) => group,
                crate::imp::Group::Compiler(_) => continue,
            };
            inner.extend(group.stream.take_inner());
        }
    }
}

pub(crate) struct TokenStreamBuilder {
    inner: RcVecBuilder<TokenTree>,
}

impl TokenStreamBuilder {
    pub fn new() -> Self {
        TokenStreamBuilder {
            inner: RcVecBuilder::new(),
        }
    }

    pub fn with_capacity(cap: usize) -> Self {
        TokenStreamBuilder {
            inner: RcVecBuilder::with_capacity(cap),
        }
    }

    pub fn push_token_from_parser(&mut self, tt: TokenTree) {
        self.inner.push(tt);
    }

    pub fn build(self) -> TokenStream {
        TokenStream {
            inner: self.inner.build(),
        }
    }
}

#[cfg(span_locations)]
fn get_cursor(src: &str) -> Cursor {
    // Create a dummy file & add it to the source map
    SOURCE_MAP.with(|cm| {
        let mut cm = cm.borrow_mut();
        let name = format!("<parsed string {}>", cm.files.len());
        let span = cm.add_file(&name, src);
        Cursor {
            rest: src,
            off: span.lo,
        }
    })
}

#[cfg(not(span_locations))]
fn get_cursor(src: &str) -> Cursor {
    Cursor { rest: src }
}

impl FromStr for TokenStream {
    type Err = LexError;

    fn from_str(src: &str) -> Result<TokenStream, LexError> {
        // Create a dummy file & add it to the source map
        let cursor = get_cursor(src);

        parse::token_stream(cursor)
    }
}

impl Display for LexError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("cannot parse string into token stream")
    }
}

impl Display for TokenStream {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut joint = false;
        for (i, tt) in self.inner.iter().enumerate() {
            if i != 0 && !joint {
                write!(f, " ")?;
            }
            joint = false;
            match tt {
                TokenTree::Group(tt) => Display::fmt(tt, f),
                TokenTree::Ident(tt) => Display::fmt(tt, f),
                TokenTree::Punct(tt) => {
                    joint = tt.spacing() == Spacing::Joint;
                    Display::fmt(tt, f)
                }
                TokenTree::Literal(tt) => Display::fmt(tt, f),
            }?;
        }

        Ok(())
    }
}

impl Debug for TokenStream {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("TokenStream ")?;
        f.debug_list().entries(self.clone()).finish()
    }
}

#[cfg(use_proc_macro)]
impl From<proc_macro::TokenStream> for TokenStream {
    fn from(inner: proc_macro::TokenStream) -> TokenStream {
        inner
            .to_string()
            .parse()
            .expect("compiler token stream parse failed")
    }
}

#[cfg(use_proc_macro)]
impl From<TokenStream> for proc_macro::TokenStream {
    fn from(inner: TokenStream) -> proc_macro::TokenStream {
        inner
            .to_string()
            .parse()
            .expect("failed to parse to compiler tokens")
    }
}

impl From<TokenTree> for TokenStream {
    fn from(tree: TokenTree) -> TokenStream {
        let mut stream = RcVecBuilder::new();
        push_token_from_proc_macro(stream.as_mut(), tree);
        TokenStream {
            inner: stream.build(),
        }
    }
}

impl FromIterator<TokenTree> for TokenStream {
    fn from_iter<I: IntoIterator<Item = TokenTree>>(tokens: I) -> Self {
        let mut stream = TokenStream::new();
        stream.extend(tokens);
        stream
    }
}

impl FromIterator<TokenStream> for TokenStream {
    fn from_iter<I: IntoIterator<Item = TokenStream>>(streams: I) -> Self {
        let mut v = RcVecBuilder::new();

        for stream in streams {
            v.extend(stream.take_inner());
        }

        TokenStream { inner: v.build() }
    }
}

impl Extend<TokenTree> for TokenStream {
    fn extend<I: IntoIterator<Item = TokenTree>>(&mut self, tokens: I) {
        let mut vec = self.inner.make_mut();
        tokens
            .into_iter()
            .for_each(|token| push_token_from_proc_macro(vec.as_mut(), token));
    }
}

impl Extend<TokenStream> for TokenStream {
    fn extend<I: IntoIterator<Item = TokenStream>>(&mut self, streams: I) {
        self.inner.make_mut().extend(streams.into_iter().flatten());
    }
}

pub(crate) type TokenTreeIter = RcVecIntoIter<TokenTree>;

impl IntoIterator for TokenStream {
    type Item = TokenTree;
    type IntoIter = TokenTreeIter;

    fn into_iter(self) -> TokenTreeIter {
        self.take_inner().into_iter()
    }
}

#[derive(Clone, PartialEq, Eq)]
pub(crate) struct SourceFile {
    path: PathBuf,
}

impl SourceFile {
    /// Get the path to this source file as a string.
    pub fn path(&self) -> PathBuf {
        self.path.clone()
    }

    pub fn is_real(&self) -> bool {
        // XXX(nika): Support real files in the future?
        false
    }
}

impl Debug for SourceFile {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("SourceFile")
            .field("path", &self.path())
            .field("is_real", &self.is_real())
            .finish()
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) struct LineColumn {
    pub line: usize,
    pub column: usize,
}

#[cfg(span_locations)]
thread_local! {
    static SOURCE_MAP: RefCell<SourceMap> = RefCell::new(SourceMap {
        // NOTE: We start with a single dummy file which all call_site() and
        // def_site() spans reference.
        files: vec![FileInfo {
            #[cfg(procmacro2_semver_exempt)]
            name: "<unspecified>".to_owned(),
            span: Span { lo: 0, hi: 0 },
            lines: vec![0],
        }],
    });
}

#[cfg(span_locations)]
struct FileInfo {
    #[cfg(procmacro2_semver_exempt)]
    name: String,
    span: Span,
    lines: Vec<usize>,
}

#[cfg(span_locations)]
impl FileInfo {
    fn offset_line_column(&self, offset: usize) -> LineColumn {
        assert!(self.span_within(Span {
            lo: offset as u32,
            hi: offset as u32
        }));
        let offset = offset - self.span.lo as usize;
        match self.lines.binary_search(&offset) {
            Ok(found) => LineColumn {
                line: found + 1,
                column: 0,
            },
            Err(idx) => LineColumn {
                line: idx,
                column: offset - self.lines[idx - 1],
            },
        }
    }

    fn span_within(&self, span: Span) -> bool {
        span.lo >= self.span.lo && span.hi <= self.span.hi
    }
}

/// Computes the offsets of each line in the given source string
/// and the total number of characters
#[cfg(span_locations)]
fn lines_offsets(s: &str) -> (usize, Vec<usize>) {
    let mut lines = vec![0];
    let mut total = 0;

    for ch in s.chars() {
        total += 1;
        if ch == '\n' {
            lines.push(total);
        }
    }

    (total, lines)
}

#[cfg(span_locations)]
struct SourceMap {
    files: Vec<FileInfo>,
}

#[cfg(span_locations)]
impl SourceMap {
    fn next_start_pos(&self) -> u32 {
        // Add 1 so there's always space between files.
        //
        // We'll always have at least 1 file, as we initialize our files list
        // with a dummy file.
        self.files.last().unwrap().span.hi + 1
    }

    fn add_file(&mut self, name: &str, src: &str) -> Span {
        let (len, lines) = lines_offsets(src);
        let lo = self.next_start_pos();
        // XXX(nika): Should we bother doing a checked cast or checked add here?
        let span = Span {
            lo,
            hi: lo + (len as u32),
        };

        self.files.push(FileInfo {
            #[cfg(procmacro2_semver_exempt)]
            name: name.to_owned(),
            span,
            lines,
        });

        #[cfg(not(procmacro2_semver_exempt))]
        let _ = name;

        span
    }

    fn fileinfo(&self, span: Span) -> &FileInfo {
        for file in &self.files {
            if file.span_within(span) {
                return file;
            }
        }
        panic!("Invalid span with no related FileInfo!");
    }
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub(crate) struct Span {
    #[cfg(span_locations)]
    pub(crate) lo: u32,
    #[cfg(span_locations)]
    pub(crate) hi: u32,
}

impl Span {
    #[cfg(not(span_locations))]
    pub fn call_site() -> Self {
        Span {}
    }

    #[cfg(span_locations)]
    pub fn call_site() -> Self {
        Span { lo: 0, hi: 0 }
    }

    #[cfg(not(no_hygiene))]
    pub fn mixed_site() -> Self {
        Span::call_site()
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn def_site() -> Self {
        Span::call_site()
    }

    pub fn resolved_at(&self, _other: Span) -> Span {
        // Stable spans consist only of line/column information, so
        // `resolved_at` and `located_at` only select which span the
        // caller wants line/column information from.
        *self
    }

    pub fn located_at(&self, other: Span) -> Span {
        other
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn source_file(&self) -> SourceFile {
        SOURCE_MAP.with(|cm| {
            let cm = cm.borrow();
            let fi = cm.fileinfo(*self);
            SourceFile {
                path: Path::new(&fi.name).to_owned(),
            }
        })
    }

    #[cfg(span_locations)]
    pub fn start(&self) -> LineColumn {
        SOURCE_MAP.with(|cm| {
            let cm = cm.borrow();
            let fi = cm.fileinfo(*self);
            fi.offset_line_column(self.lo as usize)
        })
    }

    #[cfg(span_locations)]
    pub fn end(&self) -> LineColumn {
        SOURCE_MAP.with(|cm| {
            let cm = cm.borrow();
            let fi = cm.fileinfo(*self);
            fi.offset_line_column(self.hi as usize)
        })
    }

    #[cfg(not(span_locations))]
    pub fn join(&self, _other: Span) -> Option<Span> {
        Some(Span {})
    }

    #[cfg(span_locations)]
    pub fn join(&self, other: Span) -> Option<Span> {
        SOURCE_MAP.with(|cm| {
            let cm = cm.borrow();
            // If `other` is not within the same FileInfo as us, return None.
            if !cm.fileinfo(*self).span_within(other) {
                return None;
            }
            Some(Span {
                lo: cmp::min(self.lo, other.lo),
                hi: cmp::max(self.hi, other.hi),
            })
        })
    }

    #[cfg(not(span_locations))]
    fn first_byte(self) -> Self {
        self
    }

    #[cfg(span_locations)]
    fn first_byte(self) -> Self {
        Span {
            lo: self.lo,
            hi: cmp::min(self.lo.saturating_add(1), self.hi),
        }
    }

    #[cfg(not(span_locations))]
    fn last_byte(self) -> Self {
        self
    }

    #[cfg(span_locations)]
    fn last_byte(self) -> Self {
        Span {
            lo: cmp::max(self.hi.saturating_sub(1), self.lo),
            hi: self.hi,
        }
    }
}

impl Debug for Span {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        #[cfg(span_locations)]
        return write!(f, "bytes({}..{})", self.lo, self.hi);

        #[cfg(not(span_locations))]
        write!(f, "Span")
    }
}

pub(crate) fn debug_span_field_if_nontrivial(debug: &mut fmt::DebugStruct, span: Span) {
    #[cfg(span_locations)]
    {
        if span.lo == 0 && span.hi == 0 {
            return;
        }
    }

    if cfg!(span_locations) {
        debug.field("span", &span);
    }
}

#[derive(Clone)]
pub(crate) struct Group {
    delimiter: Delimiter,
    stream: TokenStream,
    span: Span,
}

impl Group {
    pub fn new(delimiter: Delimiter, stream: TokenStream) -> Self {
        Group {
            delimiter,
            stream,
            span: Span::call_site(),
        }
    }

    pub fn delimiter(&self) -> Delimiter {
        self.delimiter
    }

    pub fn stream(&self) -> TokenStream {
        self.stream.clone()
    }

    pub fn span(&self) -> Span {
        self.span
    }

    pub fn span_open(&self) -> Span {
        self.span.first_byte()
    }

    pub fn span_close(&self) -> Span {
        self.span.last_byte()
    }

    pub fn set_span(&mut self, span: Span) {
        self.span = span;
    }
}

impl Display for Group {
    // We attempt to match libproc_macro's formatting.
    // Empty parens: ()
    // Nonempty parens: (...)
    // Empty brackets: []
    // Nonempty brackets: [...]
    // Empty braces: { }
    // Nonempty braces: { ... }
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let (open, close) = match self.delimiter {
            Delimiter::Parenthesis => ("(", ")"),
            Delimiter::Brace => ("{ ", "}"),
            Delimiter::Bracket => ("[", "]"),
            Delimiter::None => ("", ""),
        };

        f.write_str(open)?;
        Display::fmt(&self.stream, f)?;
        if self.delimiter == Delimiter::Brace && !self.stream.inner.is_empty() {
            f.write_str(" ")?;
        }
        f.write_str(close)?;

        Ok(())
    }
}

impl Debug for Group {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        let mut debug = fmt.debug_struct("Group");
        debug.field("delimiter", &self.delimiter);
        debug.field("stream", &self.stream);
        debug_span_field_if_nontrivial(&mut debug, self.span);
        debug.finish()
    }
}

#[derive(Clone)]
pub(crate) struct Ident {
    sym: String,
    span: Span,
    raw: bool,
}

impl Ident {
    fn _new(string: &str, raw: bool, span: Span) -> Self {
        validate_ident(string, raw);

        Ident {
            sym: string.to_owned(),
            span,
            raw,
        }
    }

    pub fn new(string: &str, span: Span) -> Self {
        Ident::_new(string, false, span)
    }

    pub fn new_raw(string: &str, span: Span) -> Self {
        Ident::_new(string, true, span)
    }

    pub fn span(&self) -> Span {
        self.span
    }

    pub fn set_span(&mut self, span: Span) {
        self.span = span;
    }
}

pub(crate) fn is_ident_start(c: char) -> bool {
    c == '_' || unicode_ident::is_xid_start(c)
}

pub(crate) fn is_ident_continue(c: char) -> bool {
    unicode_ident::is_xid_continue(c)
}

fn validate_ident(string: &str, raw: bool) {
    if string.is_empty() {
        panic!("Ident is not allowed to be empty; use Option<Ident>");
    }

    if string.bytes().all(|digit| digit >= b'0' && digit <= b'9') {
        panic!("Ident cannot be a number; use Literal instead");
    }

    fn ident_ok(string: &str) -> bool {
        let mut chars = string.chars();
        let first = chars.next().unwrap();
        if !is_ident_start(first) {
            return false;
        }
        for ch in chars {
            if !is_ident_continue(ch) {
                return false;
            }
        }
        true
    }

    if !ident_ok(string) {
        panic!("{:?} is not a valid Ident", string);
    }

    if raw {
        match string {
            "_" | "super" | "self" | "Self" | "crate" => {
                panic!("`r#{}` cannot be a raw identifier", string);
            }
            _ => {}
        }
    }
}

impl PartialEq for Ident {
    fn eq(&self, other: &Ident) -> bool {
        self.sym == other.sym && self.raw == other.raw
    }
}

impl<T> PartialEq<T> for Ident
where
    T: ?Sized + AsRef<str>,
{
    fn eq(&self, other: &T) -> bool {
        let other = other.as_ref();
        if self.raw {
            other.starts_with("r#") && self.sym == other[2..]
        } else {
            self.sym == other
        }
    }
}

impl Display for Ident {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if self.raw {
            f.write_str("r#")?;
        }
        Display::fmt(&self.sym, f)
    }
}

impl Debug for Ident {
    // Ident(proc_macro), Ident(r#union)
    #[cfg(not(span_locations))]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut debug = f.debug_tuple("Ident");
        debug.field(&format_args!("{}", self));
        debug.finish()
    }

    // Ident {
    //     sym: proc_macro,
    //     span: bytes(128..138)
    // }
    #[cfg(span_locations)]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut debug = f.debug_struct("Ident");
        debug.field("sym", &format_args!("{}", self));
        debug_span_field_if_nontrivial(&mut debug, self.span);
        debug.finish()
    }
}

#[derive(Clone)]
pub(crate) struct Literal {
    repr: String,
    span: Span,
}

macro_rules! suffixed_numbers {
    ($($name:ident => $kind:ident,)*) => ($(
        pub fn $name(n: $kind) -> Literal {
            Literal::_new(format!(concat!("{}", stringify!($kind)), n))
        }
    )*)
}

macro_rules! unsuffixed_numbers {
    ($($name:ident => $kind:ident,)*) => ($(
        pub fn $name(n: $kind) -> Literal {
            Literal::_new(n.to_string())
        }
    )*)
}

impl Literal {
    pub(crate) fn _new(repr: String) -> Self {
        Literal {
            repr,
            span: Span::call_site(),
        }
    }

    pub(crate) unsafe fn from_str_unchecked(repr: &str) -> Self {
        Literal::_new(repr.to_owned())
    }

    suffixed_numbers! {
        u8_suffixed => u8,
        u16_suffixed => u16,
        u32_suffixed => u32,
        u64_suffixed => u64,
        u128_suffixed => u128,
        usize_suffixed => usize,
        i8_suffixed => i8,
        i16_suffixed => i16,
        i32_suffixed => i32,
        i64_suffixed => i64,
        i128_suffixed => i128,
        isize_suffixed => isize,

        f32_suffixed => f32,
        f64_suffixed => f64,
    }

    unsuffixed_numbers! {
        u8_unsuffixed => u8,
        u16_unsuffixed => u16,
        u32_unsuffixed => u32,
        u64_unsuffixed => u64,
        u128_unsuffixed => u128,
        usize_unsuffixed => usize,
        i8_unsuffixed => i8,
        i16_unsuffixed => i16,
        i32_unsuffixed => i32,
        i64_unsuffixed => i64,
        i128_unsuffixed => i128,
        isize_unsuffixed => isize,
    }

    pub fn f32_unsuffixed(f: f32) -> Literal {
        let mut s = f.to_string();
        if !s.contains('.') {
            s.push_str(".0");
        }
        Literal::_new(s)
    }

    pub fn f64_unsuffixed(f: f64) -> Literal {
        let mut s = f.to_string();
        if !s.contains('.') {
            s.push_str(".0");
        }
        Literal::_new(s)
    }

    pub fn string(t: &str) -> Literal {
        let mut repr = String::with_capacity(t.len() + 2);
        repr.push('"');
        for c in t.chars() {
            if c == '\'' {
                // escape_debug turns this into "\'" which is unnecessary.
                repr.push(c);
            } else {
                repr.extend(c.escape_debug());
            }
        }
        repr.push('"');
        Literal::_new(repr)
    }

    pub fn character(t: char) -> Literal {
        let mut repr = String::new();
        repr.push('\'');
        if t == '"' {
            // escape_debug turns this into '\"' which is unnecessary.
            repr.push(t);
        } else {
            repr.extend(t.escape_debug());
        }
        repr.push('\'');
        Literal::_new(repr)
    }

    pub fn byte_string(bytes: &[u8]) -> Literal {
        let mut escaped = "b\"".to_string();
        for b in bytes {
            #[allow(clippy::match_overlapping_arm)]
            match *b {
                b'\0' => escaped.push_str(r"\0"),
                b'\t' => escaped.push_str(r"\t"),
                b'\n' => escaped.push_str(r"\n"),
                b'\r' => escaped.push_str(r"\r"),
                b'"' => escaped.push_str("\\\""),
                b'\\' => escaped.push_str("\\\\"),
                b'\x20'..=b'\x7E' => escaped.push(*b as char),
                _ => {
                    let _ = write!(escaped, "\\x{:02X}", b);
                }
            }
        }
        escaped.push('"');
        Literal::_new(escaped)
    }

    pub fn span(&self) -> Span {
        self.span
    }

    pub fn set_span(&mut self, span: Span) {
        self.span = span;
    }

    pub fn subspan<R: RangeBounds<usize>>(&self, _range: R) -> Option<Span> {
        None
    }
}

impl FromStr for Literal {
    type Err = LexError;

    fn from_str(mut repr: &str) -> Result<Self, Self::Err> {
        let negative = repr.starts_with('-');
        if negative {
            repr = &repr[1..];
            if !repr.starts_with(|ch: char| ch.is_ascii_digit()) {
                return Err(LexError::call_site());
            }
        }
        let cursor = get_cursor(repr);
        if let Ok((_rest, mut literal)) = parse::literal(cursor) {
            if literal.repr.len() == repr.len() {
                if negative {
                    literal.repr.insert(0, '-');
                }
                return Ok(literal);
            }
        }
        Err(LexError::call_site())
    }
}

impl Display for Literal {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        Display::fmt(&self.repr, f)
    }
}

impl Debug for Literal {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        let mut debug = fmt.debug_struct("Literal");
        debug.field("lit", &format_args!("{}", self.repr));
        debug_span_field_if_nontrivial(&mut debug, self.span);
        debug.finish()
    }
}
