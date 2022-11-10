use proc_macro2::TokenStream;
use quote::ToTokens;
use syn::{parse2, ItemMod};

use crate::BindgenOptions;

mod merge_extern_blocks;
mod sort_semantically;

use merge_extern_blocks::merge_extern_blocks;
use sort_semantically::sort_semantically;

struct PostProcessingPass {
    should_run: fn(&BindgenOptions) -> bool,
    run: fn(&mut ItemMod),
}

// TODO: This can be a const fn when mutable references are allowed in const
// context.
macro_rules! pass {
    ($pass:ident) => {
        PostProcessingPass {
            should_run: |options| options.$pass,
            run: |item_mod| $pass(item_mod),
        }
    };
}

const PASSES: &[PostProcessingPass] =
    &[pass!(merge_extern_blocks), pass!(sort_semantically)];

pub(crate) fn postprocessing(
    items: Vec<TokenStream>,
    options: &BindgenOptions,
) -> TokenStream {
    let require_syn = PASSES.iter().any(|pass| (pass.should_run)(options));
    if !require_syn {
        return items.into_iter().collect();
    }
    let module_wrapped_tokens =
        quote!(mod wrapper_for_postprocessing_hack { #( #items )* });

    // This syn business is a hack, for now. This means that we are re-parsing already
    // generated code using `syn` (as opposed to `quote`) because `syn` provides us more
    // control over the elements.
    // One caveat is that some of the items coming from `quote`d output might have
    // multiple items within them. Hence, we have to wrap the incoming in a `mod`.
    // The `unwrap` here is deliberate because bindgen should generate valid rust items at all
    // times.
    let mut item_mod = parse2::<ItemMod>(module_wrapped_tokens).unwrap();

    for pass in PASSES {
        if (pass.should_run)(options) {
            (pass.run)(&mut item_mod);
        }
    }

    let synful_items = item_mod
        .content
        .map(|(_, items)| items)
        .unwrap_or_default()
        .into_iter()
        .map(|item| item.into_token_stream());

    quote! { #( #synful_items )* }
}
