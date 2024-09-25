#![allow(clippy::too_many_lines)]

use super::*;

use std::path::Path;
use std::rc::Rc;
use std::sync::Arc;

macro_rules! t(
    ($path:expr, iter: $iter:expr) => (
        {
            let path = RelativePath::new($path);

            // Forward iteration
            let comps = path.iter().map(str::to_string).collect::<Vec<String>>();
            let exp: &[&str] = &$iter;
            let exps = exp.iter().map(|s| s.to_string()).collect::<Vec<String>>();
            assert!(comps == exps, "iter: Expected {:?}, found {:?}",
                    exps, comps);

            // Reverse iteration
            let comps = RelativePath::new($path).iter().rev().map(str::to_string)
                .collect::<Vec<String>>();
            let exps = exps.into_iter().rev().collect::<Vec<String>>();
            assert!(comps == exps, "iter().rev(): Expected {:?}, found {:?}",
                    exps, comps);
        }
    );

    ($path:expr, parent: $parent:expr, file_name: $file:expr) => (
        {
            let path = RelativePath::new($path);

            let parent = path.parent().map(|p| p.as_str());
            let exp_parent: Option<&str> = $parent;
            assert!(parent == exp_parent, "parent: Expected {:?}, found {:?}",
                    exp_parent, parent);

            let file = path.file_name();
            let exp_file: Option<&str> = $file;
            assert!(file == exp_file, "file_name: Expected {:?}, found {:?}",
                    exp_file, file);
        }
    );

    ($path:expr, file_stem: $file_stem:expr, extension: $extension:expr) => (
        {
            let path = RelativePath::new($path);

            let stem = path.file_stem();
            let exp_stem: Option<&str> = $file_stem;
            assert!(stem == exp_stem, "file_stem: Expected {:?}, found {:?}",
                    exp_stem, stem);

            let ext = path.extension();
            let exp_ext: Option<&str> = $extension;
            assert!(ext == exp_ext, "extension: Expected {:?}, found {:?}",
                    exp_ext, ext);
        }
    );

    ($path:expr, iter: $iter:expr,
                    parent: $parent:expr, file_name: $file:expr,
                    file_stem: $file_stem:expr, extension: $extension:expr) => (
        {
            t!($path, iter: $iter);
            t!($path, parent: $parent, file_name: $file);
            t!($path, file_stem: $file_stem, extension: $extension);
        }
    );
);

fn assert_components(components: &[&str], path: &RelativePath) {
    let components = components
        .iter()
        .copied()
        .map(Component::Normal)
        .collect::<Vec<_>>();
    let result: Vec<_> = path.components().collect();
    assert_eq!(&components[..], &result[..]);
}

fn rp(input: &str) -> &RelativePath {
    RelativePath::new(input)
}

#[test]
#[allow(clippy::cognitive_complexity)]
pub fn test_decompositions() {
    t!("",
    iter: [],
    parent: None,
    file_name: None,
    file_stem: None,
    extension: None
    );

    t!("foo",
    iter: ["foo"],
    parent: Some(""),
    file_name: Some("foo"),
    file_stem: Some("foo"),
    extension: None
    );

    t!("/",
    iter: [],
    parent: Some(""),
    file_name: None,
    file_stem: None,
    extension: None
    );

    t!("/foo",
    iter: ["foo"],
    parent: Some(""),
    file_name: Some("foo"),
    file_stem: Some("foo"),
    extension: None
    );

    t!("foo/",
    iter: ["foo"],
    parent: Some(""),
    file_name: Some("foo"),
    file_stem: Some("foo"),
    extension: None
    );

    t!("/foo/",
    iter: ["foo"],
    parent: Some(""),
    file_name: Some("foo"),
    file_stem: Some("foo"),
    extension: None
    );

    t!("foo/bar",
    iter: ["foo", "bar"],
    parent: Some("foo"),
    file_name: Some("bar"),
    file_stem: Some("bar"),
    extension: None
    );

    t!("/foo/bar",
    iter: ["foo", "bar"],
    parent: Some("/foo"),
    file_name: Some("bar"),
    file_stem: Some("bar"),
    extension: None
    );

    t!("///foo///",
    iter: ["foo"],
    parent: Some(""),
    file_name: Some("foo"),
    file_stem: Some("foo"),
    extension: None
    );

    t!("///foo///bar",
    iter: ["foo", "bar"],
    parent: Some("///foo"),
    file_name: Some("bar"),
    file_stem: Some("bar"),
    extension: None
    );

    t!("./.",
    iter: [".", "."],
    parent: Some(""),
    file_name: None,
    file_stem: None,
    extension: None
    );

    t!("/..",
    iter: [".."],
    parent: Some(""),
    file_name: None,
    file_stem: None,
    extension: None
    );

    t!("../",
    iter: [".."],
    parent: Some(""),
    file_name: None,
    file_stem: None,
    extension: None
    );

    t!("foo/.",
    iter: ["foo", "."],
    parent: Some(""),
    file_name: Some("foo"),
    file_stem: Some("foo"),
    extension: None
    );

    t!("foo/..",
    iter: ["foo", ".."],
    parent: Some("foo"),
    file_name: None,
    file_stem: None,
    extension: None
    );

    t!("foo/./",
    iter: ["foo", "."],
    parent: Some(""),
    file_name: Some("foo"),
    file_stem: Some("foo"),
    extension: None
    );

    t!("foo/./bar",
    iter: ["foo", ".", "bar"],
    parent: Some("foo/."),
    file_name: Some("bar"),
    file_stem: Some("bar"),
    extension: None
    );

    t!("foo/../",
    iter: ["foo", ".."],
    parent: Some("foo"),
    file_name: None,
    file_stem: None,
    extension: None
    );

    t!("foo/../bar",
    iter: ["foo", "..", "bar"],
    parent: Some("foo/.."),
    file_name: Some("bar"),
    file_stem: Some("bar"),
    extension: None
    );

    t!("./a",
    iter: [".", "a"],
    parent: Some("."),
    file_name: Some("a"),
    file_stem: Some("a"),
    extension: None
    );

    t!(".",
    iter: ["."],
    parent: Some(""),
    file_name: None,
    file_stem: None,
    extension: None
    );

    t!("./",
    iter: ["."],
    parent: Some(""),
    file_name: None,
    file_stem: None,
    extension: None
    );

    t!("a/b",
    iter: ["a", "b"],
    parent: Some("a"),
    file_name: Some("b"),
    file_stem: Some("b"),
    extension: None
    );

    t!("a//b",
    iter: ["a", "b"],
    parent: Some("a"),
    file_name: Some("b"),
    file_stem: Some("b"),
    extension: None
    );

    t!("a/./b",
    iter: ["a", ".", "b"],
    parent: Some("a/."),
    file_name: Some("b"),
    file_stem: Some("b"),
    extension: None
    );

    t!("a/b/c",
    iter: ["a", "b", "c"],
    parent: Some("a/b"),
    file_name: Some("c"),
    file_stem: Some("c"),
    extension: None
    );

    t!(".foo",
    iter: [".foo"],
    parent: Some(""),
    file_name: Some(".foo"),
    file_stem: Some(".foo"),
    extension: None
    );
}

#[test]
pub fn test_stem_ext() {
    t!("foo",
    file_stem: Some("foo"),
    extension: None
    );

    t!("foo.",
    file_stem: Some("foo"),
    extension: Some("")
    );

    t!(".foo",
    file_stem: Some(".foo"),
    extension: None
    );

    t!("foo.txt",
    file_stem: Some("foo"),
    extension: Some("txt")
    );

    t!("foo.bar.txt",
    file_stem: Some("foo.bar"),
    extension: Some("txt")
    );

    t!("foo.bar.",
    file_stem: Some("foo.bar"),
    extension: Some("")
    );

    t!(".", file_stem: None, extension: None);

    t!("..", file_stem: None, extension: None);

    t!("", file_stem: None, extension: None);
}

#[test]
pub fn test_set_file_name() {
    macro_rules! tfn(
            ($path:expr, $file:expr, $expected:expr) => ( {
            let mut p = RelativePathBuf::from($path);
            p.set_file_name($file);
            assert!(p.as_str() == $expected,
                    "setting file name of {:?} to {:?}: Expected {:?}, got {:?}",
                    $path, $file, $expected,
                    p.as_str());
        });
    );

    tfn!("foo", "foo", "foo");
    tfn!("foo", "bar", "bar");
    tfn!("foo", "", "");
    tfn!("", "foo", "foo");

    tfn!(".", "foo", "./foo");
    tfn!("foo/", "bar", "bar");
    tfn!("foo/.", "bar", "bar");
    tfn!("..", "foo", "../foo");
    tfn!("foo/..", "bar", "foo/../bar");
    tfn!("/", "foo", "/foo");
}

#[test]
pub fn test_set_extension() {
    macro_rules! tse(
            ($path:expr, $ext:expr, $expected:expr, $output:expr) => ( {
            let mut p = RelativePathBuf::from($path);
            let output = p.set_extension($ext);
            assert!(p.as_str() == $expected && output == $output,
                    "setting extension of {:?} to {:?}: Expected {:?}/{:?}, got {:?}/{:?}",
                    $path, $ext, $expected, $output,
                    p.as_str(), output);
        });
    );

    tse!("foo", "txt", "foo.txt", true);
    tse!("foo.bar", "txt", "foo.txt", true);
    tse!("foo.bar.baz", "txt", "foo.bar.txt", true);
    tse!(".test", "txt", ".test.txt", true);
    tse!("foo.txt", "", "foo", true);
    tse!("foo", "", "foo", true);
    tse!("", "foo", "", false);
    tse!(".", "foo", ".", false);
    tse!("foo/", "bar", "foo.bar", true);
    tse!("foo/.", "bar", "foo.bar", true);
    tse!("..", "foo", "..", false);
    tse!("foo/..", "bar", "foo/..", false);
    tse!("/", "foo", "/", false);
}

#[test]
fn test_eq_recievers() {
    use std::borrow::Cow;

    let borrowed: &RelativePath = RelativePath::new("foo/bar");
    let mut owned: RelativePathBuf = RelativePathBuf::new();
    owned.push("foo");
    owned.push("bar");
    let borrowed_cow: Cow<RelativePath> = borrowed.into();
    let owned_cow: Cow<RelativePath> = owned.clone().into();

    macro_rules! t {
        ($($current:expr),+) => {
            $(
                assert_eq!($current, borrowed);
                assert_eq!($current, owned);
                assert_eq!($current, borrowed_cow);
                assert_eq!($current, owned_cow);
            )+
        }
    }

    t!(borrowed, owned, borrowed_cow, owned_cow);
}

#[test]
#[allow(clippy::cognitive_complexity)]
pub fn test_compare() {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};

    fn hash<T: Hash>(t: T) -> u64 {
        let mut s = DefaultHasher::new();
        t.hash(&mut s);
        s.finish()
    }

    macro_rules! tc(
        ($path1:expr, $path2:expr, eq: $eq:expr,
            starts_with: $starts_with:expr, ends_with: $ends_with:expr,
            relative_from: $relative_from:expr) => ({
                let path1 = RelativePath::new($path1);
                let path2 = RelativePath::new($path2);

                let eq = path1 == path2;
                assert!(eq == $eq, "{:?} == {:?}, expected {:?}, got {:?}",
                        $path1, $path2, $eq, eq);
                assert!($eq == (hash(path1) == hash(path2)),
                        "{:?} == {:?}, expected {:?}, got {} and {}",
                        $path1, $path2, $eq, hash(path1), hash(path2));

                let starts_with = path1.starts_with(path2);
                assert!(starts_with == $starts_with,
                        "{:?}.starts_with({:?}), expected {:?}, got {:?}", $path1, $path2,
                        $starts_with, starts_with);

                let ends_with = path1.ends_with(path2);
                assert!(ends_with == $ends_with,
                        "{:?}.ends_with({:?}), expected {:?}, got {:?}", $path1, $path2,
                        $ends_with, ends_with);

                let relative_from = path1.strip_prefix(path2)
                                        .map(|p| p.as_str())
                                        .ok();
                let exp: Option<&str> = $relative_from;
                assert!(relative_from == exp,
                        "{:?}.strip_prefix({:?}), expected {:?}, got {:?}",
                        $path1, $path2, exp, relative_from);
        });
    );

    tc!("", "",
    eq: true,
    starts_with: true,
    ends_with: true,
    relative_from: Some("")
    );

    tc!("foo", "",
    eq: false,
    starts_with: true,
    ends_with: true,
    relative_from: Some("foo")
    );

    tc!("", "foo",
    eq: false,
    starts_with: false,
    ends_with: false,
    relative_from: None
    );

    tc!("foo", "foo",
    eq: true,
    starts_with: true,
    ends_with: true,
    relative_from: Some("")
    );

    tc!("foo/", "foo",
    eq: true,
    starts_with: true,
    ends_with: true,
    relative_from: Some("")
    );

    tc!("foo/bar", "foo",
    eq: false,
    starts_with: true,
    ends_with: false,
    relative_from: Some("bar")
    );

    tc!("foo/bar/baz", "foo/bar",
    eq: false,
    starts_with: true,
    ends_with: false,
    relative_from: Some("baz")
    );

    tc!("foo/bar", "foo/bar/baz",
    eq: false,
    starts_with: false,
    ends_with: false,
    relative_from: None
    );
}

#[test]
fn test_join() {
    assert_components(&["foo", "bar", "baz"], &rp("foo/bar").join("baz///"));
    assert_components(
        &["hello", "world", "foo", "bar", "baz"],
        &rp("hello/world").join("///foo/bar/baz"),
    );
    assert_components(&["foo", "bar", "baz"], &rp("").join("foo/bar/baz"));
}

#[test]
fn test_components_iterator() {
    use self::Component::*;

    assert_eq!(
        vec![Normal("hello"), Normal("world")],
        rp("/hello///world//").components().collect::<Vec<_>>()
    );
}

#[test]
fn test_to_path_buf() {
    let path = rp("/hello///world//");
    let path_buf = path.to_path(".");
    let expected = Path::new(".").join("hello").join("world");
    assert_eq!(expected, path_buf);
}

#[test]
fn test_eq() {
    assert_eq!(rp("//foo///bar"), rp("/foo/bar"));
    assert_eq!(rp("foo///bar"), rp("foo/bar"));
    assert_eq!(rp("foo"), rp("foo"));
    assert_eq!(rp("foo"), rp("foo").to_relative_path_buf());
}

#[test]
fn test_next_back() {
    use self::Component::*;

    let mut it = rp("baz/bar///foo").components();
    assert_eq!(Some(Normal("foo")), it.next_back());
    assert_eq!(Some(Normal("bar")), it.next_back());
    assert_eq!(Some(Normal("baz")), it.next_back());
    assert_eq!(None, it.next_back());
}

#[test]
fn test_parent() {
    let path = rp("baz/./bar/foo//./.");

    assert_eq!(Some(rp("baz/./bar")), path.parent());
    assert_eq!(
        Some(rp("baz/.")),
        path.parent().and_then(RelativePath::parent)
    );
    assert_eq!(
        Some(rp("")),
        path.parent()
            .and_then(RelativePath::parent)
            .and_then(RelativePath::parent)
    );
    assert_eq!(
        None,
        path.parent()
            .and_then(RelativePath::parent)
            .and_then(RelativePath::parent)
            .and_then(RelativePath::parent)
    );
}

#[test]
fn test_relative_path_buf() {
    assert_eq!(
        rp("hello/world/."),
        rp("/hello///world//").to_owned().join(".")
    );
}

#[test]
fn test_normalize() {
    assert_eq!(rp("c/d"), rp("a/.././b/../c/d").normalize());
}

#[test]
fn test_relative_to() {
    assert_eq!(
        rp("foo/foo/bar"),
        rp("foo/bar").join_normalized("../foo/bar")
    );

    assert_eq!(
        rp("../c/e"),
        rp("x/y").join_normalized("../../a/b/../../../c/d/../e")
    );
}

#[test]
fn test_from() {
    assert_eq!(
        rp("foo/bar").to_owned(),
        RelativePathBuf::from(String::from("foo/bar")),
    );

    assert_eq!(
        RelativePathBuf::from(rp("foo/bar")),
        RelativePathBuf::from("foo/bar"),
    );

    assert_eq!(rp("foo/bar").to_owned(), RelativePathBuf::from("foo/bar"),);

    assert_eq!(&*Box::<RelativePath>::from(rp("foo/bar")), rp("foo/bar"));
    assert_eq!(
        &*Box::<RelativePath>::from(RelativePathBuf::from("foo/bar")),
        rp("foo/bar")
    );

    assert_eq!(&*Arc::<RelativePath>::from(rp("foo/bar")), rp("foo/bar"));
    assert_eq!(
        &*Arc::<RelativePath>::from(RelativePathBuf::from("foo/bar")),
        rp("foo/bar")
    );

    assert_eq!(&*Rc::<RelativePath>::from(rp("foo/bar")), rp("foo/bar"));
    assert_eq!(
        &*Rc::<RelativePath>::from(RelativePathBuf::from("foo/bar")),
        rp("foo/bar")
    );
}

#[test]
fn test_relative_path_asref_str() {
    assert_eq!(
        <RelativePath as AsRef<str>>::as_ref(rp("foo/bar")),
        "foo/bar"
    );
}

#[test]
fn test_default() {
    assert_eq!(RelativePathBuf::new(), RelativePathBuf::default(),);
}

#[test]
pub fn test_push() {
    macro_rules! tp(
        ($path:expr, $push:expr, $expected:expr) => ( {
            let mut actual = RelativePathBuf::from($path);
            actual.push($push);
            assert!(actual.as_str() == $expected,
                    "pushing {:?} onto {:?}: Expected {:?}, got {:?}",
                    $push, $path, $expected, actual.as_str());
        });
    );

    tp!("", "foo", "foo");
    tp!("foo", "bar", "foo/bar");
    tp!("foo/", "bar", "foo/bar");
    tp!("foo//", "bar", "foo//bar");
    tp!("foo/.", "bar", "foo/./bar");
    tp!("foo./.", "bar", "foo././bar");
    tp!("foo", "", "foo/");
    tp!("foo", ".", "foo/.");
    tp!("foo", "..", "foo/..");
}

#[test]
pub fn test_pop() {
    macro_rules! tp(
        ($path:expr, $expected:expr, $output:expr) => ( {
            let mut actual = RelativePathBuf::from($path);
            let output = actual.pop();
            assert!(actual.as_str() == $expected && output == $output,
                    "popping from {:?}: Expected {:?}/{:?}, got {:?}/{:?}",
                    $path, $expected, $output,
                    actual.as_str(), output);
        });
    );

    tp!("", "", false);
    tp!("/", "", true);
    tp!("foo", "", true);
    tp!(".", "", true);
    tp!("/foo", "", true);
    tp!("/foo/bar", "/foo", true);
    tp!("/foo/bar/.", "/foo", true);
    tp!("foo/bar", "foo", true);
    tp!("foo/.", "", true);
    tp!("foo//bar", "foo", true);
}

#[test]
pub fn test_display() {
    // NB: display delegated to the underlying string.
    assert_eq!(RelativePathBuf::from("foo/bar").to_string(), "foo/bar");
    assert_eq!(RelativePath::new("foo/bar").to_string(), "foo/bar");

    assert_eq!(format!("{}", RelativePathBuf::from("foo/bar")), "foo/bar");
    assert_eq!(format!("{}", RelativePath::new("foo/bar")), "foo/bar");
}

#[cfg(unix)]
#[test]
pub fn test_unix_from_path() {
    use std::ffi::OsStr;
    use std::os::unix::ffi::OsStrExt;

    assert_eq!(
        Err(FromPathErrorKind::NonRelative.into()),
        RelativePath::from_path("/foo/bar")
    );

    // Continuation byte without continuation.
    let non_utf8 = OsStr::from_bytes(&[0x80u8]);

    assert_eq!(
        Err(FromPathErrorKind::NonUtf8.into()),
        RelativePath::from_path(non_utf8)
    );
}

#[cfg(windows)]
#[test]
pub fn test_windows_from_path() {
    assert_eq!(
        Err(FromPathErrorKind::NonRelative.into()),
        RelativePath::from_path("c:\\foo\\bar")
    );

    assert_eq!(
        Err(FromPathErrorKind::BadSeparator.into()),
        RelativePath::from_path("foo\\bar")
    );
}

#[cfg(unix)]
#[test]
pub fn test_unix_owned_from_path() {
    use std::ffi::OsStr;
    use std::os::unix::ffi::OsStrExt;

    assert_eq!(
        Err(FromPathErrorKind::NonRelative.into()),
        RelativePathBuf::from_path(Path::new("/foo/bar"))
    );

    // Continuation byte without continuation.
    let non_utf8 = OsStr::from_bytes(&[0x80u8]);

    assert_eq!(
        Err(FromPathErrorKind::NonUtf8.into()),
        RelativePathBuf::from_path(Path::new(non_utf8))
    );
}

#[cfg(windows)]
#[test]
pub fn test_windows_owned_from_path() {
    assert_eq!(
        Err(FromPathErrorKind::NonRelative.into()),
        RelativePathBuf::from_path(Path::new("c:\\foo\\bar"))
    );
}
