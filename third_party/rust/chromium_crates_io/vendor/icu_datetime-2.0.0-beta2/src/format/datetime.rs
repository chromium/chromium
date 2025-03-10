// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::time_zone::{FormatTimeZone, FormatTimeZoneError, Iso8601Format, TimeZoneFormatterUnit};
use crate::error::{DateTimeWriteError, ErrorField};
use crate::format::ExtractedInput;
use crate::provider::fields::{self, FieldLength, FieldSymbol, Second, Year};
use crate::provider::pattern::runtime::PatternMetadata;
use crate::provider::pattern::PatternItem;
use crate::{parts, pattern::*};

use core::fmt::{self, Write};
use fixed_decimal::Decimal;
use icu_calendar::types::{DayOfWeekInMonth, Weekday};
use icu_decimal::DecimalFormatter;
use writeable::{Part, PartsWrite, Writeable};

/// Apply length to input number and write to result using decimal_formatter.
fn try_write_number<W>(
    part: Part,
    w: &mut W,
    decimal_formatter: Option<&DecimalFormatter>,
    mut num: Decimal,
    length: FieldLength,
) -> Result<Result<(), DateTimeWriteError>, fmt::Error>
where
    W: writeable::PartsWrite + ?Sized,
{
    num.pad_start(length.to_len() as i16);

    if let Some(fdf) = decimal_formatter {
        w.with_part(part, |w| fdf.format(&num).write_to_parts(w))?;
        Ok(Ok(()))
    } else {
        w.with_part(part, |w| {
            w.with_part(writeable::Part::ERROR, |r| num.write_to_parts(r))
        })?;
        Ok(Err(DateTimeWriteError::DecimalFormatterNotLoaded))
    }
}

/// Apply length to input number and write to result using decimal_formatter.
/// Don't annotate it with a part.
fn try_write_number_without_part<W>(
    w: &mut W,
    decimal_formatter: Option<&DecimalFormatter>,
    mut num: Decimal,
    length: FieldLength,
) -> Result<Result<(), DateTimeWriteError>, fmt::Error>
where
    W: writeable::PartsWrite + ?Sized,
{
    num.pad_start(length.to_len() as i16);

    if let Some(fdf) = decimal_formatter {
        fdf.format(&num).write_to(w)?;
        Ok(Ok(()))
    } else {
        w.with_part(writeable::Part::ERROR, |r| num.write_to(r))?;
        Ok(Err(DateTimeWriteError::DecimalFormatterNotLoaded))
    }
}

#[allow(clippy::too_many_arguments)]
pub(crate) fn try_write_pattern_items<W>(
    pattern_metadata: PatternMetadata,
    pattern_items: impl Iterator<Item = PatternItem>,
    input: &ExtractedInput,
    datetime_names: &RawDateTimeNamesBorrowed,
    decimal_formatter: Option<&DecimalFormatter>,
    w: &mut W,
) -> Result<Result<(), DateTimeWriteError>, fmt::Error>
where
    W: writeable::PartsWrite + ?Sized,
{
    let mut r = Ok(());
    for item in pattern_items {
        match item {
            PatternItem::Literal(ch) => w.write_char(ch)?,
            PatternItem::Field(field) => {
                r = r.and(try_write_field(
                    field,
                    pattern_metadata,
                    input,
                    datetime_names,
                    decimal_formatter,
                    w,
                )?);
            }
        }
    }
    Ok(r)
}

// This function assumes that the correct decision has been
// made regarding availability of symbols in the caller.
//
// When modifying the list of fields using symbols,
// update the matching query in `analyze_pattern` function.
fn try_write_field<W>(
    field: fields::Field,
    pattern_metadata: PatternMetadata,
    input: &ExtractedInput,
    datetime_names: &RawDateTimeNamesBorrowed,
    decimal_formatter: Option<&DecimalFormatter>,
    w: &mut W,
) -> Result<Result<(), DateTimeWriteError>, fmt::Error>
where
    W: writeable::PartsWrite + ?Sized,
{
    macro_rules! input {
        // Get the input. If not found, write a replacement string but do NOT write a part.
        (_, $name:ident = $input:expr) => {
            let Some($name) = $input else {
                write_value_missing(w, field)?;
                return Ok(Err(DateTimeWriteError::MissingInputField(stringify!(
                    $name
                ))));
            };
        };
        // Get the input. If not found, write a replacement string and a part.
        ($part:ident, $name:ident = $input:expr) => {
            let Some($name) = $input else {
                w.with_part($part, |w| write_value_missing(w, field))?;
                return Ok(Err(DateTimeWriteError::MissingInputField(stringify!(
                    $name
                ))));
            };
        };
    }

    Ok(match (field.symbol, field.length) {
        (FieldSymbol::Era, l) => {
            const PART: Part = parts::ERA;
            input!(PART, year = input.year);
            input!(PART, era = year.formatting_era());
            let era_symbol = datetime_names
                .get_name_for_era(l, era)
                .map_err(|e| match e {
                    GetNameForEraError::InvalidEraCode => DateTimeWriteError::InvalidEra(era),
                    GetNameForEraError::InvalidFieldLength => {
                        DateTimeWriteError::UnsupportedLength(ErrorField(field))
                    }
                    GetNameForEraError::NotLoaded => {
                        DateTimeWriteError::NamesNotLoaded(ErrorField(field))
                    }
                });
            match era_symbol {
                Err(e) => {
                    w.with_part(PART, |w| {
                        w.with_part(Part::ERROR, |w| w.write_str(&era.fallback_name()))
                    })?;
                    Err(e)
                }
                Ok(era) => Ok(w.with_part(PART, |w| w.write_str(era))?),
            }
        }
        (FieldSymbol::Year(Year::Calendar), l) => {
            const PART: Part = parts::YEAR;
            input!(PART, year = input.year);
            let mut year = Decimal::from(year.era_year_or_extended());
            if matches!(l, FieldLength::Two) {
                // 'yy' and 'YY' truncate
                year.set_max_position(2);
            }
            try_write_number(PART, w, decimal_formatter, year, l)?
        }
        (FieldSymbol::Year(Year::Cyclic), l) => {
            const PART: Part = parts::YEAR_NAME;
            input!(PART, year = input.year);
            input!(PART, cyclic = year.cyclic());

            match datetime_names.get_name_for_cyclic(l, cyclic) {
                Ok(name) => Ok(w.with_part(PART, |w| w.write_str(name))?),
                Err(e) => {
                    w.with_part(PART, |w| {
                        w.with_part(Part::ERROR, |w| {
                            try_write_number_without_part(
                                w,
                                decimal_formatter,
                                year.era_year_or_extended().into(),
                                FieldLength::One,
                            )
                            .map(|_| ())
                        })
                    })?;
                    return Ok(Err(match e {
                        GetNameForCyclicYearError::InvalidYearNumber { max } => {
                            DateTimeWriteError::InvalidCyclicYear {
                                value: cyclic.get() as usize,
                                max,
                            }
                        }
                        GetNameForCyclicYearError::InvalidFieldLength => {
                            DateTimeWriteError::UnsupportedLength(ErrorField(field))
                        }
                        GetNameForCyclicYearError::NotLoaded => {
                            DateTimeWriteError::NamesNotLoaded(ErrorField(field))
                        }
                    }));
                }
            }
        }
        (FieldSymbol::Year(Year::RelatedIso), l) => {
            const PART: Part = parts::RELATED_YEAR;
            input!(PART, year = input.year);
            input!(PART, related_iso = year.related_iso());

            // Always in latin digits according to spec
            w.with_part(PART, |w| {
                let mut num = Decimal::from(related_iso);
                num.pad_start(l.to_len() as i16);
                num.write_to(w)
            })?;
            Ok(())
        }
        (FieldSymbol::Month(_), l @ (FieldLength::One | FieldLength::Two)) => {
            const PART: Part = parts::MONTH;
            input!(PART, month = input.month);
            try_write_number(PART, w, decimal_formatter, month.ordinal.into(), l)?
        }
        (FieldSymbol::Month(symbol), l) => {
            const PART: Part = parts::MONTH;
            input!(PART, month = input.month);
            match datetime_names.get_name_for_month(symbol, l, month.formatting_code) {
                Ok(MonthPlaceholderValue::PlainString(symbol)) => {
                    w.with_part(PART, |w| w.write_str(symbol))?;
                    Ok(())
                }
                Ok(MonthPlaceholderValue::Numeric) => {
                    debug_assert!(l == FieldLength::One);
                    try_write_number(PART, w, decimal_formatter, month.ordinal.into(), l)?
                }
                Ok(MonthPlaceholderValue::NumericPattern(substitution_pattern)) => {
                    debug_assert!(l == FieldLength::One);
                    if let Some(formatter) = decimal_formatter {
                        let mut num = Decimal::from(month.ordinal);
                        num.pad_start(l.to_len() as i16);
                        w.with_part(PART, |w| {
                            substitution_pattern
                                .interpolate([formatter.format(&num)])
                                .write_to(w)
                        })?;
                        Ok(())
                    } else {
                        w.with_part(PART, |w| {
                            w.with_part(Part::ERROR, |w| {
                                substitution_pattern
                                    .interpolate([{
                                        let mut num = Decimal::from(month.ordinal);
                                        num.pad_start(l.to_len() as i16);
                                        num
                                    }])
                                    .write_to(w)
                            })
                        })?;
                        Err(DateTimeWriteError::DecimalFormatterNotLoaded)
                    }
                }
                Err(e) => {
                    w.with_part(PART, |w| {
                        w.with_part(Part::ERROR, |w| w.write_str(&month.formatting_code.0))
                    })?;
                    Err(match e {
                        GetNameForMonthError::InvalidMonthCode => {
                            DateTimeWriteError::InvalidMonthCode(month.formatting_code)
                        }
                        GetNameForMonthError::InvalidFieldLength => {
                            DateTimeWriteError::UnsupportedLength(ErrorField(field))
                        }
                        GetNameForMonthError::NotLoaded => {
                            DateTimeWriteError::NamesNotLoaded(ErrorField(field))
                        }
                    })
                }
            }
        }
        (FieldSymbol::Week(w), _) => match w {},
        (FieldSymbol::Weekday(weekday), l) => {
            const PART: Part = parts::WEEKDAY;
            input!(PART, iso_weekday = input.iso_weekday);
            match datetime_names
                .get_name_for_weekday(weekday, l, iso_weekday)
                .map_err(|e| match e {
                    GetNameForWeekdayError::InvalidFieldLength => {
                        DateTimeWriteError::UnsupportedLength(ErrorField(field))
                    }
                    GetNameForWeekdayError::NotLoaded => {
                        DateTimeWriteError::NamesNotLoaded(ErrorField(field))
                    }
                }) {
                Err(e) => {
                    w.with_part(PART, |w| {
                        w.with_part(Part::ERROR, |w| {
                            w.write_str(match iso_weekday {
                                Weekday::Monday => "mon",
                                Weekday::Tuesday => "tue",
                                Weekday::Wednesday => "wed",
                                Weekday::Thursday => "thu",
                                Weekday::Friday => "fri",
                                Weekday::Saturday => "sat",
                                Weekday::Sunday => "sun",
                            })
                        })
                    })?;
                    Err(e)
                }
                Ok(s) => Ok(w.with_part(PART, |w| w.write_str(s))?),
            }
        }
        (FieldSymbol::Day(fields::Day::DayOfMonth), l) => {
            const PART: Part = parts::DAY;
            input!(PART, day_of_month = input.day_of_month);
            try_write_number(PART, w, decimal_formatter, day_of_month.0.into(), l)?
        }
        (FieldSymbol::Day(fields::Day::DayOfWeekInMonth), l) => {
            input!(_, day_of_month = input.day_of_month);
            try_write_number_without_part(
                w,
                decimal_formatter,
                DayOfWeekInMonth::from(day_of_month).0.into(),
                l,
            )?
        }
        (FieldSymbol::Day(fields::Day::DayOfYear), l) => {
            input!(_, day_of_year = input.day_of_year);
            try_write_number_without_part(w, decimal_formatter, day_of_year.day_of_year.into(), l)?
        }
        (FieldSymbol::Hour(symbol), l) => {
            const PART: Part = parts::HOUR;
            input!(PART, hour = input.hour);
            let h = hour.number();
            let h = match symbol {
                fields::Hour::H11 => h % 12,
                fields::Hour::H12 => {
                    let v = h % 12;
                    if v == 0 {
                        12
                    } else {
                        v
                    }
                }
                fields::Hour::H23 => h,
                fields::Hour::H24 => {
                    if h == 0 {
                        24
                    } else {
                        h
                    }
                }
            };
            try_write_number(PART, w, decimal_formatter, h.into(), l)?
        }
        (FieldSymbol::Minute, l) => {
            const PART: Part = parts::MINUTE;
            input!(PART, minute = input.minute);
            try_write_number(PART, w, decimal_formatter, minute.number().into(), l)?
        }
        (FieldSymbol::Second(Second::Second), l) => {
            const PART: Part = parts::SECOND;
            input!(PART, second = input.second);
            try_write_number(PART, w, decimal_formatter, second.number().into(), l)?
        }
        (FieldSymbol::Second(Second::MillisInDay), l) => {
            input!(_, hour = input.hour);
            input!(_, minute = input.minute);
            input!(_, second = input.second);
            input!(_, subsecond = input.subsecond);

            let milliseconds = (((hour.number() as u32 * 60) + minute.number() as u32) * 60
                + second.number() as u32)
                * 1000
                + subsecond.number() / 1_000_000;
            try_write_number_without_part(w, decimal_formatter, milliseconds.into(), l)?
        }
        (FieldSymbol::DecimalSecond(decimal_second), l) => {
            const PART: Part = parts::SECOND;
            input!(PART, second = input.second);
            input!(PART, subsecond = input.subsecond);

            // Formatting with fractional seconds
            let mut s = Decimal::from(second.number());
            let _infallible = s.concatenate_end(
                Decimal::from(subsecond.number())
                    .absolute
                    .multiplied_pow10(-9),
            );
            debug_assert!(_infallible.is_ok());
            let position = -(decimal_second as i16);
            s.trunc(position);
            s.pad_end(position);
            try_write_number(PART, w, decimal_formatter, s, l)?
        }
        (FieldSymbol::DayPeriod(period), l) => {
            const PART: Part = parts::DAY_PERIOD;
            input!(PART, hour = input.hour);

            match datetime_names.get_name_for_day_period(
                period,
                l,
                hour,
                pattern_metadata.time_granularity().is_top_of_hour(
                    input.minute.unwrap_or_default().number(),
                    input.second.unwrap_or_default().number(),
                    input.subsecond.unwrap_or_default().number(),
                ),
            ) {
                Err(e) => {
                    w.with_part(PART, |w| {
                        w.with_part(Part::ERROR, |w| {
                            w.write_str(if hour.number() < 12 { "AM" } else { "PM" })
                        })
                    })?;
                    Err(match e {
                        GetNameForDayPeriodError::InvalidFieldLength => {
                            DateTimeWriteError::UnsupportedLength(ErrorField(field))
                        }
                        GetNameForDayPeriodError::NotLoaded => {
                            DateTimeWriteError::NamesNotLoaded(ErrorField(field))
                        }
                    })
                }
                Ok(s) => Ok(w.with_part(PART, |w| w.write_str(s))?),
            }
        }
        (FieldSymbol::TimeZone(fields::TimeZone::SpecificNonLocation), FieldLength::Four) => {
            perform_timezone_fallback(
                w,
                input,
                datetime_names,
                decimal_formatter,
                field,
                &[
                    TimeZoneFormatterUnit::SpecificNonLocation(FieldLength::Four),
                    TimeZoneFormatterUnit::SpecificLocation,
                    TimeZoneFormatterUnit::LocalizedOffset(FieldLength::Four),
                ],
            )?
        }
        (FieldSymbol::TimeZone(fields::TimeZone::SpecificNonLocation), l) => {
            perform_timezone_fallback(
                w,
                input,
                datetime_names,
                decimal_formatter,
                field,
                &[
                    TimeZoneFormatterUnit::SpecificNonLocation(l),
                    TimeZoneFormatterUnit::LocalizedOffset(l),
                ],
            )?
        }
        (FieldSymbol::TimeZone(fields::TimeZone::GenericNonLocation), l) => {
            perform_timezone_fallback(
                w,
                input,
                datetime_names,
                decimal_formatter,
                field,
                &[
                    TimeZoneFormatterUnit::GenericNonLocation(l),
                    TimeZoneFormatterUnit::GenericLocation,
                    TimeZoneFormatterUnit::LocalizedOffset(l),
                ],
            )?
        }
        (FieldSymbol::TimeZone(fields::TimeZone::Location), FieldLength::Four) => {
            perform_timezone_fallback(
                w,
                input,
                datetime_names,
                decimal_formatter,
                field,
                &[
                    TimeZoneFormatterUnit::GenericLocation,
                    TimeZoneFormatterUnit::LocalizedOffset(FieldLength::Four),
                ],
            )?
        }
        (FieldSymbol::TimeZone(fields::TimeZone::Location), FieldLength::Three) => {
            perform_timezone_fallback(
                w,
                input,
                datetime_names,
                decimal_formatter,
                field,
                &[TimeZoneFormatterUnit::ExemplarCity],
            )?
        }
        (FieldSymbol::TimeZone(fields::TimeZone::LocalizedOffset), l) => perform_timezone_fallback(
            w,
            input,
            datetime_names,
            decimal_formatter,
            field,
            &[TimeZoneFormatterUnit::LocalizedOffset(l)],
        )?,
        (FieldSymbol::TimeZone(fields::TimeZone::Location), _) => perform_timezone_fallback(
            w,
            input,
            datetime_names,
            decimal_formatter,
            field,
            &[TimeZoneFormatterUnit::Bcp47Id],
        )?,
        (FieldSymbol::TimeZone(fields::TimeZone::IsoWithZ), l) => perform_timezone_fallback(
            w,
            input,
            datetime_names,
            decimal_formatter,
            field,
            &[TimeZoneFormatterUnit::Iso8601(Iso8601Format::with_z(l))],
        )?,
        (FieldSymbol::TimeZone(fields::TimeZone::Iso), l) => perform_timezone_fallback(
            w,
            input,
            datetime_names,
            decimal_formatter,
            field,
            &[TimeZoneFormatterUnit::Iso8601(Iso8601Format::without_z(l))],
        )?,
    })
}

// Writes an error string for the given symbol
fn write_value_missing(
    w: &mut (impl writeable::PartsWrite + ?Sized),
    field: fields::Field,
) -> Result<(), fmt::Error> {
    w.with_part(Part::ERROR, |w| {
        "{".write_to(w)?;
        char::from(field.symbol).write_to(w)?;
        "}".write_to(w)
    })
}

fn perform_timezone_fallback(
    w: &mut (impl writeable::PartsWrite + ?Sized),
    input: &ExtractedInput,
    datetime_names: &RawDateTimeNamesBorrowed,
    fdf: Option<&DecimalFormatter>,
    field: fields::Field,
    units: &[TimeZoneFormatterUnit],
) -> Result<Result<(), DateTimeWriteError>, core::fmt::Error> {
    const PART: Part = parts::TIME_ZONE_NAME;
    let payloads = datetime_names.get_payloads();
    let mut r = Err(FormatTimeZoneError::Fallback);
    for unit in units {
        let mut inner_result = None;
        w.with_part(PART, |w| {
            inner_result = Some(unit.format(w, input, payloads, fdf)?);
            Ok(())
        })?;
        match inner_result {
            Some(Err(FormatTimeZoneError::Fallback)) => {
                // Expected, try the next unit
                continue;
            }
            Some(r2) => {
                r = r2;
                break;
            }
            None => {
                debug_assert!(false, "unreachable");
                return Err(fmt::Error);
            }
        }
    }

    Ok(match r {
        Ok(()) => Ok(()),
        Err(e) => {
            if let Some(offset) = input.offset {
                w.with_part(PART, |w| {
                    w.with_part(Part::ERROR, |w| {
                        Iso8601Format::without_z(field.length).format_infallible(w, offset)
                    })
                })?;
            } else {
                w.with_part(PART, |w| write_value_missing(w, field))?;
            }
            match e {
                FormatTimeZoneError::DecimalFormatterNotLoaded => {
                    Err(DateTimeWriteError::DecimalFormatterNotLoaded)
                }
                FormatTimeZoneError::NamesNotLoaded => {
                    Err(DateTimeWriteError::NamesNotLoaded(ErrorField(field)))
                }
                FormatTimeZoneError::MissingInputField(f) => {
                    Err(DateTimeWriteError::MissingInputField(f))
                }
                FormatTimeZoneError::Fallback => {
                    debug_assert!(false, "timezone fallback chain fell through {input:?}");
                    Ok(())
                }
            }
        }
    })
}

#[cfg(test)]
#[allow(unused_imports)]
#[cfg(feature = "compiled_data")]
mod tests {
    use super::*;
    use icu_decimal::options::{DecimalFormatterOptions, GroupingStrategy};

    #[test]
    fn test_format_number() {
        let values = &[2, 20, 201, 2017, 20173];
        let samples = &[
            (FieldLength::One, ["2", "20", "201", "2017", "20173"]),
            (FieldLength::Two, ["02", "20", "201", "2017", "20173"]),
            (FieldLength::Three, ["002", "020", "201", "2017", "20173"]),
            (FieldLength::Four, ["0002", "0020", "0201", "2017", "20173"]),
        ];

        let mut decimal_formatter_options = DecimalFormatterOptions::default();
        decimal_formatter_options.grouping_strategy = Some(GroupingStrategy::Never);
        let decimal_formatter = DecimalFormatter::try_new(
            icu_locale_core::locale!("en").into(),
            decimal_formatter_options,
        )
        .unwrap();

        for (length, expected) in samples {
            for (value, expected) in values.iter().zip(expected) {
                let mut s = String::new();
                try_write_number_without_part(
                    &mut writeable::adapters::CoreWriteAsPartsWrite(&mut s),
                    Some(&decimal_formatter),
                    Decimal::from(*value),
                    *length,
                )
                .unwrap()
                .unwrap();
                assert_eq!(s, *expected);
            }
        }
    }
}
