# `unicode-width`

[![Build status](https://github.com/unicode-rs/unicode-width/actions/workflows/rust.yml/badge.svg)](https://github.com/unicode-rs/unicode-width/actions/workflows/rust.yml)
[![crates.io version](https://img.shields.io/crates/v/unicode-width)](https://crates.io/crates/unicode-width)
[![Docs status](https://img.shields.io/docsrs/unicode-width)](https://docs.rs/unicode-width/)

Determine displayed width of `char` and `str` types according to [Unicode Standard Annex #11][UAX11]
and other portions of the Unicode standard.

This crate is `#![no_std]`.

[UAX11]: http://www.unicode.org/reports/tr11/

```rust
use unicode_width::UnicodeWidthStr;

fn main() {
    let teststr = "Ｈｅｌｌｏ, ｗｏｒｌｄ!";
    let width = teststr.width();
    println!("{}", teststr);
    println!("The above string is {} columns wide.", width);
    let width = teststr.width_cjk();
    println!("The above string is {} columns wide (CJK).", width);
}
```

**NOTE:** The computed width values may not match the actual rendered column
width. For example, the woman scientist emoji comprises of a woman emoji, a
zero-width joiner and a microscope emoji. Such [emoji ZWJ sequences](https://www.unicode.org/reports/tr51/#Emoji_ZWJ_Sequences)
are considered to have the sum of the widths of their constituent parts:

```rust
extern crate unicode_width;
use unicode_width::UnicodeWidthStr;

fn main() {
    assert_eq!("👩".width(), 2); // Woman
    assert_eq!("🔬".width(), 2); // Microscope
    assert_eq!("👩‍🔬".width(), 4); // Woman scientist
}
```

Additionally, [defective combining character sequences](https://unicode.org/glossary/#defective_combining_character_sequence)
and nonstandard [Korean jamo](https://unicode.org/glossary/#jamo) sequences may
be rendered with a different width than what this crate says. (This is not an
exhaustive list.)

## crates.io

You can use this package in your project by adding the following
to your `Cargo.toml`:

```toml
[dependencies]
unicode-width = "0.1.11"
```
