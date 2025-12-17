// Generated from CSV.g4 by ANTLR 4.13.2
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
use super::csvlistener::*;
use super::csvvisitor::*;

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

		pub const CSV_T__0:i32=1; 
		pub const CSV_T__1:i32=2; 
		pub const CSV_T__2:i32=3; 
		pub const CSV_WS:i32=4; 
		pub const CSV_TEXT:i32=5; 
		pub const CSV_STRING:i32=6;
	pub const CSV_EOF:i32=EOF;
	pub const RULE_csvFile:usize = 0; 
	pub const RULE_hdr:usize = 1; 
	pub const RULE_row:usize = 2; 
	pub const RULE_field:usize = 3;
	pub const ruleNames: [&'static str; 4] =  [
		"csvFile", "hdr", "row", "field"
	];


	pub const _LITERAL_NAMES: [Option<&'static str>;4] = [
		None, Some("','"), Some("'\\r'"), Some("'\\n'")
	];
	pub const _SYMBOLIC_NAMES: [Option<&'static str>;7]  = [
		None, None, None, None, Some("WS"), Some("TEXT"), Some("STRING")
	];
	lazy_static!{
	    static ref _shared_context_cache: Arc<PredictionContextCache> = Arc::new(PredictionContextCache::new());
		static ref VOCABULARY: Box<dyn Vocabulary> = Box::new(VocabularyImpl::new(_LITERAL_NAMES.iter(), _SYMBOLIC_NAMES.iter(), None));
	}


type BaseParserType<'input, I> =
	BaseParser<'input,CSVParserExt<'input>, I, CSVParserContextType , dyn CSVListener<'input> + 'input >;

type TokenType<'input> = <LocalTokenFactory<'input> as TokenFactory<'input>>::Tok;

pub type LocalTokenFactory<'input> = antlr4rust::token_factory::ArenaCommonFactory<'input>;

pub type CSVTreeWalker<'input,'a> =
	ParseTreeWalker<'input, 'a, CSVParserContextType , dyn CSVListener<'input> + 'a>;

/// Parser for CSV grammar
pub struct CSVParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	base:BaseParserType<'input,I>,
	interpreter:Arc<ParserATNSimulator>,
	_shared_context_cache: Box<PredictionContextCache>,
    pub err_handler: Box<dyn ErrorStrategy<'input,BaseParserType<'input,I> > >,
}

impl<'input, I> CSVParser<'input, I>
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
				CSVParserExt{
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

impl<'input, I> CSVParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    pub fn with_dyn_strategy(input: I) -> Self{
    	Self::with_strategy(input,Box::new(DefaultErrorStrategy::new()))
    }
}

impl<'input, I> CSVParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    pub fn new(input: I) -> Self{
    	Self::with_strategy(input,Box::new(DefaultErrorStrategy::new()))
    }
}

/// Trait for monomorphized trait object that corresponds to the nodes of parse tree generated for CSVParser
pub trait CSVParserContext<'input>:
	for<'x> Listenable<dyn CSVListener<'input> + 'x > + 
	for<'x> Visitable<dyn CSVVisitor<'input> + 'x > + 
	ParserRuleContext<'input, TF=LocalTokenFactory<'input>, Ctx=CSVParserContextType>
{}

antlr4rust::coerce_from!{ 'input : CSVParserContext<'input> }

impl<'input, 'x, T> VisitableDyn<T> for dyn CSVParserContext<'input> + 'input
where
    T: CSVVisitor<'input> + 'x,
{
    fn accept_dyn(&self, visitor: &mut T) {
        self.accept(visitor as &mut (dyn CSVVisitor<'input> + 'x))
    }
}

impl<'input> CSVParserContext<'input> for TerminalNode<'input,CSVParserContextType> {}
impl<'input> CSVParserContext<'input> for ErrorNode<'input,CSVParserContextType> {}

antlr4rust::tid! { impl<'input> TidAble<'input> for dyn CSVParserContext<'input> + 'input }

antlr4rust::tid! { impl<'input> TidAble<'input> for dyn CSVListener<'input> + 'input }

pub struct CSVParserContextType;
antlr4rust::tid!{CSVParserContextType}

impl<'input> ParserNodeType<'input> for CSVParserContextType{
	type TF = LocalTokenFactory<'input>;
	type Type = dyn CSVParserContext<'input> + 'input;
}

impl<'input, I> Deref for CSVParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    type Target = BaseParserType<'input,I>;

    fn deref(&self) -> &Self::Target {
        &self.base
    }
}

impl<'input, I> DerefMut for CSVParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.base
    }
}

pub struct CSVParserExt<'input>{
	_pd: PhantomData<&'input str>,
}

impl<'input> CSVParserExt<'input>{
}
antlr4rust::tid! { CSVParserExt<'a> }

impl<'input> TokenAware<'input> for CSVParserExt<'input>{
	type TF = LocalTokenFactory<'input>;
}

impl<'input,I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>> ParserRecog<'input, BaseParserType<'input,I>> for CSVParserExt<'input>{}

impl<'input,I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>> Actions<'input, BaseParserType<'input,I>> for CSVParserExt<'input>{
	fn get_grammar_file_name(&self) -> & str{ "CSV.g4"}

   	fn get_rule_names(&self) -> &[& str] {&ruleNames}

   	fn get_vocabulary(&self) -> &dyn Vocabulary { &**VOCABULARY }
}
//------------------- csvFile ----------------
pub type CsvFileContextAll<'input> = CsvFileContext<'input>;


pub type CsvFileContext<'input> = BaseParserRuleContext<'input,CsvFileContextExt<'input>>;

#[derive(Clone)]
pub struct CsvFileContextExt<'input>{
ph:PhantomData<&'input str>
}

impl<'input> CSVParserContext<'input> for CsvFileContext<'input>{}

impl<'input,'a> Listenable<dyn CSVListener<'input> + 'a> for CsvFileContext<'input>{
		fn enter(&self,listener: &mut (dyn CSVListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.enter_every_rule(self)?;
			listener.enter_csvFile(self);
			Ok(())
		}
		fn exit(&self,listener: &mut (dyn CSVListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.exit_csvFile(self);
			listener.exit_every_rule(self)?;
			Ok(())
		}
}

impl<'input,'a> Visitable<dyn CSVVisitor<'input> + 'a> for CsvFileContext<'input>{
	fn accept(&self,visitor: &mut (dyn CSVVisitor<'input> + 'a)) {
		visitor.visit_csvFile(self);
	}
}

impl<'input> CustomRuleContext<'input> for CsvFileContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = CSVParserContextType;
	fn get_rule_index(&self) -> usize { RULE_csvFile }
	//fn type_rule_index() -> usize where Self: Sized { RULE_csvFile }
}
antlr4rust::tid!{CsvFileContextExt<'a>}

impl<'input> CsvFileContextExt<'input>{
	fn new(parent: Option<Rc<dyn CSVParserContext<'input> + 'input > >, invoking_state: i32) -> Rc<CsvFileContextAll<'input>> {
		Rc::new(
			BaseParserRuleContext::new_parser_ctx(parent, invoking_state,CsvFileContextExt{

				ph:PhantomData
			}),
		)
	}
}

pub trait CsvFileContextAttrs<'input>: CSVParserContext<'input> + BorrowMut<CsvFileContextExt<'input>>{

fn hdr(&self) -> Option<Rc<HdrContextAll<'input>>> where Self:Sized{
	self.child_of_type(0)
}
fn row_all(&self) ->  Vec<Rc<RowContextAll<'input>>> where Self:Sized{
	self.children_of_type()
}
fn row(&self, i: usize) -> Option<Rc<RowContextAll<'input>>> where Self:Sized{
	self.child_of_type(i)
}

}

impl<'input> CsvFileContextAttrs<'input> for CsvFileContext<'input>{}

impl<'input, I> CSVParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	pub fn csvFile(&mut self,)
	-> Result<Rc<CsvFileContextAll<'input>>,ANTLRError> {
		let mut recog = self;
		let _parentctx = recog.ctx.take();
		let mut _localctx = CsvFileContextExt::new(_parentctx.clone(), recog.base.get_state());
        recog.base.enter_rule(_localctx.clone(), 0, RULE_csvFile);
        let mut _localctx: Rc<CsvFileContextAll> = _localctx;
		let mut _la: i32 = -1;
		let result: Result<(), ANTLRError> = (|| {

			//recog.base.enter_outer_alt(_localctx.clone(), 1)?;
			recog.base.enter_outer_alt(None, 1)?;
			{
			/*InvokeRule hdr*/
			recog.base.set_state(8);
			recog.hdr()?;

			recog.base.set_state(10); 
			recog.err_handler.sync(&mut recog.base)?;
			_la = recog.base.input.la(1);
			loop {
				{
				{
				/*InvokeRule row*/
				recog.base.set_state(9);
				recog.row()?;

				}
				}
				recog.base.set_state(12); 
				recog.err_handler.sync(&mut recog.base)?;
				_la = recog.base.input.la(1);
				if !((((_la) & !0x3f) == 0 && ((1usize << _la) & 110) != 0)) {break}
			}
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
//------------------- hdr ----------------
pub type HdrContextAll<'input> = HdrContext<'input>;


pub type HdrContext<'input> = BaseParserRuleContext<'input,HdrContextExt<'input>>;

#[derive(Clone)]
pub struct HdrContextExt<'input>{
ph:PhantomData<&'input str>
}

impl<'input> CSVParserContext<'input> for HdrContext<'input>{}

impl<'input,'a> Listenable<dyn CSVListener<'input> + 'a> for HdrContext<'input>{
		fn enter(&self,listener: &mut (dyn CSVListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.enter_every_rule(self)?;
			listener.enter_hdr(self);
			Ok(())
		}
		fn exit(&self,listener: &mut (dyn CSVListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.exit_hdr(self);
			listener.exit_every_rule(self)?;
			Ok(())
		}
}

impl<'input,'a> Visitable<dyn CSVVisitor<'input> + 'a> for HdrContext<'input>{
	fn accept(&self,visitor: &mut (dyn CSVVisitor<'input> + 'a)) {
		visitor.visit_hdr(self);
	}
}

impl<'input> CustomRuleContext<'input> for HdrContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = CSVParserContextType;
	fn get_rule_index(&self) -> usize { RULE_hdr }
	//fn type_rule_index() -> usize where Self: Sized { RULE_hdr }
}
antlr4rust::tid!{HdrContextExt<'a>}

impl<'input> HdrContextExt<'input>{
	fn new(parent: Option<Rc<dyn CSVParserContext<'input> + 'input > >, invoking_state: i32) -> Rc<HdrContextAll<'input>> {
		Rc::new(
			BaseParserRuleContext::new_parser_ctx(parent, invoking_state,HdrContextExt{

				ph:PhantomData
			}),
		)
	}
}

pub trait HdrContextAttrs<'input>: CSVParserContext<'input> + BorrowMut<HdrContextExt<'input>>{

fn row(&self) -> Option<Rc<RowContextAll<'input>>> where Self:Sized{
	self.child_of_type(0)
}

}

impl<'input> HdrContextAttrs<'input> for HdrContext<'input>{}

impl<'input, I> CSVParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	pub fn hdr(&mut self,)
	-> Result<Rc<HdrContextAll<'input>>,ANTLRError> {
		let mut recog = self;
		let _parentctx = recog.ctx.take();
		let mut _localctx = HdrContextExt::new(_parentctx.clone(), recog.base.get_state());
        recog.base.enter_rule(_localctx.clone(), 2, RULE_hdr);
        let mut _localctx: Rc<HdrContextAll> = _localctx;
		let result: Result<(), ANTLRError> = (|| {

			//recog.base.enter_outer_alt(_localctx.clone(), 1)?;
			recog.base.enter_outer_alt(None, 1)?;
			{
			/*InvokeRule row*/
			recog.base.set_state(14);
			recog.row()?;

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
//------------------- row ----------------
pub type RowContextAll<'input> = RowContext<'input>;


pub type RowContext<'input> = BaseParserRuleContext<'input,RowContextExt<'input>>;

#[derive(Clone)]
pub struct RowContextExt<'input>{
ph:PhantomData<&'input str>
}

impl<'input> CSVParserContext<'input> for RowContext<'input>{}

impl<'input,'a> Listenable<dyn CSVListener<'input> + 'a> for RowContext<'input>{
		fn enter(&self,listener: &mut (dyn CSVListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.enter_every_rule(self)?;
			listener.enter_row(self);
			Ok(())
		}
		fn exit(&self,listener: &mut (dyn CSVListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.exit_row(self);
			listener.exit_every_rule(self)?;
			Ok(())
		}
}

impl<'input,'a> Visitable<dyn CSVVisitor<'input> + 'a> for RowContext<'input>{
	fn accept(&self,visitor: &mut (dyn CSVVisitor<'input> + 'a)) {
		visitor.visit_row(self);
	}
}

impl<'input> CustomRuleContext<'input> for RowContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = CSVParserContextType;
	fn get_rule_index(&self) -> usize { RULE_row }
	//fn type_rule_index() -> usize where Self: Sized { RULE_row }
}
antlr4rust::tid!{RowContextExt<'a>}

impl<'input> RowContextExt<'input>{
	fn new(parent: Option<Rc<dyn CSVParserContext<'input> + 'input > >, invoking_state: i32) -> Rc<RowContextAll<'input>> {
		Rc::new(
			BaseParserRuleContext::new_parser_ctx(parent, invoking_state,RowContextExt{

				ph:PhantomData
			}),
		)
	}
}

pub trait RowContextAttrs<'input>: CSVParserContext<'input> + BorrowMut<RowContextExt<'input>>{

fn field_all(&self) ->  Vec<Rc<FieldContextAll<'input>>> where Self:Sized{
	self.children_of_type()
}
fn field(&self, i: usize) -> Option<Rc<FieldContextAll<'input>>> where Self:Sized{
	self.child_of_type(i)
}

}

impl<'input> RowContextAttrs<'input> for RowContext<'input>{}

impl<'input, I> CSVParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	pub fn row(&mut self,)
	-> Result<Rc<RowContextAll<'input>>,ANTLRError> {
		let mut recog = self;
		let _parentctx = recog.ctx.take();
		let mut _localctx = RowContextExt::new(_parentctx.clone(), recog.base.get_state());
        recog.base.enter_rule(_localctx.clone(), 4, RULE_row);
        let mut _localctx: Rc<RowContextAll> = _localctx;
		let mut _la: i32 = -1;
		let result: Result<(), ANTLRError> = (|| {

			//recog.base.enter_outer_alt(_localctx.clone(), 1)?;
			recog.base.enter_outer_alt(None, 1)?;
			{
			/*InvokeRule field*/
			recog.base.set_state(16);
			recog.field()?;

			recog.base.set_state(21);
			recog.err_handler.sync(&mut recog.base)?;
			_la = recog.base.input.la(1);
			while _la==CSV_T__0 {
				{
				{
				recog.base.set_state(17);
				recog.base.match_token(CSV_T__0,&mut recog.err_handler)?;

				/*InvokeRule field*/
				recog.base.set_state(18);
				recog.field()?;

				}
				}
				recog.base.set_state(23);
				recog.err_handler.sync(&mut recog.base)?;
				_la = recog.base.input.la(1);
			}
			recog.base.set_state(25);
			recog.err_handler.sync(&mut recog.base)?;
			_la = recog.base.input.la(1);
			if _la==CSV_T__1 {
				{
				recog.base.set_state(24);
				recog.base.match_token(CSV_T__1,&mut recog.err_handler)?;

				}
			}

			recog.base.set_state(27);
			recog.base.match_token(CSV_T__2,&mut recog.err_handler)?;

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
//------------------- field ----------------
pub type FieldContextAll<'input> = FieldContext<'input>;


pub type FieldContext<'input> = BaseParserRuleContext<'input,FieldContextExt<'input>>;

#[derive(Clone)]
pub struct FieldContextExt<'input>{
ph:PhantomData<&'input str>
}

impl<'input> CSVParserContext<'input> for FieldContext<'input>{}

impl<'input,'a> Listenable<dyn CSVListener<'input> + 'a> for FieldContext<'input>{
		fn enter(&self,listener: &mut (dyn CSVListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.enter_every_rule(self)?;
			listener.enter_field(self);
			Ok(())
		}
		fn exit(&self,listener: &mut (dyn CSVListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.exit_field(self);
			listener.exit_every_rule(self)?;
			Ok(())
		}
}

impl<'input,'a> Visitable<dyn CSVVisitor<'input> + 'a> for FieldContext<'input>{
	fn accept(&self,visitor: &mut (dyn CSVVisitor<'input> + 'a)) {
		visitor.visit_field(self);
	}
}

impl<'input> CustomRuleContext<'input> for FieldContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = CSVParserContextType;
	fn get_rule_index(&self) -> usize { RULE_field }
	//fn type_rule_index() -> usize where Self: Sized { RULE_field }
}
antlr4rust::tid!{FieldContextExt<'a>}

impl<'input> FieldContextExt<'input>{
	fn new(parent: Option<Rc<dyn CSVParserContext<'input> + 'input > >, invoking_state: i32) -> Rc<FieldContextAll<'input>> {
		Rc::new(
			BaseParserRuleContext::new_parser_ctx(parent, invoking_state,FieldContextExt{

				ph:PhantomData
			}),
		)
	}
}

pub trait FieldContextAttrs<'input>: CSVParserContext<'input> + BorrowMut<FieldContextExt<'input>>{

/// Retrieves first TerminalNode corresponding to token TEXT
/// Returns `None` if there is no child corresponding to token TEXT
fn TEXT(&self) -> Option<Rc<TerminalNode<'input,CSVParserContextType>>> where Self:Sized{
	self.get_token(CSV_TEXT, 0)
}
/// Retrieves first TerminalNode corresponding to token STRING
/// Returns `None` if there is no child corresponding to token STRING
fn STRING(&self) -> Option<Rc<TerminalNode<'input,CSVParserContextType>>> where Self:Sized{
	self.get_token(CSV_STRING, 0)
}

}

impl<'input> FieldContextAttrs<'input> for FieldContext<'input>{}

impl<'input, I> CSVParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	pub fn field(&mut self,)
	-> Result<Rc<FieldContextAll<'input>>,ANTLRError> {
		let mut recog = self;
		let _parentctx = recog.ctx.take();
		let mut _localctx = FieldContextExt::new(_parentctx.clone(), recog.base.get_state());
        recog.base.enter_rule(_localctx.clone(), 6, RULE_field);
        let mut _localctx: Rc<FieldContextAll> = _localctx;
		let result: Result<(), ANTLRError> = (|| {

			recog.base.set_state(32);
			recog.err_handler.sync(&mut recog.base)?;
			match recog.base.input.la(1) {
			CSV_TEXT 
				=> {
					//recog.base.enter_outer_alt(_localctx.clone(), 1)?;
					recog.base.enter_outer_alt(None, 1)?;
					{
					recog.base.set_state(29);
					recog.base.match_token(CSV_TEXT,&mut recog.err_handler)?;

					}
				}

			CSV_STRING 
				=> {
					//recog.base.enter_outer_alt(_localctx.clone(), 2)?;
					recog.base.enter_outer_alt(None, 2)?;
					{
					recog.base.set_state(30);
					recog.base.match_token(CSV_STRING,&mut recog.err_handler)?;

					}
				}

			CSV_T__0 |CSV_T__1 |CSV_T__2 
				=> {
					//recog.base.enter_outer_alt(_localctx.clone(), 3)?;
					recog.base.enter_outer_alt(None, 3)?;
					{
					}
				}

				_ => Err(ANTLRError::NoAltError(NoViableAltError::new(&mut recog.base)))?
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
		4, 1, 6, 35, 2, 0, 7, 0, 2, 1, 7, 1, 2, 2, 7, 2, 2, 3, 7, 3, 1, 0, 1, 
		0, 4, 0, 11, 8, 0, 11, 0, 12, 0, 12, 1, 1, 1, 1, 1, 2, 1, 2, 1, 2, 5, 
		2, 20, 8, 2, 10, 2, 12, 2, 23, 9, 2, 1, 2, 3, 2, 26, 8, 2, 1, 2, 1, 2, 
		1, 3, 1, 3, 1, 3, 3, 3, 33, 8, 3, 1, 3, 0, 0, 4, 0, 2, 4, 6, 0, 0, 35, 
		0, 8, 1, 0, 0, 0, 2, 14, 1, 0, 0, 0, 4, 16, 1, 0, 0, 0, 6, 32, 1, 0, 0, 
		0, 8, 10, 3, 2, 1, 0, 9, 11, 3, 4, 2, 0, 10, 9, 1, 0, 0, 0, 11, 12, 1, 
		0, 0, 0, 12, 10, 1, 0, 0, 0, 12, 13, 1, 0, 0, 0, 13, 1, 1, 0, 0, 0, 14, 
		15, 3, 4, 2, 0, 15, 3, 1, 0, 0, 0, 16, 21, 3, 6, 3, 0, 17, 18, 5, 1, 0, 
		0, 18, 20, 3, 6, 3, 0, 19, 17, 1, 0, 0, 0, 20, 23, 1, 0, 0, 0, 21, 19, 
		1, 0, 0, 0, 21, 22, 1, 0, 0, 0, 22, 25, 1, 0, 0, 0, 23, 21, 1, 0, 0, 0, 
		24, 26, 5, 2, 0, 0, 25, 24, 1, 0, 0, 0, 25, 26, 1, 0, 0, 0, 26, 27, 1, 
		0, 0, 0, 27, 28, 5, 3, 0, 0, 28, 5, 1, 0, 0, 0, 29, 33, 5, 5, 0, 0, 30, 
		33, 5, 6, 0, 0, 31, 33, 1, 0, 0, 0, 32, 29, 1, 0, 0, 0, 32, 30, 1, 0, 
		0, 0, 32, 31, 1, 0, 0, 0, 33, 7, 1, 0, 0, 0, 4, 12, 21, 25, 32
	];
}
