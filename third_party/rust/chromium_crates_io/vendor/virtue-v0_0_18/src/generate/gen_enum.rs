use super::{
    AttributeContainer, Field, FieldBuilder, Impl, ImplFor, Parent, Path, StreamBuilder,
    StringOrIdent,
};
use crate::parse::{Generic, Generics, Visibility};
use crate::prelude::{Delimiter, Ident, Span, TokenStream};
use crate::Result;

/// Builder to generate an `enum <Name> { <value> { ... }, ... }`
///
/// ```
/// # use virtue::prelude::Generator;
/// # let mut generator = Generator::with_name("Fooz");
/// {
///     let mut enumgen = generator.generate_enum("Foo");
///     enumgen
///         .add_value("ZST")
///         .make_zst();
///     enumgen
///         .add_value("Named")
///         .add_field("bar", "u16")
///         .add_field("baz", "String");
///     enumgen
///         .add_value("Unnamed")
///         .make_tuple()
///         .add_field("", "u16")
///         .add_field("baz", "String");
/// }
/// # generator.assert_eq("enum Foo { ZST , Named { bar : u16 , baz : String , } , Unnamed (u16 , String ,) , }");
/// # Ok::<_, virtue::Error>(())
/// ```
///
/// Generates:
/// ```
/// enum Foo {
///     ZST,
///     Named {
///         bar: u16,
///         baz: String,
///     },
///     Unnamed(u16, String),
/// };
/// ```
pub struct GenEnum<'a, P: Parent> {
    parent: &'a mut P,
    name: Ident,
    visibility: Visibility,
    generics: Option<Generics>,
    values: Vec<EnumValue>,
    derives: Vec<Path>,
    attributes: Vec<StreamBuilder>,
    additional: Vec<StreamBuilder>,
}

impl<'a, P: Parent> GenEnum<'a, P> {
    pub(crate) fn new(parent: &'a mut P, name: impl Into<String>) -> Self {
        Self {
            parent,
            name: Ident::new(name.into().as_str(), Span::call_site()),
            visibility: Visibility::Default,
            generics: None,
            values: Vec::new(),
            derives: Vec::new(),
            attributes: Vec::new(),
            additional: Vec::new(),
        }
    }

    /// Make the enum `pub`. By default the struct will have no visibility modifier and will only be visible in the current scope.
    pub fn make_pub(&mut self) -> &mut Self {
        self.visibility = Visibility::Pub;
        self
    }

    /// Add a derive macro to the enum.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # use virtue::generate::Path;
    /// # let mut generator = Generator::with_name("Bar");
    /// generator
    ///     .generate_enum("Foo")
    ///     .with_derive("Clone")
    ///     .with_derive("Default")
    ///     .with_derive(Path::from_iter(vec!["serde", "Deserialize"]));
    /// # generator.assert_eq("# [derive (Clone , Default , serde ::Deserialize)] enum Foo { }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```ignore
    /// #[derive(Clone, Default, serde::Deserialize)]
    /// enum Foo { }
    pub fn with_derive(&mut self, derive: impl Into<Path>) -> &mut Self {
        AttributeContainer::with_derive(self, derive)
    }

    /// Add derive macros to the enum.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # use virtue::generate::Path;
    /// # let mut generator = Generator::with_name("Bar");
    /// generator
    ///     .generate_enum("Foo")
    ///     .with_derives([
    ///         "Clone".into(),
    ///         "Default".into(),
    ///         Path::from_iter(vec!["serde", "Deserialize"]),
    ///     ]);
    /// # generator.assert_eq("# [derive (Clone , Default , serde ::Deserialize)] enum Foo { }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```ignore
    /// #[derive(Clone, Default, serde::Deserialize)]
    /// enum Foo { }
    pub fn with_derives<T: Into<Path>>(
        &mut self,
        derives: impl IntoIterator<Item = T>,
    ) -> &mut Self {
        AttributeContainer::with_derives(self, derives)
    }

    /// Add an attribute to the enum. For `#[derive(...)]`, use [`with_derive`](Self::with_derive)
    /// instead.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Bar");
    /// generator
    ///     .generate_enum("Foo")
    ///     .with_attribute("serde", |b| {
    ///         b.push_parsed("(untagged)")?;
    ///         Ok(())
    ///     })?;
    /// # generator.assert_eq("# [serde (untagged)] enum Foo { }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```ignore
    /// #[serde(untagged)]
    /// enum Foo { }
    /// ```
    pub fn with_attribute(
        &mut self,
        name: impl AsRef<str>,
        value: impl FnOnce(&mut StreamBuilder) -> Result,
    ) -> Result<&mut Self> {
        AttributeContainer::with_attribute(self, name, value)
    }

    /// Add a parsed attribute to the enum. For `#[derive(...)]`, use [`with_derive`](Self::with_derive)
    /// instead.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Bar");
    ///
    /// generator
    ///     .generate_enum("Foo")
    ///     .with_parsed_attribute("serde(untagged)")?;
    /// # generator.assert_eq("# [serde (untagged)] enum Foo { }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```ignore
    /// #[serde(untagged)]
    /// enum Foo { }
    /// ```
    pub fn with_parsed_attribute(&mut self, attribute: impl AsRef<str>) -> Result<&mut Self> {
        AttributeContainer::with_parsed_attribute(self, attribute)
    }

    /// Add a token stream as an attribute to the enum. For `#[derive(...)]`, use
    /// [`with_derive`](Self::with_derive) instead.
    ///
    /// ```
    /// # use virtue::prelude::{Generator, TokenStream};
    /// # let mut generator = Generator::with_name("Bar");
    ///
    /// let attribute = "serde(untagged)".parse::<TokenStream>().unwrap();
    /// generator
    ///     .generate_enum("Foo")
    ///     .with_attribute_stream(attribute);
    /// # generator.assert_eq("# [serde (untagged)] enum Foo { }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```ignore
    /// #[serde(untagged)]
    /// enum Foo { }
    /// ```
    pub fn with_attribute_stream(&mut self, attribute: impl Into<TokenStream>) -> &mut Self {
        AttributeContainer::with_attribute_stream(self, attribute)
    }

    /// Inherit the generic parameters of the parent type.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # use virtue::parse::{Generic, Lifetime};
    /// # use proc_macro2::{Ident, Span};
    /// # let mut generator = Generator::with_name("Bar").with_lifetime("a");
    /// // given a derive on enum Bar<'a>
    /// generator
    ///     .generate_enum("Foo")
    ///     .inherit_generics()
    ///     .add_value("Bar")
    ///     .make_tuple()
    ///     .add_field("bar", "&'a str");
    /// # generator.assert_eq("enum Foo < 'a > { Bar (&'a str ,) , }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```ignore
    /// // given a derive on enum Bar<'a>
    /// enum Foo<'a> {
    ///     Bar(&'a str)
    /// }
    /// ```
    pub fn inherit_generics(&mut self) -> &mut Self {
        self.generics = self.parent.generics().cloned();
        self
    }

    /// Append generic parameters to the type.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # use virtue::parse::{Generic, Lifetime};
    /// # use proc_macro2::{Ident, Span};
    /// # let mut generator = Generator::with_name("Bar").with_lifetime("a");
    /// generator
    ///     .generate_enum("Foo")
    ///     .with_generics([Lifetime { ident: Ident::new("a", Span::call_site()), constraint: vec![] }.into()])
    ///     .add_value("Bar")
    ///     .make_tuple()
    ///     .add_field("bar", "&'a str");
    /// # generator.assert_eq("enum Foo < 'a > { Bar (&'a str ,) , }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```ignore
    /// enum Foo<'a> {
    ///     Bar(&'a str)
    /// }
    /// ```
    pub fn with_generics(&mut self, generics: impl IntoIterator<Item = Generic>) -> &mut Self {
        self.generics
            .get_or_insert_with(|| Generics(Vec::new()))
            .extend(generics);
        self
    }

    /// Add a generic parameter to the type.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # use virtue::parse::{Generic, Lifetime};
    /// # use proc_macro2::{Ident, Span};
    /// # let mut generator = Generator::with_name("Bar").with_lifetime("a");
    /// generator
    ///     .generate_enum("Foo")
    ///     .with_generic(Lifetime { ident: Ident::new("a", Span::call_site()), constraint: vec![] }.into())
    ///     .add_value("Bar")
    ///     .make_tuple()
    ///     .add_field("bar", "&'a str");
    /// # generator.assert_eq("enum Foo < 'a > { Bar (&'a str ,) , }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```ignore
    /// enum Foo<'a> {
    ///     Bar(&'a str)
    /// }
    /// ```
    pub fn with_generic(&mut self, generic: Generic) -> &mut Self {
        self.generics
            .get_or_insert_with(|| Generics(Vec::new()))
            .push(generic);
        self
    }

    /// Add an enum value
    ///
    /// Returns a builder for the value that's similar to GenStruct
    pub fn add_value(&mut self, name: impl Into<String>) -> &mut EnumValue {
        self.values.push(EnumValue::new(name));
        self.values.last_mut().unwrap()
    }

    /// Add an `impl <name> for <enum>`
    pub fn impl_for(&mut self, name: impl Into<StringOrIdent>) -> ImplFor<Self> {
        ImplFor::new(self, name.into(), None)
    }

    /// Generate an `impl <name>` implementation. See [`Impl`] for more information.
    pub fn r#impl(&mut self) -> Impl<Self> {
        Impl::with_parent_name(self)
    }

    /// Generate an `impl <name>` implementation. See [`Impl`] for more information.
    ///
    /// Alias for [`impl`] which doesn't need a `r#` prefix.
    ///
    /// [`impl`]: #method.impl
    pub fn generate_impl(&mut self) -> Impl<Self> {
        Impl::with_parent_name(self)
    }
}

impl<P: Parent> AttributeContainer for GenEnum<'_, P> {
    fn derives(&mut self) -> &mut Vec<Path> {
        &mut self.derives
    }

    fn attributes(&mut self) -> &mut Vec<StreamBuilder> {
        &mut self.attributes
    }
}

impl<'a, P: Parent> Parent for GenEnum<'a, P> {
    fn append(&mut self, builder: StreamBuilder) {
        self.additional.push(builder);
    }

    fn name(&self) -> &Ident {
        &self.name
    }

    fn generics(&self) -> Option<&Generics> {
        self.generics.as_ref()
    }

    fn generic_constraints(&self) -> Option<&crate::parse::GenericConstraints> {
        None
    }
}

impl<'a, P: Parent> Drop for GenEnum<'a, P> {
    fn drop(&mut self) {
        let mut builder = StreamBuilder::new();
        self.build_derives(&mut builder)
            .build_attributes(&mut builder);
        if self.visibility == Visibility::Pub {
            builder.ident_str("pub");
        }
        builder
            .ident_str("enum")
            .ident(self.name.clone())
            .append(
                self.generics()
                    .map(Generics::impl_generics)
                    .unwrap_or_default(),
            )
            .group(Delimiter::Brace, |b| {
                for value in self.values.iter_mut() {
                    build_value(b, value)?;
                }

                Ok(())
            })
            .expect("Could not build enum");

        for additional in std::mem::take(&mut self.additional) {
            builder.append(additional);
        }
        self.parent.append(builder);
    }
}

fn build_value(builder: &mut StreamBuilder, value: &mut EnumValue) -> Result {
    value.build_attributes(builder);
    builder.ident(value.name.clone());

    match value.value_type {
        ValueType::Named => builder.group(Delimiter::Brace, |b| {
            for field in value.fields.iter_mut() {
                field.build_attributes(b);
                if field.vis == Visibility::Pub {
                    b.ident_str("pub");
                }
                b.ident_str(&field.name)
                    .punct(':')
                    .push_parsed(&field.ty)?
                    .punct(',');
            }
            Ok(())
        })?,
        ValueType::Unnamed => builder.group(Delimiter::Parenthesis, |b| {
            for field in value.fields.iter_mut() {
                field.build_attributes(b);
                if field.vis == Visibility::Pub {
                    b.ident_str("pub");
                }
                b.push_parsed(&field.ty)?.punct(',');
            }
            Ok(())
        })?,
        ValueType::Zst => builder,
    };

    builder.punct(',');

    Ok(())
}

pub struct EnumValue {
    name: Ident,
    fields: Vec<Field>,
    value_type: ValueType,
    attributes: Vec<StreamBuilder>,
}

impl EnumValue {
    fn new(name: impl Into<String>) -> Self {
        Self {
            name: Ident::new(name.into().as_str(), Span::call_site()),
            fields: Vec::new(),
            value_type: ValueType::Named,
            attributes: Vec::new(),
        }
    }

    /// Make the struct a zero-sized type (no fields)
    ///
    /// Any fields will be ignored
    pub fn make_zst(&mut self) -> &mut Self {
        self.value_type = ValueType::Zst;
        self
    }

    /// Make the struct fields unnamed
    ///
    /// The names of any field will be ignored
    pub fn make_tuple(&mut self) -> &mut Self {
        self.value_type = ValueType::Unnamed;
        self
    }

    /// Add an attribute to the variant.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Bar");
    /// generator
    ///     .generate_enum("Foo")
    ///     .add_value("Bar")
    ///     .with_attribute("serde", |b| {
    ///         b.push_parsed("(rename_all = \"camelCase\")")?;
    ///         Ok(())
    ///     })?;
    /// # generator.assert_eq("enum Foo { # [serde (rename_all = \"camelCase\")] Bar { } , }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```ignore
    /// enum Foo {
    ///     #[serde(rename_all = "camelCase")]
    ///     Bar { }
    /// }
    /// ```
    pub fn with_attribute(
        &mut self,
        name: impl AsRef<str>,
        value: impl FnOnce(&mut StreamBuilder) -> Result,
    ) -> Result<&mut Self> {
        AttributeContainer::with_attribute(self, name, value)
    }

    /// Add a parsed attribute to the variant.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Bar");
    /// generator
    ///     .generate_enum("Foo")
    ///     .add_value("Bar")
    ///     .with_parsed_attribute("serde(rename_all = \"camelCase\")")?;
    /// # generator.assert_eq("enum Foo { # [serde (rename_all = \"camelCase\")] Bar { } , }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```ignore
    /// enum Foo {
    ///     #[serde(rename_all = "camelCase")]
    ///     Bar { }
    /// }
    /// ```
    pub fn with_parsed_attribute(&mut self, attribute: impl AsRef<str>) -> Result<&mut Self> {
        AttributeContainer::with_parsed_attribute(self, attribute)
    }

    /// Add a token stream as an attribute to the variant.
    ///
    /// ```
    /// # use virtue::prelude::{Generator, TokenStream};
    /// # let mut generator = Generator::with_name("Bar");
    /// let attribute = "serde(rename_all = \"camelCase\")".parse::<TokenStream>().unwrap();
    /// generator
    ///     .generate_enum("Foo")
    ///     .add_value("Bar")
    ///     .with_attribute_stream(attribute);
    /// # generator.assert_eq("enum Foo { # [serde (rename_all = \"camelCase\")] Bar { } , }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```ignore
    /// enum Foo {
    ///     #[serde(rename_all = "camelCase")]
    ///     Bar { }
    /// }
    /// ```
    pub fn with_attribute_stream(&mut self, attribute: impl Into<TokenStream>) -> &mut Self {
        AttributeContainer::with_attribute_stream(self, attribute)
    }

    /// Add a field to the enum value.
    ///
    /// Names are ignored when the enum value's fields are unnamed
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Fooz");
    /// generator
    ///     .generate_enum("Foo")
    ///     .add_value("Bar")
    ///     .add_field("bar", "u16")
    ///     .add_field("baz", "String");
    /// # generator.assert_eq("enum Foo { Bar { bar : u16 , baz : String , } , }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```
    /// enum Foo {
    ///     Bar {
    ///         bar: u16,
    ///         baz: String
    ///     }
    /// };
    /// ```
    pub fn add_field(
        &mut self,
        name: impl Into<String>,
        ty: impl Into<String>,
    ) -> FieldBuilder<Self> {
        let mut fields = FieldBuilder::from(&mut self.fields);
        fields.add_field(name, ty);
        fields
    }
}

impl AttributeContainer for EnumValue {
    fn derives(&mut self) -> &mut Vec<Path> {
        unreachable!("enum variants cannot have derives")
    }

    fn attributes(&mut self) -> &mut Vec<StreamBuilder> {
        &mut self.attributes
    }
}

enum ValueType {
    Named,
    Unnamed,
    Zst,
}
