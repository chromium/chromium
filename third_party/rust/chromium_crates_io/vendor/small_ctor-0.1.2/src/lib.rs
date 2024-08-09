//! A dependency free library that declares functions to be automatically
//! executed before `main` is invoked.
//!
//! This crate implements the exact same behavior as the
//! [`ctor`](https://crates.io/crates/ctor) crate with the following
//! differences:
//!
//! * It has no dependencies other than `proc_macro` itself.
//! * It requires that functions are marked with `unsafe`.
//! * It can only be used with functions, not static items.
//! * It only supports `#[ctor]`
//!
//! ## Example
//!
//! This is a motivating example that registers a struct as a plugin in a
//! hypothetical global plugins registry:
//!
//! ```
//! struct MyPlugin;
//! # impl MyPlugin { fn register(&self, _: MyPlugin) {} }
//! # static PLUGINS: MyPlugin = MyPlugin;
//!
//! #[small_ctor::ctor]
//! unsafe fn register_plugin() {
//!     PLUGINS.register(MyPlugin);
//! }
//! ```
//!
//! ## Safety
//!
//! This library involves "life before main" which is explicitly not permitted
//! in Rust which is why this library is anything but safe.  In fact, it's a
//! really bad idea to do what this crate is promoting.  For instance at
//! present code that runs in `#[ctor]` will run before `lang_start` managed to
//! execute.  Some of the effects of this are that the main thread does not have
//! a name yet, the stack protection is not enabled and code must not panic.
//!
//! It's recommended that the only thing you do in a `#[ctor]` function is to
//! append to a vector, insert into a hashmap or similar.
//!
//! ## Recommended Usage
//!
//! It's recommended to perform basic operations which are unlikely to panic
//! and to defer most work to when the actual `main` happens.  So for
//! instead instead of initializing plugins in the `ctor`, just push them
//! to a plugin registry and then trigger callbacks registered that way
//! regularly in `main`.
//!
//! ## Compiler and Linker Bugs
//!
//! Currently this library is prone to break due to compiler bugs in subtle
//! ways.  The core issue is [rust #47384](https://github.com/rust-lang/rust/issues/47384).
//! You can reduze the likelihood of it happening by disabling incremental
//! compilation or setting `codegen-units` to `1` in your profile in the
//! `Cargo.toml`.
//!
//! ## Destructors
//!
//! This crate intentionally does not support an at-exit mechanism.  The reason
//! for this is that those are running so late that even more Rust code is
//! unable to properly run.  Not only does panicking not work, the entire standard
//! IO system is already unusable.  More importantly on many platforms these
//! do not run properly.  For instance on macOS destructors do not run when
//! thread local storage is in use.  If you do want to use something like this
//! you can do something like invoke `libc::atexit` from within a `#[ctor]`
//! function.
use proc_macro::{Delimiter, Group, Ident, Literal, Punct, Spacing, Span, TokenStream, TokenTree};

fn get_function_name(stream: TokenStream) -> String {
    let mut iter = stream.into_iter();

    macro_rules! unexpected {
        () => {
            panic!("#[ctor] can only be applied to unsafe functions")
        };
    }

    macro_rules! expect_ident {
        () => {
            match iter.next() {
                Some(TokenTree::Ident(ident)) => ident,
                _ => unexpected!(),
            }
        };
    }

    while let Some(token) = iter.next() {
        if let TokenTree::Ident(ident) = token {
            if ident.to_string() != "unsafe" || expect_ident!().to_string() != "fn" {
                unexpected!()
            }
            return expect_ident!().to_string();
        }
    }

    unexpected!();
}

macro_rules! tokens {
    ($($expr:expr),* $(,)?) => {
        vec![$($expr,)*].into_iter().collect::<TokenStream>()
    }
}

/// Marks a function or static variable as a library/executable constructor.
/// This uses OS-specific linker sections to call a specific function at load
/// time.
///
/// Multiple startup functions/statics are supported, but the invocation order
/// is not guaranteed.  For information about what is safe or not safe to do
/// in such functions refer to the module documention.
///
/// # Example
///
/// ```
/// # struct MyPlugin;
/// # impl MyPlugin { fn insert(&self, _: MyPlugin) {} }
/// #[small_ctor::ctor]
/// unsafe fn register_plugin() {
/// # let PLUGINS = MyPlugin;
///     PLUGINS.insert(MyPlugin);
/// }
/// ```
#[proc_macro_attribute]
pub fn ctor(args: TokenStream, input: TokenStream) -> TokenStream {
    if args.into_iter().next().is_some() {
        panic!("#[ctor] takes no arguments");
    }
    let name = get_function_name(input.clone());
    let ctor_ident = TokenTree::Ident(Ident::new(
        &format!("___{}___ctor", name),
        Span::call_site(),
    ));
    vec![
        TokenTree::Punct(Punct::new('#', Spacing::Alone)),
        TokenTree::Group(Group::new(
            Delimiter::Bracket,
            tokens![TokenTree::Ident(Ident::new("used", Span::call_site()))],
        )),
        TokenTree::Punct(Punct::new('#', Spacing::Alone)),
        TokenTree::Group(Group::new(
            Delimiter::Bracket,
            tokens![
                TokenTree::Ident(Ident::new("doc", Span::call_site())),
                TokenTree::Group(Group::new(
                    Delimiter::Parenthesis,
                    vec![TokenTree::Ident(Ident::new("hidden", Span::call_site()))]
                        .into_iter()
                        .collect(),
                )),
            ],
        )),
        TokenTree::Punct(Punct::new('#', Spacing::Alone)),
        TokenTree::Group(Group::new(
            Delimiter::Bracket,
            tokens![
                TokenTree::Ident(Ident::new("allow", Span::call_site())),
                TokenTree::Group(Group::new(
                    Delimiter::Parenthesis,
                    tokens![TokenTree::Ident(Ident::new(
                        "non_upper_case_globals",
                        Span::call_site(),
                    ))]
                )),
            ],
        )),
        TokenTree::Punct(Punct::new('#', Spacing::Alone)),
        TokenTree::Group(Group::new(
            Delimiter::Bracket,
            tokens![
                TokenTree::Ident(Ident::new("cfg_attr", Span::call_site())),
                TokenTree::Group(Group::new(
                    Delimiter::Parenthesis,
                    tokens![
                        TokenTree::Ident(Ident::new("any", Span::call_site())),
                        TokenTree::Group(Group::new(
                            Delimiter::Parenthesis,
                            tokens![
                                TokenTree::Ident(Ident::new("target_os", Span::call_site())),
                                TokenTree::Punct(Punct::new('=', Spacing::Alone)),
                                TokenTree::Literal(Literal::string("linux")),
                                TokenTree::Punct(Punct::new(',', Spacing::Alone)),
                                TokenTree::Ident(Ident::new("target_os", Span::call_site())),
                                TokenTree::Punct(Punct::new('=', Spacing::Alone)),
                                TokenTree::Literal(Literal::string("freebsd")),
                                TokenTree::Punct(Punct::new(',', Spacing::Alone)),
                                TokenTree::Ident(Ident::new("target_os", Span::call_site())),
                                TokenTree::Punct(Punct::new('=', Spacing::Alone)),
                                TokenTree::Literal(Literal::string("netbsd")),
                                TokenTree::Punct(Punct::new(',', Spacing::Alone)),
                                TokenTree::Ident(Ident::new("target_os", Span::call_site())),
                                TokenTree::Punct(Punct::new('=', Spacing::Alone)),
                                TokenTree::Literal(Literal::string("android")),
                            ],
                        )),
                        TokenTree::Punct(Punct::new(',', Spacing::Alone)),
                        TokenTree::Ident(Ident::new("link_section", Span::call_site())),
                        TokenTree::Punct(Punct::new('=', Spacing::Alone)),
                        TokenTree::Literal(Literal::string(".init_array")),
                    ],
                ))
            ],
        )),
        TokenTree::Punct(Punct::new('#', Spacing::Alone)),
        TokenTree::Group(Group::new(
            Delimiter::Bracket,
            tokens![
                TokenTree::Ident(Ident::new("cfg_attr", Span::call_site())),
                TokenTree::Group(Group::new(
                    Delimiter::Parenthesis,
                    tokens![
                        TokenTree::Ident(Ident::new("target_vendor", Span::call_site())),
                        TokenTree::Punct(Punct::new('=', Spacing::Alone)),
                        TokenTree::Literal(Literal::string("apple")),
                        TokenTree::Punct(Punct::new(',', Spacing::Alone)),
                        TokenTree::Ident(Ident::new("link_section", Span::call_site())),
                        TokenTree::Punct(Punct::new('=', Spacing::Alone)),
                        TokenTree::Literal(Literal::string("__DATA,__mod_init_func")),
                    ],
                ))
            ],
        )),
        TokenTree::Punct(Punct::new('#', Spacing::Alone)),
        TokenTree::Group(Group::new(
            Delimiter::Bracket,
            tokens![
                TokenTree::Ident(Ident::new("cfg_attr", Span::call_site())),
                TokenTree::Group(Group::new(
                    Delimiter::Parenthesis,
                    tokens![
                        TokenTree::Ident(Ident::new("target_os", Span::call_site())),
                        TokenTree::Punct(Punct::new('=', Spacing::Alone)),
                        TokenTree::Literal(Literal::string("windows")),
                        TokenTree::Punct(Punct::new(',', Spacing::Alone)),
                        TokenTree::Ident(Ident::new("link_section", Span::call_site())),
                        TokenTree::Punct(Punct::new('=', Spacing::Alone)),
                        TokenTree::Literal(Literal::string(".CRT$XCU")),
                    ],
                ))
            ],
        )),
        TokenTree::Ident(Ident::new("static", Span::call_site())),
        ctor_ident.clone(),
        TokenTree::Punct(Punct::new(':', Spacing::Alone)),
        TokenTree::Ident(Ident::new("unsafe", Span::call_site())),
        TokenTree::Ident(Ident::new("extern", Span::call_site())),
        TokenTree::Literal(Literal::string("C")),
        TokenTree::Ident(Ident::new("fn", Span::call_site())),
        TokenTree::Group(Group::new(Delimiter::Parenthesis, TokenStream::default())),
        TokenTree::Punct(Punct::new('=', Spacing::Alone)),
        TokenTree::Group(Group::new(
            Delimiter::Brace,
            tokens![
                TokenTree::Punct(Punct::new('#', Spacing::Alone)),
                TokenTree::Group(Group::new(
                    Delimiter::Bracket,
                    tokens![
                        TokenTree::Ident(Ident::new("cfg_attr", Span::call_site())),
                        TokenTree::Group(Group::new(
                            Delimiter::Parenthesis,
                            tokens![
                                TokenTree::Ident(Ident::new("any", Span::call_site())),
                                TokenTree::Group(Group::new(
                                    Delimiter::Parenthesis,
                                    tokens![
                                        TokenTree::Ident(Ident::new(
                                            "target_os",
                                            Span::call_site()
                                        )),
                                        TokenTree::Punct(Punct::new('=', Spacing::Alone)),
                                        TokenTree::Literal(Literal::string("linux")),
                                        TokenTree::Punct(Punct::new(',', Spacing::Alone)),
                                        TokenTree::Ident(Ident::new(
                                            "target_os",
                                            Span::call_site()
                                        )),
                                        TokenTree::Punct(Punct::new('=', Spacing::Alone)),
                                        TokenTree::Literal(Literal::string("android")),
                                    ],
                                )),
                                TokenTree::Punct(Punct::new(',', Spacing::Alone)),
                                TokenTree::Ident(Ident::new("link_section", Span::call_site())),
                                TokenTree::Punct(Punct::new('=', Spacing::Alone)),
                                TokenTree::Literal(Literal::string(".text.startup")),
                            ],
                        ))
                    ],
                )),
                TokenTree::Ident(Ident::new("unsafe", Span::call_site())),
                TokenTree::Ident(Ident::new("extern", Span::call_site())),
                TokenTree::Literal(Literal::string("C")),
                TokenTree::Ident(Ident::new("fn", Span::call_site())),
                ctor_ident.clone(),
                TokenTree::Group(Group::new(Delimiter::Parenthesis, TokenStream::default())),
                TokenTree::Group(Group::new(
                    Delimiter::Brace,
                    vec![
                        TokenTree::Ident(Ident::new(&name, Span::call_site())),
                        TokenTree::Group(Group::new(
                            Delimiter::Parenthesis,
                            TokenStream::default(),
                        )),
                    ]
                    .into_iter()
                    .collect(),
                )),
                ctor_ident.clone(),
            ],
        )),
        TokenTree::Punct(Punct::new(';', Spacing::Alone)),
    ]
    .into_iter()
    .chain(input.into_iter())
    .collect()
}
