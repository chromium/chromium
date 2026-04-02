//! Code to help generate functions.
//!
//! The structure is:
//!
//! - [`Generator`]
//!   - `.impl_for()`: [`ImplFor`]
//!     - `.generate_fn()`: [`FnBuilder`]
//!       - `.body(|builder| { .. })`: [`StreamBuilder`]
//!
//! Afterwards, [`Generator::finish()`] **must** be called to take out the [`TokenStream`] produced.
//!
//! [`Generator::finish()`]: struct.Generator.html#method.finish
//! [`TokenStream`]: ../prelude/struct.TokenStream.html

mod gen_enum;
mod gen_struct;
mod generate_item;
mod generate_mod;
mod generator;
mod r#impl;
mod impl_for;
mod stream_builder;

use crate::parse::Visibility;
use crate::{
    parse::{GenericConstraints, Generics},
    prelude::{Delimiter, Ident, TokenStream},
};
use std::fmt;
use std::marker::PhantomData;

pub use self::gen_enum::GenEnum;
pub use self::gen_struct::GenStruct;
pub use self::generate_item::{FnBuilder, FnSelfArg, GenConst};
pub use self::generate_mod::GenerateMod;
pub use self::generator::Generator;
pub use self::impl_for::ImplFor;
pub use self::r#impl::Impl;
pub use self::stream_builder::{PushParseError, StreamBuilder};

/// Helper trait to make it possible to nest several builders. Internal use only.
#[allow(missing_docs)]
pub trait Parent {
    fn append(&mut self, builder: StreamBuilder);
    fn name(&self) -> &Ident;
    fn generics(&self) -> Option<&Generics>;
    fn generic_constraints(&self) -> Option<&GenericConstraints>;
}

/// Helper enum to differentiate between a [`Ident`] or a [`String`].
#[allow(missing_docs)]
pub enum StringOrIdent {
    String(String),
    // Note that when this is a `string` this could be much more than a single ident.
    // Therefor you should never use [`StreamBuilder`]`.ident_str(StringOrIdent.to_string())`, but instead use `.push_parsed(StringOrIdent.to_string())?`.
    Ident(Ident),
}

impl fmt::Display for StringOrIdent {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::String(s) => s.fmt(f),
            Self::Ident(i) => i.fmt(f),
        }
    }
}

impl From<String> for StringOrIdent {
    fn from(s: String) -> Self {
        Self::String(s)
    }
}
impl From<Ident> for StringOrIdent {
    fn from(i: Ident) -> Self {
        Self::Ident(i)
    }
}
impl<'a> From<&'a str> for StringOrIdent {
    fn from(s: &'a str) -> Self {
        Self::String(s.to_owned())
    }
}

/// A path of identifiers, like `mod::Type`.
pub struct Path(Vec<StringOrIdent>);

impl From<String> for Path {
    fn from(s: String) -> Self {
        StringOrIdent::from(s).into()
    }
}

impl From<Ident> for Path {
    fn from(i: Ident) -> Self {
        StringOrIdent::from(i).into()
    }
}

impl From<&str> for Path {
    fn from(s: &str) -> Self {
        StringOrIdent::from(s).into()
    }
}

impl From<StringOrIdent> for Path {
    fn from(value: StringOrIdent) -> Self {
        Self(vec![value])
    }
}

impl FromIterator<String> for Path {
    fn from_iter<T: IntoIterator<Item = String>>(iter: T) -> Self {
        iter.into_iter().map(StringOrIdent::from).collect()
    }
}

impl FromIterator<Ident> for Path {
    fn from_iter<T: IntoIterator<Item = Ident>>(iter: T) -> Self {
        iter.into_iter().map(StringOrIdent::from).collect()
    }
}

impl<'a> FromIterator<&'a str> for Path {
    fn from_iter<T: IntoIterator<Item = &'a str>>(iter: T) -> Self {
        iter.into_iter().map(StringOrIdent::from).collect()
    }
}

impl FromIterator<StringOrIdent> for Path {
    fn from_iter<T: IntoIterator<Item = StringOrIdent>>(iter: T) -> Self {
        Self(iter.into_iter().collect())
    }
}

impl IntoIterator for Path {
    type Item = StringOrIdent;
    type IntoIter = std::vec::IntoIter<StringOrIdent>;

    fn into_iter(self) -> Self::IntoIter {
        self.0.into_iter()
    }
}

/// A struct or enum variant field.
struct Field {
    name: String,
    vis: Visibility,
    ty: String,
    attributes: Vec<StreamBuilder>,
}

impl Field {
    fn new(name: impl Into<String>, vis: Visibility, ty: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            vis,
            ty: ty.into(),
            attributes: Vec::new(),
        }
    }
}

/// A builder for struct or enum variant fields.
pub struct FieldBuilder<'a, P> {
    fields: &'a mut Vec<Field>,
    _parent: PhantomData<P>, // Keep this to disallow `pub` on enum fields
}

impl<P> FieldBuilder<'_, P> {
    /// Add an attribute to the field.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Fooz");
    /// generator
    ///     .generate_struct("Foo")
    ///     .add_field("foo", "u16")
    ///     .make_pub()
    ///     .with_attribute("serde", |b| {
    ///         b.push_parsed("(default)")?;
    ///         Ok(())
    ///     })?;
    /// generator
    ///     .generate_enum("Bar")
    ///     .add_value("Baz")
    ///     .add_field("baz", "bool")
    ///     .with_attribute("serde", |b| {
    ///         b.push_parsed("(default)")?;
    ///         Ok(())
    ///     })?;
    /// # generator.assert_eq("struct Foo { # [serde (default)] pub foo : u16 , } \
    /// enum Bar { Baz { # [serde (default)] baz : bool , } , }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```ignore
    /// struct Foo {
    ///     #[serde(default)]
    ///     pub bar: u16
    /// }
    ///
    /// enum Bar {
    ///     Baz {
    ///         #[serde(default)]
    ///         baz: bool
    ///     }
    /// }
    /// ```
    pub fn with_attribute(
        &mut self,
        name: impl AsRef<str>,
        value: impl FnOnce(&mut StreamBuilder) -> crate::Result,
    ) -> crate::Result<&mut Self> {
        self.current().with_attribute(name, value)?;
        Ok(self)
    }

    /// Add a parsed attribute to the field.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Fooz");
    /// generator
    ///     .generate_struct("Foo")
    ///     .add_field("foo", "u16")
    ///     .make_pub()
    ///     .with_parsed_attribute("serde(default)")?;
    /// generator
    ///     .generate_enum("Bar")
    ///     .add_value("Baz")
    ///     .add_field("baz", "bool")
    ///     .with_parsed_attribute("serde(default)")?;
    /// # generator.assert_eq("struct Foo { # [serde (default)] pub foo : u16 , } \
    /// enum Bar { Baz { # [serde (default)] baz : bool , } , }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```ignore
    /// struct Foo {
    ///     #[serde(default)]
    ///     pub bar: u16
    /// }
    ///
    /// enum Bar {
    ///     Baz {
    ///         #[serde(default)]
    ///         baz: bool
    ///     }
    /// }
    /// ```
    pub fn with_parsed_attribute(
        &mut self,
        attribute: impl AsRef<str>,
    ) -> crate::Result<&mut Self> {
        self.current().with_parsed_attribute(attribute)?;
        Ok(self)
    }

    /// Add a token stream as an attribute to the field.
    ///
    /// ```
    /// # use virtue::prelude::{Generator, TokenStream};
    /// # let mut generator = Generator::with_name("Fooz");
    /// let attribute = "serde(default)".parse::<TokenStream>().unwrap();
    /// generator
    ///     .generate_struct("Foo")
    ///     .add_field("foo", "u16")
    ///     .make_pub()
    ///     .with_attribute_stream(attribute);
    /// # generator.assert_eq("struct Foo { # [serde (default)] pub foo : u16 , }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```ignore
    /// struct Foo {
    ///     #[serde(default)]
    ///     pub bar: u16
    /// }
    /// ```
    pub fn with_attribute_stream(&mut self, attribute: impl Into<TokenStream>) -> &mut Self {
        self.current().with_attribute_stream(attribute);
        self
    }

    /// Add a field to the parent type.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Fooz");
    /// generator
    ///     .generate_struct("Foo")
    ///     .add_field("foo", "u16")
    ///     .add_field("bar", "bool");
    /// # generator.assert_eq("struct Foo { foo : u16 , bar : bool , }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```
    /// struct Foo {
    ///     foo: u16,
    ///     bar: bool
    /// }
    /// ```
    pub fn add_field(&mut self, name: impl Into<String>, ty: impl Into<String>) -> &mut Self {
        self.fields.push(Field::new(name, Visibility::Default, ty));
        self
    }
}

// Only allow `pub` on struct fields
impl<'a, P: Parent> FieldBuilder<'_, GenStruct<'a, P>> {
    /// Make the field public.
    pub fn make_pub(&mut self) -> &mut Self {
        self.current().vis = Visibility::Pub;
        self
    }
}

impl<'a, P> From<&'a mut Vec<Field>> for FieldBuilder<'a, P> {
    fn from(fields: &'a mut Vec<Field>) -> Self {
        Self {
            fields,
            _parent: PhantomData,
        }
    }
}

impl<P> FieldBuilder<'_, P> {
    fn current(&mut self) -> &mut Field {
        // A field is always added before this is called, so the unwrap doesn't fail.
        self.fields.last_mut().unwrap()
    }
}

/// A helper trait to share attribute code between struct and enum generators.
trait AttributeContainer {
    fn derives(&mut self) -> &mut Vec<Path>;
    fn attributes(&mut self) -> &mut Vec<StreamBuilder>;

    fn with_derive(&mut self, derive: impl Into<Path>) -> &mut Self {
        self.derives().push(derive.into());
        self
    }

    fn with_derives<T: Into<Path>>(&mut self, derives: impl IntoIterator<Item = T>) -> &mut Self {
        self.derives().extend(derives.into_iter().map(Into::into));
        self
    }

    fn with_attribute(
        &mut self,
        name: impl AsRef<str>,
        value: impl FnOnce(&mut StreamBuilder) -> crate::Result,
    ) -> crate::Result<&mut Self> {
        let mut stream = StreamBuilder::new();
        value(stream.ident_str(name))?;
        self.attributes().push(stream);
        Ok(self)
    }

    fn with_parsed_attribute(&mut self, attribute: impl AsRef<str>) -> crate::Result<&mut Self> {
        let mut stream = StreamBuilder::new();
        stream.push_parsed(attribute)?;
        self.attributes().push(stream);
        Ok(self)
    }

    fn with_attribute_stream(&mut self, attribute: impl Into<TokenStream>) -> &mut Self {
        let stream = StreamBuilder {
            stream: attribute.into(),
        };
        self.attributes().push(stream);
        self
    }

    fn build_derives(&mut self, b: &mut StreamBuilder) -> &mut Self {
        let derives = std::mem::take(self.derives());
        if !derives.is_empty() {
            build_attribute(b, |b| {
                b.ident_str("derive").group(Delimiter::Parenthesis, |b| {
                    for (idx, derive) in derives.into_iter().enumerate() {
                        if idx > 0 {
                            b.punct(',');
                        }
                        for (idx, component) in derive.into_iter().enumerate() {
                            if idx > 0 {
                                b.puncts("::");
                            }

                            match component {
                                StringOrIdent::String(s) => b.ident_str(s),
                                StringOrIdent::Ident(i) => b.ident(i),
                            };
                        }
                    }
                    Ok(())
                })
            })
            .expect("could not build derives");
        }
        self
    }

    fn build_attributes(&mut self, b: &mut StreamBuilder) -> &mut Self {
        for attr in std::mem::take(self.attributes()) {
            build_attribute(b, |b| Ok(b.extend(attr.stream))).expect("could not build attribute");
        }
        self
    }
}

impl AttributeContainer for Field {
    fn derives(&mut self) -> &mut Vec<Path> {
        unreachable!("fields cannot have derives")
    }

    fn attributes(&mut self) -> &mut Vec<StreamBuilder> {
        &mut self.attributes
    }
}

fn build_attribute<T>(b: &mut StreamBuilder, build: T) -> crate::Result
where
    T: FnOnce(&mut StreamBuilder) -> crate::Result<&mut StreamBuilder>,
{
    b.punct('#').group(Delimiter::Bracket, |b| {
        build(b)?;
        Ok(())
    })?;

    Ok(())
}
