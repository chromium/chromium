import { DataProvider } from "icu4x"
import { TimeZoneIdMapperWithFastCanonicalization } from "icu4x"
export function canonicalizeIana(value) {
    return (function (...args) { return args[0].canonicalizeIana(...args.slice(1)) }).apply(
        null,
        [
            TimeZoneIdMapperWithFastCanonicalization.create.apply(
                null,
                [
                    DataProvider.compiled.apply(
                        null,
                        [
                        ]
                    )
                ]
            ),
            value
        ]
    );
}
export function canonicalIanaFromBcp47(value) {
    return (function (...args) { return args[0].canonicalIanaFromBcp47(...args.slice(1)) }).apply(
        null,
        [
            TimeZoneIdMapperWithFastCanonicalization.create.apply(
                null,
                [
                    DataProvider.compiled.apply(
                        null,
                        [
                        ]
                    )
                ]
            ),
            value
        ]
    );
}
