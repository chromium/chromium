/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * Array of pseudo classes to transform by default. These pseudo classes
 * represent state interactions from the user (such as :hover) or the browser
 * (such as :autofill) that cannot be reproduced with HTML markup.
 */
export declare const defaultTransformPseudoClasses: string[];
/**
 * Retrieves the transformed class name for a given pseudo class.
 *
 * @param pseudoClass The pseudo class to transform.
 * @return The transform pseudo class string.
 */
export declare function getTransformedPseudoClass(pseudoClass: string): string;
/**
 * Transforms a document's stylesheets' pseudo classes into normal classes with
 * a new stylesheet.
 *
 * Pseudo classes are given an underscore in their transformation. For example,
 * `:hover` transforms to `._hover`.
 *
 * ```css
 * .mdc-foo:hover {
 *   color: teal;
 * }
 * ```
 * ```css
 * .mdc-foo._hover {
 *   color: teal;
 * }
 * ```
 *
 * @param pseudoClasses An optional array of pseudo class names to transform.
 */
export declare function transformPseudoClasses(stylesheets: Iterable<CSSStyleSheet>, pseudoClasses?: string[]): void;
