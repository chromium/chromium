// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;

    #[cfg(feature = "buffer_provider")]
    use crate::unstable::errors::ffi::DataError;
    use crate::unstable::locale_core::ffi::Locale;
    #[cfg(feature = "buffer_provider")]
    use crate::unstable::provider::ffi::DataProvider;

    /// The various calendar types currently supported by [`Calendar`]
    #[diplomat::enum_convert(icu_calendar::AnyCalendarKind, needs_wildcard)]
    #[diplomat::rust_link(icu::calendar::AnyCalendarKind, Enum)]
    #[non_exhaustive]
    pub enum CalendarKind {
        /// The kind of an Iso calendar
        // AnyCalendarKind in Rust doesn't have a default, but it is useful to have one
        // here for consistent behavior.
        #[diplomat::attr(auto, default)]
        Iso = 0,
        /// The kind of a Gregorian calendar
        Gregorian = 1,
        /// The kind of a Buddhist calendar
        Buddhist = 2,
        /// The kind of a Japanese calendar
        Japanese = 3,
        /// Deprecated, use `Japanese`
        #[deprecated(note = "use `Japanese`")]
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
        /// The kind of a Hijri tabular, type II leap years, Friday epoch, calendar
        HijriTabularTypeIIFriday = 12,
        /// The kind of a Hijri simulated, Mecca calendar
        HijriSimulatedMecca = 18,
        /// The kind of a Hijri tabular, type II leap years, Thursday epoch, calendar
        HijriTabularTypeIIThursday = 14,
        /// The kind of a Hijri Umm al-Qura calendar
        HijriUmmAlQura = 15,
        /// The kind of a Persian calendar
        Persian = 16,
        /// The kind of a Roc calendar
        Roc = 17,
    }

    impl CalendarKind {
        /// Creates a new [`CalendarKind`] for the specified locale, using compiled data.
        #[diplomat::rust_link(icu::calendar::AnyCalendarKind::new, FnInEnum)]
        pub fn create(locale: &Locale) -> Self {
            let prefs = (&locale.0).into();
            icu_calendar::AnyCalendarKind::new(prefs).into()
        }
    }

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    #[diplomat::rust_link(icu::calendar::AnyCalendar, Enum)]
    pub struct Calendar(pub icu_calendar::AnyCalendar);

    impl Calendar {
        /// Creates a new [`Calendar`] for the specified kind, using compiled data.
        #[diplomat::rust_link(icu::calendar::AnyCalendar::new, FnInEnum)]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create(kind: CalendarKind) -> Box<Calendar> {
            Box::new(Calendar(icu_calendar::AnyCalendar::new(kind.into())))
        }

        /// Creates a new [`Calendar`] for the specified kind, using a particular data source.
        #[diplomat::rust_link(icu::calendar::AnyCalendar::new, FnInEnum)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "new_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
            kind: CalendarKind,
        ) -> Result<Box<Calendar>, DataError> {
            Ok(Box::new(Calendar(
                icu_calendar::AnyCalendar::try_new_with_buffer_provider(
                    provider.get()?,
                    kind.into(),
                )?,
            )))
        }

        /// Returns the kind of this calendar
        #[diplomat::rust_link(icu::calendar::AnyCalendar::kind, FnInEnum)]
        #[diplomat::attr(auto, getter)]
        #[diplomat::attr(demo_gen, disable)] // this just returns the single constructor argument
        pub fn kind(&self) -> CalendarKind {
            self.0.kind().into()
        }
    }
}
