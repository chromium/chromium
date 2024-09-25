#![allow(clippy::test_attr_in_doctest)]
#![allow(unexpected_cfgs)]
#![cfg_attr(use_proc_macro_diagnostic, feature(proc_macro_diagnostic))]
extern crate proc_macro;

// Test utility module
#[cfg(test)]
pub(crate) mod test;

#[macro_use]
mod error;
mod parse;
mod refident;
mod render;
mod resolver;
mod utils;

use syn::{parse_macro_input, ItemFn};

use crate::parse::{fixture::FixtureInfo, rstest::RsTestInfo};
use parse::ExtendWithFunctionAttrs;
use quote::ToTokens;

#[allow(missing_docs)]
#[proc_macro_attribute]
pub fn fixture(
    args: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let mut info: FixtureInfo = parse_macro_input!(args as FixtureInfo);
    let mut fixture = parse_macro_input!(input as ItemFn);

    let extend_result = info.extend_with_function_attrs(&mut fixture);

    let mut errors = error::fixture(&fixture, &info);

    if let Err(attrs_errors) = extend_result {
        attrs_errors.to_tokens(&mut errors);
    }

    if errors.is_empty() {
        render::fixture(fixture, info)
    } else {
        errors
    }
    .into()
}

#[allow(missing_docs)]
#[proc_macro_attribute]
pub fn rstest(
    args: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let mut test = parse_macro_input!(input as ItemFn);
    let mut info = parse_macro_input!(args as RsTestInfo);

    let extend_result = info.extend_with_function_attrs(&mut test);

    let mut errors = error::rstest(&test, &info);

    if let Err(attrs_errors) = extend_result {
        attrs_errors.to_tokens(&mut errors);
    }

    if errors.is_empty() {
        if info.data.has_list_values() {
            render::matrix(test, info)
        } else if info.data.has_cases() {
            render::parametrize(test, info)
        } else {
            render::single(test, info)
        }
    } else {
        errors
    }
    .into()
}
