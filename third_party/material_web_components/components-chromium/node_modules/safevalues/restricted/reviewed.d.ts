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
 * Performs a "reviewed conversion" to SafeHtml from a plain string that is
 * known to satisfy the SafeHtml type contract.
 *
 * IMPORTANT: Uses of this method must be carefully security-reviewed to ensure
 * that the value of `html` satisfies the SafeHtml type contract in all
 * possible program states. An appropriate `justification` must be provided
 * explaining why this particular use of the function is safe.
 */
export declare function htmlSafeByReview(html: string, justification: string): SafeHtml;
/**
 * Performs a "reviewed conversion" to SafeScript from a plain string that
 * is known to satisfy the SafeScript type contract.
 *
 * IMPORTANT: Uses of this method must be carefully security-reviewed to ensure
 * that the value of `script` satisfies the SafeScript type contract in
 * all possible program states. An appropriate `justification` must be provided
 * explaining why this particular use of the function is safe.
 */
export declare function scriptSafeByReview(script: string, justification: string): SafeScript;
/**
 * Performs a "reviewed conversion" to TrustedResourceUrl from a plain string
 * that is known to satisfy the SafeUrl type contract.
 *
 * IMPORTANT: Uses of this method must be carefully security-reviewed to ensure
 * that the value of `url` satisfies the TrustedResourceUrl type
 * contract in all possible program states. An appropriate `justification` must
 * be provided explaining why this particular use of the function is safe.
 */
export declare function resourceUrlSafeByReview(url: string, justification: string): TrustedResourceUrl;
/**
 * Performs a "reviewed conversion" to SafeStyle from a plain string that is
 * known to satisfy the SafeStyle type contract.
 *
 * IMPORTANT: Uses of this method must be carefully security-reviewed to ensure
 * that the value of `style` satisfies the SafeStyle type contract in all
 * possible program states. An appropriate `justification` must be provided
 * explaining why this particular use of the function is safe.
 */
export declare function styleSafeByReview(style: string, justification: string): SafeStyle;
/**
 * Performs a "reviewed conversion" to SafeStyleSheet from a plain string that
 * is known to satisfy the SafeStyleSheet type contract.
 *
 * IMPORTANT: Uses of this method must be carefully security-reviewed to ensure
 * that the value of `stylesheet` satisfies the SafeStyleSheet type
 * contract in all possible program states. An appropriate `justification` must
 * be provided explaining why this particular use of the function is safe; this
 * may include a security review ticket number.
 */
export declare function styleSheetSafeByReview(stylesheet: string, justification: string): SafeStyleSheet;
