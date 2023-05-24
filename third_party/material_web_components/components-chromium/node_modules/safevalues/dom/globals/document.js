"use strict";
/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.execCommandInsertHtml = exports.execCommand = exports.write = void 0;
var html_impl_1 = require("../../internals/html_impl");
/**
 * write safely calls {@link Document.write} on the given {@link Document} with
 * the given {@link SafeHtml}.
 */
function write(doc, text) {
    doc.write((0, html_impl_1.unwrapHtml)(text));
}
exports.write = write;
/**
 * Safely calls {@link Document.execCommand}. When command is insertHtml, a
 * SafeHtml must be passed in as value.
 */
function execCommand(doc, command, value) {
    var commandString = String(command);
    var valueArgument = value;
    if (commandString.toLowerCase() === 'inserthtml') {
        valueArgument = (0, html_impl_1.unwrapHtml)(value);
    }
    return doc.execCommand(commandString, /* showUi= */ false, valueArgument);
}
exports.execCommand = execCommand;
/**
 * Safely calls {@link Document.execCommand}('insertHtml').
 * @deprecated Use safeDocument.execCommand.
 */
function execCommandInsertHtml(doc, html) {
    return doc.execCommand('insertHTML', /* showUi= */ false, (0, html_impl_1.unwrapHtml)(html));
}
exports.execCommandInsertHtml = execCommandInsertHtml;
