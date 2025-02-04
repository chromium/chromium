import { TimeZoneInfo } from "icu4x"
export function timeZoneId(bcp47Id, offsetSeconds, dst) {
    return (function (...args) { return args[0].timeZoneId }).apply(
        null,
        [
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
