"use strict";
/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.assertIsTemplateObject = void 0;
/**
 * An object of type TemplateStringsArray represents the literal part(s) of a
 * template literal. This function checks if a TemplateStringsArray object is
 * actually from a template literal.
 *
 * @param templateObj This contains the literal part of the template literal.
 * @param hasExprs If true, the input template may contain embedded expressions.
 * @param errorMsg The custom error message in case any checks fail.
 */
function assertIsTemplateObject(templateObj, hasExprs, errorMsg) {
    if (!Array.isArray(templateObj) || !Array.isArray(templateObj.raw) ||
        (!hasExprs && templateObj.length !== 1)) {
        throw new TypeError(errorMsg);
    }
}
exports.assertIsTemplateObject = assertIsTemplateObject;
