// `ok!` and `some!` are less bloaty alternatives to the standard library's try operator (`?`).
// Since we do not need type conversions in this crate we can fall back to much easier match
// patterns that compile faster and produce less bloaty code.

macro_rules! ok {
    ($expr:expr) => {
        match $expr {
            Ok(val) => val,
            Err(err) => return Err(err),
        }
    };
}

macro_rules! some {
    ($expr:expr) => {
        match $expr {
            Some(val) => val,
            None => return None,
        }
    };
}

/// Hidden utility module for the [`context!`](crate::context!) macro.
#[doc(hidden)]
pub mod __context {
    use crate::value::{Value, ValueMap};
    use crate::Environment;
    use std::rc::Rc;

    #[inline(always)]
    pub fn make() -> ValueMap {
        ValueMap::default()
    }

    #[inline(always)]
    pub fn add(ctx: &mut ValueMap, key: &'static str, value: Value) {
        ctx.insert(key.into(), value);
    }

    #[inline(always)]
    pub fn build(ctx: ValueMap) -> Value {
        Value::from_object(ctx)
    }

    pub fn thread_local_env() -> Rc<Environment<'static>> {
        thread_local! {
            static ENV: Rc<Environment<'static>> = Rc::new(Environment::new());
        }
        ENV.with(|x| x.clone())
    }
}

/// Creates a template context from keys and values or merging in another value.
///
/// ```rust
/// # use minijinja::context;
/// let ctx = context!{
///     name => "Peter",
///     location => "World",
/// };
/// ```
///
/// Alternatively if the variable name matches the key name it can
/// be omitted:
///
/// ```rust
/// # use minijinja::context;
/// let name = "Peter";
/// let ctx = context!{ name };
/// ```
///
/// The return value is a [`Value`](crate::value::Value).
///
/// Note that [`context!`](crate::context!) can also be used recursively if you need to
/// create nested objects:
///
/// ```rust
/// # use minijinja::context;
/// let ctx = context! {
///     nav => vec![
///         context!(path => "/", title => "Index"),
///         context!(path => "/downloads", title => "Downloads"),
///         context!(path => "/faq", title => "FAQ"),
///     ]
/// };
/// ```
///
/// Additionally the macro supports a second syntax that can merge other
/// contexts or values.  In that case one or more values need to be
/// passed with a leading `..` operator.  This is useful to supply extra
/// values into render in a common place.  The order of precedence is
/// left to right:
///
/// ```rust
/// # use minijinja::context;
/// let ctx = context! { a => "A" };
/// let ctx = context! { ..ctx, ..context! {
///     b => "B"
/// }};
///
/// // or
///
/// let ctx = context! {
///     a => "A",
///     ..context! {
///         b => "B"
///     }
/// };
/// ```
///
/// The merge works with an value, not just values created by the `context!`
/// macro and is performed lazy.  This means it also works with dynamic
/// [`Object`](crate::value::Object)s.  The merge uses the underlying
/// [`merge_maps`](crate::value::merge_maps) function.
///
/// # Note on Conversions
///
/// This macro uses [`Value::from_serialize`](crate::Value::from_serialize)
/// for conversions.
///
/// This macro currently does not move passed values.  Future versions of
/// MiniJinja are going to change the move behavior and it's recommended to not
/// depend on this implicit reference behavior.  You should thus pass values
/// with `&value` if you intend on still being able to reference them
/// after the macro invocation.
#[macro_export]
macro_rules! context {
    () => {
        $crate::__context::build($crate::__context::make())
    };
    (
        $($key:ident $(=> $value:expr)?),*
        $(, .. $ctx:expr),* $(,)?
    ) => {{
        let mut ctx = $crate::__context::make();
        $(
            $crate::__context_pair!(ctx, $key $(=> $value)?);
        )*
        let ctx = $crate::__context::build(ctx);
        let merge_ctx = [
            $(
                $crate::value::Value::from($ctx),
            )*
        ];
        if merge_ctx.is_empty() {
            ctx
        } else {
            $crate::value::merge_maps(
                merge_ctx.into_iter().rev().chain(::std::iter::once(ctx)))
        }
    }};
    (
        $(.. $ctx:expr),* $(,)?
    ) => {{
        $crate::value::merge_maps([
            $(
                $crate::value::Value::from($ctx),
            )*
        ].into_iter().rev())
    }};
}

#[macro_export]
#[doc(hidden)]
macro_rules! __context_pair {
    ($ctx:ident, $key:ident) => {{
        $crate::__context_pair!($ctx, $key => $key);
    }};
    ($ctx:ident, $key:ident => $value:expr) => {
        $crate::__context::add(
            &mut $ctx,
            stringify!($key),
            $crate::value::Value::from_serialize(&$value),
        );
    };
}

/// An utility macro to create arguments for function calls.
///
/// This creates a slice of values on the stack which can be
/// passed to [`call`](crate::value::Value::call),
/// [`call_method`](crate::value::Value::call_method),
/// [`apply_filter`](crate::State::apply_filter),
/// [`perform_test`](crate::State::perform_test) or similar
/// APIs that take slices of values.
///
/// It supports both positional and keyword arguments.
/// To mark a parameter as keyword argument define it as
/// `name => value`, otherwise just use `value`.
///
/// ```
/// # use minijinja::{value::Value, args, Environment};
/// # let env = Environment::default();
/// # let state = &env.empty_state();
/// # let value = Value::from(());
/// value.call(state, args!(1, 2, foo => "bar"));
/// ```
///
/// Note that this like [`context!`](crate::context) goes through
/// [`Value::from_serialize`](crate::value::Value::from_serialize).
#[macro_export]
macro_rules! args {
    () => { &[][..] as &[$crate::value::Value] };
    ($($arg:tt)*) => { $crate::__args_helper!(branch [[$($arg)*]], [$($arg)*]) };
}

/// Utility macro for `args!`
#[macro_export]
#[doc(hidden)]
macro_rules! __args_helper {
    // branch helper between `args` and `kwargs`.
    //
    // It analyzes the first bracket enclosed tt bundle to see if kwargs are
    // used.  If yes, it uses `kwargs` to handle the second tt
    // bundle, otherwise it uses `args`.
    (branch [[]], $args:tt) => { $crate::__args_helper!(args $args) };
    (branch [[$n:ident => $e:expr]], $args:tt) => { $crate::__args_helper!(kwargs $args) };
    (branch [[$n:ident => $e:expr, $($r:tt)*]], $args:tt) => { $crate::__args_helper!(kwargs $args) };
    (branch [[$e:expr]], $args:tt) => { $crate::__args_helper!(args $args) };
    (branch [[$e:expr, $($rest:tt)*]], $args:tt) => { $crate::__args_helper!(branch [[$($rest)*]], $args) };

    // creates args on the stack
    (args [$($arg:tt)*]) => {&{
        let mut args = Vec::<$crate::value::Value>::new();
        $crate::__args_helper!(peel args, args, false, [$($arg)*]);
        args
    }[..]};

    // creates args with kwargs on the stack
    (kwargs [$($arg:tt)*]) => {&{
        let mut args = Vec::<$crate::value::Value>::new();
        let mut kwargs = Vec::<(&str, $crate::value::Value)>::new();
        $crate::__args_helper!(peel args, kwargs, false, [$($arg)*]);
        args.push($crate::value::Kwargs::from_iter(kwargs.into_iter()).into());
        args
    }[..]};

    // Peels a single argument from the arguments and stuffs them into
    // `$args` or `$kwargs` depending on type.
    (peel $args:ident, $kwargs:ident, $has_kwargs:ident, []) => {};
    (peel $args:ident, $kwargs:ident, $has_kwargs:ident, [$name:ident => $expr:expr]) => {
        $kwargs.push((stringify!($name), $crate::value::Value::from_serialize(&$expr)));
    };
    (peel $args:ident, $kwargs:ident, $has_kwargs:ident, [$name:ident => $expr:expr, $($rest:tt)*]) => {
        $kwargs.push((stringify!($name), $crate::value::Value::from_serialize(&$expr)));
        $crate::__args_helper!(peel $args, $kwargs, true, [$($rest)*]);
    };
    (peel $args:ident, $kwargs:ident, false, [$expr:expr]) => {
        $args.push($crate::value::Value::from_serialize(&$expr));
    };
    (peel $args:ident, $kwargs:ident, false, [$expr:expr, $($rest:tt)*]) => {
        $args.push($crate::value::Value::from_serialize(&$expr));
        $crate::__args_helper!(peel $args, $kwargs, false, [$($rest)*]);
    };
}

/// A macro similar to [`format!`] but that uses MiniJinja for rendering.
///
/// This can be used to quickly render a MiniJinja template into a string
/// without having to create an environment first which can be useful in
/// some situations.  Note however that the template is re-parsed every
/// time the [`render!`](crate::render) macro is called which is potentially
/// slow.
///
/// There are two forms for this macro.  The default form takes template
/// source and context variables, the extended form also lets you provide
/// a custom environment that should be used rather than a default one.
/// The context variables are passed the same way as with the
/// [`context!`](crate::context) macro.
///
/// # Example
///
/// Passing context explicitly:
///
/// ```
/// # use minijinja::render;
/// println!("{}", render!("Hello {{ name }}!", name => "World"));
/// ```
///
/// Passing variables with the default name:
///
/// ```
/// # use minijinja::render;
/// let name = "World";
/// println!("{}", render!("Hello {{ name }}!", name));
/// ```
///
/// Passing an explicit environment:
///
/// ```
/// # use minijinja::{Environment, render};
/// let env = Environment::new();
/// println!("{}", render!(in env, "Hello {{ name }}!", name => "World"));
/// ```
///
/// # Panics
///
/// This macro panics if the format string is an invalid template or the
/// template evaluation failed.
#[macro_export]
macro_rules! render {
    (
        in $env:expr,
        $tmpl:expr
        $(, $key:ident $(=> $value:expr)?)* $(,)?
    ) => {
        ($env).render_str($tmpl, $crate::context! { $($key $(=> $value)? ,)* })
            .expect("failed to render expression")
    };
    (
        $tmpl:expr
        $(, $key:ident $(=> $value:expr)?)* $(,)?
    ) => {
        $crate::render!(in $crate::__context::thread_local_env(), $tmpl, $($key $(=> $value)? ,)*)
    }
}
