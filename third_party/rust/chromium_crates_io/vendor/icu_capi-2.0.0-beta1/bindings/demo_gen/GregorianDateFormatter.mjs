import { DataProvider } from "icu4x"
import { GregorianDateFormatter } from "icu4x"
import { IsoDate } from "icu4x"
import { IsoDateTime } from "icu4x"
import { Locale } from "icu4x"
export function formatIsoDate(name, length, year, month, day) {
    return (function (...args) { return args[0].formatIsoDate(...args.slice(1)) }).apply(
        null,
        [
            GregorianDateFormatter.createWithLength.apply(
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
            IsoDate.create.apply(
                null,
                [
                    year,
                    month,
                    day
                ]
            )
        ]
    );
}
export function formatIsoDatetime(name, length, year, month, day, hour, minute, second, nanosecond) {
    return (function (...args) { return args[0].formatIsoDatetime(...args.slice(1)) }).apply(
        null,
        [
            GregorianDateFormatter.createWithLength.apply(
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
