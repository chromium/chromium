//! Channel based `TokenStream`
use std::borrow::Borrow;

use crate::int_stream::{IntStream, IterWrapper, EOF};
use crate::token::{Token, TOKEN_DEFAULT_CHANNEL, TOKEN_INVALID_TYPE};
use crate::token_factory::TokenFactory;
use crate::token_source::TokenSource;
use crate::token_stream::{TokenStream, UnbufferedTokenStream};

/// Default token stream that skips token that not correspond to current channel.
#[derive(Debug)]
pub struct CommonTokenStream<'input, T: TokenSource<'input>> {
    base: UnbufferedTokenStream<'input, T>,
    channel: i32,
}

impl<'input, T: TokenSource<'input>> CommonTokenStream<'input, T> {
    pub fn base(&self) -> &UnbufferedTokenStream<'input, T> {
        &self.base
    }
}

better_any::tid! { impl<'input,T> TidAble<'input> for CommonTokenStream<'input, T> where T: TokenSource<'input>}

impl<'input, T: TokenSource<'input>> IntStream for CommonTokenStream<'input, T> {
    #[inline]
    fn consume(&mut self) {
        self.base.consume();
        //        self.base.p = self.next_token_on_channel(self.base.p,self.channel);
        //        self.base.current_token_index = self.base.p;
        let next = self.next_token_on_channel(self.base.p, self.channel, 1);
        self.base.seek(next);
        // Ok(())
    }

    #[inline]
    fn la(&mut self, i: isize) -> i32 {
        self.lt(i)
            .map(|t| t.borrow().get_token_type())
            .unwrap_or(TOKEN_INVALID_TYPE)
    }

    #[inline(always)]
    fn mark(&mut self) -> isize {
        0
    }

    #[inline(always)]
    fn release(&mut self, _marker: isize) {}

    #[inline(always)]
    fn index(&self) -> isize {
        self.base.index()
    }

    #[inline(always)]
    fn seek(&mut self, index: isize) {
        self.base.seek(index);
    }

    #[inline(always)]
    fn size(&self) -> isize {
        self.base.size()
    }

    fn get_source_name(&self) -> String {
        self.base.get_source_name()
    }
}

impl<'input, T: TokenSource<'input>> TokenStream<'input> for CommonTokenStream<'input, T> {
    type TF = T::TF;

    #[inline(always)]
    fn lt(&mut self, k: isize) -> Option<&<Self::TF as TokenFactory<'input>>::Tok> {
        if k == 0 {
            return None;
        }
        if k < 0 {
            return self.lb(-k);
        }
        self.lt_inner(k)
    }

    #[inline]
    fn get(&self, index: isize) -> &<Self::TF as TokenFactory<'input>>::Tok {
        self.base.get(index)
    }

    fn get_token_source(&self) -> &dyn TokenSource<'input, TF = Self::TF> {
        self.base.get_token_source()
    }

    fn get_text_from_interval(&self, start: isize, stop: isize) -> String {
        self.base.get_text_from_interval(start, stop)
    }
}

impl<'input, T: TokenSource<'input>> CommonTokenStream<'input, T> {
    /// Creates CommonTokenStream that produces tokens from `TOKEN_DEFAULT_CHANNEL`
    pub fn new(lexer: T) -> CommonTokenStream<'input, T> {
        Self::with_channel(lexer, TOKEN_DEFAULT_CHANNEL)
    }

    /// Creates CommonTokenStream that produces tokens from `channel`
    pub fn with_channel(lexer: T, channel: i32) -> CommonTokenStream<'input, T> {
        let mut r = CommonTokenStream {
            base: UnbufferedTokenStream::new_buffered(lexer),
            channel,
        };
        r.sync(0);
        // todo this is definitively not optimal
        if let Some(tok) = r.lt(1) {
            if tok.borrow().get_channel() != channel {
                r.consume();
            }
        }
        r
    }
    
    pub fn get_dfa_string(&self) -> String {
        self.base.get_dfa_string()
    }

    fn lt_inner(&mut self, k: isize) -> Option<&<T::TF as TokenFactory<'input>>::Tok> {
        let mut i = self.base.p;
        let mut n = 1; // we know tokens[p] is a good one
                       // find k good tokens
        while n < k {
            // skip off-channel tokens, but make sure to not look past EOF
            if self.sync(i + 1) {
                i = self.next_token_on_channel(i + 1, self.channel, 1);
            }
            n += 1;
        }
        //		if ( i>range ) range = i;
        self.base.tokens.get(i as usize)
    }

    /// Restarts this token stream
    pub fn reset(&mut self) {
        self.base.p = 0;
        self.base.current_token_index = 0;
    }

    /// Creates iterator over this token stream
    pub fn iter(&mut self) -> IterWrapper<'_, Self> {
        IterWrapper(self, false)
    }

    fn sync(&mut self, i: isize) -> bool {
        let need = i - self.size() + 1;
        if need > 0 {
            let fetched = self.base.fill(need);
            return fetched >= need;
        }

        true
    }
    //
    //    fn fetch(&self, n: isize) -> int { unimplemented!() }
    //
    //    fn get_tokens(&self, start: isize, stop: isize, types: &IntervalSet) -> Vec<Token> { unimplemented!() }
    //
    //    fn lazy_init(&self) { unimplemented!() }
    //
    //    fn setup(&self) { unimplemented!() }
    //
    //    fn get_token_source(&self) -> TokenSource { unimplemented!() }
    //
    //    fn set_token_source(&self, tokenSource: TokenSource) { unimplemented!() }

    //todo make this const generic over direction
    fn next_token_on_channel(&mut self, mut i: isize, channel: i32, direction: isize) -> isize {
        self.sync(i);
        if i >= self.size() {
            return self.size() - 1;
        }

        let mut token = self.base.tokens[i as usize].borrow();
        while token.get_channel() != channel {
            i += direction;
            if token.get_token_type() == EOF || i < 0 {
                return i;
            }

            self.sync(i);
            token = self.base.tokens[i as usize].borrow();
        }

        i
    }
    //
    //    fn previous_token_on_channel(&self, i: isize, channel: isize) -> int { unimplemented!() }
    //
    //    fn get_hidden_tokens_to_right(&self, tokenIndex: isize, channel: isize) -> Vec<Token> { unimplemented!() }
    //
    //    fn get_hidden_tokens_to_left(&self, tokenIndex: isize, channel: isize) -> Vec<Token> { unimplemented!() }
    //
    //    fn filter_for_channel(&self, left: isize, right: isize, channel: isize) -> Vec<Token> { unimplemented!() }
    //
    //    fn get_source_name(&self) -> String { unimplemented!() }
    //
    //    fn get_all_text(&self) -> String { unimplemented!() }
    //
    //    fn get_text_from_tokens(&self, start: Token, end: Token) -> String { unimplemented!() }
    //
    //    fn get_text_from_rule_context(&self, interval: RuleContext) -> String { unimplemented!() }
    //
    //    fn get_text_from_interval(&self, interval: &Interval) -> String { unimplemented!() }
    //
    //    fn fill(&self) { unimplemented!() }
    //
    //    fn adjust_seek_index(&self, i: isize) -> int { unimplemented!() }

    fn lb(
        &mut self,
        k: isize,
    ) -> Option<&<<Self as TokenStream<'input>>::TF as TokenFactory<'input>>::Tok> {
        if k == 0 || (self.base.p - k) < 0 {
            return None;
        }

        let mut i = self.base.p;
        let mut n = 1;
        // find k good tokens looking backwards
        while n <= k && i > 0 {
            // skip off-channel tokens
            i = self.next_token_on_channel(i - 1, self.channel, -1);
            n += 1;
        }
        if i < 0 {
            return None;
        }

        self.base.tokens.get(i as usize)
    }

    //    fn get_number_of_on_channel_tokens(&self) -> int { unimplemented!() }
    
}
