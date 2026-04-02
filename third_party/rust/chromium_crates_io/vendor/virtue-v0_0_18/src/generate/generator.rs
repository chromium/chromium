use super::{GenEnum, GenStruct, GenerateMod, Impl, ImplFor, StreamBuilder, StringOrIdent};
use crate::parse::{GenericConstraints, Generics};
use crate::prelude::{Ident, TokenStream};

#[must_use]
/// The generator is used to generate code.
///
/// Often you will want to use [`impl_for`] to generate an `impl <trait_name> for <target_name()>`.
///
/// [`impl_for`]: #method.impl_for
pub struct Generator {
    name: Ident,
    generics: Option<Generics>,
    generic_constraints: Option<GenericConstraints>,
    stream: StreamBuilder,
}

impl Generator {
    pub(crate) fn new(
        name: Ident,
        generics: Option<Generics>,
        generic_constraints: Option<GenericConstraints>,
    ) -> Self {
        Self {
            name,
            generics,
            generic_constraints,
            stream: StreamBuilder::new(),
        }
    }

    /// Return the name for the struct or enum that this is going to be implemented on.
    pub fn target_name(&self) -> Ident {
        self.name.clone()
    }

    /// Generate an `impl <target_name>` implementation. See [`Impl`] for more information.
    ///
    /// This will default to the type that is associated with this generator. If you need to generate an impl for another type you can use `impl_for_other_type`
    pub fn r#impl(&mut self) -> Impl<Self> {
        Impl::with_parent_name(self)
    }

    /// Generate an `impl <target_name>` implementation. See [`Impl`] for more information.
    ///
    /// Alias for [`impl`] which doesn't need a `r#` prefix.
    ///
    /// [`impl`]: #method.impl
    pub fn generate_impl(&mut self) -> Impl<Self> {
        Impl::with_parent_name(self)
    }

    /// Generate an `for <trait_name> for <target_name>` implementation. See [ImplFor] for more information.
    ///
    /// This will default to the type that is associated with this generator. If you need to generate an impl for another type you can use `impl_trait_for_other_type`
    pub fn impl_for(&mut self, trait_name: impl Into<String>) -> ImplFor<Self> {
        ImplFor::new(
            self,
            self.name.clone().into(),
            Some(trait_name.into().into()),
        )
    }

    /// Generate an `impl <type_name>` block. See [ImplFor] for more information.
    /// ```
    /// # use virtue::prelude::*;
    /// # let mut generator = Generator::with_name("Baz");
    /// generator.impl_for_other_type("Foo");
    ///
    /// // will output:
    /// // impl Foo { }
    /// # generator.assert_eq("impl Foo { }");
    /// ```
    pub fn impl_for_other_type(&mut self, type_name: impl Into<StringOrIdent>) -> ImplFor<Self> {
        ImplFor::new(self, type_name.into(), None)
    }

    /// Generate an `impl <trait_name> for <type_name>` block. See [ImplFor] for more information.
    /// ```
    /// # use virtue::prelude::*;
    /// # let mut generator = Generator::with_name("Baz");
    /// generator.impl_trait_for_other_type("Foo", "Bar");
    ///
    /// // will output:
    /// // impl Foo for Bar { }
    /// # generator.assert_eq("impl Foo for Bar { }");
    /// ```
    pub fn impl_trait_for_other_type(
        &mut self,
        trait_name: impl Into<StringOrIdent>,
        type_name: impl Into<StringOrIdent>,
    ) -> ImplFor<Self> {
        ImplFor::new(self, type_name.into(), Some(trait_name.into()))
    }

    /// Generate an `for <..lifetimes> <trait_name> for <target_name>` implementation. See [ImplFor] for more information.
    ///
    /// Note:
    /// - Lifetimes should _not_ have the leading apostrophe.
    /// - `trait_name` should _not_ have custom lifetimes. These will be added automatically.
    ///
    /// ```
    /// # use virtue::prelude::*;
    /// # let mut generator = Generator::with_name("Bar");
    /// generator.impl_for_with_lifetimes("Foo", ["a", "b"]);
    ///
    /// // will output:
    /// // impl<'a, 'b> Foo<'a, 'b> for StructOrEnum { }
    /// # generator.assert_eq("impl < 'a , 'b > Foo < 'a , 'b > for Bar { }");
    /// ```
    ///
    /// The new lifetimes are not associated with any existing lifetimes. If you want this behavior you can call `.impl_for_with_lifetimes(...).new_lifetimes_depend_on_existing()`
    ///
    /// ```
    /// # use virtue::prelude::*;
    /// # let mut generator = Generator::with_name("Bar").with_lifetime("a");
    /// // given a derive on `struct<'a> Bar<'a>`
    /// generator.impl_for_with_lifetimes("Foo", ["b"]).new_lifetimes_depend_on_existing();
    ///
    /// // will output:
    /// // impl<'a, 'b> Foo<'b> for Bar<'a> where 'b: 'a { }
    /// # generator.assert_eq("impl < 'b , 'a > Foo < 'b > for Bar < 'a > where 'b : 'a { }");
    /// ```
    pub fn impl_for_with_lifetimes<ITER, T>(
        &mut self,
        trait_name: T,
        lifetimes: ITER,
    ) -> ImplFor<Self>
    where
        ITER: IntoIterator,
        ITER::Item: Into<String>,
        T: Into<StringOrIdent>,
    {
        ImplFor::new(self, self.name.clone().into(), Some(trait_name.into()))
            .with_lifetimes(lifetimes)
    }

    /// Generate a struct with the given name. See [`GenStruct`] for more info.
    pub fn generate_struct(&mut self, name: impl Into<String>) -> GenStruct<Self> {
        GenStruct::new(self, name)
    }

    /// Generate an enum with the given name. See [`GenEnum`] for more info.
    pub fn generate_enum(&mut self, name: impl Into<String>) -> GenEnum<Self> {
        GenEnum::new(self, name)
    }

    /// Generate a `mod <name> { ... }`. See [`GenerateMod`] for more info.
    pub fn generate_mod(&mut self, mod_name: impl Into<String>) -> GenerateMod<Self> {
        GenerateMod::new(self, mod_name)
    }

    /// Export the current stream to a file, making it very easy to debug the output of a derive macro.
    /// This will try to find rust's `target` directory, and write `target/generated/<crate_name>/<name>_<file_postfix>.rs`.
    ///
    /// Will return `true` if the file is written, `false` otherwise.
    ///
    /// The outputted file is unformatted. Use `cargo fmt -- target/generated/<crate_name>/<file>.rs` to format the file.
    pub fn export_to_file(&self, crate_name: &str, file_postfix: &str) -> bool {
        use std::io::Write;

        if let Ok(var) = std::env::var("CARGO_MANIFEST_DIR") {
            let mut path = std::path::PathBuf::from(var);
            loop {
                {
                    let mut path = path.clone();
                    path.push("target");
                    if path.exists() {
                        path.push("generated");
                        path.push(crate_name);
                        if std::fs::create_dir_all(&path).is_err() {
                            return false;
                        }
                        path.push(format!("{}_{}.rs", self.target_name(), file_postfix));
                        if let Ok(mut file) = std::fs::File::create(path) {
                            let _ = file.write_all(self.stream.stream.to_string().as_bytes());
                            return true;
                        }
                    }
                }
                if let Some(parent) = path.parent() {
                    path = parent.into();
                } else {
                    break;
                }
            }
        }
        false
    }

    /// Consume the contents of this generator. This *must* be called, or else the generator will panic on drop.
    pub fn finish(mut self) -> crate::prelude::Result<TokenStream> {
        Ok(std::mem::take(&mut self.stream).stream)
    }
}

#[cfg(feature = "proc-macro2")]
impl Generator {
    /// Create a new generator with the name `name`. This is useful for testing purposes in combination with the `assert_eq` function.
    pub fn with_name(name: &str) -> Self {
        Self::new(
            Ident::new(name, crate::prelude::Span::call_site()),
            None,
            None,
        )
    }
    /// Add a lifetime to this generator.
    pub fn with_lifetime(mut self, lt: &str) -> Self {
        self.generics
            .get_or_insert_with(|| Generics(Vec::new()))
            .push(crate::parse::Generic::Lifetime(crate::parse::Lifetime {
                ident: crate::prelude::Ident::new(lt, crate::prelude::Span::call_site()),
                constraint: Vec::new(),
            }));
        self
    }
    /// Assert that the generated code in this generator matches the given string. This is useful for testing purposes in combination with the `with_name` function.
    pub fn assert_eq(&self, expected: &str) {
        assert_eq!(expected, self.stream.stream.to_string());
    }
}

impl Drop for Generator {
    fn drop(&mut self) {
        if !self.stream.stream.is_empty() && !std::thread::panicking() {
            eprintln!("WARNING: Generator dropped but the stream is not empty. Please call `.finish()` on the generator");
        }
    }
}

impl super::Parent for Generator {
    fn append(&mut self, builder: StreamBuilder) {
        self.stream.append(builder);
    }

    fn name(&self) -> &Ident {
        &self.name
    }

    fn generics(&self) -> Option<&Generics> {
        self.generics.as_ref()
    }

    fn generic_constraints(&self) -> Option<&GenericConstraints> {
        self.generic_constraints.as_ref()
    }
}

#[cfg(test)]
mod test {
    use proc_macro2::Span;

    use crate::token_stream;

    use super::*;

    #[test]
    fn impl_for_with_lifetimes() {
        // No generics
        let mut generator =
            Generator::new(Ident::new("StructOrEnum", Span::call_site()), None, None);
        let _ = generator.impl_for_with_lifetimes("Foo", ["a", "b"]);
        let output = generator.finish().unwrap();
        assert_eq!(
            output
                .into_iter()
                .map(|v| v.to_string())
                .collect::<String>(),
            token_stream("impl<'a, 'b> Foo<'a, 'b> for StructOrEnum { }")
                .map(|v| v.to_string())
                .collect::<String>(),
        );

        //with simple generics
        let mut generator = Generator::new(
            Ident::new("StructOrEnum", Span::call_site()),
            Generics::try_take(&mut token_stream("<T1, T2>")).unwrap(),
            None,
        );
        let _ = generator.impl_for_with_lifetimes("Foo", ["a", "b"]);
        let output = generator.finish().unwrap();
        assert_eq!(
            output
                .into_iter()
                .map(|v| v.to_string())
                .collect::<String>(),
            token_stream("impl<'a, 'b, T1, T2> Foo<'a, 'b> for StructOrEnum<T1, T2> { }")
                .map(|v| v.to_string())
                .collect::<String>()
        );

        // with lifetimes
        let mut generator = Generator::new(
            Ident::new("StructOrEnum", Span::call_site()),
            Generics::try_take(&mut token_stream("<'alpha, 'beta>")).unwrap(),
            None,
        );
        let _ = generator.impl_for_with_lifetimes("Foo", ["a", "b"]);
        let output = generator.finish().unwrap();
        assert_eq!(
            output
                .into_iter()
                .map(|v| v.to_string())
                .collect::<String>(),
            token_stream(
                "impl<'a, 'b, 'alpha, 'beta> Foo<'a, 'b> for StructOrEnum<'alpha, 'beta> { }"
            )
            .map(|v| v.to_string())
            .collect::<String>()
        );
    }

    #[test]
    fn impl_for_with_trait_generics() {
        let mut generator = Generator::new(
            Ident::new("StructOrEnum", Span::call_site()),
            Generics::try_take(&mut token_stream("<'a>")).unwrap(),
            None,
        );
        let _ = generator.impl_for("Foo").with_trait_generics(["&'a str"]);
        let output = generator.finish().unwrap();
        assert_eq!(
            output
                .into_iter()
                .map(|v| v.to_string())
                .collect::<String>(),
            token_stream("impl<'a> Foo<&'a str> for StructOrEnum<'a> { }")
                .map(|v| v.to_string())
                .collect::<String>(),
        );
    }

    #[test]
    fn impl_for_with_impl_generics() {
        //with simple generics
        let mut generator = Generator::new(
            Ident::new("StructOrEnum", Span::call_site()),
            Generics::try_take(&mut token_stream("<T1, T2>")).unwrap(),
            None,
        );
        let _ = generator.impl_for("Foo").with_impl_generics(["Bar"]);

        let output = generator.finish().unwrap();
        assert_eq!(
            output
                .into_iter()
                .map(|v| v.to_string())
                .collect::<String>(),
            token_stream("impl<T1, T2, Bar> Foo for StructOrEnum<T1, T2> { }")
                .map(|v| v.to_string())
                .collect::<String>()
        );
        // with lifetimes
        let mut generator = Generator::new(
            Ident::new("StructOrEnum", Span::call_site()),
            Generics::try_take(&mut token_stream("<'alpha, 'beta>")).unwrap(),
            None,
        );
        let _ = generator.impl_for("Foo").with_impl_generics(["Bar"]);
        let output = generator.finish().unwrap();
        assert_eq!(
            output
                .into_iter()
                .map(|v| v.to_string())
                .collect::<String>(),
            token_stream("impl<'alpha, 'beta, Bar> Foo for StructOrEnum<'alpha, 'beta> { }")
                .map(|v| v.to_string())
                .collect::<String>()
        );
    }
}
