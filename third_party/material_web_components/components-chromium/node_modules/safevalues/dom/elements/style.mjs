/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { unwrapStyleSheet } from '../../internals/style_sheet_impl';
/** Safe setters for `HTMLStyleElement`s. */
export function setTextContent(elem, safeStyleSheet) {
    elem.textContent = unwrapStyleSheet(safeStyleSheet);
}
