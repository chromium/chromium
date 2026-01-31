// Generated from XMLLexer.g4 by ANTLR 4.13.2
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


	pub const COMMENT:i32=1; 
	pub const CDATA:i32=2; 
	pub const DTD:i32=3; 
	pub const EntityRef:i32=4; 
	pub const CharRef:i32=5; 
	pub const SEA_WS:i32=6; 
	pub const OPEN:i32=7; 
	pub const XMLDeclOpen:i32=8; 
	pub const TEXT:i32=9; 
	pub const CLOSE:i32=10; 
	pub const SPECIAL_CLOSE:i32=11; 
	pub const SLASH_CLOSE:i32=12; 
	pub const SLASH:i32=13; 
	pub const EQUALS:i32=14; 
	pub const STRING:i32=15; 
	pub const Name:i32=16; 
	pub const S:i32=17; 
	pub const PI:i32=18;
	pub const channelNames: [&'static str;0+2] = [
		"DEFAULT_TOKEN_CHANNEL", "HIDDEN"
	];

	pub const modeNames: [&'static str;3] = [
		"DEFAULT_MODE", "INSIDE", "PROC_INSTR"
	];

	pub const ruleNames: [&'static str;24] = [
		"COMMENT", "CDATA", "DTD", "EntityRef", "CharRef", "SEA_WS", "OPEN", "XMLDeclOpen", 
		"SPECIAL_OPEN", "TEXT", "CLOSE", "SPECIAL_CLOSE", "SLASH_CLOSE", "SLASH", 
		"EQUALS", "STRING", "Name", "S", "HEXDIGIT", "DIGIT", "NameChar", "NameStartChar", 
		"PI", "IGNORE"
	];


	pub const _LITERAL_NAMES: [Option<&'static str>;15] = [
		None, None, None, None, None, None, None, Some("'<'"), None, None, Some("'>'"), 
		None, Some("'/>'"), Some("'/'"), Some("'='")
	];
	pub const _SYMBOLIC_NAMES: [Option<&'static str>;19]  = [
		None, Some("COMMENT"), Some("CDATA"), Some("DTD"), Some("EntityRef"), 
		Some("CharRef"), Some("SEA_WS"), Some("OPEN"), Some("XMLDeclOpen"), Some("TEXT"), 
		Some("CLOSE"), Some("SPECIAL_CLOSE"), Some("SLASH_CLOSE"), Some("SLASH"), 
		Some("EQUALS"), Some("STRING"), Some("Name"), Some("S"), Some("PI")
	];
	lazy_static!{
	    static ref _shared_context_cache: Arc<PredictionContextCache> = Arc::new(PredictionContextCache::new());
		static ref VOCABULARY: Box<dyn Vocabulary> = Box::new(VocabularyImpl::new(_LITERAL_NAMES.iter(), _SYMBOLIC_NAMES.iter(), None));
	}


pub type LexerContext<'input> = BaseRuleContext<'input,EmptyCustomRuleContext<'input,LocalTokenFactory<'input> >>;
pub type LocalTokenFactory<'input> = CommonTokenFactory;

type From<'a> = <LocalTokenFactory<'a> as TokenFactory<'a> >::From;

pub struct XMLLexer<'input, Input:CharStream<From<'input> >> {
	base: BaseLexer<'input,XMLLexerActions,Input,LocalTokenFactory<'input>>,
}

antlr4rust::tid! { impl<'input,Input> TidAble<'input> for XMLLexer<'input,Input> where Input:CharStream<From<'input> > }

impl<'input, Input:CharStream<From<'input> >> Deref for XMLLexer<'input,Input>{
	type Target = BaseLexer<'input,XMLLexerActions,Input,LocalTokenFactory<'input>>;

	fn deref(&self) -> &Self::Target {
		&self.base
	}
}

impl<'input, Input:CharStream<From<'input> >> DerefMut for XMLLexer<'input,Input>{
	fn deref_mut(&mut self) -> &mut Self::Target {
		&mut self.base
	}
}


impl<'input, Input:CharStream<From<'input> >> XMLLexer<'input,Input>{
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
        "XMLLexer.g4"
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
				XMLLexerActions{},
				tf
			)
	    }
	}
}

impl<'input, Input:CharStream<From<'input> >> XMLLexer<'input,Input> where &'input LocalTokenFactory<'input>:Default{
	pub fn new(input: Input) -> Self{
		XMLLexer::new_with_token_factory(input, <&LocalTokenFactory<'input> as Default>::default())
	}
}

pub struct XMLLexerActions {
}

impl XMLLexerActions{
}

impl<'input, Input:CharStream<From<'input> >> Actions<'input,BaseLexer<'input,XMLLexerActions,Input,LocalTokenFactory<'input>>> for XMLLexerActions{

	fn action(_localctx: Option<&EmptyContext<'input,LocalTokenFactory<'input>> >, rule_index: i32, action_index: i32,
	          recog:&mut BaseLexer<'input,XMLLexerActions,Input,LocalTokenFactory<'input>>
	    ){
	    	match rule_index {
			        10 =>
			        	XMLLexer::<'input>::CLOSE_action(None, action_index, recog), 
			_ => {}
		}
	}
	fn sempred(_localctx: Option<&EmptyContext<'input,LocalTokenFactory<'input>> >, rule_index: i32, pred_index: i32,
	           recog:&mut BaseLexer<'input,XMLLexerActions,Input,LocalTokenFactory<'input>>
	    ) -> bool {
	    	match rule_index {
			        0 =>
			        	XMLLexer::<'input>::COMMENT_sempred(None, pred_index, recog), 
			_ => true
		}
	}

	}

	impl<'input, Input:CharStream<From<'input> >> XMLLexer<'input,Input>{

		fn CLOSE_action(_localctx: Option<&LexerContext<'input>>, action_index: i32,
						   recog:&mut <Self as Deref>::Target
			) {
			match action_index {
			 		0=>{
						recog.pop_mode();
					},

				_ => {}
			}
		}
		fn COMMENT_sempred(_localctx: Option<&LexerContext<'input>>, pred_index:i32,
							recog:&mut <Self as Deref>::Target
			) -> bool {
			match pred_index {
					0=>{
						true
					}
				_ => true
			}
		}


}

impl<'input, Input:CharStream<From<'input> >> LexerRecog<'input,BaseLexer<'input,XMLLexerActions,Input,LocalTokenFactory<'input>>> for XMLLexerActions{
}
impl<'input> TokenAware<'input> for XMLLexerActions{
	type TF = LocalTokenFactory<'input>;
}

impl<'input, Input:CharStream<From<'input> >> TokenSource<'input> for XMLLexer<'input,Input>{
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
			4, 0, 18, 230, 6, -1, 6, -1, 6, -1, 2, 0, 7, 0, 2, 1, 7, 1, 2, 2, 7, 
			2, 2, 3, 7, 3, 2, 4, 7, 4, 2, 5, 7, 5, 2, 6, 7, 6, 2, 7, 7, 7, 2, 8, 
			7, 8, 2, 9, 7, 9, 2, 10, 7, 10, 2, 11, 7, 11, 2, 12, 7, 12, 2, 13, 7, 
			13, 2, 14, 7, 14, 2, 15, 7, 15, 2, 16, 7, 16, 2, 17, 7, 17, 2, 18, 7, 
			18, 2, 19, 7, 19, 2, 20, 7, 20, 2, 21, 7, 21, 2, 22, 7, 22, 2, 23, 7, 
			23, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 5, 0, 58, 8, 0, 10, 0, 12, 0, 
			61, 9, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 1, 80, 8, 1, 10, 1, 12, 
			1, 83, 9, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 1, 2, 1, 2, 5, 2, 93, 
			8, 2, 10, 2, 12, 2, 96, 9, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 3, 1, 3, 1, 
			3, 1, 3, 1, 4, 1, 4, 1, 4, 1, 4, 4, 4, 110, 8, 4, 11, 4, 12, 4, 111, 
			1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 4, 4, 121, 8, 4, 11, 4, 12, 
			4, 122, 1, 4, 1, 4, 3, 4, 127, 8, 4, 1, 5, 1, 5, 3, 5, 131, 8, 5, 1, 
			5, 3, 5, 134, 8, 5, 1, 6, 1, 6, 1, 6, 1, 6, 1, 7, 1, 7, 1, 7, 1, 7, 1, 
			7, 1, 7, 1, 7, 1, 7, 1, 7, 1, 7, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 
			1, 8, 1, 8, 1, 9, 4, 9, 159, 8, 9, 11, 9, 12, 9, 160, 1, 10, 1, 10, 1, 
			10, 1, 11, 1, 11, 1, 11, 1, 11, 1, 11, 1, 12, 1, 12, 1, 12, 1, 12, 1, 
			12, 1, 13, 1, 13, 1, 14, 1, 14, 1, 15, 1, 15, 5, 15, 182, 8, 15, 10, 
			15, 12, 15, 185, 9, 15, 1, 15, 1, 15, 1, 15, 5, 15, 190, 8, 15, 10, 15, 
			12, 15, 193, 9, 15, 1, 15, 3, 15, 196, 8, 15, 1, 16, 1, 16, 5, 16, 200, 
			8, 16, 10, 16, 12, 16, 203, 9, 16, 1, 17, 1, 17, 1, 17, 1, 17, 1, 18, 
			1, 18, 1, 19, 1, 19, 1, 20, 1, 20, 1, 20, 1, 20, 3, 20, 217, 8, 20, 1, 
			21, 3, 21, 220, 8, 21, 1, 22, 1, 22, 1, 22, 1, 22, 1, 22, 1, 23, 1, 23, 
			1, 23, 1, 23, 3, 59, 81, 94, 0, 24, 3, 1, 5, 2, 7, 3, 9, 4, 11, 5, 13, 
			6, 15, 7, 17, 8, 19, 0, 21, 9, 23, 10, 25, 11, 27, 12, 29, 13, 31, 14, 
			33, 15, 35, 16, 37, 17, 39, 0, 41, 0, 43, 0, 45, 0, 47, 18, 49, 0, 3, 
			0, 1, 2, 9, 2, 0, 9, 9, 32, 32, 2, 0, 38, 38, 60, 60, 2, 0, 34, 34, 60, 
			60, 2, 0, 39, 39, 60, 60, 3, 0, 9, 10, 13, 13, 32, 32, 3, 0, 48, 57, 
			65, 70, 97, 102, 1, 0, 48, 57, 3, 0, 183, 183, 768, 879, 8255, 8256, 
			8, 0, 58, 58, 65, 90, 97, 122, 8304, 8591, 11264, 12271, 12289, 55295, 
			63744, 64975, 65008, 65533, 239, 0, 3, 1, 0, 0, 0, 0, 5, 1, 0, 0, 0, 
			0, 7, 1, 0, 0, 0, 0, 9, 1, 0, 0, 0, 0, 11, 1, 0, 0, 0, 0, 13, 1, 0, 0, 
			0, 0, 15, 1, 0, 0, 0, 0, 17, 1, 0, 0, 0, 0, 19, 1, 0, 0, 0, 0, 21, 1, 
			0, 0, 0, 1, 23, 1, 0, 0, 0, 1, 25, 1, 0, 0, 0, 1, 27, 1, 0, 0, 0, 1, 
			29, 1, 0, 0, 0, 1, 31, 1, 0, 0, 0, 1, 33, 1, 0, 0, 0, 1, 35, 1, 0, 0, 
			0, 1, 37, 1, 0, 0, 0, 2, 47, 1, 0, 0, 0, 2, 49, 1, 0, 0, 0, 3, 51, 1, 
			0, 0, 0, 5, 68, 1, 0, 0, 0, 7, 88, 1, 0, 0, 0, 9, 101, 1, 0, 0, 0, 11, 
			126, 1, 0, 0, 0, 13, 133, 1, 0, 0, 0, 15, 135, 1, 0, 0, 0, 17, 139, 1, 
			0, 0, 0, 19, 149, 1, 0, 0, 0, 21, 158, 1, 0, 0, 0, 23, 162, 1, 0, 0, 
			0, 25, 165, 1, 0, 0, 0, 27, 170, 1, 0, 0, 0, 29, 175, 1, 0, 0, 0, 31, 
			177, 1, 0, 0, 0, 33, 195, 1, 0, 0, 0, 35, 197, 1, 0, 0, 0, 37, 204, 1, 
			0, 0, 0, 39, 208, 1, 0, 0, 0, 41, 210, 1, 0, 0, 0, 43, 216, 1, 0, 0, 
			0, 45, 219, 1, 0, 0, 0, 47, 221, 1, 0, 0, 0, 49, 226, 1, 0, 0, 0, 51, 
			52, 5, 60, 0, 0, 52, 53, 5, 33, 0, 0, 53, 54, 5, 45, 0, 0, 54, 55, 5, 
			45, 0, 0, 55, 59, 1, 0, 0, 0, 56, 58, 9, 0, 0, 0, 57, 56, 1, 0, 0, 0, 
			58, 61, 1, 0, 0, 0, 59, 60, 1, 0, 0, 0, 59, 57, 1, 0, 0, 0, 60, 62, 1, 
			0, 0, 0, 61, 59, 1, 0, 0, 0, 62, 63, 5, 45, 0, 0, 63, 64, 5, 45, 0, 0, 
			64, 65, 5, 62, 0, 0, 65, 66, 1, 0, 0, 0, 66, 67, 4, 0, 0, 0, 67, 4, 1, 
			0, 0, 0, 68, 69, 5, 60, 0, 0, 69, 70, 5, 33, 0, 0, 70, 71, 5, 91, 0, 
			0, 71, 72, 5, 67, 0, 0, 72, 73, 5, 68, 0, 0, 73, 74, 5, 65, 0, 0, 74, 
			75, 5, 84, 0, 0, 75, 76, 5, 65, 0, 0, 76, 77, 5, 91, 0, 0, 77, 81, 1, 
			0, 0, 0, 78, 80, 9, 0, 0, 0, 79, 78, 1, 0, 0, 0, 80, 83, 1, 0, 0, 0, 
			81, 82, 1, 0, 0, 0, 81, 79, 1, 0, 0, 0, 82, 84, 1, 0, 0, 0, 83, 81, 1, 
			0, 0, 0, 84, 85, 5, 93, 0, 0, 85, 86, 5, 93, 0, 0, 86, 87, 5, 62, 0, 
			0, 87, 6, 1, 0, 0, 0, 88, 89, 5, 60, 0, 0, 89, 90, 5, 33, 0, 0, 90, 94, 
			1, 0, 0, 0, 91, 93, 9, 0, 0, 0, 92, 91, 1, 0, 0, 0, 93, 96, 1, 0, 0, 
			0, 94, 95, 1, 0, 0, 0, 94, 92, 1, 0, 0, 0, 95, 97, 1, 0, 0, 0, 96, 94, 
			1, 0, 0, 0, 97, 98, 5, 62, 0, 0, 98, 99, 1, 0, 0, 0, 99, 100, 6, 2, 0, 
			0, 100, 8, 1, 0, 0, 0, 101, 102, 5, 38, 0, 0, 102, 103, 3, 35, 16, 0, 
			103, 104, 5, 59, 0, 0, 104, 10, 1, 0, 0, 0, 105, 106, 5, 38, 0, 0, 106, 
			107, 5, 35, 0, 0, 107, 109, 1, 0, 0, 0, 108, 110, 3, 41, 19, 0, 109, 
			108, 1, 0, 0, 0, 110, 111, 1, 0, 0, 0, 111, 109, 1, 0, 0, 0, 111, 112, 
			1, 0, 0, 0, 112, 113, 1, 0, 0, 0, 113, 114, 5, 59, 0, 0, 114, 127, 1, 
			0, 0, 0, 115, 116, 5, 38, 0, 0, 116, 117, 5, 35, 0, 0, 117, 118, 5, 120, 
			0, 0, 118, 120, 1, 0, 0, 0, 119, 121, 3, 39, 18, 0, 120, 119, 1, 0, 0, 
			0, 121, 122, 1, 0, 0, 0, 122, 120, 1, 0, 0, 0, 122, 123, 1, 0, 0, 0, 
			123, 124, 1, 0, 0, 0, 124, 125, 5, 59, 0, 0, 125, 127, 1, 0, 0, 0, 126, 
			105, 1, 0, 0, 0, 126, 115, 1, 0, 0, 0, 127, 12, 1, 0, 0, 0, 128, 134, 
			7, 0, 0, 0, 129, 131, 5, 13, 0, 0, 130, 129, 1, 0, 0, 0, 130, 131, 1, 
			0, 0, 0, 131, 132, 1, 0, 0, 0, 132, 134, 5, 10, 0, 0, 133, 128, 1, 0, 
			0, 0, 133, 130, 1, 0, 0, 0, 134, 14, 1, 0, 0, 0, 135, 136, 5, 60, 0, 
			0, 136, 137, 1, 0, 0, 0, 137, 138, 6, 6, 1, 0, 138, 16, 1, 0, 0, 0, 139, 
			140, 5, 60, 0, 0, 140, 141, 5, 63, 0, 0, 141, 142, 5, 120, 0, 0, 142, 
			143, 5, 109, 0, 0, 143, 144, 5, 108, 0, 0, 144, 145, 1, 0, 0, 0, 145, 
			146, 3, 37, 17, 0, 146, 147, 1, 0, 0, 0, 147, 148, 6, 7, 1, 0, 148, 18, 
			1, 0, 0, 0, 149, 150, 5, 60, 0, 0, 150, 151, 5, 63, 0, 0, 151, 152, 1, 
			0, 0, 0, 152, 153, 3, 35, 16, 0, 153, 154, 1, 0, 0, 0, 154, 155, 6, 8, 
			2, 0, 155, 156, 6, 8, 3, 0, 156, 20, 1, 0, 0, 0, 157, 159, 8, 1, 0, 0, 
			158, 157, 1, 0, 0, 0, 159, 160, 1, 0, 0, 0, 160, 158, 1, 0, 0, 0, 160, 
			161, 1, 0, 0, 0, 161, 22, 1, 0, 0, 0, 162, 163, 5, 62, 0, 0, 163, 164, 
			6, 10, 4, 0, 164, 24, 1, 0, 0, 0, 165, 166, 5, 63, 0, 0, 166, 167, 5, 
			62, 0, 0, 167, 168, 1, 0, 0, 0, 168, 169, 6, 11, 5, 0, 169, 26, 1, 0, 
			0, 0, 170, 171, 5, 47, 0, 0, 171, 172, 5, 62, 0, 0, 172, 173, 1, 0, 0, 
			0, 173, 174, 6, 12, 5, 0, 174, 28, 1, 0, 0, 0, 175, 176, 5, 47, 0, 0, 
			176, 30, 1, 0, 0, 0, 177, 178, 5, 61, 0, 0, 178, 32, 1, 0, 0, 0, 179, 
			183, 5, 34, 0, 0, 180, 182, 8, 2, 0, 0, 181, 180, 1, 0, 0, 0, 182, 185, 
			1, 0, 0, 0, 183, 181, 1, 0, 0, 0, 183, 184, 1, 0, 0, 0, 184, 186, 1, 
			0, 0, 0, 185, 183, 1, 0, 0, 0, 186, 196, 5, 34, 0, 0, 187, 191, 5, 39, 
			0, 0, 188, 190, 8, 3, 0, 0, 189, 188, 1, 0, 0, 0, 190, 193, 1, 0, 0, 
			0, 191, 189, 1, 0, 0, 0, 191, 192, 1, 0, 0, 0, 192, 194, 1, 0, 0, 0, 
			193, 191, 1, 0, 0, 0, 194, 196, 5, 39, 0, 0, 195, 179, 1, 0, 0, 0, 195, 
			187, 1, 0, 0, 0, 196, 34, 1, 0, 0, 0, 197, 201, 3, 45, 21, 0, 198, 200, 
			3, 43, 20, 0, 199, 198, 1, 0, 0, 0, 200, 203, 1, 0, 0, 0, 201, 199, 1, 
			0, 0, 0, 201, 202, 1, 0, 0, 0, 202, 36, 1, 0, 0, 0, 203, 201, 1, 0, 0, 
			0, 204, 205, 7, 4, 0, 0, 205, 206, 1, 0, 0, 0, 206, 207, 6, 17, 0, 0, 
			207, 38, 1, 0, 0, 0, 208, 209, 7, 5, 0, 0, 209, 40, 1, 0, 0, 0, 210, 
			211, 7, 6, 0, 0, 211, 42, 1, 0, 0, 0, 212, 217, 3, 45, 21, 0, 213, 217, 
			2, 45, 46, 0, 214, 217, 3, 41, 19, 0, 215, 217, 7, 7, 0, 0, 216, 212, 
			1, 0, 0, 0, 216, 213, 1, 0, 0, 0, 216, 214, 1, 0, 0, 0, 216, 215, 1, 
			0, 0, 0, 217, 44, 1, 0, 0, 0, 218, 220, 7, 8, 0, 0, 219, 218, 1, 0, 0, 
			0, 220, 46, 1, 0, 0, 0, 221, 222, 5, 63, 0, 0, 222, 223, 5, 62, 0, 0, 
			223, 224, 1, 0, 0, 0, 224, 225, 6, 22, 5, 0, 225, 48, 1, 0, 0, 0, 226, 
			227, 9, 0, 0, 0, 227, 228, 1, 0, 0, 0, 228, 229, 6, 23, 2, 0, 229, 50, 
			1, 0, 0, 0, 18, 0, 1, 2, 59, 81, 94, 111, 122, 126, 130, 133, 160, 183, 
			191, 195, 201, 216, 219, 6, 6, 0, 0, 5, 1, 0, 3, 0, 0, 5, 2, 0, 1, 10, 
			0, 4, 0, 0
		];
	}