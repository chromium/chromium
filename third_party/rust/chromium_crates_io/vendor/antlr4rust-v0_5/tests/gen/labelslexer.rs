// Generated from Labels.g4 by ANTLR 4.13.2
#![allow(dead_code)]
#![allow(nonstandard_style)]
#![allow(unused_imports)]
#![allow(unused_variables)]
use antlr4rust::atn::ATN;
use antlr4rust::char_stream::CharStream;
use antlr4rust::int_stream::IntStream;
use antlr4rust::tree::ParseTree;
use antlr4rust::lexer::{BaseLexer, Lexer, LexerRecog};
use antlr4rust::atn_deserializer::ATNDeserializer;
use antlr4rust::dfa::DFA;
use antlr4rust::lexer_atn_simulator::{LexerATNSimulator, ILexerATNSimulator};
use antlr4rust::PredictionContextCache;
use antlr4rust::recognizer::{Recognizer,Actions};
use antlr4rust::error_listener::ErrorListener;
use antlr4rust::TokenSource;
use antlr4rust::token_factory::{TokenFactory,CommonTokenFactory,TokenAware};
use antlr4rust::token::*;
use antlr4rust::rule_context::{BaseRuleContext,EmptyCustomRuleContext,EmptyContext};
use antlr4rust::parser_rule_context::{ParserRuleContext,BaseParserRuleContext,cast};
use antlr4rust::vocabulary::{Vocabulary,VocabularyImpl};

use antlr4rust::{lazy_static,Tid,TidAble,TidExt};

use std::sync::Arc;
use std::cell::RefCell;
use std::rc::Rc;
use std::marker::PhantomData;
use std::ops::{Deref, DerefMut};


	pub const T__0:i32=1; 
	pub const T__1:i32=2; 
	pub const T__2:i32=3; 
	pub const T__3:i32=4; 
	pub const T__4:i32=5; 
	pub const T__5:i32=6; 
	pub const ID:i32=7; 
	pub const INT:i32=8; 
	pub const WS:i32=9;
	pub const channelNames: [&'static str;0+2] = [
		"DEFAULT_TOKEN_CHANNEL", "HIDDEN"
	];

	pub const modeNames: [&'static str;1] = [
		"DEFAULT_MODE"
	];

	pub const ruleNames: [&'static str;9] = [
		"T__0", "T__1", "T__2", "T__3", "T__4", "T__5", "ID", "INT", "WS"
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


pub type LexerContext<'input> = BaseRuleContext<'input,EmptyCustomRuleContext<'input,LocalTokenFactory<'input> >>;
pub type LocalTokenFactory<'input> = CommonTokenFactory;

type From<'a> = <LocalTokenFactory<'a> as TokenFactory<'a> >::From;

pub struct LabelsLexer<'input, Input:CharStream<From<'input> >> {
	base: BaseLexer<'input,LabelsLexerActions,Input,LocalTokenFactory<'input>>,
}

antlr4rust::tid! { impl<'input,Input> TidAble<'input> for LabelsLexer<'input,Input> where Input:CharStream<From<'input> > }

impl<'input, Input:CharStream<From<'input> >> Deref for LabelsLexer<'input,Input>{
	type Target = BaseLexer<'input,LabelsLexerActions,Input,LocalTokenFactory<'input>>;

	fn deref(&self) -> &Self::Target {
		&self.base
	}
}

impl<'input, Input:CharStream<From<'input> >> DerefMut for LabelsLexer<'input,Input>{
	fn deref_mut(&mut self) -> &mut Self::Target {
		&mut self.base
	}
}


impl<'input, Input:CharStream<From<'input> >> LabelsLexer<'input,Input>{
    fn get_rule_names(&self) -> &'static [&'static str] {
        &ruleNames
    }
    fn get_literal_names(&self) -> &[Option<&str>] {
        &_LITERAL_NAMES
    }

    fn get_symbolic_names(&self) -> &[Option<&str>] {
        &_SYMBOLIC_NAMES
    }

    fn get_grammar_file_name(&self) -> &'static str {
        "LabelsLexer.g4"
    }

	pub fn new_with_token_factory(input: Input, tf: &'input LocalTokenFactory<'input>) -> Self {
		antlr4rust::recognizer::check_version("0","5");
    	Self {
			base: BaseLexer::new_base_lexer(
				input,
				LexerATNSimulator::new_lexer_atnsimulator(
					_ATN.clone(),
					_decision_to_DFA.clone(),
					_shared_context_cache.clone(),
				),
				LabelsLexerActions{},
				tf
			)
	    }
	}
}

impl<'input, Input:CharStream<From<'input> >> LabelsLexer<'input,Input> where &'input LocalTokenFactory<'input>:Default{
	pub fn new(input: Input) -> Self{
		LabelsLexer::new_with_token_factory(input, <&LocalTokenFactory<'input> as Default>::default())
	}
}

pub struct LabelsLexerActions {
}

impl LabelsLexerActions{
}

impl<'input, Input:CharStream<From<'input> >> Actions<'input,BaseLexer<'input,LabelsLexerActions,Input,LocalTokenFactory<'input>>> for LabelsLexerActions{
	}

	impl<'input, Input:CharStream<From<'input> >> LabelsLexer<'input,Input>{

}

impl<'input, Input:CharStream<From<'input> >> LexerRecog<'input,BaseLexer<'input,LabelsLexerActions,Input,LocalTokenFactory<'input>>> for LabelsLexerActions{
}
impl<'input> TokenAware<'input> for LabelsLexerActions{
	type TF = LocalTokenFactory<'input>;
}

impl<'input, Input:CharStream<From<'input> >> TokenSource<'input> for LabelsLexer<'input,Input>{
	type TF = LocalTokenFactory<'input>;

    fn next_token(&mut self) -> <Self::TF as TokenFactory<'input>>::Tok {
        self.base.next_token()
    }

    fn get_line(&self) -> isize {
        self.base.get_line()
    }

    fn get_char_position_in_line(&self) -> isize {
        self.base.get_char_position_in_line()
    }

    fn get_input_stream(&mut self) -> Option<&mut dyn IntStream> {
        self.base.get_input_stream()
    }

	fn get_source_name(&self) -> String {
		self.base.get_source_name()
	}

    fn get_token_factory(&self) -> &'input Self::TF {
        self.base.get_token_factory()
    }

    fn get_dfa_string(&self) -> String {
        self.base.get_dfa_string()
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
			4, 0, 9, 47, 6, -1, 2, 0, 7, 0, 2, 1, 7, 1, 2, 2, 7, 2, 2, 3, 7, 3, 2, 
			4, 7, 4, 2, 5, 7, 5, 2, 6, 7, 6, 2, 7, 7, 7, 2, 8, 7, 8, 1, 0, 1, 0, 
			1, 1, 1, 1, 1, 2, 1, 2, 1, 3, 1, 3, 1, 4, 1, 4, 1, 4, 1, 5, 1, 5, 1, 
			5, 1, 6, 4, 6, 35, 8, 6, 11, 6, 12, 6, 36, 1, 7, 4, 7, 40, 8, 7, 11, 
			7, 12, 7, 41, 1, 8, 1, 8, 1, 8, 1, 8, 0, 0, 9, 1, 1, 3, 2, 5, 3, 7, 4, 
			9, 5, 11, 6, 13, 7, 15, 8, 17, 9, 1, 0, 1, 2, 0, 10, 10, 32, 32, 48, 
			0, 1, 1, 0, 0, 0, 0, 3, 1, 0, 0, 0, 0, 5, 1, 0, 0, 0, 0, 7, 1, 0, 0, 
			0, 0, 9, 1, 0, 0, 0, 0, 11, 1, 0, 0, 0, 0, 13, 1, 0, 0, 0, 0, 15, 1, 
			0, 0, 0, 0, 17, 1, 0, 0, 0, 1, 19, 1, 0, 0, 0, 3, 21, 1, 0, 0, 0, 5, 
			23, 1, 0, 0, 0, 7, 25, 1, 0, 0, 0, 9, 27, 1, 0, 0, 0, 11, 30, 1, 0, 0, 
			0, 13, 34, 1, 0, 0, 0, 15, 39, 1, 0, 0, 0, 17, 43, 1, 0, 0, 0, 19, 20, 
			5, 42, 0, 0, 20, 2, 1, 0, 0, 0, 21, 22, 5, 43, 0, 0, 22, 4, 1, 0, 0, 
			0, 23, 24, 5, 40, 0, 0, 24, 6, 1, 0, 0, 0, 25, 26, 5, 41, 0, 0, 26, 8, 
			1, 0, 0, 0, 27, 28, 5, 43, 0, 0, 28, 29, 5, 43, 0, 0, 29, 10, 1, 0, 0, 
			0, 30, 31, 5, 45, 0, 0, 31, 32, 5, 45, 0, 0, 32, 12, 1, 0, 0, 0, 33, 
			35, 2, 97, 122, 0, 34, 33, 1, 0, 0, 0, 35, 36, 1, 0, 0, 0, 36, 34, 1, 
			0, 0, 0, 36, 37, 1, 0, 0, 0, 37, 14, 1, 0, 0, 0, 38, 40, 2, 48, 57, 0, 
			39, 38, 1, 0, 0, 0, 40, 41, 1, 0, 0, 0, 41, 39, 1, 0, 0, 0, 41, 42, 1, 
			0, 0, 0, 42, 16, 1, 0, 0, 0, 43, 44, 7, 0, 0, 0, 44, 45, 1, 0, 0, 0, 
			45, 46, 6, 8, 0, 0, 46, 18, 1, 0, 0, 0, 3, 0, 36, 41, 1, 6, 0, 0
		];
	}