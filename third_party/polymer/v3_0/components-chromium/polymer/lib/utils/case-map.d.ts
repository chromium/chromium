// tslint:disable:variable-name Describing an API that's defined elsewhere.

export {dashToCamelCase};


/**
 * Converts "dash-case" identifier (e.g. `foo-bar-baz`) to "camelCase"
 * (e.g. `fooBarBaz`).
 *
 * @returns Camel-case representation of the identifier
 */
declare function dashToCamelCase(dash: string): string;

export {camelToDashCase};


/**
 * Converts "camelCase" identifier (e.g. `fooBarBaz`) to "dash-case"
 * (e.g. `foo-bar-baz`).
 *
 * @returns Dash-case representation of the identifier
 */
declare function camelToDashCase(camel: string): string;
