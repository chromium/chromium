import { Calendar } from "icu4x"
import { DataProvider } from "icu4x"
import { DateTime } from "icu4x"
import { IsoDateTime } from "icu4x"
import { Locale } from "icu4x"
import { TimeZoneInfo } from "icu4x"
import { ZonedDateTimeFormatter } from "icu4x"
export function formatDatetimeWithCustomTimeZone(name, length, year, month, day, hour, minute, second, nanosecond, name_1, bcp47Id, offsetSeconds, dst) {
    return (function (...args) { return args[0].formatDatetimeWithCustomTimeZone(...args.slice(1)) }).apply(
        null,
        [
            ZonedDateTimeFormatter.createWithLength.apply(
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
            ),
            TimeZoneInfo.fromParts.apply(
                null,
                [
                    bcp47Id,
                    offsetSeconds,
                    dst
                ]
            )
        ]
    );
}
export function formatIsoDatetimeWithCustomTimeZone(name, length, year, month, day, hour, minute, second, nanosecond, bcp47Id, offsetSeconds, dst) {
    return (function (...args) { return args[0].formatIsoDatetimeWithCustomTimeZone(...args.slice(1)) }).apply(
        null,
        [
            ZonedDateTimeFormatter.createWithLength.apply(
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
            ),
            TimeZoneInfo.fromParts.apply(
                null,
                [
                    bcp47Id,
                    offsetSeconds,
                    dst
                ]
            )
        ]
    );
}
