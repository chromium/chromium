use syn::parse::{Parse, ParseStream};

use crate::code::Code;
use crate::forward::Forward;
use crate::help::Help;
use crate::severity::Severity;
use crate::url::Url;

pub enum DiagnosticArg {
    Transparent,
    Code(Code),
    Severity(Severity),
    Help(Help),
    Url(Url),
    Forward(Forward),
}

impl Parse for DiagnosticArg {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let ident = input.fork().parse::<syn::Ident>()?;
        if ident == "transparent" {
            // consume the token
            let _: syn::Ident = input.parse()?;
            Ok(DiagnosticArg::Transparent)
        } else if ident == "forward" {
            Ok(DiagnosticArg::Forward(input.parse()?))
        } else if ident == "code" {
            Ok(DiagnosticArg::Code(input.parse()?))
        } else if ident == "severity" {
            Ok(DiagnosticArg::Severity(input.parse()?))
        } else if ident == "help" {
            Ok(DiagnosticArg::Help(input.parse()?))
        } else if ident == "url" {
            Ok(DiagnosticArg::Url(input.parse()?))
        } else {
            Err(syn::Error::new(
                ident.span(),
                "Unrecognized diagnostic option",
            ))
        }
    }
}
