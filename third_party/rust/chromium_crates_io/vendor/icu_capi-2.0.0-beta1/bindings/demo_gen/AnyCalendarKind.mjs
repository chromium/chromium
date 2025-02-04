import { AnyCalendarKind } from "icu4x"
export function bcp47(self) {
    return (function (...args) { return args[0].bcp47 }).apply(
        null,
        [
            self
        ]
    );
}
