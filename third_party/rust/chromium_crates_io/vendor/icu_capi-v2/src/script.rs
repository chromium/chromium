// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::unstable::properties_enums::ffi::Script;
    use crate::unstable::properties_iter::ffi::CodePointRangeIterator;
    use crate::unstable::properties_sets::ffi::CodePointSetData;
    #[cfg(feature = "buffer_provider")]
    use crate::unstable::{errors::ffi::DataError, provider::ffi::DataProvider};

    #[diplomat::opaque]
    /// An ICU4X `ScriptWithExtensions` map object, capable of holding a map of codepoints to scriptextensions values
    #[diplomat::rust_link(icu::properties::script::ScriptWithExtensions, Struct)]
    pub struct ScriptWithExtensions(pub icu_properties::script::ScriptWithExtensions);

    #[diplomat::opaque]
    /// A slightly faster `ScriptWithExtensions` object
    #[diplomat::rust_link(icu::properties::script::ScriptWithExtensionsBorrowed, Struct)]
    #[diplomat::attr(demo_gen, disable)] // TODO needs custom page
    pub struct ScriptWithExtensionsBorrowed<'a>(
        pub icu_properties::script::ScriptWithExtensionsBorrowed<'a>,
    );
    #[diplomat::opaque]
    /// An object that represents the `Script_Extensions` property for a single character
    #[diplomat::rust_link(icu::properties::script::ScriptExtensionsSet, Struct)]
    #[diplomat::attr(demo_gen, disable)] // TODO needs custom page
    pub struct ScriptExtensionsSet<'a>(pub icu_properties::script::ScriptExtensionsSet<'a>);

    impl ScriptWithExtensions {
        /// Create a map for the `Script`/`Script_Extensions` properties, using compiled data.
        #[diplomat::rust_link(icu::properties::script::ScriptWithExtensions::new, FnInStruct)]
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::new,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Box<ScriptWithExtensions> {
            Box::new(ScriptWithExtensions(
                icu_properties::script::ScriptWithExtensions::new().static_to_owned(),
            ))
        }

        /// Create a map for the `Script`/`Script_Extensions` properties, using compiled data.
        #[diplomat::rust_link(icu::properties::script::ScriptWithExtensions::new, FnInStruct)]
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::new,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<ScriptWithExtensions>, DataError> {
            Ok(Box::new(ScriptWithExtensions(
                icu_properties::script::ScriptWithExtensions::try_new_with_buffer_provider(
                    provider.get()?,
                )?,
            )))
        }

        /// Get the Script property value for a code point
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_val,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_val32,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(any(dart, kotlin), disable)]
        #[diplomat::attr(*, rename = "get_script_val")]
        #[diplomat::abi_rename = "icu4x_ScriptWithExtensions_get_script_val_mv1"]
        pub fn get_script_val_raw(&self, ch: DiplomatChar) -> u16 {
            self.get_script_val(ch).to_integer_value()
        }

        /// Get the Script property value for a code point
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_val,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_val32,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(not(any(dart, kotlin)), disable)]
        #[diplomat::abi_rename = "icu4x_ScriptWithExtensions_get_script_val_mv2"]
        pub fn get_script_val(&self, ch: DiplomatChar) -> Script {
            Script::from(self.0.as_borrowed().get_script_val32(ch))
        }

        /// Check if the `Script_Extensions` property of the given code point covers the given script
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::has_script,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::has_script32,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(any(dart, kotlin), disable)]
        #[diplomat::attr(*, rename = "has_script")]
        #[diplomat::abi_rename = "icu4x_ScriptWithExtensions_has_script_mv1"]
        pub fn has_script_raw(&self, ch: DiplomatChar, script: u16) -> bool {
            self.has_script(
                ch,
                Script::from_integer_value(script).unwrap_or(Script::Unknown),
            )
        }

        /// Check if the `Script_Extensions` property of the given code point covers the given script
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::has_script,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::has_script32,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(not(any(dart, kotlin)), disable)]
        #[diplomat::abi_rename = "icu4x_ScriptWithExtensions_has_script_mv2"]
        pub fn has_script(&self, ch: DiplomatChar, script: Script) -> bool {
            self.0.as_borrowed().has_script32(ch, script.into())
        }

        /// Borrow this object for a slightly faster variant with more operations
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensions::as_borrowed,
            FnInStruct
        )]
        #[diplomat::attr(auto, getter)]
        pub fn as_borrowed<'a>(&'a self) -> Box<ScriptWithExtensionsBorrowed<'a>> {
            Box::new(ScriptWithExtensionsBorrowed(self.0.as_borrowed()))
        }

        /// Get a list of ranges of code points that contain this script in their `Script_Extensions` values
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_extensions_ranges,
            FnInStruct
        )]
        #[diplomat::attr(any(dart, kotlin), disable)]
        #[diplomat::attr(*, rename = "iter_ranges_for_script")]
        #[diplomat::abi_rename = "icu4x_ScriptWithExtensions_iter_ranges_for_script_mv1"]
        pub fn iter_ranges_for_script_raw<'a>(
            &'a self,
            script: u16,
        ) -> Box<CodePointRangeIterator<'a>> {
            self.iter_ranges_for_script(
                Script::from_integer_value(script).unwrap_or(Script::Unknown),
            )
        }

        /// Get a list of ranges of code points that contain this script in their `Script_Extensions` values
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_extensions_ranges,
            FnInStruct
        )]
        #[diplomat::attr(not(any(dart, kotlin)), disable)]
        #[diplomat::abi_rename = "icu4x_ScriptWithExtensions_iter_ranges_for_script_mv2"]
        pub fn iter_ranges_for_script<'a>(
            &'a self,
            script: Script,
        ) -> Box<CodePointRangeIterator<'a>> {
            Box::new(CodePointRangeIterator(Box::new(
                self.0
                    .as_borrowed()
                    .get_script_extensions_ranges(script.into()),
            )))
        }
    }

    impl<'a> ScriptWithExtensionsBorrowed<'a> {
        /// Get the Script property value for a code point
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_val,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_val32,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(any(dart, kotlin), disable)]
        #[diplomat::attr(*, rename = "get_script_val")]
        #[diplomat::abi_rename = "icu4x_ScriptWithExtensionsBorrowed_get_script_val_mv1"]
        pub fn get_script_val_raw(&self, ch: DiplomatChar) -> u16 {
            self.get_script_val(ch).to_integer_value()
        }
        /// Get the Script property value for a code point
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_val,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_val32,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(not(any(dart, kotlin)), disable)]
        #[diplomat::abi_rename = "icu4x_ScriptWithExtensionsBorrowed_get_script_val_mv2"]
        pub fn get_script_val(&self, ch: DiplomatChar) -> Script {
            Script::from(self.0.get_script_val32(ch))
        }
        /// Get the Script property value for a code point
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_extensions_val,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_extensions_val32,
            FnInStruct,
            hidden
        )]
        pub fn get_script_extensions_val(&self, ch: DiplomatChar) -> Box<ScriptExtensionsSet<'a>> {
            Box::new(ScriptExtensionsSet(self.0.get_script_extensions_val32(ch)))
        }
        /// Check if the `Script_Extensions` property of the given code point covers the given script
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::has_script,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::has_script32,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(any(dart, kotlin), disable)]
        #[diplomat::attr(*, rename = "has_script")]
        #[diplomat::abi_rename = "icu4x_ScriptWithExtensionsBorrowed_has_script_mv1"]
        pub fn has_script_raw(&self, ch: DiplomatChar, script: u16) -> bool {
            self.has_script(
                ch,
                Script::from_integer_value(script).unwrap_or(Script::Unknown),
            )
        }
        /// Check if the `Script_Extensions` property of the given code point covers the given script
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::has_script,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::has_script32,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(not(any(dart, kotlin)), disable)]
        #[diplomat::abi_rename = "icu4x_ScriptWithExtensionsBorrowed_has_script_mv2"]
        pub fn has_script(&self, ch: DiplomatChar, script: Script) -> bool {
            self.0.has_script32(ch, script.into())
        }

        /// Build the `CodePointSetData` corresponding to a codepoints matching a particular script
        /// in their `Script_Extensions`
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_extensions_set,
            FnInStruct
        )]
        #[diplomat::attr(any(dart, kotlin), disable)]
        #[diplomat::attr(*, rename = "get_script_extensions_set")]
        #[diplomat::abi_rename = "icu4x_ScriptWithExtensionsBorrowed_get_script_extensions_set_mv1"]
        pub fn get_script_extensions_set_raw(&self, script: u16) -> Box<CodePointSetData> {
            self.get_script_extensions_set(
                Script::from_integer_value(script).unwrap_or(Script::Unknown),
            )
        }

        /// Build the `CodePointSetData` corresponding to a codepoints matching a particular script
        /// in their `Script_Extensions`
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_extensions_set,
            FnInStruct
        )]
        #[diplomat::attr(not(any(dart, kotlin)), disable)]
        #[diplomat::abi_rename = "icu4x_ScriptWithExtensionsBorrowed_get_script_extensions_set_mv2"]
        pub fn get_script_extensions_set(&self, script: Script) -> Box<CodePointSetData> {
            let list = self.0.get_script_extensions_set(script.into()).into_owned();
            let set = icu_properties::CodePointSetData::from_code_point_inversion_list(list);
            Box::new(CodePointSetData(set))
        }
    }
    impl<'a> ScriptExtensionsSet<'a> {
        /// Check if the `Script_Extensions` property of the given code point covers the given script
        #[diplomat::rust_link(icu::properties::script::ScriptExtensionsSet::contains, FnInStruct)]
        #[diplomat::attr(any(dart, kotlin), disable)]
        #[diplomat::attr(*, rename = "contains")]
        #[diplomat::abi_rename = "icu4x_ScriptExtensionsSet_contains_mv1"]
        pub fn contains_raw(&self, script: u16) -> bool {
            self.contains(Script::from_integer_value(script).unwrap_or(Script::Unknown))
        }
        /// Check if the `Script_Extensions` property of the given code point covers the given script
        #[diplomat::rust_link(icu::properties::script::ScriptExtensionsSet::contains, FnInStruct)]
        #[diplomat::attr(not(any(dart, kotlin)), disable)]
        #[diplomat::abi_rename = "icu4x_ScriptExtensionsSet_contains_mv2"]
        pub fn contains(&self, script: Script) -> bool {
            self.0.contains(&script.into())
        }

        /// Get the number of scripts contained in here
        #[diplomat::rust_link(icu::properties::script::ScriptExtensionsSet::iter, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn count(&self) -> usize {
            self.0.array_len()
        }

        /// Get script at index
        #[diplomat::rust_link(icu::properties::script::ScriptExtensionsSet::iter, FnInStruct)]
        #[diplomat::attr(any(dart, kotlin), disable)]
        #[diplomat::attr(*, rename = "script_at")]
        #[diplomat::abi_rename = "icu4x_ScriptExtensionsSet_script_at_mv1"]
        pub fn script_at_raw(&self, index: usize) -> Option<u16> {
            self.script_at(index).map(|s| s.to_integer_value())
        }

        /// Get script at index
        #[diplomat::rust_link(icu::properties::script::ScriptExtensionsSet::iter, FnInStruct)]
        #[diplomat::attr(not(any(dart, kotlin)), disable)]
        #[diplomat::abi_rename = "icu4x_ScriptExtensionsSet_script_at_mv2"]
        pub fn script_at(&self, index: usize) -> Option<Script> {
            self.0.array_get(index).map(Into::into)
        }
    }
}
