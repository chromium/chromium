use crate::bitfield::{
    const_name, mask_name, setter_name, with_name, BaseDataSize, BitfieldAttributes, CustomType,
    DefmtVariant, FieldDefinition, BITCOUNT_BOOL,
};
use proc_macro2::{Ident, TokenStream as TokenStream2, TokenStream, TokenTree};
use quote::{quote, TokenStreamExt as _};
use std::ops::Range;
use std::str::FromStr;
use syn::{LitInt, Type, Visibility};

/// Performs the codegen for the bitfield.
///
/// # Arguments
/// * `field_definitions` - The field definitions, as reported by `super::parsing`
/// * `base_data_size` - The size of the bitfield (e.g. u32 for bitfield(u32))
/// * `internal_base_data_type` - A [`syn::ty::Type`] that represents the same base data type as
///   passed in via `base_data_size.internal`. This is a redundant argument to avoid recreating it.
/// * `introspect` - Whether to generate introspection constants
pub fn generate(
    field_definitions: &[FieldDefinition],
    base_data_size: BaseDataSize,
    internal_base_data_type: &Type,
    introspect: bool,
) -> Vec<TokenStream> {
    let one = syn::parse_str::<syn::LitInt>(format!("1u{}", base_data_size.internal).as_str())
        .unwrap_or_else(|_| panic!("bitfield!: Error parsing one literal"));
    let accessors: Vec<TokenStream2> = field_definitions.iter().map(|field_definition| {
        let total_number_bits = field_definition.ranges.iter().fold(0, |a, b| a + b.len());
        let field_name = &field_definition.field_name;
        let doc_comment = &field_definition.doc_comment;

        let introspect = if introspect {
            let bits_name = const_name(field_name, "BITS");
            let ranges_len = field_definition.ranges.len();

            let bits = if ranges_len == 1 {
                let start = field_definition.ranges[0].start;
                let end = field_definition.ranges[0].end-1;
                quote! {
                    #(#doc_comment)*
                    const #bits_name: core::ops::RangeInclusive<usize> = #start..=#end;
                }
            } else {
                let range_starts = field_definition.ranges.iter().map(|r| r.start);
                let range_ends = field_definition.ranges.iter().map(|r| r.end-1);
                quote! {
                    #(#doc_comment)*
                    const #bits_name: [core::ops::RangeInclusive<usize>; #ranges_len] = [#(#range_starts..=#range_ends),*];
                }
            };

            let mask_name = mask_name(field_name);
            let mask = setter_mask(&one, field_definition);

            if let Some((count, stride)) = field_definition.array {
                let count_name = const_name(field_name, "COUNT");
                let stride_name = const_name(field_name, "STRIDE");
                quote! {
                    #bits
                    #(#doc_comment)*
                    const #count_name: usize = #count;
                    #(#doc_comment)*
                    const #stride_name: usize = #stride;
                    #(#doc_comment)*
                    #[inline]
                    pub const fn #mask_name(index: usize) -> #internal_base_data_type {
                        assert!(index < #count);
                        (#mask) << (index * #stride)
                    }
                }
            } else {
                quote! {
                    #bits
                    #(#doc_comment)*
                    #[inline]
                    pub const fn #mask_name() -> #internal_base_data_type {
                        #mask
                    }
                }
            }
        } else {
            quote! {}
        };

        let getter =
            if let Some(getter_type) = field_definition.getter_type.as_ref() {
                // Main work: Shift and mask the bits into extracted_bits
                let extracted_bits = extracted_bits(&one, field_definition, base_data_size, total_number_bits);

                // We might need a CustomType (e.g. bitenum). Do that conversion
                let converted = match &field_definition.custom_type {
                    CustomType::No => extracted_bits,
                    CustomType::Yes(convert_type) => {
                        quote! {
                            let extracted_bits = #extracted_bits;
                            #convert_type::new_with_raw_value(extracted_bits)
                        }
                    }
                };

                if let Some(array) = field_definition.array {
                    let indexed_count = array.0;
                    quote! {
                        #(#doc_comment)*
                        #[inline]
                        pub const fn #field_name(&self, index: usize) -> #getter_type {
                            assert!(index < #indexed_count);
                            #converted
                        }
                    }
                } else {
                    quote! {
                        #(#doc_comment)*
                        #[inline]
                        pub const fn #field_name(&self) -> #getter_type {
                            #converted
                        }
                    }
                }
            } else {
                quote! {}
            };

        let setter = if let Some(setter_type) = field_definition.setter_type.as_ref() {
            let argument_converted =
                match field_definition.custom_type {
                    CustomType::No => {
                        if field_definition.use_regular_int {
                            // For signed types, we first have to convert to the unsigned type. Then up the base type
                            // (e.g. i16 would go: field_value as u16 as u64)
                            if let Some(unsigned_field_type) = &field_definition.unsigned_field_type {
                                quote! { field_value as #unsigned_field_type }
                            } else {
                                quote! { field_value }
                            }
                        } else {
                            // Once signed arbitrary-ints (e.g. i7) are a thing, we'll need to pay special attention to sign extension here
                            quote! { field_value.value() }
                        }
                    }
                    CustomType::Yes(_) => {
                        // Once signed bitenum or bitfield-base-data-types are a thing, we'll need to pay special attention to sign extension here
                        if field_definition.use_regular_int {
                            quote! { field_value.raw_value() }
                        } else {
                            quote! { field_value.raw_value().value() }
                        }
                    }
                };

            let new_raw_value = setter_new_raw_value(&one, &argument_converted, field_definition, base_data_size, internal_base_data_type);

            let setter_name = setter_name(field_name);
            let with_name = with_name(field_name);

            if let Some(array) = field_definition.array {
                let indexed_count = array.0;
                quote! {
                    #(#doc_comment)*
                    #[inline]
                    pub const fn #with_name(&self, index: usize, field_value: #setter_type) -> Self {
                        assert!(index < #indexed_count);
                        Self {
                            raw_value: #new_raw_value
                        }
                    }
                    #(#doc_comment)*
                    #[inline]
                    pub fn #setter_name(&mut self, index: usize, field_value: #setter_type) {
                        assert!(index < #indexed_count);
                        self.raw_value = #new_raw_value;
                    }
                }
            } else {
                quote! {
                    #(#doc_comment)*
                    #[inline]
                    pub const fn #with_name(&self, field_value: #setter_type) -> Self {
                        Self {
                            raw_value: #new_raw_value
                        }
                    }
                    #(#doc_comment)*
                    #[inline]
                    pub fn #setter_name(&mut self, field_value: #setter_type) {
                        self.raw_value = #new_raw_value;
                    }
                }
            }
        } else {
            quote! {}
        };

        quote! {
            #introspect
            #getter
            #setter
        }
    }).collect();

    accessors
}

/// If there are multiple ranges, this packs them together
fn getter_packed(
    field_definition: &FieldDefinition,
    array_shift: &TokenStream,
    one: &LitInt,
) -> TokenStream {
    let expressions = field_definition.ranges.iter().scan(0, |target_lowest_bit, range| {
        let lowest_bit = range.start;
        let number_of_bits = range.len();
        let shift_left = *target_lowest_bit;
        *target_lowest_bit += number_of_bits;
        Some(quote! {
           (((self.raw_value >> (#lowest_bit #array_shift)) & ((#one << #number_of_bits) - #one)) << #shift_left)
        })
    });
    quote! {
        (#(#expressions)|*)
    }
}

/// Returns an expression that is used in the getter, before any CustomType conversion.
/// This extracts the required bits (in the case of multiple ranges it will also recombine them)
fn extracted_bits(
    one: &LitInt,
    field_definition: &FieldDefinition,
    base_data_size: BaseDataSize,
    total_number_bits: usize,
) -> TokenStream {
    let indexed_stride = field_definition.array.map(|(_, stride)| stride);
    let array_shift = indexed_stride.map_or_else(
        || quote! {},
        |indexed_stride| quote! { + index * #indexed_stride },
    );

    // Special case: For bools, we can shift-left the mask and compare that
    if field_definition.field_type_size == BITCOUNT_BOOL {
        assert_eq!(field_definition.ranges.len(), 1);
        let lowest_bit = field_definition.ranges[0].start;
        return quote! { (self.raw_value & (#one << (#lowest_bit #array_shift))) != 0 };
    }

    // TODO: Would it be faster to combine the expressions above in the target type, instead of the
    // source type? That's likely faster if the source is larger than the number of bits the cpu
    // easily deals with
    if field_definition.use_regular_int {
        let primitive_type = &field_definition.primitive_type;
        // Special case: If there's one bitrange which covers the whole
        if field_definition.ranges.len() == 1
            && field_definition.ranges[0].len() == base_data_size.internal
        {
            // If the field is the whole size of the bitfield and that special type is a regular type,
            // we can't apply a mask as that would overflow. In this case we can just return the whole thing
            assert_eq!(field_definition.ranges[0].start, 0);
            quote! { self.raw_value as #primitive_type }
        } else {
            let packed = getter_packed(field_definition, &array_shift, one);
            quote! {  #packed as #primitive_type }
        }
    } else {
        let custom_type =
            TokenStream2::from_str(format!("arbitrary_int::u{}", total_number_bits).as_str())
                .unwrap();
        let extract =
            TokenStream2::from_str(format!("extract_u{}", base_data_size.internal).as_str())
                .unwrap();
        if field_definition.ranges.len() == 1 {
            // Very common case: We just want to extract a single range - we can use extract for that
            let lowest_bit = field_definition.ranges[0].start;
            quote! {
                #custom_type::#extract(self.raw_value, #lowest_bit #array_shift)
            }
        } else {
            // First, pack the various range together. Then, extract from that value without shifting
            let packed = getter_packed(field_definition, &array_shift, one);
            quote! {
                #custom_type::#extract(#packed, 0)
            }
        }
    }
}

fn setter_new_raw_value(
    one: &LitInt,
    argument_converted: &TokenStream,
    field_definition: &FieldDefinition,
    base_data_size: BaseDataSize,
    internal_base_data_type: &Type,
) -> TokenStream {
    if let Some(array) = field_definition.array {
        let indexed_stride = array.1;
        // bool?
        if field_definition.field_type_size_from_data_type == Some(BITCOUNT_BOOL) {
            assert_eq!(field_definition.ranges.len(), 1);
            let lowest_bit = field_definition.ranges[0].start;
            quote! {
                {
                    let effective_index = #lowest_bit + index * #indexed_stride;
                    if #argument_converted { self.raw_value | (#one << effective_index) } else { self.raw_value & !(#one << effective_index) }
                }
            }
        } else if field_definition.ranges.len() == 1 {
            let lowest_bit = field_definition.ranges[0].start;
            let number_of_bits = field_definition.ranges[0].len();
            quote! {
                {
                    let effective_index = #lowest_bit + index * #indexed_stride;
                    (self.raw_value & !(((#one << #number_of_bits) - #one) << effective_index)) | ((#argument_converted as #internal_base_data_type) << effective_index)
                }
            }
        } else {
            // Replace multiple ranges - We have to split up the incoming value and set them into multiple places
            let clear_mask = setter_mask(one, field_definition);
            let new_bits = setter_new_bits(one, field_definition);
            quote! {
                {
                    let temp = #argument_converted as #internal_base_data_type;
                    const MASK: #internal_base_data_type = #clear_mask;
                    self.raw_value & (!(MASK << (index * #indexed_stride))) | (#new_bits << (index * #indexed_stride))
                }
            }
        }
    } else if field_definition.field_type_size_from_data_type == Some(BITCOUNT_BOOL) {
        assert_eq!(field_definition.ranges.len(), 1);
        let lowest_bit = field_definition.ranges[0].start;
        quote! {
            if #argument_converted { self.raw_value | (#one << #lowest_bit) } else { self.raw_value & !(#one << #lowest_bit) }
        }
    } else if field_definition.ranges.len() == 1 {
        let lowest_bit = field_definition.ranges[0].start;
        let number_of_bits = field_definition.ranges[0].len();
        if number_of_bits == base_data_size.internal {
            // If the field is the whole size of the bitfield, we can't apply a mask
            // as that would overflow. However, we don't need to
            assert_eq!(lowest_bit, 0);
            quote! { #argument_converted as #internal_base_data_type }
        } else {
            // This is the common case: We're replacing a single range with a given value
            quote! {
                (self.raw_value & !(((#one << #number_of_bits) - #one) << #lowest_bit)) | ((#argument_converted as #internal_base_data_type) << #lowest_bit)
            }
        }
    } else {
        // Replace multiple ranges - We have to split up the incoming value and set them into multiple places
        let mask = setter_mask(one, field_definition);
        let new_bits = setter_new_bits(one, field_definition);
        quote! {
            {
                let temp = #argument_converted as #internal_base_data_type;
                const CLEAR_MASK: #internal_base_data_type = !(#mask);
                self.raw_value & CLEAR_MASK | #new_bits
            }
        }
    }
}

fn setter_new_bits(one: &LitInt, field_definition: &FieldDefinition) -> TokenStream {
    let new_bits_expressions =
        field_definition
            .ranges
            .iter()
            .scan(0, |target_lowest_bit, range| {
                let shift_right = *target_lowest_bit;
                *target_lowest_bit += range.len();
                let lowest_bit = range.start;
                let number_of_bits = range.len();
                Some(quote! {
                    (((temp >> #shift_right) & ((#one << #number_of_bits) - #one)) << #lowest_bit)
                })
            });

    quote! { (#(#new_bits_expressions)|*) }
}

fn setter_mask(one: &LitInt, field_definition: &FieldDefinition) -> TokenStream {
    let clear_mask_expressions = field_definition.ranges.iter().map(|range| {
        let lowest_bit = range.start;
        let number_of_bits = range.len();
        quote! {
            (((#one << #number_of_bits) - #one) << #lowest_bit)
        }
    });

    quote! { (#(#clear_mask_expressions)|*) }
}

/// Range definition are pretty flexible; it is possible for them to overlap with each other.
/// In the regular definition this is allowed, but in that case we won't make a builder
fn ranges_have_self_overlap(
    ranges: &[Range<usize>],
    array_stride: usize,
    array_length: usize,
) -> bool {
    let mut mask = 0;
    for i in 0..array_length {
        for range in ranges {
            let bits = ((1u128 << range.len()) - 1) << (range.start + i * array_stride);
            if bits & mask != 0 {
                return true;
            }
            mask |= bits;
        }
    }
    false
}

pub fn make_builder(
    struct_name: &Ident,
    has_default: bool,
    struct_vis: &Visibility,
    internal_base_data_type: &Type,
    base_data_type: &Ident,
    base_data_size: BaseDataSize,
    field_definitions: &[FieldDefinition],
) -> (TokenStream2, Vec<TokenStream2>) {
    let builder_struct_name =
        syn::parse_str::<Ident>(format!("Partial{}", struct_name).as_str()).unwrap();

    let mut running_mask = 0u128;
    let mut running_mask_token_tree = syn::parse_str::<TokenTree>("0x0").unwrap();
    let mut new_with_builder_chain: Vec<TokenStream2> =
        Vec::with_capacity(field_definitions.len() + 2);

    new_with_builder_chain.push(quote! {
        /// Partial builder struct
        #struct_vis struct #builder_struct_name<const MASK: #internal_base_data_type>(#struct_name);
    });

    for field_definition in field_definitions {
        if let Some(setter_type) = field_definition.setter_type.as_ref() {
            let field_name = &field_definition.field_name;
            let with_name = with_name(field_name);

            let (field_mask, value_transform, argument_type) = if let Some(array) =
                field_definition.array
            {
                // For arrays, we'll generate this code:
                // self.0
                //   .with_a(0, value[0])
                //   .with_a(1, value[1])
                //   .with_a(2, value[2])

                let array_count = array.0;
                let array_stride = array.1;
                if ranges_have_self_overlap(&field_definition.ranges, array_stride, array_count) {
                    return (quote! {}, Vec::new());
                }
                let mut mask = 0;
                let mut array_setters = Vec::with_capacity(array_count);
                for i in 0..array_count {
                    mask |= field_definition.ranges.iter().fold(0u128, |a, range| {
                        a | (((1u128 << range.len()) - 1) << (range.start + i * array_stride))
                    });

                    array_setters.push(quote! { .#with_name(#i, value[#i]) });
                }
                let value_transform = quote!(self.0 #( #array_setters )*);
                let array_type = quote! { [#setter_type; #array_count] };

                (mask, value_transform, array_type)
            } else {
                let mask = if field_definition.ranges.len() == 1 {
                    if field_definition.ranges[0].len() == 128 {
                        u128::MAX
                    } else {
                        ((1u128 << field_definition.ranges[0].len()) - 1)
                            << (field_definition.ranges[0].start)
                    }
                } else {
                    if ranges_have_self_overlap(&field_definition.ranges, 0, 0) {
                        return (quote! {}, Vec::new());
                    }
                    field_definition.ranges.iter().fold(0u128, |a, range| {
                        a | (((1u128 << range.len()) - 1) << (range.start))
                    })
                };

                (
                    mask,
                    quote! { self.0.#with_name(value)},
                    quote! { #setter_type },
                )
            };
            let previous_mask = running_mask;
            let previous_mask_token_tree = running_mask_token_tree;

            if (previous_mask & field_mask) != 0 {
                // Some fields are writable through multiple fields. This is not supported, so don't provide the constructor
                return (quote! {}, Vec::new());
            }

            running_mask = previous_mask | field_mask;
            running_mask_token_tree =
                syn::parse_str::<TokenTree>(format!("{:#x}", running_mask).as_str()).unwrap();
            let doc_comment = &field_definition.doc_comment;
            new_with_builder_chain.push(quote! {
                impl #builder_struct_name<#previous_mask_token_tree> {
                    #(#doc_comment)*
                    pub const fn #with_name(&self, value: #argument_type) -> #builder_struct_name<#running_mask_token_tree> {
                        #builder_struct_name(#value_transform)
                    }
                }
            });
        }
    }

    // The type has to either be complete OR it has to have a default value. Otherwise we can't do constructor syntax
    if (running_mask.count_ones() as usize != base_data_size.exposed) && !has_default {
        return (quote! {}, Vec::new());
    }

    new_with_builder_chain.push(quote! {
        impl #builder_struct_name<#running_mask_token_tree> {
            /// Builds the bitfield from the values passed into this builder
            pub const fn build(&self) -> #struct_name {
                self.0
            }
        }
    });

    let default = if has_default {
        quote! { #builder_struct_name(#struct_name::DEFAULT) }
    } else if base_data_size.exposed == base_data_size.internal {
        quote! { #builder_struct_name(#struct_name::new_with_raw_value(0)) }
    } else {
        quote! {
            const ZERO: #base_data_type = #base_data_type::new(0);
            #builder_struct_name(#struct_name::new_with_raw_value(ZERO))
        }
    };
    let result_new_with_constructor = quote! {
        /// Creates a builder for this bitfield which ensures that all writable fields are initialized
        pub const fn builder() -> #builder_struct_name<0> {
            #default
        }
    };
    (result_new_with_constructor, new_with_builder_chain)
}

pub fn generate_defmt_trait_impl(
    struct_name: &Ident,
    bitfield_attrs: &BitfieldAttributes,
    field_definitions: &[FieldDefinition],
    base_data_size: BaseDataSize,
) -> TokenStream2 {
    let mut defmt_trait = TokenStream2::new();

    if let Some(defmt_format) = &bitfield_attrs.defmt_trait {
        let mut feature_gate = TokenStream2::new();
        if let Some(feature_gate_str) = &defmt_format.feature_gate {
            feature_gate.extend(quote! {
                #[cfg(feature = #feature_gate_str)]
            });
        }
        match defmt_format.variant {
            DefmtVariant::Bitfields => {
                let format_string = {
                    let labels_result: Result<Vec<_>, syn::Error> = field_definitions
                        .iter()
                        .map(|field| match field.ranges.len() {
                            0 => Err(syn::Error::new(
                                field.field_name.span(),
                                "defmt_bitfields detected fields without a range",
                            )),
                            1 => {
                                let range = field.ranges.first().unwrap();
                                Ok(format!(
                                    "{}: {{0={}..{}}}",
                                    field.field_name, range.start, range.end
                                ))
                            }
                            _ => {
                                let items = field
                                    .ranges
                                    .iter()
                                    .map(|r| {
                                        format!(
                                            "{{0={}..{}}} ({}..={})",
                                            r.start,
                                            r.end,
                                            r.start,
                                            r.end - 1
                                        )
                                    })
                                    .collect::<Vec<_>>()
                                    .join(", ");
                                Ok(format!("{}: {}", field.field_name, items))
                            }
                        })
                        .collect();
                    if let Err(e) = labels_result {
                        return e.into_compile_error();
                    }
                    let labels = labels_result.unwrap().join(", ");
                    format!("{} {{{{ {} }}}}", struct_name, labels)
                };
                let raw_val_accessor = {
                    match base_data_size.exposed {
                        8 | 16 | 32 | 64 => {
                            quote! { self.raw_value() }
                        }
                        _ => match base_data_size.internal {
                            0..=7 => quote! { self.raw_value().as_u8() },
                            8..=15 => quote! { self.raw_value().as_u16() },
                            16..=31 => quote! { self.raw_value().as_u32() },
                            32..=63 => quote! { self.raw_value().as_u64() },
                            _ => panic!("Unsupported base data size for defmt_bitfields"),
                        },
                    }
                };
                defmt_trait.append_all(quote! {
                    #feature_gate
                    impl defmt::Format for #struct_name {
                        fn format(&self, fmt: defmt::Formatter) {
                            defmt::write!(
                                fmt,
                                #format_string,
                                #raw_val_accessor,
                            )
                        }
                    }
                });
            }
            DefmtVariant::Fields => {
                let format_string = {
                    let labels = field_definitions
                        .iter()
                        .map(|field| format!("{}: {{}}", field.field_name))
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{} {{{{ {} }}}}", struct_name, labels)
                };
                let defmt_fields: Vec<TokenStream2> = field_definitions
                    .iter()
                    .map(|field| {
                        let field_name = &field.field_name;
                        quote! { self.#field_name(), }
                    })
                    .collect();
                defmt_trait.append_all(quote! {
                    #feature_gate
                    impl defmt::Format for #struct_name {
                        fn format(&self, fmt: defmt::Formatter) {
                            defmt::write!(
                                fmt,
                                #format_string,
                                #(#defmt_fields)*
                            )
                        }
                    }
                });
            }
        }
    }
    defmt_trait
}
