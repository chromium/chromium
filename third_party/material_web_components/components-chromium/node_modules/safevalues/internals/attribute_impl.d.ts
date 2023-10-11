/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import '../environment/dev';
/** A prefix with which an attribute is safe to set using plain strings. */
export declare abstract class SafeAttributePrefix {
    private readonly brand;
}
/**
 * Builds a new `SafeAttribute` from the given string, without enforcing
 * safety guarantees. This shouldn't be exposed to application developers, and
 * must only be used as a step towards safe builders or safe constants.
 */
export declare function createAttributePrefix(attrPrefix: string): SafeAttributePrefix;
/**
 * Returns the string value of the passed `SafeAttributePrefix` object while
 * ensuring it has the correct type.
 */
export declare function unwrapAttributePrefix(value: SafeAttributePrefix): string;
