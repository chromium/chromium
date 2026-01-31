//! Error handling and recovery
use std::borrow::Borrow;
use std::error::Error;
use std::fmt;
use std::fmt::{Display, Formatter};
use std::ops::{Deref, DerefMut};
use std::rc::Rc;
use std::sync::Arc;

use crate::atn_simulator::IATNSimulator;
use crate::atn_state::*;
use crate::char_stream::{CharStream, InputData};
use crate::dfa::ScopeExt;
use crate::errors::{ANTLRError, FailedPredicateError, InputMisMatchError, NoViableAltError};
use crate::interval_set::IntervalSet;
use crate::parser::{Parser, ParserNodeType};
use crate::parser_rule_context::ParserRuleContext;
use crate::rule_context::{CustomRuleContext, RuleContext};
use crate::token::{Token, TOKEN_DEFAULT_CHANNEL, TOKEN_EOF, TOKEN_EPSILON, TOKEN_INVALID_TYPE};
use crate::token_factory::TokenFactory;
use crate::transition::RuleTransition;
use crate::tree::Tree;
use crate::utils::escape_whitespaces;
use better_any::{Tid, TidAble};

/// The interface for defining strategies to deal with syntax errors encountered
/// during a parse by ANTLR-generated parsers. We distinguish between three
/// different kinds of errors:
///  - The parser could not figure out which path to take in the ATN (none of
/// the available alternatives could possibly match)
///  - The current input does not match what we were looking for
///  - A predicate evaluated to false
///
/// Implementations of this interface should report syntax errors by calling [`Parser::notifyErrorListeners`]
///
/// [`Parser::notifyErrorListeners`]: crate::parser::Parser::notifyErrorListeners
pub trait ErrorStrategy<'a, T: Parser<'a>>: Tid<'a> {
    ///Reset the error handler state for the specified `recognizer`.
    fn reset(&mut self, recognizer: &mut T);

    /// This method is called when an unexpected symbol is encountered during an
    /// inline match operation, such as `Parser::match`. If the error
    /// strategy successfully recovers from the match failure, this method
    /// returns the `Token` instance which should be treated as the
    /// successful result of the match.
    ///
    /// This method handles the consumption of any tokens - the caller should
    /// **not** call `Parser::consume` after a successful recovery.
    ///
    /// Note that the calling code will not report an error if this method
    /// returns successfully. The error strategy implementation is responsible
    /// for calling `Parser::notifyErrorListeners` as appropriate.
    ///
    /// Returns `ANTLRError` if can't recover from unexpected input symbol
    fn recover_inline(
        &mut self,
        recognizer: &mut T,
    ) -> Result<<T::TF as TokenFactory<'a>>::Tok, ANTLRError>;

    /// This method is called to recover from error `e`. This method is
    /// called after `ErrorStrategy::reportError` by the default error handler
    /// generated for a rule method.
    ///
    ///
    fn recover(&mut self, recognizer: &mut T, e: &ANTLRError) -> Result<(), ANTLRError>;

    /// This method provides the error handler with an opportunity to handle
    /// syntactic or semantic errors in the input stream before they result in a
    /// error.
    ///
    /// The generated code currently contains calls to `ErrorStrategy::sync` after
    /// entering the decision state of a closure block ({@code (...)*} or
    /// {@code (...)+}).</p>
    fn sync(&mut self, recognizer: &mut T) -> Result<(), ANTLRError>;

    /// Tests whether or not {@code recognizer} is in the process of recovering
    /// from an error. In error recovery mode, `Parser::consume` will create
    /// `ErrorNode` leaf instead of `TerminalNode` one  
    fn in_error_recovery_mode(&mut self, recognizer: &mut T) -> bool;

    /// Report any kind of `ANTLRError`. This method is called by
    /// the default exception handler generated for a rule method.
    fn report_error(&mut self, recognizer: &mut T, e: &ANTLRError);

    /// This method is called when the parser successfully matches an input
    /// symbol.
    fn report_match(&mut self, recognizer: &mut T);
}
//
// impl<'a, T: Parser<'a>> Default for Box<dyn ErrorStrategy<'a, T> + 'a> {
//     fn default() -> Self { Box::new(DefaultErrorStrategy::new()) }
// }
//
// /// Error strategy trait object if there is a need to change error strategy at runtime
// /// Supports downcasting.
// pub type DynHandler<'a, T> = Box<dyn ErrorStrategy<'a, T> + 'a>;

// impl<'a, T: Parser<'a> + TidAble<'a>> TidAble<'a> for Box<dyn ErrorStrategy<'a, T> + 'a> {}
better_any::tid! { impl<'a, T> TidAble<'a> for Box<dyn ErrorStrategy<'a, T> + 'a> where T: Parser<'a>}

impl<'a, T: Parser<'a> + TidAble<'a>> ErrorStrategy<'a, T> for Box<dyn ErrorStrategy<'a, T> + 'a> {
    #[inline(always)]
    fn reset(&mut self, recognizer: &mut T) {
        self.deref_mut().reset(recognizer)
    }

    #[inline(always)]
    fn recover_inline(
        &mut self,
        recognizer: &mut T,
    ) -> Result<<T::TF as TokenFactory<'a>>::Tok, ANTLRError> {
        self.deref_mut().recover_inline(recognizer)
    }

    #[inline(always)]
    fn recover(&mut self, recognizer: &mut T, e: &ANTLRError) -> Result<(), ANTLRError> {
        self.deref_mut().recover(recognizer, e)
    }

    #[inline(always)]
    fn sync(&mut self, recognizer: &mut T) -> Result<(), ANTLRError> {
        self.deref_mut().sync(recognizer)
    }

    #[inline(always)]
    fn in_error_recovery_mode(&mut self, recognizer: &mut T) -> bool {
        self.deref_mut().in_error_recovery_mode(recognizer)
    }

    #[inline(always)]
    fn report_error(&mut self, recognizer: &mut T, e: &ANTLRError) {
        self.deref_mut().report_error(recognizer, e)
    }

    #[inline(always)]
    fn report_match(&mut self, recognizer: &mut T) {
        self.deref_mut().report_match(recognizer)
    }
}

/// This is the default implementation of `ErrorStrategy` used for
/// error reporting and recovery in ANTLR parsers.
#[derive(Debug)]
pub struct DefaultErrorStrategy<'input, Ctx: ParserNodeType<'input>> {
    error_recovery_mode: bool,
    last_error_index: isize,
    last_error_states: Option<IntervalSet>,
    next_tokens_state: i32,
    next_tokens_ctx: Option<Rc<Ctx::Type>>,
}

better_any::tid! { impl<'i,Ctx> TidAble<'i> for DefaultErrorStrategy<'i,Ctx> where Ctx: ParserNodeType<'i>}

impl<'input, Ctx: ParserNodeType<'input>> Default for DefaultErrorStrategy<'input, Ctx> {
    fn default() -> Self {
        Self::new()
    }
}

impl<'input, Ctx: ParserNodeType<'input>> DefaultErrorStrategy<'input, Ctx> {
    /// Creates new instance of `DefaultErrorStrategy`
    pub fn new() -> Self {
        Self {
            error_recovery_mode: false,
            last_error_index: -1,
            last_error_states: None,
            next_tokens_state: ATNSTATE_INVALID_STATE_NUMBER,
            next_tokens_ctx: None,
        }
    }

    fn begin_error_condition<T: Parser<'input, Node = Ctx, TF = Ctx::TF>>(
        &mut self,
        _recognizer: &T,
    ) {
        self.error_recovery_mode = true;
    }

    fn end_error_condition<T: Parser<'input, Node = Ctx, TF = Ctx::TF>>(
        &mut self,
        _recognizer: &T,
    ) {
        self.error_recovery_mode = false;
        self.last_error_index = -1;
        self.last_error_states = None;
    }

    fn report_no_viable_alternative<T: Parser<'input, Node = Ctx, TF = Ctx::TF>>(
        &self,
        recognizer: &mut T,
        e: &NoViableAltError,
    ) -> String {
        let input = if e.start_token.token_type == TOKEN_EOF {
            "<EOF>".to_owned()
        } else {
            recognizer.get_input_stream_mut().get_text_from_interval(
                e.start_token.get_token_index(),
                e.base.offending_token.get_token_index(),
            )
        };

        format!("no viable alternative at input '{}'", input)
    }

    fn report_input_mismatch<T: Parser<'input, Node = Ctx, TF = Ctx::TF>>(
        &self,
        recognizer: &T,
        e: &InputMisMatchError,
    ) -> String {
        format!(
            "mismatched input {} expecting {}",
            self.get_token_error_display(&e.base.offending_token),
            e.base
                .get_expected_tokens(recognizer)
                .to_token_string(recognizer.get_vocabulary())
        )
    }

    fn report_failed_predicate<T: Parser<'input, Node = Ctx, TF = Ctx::TF>>(
        &self,
        recognizer: &T,
        e: &FailedPredicateError,
    ) -> String {
        format!(
            "rule {} {}",
            recognizer.get_rule_names()[recognizer.get_parser_rule_context().get_rule_index()],
            e.base.message
        )
    }

    fn report_unwanted_token<T: Parser<'input, Node = Ctx, TF = Ctx::TF>>(
        &mut self,
        recognizer: &mut T,
    ) {
        if self.in_error_recovery_mode(recognizer) {
            return;
        }

        self.begin_error_condition(recognizer);
        let expecting = self.get_expected_tokens(recognizer);
        let expecting = expecting.to_token_string(recognizer.get_vocabulary());
        let t = recognizer.get_current_token().borrow();
        let token_name = self.get_token_error_display(t);
        let msg = format!("extraneous input {} expecting {}", token_name, expecting);
        let t = t.get_token_index();
        recognizer.notify_error_listeners(msg, Some(t), None);
    }

    fn report_missing_token<T: Parser<'input, Node = Ctx, TF = Ctx::TF>>(
        &mut self,
        recognizer: &mut T,
    ) {
        if self.in_error_recovery_mode(recognizer) {
            return;
        }

        self.begin_error_condition(recognizer);
        let expecting = self.get_expected_tokens(recognizer);
        let expecting = expecting.to_token_string(recognizer.get_vocabulary());
        let t = recognizer.get_current_token().borrow();
        let _token_name = self.get_token_error_display(t);
        let msg = format!(
            "missing {} at {}",
            expecting,
            self.get_token_error_display(t)
        );
        let t = t.get_token_index();
        recognizer.notify_error_listeners(msg, Some(t), None);
    }

    fn single_token_insertion<T: Parser<'input, Node = Ctx, TF = Ctx::TF>>(
        &mut self,
        recognizer: &mut T,
    ) -> bool {
        let current_token = recognizer.get_input_stream_mut().la(1);

        let atn = recognizer.get_interpreter().atn();
        let current_state = atn.states[recognizer.get_state() as usize].as_ref();
        let next = current_state
            .get_transitions()
            .first()
            .unwrap()
            .get_target();
        let expect_at_ll2 = atn.next_tokens_in_ctx::<Ctx>(
            atn.states[next as usize].as_ref(),
            Some(recognizer.get_parser_rule_context().deref()),
        );
        if expect_at_ll2.contains(current_token) {
            self.report_missing_token(recognizer);
            return true;
        }
        false
    }

    fn single_token_deletion<'a, T: Parser<'input, Node = Ctx, TF = Ctx::TF>>(
        &mut self,
        recognizer: &'a mut T,
    ) -> Option<&'a <T::TF as TokenFactory<'input>>::Tok> {
        let next_token_type = recognizer.get_input_stream_mut().la(2);
        let expecting = self.get_expected_tokens(recognizer);
        //        println!("expecting {}", expecting.to_token_string(recognizer.get_vocabulary()));
        if expecting.contains(next_token_type) {
            self.report_unwanted_token(recognizer);
            recognizer.consume(self);
            self.report_match(recognizer);
            let matched_symbol = recognizer.get_current_token();
            return Some(matched_symbol);
        }
        None
    }

    fn get_missing_symbol<T: Parser<'input, Node = Ctx, TF = Ctx::TF>>(
        &self,
        recognizer: &mut T,
    ) -> <T::TF as TokenFactory<'input>>::Tok {
        let expected = self.get_expected_tokens(recognizer);
        let expected_token_type = expected.get_min().unwrap_or(TOKEN_INVALID_TYPE) as i32;
        let token_text = if expected_token_type == TOKEN_EOF {
            "<missing EOF>".to_owned()
        } else {
            format!(
                "<missing {}>",
                recognizer
                    .get_vocabulary()
                    .get_display_name(expected_token_type as i32)
            )
        };
        let token_text = <T::TF as TokenFactory<'input>>::Data::from_text(&token_text);
        let mut curr = recognizer.get_current_token().borrow();
        if curr.get_token_type() == TOKEN_EOF {
            curr = recognizer
                .get_input_stream()
                .run(|it| it.get((it.index() - 1).max(0)).borrow());
        }
        let (line, column) = (curr.get_line(), curr.get_column());
        recognizer.get_token_factory().create(
            None::<&mut dyn CharStream<<Ctx::TF as TokenFactory<'input>>::From>>,
            expected_token_type,
            Some(token_text),
            TOKEN_DEFAULT_CHANNEL,
            -1,
            -1,
            line,
            column,
        )
        // Token::to_owned(token.borrow())
        // .modify_with(|it| it.text = token_text)
    }

    fn get_expected_tokens<T: Parser<'input, Node = Ctx, TF = Ctx::TF>>(
        &self,
        recognizer: &T,
    ) -> IntervalSet {
        recognizer.get_expected_tokens()
    }

    fn get_token_error_display<T: Token + ?Sized>(&self, t: &T) -> String {
        let text = t.get_text().to_display();
        self.escape_ws_and_quote(&text)
    }

    fn escape_ws_and_quote(&self, s: &str) -> String {
        format!("'{}'", escape_whitespaces(s, false))
    }

    fn get_error_recovery_set<T: Parser<'input, Node = Ctx, TF = Ctx::TF>>(
        &self,
        recognizer: &T,
    ) -> IntervalSet {
        let atn = recognizer.get_interpreter().atn();
        let mut ctx = Some(recognizer.get_parser_rule_context().clone());
        let mut recover_set = IntervalSet::new();
        while let Some(c) = ctx {
            if c.get_invoking_state() < 0 {
                break;
            }

            let invoking_state = atn.states[c.get_invoking_state() as usize].as_ref();
            let tr = invoking_state.get_transitions().first().unwrap().as_ref();
            let tr = tr.cast::<RuleTransition>();
            let follow = atn.next_tokens(atn.states[tr.follow_state as usize].as_ref());
            recover_set.add_set(follow);
            ctx = c.get_parent_ctx();
        }
        recover_set.remove_one(TOKEN_EPSILON);
        recover_set
    }

    fn consume_until<T: Parser<'input, Node = Ctx, TF = Ctx::TF>>(
        &mut self,
        recognizer: &mut T,
        set: &IntervalSet,
    ) {
        let mut ttype = recognizer.get_input_stream_mut().la(1);
        while ttype != TOKEN_EOF && !set.contains(ttype) {
            recognizer.consume(self);
            ttype = recognizer.get_input_stream_mut().la(1);
        }
    }
}

impl<'a, T: Parser<'a>> ErrorStrategy<'a, T> for DefaultErrorStrategy<'a, T::Node> {
    fn reset(&mut self, recognizer: &mut T) {
        self.end_error_condition(recognizer)
    }

    fn recover_inline(
        &mut self,
        recognizer: &mut T,
    ) -> Result<<T::TF as TokenFactory<'a>>::Tok, ANTLRError> {
        let t = self
            .single_token_deletion(recognizer)
            .map(|it| it.to_owned());
        if let Some(t) = t {
            recognizer.consume(self);
            return Ok(t);
        }

        if self.single_token_insertion(recognizer) {
            return Ok(self.get_missing_symbol(recognizer));
        }

        if let Some(next_tokens_ctx) = &self.next_tokens_ctx {
            Err(ANTLRError::InputMismatchError(
                InputMisMatchError::with_state(
                    recognizer,
                    self.next_tokens_state,
                    next_tokens_ctx.clone(),
                ),
            ))
        } else {
            Err(ANTLRError::InputMismatchError(InputMisMatchError::new(
                recognizer,
            )))
        }
        //        Err(ANTLRError::IllegalStateError("aaa".to_string()))
    }

    fn recover(&mut self, recognizer: &mut T, _e: &ANTLRError) -> Result<(), ANTLRError> {
        if self.last_error_index == recognizer.get_input_stream_mut().index()
            && self.last_error_states.is_some()
            && self
                .last_error_states
                .as_ref()
                .unwrap()
                .contains(recognizer.get_state())
        {
            recognizer.consume(self)
        }

        self.last_error_index = recognizer.get_input_stream_mut().index();
        self.last_error_states
            .get_or_insert(IntervalSet::new())
            .apply(|x| x.add_one(recognizer.get_state()));
        let follow_set = self.get_error_recovery_set(recognizer);
        self.consume_until(recognizer, &follow_set);
        Ok(())
    }

    fn sync(&mut self, recognizer: &mut T) -> Result<(), ANTLRError> {
        if self.in_error_recovery_mode(recognizer) {
            return Ok(());
        }
        let next = recognizer.get_input_stream_mut().la(1);
        let state =
            recognizer.get_interpreter().atn().states[recognizer.get_state() as usize].as_ref();

        let next_tokens = recognizer.get_interpreter().atn().next_tokens(state);
        //        println!("{:?}",next_tokens);

        if next_tokens.contains(next) {
            self.next_tokens_state = ATNSTATE_INVALID_STATE_NUMBER;
            self.next_tokens_ctx = None;
            return Ok(());
        }

        if next_tokens.contains(TOKEN_EPSILON) {
            if self.next_tokens_ctx.is_none() {
                self.next_tokens_state = recognizer.get_state();
                self.next_tokens_ctx = Some(recognizer.get_parser_rule_context().clone());
            }
            return Ok(());
        }

        match state.get_state_type_id() {
            ATNSTATE_BLOCK_START
            | ATNSTATE_PLUS_BLOCK_START
            | ATNSTATE_STAR_BLOCK_START
            | ATNSTATE_STAR_LOOP_ENTRY => {
                if self.single_token_deletion(recognizer).is_none() {
                    return Err(ANTLRError::InputMismatchError(InputMisMatchError::new(
                        recognizer,
                    )));
                }
            }
            ATNSTATE_PLUS_LOOP_BACK | ATNSTATE_STAR_LOOP_BACK => {
                self.report_unwanted_token(recognizer);
                let mut expecting = recognizer.get_expected_tokens();
                expecting.add_set(&self.get_error_recovery_set(recognizer));
                self.consume_until(recognizer, &expecting);
            }
            _ => panic!("invalid ANTState type id"),
        }

        Ok(())
    }

    fn in_error_recovery_mode(&mut self, _recognizer: &mut T) -> bool {
        self.error_recovery_mode
    }

    fn report_error(&mut self, recognizer: &mut T, e: &ANTLRError) {
        if self.in_error_recovery_mode(recognizer) {
            return;
        }

        self.begin_error_condition(recognizer);
        let msg = match e {
            ANTLRError::NoAltError(e) => self.report_no_viable_alternative(recognizer, e),
            ANTLRError::InputMismatchError(e) => self.report_input_mismatch(recognizer, e),
            ANTLRError::PredicateError(e) => self.report_failed_predicate(recognizer, e),
            _ => e.to_string(),
        };
        let offending_token_index = e.get_offending_token().map(|it| it.get_token_index());
        recognizer.notify_error_listeners(msg, offending_token_index, Some(e))
    }

    fn report_match(&mut self, recognizer: &mut T) {
        self.end_error_condition(recognizer);
        //println!("matched token succesfully {}", recognizer.get_input_stream().la(1))
    }
}

/// This implementation of `ANTLRErrorStrategy` responds to syntax errors
/// by immediately canceling the parse operation with a
/// `ParseCancellationException`. The implementation ensures that the
/// [`ParserRuleContext.exception`] field is set for all parse tree nodes
/// that were not completed prior to encountering the error.
///
/// <p> This error strategy is useful in the following scenarios.</p>
///
///  - Two-stage parsing: This error strategy allows the first
/// stage of two-stage parsing to immediately terminate if an error is
/// encountered, and immediately fall back to the second stage. In addition to
/// avoiding wasted work by attempting to recover from errors here, the empty
/// implementation of `sync` improves the performance of
/// the first stage.
///  - Silent validation: When syntax errors are not being
/// reported or logged, and the parse result is simply ignored if errors occur,
/// the `BailErrorStrategy` avoids wasting work on recovering from errors
/// when the result will be ignored either way.
///
/// # Usage
/// ```ignore
/// use antlr4rust::error_strategy::BailErrorStrategy;
/// myparser.err_handler = BailErrorStrategy::new();
/// ```
///
/// [`ParserRuleContext.exception`]: todo
/// */
#[derive(Default, Debug)]
pub struct BailErrorStrategy<'input, Ctx: ParserNodeType<'input>>(
    DefaultErrorStrategy<'input, Ctx>,
);

better_any::tid! {impl<'i,Ctx> TidAble<'i> for BailErrorStrategy<'i,Ctx> where Ctx:ParserNodeType<'i> }

impl<'input, Ctx: ParserNodeType<'input>> BailErrorStrategy<'input, Ctx> {
    /// Creates new instance of `BailErrorStrategy`
    pub fn new() -> Self {
        Self(DefaultErrorStrategy::new())
    }

    fn process_error<T: Parser<'input, Node = Ctx, TF = Ctx::TF>>(
        &self,
        recognizer: &mut T,
        e: &ANTLRError,
    ) -> ANTLRError {
        let mut ctx = recognizer.get_parser_rule_context().clone();
        let _: Option<()> = (|| {
            loop {
                ctx.set_exception(e.clone());
                ctx = ctx.get_parent()?
            }
        })();
        ANTLRError::FallThrough(Arc::new(ParseCancelledError(e.clone())))
    }
}

/// `ANTLRError::FallThrough` Error returned `BailErrorStrategy` to bail out from parsing
#[derive(Debug)]
pub struct ParseCancelledError(ANTLRError);

impl Error for ParseCancelledError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        Some(&self.0)
    }
}

impl Display for ParseCancelledError {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.write_str("ParseCancelledError, caused by ")?;
        self.0.fmt(f)
    }
}

impl<'a, T: Parser<'a>> ErrorStrategy<'a, T> for BailErrorStrategy<'a, T::Node> {
    #[inline(always)]
    fn reset(&mut self, recognizer: &mut T) {
        self.0.reset(recognizer)
    }

    #[cold]
    fn recover_inline(
        &mut self,
        recognizer: &mut T,
    ) -> Result<<T::TF as TokenFactory<'a>>::Tok, ANTLRError> {
        let err = ANTLRError::InputMismatchError(InputMisMatchError::new(recognizer));

        Err(self.process_error(recognizer, &err))
    }

    #[cold]
    fn recover(&mut self, recognizer: &mut T, e: &ANTLRError) -> Result<(), ANTLRError> {
        Err(self.process_error(recognizer, e))
    }

    #[inline(always)]
    fn sync(&mut self, _recognizer: &mut T) -> Result<(), ANTLRError> {
        /* empty */
        Ok(())
    }

    #[inline(always)]
    fn in_error_recovery_mode(&mut self, recognizer: &mut T) -> bool {
        self.0.in_error_recovery_mode(recognizer)
    }

    #[inline(always)]
    fn report_error(&mut self, recognizer: &mut T, e: &ANTLRError) {
        self.0.report_error(recognizer, e)
    }

    #[inline(always)]
    fn report_match(&mut self, _recognizer: &mut T) {}
}
