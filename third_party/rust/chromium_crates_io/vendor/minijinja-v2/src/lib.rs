//! <div align=center>
//!   <img src="https://github.com/mitsuhiko/minijinja/raw/main/artwork/logo.png" alt="" width=320>
//!   <p><strong>MiniJinja: a powerful template engine for Rust with minimal dependencies</strong></p>
//! </div>
//!
//! MiniJinja is a powerful but minimal dependency template engine for Rust which
//! is based on the syntax and behavior of the
//! [Jinja2](https://jinja.palletsprojects.com/) template engine for Python.  It's
//! implemented on top of [`serde`].  The goal is to be able to render a large
//! chunk of the Jinja2 template ecosystem from Rust with a minimal engine and to
//! leverage an already existing ecosystem of editor integrations.
//!
//! ```jinja
//! {% for user in users %}
//!   <li>{{ user.name }}</li>
//! {% endfor %}
//! ```
//!
//! You can play with MiniJinja online [in the browser
//! playground](https://mitsuhiko.github.io/minijinja-playground/) powered by a
//! WASM build of MiniJinja.
//!
//! # Why MiniJinja
//!
//! MiniJinja by its name wants to be a good default choice if you need a little
//! bit of templating with minimal dependencies.  It has the following goals:
//!
//! * Well documented, compact API
//! * Minimal dependencies, reasonable compile times and decent runtime performance
//! * Stay close as possible to Jinja2
//! * Support for expression evaluation
//! * Support for all `serde` compatible types
//! * Excellent test coverage
//! * Support for dynamic runtime objects with methods and dynamic attributes
//!
//! # Template Usage
//!
//! To use MiniJinja one needs to create an [`Environment`] and populate it with
//! templates.  Afterwards templates can be loaded and rendered.  To pass data
//! one can pass any serde serializable value.  The [`context!`] macro can be
//! used to quickly construct a template context:
//!
//! ```
//! use minijinja::{Environment, context};
//!
//! let mut env = Environment::new();
//! env.add_template("hello", "Hello {{ name }}!").unwrap();
//! let tmpl = env.get_template("hello").unwrap();
//! println!("{}", tmpl.render(context!(name => "John")).unwrap());
//! ```
//!
//! ```plain
//! Hello John!
//! ```
//!
//! For super trivial cases where you need to render a string once, you can
//! also use the [`render!`] macro which acts a bit like a replacement
//! for the [`format!`] macro.
//!
//! # Expression Usage
//!
//! MiniJinja — like Jinja2 — allows to be used as expression language.  This can be
//! useful to express logic in configuration files or similar things.  For this
//! purpose the [`Environment::compile_expression`] method can be used.  It returns
//! an expression object that can then be evaluated, returning the result:
//!
//! ```
//! use minijinja::{Environment, context};
//!
//! let env = Environment::new();
//! let expr = env.compile_expression("number < 42").unwrap();
//! let result = expr.eval(context!(number => 23)).unwrap();
//! assert_eq!(result.is_true(), true);
//! ```
//!
//! This becomes particularly powerful when [dynamic objects](crate::value::Object) are
//! exposed to templates.
//!
//! # Custom Filters
//!
//! MiniJinja lets you register functions as filter functions (see
//! [`Function`](crate::functions::Function)) with the engine.  These can then be
//! invoked directly from the template:
//!
//! ```
//! use minijinja::{Environment, context};
//!
//! let mut env = Environment::new();
//! env.add_filter("repeat", str::repeat);
//! env.add_template("hello", "{{ 'Na '|repeat(3) }} {{ name }}!").unwrap();
//! let tmpl = env.get_template("hello").unwrap();
//! println!("{}", tmpl.render(context!(name => "Batman")).unwrap());
//! ```
//!
//! ```plain
//! Na Na Na Batman!
//! ```
//!
//! # Learn more
//!
//! - [`Environment`]: the main API entry point.  Teaches you how to configure the environment.
//! - [`Template`]: the template object API.  Shows you how templates can be rendered.
//! - [`syntax`]: provides documentation of the template engine syntax.
//! - [`filters`]: teaches you how to write custom filters and to see the list of built-in filters.
//! - [`tests`]: teaches you how to write custom test functions and to see the list of built-in tests.
//! - [`functions`]: teaches how to write custom functions and to see the list of built-in functions.
//! - For auto reloading use the [`minijinja-autoreload`](https://docs.rs/minijinja-autoreload) crate.
//! - For simpler embedding of templates use the [`minijinja-embed`](https://docs.rs/minijinja-embed) crate.
//!
//! Additionally there is an [list of examples](https://github.com/mitsuhiko/minijinja/tree/main/examples)
//! with many different small example programs on GitHub to explore.
//!
//! # Error Handling
//!
//! MiniJinja tries to give you good errors out of the box.  However if you use includes or
//! template inheritance your experience will improve greatly if you ensure to render chained
//! errors.  For more information see [`Error`] with an example.
//!
//! # Size and Compile Times
//!
//! MiniJinja attempts to compile fast so it can be used as a sensible template engine choice
//! when compile times matter.  Because of this it's internally modular so unnecessary bits and
//! pieces can be removed.  The default set of features tries to strike a balance but in
//! situations where only a subset is needed (eg: `build.rs`) all default features can be
//! be disabled.
//!
//! # Optional Features
//!
//! MiniJinja comes with a lot of optional features, some of which are turned on by
//! default.  If you plan on using MiniJinja in a library, please consider turning
//! off all default features and to opt-in explicitly into the ones you actually
//! need.
//!
//! <details><summary><strong style="cursor: pointer">Configurable Features</strong></summary>
//!
//! To cut down on size of the engine some default functionality can be removed:
//!
//! - **Engine Features:**
//!
//!   - `builtins`: if this feature is removed the default filters, tests and
//!     functions are not implemented.
//!   - `macros`: when removed the `{% macro %}` tag is not included.
//!   - `multi_template`: when removed the templates related to imports and extends
//!     are removed (`{% from %}`, `{% import %}`, `{% include %}`, and `{% extends %}`
//!     as well as `{% block %}`).
//!   - `adjacent_loop_items`: when removed the `previtem` and `nextitem` attributes of
//!     the `loop` object no longer exist.  Removing this feature can provide faster
//!     template execution when a lot of loops are involved.
//!   - `unicode`: when added unicode identifiers are supported and the `sort`
//!     filter's case insensitive comparison changes to using unicode and not
//!     ASCII rules.  Without this features only ASCII identifiers can be used
//!     for variable names and attributes.
//!   - `serde`: enables or disables serde support.  In current versions of MiniJinja
//!     it's not possible to disable serde but it will become possible.  To prevent
//!     breakage, MiniJinja warns if this feature is disabled.
//!
//! - **Rust Functionality:**
//!
//!   - `debug`: if this feature is removed some debug functionality of the engine is
//!     removed as well.  This mainly affects the quality of error reporting.
//!   - `deserialization`: when removed this disables deserialization support for
//!     the [`Value`] type, removes the `ViaDeserialize` type and the error type
//!     no longer implements `serde::de::Error`.
//!   - `std_collections`: if this feature is removed some [`Object`](crate::value::Object)
//!     implementations for standard library collections are removed.  Only the
//!     ones needed for the engine to function itself are retained.
//!
//! There are some additional features that provide extra functionality:
//!
//! - `fuel`: enables the `fuel` feature which makes the engine track fuel consumption which
//!   can be used to better protect against expensive templates.
//! - `loader`: enables owned and dynamic template loading of templates.
//! - `custom_syntax`: when this feature is enabled, custom delimiters are supported by
//!   the parser.
//! - `preserve_order`: When enable the internal value implementation uses an indexmap
//!   which preserves the original order of maps and structs.
//! - `json`: When enabled the `tojson` filter is added as builtin filter as well as
//!   the ability to auto escape via `AutoEscape::Json`.
//! - `urlencode`: When enabled the `urlencode` filter is added as builtin filter.
//! - `loop_controls`: enables the `{% break %}` and `{% continue %}` loop control flow
//!   tags.
//!
//! Performance and memory related features:
//!
//! - `stacker`: enables automatic stack growth which permits much larger levels of recursion
//!   at runtime.  This does not affect the maximum recursion at parsing time which is always
//!   limited.
//! - `speedups`: enables all speedups, in particular it turns on the `v_htmlescape` dependency
//!   for faster HTML escaping.
//!
//! Internals:
//!
//! - `unstable_machinery`: exposes an unstable internal API (no semver guarantees) to parse
//!   templates and interact with the engine internals.  If you need this functionality, please
//!   leave some feedback on GitHub.
//!
//! </details>
#![allow(clippy::cognitive_complexity)]
#![allow(clippy::get_first)]
#![allow(clippy::default_constructed_unit_structs)]
#![allow(clippy::needless_borrowed_reference)]
#![allow(clippy::vec_init_then_push)]
#![cfg_attr(docsrs, feature(doc_cfg))]
#![deny(missing_docs)]
#![doc(html_logo_url = "https://github.com/mitsuhiko/minijinja/raw/main/artwork/logo-square.png")]

#[macro_use]
mod macros;

mod compiler;
mod defaults;
mod environment;
mod error;
mod expression;
#[cfg(feature = "builtins")]
mod format_utils;
mod output;
mod template;
mod utils;
mod vm;

pub mod filters;
pub mod functions;
pub mod syntax;
pub mod tests;
pub mod value;

#[cfg(feature = "loader")]
mod loader;

#[cfg(feature = "loader")]
pub use loader::path_loader;

#[cfg(feature = "debug")]
mod debug;

pub use self::defaults::{default_auto_escape_callback, escape_formatter};
pub use self::environment::Environment;
pub use self::error::{Error, ErrorKind};
pub use self::expression::Expression;
pub use self::output::Output;
pub use self::template::Template;
pub use self::utils::{AutoEscape, HtmlEscape, UndefinedBehavior};

#[cfg(feature = "builtins")]
pub use self::format_utils::{format_filter, FormatStyle};

/// Re-export for convenience.
pub use self::value::Value;

pub use self::macros::__context;
pub use self::vm::State;

/// This module gives access to the low level machinery.
///
/// This module is only provided by the `unstable_machinery` feature and does not
/// have a stable interface.  It mostly exists for internal testing purposes and
/// for debugging.
#[cfg(feature = "unstable_machinery")]
#[cfg_attr(docsrs, doc(cfg(feature = "unstable_machinery")))]
pub mod machinery {
    #![allow(missing_docs)]
    pub use crate::compiler::ast;
    pub use crate::compiler::codegen::CodeGenerator;
    pub use crate::compiler::instructions::{Instruction, Instructions};
    pub use crate::compiler::lexer::{tokenize, Tokenizer, WhitespaceConfig};
    pub use crate::compiler::parser::{parse, parse_expr};
    pub use crate::compiler::tokens::{Span, Token};
    pub use crate::template::{CompiledTemplate, TemplateConfig};
    pub use crate::vm::Vm;

    use crate::Output;

    /// Returns a reference to a [`CompiledTemplate`] from a [`Template`](crate::Template).
    pub fn get_compiled_template<'x, 'env>(
        tmpl: &'x crate::Template<'env, 'env>,
    ) -> &'x CompiledTemplate<'env> {
        &tmpl.compiled
    }

    /// Creates an [`Output`] that writes into a string.
    pub fn make_string_output(s: &mut String) -> Output<'_> {
        Output::new(s)
    }
}
