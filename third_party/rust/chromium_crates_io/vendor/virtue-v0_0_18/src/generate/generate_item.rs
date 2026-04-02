use super::StreamBuilder;
use crate::{
    parse::Visibility,
    prelude::{Delimiter, Result},
};

/// A builder for constants.
pub struct GenConst<'a> {
    consts: &'a mut Vec<StreamBuilder>,
    attrs: Vec<String>,
    name: String,
    ty: String,
    vis: Visibility,
}

impl<'a> GenConst<'a> {
    pub(crate) fn new(
        consts: &'a mut Vec<StreamBuilder>,
        name: impl Into<String>,
        ty: impl Into<String>,
    ) -> Self {
        Self {
            consts,
            attrs: Vec::new(),
            name: name.into(),
            ty: ty.into(),
            vis: Visibility::Default,
        }
    }

    /// Make the const `pub`. By default the const will have no visibility modifier and will only be visible in the current scope.
    #[must_use]
    pub fn make_pub(mut self) -> Self {
        self.vis = Visibility::Pub;
        self
    }

    /// Add an outer attribute
    #[must_use]
    pub fn with_attr(mut self, attr: impl Into<String>) -> Self {
        self.attrs.push(attr.into());
        self
    }

    /// Complete the constant definition. This function takes a callback that will form the value of the constant.
    ///
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
    /// ```
    pub fn with_value<F>(self, f: F) -> Result
    where
        F: FnOnce(&mut StreamBuilder) -> Result,
    {
        let mut builder = StreamBuilder::new();

        for attr in self.attrs {
            builder
                .punct('#')
                .punct('!')
                .group(Delimiter::Bracket, |builder| {
                    builder.push_parsed(attr)?;
                    Ok(())
                })?;
        }

        if self.vis == Visibility::Pub {
            builder.ident_str("pub");
        }

        builder
            .ident_str("const")
            .push_parsed(self.name)?
            .punct(':')
            .push_parsed(self.ty)?
            .punct('=');
        f(&mut builder)?;
        builder.punct(';');

        self.consts.push(builder);
        Ok(())
    }
}

/// A builder for functions.
pub struct FnBuilder<'a, P> {
    parent: &'a mut P,
    name: String,

    attrs: Vec<String>,
    is_async: bool,
    lifetimes: Vec<(String, Vec<String>)>,
    generics: Vec<(String, Vec<String>)>,
    self_arg: FnSelfArg,
    args: Vec<(String, String)>,
    return_type: Option<String>,
    vis: Visibility,
}

impl<'a, P: FnParent> FnBuilder<'a, P> {
    pub(super) fn new(parent: &'a mut P, name: impl Into<String>) -> Self {
        Self {
            parent,
            name: name.into(),
            attrs: Vec::new(),
            is_async: false,
            lifetimes: Vec::new(),
            generics: Vec::new(),
            self_arg: FnSelfArg::None,
            args: Vec::new(),
            return_type: None,
            vis: Visibility::Default,
        }
    }

    /// Add an outer attribute
    #[must_use]
    pub fn with_attr(mut self, attr: impl Into<String>) -> Self {
        self.attrs.push(attr.into());
        self
    }

    /// Add a lifetime parameter.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Foo");
    /// generator
    ///     .r#impl()
    ///     .generate_fn("foo") // fn foo()
    ///     .with_lifetime("a") // fn foo<'a>()
    /// # .body(|_| Ok(())).unwrap();
    /// # generator.assert_eq("impl Foo { fn foo < 'a > () { } }");
    /// ```
    #[must_use]
    pub fn with_lifetime(mut self, name: impl Into<String>) -> Self {
        self.lifetimes.push((name.into(), Vec::new()));
        self
    }

    /// Make the function async
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Foo");
    /// generator
    ///     .r#impl()
    ///     .generate_fn("foo") // fn foo()
    ///     .as_async() // async fn foo()
    /// # .body(|_| Ok(())).unwrap();
    /// # generator.assert_eq("impl Foo { async fn foo () { } }");
    /// ```
    #[must_use]
    pub fn as_async(mut self) -> Self {
        self.is_async = true;
        self
    }

    /// Add a lifetime parameter.
    ///
    /// `dependencies` are the lifetime dependencies of the given lifetime.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Foo");
    /// generator
    ///     .r#impl()
    ///     .generate_fn("foo") // fn foo()
    ///     .with_lifetime("a") // fn foo<'a>()
    ///     .with_lifetime_deps("b", ["a"]) // fn foo<'b: 'a>()
    /// # .body(|_| Ok(())).unwrap();
    /// # generator.assert_eq("impl Foo { fn foo < 'a , 'b : 'a > () { } }");
    /// ```
    #[must_use]
    pub fn with_lifetime_deps<ITER, I>(
        mut self,
        name: impl Into<String>,
        dependencies: ITER,
    ) -> Self
    where
        ITER: IntoIterator<Item = I>,
        I: Into<String>,
    {
        self.lifetimes.push((
            name.into(),
            dependencies.into_iter().map(Into::into).collect(),
        ));
        self
    }

    /// Add a generic parameter. Keep in mind that will *not* work for lifetimes.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Foo");
    /// generator
    ///     .r#impl()
    ///     .generate_fn("foo") // fn foo()
    ///     .with_generic("D") // fn foo<D>()
    /// # .body(|_| Ok(())).unwrap();
    /// # generator.assert_eq("impl Foo { fn foo < D > () { } }");
    /// ```
    #[must_use]
    pub fn with_generic(mut self, name: impl Into<String>) -> Self {
        self.generics.push((name.into(), Vec::new()));
        self
    }

    /// Add a generic parameter. Keep in mind that will *not* work for lifetimes.
    ///
    /// `dependencies` are the dependencies of the parameter.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Foo");
    /// generator
    ///     .r#impl()
    ///     .generate_fn("foo") // fn foo()
    ///     .with_generic("D") // fn foo<D>()
    ///     .with_generic_deps("E", ["Encodable"]) // fn foo<D, E: Encodable>();
    /// # .body(|_| Ok(())).unwrap();
    /// # generator.assert_eq("impl Foo { fn foo < D , E : Encodable > () { } }");
    /// ```
    #[must_use]
    pub fn with_generic_deps<DEP, I>(mut self, name: impl Into<String>, dependencies: DEP) -> Self
    where
        DEP: IntoIterator<Item = I>,
        I: Into<String>,
    {
        self.generics.push((
            name.into(),
            dependencies.into_iter().map(Into::into).collect(),
        ));
        self
    }

    /// Set the value for `self`. See [FnSelfArg] for more information.
    ///
    /// ```
    /// # use virtue::prelude::{Generator, FnSelfArg};
    /// # let mut generator = Generator::with_name("Foo");
    /// generator
    ///     .r#impl()
    ///     .generate_fn("foo") // fn foo()
    ///     .with_self_arg(FnSelfArg::RefSelf) // fn foo(&self)
    /// # .body(|_| Ok(())).unwrap();
    /// # generator.assert_eq("impl Foo { fn foo (& self ,) { } }");
    /// ```
    #[must_use]
    pub fn with_self_arg(mut self, self_arg: FnSelfArg) -> Self {
        self.self_arg = self_arg;
        self
    }

    /// Add an argument with a `name` and a `ty`.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Foo");
    /// generator
    ///     .r#impl()
    ///     .generate_fn("foo") // fn foo()
    ///     .with_arg("a", "u32") // fn foo(a: u32)
    ///     .with_arg("b", "u32") // fn foo(a: u32, b: u32)
    /// # .body(|_| Ok(())).unwrap();
    /// # generator.assert_eq("impl Foo { fn foo (a : u32 , b : u32) { } }");
    /// ```
    #[must_use]
    pub fn with_arg(mut self, name: impl Into<String>, ty: impl Into<String>) -> Self {
        self.args.push((name.into(), ty.into()));
        self
    }

    /// Set the return type for the function. By default the function will have no return type.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Foo");
    /// generator
    ///     .r#impl()
    ///     .generate_fn("foo") // fn foo()
    ///     .with_return_type("u32") // fn foo() -> u32
    /// # .body(|_| Ok(())).unwrap();
    /// # generator.assert_eq("impl Foo { fn foo () ->u32 { } }");
    /// ```
    #[must_use]
    pub fn with_return_type(mut self, ret_type: impl Into<String>) -> Self {
        self.return_type = Some(ret_type.into());
        self
    }

    /// Make the function `pub`. If this is not called, the function will have no visibility modifier.
    #[must_use]
    pub fn make_pub(mut self) -> Self {
        self.vis = Visibility::Pub;
        self
    }

    /// Complete the function definition. This function takes a callback that will form the body of the function.
    ///
    /// ```
    /// # use virtue::prelude::Generator;
    /// # let mut generator = Generator::with_name("Foo");
    /// generator
    ///     .r#impl()
    ///     .generate_fn("foo") // fn foo()
    ///     .body(|b| {
    ///         b.push_parsed("println!(\"hello world\");")?;
    ///         Ok(())
    ///     })
    ///     .unwrap();
    /// // fn foo() {
    /// //     println!("Hello world");
    /// // }
    /// # generator.assert_eq("impl Foo { fn foo () { println ! (\"hello world\") ; } }");
    /// ```
    pub fn body(
        self,
        body_builder: impl FnOnce(&mut StreamBuilder) -> crate::Result,
    ) -> crate::Result {
        let FnBuilder {
            parent,
            name,
            attrs,
            is_async,
            lifetimes,
            generics,
            self_arg,
            args,
            return_type,
            vis,
        } = self;

        let mut builder = StreamBuilder::new();

        // attrs
        for attr in attrs {
            builder.punct('#').group(Delimiter::Bracket, |builder| {
                builder.push_parsed(attr)?;
                Ok(())
            })?;
        }

        // function name; `fn name`
        if vis == Visibility::Pub {
            builder.ident_str("pub");
        }
        if is_async {
            builder.ident_str("async");
        }
        builder.ident_str("fn");
        builder.ident_str(name);

        // lifetimes; `<'a: 'b, D: Display>`
        if !lifetimes.is_empty() || !generics.is_empty() {
            builder.punct('<');
            let mut is_first = true;
            for (lifetime, dependencies) in lifetimes {
                if is_first {
                    is_first = false;
                } else {
                    builder.punct(',');
                }
                builder.lifetime_str(lifetime.as_ref());
                if !dependencies.is_empty() {
                    for (idx, dependency) in dependencies.into_iter().enumerate() {
                        builder.punct(if idx == 0 { ':' } else { '+' });
                        builder.lifetime_str(dependency.as_ref());
                    }
                }
            }
            for (generic, dependencies) in generics {
                if is_first {
                    is_first = false;
                } else {
                    builder.punct(',');
                }
                builder.ident_str(&generic);
                if !dependencies.is_empty() {
                    for (idx, dependency) in dependencies.into_iter().enumerate() {
                        builder.punct(if idx == 0 { ':' } else { '+' });
                        builder.push_parsed(&dependency)?;
                    }
                }
            }
            builder.punct('>');
        }

        // Arguments; `(&self, foo: &Bar)`
        builder.group(Delimiter::Parenthesis, |arg_stream| {
            if let Some(self_arg) = self_arg.into_token_tree() {
                arg_stream.append(self_arg);
                arg_stream.punct(',');
            }
            for (idx, (arg_name, arg_ty)) in args.into_iter().enumerate() {
                if idx != 0 {
                    arg_stream.punct(',');
                }
                arg_stream.push_parsed(&arg_name)?;
                arg_stream.punct(':');
                arg_stream.push_parsed(&arg_ty)?;
            }
            Ok(())
        })?;

        // Return type: `-> ResultType`
        if let Some(return_type) = return_type {
            builder.puncts("->");
            builder.push_parsed(&return_type)?;
        }

        let mut body_stream = StreamBuilder::new();
        body_builder(&mut body_stream)?;

        parent.append(builder, body_stream)
    }
}

pub trait FnParent {
    fn append(&mut self, fn_definition: StreamBuilder, fn_body: StreamBuilder) -> Result;
}

/// The `self` argument of a function
#[allow(dead_code)]
#[non_exhaustive]
pub enum FnSelfArg {
    /// No `self` argument. The function will be a static function.
    None,

    /// `self`. The function will consume self.
    TakeSelf,

    /// `mut self`. The function will consume self.
    MutTakeSelf,

    /// `&self`. The function will take self by reference.
    RefSelf,

    /// `&mut self`. The function will take self by mutable reference.
    MutSelf,
}

impl FnSelfArg {
    fn into_token_tree(self) -> Option<StreamBuilder> {
        let mut builder = StreamBuilder::new();
        match self {
            Self::None => return None,
            Self::TakeSelf => {
                builder.ident_str("self");
            }
            Self::MutTakeSelf => {
                builder.ident_str("mut");
                builder.ident_str("self");
            }
            Self::RefSelf => {
                builder.punct('&');
                builder.ident_str("self");
            }
            Self::MutSelf => {
                builder.punct('&');
                builder.ident_str("mut");
                builder.ident_str("self");
            }
        }
        Some(builder)
    }
}
