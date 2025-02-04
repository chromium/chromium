// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::fields::{self, Field, FieldLength, FieldSymbol, TimeZone};
use crate::fieldsets::enums::{CompositeFieldSet, TimeFieldSet, ZoneFieldSet};
use crate::input::ExtractedInput;
use crate::options::*;
use crate::pattern::DateTimePattern;
use crate::provider::pattern::{
    runtime::{self, PatternMetadata},
    GenericPatternItem, PatternItem,
};
use crate::provider::{neo::*, ErasedPackedPatterns, PackedSkeletonVariant};
use crate::DateTimeFormatterPreferences;
use icu_calendar::types::YearAmbiguity;
use icu_provider::prelude::*;
use marker_attrs::GlueType;
use zerovec::ule::AsULE;
use zerovec::ZeroSlice;

#[derive(Debug, Copy, Clone)]
pub(crate) struct RawOptions {
    pub(crate) length: Length,
    pub(crate) alignment: Option<Alignment>,
    pub(crate) year_style: Option<YearStyle>,
    pub(crate) time_precision: Option<TimePrecision>,
}

impl RawOptions {
    #[cfg(all(feature = "serde", feature = "experimental"))]
    pub(crate) fn merge(self, other: RawOptions) -> Self {
        Self {
            length: self.length,
            alignment: self.alignment.or(other.alignment),
            year_style: self.year_style.or(other.year_style),
            time_precision: self.time_precision.or(other.time_precision),
        }
    }
}

#[derive(Debug, Copy, Clone, Default)]
pub(crate) struct RawPreferences {
    pub(crate) hour_cycle: Option<fields::Hour>,
}

impl RawPreferences {
    pub(crate) fn from_prefs(prefs: DateTimeFormatterPreferences) -> Self {
        Self {
            hour_cycle: prefs.hour_cycle.map(fields::Hour::from_hour_cycle),
        }
    }
}

#[derive(Debug)]
pub(crate) enum DatePatternSelectionData {
    SkeletonDate {
        options: RawOptions,
        payload: DataPayload<ErasedPackedPatterns>,
    },
    // TODO(#4478): add support for optional eras
}

#[derive(Debug, Copy, Clone)]
pub(crate) enum DatePatternDataBorrowed<'a> {
    Resolved(runtime::PatternBorrowed<'a>, Option<Alignment>),
}

/// An "overlap" pattern: one that has fields from at least 2 of date, time, and zone.
///
/// TODO: Consider reducing data size by filtering out explicit overlap patterns when they are
/// the same as their individual patterns with glue.
#[derive(Debug)]
pub(crate) enum OverlapPatternSelectionData {
    SkeletonDateTime {
        options: RawOptions,
        prefs: RawPreferences,
        payload: DataPayload<ErasedPackedPatterns>,
    },
}

#[derive(Debug)]
pub(crate) enum TimePatternSelectionData {
    SkeletonTime {
        options: RawOptions,
        prefs: RawPreferences,
        payload: DataPayload<ErasedPackedPatterns>,
    },
}

#[derive(Debug, Copy, Clone)]
pub(crate) enum TimePatternDataBorrowed<'a> {
    Resolved(
        runtime::PatternBorrowed<'a>,
        Option<Alignment>,
        Option<fields::Hour>,
        Option<FractionalSecondDigits>,
    ),
}

#[derive(Debug)]
pub(crate) enum ZonePatternSelectionData {
    SinglePatternItem(TimeZone, FieldLength, <PatternItem as AsULE>::ULE),
}

#[derive(Debug, Copy, Clone)]
pub(crate) enum ZonePatternDataBorrowed<'a> {
    SinglePatternItem(&'a <PatternItem as AsULE>::ULE),
}

#[derive(Debug, Copy, Clone, Default)]
pub(crate) struct ItemsAndOptions<'a> {
    pub(crate) items: &'a ZeroSlice<PatternItem>,
    pub(crate) alignment: Option<Alignment>,
    pub(crate) hour_cycle: Option<fields::Hour>,
    pub(crate) fractional_second_digits: Option<FractionalSecondDigits>,
}

impl ItemsAndOptions<'_> {
    fn new_empty() -> Self {
        Self {
            items: ZeroSlice::new_empty(),
            ..Default::default()
        }
    }
}

// TODO: Use markers instead of an enum for DateTimeFormatter pattern storage.

#[derive(Debug)]
pub(crate) enum DateTimeZonePatternSelectionData {
    Date(DatePatternSelectionData),
    Time(TimePatternSelectionData),
    Zone(ZonePatternSelectionData),
    Overlap(OverlapPatternSelectionData),
    DateTimeGlue {
        date: DatePatternSelectionData,
        time: TimePatternSelectionData,
        glue: DataPayload<GluePatternV1Marker>,
    },
    DateZoneGlue {
        date: DatePatternSelectionData,
        zone: ZonePatternSelectionData,
        glue: DataPayload<GluePatternV1Marker>,
    },
    TimeZoneGlue {
        time: TimePatternSelectionData,
        zone: ZonePatternSelectionData,
        glue: DataPayload<GluePatternV1Marker>,
    },
    DateTimeZoneGlue {
        date: DatePatternSelectionData,
        time: TimePatternSelectionData,
        zone: ZonePatternSelectionData,
        glue: DataPayload<GluePatternV1Marker>,
    },
}

#[derive(Debug, Copy, Clone)]
pub(crate) enum DateTimeZonePatternDataBorrowed<'a> {
    Date(DatePatternDataBorrowed<'a>),
    Time(TimePatternDataBorrowed<'a>),
    Zone(ZonePatternDataBorrowed<'a>),
    // Minor hack: the borrowed runtime data for the overlap case is the same as for time,
    // so use the same intermediate type. This assumption might need to be revisited.
    Overlap(TimePatternDataBorrowed<'a>),
    DateTimeGlue {
        date: DatePatternDataBorrowed<'a>,
        time: TimePatternDataBorrowed<'a>,
        glue: &'a GluePatternV1<'a>,
    },
    DateZoneGlue {
        date: DatePatternDataBorrowed<'a>,
        zone: ZonePatternDataBorrowed<'a>,
        glue: &'a GluePatternV1<'a>,
    },
    TimeZoneGlue {
        time: TimePatternDataBorrowed<'a>,
        zone: ZonePatternDataBorrowed<'a>,
        glue: &'a GluePatternV1<'a>,
    },
    DateTimeZoneGlue {
        date: DatePatternDataBorrowed<'a>,
        time: TimePatternDataBorrowed<'a>,
        zone: ZonePatternDataBorrowed<'a>,
        glue: &'a GluePatternV1<'a>,
    },
}

impl DatePatternSelectionData {
    pub(crate) fn try_new_with_skeleton(
        provider: &(impl BoundDataProvider<ErasedPackedPatterns> + ?Sized),
        prefs: DateTimeFormatterPreferences,
        attributes: &DataMarkerAttributes,
        options: RawOptions,
    ) -> Result<Self, DataError> {
        let locale = provider.bound_marker().make_locale(prefs.locale_prefs);
        let payload = provider
            .load_bound(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(attributes, &locale),
                ..Default::default()
            })?
            .payload;
        Ok(Self::SkeletonDate { options, payload })
    }

    /// Borrows a pattern containing all of the fields that need to be loaded.
    #[inline]
    pub(crate) fn pattern_items_for_data_loading(&self) -> impl Iterator<Item = PatternItem> + '_ {
        let items: &ZeroSlice<PatternItem> = match self {
            DatePatternSelectionData::SkeletonDate { options, payload } => {
                payload
                    .get()
                    .get(options.length, PackedSkeletonVariant::Variant1)
                    .items
            }
        };
        items.iter()
    }

    /// Borrows a resolved pattern based on the given datetime
    pub(crate) fn select(&self, input: &ExtractedInput) -> DatePatternDataBorrowed {
        match self {
            DatePatternSelectionData::SkeletonDate { options, payload } => {
                let year_style = options.year_style.unwrap_or(YearStyle::Auto);
                let variant = match (
                    year_style,
                    input
                        .year
                        .map(|y| y.year_ambiguity())
                        .unwrap_or(YearAmbiguity::EraAndCenturyRequired),
                ) {
                    (YearStyle::Always, _) | (_, YearAmbiguity::EraAndCenturyRequired) => {
                        PackedSkeletonVariant::Variant1
                    }
                    (YearStyle::Full, _) | (_, YearAmbiguity::CenturyRequired) => {
                        PackedSkeletonVariant::Variant0
                    }
                    (YearStyle::Auto, YearAmbiguity::Unambiguous | YearAmbiguity::EraRequired) => {
                        PackedSkeletonVariant::Standard
                    }
                };
                DatePatternDataBorrowed::Resolved(
                    payload.get().get(options.length, variant),
                    options.alignment,
                )
            }
        }
    }
}

impl ExtractedInput {
    fn resolve_time_precision(
        &self,
        time_precision: TimePrecision,
    ) -> (PackedSkeletonVariant, Option<FractionalSecondDigits>) {
        enum HourMinute {
            Hour,
            Minute,
        }
        let smallest_required_field = match time_precision {
            TimePrecision::HourExact => return (PackedSkeletonVariant::Standard, None),
            TimePrecision::MinuteExact => return (PackedSkeletonVariant::Variant0, None),
            TimePrecision::SecondExact(f) => return (PackedSkeletonVariant::Variant1, Some(f)),
            TimePrecision::HourPlus => HourMinute::Hour,
            TimePrecision::MinutePlus => HourMinute::Minute,
            TimePrecision::SecondPlus => return (PackedSkeletonVariant::Variant1, None),
        };
        let minute = self.minute.unwrap_or_default();
        let second = self.second.unwrap_or_default();
        let nanosecond = self.nanosecond.unwrap_or_default();
        if !nanosecond.is_zero() || !second.is_zero() {
            (PackedSkeletonVariant::Variant1, None)
        } else if !minute.is_zero() || matches!(smallest_required_field, HourMinute::Minute) {
            (PackedSkeletonVariant::Variant0, None)
        } else {
            (PackedSkeletonVariant::Standard, None)
        }
    }
}

impl<'a> DatePatternDataBorrowed<'a> {
    pub(crate) fn items_and_options(self) -> ItemsAndOptions<'a> {
        let Self::Resolved(pattern, alignment) = self;
        ItemsAndOptions {
            items: pattern.items,
            alignment,
            ..Default::default()
        }
    }
}

impl OverlapPatternSelectionData {
    pub(crate) fn try_new_with_skeleton(
        provider: &(impl BoundDataProvider<ErasedPackedPatterns> + ?Sized),
        prefs: DateTimeFormatterPreferences,
        attributes: &DataMarkerAttributes,
        options: RawOptions,
    ) -> Result<Self, DataError> {
        let locale = provider.bound_marker().make_locale(prefs.locale_prefs);
        let prefs = RawPreferences::from_prefs(prefs);
        let payload = provider
            .load_bound(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(attributes, &locale),
                ..Default::default()
            })?
            .payload;
        Ok(Self::SkeletonDateTime {
            options,
            prefs,
            payload,
        })
    }

    /// Borrows a pattern containing all of the fields that need to be loaded.
    #[inline]
    pub(crate) fn pattern_items_for_data_loading(&self) -> impl Iterator<Item = PatternItem> + '_ {
        let items: &ZeroSlice<PatternItem> = match self {
            OverlapPatternSelectionData::SkeletonDateTime {
                options, payload, ..
            } => {
                payload
                    .get()
                    .get(options.length, PackedSkeletonVariant::Variant1)
                    .items
            }
        };
        items.iter()
    }

    /// Borrows a resolved pattern based on the given datetime
    pub(crate) fn select(&self, input: &ExtractedInput) -> TimePatternDataBorrowed {
        match self {
            OverlapPatternSelectionData::SkeletonDateTime {
                options,
                prefs,
                payload,
            } => {
                // Currently, none of the overlap patterns have a year field,
                // so we can use the variant to select the time precision.
                //
                // We do not currently support overlap patterns with both a
                // year and a time because that would involve 3*3 = 9 variants
                // instead of 3 variants.
                debug_assert!(options.year_style.is_none());
                let time_precision = options.time_precision.unwrap_or(TimePrecision::SecondPlus);
                let (variant, fractional_second_digits) =
                    input.resolve_time_precision(time_precision);
                TimePatternDataBorrowed::Resolved(
                    payload.get().get(options.length, variant),
                    options.alignment,
                    prefs.hour_cycle,
                    fractional_second_digits,
                )
            }
        }
    }
}

impl TimePatternSelectionData {
    pub(crate) fn try_new_with_skeleton(
        provider: &(impl BoundDataProvider<ErasedPackedPatterns> + ?Sized),
        prefs: DateTimeFormatterPreferences,
        components: TimeFieldSet,
        options: RawOptions,
    ) -> Result<Self, DataError> {
        let locale = provider.bound_marker().make_locale(prefs.locale_prefs);
        let prefs = RawPreferences::from_prefs(prefs);
        // First try to load with the explicit hour cycle. If there is no explicit hour cycle,
        // or if loading the explicit hour cycle fails, then load with the default hour cycle.
        let mut maybe_payload = None;
        if let Some(hour_cycle) = prefs.hour_cycle {
            maybe_payload = provider
                .load_bound(DataRequest {
                    id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                        components.id_str_for_hour_cycle(Some(hour_cycle)),
                        &locale,
                    ),
                    ..Default::default()
                })
                .allow_identifier_not_found()?
                .map(|r| r.payload);
        }
        let payload = match maybe_payload {
            Some(payload) => payload,
            None => {
                provider
                    .load_bound(DataRequest {
                        id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                            components.id_str_for_hour_cycle(None),
                            &locale,
                        ),
                        ..Default::default()
                    })?
                    .payload
            }
        };
        Ok(Self::SkeletonTime {
            options,
            prefs,
            payload,
        })
    }

    /// Borrows a pattern containing all of the fields that need to be loaded.
    #[inline]
    pub(crate) fn pattern_items_for_data_loading(&self) -> impl Iterator<Item = PatternItem> + '_ {
        let items: &ZeroSlice<PatternItem> = match self {
            TimePatternSelectionData::SkeletonTime {
                options, payload, ..
            } => {
                payload
                    .get()
                    .get(options.length, PackedSkeletonVariant::Standard)
                    .items
            }
        };
        items.iter()
    }

    /// Borrows a resolved pattern based on the given datetime
    pub(crate) fn select(&self, input: &ExtractedInput) -> TimePatternDataBorrowed {
        match self {
            TimePatternSelectionData::SkeletonTime {
                options,
                prefs,
                payload,
            } => {
                let time_precision = options.time_precision.unwrap_or(TimePrecision::SecondPlus);
                let (variant, fractional_second_digits) =
                    input.resolve_time_precision(time_precision);
                TimePatternDataBorrowed::Resolved(
                    payload.get().get(options.length, variant),
                    options.alignment,
                    prefs.hour_cycle,
                    fractional_second_digits,
                )
            }
        }
    }
}

impl<'a> TimePatternDataBorrowed<'a> {
    pub(crate) fn items_and_options(self) -> ItemsAndOptions<'a> {
        let Self::Resolved(pattern, alignment, hour_cycle, fractional_second_digits) = self;
        ItemsAndOptions {
            items: pattern.items,
            alignment,
            hour_cycle,
            fractional_second_digits,
        }
    }
}

impl ZonePatternSelectionData {
    pub(crate) fn new_with_skeleton(field_set: ZoneFieldSet) -> Self {
        let (symbol, length) = field_set.to_field();
        let pattern_item = PatternItem::Field(Field {
            symbol: FieldSymbol::TimeZone(symbol),
            length,
        });
        Self::SinglePatternItem(symbol, length, pattern_item.to_unaligned())
    }

    /// Borrows a pattern containing all of the fields that need to be loaded.
    #[inline]
    pub(crate) fn pattern_items_for_data_loading(&self) -> impl Iterator<Item = PatternItem> + '_ {
        let Self::SinglePatternItem(symbol, length, _) = self;
        [PatternItem::Field(Field {
            symbol: FieldSymbol::TimeZone(*symbol),
            length: *length,
        })]
        .into_iter()
    }

    /// Borrows a resolved pattern based on the given datetime
    pub(crate) fn select(&self, _input: &ExtractedInput) -> ZonePatternDataBorrowed {
        let Self::SinglePatternItem(_, _, pattern_item) = self;
        ZonePatternDataBorrowed::SinglePatternItem(pattern_item)
    }
}

impl<'a> ZonePatternDataBorrowed<'a> {
    pub(crate) fn items_and_options(self) -> ItemsAndOptions<'a> {
        let Self::SinglePatternItem(item) = self;
        ItemsAndOptions {
            items: ZeroSlice::from_ule_slice(core::slice::from_ref(item)),
            ..Default::default()
        }
    }
}

impl DateTimeZonePatternSelectionData {
    #[allow(clippy::too_many_arguments)] // private function with lots of generics
    pub(crate) fn try_new_with_skeleton(
        date_provider: &(impl BoundDataProvider<ErasedPackedPatterns> + ?Sized),
        time_provider: &(impl BoundDataProvider<ErasedPackedPatterns> + ?Sized),
        glue_provider: &(impl BoundDataProvider<GluePatternV1Marker> + ?Sized),
        prefs: DateTimeFormatterPreferences,
        skeleton: CompositeFieldSet,
    ) -> Result<Self, DataError> {
        match skeleton {
            CompositeFieldSet::Date(field_set) => {
                let options = field_set.to_raw_options();
                let selection = DatePatternSelectionData::try_new_with_skeleton(
                    date_provider,
                    prefs,
                    field_set.id_str(),
                    options,
                )?;
                Ok(Self::Date(selection))
            }
            CompositeFieldSet::CalendarPeriod(field_set) => {
                let options = field_set.to_raw_options();
                let selection = DatePatternSelectionData::try_new_with_skeleton(
                    date_provider,
                    prefs,
                    field_set.id_str(),
                    options,
                )?;
                Ok(Self::Date(selection))
            }
            CompositeFieldSet::Time(field_set) => {
                let options = field_set.to_raw_options();
                let selection = TimePatternSelectionData::try_new_with_skeleton(
                    time_provider,
                    prefs,
                    field_set,
                    options,
                )?;
                Ok(Self::Time(selection))
            }
            CompositeFieldSet::Zone(field_set) => {
                let selection = ZonePatternSelectionData::new_with_skeleton(field_set);
                Ok(Self::Zone(selection))
            }
            CompositeFieldSet::DateTime(field_set) => {
                let options = field_set.to_raw_options();
                // TODO(#5387): load the patterns for custom hour cycles here
                if let (Some(attributes), None) = (field_set.id_str(), prefs.hour_cycle) {
                    // Try loading an overlap pattern.
                    if let Some(overlap) = OverlapPatternSelectionData::try_new_with_skeleton(
                        // Note: overlap patterns are stored in the date provider
                        date_provider,
                        prefs,
                        attributes,
                        options,
                    )
                    .allow_identifier_not_found()?
                    {
                        return Ok(Self::Overlap(overlap));
                    }
                }
                let date = DatePatternSelectionData::try_new_with_skeleton(
                    date_provider,
                    prefs,
                    field_set.to_date_field_set().id_str(),
                    options,
                )?;
                let time = TimePatternSelectionData::try_new_with_skeleton(
                    time_provider,
                    prefs,
                    field_set.to_time_field_set(),
                    options,
                )?;
                let glue = Self::load_glue(glue_provider, prefs, options, GlueType::DateTime)?;
                Ok(Self::DateTimeGlue { date, time, glue })
            }
            CompositeFieldSet::DateZone(field_set, time_zone_style) => {
                let options = field_set.to_raw_options();
                let date = DatePatternSelectionData::try_new_with_skeleton(
                    date_provider,
                    prefs,
                    field_set.id_str(),
                    options,
                )?;
                // Always use the short length for time zones when mixed with another field (Date)
                let zone_field_set =
                    ZoneFieldSet::from_time_zone_style_and_length(time_zone_style, Length::Short);
                let zone = ZonePatternSelectionData::new_with_skeleton(zone_field_set);
                let glue = Self::load_glue(glue_provider, prefs, options, GlueType::DateZone)?;
                Ok(Self::DateZoneGlue { date, zone, glue })
            }
            CompositeFieldSet::TimeZone(field_set, time_zone_style) => {
                let options = field_set.to_raw_options();
                let time = TimePatternSelectionData::try_new_with_skeleton(
                    time_provider,
                    prefs,
                    field_set,
                    options,
                )?;
                // Always use the short length for time zones when mixed with another field (Time)
                let zone_field_set =
                    ZoneFieldSet::from_time_zone_style_and_length(time_zone_style, Length::Short);
                let zone = ZonePatternSelectionData::new_with_skeleton(zone_field_set);
                let glue = Self::load_glue(glue_provider, prefs, options, GlueType::TimeZone)?;
                Ok(Self::TimeZoneGlue { time, zone, glue })
            }
            CompositeFieldSet::DateTimeZone(field_set, time_zone_style) => {
                let options = field_set.to_raw_options();
                let date = DatePatternSelectionData::try_new_with_skeleton(
                    date_provider,
                    prefs,
                    field_set.to_date_field_set().id_str(),
                    options,
                )?;
                let time = TimePatternSelectionData::try_new_with_skeleton(
                    time_provider,
                    prefs,
                    field_set.to_time_field_set(),
                    options,
                )?;
                // Always use the short length for time zones when mixed with another field (Date + Time)
                let zone_field_set =
                    ZoneFieldSet::from_time_zone_style_and_length(time_zone_style, Length::Short);
                let zone = ZonePatternSelectionData::new_with_skeleton(zone_field_set);
                let glue = Self::load_glue(glue_provider, prefs, options, GlueType::DateTimeZone)?;
                Ok(Self::DateTimeZoneGlue {
                    date,
                    time,
                    zone,
                    glue,
                })
            }
        }
    }

    fn load_glue(
        provider: &(impl BoundDataProvider<GluePatternV1Marker> + ?Sized),
        prefs: DateTimeFormatterPreferences,
        options: RawOptions,
        glue_type: GlueType,
    ) -> Result<DataPayload<GluePatternV1Marker>, DataError> {
        let locale = provider.bound_marker().make_locale(prefs.locale_prefs);
        provider
            .load_bound(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    marker_attrs::pattern_marker_attr_for_glue(
                        // According to UTS 35, use the date length here: use the glue
                        // pattern "whose type matches the type of the date pattern"
                        match options.length {
                            Length::Long => marker_attrs::PatternLength::Long,
                            Length::Medium => marker_attrs::PatternLength::Medium,
                            Length::Short => marker_attrs::PatternLength::Short,
                        },
                        glue_type,
                    ),
                    &locale,
                ),
                ..Default::default()
            })
            .map(|r| r.payload)
    }

    /// Returns an iterator over the pattern items that may need to be loaded.
    #[inline]
    pub(crate) fn pattern_items_for_data_loading(&self) -> impl Iterator<Item = PatternItem> + '_ {
        let (date, time, zone, overlap) = match self {
            DateTimeZonePatternSelectionData::Date(date) => (Some(date), None, None, None),
            DateTimeZonePatternSelectionData::Time(time) => (None, Some(time), None, None),
            DateTimeZonePatternSelectionData::Zone(zone) => (None, None, Some(zone), None),
            DateTimeZonePatternSelectionData::Overlap(overlap) => (None, None, None, Some(overlap)),
            DateTimeZonePatternSelectionData::DateTimeGlue {
                date,
                time,
                glue: _,
            } => (Some(date), Some(time), None, None),
            DateTimeZonePatternSelectionData::DateZoneGlue {
                date,
                zone,
                glue: _,
            } => (Some(date), None, Some(zone), None),
            DateTimeZonePatternSelectionData::TimeZoneGlue {
                time,
                zone,
                glue: _,
            } => (None, Some(time), Some(zone), None),
            DateTimeZonePatternSelectionData::DateTimeZoneGlue {
                date,
                time,
                zone,
                glue: _,
            } => (Some(date), Some(time), Some(zone), None),
        };
        let date_items = date
            .into_iter()
            .flat_map(|x| x.pattern_items_for_data_loading());
        let time_items = time
            .into_iter()
            .flat_map(|x| x.pattern_items_for_data_loading());
        let zone_items = zone
            .into_iter()
            .flat_map(|x| x.pattern_items_for_data_loading());
        let overlap_items = overlap
            .into_iter()
            .flat_map(|x| x.pattern_items_for_data_loading());
        date_items
            .chain(time_items)
            .chain(zone_items)
            .chain(overlap_items)
    }

    /// Borrows a resolved pattern based on the given datetime
    pub(crate) fn select(&self, input: &ExtractedInput) -> DateTimeZonePatternDataBorrowed {
        match self {
            DateTimeZonePatternSelectionData::Date(date) => {
                DateTimeZonePatternDataBorrowed::Date(date.select(input))
            }
            DateTimeZonePatternSelectionData::Time(time) => {
                DateTimeZonePatternDataBorrowed::Time(time.select(input))
            }
            DateTimeZonePatternSelectionData::Zone(zone) => {
                DateTimeZonePatternDataBorrowed::Zone(zone.select(input))
            }
            DateTimeZonePatternSelectionData::Overlap(overlap) => {
                DateTimeZonePatternDataBorrowed::Overlap(overlap.select(input))
            }
            DateTimeZonePatternSelectionData::DateTimeGlue { date, time, glue } => {
                DateTimeZonePatternDataBorrowed::DateTimeGlue {
                    date: date.select(input),
                    time: time.select(input),
                    glue: glue.get(),
                }
            }
            DateTimeZonePatternSelectionData::DateZoneGlue { date, zone, glue } => {
                DateTimeZonePatternDataBorrowed::DateZoneGlue {
                    date: date.select(input),
                    zone: zone.select(input),
                    glue: glue.get(),
                }
            }
            DateTimeZonePatternSelectionData::TimeZoneGlue { time, zone, glue } => {
                DateTimeZonePatternDataBorrowed::TimeZoneGlue {
                    time: time.select(input),
                    zone: zone.select(input),
                    glue: glue.get(),
                }
            }
            DateTimeZonePatternSelectionData::DateTimeZoneGlue {
                date,
                time,
                zone,
                glue,
            } => DateTimeZonePatternDataBorrowed::DateTimeZoneGlue {
                date: date.select(input),
                time: time.select(input),
                zone: zone.select(input),
                glue: glue.get(),
            },
        }
    }
}

impl<'a> DateTimeZonePatternDataBorrowed<'a> {
    #[inline]
    fn date_pattern(self) -> Option<DatePatternDataBorrowed<'a>> {
        match self {
            Self::Date(date) => Some(date),
            Self::Time(_) => None,
            Self::Zone(_) => None,
            Self::Overlap(_) => None,
            Self::DateTimeGlue { date, .. } => Some(date),
            Self::DateZoneGlue { date, .. } => Some(date),
            Self::TimeZoneGlue { .. } => None,
            Self::DateTimeZoneGlue { date, .. } => Some(date),
        }
    }

    #[inline]
    fn time_pattern(self) -> Option<TimePatternDataBorrowed<'a>> {
        match self {
            Self::Date(_) => None,
            Self::Time(time) => Some(time),
            Self::Zone(_) => None,
            Self::Overlap(time) => Some(time),
            Self::DateTimeGlue { time, .. } => Some(time),
            Self::DateZoneGlue { .. } => None,
            Self::TimeZoneGlue { time, .. } => Some(time),
            Self::DateTimeZoneGlue { time, .. } => Some(time),
        }
    }

    #[inline]
    fn zone_pattern(self) -> Option<ZonePatternDataBorrowed<'a>> {
        match self {
            Self::Date(_) => None,
            Self::Time(_) => None,
            Self::Zone(zone) => Some(zone),
            Self::Overlap(_) => None,
            Self::DateTimeGlue { .. } => None,
            Self::DateZoneGlue { zone, .. } => Some(zone),
            Self::TimeZoneGlue { zone, .. } => Some(zone),
            Self::DateTimeZoneGlue { zone, .. } => Some(zone),
        }
    }

    #[inline]
    fn glue_pattern(self) -> Option<&'a ZeroSlice<GenericPatternItem>> {
        match self {
            Self::Date(_) => None,
            Self::Time(_) => None,
            Self::Zone(_) => None,
            Self::Overlap(_) => None,
            Self::DateTimeGlue { glue, .. } => Some(&glue.pattern.items),
            Self::DateZoneGlue { glue, .. } => Some(&glue.pattern.items),
            Self::TimeZoneGlue { glue, .. } => Some(&glue.pattern.items),
            Self::DateTimeZoneGlue { glue, .. } => Some(&glue.pattern.items),
        }
    }

    #[inline]
    pub(crate) fn metadata(self) -> PatternMetadata {
        match self {
            Self::Date(DatePatternDataBorrowed::Resolved(pb, _)) => pb.metadata,
            Self::Time(TimePatternDataBorrowed::Resolved(pb, _, _, _)) => pb.metadata,
            Self::Zone(_) => Default::default(),
            Self::Overlap(_) => Default::default(),
            Self::DateTimeGlue {
                date: DatePatternDataBorrowed::Resolved(date, _),
                time: TimePatternDataBorrowed::Resolved(time, _, _, _),
                ..
            } => PatternMetadata::merge_date_and_time_metadata(date.metadata, time.metadata),
            Self::DateZoneGlue {
                date: DatePatternDataBorrowed::Resolved(date, _),
                ..
            } => date.metadata,
            Self::TimeZoneGlue {
                time: TimePatternDataBorrowed::Resolved(time, _, _, _),
                ..
            } => time.metadata,
            Self::DateTimeZoneGlue {
                date: DatePatternDataBorrowed::Resolved(date, _),
                time: TimePatternDataBorrowed::Resolved(time, _, _, _),
                ..
            } => PatternMetadata::merge_date_and_time_metadata(date.metadata, time.metadata),
        }
    }

    pub(crate) fn iter_items(self) -> impl Iterator<Item = PatternItem> + 'a {
        let glue_pattern_slice = match self.glue_pattern() {
            Some(glue) => glue.as_ule_slice(),
            None => runtime::ZERO_ONE_TWO_SLICE.as_ule_slice(),
        };
        glue_pattern_slice
            .iter()
            .map(
                move |generic_item_ule| match generic_item_ule.as_pattern_item_ule() {
                    Ok(pattern_item_ule) => ItemsAndOptions {
                        items: ZeroSlice::from_ule_slice(core::slice::from_ref(pattern_item_ule)),
                        ..Default::default()
                    },
                    Err(1) => self
                        .date_pattern()
                        .map(|p| p.items_and_options())
                        .unwrap_or(ItemsAndOptions::new_empty()),
                    Err(0) => self
                        .time_pattern()
                        .map(|p| p.items_and_options())
                        .unwrap_or(ItemsAndOptions::new_empty()),
                    Err(2) => self
                        .zone_pattern()
                        .map(|p| p.items_and_options())
                        .unwrap_or(ItemsAndOptions::new_empty()),
                    _ => ItemsAndOptions::new_empty(),
                },
            )
            .flat_map(|items_and_options| items_and_options.iter_items())
    }

    pub(crate) fn to_pattern(self) -> DateTimePattern {
        let pattern = self.iter_items().collect::<runtime::Pattern>();
        DateTimePattern::from_runtime_pattern(pattern)
    }
}

impl<'a> ItemsAndOptions<'a> {
    pub(crate) fn iter_items(self) -> impl Iterator<Item = PatternItem> + 'a {
        self.items.iter().map(move |mut pattern_item| {
            #[allow(clippy::single_match)] // need `ref mut`, which doesn't work in `if let`?
            match &mut pattern_item {
                PatternItem::Field(ref mut field) => {
                    if matches!(self.alignment, Some(Alignment::Column))
                        && field.length == FieldLength::One
                        && matches!(
                            field.symbol,
                            FieldSymbol::Month(_)
                                | FieldSymbol::Day(_)
                                | FieldSymbol::Week(_)
                                | FieldSymbol::Hour(_)
                        )
                    {
                        field.length = FieldLength::Two;
                    }
                    if let Some(hour_cycle) = self.hour_cycle {
                        if let FieldSymbol::Hour(_) = field.symbol {
                            field.symbol = FieldSymbol::Hour(hour_cycle);
                        }
                    }
                    if let Some(fractional_second_digits) = self.fractional_second_digits {
                        if matches!(
                            field.symbol,
                            FieldSymbol::Second(fields::Second::Second)
                                | FieldSymbol::DecimalSecond(_)
                        ) {
                            field.symbol = FieldSymbol::from_fractional_second_digits(
                                fractional_second_digits,
                            );
                        }
                    }
                }
                _ => (),
            }
            pattern_item
        })
    }
}
