#![allow(missing_docs)]
//! Mapping from symbol type to its string representation
use std::borrow::Borrow;
//use std::borrow::Cow;
use std::borrow::Cow::{self, Borrowed, Owned};
use std::cmp::max;
use std::fmt::Debug;

use crate::dfa::ScopeExt;
use crate::token::TOKEN_EOF;

pub trait Vocabulary: Sync + Send + Debug {
    fn get_max_token_type(&self) -> i32;
    fn get_literal_name(&self, token_type: i32) -> Option<&str>;
    fn get_symbolic_name(&self, token_type: i32) -> Option<&str>;
    fn get_display_name(&self, token_type: i32) -> Cow<'_, str>;
}

#[derive(Debug)]
pub struct VocabularyImpl {
    literal_names: Vec<Option<String>>,
    symbolic_names: Vec<Option<String>>,
    display_names: Vec<Option<String>>,
    max_token_type: i32,
}

fn collect_to_string<'b, T: Borrow<str> + 'b>(
    iter: impl IntoIterator<Item = &'b Option<T>>,
) -> Vec<Option<String>> {
    iter.into_iter()
        .map(|x| x.as_ref().map(|it| it.borrow().to_owned()))
        .collect()
}

impl VocabularyImpl {
    pub fn new<'b, T: Borrow<str> + 'b, Iter: IntoIterator<Item = &'b Option<T>>>(
        literal_names: Iter,
        symbolic_names: Iter,
        display_names: Option<Iter>,
    ) -> VocabularyImpl {
        //        let display_names = display_names.unwrap_or(&[]);
        VocabularyImpl {
            literal_names: collect_to_string(literal_names),
            symbolic_names: collect_to_string(symbolic_names),
            display_names: collect_to_string(display_names.into_iter().flatten()),
            max_token_type: 0,
        }
        .modify_with(|it| {
            it.max_token_type = max(
                it.literal_names.len(),
                max(it.symbolic_names.len(), it.display_names.len()),
            ) as i32
                - 1
        })
    }

    pub fn from_token_names(token_names: &[Option<&str>]) -> VocabularyImpl {
        let token_names = collect_to_string(token_names.iter());
        let mut literal_names = token_names.clone();
        let mut symbolic_names = token_names.clone();

        for (i, tn) in token_names.iter().enumerate() {
            match tn {
                Some(tn) if !tn.is_empty() && tn.starts_with('\'') => {
                    symbolic_names[i] = None;
                    continue;
                }
                Some(tn) if !tn.is_empty() && tn.chars().next().unwrap().is_uppercase() => {
                    literal_names[i] = None;
                    continue;
                }
                None => {
                    continue;
                }
                _ => {}
            }
            literal_names[i] = None;
            symbolic_names[i] = None;
        }

        Self::new(
            literal_names.iter(),
            symbolic_names.iter(),
            Some(token_names.iter()),
        )
    }
}

impl Vocabulary for VocabularyImpl {
    fn get_max_token_type(&self) -> i32 {
        self.max_token_type
    }

    fn get_literal_name(&self, token_type: i32) -> Option<&str> {
        self.literal_names
            .get(token_type as usize)
            .and_then(|x| x.as_deref())
    }

    fn get_symbolic_name(&self, token_type: i32) -> Option<&str> {
        if token_type == TOKEN_EOF {
            return Some("EOF");
        }
        self.symbolic_names
            .get(token_type as usize)
            .and_then(|x| x.as_deref())
    }

    fn get_display_name(&self, token_type: i32) -> Cow<'_, str> {
        self.display_names
            .get(token_type as usize)
            .and_then(|x| x.as_deref())
            .or_else(|| self.get_literal_name(token_type))
            .or_else(|| self.get_symbolic_name(token_type))
            .map(Borrowed)
            .unwrap_or(Owned(token_type.to_string()))
    }
}

pub(crate) static DUMMY_VOCAB: DummyVocab = DummyVocab;

#[derive(Debug)]
pub(crate) struct DummyVocab;

impl Vocabulary for DummyVocab {
    fn get_max_token_type(&self) -> i32 {
        unimplemented!()
    }

    fn get_literal_name(&self, _token_type: i32) -> Option<&str> {
        unimplemented!()
    }

    fn get_symbolic_name(&self, _token_type: i32) -> Option<&str> {
        unimplemented!()
    }

    fn get_display_name(&self, token_type: i32) -> Cow<'_, str> {
        token_type.to_string().into()
    }
}
