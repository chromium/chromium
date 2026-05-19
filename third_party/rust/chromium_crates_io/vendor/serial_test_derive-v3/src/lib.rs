//! # serial_test_derive
//! Helper crate for [serial_test](../serial_test/index.html)

#![cfg_attr(docsrs, feature(doc_cfg))]
#![deny(missing_docs)]

extern crate proc_macro;

use proc_macro::TokenStream;
use proc_macro2::{Literal, TokenTree};
use quote::{format_ident, quote, ToTokens, TokenStreamExt};
use std::{iter::FromIterator, ops::Deref};
use syn::Result as SynResult;

/// Allows for the creation of serialised Rust tests
/// ````no_run
/// #[test]
/// #[serial]
/// fn test_serial_one() {
///   // Do things
/// }
///
/// #[test]
/// #[serial]
/// fn test_serial_another() {
///   // Do things
/// }
/// ````
/// Multiple tests with the [serial](macro@serial) attribute are guaranteed to be executed in serial. Ordering
/// of the tests is not guaranteed however. If you have other tests that can be run in parallel, but would clash
/// if run at the same time as the [serial](macro@serial) tests, you can use the [parallel](macro@parallel) attribute.
///
/// ## Serial Keys
///
/// If you want different subsets of tests to be serialised with each
/// other, but not depend on other subsets, you can add a key argument to [serial](macro@serial), and all calls
/// with identical arguments will be called in serial. Multiple comma-separated keys will make a test run in serial with all of the sets with any of those keys.
///
/// ````no_run
/// #[test]
/// #[serial(something)]
/// fn test_serial_one() {
///   // Do things
/// }
///
/// #[test]
/// #[serial(something)]
/// fn test_serial_another() {
///   // Do things
/// }
///
/// #[test]
/// #[serial(other)]
/// fn test_serial_third() {
///   // Do things
/// }
///
/// #[test]
/// #[serial(other)]
/// fn test_serial_fourth() {
///   // Do things
/// }
///
/// #[test]
/// #[serial(something, other)]
/// fn test_serial_fifth() {
///   // Do things, eventually
/// }
/// ````
/// `test_serial_one` and `test_serial_another` will be executed in serial, as will `test_serial_third` and `test_serial_fourth`
/// but neither sequence will be blocked by the other. `test_serial_fifth` is blocked by tests in either sequence.
///
/// Nested serialised tests (i.e. a [serial](macro@serial) tagged test calling another) are supported.
///
/// ## Inner Attributes
///
/// You can apply attributes to an inner test function using `inner_attrs`. This is useful for
/// applying attributes like `ntest::timeout` that should only affect the test body, not the
/// mutex acquisition:
///
/// ````no_run
/// #[test]
/// #[serial(inner_attrs = [ntest::timeout(1000)])]
/// fn test_with_timeout() {
///   // The timeout only applies to this body, not the serial lock acquisition
/// }
/// ````
///
/// Multiple inner attributes can be specified:
/// ````no_run
/// #[test]
/// #[serial(inner_attrs = [ntest::timeout(1000), other_attr])]
/// fn test_with_multiple_attrs() {
///   // Both attributes apply to the inner function
/// }
/// ````
///
/// Inner attributes can be combined with keys:
/// ````no_run
/// #[test]
/// #[serial(my_key, inner_attrs = [ntest::timeout(1000)])]
/// fn test_with_key_and_timeout() {
///   // Serialized with 'my_key' group, with timeout on the body
/// }
/// ````
#[proc_macro_attribute]
pub fn serial(attr: TokenStream, input: TokenStream) -> TokenStream {
    local_serial_core(attr.into(), input.into()).into()
}

/// Allows for the creation of parallel Rust tests that won't clash with serial tests
/// ````no_run
/// #[test]
/// #[serial]
/// fn test_serial_one() {
///   // Do things
/// }
///
/// #[test]
/// #[parallel]
/// fn test_parallel_one() {
///   // Do things
/// }
///
/// #[test]
/// #[parallel]
/// fn test_parallel_two() {
///   // Do things
/// }
/// ````
/// Multiple tests with the [parallel](macro@parallel) attribute may run in parallel, but not at the
/// same time as [serial](macro@serial) tests. e.g. in the example code above, `test_parallel_one`
/// and `test_parallel_two` may run at the same time, but `test_serial_one` is guaranteed not to run
/// at the same time as either of them. [parallel](macro@parallel) also takes key arguments for groups
/// of tests as per [serial](macro@serial).
///
/// Note that this has zero effect on [file_serial](macro@file_serial) tests, as that uses a different
/// serialisation mechanism. For that, you want [file_parallel](macro@file_parallel).
///
/// Inner attributes are also supported via `inner_attrs`, see [serial](macro@serial) for details.
#[proc_macro_attribute]
pub fn parallel(attr: TokenStream, input: TokenStream) -> TokenStream {
    local_parallel_core(attr.into(), input.into()).into()
}

/// Allows for the creation of file-serialised Rust tests
/// ````no_run
/// #[test]
/// #[file_serial]
/// fn test_serial_one() {
///   // Do things
/// }
///
/// #[test]
/// #[file_serial]
/// fn test_serial_another() {
///   // Do things
/// }
/// ````
///
/// Multiple tests with the [file_serial](macro@file_serial) attribute are guaranteed to run in serial, as per the [serial](macro@serial)
/// attribute. Note that there are no guarantees about one test with [serial](macro@serial) and another with [file_serial](macro@file_serial)
/// as they lock using different methods, and [file_serial](macro@file_serial) does not support nested serialised tests, but otherwise acts
/// like [serial](macro@serial). If you have other tests that can be run in parallel, but would clash
/// if run at the same time as the [file_serial](macro@file_serial) tests, you can use the [file_parallel](macro@file_parallel) attribute.
///
/// It also supports an optional `path` arg as well as key(s) as per [serial](macro@serial), which is the path to the file used for
/// locking purposes. This file is managed by `serial_test` and no assumptions about it's format should be made. The `path` defaults to
/// a file under a reasonable temp directory for the OS if not specified. If the `path` is specified, you can only use one key, as we
/// can't generate per-key paths if you've done that.
/// ````no_run
/// #[test]
/// #[file_serial(key)]
/// fn test_serial_one() {
///   // Do things
/// }
///
/// #[test]
/// #[file_serial(key, path => "/tmp/foo")]
/// fn test_serial_another() {
///   // Do things
/// }
/// ````
///
/// Inner attributes are also supported via `inner_attrs`, see [serial](macro@serial) for details.
#[proc_macro_attribute]
#[cfg_attr(docsrs, doc(cfg(feature = "file_locks")))]
pub fn file_serial(attr: TokenStream, input: TokenStream) -> TokenStream {
    fs_serial_core(attr.into(), input.into()).into()
}

/// Allows for the creation of file-serialised parallel Rust tests that won't clash with file-serialised serial tests
/// ````no_run
/// #[test]
/// #[file_serial]
/// fn test_serial_one() {
///   // Do things
/// }
///
/// #[test]
/// #[file_parallel]
/// fn test_parallel_one() {
///   // Do things
/// }
///
/// #[test]
/// #[file_parallel]
/// fn test_parallel_two() {
///   // Do things
/// }
/// ````
/// Effectively, this should behave like [parallel](macro@parallel) but for [file_serial](macro@file_serial).
/// Note that as per [file_serial](macro@file_serial) this doesn't do anything for [serial](macro@serial)/[parallel](macro@parallel) tests.
///
/// It also supports an optional `path` arg as well as key(s) as per [serial](macro@serial), which is the path to the file used for
/// locking purposes. This file is managed by `serial_test` and no assumptions about it's format should be made. The `path` defaults to
/// a file under a reasonable temp directory for the OS if not specified. If the `path` is specified, you can only use one key, as we
/// can't generate per-key paths if you've done that.
/// ````no_run
/// #[test]
/// #[file_parallel(key, path => "/tmp/foo")]
/// fn test_parallel_one() {
///   // Do things
/// }
///
/// #[test]
/// #[file_parallel(key, path => "/tmp/foo")]
/// fn test_parallel_another() {
///   // Do things
/// }
/// ````
///
/// Inner attributes are also supported via `inner_attrs`, see [serial](macro@serial) for details.
#[proc_macro_attribute]
#[cfg_attr(docsrs, doc(cfg(feature = "file_locks")))]
pub fn file_parallel(attr: TokenStream, input: TokenStream) -> TokenStream {
    fs_parallel_core(attr.into(), input.into()).into()
}

// Based off of https://github.com/dtolnay/quote/issues/20#issuecomment-437341743
#[derive(Default, Debug, Clone)]
struct QuoteOption<T>(Option<T>);

impl<T: ToTokens> ToTokens for QuoteOption<T> {
    fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
        tokens.append_all(match self.0 {
            Some(ref t) => quote! { ::std::option::Option::Some(#t) },
            None => quote! { ::std::option::Option::None },
        });
    }
}

#[derive(Default, Debug)]
struct Config {
    names: Vec<String>,
    path: QuoteOption<String>,
    crate_ident: Vec<TokenTree>,
    inner_attrs: Vec<proc_macro2::TokenStream>,
}

fn string_from_literal(literal: Literal) -> String {
    let string_literal = literal.to_string();
    if !string_literal.starts_with('\"') || !string_literal.ends_with('\"') {
        panic!("Expected a string literal, got '{}'", string_literal);
    }
    // Hacky way of getting a string without the enclosing quotes
    string_literal[1..string_literal.len() - 1].to_string()
}

/// Parse the contents of a bracket group `[attr1(args), attr2]` into a vector of token streams
/// where each element represents a single attribute.
fn parse_inner_attrs_from_group(group: proc_macro2::Group) -> Vec<proc_macro2::TokenStream> {
    let mut inner_attrs = Vec::new();
    let mut current_attr: Vec<TokenTree> = Vec::new();

    for token in group.stream() {
        let is_comma = matches!(&token, TokenTree::Punct(p) if p.as_char() == ',');

        if is_comma {
            // End of current attribute
            if !current_attr.is_empty() {
                inner_attrs.push(proc_macro2::TokenStream::from_iter(current_attr.drain(..)));
            }
        } else {
            current_attr.push(token);
        }
    }

    // Don't forget the last attribute
    if !current_attr.is_empty() {
        inner_attrs.push(proc_macro2::TokenStream::from_iter(current_attr));
    }

    inner_attrs
}

fn get_config(attr: proc_macro2::TokenStream) -> Config {
    let mut attrs = attr.into_iter().collect::<Vec<TokenTree>>();
    let mut raw_args: Vec<String> = Vec::new();
    let mut in_path: bool = false;
    let mut path: Option<String> = None;
    let mut in_crate: bool = false;
    let mut crate_ident: Option<Vec<TokenTree>> = None;
    let mut in_inner_attrs: bool = false;
    let mut inner_attrs: Vec<proc_macro2::TokenStream> = Vec::new();
    while !attrs.is_empty() {
        match attrs.remove(0) {
            TokenTree::Ident(id) if id.to_string().eq_ignore_ascii_case("path") => {
                in_path = true;
            }
            TokenTree::Ident(id) if id.to_string().eq_ignore_ascii_case("crate") => {
                in_crate = true;
            }
            TokenTree::Ident(id) if id.to_string().eq_ignore_ascii_case("inner_attrs") => {
                in_inner_attrs = true;
            }
            TokenTree::Ident(id) => {
                let name = id.to_string();
                raw_args.push(name);
            }
            x => {
                panic!(
                    "Expected literal as key args (or a 'path => '\"foo\"'), not {}",
                    x
                );
            }
        }
        if in_path {
            if attrs.len() < 3 {
                panic!("Expected a '=> <path>' after 'path'");
            }
            match attrs.remove(0) {
                TokenTree::Punct(p) if p.as_char() == '=' => {}
                x => {
                    panic!("Expected = after path, not {}", x);
                }
            }
            match attrs.remove(0) {
                TokenTree::Punct(p) if p.as_char() == '>' => {}
                x => {
                    panic!("Expected > after path, not {}", x);
                }
            }
            match attrs.remove(0) {
                TokenTree::Literal(literal) => {
                    path = Some(string_from_literal(literal));
                }
                x => {
                    panic!("Expected literals as path arg, not {}", x);
                }
            }
            in_path = false;
        }
        if in_crate {
            if attrs.len() < 2 {
                panic!("Expected a '= <import-path>' after 'crate'");
            }
            match attrs.remove(0) {
                TokenTree::Punct(p) if p.as_char() == '=' => {}
                x => {
                    panic!("Expected = after crate, not {}", x);
                }
            }
            let ident_items: Vec<_> = attrs
                .iter()
                .map_while(|t| {
                    match t {
                        TokenTree::Ident(_) => {}
                        TokenTree::Punct(p) if p.as_char() != ',' => {}
                        _ => {
                            return None;
                        }
                    };
                    Some(t.clone())
                })
                .collect();
            for _ in 0..ident_items.len() {
                attrs.remove(0);
            }
            crate_ident = Some(ident_items);
            in_crate = false;
        }
        if in_inner_attrs {
            if attrs.len() < 2 {
                panic!("Expected a '= [attr1, attr2, ...]' after 'inner_attrs'");
            }
            match attrs.remove(0) {
                TokenTree::Punct(p) if p.as_char() == '=' => {}
                x => {
                    panic!("Expected '=' after 'inner_attrs' not {}", x);
                }
            }
            match attrs.remove(0) {
                TokenTree::Group(group) if group.delimiter() == proc_macro2::Delimiter::Bracket => {
                    inner_attrs = parse_inner_attrs_from_group(group);
                }
                x => {
                    panic!("Expected [...] after 'inner_attrs =' not {}", x);
                }
            }
            in_inner_attrs = false;
        }
        if !attrs.is_empty() {
            match attrs.remove(0) {
                TokenTree::Punct(p) if p.as_char() == ',' => {}
                x => {
                    panic!("Expected ',' between args not {}", x);
                }
            }
        }
    }
    if raw_args.is_empty() {
        raw_args.push(String::new());
    }
    raw_args.sort(); // So the keys are always requested in the same order. Avoids dining philosopher issues.
    Config {
        names: raw_args,
        path: QuoteOption(path),
        crate_ident: crate_ident.unwrap_or(vec![TokenTree::Ident(format_ident!("serial_test"))]),
        inner_attrs,
    }
}

fn local_serial_core(
    attr: proc_macro2::TokenStream,
    input: proc_macro2::TokenStream,
) -> proc_macro2::TokenStream {
    let config = get_config(attr);
    serial_setup(input, config, "local")
}

fn local_parallel_core(
    attr: proc_macro2::TokenStream,
    input: proc_macro2::TokenStream,
) -> proc_macro2::TokenStream {
    let config = get_config(attr);
    parallel_setup(input, config, "local")
}

fn fs_serial_core(
    attr: proc_macro2::TokenStream,
    input: proc_macro2::TokenStream,
) -> proc_macro2::TokenStream {
    let config = get_config(attr);
    serial_setup(input, config, "fs")
}

fn fs_parallel_core(
    attr: proc_macro2::TokenStream,
    input: proc_macro2::TokenStream,
) -> proc_macro2::TokenStream {
    let config = get_config(attr);
    parallel_setup(input, config, "fs")
}

#[allow(clippy::cmp_owned)]
fn core_setup(
    input: proc_macro2::TokenStream,
    config: &Config,
    prefix: &str,
    kind: &str,
) -> proc_macro2::TokenStream {
    let fn_ast: SynResult<syn::ItemFn> = syn::parse2(input.clone());
    if let Ok(ast) = fn_ast {
        return fn_setup(ast, config, prefix, kind);
    };
    let mod_ast: SynResult<syn::ItemMod> = syn::parse2(input);
    match mod_ast {
        Ok(mut ast) => {
            let new_content = ast.content.clone().map(|(brace, items)| {
                let new_items = items
                    .into_iter()
                    .map(|item| match item {
                        syn::Item::Fn(item_fn)
                            if item_fn.attrs.iter().any(|attr| {
                                attr.meta
                                    .path()
                                    .segments
                                    .iter()
                                    .map(|s| s.ident.to_string())
                                    .collect::<Vec<String>>()
                                    .join("::")
                                    .contains("test")
                            }) =>
                        {
                            let tokens = fn_setup(item_fn, config, prefix, kind);
                            let token_display = format!("tokens: {tokens}");
                            syn::parse2(tokens).expect(&token_display)
                        }
                        other => other,
                    })
                    .collect();
                (brace, new_items)
            });
            if let Some(nc) = new_content {
                ast.content.replace(nc);
            }
            ast.attrs.retain(|attr| {
                attr.meta.path().segments.first().unwrap().ident.to_string() != "serial"
            });
            ast.into_token_stream()
        }
        Err(_) => {
            panic!("Attribute applied to something other than mod or fn!");
        }
    }
}

fn fn_setup(
    ast: syn::ItemFn,
    config: &Config,
    prefix: &str,
    kind: &str,
) -> proc_macro2::TokenStream {
    let asyncness = ast.sig.asyncness;
    if asyncness.is_some() && cfg!(not(feature = "async")) {
        panic!("async testing attempted with async feature disabled in serial_test!");
    }
    let vis = ast.vis;
    let name = ast.sig.ident;
    #[cfg(all(feature = "test_logging", not(test)))]
    let print_name = {
        let print_str = format!("Starting {name}");
        quote! {
            println!(#print_str);
        }
    };
    #[cfg(any(not(feature = "test_logging"), test))]
    let print_name = quote! {};
    let return_type = match ast.sig.output {
        syn::ReturnType::Default => None,
        syn::ReturnType::Type(_rarrow, ref box_type) => Some(box_type.deref()),
    };
    let block = ast.block;
    let attrs: Vec<syn::Attribute> = ast.attrs.into_iter().collect();
    let names = config.names.clone();
    let path = config.path.clone();
    let crate_ident = config.crate_ident.clone();
    let inner_attrs = &config.inner_attrs;
    let has_inner_attrs = !inner_attrs.is_empty();
    if let Some(ret) = return_type {
        match asyncness {
            Some(_) => {
                let fnname = format_ident!("{}_async_{}_core_with_return", prefix, kind);
                let temp_fn = format_ident!("_{}_internal", name);
                quote! {
                    #(#attrs)
                    *
                    #vis async fn #name () -> #ret {
                        #(#[#inner_attrs])*
                        async fn #temp_fn () -> #ret
                        #block

                        #print_name
                        #(#crate_ident)*::#fnname(vec![#(#names ),*], #path, #temp_fn()).await
                    }
                }
            }
            None => {
                let fnname = format_ident!("{}_{}_core_with_return", prefix, kind);
                if has_inner_attrs {
                    let temp_fn = format_ident!("_{}_inner", name);
                    quote! {
                        #(#attrs)
                        *
                        #vis fn #name () -> #ret {
                            #(#[#inner_attrs])*
                            fn #temp_fn () -> #ret
                            #block

                            #print_name
                            #(#crate_ident)*::#fnname(vec![#(#names ),*], #path, || #temp_fn() )
                        }
                    }
                } else {
                    quote! {
                        #(#attrs)
                        *
                        #vis fn #name () -> #ret {
                            #print_name
                            #(#crate_ident)*::#fnname(vec![#(#names ),*], #path, || #block )
                        }
                    }
                }
            }
        }
    } else {
        match asyncness {
            Some(_) => {
                let fnname = format_ident!("{}_async_{}_core", prefix, kind);
                let temp_fn = format_ident!("_{}_internal", name);
                quote! {
                    #(#attrs)
                    *
                    #vis async fn #name () {
                        #(#[#inner_attrs])*
                        async fn #temp_fn ()
                        #block

                        #print_name
                        #(#crate_ident)*::#fnname(vec![#(#names ),*], #path, #temp_fn()).await;
                    }
                }
            }
            None => {
                let fnname = format_ident!("{}_{}_core", prefix, kind);
                if has_inner_attrs {
                    let temp_fn = format_ident!("_{}_inner", name);
                    quote! {
                        #(#attrs)
                        *
                        #vis fn #name () {
                            #(#[#inner_attrs])*
                            fn #temp_fn ()
                            #block

                            #print_name
                            #(#crate_ident)*::#fnname(vec![#(#names ),*], #path, || #temp_fn() );
                        }
                    }
                } else {
                    quote! {
                        #(#attrs)
                        *
                        #vis fn #name () {
                            #print_name
                            #(#crate_ident)*::#fnname(vec![#(#names ),*], #path, || #block );
                        }
                    }
                }
            }
        }
    }
}

fn serial_setup(
    input: proc_macro2::TokenStream,
    config: Config,
    prefix: &str,
) -> proc_macro2::TokenStream {
    core_setup(input, &config, prefix, "serial")
}

fn parallel_setup(
    input: proc_macro2::TokenStream,
    config: Config,
    prefix: &str,
) -> proc_macro2::TokenStream {
    core_setup(input, &config, prefix, "parallel")
}

#[cfg(test)]
mod tests {
    use super::{fs_serial_core, local_parallel_core, local_serial_core};
    use proc_macro2::{TokenStream, TokenTree};
    use quote::quote;
    use std::iter::FromIterator;

    fn init() {
        let _ = env_logger::builder().is_test(false).try_init();
    }

    fn unparse(input: TokenStream) -> String {
        let item = syn::parse2(input).unwrap();
        let file = syn::File {
            attrs: vec![],
            items: vec![item],
            shebang: None,
        };

        prettyplease::unparse(&file)
    }

    fn compare_streams(first: TokenStream, second: TokenStream) {
        let f = unparse(first);
        assert_eq!(f, unparse(second));
    }

    #[test]
    fn test_serial() {
        init();
        let attrs = proc_macro2::TokenStream::new();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        let stream = local_serial_core(attrs.into(), input);
        let compare = quote! {
            #[test]
            fn foo () {
                serial_test::local_serial_core(vec![""], ::std::option::Option::None, || {} );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_serial_with_pub() {
        init();
        let attrs = proc_macro2::TokenStream::new();
        let input = quote! {
            #[test]
            pub fn foo() {}
        };
        let stream = local_serial_core(attrs.into(), input);
        let compare = quote! {
            #[test]
            pub fn foo () {
                serial_test::local_serial_core(vec![""], ::std::option::Option::None, || {} );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_other_attributes() {
        init();
        let attrs = proc_macro2::TokenStream::new();
        let input = quote! {
            #[test]
            #[ignore]
            #[should_panic(expected = "Testing panic")]
            #[something_else]
            fn foo() {}
        };
        let stream = local_serial_core(attrs.into(), input);
        let compare = quote! {
            #[test]
            #[ignore]
            #[should_panic(expected = "Testing panic")]
            #[something_else]
            fn foo () {
                serial_test::local_serial_core(vec![""], ::std::option::Option::None, || {} );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    #[cfg(feature = "async")]
    fn test_serial_async() {
        init();
        let attrs = proc_macro2::TokenStream::new();
        let input = quote! {
            async fn foo() {}
        };
        let stream = local_serial_core(attrs.into(), input);
        let compare = quote! {
            async fn foo () {
                async fn _foo_internal () { }
                serial_test::local_async_serial_core(vec![""], ::std::option::Option::None, _foo_internal() ).await;
            }
        };
        assert_eq!(format!("{}", compare), format!("{}", stream));
    }

    #[test]
    #[cfg(feature = "async")]
    fn test_serial_async_return() {
        init();
        let attrs = proc_macro2::TokenStream::new();
        let input = quote! {
            async fn foo() -> Result<(), ()> { Ok(()) }
        };
        let stream = local_serial_core(attrs.into(), input);
        let compare = quote! {
            async fn foo () -> Result<(), ()> {
                async fn _foo_internal ()  -> Result<(), ()> { Ok(()) }
                serial_test::local_async_serial_core_with_return(vec![""], ::std::option::Option::None, _foo_internal() ).await
            }
        };
        assert_eq!(format!("{}", compare), format!("{}", stream));
    }

    #[test]
    fn test_file_serial() {
        init();
        let attrs: Vec<_> = quote! { foo }.into_iter().collect();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        let stream = fs_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn foo () {
                serial_test::fs_serial_core(vec!["foo"], ::std::option::Option::None, || {} );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_file_serial_no_args() {
        init();
        let attrs = proc_macro2::TokenStream::new();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        let stream = fs_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn foo () {
                serial_test::fs_serial_core(vec![""], ::std::option::Option::None, || {} );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_file_serial_with_path() {
        init();
        let attrs: Vec<_> = quote! { foo, path => "bar_path" }.into_iter().collect();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        let stream = fs_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn foo () {
                serial_test::fs_serial_core(vec!["foo"], ::std::option::Option::Some("bar_path"), || {} );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_single_attr() {
        init();
        let attrs: Vec<_> = quote! { one}.into_iter().collect();
        let input = quote! {
            #[test]
            fn single() {}
        };
        let stream = local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn single () {
                serial_test::local_serial_core(vec!["one"], ::std::option::Option::None, || {} );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_multiple_attr() {
        init();
        let attrs: Vec<_> = quote! { two, one }.into_iter().collect();
        let input = quote! {
            #[test]
            fn multiple() {}
        };
        let stream = local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn multiple () {
                serial_test::local_serial_core(vec!["one", "two"], ::std::option::Option::None, || {} );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_mod() {
        init();
        let attrs = proc_macro2::TokenStream::new();
        let input = quote! {
            #[cfg(test)]
            #[serial]
            mod serial_attr_tests {
                pub fn foo() {
                    println!("Nothing");
                }

                #[test]
                fn bar() {}
            }
        };
        let stream = local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[cfg(test)]
            mod serial_attr_tests {
                pub fn foo() {
                    println!("Nothing");
                }

                #[test]
                fn bar() {
                    serial_test::local_serial_core(vec![""], ::std::option::Option::None, || {} );
                }
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_later_test_mod() {
        init();
        let attrs = proc_macro2::TokenStream::new();
        let input = quote! {
            #[cfg(test)]
            #[serial]
            mod serial_attr_tests {
                pub fn foo() {
                    println!("Nothing");
                }

                #[demo_library::test]
                fn bar() {}
            }
        };
        let stream = local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[cfg(test)]
            mod serial_attr_tests {
                pub fn foo() {
                    println!("Nothing");
                }

                #[demo_library::test]
                fn bar() {
                    serial_test::local_serial_core(vec![""], ::std::option::Option::None, || {} );
                }
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    #[cfg(feature = "async")]
    fn test_mod_with_async() {
        init();
        let attrs = proc_macro2::TokenStream::new();
        let input = quote! {
            #[cfg(test)]
            #[serial]
            mod serial_attr_tests {
                #[demo_library::test]
                async fn foo() -> Result<(), ()> {
                    Ok(())
                }

                #[demo_library::test]
                #[ignore = "bla"]
                async fn bar() -> Result<(), ()> {
                    Ok(())
                }
            }
        };
        let stream = local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[cfg(test)]
            mod serial_attr_tests {
                #[demo_library::test]
                async fn foo() -> Result<(), ()> {
                    async fn _foo_internal() -> Result<(), ()> { Ok(())}
                    serial_test::local_async_serial_core_with_return(vec![""], ::std::option::Option::None, _foo_internal() ).await
                }

                #[demo_library::test]
                #[ignore = "bla"]
                async fn bar() -> Result<(), ()> {
                    async fn _bar_internal() -> Result<(), ()> { Ok(())}
                    serial_test::local_async_serial_core_with_return(vec![""], ::std::option::Option::None, _bar_internal() ).await
                }
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_nested_return() {
        init();
        let attrs = proc_macro2::TokenStream::new();
        let input = quote! {
            #[test]
            fn test() -> Result<Result<(), ()>, ()> {
                Ok(Ok(()))
            }
        };
        let stream = local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn test() -> Result<Result<(), ()>, ()> {
                serial_test::local_serial_core_with_return(vec![""], ::std::option::Option::None, || {Ok(Ok(()))} )
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_crate_wrapper() {
        init();
        let attrs: Vec<_> = quote! { crate = wrapper::__derive_refs::serial }
            .into_iter()
            .collect();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        let stream = fs_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn foo () {
                wrapper::__derive_refs::serial::fs_serial_core(vec![""], ::std::option::Option::None, || {} );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_crate_wrapper_with_path() {
        init();
        let attrs: Vec<_> = quote! {crate = wrapper::__derive_refs::serial, path => "/tmp/bar" }
            .into_iter()
            .collect();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        let stream = fs_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn foo () {
                wrapper::__derive_refs::serial::fs_serial_core(vec![""], ::std::option::Option::Some("/tmp/bar"), || {} );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_crate_wrapper_with_path_and_key() {
        init();
        let attrs: Vec<_> =
            quote! { key1, key2, path => "/tmp/bar", crate = wrapper::__derive_refs::serial }
                .into_iter()
                .collect();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        let stream = fs_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn foo () {
                wrapper::__derive_refs::serial::fs_serial_core(vec!["key1", "key2"], ::std::option::Option::Some("/tmp/bar"), || {} );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_inner_attrs_single() {
        init();
        let attrs: Vec<_> = quote! { inner_attrs = [timeout(100)] }
            .into_iter()
            .collect();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        let stream = local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn foo () {
                #[timeout(100)]
                fn _foo_inner() {}
                serial_test::local_serial_core(vec![""], ::std::option::Option::None, || _foo_inner() );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_inner_attrs_multiple() {
        init();
        let attrs: Vec<_> = quote! { inner_attrs = [timeout(100), other_attr] }
            .into_iter()
            .collect();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        let stream = local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn foo () {
                #[timeout(100)]
                #[other_attr]
                fn _foo_inner() {}
                serial_test::local_serial_core(vec![""], ::std::option::Option::None, || _foo_inner() );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_inner_attrs_with_key() {
        init();
        let attrs: Vec<_> = quote! { my_key, inner_attrs = [timeout(100)] }
            .into_iter()
            .collect();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        let stream = local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn foo () {
                #[timeout(100)]
                fn _foo_inner() {}
                serial_test::local_serial_core(vec!["my_key"], ::std::option::Option::None, || _foo_inner() );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_inner_attrs_with_return() {
        init();
        let attrs: Vec<_> = quote! { inner_attrs = [timeout(100)] }
            .into_iter()
            .collect();
        let input = quote! {
            #[test]
            fn foo() -> Result<(), ()> { Ok(()) }
        };
        let stream = local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn foo () -> Result<(), ()> {
                #[timeout(100)]
                fn _foo_inner() -> Result<(), ()> { Ok(()) }
                serial_test::local_serial_core_with_return(vec![""], ::std::option::Option::None, || _foo_inner() )
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    #[cfg(feature = "async")]
    fn test_inner_attrs_async() {
        init();
        let attrs: Vec<_> = quote! { inner_attrs = [timeout(100)] }
            .into_iter()
            .collect();
        let input = quote! {
            async fn foo() {}
        };
        let stream = local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            async fn foo () {
                #[timeout(100)]
                async fn _foo_internal () { }
                serial_test::local_async_serial_core(vec![""], ::std::option::Option::None, _foo_internal() ).await;
            }
        };
        assert_eq!(format!("{}", compare), format!("{}", stream));
    }

    #[test]
    #[cfg(feature = "async")]
    fn test_inner_attrs_async_with_return() {
        init();
        let attrs: Vec<_> = quote! { inner_attrs = [timeout(100)] }
            .into_iter()
            .collect();
        let input = quote! {
            async fn foo() -> Result<(), ()> { Ok(()) }
        };
        let stream = local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            async fn foo () -> Result<(), ()> {
                #[timeout(100)]
                async fn _foo_internal () -> Result<(), ()> { Ok(()) }
                serial_test::local_async_serial_core_with_return(vec![""], ::std::option::Option::None, _foo_internal() ).await
            }
        };
        assert_eq!(format!("{}", compare), format!("{}", stream));
    }

    #[test]
    fn test_inner_attrs_with_namespaced_attr() {
        init();
        let attrs: Vec<_> = quote! { inner_attrs = [ntest::timeout(100)] }
            .into_iter()
            .collect();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        let stream = local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn foo () {
                #[ntest::timeout(100)]
                fn _foo_inner() {}
                serial_test::local_serial_core(vec![""], ::std::option::Option::None, || _foo_inner() );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    fn test_inner_attrs_parallel() {
        init();
        let attrs: Vec<_> = quote! { inner_attrs = [timeout(100)] }
            .into_iter()
            .collect();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        let stream = local_parallel_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn foo () {
                #[timeout(100)]
                fn _foo_inner() {}
                serial_test::local_parallel_core(vec![""], ::std::option::Option::None, || _foo_inner() );
            }
        };
        compare_streams(compare, stream);
    }

    #[test]
    #[should_panic(expected = "Expected '=' after 'inner_attrs'")]
    fn test_inner_attrs_missing_equals() {
        // Use a comma after the bracket to ensure we get past the length check
        let attrs: Vec<TokenTree> = quote! { inner_attrs [timeout(100)], foo }
            .into_iter()
            .collect();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
    }

    #[test]
    #[should_panic(expected = "Expected [...] after 'inner_attrs ='")]
    fn test_inner_attrs_missing_brackets() {
        let attrs: Vec<TokenTree> = quote! { inner_attrs = timeout(100) }.into_iter().collect();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
    }

    #[test]
    #[should_panic(expected = "Expected a '= [attr1, attr2, ...]' after 'inner_attrs'")]
    fn test_inner_attrs_nothing_after() {
        let attrs: Vec<TokenTree> = quote! { inner_attrs }.into_iter().collect();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
    }

    #[test]
    #[should_panic(expected = "Expected [...] after 'inner_attrs ='")]
    fn test_inner_attrs_wrong_delimiter() {
        // Using parentheses instead of brackets
        let attrs: Vec<TokenTree> = quote! { inner_attrs = (timeout(100)) }
            .into_iter()
            .collect();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        local_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
    }
}
