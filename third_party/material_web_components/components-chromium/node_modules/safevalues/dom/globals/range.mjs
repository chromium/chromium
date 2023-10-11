/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { unwrapHtml } from '../../internals/html_impl';
/** Safely creates a contextualFragment. */
export function createContextualFragment(range, html) {
    return range.createContextualFragment(unwrapHtml(html));
}
