"use strict";
/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.setSrc = void 0;
var resource_url_impl_1 = require("../../internals/resource_url_impl");
/**
 * Sets the Src attribute from the given SafeUrl.
 */
function setSrc(embedEl, url) {
    embedEl.src = (0, resource_url_impl_1.unwrapResourceUrl)(url);
}
exports.setSrc = setSrc;
