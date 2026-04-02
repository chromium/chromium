use crate::prelude::{
    Delimiter, Group, Ident, LexError, Literal, Punct, Result, Spacing, Span, TokenStream,
    TokenTree,
};
use std::str::FromStr;

/// A helper struct build around a [TokenStream] to make it easier to build code.
#[must_use]
#[derive(Default)]
pub struct StreamBuilder {
    pub(crate) stream: TokenStream,
}

impl StreamBuilder {
    /// Generate a new StreamBuilder
    pub fn new() -> Self {
        Self {
            stream: TokenStream::new(),
        }
    }

    /// Add multiple `TokenTree` items to the stream.
    pub fn extend(&mut self, item: impl IntoIterator<Item = TokenTree>) -> &mut Self {
        self.stream.extend(item);
        self
    }

    /// Append another StreamBuilder to the current StreamBuilder.
    pub fn append(&mut self, builder: StreamBuilder) -> &mut Self {
        self.stream.extend(builder.stream);
        self
    }

    /// Push a single token to the stream.
    pub fn push(&mut self, item: impl Into<TokenTree>) -> &mut Self {
        self.stream.extend([item.into()]);
        self
    }

    /// Attempt to parse the given string as valid Rust code, and append the parsed result to the internal stream.
    ///
    /// Currently panics if the string could not be parsed as valid Rust code.
    pub fn push_parsed(&mut self, item: impl AsRef<str>) -> Result<&mut Self> {
        let tokens = TokenStream::from_str(item.as_ref()).map_err(|e| PushParseError {
            error: e,
            code: item.as_ref().to_string(),
        })?;
        self.stream.extend(tokens);
        Ok(self)
    }

    /// Push a single ident to the stream. An ident is any word that a code file may contain, e.g. `fn`, `struct`, `where`, names of functions and structs, etc.
    pub fn ident(&mut self, ident: Ident) -> &mut Self {
        self.stream.extend([TokenTree::Ident(ident)]);
        self
    }

    /// Push a single ident to the stream. An ident is any word that a code file may contain, e.g. `fn`, `struct`, `where`, names of functions and structs, etc.
    pub fn ident_str(&mut self, ident: impl AsRef<str>) -> &mut Self {
        self.stream.extend([TokenTree::Ident(Ident::new(
            ident.as_ref(),
            Span::call_site(),
        ))]);
        self
    }

    /// Add a group. A group is any block surrounded by `{ .. }`, `[ .. ]` or `( .. )`.
    ///
    /// `delim` indicates which group it is. The `inner` callback is used to fill the contents of the group.
    pub fn group<FN>(&mut self, delim: Delimiter, inner: FN) -> crate::Result<&mut Self>
    where
        FN: FnOnce(&mut StreamBuilder) -> crate::Result<()>,
    {
        let mut stream = StreamBuilder::new();
        inner(&mut stream)?;
        self.stream
            .extend([TokenTree::Group(Group::new(delim, stream.stream))]);
        Ok(self)
    }

    /// Add a single punctuation to the stream. Puncts are single-character tokens like `.`, `<`, `#`, etc
    ///
    /// Note that this should not be used for multi-punct constructions like `::` or `->`. For that use [`puncts`] instead.
    ///
    /// [`puncts`]: #method.puncts
    pub fn punct(&mut self, p: char) -> &mut Self {
        self.stream
            .extend([TokenTree::Punct(Punct::new(p, Spacing::Alone))]);
        self
    }

    /// Add multiple punctuations to the stream. Multi punct tokens are e.g. `::`, `->` and `=>`.
    ///
    /// Note that this is the only way to add multi punct tokens.
    /// If you were to use [`Punct`] to insert `->` it would be inserted as `-` and then `>`, and not form a single token. Rust would interpret this as a "minus sign and then a greater than sign", not as a single arrow.
    pub fn puncts(&mut self, puncts: &str) -> &mut Self {
        self.stream.extend(
            puncts
                .chars()
                .map(|char| TokenTree::Punct(Punct::new(char, Spacing::Joint))),
        );
        self
    }

    /// Add a lifetime to the stream.
    ///
    /// Note that this is the only way to add lifetimes, if you were to do:
    /// ```ignore
    /// builder.punct('\'');
    /// builder.ident_str("static");
    /// ```
    /// It would not add `'static`, but instead it would add `' static` as seperate tokens, and the lifetime would not work.
    pub fn lifetime(&mut self, lt: Ident) -> &mut Self {
        self.stream.extend([
            TokenTree::Punct(Punct::new('\'', Spacing::Joint)),
            TokenTree::Ident(lt),
        ]);
        self
    }

    /// Add a lifetime to the stream.
    ///
    /// Note that this is the only way to add lifetimes, if you were to do:
    /// ```ignore
    /// builder.punct('\'');
    /// builder.ident_str("static");
    /// ```
    /// It would not add `'static`, but instead it would add `' static` as seperate tokens, and the lifetime would not work.
    pub fn lifetime_str(&mut self, lt: &str) -> &mut Self {
        self.stream.extend([
            TokenTree::Punct(Punct::new('\'', Spacing::Joint)),
            TokenTree::Ident(Ident::new(lt, Span::call_site())),
        ]);
        self
    }

    /// Add a literal string (`&'static str`) to the stream.
    pub fn lit_str(&mut self, str: impl AsRef<str>) -> &mut Self {
        self.stream
            .extend([TokenTree::Literal(Literal::string(str.as_ref()))]);
        self
    }

    /// Add an `usize` value to the stream.
    pub fn lit_usize(&mut self, val: usize) -> &mut Self {
        self.stream
            .extend([TokenTree::Literal(Literal::usize_unsuffixed(val))]);
        self
    }

    /// Set the given span on all tokens in the stream. This span is used by rust for e.g. compiler errors, to indicate the position of the error.
    ///
    /// Normally your derive will report an error on the derive, e.g.:
    ///
    /// ```text
    /// #[derive(YourMacro)]
    ///          ^^^^^^^^^
    ///          |
    ///          `self` value is a keyword only available in methods with a `self` parameter
    /// ```
    ///
    /// If you want to improve feedback to the user of your macro, you can use this macro to set the location for a given streambuilder.
    ///
    /// A `span` can be obtained from e.g. an ident with `ident.span()`.
    pub fn set_span_on_all_tokens(&mut self, span: Span) {
        self.stream = std::mem::take(&mut self.stream)
            .into_iter()
            .map(|mut token| {
                token.set_span(span);
                token
            })
            .collect();
    }
}

/// Failed to parse the code passed to [`StreamBuilder::push_parsed`]
///
/// [`StreamBuilder::push_parsed`]: struct.StreamBuilder.html#method.push_parsed
#[derive(Debug)]
pub struct PushParseError {
    /// The parsing error
    pub error: LexError,
    /// The code that was being parsed
    pub code: String,
}
