//! Symbols that parser works on
use std::borrow::{Borrow, Cow};

use std::fmt::Formatter;
use std::fmt::{Debug, Display};

use std::sync::atomic::{AtomicIsize, Ordering};

use crate::char_stream::InputData;
use crate::int_stream::EOF;
use crate::token_factory::{INVALID_COMMON, INVALID_OWNING};

use better_any::type_id;

/// Type of tokens that parser considers invalid
pub const TOKEN_INVALID_TYPE: i32 = 0;
/// Type of tokens that DFA can use to advance to next state without consuming actual input token.
/// Should not be created by downstream implementations.
pub const TOKEN_EPSILON: i32 = -2;
/// Min token type that can be assigned to tokens created by downstream implementations.
pub const TOKEN_MIN_USER_TOKEN_TYPE: i32 = 1;
/// Type of EOF token
pub const TOKEN_EOF: i32 = EOF;
/// Default channel lexer emits tokens to
pub const TOKEN_DEFAULT_CHANNEL: i32 = 0;
/// Predefined additional channel for lexer to assign tokens to
pub const TOKEN_HIDDEN_CHANNEL: i32 = 1;
/// Shorthand for TOKEN_HIDDEN_CHANNEL
pub const HIDDEN: i32 = TOKEN_HIDDEN_CHANNEL;

/// Implemented by tokens that are produced by a `TokenFactory`
#[allow(missing_docs)]
pub trait Token: Debug + Display {
    /// Type of the underlying data this token refers to
    type Data: ?Sized + InputData;
    // fn get_source(&self) -> Option<(Box<dyn TokenSource>, Box<dyn CharStream>)>;
    fn get_token_type(&self) -> i32;
    fn get_channel(&self) -> i32 {
        TOKEN_DEFAULT_CHANNEL
    }
    fn get_start(&self) -> isize {
        0
    }
    fn get_stop(&self) -> isize {
        0
    }
    fn get_line(&self) -> isize {
        0
    }
    fn get_column(&self) -> isize {
        0
    }

    fn get_text(&self) -> &Self::Data;
    fn set_text(&mut self, _text: <Self::Data as ToOwned>::Owned) {}

    fn get_token_index(&self) -> isize {
        0
    }
    fn set_token_index(&self, _v: isize) {}

    // fn get_token_source(&self) -> &dyn TokenSource;
    // fn get_input_stream(&self) -> &dyn CharStream;

    /// returns fully owned representation of this token
    fn to_owned(&self) -> OwningToken {
        OwningToken {
            token_type: self.get_token_type(),
            channel: self.get_channel(),
            start: self.get_start(),
            stop: self.get_stop(),
            token_index: AtomicIsize::from(self.get_token_index()),
            line: self.get_line(),
            column: self.get_column(),
            text: self.get_text().to_display(),
            read_only: true,
        }
    }
}

/// Token that owns its data
pub type OwningToken = GenericToken<String>;
/// Most versatile Token that uses Cow to save data
/// Can be used seamlessly switch from owned to zero-copy parsing
pub type CommonToken<'a> = GenericToken<Cow<'a, str>>;

type_id!(OwningToken);
type_id!(CommonToken<'a>);

#[derive(Debug)]
#[allow(missing_docs)]
pub struct GenericToken<T> {
    //    source: Option<(Box<TokenSource>,Box<CharStream>)>,
    pub token_type: i32,
    pub channel: i32,
    pub start: isize,
    pub stop: isize,
    pub token_index: AtomicIsize,
    pub line: isize,
    pub column: isize,
    pub text: T,
    pub read_only: bool,
}

impl<T: Clone> Clone for GenericToken<T>
where
    Self: Token,
{
    fn clone(&self) -> Self {
        Self {
            token_type: self.token_type,
            channel: self.channel,
            start: self.start,
            stop: self.stop,
            token_index: AtomicIsize::new(self.get_token_index()),
            line: self.line,
            column: self.column,
            text: self.text.clone(),
            read_only: false,
        }
    }
}

impl<T: Borrow<str> + Debug> Display for GenericToken<T> {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        let txt = if self.token_type == TOKEN_EOF {
            "<EOF>"
        } else {
            self.text.borrow()
        };
        let txt = txt.replace("\n", "\\n");
        let txt = txt.replace("\r", "\\r");
        let txt = txt.replace("\t", "\\t");
        //        let txt = escape_whitespaces(txt,false);
        f.write_fmt(format_args!(
            "[@{},{}:{}='{}',<{}>{},{}:{}]",
            self.get_token_index(),
            self.start,
            self.stop,
            txt,
            self.token_type,
            if self.channel > 0 {
                ",channel=".to_string() + self.channel.to_string().as_str()
            } else {
                String::new()
            },
            self.line,
            self.column
        ))
    }
}

// impl<T: Borrow<str> + Debug> TokenWrapper for GenericToken<T> { type Inner = Self; }

impl<T: Borrow<str> + Debug> Token for GenericToken<T> {
    type Data = str;

    fn get_token_type(&self) -> i32 {
        self.token_type
    }

    fn get_channel(&self) -> i32 {
        self.channel
    }

    fn get_start(&self) -> isize {
        self.start
    }

    fn get_stop(&self) -> isize {
        self.stop
    }

    fn get_line(&self) -> isize {
        self.line
    }

    fn get_column(&self) -> isize {
        self.column
    }

    // fn get_source(&self) -> Option<(Box<dyn TokenSource>, Box<dyn CharStream>)> {
    //     unimplemented!()
    // }

    fn get_text(&self) -> &str {
        if self.token_type == EOF {
            "<EOF>"
        } else {
            self.text.borrow()
        }
    }

    fn set_text(&mut self, _text: String) {
        unimplemented!()
    }

    fn get_token_index(&self) -> isize {
        self.token_index.load(Ordering::Relaxed)
    }

    fn set_token_index(&self, _v: isize) {
        self.token_index.store(_v, Ordering::Relaxed)
    }

    fn to_owned(&self) -> OwningToken {
        OwningToken {
            token_type: self.token_type,
            channel: self.channel,
            start: self.start,
            stop: self.stop,
            token_index: AtomicIsize::new(self.get_token_index()),
            line: self.line,
            column: self.column,
            text: self.text.borrow().to_owned(),
            read_only: self.read_only,
        }
    }
}

impl Default for &'_ OwningToken {
    fn default() -> Self {
        &INVALID_OWNING
    }
}

impl Default for &'_ CommonToken<'_> {
    fn default() -> Self {
        &INVALID_COMMON
    }
}

//
// impl CommonToken {
//     fn new_common_token(
//         _source: Option<(Box<dyn TokenSource>, Box<dyn CharStream>)>,
//         _token_type: i32,
//         _channel: i32,
//         _start: isize,
//         _stop: isize,
//     ) -> CommonToken {
//         unimplemented!()
//     }
//
//     fn clone(&self) -> CommonToken {
//         unimplemented!()
//     }
// }
