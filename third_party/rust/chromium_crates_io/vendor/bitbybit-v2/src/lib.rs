use proc_macro::TokenStream as TokenStream1;
use quote::quote;
use syn::parse_macro_input;

mod bit_size;
mod bitenum;
mod bitfield;

/// Defines a bitfield: `#[bitfield(<base-data-type>, default = 0)]`
/// `<base-data-type>` is a data type like [`u32`] which is used to represent all the bits of the bitfield.
/// `default` is an optional default when the bitfield is created
#[proc_macro_attribute]
pub fn bitfield(args: TokenStream1, input: TokenStream1) -> TokenStream1 {
    bitfield::bitfield(args, input)
}

/// Defines a bitenum: `#[bitenum(<base-data-type>, exhaustive = true)]`
/// `<base-data-type>` is a data type like [`u2`] or [`u8`]
/// which is used as the storage type when in in bitfields.
/// `exhaustive` specifies whether the bitenum includes all possible value of the
/// given base data type (for example, a bitenum over [`u2`] with 4 values is exhaustive)
///
/// [`u2`]: arbitrary_int::u2
#[proc_macro_attribute]
pub fn bitenum(args: TokenStream1, input: TokenStream1) -> TokenStream1 {
    let mut config = bitenum::Config::default();

    let config_parser = syn::meta::parser(|meta| config.parse(meta));
    parse_macro_input!(args with config_parser);

    let input = parse_macro_input!(input as syn::ItemEnum);
    match bitenum::bitenum(config, &input) {
        Ok(stream) => stream.into(),
        Err(err) => {
            let error = err.into_compile_error();
            let input = bitenum::fallback_impl(&input);
            quote!(#input #error).into()
        }
    }
}
