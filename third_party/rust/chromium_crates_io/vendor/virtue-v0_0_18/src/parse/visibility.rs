use super::utils::*;
use crate::prelude::{Delimiter, TokenTree};
use crate::Result;
use std::iter::Peekable;

/// The visibility of a struct, enum, field, etc
#[derive(Debug, PartialEq, Eq, Clone)]
pub enum Visibility {
    /// Default visibility. Most items are private by default.
    Default,

    /// Public visibility
    Pub,
}

impl Visibility {
    pub(crate) fn try_take(input: &mut Peekable<impl Iterator<Item = TokenTree>>) -> Result<Self> {
        match input.peek() {
            Some(TokenTree::Ident(ident)) if ident_eq(ident, "pub") => {
                // Consume this token
                assume_ident(input.next());

                // check if the next token is `pub(...)`
                if let Some(TokenTree::Group(g)) = input.peek() {
                    if g.delimiter() == Delimiter::Parenthesis {
                        // check if this is one of:
                        // - pub ( crate )
                        // - pub ( self )
                        // - pub ( super )
                        // - pub ( in ... )
                        if let Some(TokenTree::Ident(i)) = g.stream().into_iter().next() {
                            if matches!(i.to_string().as_str(), "crate" | "self" | "super" | "in") {
                                // it is, ignore this token
                                assume_group(input.next());
                            }
                        }
                    }
                }

                Ok(Visibility::Pub)
            }
            Some(TokenTree::Group(group)) => {
                // sometimes this is a group instead of an ident
                // e.g. when used in `bitflags! {}`
                let mut iter = group.stream().into_iter();
                match (iter.next(), iter.next()) {
                    (Some(TokenTree::Ident(ident)), None) if ident_eq(&ident, "pub") => {
                        // Consume this token
                        assume_group(input.next());

                        // check if the next token is `pub(...)`
                        if let Some(TokenTree::Group(_)) = input.peek() {
                            // we just consume the visibility, we're not actually using it for generation
                            assume_group(input.next());
                        }
                        Ok(Visibility::Pub)
                    }
                    _ => Ok(Visibility::Default),
                }
            }
            _ => Ok(Visibility::Default),
        }
    }
}

#[test]
fn test_visibility_try_take() {
    use crate::token_stream;

    assert_eq!(
        Visibility::Default,
        Visibility::try_take(&mut token_stream("")).unwrap()
    );
    assert_eq!(
        Visibility::Pub,
        Visibility::try_take(&mut token_stream("pub")).unwrap()
    );
    assert_eq!(
        Visibility::Pub,
        Visibility::try_take(&mut token_stream(" pub ")).unwrap(),
    );
    assert_eq!(
        Visibility::Pub,
        Visibility::try_take(&mut token_stream("\tpub\t")).unwrap()
    );
    assert_eq!(
        Visibility::Pub,
        Visibility::try_take(&mut token_stream("pub(crate)")).unwrap()
    );
    assert_eq!(
        Visibility::Pub,
        Visibility::try_take(&mut token_stream(" pub ( crate ) ")).unwrap()
    );
    assert_eq!(
        Visibility::Pub,
        Visibility::try_take(&mut token_stream("\tpub\t(\tcrate\t)\t")).unwrap()
    );

    assert_eq!(
        Visibility::Default,
        Visibility::try_take(&mut token_stream("pb")).unwrap()
    );
}
