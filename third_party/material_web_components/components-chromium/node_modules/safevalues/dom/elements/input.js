"use strict";
/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.setFormaction = void 0;
var url_sanitizer_1 = require("../../builders/url_sanitizer");
/**
 * Sets the Formaction attribute from the given Url.
 */
function setFormaction(input, url) {
    var sanitizedUrl = (0, url_sanitizer_1.unwrapUrlOrSanitize)(url);
    if (sanitizedUrl !== undefined) {
        input.formAction = sanitizedUrl;
    }
}
exports.setFormaction = setFormaction;
