// Generated from Labels.g4 by ANTLR 4.13.2
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
use super::labelslistener::*;
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

		pub const Labels_T__0:i32=1; 
		pub const Labels_T__1:i32=2; 
		pub const Labels_T__2:i32=3; 
		pub const Labels_T__3:i32=4; 
		pub const Labels_T__4:i32=5; 
		pub const Labels_T__5:i32=6; 
		pub const Labels_ID:i32=7; 
		pub const Labels_INT:i32=8; 
		pub const Labels_WS:i32=9;
	pub const Labels_EOF:i32=EOF;
	pub const RULE_s:usize = 0; 
	pub const RULE_e:usize = 1;
	pub const ruleNames: [&'static str; 2] =  [
		"s", "e"
	];


	pub const _LITERAL_NAMES: [Option<&'static str>;7] = [
		None, Some("'*'"), Some("'+'"), Some("'('"), Some("')'"), Some("'++'"), 
		Some("'--'")
	];
	pub const _SYMBOLIC_NAMES: [Option<&'static str>;10]  = [
		None, None, None, None, None, None, None, Some("ID"), Some("INT"), Some("WS")
	];
	lazy_static!{
	    static ref _shared_context_cache: Arc<PredictionContextCache> = Arc::new(PredictionContextCache::new());
		static ref VOCABULARY: Box<dyn Vocabulary> = Box::new(VocabularyImpl::new(_LITERAL_NAMES.iter(), _SYMBOLIC_NAMES.iter(), None));
	}


type BaseParserType<'input, I> =
	BaseParser<'input,LabelsParserExt<'input>, I, LabelsParserContextType , dyn LabelsListener<'input> + 'input >;

type TokenType<'input> = <LocalTokenFactory<'input> as TokenFactory<'input>>::Tok;
pub type LocalTokenFactory<'input> = CommonTokenFactory;

pub type LabelsTreeWalker<'input,'a> =
	ParseTreeWalker<'input, 'a, LabelsParserContextType , dyn LabelsListener<'input> + 'a>;

/// Parser for Labels grammar
pub struct LabelsParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	base:BaseParserType<'input,I>,
	interpreter:Arc<ParserATNSimulator>,
	_shared_context_cache: Box<PredictionContextCache>,
    pub err_handler: Box<dyn ErrorStrategy<'input,BaseParserType<'input,I> > >,
}

impl<'input, I> LabelsParser<'input, I>
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
				LabelsParserExt{
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

impl<'input, I> LabelsParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    pub fn with_dyn_strategy(input: I) -> Self{
    	Self::with_strategy(input,Box::new(DefaultErrorStrategy::new()))
    }
}

impl<'input, I> LabelsParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    pub fn new(input: I) -> Self{
    	Self::with_strategy(input,Box::new(DefaultErrorStrategy::new()))
    }
}

/// Trait for monomorphized trait object that corresponds to the nodes of parse tree generated for LabelsParser
pub trait LabelsParserContext<'input>:
	for<'x> Listenable<dyn LabelsListener<'input> + 'x > + 
	ParserRuleContext<'input, TF=LocalTokenFactory<'input>, Ctx=LabelsParserContextType>
{}

antlr4rust::coerce_from!{ 'input : LabelsParserContext<'input> }

impl<'input> LabelsParserContext<'input> for TerminalNode<'input,LabelsParserContextType> {}
impl<'input> LabelsParserContext<'input> for ErrorNode<'input,LabelsParserContextType> {}

antlr4rust::tid! { impl<'input> TidAble<'input> for dyn LabelsParserContext<'input> + 'input }

antlr4rust::tid! { impl<'input> TidAble<'input> for dyn LabelsListener<'input> + 'input }

pub struct LabelsParserContextType;
antlr4rust::tid!{LabelsParserContextType}

impl<'input> ParserNodeType<'input> for LabelsParserContextType{
	type TF = LocalTokenFactory<'input>;
	type Type = dyn LabelsParserContext<'input> + 'input;
}

impl<'input, I> Deref for LabelsParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    type Target = BaseParserType<'input,I>;

    fn deref(&self) -> &Self::Target {
        &self.base
    }
}

impl<'input, I> DerefMut for LabelsParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.base
    }
}

pub struct LabelsParserExt<'input>{
	_pd: PhantomData<&'input str>,
}

impl<'input> LabelsParserExt<'input>{
}
antlr4rust::tid! { LabelsParserExt<'a> }

impl<'input> TokenAware<'input> for LabelsParserExt<'input>{
	type TF = LocalTokenFactory<'input>;
}

impl<'input,I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>> ParserRecog<'input, BaseParserType<'input,I>> for LabelsParserExt<'input>{}

impl<'input,I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>> Actions<'input, BaseParserType<'input,I>> for LabelsParserExt<'input>{
	fn get_grammar_file_name(&self) -> & str{ "Labels.g4"}

   	fn get_rule_names(&self) -> &[& str] {&ruleNames}

   	fn get_vocabulary(&self) -> &dyn Vocabulary { &**VOCABULARY }
	fn sempred(_localctx: Option<&(dyn LabelsParserContext<'input> + 'input)>, rule_index: i32, pred_index: i32,
			   recog:&mut BaseParserType<'input,I>
	)->bool{
		match rule_index {
					1 => LabelsParser::<'input,I>::e_sempred(_localctx.and_then(|x|x.downcast_ref()), pred_index, recog),
			_ => true
		}
	}
}

impl<'input, I> LabelsParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	fn e_sempred(_localctx: Option<&EContext<'input>>, pred_index:i32,
						recog:&mut <Self as Deref>::Target
		) -> bool {
		match pred_index {
				0=>{
					recog.precpred(None, 7)
				}
				1=>{
					recog.precpred(None, 6)
				}
				2=>{
					recog.precpred(None, 3)
				}
				3=>{
					recog.precpred(None, 2)
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
	pub q: Option<Rc<EContextAll<'input>>>,
ph:PhantomData<&'input str>
}

impl<'input> LabelsParserContext<'input> for SContext<'input>{}

impl<'input,'a> Listenable<dyn LabelsListener<'input> + 'a> for SContext<'input>{
		fn enter(&self,listener: &mut (dyn LabelsListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.enter_every_rule(self)?;
			listener.enter_s(self);
			Ok(())
		}fn exit(&self,listener: &mut (dyn LabelsListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.exit_s(self);
			listener.exit_every_rule(self)?;
			Ok(())
		}
}

impl<'input> CustomRuleContext<'input> for SContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = LabelsParserContextType;
	fn get_rule_index(&self) -> usize { RULE_s }
	//fn type_rule_index() -> usize where Self: Sized { RULE_s }
}
antlr4rust::tid!{SContextExt<'a>}

impl<'input> SContextExt<'input>{
	fn new(parent: Option<Rc<dyn LabelsParserContext<'input> + 'input > >, invoking_state: i32) -> Rc<SContextAll<'input>> {
		Rc::new(
			BaseParserRuleContext::new_parser_ctx(parent, invoking_state,SContextExt{
				q: None, 

				ph:PhantomData
			}),
		)
	}
}

pub trait SContextAttrs<'input>: LabelsParserContext<'input> + BorrowMut<SContextExt<'input>>{

fn e(&self) -> Option<Rc<EContextAll<'input>>> where Self:Sized{
	self.child_of_type(0)
}

}

impl<'input> SContextAttrs<'input> for SContext<'input>{}

impl<'input, I> LabelsParser<'input, I>
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
			/*InvokeRule e*/
			recog.base.set_state(4);
			let tmp = recog.e_rec(0)?;
			 cast_mut::<_,SContext >(&mut _localctx).q = Some(tmp.clone());
			  

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
//------------------- e ----------------
#[derive(Debug)]
pub enum EContextAll<'input>{
	AddContext(AddContext<'input>),
	ParensContext(ParensContext<'input>),
	MultContext(MultContext<'input>),
	DecContext(DecContext<'input>),
	AnIDContext(AnIDContext<'input>),
	AnIntContext(AnIntContext<'input>),
	IncContext(IncContext<'input>),
Error(EContext<'input>)
}
antlr4rust::tid!{EContextAll<'a>}

impl<'input> antlr4rust::parser_rule_context::DerefSeal for EContextAll<'input>{}

impl<'input> LabelsParserContext<'input> for EContextAll<'input>{}

impl<'input> Deref for EContextAll<'input>{
	type Target = dyn EContextAttrs<'input> + 'input;
	fn deref(&self) -> &Self::Target{
		use EContextAll::*;
		match self{
			AddContext(inner) => inner,
			ParensContext(inner) => inner,
			MultContext(inner) => inner,
			DecContext(inner) => inner,
			AnIDContext(inner) => inner,
			AnIntContext(inner) => inner,
			IncContext(inner) => inner,
Error(inner) => inner
		}
	}
}
impl<'input,'a> Listenable<dyn LabelsListener<'input> + 'a> for EContextAll<'input>{
    fn enter(&self, listener: &mut (dyn LabelsListener<'input> + 'a)) -> Result<(), ANTLRError> { self.deref().enter(listener) }
    fn exit(&self, listener: &mut (dyn LabelsListener<'input> + 'a)) -> Result<(), ANTLRError> { self.deref().exit(listener) }
}



pub type EContext<'input> = BaseParserRuleContext<'input,EContextExt<'input>>;

#[derive(Clone)]
pub struct EContextExt<'input>{
	pub v: String,
ph:PhantomData<&'input str>
}

impl<'input> LabelsParserContext<'input> for EContext<'input>{}

impl<'input,'a> Listenable<dyn LabelsListener<'input> + 'a> for EContext<'input>{
}

impl<'input> CustomRuleContext<'input> for EContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = LabelsParserContextType;
	fn get_rule_index(&self) -> usize { RULE_e }
	//fn type_rule_index() -> usize where Self: Sized { RULE_e }
}
antlr4rust::tid!{EContextExt<'a>}

impl<'input> EContextExt<'input>{
	fn new(parent: Option<Rc<dyn LabelsParserContext<'input> + 'input > >, invoking_state: i32) -> Rc<EContextAll<'input>> {
		let mut _init_v = String::new();

		Rc::new(
		EContextAll::Error(
			BaseParserRuleContext::new_parser_ctx(parent, invoking_state,EContextExt{
				v: _init_v, 

				ph:PhantomData
			}),
		)
		)
	}
}

pub trait EContextAttrs<'input>: LabelsParserContext<'input> + BorrowMut<EContextExt<'input>>{

fn get_v<'a>(&'a self) -> &'a String where 'input: 'a { &self.borrow().v }  
fn set_v(&mut self,attr: String) { self.borrow_mut().v = attr; }  

}

impl<'input> EContextAttrs<'input> for EContext<'input>{}

pub type AddContext<'input> = BaseParserRuleContext<'input,AddContextExt<'input>>;

pub trait AddContextAttrs<'input>: LabelsParserContext<'input>{
	fn e_all(&self) ->  Vec<Rc<EContextAll<'input>>> where Self:Sized{
		self.children_of_type()
	}
	fn e(&self, i: usize) -> Option<Rc<EContextAll<'input>>> where Self:Sized{
		self.child_of_type(i)
	}
}

impl<'input> AddContextAttrs<'input> for AddContext<'input>{}

pub struct AddContextExt<'input>{
	base:EContextExt<'input>,
	pub a: Option<Rc<EContextAll<'input>>>,
	pub b: Option<Rc<EContextAll<'input>>>,
	ph:PhantomData<&'input str>
}

antlr4rust::tid!{AddContextExt<'a>}

impl<'input> LabelsParserContext<'input> for AddContext<'input>{}

impl<'input,'a> Listenable<dyn LabelsListener<'input> + 'a> for AddContext<'input>{
	fn enter(&self,listener: &mut (dyn LabelsListener<'input> + 'a)) -> Result<(), ANTLRError> {
		listener.enter_every_rule(self)?;
		listener.enter_add(self);
		Ok(())
	}
}

impl<'input> CustomRuleContext<'input> for AddContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = LabelsParserContextType;
	fn get_rule_index(&self) -> usize { RULE_e }
	//fn type_rule_index() -> usize where Self: Sized { RULE_e }
}

impl<'input> Borrow<EContextExt<'input>> for AddContext<'input>{
	fn borrow(&self) -> &EContextExt<'input> { &self.base }
}
impl<'input> BorrowMut<EContextExt<'input>> for AddContext<'input>{
	fn borrow_mut(&mut self) -> &mut EContextExt<'input> { &mut self.base }
}

impl<'input> EContextAttrs<'input> for AddContext<'input> {}

impl<'input> AddContextExt<'input>{
	fn new(ctx: &dyn EContextAttrs<'input>) -> Rc<EContextAll<'input>>  {
		Rc::new(
			EContextAll::AddContext(
				BaseParserRuleContext::copy_from(ctx,AddContextExt{
        			a:None, b:None, 
        			base: ctx.borrow().clone(),
        			ph:PhantomData
				})
			)
		)
	}
}

pub type ParensContext<'input> = BaseParserRuleContext<'input,ParensContextExt<'input>>;

pub trait ParensContextAttrs<'input>: LabelsParserContext<'input>{
	fn e(&self) -> Option<Rc<EContextAll<'input>>> where Self:Sized{
		self.child_of_type(0)
	}
}

impl<'input> ParensContextAttrs<'input> for ParensContext<'input>{}

pub struct ParensContextExt<'input>{
	base:EContextExt<'input>,
	pub x: Option<Rc<EContextAll<'input>>>,
	ph:PhantomData<&'input str>
}

antlr4rust::tid!{ParensContextExt<'a>}

impl<'input> LabelsParserContext<'input> for ParensContext<'input>{}

impl<'input,'a> Listenable<dyn LabelsListener<'input> + 'a> for ParensContext<'input>{
	fn enter(&self,listener: &mut (dyn LabelsListener<'input> + 'a)) -> Result<(), ANTLRError> {
		listener.enter_every_rule(self)?;
		listener.enter_parens(self);
		Ok(())
	}
}

impl<'input> CustomRuleContext<'input> for ParensContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = LabelsParserContextType;
	fn get_rule_index(&self) -> usize { RULE_e }
	//fn type_rule_index() -> usize where Self: Sized { RULE_e }
}

impl<'input> Borrow<EContextExt<'input>> for ParensContext<'input>{
	fn borrow(&self) -> &EContextExt<'input> { &self.base }
}
impl<'input> BorrowMut<EContextExt<'input>> for ParensContext<'input>{
	fn borrow_mut(&mut self) -> &mut EContextExt<'input> { &mut self.base }
}

impl<'input> EContextAttrs<'input> for ParensContext<'input> {}

impl<'input> ParensContextExt<'input>{
	fn new(ctx: &dyn EContextAttrs<'input>) -> Rc<EContextAll<'input>>  {
		Rc::new(
			EContextAll::ParensContext(
				BaseParserRuleContext::copy_from(ctx,ParensContextExt{
        			x:None, 
        			base: ctx.borrow().clone(),
        			ph:PhantomData
				})
			)
		)
	}
}

pub type MultContext<'input> = BaseParserRuleContext<'input,MultContextExt<'input>>;

pub trait MultContextAttrs<'input>: LabelsParserContext<'input>{
	fn e_all(&self) ->  Vec<Rc<EContextAll<'input>>> where Self:Sized{
		self.children_of_type()
	}
	fn e(&self, i: usize) -> Option<Rc<EContextAll<'input>>> where Self:Sized{
		self.child_of_type(i)
	}
}

impl<'input> MultContextAttrs<'input> for MultContext<'input>{}

pub struct MultContextExt<'input>{
	base:EContextExt<'input>,
	pub a: Option<Rc<EContextAll<'input>>>,
	pub op: Option<TokenType<'input>>,
	pub b: Option<Rc<EContextAll<'input>>>,
	ph:PhantomData<&'input str>
}

antlr4rust::tid!{MultContextExt<'a>}

impl<'input> LabelsParserContext<'input> for MultContext<'input>{}

impl<'input,'a> Listenable<dyn LabelsListener<'input> + 'a> for MultContext<'input>{
	fn enter(&self,listener: &mut (dyn LabelsListener<'input> + 'a)) -> Result<(), ANTLRError> {
		listener.enter_every_rule(self)?;
		listener.enter_mult(self);
		Ok(())
	}
}

impl<'input> CustomRuleContext<'input> for MultContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = LabelsParserContextType;
	fn get_rule_index(&self) -> usize { RULE_e }
	//fn type_rule_index() -> usize where Self: Sized { RULE_e }
}

impl<'input> Borrow<EContextExt<'input>> for MultContext<'input>{
	fn borrow(&self) -> &EContextExt<'input> { &self.base }
}
impl<'input> BorrowMut<EContextExt<'input>> for MultContext<'input>{
	fn borrow_mut(&mut self) -> &mut EContextExt<'input> { &mut self.base }
}

impl<'input> EContextAttrs<'input> for MultContext<'input> {}

impl<'input> MultContextExt<'input>{
	fn new(ctx: &dyn EContextAttrs<'input>) -> Rc<EContextAll<'input>>  {
		Rc::new(
			EContextAll::MultContext(
				BaseParserRuleContext::copy_from(ctx,MultContextExt{
					op:None, 
        			a:None, b:None, 
        			base: ctx.borrow().clone(),
        			ph:PhantomData
				})
			)
		)
	}
}

pub type DecContext<'input> = BaseParserRuleContext<'input,DecContextExt<'input>>;

pub trait DecContextAttrs<'input>: LabelsParserContext<'input>{
	fn e(&self) -> Option<Rc<EContextAll<'input>>> where Self:Sized{
		self.child_of_type(0)
	}
}

impl<'input> DecContextAttrs<'input> for DecContext<'input>{}

pub struct DecContextExt<'input>{
	base:EContextExt<'input>,
	pub x: Option<Rc<EContextAll<'input>>>,
	ph:PhantomData<&'input str>
}

antlr4rust::tid!{DecContextExt<'a>}

impl<'input> LabelsParserContext<'input> for DecContext<'input>{}

impl<'input,'a> Listenable<dyn LabelsListener<'input> + 'a> for DecContext<'input>{
	fn enter(&self,listener: &mut (dyn LabelsListener<'input> + 'a)) -> Result<(), ANTLRError> {
		listener.enter_every_rule(self)?;
		listener.enter_dec(self);
		Ok(())
	}
}

impl<'input> CustomRuleContext<'input> for DecContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = LabelsParserContextType;
	fn get_rule_index(&self) -> usize { RULE_e }
	//fn type_rule_index() -> usize where Self: Sized { RULE_e }
}

impl<'input> Borrow<EContextExt<'input>> for DecContext<'input>{
	fn borrow(&self) -> &EContextExt<'input> { &self.base }
}
impl<'input> BorrowMut<EContextExt<'input>> for DecContext<'input>{
	fn borrow_mut(&mut self) -> &mut EContextExt<'input> { &mut self.base }
}

impl<'input> EContextAttrs<'input> for DecContext<'input> {}

impl<'input> DecContextExt<'input>{
	fn new(ctx: &dyn EContextAttrs<'input>) -> Rc<EContextAll<'input>>  {
		Rc::new(
			EContextAll::DecContext(
				BaseParserRuleContext::copy_from(ctx,DecContextExt{
        			x:None, 
        			base: ctx.borrow().clone(),
        			ph:PhantomData
				})
			)
		)
	}
}

pub type AnIDContext<'input> = BaseParserRuleContext<'input,AnIDContextExt<'input>>;

pub trait AnIDContextAttrs<'input>: LabelsParserContext<'input>{
	/// Retrieves first TerminalNode corresponding to token ID
	/// Returns `None` if there is no child corresponding to token ID
	fn ID(&self) -> Option<Rc<TerminalNode<'input,LabelsParserContextType>>> where Self:Sized{
		self.get_token(Labels_ID, 0)
	}
}

impl<'input> AnIDContextAttrs<'input> for AnIDContext<'input>{}

pub struct AnIDContextExt<'input>{
	base:EContextExt<'input>,
	pub ID: Option<TokenType<'input>>,
	ph:PhantomData<&'input str>
}

antlr4rust::tid!{AnIDContextExt<'a>}

impl<'input> LabelsParserContext<'input> for AnIDContext<'input>{}

impl<'input,'a> Listenable<dyn LabelsListener<'input> + 'a> for AnIDContext<'input>{
	fn enter(&self,listener: &mut (dyn LabelsListener<'input> + 'a)) -> Result<(), ANTLRError> {
		listener.enter_every_rule(self)?;
		listener.enter_anID(self);
		Ok(())
	}
}

impl<'input> CustomRuleContext<'input> for AnIDContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = LabelsParserContextType;
	fn get_rule_index(&self) -> usize { RULE_e }
	//fn type_rule_index() -> usize where Self: Sized { RULE_e }
}

impl<'input> Borrow<EContextExt<'input>> for AnIDContext<'input>{
	fn borrow(&self) -> &EContextExt<'input> { &self.base }
}
impl<'input> BorrowMut<EContextExt<'input>> for AnIDContext<'input>{
	fn borrow_mut(&mut self) -> &mut EContextExt<'input> { &mut self.base }
}

impl<'input> EContextAttrs<'input> for AnIDContext<'input> {}

impl<'input> AnIDContextExt<'input>{
	fn new(ctx: &dyn EContextAttrs<'input>) -> Rc<EContextAll<'input>>  {
		Rc::new(
			EContextAll::AnIDContext(
				BaseParserRuleContext::copy_from(ctx,AnIDContextExt{
					ID:None, 
        			base: ctx.borrow().clone(),
        			ph:PhantomData
				})
			)
		)
	}
}

pub type AnIntContext<'input> = BaseParserRuleContext<'input,AnIntContextExt<'input>>;

pub trait AnIntContextAttrs<'input>: LabelsParserContext<'input>{
	/// Retrieves first TerminalNode corresponding to token INT
	/// Returns `None` if there is no child corresponding to token INT
	fn INT(&self) -> Option<Rc<TerminalNode<'input,LabelsParserContextType>>> where Self:Sized{
		self.get_token(Labels_INT, 0)
	}
}

impl<'input> AnIntContextAttrs<'input> for AnIntContext<'input>{}

pub struct AnIntContextExt<'input>{
	base:EContextExt<'input>,
	pub INT: Option<TokenType<'input>>,
	ph:PhantomData<&'input str>
}

antlr4rust::tid!{AnIntContextExt<'a>}

impl<'input> LabelsParserContext<'input> for AnIntContext<'input>{}

impl<'input,'a> Listenable<dyn LabelsListener<'input> + 'a> for AnIntContext<'input>{
	fn enter(&self,listener: &mut (dyn LabelsListener<'input> + 'a)) -> Result<(), ANTLRError> {
		listener.enter_every_rule(self)?;
		listener.enter_anInt(self);
		Ok(())
	}
}

impl<'input> CustomRuleContext<'input> for AnIntContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = LabelsParserContextType;
	fn get_rule_index(&self) -> usize { RULE_e }
	//fn type_rule_index() -> usize where Self: Sized { RULE_e }
}

impl<'input> Borrow<EContextExt<'input>> for AnIntContext<'input>{
	fn borrow(&self) -> &EContextExt<'input> { &self.base }
}
impl<'input> BorrowMut<EContextExt<'input>> for AnIntContext<'input>{
	fn borrow_mut(&mut self) -> &mut EContextExt<'input> { &mut self.base }
}

impl<'input> EContextAttrs<'input> for AnIntContext<'input> {}

impl<'input> AnIntContextExt<'input>{
	fn new(ctx: &dyn EContextAttrs<'input>) -> Rc<EContextAll<'input>>  {
		Rc::new(
			EContextAll::AnIntContext(
				BaseParserRuleContext::copy_from(ctx,AnIntContextExt{
					INT:None, 
        			base: ctx.borrow().clone(),
        			ph:PhantomData
				})
			)
		)
	}
}

pub type IncContext<'input> = BaseParserRuleContext<'input,IncContextExt<'input>>;

pub trait IncContextAttrs<'input>: LabelsParserContext<'input>{
	fn e(&self) -> Option<Rc<EContextAll<'input>>> where Self:Sized{
		self.child_of_type(0)
	}
}

impl<'input> IncContextAttrs<'input> for IncContext<'input>{}

pub struct IncContextExt<'input>{
	base:EContextExt<'input>,
	pub x: Option<Rc<EContextAll<'input>>>,
	ph:PhantomData<&'input str>
}

antlr4rust::tid!{IncContextExt<'a>}

impl<'input> LabelsParserContext<'input> for IncContext<'input>{}

impl<'input,'a> Listenable<dyn LabelsListener<'input> + 'a> for IncContext<'input>{
	fn enter(&self,listener: &mut (dyn LabelsListener<'input> + 'a)) -> Result<(), ANTLRError> {
		listener.enter_every_rule(self)?;
		listener.enter_inc(self);
		Ok(())
	}
}

impl<'input> CustomRuleContext<'input> for IncContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = LabelsParserContextType;
	fn get_rule_index(&self) -> usize { RULE_e }
	//fn type_rule_index() -> usize where Self: Sized { RULE_e }
}

impl<'input> Borrow<EContextExt<'input>> for IncContext<'input>{
	fn borrow(&self) -> &EContextExt<'input> { &self.base }
}
impl<'input> BorrowMut<EContextExt<'input>> for IncContext<'input>{
	fn borrow_mut(&mut self) -> &mut EContextExt<'input> { &mut self.base }
}

impl<'input> EContextAttrs<'input> for IncContext<'input> {}

impl<'input> IncContextExt<'input>{
	fn new(ctx: &dyn EContextAttrs<'input>) -> Rc<EContextAll<'input>>  {
		Rc::new(
			EContextAll::IncContext(
				BaseParserRuleContext::copy_from(ctx,IncContextExt{
        			x:None, 
        			base: ctx.borrow().clone(),
        			ph:PhantomData
				})
			)
		)
	}
}

impl<'input, I> LabelsParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	pub fn  e(&mut self,)
	-> Result<Rc<EContextAll<'input>>,ANTLRError> {
		self.e_rec(0)
	}

	fn e_rec(&mut self, _p: i32)
	-> Result<Rc<EContextAll<'input>>,ANTLRError> {
		let recog = self;
		let _parentctx = recog.ctx.take();
		let _parentState = recog.base.get_state();
		let mut _localctx = EContextExt::new(_parentctx.clone(), recog.base.get_state());
		recog.base.enter_recursion_rule(_localctx.clone(), 2, RULE_e, _p);
	    let mut _localctx: Rc<EContextAll> = _localctx;
        let mut _prevctx = _localctx.clone();
		let _startState = 2;
		let result: Result<(), ANTLRError> = (|| {
			let mut _alt: i32;
			//recog.base.enter_outer_alt(_localctx.clone(), 1)?;
			recog.base.enter_outer_alt(None, 1)?;
			{
			recog.base.set_state(16);
			recog.err_handler.sync(&mut recog.base)?;
			match recog.base.input.la(1) {
			Labels_INT 
				=> {
					{
					let mut tmp = AnIntContextExt::new(&**_localctx);
					recog.ctx = Some(tmp.clone());
					_localctx = tmp;
					_prevctx = _localctx.clone();

					recog.base.set_state(7);
					let tmp = recog.base.match_token(Labels_INT,&mut recog.err_handler)?;
					if let EContextAll::AnIntContext(ctx) = cast_mut::<_,EContextAll >(&mut _localctx){
					ctx.INT = Some(tmp.clone()); } else {unreachable!("cant cast");}  

					let tmp = { if let Some(it) = &if let EContextAll::AnIntContext(ctx) = cast::<_,EContextAll >(&*_localctx){
					ctx } else {unreachable!("cant cast")} .INT { it.get_text() } else { "null" } .to_owned()}.to_owned();
					if let EContextAll::AnIntContext(ctx) = cast_mut::<_,EContextAll >(&mut _localctx){
					ctx.set_v(tmp); } else {unreachable!("cant cast");} 
					}
				}

			Labels_T__2 
				=> {
					{
					let mut tmp = ParensContextExt::new(&**_localctx);
					recog.ctx = Some(tmp.clone());
					_localctx = tmp;
					_prevctx = _localctx.clone();
					recog.base.set_state(9);
					recog.base.match_token(Labels_T__2,&mut recog.err_handler)?;

					/*InvokeRule e*/
					recog.base.set_state(10);
					let tmp = recog.e_rec(0)?;
					if let EContextAll::ParensContext(ctx) = cast_mut::<_,EContextAll >(&mut _localctx){
					ctx.x = Some(tmp.clone()); } else {unreachable!("cant cast");}  

					recog.base.set_state(11);
					recog.base.match_token(Labels_T__3,&mut recog.err_handler)?;

					let tmp = { if let EContextAll::ParensContext(ctx) = cast::<_,EContextAll >(&*_localctx){
					ctx } else {unreachable!("cant cast")} .x.as_ref().unwrap().get_v()}.to_owned();
					if let EContextAll::ParensContext(ctx) = cast_mut::<_,EContextAll >(&mut _localctx){
					ctx.set_v(tmp); } else {unreachable!("cant cast");} 
					}
				}

			Labels_ID 
				=> {
					{
					let mut tmp = AnIDContextExt::new(&**_localctx);
					recog.ctx = Some(tmp.clone());
					_localctx = tmp;
					_prevctx = _localctx.clone();
					recog.base.set_state(14);
					let tmp = recog.base.match_token(Labels_ID,&mut recog.err_handler)?;
					if let EContextAll::AnIDContext(ctx) = cast_mut::<_,EContextAll >(&mut _localctx){
					ctx.ID = Some(tmp.clone()); } else {unreachable!("cant cast");}  

					let tmp = { if let Some(it) = &if let EContextAll::AnIDContext(ctx) = cast::<_,EContextAll >(&*_localctx){
					ctx } else {unreachable!("cant cast")} .ID { it.get_text() } else { "null" } .to_owned()}.to_owned();
					if let EContextAll::AnIDContext(ctx) = cast_mut::<_,EContextAll >(&mut _localctx){
					ctx.set_v(tmp); } else {unreachable!("cant cast");} 
					}
				}

				_ => Err(ANTLRError::NoAltError(NoViableAltError::new(&mut recog.base)))?
			}
			let tmp = recog.input.lt(-1).cloned();
			recog.ctx.as_ref().unwrap().set_stop(tmp);
			recog.base.set_state(36);
			recog.err_handler.sync(&mut recog.base)?;
			_alt = recog.interpreter.adaptive_predict(2,&mut recog.base)?;
			while { _alt!=2 && _alt!=INVALID_ALT } {
				if _alt==1 {
					recog.trigger_exit_rule_event()?;
					_prevctx = _localctx.clone();
					{
					recog.base.set_state(34);
					recog.err_handler.sync(&mut recog.base)?;
					match  recog.interpreter.adaptive_predict(1,&mut recog.base)? {
						1 =>{
							{
							/*recRuleLabeledAltStartAction*/
							let mut tmp = MultContextExt::new(&**EContextExt::new(_parentctx.clone(), _parentState));
							if let EContextAll::MultContext(ctx) = cast_mut::<_,EContextAll >(&mut tmp){
								ctx.a = Some(_prevctx.clone());
							} else {unreachable!("cant cast");}
							recog.push_new_recursion_context(tmp.clone(), _startState, RULE_e)?;
							_localctx = tmp;
							recog.base.set_state(18);
							if !({let _localctx = Some(_localctx.clone());
							recog.precpred(None, 7)}) {
								Err(FailedPredicateError::new(&mut recog.base, Some("recog.precpred(None, 7)".to_owned()), None))?;
							}
							recog.base.set_state(19);
							let tmp = recog.base.match_token(Labels_T__0,&mut recog.err_handler)?;
							if let EContextAll::MultContext(ctx) = cast_mut::<_,EContextAll >(&mut _localctx){
							ctx.op = Some(tmp.clone()); } else {unreachable!("cant cast");}  

							/*InvokeRule e*/
							recog.base.set_state(20);
							let tmp = recog.e_rec(8)?;
							if let EContextAll::MultContext(ctx) = cast_mut::<_,EContextAll >(&mut _localctx){
							ctx.b = Some(tmp.clone()); } else {unreachable!("cant cast");}  

							let tmp = { "* ".to_owned() + if let EContextAll::MultContext(ctx) = cast::<_,EContextAll >(&*_localctx){
							ctx } else {unreachable!("cant cast")} .a.as_ref().unwrap().get_v() + " " + if let EContextAll::MultContext(ctx) = cast::<_,EContextAll >(&*_localctx){
							ctx } else {unreachable!("cant cast")} .b.as_ref().unwrap().get_v()}.to_owned();
							if let EContextAll::MultContext(ctx) = cast_mut::<_,EContextAll >(&mut _localctx){
							ctx.set_v(tmp); } else {unreachable!("cant cast");} 
							}
						}
					,
						2 =>{
							{
							/*recRuleLabeledAltStartAction*/
							let mut tmp = AddContextExt::new(&**EContextExt::new(_parentctx.clone(), _parentState));
							if let EContextAll::AddContext(ctx) = cast_mut::<_,EContextAll >(&mut tmp){
								ctx.a = Some(_prevctx.clone());
							} else {unreachable!("cant cast");}
							recog.push_new_recursion_context(tmp.clone(), _startState, RULE_e)?;
							_localctx = tmp;
							recog.base.set_state(23);
							if !({let _localctx = Some(_localctx.clone());
							recog.precpred(None, 6)}) {
								Err(FailedPredicateError::new(&mut recog.base, Some("recog.precpred(None, 6)".to_owned()), None))?;
							}
							recog.base.set_state(24);
							recog.base.match_token(Labels_T__1,&mut recog.err_handler)?;

							/*InvokeRule e*/
							recog.base.set_state(25);
							let tmp = recog.e_rec(7)?;
							if let EContextAll::AddContext(ctx) = cast_mut::<_,EContextAll >(&mut _localctx){
							ctx.b = Some(tmp.clone()); } else {unreachable!("cant cast");}  

							let tmp = { "+ ".to_owned() + if let EContextAll::AddContext(ctx) = cast::<_,EContextAll >(&*_localctx){
							ctx } else {unreachable!("cant cast")} .a.as_ref().unwrap().get_v() + " " + if let EContextAll::AddContext(ctx) = cast::<_,EContextAll >(&*_localctx){
							ctx } else {unreachable!("cant cast")} .b.as_ref().unwrap().get_v()}.to_owned();
							if let EContextAll::AddContext(ctx) = cast_mut::<_,EContextAll >(&mut _localctx){
							ctx.set_v(tmp); } else {unreachable!("cant cast");} 
							}
						}
					,
						3 =>{
							{
							/*recRuleLabeledAltStartAction*/
							let mut tmp = IncContextExt::new(&**EContextExt::new(_parentctx.clone(), _parentState));
							if let EContextAll::IncContext(ctx) = cast_mut::<_,EContextAll >(&mut tmp){
								ctx.x = Some(_prevctx.clone());
							} else {unreachable!("cant cast");}
							recog.push_new_recursion_context(tmp.clone(), _startState, RULE_e)?;
							_localctx = tmp;
							recog.base.set_state(28);
							if !({let _localctx = Some(_localctx.clone());
							recog.precpred(None, 3)}) {
								Err(FailedPredicateError::new(&mut recog.base, Some("recog.precpred(None, 3)".to_owned()), None))?;
							}
							recog.base.set_state(29);
							recog.base.match_token(Labels_T__4,&mut recog.err_handler)?;

							let tmp = { " ++".to_owned() + if let EContextAll::IncContext(ctx) = cast::<_,EContextAll >(&*_localctx){
							ctx } else {unreachable!("cant cast")} .x.as_ref().unwrap().get_v()}.to_owned();
							if let EContextAll::IncContext(ctx) = cast_mut::<_,EContextAll >(&mut _localctx){
							ctx.set_v(tmp); } else {unreachable!("cant cast");} 
							}
						}
					,
						4 =>{
							{
							/*recRuleLabeledAltStartAction*/
							let mut tmp = DecContextExt::new(&**EContextExt::new(_parentctx.clone(), _parentState));
							if let EContextAll::DecContext(ctx) = cast_mut::<_,EContextAll >(&mut tmp){
								ctx.x = Some(_prevctx.clone());
							} else {unreachable!("cant cast");}
							recog.push_new_recursion_context(tmp.clone(), _startState, RULE_e)?;
							_localctx = tmp;
							recog.base.set_state(31);
							if !({let _localctx = Some(_localctx.clone());
							recog.precpred(None, 2)}) {
								Err(FailedPredicateError::new(&mut recog.base, Some("recog.precpred(None, 2)".to_owned()), None))?;
							}
							recog.base.set_state(32);
							recog.base.match_token(Labels_T__5,&mut recog.err_handler)?;

							let tmp = { " --".to_owned() + if let EContextAll::DecContext(ctx) = cast::<_,EContextAll >(&*_localctx){
							ctx } else {unreachable!("cant cast")} .x.as_ref().unwrap().get_v()}.to_owned();
							if let EContextAll::DecContext(ctx) = cast_mut::<_,EContextAll >(&mut _localctx){
							ctx.set_v(tmp); } else {unreachable!("cant cast");} 
							}
						}

						_ => {}
					}
					} 
				}
				recog.base.set_state(38);
				recog.err_handler.sync(&mut recog.base)?;
				_alt = recog.interpreter.adaptive_predict(2,&mut recog.base)?;
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
		4, 1, 9, 40, 2, 0, 7, 0, 2, 1, 7, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 17, 8, 1, 1, 1, 1, 1, 1, 
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
		1, 1, 1, 5, 1, 35, 8, 1, 10, 1, 12, 1, 38, 9, 1, 1, 1, 0, 1, 2, 2, 0, 
		2, 0, 0, 43, 0, 4, 1, 0, 0, 0, 2, 16, 1, 0, 0, 0, 4, 5, 3, 2, 1, 0, 5, 
		1, 1, 0, 0, 0, 6, 7, 6, 1, -1, 0, 7, 8, 5, 8, 0, 0, 8, 17, 6, 1, -1, 0, 
		9, 10, 5, 3, 0, 0, 10, 11, 3, 2, 1, 0, 11, 12, 5, 4, 0, 0, 12, 13, 6, 
		1, -1, 0, 13, 17, 1, 0, 0, 0, 14, 15, 5, 7, 0, 0, 15, 17, 6, 1, -1, 0, 
		16, 6, 1, 0, 0, 0, 16, 9, 1, 0, 0, 0, 16, 14, 1, 0, 0, 0, 17, 36, 1, 0, 
		0, 0, 18, 19, 10, 7, 0, 0, 19, 20, 5, 1, 0, 0, 20, 21, 3, 2, 1, 8, 21, 
		22, 6, 1, -1, 0, 22, 35, 1, 0, 0, 0, 23, 24, 10, 6, 0, 0, 24, 25, 5, 2, 
		0, 0, 25, 26, 3, 2, 1, 7, 26, 27, 6, 1, -1, 0, 27, 35, 1, 0, 0, 0, 28, 
		29, 10, 3, 0, 0, 29, 30, 5, 5, 0, 0, 30, 35, 6, 1, -1, 0, 31, 32, 10, 
		2, 0, 0, 32, 33, 5, 6, 0, 0, 33, 35, 6, 1, -1, 0, 34, 18, 1, 0, 0, 0, 
		34, 23, 1, 0, 0, 0, 34, 28, 1, 0, 0, 0, 34, 31, 1, 0, 0, 0, 35, 38, 1, 
		0, 0, 0, 36, 34, 1, 0, 0, 0, 36, 37, 1, 0, 0, 0, 37, 3, 1, 0, 0, 0, 38, 
		36, 1, 0, 0, 0, 3, 16, 34, 36
	];
}
