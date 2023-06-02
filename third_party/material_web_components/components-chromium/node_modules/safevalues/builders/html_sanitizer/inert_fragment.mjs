/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { setInnerHtml } from '../../dom/elements/element';
import { createHtml } from '../../internals/html_impl';
/**
 * Returns a fragment that contains the parsed HTML for `dirtyHtml` without
 * executing any of the potential payload.
 */
export function createInertFragment(dirtyHtml) {
    const template = document.createElement('template');
    // This call is only used to create an inert tree for the sanitizer to
    // further process and is never returned directly to the caller. We can't use
    // a reviewed conversion in order to avoid an import loop.
    const temporarySafeHtml = createHtml(dirtyHtml);
    setInnerHtml(template, temporarySafeHtml);
    return template.content;
}
