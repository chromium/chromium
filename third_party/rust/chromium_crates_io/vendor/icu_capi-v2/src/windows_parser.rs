// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;
    use diplomat_runtime::DiplomatStr;

    use crate::unstable::timezone::ffi::TimeZone;
    #[cfg(feature = "buffer_provider")]
    use crate::unstable::{errors::ffi::DataError, provider::ffi::DataProvider};

    /// A mapper between Windows time zone identifiers and BCP-47 time zone identifiers.
    ///
    /// This mapper supports two-way mapping, but it is optimized for the case of Windows to BCP-47.
    /// It also supports normalizing and canonicalizing the Windows strings.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::time::zone::windows::WindowsParser, Struct)]
    #[diplomat::rust_link(icu::time::zone::windows::WindowsParserBorrowed, Struct, hidden)]
    #[diplomat::rust_link(
        icu::time::zone::windows::WindowsParserBorrowed::new,
        FnInStruct,
        hidden
    )]
    pub struct WindowsParser(pub icu_time::zone::windows::WindowsParser);

    impl WindowsParser {
        /// Create a new [`WindowsParser`] using compiled data
        #[diplomat::rust_link(icu::time::zone::windows::WindowsParser::new, FnInStruct)]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Box<WindowsParser> {
            Box::new(WindowsParser(
                icu_time::zone::windows::WindowsParser::new().static_to_owned(),
            ))
        }

        /// Create a new [`WindowsParser`] using a particular data source
        #[diplomat::rust_link(icu::time::zone::windows::WindowsParser::new, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<WindowsParser>, DataError> {
            Ok(Box::new(WindowsParser(
                icu_time::zone::windows::WindowsParser::try_new_with_buffer_provider(
                    provider.get()?,
                )?,
            )))
        }

        #[diplomat::rust_link(icu::time::zone::windows::WindowsParserBorrowed::parse, FnInStruct)]
        #[diplomat::rust_link(
            icu::time::zone::windows::WindowsParserBorrowed::parse_from_utf8,
            FnInStruct,
            hidden
        )]
        pub fn parse(&self, value: &DiplomatStr, region: &DiplomatStr) -> Option<Box<TimeZone>> {
            self.0
                .as_borrowed()
                .parse_from_utf8(
                    value,
                    Some(icu_locale_core::subtags::Region::try_from_utf8(region).ok()?),
                )
                .map(TimeZone)
                .map(Box::new)
        }
    }
}
