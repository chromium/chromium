// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use std::collections::BTreeMap;

use icu::calendar::Date;
use icu::datetime::{fieldsets, NoCalendarFormatter};
use icu::locale::locale;
use icu::time::Time;

fn main() {
    let parser = icu::time::zone::IanaParser::new();
    let offsets = icu::time::zone::UtcOffsetCalculator::new();

    let prefs = locale!("en").into();

    let offset_formatter =
        NoCalendarFormatter::try_new(prefs, fieldsets::zone::LocalizedOffsetLong).unwrap();
    let non_location_formatter =
        NoCalendarFormatter::try_new(prefs, fieldsets::zone::GenericLong).unwrap();
    let city_formatter =
        NoCalendarFormatter::try_new(prefs, fieldsets::zone::ExemplarCity).unwrap();

    let reference_date = (Date::try_new_iso(2025, 1, 1).unwrap(), Time::midnight());

    let mut grouped_tzs = BTreeMap::<_, Vec<_>>::new();

    for tz in parser.iter() {
        if tz.0 == "unk" || tz.starts_with("utc") || tz.0 == "gmt" {
            continue;
        }

        let offsets = offsets
            .compute_offsets_from_time_zone(tz, reference_date)
            .unwrap();

        let tzi = tz
            .with_offset(Some(offsets.standard))
            .at_time(reference_date);

        grouped_tzs
            .entry(non_location_formatter.format(&tzi).to_string())
            .or_default()
            .push((offsets, tzi));
    }

    let mut list = Vec::new();

    for (non_location, zones) in grouped_tzs {
        for (offsets, tzi) in &zones {
            list.push((
                -offsets.standard.to_seconds(),
                format!(
                    "({}{})",
                    offset_formatter.format(tzi),
                    if let Some(daylight) = offsets.daylight {
                        format!(
                            "/{}",
                            offset_formatter.format(
                                &tzi.time_zone_id()
                                    .with_offset(Some(daylight))
                                    .at_time(reference_date)
                            )
                        )
                    } else {
                        String::new()
                    }
                ),
                if zones.len() == 1 {
                    non_location.clone()
                } else {
                    format!("{non_location} - {}", city_formatter.format(tzi))
                },
            ));
        }
    }

    list.sort_by(|a, b| (a.0, &a.2).cmp(&(b.0, &b.2)));

    for (_, offset, non_location) in &list {
        println!(
            "{offset:0$} {non_location}",
            list.iter().map(|(_, l, ..)| l.len()).max().unwrap()
        );
    }
}
