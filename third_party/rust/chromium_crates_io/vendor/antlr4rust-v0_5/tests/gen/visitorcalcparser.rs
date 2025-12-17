// Generated from VisitorCalc.g4 by ANTLR 4.13.2
#![allow(dead_code)]
#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]
#![allow(nonstandard_style)]
#![allow(unused_imports)]
#![allow(unused_mut)]
#![allow(unused_braces)]
use antlr4rust::PredictionContextCache;
use antlr4rust::parser::{Parser, BaseParser, ParserRecog, ParserNodeType};
use antlr4rust::token_stream::TokenStream;
use antlr4rust::TokenSource;
use antlr4rust::parser_atn_simulator::ParserATNSimulator;
use antlr4rust::errors::*;
use antlr4rust::rule_context::{BaseRuleContext, CustomRuleContext, RuleContext};
use antlr4rust::recognizer::{Recognizer,Actions};
use antlr4rust::atn_deserializer::ATNDeserializer;
use antlr4rust::dfa::DFA;
use antlr4rust::atn::{ATN, INVALID_ALT};
use antlr4rust::error_strategy::{ErrorStrategy, DefaultErrorStrategy};
use antlr4rust::parser_rule_context::{BaseParserRuleContext, ParserRuleContext,cast,cast_mut};
use antlr4rust::tree::*;
use antlr4rust::token::{TOKEN_EOF,OwningToken,Token};
use antlr4rust::int_stream::EOF;
use antlr4rust::vocabulary::{Vocabulary,VocabularyImpl};
use antlr4rust::token_factory::{CommonTokenFactory,TokenFactory, TokenAware};
use super::visitorcalclistener::*;
use super::visitorcalcvisitor::*;

use antlr4rust::lazy_static;
use antlr4rust::{TidAble,TidExt};

use std::marker::PhantomData;
use std::sync::Arc;
use std::rc::Rc;
use std::convert::TryFrom;
use std::cell::RefCell;
use std::ops::{DerefMut, Deref};
use std::borrow::{Borrow,BorrowMut};
use std::any::{Any,TypeId};

		pub const VisitorCalc_INT:i32=1; 
		pub const VisitorCalc_MUL:i32=2; 
		pub const VisitorCalc_DIV:i32=3; 
		pub const VisitorCalc_ADD:i32=4; 
		pub const VisitorCalc_SUB:i32=5; 
		pub const VisitorCalc_WS:i32=6;
	pub const VisitorCalc_EOF:i32=EOF;
	pub const RULE_s:usize = 0; 
	pub const RULE_expr:usize = 1;
	pub const ruleNames: [&'static str; 2] =  [
		"s", "expr"
	];


	pub const _LITERAL_NAMES: [Option<&'static str>;6] = [
		None, None, Some("'*'"), Some("'/'"), Some("'+'"), Some("'-'")
	];
	pub const _SYMBOLIC_NAMES: [Option<&'static str>;7]  = [
		None, Some("INT"), Some("MUL"), Some("DIV"), Some("ADD"), Some("SUB"), 
		Some("WS")
	];
	lazy_static!{
	    static ref _shared_context_cache: Arc<PredictionContextCache> = Arc::new(PredictionContextCache::new());
		static ref VOCABULARY: Box<dyn Vocabulary> = Box::new(VocabularyImpl::new(_LITERAL_NAMES.iter(), _SYMBOLIC_NAMES.iter(), None));
	}


type BaseParserType<'input, I> =
	BaseParser<'input,VisitorCalcParserExt<'input>, I, VisitorCalcParserContextType , dyn VisitorCalcListener<'input> + 'input >;

type TokenType<'input> = <LocalTokenFactory<'input> as TokenFactory<'input>>::Tok;
pub type LocalTokenFactory<'input> = CommonTokenFactory;

pub type VisitorCalcTreeWalker<'input,'a> =
	ParseTreeWalker<'input, 'a, VisitorCalcParserContextType , dyn VisitorCalcListener<'input> + 'a>;

/// Parser for VisitorCalc grammar
pub struct VisitorCalcParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	base:BaseParserType<'input,I>,
	interpreter:Arc<ParserATNSimulator>,
	_shared_context_cache: Box<PredictionContextCache>,
    pub err_handler: Box<dyn ErrorStrategy<'input,BaseParserType<'input,I> > >,
}

impl<'input, I> VisitorCalcParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    pub fn set_error_strategy(&mut self, strategy: Box<dyn ErrorStrategy<'input,BaseParserType<'input,I> > >) {
        self.err_handler = strategy
    }

    pub fn with_strategy(input: I, strategy: Box<dyn ErrorStrategy<'input,BaseParserType<'input,I> > >) -> Self {
		antlr4rust::recognizer::check_version("0","5");
		let interpreter = Arc::new(ParserATNSimulator::new(
			_ATN.clone(),
			_decision_to_DFA.clone(),
			_shared_context_cache.clone(),
		));
		Self {
			base: BaseParser::new_base_parser(
				input,
				Arc::clone(&interpreter),
				VisitorCalcParserExt{
					_pd: Default::default(),
				}
			),
			interpreter,
            _shared_context_cache: Box::new(PredictionContextCache::new()),
            err_handler: strategy,
        }
    }

}

type DynStrategy<'input,I> = Box<dyn ErrorStrategy<'input,BaseParserType<'input,I>> + 'input>;

impl<'input, I> VisitorCalcParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    pub fn with_dyn_strategy(input: I) -> Self{
    	Self::with_strategy(input,Box::new(DefaultErrorStrategy::new()))
    }
}

impl<'input, I> VisitorCalcParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    pub fn new(input: I) -> Self{
    	Self::with_strategy(input,Box::new(DefaultErrorStrategy::new()))
    }
}

/// Trait for monomorphized trait object that corresponds to the nodes of parse tree generated for VisitorCalcParser
pub trait VisitorCalcParserContext<'input>:
	for<'x> Listenable<dyn VisitorCalcListener<'input> + 'x > + 
	for<'x> Visitable<dyn VisitorCalcVisitor<'input> + 'x > + 
	ParserRuleContext<'input, TF=LocalTokenFactory<'input>, Ctx=VisitorCalcParserContextType>
{}

antlr4rust::coerce_from!{ 'input : VisitorCalcParserContext<'input> }

impl<'input, 'x, T> VisitableDyn<T> for dyn VisitorCalcParserContext<'input> + 'input
where
    T: VisitorCalcVisitor<'input> + 'x,
{
    fn accept_dyn(&self, visitor: &mut T) {
        self.accept(visitor as &mut (dyn VisitorCalcVisitor<'input> + 'x))
    }
}

impl<'input> VisitorCalcParserContext<'input> for TerminalNode<'input,VisitorCalcParserContextType> {}
impl<'input> VisitorCalcParserContext<'input> for ErrorNode<'input,VisitorCalcParserContextType> {}

antlr4rust::tid! { impl<'input> TidAble<'input> for dyn VisitorCalcParserContext<'input> + 'input }

antlr4rust::tid! { impl<'input> TidAble<'input> for dyn VisitorCalcListener<'input> + 'input }

pub struct VisitorCalcParserContextType;
antlr4rust::tid!{VisitorCalcParserContextType}

impl<'input> ParserNodeType<'input> for VisitorCalcParserContextType{
	type TF = LocalTokenFactory<'input>;
	type Type = dyn VisitorCalcParserContext<'input> + 'input;
}

impl<'input, I> Deref for VisitorCalcParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    type Target = BaseParserType<'input,I>;

    fn deref(&self) -> &Self::Target {
        &self.base
    }
}

impl<'input, I> DerefMut for VisitorCalcParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.base
    }
}

pub struct VisitorCalcParserExt<'input>{
	_pd: PhantomData<&'input str>,
}

impl<'input> VisitorCalcParserExt<'input>{
}
antlr4rust::tid! { VisitorCalcParserExt<'a> }

impl<'input> TokenAware<'input> for VisitorCalcParserExt<'input>{
	type TF = LocalTokenFactory<'input>;
}

impl<'input,I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>> ParserRecog<'input, BaseParserType<'input,I>> for VisitorCalcParserExt<'input>{}

impl<'input,I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>> Actions<'input, BaseParserType<'input,I>> for VisitorCalcParserExt<'input>{
	fn get_grammar_file_name(&self) -> & str{ "VisitorCalc.g4"}

   	fn get_rule_names(&self) -> &[& str] {&ruleNames}

   	fn get_vocabulary(&self) -> &dyn Vocabulary { &**VOCABULARY }
	fn sempred(_localctx: Option<&(dyn VisitorCalcParserContext<'input> + 'input)>, rule_index: i32, pred_index: i32,
			   recog:&mut BaseParserType<'input,I>
	)->bool{
		match rule_index {
					1 => VisitorCalcParser::<'input,I>::expr_sempred(_localctx.and_then(|x|x.downcast_ref()), pred_index, recog),
			_ => true
		}
	}
}

impl<'input, I> VisitorCalcParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	fn expr_sempred(_localctx: Option<&ExprContext<'input>>, pred_index:i32,
						recog:&mut <Self as Deref>::Target
		) -> bool {
		match pred_index {
				0=>{
					recog.precpred(None, 2)
				}
				1=>{
					recog.precpred(None, 1)
				}
			_ => true
		}
	}
}
//------------------- s ----------------
pub type SContextAll<'input> = SContext<'input>;


pub type SContext<'input> = BaseParserRuleContext<'input,SContextExt<'input>>;

#[derive(Clone)]
pub struct SContextExt<'input>{
ph:PhantomData<&'input str>
}

impl<'input> VisitorCalcParserContext<'input> for SContext<'input>{}

impl<'input,'a> Listenable<dyn VisitorCalcListener<'input> + 'a> for SContext<'input>{
		fn enter(&self,listener: &mut (dyn VisitorCalcListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.enter_every_rule(self)?;
			listener.enter_s(self);
			Ok(())
		}
		fn exit(&self,listener: &mut (dyn VisitorCalcListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.exit_s(self);
			listener.exit_every_rule(self)?;
			Ok(())
		}
}

impl<'input,'a> Visitable<dyn VisitorCalcVisitor<'input> + 'a> for SContext<'input>{
	fn accept(&self,visitor: &mut (dyn VisitorCalcVisitor<'input> + 'a)) {
		visitor.visit_s(self);
	}
}

impl<'input> CustomRuleContext<'input> for SContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = VisitorCalcParserContextType;
	fn get_rule_index(&self) -> usize { RULE_s }
	//fn type_rule_index() -> usize where Self: Sized { RULE_s }
}
antlr4rust::tid!{SContextExt<'a>}

impl<'input> SContextExt<'input>{
	fn new(parent: Option<Rc<dyn VisitorCalcParserContext<'input> + 'input > >, invoking_state: i32) -> Rc<SContextAll<'input>> {
		Rc::new(
			BaseParserRuleContext::new_parser_ctx(parent, invoking_state,SContextExt{

				ph:PhantomData
			}),
		)
	}
}

pub trait SContextAttrs<'input>: VisitorCalcParserContext<'input> + BorrowMut<SContextExt<'input>>{

fn expr(&self) -> Option<Rc<ExprContextAll<'input>>> where Self:Sized{
	self.child_of_type(0)
}
/// Retrieves first TerminalNode corresponding to token EOF
/// Returns `None` if there is no child corresponding to token EOF
fn EOF(&self) -> Option<Rc<TerminalNode<'input,VisitorCalcParserContextType>>> where Self:Sized{
	self.get_token(VisitorCalc_EOF, 0)
}

}

impl<'input> SContextAttrs<'input> for SContext<'input>{}

impl<'input, I> VisitorCalcParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	pub fn s(&mut self,)
	-> Result<Rc<SContextAll<'input>>,ANTLRError> {
		let mut recog = self;
		let _parentctx = recog.ctx.take();
		let mut _localctx = SContextExt::new(_parentctx.clone(), recog.base.get_state());
        recog.base.enter_rule(_localctx.clone(), 0, RULE_s);
        let mut _localctx: Rc<SContextAll> = _localctx;
		let result: Result<(), ANTLRError> = (|| {

			//recog.base.enter_outer_alt(_localctx.clone(), 1)?;
			recog.base.enter_outer_alt(None, 1)?;
			{
			/*InvokeRule expr*/
			recog.base.set_state(4);
			recog.expr_rec(0)?;

			recog.base.set_state(5);
			recog.base.match_token(VisitorCalc_EOF,&mut recog.err_handler)?;

			}
			Ok(())
		})();
		match result {
		Ok(_)=>{},
        Err(e @ ANTLRError::FallThrough(_)) => return Err(e),
		Err(ref re) => {
				//_localctx.exception = re;
				recog.err_handler.report_error(&mut recog.base, re);
				recog.err_handler.recover(&mut recog.base, re)?;
			}
		}
		recog.base.exit_rule()?;

		Ok(_localctx)
	}
}
//------------------- expr ----------------
#[derive(Debug)]
pub enum ExprContextAll<'input>{
	AddContext(AddContext<'input>),
	NumberContext(NumberContext<'input>),
	MultiplyContext(MultiplyContext<'input>),
Error(ExprContext<'input>)
}
antlr4rust::tid!{ExprContextAll<'a>}

impl<'input> antlr4rust::parser_rule_context::DerefSeal for ExprContextAll<'input>{}

impl<'input> VisitorCalcParserContext<'input> for ExprContextAll<'input>{}

impl<'input> Deref for ExprContextAll<'input>{
	type Target = dyn ExprContextAttrs<'input> + 'input;
	fn deref(&self) -> &Self::Target{
		use ExprContextAll::*;
		match self{
			AddContext(inner) => inner,
			NumberContext(inner) => inner,
			MultiplyContext(inner) => inner,
Error(inner) => inner
		}
	}
}
impl<'input,'a> Visitable<dyn VisitorCalcVisitor<'input> + 'a> for ExprContextAll<'input>{
	fn accept(&self, visitor: &mut (dyn VisitorCalcVisitor<'input> + 'a)) { self.deref().accept(visitor) }
}
impl<'input,'a> Listenable<dyn VisitorCalcListener<'input> + 'a> for ExprContextAll<'input>{
    fn enter(&self, listener: &mut (dyn VisitorCalcListener<'input> + 'a)) -> Result<(), ANTLRError> { self.deref().enter(listener) }
    fn exit(&self, listener: &mut (dyn VisitorCalcListener<'input> + 'a)) -> Result<(), ANTLRError> { self.deref().exit(listener) }
}



pub type ExprContext<'input> = BaseParserRuleContext<'input,ExprContextExt<'input>>;

#[derive(Clone)]
pub struct ExprContextExt<'input>{
ph:PhantomData<&'input str>
}

impl<'input> VisitorCalcParserContext<'input> for ExprContext<'input>{}

impl<'input,'a> Listenable<dyn VisitorCalcListener<'input> + 'a> for ExprContext<'input>{
}

impl<'input,'a> Visitable<dyn VisitorCalcVisitor<'input> + 'a> for ExprContext<'input>{
}

impl<'input> CustomRuleContext<'input> for ExprContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = VisitorCalcParserContextType;
	fn get_rule_index(&self) -> usize { RULE_expr }
	//fn type_rule_index() -> usize where Self: Sized { RULE_expr }
}
antlr4rust::tid!{ExprContextExt<'a>}

impl<'input> ExprContextExt<'input>{
	fn new(parent: Option<Rc<dyn VisitorCalcParserContext<'input> + 'input > >, invoking_state: i32) -> Rc<ExprContextAll<'input>> {
		Rc::new(
		ExprContextAll::Error(
			BaseParserRuleContext::new_parser_ctx(parent, invoking_state,ExprContextExt{

				ph:PhantomData
			}),
		)
		)
	}
}

pub trait ExprContextAttrs<'input>: VisitorCalcParserContext<'input> + BorrowMut<ExprContextExt<'input>>{


}

impl<'input> ExprContextAttrs<'input> for ExprContext<'input>{}

pub type AddContext<'input> = BaseParserRuleContext<'input,AddContextExt<'input>>;

pub trait AddContextAttrs<'input>: VisitorCalcParserContext<'input>{
	fn expr_all(&self) ->  Vec<Rc<ExprContextAll<'input>>> where Self:Sized{
		self.children_of_type()
	}
	fn expr(&self, i: usize) -> Option<Rc<ExprContextAll<'input>>> where Self:Sized{
		self.child_of_type(i)
	}
	/// Retrieves first TerminalNode corresponding to token ADD
	/// Returns `None` if there is no child corresponding to token ADD
	fn ADD(&self) -> Option<Rc<TerminalNode<'input,VisitorCalcParserContextType>>> where Self:Sized{
		self.get_token(VisitorCalc_ADD, 0)
	}
	/// Retrieves first TerminalNode corresponding to token SUB
	/// Returns `None` if there is no child corresponding to token SUB
	fn SUB(&self) -> Option<Rc<TerminalNode<'input,VisitorCalcParserContextType>>> where Self:Sized{
		self.get_token(VisitorCalc_SUB, 0)
	}
}

impl<'input> AddContextAttrs<'input> for AddContext<'input>{}

pub struct AddContextExt<'input>{
	base:ExprContextExt<'input>,
	ph:PhantomData<&'input str>
}

antlr4rust::tid!{AddContextExt<'a>}

impl<'input> VisitorCalcParserContext<'input> for AddContext<'input>{}

impl<'input,'a> Listenable<dyn VisitorCalcListener<'input> + 'a> for AddContext<'input>{
	fn enter(&self,listener: &mut (dyn VisitorCalcListener<'input> + 'a)) -> Result<(), ANTLRError> {
		listener.enter_every_rule(self)?;
		listener.enter_add(self);
		Ok(())
	}
	fn exit(&self,listener: &mut (dyn VisitorCalcListener<'input> + 'a)) -> Result<(), ANTLRError> {
		listener.exit_add(self);
		listener.exit_every_rule(self)?;
		Ok(())
	}
}

impl<'input,'a> Visitable<dyn VisitorCalcVisitor<'input> + 'a> for AddContext<'input>{
	fn accept(&self,visitor: &mut (dyn VisitorCalcVisitor<'input> + 'a)) {
		visitor.visit_add(self);
	}
}

impl<'input> CustomRuleContext<'input> for AddContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = VisitorCalcParserContextType;
	fn get_rule_index(&self) -> usize { RULE_expr }
	//fn type_rule_index() -> usize where Self: Sized { RULE_expr }
}

impl<'input> Borrow<ExprContextExt<'input>> for AddContext<'input>{
	fn borrow(&self) -> &ExprContextExt<'input> { &self.base }
}
impl<'input> BorrowMut<ExprContextExt<'input>> for AddContext<'input>{
	fn borrow_mut(&mut self) -> &mut ExprContextExt<'input> { &mut self.base }
}

impl<'input> ExprContextAttrs<'input> for AddContext<'input> {}

impl<'input> AddContextExt<'input>{
	fn new(ctx: &dyn ExprContextAttrs<'input>) -> Rc<ExprContextAll<'input>>  {
		Rc::new(
			ExprContextAll::AddContext(
				BaseParserRuleContext::copy_from(ctx,AddContextExt{
        			base: ctx.borrow().clone(),
        			ph:PhantomData
				})
			)
		)
	}
}

pub type NumberContext<'input> = BaseParserRuleContext<'input,NumberContextExt<'input>>;

pub trait NumberContextAttrs<'input>: VisitorCalcParserContext<'input>{
	/// Retrieves first TerminalNode corresponding to token INT
	/// Returns `None` if there is no child corresponding to token INT
	fn INT(&self) -> Option<Rc<TerminalNode<'input,VisitorCalcParserContextType>>> where Self:Sized{
		self.get_token(VisitorCalc_INT, 0)
	}
}

impl<'input> NumberContextAttrs<'input> for NumberContext<'input>{}

pub struct NumberContextExt<'input>{
	base:ExprContextExt<'input>,
	ph:PhantomData<&'input str>
}

antlr4rust::tid!{NumberContextExt<'a>}

impl<'input> VisitorCalcParserContext<'input> for NumberContext<'input>{}

impl<'input,'a> Listenable<dyn VisitorCalcListener<'input> + 'a> for NumberContext<'input>{
	fn enter(&self,listener: &mut (dyn VisitorCalcListener<'input> + 'a)) -> Result<(), ANTLRError> {
		listener.enter_every_rule(self)?;
		listener.enter_number(self);
		Ok(())
	}
	fn exit(&self,listener: &mut (dyn VisitorCalcListener<'input> + 'a)) -> Result<(), ANTLRError> {
		listener.exit_number(self);
		listener.exit_every_rule(self)?;
		Ok(())
	}
}

impl<'input,'a> Visitable<dyn VisitorCalcVisitor<'input> + 'a> for NumberContext<'input>{
	fn accept(&self,visitor: &mut (dyn VisitorCalcVisitor<'input> + 'a)) {
		visitor.visit_number(self);
	}
}

impl<'input> CustomRuleContext<'input> for NumberContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = VisitorCalcParserContextType;
	fn get_rule_index(&self) -> usize { RULE_expr }
	//fn type_rule_index() -> usize where Self: Sized { RULE_expr }
}

impl<'input> Borrow<ExprContextExt<'input>> for NumberContext<'input>{
	fn borrow(&self) -> &ExprContextExt<'input> { &self.base }
}
impl<'input> BorrowMut<ExprContextExt<'input>> for NumberContext<'input>{
	fn borrow_mut(&mut self) -> &mut ExprContextExt<'input> { &mut self.base }
}

impl<'input> ExprContextAttrs<'input> for NumberContext<'input> {}

impl<'input> NumberContextExt<'input>{
	fn new(ctx: &dyn ExprContextAttrs<'input>) -> Rc<ExprContextAll<'input>>  {
		Rc::new(
			ExprContextAll::NumberContext(
				BaseParserRuleContext::copy_from(ctx,NumberContextExt{
        			base: ctx.borrow().clone(),
        			ph:PhantomData
				})
			)
		)
	}
}

pub type MultiplyContext<'input> = BaseParserRuleContext<'input,MultiplyContextExt<'input>>;

pub trait MultiplyContextAttrs<'input>: VisitorCalcParserContext<'input>{
	fn expr_all(&self) ->  Vec<Rc<ExprContextAll<'input>>> where Self:Sized{
		self.children_of_type()
	}
	fn expr(&self, i: usize) -> Option<Rc<ExprContextAll<'input>>> where Self:Sized{
		self.child_of_type(i)
	}
	/// Retrieves first TerminalNode corresponding to token MUL
	/// Returns `None` if there is no child corresponding to token MUL
	fn MUL(&self) -> Option<Rc<TerminalNode<'input,VisitorCalcParserContextType>>> where Self:Sized{
		self.get_token(VisitorCalc_MUL, 0)
	}
	/// Retrieves first TerminalNode corresponding to token DIV
	/// Returns `None` if there is no child corresponding to token DIV
	fn DIV(&self) -> Option<Rc<TerminalNode<'input,VisitorCalcParserContextType>>> where Self:Sized{
		self.get_token(VisitorCalc_DIV, 0)
	}
}

impl<'input> MultiplyContextAttrs<'input> for MultiplyContext<'input>{}

pub struct MultiplyContextExt<'input>{
	base:ExprContextExt<'input>,
	ph:PhantomData<&'input str>
}

antlr4rust::tid!{MultiplyContextExt<'a>}

impl<'input> VisitorCalcParserContext<'input> for MultiplyContext<'input>{}

impl<'input,'a> Listenable<dyn VisitorCalcListener<'input> + 'a> for MultiplyContext<'input>{
	fn enter(&self,listener: &mut (dyn VisitorCalcListener<'input> + 'a)) -> Result<(), ANTLRError> {
		listener.enter_every_rule(self)?;
		listener.enter_multiply(self);
		Ok(())
	}
	fn exit(&self,listener: &mut (dyn VisitorCalcListener<'input> + 'a)) -> Result<(), ANTLRError> {
		listener.exit_multiply(self);
		listener.exit_every_rule(self)?;
		Ok(())
	}
}

impl<'input,'a> Visitable<dyn VisitorCalcVisitor<'input> + 'a> for MultiplyContext<'input>{
	fn accept(&self,visitor: &mut (dyn VisitorCalcVisitor<'input> + 'a)) {
		visitor.visit_multiply(self);
	}
}

impl<'input> CustomRuleContext<'input> for MultiplyContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = VisitorCalcParserContextType;
	fn get_rule_index(&self) -> usize { RULE_expr }
	//fn type_rule_index() -> usize where Self: Sized { RULE_expr }
}

impl<'input> Borrow<ExprContextExt<'input>> for MultiplyContext<'input>{
	fn borrow(&self) -> &ExprContextExt<'input> { &self.base }
}
impl<'input> BorrowMut<ExprContextExt<'input>> for MultiplyContext<'input>{
	fn borrow_mut(&mut self) -> &mut ExprContextExt<'input> { &mut self.base }
}

impl<'input> ExprContextAttrs<'input> for MultiplyContext<'input> {}

impl<'input> MultiplyContextExt<'input>{
	fn new(ctx: &dyn ExprContextAttrs<'input>) -> Rc<ExprContextAll<'input>>  {
		Rc::new(
			ExprContextAll::MultiplyContext(
				BaseParserRuleContext::copy_from(ctx,MultiplyContextExt{
        			base: ctx.borrow().clone(),
        			ph:PhantomData
				})
			)
		)
	}
}

impl<'input, I> VisitorCalcParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	pub fn  expr(&mut self,)
	-> Result<Rc<ExprContextAll<'input>>,ANTLRError> {
		self.expr_rec(0)
	}

	fn expr_rec(&mut self, _p: i32)
	-> Result<Rc<ExprContextAll<'input>>,ANTLRError> {
		let recog = self;
		let _parentctx = recog.ctx.take();
		let _parentState = recog.base.get_state();
		let mut _localctx = ExprContextExt::new(_parentctx.clone(), recog.base.get_state());
		recog.base.enter_recursion_rule(_localctx.clone(), 2, RULE_expr, _p);
	    let mut _localctx: Rc<ExprContextAll> = _localctx;
        let mut _prevctx = _localctx.clone();
		let _startState = 2;
		let mut _la: i32 = -1;
		let result: Result<(), ANTLRError> = (|| {
			let mut _alt: i32;
			//recog.base.enter_outer_alt(_localctx.clone(), 1)?;
			recog.base.enter_outer_alt(None, 1)?;
			{
			{
			let mut tmp = NumberContextExt::new(&**_localctx);
			recog.ctx = Some(tmp.clone());
			_localctx = tmp;
			_prevctx = _localctx.clone();

			recog.base.set_state(8);
			recog.base.match_token(VisitorCalc_INT,&mut recog.err_handler)?;

			}
			let tmp = recog.input.lt(-1).cloned();
			recog.ctx.as_ref().unwrap().set_stop(tmp);
			recog.base.set_state(18);
			recog.err_handler.sync(&mut recog.base)?;
			_alt = recog.interpreter.adaptive_predict(1,&mut recog.base)?;
			while { _alt!=2 && _alt!=INVALID_ALT } {
				if _alt==1 {
					recog.trigger_exit_rule_event()?;
					_prevctx = _localctx.clone();
					{
					recog.base.set_state(16);
					recog.err_handler.sync(&mut recog.base)?;
					match  recog.interpreter.adaptive_predict(0,&mut recog.base)? {
						1 =>{
							{
							/*recRuleLabeledAltStartAction*/
							let mut tmp = MultiplyContextExt::new(&**ExprContextExt::new(_parentctx.clone(), _parentState));
							recog.push_new_recursion_context(tmp.clone(), _startState, RULE_expr)?;
							_localctx = tmp;
							recog.base.set_state(10);
							if !({let _localctx = Some(_localctx.clone());
							recog.precpred(None, 2)}) {
								Err(FailedPredicateError::new(&mut recog.base, Some("recog.precpred(None, 2)".to_owned()), None))?;
							}
							recog.base.set_state(11);
							_la = recog.base.input.la(1);
							if { !(_la==VisitorCalc_MUL || _la==VisitorCalc_DIV) } {
								recog.err_handler.recover_inline(&mut recog.base)?;

							}
							else {
								if  recog.base.input.la(1)==TOKEN_EOF { recog.base.matched_eof = true };
								recog.err_handler.report_match(&mut recog.base);
								recog.base.consume(&mut recog.err_handler);
							}
							/*InvokeRule expr*/
							recog.base.set_state(12);
							recog.expr_rec(3)?;

							}
						}
					,
						2 =>{
							{
							/*recRuleLabeledAltStartAction*/
							let mut tmp = AddContextExt::new(&**ExprContextExt::new(_parentctx.clone(), _parentState));
							recog.push_new_recursion_context(tmp.clone(), _startState, RULE_expr)?;
							_localctx = tmp;
							recog.base.set_state(13);
							if !({let _localctx = Some(_localctx.clone());
							recog.precpred(None, 1)}) {
								Err(FailedPredicateError::new(&mut recog.base, Some("recog.precpred(None, 1)".to_owned()), None))?;
							}
							recog.base.set_state(14);
							_la = recog.base.input.la(1);
							if { !(_la==VisitorCalc_ADD || _la==VisitorCalc_SUB) } {
								recog.err_handler.recover_inline(&mut recog.base)?;

							}
							else {
								if  recog.base.input.la(1)==TOKEN_EOF { recog.base.matched_eof = true };
								recog.err_handler.report_match(&mut recog.base);
								recog.base.consume(&mut recog.err_handler);
							}
							/*InvokeRule expr*/
							recog.base.set_state(15);
							recog.expr_rec(2)?;

							}
						}

						_ => {}
					}
					} 
				}
				recog.base.set_state(20);
				recog.err_handler.sync(&mut recog.base)?;
				_alt = recog.interpreter.adaptive_predict(1,&mut recog.base)?;
			}
			}
			Ok(())
		})();
		match result {
		Ok(_) => {},
        Err(e @ ANTLRError::FallThrough(_)) => return Err(e),
		Err(ref re)=>{
			//_localctx.exception = re;
			recog.err_handler.report_error(&mut recog.base, re);
	        recog.err_handler.recover(&mut recog.base, re)?;}
		}
		recog.base.unroll_recursion_context(_parentctx)?;

		Ok(_localctx)
	}
}
	lazy_static!{
    static ref _ATN: Arc<ATN> =
        Arc::new(ATNDeserializer::new(None).deserialize(&mut _serializedATN.iter()));
    static ref _decision_to_DFA: Arc<Vec<antlr4rust::RwLock<DFA>>> = {
        let mut dfa = Vec::new();
        let size = _ATN.decision_to_state.len() as i32;
        for i in 0..size {
            dfa.push(DFA::new(
                _ATN.clone(),
                _ATN.get_decision_state(i),
                i,
            ).into())
        }
        Arc::new(dfa)
    };
	static ref _serializedATN: Vec<i32> = vec![
		4, 1, 6, 22, 2, 0, 7, 0, 2, 1, 7, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 1, 17, 8, 1, 10, 1, 12, 1, 20, 
		9, 1, 1, 1, 0, 1, 2, 2, 0, 2, 0, 2, 1, 0, 2, 3, 1, 0, 4, 5, 21, 0, 4, 
		1, 0, 0, 0, 2, 7, 1, 0, 0, 0, 4, 5, 3, 2, 1, 0, 5, 6, 5, 0, 0, 1, 6, 1, 
		1, 0, 0, 0, 7, 8, 6, 1, -1, 0, 8, 9, 5, 1, 0, 0, 9, 18, 1, 0, 0, 0, 10, 
		11, 10, 2, 0, 0, 11, 12, 7, 0, 0, 0, 12, 17, 3, 2, 1, 3, 13, 14, 10, 1, 
		0, 0, 14, 15, 7, 1, 0, 0, 15, 17, 3, 2, 1, 2, 16, 10, 1, 0, 0, 0, 16, 
		13, 1, 0, 0, 0, 17, 20, 1, 0, 0, 0, 18, 16, 1, 0, 0, 0, 18, 19, 1, 0, 
		0, 0, 19, 3, 1, 0, 0, 0, 20, 18, 1, 0, 0, 0, 2, 16, 18
	];
}
