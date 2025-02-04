import { Calendar } from "icu4x"
import { DataProvider } from "icu4x"
import { DateTime } from "icu4x"
import { IsoDateTime } from "icu4x"
import { Locale } from "icu4x"
import { Time } from "icu4x"
import { TimeFormatter } from "icu4x"
export function formatTime(name, length, hour, minute, second, nanosecond) {
    return (function (...args) { return args[0].formatTime(...args.slice(1)) }).apply(
        null,
        [
            TimeFormatter.createWithLength.apply(
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
                    ),
                    length
                ]
            ),
            Time.create.apply(
                null,
                [
                    hour,
                    minute,
                    second,
                    nanosecond
                ]
            )
        ]
    );
}
export function formatDatetime(name, length, year, month, day, hour, minute, second, nanosecond, name_1) {
    return (function (...args) { return args[0].formatDatetime(...args.slice(1)) }).apply(
        null,
        [
            TimeFormatter.createWithLength.apply(
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
                    ),
                    length
                ]
            ),
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
                                    name_1
                                ]
                            )
                        ]
                    )
                ]
            )
        ]
    );
}
export function formatIsoDatetime(name, length, year, month, day, hour, minute, second, nanosecond) {
    return (function (...args) { return args[0].formatIsoDatetime(...args.slice(1)) }).apply(
        null,
        [
            TimeFormatter.createWithLength.apply(
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
                    ),
                    length
                ]
            ),
            IsoDateTime.create.apply(
                null,
                [
                    year,
                    month,
                    day,
                    hour,
                    minute,
                    second,
                    nanosecond
                ]
            )
        ]
    );
}
