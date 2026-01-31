//! `IntStream` that produces tokens for Parser
use std::borrow::Borrow;
use std::cmp::min;
use std::marker::PhantomData;

use crate::char_stream::InputData;
use crate::int_stream::{IntStream, IterWrapper};
use crate::token::{OwningToken, Token, TOKEN_EOF, TOKEN_INVALID_TYPE};
use crate::token_factory::TokenFactory;
use crate::token_source::TokenSource;
use std::fmt::{Debug, Formatter};

/// An `IntSteam` of `Token`s
///
/// Used as an input for `Parser`s
/// If there is an existing source of tokens, you should implement
/// `TokenSource`, not `TokenStream`
pub trait TokenStream<'input>: IntStream {
    /// Token factory that created tokens in this stream
    type TF: TokenFactory<'input> + 'input;

    /// Lookahead for tokens, same as `IntSteam::la` but return reference to full token
    fn lt(&mut self, k: isize) -> Option<&<Self::TF as TokenFactory<'input>>::Tok>;
    /// Returns reference to token at `index`
    fn get(&self, index: isize) -> &<Self::TF as TokenFactory<'input>>::Tok;

    /// Token source that produced data for tokens for this stream
    fn get_token_source(&self) -> &dyn TokenSource<'input, TF = Self::TF>;
    //    fn set_token_source(&self,source: Box<TokenSource>);
    /// Get combined text of all tokens in this stream
    fn get_all_text(&self) -> String {
        self.get_text_from_interval(0, self.size() - 1)
    }
    /// Get combined text of tokens in start..=stop interval
    fn get_text_from_interval(&self, start: isize, stop: isize) -> String;
    //    fn get_text_from_rule_context(&self,context: RuleContext) -> String;
    /// Get combined text of tokens in between `a` and `b`
    fn get_text_from_tokens<T: Token + ?Sized>(&self, a: &T, b: &T) -> String
    where
        Self: Sized,
    {
        self.get_text_from_interval(a.get_token_index(), b.get_token_index())
    }
}

/// Iterator over tokens in `T`
#[derive(Debug)]
pub struct TokenIter<'a, 'input: 'a, T: TokenStream<'input>>(
    &'a mut T,
    bool,
    PhantomData<fn() -> &'input str>,
);

impl<'a, 'input: 'a, T: TokenStream<'input>> Iterator for TokenIter<'a, 'input, T> {
    type Item = OwningToken;

    fn next(&mut self) -> Option<Self::Item> {
        if self.1 {
            return None;
        }
        let result = self.0.lt(1).unwrap().borrow().to_owned();
        if result.get_token_type() == TOKEN_EOF {
            self.1 = true;
        } else {
            self.0.consume();
        }
        Some(result)
    }
}

/// Token stream that keeps all data in internal Vec
pub struct UnbufferedTokenStream<'input, T: TokenSource<'input>> {
    token_source: T,
    pub(crate) tokens: Vec<<T::TF as TokenFactory<'input>>::Tok>,
    //todo prev token for lt(-1)
    pub(crate) current_token_index: isize,
    markers_count: isize,
    pub(crate) p: isize,
    fetched_eof: bool,
}
better_any::tid! { impl<'input,T> TidAble<'input> for UnbufferedTokenStream<'input, T> where T: TokenSource<'input>}

impl<'input, T: TokenSource<'input>> Debug for UnbufferedTokenStream<'input, T> {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("UnbufferedTokenStream")
            .field("tokens", &self.tokens)
            .field("current_token_index", &self.current_token_index)
            .field("markers_count", &self.markers_count)
            .field("p(buffer index)", &self.p)
            .finish()
    }
}

impl<'input, T: TokenSource<'input>> UnbufferedTokenStream<'input, T> {
    /// Creates iterator over this token stream
    pub fn iter(&mut self) -> IterWrapper<'_, Self> {
        IterWrapper(self, false)
    }

    /// Creates iterator over tokens in this token stream
    pub fn token_iter(&mut self) -> TokenIter<'_, 'input, Self> {
        TokenIter(self, false, PhantomData)
    }

    /// Creates token stream that keeps all tokens inside
    pub fn new_buffered(source: T) -> Self {
        let mut a = UnbufferedTokenStream::new_unbuffered(source);
        a.mark();
        a
    }

    /// Creates token stream that keeps only tokens required by `mark`
    pub fn new_unbuffered(source: T) -> Self {
        UnbufferedTokenStream {
            token_source: source,
            tokens: vec![],
            current_token_index: 0,
            markers_count: 0,
            p: 0,
            fetched_eof: false,
        }
    }

    pub fn get_dfa_string(&self) -> String {
        self.token_source.get_dfa_string()
    }

    fn sync(&mut self, want: isize) {
        let need = (self.p + want - 1) - self.tokens.len() as isize + 1;
        if need > 0 {
            self.fill(need);
        }
    }

    fn get_buffer_start_index(&self) -> isize {
        self.current_token_index - self.p
    }

    pub(crate) fn fill(&mut self, need: isize) -> isize {
        for i in 0..need {
            if !self.tokens.is_empty()
                && self.tokens.last().unwrap().borrow().get_token_type() == TOKEN_EOF
            {
                return i;
            }
            let token = self.token_source.next_token();
            token
                .borrow()
                .set_token_index(self.get_buffer_start_index() + self.tokens.len() as isize);
            self.tokens.push(token);
        }

        need
    }
}

impl<'input, T: TokenSource<'input>> TokenStream<'input> for UnbufferedTokenStream<'input, T> {
    type TF = T::TF;

    #[inline]
    fn lt(&mut self, i: isize) -> Option<&<Self::TF as TokenFactory<'input>>::Tok> {
        if i == -1 {
            return self.tokens.get(self.p as usize - 1);
        }

        self.sync(i);

        self.tokens.get((self.p + i - 1) as usize)
    }

    #[inline]
    fn get(&self, index: isize) -> &<Self::TF as TokenFactory<'input>>::Tok {
        &self.tokens[(index - self.get_buffer_start_index()) as usize]
    }

    fn get_token_source(&self) -> &dyn TokenSource<'input, TF = Self::TF> {
        &self.token_source
    }

    fn get_text_from_interval(&self, start: isize, stop: isize) -> String {
        //        println!("get_text_from_interval {}..{}",start,stop);
        //        println!("all tokens {:?}",self.tokens.iter().map(|x|x.as_ref().to_owned()).collect::<Vec<OwningToken>>());

        let buffer_start_index = self.get_buffer_start_index();
        let buffer_stop_index = buffer_start_index + self.tokens.len() as isize - 1;
        if start < buffer_start_index || stop > buffer_stop_index {
            panic!(
                "interval {}..={} not in token buffer window: {}..{}",
                start, stop, buffer_start_index, buffer_stop_index
            );
        }

        let a = start - buffer_start_index;
        let b = stop - buffer_start_index;

        let mut buf = String::new();
        for i in a..(b + 1) {
            let t = self.tokens[i as usize].borrow();
            if t.get_token_type() == TOKEN_EOF {
                break;
            }
            buf.push_str(&t.get_text().to_display());
        }

        buf
    }
}

impl<'input, T: TokenSource<'input>> IntStream for UnbufferedTokenStream<'input, T> {
    #[inline]
    fn consume(&mut self) {
        if self.fetched_eof {
            panic!("cannot consume EOF");
        }
        if self.la(1) == TOKEN_EOF {
            self.fetched_eof = true;
        }

        if self.p == self.tokens.len() as isize && self.markers_count == 0 {
            self.tokens.clear();
            self.p = -1;
        }

        self.p += 1;
        self.current_token_index += 1;

        self.sync(1);
        // Ok(())
    }

    #[inline]
    fn la(&mut self, i: isize) -> i32 {
        self.lt(i)
            .map(|t| t.borrow().get_token_type())
            .unwrap_or(TOKEN_INVALID_TYPE)
    }

    #[inline]
    fn mark(&mut self) -> isize {
        self.markers_count += 1;
        -self.markers_count
    }

    #[inline]
    fn release(&mut self, marker: isize) {
        assert_eq!(marker, -self.markers_count);

        self.markers_count -= 1;
        if self.markers_count == 0 && self.p > 0 {
            self.tokens.drain(0..self.p as usize);
            //todo drain assembly is almost 2x longer than
            // unsafe manual copy but need to bench before using unsafe
            //let new_len = self.tokens.len() - self.p as usize;
            // unsafe {
            //     // drop first p elements
            //     for i in 0..(self.p as usize) {
            //         drop_in_place(&mut self.tokens[i]);
            //     }
            //     // move len-p elements to beginning
            //     std::intrinsics::copy(
            //         &self.tokens[self.p as usize],
            //         &mut self.tokens[0],
            //         new_len,
            //     );
            //     self.tokens.set_len(new_len);
            // }

            self.p = 0;
        }
    }

    #[inline(always)]
    fn index(&self) -> isize {
        self.current_token_index
    }

    #[inline]
    fn seek(&mut self, mut index: isize) {
        if self.current_token_index == index {
            return;
        }
        if index > self.current_token_index {
            self.sync(index - self.current_token_index);
            index = min(index, self.get_buffer_start_index() + self.size() + 1);
        }
        let i = index - self.get_buffer_start_index();
        if i < 0 || i >= self.tokens.len() as isize {
            panic!()
        }

        self.p = i;
        self.current_token_index = index;
    }

    #[inline(always)]
    fn size(&self) -> isize {
        self.tokens.len() as isize
    }

    fn get_source_name(&self) -> String {
        self.token_source.get_source_name()
    }
}
