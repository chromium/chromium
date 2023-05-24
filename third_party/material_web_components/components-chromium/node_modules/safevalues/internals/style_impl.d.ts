/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import '../environment/dev';
/**
 * Sequence of CSS declarations safe to use in style contexts in an HTML
 * document or in DOM APIs.
 */
export declare abstract class SafeStyle {
    private readonly brand;
}
/**
 * Builds a new `SafeStyle` from the given string, without enforcing
 * safety guarantees. This shouldn't be exposed to application developers, and
 * must only be used as a step towards safe builders or safe constants.
 */
export declare function createStyle(style: string): SafeStyle;
/**
 * Checks if the given value is a `SafeStyle` instance.
 */
export declare function isStyle(value: unknown): value is SafeStyle;
/**
 * Returns the string value of the passed `SafeStyle` object while ensuring it
 * has the correct type.
 */
export declare function unwrapStyle(value: SafeStyle): string;
