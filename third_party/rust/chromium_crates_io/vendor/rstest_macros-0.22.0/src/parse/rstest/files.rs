use std::{env, path::PathBuf};

use glob::glob;
use quote::ToTokens;
use regex::Regex;
use relative_path::RelativePath;
use syn::{parse_quote, visit_mut::VisitMut, Attribute, Expr, FnArg, Ident, ItemFn, LitStr};

use crate::{
    error::ErrorsVec,
    parse::{
        extract_argument_attrs,
        vlist::{Value, ValueList},
    },
    refident::{IntoPat, MaybeIdent},
    utils::attr_is,
};

#[derive(Debug, Clone, PartialEq)]
pub(crate) struct FilesGlobReferences {
    glob: Vec<LitStrAttr>,
    exclude: Vec<Exclude>,
    ignore_dot_files: bool,
}

impl FilesGlobReferences {
    /// Return the tuples attribute, path string if they are valid relative paths
    fn paths(&self, base_dir: &PathBuf) -> Result<Vec<(&LitStrAttr, String)>, syn::Error> {
        self.glob
            .iter()
            .map(|attr| {
                RelativePath::from_path(&attr.value())
                    .map_err(|e| attr.error(&format!("Invalid glob path: {e}")))
                    .map(|p| p.to_logical_path(base_dir))
                    .map(|p| (attr, p.to_string_lossy().into_owned()))
            })
            .collect::<Result<Vec<_>, _>>()
    }
}

trait RaiseError: ToTokens {
    fn error(&self, msg: &str) -> syn::Error {
        syn::Error::new_spanned(self, msg)
    }
}

impl RaiseError for Attribute {}
impl RaiseError for LitStrAttr {}
impl ToTokens for LitStrAttr {
    fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
        self.attr.to_tokens(tokens)
    }
}

impl FilesGlobReferences {
    fn new(glob: Vec<LitStrAttr>, exclude: Vec<Exclude>, ignore_dot_files: bool) -> Self {
        Self {
            glob,
            exclude,
            ignore_dot_files,
        }
    }

    fn is_valid(&self, p: &RelativePath) -> bool {
        if self.ignore_dot_files
            && p.components()
                .any(|c| matches!(c, relative_path::Component::Normal(c) if c.starts_with('.')))
        {
            return false;
        }
        !self.exclude.iter().any(|e| e.r.is_match(p.as_ref()))
    }
}

/// An attribute in the form `#[name("some string")]`
#[derive(Debug, Clone, PartialEq)]
struct LitStrAttr {
    attr: Attribute,
    value: LitStr,
}

impl LitStrAttr {
    fn value(&self) -> String {
        self.value.value()
    }
}

impl TryFrom<Attribute> for LitStrAttr {
    type Error = syn::Error;

    fn try_from(attr: Attribute) -> Result<Self, Self::Error> {
        let value = attr.parse_args::<LitStr>()?;
        Ok(Self { attr, value })
    }
}

/// The `#[exclude("regex")]` attribute
#[derive(Debug, Clone)]
struct Exclude {
    attr: LitStrAttr,
    r: Regex,
}

impl PartialEq for Exclude {
    fn eq(&self, other: &Self) -> bool {
        self.attr.value == other.attr.value
    }
}

impl TryFrom<Attribute> for Exclude {
    type Error = syn::Error;

    fn try_from(attr: Attribute) -> Result<Self, Self::Error> {
        let attr: LitStrAttr = attr.try_into()?;
        let r = regex::Regex::new(&attr.value()).map_err(|e| {
            syn::Error::new_spanned(
                &attr,
                format!(r#""{}" Should be a valid regex: {e}"#, attr.value()),
            )
        })?;
        Ok(Self { attr, r })
    }
}

impl From<Vec<LitStrAttr>> for FilesGlobReferences {
    fn from(value: Vec<LitStrAttr>) -> Self {
        Self::new(value, Default::default(), true)
    }
}

/// Entry point function to extract files attributes
pub(crate) fn extract_files(
    item_fn: &mut ItemFn,
) -> Result<Vec<(Ident, FilesGlobReferences)>, ErrorsVec> {
    let mut extractor = ValueFilesExtractor::default();
    extractor.visit_item_fn_mut(item_fn);
    extractor.take()
}

/// Simple struct used to visit function attributes and extract future args to
/// implement the boilerplate.
#[derive(Default)]
struct ValueFilesExtractor {
    files: Vec<(Ident, FilesGlobReferences)>,
    errors: Vec<syn::Error>,
}

impl ValueFilesExtractor {
    pub(crate) fn take(self) -> Result<Vec<(Ident, FilesGlobReferences)>, ErrorsVec> {
        if self.errors.is_empty() {
            Ok(self.files)
        } else {
            Err(self.errors.into())
        }
    }

    fn collect_errors<T: Default>(&mut self, result: Result<T, syn::Error>) -> T {
        match result {
            Ok(v) => v,
            Err(e) => {
                self.errors.push(e);
                T::default()
            }
        }
    }

    fn extract_argument_attrs<'a, B: 'a + std::fmt::Debug>(
        &mut self,
        node: &mut FnArg,
        is_valid_attr: fn(&syn::Attribute) -> bool,
        build: impl Fn(syn::Attribute) -> syn::Result<B> + 'a,
    ) -> Vec<B> {
        self.collect_errors(
            extract_argument_attrs(node, is_valid_attr, build).collect::<Result<Vec<_>, _>>(),
        )
    }

    fn extract_files(&mut self, node: &mut FnArg) -> Vec<LitStrAttr> {
        self.extract_argument_attrs(node, |a| attr_is(a, "files"), |attr| attr.try_into())
    }

    fn extract_exclude(&mut self, node: &mut FnArg) -> Vec<Exclude> {
        self.extract_argument_attrs(node, |a| attr_is(a, "exclude"), Exclude::try_from)
    }

    fn extract_include_dot_files(&mut self, node: &mut FnArg) -> Vec<Attribute> {
        self.extract_argument_attrs(
            node,
            |a| attr_is(a, "include_dot_files"),
            |attr| {
                attr.meta
                    .require_path_only()
                    .map_err(|_| attr.error("Use #[include_dot_files] to include dot files"))?;
                Ok(attr)
            },
        )
    }
}

impl VisitMut for ValueFilesExtractor {
    fn visit_fn_arg_mut(&mut self, node: &mut FnArg) {
        let name = node.maybe_ident().cloned();
        if matches!(node, FnArg::Receiver(_)) || name.is_none() {
            return;
        }
        let name = name.unwrap();
        let files = self.extract_files(node);
        let excludes = self.extract_exclude(node);
        let include_dot_files = self.extract_include_dot_files(node);
        if !include_dot_files.is_empty() {
            include_dot_files.iter().skip(1).for_each(|attr| {
                self.errors
                    .push(attr.error("Cannot use #[include_dot_files] more than once"))
            })
        }
        if !files.is_empty() {
            self.files.push((
                name,
                FilesGlobReferences::new(files, excludes, include_dot_files.is_empty()),
            ))
        } else {
            excludes.into_iter().for_each(|e| {
                self.errors.push(
                    e.attr
                        .error("You cannot use #[exclude(...)] without #[files(...)]"),
                )
            });
            include_dot_files.into_iter().for_each(|attr| {
                self.errors
                    .push(attr.error("You cannot use #[include_dot_files] without #[files(...)]"))
            });
        }
    }
}

trait BaseDir {
    fn base_dir(&self) -> Result<PathBuf, String> {
        env::var("CARGO_MANIFEST_DIR")
            .map(PathBuf::from)
            .map_err(|_|
                "Rstest's #[files(...)] requires that CARGO_MANIFEST_DIR is defined to define glob the relative path".to_string()
            )
    }
}

struct DefaultBaseDir;

impl BaseDir for DefaultBaseDir {}

trait GlobResolver {
    fn glob(&self, pattern: &str) -> Result<Vec<PathBuf>, String> {
        let globs =
            glob(pattern).map_err(|e| format!("glob failed for whole path `{pattern}` due {e}"))?;
        globs
            .into_iter()
            .map(|p| p.map_err(|e| format!("glob failed for file due {e}")))
            .map(|r| {
                r.and_then(|p| {
                    p.canonicalize()
                        .map_err(|e| format!("failed to canonicalize {} due {e}", p.display()))
                })
            })
            .collect()
    }
}

struct DefaultGlobResolver;

impl GlobResolver for DefaultGlobResolver {}

/// The struct used to gel te values from the files attributes. You can inject
/// the base dir resolver and glob resolver implementation.
pub(crate) struct ValueListFromFiles<'a> {
    base_dir: Box<dyn BaseDir + 'a>,
    g_resolver: Box<dyn GlobResolver + 'a>,
}

impl<'a> Default for ValueListFromFiles<'a> {
    fn default() -> Self {
        Self {
            g_resolver: Box::new(DefaultGlobResolver),
            base_dir: Box::new(DefaultBaseDir),
        }
    }
}

impl<'a> ValueListFromFiles<'a> {
    pub fn to_value_list(
        &self,
        files: Vec<(Ident, FilesGlobReferences)>,
    ) -> Result<Vec<ValueList>, syn::Error> {
        files
            .into_iter()
            .map(|(arg, refs)| {
                self.file_list_values(refs).map(|values| ValueList {
                    arg: arg.into_pat(),
                    values,
                })
            })
            .collect::<Result<Vec<ValueList>, _>>()
    }

    fn file_list_values(&self, refs: FilesGlobReferences) -> Result<Vec<Value>, syn::Error> {
        let base_dir = self
            .base_dir
            .base_dir()
            .map_err(|msg| refs.glob[0].error(&msg))?;
        let resolved_paths = refs.paths(&base_dir)?;
        let base_dir = base_dir
            .into_os_string()
            .into_string()
            .map_err(|p| refs.glob[0].error(&format!("Cannot get a valid string from {p:?}")))?;

        let mut values: Vec<(Expr, String)> = vec![];
        for (attr, abs_path) in self.all_files_path(resolved_paths)? {
            let relative_path = abs_path
                .clone()
                .into_os_string()
                .into_string()
                .map(|inner| RelativePath::new(base_dir.as_str()).relative(inner))
                .map_err(|e| {
                    attr.error(&format!("Invalid absolute path {}", e.to_string_lossy()))
                })?;

            if !refs.is_valid(&relative_path) {
                continue;
            }

            let path_str = abs_path.to_string_lossy();
            values.push((
                parse_quote! {
                    <::std::path::PathBuf as std::str::FromStr>::from_str(#path_str).unwrap()
                },
                render_file_description(&relative_path),
            ));
        }

        if values.is_empty() {
            Err(refs.glob[0].error("No file found"))?;
        }

        Ok(values
            .into_iter()
            .map(|(e, desc)| Value::new(e, Some(desc)))
            .collect())
    }

    /// Return the tuples of attribute, file path resolved via glob resolver, sorted by path and without duplications.
    fn all_files_path<'b>(
        &self,
        resolved_paths: Vec<(&'b LitStrAttr, String)>,
    ) -> Result<Vec<(&'b LitStrAttr, PathBuf)>, syn::Error> {
        let mut paths = resolved_paths
            .iter()
            .map(|(attr, pattern)| {
                self.g_resolver
                    .glob(pattern.as_ref())
                    .map_err(|msg| attr.error(&msg))
                    .map(|p| (attr, p))
            })
            .collect::<Result<Vec<_>, _>>()?
            .into_iter()
            .flat_map(|(&attr, inner)| inner.into_iter().map(move |p| (attr, p)))
            .collect::<Vec<_>>();
        paths.sort_by(|(_, a), (_, b)| a.cmp(b));
        paths.dedup_by(|(_, a), (_, b)| a.eq(&b));
        Ok(paths)
    }
}

fn render_file_description(file: &RelativePath) -> String {
    let mut description = String::new();
    for c in file.components() {
        match c {
            relative_path::Component::CurDir => continue,
            relative_path::Component::ParentDir => description.push_str("_UP"),
            relative_path::Component::Normal(segment) => description.push_str(segment),
        }
        description.push('/')
    }
    description.pop();
    description
}

#[cfg(test)]
mod should {
    use std::collections::HashMap;

    use super::*;
    use crate::test::{assert_eq, *};
    use maplit::hashmap;
    use rstest_test::assert_in;

    fn lit_str_attr(name: &str, value: impl AsRef<str>) -> LitStrAttr {
        attrs(&format!(r#"#[{name}("{}")]"#, value.as_ref()))
            .into_iter()
            .next()
            .unwrap()
            .try_into()
            .unwrap()
    }

    fn files_attr(lstr: impl AsRef<str>) -> LitStrAttr {
        lit_str_attr("files", lstr)
    }

    fn exclude_attr(lstr: impl AsRef<str>) -> LitStrAttr {
        lit_str_attr("exclude", lstr)
    }

    impl Exclude {
        fn fake(value: &str, r: Option<Regex>) -> Self {
            let r = r.unwrap_or_else(|| regex::Regex::new(value).unwrap());
            Self {
                attr: exclude_attr(value),
                r,
            }
        }
    }

    impl From<&str> for Exclude {
        fn from(value: &str) -> Self {
            Self {
                attr: exclude_attr(value),
                r: regex::Regex::new(value).unwrap(),
            }
        }
    }

    #[rstest]
    #[case::simple(r#"fn f(#[files("some_glob")] a: PathBuf) {}"#, "fn f(a: PathBuf) {}", &[("a", &["some_glob"], &[], true)])]
    #[case::more_than_one(
        r#"fn f(#[files("first")] a: PathBuf, b: u32, #[files("third")] c: PathBuf) {}"#,
        r#"fn f(a: PathBuf, 
                b: u32, 
                c: PathBuf) {}"#,
        &[("a", &["first"], &[], true), ("c", &["third"], &[], true)],
    )]
    #[case::more_globs_on_the_same_var(
        r#"fn f(#[files("first")] #[files("second")] a: PathBuf) {}"#,
        r#"fn f(a: PathBuf) {}"#,
        &[("a", &["first", "second"], &[], true)],
    )]
    #[case::exclude(r#"fn f(#[files("some_glob")] #[exclude("exclude")] a: PathBuf) {}"#, 
    "fn f(a: PathBuf) {}", &[("a", &["some_glob"], &["exclude"], true)])]
    #[case::exclude_more(r#"fn f(#[files("some_glob")] #[exclude("first")]  #[exclude("second")] a: PathBuf) {}"#, 
    "fn f(a: PathBuf) {}", &[("a", &["some_glob"], &["first", "second"], true)])]
    #[case::include_dot_files(r#"fn f(#[files("some_glob")] #[include_dot_files] a: PathBuf) {}"#, 
    "fn f(a: PathBuf) {}", &[("a", &["some_glob"], &[], false)])]

    fn extract<'a, G: AsRef<[&'a str]>, E: AsRef<[&'a str]>>(
        #[case] item_fn: &str,
        #[case] expected: &str,
        #[case] expected_files: &[(&str, G, E, bool)],
    ) {
        let mut item_fn: ItemFn = item_fn.ast();
        let expected: ItemFn = expected.ast();

        let files = extract_files(&mut item_fn).unwrap();

        assert_eq!(expected, item_fn);
        assert_eq!(
            files,
            expected_files
                .into_iter()
                .map(|(id, globs, ex, ignore)| (
                    ident(id),
                    FilesGlobReferences::new(
                        globs.as_ref().iter().map(files_attr).collect(),
                        ex.as_ref().iter().map(|&ex| ex.into()).collect(),
                        *ignore
                    )
                ))
                .collect::<Vec<_>>()
        );
    }

    #[rstest]
    #[case::no_files_arg("fn f(#[files] a: PathBuf) {}", "#[files(...)]")]
    #[case::invalid_files_inner("fn f(#[files(a::b::c)] a: PathBuf) {}", "string literal")]
    #[case::no_exclude_args(
        r#"fn f(#[files("some")] #[exclude] a: PathBuf) {}"#,
        "#[exclude(...)]"
    )]
    #[case::invalid_exclude_inner(
        r#"fn f(#[files("some")] #[exclude(a::b)] a: PathBuf) {}"#,
        "string literal"
    )]
    #[case::invalid_exclude_regex(
        r#"fn f(#[files("some")] #[exclude("invalid(reg(ex")] a: PathBuf) {}"#,
        "valid regex"
    )]
    #[case::include_dot_files_with_args(
        r#"fn f(#[files("some")] #[include_dot_files(some)] a: PathBuf) {}"#,
        "#[include_dot_files]"
    )]
    #[case::exclude_without_files(
        r#"fn f(#[exclude("some")] a: PathBuf) {}"#,
        "#[exclude(...)] without #[files(...)]"
    )]
    #[case::include_dot_files_without_files(
        r#"fn f(#[include_dot_files] a: PathBuf) {}"#,
        "#[include_dot_files] without #[files(...)]"
    )]
    #[case::include_dot_files_more_than_once(
        r#"fn f(#[files("some")] #[include_dot_files] #[include_dot_files] a: PathBuf) {}"#,
        "more than once"
    )]
    fn raise_error(#[case] item_fn: &str, #[case] message: &str) {
        let mut item_fn: ItemFn = item_fn.ast();

        let err = extract_files(&mut item_fn).unwrap_err();

        assert_in!(format!("{:?}", err), message);
    }

    #[derive(Default)]
    struct FakeBaseDir(PathBuf);
    impl From<&str> for FakeBaseDir {
        fn from(value: &str) -> Self {
            Self(PathBuf::from(value))
        }
    }
    impl BaseDir for FakeBaseDir {
        fn base_dir(&self) -> Result<PathBuf, String> {
            Ok(self.0.clone())
        }
    }

    impl<'a> ValueListFromFiles<'a> {
        fn new(bdir: impl BaseDir + 'a, g_resover: impl GlobResolver + 'a) -> Self {
            Self {
                base_dir: Box::new(bdir),
                g_resolver: Box::new(g_resover),
            }
        }
    }

    #[derive(Default)]
    struct FakeResolver(Vec<String>);

    impl From<&[&str]> for FakeResolver {
        fn from(value: &[&str]) -> Self {
            Self(value.iter().map(ToString::to_string).collect())
        }
    }

    impl GlobResolver for FakeResolver {
        fn glob(&self, _pattern: &str) -> Result<Vec<PathBuf>, String> {
            Ok(self.0.iter().map(PathBuf::from).collect())
        }
    }

    #[derive(Default)]
    struct FakeMapResolver(String, HashMap<String, Vec<PathBuf>>);

    impl From<(&str, &HashMap<&str, &[&str]>)> for FakeMapResolver {
        fn from(value: (&str, &HashMap<&str, &[&str]>)) -> Self {
            Self(
                value.0.to_string(),
                value
                    .1
                    .iter()
                    .map(|(&key, &values)| {
                        (
                            key.to_string(),
                            values
                                .iter()
                                .map(|&v| PathBuf::from(format!("{}/{v}", value.0)))
                                .collect::<Vec<_>>(),
                        )
                    })
                    .collect(),
            )
        }
    }

    impl GlobResolver for FakeMapResolver {
        fn glob(&self, pattern: &str) -> Result<Vec<PathBuf>, String> {
            let pattern = pattern.strip_prefix(&format!("{}/", self.0)).unwrap();
            Ok(self.1.get(pattern).cloned().unwrap_or_default())
        }
    }

    #[rstest]
    #[case::simple("/base", None, FakeResolver::from(["/base/first", "/base/second"].as_slice()), vec![], true, &["first", "second"])]
    #[case::more_glob("/base", Some(["path1", "path2"].as_slice()), FakeMapResolver::from(
        ("/base", &hashmap!(
            "path1" => ["first", "second"].as_slice(),
            "path2" => ["third", "zzzz"].as_slice()
        ))
    ), vec![], true, &["first", "second", "third", "zzzz"])]
    #[case::should_remove_duplicates("/base", Some(["path1", "path2"].as_slice()), FakeMapResolver::from(
        ("/base", &hashmap!(
            "path1" => ["first", "second"].as_slice(),
            "path2" => ["second", "third"].as_slice()
        ))
    ), vec![], true, &["first", "second", "third"])]
    #[case::should_sort("/base", None, FakeResolver::from(["/base/second", "/base/first"].as_slice()), vec![], true, &["first", "second"])]
    #[case::exclude("/base", None, FakeResolver::from([
        "/base/first", "/base/rem_1", "/base/other/rem_2", "/base/second"].as_slice()), 
        vec![Exclude::fake("no_mater", Some(Regex::new("rem_").unwrap()))], true, &["first", "second"])]
    #[case::exclude_more("/base", None, FakeResolver::from([
        "/base/first", "/base/rem_1", "/base/other/rem_2", "/base/some/other", "/base/second"].as_slice()), 
        vec![
            Exclude::fake("no_mater", Some(Regex::new("rem_").unwrap())),
            Exclude::fake("no_mater", Some(Regex::new("some").unwrap())),
            ], true, &["first", "second"])]
    #[case::ignore_dot_files("/base", None, FakeResolver::from([
        "/base/first", "/base/.ignore", "/base/.ignore_dir/a", "/base/second/.not", "/base/second/but_include", "/base/in/.out/other/ignored"].as_slice()), 
        vec![], true, &["first", "second/but_include"])]
    #[case::include_dot_files("/base", None, FakeResolver::from([
        "/base/first", "/base/.ignore", "/base/.ignore_dir/a", "/base/second/.not", "/base/second/but_include", "/base/in/.out/other/ignored"].as_slice()), 
        vec![], false, &[".ignore", ".ignore_dir/a", "first", "in/.out/other/ignored", "second/.not", "second/but_include"])]
    #[case::relative_path("/base/some/other/folders", None, 
        FakeResolver::from(["/base/first", "/base/second"].as_slice()), vec![], true, &["../../../first", "../../../second"])]
    fn generate_a_variable_with_the_glob_resolved_path(
        #[case] bdir: &str,
        #[case] paths: Option<&[&str]>,
        #[case] resolver: impl GlobResolver,
        #[case] exclude: Vec<Exclude>,
        #[case] ignore_dot_files: bool,
        #[case] expected: &[&str],
    ) {
        let paths = paths
            .map(|inner| inner.into_iter().map(files_attr).collect())
            .unwrap_or(vec![files_attr("no_mater")]);
        let values = ValueListFromFiles::new(FakeBaseDir::from(bdir), resolver)
            .to_value_list(vec![(
                ident("a"),
                FilesGlobReferences::new(paths, exclude, ignore_dot_files),
            )])
            .unwrap();

        let mut v_list = values_list(
            "a",
            &expected
                .iter()
                .map(|&p| RelativePath::from_path(p).unwrap())
                .map(|r| r.to_logical_path(bdir))
                .map(|p| {
                    format!(
                        r#"<::std::path::PathBuf as std::str::FromStr>::from_str("{}").unwrap()"#,
                        p.as_os_str().to_str().unwrap()
                    )
                })
                .collect::<Vec<_>>(),
        );
        v_list
            .values
            .iter_mut()
            .zip(expected.iter())
            .for_each(|(v, &ex)| {
                v.description = Some(render_file_description(
                    &RelativePath::from_path(ex).unwrap(),
                ))
            });
        assert_eq!(vec![v_list], values);
    }

    #[rstest]
    #[case::file("name.txt", "name.txt")]
    #[case::in_folder("some/folder/name.txt", "some/folder/name.txt")]
    #[case::no_extension("name", "name")]
    #[case::parent("../../name.txt", "_UP/_UP/name.txt")]
    #[case::ignore_current("./../other/name.txt", "_UP/other/name.txt")]
    fn render_file_description_should(#[case] path: &str, #[case] expected: &str) {
        assert_eq!(
            render_file_description(&RelativePath::from_path(path).unwrap()),
            expected
        );
    }

    #[test]
    #[should_panic(expected = "Fake error")]
    fn raise_error_if_fail_to_get_root() {
        #[derive(Default)]
        struct ErrorBaseDir;
        impl BaseDir for ErrorBaseDir {
            fn base_dir(&self) -> Result<PathBuf, String> {
                Err("Fake error".to_string())
            }
        }

        ValueListFromFiles::new(ErrorBaseDir::default(), FakeResolver::default())
            .to_value_list(vec![(
                ident("a"),
                FilesGlobReferences::new(vec![files_attr("no_mater")], Default::default(), true),
            )])
            .unwrap();
    }

    #[test]
    #[should_panic(expected = "No file found")]
    fn raise_error_if_no_files_found() {
        ValueListFromFiles::new(FakeBaseDir::default(), FakeResolver::default())
            .to_value_list(vec![(
                ident("a"),
                FilesGlobReferences::new(vec![files_attr("no_mater")], Default::default(), true),
            )])
            .unwrap();
    }

    #[test]
    #[should_panic(expected = "glob failed")]
    fn default_glob_resolver_raise_error_if_invalid_glob_path() {
        DefaultGlobResolver.glob("/invalid/path/***").unwrap();
    }
}
