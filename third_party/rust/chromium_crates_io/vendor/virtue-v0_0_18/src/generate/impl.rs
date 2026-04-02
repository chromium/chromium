use super::{generate_item::FnParent, FnBuilder, GenConst, Generator, Parent, StreamBuilder};
use crate::{
    parse::{GenericConstraints, Generics},
    prelude::{Delimiter, Result},
};

#[must_use]
/// A helper struct for implementing functions for a given struct or enum.
pub struct Impl<'a, P: Parent> {
    parent: &'a mut P,
    outer_attr: Vec<StreamBuilder>,
    inner_attr: Vec<StreamBuilder>,
    name: String,
    // pub(super) group: StreamBuilder,
    consts: Vec<StreamBuilder>,
    custom_generic_constraints: Option<GenericConstraints>,
    fns: Vec<(StreamBuilder, StreamBuilder)>,
}

impl<'a, P: Parent> Impl<'a, P> {
    pub(super) fn with_parent_name(parent: &'a mut P) -> Self {
        Self {
            outer_attr: Vec::new(),
            inner_attr: Vec::new(),
            name: parent.name().to_string(),
            parent,
            consts: Vec::new(),
            custom_generic_constraints: None,
            fns: Vec::new(),
        }
    }

    pub(super) fn new(parent: &'a mut P, name: impl Into<String>) -> Self {
        Self {
            outer_attr: Vec::new(),
            inner_attr: Vec::new(),
            parent,
            name: name.into(),
            consts: Vec::new(),
            custom_generic_constraints: None,
            fns: Vec::new(),
        }
    }

    /// Add a outer attribute to the trait implementation
    pub fn impl_outer_attr(&mut self, attr: impl AsRef<str>) -> Result {
        let mut builder = StreamBuilder::new();
        builder.punct('#').group(Delimiter::Bracket, |builder| {
            builder.push_parsed(attr)?;
            Ok(())
        })?;
        self.outer_attr.push(builder);
        Ok(())
    }

    /// Add a inner attribute to the trait implementation
    pub fn impl_inner_attr(&mut self, attr: impl AsRef<str>) -> Result {
        let mut builder = StreamBuilder::new();
        builder
            .punct('#')
            .punct('!')
            .group(Delimiter::Brace, |builder| {
                builder.push_parsed(attr)?;
                Ok(())
            })?;
        self.inner_attr.push(builder);
        Ok(())
    }

    /// Add a function to the trait implementation.
    ///
    /// `generator.impl().generate_fn("bar")` results in code like:
    ///
    /// ```ignore
    /// impl <struct or enum> {
    ///     fn bar() {}
    /// }
    /// ```
    ///
    /// See [`FnBuilder`] for more options, as well as information on how to fill the function body.
    pub fn generate_fn(&mut self, name: impl Into<String>) -> FnBuilder<Self> {
        FnBuilder::new(self, name)
    }

    /// Add a const to the trait implementation
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Bar");
    /// generator.impl_for("Foo")
    ///          .generate_const("BAR", "u8")
    ///          .with_value(|b| {
    ///             b.push_parsed("5")?;
    ///             Ok(())
    ///          })?;
    /// # generator.assert_eq("impl Foo for Bar { const BAR : u8 = 5 ; }");
    /// # Ok::<_, virtue::Error>(())
    /// ```
    ///
    /// Generates:
    /// ```ignore
    /// impl Foo for <struct or enum> {
    ///     const BAR: u8 = 5;
    /// }
    pub fn generate_const(&mut self, name: impl Into<String>, ty: impl Into<String>) -> GenConst {
        GenConst::new(&mut self.consts, name, ty)
    }
}

impl<'a> Impl<'a, Generator> {
    /// Modify the generic constraints of a type.
    /// This can be used to add additional type constraints to your implementation.
    ///
    /// ```ignore
    /// // Your derive:
    /// #[derive(YourTrait)]
    /// pub struct Foo<B> {
    ///     ...
    /// }
    ///
    /// // With this code:
    /// generator
    ///     .r#impl()
    ///     .modify_generic_constraints(|generics, constraints| {
    ///         for g in generics.iter_generics() {
    ///             constraints.push_generic(g, "YourTrait");
    ///         }
    ///     })
    ///
    /// // will generate:
    /// impl<B> Foo<B>
    ///     where B: YourTrait // <-
    /// {
    /// }
    /// ```
    ///
    /// Note that this function is only implemented when you call `.r#impl` on [`Generator`].
    pub fn modify_generic_constraints<CB>(&mut self, cb: CB) -> &mut Self
    where
        CB: FnOnce(&Generics, &mut GenericConstraints),
    {
        if let Some(generics) = self.parent.generics() {
            let constraints = self.custom_generic_constraints.get_or_insert_with(|| {
                self.parent
                    .generic_constraints()
                    .cloned()
                    .unwrap_or_default()
            });
            cb(generics, constraints);
        }
        self
    }
}

impl<'a, P: Parent> FnParent for Impl<'a, P> {
    fn append(&mut self, fn_definition: StreamBuilder, fn_body: StreamBuilder) -> Result {
        self.fns.push((fn_definition, fn_body));
        Ok(())
    }
}

impl<'a, P: Parent> Drop for Impl<'a, P> {
    fn drop(&mut self) {
        if std::thread::panicking() {
            return;
        }
        let mut builder = StreamBuilder::new();
        for attr in std::mem::take(&mut self.outer_attr) {
            builder.append(attr);
        }
        builder.ident_str("impl");

        if let Some(generics) = self.parent.generics() {
            builder.append(generics.impl_generics());
        }
        builder.push_parsed(&self.name).unwrap();

        if let Some(generics) = self.parent.generics() {
            builder.append(generics.type_generics());
        }
        if let Some(generic_constraints) = self.custom_generic_constraints.take() {
            builder.append(generic_constraints.where_clause());
        } else if let Some(generic_constraints) = self.parent.generic_constraints() {
            builder.append(generic_constraints.where_clause());
        }

        builder
            .group(Delimiter::Brace, |builder| {
                for attr in std::mem::take(&mut self.inner_attr) {
                    builder.append(attr);
                }
                for r#const in std::mem::take(&mut self.consts) {
                    builder.append(r#const);
                }
                for (fn_def, fn_body) in std::mem::take(&mut self.fns) {
                    builder.append(fn_def);
                    builder
                        .group(Delimiter::Brace, |body| {
                            *body = fn_body;
                            Ok(())
                        })
                        .unwrap();
                }
                Ok(())
            })
            .unwrap();

        self.parent.append(builder);
    }
}
