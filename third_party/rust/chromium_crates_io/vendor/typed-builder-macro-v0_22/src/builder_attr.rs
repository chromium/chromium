use proc_macro2::TokenStream;
use quote::{quote, ToTokens};
use syn::parse::Error;

use crate::field_info::FieldBuilderAttr;
use crate::mutator::Mutator;
use crate::util::{path_to_single_string, ApplyMeta, AttrArg};

#[derive(Debug, Default, Clone)]
pub struct CommonDeclarationSettings {
    pub vis: Option<syn::Visibility>,
    pub name: Option<syn::Expr>,
    pub doc: Option<syn::Expr>,
}

impl ApplyMeta for CommonDeclarationSettings {
    fn apply_meta(&mut self, expr: AttrArg) -> Result<(), Error> {
        match expr.name().to_string().as_str() {
            "vis" => {
                let expr_str = expr.key_value()?.parse_value::<syn::LitStr>()?.value();
                self.vis = Some(syn::parse_str(&expr_str)?);
                Ok(())
            }
            "name" => {
                self.name = Some(expr.key_value()?.parse_value()?);
                Ok(())
            }
            "doc" => {
                self.doc = Some(expr.key_value()?.parse_value()?);
                Ok(())
            }
            _ => Err(Error::new_spanned(
                expr.name(),
                format!("Unknown parameter {:?}", expr.name().to_string()),
            )),
        }
    }
}

impl CommonDeclarationSettings {
    pub fn get_name(&self) -> Option<TokenStream> {
        self.name.as_ref().map(|name| name.to_token_stream())
    }

    pub fn get_doc_or(&self, gen_doc: impl FnOnce() -> String) -> TokenStream {
        if let Some(ref doc) = self.doc {
            quote!(#[doc = #doc])
        } else {
            let doc = gen_doc();
            quote!(#[doc = #doc])
        }
    }
}

/// Setting of the `into` argument.
#[derive(Debug, Clone)]
pub enum IntoSetting {
    /// Do not run any conversion on the built value.
    NoConversion,
    /// Convert the build value into the generic parameter passed to the `build` method.
    GenericConversion,
    /// Convert the build value into a specific type specified in the attribute.
    TypeConversionToSpecificType(syn::TypePath),
}

impl Default for IntoSetting {
    fn default() -> Self {
        Self::NoConversion
    }
}

#[derive(Debug, Default, Clone)]
pub struct BuildMethodSettings {
    pub common: CommonDeclarationSettings,

    /// Whether to convert the built type into another while finishing the build.
    pub into: IntoSetting,
}

impl ApplyMeta for BuildMethodSettings {
    fn apply_meta(&mut self, expr: AttrArg) -> Result<(), Error> {
        match expr.name().to_string().as_str() {
            "into" => match expr {
                AttrArg::Flag(_) => {
                    self.into = IntoSetting::GenericConversion;
                    Ok(())
                }
                AttrArg::KeyValue(key_value) => {
                    let type_path = key_value.parse_value::<syn::TypePath>()?;
                    self.into = IntoSetting::TypeConversionToSpecificType(type_path);
                    Ok(())
                }
                _ => Err(expr.incorrect_type()),
            },
            _ => self.common.apply_meta(expr),
        }
    }
}

#[derive(Debug)]
pub struct TypeBuilderAttr<'a> {
    /// Whether to show docs for the `TypeBuilder` type (rather than hiding them).
    pub doc: bool,

    /// Customize builder method, ex. visibility, name
    pub builder_method: CommonDeclarationSettings,

    /// Customize builder type, ex. visibility, name
    pub builder_type: CommonDeclarationSettings,

    /// Customize build method, ex. visibility, name
    pub build_method: BuildMethodSettings,

    pub field_defaults: FieldBuilderAttr<'a>,

    pub crate_module_path: syn::Path,

    /// Functions that are able to mutate fields in the builder that are already set
    pub mutators: Vec<Mutator>,
}

impl Default for TypeBuilderAttr<'_> {
    fn default() -> Self {
        Self {
            doc: Default::default(),
            builder_method: Default::default(),
            builder_type: Default::default(),
            build_method: Default::default(),
            field_defaults: Default::default(),
            crate_module_path: syn::parse_quote!(::typed_builder),
            mutators: Default::default(),
        }
    }
}

impl<'a> TypeBuilderAttr<'a> {
    pub fn new(attrs: &[syn::Attribute]) -> Result<Self, Error> {
        let mut result = Self::default();

        for attr in attrs {
            let list = match &attr.meta {
                syn::Meta::List(list) => {
                    if path_to_single_string(&list.path).as_deref() != Some("builder") {
                        continue;
                    }

                    list
                }
                _ => continue,
            };

            result.apply_subsections(list)?;
        }

        if result.builder_type.doc.is_some() || result.build_method.common.doc.is_some() {
            result.doc = true;
        }

        Ok(result)
    }
}

impl ApplyMeta for TypeBuilderAttr<'_> {
    fn apply_meta(&mut self, expr: AttrArg) -> Result<(), Error> {
        match expr.name().to_string().as_str() {
            "crate_module_path" => {
                let crate_module_path = expr.key_value()?.parse_value::<syn::ExprPath>()?;
                self.crate_module_path = crate_module_path.path;
                Ok(())
            }
            "builder_method_doc" => Err(Error::new_spanned(
                expr.name(),
                "`builder_method_doc` is deprecated - use `builder_method(doc = \"...\")`",
            )),
            "builder_type_doc" => Err(Error::new_spanned(
                expr.name(),
                "`builder_typemethod_doc` is deprecated - use `builder_type(doc = \"...\")`",
            )),
            "build_method_doc" => Err(Error::new_spanned(
                expr.name(),
                "`build_method_doc` is deprecated - use `build_method(doc = \"...\")`",
            )),
            "doc" => {
                expr.flag()?;
                self.doc = true;
                Ok(())
            }
            "mutators" => {
                self.mutators.extend(expr.sub_attr()?.undelimited()?);
                Ok(())
            }
            "field_defaults" => self.field_defaults.apply_sub_attr(expr.sub_attr()?),
            "builder_method" => self.builder_method.apply_sub_attr(expr.sub_attr()?),
            "builder_type" => self.builder_type.apply_sub_attr(expr.sub_attr()?),
            "build_method" => self.build_method.apply_sub_attr(expr.sub_attr()?),
            _ => Err(Error::new_spanned(
                expr.name(),
                format!("Unknown parameter {:?}", expr.name().to_string()),
            )),
        }
    }
}
