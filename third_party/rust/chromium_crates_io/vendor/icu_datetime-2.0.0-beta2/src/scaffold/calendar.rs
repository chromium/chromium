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
use icu_calendar::{any_calendar::IntoAnyCalendar, AnyCalendar, AsCalendar, Calendar, Date, Ref};
use icu_provider::marker::NeverMarker;
use icu_provider::prelude::*;
use icu_time::{
    zone::{models::TimeZoneModel, UtcOffset},
    DateTime, Time, TimeZoneInfo, ZonedDateTime,
};

mod private {
    pub trait Sealed {}
}

/// A calendar that can be found in CLDR.
///
/// New implementors of this trait will likely also wish to modify `get_era_code_map()`
/// in the CLDR transformer to support any new era maps.
///
/// <div class="stab unstable">
/// 🚫 This trait is sealed; it cannot be implemented by user code. If an API requests an item that implements this
/// trait, please consider using a type from the implementors listed below.
/// </div>
pub trait CldrCalendar: private::Sealed {
    /// The data marker for loading year symbols for this calendar.
    type YearNamesV1: DataMarker<DataStruct = YearNames<'static>>;

    /// The data marker for loading month symbols for this calendar.
    type MonthNamesV1: DataMarker<DataStruct = MonthNames<'static>>;

    /// The data marker for loading skeleton patterns for this calendar.
    type SkeletaV1: DataMarker<DataStruct = PackedPatterns<'static>>;
}

impl private::Sealed for () {}
impl CldrCalendar for () {
    type YearNamesV1 = NeverMarker<YearNames<'static>>;
    type MonthNamesV1 = NeverMarker<MonthNames<'static>>;
    type SkeletaV1 = NeverMarker<PackedPatterns<'static>>;
}

impl private::Sealed for Buddhist {}
impl CldrCalendar for Buddhist {
    type YearNamesV1 = BuddhistYearNamesV1;
    type MonthNamesV1 = BuddhistMonthNamesV1;
    type SkeletaV1 = BuddhistDateNeoSkeletonPatternsV1;
}

impl private::Sealed for Chinese {}
impl CldrCalendar for Chinese {
    type YearNamesV1 = ChineseYearNamesV1;
    type MonthNamesV1 = ChineseMonthNamesV1;
    type SkeletaV1 = ChineseDateNeoSkeletonPatternsV1;
}

impl private::Sealed for Coptic {}
impl CldrCalendar for Coptic {
    type YearNamesV1 = CopticYearNamesV1;
    type MonthNamesV1 = CopticMonthNamesV1;
    type SkeletaV1 = CopticDateNeoSkeletonPatternsV1;
}

impl private::Sealed for Dangi {}
impl CldrCalendar for Dangi {
    type YearNamesV1 = DangiYearNamesV1;
    type MonthNamesV1 = DangiMonthNamesV1;
    type SkeletaV1 = DangiDateNeoSkeletonPatternsV1;
}

impl private::Sealed for Ethiopian {}
impl CldrCalendar for Ethiopian {
    type YearNamesV1 = EthiopianYearNamesV1;
    type MonthNamesV1 = EthiopianMonthNamesV1;
    type SkeletaV1 = EthiopianDateNeoSkeletonPatternsV1;
}

impl private::Sealed for Gregorian {}
impl CldrCalendar for Gregorian {
    type YearNamesV1 = GregorianYearNamesV1;
    type MonthNamesV1 = GregorianMonthNamesV1;
    type SkeletaV1 = GregorianDateNeoSkeletonPatternsV1;
}

impl private::Sealed for Hebrew {}
impl CldrCalendar for Hebrew {
    type YearNamesV1 = HebrewYearNamesV1;
    type MonthNamesV1 = HebrewMonthNamesV1;
    type SkeletaV1 = HebrewDateNeoSkeletonPatternsV1;
}

impl private::Sealed for Indian {}
impl CldrCalendar for Indian {
    type YearNamesV1 = IndianYearNamesV1;
    type MonthNamesV1 = IndianMonthNamesV1;
    type SkeletaV1 = IndianDateNeoSkeletonPatternsV1;
}

impl private::Sealed for IslamicCivil {}
impl CldrCalendar for IslamicCivil {
    // this value is not actually a valid identifier for this calendar,
    // however since we are overriding is_identifier_allowed_for_calendar we are using
    // this solely for its effects on skeleton data loading
    type YearNamesV1 = IslamicYearNamesV1;
    type MonthNamesV1 = IslamicMonthNamesV1;
    type SkeletaV1 = IslamicDateNeoSkeletonPatternsV1;
}

impl private::Sealed for IslamicObservational {}
impl CldrCalendar for IslamicObservational {
    type YearNamesV1 = IslamicYearNamesV1;
    type MonthNamesV1 = IslamicMonthNamesV1;
    type SkeletaV1 = IslamicDateNeoSkeletonPatternsV1;
}

impl private::Sealed for IslamicTabular {}
impl CldrCalendar for IslamicTabular {
    type YearNamesV1 = IslamicYearNamesV1;
    type MonthNamesV1 = IslamicMonthNamesV1;
    type SkeletaV1 = IslamicDateNeoSkeletonPatternsV1;
}

impl private::Sealed for IslamicUmmAlQura {}
impl CldrCalendar for IslamicUmmAlQura {
    type YearNamesV1 = IslamicYearNamesV1;
    type MonthNamesV1 = IslamicMonthNamesV1;
    type SkeletaV1 = IslamicDateNeoSkeletonPatternsV1;
}

impl private::Sealed for Japanese {}
impl CldrCalendar for Japanese {
    type YearNamesV1 = JapaneseYearNamesV1;
    type MonthNamesV1 = JapaneseMonthNamesV1;
    type SkeletaV1 = JapaneseDateNeoSkeletonPatternsV1;
}

impl private::Sealed for JapaneseExtended {}
impl CldrCalendar for JapaneseExtended {
    type YearNamesV1 = JapaneseExtendedYearNamesV1;
    type MonthNamesV1 = JapaneseExtendedMonthNamesV1;
    type SkeletaV1 = JapaneseExtendedDateNeoSkeletonPatternsV1;
}

impl private::Sealed for Persian {}
impl CldrCalendar for Persian {
    type YearNamesV1 = PersianYearNamesV1;
    type MonthNamesV1 = PersianMonthNamesV1;
    type SkeletaV1 = PersianDateNeoSkeletonPatternsV1;
}

impl private::Sealed for Roc {}
impl CldrCalendar for Roc {
    type YearNamesV1 = RocYearNamesV1;
    type MonthNamesV1 = RocMonthNamesV1;
    type SkeletaV1 = RocDateNeoSkeletonPatternsV1;
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
/// [`DynamicDataMarker`]. For example, this trait can be implemented for [`YearNamesV1`].
///
/// This trait serves as a building block for a cross-calendar [`BoundDataProvider`].
///
/// <div class="stab unstable">
/// 🚧 This trait is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Do not implement this trait in userland unless you are prepared for things to occasionally break.
/// </div>
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
    (YearNamesV1, YearNamesV1),
    (MonthNamesV1, MonthNamesV1),
    (ErasedPackedPatterns, SkeletaV1)
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
// This trait is implementable
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
        self.to_calendar(Ref(calendar))
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
        DateTime {
            date: self.date.to_calendar(Ref(calendar)),
            time: self.time,
        }
    }
}

impl<C: IntoAnyCalendar, A: AsCalendar<Calendar = C>, Z: Copy> ConvertCalendar
    for ZonedDateTime<A, Z>
{
    type Converted<'a> = ZonedDateTime<Ref<'a, AnyCalendar>, Z>;
    #[inline]
    fn to_calendar<'a>(&self, calendar: &'a AnyCalendar) -> Self::Converted<'a> {
        ZonedDateTime {
            date: self.date.to_calendar(Ref(calendar)),
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
// This trait is implementable
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

impl<C: Calendar, A: AsCalendar<Calendar = C>, Z> InSameCalendar for ZonedDateTime<A, Z> {
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
// This trait is implementable
pub trait InFixedCalendar<C> {}

impl<C: CldrCalendar, A: AsCalendar<Calendar = C>> InFixedCalendar<C> for Date<A> {}

impl<C> InFixedCalendar<C> for Time {}

impl<C: CldrCalendar, A: AsCalendar<Calendar = C>> InFixedCalendar<C> for DateTime<A> {}

impl<C: CldrCalendar, A: AsCalendar<Calendar = C>, Z> InFixedCalendar<C> for ZonedDateTime<A, Z> {}

impl<C> InFixedCalendar<C> for UtcOffset {}

impl<C, O: TimeZoneModel> InFixedCalendar<C> for TimeZoneInfo<O> {}
