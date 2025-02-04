import { DataProvider } from "icu4x"
import { FixedDecimal } from "icu4x"
import { FixedDecimalFormatter } from "icu4x"
import { Locale } from "icu4x"
export function format(name, groupingStrategy, f, magnitude) {
    return (function (...args) { return args[0].format(...args.slice(1)) }).apply(
        null,
        [
            FixedDecimalFormatter.createWithGroupingStrategy.apply(
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
                    groupingStrategy
                ]
            ),
            FixedDecimal.fromNumberWithLowerMagnitude.apply(
                null,
                [
                    f,
                    magnitude
                ]
            )
        ]
    );
}
