//! Utility functions
use crate::{prelude::*, Error};

/// Parse a tagged attribute. This is very helpful for implementing [`FromAttribute`].
///
/// A tagged attribute is an attribute in the form of `#[prefix(result)]`. This function will return `Some(result)` if the `prefix` matches.
///
/// The contents of the result can be either:
/// - `ParsedAttribute::Tagged(Ident)`, e.g. `#[serde(skip)]` will be `Tagged("skip")`
/// - `ParsedAttribute::Property(Ident, lit)`, e.g. `#[bincode(crate = "foo")]` will be `Property("crate", "foo")`
///
/// # Examples
/// ```
/// # use virtue::prelude::*;
/// # use std::str::FromStr;
/// # fn parse_token_stream_group(input: &'static str) -> Group {
/// #     let token_stream: TokenStream = proc_macro2::TokenStream::from_str(input).unwrap().into();
/// #     let mut iter = token_stream.into_iter();
/// #     let Some(TokenTree::Punct(_)) = iter.next() else { panic!() };
/// #     let Some(TokenTree::Group(group)) = iter.next() else { panic!() };
/// #     group
/// # }
/// use virtue::utils::{parse_tagged_attribute, ParsedAttribute};
///
/// // The attribute being parsed
/// let group: Group = parse_token_stream_group("#[prefix(result, foo = \"bar\")]");
///
/// let attributes = parse_tagged_attribute(&group, "prefix").unwrap().unwrap();
/// let mut iter = attributes.into_iter();
///
/// // The stream will contain the contents of the `prefix(...)`
/// match iter.next() {
///     Some(ParsedAttribute::Tag(i)) => {
///         assert_eq!(i.to_string(), String::from("result"));
///     },
///     x => panic!("Unexpected attribute: {:?}", x)
/// }
///  match iter.next() {
///     Some(ParsedAttribute::Property(key, val)) => {
///         assert_eq!(key.to_string(), String::from("foo"));
///         assert_eq!(val.to_string(), String::from("\"bar\""));
///     },
///     x => panic!("Unexpected attribute: {:?}", x)
/// }
///
/// ```
pub fn parse_tagged_attribute(group: &Group, prefix: &str) -> Result<Option<Vec<ParsedAttribute>>> {
    let stream = &mut group.stream().into_iter();
    if let Some(TokenTree::Ident(attribute_ident)) = stream.next() {
        #[allow(clippy::cmp_owned)] // clippy is wrong
        if attribute_ident.to_string() == prefix {
            if let Some(TokenTree::Group(group)) = stream.next() {
                let mut result = Vec::new();
                let mut stream = group.stream().into_iter().peekable();
                while let Some(token) = stream.next() {
                    match (token, stream.peek()) {
                        (TokenTree::Ident(key), Some(TokenTree::Punct(p)))
                            if p.as_char() == ',' =>
                        {
                            result.push(ParsedAttribute::Tag(key));
                            stream.next();
                        }
                        (TokenTree::Ident(key), None) => {
                            result.push(ParsedAttribute::Tag(key));
                            stream.next();
                        }
                        (TokenTree::Ident(key), Some(TokenTree::Punct(p)))
                            if p.as_char() == '=' =>
                        {
                            stream.next();
                            if let Some(TokenTree::Literal(lit)) = stream.next() {
                                result.push(ParsedAttribute::Property(key, lit));

                                match stream.next() {
                                    Some(TokenTree::Punct(p)) if p.as_char() == ',' => {}
                                    None => {}
                                    x => {
                                        return Err(Error::custom_at_opt_token("Expected `,`", x));
                                    }
                                }
                            }
                        }
                        (x, _) => {
                            return Err(Error::custom_at(
                                "Expected `key` or `key = \"val\"`",
                                x.span(),
                            ));
                        }
                    }
                }

                return Ok(Some(result));
            }
        }
    }
    Ok(None)
}

#[derive(Clone, Debug)]
#[non_exhaustive]
/// A parsed attribute. See [`parse_tagged_attribute`] for more information.
pub enum ParsedAttribute {
    /// A tag, created by parsing `#[prefix(foo)]`
    Tag(Ident),
    /// A property, created by parsing `#[prefix(foo = "bar")]`
    Property(Ident, Literal),
}

#[test]
fn test_parse_tagged_attribute() {
    let group: Group = match crate::token_stream("[prefix(result, foo = \"bar\", baz)]").next() {
        Some(TokenTree::Group(group)) => group,
        x => panic!("Unexpected token {:?}", x),
    };

    let attributes = parse_tagged_attribute(&group, "prefix").unwrap().unwrap();
    let mut iter = attributes.into_iter();

    // The stream will contain the contents of the `prefix(...)`
    match iter.next() {
        Some(ParsedAttribute::Tag(i)) => {
            assert_eq!(i.to_string(), String::from("result"));
        }
        x => panic!("Unexpected attribute: {:?}", x),
    }
    match iter.next() {
        Some(ParsedAttribute::Property(key, val)) => {
            assert_eq!(key.to_string(), String::from("foo"));
            assert_eq!(val.to_string(), String::from("\"bar\""));
        }
        x => panic!("Unexpected attribute: {:?}", x),
    }
    match iter.next() {
        Some(ParsedAttribute::Tag(i)) => {
            assert_eq!(i.to_string(), String::from("baz"));
        }
        x => panic!("Unexpected attribute: {:?}", x),
    }
}
