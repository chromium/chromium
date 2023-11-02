//! Aquamarine is a procedural macro extension for [rustdoc](https://doc.rust-lang.org/rustdoc/what-is-rustdoc.html),
//! that aims to improve the visual component of Rust documentation through use of the [mermaid.js](https://mermaid-js.github.io/mermaid/#/) diagrams.
//!
//! `#[aquamarine]` macro works through embedding the [mermaid.js](https://github.com/mermaid-js/mermaid) into the generated rustdoc HTML page, modifying the doc comment attributes.
//!
//! To inline a diagram into the documentation, use the `mermaid` snippet in a doc-string:
//!
//! ```rust
//! # use aquamarine::aquamarine;
//! #[cfg_attr(doc, aquamarine)]
//! /// ```mermaid
//! /// graph LR
//! ///     s([Source]) --> a[[aquamarine]]
//! ///     r[[rustdoc]] --> f([Docs w/ Mermaid!])
//! ///     subgraph rustc[Rust Compiler]
//! ///     a -. inject mermaid.js .-> r
//! ///     end
//! /// ```
//! pub fn example() {}
//! ```
//! The diagram will appear in place of the `mermaid` code block, preserving all the comments around it.
//!
//! You can even add multiple diagrams!
//!
//! To see it in action, go to the [demo crate](https://docs.rs/aquamarine-demo-crate/0.1.11/aquamarine_demo_crate/fn.example.html) docs.rs page.
//!
//! ### Dark-mode
//!
//! Aquamarine will automatically select the `dark` theme as a default, if the current `rustdoc` theme is either `ayu` or `dark`.
//!
//! You might need to reload the page to redraw the diagrams after changing the theme.
//!
//! ### Custom themes
//!
//! Theming is supported on per-diagram basis, through the mermaid's `%%init%%` attribute.
//!
//! *Note*: custom theme will override the default theme
//!
//! ```no_run
//! /// ```mermaid
//! /// %%{init: {
//! ///     'theme': 'base',
//! ///     'themeVariables': {
//! ///            'primaryColor': '#ffcccc',
//! ///            'edgeLabelBackground':'#ccccff',
//! ///            'tertiaryColor': '#fff0f0' }}}%%
//! /// graph TD
//! ///      A(Diagram needs to be drawn) --> B{Does it have 'init' annotation?}
//! ///      B -->|No| C(Apply default theme)
//! ///      B -->|Yes| D(Apply customized theme)
//! /// ```
//! ```
//!
//! [Demo on docs.rs](https://docs.rs/aquamarine-demo-crate/0.1.11/aquamarine_demo_crate/fn.example_with_styling.html)
//!
//! To learn more, see the [Theming Section](https://mermaid-js.github.io/mermaid/#/theming) of the mermaid.js book

extern crate proc_macro;

use proc_macro::TokenStream;
use proc_macro_error::{abort, proc_macro_error};

use quote::quote;
use syn::{parse_macro_input, Attribute};

mod attrs;
mod parse;

/// Aquamarine is a proc-macro that adds [Mermaid](https://mermaid-js.github.io/mermaid/#/) diagrams to rustdoc
///
/// To inline a diagram into the documentation, use the `mermaid` snippet:
///
/// ```rust
/// # use aquamarine::aquamarine;
/// #[aquamarine]
/// /// ```mermaid
/// ///   --- here goes your mermaid diagram ---
/// /// ```
/// struct Foo;
/// ```
#[proc_macro_attribute]
#[proc_macro_error]
pub fn aquamarine(_args: TokenStream, input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as parse::Input);

    check_input_attrs(&input.attrs);

    let attrs = attrs::Attrs::from(input.attrs);
    let forward = input.rest;

    let tokens = quote! {
        #attrs
        #forward
    };

    tokens.into()
}

fn check_input_attrs(input: &[Attribute]) {
    for attr in input {
        if attr.path.is_ident("aquamarine") {
            abort!(
                attr,
                "multiple `aquamarine` attributes on one entity are illegal"
            );
        }
    }
}
