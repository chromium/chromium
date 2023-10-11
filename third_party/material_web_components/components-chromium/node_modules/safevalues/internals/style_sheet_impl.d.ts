/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import '../environment/dev';
/**
 * A complete CSS style sheet, safe to use in style contexts in an HTML document
 * or DOM APIs.
 */
export declare abstract class SafeStyleSheet {
    private readonly brand;
}
/**
 * Builds a new `SafeStyleSheet` from the given string, without enforcing
 * safety guarantees. This shouldn't be exposed to application developers, and
 * must only be used as a step towards safe builders or safe constants.
 */
export declare function createStyleSheet(styleSheet: string): SafeStyleSheet;
/**
 * Checks if the given value is a `SafeStyleSheet` instance.
 */
export declare function isStyleSheet(value: unknown): value is SafeStyleSheet;
/**
 * Returns the string value of the passed `SafeStyleSheet` object while
 * ensuring it has the correct type.
 */
export declare function unwrapStyleSheet(value: SafeStyleSheet): string;
