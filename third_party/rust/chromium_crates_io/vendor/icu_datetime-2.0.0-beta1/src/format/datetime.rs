// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::time_zone::{FormatTimeZone, FormatTimeZoneError, Iso8601Format, TimeZoneFormatterUnit};
use crate::error::DateTimeWriteError;
use crate::fields::{self, FieldLength, FieldSymbol, Second, Year};
use crate::input::ExtractedInput;
use crate::pattern::*;
use crate::provider::pattern::runtime::PatternMetadata;
use crate::provider::pattern::PatternItem;

use core::fmt::{self, Write};
use fixed_decimal::FixedDecimal;
use icu_calendar::types::{DayOfWeekInMonth, IsoWeekday};
use icu_decimal::FixedDecimalFormatter;
use writeable::{Part, Writeable};

/// Apply length to input number and write to result using fixed_decimal_format.
fn try_write_number<W>(
    result: &mut W,
    fixed_decimal_format: Option<&FixedDecimalFormatter>,
    mut num: FixedDecimal,
    length: FieldLength,
) -> Result<Result<(), DateTimeWriteError>, fmt::Error>
where
    W: writeable::PartsWrite + ?Sized,
{
    num.pad_start(length.to_len() as i16);

    if let Some(fdf) = fixed_decimal_format {
        fdf.format(&num).write_to(result)?;
        Ok(Ok(()))
    } else {
        result.with_part(writeable::Part::ERROR, |r| num.write_to(r))?;
        Ok(Err(DateTimeWriteError::FixedDecimalFormatterNotLoaded))
    }
}

#[allow(clippy::too_many_arguments)]
pub(crate) fn try_write_pattern_items<W>(
    pattern_metadata: PatternMetadata,
    pattern_items: impl Iterator<Item = PatternItem>,
    input: &ExtractedInput,
    datetime_names: &RawDateTimeNamesBorrowed,
    fixed_decimal_format: Option<&FixedDecimalFormatter>,
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
                    fixed_decimal_format,
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
    fdf: Option<&FixedDecimalFormatter>,
    w: &mut W,
) -> Result<Result<(), DateTimeWriteError>, fmt::Error>
where
    W: writeable::PartsWrite + ?Sized,
{
    macro_rules! input {
        ($name:ident = $input:expr) => {
            let Some($name) = $input else {
                write_value_missing(w, field)?;
                return Ok(Err(DateTimeWriteError::MissingInputField(stringify!(
                    $name
                ))));
            };
        };
    }

    Ok(match (field.symbol, field.length) {
        (FieldSymbol::Era, l) => {
            input!(year = input.year);
            input!(era = year.formatting_era());
            let era_symbol = datetime_names
                .get_name_for_era(l, era)
                .map_err(|e| match e {
                    GetSymbolForEraError::Invalid => DateTimeWriteError::InvalidEra(era),
                    GetSymbolForEraError::NotLoaded => DateTimeWriteError::NamesNotLoaded(field),
                });
            match era_symbol {
                Err(e) => {
                    w.with_part(Part::ERROR, |w| w.write_str(&era.fallback_name()))?;
                    Err(e)
                }
                Ok(era) => Ok(w.write_str(era)?),
            }
        }
        (FieldSymbol::Year(Year::Calendar), l) => {
            input!(year = input.year);
            let mut year = FixedDecimal::from(year.era_year_or_extended());
            if matches!(l, FieldLength::Two) {
                // 'yy' and 'YY' truncate
                year.set_max_position(2);
            }
            try_write_number(w, fdf, year, l)?
        }
        (FieldSymbol::Year(Year::Cyclic), l) => {
            input!(year = input.year);
            input!(cyclic = year.cyclic());

            match datetime_names.get_name_for_cyclic(l, cyclic) {
                Ok(name) => Ok(w.write_str(name)?),
                Err(e) => {
                    w.with_part(Part::ERROR, |w| {
                        try_write_number(
                            w,
                            fdf,
                            year.era_year_or_extended().into(),
                            FieldLength::One,
                        )
                        .map(|_| ())
                    })?;
                    return Ok(Err(match e {
                        GetSymbolForCyclicYearError::Invalid { max } => {
                            DateTimeWriteError::InvalidCyclicYear {
                                value: cyclic.get() as usize,
                                max,
                            }
                        }
                        GetSymbolForCyclicYearError::NotLoaded => {
                            DateTimeWriteError::NamesNotLoaded(field)
                        }
                    }));
                }
            }
        }
        (FieldSymbol::Year(Year::RelatedIso), l) => {
            input!(year = input.year);
            input!(related_iso = year.related_iso());

            // Always in latin digits according to spec
            FixedDecimal::from(related_iso)
                .padded_start(l.to_len() as i16)
                .write_to(w)?;
            Ok(())
        }
        (FieldSymbol::Month(_), l @ (FieldLength::One | FieldLength::Two)) => {
            input!(month = input.month);
            try_write_number(w, fdf, month.ordinal.into(), l)?
        }
        (FieldSymbol::Month(symbol), l) => {
            input!(month = input.month);
            match datetime_names.get_name_for_month(symbol, l, month.formatting_code) {
                Ok(MonthPlaceholderValue::PlainString(symbol)) => {
                    w.write_str(symbol)?;
                    Ok(())
                }
                Ok(MonthPlaceholderValue::Numeric) => {
                    debug_assert!(l == FieldLength::One);
                    try_write_number(w, fdf, month.ordinal.into(), l)?
                }
                Ok(MonthPlaceholderValue::NumericPattern(substitution_pattern)) => {
                    debug_assert!(l == FieldLength::One);
                    if let Some(fdf) = fdf {
                        substitution_pattern
                            .interpolate([fdf.format(
                                &FixedDecimal::from(month.ordinal).padded_start(l.to_len() as i16),
                            )])
                            .write_to(w)?;
                        Ok(())
                    } else {
                        w.with_part(Part::ERROR, |w| {
                            substitution_pattern
                                .interpolate([FixedDecimal::from(month.ordinal)
                                    .padded_start(l.to_len() as i16)])
                                .write_to(w)
                        })?;
                        Err(DateTimeWriteError::FixedDecimalFormatterNotLoaded)
                    }
                }
                Err(e) => {
                    w.with_part(Part::ERROR, |w| w.write_str(&month.formatting_code.0))?;
                    Err(match e {
                        GetNameForMonthError::Invalid => {
                            DateTimeWriteError::InvalidMonthCode(month.formatting_code)
                        }
                        GetNameForMonthError::NotLoaded => {
                            DateTimeWriteError::NamesNotLoaded(field)
                        }
                    })
                }
            }
        }
        (FieldSymbol::Week(w), _) => match w {},
        (FieldSymbol::Weekday(weekday), l) => {
            input!(iso_weekday = input.iso_weekday);
            match datetime_names
                .get_name_for_weekday(weekday, l, iso_weekday)
                .map_err(|e| match e {
                    GetNameForWeekdayError::NotLoaded => DateTimeWriteError::NamesNotLoaded(field),
                }) {
                Err(e) => {
                    w.with_part(Part::ERROR, |w| {
                        w.write_str(match iso_weekday {
                            IsoWeekday::Monday => "mon",
                            IsoWeekday::Tuesday => "tue",
                            IsoWeekday::Wednesday => "wed",
                            IsoWeekday::Thursday => "thu",
                            IsoWeekday::Friday => "fri",
                            IsoWeekday::Saturday => "sat",
                            IsoWeekday::Sunday => "sun",
                        })
                    })?;
                    Err(e)
                }
                Ok(s) => Ok(w.write_str(s)?),
            }
        }
        (FieldSymbol::Day(fields::Day::DayOfMonth), l) => {
            input!(day_of_month = input.day_of_month);
            try_write_number(w, fdf, day_of_month.0.into(), l)?
        }
        (FieldSymbol::Day(fields::Day::DayOfWeekInMonth), l) => {
            input!(day_of_month = input.day_of_month);
            try_write_number(w, fdf, DayOfWeekInMonth::from(day_of_month).0.into(), l)?
        }
        (FieldSymbol::Day(fields::Day::DayOfYear), l) => {
            input!(day_of_year = input.day_of_year);
            try_write_number(w, fdf, day_of_year.day_of_year.into(), l)?
        }
        (FieldSymbol::Hour(symbol), l) => {
            input!(hour = input.hour);
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
            try_write_number(w, fdf, h.into(), l)?
        }
        (FieldSymbol::Minute, l) => {
            input!(minute = input.minute);
            try_write_number(w, fdf, minute.number().into(), l)?
        }
        (FieldSymbol::Second(Second::Second), l) => {
            input!(second = input.second);
            try_write_number(w, fdf, second.number().into(), l)?
        }
        (FieldSymbol::Second(Second::MillisInDay), l) => {
            input!(hour = input.hour);
            input!(minute = input.minute);
            input!(second = input.second);
            input!(nanosecond = input.nanosecond);

            let milliseconds = (((hour.number() as u32 * 60) + minute.number() as u32) * 60
                + second.number() as u32)
                * 1000
                + nanosecond.number() / 1_000_000;
            try_write_number(w, fdf, milliseconds.into(), l)?
        }
        (FieldSymbol::DecimalSecond(decimal_second), l) => {
            input!(second = input.second);
            input!(nanosecond = input.nanosecond);

            // Formatting with fractional seconds
            let mut s = FixedDecimal::from(second.number());
            let _infallible =
                s.concatenate_end(FixedDecimal::from(nanosecond.number()).multiplied_pow10(-9));
            debug_assert!(_infallible.is_ok());
            let position = -(decimal_second as i16);
            s.trunc(position);
            s.pad_end(position);
            try_write_number(w, fdf, s, l)?
        }
        (FieldSymbol::DayPeriod(period), l) => {
            input!(hour = input.hour);

            match datetime_names.get_name_for_day_period(
                period,
                l,
                hour,
                pattern_metadata.time_granularity().is_top_of_hour(
                    input.minute.unwrap_or_default().number(),
                    input.second.unwrap_or_default().number(),
                    input.nanosecond.unwrap_or_default().number(),
                ),
            ) {
                Err(e) => {
                    w.with_part(Part::ERROR, |w| {
                        w.write_str(if hour.number() < 12 { "AM" } else { "PM" })
                    })?;
                    Err(match e {
                        GetNameForDayPeriodError::NotLoaded => {
                            DateTimeWriteError::NamesNotLoaded(field)
                        }
                    })
                }
                Ok(s) => Ok(w.write_str(s)?),
            }
        }
        (FieldSymbol::TimeZone(fields::TimeZone::SpecificNonLocation), FieldLength::Four) => {
            perform_timezone_fallback(
                w,
                input,
                datetime_names,
                fdf,
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
                fdf,
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
                fdf,
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
                fdf,
                field,
                &[
                    TimeZoneFormatterUnit::GenericLocation,
                    TimeZoneFormatterUnit::LocalizedOffset(FieldLength::Four),
                ],
            )?
        }
        (FieldSymbol::TimeZone(fields::TimeZone::LocalizedOffset), l) => perform_timezone_fallback(
            w,
            input,
            datetime_names,
            fdf,
            field,
            &[TimeZoneFormatterUnit::LocalizedOffset(l)],
        )?,
        (FieldSymbol::TimeZone(fields::TimeZone::Location), _) => perform_timezone_fallback(
            w,
            input,
            datetime_names,
            fdf,
            field,
            &[TimeZoneFormatterUnit::Bcp47Id],
        )?,
        (FieldSymbol::TimeZone(fields::TimeZone::IsoWithZ), l) => perform_timezone_fallback(
            w,
            input,
            datetime_names,
            fdf,
            field,
            &[TimeZoneFormatterUnit::Iso8601(Iso8601Format::with_z(l))],
        )?,
        (FieldSymbol::TimeZone(fields::TimeZone::Iso), l) => perform_timezone_fallback(
            w,
            input,
            datetime_names,
            fdf,
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
    fdf: Option<&FixedDecimalFormatter>,
    field: fields::Field,
    units: &[TimeZoneFormatterUnit],
) -> Result<Result<(), DateTimeWriteError>, core::fmt::Error> {
    let payloads = datetime_names.get_payloads();
    let mut r = Err(FormatTimeZoneError::Fallback);
    for unit in units {
        match unit.format(w, input, payloads, fdf)? {
            Err(FormatTimeZoneError::Fallback) => {
                // Expected, try the next unit
                continue;
            }
            r2 => {
                r = r2;
                break;
            }
        }
    }

    Ok(match r {
        Ok(()) => Ok(()),
        Err(e) => {
            if let Some(offset) = input.offset {
                w.with_part(Part::ERROR, |w| {
                    Iso8601Format::without_z(field.length).format_infallible(w, offset)
                })?;
            } else {
                write_value_missing(w, field)?;
            }
            match e {
                FormatTimeZoneError::FixedDecimalFormatterNotLoaded => {
                    Err(DateTimeWriteError::FixedDecimalFormatterNotLoaded)
                }
                FormatTimeZoneError::NamesNotLoaded => {
                    Err(DateTimeWriteError::NamesNotLoaded(field))
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
    use icu_decimal::options::{FixedDecimalFormatterOptions, GroupingStrategy};

    #[test]
    fn test_format_number() {
        let values = &[2, 20, 201, 2017, 20173];
        let samples = &[
            (FieldLength::One, ["2", "20", "201", "2017", "20173"]),
            (FieldLength::Two, ["02", "20", "201", "2017", "20173"]),
            (FieldLength::Three, ["002", "020", "201", "2017", "20173"]),
            (FieldLength::Four, ["0002", "0020", "0201", "2017", "20173"]),
        ];

        let mut fixed_decimal_format_options = FixedDecimalFormatterOptions::default();
        fixed_decimal_format_options.grouping_strategy = GroupingStrategy::Never;
        let fixed_decimal_format = FixedDecimalFormatter::try_new(
            icu_locale_core::locale!("en").into(),
            fixed_decimal_format_options,
        )
        .unwrap();

        for (length, expected) in samples {
            for (value, expected) in values.iter().zip(expected) {
                let mut s = String::new();
                try_write_number(
                    &mut writeable::adapters::CoreWriteAsPartsWrite(&mut s),
                    Some(&fixed_decimal_format),
                    FixedDecimal::from(*value),
                    *length,
                )
                .unwrap()
                .unwrap();
                assert_eq!(s, *expected);
            }
        }
    }
}
