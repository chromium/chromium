"use strict";
/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.register = void 0;
var resource_url_impl_1 = require("../../internals/resource_url_impl");
/** Safely registers a service worker by URL */
function register(container, scriptURL, options) {
    return container.register((0, resource_url_impl_1.unwrapResourceUrl)(scriptURL), options);
}
exports.register = register;
