use std::fmt;

use proc_macro2::{Span, TokenStream};
use quote::{quote, spanned::Spanned};
use syn::LitInt;
use syn::{meta::ParseNestedMeta, Token};

use crate::bit_size::Bits;

enum Error<'a> {
    MissingSize,
    InvalidExhaustive,
    InvalidAttribute,
    InvalidExhaustiveNextToken,
    NotConditional,
    NotExhaustive {
        count: u32,
        size: usize,
    },
    Exhaustive {
        count: u32,
        max_count: u128,
    },
    TooManyVariants {
        count: u32,
        max_count: u128,
    },
    NonLitDiscriminant {
        variant: &'a syn::Ident,
        suggest: u128,
    },
    TooLargeDiscriminant {
        max_value: u128,
    },
}

impl fmt::Display for Error<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::MissingSize => write!(
                f,
                "Missing the storage type. It must be explicitly declared with #[bitenum(uN)], where N in range 1..=64"
            ),
            Self::InvalidAttribute => write!(
                f,
                "Invalid attribute. Expected either 'uN' where N in range 1..=64, or 'exhaustive = …'."
            ),
            Self::InvalidExhaustiveNextToken => write!(
                f,
                "'exhaustive' should be specified as 'exhaustive = …'. Possible values are: true, false, conditional."
            ),
            Self::InvalidExhaustive => write!(
                f,
                "The specified 'exhaustive' is invalid. Possible values are: true, false, conditional."
            ),
            Self::NotConditional => write!(
                f,
                "The enum contains at least one variant with a '#[cfg(…)]' attribute. It should be specified as 'exhaustive = conditional'."
            ),
            Self::NotExhaustive { count, size } => write!(
                f,
                "The enum has {count} variants, it is exhaustive for {size} bits. Either remove variants, use a larger storage type, or mark this enum as 'exhaustive = true'."
            ),
            Self::Exhaustive { count, max_count } => write!(
                f,
                "The enum has {count} variants, it would need {max_count} variants to be exhaustive. Either add variants, use a smaller storage type, or mark this enum as 'exhaustive = false'."
            ),
            Self::TooManyVariants { count, max_count } => write!(
                f,
                "The enum has more variants than can be stored in the provided storage type. Either use a larger storage type or reduce the number of variants. Up to {max_count} variants possible, got {count}."
            ),
            Self::NonLitDiscriminant { variant, suggest } => write!(
                f,
                "Discriminants must be literal integers. Eg: '{variant} = {suggest}' '{suggest:#x}' '{suggest:#b}' '{suggest:#o}'."
            ),
            Self::TooLargeDiscriminant { max_value } => write!(
                f,
                "The largest discriminant value is larger than can be stored in the provided storage type. Max discriminant value is {max_value}."
            ),
        }
    }
}

#[derive(Clone, Copy)]
enum Exhaustiveness {
    True,
    False,
    Conditional,
}
struct Exhaustive {
    span: Span,
    kind: Exhaustiveness,
}

impl Exhaustive {
    fn is_conditional(&self) -> bool {
        matches!(self.kind, Exhaustiveness::Conditional)
    }
    fn matches(&self, expected: bool) -> bool {
        !matches!(
            (self.kind, expected),
            (Exhaustiveness::True, false) | (Exhaustiveness::False, true)
        )
    }
}
impl syn::parse::Parse for Exhaustive {
    fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
        let span = input.span();
        let kind = match (input.parse::<syn::LitBool>(), input.parse::<syn::Ident>()) {
            (Ok(bool), _) if bool.value => Exhaustiveness::True,
            (Ok(_), _) => Exhaustiveness::False,
            (_, Ok(ident)) if ident == "conditional" => Exhaustiveness::Conditional,
            _ => return Err(syn::Error::new(span, Error::InvalidExhaustive)),
        };
        Ok(Exhaustive { span, kind })
    }
}

#[derive(Default)]
pub(crate) struct Config {
    explicit_bits: Option<Bits>,
    explicit_exhaustive: Option<Exhaustive>,
}
struct FullConfig {
    bits: Bits,
    exhaustive: Exhaustive,
}

impl Config {
    pub(crate) fn parse(&mut self, meta: ParseNestedMeta) -> syn::Result<()> {
        if meta.path.is_ident("exhaustive") {
            let result1 = meta.input.parse::<Token![:]>();
            let result2 = meta.input.parse::<Token![=]>();
            if result1.is_err() && result2.is_err() {
                return Err(meta.error(Error::InvalidExhaustiveNextToken));
            }
            self.explicit_exhaustive = Some(meta.input.parse()?);
        } else {
            let err = || meta.error(Error::InvalidAttribute);
            let last_segment = meta.path.segments.last().ok_or_else(err)?;
            let value = last_segment.ident.to_string();
            let ("u", size) = value.split_at(1) else {
                return Err(err());
            };
            self.explicit_bits = Some(Bits {
                path: meta.path.clone(),
                size: size.parse().map_err(|_| err())?,
            });
        }
        Ok(())
    }

    fn explicit(self) -> syn::Result<FullConfig> {
        let span = Span::call_site();
        let Some(bits) = self.explicit_bits else {
            return Err(syn::Error::new(span, Error::MissingSize));
        };
        let exhaustive = self.explicit_exhaustive.unwrap_or(Exhaustive {
            span,
            kind: Exhaustiveness::False,
        });
        Ok(FullConfig { bits, exhaustive })
    }
}

fn conditional_attr(attr: &syn::Attribute) -> bool {
    attr.path().is_ident("cfg")
}
fn check_explicit_conditional(config: &FullConfig, input: &syn::ItemEnum) -> syn::Result<()> {
    let conditional_variant = |v: &syn::Variant| v.attrs.iter().any(conditional_attr);
    let is_conditional = input.variants.iter().any(conditional_variant);

    if is_conditional && !config.exhaustive.is_conditional() {
        let span = config.exhaustive.span;
        Err(syn::Error::new(span, Error::NotConditional))
    } else {
        Ok(())
    }
}
/// Determine the integer value itself.
///
/// While we don't need it further down (for now), this ensures that only
/// constants are being used; due to the way how new_with_raw_value() is
/// written, some expressions would cause compilation issues (e.g. those that
/// refer to other enum values).
fn parse_expr(expr: &syn::Expr) -> Option<u128> {
    let syn::Expr::Lit(lit) = expr else {
        return None;
    };
    let syn::Lit::Int(lit_int) = &lit.lit else {
        return None;
    };
    lit_int.base10_parse().ok()
}

fn check_explicit_exhaustive(
    config: &FullConfig,
    input: &syn::ItemEnum,
) -> syn::Result<Vec<LitInt>> {
    let max_count = 1_u128 << config.bits.size;
    let count = input.variants.len() as u128;
    let actually_exhaustive = match count.cmp(&max_count) {
        std::cmp::Ordering::Equal => true,
        std::cmp::Ordering::Greater if !config.exhaustive.is_conditional() => {
            let count = count as u32;
            let err = Error::TooManyVariants { max_count, count };
            return Err(syn::Error::new_spanned(&config.bits.path, err));
        }
        _ => false,
    };
    let mut values = Vec::new();
    if !config.exhaustive.matches(actually_exhaustive) {
        let size = config.bits.size;
        let count = count as u32;
        let err = if actually_exhaustive {
            Error::NotExhaustive { count, size }
        } else {
            Error::Exhaustive { count, max_count }
        };
        return Err(syn::Error::new(config.exhaustive.span, err));
    }
    let (mut max_discr, mut next_implicit_discr, mut max_discr_span) = (0, 0, Span::call_site());
    for variant in &input.variants {
        let (value, discr_span) = match variant.discriminant.as_ref() {
            None => (next_implicit_discr, variant.ident.span()),
            Some((_, discriminant)) => {
                let Some(value) = parse_expr(discriminant) else {
                    let variant = &variant.ident;
                    let suggest = max_discr + 1;
                    let err = Error::NonLitDiscriminant { variant, suggest };
                    return Err(syn::Error::new_spanned(discriminant, err));
                };
                (value, discriminant.__span())
            }
        };
        next_implicit_discr = value + 1;
        if value > max_discr {
            max_discr = value;
            max_discr_span = discr_span;
        }
        values.push(LitInt::new(&value.to_string(), discr_span));
    }
    if max_discr >= max_count {
        let max_value = max_count - 1;
        let err = Error::TooLargeDiscriminant { max_value };
        return Err(syn::Error::new(max_discr_span, err));
    }
    Ok(values)
}

/// Generate _some code_ when the declared `#[bitenum]` is invalid,
/// to avoid compilation errors caused by the `enum` not existing and missing
/// methods.
pub(crate) fn fallback_impl(input: &syn::ItemEnum) -> TokenStream {
    let name = &input.ident;
    // Error string in unreachable! would be nice, but causes errors because a non-const
    // formatting macro is used in a const method.
    quote! {
        #[derive(Copy, Clone)]
        #input

        impl #name {
            pub const fn raw_value<T>(self) -> T {
                unreachable!()
            }
            pub const fn new_with_raw_value<T>(value: T) -> Self {
                unreachable!()
            }
            pub fn unwrap(self) -> Self { self }
        }
    }
}
pub(crate) fn bitenum(config: Config, input: &syn::ItemEnum) -> syn::Result<TokenStream> {
    let config = config.explicit()?;
    check_explicit_conditional(&config, input)?;
    let values = check_explicit_exhaustive(&config, input)?;

    let bits = config.bits;
    let (base_type, qualified_type) = (bits.base_type()?, bits.qualified_path()?);
    let (raw_value_constructor, reader) = (bits.constructor()?, bits.reader());

    let non_exhaustive = config.exhaustive.matches(false);
    let ok = non_exhaustive.then_some(quote!(Ok));
    let new_return_type = match non_exhaustive {
        true => quote!(Result<Self, #qualified_type>),
        false => quote!(Self),
    };
    let new_default_branch = match non_exhaustive {
        true => quote!(_ => Err(value)),
        false => quote!(_ => unreachable!()),
    };
    let new_match_branches = input.variants.iter().zip(values).map(|(variant, value)| {
        let cfg_attrs = variant.attrs.iter().filter(|a| conditional_attr(a));
        let variant_name = &variant.ident;
        quote!( #( #cfg_attrs )* (#value) => #ok(Self::#variant_name) )
    });
    let (attrs, vis, name, variants) = (&input.attrs, &input.vis, &input.ident, &input.variants);
    Ok(quote! {
        #[derive(Copy, Clone)]
        #( #attrs )*
        #vis enum #name {
            #variants
        }

        impl #name {
            /// Returns the underlying raw value of this bitfield.
            pub const fn raw_value(self) -> #qualified_type {
                #raw_value_constructor(self as #base_type)
            }

            /// Creates a new instance of this bitfield with the given raw value.
            pub const fn new_with_raw_value(value: #qualified_type) -> #new_return_type {
                match value #reader {
                    #( #new_match_branches ,)*
                    #new_default_branch
                }
            }
        }
    })
}
