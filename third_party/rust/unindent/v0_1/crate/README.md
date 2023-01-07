# Unindent

[<img alt="github" src="https://img.shields.io/badge/github-dtolnay/indoc-8da0cb?style=for-the-badge&labelColor=555555&logo=github" height="20">](https://github.com/dtolnay/indoc)
[<img alt="crates.io" src="https://img.shields.io/crates/v/unindent.svg?style=for-the-badge&color=fc8d62&logo=rust" height="20">](https://crates.io/crates/unindent)
[<img alt="docs.rs" src="https://img.shields.io/badge/docs.rs-unindent-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K" height="20">](https://docs.rs/unindent)
[<img alt="build status" src="https://img.shields.io/github/workflow/status/dtolnay/indoc/CI/master?style=for-the-badge" height="20">](https://github.com/dtolnay/indoc/actions?query=branch%3Amaster)

This crate provides [`indoc`]'s indentation logic for use with strings that are
not statically known at compile time. For unindenting string literals, use
`indoc` instead.

[`indoc`]: https://github.com/dtolnay/indoc

This crate exposes two functions:

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

## Explanation

The following rules characterize the behavior of unindent:

1. Count the leading spaces of each line, ignoring the first line and any lines
   that are empty or contain spaces only.
2. Take the minimum.
3. If the first line is empty i.e. the string begins with a newline, remove the
   first line.
4. Remove the computed number of spaces from the beginning of each line.

This means there are a few equivalent ways to format the same string, so choose
one you like. All of the following result in the string `"line one\nline
two\n"`:

```
unindent("          /      unindent(           /      unindent("line one
   line one        /         "line one        /                 line two
   line two       /           line two       /                  ")
   ")            /            ")            /
```

## License

Licensed under either of

 * Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in Indoc by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
