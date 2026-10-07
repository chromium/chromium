mod codegen;
mod parsing;

use proc_macro::Span;
use proc_macro::TokenStream;
use proc_macro2::Ident;
use proc_macro2::TokenStream as TokenStream2;
use quote::{quote, quote_spanned, ToTokens};
use std::ops::Range;
use std::str::FromStr;
use syn::meta::ParseNestedMeta;
use syn::LitStr;
use syn::{parse_macro_input, Attribute, Data, DeriveInput, LitInt, Token, Type};

/// In the code below, bools are considered to have 0 bits. This lets us distinguish them
/// from u1
const BITCOUNT_BOOL: usize = 0;

/// Returns true if the number can be expressed by a regular data type like u8 or u32.
/// 0 is special as it means bool (technically should be 1, but we use that for u1)
const fn is_int_size_regular_type(size: usize) -> bool {
    size == BITCOUNT_BOOL || size == 8 || size == 16 || size == 32 || size == 64 || size == 128
}

/// Parses an integer string, like "u8" or "i24".
/// Allows "i" types if allow_signed is true.
///
/// Returns Some((size, is_signed)) if successful, None if not.
fn try_parse_arbitrary_int_type(s: &str, allow_signed: bool) -> Option<(usize, bool)> {
    if s.len() < 2 {
        return None;
    }

    let is_unsigned = s.starts_with('u');
    let is_signed = s.starts_with('i');

    if is_unsigned || (is_signed && allow_signed) {
        let size = usize::from_str(s.split_at(1).1);
        match size {
            Ok(size) => {
                if (1..128).contains(&size) && !is_int_size_regular_type(size) {
                    Some((size, is_signed))
                } else {
                    None
                }
            }
            Err(_) => None,
        }
    } else {
        None
    }
}

#[derive(Debug, Clone, Copy)]
pub struct ArrayInfo {
    count: usize,
    indexed_stride: usize,
}

#[cfg_attr(feature = "extra-traits", derive(Debug))]
struct FieldDefinition {
    field_name: Ident,
    ranges: Vec<Range<usize>>,
    unsigned_field_type: Option<Type>,
    array: Option<ArrayInfo>,
    field_type_size: usize,
    getter_type: Option<Type>,
    setter_type: Option<Type>,
    field_type_size_from_data_type: Option<usize>,
    is_signed: bool,
    /// If non-null: (count, stride)
    use_regular_int: bool,
    primitive_type: TokenStream2,
    custom_type: CustomType,
    doc_comment: Vec<Attribute>,
}

// If a convert_type is given, that will be the final getter/setter type. If not, it is the base type
#[cfg_attr(feature = "extra-traits", derive(Debug))]
enum CustomType {
    No,
    /// Boxed because this is a relatively large type.
    Yes(Box<Type>),
}

#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Copy, Clone)]
struct BaseDataSize {
    /// The size of the raw_value field, e.g. u32
    internal: usize,

    /// The size exposed via raw_value() and new_with_raw_value(), e.g. u24
    exposed: usize,
}

impl BaseDataSize {
    const fn new(size: usize) -> Self {
        let built_in_size = if size <= 8 {
            8
        } else if size <= 16 {
            16
        } else if size <= 32 {
            32
        } else if size <= 64 {
            64
        } else {
            128
        };
        assert!(size <= built_in_size);
        Self {
            internal: built_in_size,
            exposed: size,
        }
    }
}

#[cfg_attr(feature = "extra-traits", derive(Debug))]
pub enum DefaultVal {
    Lit(LitInt),
    Constant(Ident),
}

impl ToTokens for DefaultVal {
    fn to_tokens(&self, tokens: &mut TokenStream2) {
        match self {
            DefaultVal::Lit(lit) => lit.to_tokens(tokens),
            DefaultVal::Constant(ident) => ident.to_tokens(tokens),
        }
    }
}

#[cfg_attr(feature = "extra-traits", derive(Debug))]
pub struct DefmtTrait {
    variant: DefmtVariant,
    feature_gate: Option<String>,
}

#[cfg_attr(feature = "extra-traits", derive(Debug))]
pub enum DefmtVariant {
    Bitfields,
    Fields,
}

#[derive(Default)]
#[cfg_attr(feature = "extra-traits", derive(Debug))]
struct BitfieldAttributes {
    pub base_type: Option<Ident>,
    pub default_val: Option<DefaultVal>,
    pub forbid_overlaps: bool,
    pub debug_trait: bool,
    pub introspect: bool,
    pub defmt_trait: Option<DefmtTrait>,
}

impl BitfieldAttributes {
    fn parse(&mut self, meta: ParseNestedMeta, index: usize) -> Result<(), syn::Error> {
        if index == 0 {
            self.base_type = Some(meta.path.require_ident()?.clone());
            return Ok(());
        }
        if meta.path.is_ident("default") {
            let stream = &meta.input;

            // Try parsing either `:` or `=`
            if stream.parse::<Token![:]>().is_err() && stream.parse::<Token![=]>().is_err() {
                return Err(syn::Error::new(
                    meta.input.span(),
                    "Expected `:` or `=` after `default`",
                ));
            }
            let lit_int: Result<LitInt, syn::Error> = stream.parse();
            if let Ok(lit_int) = lit_int {
                self.default_val = Some(DefaultVal::Lit(lit_int));
                return Ok(());
            }
            let path: Result<Ident, syn::Error> = stream.parse();
            if let Ok(path) = path {
                self.default_val = Some(DefaultVal::Constant(path));
                return Ok(());
            }
            return Ok(());
        }
        if meta.path.is_ident("forbid_overlaps") {
            self.forbid_overlaps = true;
            return Ok(());
        }
        if meta.path.is_ident("debug") {
            self.debug_trait = true;
            return Ok(());
        }
        if meta.path.is_ident("introspect") {
            self.introspect = true;
            return Ok(());
        }
        let parse_feature_gate = |meta: ParseNestedMeta<'_>| -> Result<Option<String>, syn::Error> {
            let mut feature_gate = None;
            if meta.input.is_empty() {
                return Ok(feature_gate);
            }
            meta.parse_nested_meta(|meta| {
                if meta.path.is_ident("feature") {
                    let value = meta.value()?; // this parses the `=`
                    let s: LitStr = value.parse()?;
                    feature_gate = Some(s.value());
                    Ok(())
                } else {
                    Err(meta.error("unsupported attribute"))
                }
            })?;
            Ok(feature_gate)
        };
        if meta.path.is_ident("defmt_fields") {
            let feature_gate = parse_feature_gate(meta)?;
            self.defmt_trait = Some(DefmtTrait {
                variant: DefmtVariant::Fields,
                feature_gate,
            });
            return Ok(());
        }
        if meta.path.is_ident("defmt_bitfields") {
            let feature_gate = parse_feature_gate(meta)?;
            self.defmt_trait = Some(DefmtTrait {
                variant: DefmtVariant::Bitfields,
                feature_gate,
            });
            return Ok(());
        }
        Ok(())
    }
}

pub fn bitfield(args: TokenStream, input: TokenStream) -> TokenStream {
    let mut bitfield_attrs = BitfieldAttributes::default();
    if cfg!(feature = "introspect") {
        bitfield_attrs.introspect = true;
    }
    let mut index = 0;
    let bitfield_parser = syn::meta::parser(|meta| {
        let result = bitfield_attrs.parse(meta, index);
        index += 1;
        result
    });
    if args.is_empty() {
        return syn::Error::new(
            Span::call_site().into(),
            "bitfield! No arguments given, but need at least a base data type (e.g. 'bitfield(u32)')").to_compile_error().into();
    }
    parse_macro_input!(args with bitfield_parser);

    if bitfield_attrs.base_type.is_none() {
        panic!("bitfield!: First argument must be the base data type, e.g. 'bitfield(u32)'",);
    }
    let base_data_type = bitfield_attrs.base_type.as_ref().unwrap();
    // If an arbitrary-int is specified as a base-type, we only use that when exposing it
    // (e.g. through raw_value() and for bounds-checks). The actual raw_value field will be the next
    // larger integer field
    let base_data_size = match base_data_type.to_string().as_str() {
        "u8" => BaseDataSize::new(8),
        "u16" => BaseDataSize::new(16),
        "u32" => BaseDataSize::new(32),
        "u64" => BaseDataSize::new(64),
        "u128" => BaseDataSize::new(128),
        s if try_parse_arbitrary_int_type(s, false).is_some() => {
            BaseDataSize::new(try_parse_arbitrary_int_type(s, false).unwrap().0)
        }
        _ => {
            return syn::Error::new_spanned(
                base_data_type,
                format!("bitfield!: Supported values for base data type are u8, u16, u32, u64, u128. {} is invalid", base_data_type.to_string().as_str()),
            ).to_compile_error().into();
        }
    };
    let internal_base_data_type =
        syn::parse_str::<Type>(format!("u{}", base_data_size.internal).as_str())
            .unwrap_or_else(|_| panic!("bitfield!: Error parsing internal base data type"));

    let input = syn::parse_macro_input!(input as DeriveInput);
    let struct_name = &input.ident;
    let struct_vis = &input.vis;
    let struct_attrs = &input.attrs;

    let fields = match &input.data {
        Data::Struct(struct_data) => &struct_data.fields,
        _ => panic!("bitfield!: Must be used on struct"),
    };

    let field_definitions = match parsing::parse(fields, base_data_size) {
        Ok(definitions) => definitions,
        Err(token_stream) => return token_stream.into_compile_error().into(),
    };

    if bitfield_attrs.forbid_overlaps {
        if let Err(e) = check_for_overlaps(&field_definitions, &input) {
            return e.into_compile_error().into();
        }
    }
    let accessors = codegen::generate(
        &field_definitions,
        base_data_size,
        &internal_base_data_type,
        bitfield_attrs.introspect,
    );

    let (default_constructor, default_trait) = if let Some(default_value) =
        &bitfield_attrs.default_val
    {
        let default_value = default_value.to_token_stream();
        let constructor = {
            let comment = format!("An instance that uses the default value {}", default_value);
            let deprecated_warning = format!(
                "Use {}::Default (or {}::DEFAULT in const context) instead",
                struct_name, struct_name
            );

            let default_raw_value = if base_data_size.exposed == base_data_size.internal {
                quote! { const DEFAULT_RAW_VALUE: #base_data_type = #default_value; }
            } else {
                quote! { const DEFAULT_RAW_VALUE: #base_data_type = #base_data_type::new(#default_value); }
            };
            quote! {
                #default_raw_value

                #[doc = #comment]
                pub const DEFAULT: Self = Self::new_with_raw_value(Self::DEFAULT_RAW_VALUE);

                /// Creates a new instance of this struct using the default value
                #[deprecated(note = #deprecated_warning)]
                pub const fn new() -> Self {
                    Self::DEFAULT
                }
            }
        };

        let default_trait = quote! {
            impl Default for #struct_name {
                fn default() -> Self {
                    Self::DEFAULT
                }
            }
        };

        (constructor, default_trait)
    } else {
        (quote! {}, quote! {})
    };

    let mut debug_trait = TokenStream2::new();
    if bitfield_attrs.debug_trait {
        debug_trait = codegen::generate_debug_trait_impl(struct_name, &field_definitions);
    }

    let mut defmt_trait = TokenStream2::new();
    if bitfield_attrs.defmt_trait.is_some() {
        defmt_trait = codegen::generate_defmt_trait_impl(
            struct_name,
            &bitfield_attrs,
            &field_definitions,
            base_data_size,
        );
    }

    let (new_with_constructor, new_with_builder_chain) = codegen::make_builder(
        struct_name,
        bitfield_attrs.default_val.is_some(),
        struct_vis,
        base_data_type,
        base_data_size,
        &field_definitions,
    );

    let raw_value_unwrap = if base_data_size.exposed == base_data_size.internal {
        quote! { value }
    } else {
        quote! { value.value() }
    };

    let raw_value_wrap = if base_data_size.exposed == base_data_size.internal {
        quote! { self.raw_value }
    } else {
        // We use extract as that - unlike new() - never panics. This macro already guarantees that
        // the upper bits can't be set.
        let extract =
            syn::parse_str::<Type>(format!("extract_u{}", base_data_size.internal).as_str())
                .unwrap_or_else(|_| panic!("bitfield!: Error parsing one literal"));
        quote! { #base_data_type::#extract(self.raw_value, 0) }
    };

    let zero = if base_data_size.exposed == base_data_size.internal {
        quote! { 0 }
    } else {
        quote! { #base_data_type::new(0) }
    };

    let zero_comment = format!(
        "Creates a new instance with a raw value of 0. Equivalent to [`Self::new_with_raw_value({})`].",
        zero
    );

    // We split this out so that when rustc mentions that `new_with_raw_value` might be the intended
    // constructor when not encountering `build`, the span points at the type and not the attribute.
    let new_with_raw_value = quote_spanned!(struct_name.span() =>
        /// Creates a new instance of this bitfield with the given raw value.
        ///
        /// No checks are performed on the value, so it is possible to set bits that don't have any
        /// accessors specified.
        #[inline]
        pub const fn new_with_raw_value(value: #base_data_type) -> #struct_name {
            #struct_name {
                raw_value: #raw_value_unwrap
            }
        }
    );
    let expanded = quote! {
        #[derive(Copy, Clone)]
        #[repr(C)]
        #( #struct_attrs )*
        #struct_vis struct #struct_name {
            raw_value: #internal_base_data_type,
        }

        impl #struct_name {
            #[doc = #zero_comment]
            pub const ZERO: Self = Self::new_with_raw_value(#zero);

            #default_constructor
            /// Returns the underlying raw value of this bitfield
            #[inline]
            pub const fn raw_value(&self) -> #base_data_type {
                #raw_value_wrap
            }

            #new_with_raw_value

            #new_with_constructor

            #( #accessors )*
        }
        #default_trait

        #debug_trait

        #defmt_trait

        #( #new_with_builder_chain )*
    };
    TokenStream::from(expanded)
}

const fn mask_for_width_and_offset(width: usize, offset: usize) -> u128 {
    if width == 128 {
        return u128::MAX;
    }
    if width + offset > 128 {
        panic!("bitfield!: Internal error: width + offset > 128");
    }
    // The wrapping subtraction is important for the case where width is 128.
    // A shift with 128 would yield a value of 0, and a regular subtraction of 1 could panic.
    ((1_u128 << width) - 1) << offset
}

fn check_for_overlaps(
    field_definitions: &[FieldDefinition],
    input: &DeriveInput,
) -> syn::Result<()> {
    let mut current_bitmask: u128 = 0;

    for field in field_definitions {
        let mut check_and_update_bitmask = |offset: usize, width: usize| {
            // Create an all-ones bitmask for the given range, e.g. 0b1110 for range (1..=3).
            let mask = mask_for_width_and_offset(width, offset);
            if (current_bitmask & mask) != 0 {
                return Err(syn::Error::new_spanned(
                    input,
                    format!(
                        "bitfield!: Detected overlap of field `{}` with other field",
                        field.field_name
                    ),
                ));
            }
            current_bitmask |= mask;
            Ok(())
        };
        for range in &field.ranges {
            let width = range.end - range.start;
            match field.array {
                Some(info) => {
                    let mut current_offset = range.start;
                    for _ in 0..info.count {
                        check_and_update_bitmask(current_offset, width)?;
                        current_offset += info.indexed_stride;
                    }
                }
                None => {
                    check_and_update_bitmask(range.start, width)?;
                }
            }
        }
    }
    Ok(())
}

fn with_name(field_name: &Ident) -> Ident {
    // The field might have started with r#. If so, it was likely used for a keyword. This can be dropped here
    let field_name_without_prefix = {
        let s = field_name.to_string();
        if let Some(s) = s.strip_prefix("r#") {
            s.to_string()
        } else {
            s
        }
    };

    syn::parse_str::<Ident>(format!("with_{}", field_name_without_prefix).as_str())
        .unwrap_or_else(|_| panic!("bitfield!: Error creating setter name"))
}

fn setter_name(field_name: &Ident) -> Ident {
    // The field might have started with r#. If so, it was likely used for a keyword. This can be dropped here
    let field_name_without_prefix = {
        let s = field_name.to_string();
        if let Some(s) = s.strip_prefix("r#") {
            s.to_string()
        } else {
            s
        }
    };

    syn::parse_str::<Ident>(format!("set_{}", field_name_without_prefix).as_str())
        .unwrap_or_else(|_| panic!("bitfield!: Error creating setter name"))
}

fn mask_name(field_name: &Ident) -> Ident {
    // The field might have started with r#. If so, it was likely used for a keyword. This can be dropped here
    let field_name_without_prefix = {
        let s = field_name.to_string();
        if let Some(s) = s.strip_prefix("r#") {
            s.to_string()
        } else {
            s
        }
    };

    syn::parse_str::<Ident>(&format!("{field_name_without_prefix}_mask"))
        .unwrap_or_else(|_| panic!("bitfield!: Error creating mask name"))
}

fn const_name(field_name: &Ident, suffix: &str) -> Ident {
    // The field might have started with r#. If so, it was likely used for a keyword. This can be dropped here
    let field_name_without_prefix = {
        let s = field_name.to_string();
        if let Some(s) = s.strip_prefix("r#") {
            s.to_string()
        } else {
            s
        }
    };

    let name = format!("{field_name_without_prefix}_{suffix}")
        .to_uppercase()
        .to_string();

    syn::parse_str::<Ident>(&name)
        .unwrap_or_else(|_| panic!("bitfield!: Error creating {name} name"))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_mask_underflow() {
        assert_eq!(mask_for_width_and_offset(128, 0), u128::MAX);
    }

    #[test]
    fn test_mask_at_limit() {
        assert_eq!(mask_for_width_and_offset(127, 1), u128::MAX - 1);
    }

    #[test]
    #[should_panic]
    fn test_mask_error() {
        // This panics.
        assert_eq!(mask_for_width_and_offset(127, 2), u128::MAX);
    }

    #[test]
    fn test_mask_0() {
        assert_eq!(mask_for_width_and_offset(3, 1), 0b1110);
    }

    #[test]
    fn test_mask_1() {
        assert_eq!(mask_for_width_and_offset(0, 0), 0);
    }

    #[test]
    fn test_mask_2() {
        assert_eq!(mask_for_width_and_offset(1, 0), 1);
    }

    #[test]
    fn test_mask_3() {
        assert_eq!(mask_for_width_and_offset(2, 0), 0b11);
    }
}
