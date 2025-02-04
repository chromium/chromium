import { DataProvider } from "icu4x"
import { DecomposingNormalizer } from "icu4x"
export function normalize(s) {
    return (function (...args) { return args[0].normalize(...args.slice(1)) }).apply(
        null,
        [
            DecomposingNormalizer.createNfd.apply(
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
