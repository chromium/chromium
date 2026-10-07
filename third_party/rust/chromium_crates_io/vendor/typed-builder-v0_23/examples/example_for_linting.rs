#![deny(clippy::all, clippy::pedantic)]
//! This example is mainly to make sure `TypedBuilder` does not do anything that triggers Clippy
//! warnings. It's not for checking behavior.

use typed_builder::TypedBuilder;

#[derive(TypedBuilder)]
struct BigStruct<'a> {
    option_with_ref: Option<&'a str>,

    #[builder(default = None)]
    option_with_default: Option<()>,
}

fn main() {
    #[allow(unused)]
    let BigStruct {
        option_with_ref: option_with_str,
        option_with_default: skipped_option,
    } = BigStruct::builder()
        .option_with_ref(Some("option with string"))
        .option_with_default(None)
        .build();
}
