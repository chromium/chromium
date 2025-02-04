// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;
    use alloc::sync::Arc;
    use core::fmt::Write;

    use crate::errors::ffi::DataError;
    use crate::locale_core::ffi::Locale;
    use crate::provider::ffi::DataProvider;

    /// The various calendar types currently supported by [`Calendar`]
    #[diplomat::enum_convert(icu_calendar::AnyCalendarKind, needs_wildcard)]
    #[diplomat::rust_link(icu::calendar::AnyCalendarKind, Enum)]
    pub enum AnyCalendarKind {
        /// The kind of an Iso calendar
        Iso = 0,
        /// The kind of a Gregorian calendar
        Gregorian = 1,
        /// The kind of a Buddhist calendar
        Buddhist = 2,
        /// The kind of a Japanese calendar with modern eras
        Japanese = 3,
        /// The kind of a Japanese calendar with modern and historic eras
        JapaneseExtended = 4,
        /// The kind of an Ethiopian calendar, with Amete Mihret era
        Ethiopian = 5,
        /// The kind of an Ethiopian calendar, with Amete Alem era
        EthiopianAmeteAlem = 6,
        /// The kind of a Indian calendar
        Indian = 7,
        /// The kind of a Coptic calendar
        Coptic = 8,
        /// The kind of a Dangi calendar
        Dangi = 9,
        /// The kind of a Chinese calendar
        Chinese = 10,
        /// The kind of a Hebrew calendar
        Hebrew = 11,
        /// The kind of a Islamic civil calendar
        IslamicCivil = 12,
        /// The kind of a Islamic observational calendar
        IslamicObservational = 13,
        /// The kind of a Islamic tabular calendar
        IslamicTabular = 14,
        /// The kind of a Islamic Umm al-Qura calendar
        IslamicUmmAlQura = 15,
        /// The kind of a Persian calendar
        Persian = 16,
        /// The kind of a Roc calendar
        Roc = 17,
    }

    impl AnyCalendarKind {
        /// Read the calendar type off of the -u-ca- extension on a locale.
        ///
        /// Returns nothing if there is no calendar on the locale or if the locale's calendar
        /// is not known or supported.
        #[diplomat::rust_link(icu::calendar::AnyCalendarKind::get_for_locale, FnInEnum)]
        pub fn get_for_locale(locale: &Locale) -> Option<AnyCalendarKind> {
            icu_calendar::AnyCalendarKind::get_for_locale(&locale.0).map(Into::into)
        }

        /// Obtain the calendar type given a BCP-47 -u-ca- extension string.
        ///
        /// Returns nothing if the calendar is not known or supported.
        #[diplomat::rust_link(icu::calendar::AnyCalendarKind::get_for_bcp47_value, FnInEnum)]
        #[diplomat::rust_link(
            icu::calendar::AnyCalendarKind::get_for_bcp47_string,
            FnInEnum,
            hidden
        )]
        #[diplomat::rust_link(
            icu::calendar::AnyCalendarKind::get_for_bcp47_bytes,
            FnInEnum,
            hidden
        )]
        pub fn get_for_bcp47(s: &DiplomatStr) -> Option<AnyCalendarKind> {
            icu_calendar::AnyCalendarKind::get_for_bcp47_bytes(s).map(Into::into)
        }

        /// Obtain the string suitable for use in the -u-ca- extension in a BCP47 locale.
        #[diplomat::rust_link(icu::calendar::AnyCalendarKind::as_bcp47_string, FnInEnum)]
        #[diplomat::rust_link(icu::calendar::AnyCalendarKind::as_bcp47_value, FnInEnum, hidden)]
        #[diplomat::attr(auto, getter)]
        pub fn bcp47(self, write: &mut diplomat_runtime::DiplomatWrite) {
            let kind = icu_calendar::AnyCalendarKind::from(self);
            let _infallible = write.write_str(kind.as_bcp47_string());
        }
    }

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    #[diplomat::rust_link(icu::calendar::AnyCalendar, Enum)]
    pub struct Calendar(pub Arc<icu_calendar::AnyCalendar>);

    impl Calendar {
        /// Creates a new [`Calendar`] from the specified date and time.
        #[diplomat::rust_link(icu::calendar::AnyCalendar::try_new, FnInEnum)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "for_locale")]
        #[diplomat::demo(default_constructor)]
        pub fn create_for_locale(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<Calendar>, DataError> {
            let prefs = (&locale.0).into();

            Ok(Box::new(Calendar(Arc::new(call_constructor!(
                icu_calendar::AnyCalendar::try_new,
                icu_calendar::AnyCalendar::try_new_with_any_provider,
                icu_calendar::AnyCalendar::try_new_with_buffer_provider,
                provider,
                prefs
            )?))))
        }

        /// Creates a new [`Calendar`] from the specified date and time.
        #[diplomat::rust_link(icu::calendar::AnyCalendar::new_for_kind, FnInEnum)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "for_kind")]
        pub fn create_for_kind(
            provider: &DataProvider,
            kind: AnyCalendarKind,
        ) -> Result<Box<Calendar>, DataError> {
            Ok(Box::new(Calendar(Arc::new(call_constructor!(
                icu_calendar::AnyCalendar::new_for_kind [r => Ok(r)],
                icu_calendar::AnyCalendar::try_new_for_kind_with_any_provider,
                icu_calendar::AnyCalendar::try_new_for_kind_with_buffer_provider,
                provider,
                kind.into()
            )?))))
        }

        /// Returns the kind of this calendar
        #[diplomat::rust_link(icu::calendar::AnyCalendar::kind, FnInEnum)]
        #[diplomat::attr(auto, getter)]
        pub fn kind(&self) -> AnyCalendarKind {
            self.0.kind().into()
        }
    }
}
