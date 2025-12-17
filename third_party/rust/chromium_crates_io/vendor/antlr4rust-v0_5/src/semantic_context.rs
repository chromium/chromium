use std::borrow::Cow::{Borrowed, Owned};
use std::borrow::{Borrow, Cow};
use std::cmp::Ordering;
use std::collections::HashSet;

use crate::parser::{Parser, ParserNodeType};

//pub trait SemanticContext:Sync + Send {
///    fn evaluate(&self, parser: &Recognizer, outerContext: &RuleContext) -> bool;
///    fn eval_precedence(&self, parser: &Recognizer, outerContext: &RuleContext, ) -> Box<dyn SemanticContext>;
//}

// fn empty() -> SemanticContext {
//     SemanticContext::Predicate {
//         rule_index: -1,
//         pred_index: -1,
//         is_ctx_dependent: false,
//     }
// }

#[derive(Clone, Eq, PartialEq, Hash, Debug)]
pub enum SemanticContext {
    Predicate {
        rule_index: i32,
        pred_index: i32,
        is_ctx_dependent: bool,
    },
    Precedence(i32),
    AND(Vec<SemanticContext>),
    OR(Vec<SemanticContext>),
}

impl SemanticContext {
    pub const NONE: SemanticContext = SemanticContext::Predicate {
        rule_index: -1,
        pred_index: -1,
        is_ctx_dependent: false,
    };
    pub(crate) fn evaluate<'a, T: Parser<'a>>(
        &self,
        parser: &mut T,
        outer_context: &<T::Node as ParserNodeType<'a>>::Type,
    ) -> bool {
        match self {
            SemanticContext::Predicate {
                rule_index,
                pred_index,
                is_ctx_dependent,
            } => {
                let _localctx = if *is_ctx_dependent {
                    Some(outer_context)
                } else {
                    None
                };
                parser.sempred(_localctx, *rule_index, *pred_index)
            }
            SemanticContext::Precedence(prec) => parser.precpred(Some(outer_context), *prec),
            SemanticContext::AND(ops) => ops.iter().all(|sem| sem.evaluate(parser, outer_context)),
            SemanticContext::OR(ops) => ops.iter().any(|sem| sem.evaluate(parser, outer_context)),
        }
    }
    pub(crate) fn eval_precedence<'a, 'b, T: Parser<'b>>(
        &'a self,
        parser: &T,
        outer_context: &<T::Node as ParserNodeType<'b>>::Type,
    ) -> Option<Cow<'a, SemanticContext>> {
        match self {
            SemanticContext::Predicate { .. } => Some(Borrowed(self)),
            SemanticContext::Precedence(prec) => {
                if parser.precpred(Some(outer_context), *prec) {
                    Some(Owned(Self::NONE))
                } else {
                    None
                }
            }
            SemanticContext::OR(ops) => {
                let mut differs = false;
                let mut operands = vec![];
                for context in ops {
                    let evaluated = context.eval_precedence(parser, outer_context);
                    differs |= evaluated.is_some() && context == evaluated.as_deref().unwrap();

                    if let Some(evaluated) = evaluated {
                        if *evaluated == Self::NONE {
                            return Some(Owned(Self::NONE));
                        } else {
                            operands.push(evaluated);
                        }
                    }
                }

                if !differs {
                    return Some(Borrowed(self));
                }

                if operands.is_empty() {
                    return None;
                }

                let mut operands = operands.drain(..);
                let result = operands.next().unwrap();
                Some(operands.fold(result, |acc, it| {
                    Owned(SemanticContext::or(Some(acc), Some(it)))
                }))
            }
            SemanticContext::AND(ops) => {
                let mut differs = false;
                let mut operands = vec![];
                for context in ops {
                    let evaluated = context.eval_precedence(parser, outer_context);
                    differs |= evaluated.is_some() && context == evaluated.as_deref().unwrap();

                    if let Some(evaluated) = evaluated {
                        if *evaluated != Self::NONE {
                            operands.push(evaluated);
                        }
                    } else {
                        return None;
                    }
                }

                if !differs {
                    return Some(Borrowed(self));
                }

                if operands.is_empty() {
                    return Some(Owned(Self::NONE));
                }

                let mut operands = operands.drain(..);
                let result = operands.next().unwrap();
                Some(operands.fold(result, |acc, it| {
                    Owned(SemanticContext::and(Some(acc), Some(it)))
                }))
            }
        }
    }

    pub fn new_and(a: &SemanticContext, b: &SemanticContext) -> SemanticContext {
        let mut operands = HashSet::new();
        if let SemanticContext::AND(ops) = a {
            operands.extend(ops.iter().cloned())
        } else {
            operands.insert(a.clone());
        }
        if let SemanticContext::AND(ops) = b {
            operands.extend(ops.iter().cloned())
        } else {
            operands.insert(b.clone());
        }

        let precedence_predicates = filter_precedence_predicate(&mut operands);
        if !precedence_predicates.is_empty() {
            let reduced = precedence_predicates.iter().min_by(sort_prec_pred);
            operands.insert(reduced.unwrap().clone());
        }

        if operands.len() == 1 {
            return operands.into_iter().next().unwrap();
        }

        SemanticContext::AND(operands.into_iter().collect())
    }

    pub fn new_or(a: &SemanticContext, b: &SemanticContext) -> SemanticContext {
        let mut operands = HashSet::new();
        if let SemanticContext::OR(ops) = a {
            operands.extend(ops.iter().cloned())
        } else {
            operands.insert(a.clone());
        }
        if let SemanticContext::OR(ops) = b {
            ops.iter().for_each(|it| {
                operands.insert(it.clone());
            });
        } else {
            operands.insert(b.clone());
        }

        let precedence_predicates = filter_precedence_predicate(&mut operands);
        if !precedence_predicates.is_empty() {
            let reduced = precedence_predicates.iter().max_by(sort_prec_pred);
            operands.insert(reduced.unwrap().clone());
        }

        if operands.len() == 1 {
            return operands.into_iter().next().unwrap();
        }

        SemanticContext::OR(operands.into_iter().collect())
    }

    pub fn and(
        a: Option<impl Borrow<SemanticContext>>,
        b: Option<impl Borrow<SemanticContext>>,
    ) -> SemanticContext {
        match (a, b) {
            (None, None) => Self::NONE,
            (None, Some(b)) => b.borrow().clone(),
            (Some(a), None) => a.borrow().clone(),
            (Some(a), Some(b)) => {
                let (a, b) = (a.borrow(), b.borrow());
                if *a == Self::NONE {
                    return b.clone();
                }
                if *b == Self::NONE {
                    return a.clone();
                }

                Self::new_and(a, b)
            }
        }
    }

    pub fn or(
        a: Option<impl Borrow<SemanticContext>>,
        b: Option<impl Borrow<SemanticContext>>,
    ) -> SemanticContext {
        match (a, b) {
            (None, None) => Self::NONE,
            (None, Some(b)) => b.borrow().clone(),
            (Some(a), None) => a.borrow().clone(),
            (Some(a), Some(b)) => {
                let (a, b) = (a.borrow(), b.borrow());
                if *a == Self::NONE || *b == Self::NONE {
                    return Self::NONE;
                }

                Self::new_or(a, b)
            }
        }
    }
}

fn sort_prec_pred(a: &&SemanticContext, b: &&SemanticContext) -> Ordering {
    match (*a, *b) {
        (SemanticContext::Precedence(a), SemanticContext::Precedence(b)) => a.cmp(b),
        _ => panic!("shoudl be sorting list of precedence predicates"),
    }
}

fn filter_precedence_predicate(collection: &mut HashSet<SemanticContext>) -> Vec<SemanticContext> {
    let mut result = vec![];
    collection.retain(|it| {
        if let SemanticContext::Precedence(_) = it {
            result.push(it.clone());
            false
        } else {
            true
        }
    });
    result
}
