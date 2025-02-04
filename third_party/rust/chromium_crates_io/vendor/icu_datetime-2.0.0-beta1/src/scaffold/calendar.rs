// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Scaffolding traits and impls for calendars.

use crate::provider::{neo::*, *};
use crate::scaffold::UnstableSealed;
use crate::MismatchedCalendarError;
use core::marker::PhantomData;
use icu_calendar::any_calendar::AnyCalendarKind;
use icu_calendar::cal::Chinese;
use icu_calendar::cal::Roc;
use icu_calendar::cal::{
    Buddhist, Coptic, Dangi, Ethiopian, Gregorian, Hebrew, Indian, IslamicCivil,
    IslamicObservational, IslamicTabular, IslamicUmmAlQura, Japanese, JapaneseExtended, Persian,
};
use icu_calendar::{
    any_calendar::IntoAnyCalendar, AnyCalendar, AsCalendar, Calendar, Date, DateTime, Ref, Time,
};
use icu_provider::marker::NeverMarker;
use icu_provider::prelude::*;
use icu_timezone::{CustomZonedDateTime, TimeZoneInfo, TimeZoneModel, UtcOffset};

/// A calendar that can be found in CLDR.
///
/// New implementors of this trait will likely also wish to modify `get_era_code_map()`
/// in the CLDR transformer to support any new era maps.
///
/// <div class="stab unstable">
/// 🚧 This trait is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Do not implement this trait in userland.
/// </div>
pub trait CldrCalendar: UnstableSealed {
    /// The data marker for loading year symbols for this calendar.
    type YearNamesV1Marker: DataMarker<DataStruct = YearNamesV1<'static>>;

    /// The data marker for loading month symbols for this calendar.
    type MonthNamesV1Marker: DataMarker<DataStruct = MonthNamesV1<'static>>;

    /// The data marker for loading skeleton patterns for this calendar.
    type SkeletaV1Marker: DataMarker<DataStruct = PackedPatternsV1<'static>>;
}

impl CldrCalendar for () {
    type YearNamesV1Marker = NeverMarker<YearNamesV1<'static>>;
    type MonthNamesV1Marker = NeverMarker<MonthNamesV1<'static>>;
    type SkeletaV1Marker = NeverMarker<PackedPatternsV1<'static>>;
}

impl CldrCalendar for Buddhist {
    type YearNamesV1Marker = BuddhistYearNamesV1Marker;
    type MonthNamesV1Marker = BuddhistMonthNamesV1Marker;
    type SkeletaV1Marker = BuddhistDateNeoSkeletonPatternsV1Marker;
}

impl CldrCalendar for Chinese {
    type YearNamesV1Marker = ChineseYearNamesV1Marker;
    type MonthNamesV1Marker = ChineseMonthNamesV1Marker;
    type SkeletaV1Marker = ChineseDateNeoSkeletonPatternsV1Marker;
}

impl CldrCalendar for Coptic {
    type YearNamesV1Marker = CopticYearNamesV1Marker;
    type MonthNamesV1Marker = CopticMonthNamesV1Marker;
    type SkeletaV1Marker = CopticDateNeoSkeletonPatternsV1Marker;
}

impl CldrCalendar for Dangi {
    type YearNamesV1Marker = DangiYearNamesV1Marker;
    type MonthNamesV1Marker = DangiMonthNamesV1Marker;
    type SkeletaV1Marker = DangiDateNeoSkeletonPatternsV1Marker;
}

impl CldrCalendar for Ethiopian {
    type YearNamesV1Marker = EthiopianYearNamesV1Marker;
    type MonthNamesV1Marker = EthiopianMonthNamesV1Marker;
    type SkeletaV1Marker = EthiopianDateNeoSkeletonPatternsV1Marker;
}

impl CldrCalendar for Gregorian {
    type YearNamesV1Marker = GregorianYearNamesV1Marker;
    type MonthNamesV1Marker = GregorianMonthNamesV1Marker;
    type SkeletaV1Marker = GregorianDateNeoSkeletonPatternsV1Marker;
}

impl CldrCalendar for Hebrew {
    type YearNamesV1Marker = HebrewYearNamesV1Marker;
    type MonthNamesV1Marker = HebrewMonthNamesV1Marker;
    type SkeletaV1Marker = HebrewDateNeoSkeletonPatternsV1Marker;
}

impl CldrCalendar for Indian {
    type YearNamesV1Marker = IndianYearNamesV1Marker;
    type MonthNamesV1Marker = IndianMonthNamesV1Marker;
    type SkeletaV1Marker = IndianDateNeoSkeletonPatternsV1Marker;
}

impl CldrCalendar for IslamicCivil {
    // this value is not actually a valid identifier for this calendar,
    // however since we are overriding is_identifier_allowed_for_calendar we are using
    // this solely for its effects on skeleton data loading
    type YearNamesV1Marker = IslamicYearNamesV1Marker;
    type MonthNamesV1Marker = IslamicMonthNamesV1Marker;
    type SkeletaV1Marker = IslamicDateNeoSkeletonPatternsV1Marker;
}

impl CldrCalendar for IslamicObservational {
    type YearNamesV1Marker = IslamicYearNamesV1Marker;
    type MonthNamesV1Marker = IslamicMonthNamesV1Marker;
    type SkeletaV1Marker = IslamicDateNeoSkeletonPatternsV1Marker;
}

impl CldrCalendar for IslamicTabular {
    type YearNamesV1Marker = IslamicYearNamesV1Marker;
    type MonthNamesV1Marker = IslamicMonthNamesV1Marker;
    type SkeletaV1Marker = IslamicDateNeoSkeletonPatternsV1Marker;
}

impl CldrCalendar for IslamicUmmAlQura {
    type YearNamesV1Marker = IslamicYearNamesV1Marker;
    type MonthNamesV1Marker = IslamicMonthNamesV1Marker;
    type SkeletaV1Marker = IslamicDateNeoSkeletonPatternsV1Marker;
}

impl CldrCalendar for Japanese {
    type YearNamesV1Marker = JapaneseYearNamesV1Marker;
    type MonthNamesV1Marker = JapaneseMonthNamesV1Marker;
    type SkeletaV1Marker = JapaneseDateNeoSkeletonPatternsV1Marker;
}

impl CldrCalendar for JapaneseExtended {
    type YearNamesV1Marker = JapaneseExtendedYearNamesV1Marker;
    type MonthNamesV1Marker = JapaneseExtendedMonthNamesV1Marker;
    type SkeletaV1Marker = JapaneseExtendedDateNeoSkeletonPatternsV1Marker;
}

impl CldrCalendar for Persian {
    type YearNamesV1Marker = PersianYearNamesV1Marker;
    type MonthNamesV1Marker = PersianMonthNamesV1Marker;
    type SkeletaV1Marker = PersianDateNeoSkeletonPatternsV1Marker;
}

impl CldrCalendar for Roc {
    type YearNamesV1Marker = RocYearNamesV1Marker;
    type MonthNamesV1Marker = RocMonthNamesV1Marker;
    type SkeletaV1Marker = RocDateNeoSkeletonPatternsV1Marker;
}

impl UnstableSealed for () {}
impl UnstableSealed for Buddhist {}
impl UnstableSealed for Chinese {}
impl UnstableSealed for Coptic {}
impl UnstableSealed for Dangi {}
impl UnstableSealed for Ethiopian {}
impl UnstableSealed for Gregorian {}
impl UnstableSealed for Hebrew {}
impl UnstableSealed for Indian {}
impl UnstableSealed for IslamicCivil {}
impl UnstableSealed for IslamicObservational {}
impl UnstableSealed for IslamicTabular {}
impl UnstableSealed for IslamicUmmAlQura {}
impl UnstableSealed for Japanese {}
impl UnstableSealed for JapaneseExtended {}
impl UnstableSealed for Persian {}
impl UnstableSealed for Roc {}

/// A collection of marker types associated with all calendars.
///
/// This is used to group together the calendar-specific marker types that produce a common
/// [`DynamicDataMarker`]. For example, this trait can be implemented for [`YearNamesV1Marker`].
///
/// This trait serves as a building block for a cross-calendar [`BoundDataProvider`].
pub trait CalMarkers<M>: UnstableSealed
where
    M: DynamicDataMarker,
{
    /// The type for a [`Buddhist`] calendar
    type Buddhist: DataMarker<DataStruct = M::DataStruct>;
    /// The type for a [`Chinese`] calendar
    type Chinese: DataMarker<DataStruct = M::DataStruct>;
    /// The type for a [`Coptic`] calendar
    type Coptic: DataMarker<DataStruct = M::DataStruct>;
    /// The type for a [`Dangi`] calendar
    type Dangi: DataMarker<DataStruct = M::DataStruct>;
    /// The type for an [`Ethiopian`] calendar, with Amete Mihret era
    type Ethiopian: DataMarker<DataStruct = M::DataStruct>;
    /// The type for an [`Ethiopian`] calendar, with Amete Alem era
    type EthiopianAmeteAlem: DataMarker<DataStruct = M::DataStruct>;
    /// The type for a [`Gregorian`] calendar
    type Gregorian: DataMarker<DataStruct = M::DataStruct>;
    /// The type for a [`Hebrew`] calendar
    type Hebrew: DataMarker<DataStruct = M::DataStruct>;
    /// The type for a [`Indian`] calendar
    type Indian: DataMarker<DataStruct = M::DataStruct>;
    /// The type for an [`IslamicCivil`] calendar
    type IslamicCivil: DataMarker<DataStruct = M::DataStruct>;
    /// The type for an [`IslamicObservational`] calendar
    type IslamicObservational: DataMarker<DataStruct = M::DataStruct>;
    /// The type for an [`IslamicTabular`] calendar
    type IslamicTabular: DataMarker<DataStruct = M::DataStruct>;
    /// The type for an [`IslamicUmmAlQura`] calendar
    type IslamicUmmAlQura: DataMarker<DataStruct = M::DataStruct>;
    /// The type for a [`Japanese`] calendar
    type Japanese: DataMarker<DataStruct = M::DataStruct>;
    /// The type for a [`JapaneseExtended`] calendar
    type JapaneseExtended: DataMarker<DataStruct = M::DataStruct>;
    /// The type for a [`Persian`] calendar
    type Persian: DataMarker<DataStruct = M::DataStruct>;
    /// The type for a [`Roc`] calendar
    type Roc: DataMarker<DataStruct = M::DataStruct>;
}

/// Implementation of [`CalMarkers`] that includes data for all calendars.
#[derive(Debug)]
#[allow(clippy::exhaustive_enums)] // empty enum
pub enum FullDataCalMarkers {}

impl UnstableSealed for FullDataCalMarkers {}

/// Implementation of [`CalMarkers`] that includes data for no calendars.
#[derive(Debug)]
#[allow(clippy::exhaustive_enums)] // empty enum
pub enum NoDataCalMarkers {}

impl UnstableSealed for NoDataCalMarkers {}

impl<M> CalMarkers<M> for NoDataCalMarkers
where
    M: DynamicDataMarker,
{
    type Buddhist = NeverMarker<M::DataStruct>;
    type Chinese = NeverMarker<M::DataStruct>;
    type Coptic = NeverMarker<M::DataStruct>;
    type Dangi = NeverMarker<M::DataStruct>;
    type Ethiopian = NeverMarker<M::DataStruct>;
    type EthiopianAmeteAlem = NeverMarker<M::DataStruct>;
    type Gregorian = NeverMarker<M::DataStruct>;
    type Hebrew = NeverMarker<M::DataStruct>;
    type Indian = NeverMarker<M::DataStruct>;
    type IslamicCivil = NeverMarker<M::DataStruct>;
    type IslamicObservational = NeverMarker<M::DataStruct>;
    type IslamicTabular = NeverMarker<M::DataStruct>;
    type IslamicUmmAlQura = NeverMarker<M::DataStruct>;
    type Japanese = NeverMarker<M::DataStruct>;
    type JapaneseExtended = NeverMarker<M::DataStruct>;
    type Persian = NeverMarker<M::DataStruct>;
    type Roc = NeverMarker<M::DataStruct>;
}

pub(crate) struct AnyCalendarProvider<H, P> {
    provider: P,
    kind: AnyCalendarKind,
    _helper: PhantomData<H>,
}

impl<H, P> AnyCalendarProvider<H, P> {
    pub(crate) fn new(provider: P, kind: AnyCalendarKind) -> Self {
        Self {
            provider,
            kind,
            _helper: PhantomData,
        }
    }
}

impl<M, H, P> BoundDataProvider<M> for AnyCalendarProvider<H, P>
where
    M: DynamicDataMarker,
    H: CalMarkers<M>,
    P: Sized
        + DataProvider<H::Buddhist>
        + DataProvider<H::Chinese>
        + DataProvider<H::Coptic>
        + DataProvider<H::Dangi>
        + DataProvider<H::Ethiopian>
        + DataProvider<H::EthiopianAmeteAlem>
        + DataProvider<H::Gregorian>
        + DataProvider<H::Hebrew>
        + DataProvider<H::Indian>
        + DataProvider<H::IslamicCivil>
        + DataProvider<H::IslamicObservational>
        + DataProvider<H::IslamicTabular>
        + DataProvider<H::IslamicUmmAlQura>
        + DataProvider<H::Japanese>
        + DataProvider<H::JapaneseExtended>
        + DataProvider<H::Persian>
        + DataProvider<H::Roc>,
{
    fn load_bound(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        use AnyCalendarKind::*;
        let p = &self.provider;
        match self.kind {
            Buddhist => H::Buddhist::bind(p).load_bound(req),
            Chinese => H::Chinese::bind(p).load_bound(req),
            Coptic => H::Coptic::bind(p).load_bound(req),
            Dangi => H::Dangi::bind(p).load_bound(req),
            Ethiopian => H::Ethiopian::bind(p).load_bound(req),
            EthiopianAmeteAlem => H::EthiopianAmeteAlem::bind(p).load_bound(req),
            Gregorian => H::Gregorian::bind(p).load_bound(req),
            Hebrew => H::Hebrew::bind(p).load_bound(req),
            Indian => H::Indian::bind(p).load_bound(req),
            IslamicCivil => H::IslamicCivil::bind(p).load_bound(req),
            IslamicObservational => H::IslamicObservational::bind(p).load_bound(req),
            IslamicTabular => H::IslamicTabular::bind(p).load_bound(req),
            IslamicUmmAlQura => H::IslamicUmmAlQura::bind(p).load_bound(req),
            Japanese => H::Japanese::bind(p).load_bound(req),
            JapaneseExtended => H::JapaneseExtended::bind(p).load_bound(req),
            Persian => H::Persian::bind(p).load_bound(req),
            Roc => H::Roc::bind(p).load_bound(req),
            _ => Err(
                DataError::custom("Don't know how to load data for specified calendar")
                    .with_debug_context(&self.kind),
            ),
        }
    }
    fn bound_marker(&self) -> DataMarkerInfo {
        use AnyCalendarKind::*;
        match self.kind {
            Buddhist => H::Buddhist::INFO,
            Chinese => H::Chinese::INFO,
            Coptic => H::Coptic::INFO,
            Dangi => H::Dangi::INFO,
            Ethiopian => H::Ethiopian::INFO,
            EthiopianAmeteAlem => H::EthiopianAmeteAlem::INFO,
            Gregorian => H::Gregorian::INFO,
            Hebrew => H::Hebrew::INFO,
            Indian => H::Indian::INFO,
            IslamicCivil => H::IslamicCivil::INFO,
            IslamicObservational => H::IslamicObservational::INFO,
            IslamicTabular => H::IslamicTabular::INFO,
            IslamicUmmAlQura => H::IslamicUmmAlQura::INFO,
            Japanese => H::Japanese::INFO,
            JapaneseExtended => H::JapaneseExtended::INFO,
            Persian => H::Persian::INFO,
            Roc => H::Roc::INFO,
            _ => NeverMarker::<M::DataStruct>::INFO,
        }
    }
}

macro_rules! impl_load_any_calendar {
    ([$(($erased:ident, $marker:ident)),+], [$($kind_cal:ident),+], [$($kind:ident => $cal:ident),+]) => {
        impl_load_any_calendar!(@expand [$(($erased, $marker)),+], [$($kind_cal),+], [$($kind => $cal),+]);
    };
    (@expand [$(($erased:ident, $marker:ident)),+], $tail1:tt, $tail2:tt) => {
        $(impl_load_any_calendar!(@single_impl $erased, $marker, $tail1, $tail2);)+
    };
    (@single_impl $erased:ident, $marker:ident, [$($kind_cal:ident),+], [$($kind:ident => $cal:ident),+]) => {
        impl CalMarkers<$erased> for FullDataCalMarkers {
            $(
                type $kind_cal = <$kind_cal as CldrCalendar>::$marker;
            )+
            $(
                type $kind = <$cal as CldrCalendar>::$marker;
            )+
        }
    };
}

impl_load_any_calendar!([
    (YearNamesV1Marker, YearNamesV1Marker),
    (MonthNamesV1Marker, MonthNamesV1Marker),
    (ErasedPackedPatterns, SkeletaV1Marker)
], [
    Buddhist,
    Chinese,
    Coptic,
    Dangi,
    Ethiopian,
    Gregorian,
    Hebrew,
    Indian,
    IslamicCivil,
    IslamicObservational,
    IslamicTabular,
    IslamicUmmAlQura,
    Japanese,
    JapaneseExtended,
    Persian,
    Roc
], [
    EthiopianAmeteAlem => Ethiopian
]);

/// A type that can be converted into a specific calendar system.
pub trait ConvertCalendar {
    /// The converted type. This can be the same as the receiver type.
    type Converted<'a>: Sized;
    /// Converts `self` to the specified [`AnyCalendar`].
    fn to_calendar<'a>(&self, calendar: &'a AnyCalendar) -> Self::Converted<'a>;
}

impl<C: IntoAnyCalendar, A: AsCalendar<Calendar = C>> ConvertCalendar for Date<A> {
    type Converted<'a> = Date<Ref<'a, AnyCalendar>>;
    #[inline]
    fn to_calendar<'a>(&self, calendar: &'a AnyCalendar) -> Self::Converted<'a> {
        self.to_any().to_calendar(Ref(calendar))
    }
}

impl ConvertCalendar for Time {
    type Converted<'a> = Time;
    #[inline]
    fn to_calendar<'a>(&self, _: &'a AnyCalendar) -> Self::Converted<'a> {
        *self
    }
}

impl<C: IntoAnyCalendar, A: AsCalendar<Calendar = C>> ConvertCalendar for DateTime<A> {
    type Converted<'a> = DateTime<Ref<'a, AnyCalendar>>;
    #[inline]
    fn to_calendar<'a>(&self, calendar: &'a AnyCalendar) -> Self::Converted<'a> {
        self.to_any().to_calendar(Ref(calendar))
    }
}

impl<C: IntoAnyCalendar, A: AsCalendar<Calendar = C>, Z: Copy> ConvertCalendar
    for CustomZonedDateTime<A, Z>
{
    type Converted<'a> = CustomZonedDateTime<Ref<'a, AnyCalendar>, Z>;
    #[inline]
    fn to_calendar<'a>(&self, calendar: &'a AnyCalendar) -> Self::Converted<'a> {
        let date = self.date.to_any().to_calendar(Ref(calendar));
        CustomZonedDateTime {
            date,
            time: self.time,
            zone: self.zone,
        }
    }
}

impl<O: TimeZoneModel> ConvertCalendar for TimeZoneInfo<O> {
    type Converted<'a> = TimeZoneInfo<O>;
    #[inline]
    fn to_calendar<'a>(&self, _: &'a AnyCalendar) -> Self::Converted<'a> {
        *self
    }
}

/// An input that may be associated with a specific runtime calendar.
pub trait InSameCalendar {
    /// Checks whether this type is compatible with the given calendar.
    ///
    /// Types that are agnostic to calendar systems should return `Ok(())`.
    fn check_any_calendar_kind(
        &self,
        any_calendar_kind: AnyCalendarKind,
    ) -> Result<(), MismatchedCalendarError>;
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> InSameCalendar for Date<A> {
    #[inline]
    fn check_any_calendar_kind(
        &self,
        any_calendar_kind: AnyCalendarKind,
    ) -> Result<(), MismatchedCalendarError> {
        if self.calendar().any_calendar_kind() == Some(any_calendar_kind) {
            Ok(())
        } else {
            Err(MismatchedCalendarError {
                this_kind: any_calendar_kind,
                date_kind: self.calendar().any_calendar_kind(),
            })
        }
    }
}

impl InSameCalendar for Time {
    #[inline]
    fn check_any_calendar_kind(&self, _: AnyCalendarKind) -> Result<(), MismatchedCalendarError> {
        Ok(())
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>> InSameCalendar for DateTime<A> {
    #[inline]
    fn check_any_calendar_kind(
        &self,
        any_calendar_kind: AnyCalendarKind,
    ) -> Result<(), MismatchedCalendarError> {
        if self.date.calendar().any_calendar_kind() == Some(any_calendar_kind) {
            Ok(())
        } else {
            Err(MismatchedCalendarError {
                this_kind: any_calendar_kind,
                date_kind: self.date.calendar().any_calendar_kind(),
            })
        }
    }
}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> InSameCalendar for CustomZonedDateTime<A, Z> {
    #[inline]
    fn check_any_calendar_kind(&self, _: AnyCalendarKind) -> Result<(), MismatchedCalendarError> {
        Ok(())
    }
}

impl InSameCalendar for UtcOffset {
    #[inline]
    fn check_any_calendar_kind(&self, _: AnyCalendarKind) -> Result<(), MismatchedCalendarError> {
        Ok(())
    }
}

impl<O: TimeZoneModel> InSameCalendar for TimeZoneInfo<O> {
    #[inline]
    fn check_any_calendar_kind(&self, _: AnyCalendarKind) -> Result<(), MismatchedCalendarError> {
        Ok(())
    }
}

/// An input associated with a fixed, static calendar.
pub trait InFixedCalendar<C> {}

impl<C: Calendar, A: AsCalendar<Calendar = C>> InFixedCalendar<C> for Date<A> {}

impl<C> InFixedCalendar<C> for Time {}

impl<C: Calendar, A: AsCalendar<Calendar = C>> InFixedCalendar<C> for DateTime<A> {}

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> InFixedCalendar<C> for CustomZonedDateTime<A, Z> {}

impl<C> InFixedCalendar<C> for UtcOffset {}

impl<C, O: TimeZoneModel> InFixedCalendar<C> for TimeZoneInfo<O> {}
