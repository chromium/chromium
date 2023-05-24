"use strict";
/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.open = void 0;
var url_sanitizer_1 = require("../../builders/url_sanitizer");
/**
 * open calls {@link Window.open} on the given {@link Window}, given a
 * target {@link Url}.
 */
function open(win, url, target, features) {
    var sanitizedUrl = (0, url_sanitizer_1.unwrapUrlOrSanitize)(url);
    if (sanitizedUrl !== undefined) {
        return win.open(sanitizedUrl, target, features);
    }
    return null;
}
exports.open = open;
