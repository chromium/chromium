// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_calendar::cal::Hebrew;
use icu_calendar::Date;
use icu_datetime::fieldsets::enums::{
    CompositeDateTimeFieldSet, DateAndTimeFieldSet, DateFieldSet,
};
use icu_datetime::fieldsets::{self, YMD};
use icu_datetime::{DateTimeFormatterPreferences, FixedCalendarDateTimeFormatter};
use icu_locale_core::{locale, Locale};
use icu_time::{DateTime, Time};
use writeable::assert_writeable_eq;

const EXPECTED_DATETIME: &[&str] = &[
    "Friday, December 22, 2023, 9:22:53 PM",
    "vendredi 22 décembre 2023, 21:22:53",
    "2023年12月22日星期五 21:22:53",
    "शुक्रवार, 22 दिसंबर 2023, 9:22:53 pm",
    "Friday, December 22, 2023, 9:22 PM",
    "vendredi 22 décembre 2023, 21:22",
    "2023年12月22日星期五 21:22",
    "शुक्रवार, 22 दिसंबर 2023, 9:22 pm",
    "December 22, 2023, 9:22:53 PM",
    "22 décembre 2023, 21:22:53",
    "2023/12/22 21:22:53", // TODO(#5806) "2023年12月22日 21:22:53",
    "22 दिसंबर 2023, 9:22:53 pm",
    "December 22, 2023, 9:22 PM",
    "22 décembre 2023, 21:22",
    "2023/12/22 21:22", // TODO(#5806) "2023年12月22日 21:22",
    "22 दिसंबर 2023, 9:22 pm",
    "Dec 22, 2023, 9:22:53 PM",
    "22 déc. 2023, 21:22:53",
    "2023/12/22 21:22:53", // TODO(#5806) "2023年12月22日 21:22:53",
    "22 दिस॰ 2023, 9:22:53 pm",
    "Dec 22, 2023, 9:22 PM",
    "22 déc. 2023, 21:22",
    "2023/12/22 21:22", // TODO(#5806) "2023年12月22日 21:22",
    "22 दिस॰ 2023, 9:22 pm",
    "12/22/23, 9:22:53 PM",
    "22/12/2023 21:22:53",
    "2023/12/22 21:22:53",
    "22/12/23, 9:22:53 pm",
    "12/22/23, 9:22 PM",
    "22/12/2023 21:22",
    "2023/12/22 21:22",
    "22/12/23, 9:22 pm",
];

const EXPECTED_DATE: &[&str] = &[
    "Friday, December 22, 2023",
    "vendredi 22 décembre 2023",
    "2023年12月22日星期五",
    "शुक्रवार, 22 दिसंबर 2023",
    "December 22, 2023",
    "22 décembre 2023",
    "2023/12/22", // TODO(#5806) "2023年12月22日",
    "22 दिसंबर 2023",
    "Dec 22, 2023",
    "22 déc. 2023",
    "2023/12/22", // TODO(#5806) "2023年12月22日",
    "22 दिस॰ 2023",
    "12/22/23",
    "22/12/2023",
    "2023/12/22",
    "22/12/23",
];

#[test]
fn neo_datetime_lengths() {
    let datetime = DateTime {
        date: Date::try_new_gregorian(2023, 12, 22).unwrap(),
        time: Time::try_new(21, 22, 53, 0).unwrap(),
    };
    let mut expected_iter = EXPECTED_DATETIME.iter();
    for field_set in [
        DateAndTimeFieldSet::YMDET(fieldsets::YMDET::long()),
        DateAndTimeFieldSet::YMDET(fieldsets::YMDET::long().hm()),
        DateAndTimeFieldSet::YMDT(fieldsets::YMDT::long()),
        DateAndTimeFieldSet::YMDT(fieldsets::YMDT::long().hm()),
        DateAndTimeFieldSet::YMDT(fieldsets::YMDT::medium()),
        DateAndTimeFieldSet::YMDT(fieldsets::YMDT::medium().hm()),
        DateAndTimeFieldSet::YMDT(fieldsets::YMDT::short()),
        DateAndTimeFieldSet::YMDT(fieldsets::YMDT::short().hm()),
    ] {
        for locale in [locale!("en"), locale!("fr"), locale!("zh"), locale!("hi")] {
            let prefs = DateTimeFormatterPreferences::from(&locale);
            let skeleton = CompositeDateTimeFieldSet::DateTime(field_set);
            let formatter = FixedCalendarDateTimeFormatter::try_new(prefs, skeleton).unwrap();
            let formatted = formatter.format(&datetime);
            let expected = expected_iter.next().unwrap();
            assert_writeable_eq!(formatted, *expected, "{skeleton:?} {locale:?}");
        }
    }
}

#[test]
fn neo_date_lengths() {
    let datetime = DateTime {
        date: Date::try_new_gregorian(2023, 12, 22).unwrap(),
        time: Time::try_new(21, 22, 53, 0).unwrap(),
    };
    let mut expected_iter = EXPECTED_DATE.iter();
    for field_set in [
        DateFieldSet::YMDE(fieldsets::YMDE::long()),
        DateFieldSet::YMD(fieldsets::YMD::long()),
        DateFieldSet::YMD(fieldsets::YMD::medium()),
        DateFieldSet::YMD(fieldsets::YMD::short()),
    ] {
        let date_skeleton = CompositeDateTimeFieldSet::Date(field_set);
        for locale in [locale!("en"), locale!("fr"), locale!("zh"), locale!("hi")] {
            let prefs = DateTimeFormatterPreferences::from(&locale);
            let formatter = FixedCalendarDateTimeFormatter::try_new(prefs, date_skeleton).unwrap();
            let formatted = formatter.format(&datetime);
            let expected = expected_iter.next().unwrap();
            assert_writeable_eq!(formatted, *expected, "{date_skeleton:?} {locale:?}");
        }
    }
}

#[test]
fn overlap_patterns() {
    let datetime = DateTime {
        date: Date::try_new_gregorian(2024, 8, 9).unwrap(),
        time: Time::try_new(20, 40, 7, 250).unwrap(),
    };
    struct TestCase {
        locale: Locale,
        skeleton: CompositeDateTimeFieldSet,
        expected: &'static str,
    }
    let cases = [
        // Note: in en-US, there is no comma in the overlap pattern
        TestCase {
            locale: locale!("en-US"),
            skeleton: CompositeDateTimeFieldSet::DateTime(DateAndTimeFieldSet::ET(
                fieldsets::ET::medium(),
            )),
            expected: "Fri 8:40:07\u{202f}PM",
        },
        TestCase {
            locale: locale!("en-US"),
            skeleton: CompositeDateTimeFieldSet::DateTime(DateAndTimeFieldSet::MDET(
                fieldsets::MDET::medium(),
            )),
            expected: "Fri, Aug 9, 8:40:07\u{202f}PM",
        },
        // Note: in ru, the standalone weekday name is used when it is the only one in the pattern
        // (but the strings are the same in data)
        TestCase {
            locale: locale!("ru"),
            skeleton: CompositeDateTimeFieldSet::DateTime(DateAndTimeFieldSet::ET(
                fieldsets::ET::medium(),
            )),
            expected: "пт 20:40:07",
        },
        TestCase {
            locale: locale!("ru"),
            skeleton: CompositeDateTimeFieldSet::Date(DateFieldSet::E(fieldsets::E::medium())),
            expected: "пт",
        },
    ];
    for TestCase {
        locale,
        skeleton,
        expected,
    } in cases
    {
        let prefs = DateTimeFormatterPreferences::from(&locale);
        let formatter = FixedCalendarDateTimeFormatter::try_new(prefs, skeleton).unwrap();
        let formatted = formatter.format(&datetime);
        assert_writeable_eq!(formatted, expected, "{locale:?} {skeleton:?}");
    }
}

#[test]
fn hebrew_months() {
    let datetime = DateTime {
        date: Date::try_new_iso(2011, 4, 3).unwrap().to_calendar(Hebrew),
        time: Time::try_new(14, 15, 7, 0).unwrap(),
    };
    let formatter =
        FixedCalendarDateTimeFormatter::try_new(locale!("en").into(), YMD::medium()).unwrap();

    let formatted_datetime = formatter.format(&datetime);

    assert_writeable_eq!(formatted_datetime, "28 Adar II 5771");
}

#[test]
fn test_5387() {
    let datetime = DateTime {
        date: Date::try_new_gregorian(2024, 8, 16).unwrap(),
        time: Time::try_new(14, 15, 16, 0).unwrap(),
    };
    let formatter_auto = FixedCalendarDateTimeFormatter::try_new(
        locale!("en").into(),
        CompositeDateTimeFieldSet::DateTime(DateAndTimeFieldSet::ET(fieldsets::ET::medium())),
    )
    .unwrap();
    let formatter_h12 = FixedCalendarDateTimeFormatter::try_new(
        locale!("en-u-hc-h12").into(),
        CompositeDateTimeFieldSet::DateTime(DateAndTimeFieldSet::ET(fieldsets::ET::medium())),
    )
    .unwrap();
    let formatter_h24 = FixedCalendarDateTimeFormatter::try_new(
        locale!("en-u-hc-h23").into(),
        CompositeDateTimeFieldSet::DateTime(DateAndTimeFieldSet::ET(fieldsets::ET::medium())),
    )
    .unwrap();

    // TODO(#5387): All of these should resolve to a pattern without a comma
    assert_writeable_eq!(formatter_auto.format(&datetime), "Fri 2:15:16\u{202f}PM");
    assert_writeable_eq!(formatter_h12.format(&datetime), "Fri, 2:15:16\u{202f}PM");
    assert_writeable_eq!(formatter_h24.format(&datetime), "Fri, 14:15:16");
}
