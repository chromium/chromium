//! A public API for more fine-grained customization of bindgen behavior.

pub use crate::ir::analysis::DeriveTrait;
pub use crate::ir::derive::CanDerive as ImplementsTrait;
pub use crate::ir::enum_ty::{EnumVariantCustomBehavior, EnumVariantValue};
pub use crate::ir::int::IntKind;
use std::fmt;
use std::panic::UnwindSafe;

/// An enum to allow ignoring parsing of macros.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum MacroParsingBehavior {
    /// Ignore the macro, generating no code for it, or anything that depends on
    /// it.
    Ignore,
    /// The default behavior bindgen would have otherwise.
    Default,
}

impl Default for MacroParsingBehavior {
    fn default() -> Self {
        MacroParsingBehavior::Default
    }
}

/// A trait to allow configuring different kinds of types in different
/// situations.
pub trait ParseCallbacks: fmt::Debug + UnwindSafe {
    /// This function will be run on every macro that is identified.
    fn will_parse_macro(&self, _name: &str) -> MacroParsingBehavior {
        MacroParsingBehavior::Default
    }

    /// The integer kind an integer macro should have, given a name and the
    /// value of that macro, or `None` if you want the default to be chosen.
    fn int_macro(&self, _name: &str, _value: i64) -> Option<IntKind> {
        None
    }

    /// This will be run on every string macro. The callback cannot influence the further
    /// treatment of the macro, but may use the value to generate additional code or configuration.
    fn str_macro(&self, _name: &str, _value: &[u8]) {}

    /// This will be run on every function-like macro. The callback cannot
    /// influence the further treatment of the macro, but may use the value to
    /// generate additional code or configuration.
    ///
    /// The first parameter represents the name and argument list (including the
    /// parentheses) of the function-like macro. The second parameter represents
    /// the expansion of the macro as a sequence of tokens.
    fn func_macro(&self, _name: &str, _value: &[&[u8]]) {}

    /// This function should return whether, given an enum variant
    /// name, and value, this enum variant will forcibly be a constant.
    fn enum_variant_behavior(
        &self,
        _enum_name: Option<&str>,
        _original_variant_name: &str,
        _variant_value: EnumVariantValue,
    ) -> Option<EnumVariantCustomBehavior> {
        None
    }

    /// Allows to rename an enum variant, replacing `_original_variant_name`.
    fn enum_variant_name(
        &self,
        _enum_name: Option<&str>,
        _original_variant_name: &str,
        _variant_value: EnumVariantValue,
    ) -> Option<String> {
        None
    }

    /// Allows to rename an item, replacing `_original_item_name`.
    fn item_name(&self, _original_item_name: &str) -> Option<String> {
        None
    }

    /// This will be called on every file inclusion, with the full path of the included file.
    fn include_file(&self, _filename: &str) {}

    /// This will be called to determine whether a particular blocklisted type
    /// implements a trait or not. This will be used to implement traits on
    /// other types containing the blocklisted type.
    ///
    /// * `None`: use the default behavior
    /// * `Some(ImplementsTrait::Yes)`: `_name` implements `_derive_trait`
    /// * `Some(ImplementsTrait::Manually)`: any type including `_name` can't
    ///   derive `_derive_trait` but can implemented it manually
    /// * `Some(ImplementsTrait::No)`: `_name` doesn't implement `_derive_trait`
    fn blocklisted_type_implements_trait(
        &self,
        _name: &str,
        _derive_trait: DeriveTrait,
    ) -> Option<ImplementsTrait> {
        None
    }

    /// Provide a list of custom derive attributes.
    ///
    /// If no additional attributes are wanted, this function should return an
    /// empty `Vec`.
    fn add_derives(&self, _name: &str) -> Vec<String> {
        vec![]
    }
}
