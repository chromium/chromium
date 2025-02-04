import { DataProvider } from "icu4x"
import { ListFormatter } from "icu4x"
import { Locale } from "icu4x"
export function format(name, length, list) {
    return (function (...args) { return args[0].format(...args.slice(1)) }).apply(
        null,
        [
            ListFormatter.createAndWithLength.apply(
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
            list
        ]
    );
}
