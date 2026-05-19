use proc_macro2::TokenStream;
use quote::quote;
use quote::spanned::Spanned;

const BAD_SIZE: &str = "The specified storage size is invalid, valid is in range 1..=64";

/// Explicitly declared storage type for `bitenum` enums.
pub(crate) struct Bits {
    pub(crate) path: syn::Path,
    pub(crate) size: usize,
}

impl Bits {
    pub(crate) fn base_type(&self) -> syn::Result<syn::Ident> {
        let span = self.path.__span();
        let ident_str = match self.size {
            1..=8 => "u8",
            9..=16 => "u16",
            17..=32 => "u32",
            33..=64 => "u64",
            _ => return Err(syn::Error::new(span, BAD_SIZE)),
        };
        Ok(syn::Ident::new(ident_str, span))
    }
    fn is_arbitrary_int(&self) -> bool {
        let is_ident = self.path.get_ident().is_some();
        let is_native_int = [8, 16, 32, 64].contains(&self.size);
        !is_native_int && is_ident
    }
    pub(crate) fn qualified_path(&self) -> syn::Result<TokenStream> {
        let (path, base, size) = (&self.path, self.base_type()?, self.size);

        if self.is_arbitrary_int() {
            Ok(quote!(arbitrary_int::UInt::<#base, #size>))
        } else {
            Ok(quote!(#path))
        }
    }
    pub(crate) fn constructor(&self) -> syn::Result<Option<TokenStream>> {
        let (base, size) = (self.base_type()?, self.size);
        let some = || quote!(arbitrary_int::UInt::<#base, #size>::new);
        Ok(self.is_arbitrary_int().then(some))
    }
    pub(crate) fn reader(&self) -> Option<TokenStream> {
        self.is_arbitrary_int().then_some(quote!(.value()))
    }
}
