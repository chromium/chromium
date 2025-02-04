import { Locale } from "icu4x"
export function basename(name) {
    return (function (...args) { return args[0].basename }).apply(
        null,
        [
            Locale.fromString.apply(
                null,
                [
                    name
                ]
            )
        ]
    );
}
export function getUnicodeExtension(name, s) {
    return (function (...args) { return args[0].getUnicodeExtension(...args.slice(1)) }).apply(
        null,
        [
            Locale.fromString.apply(
                null,
                [
                    name
                ]
            ),
            s
        ]
    );
}
export function language(name) {
    return (function (...args) { return args[0].language }).apply(
        null,
        [
            Locale.fromString.apply(
                null,
                [
                    name
                ]
            )
        ]
    );
}
export function region(name) {
    return (function (...args) { return args[0].region }).apply(
        null,
        [
            Locale.fromString.apply(
                null,
                [
                    name
                ]
            )
        ]
    );
}
export function script(name) {
    return (function (...args) { return args[0].script }).apply(
        null,
        [
            Locale.fromString.apply(
                null,
                [
                    name
                ]
            )
        ]
    );
}
export function normalize(s) {
    return Locale.normalize.apply(
        null,
        [
            s
        ]
    );
}
export function toString(name) {
    return (function (...args) { return args[0].toString(...args.slice(1)) }).apply(
        null,
        [
            Locale.fromString.apply(
                null,
                [
                    name
                ]
            )
        ]
    );
}
