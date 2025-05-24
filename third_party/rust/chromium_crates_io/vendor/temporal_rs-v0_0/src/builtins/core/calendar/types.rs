//! Implementation of `ResolvedCalendarFields`

use tinystr::tinystr;
use tinystr::TinyAsciiStr;

use alloc::format;

use crate::iso::{constrain_iso_day, is_valid_iso_day};
use crate::options::ArithmeticOverflow;
use crate::{TemporalError, TemporalResult};

use crate::builtins::core::{calendar::Calendar, PartialDate};

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ResolutionType {
    Date,
    YearMonth,
    MonthDay,
}

/// `ResolvedCalendarFields` represents the resolved field values necessary for
/// creating a Date from potentially partial values.
#[derive(Debug)]
pub struct ResolvedCalendarFields {
    pub(crate) era_year: EraYear,
    pub(crate) month_code: MonthCode,
    pub(crate) day: u8,
}

impl ResolvedCalendarFields {
    #[inline]
    pub fn try_from_partial(
        partial_date: &PartialDate,
        overflow: ArithmeticOverflow,
        resolve_type: ResolutionType,
    ) -> TemporalResult<Self> {
        let era_year = EraYear::try_from_partial_date(partial_date)?;
        if partial_date.calendar.is_iso() {
            let month_code = resolve_iso_month(partial_date, overflow)?;
            let day = resolve_day(partial_date.day, resolve_type == ResolutionType::YearMonth)?;
            let day = if overflow == ArithmeticOverflow::Constrain {
                constrain_iso_day(era_year.year, month_code.to_month_integer(), day)
            } else {
                if !is_valid_iso_day(era_year.year, month_code.to_month_integer(), day) {
                    return Err(
                        TemporalError::range().with_message("day value is not in a valid range.")
                    );
                }
                day
            };
            return Ok(Self {
                era_year,
                month_code,
                day,
            });
        }

        let month_code = MonthCode::try_from_partial_date(partial_date)?;
        let day = resolve_day(partial_date.day, resolve_type == ResolutionType::YearMonth)?;
        // TODO: Constrain day to calendar range for month?

        Ok(Self {
            era_year,
            month_code,
            day,
        })
    }
}

fn resolve_day(day: Option<u8>, is_year_month: bool) -> TemporalResult<u8> {
    if is_year_month {
        Ok(day.unwrap_or(1))
    } else {
        day.ok_or(TemporalError::r#type().with_message("Required day field is empty."))
    }
}

#[derive(Debug)]
pub struct Era(pub(crate) TinyAsciiStr<16>);

#[derive(Debug)]
pub struct EraYear {
    pub(crate) era: Era,
    pub(crate) year: i32,
}

impl EraYear {
    pub(crate) fn try_from_partial_date(partial: &PartialDate) -> TemporalResult<Self> {
        match (partial.year, partial.era, partial.era_year) {
            (Some(year), None, None) => {
                let Some(era) = partial.calendar.get_calendar_default_era() else {
                    return Err(TemporalError::r#type()
                        .with_message("Era is required for the provided calendar."));
                };
                Ok(Self {
                    era: Era(era.name),
                    year,
                })
            }
            (None, Some(era), Some(era_year)) => {
                let Some(era_info) = partial.calendar.get_era_info(&era) else {
                    return Err(TemporalError::range().with_message("Invalid era provided."));
                };
                if !era_info.range.contains(&era_year) {
                    return Err(TemporalError::range().with_message(format!(
                        "Year is not valid for the era {}",
                        era_info.name.as_str()
                    )));
                }
                Ok(Self {
                    year: era_year,
                    era: Era(era_info.name),
                })
            }
            _ => Err(TemporalError::r#type()
                .with_message("Required fields missing to determine an era and year.")),
        }
    }
}

// MonthCode constants.
const MONTH_ONE: TinyAsciiStr<4> = tinystr!(4, "M01");
const MONTH_ONE_LEAP: TinyAsciiStr<4> = tinystr!(4, "M01L");
const MONTH_TWO: TinyAsciiStr<4> = tinystr!(4, "M02");
const MONTH_TWO_LEAP: TinyAsciiStr<4> = tinystr!(4, "M02L");
const MONTH_THREE: TinyAsciiStr<4> = tinystr!(4, "M03");
const MONTH_THREE_LEAP: TinyAsciiStr<4> = tinystr!(4, "M03L");
const MONTH_FOUR: TinyAsciiStr<4> = tinystr!(4, "M04");
const MONTH_FOUR_LEAP: TinyAsciiStr<4> = tinystr!(4, "M04L");
const MONTH_FIVE: TinyAsciiStr<4> = tinystr!(4, "M05");
const MONTH_FIVE_LEAP: TinyAsciiStr<4> = tinystr!(4, "M05L");
const MONTH_SIX: TinyAsciiStr<4> = tinystr!(4, "M06");
const MONTH_SIX_LEAP: TinyAsciiStr<4> = tinystr!(4, "M06L");
const MONTH_SEVEN: TinyAsciiStr<4> = tinystr!(4, "M07");
const MONTH_SEVEN_LEAP: TinyAsciiStr<4> = tinystr!(4, "M07L");
const MONTH_EIGHT: TinyAsciiStr<4> = tinystr!(4, "M08");
const MONTH_EIGHT_LEAP: TinyAsciiStr<4> = tinystr!(4, "M08L");
const MONTH_NINE: TinyAsciiStr<4> = tinystr!(4, "M09");
const MONTH_NINE_LEAP: TinyAsciiStr<4> = tinystr!(4, "M09L");
const MONTH_TEN: TinyAsciiStr<4> = tinystr!(4, "M10");
const MONTH_TEN_LEAP: TinyAsciiStr<4> = tinystr!(4, "M10L");
const MONTH_ELEVEN: TinyAsciiStr<4> = tinystr!(4, "M11");
const MONTH_ELEVEN_LEAP: TinyAsciiStr<4> = tinystr!(4, "M11L");
const MONTH_TWELVE: TinyAsciiStr<4> = tinystr!(4, "M12");
const MONTH_TWELVE_LEAP: TinyAsciiStr<4> = tinystr!(4, "M12L");
const MONTH_THIRTEEN: TinyAsciiStr<4> = tinystr!(4, "M13");

// TODO: Handle instances where month values may be outside of valid
// bounds. In other words, it is totally possible for a value to be
// passed in that is { month: 300 } with overflow::constrain.
/// MonthCode struct v2
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct MonthCode(pub(crate) TinyAsciiStr<4>);

impl MonthCode {
    pub(crate) fn validate(&self, calendar: &Calendar) -> TemporalResult<()> {
        const COMMON_MONTH_CODES: [TinyAsciiStr<4>; 12] = [
            MONTH_ONE,
            MONTH_TWO,
            MONTH_THREE,
            MONTH_FOUR,
            MONTH_FIVE,
            MONTH_SIX,
            MONTH_SEVEN,
            MONTH_EIGHT,
            MONTH_NINE,
            MONTH_TEN,
            MONTH_ELEVEN,
            MONTH_TWELVE,
        ];

        const LUNAR_LEAP_MONTHS: [TinyAsciiStr<4>; 12] = [
            MONTH_ONE_LEAP,
            MONTH_TWO_LEAP,
            MONTH_THREE_LEAP,
            MONTH_FOUR_LEAP,
            MONTH_FIVE_LEAP,
            MONTH_SIX_LEAP,
            MONTH_SEVEN_LEAP,
            MONTH_EIGHT_LEAP,
            MONTH_NINE_LEAP,
            MONTH_TEN_LEAP,
            MONTH_ELEVEN_LEAP,
            MONTH_TWELVE_LEAP,
        ];

        if COMMON_MONTH_CODES.contains(&self.0) {
            return Ok(());
        }

        match calendar.identifier() {
            "chinese" | "dangi" if LUNAR_LEAP_MONTHS.contains(&self.0) => Ok(()),
            "coptic" | "ethiopic" | "ethiopicaa" if MONTH_THIRTEEN == self.0 => Ok(()),
            "hebrew" if MONTH_FIVE_LEAP == self.0 => Ok(()),
            _ => Err(TemporalError::range()
                .with_message("MonthCode was not valid for the current calendar.")),
        }
    }

    pub(crate) fn try_from_partial_date(partial_date: &PartialDate) -> TemporalResult<Self> {
        match partial_date {
            PartialDate {
                month: Some(month),
                month_code: None,
                calendar,
                ..
            } => {
                let month_code = month_to_month_code(*month)?;
                month_code.validate(calendar)?;
                Ok(month_code)
            }
            PartialDate {
                month_code: Some(month_code),
                month: None,
                calendar,
                ..
            } => {
                month_code.validate(calendar)?;
                Ok(*month_code)
            }
            PartialDate {
                month: Some(month),
                month_code: Some(month_code),
                calendar,
                ..
            } => {
                are_month_and_month_code_resolvable(*month, month_code)?;
                month_code.validate(calendar)?;
                Ok(*month_code)
            }
            _ => Err(TemporalError::r#type()
                .with_message("Month or monthCode is required to determine date.")),
        }
    }

    /// Returns the `MonthCode` as an integer
    pub fn to_month_integer(&self) -> u8 {
        ascii_four_to_integer(self.0)
    }

    /// Returns whether the `MonthCode` is a leap month.
    pub fn is_leap_month(&self) -> bool {
        let bytes = self.0.all_bytes();
        bytes[3] == b'L'
    }

    pub fn as_str(&self) -> &str {
        self.0.as_str()
    }

    pub fn as_tinystr(&self) -> TinyAsciiStr<4> {
        self.0
    }

    pub fn try_from_utf8(src: &[u8]) -> TemporalResult<Self> {
        if !(3..=4).contains(&src.len()) {
            return Err(TemporalError::range());
        }

        let inner = TinyAsciiStr::<4>::try_from_utf8(src).map_err(|_e| TemporalError::range())?;

        let bytes = inner.all_bytes();
        if bytes[0] != b'M' {
            return Err(
                TemporalError::range().with_message("First month code character must be 'M'")
            );
        }
        if !bytes[1].is_ascii_digit() || !bytes[2].is_ascii_digit() {
            return Err(TemporalError::range().with_message("Invalid month code digit"));
        }
        if src.len() == 4 && bytes[3] != b'L' {
            return Err(TemporalError::range().with_message("Leap month code must end with 'L'"));
        }

        Ok(Self(inner))
    }
}

impl core::str::FromStr for MonthCode {
    type Err = TemporalError;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::try_from_utf8(s.as_bytes())
    }
}

// NOTE: This is a greedy function, should handle differently for all calendars.
#[inline]
pub(crate) fn month_to_month_code(month: u8) -> TemporalResult<MonthCode> {
    if !(1..=13).contains(&month) {
        return Err(TemporalError::range().with_message("Month not in a valid range."));
    }
    let first = month / 10;
    let second = month % 10;
    let tinystr = TinyAsciiStr::<4>::try_from_raw([b'M', first + 48, second + 48, b'\0'])
        .map_err(|e| TemporalError::range().with_message(format!("tinystr error {e}")))?;
    Ok(MonthCode(tinystr))
}

#[inline]
fn are_month_and_month_code_resolvable(month: u8, mc: &MonthCode) -> TemporalResult<()> {
    if month != mc.to_month_integer() {
        return Err(TemporalError::range()
            .with_message("Month and monthCode values could not be resolved."));
    }
    Ok(())
}

// Potentially greedy. Need to verify for all calendars that
// the month code integer aligns with the month integer, which
// may require calendar info
#[inline]
pub(crate) fn ascii_four_to_integer(mc: TinyAsciiStr<4>) -> u8 {
    let bytes = mc.all_bytes();
    // Invariant: second and third character (index 1 and 2) are ascii digits.
    debug_assert!(bytes[1].is_ascii_digit());
    debug_assert!(bytes[2].is_ascii_digit());
    let first = ascii_digit_to_int(bytes[1]) * 10;
    first + ascii_digit_to_int(bytes[2])
}

#[inline]
const fn ascii_digit_to_int(ascii_digit: u8) -> u8 {
    ascii_digit - 48
}

fn resolve_iso_month(
    partial_date: &PartialDate,
    overflow: ArithmeticOverflow,
) -> TemporalResult<MonthCode> {
    let month_code = match (partial_date.month_code, partial_date.month) {
        (None, None) => {
            return Err(TemporalError::r#type().with_message("Month or monthCode must be provided."))
        }
        (None, Some(month)) => {
            if overflow == ArithmeticOverflow::Constrain {
                return month_to_month_code(month.clamp(1, 12));
            }
            if !(1..=12).contains(&month) {
                return Err(
                    TemporalError::range().with_message("month value is not in a valid range.")
                );
            }
            month_to_month_code(month)?
        }
        (Some(month_code), None) => month_code,
        (Some(month_code), Some(month)) => {
            if month != month_code.to_month_integer() {
                return Err(TemporalError::range()
                    .with_message("month and monthCode could not be resolved."));
            }
            month_code
        }
    };
    month_code.validate(&partial_date.calendar)?;
    Ok(month_code)
}

#[cfg(test)]
mod tests {
    use core::str::FromStr;

    use tinystr::tinystr;

    use crate::{
        builtins::{
            calendar::types::ResolutionType,
            core::{calendar::Calendar, PartialDate},
        },
        options::ArithmeticOverflow,
    };

    use super::{month_to_month_code, MonthCode, ResolvedCalendarFields};

    #[test]
    fn valid_month_code() {
        let month_code = MonthCode::from_str("M01").unwrap();
        assert!(!month_code.is_leap_month());
        assert_eq!(month_code.to_month_integer(), 1);

        let month_code = MonthCode::from_str("M12").unwrap();
        assert!(!month_code.is_leap_month());
        assert_eq!(month_code.to_month_integer(), 12);

        let month_code = MonthCode::from_str("M13L").unwrap();
        assert!(month_code.is_leap_month());
        assert_eq!(month_code.to_month_integer(), 13);
    }

    #[test]
    fn invalid_month_code() {
        let _ = MonthCode::from_str("01").unwrap_err();
        let _ = MonthCode::from_str("N01").unwrap_err();
        let _ = MonthCode::from_str("M01R").unwrap_err();
        let _ = MonthCode::from_str("M1").unwrap_err();
        let _ = MonthCode::from_str("M1L").unwrap_err();
    }

    #[test]
    fn month_to_mc() {
        let mc = month_to_month_code(1).unwrap();
        assert_eq!(mc.as_str(), "M01");

        let mc = month_to_month_code(13).unwrap();
        assert_eq!(mc.as_str(), "M13");

        let _ = month_to_month_code(0).unwrap_err();
        let _ = month_to_month_code(14).unwrap_err();
    }

    #[test]
    fn day_overflow_test() {
        let bad_fields = PartialDate {
            year: Some(2019),
            month: Some(1),
            day: Some(32),
            ..Default::default()
        };

        let cal = Calendar::default();

        let err = cal.date_from_partial(&bad_fields, ArithmeticOverflow::Reject);
        assert!(err.is_err());
        let result = cal.date_from_partial(&bad_fields, ArithmeticOverflow::Constrain);
        assert!(result.is_ok());
    }

    #[test]
    fn unresolved_month_and_month_code() {
        let bad_fields = PartialDate {
            year: Some(1976),
            month: Some(11),
            month_code: Some(MonthCode(tinystr!(4, "M12"))),
            day: Some(18),
            ..Default::default()
        };

        let err = ResolvedCalendarFields::try_from_partial(
            &bad_fields,
            ArithmeticOverflow::Reject,
            ResolutionType::Date,
        );
        assert!(err.is_err());
    }

    #[test]
    fn missing_partial_fields() {
        let bad_fields = PartialDate {
            year: Some(2019),
            day: Some(19),
            ..Default::default()
        };

        let err = ResolvedCalendarFields::try_from_partial(
            &bad_fields,
            ArithmeticOverflow::Reject,
            ResolutionType::Date,
        );
        assert!(err.is_err());

        let bad_fields = PartialDate::default();
        let err = ResolvedCalendarFields::try_from_partial(
            &bad_fields,
            ArithmeticOverflow::Reject,
            ResolutionType::Date,
        );
        assert!(err.is_err());
    }
}
