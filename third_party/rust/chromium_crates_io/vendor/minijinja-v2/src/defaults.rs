use std::borrow::Cow;
use std::collections::BTreeMap;

use crate::error::Error;
use crate::filters;
use crate::output::Output;
use crate::tests;
use crate::utils::{write_escaped, AutoEscape};
use crate::value::Value;
use crate::vm::State;

pub(crate) fn no_auto_escape(_: &str) -> AutoEscape {
    AutoEscape::None
}

/// These are extensions on template names that are ignored for the auto escape logic.
const IGNORED_EXTENSIONS: [&str; 3] = [".j2", ".jinja2", ".jinja"];

/// The default logic for auto escaping based on file extension.
///
/// * [`Html`](AutoEscape::Html): `.html`, `.htm`, `.xml`
#[cfg_attr(
    feature = "json",
    doc = r" * [`Json`](AutoEscape::Json): `.json`, `.json5`, `.js`, `.yaml`, `.yml`"
)]
/// * [`None`](AutoEscape::None): _all others_
///
/// Additionally `.j2`, `.jinja` and `.jinja2` as final extension is ignored. So
/// `.html.j2` is the considered the same as `.html`.
pub fn default_auto_escape_callback(mut name: &str) -> AutoEscape {
    for ext in IGNORED_EXTENSIONS {
        if let Some(stripped) = name.strip_suffix(ext) {
            name = stripped;
            break;
        }
    }

    match name.rsplit('.').next() {
        Some("html" | "htm" | "xml") => AutoEscape::Html,
        #[cfg(feature = "json")]
        Some("json" | "json5" | "js" | "yaml" | "yml") => AutoEscape::Json,
        _ => AutoEscape::None,
    }
}

/// The default formatter.
///
/// This formatter takes a value and directly writes it into the output format
/// while honoring the requested auto escape format of the state.  If the
/// value is already marked as safe, it's handled as if no auto escaping
/// was requested.
///
/// * [`Html`](AutoEscape::Html): performs HTML escaping
#[cfg_attr(
    feature = "json",
    doc = r" * [`Json`](AutoEscape::Json): serializes values to JSON"
)]
/// * [`None`](AutoEscape::None): no escaping
/// * [`Custom(..)`](AutoEscape::Custom): results in an error
pub fn escape_formatter(out: &mut Output, state: &State, value: &Value) -> Result<(), Error> {
    write_escaped(out, state.auto_escape(), value)
}

pub(crate) fn get_builtin_filters() -> BTreeMap<Cow<'static, str>, Value> {
    let mut rv = BTreeMap::new();
    rv.insert("safe".into(), Value::from_function(filters::safe));
    let escape = Value::from_function(filters::escape);
    rv.insert("escape".into(), escape.clone());
    rv.insert("e".into(), escape);
    #[cfg(feature = "builtins")]
    {
        rv.insert("lower".into(), Value::from_function(filters::lower));
        rv.insert("upper".into(), Value::from_function(filters::upper));
        rv.insert("title".into(), Value::from_function(filters::title));
        rv.insert(
            "capitalize".into(),
            Value::from_function(filters::capitalize),
        );
        rv.insert("replace".into(), Value::from_function(filters::replace));
        let length = Value::from_function(filters::length);
        rv.insert("length".into(), length.clone());
        rv.insert("count".into(), length);
        rv.insert("dictsort".into(), Value::from_function(filters::dictsort));
        rv.insert("items".into(), Value::from_function(filters::items));
        rv.insert("reverse".into(), Value::from_function(filters::reverse));
        rv.insert("trim".into(), Value::from_function(filters::trim));
        rv.insert("join".into(), Value::from_function(filters::join));
        rv.insert("split".into(), Value::from_function(filters::split));
        rv.insert("lines".into(), Value::from_function(filters::lines));
        rv.insert("default".into(), Value::from_function(filters::default));
        rv.insert("d".into(), Value::from_function(filters::default));
        rv.insert("round".into(), Value::from_function(filters::round));
        rv.insert("abs".into(), Value::from_function(filters::abs));
        rv.insert("int".into(), Value::from_function(filters::int));
        rv.insert("float".into(), Value::from_function(filters::float));
        rv.insert("attr".into(), Value::from_function(filters::attr));
        rv.insert("first".into(), Value::from_function(filters::first));
        rv.insert("last".into(), Value::from_function(filters::last));
        rv.insert("min".into(), Value::from_function(filters::min));
        rv.insert("max".into(), Value::from_function(filters::max));
        rv.insert("sort".into(), Value::from_function(filters::sort));
        rv.insert("list".into(), Value::from_function(filters::list));
        rv.insert("string".into(), Value::from_function(filters::string));
        rv.insert("bool".into(), Value::from_function(filters::bool));
        rv.insert("batch".into(), Value::from_function(filters::batch));
        rv.insert("slice".into(), Value::from_function(filters::slice));
        rv.insert("sum".into(), Value::from_function(filters::sum));
        rv.insert("indent".into(), Value::from_function(filters::indent));
        rv.insert("select".into(), Value::from_function(filters::select));
        rv.insert("reject".into(), Value::from_function(filters::reject));
        rv.insert(
            "selectattr".into(),
            Value::from_function(filters::selectattr),
        );
        rv.insert(
            "rejectattr".into(),
            Value::from_function(filters::rejectattr),
        );
        rv.insert("map".into(), Value::from_function(filters::map));
        rv.insert("groupby".into(), Value::from_function(filters::groupby));
        rv.insert("unique".into(), Value::from_function(filters::unique));
        rv.insert("chain".into(), Value::from_function(filters::chain));
        rv.insert("zip".into(), Value::from_function(filters::zip));
        rv.insert("pprint".into(), Value::from_function(filters::pprint));
        rv.insert("format".into(), Value::from_function(filters::format));

        #[cfg(feature = "json")]
        {
            rv.insert("tojson".into(), Value::from_function(filters::tojson));
        }
        #[cfg(feature = "urlencode")]
        {
            rv.insert("urlencode".into(), Value::from_function(filters::urlencode));
        }
    }

    rv
}

pub(crate) fn get_builtin_tests() -> BTreeMap<Cow<'static, str>, Value> {
    let mut rv = BTreeMap::new();
    rv.insert(
        "undefined".into(),
        Value::from_function(tests::is_undefined),
    );
    rv.insert("defined".into(), Value::from_function(tests::is_defined));
    rv.insert("none".into(), Value::from_function(tests::is_none));
    let is_safe = Value::from_function(tests::is_safe);
    rv.insert("safe".into(), is_safe.clone());
    rv.insert("escaped".into(), is_safe);
    #[cfg(feature = "builtins")]
    {
        rv.insert("boolean".into(), Value::from_function(tests::is_boolean));
        rv.insert("odd".into(), Value::from_function(tests::is_odd));
        rv.insert("even".into(), Value::from_function(tests::is_even));
        rv.insert(
            "divisibleby".into(),
            Value::from_function(tests::is_divisibleby),
        );
        rv.insert("number".into(), Value::from_function(tests::is_number));
        rv.insert("integer".into(), Value::from_function(tests::is_integer));
        rv.insert("int".into(), Value::from_function(tests::is_integer));
        rv.insert("float".into(), Value::from_function(tests::is_float));
        rv.insert("string".into(), Value::from_function(tests::is_string));
        rv.insert("sequence".into(), Value::from_function(tests::is_sequence));
        rv.insert("iterable".into(), Value::from_function(tests::is_iterable));
        rv.insert("mapping".into(), Value::from_function(tests::is_mapping));
        rv.insert(
            "startingwith".into(),
            Value::from_function(tests::is_startingwith),
        );
        rv.insert(
            "endingwith".into(),
            Value::from_function(tests::is_endingwith),
        );
        rv.insert("lower".into(), Value::from_function(tests::is_lower));
        rv.insert("upper".into(), Value::from_function(tests::is_upper));
        rv.insert("sameas".into(), Value::from_function(tests::is_sameas));

        // operators
        let is_eq = Value::from_function(tests::is_eq);
        rv.insert("eq".into(), is_eq.clone());
        rv.insert("equalto".into(), is_eq.clone());
        rv.insert("==".into(), is_eq);
        let is_ne = Value::from_function(tests::is_ne);
        rv.insert("ne".into(), is_ne.clone());
        rv.insert("!=".into(), is_ne);
        let is_lt = Value::from_function(tests::is_lt);
        rv.insert("lt".into(), is_lt.clone());
        rv.insert("lessthan".into(), is_lt.clone());
        rv.insert("<".into(), is_lt);
        let is_le = Value::from_function(tests::is_le);
        rv.insert("le".into(), is_le.clone());
        rv.insert("<=".into(), is_le);
        let is_gt = Value::from_function(tests::is_gt);
        rv.insert("gt".into(), is_gt.clone());
        rv.insert("greaterthan".into(), is_gt.clone());
        rv.insert(">".into(), is_gt);
        let is_ge = Value::from_function(tests::is_ge);
        rv.insert("ge".into(), is_ge.clone());
        rv.insert(">=".into(), is_ge);
        rv.insert("in".into(), Value::from_function(tests::is_in));
        rv.insert("true".into(), Value::from_function(tests::is_true));
        rv.insert("false".into(), Value::from_function(tests::is_false));
        rv.insert("filter".into(), Value::from_function(tests::is_filter));
        rv.insert("test".into(), Value::from_function(tests::is_test));
    }
    rv
}

pub(crate) fn get_globals() -> BTreeMap<Cow<'static, str>, Value> {
    #[allow(unused_mut)]
    let mut rv = BTreeMap::new();
    #[cfg(feature = "builtins")]
    {
        use crate::functions::{self, BoxedFunction};
        rv.insert(
            "range".into(),
            BoxedFunction::new(functions::range).to_value(),
        );
        rv.insert(
            "dict".into(),
            BoxedFunction::new(functions::dict).to_value(),
        );
        rv.insert(
            "debug".into(),
            BoxedFunction::new(functions::debug).to_value(),
        );
        rv.insert(
            "namespace".into(),
            BoxedFunction::new(functions::namespace).to_value(),
        );
    }

    rv
}

#[cfg(test)]
mod unit_tests {
    use super::*;

    use similar_asserts::assert_eq;

    #[test]
    fn test_default_uto_escape() {
        assert_eq!(default_auto_escape_callback("foo.html"), AutoEscape::Html);
        assert_eq!(
            default_auto_escape_callback("foo.html.j2"),
            AutoEscape::Html
        );
        assert_eq!(default_auto_escape_callback("foo.htm"), AutoEscape::Html);
        assert_eq!(default_auto_escape_callback("foo.htm.j2"), AutoEscape::Html);
        assert_eq!(default_auto_escape_callback("foo.txt"), AutoEscape::None);
        assert_eq!(default_auto_escape_callback("foo.txt.j2"), AutoEscape::None);

        assert_eq!(
            default_auto_escape_callback("foo.html.jinja"),
            AutoEscape::Html
        );
        assert_eq!(
            default_auto_escape_callback("foo.htm.jinja"),
            AutoEscape::Html
        );
        assert_eq!(
            default_auto_escape_callback("foo.txt.jinja"),
            AutoEscape::None
        );

        assert_eq!(
            default_auto_escape_callback("foo.html.jinja2"),
            AutoEscape::Html
        );
        assert_eq!(
            default_auto_escape_callback("foo.htm.jinja2"),
            AutoEscape::Html
        );
        assert_eq!(
            default_auto_escape_callback("foo.txt.jinja2"),
            AutoEscape::None
        );

        // only one is removed
        assert_eq!(
            default_auto_escape_callback("foo.html.j2.jinja"),
            AutoEscape::None
        );

        #[cfg(feature = "json")]
        {
            assert_eq!(default_auto_escape_callback("foo.yaml"), AutoEscape::Json);
            assert_eq!(
                default_auto_escape_callback("foo.json.j2"),
                AutoEscape::Json
            );
        }
    }
}
