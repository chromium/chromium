// Generated from VisitorBasic.g4 by ANTLR 4.13.2
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
use super::visitorbasiclistener::*;
use super::visitorbasicvisitor::*;

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

		pub const VisitorBasic_A:i32=1;
	pub const VisitorBasic_EOF:i32=EOF;
	pub const RULE_s:usize = 0;
	pub const ruleNames: [&'static str; 1] =  [
		"s"
	];


	pub const _LITERAL_NAMES: [Option<&'static str>;2] = [
		None, Some("'A'")
	];
	pub const _SYMBOLIC_NAMES: [Option<&'static str>;2]  = [
		None, Some("A")
	];
	lazy_static!{
	    static ref _shared_context_cache: Arc<PredictionContextCache> = Arc::new(PredictionContextCache::new());
		static ref VOCABULARY: Box<dyn Vocabulary> = Box::new(VocabularyImpl::new(_LITERAL_NAMES.iter(), _SYMBOLIC_NAMES.iter(), None));
	}


type BaseParserType<'input, I> =
	BaseParser<'input,VisitorBasicParserExt<'input>, I, VisitorBasicParserContextType , dyn VisitorBasicListener<'input> + 'input >;

type TokenType<'input> = <LocalTokenFactory<'input> as TokenFactory<'input>>::Tok;
pub type LocalTokenFactory<'input> = CommonTokenFactory;

pub type VisitorBasicTreeWalker<'input,'a> =
	ParseTreeWalker<'input, 'a, VisitorBasicParserContextType , dyn VisitorBasicListener<'input> + 'a>;

/// Parser for VisitorBasic grammar
pub struct VisitorBasicParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
	base:BaseParserType<'input,I>,
	interpreter:Arc<ParserATNSimulator>,
	_shared_context_cache: Box<PredictionContextCache>,
    pub err_handler: Box<dyn ErrorStrategy<'input,BaseParserType<'input,I> > >,
}

impl<'input, I> VisitorBasicParser<'input, I>
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
				VisitorBasicParserExt{
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

impl<'input, I> VisitorBasicParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    pub fn with_dyn_strategy(input: I) -> Self{
    	Self::with_strategy(input,Box::new(DefaultErrorStrategy::new()))
    }
}

impl<'input, I> VisitorBasicParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    pub fn new(input: I) -> Self{
    	Self::with_strategy(input,Box::new(DefaultErrorStrategy::new()))
    }
}

/// Trait for monomorphized trait object that corresponds to the nodes of parse tree generated for VisitorBasicParser
pub trait VisitorBasicParserContext<'input>:
	for<'x> Listenable<dyn VisitorBasicListener<'input> + 'x > + 
	for<'x> Visitable<dyn VisitorBasicVisitor<'input> + 'x > + 
	ParserRuleContext<'input, TF=LocalTokenFactory<'input>, Ctx=VisitorBasicParserContextType>
{}

antlr4rust::coerce_from!{ 'input : VisitorBasicParserContext<'input> }

impl<'input, 'x, T> VisitableDyn<T> for dyn VisitorBasicParserContext<'input> + 'input
where
    T: VisitorBasicVisitor<'input> + 'x,
{
    fn accept_dyn(&self, visitor: &mut T) {
        self.accept(visitor as &mut (dyn VisitorBasicVisitor<'input> + 'x))
    }
}

impl<'input> VisitorBasicParserContext<'input> for TerminalNode<'input,VisitorBasicParserContextType> {}
impl<'input> VisitorBasicParserContext<'input> for ErrorNode<'input,VisitorBasicParserContextType> {}

antlr4rust::tid! { impl<'input> TidAble<'input> for dyn VisitorBasicParserContext<'input> + 'input }

antlr4rust::tid! { impl<'input> TidAble<'input> for dyn VisitorBasicListener<'input> + 'input }

pub struct VisitorBasicParserContextType;
antlr4rust::tid!{VisitorBasicParserContextType}

impl<'input> ParserNodeType<'input> for VisitorBasicParserContextType{
	type TF = LocalTokenFactory<'input>;
	type Type = dyn VisitorBasicParserContext<'input> + 'input;
}

impl<'input, I> Deref for VisitorBasicParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    type Target = BaseParserType<'input,I>;

    fn deref(&self) -> &Self::Target {
        &self.base
    }
}

impl<'input, I> DerefMut for VisitorBasicParser<'input, I>
where
    I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.base
    }
}

pub struct VisitorBasicParserExt<'input>{
	_pd: PhantomData<&'input str>,
}

impl<'input> VisitorBasicParserExt<'input>{
}
antlr4rust::tid! { VisitorBasicParserExt<'a> }

impl<'input> TokenAware<'input> for VisitorBasicParserExt<'input>{
	type TF = LocalTokenFactory<'input>;
}

impl<'input,I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>> ParserRecog<'input, BaseParserType<'input,I>> for VisitorBasicParserExt<'input>{}

impl<'input,I: TokenStream<'input, TF = LocalTokenFactory<'input> > + TidAble<'input>> Actions<'input, BaseParserType<'input,I>> for VisitorBasicParserExt<'input>{
	fn get_grammar_file_name(&self) -> & str{ "VisitorBasic.g4"}

   	fn get_rule_names(&self) -> &[& str] {&ruleNames}

   	fn get_vocabulary(&self) -> &dyn Vocabulary { &**VOCABULARY }
}
//------------------- s ----------------
pub type SContextAll<'input> = SContext<'input>;


pub type SContext<'input> = BaseParserRuleContext<'input,SContextExt<'input>>;

#[derive(Clone)]
pub struct SContextExt<'input>{
ph:PhantomData<&'input str>
}

impl<'input> VisitorBasicParserContext<'input> for SContext<'input>{}

impl<'input,'a> Listenable<dyn VisitorBasicListener<'input> + 'a> for SContext<'input>{
		fn enter(&self,listener: &mut (dyn VisitorBasicListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.enter_every_rule(self)?;
			listener.enter_s(self);
			Ok(())
		}
		fn exit(&self,listener: &mut (dyn VisitorBasicListener<'input> + 'a)) -> Result<(), ANTLRError> {
			listener.exit_s(self);
			listener.exit_every_rule(self)?;
			Ok(())
		}
}

impl<'input,'a> Visitable<dyn VisitorBasicVisitor<'input> + 'a> for SContext<'input>{
	fn accept(&self,visitor: &mut (dyn VisitorBasicVisitor<'input> + 'a)) {
		visitor.visit_s(self);
	}
}

impl<'input> CustomRuleContext<'input> for SContextExt<'input>{
	type TF = LocalTokenFactory<'input>;
	type Ctx = VisitorBasicParserContextType;
	fn get_rule_index(&self) -> usize { RULE_s }
	//fn type_rule_index() -> usize where Self: Sized { RULE_s }
}
antlr4rust::tid!{SContextExt<'a>}

impl<'input> SContextExt<'input>{
	fn new(parent: Option<Rc<dyn VisitorBasicParserContext<'input> + 'input > >, invoking_state: i32) -> Rc<SContextAll<'input>> {
		Rc::new(
			BaseParserRuleContext::new_parser_ctx(parent, invoking_state,SContextExt{

				ph:PhantomData
			}),
		)
	}
}

pub trait SContextAttrs<'input>: VisitorBasicParserContext<'input> + BorrowMut<SContextExt<'input>>{

/// Retrieves first TerminalNode corresponding to token A
/// Returns `None` if there is no child corresponding to token A
fn A(&self) -> Option<Rc<TerminalNode<'input,VisitorBasicParserContextType>>> where Self:Sized{
	self.get_token(VisitorBasic_A, 0)
}
/// Retrieves first TerminalNode corresponding to token EOF
/// Returns `None` if there is no child corresponding to token EOF
fn EOF(&self) -> Option<Rc<TerminalNode<'input,VisitorBasicParserContextType>>> where Self:Sized{
	self.get_token(VisitorBasic_EOF, 0)
}

}

impl<'input> SContextAttrs<'input> for SContext<'input>{}

impl<'input, I> VisitorBasicParser<'input, I>
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
			recog.base.set_state(2);
			recog.base.match_token(VisitorBasic_A,&mut recog.err_handler)?;

			recog.base.set_state(3);
			recog.base.match_token(VisitorBasic_EOF,&mut recog.err_handler)?;

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
		4, 1, 1, 6, 2, 0, 7, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 4, 0, 
		2, 1, 0, 0, 0, 2, 3, 5, 1, 0, 0, 3, 4, 5, 0, 0, 1, 4, 1, 1, 0, 0, 0, 0
	];
}
