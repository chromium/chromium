use crate::attr::Attribute;
use crate::expr::Expr;
use crate::ident::Ident;
use crate::punctuated::{self, Punctuated};
use crate::restriction::{FieldMutability, Visibility};
use crate::token;
use crate::ty::Type;

ast_struct! {
    /// An enum variant.
    #[cfg_attr(doc_cfg, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct Variant {
        pub attrs: Vec<Attribute>,

        /// Name of the variant.
        pub ident: Ident,

        /// Content stored in the variant.
        pub fields: Fields,

        /// Explicit discriminant: `Variant = 1`
        pub discriminant: Option<(Token![=], Expr)>,
    }
}

ast_enum_of_structs! {
    /// Data stored within an enum variant or struct.
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: crate::expr::Expr#syntax-tree-enums
    #[cfg_attr(doc_cfg, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub enum Fields {
        /// Named fields of a struct or struct variant such as `Point { x: f64,
        /// y: f64 }`.
        Named(FieldsNamed),

        /// Unnamed fields of a tuple struct or tuple variant such as `Some(T)`.
        Unnamed(FieldsUnnamed),

        /// Unit struct or unit variant such as `None`.
        Unit,
    }
}

ast_struct! {
    /// Named fields of a struct or struct variant such as `Point { x: f64,
    /// y: f64 }`.
    #[cfg_attr(doc_cfg, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct FieldsNamed {
        pub brace_token: token::Brace,
        pub named: Punctuated<Field, Token![,]>,
    }
}

ast_struct! {
    /// Unnamed fields of a tuple struct or tuple variant such as `Some(T)`.
    #[cfg_attr(doc_cfg, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct FieldsUnnamed {
        pub paren_token: token::Paren,
        pub unnamed: Punctuated<Field, Token![,]>,
    }
}

impl Fields {
    /// Get an iterator over the borrowed [`Field`] items in this object. This
    /// iterator can be used to iterate over a named or unnamed struct or
    /// variant's fields uniformly.
    pub fn iter(&self) -> punctuated::Iter<Field> {
        match self {
            Fields::Unit => crate::punctuated::empty_punctuated_iter(),
            Fields::Named(f) => f.named.iter(),
            Fields::Unnamed(f) => f.unnamed.iter(),
        }
    }

    /// Get an iterator over the mutably borrowed [`Field`] items in this
    /// object. This iterator can be used to iterate over a named or unnamed
    /// struct or variant's fields uniformly.
    pub fn iter_mut(&mut self) -> punctuated::IterMut<Field> {
        match self {
            Fields::Unit => crate::punctuated::empty_punctuated_iter_mut(),
            Fields::Named(f) => f.named.iter_mut(),
            Fields::Unnamed(f) => f.unnamed.iter_mut(),
        }
    }

    /// Returns the number of fields.
    pub fn len(&self) -> usize {
        match self {
            Fields::Unit => 0,
            Fields::Named(f) => f.named.len(),
            Fields::Unnamed(f) => f.unnamed.len(),
        }
    }

    /// Returns `true` if there are zero fields.
    pub fn is_empty(&self) -> bool {
        match self {
            Fields::Unit => true,
            Fields::Named(f) => f.named.is_empty(),
            Fields::Unnamed(f) => f.unnamed.is_empty(),
        }
    }
}

impl IntoIterator for Fields {
    type Item = Field;
    type IntoIter = punctuated::IntoIter<Field>;

    fn into_iter(self) -> Self::IntoIter {
        match self {
            Fields::Unit => Punctuated::<Field, ()>::new().into_iter(),
            Fields::Named(f) => f.named.into_iter(),
            Fields::Unnamed(f) => f.unnamed.into_iter(),
        }
    }
}

impl<'a> IntoIterator for &'a Fields {
    type Item = &'a Field;
    type IntoIter = punctuated::Iter<'a, Field>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

impl<'a> IntoIterator for &'a mut Fields {
    type Item = &'a mut Field;
    type IntoIter = punctuated::IterMut<'a, Field>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter_mut()
    }
}

ast_struct! {
    /// A field of a struct or enum variant.
    #[cfg_attr(doc_cfg, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct Field {
        pub attrs: Vec<Attribute>,

        pub vis: Visibility,

        pub mutability: FieldMutability,

        /// Name of the field, if any.
        ///
        /// Fields of tuple structs have no names.
        pub ident: Option<Ident>,

        pub colon_token: Option<Token![:]>,

        pub ty: Type,
    }
}

#[cfg(feature = "parsing")]
pub(crate) mod parsing {
    use crate::attr::Attribute;
    use crate::data::{Field, Fields, FieldsNamed, FieldsUnnamed, Variant};
    use crate::error::Result;
    use crate::expr::Expr;
    use crate::ext::IdentExt as _;
    use crate::ident::Ident;
    #[cfg(not(feature = "full"))]
    use crate::parse::discouraged::Speculative as _;
    use crate::parse::{Parse, ParseStream};
    use crate::restriction::{FieldMutability, Visibility};
    use crate::token;
    use crate::ty::Type;
    use crate::verbatim;

    #[cfg_attr(doc_cfg, doc(cfg(feature = "parsing")))]
    impl Parse for Variant {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let _visibility: Visibility = input.parse()?;
            let ident: Ident = input.parse()?;
            let fields = if input.peek(token::Brace) {
                Fields::Named(input.parse()?)
            } else if input.peek(token::Paren) {
                Fields::Unnamed(input.parse()?)
            } else {
                Fields::Unit
            };
            let discriminant = if input.peek(Token![=]) {
                let eq_token: Token![=] = input.parse()?;
                #[cfg(feature = "full")]
                let discriminant: Expr = input.parse()?;
                #[cfg(not(feature = "full"))]
                let discriminant = {
                    let begin = input.fork();
                    let ahead = input.fork();
                    let mut discriminant: Result<Expr> = ahead.parse();
                    if discriminant.is_ok() {
                        input.advance_to(&ahead);
                    } else if scan_lenient_discriminant(input).is_ok() {
                        discriminant = Ok(Expr::Verbatim(verbatim::between(&begin, input)));
                    }
                    discriminant?
                };
                Some((eq_token, discriminant))
            } else {
                None
            };
            Ok(Variant {
                attrs,
                ident,
                fields,
                discriminant,
            })
        }
    }

    #[cfg(not(feature = "full"))]
    pub(crate) fn scan_lenient_discriminant(input: ParseStream) -> Result<()> {
        use crate::expr::Member;
        use crate::lifetime::Lifetime;
        use crate::lit::Lit;
        use crate::lit::LitFloat;
        use crate::op::{BinOp, UnOp};
        use crate::path::{self, AngleBracketedGenericArguments};
        use proc_macro2::Delimiter::{self, Brace, Bracket, Parenthesis};

        let consume = |delimiter: Delimiter| {
            Result::unwrap(input.step(|cursor| match cursor.group(delimiter) {
                Some((_inside, _span, rest)) => Ok((true, rest)),
                None => Ok((false, *cursor)),
            }))
        };

        macro_rules! consume {
            [$token:tt] => {
                input.parse::<Option<Token![$token]>>().unwrap().is_some()
            };
        }

        let mut initial = true;
        let mut depth = 0usize;
        loop {
            if initial {
                if consume![&] {
                    input.parse::<Option<Token![mut]>>()?;
                } else if consume![if] || consume![match] || consume![while] {
                    depth += 1;
                } else if input.parse::<Option<Lit>>()?.is_some()
                    || (consume(Brace) || consume(Bracket) || consume(Parenthesis))
                    || (consume![async] || consume![const] || consume![loop] || consume![unsafe])
                        && (consume(Brace) || break)
                {
                    initial = false;
                } else if consume![let] {
                    while !consume![=] {
                        if !((consume![|] || consume![ref] || consume![mut] || consume![@])
                            || (consume![!] || input.parse::<Option<Lit>>()?.is_some())
                            || (consume![..=] || consume![..] || consume![&] || consume![_])
                            || (consume(Brace) || consume(Bracket) || consume(Parenthesis)))
                        {
                            path::parsing::qpath(input, true)?;
                        }
                    }
                } else if input.parse::<Option<Lifetime>>()?.is_some() && !consume![:] {
                    break;
                } else if input.parse::<UnOp>().is_err() {
                    path::parsing::qpath(input, true)?;
                    initial = consume![!] || depth == 0 && input.peek(token::Brace);
                }
            } else if input.is_empty() || input.peek(Token![,]) {
                return Ok(());
            } else if depth > 0 && consume(Brace) {
                if consume![else] && !consume(Brace) {
                    initial = consume![if] || break;
                } else {
                    depth -= 1;
                }
            } else if input.parse::<BinOp>().is_ok() || (consume![..] | consume![=]) {
                initial = true;
            } else if consume![.] {
                if input.parse::<Option<LitFloat>>()?.is_none()
                    && (input.parse::<Member>()?.is_named() && consume![::])
                {
                    AngleBracketedGenericArguments::do_parse(None, input)?;
                }
            } else if consume![as] {
                input.parse::<Type>()?;
            } else if !(consume(Brace) || consume(Bracket) || consume(Parenthesis)) {
                break;
            }
        }

        Err(input.error("unsupported expression"))
    }

    #[cfg_attr(doc_cfg, doc(cfg(feature = "parsing")))]
    impl Parse for FieldsNamed {
        fn parse(input: ParseStream) -> Result<Self> {
            let content;
            Ok(FieldsNamed {
                brace_token: braced!(content in input),
                named: content.parse_terminated(Field::parse_named, Token![,])?,
            })
        }
    }

    #[cfg_attr(doc_cfg, doc(cfg(feature = "parsing")))]
    impl Parse for FieldsUnnamed {
        fn parse(input: ParseStream) -> Result<Self> {
            let content;
            Ok(FieldsUnnamed {
                paren_token: parenthesized!(content in input),
                unnamed: content.parse_terminated(Field::parse_unnamed, Token![,])?,
            })
        }
    }

    impl Field {
        /// Parses a named (braced struct) field.
        #[cfg_attr(doc_cfg, doc(cfg(feature = "parsing")))]
        pub fn parse_named(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let vis: Visibility = input.parse()?;

            let unnamed_field = cfg!(feature = "full") && input.peek(Token![_]);
            let ident = if unnamed_field {
                input.call(Ident::parse_any)
            } else {
                input.parse()
            }?;

            let colon_token: Token![:] = input.parse()?;

            let ty: Type = if unnamed_field
                && (input.peek(Token![struct])
                    || input.peek(Token![union]) && input.peek2(token::Brace))
            {
                let begin = input.fork();
                input.call(Ident::parse_any)?;
                input.parse::<FieldsNamed>()?;
                Type::Verbatim(verbatim::between(&begin, input))
            } else {
                input.parse()?
            };

            Ok(Field {
                attrs,
                vis,
                mutability: FieldMutability::None,
                ident: Some(ident),
                colon_token: Some(colon_token),
                ty,
            })
        }

        /// Parses an unnamed (tuple struct) field.
        #[cfg_attr(doc_cfg, doc(cfg(feature = "parsing")))]
        pub fn parse_unnamed(input: ParseStream) -> Result<Self> {
            Ok(Field {
                attrs: input.call(Attribute::parse_outer)?,
                vis: input.parse()?,
                mutability: FieldMutability::None,
                ident: None,
                colon_token: None,
                ty: input.parse()?,
            })
        }
    }
}

#[cfg(feature = "printing")]
mod printing {
    use crate::data::{Field, FieldsNamed, FieldsUnnamed, Variant};
    use crate::print::TokensOrDefault;
    use proc_macro2::TokenStream;
    use quote::{ToTokens, TokenStreamExt};

    #[cfg_attr(doc_cfg, doc(cfg(feature = "printing")))]
    impl ToTokens for Variant {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(&self.attrs);
            self.ident.to_tokens(tokens);
            self.fields.to_tokens(tokens);
            if let Some((eq_token, disc)) = &self.discriminant {
                eq_token.to_tokens(tokens);
                disc.to_tokens(tokens);
            }
        }
    }

    #[cfg_attr(doc_cfg, doc(cfg(feature = "printing")))]
    impl ToTokens for FieldsNamed {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.brace_token.surround(tokens, |tokens| {
                self.named.to_tokens(tokens);
            });
        }
    }

    #[cfg_attr(doc_cfg, doc(cfg(feature = "printing")))]
    impl ToTokens for FieldsUnnamed {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.paren_token.surround(tokens, |tokens| {
                self.unnamed.to_tokens(tokens);
            });
        }
    }

    #[cfg_attr(doc_cfg, doc(cfg(feature = "printing")))]
    impl ToTokens for Field {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(&self.attrs);
            self.vis.to_tokens(tokens);
            if let Some(ident) = &self.ident {
                ident.to_tokens(tokens);
                TokensOrDefault(&self.colon_token).to_tokens(tokens);
            }
            self.ty.to_tokens(tokens);
        }
    }
}
