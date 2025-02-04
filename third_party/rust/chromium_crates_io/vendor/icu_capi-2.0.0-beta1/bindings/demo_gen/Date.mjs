import { Calendar } from "icu4x"
import { DataProvider } from "icu4x"
import { Date } from "icu4x"
import { Locale } from "icu4x"
export function monthCode(year, month, day, name) {
    return (function (...args) { return args[0].monthCode }).apply(
        null,
        [
            Date.fromIsoInCalendar.apply(
                null,
                [
                    year,
                    month,
                    day,
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
export function era(year, month, day, name) {
    return (function (...args) { return args[0].era }).apply(
        null,
        [
            Date.fromIsoInCalendar.apply(
                null,
                [
                    year,
                    month,
                    day,
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
