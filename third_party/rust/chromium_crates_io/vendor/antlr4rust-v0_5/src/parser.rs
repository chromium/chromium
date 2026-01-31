//! Base parser implementation
use std::borrow::Borrow;
use std::cell::{Cell, RefCell};
use std::marker::PhantomData;
use std::ops::{Deref, DerefMut};
use std::rc::Rc;
use std::sync::Arc;

use crate::atn::ATN;
use crate::atn_simulator::IATNSimulator;
use crate::error_listener::{ConsoleErrorListener, ErrorListener, ProxyErrorListener};
use crate::error_strategy::ErrorStrategy;
use crate::errors::ANTLRError;
use crate::interval_set::IntervalSet;
use crate::parser_atn_simulator::ParserATNSimulator;
use crate::parser_rule_context::ParserRuleContext;
use crate::recognizer::{Actions, Recognizer};
use crate::rule_context::{states_stack, CustomRuleContext, RuleContext};
use crate::token::{Token, TOKEN_EOF};
use crate::token_factory::{TokenAware, TokenFactory};
use crate::token_stream::TokenStream;
use crate::tree::{ErrorNode, Listenable, ParseTreeListener, TerminalNode};
use crate::utils::cell_update;
use crate::vocabulary::Vocabulary;
use crate::{CoerceFrom, CoerceTo};
use better_any::TidAble;

/// parser functionality required for `ParserATNSimulator` to work
#[allow(missing_docs)] // todo rewrite it so downstream crates actually could meaningfully implement it
pub trait Parser<'input>: Recognizer<'input> {
    fn get_interpreter(&self) -> &ParserATNSimulator;

    fn get_token_factory(&self) -> &'input Self::TF;
    fn get_parser_rule_context(&self) -> &Rc<<Self::Node as ParserNodeType<'input>>::Type>;
    //    fn set_parser_rule_context(&self, v: ParserRuleContext);
    fn consume(&mut self, err_handler: &mut impl ErrorStrategy<'input, Self>)
    where
        Self: Sized;
    //    fn get_parse_listeners(&self) -> Vec<ParseTreeListener>;
    //fn sempred(&mut self, _localctx: Option<&dyn ParserRuleContext>, rule_index: i32, action_index: i32) -> bool { true }

    fn precpred(
        &self,
        localctx: Option<&<Self::Node as ParserNodeType<'input>>::Type>,
        precedence: i32,
    ) -> bool;

    //    fn get_error_handler(&self) -> ErrorStrategy;
    //    fn set_error_handler(&self, e: ErrorStrategy);
    fn get_input_stream_mut(&mut self) -> &mut dyn TokenStream<'input, TF = Self::TF>;
    fn get_input_stream(&self) -> &dyn TokenStream<'input, TF = Self::TF>;
    fn get_current_token(&self) -> &<Self::TF as TokenFactory<'input>>::Tok;
    fn get_expected_tokens(&self) -> IntervalSet;

    fn add_error_listener(&mut self, listener: Box<dyn ErrorListener<'input, Self>>)
    where
        Self: Sized;
    fn remove_error_listeners(&mut self);
    fn notify_error_listeners(
        &self,
        msg: String,
        offending_token: Option<isize>,
        err: Option<&ANTLRError>,
    );
    fn get_error_lister_dispatch<'a>(&'a self) -> Box<dyn ErrorListener<'input, Self> + 'a>
    where
        Self: Sized;

    fn is_expected_token(&self, symbol: i32) -> bool;
    fn get_precedence(&self) -> i32;

    fn get_state(&self) -> i32;
    fn set_state(&mut self, v: i32);
    fn get_rule_invocation_stack(&self) -> Vec<String>;
}

// trait CsvContext<'input>: for<'x> Listenable<'input, dyn CsvParseTreeListener<'input,CsvTreeNodeType> + 'x> + ParserRuleContext<'input,TF=CommonTokenFactory,Ctx=CsvTreeNodeType>{}
//
// struct CsvTreeNodeType;
// impl<'a> ParserNodeType<'a> for CsvTreeNodeType{
//     type Type = dyn CsvContext<'a>;
// }

// workaround trait for rustc not being able to handle cycles in trait defenition yet, e.g. `trait A: Super<Assoc=dyn A>{}`
// whyyy rustc... whyyy... (╯°□°）╯︵ ┻━┻  It would have been so much cleaner.
/// Workaround trait for rustc current limitations.
///
/// Basically you can consider it as if context trait for generated parser has been implemented as
/// ```text
/// trait GenratedParserContext:ParserRuleContext<Ctx=dyn GeneratedParserContext>{ ... }
/// ```
/// which is not possible, hence this a bit ugly workaround.
///
/// Implemented by generated parser for the type that is going to carry information about
/// parse tree node.
pub trait ParserNodeType<'input>: TidAble<'input> + Sized {
    /// Shortcut for `Type::TF`
    type TF: TokenFactory<'input> + 'input;
    /// Actual type of the parse tree node
    type Type: ?Sized + ParserRuleContext<'input, Ctx = Self, TF = Self::TF> + 'input;
    // type Visitor: ?Sized + ParseTreeVisitor<'input, Self>;
}

/// ### Main underlying Parser struct
///
/// It is a member of generated parser struct, so
/// almost always you don't need to create it yourself.
/// Generated parser hides complexity of this struct and expose required flexibility via generic parameters
pub struct BaseParser<
    'input,
    Ext, //: 'static, //: ParserRecog<'input, Self> + 'static, // user provided behavior, such as semantic predicates
    I: TokenStream<'input>, // input stream
    Ctx: ParserNodeType<'input, TF = I::TF>, // Ctx::Type is trait object type for tree node of the parser
    T: ParseTreeListener<'input, Ctx> + ?Sized = dyn ParseTreeListener<'input, Ctx>,
> {
    interp: Arc<ParserATNSimulator>,
    /// Rule context parser is currently processing
    pub ctx: Option<Rc<Ctx::Type>>,

    /// Track the {@link ParserRuleContext} objects during the parse and hook
    /// them up using the {@link ParserRuleContext#children} list so that it
    /// forms a parse tree. The {@link ParserRuleContext} returned from the start
    /// rule represents the root of the parse tree.
    ///
    /// <p>Note that if we are not building parse trees, rule contexts only point
    /// upwards. When a rule exits, it returns the context bute that gets garbage
    /// collected if nobody holds a reference. It points upwards but nobody
    /// points at it. </p>
    ///
    /// <p>When we build parse trees, we are adding all of these contexts to
    /// {@link ParserRuleContext#children} list. Contexts are then not candidates
    /// for garbage collection.</p>
    ///
    /// Returns {@code true} if a complete parse tree will be constructed while
    /// parsing, otherwise {@code false}
    pub build_parse_trees: bool,

    /// true if parser reached EOF
    pub matched_eof: bool,

    state: i32,
    /// Token stream that is currently used by this parser
    pub input: I,
    precedence_stack: Vec<i32>,

    parse_listeners: Vec<Box<T>>,
    _syntax_errors: Cell<i32>,
    error_listeners: RefCell<Vec<Box<dyn ErrorListener<'input, Self>>>>,

    ext: Ext,
    pd: PhantomData<fn() -> &'input str>,
}

better_any::tid! {
    impl<'input, Ext, I, Ctx, T> TidAble<'input> for BaseParser<'input,Ext, I, Ctx, T>
    where I: TokenStream<'input>,
        Ctx: ParserNodeType<'input, TF = I::TF>,
        T: ParseTreeListener<'input, Ctx> + ?Sized
}

impl<'input, Ext, I, Ctx, T> Deref for BaseParser<'input, Ext, I, Ctx, T>
where
    Ext: ParserRecog<'input, Self>,
    I: TokenStream<'input>,
    Ctx: ParserNodeType<'input, TF = I::TF>,
    T: ParseTreeListener<'input, Ctx> + ?Sized,
    // Ctx::Type: Listenable<T>,
{
    type Target = Ext;

    fn deref(&self) -> &Self::Target {
        &self.ext
    }
}

impl<'input, Ext, I, Ctx, T> DerefMut for BaseParser<'input, Ext, I, Ctx, T>
where
    Ext: ParserRecog<'input, Self>,
    I: TokenStream<'input>,
    Ctx: ParserNodeType<'input, TF = I::TF>,
    T: ParseTreeListener<'input, Ctx> + ?Sized,
    // Ctx::Type: Listenable<T>,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.ext
    }
}

///
pub trait ParserRecog<'a, P: Recognizer<'a>>: Actions<'a, P> {}

impl<'input, Ext, I, Ctx, T> Recognizer<'input> for BaseParser<'input, Ext, I, Ctx, T>
where
    Ext: ParserRecog<'input, Self>,
    I: TokenStream<'input>,
    Ctx: ParserNodeType<'input, TF = I::TF>,
    T: ParseTreeListener<'input, Ctx> + ?Sized,
    // Ctx::Type: Listenable<T>,
{
    type Node = Ctx;

    fn sempred(
        &mut self,
        localctx: Option<&Ctx::Type>,
        rule_index: i32,
        action_index: i32,
    ) -> bool {
        <Ext as Actions<'input, Self>>::sempred(localctx, rule_index, action_index, self)
    }

    fn get_rule_names(&self) -> &[&str] {
        self.ext.get_rule_names()
    }

    fn get_vocabulary(&self) -> &dyn Vocabulary {
        self.ext.get_vocabulary()
    }

    fn get_grammar_file_name(&self) -> &str {
        self.ext.get_grammar_file_name()
    }

    fn get_atn(&self) -> &ATN {
        self.interp.atn()
    }
}

impl<'input, Ext, I, Ctx, T> TokenAware<'input> for BaseParser<'input, Ext, I, Ctx, T>
where
    Ext: ParserRecog<'input, Self>,
    I: TokenStream<'input>,
    Ctx: ParserNodeType<'input, TF = I::TF>,
    T: ParseTreeListener<'input, Ctx> + ?Sized,
    // Ctx::Type: Listenable<T>,
{
    type TF = I::TF;
}

impl<'input, Ext, I, Ctx, T> Parser<'input> for BaseParser<'input, Ext, I, Ctx, T>
where
    Ext: ParserRecog<'input, Self>,
    I: TokenStream<'input>,
    Ctx: ParserNodeType<'input, TF = I::TF>,
    T: ParseTreeListener<'input, Ctx> + ?Sized,
    Ctx::Type:
        Listenable<T> + CoerceFrom<TerminalNode<'input, Ctx>> + CoerceFrom<ErrorNode<'input, Ctx>>,
    // TerminalNode<'input, Ctx>: CoerceTo<Ctx::Type>,
    // ErrorNode<'input, Ctx>: CoerceTo<Ctx::Type>,
{
    fn get_interpreter(&self) -> &ParserATNSimulator {
        self.interp.as_ref()
    }

    fn get_token_factory(&self) -> &'input Self::TF {
        // &**crate::common_token_factory::COMMON_TOKEN_FACTORY_DEFAULT
        self.input.get_token_source().get_token_factory()
    }

    #[inline(always)]
    fn get_parser_rule_context(&self) -> &Rc<Ctx::Type> {
        self.ctx.as_ref().unwrap()
    }

    fn consume(&mut self, err_handler: &mut impl ErrorStrategy<'input, Self>) {
        let o = self.get_current_token().clone();
        if o.borrow().get_token_type() != TOKEN_EOF {
            self.input.consume();
        }
        if self.build_parse_trees || !self.parse_listeners.is_empty() {
            if err_handler.in_error_recovery_mode(self) {
                // todo report ructc inference issue
                let node: Rc<ErrorNode<'_, Ctx>> = self.create_error_node(o.clone());
                self.ctx
                    .as_deref()
                    .unwrap()
                    .add_child(node.clone().coerce_rc_to());
                for listener in &mut self.parse_listeners {
                    listener.visit_error_node(&*node)
                }
            } else {
                let node: Rc<TerminalNode<'_, Ctx>> = self.create_token_node(o.clone());
                self.ctx
                    .as_deref()
                    .unwrap()
                    .add_child(node.clone().coerce_rc_to());
                for listener in &mut self.parse_listeners {
                    listener.visit_terminal(&*node)
                }
            }
        }
    }

    fn precpred(&self, _localctx: Option<&Ctx::Type>, precedence: i32) -> bool {
        //        localctx.map(|it|println!("check at{}",it.to_string_tree(self)));
        //        println!("{}",self.get_precedence());
        precedence >= self.get_precedence()
    }

    fn get_input_stream_mut(&mut self) -> &mut dyn TokenStream<'input, TF = Self::TF> {
        &mut self.input //.as_mut()
    }

    fn get_input_stream(&self) -> &dyn TokenStream<'input, TF = Self::TF> {
        &self.input
    }

    #[inline]
    fn get_current_token(&self) -> &<Self::TF as TokenFactory<'input>>::Tok {
        self.input.get(self.input.index())
    }

    fn get_expected_tokens(&self) -> IntervalSet {
        let states_stack = states_stack(self.ctx.as_ref().unwrap().clone());
        self.interp
            .atn()
            .get_expected_tokens(self.state, states_stack)
    }

    fn add_error_listener(&mut self, listener: Box<dyn ErrorListener<'input, Self>>) {
        self.error_listeners.borrow_mut().push(listener)
    }

    fn remove_error_listeners(&mut self) {
        self.error_listeners.borrow_mut().clear();
    }

    fn notify_error_listeners(
        &self,
        msg: String,
        offending_token: Option<isize>,
        err: Option<&ANTLRError>,
    ) {
        cell_update(&self._syntax_errors, |it| it + 1);
        let offending_token: Option<&_> = match offending_token {
            None => Some(self.get_current_token().borrow()),
            Some(x) => Some(self.input.get(x).borrow()),
        };
        let line = offending_token.map(|x| x.get_line()).unwrap_or(-1);
        let column = offending_token.map(|x| x.get_column()).unwrap_or(-1);

        for listener in self.error_listeners.borrow().iter() {
            listener.syntax_error(self, offending_token, line, column, &msg, err)
        }
    }

    fn get_error_lister_dispatch<'a>(&'a self) -> Box<dyn ErrorListener<'input, Self> + 'a> {
        Box::new(ProxyErrorListener {
            delegates: self.error_listeners.borrow(),
        })
    }

    fn is_expected_token(&self, _symbol: i32) -> bool {
        unimplemented!()
    }

    fn get_precedence(&self) -> i32 {
        *self.precedence_stack.last().unwrap_or(&-1)
    }

    #[inline(always)]
    fn get_state(&self) -> i32 {
        self.state
    }

    #[inline(always)]
    fn set_state(&mut self, v: i32) {
        self.state = v;
    }

    fn get_rule_invocation_stack(&self) -> Vec<String> {
        let mut vec = Vec::new();
        let rule_names = self.get_rule_names();
        let mut ctx = self.get_parser_rule_context().clone();
        loop {
            let rule_index = ctx.get_rule_index();
            vec.push(rule_names.get(rule_index).unwrap_or(&"n/a").to_string());
            ctx = if let Some(parent) = ctx.get_parent_ctx() {
                parent
            } else {
                break;
            }
        }
        vec
    }

    //    fn get_rule_invocation_stack(&self, c: _) -> Vec<String> {
    //        unimplemented!()
    //    }
}

#[allow(missing_docs)] // todo docs
impl<'input, Ext, I, Ctx, T> BaseParser<'input, Ext, I, Ctx, T>
where
    Ext: ParserRecog<'input, Self>,
    I: TokenStream<'input>,
    Ctx: ParserNodeType<'input, TF = I::TF>,
    T: ParseTreeListener<'input, Ctx> + ?Sized,
    Ctx::Type:
        Listenable<T> + CoerceFrom<TerminalNode<'input, Ctx>> + CoerceFrom<ErrorNode<'input, Ctx>>,
    //     TerminalNode<'input, Ctx>: CoerceTo<Ctx::Type>,
    //     ErrorNode<'input, Ctx>: CoerceTo<Ctx::Type>,
{
    pub fn new_base_parser(input: I, interpreter: Arc<ParserATNSimulator>, ext: Ext) -> Self {
        Self {
            interp: interpreter,
            ctx: None,
            build_parse_trees: true,
            matched_eof: false,
            state: -1,
            input,
            precedence_stack: vec![0],
            parse_listeners: vec![],
            _syntax_errors: Cell::new(0),
            error_listeners: RefCell::new(vec![Box::new(ConsoleErrorListener {})]),
            ext,
            pd: PhantomData,
        }
    }

    //
    //    fn reset(&self) { unimplemented!() }

    #[inline]
    pub fn match_token(
        &mut self,
        ttype: i32,
        err_handler: &mut impl ErrorStrategy<'input, Self>,
    ) -> Result<<I::TF as TokenFactory<'input>>::Tok, ANTLRError> {
        let mut token = self.get_current_token().clone();
        if token.borrow().get_token_type() == ttype {
            if ttype == TOKEN_EOF {
                self.matched_eof = true;
            }

            err_handler.report_match(self);
            self.consume(err_handler);
        } else {
            token = err_handler.recover_inline(self)?;
            if self.build_parse_trees && token.borrow().get_token_index() == -1 {
                self.ctx
                    .as_ref()
                    .unwrap()
                    .add_child(self.create_error_node(token.clone()).coerce_rc_to());
            }
        }
        Ok(token)
    }

    #[inline]
    pub fn match_wildcard(
        &mut self,
        err_handler: &mut impl ErrorStrategy<'input, Self>,
    ) -> Result<<I::TF as TokenFactory<'input>>::Tok, ANTLRError> {
        let mut t = self.get_current_token().clone();
        if t.borrow().get_token_type() > 0 {
            err_handler.report_match(self);
            self.consume(err_handler);
        } else {
            t = err_handler.recover_inline(self)?;
            if self.build_parse_trees && t.borrow().get_token_index() == -1 {
                self.ctx
                    .as_ref()
                    .unwrap()
                    .add_child(self.create_error_node(t.clone()).coerce_rc_to());
            }
        }
        Ok(t)
    }

    /// Adds parse listener for this parser
    /// returns `listener_id` that can be used later to get listener back
    ///
    /// Embedded listener currently must outlive `'input`. If you need to have arbitrary listener use ParseTreeWalker.
    ///
    /// ### Example for listener usage:
    /// todo
    pub fn add_parse_listener<L>(&mut self, listener: Box<L>) -> ListenerId<L>
    where
        L: CoerceTo<T>,
    {
        let id = ListenerId::new(&listener);
        self.parse_listeners.push(listener.coerce_box_to());
        id
    }

    /// Removes parse listener with corresponding `listener_id`, casts it back to user type and returns it to the caller.
    /// `listener_id` is returned when listener is added via `add_parse_listener`
    pub fn remove_parse_listener<L>(&mut self, listener_id: ListenerId<L>) -> Box<L>
    where
        L: CoerceTo<T>,
    {
        let index = self
            .parse_listeners
            .iter()
            .position(|it| ListenerId::new(it).actual_id == listener_id.actual_id)
            .expect("listener not found");
        unsafe { listener_id.into_listener(self.parse_listeners.remove(index)) }
    }

    /// Removes all added parse listeners without returning them
    pub fn remove_parse_listeners(&mut self) {
        self.parse_listeners.clear()
    }

    pub fn trigger_enter_rule_event(&mut self) -> Result<(), ANTLRError> {
        let ctx = self.ctx.as_deref().unwrap();
        for listener in self.parse_listeners.iter_mut() {
            // listener.enter_every_rule(ctx);
            ctx.enter(listener)?;
        }
        Ok(())
    }

    pub fn trigger_exit_rule_event(&mut self) -> Result<(), ANTLRError> {
        let ctx = self.ctx.as_deref().unwrap();
        for listener in self.parse_listeners.iter_mut().rev() {
            ctx.exit(listener)?;
            // listener.exit_every_rule(ctx);
        }
        Ok(())
    }
    //
    //    fn set_token_factory(&self, factory: TokenFactory) { unimplemented!() }
    //
    //
    //    fn get_atn_with_bypass_alts(&self) { unimplemented!() }
    //
    //    fn compile_parse_tree_pattern(&self, pattern, patternRuleIndex: Lexer, lexer: Lexer) { unimplemented!() }
    //
    //    fn set_input_stream(&self, input: TokenStream) { unimplemented!() }
    //
    //    fn set_token_stream(&self, input: TokenStream) { unimplemented!() }

    fn add_context_to_parse_tree(&mut self) {
        let parent = self.ctx.as_ref().unwrap().get_parent_ctx();

        if let Some(parent) = parent {
            parent.add_child(self.ctx.clone().unwrap())
        }
    }

    #[inline]
    pub fn enter_rule(&mut self, localctx: Rc<Ctx::Type>, state: i32, _rule_index: usize) {
        self.set_state(state);
        localctx.set_start(self.input.lt(1).cloned());
        self.ctx = Some(localctx);
        //        let mut localctx = Rc::get_mut(self.ctx.as_mut().unwrap()).unwrap();
        if self.build_parse_trees {
            self.add_context_to_parse_tree()
        }
    }

    #[inline]
    pub fn exit_rule(&mut self) -> Result<(), ANTLRError> {
        if self.matched_eof {
            self.ctx
                .as_ref()
                .unwrap()
                .set_stop(self.input.lt(1).cloned())
        } else {
            self.ctx
                .as_ref()
                .unwrap()
                .set_stop(self.input.lt(-1).cloned())
        }
        self.trigger_exit_rule_event()?;
        self.set_state(self.get_parser_rule_context().get_invoking_state());
        let parent = self.ctx.as_ref().unwrap().get_parent_ctx();
        // mem::replace(&mut self.ctx, parent);
        self.ctx = parent;
        Ok(())
    }

    // todo make new_ctx not option
    #[inline]
    pub fn enter_outer_alt(
        &mut self,
        new_ctx: Option<Rc<Ctx::Type>>,
        alt_num: i32,
    ) -> Result<(), ANTLRError> {
        if let Some(new_ctx) = new_ctx {
            new_ctx.set_alt_number(alt_num);

            let ctx = self.ctx.as_ref().unwrap();
            if self.build_parse_trees && self.ctx.is_some() && !Rc::ptr_eq(&new_ctx, ctx) {
                if let Some(parent) = ctx.get_parent_ctx() {
                    parent.remove_last_child();
                    parent.add_child(new_ctx.clone())
                }
            }

            self.ctx = Some(new_ctx);
        }

        self.trigger_enter_rule_event()
    }

    pub fn enter_recursion_rule(
        &mut self,
        localctx: Rc<Ctx::Type>,
        state: i32,
        _rule_index: usize,
        precedence: i32,
    ) {
        self.set_state(state);
        self.precedence_stack.push(precedence);
        localctx.set_start(self.input.lt(1).cloned());
        //println!("{}",self.input.lt(1).map(Token::to_owned).unwrap());
        self.ctx = Some(localctx);
    }

    pub fn push_new_recursion_context(
        &mut self,
        localctx: Rc<Ctx::Type>,
        state: i32,
        _rule_index: usize,
    ) -> Result<(), ANTLRError> {
        let prev = self.ctx.take().unwrap();
        prev.set_parent(&Some(localctx.clone()));
        prev.set_invoking_state(state);
        prev.set_stop(self.input.lt(-1).cloned());

        //        println!("{}",prev.get_start().unwrap());
        localctx.set_start(Some(prev.start_mut().clone()));
        self.ctx = Some(localctx);

        if self.build_parse_trees {
            self.ctx.as_ref().unwrap().add_child(prev);
        }
        self.trigger_enter_rule_event()
    }

    pub fn unroll_recursion_context(
        &mut self,
        parent_ctx: Option<Rc<Ctx::Type>>,
    ) -> Result<(), ANTLRError> {
        self.precedence_stack.pop();
        let retctx = self.ctx.clone().unwrap();
        retctx.set_stop(self.input.lt(-1).cloned());
        if !self.parse_listeners.is_empty() {
            while self.ctx.as_ref().map(Rc::as_ptr) != parent_ctx.as_ref().map(Rc::as_ptr) {
                self.trigger_exit_rule_event()?;
                self.ctx = self.ctx.as_ref().unwrap().get_parent_ctx()
            }
        } else {
            self.ctx = parent_ctx;
        }

        //self.ctx is now parent
        retctx.set_parent(&self.ctx);

        //        println!("{:?}",self.ctx.as_ref().map(|it|it.to_string_tree(self)));
        if self.build_parse_trees && self.ctx.is_some() {
            self.ctx.as_ref().unwrap().add_child(retctx);
        }
        Ok(())
    }

    fn create_token_node(
        &self,
        token: <I::TF as TokenFactory<'input>>::Tok,
    ) -> Rc<TerminalNode<'input, Ctx>> {
        TerminalNode::new(token).into()
    }

    fn create_error_node(
        &self,
        token: <I::TF as TokenFactory<'input>>::Tok,
    ) -> Rc<ErrorNode<'input, Ctx>> {
        ErrorNode::new(token).into()
    }

    /// Text representation of generated DFA for debugging purposes
    pub fn dump_dfa(&self) {
        let mut seen_one = false;
        for dfa in self.interp.decision_to_dfa() {
            let dfa = dfa.read();
            // because s0 is saved in dfa for Rust version
            if dfa.states.len() > 1 + (dfa.is_precedence_dfa() as usize) {
                if seen_one {
                    println!()
                }
                println!("Decision {}:", dfa.decision);
                print!("{}", dfa.to_string(self.get_vocabulary()));
                seen_one = true;
            }
        }
    }

    //    fn get_invoking_context(&self, ruleIndex: i32) -> ParserRuleContext { unimplemented!() }
    //
    //    fn in_context(&self, context: ParserRuleContext) -> bool { unimplemented!() }
    //
    //    fn get_expected_tokens_within_current_rule(&self) -> * IntervalSet { unimplemented!() }
    //
    //
    //    fn get_rule_index(&self, ruleName: String) -> int { unimplemented!() }
    //
    //    fn get_dfaStrings(&self) -> String { unimplemented!() }
    //
    //    fn get_source_name(&self) -> String { unimplemented!() }
    //
    //    fn set_trace(&self, trace: * TraceListener) { unimplemented!() }
}

/// Allows to safely cast listener back to user type
#[derive(Debug)]
pub struct ListenerId<T: ?Sized> {
    pub(crate) actual_id: usize,
    phantom: PhantomData<fn() -> T>,
}

impl<T: ?Sized> ListenerId<T> {
    fn new(listener: &Box<T>) -> ListenerId<T> {
        ListenerId {
            actual_id: listener.as_ref() as *const T as *const () as usize,
            phantom: Default::default(),
        }
    }
}

impl<T> ListenerId<T> {
    unsafe fn into_listener<U: ?Sized>(self, boxed: Box<U>) -> Box<T> {
        Box::from_raw(Box::into_raw(boxed) as *mut T)
    }
}
