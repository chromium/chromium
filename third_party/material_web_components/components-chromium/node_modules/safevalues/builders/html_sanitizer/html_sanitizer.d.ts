/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../environment/dev';
import { SafeHtml } from '../../internals/html_impl';
import { SanitizerTable } from './sanitizer_table/sanitizer_table';
/**
 * An HTML5-compliant markup sanitizer that produces SafeHtml markup.
 *
 * You can build sanitizers with a custom configuration using the
 * HtmlSanitizerBuilder.
 */
export interface HtmlSanitizer {
    sanitize(html: string): SafeHtml;
    sanitizeToFragment(html: string): DocumentFragment;
    sanitizeAssertUnchanged(html: string): SafeHtml;
}
/** Implementation for `HtmlSanitizer` */
export declare class HtmlSanitizerImpl implements HtmlSanitizer {
    private readonly sanitizerTable;
    private changes;
    constructor(sanitizerTable: SanitizerTable, token: object);
    sanitizeAssertUnchanged(html: string): SafeHtml;
    sanitize(html: string): SafeHtml;
    sanitizeToFragment(html: string): DocumentFragment;
    private sanitizeTextNode;
    private sanitizeElementNode;
    nodeFilter(node: Node): number;
    private recordChange;
    private satisfiesAllConditions;
}
/** Sanitizes untrusted html using the default sanitizer configuration. */
export declare function sanitizeHtml(html: string): SafeHtml;
/**
 * Sanitizes untrusted html using the default sanitizer configuration. Throws
 * an error if the html was changed.
 */
export declare function sanitizeHtmlAssertUnchanged(html: string): SafeHtml;
/**
 * Sanitizes untrusted html using the default sanitizer configuration. Throws
 * an error if the html was changed.
 */
export declare function sanitizeHtmlToFragment(html: string): DocumentFragment;
