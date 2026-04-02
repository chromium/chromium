use super::attributes::AttributeLocation;
use super::{utils::*, Attribute, Visibility};
use crate::prelude::{Delimiter, Ident, Literal, Span, TokenTree};
use crate::{Error, Result};
use std::iter::Peekable;

/// The body of a struct
#[derive(Debug)]
pub struct StructBody {
    /// The fields of this struct, `None` if this struct has no fields
    pub fields: Option<Fields>,
}

impl StructBody {
    pub(crate) fn take(input: &mut Peekable<impl Iterator<Item = TokenTree>>) -> Result<Self> {
        match input.peek() {
            Some(TokenTree::Group(_)) => {}
            Some(TokenTree::Punct(p)) if p.as_char() == ';' => {
                return Ok(StructBody { fields: None })
            }
            token => return Error::wrong_token(token, "group or punct"),
        }
        let group = assume_group(input.next());
        let mut stream = group.stream().into_iter().peekable();
        let fields = match group.delimiter() {
            Delimiter::Brace => {
                let fields = UnnamedField::parse_with_name(&mut stream)?;
                Some(Fields::Struct(fields))
            }
            Delimiter::Parenthesis => {
                let fields = UnnamedField::parse(&mut stream)?;
                Some(Fields::Tuple(fields))
            }
            found => {
                return Err(Error::InvalidRustSyntax {
                    span: group.span(),
                    expected: format!("brace or parenthesis, found {:?}", found),
                })
            }
        };
        Ok(StructBody { fields })
    }
}

#[test]
fn test_struct_body_take() {
    use crate::token_stream;

    let stream = &mut token_stream(
        "struct Foo { pub bar: u8, pub(crate) baz: u32, bla: Vec<Box<dyn Future<Output = ()>>> }",
    );
    let (data_type, ident) = super::DataType::take(stream).unwrap();
    assert_eq!(data_type, super::DataType::Struct);
    assert_eq!(ident, "Foo");
    let body = StructBody::take(stream).unwrap();
    let fields = body.fields.as_ref().unwrap();

    assert_eq!(fields.len(), 3);
    let (ident, field) = fields.get(0).unwrap();
    assert_eq!(ident.unwrap(), "bar");
    assert_eq!(field.vis, Visibility::Pub);
    assert_eq!(field.type_string(), "u8");

    let (ident, field) = fields.get(1).unwrap();
    assert_eq!(ident.unwrap(), "baz");
    assert_eq!(field.vis, Visibility::Pub);
    assert_eq!(field.type_string(), "u32");

    let (ident, field) = fields.get(2).unwrap();
    assert_eq!(ident.unwrap(), "bla");
    assert_eq!(field.vis, Visibility::Default);
    assert_eq!(field.type_string(), "Vec<Box<dynFuture<Output=()>>>");

    let stream = &mut token_stream(
        "struct Foo ( pub u8, pub(crate) u32, Vec<Box<dyn Future<Output = ()>>> )",
    );
    let (data_type, ident) = super::DataType::take(stream).unwrap();
    assert_eq!(data_type, super::DataType::Struct);
    assert_eq!(ident, "Foo");
    let body = StructBody::take(stream).unwrap();
    let fields = body.fields.as_ref().unwrap();

    assert_eq!(fields.len(), 3);

    let (ident, field) = fields.get(0).unwrap();
    assert!(ident.is_none());
    assert_eq!(field.vis, Visibility::Pub);
    assert_eq!(field.type_string(), "u8");

    let (ident, field) = fields.get(1).unwrap();
    assert!(ident.is_none());
    assert_eq!(field.vis, Visibility::Pub);
    assert_eq!(field.type_string(), "u32");

    let (ident, field) = fields.get(2).unwrap();
    assert!(ident.is_none());
    assert_eq!(field.vis, Visibility::Default);
    assert_eq!(field.type_string(), "Vec<Box<dynFuture<Output=()>>>");

    let stream = &mut token_stream("struct Foo;");
    let (data_type, ident) = super::DataType::take(stream).unwrap();
    assert_eq!(data_type, super::DataType::Struct);
    assert_eq!(ident, "Foo");
    let body = StructBody::take(stream).unwrap();
    assert!(body.fields.is_none());

    let stream = &mut token_stream("struct Foo {}");
    let (data_type, ident) = super::DataType::take(stream).unwrap();
    assert_eq!(data_type, super::DataType::Struct);
    assert_eq!(ident, "Foo");
    let body = StructBody::take(stream).unwrap();
    if let Some(Fields::Struct(v)) = body.fields {
        assert!(v.is_empty());
    } else {
        panic!("wrong fields {:?}", body.fields);
    }

    let stream = &mut token_stream("struct Foo ()");
    let (data_type, ident) = super::DataType::take(stream).unwrap();
    assert_eq!(data_type, super::DataType::Struct);
    assert_eq!(ident, "Foo");
    let body = StructBody::take(stream).unwrap();
    if let Some(Fields::Tuple(v)) = body.fields {
        assert!(v.is_empty());
    } else {
        panic!("wrong fields {:?}", body.fields);
    }
}

#[test]
fn issue_77() {
    // https://github.com/bincode-org/virtue/issues/77
    use crate::token_stream;

    let stream = &mut token_stream("struct Test(pub [u8; 32])");
    let (data_type, ident) = super::DataType::take(stream).unwrap();
    assert_eq!(data_type, super::DataType::Struct);
    assert_eq!(ident, "Test");
    let body = StructBody::take(stream).unwrap();
    let fields = body.fields.unwrap();
    let Fields::Tuple(t) = fields else {
        panic!("Fields is not a tuple")
    };
    assert_eq!(t.len(), 1);
    assert_eq!(t[0].r#type[0].to_string(), "[u8 ; 32]");

    let stream = &mut token_stream("struct Foo(pub (u8, ))");
    let (data_type, ident) = super::DataType::take(stream).unwrap();
    assert_eq!(data_type, super::DataType::Struct);
    assert_eq!(ident, "Foo");
    let body = StructBody::take(stream).unwrap();
    let fields = body.fields.unwrap();
    let Fields::Tuple(t) = fields else {
        panic!("Fields is not a tuple")
    };
    assert_eq!(t.len(), 1);
    assert_eq!(t[0].r#type[0].to_string(), "(u8 ,)");
}

/// The body of an enum
#[derive(Debug)]
pub struct EnumBody {
    /// The enum's variants
    pub variants: Vec<EnumVariant>,
}

impl EnumBody {
    pub(crate) fn take(input: &mut Peekable<impl Iterator<Item = TokenTree>>) -> Result<Self> {
        match input.peek() {
            Some(TokenTree::Group(_)) => {}
            Some(TokenTree::Punct(p)) if p.as_char() == ';' => {
                return Ok(EnumBody {
                    variants: Vec::new(),
                })
            }
            token => return Error::wrong_token(token, "group or ;"),
        }
        let group = assume_group(input.next());
        let mut variants = Vec::new();
        let stream = &mut group.stream().into_iter().peekable();
        while stream.peek().is_some() {
            let attributes = Attribute::try_take(AttributeLocation::Variant, stream)?;
            let ident = match super::utils::consume_ident(stream) {
                Some(ident) => ident,
                None => Error::wrong_token(stream.peek(), "ident")?,
            };

            let mut fields = None;
            let mut value = None;

            if let Some(TokenTree::Group(_)) = stream.peek() {
                let group = assume_group(stream.next());
                let stream = &mut group.stream().into_iter().peekable();
                match group.delimiter() {
                    Delimiter::Brace => {
                        fields = Some(Fields::Struct(UnnamedField::parse_with_name(stream)?));
                    }
                    Delimiter::Parenthesis => {
                        fields = Some(Fields::Tuple(UnnamedField::parse(stream)?));
                    }
                    delim => {
                        return Err(Error::InvalidRustSyntax {
                            span: group.span(),
                            expected: format!("Brace or parenthesis, found {:?}", delim),
                        })
                    }
                }
            }
            match stream.peek() {
                Some(TokenTree::Punct(p)) if p.as_char() == '=' => {
                    assume_punct(stream.next(), '=');
                    match stream.next() {
                        Some(TokenTree::Literal(lit)) => {
                            value = Some(lit);
                        }
                        Some(TokenTree::Punct(p)) if p.as_char() == '-' => match stream.next() {
                            Some(TokenTree::Literal(lit)) => {
                                match lit.to_string().parse::<i64>() {
                                    Ok(val) => value = Some(Literal::i64_unsuffixed(-val)),
                                    Err(_) => {
                                        return Err(Error::custom_at(
                                            "parse::<i64> failed",
                                            lit.span(),
                                        ))
                                    }
                                };
                            }
                            token => return Error::wrong_token(token.as_ref(), "literal"),
                        },
                        token => return Error::wrong_token(token.as_ref(), "literal"),
                    }
                }
                Some(TokenTree::Punct(p)) if p.as_char() == ',' => {
                    // next field
                }
                None => {
                    // group done
                }
                token => return Error::wrong_token(token, "group, comma or ="),
            }

            consume_punct_if(stream, ',');

            variants.push(EnumVariant {
                name: ident,
                fields,
                value,
                attributes,
            });
        }

        Ok(EnumBody { variants })
    }
}

#[test]
fn test_enum_body_take() {
    use crate::token_stream;

    let stream = &mut token_stream("enum Foo { }");
    let (data_type, ident) = super::DataType::take(stream).unwrap();
    assert_eq!(data_type, super::DataType::Enum);
    assert_eq!(ident, "Foo");
    let body = EnumBody::take(stream).unwrap();
    assert!(body.variants.is_empty());

    let stream = &mut token_stream("enum Foo { Bar, Baz(u8), Blah { a: u32, b: u128 } }");
    let (data_type, ident) = super::DataType::take(stream).unwrap();
    assert_eq!(data_type, super::DataType::Enum);
    assert_eq!(ident, "Foo");
    let body = EnumBody::take(stream).unwrap();
    assert_eq!(3, body.variants.len());

    assert_eq!(body.variants[0].name, "Bar");
    assert!(body.variants[0].fields.is_none());

    assert_eq!(body.variants[1].name, "Baz");
    assert!(body.variants[1].fields.is_some());
    let fields = body.variants[1].fields.as_ref().unwrap();
    assert_eq!(1, fields.len());
    let (ident, field) = fields.get(0).unwrap();
    assert!(ident.is_none());
    assert_eq!(field.type_string(), "u8");

    assert_eq!(body.variants[2].name, "Blah");
    assert!(body.variants[2].fields.is_some());
    let fields = body.variants[2].fields.as_ref().unwrap();
    assert_eq!(2, fields.len());
    let (ident, field) = fields.get(0).unwrap();
    assert_eq!(ident.unwrap(), "a");
    assert_eq!(field.type_string(), "u32");
    let (ident, field) = fields.get(1).unwrap();
    assert_eq!(ident.unwrap(), "b");
    assert_eq!(field.type_string(), "u128");

    let stream = &mut token_stream("enum Foo { Bar = -1, Baz = 2 }");
    let (data_type, ident) = super::DataType::take(stream).unwrap();
    assert_eq!(data_type, super::DataType::Enum);
    assert_eq!(ident, "Foo");
    let body = EnumBody::take(stream).unwrap();
    assert_eq!(2, body.variants.len());

    assert_eq!(body.variants[0].name, "Bar");
    assert!(body.variants[0].fields.is_none());
    assert_eq!(body.variants[0].get_integer(), -1);

    assert_eq!(body.variants[1].name, "Baz");
    assert!(body.variants[1].fields.is_none());
    assert_eq!(body.variants[1].get_integer(), 2);

    let stream = &mut token_stream("enum Foo { Bar(i32) = -1, Baz { a: i32 } = 2 }");
    let (data_type, ident) = super::DataType::take(stream).unwrap();
    assert_eq!(data_type, super::DataType::Enum);
    assert_eq!(ident, "Foo");
    let body = EnumBody::take(stream).unwrap();
    assert_eq!(2, body.variants.len());

    assert_eq!(body.variants[0].name, "Bar");
    assert!(body.variants[0].fields.is_some());
    let fields = body.variants[0].fields.as_ref().unwrap();
    assert_eq!(fields.len(), 1);
    assert!(matches!(fields.names()[0], IdentOrIndex::Index { index, .. } if index == 0));
    assert_eq!(body.variants[0].get_integer(), -1);

    assert_eq!(body.variants[1].name, "Baz");
    assert!(body.variants[1].fields.is_some());
    let fields = body.variants[1].fields.as_ref().unwrap();
    assert_eq!(fields.len(), 1);
    assert_eq!(fields.names().len(), 1);
    assert!(matches!(&fields.names()[0], IdentOrIndex::Ident { ident, .. } if *ident == "a"));
    assert_eq!(body.variants[1].get_integer(), 2);

    let stream = &mut token_stream("enum Foo { Round(), Curly{}, Without }");
    let (data_type, ident) = super::DataType::take(stream).unwrap();
    assert_eq!(data_type, super::DataType::Enum);
    assert_eq!(ident, "Foo");
    let body = EnumBody::take(stream).unwrap();
    assert_eq!(3, body.variants.len());

    assert_eq!(body.variants[0].name, "Round");
    assert!(body.variants[0].fields.is_some());
    let fields = body.variants[0].fields.as_ref().unwrap();
    assert!(fields.names().is_empty());
    assert_eq!(fields.len(), 0);

    assert_eq!(body.variants[1].name, "Curly");
    assert!(body.variants[1].fields.is_some());
    let fields = body.variants[1].fields.as_ref().unwrap();
    assert!(fields.names().is_empty());
    assert_eq!(fields.len(), 0);

    assert_eq!(body.variants[2].name, "Without");
    assert!(body.variants[2].fields.is_none());
}

/// A variant of an enum
#[derive(Debug)]
pub struct EnumVariant {
    /// The name of the variant
    pub name: Ident,
    /// The field of the variant. See [`Fields`] for more info
    pub fields: Option<Fields>,
    /// The value of this variant. This can be one of:
    /// - `Baz = 5`
    /// - `Baz(i32) = 5`
    /// - `Baz { a: i32} = 5`
    ///
    /// In either case this value will be `Some(Literal::i32(5))`
    pub value: Option<Literal>,
    /// The attributes of this variant
    pub attributes: Vec<Attribute>,
}

#[cfg(test)]
impl EnumVariant {
    fn get_integer(&self) -> i64 {
        let value = self.value.as_ref().expect("Variant has no value");
        value
            .to_string()
            .parse()
            .expect("Value is not a valid integer")
    }
}

/// The different field types an enum variant can have.
#[derive(Debug)]
pub enum Fields {
    /// Tuple-like variant
    /// ```rs
    /// enum Foo {
    ///     Baz(u32)
    /// }
    /// struct Bar(u32);
    /// ```
    Tuple(Vec<UnnamedField>),

    /// Struct-like variant
    /// ```rs
    /// enum Foo {
    ///     Baz {
    ///         baz: u32
    ///     }
    /// }
    /// struct Bar {
    ///     baz: u32
    /// }
    /// ```
    Struct(Vec<(Ident, UnnamedField)>),
}

impl Fields {
    /// Returns a list of names for the variant.
    ///
    /// ```
    /// enum Foo {
    ///     C(u32, u32), // will return `vec[Index { index: 0 }, Index { index: 1 }]`
    ///     D { a: u32, b: u32 }, // will return `vec[Ident { ident: "a" }, Ident { ident: "b" }]`
    /// }
    pub fn names(&self) -> Vec<IdentOrIndex> {
        let result: Vec<IdentOrIndex> = match self {
            Self::Tuple(fields) => fields
                .iter()
                .enumerate()
                .map(|(index, field)| IdentOrIndex::Index {
                    index,
                    span: field.span(),
                    attributes: field.attributes.clone(),
                })
                .collect(),
            Self::Struct(fields) => fields
                .iter()
                .map(|(ident, field)| IdentOrIndex::Ident {
                    ident: ident.clone(),
                    attributes: field.attributes.clone(),
                })
                .collect(),
        };
        result
    }

    /// Return the delimiter of the group for this variant
    ///
    /// ```
    /// enum Foo {
    ///     C(u32, u32), // will return `Delimiter::Paranthesis`
    ///     D { a: u32, b: u32 }, // will return `Delimiter::Brace`
    /// }
    /// ```
    pub fn delimiter(&self) -> Delimiter {
        match self {
            Self::Tuple(_) => Delimiter::Parenthesis,
            Self::Struct(_) => Delimiter::Brace,
        }
    }
}

#[cfg(test)]
impl Fields {
    fn len(&self) -> usize {
        match self {
            Self::Tuple(fields) => fields.len(),
            Self::Struct(fields) => fields.len(),
        }
    }

    fn get(&self, index: usize) -> Option<(Option<&Ident>, &UnnamedField)> {
        match self {
            Self::Tuple(fields) => fields.get(index).map(|f| (None, f)),
            Self::Struct(fields) => fields.get(index).map(|(ident, field)| (Some(ident), field)),
        }
    }
}

/// An unnamed field
#[derive(Debug)]
pub struct UnnamedField {
    /// The visibility of the field
    pub vis: Visibility,
    /// The type of the field
    pub r#type: Vec<TokenTree>,
    /// The attributes of the field
    pub attributes: Vec<Attribute>,
}

impl UnnamedField {
    pub(crate) fn parse_with_name(
        input: &mut Peekable<impl Iterator<Item = TokenTree>>,
    ) -> Result<Vec<(Ident, Self)>> {
        let mut result = Vec::new();
        loop {
            let attributes = Attribute::try_take(AttributeLocation::Field, input)?;
            let vis = Visibility::try_take(input)?;

            let ident = match input.peek() {
                Some(TokenTree::Ident(_)) => assume_ident(input.next()),
                Some(x) => {
                    return Err(Error::InvalidRustSyntax {
                        span: x.span(),
                        expected: format!("ident or end of group, got {:?}", x),
                    })
                }
                None => break,
            };
            match input.peek() {
                Some(TokenTree::Punct(p)) if p.as_char() == ':' => {
                    input.next();
                }
                token => return Error::wrong_token(token, ":"),
            }
            let r#type = read_tokens_until_punct(input, &[','])?;
            consume_punct_if(input, ',');
            result.push((
                ident,
                Self {
                    vis,
                    r#type,
                    attributes,
                },
            ));
        }
        Ok(result)
    }

    pub(crate) fn parse(
        input: &mut Peekable<impl Iterator<Item = TokenTree>>,
    ) -> Result<Vec<Self>> {
        let mut result = Vec::new();
        while input.peek().is_some() {
            let attributes = Attribute::try_take(AttributeLocation::Field, input)?;
            let vis = Visibility::try_take(input)?;

            let r#type = read_tokens_until_punct(input, &[','])?;
            consume_punct_if(input, ',');
            result.push(Self {
                vis,
                r#type,
                attributes,
            });
        }
        Ok(result)
    }

    /// Return [`type`] as a string. Useful for comparing it for known values.
    ///
    /// [`type`]: #structfield.type
    pub fn type_string(&self) -> String {
        self.r#type.iter().map(|t| t.to_string()).collect()
    }

    /// Return the span of [`type`].
    ///
    /// **note**: Until <https://github.com/rust-lang/rust/issues/54725> is stable, this will return the first span of the type instead
    ///
    /// [`type`]: #structfield.type
    pub fn span(&self) -> Span {
        // BlockedTODO: https://github.com/rust-lang/rust/issues/54725
        // Span::join is unstable
        // if let Some(first) = self.r#type.first() {
        //     let mut span = first.span();
        //     for token in self.r#type.iter().skip(1) {
        //         span = span.join(span).unwrap();
        //     }
        //     span
        // } else {
        //     Span::call_site()
        // }

        match self.r#type.first() {
            Some(first) => first.span(),
            None => Span::call_site(),
        }
    }
}

/// Reference to an enum variant's field. Either by index or by ident.
///
/// ```
/// enum Foo {
///     Bar(u32), // will be IdentOrIndex::Index { index: 0, .. }
///     Baz {
///         a: u32, // will be IdentOrIndex::Ident { ident: "a", .. }
///     },
/// }
#[derive(Debug, Clone)]
pub enum IdentOrIndex {
    /// The variant is a named field
    Ident {
        /// The name of the field
        ident: Ident,
        /// The attributes of the field
        attributes: Vec<Attribute>,
    },
    /// The variant is an unnamed field
    Index {
        /// The field index
        index: usize,
        /// The span of the field type
        span: Span,
        /// The attributes of this field
        attributes: Vec<Attribute>,
    },
}

impl IdentOrIndex {
    /// Get the ident. Will panic if this is an `IdentOrIndex::Index`
    pub fn unwrap_ident(&self) -> Ident {
        match self {
            Self::Ident { ident, .. } => ident.clone(),
            x => panic!("Expected ident, found {:?}", x),
        }
    }

    /// Convert this ident into a TokenTree. If this is an `Index`, will return `prefix + index` instead.
    pub fn to_token_tree_with_prefix(&self, prefix: &str) -> TokenTree {
        TokenTree::Ident(match self {
            IdentOrIndex::Ident { ident, .. } => (*ident).clone(),
            IdentOrIndex::Index { index, span, .. } => {
                let name = format!("{}{}", prefix, index);
                Ident::new(&name, *span)
            }
        })
    }

    /// Return either the index or the ident of this field with a fixed prefix. The prefix will always be added.
    pub fn to_string_with_prefix(&self, prefix: &str) -> String {
        match self {
            IdentOrIndex::Ident { ident, .. } => ident.to_string(),
            IdentOrIndex::Index { index, .. } => {
                format!("{}{}", prefix, index)
            }
        }
    }

    /// Returns the attributes of this field.
    pub fn attributes(&self) -> &Vec<Attribute> {
        match self {
            Self::Ident { attributes, .. } => attributes,
            Self::Index { attributes, .. } => attributes,
        }
    }
}

impl std::fmt::Display for IdentOrIndex {
    fn fmt(&self, fmt: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            IdentOrIndex::Ident { ident, .. } => write!(fmt, "{}", ident),
            IdentOrIndex::Index { index, .. } => write!(fmt, "{}", index),
        }
    }
}

#[test]
fn enum_explicit_variants() {
    use crate::token_stream;
    let stream = &mut token_stream("{ A = 1, B = 2 }");
    let body = EnumBody::take(stream).unwrap();
    assert_eq!(body.variants.len(), 2);
}
