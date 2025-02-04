// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::{
    error::RangeError,
    provider::*,
    types::{DayOfMonth, DayOfYearInfo, IsoWeekday, WeekOfMonth},
};
use icu_locale_core::preferences::define_preferences;
use icu_provider::prelude::*;

/// Minimum number of days in a month unit required for using this module
pub const MIN_UNIT_DAYS: u16 = 14;

define_preferences!(
    /// The preferences for the week calculator.
    [Copy]
    WeekPreferences,
    {}
);

/// Calculator for week-of-month and week-of-year based on locale-specific configurations.
///
/// Note that things get subtly tricky for weeks that straddle the boundary between two years: different locales
/// may consider them as belonging to the preceding or succeeding year based on where exactly the week gets split.
/// While this struct can be populated by values supplied by the programmer, we do recommend using one of the
/// constructors to get locale data.
#[derive(Clone, Copy, Debug)]
#[non_exhaustive]
pub struct WeekCalculator {
    /// The first day of a week.
    pub first_weekday: IsoWeekday,
    /// For a given week, the minimum number of that week's days present in a given month or year
    /// for the week to be considered part of that month or year.
    pub min_week_days: u8,
    /// The set of weekend days, if available
    pub weekend: Option<WeekdaySet>,
}

impl WeekCalculator {
    icu_provider::gen_any_buffer_data_constructors!(
        (prefs: WeekPreferences) -> error: DataError,
        /// Creates a new [`WeekCalculator`] from compiled data.
    );

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<P>(provider: &P, prefs: WeekPreferences) -> Result<Self, DataError>
    where
        P: DataProvider<crate::provider::WeekDataV2Marker> + ?Sized,
    {
        let locale = DataLocale::from_preferences_locale::<WeekDataV2Marker>(prefs.locale_prefs);
        provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                ..Default::default()
            })
            .map(|response| WeekCalculator {
                first_weekday: response.payload.get().first_weekday,
                min_week_days: response.payload.get().min_week_days,
                weekend: Some(response.payload.get().weekend),
            })
            .map_err(Into::into)
    }

    /// Returns the week of month according to a calendar with min_week_days = 1.
    ///
    /// This is different from what the UTS35 spec describes [1] but the latter is
    /// missing a month of week-of-month field so following the spec would result
    /// in inconsistencies (e.g. in the ISO calendar 2021-01-01 is the last week
    /// of December but 'MMMMW' would have it formatted as 'week 5 of January').
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::types::{DayOfMonth, IsoWeekday, WeekOfMonth};
    /// use icu::calendar::week::WeekCalculator;
    ///
    /// let week_calculator =
    ///     WeekCalculator::try_new(icu::locale::locale!("und-GB").into())
    ///         .expect("locale should be present");
    ///
    /// // Wednesday the 10th is in week 2:
    /// assert_eq!(
    ///     WeekOfMonth(2),
    ///     week_calculator.week_of_month(DayOfMonth(10), IsoWeekday::Wednesday)
    /// );
    /// ```
    ///
    /// [1]: https://www.unicode.org/reports/tr35/tr35-55/tr35-dates.html#Date_Patterns_Week_Of_Year
    pub fn week_of_month(&self, day_of_month: DayOfMonth, iso_weekday: IsoWeekday) -> WeekOfMonth {
        WeekOfMonth(simple_week_of(
            self.first_weekday,
            day_of_month.0 as u16,
            iso_weekday,
        ))
    }

    /// Returns the week of year according to the weekday and [`DayOfYearInfo`].
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::types::IsoWeekday;
    /// use icu::calendar::week::{RelativeUnit, WeekCalculator, WeekOf};
    /// use icu::calendar::Date;
    ///
    /// let week_calculator =
    ///     WeekCalculator::try_new(icu::locale::locale!("und-GB").into())
    ///         .expect("locale should be present");
    ///
    /// let iso_date = Date::try_new_iso(2022, 8, 26).unwrap();
    ///
    /// // Friday August 26 is in week 34 of year 2022:
    /// assert_eq!(
    ///     WeekOf {
    ///         unit: RelativeUnit::Current,
    ///         week: 34
    ///     },
    ///     week_calculator
    ///         .week_of_year(iso_date.day_of_year_info(), IsoWeekday::Friday)
    /// );
    /// ```
    pub fn week_of_year(&self, day_of_year_info: DayOfYearInfo, iso_weekday: IsoWeekday) -> WeekOf {
        week_of(
            self,
            day_of_year_info.days_in_prev_year,
            day_of_year_info.days_in_year,
            day_of_year_info.day_of_year,
            iso_weekday,
        )
        .unwrap_or_else(|_| {
            // We don't have any calendars with < 14 days per year, and it's unlikely we'll add one
            debug_assert!(false);
            WeekOf {
                week: 1,
                unit: RelativeUnit::Current,
            }
        })
    }

    /// Returns the zero based index of `weekday` vs this calendar's start of week.
    fn weekday_index(&self, weekday: IsoWeekday) -> i8 {
        (7 + (weekday as i8) - (self.first_weekday as i8)) % 7
    }

    /// Weekdays that are part of the 'weekend', for calendar purposes.
    /// Days may not be contiguous, and order is based off the first weekday.
    pub fn weekend(&self) -> impl Iterator<Item = IsoWeekday> {
        WeekdaySetIterator::new(
            self.first_weekday,
            self.weekend.unwrap_or(WeekdaySet::new(&[])),
        )
    }
}

impl Default for WeekCalculator {
    fn default() -> Self {
        Self {
            first_weekday: IsoWeekday::Monday,
            min_week_days: 1,
            weekend: Some(WeekdaySet::new(&[IsoWeekday::Saturday, IsoWeekday::Sunday])),
        }
    }
}

/// Returns the weekday that's `num_days` after `weekday`.
fn add_to_weekday(weekday: IsoWeekday, num_days: i32) -> IsoWeekday {
    let new_weekday = (7 + (weekday as i32) + (num_days % 7)) % 7;
    IsoWeekday::from(new_weekday as usize)
}

/// Which year or month that a calendar assigns a week to relative to the year/month
/// the week is in.
#[derive(Clone, Copy, Debug, PartialEq)]
#[allow(clippy::enum_variant_names)]
enum RelativeWeek {
    /// A week that is assigned to the last week of the previous year/month. e.g. 2021-01-01 is week 54 of 2020 per the ISO calendar.
    LastWeekOfPreviousUnit,
    /// A week that's assigned to the current year/month. The offset is 1-based. e.g. 2021-01-11 is week 2 of 2021 per the ISO calendar so would be WeekOfCurrentUnit(2).
    WeekOfCurrentUnit(u8),
    /// A week that is assigned to the first week of the next year/month. e.g. 2019-12-31 is week 1 of 2020 per the ISO calendar.
    FirstWeekOfNextUnit,
}

/// Information about a year or month.
struct UnitInfo {
    /// The weekday of this year/month's first day.
    first_day: IsoWeekday,
    /// The number of days in this year/month.
    duration_days: u16,
}

impl UnitInfo {
    /// Creates a UnitInfo for a given year or month.
    fn new(first_day: IsoWeekday, duration_days: u16) -> Result<UnitInfo, RangeError> {
        if duration_days < MIN_UNIT_DAYS {
            return Err(RangeError {
                field: "num_days_in_unit",
                value: duration_days as i32,
                min: MIN_UNIT_DAYS as i32,
                max: i32::MAX,
            });
        }
        Ok(UnitInfo {
            first_day,
            duration_days,
        })
    }

    /// Returns the start of this unit's first week.
    ///
    /// The returned value can be negative if this unit's first week started during the previous
    /// unit.
    fn first_week_offset(&self, calendar: &WeekCalculator) -> i8 {
        let first_day_index = calendar.weekday_index(self.first_day);
        if 7 - first_day_index >= calendar.min_week_days as i8 {
            -first_day_index
        } else {
            7 - first_day_index
        }
    }

    /// Returns the number of weeks in this unit according to `calendar`.
    fn num_weeks(&self, calendar: &WeekCalculator) -> u8 {
        let first_week_offset = self.first_week_offset(calendar);
        let num_days_including_first_week =
            (self.duration_days as i32) - (first_week_offset as i32);
        debug_assert!(
            num_days_including_first_week >= 0,
            "Unit is shorter than a week."
        );
        ((num_days_including_first_week + 7 - (calendar.min_week_days as i32)) / 7) as u8
    }

    /// Returns the week number for the given day in this unit.
    fn relative_week(&self, calendar: &WeekCalculator, day: u16) -> RelativeWeek {
        let days_since_first_week =
            i32::from(day) - i32::from(self.first_week_offset(calendar)) - 1;
        if days_since_first_week < 0 {
            return RelativeWeek::LastWeekOfPreviousUnit;
        }

        let week_number = (1 + days_since_first_week / 7) as u8;
        if week_number > self.num_weeks(calendar) {
            return RelativeWeek::FirstWeekOfNextUnit;
        }
        RelativeWeek::WeekOfCurrentUnit(week_number)
    }
}

/// The year or month that a calendar assigns a week to relative to the year/month that it is in.
#[derive(Debug, PartialEq)]
#[allow(clippy::exhaustive_enums)] // this type is stable
pub enum RelativeUnit {
    /// A week that is assigned to previous year/month. e.g. 2021-01-01 is week 54 of 2020 per the ISO calendar.
    Previous,
    /// A week that's assigned to the current year/month. e.g. 2021-01-11 is week 2 of 2021 per the ISO calendar.
    Current,
    /// A week that is assigned to the next year/month. e.g. 2019-12-31 is week 1 of 2020 per the ISO calendar.
    Next,
}

/// The week number assigned to a given week according to a calendar.
#[derive(Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct WeekOf {
    /// Week of month/year. 1 based.
    pub week: u8,
    /// The month/year that this week is in, relative to the month/year of the input date.
    pub unit: RelativeUnit,
}

/// Computes & returns the week of given month/year according to `calendar`.
///
/// # Arguments
///  - calendar: Calendar information used to compute the week number.
///  - num_days_in_previous_unit: The number of days in the preceding month/year.
///  - num_days_in_unit: The number of days in the month/year.
///  - day: 1-based day of month/year.
///  - week_day: The weekday of `day`..
pub fn week_of(
    calendar: &WeekCalculator,
    num_days_in_previous_unit: u16,
    num_days_in_unit: u16,
    day: u16,
    week_day: IsoWeekday,
) -> Result<WeekOf, RangeError> {
    let current = UnitInfo::new(
        // The first day of this month/year is (day - 1) days from `day`.
        add_to_weekday(week_day, 1 - i32::from(day)),
        num_days_in_unit,
    )?;

    match current.relative_week(calendar, day) {
        RelativeWeek::LastWeekOfPreviousUnit => {
            let previous = UnitInfo::new(
                add_to_weekday(current.first_day, -i32::from(num_days_in_previous_unit)),
                num_days_in_previous_unit,
            )?;

            Ok(WeekOf {
                week: previous.num_weeks(calendar),
                unit: RelativeUnit::Previous,
            })
        }
        RelativeWeek::WeekOfCurrentUnit(w) => Ok(WeekOf {
            week: w,
            unit: RelativeUnit::Current,
        }),
        RelativeWeek::FirstWeekOfNextUnit => Ok(WeekOf {
            week: 1,
            unit: RelativeUnit::Next,
        }),
    }
}

/// Computes & returns the week of given month or year according to a calendar with min_week_days = 1.
///
/// Does not know anything about the unit size (month or year), and will just assume the date falls
/// within whatever unit that is being considered. In other words, this function returns strictly increasing
/// values as `day` increases, unlike [`week_of()`] which is cyclic.
///
/// # Arguments
///  - first_weekday: The first day of a week.
///  - day: 1-based day of the month or year.
///  - week_day: The weekday of `day`.
pub fn simple_week_of(first_weekday: IsoWeekday, day: u16, week_day: IsoWeekday) -> u8 {
    let calendar = WeekCalculator {
        first_weekday,
        min_week_days: 1,
        weekend: None,
    };

    #[allow(clippy::unwrap_used)] // week_of should can't fail with MIN_UNIT_DAYS
    week_of(
        &calendar,
        // The duration of the previous unit does not influence the result if min_week_days = 1
        // so we only need to use a valid value.
        MIN_UNIT_DAYS,
        u16::MAX,
        day,
        week_day,
    )
    .unwrap()
    .week
}

/// [Iterator] that yields weekdays that are part of the weekend.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct WeekdaySetIterator {
    /// Determines the order in which we should start reading values from `weekend`.
    first_weekday: IsoWeekday,
    /// Day being evaluated.
    current_day: IsoWeekday,
    /// Bitset to read weekdays from.
    weekend: WeekdaySet,
}

impl WeekdaySetIterator {
    /// Creates the Iterator. Sets `current_day` to the day after `first_weekday`.
    pub(crate) fn new(first_weekday: IsoWeekday, weekend: WeekdaySet) -> Self {
        WeekdaySetIterator {
            first_weekday,
            current_day: first_weekday,
            weekend,
        }
    }
}

impl Iterator for WeekdaySetIterator {
    type Item = IsoWeekday;

    fn next(&mut self) -> Option<Self::Item> {
        // Check each bit until we find one that is ON or until we are back to the start of the week.
        while self.current_day.next_day() != self.first_weekday {
            if self.weekend.contains(self.current_day) {
                let result = self.current_day;
                self.current_day = self.current_day.next_day();
                return Some(result);
            } else {
                self.current_day = self.current_day.next_day();
            }
        }

        if self.weekend.contains(self.current_day) {
            // Clear weekend, we've seen all bits.
            // Breaks the loop next time `next()` is called
            self.weekend = WeekdaySet::new(&[]);
            return Some(self.current_day);
        }

        Option::None
    }
}

#[cfg(test)]
mod tests {
    use super::{week_of, RelativeUnit, RelativeWeek, UnitInfo, WeekCalculator, WeekOf};
    use crate::{types::IsoWeekday, Date, DateDuration, RangeError};

    static ISO_CALENDAR: WeekCalculator = WeekCalculator {
        first_weekday: IsoWeekday::Monday,
        min_week_days: 4,
        weekend: None,
    };

    static AE_CALENDAR: WeekCalculator = WeekCalculator {
        first_weekday: IsoWeekday::Saturday,
        min_week_days: 4,
        weekend: None,
    };

    static US_CALENDAR: WeekCalculator = WeekCalculator {
        first_weekday: IsoWeekday::Sunday,
        min_week_days: 1,
        weekend: None,
    };

    #[test]
    fn test_weekday_index() {
        assert_eq!(ISO_CALENDAR.weekday_index(IsoWeekday::Monday), 0);
        assert_eq!(ISO_CALENDAR.weekday_index(IsoWeekday::Sunday), 6);

        assert_eq!(AE_CALENDAR.weekday_index(IsoWeekday::Saturday), 0);
        assert_eq!(AE_CALENDAR.weekday_index(IsoWeekday::Friday), 6);
    }

    #[test]
    fn test_first_week_offset() {
        let first_week_offset =
            |calendar, day| UnitInfo::new(day, 30).unwrap().first_week_offset(calendar);
        assert_eq!(first_week_offset(&ISO_CALENDAR, IsoWeekday::Monday), 0);
        assert_eq!(first_week_offset(&ISO_CALENDAR, IsoWeekday::Tuesday), -1);
        assert_eq!(first_week_offset(&ISO_CALENDAR, IsoWeekday::Wednesday), -2);
        assert_eq!(first_week_offset(&ISO_CALENDAR, IsoWeekday::Thursday), -3);
        assert_eq!(first_week_offset(&ISO_CALENDAR, IsoWeekday::Friday), 3);
        assert_eq!(first_week_offset(&ISO_CALENDAR, IsoWeekday::Saturday), 2);
        assert_eq!(first_week_offset(&ISO_CALENDAR, IsoWeekday::Sunday), 1);

        assert_eq!(first_week_offset(&AE_CALENDAR, IsoWeekday::Saturday), 0);
        assert_eq!(first_week_offset(&AE_CALENDAR, IsoWeekday::Sunday), -1);
        assert_eq!(first_week_offset(&AE_CALENDAR, IsoWeekday::Monday), -2);
        assert_eq!(first_week_offset(&AE_CALENDAR, IsoWeekday::Tuesday), -3);
        assert_eq!(first_week_offset(&AE_CALENDAR, IsoWeekday::Wednesday), 3);
        assert_eq!(first_week_offset(&AE_CALENDAR, IsoWeekday::Thursday), 2);
        assert_eq!(first_week_offset(&AE_CALENDAR, IsoWeekday::Friday), 1);

        assert_eq!(first_week_offset(&US_CALENDAR, IsoWeekday::Sunday), 0);
        assert_eq!(first_week_offset(&US_CALENDAR, IsoWeekday::Monday), -1);
        assert_eq!(first_week_offset(&US_CALENDAR, IsoWeekday::Tuesday), -2);
        assert_eq!(first_week_offset(&US_CALENDAR, IsoWeekday::Wednesday), -3);
        assert_eq!(first_week_offset(&US_CALENDAR, IsoWeekday::Thursday), -4);
        assert_eq!(first_week_offset(&US_CALENDAR, IsoWeekday::Friday), -5);
        assert_eq!(first_week_offset(&US_CALENDAR, IsoWeekday::Saturday), -6);
    }

    #[test]
    fn test_num_weeks() {
        // 4 days in first & last week.
        assert_eq!(
            UnitInfo::new(IsoWeekday::Thursday, 4 + 2 * 7 + 4)
                .unwrap()
                .num_weeks(&ISO_CALENDAR),
            4
        );
        // 3 days in first week, 4 in last week.
        assert_eq!(
            UnitInfo::new(IsoWeekday::Friday, 3 + 2 * 7 + 4)
                .unwrap()
                .num_weeks(&ISO_CALENDAR),
            3
        );
        // 3 days in first & last week.
        assert_eq!(
            UnitInfo::new(IsoWeekday::Friday, 3 + 2 * 7 + 3)
                .unwrap()
                .num_weeks(&ISO_CALENDAR),
            2
        );

        // 1 day in first & last week.
        assert_eq!(
            UnitInfo::new(IsoWeekday::Saturday, 1 + 2 * 7 + 1)
                .unwrap()
                .num_weeks(&US_CALENDAR),
            4
        );
    }

    /// Uses enumeration & bucketing to assign each day of a month or year `unit` to a week.
    ///
    /// This alternative implementation serves as an exhaustive safety check
    /// of relative_week() (in addition to the manual test points used
    /// for testing week_of()).
    fn classify_days_of_unit(calendar: &WeekCalculator, unit: &UnitInfo) -> Vec<RelativeWeek> {
        let mut weeks: Vec<Vec<IsoWeekday>> = Vec::new();
        for day_index in 0..unit.duration_days {
            let day = super::add_to_weekday(unit.first_day, i32::from(day_index));
            if day == calendar.first_weekday || weeks.is_empty() {
                weeks.push(Vec::new());
            }
            weeks.last_mut().unwrap().push(day);
        }

        let mut day_week_of_units = Vec::new();
        let mut weeks_in_unit = 0;
        for (index, week) in weeks.iter().enumerate() {
            let week_of_unit = if week.len() < usize::from(calendar.min_week_days) {
                match index {
                    0 => RelativeWeek::LastWeekOfPreviousUnit,
                    x if x == weeks.len() - 1 => RelativeWeek::FirstWeekOfNextUnit,
                    _ => panic!(),
                }
            } else {
                weeks_in_unit += 1;
                RelativeWeek::WeekOfCurrentUnit(weeks_in_unit)
            };

            day_week_of_units.append(&mut [week_of_unit].repeat(week.len()));
        }
        day_week_of_units
    }

    #[test]
    fn test_relative_week_of_month() {
        for min_week_days in 1..7 {
            for start_of_week in 1..7 {
                let calendar = WeekCalculator {
                    first_weekday: IsoWeekday::from(start_of_week),
                    min_week_days,
                    weekend: None,
                };
                for unit_duration in super::MIN_UNIT_DAYS..400 {
                    for start_of_unit in 1..7 {
                        let unit =
                            UnitInfo::new(IsoWeekday::from(start_of_unit), unit_duration).unwrap();
                        let expected = classify_days_of_unit(&calendar, &unit);
                        for (index, expected_week_of) in expected.iter().enumerate() {
                            let day = index + 1;
                            assert_eq!(
                                unit.relative_week(&calendar, day as u16),
                                *expected_week_of,
                                "For the {day}/{unit_duration} starting on IsoWeekday \
                        {start_of_unit} using start_of_week {start_of_week} \
                        & min_week_days {min_week_days}"
                            );
                        }
                    }
                }
            }
        }
    }

    fn week_of_month_from_iso_date(
        calendar: &WeekCalculator,
        yyyymmdd: u32,
    ) -> Result<WeekOf, RangeError> {
        let year = (yyyymmdd / 10000) as i32;
        let month = ((yyyymmdd / 100) % 100) as u8;
        let day = (yyyymmdd % 100) as u8;

        let date = Date::try_new_iso(year, month, day)?;
        let previous_month = date.added(DateDuration::new(0, -1, 0, 0));

        week_of(
            calendar,
            u16::from(previous_month.days_in_month()),
            u16::from(date.days_in_month()),
            u16::from(day),
            date.day_of_week(),
        )
    }

    #[test]
    fn test_week_of_month_using_dates() {
        assert_eq!(
            week_of_month_from_iso_date(&ISO_CALENDAR, 20210418).unwrap(),
            WeekOf {
                week: 3,
                unit: RelativeUnit::Current,
            }
        );
        assert_eq!(
            week_of_month_from_iso_date(&ISO_CALENDAR, 20210419).unwrap(),
            WeekOf {
                week: 4,
                unit: RelativeUnit::Current,
            }
        );

        // First day of year is a Thursday.
        assert_eq!(
            week_of_month_from_iso_date(&ISO_CALENDAR, 20180101).unwrap(),
            WeekOf {
                week: 1,
                unit: RelativeUnit::Current,
            }
        );
        // First day of year is a Friday.
        assert_eq!(
            week_of_month_from_iso_date(&ISO_CALENDAR, 20210101).unwrap(),
            WeekOf {
                week: 5,
                unit: RelativeUnit::Previous,
            }
        );

        // The month ends on a Wednesday.
        assert_eq!(
            week_of_month_from_iso_date(&ISO_CALENDAR, 20200930).unwrap(),
            WeekOf {
                week: 1,
                unit: RelativeUnit::Next,
            }
        );
        // The month ends on a Thursday.
        assert_eq!(
            week_of_month_from_iso_date(&ISO_CALENDAR, 20201231).unwrap(),
            WeekOf {
                week: 5,
                unit: RelativeUnit::Current,
            }
        );

        // US calendar always assigns the week to the current month. 2020-12-31 is a Thursday.
        assert_eq!(
            week_of_month_from_iso_date(&US_CALENDAR, 20201231).unwrap(),
            WeekOf {
                week: 5,
                unit: RelativeUnit::Current,
            }
        );
        assert_eq!(
            week_of_month_from_iso_date(&US_CALENDAR, 20210101).unwrap(),
            WeekOf {
                week: 1,
                unit: RelativeUnit::Current,
            }
        );
    }
}

#[test]
fn test_simple_week_of() {
    // The 1st is a Monday and the week starts on Mondays.
    assert_eq!(
        simple_week_of(IsoWeekday::Monday, 2, IsoWeekday::Tuesday),
        1
    );
    assert_eq!(simple_week_of(IsoWeekday::Monday, 7, IsoWeekday::Sunday), 1);
    assert_eq!(simple_week_of(IsoWeekday::Monday, 8, IsoWeekday::Monday), 2);

    // The 1st is a Wednesday and the week starts on Tuesdays.
    assert_eq!(
        simple_week_of(IsoWeekday::Tuesday, 1, IsoWeekday::Wednesday),
        1
    );
    assert_eq!(
        simple_week_of(IsoWeekday::Tuesday, 6, IsoWeekday::Monday),
        1
    );
    assert_eq!(
        simple_week_of(IsoWeekday::Tuesday, 7, IsoWeekday::Tuesday),
        2
    );

    // The 1st is a Monday and the week starts on Sundays.
    assert_eq!(
        simple_week_of(IsoWeekday::Sunday, 26, IsoWeekday::Friday),
        4
    );
}

#[test]
fn test_weekend() {
    use icu_locale_core::locale;

    assert_eq!(
        WeekCalculator::try_new(locale!("und").into())
            .unwrap()
            .weekend()
            .collect::<Vec<_>>(),
        vec![IsoWeekday::Saturday, IsoWeekday::Sunday],
    );

    assert_eq!(
        WeekCalculator::try_new(locale!("und-FR").into())
            .unwrap()
            .weekend()
            .collect::<Vec<_>>(),
        vec![IsoWeekday::Saturday, IsoWeekday::Sunday],
    );

    assert_eq!(
        WeekCalculator::try_new(locale!("und-IQ").into())
            .unwrap()
            .weekend()
            .collect::<Vec<_>>(),
        vec![IsoWeekday::Saturday, IsoWeekday::Friday],
    );

    assert_eq!(
        WeekCalculator::try_new(locale!("und-IR").into())
            .unwrap()
            .weekend()
            .collect::<Vec<_>>(),
        vec![IsoWeekday::Friday],
    );
}

#[test]
fn test_weekdays_iter() {
    use IsoWeekday::*;

    // Weekend ends one day before week starts
    let default_weekend = WeekdaySetIterator::new(Monday, WeekdaySet::new(&[Saturday, Sunday]));
    assert_eq!(vec![Saturday, Sunday], default_weekend.collect::<Vec<_>>());

    // Non-contiguous weekend
    let fri_sun_weekend = WeekdaySetIterator::new(Monday, WeekdaySet::new(&[Friday, Sunday]));
    assert_eq!(vec![Friday, Sunday], fri_sun_weekend.collect::<Vec<_>>());

    let multiple_contiguous_days = WeekdaySetIterator::new(
        Monday,
        WeekdaySet::new(&[
            IsoWeekday::Tuesday,
            IsoWeekday::Wednesday,
            IsoWeekday::Thursday,
            IsoWeekday::Friday,
        ]),
    );
    assert_eq!(
        vec![Tuesday, Wednesday, Thursday, Friday],
        multiple_contiguous_days.collect::<Vec<_>>()
    );

    // Non-contiguous days and iterator yielding elements based off first_weekday
    let multiple_non_contiguous_days = WeekdaySetIterator::new(
        Wednesday,
        WeekdaySet::new(&[
            IsoWeekday::Tuesday,
            IsoWeekday::Thursday,
            IsoWeekday::Friday,
            IsoWeekday::Sunday,
        ]),
    );
    assert_eq!(
        vec![Thursday, Friday, Sunday, Tuesday],
        multiple_non_contiguous_days.collect::<Vec<_>>()
    );
}
