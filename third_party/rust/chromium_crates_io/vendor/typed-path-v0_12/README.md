# Typed Path

[![Crates.io][crates_img]][crates_lnk] [![Docs.rs][doc_img]][doc_lnk] [![CI][ci_img]][ci_lnk] [![RustC 1.58.1+][rustc_img]][rustc_lnk] 

[crates_img]: https://img.shields.io/crates/v/typed-path.svg
[crates_lnk]: https://crates.io/crates/typed-path
[doc_img]: https://docs.rs/typed-path/badge.svg
[doc_lnk]: https://docs.rs/typed-path
[ci_img]: https://github.com/chipsenkbeil/typed-path/actions/workflows/ci.yml/badge.svg
[ci_lnk]: https://github.com/chipsenkbeil/typed-path/actions/workflows/ci.yml
[rustc_img]: https://img.shields.io/badge/rustc_1.65.0+-lightgray.svg
[rustc_lnk]: https://blog.rust-lang.org/2022/11/03/Rust-1.65.0/

Provides typed variants of [`Path`][StdPath] and [`PathBuf`][StdPathBuf] for
Unix and Windows.

## Install

```toml
[dependencies]
typed-path = "0.12"
```

As of version `0.7`, this library also supports `no_std` environments that
depend on `alloc`. To build in this manner, remove the default `std` feature:

```toml
[dependencies]
typed-path = { version = "...", default-features = false }
```

## Why?

> Some applications need to manipulate Windows or UNIX paths on different
> platforms, for a variety of reasons: constructing portable file formats,
> parsing files from other platforms, handling archive formats, working with
> certain network protocols, and so on.
>
> -- Josh Triplett

[Check out this issue](https://github.com/rust-lang/rust/issues/66621) of a
discussion for this. The functionality actually exists within the standard
library, but is not exposed!

This means that parsing a path like `C:\path\to\file.txt` will be parsed
differently by [`std::path::Path`][StdPath] depending on which platform you are
on!

```rust
use std::path::Path;

// On Windows, this prints out:
//
// * Prefix(PrefixComponent { raw: "C:", parsed: Disk(67) })
// * RootDir
// * Normal("path")
// * Normal("to")
// * Normal("file.txt")]
//
// But on Unix, this prints out:
//
// * Normal("C:\\path\\to\\file.txt")
let path = Path::new(r"C:\path\to\file.txt");
for component in path.components() {
    println!("* {:?}", component);
}
```

## Usage

### Byte paths

The library provides a generic [`Path<T>`][Path] and [`PathBuf<T>`][PathBuf]
that use `[u8]` and `Vec<u8>` underneath instead of `OsStr` and `OsString`. An
encoding generic type is provided to dictate how the underlying bytes are
parsed in order to support consistent path functionality no matter what
operating system you are compiling against!

```rust
use typed_path::WindowsPath;

// On all platforms, this prints out:
//
// * Prefix(PrefixComponent { raw: "C:", parsed: Disk(67) })
// * RootDir
// * Normal("path")
// * Normal("to")
// * Normal("file.txt")]
//
let path = WindowsPath::new(r"C:\path\to\file.txt");
for component in path.components() {
    println!("* {:?}", component);
}
```

### UTF8-enforced paths

Alongside the byte paths, this library also supports UTF8-enforced paths
through [`Utf8Path<T>`][Utf8Path] and [`Utf8PathBuf<T>`][Utf8PathBuf], which
internally use `str` and `String`. An encoding generic type is provided to
dictate how the underlying characters are parsed in order to support consistent
path functionality no matter what operating system you are compiling against!

```rust
use typed_path::Utf8WindowsPath;

// On all platforms, this prints out:
//
// * Prefix(Utf8WindowsPrefixComponent { raw: "C:", parsed: Disk(67) })
// * RootDir
// * Normal("path")
// * Normal("to")
// * Normal("file.txt")]
//
let path = Utf8WindowsPath::new(r"C:\path\to\file.txt");
for component in path.components() {
    println!("* {:?}", component);
}
```

### Checking paths

When working with user-defined paths, there is an additional layer of defense needed to prevent abuse to avoid [path traversal attacks](https://owasp.org/www-community/attacks/Path_Traversal) and other risks.

To that end, you can use `PathBuf::push_checked` and `Path::join_checked` (and equivalents) to ensure that the paths being created do not alter pre-existing paths in unexpected ways.

```rust
use typed_path::{CheckedPathError, UnixPath, UnixPathBuf};

let path = UnixPath::new("/etc");

// A valid path can be joined onto the existing one
assert_eq!(path.join_checked("passwd"), Ok(UnixPathBuf::from("/etc/passwd")));

// An invalid path will result in an error
assert_eq!(
    path.join_checked("/sneaky/replacement"), 
    Err(CheckedPathError::UnexpectedRoot)
);

let mut path = UnixPathBuf::from("/etc");

// Pushing a relative path that contains parent directory references that cannot be
// resolved within the path is considered an error as this is considered a path
// traversal attack!
assert_eq!(
    path.push_checked(".."), 
    Err(CheckedPathError::PathTraversalAttack)
);
assert_eq!(path, UnixPathBuf::from("/etc"));

// Pushing an absolute path will fail with an error
assert_eq!(
    path.push_checked("/sneaky/replacement"), 
    Err(CheckedPathError::UnexpectedRoot)
);
assert_eq!(path, UnixPathBuf::from("/etc"));

// Pushing a relative path that is safe will succeed
assert!(path.push_checked("abc/../def").is_ok());
assert_eq!(path, UnixPathBuf::from("/etc/abc/../def"));
```

### Converting between encodings

There may be times in which you need to convert between encodings such as when
you want to load a native path and convert it into another format. In that
case, you can use the `with_encoding` method (or specific variants like
`with_unix_encoding` and `with_windows_encoding`) to convert a [`Path`][Path]
or [`Utf8Path`][Utf8Path] into their respective [`PathBuf`][PathBuf] and
[`Utf8PathBuf`][Utf8PathBuf] with an explicit encoding:

```rust
use typed_path::{Utf8WindowsPath, Utf8UnixPath};

// Convert from Unix to Windows
let unix_path = Utf8UnixPath::new("/tmp/foo.txt");
let windows_path = unix_path.with_windows_encoding();
assert_eq!(windows_path, Utf8WindowsPath::new(r"\tmp\foo.txt"));

// Converting from Windows to Unix will drop any prefix
let windows_path = Utf8WindowsPath::new(r"C:\tmp\foo.txt");
let unix_path = windows_path.with_unix_encoding();
assert_eq!(unix_path, Utf8UnixPath::new(r"/tmp/foo.txt"));

// Converting to itself should retain everything
let path = Utf8WindowsPath::new(r"C:\tmp\foo.txt");
assert_eq!(
    path.with_windows_encoding(),
    Utf8WindowsPath::new(r"C:\tmp\foo.txt"),
);
```

Like with pushing and joining paths using *checked* variants, we can also ensure that paths created from changing encodings are still valid:

```rust
use typed_path::{CheckedPathError, Utf8UnixPath, Utf8WindowsPath};

// Convert from Unix to Windows
let unix_path = Utf8UnixPath::new("/tmp/foo.txt");
let windows_path = unix_path.with_windows_encoding_checked().unwrap();
assert_eq!(windows_path, Utf8WindowsPath::new(r"\tmp\foo.txt"));

// Convert from Unix to Windows will fail if there are characters that are valid in Unix but not in Windows
let unix_path = Utf8UnixPath::new("/tmp/|invalid|/foo.txt");
assert_eq!(
    unix_path.with_windows_encoding_checked(),
    Err(CheckedPathError::InvalidFilename),
);
```

### Typed Paths

In the above examples, we were using paths where the encoding (Unix or Windows)
was known at compile time. There may be situations where we need runtime
support to decide and switch between encodings. For that, this crate provides
the [`TypedPath`][TypedPath] and [`TypedPathBuf`][TypedPathBuf] enumerations
(and their [`Utf8TypedPath`][Utf8TypedPath] and
[`Utf8TypedPathBuf`][Utf8TypedPathBuf] variations):

```rust
use typed_path::Utf8TypedPath;

// Derive the path by determining if it is Unix or Windows
let path = Utf8TypedPath::derive(r"C:\path\to\file.txt");
assert!(path.is_windows());

// Change the encoding to Unix
let path = path.with_unix_encoding();
assert_eq!(path, "/path/to/file.txt");
```

### Normalization

Alongside implementing the standard methods associated with [`Path`][StdPath]
and [`PathBuf`][StdPathBuf] from the standard library, this crate also
implements several additional methods including the ability to normalize a path
by resolving `.` and `..` without the need to have the path exist.


```rust
use typed_path::Utf8UnixPath;

assert_eq!(
    Utf8UnixPath::new("foo/bar//baz/./asdf/quux/..").normalize(),
    Utf8UnixPath::new("foo/bar/baz/asdf"),
);
```

In addition, you can leverage `absolutize` to convert a path to an absolute
form by prepending the current working directory if the path is relative and
then normalizing it (requires `std` feature):

```rust
use typed_path::{utils, Utf8UnixPath};

// With an absolute path, it is just normalized
// NOTE: This requires `std` feature, otherwise `absolutize` is missing!
let path = Utf8UnixPath::new("/a/b/../c/./d");
assert_eq!(path.absolutize().unwrap(), Utf8UnixPath::new("/a/c/d"));

// With a relative path, it is first joined with the current working directory
// and then normalized
// NOTE: This requires `std` feature, otherwise `utf8_current_dir` and
//       `absolutize` are missing!
let cwd = utils::utf8_current_dir().unwrap().with_unix_encoding();
let path = cwd.join(Utf8UnixPath::new("a/b/../c/./d"));
assert_eq!(path.absolutize().unwrap(), cwd.join(Utf8UnixPath::new("a/c/d")));
```

### Utility Functions

Helper functions are available in the [`utils`][utils] module (requires `std`
feature).

Today, there are three mirrored methods to those found in
[`std::env`](https://doc.rust-lang.org/std/env/index.html):

- [`std::env::current_dir`](https://doc.rust-lang.org/std/env/fn.current_dir.html)
- [`std::env::current_exe`](https://doc.rust-lang.org/std/env/fn.current_exe.html)
- [`std::env::temp_dir`](https://doc.rust-lang.org/std/env/fn.temp_dir.html)

Each has an implementation to produce a [`NativePathBuf`][NativePathBuf] and a
[`Utf8NativePathBuf`][Utf8NativePathBuf].

#### Current directory

```rust
// Retrieves the current directory as a NativePathBuf:
//
// * For Unix family, this would be UnixPathBuf
// * For Windows family, this would be WindowsPathBuf
//
// NOTE: This requires `std` feature, otherwise `current_dir` is missing!
let _cwd = typed_path::utils::current_dir().unwrap();

// Retrieves the current directory as a Utf8NativePathBuf:
//
// * For Unix family, this would be Utf8UnixPathBuf
// * For Windows family, this would be Utf8WindowsPathBuf
//
// NOTE: This requires `std` feature, otherwise `utf8_current_dir` is missing!
let _utf8_cwd = typed_path::utils::utf8_current_dir().unwrap();
```

#### Current exe

```rust
// Returns the full filesystem path of the current running executable as a NativePathBuf:
//
// * For Unix family, this would be UnixPathBuf
// * For Windows family, this would be WindowsPathBuf
//
// NOTE: This requires `std` feature, otherwise `current_exe` is missing!
let _exe = typed_path::utils::current_exe().unwrap();

// Returns the full filesystem path of the current running executable as a Utf8NativePathBuf:
//
// * For Unix family, this would be Utf8UnixPathBuf
// * For Windows family, this would be Utf8WindowsPathBuf
//
// NOTE: This requires `std` feature, otherwise `utf8_current_exe` is missing!
let _utf8_exe = typed_path::utils::utf8_current_exe().unwrap();
```

#### Temporary directory

```rust
// Returns the path of a temporary directory as a NativePathBuf:
//
// * For Unix family, this would be UnixPathBuf
// * For Windows family, this would be WindowsPathBuf
//
// NOTE: This requires `std` feature, otherwise `temp_dir` is missing!
let _temp_dir = typed_path::utils::temp_dir().unwrap();

// Returns the path of a temporary directory as a Utf8NativePathBuf:
//
// * For Unix family, this would be Utf8UnixPathBuf
// * For Windows family, this would be Utf8WindowsPathBuf
//
// NOTE: This requires `std` feature, otherwise `utf8_temp_dir` is missing!
let _utf8_temp_dir = typed_path::utils::utf8_temp_dir().unwrap();
```

## License

This project is licensed under either of

Apache License, Version 2.0, (LICENSE-APACHE or
[apache-license][apache-license]) MIT license (LICENSE-MIT or
[mit-license][mit-license]) at your option.

[apache-license]: http://www.apache.org/licenses/LICENSE-2.0
[mit-license]: http://opensource.org/licenses/MIT

[StdPath]: https://doc.rust-lang.org/std/path/struct.Path.html
[StdPathBuf]: https://doc.rust-lang.org/std/path/struct.PathBuf.html
[Path]: https://docs.rs/typed-path/latest/typed_path/struct.Path.html
[PathBuf]: https://docs.rs/typed-path/latest/typed_path/struct.PathBuf.html
[Utf8Path]: https://docs.rs/typed-path/latest/typed_path/struct.Utf8Path.html
[Utf8PathBuf]: https://docs.rs/typed-path/latest/typed_path/struct.Utf8PathBuf.html
[UnixPath]: https://docs.rs/typed-path/latest/typed_path/type.UnixPath.html
[UnixPathBuf]: https://docs.rs/typed-path/latest/typed_path/type.UnixPathBuf.html
[Utf8UnixPath]: https://docs.rs/typed-path/latest/typed_path/type.Utf8UnixPath.html
[Utf8UnixPathBuf]: https://docs.rs/typed-path/latest/typed_path/type.Utf8UnixPathBuf.html
[WindowsPath]: https://docs.rs/typed-path/latest/typed_path/type.WindowsPath.html
[WindowsPathBuf]: https://docs.rs/typed-path/latest/typed_path/type.WindowsPathBuf.html
[Utf8WindowsPath]: https://docs.rs/typed-path/latest/typed_path/type.Utf8WindowsPath.html
[Utf8WindowsPathBuf]: https://docs.rs/typed-path/latest/typed_path/type.Utf8WindowsPathBuf.html
[TypedPath]: https://docs.rs/typed-path/latest/typed_path/enum.TypedPath.html
[TypedPathBuf]: https://docs.rs/typed-path/latest/typed_path/enum.TypedPathBuf.html
[Utf8TypedPath]: https://docs.rs/typed-path/latest/typed_path/enum.Utf8TypedPath.html
[Utf8TypedPathBuf]: https://docs.rs/typed-path/latest/typed_path/enum.Utf8TypedPathBuf.html
[NativePathBuf]: https://docs.rs/typed-path/latest/typed_path/type.NativePathBuf.html
[Utf8NativePathBuf]: https://docs.rs/typed-path/latest/typed_path/type.Utf8NativePathBuf.html
[utils]: https://docs.rs/typed-path/latest/typed_path/utils/index.html
