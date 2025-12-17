use std::hash::{Hash, Hasher};

use murmur3::murmur3_32::MurmurHasher;

use crate::int_stream::IntStream;
use crate::lexer::Lexer;
use crate::lexer_action::LexerAction;
use crate::lexer_action::LexerAction::LexerIndexedCustomAction;

#[derive(Clone, Eq, PartialEq, Debug)]
pub(crate) struct LexerActionExecutor {
    cached_hash: u64,
    lexer_actions: Vec<LexerAction>,
}

impl Hash for LexerActionExecutor {
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_u64(self.cached_hash)
    }
}

impl LexerActionExecutor {
    pub(crate) fn new(lexer_actions: Vec<LexerAction>) -> LexerActionExecutor {
        //        let mut hasher = ;
        let cached_hash = lexer_actions
            .iter()
            .fold(MurmurHasher::default(), |mut acc, x| {
                x.hash(&mut acc);
                acc
            })
            .finish();
        LexerActionExecutor {
            lexer_actions,
            cached_hash,
        }
    }

    pub(crate) fn new_copy_append(
        old: Option<&Self>,
        lexer_action: LexerAction,
    ) -> LexerActionExecutor {
        let mut new = old
            .cloned()
            .unwrap_or_else(|| LexerActionExecutor::new(Vec::new()));
        new.lexer_actions.push(lexer_action);
        new
    }

    pub fn fix_offset_before_match(mut self, offset: isize) -> LexerActionExecutor {
        for action in self.lexer_actions.iter_mut() {
            match action {
                LexerAction::LexerIndexedCustomAction { .. } => {}
                _ => {
                    if action.is_position_dependent() {
                        *action = LexerIndexedCustomAction {
                            offset,
                            action: Box::new(action.clone()),
                        };
                    }
                }
            }
        }
        self
    }

    pub fn execute<'input>(&self, lexer: &mut impl Lexer<'input>, start_index: isize) {
        let mut requires_seek = false;
        let stop_index = lexer.input().index();
        for action in self.lexer_actions.iter() {
            //println!("executing action {:?}",action);
            if let LexerAction::LexerIndexedCustomAction { offset, .. } = action {
                lexer.input().seek(start_index + offset);
                requires_seek = start_index + offset != stop_index;
            } else if action.is_position_dependent() {
                lexer.input().seek(stop_index);
                requires_seek = false
            }
            action.execute(lexer);
        }
        if requires_seek {
            lexer.input().seek(stop_index);
        }
    }

    //    fn hash(&self) -> int { unimplemented!() }
}
