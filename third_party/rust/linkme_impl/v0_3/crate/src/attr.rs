use syn::parse::{Error, Result};
use syn::{parse_quote, Attribute, Path};

// #[linkme(crate = path::to::linkme)]
pub(crate) fn linkme_path(attrs: &mut Vec<Attribute>) -> Result<Path> {
    let mut linkme_path = None;
    let mut errors: Option<Error> = None;

    attrs.retain(|attr| {
        if !attr.path().is_ident("linkme") {
            return true;
        }
        if let Err(err) = attr.parse_nested_meta(|meta| {
            if meta.path.is_ident("crate") {
                if linkme_path.is_some() {
                    return Err(meta.error("duplicate linkme crate attribute"));
                }
                let path = meta.value()?.call(Path::parse_mod_style)?;
                linkme_path = Some(path);
                Ok(())
            } else {
                Err(meta.error("unsupported linkme attribute"))
            }
        }) {
            match &mut errors {
                None => errors = Some(err),
                Some(errors) => errors.combine(err),
            }
        }
        false
    });

    match errors {
        None => Ok(linkme_path.unwrap_or_else(|| parse_quote!(::linkme))),
        Some(errors) => Err(errors),
    }
}
