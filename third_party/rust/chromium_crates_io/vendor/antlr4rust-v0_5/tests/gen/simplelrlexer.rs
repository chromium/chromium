// Generated from SimpleLR.g4 by ANTLR 4.13.2
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


	pub const ID:i32=1; 
	pub const WS:i32=2;
	pub const channelNames: [&'static str;0+2] = [
		"DEFAULT_TOKEN_CHANNEL", "HIDDEN"
	];

	pub const modeNames: [&'static str;1] = [
		"DEFAULT_MODE"
	];

	pub const ruleNames: [&'static str;2] = [
		"ID", "WS"
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


pub type LexerContext<'input> = BaseRuleContext<'input,EmptyCustomRuleContext<'input,LocalTokenFactory<'input> >>;
pub type LocalTokenFactory<'input> = CommonTokenFactory;

type From<'a> = <LocalTokenFactory<'a> as TokenFactory<'a> >::From;

pub struct SimpleLRLexer<'input, Input:CharStream<From<'input> >> {
	base: BaseLexer<'input,SimpleLRLexerActions,Input,LocalTokenFactory<'input>>,
}

antlr4rust::tid! { impl<'input,Input> TidAble<'input> for SimpleLRLexer<'input,Input> where Input:CharStream<From<'input> > }

impl<'input, Input:CharStream<From<'input> >> Deref for SimpleLRLexer<'input,Input>{
	type Target = BaseLexer<'input,SimpleLRLexerActions,Input,LocalTokenFactory<'input>>;

	fn deref(&self) -> &Self::Target {
		&self.base
	}
}

impl<'input, Input:CharStream<From<'input> >> DerefMut for SimpleLRLexer<'input,Input>{
	fn deref_mut(&mut self) -> &mut Self::Target {
		&mut self.base
	}
}


impl<'input, Input:CharStream<From<'input> >> SimpleLRLexer<'input,Input>{
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
        "SimpleLRLexer.g4"
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
				SimpleLRLexerActions{},
				tf
			)
	    }
	}
}

impl<'input, Input:CharStream<From<'input> >> SimpleLRLexer<'input,Input> where &'input LocalTokenFactory<'input>:Default{
	pub fn new(input: Input) -> Self{
		SimpleLRLexer::new_with_token_factory(input, <&LocalTokenFactory<'input> as Default>::default())
	}
}

pub struct SimpleLRLexerActions {
}

impl SimpleLRLexerActions{
}

impl<'input, Input:CharStream<From<'input> >> Actions<'input,BaseLexer<'input,SimpleLRLexerActions,Input,LocalTokenFactory<'input>>> for SimpleLRLexerActions{
	}

	impl<'input, Input:CharStream<From<'input> >> SimpleLRLexer<'input,Input>{

}

impl<'input, Input:CharStream<From<'input> >> LexerRecog<'input,BaseLexer<'input,SimpleLRLexerActions,Input,LocalTokenFactory<'input>>> for SimpleLRLexerActions{
}
impl<'input> TokenAware<'input> for SimpleLRLexerActions{
	type TF = LocalTokenFactory<'input>;
}

impl<'input, Input:CharStream<From<'input> >> TokenSource<'input> for SimpleLRLexer<'input,Input>{
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
			4, 0, 2, 14, 6, -1, 2, 0, 7, 0, 2, 1, 7, 1, 1, 0, 4, 0, 7, 8, 0, 11, 
			0, 12, 0, 8, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 2, 1, 1, 3, 2, 1, 0, 1, 2, 
			0, 10, 10, 32, 32, 14, 0, 1, 1, 0, 0, 0, 0, 3, 1, 0, 0, 0, 1, 6, 1, 0, 
			0, 0, 3, 10, 1, 0, 0, 0, 5, 7, 2, 97, 122, 0, 6, 5, 1, 0, 0, 0, 7, 8, 
			1, 0, 0, 0, 8, 6, 1, 0, 0, 0, 8, 9, 1, 0, 0, 0, 9, 2, 1, 0, 0, 0, 10, 
			11, 7, 0, 0, 0, 11, 12, 1, 0, 0, 0, 12, 13, 6, 1, 0, 0, 13, 4, 1, 0, 
			0, 0, 2, 0, 8, 1, 6, 0, 0
		];
	}