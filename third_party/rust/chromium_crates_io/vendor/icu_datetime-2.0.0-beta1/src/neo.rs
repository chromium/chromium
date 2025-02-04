// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! High-level entrypoints for Neo DateTime Formatter

use crate::error::DateTimeFormatterLoadError;
use crate::external_loaders::*;
use crate::fieldsets::enums::CompositeFieldSet;
use crate::format::datetime::try_write_pattern_items;
use crate::input::ExtractedInput;
use crate::pattern::*;
use crate::raw::neo::*;
use crate::scaffold::*;
use crate::scaffold::{
    AllInputMarkers, ConvertCalendar, DateDataMarkers, DateInputMarkers, DateTimeMarkers, GetField,
    InFixedCalendar, InSameCalendar, TimeMarkers, TypedDateDataMarkers, ZoneMarkers,
};
use crate::size_test_macro::size_test;
use crate::MismatchedCalendarError;
use core::fmt;
use core::marker::PhantomData;
use icu_calendar::any_calendar::IntoAnyCalendar;
use icu_calendar::{AnyCalendar, AnyCalendarPreferences};
use icu_decimal::FixedDecimalFormatterPreferences;
use icu_locale_core::preferences::extensions::unicode::keywords::{
    CalendarAlgorithm, HourCycle, NumberingSystem,
};
use icu_locale_core::preferences::{define_preferences, prefs_convert};
use icu_provider::prelude::*;
use writeable::{impl_display_with_writeable, Writeable};

define_preferences!(
    /// The user locale preferences for datetime formatting.
    ///
    /// # Examples
    ///
    /// Two ways to build a preferences bag with a custom hour cycle and calendar system:
    ///
    /// ```
    /// use icu::datetime::DateTimeFormatterPreferences;
    /// use icu::locale::Locale;
    /// use icu::locale::preferences::extensions::unicode::keywords::CalendarAlgorithm;
    /// use icu::locale::preferences::extensions::unicode::keywords::HourCycle;
    /// use icu::locale::subtags::Language;
    ///
    /// let prefs1: DateTimeFormatterPreferences = Locale::try_from_str("fr-u-ca-buddhist-hc-h12").unwrap().into();
    ///
    /// let mut prefs2 = DateTimeFormatterPreferences::default();
    /// prefs2.locale_prefs.language = Language::try_from_str("fr").unwrap();
    /// prefs2.hour_cycle = Some(HourCycle::H12);
    /// prefs2.calendar_algorithm = Some(CalendarAlgorithm::Buddhist);
    ///
    /// assert_eq!(prefs1, prefs2);
    /// ```
    [Copy]
    DateTimeFormatterPreferences,
    {
        /// The user's preferred numbering system
        numbering_system: NumberingSystem,
        /// The user's preferred hour cycle
        hour_cycle: HourCycle,
        /// The user's preferred calendar system
        calendar_algorithm: CalendarAlgorithm
    }
);

prefs_convert!(
    DateTimeFormatterPreferences,
    FixedDecimalFormatterPreferences,
    { numbering_system }
);

prefs_convert!(DateTimeFormatterPreferences, AnyCalendarPreferences, {
    calendar_algorithm
});

/// Helper macro for generating any/buffer constructors in this file.
macro_rules! gen_any_buffer_constructors_with_external_loader {
    (@runtime_fset, $fset:ident, $compiled_fn:ident, $any_fn:ident, $buffer_fn:ident, $internal_fn:ident) => {
        #[doc = icu_provider::gen_any_buffer_unstable_docs!(ANY, Self::$compiled_fn)]
        pub fn $any_fn<P>(
            provider: &P,
            prefs: DateTimeFormatterPreferences,
            field_set: $fset,
        ) -> Result<Self, DateTimeFormatterLoadError>
        where
            P: AnyProvider + ?Sized,
        {
            Self::$internal_fn(
                &provider.as_downcasting(),
                &ExternalLoaderAny(provider),
                prefs,
                field_set.get_field(),
            )
        }
        #[doc = icu_provider::gen_any_buffer_unstable_docs!(BUFFER, Self::$compiled_fn)]
        #[cfg(feature = "serde")]
        pub fn $buffer_fn<P>(
            provider: &P,
            prefs: DateTimeFormatterPreferences,
            field_set: $fset,
        ) -> Result<Self, DateTimeFormatterLoadError>
        where
            P: BufferProvider + ?Sized,
        {
            Self::$internal_fn(
                &provider.as_deserializing(),
                &ExternalLoaderBuffer(provider),
                prefs,
                field_set.get_field(),
            )
        }
    };
    (@compiletime_fset, $fset:ident, $compiled_fn:ident, $any_fn:ident, $buffer_fn:ident, $internal_fn:ident) => {
        #[doc = icu_provider::gen_any_buffer_unstable_docs!(ANY, Self::$compiled_fn)]
        pub fn $any_fn<P>(
            provider: &P,
            prefs: DateTimeFormatterPreferences,
            field_set: $fset,
        ) -> Result<Self, DateTimeFormatterLoadError>
        where
            P: AnyProvider + ?Sized,
        {
            Self::$internal_fn(
                &provider.as_downcasting(),
                &ExternalLoaderAny(provider),
                prefs,
                field_set.get_field(),
            )
        }
        #[doc = icu_provider::gen_any_buffer_unstable_docs!(BUFFER, Self::$compiled_fn)]
        #[cfg(feature = "serde")]
        pub fn $buffer_fn<P>(
            provider: &P,
            prefs: DateTimeFormatterPreferences,
            field_set: $fset,
        ) -> Result<Self, DateTimeFormatterLoadError>
        where
            P: BufferProvider + ?Sized,
        {
            Self::$internal_fn(
                &provider.as_deserializing(),
                &ExternalLoaderBuffer(provider),
                prefs,
                field_set.get_field(),
            )
        }
    };
}

size_test!(FixedCalendarDateTimeFormatter<icu_calendar::Gregorian, crate::fieldsets::YMD>, typed_neo_year_month_day_formatter_size, 344);

/// [`FixedCalendarDateTimeFormatter`] is a formatter capable of formatting dates and/or times from
/// a calendar selected at compile time.
///
/// For more details, please read the [crate root docs][crate].
#[doc = typed_neo_year_month_day_formatter_size!()]
#[derive(Debug)]
pub struct FixedCalendarDateTimeFormatter<C: CldrCalendar, FSet: DateTimeNamesMarker> {
    selection: DateTimeZonePatternSelectionData,
    names: RawDateTimeNames<FSet>,
    _calendar: PhantomData<C>,
}

impl<C: CldrCalendar, FSet: DateTimeMarkers> FixedCalendarDateTimeFormatter<C, FSet>
where
    FSet::D: TypedDateDataMarkers<C>,
    FSet::T: TimeMarkers,
    FSet::Z: ZoneMarkers,
    FSet: GetField<CompositeFieldSet>,
{
    /// Creates a new [`FixedCalendarDateTimeFormatter`] from compiled data with
    /// datetime components specified at build time.
    ///
    /// Use this constructor for optimal data size and memory use
    /// if you know the required datetime components at build time.
    /// If you do not know the datetime components until runtime,
    /// use a `with_components` constructor.
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// use icu::calendar::Date;
    /// use icu::calendar::Gregorian;
    /// use icu::datetime::fieldsets::YMD;
    /// use icu::datetime::FixedCalendarDateTimeFormatter;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let formatter = FixedCalendarDateTimeFormatter::try_new(
    ///     locale!("es-MX").into(),
    ///     YMD::long(),
    /// )
    /// .unwrap();
    ///
    /// assert_writeable_eq!(
    ///     formatter.format(&Date::try_new_gregorian(2023, 12, 20).unwrap()),
    ///     "20 de diciembre de 2023"
    /// );
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn try_new(
        prefs: DateTimeFormatterPreferences,
        field_set: FSet,
    ) -> Result<Self, DateTimeFormatterLoadError>
    where
        crate::provider::Baked: AllFixedCalendarFormattingDataMarkers<C, FSet>,
    {
        Self::try_new_internal(
            &crate::provider::Baked,
            &ExternalLoaderCompiledData,
            prefs,
            field_set.get_field(),
        )
    }

    gen_any_buffer_constructors_with_external_loader!(
        @compiletime_fset,
        FSet,
        try_new,
        try_new_with_any_provider,
        try_new_with_buffer_provider,
        try_new_internal
    );

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<P>(
        provider: &P,
        prefs: DateTimeFormatterPreferences,
        field_set: FSet,
    ) -> Result<Self, DateTimeFormatterLoadError>
    where
        P: ?Sized
            + AllFixedCalendarFormattingDataMarkers<C, FSet>
            + AllFixedCalendarExternalDataMarkers,
    {
        Self::try_new_internal(
            provider,
            &ExternalLoaderUnstable(provider),
            prefs,
            field_set.get_field(),
        )
    }
}

impl<C: CldrCalendar, FSet: DateTimeMarkers> FixedCalendarDateTimeFormatter<C, FSet>
where
    FSet::D: TypedDateDataMarkers<C>,
    FSet::T: TimeMarkers,
    FSet::Z: ZoneMarkers,
{
    fn try_new_internal<P, L>(
        provider: &P,
        loader: &L,
        prefs: DateTimeFormatterPreferences,
        field_set: CompositeFieldSet,
    ) -> Result<Self, DateTimeFormatterLoadError>
    where
        P: ?Sized + AllFixedCalendarFormattingDataMarkers<C, FSet>,
        L: FixedDecimalFormatterLoader,
    {
        let selection = DateTimeZonePatternSelectionData::try_new_with_skeleton(
            &<FSet::D as TypedDateDataMarkers<C>>::DateSkeletonPatternsV1Marker::bind(provider),
            &<FSet::T as TimeMarkers>::TimeSkeletonPatternsV1Marker::bind(provider),
            &FSet::GluePatternV1Marker::bind(provider),
            prefs,
            field_set,
        )
        .map_err(DateTimeFormatterLoadError::Data)?;
        let mut names = RawDateTimeNames::new_without_number_formatting();
        names
            .load_for_pattern(
                &<FSet::D as TypedDateDataMarkers<C>>::YearNamesV1Marker::bind(provider),
                &<FSet::D as TypedDateDataMarkers<C>>::MonthNamesV1Marker::bind(provider),
                &<FSet::D as TypedDateDataMarkers<C>>::WeekdayNamesV1Marker::bind(provider),
                &<FSet::T as TimeMarkers>::DayPeriodNamesV1Marker::bind(provider),
                &<FSet::Z as ZoneMarkers>::EssentialsV1Marker::bind(provider),
                &<FSet::Z as ZoneMarkers>::LocationsV1Marker::bind(provider),
                &<FSet::Z as ZoneMarkers>::GenericLongV1Marker::bind(provider),
                &<FSet::Z as ZoneMarkers>::GenericShortV1Marker::bind(provider),
                &<FSet::Z as ZoneMarkers>::SpecificLongV1Marker::bind(provider),
                &<FSet::Z as ZoneMarkers>::SpecificShortV1Marker::bind(provider),
                &<FSet::Z as ZoneMarkers>::MetazonePeriodV1Marker::bind(provider),
                loader, // fixed decimal formatter
                prefs,
                selection.pattern_items_for_data_loading(),
            )
            .map_err(DateTimeFormatterLoadError::Names)?;
        Ok(Self {
            selection,
            names,
            _calendar: PhantomData,
        })
    }
}

impl<C: CldrCalendar, FSet: DateTimeMarkers> FixedCalendarDateTimeFormatter<C, FSet>
where
    FSet::D: DateInputMarkers,
    FSet::T: TimeMarkers,
    FSet::Z: ZoneMarkers,
{
    /// Formats a datetime. Calendars and fields must match at compile time.
    ///
    /// # Examples
    ///
    /// Mismatched calendars will not compile:
    ///
    /// ```compile_fail
    /// use icu::calendar::Date;
    /// use icu::calendar::cal::Buddhist;
    /// use icu::datetime::FixedCalendarDateTimeFormatter;
    /// use icu::datetime::fieldsets::YMD;
    /// use icu::locale::locale;
    ///
    /// let formatter =
    ///     FixedCalendarDateTimeFormatter::<Buddhist, _>::try_new(
    ///         locale!("es-MX").into(),
    ///         YMD::long(),
    ///     )
    ///     .unwrap();
    ///
    /// // type mismatch resolving `<Gregorian as AsCalendar>::Calendar == Buddhist`
    /// formatter.format(&Date::try_new_gregorian(2023, 12, 20).unwrap());
    /// ```
    ///
    /// A time cannot be passed into the formatter when a date is expected:
    ///
    /// ```compile_fail
    /// use icu::calendar::Time;
    /// use icu::calendar::Gregorian;
    /// use icu::datetime::FixedCalendarDateTimeFormatter;
    /// use icu::datetime::fieldsets::YMD;
    /// use icu::locale::locale;
    ///
    /// let formatter =
    ///     FixedCalendarDateTimeFormatter::<Gregorian, _>::try_new(
    ///         locale!("es-MX").into(),
    ///         YMD::long(),
    ///     )
    ///     .unwrap();
    ///
    /// // the trait `GetField<AnyCalendarKind>`
    /// // is not implemented for `icu::icu_calendar::Time`
    /// formatter.format(&Time::try_new(0, 0, 0, 0).unwrap());
    /// ```
    pub fn format<I>(&self, input: &I) -> FormattedDateTime
    where
        I: ?Sized + InFixedCalendar<C> + AllInputMarkers<FSet>,
    {
        let input = ExtractedInput::extract_from_neo_input::<FSet::D, FSet::T, FSet::Z, I>(input);
        FormattedDateTime {
            pattern: self.selection.select(&input),
            input,
            names: self.names.as_borrowed(),
        }
    }
}

size_test!(
    DateTimeFormatter<crate::fieldsets::YMD>,
    neo_year_month_day_formatter_size,
    400
);

/// [`DateTimeFormatter`] is a formatter capable of formatting dates and/or times from
/// a calendar selected at runtime.
///
/// For more details, please read the [crate root docs][crate].
#[doc = neo_year_month_day_formatter_size!()]
#[derive(Debug)]
pub struct DateTimeFormatter<FSet: DateTimeNamesMarker> {
    selection: DateTimeZonePatternSelectionData,
    names: RawDateTimeNames<FSet>,
    calendar: AnyCalendar,
}

impl<FSet: DateTimeMarkers> DateTimeFormatter<FSet>
where
    FSet::D: DateDataMarkers,
    FSet::T: TimeMarkers,
    FSet::Z: ZoneMarkers,
    FSet: GetField<CompositeFieldSet>,
{
    /// Creates a new [`DateTimeFormatter`] from compiled data with
    /// datetime components specified at build time.
    ///
    /// This method will pick the calendar off of the locale; and if unspecified or unknown will fall back to the default
    /// calendar for the locale. See [`AnyCalendarKind`] for a list of supported calendars.
    ///
    /// Use this constructor for optimal data size and memory use
    /// if you know the required datetime components at build time.
    /// If you do not know the datetime components until runtime,
    /// use a `with_components` constructor.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// use icu::calendar::{any_calendar::AnyCalendar, DateTime};
    /// use icu::datetime::fieldsets::YMD;
    /// use icu::datetime::DateTimeFormatter;
    /// use icu::locale::locale;
    /// use std::str::FromStr;
    /// use writeable::assert_writeable_eq;
    ///
    /// let formatter = DateTimeFormatter::try_new(
    ///     locale!("en-u-ca-hebrew").into(),
    ///     YMD::medium(),
    /// )
    /// .unwrap();
    ///
    /// let datetime = DateTime::try_new_iso(2024, 5, 8, 0, 0, 0).unwrap();
    ///
    /// assert_writeable_eq!(
    ///     formatter.format_any_calendar(&datetime),
    ///     "30 Nisan 5784"
    /// );
    /// ```
    ///
    /// [`AnyCalendarKind`]: icu_calendar::AnyCalendarKind
    #[inline(never)]
    #[cfg(feature = "compiled_data")]
    pub fn try_new(
        prefs: DateTimeFormatterPreferences,
        field_set: FSet,
    ) -> Result<Self, DateTimeFormatterLoadError>
    where
        crate::provider::Baked: AllAnyCalendarFormattingDataMarkers<FSet>,
    {
        Self::try_new_internal(
            &crate::provider::Baked,
            &ExternalLoaderCompiledData,
            prefs,
            field_set.get_field(),
        )
    }

    gen_any_buffer_constructors_with_external_loader!(
        @compiletime_fset,
        FSet,
        try_new,
        try_new_with_any_provider,
        try_new_with_buffer_provider,
        try_new_internal
    );

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<P>(
        provider: &P,
        prefs: DateTimeFormatterPreferences,
        field_set: FSet,
    ) -> Result<Self, DateTimeFormatterLoadError>
    where
        P: ?Sized + AllAnyCalendarFormattingDataMarkers<FSet> + AllAnyCalendarExternalDataMarkers,
    {
        Self::try_new_internal(
            provider,
            &ExternalLoaderUnstable(provider),
            prefs,
            field_set.get_field(),
        )
    }
}

impl<FSet: DateTimeMarkers> DateTimeFormatter<FSet>
where
    FSet::D: DateDataMarkers,
    FSet::T: TimeMarkers,
    FSet::Z: ZoneMarkers,
{
    fn try_new_internal<P, L>(
        provider: &P,
        loader: &L,
        prefs: DateTimeFormatterPreferences,
        field_set: CompositeFieldSet,
    ) -> Result<Self, DateTimeFormatterLoadError>
    where
        P: ?Sized + AllAnyCalendarFormattingDataMarkers<FSet>,
        L: FixedDecimalFormatterLoader + AnyCalendarLoader,
    {
        let calendar = AnyCalendarLoader::load(loader, (&prefs).into())
            .map_err(DateTimeFormatterLoadError::Data)?;
        let kind = calendar.kind();
        let selection = DateTimeZonePatternSelectionData::try_new_with_skeleton(
            &AnyCalendarProvider::<<FSet::D as DateDataMarkers>::Skel, _>::new(provider, kind),
            &<FSet::T as TimeMarkers>::TimeSkeletonPatternsV1Marker::bind(provider),
            &FSet::GluePatternV1Marker::bind(provider),
            prefs,
            field_set,
        )
        .map_err(DateTimeFormatterLoadError::Data)?;
        let mut names = RawDateTimeNames::new_without_number_formatting();
        names
            .load_for_pattern(
                &AnyCalendarProvider::<<FSet::D as DateDataMarkers>::Year, _>::new(provider, kind),
                &AnyCalendarProvider::<<FSet::D as DateDataMarkers>::Month, _>::new(provider, kind),
                &<FSet::D as DateDataMarkers>::WeekdayNamesV1Marker::bind(provider),
                &<FSet::T as TimeMarkers>::DayPeriodNamesV1Marker::bind(provider),
                &<FSet::Z as ZoneMarkers>::EssentialsV1Marker::bind(provider),
                &<FSet::Z as ZoneMarkers>::LocationsV1Marker::bind(provider),
                &<FSet::Z as ZoneMarkers>::GenericLongV1Marker::bind(provider),
                &<FSet::Z as ZoneMarkers>::GenericShortV1Marker::bind(provider),
                &<FSet::Z as ZoneMarkers>::SpecificLongV1Marker::bind(provider),
                &<FSet::Z as ZoneMarkers>::SpecificShortV1Marker::bind(provider),
                &<FSet::Z as ZoneMarkers>::MetazonePeriodV1Marker::bind(provider),
                loader, // fixed decimal formatter
                prefs,
                selection.pattern_items_for_data_loading(),
            )
            .map_err(DateTimeFormatterLoadError::Names)?;
        Ok(Self {
            selection,
            names,
            calendar,
        })
    }
}

impl<FSet: DateTimeMarkers> DateTimeFormatter<FSet>
where
    FSet::D: DateInputMarkers,
    FSet::T: TimeMarkers,
    FSet::Z: ZoneMarkers,
{
    /// Formats a datetime, checking that the calendar system is correct.
    ///
    /// If the datetime is not in the same calendar system as the formatter,
    /// an error is returned.
    ///
    /// # Examples
    ///
    /// Mismatched calendars will return an error:
    ///
    /// ```
    /// use icu::calendar::Date;
    /// use icu::datetime::fieldsets::YMD;
    /// use icu::datetime::DateTimeFormatter;
    /// use icu::datetime::MismatchedCalendarError;
    /// use icu::locale::locale;
    ///
    /// let formatter = DateTimeFormatter::try_new(
    ///     locale!("en-u-ca-hebrew").into(),
    ///     YMD::long(),
    /// )
    /// .unwrap();
    ///
    /// let date = Date::try_new_gregorian(2023, 12, 20).unwrap();
    ///
    /// assert!(matches!(
    ///     formatter.format_same_calendar(&date),
    ///     Err(MismatchedCalendarError { .. })
    /// ));
    /// ```
    ///
    /// A time cannot be passed into the formatter when a date is expected:
    ///
    /// ```compile_fail
    /// use icu::calendar::Time;
    /// use icu::datetime::DateTimeFormatter;
    /// use icu::datetime::fieldsets::YMD;
    /// use icu::locale::locale;
    ///
    /// let formatter = DateTimeFormatter::try_new(
    ///     locale!("es-MX").into(),
    ///     Length::Long.into(),
    /// )
    /// .unwrap();
    ///
    /// // the trait `GetField<AnyCalendarKind>`
    /// // is not implemented for `icu::icu_calendar::Time`
    /// formatter.format_same_calendar(&Time::try_new(0, 0, 0, 0).unwrap());
    /// ```
    pub fn format_same_calendar<I>(
        &self,
        datetime: &I,
    ) -> Result<FormattedDateTime, crate::MismatchedCalendarError>
    where
        I: ?Sized + InSameCalendar + AllInputMarkers<FSet>,
    {
        datetime.check_any_calendar_kind(self.calendar.kind())?;
        let datetime =
            ExtractedInput::extract_from_neo_input::<FSet::D, FSet::T, FSet::Z, I>(datetime);
        Ok(FormattedDateTime {
            pattern: self.selection.select(&datetime),
            input: datetime,
            names: self.names.as_borrowed(),
        })
    }

    /// Formats a datetime after first converting it
    /// to the formatter's calendar.
    ///
    /// # Examples
    ///
    /// Mismatched calendars convert and format automatically:
    ///
    /// ```
    /// use icu::calendar::Date;
    /// use icu::datetime::fieldsets::YMD;
    /// use icu::datetime::DateTimeFormatter;
    /// use icu::datetime::MismatchedCalendarError;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let formatter = DateTimeFormatter::try_new(
    ///     locale!("en-u-ca-hebrew").into(),
    ///     YMD::long(),
    /// )
    /// .unwrap();
    ///
    /// let date = Date::try_new_roc(113, 5, 8).unwrap();
    ///
    /// assert_writeable_eq!(formatter.format_any_calendar(&date), "30 Nisan 5784");
    /// ```
    ///
    /// A time cannot be passed into the formatter when a date is expected:
    ///
    /// ```compile_fail
    /// use icu::calendar::Time;
    /// use icu::datetime::DateTimeFormatter;
    /// use icu::datetime::fieldsets::YMD;
    /// use icu::locale::locale;
    ///
    /// let formatter = DateTimeFormatter::try_new(
    ///     locale!("es-MX").into(),
    ///     Length::Long.into(),
    /// )
    /// .unwrap();
    ///
    /// // the trait `GetField<AnyCalendarKind>`
    /// // is not implemented for `icu::icu_calendar::Time`
    /// formatter.format_any_calendar(&Time::try_new(0, 0, 0, 0).unwrap());
    /// ```
    pub fn format_any_calendar<'a, I>(&'a self, datetime: &I) -> FormattedDateTime<'a>
    where
        I: ?Sized + ConvertCalendar,
        I::Converted<'a>: Sized + AllInputMarkers<FSet>,
    {
        let datetime = datetime.to_calendar(&self.calendar);
        let datetime =
            ExtractedInput::extract_from_neo_input::<FSet::D, FSet::T, FSet::Z, I::Converted<'a>>(
                &datetime,
            );
        FormattedDateTime {
            pattern: self.selection.select(&datetime),
            input: datetime,
            names: self.names.as_borrowed(),
        }
    }
}

impl<C: CldrCalendar, FSet: DateTimeMarkers> FixedCalendarDateTimeFormatter<C, FSet> {
    /// Make this [`FixedCalendarDateTimeFormatter`] adopt a calendar so it can format any date.
    ///
    /// This is useful if you need a [`DateTimeFormatter`] but know the calendar system ahead of time,
    /// so that you do not need to link extra data you aren't using.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::cal::Hebrew;
    /// use icu::calendar::Date;
    /// use icu::datetime::fieldsets::YMD;
    /// use icu::datetime::FixedCalendarDateTimeFormatter;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let formatter = FixedCalendarDateTimeFormatter::try_new(
    ///     locale!("en").into(),
    ///     YMD::long(),
    /// )
    /// .unwrap()
    /// .into_formatter(Hebrew::new());
    ///
    /// let date = Date::try_new_iso(2024, 10, 14).unwrap();
    ///
    /// assert_writeable_eq!(
    ///     formatter.format_any_calendar(&date),
    ///     "12 Tishri 5785"
    /// );
    /// ```
    pub fn into_formatter(self, calendar: C) -> DateTimeFormatter<FSet>
    where
        C: IntoAnyCalendar,
    {
        DateTimeFormatter {
            selection: self.selection,
            names: self.names,
            calendar: calendar.to_any(),
        }
    }
}

impl<FSet: DateTimeMarkers> DateTimeFormatter<FSet> {
    /// Attempt to convert this [`DateTimeFormatter`] into one with a specific calendar.
    ///
    /// Returns an error if the type parameter does not match the inner calendar.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::cal::Hebrew;
    /// use icu::calendar::Date;
    /// use icu::datetime::fieldsets::YMD;
    /// use icu::datetime::DateTimeFormatter;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let formatter = DateTimeFormatter::try_new(
    ///     locale!("en-u-ca-hebrew").into(),
    ///     YMD::long(),
    /// )
    /// .unwrap()
    /// .try_into_typed_formatter::<Hebrew>()
    /// .unwrap();
    ///
    /// let date = Date::try_new_hebrew(5785, 1, 12).unwrap();
    ///
    /// assert_writeable_eq!(formatter.format(&date), "12 Tishri 5785");
    /// ```
    ///
    /// An error occurs if the calendars don't match:
    ///
    /// ```
    /// use icu::calendar::cal::Hebrew;
    /// use icu::calendar::Date;
    /// use icu::datetime::fieldsets::YMD;
    /// use icu::datetime::DateTimeFormatter;
    /// use icu::datetime::MismatchedCalendarError;
    /// use icu::locale::locale;
    ///
    /// let result = DateTimeFormatter::try_new(
    ///     locale!("en-u-ca-buddhist").into(),
    ///     YMD::long(),
    /// )
    /// .unwrap()
    /// .try_into_typed_formatter::<Hebrew>();
    ///
    /// assert!(matches!(result, Err(MismatchedCalendarError { .. })));
    /// ```
    pub fn try_into_typed_formatter<C>(
        self,
    ) -> Result<FixedCalendarDateTimeFormatter<C, FSet>, MismatchedCalendarError>
    where
        C: CldrCalendar + IntoAnyCalendar,
    {
        if let Err(cal) = C::from_any(self.calendar) {
            return Err(MismatchedCalendarError {
                this_kind: cal.kind(),
                date_kind: None,
            });
        }
        Ok(FixedCalendarDateTimeFormatter {
            selection: self.selection,
            names: self.names,
            _calendar: PhantomData,
        })
    }
}

/// A formatter optimized for time and time zone formatting.
///
/// # Examples
///
/// A [`TimeFormatter`] cannot be constructed with a fieldset that involves dates:
///
/// ```
/// use icu::datetime::TimeFormatter;
/// use icu::datetime::fieldsets::Y;
/// use icu::locale::locale;
///
/// assert!(TimeFormatter::try_new(locale!("und").into(), Y::medium()).is_err());
/// ```
///
/// Furthermore, it is a compile error in the format function:
///
/// ```compile_fail,E0271
/// use icu::datetime::TimeFormatter;
/// use icu::datetime::fieldsets::Y;
/// use icu::locale::locale;
///
/// let date: icu::calendar::Date<icu::calendar::Gregorian> = unimplemented!();
/// let formatter = TimeFormatter::try_new(locale!("und").into(), Y::medium()).unwrap();
///
/// // error[E0271]: type mismatch resolving `<Gregorian as AsCalendar>::Calendar == ()`
/// formatter.format(&date);
/// ```
pub type TimeFormatter<FSet> = FixedCalendarDateTimeFormatter<(), FSet>;

/// An intermediate type during a datetime formatting operation.
///
/// Not intended to be stored: convert to a string first.
#[derive(Debug)]
pub struct FormattedDateTime<'a> {
    pattern: DateTimeZonePatternDataBorrowed<'a>,
    input: ExtractedInput,
    names: RawDateTimeNamesBorrowed<'a>,
}

impl Writeable for FormattedDateTime<'_> {
    fn write_to_parts<S: writeable::PartsWrite + ?Sized>(
        &self,
        sink: &mut S,
    ) -> Result<(), fmt::Error> {
        let result = try_write_pattern_items(
            self.pattern.metadata(),
            self.pattern.iter_items(),
            &self.input,
            &self.names,
            self.names.fixed_decimal_formatter,
            sink,
        );
        // A DateTimeWriteError should not occur in normal usage because DateTimeFormatter
        // guarantees that all names for the pattern have been loaded and that the input type
        // is compatible with the pattern. However, this code path might be reachable with
        // invalid data. In that case, debug-panic and return the fallback string.
        match result {
            Ok(Ok(())) => Ok(()),
            Err(fmt::Error) => Err(fmt::Error),
            Ok(Err(e)) => {
                debug_assert!(false, "unexpected error in FormattedDateTime: {e:?}");
                Ok(())
            }
        }
    }

    // TODO(#489): Implement writeable_length_hint
}

impl_display_with_writeable!(FormattedDateTime<'_>);

impl FormattedDateTime<'_> {
    /// Gets the pattern used in this formatted value.
    pub fn pattern(&self) -> DateTimePattern {
        self.pattern.to_pattern()
    }
}
