import { DataProvider } from "icu4x"
import { Locale } from "icu4x"
import { TitlecaseMapper } from "icu4x"
import { TitlecaseOptions } from "icu4x"
export function titlecaseSegment(s, name, leading_adjustment, trailing_case) {
    return (function (...args) { return args[0].titlecaseSegment(...args.slice(1)) }).apply(
        null,
        [
            TitlecaseMapper.create.apply(
                null,
                [
                    DataProvider.compiled.apply(
                        null,
                        [
                        ]
                    )
                ]
            ),
            s,
            Locale.fromString.apply(
                null,
                [
                    name
                ]
            ),
            (function (...args) {
                return new TitlecaseOptions({
                    leadingAdjustment: args[0],
                    trailingCase: args[1]});
            }).apply(
                null,
                [
                    leading_adjustment,
                    trailing_case
                ]
            )
        ]
    );
}
