//! How Lexer should produce tokens
use std::borrow::Cow::{Borrowed, Owned};
use std::borrow::{Borrow, Cow};

use std::fmt::{Debug, Formatter};
use std::marker::PhantomData;

use std::sync::atomic::AtomicIsize;

use typed_arena::Arena;

use crate::char_stream::{CharStream, InputData};
use crate::token::Token;
use crate::token::{CommonToken, OwningToken, TOKEN_INVALID_TYPE};
use better_any::TidAble;

lazy_static! {
    pub(crate) static ref COMMON_TOKEN_FACTORY_DEFAULT: Box<CommonTokenFactory> =
        Box::new(CommonTokenFactory {});
    pub(crate) static ref INVALID_OWNING: Box<OwningToken> = Box::new(OwningToken {
        token_type: TOKEN_INVALID_TYPE,
        channel: 0,
        start: -1,
        stop: -1,
        token_index: AtomicIsize::new(-1),
        line: -1,
        column: -1,
        text: "<invalid>".to_owned(),
        read_only: true,
    });
    pub(crate) static ref INVALID_COMMON: Box<CommonToken<'static>> = Box::new(CommonToken {
        token_type: TOKEN_INVALID_TYPE,
        channel: 0,
        start: -1,
        stop: -1,
        token_index: AtomicIsize::new(-1),
        line: -1,
        column: -1,
        text: Borrowed("<invalid>"),
        read_only: true,
    });
}

/// Trait for creating tokens.
pub trait TokenFactory<'a>: TidAble<'a> + Sized {
    /// Type of tokens emitted by this factory.
    type Inner: Token<Data = Self::Data> + ?Sized + 'a;
    /// Ownership of the emitted tokens
    type Tok: Borrow<Self::Inner> + Clone + 'a + Debug;
    // can relax InputData to just ToOwned here?
    /// Type of the underlying storage
    type Data: InputData + ?Sized;
    /// Type of the `CharStream` that factory can produce tokens from
    type From;

    /// Creates token either from `sourse` or from pure data in `text`
    /// Either `source` or `text` are not None
    fn create<T>(
        &'a self,
        source: Option<&mut T>,
        ttype: i32,
        text: Option<<Self::Data as ToOwned>::Owned>,
        channel: i32,
        start: isize,
        stop: isize,
        line: isize,
        column: isize,
    ) -> Self::Tok
    where
        T: CharStream<Self::From> + ?Sized;

    /// Creates invalid token
    /// Invalid tokens must have `TOKEN_INVALID_TYPE` token type.
    fn create_invalid() -> Self::Tok;

    /// Creates `Self::Data` representation for `from` for lexer to work with
    /// when it does not need to create full token   
    fn get_data(from: Self::From) -> Cow<'a, Self::Data>;
}

/// Default token factory
#[derive(Default, Debug)]
pub struct CommonTokenFactory;

better_any::tid! {CommonTokenFactory}

impl Default for &'_ CommonTokenFactory {
    fn default() -> Self {
        &COMMON_TOKEN_FACTORY_DEFAULT
    }
}

impl<'a> TokenFactory<'a> for CommonTokenFactory {
    type Inner = CommonToken<'a>;
    type Tok = Box<Self::Inner>;
    type Data = str;
    type From = Cow<'a, str>;

    #[inline]
    fn create<T>(
        &'a self,
        source: Option<&mut T>,
        ttype: i32,
        text: Option<String>,
        channel: i32,
        start: isize,
        stop: isize,
        line: isize,
        column: isize,
    ) -> Self::Tok
    where
        T: CharStream<Self::From> + ?Sized,
    {
        let text = match (text, source) {
            (Some(t), _) => Owned(t),
            (None, Some(x)) => {
                if stop >= x.size() || start >= x.size() {
                    Borrowed("<EOF>")
                } else {
                    x.get_text(start, stop)
                }
            }
            _ => Borrowed(""),
        };
        Box::new(CommonToken {
            token_type: ttype,
            channel,
            start,
            stop,
            token_index: AtomicIsize::new(-1),
            line,
            column,
            text,
            read_only: false,
        })
    }

    fn create_invalid() -> Self::Tok {
        INVALID_COMMON.clone()
    }

    fn get_data(from: Self::From) -> Cow<'a, Self::Data> {
        from
    }
}

/// Token factory that produces heap allocated
/// `OwningToken`s
#[derive(Default, Debug)]
pub struct OwningTokenFactory;

better_any::tid! {OwningTokenFactory}

impl<'a> TokenFactory<'a> for OwningTokenFactory {
    type Inner = OwningToken;
    type Tok = Box<Self::Inner>;
    type Data = str;
    type From = String;

    #[inline]
    fn create<T>(
        &'a self,
        source: Option<&mut T>,
        ttype: i32,
        text: Option<String>,
        channel: i32,
        start: isize,
        stop: isize,
        line: isize,
        column: isize,
    ) -> Self::Tok
    where
        T: CharStream<String> + ?Sized,
    {
        let text = match (text, source) {
            (Some(t), _) => t,
            (None, Some(x)) => {
                if stop >= x.size() || start >= x.size() {
                    "<EOF>".to_owned()
                } else {
                    x.get_text(start, stop)
                }
            }
            _ => String::new(),
        };
        Box::new(OwningToken {
            token_type: ttype,
            channel,
            start,
            stop,
            token_index: AtomicIsize::new(-1),
            line,
            column,
            text,
            read_only: false,
        })
    }

    fn create_invalid() -> Self::Tok {
        INVALID_OWNING.clone()
    }

    fn get_data(from: Self::From) -> Cow<'a, Self::Data> {
        from.into()
    }
}

// pub struct DynFactory<'input,TF:TokenFactory<'.into()input>>(TF) where TF::Tok:CoerceUnsized<Box<dyn Token+'input>>;
// impl <'input,TF:TokenFactory<'input>> TokenFactory<'input> for DynFactory<'input,TF>
// where TF::Tok:CoerceUnsized<Box<dyn Token+'input>>
// {
//
// }

///Arena token factory that contains `OwningToken`s
pub type ArenaOwningFactory<'a> = ArenaFactory<'a, OwningTokenFactory, OwningToken>;
///Arena token factory that contains `CommonToken`s
pub type ArenaCommonFactory<'a> = ArenaFactory<'a, CommonTokenFactory, CommonToken<'a>>;

/// This is a wrapper for Token factory that allows to allocate tokens in separate arena.
/// It can allow to significantly improve performance by passing Tokens by references everywhere.
///
/// Requires `&'a Tok: Default` bound to produce invalid tokens, which can be easily implemented
/// like this:
/// ```text
/// lazy_static!{ static ref INVALID_TOKEN:Box<CustomToken> = ... }
/// impl Default for &'_ CustomToken {
///     fn default() -> Self { &**INVALID_TOKEN }
/// }
/// ```
/// or if possible just
/// ```text
/// const INVALID_TOKEN:CustomToken = ...
/// ```
// Box is used here because it is almost always should be used for token factory
pub struct ArenaFactory<'input, TF, T> {
    arena: Arena<T>,
    factory: TF,
    pd: PhantomData<&'input str>,
}

better_any::tid! {impl<'input,TF,T> TidAble<'input> for ArenaFactory<'input,TF,T>}

impl<TF: Debug, T> Debug for ArenaFactory<'_, TF, T> {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("ArenaFactory")
            .field("arena", &"Arena")
            .field("factory", &self.factory)
            .finish()
    }
}

impl<TF, T> Default for ArenaFactory<'_, TF, T>
where
    TF: Default,
{
    fn default() -> Self {
        Self {
            arena: Default::default(),
            factory: Default::default(),
            pd: Default::default(),
        }
    }
}

impl<'input, TF, Tok> TokenFactory<'input> for ArenaFactory<'input, TF, Tok>
where
    TF: TokenFactory<'input, Tok = Box<Tok>, Inner = Tok>,
    Tok: Token<Data = TF::Data> + Clone + TidAble<'input>,
    for<'a> &'a Tok: Default,
{
    type Inner = Tok;
    type Tok = &'input Tok;
    type Data = TF::Data;
    type From = TF::From;

    #[inline]
    fn create<T>(
        &'input self,
        source: Option<&mut T>,
        ttype: i32,
        text: Option<<Self::Data as ToOwned>::Owned>,
        channel: i32,
        start: isize,
        stop: isize,
        line: isize,
        column: isize,
    ) -> Self::Tok
    where
        T: CharStream<Self::From> + ?Sized,
    {
        // todo remove redundant allocation
        let token = self
            .factory
            .create(source, ttype, text, channel, start, stop, line, column);
        self.arena.alloc(*token)
    }

    fn create_invalid() -> &'input Tok {
        <&Tok as Default>::default()
    }

    fn get_data(from: Self::From) -> Cow<'input, Self::Data> {
        TF::get_data(from)
    }
}

#[doc(hidden)]
pub trait TokenAware<'input> {
    type TF: TokenFactory<'input> + 'input;
}
