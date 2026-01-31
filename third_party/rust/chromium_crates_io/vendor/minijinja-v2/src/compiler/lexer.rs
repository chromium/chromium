use std::borrow::Cow;
use std::ops::ControlFlow;

use crate::compiler::tokens::{Span, Token};
use crate::error::{Error, ErrorKind};
use crate::syntax::SyntaxConfig;
use crate::utils::{memchr, memstr, unescape};

/// Internal config struct to control whitespace in the engine.
#[derive(Copy, Clone, Debug, Default)]
pub struct WhitespaceConfig {
    pub keep_trailing_newline: bool,
    pub lstrip_blocks: bool,
    pub trim_blocks: bool,
}

/// Tokenizes jinja templates.
pub struct Tokenizer<'s> {
    stack: Vec<LexerState>,
    source: &'s str,
    filename: &'s str,
    current_line: u16,
    current_col: u16,
    current_offset: usize,
    trim_leading_whitespace: bool,
    pending_start_marker: Option<(StartMarker, usize)>,
    paren_balance: isize,
    syntax_config: SyntaxConfig,
    ws_config: WhitespaceConfig,
}

enum LexerState {
    Template,
    Variable,
    Block,
    #[cfg(feature = "custom_syntax")]
    LineStatement,
}

/// Utility enum that defines a marker.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum StartMarker {
    Variable,
    Block,
    Comment,
    #[cfg(feature = "custom_syntax")]
    LineStatement,
    #[cfg(feature = "custom_syntax")]
    LineComment,
}

/// What ends this block tokenization?
#[derive(Debug, Copy, Clone)]
enum BlockSentinel {
    Variable,
    Block,
    #[cfg(feature = "custom_syntax")]
    LineStatement,
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
enum Whitespace {
    Default,
    Preserve,
    Remove,
}

impl Whitespace {
    fn from_byte(b: Option<u8>) -> Whitespace {
        match b {
            Some(b'-') => Whitespace::Remove,
            Some(b'+') => Whitespace::Preserve,
            _ => Whitespace::Default,
        }
    }

    fn len(&self) -> usize {
        match self {
            Whitespace::Default => 0,
            Whitespace::Preserve | Whitespace::Remove => 1,
        }
    }
}

fn find_start_marker_memchr(a: &str) -> Option<(usize, StartMarker, usize, Whitespace)> {
    let bytes = a.as_bytes();
    let mut offset = 0;
    loop {
        let idx = some!(memchr(&bytes[offset..], b'{'));
        let marker = match bytes.get(offset + idx + 1).copied() {
            Some(b'{') => StartMarker::Variable,
            Some(b'%') => StartMarker::Block,
            Some(b'#') => StartMarker::Comment,
            _ => {
                offset += idx + 1;
                continue;
            }
        };
        let ws = Whitespace::from_byte(bytes.get(offset + idx + 2).copied());
        return Some((offset + idx, marker, 2 + ws.len(), ws));
    }
}

#[cfg(feature = "custom_syntax")]
fn find_start_marker(
    a: &str,
    offset: usize,
    syntax_config: &SyntaxConfig,
) -> Option<(usize, StartMarker, usize, Whitespace)> {
    // If we have a custom delimiter we need to use the aho-corasick
    // otherwise we can use internal memchr.
    let Some(ref ac) = syntax_config.aho_corasick else {
        return find_start_marker_memchr(&a[offset..]);
    };

    let bytes = &a.as_bytes()[offset..];
    let mut state = aho_corasick::automaton::OverlappingState::start();
    let mut longest_match = None::<(usize, StartMarker, usize, Whitespace)>;

    loop {
        ac.find_overlapping(bytes, &mut state);
        let m = match state.get_match() {
            None => break,
            Some(m) => m,
        };

        let marker = syntax_config.pattern_to_marker(m.pattern());
        let ws = if matches!(marker, StartMarker::LineStatement) {
            let prefix = &a.as_bytes()[..offset + m.start()];
            if matches!(
                prefix
                    .iter()
                    .copied()
                    .rev()
                    .find(|&x| x != b' ' && x != b'\t'),
                None | Some(b'\r') | Some(b'\n')
            ) {
                Whitespace::Default
            } else {
                continue;
            }
        } else {
            Whitespace::from_byte(bytes.get(m.start() + m.len()).copied())
        };
        let new_match = (m.start(), marker, m.len() + ws.len(), ws);

        if longest_match.as_ref().is_some_and(|x| new_match.0 > x.0) {
            break;
        }
        longest_match = Some(new_match);
    }

    longest_match
}

#[cfg(not(feature = "custom_syntax"))]
fn find_start_marker(
    a: &str,
    offset: usize,
    _syntax_config: &SyntaxConfig,
) -> Option<(usize, StartMarker, usize, Whitespace)> {
    find_start_marker_memchr(&a[offset..])
}

#[cfg(feature = "unicode")]
fn lex_identifier(s: &str) -> usize {
    s.chars()
        .enumerate()
        .map_while(|(idx, c)| {
            let cont = if c == '_' {
                true
            } else if idx == 0 {
                unicode_ident::is_xid_start(c)
            } else {
                unicode_ident::is_xid_continue(c)
            };
            cont.then(|| c.len_utf8())
        })
        .sum::<usize>()
}

#[cfg(not(feature = "unicode"))]
fn lex_identifier(s: &str) -> usize {
    s.as_bytes()
        .iter()
        .enumerate()
        .take_while(|&(idx, &c)| {
            if c == b'_' {
                true
            } else if idx == 0 {
                c.is_ascii_alphabetic()
            } else {
                c.is_ascii_alphanumeric()
            }
        })
        .count()
}

fn is_nl(c: char) -> bool {
    c == '\r' || c == '\n'
}

#[cfg(feature = "custom_syntax")]
fn skip_nl(mut rest: &str) -> (bool, usize) {
    let mut skip = 0;
    let mut was_nl = false;
    if let Some(new_rest) = rest.strip_prefix('\n') {
        rest = new_rest;
        skip += 1;
        was_nl = true;
    }
    if let Some(new_rest) = rest.strip_prefix('\r') {
        rest = new_rest;
        skip += 1;
        was_nl = true;
    }
    (was_nl || rest.is_empty(), skip)
}

fn lstrip_block(s: &str) -> &str {
    let trimmed = s.trim_end_matches(|x: char| x.is_whitespace() && !is_nl(x));
    if trimmed.is_empty() || trimmed.as_bytes().get(trimmed.len() - 1) == Some(&b'\n') {
        trimmed
    } else {
        s
    }
}

fn should_lstrip_block(flag: bool, marker: StartMarker, prefix: &str) -> bool {
    if flag && !matches!(marker, StartMarker::Variable) {
        // Only strip if we're at the start of a line
        for c in prefix.chars().rev() {
            if is_nl(c) {
                return true;
            } else if !c.is_whitespace() {
                return false;
            }
        }
        // If we get here, we're at the start of the file
        return true;
    }
    #[cfg(feature = "custom_syntax")]
    {
        if matches!(
            marker,
            StartMarker::LineStatement | StartMarker::LineComment
        ) {
            return true;
        }
    }
    false
}

fn skip_basic_tag(
    block_str: &str,
    name: &str,
    block_end: &str,
    skip_ws_control: bool,
) -> Option<(usize, Whitespace)> {
    let mut ptr = block_str;

    if skip_ws_control {
        if let Some(rest) = ptr.strip_prefix(['-', '+']) {
            ptr = rest;
        }
    }
    while let Some(rest) = ptr.strip_prefix(|x: char| x.is_ascii_whitespace()) {
        ptr = rest;
    }

    ptr = some!(ptr.strip_prefix(name));

    while let Some(rest) = ptr.strip_prefix(|x: char| x.is_ascii_whitespace()) {
        ptr = rest;
    }

    let ws = if let Some(rest) = ptr.strip_prefix('-') {
        ptr = rest;
        Whitespace::Remove
    } else if let Some(rest) = ptr.strip_prefix('+') {
        ptr = rest;
        Whitespace::Preserve
    } else {
        Whitespace::Default
    };

    ptr.strip_prefix(block_end)
        .map(|ptr| (block_str.len() - ptr.len(), ws))
}

impl<'s> Tokenizer<'s> {
    /// Creates a new tokenizer.
    pub fn new(
        input: &'s str,
        filename: &'s str,
        in_expr: bool,
        syntax_config: SyntaxConfig,
        whitespace_config: WhitespaceConfig,
    ) -> Tokenizer<'s> {
        let mut source = input;
        if !whitespace_config.keep_trailing_newline {
            if source.ends_with('\n') {
                source = &source[..source.len() - 1];
            }
            if source.ends_with('\r') {
                source = &source[..source.len() - 1];
            }
        }
        Tokenizer {
            source,
            filename,
            stack: vec![if in_expr {
                LexerState::Variable
            } else {
                LexerState::Template
            }],
            current_line: 1,
            current_col: 0,
            current_offset: 0,
            paren_balance: 0,
            trim_leading_whitespace: false,
            pending_start_marker: None,
            syntax_config,
            ws_config: whitespace_config,
        }
    }

    /// Returns the current filename.
    pub fn filename(&self) -> &str {
        self.filename
    }

    /// Produces the next token from the tokenizer.
    pub fn next_token(&mut self) -> Result<Option<(Token<'s>, Span)>, Error> {
        loop {
            if self.rest_bytes().is_empty() {
                // line statements normally close with newlines.  At the end of the file
                // however we need to use the stack to close out the block instead.
                #[cfg(feature = "custom_syntax")]
                {
                    if matches!(self.stack.pop(), Some(LexerState::LineStatement)) {
                        return Ok(Some((Token::BlockEnd, self.span(self.loc()))));
                    }
                }
                return Ok(None);
            }
            let outcome = match self.stack.last() {
                Some(LexerState::Template) => self.tokenize_root(),
                Some(LexerState::Block) => self.tokenize_block_or_var(BlockSentinel::Block),
                #[cfg(feature = "custom_syntax")]
                Some(LexerState::LineStatement) => {
                    self.tokenize_block_or_var(BlockSentinel::LineStatement)
                }
                Some(LexerState::Variable) => self.tokenize_block_or_var(BlockSentinel::Variable),
                None => panic!("empty lexer stack"),
            };
            match ok!(outcome) {
                ControlFlow::Break(rv) => return Ok(Some(rv)),
                ControlFlow::Continue(()) => continue,
            }
        }
    }

    #[inline]
    fn rest(&self) -> &'s str {
        &self.source[self.current_offset..]
    }

    #[inline]
    fn rest_bytes(&self) -> &'s [u8] {
        &self.source.as_bytes()[self.current_offset..]
    }

    fn advance(&mut self, bytes: usize) -> &'s str {
        let skipped = &self.rest()[..bytes];
        for c in skipped.chars() {
            match c {
                '\n' => {
                    self.current_line = self.current_line.saturating_add(1);
                    self.current_col = 0;
                }
                _ => self.current_col = self.current_col.saturating_add(1),
            }
        }
        self.current_offset += bytes;
        skipped
    }

    #[inline]
    fn loc(&self) -> (u16, u16, u32) {
        (
            self.current_line,
            self.current_col,
            self.current_offset as u32,
        )
    }

    #[inline]
    fn span(&self, (start_line, start_col, start_offset): (u16, u16, u32)) -> Span {
        Span {
            start_line,
            start_col,
            start_offset,
            end_line: self.current_line,
            end_col: self.current_col,
            end_offset: self.current_offset as u32,
        }
    }

    #[inline]
    fn syntax_error(&mut self, msg: &'static str) -> Error {
        let mut span = self.span(self.loc());
        if span.start_col == span.end_col {
            span.end_col += 1;
            span.end_offset += 1;
        }
        let mut err = Error::new(ErrorKind::SyntaxError, msg);
        err.set_filename_and_span(self.filename, span);
        err
    }

    fn eat_number(&mut self) -> Result<(Token<'s>, Span), Error> {
        #[derive(Copy, Clone)]
        enum State {
            RadixInteger, // 0x10
            Integer,      // 123
            Fraction,     // .123
            Exponent,     // E | e
            ExponentSign, // +|-
        }

        let old_loc = self.loc();

        let radix = match self.rest_bytes().get(..2) {
            Some(b"0b" | b"0B") => 2,
            Some(b"0o" | b"0O") => 8,
            Some(b"0x" | b"0X") => 16,
            _ => 10,
        };

        let mut state = if radix == 10 {
            State::Integer
        } else {
            self.advance(2);
            State::RadixInteger
        };

        let mut num_len = self
            .rest_bytes()
            .iter()
            .take_while(|&c| c.is_ascii_digit())
            .count();
        let mut has_underscore = false;
        for c in self.rest_bytes()[num_len..].iter().copied() {
            state = match (c, state) {
                (b'.', State::Integer) => State::Fraction,
                (b'E' | b'e', State::Integer | State::Fraction) => State::Exponent,
                (b'+' | b'-', State::Exponent) => State::ExponentSign,
                (b'0'..=b'9', State::Exponent) => State::ExponentSign,
                (b'0'..=b'9', state) => state,
                (b'a'..=b'f' | b'A'..=b'F', State::RadixInteger) if radix == 16 => state,
                (b'_', _) => {
                    has_underscore = true;
                    state
                }
                _ => break,
            };
            num_len += 1;
        }
        let is_float = !matches!(state, State::Integer | State::RadixInteger);

        let mut num = Cow::Borrowed(self.advance(num_len));
        if has_underscore {
            if num.ends_with('_') {
                return Err(self.syntax_error("'_' may not occur at end of number"));
            }
            num = Cow::Owned(num.replace('_', ""));
        }

        Ok((
            ok!(if is_float {
                num.parse()
                    .map(Token::Float)
                    .map_err(|_| self.syntax_error("invalid float"))
            } else if let Ok(int) = u64::from_str_radix(&num, radix) {
                Ok(Token::Int(int))
            } else {
                u128::from_str_radix(&num, radix)
                    .map(|x| Token::Int128(Box::new(x)))
                    .map_err(|_| self.syntax_error("invalid integer (too large)"))
            }),
            self.span(old_loc),
        ))
    }

    fn eat_identifier(&mut self) -> Result<(Token<'s>, Span), Error> {
        let ident_len = lex_identifier(self.rest());
        if ident_len > 0 {
            let old_loc = self.loc();
            let ident = self.advance(ident_len);
            Ok((Token::Ident(ident), self.span(old_loc)))
        } else {
            Err(self.syntax_error("unexpected character"))
        }
    }

    fn eat_string(&mut self, delim: u8) -> Result<(Token<'s>, Span), Error> {
        let old_loc = self.loc();
        let mut escaped = false;
        let mut has_escapes = false;
        let str_len = self
            .rest_bytes()
            .iter()
            .skip(1)
            .take_while(|&&c| match (escaped, c) {
                (true, _) => {
                    escaped = false;
                    true
                }
                (_, b'\\') => {
                    escaped = true;
                    has_escapes = true;
                    true
                }
                (_, c) if c == delim => false,
                _ => true,
            })
            .count();
        if escaped || self.rest_bytes().get(str_len + 1) != Some(&delim) {
            self.advance(str_len + 1);
            return Err(self.syntax_error("unexpected end of string"));
        }
        let s = self.advance(str_len + 2);
        Ok(if has_escapes {
            (
                Token::String(ok!(unescape(&s[1..s.len() - 1])).into_boxed_str()),
                self.span(old_loc),
            )
        } else {
            (Token::Str(&s[1..s.len() - 1]), self.span(old_loc))
        })
    }

    fn skip_whitespace(&mut self) {
        let skipped = self
            .rest()
            .chars()
            .map_while(|c| c.is_whitespace().then(|| c.len_utf8()))
            .sum();
        if skipped > 0 {
            self.advance(skipped);
        }
    }

    fn skip_newline_if_trim_blocks(&mut self) {
        if self.ws_config.trim_blocks {
            if self.rest_bytes().get(0) == Some(&b'\r') {
                self.advance(1);
            }
            if self.rest_bytes().get(0) == Some(&b'\n') {
                self.advance(1);
            }
        }
    }

    fn handle_tail_ws(&mut self, ws: Whitespace) {
        match ws {
            Whitespace::Preserve => {}
            Whitespace::Default => {
                self.skip_newline_if_trim_blocks();
            }
            Whitespace::Remove => {
                self.trim_leading_whitespace = true;
            }
        }
    }

    fn variable_end(&self) -> &str {
        self.syntax_config.variable_delimiters().1
    }

    fn block_start(&self) -> &str {
        self.syntax_config.block_delimiters().0
    }

    fn block_end(&self) -> &str {
        self.syntax_config.block_delimiters().1
    }

    fn comment_end(&self) -> &str {
        self.syntax_config.comment_delimiters().1
    }

    fn tokenize_root(&mut self) -> Result<ControlFlow<(Token<'s>, Span)>, Error> {
        if let Some((marker, len)) = self.pending_start_marker.take() {
            return self.handle_start_marker(marker, len);
        }
        if self.trim_leading_whitespace {
            self.trim_leading_whitespace = false;
            self.skip_whitespace();
        }
        let old_loc = self.loc();
        let (lead, span) =
            match find_start_marker(self.source, self.current_offset, &self.syntax_config) {
                Some((start, marker, len, whitespace)) => {
                    self.pending_start_marker = Some((marker, len));
                    match whitespace {
                        Whitespace::Default
                            if should_lstrip_block(
                                self.ws_config.lstrip_blocks,
                                marker,
                                &self.source[..self.current_offset + start],
                            ) =>
                        {
                            let peeked = &self.rest()[..start];
                            let trimmed = lstrip_block(peeked);
                            let lead = self.advance(trimmed.len());
                            let span = self.span(old_loc);
                            self.advance(peeked.len() - trimmed.len());
                            (lead, span)
                        }
                        Whitespace::Default | Whitespace::Preserve => {
                            (self.advance(start), self.span(old_loc))
                        }
                        Whitespace::Remove => {
                            let peeked = &self.rest()[..start];
                            let trimmed = peeked.trim_end();
                            let lead = self.advance(trimmed.len());
                            let span = self.span(old_loc);
                            self.advance(peeked.len() - trimmed.len());
                            (lead, span)
                        }
                    }
                }
                None => (self.advance(self.rest().len()), self.span(old_loc)),
            };

        if lead.is_empty() {
            Ok(ControlFlow::Continue(()))
        } else {
            Ok(ControlFlow::Break((Token::TemplateData(lead), span)))
        }
    }

    fn handle_start_marker(
        &mut self,
        marker: StartMarker,
        skip: usize,
    ) -> Result<ControlFlow<(Token<'s>, Span)>, Error> {
        match marker {
            StartMarker::Comment => {
                if let Some(end) = memstr(&self.rest_bytes()[skip..], self.comment_end().as_bytes())
                {
                    let ws = Whitespace::from_byte(
                        self.rest_bytes().get(end.saturating_sub(1) + skip).copied(),
                    );
                    self.advance(end + skip + self.comment_end().len());
                    self.handle_tail_ws(ws);
                    Ok(ControlFlow::Continue(()))
                } else {
                    self.advance(self.rest_bytes().len());
                    Err(self.syntax_error("unexpected end of comment"))
                }
            }
            StartMarker::Variable => {
                let old_loc = self.loc();
                self.advance(skip);
                self.stack.push(LexerState::Variable);
                Ok(ControlFlow::Break((
                    Token::VariableStart,
                    self.span(old_loc),
                )))
            }
            StartMarker::Block => {
                // raw blocks require some special handling.  If we are at the beginning of a raw
                // block we want to skip everything until {% endraw %} completely ignoring interior
                // syntax and emit the entire raw block as TemplateData.
                if let Some((raw, ws_start)) =
                    skip_basic_tag(&self.rest()[skip..], "raw", self.block_end(), false)
                {
                    self.advance(raw + skip);
                    self.handle_raw_tag(ws_start)
                } else {
                    let old_loc = self.loc();
                    self.advance(skip);
                    self.stack.push(LexerState::Block);
                    Ok(ControlFlow::Break((Token::BlockStart, self.span(old_loc))))
                }
            }
            #[cfg(feature = "custom_syntax")]
            StartMarker::LineStatement => {
                let old_loc = self.loc();
                self.advance(skip);
                self.stack.push(LexerState::LineStatement);
                Ok(ControlFlow::Break((Token::BlockStart, self.span(old_loc))))
            }
            #[cfg(feature = "custom_syntax")]
            StartMarker::LineComment => {
                let comment_skip = self.rest_bytes()[skip..]
                    .iter()
                    .take_while(|&&c| c != b'\r' && c != b'\n')
                    .count();
                let (_, nl_skip) = skip_nl(&self.rest()[skip + comment_skip..]);
                self.advance(skip + comment_skip + nl_skip);
                Ok(ControlFlow::Continue(()))
            }
        }
    }

    fn handle_raw_tag(
        &mut self,
        ws_start: Whitespace,
    ) -> Result<ControlFlow<(Token<'s>, Span)>, Error> {
        let old_loc = self.loc();
        let mut ptr = 0;
        while let Some(block) = memstr(&self.rest_bytes()[ptr..], self.block_start().as_bytes()) {
            ptr += block + self.block_start().len();
            if let Some((endraw, ws_next)) =
                skip_basic_tag(&self.rest()[ptr..], "endraw", self.block_end(), true)
            {
                let ws = Whitespace::from_byte(self.rest_bytes().get(ptr).copied());
                let end = ptr - self.block_start().len();
                let mut result = &self.rest()[..end];
                self.advance(end);
                let span = self.span(old_loc);
                self.advance(self.block_start().len() + endraw);
                match ws_start {
                    Whitespace::Default if self.ws_config.trim_blocks => {
                        if result.starts_with('\r') {
                            result = &result[1..];
                        }
                        if result.starts_with('\n') {
                            result = &result[1..];
                        }
                    }
                    Whitespace::Remove => {
                        result = result.trim_start();
                    }
                    _ => {}
                }
                result = match ws {
                    Whitespace::Default if self.ws_config.lstrip_blocks => lstrip_block(result),
                    Whitespace::Remove => result.trim_end(),
                    _ => result,
                };
                self.handle_tail_ws(ws_next);
                return Ok(ControlFlow::Break((Token::TemplateData(result), span)));
            }
        }
        self.advance(self.rest_bytes().len());
        Err(self.syntax_error("unexpected end of raw block"))
    }

    fn tokenize_block_or_var(
        &mut self,
        sentinel: BlockSentinel,
    ) -> Result<ControlFlow<(Token<'s>, Span)>, Error> {
        let old_loc = self.loc();
        let rest = self.rest();

        // special case for looking for the end of a line statements if there are no
        // open parens, braces etc.  This can only happen with custom syntax
        #[cfg(feature = "custom_syntax")]
        {
            if matches!(sentinel, BlockSentinel::LineStatement)
                && self.paren_balance == 0
                && self.syntax_config.line_statement_prefix().is_some()
            {
                let skip = rest
                    .chars()
                    .take_while(|&x| x.is_whitespace() && !is_nl(x))
                    .map(|x| x.len_utf8())
                    .sum();
                let (was_nl, nl_skip) = skip_nl(&rest[skip..]);
                if was_nl {
                    self.advance(skip + nl_skip);
                    self.stack.pop();
                    return Ok(ControlFlow::Break((Token::BlockEnd, self.span(old_loc))));
                }
            }
        }

        // in blocks whitespace is generally ignored, skip it.
        match rest
            .as_bytes()
            .iter()
            .position(|&x| !x.is_ascii_whitespace())
        {
            Some(0) => {}
            None => {
                self.advance(rest.len());
                return Ok(ControlFlow::Continue(()));
            }
            Some(offset) => {
                self.advance(offset);
                return Ok(ControlFlow::Continue(()));
            }
        }

        // look out for the end of blocks
        if self.paren_balance == 0 {
            match sentinel {
                BlockSentinel::Block => {
                    if matches!(rest.get(..1), Some("-" | "+"))
                        && rest[1..].starts_with(self.block_end())
                    {
                        self.stack.pop();
                        let was_minus = &rest[..1] == "-";
                        self.advance(self.block_end().len() + 1);
                        let span = self.span(old_loc);
                        if was_minus {
                            self.trim_leading_whitespace = true;
                        }
                        return Ok(ControlFlow::Break((Token::BlockEnd, span)));
                    }
                    if rest.starts_with(self.block_end()) {
                        self.stack.pop();
                        self.advance(self.block_end().len());
                        let span = self.span(old_loc);
                        self.skip_newline_if_trim_blocks();
                        return Ok(ControlFlow::Break((Token::BlockEnd, span)));
                    }
                }
                BlockSentinel::Variable => {
                    if matches!(rest.get(..1), Some("-" | "+"))
                        && rest[1..].starts_with(self.variable_end())
                    {
                        self.stack.pop();
                        let was_minus = &rest[..1] == "-";
                        self.advance(self.variable_end().len() + 1);
                        let span = self.span(old_loc);
                        if was_minus {
                            self.trim_leading_whitespace = true;
                        }
                        return Ok(ControlFlow::Break((Token::VariableEnd, span)));
                    }
                    if rest.starts_with(self.variable_end()) {
                        self.stack.pop();
                        self.advance(self.variable_end().len());
                        return Ok(ControlFlow::Break((Token::VariableEnd, self.span(old_loc))));
                    }
                }
                // line statements are handled above
                #[cfg(feature = "custom_syntax")]
                BlockSentinel::LineStatement => {}
            }
        }

        // two character operators
        let op = match rest.as_bytes().get(..2) {
            Some(b"//") => Some(Token::FloorDiv),
            Some(b"**") => Some(Token::Pow),
            Some(b"==") => Some(Token::Eq),
            Some(b"!=") => Some(Token::Ne),
            Some(b">=") => Some(Token::Gte),
            Some(b"<=") => Some(Token::Lte),
            _ => None,
        };
        if let Some(op) = op {
            self.advance(2);
            return Ok(ControlFlow::Break((op, self.span(old_loc))));
        }

        macro_rules! with_paren_balance {
            ($delta:expr, $tok:expr) => {{
                self.paren_balance += $delta;
                Some($tok)
            }};
        }

        // single character operators (and strings)
        let op = match rest.as_bytes().get(0) {
            Some(b'+') => Some(Token::Plus),
            Some(b'-') => Some(Token::Minus),
            Some(b'*') => Some(Token::Mul),
            Some(b'/') => Some(Token::Div),
            Some(b'%') => Some(Token::Mod),
            Some(b'.') => Some(Token::Dot),
            Some(b',') => Some(Token::Comma),
            Some(b':') => Some(Token::Colon),
            Some(b'~') => Some(Token::Tilde),
            Some(b'|') => Some(Token::Pipe),
            Some(b'=') => Some(Token::Assign),
            Some(b'>') => Some(Token::Gt),
            Some(b'<') => Some(Token::Lt),
            Some(b'(') => with_paren_balance!(1, Token::ParenOpen),
            Some(b')') => with_paren_balance!(-1, Token::ParenClose),
            Some(b'[') => with_paren_balance!(1, Token::BracketOpen),
            Some(b']') => with_paren_balance!(-1, Token::BracketClose),
            Some(b'{') => with_paren_balance!(1, Token::BraceOpen),
            Some(b'}') => with_paren_balance!(-1, Token::BraceClose),
            Some(b'\'') => {
                return Ok(ControlFlow::Break(ok!(self.eat_string(b'\''))));
            }
            Some(b'"') => {
                return Ok(ControlFlow::Break(ok!(self.eat_string(b'"'))));
            }
            Some(c) if c.is_ascii_digit() => return Ok(ControlFlow::Break(ok!(self.eat_number()))),
            _ => None,
        };
        if let Some(op) = op {
            self.advance(1);
            Ok(ControlFlow::Break((op, self.span(old_loc))))
        } else {
            Ok(ControlFlow::Break(ok!(self.eat_identifier())))
        }
    }
}

/// Utility function to quickly tokenize into an iterator.
#[cfg(any(test, feature = "unstable_machinery"))]
pub fn tokenize(
    input: &str,
    in_expr: bool,
    syntax_config: SyntaxConfig,
    whitespace_config: WhitespaceConfig,
) -> impl Iterator<Item = Result<(Token<'_>, Span), Error>> {
    // This function is unused in minijinja itself, it's only used in tests and in the
    // unstable machinery as a convenient alternative to the tokenizer.
    let mut tokenizer =
        Tokenizer::new(input, "<string>", in_expr, syntax_config, whitespace_config);
    std::iter::from_fn(move || tokenizer.next_token().transpose())
}

#[cfg(test)]
mod tests {
    use super::*;

    use similar_asserts::assert_eq;

    #[test]
    fn test_is_basic_tag() {
        assert_eq!(
            skip_basic_tag(" raw %}", "raw", "%}", false),
            Some((7, Whitespace::Default))
        );
        assert_eq!(skip_basic_tag(" raw %}", "endraw", "%}", false), None);
        assert_eq!(
            skip_basic_tag("  raw  %}", "raw", "%}", false),
            Some((9, Whitespace::Default))
        );
        assert_eq!(
            skip_basic_tag("  raw  -%}", "raw", "%}", false),
            Some((10, Whitespace::Remove))
        );
        assert_eq!(
            skip_basic_tag("  raw  +%}", "raw", "%}", false),
            Some((10, Whitespace::Preserve))
        );
    }

    #[test]
    fn test_basic_identifiers() {
        fn assert_ident(s: &str) {
            match tokenize(s, true, Default::default(), Default::default()).next() {
                Some(Ok((Token::Ident(ident), _))) if ident == s => {}
                _ => panic!("did not get a matching token result: {s:?}"),
            }
        }

        fn assert_not_ident(s: &str) {
            let res = tokenize(s, true, Default::default(), Default::default())
                .collect::<Result<Vec<_>, _>>();
            if let Ok(tokens) = res {
                if let &[(Token::Ident(_), _)] = &tokens[..] {
                    panic!("got a single ident for {s:?}")
                }
            }
        }

        assert_ident("foo_bar_baz");
        assert_ident("_foo_bar_baz");
        assert_ident("_42world");
        assert_ident("_world42");
        assert_ident("world42");
        assert_not_ident("42world");

        #[cfg(feature = "unicode")]
        {
            assert_ident("foo");
            assert_ident("föö");
            assert_ident("き");
            assert_ident("_");
            assert_not_ident("1a");
            assert_not_ident("a-");
            assert_not_ident("🐍a");
            assert_not_ident("a🐍🐍");
            assert_ident("ᢅ");
            assert_ident("ᢆ");
            assert_ident("℘");
            assert_ident("℮");
            assert_not_ident("·");
            assert_ident("a·");
        }
    }
}
