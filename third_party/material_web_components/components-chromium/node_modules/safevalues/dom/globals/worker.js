"use strict";
/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
var __read = (this && this.__read) || function (o, n) {
    var m = typeof Symbol === "function" && o[Symbol.iterator];
    if (!m) return o;
    var i = m.call(o), r, ar = [], e;
    try {
        while ((n === void 0 || n-- > 0) && !(r = i.next()).done) ar.push(r.value);
    }
    catch (error) { e = { error: error }; }
    finally {
        try {
            if (r && !r.done && (m = i["return"])) m.call(i);
        }
        finally { if (e) throw e.error; }
    }
    return ar;
};
var __spreadArray = (this && this.__spreadArray) || function (to, from, pack) {
    if (pack || arguments.length === 2) for (var i = 0, l = from.length, ar; i < l; i++) {
        if (ar || !(i in from)) {
            if (!ar) ar = Array.prototype.slice.call(from, 0, i);
            ar[i] = from[i];
        }
    }
    return to.concat(ar || Array.prototype.slice.call(from));
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.importScripts = exports.createShared = exports.create = void 0;
var resource_url_impl_1 = require("../../internals/resource_url_impl");
/**
 * Safely creates a Web Worker.
 *
 * Example usage:
 *   const trustedResourceUrl = trustedResourceUrl`/safe_script.js`;
 *   safedom.safeWorker.create(trustedResourceUrl);
 * which is a safe alternative to
 *   new Worker(url);
 * The latter can result in loading untrusted code.
 */
function create(url, options) {
    return new Worker((0, resource_url_impl_1.unwrapResourceUrl)(url), options);
}
exports.create = create;
/** Safely creates a shared Web Worker. */
function createShared(url, options) {
    return new SharedWorker((0, resource_url_impl_1.unwrapResourceUrl)(url), options);
}
exports.createShared = createShared;
/** Safely calls importScripts */
function importScripts(scope) {
    var urls = [];
    for (var _i = 1; _i < arguments.length; _i++) {
        urls[_i - 1] = arguments[_i];
    }
    scope.importScripts.apply(scope, __spreadArray([], __read(urls.map(function (url) { return (0, resource_url_impl_1.unwrapResourceUrl)(url); })), false));
}
exports.importScripts = importScripts;
