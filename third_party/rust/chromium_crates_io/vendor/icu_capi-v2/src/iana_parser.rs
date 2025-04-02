// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;
    use diplomat_runtime::DiplomatStr;

    use crate::timezone::ffi::TimeZone;
    #[cfg(feature = "buffer_provider")]
    use crate::{errors::ffi::DataError, provider::ffi::DataProvider};

    /// A mapper between IANA time zone identifiers and BCP-47 time zone identifiers.
    ///
    /// This mapper supports two-way mapping, but it is optimized for the case of IANA to BCP-47.
    /// It also supports normalizing and canonicalizing the IANA strings.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::time::zone::iana::IanaParser, Struct)]
    #[diplomat::rust_link(icu::time::zone::iana::IanaParser::as_borrowed, FnInStruct, hidden)]
    #[diplomat::rust_link(icu::time::zone::iana::IanaParserBorrowed, Struct, hidden)]
    #[diplomat::rust_link(icu::time::zone::iana::IanaParserBorrowed::new, FnInStruct, hidden)]
    pub struct IanaParser(pub icu_time::zone::iana::IanaParser);

    impl IanaParser {
        /// Create a new [`IanaParser`] using compiled data
        #[diplomat::rust_link(icu::time::zone::iana::IanaParser::new, FnInStruct)]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Box<IanaParser> {
            Box::new(IanaParser(
                icu_time::zone::iana::IanaParser::new().static_to_owned(),
            ))
        }

        /// Create a new [`IanaParser`] using a particular data source
        #[diplomat::rust_link(icu::time::zone::iana::IanaParser::new, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(provider: &DataProvider) -> Result<Box<IanaParser>, DataError> {
            Ok(Box::new(IanaParser(
                icu_time::zone::iana::IanaParser::try_new_with_buffer_provider(provider.get()?)?,
            )))
        }

        #[diplomat::rust_link(icu::time::zone::iana::IanaParserBorrowed::parse, FnInStruct)]
        #[diplomat::rust_link(
            icu::time::zone::iana::IanaParserBorrowed::parse_from_utf8,
            FnInStruct,
            hidden
        )]
        pub fn parse(&self, value: &DiplomatStr) -> Box<TimeZone> {
            Box::new(TimeZone(self.0.as_borrowed().parse_from_utf8(value)))
        }

        #[diplomat::rust_link(icu::time::zone::iana::IanaParserBorrowed::iter, FnInStruct)]
        pub fn iter<'a>(&'a self) -> Box<TimeZoneIterator<'a>> {
            Box::new(TimeZoneIterator(self.0.as_borrowed().iter()))
        }
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::time::zone::iana::TimeZoneIter, Struct)]
    pub struct TimeZoneIterator<'a>(icu_time::zone::iana::TimeZoneIter<'a>);

    impl<'a> TimeZoneIterator<'a> {
        #[diplomat::attr(auto, iterator)]
        #[diplomat::rust_link(icu::time::zone::iana::TimeZoneIter::next, FnInStruct)]
        pub fn next(&mut self) -> Option<Box<TimeZone>> {
            Some(Box::new(TimeZone(self.0.next()?)))
        }
    }

    /// A mapper between IANA time zone identifiers and BCP-47 time zone identifiers.
    ///
    /// This mapper supports two-way mapping, but it is optimized for the case of IANA to BCP-47.
    /// It also supports normalizing and canonicalizing the IANA strings.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::time::zone::iana::IanaParserExtended, Struct)]
    #[diplomat::rust_link(
        icu::time::zone::iana::IanaParserExtended::as_borrowed,
        FnInStruct,
        hidden
    )]
    #[diplomat::rust_link(icu::time::zone::iana::IanaParserExtendedBorrowed, Struct, hidden)]
    #[diplomat::rust_link(
        icu::time::zone::iana::IanaParserExtendedBorrowed::new,
        FnInStruct,
        hidden
    )]
    pub struct IanaParserExtended(
        pub icu_time::zone::iana::IanaParserExtended<icu_time::zone::iana::IanaParser>,
    );

    impl IanaParserExtended {
        /// Create a new [`IanaParserExtended`] using compiled data
        #[diplomat::rust_link(icu::time::zone::iana::IanaParserExtended::new, FnInStruct)]
        #[diplomat::rust_link(
            icu::time::zone::iana::IanaParserExtended::try_new_with_parser,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Box<IanaParserExtended> {
            Box::new(IanaParserExtended(
                icu_time::zone::iana::IanaParserExtended::new().static_to_owned(),
            ))
        }

        /// Create a new [`IanaParserExtended`] using a particular data source
        #[diplomat::rust_link(icu::time::zone::iana::IanaParserExtended::new, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<IanaParserExtended>, DataError> {
            Ok(Box::new(IanaParserExtended(
                icu_time::zone::iana::IanaParserExtended::try_new_with_buffer_provider(
                    provider.get()?,
                )?,
            )))
        }

        #[diplomat::rust_link(icu::time::zone::iana::IanaParserExtendedBorrowed::parse, FnInStruct)]
        #[diplomat::rust_link(
            icu::time::zone::iana::IanaParserExtendedBorrowed::parse_from_utf8,
            FnInStruct,
            hidden
        )]
        pub fn parse<'a>(&'a self, value: &DiplomatStr) -> TimeZoneAndCanonicalAndNormalized<'a> {
            let (time_zone_id, canonical, normalized) = self.0.as_borrowed().parse_from_utf8(value);
            TimeZoneAndCanonicalAndNormalized {
                time_zone: Box::new(TimeZone(time_zone_id)),
                canonical: canonical.into(),
                normalized: normalized.into(),
            }
        }

        #[diplomat::rust_link(icu::time::zone::iana::IanaParserExtendedBorrowed::iter, FnInStruct)]
        pub fn iter<'a>(&'a self) -> Box<TimeZoneAndCanonicalIterator<'a>> {
            Box::new(TimeZoneAndCanonicalIterator(self.0.as_borrowed().iter()))
        }

        #[diplomat::rust_link(
            icu::time::zone::iana::IanaParserExtendedBorrowed::iter_all,
            FnInStruct
        )]
        pub fn iter_all<'a>(&'a self) -> Box<TimeZoneAndCanonicalAndNormalizedIterator<'a>> {
            Box::new(TimeZoneAndCanonicalAndNormalizedIterator(
                self.0.as_borrowed().iter_all(),
            ))
        }
    }

    #[diplomat::out]
    pub struct TimeZoneAndCanonical<'a> {
        time_zone: Box<TimeZone>,
        canonical: DiplomatUtf8StrSlice<'a>,
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::time::zone::iana::TimeZoneAndCanonicalIter, Struct)]
    pub struct TimeZoneAndCanonicalIterator<'a>(icu_time::zone::iana::TimeZoneAndCanonicalIter<'a>);

    impl<'a> TimeZoneAndCanonicalIterator<'a> {
        #[diplomat::attr(auto, iterator)]
        #[diplomat::rust_link(icu::time::zone::iana::TimeZoneAndCanonicalIter::next, FnInStruct)]
        pub fn next(&mut self) -> Option<TimeZoneAndCanonical<'a>> {
            let (time_zone_id, canonical) = self.0.next()?;
            Some(TimeZoneAndCanonical {
                time_zone: Box::new(TimeZone(time_zone_id)),
                canonical: canonical.into(),
            })
        }
    }

    #[diplomat::out]
    pub struct TimeZoneAndCanonicalAndNormalized<'a> {
        time_zone: Box<TimeZone>,
        canonical: DiplomatUtf8StrSlice<'a>,
        normalized: DiplomatUtf8StrSlice<'a>,
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::time::zone::iana::TimeZoneAndCanonicalAndNormalizedIter, Struct)]
    pub struct TimeZoneAndCanonicalAndNormalizedIterator<'a>(
        icu_time::zone::iana::TimeZoneAndCanonicalAndNormalizedIter<'a>,
    );

    impl<'a> TimeZoneAndCanonicalAndNormalizedIterator<'a> {
        #[diplomat::attr(auto, iterator)]
        #[diplomat::rust_link(
            icu::time::zone::iana::TimeZoneAndCanonicalAndNormalizedIter::next,
            FnInStruct
        )]
        pub fn next(&mut self) -> Option<TimeZoneAndCanonicalAndNormalized<'a>> {
            let (time_zone_id, canonical, normalized) = self.0.next()?;
            Some(TimeZoneAndCanonicalAndNormalized {
                time_zone: Box::new(TimeZone(time_zone_id)),
                canonical: canonical.into(),
                normalized: normalized.into(),
            })
        }
    }
}
