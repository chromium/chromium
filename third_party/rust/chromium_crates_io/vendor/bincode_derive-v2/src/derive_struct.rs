use crate::attribute::{ContainerAttributes, FieldAttributes};
use virtue::prelude::*;

pub(crate) struct DeriveStruct {
    pub fields: Option<Fields>,
    pub attributes: ContainerAttributes,
}

impl DeriveStruct {
    pub fn generate_encode(self, generator: &mut Generator) -> Result<()> {
        let crate_name = &self.attributes.crate_name;
        generator
            .impl_for(format!("{}::Encode", crate_name))
            .modify_generic_constraints(|generics, where_constraints| {
                if let Some((bounds, lit)) =
                    (self.attributes.encode_bounds.as_ref()).or(self.attributes.bounds.as_ref())
                {
                    where_constraints.clear();
                    where_constraints
                        .push_parsed_constraint(bounds)
                        .map_err(|e| e.with_span(lit.span()))?;
                } else {
                    for g in generics.iter_generics() {
                        where_constraints
                            .push_constraint(g, format!("{}::Encode", crate_name))
                            .unwrap();
                    }
                }
                Ok(())
            })?
            .generate_fn("encode")
            .with_generic_deps("__E", [format!("{}::enc::Encoder", crate_name)])
            .with_self_arg(virtue::generate::FnSelfArg::RefSelf)
            .with_arg("encoder", "&mut __E")
            .with_return_type(format!(
                "core::result::Result<(), {}::error::EncodeError>",
                crate_name
            ))
            .body(|fn_body| {
                if let Some(fields) = self.fields.as_ref() {
                    for field in fields.names() {
                        let attributes = field
                            .attributes()
                            .get_attribute::<FieldAttributes>()?
                            .unwrap_or_default();
                        if attributes.with_serde {
                            fn_body.push_parsed(format!(
                                "{0}::Encode::encode(&{0}::serde::Compat(&self.{1}), encoder)?;",
                                crate_name, field
                            ))?;
                        } else {
                            fn_body.push_parsed(format!(
                                "{}::Encode::encode(&self.{}, encoder)?;",
                                crate_name, field
                            ))?;
                        }
                    }
                }
                fn_body.push_parsed("core::result::Result::Ok(())")?;
                Ok(())
            })?;
        Ok(())
    }

    pub fn generate_decode(self, generator: &mut Generator) -> Result<()> {
        // Remember to keep this mostly in sync with generate_borrow_decode
        let crate_name = &self.attributes.crate_name;
        let decode_context = if let Some((decode_context, _)) = &self.attributes.decode_context {
            decode_context.as_str()
        } else {
            "__Context"
        };

        let mut impl_for = generator.impl_for(format!("{}::Decode", crate_name));
        if self.attributes.decode_context.is_none() {
            impl_for = impl_for.with_impl_generics(["__Context"]);
        }

        impl_for
            .with_trait_generics([decode_context])
            .modify_generic_constraints(|generics, where_constraints| {
                if let Some((bounds, lit)) = (self.attributes.decode_bounds.as_ref()).or(self.attributes.bounds.as_ref()) {
                    where_constraints.clear();
                    where_constraints.push_parsed_constraint(bounds).map_err(|e| e.with_span(lit.span()))?;
                } else {
                    for g in generics.iter_generics() {
                        where_constraints.push_constraint(g, format!("{}::Decode<{}>", crate_name, decode_context)).unwrap();
                    }
                }
                Ok(())
            })?
            .generate_fn("decode")
            .with_generic_deps("__D", [format!("{}::de::Decoder<Context = {}>", crate_name, decode_context)])
            .with_arg("decoder", "&mut __D")
            .with_return_type(format!("core::result::Result<Self, {}::error::DecodeError>", crate_name))
            .body(|fn_body| {
                // Ok(Self {
                fn_body.push_parsed("core::result::Result::Ok")?;
                fn_body.group(Delimiter::Parenthesis, |ok_group| {
                    ok_group.ident_str("Self");
                    ok_group.group(Delimiter::Brace, |struct_body| {
                        // Fields
                        // {
                        //      a: bincode::Decode::decode(decoder)?,
                        //      b: bincode::Decode::decode(decoder)?,
                        //      ...
                        // }
                        if let Some(fields) = self.fields.as_ref() {
                            for field in fields.names() {
                                let attributes = field.attributes().get_attribute::<FieldAttributes>()?.unwrap_or_default();
                                if attributes.with_serde {
                                    struct_body
                                        .push_parsed(format!(
                                            "{1}: (<{0}::serde::Compat<_> as {0}::Decode::<{2}>>::decode(decoder)?).0,",
                                            crate_name,
                                            field,
                                            decode_context,
                                        ))?;
                                } else {
                                    struct_body
                                        .push_parsed(format!(
                                            "{1}: {0}::Decode::decode(decoder)?,",
                                            crate_name,
                                            field
                                        ))?;
                                }
                            }
                        }
                        Ok(())
                    })?;
                    Ok(())
                })?;
                Ok(())
            })?;
        self.generate_borrow_decode(generator)?;
        Ok(())
    }

    pub fn generate_borrow_decode(self, generator: &mut Generator) -> Result<()> {
        // Remember to keep this mostly in sync with generate_decode
        let crate_name = self.attributes.crate_name;

        let decode_context = if let Some((decode_context, _)) = &self.attributes.decode_context {
            decode_context.as_str()
        } else {
            "__Context"
        };

        let mut impl_for = generator
            .impl_for_with_lifetimes(format!("{}::BorrowDecode", crate_name), ["__de"])
            .with_trait_generics([decode_context]);
        if self.attributes.decode_context.is_none() {
            impl_for = impl_for.with_impl_generics(["__Context"]);
        }

        impl_for
            .modify_generic_constraints(|generics, where_constraints| {
                if let Some((bounds, lit)) = (self.attributes.borrow_decode_bounds.as_ref()).or(self.attributes.bounds.as_ref()) {
                    where_constraints.clear();
                    where_constraints.push_parsed_constraint(bounds).map_err(|e| e.with_span(lit.span()))?;
                } else {
                    for g in generics.iter_generics() {
                        where_constraints.push_constraint(g, format!("{}::de::BorrowDecode<'__de, {}>", crate_name, decode_context)).unwrap();
                    }
                    for lt in generics.iter_lifetimes() {
                        where_constraints.push_parsed_constraint(format!("'__de: '{}", lt.ident))?;
                    }
                }
                Ok(())
            })?
            .generate_fn("borrow_decode")
            .with_generic_deps("__D", [format!("{}::de::BorrowDecoder<'__de, Context = {}>", crate_name, decode_context)])
            .with_arg("decoder", "&mut __D")
            .with_return_type(format!("core::result::Result<Self, {}::error::DecodeError>", crate_name))
            .body(|fn_body| {
                // Ok(Self {
                fn_body.push_parsed("core::result::Result::Ok")?;
                fn_body.group(Delimiter::Parenthesis, |ok_group| {
                    ok_group.ident_str("Self");
                    ok_group.group(Delimiter::Brace, |struct_body| {
                        if let Some(fields) = self.fields.as_ref() {
                            for field in fields.names() {
                                let attributes = field.attributes().get_attribute::<FieldAttributes>()?.unwrap_or_default();
                                if attributes.with_serde {
                                    struct_body
                                        .push_parsed(format!(
                                            "{1}: (<{0}::serde::BorrowCompat<_> as {0}::BorrowDecode::<'_, {2}>>::borrow_decode(decoder)?).0,",
                                            crate_name,
                                            field,
                                            decode_context,
                                        ))?;
                                } else {
                                    struct_body
                                        .push_parsed(format!(
                                            "{1}: {0}::BorrowDecode::<'_, {2}>::borrow_decode(decoder)?,",
                                            crate_name,
                                            field,
                                            decode_context,
                                        ))?;
                                }
                            }
                        }
                        Ok(())
                    })?;
                    Ok(())
                })?;
                Ok(())
            })?;
        Ok(())
    }
}
