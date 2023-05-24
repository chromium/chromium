"use strict";
/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.setTextContent = void 0;
var style_sheet_impl_1 = require("../../internals/style_sheet_impl");
/** Safe setters for `HTMLStyleElement`s. */
function setTextContent(elem, safeStyleSheet) {
    elem.textContent = (0, style_sheet_impl_1.unwrapStyleSheet)(safeStyleSheet);
}
exports.setTextContent = setTextContent;
