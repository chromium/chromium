// Generated from CSV.g4 by ANTLR 4.13.2
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
	pub const WS:i32=4; 
	pub const TEXT:i32=5; 
	pub const STRING:i32=6;
	pub const channelNames: [&'static str;0+2] = [
		"DEFAULT_TOKEN_CHANNEL", "HIDDEN"
	];

	pub const modeNames: [&'static str;1] = [
		"DEFAULT_MODE"
	];

	pub const ruleNames: [&'static str;6] = [
		"T__0", "T__1", "T__2", "WS", "TEXT", "STRING"
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


pub type LexerContext<'input> = BaseRuleContext<'input,EmptyCustomRuleContext<'input,LocalTokenFactory<'input> >>;

pub type LocalTokenFactory<'input> = antlr4rust::token_factory::ArenaCommonFactory<'input>;

type From<'a> = <LocalTokenFactory<'a> as TokenFactory<'a> >::From;

pub struct CSVLexer<'input, Input:CharStream<From<'input> >> {
	base: BaseLexer<'input,CSVLexerActions,Input,LocalTokenFactory<'input>>,
}

antlr4rust::tid! { impl<'input,Input> TidAble<'input> for CSVLexer<'input,Input> where Input:CharStream<From<'input> > }

impl<'input, Input:CharStream<From<'input> >> Deref for CSVLexer<'input,Input>{
	type Target = BaseLexer<'input,CSVLexerActions,Input,LocalTokenFactory<'input>>;

	fn deref(&self) -> &Self::Target {
		&self.base
	}
}

impl<'input, Input:CharStream<From<'input> >> DerefMut for CSVLexer<'input,Input>{
	fn deref_mut(&mut self) -> &mut Self::Target {
		&mut self.base
	}
}


impl<'input, Input:CharStream<From<'input> >> CSVLexer<'input,Input>{
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
        "CSVLexer.g4"
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
				CSVLexerActions{},
				tf
			)
	    }
	}
}

impl<'input, Input:CharStream<From<'input> >> CSVLexer<'input,Input> where &'input LocalTokenFactory<'input>:Default{
	pub fn new(input: Input) -> Self{
		CSVLexer::new_with_token_factory(input, <&LocalTokenFactory<'input> as Default>::default())
	}
}

pub struct CSVLexerActions {
}

impl CSVLexerActions{
}

impl<'input, Input:CharStream<From<'input> >> Actions<'input,BaseLexer<'input,CSVLexerActions,Input,LocalTokenFactory<'input>>> for CSVLexerActions{
	}

	impl<'input, Input:CharStream<From<'input> >> CSVLexer<'input,Input>{

}

impl<'input, Input:CharStream<From<'input> >> LexerRecog<'input,BaseLexer<'input,CSVLexerActions,Input,LocalTokenFactory<'input>>> for CSVLexerActions{
}
impl<'input> TokenAware<'input> for CSVLexerActions{
	type TF = LocalTokenFactory<'input>;
}

impl<'input, Input:CharStream<From<'input> >> TokenSource<'input> for CSVLexer<'input,Input>{
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
			4, 0, 6, 42, 6, -1, 2, 0, 7, 0, 2, 1, 7, 1, 2, 2, 7, 2, 2, 3, 7, 3, 2, 
			4, 7, 4, 2, 5, 7, 5, 1, 0, 1, 0, 1, 1, 1, 1, 1, 2, 1, 2, 1, 3, 4, 3, 
			21, 8, 3, 11, 3, 12, 3, 22, 1, 3, 1, 3, 1, 4, 4, 4, 28, 8, 4, 11, 4, 
			12, 4, 29, 1, 5, 1, 5, 1, 5, 1, 5, 5, 5, 36, 8, 5, 10, 5, 12, 5, 39, 
			9, 5, 1, 5, 1, 5, 0, 0, 6, 1, 1, 3, 2, 5, 3, 7, 4, 9, 5, 11, 6, 1, 0, 
			3, 1, 0, 32, 32, 5, 0, 10, 10, 13, 13, 32, 32, 34, 34, 44, 44, 1, 0, 
			34, 34, 45, 0, 1, 1, 0, 0, 0, 0, 3, 1, 0, 0, 0, 0, 5, 1, 0, 0, 0, 0, 
			7, 1, 0, 0, 0, 0, 9, 1, 0, 0, 0, 0, 11, 1, 0, 0, 0, 1, 13, 1, 0, 0, 0, 
			3, 15, 1, 0, 0, 0, 5, 17, 1, 0, 0, 0, 7, 20, 1, 0, 0, 0, 9, 27, 1, 0, 
			0, 0, 11, 31, 1, 0, 0, 0, 13, 14, 5, 44, 0, 0, 14, 2, 1, 0, 0, 0, 15, 
			16, 5, 13, 0, 0, 16, 4, 1, 0, 0, 0, 17, 18, 5, 10, 0, 0, 18, 6, 1, 0, 
			0, 0, 19, 21, 7, 0, 0, 0, 20, 19, 1, 0, 0, 0, 21, 22, 1, 0, 0, 0, 22, 
			20, 1, 0, 0, 0, 22, 23, 1, 0, 0, 0, 23, 24, 1, 0, 0, 0, 24, 25, 6, 3, 
			0, 0, 25, 8, 1, 0, 0, 0, 26, 28, 8, 1, 0, 0, 27, 26, 1, 0, 0, 0, 28, 
			29, 1, 0, 0, 0, 29, 27, 1, 0, 0, 0, 29, 30, 1, 0, 0, 0, 30, 10, 1, 0, 
			0, 0, 31, 37, 5, 34, 0, 0, 32, 33, 5, 34, 0, 0, 33, 36, 5, 34, 0, 0, 
			34, 36, 8, 2, 0, 0, 35, 32, 1, 0, 0, 0, 35, 34, 1, 0, 0, 0, 36, 39, 1, 
			0, 0, 0, 37, 35, 1, 0, 0, 0, 37, 38, 1, 0, 0, 0, 38, 40, 1, 0, 0, 0, 
			39, 37, 1, 0, 0, 0, 40, 41, 5, 34, 0, 0, 41, 12, 1, 0, 0, 0, 5, 0, 22, 
			29, 35, 37, 1, 0, 1, 0
		];
	}