/**
 * @license
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
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
export declare function ariaSelector(selector: string): string;
