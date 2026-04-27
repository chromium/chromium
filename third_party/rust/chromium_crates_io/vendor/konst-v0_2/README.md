[![Rust](https://github.com/rodrimati1992/konst/workflows/Rust/badge.svg)](https://github.com/rodrimati1992/konst/actions)
[![crates-io](https://img.shields.io/crates/v/konst.svg)](https://crates.io/crates/konst)
[![api-docs](https://docs.rs/konst/badge.svg)](https://docs.rs/konst/*)

Const equivalents of std functions, compile-time comparison, and parsing.

# Features

This crate provides:

- Const fn equivalents of standard library functions and methods.

- Compile-time parsing through the [`Parser`] type, and [`parse_any`] macro.

- Functions for comparing many standard library types,
with the [`const_eq`]/[`const_eq_for`]/[`const_cmp`]/[`const_cmp_for`] macros
for more conveniently calling them, powered by the [`polymorphism`] module.


# Examples

### Parsing an enum

This example demonstrates how you can parse a simple enum from an environment variable,
at compile-time.

```rust
use konst::eq_str;
use konst::{unwrap_opt_or, unwrap_ctx};

#[derive(Debug, PartialEq)]
enum Direction {
    Forward,
    Backward,
    Left,
    Right,
}

impl Direction {
    const fn try_parse(input: &str) -> Result<Self, ParseDirectionError> {
        // As of Rust 1.51.0, string patterns don't work in const contexts
        match () {
            _ if eq_str(input, "forward") => Ok(Direction::Forward),
            _ if eq_str(input, "backward") => Ok(Direction::Backward),
            _ if eq_str(input, "left") => Ok(Direction::Left),
            _ if eq_str(input, "right") => Ok(Direction::Right),
            _ => Err(ParseDirectionError),
        }
    }
}

const CHOICE: &str = unwrap_opt_or!(option_env!("chosen-direction"), "forward");

const DIRECTION: Direction = unwrap_ctx!(Direction::try_parse(CHOICE));

fn main() {
    match DIRECTION {
        Direction::Forward => assert_eq!(CHOICE, "forward"),
        Direction::Backward => assert_eq!(CHOICE, "backward"),
        Direction::Left => assert_eq!(CHOICE, "left"),
        Direction::Right => assert_eq!(CHOICE, "right"),
    }
}

#[derive(Debug, PartialEq)]
pub struct ParseDirectionError;

use std::fmt::{self, Display};

impl Display for ParseDirectionError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("Failed to parse a Direction")
    }
}

impl ParseDirectionError {
    #[allow(unconditional_panic)]
    const fn panic(&self) -> ! {
        [/*failed to parse a Direction*/][0]
    }
}


```

### Parsing CSV

This example demonstrates how an CSV environment variable can be parsed into integers.

This requires the `"rust_1_64"` and `""parsing_no_proc""` features 
(the latter is enabled by default).

```rust
use konst::{
    primitive::parse_u64,
    result::unwrap_ctx,
    iter, string,
};

const CSV: &str = env!("NUMBERS");

static PARSED: [u64; 5] = iter::collect_const!(u64 => 
    string::split(CSV, ","),
        map(string::trim),
        map(|s| unwrap_ctx!(parse_u64(s))),
);

assert_eq!(PARSED, [3, 8, 13, 21, 34]);

```

### Parsing a struct

This example demonstrates how you can use [`Parser`] to parse a struct at compile-time.

```rust
use konst::{
    parsing::{Parser, ParseValueResult},
    for_range, parse_any, try_rebind, unwrap_ctx,
};

const PARSED: Struct = {
    // You can also parse strings from environment variables, or from an `include_str!(....)`
    let input = "\
        1000,
        circle,
        red, blue, green, blue,
    ";
    
    unwrap_ctx!(parse_struct(Parser::from_str(input))).0
};

fn main(){
    assert_eq!(
        PARSED,
        Struct{
            amount: 1000,
            repeating: Shape::Circle,
            colors: [Color::Red, Color::Blue, Color::Green, Color::Blue],
        }
    );
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Struct {
    pub amount: usize,
    pub repeating: Shape,
    pub colors: [Color; 4],
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Shape {
    Circle,
    Square,
    Line,
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum Color {
    Red,
    Blue,
    Green,
}

pub const fn parse_struct(mut parser: Parser<'_>) -> ParseValueResult<'_, Struct> {
    try_rebind!{(let amount, parser) = parser.trim_start().parse_usize()}
    try_rebind!{parser = parser.strip_prefix(",")}

    try_rebind!{(let repeating, parser) = parse_shape(parser.trim_start())}
    try_rebind!{parser = parser.strip_prefix(",")}

    try_rebind!{(let colors, parser) = parse_colors(parser.trim_start())}

    Ok((Struct{amount, repeating, colors}, parser))
}

pub const fn parse_shape(mut parser: Parser<'_>) -> ParseValueResult<'_, Shape> {
    let shape = parse_any!{parser, strip_prefix;
        "circle" => Shape::Circle,
        "square" => Shape::Square,
        "line" => Shape::Line,
        _ => return Err(parser.into_other_error())
    };
    Ok((shape, parser))
}

pub const fn parse_colors(mut parser: Parser<'_>) -> ParseValueResult<'_, [Color; 4]> {
    let mut colors = [Color::Red; 4];

    for_range!{i in 0..4 =>
        try_rebind!{(colors[i], parser) = parse_color(parser.trim_start())}
        try_rebind!{parser = parser.strip_prefix(",")}
    }

    Ok((colors, parser))
}

pub const fn parse_color(mut parser: Parser<'_>) -> ParseValueResult<'_, Color> {
    let color = parse_any!{parser, strip_prefix;
        "red" => Color::Red,
        "blue" => Color::Blue,
        "green" => Color::Green,
        _ => return Err(parser.into_other_error())
    };
    Ok((color, parser))
}



```

# Cargo features

These are the features of these crates:

- `"cmp"`(enabled by default):
Enables all comparison functions and macros,
the string equality and ordering comparison functions don't require this feature.

- `"parsing"`(enabled by default):
Enables the `"parsing_no_proc"` feature, compiles the `konst_proc_macros` dependency,
and enables the [`parse_any`] macro.
You can use this feature instead of `"parsing_no_proc"` if the slightly longer
compile times aren't a problem.

- `"parsing_no_proc"`(enabled by default):
Enables the [`parsing`] module (for parsing from `&str` and `&[u8]`),
the `primitive::parse_*` functions, `try_rebind`, and `rebind_if_ok` macros.

- `alloc"`:
Enables items that use types from the [`alloc`] crate, including `Vec` and `String`.

### Rust release related

None of thse features are enabled by default.

- `"rust_1_51"`:
Enables items that require const generics,
and impls for arrays to use const generics instead of only supporting small arrays.

- `"rust_1_55"`: Enables the `string::from_utf8` function
(the macro works in all versions),
`str` indexing functions,  and the `"rust_1_51"` feature.

- `"rust_1_56"`:
Enables items that internally use raw pointer dereferences or transmutes,
and the `"rust_1_55"` feature.

- `"rust_1_57"`: Allows `konst` to use the `panic` macro, 
and enables the `"rust_1_56"` feature.

- `"rust_1_61"`:
Enables const fns that use trait bounds, and the `"rust_1_57"` feature.

- `"rust_1_64"`:<br>
Adds slice and string iterators,
string splitting functions(`[r]split_once`),
const equivalents of iterator methods(in `konst::iter`),
and makes slicing functions more efficient.
<br>Note that only functions which mention this feature in their documentation are affected.
<br>Enables the `"rust_1_61"` feature.

- `"rust_latest_stable"`: enables the latest `"rust_1_*"` feature.
Only recommendable if you can update the Rust compiler every stable release.

- `"mut_refs"`(disabled by default):
Enables const functions that take mutable references.
Use this whenever mutable references in const contexts are stabilized.
Also enables the `"rust_latest_stable"` feature.

- `"nightly_mut_refs"`(disabled by default):
Enables the `"mut_refs"` feature. Requires Rust nightly.

# No-std support

`konst` is `#![no_std]`, it can be used anywhere Rust can be used.

# Minimum Supported Rust Version

`konst` requires Rust 1.46.0, because it uses looping an branching in const contexts.

Features that require newer versions of Rust, or the nightly compiler,
need to be explicitly enabled with cargo features.


[`alloc`]: https://doc.rust-lang.org/alloc/
[`const_eq`]: https://docs.rs/konst/*/konst/macro.const_eq.html
[`const_eq_for`]: https://docs.rs/konst/*/konst/macro.const_eq_for.html
[`const_cmp`]: https://docs.rs/konst/*/konst/macro.const_cmp.html
[`const_cmp_for`]: https://docs.rs/konst/*/konst/macro.const_cmp_for.html
[`polymorphism`]: https://docs.rs/konst/*/konst/polymorphism/index.html
[`parsing`]: https://docs.rs/konst/*/konst/parsing/index.html
[`primitive`]: https://docs.rs/konst/*/konst/primitive/index.html
[`parse_any`]: https://docs.rs/konst/*/konst/macro.parse_any.html
[`Parser`]: https://docs.rs/konst/*/konst/parsing/struct.Parser.html
[`Parser::parse_u128`]: https://docs.rs/konst/*/konst/parsing/struct.Parser.html#method.parse_u128
