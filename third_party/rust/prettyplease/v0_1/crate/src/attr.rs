use crate::algorithm::Printer;
use crate::INDENT;
use proc_macro2::{Delimiter, TokenStream, TokenTree};
use syn::{AttrStyle, Attribute, Lit, PathArguments};

impl Printer {
    pub fn outer_attrs(&mut self, attrs: &[Attribute]) {
        for attr in attrs {
            if let AttrStyle::Outer = attr.style {
                self.attr(attr);
            }
        }
    }

    pub fn inner_attrs(&mut self, attrs: &[Attribute]) {
        for attr in attrs {
            if let AttrStyle::Inner(_) = attr.style {
                self.attr(attr);
            }
        }
    }

    fn attr(&mut self, attr: &Attribute) {
        if let Some(mut doc) = value_of_attribute("doc", attr) {
            if doc.contains('\n') {
                trim_interior_trailing_spaces(&mut doc);
                self.word(match attr.style {
                    AttrStyle::Outer => "/**",
                    AttrStyle::Inner(_) => "/*!",
                });
                self.word(doc);
                self.word("*/");
            } else {
                trim_trailing_spaces(&mut doc);
                self.word(match attr.style {
                    AttrStyle::Outer => "///",
                    AttrStyle::Inner(_) => "//!",
                });
                self.word(doc);
            }
            self.hardbreak();
        } else if let Some(mut comment) = value_of_attribute("comment", attr) {
            if comment.contains('\n') {
                trim_interior_trailing_spaces(&mut comment);
                self.word("/*");
                self.word(comment);
                self.word("*/");
            } else {
                trim_trailing_spaces(&mut comment);
                self.word("//");
                self.word(comment);
            }
            self.hardbreak();
        } else {
            self.word(match attr.style {
                AttrStyle::Outer => "#",
                AttrStyle::Inner(_) => "#!",
            });
            self.word("[");
            self.path(&attr.path);
            self.attr_tokens(attr.tokens.clone());
            self.word("]");
            self.space();
        }
    }

    fn attr_tokens(&mut self, tokens: TokenStream) {
        let mut stack = Vec::new();
        stack.push((tokens.into_iter().peekable(), Delimiter::None));
        let mut space = Self::nbsp as fn(&mut Self);

        #[derive(PartialEq)]
        enum State {
            Word,
            Punct,
            TrailingComma,
        }

        use State::*;
        let mut state = Word;

        while let Some((tokens, delimiter)) = stack.last_mut() {
            match tokens.next() {
                Some(TokenTree::Ident(ident)) => {
                    if let Word = state {
                        space(self);
                    }
                    self.ident(&ident);
                    state = Word;
                }
                Some(TokenTree::Punct(punct)) => {
                    let ch = punct.as_char();
                    if let (Word, '=') = (state, ch) {
                        self.nbsp();
                    }
                    if ch == ',' && tokens.peek().is_none() {
                        self.trailing_comma(true);
                        state = TrailingComma;
                    } else {
                        self.token_punct(ch);
                        if ch == '=' {
                            self.nbsp();
                        } else if ch == ',' {
                            space(self);
                        }
                        state = Punct;
                    }
                }
                Some(TokenTree::Literal(literal)) => {
                    if let Word = state {
                        space(self);
                    }
                    self.token_literal(&literal);
                    state = Word;
                }
                Some(TokenTree::Group(group)) => {
                    let delimiter = group.delimiter();
                    let stream = group.stream();
                    match delimiter {
                        Delimiter::Parenthesis => {
                            self.word("(");
                            self.cbox(INDENT);
                            self.zerobreak();
                            state = Punct;
                        }
                        Delimiter::Brace => {
                            self.word("{");
                            state = Punct;
                        }
                        Delimiter::Bracket => {
                            self.word("[");
                            state = Punct;
                        }
                        Delimiter::None => {}
                    }
                    stack.push((stream.into_iter().peekable(), delimiter));
                    space = Self::space;
                }
                None => {
                    match delimiter {
                        Delimiter::Parenthesis => {
                            if state != TrailingComma {
                                self.zerobreak();
                            }
                            self.offset(-INDENT);
                            self.end();
                            self.word(")");
                            state = Punct;
                        }
                        Delimiter::Brace => {
                            self.word("}");
                            state = Punct;
                        }
                        Delimiter::Bracket => {
                            self.word("]");
                            state = Punct;
                        }
                        Delimiter::None => {}
                    }
                    stack.pop();
                    if stack.is_empty() {
                        space = Self::nbsp;
                    }
                }
            }
        }
    }
}

fn value_of_attribute(requested: &str, attr: &Attribute) -> Option<String> {
    let is_doc = attr.path.leading_colon.is_none()
        && attr.path.segments.len() == 1
        && matches!(attr.path.segments[0].arguments, PathArguments::None)
        && attr.path.segments[0].ident == requested;
    if !is_doc {
        return None;
    }
    let mut tokens = attr.tokens.clone().into_iter();
    match tokens.next() {
        Some(TokenTree::Punct(punct)) if punct.as_char() == '=' => {}
        _ => return None,
    }
    let literal = match tokens.next() {
        Some(TokenTree::Literal(literal)) => literal,
        _ => return None,
    };
    if tokens.next().is_some() {
        return None;
    }
    match Lit::new(literal) {
        Lit::Str(string) => Some(string.value()),
        _ => None,
    }
}

pub fn has_outer(attrs: &[Attribute]) -> bool {
    for attr in attrs {
        if let AttrStyle::Outer = attr.style {
            return true;
        }
    }
    false
}

pub fn has_inner(attrs: &[Attribute]) -> bool {
    for attr in attrs {
        if let AttrStyle::Inner(_) = attr.style {
            return true;
        }
    }
    false
}

fn trim_trailing_spaces(doc: &mut String) {
    doc.truncate(doc.trim_end_matches(' ').len());
}

fn trim_interior_trailing_spaces(doc: &mut String) {
    if !doc.contains(" \n") {
        return;
    }
    let mut trimmed = String::with_capacity(doc.len());
    let mut lines = doc.split('\n').peekable();
    while let Some(line) = lines.next() {
        if lines.peek().is_some() {
            trimmed.push_str(line.trim_end_matches(' '));
            trimmed.push('\n');
        } else {
            trimmed.push_str(line);
        }
    }
    *doc = trimmed;
}
