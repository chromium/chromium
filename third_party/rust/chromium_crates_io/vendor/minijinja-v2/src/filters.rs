//! Filter functions and abstractions.
//!
//! MiniJinja inherits from Jinja2 the concept of filter functions.  These are functions
//! which are applied to values to modify them.  For example the expression `{{ 42|filter(23) }}`
//! invokes the filter `filter` with the arguments `42` and `23`.
//!
//! MiniJinja comes with some built-in filters that are listed below. To create a
//! custom filter write a function that takes at least a value, then registers it
//! with [`add_filter`](crate::Environment::add_filter).
//!
//! # Using Filters
//!
//! Using filters in templates is possible in all places an expression is permitted.
//! This means they are not just used for printing but also are useful for iteration
//! or similar situations.
//!
//! Motivating example:
//!
//! ```jinja
//! <dl>
//! {% for key, value in config|items %}
//!   <dt>{{ key }}
//!   <dd><pre>{{ value|tojson }}</pre>
//! {% endfor %}
//! </dl>
//! ```
//!
//! # Custom Filters
//!
//! A custom filter is just a simple function which accepts its inputs
//! as parameters and then returns a new value.  For instance the following
//! shows a filter which takes an input value and replaces whitespace with
//! dashes and converts it to lowercase:
//!
//! ```
//! # use minijinja::Environment;
//! # let mut env = Environment::new();
//! fn slugify(value: String) -> String {
//!     value.to_lowercase().split_whitespace().collect::<Vec<_>>().join("-")
//! }
//!
//! env.add_filter("slugify", slugify);
//! ```
//!
//! MiniJinja will perform the necessary conversions automatically.  For more
//! information see the [`Function`](crate::functions::Function) trait.
//!
//! # Accessing State
//!
//! In some cases it can be necessary to access the execution [`State`].  Since a borrowed
//! state implements [`ArgType`](crate::value::ArgType) it's possible to add a
//! parameter that holds the state.  For instance the following filter appends
//! the current template name to the string:
//!
//! ```
//! # use minijinja::Environment;
//! # let mut env = Environment::new();
//! use minijinja::{Value, State};
//!
//! fn append_template(state: &State, value: &Value) -> String {
//!     format!("{}-{}", value, state.name())
//! }
//!
//! env.add_filter("append_template", append_template);
//! ```
//!
//! # Filter configuration
//!
//! The recommended pattern for filters to change their behavior is to leverage global
//! variables in the template.  For instance take a filter that performs date formatting.
//! You might want to change the default time format format on a per-template basis
//! without having to update every filter invocation.  In this case the recommended
//! pattern is to reserve upper case variables and look them up in the filter:
//!
//! ```
//! # use minijinja::Environment;
//! # let mut env = Environment::new();
//! # fn format_unix_timestamp(_: f64, _: &str) -> String { "".into() }
//! use minijinja::State;
//!
//! fn timeformat(state: &State, ts: f64) -> String {
//!     let configured_format = state.lookup("TIME_FORMAT");
//!     let format = configured_format
//!         .as_ref()
//!         .and_then(|x| x.as_str())
//!         .unwrap_or("HH:MM:SS");
//!     format_unix_timestamp(ts, format)
//! }
//!
//! env.add_filter("timeformat", timeformat);
//! ```
//!
//! This then later lets a user override the default either by using
//! [`add_global`](crate::Environment::add_global) or by passing it with the
//! [`context!`] macro or similar.
//!
//! ```
//! # use minijinja::context;
//! # let other_variables = context!{};
//! let ctx = context! {
//!     TIME_FORMAT => "HH:MM",
//!     ..other_variables
//! };
//! ```
//!
//! # Built-in Filters
//!
//! When the `builtins` feature is enabled a range of built-in filters are
//! automatically added to the environment.  These are also all provided in
//! this module.  Note though that these functions are not to be
//! called from Rust code as their exact interface (arguments and return types)
//! might change from one MiniJinja version to another.
//!
//! Some additional filters are available in the
//! [`minijinja-contrib`](https://crates.io/crates/minijinja-contrib) crate.
use crate::error::Error;
use crate::utils::write_escaped;
use crate::value::Value;
use crate::vm::State;
use crate::{AutoEscape, Output};

/// Deprecated alias
#[deprecated = "Use the minijinja::functions::Function instead"]
#[doc(hidden)]
pub use crate::functions::Function as Filter;

/// Marks a value as safe.  This converts it into a string.
///
/// When a value is marked as safe, no further auto escaping will take place.
pub fn safe(v: String) -> Value {
    Value::from_safe_string(v)
}

/// Escapes a string.  By default to HTML.
///
/// By default this filter is also registered under the alias `e`.  Note that
/// this filter escapes with the format that is native to the format or HTML
/// otherwise.  This means that if the auto escape setting is set to
/// `Json` for instance then this filter will serialize to JSON instead.
pub fn escape(state: &State, v: &Value) -> Result<Value, Error> {
    if v.is_safe() {
        return Ok(v.clone());
    }

    // this tries to use the escaping flag of the current scope, then
    // of the initial state and if that is also not set it falls back
    // to HTML.
    let auto_escape = match state.auto_escape() {
        AutoEscape::None => match state.env().initial_auto_escape(state.name()) {
            AutoEscape::None => AutoEscape::Html,
            other => other,
        },
        other => other,
    };
    let mut rv = match v.as_str() {
        Some(s) => String::with_capacity(s.len()),
        None => String::new(),
    };
    let mut out = Output::new(&mut rv);
    ok!(write_escaped(&mut out, auto_escape, v));
    Ok(Value::from_safe_string(rv))
}

#[cfg(feature = "builtins")]
mod builtins {
    use super::*;

    use crate::error::ErrorKind;
    use crate::format_utils::{format_filter, FormatStyle};
    use crate::utils::{safe_sort, splitn_whitespace};
    use crate::value::merge_object::{MergeDict, MergeSeq};
    use crate::value::ops::{self, as_f64, LenIterWrap};
    use crate::value::{Enumerator, Kwargs, Object, ObjectRepr, Rest, ValueKind, ValueRepr};
    use std::borrow::Cow;
    use std::cmp::Ordering;
    use std::fmt::Write;
    use std::mem;
    use std::sync::Arc;

    /// Converts a value to uppercase.
    ///
    /// ```jinja
    /// <h1>{{ chapter.title|upper }}</h1>
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn upper(v: Cow<'_, str>) -> String {
        v.to_uppercase()
    }

    /// Converts a value to lowercase.
    ///
    /// ```jinja
    /// <h1>{{ chapter.title|lower }}</h1>
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn lower(v: Cow<'_, str>) -> String {
        v.to_lowercase()
    }

    /// Converts a value to title case.
    ///
    /// ```jinja
    /// <h1>{{ chapter.title|title }}</h1>
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn title(v: Cow<'_, str>) -> String {
        let mut rv = String::new();
        let mut capitalize = true;
        for c in v.chars() {
            if c.is_ascii_punctuation() || c.is_whitespace() {
                rv.push(c);
                capitalize = true;
            } else if capitalize {
                write!(rv, "{}", c.to_uppercase()).unwrap();
                capitalize = false;
            } else {
                write!(rv, "{}", c.to_lowercase()).unwrap();
            }
        }
        rv
    }

    /// Convert the string with all its characters lowercased
    /// apart from the first char which is uppercased.
    ///
    /// ```jinja
    /// <h1>{{ chapter.title|capitalize }}</h1>
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn capitalize(text: Cow<'_, str>) -> String {
        let mut chars = text.chars();
        match chars.next() {
            None => String::new(),
            Some(f) => f.to_uppercase().collect::<String>() + &chars.as_str().to_lowercase(),
        }
    }

    /// Does a string replace.
    ///
    /// It replaces all occurrences of the first parameter with the second.
    ///
    /// ```jinja
    /// {{ "Hello World"|replace("Hello", "Goodbye") }}
    ///   -> Goodbye World
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn replace(
        _state: &State,
        v: Cow<'_, str>,
        from: Cow<'_, str>,
        to: Cow<'_, str>,
    ) -> String {
        v.replace(&from as &str, &to as &str)
    }

    /// Returns the "length" of the value
    ///
    /// By default this filter is also registered under the alias `count`.
    ///
    /// ```jinja
    /// <p>Search results: {{ results|length }}
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn length(v: &Value) -> Result<usize, Error> {
        v.len().ok_or_else(|| {
            Error::new(
                ErrorKind::InvalidOperation,
                format!("cannot calculate length of value of type {}", v.kind()),
            )
        })
    }

    fn cmp_helper(a: &Value, b: &Value, case_sensitive: bool, reverse: bool) -> Ordering {
        let ordering = if !case_sensitive {
            if let (Some(a), Some(b)) = (a.as_str(), b.as_str()) {
                #[cfg(feature = "unicode")]
                {
                    unicase::UniCase::new(a).cmp(&unicase::UniCase::new(b))
                }
                #[cfg(not(feature = "unicode"))]
                {
                    a.to_ascii_lowercase().cmp(&b.to_ascii_lowercase())
                }
            } else {
                a.cmp(b)
            }
        } else {
            a.cmp(b)
        };

        if reverse {
            ordering.reverse()
        } else {
            ordering
        }
    }

    /// Dict sorting functionality.
    ///
    /// This filter works like `|items` but sorts the pairs by key first.
    ///
    /// The filter accepts a few keyword arguments:
    ///
    /// * `case_sensitive`: set to `true` to make the sorting of strings case sensitive.
    /// * `by`: set to `"value"` to sort by value. Defaults to `"key"`.
    /// * `reverse`: set to `true` to sort in reverse.
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn dictsort(v: &Value, kwargs: Kwargs) -> Result<Value, Error> {
        if v.kind() != ValueKind::Map {
            return Err(Error::new(
                ErrorKind::InvalidOperation,
                "cannot convert value into pair list",
            ));
        }

        let by_value = matches!(ok!(kwargs.get("by")), Some("value"));
        let case_sensitive = ok!(kwargs.get::<Option<bool>>("case_sensitive")).unwrap_or(false);
        let reverse = ok!(kwargs.get::<Option<bool>>("reverse")).unwrap_or(false);
        let mut rv: Vec<_> = ok!(v.try_iter())
            .map(|key| (key.clone(), v.get_item(&key).unwrap_or(Value::UNDEFINED)))
            .collect();
        safe_sort(&mut rv, |a, b| {
            let (a, b) = if by_value { (&a.1, &b.1) } else { (&a.0, &b.0) };
            cmp_helper(a, b, case_sensitive, reverse)
        })?;
        kwargs.assert_all_used()?;
        Ok(rv
            .into_iter()
            .map(|(k, v)| Value::from(vec![k, v]))
            .collect())
    }

    /// Returns an iterable of pairs (items) from a mapping.
    ///
    /// This can be used to iterate over keys and values of a mapping
    /// at once.  Note that this will use the original order of the map
    /// which is typically arbitrary unless the `preserve_order` feature
    /// is used in which case the original order of the map is retained.
    /// It's generally better to use `|dictsort` which sorts the map by
    /// key before iterating.
    ///
    /// ```jinja
    /// <dl>
    /// {% for key, value in my_dict|items %}
    ///   <dt>{{ key }}
    ///   <dd>{{ value }}
    /// {% endfor %}
    /// </dl>
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn items(v: &Value) -> Result<Value, Error> {
        if v.kind() == ValueKind::Map {
            Ok(Value::make_object_iterable(v.clone(), |v| {
                match v.as_object().and_then(|v| v.try_iter_pairs()) {
                    Some(iter) => Box::new(iter.map(|(key, value)| Value::from(vec![key, value]))),
                    None => Box::new(
                        // this really should not happen unless the object changes it's shape
                        // after the initial check
                        Some(Value::from(Error::new(
                            ErrorKind::InvalidOperation,
                            format!("{} is not iterable", v.kind()),
                        )))
                        .into_iter(),
                    ),
                }
            }))
        } else {
            Err(Error::new(
                ErrorKind::InvalidOperation,
                "cannot convert value into pairs",
            ))
        }
    }

    /// Reverses an iterable or string
    ///
    /// ```jinja
    /// {% for user in users|reverse %}
    ///   <li>{{ user.name }}
    /// {% endfor %}
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn reverse(v: &Value) -> Result<Value, Error> {
        v.reverse()
    }

    /// Trims a string.
    ///
    /// By default, it trims leading and trailing whitespaces:
    ///
    /// ```jinja
    /// {{ "  non-space characters  " | trim }} -> "non-space characters"
    /// ```
    ///
    /// You can also remove a character sequence.  All the prefixes and suffixes
    /// matching the sequence are removed:
    ///
    /// ```jinja
    /// {{ "1212foo12bar1212" | trim("12") }} -> "foo12bar"
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn trim(s: Cow<'_, str>, chars: Option<Cow<'_, str>>) -> String {
        match chars {
            Some(chars) => {
                let chars = chars.chars().collect::<Vec<_>>();
                s.trim_matches(&chars[..]).to_string()
            }
            None => s.trim().to_string(),
        }
    }

    /// Joins a sequence by a character
    ///
    /// ```jinja
    /// {{ "Foo Bar Baz" | join(", ") }} -> foo, bar, baz
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn join(val: &Value, joiner: Option<Cow<'_, str>>) -> Result<String, Error> {
        if val.is_undefined() || val.is_none() {
            return Ok(String::new());
        }

        let joiner = joiner.as_ref().unwrap_or(&Cow::Borrowed(""));
        let iter = ok!(val.try_iter().map_err(|err| {
            Error::new(
                ErrorKind::InvalidOperation,
                format!("cannot join value of type {}", val.kind()),
            )
            .with_source(err)
        }));

        let mut rv = String::new();
        for (idx, item) in iter.enumerate() {
            if idx > 0 {
                rv.push_str(joiner);
            }
            if let Some(s) = item.as_str() {
                rv.push_str(s);
            } else {
                write!(rv, "{item}").ok();
            }
        }
        Ok(rv)
    }

    /// Split a string into its substrings, using `split` as the separator string.
    ///
    /// If `split` is not provided or `none` the string is split at all whitespace
    /// characters and multiple spaces and empty strings will be removed from the
    /// result.
    ///
    /// The `maxsplits` parameter defines the maximum number of splits
    /// (starting from the left).  Note that this follows Python conventions
    /// rather than Rust ones so `1` means one split and two resulting items.
    ///
    /// ```jinja
    /// {{ "hello world"|split|list }}
    ///     -> ["hello", "world"]
    ///
    /// {{ "c,s,v"|split(",")|list }}
    ///     -> ["c", "s", "v"]
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn split(s: Arc<str>, split: Option<Arc<str>>, maxsplits: Option<i64>) -> Value {
        let maxsplits = maxsplits.and_then(|x| if x >= 0 { Some(x as usize + 1) } else { None });

        Value::make_object_iterable((s, split), move |(s, split)| match (split, maxsplits) {
            (None, None) => Box::new(s.split_whitespace().map(Value::from)),
            (Some(split), None) => Box::new(s.split(split as &str).map(Value::from)),
            (None, Some(n)) => Box::new(splitn_whitespace(s, n).map(Value::from)),
            (Some(split), Some(n)) => Box::new(s.splitn(n, split as &str).map(Value::from)),
        })
    }

    /// Splits a string into lines.
    ///
    /// The newline character is removed in the process and not retained.  This
    /// function supports both Windows and UNIX style newlines.
    ///
    /// ```jinja
    /// {{ "foo\nbar\nbaz"|lines }}
    ///     -> ["foo", "bar", "baz"]
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn lines(s: Arc<str>) -> Value {
        Value::from_iter(s.lines().map(|x| x.to_string()))
    }

    /// If the value is undefined it will return the passed default value,
    /// otherwise the value of the variable:
    ///
    /// ```jinja
    /// <p>{{ my_variable|default("my_variable was not defined") }}</p>
    /// ```
    ///
    /// Setting the optional second parameter to `true` will also treat falsy
    /// values as undefined, e.g. empty strings:
    ///
    /// ```jinja
    /// <p>{{ ""|default("string was empty", true) }}</p>
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn default(value: &Value, other: Option<Value>, lax: Option<bool>) -> Value {
        if value.is_undefined() {
            other.unwrap_or_else(|| Value::from(""))
        } else if lax.unwrap_or(false) && !value.is_true() {
            other.unwrap_or_else(|| Value::from(""))
        } else {
            value.clone()
        }
    }

    /// Returns the absolute value of a number.
    ///
    /// ```jinja
    /// |a - b| = {{ (a - b)|abs }}
    ///   -> |2 - 4| = 2
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn abs(value: Value) -> Result<Value, Error> {
        match value.0 {
            ValueRepr::U64(_) | ValueRepr::U128(_) => Ok(value),
            ValueRepr::I64(x) => match x.checked_abs() {
                Some(rv) => Ok(Value::from(rv)),
                None => Ok(Value::from((x as i128).abs())), // this cannot overflow
            },
            ValueRepr::I128(x) => {
                x.0.checked_abs()
                    .map(Value::from)
                    .ok_or_else(|| Error::new(ErrorKind::InvalidOperation, "overflow on abs"))
            }
            ValueRepr::F64(x) => Ok(Value::from(x.abs())),
            _ => Err(Error::new(
                ErrorKind::InvalidOperation,
                "cannot get absolute value",
            )),
        }
    }

    /// Converts a value into an integer.
    ///
    /// ```jinja
    /// {{ "42"|int == 42 }} -> true
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn int(value: &Value) -> Result<Value, Error> {
        match &value.0 {
            ValueRepr::Undefined(_) | ValueRepr::None => Ok(Value::from(0)),
            ValueRepr::Bool(x) => Ok(Value::from(*x as u64)),
            ValueRepr::U64(_) | ValueRepr::I64(_) | ValueRepr::U128(_) | ValueRepr::I128(_) => {
                Ok(value.clone())
            }
            ValueRepr::F64(v) => Ok(Value::from(*v as i128)),
            ValueRepr::String(..) | ValueRepr::SmallStr(_) => {
                let s = value.as_str().unwrap();
                if let Ok(i) = s.parse::<i128>() {
                    Ok(Value::from(i))
                } else {
                    match s.parse::<f64>() {
                        Ok(f) => Ok(Value::from(f as i128)),
                        Err(err) => Err(Error::new(ErrorKind::InvalidOperation, err.to_string())),
                    }
                }
            }
            ValueRepr::Bytes(_) | ValueRepr::Object(_) => Err(Error::new(
                ErrorKind::InvalidOperation,
                format!("cannot convert {} to integer", value.kind()),
            )),
            ValueRepr::Invalid(_) => value.clone().validate(),
        }
    }

    /// Converts a value into a float.
    ///
    /// ```jinja
    /// {{ "42.5"|float == 42.5 }} -> true
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn float(value: &Value) -> Result<Value, Error> {
        match &value.0 {
            ValueRepr::Undefined(_) | ValueRepr::None => Ok(Value::from(0.0)),
            ValueRepr::Bool(x) => Ok(Value::from(*x as u64 as f64)),
            ValueRepr::String(..) | ValueRepr::SmallStr(_) => value
                .as_str()
                .unwrap()
                .parse::<f64>()
                .map(Value::from)
                .map_err(|err| Error::new(ErrorKind::InvalidOperation, err.to_string())),
            ValueRepr::Invalid(_) => value.clone().validate(),
            _ => as_f64(value, true).map(Value::from).ok_or_else(|| {
                Error::new(
                    ErrorKind::InvalidOperation,
                    format!("cannot convert {} to float", value.kind()),
                )
            }),
        }
    }

    /// Sums up all the values in a sequence.
    ///
    /// ```jinja
    /// {{ range(10)|sum }} -> 45
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn sum(state: &State, values: Value) -> Result<Value, Error> {
        let mut rv = Value::from(0);
        let iter = ok!(state.undefined_behavior().try_iter(values));
        for value in iter {
            if value.is_undefined() {
                ok!(state.undefined_behavior().handle_undefined(false));
                continue;
            } else if !value.is_number() {
                return Err(Error::new(
                    ErrorKind::InvalidOperation,
                    format!("can only sum numbers, got {}", value.kind()),
                ));
            }
            rv = ok!(ops::add(&rv, &value));
        }

        Ok(rv)
    }

    /// Looks up an attribute.
    ///
    /// In MiniJinja this is the same as the `[]` operator.  In Jinja2 there is a
    /// small difference which is why this filter is sometimes used in Jinja2
    /// templates.  For compatibility it's provided here as well.
    ///
    /// ```jinja
    /// {{ value['key'] == value|attr('key') }} -> true
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn attr(value: &Value, key: &Value) -> Result<Value, Error> {
        value.get_item(key)
    }

    /// Round the number to a given precision.
    ///
    /// Round the number to a given precision. The first parameter specifies the
    /// precision (default is 0).
    ///
    /// ```jinja
    /// {{ 42.55|round }}
    ///   -> 43.0
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn round(value: Value, precision: Option<i32>) -> Result<Value, Error> {
        match value.0 {
            ValueRepr::I64(_) | ValueRepr::I128(_) | ValueRepr::U64(_) | ValueRepr::U128(_) => {
                Ok(value)
            }
            ValueRepr::F64(val) => {
                let x = 10f64.powi(precision.unwrap_or(0));
                Ok(Value::from((x * val).round() / x))
            }
            _ => Err(Error::new(
                ErrorKind::InvalidOperation,
                format!("cannot round value ({})", value.kind()),
            )),
        }
    }

    /// Returns the first item from an iterable.
    ///
    /// If the list is empty `undefined` is returned.
    ///
    /// ```jinja
    /// <dl>
    ///   <dt>primary email
    ///   <dd>{{ user.email_addresses|first|default('no user') }}
    /// </dl>
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn first(value: &Value) -> Result<Value, Error> {
        if let Some(s) = value.as_str() {
            Ok(s.chars().next().map_or(Value::UNDEFINED, Value::from))
        } else if let Some(mut iter) = value.as_object().and_then(|x| x.try_iter()) {
            Ok(iter.next().unwrap_or(Value::UNDEFINED))
        } else {
            Err(Error::new(
                ErrorKind::InvalidOperation,
                "cannot get first item from value",
            ))
        }
    }

    /// Returns the last item from an iterable.
    ///
    /// If the list is empty `undefined` is returned.
    ///
    /// ```jinja
    /// <h2>Most Recent Update</h2>
    /// {% with update = updates|last %}
    ///   <dl>
    ///     <dt>Location
    ///     <dd>{{ update.location }}
    ///     <dt>Status
    ///     <dd>{{ update.status }}
    ///   </dl>
    /// {% endwith %}
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn last(value: Value) -> Result<Value, Error> {
        if let Some(s) = value.as_str() {
            Ok(s.chars().next_back().map_or(Value::UNDEFINED, Value::from))
        } else if matches!(value.kind(), ValueKind::Seq | ValueKind::Iterable) {
            let rev = ok!(value.reverse());
            let mut iter = ok!(rev.try_iter());
            Ok(iter.next().unwrap_or_default())
        } else {
            Err(Error::new(
                ErrorKind::InvalidOperation,
                "cannot get last item from value",
            ))
        }
    }

    /// Returns the smallest item from an iterable.
    ///
    /// ```jinja
    /// {{ [1, 2, 3, 4]|min }} -> 1
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn min(state: &State, value: Value) -> Result<Value, Error> {
        let iter = ok!(state.undefined_behavior().try_iter(value).map_err(|err| {
            Error::new(ErrorKind::InvalidOperation, "cannot convert value to list").with_source(err)
        }));
        Ok(iter.min().unwrap_or(Value::UNDEFINED))
    }

    /// Returns the largest item from an iterable.
    ///
    /// ```jinja
    /// {{ [1, 2, 3, 4]|max }} -> 4
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn max(state: &State, value: Value) -> Result<Value, Error> {
        let iter = ok!(state.undefined_behavior().try_iter(value).map_err(|err| {
            Error::new(ErrorKind::InvalidOperation, "cannot convert value to list").with_source(err)
        }));
        Ok(iter.max().unwrap_or(Value::UNDEFINED))
    }

    /// Returns the sorted version of the given list.
    ///
    /// The filter accepts a few keyword arguments:
    ///
    /// * `case_sensitive`: set to `true` to make the sorting of strings case sensitive.
    /// * `attribute`: can be set to an attribute or dotted path to sort by that attribute.
    ///   can be a comma-separated list of attributes forming a composite key like "age, name".
    /// * `reverse`: set to `true` to sort in reverse.
    ///
    /// ```jinja
    /// {{ [1, 3, 2, 4]|sort }} -> [4, 3, 2, 1]
    /// {{ [1, 3, 2, 4]|sort(reverse=true) }} -> [1, 2, 3, 4]
    /// # Sort users by age attribute in descending order.
    /// {{ users|sort(attribute="age") }}
    /// # Sort users by age attribute in ascending order.
    /// {{ users|sort(attribute="age", reverse=true) }}
    /// # Sort cities by their name, and sort those with the same name by their state.
    /// {{ cities|sort(attribute="name, state") }}
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn sort(state: &State, value: Value, kwargs: Kwargs) -> Result<Value, Error> {
        let mut items = ok!(state.undefined_behavior().try_iter(value).map_err(|err| {
            Error::new(ErrorKind::InvalidOperation, "cannot convert value to list").with_source(err)
        }))
        .collect::<Vec<_>>();

        let case_sensitive = ok!(kwargs.get::<Option<bool>>("case_sensitive")).unwrap_or(false);
        let reverse = ok!(kwargs.get::<Option<bool>>("reverse")).unwrap_or(false);

        if let Some(attr) = ok!(kwargs.get::<Option<&str>>("attribute")) {
            let keys: Vec<_> = attr
                .split(',')
                .filter_map(|key| {
                    let trimmed = key.trim();
                    if !key.is_empty() {
                        Some(trimmed)
                    } else {
                        None
                    }
                })
                .collect();

            if keys.len() > 1 {
                // More than one keys
                safe_sort(&mut items, |a, b| {
                    let key_a = Value::from_iter(
                        keys.iter()
                            .map(|k| a.get_path_or_default(k, &Value::UNDEFINED)),
                    );
                    let key_b = Value::from_iter(
                        keys.iter()
                            .map(|k| b.get_path_or_default(k, &Value::UNDEFINED)),
                    );
                    cmp_helper(&key_a, &key_b, case_sensitive, reverse)
                })?;
            } else {
                // Fast path for a more common case of single key
                let key = if !keys.is_empty() { keys[0] } else { attr };
                safe_sort(&mut items, |a, b| {
                    match (a.get_path(key), b.get_path(key)) {
                        (Ok(a), Ok(b)) => cmp_helper(&a, &b, case_sensitive, reverse),
                        _ => Ordering::Equal,
                    }
                })?;
            }
        } else {
            safe_sort(&mut items, |a, b| cmp_helper(a, b, case_sensitive, reverse))?;
        }
        ok!(kwargs.assert_all_used());
        Ok(Value::from(items))
    }

    /// Converts the input value into a list.
    ///
    /// If the value is already a list, then it's returned unchanged.
    /// Applied to a map this returns the list of keys, applied to a
    /// string this returns the characters.  If the value is undefined
    /// an empty list is returned.
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn list(state: &State, value: Value) -> Result<Value, Error> {
        let iter = ok!(state.undefined_behavior().try_iter(value).map_err(|err| {
            Error::new(ErrorKind::InvalidOperation, "cannot convert value to list").with_source(err)
        }));
        Ok(Value::from(iter.collect::<Vec<_>>()))
    }

    /// Converts a value into a string if it's not one already.
    ///
    /// If the string has been marked as safe, that value is preserved.
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn string(value: &Value) -> Value {
        if value.kind() == ValueKind::String {
            value.clone()
        } else {
            value.to_string().into()
        }
    }

    /// Converts the value into a boolean value.
    ///
    /// This behaves the same as the if statement does with regards to
    /// handling of boolean values.
    ///
    /// ```jinja
    /// {{ 42|bool }} -> true
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn bool(value: &Value) -> bool {
        value.is_true()
    }

    /// Slice an iterable and return a list of lists containing
    /// those items.
    ///
    /// Useful if you want to create a div containing three ul tags that
    /// represent columns:
    ///
    /// ```jinja
    /// <div class="columnwrapper">
    /// {% for column in items|slice(3) %}
    ///   <ul class="column-{{ loop.index }}">
    ///   {% for item in column %}
    ///     <li>{{ item }}</li>
    ///   {% endfor %}
    ///   </ul>
    /// {% endfor %}
    /// </div>
    /// ```
    ///
    /// If you pass it a second argument it’s used to fill missing values on the
    /// last iteration.
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn slice(
        state: &State,
        value: Value,
        count: usize,
        fill_with: Option<Value>,
    ) -> Result<Value, Error> {
        if count == 0 {
            return Err(Error::new(ErrorKind::InvalidOperation, "count cannot be 0"));
        }
        let items = ok!(state.undefined_behavior().try_iter(value)).collect::<Vec<_>>();
        let len = items.len();
        let items_per_slice = len / count;
        let slices_with_extra = len % count;
        let mut offset = 0;
        let mut rv = Vec::with_capacity(count);

        for slice in 0..count {
            let start = offset + slice * items_per_slice;
            if slice < slices_with_extra {
                offset += 1;
            }
            let end = offset + (slice + 1) * items_per_slice;
            let tmp = &items[start..end];

            if let Some(ref filler) = fill_with {
                if slice >= slices_with_extra {
                    let mut tmp = tmp.to_vec();
                    tmp.push(filler.clone());
                    rv.push(Value::from(tmp));
                    continue;
                }
            }

            rv.push(Value::from(tmp.to_vec()));
        }

        Ok(Value::from(rv))
    }

    /// Batch items.
    ///
    /// This filter works pretty much like `slice` just the other way round. It
    /// returns a list of lists with the given number of items. If you provide a
    /// second parameter this is used to fill up missing items.
    ///
    /// ```jinja
    /// <table>
    ///   {% for row in items|batch(3, '&nbsp;') %}
    ///   <tr>
    ///   {% for column in row %}
    ///     <td>{{ column }}</td>
    ///   {% endfor %}
    ///   </tr>
    ///   {% endfor %}
    /// </table>
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn batch(
        state: &State,
        value: Value,
        count: usize,
        fill_with: Option<Value>,
    ) -> Result<Value, Error> {
        if count == 0 {
            return Err(Error::new(ErrorKind::InvalidOperation, "count cannot be 0"));
        }
        let mut rv = Vec::with_capacity(value.len().unwrap_or(0) / count);
        let mut tmp = Vec::with_capacity(count);

        for item in ok!(state.undefined_behavior().try_iter(value)) {
            if tmp.len() == count {
                rv.push(Value::from(mem::replace(
                    &mut tmp,
                    Vec::with_capacity(count),
                )));
            }
            tmp.push(item);
        }

        if !tmp.is_empty() {
            if let Some(filler) = fill_with {
                for _ in 0..count - tmp.len() {
                    tmp.push(filler.clone());
                }
            }
            rv.push(Value::from(tmp));
        }

        Ok(Value::from(rv))
    }

    /// Dumps a value to JSON.
    ///
    /// This filter is only available if the `json` feature is enabled.  The resulting
    /// value is safe to use in HTML as well as it will not contain any special HTML
    /// characters.  The optional parameter to the filter can be set to `true` to enable
    /// pretty printing.  Not that the `"` character is left unchanged as it's the
    /// JSON string delimiter.  If you want to pass JSON serialized this way into an
    /// HTTP attribute use single quoted HTML attributes:
    ///
    /// ```jinja
    /// <script>
    ///   const GLOBAL_CONFIG = {{ global_config|tojson }};
    /// </script>
    /// <a href="#" data-info='{{ json_object|tojson }}'>...</a>
    /// ```
    ///
    /// The filter takes one argument `indent` (which can also be passed as keyword
    /// argument for compatibility with Jinja2) which can be set to `true` to enable
    /// pretty printing or an integer to control the indentation of the pretty
    /// printing feature.
    ///
    /// ```jinja
    /// <script>
    ///   const GLOBAL_CONFIG = {{ global_config|tojson(indent=2) }};
    /// </script>
    /// ```
    #[cfg_attr(docsrs, doc(cfg(all(feature = "builtins", feature = "json"))))]
    #[cfg(feature = "json")]
    pub fn tojson(value: &Value, indent: Option<Value>, args: Kwargs) -> Result<Value, Error> {
        let indent = match indent {
            Some(indent) => Some(indent),
            None => ok!(args.get("indent")),
        };
        let indent = match indent {
            None => None,
            Some(ref val) => match bool::try_from(val.clone()).ok() {
                Some(true) => Some(2),
                Some(false) => None,
                None => Some(ok!(usize::try_from(val.clone()))),
            },
        };
        ok!(args.assert_all_used());
        if let Some(indent) = indent {
            let mut out = Vec::<u8>::new();
            let indentation = " ".repeat(indent);
            let formatter = serde_json::ser::PrettyFormatter::with_indent(indentation.as_bytes());
            let mut s = serde_json::Serializer::with_formatter(&mut out, formatter);
            serde::Serialize::serialize(&value, &mut s)
                .map(|_| unsafe { String::from_utf8_unchecked(out) })
        } else {
            serde_json::to_string(&value)
        }
        .map_err(|err| {
            Error::new(ErrorKind::InvalidOperation, "cannot serialize to JSON").with_source(err)
        })
        .map(|s| {
            // When this filter is used the return value is safe for both HTML and JSON
            let mut rv = String::with_capacity(s.len());
            for c in s.chars() {
                match c {
                    '<' => rv.push_str("\\u003c"),
                    '>' => rv.push_str("\\u003e"),
                    '&' => rv.push_str("\\u0026"),
                    '\'' => rv.push_str("\\u0027"),
                    _ => rv.push(c),
                }
            }
            Value::from_safe_string(rv)
        })
    }

    /// Indents Value with spaces
    ///
    /// The first optional parameter to the filter can be set to `true` to
    /// indent the first line. The parameter defaults to false.
    /// the second optional parameter to the filter can be set to `true`
    /// to indent blank lines. The parameter defaults to false.
    /// This filter is useful, if you want to template yaml-files
    ///
    /// ```jinja
    /// example:
    ///   config:
    /// {{ global_config|indent(2) }}          # does not indent first line
    /// {{ global_config|indent(2,true) }}     # indent whole Value with two spaces
    /// {{ global_config|indent(2,true,true)}} # indent whole Value and all blank lines
    /// ```
    #[cfg_attr(docsrs, doc(cfg(all(feature = "builtins"))))]
    pub fn indent(
        mut value: String,
        width: usize,
        indent_first_line: Option<bool>,
        indent_blank_lines: Option<bool>,
    ) -> String {
        fn strip_trailing_newline(input: &mut String) {
            if input.ends_with('\n') {
                input.truncate(input.len() - 1);
            }
            if input.ends_with('\r') {
                input.truncate(input.len() - 1);
            }
        }

        strip_trailing_newline(&mut value);
        let indent_with = " ".repeat(width);
        let mut output = String::new();
        let mut iterator = value.split('\n');
        if !indent_first_line.unwrap_or(false) {
            output.push_str(iterator.next().unwrap());
            output.push('\n');
        }
        for line in iterator {
            if line.is_empty() {
                if indent_blank_lines.unwrap_or(false) {
                    output.push_str(&indent_with);
                }
            } else {
                write!(output, "{indent_with}{line}").ok();
            }
            output.push('\n');
        }
        strip_trailing_newline(&mut output);
        output
    }

    /// URL encodes a value.
    ///
    /// If given a map it encodes the parameters into a query set, otherwise it
    /// encodes the stringified value.  If the value is none or undefined, an
    /// empty string is returned.
    ///
    /// ```jinja
    /// <a href="/search?{{ {"q": "my search", "lang": "fr"}|urlencode }}">Search</a>
    /// ```
    #[cfg_attr(docsrs, doc(cfg(all(feature = "builtins", feature = "urlencode"))))]
    #[cfg(feature = "urlencode")]
    pub fn urlencode(value: &Value) -> Result<String, Error> {
        const SET: &percent_encoding::AsciiSet = &percent_encoding::NON_ALPHANUMERIC
            .remove(b'/')
            .remove(b'.')
            .remove(b'-')
            .remove(b'_')
            .add(b' ');

        if value.kind() == ValueKind::Map {
            let mut rv = String::new();
            for k in ok!(value.try_iter()) {
                let v = ok!(value.get_item(&k));
                if v.is_none() || v.is_undefined() {
                    continue;
                }
                if !rv.is_empty() {
                    rv.push('&');
                }
                write!(
                    rv,
                    "{}={}",
                    percent_encoding::utf8_percent_encode(&k.to_string(), SET),
                    percent_encoding::utf8_percent_encode(&v.to_string(), SET)
                )
                .unwrap();
            }
            Ok(rv)
        } else {
            match &value.0 {
                ValueRepr::None | ValueRepr::Undefined(_) => Ok("".into()),
                ValueRepr::Bytes(b) => Ok(percent_encoding::percent_encode(b, SET).to_string()),
                ValueRepr::String(..) | ValueRepr::SmallStr(_) => Ok(
                    percent_encoding::utf8_percent_encode(value.as_str().unwrap(), SET).to_string(),
                ),
                _ => Ok(percent_encoding::utf8_percent_encode(&value.to_string(), SET).to_string()),
            }
        }
    }

    fn select_or_reject(
        state: &State,
        invert: bool,
        value: Value,
        attr: Option<Cow<'_, str>>,
        test_name: Option<Cow<'_, str>>,
        args: crate::value::Rest<Value>,
    ) -> Result<Vec<Value>, Error> {
        let mut rv = vec![];
        let test = if let Some(test_name) = test_name {
            Some(ok!(state
                .env()
                .get_test(&test_name)
                .ok_or_else(|| Error::from(ErrorKind::UnknownTest))))
        } else {
            None
        };
        for value in ok!(state.undefined_behavior().try_iter(value)) {
            let test_value = if let Some(ref attr) = attr {
                ok!(value.get_path(attr))
            } else {
                value.clone()
            };
            let passed = if let Some(test) = test {
                let new_args = Some(test_value)
                    .into_iter()
                    .chain(args.0.iter().cloned())
                    .collect::<Vec<_>>();
                ok!(test.call(state, &new_args)).is_true()
            } else {
                test_value.is_true()
            };
            if passed != invert {
                rv.push(value);
            }
        }
        Ok(rv)
    }

    /// Creates a new sequence of values that pass a test.
    ///
    /// Filters a sequence of objects by applying a test to each object.
    /// Only values that pass the test are included.
    ///
    /// If no test is specified, each object will be evaluated as a boolean.
    ///
    /// ```jinja
    /// {{ [1, 2, 3, 4]|select("odd") }} -> [1, 3]
    /// {{ [false, null, 42]|select }} -> [42]
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn select(
        state: &State,
        value: Value,
        test_name: Option<Cow<'_, str>>,
        args: crate::value::Rest<Value>,
    ) -> Result<Vec<Value>, Error> {
        select_or_reject(state, false, value, None, test_name, args)
    }

    /// Creates a new sequence of values of which an attribute passes a test.
    ///
    /// This functions like [`select`] but it will test an attribute of the
    /// object itself:
    ///
    /// ```jinja
    /// {{ users|selectattr("is_active") }} -> all users where x.is_active is true
    /// {{ users|selectattr("id", "even") }} -> returns all users with an even id
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn selectattr(
        state: &State,
        value: Value,
        attr: Cow<'_, str>,
        test_name: Option<Cow<'_, str>>,
        args: crate::value::Rest<Value>,
    ) -> Result<Vec<Value>, Error> {
        select_or_reject(state, false, value, Some(attr), test_name, args)
    }

    /// Creates a new sequence of values that don't pass a test.
    ///
    /// This is the inverse of [`select`].
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn reject(
        state: &State,
        value: Value,
        test_name: Option<Cow<'_, str>>,
        args: crate::value::Rest<Value>,
    ) -> Result<Vec<Value>, Error> {
        select_or_reject(state, true, value, None, test_name, args)
    }

    /// Creates a new sequence of values of which an attribute does not pass a test.
    ///
    /// This functions like [`select`] but it will test an attribute of the
    /// object itself:
    ///
    /// ```jinja
    /// {{ users|rejectattr("is_active") }} -> all users where x.is_active is false
    /// {{ users|rejectattr("id", "even") }} -> returns all users with an odd id
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn rejectattr(
        state: &State,
        value: Value,
        attr: Cow<'_, str>,
        test_name: Option<Cow<'_, str>>,
        args: crate::value::Rest<Value>,
    ) -> Result<Vec<Value>, Error> {
        select_or_reject(state, true, value, Some(attr), test_name, args)
    }

    /// Applies a filter to a sequence of objects or looks up an attribute.
    ///
    /// This is useful when dealing with lists of objects but you are really
    /// only interested in a certain value of it.
    ///
    /// The basic usage is mapping on an attribute. Given a list of users
    /// you can for instance quickly select the username and join on it:
    ///
    /// ```jinja
    /// {{ users|map(attribute='username')|join(', ') }}
    /// ```
    ///
    /// You can specify a `default` value to use if an object in the list does
    /// not have the given attribute.
    ///
    /// ```jinja
    /// {{ users|map(attribute="username", default="Anonymous")|join(", ") }}
    /// ```
    ///
    /// Alternatively you can have `map` invoke a filter by passing the name of the
    /// filter and the arguments afterwards. A good example would be applying a
    /// text conversion filter on a sequence:
    ///
    /// ```jinja
    /// Users on this page: {{ titles|map('lower')|join(', ') }}
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn map(
        state: &State,
        value: Value,
        args: crate::value::Rest<Value>,
    ) -> Result<Vec<Value>, Error> {
        let mut rv = Vec::with_capacity(value.len().unwrap_or(0));

        // attribute mapping
        let (args, kwargs): (&[Value], Kwargs) = crate::value::from_args(&args)?;

        if let Some(attr) = ok!(kwargs.get::<Option<Value>>("attribute")) {
            if !args.is_empty() {
                return Err(Error::from(ErrorKind::TooManyArguments));
            }
            let default = if kwargs.has("default") {
                ok!(kwargs.get::<Value>("default"))
            } else {
                Value::UNDEFINED
            };
            for value in ok!(state.undefined_behavior().try_iter(value)) {
                let sub_val = match attr.as_str() {
                    Some(path) => value.get_path(path),
                    None => value.get_item(&attr),
                };
                rv.push(match (sub_val, &default) {
                    (Ok(attr), _) => {
                        if attr.is_undefined() {
                            default.clone()
                        } else {
                            attr
                        }
                    }
                    (Err(_), default) if !default.is_undefined() => default.clone(),
                    (Err(err), _) => return Err(err),
                });
            }
            ok!(kwargs.assert_all_used());
            return Ok(rv);
        }

        // filter mapping
        let filter_name = ok!(args
            .first()
            .ok_or_else(|| Error::new(ErrorKind::InvalidOperation, "filter name is required")));
        let filter_name = ok!(filter_name.as_str().ok_or_else(|| {
            Error::new(ErrorKind::InvalidOperation, "filter name must be a string")
        }));

        let filter = ok!(state
            .env()
            .get_filter(filter_name)
            .ok_or_else(|| Error::from(ErrorKind::UnknownFilter)));
        for value in ok!(state.undefined_behavior().try_iter(value)) {
            let new_args = Some(value.clone())
                .into_iter()
                .chain(args.iter().skip(1).cloned())
                .collect::<Vec<_>>();
            rv.push(ok!(filter.call(state, &new_args)));
        }
        Ok(rv)
    }

    /// Group a sequence of objects by an attribute.
    ///
    /// The attribute can use dot notation for nested access, like `"address.city"``.
    /// The values are sorted first so only one group is returned for each unique value.
    /// The attribute can be passed as first argument or as keyword argument named
    /// `attribute`.
    ///
    /// For example, a list of User objects with a city attribute can be
    /// rendered in groups. In this example, grouper refers to the city value of
    /// the group.
    ///
    /// ```jinja
    /// <ul>{% for city, items in users|groupby("city") %}
    ///   <li>{{ city }}
    ///   <ul>{% for user in items %}
    ///     <li>{{ user.name }}
    ///   {% endfor %}</ul>
    /// </li>
    /// {% endfor %}</ul>
    /// ```
    ///
    /// groupby yields named tuples of `(grouper, list)``, which can be used instead
    /// of the tuple unpacking above.  As such this example is equivalent:
    ///
    /// ```jinja
    /// <ul>{% for group in users|groupby(attribute="city") %}
    ///   <li>{{ group.grouper }}
    ///   <ul>{% for user in group.list %}
    ///     <li>{{ user.name }}
    ///   {% endfor %}</ul>
    /// </li>
    /// {% endfor %}</ul>
    /// ```
    ///
    /// You can specify a default value to use if an object in the list does not
    /// have the given attribute.
    ///
    /// ```jinja
    /// <ul>{% for city, items in users|groupby("city", default="NY") %}
    ///   <li>{{ city }}: {{ items|map(attribute="name")|join(", ") }}</li>
    /// {% endfor %}</ul>
    /// ```
    ///
    /// Like the [`sort`] filter, sorting and grouping is case-insensitive by default.
    /// The key for each group will have the case of the first item in that group
    /// of values. For example, if a list of users has cities `["CA", "NY", "ca"]``,
    /// the "CA" group will have two values.  This can be disabled by passing
    /// `case_sensitive=True`.
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn groupby(value: Value, attribute: Option<&str>, kwargs: Kwargs) -> Result<Value, Error> {
        let default = ok!(kwargs.get::<Option<Value>>("default")).unwrap_or_default();
        let case_sensitive = ok!(kwargs.get::<Option<bool>>("case_sensitive")).unwrap_or(false);
        let attr = match attribute {
            Some(attr) => attr,
            None => ok!(kwargs.get::<&str>("attribute")),
        };
        let mut items: Vec<Value> = ok!(value.try_iter()).collect();
        safe_sort(&mut items, |a, b| {
            let a = a.get_path_or_default(attr, &default);
            let b = b.get_path_or_default(attr, &default);
            cmp_helper(&a, &b, case_sensitive, false)
        })?;
        ok!(kwargs.assert_all_used());

        #[derive(Debug)]
        pub struct GroupTuple {
            grouper: Value,
            list: Vec<Value>,
        }

        impl Object for GroupTuple {
            fn repr(self: &Arc<Self>) -> ObjectRepr {
                ObjectRepr::Seq
            }

            fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
                match (key.as_usize(), key.as_str()) {
                    (Some(0), None) | (None, Some("grouper")) => Some(self.grouper.clone()),
                    (Some(1), None) | (None, Some("list")) => {
                        Some(Value::make_object_iterable(self.clone(), |this| {
                            Box::new(this.list.iter().cloned())
                                as Box<dyn Iterator<Item = _> + Send + Sync>
                        }))
                    }
                    _ => None,
                }
            }

            fn enumerate(self: &Arc<Self>) -> Enumerator {
                Enumerator::Seq(2)
            }
        }

        let mut rv = Vec::new();
        let mut grouper = None::<Value>;
        let mut list = Vec::new();

        for item in items {
            let group_by = item.get_path_or_default(attr, &default);
            if let Some(ref last_grouper) = grouper {
                if cmp_helper(last_grouper, &group_by, case_sensitive, false) != Ordering::Equal {
                    rv.push(Value::from_object(GroupTuple {
                        grouper: last_grouper.clone(),
                        list: std::mem::take(&mut list),
                    }));
                }
            }
            grouper = Some(group_by);
            list.push(item);
        }

        if !list.is_empty() {
            rv.push(Value::from_object(GroupTuple {
                grouper: grouper.unwrap(),
                list,
            }));
        }

        Ok(Value::from_object(rv))
    }

    /// Returns a list of unique items from the given iterable.
    ///
    /// ```jinja
    /// {{ ['foo', 'bar', 'foobar', 'foobar']|unique|list }}
    ///   -> ['foo', 'bar', 'foobar']
    /// ```
    ///
    /// The unique items are yielded in the same order as their first occurrence
    /// in the iterable passed to the filter.  The filter will not detect
    /// duplicate objects or arrays, only primitives such as strings or numbers.
    ///
    /// Optionally the `attribute` keyword argument can be used to make the filter
    /// operate on an attribute instead of the value itself.  In this case only
    /// one city per state would be returned:
    ///
    /// ```jinja
    /// {{ list_of_cities|unique(attribute='state') }}
    /// ```
    ///
    /// Like the [`sort`] filter this operates case-insensitive by default.
    /// For example, if a list has the US state codes `["CA", "NY", "ca"]``,
    /// the resulting list will have `["CA", "NY"]`.  This can be disabled by
    /// passing `case_sensitive=True`.
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn unique(state: &State, values: Value, kwargs: Kwargs) -> Result<Value, Error> {
        use std::collections::BTreeSet;

        let attr = ok!(kwargs.get::<Option<&str>>("attribute"));
        let case_sensitive = ok!(kwargs.get::<Option<bool>>("case_sensitive")).unwrap_or(false);
        ok!(kwargs.assert_all_used());

        let mut rv = Vec::new();
        let mut seen = BTreeSet::new();

        let iter = ok!(state.undefined_behavior().try_iter(values));
        for item in iter {
            let value_to_compare = if let Some(attr) = attr {
                item.get_path_or_default(attr, &Value::UNDEFINED)
            } else {
                item.clone()
            };
            let memorized_value = if case_sensitive {
                value_to_compare.clone()
            } else if let Some(s) = value_to_compare.as_str() {
                Value::from(s.to_lowercase())
            } else {
                value_to_compare.clone()
            };

            if !seen.contains(&memorized_value) {
                rv.push(item);
                seen.insert(memorized_value);
            }
        }

        Ok(Value::from(rv))
    }

    /// Chain two or more iterable objects as a single iterable object.
    ///
    /// If all the individual objects are dictionaries, then the final chained object
    /// also acts like a dictionary -- you can lookup a key, or iterate over the keys
    /// etc. Note that the dictionaries are not merged, so if there are duplicate keys,
    /// then the lookup will return the value from the last matching dictionary in the
    /// chain.
    ///
    /// If all the individual objects are sequences, then the final chained
    /// object also acts like a list as if the lists are appended.
    ///
    /// Otherwise, the chained object acts like an iterator chaining individual
    /// iterators, but it cannot be indexed.
    ///
    /// ```jinja
    /// {{ users | chain(moreusers) | length }}
    /// {% for user, info in shard0 | chain(shard1, shard2) | dictsort %}
    ///   {{user}}: {{info}}
    /// {% endfor %}
    /// {{ list1 | chain(list2) | attr(1) }}
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn chain(
        _state: &State,
        value: Value,
        others: crate::value::Rest<Value>,
    ) -> Result<Value, Error> {
        let all_values = Some(value.clone())
            .into_iter()
            .chain(others.0.iter().cloned())
            .collect::<Vec<_>>();

        if all_values.iter().all(|v| v.kind() == ValueKind::Map) {
            Ok(Value::from_object(MergeDict::new(all_values)))
        } else if all_values
            .iter()
            .all(|v| matches!(v.kind(), ValueKind::Seq))
        {
            Ok(Value::from_object(MergeSeq::new(all_values)))
        } else {
            // General iterator chaining behavior
            Ok(Value::make_object_iterable(all_values, |values| {
                Box::new(values.iter().flat_map(|v| match v.try_iter() {
                    Ok(iter) => Box::new(iter) as Box<dyn Iterator<Item = Value> + Send + Sync>,
                    Err(err) => Box::new(Some(Value::from(err)).into_iter())
                        as Box<dyn Iterator<Item = Value> + Send + Sync>,
                })) as Box<dyn Iterator<Item = Value> + Send + Sync>
            }))
        }
    }

    /// Zip multiple iterables into tuples.
    ///
    /// This filter works like the Python `zip` function. It takes one or more
    /// iterables and returns an iterable of tuples where each tuple contains
    /// one element from each input iterable. The iteration stops when the
    /// shortest iterable is exhausted.
    ///
    /// ```jinja
    /// {{ [1, 2, 3]|zip(['a', 'b', 'c']) }}
    /// -> [(1, 'a'), (2, 'b'), (3, 'c')]
    ///
    /// {{ [1, 2]|zip(['a', 'b', 'c'], ['x', 'y', 'z']) }}
    /// -> [(1, 'a', 'x'), (2, 'b', 'y')]
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn zip(_state: &State, value: Value, others: Rest<Value>) -> Result<Value, Error> {
        let all_values = Some(value).into_iter().chain(others.0).collect::<Vec<_>>();

        // Validate all values are iterable and calculate minimum length
        let mut known_len: Option<usize> = None;
        for val in &all_values {
            match val.try_iter() {
                Ok(_) => {
                    // If all values have known lengths, track the minimum
                    if let Some(len) = val.len() {
                        known_len = Some(match known_len {
                            None => len,
                            Some(current_min) => current_min.min(len),
                        });
                    } else {
                        // If any value doesn't have a known length, we can't know the zip length
                        known_len = None;
                        break;
                    }
                }
                Err(_) => {
                    return Err(Error::new(
                        ErrorKind::InvalidOperation,
                        format!("zip filter argument must be iterable, got {}", val.kind()),
                    ));
                }
            }
        }

        Ok(Value::make_object_iterable(all_values, move |values| {
            let iter = std::iter::from_fn({
                let mut iters = values
                    .iter()
                    .map(|val| val.try_iter().ok())
                    .collect::<Option<Vec<_>>>()
                    .unwrap_or_default();

                move || {
                    if iters.is_empty() {
                        return None;
                    }

                    let mut tuple = Vec::with_capacity(iters.len());
                    for iter in &mut iters {
                        match iter.next() {
                            Some(val) => tuple.push(val),
                            None => return None,
                        }
                    }
                    Some(Value::from(tuple))
                }
            });

            if let Some(len) = known_len {
                Box::new(LenIterWrap(len, iter)) as Box<dyn Iterator<Item = Value> + Send + Sync>
            } else {
                Box::new(iter) as Box<dyn Iterator<Item = Value> + Send + Sync>
            }
        }))
    }

    /// Pretty print a variable.
    ///
    /// This is useful for debugging as it better shows what's inside an object.
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn pprint(value: &Value) -> String {
        format!("{value:#?}")
    }

    /// Apply the given values to a [printf-style] format string.
    ///
    /// ```jinja
    /// {{ "%s, %s!"|format(greeting, name) }}
    /// -> Hello, World!
    /// ```
    ///
    /// In many cases, the [str.format()] style could be more convenient than the
    /// printf-style formatting:
    ///
    /// ```jinja
    /// {{ "{}, {name}!".format(greeting, name="Alice") }}
    /// -> Hello, Alice!
    /// ```
    ///
    /// This option is available through `minijinja-contrib`'s `pycompat` feature.
    ///
    /// [printf-style]: https://docs.python.org/3/library/stdtypes.html#printf-style-string-formatting
    /// [str.format()]: https://docs.python.org/3/library/string.html#format-string-syntax
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn format(format_str: &str, format_args: Rest<Value>) -> Result<String, Error> {
        format_filter(FormatStyle::Printf, format_str, &format_args)
    }
}

#[cfg(feature = "builtins")]
pub use self::builtins::*;
