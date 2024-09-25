# relative-path

[<img alt="github" src="https://img.shields.io/badge/github-udoprog/relative--path-8da0cb?style=for-the-badge&logo=github" height="20">](https://github.com/udoprog/relative-path)
[<img alt="crates.io" src="https://img.shields.io/crates/v/relative-path.svg?style=for-the-badge&color=fc8d62&logo=rust" height="20">](https://crates.io/crates/relative-path)
[<img alt="docs.rs" src="https://img.shields.io/badge/docs.rs-relative--path-66c2a5?style=for-the-badge&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K" height="20">](https://docs.rs/relative-path)
[<img alt="build status" src="https://img.shields.io/github/actions/workflow/status/udoprog/relative-path/ci.yml?branch=main&style=for-the-badge" height="20">](https://github.com/udoprog/relative-path/actions?query=branch%3Amain)

Portable relative UTF-8 paths for Rust.

This crate provides a module analogous to [`std::path`], with the following
characteristics:

* The path separator is set to a fixed character (`/`), regardless of
  platform.
* Relative paths cannot represent a path in the filesystem without first
  specifying *what they are relative to* using functions such as [`to_path`]
  and [`to_logical_path`].
* Relative paths are always guaranteed to be valid UTF-8 strings.

On top of this we support many operations that guarantee the same behavior
across platforms.

For more utilities to manipulate relative paths, see the
[`relative-path-utils` crate].

<br>

## Usage

Add `relative-path` to your `Cargo.toml`:

```toml
relative-path = "1.9.2"
```

Start using relative paths:

```rust
use serde::{Serialize, Deserialize};
use relative_path::RelativePath;

#[derive(Serialize, Deserialize)]
struct Manifest<'a> {
    #[serde(borrow)]
    source: &'a RelativePath,
}

```

<br>

## Serde Support

This library includes serde support that can be enabled with the `serde`
feature.

<br>

## Why is `std::path` a portability hazard?

Path representations differ across platforms.

* Windows permits using drive volumes (multiple roots) as a prefix (e.g.
  `"c:\"`) and backslash (`\`) as a separator.
* Unix references absolute paths from a single root and uses forward slash
  (`/`) as a separator.

If we use `PathBuf`, Storing paths in a manifest would allow our application
to build and run on one platform but potentially not others.

Consider the following data model and corresponding toml for a manifest:

```rust
use std::path::PathBuf;

use serde::{Serialize, Deserialize};

#[derive(Serialize, Deserialize)]
struct Manifest {
    source: PathBuf,
}
```

```toml
source = "C:\\Users\\udoprog\\repo\\data\\source"
```

This will run for you (assuming `source` exists). So you go ahead and check
the manifest into git. The next day your Linux colleague calls you and
wonders what they have ever done to wrong you?

So what went wrong? Well two things. You forgot to make the `source`
relative, so anyone at the company which has a different username than you
won't be able to use it. So you go ahead and fix that:

```toml
source = "data\\source"
```

But there is still one problem! A backslash (`\`) is only a legal path
separator on Windows. Luckily you learn that forward slashes are supported
both on Windows *and* Linux. So you opt for:

```toml
source = "data/source"
```

Things are working now. So all is well... Right? Sure, but we can do better.

This crate provides types that work with *portable relative paths* (hence
the name). So by using [`RelativePath`] we can systematically help avoid
portability issues like the one above. Avoiding issues at the source is
preferably over spending 5 minutes of onboarding time on a theoretical
problem, hoping that your new hires will remember what to do if they ever
encounter it.

Using [`RelativePathBuf`] we can fix our data model like this:

```rust
use relative_path::RelativePathBuf;
use serde::{Serialize, Deserialize};

#[derive(Serialize, Deserialize)]
pub struct Manifest {
    source: RelativePathBuf,
}
```

And where it's used:

```rust
use std::fs;
use std::env::current_dir;

let manifest: Manifest = todo!();

let root = current_dir()?;
let source = manifest.source.to_path(&root);
let content = fs::read(&source)?;
```

<br>

## Overview

Conversion to a platform-specific [`Path`] happens through the [`to_path`]
and [`to_logical_path`] functions. Where you are required to specify the
path that prefixes the relative path. This can come from a function such as
[`std::env::current_dir`].

```rust
use std::env::current_dir;
use std::path::Path;

use relative_path::RelativePath;

let root = current_dir()?;

// to_path unconditionally concatenates a relative path with its base:
let relative_path = RelativePath::new("../foo/./bar");
let full_path = relative_path.to_path(&root);
assert_eq!(full_path, root.join("..\\foo\\.\\bar"));

// to_logical_path tries to apply the logical operations that the relative
// path corresponds to:
let relative_path = RelativePath::new("../foo/./bar");
let full_path = relative_path.to_logical_path(&root);

// Replicate the operation performed by `to_logical_path`.
let mut parent = root.clone();
parent.pop();
assert_eq!(full_path, parent.join("foo\\bar"));
```

When two relative paths are compared to each other, their exact component
makeup determines equality.

```rust
use relative_path::RelativePath;

assert_ne!(
    RelativePath::new("foo/bar/../baz"),
    RelativePath::new("foo/baz")
);
```

Using platform-specific path separators to construct relative paths is not
supported.

Path separators from other platforms are simply treated as part of a
component:

```rust
use relative_path::RelativePath;

assert_ne!(
    RelativePath::new("foo/bar"),
    RelativePath::new("foo\\bar")
);

assert_eq!(1, RelativePath::new("foo\\bar").components().count());
assert_eq!(2, RelativePath::new("foo/bar").components().count());
```

To see if two relative paths are equivalent you can use [`normalize`]:

```rust
use relative_path::RelativePath;

assert_eq!(
    RelativePath::new("foo/bar/../baz").normalize(),
    RelativePath::new("foo/baz").normalize(),
);
```

<br>

## Additional portability notes

While relative paths avoid the most egregious portability issue, that
absolute paths will work equally unwell on all platforms. We cannot avoid
all. This section tries to document additional portability hazards that we
are aware of.

[`RelativePath`], similarly to [`Path`], makes no guarantees that its
constituent components make up legal file names. While components are
strictly separated by slashes, we can still store things in them which may
not be used as legal paths on all platforms.

* A `NUL` character is not permitted on unix platforms - this is a
  terminator in C-based filesystem APIs. Slash (`/`) is also used as a path
  separator.
* Windows has a number of [reserved characters and names][windows-reserved]
  (like `CON`, `PRN`, and `AUX`) which cannot legally be part of a
  filesystem component.
* Windows paths are [case-insensitive by default][windows-case]. So,
  `Foo.txt` and `foo.txt` are the same files on windows. But they are
  considered different paths on most unix systems.

A relative path that *accidentally* contains a platform-specific components
will largely result in a nonsensical paths being generated in the hope that
they will fail fast during development and testing.

```rust
use relative_path::{RelativePath, PathExt};
use std::path::Path;

if cfg!(windows) {
    assert_eq!(
        Path::new("foo\\c:\\bar\\baz"),
        RelativePath::new("c:\\bar\\baz").to_path("foo")
    );
}

if cfg!(unix) {
    assert_eq!(
        Path::new("foo/bar/baz"),
        RelativePath::new("/bar/baz").to_path("foo")
    );
}

assert_eq!(
    Path::new("foo").relative_to("bar")?,
    RelativePath::new("../foo"),
);
```

[`None`]: https://doc.rust-lang.org/std/option/enum.Option.html
[`normalize`]: https://docs.rs/relative-path/1/relative_path/struct.RelativePath.html#method.normalize
[`Path`]: https://doc.rust-lang.org/std/path/struct.Path.html
[`RelativePath`]: https://docs.rs/relative-path/1/relative_path/struct.RelativePath.html
[`RelativePathBuf`]: https://docs.rs/relative-path/1/relative_path/struct.RelativePathBuf.html
[`std::env::current_dir`]: https://doc.rust-lang.org/std/env/fn.current_dir.html
[`std::path`]: https://doc.rust-lang.org/std/path/index.html
[`to_logical_path`]: https://docs.rs/relative-path/1/relative_path/struct.RelativePath.html#method.to_logical_path
[`to_path`]: https://docs.rs/relative-path/1/relative_path/struct.RelativePath.html#method.to_path
[windows-reserved]: https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
[windows-case]: https://learn.microsoft.com/en-us/windows/wsl/case-sensitivity
[`relative-path-utils` crate]: https://docs.rs/relative-path-utils
