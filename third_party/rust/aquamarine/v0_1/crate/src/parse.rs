use proc_macro2::TokenStream;
use syn::{
    self,
    parse::{Parse, ParseStream},
    Attribute,
};

pub struct Input {
    pub attrs: Vec<Attribute>,
    pub rest: TokenStream,
}

impl Parse for Input {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let attrs = input.call(Attribute::parse_outer)?;
        let rest = input.parse()?;
        Ok(Input { attrs, rest })
    }
}
