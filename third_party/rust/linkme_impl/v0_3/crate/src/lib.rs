#![cfg_attr(all(test, exhaustive), feature(non_exhaustive_omitted_patterns_lint))]
#![allow(
    clippy::cast_possible_truncation, // https://github.com/rust-lang/rust-clippy/issues/7486
    clippy::needless_pass_by_value,
    clippy::too_many_lines,
    clippy::uninlined_format_args,
)]

mod args;
mod attr;
mod declaration;
mod element;
mod hash;
mod linker;

use crate::args::Args;
use crate::hash::hash;
use proc_macro::TokenStream;
use syn::parse_macro_input;

#[proc_macro_attribute]
pub fn distributed_slice(args: TokenStream, input: TokenStream) -> TokenStream {
    let args = parse_macro_input!(args as Args);

    let expanded = match args {
        Args::None => declaration::expand(parse_macro_input!(input)),
        Args::Path(path) => element::expand(path, None, parse_macro_input!(input)),
        Args::PathPos(path, pos) => element::expand(path, pos, parse_macro_input!(input)),
    };

    TokenStream::from(expanded)
}
