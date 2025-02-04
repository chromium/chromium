// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::provider::calendar::*;
use crate::provider::neo::*;
use alloc::vec;
use icu_calendar::types::MonthCode;
use icu_provider::prelude::*;

mod key_attr_consts {
    use super::*;

    pub const STADLN_ABBR: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("3s");
    pub const STADLN_WIDE: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("4s");
    pub const STADLN_NARW: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("5s");
    pub const STADLN_SHRT: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("6s");
    pub const FORMAT_ABBR: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("3");
    pub const FORMAT_WIDE: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("4");
    pub const FORMAT_NARW: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("5");
    pub const FORMAT_SHRT: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("6");

    /// Used for matching
    pub const STADLN_ABBR_STR: &str = STADLN_ABBR.as_str();
    pub const STADLN_WIDE_STR: &str = STADLN_WIDE.as_str();
    pub const STADLN_NARW_STR: &str = STADLN_NARW.as_str();
    pub const STADLN_SHRT_STR: &str = STADLN_SHRT.as_str();
    pub const FORMAT_ABBR_STR: &str = FORMAT_ABBR.as_str();
    pub const FORMAT_WIDE_STR: &str = FORMAT_WIDE.as_str();
    pub const FORMAT_NARW_STR: &str = FORMAT_NARW.as_str();
    pub const FORMAT_SHRT_STR: &str = FORMAT_SHRT.as_str();
}

fn month_symbols_map_project_cloned<M, P>(
    payload: &DataPayload<M>,
    req: DataRequest,
) -> Result<DataResponse<P>, DataError>
where
    M: DataMarker<DataStruct = DateSymbolsV1<'static>>,
    P: DataMarker<DataStruct = MonthNamesV1<'static>>,
{
    let new_payload = payload.try_map_project_cloned(|payload, _| {
        use key_attr_consts::*;
        let result = match req.id.marker_attributes.as_str() {
            STADLN_ABBR_STR => payload.months.stand_alone_abbreviated(),
            STADLN_WIDE_STR => payload.months.stand_alone_wide(),
            STADLN_NARW_STR => payload.months.stand_alone_narrow(),
            _ => None,
        };
        if let Some(result) = result {
            return Ok(result.into());
        }
        let result = match req.id.marker_attributes.as_str() {
            STADLN_ABBR_STR | FORMAT_ABBR_STR => &payload.months.format.abbreviated,
            STADLN_WIDE_STR | FORMAT_WIDE_STR => &payload.months.format.wide,
            STADLN_NARW_STR | FORMAT_NARW_STR => &payload.months.format.narrow,
            _ => {
                return Err(DataError::custom("Unknown marker attribute")
                    .with_marker(M::INFO)
                    .with_display_context(req.id.marker_attributes.as_str()))
            }
        };
        Ok(result.into())
    })?;
    Ok(DataResponse {
        payload: new_payload,
        metadata: Default::default(),
    })
}

fn weekday_symbols_map_project_cloned<M, P>(
    payload: &DataPayload<M>,
    req: DataRequest,
) -> Result<DataResponse<P>, DataError>
where
    M: DataMarker<DataStruct = DateSymbolsV1<'static>>,
    P: DataMarker<DataStruct = LinearNamesV1<'static>>,
{
    let new_payload = payload.try_map_project_cloned(|payload, _| {
        use key_attr_consts::*;
        let result = match req.id.marker_attributes.as_str() {
            STADLN_ABBR_STR => payload.weekdays.stand_alone_abbreviated(),
            STADLN_WIDE_STR => payload.weekdays.stand_alone_wide(),
            STADLN_NARW_STR => payload.weekdays.stand_alone_narrow(),
            STADLN_SHRT_STR => payload.weekdays.stand_alone_short(),
            _ => None,
        };
        if let Some(result) = result {
            return Ok(result.into());
        }
        let result = match req.id.marker_attributes.as_str() {
            STADLN_SHRT_STR | FORMAT_SHRT_STR => payload.weekdays.format.short.as_ref(),
            _ => None,
        };
        if let Some(result) = result {
            return Ok(result.into());
        }
        let result = match req.id.marker_attributes.as_str() {
            STADLN_ABBR_STR | FORMAT_ABBR_STR | STADLN_SHRT_STR | FORMAT_SHRT_STR => {
                &payload.weekdays.format.abbreviated
            }
            STADLN_WIDE_STR | FORMAT_WIDE_STR => &payload.weekdays.format.wide,
            STADLN_NARW_STR | FORMAT_NARW_STR => &payload.weekdays.format.narrow,
            _ => {
                return Err(DataError::custom("Unknown marker attribute")
                    .with_marker(M::INFO)
                    .with_display_context(req.id.marker_attributes.as_str()))
            }
        };
        Ok(result.into())
    })?;
    Ok(DataResponse {
        payload: new_payload,
        metadata: Default::default(),
    })
}

fn dayperiod_symbols_map_project_cloned<M, P>(
    payload: &DataPayload<M>,
    req: DataRequest,
) -> Result<DataResponse<P>, DataError>
where
    M: DataMarker<DataStruct = TimeSymbolsV1<'static>>,
    P: DataMarker<DataStruct = LinearNamesV1<'static>>,
{
    let new_payload = payload.try_map_project_cloned(|payload, _| {
        use key_attr_consts::*;
        let result = match req.id.marker_attributes.as_str() {
            STADLN_ABBR_STR => payload.day_periods.stand_alone_abbreviated(),
            STADLN_WIDE_STR => payload.day_periods.stand_alone_wide(),
            STADLN_NARW_STR => payload.day_periods.stand_alone_narrow(),
            _ => None,
        };
        if let Some(result) = result {
            return Ok(result.into());
        }
        let result = match req.id.marker_attributes.as_str() {
            STADLN_ABBR_STR | FORMAT_ABBR_STR => &payload.day_periods.format.abbreviated,
            STADLN_WIDE_STR | FORMAT_WIDE_STR => &payload.day_periods.format.wide,
            STADLN_NARW_STR | FORMAT_NARW_STR => &payload.day_periods.format.narrow,
            _ => {
                return Err(DataError::custom("Unknown marker attribute")
                    .with_marker(M::INFO)
                    .with_display_context(req.id.marker_attributes.as_str()))
            }
        };
        Ok(result.into())
    })?;
    Ok(DataResponse {
        payload: new_payload,
        metadata: Default::default(),
    })
}

impl<'a> From<&months::SymbolsV1<'a>> for MonthNamesV1<'a> {
    fn from(other: &months::SymbolsV1<'a>) -> Self {
        match other {
            months::SymbolsV1::SolarTwelve(cow_list) => {
                // Can't zero-copy convert a cow list to a VarZeroVec, so we need to allocate
                // a new VarZeroVec. Since VarZeroVec does not implement `from_iter`, first we
                // make a Vec of string references.
                let vec: alloc::vec::Vec<&str> = cow_list.iter().map(|x| &**x).collect();
                MonthNamesV1::Linear((&vec).into())
            }
            months::SymbolsV1::Other(zero_map) => {
                // Only calendar that uses this is hebrew, we can assume it is 12-month
                let mut vec = vec![""; 24];

                for (k, v) in zero_map.iter() {
                    let Some((number, leap)) = MonthCode(*k).parsed() else {
                        debug_assert!(false, "Found unknown month code {k}");
                        continue;
                    };
                    let offset = if leap { 12 } else { 0 };
                    if let Some(entry) = vec.get_mut((number + offset - 1) as usize) {
                        *entry = v;
                    } else {
                        debug_assert!(false, "Found out of bounds hebrew month code {k}")
                    }
                }
                MonthNamesV1::LeapLinear((&vec).into())
            }
        }
    }
}

impl<'a> From<&weekdays::SymbolsV1<'a>> for LinearNamesV1<'a> {
    fn from(other: &weekdays::SymbolsV1<'a>) -> Self {
        // Input is a cow array of length 7. Need to make it a VarZeroVec.
        let vec: alloc::vec::Vec<&str> = other.0.iter().map(|x| &**x).collect();
        LinearNamesV1 {
            names: (&vec).into(),
        }
    }
}

impl<'a> From<&day_periods::SymbolsV1<'a>> for LinearNamesV1<'a> {
    fn from(other: &day_periods::SymbolsV1<'a>) -> Self {
        // Input is a struct with four fields. Need to make it a VarZeroVec.
        let vec: alloc::vec::Vec<&str> = match (other.noon.as_ref(), other.midnight.as_ref()) {
            (Some(noon), Some(midnight)) => vec![&other.am, &other.pm, &noon, &midnight],
            (Some(noon), None) => vec![&other.am, &other.pm, &noon],
            (None, Some(midnight)) => vec![&other.am, &other.pm, "", &midnight],
            (None, None) => vec![&other.am, &other.pm],
        };
        LinearNamesV1 {
            names: (&vec).into(),
        }
    }
}

macro_rules! impl_data_provider_adapter {
    ($old_ty:ty, $new_ty:ty, $cnv:ident) => {
        impl DataProvider<$new_ty> for DataPayload<$old_ty> {
            fn load(&self, req: DataRequest) -> Result<DataResponse<$new_ty>, DataError> {
                $cnv(self, req)
            }
        }
    };
}

impl_data_provider_adapter!(
    BuddhistDateSymbolsV1Marker,
    BuddhistMonthNamesV1Marker,
    month_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    ChineseDateSymbolsV1Marker,
    ChineseMonthNamesV1Marker,
    month_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    CopticDateSymbolsV1Marker,
    CopticMonthNamesV1Marker,
    month_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    DangiDateSymbolsV1Marker,
    DangiMonthNamesV1Marker,
    month_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    EthiopianDateSymbolsV1Marker,
    EthiopianMonthNamesV1Marker,
    month_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    GregorianDateSymbolsV1Marker,
    GregorianMonthNamesV1Marker,
    month_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    HebrewDateSymbolsV1Marker,
    HebrewMonthNamesV1Marker,
    month_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    IndianDateSymbolsV1Marker,
    IndianMonthNamesV1Marker,
    month_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    IslamicDateSymbolsV1Marker,
    IslamicMonthNamesV1Marker,
    month_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    JapaneseDateSymbolsV1Marker,
    JapaneseMonthNamesV1Marker,
    month_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    JapaneseExtendedDateSymbolsV1Marker,
    JapaneseExtendedMonthNamesV1Marker,
    month_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    PersianDateSymbolsV1Marker,
    PersianMonthNamesV1Marker,
    month_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    RocDateSymbolsV1Marker,
    RocMonthNamesV1Marker,
    month_symbols_map_project_cloned
);

impl_data_provider_adapter!(
    BuddhistDateSymbolsV1Marker,
    WeekdayNamesV1Marker,
    weekday_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    ChineseDateSymbolsV1Marker,
    WeekdayNamesV1Marker,
    weekday_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    CopticDateSymbolsV1Marker,
    WeekdayNamesV1Marker,
    weekday_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    DangiDateSymbolsV1Marker,
    WeekdayNamesV1Marker,
    weekday_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    EthiopianDateSymbolsV1Marker,
    WeekdayNamesV1Marker,
    weekday_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    GregorianDateSymbolsV1Marker,
    WeekdayNamesV1Marker,
    weekday_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    HebrewDateSymbolsV1Marker,
    WeekdayNamesV1Marker,
    weekday_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    IndianDateSymbolsV1Marker,
    WeekdayNamesV1Marker,
    weekday_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    IslamicDateSymbolsV1Marker,
    WeekdayNamesV1Marker,
    weekday_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    JapaneseDateSymbolsV1Marker,
    WeekdayNamesV1Marker,
    weekday_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    JapaneseExtendedDateSymbolsV1Marker,
    WeekdayNamesV1Marker,
    weekday_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    PersianDateSymbolsV1Marker,
    WeekdayNamesV1Marker,
    weekday_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    RocDateSymbolsV1Marker,
    WeekdayNamesV1Marker,
    weekday_symbols_map_project_cloned
);
impl_data_provider_adapter!(
    TimeSymbolsV1Marker,
    DayPeriodNamesV1Marker,
    dayperiod_symbols_map_project_cloned
);
