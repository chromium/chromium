use std::borrow::Cow;
use std::collections::BTreeMap;
use std::fmt;
use std::sync::Arc;

use serde::Serialize;

use crate::compiler::codegen::CodeGenerator;
use crate::compiler::instructions::Instructions;
use crate::compiler::parser::parse_expr;
use crate::error::{attach_basic_debug_info, Error, ErrorKind};
use crate::expression::Expression;
use crate::output::Output;
use crate::template::{CompiledTemplate, CompiledTemplateRef, Template, TemplateConfig};
use crate::utils::{AutoEscape, BTreeMapKeysDebug, UndefinedBehavior};
use crate::value::{FunctionArgs, FunctionResult, UndefinedType, Value, ValueRepr};
use crate::vm::State;
use crate::{defaults, functions};

type FormatterFunc = dyn Fn(&mut Output, &State, &Value) -> Result<(), Error> + Sync + Send;
type PathJoinFunc = dyn for<'s> Fn(&'s str, &'s str) -> Cow<'s, str> + Sync + Send;
type UnknownMethodFunc =
    dyn Fn(&State, &Value, &str, &[Value]) -> Result<Value, Error> + Sync + Send;

/// The maximum recursion in the VM.  Normally each stack frame
/// adds one to this counter (eg: every time a frame is added).
/// However in some situations more depth is pushed if the cost
/// of the stack frame is higher.  Raising this above this limit
/// requires enabling the `stacker` feature.
const MAX_RECURSION: usize = 500;

/// An abstraction that holds the engine configuration.
///
/// This object holds the central configuration state for templates.  It is also
/// the container for all loaded templates.
///
/// The environment holds references to the source the templates were created from.
/// This makes it very inconvenient to pass around unless the templates are static
/// strings.
///
/// There are generally two ways to construct an environment:
///
/// * [`Environment::new`] creates an environment preconfigured with sensible
///   defaults.  It will contain all built-in filters, tests and globals as well
///   as a callback for auto escaping based on file extension.
/// * [`Environment::empty`] creates a completely blank environment.
#[derive(Clone)]
pub struct Environment<'source> {
    templates: TemplateStore<'source>,
    filters: BTreeMap<Cow<'source, str>, Value>,
    tests: BTreeMap<Cow<'source, str>, Value>,
    globals: BTreeMap<Cow<'source, str>, Value>,
    path_join_callback: Option<Arc<PathJoinFunc>>,
    pub(crate) unknown_method_callback: Option<Arc<UnknownMethodFunc>>,
    undefined_behavior: UndefinedBehavior,
    formatter: Arc<FormatterFunc>,
    #[cfg(feature = "debug")]
    debug: bool,
    #[cfg(feature = "fuel")]
    fuel: Option<u64>,
    recursion_limit: usize,
}

impl Default for Environment<'_> {
    fn default() -> Self {
        Environment::empty()
    }
}

impl fmt::Debug for Environment<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Environment")
            .field("globals", &self.globals)
            .field("tests", &BTreeMapKeysDebug(&self.tests))
            .field("filters", &BTreeMapKeysDebug(&self.filters))
            .field("templates", &self.templates)
            .finish()
    }
}

impl<'source> Environment<'source> {
    /// Creates a new environment with sensible defaults.
    ///
    /// This environment does not yet contain any templates but it will have all
    /// the default filters, tests and globals loaded.  If you do not want any
    /// default configuration you can use the alternative
    /// [`empty`](Environment::empty) method.
    #[cfg_attr(
        not(feature = "serde"),
        deprecated(
            since = "2.0.4",
            note = "Attempted to instantiate an environment with serde.  Future \
        versions of MiniJinja will require enabling the 'serde' feature to use \
        serde types.  To silence this warning add 'serde' to the list of features of minijinja."
        )
    )]
    pub fn new() -> Environment<'source> {
        Environment {
            templates: TemplateStore::new(TemplateConfig::new(Arc::new(
                defaults::default_auto_escape_callback,
            ))),
            filters: defaults::get_builtin_filters(),
            tests: defaults::get_builtin_tests(),
            globals: defaults::get_globals(),
            path_join_callback: None,
            unknown_method_callback: None,
            undefined_behavior: UndefinedBehavior::default(),
            formatter: Arc::new(defaults::escape_formatter),
            #[cfg(feature = "debug")]
            debug: cfg!(debug_assertions),
            #[cfg(feature = "fuel")]
            fuel: None,
            recursion_limit: MAX_RECURSION,
        }
    }

    /// Creates a completely empty environment.
    ///
    /// This environment has no filters, no templates, no globals and no default
    /// logic for auto escaping configured.
    pub fn empty() -> Environment<'source> {
        Environment {
            templates: TemplateStore::new(TemplateConfig::new(Arc::new(defaults::no_auto_escape))),
            filters: Default::default(),
            tests: Default::default(),
            globals: Default::default(),
            path_join_callback: None,
            unknown_method_callback: None,
            undefined_behavior: UndefinedBehavior::default(),
            formatter: Arc::new(defaults::escape_formatter),
            #[cfg(feature = "debug")]
            debug: cfg!(debug_assertions),
            #[cfg(feature = "fuel")]
            fuel: None,
            recursion_limit: MAX_RECURSION,
        }
    }

    /// Loads a template from a string into the environment.
    ///
    /// The `name` parameter defines the name of the template which identifies
    /// it.  To look up a loaded template use the [`get_template`](Self::get_template)
    /// method.
    ///
    /// ```
    /// # use minijinja::Environment;
    /// let mut env = Environment::new();
    /// env.add_template("index.html", "Hello {{ name }}!").unwrap();
    /// ```
    /// This method fails if the template has a syntax error.
    ///
    /// Note that there are situations where the interface of this method is
    /// too restrictive as you need to hold on to the strings for the lifetime
    /// of the environment. To avoid this restriction use
    /// [`add_template_owned`](Self::add_template_owned), which is available
    /// when the `loader` feature is enabled.
    pub fn add_template(&mut self, name: &'source str, source: &'source str) -> Result<(), Error> {
        self.templates.insert(name, source)
    }

    /// Adds a template without borrowing.
    ///
    /// This lets you place an owned [`String`] in the environment rather than the
    /// borrowed `&str` without having to worry about lifetimes.
    ///
    /// ```
    /// # use minijinja::Environment;
    /// let mut env = Environment::new();
    /// env.add_template_owned("index.html".to_string(), "Hello {{ name }}!".to_string()).unwrap();
    /// ```
    ///
    /// **Note**: the name is a bit of a misnomer as this API also allows to borrow too as
    /// the parameters are actually [`Cow`].  This method fails if the template has a syntax error.
    #[cfg(feature = "loader")]
    #[cfg_attr(docsrs, doc(cfg(feature = "loader")))]
    pub fn add_template_owned<N, S>(&mut self, name: N, source: S) -> Result<(), Error>
    where
        N: Into<Cow<'source, str>>,
        S: Into<Cow<'source, str>>,
    {
        self.templates.insert_cow(name.into(), source.into())
    }

    /// Register a template loader as source of templates.
    ///
    /// When a template loader is registered, the environment gains the ability
    /// to dynamically load templates.  The loader is invoked with the name of
    /// the template.  If this template exists `Ok(Some(template_source))` has
    /// to be returned, otherwise `Ok(None)`.  Once a template has been loaded
    /// it's stored on the environment.  This means the loader is only invoked
    /// once per template name.
    ///
    /// For loading templates from the file system, you can use the
    /// [`path_loader`](crate::path_loader) function.
    ///
    /// # Example
    ///
    /// ```rust
    /// # use minijinja::Environment;
    /// fn create_env() -> Environment<'static> {
    ///     let mut env = Environment::new();
    ///     env.set_loader(|name| {
    ///         if name == "layout.html" {
    ///             Ok(Some("...".into()))
    ///         } else {
    ///             Ok(None)
    ///         }
    ///     });
    ///     env
    /// }
    /// ```
    #[cfg(feature = "loader")]
    #[cfg_attr(docsrs, doc(cfg(feature = "loader")))]
    pub fn set_loader<F>(&mut self, f: F)
    where
        F: Fn(&str) -> Result<Option<String>, Error> + Send + Sync + 'static,
    {
        self.templates.set_loader(f);
    }

    /// Preserve the trailing newline when rendering templates.
    ///
    /// The default is `false`, which causes a single newline, if present, to be
    /// stripped from the end of the template.
    ///
    /// This setting is used whenever a template is loaded into the environment.
    /// Changing it at a later point only affects future templates loaded.
    pub fn set_keep_trailing_newline(&mut self, yes: bool) {
        self.templates
            .template_config
            .ws_config
            .keep_trailing_newline = yes;
    }

    /// Returns the value of the trailing newline preservation flag.
    pub fn keep_trailing_newline(&self) -> bool {
        self.templates
            .template_config
            .ws_config
            .keep_trailing_newline
    }

    /// Remove the first newline after a block.
    ///
    /// If this is set to `true` then the first newline after a block is removed
    /// (block, not variable tag!). Defaults to `false`.
    pub fn set_trim_blocks(&mut self, yes: bool) {
        self.templates.template_config.ws_config.trim_blocks = yes;
    }

    /// Returns the value of the trim blocks flag.
    pub fn trim_blocks(&self) -> bool {
        self.templates.template_config.ws_config.trim_blocks
    }

    /// Remove leading spaces and tabs from the start of a line to a block.
    ///
    /// If this is set to `true` then leading spaces and tabs from the start of a line
    /// to the block tag are removed.
    pub fn set_lstrip_blocks(&mut self, yes: bool) {
        self.templates.template_config.ws_config.lstrip_blocks = yes;
    }

    /// Returns the value of the lstrip blocks flag.
    pub fn lstrip_blocks(&self) -> bool {
        self.templates.template_config.ws_config.lstrip_blocks
    }

    /// Removes a template by name.
    pub fn remove_template(&mut self, name: &str) {
        self.templates.remove(name);
    }

    /// Sets a callback to join template paths.
    ///
    /// By default this returns the template path unchanged, but it can be used
    /// to implement relative path resolution between templates.  The first
    /// argument to the callback is the name of the template to be loaded, the
    /// second argument is the parent path.
    ///
    /// The following example demonstrates how a basic path joining algorithm
    /// can be implemented.
    ///
    /// ```
    /// # let mut env = minijinja::Environment::new();
    /// env.set_path_join_callback(|name, parent| {
    ///     let mut rv = parent.split('/').collect::<Vec<_>>();
    ///     rv.pop();
    ///     name.split('/').for_each(|segment| match segment {
    ///         "." => {}
    ///         ".." => { rv.pop(); }
    ///         _ => { rv.push(segment); }
    ///     });
    ///     rv.join("/").into()
    /// });
    /// ```
    pub fn set_path_join_callback<F>(&mut self, f: F)
    where
        F: for<'s> Fn(&'s str, &'s str) -> Cow<'s, str> + Send + Sync + 'static,
    {
        self.path_join_callback = Some(Arc::new(f));
    }

    /// Sets a callback invoked for unknown methods on objects.
    ///
    /// This registers a function with the environment that is invoked when invoking a method
    /// on a value results in a [`UnknownMethod`](crate::ErrorKind::UnknownMethod) error.
    /// In that case the callback is invoked with [`State`], the [`Value`], the name of
    /// the method as `&str` as well as all arguments in a slice.
    ///
    /// This for instance implements a `.items()` method that invokes the `|items` filter:
    ///
    /// ```rust
    /// use minijinja::value::{ValueKind, from_args};
    /// use minijinja::{Error, ErrorKind};
    /// # let mut env = minijinja::Environment::new();
    ///
    /// env.set_unknown_method_callback(|state, value, method, args| {
    ///     if value.kind() == ValueKind::Map && method == "items" {
    ///         let _: () = from_args(args)?;
    ///         state.apply_filter("items", &[value.clone()])
    ///     } else {
    ///         Err(Error::from(ErrorKind::UnknownMethod))
    ///     }
    /// });
    /// ```
    ///
    /// This can be used to increase the compatibility with Jinja2 templates that might
    /// call Python methods on objects which are not available in minijinja.  A range of
    /// common Python methods is implemented in `minijinja-contrib`.  For more information
    /// see [minijinja_contrib::pycompat](https://docs.rs/minijinja-contrib/latest/minijinja_contrib/pycompat/).
    pub fn set_unknown_method_callback<F>(&mut self, f: F)
    where
        F: Fn(&State, &Value, &str, &[Value]) -> Result<Value, Error> + Sync + Send + 'static,
    {
        self.unknown_method_callback = Some(Arc::new(f));
    }

    /// Removes all stored templates.
    ///
    /// This method is mainly useful when combined with a loader as it causes
    /// the loader to "reload" templates.  By calling this method one can trigger
    /// a reload.
    pub fn clear_templates(&mut self) {
        self.templates.clear();
    }

    /// Returns an iterator over the already loaded templates and their names.
    ///
    /// Only templates that are already loaded will be returned.
    ///
    /// ```
    /// # use minijinja::{Environment, context};
    /// let mut env = Environment::new();
    /// env.add_template("hello.txt", "Hello {{ name }}!").unwrap();
    /// env.add_template("goodbye.txt", "Goodbye {{ name }}!").unwrap();
    ///
    /// # assert_eq!(env.templates().count(), 2);
    /// for (name, tmpl) in env.templates() {
    ///     println!("{}", tmpl.render(context!{ name => "World" }).unwrap());
    /// }
    /// ```
    pub fn templates(&self) -> impl Iterator<Item = (&str, Template<'_, '_>)> {
        self.templates.iter().map(|(name, template)| {
            let template = Template::new(self, CompiledTemplateRef::Borrowed(template));
            (name, template)
        })
    }

    /// Fetches a template by name.
    ///
    /// This requires that the template has been loaded with
    /// [`add_template`](Environment::add_template) beforehand.  If the template was
    /// not loaded an error of kind `TemplateNotFound` is returned.  If a loader was
    /// added to the engine this can also dynamically load templates.
    ///
    /// ```
    /// # use minijinja::{Environment, context};
    /// let mut env = Environment::new();
    /// env.add_template("hello.txt", "Hello {{ name }}!").unwrap();
    /// let tmpl = env.get_template("hello.txt").unwrap();
    /// println!("{}", tmpl.render(context!{ name => "World" }).unwrap());
    /// ```
    pub fn get_template(&self, name: &str) -> Result<Template<'_, '_>, Error> {
        let compiled = ok!(self.templates.get(name));
        Ok(Template::new(self, CompiledTemplateRef::Borrowed(compiled)))
    }

    /// Loads a template from a string.
    ///
    /// In some cases you really only need to work with (eg: render) a template to be
    /// rendered once only.
    ///
    /// ```
    /// # use minijinja::{Environment, context};
    /// let env = Environment::new();
    /// let tmpl = env.template_from_named_str("template_name", "Hello {{ name }}").unwrap();
    /// let rv = tmpl.render(context! { name => "World" });
    /// println!("{}", rv.unwrap());
    /// ```
    pub fn template_from_named_str(
        &self,
        name: &'source str,
        source: &'source str,
    ) -> Result<Template<'_, 'source>, Error> {
        Ok(Template::new(
            self,
            CompiledTemplateRef::Owned(Arc::new(ok!(CompiledTemplate::new(
                name,
                source,
                &self.templates.template_config,
            )))),
        ))
    }

    /// Loads a template from a string, with name `<string>`.
    ///
    /// This is a shortcut to [`template_from_named_str`](Self::template_from_named_str)
    /// with name set to `<string>`.
    pub fn template_from_str(&self, source: &'source str) -> Result<Template<'_, 'source>, Error> {
        self.template_from_named_str("<string>", source)
    }

    /// Parses and renders a template from a string in one go with name.
    ///
    /// Like [`render_str`](Self::render_str), but provide a name for the
    /// template to be used instead of the default `<string>`.  This is an
    /// alias for [`template_from_named_str`](Self::template_from_named_str) paired with
    /// [`render`](Template::render).
    ///
    /// ```
    /// # use minijinja::{Environment, context};
    /// let env = Environment::new();
    /// let rv = env.render_named_str(
    ///     "template_name",
    ///     "Hello {{ name }}",
    ///     context!{ name => "World" }
    /// );
    /// println!("{}", rv.unwrap());
    /// ```
    ///
    /// **Note on values:** The [`Value`] type implements `Serialize` and can be
    /// efficiently passed to render.  It does not undergo actual serialization.
    pub fn render_named_str<S: Serialize>(
        &self,
        name: &str,
        source: &str,
        ctx: S,
    ) -> Result<String, Error> {
        ok!(self.template_from_named_str(name, source)).render(ctx)
    }

    /// Parses and renders a template from a string in one go.
    ///
    /// In some cases you really only need a template to be rendered once from
    /// a string and returned.  The internal name of the template is `<string>`.
    ///
    /// This is an alias for [`template_from_str`](Self::template_from_str) paired with
    /// [`render`](Template::render).
    ///
    /// **Note on values:** The [`Value`] type implements `Serialize` and can be
    /// efficiently passed to render.  It does not undergo actual serialization.
    pub fn render_str<S: Serialize>(&self, source: &str, ctx: S) -> Result<String, Error> {
        // reduce total amount of code falling under mono morphization into
        // this function, and share the rest in _eval.
        ok!(self.template_from_str(source)).render(ctx)
    }

    /// Sets a new function to select the default auto escaping.
    ///
    /// This function is invoked when templates are loaded into the environment
    /// to determine the default auto escaping behavior.  The function is
    /// invoked with the name of the template and can make an initial auto
    /// escaping decision based on that.  The default implementation
    /// ([`default_auto_escape_callback`](defaults::default_auto_escape_callback))
    /// turn on escaping depending on the file extension.
    ///
    /// ```
    /// # use minijinja::{Environment, AutoEscape};
    /// # let mut env = Environment::new();
    /// env.set_auto_escape_callback(|name| {
    ///     if matches!(name.rsplit('.').next().unwrap_or(""), "html" | "htm" | "aspx") {
    ///         AutoEscape::Html
    ///     } else {
    ///         AutoEscape::None
    ///     }
    /// });
    /// ```
    pub fn set_auto_escape_callback<F>(&mut self, f: F)
    where
        F: Fn(&str) -> AutoEscape + 'static + Sync + Send,
    {
        self.templates.template_config.default_auto_escape = Arc::new(f);
    }

    /// Changes the undefined behavior.
    ///
    /// This changes the runtime behavior of [`undefined`](Value::UNDEFINED) values in
    /// the template engine.  For more information see [`UndefinedBehavior`].  The
    /// default is [`UndefinedBehavior::Lenient`].
    pub fn set_undefined_behavior(&mut self, behavior: UndefinedBehavior) {
        self.undefined_behavior = behavior;
    }

    /// Returns the current undefined behavior.
    ///
    /// This is particularly useful if a filter function or similar wants to change its
    /// behavior with regards to undefined values.
    #[inline(always)]
    pub fn undefined_behavior(&self) -> UndefinedBehavior {
        self.undefined_behavior
    }

    /// Sets a different formatter function.
    ///
    /// The formatter is invoked to format the given value into the provided
    /// [`Output`].  The default implementation is
    /// [`escape_formatter`](defaults::escape_formatter).
    ///
    /// When implementing a custom formatter it depends on if auto escaping
    /// should be supported or not.  If auto escaping should be supported then
    /// it's easiest to just wrap the default formatter.  The
    /// following example swaps out `None` values before rendering for
    /// `Undefined` which renders as an empty string instead.
    ///
    /// The current value of the auto escape flag can be retrieved directly
    /// from the [`State`].
    ///
    /// ```
    /// # use minijinja::Environment;
    /// # let mut env = Environment::new();
    /// use minijinja::escape_formatter;
    /// use minijinja::value::Value;
    ///
    /// env.set_formatter(|out, state, value| {
    ///     escape_formatter(
    ///         out,
    ///         state,
    ///         if value.is_none() {
    ///             &Value::UNDEFINED
    ///         } else {
    ///             value
    ///         },
    ///     )
    ///});
    /// # assert_eq!(env.render_str("{{ none }}", ()).unwrap(), "");
    /// ```
    pub fn set_formatter<F>(&mut self, f: F)
    where
        F: Fn(&mut Output, &State, &Value) -> Result<(), Error> + 'static + Sync + Send,
    {
        self.formatter = Arc::new(f);
    }

    /// Enable or disable the debug mode.
    ///
    /// When the debug mode is enabled the engine will dump out some of the
    /// execution state together with the source information of the executing
    /// template when an error is created.  The cost of this is relatively
    /// high as the data including the template source is cloned.
    ///
    /// When this is enabled templates will print debug information with source
    /// context when the error is printed.
    ///
    /// This requires the `debug` feature.  This is enabled by default if
    /// debug assertions are enabled and false otherwise.
    #[cfg(feature = "debug")]
    #[cfg_attr(docsrs, doc(cfg(feature = "debug")))]
    pub fn set_debug(&mut self, enabled: bool) {
        self.debug = enabled;
    }

    /// Returns the current value of the debug flag.
    #[cfg(feature = "debug")]
    pub fn debug(&self) -> bool {
        self.debug
    }

    /// Sets the optional fuel of the engine.
    ///
    /// When MiniJinja is compiled with the `fuel` feature then every
    /// instruction consumes a certain amount of fuel.  Usually `1`, some will
    /// consume no fuel.  By default the engine has the fuel feature disabled
    /// (`None`).  To turn on fuel set something like `Some(50000)` which will
    /// allow 50.000 instructions to execute before running out of fuel.
    ///
    /// To find out how much fuel is consumed, you can access the fuel levels
    /// from the [`State`](crate::State).
    ///
    /// Fuel consumed per-render.
    #[cfg(feature = "fuel")]
    #[cfg_attr(docsrs, doc(cfg(feature = "fuel")))]
    pub fn set_fuel(&mut self, fuel: Option<u64>) {
        self.fuel = fuel;
    }

    /// Returns the configured fuel.
    #[cfg(feature = "fuel")]
    #[cfg_attr(docsrs, doc(cfg(feature = "fuel")))]
    pub fn fuel(&self) -> Option<u64> {
        self.fuel
    }

    /// Sets the syntax for the environment.
    ///
    /// This setting is used whenever a template is loaded into the environment.
    /// Changing it at a later point only affects future templates loaded.
    ///
    /// See [`SyntaxConfig`](crate::syntax::SyntaxConfig) for more information.
    #[cfg(feature = "custom_syntax")]
    #[cfg_attr(docsrs, doc(cfg(feature = "custom_syntax")))]
    pub fn set_syntax(&mut self, syntax: crate::syntax::SyntaxConfig) {
        self.templates.template_config.syntax_config = syntax;
    }

    /// Returns the current syntax config.
    #[cfg(feature = "custom_syntax")]
    #[cfg_attr(docsrs, doc(cfg(feature = "custom_syntax")))]
    pub fn syntax(&self) -> &crate::syntax::SyntaxConfig {
        &self.templates.template_config.syntax_config
    }

    /// Reconfigures the runtime recursion limit.
    ///
    /// This defaults to `500`.  Raising it above that level requires the `stacker`
    /// feature to be enabled.  Otherwise the limit is silently capped at that safe
    /// maximum.  Note that the maximum is not necessarily safe if the thread uses
    /// a lot of stack space already, it's just a value that was validated once to
    /// provide basic protection.
    ///
    /// Every operation that requires recursion in MiniJinja increments an internal
    /// recursion counter.  The actual cost attributed to that recursion depends on
    /// the cost of the operation.  If statements and for loops for instance only
    /// increase the counter by 1, whereas template includes and macros might increase
    /// it to 10 or more.
    ///
    /// **Note on stack growth:** even if the stacker feature is enabled it does not
    /// mean that in all cases stack can grow to the limits desired.  For instance in
    /// WASM the maximum limits are additionally enforced by the runtime.
    pub fn set_recursion_limit(&mut self, level: usize) {
        #[cfg(not(feature = "stacker"))]
        {
            self.recursion_limit = level.min(MAX_RECURSION);
        }
        #[cfg(feature = "stacker")]
        {
            self.recursion_limit = level;
        }
    }

    /// Returns the current max recursion limit.
    pub fn recursion_limit(&self) -> usize {
        self.recursion_limit
    }

    /// Compiles an expression.
    ///
    /// This lets one compile an expression in the template language and
    /// receive the output.  This lets one use the expressions of the language
    /// be used as a minimal scripting language.  For more information and an
    /// example see [`Expression`].
    pub fn compile_expression(&self, expr: &'source str) -> Result<Expression<'_, 'source>, Error> {
        self._compile_expression(expr)
            .map(|instr| Expression::new(self, instr))
    }

    /// Compiles an expression without capturing the lifetime.
    ///
    /// This works exactly like [`compile_expression`](Self::compile_expression) but
    /// lets you pass an owned string without capturing the lifetime.
    #[cfg(feature = "loader")]
    #[cfg_attr(docsrs, doc(cfg(feature = "loader")))]
    pub fn compile_expression_owned<E>(&self, expr: E) -> Result<Expression<'_, 'source>, Error>
    where
        E: Into<Cow<'source, str>>,
    {
        crate::loader::OwnedInstructions::try_new(Box::from(expr.into()), |expr| {
            self._compile_expression(expr)
        })
        .map(|instr| Expression::new_owned(self, instr))
    }

    fn _compile_expression<'expr>(&self, expr: &'expr str) -> Result<Instructions<'expr>, Error> {
        attach_basic_debug_info(
            parse_expr(expr).map(|ast| {
                let mut g = CodeGenerator::new("<expression>", expr);
                g.compile_expr(&ast);
                g.finish().0
            }),
            expr,
        )
    }

    /// Adds a new filter function.
    ///
    /// Filter functions are functions that can be applied to values in
    /// templates.  For details about filters have a look at
    /// [`filters`](crate::filters).
    pub fn add_filter<N, F, Rv, Args>(&mut self, name: N, f: F)
    where
        N: Into<Cow<'source, str>>,
        F: functions::Function<Rv, Args>,
        Rv: FunctionResult,
        Args: for<'a> FunctionArgs<'a>,
    {
        self.filters.insert(name.into(), Value::from_function(f));
    }

    /// Removes a filter by name.
    pub fn remove_filter(&mut self, name: &str) {
        self.filters.remove(name);
    }

    /// Adds a new test function.
    ///
    /// Test functions are similar to filters but perform a check on a value
    /// where the return value is always true or false.  For details about tests
    /// have a look at [`tests`](crate::tests).
    pub fn add_test<N, F, Rv, Args>(&mut self, name: N, f: F)
    where
        N: Into<Cow<'source, str>>,
        F: functions::Function<Rv, Args>,
        Rv: FunctionResult,
        Args: for<'a> FunctionArgs<'a>,
    {
        self.tests.insert(name.into(), Value::from_function(f));
    }

    /// Removes a test by name.
    pub fn remove_test(&mut self, name: &str) {
        self.tests.remove(name);
    }

    /// Adds a new global function.
    ///
    /// For details about functions have a look at [`functions`].  Note that
    /// functions and other global variables share the same namespace.
    /// For more details about functions have a look at
    /// [`Function`](crate::functions::Function).
    ///
    /// This is a shortcut for calling [`add_global`](Self::add_global) with the
    /// function wrapped with [`Value::from_function`].
    pub fn add_function<N, F, Rv, Args>(&mut self, name: N, f: F)
    where
        N: Into<Cow<'source, str>>,
        F: functions::Function<Rv, Args>,
        Rv: FunctionResult,
        Args: for<'a> FunctionArgs<'a>,
    {
        self.add_global(name.into(), Value::from_function(f))
    }

    /// Adds a global variable.
    pub fn add_global<N, V>(&mut self, name: N, value: V)
    where
        N: Into<Cow<'source, str>>,
        V: Into<Value>,
    {
        self.globals.insert(name.into(), value.into());
    }

    /// Removes a global function or variable by name.
    pub fn remove_global(&mut self, name: &str) {
        self.globals.remove(name);
    }

    /// Returns an iterator of all global variables.
    pub fn globals(&self) -> impl Iterator<Item = (&str, Value)> {
        self.globals
            .iter()
            .map(|(key, value)| (key as &str, value.clone()))
    }

    /// Returns an empty [`State`] for testing purposes and similar.
    pub fn empty_state(&self) -> State<'_, '_> {
        State::new_for_env(self)
    }

    /// Looks up a global.
    pub(crate) fn get_global(&self, name: &str) -> Option<Value> {
        self.globals.get(name).cloned()
    }

    /// Looks up a filter.
    pub(crate) fn get_filter(&self, name: &str) -> Option<&Value> {
        self.filters.get(name)
    }

    /// Looks up a test function.
    pub(crate) fn get_test(&self, name: &str) -> Option<&Value> {
        self.tests.get(name)
    }

    pub(crate) fn initial_auto_escape(&self, name: &str) -> AutoEscape {
        (self.templates.template_config.default_auto_escape)(name)
    }

    /// Formats a value into the final format.
    ///
    /// This step is called finalization in Jinja2 but since we are writing into
    /// the output stream rather than converting values, it's renamed to format
    /// here.
    pub(crate) fn format(
        &self,
        value: &Value,
        state: &State,
        out: &mut Output,
    ) -> Result<(), Error> {
        match (self.undefined_behavior, &value.0) {
            // this intentionally does not check for SilentUndefined.  SilentUndefined is
            // exclusively used in the missing else condition of an if expression to match
            // Jinja2 behavior.  Those go straight to the formatter.
            (
                UndefinedBehavior::Strict | UndefinedBehavior::SemiStrict,
                &ValueRepr::Undefined(UndefinedType::Default),
            ) => Err(Error::from(ErrorKind::UndefinedError)),
            _ => (self.formatter)(out, state, value),
        }
    }

    /// Performs a template path join.
    pub(crate) fn join_template_path<'s>(&self, name: &'s str, parent: &'s str) -> Cow<'s, str> {
        match self.path_join_callback {
            Some(ref cb) => cb(name, parent),
            None => Cow::Borrowed(name),
        }
    }
}

#[cfg(not(feature = "loader"))]
mod basic_store {
    use super::*;

    #[derive(Clone)]
    pub(crate) struct BasicStore<'source> {
        pub template_config: TemplateConfig,
        map: BTreeMap<&'source str, Arc<CompiledTemplate<'source>>>,
    }

    impl fmt::Debug for BasicStore<'_> {
        fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
            BTreeMapKeysDebug(&self.map).fmt(f)
        }
    }

    impl<'source> BasicStore<'source> {
        pub fn new(template_config: TemplateConfig) -> BasicStore<'source> {
            BasicStore {
                template_config,
                map: BTreeMap::default(),
            }
        }

        pub fn insert(&mut self, name: &'source str, source: &'source str) -> Result<(), Error> {
            self.map.insert(
                name,
                Arc::new(ok!(CompiledTemplate::new(
                    name,
                    source,
                    &self.template_config
                ))),
            );
            Ok(())
        }

        pub fn remove(&mut self, name: &str) {
            self.map.remove(name);
        }

        pub fn clear(&mut self) {
            self.map.clear();
        }

        pub fn get(&self, name: &str) -> Result<&CompiledTemplate<'source>, Error> {
            self.map
                .get(name)
                .map(|x| &**x)
                .ok_or_else(|| Error::new_not_found(name))
        }

        pub fn iter(&self) -> impl Iterator<Item = (&str, &CompiledTemplate<'source>)> {
            self.map.iter().map(|(name, template)| (*name, &**template))
        }
    }
}

#[cfg(not(feature = "loader"))]
use self::basic_store::BasicStore as TemplateStore;

#[cfg(feature = "loader")]
use crate::loader::LoaderStore as TemplateStore;
