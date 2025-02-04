import { CaseMapper } from "icu4x"
import { DataProvider } from "icu4x"
import { Locale } from "icu4x"
import { TitlecaseOptions } from "icu4x"
export function lowercase(s, name) {
    return (function (...args) { return args[0].lowercase(...args.slice(1)) }).apply(
        null,
        [
            CaseMapper.create.apply(
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
            )
        ]
    );
}
export function uppercase(s, name) {
    return (function (...args) { return args[0].uppercase(...args.slice(1)) }).apply(
        null,
        [
            CaseMapper.create.apply(
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
            )
        ]
    );
}
export function titlecaseSegmentWithOnlyCaseData(s, name, leading_adjustment, trailing_case) {
    return (function (...args) { return args[0].titlecaseSegmentWithOnlyCaseData(...args.slice(1)) }).apply(
        null,
        [
            CaseMapper.create.apply(
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
export function fold(s) {
    return (function (...args) { return args[0].fold(...args.slice(1)) }).apply(
        null,
        [
            CaseMapper.create.apply(
                null,
                [
                    DataProvider.compiled.apply(
                        null,
                        [
                        ]
                    )
                ]
            ),
            s
        ]
    );
}
export function foldTurkic(s) {
    return (function (...args) { return args[0].foldTurkic(...args.slice(1)) }).apply(
        null,
        [
            CaseMapper.create.apply(
                null,
                [
                    DataProvider.compiled.apply(
                        null,
                        [
                        ]
                    )
                ]
            ),
            s
        ]
    );
}
