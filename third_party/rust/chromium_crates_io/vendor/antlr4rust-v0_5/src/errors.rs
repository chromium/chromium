//! Error types
use crate::atn_simulator::IATNSimulator;
use crate::interval_set::IntervalSet;
use crate::parser::{Parser, ParserNodeType};
use crate::rule_context::states_stack;
use crate::token::{OwningToken, Token};
use crate::transition::PredicateTransition;
use crate::transition::TransitionType::TRANSITION_PREDICATE;
use std::borrow::Borrow;
use std::error::Error;
use std::fmt;
use std::fmt::Formatter;
use std::fmt::{Debug, Display};
use std::ops::Deref;
use std::rc::Rc;
use std::sync::Arc;

/// Main ANTLR4 Rust runtime error
#[derive(Debug, Clone)]
pub enum ANTLRError {
    /// Returned from Lexer when it fails to find matching token type for current input
    ///
    /// Usually Lexers contain last rule that captures all invalid tokens like:
    /// ```text
    /// ERROR_TOKEN: . ;
    /// ```
    /// to prevent lexer from throwing errors and have all error handling in parser.
    LexerNoAltError {
        /// Index at which error has happened
        start_index: isize,
    },

    /// Indicates that the parser could not decide which of two or more paths
    /// to take based upon the remaining input. It tracks the starting token
    /// of the offending input and also knows where the parser was
    /// in the various paths when the error. Reported by reportNoViableAlternative()
    NoAltError(NoViableAltError),

    /// This signifies any kind of mismatched input exceptions such as
    /// when the current input does not match the expected token.
    InputMismatchError(InputMisMatchError),

    /// A semantic predicate failed during validation. Validation of predicates
    /// occurs when normally parsing the alternative just like matching a token.
    /// Disambiguating predicate evaluation occurs when we test a predicate during
    /// prediction.
    PredicateError(FailedPredicateError),

    /// Internal error. Or user provided type returned data that is
    /// incompatible with current parser state
    IllegalStateError(String),

    /// Unrecoverable error. Indicates that error should not be processed by parser/error strategy
    /// and it should abort parsing and immediately return to caller.
    FallThrough(Arc<dyn Error + Send + Sync + 'static>),

    /// Potentially recoverable error.
    /// Used to allow user to emit his own errors from parser actions or from custom error strategy.
    /// Parser will try to recover with provided `ErrorStrategy`
    OtherError(Arc<dyn Error + Send + Sync + 'static>),
}

// impl Clone for ANTLRError {
//     fn clone(&self) -> Self {
//         match self {
//             ANTLRError::LexerNoAltError { start_index } => ANTLRError::LexerNoAltError {
//                 start_index: *start_index,
//             },
//             ANTLRError::NoAltError(e) => ANTLRError::NoAltError(e.clone()),
//             ANTLRError::InputMismatchError(e) => ANTLRError::InputMismatchError(e.clone()),
//             ANTLRError::PredicateError(e) => ANTLRError::PredicateError(e.clone()),
//             ANTLRError::IllegalStateError(e) => ANTLRError::IllegalStateError(e.clone()),
//             ANTLRError::FallThrough(_) => panic!("clone not supported"),
//             ANTLRError::OtherError(_) => panic!("clone not supported"),
//         }
//     }
// }

impl Display for ANTLRError {
    fn fmt(&self, _f: &mut Formatter<'_>) -> fmt::Result {
        <Self as Debug>::fmt(self, _f)
    }
}

impl Error for ANTLRError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            ANTLRError::FallThrough(x) => Some(x.as_ref()),
            ANTLRError::OtherError(x) => Some(x.as_ref()),
            _ => None,
        }
    }
}

impl ANTLRError {
    /// Returns first token that caused parser to fail.
    pub fn get_offending_token(&self) -> Option<&OwningToken> {
        Some(match self {
            ANTLRError::NoAltError(e) => &e.base.offending_token,
            ANTLRError::InputMismatchError(e) => &e.base.offending_token,
            ANTLRError::PredicateError(e) => &e.base.offending_token,
            _ => return None,
        })
    }
}

//impl ANTLRError {
//    fn get_expected_tokens(&self, _atn: &ATN) -> IntervalSet {
//        atn.get_expected_tokens(se)
//        unimplemented!()
//    }
//}

/// Common part of ANTLR parser errors
#[derive(Debug, Clone)]
#[allow(missing_docs)]
pub struct BaseRecognitionError {
    pub message: String,
    //    recognizer: Box<Recognizer>,
    pub offending_token: OwningToken,
    pub offending_state: i32,
    states_stack: Vec<i32>, // ctx: Rc<dyn ParserRuleContext>
                              //    input: Box<IntStream>
}

impl BaseRecognitionError {
    /// Returns tokens that were expected by parser in error place
    pub fn get_expected_tokens<'a, T: Parser<'a>>(&self, recognizer: &T) -> IntervalSet {
        recognizer
            .get_interpreter()
            .atn()
            .get_expected_tokens(self.offending_state, self.states_stack.iter().copied())
    }

    fn new<'a, T: Parser<'a>>(recog: &mut T) -> BaseRecognitionError {
        BaseRecognitionError {
            message: "".to_string(),
            offending_token: recog.get_current_token().borrow().to_owned(),
            offending_state: recog.get_state(),
            // ctx: recog.get_parser_rule_context().clone(),
            states_stack: states_stack(recog.get_parser_rule_context().clone()).collect(),
        }
    }
}

/// See `ANTLRError::NoAltError`
#[derive(Debug, Clone)]
#[allow(missing_docs)]
pub struct NoViableAltError {
    pub base: BaseRecognitionError,
    pub start_token: OwningToken,
    //    ctx: Rc<dyn ParserRuleContext>,
    //    dead_end_configs: BaseATNConfigSet,
}

#[allow(missing_docs)]
impl NoViableAltError {
    pub fn new<'a, T: Parser<'a>>(recog: &mut T) -> NoViableAltError {
        Self {
            base: BaseRecognitionError {
                message: "".to_string(),
                offending_token: recog.get_current_token().borrow().to_owned(),
                offending_state: recog.get_state(),
                // ctx: recog.get_parser_rule_context().clone(),
                states_stack: states_stack(recog.get_parser_rule_context().clone()).collect(),
            },
            start_token: recog.get_current_token().borrow().to_owned(),
            //            ctx: recog.get_parser_rule_context().clone()
        }
    }
    pub fn new_full<'a, T: Parser<'a>>(
        recog: &mut T,
        start_token: OwningToken,
        offending_token: OwningToken,
    ) -> NoViableAltError {
        Self {
            base: BaseRecognitionError {
                message: "".to_string(),
                offending_token,
                offending_state: recog.get_state(),
                states_stack: states_stack(recog.get_parser_rule_context().clone()).collect(), // ctx: recog.get_parser_rule_context().clone(),
            },
            start_token,
            //            ctx
        }
    }
}

/// See `ANTLRError::InputMismatchError`
#[derive(Debug, Clone)]
#[allow(missing_docs)]
pub struct InputMisMatchError {
    pub base: BaseRecognitionError,
}

#[allow(missing_docs)]
impl InputMisMatchError {
    pub fn new<'a, T: Parser<'a>>(recognizer: &mut T) -> InputMisMatchError {
        InputMisMatchError {
            base: BaseRecognitionError::new(recognizer),
        }
    }

    pub fn with_state<'a, T: Parser<'a>>(
        recognizer: &mut T,
        offending_state: i32,
        ctx: Rc<<T::Node as ParserNodeType<'a>>::Type>,
    ) -> InputMisMatchError {
        let mut a = Self::new(recognizer);
        // a.base.ctx = ctx;
        a.base.offending_state = offending_state;
        a.base.states_stack = states_stack(ctx).collect();
        a
    }
}

//fn new_input_mis_match_exception(recognizer: Parser) -> InputMisMatchError { unimplemented!() }

/// See `ANTLRError::PredicateError`
#[derive(Debug, Clone)]
#[allow(missing_docs)]
pub struct FailedPredicateError {
    pub base: BaseRecognitionError,
    pub rule_index: i32,
    pub predicate: String,
}

#[allow(missing_docs)]
impl FailedPredicateError {
    pub fn new<'a, T: Parser<'a>>(
        recog: &mut T,
        predicate: Option<String>,
        msg: Option<String>,
    ) -> ANTLRError {
        let tr = recog.get_interpreter().atn().states[recog.get_state() as usize]
            .get_transitions()
            .first()
            .unwrap();
        let (rule_index, _) = if tr.get_serialization_type() == TRANSITION_PREDICATE {
            let pr = tr.deref().cast::<PredicateTransition>();
            (pr.rule_index, pr.pred_index)
        } else {
            (0, 0)
        };

        ANTLRError::PredicateError(FailedPredicateError {
            base: BaseRecognitionError {
                message: msg.unwrap_or_else(|| {
                    format!(
                        "failed predicate: {}",
                        predicate.as_deref().unwrap_or("None")
                    )
                }),
                offending_token: recog.get_current_token().borrow().to_owned(),
                offending_state: recog.get_state(),
                states_stack: states_stack(recog.get_parser_rule_context().clone()).collect(), // ctx: recog.get_parser_rule_context().clone()
            },
            rule_index,
            predicate: predicate.unwrap_or_default(),
        })
    }
}
