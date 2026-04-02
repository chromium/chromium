//! Module for parsing code. The main enum is [`Parse`].

use crate::prelude::*;

mod attributes;
mod body;
mod data_type;
mod generics;
mod utils;
mod visibility;

pub use self::attributes::{Attribute, AttributeAccess, AttributeLocation, FromAttribute};
pub use self::body::{EnumBody, EnumVariant, Fields, IdentOrIndex, StructBody, UnnamedField};
pub(crate) use self::data_type::DataType;
pub use self::generics::{
    ConstGeneric, Generic, GenericConstraints, Generics, Lifetime, SimpleGeneric,
};
pub use self::visibility::Visibility;

use crate::generate::Generator;

/// Parser for Enum and Struct derives.
///
/// You can generate this enum by calling
///
/// ```ignore
/// use virtue::prelude::*;
///
/// #[proc_macro_derive(YourDerive)]
/// pub fn derive_your_derive(input: TokenStream) -> TokenStream {
///     let parse = Parse::new(input).unwrap();
///     // rest
/// # unimplemented!()
/// }
/// ```
#[non_exhaustive]
pub enum Parse {
    /// The given input is a struct
    Struct {
        /// The attributes of the struct
        attributes: Vec<Attribute>,
        /// The visibility of the struct
        visibility: Visibility,
        /// The name of the struct
        name: Ident,
        /// The generics of the struct, e.g. `struct Foo<F> { ... }` will be `F`
        generics: Option<Generics>,
        /// The generic constraits of the struct, e.g. `struct Foo<F> { ... } where F: Display` will be `F: Display`
        generic_constraints: Option<GenericConstraints>,
        /// The body of the struct
        body: StructBody,
    },
    /// The given input is an enum
    Enum {
        /// The attributes of the enum
        attributes: Vec<Attribute>,
        /// The visibility of the enum
        visibility: Visibility,
        /// The name of the enum
        name: Ident,
        /// The generics of the enum, e.g. `enum Foo<F> { ... }` will be `F`
        generics: Option<Generics>,
        /// The generic constraits of the enum, e.g. `enum Foo<F> { ... } where F: Display` will be `F: Display`
        generic_constraints: Option<GenericConstraints>,
        /// The body of the enum
        body: EnumBody,
    },
}

impl Parse {
    /// Parse the given [`TokenStream`] and return the result.
    pub fn new(input: TokenStream) -> Result<Self> {
        let source = &mut input.into_iter().peekable();

        let attributes = Attribute::try_take(AttributeLocation::Container, source)?;
        let visibility = Visibility::try_take(source)?;
        let (datatype, name) = DataType::take(source)?;
        let generics = Generics::try_take(source)?;
        let generic_constraints = GenericConstraints::try_take(source)?;
        match datatype {
            DataType::Struct => {
                let body = StructBody::take(source)?;
                Ok(Self::Struct {
                    attributes,
                    visibility,
                    name,
                    generics,
                    generic_constraints,
                    body,
                })
            }
            DataType::Enum => {
                let body = EnumBody::take(source)?;
                Ok(Self::Enum {
                    attributes,
                    visibility,
                    name,
                    generics,
                    generic_constraints,
                    body,
                })
            }
        }
    }

    /// Split this struct or enum into a [`Generator`], list of [`Attribute`] and [`Body`].
    pub fn into_generator(self) -> (Generator, Vec<Attribute>, Body) {
        match self {
            Parse::Struct {
                name,
                generics,
                generic_constraints,
                body,
                attributes,
                ..
            } => (
                Generator::new(name, generics, generic_constraints),
                attributes,
                Body::Struct(body),
            ),
            Parse::Enum {
                name,
                generics,
                generic_constraints,
                body,
                attributes,
                ..
            } => (
                Generator::new(name, generics, generic_constraints),
                attributes,
                Body::Enum(body),
            ),
        }
    }
}

/// The body of the enum or struct
#[allow(missing_docs)]
pub enum Body {
    Struct(StructBody),
    Enum(EnumBody),
}
