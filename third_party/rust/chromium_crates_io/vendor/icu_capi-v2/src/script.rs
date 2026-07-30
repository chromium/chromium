// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;

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
        pub fn get_script_val(&self, ch: DiplomatChar) -> u16 {
            self.0.as_borrowed().get_script_val32(ch).to_icu4c_value()
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
        pub fn has_script(&self, ch: DiplomatChar, script: u16) -> bool {
            self.0
                .as_borrowed()
                .has_script32(ch, icu_properties::props::Script::from_icu4c_value(script))
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
        pub fn iter_ranges_for_script<'a>(
            &'a self,
            script: u16,
        ) -> Box<CodePointRangeIterator<'a>> {
            Box::new(CodePointRangeIterator(Box::new(
                self.0.as_borrowed().get_script_extensions_ranges(
                    icu_properties::props::Script::from_icu4c_value(script),
                ),
            )))
        }
    }

    impl<'a> ScriptWithExtensionsBorrowed<'a> {
        /// Get the Script property value for a code point
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_val,
            FnInStruct
        )]
        /// Get the Script property value for a code point
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_val32,
            FnInStruct,
            hidden
        )]
        pub fn get_script_val(&self, ch: DiplomatChar) -> u16 {
            self.0.get_script_val32(ch).to_icu4c_value()
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
        pub fn has_script(&self, ch: DiplomatChar, script: u16) -> bool {
            self.0
                .has_script32(ch, icu_properties::props::Script::from_icu4c_value(script))
        }

        /// Build the `CodePointSetData` corresponding to a codepoints matching a particular script
        /// in their `Script_Extensions`
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_extensions_set,
            FnInStruct
        )]
        pub fn get_script_extensions_set(&self, script: u16) -> Box<CodePointSetData> {
            let list = self
                .0
                .get_script_extensions_set(icu_properties::props::Script::from_icu4c_value(script))
                .into_owned();
            let set = icu_properties::CodePointSetData::from_code_point_inversion_list(list);
            Box::new(CodePointSetData(set))
        }
    }
    impl<'a> ScriptExtensionsSet<'a> {
        /// Check if the `Script_Extensions` property of the given code point covers the given script
        #[diplomat::rust_link(icu::properties::script::ScriptExtensionsSet::contains, FnInStruct)]
        pub fn contains(&self, script: u16) -> bool {
            self.0
                .contains(&icu_properties::props::Script::from_icu4c_value(script))
        }

        /// Get the number of scripts contained in here
        #[diplomat::rust_link(icu::properties::script::ScriptExtensionsSet::iter, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn count(&self) -> usize {
            self.0.array_len()
        }

        /// Get script at index
        #[diplomat::rust_link(icu::properties::script::ScriptExtensionsSet::iter, FnInStruct)]
        pub fn script_at(&self, index: usize) -> Option<u16> {
            self.0.array_get(index).map(|x| x.to_icu4c_value())
        }
    }
}
