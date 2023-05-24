/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import '../environment/dev';
import { SafeStyleSheet } from '../internals/style_sheet_impl';
/**
 * Creates a SafeStyleSheet object from a template literal (without any
 * embedded expressions).
 *
 * This function is a template literal tag function. It should be called with
 * a template literal that does not contain any expressions. For example,
 *                         safeStyleSheet`foo`;
 * The argument must not have any < or > characters in it. This is so that
 * SafeStyleSheet's contract is preserved, allowing the SafeStyleSheet to
 * correctly be interpreted as a sequence of CSS declarations and without
 * affecting the syntactic structure of any surrounding CSS and HTML.
 *
 * @param templateObj This contains the literal part of the template literal.
 */
export declare function safeStyleSheet(templateObj: TemplateStringsArray): SafeStyleSheet;
/**
 * Creates a `SafeStyleSheet` value by concatenating multiple `SafeStyleSheet`s.
 */
export declare function concatStyleSheets(sheets: readonly SafeStyleSheet[]): SafeStyleSheet;
