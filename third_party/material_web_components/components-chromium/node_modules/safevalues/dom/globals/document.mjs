/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { unwrapHtml } from '../../internals/html_impl';
/**
 * write safely calls {@link Document.write} on the given {@link Document} with
 * the given {@link SafeHtml}.
 */
export function write(doc, text) {
    doc.write(unwrapHtml(text));
}
/**
 * Safely calls {@link Document.execCommand}. When command is insertHtml, a
 * SafeHtml must be passed in as value.
 */
export function execCommand(doc, command, value) {
    const commandString = String(command);
    let valueArgument = value;
    if (commandString.toLowerCase() === 'inserthtml') {
        valueArgument = unwrapHtml(value);
    }
    return doc.execCommand(commandString, /* showUi= */ false, valueArgument);
}
/**
 * Safely calls {@link Document.execCommand}('insertHtml').
 * @deprecated Use safeDocument.execCommand.
 */
export function execCommandInsertHtml(doc, html) {
    return doc.execCommand('insertHTML', /* showUi= */ false, unwrapHtml(html));
}
