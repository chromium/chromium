import { FixedDecimal } from "icu4x"
export function toString(f, magnitude) {
    return (function (...args) { return args[0].toString(...args.slice(1)) }).apply(
        null,
        [
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
