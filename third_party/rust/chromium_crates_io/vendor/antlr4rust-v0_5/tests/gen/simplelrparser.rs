// Generated from SimpleLR.g4 by ANTLR 4.13.2
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
use super::simplelrlistener::*;
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

		pub const SimpleLR_ID:i32=1; 
		pub const SimpleLR_WS:i32=2;
	pub const SimpleLR_EOF:i32=EOF;
	pub const RULE_s:usize = 0; 
	pub const RULE_a:usize = 1;
	pub const ruleNames: [&'static str; 2] =  [
		"s", "a"
	];


	pub const _LITERAL_NAMES: [Option<&'static str>;0] = [
	];
	pub const _SYMBOLIC_NAMES: [Option<&'static str>;3]  = [
		None, Some("ID"), Some("WS")
	];
	lazy_static!{
	    static ref _shared_context_cache: Arc<PredictionContextCache> = Arc::new(PredictionContextCache::new());
		static ref VOCABULARY: Box<dyn Vocabulary> = Box::new(VocabularyImpl::new(_LITERAL_NAMES.iter(), _SYMBOLIC_NAMES.iter(), None));
	}


type BaseParserType<'input, I> =
	BaseParser<'input,SimpleLRParserExt<'input>, I, SimpleLRParserContextType , dyn SimpleLRListener<'input> + 'input >;

type TokenType<'input> = <LocalTokenFactory<'input> as TokenFactory<'input>>::Tok;
pub type LocalTokenFactory<'input> = CommonTokenFactory;

pub type SimpleLRTreeWalker<'input,'a> =
	ParseTreeWalker<'input, 'a, SimpleLRParserContextType , dyn SimpleLRListener<'input> + 'a>;

/// Parser for SimpleLR grammar
pub struct SimpleLRParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	base:BaseParserType<'input,I>,
	interpreter:Arc<ParserATNSimulator>,
	_shared_context_cache: Box<PredictionContextCache>,
    pub err_handler: Box<dyn ErrorStrategy<'input,BaseParserType<'input,I> > >,
}

impl<'input, I> SimpleLRParser<'input, I>
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
				SimpleLRParserExt{
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

impl<'input, I> SimpleLRParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    pub fn with_dyn_strategy(input: I) -> Self{
    	Self::with_strategy(input,Box::new(DefaultErrorStrategy::new()))
    }
}

impl<'input, I> SimpleLRParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    pub fn new(input: I) -> Self{
    	Self::with_strategy(input,Box::new(DefaultErrorStrategy::new()))
    }
}

/// Trait for monomorphized trait object that corresponds to the nodes of parse tree generated for SimpleLRParser
pub trait SimpleLRParserContext<'input>:
	for<'x> Listenable<dyn SimpleLRListener<'input> + 'x > + 
	ParserRuleContext<'input, TF=LocalTokenFactory<'input>, Ctx=SimpleLRParserContextType>
{}

antlr4rust::coerce_from!{ 'input : SimpleLRParserContext<'input> }

impl<'input> SimpleLRParserContext<'input> for TerminalNode<'input,SimpleLRParserContextType> {}
impl<'input> SimpleLRParserContext<'input> for ErrorNode<'input,SimpleLRParserContextType> {}

antlr4rust::tid! { impl<'input> TidAble<'input> for dyn SimpleLRParserContext<'input> + 'input }

antlr4rust::tid! { impl<'input> TidAble<'input> for dyn SimpleLRListener<'input> + 'input }

pub struct SimpleLRParserContextType;
antlr4rust::tid!{SimpleLRParserContextType}

impl<'input> ParserNodeType<'input> for SimpleLRParserContextType{
	type TF = LocalTokenFactory<'input>;
	type Type = dyn SimpleLRParserContext<'input> + 'input;
}

impl<'input, I> Deref for SimpleLRParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    type Target = BaseParserType<'input,I>;

    fn deref(&self) -> &Self::Target {
        &self.base
    }
}

impl<'input, I> DerefMut for SimpleLRParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.base
    }
}

pub struct SimpleLRParserExt<'input>{
	_pd: PhantomData<&'input str>,
}

impl<'input> SimpleLRParserExt<'input>{
}
antlr4rust::tid! { SimpleLRParserExt<'a> }

impl<'input> TokenAware<'input> for SimpleLRParserExt<'input>{
	type TF = LocalTokenFactory<'input>;
}

impl<'input,I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>> ParserRecog<'input, BaseParserType<'input,I>> for SimpleLRParserExt<'input>{}

impl<'input,I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>> Actions<'input, BaseParserType<'input,I>> for SimpleLRParserExt<'input>{
	fn get_grammar_file_name(&self) -> & str{ "SimpleLR.g4"}

   	fn get_rule_names(&self) -> &[& str] {&ruleNames}

   	fn get_vocabulary(&self) -> &dyn Vocabulary { &**VOCABULARY }
	fn sempred(_localctx: Option<&(dyn SimpleLRParserContext<'input> + 'input)>, rule_index: i32, pred_index: i32,
			   recog:&mut BaseParserType<'input,I>
	)->bool{
		match rule_index {
					1 => SimpleLRParser::<'input,I>::a_sempred(_localctx.and_then(|x|x.downcast_ref()), pred_index, recog),
			_ => true
		}
	}
}

impl<'input, I> SimpleLRParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	fn a_sempred(_localctx: Option<&AContext<'input>>, pred_index:i32,
						recog:&mut <Self as Deref>::Target
		) -> bool {
		match pred_index {
				0=>{
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
ph:PhantomData<&'input str>
}

impl<'input> SimpleLRParserContext<'input> for SContext<'input>{}

impl<'input,'a> Listenable<dyn SimpleLRListener<'input> + 'a> for SContext<'input>{
		fn enter(&self,listener: &mut (dyn SimpleLRListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.enter_every_rule(self)?;
			listener.enter_s(self);
			Ok(())
		}fn exit(&self,listener: &mut (dyn SimpleLRListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.exit_s(self);
			listener.exit_every_rule(self)?;
			Ok(())
		}
}

impl<'input> CustomRuleContext<'input> for SContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = SimpleLRParserContextType;
	fn get_rule_index(&self) -> usize { RULE_s }
	//fn type_rule_index() -> usize where Self: Sized { RULE_s }
}
antlr4rust::tid!{SContextExt<'a>}

impl<'input> SContextExt<'input>{
	fn new(parent: Option<Rc<dyn SimpleLRParserContext<'input> + 'input > >, invoking_state: i32) -> Rc<SContextAll<'input>> {
		Rc::new(
			BaseParserRuleContext::new_parser_ctx(parent, invoking_state,SContextExt{

				ph:PhantomData
			}),
		)
	}
}

pub trait SContextAttrs<'input>: SimpleLRParserContext<'input> + BorrowMut<SContextExt<'input>>{

fn a(&self) -> Option<Rc<AContextAll<'input>>> where Self:Sized{
	self.child_of_type(0)
}

}

impl<'input> SContextAttrs<'input> for SContext<'input>{}

impl<'input, I> SimpleLRParser<'input, I>
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
			/*InvokeRule a*/
			recog.base.set_state(4);
			recog.a_rec(0)?;

			}
			let tmp = recog.input.lt(-1).cloned();
			recog.ctx.as_ref().unwrap().set_stop(tmp);
			println!("test");
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
//------------------- a ----------------
pub type AContextAll<'input> = AContext<'input>;


pub type AContext<'input> = BaseParserRuleContext<'input,AContextExt<'input>>;

#[derive(Clone)]
pub struct AContextExt<'input>{
ph:PhantomData<&'input str>
}

impl<'input> SimpleLRParserContext<'input> for AContext<'input>{}

impl<'input,'a> Listenable<dyn SimpleLRListener<'input> + 'a> for AContext<'input>{
		fn enter(&self,listener: &mut (dyn SimpleLRListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.enter_every_rule(self)?;
			listener.enter_a(self);
			Ok(())
		}fn exit(&self,listener: &mut (dyn SimpleLRListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.exit_a(self);
			listener.exit_every_rule(self)?;
			Ok(())
		}
}

impl<'input> CustomRuleContext<'input> for AContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = SimpleLRParserContextType;
	fn get_rule_index(&self) -> usize { RULE_a }
	//fn type_rule_index() -> usize where Self: Sized { RULE_a }
}
antlr4rust::tid!{AContextExt<'a>}

impl<'input> AContextExt<'input>{
	fn new(parent: Option<Rc<dyn SimpleLRParserContext<'input> + 'input > >, invoking_state: i32) -> Rc<AContextAll<'input>> {
		Rc::new(
			BaseParserRuleContext::new_parser_ctx(parent, invoking_state,AContextExt{

				ph:PhantomData
			}),
		)
	}
}

pub trait AContextAttrs<'input>: SimpleLRParserContext<'input> + BorrowMut<AContextExt<'input>>{

/// Retrieves first TerminalNode corresponding to token ID
/// Returns `None` if there is no child corresponding to token ID
fn ID(&self) -> Option<Rc<TerminalNode<'input,SimpleLRParserContextType>>> where Self:Sized{
	self.get_token(SimpleLR_ID, 0)
}
fn a(&self) -> Option<Rc<AContextAll<'input>>> where Self:Sized{
	self.child_of_type(0)
}

}

impl<'input> AContextAttrs<'input> for AContext<'input>{}

impl<'input, I> SimpleLRParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	pub fn  a(&mut self,)
	-> Result<Rc<AContextAll<'input>>,ANTLRError> {
		self.a_rec(0)
	}

	fn a_rec(&mut self, _p: i32)
	-> Result<Rc<AContextAll<'input>>,ANTLRError> {
		let recog = self;
		let _parentctx = recog.ctx.take();
		let _parentState = recog.base.get_state();
		let mut _localctx = AContextExt::new(_parentctx.clone(), recog.base.get_state());
		recog.base.enter_recursion_rule(_localctx.clone(), 2, RULE_a, _p);
	    let mut _localctx: Rc<AContextAll> = _localctx;
        let mut _prevctx = _localctx.clone();
		let _startState = 2;
		let result: Result<(), ANTLRError> = (|| {
			let mut _alt: i32;
			//recog.base.enter_outer_alt(_localctx.clone(), 1)?;
			recog.base.enter_outer_alt(None, 1)?;
			{
			{
			recog.base.set_state(7);
			recog.base.match_token(SimpleLR_ID,&mut recog.err_handler)?;

			}
			let tmp = recog.input.lt(-1).cloned();
			recog.ctx.as_ref().unwrap().set_stop(tmp);
			recog.base.set_state(13);
			recog.err_handler.sync(&mut recog.base)?;
			_alt = recog.interpreter.adaptive_predict(0,&mut recog.base)?;
			while { _alt!=2 && _alt!=INVALID_ALT } {
				if _alt==1 {
					recog.trigger_exit_rule_event()?;
					_prevctx = _localctx.clone();
					{
					{
					/*recRuleAltStartAction*/
					let mut tmp = AContextExt::new(_parentctx.clone(), _parentState);
					recog.push_new_recursion_context(tmp.clone(), _startState, RULE_a)?;
					_localctx = tmp;
					recog.base.set_state(9);
					if !({let _localctx = Some(_localctx.clone());
					recog.precpred(None, 2)}) {
						Err(FailedPredicateError::new(&mut recog.base, Some("recog.precpred(None, 2)".to_owned()), None))?;
					}
					recog.base.set_state(10);
					recog.base.match_token(SimpleLR_ID,&mut recog.err_handler)?;

					}
					} 
				}
				recog.base.set_state(15);
				recog.err_handler.sync(&mut recog.base)?;
				_alt = recog.interpreter.adaptive_predict(0,&mut recog.base)?;
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
		4, 1, 2, 17, 2, 0, 7, 0, 2, 1, 7, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 
		1, 1, 1, 5, 1, 12, 8, 1, 10, 1, 12, 1, 15, 9, 1, 1, 1, 0, 1, 2, 2, 0, 
		2, 0, 0, 15, 0, 4, 1, 0, 0, 0, 2, 6, 1, 0, 0, 0, 4, 5, 3, 2, 1, 0, 5, 
		1, 1, 0, 0, 0, 6, 7, 6, 1, -1, 0, 7, 8, 5, 1, 0, 0, 8, 13, 1, 0, 0, 0, 
		9, 10, 10, 2, 0, 0, 10, 12, 5, 1, 0, 0, 11, 9, 1, 0, 0, 0, 12, 15, 1, 
		0, 0, 0, 13, 11, 1, 0, 0, 0, 13, 14, 1, 0, 0, 0, 14, 3, 1, 0, 0, 0, 15, 
		13, 1, 0, 0, 0, 1, 13
	];
}
