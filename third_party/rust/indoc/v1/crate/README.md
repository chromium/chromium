Indented Documents (indoc)
==========================

[<img alt="github" src="https://img.shields.io/badge/github-dtolnay/indoc-8da0cb?style=for-the-badge&labelColor=555555&logo=github" height="20">](https://github.com/dtolnay/indoc)
[<img alt="crates.io" src="https://img.shields.io/crates/v/indoc.svg?style=for-the-badge&color=fc8d62&logo=rust" height="20">](https://crates.io/crates/indoc)
[<img alt="docs.rs" src="https://img.shields.io/badge/docs.rs-indoc-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K" height="20">](https://docs.rs/indoc)
[<img alt="build status" src="https://img.shields.io/github/workflow/status/dtolnay/indoc/CI/master?style=for-the-badge" height="20">](https://github.com/dtolnay/indoc/actions?query=branch%3Amaster)

This crate provides a procedural macro for indented string literals. The
`indoc!()` macro takes a multiline string literal and un-indents it at compile
time so the leftmost non-space character is in the first column.

```toml
[dependencies]
indoc = "1.0"
```

*Compiler requirement: rustc 1.45 or greater.*

<br>

## Using indoc

```rust
use indoc::indoc;

fn main() {
    let testing = indoc! {"
        def hello():
            print('Hello, world!')

        hello()
    "};
    let expected = "def hello():\n    print('Hello, world!')\n\nhello()\n";
    assert_eq!(testing, expected);
}
```

Indoc also works with raw string literals:

```rust
use indoc::indoc;

fn main() {
    let testing = indoc! {r#"
        def hello():
            print("Hello, world!")

        hello()
    "#};
    let expected = "def hello():\n    print(\"Hello, world!\")\n\nhello()\n";
    assert_eq!(testing, expected);
}
```

And byte string literals:

```rust
use indoc::indoc;

fn main() {
    let testing = indoc! {b"
        def hello():
            print('Hello, world!')

        hello()
    "};
    let expected = b"def hello():\n    print('Hello, world!')\n\nhello()\n";
    assert_eq!(testing[..], expected[..]);
}
```

<br>

## Formatting macros

The indoc crate exports four additional macros to substitute conveniently for
the standard library's formatting macros:

- `formatdoc!($fmt, ...)`&ensp;&mdash;&ensp;equivalent to `format!(indoc!($fmt), ...)`
- `printdoc!($fmt, ...)`&ensp;&mdash;&ensp;equivalent to `print!(indoc!($fmt), ...)`
- `eprintdoc!($fmt, ...)`&ensp;&mdash;&ensp;equivalent to `eprint!(indoc!($fmt), ...)`
- `writedoc!($dest, $fmt, ...)`&ensp;&mdash;&ensp;equivalent to `write!($dest, indoc!($fmt), ...)`

```rust
use indoc::printdoc;

fn main() {
    printdoc! {"
        GET {url}
        Accept: {mime}
        ",
        url = "http://localhost:8080",
        mime = "application/json",
    }
}
```

<br>

## Explanation

The following rules characterize the behavior of the `indoc!()` macro:

1. Count the leading spaces of each line, ignoring the first line and any lines
   that are empty or contain spaces only.
2. Take the minimum.
3. If the first line is empty i.e. the string begins with a newline, remove the
   first line.
4. Remove the computed number of spaces from the beginning of each line.

<br>

## Unindent

Indoc's indentation logic is available in the `unindent` crate. This may be
useful for processing strings that are not statically known at compile time.

The crate exposes two functions:

- `unindent(&str) -> String`
- `unindent_bytes(&[u8]) -> Vec<u8>`

```rust
use unindent::unindent;

fn main() {
    let indented = "
            line one
            line two";
    assert_eq!("line one\nline two", unindent(indented));
}
```

<br>

#### License

<sup>
Licensed under either of <a href="LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="LICENSE-MIT">MIT license</a> at your option.
</sup>

<br>

<sub>
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this crate by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
</sub>
