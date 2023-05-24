/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import '../environment/dev';
import { SafeHtml } from '../internals/html_impl';
import { TrustedResourceUrl } from '../internals/resource_url_impl';
import { SafeScript } from '../internals/script_impl';
import { SafeStyle } from '../internals/style_impl';
import { SafeStyleSheet } from '../internals/style_sheet_impl';
/**
 * Turns a string into SafeHtml for legacy API purposes.
 *
 * Please read fileoverview documentation before using.
 */
export declare function legacyUnsafeHtml(s: string): SafeHtml;
/**
 * Turns a string into SafeScript for legacy API purposes.
 *
 * Please read fileoverview documentation before using.
 */
export declare function legacyUnsafeScript(s: string): SafeScript;
/**
 * Turns a string into TrustedResourceUrl for legacy API purposes.
 *
 * Please read fileoverview documentation before using.
 */
export declare function legacyUnsafeResourceUrl(s: string): TrustedResourceUrl;
/**
 * Turns a string into SafeStyle for legacy API purposes.
 *
 * Please read fileoverview documentation before using.
 */
export declare function legacyUnsafeStyle(s: string): SafeStyle;
/**
 * Turns a string into SafeStyleSheet for legacy API purposes.
 *
 * Please read fileoverview documentation before using.
 */
export declare function legacyUnsafeStyleSheet(s: string): SafeStyleSheet;
