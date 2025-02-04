// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;
    use writeable::Writeable;

    use crate::errors::ffi::DataError;
    use crate::provider::ffi::DataProvider;

    use tinystr::TinyAsciiStr;

    /// A mapper between IANA time zone identifiers and BCP-47 time zone identifiers.
    ///
    /// This mapper supports two-way mapping, but it is optimized for the case of IANA to BCP-47.
    /// It also supports normalizing and canonicalizing the IANA strings.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::timezone::TimeZoneIdMapper, Struct)]
    #[diplomat::rust_link(icu::timezone::TimeZoneIdMapper::as_borrowed, FnInStruct, hidden)]
    #[diplomat::rust_link(icu::timezone::TimeZoneIdMapperBorrowed, Struct, hidden)]
    #[diplomat::rust_link(icu::timezone::TimeZoneIdMapperBorrowed::new, FnInStruct, hidden)]
    #[diplomat::rust_link(icu::timezone::NormalizedIana, Struct, hidden)]
    pub struct TimeZoneIdMapper(pub icu_timezone::TimeZoneIdMapper);

    impl TimeZoneIdMapper {
        #[diplomat::rust_link(icu::timezone::TimeZoneIdMapper::new, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, constructor)]
        pub fn create(provider: &DataProvider) -> Result<Box<TimeZoneIdMapper>, DataError> {
            Ok(Box::new(TimeZoneIdMapper(call_constructor!(
                icu_timezone::TimeZoneIdMapper::new [r => Ok(r.static_to_owned())],
                icu_timezone::TimeZoneIdMapper::try_new_with_any_provider,
                icu_timezone::TimeZoneIdMapper::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        #[diplomat::rust_link(icu::timezone::TimeZoneIdMapperBorrowed::iana_to_bcp47, FnInStruct)]
        #[diplomat::rust_link(
            icu::timezone::TimeZoneIdMapperBorrowed::iana_bytes_to_bcp47,
            FnInStruct,
            hidden
        )]
        pub fn iana_to_bcp47(
            &self,
            value: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) {
            let handle = self.0.as_borrowed();
            let bcp47 = handle.iana_bytes_to_bcp47(value);
            let _infallible = bcp47.0.write_to(write);
        }

        #[diplomat::rust_link(icu::timezone::TimeZoneIdMapperBorrowed::normalize_iana, FnInStruct)]
        pub fn normalize_iana(
            &self,
            value: &str,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Option<()> {
            let handle = self.0.as_borrowed();
            let iana = handle.normalize_iana(value)?;
            let _infallible = iana.0.write_to(write);
            Some(())
        }

        #[diplomat::rust_link(
            icu::timezone::TimeZoneIdMapperBorrowed::canonicalize_iana,
            FnInStruct
        )]
        pub fn canonicalize_iana(
            &self,
            value: &str,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Option<()> {
            let handle = self.0.as_borrowed();
            let iana = handle.canonicalize_iana(value)?;
            let _infallible = iana.0.write_to(write);
            Some(())
        }

        #[diplomat::rust_link(
            icu::timezone::TimeZoneIdMapperBorrowed::find_canonical_iana_from_bcp47,
            FnInStruct
        )]
        pub fn find_canonical_iana_from_bcp47(
            &self,
            value: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Option<()> {
            let handle = self.0.as_borrowed();
            let iana = TinyAsciiStr::try_from_utf8(value).ok().and_then(|s| {
                handle.find_canonical_iana_from_bcp47(icu_timezone::TimeZoneBcp47Id(s))
            })?;
            let _infallible = iana.write_to(write);
            Some(())
        }
    }

    /// A mapper between IANA time zone identifiers and BCP-47 time zone identifiers.
    ///
    /// This mapper supports two-way mapping, but it is optimized for the case of IANA to BCP-47.
    /// It also supports normalizing and canonicalizing the IANA strings.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::timezone::TimeZoneIdMapperWithFastCanonicalization, Struct)]
    #[diplomat::rust_link(
        icu::timezone::TimeZoneIdMapperWithFastCanonicalization::as_borrowed,
        FnInStruct,
        hidden
    )]
    #[diplomat::rust_link(
        icu::timezone::TimeZoneIdMapperWithFastCanonicalization::inner,
        FnInStruct,
        hidden
    )]
    #[diplomat::rust_link(
        icu::timezone::TimeZoneIdMapperWithFastCanonicalizationBorrowed,
        Struct,
        hidden
    )]
    #[diplomat::rust_link(
        icu::timezone::TimeZoneIdMapperWithFastCanonicalizationBorrowed::inner,
        FnInStruct,
        hidden
    )]
    pub struct TimeZoneIdMapperWithFastCanonicalization(
        pub icu_timezone::TimeZoneIdMapperWithFastCanonicalization<icu_timezone::TimeZoneIdMapper>,
    );

    impl TimeZoneIdMapperWithFastCanonicalization {
        #[diplomat::rust_link(
            icu::timezone::TimeZoneIdMapperWithFastCanonicalization::new,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::timezone::TimeZoneIdMapperWithFastCanonicalizationBorrowed::new,
            FnInStruct
        )]
        #[diplomat::attr(supports = fallible_constructors, constructor)]
        pub fn create(
            provider: &DataProvider,
        ) -> Result<Box<TimeZoneIdMapperWithFastCanonicalization>, DataError> {
            Ok(Box::new(TimeZoneIdMapperWithFastCanonicalization(
                call_constructor!(
                    icu_timezone::TimeZoneIdMapperWithFastCanonicalization::new [r => Ok(r.static_to_owned())],
                    icu_timezone::TimeZoneIdMapperWithFastCanonicalization::try_new_with_any_provider,
                    icu_timezone::TimeZoneIdMapperWithFastCanonicalization::try_new_with_buffer_provider,
                    provider,
                )?,
            )))
        }

        #[diplomat::rust_link(
            icu::timezone::TimeZoneIdMapperWithFastCanonicalizationBorrowed::canonicalize_iana,
            FnInStruct
        )]
        pub fn canonicalize_iana(
            &self,
            value: &str,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Option<()> {
            let handle = self.0.as_borrowed();
            let iana = handle.canonicalize_iana(value)?;
            let _infallible = iana.0.write_to(write);
            Some(())
        }

        #[diplomat::rust_link(
            icu::timezone::TimeZoneIdMapperWithFastCanonicalizationBorrowed::canonical_iana_from_bcp47,
            FnInStruct
        )]
        pub fn canonical_iana_from_bcp47(
            &self,
            value: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Option<()> {
            let handle = self.0.as_borrowed();
            let iana = TinyAsciiStr::try_from_utf8(value)
                .ok()
                .map(icu_timezone::TimeZoneBcp47Id)
                .and_then(|t| handle.canonical_iana_from_bcp47(t))?;
            let _infallible = iana.write_to(write);
            Some(())
        }
    }
}
