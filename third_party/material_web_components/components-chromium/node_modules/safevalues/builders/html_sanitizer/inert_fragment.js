"use strict";
/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.createInertFragment = void 0;
var element_1 = require("../../dom/elements/element");
var html_impl_1 = require("../../internals/html_impl");
/**
 * Returns a fragment that contains the parsed HTML for `dirtyHtml` without
 * executing any of the potential payload.
 */
function createInertFragment(dirtyHtml) {
    var template = document.createElement('template');
    // This call is only used to create an inert tree for the sanitizer to
    // further process and is never returned directly to the caller. We can't use
    // a reviewed conversion in order to avoid an import loop.
    var temporarySafeHtml = (0, html_impl_1.createHtml)(dirtyHtml);
    (0, element_1.setInnerHtml)(template, temporarySafeHtml);
    return template.content;
}
exports.createInertFragment = createInertFragment;
