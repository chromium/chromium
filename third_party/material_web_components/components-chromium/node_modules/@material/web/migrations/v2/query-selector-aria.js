/**
 * @license
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
const HAS_ARIA_ATTRIBUTE_REGEX = /\[(aria-|role)/g;
/**
 * Patches a CSS selector string to include `data-*` shifting `role` and
 * `aria-*` attributes. Use this with `querySelector()` and `querySelectorAll()`
 * for MWC elements.
 *
 * @example
 * ```ts
 * const agreeCheckbox = document.querySelector(
 *   ariaSelector('md-checkbox[aria-label="Agree"]')
 * );
 * ```
 *
 * @param selector A CSS selector string.
 * @return A CSS selector string that includes `data-*` shifting aria
 *     attributes.
 */
export function ariaSelector(selector) {
    if (!HAS_ARIA_ATTRIBUTE_REGEX.test(selector)) {
        return selector;
    }
    const selectorWithDataShifted = selector.replaceAll(HAS_ARIA_ATTRIBUTE_REGEX, '[data-$1');
    return `${selector},${selectorWithDataShifted}`;
}
//# sourceMappingURL=query-selector-aria.js.map