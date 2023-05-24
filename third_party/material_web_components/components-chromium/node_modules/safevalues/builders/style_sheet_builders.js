"use strict";
/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.concatStyleSheets = exports.safeStyleSheet = void 0;
require("../environment/dev");
var string_literal_1 = require("../internals/string_literal");
var style_sheet_impl_1 = require("../internals/style_sheet_impl");
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
function safeStyleSheet(templateObj) {
    if (process.env.NODE_ENV !== 'production') {
        (0, string_literal_1.assertIsTemplateObject)(templateObj, false, 'safeStyleSheet is a template literal tag ' +
            'function that only accepts template literals without ' +
            'expressions. For example, safeStyleSheet`foo`;');
    }
    var styleSheet = templateObj[0];
    if (process.env.NODE_ENV !== 'production') {
        if (/[<>]/.test(styleSheet)) {
            throw new Error('Forbidden characters in styleSheet string: ' + styleSheet);
        }
    }
    return (0, style_sheet_impl_1.createStyleSheet)(styleSheet);
}
exports.safeStyleSheet = safeStyleSheet;
/**
 * Creates a `SafeStyleSheet` value by concatenating multiple `SafeStyleSheet`s.
 */
function concatStyleSheets(sheets) {
    return (0, style_sheet_impl_1.createStyleSheet)(sheets.map(style_sheet_impl_1.unwrapStyleSheet).join(''));
}
exports.concatStyleSheets = concatStyleSheets;
