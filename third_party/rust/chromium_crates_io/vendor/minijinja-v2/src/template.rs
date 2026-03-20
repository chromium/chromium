use std::cell::RefCell;
use std::collections::{BTreeMap, HashSet};
use std::ops::Deref;
use std::sync::Arc;
use std::{fmt, io};

use crate::vendor::self_cell::self_cell;
use serde::Serialize;

use crate::compiler::codegen::CodeGenerator;
use crate::compiler::instructions::Instructions;
use crate::compiler::lexer::WhitespaceConfig;
use crate::compiler::meta::find_undeclared;
use crate::compiler::parser::parse;
use crate::environment::Environment;
use crate::error::{attach_basic_debug_info, Error};
use crate::output::{Output, WriteWrapper};
use crate::syntax::SyntaxConfig;
use crate::utils::AutoEscape;
use crate::value::Value;
use crate::vm::{prepare_blocks, Context, State, Vm};

/// Callback for auto escape determination
pub type AutoEscapeFunc = dyn Fn(&str) -> AutoEscape + Sync + Send;

/// Internal struct that holds template loading level config values.
#[derive(Clone)]
pub struct TemplateConfig {
    /// The syntax used for the template.
    pub syntax_config: SyntaxConfig,
    /// Controls whitespace behavior.
    pub ws_config: WhitespaceConfig,
    /// The callback that determines the initial auto escaping for templates.
    pub default_auto_escape: Arc<AutoEscapeFunc>,
}

impl TemplateConfig {
    pub(crate) fn new(default_auto_escape: Arc<AutoEscapeFunc>) -> TemplateConfig {
        TemplateConfig {
            syntax_config: SyntaxConfig::default(),
            ws_config: WhitespaceConfig::default(),
            default_auto_escape,
        }
    }
}

/// Represents a handle to a template.
///
/// Templates are stored in the [`Environment`] as bytecode instructions.  With the
/// [`Environment::get_template`] method that is looked up and returned in form of
/// this handle.  Such a template can be cheaply copied as it only holds references.
///
/// To render the [`render`](Template::render) method can be used.
#[derive(Clone)]
pub struct Template<'env: 'source, 'source> {
    env: &'env Environment<'env>,
    pub(crate) compiled: CompiledTemplateRef<'env, 'source>,
}

struct CapturedData<'state> {
    output: String,
    state: State<'state, 'state>,
}

self_cell! {
    struct CapturedCell<'source> {
        owner: Template<'source, 'source>,

        #[covariant]
        dependent: CapturedData,
    }
}

/// Represents a rendered template output together with a captured [`State`].
///
/// This type keeps the originating [`Template`] alive together with its
/// [`State`].  This is useful in situations where a temporary template handle
/// is used and a state needs to be inspected later (for instance to call
/// exported macros).
///
/// When created from [`render_captured`](Template::render_captured)
/// the [`output`](Self::output) contains the rendered string.  When created from
/// [`render_captured_to`](Template::render_captured_to) the output is
/// an empty string as the output was written to the provided writer.
pub struct Captured<'source> {
    cell: CapturedCell<'source>,
}

impl fmt::Debug for Captured<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Captured")
            .field("output", &self.output())
            .field("state", &self.state())
            .finish()
    }
}

impl<'source> Captured<'source> {
    /// Returns the rendered output.
    ///
    /// When created from [`render_captured_to`](Template::render_captured_to)
    /// this returns an empty string.
    pub fn output(&self) -> &str {
        self.cell.borrow_dependent().output.as_str()
    }

    /// Returns a reference to the captured state.
    pub fn state(&self) -> &State<'_, '_> {
        &self.cell.borrow_dependent().state
    }

    /// Invokes a closure with mutable access to the captured state.
    pub fn with_state_mut<R>(
        &mut self,
        f: impl for<'state> FnOnce(&mut State<'state, 'state>) -> R,
    ) -> R {
        self.cell
            .with_dependent_mut(|_, dependent| f(&mut dependent.state))
    }

    /// Consumes the capture and returns the rendered output string.
    ///
    /// When created from [`render_captured_to`](Template::render_captured_to)
    /// this returns an empty string.
    pub fn into_output(mut self) -> String {
        self.cell
            .with_dependent_mut(|_, dependent| std::mem::take(&mut dependent.output))
    }
}

impl fmt::Debug for Template<'_, '_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut ds = f.debug_struct("Template");
        ds.field("name", &self.name());
        #[cfg(feature = "internal_debug")]
        {
            ds.field("instructions", &self.compiled.instructions);
            ds.field("blocks", &self.compiled.blocks);
        }
        ds.field("initial_auto_escape", &self.compiled.initial_auto_escape);
        ds.finish()
    }
}

impl<'env, 'source> Template<'env, 'source> {
    pub(crate) fn new(
        env: &'env Environment<'env>,
        compiled: CompiledTemplateRef<'env, 'source>,
    ) -> Template<'env, 'source> {
        Template { env, compiled }
    }

    /// Returns the name of the template.
    pub fn name(&self) -> &str {
        self.compiled.instructions.name()
    }

    /// Returns the source code of the template.
    pub fn source(&self) -> &str {
        self.compiled.instructions.source()
    }

    /// Renders the template into a string.
    ///
    /// The provided value is used as the initial context for the template.  It
    /// can be any object that implements [`Serialize`](serde::Serialize).  You
    /// can either create your own struct and derive `Serialize` for it or the
    /// [`context!`](crate::context) macro can be used to create an ad-hoc context.
    ///
    /// For very large contexts and to avoid the overhead of serialization of
    /// potentially unused values, you might consider using a dynamic
    /// [`Object`](crate::value::Object) as value.  For more
    /// information see [Map as Context](crate::value::Object#map-as-context).
    ///
    /// ```
    /// # use minijinja::{Environment, context};
    /// # let mut env = Environment::new();
    /// # env.add_template("hello", "Hello {{ name }}!").unwrap();
    /// let tmpl = env.get_template("hello").unwrap();
    /// println!("{}", tmpl.render(context!(name => "John")).unwrap());
    /// ```
    ///
    /// To render a single block use [`render_captured`](Self::render_captured)
    /// in combination with [`State::render_block`].
    ///
    /// **Note on values:** The [`Value`] type implements `Serialize` and can be
    /// efficiently passed to render.  It does not undergo actual serialization.
    pub fn render<S: Serialize>(&self, ctx: S) -> Result<String, Error> {
        // reduce total amount of code falling under mono morphization into
        // this function, and share the rest in _render.
        self._render(Value::from_serialize(&ctx)).map(|x| x.0)
    }

    /// Like [`render`](Self::render) but also return the evaluated [`State`].
    ///
    /// This can be used to inspect the [`State`] of the template post evaluation
    /// for instance to get fuel consumption numbers or to access globally set
    /// variables.
    ///
    /// ```
    /// # use minijinja::{Environment, context, value::Value};
    /// # let mut env = Environment::new();
    /// let tmpl = env.template_from_str("{% set x = 42 %}Hello {{ what }}!").unwrap();
    /// let (rv, state) = tmpl.render_and_return_state(context!{ what => "World" }).unwrap();
    /// assert_eq!(rv, "Hello World!");
    /// assert_eq!(state.lookup("x"), Some(Value::from(42)));
    /// ```
    ///
    /// **Note on values:** The [`Value`] type implements `Serialize` and can be
    /// efficiently passed to render.  It does not undergo actual serialization.
    #[deprecated(since = "2.18.0", note = "use render_captured instead")]
    pub fn render_and_return_state<S: Serialize>(
        &self,
        ctx: S,
    ) -> Result<(String, State<'_, 'env>), Error> {
        // reduce total amount of code falling under mono morphization into
        // this function, and share the rest in _render.
        self._render(Value::from_serialize(&ctx))
    }

    /// Like [`render`](Self::render) but also returns the evaluated [`State`]
    /// while keeping the template alive with the returned state.
    ///
    /// This is primarily useful when working with temporary template handles,
    /// as the resulting [`State`] can continue to be used through the returned
    /// wrapper.
    ///
    /// ```
    /// # use minijinja::{Environment, value::Value};
    /// let env = Environment::new();
    /// let rendered = env
    ///     .template_from_str("{% set x = 42 %}")
    ///     .unwrap()
    ///     .render_captured(())
    ///     .unwrap();
    /// assert_eq!(rendered.output(), "");
    /// assert_eq!(rendered.state().lookup("x"), Some(Value::from(42)));
    /// ```
    pub fn render_captured<S: Serialize>(&self, ctx: S) -> Result<Captured<'source>, Error> {
        self.clone()
            ._capture_state(Value::from_serialize(&ctx), true)
    }

    /// Like [`render`](Self::render) but writes to an [`io::Write`] and keeps
    /// the template alive with the returned state.
    ///
    /// This is useful when working with temporary template handles and
    /// the state needs to be inspected afterwards.  The [`output`](Captured::output)
    /// of the returned [`Captured`] will be an empty string since the
    /// output was written to the provided writer.
    ///
    /// ```
    /// # use minijinja::{Environment, context};
    /// let env = Environment::new();
    /// let mut buf = Vec::new();
    /// let captured = env
    ///     .template_from_str("{% set x = 42 %}Hello!")
    ///     .unwrap()
    ///     .render_captured_to((), &mut buf)
    ///     .unwrap();
    /// assert_eq!(std::str::from_utf8(&buf).unwrap(), "Hello!");
    /// assert_eq!(captured.output(), "");
    /// ```
    pub fn render_captured_to<S: Serialize, W: io::Write>(
        &self,
        ctx: S,
        w: W,
    ) -> Result<Captured<'source>, Error> {
        let root = Value::from_serialize(&ctx);
        let w = std::cell::RefCell::new(WriteWrapper { w, err: None });
        self.clone()
            ._capture_state_with_output(root, &w)
            .map_err(|err| w.into_inner().take_err(err))
    }

    fn _render(&self, root: Value) -> Result<(String, State<'_, 'env>), Error> {
        let mut rv = String::with_capacity(self.compiled.buffer_size_hint);
        self._eval(root, &mut Output::new(&mut rv))
            .map(|(_, state)| (rv, state))
    }

    fn _capture_state(self, root: Value, capture_output: bool) -> Result<Captured<'source>, Error> {
        let this: Template<'source, 'source> = self;
        let cell = ok!(CapturedCell::try_new(
            this,
            move |template| -> Result<CapturedData<'_>, Error> {
                let mut output = if capture_output {
                    String::with_capacity(template.compiled.buffer_size_hint)
                } else {
                    String::new()
                };
                let (_, state) = if capture_output {
                    ok!(template._eval(root, &mut Output::new(&mut output)))
                } else {
                    ok!(template._eval(root, &mut Output::null()))
                };
                Ok(CapturedData { output, state })
            }
        ));
        Ok(Captured { cell })
    }

    fn _capture_state_with_output<W: io::Write>(
        self,
        root: Value,
        w: &RefCell<WriteWrapper<W>>,
    ) -> Result<Captured<'source>, Error> {
        let this: Template<'source, 'source> = self;
        let cell = ok!(CapturedCell::try_new(
            this,
            move |template| -> Result<CapturedData<'_>, Error> {
                let (_, state) = ok!(template._eval(root, &mut Output::new(&mut *w.borrow_mut())));
                Ok(CapturedData {
                    output: String::new(),
                    state,
                })
            }
        ));
        Ok(Captured { cell })
    }

    /// Renders the template into an [`io::Write`].
    ///
    /// This works exactly like [`render`](Self::render), but writes the template
    /// into an [`io::Write`] as it is evaluated.
    ///
    /// ```
    /// # use minijinja::{Environment, context};
    /// # let mut env = Environment::new();
    /// # env.add_template("hello", "Hello {{ name }}!").unwrap();
    /// use std::io::stdout;
    ///
    /// let tmpl = env.get_template("hello").unwrap();
    /// tmpl.render_to_write(context!(name => "John"), &mut stdout()).unwrap();
    /// ```
    ///
    /// **Note on values:** The [`Value`] type implements `Serialize` and can be
    /// efficiently passed to render.  It does not undergo actual serialization.
    #[deprecated(since = "2.18.0", note = "use render_captured_to instead")]
    pub fn render_to_write<S: Serialize, W: io::Write>(
        &self,
        ctx: S,
        w: W,
    ) -> Result<State<'_, 'env>, Error> {
        let mut wrapper = WriteWrapper { w, err: None };
        self._eval(Value::from_serialize(&ctx), &mut Output::new(&mut wrapper))
            .map(|(_, state)| state)
            .map_err(|err| wrapper.take_err(err))
    }

    /// Evaluates the template into a [`State`].
    ///
    /// This evaluates the template, discards the output and returns the final
    /// `State` for introspection.  From there global variables or blocks
    /// can be accessed.  What this does is quite similar to how the engine
    /// internally works with templates that are extended or imported from.
    ///
    /// ```
    /// # use minijinja::{Environment, context};
    /// # fn test() -> Result<(), minijinja::Error> {
    /// # let mut env = Environment::new();
    /// # env.add_template("hello", "")?;
    /// let tmpl = env.get_template("hello")?;
    /// let state = tmpl.eval_to_state(context!(name => "John"))?;
    /// println!("{:?}", state.exports());
    /// # Ok(()) }
    /// ```
    ///
    /// If you also want to render, use [`render_captured`](Self::render_captured).
    ///
    /// For more information see [`State`].
    #[deprecated(since = "2.18.0", note = "use render_captured instead")]
    pub fn eval_to_state<S: Serialize>(&self, ctx: S) -> Result<State<'_, 'env>, Error> {
        let root = Value::from_serialize(&ctx);
        let mut out = Output::null();
        let vm = Vm::new(self.env);
        let state = ok!(vm.eval(
            &self.compiled.instructions,
            root,
            &self.compiled.blocks,
            &mut out,
            self.compiled.initial_auto_escape,
        ))
        .1;
        Ok(state)
    }

    fn _eval(
        &self,
        root: Value,
        out: &mut Output,
    ) -> Result<(Option<Value>, State<'_, 'env>), Error> {
        Vm::new(self.env).eval(
            &self.compiled.instructions,
            root,
            &self.compiled.blocks,
            out,
            self.compiled.initial_auto_escape,
        )
    }

    /// Returns a set of all undeclared variables in the template.
    ///
    /// This returns a set of all variables that might be looked up
    /// at runtime by the template.  Since this runs a static
    /// analysis, the actual control flow is not considered.  This
    /// also cannot take into account what happens due to includes,
    /// imports or extending.  If `nested` is set to `true`, then also
    /// nested trivial attribute lookups are considered and returned.
    ///
    /// ```rust
    /// # use minijinja::Environment;
    /// let mut env = Environment::new();
    /// env.add_template("x", "{% set x = foo %}{{ x }}{{ bar.baz }}").unwrap();
    /// let tmpl = env.get_template("x").unwrap();
    /// let undeclared = tmpl.undeclared_variables(false);
    /// // returns ["foo", "bar"]
    /// let undeclared = tmpl.undeclared_variables(true);
    /// // returns ["foo", "bar.baz"]
    /// ```
    ///
    /// Note that this does not special case global variables.  This means
    /// that for instance a template that uses `namespace()` will return
    /// `namespace` in the return value.
    pub fn undeclared_variables(&self, nested: bool) -> HashSet<String> {
        match parse(
            self.compiled.instructions.source(),
            self.name(),
            self.compiled.syntax_config.clone(),
            // TODO: this is not entirely great, but good enough for this use case.
            Default::default(),
        ) {
            Ok(ast) => find_undeclared(&ast, nested),
            Err(_) => HashSet::new(),
        }
    }

    /// Creates an empty [`State`] for this template.
    ///
    /// It's very rare that you need to actually do this but it can be useful when
    /// testing values or working with macros or other callable objects from outside
    /// the template environment.
    pub fn new_state(&self) -> State<'_, 'env> {
        State::new(
            Context::new(self.env),
            self.compiled.initial_auto_escape,
            &self.compiled.instructions,
            prepare_blocks(&self.compiled.blocks),
        )
    }

    /// Returns the instructions and blocks if the template is loaded from the
    /// environment.
    ///
    /// For templates loaded as string on the environment this API contract
    /// cannot be upheld because the template might not live long enough.  Under
    /// normal circumstances however such a template object would never make it
    /// to the callers of this API as this API is used for including or extending,
    /// both of which should only ever get access to a template from the environment
    /// which holds a borrowed ref.
    #[cfg(feature = "multi_template")]
    pub(crate) fn instructions_and_blocks(
        &self,
    ) -> Result<
        (
            &'env Instructions<'env>,
            &'env BTreeMap<&'env str, Instructions<'env>>,
        ),
        Error,
    > {
        match self.compiled {
            CompiledTemplateRef::Borrowed(x) => Ok((&x.instructions, &x.blocks)),
            CompiledTemplateRef::Owned(_) => Err(Error::new(
                crate::ErrorKind::InvalidOperation,
                "cannot extend or include template not borrowed from environment",
            )),
        }
    }

    /// Returns the initial auto escape setting.
    #[cfg(feature = "multi_template")]
    pub(crate) fn initial_auto_escape(&self) -> AutoEscape {
        self.compiled.initial_auto_escape
    }
}

#[derive(Clone)]
pub(crate) enum CompiledTemplateRef<'env: 'source, 'source> {
    Owned(Arc<CompiledTemplate<'source>>),
    Borrowed(&'env CompiledTemplate<'source>),
}

impl<'source> Deref for CompiledTemplateRef<'_, 'source> {
    type Target = CompiledTemplate<'source>;

    fn deref(&self) -> &Self::Target {
        match *self {
            CompiledTemplateRef::Owned(ref x) => x,
            CompiledTemplateRef::Borrowed(x) => x,
        }
    }
}

/// Represents a compiled template in memory.
pub struct CompiledTemplate<'source> {
    /// The root instructions.
    pub instructions: Instructions<'source>,
    /// Block local instructions.
    pub blocks: BTreeMap<&'source str, Instructions<'source>>,
    /// Optional size hint for string rendering.
    pub buffer_size_hint: usize,
    /// The syntax config that created it.
    pub syntax_config: SyntaxConfig,
    /// The initial setting of auto escaping.
    pub initial_auto_escape: AutoEscape,
}

impl fmt::Debug for CompiledTemplate<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut ds = f.debug_struct("CompiledTemplate");
        #[cfg(feature = "internal_debug")]
        {
            ds.field("instructions", &self.instructions);
            ds.field("blocks", &self.blocks);
        }
        ds.finish()
    }
}

impl<'source> CompiledTemplate<'source> {
    /// Creates a compiled template from name and source using the given settings.
    pub fn new(
        name: &'source str,
        source: &'source str,
        config: &TemplateConfig,
    ) -> Result<CompiledTemplate<'source>, Error> {
        attach_basic_debug_info(Self::_new_impl(name, source, config), source)
    }

    fn _new_impl(
        name: &'source str,
        source: &'source str,
        config: &TemplateConfig,
    ) -> Result<CompiledTemplate<'source>, Error> {
        let ast = ok!(parse(
            source,
            name,
            config.syntax_config.clone(),
            config.ws_config
        ));
        let mut g = CodeGenerator::new(name, source);
        g.compile_stmt(&ast);
        let buffer_size_hint = g.buffer_size_hint();
        let (instructions, blocks) = g.finish();
        Ok(CompiledTemplate {
            instructions,
            blocks,
            buffer_size_hint,
            syntax_config: config.syntax_config.clone(),
            initial_auto_escape: (config.default_auto_escape)(name),
        })
    }
}
