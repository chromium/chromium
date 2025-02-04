import { Calendar } from "icu4x"
import { DataProvider } from "icu4x"
import { DateTime } from "icu4x"
import { Locale } from "icu4x"
export function monthCode(year, month, day, hour, minute, second, nanosecond, name) {
    return (function (...args) { return args[0].monthCode }).apply(
        null,
        [
            DateTime.fromIsoInCalendar.apply(
                null,
                [
                    year,
                    month,
                    day,
                    hour,
                    minute,
                    second,
                    nanosecond,
                    Calendar.createForLocale.apply(
                        null,
                        [
                            DataProvider.compiled.apply(
                                null,
                                [
                                ]
                            ),
                            Locale.fromString.apply(
                                null,
                                [
                                    name
                                ]
                            )
                        ]
                    )
                ]
            )
        ]
    );
}
export function era(year, month, day, hour, minute, second, nanosecond, name) {
    return (function (...args) { return args[0].era }).apply(
        null,
        [
            DateTime.fromIsoInCalendar.apply(
                null,
                [
                    year,
                    month,
                    day,
                    hour,
                    minute,
                    second,
                    nanosecond,
                    Calendar.createForLocale.apply(
                        null,
                        [
                            DataProvider.compiled.apply(
                                null,
                                [
                                ]
                            ),
                            Locale.fromString.apply(
                                null,
                                [
                                    name
                                ]
                            )
                        ]
                    )
                ]
            )
        ]
    );
}
